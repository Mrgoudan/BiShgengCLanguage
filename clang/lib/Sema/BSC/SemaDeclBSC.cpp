//===--- SemaDeclBSC.cpp - Semantic Analysis for Declarations
//------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for statements.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/BSC/WalkerBSC.h"
#include "clang/Analysis/Analyses/BSCBorrowCheck.h"
#include "clang/Analysis/Analyses/BSCNullabilityCheck.h"
#include "clang/Analysis/Analyses/BSCOwnership.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaInternal.h"

using namespace clang;
using namespace sema;

void Sema::CheckBSCConstexprFunction(FunctionDecl* FD) {
  assert(getLangOpts().BSC && FD->isConstexprSpecified());
  // BSC constexpr function can not be async.
  if (FD->isAsyncSpecified()) {
    Diag(FD->getBeginLoc(), diag::err_async_func_unsupported)
        << "constexpr";
  }
  // BSC constexpr function can not be variadic.
  if (FD->isVariadic()) {
    Diag(FD->getBeginLoc(), diag::err_constexpr_func_unsupported)
        << "variadic";
  }
  // The return type and parameter type of BSC constexpr function should be compile_time_calculated type.
  QualType RT = FD->getReturnType();
  if (!RT->isDependentType() && !RT->isBSCCalculatedTypeInCompileTime()) {
    Diag(FD->getBeginLoc(), diag::err_constexpr_func_unsupported_type) << RT;
  }
  for (ParmVarDecl* PVD: FD->parameters()) {
    QualType PT = PVD->getType();
    if (!PT->isDependentType() && !PT->isBSCCalculatedTypeInCompileTime()) {
      Diag(PVD->getLocation(), diag::err_constexpr_func_unsupported_type) << PT;
    }
  }
}

// The type of BSC constexpr variable should be compile_time_calculated type.
void Sema::CheckBSCConstexprVarType(VarDecl* VD) {
  assert(getLangOpts().BSC);
  QualType T = VD->getType();
  if (T->isDependentType())
    return;
  if (VD->isConstexpr() && !T->isBSCCalculatedTypeInCompileTime()) {
    Diag(VD->getLocation(), diag::err_constexpr_var_unsupported_type) << T;
    VD->setInvalidDecl();
    return;
  }
  if (FunctionDecl* FD = dyn_cast_or_null<FunctionDecl>(VD->getDeclContext())) {
    if (FD->isConstexpr() && !T->isBSCCalculatedTypeInCompileTime()) {
      Diag(VD->getLocation(), diag::err_constexpr_func_unsupported_type) << VD->getType();
      VD->setInvalidDecl();
      return;
    }
  }
}

bool HasDiffBorrorOrOwnedQualifiers(QualType LHSType, QualType RHSType) {
  if (LHSType.isOwnedQualified() != RHSType.isOwnedQualified()) {
    return true;
  }
  if (LHSType.isBorrowQualified() != RHSType.isBorrowQualified()) {
    return true;
  }
  if (LHSType->isPointerType() && RHSType->isPointerType()) {
    QualType LHSPType = LHSType->getPointeeType();
    QualType RHSPType = RHSType->getPointeeType();
    return HasDiffBorrorOrOwnedQualifiers(LHSPType, RHSPType);
  }
  return false;
}

bool Sema::HasDiffBorrowOrOwnedParamsTypeAtBothFunction(QualType LHS,
                                                        QualType RHS) {
  const FunctionProtoType *LSHFuncType = LHS->getAs<FunctionProtoType>();
  const FunctionProtoType *RSHFuncType = RHS->getAs<FunctionProtoType>();
  if (!LSHFuncType || !RSHFuncType) {
    return false;
  }

  QualType LHSRetType = LSHFuncType->getReturnType();
  QualType RHSRetType = RSHFuncType->getReturnType();
  if (HasDiffBorrorOrOwnedQualifiers(LHSRetType, RHSRetType)) {
    return true;
  }
  if (LSHFuncType->getNumParams() != RSHFuncType->getNumParams()) {
    return true;
  }
  for (unsigned i = 0; i < LSHFuncType->getNumParams(); i++) {
    QualType LHSParType = LSHFuncType->getParamType(i).getUnqualifiedType();
    QualType RHSParType = RSHFuncType->getParamType(i).getUnqualifiedType();
    if (HasDiffBorrorOrOwnedQualifiers(LHSParType, RHSParType)) {
      return true;
    }
  }
  return false;
}

bool Sema::CheckOperatorDeclNeedAddToContext(Declarator &D) {
  for (ParsedAttr &AL : D.getDeclSpec().getAttributes()) {
    if (AL.getKind() == ParsedAttr::AT_Operator) {
      return AL.getOperatorTypeBuffer().AddToContext;
    }
  }
  return true;
}

