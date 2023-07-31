//===- DeclBSC.h - Classes for representing BSC declarations --*- BSC -*-=====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Defines the BSC Decl subclasses
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_DECLBSC_H
#define LLVM_CLANG_AST_DECLBSC_H

#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclarationName.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeOrdering.h"

namespace clang {

class ASTContext;
class IdentifierInfo;
class UsingDecl;

class BSCMethodDecl : public FunctionDecl {
protected:
  BSCMethodDecl(Kind DK, ASTContext &C, DeclContext *RD,
                SourceLocation StartLoc, const DeclarationNameInfo &NameInfo,
                QualType T, TypeSourceInfo *TInfo, StorageClass SC,
                bool UsesFPIntrin, bool isInline,
                ConstexprSpecKind ConstexprKind, SourceLocation EndLocation,
                Expr *TrailingRequiresClause = nullptr, bool isAsync = false)
      : FunctionDecl(DK, C, RD, StartLoc, NameInfo, T, TInfo, SC, UsesFPIntrin,
                     isInline, ConstexprKind, TrailingRequiresClause, isAsync) {
    if (EndLocation.isValid())
      setRangeEnd(EndLocation);
  }

public:
  static BSCMethodDecl *
  Create(ASTContext &C, DeclContext *RD, SourceLocation StartLoc,
         const DeclarationNameInfo &NameInfo, QualType T, TypeSourceInfo *TInfo,
         StorageClass SC, bool UsesFPIntrin, bool isInline,
         ConstexprSpecKind ConstexprKind, SourceLocation EndLocation,
         Expr *TrailingRequiresClause = nullptr, bool isAsync = false);
  static BSCMethodDecl *CreateDeserialized(ASTContext &C, unsigned ID);

  bool getHasThisParam() const { return HasThisParam; }
  void setHasThisParam(bool HasThisParam) { this->HasThisParam = HasThisParam; }
  QualType getExtendedType() const { return ExtendedType; }
  void setExtendedType(QualType ExtendedType) {
    this->ExtendedType = ExtendedType;
  }

  /// Returns the start sourcelocation of extended type in BSCMethodDecl.
  SourceLocation getExtentedTypeBeginLoc() { return BLoc; }
  void setExtentedTypeBeginLoc(SourceLocation L) { BLoc = L; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) { return classofKind(D->getKind()); }
  static bool classofKind(Kind K) { return K == BSCMethod; }

private:
  QualType ExtendedType;
  bool HasThisParam = false;
  SourceLocation BLoc;
};

class TraitDecl : public TagDecl {
  RecordDecl *TraitR = nullptr;
  RecordDecl *Vtable = nullptr;
  std::map<QualType, VarDecl *, QualTypeOrdering> TypeImpled;

public:
  void MapInsert(QualType QT, VarDecl *VD) {
    TypeImpled.insert(std::pair<QualType, VarDecl *>(QT, VD));
  }

  VarDecl *getTypeImpledVarDecl(QualType QT) {
    std::map<QualType, VarDecl *, QualTypeOrdering>::iterator find;
    find = TypeImpled.find(QT);
    if (find == TypeImpled.end())
      return nullptr;
    return find->second;
  }

  void dumpTypeImplMap() {
    for (auto i = TypeImpled.begin(); i != TypeImpled.end(); i++) {
      llvm::outs() << "[key]:\n";
      i->first->dump();
      llvm::outs() << "[value]:\n";
      i->second->dump();
    }
  }

  friend class DeclContext;

protected:
  TraitDecl(Kind DK, TagKind TK, const ASTContext &C, DeclContext *DC,
            SourceLocation StartLoc, SourceLocation IdLoc, IdentifierInfo *Id,
            TraitDecl *PrevDecl);

public:
  static TraitDecl *Create(const ASTContext &C, TagKind TK, DeclContext *DC,
                           SourceLocation StartLoc, SourceLocation IdLoc,
                           IdentifierInfo *Id, TraitDecl *PrevDecl = nullptr);
  TraitDecl *getPreviousDecl() {
    return cast_or_null<TraitDecl>(
        static_cast<TagDecl *>(this)->getPreviousDecl());
  }
  const TraitDecl *getPreviousDecl() const {
    return const_cast<TraitDecl *>(this)->getPreviousDecl();
  }

  using field_iterator = specific_decl_iterator<FunctionDecl>;
  using field_range =
      llvm::iterator_range<specific_decl_iterator<FunctionDecl>>;

  field_range fields() const { return field_range(field_begin(), field_end()); }
  field_iterator field_begin() const;

  void setTrait(RecordDecl *RD) { TraitR = RD; }

  void setVtable(RecordDecl *RD) { Vtable = RD; }

  RecordDecl *getTrait() { return TraitR; }

  RecordDecl *getVtable() { return Vtable; }

  field_iterator field_end() const { return field_iterator(decl_iterator()); }

  bool field_empty() const { return field_begin() == field_end(); }

  virtual void completeDefinition();

  TraitDecl *getDefinition() const {
    return cast_or_null<TraitDecl>(TagDecl::getDefinition());
  }

  static bool classof(const Decl *D) { return classofKind(D->getKind()); }
  static bool classofKind(Kind K) { return K == Trait; }
};

class ImplTraitDecl : public DeclaratorDecl,
                      public Redeclarable<ImplTraitDecl> {
protected:
  ImplTraitDecl(Kind DK, ASTContext &C, DeclContext *DC,
                SourceLocation StartLoc, SourceLocation IdLoc,
                IdentifierInfo *Id, QualType T, TypeSourceInfo *TInfo,
                StorageClass SC);

  using redeclarable_base = Redeclarable<ImplTraitDecl>;

  TraitDecl *TD;

public:
  using redeclarable_base::getPreviousDecl;

  static ImplTraitDecl *Create(ASTContext &C, DeclContext *DC,
                               SourceLocation StartLoc, SourceLocation IdLoc,
                               IdentifierInfo *Id, QualType T,
                               TypeSourceInfo *TInfo, StorageClass S);

  static ImplTraitDecl *CreateDeserialized(ASTContext &C, unsigned ID);

  SourceRange getSourceRange() const override LLVM_READONLY;

  LanguageLinkage getLanguageLinkage() const;

  bool isInExternCContext() const;

  void setTraitDecl(TraitDecl *D);

  TraitDecl *getTraitDecl();

  ImplTraitDecl *getCanonicalDecl() override;
};

} // namespace clang

#endif // LLVM_CLANG_AST_DECLBSC_H