//===--- SemaBSCOwnership.cpp - Semantic Analysis for BSC Ownership
//----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements dataflow analysis for BSC Ownership.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/Type.h"
#include "clang/Basic/Builtins.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"

using namespace clang;
using namespace sema;

// for union fields/array/global variable type check
void Sema::CheckOwnedOrIndirectOwnedType(SourceLocation ErrLoc, QualType T, StringRef Env) {
  enum {
    ownedQualified,
    ownedTypedef,
    ownedFields
  };
  if (T.getCanonicalType().isOwnedQualified() && !T.getTypePtr()->getAs<TypedefType>()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << ownedQualified << "_Owned" << Env;
  } else if (T.getCanonicalType().isOwnedQualified() && T.getTypePtr()->getAs<TypedefType>()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << ownedTypedef << "_Owned" << Env << T;
  } else if (T.getCanonicalType().getTypePtr()->isMoveSemanticType()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << ownedFields << "_Owned" << Env << T;
  }
}

// Check that owned qualifiers on an instantiated type are valid.
// This is called after template instantiation to validate that template
// parameters which were qualified with 'owned' instantiate to pointer types
// (or other valid owned types).
//
// For example:
//   template<typename T> void f(owned T t);
//   f<int>(x);  // Error: 'int' cannot be qualified by 'owned'
//   f<int*>(x); // OK: 'int*' can be qualified by 'owned'
//
// Returns true if the type is valid, false if an error was reported.
bool Sema::CheckInstantiatedTypeOwnedQualifiers(QualType T, SourceLocation Loc) {
  if (!getLangOpts().BSC)
    return true;

  // Helper to check if a type can validly have owned qualifier
  auto isValidOwnedType = [](QualType Ty) {
    return Ty->isPointerType() || Ty->isOwnedStructureType() ||
           Ty->isOwnedTemplateSpecializationType();
  };

  // Check owned qualifier
  if (T.isOwnedQualified() && !isValidOwnedType(T)) {
    QualType UnqualType = T;
    UnqualType.removeLocalFastQualifiers(Qualifiers::Owned);
    Diag(Loc, diag::err_owned_qualifier_non_pointer) << "_Owned" << UnqualType;
    return false;
  }

  return true;
}

// Check if 'owned' qualifier is applied to a non-pointer type
// The 'owned' qualifier is only valid on:
//   - Pointer types (e.g., int* owned, int** owned *)
//   - Owned structure types
//   - Owned template specialization types
//   - Dependent types (template parameters - checked at instantiation)
// Invalid examples:
//   - owned int x           (owned on primitive type)
//   - owned int * a[3]      (owned int inside array of pointers)
void Sema::CheckOwnedQualifierOnNonPointerType(const DeclSpec &DS, QualType T) {
  // Early exit if feature is disabled or owned not explicitly specified
  if (!getLangOpts().BSC || !DS.getOwnedSpecLoc().isValid())
    return;

  // Helper to check if a type can validly have owned qualifier
  // Returns true for pointers, owned structures, owned template types, and dependent types
  // Dependent types (like template parameters) are allowed - validity checked at instantiation
  auto isValidOwnedType = [](QualType Ty) {
    return Ty->isPointerType() || Ty->isOwnedStructureType() ||
           Ty->isOwnedTemplateSpecializationType() || Ty->isDependentType();
  };

  // Helper to emit diagnostic with unqualified type
  // Removes 'owned' qualifier before displaying the type in error message
  auto emitDiagnostic = [&](QualType Ty) {
    QualType UnqualType = Ty;
    UnqualType.removeLocalFastQualifiers(Qualifiers::Owned);
    Diag(DS.getOwnedSpecLoc(), diag::err_owned_qualifier_non_pointer)
        << "_Owned" << UnqualType;
  };

  // First Check: Deep type analysis
  // Strip all pointer and array levels to reach the innermost base type
  // Example: owned int **p → strip 2 levels → owned int (base type)
  QualType BaseType = T;
  while (const auto *PT = BaseType->getAs<PointerType>())
    BaseType = PT->getPointeeType();
  if (const auto *AT = BaseType->getAsArrayTypeUnsafe())
    BaseType = AT->getElementType();

  // Check if the innermost base type incorrectly has 'owned' on a non-pointer
  // This catches: owned int **p (where 'owned int' is at the base)
  if (BaseType.isOwnedQualified() && !isValidOwnedType(BaseType)) {
    emitDiagnostic(BaseType);
    return;
  }

  // Second Check: Top-level type analysis
  // Handle cases where the complete type declaration is invalid
  // Example: owned int x, or owned int * a[3]
  if (!isValidOwnedType(T)) {
    QualType CheckType = T;
    // Strip at most one array level, then one pointer level
    // For "owned int * a[3]": strip array → owned int*, then strip pointer → owned int
    if (const auto *AT = CheckType->getAsArrayTypeUnsafe())
      CheckType = AT->getElementType();
    if (const auto *PT = CheckType->getAs<PointerType>())
      CheckType = PT->getPointeeType();

    // If we found 'owned' after stripping, it's invalid
    if (CheckType.isOwnedQualified())
      emitDiagnostic(CheckType);
  }
}

