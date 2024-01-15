#if ENABLE_BSC
#include "clang/AST/Type.h"
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
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check) << ownedQualified << Env;
  } else if (T.getCanonicalType().isOwnedQualified() && T.getTypePtr()->getAs<TypedefType>()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check) << ownedTypedef << Env << T;
  } else if (T.getCanonicalType().getTypePtr()->hasOwnedFields()) {
    Diag(ErrLoc, diag::err_owned_inderictOwned_type_check) << ownedFields << Env << T;
  }
}

bool Sema::CheckOwnedDecl(SourceLocation ErrLoc, QualType T) {
  // owned union check
  if (T.getCanonicalType()->isUnionType()) {
    Diag(ErrLoc, diag::err_owned_decl) << T;
    return false;
  }
  // owned trait check
  QualType BaseType = T;
  while (!BaseType.isNull() && BaseType->isPointerType()) {
    BaseType = BaseType->getPointeeType();
  }
  if (!BaseType.isNull() && BaseType.getCanonicalType()->isTraitType()) {
    Diag(ErrLoc, diag::err_owned_decl) << T;
    return false;
  }
  return true;
}

bool Sema::CheckOwnedQualTypeCStyleCast(QualType LHSType, QualType RHSType) {
  QualType RHSCanType = RHSType.getCanonicalType();
  QualType LHSCanType = LHSType.getCanonicalType();
  bool IsSameType = (LHSCanType.getTypePtr() == RHSCanType.getTypePtr());
  const auto *LHSPtrType = LHSType->getAs<PointerType>();
  const auto *RHSPtrType = RHSType->getAs<PointerType>();
  bool IsPointer = LHSPtrType && RHSPtrType;

  // legal cases:
  // 'int owned'    <->  'int owned'                      // same type same owned
  // 'int'          <->  'owned int'                      // same type diff owned
  // 'owned int*'   <->  'int*'                           // pointer to 'same type diff owned'
  // 'int**'        <->  'owned int* const owned * owned' // multi-layer pointer same type diff owned or const
  // 'int**'        <->  'int* const * owned'             // multi-layer pointer same type diff owned or const
  // 'float* owned' <->  'void* owned'                    // pointer to diff type but voidpointer same owned
  // 'float* owned' <->  'void*'                          // diff type but voidpointer diff owned
  // 'int** owned'' <->  const int** owned'

  // illegal cases:
  // 'float owned'  <->  'int owned'    // diff type
  // 'float* owned' <->  'int* owned'   // diff type same owned
  // 'owned float*' <->  'owned int*'   // pointer to diff type same owned
  // 'int'          <->  'void* owned'  //

  if (IsSameType) {
    return true;
  }
  if (IsPointer) {
    if (LHSCanType.getTypePtr()->isVoidPointerType() || RHSCanType.getTypePtr()->isVoidPointerType()) {
      return true;
    } else {
      return CheckOwnedQualTypeCStyleCast(LHSPtrType->getPointeeType(), RHSPtrType->getPointeeType());
    }
  }
  return false;
}

bool Sema::CheckOwnedQualTypeCStyleCast(QualType LHSType, QualType RHSType, SourceLocation RLoc) {
  if (!CheckOwnedQualTypeCStyleCast(LHSType, RHSType)) {
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

  // owned to owned cases:
  // int* owned  <->  int* owned   // legal
  // int* owned  <->  float* owned // illegal
  // const int** owned  <->  int** owned  // legal
  // unOwned to unOwned cases:
  // owned int* owned *  <->  owned int**  // illegal
  // owned int* const *  <->  owned int**  // legal
  if (LHSCanType.isOwnedQualified() == RHSCanType.isOwnedQualified()) {
    if (IsSameType) {
      return true;
    }
    if (!IsPointer) {
      return false;
    } else {
      return CheckOwnedQualTypeAssignment(LHSPtrType->getPointeeType(), RHSPtrType->getPointeeType(), RLoc);
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
  if (RetType.isOwnedQualified() || RetType->hasOwnedFields()) {
    std::string ExprString;
    llvm::raw_string_ostream ExprStream(ExprString);
    E->printPretty(ExprStream, nullptr, clang::PrintingPolicy(getLangOpts()));
    Diag(E->getBeginLoc(), diag::err_owned_temporary_memLeak) << ExprStream.str();
    return true;
  }
  return false;
}
#endif
