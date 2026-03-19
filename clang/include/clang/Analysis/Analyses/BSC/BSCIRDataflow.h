//===- BSCIRDataflow.h - Generic dataflow on BSCIR -*- C++ -*--------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generic forward and backward dataflow analysis framework operating on BSCIR.
// Replaces per-analysis worklist implementations with a single template.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDATAFLOW_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDATAFLOW_H

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIR.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <queue>

namespace clang {
namespace bscir {

/// Direction of dataflow analysis.
enum class Direction { Forward, Backward };

/// Result of a dataflow analysis: per-block entry and exit states.
template <typename Lattice> struct DataflowResult {
  llvm::DenseMap<BasicBlockId, Lattice> EntryStates;
  llvm::DenseMap<BasicBlockId, Lattice> ExitStates;
};

/// Abstract base class for dataflow analyses on BSCIR.
///
/// Subclasses define the lattice type and implement:
/// - entryState: initial state at function entry (forward) or exit (backward)
/// - transferStatement: apply a single statement's effect to the state
/// - transferTerminator: compute the state flowing to a specific successor
/// - merge: join two states (returns true if dst changed)
template <Direction Dir, typename Lattice> class DataflowAnalysis {
public:
  virtual ~DataflowAnalysis() = default;

  /// Initial state at the entry point of the analysis.
  /// For forward: state at function entry.
  /// For backward: state at function exit.
  virtual Lattice entryState(const Body &B) const = 0;

  /// Transfer function for a single statement.
  /// Modifies State in place. Returns true if State changed.
  virtual bool transferStatement(const Statement &S, Lattice &State) const = 0;

  /// Transfer function at a terminator.
  /// Given the state before the terminator and a target successor block,
  /// returns the state flowing into that successor.
  /// This enables condition-dependent refinement (e.g., null checks).
  virtual Lattice transferTerminator(const Terminator &T,
                                     const Lattice &StateBeforeTerm,
                                     BasicBlockId Target) const = 0;

  /// Merge src into dst. Returns true if dst was modified.
  virtual bool merge(const Lattice &Src, Lattice &Dst) const = 0;
};

//===----------------------------------------------------------------------===//
// Traversal Helpers
//===----------------------------------------------------------------------===//

namespace detail {

/// Compute postorder numbering for BSCIR blocks via iterative DFS.
inline SmallVector<BasicBlockId, 32> computePO(const Body &B) {
  SmallVector<BasicBlockId, 32> PO;
  if (B.Blocks.empty())
    return PO;

  llvm::BitVector Visited(B.Blocks.size(), false);

  // Stack entry: (BlockId, successor index into a local successor list).
  // We collect successors once per block to avoid repeated allocation.
  struct StackEntry {
    BasicBlockId Block;
    SmallVector<BasicBlockId, 4> Succs;
    unsigned Idx = 0;
  };
  SmallVector<StackEntry, 32> Stack;

  auto pushBlock = [&](BasicBlockId Id) {
    StackEntry E;
    E.Block = Id;
    B.getBlock(Id).Term.forEachSuccessor(
        [&](BasicBlockId S) { E.Succs.push_back(S); });
    Stack.push_back(std::move(E));
    Visited[Id.Index] = true;
  };

  pushBlock(B.Blocks[0].Id);

  while (!Stack.empty()) {
    auto &Top = Stack.back();
    if (Top.Idx < Top.Succs.size()) {
      BasicBlockId Succ = Top.Succs[Top.Idx++];
      if (Succ.Index < B.Blocks.size() && !Visited[Succ.Index])
        pushBlock(Succ);
    } else {
      PO.push_back(Top.Block);
      Stack.pop_back();
    }
  }
  return PO;
}

/// Compute reverse-postorder numbering for BSCIR blocks.
inline SmallVector<BasicBlockId, 32> computeRPO(const Body &B) {
  auto PO = computePO(B);
  SmallVector<BasicBlockId, 32> RPO(PO.rbegin(), PO.rend());
  return RPO;
}


} // namespace detail

//===----------------------------------------------------------------------===//
// Forward Analysis Solver
//===----------------------------------------------------------------------===//

template <typename Lattice>
DataflowResult<Lattice>
runForwardAnalysis(const Body &B,
                   const DataflowAnalysis<Direction::Forward, Lattice> &Analysis) {
  DataflowResult<Lattice> Result;
  if (B.Blocks.empty())
    return Result;

  auto RPO = detail::computeRPO(B);

  BasicBlockId EntryId = B.Blocks[0].Id;
  Result.EntryStates[EntryId] = Analysis.entryState(B);

  llvm::BitVector InWorklist(B.Blocks.size(), false);
  std::queue<BasicBlockId> Worklist;
  for (BasicBlockId Id : RPO) {
    Worklist.push(Id);
    InWorklist[Id.Index] = true;
  }

  while (!Worklist.empty()) {
    BasicBlockId BId = Worklist.front();
    Worklist.pop();
    InWorklist[BId.Index] = false;

    const BasicBlock &BB = B.getBlock(BId);

    // Compute entry state by merging predecessor exit states
    if (BId != EntryId) {
      bool First = true;
      for (BasicBlockId Pred : B.getPredecessors(BId)) {
        auto ExitIt = Result.ExitStates.find(Pred);
        if (ExitIt == Result.ExitStates.end())
          continue;

        Lattice EdgeState = Analysis.transferTerminator(
            B.getBlock(Pred).Term, ExitIt->second, BId);

        if (First) {
          Result.EntryStates[BId] = std::move(EdgeState);
          First = false;
        } else {
          Analysis.merge(EdgeState, Result.EntryStates[BId]);
        }
      }
      if (First)
        continue;
    }

    // Apply transfer functions for all statements
    Lattice State = Result.EntryStates[BId];
    for (const Statement &Stmt : BB.Statements) {
      Analysis.transferStatement(Stmt, State);
    }

    // Check if exit state changed: merge new state into old.
    // merge(Src, Dst) joins Src into Dst and returns true if Dst changed.
    auto OldIt = Result.ExitStates.find(BId);
    bool Changed = (OldIt == Result.ExitStates.end());
    if (Changed) {
      Result.ExitStates[BId] = std::move(State);
    } else {
      Changed = Analysis.merge(State, OldIt->second);
      // OldIt->second now holds the join — no need to overwrite.
    }

    if (Changed) {
      BB.Term.forEachSuccessor([&](BasicBlockId Succ) {
        if (Succ.Index < B.Blocks.size() && !InWorklist[Succ.Index]) {
          Worklist.push(Succ);
          InWorklist[Succ.Index] = true;
        }
      });
    }
  }

  return Result;
}

//===----------------------------------------------------------------------===//
// Backward Analysis Solver
//===----------------------------------------------------------------------===//

template <typename Lattice>
DataflowResult<Lattice>
runBackwardAnalysis(const Body &B,
                    const DataflowAnalysis<Direction::Backward, Lattice> &Analysis) {
  DataflowResult<Lattice> Result;
  if (B.Blocks.empty())
    return Result;

  auto PO = detail::computePO(B);

  // Find exit blocks
  SmallVector<BasicBlockId, 4> ExitBlocks;
  for (const auto &BB : B.Blocks) {
    if (BB.Term.K == Terminator::Return || BB.Term.K == Terminator::Unreachable)
      ExitBlocks.push_back(BB.Id);
  }

  Lattice InitState = Analysis.entryState(B);
  for (BasicBlockId ExitId : ExitBlocks)
    Result.ExitStates[ExitId] = InitState;

  llvm::BitVector InWorklist(B.Blocks.size(), false);
  std::queue<BasicBlockId> Worklist;
  for (BasicBlockId Id : PO) {
    Worklist.push(Id);
    InWorklist[Id.Index] = true;
  }

  while (!Worklist.empty()) {
    BasicBlockId BId = Worklist.front();
    Worklist.pop();
    InWorklist[BId.Index] = false;

    const BasicBlock &BB = B.getBlock(BId);

    bool IsExit = (BB.Term.K == Terminator::Return ||
                   BB.Term.K == Terminator::Unreachable);
    if (!IsExit) {
      bool First = true;
      BB.Term.forEachSuccessor([&](BasicBlockId Succ) {
        auto EntryIt = Result.EntryStates.find(Succ);
        if (EntryIt == Result.EntryStates.end())
          return;
        if (First) {
          Result.ExitStates[BId] = EntryIt->second;
          First = false;
        } else {
          Analysis.merge(EntryIt->second, Result.ExitStates[BId]);
        }
      });
      if (First)
        continue;
    }

    Lattice State = Result.ExitStates[BId];
    for (auto It = BB.Statements.rbegin(); It != BB.Statements.rend(); ++It)
      Analysis.transferStatement(*It, State);

    auto OldIt = Result.EntryStates.find(BId);
    bool Changed = (OldIt == Result.EntryStates.end());
    if (Changed) {
      Result.EntryStates[BId] = std::move(State);
    } else {
      Changed = Analysis.merge(State, OldIt->second);
      // OldIt->second now holds the join — no need to overwrite.
    }

    if (Changed) {
      for (BasicBlockId Pred : B.getPredecessors(BId)) {
        if (Pred.Index < B.Blocks.size() && !InWorklist[Pred.Index]) {
          Worklist.push(Pred);
          InWorklist[Pred.Index] = true;
        }
      }
    }
  }

  return Result;
}

} // namespace bscir
} // namespace clang

#endif // ENABLE_BSC
#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDATAFLOW_H
