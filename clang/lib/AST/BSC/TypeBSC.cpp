//===- TypeBSC.cpp - Type representation and manipulation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements the BSC type-related functionality.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"

using namespace clang;

// hasOwnedFields is used to determine whether a type has a field
// that is directly or indirectly qualified by owned.
// If you want to determine whether a type is a move semantic type,
// use isMoveSemanticType instead.
bool PointerType::hasOwnedFields() const {
  QualType R = getPointeeType();
  if (R.isOwnedQualified()) {
    return true;
  }
  if (R.getTypePtr()->hasOwnedFields()) {
    return true;
  }
  return false;
}

// hasOwnedFields is used to determine whether a type has a field
// that is directly or indirectly qualified by owned.
// If you want to determine whether a type is a move semantic type,
// use isMoveSemanticType instead.
bool Type::hasOwnedFields() const {
  if (const auto *RecTy = dyn_cast<RecordType>(CanonicalType)) {
    return RecTy->hasOwnedFields();
  } else if (const auto *PointerTy = dyn_cast<PointerType>(CanonicalType)) {
    return PointerTy->hasOwnedFields();
  }
  return false;
}

bool PointerType::hasBorrowFields() const {
  QualType R = getPointeeType();
  if (R.isBorrowQualified()) {
    return true;
  }
  if (R.getTypePtr()->hasBorrowFields()) {
    return true;
  }
  return false;
}

bool Type::hasBorrowFields() const {
  if (const auto *RecTy = dyn_cast<RecordType>(CanonicalType)) {
    return RecTy->hasBorrowFields();
  } else if (const auto *PointerTy = dyn_cast<PointerType>(CanonicalType)) {
    return PointerTy->hasBorrowFields();
  }
  return false;
}

bool Type::withBorrowFields() const {
  if (const auto *RT = dyn_cast<RecordType>(CanonicalType)) {
    return RT->withBorrowFields();
  }
  return false;
}

bool FunctionProtoType::hasOwnedRetOrParams() const {
  if (getReturnType().isOwnedQualified()) {
    return true;
  }
  for (auto ParamType : getParamTypes()) {
    if (ParamType.isOwnedQualified()) {
      return true;
    }
  }
  return false;
}

bool FunctionProtoType::hasBorrowRetOrParams() const {
  if (getReturnType().hasBorrow()) {
    return true;
  }
  for (auto ParamType : getParamTypes()) {
    if (ParamType.hasBorrow()) {
      return true;
    }
  }
  return false;
}

bool Type::checkFunctionProtoType(SafeZoneSpecifier SZS) const {
  const FunctionProtoType *FPT = nullptr;
  if (isFunctionType()) {
    FPT = getAs<FunctionProtoType>();
  } else if (isFunctionPointerType()) {
    FPT = getPointeeType()->getAs<FunctionProtoType>();
  }
  if (FPT) {
    FunctionProtoType::ExtProtoInfo EPI = FPT->getExtProtoInfo();
    return EPI.SafeZoneSpec == SZS;
  }
  return false;
}

