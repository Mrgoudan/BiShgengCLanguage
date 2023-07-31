//===--- SemaBSCTrait.cpp - Semantic Analysis for BSC Trait ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements semantic analysis for bishengc trait
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBSC.h"
#include "clang/AST/Expr.h"
#include "clang/Sema/Designator.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;
using namespace sema;

RecordDecl *Sema::ActOnDesugarVtableRecord(SourceLocation StartLoc,
                                           SourceLocation NameLoc,
                                           IdentifierInfo *Name) {
  RecordDecl *Result = nullptr;
  std::string TraitVTableName = "__Trait_" + Name->getName().str() + "_Vtable";
  DeclContext::lookup_result VtableDecls =
      getASTContext().getTranslationUnitDecl()->lookup(
          DeclarationName(&Context.Idents.get(TraitVTableName)));
  if (VtableDecls.empty()) {
    Result = RecordDecl::Create(Context, TTK_Struct, CurContext, StartLoc,
                                NameLoc, &Context.Idents.get(TraitVTableName));
    Result->startDefinition();
    Result->completeDefinition();
    PushOnScopeChains(Result, getCurScope());
  } else if (VtableDecls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = VtableDecls.begin(),
                                              E = VtableDecls.end();
         I != E; ++I) {
      if (isa<RecordDecl>(*I)) {
        Result = dyn_cast<RecordDecl>(*I);
      }
    }
  } else {
    // todo: error report?
  }
  return Result;
}

// when we saw a trait like:
// ` trait I {int g(char a);};
// we should generate two struct in ast:
// |--RecordDecl  struct Trait_I_Vtable
// |----FieldDecl  g 'int (*)(char)'
// |--RecordDecl  struct Trait_I
// |----FieldDecl  data (*)(void)
// |----FieldDecl  vtable struct (*)Trait_I_Vtable
void Sema::ActOnDesugarTraitVtable(TraitDecl *Find, SourceLocation StartLoc,
                                   SourceLocation NameLoc, IdentifierInfo *Name,
                                   DeclSpec &DS) {
  for (TraitDecl::field_iterator FieldIt = Find->field_begin();
       FieldIt != Find->field_end(); ++FieldIt) {
    QualType FT = FieldIt->getType();
    QualType PT = Context.getPointerType(FT);
    FieldDecl *NewFD = FieldDecl::Create(
        Context, CurContext, StartLoc, NameLoc, FieldIt->getIdentifier(), PT,
        Context.CreateTypeSourceInfo(PT), nullptr, false, ICIS_CopyInit);
    PushOnScopeChains(NewFD, getCurScope());
  }
}

RecordDecl *Sema::ActOnDesugarTraitRecord(SourceLocation StartLoc,
                                          SourceLocation NameLoc,
                                          IdentifierInfo *Name) {
  std::string TraitName = "__Trait_" + Name->getName().str();
  RecordDecl *NewTrait =
      RecordDecl::Create(Context, TTK_Struct, CurContext, StartLoc, NameLoc,
                         &Context.Idents.get(TraitName));
  NewTrait->startDefinition();
  NewTrait->completeDefinition();
  PushOnScopeChains(NewTrait, getCurScope());
  return NewTrait;
}

void Sema::ActOnDesugarTrait(RecordDecl *TraitVtableRecord,
                             SourceLocation StartLoc, SourceLocation NameLoc) {
  std::string DataName = "data";
  std::string VtableName = "vtable";
  QualType DataPT = Context.getPointerType(Context.VoidTy);
  QualType RecordTy = Context.getRecordType(TraitVtableRecord);
  QualType VtablePT = Context.getPointerType(RecordTy);
  FieldDecl *DataFD = FieldDecl::Create(Context, CurContext, StartLoc, NameLoc,
                                        &Context.Idents.get(DataName), DataPT,
                                        Context.CreateTypeSourceInfo(DataPT),
                                        nullptr, false, ICIS_CopyInit);
  FieldDecl *VtableFD = FieldDecl::Create(
      Context, CurContext, StartLoc, NameLoc, &Context.Idents.get(VtableName),
      VtablePT, Context.CreateTypeSourceInfo(VtablePT), nullptr, false,
      ICIS_CopyInit);
  PushOnScopeChains(DataFD, getCurScope());
  PushOnScopeChains(VtableFD, getCurScope());
}

