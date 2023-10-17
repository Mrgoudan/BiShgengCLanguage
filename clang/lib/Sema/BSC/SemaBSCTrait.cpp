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

#if ENABLE_BSC

#include "TypeLocBuilder.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/BSC/DeclBSC.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/Sema/Designator.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;
using namespace sema;

// When we see a trait like:
// trait I {
//   int g(This *this);
// };
//
// We generate two structs in ast:
// |--RecordDecl  struct Trait_I_Vtable
//  `---FieldDecl  g 'int (*)(void *this)'
// |--RecordDecl  struct Trait_I
//  `---FieldDecl  data (*)(void)
//  `---FieldDecl  vtable struct (*)Trait_I_Vtable
RecordDecl *Sema::ActOnDesugarVtableRecord(TraitDecl *TD) {
  RecordDecl *TraitVtableRD = nullptr;
  SourceLocation NameLoc = TD->getLocation();
  SourceLocation StartLoc = TD->getBeginLoc();
  std::string TraitVTableName = "__Trait_" + TD->getNameAsString() + "_Vtable";
  DeclContext::lookup_result VtableDecls =
      getASTContext().getTranslationUnitDecl()->lookup(
          DeclarationName(&Context.Idents.get(TraitVTableName)));
  TraitTemplateDecl *TTD = TD->getDescribedTraitTemplate();
  TemplateParameterList *TParams = nullptr;
  bool DelayTypeCreation = false;
  if (TTD) {
    TParams = TTD->getTemplateParameters();
    DelayTypeCreation = true;
  }
  if (VtableDecls.empty()) {
    TraitVtableRD = RecordDecl::Create(
        Context, TTK_Struct, CurContext, StartLoc, NameLoc,
        &Context.Idents.get(TraitVTableName), nullptr, DelayTypeCreation);
    TraitVtableRD->setLexicalDeclContext(CurContext);
    Scope *Outer = getCurScope();
    while ((Outer->getFlags() & Scope::TemplateParamScope) != 0)
      Outer = Outer->getParent();
    if (DelayTypeCreation) {
      ClassTemplateDecl *CTD = ClassTemplateDecl::Create(
          Context, CurContext, StartLoc, &Context.Idents.get(TraitVTableName),
          TParams, TraitVtableRD);
      PushOnScopeChains(CTD, Outer);
      TraitVtableRD->setDescribedClassTemplate(CTD);
      QualType T = CTD->getInjectedClassNameSpecialization();
      T = Context.getInjectedClassNameType(TraitVtableRD, T);
      assert(T->isDependentType() && "Class template type is not dependent?");
      CTD->setLexicalDeclContext(CurContext);
    } else {
      PushOnScopeChains(TraitVtableRD, Outer);
    }
  } else {
    // todo: error report?
  }

  TraitVtableRD->startDefinition();
  for (TraitDecl::field_iterator FieldIt = TD->field_begin();
       FieldIt != TD->field_end(); ++FieldIt) {
    QualType FT = FieldIt->getType();
    if (auto *FPT = FT->getAs<FunctionProtoType>()) {
      SmallVector<QualType, 4> Args;
      for (unsigned i = 0; i < FPT->getNumParams(); i++) {
        QualType T = FPT->getParamType(i);
        if (T->isPointerType() &&
            T->getPointeeType().getCanonicalType() == Context.ThisTy) {
          QualType ThisPT = Context.getPointerType(Context.VoidTy);
          Args.push_back(ThisPT);
        } else {
          Args.push_back(T);
        }
      }
      SourceLocation BL = FieldIt->getBeginLoc();
      SourceLocation EL = FieldIt->getEndLoc();
      IdentifierInfo *Name = TD->getIdentifier();
      QualType FunctionTy = BuildFunctionType(FPT->getReturnType(), Args, BL,
                                              Name, FPT->getExtProtoInfo());
      QualType PT = BuildPointerType(FunctionTy, BL, Name);

      // Set SourceLocation and Param information for TypeSourceInfo to use
      // during serialization
      TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(PT);
      UnqualTypeLoc CurrTL = TInfo->getTypeLoc().getUnqualifiedLoc();
      CurrTL.getAs<PointerTypeLoc>().setStarLoc(BL);
      CurrTL = CurrTL.getNextTypeLoc().getUnqualifiedLoc();
      FunctionTypeLoc FTL = CurrTL.getAs<FunctionTypeLoc>();
      FTL.setLocalRangeBegin(BL);
      FTL.setLocalRangeEnd(EL);
      FTL.setLParenLoc(BL);
      FTL.setRParenLoc(EL);

      FieldDecl *NewFD = FieldDecl::Create(Context, TraitVtableRD, BL, EL,
                                           FieldIt->getIdentifier(), PT, TInfo,
                                           nullptr, false, ICIS_NoInit);
      NewFD->setAccess(AS_public);
      TraitVtableRD->addDecl(NewFD);
    }
  }
  TraitVtableRD->completeDefinition();

  return TraitVtableRD;
}