namespace clang {

/// Check owned/borrow qualifier compatibility for heterogeneous redeclarations.
/// UnsafeType is the unsafe decl, SafeType is the _Safe redeclaration.
/// The _Safe redeclaration may only add qualifiers, not remove them.
static bool AreOwnedBorrowQualifiersCompatible(QualType UnsafeType,
                                               QualType SafeType) {
  bool UnsafeIsOwned =
      UnsafeType->isPointerType() && UnsafeType.isOwnedQualified();
  bool UnsafeIsBorrow =
      UnsafeType->isPointerType() && UnsafeType.isBorrowQualified();
  bool SafeIsOwned = SafeType->isPointerType() && SafeType.isOwnedQualified();
  bool SafeIsBorrow = SafeType->isPointerType() && SafeType.isBorrowQualified();

  // Safe redecl must not drop a qualifier present in the unsafe decl.
  if (UnsafeIsOwned && !SafeIsOwned)
    return false;
  if (UnsafeIsBorrow && !SafeIsBorrow)
    return false;
  // owned ⟷ borrow is always incompatible.
  if ((SafeIsOwned && UnsafeIsBorrow) || (SafeIsBorrow && UnsafeIsOwned))
    return false;
  return true;
}

/// Check if two function types are compatible for heterogeneous redeclarations
/// where one is declared safe and the other unsafe.
///
/// Strategy:
/// 1. Use Clang's typesAreCompatible (which automatically strips owned/borrow
///    via mergeTypes, while preserving const/volatile/restrict checking)
/// 2. Add BSC-specific check: ensure owned and borrow are not mixed
bool areFunctionTypesCompatibleForHeterogeneousRedecl(
    ASTContext &Ctx, QualType Type1, QualType Type2,
    SafeZoneSpecifier SZS1, SafeZoneSpecifier SZS2) {
  // Verify exactly one is safe and the other is unsafe (heterogeneous redeclaration).
  // Safe: SZ_Safe
  // Unsafe: SZ_Unsafe or SZ_None
  bool Type1IsSafe = (SZS1 == SZ_Safe);
  bool Type2IsSafe = (SZS2 == SZ_Safe);

  // Must be heterogeneous: exactly one safe, one unsafe (XOR)
  if (Type1IsSafe == Type2IsSafe)
    return false;

  // Determine which is unsafe and which is safe for later use.
  bool Type1IsUnsafe = !Type1IsSafe;

  const FunctionProtoType *FPT1 = Type1->getAs<FunctionProtoType>();
  const FunctionProtoType *FPT2 = Type2->getAs<FunctionProtoType>();

  if (!FPT1 || !FPT2)
    return false;

  // Verify parameter count and variadic consistency.
  if (FPT1->getNumParams() != FPT2->getNumParams())
    return false;
  if (FPT1->isVariadic() != FPT2->isVariadic())
    return false;

  // Identify which FunctionProtoType is unsafe and which is safe.
  // Reuse the FPT1/FPT2 pointers we already retrieved.
  const FunctionProtoType *UnsafeFPT = Type1IsUnsafe ? FPT1 : FPT2;
  const FunctionProtoType *SafeFPT = Type1IsUnsafe ? FPT2 : FPT1;

  // Helper lambda: check if two (possibly function-pointer) types are compatible
  // in the context of a heterogeneous redeclaration. For function pointer types
  // that differ only in SafeZoneSpecifier, we apply the heterogeneous check
  // recursively instead of relying on typesAreCompatible (which rejects
  // safe/unsafe mismatches unconditionally).
  auto AreParamTypesCompatible = [&](QualType UnsafeT, QualType SafeT) -> bool {
    // Save originals for owned/borrow check.
    QualType UnsafeTOrig = UnsafeT;
    QualType SafeTOrig = SafeT;

    // Strip nullability and owned/borrow for base type compatibility checking.
    AttributedType::stripOuterNullability(UnsafeT);
    AttributedType::stripOuterNullability(SafeT);
    UnsafeT.removeLocalOwned();
    UnsafeT.removeLocalBorrow();
    SafeT.removeLocalOwned();
    SafeT.removeLocalBorrow();

    // Fast path: identical canonical unqualified types.
    if (UnsafeT.getCanonicalType().getUnqualifiedType() ==
        SafeT.getCanonicalType().getUnqualifiedType()) {
      return AreOwnedBorrowQualifiersCompatible(UnsafeTOrig, SafeTOrig);
    }

    // If both are function pointer types, check heterogeneous compatibility
    // recursively so that e.g. `func` and `func_safe` are accepted as a
    // compatible pair when used as parameters in a heterogeneous redeclaration.
    if (UnsafeT->isFunctionPointerType() && SafeT->isFunctionPointerType()) {
      QualType UnsafePointee = UnsafeT->getPointeeType();
      QualType SafePointee = SafeT->getPointeeType();
      const FunctionProtoType *UnsafeFP =
          UnsafePointee->getAs<FunctionProtoType>();
      const FunctionProtoType *SafeFP =
          SafePointee->getAs<FunctionProtoType>();
      if (UnsafeFP && SafeFP) {
        SafeZoneSpecifier UnsafeFPSZS = UnsafeFP->getFunSafeZoneSpecifier();
        SafeZoneSpecifier SafeFPSZS = SafeFP->getFunSafeZoneSpecifier();
        // Only treat as a heterogeneous function-pointer pair when one side is
        // safe and the other is not.
        bool UnsafeFPIsSafe = (UnsafeFPSZS == SZ_Safe);
        bool SafeFPIsSafe = (SafeFPSZS == SZ_Safe);
        if (UnsafeFPIsSafe != SafeFPIsSafe) {
          return areFunctionTypesCompatibleForHeterogeneousRedecl(
              Ctx, UnsafePointee, SafePointee, UnsafeFPSZS, SafeFPSZS);
        }
      }
    }

    // General case: use Clang's standard type compatibility check.
    if (!Ctx.typesAreCompatible(UnsafeT, SafeT))
      return false;

    return AreOwnedBorrowQualifiersCompatible(UnsafeTOrig, SafeTOrig);
  };

  // Check return type compatibility.
  QualType UnsafeRet = UnsafeFPT->getReturnType();
  QualType SafeRet = SafeFPT->getReturnType();

  if (!AreParamTypesCompatible(UnsafeRet, SafeRet))
    return false;

  // Check parameter type compatibility for each parameter.
  for (unsigned i = 0; i < FPT1->getNumParams(); ++i) {
    QualType UnsafeParam = UnsafeFPT->getParamType(i);
    QualType SafeParam = SafeFPT->getParamType(i);

    if (!AreParamTypesCompatible(UnsafeParam, SafeParam))
      return false;
  }

  return true;
}

} // namespace clang