ImplTraitDecl *Sema::BuildImplTraitDecl(Scope *S, Declarator &D,
                                        SourceLocation TypeLoc, TraitDecl *TD) {
  if (IsImplTraitDeclIllegal(D, TypeLoc, TD))
    return nullptr;
  DeclContext *DC = CurContext;
  TypeSourceInfo *TInfo = GetTypeForDeclarator(D, S);

  QualType R = TInfo->getType();
  DeclarationName Name = GetNameForDeclarator(D).getName();
  IdentifierInfo *II = Name.getAsIdentifierInfo();

  // We should move this piece of code.
  ImplTraitDecl *ITD = nullptr;
  ITD =
      ImplTraitDecl::Create(Context, DC, D.getBeginLoc(), D.getIdentifierLoc(),
                            II, R, TInfo, StorageClass::SC_None);
  ITD->setTraitDecl(TD);
  CurContext->addDecl(ITD);
  return ITD;
}

TraitDecl *Sema::FindTraitDecl(IdentifierInfo *II) {
  DeclContext::lookup_result Decls =
      getASTContext().getTranslationUnitDecl()->lookup(II);
  for (DeclContext::lookup_result::iterator I = Decls.begin(), E = Decls.end();
       I != E; ++I)
    if (isa<TraitDecl>(*I))
      return dyn_cast<TraitDecl>(*I);
  return nullptr;
}

void Sema::ActOnFinishTraitMemberSpecification(Decl *TagDecl) {
  if (!TagDecl)
    return;
  AdjustDeclIfTemplate(TagDecl);
}

ExprResult Sema::AddAfterStructTrait(ExprResult ULE, SourceLocation DSLoc,
                                     StringRef ID) {
  CXXScopeSpec SS;
  SourceLocation TemplateKWLoc;
  UnqualifiedId Name;
  IdentifierInfo *VId = &Context.Idents.get(ID);
  Name.setIdentifier(VId, DSLoc);
  ULE = ActOnMemberAccessExpr(getCurScope(), ULE.get(), DSLoc, tok::period, SS,
                              TemplateKWLoc, Name, nullptr);
  return ULE;
}

Expr *Sema::ConvertParmTraitToStructTrait(Expr *UO, QualType ProtoArgType,
                                          SourceLocation DSLoc) {
  QualType T = UO->getType();
  const PointerType *PT = dyn_cast_or_null<PointerType>(T.getTypePtr());
  RecordDecl *RD = ProtoArgType.getTypePtr()->getAsRecordDecl();
  TraitDecl *TD = RD->getDesugaredTraitDecl();
  if (!PT) {
    Diag(DSLoc, diag::err_type_has_not_impl_trait)
        << TD->getNameAsString() << T;
    return nullptr;
  }
  // For "impl trait TR for struct S",
  // this might be a ElaboratedType for "struct S"
  T = PT->getPointeeType().getCanonicalType();
  VarDecl *LookUpVar = TD->getTypeImpledVarDecl(T);
  if (!LookUpVar) {
    Diag(DSLoc, diag::err_type_has_not_impl_trait)
        << TD->getNameAsString() << T;
    return nullptr;
  }

  LookUpVar->setIsUsed();
  RecordDecl *LookUpVtable = TD->getVtable();
  QualType VoidPT = Context.getPointerType(Context.VoidTy);
  QualType VtableTy = Context.getRecordType(LookUpVtable);
  QualType VtablePT = Context.getPointerType(VtableTy);

  ImplicitCastExpr *TraitData =
      ImplicitCastExpr::Create(Context, VoidPT, CK_BitCast, UO, nullptr,
                               VK_PRValue, FPOptionsOverride());
  DeclRefExpr *VtableRef =
      DeclRefExpr::Create(Context, NestedNameSpecifierLoc(), DSLoc, LookUpVar,
                          false, DSLoc, VtableTy, VK_LValue);
  UnaryOperator *UOVtable =
      UnaryOperator::Create(Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue,
                            OK_Ordinary, DSLoc, false, FPOptionsOverride());

  std::vector<Expr *> Exprs = {TraitData, UOVtable};
  MutableArrayRef<Expr *> initExprs = MutableArrayRef<Expr *>(Exprs);
  ExprResult Result = ActOnInitList(DSLoc, initExprs, DSLoc);
  TypeSourceInfo *TInfo = Context.CreateTypeSourceInfo(ProtoArgType);
  TypeResult TR = CreateParsedType(ProtoArgType, TInfo);
  Result = ActOnCompoundLiteral(DSLoc, TR.get(), DSLoc, Result.get());
  return Result.get();
}

