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
  if (!ManglePolicy.RewriteBSC) {
    ManglePolicy.adjustForRewritingBSC();
  }
  std::string MethodStr;

  if (BMD) {
    // Handle the BSC method, including the name of the type template.
    MethodStr = getBSCMethodMangleName(BMD);
    if (BMD->isTemplateInstantiation()) {
      if (const TemplateArgumentList *TArgs =
              BFD->getTemplateSpecializationArgs()) {
        ArrayRef<TemplateArgument> Args = TArgs->asArray();
        std::string TemplateArgsName =
            getBSCTemplateArgsName(Args, ManglePolicy);
        MethodStr += TemplateArgsName;
      }
    }
  } else if (BFD && BFD->isTemplateInstantiation()) {
    // Handle the functiondecl with template args.
    MethodStr = getBSCFunctionMangleName(BFD, ManglePolicy);
  } else {
    // Without the BSC mangling, handle it separately at the point of call-in.
    return false;
  }
  Out << MethodStr;
  return true;
}

std::string
MangleBSCContext::getBSCMethodMangleName(const BSCMethodDecl *BMD) {
  std::string TypeStr;
  // For destructors, especially the automatically generated destructors, there
  // is no ExtendedType, and handle separately.
  if (BMD->isDestructor()) {
    TypeStr = getBSCDesturctorMangleName(BMD);
  } else {
    QualType T = BMD->getExtendedType();
    TypeStr = getBSCTypeName(T, ManglePolicy)+ "_" + BMD->getNameAsString();
  }
  return TypeStr;
}

std::string
MangleBSCContext::getBSCDesturctorMangleName(const BSCMethodDecl *BMD) {
  QualType BDT =
      Context.getTypeDeclType(dyn_cast<RecordDecl>(BMD->getParent()));
  std::string TypeStr = getBSCTypeName(BDT, ManglePolicy);
  TypeStr += "_D";
  return TypeStr;
}

std::string MangleBSCContext::getBSCFunctionMangleName(const FunctionDecl *BFD,
                                                       PrintingPolicy &Policy) {
  Policy.MangleWithSafeQualifier = true;
  std::string FunctionName = BFD->getNameAsString();
  if (const TemplateArgumentList *TArgs =
          BFD->getTemplateSpecializationArgs()) {
    ArrayRef<TemplateArgument> Args = TArgs->asArray();
    std::string TemplateArgsName = getBSCTemplateArgsName(Args, Policy);
    FunctionName += TemplateArgsName;
  }
  Policy.MangleWithSafeQualifier = false;
  return FunctionName;
}

std::string MangleBSCContext::getBSCTypeName(QualType QT,
                                             PrintingPolicy &Policy) {
  std::string typeStr;
  llvm::raw_string_ostream OS(typeStr);
  QT.print(OS, Policy);
  OS.flush();

  if (const BuiltinType *T = QT->getAs<BuiltinType>()) {
    // Eliminate the space character in the name of the BuiltinType type,
    // Avoid generic names such as <long, long double> and <long long,
    // double> having duplicate function names after mangle.
    StringRef BTS = T->getName(Policy);
    std::string BTSS = BTS.str();
    size_t pos = typeStr.find(BTSS);
    if (pos != std::string::npos) {
      std::string replacement = BTS.str();
      replacement.erase(
          std::remove(replacement.begin(), replacement.end(), ' '),
          replacement.end());
      typeStr.replace(pos, BTSS.length(), replacement);
    }
  }

  std::string result;
  result.reserve(typeStr.size() * 2);

  for (char c : typeStr) {
    switch (c) {
    case ' ':
      if (!result.empty()) {
        result += '_';
      }
      break;
    case '*':
      result += 'P';
      break;
    case '(':
      result += "LP";
      break;
    case ')':
      result += "RP";
      break;
    case '[':
      result += "LB";
      break;
    case ']':
      result += "RB";
      break;
    case ',':
      result += "COMMA";
      break;
    default:
      result += c;
      break;
    }
  }

  return result;
}

std::string
MangleBSCContext::getBSCTemplateArgsName(ArrayRef<TemplateArgument> Args,
                                         PrintingPolicy &Policy) {
  std::string ArgsName = "";
  for (size_t i = 0; i < Args.size(); i++) {
    ArgsName = ArgsName + "_" + getBSCArgName(Args[i], Policy);
  }
  return ArgsName;
}

std::string MangleBSCContext::getBSCArgName(const TemplateArgument &TemplateArg,
                                            PrintingPolicy &Policy) {
  std::string ArgName;
  if (TemplateArg.getKind() == clang::TemplateArgument::ArgKind::Type) {
    ArgName = getBSCTypeName(TemplateArg.getAsType(), Policy);
  } else if (TemplateArg.getKind() ==
             clang::TemplateArgument::ArgKind::Integral) {
    llvm::APSInt TemplateInt = TemplateArg.getAsIntegral();
    if (TemplateInt.isNegative())
      ArgName = "n" + std::to_string(-TemplateInt.getExtValue());
    else
      ArgName = std::to_string(TemplateInt.getExtValue());
  }
  return ArgName;
}

#endif

