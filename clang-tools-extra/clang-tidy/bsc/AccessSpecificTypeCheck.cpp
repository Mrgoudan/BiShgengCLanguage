//===--- AccessSpecificTypeCheck.cpp - clang-tidy -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AccessSpecificTypeCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace bsc {

void AccessSpecificTypeCheck::registerMatchers(MatchFinder *Finder) {
  for (auto TargetType : TargetTypes) {
    Finder->addMatcher(memberExpr(hasType(asString(std::string(TargetType)))).bind("x"), this);
  }
}

void AccessSpecificTypeCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *TargetTypeMember = Result.Nodes.getNodeAs<MemberExpr>("x");

  if (TargetTypeMember) {
    std::string TypeName = TargetTypeMember->getType().getAsString();
    SourceLocation SL = TargetTypeMember->getSourceRange().getEnd();
    diag(SL, "Found access to a field with target type : " + TypeName, DiagnosticIDs::Warning);
  }
}

} // namespace bsc
} // namespace tidy
} // namespace clang