VarDecl *Sema::DesugarImplTrait(ImplTraitDecl *ITD, Declarator &D) {
  TraitDecl *TD = ITD->getTraitDecl();
  RecordDecl *TraitRecord = TD->getVtable();
  SourceLocation TraitLoc = ITD->getLocation();
  CXXScopeSpec SS;
  Scope *S = getCurScope();
  DeclContext *DC = CurContext;
  QualType QT = TraitRecord->getTypeForDecl()->getCanonicalTypeInternal();
  TypeSourceInfo *TInfo = GetTypeForDeclarator(D, S);
  QualType T = TInfo->getType().getCanonicalType();
  VarDecl *LookUpVar = TD->getTypeImpledVarDecl(
      T); // If we have the same ImplTraitDecl before, return nullptr
  if (LookUpVar)
    return nullptr;
  PrintingPolicy PrintPolicy = LangOptions();
  SplitQualType T_split = T.split();
  StringRef Ty = T.getAsString(T_split, PrintPolicy);
  int n = Ty.find(' ');
  std::string Tmp;
  if (n > 0)
    Tmp = Ty.str().substr(0, n) + "_" + Ty.str().substr(n + 1, -1);
  else
    Tmp = Ty.str();
  StringRef Prof = Tmp;
  Tmp = "__" + Prof.str() + "_trait_" + D.getIdentifier()->getName().str();
  StringRef ImplTraitName = Tmp;
  IdentifierInfo *ITII = &Context.Idents.get(ImplTraitName);
  StorageClass SC = clang::SC_None;
  VarDecl *NewVD = VarDecl::Create(Context, DC, TraitLoc, D.getIdentifierLoc(),
                                   ITII, QT, TInfo, SC);
  NewVD->setLexicalDeclContext(CurContext);
  PushOnScopeChains(NewVD, S, true);

  SmallVector<Expr *, 12> InitExprs;
  for (TraitDecl::field_iterator FieldIt = TD->field_begin();
       FieldIt != TD->field_end(); ++FieldIt) {
    IdentifierInfo *FunctionID = FieldIt->getIdentifier();
    Designation Desig;
    Desig.AddDesignator(Designator::getField(FunctionID, TraitLoc, TraitLoc));
    UnqualifiedId Id;
    Id.setIdentifier(FunctionID, TraitLoc);
    TemplateArgumentListInfo TemplateArgsBuffer;
    DeclarationNameInfo NameInfo;
    const TemplateArgumentListInfo *TemplateArgs;
    DecomposeUnqualifiedId(Id, TemplateArgsBuffer, NameInfo, TemplateArgs);
    LookupResult R(*this, NameInfo, LookupOrdinaryName);
    LookupParsedName(R, S, &SS, true, false, T);
    ExprResult Res = BuildTemplateIdExpr(SS, TraitLoc, R, false, TemplateArgs);
    Res.get()->HasBSCScopeSpec = true;
    typedef DesignatedInitExpr::Designator ASTDesignator;
    SmallVector<ASTDesignator, 32> Designators;
    SmallVector<Expr *, 32> InitExpressions;
    const Designator &DDD = Desig.getDesignator(0);
    Designators.push_back(ASTDesignator(DDD.getField(), TraitLoc, TraitLoc));
    Desig.ClearExprs(*this);
    Res = DesignatedInitExpr::Create(Context, Designators, InitExpressions,
                                     TraitLoc, false, Res.getAs<Expr>());
    Res = this->CorrectDelayedTyposInExpr(Res.get());
    InitExprs.push_back(Res.get());
  }

  ExprResult LHS = this->ActOnInitList(TraitLoc, InitExprs, TraitLoc);

  Expr *Init = LHS.get();
  QualType DclT = NewVD->getType();
  InitializedEntity Entity = InitializedEntity::InitializeVariable(NewVD);
  InitializationKind Kind =
      InitializationKind::CreateForInit(TraitLoc, false, Init);
  MultiExprArg Args = Init;
  ExprResult ResTmpFor = CorrectDelayedTyposInExpr(
      Args[0], NewVD, true, [this, Entity, Kind](Expr *E) {
        InitializationSequence Init(*this, Entity, Kind, MultiExprArg(E));
        return Init.Failed() ? ExprError() : E;
      });
  Args[0] = ResTmpFor.get();
  InitializationSequence InitSeq(*this, Entity, Kind, Args, false, false);
  ExprResult ResultTmpIf = InitSeq.Perform(*this, Entity, Kind, Args, &DclT);
  Init = ResultTmpIf.getAs<Expr>();
  ExprResult Result =
      ActOnFinishFullExpr(Init, TraitLoc, false, NewVD->isConstexpr());
  Init = Result.get();
  NewVD->setInit(Init);
  TD->MapInsert(T, NewVD);
  return NewVD;
}
QualType Sema::DesugarTraitToStructTrait(QualType T) {
  T = T.getCanonicalType();
  TraitDecl *TD = dyn_cast<TraitDecl>(T.getTypePtr()->getAsTagDecl());
  RecordDecl *RD = TD->getTrait();
  if (RD == nullptr)
    return T;
  return RD->getTypeForDecl()->getCanonicalTypeInternal();
}

