//===- BSCNullabilityCheck.cpp - Nullability Check for Source CFGs -*- BSC--//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BSC Pointer Nullability Check for source-level CFGs.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/BSC/ExprBSC.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/Analyses/BSC/BSCNullabilityCheck.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/FlowSensitive/DataflowWorklist.h"
#include "clang/Basic/Builtins.h"
#include "llvm/ADT/DenseMap.h"

using namespace clang;
using namespace std;

// DefNullability is determined when a pointer is declared:
//   1. has nullability specifier, DefNullability depends on nullability
//   specifier.
//   2. has no nullability specifier, DefNullability depends on type:
//      1) raw pointer is nullable by default,
//      2) owned or borrow pointer is nonnull by default.
// Pointer with nullable DefNullability have PathNullability,
// which will change with control flow.
using StatusVD = llvm::DenseMap<VarDecl *, NullabilityKind>;
using FieldPath = std::pair<VarDecl *, std::string>;
using StatusFP = std::map<FieldPath, NullabilityKind>;

class NullabilityCheckImpl {
public:
  llvm::DenseMap<const CFGBlock *, StatusVD> BlocksBeginStatusVD;
  llvm::DenseMap<const CFGBlock *, StatusVD> BlocksEndStatusVD;

  llvm::DenseMap<const CFGBlock *, StatusFP> BlocksBeginStatusFP;
  llvm::DenseMap<const CFGBlock *, StatusFP> BlocksEndStatusFP;
  // For branch statement with condition, such as IfStmt, WhileStmt,
  // true branch and else branch may have different status.
  // For example:
  // @code
  //     int *p = nullptr;
  //     if (p != nullptr) {
  //         *p = 5;   // p is NonNull, so deref p is Ok
  //     } else {
  //         *p = 10;  // p is Nullable, so deref p is forbidden
  //     }
  // @endcode
  // CFG is:
  //        B4(has condition as terminitor)
  //    true /      \ false
  //        B3      B2
  //          \    /
  //            B1
  // BlocksConditionStatus records condition status:
  // Key is current BB, value is condition status passed from pred BB to current
  // BB, for this example, BlocksConditionStatusVD will be:
  // B3 : { B4 : p NonNull }  B2 : { B4 : p Nullable }
  llvm::DenseMap<
      const CFGBlock *,
      llvm::DenseMap<const CFGBlock *, std::pair<VarDecl *, NullabilityKind>>>
      BlocksConditionStatusVD;
  llvm::DenseMap<
      const CFGBlock *,
      llvm::DenseMap<const CFGBlock *, std::pair<FieldPath, NullabilityKind>>>
      BlocksConditionStatusFP;

  StatusVD mergeVD(StatusVD statusA, StatusVD statusB);
  StatusFP mergeFP(StatusFP statusA, StatusFP statusB);

  std::pair<StatusVD, StatusFP>
  runOnBlock(const CFGBlock *block, StatusVD statusVD, StatusFP statusFP,
             NullabilityCheckDiagReporter &reporter, ASTContext &ctx,
             const FunctionDecl &fd, ParentMap &PM);
  void initStatus(const CFG &cfg, ASTContext &ctx);

  NullabilityCheckImpl()
      : BlocksBeginStatusVD(0), BlocksEndStatusVD(0), BlocksBeginStatusFP(0),
        BlocksEndStatusFP(0), BlocksConditionStatusVD(0),
        BlocksConditionStatusFP(0) {}
};

//===----------------------------------------------------------------------===//
// Dataflow computation.
//===----------------------------------------------------------------------===//
namespace {
class TransferFunctions : public StmtVisitor<TransferFunctions> {
  NullabilityCheckImpl &NCI;
  const CFGBlock *Block;
  StatusVD &CurrStatusVD;
  StatusFP &CurrStatusFP;
  NullabilityCheckDiagReporter &Reporter;
  ASTContext &Ctx;
  const FunctionDecl &Fd;
  ParentMap &PM;

public:
  TransferFunctions(NullabilityCheckImpl &nci, const CFGBlock *block,
                    StatusVD &statusVD, StatusFP &statusFP,
                    NullabilityCheckDiagReporter &reporter, ASTContext &ctx,
                    const FunctionDecl &fd, ParentMap &pm)
      : NCI(nci), Block(block), CurrStatusVD(statusVD), CurrStatusFP(statusFP),
        Reporter(reporter), Ctx(ctx), Fd(fd), PM(pm) {}