namespace {
bool IsOwnedRawPointerCastDisallowed(QualType LHSCanType, QualType RHSCanType) {
  const auto *LHSPtrType = LHSCanType->getAs<PointerType>();
  const auto *RHSPtrType = RHSCanType->getAs<PointerType>();
  if (!LHSPtrType || !RHSPtrType)
    return false;

  bool LHSOwned = LHSCanType.isOwnedQualified();
  bool RHSOwned = RHSCanType.isOwnedQualified();
  bool LHSRaw = !LHSOwned && !LHSCanType.isBorrowQualified();
  bool RHSRaw = !RHSOwned && !RHSCanType.isBorrowQualified();
  if ((LHSOwned && RHSRaw) || (RHSOwned && LHSRaw))
    return true;

  return IsOwnedRawPointerCastDisallowed(LHSPtrType->getPointeeType(),
                                         RHSPtrType->getPointeeType());
}
} // end anonymous namespace

bool Sema::CheckOwnedQualTypeCStyleCast(QualType LHSType, QualType RHSType) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  if ((LHSCanType.isBorrowQualified() && LHSCanType->isPointerType() &&
       RHSCanType.isOwnedQualified() && RHSCanType->isPointerType()) ||
      (RHSCanType.isBorrowQualified() && RHSCanType->isPointerType() &&
       LHSCanType.isOwnedQualified() && LHSCanType->isPointerType())) {
    return false;
  }

  // Allow owned pointer to be cast from nullptr_t
  if (LHSCanType.isOwnedQualified() && LHSCanType->isPointerType() &&
      RHSCanType->isNullPtrType()) {
    return true;
  }

  if (IsOwnedRawPointerCastDisallowed(LHSCanType, RHSCanType))
    return false;

  bool IsSameType = (LHSCanType.getTypePtr() == RHSCanType.getTypePtr());
  const auto *LHSPtrType = LHSType->getAs<PointerType>();
  const auto *RHSPtrType = RHSType->getAs<PointerType>();
  bool IsPointer = LHSPtrType && RHSPtrType;

  if (RHSCanType->isDependentType()) {
    return true;
  }

  // legal cases:
  // 'int owned'    <->  'int owned'                      // same type same owned
  // 'int'          <->  'owned int'                      // same type diff owned
  // 'owned int*'   <->  'int*'                           // pointer to 'same type diff owned'
  // 'int**'        <->  'owned int* const owned * owned' // multi-layer pointer same type diff owned or const
  // 'int**'        <->  'int* const * owned'             // multi-layer pointer same type diff owned or const
  // 'float* owned' <->  'void* owned'                    // pointer to diff type but voidpointer same owned
  // 'float* owned' <->  'void*'                          // diff type but voidpointer diff owned
  // 'int** owned'' <->  const int** owned'
  // <integral type><--  'T* owned' // owned pointer can be cast to integral type, but not the opposite

  // illegal cases:
  // 'float owned'  <->  'int owned'    // diff type
  // 'float* owned' <->  'int* owned'   // diff type same owned
  // 'owned float*' <->  'owned int*'   // pointer to diff type same owned
  // 'int *owned'   <->  'int *borrow'  // should use borrow operators instead

  if (IsPointer) {
    // Disallow conversion between owned and borrow pointers
    if ((LHSCanType.isBorrowQualified() && RHSCanType.isOwnedQualified()) ||
        (LHSCanType.isOwnedQualified() && RHSCanType.isBorrowQualified())) {
      return false;
    }
    // Allow owned pointer to be cast from nullptr_t
    if (LHSCanType.isOwnedQualified() && RHSCanType->isNullPtrType()) {
      return true;
    }
    return LHSCanType.getTypePtr()->isVoidPointerType() ||
           RHSCanType.getTypePtr()->isVoidPointerType() ||
           CheckOwnedQualTypeCStyleCast(LHSPtrType->getPointeeType(),
                                        RHSPtrType->getPointeeType());
  }
  if (LHSCanType->isIntegerType() && RHSCanType.isOwnedQualified() &&
      RHSCanType->isPointerType()) {
    return true;
  }
  return IsSameType;
}

