//===--- SemaBSCCoroutine.cpp - Semantic Analysis for BSC Coroutines
//----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for BSC Coroutines.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"
#include "TypeLocBuilder.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/NonTrivialTypeVisitor.h"
#include "clang/AST/Randstruct.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"
#include "llvm/ADT/SmallString.h"
#include <algorithm>
#include <cstring>
#include <functional>

using namespace clang;
using namespace sema;


namespace {
class AwaitExprFinder : public StmtVisitor<AwaitExprFinder> {
  int AwaitCount = 0;
  std::vector<Expr *> Args;
  std::vector<std::pair<DeclarationName, QualType>> LocalVarList;

public:
  void VisitAwaitExpr(AwaitExpr *E) {
    Visit(E->getSubExpr());
    Args.push_back(E);
    AwaitCount++;
  }

  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C) {
        if (auto DeclS = dyn_cast_or_null<DeclStmt>(C)) {
          for (const auto &DI : DeclS->decls()) {
            if (auto VD = dyn_cast_or_null<VarDecl>(DI)) {
              QualType QT = VD->getType();
              if (QT.isConstQualified()) {
                QT.removeLocalConst();
                VD->setType(QT);
              }
              LocalVarList.push_back(std::make_pair<DeclarationName, QualType>(
                  VD->getDeclName(), VD->getType()));
            }
          }
        }
        Visit(C);
      }
    }
  }

  int GetAwaitExprNum() const { return AwaitCount; }

  std::vector<Expr *> GetAwaitExpr() const { return Args; }

  std::vector<std::pair<DeclarationName, QualType>> GetLocalVarList() const {
    return LocalVarList;
  }
};

bool HasAwaitExpr(Stmt *S) {
  if (S == nullptr)
    return false;

  if (isa<AwaitExpr>(S))
    return true;

  for (auto *C : S->children()) {
    if (HasAwaitExpr(C)) {
      return true;
    }
  }

  return false;
}


std::string GetPrefix(QualType T) {
  std::string ExtendedTypeStr = T.getAsString();
  for (int i = ExtendedTypeStr.length() - 1; i >= 0; i--) {
    if (ExtendedTypeStr[i] == ' ') {
      ExtendedTypeStr.replace(i, 1, "_");
    }
  }
  return ExtendedTypeStr;
}

bool IsFutureType(QualType T) {
  std::string prefix = "struct __FatPointer_";
  std::string string = T.getAsString();
  bool isPrefix =
      prefix.size() <= string.size() &&
      std::mismatch(prefix.begin(), prefix.end(), string.begin(), string.end())
              .first == prefix.end();
  return isPrefix;
}

static QualType lookupPollResultType(Sema &S, SourceLocation SLoc, QualType T) {
  std::string PollResultName = "PollResult";

  DeclContext::lookup_result Decls = S.Context.getTranslationUnitDecl()->lookup(
      DeclarationName(&(S.Context.Idents).get(PollResultName)));
  ClassTemplateDecl *CTD = nullptr;
  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<ClassTemplateDecl>(*I)) {
        CTD = dyn_cast<ClassTemplateDecl>(*I);
        break;
      }
    }
  } else {
    S.Diag(SLoc, diag::err_function_not_found)
        << PollResultName << "\"future.hbs\"";
    return QualType();
  }

  TemplateArgumentListInfo Args(SLoc, SLoc);
  Args.addArgument(TemplateArgumentLoc(
      TemplateArgument(T), S.Context.getTrivialTypeSourceInfo(T, SLoc)));
  QualType PollResultRecord =
      S.CheckTemplateIdType(TemplateName(CTD), SourceLocation(), Args);

  if (PollResultRecord.isNull())
    return QualType();
  if (S.RequireCompleteType(SLoc, PollResultRecord,
                            diag::err_coroutine_type_missing_specialization))
    return QualType();
  return PollResultRecord;
}
} // namespace

// build struct Future for async function
static RecordDecl *buildFutureRecordDecl(
    Sema &S, FunctionDecl *FD, ArrayRef<Expr *> Args,
    std::vector<std::pair<DeclarationName, QualType>> LocalVarList) {
  std::vector<std::pair<DeclarationName, QualType>> paramList;
  DeclarationName funcName = FD->getDeclName();
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation ELoc = FD->getEndLoc();
  for (FunctionDecl::param_const_iterator pi = FD->param_begin();
       pi != FD->param_end(); pi++) {
    paramList.push_back(std::make_pair<DeclarationName, QualType>(
        (*pi)->getDeclName(), (*pi)->getType()));
  }

  for (unsigned I = 0; I != Args.size(); ++I) {
    auto *AE = cast<AwaitExpr>(Args[I])->getSubExpr();
    if (!IsFutureType(AE->getType())) {
      RecordDecl *FatPointerStruct;
      const std::string FatPointerName =
          "__FatPointer_" + GetPrefix(AE->getType());
      DeclContext::lookup_result FatPointerDecls =
          S.Context.getTranslationUnitDecl()->lookup(
              DeclarationName(&(S.Context.Idents).get(FatPointerName)));

      if (FatPointerDecls.isSingleResult()) {
        for (DeclContext::lookup_result::iterator I = FatPointerDecls.begin(),
                                                  E = FatPointerDecls.end();
             I != E; ++I) {
          if (isa<RecordDecl>(*I)) {
            FatPointerStruct = dyn_cast<RecordDecl>(*I);
            break;
          }
        }
      } else {
        abort();
      }
      LocalVarList.push_back(std::make_pair<DeclarationName, QualType>(
          &(S.Context.Idents).get("Ft_" + std::to_string(I + 1)),
          S.Context.getRecordType(FatPointerStruct)));
    } else {
      LocalVarList.push_back(std::make_pair<DeclarationName, QualType>(
          &(S.Context.Idents).get("Ft_" + std::to_string(I + 1)),
          AE->getType()));
    }
  }

  const std::string Recordname = "_Future" + funcName.getAsString();
  RecordDecl *RD = buildAsyncDataRecord(S.Context, Recordname, SLoc, ELoc,
                                        clang::TagDecl::TagKind::TTK_Struct);
  RD->startDefinition();
  for (std::vector<std::pair<DeclarationName, QualType>>::iterator it =
           paramList.begin();
       it != paramList.end(); it++) {
    std::string VarName = it->first.getAsString();
    QualType VarType = it->second;
    addAsyncRecordDecl(S.Context, VarName, VarType, SLoc, ELoc, RD);
  }
  for (std::vector<std::pair<DeclarationName, QualType>>::iterator it =
           LocalVarList.begin();
       it != LocalVarList.end(); it++) {
    const std::string VarName = it->first.getAsString();
    QualType VarType = it->second;
    addAsyncRecordDecl(S.Context, VarName, VarType, SLoc, ELoc, RD);
  }

  const std::string FutureStateName = "__future_state";
  addAsyncRecordDecl(S.Context, FutureStateName, S.Context.IntTy, SLoc, ELoc,
                     RD);
  RD->completeDefinition();
  S.PushOnScopeChains(RD, S.getCurScope(), true);
  return RD;
}


