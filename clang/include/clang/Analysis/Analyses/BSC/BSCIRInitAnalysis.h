//===- BSCIRInitAnalysis.h - Initialization analysis on BSCIR -*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// P2795-style initialization analysis on BSCIR. In _Safe zones, tracks
// definite initialization of ALL types (not just _Owned). Reports use of
// uninitialized and possibly-uninitialized values.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRINITANALYSIS_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRINITANALYSIS_H

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIR.h"
#include "clang/Analysis/Analyses/BSC/BSCIRDataflow.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include <map>

namespace clang {
namespace bscir {

//===----------------------------------------------------------------------===//
// Init Analysis Lattice
//===----------------------------------------------------------------------===//

/// Initialization state for a single local or place.
enum class InitState : uint8_t {
  Uninitialized, // Definitely not initialized
  MaybeInit,     // Initialized on some paths but not all
  Initialized    // Definitely initialized on all paths
};

/// Identifies a specific field path from a local base.
/// E.g., FieldPath{_1, {0, 1}} = _1.field0.field1
struct FieldPath {
  LocalId Base;
  llvm::SmallVector<unsigned, 4> Indices;

  bool operator<(const FieldPath &O) const {
    if (Base.Index != O.Base.Index)
      return Base.Index < O.Base.Index;
    return Indices < O.Indices;
  }
  bool operator==(const FieldPath &O) const {
    return Base == O.Base && Indices == O.Indices;
  }
};

/// Lattice for initialization analysis.
/// Tracks which locals/places are definitely initialized.
struct InitLattice {
  /// Per-local initialization state.
  llvm::DenseMap<LocalId, InitState> LocalStates;

  /// Per-field initialization state for struct locals.
  /// Key is a FieldPath (base local + recursive field indices).
  std::map<FieldPath, InitState> FieldStates;

  /// For callee-side ensure_init verification: tracks whether *param has been
  /// initialized, keyed by the parameter's LocalId.
  llvm::DenseMap<LocalId, InitState> EnsureInitDerefStates;

  bool operator==(const InitLattice &Other) const {
    return LocalStates == Other.LocalStates &&
           FieldStates == Other.FieldStates &&
           EnsureInitDerefStates == Other.EnsureInitDerefStates;
  }
};

//===----------------------------------------------------------------------===//
// Init Analysis Diagnostics
//===----------------------------------------------------------------------===//

enum class InitDiagKind {
  UseOfUninit,
  UseOfMaybeUninit,
  ReturnUninit,           // return value not initialized on all paths
  ReturnMaybeUninit,      // return value possibly not initialized on all paths
  EnsureInitNotInit,        // ensure_init param not initialized at return
  EnsureInitMaybeNotInit,   // ensure_init param possibly not initialized at return
  EnsureInitPtrAliased,     // ensure_init pointer reassigned or copied before *param initialized
  EnsureInitDerefReadUninit,// *param read before initialization
};

struct InitDiagInfo {
  InitDiagKind Kind;
  SourceLocation Loc;
  std::string VarName;

  InitDiagInfo(InitDiagKind K, SourceLocation L, StringRef Name)
      : Kind(K), Loc(L), VarName(Name.str()) {}
};

//===----------------------------------------------------------------------===//
// Initialization Analysis
//===----------------------------------------------------------------------===//

class InitAnalysis
    : public DataflowAnalysis<Direction::Forward, InitLattice> {
public:
  InitAnalysis(const Body &B, bool CheckAllZones = false)
      : B(B), CheckAllZones(CheckAllZones) {}

  InitLattice entryState(const Body &B) const override;
  bool transferStatement(const Statement &S, InitLattice &State) const override;
  InitLattice transferTerminator(const Terminator &T,
                                 const InitLattice &StateBeforeTerm,
                                 BasicBlockId Target) const override;
  bool merge(const InitLattice &Src, InitLattice &Dst) const override;

  /// Run the analysis and collect diagnostics.
  void run(SmallVectorImpl<InitDiagInfo> &Diags) const;

private:
  const Body &B;
  bool CheckAllZones;

  /// Check if a local/place is initialized in the given state.
  InitState getInitState(const InitLattice &State, LocalId Id) const;

  /// Mark a local as initialized.
  void markInit(InitLattice &State, LocalId Id) const;

  /// Mark a local as uninitialized (e.g., on StorageDead).
  void markUninit(InitLattice &State, LocalId Id) const;

  /// Check an operand for use of uninitialized values.
  void checkOperand(const Operand &Op, const InitLattice &State,
                    SourceLocation Loc,
                    SmallVectorImpl<InitDiagInfo> &Diags) const;

  /// Get the number of fields for a type (0 for unions and non-record types).
  static unsigned getNumFields(QualType Ty);

  /// Get init state of a specific field path.
  InitState getFieldInitState(const InitLattice &State,
                              const FieldPath &FP) const;

  /// Mark a field path as initialized, then recursively promote parents.
  void markFieldInit(InitLattice &State, const FieldPath &FP,
                     bool &Changed) const;

  /// Clear all field states for a local (on StorageLive/Dead).
  void clearFieldStates(InitLattice &State, LocalId Id, bool &Changed) const;

  /// Check if all siblings at the leaf level are Init, promote parent,
  /// then recurse upward.
  void tryPromoteParent(InitLattice &State, const FieldPath &FP,
                        bool &Changed) const;

  /// Extract the full field path from a Place (consecutive leading Field
  /// projections, stopping at Deref/Index or union boundary).
  /// Returns None if no leading Field projection.
  llvm::Optional<FieldPath> getFieldPath(const Place &P) const;

  /// Get the QualType at a given field path prefix for a local.
  QualType getFieldType(LocalId Id, ArrayRef<unsigned> Path) const;

  /// Recursively mark all leaf fields of a struct type as Initialized.
  void markAllFieldsInit(InitLattice &State, LocalId Base,
                         QualType Ty, SmallVector<unsigned, 4> &Prefix) const;

  /// Build a human-readable field-qualified name from a FieldPath.
  std::string buildFieldName(const FieldPath &FP) const;

  /// Check if a FieldPath traverses through a union into a struct's fields.
  /// Returns true if the path enters a union variant that is a struct type
  /// and continues deeper into that struct. Sets UnionDepth to the index
  /// in FP.Indices where the union variant index sits.
  bool isUnionStructFieldPath(const FieldPath &FP,
                              unsigned &UnionDepth) const;

  /// Check if a FieldPath ends at a union variant (enters union but doesn't
  /// continue into struct fields). Used to detect writes like u.f or u.s.
  bool isUnionVariantPath(const FieldPath &FP, unsigned &UnionDepth) const;

  /// Check if there are any FieldStates entries under a union prefix.
  bool hasUnionFieldEntries(const InitLattice &State, LocalId Base,
                            ArrayRef<unsigned> UnionPrefix) const;

  /// Clear all FieldStates entries under a union prefix.
  void clearUnionFieldEntries(InitLattice &State, LocalId Base,
                              ArrayRef<unsigned> UnionPrefix,
                              bool &Changed) const;
};

/// Run initialization analysis on a BSCIR Body and collect diagnostics.
/// Caller is responsible for emitting diagnostics via Sema::Diag().
void runInitAnalysis(const Body &B, SmallVectorImpl<InitDiagInfo> &Diags,
                     bool CheckAllZones = false);

} // namespace bscir
} // namespace clang

#endif // ENABLE_BSC
#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRINITANALYSIS_H