bool Sema::CheckOwnedQualTypeCStyleCast(QualType LHSType, QualType RHSType, SourceLocation RLoc) {
  if (!CheckOwnedQualTypeCStyleCast(LHSType, RHSType)) {
    QualType RHSCanType = RHSType.getCanonicalType();
    QualType LHSCanType = LHSType.getCanonicalType();
    if (IsOwnedRawPointerCastDisallowed(LHSCanType, RHSCanType))
      Diag(RLoc, diag::err_owned_raw_cast_disallowed);
    else
      Diag(RLoc, diag::err_owned_qualcheck_incompatible) << RHSType << LHSType;
    return false;
  } else {
    return true;
  }
}

bool Sema::CheckOwnedQualTypeAssignment(QualType LHSType, QualType RHSType, SourceLocation RLoc) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  const auto *LHSPtrType = LHSType->getAs<PointerType>();
  const auto *RHSPtrType = RHSType->getAs<PointerType>();
  bool IsPointer = LHSPtrType && RHSPtrType;
  bool IsSameType = (LHSCanType.getTypePtr() == RHSCanType.getTypePtr());
  bool IsTraitImplType = (LHSCanType->isTraitType() || RHSCanType->isTraitType());

  // _Bool <- T *_Owned // legal, doesn't consume ownership
  if (RHSPtrType && RHSCanType.isOwnedQualified() &&
      LHSCanType->isBooleanType()) {
    return true;
  }

  // owned to owned cases:
  // int* owned  <->  int* owned   // legal
  // int* owned  <->  float* owned // illegal
  // const int** owned  <->  int** owned  // legal
  // trait T* owned <- [type : trait T] * owned // legal
  // unOwned to unOwned cases:
  // owned int* owned *  <->  owned int**  // illegal
  // owned int* const *  <->  owned int**  // legal
  if (LHSCanType.isOwnedQualified() == RHSCanType.isOwnedQualified() ||
      (LHSCanType->isTraitType() && RHSCanType->isOwnedStructureType())) {
    if (IsSameType) {
      return true;
    }
    if (IsTraitImplType) {
      return true;
    }
    if (!IsPointer) {
      return false;
    } else {
      // owned struct S* <-> void* //legal
      if (!LHSCanType.isOwnedQualified() && (LHSPtrType->isVoidPointerType() || RHSPtrType->isVoidPointerType()))
        return true;
      return CheckOwnedQualTypeAssignment(LHSPtrType->getPointeeType(), RHSPtrType->getPointeeType(), RLoc);
    }
  }

  // trait T* owned <-> trait T* owned // legal
  if (LHSCanType.isOwnedQualified() || RHSCanType.isOwnedQualified()) {
    TraitDecl *TD = TryDesugarTrait(LHSType);
    if (TD) {
      QualType QT = DesugarTraitToStructTrait(TD, LHSCanType, RLoc);
      if (QT.getCanonicalType() == RHSCanType) {
        return true;
      }
    }
  }

  // unOwned <-> owned
  // int* owned <-> int*  //illegal
  return false;
}