  bool IsStmtInSafeZone(Stmt *S);
  bool ShouldReportNullPtrError(Stmt *S);
  void VisitDeclStmt(DeclStmt *S);
  void HandleVarDeclInit(DeclStmt *DS, VarDecl *VD);
  void HandlePointerInit(DeclStmt *DS, VarDecl *VD);
  void HandleRecordInit(DeclStmt *DS, VarDecl *VD, RecordDecl *RD);
  void HandleFieldInit(DeclStmt *DS, VarDecl *VD, FieldDecl *FD,
                       Expr *FieldInit, std::string FullFieldPath);
  void VisitBinaryOperator(BinaryOperator *BO);
  void VisitUnaryOperator(UnaryOperator *UO);
  void VisitArraySubscriptExpr(ArraySubscriptExpr *ASE);
  void VisitMemberExpr(MemberExpr *ME);
  void VisitCallExpr(CallExpr *CE);
  void VisitReturnStmt(ReturnStmt *RS);
  void VisitCStyleCastExpr(CStyleCastExpr *CSCE);
  NullabilityKind getExprPathNullability(Expr *E, bool Point = false);
  void SetCFGBlocksByExpr(Expr *PtrE, const CFGBlock *NonNullBlock,
                          const CFGBlock *NullableBlock);
  void PassConditionStatusToSuccBlocks(Expr *CondExpr);
  Expr *NormalizeInitExpr(Expr *E);
  void HandleNestedRecordInit(DeclStmt *DS, VarDecl *TopVD, RecordDecl *RD,
                              InitListExpr *InitList, std::string FieldPrefix);
  void HandleArrayInit(DeclStmt *DS, VarDecl *VD, const ArrayType *AT,
                       InitListExpr *ILE, std::string FieldPath);
};

/// Return whether E or its sub-expressions contains built-in []
bool containsBuiltinArraySubscript(Expr *E) {
  E = E->IgnoreParenImpCasts();
  return isa<ArraySubscriptExpr>(E);
}

/// Return whether E contains dereference of pointer arithmetic, e.g. *(a + 1).
bool containsPointerArithmeticDeref(Expr *E) {
  E = E->IgnoreParenImpCasts();
  if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref) {
      Expr *SubE = UO->getSubExpr()->IgnoreParenImpCasts();
      if (auto *BO = dyn_cast<BinaryOperator>(SubE)) {
        if (BO->isAdditiveOp())
          return true;
      }
    }
  }
  return false;
}

/// Extract the pointer expression that satisfies nullability state transfer
/// requirements:
///
/// pointer type, lvalue, not volatile and doesn't contain built-in
/// array subscription [i] or pointer-arithmetic-dereference *(p + 1).
///
/// Return nullptr if no such expression exists.
Expr *GetPointerExpr(Expr *E) {
  if (!E)
    return nullptr;
  E = E->IgnoreParenImpCasts();

  // recursively process (m, n, o, p) and (p = q = r) to get p
  if (auto *BO = dyn_cast<BinaryOperator>(E)) {
    if (BO->isAssignmentOp())
      return GetPointerExpr(BO->getLHS());
    if (BO->getOpcode() == BO_Comma)
      return GetPointerExpr(BO->getRHS());
  }

  if (!E->isLValue())
    return nullptr;

  QualType QT = E->getType();
  if (!QT.getCanonicalType()->isPointerType())
    return nullptr;

  if (QT.isVolatileQualified())
    return nullptr;

  if (containsBuiltinArraySubscript(E) || containsPointerArithmeticDeref(E))
    return nullptr;

  return E;
}

/// Helper of PassConditionStatusToSuccBlocks, under Ctx, extract a binary
/// operator condition expression CondExpr into underlying core pointer
/// expression and returns it.
/// Inverse: output paremeter indicates whether the condition is checking for
/// null-ness or non-null-ness
Expr *GetPointerExprFromBinaryCondition(BinaryOperator *BO, ASTContext &Ctx,
                                        bool &Inverse) {
  if (!BO)
    return nullptr;

  // Unwrap expression whose value is determined by RHS, such as
  // (x, p != nullptr) or (x = (p == nullptr))
  if (BO->getOpcode() == BO_Comma || BO->isAssignmentOp()) {
    Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();
    if (auto *RHSBO = dyn_cast<BinaryOperator>(RHS))
      return GetPointerExprFromBinaryCondition(RHSBO, Ctx, Inverse);
    return nullptr;
  }

  // Check Equality operator, such as p == nullptr, p != nullptr
  // and s.p == nullptr, s.p != nullptr
  if (!BO->isEqualityOp())
    return nullptr;

  Expr *LHS = BO->getLHS()->IgnoreParenImpCasts();
  Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();
  bool NullLHS = LHS->isNullExpr(Ctx);
  bool NullRHS = RHS->isNullExpr(Ctx);
  if (NullLHS == NullRHS)
    // not a nullability check, or both sides are trivially semantic null
    return nullptr;

  Expr *Candidate = NullLHS ? RHS : LHS;
  Expr *PtrE = GetPointerExpr(Candidate);
  if (!PtrE)
    return nullptr;

  Inverse = BO->getOpcode() == BO_EQ;
  return PtrE;
}

} // namespace

namespace clang{
// basic tool for CFG check and global Nullability check
NullabilityKind getDefNullability(QualType QT, ASTContext &Ctx) {
  QualType CanQT = QT.getCanonicalType();
  if (CanQT->isPointerType()) {
    Optional<NullabilityKind> Kind = QT->getNullability(Ctx);
    if (Kind && (*Kind == NullabilityKind::NonNull ||
                 *Kind == NullabilityKind::Nullable)) {
      return *Kind;
    } else if (CanQT.isOwnedQualified() || CanQT.isBorrowQualified()) {
      return NullabilityKind::NonNull;
    } else // Raw Pointer is nullable by default.
      return NullabilityKind::Nullable;
  }
  return NullabilityKind::Unspecified;
}
} // namespace clang

static void VisitMEForFieldPath(Expr *E, FieldPath &FP) {
  if (auto ME = dyn_cast<MemberExpr>(E)) {
    if (auto FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      FP.second = "." + FD->getNameAsString() + FP.second;
      VisitMEForFieldPath(ME->getBase(), FP);
    }
  } else if (auto DRE = dyn_cast<DeclRefExpr>(E)) {
    if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
      FP.first = VD;
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(E)) {
    VisitMEForFieldPath(ICE->getSubExpr(), FP);
  } else if (auto PE = dyn_cast<ParenExpr>(E)) {
    VisitMEForFieldPath(PE->getSubExpr(), FP);
  }
}

