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

#include <algorithm>
#include <cstring>
#include <functional>

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

using namespace clang;
using namespace sema;

static RecordDecl *buildAsyncDataRecord(ASTContext &C, StringRef Name,
                                        SourceLocation StartLoc,
                                        SourceLocation EndLoc,
                                        RecordDecl::TagKind TK) {
  RecordDecl *NewDecl;
  NewDecl = RecordDecl::Create(C, TK, C.getTranslationUnitDecl(), StartLoc,
                               EndLoc, &C.Idents.get(Name));
  return NewDecl;
}

static void addAsyncRecordDecl(ASTContext &C, StringRef Name, QualType Type,
                               SourceLocation StartLoc, SourceLocation EndLoc,
                               RecordDecl *RD) {
  FieldDecl *Field = FieldDecl::Create(
      C, RD, StartLoc, EndLoc, &C.Idents.get(Name), Type,
      C.getTrivialTypeSourceInfo(Type, SourceLocation()),
      /*BW=*/nullptr, /*Mutable=*/false, /*InitStyle=*/ICIS_NoInit);
  Field->setAccess(AS_public);
  RD->addDecl(Field);
}

static FunctionDecl *buildAsyncFuncDecl(ASTContext &C, DeclContext *DC,
                                        SourceLocation StartLoc,
                                        SourceLocation NLoc, DeclarationName N,
                                        QualType T, TypeSourceInfo *TInfo) {
  FunctionDecl *NewDecl =
      FunctionDecl::Create(C, DC, StartLoc, NLoc, N, T, TInfo, SC_None);
  return NewDecl;
}

static BSCMethodDecl *
buildAsyncBSCMethodDecl(ASTContext &C, DeclContext *DC, SourceLocation StartLoc,
                        SourceLocation NLoc, DeclarationName N, QualType T,
                        TypeSourceInfo *TInfo, StorageClass SC, QualType ET) {
  BSCMethodDecl *NewDecl =
      BSCMethodDecl::Create(  // TODO: inline should be passed.
          C, DC, StartLoc, DeclarationNameInfo(N, NLoc), T, TInfo, SC, false,
          false, ConstexprSpecKind::Unspecified, NLoc);
  if (auto RD = dyn_cast_or_null<RecordDecl>(DC)) {
    C.BSCDeclContextMap[RD->getTypeForDecl()] = DC;
    NewDecl->setHasThisParam(true); // bug
    NewDecl->setExtendedType(ET);
  }
  return NewDecl;
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

namespace {
class AwaitExprFinder : public StmtVisitor<AwaitExprFinder> {
  ASTContext &Context;
  int AwaitCount = 0;
  std::vector<Expr *> Args;
  std::vector<std::pair<DeclarationName, QualType>> LocalVarList;
  llvm::DenseMap<StringRef, int> IdentifierNumber;

 public:
  AwaitExprFinder(ASTContext &Context) : Context(Context) {}

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
              if (VD->isExternallyVisible()) continue;

              QualType QT = VD->getType();
              if (QT.isConstQualified()) {
                QT.removeLocalConst();
                VD->setType(QT);
              }
              std::string VDName = VD->getName().str();
              SetIdentifierNumber(StringRef(VDName),
                               GetIdentifierNumber(StringRef(VDName)) + 1);
              if (GetIdentifierNumber(StringRef(VDName)) > 1) {
                VDName =
                    VDName + "_" +
                    std::to_string(GetIdentifierNumber(StringRef(VDName)) - 1);
                VD->setDeclName(&(Context.Idents).get(VDName));
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

  void SetIdentifierNumber(StringRef identifier, int index) {
    assert(!identifier.empty() && "Passed null name");
    IdentifierNumber[identifier] = index;
  }

  int GetIdentifierNumber(StringRef identifier) {
    llvm::DenseMap<StringRef, int>::iterator I = IdentifierNumber.find(identifier);
    if (I != IdentifierNumber.end()) return I->second;
    return 0;
  }
};

bool HasAwaitExpr(Stmt *S) {
  if (S == nullptr) return false;

  if (isa<AwaitExpr>(S)) return true;

  for (auto *C : S->children()) {
    if (HasAwaitExpr(C)) {
      return true;
    }
  }

  return false;
}

/// Look for Illegal AwaitExpr
class IllegalAEFinder : public StmtVisitor<IllegalAEFinder> {
  Sema &SemaRef;
  bool HasIllegalAwait;

 public:
  IllegalAEFinder(Sema &SemaRef) : SemaRef(SemaRef) { HasIllegalAwait = false; }

  bool CheckCondHasAwaitExpr(Stmt *S) {
    Expr *condE = nullptr;
    bool CHasIllegalAwait = false;
    std::string ArgString;

    switch (S->getStmtClass()) {
      case Stmt::IfStmtClass: {
        ArgString = "condition statement of if statement";
        condE = cast<IfStmt>(S)->getCond();
        CHasIllegalAwait = HasAwaitExpr(cast<Stmt>(condE));
        break;
      }

      case Stmt::WhileStmtClass: {
        ArgString = "condition statement of while loop";
        condE = cast<WhileStmt>(S)->getCond();
        CHasIllegalAwait = HasAwaitExpr(cast<Stmt>(condE));
        break;
      }

      case Stmt::DoStmtClass: {
        ArgString = "condition statement of do/while loop";
        condE = cast<DoStmt>(S)->getCond();
        CHasIllegalAwait = HasAwaitExpr(cast<Stmt>(condE));
        break;
      }

      case Stmt::ForStmtClass: {
        ArgString = "condition statement of for loop";
        Stmt *Init = cast<ForStmt>(S)->getInit();
        if (Init != nullptr) CHasIllegalAwait = HasAwaitExpr(Init);

        if (!CHasIllegalAwait) {
          condE = cast<ForStmt>(S)->getCond();
          CHasIllegalAwait = HasAwaitExpr(cast<Stmt>(condE));
        }

        break;
      }

      case Stmt::SwitchStmtClass: {
        ArgString = "condition statement of switch statement";
        condE = cast<SwitchStmt>(S)->getCond();
        CHasIllegalAwait = HasAwaitExpr(cast<Stmt>(condE));
        break;
      }

      default:
        break;
    }

    if (CHasIllegalAwait) {
      SemaRef.Diag(S->getBeginLoc(), diag::err_await_invalid_scope)
          << ArgString;
    }

    if (CHasIllegalAwait && !HasIllegalAwait)
      HasIllegalAwait = CHasIllegalAwait;

    return CHasIllegalAwait;
  }

  bool CheckExprHasAwaitExpr(Stmt *S) {
    bool IsContinue = true;
    std::string ArgString;

    if (isa<BinaryOperator>(S)) {
      ArgString = "binary operator expression";
      BinaryOperator *BO = cast<BinaryOperator>(S);
      switch (BO->getOpcode()) {
        case BO_Comma:
        case BO_Assign:
          return false;

        default:
          break;
      }
    } else if (isa<ConditionalOperator>(S)) {
      ArgString = "condition operator expression";
    } else if (isa<CallExpr>(S)) {
      ArgString = "function parameters";
      CallExpr *CE = cast<CallExpr>(S);
      int AwaitNum = 0;
      int CallNum = 0;
      int Count = CE->getNumArgs();
      for (int i = 0; i < Count; i++) {
        CheckAwaitInFuncParams(CE->getArg(i), AwaitNum, CallNum);
      }

      if (AwaitNum >= 2 || (AwaitNum >= 1 && CallNum >= 1)) {
        SemaRef.Diag(S->getBeginLoc(), diag::err_await_invalid_scope)
            << ArgString;

        if (!HasIllegalAwait) HasIllegalAwait = true;
      }
      return IsContinue;
    }

    bool CHasIllegalAwait = HasAwaitExpr(S);

    if (CHasIllegalAwait && !HasIllegalAwait)
      HasIllegalAwait = CHasIllegalAwait;

    if (CHasIllegalAwait) {
      SemaRef.Diag(S->getBeginLoc(), diag::err_await_invalid_scope)
          << ArgString;
    }
    return IsContinue;
  }

  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C) {
        if (isa<IfStmt>(C) || isa<WhileStmt>(C) || isa<ForStmt>(C) ||
            isa<DoStmt>(C) || isa<SwitchStmt>(C)) {
          CheckCondHasAwaitExpr(C);
        } else if (isa<BinaryOperator>(C) || isa<ConditionalOperator>(C) ||
                   isa<CallExpr>(C)) {
          bool IsContinue = CheckExprHasAwaitExpr(C);
          if (IsContinue) continue;
        }

        Visit(C);
      }
    }
  }

  void CheckAwaitInFuncParams(Stmt *S, int &AwaitNum, int &CallNum) {
    if (isa<AwaitExpr>(S)) {
      AwaitNum++;
      return;
    }

    if (isa<CallExpr>(S)) {
      CallNum++;
      return;
    }

    for (auto *C : S->children()) {
      if (C) {
        // In function parameters, if the number of await expr is greater than
        // or equal 2, or the function parameters have await expr and call expr,
        // then report error.
        if (AwaitNum >= 2 || (AwaitNum >= 1 && CallNum >= 1)) return;
        Visit(C);
      }
    }
  }

  bool hasIllegalAwaitExpr() { return HasIllegalAwait; }
};