// In this function, we aim to assign the Trait_xxx struct,
// for example, we have desugared as follows:
//`struct Trait_I {
//`  void *data;
//`  void *vtable;
//`}
// now we parsing a stmt like: trait I *i = &s; (s is a struct S instance)
// or: trait I *i = (trait I*)&s; (s is a struct S instance)
// we should give the Trait_I assignment like:
//`struct Trait_I trait_i = {
//  .data = &s;
//  .vtable = *struct_S_Trait_I_Vtable;
//`}
VarDecl *Sema::ActOnDesugarTraitInstance(Declarator &D, QualType QT,
                                         VarDecl *VarDec) {
  // build expr: .data = &s;
  TraitDecl *TD = dyn_cast<TraitDecl>(QT.getTypePtr()->getAsTagDecl());
  if (TD->getVtable() == nullptr) {
    Diag(VarDec->getLocation(), diag::err_typecheck_decl_incomplete_type) << QT;
    return nullptr;
  }
  RecordDecl *LookUpTrait = TD->getTrait();
  RecordDecl *LookUpVtable = TD->getVtable();
  QualType RecordTy = Context.getRecordType(LookUpTrait);
  VarDecl *NewVD = VarDecl::Create(
      Context, CurContext, D.getBeginLoc(), D.getIdentifierLoc(),
      VarDec->getIdentifier(), RecordTy, Context.CreateTypeSourceInfo(RecordTy),
      StorageClass::SC_None);
  PushOnScopeChains(NewVD, getCurScope(), true);
  Expr *exp = VarDec->getInit();
  if (exp == nullptr) // trait I *a;
    return NewVD;

  CastExpr *Cexpr = dyn_cast<CastExpr>(exp);
  if (!Cexpr) {
    AddInitializerToDecl(NewVD, exp, false);
    return NewVD;
  }

  Expr *UO = Cexpr->getSubExpr();
  QualType T = UO->getType();
  const PointerType *PT = dyn_cast_or_null<PointerType>(T.getTypePtr());
  if (!PT) {
    Diag(UO->getBeginLoc(), diag::err_type_has_not_impl_trait)
        << TD->getNameAsString() << T;
    return nullptr;
  }
  T = PT->getPointeeType().getCanonicalType();
  VarDecl *LookUpVar = TD->getTypeImpledVarDecl(T);
  if (!LookUpVar) {
    Diag(UO->getBeginLoc(), diag::err_type_has_not_impl_trait)
        << TD->getNameAsString() << T;
    return nullptr;
  }

  QualType VoidPT = Context.getPointerType(Context.VoidTy);
  ImplicitCastExpr *TraitData =
      ImplicitCastExpr::Create(Context, VoidPT,
                               /* CastKind=*/CK_BitCast,
                               /* Expr=*/UO,
                               /* CXXCastPath=*/nullptr,
                               /* ExprValueKind=*/VK_PRValue,
                               /* FPFeatures */ FPOptionsOverride());

  QualType VtableTy = Context.getRecordType(LookUpVtable);
  QualType VtablePT = Context.getPointerType(VtableTy);
  DeclRefExpr *VtableRef = DeclRefExpr::Create(
      Context, NestedNameSpecifierLoc(), SourceLocation(), LookUpVar, false,
      SourceLocation(), VtableTy, VK_LValue);
  UnaryOperator *UOVtable = UnaryOperator::Create(
      Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue, OK_Ordinary,
      SourceLocation(), false, FPOptionsOverride());

  std::vector<Expr *> Exprs = {TraitData, UOVtable};
  MutableArrayRef<Expr *> initExprs = MutableArrayRef<Expr *>(Exprs);
  ExprResult TraitInit =
      ActOnInitList(SourceLocation(), initExprs, SourceLocation());
  InitListExpr *ResList = dyn_cast<InitListExpr>(TraitInit.get());
  AddInitializerToDecl(NewVD, ResList, false);
  return NewVD;
}