bool Type::isOwnedStructureType() const {
  if (const auto *RT = getAs<RecordType>())
    return RT->getDecl()->isStruct() && RT->getDecl()->isOwnedDecl();
  return false;
}

bool Type::isOwnedTemplateSpecializationType() const {
  if (const auto *RT = getAs<TemplateSpecializationType>()) {
    if (RT->getTemplateName().getAsTemplateDecl() &&
        RT->getTemplateName().getAsTemplateDecl()->getTemplatedDecl()) {
      if (auto RD = dyn_cast<RecordDecl>(
              RT->getTemplateName().getAsTemplateDecl()->getTemplatedDecl()))
        return RD->isOwnedDecl();
    }
  }
  return false;
}

// Return true when a type is move semantic type,
// including owned pointer(int *owned, int **owned, ...),
// owned struct and struct which has owned fields, for example:
// @code
//     owned struct S1 { };
//     struct S2 { int* owned p; };
//     struct S3 { S1 s; };
//     struct S4 { struct S2 s; };
// @endcode
// These types are not move semantic:
// @code
//     struct S5 { S1* s};
//     struct S6 { int *owned * p};
// @endcode
bool Type::isMoveSemanticType() const {
  // Owned pointer or owned struct is owned qualified.
  if (CanonicalType.isOwnedQualified())
    return true;
  if (const auto *RecTy = dyn_cast<RecordType>(CanonicalType)) {
    if (RecordDecl *RD = RecTy->getDecl()) {
      for (FieldDecl *FD : RD->fields()) {
        QualType FQT = FD->getType().getCanonicalType();
        if (FQT.isOwnedQualified())
          return true;
        else if (isa<RecordType>(FQT))
          return FQT->isMoveSemanticType();
      }
    }
  }
  return false;
}

bool Type::isTrivialDataType(bool AllowIncompleteType) const {
  if (CanonicalType.isBorrowQualified() || CanonicalType.isOwnedQualified()) {
    return false;
  }
  if (CanonicalType->isOwnedStructureType()) {
    return false;
  }
  if (CanonicalType->isPointerType()) {
    return false;
  }
  if (const auto *ArrTy = dyn_cast<ArrayType>(CanonicalType)) {
    QualType ET = ArrTy->getElementType().getCanonicalType();
    return ET->isTrivialDataType(true);
  }
  if (!AllowIncompleteType && CanonicalType->isIncompleteType())
    return false;

  if (const auto *RecTy = dyn_cast<RecordType>(CanonicalType)) {
    if (RecordDecl *RD = RecTy->getDecl()) {
      for (FieldDecl *FD : RD->fields()) {
        QualType FQT = FD->getType().getCanonicalType();
        if (FQT.isBorrowQualified() || FQT.isOwnedQualified()) {
          return false;
        }
        if (!FQT->isTrivialDataType(true)) {
          return false;
        }
      }
    }
  }
  return true;
}

