//===--- SemaBSCSafeZone.cpp - Semantic Analysis for BSC safe zone ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements semantic analysis for BSC safe zone
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/Template.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang;
using namespace sema;

bool Sema::IsInSafeZone() {
  if (!getLangOpts().BSC) {
    return false;
  }
  // The data flow analysis and destructor function process does not check the
  // safe zone rules
  if (BSCDataflowAnalysisFlag) {
    return false;
  }

  if (CurrentInstantiationScope) {
    return getInstantiationSafeZoneSpecifier() == SZ_Safe;
  } else {
    return getCurScope()->getScopeSafeZoneSpecifier() == SZ_Safe;
  }
}

bool Sema::IsSafeZoneIncDecVoidExpr(Expr *E) {
  if (!E || !E->getType()->isVoidType())
    return false;
  const Expr *Stripped = E->IgnoreParenImpCasts();
  // Look through safe(expr) so that safe(a++) is recognized.
  if (const SafeExpr *SE = dyn_cast<SafeExpr>(Stripped))
    Stripped = SE->getSubExpr()->IgnoreParenImpCasts();
  const UnaryOperator *UO = dyn_cast<UnaryOperator>(Stripped);
  if (!UO)
    return false;
  return UnaryOperator::isIncrementDecrementOp(UO->getOpcode());
}

bool Sema::IsSafeFunctionPointerType(QualType Type) {
  if (Type->isFunctionPointerType()) {
    const FunctionProtoType *LSHFuncType =
        Type->getPointeeType()->getAs<FunctionProtoType>();
    if (LSHFuncType && LSHFuncType->getFunSafeZoneSpecifier() == SZ_Safe) {
      return true;
    }
  }
  return false;
}

// Allow or not allow base-type conversion in the safe zone
// 0:void; 1:_Bool; 2:unsigned char; 3:unsigned short; 4: unsigned int;
// 5:unsigned long 6:unsigned long long; 7:char; 8:short; 9:int; 10:long;
// 11:long long; 12 float 13:double; 14:long double;
// sourceType | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14|
// ------------------------------------------------------------------------
// destType.0 | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
// .........1 | N | Y | N | N | N | N | N | N | N | N | N | N | N | N | N |
// .........2 | N | Y | Y | N | N | N | N | N | N | N | N | N | N | N | N |
// .........3 | N | Y | Y | Y | N | N | N | N | N | N | N | N | N | N | N |
// .........4 | N | Y | Y | Y | Y | N | N | N | N | N | N | N | N | N | N |
// .........5 | N | Y | Y | Y | Y | Y | N | N | N | N | N | N | N | N | N |
// .........6 | N | Y | Y | Y | Y | Y | Y | N | N | N | N | N | N | N | N |
// .........7 | N | Y | N | N | N | N | N | Y | N | N | N | N | N | N | N |
// .........8 | N | Y | Y | N | N | N | N | Y | Y | N | N | N | N | N | N |
// .........9 | N | Y | Y | Y | N | N | N | Y | Y | Y | N | N | N | N | N |
// ........10 | N | Y | Y | Y | N | N | N | Y | Y | Y | Y | N | N | N | N |
// ........11 | N | Y | Y | Y | Y | Y | N | Y | Y | Y | Y | Y | N | N | N |
// ........12 | N | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | N | N |
// ........13 | N | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | N |
// ........14 | N | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |

bool Sema::IsSafeBuiltinTypeConversion(BuiltinType::Kind SourceType,
                                       BuiltinType::Kind DestType) {
  static const std::map<BuiltinType::Kind, int> SafeZoneMap = {
      {BuiltinType::Void, 0},       {BuiltinType::Bool, 1},
      {BuiltinType::Char_U, 2},     {BuiltinType::UChar, 2},
      {BuiltinType::UShort, 3},     {BuiltinType::UInt, 4},
      {BuiltinType::ULong, 5},      {BuiltinType::ULongLong, 6},
      {BuiltinType::SChar, 7},      {BuiltinType::Char_S, 7},
      {BuiltinType::Short, 8},      {BuiltinType::Int, 9},
      {BuiltinType::Long, 10},      {BuiltinType::LongLong, 11},
      {BuiltinType::Float, 12},     {BuiltinType::Double, 13},
      {BuiltinType::LongDouble, 14}};
  bool Y = true;
  bool N = false;

  static const bool EnableToConvert[15][15] = {
      {Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y},
      {N, Y, N, N, N, N, N, N, N, N, N, N, N, N, N},
      {N, Y, Y, N, N, N, N, N, N, N, N, N, N, N, N},
      {N, Y, Y, Y, N, N, N, N, N, N, N, N, N, N, N},
      {N, Y, Y, Y, Y, N, N, N, N, N, N, N, N, N, N},
      {N, Y, Y, Y, Y, Y, N, N, N, N, N, N, N, N, N},
      {N, Y, Y, Y, Y, Y, Y, N, N, N, N, N, N, N, N},
      {N, Y, N, N, N, N, N, Y, N, N, N, N, N, N, N},
      {N, Y, Y, N, N, N, N, Y, Y, N, N, N, N, N, N},
      {N, Y, Y, Y, N, N, N, Y, Y, Y, N, N, N, N, N},
      {N, Y, Y, Y, N, N, N, Y, Y, Y, Y, N, N, N, N},
      {N, Y, Y, Y, Y, Y, N, Y, Y, Y, Y, Y, N, N, N},
      {N, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, N, N},
      {N, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, N},
      {N, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y},
  };

  auto ItSource = SafeZoneMap.find(SourceType);
  auto ItDest = SafeZoneMap.find(DestType);
  if (ItSource != SafeZoneMap.end() && ItDest != SafeZoneMap.end()) {
    return EnableToConvert[ItDest->second][ItSource->second];
  }
  return false;
}