/// TODO: will be removed after using Future in stdlib
static std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
generateVtableAndFatPointerStruct(Sema &S, QualType T,
                                  RecordDecl *PollResultRD) {
  RecordDecl *VtableStruct = nullptr;
  QualType ReturnTy = T;
  std::string VtableStructName = "__Vtable_" + GetPrefix(ReturnTy);
  bool IsVtableExisted = false;
  DeclContext::lookup_result Decls = S.Context.getTranslationUnitDecl()->lookup(
      DeclarationName(&(S.Context.Idents).get(VtableStructName)));

  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<RecordDecl>(*I)) {
        VtableStruct = dyn_cast<RecordDecl>(*I);
        break;
      }
    }
    IsVtableExisted = true;
    VtableStruct->addAttr(WeakAttr::CreateImplicit(S.Context));
  } else if (Decls.empty()) {
    VtableStruct = buildAsyncDataRecord(S.Context, VtableStructName,
                                        SourceLocation(), SourceLocation(),
                                        clang::TagDecl::TagKind::TTK_Struct);
    VtableStruct->startDefinition();
    SmallVector<QualType, 1> Args;
    Args.push_back(S.Context.VoidPtrTy);
    QualType PollFuncType = S.Context.getPointerType(S.Context.getFunctionType(
        S.Context.getRecordType(PollResultRD), Args, {}));
    QualType FreeFuncType = S.Context.getPointerType(
        S.Context.getFunctionType(S.Context.VoidTy, Args, {}));
    addAsyncRecordDecl(S.Context, "poll", PollFuncType, SourceLocation(),
                       SourceLocation(), VtableStruct);
    addAsyncRecordDecl(S.Context, "free", FreeFuncType, SourceLocation(),
                       SourceLocation(), VtableStruct);
    VtableStruct->completeDefinition();
    VtableStruct->addAttr(WeakAttr::CreateImplicit(S.Context));
    S.PushOnScopeChains(VtableStruct, S.getCurScope(), true);
  } else {
    // should not reach here. todo: change to assert
    abort();
  }
  RecordDecl *FatPointerStruct = nullptr;
  std::string FatPointerName = "__FatPointer_" + GetPrefix(ReturnTy);
  bool IsFatPointerExisted = false;
  DeclContext::lookup_result FatPointerDecls =
      S.Context.getTranslationUnitDecl()->lookup(
          DeclarationName(&(S.Context.Idents).get(FatPointerName)));

  if (FatPointerDecls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = FatPointerDecls.begin(),
                                              E = FatPointerDecls.end();
         I != E; ++I) {
      if (isa<RecordDecl>(*I)) {
        FatPointerStruct = dyn_cast<RecordDecl>(*I);
        break;
      }
    }
    IsFatPointerExisted = true;
    FatPointerStruct->addAttr(WeakAttr::CreateImplicit(S.Context));
  } else if (FatPointerDecls.empty()) {
    FatPointerStruct = buildAsyncDataRecord(
        S.Context, FatPointerName, SourceLocation(), SourceLocation(),
        clang::TagDecl::TagKind::TTK_Struct);
    FatPointerStruct->startDefinition();
    addAsyncRecordDecl(S.Context, "data", S.Context.VoidPtrTy, SourceLocation(),
                       SourceLocation(), FatPointerStruct);
    addAsyncRecordDecl(
        S.Context, "vtable",
        S.Context.getPointerType(S.Context.getRecordType(VtableStruct)),
        SourceLocation(), SourceLocation(), FatPointerStruct);
    FatPointerStruct->completeDefinition();
    FatPointerStruct->addAttr(WeakAttr::CreateImplicit(S.Context));
    S.PushOnScopeChains(FatPointerStruct, S.getCurScope(), true);
  } else {
    // should not reach here. todo: change to assert
    abort();
  }
  return std::make_tuple(std::make_pair(VtableStruct, IsVtableExisted),
                         std::make_pair(FatPointerStruct, IsFatPointerExisted));
}

static VarDecl *buildVtableInitDecl(Sema &S, FunctionDecl *FD,
                                    RecordDecl *VtableRD,
                                    RecordDecl *PollResultRD,
                                    BSCMethodDecl *PollFunc,
                                    BSCMethodDecl *FreeFunc) {
  std::string IName =
      VtableRD->getDeclName().getAsString() + FD->getDeclName().getAsString();
  VarDecl *VD = VarDecl::Create(
      S.Context, S.Context.getTranslationUnitDecl(), SourceLocation(),
      SourceLocation(), &(S.Context.Idents).get(IName),
      S.Context.getRecordType(VtableRD), nullptr, SC_None);
  DeclGroupRef DG(VD);
  S.PushOnScopeChains(VD, S.getCurScope(), false);
  SmallVector<Expr *, 2> InitExprs;

  SmallVector<QualType, 1> Args;
  Args.push_back(S.Context.VoidPtrTy);

  QualType PollFuncType = S.Context.getPointerType(S.Context.getFunctionType(
      S.Context.getRecordType(PollResultRD), Args, {}));
  Expr *PollInit = S.BuildDeclRefExpr(PollFunc, PollFunc->getType(), VK_LValue,
                                      SourceLocation());
  PollInit = S.ImpCastExprToType(PollInit,
                                 S.Context.getPointerType(PollInit->getType()),
                                 CK_FunctionToPointerDecay)
                 .get();
  PollInit = S.ImpCastExprToType(PollInit, PollFuncType, CK_BitCast).get();
  InitExprs.push_back(PollInit);

  QualType FreeFuncType = S.Context.getPointerType(
      S.Context.getFunctionType(S.Context.VoidTy, Args, {}));
  Expr *FreeInit = S.BuildDeclRefExpr(FreeFunc, FreeFunc->getType(), VK_LValue,
                                      SourceLocation());
  FreeInit = S.ImpCastExprToType(FreeInit,
                                 S.Context.getPointerType(FreeInit->getType()),
                                 CK_FunctionToPointerDecay)
                 .get();
  FreeInit = S.ImpCastExprToType(FreeInit, FreeFuncType, CK_BitCast).get();
  InitExprs.push_back(FreeInit);

  Expr *ILE =
      S.BuildInitList(SourceLocation(), InitExprs, SourceLocation()).get();
  ILE->setType(S.Context.getRecordType(VtableRD));
  VD->setInit(ILE);
  return VD;
}