void RecordType::initOwnedStatus() const {
  if (hasOwn != ownedStatus::unInitOwned)
    return;
  std::vector<const RecordType *> RecordTypeList;
  RecordTypeList.push_back(this);
  unsigned NextToCheckIndex = 0;

  while (RecordTypeList.size() > NextToCheckIndex) {
    for (FieldDecl *FD :
         RecordTypeList[NextToCheckIndex]->getDecl()->fields()) {
      QualType FieldTy = FD->getType().getCanonicalType();
      bool isOwnedStructType = FieldTy->isOwnedStructureType();
      if (FieldTy.isOwnedQualified() || isOwnedStructType) {
        hasOwn = ownedStatus::withOwned;
        return;
      }
      QualType tempQT = FieldTy;
      const Type *tempT = tempQT.getTypePtr();
      while (tempT->isPointerType()) {
        tempQT = tempT->getPointeeType();
        isOwnedStructType = tempQT.getCanonicalType()->isOwnedStructureType();
        if (tempQT.isOwnedQualified() && !isOwnedStructType) {
          hasOwn = ownedStatus::withOwned;
          return;
        } else {
          tempQT = tempQT.getCanonicalType();
          tempT = tempQT.getTypePtr();
        }
      }
      FieldTy = tempQT.getCanonicalType();
      if (const auto *FieldRecTy = FieldTy->getAs<RecordType>()) {
        if (llvm::find(RecordTypeList, FieldRecTy) == RecordTypeList.end())
          RecordTypeList.push_back(FieldRecTy);
      }
    }
    ++NextToCheckIndex;
  }
  hasOwn = ownedStatus::withoutOwned;
  return;
}

// hasOwnedFields is used to determine whether a type has a field
// that is directly or indirectly qualified by owned.
// If you want to determine whether a type is a move semantic type,
// use isMoveSemanticType instead.
bool RecordType::hasOwnedFields() const {
  if (hasOwn == ownedStatus::unInitOwned)
    initOwnedStatus();
  if (hasOwn == ownedStatus::withOwned)
    return true;
  return false;
}

void RecordType::initBorrowStatus() const {
  if (hasBorrow != borrowStatus::unInitBorrow)
    return;
  std::vector<const RecordType *> RecordTypeList;
  RecordTypeList.push_back(this);
  unsigned NextToCheckIndex = 0;

  while (RecordTypeList.size() > NextToCheckIndex) {
    for (FieldDecl *FD :
         RecordTypeList[NextToCheckIndex]->getDecl()->fields()) {
      QualType FieldTy = FD->getType();
      if (FieldTy.isBorrowQualified()) {
        hasBorrow = borrowStatus::withBorrow;
        return;
      }
      QualType tempQT = FieldTy;
      const Type *tempT = tempQT.getTypePtr();
      while (tempT->isPointerType()) {
        tempQT = tempT->getPointeeType();
        if (tempQT.isBorrowQualified()) {
          hasBorrow = borrowStatus::withBorrow;
          return;
        } else {
          tempQT = tempQT.getCanonicalType();
          tempT = tempQT.getTypePtr();
        }
      }
      FieldTy = tempQT.getCanonicalType();
      if (const auto *FieldRecTy = FieldTy->getAs<RecordType>()) {
        if (llvm::find(RecordTypeList, FieldRecTy) == RecordTypeList.end())
          RecordTypeList.push_back(FieldRecTy);
      }
    }
    ++NextToCheckIndex;
  }
  hasBorrow = borrowStatus::withoutBorrow;
  return;
}

bool RecordType::hasBorrowFields() const {
  if (hasBorrow == borrowStatus::unInitBorrow)
    initBorrowStatus();
  if (hasBorrow == borrowStatus::withBorrow)
    return true;
  return false;
}