bool Sema::CheckOwnedQualTypeAssignment(QualType LHSType, Expr* RHSExpr) {
  QualType RHSCanType = RHSExpr->getType().getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  bool IsLiteral = false;
  Stmt::StmtClass RHSClass = RHSExpr->getStmtClass();
  if (RHSClass == Expr::IntegerLiteralClass
      || RHSClass == Expr::FloatingLiteralClass
      || RHSClass == Expr::CharacterLiteralClass) {
    IsLiteral = true;
  }
  SourceLocation ExprLoc = RHSExpr->getBeginLoc();
  // Owned pointer can be inited by nullptr.
  if (LHSCanType.isOwnedQualified() && LHSCanType->isPointerType() &&
      isa<CXXNullPtrLiteralExpr>(RHSExpr->IgnoreParens()))
    return true;

  bool Res = true;

  // unOwned to owned initialize cases:
  // int owned a = 10;        //legal even 10 is not owned type
  // int owned b = 10 + 10;   //ilegal
  // char owned c = 'c';      // legal even 'c' is int type
  // int owned d = (int)a;    // illegal
  if (LHSCanType.isOwnedQualified() && !RHSCanType.isOwnedQualified() && IsLiteral) {
    if (LHSCanType.getTypePtr() != RHSCanType.getTypePtr()
        && !(LHSCanType.getTypePtr()->isCharType() && RHSCanType.getTypePtr()->isIntegerType())) {
      Res = false;
    }
  } else {
    Res = CheckOwnedQualTypeAssignment(LHSType, RHSCanType, ExprLoc);
  }
  if (!Res) {
    Diag(ExprLoc, diag::err_owned_qualcheck_incompatible) << RHSExpr->getType() << LHSType;
  }
  return Res;
}

bool Sema::CheckOwnedFunctionPointerType(QualType LHSType, Expr* RHSExpr) {
  const FunctionProtoType* LSHFuncType = LHSType->getAs<PointerType>()->getPointeeType()->getAs<FunctionProtoType>();
  const FunctionProtoType* RSHFuncType = RHSExpr->getType()->isFunctionPointerType()?
    RHSExpr->getType()->getAs<PointerType>()->getPointeeType()->getAs<FunctionProtoType>():
    RHSExpr->getType()->getAs<FunctionProtoType>();
  SourceLocation ExprLoc = RHSExpr->getBeginLoc();

  // For heterogeneous redeclarations, select the best matching declaration.
  if (FunctionDecl *SelectedFD =
          SelectFunctionDeclForPointerAssignment(RHSExpr, LSHFuncType))
    RSHFuncType = SelectedFD->getType()->getAs<FunctionProtoType>();

  // return if no 'owned' in both side
  if (!LSHFuncType->hasOwnedRetOrParams() && !RSHFuncType->hasOwnedRetOrParams()) {
    return true;
  }
  if ((LSHFuncType->getReturnType().isOwnedQualified() && !RSHFuncType->getReturnType().isOwnedQualified())
       || (!LSHFuncType->getReturnType().isOwnedQualified() && RSHFuncType->getReturnType().isOwnedQualified())) {
    Diag(ExprLoc, diag::err_owned_funcPtr_incompatible) << LHSType << RHSExpr->getType();
    return false;
  }
  if (LSHFuncType->getNumParams() != RSHFuncType->getNumParams()) {
    Diag(ExprLoc, diag::err_owned_funcPtr_incompatible) << LHSType << RHSExpr->getType();
    return false;
  }
  for (unsigned i = 0; i < LSHFuncType->getNumParams(); i++) {
    if ((LSHFuncType->getParamType(i).isOwnedQualified() && !RSHFuncType->getParamType(i).isOwnedQualified())
         || (!LSHFuncType->getParamType(i).isOwnedQualified() && RSHFuncType->getParamType(i).isOwnedQualified())) {
      Diag(ExprLoc, diag::err_owned_funcPtr_incompatible) << LHSType << RHSExpr->getType();
      return false;
    }
  }
  return true;
}