bool Sema::IsSafeConstantValueConversion(QualType DestType, Expr *E) {
  // Dependent expressions cannot be fully evaluated at template definition
  // time. Defer the safety check until instantiation when the expression
  // becomes non-dependent.
  if (E->isValueDependent() || E->isInstantiationDependent())
    return true;
  QualType SrcType = E->getType();
  if (SrcType->isIntegralType(Context) && DestType->isIntegralType(Context)) {
    QualType IntTy = E->getType().getUnqualifiedType();
    Expr::EvalResult EVResult;
    bool CstInt = E->EvaluateAsInt(EVResult, Context);
    bool IntSigned = IntTy->hasSignedIntegerRepresentation();
    bool OtherIntSigned = DestType->hasSignedIntegerRepresentation();

    if (CstInt) {
      llvm::APSInt Result = EVResult.Val.getInt();
      unsigned NumBits = IntSigned
                             ? (Result.isNegative() ? Result.getMinSignedBits()
                                                    : Result.getActiveBits())
                             : Result.getActiveBits();

      if (Result.isNegative()) {
        if (OtherIntSigned) {
          // source is negative number, destination is signed type
          return Context.getIntWidth(DestType) >= NumBits;
        } else {
          // source is negative number, destination is unsigned type
          return false;
        }
      } else {
        if (OtherIntSigned) {
          // source is positive number, destination is signed type
          return Context.getIntWidth(DestType) > NumBits;
        } else {
          // source is positive number, destination is unsigned type
          return Context.getIntWidth(DestType) >= NumBits;
        }
      }
    }
  }
  if (DestType->isRealFloatingType()) {
    if (SrcType->isRealFloatingType() && !E->isValueDependent()) {
      // lose of the precision conversion is not allowed
      llvm::APFloat Result(0.0);
      bool CstFloat = E->EvaluateAsFloat(Result, Context);
      if (CstFloat) {
        bool Truncated = true;
        Result.convert(Context.getFloatTypeSemantics(DestType),
                       llvm::APFloat::rmNearestTiesToEven, &Truncated);
        if (!Truncated)
          return true;
      }
    }
    // Integral constant to float: allow if value is exactly representable.
    if (SrcType->isIntegralType(Context) && !E->isValueDependent()) {
      Expr::EvalResult EVResult;
      if (E->EvaluateAsInt(EVResult, Context)) {
        llvm::APSInt IntVal = EVResult.Val.getInt();
        llvm::APFloat FloatVal(Context.getFloatTypeSemantics(DestType));
        FloatVal.convertFromAPInt(
            IntVal, SrcType->hasSignedIntegerRepresentation(),
            llvm::APFloat::rmTowardZero);
        llvm::APSInt ConvertBack(IntVal.getBitWidth(),
                                 !SrcType->hasSignedIntegerRepresentation());
        bool Ignored = false;
        FloatVal.convertToInteger(ConvertBack,
                                 llvm::APFloat::rmNearestTiesToEven, &Ignored);
        if (IntVal == ConvertBack)
          return true;
      }
    }
  }

  return false;
}

/// Select the best matching declaration from heterogeneous redeclarations
/// (functions with both safe and unsafe declarations) based on context and constraints.
/// Used for both function pointer assignment and function call resolution.
///
/// @param CurrentDecl The current function declaration (any redeclaration)
/// @param IsInSafeContext Whether we're in a safe zone
/// @param CheckConstraints Callback to check if a FunctionDecl satisfies constraints
/// @return The best matching FunctionDecl, or nullptr if no match found
FunctionDecl *Sema::SelectDeclForHeterogeneousRedecl(
    FunctionDecl *CurrentDecl, bool IsInSafeContext,
    llvm::function_ref<bool(FunctionDecl *)> CheckConstraints) {

  if (!CurrentDecl || !getLangOpts().BSC)
    return CurrentDecl;

  // Collect safe and unsafe declarations.
  SmallVector<FunctionDecl *, 4> SafeDecls;
  SmallVector<FunctionDecl *, 4> UnsafeDecls;

  for (auto *Redecl : CurrentDecl->redecls()) {
    if (auto *FD = dyn_cast<FunctionDecl>(Redecl)) {
      SafeZoneSpecifier SZS = FD->getSafeZoneSpecifier();
      if (SZS == SZ_Safe)
        SafeDecls.push_back(FD);
      else
        UnsafeDecls.push_back(FD);
    }
  }

  // If all declarations have the same safety level, not heterogeneous.
  if (SafeDecls.empty() || UnsafeDecls.empty())
    return CurrentDecl;

  // Safe context: must use a safe declaration that satisfies constraints.
  if (IsInSafeContext) {
    for (FunctionDecl *SafeFD : SafeDecls) {
      if (CheckConstraints(SafeFD))
        return SafeFD;
    }
    // No safe declaration satisfies constraints.
    return nullptr;
  }

  // Unsafe context: prefer safe declaration if it satisfies constraints.
  for (FunctionDecl *SafeFD : SafeDecls) {
    if (CheckConstraints(SafeFD))
      return SafeFD;
  }

  // No safe declaration matches, try unsafe declarations.
  for (FunctionDecl *UnsafeFD : UnsafeDecls) {
    if (CheckConstraints(UnsafeFD))
      return UnsafeFD;
  }

  // No declaration satisfies constraints.
  return nullptr;
}

/// Check if BSC pointer qualifiers (owned/borrow) are compatible between Dest and Src.
/// For array sources (which decay to raw pointers), owned/borrow dest is rejected.
/// For pointer sources, owned and borrow must match exactly.
static bool AreBSCPointerQualifiersCompatible(QualType Dest, QualType Src,
                                              bool SrcIsArray) {
  if (SrcIsArray) {
    // Arrays decay to raw pointers (no owned/borrow qualifiers).
    // Only raw pointer parameters (no owned/borrow) can match.
    // Exception: String literals CAN match borrow pointers via auto-borrow,
    // but that check is done earlier with access to the Expr*.
    if (Dest.isOwnedQualified() || Dest.isBorrowQualified())
      return false;
    return true;
  }

  // For pointer-to-pointer, owned and borrow must match exactly.
  if (Dest.isOwnedQualified() != Src.isOwnedQualified())
    return false;
  if (Dest.isBorrowQualified() != Src.isBorrowQualified())
    return false;
  return true;
}

