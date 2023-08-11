//===- DeclCXX.cpp - C++ Declaration AST Node Implementation --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the C++ related Decl classes.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/DeclBSC.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/Linkage.h"

using namespace clang;

//===----------------------------------------------------------------------===//
// BSCMethod Implementation
//===----------------------------------------------------------------------===//
BSCMethodDecl *BSCMethodDecl::Create(
    ASTContext &C, DeclContext *RD, SourceLocation StartLoc,
    const DeclarationNameInfo &NameInfo, QualType T, TypeSourceInfo *TInfo,
    StorageClass SC, bool UsesFPIntrinbool, bool isInline,
    ConstexprSpecKind ConstexprKind, SourceLocation EndLocation,
    Expr *TrailingRequiresClause, bool isAsync) {
  return new (C, RD) BSCMethodDecl(
      BSCMethod, C, RD, StartLoc, NameInfo, T, TInfo, SC, UsesFPIntrinbool,
      isInline, ConstexprKind, EndLocation, TrailingRequiresClause, isAsync);
}

BSCMethodDecl *BSCMethodDecl::CreateDeserialized(ASTContext &C, unsigned ID) {
  return new (C, ID) BSCMethodDecl(
      BSCMethod, C, nullptr, SourceLocation(), DeclarationNameInfo(),
      QualType(), nullptr, SC_None, false, false,
      ConstexprSpecKind::Unspecified, SourceLocation(), nullptr);
}

//===----------------------------------------------------------------------===//
// ImplTraitDecl Implementation
//===----------------------------------------------------------------------===//
TraitDecl::TraitDecl(const ASTContext &C, DeclContext *DC,
                     SourceLocation StartLoc, SourceLocation IdLoc,
                     IdentifierInfo *Id, TraitDecl *PrevDecl)
    : TagDecl(Trait, TTK_Trait, C, DC, IdLoc, Id, PrevDecl, StartLoc) {
  assert(classof(static_cast<Decl *>(this)) && "Invalid Kind!");
}

TraitDecl *TraitDecl::Create(const ASTContext &C, DeclContext *DC,
                             SourceLocation StartLoc, SourceLocation IdLoc,
                             IdentifierInfo *Id, TraitDecl *PrevDecl) {
  TraitDecl *R = new (C, DC) TraitDecl(C, DC, StartLoc, IdLoc, Id, PrevDecl);
  R->setMayHaveOutOfDateDef(C.getLangOpts().Modules);

  C.getTypeDeclType(R, PrevDecl);
  return R;
}

TraitDecl *TraitDecl::CreateDeserialized(ASTContext &C, unsigned ID) {
  TraitDecl *Trait = new (C, ID) TraitDecl(C, nullptr, SourceLocation(),
                                           SourceLocation(), nullptr, nullptr);
  Trait->setMayHaveOutOfDateDef(C.getLangOpts().Modules);
  return Trait;
}

TraitDecl::field_iterator TraitDecl::field_begin() const {
  return field_iterator(decl_iterator(FirstDecl));
}

void TraitDecl::completeDefinition() {
  assert(!isCompleteDefinition() && "Cannot redefine trait!");
  TagDecl::completeDefinition();
}

//===----------------------------------------------------------------------===//
// ImplTraitDecl Implementation
//===----------------------------------------------------------------------===//

ImplTraitDecl::ImplTraitDecl(ASTContext &C, DeclContext *DC,
                             SourceLocation StartLoc, SourceLocation IdLoc,
                             IdentifierInfo *Id, QualType T,
                             TypeSourceInfo *TInfo, StorageClass SC)
    : DeclaratorDecl(ImplTrait, DC, IdLoc, Id, T, TInfo, StartLoc),
      redeclarable_base(C) {}

ImplTraitDecl *ImplTraitDecl::Create(ASTContext &C, DeclContext *DC,
                                     SourceLocation StartL, SourceLocation IdL,
                                     IdentifierInfo *Id, QualType T,
                                     TypeSourceInfo *TInfo, StorageClass S) {
  return new (C, DC) ImplTraitDecl(C, DC, StartL, IdL, Id, T, TInfo, S);
}

ImplTraitDecl *ImplTraitDecl::CreateDeserialized(ASTContext &C, unsigned ID) {
  return new (C, ID)
      ImplTraitDecl(C, nullptr, SourceLocation(), SourceLocation(), nullptr,
                    QualType(), nullptr, SC_None);
}

SourceRange ImplTraitDecl::getSourceRange() const {
  return DeclaratorDecl::getSourceRange();
}

LanguageLinkage ImplTraitDecl::getLanguageLinkage() const {
  return LanguageLinkage::CLanguageLinkage;
}

bool ImplTraitDecl::isInExternCContext() const { return true; }

void ImplTraitDecl::setTraitDecl(TraitDecl *D) { ImplTraitDecl::TD = D; }

TraitDecl *ImplTraitDecl::getTraitDecl() { return ImplTraitDecl::TD; }

ImplTraitDecl *ImplTraitDecl::getCanonicalDecl() { return getFirstDecl(); }