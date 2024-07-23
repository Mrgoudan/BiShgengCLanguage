//===- BSCBorrowCheck.h - Borrow Check for Source CFGs -*- BSC --*-------//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BSC borrow check for source-level CFGs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSCBORROWCHECK_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSCBORROWCHECK_H

#if ENABLE_BSC
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include <string>
#include <utility>

namespace clang {
struct NonLexicalLifetimeRange {
  // The first element of pair is begin, the second is end.
  std::pair<unsigned, unsigned> Range; 
  
  // Record the targets of a borrow during this NonLexicalLifetimeSegment.
  // For owned and other variables, Targets will be void.
  VarDecl* Target;
  
  NonLexicalLifetimeRange() {}

  // These two constructors are for borrow variables, which have binding targets. 
  NonLexicalLifetimeRange(std::pair<unsigned, unsigned> range, VarDecl* target)
      : Range(range), Target(target) {}
  NonLexicalLifetimeRange(unsigned begin, unsigned end, VarDecl* target)
      : Range(std::pair<unsigned, unsigned>(begin, end)), Target(target) {}

  // This constructor is for no-borrow variables, which don't have binding targets. 
  NonLexicalLifetimeRange(unsigned begin, unsigned end)
      : Range(std::pair<unsigned, unsigned>(begin, end)), Target(nullptr)  {}

  bool operator==(const NonLexicalLifetimeRange &Other) const { 
    return Range.first == Other.Range.first && 
           Range.second == Other.Range.second &&
           Target == Other.Target;
  }
};

// NonLexicalLifetimeOfVar: NLL for a VarDecl in a cfg path.
using NonLexicalLifetimeOfVar = llvm::SmallVector<NonLexicalLifetimeRange>;

// NonLexicalLifetime: NLL for all variables in a cfg path.
// map<var, lifetime>:
//     var: 
//         obj_var, owned_var, borrow_var
//     lifetime:
//         obj_lifetime: range
//         owned_lifetime: range
//         borrow_lifetime: vector<pair<range, target>>
using NonLexicalLifetime = llvm::DenseMap<VarDecl*, NonLexicalLifetimeOfVar>;

enum BorrowCheckDiagKind {
  LiveLonger,
  AtMostOneMutBorrow,
  ReturnLocal,
};

struct BorrowCheckDiagInfo {
  std::string Name;
  BorrowCheckDiagKind Kind;
  SourceLocation Loc;

  BorrowCheckDiagInfo(std::string Name, BorrowCheckDiagKind Kind, SourceLocation Loc)
    : Name(Name), Kind(Kind), Loc(Loc) {}

  bool operator==(const BorrowCheckDiagInfo& other) const {
    return Name == other.Name &&
           Kind == other.Kind &&
           Loc == other.Loc;
  }
};

class BorrowCheckDiagReporter {
  Sema &S;
  std::vector<BorrowCheckDiagInfo> DIV;

public:
  BorrowCheckDiagReporter(Sema &S) : S(S) {}
  ~BorrowCheckDiagReporter() { flushDiagnostics(); }

  void addDiagInfo(BorrowCheckDiagInfo &DI) {
    for (auto it = DIV.begin(), ei = DIV.end();
          it != ei; ++it) {
      if (DI == *it)
        return;
    }
    DIV.push_back(DI);
  }

private:
  void flushDiagnostics() {
    // Sort the diag info by their SourceLocations. While not strictly
    // guaranteed to produce them in line/column order, this will provide
    // a stable ordering.
    std::sort(DIV.begin(), DIV.end(), [this](const BorrowCheckDiagInfo &a, const BorrowCheckDiagInfo &b){
      return S.getSourceManager().isBeforeInTranslationUnit(a.Loc, b.Loc);
    });

    for (const BorrowCheckDiagInfo & DI: DIV) {
      switch (DI.Kind) {
        case LiveLonger:
          S.Diag(DI.Loc, diag::err_borrow_live_longer_than_target_var) << DI.Name;
          break;
        case AtMostOneMutBorrow:
          S.Diag(DI.Loc, diag::err_at_most_one_mut_borrow) << DI.Name;
          break;
        case ReturnLocal:
          S.Diag(DI.Loc, diag::err_return_value_borrow_local) << DI.Name;
          break;
        default:
          llvm_unreachable("unknown error type");
          break;
      }
      S.getDiagnostics().increaseBorrowCheckErrors();
    }
  }
};

void runBorrowCheck(const FunctionDecl &fd, const CFG &cfg,
                    BorrowCheckDiagReporter &reporter, ASTContext& Ctx);

} // end namespace clang

#endif // ENABLE_BSC

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSCBORROWCHECK_H