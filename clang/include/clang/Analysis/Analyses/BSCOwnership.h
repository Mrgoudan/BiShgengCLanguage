//===- BSCOwnerShip.h - OwnerShip Analysis for Source CFGs -*- BSC --*--------//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BSC Ownership analysis for source-level CFGs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSCOWNERSHIP_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSCOWNERSHIP_H

#if ENABLE_BSC

#include "clang/AST/Decl.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ImmutableMap.h"
#include "llvm/ADT/ImmutableSet.h"
#include "llvm/ADT/SmallSet.h"
#include <string>

namespace clang {

class CFG;
class CFGBlock;
class Stmt;
class DeclRefExpr;
class SourceManager;

class Ownership : public ManagedAnalysis {
public:
  enum Status {
    Uninitialized = 0x1,
    Null = 0x2,
    Owned = 0x4,
    Moved = 0x8,
    PartialMoved = 0x10,
    AllMoved = 0x20,
  };

  class OwnershipStatus {
  public:
    using OwnershipSet = llvm::BitVector;
    llvm::DenseMap<const VarDecl *, OwnershipSet> status;
    llvm::DenseMap<const VarDecl *, unsigned> pointer;
    using OwnedFields = llvm::SmallVector<llvm::SmallSet<std::string, 8>, 2>;
    llvm::DenseMap<const VarDecl *, OwnedFields> structFields;
    llvm::DenseMap<const VarDecl *, SourceLocation> location;

    bool equals(const OwnershipStatus &V) const;

    bool empty() const;
    bool is(const VarDecl *VD, Status S) const;
    bool has(const VarDecl *VD, Status S) const;
    void set(const VarDecl *VD, Status S);
    void reset(const VarDecl *VD, Status S);
    bool contains(const VarDecl *VD) const;
    void initOwnedFields(RecordDecl *RD, const VarDecl *VD, bool hasInit,
                         std::string fieldName = "");
    void addOwnedField(const VarDecl *VD, std::string name);
    void moveOwnedField(const VarDecl *VD, std::string name);
    void fieldsAllMoved(const VarDecl *VD);
    bool hasMovedField(const VarDecl *VD, std::string name, bool needComplete);

    OwnershipSet getStatus(const VarDecl * VD) const;

    OwnershipStatus() : status(0), pointer(0), structFields(0) {}

    OwnershipStatus(llvm::DenseMap<const VarDecl *, OwnershipSet> status,
                    llvm::DenseMap<const VarDecl *, unsigned> pointer,
                    llvm::DenseMap<const VarDecl *, OwnedFields> structFields)
        : status(status), pointer(pointer), structFields(structFields) {}

    friend class OwnerShip;
  };

private:
  Ownership(void *impl);
  void *impl;
};

enum DiagKind {
  InvalidUse,
  InvalidUseUninit,
  InvalidUseMoved,
  MemoryLeak,
  PointerInnerMoved,
  PointerInnerAssign,
  PointerCast,
  Reassign,
};

struct DiagInfo {
  std::string Name;
  DiagKind Kind;
  SourceLocation Loc;
  unsigned PointerDepth;
  SourceLocation Location;

  DiagInfo(std::string Name, DiagKind Kind, SourceLocation Loc)
    : Name(Name), Kind(Kind), Loc(Loc), PointerDepth(0), Location(SourceLocation()) {}
  
  DiagInfo(std::string Name, DiagKind Kind, SourceLocation Loc, SourceLocation location)
    : Name(Name), Kind(Kind), Loc(Loc), PointerDepth(0), Location(location) {}
  

  bool operator==(const DiagInfo& other) const {
    return Name == other.Name &&
           Kind == other.Kind &&
           Loc == other.Loc &&
           PointerDepth == other.PointerDepth &&
           Location == other.Location;
  }
};

class OwnershipDiagReporter {
  Sema &S;
  std::vector<DiagInfo> DIV;

public:
  OwnershipDiagReporter(Sema &S) : S(S) {}
  ~OwnershipDiagReporter() { flushDiagnostics(); }

  void addDiagInfo(DiagInfo &DI) {
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
    std::sort(DIV.begin(), DIV.end(), [this](const DiagInfo &a, const DiagInfo &b){
      return S.getSourceManager().isBeforeInTranslationUnit(a.Loc, b.Loc);
    });

    for (const DiagInfo & DI: DIV) {
      switch (DI.Kind) {
        case InvalidUse:
          S.Diag(DI.Loc, diag::err_ownership_read_invalid) << DI.Name;
          break;
        case MemoryLeak:
          S.Diag(DI.Loc, diag::err_ownership_scope_memory_leak) << DI.Name;
          break;
        case PointerInnerMoved:
          S.Diag(DI.Loc, diag::err_ownership_pointer_inner_moved) << DI.Name;
          break;
        case PointerInnerAssign:
          S.Diag(DI.Loc, diag::err_ownership_pointer_inner_assign) << DI.Name;
          break;
        case PointerCast:
          S.Diag(DI.Loc, diag::err_ownership_cast_to_void_invalid) << DI.Name;
          break;
        case Reassign:
          S.Diag(DI.Loc, diag::err_ownership_reassign) << DI.Name;
          break;
        case InvalidUseUninit:
          S.Diag(DI.Loc, diag::err_ownership_read_invalid_uninit) << DI.Name;
          break;
        case InvalidUseMoved: {
          unsigned line = S.getSourceManager().getPresumedLineNumber(DI.Location);
          S.Diag(DI.Loc, diag::err_ownership_read_invalid_moved) << DI.Name << line;
          break;
        }
        default:
          llvm_unreachable("unknown error type");
          break;
      }
      S.getDiagnostics().increaseOwnershipErrors();
    }
  }
};

void runOwnershipAnalysis(const FunctionDecl &fd, const CFG &cfg,
                          AnalysisDeclContext &ac,
                          OwnershipDiagReporter &reporter);

} // end namespace clang

#endif // ENABLE_BSC

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSCOWNERSHIP_H