bool HasReturnStmt(Stmt *S) {
  if (S == nullptr) return false;

  if (isa<ReturnStmt>(S)) return true;

  for (auto *C : S->children()) {
    if (HasReturnStmt(C)) {
      return true;
    }
  }

  return false;
}

bool IsRefactorStmt(Stmt *S) {
  if (isa<CompoundStmt>(S) || isa<IfStmt>(S) || isa<WhileStmt>(S) ||
      isa<ForStmt>(S) || isa<DoStmt>(S) || isa<SwitchStmt>(S))
    return true;

  return false;
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

  if (PollResultRecord.isNull()) return QualType();
  if (S.RequireCompleteType(SLoc, PollResultRecord,
                            diag::err_coroutine_type_missing_specialization))
    return QualType();
  return PollResultRecord;
}
}  // namespace

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
      RecordDecl *FatPointerStruct = nullptr;
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
      }

      assert(FatPointerStruct != nullptr);

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

static RecordDecl *generateVoidStruct(Sema &S, SourceLocation BLoc,
                                      SourceLocation ELoc) {
  std::string Recordname = "Void";
  DeclContext::lookup_result Decls = S.Context.getTranslationUnitDecl()->lookup(
      DeclarationName(&(S.Context.Idents).get(Recordname)));
  RecordDecl *VoidRD = nullptr;

  if (Decls.isSingleResult()) {
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<RecordDecl>(*I)) {
        VoidRD = dyn_cast<RecordDecl>(*I);
        break;
      }
    }
  } else if (Decls.empty()) {
    VoidRD = buildAsyncDataRecord(S.Context, Recordname, BLoc, ELoc,
                                  clang::TagDecl::TagKind::TTK_Struct);
    VoidRD->startDefinition();
    VoidRD->completeDefinition();
    S.PushOnScopeChains(VoidRD, S.getCurScope(), true);
  }
  return VoidRD;
}