RecordDecl *Sema::ActOnDesugarTraitRecord(TraitDecl *TD,
                                          RecordDecl *TraitVtableRD) {
  RecordDecl *TraitRD = nullptr;
  SourceLocation NameLoc = TD->getLocation();
  SourceLocation StartLoc = TD->getBeginLoc();
  std::string TraitName = "__Trait_" + TD->getNameAsString();
  TraitTemplateDecl *TTD = TD->getDescribedTraitTemplate();
  TemplateParameterList *TParams = nullptr;
  bool DelayTypeCreation = false;
  if (TTD) {
    TParams = TTD->getTemplateParameters();
    DelayTypeCreation = true;
  }
  DeclContext::lookup_result TraitDecls =
      getASTContext().getTranslationUnitDecl()->lookup(
          DeclarationName(&Context.Idents.get(TraitName)));
  if (TraitDecls.empty()) {
    TraitRD = RecordDecl::Create(Context, TTK_Struct, CurContext, StartLoc,
                                 NameLoc, &Context.Idents.get(TraitName),
                                 nullptr, DelayTypeCreation);
    TraitRD->setLexicalDeclContext(CurContext);

    Scope *Outer = getCurScope();
    while ((Outer->getFlags() & Scope::TemplateParamScope) != 0)
      Outer = Outer->getParent();
    if (DelayTypeCreation) {
      ClassTemplateDecl *CTD = ClassTemplateDecl::Create(
          Context, CurContext, StartLoc, &Context.Idents.get(TraitName),
          TParams, TraitRD);
      PushOnScopeChains(CTD, Outer);
      TraitRD->setDescribedClassTemplate(CTD);
      QualType T = CTD->getInjectedClassNameSpecialization();
      T = Context.getInjectedClassNameType(TraitRD, T);
      assert(T->isDependentType() && "Class template type is not dependent?");
      CTD->setLexicalDeclContext(CurContext);
    } else {
      PushOnScopeChains(TraitRD, Outer);
    }
  } else {
    // todo: error report?
  }

  TraitRD->startDefinition();
  std::string DataName = "data";
  std::string VtableName = "vtable";
  QualType DataPT = Context.getPointerType(Context.VoidTy);
  QualType RecordTy = Context.getRecordType(TraitVtableRD);
  ClassTemplateDecl *CTD = TraitVtableRD->getDescribedClassTemplate();
  if (CTD)
    RecordTy = CTD->getInjectedClassNameSpecialization();
  RecordTy = Context.getElaboratedType(ETK_Struct, nullptr, RecordTy);
  QualType VtablePT = Context.getPointerType(RecordTy);
  TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(VtablePT);

  FieldDecl *DataFD = FieldDecl::Create(
      Context, TraitRD, StartLoc, NameLoc, &Context.Idents.get(DataName),
      DataPT, Context.getTrivialTypeSourceInfo(DataPT), nullptr, false,
      ICIS_NoInit);
  FieldDecl *VtableFD = FieldDecl::Create(
      Context, TraitRD, StartLoc, NameLoc, &Context.Idents.get(VtableName),
      VtablePT, TInfo, nullptr, false, ICIS_NoInit);
  DataFD->setAccess(AS_public);
  VtableFD->setAccess(AS_public);
  TraitRD->addDecl(DataFD);
  TraitRD->addDecl(VtableFD);
  TraitRD->completeDefinition();
  TraitRD->setDesugaredTraitDecl(TD);
  return TraitRD;
}