/// Check if two pointer types satisfy assignment constraints.
/// This is used for both function calls and function pointer assignments.
/// Returns true if Source can be assigned to Dest considering owned/borrow/const qualifiers.
/// @param AllowImplicitConversions If true, allow implicit conversions for non-pointer types
///                                 (for function calls). If false, require strict compatibility
///                                 (for function pointer assignments).
static bool DoPointerTypesSatisfyAssignmentConstraintsImpl(
    Sema &S, QualType Dest, QualType Src, bool AllowImplicitConversions) {
  bool DestIsPtr = Dest->isPointerType();
  bool SrcIsPtr = Src->isPointerType();
  bool SrcIsArray = Src->isArrayType();

  // Handle array-to-pointer decay for source (e.g., "string" -> const char*)
  if (SrcIsArray && DestIsPtr) {
    SrcIsPtr = true;  // Treat arrays as pointers for this check
  }

  if (Src->isFunctionType() && DestIsPtr &&
      Dest->getPointeeType()->isFunctionType()) {
    Src = S.Context.getPointerType(Src);
    SrcIsPtr = true;
  }

  if (!DestIsPtr && !SrcIsPtr) {
    // For non-pointer types:
    // - Function calls: allow implicit conversions (int->char, etc.)
    // - Function pointer assignments: require strict type compatibility
    if (AllowImplicitConversions) {
      return true;
    } else {
      return S.Context.typesAreCompatible(Dest, Src);
    }
  }

  // nullptr_t is compatible with any pointer type.
  if (DestIsPtr && Src->isNullPtrType())
    return true;

  // If only one is a pointer (after considering array decay), they're incompatible.
  if (DestIsPtr != SrcIsPtr)
    return false;

  // Check BSC-specific qualifiers (owned/borrow) - shared between both modes.
  if (!AreBSCPointerQualifiersCompatible(Dest, Src, SrcIsArray))
    return false;

  // For array sources, pointee compatibility depends on mode.
  if (SrcIsArray) {
    if (AllowImplicitConversions) {
      // Function call context: Clang's normal type checking will handle
      // whether array element type is compatible with destination pointee type.
      return true;
    }
    // Function pointer assignment context: require exact pointee match.
    QualType DestPointee = Dest->getPointeeType();
    QualType SrcPointee = Src->getAsArrayTypeUnsafe()->getElementType();
    return DestPointee.getCanonicalType().getUnqualifiedType() ==
           SrcPointee.getCanonicalType().getUnqualifiedType();
  }

  // For function calls (AllowImplicitConversions=true), we rely on Clang's
  // standard type checking for base type compatibility and only check BSC qualifiers.
  // For function pointer assignments (AllowImplicitConversions=false), we require
  // strict type matching including pointee types.

  if (AllowImplicitConversions) {
    // Function call context: Allow standard C implicit pointer conversions.
    QualType DestPointee = Dest->getPointeeType();
    QualType SrcPointee = Src->getPointeeType();

    // Allow any pointer to void*.
    if (Dest->isVoidPointerType())
      return true;

    // Allow void* to any pointer.
    if (Src->isVoidPointerType())
      return true;

    // Check if pointee types are compatible, ignoring const/volatile qualifiers.
    // Standard C allows const conversions (char* -> const char*), so we check
    // unqualified type compatibility and let Clang handle const checking later.
    if (!S.Context.typesAreCompatible(DestPointee.getUnqualifiedType(),
                                       SrcPointee.getUnqualifiedType()))
      return false;

    return true;
  }

  // Function pointer assignment context: Require strict type matching.
  QualType DestPointee = Dest->getPointeeType();
  QualType SrcPointee = Src->getPointeeType();

  // Pointee types must match exactly.
  if (DestPointee.getCanonicalType().getUnqualifiedType() !=
      SrcPointee.getCanonicalType().getUnqualifiedType())
    return false;

  // Const compatibility: mut -> const is OK, const -> mut is NOT OK.
  // Dest is the target (parameter), Src is the source (argument).
  if (!Dest.isConstQualified() && Src.isConstQualified())
    return false;

  return true;
}

/// Public wrapper for function calls - allows implicit conversions.
bool Sema::DoPointerTypesSatisfyAssignmentConstraints(QualType Dest, QualType Src) {
  return DoPointerTypesSatisfyAssignmentConstraintsImpl(*this, Dest, Src,
                                                         /*AllowImplicitConversions=*/true);
}

/// Public wrapper for function pointer assignment - strict, no implicit conversions.
bool Sema::DoPointerTypesSatisfyAssignmentConstraintsStrict(QualType Dest, QualType Src) {
  return DoPointerTypesSatisfyAssignmentConstraintsImpl(*this, Dest, Src,
                                                         /*AllowImplicitConversions=*/false);
}

/// Helper function: Check if function pointer types satisfy assignment constraints.
/// This checks owned/borrow qualifiers, const compatibility, and type compatibility.
static bool DoesFunctionPointerSatisfyConstraints(Sema &S,
                                                   const FunctionProtoType *DestType,
                                                   const FunctionProtoType *SrcType,
                                                   SourceLocation Loc) {
  // Check return type constraints using strict checking (no implicit conversions).
  QualType DestRetType = DestType->getReturnType();
  QualType SrcRetType = SrcType->getReturnType();
  if (!DoPointerTypesSatisfyAssignmentConstraintsImpl(S, DestRetType, SrcRetType,
                                                       /*AllowImplicitConversions=*/false))
    return false;

  // Check parameter count.
  if (DestType->getNumParams() != SrcType->getNumParams())
    return false;

  // Check each parameter's constraints using strict checking (no implicit conversions).
  for (unsigned i = 0; i < DestType->getNumParams(); ++i) {
    QualType DestParamType = DestType->getParamType(i);
    QualType SrcParamType = SrcType->getParamType(i);
    if (!DoPointerTypesSatisfyAssignmentConstraintsImpl(S, DestParamType, SrcParamType,
                                                         /*AllowImplicitConversions=*/false))
      return false;
  }

  return true;
}