static VarDecl *getVarDeclFromExpr(Expr *E) {
  if (auto DRE = dyn_cast<DeclRefExpr>(E)) {
    if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
      return VD;
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(E)) {
    return getVarDeclFromExpr(ICE->getSubExpr());
  } else if (auto PE = dyn_cast<ParenExpr>(E)) {
    return getVarDeclFromExpr(PE->getSubExpr());
  } else if (auto BO = dyn_cast<BinaryOperator>(E)) {
    return getVarDeclFromExpr(BO->getLHS());
  }
  return nullptr;
}

static MemberExpr *getMemberExprFromExpr(Expr *E) {
  if (auto ME = dyn_cast<MemberExpr>(E)) {
    return ME;
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(E)) {
    return getMemberExprFromExpr(ICE->getSubExpr());
  } else if (auto PE = dyn_cast<ParenExpr>(E)) {
    return getMemberExprFromExpr(PE->getSubExpr());
  }
  return nullptr;
}

// We can get PathNullability for these exprs:
//   1. int *p = nullptr;   // nullptr is NullExpr
//   2. int *p = foo();     // foo() is CallExpr
//   3. int *p = &a;        // &a is UnaryOperator
//   4. int *p = p1;        // p1 is VarDecl
//   5. int *p = s.p;       // s.p is MemberExpr
//   6. int *p = a == 1 ? nullptr : &a; // ConditionOperator
NullabilityKind TransferFunctions::getExprPathNullability(Expr *E, bool Point) {
  QualType QT = E->getType();
  QualType CanQT = QT.getCanonicalType();
  if (Point || CanQT->isPointerType()) {
    switch (E->getStmtClass()) {
    case Expr::ParenExprClass:
      return getExprPathNullability(cast<ParenExpr>(E)->getSubExpr(), true);
    case Expr::SafeExprClass:
      return getExprPathNullability(cast<SafeExpr>(E)->getSubExpr(), true);
    case Expr::ImplicitCastExprClass:
      return getExprPathNullability(cast<ImplicitCastExpr>(E)->getSubExpr(),
                                    true);
    case Expr::CallExprClass: {
      CallExpr *CE = cast<CallExpr>(E);
      if (FunctionDecl *FD = CE->getDirectCallee()) {
        if (CE->getNumArgs() == 1 &&
            (FD->getBuiltinID() == Builtin::BI__move_to_raw ||
             FD->getBuiltinID() == Builtin::BI__take_from_raw)) {
          return getExprPathNullability(CE->getArg(0), true);
        }
      }
      return getDefNullability(CE->getType(), Ctx);
    }
    case Expr::ConditionalOperatorClass: {
      NullabilityKind LHSNK =
          getExprPathNullability(cast<ConditionalOperator>(E)->getLHS(), true);
      NullabilityKind RHSNK =
          getExprPathNullability(cast<ConditionalOperator>(E)->getRHS(), true);
      if (LHSNK == NullabilityKind::Nullable ||
          RHSNK == NullabilityKind::Nullable)
        return NullabilityKind::Nullable;
      else if (LHSNK == NullabilityKind::NonNull &&
               RHSNK == NullabilityKind::NonNull)
        return NullabilityKind::NonNull;
      break;
    }
    case Expr::CStyleCastExprClass:
      return getDefNullability(cast<CStyleCastExpr>(E)->getTypeAsWritten(),
                               Ctx);
    case Expr::UnaryOperatorClass: {
      UnaryOperator::Opcode Op = cast<UnaryOperator>(E)->getOpcode();
      if (Op == UO_AddrOf || Op == UO_AddrMut || Op == UO_AddrConst)
        return NullabilityKind::NonNull;
      if (Op == UO_AddrMutDeref || Op == UO_AddrConstDeref) {
        return getExprPathNullability(cast<UnaryOperator>(E)->getSubExpr(), true);
      }
      break;
    }
    case Expr::BinaryOperatorClass: {
      BinaryOperator::Opcode Op = cast<BinaryOperator>(E)->getOpcode();
      if (Op == BO_Comma || Op == BO_Assign) {
        return getExprPathNullability(cast<BinaryOperator>(E)->getRHS(), true);
      }
      break;
    }
    case Expr::DeclRefExprClass: {
      if (VarDecl *VD = dyn_cast<VarDecl>(cast<DeclRefExpr>(E)->getDecl())) {
        NullabilityKind NK = getDefNullability(VD->getType(), Ctx);
        if (NK == NullabilityKind::NonNull)
          return NullabilityKind::NonNull;
        else if (NK == NullabilityKind::Nullable && CurrStatusVD.count(VD))
          return CurrStatusVD[VD];
      }
      break;
    }
    case Expr::ArraySubscriptExprClass: {
      // Builtin array elements cannot have independent path-sensitive state.
      NullabilityKind NK =
          getDefNullability(cast<ArraySubscriptExpr>(E)->getType(), Ctx);
      if (NK == NullabilityKind::NonNull || NK == NullabilityKind::Nullable)
        return NK;
      break;
    }
    case Expr::MemberExprClass: {
      if (auto FD = dyn_cast<FieldDecl>(cast<MemberExpr>(E)->getMemberDecl())) {
        NullabilityKind NK = getDefNullability(FD->getType(), Ctx);
        if (NK == NullabilityKind::NonNull)
          return NullabilityKind::NonNull;
        else if (NK == NullabilityKind::Nullable) {
          FieldPath FP;
          VisitMEForFieldPath(cast<MemberExpr>(E), FP);
          if (CurrStatusFP.count(FP))
            return CurrStatusFP[FP];
        }
      }
      break;
    }
    case Expr::IntegerLiteralClass: {
      if (cast<IntegerLiteral>(E)->getValue().getZExtValue() == 0)
        return NullabilityKind::Nullable;
      break;
    }
    default:
      break;
    }
    if (QT->isNullPtrType()) {
      return NullabilityKind::Nullable;
    }
    if (Optional<llvm::APSInt> I = E->getIntegerConstantExpr(Ctx)) {
      if (*I == 0)
        return NullabilityKind::Nullable;
    }
  }
  // For no-pointer type, we treat it as Unspecified.
  return NullabilityKind::Unspecified;
}