static std::string TypeAsString(QualType T) {
  PrintingPolicy PrintPolicy = LangOptions();
  SplitQualType T_split = T.split();
  std::string Ty = T.getAsString(T_split, PrintPolicy);
  int n = Ty.find(' ');
  std::string TyName = Ty;
  if (n > 0)
    TyName = Ty.substr(0, n) + "_" + Ty.substr(n + 1, -1);
  return TyName;
}

ImplTraitDecl *Sema::BuildImplTraitDecl(Scope *S, Declarator &TypeDeclarator,
                                        Declarator &TraitDeclarator,
                                        TraitDecl *TD) {
  TypeSourceInfo *TypeInfo = GetTypeForDeclarator(TypeDeclarator, S);
  QualType ImplQT = TypeInfo->getType();
  TypeSourceInfo *TraitInfo = GetTypeForDeclarator(TraitDeclarator, S);
  QualType TraitQT =
      TraitInfo->getType()->getLocallyUnqualifiedSingleStepDesugaredType();
  std::string Name =
      GetNameForDeclarator(TraitDeclarator).getName().getAsString();
  if (auto TST = dyn_cast<TemplateSpecializationType>(TraitQT)) {
    for (auto it = TST->begin(); it != TST->end(); ++it) {
      if (!it->getAsType().isNull())
        Name += "_" + TypeAsString(it->getAsType());
    }
  }

  ImplTraitDecl *ITD = ImplTraitDecl::Create(
      Context, CurContext, TraitDeclarator.getBeginLoc(),
      TypeDeclarator.getBeginLoc(), &Context.Idents.get(Name), ImplQT, TypeInfo,
      StorageClass::SC_None);
  ITD->setTraitDecl(TD);
  CurContext->addDecl(ITD);
  return ITD;
}

void Sema::ActOnFinishTraitMemberSpecification(Decl *TagDecl) {
  if (!TagDecl)
    return;
  AdjustDeclIfTemplate(TagDecl);
}

ExprResult Sema::AddAfterStructTrait(ExprResult ULE, SourceLocation DSLoc,
                                     std::string ID) {
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
  TraitDecl *TD = TryDesugarTrait(ProtoArgType);
  QualType QT = CompleteTraitType(TD, ProtoArgType);
  if (!PT || !TD) {
    Diag(DSLoc, diag::err_type_has_not_impl_trait) << QT << T;
    return nullptr;
  }
  // For "impl trait TR for struct S",
  // this might be a ElaboratedType for "struct S"
  T = PT->getPointeeType().getCanonicalType();
  VarDecl *LookupVar = TD->getTypeImpledVarDecl(T);
  if (!LookupVar) {
    Diag(DSLoc, diag::err_type_has_not_impl_trait) << QT << T;
    return nullptr;
  }
  LookupVar->setIsUsed();

  QualType VoidPT = Context.getPointerType(Context.VoidTy);
  QualType VtablePT = QualType();
  RecordDecl *RD = ProtoArgType->getAsRecordDecl();
  for (auto Field : RD->fields()) {
    if (Field->getNameAsString() == "vtable")
      VtablePT = Field->getType();
  }
  if (VtablePT.isNull())
    return ExprError().get();
  QualType VtableTy = VtablePT->getPointeeType();

  ExprResult TraitData =
      ImplicitCastExpr::Create(Context, VoidPT, CK_BitCast, UO, nullptr,
                               VK_PRValue, FPOptionsOverride());
  Designation Desig;
  Desig.AddDesignator(
      Designator::getField(&Context.Idents.get("data"), DSLoc, DSLoc));
  TraitData = ActOnDesignatedInitializer(Desig, DSLoc, false, TraitData);
  DeclRefExpr *VtableRef =
      DeclRefExpr::Create(Context, NestedNameSpecifierLoc(), SourceLocation(),
                          LookupVar, false, DSLoc, VtableTy, VK_LValue);
  ExprResult UOVtable =
      UnaryOperator::Create(Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue,
                            OK_Ordinary, DSLoc, false, FPOptionsOverride());
  Designation Desig1;
  Desig1.AddDesignator(
      Designator::getField(&Context.Idents.get("vtable"), DSLoc, DSLoc));
  UOVtable = ActOnDesignatedInitializer(Desig1, DSLoc, false, UOVtable);
  std::vector<Expr *> Exprs = {TraitData.get(), UOVtable.get()};
  MutableArrayRef<Expr *> initExprs = MutableArrayRef<Expr *>(Exprs);
  ExprResult Result = ActOnInitList(DSLoc, initExprs, DSLoc);
  dyn_cast<InitListExpr>(Result.getAs<Expr>())->setHasDesugar(true);
  TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(ProtoArgType);
  TypeResult TR = CreateParsedType(ProtoArgType, TInfo);
  Result = ActOnCompoundLiteral(DSLoc, TR.get(), DSLoc, Result.get());
  return Result.get();
}