/// Helper function: Select appropriate function declaration for pointer assignment
/// when source is a function with heterogeneous redeclarations (safe + unsafe).
FunctionDecl *
Sema::SelectFunctionDeclForPointerAssignment(Expr *SrcExpr,
                                              const FunctionProtoType *DestFuncType) {
  // Check if SrcExpr is a DeclRefExpr pointing to a FunctionDecl.
  DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(SrcExpr->IgnoreParenImpCasts());
  if (!DRE)
    return nullptr;

  FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl());
  if (!FD)
    return nullptr;

  // Determine if we're in a safe context based on the destination function pointer type.
  SafeZoneSpecifier DestSZS = DestFuncType->getFunSafeZoneSpecifier();
  bool IsInSafeContext = (DestSZS == SZ_Safe);

  // Use the generic selector with a lambda to check function pointer constraints.
  SourceLocation Loc = SrcExpr->getBeginLoc();
  auto CheckConstraints = [&](FunctionDecl *CandidateFD) -> bool {
    const FunctionProtoType *CandidateType =
        CandidateFD->getType()->getAs<FunctionProtoType>();
    return DoesFunctionPointerSatisfyConstraints(*this, DestFuncType, CandidateType, Loc);
  };

  return SelectDeclForHeterogeneousRedecl(FD, IsInSafeContext, CheckConstraints);
}

bool Sema::IsSafeFunctionPointerTypeCast(QualType DestType, Expr *SrcExpr) {
  if (!DestType->isFunctionPointerType()) {
    return true;
  }
  if (!SrcExpr->getType()->isFunctionPointerType() &&
      !SrcExpr->getType()->isFunctionType()) {
    return true;
  }
  // bsc desugar cast expression is a safe conversion.
  if (SrcExpr->IsDesugaredCastExpr) {
    return true;
  }
  const FunctionProtoType *LSHFuncType = DestType->getAs<PointerType>()
                                             ->getPointeeType()
                                             ->getAs<FunctionProtoType>();
  const FunctionProtoType *RSHFuncType =
      SrcExpr->getType()->isFunctionPointerType()
          ? SrcExpr->getType()
                ->getAs<PointerType>()
                ->getPointeeType()
                ->getAs<FunctionProtoType>()
          : SrcExpr->getType()->getAs<FunctionProtoType>();

  // For heterogeneous function redeclarations (functions with both safe and
  // unsafe declarations), select the appropriate declaration based on the
  // destination function pointer type and assignment constraints.
  FunctionDecl *SelectedFD =
      SelectFunctionDeclForPointerAssignment(SrcExpr, LSHFuncType);
  if (SelectedFD) {
    // Update RSHFuncType to the selected declaration's function type.
    RSHFuncType = SelectedFD->getType()->getAs<FunctionProtoType>();
  } else {
    // SelectFunctionDeclForPointerAssignment returns nullptr only when the
    // source is a heterogeneous redeclaration and no decl satisfies the
    // destination constraints.
    DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(SrcExpr->IgnoreParenImpCasts());
    if (DRE) {
      if (FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
        Diag(SrcExpr->getBeginLoc(),
             diag::err_bsc_no_matching_heterogeneous_function)
            << FD->getDeclName();
        return false;
      }
    }
  }

  // conversion to an unsafe type is allowed in the unsafe zone
  // only need to care about the safe zone or safe type
  if (!IsInSafeZone() && LSHFuncType->getFunSafeZoneSpecifier() != SZ_Safe) {
    return true;
  }

  // Function pointer assignment rules (Manual section 9):
  // - safe -> unsafe: allowed (widening, safe functions can be used in
  //                   unsafe contexts)
  // - unsafe -> safe: forbidden (narrowing, loss of safety guarantee)
  if (LSHFuncType->getFunSafeZoneSpecifier() !=
      RSHFuncType->getFunSafeZoneSpecifier()) {
    SafeZoneSpecifier DestSZS = LSHFuncType->getFunSafeZoneSpecifier();
    SafeZoneSpecifier SrcSZS = RSHFuncType->getFunSafeZoneSpecifier();

    // Assigning unsafe function to safe function pointer is forbidden.
    if (DestSZS == SZ_Safe &&
        (SrcSZS == SZ_Unsafe || SrcSZS == SZ_None)) {
      // Emit error with proper type strings that include safe/unsafe specifiers
      Diag(SrcExpr->getBeginLoc(), diag::err_unsafe_fun_cast)
          << Context.getPointerType(QualType(RSHFuncType, 0)) << DestType;
      Diag(SrcExpr->getBeginLoc(), diag::note_unsafe_to_safe_function_pointer);
      return false;
    }

    // Assigning safe function to unsafe function pointer is allowed.
    if ((DestSZS == SZ_Unsafe || DestSZS == SZ_None) &&
        SrcSZS == SZ_Safe) {
      // Type compatibility is already checked below.
    }
  }

  // Check return type constraints using the constraint-aware helper.
  if (!DoesFunctionPointerSatisfyConstraints(*this, LSHFuncType, RSHFuncType,
                                              SrcExpr->getBeginLoc())) {
    // Emit error with proper type strings that include the actual function signatures
    Diag(SrcExpr->getBeginLoc(), diag::err_unsafe_fun_cast)
        << Context.getPointerType(QualType(RSHFuncType, 0)) << DestType;
    return false;
  }

  return true;
}