bool Sema::CheckTemporaryVarMemoryLeak(Expr* E) {
  if (!dyn_cast<CallExpr>(E)) return false;
  QualType RetType = E->getType().getCanonicalType();
  if (RetType.isOwnedQualified() || RetType->isMoveSemanticType()) {
    std::string ExprString;
    llvm::raw_string_ostream ExprStream(ExprString);
    E->printPretty(ExprStream, nullptr, clang::PrintingPolicy(getLangOpts()));
    Diag(E->getBeginLoc(), diag::err_owned_temporary_memLeak) << ExprStream.str();
    return true;
  }
  return false;
}

void Sema::CheckMoveVarMemoryLeak(Expr* E, SourceLocation SL) {
  if (E == nullptr)
    return;
  if (auto UO = dyn_cast_or_null<UnaryOperator>(E->IgnoreParenCasts())) {
    if (UO->getOpcode() == UO_Deref && UO->getType().isOwnedQualified()
        && UO->getSubExpr()->getType().isBorrowQualified()) {
        Diag(SL, diag::err_move_borrow);
    }
  } else if (auto ME = dyn_cast_or_null<MemberExpr>(E->IgnoreParenCasts())) {
    if (ME->getType().isOwnedQualified() && ME->getBase()->getType().isBorrowQualified())
      Diag(SL, diag::err_move_borrow);
  }
}

namespace {
/// Returns true if casting from RHS to LHS would cast away const qualifiers
/// (i.e. RHS has const that LHS doesn't at some level).
bool isCastingAwayConst(QualType LHS, QualType RHS) {
  if (LHS->isPointerType() && RHS->isPointerType()) {
    const auto *LHSPtr = LHS->getAs<PointerType>();
    const auto *RHSPtr = RHS->getAs<PointerType>();
    if (LHSPtr && RHSPtr)
      return isCastingAwayConst(LHSPtr->getPointeeType(), RHSPtr->getPointeeType());
  }
  // For non-pointer types: casting away const means RHS has const that LHS doesn't
  return RHS.isConstQualified() && !LHS.isConstQualified();
}
} // namespace

bool Sema::CheckBorrowQualTypeCStyleCast(QualType LHSType, QualType RHSType) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();

  // Allow borrow pointer to be cast from nullptr_t
  if (LHSCanType.isBorrowQualified() && LHSCanType->isPointerType() &&
      RHSCanType->isNullPtrType()) {
    return true;
  }

  // Defer borrow compatibility checks for dependent types to instantiation,
  // where concrete types are available for a precise check.
  if (RHSCanType->isDependentType() || LHSCanType->isDependentType()) {
    return true;
  }

  bool IsSameType = (LHSCanType.getTypePtr() == RHSCanType.getTypePtr());
  const auto *LHSPtrType = LHSType->getAs<PointerType>();
  const auto *RHSPtrType = RHSType->getAs<PointerType>();
  bool IsPointer = LHSPtrType && RHSPtrType;
  QualType RHSRawType = RHSCanType.getUnqualifiedType();
  QualType LHSRawType = LHSCanType.getUnqualifiedType();

  if (IsSameType) {
    return true;
  }
  if (LHSCanType->isIntegerType() && RHSCanType.isBorrowQualified() &&
      RHSCanType->isPointerType()) {
    return true;
  }
  if (!IsPointer)
    return false;
  if (LHSCanType->isVoidPointerType())
    return true;
  if (RHSCanType->isVoidPointerType() && !IsInSafeZone())
    return true;
  // Check borrow qualifier compatibility first - prevent casting between
  // mutable and const borrows to avoid aliasing (must run before hasSameType
  // which may treat types as equivalent)
  if (RHSCanType.isBorrowQualified() && LHSCanType.isBorrowQualified() &&
      (RHSCanType.isConstBorrow() != LHSCanType.isConstBorrow()))
    return false;
  if (Context.hasSameType(LHSRawType, RHSRawType))
    return true;
  if (TryDesugarTrait(RHSType))
    return true;

  // Reject casting away const (e.g. const int *const * -> int *const *)
  QualType LHSPointee = LHSPtrType->getPointeeType();
  QualType RHSPointee = RHSPtrType->getPointeeType();
  if (isCastingAwayConst(LHSPointee, RHSPointee))
    return false;
  return CheckBorrowQualTypeCStyleCast(LHSPointee, RHSPointee);
}