/// TODO: will be removed after using Future in stdlib
static std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
generateVtableAndFatPointerStruct(Sema &S, QualType T, RecordDecl *PollResultRD,
                                  FunctionDecl *FD) {
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
                                        FD->getBeginLoc(), FD->getEndLoc(),
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
        S.Context, FatPointerName, FD->getBeginLoc(), FD->getEndLoc(),
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
      S.Context, S.Context.getTranslationUnitDecl(), FD->getBeginLoc(),
      FD->getEndLoc(), &(S.Context.Idents).get(IName),
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
  PollInit->HasBSCScopeSpec = true;
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
  FreeInit->HasBSCScopeSpec = true;
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

FunctionDecl *buildFutureInitFunctionDefinition(Sema &S, RecordDecl *RD,
                                                FunctionDecl *FD,
                                                RecordDecl *FatPointerRD,
                                                VarDecl *VtableInit,
                                                FunctionDecl *NewFD) {
  SourceLocation SLoc = FD->getBeginLoc();
  SourceLocation NLoc = FD->getNameInfo().getLoc();
  DeclarationName funcName = FD->getDeclName();
  SmallVector<QualType, 16> ParamTys;
  FunctionDecl::param_const_iterator pi;
  for (pi = FD->param_begin(); pi != FD->param_end(); pi++) {
    ParamTys.push_back((*pi)->getType());
  }
  std::string FName = "__" + funcName.getAsString();

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

FunctionDecl *buildFutureInitFunctionDeclaration(Sema &S, FunctionDecl *FD,
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

  FunctionDecl *NewFD = nullptr;
  if (isa<BSCMethodDecl>(FD)) {
    BSCMethodDecl *BMD = cast<BSCMethodDecl>(FD);
    NewFD = buildAsyncBSCMethodDecl(S.Context, FD->getDeclContext(), SLoc, NLoc,
                                    &(S.Context.Idents).get(FName), FuncType,
                                    Tinfo, SC_None, BMD->getExtendedType());
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
    IsCompletedFDRef->HasBSCScopeSpec = true;
    IsCompletedFDRef =
        S.ImpCastExprToType(
             IsCompletedFDRef,
             S.Context.getPointerType(IsCompletedFDRef->getType()),
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
    PendingFDRef->HasBSCScopeSpec = true;
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

namespace {
class TransformToReturnVoid : public TreeTransform<TransformToReturnVoid> {
  typedef TreeTransform<TransformToReturnVoid> BaseTransform;
  QualType ReturnTy = QualType();

 public:
  TransformToReturnVoid(Sema &SemaRef) : BaseTransform(SemaRef) {}

  // make sure redo semantic analysis
  bool AlwaysRebuild() { return true; }

  FunctionDecl *TransformFunctionDecl(FunctionDecl *D) {
    FunctionDecl *NewFD = D;
    if (D->getReturnType()->isVoidType()) {
      std::string ReturnStructName = "Void";
      DeclContext::lookup_result ReturnDecls =
          SemaRef.Context.getTranslationUnitDecl()->lookup(
              DeclarationName(&(SemaRef.Context.Idents).get(ReturnStructName)));
      RecordDecl *ReturnDecl = nullptr;
      if (ReturnDecls.isSingleResult()) {
        for (DeclContext::lookup_result::iterator I = ReturnDecls.begin(),
                                                  E = ReturnDecls.end();
             I != E; ++I) {
          if (isa<RecordDecl>(*I)) {
            ReturnDecl = dyn_cast<RecordDecl>(*I);
            break;
          }
        }
      }

      assert(ReturnDecl && "struct Void not generated");
      ReturnTy = SemaRef.Context.getRecordType(ReturnDecl);
      SmallVector<QualType, 16> ParamTys;
      FunctionDecl::param_const_iterator pi;
      for (pi = D->param_begin(); pi != D->param_end(); pi++) {
        ParamTys.push_back((*pi)->getType());
      }
      QualType FuncType =
          SemaRef.Context.getFunctionType(ReturnTy, ParamTys, {});

      SourceLocation SLoc = D->getBeginLoc();
      SourceLocation NLoc = D->getNameInfo().getLoc();
      TypeSourceInfo *Tinfo = D->getTypeSourceInfo();
      std::string FName = std::string(D->getIdentifier()->getName());

      if (isa<BSCMethodDecl>(D)) {
        BSCMethodDecl *BMD = cast<BSCMethodDecl>(D);
        NewFD = buildAsyncBSCMethodDecl(
            SemaRef.Context, D->getDeclContext(), SLoc, NLoc,
            &(SemaRef.Context.Idents).get(FName), FuncType, Tinfo, SC_None,
            BMD->getExtendedType());
      } else {
        NewFD = buildAsyncFuncDecl(SemaRef.Context, D->getDeclContext(), SLoc,
                                   NLoc, &(SemaRef.Context.Idents).get(FName),
                                   FuncType, Tinfo);
      }
      SmallVector<ParmVarDecl *, 4> ParmVarDecls;
      for (const auto &I : D->parameters()) {
        ParmVarDecl *PVD = ParmVarDecl::Create(
            SemaRef.Context, NewFD, SourceLocation(), SourceLocation(),
            &(SemaRef.Context.Idents).get(I->getName()), I->getType(), nullptr,
            SC_None, nullptr);
        ParmVarDecls.push_back(PVD);
      }
      NewFD->setParams(ParmVarDecls);
      NewFD->setLexicalDeclContext(SemaRef.Context.getTranslationUnitDecl());

      CompoundStmt *body = dyn_cast<CompoundStmt>(D->getBody());
      Stmt *LastStmt = body->body_back();
      if (!LastStmt || !dyn_cast<ReturnStmt>(LastStmt)) {
        std::vector<Stmt *> Stmts;
        for (auto *C : body->children()) {
          Stmts.push_back(C);
        }
        ReturnStmt *RS = ReturnStmt::Create(SemaRef.Context, SourceLocation(),
                                            nullptr, nullptr);
        Stmts.push_back(RS);
        Sema::CompoundScopeRAII CompoundScope(SemaRef);
        body = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                                  SourceLocation(), false)
                   .getAs<CompoundStmt>();
      }

      Stmt *FuncBody = BaseTransform::TransformStmt(body).getAs<Stmt>();
      NewFD->setBody(FuncBody);
    }
    return NewFD;
  }

  StmtResult TransformReturnStmt(ReturnStmt *S) {
    assert(!S->getRetValue() && "void function should not return a value");
    InitListExpr *ILE = new (SemaRef.Context)
        InitListExpr(SemaRef.Context, SourceLocation(), {}, SourceLocation());
    QualType Ty = SemaRef.Context.getElaboratedType(ETK_Struct, nullptr,
                                                    ReturnTy, nullptr);
    ILE->setType(Ty);
    TypeSourceInfo *superTInfo = SemaRef.Context.getTrivialTypeSourceInfo(Ty);
    CompoundLiteralExpr *CLE = new (SemaRef.Context) CompoundLiteralExpr(
        SourceLocation(), superTInfo, Ty, VK_LValue, ILE, false);
    ImplicitCastExpr *ICE =
        ImplicitCastExpr::Create(SemaRef.Context, Ty, CK_LValueToRValue, CLE,
                                 nullptr, VK_PRValue, FPOptionsOverride());
    ReturnStmt *RS =
        ReturnStmt::Create(SemaRef.Context, SourceLocation(), ICE, nullptr);
    return RS;
  }
};
}  // namespace

namespace {
class TransformToAP : public TreeTransform<TransformToAP> {
  typedef TreeTransform<TransformToAP> BaseTransform;
  Expr *PDRE;
  RecordDecl *FutureRD;
  BSCMethodDecl *FD;
  std::vector<Stmt *> DeclStmts;
  int DIndex;
  llvm::DenseMap<Stmt *, std::tuple<int, int>> DMap;
  std::map<std::string, VarDecl *> ArrayPointerMap;
  std::map<std::string, VarDecl *> ArrayAssignedPointerMap;

 public:
  TransformToAP(Sema &SemaRef, Expr *PDRE, RecordDecl *FutureRD,
                BSCMethodDecl *FD)
      : BaseTransform(SemaRef), PDRE(PDRE), FutureRD(FutureRD), FD(FD) {
    DIndex = 0;
  }

  // make sure redo semantic analysis
  bool AlwaysRebuild() { return true; }

  ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
    Expr *RE = E;
    RecordDecl::field_iterator TheField, FieldIt;
    for (FieldIt = FutureRD->field_begin(); FieldIt != FutureRD->field_end();
         ++FieldIt) {
      if (FieldIt->getDeclName().getAsString() ==
          cast<DeclRefExpr>(RE)->getDecl()->getName()) {
        TheField = FieldIt;
        break;
      }
    }

    if (FieldIt != FutureRD->field_end()) {
      DeclarationName Name = TheField->getDeclName();
      DeclarationNameInfo MemberNameInfo(Name, TheField->getLocation());
      RE = BaseTransform::RebuildMemberExpr(
               PDRE, SourceLocation(), true, NestedNameSpecifierLoc(),
               SourceLocation(), MemberNameInfo, *TheField,
               DeclAccessPair::make(*TheField, TheField->getAccess()).getDecl(),
               nullptr, nullptr)
               .getAs<Expr>();
    }

    return RE;
  }

  StmtResult TransformDeclStmt(DeclStmt *S) {
    Stmt *Result = nullptr;
    DeclStmt *StmDecl = S;
    int CIndex = 0;

    std::vector<BinaryOperator *> BOStmts;
    std::vector<Stmt *> NullStmts;
    for (auto *SD : StmDecl->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(SD)) {
        Expr *Init = const_cast<Expr *>(VD->getInit());
        Expr *LE = nullptr;
        Expr *RE = nullptr;
        QualType QT = VD->getType();

        if (VD->isExternallyVisible()) return S;

        RecordDecl::field_iterator LField, FieldIt;
        for (FieldIt = FutureRD->field_begin();
             FieldIt != FutureRD->field_end(); ++FieldIt) {
          if (FieldIt->getDeclName().getAsString() == VD->getName()) {
            LField = FieldIt;
            break;
          }
        }

        if (FieldIt != FutureRD->field_end()) {
          DeclarationName Name = LField->getDeclName();
          DeclarationNameInfo MemberNameInfo(Name, LField->getLocation());
          LE = BaseTransform::RebuildMemberExpr(
                   PDRE, SourceLocation(), true, NestedNameSpecifierLoc(),
                   SourceLocation(), MemberNameInfo, *LField,
                   DeclAccessPair::make(*LField, LField->getAccess()).getDecl(),
                   nullptr, nullptr)
                   .getAs<Expr>();
        }

        if (QT->isArrayType() || QT->isRecordType() ||
            QT->isRValueReferenceType()) {
          std::string ArgName = VD->getName().str();
          VarDecl *ArgVDNew = VarDecl::Create(
              SemaRef.Context, FD, SourceLocation(), SourceLocation(),
              &(SemaRef.Context.Idents).get(ArgName),
              VD->getType().getNonReferenceType(), nullptr, SC_None);

          Expr *CInit = nullptr;
          if (Init != nullptr) {
            CInit = BaseTransform::TransformExpr(Init).get();
          } else {
            if (VD->getType()->isArrayType()) {
              auto *CAT = cast<ConstantArrayType>(
                  VD->getType()->castAsArrayTypeUnsafe());
              InitListExpr *ILE = new (SemaRef.Context) InitListExpr(
                  SemaRef.Context, SourceLocation(), None, SourceLocation());
              ILE->setType(VD->getType());
              Expr *Filler = new (SemaRef.Context)
                  ImplicitValueInitExpr(CAT->getElementType());
              ILE->setArrayFiller(Filler);
              Init = CInit = ILE;
            } else {
              Expr *CE =
                  new (SemaRef.Context) ImplicitValueInitExpr(LE->getType());
              Init = CInit = CE;
            }
          }

          SemaRef.AddInitializerToDecl(ArgVDNew, CInit, /*DirectInit=*/false);
          DeclStmt *DSNew =
              SemaRef
                  .ActOnDeclStmt(SemaRef.ConvertDeclToDeclGroup(ArgVDNew),
                                 SourceLocation(), SourceLocation())
                  .getAs<DeclStmt>();

          DeclStmts.push_back(DSNew);

          RE = SemaRef.BuildDeclRefExpr(ArgVDNew,
                                        VD->getType().getNonReferenceType(),
                                        VK_LValue, SourceLocation());
          RE = ImplicitCastExpr::Create(
              SemaRef.Context, VD->getType().getNonReferenceType(),
              CK_LValueToRValue, RE, nullptr, VK_PRValue, FPOptionsOverride());
          DIndex++;
          CIndex++;

          if (QT->isArrayType()) {
            int Elements = 1;

            QualType SubQT = QT;
            while (const ConstantArrayType *AT =
                       SemaRef.Context.getAsConstantArrayType(SubQT)) {
              Elements *= AT->getSize().getZExtValue();
              if (AT->getSize() == 0) return true;
              SubQT = AT->getElementType();
            }

            QualType Pty = SemaRef.Context.getPointerType(SubQT);
            Expr *AssignedRVExpr = SemaRef.BuildDeclRefExpr(
                ArgVDNew, ArgVDNew->getType(), VK_LValue, SourceLocation());
            AssignedRVExpr =
                SemaRef
                    .ImpCastExprToType(AssignedRVExpr,
                                       SemaRef.Context.getArrayDecayedType(QT),
                                       CK_ArrayToPointerDecay)
                    .get();
            TypeSourceInfo *AssignedType =
                SemaRef.Context.getTrivialTypeSourceInfo(Pty);
            Expr *AssignedCCE = BaseTransform::RebuildCStyleCastExpr(
                                    SourceLocation(), AssignedType,
                                    SourceLocation(), AssignedRVExpr)
                                    .get();

            std::string AssignedPtrName =
                "__ASSIGNED_ARRAY_PTR_" + GetPrefix(SubQT);
            VarDecl *AssignedPtrVar =
                GetArrayAssignedPointerMap(AssignedPtrName);
            if (AssignedPtrVar == nullptr) {
              AssignedPtrVar = VarDecl::Create(
                  SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                  &(SemaRef.Context.Idents).get(AssignedPtrName),
                  Pty.getNonReferenceType(), nullptr, SC_None);

              SemaRef.AddInitializerToDecl(AssignedPtrVar, AssignedCCE,
                                           /*DirectInit=*/false);

              DeclStmt *AssignedDS =
                  SemaRef
                      .ActOnDeclStmt(
                          SemaRef.ConvertDeclToDeclGroup(AssignedPtrVar),
                          SourceLocation(), SourceLocation())
                      .getAs<DeclStmt>();

              DeclStmts.push_back(AssignedDS);
              SetArrayAssignedPointerMap(AssignedPtrName, AssignedPtrVar);
            } else {
              Expr *AssignedDRE = SemaRef.BuildDeclRefExpr(
                  AssignedPtrVar, AssignedPtrVar->getType(), VK_LValue,
                  SourceLocation());
              Stmt *AssignedBO =
                  BaseTransform::RebuildBinaryOperator(
                      SourceLocation(), BO_Assign, AssignedDRE, AssignedCCE)
                      .getAs<Stmt>();
              DeclStmts.push_back(AssignedBO);
            }
            DIndex++;
            CIndex++;

            Expr *ArrayRVExpr =
                SemaRef
                    .ImpCastExprToType(LE,
                                       SemaRef.Context.getArrayDecayedType(QT),
                                       CK_ArrayToPointerDecay)
                    .get();
            TypeSourceInfo *ArrayType =
                SemaRef.Context.getTrivialTypeSourceInfo(Pty);
            Expr *ArrayCCE =
                BaseTransform::RebuildCStyleCastExpr(
                    SourceLocation(), ArrayType, SourceLocation(), ArrayRVExpr)
                    .get();

            std::string ArrayPtrName = "__ARRAY_PTR_" + GetPrefix(SubQT);
            VarDecl *ArrayPtrVar = GetArrayPointerMap(ArrayPtrName);
            if (ArrayPtrVar == nullptr) {
              ArrayPtrVar = VarDecl::Create(
                  SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                  &(SemaRef.Context.Idents).get(ArrayPtrName),
                  Pty.getNonReferenceType(), nullptr, SC_None);

              SemaRef.AddInitializerToDecl(ArrayPtrVar, ArrayCCE,
                                           /*DirectInit=*/false);
              DeclStmt *ArrayDS =
                  SemaRef
                      .ActOnDeclStmt(
                          SemaRef.ConvertDeclToDeclGroup(ArrayPtrVar),
                          SourceLocation(), SourceLocation())
                      .getAs<DeclStmt>();

              DeclStmts.push_back(ArrayDS);
              SetArrayPointerMap(ArrayPtrName, ArrayPtrVar);
            } else {
              Expr *ArrayDRE =
                  SemaRef.BuildDeclRefExpr(ArrayPtrVar, ArrayPtrVar->getType(),
                                           VK_LValue, SourceLocation());
              Stmt *ArrayBO =
                  BaseTransform::RebuildBinaryOperator(
                      SourceLocation(), BO_Assign, ArrayDRE, ArrayCCE)
                      .getAs<Stmt>();
              DeclStmts.push_back(ArrayBO);
            }
            DIndex++;
            CIndex++;

            if (Elements > 1) {
              VarDecl *IArgVDNew = VarDecl::Create(
                  SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                  &(SemaRef.Context.Idents).get("I"), SemaRef.Context.IntTy,
                  nullptr, SC_None);
              llvm::APInt Zero(
                  SemaRef.Context.getTypeSize(SemaRef.Context.IntTy), 0);
              Expr *IInit = IntegerLiteral::Create(SemaRef.Context, Zero,
                                                   SemaRef.Context.IntTy,
                                                   SourceLocation());
              IArgVDNew->setInit(IInit);
              DeclGroupRef IDG(IArgVDNew);
              Stmt *Init = new (SemaRef.Context)
                  DeclStmt(IDG, SourceLocation(), SourceLocation());

              Expr *IDRE =
                  SemaRef.BuildDeclRefExpr(IArgVDNew, SemaRef.Context.IntTy,
                                           VK_LValue, SourceLocation());
              Expr *ILE = ImplicitCastExpr::Create(
                  SemaRef.Context, SemaRef.Context.IntTy, CK_LValueToRValue,
                  IDRE, nullptr, VK_PRValue, FPOptionsOverride());

              llvm::APInt ISize(
                  SemaRef.Context.getTypeSize(SemaRef.Context.IntTy), Elements);
              Expr *IRE = IntegerLiteral::Create(SemaRef.Context, ISize,
                                                 SemaRef.Context.IntTy,
                                                 SourceLocation());

              Expr *Cond = BaseTransform::RebuildBinaryOperator(
                               SourceLocation(), BO_LT, ILE, IRE)
                               .getAs<Expr>();

              Expr *Inc = BaseTransform::RebuildUnaryOperator(SourceLocation(),
                                                              UO_PreInc, IDRE)
                              .getAs<Expr>();

              llvm::SmallVector<Stmt *, 1> Stmts;

              Expr *LHS =
                  SemaRef.BuildDeclRefExpr(ArrayPtrVar, ArrayPtrVar->getType(),
                                           VK_LValue, SourceLocation());
              LHS = SemaRef
                        .CreateBuiltinUnaryOp(SourceLocation(), UO_PostInc, LHS)
                        .get();
              LHS =
                  SemaRef.CreateBuiltinUnaryOp(SourceLocation(), UO_Deref, LHS)
                      .get();

              Expr *RHS = SemaRef.BuildDeclRefExpr(AssignedPtrVar,
                                                   AssignedPtrVar->getType(),
                                                   VK_LValue, SourceLocation());
              RHS = SemaRef
                        .CreateBuiltinUnaryOp(SourceLocation(), UO_PostInc, RHS)
                        .get();
              RHS =
                  SemaRef.CreateBuiltinUnaryOp(SourceLocation(), UO_Deref, RHS)
                      .get();
              RHS =
                  SemaRef
                      .ImpCastExprToType(RHS, LHS->getType(), CK_LValueToRValue)
                      .get();

              Expr *BO = BaseTransform::RebuildBinaryOperator(
                             SourceLocation(), BO_Assign, LHS, RHS)
                             .getAs<Expr>();

              Stmts.push_back(BO);
              Sema::CompoundScopeRAII CompoundScope(SemaRef);
              Stmt *Body = BaseTransform::RebuildCompoundStmt(
                               SourceLocation(), Stmts, SourceLocation(), false)
                               .getAs<CompoundStmt>();
              ForStmt *For = new (SemaRef.Context)
                  ForStmt(SemaRef.Context, Init, Cond, nullptr, Inc, Body,
                          SourceLocation(), SourceLocation(), SourceLocation());

              DeclStmts.push_back(For);
              DIndex++;
              CIndex++;

              LE = SemaRef.BuildDeclRefExpr(ArrayPtrVar, ArrayPtrVar->getType(),
                                            VK_LValue, SourceLocation());
              RE = new (SemaRef.Context) ImplicitValueInitExpr(LE->getType());
            }
          }
        }

        if (LE && Init) {
          if (RE == nullptr)
            RE = BaseTransform::TransformExpr(const_cast<Expr *>(Init))
                     .getAs<Expr>();
          BinaryOperator *BO = BaseTransform::RebuildBinaryOperator(
                                   LField->getLocation(), BO_Assign, LE, RE)
                                   .getAs<BinaryOperator>();
          BOStmts.push_back(BO);
        } else if (LE && Init == nullptr) {
          RE = new (SemaRef.Context) ImplicitValueInitExpr(LE->getType());
          Stmt *NoInit = BaseTransform::RebuildBinaryOperator(
                             LField->getLocation(), BO_Assign, LE, RE)
                             .getAs<Stmt>();
          NullStmts.push_back(NoInit);
        }
      }
    }

    int BOSize = BOStmts.size();
    if (BOSize == 0) {
      if (NullStmts.size() != 0)
        Result = NullStmts.front();
      else
        Result = StmDecl;
    } else if (BOSize == 1) {
      Result = cast<Stmt>(BOStmts.front());
    } else {
      BinaryOperator *PreBO = BOStmts.front();
      for (int I = 1; I < BOSize; I++) {
        BinaryOperator *BO = BaseTransform::RebuildBinaryOperator(
                                 SourceLocation(), BO_Comma, PreBO, BOStmts[I])
                                 .getAs<BinaryOperator>();
        PreBO = BO;
      }
      Result = cast<Stmt>(PreBO);
    }

    SetDMap(Result, std::make_tuple(DIndex - CIndex, CIndex));
    return Result;
  }

  void SetDMap(Stmt *S, std::tuple<int, int> val) {
    assert(S && "Passed null DeclStmt");
    DMap[S] = val;
  }

  std::tuple<int, int> GetDMap(Stmt *S) {
    llvm::DenseMap<Stmt *, std::tuple<int, int>>::iterator I = DMap.find(S);
    if (I != DMap.end()) return I->second;
    return {-1, -1};
  }

  std::vector<Stmt *> GetDeclStmts() { return DeclStmts; }

  void SetArrayPointerMap(std::string APName, VarDecl *VD) {
    assert(VD && "Passed null array pointers variable");
    ArrayPointerMap[APName] = VD;
  }

  VarDecl *GetArrayPointerMap(std::string APName) {
    std::map<std::string, VarDecl *>::iterator I = ArrayPointerMap.find(APName);
    if (I != ArrayPointerMap.end())
      return I->second;
    return nullptr;
  }

  void SetArrayAssignedPointerMap(std::string AAPName, VarDecl *VD) {
    assert(VD && "Passed null array pointers variable");
    ArrayAssignedPointerMap[AAPName] = VD;
  }

  VarDecl *GetArrayAssignedPointerMap(std::string AAPName) {
    std::map<std::string, VarDecl *>::iterator I =
        ArrayAssignedPointerMap.find(AAPName);
    if (I != ArrayAssignedPointerMap.end())
      return I->second;
    return nullptr;
  }
};
}  // namespace

namespace {
class TransformToHasSingleState
    : public TreeTransform<TransformToHasSingleState> {
  typedef TreeTransform<TransformToHasSingleState> BaseTransform;
  TransformToAP DT;

 public:
  TransformToHasSingleState(Sema &SemaRef, TransformToAP DT)
      : BaseTransform(SemaRef), DT(DT) {}

  // make sure redo semantic analysis
  bool AlwaysRebuild() { return true; }

  StmtResult TransformIfStmt(IfStmt *S) {
    bool HasStatement = false;
    bool HasStatementElse = false;
    IfStmt *If = S;
    Stmt *TS = If->getThen();
    Stmt *ES = If->getElse();
    if (TS != nullptr) HasStatement = (HasAwaitExpr(TS) || HasReturnStmt(TS));
    if (ES != nullptr)
      HasStatementElse = (HasAwaitExpr(ES) || HasReturnStmt(ES));

    if (HasStatementElse && !IsRefactorStmt(ES)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(ES);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      ES = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                              SourceLocation(), false)
               .getAs<CompoundStmt>();
      If->setElse(ES);
    }

    if (HasStatement && !IsRefactorStmt(TS)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(TS);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      TS = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                              SourceLocation(), false)
               .getAs<CompoundStmt>();
      If->setThen(TS);
    }

    return BaseTransform::TransformIfStmt(If);
  }

  StmtResult TransformWhileStmt(WhileStmt *S) {
    bool HasStatement = false;
    WhileStmt *WS = S;
    Stmt *Body = WS->getBody();

    if (Body != nullptr)
      HasStatement = (HasAwaitExpr(Body) || HasReturnStmt(Body));

    if (HasStatement && !IsRefactorStmt(Body)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(Body);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      Body = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                                SourceLocation(), false)
                 .getAs<CompoundStmt>();
      WS->setBody(Body);
    }
    return BaseTransform::TransformWhileStmt(WS);
  }

  StmtResult TransformDoStmt(DoStmt *S) {
    bool HasStatement = false;
    DoStmt *DS = S;
    Stmt *Body = DS->getBody();

    if (Body != nullptr)
      HasStatement = (HasAwaitExpr(Body) || HasReturnStmt(Body));

    if (HasStatement && !IsRefactorStmt(Body)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(Body);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      Body = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                                SourceLocation(), false)
                 .getAs<CompoundStmt>();
      DS->setBody(Body);
    }
    return BaseTransform::TransformDoStmt(DS);
  }

  StmtResult TransformForStmt(ForStmt *S) {
    bool HasStatement = false;
    ForStmt *FS = S;
    Stmt *Body = FS->getBody();

    if (Body != nullptr)
      HasStatement = (HasAwaitExpr(Body) || HasReturnStmt(Body));
    if (HasStatement && !IsRefactorStmt(Body)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(Body);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      Body = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                                SourceLocation(), false)
                 .getAs<CompoundStmt>();
      FS->setBody(Body);
    }
    return BaseTransform::TransformForStmt(FS);
  }

  StmtResult TransformCaseStmt(CaseStmt *S) {
    bool HasStatement = false;
    CaseStmt *CS = S;
    Stmt *SS = CS->getSubStmt();
    if (SS != nullptr) HasStatement = (HasAwaitExpr(SS) || HasReturnStmt(SS));
    if (HasStatement && !IsRefactorStmt(SS)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(SS);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      SS = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                              SourceLocation(), false)
               .getAs<CompoundStmt>();
      CS->setSubStmt(SS);
    }
    return BaseTransform::TransformCaseStmt(CS);
  }

  StmtResult TransformDefaultStmt(DefaultStmt *S) {
    bool HasStatement = false;
    DefaultStmt *DS = S;
    Stmt *SS = DS->getSubStmt();
    if (SS != nullptr) HasStatement = (HasAwaitExpr(SS) || HasReturnStmt(SS));
    if (HasStatement && !IsRefactorStmt(SS)) {
      std::vector<Stmt *> Stmts;
      Stmts.push_back(SS);
      Sema::CompoundScopeRAII CompoundScope(SemaRef);
      SS = BaseTransform::RebuildCompoundStmt(SourceLocation(), Stmts,
                                              SourceLocation(), false)
               .getAs<CompoundStmt>();
      DS->setSubStmt(SS);
    }
    return BaseTransform::TransformDefaultStmt(DS);
  }

  StmtResult TransformCompoundStmt(CompoundStmt *S) {
    if (S == nullptr) return S;

    std::vector<Stmt *> Statements;
    for (auto *C : S->children()) {
      Stmt *SS = const_cast<Stmt *>(C);
      std::tuple<int, int> DRes = DT.GetDMap(SS);
      int DIndex = std::get<0>(DRes);
      if (DIndex != -1) {
        int DNum = std::get<1>(DRes);
        std::vector<Stmt *> DeclStmts = DT.GetDeclStmts();
        int size = DeclStmts.size();
        for (int i = DIndex; i < DIndex + DNum; i++) {
          if (i < size) Statements.push_back(DeclStmts[i]);
        }
      }
      SS = BaseTransform::TransformStmt(SS).getAs<Stmt>();
      Statements.push_back(SS);
    }
    Sema::CompoundScopeRAII CompoundScope(SemaRef);
    CompoundStmt *CS =
        BaseTransform::RebuildCompoundStmt(SourceLocation(), Statements,
                                           SourceLocation(), false)
            .getAs<CompoundStmt>();
    return CS;
  }

  StmtResult TransformCompoundStmt(CompoundStmt *S, bool IsStmtExpr) {
    return S;
  }
};
}  // namespace

