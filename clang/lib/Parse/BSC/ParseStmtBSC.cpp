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

#include "clang/Parse/Parser.h"

using namespace clang;

void Parser::CheckStmtTokInSafeZone(tok::TokenKind Kind) {
  if (!Actions.IsInSafeZone()) {
    return;
  }
  switch (Kind) {
  case tok::identifier: {
    Token Next = NextToken();
    if (Next.is(tok::colon))
      Diag(Tok, diag::err_unsafe_action) << "label statement";
    break;
  }
  case tok::kw_goto:
    Diag(Tok, diag::err_unsafe_action) << "goto statement";
    break;
  case tok::kw_asm:
    Diag(Tok, diag::err_unsafe_action) << "asm statement";
    break;
  default:
    break;
  }
}

#endif