bool Sema::CheckBorrowQualTypeCStyleCast(QualType LHSType, QualType RHSType, SourceLocation RLoc) {
  if (!CheckBorrowQualTypeCStyleCast(LHSType, RHSType)) {
    Diag(RLoc, diag::err_borrow_qualcheck_incompatible) << CompleteTraitType(RHSType) << CompleteTraitType(LHSType);
    return false;
  } else {
    return true;
  }
}

bool Sema::CheckBorrowQualTypeAssignment(QualType LHSType, QualType RHSType, SourceLocation RLoc) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  const auto *LHSPtrType = LHSType->getAs<PointerType>();
  const auto *RHSPtrType = RHSType->getAs<PointerType>();
  bool IsPointer = LHSPtrType && RHSPtrType;

  if (LHSCanType.isBorrowQualified() == RHSCanType.isBorrowQualified()) {
    if (TraitDecl *TD = TryDesugarTrait(LHSCanType)) {
      if (TD->getTypeImpledVarDecl(RHSCanType->getPointeeType()))
        return true;
    }
    if (Context.hasSameType(LHSCanType, RHSCanType)) {
      return true;
    }
    if (LHSCanType->isVoidPointerType()) {
      if (LHSCanType->getPointeeType().isConstQualified() == RHSCanType->getPointeeType().isConstQualified())
        return true;
    }
    if (!IsPointer) {
      return false;
    } else {
      return CheckBorrowQualTypeAssignment(LHSPtrType->getPointeeType(), RHSPtrType->getPointeeType(), RLoc);
    }
  }

  return false;
}

bool Sema::CheckBorrowQualTypeAssignment(QualType LHSType, ExprResult &RHS) {
  Expr *RHSExpr = RHS.get();
  QualType RHSCanType =
      RHSExpr->getType().getCanonicalType().getUnqualifiedType();
  QualType LHSCanType = LHSType.getCanonicalType().getUnqualifiedType();
  SourceLocation ExprLoc = RHSExpr->getBeginLoc();
  bool Res = true;

  // Defer borrow compatibility checks for dependent types to instantiation,
  // where concrete types are available for a precise check.
  if (RHSCanType->isDependentType() || LHSCanType->isDependentType())
    return true;

  if (LHSCanType.isBorrowQualified() || RHSCanType.isBorrowQualified()) {
    if (TraitDecl *TD = TryDesugarTrait(LHSCanType)) {
      if (RHSCanType->isPointerType()) {
        QualType ImplType = RHSCanType->getPointeeType().getUnqualifiedType().getCanonicalType();
        ImplType.removeLocalOwned();
        if (TD->getTypeImpledVarDecl(ImplType))
          return true;
      }
      // trait T* borrow <-> trait T* borrow // legal
      if (TD) {
        QualType QT = DesugarTraitToStructTrait(TD, LHSCanType, RHSExpr->getExprLoc());
        if (QT.getCanonicalType() == RHSCanType) {
          return true;
        }
      }
    }

    // Borrow pointer can be inited by nullptr.
    if (LHSCanType.isBorrowQualified() && LHSCanType->isPointerType() &&
        isa<CXXNullPtrLiteralExpr>(RHSExpr->IgnoreParens()))
      return true;

    if (LHSCanType->isVoidPointerType()) {
      // Handle array-to-pointer decay: arrays decay to raw pointers (no borrow).
      if (RHSCanType->isArrayType()) {
        // Arrays cannot match borrow pointers (they decay to raw pointers).
        if (LHSCanType.isBorrowQualified()) {
          Res = false;
        } else {
          // Check const compatibility between void* and array element type.
          QualType RHSElementType = RHSCanType->getAsArrayTypeUnsafe()->getElementType();
          if (LHSCanType->getPointeeType().isConstQualified() == RHSElementType.isConstQualified())
            return true;
        }
      } else if (RHSCanType->isPointerType()) {
        // Pointer-to-pointer: check const and borrow qualifiers.
        if (LHSCanType->getPointeeType().isConstQualified() == RHSCanType->getPointeeType().isConstQualified() &&
            LHSCanType.isBorrowQualified() == RHSCanType.isBorrowQualified())
          return true;
      }
    }

    // Allow mutable borrow downgrading to immutable borrow (re-borrow)
    if (LHSCanType->isPointerType() && LHSCanType.isBorrowQualified() &&
        RHSCanType->isPointerType() && RHSCanType.isBorrowQualified()) {
      QualType LHSPointee = LHSCanType->getPointeeType();
      QualType RHSPointee = RHSCanType->getPointeeType();
      if (LHSPointee.isConstQualified() && !RHSPointee.isConstQualified()) {
        LHSPointee.removeLocalConst();
        if (LHSPointee == RHSPointee) {
          ExprResult DerefExpr =
              CreateBuiltinUnaryOp(ExprLoc, UO_Deref, RHSExpr);
          if (!DerefExpr.isInvalid()) {
            ExprResult ReBorrowExpr =
                CreateBuiltinUnaryOp(ExprLoc, UO_AddrConst, DerefExpr.get());
            if (!ReBorrowExpr.isInvalid()) {
              RHS = ReBorrowExpr;
              return true;
            }
          }
          Res = false;
        }
      }
    }

    if (!Context.hasSameType(LHSCanType, RHSCanType))
      Res = false;

    // _Bool <- T *_Borrow is allowed
    if (RHSCanType->isPointerType() && RHSCanType.isBorrowQualified() &&
        LHSCanType->isBooleanType()) {
      Res = true;
    }
  } else {
    Res = CheckBorrowQualTypeAssignment(LHSType, RHSCanType, ExprLoc);
  }
  if (!Res) {
    Diag(ExprLoc, diag::err_borrow_qualcheck_incompatible)
        << CompleteTraitType(RHSExpr->getType()) << CompleteTraitType(LHSType);
  }
  return Res;
}