namespace {

/// Check whether the destination enum E2 contains all enumerator values of
/// source enum E1. Only then is explicit cast from E1 to E2 allowed in safe zone.
static bool EnumDestContainsAllValuesOfSource(const EnumDecl *DestED,
                                              const EnumDecl *SrcED,
                                              ASTContext &Context) {
  const EnumDecl *SrcDef = SrcED->getDefinition();
  const EnumDecl *DestDef = DestED->getDefinition();
  if (!SrcDef || !DestDef)
    return false;
  // Collect all values of the destination enum.
  llvm::SmallVector<llvm::APSInt, 8> DestValues;
  for (const auto *D : DestDef->enumerators()) {
    const auto *ECD = dyn_cast<EnumConstantDecl>(D);
    if (ECD)
      DestValues.push_back(ECD->getInitVal());
  }
  // Check every source enumerator value is in the destination set.
  for (const auto *D : SrcDef->enumerators()) {
    const auto *ECD = dyn_cast<EnumConstantDecl>(D);
    if (!ECD)
      continue;
    const llvm::APSInt &Val = ECD->getInitVal();
    bool Found = false;
    for (const llvm::APSInt &DestVal : DestValues) {
      if (Val == DestVal) {
        Found = true;
        break;
      }
    }
    if (!Found)
      return false;
  }
  return true;
}

/// Get the type to use in diagnostics for a source expression. Looks through
/// SafeExpr, ImplicitCastExpr, ParenExpr; for enum constants and enum-typed
/// variables returns the enum type so the message shows "enum X" instead of "int".
static QualType getDiagnosticSourceType(Expr *E, ASTContext &Context,
                                        bool &HasImplicitCast) {
  HasImplicitCast = false;
  if (!E)
    return QualType();
  E = E->IgnoreParens();
  while (true) {
    if (auto *SE = dyn_cast<SafeExpr>(E)) {
      E = SE->getSubExpr();
      continue;
    }
    if (auto *ICE = dyn_cast<ImplicitCastExpr>(E)) {
      E = ICE->getSubExpr();
      HasImplicitCast = true;
      continue;
    }
    if (auto *PE = dyn_cast<ParenExpr>(E)) {
      E = PE->getSubExpr();
      continue;
    }
    break;
  }
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    if (EnumConstantDecl *ECD =
            dyn_cast<EnumConstantDecl>(DRE->getDecl())) {
      EnumDecl *Enum = cast<EnumDecl>(ECD->getDeclContext());
      return Context.getTypeDeclType(Enum);
    }
    if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      QualType T = VD->getType();
      if (T->isEnumeralType())
        return T;
    }
  }
  return E->getType();
}

DeclRefExpr *getDeclRefExprForEnumCoversion(Expr *E) {
  if (!E) {
    return nullptr;
  }
  switch (E->getStmtClass()) {
  case Expr::ParenExprClass:
    return getDeclRefExprForEnumCoversion(cast<ParenExpr>(E)->getSubExpr());
  case Expr::SafeExprClass:
    return getDeclRefExprForEnumCoversion(cast<SafeExpr>(E)->getSubExpr());
  case Expr::ImplicitCastExprClass:
    return getDeclRefExprForEnumCoversion(
        cast<ImplicitCastExpr>(E)->getSubExpr());
  case Expr::CStyleCastExprClass:
    return getDeclRefExprForEnumCoversion(
        cast<CStyleCastExpr>(E)->getSubExpr());
  case Expr::DeclRefExprClass:
    return dyn_cast<DeclRefExpr>(E);
  case Expr::BinaryOperatorClass: {
    auto *BO = cast<BinaryOperator>(E);
    if (BO->getOpcode() == BO_Comma) {
      return getDeclRefExprForEnumCoversion(cast<BinaryOperator>(E)->getRHS());
    }
    if (BO->getOpcode() == BO_Assign) {
      // (e1 = ONE) or (e1 = e2): RHS is the value produced, LHS is enum var
      if (DeclRefExpr *DRE = getDeclRefExprForEnumCoversion(BO->getRHS()))
        return DRE;
      return getDeclRefExprForEnumCoversion(BO->getLHS());
    }
    break;
  }
  default:
    break;
  }
  return nullptr;
}

/// Examine whether an expression is evaluating to a boolean value,
/// so that implicit type casting restriction could be loosen in safezone
bool IsBooleanEvaluation(const Expr *E) {
  E = E->IgnoreParenImpCasts();

  if (E->getType()->isBooleanType())
    return true;

  if (const auto *UO = dyn_cast<UnaryOperator>(E))
    return UO->getOpcode() == UO_LNot;

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    if (BO->getOpcode() == BO_Comma)
      return IsBooleanEvaluation(BO->getRHS());
    return BO->isLogicalOp() || BO->isComparisonOp();
  }

  if (const auto *CO = dyn_cast<ConditionalOperator>(E)) {
    return IsBooleanEvaluation(CO->getTrueExpr()) &&
           IsBooleanEvaluation(CO->getFalseExpr());
  }

  return false;
}

/// Returns whether conversions from `SrcCanPtr` to `DstCanPtr` is allowed
/// in safezone.
/// Assumes both `SrcCanPtr` and `DstCanPtr` are canonical pointer types.
bool IsSafePointerConversion(const QualType SrcCanPtr,
                             const QualType DstCanPtr) {
  const QualType SrcPointee = SrcCanPtr->getPointeeType();
  QualType DstPointee = DstCanPtr->getPointeeType();
  // allow `const T *borrow` <- `T *borrow`
  // mirrors the logic in Sema::CheckBorrowQualTypeAssignment
  if (DstPointee.isConstQualified() && DstCanPtr.isBorrowQualified() &&
      !SrcPointee.isConstQualified() && SrcCanPtr.isBorrowQualified()) {
    DstPointee.removeLocalConst();
    if (SrcPointee == DstPointee)
      return true;
  }
  // allow `void *owned` <- `T *owned`
  if (SrcCanPtr.isOwnedQualified() && DstCanPtr->isVoidPointerType() &&
      DstCanPtr.isOwnedQualified())
    return true;

  // fallback: disallow conversion between different pointer types
  return SrcCanPtr == DstCanPtr;
}

} // namespace