bool TransferFunctions::IsStmtInSafeZone(Stmt *S) {
  if (!S)
    return false;
  const Stmt *ParentStmt = PM.getParent(S);
  while (ParentStmt) {
    if (auto *CS = dyn_cast<CompoundStmt>(ParentStmt)) {
      SafeZoneSpecifier SafeZoneSpec = CS->getCompSafeZoneSpecifier();
      if (SafeZoneSpec == SZ_Safe) {
        return true;
      } else if (SafeZoneSpec == SZ_Unsafe) {
        return false;
      }
    }
    if (auto *SS = dyn_cast<SafeStmt>(ParentStmt)) {
      SafeZoneSpecifier SafeZoneSpec = SS->getSafeZoneSpecifier();
      if (SafeZoneSpec == SZ_Safe) {
        return true;
      } else if (SafeZoneSpec == SZ_Unsafe) {
        return false;
      }
    }
    if (auto *SE = dyn_cast<SafeExpr>(ParentStmt)) {
      SafeZoneSpecifier SafeZoneSpec = SE->getSafeZoneSpecifier();
      if (SafeZoneSpec == SZ_Safe) {
        return true;
      } else if (SafeZoneSpec == SZ_Unsafe) {
        return false;
      }
    }
    ParentStmt = PM.getParent(ParentStmt);
  }
  return Fd.getSafeZoneSpecifier() == SZ_Safe;
}

bool TransferFunctions::ShouldReportNullPtrError(Stmt *S) {
  LangOptions::NullCheckZone CheckZone = Ctx.getLangOpts().getNullabilityCheck();
  if (CheckZone == LangOptions::NC_ALL) {
    return true;
  }
  return IsStmtInSafeZone(S);
}

void TransferFunctions::VisitDeclStmt(DeclStmt *DS) {
  for (Decl *D : DS->decls())
    if (VarDecl *VD = dyn_cast<VarDecl>(D))
      if (VD->getInit())
        HandleVarDeclInit(DS, VD);
}

void TransferFunctions::HandleVarDeclInit(DeclStmt *DS, VarDecl *VD) {
  if (VD->getType()->isPointerType())
    HandlePointerInit(DS, VD);
  else if (auto RecTy =
               dyn_cast<RecordType>(VD->getType().getCanonicalType())) {
    if (RecordDecl *RD = RecTy->getDecl())
      HandleRecordInit(DS, VD, RD);
  } else if (auto *AT = VD->getType()->getAsArrayTypeUnsafe()) {
    if (auto *ILE = dyn_cast<InitListExpr>(VD->getInit()))
      HandleArrayInit(DS, VD, AT, ILE, "");
  }
}

// Init a pointer variable
void TransferFunctions::HandlePointerInit(DeclStmt *DS, VarDecl *VD) {
  NullabilityKind LHSKind = getDefNullability(VD->getType(), Ctx);
  NullabilityKind RHSKind = getExprPathNullability(VD->getInit());
  if (LHSKind == NullabilityKind::NonNull) {
    // NonNull pointer cannot be assigned by expr
    // whose PathNullability is nullable.
    if (RHSKind == NullabilityKind::Nullable && ShouldReportNullPtrError(DS)) {
      NullabilityCheckDiagInfo DI(VD->getLocation(), NonnullAssignedByNullable);
      Reporter.addDiagInfo(DI);
    }
  } else if (CurrStatusVD.count(VD))
    // Here we update PathNullability of nullable pointer.
    CurrStatusVD[VD] = RHSKind;
}

// Init a record type variable.
void TransferFunctions::HandleRecordInit(DeclStmt *DS, VarDecl *VD,
                                             RecordDecl *RD) {
  if (RD->field_empty())
    return;
  if (Expr *Init = VD->getInit()) {
    Init = Init->IgnoreParenImpCasts();
    while (true) {
      if (auto *CSE = dyn_cast<CStyleCastExpr>(Init)) {
        Init = CSE->getSubExpr()->IgnoreParenImpCasts();
        continue;
      }
      if (auto *CLE = dyn_cast<CompoundLiteralExpr>(Init)) {
        if (Expr *Sub = CLE->getInitializer()) {
          Init = Sub->IgnoreParenImpCasts();
          continue;
        }
      }
      break;
    }
    if (auto *InitListE = dyn_cast<InitListExpr>(Init)) {
      HandleNestedRecordInit(DS, VD, RD, InitListE, "");
    }
  }
}

