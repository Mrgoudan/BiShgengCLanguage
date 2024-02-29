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

bool Type::hasOwnedFields() const {
  if (const auto *RecTy = dyn_cast<RecordType>(CanonicalType)) {
    return RecTy->hasOwnedFields();
  } else if (const auto *PointerTy = dyn_cast<PointerType>(CanonicalType)) {
    return PointerTy->hasOwnedFields();
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

void RecordType::initOwnedStatus() const {
  if (hasOwn != ownedStatus::unInit)
    return;
  std::vector<const RecordType *> RecordTypeList;
  RecordTypeList.push_back(this);
  unsigned NextToCheckIndex = 0;

  while (RecordTypeList.size() > NextToCheckIndex) {
    for (FieldDecl *FD :
         RecordTypeList[NextToCheckIndex]->getDecl()->fields()) {
      QualType FieldTy = FD->getType();
      if (FieldTy.isOwnedQualified()) {
        hasOwn = ownedStatus::withOwned;
        return;
      }
      QualType tempQT = FieldTy;
      const Type *tempT = tempQT.getTypePtr();
      while (tempT->isPointerType()) {
        tempQT = tempT->getPointeeType();
        if (tempQT.isOwnedQualified()) {
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

bool RecordType::hasOwnedFields() const {
  if (hasOwn == ownedStatus::unInit)
    initOwnedStatus();
  if (hasOwn == ownedStatus::withOwned)
    return true;
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

#endif