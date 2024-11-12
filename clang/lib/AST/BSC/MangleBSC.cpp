//===--- MangleBSC.cpp - Mangle BSC Names --------------------------*- cbs -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements generic name mangling support for bsc.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/BSC/MangleBSC.h"


using namespace clang;

bool MangleBSCContext::mangleBSCName(const NamedDecl *ND, raw_ostream &Out) {
  const auto *BMD = dyn_cast<BSCMethodDecl>(ND);
  const auto *BFD = dyn_cast<FunctionDecl>(ND);
  clang::PrintingPolicy SubPolicy(Context.getLangOpts());
  SubPolicy.RewriteBSC = true;
  std::string MethodStr;
  if (BMD && !BMD->getExtendedType().isNull()) {
    MethodStr = getBSCMethodMangleName(BMD, SubPolicy);
  } else if (BFD->isTemplateInstantiation()) {
    MethodStr = getBSCFunctionMangleName(BFD, SubPolicy);
  } else if (BMD && BMD->isDestructor()) {
    MethodStr = getBSCDesturctorMangleName(BFD, SubPolicy);
  } else{
    return false;
  }
  llvm::outs() << "MethodStr is " << MethodStr << " \n";
  Out << MethodStr;
  return true;
}

std::string
MangleBSCContext::getBSCMethodMangleName(const BSCMethodDecl *BMD,
                                         clang::PrintingPolicy SubPolicy) {
  QualType T = BMD->getExtendedType();
  std::string TypeStr = getBSCTypeName(T, SubPolicy);
  if (BMD->isDestructor())
    TypeStr += "_D";
  else
    TypeStr = TypeStr + "_" + BMD->getNameAsString();
  return TypeStr;
}

std::string
MangleBSCContext::getBSCDesturctorMangleName(const FunctionDecl *BFD,
                                             clang::PrintingPolicy SubPolicy) {
  QualType BDT =
      Context.getTypeDeclType(dyn_cast<RecordDecl>(BFD->getParent()));
  std::string TypeStr = getBSCTypeName(BDT, SubPolicy);
  TypeStr += "_D";
  return TypeStr;
}

std::string
MangleBSCContext::getBSCFunctionMangleName(const FunctionDecl *BFD,
                                           clang::PrintingPolicy SubPolicy) {
  std::string FunctionName = BFD->getNameAsString();
  if (const TemplateArgumentList *TArgs =
          BFD->getTemplateSpecializationArgs()) {
    for (size_t i = 0; i < TArgs->size(); i++) {
      std::string ArgType = getBSCTemplateArgName(TArgs->get(i), SubPolicy);
      FunctionName = FunctionName + "_" + ArgType;
    }
  }
  return FunctionName;
}

std::string MangleBSCContext::getBSCTypeName(QualType QT,
                                             const PrintingPolicy &Policy) {
  std::string ExtendedTypeStr;
  llvm::raw_string_ostream OS(ExtendedTypeStr);
  QT.print(OS, Policy);
  int len = ExtendedTypeStr.length() - 1;
  for (int i = len; i >= 0; i--) {
    if (ExtendedTypeStr[i] == ' ') {
      if (i == 0) {
        ExtendedTypeStr.replace(i, 1, "");
        continue;
      }
      ExtendedTypeStr.replace(i, 1, "_");
    } else if (ExtendedTypeStr[i] == '*') {
      // Since '*' is not allowed to appear in identifier,
      // we replace it with 'P'.
      // FIXME: it may conflict with user defined type Char_P.
      ExtendedTypeStr.replace(i, 1, "P");
    } else if (ExtendedTypeStr[i] == '(') {
      // Since '(' is not allowed to appear in identifier,
      // we replace it with 'LP'.
      ExtendedTypeStr.replace(i, 1, "LP");
    } else if (ExtendedTypeStr[i] == ')') {
      // Since ')' is not allowed to appear in identifier,
      // we replace it with 'RP'.
      ExtendedTypeStr.replace(i, 1, "RP");
    } else if (ExtendedTypeStr[i] == '[') {
      // Since '[' is not allowed to appear in identifier,
      // we replace it with 'LB'.
      ExtendedTypeStr.replace(i, 1, "LB");
    } else if (ExtendedTypeStr[i] == ']') {
      // Since ']' is not allowed to appear in identifier,
      // we replace it with 'RB'.
      ExtendedTypeStr.replace(i, 1, "RB");
    } else if (ExtendedTypeStr[i] == ',') {
      // Since ',' is not allowed to appear in identifier,
      // we replace it with 'COMMA'.
      ExtendedTypeStr.replace(i, 1, "COMMA");
    }
  }
  return ExtendedTypeStr;
}

std::string MangleBSCContext::getBSCTemplateArgName(
    const TemplateArgument &TemplateArg, const PrintingPolicy &Policy) {
  std::string ArgName;
  if (TemplateArg.getKind() == clang::TemplateArgument::ArgKind::Type)
    ArgName = getBSCTypeName(TemplateArg.getAsType(), Policy);
  else if (TemplateArg.getKind() ==
           clang::TemplateArgument::ArgKind::Integral) {
    llvm::APSInt TemplateInt = TemplateArg.getAsIntegral();
    if (TemplateInt.isNegative())
      ArgName = "_n" + std::to_string(-TemplateInt.getExtValue());
    else
      ArgName = "_" + std::to_string(TemplateInt.getExtValue());
  }
  return ArgName;
}

#endif