FunctionDecl *buildFutureInitFunctionDeclaraion(Sema &S, RecordDecl *RD,
                                                FunctionDecl *FD,
                                                RecordDecl *FatPointerRD,
                                                VarDecl *VtableInit) {
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation NLoc = FD->getNameInfo().getLoc();
  DeclarationName funcName = FD->getDeclName();
  QualType FuncRetType = S.Context.getRecordType(FatPointerRD);
  SmallVector<QualType, 16> ParamTys;
  FunctionDecl::param_const_iterator pi;
  for (pi = FD->param_begin(); pi != FD->param_end(); pi++) {
    ParamTys.push_back((*pi)->getType());
  }
  QualType FuncType = S.Context.getFunctionType(FuncRetType, ParamTys, {});
  TypeSourceInfo *Tinfo = FD->getTypeSourceInfo();
  std::string FName = "__" + funcName.getAsString();

  DeclContext::lookup_result Decls = FD->getDeclContext()->lookup(
      DeclarationName(&(S.Context.Idents).get(FName)));
  FunctionDecl *NewFD = nullptr;
  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<FunctionDecl>(*I)) {
        NewFD = dyn_cast<FunctionDecl>(*I);
        break;
      }
    }
  } else if (Decls.empty()) {
    if (dyn_cast<BSCMethodDecl>(FD)) {
      NewFD = buildAsyncBSCMethodDecl(S.Context, FD->getDeclContext(), SLoc,
                                      NLoc, &(S.Context.Idents).get(FName),
                                      FuncType, Tinfo, SC_None);
    } else {
      NewFD =
          buildAsyncFuncDecl(S.Context, FD->getDeclContext(), SLoc, NLoc,
                             &(S.Context.Idents).get(FName), FuncType, Tinfo);
    }
    SmallVector<ParmVarDecl *, 4> ParmVarDecls;
    for (const auto &I : FD->parameters()) {
      ParmVarDecl *PVD = ParmVarDecl::Create(
          S.Context, NewFD, SourceLocation(), SourceLocation(),
          &(S.Context.Idents).get(I->getName()), I->getType(), nullptr, SC_None,
          nullptr);
      ParmVarDecls.push_back(PVD);
    }
    NewFD->setParams(ParmVarDecls);
    NewFD->setLexicalDeclContext(S.Context.getTranslationUnitDecl());
    S.PushOnScopeChains(NewFD, S.getCurScope(), true);
  } else {
    abort();
  }
  S.PushFunctionScope();
  S.PushDeclContext(S.getCurScope(), NewFD);

  std::string IName = "data";
  VarDecl *VD = VarDecl::Create(
      S.Context, NewFD, SLoc, SLoc, &(S.Context.Idents).get(IName),
      S.Context.getPointerType(S.Context.getRecordType(RD)),
      S.Context.getTrivialTypeSourceInfo(
          S.Context.getPointerType(S.Context.getRecordType(RD)), SLoc),
      SC_None);

  DeclGroupRef DataDG(VD);
  DeclStmt *DataDS = new (S.Context) DeclStmt(DataDG, NLoc, NLoc);
  std::vector<Stmt *> Stmts;
  Stmts.push_back(DataDS);

  std::string MallocName = "malloc";

  DeclContext::lookup_result MallocDecls =
      S.Context.getTranslationUnitDecl()->lookup(
          DeclarationName(&(S.Context.Idents).get(MallocName)));
  FunctionDecl *MallocFunc = nullptr;
  if (MallocDecls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = MallocDecls.begin(),
                                              E = MallocDecls.end();
         I != E; ++I) {
      if (isa<FunctionDecl>(*I)) {
        MallocFunc = dyn_cast<FunctionDecl>(*I);
        break;
      }
    }
  } else {
    S.Diag(FD->getBeginLoc(), diag::err_function_not_found)
        << MallocName << "<stdlib.h>";
    return nullptr;
  }

  Expr *MallocRef =
      S.BuildDeclRefExpr(MallocFunc, MallocFunc->getType(), VK_LValue, NLoc);
  MallocRef = S.ImpCastExprToType(
                   MallocRef, S.Context.getPointerType(MallocRef->getType()),
                   CK_FunctionToPointerDecay)
                  .get();
  // sizeof(struct __Futurex)
  Expr *SizeOfExpr =
      S.CreateUnaryExprOrTypeTraitExpr(
           S.Context.getTrivialTypeSourceInfo(S.Context.getRecordType(RD)),
           NLoc, UETT_SizeOf, SourceRange())
          .get();
  SmallVector<Expr *, 1> Args;
  Args.push_back(SizeOfExpr);

  // malloc(sizeof(struct __Futurex))
  Expr *CE = S.BuildCallExpr(nullptr, MallocRef, SourceLocation(), Args,
                             SourceLocation())
                 .get();
  CE = S.ImpCastExprToType(
            CE, S.Context.getPointerType(S.Context.getRecordType(RD)),
            CK_BitCast)
           .get();
  VD->setInit(CE);

  Expr *DataRef = S.BuildDeclRefExpr(VD, VD->getType(), VK_LValue, NLoc);
  DataRef = ImplicitCastExpr::Create(S.Context, DataRef->getType(),
                                     CK_LValueToRValue, DataRef, nullptr,
                                     VK_PRValue, FPOptionsOverride());
  pi = NewFD->param_begin();
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       pi != NewFD->param_end(); ++pi, ++FieldIt) {

    Expr *LHSExpr = S.BuildMemberExpr(
        DataRef, true, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *FieldIt,
        DeclAccessPair::make(*pi, FieldIt->getAccess()), false,
        DeclarationNameInfo(), FieldIt->getType().getNonReferenceType(),
        VK_LValue, OK_Ordinary);

    Expr *RHSExpr =
        S.BuildDeclRefExpr(*pi, FieldIt->getType().getNonReferenceType(),
                           VK_LValue, (*pi)->getLocation());
    RHSExpr = ImplicitCastExpr::Create(S.Context, (*pi)->getType(),
                                       CK_LValueToRValue, RHSExpr, nullptr,
                                       VK_PRValue, FPOptionsOverride());

    Expr *BO =
        S.CreateBuiltinBinOp((*pi)->getLocation(), BO_Assign, LHSExpr, RHSExpr)
            .get();
    Stmts.push_back(BO);
  }

  RecordDecl::field_iterator FutureStateField;
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       FieldIt != RD->field_end(); ++FieldIt) {
    if (FieldIt->getDeclName().getAsString() == "__future_state") {
      FutureStateField = FieldIt;
    }
  }

  Expr *LHSExpr = S.BuildMemberExpr(
      DataRef, true, SourceLocation(), NestedNameSpecifierLoc(),
      SourceLocation(), *FutureStateField,
      DeclAccessPair::make(VD, FutureStateField->getAccess()), false,
      DeclarationNameInfo(), FutureStateField->getType().getNonReferenceType(),
      VK_LValue, OK_Ordinary);

  llvm::APInt ResultVal(S.Context.getTargetInfo().getIntWidth(), 0);
  Expr *RHSExpr = IntegerLiteral::Create(S.Context, ResultVal, S.Context.IntTy,
                                         SourceLocation());

  Expr *BO = S.CreateBuiltinBinOp((*FutureStateField)->getLocation(), BO_Assign,
                                  LHSExpr, RHSExpr)
                 .get();
  Stmts.push_back(BO);

  std::string FatPointerVDName = "fp";
  VarDecl *FatPointerVD = VarDecl::Create(
      S.Context, NewFD, SLoc, SLoc, &(S.Context.Idents).get(FatPointerVDName),
      S.Context.getRecordType(FatPointerRD),
      S.Context.getTrivialTypeSourceInfo(S.Context.getRecordType(FatPointerRD),
                                         SLoc),
      SC_None);

  DeclGroupRef FatPointerDG(FatPointerVD);
  DeclStmt *FatPointerDS = new (S.Context) DeclStmt(FatPointerDG, NLoc, NLoc);
  Stmts.push_back(FatPointerDS);

  SmallVector<Expr *, 2> InitExprs;
  Expr *FutureRefExpr = S.BuildDeclRefExpr(VD, VD->getType(), VK_LValue, NLoc);
  FutureRefExpr = ImplicitCastExpr::Create(
      S.Context, VD->getType(), CK_LValueToRValue, FutureRefExpr, nullptr,
      VK_PRValue, FPOptionsOverride());
  FutureRefExpr =
      S.BuildCStyleCastExpr(
           NLoc, S.Context.getTrivialTypeSourceInfo(S.Context.VoidPtrTy), NLoc,
           FutureRefExpr)
          .get();

  Expr *VtableRefExpr =
      S.BuildDeclRefExpr(VtableInit, VtableInit->getType(), VK_LValue, NLoc);

  Expr *Unop =
      S.CreateBuiltinUnaryOp(SourceLocation(), UO_AddrOf, VtableRefExpr).get();
  InitExprs.push_back(FutureRefExpr);
  InitExprs.push_back(Unop);
  Expr *ILE =
      S.BuildInitList(SourceLocation(), InitExprs, SourceLocation()).get();
  ILE->setType(S.Context.getRecordType(FatPointerRD));
  FatPointerVD->setInit(ILE);

  Expr *FatPointerRef = S.BuildDeclRefExpr(
      FatPointerVD, FatPointerVD->getType(), VK_LValue, NLoc);
  FatPointerRef = ImplicitCastExpr::Create(
      S.Context, FatPointerRef->getType(), CK_LValueToRValue, FatPointerRef,
      nullptr, VK_PRValue, FPOptionsOverride());
  Stmt *RS = S.BuildReturnStmt(NLoc, FatPointerRef).get();
  Stmts.push_back(RS);

  CompoundStmt *CS =
      CompoundStmt::Create(S.Context, Stmts, FPOptionsOverride(), SLoc, NLoc);
  NewFD->setBody(CS);
  S.PopDeclContext();
  sema::AnalysisBasedWarnings::Policy *ActivePolicy = nullptr;
  S.PopFunctionScopeInfo(ActivePolicy, NewFD);
  return NewFD;
}