namespace {
/// Look for Return Stmt
class ARFinder : public StmtVisitor<ARFinder> {
  Sema &SemaRef;
  Expr *PDRE;
  RecordDecl *FutureRD;
  BSCMethodDecl *FD;
  QualType ReturnTy;
  std::vector<Stmt *> ReturnStmts;
  const int UnitNum = 3;
  int ReturnNum;
  llvm::DenseMap<ReturnStmt *, int> ARMap;

 public:
  ARFinder(Sema &SemaRef, Expr *PDRE, RecordDecl *FutureRD, BSCMethodDecl *FD,
           QualType ReturnTy)
      : SemaRef(SemaRef),
        PDRE(PDRE),
        FutureRD(FutureRD),
        FD(FD),
        ReturnTy(ReturnTy) {
    ReturnNum = 0;
  }

  void VisitReturnStmt(ReturnStmt *S) {
    int ARSecond = ReturnStmts.size();
    SetARMap(S, ARSecond);
    ReturnNum++;
    ReturnStmt *RSS =
        ReturnStmt::Create(SemaRef.Context, SourceLocation(),
                           cast<ReturnStmt>(S)->getRetValue(), nullptr);
    const Expr *RetVal = RSS->getRetValue();
    Expr *RHSExpr = const_cast<Expr *>(RetVal);

    RecordDecl::field_iterator FutureStateField, TheFieldIt;
    for (TheFieldIt = FutureRD->field_begin();
         TheFieldIt != FutureRD->field_end(); ++TheFieldIt) {
      if (TheFieldIt->getDeclName().getAsString() == "__future_state") {
        FutureStateField = TheFieldIt;
        break;
      }
    }
    if (TheFieldIt != FutureRD->field_end()) {
      DeclarationName Name = FutureStateField->getDeclName();
      DeclarationNameInfo MemberNameInfo(Name, FutureStateField->getLocation());
      Expr *LHSExpr = SemaRef.BuildMemberExpr(
          PDRE, true, SourceLocation(), NestedNameSpecifierLoc(),
          SourceLocation(), *FutureStateField,
          DeclAccessPair::make(*FutureStateField,
                               FutureStateField->getAccess()),
          false, MemberNameInfo,
          FutureStateField->getType().getNonReferenceType(), VK_LValue,
          OK_Ordinary);

      llvm::APInt ResultVal(SemaRef.Context.getTargetInfo().getIntWidth(), 1);
      Expr *FutureStateVal = IntegerLiteral::Create(
          SemaRef.Context, ResultVal, SemaRef.Context.IntTy, SourceLocation());

      Expr *Unop =
          SemaRef
              .CreateBuiltinUnaryOp(SourceLocation(), UO_Minus, FutureStateVal)
              .get();
      Expr *BO =
          SemaRef.CreateBuiltinBinOp(SourceLocation(), BO_Assign, LHSExpr, Unop)
              .get();
      ReturnStmts.push_back(BO);
    }

    std::string ResReturnName = "__RES_RETURN";
    VarDecl *ResReturnVD =
        VarDecl::Create(SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                        &(SemaRef.Context.Idents).get(ResReturnName),
                        RHSExpr->getType(), nullptr, SC_None);

    ResReturnVD->setInit(RHSExpr);
    DeclGroupRef ResReturnDG(ResReturnVD);
    DeclStmt *ResReturnDS = new (SemaRef.Context)
        DeclStmt(ResReturnDG, SourceLocation(), SourceLocation());
    ReturnStmts.push_back(ResReturnDS);

    Expr *ResExpr = SemaRef.BuildDeclRefExpr(
        ResReturnVD, ResReturnVD->getType(), VK_LValue, SourceLocation());

    ResExpr = ImplicitCastExpr::Create(SemaRef.Context, ResReturnVD->getType(),
                                       CK_LValueToRValue, ResExpr, nullptr,
                                       VK_PRValue, FPOptionsOverride());

    SmallVector<Expr *, 1> Args;
    Args.push_back(ResExpr);

    QualType PollResultType = FD->getReturnType();
    RecordDecl *PollResultRD = PollResultType->getAsRecordDecl();

    std::string CompletedFuncName = "completed";
    LookupResult CompletedFDResult(
        SemaRef,
        DeclarationNameInfo(&(SemaRef.Context.Idents).get(CompletedFuncName),
                            SourceLocation()),
        SemaRef.LookupOrdinaryName);
    SemaRef.LookupQualifiedName(CompletedFDResult, PollResultRD);
    BSCMethodDecl *CompletedFD = nullptr;
    if (!CompletedFDResult.empty())
      CompletedFD =
          dyn_cast_or_null<BSCMethodDecl>(CompletedFDResult.getFoundDecl());

    Expr *CompletedRef = SemaRef.BuildDeclRefExpr(
        CompletedFD, CompletedFD->getType(), VK_LValue, SourceLocation());
    CompletedRef->HasBSCScopeSpec = true;
    CompletedRef =
        SemaRef
            .ImpCastExprToType(
                CompletedRef,
                SemaRef.Context.getPointerType(CompletedRef->getType()),
                CK_FunctionToPointerDecay)
            .get();

    Expr *CE = SemaRef
                   .BuildCallExpr(nullptr, CompletedRef, SourceLocation(), Args,
                                  SourceLocation())
                   .get();
    Stmt *RS = SemaRef.BuildReturnStmt(SourceLocation(), CE).get();
    ReturnStmts.push_back(RS);
    return;
  }

  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C) {
        Visit(C);
      }
    }
  }

  int GetReturnNum() { return ReturnNum; }

  std::vector<Stmt *> GetReturnStmts() { return ReturnStmts; }

  int GetUnitNum() { return UnitNum; }

  void SetARMap(ReturnStmt *RS, int index) {
    assert(RS && "Passed null Return");
    ARMap[RS] = index;
  }

  int GetARMap(ReturnStmt *RS) {
    llvm::DenseMap<ReturnStmt *, int>::iterator I = ARMap.find(RS);
    if (I != ARMap.end()) return I->second;
    return -1;
  }
};
}  // namespace