void TransferFunctions::HandleArrayInit(DeclStmt *DS, VarDecl *VD,
                                        const ArrayType *AT,
                                        InitListExpr *ILE,
                                        std::string FieldPath) {
  QualType ElemTy = AT->getElementType();
  unsigned NumInits = ILE->getNumInits();

  // Determine array size for constant-size arrays.
  unsigned ArraySize = 0;
  bool HasKnownSize = false;
  if (auto *CAT = dyn_cast<ConstantArrayType>(AT)) {
    ArraySize = (unsigned)CAT->getSize().getZExtValue();
    HasKnownSize = true;
  }

  // Check explicitly provided initializers.
  for (unsigned i = 0; i < NumInits; ++i) {
    Expr *ElemInit = ILE->getInit(i)->IgnoreParenImpCasts();
    while (true) {
      if (auto *CSE = dyn_cast<CStyleCastExpr>(ElemInit)) {
        ElemInit = CSE->getSubExpr()->IgnoreParenImpCasts();
        continue;
      }
      if (auto *CLE = dyn_cast<CompoundLiteralExpr>(ElemInit)) {
        if (Expr *Sub = CLE->getInitializer()) {
          ElemInit = Sub->IgnoreParenImpCasts();
          continue;
        }
      }
      break;
    }

    if (ElemTy->isPointerType()) {
      NullabilityKind LHSKind = getDefNullability(ElemTy, Ctx);
      NullabilityKind RHSKind = getExprPathNullability(ElemInit);
      if (LHSKind == NullabilityKind::NonNull &&
          RHSKind != NullabilityKind::NonNull && ShouldReportNullPtrError(DS)) {
        NullabilityCheckDiagInfo DI(ILE->getBeginLoc(), NonnullInitByDefault,
                                    VD->getNameAsString() + FieldPath);
        Reporter.addDiagInfo(DI);
        return;
      }
    } else if (auto NestedRecTy =
                   dyn_cast<RecordType>(ElemTy.getCanonicalType())) {
      if (RecordDecl *NestedRD = NestedRecTy->getDecl()) {
        if (auto *NestedILE = dyn_cast<InitListExpr>(ElemInit)) {
          HandleNestedRecordInit(DS, VD, NestedRD, NestedILE, "");
        }
      }
    } else if (auto *NestedAT = ElemTy->getAsArrayTypeUnsafe()) {
      if (auto *NestedILE = dyn_cast<InitListExpr>(ElemInit))
        HandleArrayInit(DS, VD, NestedAT, NestedILE, "");
    }
  }

  // Check implicitly zero-initialized trailing elements.
  if (HasKnownSize && NumInits < ArraySize) {
    if (ElemTy->isPointerType()) {
      if (getDefNullability(ElemTy, Ctx) == NullabilityKind::NonNull &&
          ShouldReportNullPtrError(DS)) {
        NullabilityCheckDiagInfo DI(VD->getBeginLoc(), NonnullInitByDefault,
                                    VD->getNameAsString());
        Reporter.addDiagInfo(DI);
      }
    }
  }
}

Expr *TransferFunctions::NormalizeInitExpr(Expr *E) {
  if (!E)
    return nullptr;
  while (true) {
    if (auto *CSE = dyn_cast<CStyleCastExpr>(E)) {
      E = CSE->getSubExpr()->IgnoreParenImpCasts();
      continue;
    }
    if (auto *CLE = dyn_cast<CompoundLiteralExpr>(E)) {
      if (Expr *Sub = CLE->getInitializer()) {
        E = Sub->IgnoreParenImpCasts();
        continue;
      }
    }
    break;
  }
  return E;
}

void TransferFunctions::HandleNestedRecordInit(DeclStmt *DS, VarDecl *TopVD,
                                               RecordDecl *RD,
                                               InitListExpr *InitList,
                                               std::string FieldPrefix) {
  if (!RD || RD->field_empty())
    return;
  std::vector<FieldDecl *> FieldsToProcess;
  if (RD->isUnion()) {
    if (!RD->field_empty())
      FieldsToProcess.push_back(*RD->field_begin());
  } else if (RD->isStruct()) {
    for (FieldDecl *FD : RD->fields())
      FieldsToProcess.push_back(FD);
  }
  for (FieldDecl *FD : FieldsToProcess) {
    std::string fullFieldPath = FieldPrefix + "." + FD->getNameAsString();

    Expr *fieldInit = nullptr;
    if (InitList && !InitList->inits().empty()) {
      unsigned idx = RD->isStruct() ? FD->getFieldIndex() : 0;
      fieldInit = InitList->getInit(idx);
    }
    // normalize init (ignore casts, compound literal)
    fieldInit = NormalizeInitExpr(fieldInit);
    if (FD->getType()->isPointerType()) {
      if (!fieldInit) {
        // handle ImplicitValueInitExpr / empty init
        if (!ShouldReportNullPtrError(DS))
          continue;
        if (getDefNullability(FD->getType(), Ctx) != NullabilityKind::NonNull)
          continue;
        NullabilityCheckDiagInfo DI(TopVD->getLocation(), NonnullInitByDefault,
                                    TopVD->getNameAsString() + fullFieldPath);
        Reporter.addDiagInfo(DI);
        continue;
      }
      HandleFieldInit(DS, TopVD, FD, fieldInit, fullFieldPath);
    } else if (auto *AT = FD->getType()->getAsArrayTypeUnsafe()) {
      if (fieldInit) {
        if (auto *ILE = dyn_cast<InitListExpr>(fieldInit)) {
          HandleArrayInit(DS, TopVD, AT, ILE, fullFieldPath);
        }
      }
    } else if (auto *nestedRecTy =
                   dyn_cast<RecordType>(FD->getType().getCanonicalType())) {
      if (RecordDecl *nestedRD = nestedRecTy->getDecl()) {
        if (!fieldInit || isa<ImplicitValueInitExpr>(fieldInit) ||
            isa<InitListExpr>(fieldInit)) {
          HandleNestedRecordInit(DS, TopVD, nestedRD,
                                 dyn_cast_or_null<InitListExpr>(fieldInit),
                                 fullFieldPath);
        }
      }
    }
  }
}

