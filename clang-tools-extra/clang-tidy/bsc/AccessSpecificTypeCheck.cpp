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
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang::ast_matchers;
using namespace std;

static llvm::cl::opt<bool> CheckLoadOnly(
    "check-load-only",
    llvm::cl::desc(
        "Only check load situation when access specific type."),
    llvm::cl::init(false), llvm::cl::Hidden);


namespace clang {
namespace tidy {
namespace bsc {

// Avoid the store situation, control by opiton "check-load-only".
llvm::SmallVector<const clang::Expr *, 6> AoivdStoreMemberList;
// Avoid repeated warnings of certain function calls.
llvm::SmallVector<const clang::Expr *, 6> AvoidCallMemberList;

void AccessSpecificTypeCheck::registerMatchers(MatchFinder *Finder) {
  for (auto TargetType : TargetTypes) {
    Finder->addMatcher(binaryOperator(hasOperatorName("="), hasLHS(memberExpr(hasType(asString(std::string(TargetType)))))).bind("AvoidStore"), this);
    Finder->addMatcher(memberExpr(hasType(asString(std::string(TargetType))), hasAncestor(callExpr(callee(namedDecl(hasAnyName(AvoidCalls)))))).bind("AvoidCall"), this);
    Finder->addMatcher(memberExpr(hasType(asString(std::string(TargetType)))).bind("TargetType"), this);
  }
}

void AccessSpecificTypeCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *StoreTypeMember = Result.Nodes.getNodeAs<BinaryOperator>("AvoidStore");
  const auto *AvoidCallMember = Result.Nodes.getNodeAs<MemberExpr>("AvoidCall");
  const auto *TargetTypeMember = Result.Nodes.getNodeAs<MemberExpr>("TargetType");

  if (StoreTypeMember) {
    const auto *LHS = dyn_cast<MemberExpr>(StoreTypeMember->getLHS());
    if (LHS) {
      AoivdStoreMemberList.push_back(LHS);
    }
  }

  if (AvoidCallMember)
    AvoidCallMemberList.push_back(AvoidCallMember);
  
  if (TargetTypeMember) {
    std::string TypeName = TargetTypeMember->getType().getAsString();
    SourceLocation SL = TargetTypeMember->getSourceRange().getEnd();

    auto AvoidStoreResult = std::find(AoivdStoreMemberList.begin(), AoivdStoreMemberList.end(), TargetTypeMember);
    auto AvoidCallResult = std::find(AvoidCallMemberList.begin(), AvoidCallMemberList.end(), TargetTypeMember);

    if (CheckLoadOnly) {
      if (AvoidStoreResult == AoivdStoreMemberList.end()
          && AvoidCallResult == AvoidCallMemberList.end()) {
            diag(SL, "Found access to a field with target type : " + TypeName, DiagnosticIDs::Warning);
          } 
    } else {
      if (AvoidCallResult == AvoidCallMemberList.end())
        diag(SL, "Found access to a field with target type : " + TypeName, DiagnosticIDs::Warning);
    }
  } 
}

} // namespace bsc
} // namespace tidy
} // namespace clang