namespace {
class TransformARToCS : public TreeTransform<TransformARToCS> {
  typedef TreeTransform<TransformARToCS> BaseTransform;
  ARFinder ARInitlizer;
  int ReturnNum;
  int ReturnIndex;

 public:
  TransformARToCS(Sema &SemaRef, ARFinder ARInitlizer)
      : BaseTransform(SemaRef), ARInitlizer(ARInitlizer) {
    ReturnIndex = 0;
    ReturnNum = ARInitlizer.GetReturnNum();
  }

  // make sure redo semantic analysis
  bool AlwaysRebuild() { return true; }

  StmtResult TransformCompoundStmt(CompoundStmt *S) {
    if (S == nullptr) return S;

    int UnitNum = ARInitlizer.GetUnitNum();
    std::vector<Stmt *> ReturnStmts = ARInitlizer.GetReturnStmts();
    int StmtsSize = ReturnStmts.size();

    std::vector<Stmt *> Statements;
    for (auto *C : S->children()) {
      Stmt *SS = const_cast<Stmt *>(C);
      if (isa<ReturnStmt>(SS)) {
        int index = ARInitlizer.GetARMap(cast<ReturnStmt>(SS));
        if (index != -1) {
          for (int i = index; i < index + UnitNum; i++) {
            if (i < StmtsSize) Statements.push_back(ReturnStmts[i]);
          }
        }
        continue;
      }
      SS = BaseTransform::TransformStmt(SS).getAs<Stmt>();
      Statements.push_back(SS);
    }
    Sema::CompoundScopeRAII CompoundScope(SemaRef);
    CompoundStmt *CS =
        BaseTransform::RebuildCompoundStmt(SourceLocation(), Statements,
                                           SourceLocation(), false)
            .getAs<CompoundStmt>();
    return CS;
  }