bool RecordType::withBorrowFields() const {
  std::vector<const RecordType*> RecordTypeList;
  RecordTypeList.push_back(this);
  unsigned NextToCheckIndex = 0;

  while (RecordTypeList.size() > NextToCheckIndex) {
    for (FieldDecl *FD :
         RecordTypeList[NextToCheckIndex]->getDecl()->fields()) {
      QualType FieldTy = FD->getType();
      if (FieldTy->isPointerType() && FieldTy.isOwnedQualified())
        return FieldTy->getPointeeType()->withBorrowFields();
      if (FieldTy.isBorrowQualified())
        return true;
      FieldTy = FieldTy.getCanonicalType();
      if (const auto *FieldRecTy = FieldTy->getAs<RecordType>()) {
        if (!llvm::is_contained(RecordTypeList, FieldRecTy))
          RecordTypeList.push_back(FieldRecTy);
      }
    }
    ++NextToCheckIndex;
  }
  return false;
}

bool Type::isBSCFutureType() const {
  if (const auto *RT = getAs<RecordType>()) {
    RecordDecl *RD = RT->getAsRecordDecl();
    if (isa<ClassTemplateSpecializationDecl>(RD)) {
      return RD->getNameAsString() == "__Trait_Future";
    }
  }
  return false;
}

ConditionalType::ConditionalType(llvm::Optional<bool> CondRes, Expr *CondE,
                                 QualType T1, QualType T2, QualType can)
    : Type(Conditional, can,
           toTypeDependence(CondE->getDependence()) |
               (CondE->isInstantiationDependent() ? TypeDependence::Dependent
                                                  : TypeDependence::None) |
               (CondE->getType()->getDependence() &
                TypeDependence::VariablyModified) |
               T1->getDependence() | T2->getDependence()),
      CondResult(CondRes), CondExpr(CondE), Type1(T1), Type2(T2),
      UnderlyingType(can) {}

bool ConditionalType::isSugared() const {
  return !CondExpr->isInstantiationDependent();
}

QualType ConditionalType::desugar() const {
  if (isSugared())
    return getUnderlyingType();

  return QualType(this, 0);
}
bool QualType::hasBorrow() const {
  if (isBorrowQualified())
    return true;
  return getTypePtr()->hasBorrowFields();
}

bool QualType::isConstBorrow() const {
  if (!isBorrowQualified())
    return false;
  if (!getTypePtr()->isPointerType())
    return false;
  QualType directPointee = getTypePtr()->getPointeeType();
  return directPointee.isConstQualified();
}

bool QualType::isConstPointee() const {
  QualType QT = QualType(getTypePtr(), getLocalFastQualifiers());
  while (QT->isPointerType()) {
      QT = QT->getPointeeType();
  }
  if (QT.isLocalConstQualified())
      return true;
  return false;
}

QualType QualType::addConstBorrow(const ASTContext &Context) {
  QualType pointee;
  if (getTypePtr()->isPointerType()) {
    // Use the pointee type as stored (preserve sugar) so the result type prints
    // without an extra tag (e.g. "const s<int> *_Borrow" not "const struct s<int> *_Borrow").
    pointee = getTypePtr()->getPointeeType();
  } else {
    // Non-pointer: &_Const applied to a value of type T (e.g. *a with type s<T>)
    // yields const T* _Borrow. Use the type as-is so printing matches the
    // operand (e.g. "s<int>" not "struct s<int>").
    pointee = *this;
  }
  pointee.addConst();  // Add const to the (direct) pointee (the borrowed object)
  QualType result = Context.getPointerType(pointee);
  result.addBorrow();
  return result;
}

QualType QualType::removeConstForBorrow(const ASTContext &Context) {
  // Only applies to pointer types (e.g. const int * from dereferencing const int * borrow).
  // For non-pointer types (e.g. struct S from dereferencing struct S * borrow),
  // return unchanged - no const to remove.
  if (!getTypePtr()->isPointerType())
    return *this;
  QualType directPointee = getTypePtr()->getPointeeType();
  directPointee.removeLocalConst();
  return Context.getPointerType(directPointee);
}

#endif