bool Sema::IsSafeConversion(QualType DestType, Expr *E, bool IsExplicitCast) {
  // check function pointer Type in 'IsSafeFunctionPointerTypeCast'
  if (DestType->isFunctionPointerType()) {
    return true;
  }
  // only check in the safe zone
  if (!IsInSafeZone()) {
    return true;
  }

  // Init any pointer (raw, owned, or borrow) by nullptr is allowed in the safezone
  if (DestType->isPointerType()) {
    if (isa<CXXNullPtrLiteralExpr>(E->IgnoreParens()))
      return true;

    // Allow initializing 'char*' pointers with string literals.
    QualType Pointee = DestType->getPointeeType();
    if (Pointee->isCharType()) {
      // Check if E is a legal string(char,const[],stringLiteral,
      // possibly through parens/casts/ternary)
      if (isSafeZoneStringType(E))
        return true;
    }
  }

  // Strip implicit casts so we recurse into ternaries whose result was promoted
  // (e.g. enum E -> unsigned int) to int.
  if (const ConditionalOperator *Exp =
          dyn_cast<ConditionalOperator>(E->IgnoreParenImpCasts())) {
    return IsSafeConversion(DestType, Exp->getTrueExpr(), IsExplicitCast) &&
           IsSafeConversion(DestType, Exp->getFalseExpr(), IsExplicitCast);
  }
  bool IsSafeBehavior = true;
  bool IsExplicitConversionAllowed = false;
  QualType SrcType = E->getType();
  if (IsTraitExpr(E)) {
    SrcType = CompleteTraitType(SrcType);
  }
  // conversion from non trait pointer type to trait pointer type is allowed
  if (DestType->isTraitPointerType() && !SrcType->isTraitPointerType()) {
    return true;
  }
  if (SrcType->isPointerType() && DestType->isPointerType()) {
    QualType SrcCanType = SrcType.getCanonicalType();
    QualType DestCanType = DestType.getCanonicalType();
    IsSafeBehavior = IsSafePointerConversion(SrcCanType, DestCanType);
  } else if (SrcType->isArrayType() && DestType->isPointerType()) {
    // Array-to-pointer decay: check compatibility after canonical decay.
    QualType SrcDecayedCanType =
        Context.getArrayDecayedType(SrcType).getCanonicalType();
    QualType DestCanType = DestType.getCanonicalType();
    IsSafeBehavior = IsSafePointerConversion(SrcDecayedCanType, DestCanType);
  } else if (SrcType->isPointerType() || DestType->isPointerType()) {
    // conversion from pointer to non-pointer or non-pointer to pointer is not
    // allowed
    IsSafeBehavior = false;
  } else {
    // Forbid float to integer conversion in safe zone (even with explicit cast).
    if (DestType->isIntegerType() && SrcType->isRealFloatingType()) {
      IsSafeBehavior = false;
    }
    const auto *SBT =
        dyn_cast<BuiltinType>(SrcType->getUnqualifiedDesugaredType());
    const auto *DBT =
        dyn_cast<BuiltinType>(DestType->getUnqualifiedDesugaredType());
    if (SBT && DBT) {
      if (SBT->getKind() != DBT->getKind()) {
        // Allow explicit C-style casts, but forbid implicit conversions
        if (!IsExplicitCast) {
          // conversion from high-precision to low-precision is not allowed
          // conversion from wide range to narrow range is not allowed
          if (!IsSafeBuiltinTypeConversion(SBT->getKind(), DBT->getKind())) {
            if (!DestType->isIntegerType() || !IsBooleanEvaluation(E)) {
              IsSafeBehavior = false;
              if (!SrcType->isVoidType())
                IsExplicitConversionAllowed = true; // arithmetic explicit cast OK
            }
          }
        }
      }
    }
    // Implicit integer to floating is forbidden unless constant fits (manual rule 6).
    if (!IsExplicitCast && SrcType->isIntegerType() &&
        DestType->isRealFloatingType() &&
        !IsSafeConstantValueConversion(DestType, E)) {
      IsSafeBehavior = false;
      IsExplicitConversionAllowed = true; // arithmetic explicit cast OK
    }
    // conversion const value is allowed, if the destination type can embrace it
    if (!DestType->isEnumeralType() && !SrcType->isEnumeralType() &&
        IsSafeConstantValueConversion(DestType, E)) {
      IsSafeBehavior = true;
    }

    if (DestType->isEnumeralType() || SrcType->isEnumeralType()) {
      if (IsExplicitCast) {
        // Explicit enum to enum: only allow when E2 contains all values of E1.
        if (SrcType->isEnumeralType() && DestType->isEnumeralType()) {
          if (SrcType.getCanonicalType() == DestType.getCanonicalType()) {
            IsSafeBehavior = true;
          } else {
            const EnumType *SrcET = SrcType->getAs<EnumType>();
            const EnumType *DestET = DestType->getAs<EnumType>();
            if (SrcET && DestET &&
                EnumDestContainsAllValuesOfSource(DestET->getDecl(),
                                                  SrcET->getDecl(), Context)) {
              IsSafeBehavior = true;
            } else {
              IsSafeBehavior = false;
            }
          }
        } else if (DestType->isEnumeralType() && !SrcType->isEnumeralType()) {
          // In C, enum constants and sometimes enum variables have type int.
          // Allow (enum E)x when x is an enum constant or variable and dest
          // enum contains all values of the source enum.
          IsSafeBehavior = false;
          const EnumType *DestET = DestType->getAs<EnumType>();
          if (DestET && SrcType->isIntegerType()) {
            EnumDecl *SrcED = nullptr;
            if (auto *DRE = getDeclRefExprForEnumCoversion(E)) {
              if (EnumConstantDecl *ECD =
                      dyn_cast<EnumConstantDecl>(DRE->getDecl())) {
                SrcED = cast<EnumDecl>(ECD->getDeclContext());
              } else if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
                QualType VarTy = VD->getType();
                if (const EnumType *VarET = VarTy->getAs<EnumType>())
                  SrcED = VarET->getDecl();
              }
            }
            if (SrcED &&
                EnumDestContainsAllValuesOfSource(DestET->getDecl(), SrcED,
                                                  Context)) {
              IsSafeBehavior = true;
            }
          }
        }
      } else {
        IsSafeBehavior = false;
        // Explicit cast would be allowed for enum-to-enum (E1 -> E2)
        // when E2 contains all values of E1 (or same).
        if (SrcType->isEnumeralType() && DestType->isEnumeralType()) {
          const EnumType *SrcET = SrcType->getAs<EnumType>();
          const EnumType *DestET = DestType->getAs<EnumType>();
          if (SrcET && DestET &&
              (SrcType.getCanonicalType() == DestType.getCanonicalType() ||
               EnumDestContainsAllValuesOfSource(DestET->getDecl(),
                                                SrcET->getDecl(), Context)))
            IsExplicitConversionAllowed = true;
        }
        // Same enum type: allowed.
        if (SrcType.getCanonicalType() == DestType.getCanonicalType()) {
          IsSafeBehavior = true;
        }
        if (auto *DRE = getDeclRefExprForEnumCoversion(E)) {
          // Enum constant to same enum type: allowed.
          if (EnumConstantDecl *ECD =
                  dyn_cast<EnumConstantDecl>(DRE->getDecl())) {
            EnumDecl *Enum = cast<EnumDecl>(ECD->getDeclContext());
            QualType EnumTy = Context.getTypeDeclType(Enum);
            if (DestType.getCanonicalType() == EnumTy.getCanonicalType()) {
              IsSafeBehavior = true;
            }
          }
          // Enum variable of same enum type: allowed (e.g. ternary (cond)?e1:e2
          // branches where e1,e2 are enum vars promoted to int).
          if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
            QualType VarTy = VD->getType();
            if (const EnumType *VarET = VarTy->getAs<EnumType>()) {
              const EnumType *DestET = DestType->getAs<EnumType>();
              if (DestET && SrcType->isIntegerType() &&
                  (VarTy.getCanonicalType() == DestType.getCanonicalType() ||
                   EnumDestContainsAllValuesOfSource(DestET->getDecl(),
                                                    VarET->getDecl(), Context))) {
                IsSafeBehavior = true;
              }
            }
          }
        }
        // Enum to underlying integer type: implicit conversion allowed.
        if (SrcType->isEnumeralType() && DestType->isIntegerType()) {
          const EnumType *SrcET = SrcType->getAs<EnumType>();
          if (SrcET) {
            EnumDecl *ED = SrcET->getDecl();
            QualType Underlying = ED->getIntegerType();
            QualType DestCanon = DestType.getUnqualifiedType().getCanonicalType();
            if (!Underlying.isNull()) {
              QualType UnderCanon =
                  Underlying.getUnqualifiedType().getCanonicalType();
              if (DestCanon == UnderCanon)
                IsSafeBehavior = true;
              else if (Context.getTypeSize(DestType) >=
                       Context.getTypeSize(Underlying))
                IsSafeBehavior = true;
            } else if (Context.hasSameUnqualifiedType(DestType, Context.IntTy)) {
              IsSafeBehavior = true;
            }
          }
        }
      }
    }
  }

  if (!IsSafeBehavior) {
    if (!IsExplicitCast && IsExplicitConversionAllowed) {
      Diag(E->getExprLoc(), diag::err_unsafe_implicit_cast) << SrcType
                                                            << DestType;
    } else {
      Diag(E->getExprLoc(), diag::err_unsafe_cast) << SrcType << DestType;
      if (SrcType->isVoidType() && IsSafeZoneIncDecVoidExpr(E))
        Diag(E->getExprLoc(), diag::note_inc_dec_void_in_safe_zone);
    }
    bool hasImplicitCast = false;
    QualType DiagSrcType = getDiagnosticSourceType(E, Context, hasImplicitCast);
    if (!DiagSrcType.isNull() && DiagSrcType != SrcType) {
      if (hasImplicitCast ||
          (SrcType->isIntegerType() && DiagSrcType->isEnumeralType())) {
        Diag(E->getExprLoc(), diag::note_unsafe_cast_implicit_conversion)
            << SrcType << DiagSrcType;
      }
    }
  }
  return IsSafeBehavior;
}