  StmtResult TransformCompoundStmt(CompoundStmt *S, bool IsStmtExpr) {
    return S;
  }
};
}  // namespace

namespace {
class AEFinder : public StmtVisitor<AEFinder> {
  Sema &SemaRef;
  ParmVarDecl *PVD;
  Expr *PDRE;
  RecordDecl *FutureRD;
  BSCMethodDecl *FD;
  std::vector<Stmt *> AwaitStmts;
  const int UnitNum = 5;
  int AwaitCount = 0;
  llvm::DenseMap<AwaitExpr *, int> AEMap;
  llvm::DenseMap<AwaitExpr *, Expr *> AEReplaceMap;

 public:
  AEFinder(Sema &SemaRef, ParmVarDecl *PVD, Expr *PDRE, RecordDecl *FutureRD,
           BSCMethodDecl *FD)
      : SemaRef(SemaRef), PVD(PVD), PDRE(PDRE), FutureRD(FutureRD), FD(FD) {
    AwaitCount = 0;
  }

  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C) {
        Visit(C);
      }
    }
  }

  void VisitAwaitExpr(AwaitExpr *E) {
    Visit(E->getSubExpr());
    AwaitCount++;
    int AESecond = AwaitStmts.size();
    SetAEMap(E, AESecond);
    AwaitExpr *AtEr = E;
    auto *AE = AtEr->getSubExpr();
    RecordDecl::field_iterator FtField, FtFieldIt;
    for (FtFieldIt = FutureRD->field_begin();
         FtFieldIt != FutureRD->field_end(); ++FtFieldIt) {
      if (FtFieldIt->getDeclName().getAsString() ==
          "Ft_" + std::to_string(AwaitCount)) {
        FtField = FtFieldIt;
        break;
      }
    }

    Expr *LHSExpr = SemaRef.BuildMemberExpr(
        PDRE, true, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *FtField,
        DeclAccessPair::make(PVD, FtField->getAccess()), false,
        DeclarationNameInfo(), FtField->getType().getNonReferenceType(),
        VK_LValue, OK_Ordinary);
    Expr *RHSExpr = AE;
    // Handle nested call
    if (isa<CallExpr>(AE)) {
      CallExpr *CE = const_cast<CallExpr *>(cast<CallExpr>(AE));
      FunctionDecl *CFD = CE->getDirectCallee();
      std::vector<Expr *> CallArgs;
      for (unsigned I = 0; I < CE->getNumArgs(); ++I) {
        if (auto *KReplaceAE = dyn_cast<AwaitExpr>(CE->getArg(I))) {
          Expr *VReplaceAE = GetAEReplaceMap(KReplaceAE);
          if (VReplaceAE != nullptr) {
            CallArgs.push_back(VReplaceAE);
            continue;
          }
        }
        CallArgs.push_back(CE->getArg(I));
      }
      Expr *FDRef =
          SemaRef.BuildDeclRefExpr(CFD, CFD->getType().getNonReferenceType(),
                                   VK_LValue, CFD->getLocation());
      FDRef->HasBSCScopeSpec = true;
      FDRef = SemaRef
                  .ImpCastExprToType(
                      FDRef, SemaRef.Context.getPointerType(CFD->getType()),
                      CK_FunctionToPointerDecay)
                  .get();
      FDRef->HasBSCScopeSpec = true;
      RHSExpr = SemaRef
                    .BuildCallExpr(nullptr, FDRef, SourceLocation(), CallArgs,
                                   SourceLocation())
                    .get();
    }

    Expr *ResultStmt = SemaRef
                           .CreateBuiltinBinOp((*FtField)->getLocation(),
                                               BO_Assign, LHSExpr, RHSExpr)
                           .get();
    AwaitStmts.push_back(ResultStmt);
    RecordDecl *FatPointerRD =
        dyn_cast<RecordType>(
            LHSExpr->getType().getDesugaredType(SemaRef.Context))
            ->getDecl();
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
    Expr *VtableExpr = SemaRef.BuildMemberExpr(
        LHSExpr, false, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *VtableField,
        DeclAccessPair::make(FatPointerRD, VtableField->getAccess()), false,
        DeclarationNameInfo(), VtableField->getType(), VK_LValue, OK_Ordinary);
    VtableExpr = ImplicitCastExpr::Create(
        SemaRef.Context, VtableExpr->getType(), CK_LValueToRValue, VtableExpr,
        nullptr, VK_PRValue, FPOptionsOverride());

    RecordDecl::field_iterator PollFuncField, VtableFieldIt;
    const RecordType *RT = dyn_cast<RecordType>(
        VtableField->getType()->getPointeeType().getDesugaredType(
            SemaRef.Context));
    RecordDecl *VtableRD = RT->getDecl();
    for (VtableFieldIt = VtableRD->field_begin();
         VtableFieldIt != VtableRD->field_end(); ++VtableFieldIt) {
      if (VtableFieldIt->getDeclName().getAsString() == "poll") {
        PollFuncField = VtableFieldIt;
      }
    }
    const FunctionType *FT = dyn_cast<FunctionType>(
        PollFuncField->getType()->getPointeeType().getDesugaredType(
            SemaRef.Context));
    RecordDecl *PollResultRD = dyn_cast<RecordDecl>(
        dyn_cast<RecordType>(
            SemaRef.Context.getCanonicalType(FT->getReturnType()))
            ->getDecl());

    Expr *PollFuncExpr = SemaRef.BuildMemberExpr(
        VtableExpr, true, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *PollFuncField,
        DeclAccessPair::make(VtableRD, PollFuncField->getAccess()), false,
        DeclarationNameInfo(), PollFuncField->getType(), VK_LValue,
        OK_Ordinary);
    PollFuncExpr = ImplicitCastExpr::Create(
        SemaRef.Context, PollFuncExpr->getType(), CK_LValueToRValue,
        PollFuncExpr, nullptr, VK_PRValue, FPOptionsOverride());
    std::vector<Expr *> PollArgs;
    Expr *PtrExpr = SemaRef.BuildMemberExpr(
        LHSExpr, false, SourceLocation(), NestedNameSpecifierLoc(),
        SourceLocation(), *PtrField,
        DeclAccessPair::make(FatPointerRD, PtrField->getAccess()), false,
        DeclarationNameInfo(), PtrField->getType(), VK_LValue, OK_Ordinary);
    PtrExpr = ImplicitCastExpr::Create(SemaRef.Context, PtrExpr->getType(),
                                       CK_LValueToRValue, PtrExpr, nullptr,
                                       VK_PRValue, FPOptionsOverride());
    PollArgs.push_back(PtrExpr);
    Expr *PollFuncCall =
        SemaRef
            .BuildCallExpr(nullptr, PollFuncExpr, SourceLocation(), PollArgs,
                           SourceLocation())
            .get();

    RecordDecl::field_iterator ResField, FieldIt;
    for (FieldIt = PollResultRD->field_begin();
         FieldIt != PollResultRD->field_end(); ++FieldIt) {
      if (FieldIt->getDeclName().getAsString() == "res") {
        ResField = FieldIt;
        break;
      }
    }
    std::string AwaitResultVDName = "Res_" + std::to_string(AwaitCount);
    VarDecl *AwaitResultVD =
        VarDecl::Create(SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                        &(SemaRef.Context.Idents).get(AwaitResultVDName),
                        ResField->getType(), nullptr, SC_None);

    DeclGroupRef AwaitResultDG(AwaitResultVD);
    DeclStmt *AwaitResultDS = new (SemaRef.Context)
        DeclStmt(AwaitResultDG, SourceLocation(), SourceLocation());
    AwaitStmts.push_back(AwaitResultDS);

    std::string PollResultVDName = "PR_" + std::to_string(AwaitCount);
    VarDecl *PollResultVD =
        VarDecl::Create(SemaRef.Context, FD, SourceLocation(), SourceLocation(),
                        &(SemaRef.Context.Idents).get(PollResultVDName),
                        PollFuncCall->getType(), nullptr, SC_None);

    PollResultVD->setInit(PollFuncCall);
    DeclGroupRef PollResultDG(PollResultVD);
    DeclStmt *PollResultDS = new (SemaRef.Context)
        DeclStmt(PollResultDG, SourceLocation(), SourceLocation());
    AwaitStmts.push_back(PollResultDS);

    auto *If = ProcessAwaitExprStatus(SemaRef, AwaitCount, FutureRD, PDRE, PVD,
                                      PollResultVD, AwaitResultVD, FD);
    if (If != nullptr) AwaitStmts.push_back(If);

    Expr *AwaitResultRef = SemaRef.BuildDeclRefExpr(
        AwaitResultVD, AwaitResultVD->getType(), VK_LValue, SourceLocation());
    AwaitResultRef = ImplicitCastExpr::Create(
        SemaRef.Context, AwaitResultVD->getType(), CK_LValueToRValue,
        AwaitResultRef, nullptr, VK_PRValue, FPOptionsOverride());
    SetAEReplaceMap(E, AwaitResultRef);
    AwaitStmts.push_back(cast<Stmt>(AwaitResultRef));
  }

  int GetAwaitCount() { return AwaitCount; }

  std::vector<Stmt *> GetAwaitStmts() { return AwaitStmts; }

  int GetUnitNum() { return UnitNum; }

  void SetAEMap(AwaitExpr *AE, int index) {
    assert(AE && "Passed null AwaitExpr");
    AEMap[AE] = index;
  }

  int GetAEMap(AwaitExpr *AE) {
    llvm::DenseMap<AwaitExpr *, int>::iterator I = AEMap.find(AE);
    if (I != AEMap.end()) return I->second;
    return -1;
  }

  void SetAEReplaceMap(AwaitExpr *AE, Expr *AEReplace) {
    assert(AE && "Passed null AwaitExpr");
    AEReplaceMap[AE] = AEReplace;
  }

  Expr *GetAEReplaceMap(AwaitExpr *AE) {
    llvm::DenseMap<AwaitExpr *, Expr *>::iterator I = AEReplaceMap.find(AE);
    if (I != AEReplaceMap.end()) return cast<Expr>(I->second);
    return nullptr;
  }
};
}  // namespace

