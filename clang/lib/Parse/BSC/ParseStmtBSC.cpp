//===--- ParseStmtBSC.cpp - Statement and Block Parser -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Statement and Block portions of the Parser
// interface for BSC.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Basic/SourceLocation.h"
#include "clang/Parse/Parser.h"

using namespace clang;

void Parser::CheckStmtTokInSafeZone(tok::TokenKind Kind) {
  if (!Actions.IsInSafeZone()) {
    return;
  }
  switch (Kind) {
  case tok::kw_asm:
    Diag(Tok, diag::err_unsafe_action) << "asm statement";
    break;
  default:
    break;
  }
}
struct ScopeSafeZoneInfo Parser::getCurScopeSafeZoneInfo() {
  if (getCurScope()) {
    return {getCurScope()->getScopeSafeZoneSpecifier(),
            getCurScope()->getScopeSafeZoneSource(),
            getCurScope()->getScopeSafeZoneLoc()};
  }
  return {SZ_None, SZS_Inherit, SourceLocation()};
}

void Parser::setCurScopeSafeZoneInfo(struct ScopeSafeZoneInfo SZ) {
  if (getCurScope()) {
    getCurScope()->setScopeSafeZoneSpecifier(SZ.SafeZoneSpec);
    getCurScope()->setScopeSafeZoneSource(SZ.SafeZoneSrc);
    getCurScope()->setScopeSafeZoneLoc(SZ.SafeZoneLoc);
  }
}

/// ParseSafeExpression:In BSC grammar, use of the '_Safe' or '_Unsafe' keyword to
/// modify statement is permitted, such as `_Safe int a = funcall()`.
///
/// safe-statement
///   '_Safe' statement
///   '_Unsafe' statement
StmtResult Parser::ParseSafeStatement(ParsedStmtContext StmtCtx) {
  SafeZoneSpecifier SafeZoneSpec = SZ_None;
  SourceLocation SafeLoc;
  if (Tok.is(tok::kw__Safe)) {
    SafeZoneSpec = SZ_Safe;
    SafeLoc = ConsumeToken();
  } else if (Tok.is(tok::kw__Unsafe)) {
    SafeZoneSpec = SZ_Unsafe;
    SafeLoc = ConsumeToken();
  }
  struct ScopeSafeZoneInfo newInfo = {SafeZoneSpec, SZS_SafeStmt, SafeLoc};
  struct ScopeSafeZoneInfo oldInfo = getCurScopeSafeZoneInfo();
  setCurScopeSafeZoneInfo(newInfo);
  StmtResult SubStmt = ParseStatement(nullptr, StmtCtx);
  if (SubStmt.isInvalid())
    SubStmt = Actions.ActOnNullStmt(SafeLoc);

  StmtResult Stmt = Actions.ActOnSafeStmt(SafeLoc, SafeZoneSpec, SubStmt.get());
  setCurScopeSafeZoneInfo(oldInfo);
  return Stmt;
}

#endif