bool Sema::IsUnsafeType(QualType Type) {
  llvm::SmallPtrSet<const clang::Type *, 16> Visited;
  llvm::SmallVector<QualType, 16> Stack;

  Stack.push_back(Type);
  while (!Stack.empty()) {
    QualType CurType = Stack.pop_back_val();

    if (CurType.isNull())
      continue;
    const clang::Type *TyPtr = CurType.getTypePtr();
    if (!Visited.insert(TyPtr).second)
      continue;

    // va_list (__builtin_va_list) is unsafe in safe zones
    if (const BuiltinType *BT = CurType->getAs<BuiltinType>()) {
      if (BT->getKind() == BuiltinType::BuiltinFn) {
        return true;
      }
    }
    if (CurType == Context.getBuiltinVaListType()) {
      return true;
    }

    if (CurType->isPointerType()) {
      Stack.push_back(CurType->getPointeeType());
    }
    if (CurType->isOwnedStructureType()) {
      continue;
    }
    if (CurType->isStructureType()) {
      if (const auto *RT = CurType->getAs<RecordType>()) {
        RecordDecl *RD = RT->getDecl();
        for (RecordDecl::field_iterator i = RD->field_begin(),
                                        e = RD->field_end();
             i != e; ++i) {
          Stack.push_back(i->getType());
        }
      }
    }
    if (CurType->isArrayType()) {
      Stack.push_back(CurType->castAsArrayTypeUnsafe()->getElementType());
    }
  }
  return false;
}

