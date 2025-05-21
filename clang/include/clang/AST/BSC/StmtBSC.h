//===- StmtBSC.h - Classes for representing BSC statements ---*- BSC -*-=====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Defines the BSC Stmt subclasses
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STMTBSC_H
#define LLVM_CLANG_AST_STMTBSC_H

#if ENABLE_BSC

#include "clang/AST/Stmt.h"
#include "clang/Sema/Scope.h"
namespace clang {
class ASTContext;

struct ScopeSafeZoneInfo {
  SafeZoneSpecifier SafeZoneSpec;
  SafeZoneSource SafeZoneSrc;
  SourceLocation SafeZoneLoc;
};

class SafeStmt : public Stmt {
  SafeZoneSpecifier SafeZoneSpec;
  Stmt *SubStmt;
  SourceLocation Loc;

public:
  // build safe or unsafe statement
  SafeStmt(SourceLocation loc, SafeZoneSpecifier safeZoneSpec, Stmt *substmt)
      : Stmt(SafeStmtClass), SafeZoneSpec(safeZoneSpec), SubStmt(substmt) {
    setSafeLoc(loc);
  };

  explicit SafeStmt(EmptyShell Empty) : Stmt(SafeStmtClass, Empty) {}

  Stmt *getSubStmt() { return SubStmt; }
  const Stmt *getSubStmt() const { return SubStmt; }
  void setSubStmt(Stmt *SS) { SubStmt = SS; }

  SafeZoneSpecifier getSafeZoneSpecifier() const { return SafeZoneSpec; }
  void setSafeZoneSpecifier(SafeZoneSpecifier sz) { SafeZoneSpec = sz; }

  SourceLocation getSafeLoc() const { return Loc; }
  void setSafeLoc(SourceLocation L) { Loc = L; }

  SourceLocation getBeginLoc() const { return getSafeLoc(); }
  SourceLocation getEndLoc() const LLVM_READONLY {
    return SubStmt->getEndLoc();
  }

  child_range children() { return child_range(&SubStmt, &SubStmt + 1); }
  const_child_range children() const {
    return const_child_range(&SubStmt, &SubStmt + 1);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == SafeStmtClass;
  }
};

} // namespace clang

#endif // ENABLE_BSC

#endif