void TransferFunctions::HandleFieldInit(DeclStmt *DS, VarDecl *VD,
                                        FieldDecl *FD, Expr *FieldInit,
                                        std::string FullFieldPath) {
  FieldPath FP(VD, FullFieldPath);

  NullabilityKind LHSKind = getDefNullability(FD->getType(), Ctx);
  NullabilityKind RHSKind = getExprPathNullability(FieldInit);

  if (LHSKind == NullabilityKind::NonNull) {
    if (RHSKind != NullabilityKind::NonNull && ShouldReportNullPtrError(DS)) {
      NullabilityCheckDiagInfo DI(VD->getBeginLoc(), NonnullInitByDefault,
                                  VD->getNameAsString() + FullFieldPath);
      Reporter.addDiagInfo(DI);
    }
  } else {
    if (CurrStatusFP.count(FP)) {
      CurrStatusFP[FP] = RHSKind;
    }
  }
}

void TransferFunctions::VisitBinaryOperator(BinaryOperator *BO) {
  if (BO->isAssignmentOp()) {
    Expr *LHS = BO->getLHS();
    QualType LHSQT = LHS->getType();
    if (LHSQT.getCanonicalType()->isPointerType()) {
      NullabilityKind RHSKind = getExprPathNullability(BO->getRHS());
      if (VarDecl *VD = getVarDeclFromExpr(LHS)) {
        NullabilityKind LHSKind = getDefNullability(VD->getType(), Ctx);
        if (LHSKind == NullabilityKind::NonNull) {
          // NonNull pointer cannot be assigned by expr
          // whose PathNullability is nullable.
          if (RHSKind == NullabilityKind::Nullable && ShouldReportNullPtrError(BO)) {
            NullabilityCheckDiagInfo DI(BO->getBeginLoc(),
                                        NonnullAssignedByNullable);
            Reporter.addDiagInfo(DI);
          }
        } else if (CurrStatusVD.count(VD)) {
          // Here we update PathNullability of nullable pointer.
          CurrStatusVD[VD] = RHSKind;
        }
      } else if (MemberExpr *ME = getMemberExprFromExpr(LHS)) {
        if (FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
          NullabilityKind LHSKind = getDefNullability(FD->getType(), Ctx);
          if (LHSKind == NullabilityKind::NonNull) {
            if (RHSKind == NullabilityKind::Nullable && ShouldReportNullPtrError(BO)) {
              NullabilityCheckDiagInfo DI(ME->getBeginLoc(),
                                          NonnullAssignedByNullable);
              Reporter.addDiagInfo(DI);
            }
          } else {
            FieldPath FP;
            VisitMEForFieldPath(ME, FP);
            if (CurrStatusFP.count(FP))
              CurrStatusFP[FP] = RHSKind;
          }
        }
      }
    }
  }
}

// NonNull parameter cannot take Nullable pointer as argument.
void TransferFunctions::VisitCallExpr(CallExpr *CE) {
  if (FunctionDecl *FD = CE->getDirectCallee()) {
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      ParmVarDecl *PVD = FD->getParamDecl(i);
      if (getDefNullability(PVD->getType(), Ctx) == NullabilityKind::NonNull) {
        Expr *ArgE = CE->getArg(i);
        if (getExprPathNullability(ArgE) == NullabilityKind::Nullable && ShouldReportNullPtrError(CE)) {
          NullabilityCheckDiagInfo DI(ArgE->getBeginLoc(),
                                      PassNullableArgument);
          Reporter.addDiagInfo(DI);
        }
      }
    }
  }
}

// *p is not allowed when p has nullable PathNullability.
// &mut *p and &const *p are allowed because no actual dereferencing happens.
// These expressions only change the pointer type without accessing the memory,
// so they do not violate any nullability guarantees.
void TransferFunctions::VisitUnaryOperator(UnaryOperator *UO) {
  UnaryOperator::Opcode Op = UO->getOpcode();
  if (Op == UO_Deref) {
    if (getExprPathNullability(UO->getSubExpr()) == NullabilityKind::Nullable && ShouldReportNullPtrError(UO)) {
      NullabilityCheckDiagInfo DI(UO->getBeginLoc(),
                                  NullablePointerDereference);
      Reporter.addDiagInfo(DI);
    }
  }
}

// p[i] is not allowed when p has nullable PathNullability.
// Array subscript is equivalent to *(p + i), so it dereferences the pointer.
void TransferFunctions::VisitArraySubscriptExpr(ArraySubscriptExpr *ASE) {
  if (getExprPathNullability(ASE->getBase()) == NullabilityKind::Nullable && ShouldReportNullPtrError(ASE)) {
    NullabilityCheckDiagInfo DI(ASE->getBeginLoc(),
                                NullablePointerDereference);
    Reporter.addDiagInfo(DI);
  }
}

// p->a is not allowed when p has nullable PathNullability.
void TransferFunctions::VisitMemberExpr(MemberExpr *ME) {
  if (ME->isArrow()) {
    if (getExprPathNullability(ME->getBase()) == NullabilityKind::Nullable && ShouldReportNullPtrError(ME)) {
      NullabilityCheckDiagInfo DI(ME->getBeginLoc(),
                                  NullablePointerAccessMember);
      Reporter.addDiagInfo(DI);
    }
  }
}

// (int *_Nonnull)p, (int *borrow)p, (int *owned)p is not allowed
// when p has nullable PathNullability.
void TransferFunctions::VisitCStyleCastExpr(CStyleCastExpr *CSCE) {
  if (getDefNullability(CSCE->getTypeAsWritten(), Ctx) == NullabilityKind::NonNull) {
    if (getExprPathNullability(CSCE->getSubExpr()) == NullabilityKind::Nullable &&
        ShouldReportNullPtrError(CSCE)) {
      NullabilityCheckDiagInfo DI(CSCE->getBeginLoc(), NullableCastNonnull);
      Reporter.addDiagInfo(DI);
    }
  }
}