namespace {
class TransformAEToCS : public TreeTransform<TransformAEToCS> {
  typedef TreeTransform<TransformAEToCS> BaseTransform;
  AEFinder AEInitlizer;
  int AwaitNum;
  int AwaitIndex;
  std::vector<AwaitExpr *> AEStmts;
  std::vector<LabelDecl *> &LabelDecls;

 public:
  TransformAEToCS(Sema &SemaRef, AEFinder AEInitlizer,
                  std::vector<LabelDecl *> &LabelDecls)
      : BaseTransform(SemaRef),
        AEInitlizer(AEInitlizer),
        LabelDecls(LabelDecls) {
    AwaitIndex = 0;
    AwaitNum = AEInitlizer.GetAwaitCount();
  }

  // make sure redo semantic analysis
  bool AlwaysRebuild() { return true; }

  ExprResult TransformAwaitExpr(AwaitExpr *E) {
    return AEInitlizer.GetAEReplaceMap(E);
  }

  StmtResult TransformDeclStmt(DeclStmt *S) {
    SmallVector<Decl *, 4> Decls;
    for (auto *D : S->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        Expr *Init = const_cast<Expr *>(VD->getInit());
        bool HasAwait = HasAwaitExpr(Init);
        if (HasAwait && Init != nullptr) {
          Init = BaseTransform::TransformExpr(Init).get();
          SemaRef.AddInitializerToDecl(VD, Init, /*DirectInit=*/false);
        }
      }
      Decls.push_back(D);
    }
    return BaseTransform::RebuildDeclStmt(Decls, S->getBeginLoc(),
                                          S->getEndLoc());
  }

  StmtResult TransformReturnStmt(ReturnStmt *S) { return S; }

  StmtResult TransformCompoundStmt(CompoundStmt *S) {
    if (S == nullptr) return S;

    int UnitNum = AEInitlizer.GetUnitNum();
    std::vector<Stmt *> AwaitStmts = AEInitlizer.GetAwaitStmts();
    int StmtsSize = AwaitStmts.size();

    std::vector<Stmt *> Statements;
    for (auto *C : S->children()) {
      Stmt *SS = const_cast<Stmt *>(C);
      AEStmts.clear();
      GetAwaitExpr(SS);
      int AESize = AEStmts.size();
      if (AESize > 0) {
        int BaseIndex = Statements.size() - 1;
        for (int i = 0; i < AESize; i++) {
          int index = AEInitlizer.GetAEMap(AEStmts[i]);

          if (index == -1) continue;

          for (int j = index; j < index + UnitNum - 1; j++) {
            if (j == index + 1) {
              Stmt::EmptyShell Empty;
              Stmt *Null = new (SemaRef.Context) NullStmt(Empty);
              LabelStmt *LS =
                  BaseTransform::RebuildLabelStmt(
                      SourceLocation(),
                      cast<LabelDecl>(LabelDecls[AwaitIndex + i + 1]),
                      SourceLocation(), Null)
                      .getAs<LabelStmt>();
              Statements.push_back(LS);
              Statements.push_back(AwaitStmts[j]);
              continue;
            }

            if (j < StmtsSize) Statements.push_back(AwaitStmts[j]);
          }
        }

        if (isa<DeclStmt>(SS) && cast<DeclStmt>(SS)->isSingleDecl() &&
            BaseIndex >= 0) {
          VarDecl *VD = cast<VarDecl>(cast<DeclStmt>(SS)->getSingleDecl());
          if (VD->getNameAsString() == "__RES_RETURN") {
            Stmt *LastStmt = Statements[BaseIndex];
            Statements.erase(Statements.begin() + BaseIndex);
            Statements.push_back(LastStmt);
          }
        }

        AwaitIndex = AwaitIndex + AESize;
      }

      if (isa<AwaitExpr>(SS)) continue;

      SS = BaseTransform::TransformStmt(SS).getAs<Stmt>();
      Statements.push_back(SS);
    }
    Sema::CompoundScopeRAII CompoundScope(SemaRef);
    CompoundStmt *CS =
        BaseTransform::RebuildCompoundStmt(SourceLocation(), Statements,
                                           SourceLocation(), false)
            .getAs<CompoundStmt>();
    return CS;
  }

  StmtResult TransformCompoundStmt(CompoundStmt *S, bool IsStmtExpr) {
    return S;
  }