bool Sema::CheckBorrowQualTypeCompare(QualType LHSType, QualType RHSType) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  if (LHSCanType.isBorrowQualified() || RHSCanType.isBorrowQualified()) {
    if (RHSCanType != LHSCanType) {
      return false;
    }
  }
  return true;
}

void Sema::CheckBorrowFunctionType(QualType ReturnTy, SmallVector<QualType, 16> ParamTys, SourceLocation SL) {
  if (ReturnTy->isDependentType()) {
    return;
  }
  if (ReturnTy.hasBorrow()) {
    bool HasBorrowParam = false;
    for (QualType PT : ParamTys) {
      if (PT->isDependentType()) {
        return;
      }
      if (PT.hasBorrow()) {
        HasBorrowParam = true;
        break;
      }
    }
    if (!HasBorrowParam)
      Diag(SL, diag::err_typecheck_borrow_func);
  }
}

bool Sema::CheckBorrowFunctionPointerType(QualType LHSType, Expr *RHSExpr) {
  const FunctionProtoType *LSHFuncType = LHSType->getAs<PointerType>()
                                             ->getPointeeType()
                                             ->getAs<FunctionProtoType>();
  const FunctionProtoType *RSHFuncType =
      RHSExpr->getType()->isFunctionPointerType()
          ? RHSExpr->getType()
                ->getAs<PointerType>()
                ->getPointeeType()
                ->getAs<FunctionProtoType>()
          : RHSExpr->getType()->getAs<FunctionProtoType>();
  SourceLocation ExprLoc = RHSExpr->getBeginLoc();

  // For heterogeneous redeclarations, select the best matching declaration.
  if (FunctionDecl *SelectedFD =
          SelectFunctionDeclForPointerAssignment(RHSExpr, LSHFuncType))
    RSHFuncType = SelectedFD->getType()->getAs<FunctionProtoType>();

  // return if no 'borrow' in both side
  if (!LSHFuncType->hasBorrowRetOrParams() &&
      !RSHFuncType->hasBorrowRetOrParams()) {
    return true;
  }
  
  auto BorrowParamTypesMatch = [&](QualType Dest, QualType Src) -> bool {
    Dest = Dest.getUnqualifiedType();
    Src = Src.getUnqualifiedType();
    if (!DoPointerTypesSatisfyAssignmentConstraintsStrict(Dest, Src))
      return false;
    // For pointer params, additionally require that the pointee types match
    // exactly including const/volatile (not just unqualified base type).
    if (Dest->isPointerType() && Src->isPointerType()) {
      QualType DestPointee = Dest->getPointeeType().getCanonicalType();
      QualType SrcPointee = Src->getPointeeType().getCanonicalType();
      // Strip BSC qualifiers (_Borrow/_Owned) from the pointee for comparison;
      // keep all standard qualifiers (const, volatile, restrict).
      DestPointee.removeLocalOwned();
      DestPointee.removeLocalBorrow();
      SrcPointee.removeLocalOwned();
      SrcPointee.removeLocalBorrow();
      if (DestPointee != SrcPointee)
        return false;
    }
    return true;
  };

  bool Compatible = true;
  if (LSHFuncType->getNumParams() != RSHFuncType->getNumParams()) {
    Compatible = false;
  } else {
    if (!BorrowParamTypesMatch(LSHFuncType->getReturnType(),
                               RSHFuncType->getReturnType()))
      Compatible = false;
    for (unsigned I = 0, N = LSHFuncType->getNumParams(); Compatible && I < N; ++I) {
      if (!BorrowParamTypesMatch(LSHFuncType->getParamType(I),
                                 RSHFuncType->getParamType(I)))
        Compatible = false;
    }
  }
  if (!Compatible) {
    Diag(ExprLoc, diag::err_funcPtr_incompatible)
        << LHSType << RHSExpr->getType();
    return false;
  }
  return true;
}