bool Sema::CheckOperatorFunReturnTypeIsLegal(FunctionDecl *FnDecl) {
  OverloadedOperatorKind Op = FnDecl->getOverloadedOperator();
  DefaultedComparisonKind DCK = DefaultedComparisonKind::None;
  switch (Op) {
  case OO_Less:
  case OO_Greater:
  case OO_LessEqual:
  case OO_GreaterEqual:
    DCK = DefaultedComparisonKind::Relational;
    break;
  case OO_EqualEqual:
    DCK = DefaultedComparisonKind::Equal;
    break;
  case OO_ExclaimEqual:
    DCK = DefaultedComparisonKind::Equal;
    break;

  default:
    break;
  }
  if (DCK != DefaultedComparisonKind::None &&
      !FnDecl->getReturnType()->isBooleanType()) {
    Diag(FnDecl->getLocation(),
         diag::err_defaulted_comparison_return_type_not_bool)
        << (int)DCK << FnDecl->getDeclaredReturnType() << Context.BoolTy
        << FnDecl->getReturnTypeSourceRange();
    return false;
  }
  return true;
}

// Return true if any memory safe features are found in a FunctionDecl.
bool Sema::FindSafeFeatures(const FunctionDecl* FnDecl) {
  if (!FnDecl) return false;
  BSCFeatureFinder finder = BSCFeatureFinder(Context);
  if (finder.Visit(const_cast<FunctionDecl*>(FnDecl))) {
    return true;
  }
  return false;
}

bool Sema::HasSafeZoneInCompoundStmt(const CompoundStmt* CompStmt) {
  if (!CompStmt) {
    return false;
  }
  for (const Stmt *child: CompStmt->children()) {
    if (auto *CompChild = dyn_cast<CompoundStmt>(child)) {
      if (CompChild->getCompSafeZoneSpecifier() == SZ_Safe) {
        return true;
      }
      if (HasSafeZoneInCompoundStmt(CompChild)) {
        return true;
      }
    }
  }
  return false;
}

bool Sema::HasSafeZoneInFunction(const FunctionDecl* FnDecl) {
  if (!FnDecl || !FnDecl->getBody()) {
    return false;
  }
  if (FnDecl->getSafeZoneSpecifier() == SZ_Safe) {
    return true;
  }
  CompoundStmt *FuncBody = cast<CompoundStmt>(FnDecl->getBody());
  if (!FuncBody) {
    return false;
  }
  return HasSafeZoneInCompoundStmt(FuncBody);
}

void Sema::BSCDataflowAnalysis(const Decl *D, bool EnableOwnershipCheck,
                               bool EnableNullabilityCheck) {
  AnalysisDeclContext AC(/* AnalysisDeclContextManager */ nullptr, D);

  AC.getCFGBuildOptions().PruneTriviallyFalseEdges = true;
  AC.getCFGBuildOptions().AddEHEdges = false;
  AC.getCFGBuildOptions().AddInitializers = true;
  AC.getCFGBuildOptions().AddImplicitDtors = true;
  AC.getCFGBuildOptions().AddTemporaryDtors = true;
  AC.getCFGBuildOptions().AddScopes = true;
  AC.getCFGBuildOptions().AddAllScopes = true;
  AC.getCFGBuildOptions().setAllAlwaysAdd();

  if (AC.getCFG()) {
    const FunctionDecl *FD = cast<FunctionDecl>(D);
    unsigned NumNullabilityCheckErrorsInCurrFD = 0;
    LangOptions::NullCheckZone CheckZone = getLangOpts().getNullabilityCheck();
    if (EnableNullabilityCheck && (CheckZone == LangOptions::NC_ALL || HasSafeZoneInFunction(FD))) {
      NullabilityCheckDiagReporter NullabilityCheckReporter(*this);
      runNullabilityCheck(*FD, *AC.getCFG(), AC, NullabilityCheckReporter,
                          Context);
      NullabilityCheckReporter.flushDiagnostics();
      NumNullabilityCheckErrorsInCurrFD =
          NullabilityCheckReporter.getNumErrors();
    }
    // Run ownership analysis when there is no nullability errors in current
    // function.
    if (EnableOwnershipCheck && !NumNullabilityCheckErrorsInCurrFD) {
      OwnershipDiagReporter OwnershipReporter(*this);
      runOwnershipAnalysis(*FD, *AC.getCFG(), AC, OwnershipReporter, Context);
      OwnershipReporter.flushDiagnostics();
      // Run borrow check when there is no other ownership errors in current
      // function.
      if (!OwnershipReporter.getNumErrors()) {
        BorrowCheckDiagReporter BorrowCheckReporter(*this);
        runBorrowCheck(*FD, *AC.getCFG(), BorrowCheckReporter, Context);
      }
    }
  }
}
#endif