  void GetAwaitExpr(Stmt *S) {
    if (S == nullptr || isa<CompoundStmt>(S)) return;

    for (auto *C : S->children()) {
      GetAwaitExpr(C);
    }

    if (isa<AwaitExpr>(S)) {
      AEStmts.push_back(cast<AwaitExpr>(S));
    }
  }
};
}  // namespace

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

  BSCMethodDecl *NewFD = buildAsyncBSCMethodDecl(
      S.Context, RD, SLoc, NLoc, &(S.Context.Idents).get(FName), FuncType,
      nullptr, SC_None, RD->getTypeForDecl()->getCanonicalTypeInternal());
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

  BSCMethodDecl *NewFD = buildAsyncBSCMethodDecl(
      S.Context, RD, SLoc, NLoc, &(S.Context.Idents).get(FName), OriginType,
      nullptr, SC_None, RD->getTypeForDecl()->getCanonicalTypeInternal());
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

  FunctionDecl *TransformedFD =
      TransformToReturnVoid(S).TransformFunctionDecl(FD);

  NewFD->setType(
      S.Context.getFunctionType(TransformedFD->getReturnType(), ParamTys, {}));
  S.PushDeclContext(S.getCurScope(), NewFD);

  TransformToAP DT = TransformToAP(S, FutureObj, RD, NewFD);
  StmtResult MemberChangeRes = DT.TransformStmt(TransformedFD->getBody());
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
  Stmt::EmptyShell Empty;
  Stmt *Null = new (S.Context) NullStmt(Empty);
  int CurStmtSize = Stmts.size();
  if (CurStmtSize > StmtSize) {
    LabelStmt *LS =
        new (S.Context) LabelStmt(SourceLocation(), LabelDecls[0], Null);
    Stmts.insert(Stmts.begin() + StmtSize, LS);
  } else {
    LabelStmt *LS =
        new (S.Context) LabelStmt(SourceLocation(), LabelDecls[0], Null);
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

  QualType AwaitReturnTy = E->getType();
  bool IsCall = isa<CallExpr>(E);
  if (IsCall) {
    Decl *AwaitDecl = (dyn_cast<CallExpr>(E))->getCalleeDecl();
    FunctionDecl *FDecl = dyn_cast_or_null<FunctionDecl>(AwaitDecl);
    if (!FDecl) {
      return ExprError();
    }
    if (!FDecl->isAsyncSpecified() && !IsFutureType(AwaitReturnTy)) {
      Diag(E->getExprLoc(), PDiag(diag::err_not_a_async_call)
                                << getExprRange(E));
      return ExprError();
    }
  } else {
    if (!IsFutureType(AwaitReturnTy)) {
      Diag(E->getExprLoc(), PDiag(diag::err_not_a_async_call)
                                << getExprRange(E));
      return ExprError();
    }
  }

  // TODO: After we use future in stdlib, get template argument directly.
  if (IsFutureType(AwaitReturnTy)) {
    const RecordType *FatPointerType =
        dyn_cast<RecordType>(AwaitReturnTy.getDesugaredType(Context));
    RecordDecl *FatPointer = FatPointerType->getDecl();
    for (RecordDecl::field_iterator FieldIt = FatPointer->field_begin(),
                                    Field_end = FatPointer->field_end();
         FieldIt != Field_end; ++FieldIt) {
      if (FieldIt->getDeclName().getAsString() == "vtable") {
        const RecordType *VtableType = dyn_cast<RecordType>(
            FieldIt->getType()->getPointeeType().getDesugaredType(Context));
        RecordDecl *Vtable = VtableType->getDecl();
        for (RecordDecl::field_iterator FieldIt = Vtable->field_begin(),
                                        Field_end = Vtable->field_end();
             FieldIt != Field_end; ++FieldIt) {
          if (FieldIt->getDeclName().getAsString() == "poll") {
            const RecordType *PollResultType = dyn_cast<RecordType>(
                dyn_cast<FunctionType>(
                    FieldIt->getType()->getPointeeType().getDesugaredType(
                        Context))
                    ->getReturnType()
                    .getDesugaredType(Context));
            RecordDecl *PollResult = PollResultType->getDecl();
            for (RecordDecl::field_iterator FieldIt = PollResult->field_begin(),
                                            Field_end = PollResult->field_end();
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
  AwaitExpr *Res = new (Context) AwaitExpr(AwaitLoc, E, AwaitReturnTy);
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

SmallVector<Decl *, 8> Sema::ActOnAsyncFunctionDeclaration(FunctionDecl *FD) {
  SmallVector<Decl *, 8> decls;
  if (!IsFutureType(FD->getReturnType())) {
    QualType ReturnTy = FD->getReturnType();
    ReturnTy.removeLocalConst();
    if (ReturnTy->isVoidType()) {
      RecordDecl *VoidRD =
          generateVoidStruct(*this, FD->getBeginLoc(), FD->getEndLoc());
      decls.push_back(VoidRD);
      Context.BSCDesugaredMap[FD].push_back(VoidRD);
      ReturnTy = Context.getRecordType(VoidRD);
    }
    QualType PollResultType =
        lookupPollResultType(*this, FD->getBeginLoc(), ReturnTy);
    if (PollResultType.isNull()) {
      return decls;
    }
    RecordDecl *PollResultRD = PollResultType->getAsRecordDecl();
    std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
        NewRDs = generateVtableAndFatPointerStruct(*this, ReturnTy,
                                                   PollResultRD, FD);

    if (!std::get<1>(std::get<0>(NewRDs))) {
      decls.push_back(std::get<0>(std::get<0>(NewRDs)));
      Context.BSCDesugaredMap[FD].push_back(std::get<0>(std::get<0>(NewRDs)));
    }
    if (!std::get<1>(std::get<1>(NewRDs))) {
      decls.push_back(std::get<0>(std::get<1>(NewRDs)));
      Context.BSCDesugaredMap[FD].push_back(std::get<0>(std::get<1>(NewRDs)));
    }

    FunctionDecl *FutureInitDef = buildFutureInitFunctionDeclaration(
        *this, FD, std::get<0>(std::get<1>(NewRDs)));
    decls.push_back(FutureInitDef);
    Context.BSCDesugaredMap[FD].push_back(FutureInitDef);
  }
  return decls;
}

SmallVector<Decl *, 8> Sema::ActOnAsyncFunctionDefinition(FunctionDecl *FD) {
  SmallVector<Decl *, 8> decls;
  decls.push_back(FD);

  AwaitExprFinder finder = AwaitExprFinder(Context);
  finder.Visit(FD->getBody());

  // Report if await expression appear in non-async functions.
  if (!FD->isAsyncSpecified()) {
    if (finder.GetAwaitExprNum() != 0) {
      Diag(FD->getBeginLoc(), diag::err_await_invalid_scope)
          << "non-async function.";
    }
    return decls;
  }

  // For leaf nodes, should not be modified async.
  if (IsFutureType(FD->getReturnType())) {
    Diag(FD->getBeginLoc(), diag::err_invalid_async_function);
    return decls;
  }

  // Do not process desugar if we already met errors.
  if (Diags.hasErrorOccurred()) {
    return decls;
  }

  IllegalAEFinder IAEFinder = IllegalAEFinder(*this);
  IAEFinder.Visit(FD->getBody());
  if (IAEFinder.hasIllegalAwaitExpr()) return decls;

  QualType ReturnTy = FD->getReturnType();
  ReturnTy.removeLocalConst();
  if (ReturnTy->isVoidType()) {
    RecordDecl *VoidRD =
        generateVoidStruct(*this, FD->getBeginLoc(), FD->getEndLoc());
    decls.push_back(VoidRD);
    Context.BSCDesugaredMap[FD].push_back(VoidRD);
    ReturnTy = Context.getRecordType(VoidRD);
  }
  QualType PollResultType =
      lookupPollResultType(*this, FD->getBeginLoc(), ReturnTy);
  if (PollResultType.isNull()) {
    return decls;
  }
  RecordDecl *PollResultRD = PollResultType->getAsRecordDecl();

  std::tuple<std::pair<RecordDecl *, bool>, std::pair<RecordDecl *, bool>>
      NewRDs =
          generateVtableAndFatPointerStruct(*this, ReturnTy, PollResultRD, FD);

  if (!std::get<1>(std::get<0>(NewRDs))) {
    decls.push_back(std::get<0>(std::get<0>(NewRDs)));
    Context.BSCDesugaredMap[FD].push_back(std::get<0>(std::get<0>(NewRDs)));
  }

  if (!std::get<1>(std::get<1>(NewRDs))) {
    decls.push_back(std::get<0>(std::get<1>(NewRDs)));
    Context.BSCDesugaredMap[FD].push_back(std::get<0>(std::get<1>(NewRDs)));
  }

  // Handle declaration first.
  FunctionDecl *FutureInitDef = buildFutureInitFunctionDeclaration(
      *this, FD, std::get<0>(std::get<1>(NewRDs)));

  const int FutureStateNumber = finder.GetAwaitExprNum() + 1;

  RecordDecl *RD = buildFutureRecordDecl(*this, FD, finder.GetAwaitExpr(),
                                         finder.GetLocalVarList());
  if (!RD) {
    return decls;
  }
  decls.push_back(RD);
  Context.BSCDesugaredMap[FD].push_back(RD);

  BSCMethodDecl *PollDecl =
      buildPollFunction(*this, RD, PollResultRD, FD,
                        std::get<0>(std::get<1>(NewRDs)), FutureStateNumber);
  if (!PollDecl) {
    return decls;
  }
  decls.push_back(PollDecl);
  Context.BSCDesugaredMap[FD].push_back(PollDecl);

  BSCMethodDecl *FreeDecl = buildFreeFunction(*this, RD, FD);
  if (!FreeDecl) {
    return decls;
  }
  decls.push_back(FreeDecl);
  Context.BSCDesugaredMap[FD].push_back(FreeDecl);

  VarDecl *VtableDecl =
      buildVtableInitDecl(*this, FD, std::get<0>(std::get<0>(NewRDs)),
                          PollResultRD, PollDecl, FreeDecl);
  if (!VtableDecl) {
    return decls;
  }
  decls.push_back(VtableDecl);
  Context.BSCDesugaredMap[FD].push_back(VtableDecl);

  FunctionDecl *FutureInit = buildFutureInitFunctionDefinition(
      *this, RD, FD, std::get<0>(std::get<1>(NewRDs)), VtableDecl,
      FutureInitDef);
  if (!FutureInit) {
    return decls;
  }
  decls.push_back(FutureInit);
  Context.BSCDesugaredMap[FD].push_back(FutureInit);
  return decls;
}