// If function return type is NonNull, cannot return pointer
// which has nullable PathNullability.
void TransferFunctions::VisitReturnStmt(ReturnStmt *RS) {
  Expr *RV = RS->getRetValue();
  if (!RV)
    return;
  if (getDefNullability(Fd.getReturnType(), Ctx) == NullabilityKind::NonNull) {
    if (getExprPathNullability(RV) == NullabilityKind::Nullable && ShouldReportNullPtrError(RS)) {
      NullabilityCheckDiagInfo DI(RV->getBeginLoc(), ReturnNullable);
      Reporter.addDiagInfo(DI);
    }
  }
}

// This function handles CFGBlocks setting by expression
void TransferFunctions::SetCFGBlocksByExpr(Expr *PtrE,
                                           const CFGBlock *NonNullBlock,
                                           const CFGBlock *NullableBlock) {
  if (VarDecl *VD = getVarDeclFromExpr(PtrE)) {
    if (getDefNullability(VD->getType(), Ctx) == NullabilityKind::Nullable &&
        CurrStatusVD.count(VD) &&
        CurrStatusVD[VD] != NullabilityKind::NonNull) {
      NCI.BlocksConditionStatusVD[NonNullBlock][Block] =
          std::pair<VarDecl *, NullabilityKind>(VD, NullabilityKind::NonNull);
      NCI.BlocksConditionStatusVD[NullableBlock][Block] =
          std::pair<VarDecl *, NullabilityKind>(VD, NullabilityKind::Nullable);
    }
  } else if (MemberExpr *ME = getMemberExprFromExpr(PtrE)) {
    if (auto FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      FieldPath FP;
      VisitMEForFieldPath(ME, FP);
      if (getDefNullability(FD->getType(), Ctx) == NullabilityKind::Nullable &&
          CurrStatusFP.count(FP) &&
          CurrStatusFP[FP] != NullabilityKind::NonNull) {
        FieldPath FP;
        VisitMEForFieldPath(ME, FP);
        NCI.BlocksConditionStatusFP[NonNullBlock][Block] =
            std::pair<FieldPath, NullabilityKind>(FP, NullabilityKind::NonNull);
        NCI.BlocksConditionStatusFP[NullableBlock][Block] =
            std::pair<FieldPath, NullabilityKind>(FP,
                                                  NullabilityKind::Nullable);
      }
    }
  }
}

// This function handles condition expression and
// pass the conditions to successor block.
void TransferFunctions::PassConditionStatusToSuccBlocks(Expr *CondExpr) {
  if (!CondExpr)
    return;
  CondExpr = CondExpr->IgnoreParenImpCasts();

  // When building CFG, if a block has a condition as terminitor,
  // the successors has certain order, for example:
  //    B4(has condition as terminitor)
  //   /  \
  //  B3  B2
  // The first successor of B4 must be true branch,
  // and the second successor of B4 must be false branch.
  // Here we only handle terminators with two successors.

  // Because the caller has guaranteed that the block has two successors,
  // we can directly get these two successor blocks without
  // checking results of the iterator step by step
  CFGBlock::const_succ_iterator it = Block->succ_begin();
  const CFGBlock *TrueBlock = *it++;
  const CFGBlock *FalseBlock = *it;
  if (!TrueBlock || !FalseBlock)
    return;

  if (Expr *PtrE = GetPointerExpr(CondExpr)) {
    // Condition expr directly references a pointer
    // such as: if (p), if (s.p), ...
    SetCFGBlocksByExpr(PtrE, TrueBlock, FalseBlock);
    return;
  }
  if (auto UO = dyn_cast<UnaryOperator>(CondExpr)) {
    // Condition expr is UnaryOperator (logical not), such as:
    // if (!p), if (!s.p), ...
    if (UO->getOpcode() == UO_LNot) {
      if (Expr *PtrE = GetPointerExpr(UO->getSubExpr())) {
        // set FalseBlock NoNull
        SetCFGBlocksByExpr(PtrE, FalseBlock, TrueBlock);
      }
    }
  }
  // General binary operator cases including comparison and assignment,
  // binary logical operator is already split when building CFG
  if (auto BO = dyn_cast<BinaryOperator>(CondExpr)) {
    bool Inverse = false;
    if (Expr *PtrE = GetPointerExprFromBinaryCondition(BO, Ctx, Inverse)) {
      if (Inverse)
        SetCFGBlocksByExpr(PtrE, FalseBlock, TrueBlock);
      else
        SetCFGBlocksByExpr(PtrE, TrueBlock, FalseBlock);
    }
  }
}

// Traverse all blocks of cfg to collect all nullable pointers used,
// including local and global variable and parameters.
// Init PathNullability of these pointers by Nullable.
void NullabilityCheckImpl::initStatus(const CFG &cfg, ASTContext &ctx) {
  const CFGBlock *entry = &cfg.getEntry();
  for (const CFGBlock *B : cfg.const_nodes()) {
    if (B != entry && B != &cfg.getExit() && !B->succ_empty() &&
        !B->pred_empty()) {
      for (CFGBlock::const_iterator it = B->begin(), ei = B->end(); it != ei;
           ++it) {
        const CFGElement &elem = *it;
        if (elem.getAs<CFGStmt>()) {
          Stmt *S = const_cast<Stmt *>(elem.castAs<CFGStmt>().getStmt());
          if (auto DRE = dyn_cast<DeclRefExpr>(S)) {
            if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
              if (getDefNullability(VD->getType(), ctx) ==
                  NullabilityKind::Nullable)
                BlocksEndStatusVD[entry][VD] = NullabilityKind::Nullable;
          } else if (auto ME = dyn_cast<MemberExpr>(S)) {
            if (FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
              if (getDefNullability(FD->getType(), ctx) ==
                  NullabilityKind::Nullable) {
                FieldPath FP;
                VisitMEForFieldPath(ME, FP);
                BlocksEndStatusFP[entry][FP] = NullabilityKind::Nullable;
              }
            }
          }
        }
      }
    }
  }
}