FunctionDecl *buildFutureInitFunctionDefinition(Sema &S, FunctionDecl *FD,
                                                RecordDecl *FatPointerRD) {
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation NLoc = FD->getNameInfo().getLoc();
  DeclarationName funcName = FD->getDeclName();
  QualType FuncRetType = S.Context.getRecordType(FatPointerRD);
  SmallVector<QualType, 16> ParamTys;
  FunctionDecl::param_const_iterator pi;
  for (pi = FD->param_begin(); pi != FD->param_end(); pi++) {
    ParamTys.push_back((*pi)->getType());
  }
  QualType FuncType = S.Context.getFunctionType(FuncRetType, ParamTys, {});
  TypeSourceInfo *Tinfo = FD->getTypeSourceInfo();
  std::string FName = "__" + funcName.getAsString();

  DeclContext::lookup_result Decls = FD->getDeclContext()->lookup(
      DeclarationName(&(S.Context.Idents).get(FName)));

  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<FunctionDecl>(*I)) {
        return dyn_cast<FunctionDecl>(*I);
        break;
      }
    }
  }

  FunctionDecl *NewFD;
  if (dyn_cast<BSCMethodDecl>(FD)) {
    NewFD = buildAsyncBSCMethodDecl(S.Context, FD->getDeclContext(), SLoc, NLoc,
                                    &(S.Context.Idents).get(FName), FuncType,
                                    Tinfo, SC_None);
  } else {
    NewFD = buildAsyncFuncDecl(S.Context, FD->getDeclContext(), SLoc, NLoc,
                               &(S.Context.Idents).get(FName), FuncType, Tinfo);
  }
  SmallVector<ParmVarDecl *, 4> ParmVarDecls;
  for (const auto &I : FD->parameters()) {
    ParmVarDecl *PVD = ParmVarDecl::Create(
        S.Context, NewFD, SourceLocation(), SourceLocation(),
        &(S.Context.Idents).get(I->getName()), I->getType(), nullptr, SC_None,
        nullptr);
    ParmVarDecls.push_back(PVD);
  }
  NewFD->setParams(ParmVarDecls);
  NewFD->setLexicalDeclContext(S.Context.getTranslationUnitDecl());
  S.PushOnScopeChains(NewFD, S.getCurScope(), true);
  return NewFD;
}