bool Sema::CheckEnsureInitFunctionPointerType(QualType LHSType, Expr *RHSExpr) {
  const FunctionProtoType *LHSFuncType = LHSType->getAs<PointerType>()
                                             ->getPointeeType()
                                             ->getAs<FunctionProtoType>();
  const FunctionProtoType *RHSFuncType =
      RHSExpr->getType()->isFunctionPointerType()
          ? RHSExpr->getType()
                ->getAs<PointerType>()
                ->getPointeeType()
                ->getAs<FunctionProtoType>()
          : RHSExpr->getType()->getAs<FunctionProtoType>();

  if (!LHSFuncType || !RHSFuncType)
    return true;

  // For heterogeneous redeclarations, select the best matching declaration.
  if (FunctionDecl *SelectedFD =
          SelectFunctionDeclForPointerAssignment(RHSExpr, LHSFuncType))
    RHSFuncType = SelectedFD->getType()->getAs<FunctionProtoType>();

  if (LHSFuncType->getNumParams() != RHSFuncType->getNumParams())
    return true; // Param count mismatch handled elsewhere

  // Check: if target has ensure_init but source doesn't → error.
  // Source having ensure_init but target not → ok (stronger to weaker).
  for (unsigned I = 0; I < LHSFuncType->getNumParams(); ++I) {
    bool LHSHasEnsureInit = LHSFuncType->hasExtParameterInfos() &&
                            LHSFuncType->getExtParameterInfo(I).isEnsureInit();
    bool RHSHasEnsureInit = RHSFuncType->hasExtParameterInfos() &&
                            RHSFuncType->getExtParameterInfo(I).isEnsureInit();

    if (LHSHasEnsureInit && !RHSHasEnsureInit) {
      Diag(RHSExpr->getBeginLoc(),
           diag::err_ensure_init_funcptr_incompatible)
          << I;
      return false;
    }
  }
  return true;
}

// for global borrow variable type check
void Sema::CheckBorrowOrIndirectBorrowType(SourceLocation ErrLoc, QualType T,
                                           StringRef Env) {
  enum { BorrowQualified, BorrowTypedef, BorrowFields };
  if (T.getCanonicalType().isBorrowQualified() &&
      !T.getTypePtr()->getAs<TypedefType>()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << BorrowQualified << "_Borrow" << Env;
  } else if (T.getCanonicalType().isBorrowQualified() &&
             T.getTypePtr()->getAs<TypedefType>()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << BorrowTypedef << "_Borrow" << Env << T;
  } else if (T.getCanonicalType().getTypePtr()->hasBorrowFields()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check)
        << BorrowFields << "_Borrow" << Env << T;
  }
}
#endif