static bool IsImplTraitDeclIllegal(Sema &S, QualType TraitQT, QualType &ImplQT,
                                   SourceLocation TypeLoc, TraitDecl *TD,
                                   QualType OriginTraitTy) {
  CXXScopeSpec SS;
  NamedDecl *Def = nullptr;
  Sema::BoundTypeDiagnoser<> Diagnoser(
      diag::err_typecheck_decl_incomplete_type);

  if (ImplQT->isIncompleteType(&Def))
    Diagnoser.diagnose(S, TypeLoc, ImplQT);

  IdentifierInfo *FunctionID = nullptr;
  RecordDecl *VT = TD->getVtable();
  const TemplateSpecializationType *TST =
      TraitQT->getAs<TemplateSpecializationType>();
  if (TST) {
    ClassTemplateDecl *CTD = VT->getDescribedClassTemplate();
    void *InsertPos = nullptr;
    TemplateArgumentListInfo Args(TypeLoc, TypeLoc);
    for (auto T : TST->template_arguments())
      Args.addArgument(TemplateArgumentLoc(
          T, S.Context.getTrivialTypeSourceInfo(T.getAsType(), TypeLoc)));
    SmallVector<TemplateArgument, 4> Converted;
    S.CheckTemplateArgumentList(CTD, CTD->getBeginLoc(), Args, false, Converted,
                                /*UpdateArgsWithConversions=*/true);
    if (ClassTemplateSpecializationDecl *CTSD =
            CTD->findSpecialization(Converted, InsertPos))
      VT = CTSD;
  }

  for (RecordDecl::field_iterator FieldIt = VT->field_begin();
       FieldIt != VT->field_end(); ++FieldIt) {
    FunctionID = FieldIt->getIdentifier();
    QualType TraitQT = FieldIt->getType()->getPointeeType();
    const FunctionProtoType *TraitTy = TraitQT->getAs<FunctionProtoType>();
    BSCMethodDecl *MD = nullptr;
    DeclContext *DC = // The Type's member functions
        S.getASTContext()
            .BSCDeclContextMap[ImplQT.getCanonicalType().getTypePtr()];
    if (DC) {
      DeclContext::lookup_result DR = DC->lookup(FunctionID);
      for (NamedDecl *D : DR)
        if (D)
          MD = dyn_cast<BSCMethodDecl>(D);
    }
    // Check whether function prototypes match in traitBody and type's member
    // funcs
    QualType MethodQT = MD->getType();
    const FunctionProtoType *MethodTy = MethodQT->getAs<FunctionProtoType>();
    bool TypeDiagFlag = false;
    if (!MD->getHasThisParam() ||
        !S.Context.hasSameFunctionTypeIgnoringPtrSizes(
            TraitTy->getReturnType(), MethodTy->getReturnType()))
      TypeDiagFlag = true;
    else {
      int n = TraitTy->getNumParams();
      int m = MethodTy->getNumParams();
      if (n == m)
        for (int i = 1; i < n; ++i) {
          QualType T1 = TraitTy->getParamType(i);
          QualType T2 = MethodTy->getParamType(i);
          if (!S.Context.hasSameFunctionTypeIgnoringPtrSizes(T1, T2))
            TypeDiagFlag = true;
        }
      else
        TypeDiagFlag = true;
    }
    if (TypeDiagFlag) {
      S.Diag(TypeLoc, diag::err_trait_impl_function_type_conflict)
          << FieldIt->getIdentifier() << OriginTraitTy;
      return true;
    }
  }
  return false;
}