Stmt *ProcessAwaitExprStatus(Sema &S, int AwaitCount, RecordDecl *RD, Expr *ICE,
                             ParmVarDecl *PVD, VarDecl *PollResultVar,
                             VarDecl *AwaitResult, BSCMethodDecl *NewFD) {
  std::vector<Stmt *> BodyStmts;
  std::vector<Stmt *> ElseStmts;
  Expr *IfCond = nullptr;

  RecordDecl::field_iterator TheField;
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       FieldIt != RD->field_end(); ++FieldIt) {
    if (FieldIt->getDeclName().getAsString() == "__future_state") {
      TheField = FieldIt;
      break;
    }
  }

  if (*TheField) {
    DeclarationName Name = TheField->getDeclName();
    DeclarationNameInfo MemberNameInfo(Name, TheField->getLocation());
    Expr *LHSExpr = S.BuildMemberExpr(
        ICE, true, SourceLocation(), NestedNameSpecifierLoc(), SourceLocation(),
        *TheField, DeclAccessPair::make(*TheField, TheField->getAccess()),
        false, MemberNameInfo, TheField->getType().getNonReferenceType(),
        VK_LValue, OK_Ordinary);

    llvm::APInt ResultVal(S.Context.getTargetInfo().getIntWidth(), AwaitCount);
    Expr *RHSExpr = IntegerLiteral::Create(S.Context, ResultVal,
                                           S.Context.IntTy, SourceLocation());

    Expr *BO = S.CreateBuiltinBinOp((*TheField)->getLocation(), BO_Assign,
                                    LHSExpr, RHSExpr)
                   .get();
    ElseStmts.push_back(BO);
  }

  RecordDecl *PollResultRD = PollResultVar->getType()->getAsRecordDecl();

  BSCMethodDecl *IsCompletedFD = nullptr;
  std::string IsCompletedFDName = "is_completed";
  LookupResult IsCompletedFDResult(
      S,
      DeclarationNameInfo(&(S.Context.Idents).get(IsCompletedFDName),
                          SourceLocation()),
      S.LookupOrdinaryName);
  S.LookupQualifiedName(IsCompletedFDResult, PollResultRD);
  if (!IsCompletedFDResult.empty())
    IsCompletedFD =
        dyn_cast_or_null<BSCMethodDecl>(IsCompletedFDResult.getFoundDecl());

  if (IsCompletedFD) {
    Expr *IsCompletedFDRef = S.BuildDeclRefExpr(
        IsCompletedFD, IsCompletedFD->getType(), VK_LValue, SourceLocation());
    IsCompletedFDRef = S.ImpCastExprToType(IsCompletedFDRef,
                                           S.Context.getPointerType(
                                               IsCompletedFDRef->getType()),
                                           CK_FunctionToPointerDecay)
                           .get();

    if (PollResultRD) {
      Expr *PollResultVarRef = S.BuildDeclRefExpr(
          PollResultVar, PollResultVar->getType(), VK_LValue, SourceLocation());
      PollResultVarRef =
          S.CreateBuiltinUnaryOp(SourceLocation(), UO_AddrOf, PollResultVarRef)
              .get();
      Expr *AwaitResultRef = S.BuildDeclRefExpr(
          AwaitResult, AwaitResult->getType(), VK_LValue, SourceLocation());
      AwaitResultRef =
          S.CreateBuiltinUnaryOp(SourceLocation(), UO_AddrOf, AwaitResultRef)
              .get();

      SmallVector<Expr *, 2> Args;
      Args.push_back(PollResultVarRef);
      Args.push_back(AwaitResultRef);
      IfCond = S.BuildCallExpr(nullptr, IsCompletedFDRef, SourceLocation(),
                               Args, SourceLocation())
                   .get();
    }
  }

  RecordDecl *PollResultRDForPending =
      NewFD->getReturnType()->getAsRecordDecl();
  std::string PendingFDName = "pending";
  LookupResult PendingFDResult(
      S,
      DeclarationNameInfo(&(S.Context.Idents).get(PendingFDName),
                          SourceLocation()),
      S.LookupOrdinaryName);
  S.LookupQualifiedName(PendingFDResult, PollResultRDForPending);
  BSCMethodDecl *PendingFD = nullptr;
  if (!PendingFDResult.empty())
    PendingFD = dyn_cast_or_null<BSCMethodDecl>(PendingFDResult.getFoundDecl());

  if (PendingFD) {
    Expr *PendingFDRef = S.BuildDeclRefExpr(PendingFD, PendingFD->getType(),
                                            VK_LValue, SourceLocation());
    PendingFDRef =
        S.ImpCastExprToType(PendingFDRef,
                            S.Context.getPointerType(PendingFDRef->getType()),
                            CK_FunctionToPointerDecay)
            .get();
    Expr *PendingCall = S.BuildCallExpr(nullptr, PendingFDRef, SourceLocation(),
                                        {}, SourceLocation())
                            .get();
    Stmt *RS = S.BuildReturnStmt(SourceLocation(), PendingCall).get();
    ElseStmts.push_back(RS);
  }

  Stmt *Body = CompoundStmt::Create(S.Context, BodyStmts, FPOptionsOverride(),
                                    SourceLocation(), SourceLocation());
  Stmt *Else = CompoundStmt::Create(S.Context, ElseStmts, FPOptionsOverride(),
                                    SourceLocation(), SourceLocation());
  auto *If =
      IfStmt::Create(S.Context, SourceLocation(), IfStatementKind::Ordinary,
                     /* Init=*/nullptr,
                     /* Var=*/nullptr, IfCond,
                     /* LPL=*/SourceLocation(),
                     /* RPL=*/SourceLocation(), Body, SourceLocation(), Else);
  return If;
}