StatusVD NullabilityCheckImpl::mergeVD(StatusVD statusA, StatusVD statusB) {
  if (statusA.empty())
    return statusB;
  for (auto NullabilityOfVD : statusB) {
    VarDecl *VD = NullabilityOfVD.first;
    NullabilityKind NK = NullabilityOfVD.second;
    if (statusA.count(VD)) {
      statusA[VD] = NK == NullabilityKind::Nullable ? NullabilityKind::Nullable
                                                    : statusA[VD];
    } else {
      statusA[VD] = NK;
    }
  }
  return statusA;
}

StatusFP NullabilityCheckImpl::mergeFP(StatusFP statusA, StatusFP statusB) {
  if (statusA.empty())
    return statusB;
  for (auto NullabilityOfFP : statusB) {
    FieldPath FP = NullabilityOfFP.first;
    NullabilityKind NK = NullabilityOfFP.second;
    if (statusA.count(FP)) {
      statusA[FP] = NK == NullabilityKind::Nullable ? NullabilityKind::Nullable
                                                    : statusA[FP];
    } else {
      statusA[FP] = NK;
    }
  }
  return statusA;
}

std::pair<StatusVD, StatusFP>
NullabilityCheckImpl::runOnBlock(const CFGBlock *block, StatusVD statusVD,
                                 StatusFP statusFP,
                                 NullabilityCheckDiagReporter &reporter,
                                 ASTContext &ctx, const FunctionDecl &fd, ParentMap &PM) {
  TransferFunctions TF(*this, block, statusVD, statusFP, reporter, ctx, fd, PM);

  for (CFGBlock::const_iterator it = block->begin(), ei = block->end();
       it != ei; ++it) {
    const CFGElement &elem = *it;
    if (elem.getAs<CFGStmt>()) {
      const Stmt *S = elem.castAs<CFGStmt>().getStmt();
      TF.Visit(const_cast<Stmt *>(S));
    }
  }

  // Here we will handle the condition in IfStmt, or other branch stmts
  // which will change the nullability of VarDecl or FiledPath.
  // Limit block's successor to 2 to ensure compatibility of existing
  // implementation of PassConditionStatusToSuccBlocks, which assumes the first
  // successor is true branch and the second successor is false branch.
  if (block->succ_size() == 2) {
    // Use block-local last condition instead of terminator condition to be
    // consistent for conditions already split in CFG (&& ||).
    Expr *CondExpr = const_cast<Expr *>(block->getLastCondition());
    TF.PassConditionStatusToSuccBlocks(CondExpr);
  }
  return std::make_pair(statusVD, statusFP);
}

void clang::runNullabilityCheck(const FunctionDecl &fd, const CFG &cfg,
                                AnalysisDeclContext &ac,
                                NullabilityCheckDiagReporter &reporter,
                                ASTContext &ctx) {
  // The analysis currently has scalability issues for very large CFGs.
  // Bail out if it looks too large.
  if (cfg.getNumBlockIDs() > 300000)
    return;

  NullabilityCheckImpl NCI;
  NCI.initStatus(cfg, ctx);

  // Proceed with the worklist.
  ForwardDataflowWorklist worklist(cfg, ac);
  const CFGBlock *entry = &cfg.getEntry();
  for (const CFGBlock *B : cfg.const_reverse_nodes())
    if (B != entry && !B->pred_empty())
      worklist.enqueueBlock(B);

  while (const CFGBlock *block = worklist.dequeue()) {
    StatusVD &preValVD = NCI.BlocksBeginStatusVD[block];
    StatusFP &preValFP = NCI.BlocksBeginStatusFP[block];
    StatusVD valVD;
    StatusFP valFP;
    for (CFGBlock::const_pred_iterator it = block->pred_begin(),
                                       ei = block->pred_end();
         it != ei; ++it) {
      if (const CFGBlock *pred = *it) {
        StatusVD predValVD = NCI.BlocksEndStatusVD[pred];
        if (NCI.BlocksConditionStatusVD.count(block)) {
          if (NCI.BlocksConditionStatusVD[block].count(pred)) {
            std::pair<VarDecl *, NullabilityKind> condition =
                NCI.BlocksConditionStatusVD[block][pred];
            predValVD[condition.first] = condition.second;
          }
        }
        valVD = NCI.mergeVD(valVD, predValVD);

        StatusFP predValFP = NCI.BlocksEndStatusFP[pred];
        if (NCI.BlocksConditionStatusFP.count(block)) {
          if (NCI.BlocksConditionStatusFP[block].count(pred)) {
            std::pair<FieldPath, NullabilityKind> condition =
                NCI.BlocksConditionStatusFP[block][pred];
            predValFP[condition.first] = condition.second;
          }
        }
        valFP = NCI.mergeFP(valFP, predValFP);
      }
    }

    std::pair<StatusVD, StatusFP> val =
        NCI.runOnBlock(block, valVD, valFP, reporter, ctx, fd, ac.getParentMap());
    NCI.BlocksEndStatusVD[block] = val.first;
    NCI.BlocksEndStatusFP[block] = val.second;
    if (preValVD == val.first && preValFP == val.second)
      continue;

    preValVD = val.first;
    preValFP = val.second;

    // Enqueue the value to the successors.
    worklist.enqueueSuccessors(block);
  }
}
#endif // ENABLE_BSC