NamedDecl *Sema::ActOnTraitMemberDeclarator(Scope *S, Declarator &D) {
  D.setIsTraitMember(true);
  MultiTemplateParamsArg TemplateParams;
  NamedDecl *Member = HandleDeclarator(S, D, TemplateParams);
  return Member;
}

bool Sema::ShouldDesugarTrait(QualType T) {
  if (RecordDecl *RD = T.getTypePtr()->getAsRecordDecl())
    return RD->getDesugaredTraitDecl() != nullptr;
  return false;
}

// Handling reassignments of variable types with trait pointers:
// trait F* f = &a;
// f = &a;
ExprResult Sema::ActOnTraitReassign(Scope *S, SourceLocation TokLoc,
                                    BinaryOperatorKind Opc, RecordDecl *RD,
                                    Expr *LHSExpr, Expr *RHSExpr) {
  Expr *Bin1 = nullptr;
  Expr *Bin2 = nullptr;
  QualType T = RHSExpr->getType()->getPointeeType().getCanonicalType();
  for (RecordDecl::field_iterator I = RD->field_begin(), E = RD->field_end();
       I != E; ++I) {
    Expr *NewLHSExpr = BuildMemberExpr(
        LHSExpr, false, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *I, DeclAccessPair::make(*I, I->getAccess()), false,
        DeclarationNameInfo(), I->getType(), VK_LValue, OK_Ordinary);
    Expr *NewRHSExpr = nullptr;
    if (I->getNameAsString() == "data") {
      NewRHSExpr =
          ImplicitCastExpr::Create(Context, I->getType(), CK_BitCast, RHSExpr,
                                   nullptr, VK_PRValue, FPOptionsOverride());
    } else {
      TraitDecl *TD = RD->getDesugaredTraitDecl();
      RecordDecl *LookUpVtable = TD->getVtable();
      VarDecl *LookUpVar = TD->getTypeImpledVarDecl(T);
      QualType VtableTy = Context.getRecordType(LookUpVtable);
      QualType VtablePT = Context.getPointerType(VtableTy);
      DeclRefExpr *VtableRef = BuildDeclRefExpr(LookUpVar, VtableTy, VK_LValue,
                                                LookUpVar->getLocation());
      NewRHSExpr = UnaryOperator::Create(
          Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue, OK_Ordinary,
          SourceLocation(), false, FPOptionsOverride());
    }
    if (Bin1 == nullptr)
      Bin1 = BuildBinOp(S, TokLoc, Opc, NewLHSExpr, NewRHSExpr)
                 .get(); // f.data = &a;WWWW
    else
      Bin2 = BuildBinOp(S, TokLoc, Opc, NewLHSExpr, NewRHSExpr)
                 .get(); // f.vtable = &__int_trait_T;
  }
  return BuildBinOp(S, TokLoc, BO_Comma, Bin1,
                    Bin2); // f.data = &a, f.vtable = &__int_trait_T;
}