static BSCMethodDecl *buildFreeFunction(Sema &S, RecordDecl *RD,
                                        FunctionDecl *FD) {
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation NLoc = FD->getNameInfo().getLoc();

  std::string FName = "free";
  QualType FuncRetType = S.Context.VoidTy;
  QualType ParamType = S.Context.getPointerType(S.Context.getRecordType(RD));
  SmallVector<QualType, 1> ParamTys;
  ParamTys.push_back(ParamType);

  QualType FuncType = S.Context.getFunctionType(FuncRetType, ParamTys, {});

  BSCMethodDecl *NewFD = buildAsyncBSCMethodDecl(S.Context, RD, SLoc, NLoc,
                                                 &(S.Context.Idents).get(FName),
                                                 FuncType, nullptr, SC_None);
  NewFD->setLexicalDeclContext(S.Context.getTranslationUnitDecl());
  S.Context.BSCDeclContextMap[RD->getTypeForDecl()] = RD;

  SmallVector<ParmVarDecl *, 1> ParmVarDecls;
  ParmVarDecl *PVD = ParmVarDecl::Create(
      S.Context, NewFD, SourceLocation(), SourceLocation(),
      &(S.Context.Idents).get("this"), ParamType, nullptr, SC_None, nullptr);
  ParmVarDecls.push_back(PVD);
  NewFD->setParams(ParmVarDecls);
  S.PushFunctionScope();

  std::vector<Stmt *> Stmts;

  std::stack<RecordDecl::field_iterator> Futures;
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       FieldIt != RD->field_end(); ++FieldIt) {
    if (IsFutureType(FieldIt->getType())) {
      Futures.push(FieldIt);
    }
  }

  while (!Futures.empty()) {
    RecordDecl::field_iterator FtField = Futures.top();
    RecordDecl *FatPointerRD =
        dyn_cast<RecordType>(FtField->getType().getDesugaredType(S.Context))
            ->getDecl();
    Futures.pop();

    Expr *DRE = S.BuildDeclRefExpr(PVD, ParamType, VK_LValue, SourceLocation());
    Expr *PDRE =
        ImplicitCastExpr::Create(S.Context, ParamType, CK_LValueToRValue, DRE,
                                 nullptr, VK_PRValue, FPOptionsOverride());
    // this.Ft_<x>
    Expr *FutureExpr = S.BuildMemberExpr(
        PDRE, true, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *FtField,
        DeclAccessPair::make(PVD, FtField->getAccess()), false,
        DeclarationNameInfo(), FtField->getType().getNonReferenceType(),
        VK_LValue, OK_Ordinary);

    RecordDecl::field_iterator PtrField, VtableField, FPFieldIt;
    for (FPFieldIt = FatPointerRD->field_begin();
         FPFieldIt != FatPointerRD->field_end(); ++FPFieldIt) {
      if (FPFieldIt->getDeclName().getAsString() == "data") {
        PtrField = FPFieldIt;
      } else if (FPFieldIt->getDeclName().getAsString() == "vtable") {
        VtableField = FPFieldIt;
      }
    }

    // this.Ft_<x>.vtable
    Expr *VtableExpr = S.BuildMemberExpr(
        FutureExpr, false, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *VtableField,
        DeclAccessPair::make(FatPointerRD, VtableField->getAccess()), false,
        DeclarationNameInfo(), VtableField->getType(), VK_LValue, OK_Ordinary);
    VtableExpr = ImplicitCastExpr::Create(
        S.Context, VtableExpr->getType(), CK_LValueToRValue, VtableExpr,
        nullptr, VK_PRValue, FPOptionsOverride());

    RecordDecl::field_iterator FreeFuncField, FreeFuncFieldIt;
    const RecordType *RT = dyn_cast<RecordType>(
        VtableField->getType()->getPointeeType().getDesugaredType(S.Context));
    RecordDecl *VtableRD = RT->getDecl();
    for (FreeFuncFieldIt = VtableRD->field_begin();
         FreeFuncFieldIt != VtableRD->field_end(); ++FreeFuncFieldIt) {
      if (FreeFuncFieldIt->getDeclName().getAsString() == "free") {
        FreeFuncField = FreeFuncFieldIt;
      }
    }

    // this.Ft_<x>.vtable->free
    Expr *FreeFuncExpr = S.BuildMemberExpr(
        VtableExpr, true, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *FreeFuncField,
        DeclAccessPair::make(VtableRD, FreeFuncField->getAccess()), false,
        DeclarationNameInfo(), FreeFuncField->getType(), VK_LValue,
        OK_Ordinary);
    FreeFuncExpr = ImplicitCastExpr::Create(
        S.Context, FreeFuncExpr->getType(), CK_LValueToRValue, FreeFuncExpr,
        nullptr, VK_PRValue, FPOptionsOverride());

    std::vector<Expr *> FreeArgs;
    // this.Ft_<x>.data
    Expr *DataExpr = S.BuildMemberExpr(
        FutureExpr, false, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *PtrField,
        DeclAccessPair::make(FatPointerRD, PtrField->getAccess()), false,
        DeclarationNameInfo(), PtrField->getType(), VK_LValue, OK_Ordinary);
    DataExpr = ImplicitCastExpr::Create(S.Context, DataExpr->getType(),
                                        CK_LValueToRValue, DataExpr, nullptr,
                                        VK_PRValue, FPOptionsOverride());
    FreeArgs.push_back(DataExpr);

    // this.Ft_<x>.vtable->free(this.Ft_<x>.data)
    Expr *RHSExpr = S.BuildCallExpr(nullptr, FreeFuncExpr, SourceLocation(),
                                    FreeArgs, SourceLocation())
                        .get();
    Stmts.push_back(RHSExpr);
  }

  // TODO: if future is not malloced, do not need to free it.
  std::string FreeName = "free";
  DeclContext::lookup_result Decls = S.Context.getTranslationUnitDecl()->lookup(
      DeclarationName(&(S.Context.Idents).get(FreeName)));
  FunctionDecl *FreeFunc = nullptr;
  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<FunctionDecl>(*I)) {
        FreeFunc = dyn_cast<FunctionDecl>(*I);
        break;
      }
    }
  } else {
    S.Diag(FD->getBeginLoc(), diag::err_function_not_found)
        << FreeName << "<stdlib.h>";
    return nullptr;
  }

  Expr *FreeFuncRef =
      S.BuildDeclRefExpr(FreeFunc, FreeFunc->getType(), VK_LValue, NLoc);
  FreeFuncRef =
      S.ImpCastExprToType(FreeFuncRef,
                          S.Context.getPointerType(FreeFuncRef->getType()),
                          CK_FunctionToPointerDecay)
          .get();
  Expr *FutureObj =
      S.BuildDeclRefExpr(PVD, ParamType, VK_LValue, SourceLocation());
  FutureObj = ImplicitCastExpr::Create(S.Context, ParamType, CK_LValueToRValue,
                                       FutureObj, nullptr, VK_PRValue,
                                       FPOptionsOverride());

  FutureObj = S.BuildCStyleCastExpr(
                   SourceLocation(),
                   S.Context.getTrivialTypeSourceInfo(S.Context.VoidPtrTy),
                   SourceLocation(), FutureObj)
                  .get();

  SmallVector<Expr *, 1> Args;
  Args.push_back(FutureObj);
  // free(this)
  Expr *CE = S.BuildCallExpr(nullptr, FreeFuncRef, SourceLocation(), Args,
                             SourceLocation())
                 .get();
  Stmts.push_back(CE);

  CompoundStmt *CS =
      CompoundStmt::Create(S.Context, Stmts, FPOptionsOverride(), SLoc, NLoc);
  NewFD->setBody(CS);
  sema::AnalysisBasedWarnings::Policy *ActivePolicy = nullptr;
  S.PopFunctionScopeInfo(ActivePolicy, NewFD);
  S.PushOnScopeChains(NewFD, S.getCurScope(), true);
  return NewFD;
}

