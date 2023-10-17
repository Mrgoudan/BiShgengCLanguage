//===--- ParseExprBSC.cpp - BSC Expression Parsing ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Expression parsing implementation for BSC.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Parse/Parser.h"
#include "clang/AST/ASTContext.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

ExprResult Parser::ParseOptionalBSCScopeSpecifier(
    CastParseKind ParseKind, bool isAddressOfOperand, bool &NotCastExpr,
    TypeCastState isTypeCast, bool isVectorLiteral, bool *NotPrimaryExpression,
    bool HasBSCScopeSpec) {
  ParsingDeclSpec DS(*this);
  ParseBSCScopeSpecifiers(DS);
  ParsedAttributes EmptyDeclSpecAttrs(AttrFactory);
  ParsingDeclarator D(*this, DS, EmptyDeclSpecAttrs, DeclaratorContext::File);

  BSCScopeSpec BSS;
  BSS.setBeginLoc(DS.getBeginLoc());
  QualType T =
      Actions.ConvertBSCScopeSpecToType(D, DS.getBeginLoc(), false, BSS, DS);
  // For generic member functions, if the generic 'T' comes from a scope when
  // called, we need to obtain the NestedNameSpecifier from the scope and store
  // it in QualType to facilitate the creation of 'DependentScopeDeclRefExpr'
  // ast nodes.
  auto *TST = dyn_cast_or_null<TemplateSpecializationType>(T.getCanonicalType().getTypePtr());
  if (Tok.is(tok::coloncolon) && TST) {
    TagDecl *TD = dyn_cast_or_null<TagDecl>(Actions.getASTContext().BSCDeclContextMap[TST]);
    if (TD)
      TD->setQualifierInfo(DS.getTypeSpecScope().getWithLocInContext(Actions.getASTContext()));
  }
  HasBSCScopeSpec = TryConsumeToken(tok::coloncolon);
  D.getBSCScopeSpec() = BSS;

  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::err_expected_unqualified_id) << 0;
    return ExprError();
  }
  return ParseCastExpression(ParseKind, isAddressOfOperand, NotCastExpr,
                             isTypeCast, isVectorLiteral, NotPrimaryExpression,
                             T, HasBSCScopeSpec, DS.getBeginLoc());
}

#endif // ENABLE_BSC