QualType Sema::CompleteTraitType(TraitDecl *TD, QualType QT) {
  QualType TraitQT = Context.getTraitType(TD);
  if (TraitTemplateDecl *TTD = TD->getDescribedTraitTemplate()) {
    TemplateArgumentListInfo Args(TTD->getBeginLoc(), TTD->getEndLoc());
    const TemplateSpecializationType *TST =
        dyn_cast<TemplateSpecializationType>(
            QT->getLocallyUnqualifiedSingleStepDesugaredType());
    for (auto T : TST->template_arguments())
      Args.addArgument(TemplateArgumentLoc(
          T,
          Context.getTrivialTypeSourceInfo(T.getAsType(), TTD->getBeginLoc())));
    TraitQT = CheckTemplateIdType(TemplateName(TTD), TTD->getBeginLoc(), Args);
    TraitQT = Context.getElaboratedType(ETK_Trait, nullptr, TraitQT);
  }
  return TraitQT;
}

QualType Sema::CompleteRecordType(RecordDecl *RD, TypeSourceInfo *TInfo) {
  ClassTemplateDecl *CTD = RD->getDescribedClassTemplate();
  SourceLocation BL = TInfo->getTypeLoc().getBeginLoc();
  SourceLocation EL = TInfo->getTypeLoc().getEndLoc();
  TemplateArgumentListInfo Args(BL, EL);
  QualType QT = TInfo->getType();
  if (QT->isPointerType())
    QT = QT->getPointeeType();
  // Remove ElaboratedType
  const TemplateSpecializationType *TST = dyn_cast<TemplateSpecializationType>(
      QT->getLocallyUnqualifiedSingleStepDesugaredType());
  if (!TST)
    return QualType();
  for (auto T : TST->template_arguments())
    Args.addArgument(TemplateArgumentLoc(
        T, Context.getTrivialTypeSourceInfo(T.getAsType(), BL)));
  QualType TraitQT = CheckTemplateIdType(TemplateName(CTD), BL, Args);
  if (TraitQT.isNull())
    return QualType();
  if (RequireCompleteType(BL, TraitQT,
                          diag::err_typecheck_decl_incomplete_type))
    return QualType();
  return TraitQT;
}