static BSCMethodDecl *buildPollFunction(Sema &S, RecordDecl *RD,
                                        RecordDecl *PollResultRD,
                                        FunctionDecl *FD,
                                        RecordDecl *FatPointerRD,
                                        int FutureStateNumber) {
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation NLoc = FD->getNameInfo().getLoc();
  QualType Ty = FD->getDeclaredReturnType();

  std::string FName = "poll";

  QualType FuncRetType = S.Context.getRecordType(PollResultRD);
  QualType ParamType = S.Context.getPointerType(S.Context.getRecordType(RD));
  SmallVector<QualType, 1> ParamTys;
  ParamTys.push_back(ParamType);

  QualType FuncType = S.Context.getFunctionType(FuncRetType, ParamTys, {});
  QualType OriginType = S.Context.getFunctionType(Ty, ParamTys, {});

  BSCMethodDecl *NewFD = buildAsyncBSCMethodDecl(S.Context, RD, SLoc, NLoc,
                                                 &(S.Context.Idents).get(FName),
                                                 OriginType, nullptr, SC_None);
  NewFD->setLexicalDeclContext(S.Context.getTranslationUnitDecl());
  S.Context.BSCDeclContextMap[RD->getTypeForDecl()] = RD;
  S.PushFunctionScope();

  SmallVector<ParmVarDecl *, 1> ParmVarDecls;
  ParmVarDecl *PVD = ParmVarDecl::Create(
      S.Context, NewFD, SourceLocation(), SourceLocation(),
      &(S.Context.Idents).get("this"), ParamType, nullptr, SC_None, nullptr);
  ParmVarDecls.push_back(PVD);
  NewFD->setParams(ParmVarDecls);

  std::vector<Stmt *> Stmts;

  RecordDecl::field_iterator FutureStateField;
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       FieldIt != RD->field_end(); ++FieldIt) {
    if (FieldIt->getDeclName().getAsString() == "__future_state") {
      FutureStateField = FieldIt;
    }
  }

  Expr *FutureObj =
      S.BuildDeclRefExpr(PVD, ParamType, VK_LValue, SourceLocation());
  FutureObj = ImplicitCastExpr::Create(S.Context, ParamType, CK_LValueToRValue,
                                       FutureObj, nullptr, VK_PRValue,
                                       FPOptionsOverride());
  // Creating Switch Stmt
  DeclarationName FutureName = FutureStateField->getDeclName();
  DeclarationNameInfo MemberNameInfo(FutureName,
                                     FutureStateField->getLocation());
  // this.__future_state
  Expr *ConditionVariable = S.BuildMemberExpr(
      FutureObj, true, SourceLocation(), NestedNameSpecifierLoc(),
      SourceLocation(), *FutureStateField,
      DeclAccessPair::make(*FutureStateField, FutureStateField->getAccess()),
      false, MemberNameInfo, FutureStateField->getType().getNonReferenceType(),
      VK_LValue, OK_Ordinary);
  S.setFunctionHasBranchIntoScope();
  SwitchStmt *SS =
      SwitchStmt::Create(S.Context, nullptr, nullptr, ConditionVariable,
                         SourceLocation(), SourceLocation());

  S.getCurFunction()->SwitchStack.push_back(
      FunctionScopeInfo::SwitchInfo(SS, false));

  std::vector<Stmt *> CaseStmts;
  std::vector<LabelDecl *> LabelDecls;
  for (int i = 0; i < FutureStateNumber; i++) {
    llvm::APInt ResultVal(S.Context.getTargetInfo().getIntWidth(), i);
    Expr *LHSExpr = IntegerLiteral::Create(S.Context, ResultVal,
                                           S.Context.IntTy, SourceLocation());
    CaseStmt *CS =
        CaseStmt::Create(S.Context, LHSExpr, nullptr, SourceLocation(),
                         SourceLocation(), SourceLocation());
    LabelDecl *LD =
        LabelDecl::Create(S.Context, NewFD, SourceLocation(),
                          &(S.Context.Idents).get("__L" + std::to_string(i)));
    LabelDecls.push_back(LD);
    Stmt *RHSExpr =
        new (S.Context) GotoStmt(LD, SourceLocation(), SourceLocation());
    CS->setSubStmt(RHSExpr);
    CaseStmts.push_back(CS);
    SS->addSwitchCase(CS);
  }

  CompoundStmt *SSBody = CompoundStmt::Create(S.Context, CaseStmts,
                                              FPOptionsOverride(), SLoc, NLoc);

  SS->setBody(SSBody);
  Stmts.push_back(SS);

  int StmtSize = Stmts.size();

  TransformToReturnVoid(S).TransformFunctionDecl(FD);

  NewFD->setType(S.Context.getFunctionType(FD->getReturnType(), ParamTys, {}));
  S.PushDeclContext(S.getCurScope(), NewFD);

  TransformToAP DT = TransformToAP(S, FutureObj, RD, NewFD);
  StmtResult MemberChangeRes = DT.TransformStmt(FD->getBody());
  Stmt *FuncBody = MemberChangeRes.get();

  StmtResult SingleStateRes =
      TransformToHasSingleState(S, DT).TransformStmt(FuncBody);
  FuncBody = SingleStateRes.get();

  NewFD->setType(FuncType);
  ARFinder ARInitlizer = ARFinder(S, FutureObj, RD, NewFD, Ty);
  ARInitlizer.Visit(FuncBody);

  StmtResult ARToCSRes =
      TransformARToCS(S, ARInitlizer).TransformStmt(FuncBody);
  FuncBody = ARToCSRes.get();

  AEFinder AEInitlizer =
      AEFinder(S, NewFD->getParamDecl(0), FutureObj, RD, NewFD);
  AEInitlizer.Visit(FuncBody);

  StmtResult AEToCSRes =
      TransformAEToCS(S, AEInitlizer, LabelDecls).TransformStmt(FuncBody);
  S.PopDeclContext();

  for (auto *C : AEToCSRes.get()->children()) {
    Stmts.push_back(C);
  }

  int CurStmtSize = Stmts.size();
  if (CurStmtSize > StmtSize) {
    Stmt *FirstStmt = Stmts[StmtSize];
    LabelStmt *LS =
        new (S.Context) LabelStmt(SourceLocation(), LabelDecls[0], FirstStmt);
    Stmts.erase(Stmts.begin() + StmtSize);
    Stmts.insert(Stmts.begin() + StmtSize, LS);
  } else {
    llvm::APInt ResultVal(S.Context.getTargetInfo().getIntWidth(), 0);
    Stmt *IL = IntegerLiteral::Create(S.Context, ResultVal, S.Context.IntTy,
                                      SourceLocation());
    LabelStmt *LS =
        new (S.Context) LabelStmt(SourceLocation(), LabelDecls[0], IL);
    Stmts.push_back(LS);
  }

  CompoundStmt *CS =
      CompoundStmt::Create(S.Context, Stmts, FPOptionsOverride(), SLoc, NLoc);
  NewFD->setBody(CS);
  S.PushOnScopeChains(NewFD, S.getCurScope(), true);
  sema::AnalysisBasedWarnings::Policy *ActivePolicy = nullptr;
  S.PopFunctionScopeInfo(ActivePolicy, NewFD);
  return NewFD->isInvalidDecl() ? nullptr : NewFD;
}