// Check if a type can be left uninitialized in safe zones.
// Returns true if the type is allowed to be uninitialized.
// owned struct and struct with owned struct fields must be initialized.
bool Sema::CanBeUninitializedInSafeZone(QualType Type) {
  if (Type.isNull())
    return false;

  QualType CanonType = Type.getCanonicalType();

  if (CanonType->isOwnedStructureType())
    return false;

  // Recursively check struct/union fields for owned structs.
  if (CanonType->isStructureType() || CanonType->isUnionType()) {
    llvm::SmallPtrSet<const clang::Type *, 16> Visited;
    llvm::SmallVector<QualType, 16> Stack;
    Stack.push_back(CanonType);

    while (!Stack.empty()) {
      QualType CurType = Stack.pop_back_val().getCanonicalType();
      if (CurType.isNull())
        continue;
      if (!Visited.insert(CurType.getTypePtr()).second)
        continue;

      if (CurType->isOwnedStructureType())
        return false;

      if (CurType->isStructureType() || CurType->isUnionType()) {
        if (const auto *RT = CurType->getAs<RecordType>()) {
          for (const auto *FD : RT->getDecl()->fields())
            Stack.push_back(FD->getType());
        }
      }

      if (CurType->isArrayType())
        Stack.push_back(cast<ArrayType>(CurType)->getElementType());
    }
    return true;
  }

  // Array: check element type.
  if (CanonType->isArrayType())
    return CanBeUninitializedInSafeZone(
        Context.getBaseElementType(CanonType));

  return true;
}

void Sema::DiagnoseInvalidMemberAccessExprInSafeZone(SourceLocation OpLoc,
                                                     tok::TokenKind Kind,
                                                     QualType T) {
  if (!IsInSafeZone())
    return;

  switch (Kind) {
  case tok::arrow: {
    if (!T.isNull() && T->isPointerType()) {
      // Check for raw pointer (not owned/borrow)
      if (!(T.getCanonicalType().isOwnedQualified() || T.getCanonicalType().isBorrowQualified()))
        Diag(OpLoc, diag::err_unsafe_action)
            << "'->' operator used by raw pointer type";
      // Check if pointing to union type
      else if (T->getPointeeType()->isUnionType())
        Diag(OpLoc, diag::err_union_member_access_in_safe_zone);
    }
    break;
  }
  case tok::period: {
    if (!T.isNull() && T->isUnionType())
      Diag(OpLoc, diag::err_union_member_access_in_safe_zone);
    break;
  }
  default:
    break;
  }
}

void Sema::DiagnoseInvalidUnaryExprInSafeZone(SourceLocation OpLoc,
                                              UnaryOperatorKind Opc,
                                              QualType T,
                                              Expr *InputExpr) {
  if (!IsInSafeZone())
    return;

  switch (Opc) {
  case UO_PreInc:
  case UO_PostInc:
    if (T->isPointerType()) {
      Diag(OpLoc, diag::err_unsafe_action) << "'++' operator";
    }
    break;
  case UO_PreDec:
  case UO_PostDec:
    if (T->isPointerType()) {
      Diag(OpLoc, diag::err_unsafe_action) << "'--' operator";
    }
    break;
  case UO_AddrOf: {
    if (T.isNull() || !T->isFunctionType()) {
      Diag(OpLoc, diag::err_unsafe_action) << "'&' operator";
    }
    break;
  }
  case UO_Deref: {
    if (!T.isNull() && T->isPointerType() &&
        !T.getCanonicalType().isOwnedQualified() &&
        !T.getCanonicalType().isBorrowQualified()) {
      // Allow dereferencing function pointers.
      if (T->isFunctionPointerType())
        break;
      // Allow dereferencing string literals, __FUNCTION__, and ternary string
      // expressions for borrow conversion.
      if (!IsStringLiteralExpr(InputExpr)) {
        Diag(OpLoc, diag::err_unsafe_action) << "'*' operator";
      }
    }
    break;
  }
  default:
    break;
  }
}

#if ENABLE_BSC_FUTURE
void Sema::DiagnoseInvalidArraySubscriptInSafeZone(SourceLocation LBracLoc,
                                                   QualType BaseType) {
  if (!IsInSafeZone())
    return;

  // Array subscript on a raw pointer is equivalent to dereferencing,
  // so we apply the same check as UO_Deref
  if (!BaseType.isNull() && BaseType->isPointerType() &&
      !BaseType.getCanonicalType().isOwnedQualified() &&
      !BaseType.getCanonicalType().isBorrowQualified()) {
    Diag(LBracLoc, diag::err_unsafe_action) << "'[]' operator";
  }
}
#endif

void Sema::PushInsSafeZone(SafeZoneSpecifier SafeZoneSpec) {
  getCurFunction()->InsCompoundSafeZone.push_back(
      InsCompoundSafeZoneInfo(SafeZoneSpec));
}

void Sema::PopInsSafeZone() {
  FunctionScopeInfo *CurFunction = getCurFunction();
  assert(!CurFunction->InsCompoundSafeZone.empty() && "mismatched push/pop");

  CurFunction->InsCompoundSafeZone.pop_back();
}

sema::InsCompoundSafeZoneInfo &Sema::getCurInsCompoundSafeZone() const {
  return getCurFunction()->InsCompoundSafeZone.back();
}

void Sema::setInstantiationSafeZoneSpecifier(SafeZoneSpecifier SZ) {
  if (getCurFunction()) {
    if (getCurFunction()->InsCompoundSafeZone.size() == 0) {
      if (CurrentInstantiationScope)
        CurrentInstantiationScope->setScopeSafeZoneSpecifier(SZ);
    } else {
      PopInsSafeZone();
      PushInsSafeZone(SZ);
    }
  }
}

SafeZoneSpecifier Sema::getInstantiationSafeZoneSpecifier() {
  SafeZoneSpecifier SafeZoneSpec = SZ_None;
  if (getCurFunction()) {
    if (getCurFunction()->InsCompoundSafeZone.size() == 0) {
      if (CurrentInstantiationScope)
        SafeZoneSpec = CurrentInstantiationScope->getScopeSafeZoneSpecifier();
    } else {
      SafeZoneSpec =
          getCurInsCompoundSafeZone().getInsCompoundSafeZoneSpecifier();
    }
  }
  return SafeZoneSpec;
}
#endif