VarDecl *Sema::DesugarImplTrait(ImplTraitDecl *ITD, Declarator &TypeDeclarator,
                                Declarator &TraitDeclarator,
                                SourceLocation TypeLoc) {
  TraitDecl *TD = ITD->getTraitDecl();
  SourceLocation TraitLoc = ITD->getLocation();
  CXXScopeSpec SS;
  Scope *S = getCurScope();
  DeclContext *DC = CurContext;

  TypeSourceInfo *TraitInfo = GetTypeForDeclarator(TraitDeclarator, S);
  QualType OriginTraitTy = TraitInfo->getType();
  RecordDecl *TraitVT = TD->getVtable();
  QualType TraitQT = TraitVT->getTypeForDecl()->getCanonicalTypeInternal();
  if (TraitVT->getDescribedClassTemplate())
    TraitQT = CompleteRecordType(TraitVT, TraitInfo);
  if (TraitQT.isNull())
    return nullptr;
  TraitQT = Context.getElaboratedType(ETK_Struct, nullptr, TraitQT);
  TypeSourceInfo *TypeInfo = GetTypeForDeclarator(TypeDeclarator, S);
  QualType ImplQT = TypeInfo->getType().getCanonicalType();
  VarDecl *LookupVar = TD->getTypeImpledVarDecl(ImplQT);
  // If we have the same ImplTraitDecl before, return nullptr
  if (LookupVar)
    return nullptr;

  std::string ImplTraitName = "__" + TypeAsString(ImplQT) + "_trait_" +
                              TraitDeclarator.getIdentifier()->getName().str();

  IdentifierInfo *ITII = &Context.Idents.get(ImplTraitName);
  StorageClass SC = clang::SC_None;
  VarDecl *NewVD =
      VarDecl::Create(Context, DC, ITD->getBeginLoc(), TraitLoc, ITII, TraitQT,
                      Context.getTrivialTypeSourceInfo(TraitQT), SC);
  NewVD->setLexicalDeclContext(CurContext);

  SmallVector<Expr *, 12> InitExprs;
  for (RecordDecl::field_iterator FieldIt = TraitVT->field_begin();
       FieldIt != TraitVT->field_end(); ++FieldIt) {
    IdentifierInfo *II = FieldIt->getIdentifier();
    DeclContext *LookupDC =
        Context.BSCDeclContextMap[ImplQT.getCanonicalType().getTypePtr()];
    DeclContext::lookup_result Decls;
    if (LookupDC)
      Decls = LookupDC->lookup(DeclarationName(II));
    BSCMethodDecl *MD = nullptr;
    if (Decls.isSingleResult()) {
      for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                                E = Decls.end();
           I != E; ++I) {
        if (isa<BSCMethodDecl>(*I)) {
          MD = dyn_cast<BSCMethodDecl>(*I);
          break;
        }
      }
    }

    if (MD) {
      QualType FT = MD->getType();
      assert(FT->isFunctionProtoType());
      const FunctionProtoType *FPT = FT->getAs<FunctionProtoType>();
      SmallVector<QualType, 16> ParamTys;
      QualType VoidPT = Context.getPointerType(Context.VoidTy);
      ParamTys.push_back(VoidPT);
      for (unsigned int i = 1; i < FPT->getNumParams(); i++) {
        ParamTys.push_back(FPT->getParamType(i));
      }
      QualType FuncTy = Context.getFunctionType(FPT->getReturnType(), ParamTys,
                                                FPT->getExtProtoInfo());
      QualType ParenTy = Context.getParenType(FuncTy);
      QualType PointerTy = Context.getPointerType(ParenTy);
      TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(PointerTy);
      ExprResult Res = BuildDeclRefExpr(MD, MD->getType(), VK_LValue, TraitLoc);
      Res.get()->HasBSCScopeSpec = true;
      Res = ImplicitCastExpr::Create(
          Context, Context.getPointerType(Res.get()->getType()),
          CK_FunctionToPointerDecay, Res.get(), nullptr, VK_PRValue,
          FPOptionsOverride());
      Res = BuildCStyleCastExpr(ITD->getLocation(), TInfo, ITD->getLocation(),
                                Res.get())
                .get();
      Designation Desig;
      Desig.AddDesignator(Designator::getField(II, TraitLoc, TraitLoc));
      Res = ActOnDesignatedInitializer(Desig, TraitLoc, false, Res);
      InitExprs.push_back(Res.get());
    } else {
      Diag(TypeLoc, diag::err_trait_impl)
          << II << TraitInfo->getType() << ImplQT;
      return nullptr;
    }
  }

  ExprResult ER = BuildInitList(ITD->getBeginLoc(), InitExprs, TraitLoc);
  InitListExpr *ILE = dyn_cast<InitListExpr>(ER.get());
  AddInitializerToDecl(NewVD, ILE, false);
  PushOnScopeChains(NewVD, S, true);

  if (IsImplTraitDeclIllegal(*this, TraitQT, ImplQT, TypeLoc, TD,
                             OriginTraitTy))
    return nullptr;

  TD->MapInsert(ImplQT, NewVD);
  return NewVD;
}