// BSC extensions for await keyword
ExprResult Sema::BuildAwaitExpr(SourceLocation AwaitLoc, Expr *E) {
  assert(E && "null expression");

  if (E->getType()->isDependentType()) {
    Expr *Res = new (Context) AwaitExpr(AwaitLoc, E, Context.DependentTy);
    return Res;
  }

  Expr *InnerE = E;
  QualType AwaitReturnTy = InnerE->getType();
  bool IsCall = isa<CallExpr>(InnerE);
  if (IsCall) {
    Decl *AwaitDecl = (dyn_cast<CallExpr>(InnerE))->getCalleeDecl();
    FunctionDecl *FDecl = dyn_cast_or_null<FunctionDecl>(AwaitDecl);
    if (!FDecl) {
      return ExprError();
    }
    if (!FDecl->isAsyncSpecified() && !IsFutureType(AwaitReturnTy)) {
      // TODO: modify error message
      Diag(InnerE->getExprLoc(), PDiag(diag::err_not_a_async_call)
                                     << getExprRange(InnerE));
      return ExprError();
    }
  } else {
    if (!IsFutureType(AwaitReturnTy)) {
      Diag(InnerE->getExprLoc(), PDiag(diag::err_not_a_async_call)
                                     << getExprRange(InnerE));
      return ExprError();
    }
  }

  if (IsFutureType(AwaitReturnTy)) {
    const RecordType *FPRD =
        dyn_cast<RecordType>(AwaitReturnTy.getDesugaredType(Context));
    RecordDecl *RD = FPRD->getDecl();
    for (RecordDecl::field_iterator FieldIt = RD->field_begin(),
                                    Field_end = RD->field_end();
         FieldIt != Field_end; ++FieldIt) {
      if (FieldIt->getDeclName().getAsString() == "vtable") {
        const RecordType *VTRD = dyn_cast<RecordType>(
            FieldIt->getType()->getPointeeType().getDesugaredType(Context));
        RecordDecl *NewRD = VTRD->getDecl();
        for (RecordDecl::field_iterator FieldIt = NewRD->field_begin(),
                                        Field_end = NewRD->field_end();
             FieldIt != Field_end; ++FieldIt) {
          if (FieldIt->getDeclName().getAsString() == "poll") {
            const RecordType *VTRD1 = dyn_cast<RecordType>(
                dyn_cast<FunctionType>(
                    FieldIt->getType()->getPointeeType().getDesugaredType(
                        Context))
                    ->getReturnType()
                    .getDesugaredType(Context));
            RecordDecl *NewRD1 = VTRD1->getDecl();
            for (RecordDecl::field_iterator FieldIt = NewRD1->field_begin(),
                                            Field_end = NewRD1->field_end();
                 FieldIt != Field_end; ++FieldIt) {
              if (FieldIt->getDeclName().getAsString() == "res") {
                AwaitReturnTy = FieldIt->getType();
              }
            }
          }
        }
      }
    }
  }

  // build AwaitExpr
  AwaitExpr *Res = new (Context) AwaitExpr(AwaitLoc, InnerE, AwaitReturnTy);
  return Res;
}

ExprResult Sema::ActOnAwaitExpr(SourceLocation AwaitLoc, Expr *E) {
  assert(FunctionScopes.size() > 0 && "FunctionScopes missing");
  if (FunctionScopes.size() < 1 ||
      getCurFunction()->CompoundScopes.size() < 1) {
    Diag(AwaitLoc, diag::err_await_invalid_scope) << "this scope.";
    return ExprError();
  }
  return BuildAwaitExpr(AwaitLoc, E);
}

void Sema::ActOnAsyncFunctionDefinition(FunctionDecl *FD,
                                        SmallVector<Decl *, 8> DeclsInGroup) {
  if (!IsFutureType(FD->getReturnType())) {
    QualType ReturnTy = FD->getReturnType();
    ReturnTy.removeLocalConst();
    if (ReturnTy->isVoidType()) {
      RecordDecl *VoidRD = generateVoidStruct(*this);
      ReturnTy = Context.getRecordType(VoidRD);
    }
    QualType PollResultType =
        lookupPollResultType(*this, FD->getBeginLoc(), ReturnTy);
    if (PollResultType.isNull()) {
      return;
    }
    RecordDecl *PollResultRD = PollResultType->getAsRecordDecl();
    std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
        NewRDs =
            generateVtableAndFatPointerStruct(*this, ReturnTy, PollResultRD);

    if (!std::get<1>(std::get<0>(NewRDs)))
      DeclsInGroup.push_back(std::get<0>(std::get<0>(NewRDs)));
    if (!std::get<1>(std::get<1>(NewRDs)))
      DeclsInGroup.push_back(std::get<0>(std::get<1>(NewRDs)));

    FunctionDecl *FutureInitDef = buildFutureInitFunctionDefinition(
        *this, FD, std::get<0>(std::get<1>(NewRDs)));
    DeclsInGroup.push_back(FutureInitDef);
  }
}

SmallVector<Decl *, 8> Sema::ActOnAsyncFunctionDeclaration(FunctionDecl *FD) {
  SmallVector<Decl *, 8> decls;

  if (IsFutureType(FD->getReturnType())) {
    decls.push_back(FD);
    return decls;
  }
  AwaitExprFinder finder = AwaitExprFinder();
  finder.Visit(FD->getBody());

  if (finder.GetAwaitExprNum() == 0) {
    // TODO: return type check.
    decls.push_back(FD);
    return decls;
  }

  IllegalAEFinder IAEFinder = IllegalAEFinder(*this);
  IAEFinder.Visit(FD->getBody());
  if (IAEFinder.hasIllegalAwaitExpr())
    return decls;

  QualType ReturnTy = FD->getReturnType();
  ReturnTy.removeLocalConst();
  if (ReturnTy->isVoidType()) {
    RecordDecl *VoidRD = generateVoidStruct(*this);
    ReturnTy = Context.getRecordType(VoidRD);
  }
  QualType PollResultType =
      lookupPollResultType(*this, FD->getBeginLoc(), ReturnTy);
  if (PollResultType.isNull()) {
    return decls;
  }
  RecordDecl *PollResultRD = PollResultType->getAsRecordDecl();

  std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
      NewRDs = generateVtableAndFatPointerStruct(*this, ReturnTy, PollResultRD);

  if (!std::get<1>(std::get<0>(NewRDs)))
    decls.push_back(std::get<0>(std::get<0>(NewRDs)));
  if (!std::get<1>(std::get<1>(NewRDs)))
    decls.push_back(std::get<0>(std::get<1>(NewRDs)));

  FunctionDecl *FutureInitDef = buildFutureInitFunctionDefinition(
      *this, FD, std::get<0>(std::get<1>(NewRDs)));

  const int FutureStateNumber = finder.GetAwaitExprNum() + 1;
  RecordDecl *RD = buildFutureRecordDecl(*this, FD, finder.GetAwaitExpr(),
                                         finder.GetLocalVarList());

  BSCMethodDecl *PollDecl =
      buildPollFunction(*this, RD, PollResultRD, FD,
                        std::get<0>(std::get<1>(NewRDs)), FutureStateNumber);

  BSCMethodDecl *FreeDecl = buildFreeFunction(*this, RD, FD);
  if (!FreeDecl) {
    return decls;
  }

  VarDecl *VtableDecl =
      buildVtableInitDecl(*this, FD, std::get<0>(std::get<0>(NewRDs)),
                          PollResultRD, PollDecl, FreeDecl);

  FunctionDecl *FutureInit = buildFutureInitFunctionDeclaraion(
      *this, RD, FD, std::get<0>(std::get<1>(NewRDs)), VtableDecl);

  decls.push_back(RD);
  decls.push_back(PollDecl);
  decls.push_back(FreeDecl);
  decls.push_back(VtableDecl);
  decls.push_back(FutureInit);
  return decls;
}