QualType Sema::DesugarTraitToStructTrait(TraitDecl *TD, QualType T,
                                         SourceLocation Loc) {
  RecordDecl *RD = TD->getTrait();
  assert(RD && "The TraitDecl did not generate a corresponding TraitRecord");
  QualType RT = QualType();
  if (dyn_cast<TemplateSpecializationType>(
          T->getPointeeType()
              ->getLocallyUnqualifiedSingleStepDesugaredType())) {
    TypeSourceInfo *TInfo = Context.getTrivialTypeSourceInfo(T, Loc);
    RT = CompleteRecordType(RD, TInfo);
  } else {
    RT = RD->getTypeForDecl()->getCanonicalTypeInternal();
  }
  return Context.getElaboratedType(ETK_Struct, nullptr, RT);
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
VarDecl *Sema::ActOnDesugarTraitInstance(Decl *D) {
  VarDecl *VD = dyn_cast<VarDecl>(D);
  if (!VD)
    return nullptr;
  QualType QT = VD->getType();
  QualType OriginQT = VD->getType();
  TraitDecl *TD = nullptr;
  if (QT->isPointerType()) {
    TD = TryDesugarTrait(QT);
    if (TD) {
      RecordDecl *LookupTrait = TD->getTrait();
      if (!LookupTrait)
        return nullptr;
      if (LookupTrait->getDescribedClassTemplate()) {
        QT = CompleteRecordType(LookupTrait, VD->getTypeSourceInfo());
      } else {
        QT = Context.getRecordType(LookupTrait);
      }
    }
    OriginQT = OriginQT->getPointeeType();
  }
  QT = Context.getElaboratedType(ETK_Struct, nullptr, QT);

  if (TD == nullptr || QT.isNull())
    return nullptr;
  // build expr: .data = &s;
  if (TD->getVtable() == nullptr) {
    Diag(VD->getLocation(), diag::err_typecheck_decl_incomplete_type) << QT;
    return nullptr;
  }

  VarDecl *NewVD = VarDecl::Create(Context, CurContext, D->getBeginLoc(),
                                   D->getLocation(), VD->getIdentifier(), QT,
                                   Context.getTrivialTypeSourceInfo(QT),
                                   StorageClass::SC_None);

  PushOnScopeChains(NewVD, getCurScope(), true);
  Expr *exp = VD->getInit();
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
    Diag(UO->getBeginLoc(), diag::err_type_has_not_impl_trait) << OriginQT << T;
    return nullptr;
  }
  T = PT->getPointeeType().getCanonicalType();
  VarDecl *LookupVar = TD->getTypeImpledVarDecl(T);
  if (!LookupVar) {
    Diag(UO->getBeginLoc(), diag::err_type_has_not_impl_trait) << OriginQT << T;
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

  QualType VtableTy = LookupVar->getType();
  QualType VtablePT = Context.getPointerType(VtableTy);
  DeclRefExpr *VtableRef = DeclRefExpr::Create(
      Context, NestedNameSpecifierLoc(), SourceLocation(), LookupVar, false,
      SourceLocation(), VtableTy, VK_LValue);
  UnaryOperator *UOVtable = UnaryOperator::Create(
      Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue, OK_Ordinary,
      SourceLocation(), false, FPOptionsOverride());

  std::vector<Expr *> Exprs = {TraitData, UOVtable};
  MutableArrayRef<Expr *> initExprs = MutableArrayRef<Expr *>(Exprs);
  ExprResult TraitInit =
      ActOnInitList(SourceLocation(), initExprs, SourceLocation());
  AddInitializerToDecl(NewVD, TraitInit.get(), false);
  return NewVD;
}

NamedDecl *Sema::ActOnTraitMemberDeclarator(Scope *S, Declarator &D) {
  D.setIsTraitMember(true);
  MultiTemplateParamsArg TemplateParams;
  NamedDecl *Member = HandleDeclarator(S, D, TemplateParams);
  return Member;
}

TraitDecl *Sema::TryDesugarTrait(QualType T) {
  TraitDecl *TD = nullptr;
  if (T.isNull())
    return TD;
  if (T->isPointerType()) {
    // Remove ElaboratedType
    const TemplateSpecializationType *TST =
        dyn_cast<TemplateSpecializationType>(
            T->getPointeeType()
                ->getLocallyUnqualifiedSingleStepDesugaredType());
    if (TST) {
      TemplateDecl *TempT = TST->getTemplateName().getAsTemplateDecl();
      TD = dyn_cast_or_null<TraitDecl>(TempT->getTemplatedDecl());
    } else {
      TD = dyn_cast_or_null<TraitDecl>(T->getPointeeType()->getAsTagDecl());
    }
  } else if (RecordDecl *RD = T->getAsRecordDecl()) {
    if (auto *TST = dyn_cast_or_null<TemplateSpecializationType>(T)) {
      TemplateDecl *TempT = TST->getTemplateName().getAsTemplateDecl();
      RD = dyn_cast_or_null<RecordDecl>(TempT->getTemplatedDecl());
    }
    TD = RD->getDesugaredTraitDecl();
  }
  return TD;
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
      RecordDecl *LookupVtable = TD->getVtable();
      VarDecl *LookupVar = TD->getTypeImpledVarDecl(T);
      QualType QT = Context.getTraitType(TD);
      if (TraitTemplateDecl *TTD = TD->getDescribedTraitTemplate()) {
        TemplateArgumentListInfo Args(TTD->getBeginLoc(), TTD->getEndLoc());
        ClassTemplateSpecializationDecl *CTD =
            dyn_cast<ClassTemplateSpecializationDecl>(RD);
        ArrayRef<TemplateArgument> TAL = CTD->getTemplateArgs().asArray();
        for (auto T : TAL)
          Args.addArgument(
              TemplateArgumentLoc(T, Context.getTrivialTypeSourceInfo(
                                         T.getAsType(), TTD->getBeginLoc())));
        QT = CheckTemplateIdType(TemplateName(TTD), TTD->getBeginLoc(), Args);
        QT = Context.getElaboratedType(ETK_Trait, nullptr, QT);
      }
      if (!LookupVar) {
        Diag(RHSExpr->getBeginLoc(), diag::err_type_has_not_impl_trait)
            << QT << T;
        return ExprError();
      }
      QualType VtableTy = Context.getRecordType(LookupVtable);
      if (LookupVtable->getDescribedClassTemplate())
        VtableTy =
            CompleteRecordType(LookupVtable, LookupVar->getTypeSourceInfo());
      VtableTy = Context.getElaboratedType(ETK_Struct, nullptr, VtableTy);
      QualType VtablePT = Context.getPointerType(VtableTy);
      DeclRefExpr *VtableRef = BuildDeclRefExpr(LookupVar, VtableTy, VK_LValue,
                                                LookupVar->getLocation());
      NewRHSExpr = UnaryOperator::Create(
          Context, VtableRef, UO_AddrOf, VtablePT, VK_PRValue, OK_Ordinary,
          SourceLocation(), false, FPOptionsOverride());
    }
    if (Bin1 == nullptr)
      Bin1 = BuildBinOp(S, TokLoc, Opc, NewLHSExpr, NewRHSExpr)
                 .get(); // f.data = &a;
    else
      Bin2 = BuildBinOp(S, TokLoc, Opc, NewLHSExpr, NewRHSExpr)
                 .get(); // f.vtable = &__int_trait_T;
  }
  return BuildBinOp(S, TokLoc, BO_Comma, Bin1,
                    Bin2); // f.data = &a, f.vtable = &__int_trait_T;
}

#endif // ENABLE_BSC