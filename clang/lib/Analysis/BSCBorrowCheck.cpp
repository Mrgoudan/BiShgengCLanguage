//===- BSCBorrowCheck.cpp - Borrow Check for Source CFGs -*- BSC --*-------//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BSC borrow check for source-level CFGs.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSCBorrowCheck.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Analysis/CFG.h"
#include <algorithm>
#include <map>
#include <set>
#include <vector>
using namespace clang;
using namespace std;

using CFGPath = std::vector<const CFGBlock *>;
using CFGPathVec = std::vector<std::vector<const CFGBlock *>>;

// This function checks if the current field is a prefix of the borrowed field.
static bool IsPrefix(const string &currentField, const string &borrowedField) {
  if (borrowedField.length() < currentField.length()) {
    return false;
  }

  bool compareFlag = (borrowedField.compare(0, currentField.length(), currentField) == 0);

  if (borrowedField.length() == currentField.length()) {
    return compareFlag;
  }

  return compareFlag && (borrowedField[currentField.length()] == '.');
}

class BorrowCheckImpl {
public:
  ASTContext &Ctx;
  const FunctionDecl &fd;

  CFGPathVec FindPathOfCFG(const CFG &cfg);
  NonLexicalLifetime CalculateNLLForPath(
      const CFGPath &Path,
      const std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
                     BorrowTargetInfo> &ParamTarget,
      unsigned NumElements);
  void BorrowCheckForPath(const CFGPath &Path,
                          BorrowCheckDiagReporter &reporter,
                          const NonLexicalLifetime &NLLForAllVars,
                          unsigned NumElements);
  void
  BuildParamTarget(std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
                            BorrowTargetInfo> &ParamTarget);

  BorrowCheckImpl(ASTContext &ASTCtx, const FunctionDecl &FD)
      : Ctx(ASTCtx), fd(FD) {}
};

class NLLCalculator : public StmtVisitor<NLLCalculator> {
  ASTContext &Ctx;
  DeclContext *DC;
  // Record VarDecls when we traverse path in reverse order.
  // For borrow and owned variables,
  // we should record the location they are used for the last time in path.
  // For other local variables,
  // we should record the location they leave current scope.
  llvm::DenseMap<VarDecl *, unsigned> FoundVars;

  // Only record borrow struct VarDecl and FieldPath when we traverse path in
  // reverse order. For example:
  // @code
  //     struct S { int* borrow p; };
  //     void test() {
  //         int local = 5;
  //         struct S s = { .p = &mut local };
  //         use(s.p);       // Record `s` and ".p"
  //     }
  // @endcode
  std::map<std::pair<VarDecl *, std::string>, unsigned> FoundBorrowFields;

  unsigned NumElements;
  bool CurrOpIsBorrowUse = false;

public:
  // Record current CFGElement index we are visiting in the path.
  unsigned CurElemID;
  // Record NLL for all variables in a cfg path.
  NonLexicalLifetime NLLForAllVars;

  NLLCalculator(ASTContext &ASTCtx, DeclContext *DeclCtx, unsigned NumElems)
      : Ctx(ASTCtx), DC(DeclCtx), NumElements(NumElems),
        CurElemID(NumElems + 1) {}

  void VisitDeclStmt(DeclStmt *DS);
  void VisitBinaryOperator(BinaryOperator *BO);
  void VisitDeclRefExpr(DeclRefExpr *DRE);
  void VisitReturnStmt(ReturnStmt *RS);
  void VisitCallExpr(CallExpr *CE);
  void VisitUnaryOperator(UnaryOperator *UO);
  void VisitMemberExpr(MemberExpr *ME);
  void VisitImplicitCastExpr(ImplicitCastExpr *ICE);
  void VisitParenExpr(ParenExpr *PE);
  void VisitScopeEnd(VarDecl *VD);
  void VisitScopeBegin(VarDecl *VD);
  void HandleNLLAfterTraversing(
      const std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
                     BorrowTargetInfo> &ParamTarget);

private:
  void VisitInitForTargets(Expr *InitE,
                           llvm::SmallVector<BorrowTargetInfo> &Targets);
  void VisitInitForBorrowFieldTargets(VarDecl *VD, std::string FP,
                                      RecordDecl *RD, Expr *InitE);
  void VisitFieldInitForBorrowFieldTargets(FieldDecl *FD, VarDecl *VD,
                                           Expr *InitE);
  void VisitCallExprForBorrowFieldTargets(
      CallExpr *CE, llvm::SmallVector<BorrowTargetInfo> &Targets);
  void VisitMEForFieldPath(MemberExpr *ME,
                           std::pair<VarDecl *, std::string> &VDAndFP);
  void UpdateNLLWhenTargetFound(
      BorrowTargetInfo OldTarget,
      const llvm::SmallVector<BorrowTargetInfo> &NewTargets);
};

class BorrowRuleChecker : public StmtVisitor<BorrowRuleChecker> {
  BorrowCheckDiagReporter &reporter;

public:
  NonLexicalLifetime NLLForAllVars;
  unsigned CurElemID = 0;
  enum Operation { None, Read, Write };
  Operation Op = None;
  enum InitStatus { NotInit, Init };
  InitStatus Status = NotInit;
  BorrowRuleChecker(BorrowCheckDiagReporter &reporter,
                    const NonLexicalLifetime &NLLForVars)
      : reporter(reporter), NLLForAllVars(NLLForVars) {}

  // Record the correspondence of borrowed and borrow variables.
  // The key of map is borrowed variable(i.e. targets), the value is variables
  // borrow from its key variable. For example, if we have such borrow
  // correspondence as follows:
  //    `p1` borrows from `local1` from index 5 to index 10 in the path.
  //    `p2` borrows from `local2` from index 3 to index 4 in the path.
  //    `p3` borrows from `local1` and 'local2' from index 6 to index 8 in the
  //    path. `p4` borrows from `s.a.b` from index 11 to index 15 in the path.
  //    `g.c.d` borrows from `s.a.b` and `local2`from index 16 to index 18 in
  //    the path.
  // Then BorrowTargetMap will be:
  //   local1 : { [ "", p1, "", 5, 10 ],
  //              [ "", p3, "", 6, 8 ] }
  //   local2 : { [ "", p2, "", 3, 4 ],
  //              [ "", p3, "", 6, 8 ],
  //              [ "", g,  ".c.d", 16, 18 ] }
  //   s      : { [ ".a.b", p4, "", 11, 15 ],
  //              [ ".a.b", g, ".c.d", 16, 18 ] }
  BorrowTargetMapInfo BorrowTargetMap;
  // save valid borrowed data within the current scope
  // For each target as the KEY, we have all the borrowed refs as VALUE
  // TODO: To have the same type as BorrowTargetMap
  llvm::DenseMap<VarDecl *, std::list<BorrowInfo>> ActiveBorrowTargetMap;

  void BuildBorrowTargetMap();
  void CheckBeBorrowedTarget(const CFGPath &Path);
  void CheckBorrowNLLShorterThanTarget();
  void CheckLifetimeOfBorrowReturnValue(unsigned NumElements);
  void CheckValIsLegalUse(VarDecl *VD, SourceLocation Loc);
  VarDecl *
  FindTargetFromActiveBorrowTargetMap(const VarDecl *VD,
                                      std::string borrowFieldName = "");
  void VisitArraySubscriptExpr(ArraySubscriptExpr *E);
  void VisitBinaryOperator(BinaryOperator *BO);
  void VisitCallExpr(CallExpr *CE);
  void VisitConditionalOperator(ConditionalOperator *E);
  void VisitCastExpr(CastExpr *E);
  void VisitDeclRefExpr(DeclRefExpr *DRE);
  void VisitDeclStmt(DeclStmt *DS);
  void VisitInitListExpr(InitListExpr *E);
  void VisitMemberExpr(MemberExpr *ME);
  void VisitParenExpr(ParenExpr *Node);
  void VisitReturnStmt(ReturnStmt *RS);
  void VisitUnaryOperator(UnaryOperator *UO);
  void UpdateActiveBorrowTargetMap();
  void ClearActiveBorrowTargetMap();

private:
  void HandleDRE(const DeclRefExpr *DRE, std::string fieldName,
                 bool borrowFlag);

  void CheckBorrowVar(const VarDecl *VD, const SourceLocation &Loc);
  void CheckBorrowField(const VarDecl *VD, const SourceLocation &Loc,
                        std::string fieldName);
  void CheckMutBorrowVarUse(const VarDecl *VD, const SourceLocation &Loc,
                            std::string borrowFieldPath = "");
  void CheckMutBorrowFieldUse(const VarDecl *VD, const SourceLocation &Loc,
                              std::string targetFieldPath,
                              std::string borrowFieldPath = "");
  void CheckConstBorrowVarUse(const VarDecl *VD, const SourceLocation &Loc,
                              std::string borrowFieldPath = "");
  void CheckConstBorrowFieldUse(const VarDecl *VD, const SourceLocation &Loc,
                                std::string targetFieldPath,
                                std::string borrowFieldPath = "");

  void CheckNonBorrowVar(const VarDecl *VD, const SourceLocation &Loc);
  void CheckNonBorrowVarWrite(const VarDecl *VD, const SourceLocation &Loc);
  void CheckNonBorrowVarRead(const VarDecl *VD, const SourceLocation &Loc);

  void CheckNonBorrowField(const VarDecl *VD, const SourceLocation &Loc,
                           std::string fieldName);
  void CheckNonBorrowFieldWrite(const VarDecl *VD, const SourceLocation &Loc,
                                std::string fieldName);
  void CheckNonBorrowFieldRead(const VarDecl *VD, const SourceLocation &Loc,
                               std::string fieldName);

  BorrowInfo GetBorrowInfo(const VarDecl *VD, std::string fieldName = "");
};

// check current variable has been borrowed
// for a borrow qualified variable, function returns the borrowee
// for a non borrow qualified variable, function returns the borrower
VarDecl *
BorrowRuleChecker::FindTargetFromActiveBorrowTargetMap(const VarDecl *VD,
                                                       string borrowFieldName) {
  if (VD->getType().isBorrowQualified() ||
      VD->getType().getCanonicalType()->hasBorrowFields()) {
    for (const auto &v : ActiveBorrowTargetMap) {
      auto it = v.second.rbegin();
      while (it != v.second.rend()) {
        if (it->BorrowVD == VD && it->BorrowFieldPath == borrowFieldName) {
          return v.first;
        }
        it++;
      }
    }
  } else {
    if (VD->getType()->isPointerType() && !VD->getType().isOwnedQualified()) {
      for (const auto &v : ActiveBorrowTargetMap) {
        if ((v.first)->getBeginLoc() == VD->getBeginLoc() &&
            (v.first)->getLocation() == VD->getLocation()) {
          return v.first;
        }
      }
    } else {
      for (const auto &v : ActiveBorrowTargetMap) {
        if (v.first == VD) {
          return v.first;
        }
      }
    }
  }

  return nullptr;
}

// visit variable as a whole
void BorrowRuleChecker::VisitDeclRefExpr(DeclRefExpr *DRE) {
  if (Op == None) {
    return;
  }
  HandleDRE(DRE, "", false);
}

void BorrowRuleChecker::VisitUnaryOperator(UnaryOperator *UO) {
  Operation TempOp = Op;
  switch (UO->getOpcode()) {
  case UO_PostInc:
  case UO_PostDec:
  case UO_PreInc:
  case UO_PreDec:
    Op = Write;
    break;
  case UO_AddrMut:
  case UO_AddrConst:
  case UO_AddrMutDeref:
  case UO_AddrConstDeref:
    return;
  default:
    break;
  }
  Visit(UO->getSubExpr());
  Op = TempOp;
}

void BorrowRuleChecker::VisitCallExpr(CallExpr *CE) {
  Operation TempOp = Op;
  Op = Write;
  for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it) {
    Visit(*it);
  }
  Op = TempOp;
}

void BorrowRuleChecker::VisitConditionalOperator(ConditionalOperator *E) {
  Visit(E->getCond());
  Visit(E->getTrueExpr());
  Visit(E->getFalseExpr());
}

void BorrowRuleChecker::VisitInitListExpr(InitListExpr *E) {
  for (unsigned i = 0, e = E->getNumInits(); i != e; ++i) {
    if (Expr *Init = E->getInit(i))
      Visit(Init);
  }
}

void BorrowRuleChecker::VisitCastExpr(CastExpr *E) { Visit(E->getSubExpr()); }

void BorrowRuleChecker::VisitParenExpr(ParenExpr *E) { Visit(E->getSubExpr()); }

void BorrowRuleChecker::VisitReturnStmt(ReturnStmt *RS) {
  if (RS->getRetValue()) {
    Operation TempOp = Op;
    Op = Write;
    Visit(RS->getRetValue());
    Op = TempOp;
  }
}

static std::pair<const Expr *, std::string> getMemberField(const MemberExpr *ME,
                                                           bool &borrowFlag) {
  const Expr *base = ME->getBase();
  borrowFlag = ME->getType().isBorrowQualified();
  std::string memberFieldName = "." + ME->getMemberNameInfo().getAsString(),
              borrowFieldName;
  if (borrowFlag) {
    borrowFieldName = "." + ME->getMemberNameInfo().getAsString();
  }

  while (true) {
    if (const MemberExpr *me = dyn_cast<MemberExpr>(base)) {
      borrowFlag |= me->getType().isBorrowQualified();
      base = me->getBase();
      memberFieldName =
          me->getMemberNameInfo().getAsString() + "." + memberFieldName;
      if (borrowFlag) {
        borrowFieldName =
            me->getMemberNameInfo().getAsString() + "." + borrowFieldName;
      }
    } else if (const ImplicitCastExpr *ice = dyn_cast<ImplicitCastExpr>(base)) {
      base = ice->getSubExpr();
    } else {
      break;
    }
  }

  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(base)) {
    if (DRE->getDecl()->getType().getCanonicalType().isBorrowQualified()) {
      if (!borrowFlag) {
        borrowFlag = true;
        return {base, ""};
      }
    }
  }

  if (borrowFlag) {
    return {base, borrowFieldName};
  }

  return {base, memberFieldName};
}

void BorrowRuleChecker::VisitMemberExpr(MemberExpr *ME) {
  if (Op == None) {
    return;
  }

  bool borrowFlag = false;
  auto memberField = getMemberField(ME, borrowFlag);

  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(memberField.first)) {
    HandleDRE(DRE, memberField.second, borrowFlag);
  }
}

void BorrowRuleChecker::VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
  Visit(E->getLHS());
}

void BorrowRuleChecker::VisitBinaryOperator(BinaryOperator *BO) {
  if (BO->isAssignmentOp()) {
    Operation TempOp = Op;
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(BO->getLHS())) {
      if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        if (VD->getType().isBorrowQualified()) {
          return;
        }
      }
    }
    Op = Write;
    Visit(BO->getLHS());
    Op = Read;
    Visit(BO->getRHS());
    Op = TempOp;
  }
}

void BorrowRuleChecker::VisitDeclStmt(DeclStmt *DS) {
  Operation TempOp = Op;
  Status = Init;
  for (auto *D : DS->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->getType().isBorrowQualified()) {
        return;
      }
      Expr *Init = VD->getInit();
      if (Init) {
        Op = Read;
        Visit(Init);
      }
    }
  }
  Status = NotInit;
  Op = TempOp;
}

void BorrowRuleChecker::HandleDRE(const DeclRefExpr *DRE,
                                  std::string fieldName, bool borrowFlag) {
  if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    if (fieldName == "") {
      if (VD->getType().isBorrowQualified()) {
        CheckBorrowVar(VD, DRE->getLocation());
      } else {
        CheckNonBorrowVar(VD, DRE->getLocation());
      }
    } else {
      if (borrowFlag) {
        CheckBorrowField(VD, DRE->getLocation(), fieldName);
      } else {
        CheckNonBorrowField(VD, DRE->getLocation(), fieldName);
      }
    }
  }
}

// for a borrow var or a borrow field, return the BorrowInfo
// BorrowInfo.BorrowVD == VD
BorrowInfo BorrowRuleChecker::GetBorrowInfo(const VarDecl *VD,
                                            string fieldName) {
  // FIXME this condition is not suitable
  if (VD->getType().isBorrowQualified() || VD->getType()->hasBorrowFields()) {
    for (const auto &v : ActiveBorrowTargetMap) {
      auto it = v.second.rbegin();
      while (it != v.second.rend()) {
        if (it->BorrowVD == VD) {
          if (fieldName == "") {
            return *it;
          } else if (fieldName == it->BorrowFieldPath) {
            return *it;
          }
        }
        it++;
      }
    }
  }

  return BorrowInfo();
}

// VD is the borrow pointer variable
void BorrowRuleChecker::CheckBorrowVar(const VarDecl *VD,
                                       const SourceLocation &Loc) {
  BorrowInfo BI = GetBorrowInfo(VD);
  if (!VD->getType().isConstBorrow()) {
    if (BI.TargetFieldPath == "") {
      CheckMutBorrowVarUse(VD, Loc);
    } else {
      CheckMutBorrowFieldUse(VD, Loc, BI.TargetFieldPath);
    }
  } else {
    if (BI.TargetFieldPath == "") {
      CheckConstBorrowVarUse(VD, Loc);
    } else {
      CheckConstBorrowFieldUse(VD, Loc, BI.TargetFieldPath);
    }
  }
}

void BorrowRuleChecker::CheckBorrowField(const VarDecl *VD,
                                         const SourceLocation &Loc,
                                         std::string fieldName) {
  BorrowInfo BI = GetBorrowInfo(VD, fieldName);
  if (BI.Kind == BorrowKind::Mut) {
    if (BI.TargetFieldPath == "") {
      CheckMutBorrowVarUse(VD, Loc, BI.BorrowFieldPath);
    } else {
      CheckMutBorrowFieldUse(VD, Loc, BI.TargetFieldPath, BI.BorrowFieldPath);
    }
  } else {
    if (BI.TargetFieldPath == "") {
      CheckConstBorrowVarUse(VD, Loc, BI.BorrowFieldPath);
    } else {
      CheckConstBorrowFieldUse(VD, Loc, BI.TargetFieldPath, BI.BorrowFieldPath);
    }
  }
}

// for mut borrowed variable, only allow use the last alive mut borrow
// variable. VD is the mut borrow pointer variable
void BorrowRuleChecker::CheckMutBorrowVarUse(const VarDecl *VD,
                                             const SourceLocation &Loc,
                                             string borrowFieldPath) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD, borrowFieldPath)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      if (it->BorrowVD->getNameAsString() == "_ReturnVar") {
        it++;
        continue;
      }
      if (it->BorrowVD == VD) {
        return;
      } else {
        // Only allow used the last alive mut borrow variable.
        BorrowCheckDiagInfo DI(VD->getNameAsString() + borrowFieldPath,
                               AtMostOneMutBorrow, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
    }
  }
}

// for mut borrowed field, borrow to itself, its parent and its child are not
// allowed to exist VD is the mut borrow pointer variable
void BorrowRuleChecker::CheckMutBorrowFieldUse(const VarDecl *VD,
                                               const SourceLocation &Loc,
                                               string targetFieldPath,
                                               string borrowFieldPath) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD, borrowFieldPath)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      if (it->BorrowVD == VD && it->TargetFieldPath == targetFieldPath) {
        return;
      }
      // borrow to itself and its parent exist
      if (IsPrefix(it->TargetFieldPath, targetFieldPath)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString() + it->TargetFieldPath,
                               AtMostOneMutBorrow, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      // borrow to its child exist
      if (IsPrefix(targetFieldPath, it->TargetFieldPath)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString() + it->TargetFieldPath,
                               AtMostOneMutBorrow, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      it++;
    }
  }
}

// for const borrowed variable, mut borrow variables are not allowed to exist
// VD is the mut borrow pointer variable
void BorrowRuleChecker::CheckConstBorrowVarUse(const VarDecl *VD,
                                               const SourceLocation &Loc,
                                               string borrowFieldPath) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD, borrowFieldPath)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      if (it->BorrowVD->getNameAsString() == "_ReturnVar") {
        it++;
        continue;
      }
      if (it->BorrowVD == VD) {
        return;
      }
      if (!(it->Kind == BorrowKind::Immut)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString() + borrowFieldPath,
                               AtMostOneMutBorrow, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      it++;
    }
  }
}

// for const borrowed field, mut borrow to itself, its parent and its child are
// not allowed to exist VD is the mut borrow pointer variable
void BorrowRuleChecker::CheckConstBorrowFieldUse(const VarDecl *VD,
                                                 const SourceLocation &Loc,
                                                 string targetFieldPath,
                                                 string borrowFieldPath) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD, borrowFieldPath)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      if (it->BorrowVD == VD && it->TargetFieldPath == targetFieldPath) {
        return;
      }
      if (!(it->Kind == BorrowKind::Immut)) {
        // mut borrow to itself and its parent exist
        if (IsPrefix(it->TargetFieldPath, targetFieldPath)) {
          BorrowCheckDiagInfo DI(VD->getNameAsString() + it->TargetFieldPath,
                                 AtMostOneMutBorrow, Loc,
                                 it->BorrowVD->getLocation());
          reporter.addDiagInfo(DI);
          return;
        }
        // mut borrow to its child exist
        if (IsPrefix(targetFieldPath, it->TargetFieldPath)) {
          BorrowCheckDiagInfo DI(VD->getNameAsString() + it->TargetFieldPath,
                                 AtMostOneMutBorrow, Loc,
                                 it->BorrowVD->getLocation());
          reporter.addDiagInfo(DI);
          return;
        }
      }
      it++;
    }
  }
}

static void
FindBorrowFieldsOfStruct(RecordDecl *RD,
                         llvm::SmallVector<std::pair<std::string, BorrowKind>>
                             &BorrowFieldsOfStruct);

void BorrowRuleChecker::CheckNonBorrowVar(const VarDecl *VD,
                                          const SourceLocation &Loc) {
  if (Op == Write) {
    CheckNonBorrowVarWrite(VD, Loc);
  }

  if (Op == Read) {
    CheckNonBorrowVarRead(VD, Loc);
  }
  QualType QT = VD->getType().getCanonicalType();
  if (Status == NotInit && QT->isRecordType() && QT->hasBorrowFields()) {
    llvm::SmallVector<std::pair<std::string, BorrowKind>> BorrowFieldsOfStruct;
    FindBorrowFieldsOfStruct(QT->getAsRecordDecl(), BorrowFieldsOfStruct);
    for (auto &Field : BorrowFieldsOfStruct) {
      CheckBorrowField(VD, Loc, Field.first);
    }
  }
}

// for struct type or primitive type, `any borrow` is not allowed when writing
void BorrowRuleChecker::CheckNonBorrowVarWrite(const VarDecl *VD,
                                               const SourceLocation &Loc) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    // Variables cannot be modified when be borrowed
    BorrowCheckDiagInfo DI(VD->getNameAsString(), ModifyAfterBeBorrowed, Loc,
                           it->BorrowVD->getLocation());
    reporter.addDiagInfo(DI);
  }
}

// for struct type or primitive type, `any mutable borrow` is not allowed when
// reading
void BorrowRuleChecker::CheckNonBorrowVarRead(const VarDecl *VD,
                                              const SourceLocation &Loc) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      // Variables cannot be read when be mut borrowed
      if (!(it->Kind == BorrowKind::Immut)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString(), ReadAfterBeMutBorrowed,
                               Loc, it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      it++;
    }
  }
}

void BorrowRuleChecker::CheckNonBorrowField(const VarDecl *VD,
                                            const SourceLocation &Loc,
                                            string fieldName) {
  if (Op == Write) {
    CheckNonBorrowFieldWrite(VD, Loc, fieldName);
  }

  if (Op == Read) {
    CheckNonBorrowFieldRead(VD, Loc, fieldName);
  }
  QualType QT = VD->getType().getCanonicalType();
  if (Status == NotInit && QT->isRecordType() && QT->hasBorrowFields()) {
    llvm::SmallVector<std::pair<std::string, BorrowKind>> BorrowFieldsOfStruct;
    FindBorrowFieldsOfStruct(QT->getAsRecordDecl(), BorrowFieldsOfStruct);
    for (auto &Field : BorrowFieldsOfStruct) {
      if (IsPrefix(fieldName, Field.first)) {
        CheckBorrowField(VD, Loc, Field.first);
      }
    }
  }
}

// for struct field, `any borrow to itself, its parent and its child` is not
// allowed when writing
void BorrowRuleChecker::CheckNonBorrowFieldWrite(const VarDecl *VD,
                                                 const SourceLocation &Loc,
                                                 string fieldName) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      // borrow to itself and its parent exist
      if (IsPrefix(it->TargetFieldPath, fieldName)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString() + fieldName,
                               ModifyAfterBeBorrowed, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      // borrow to its child exist
      if (IsPrefix(fieldName, it->TargetFieldPath)) {
        BorrowCheckDiagInfo DI(VD->getNameAsString() + fieldName,
                               ModifyAfterBeBorrowed, Loc,
                               it->BorrowVD->getLocation());
        reporter.addDiagInfo(DI);
        return;
      }
      it++;
    }
  }
}

// for struct field, `any mut borrow to itself, its parent and its child` is not
// allowed when writing
void BorrowRuleChecker::CheckNonBorrowFieldRead(const VarDecl *VD,
                                                const SourceLocation &Loc,
                                                string fieldName) {
  if (VarDecl *TVD = FindTargetFromActiveBorrowTargetMap(VD)) {
    auto it = ActiveBorrowTargetMap[TVD].rbegin();
    while (it != ActiveBorrowTargetMap[TVD].rend()) {
      // mut borrow to itself and its parent exist
      if (it->Kind == BorrowKind::Mut) {
        if (IsPrefix(it->TargetFieldPath, fieldName)) {
          BorrowCheckDiagInfo DI(VD->getNameAsString() + fieldName,
                                 ModifyAfterBeBorrowed, Loc,
                                 it->BorrowVD->getLocation());
          reporter.addDiagInfo(DI);
          return;
        }
        // borrow to its child exist
        if (IsPrefix(fieldName, it->TargetFieldPath)) {
          BorrowCheckDiagInfo DI(VD->getNameAsString() + fieldName,
                                 ModifyAfterBeBorrowed, Loc,
                                 it->BorrowVD->getLocation());
          reporter.addDiagInfo(DI);
          return;
        }
      }
      it++;
    }
  }
}

void BorrowRuleChecker::UpdateActiveBorrowTargetMap() {
  for (const auto &v : BorrowTargetMap) {
    for (const auto &w : v.second) {
      unsigned firstElemId = w.Begin;
      if (CurElemID == firstElemId) {
        ActiveBorrowTargetMap[v.first].push_back(w);
      }
    }
  }
}

void BorrowRuleChecker::ClearActiveBorrowTargetMap() {
  for (const auto &v : BorrowTargetMap) {
    for (const auto &w : v.second) {
      unsigned secondElemId = w.End;
      if (CurElemID == secondElemId) {
        ActiveBorrowTargetMap[v.first].remove(w);
      }
    }
    if (ActiveBorrowTargetMap[v.first].empty()) {
      ActiveBorrowTargetMap.erase(v.first);
    }
  }
}

void BorrowRuleChecker::CheckBeBorrowedTarget(const CFGPath &Path) {
  // add global variable
  CurElemID = 0;
  UpdateActiveBorrowTargetMap();
  // add param variable
  CurElemID = 1;
  UpdateActiveBorrowTargetMap();
  ClearActiveBorrowTargetMap();
  for (auto blockIt = Path.begin(); blockIt != Path.end(); blockIt++) {
    const CFGBlock *block = *blockIt;

    for (CFGBlock::const_iterator elemIt = block->begin(),
                                  elemEI = block->end();
         elemIt != elemEI; ++elemIt) {
      const CFGElement &elem = *elemIt;
      CurElemID++;
      Op = None;
      UpdateActiveBorrowTargetMap();
      if (elem.getAs<CFGStmt>()) {
        const Stmt *S = elem.castAs<CFGStmt>().getStmt();
        Visit(const_cast<Stmt *>(S));
      }
      ClearActiveBorrowTargetMap();
    }
  }
}

void BorrowRuleChecker::BuildBorrowTargetMap() {
  for (const auto &NLLOfVar : NLLForAllVars) {
    VarDecl *BorrowVD = NLLOfVar.first;
    NonLexicalLifetimeOfVar NLLRangesOfVar = NLLOfVar.second;
    for (const auto &NLLRange : NLLRangesOfVar) {
      if (NLLRange.Target.TargetVD)
        BorrowTargetMap[NLLRange.Target.TargetVD].push_back(BorrowInfo(
            NLLRange.Target.TargetFieldPath, BorrowVD, NLLRange.BorrowFieldPath,
            NLLRange.Begin, NLLRange.End, NLLRange.Kind));
    }
  }
}
// Core rule of borrow:
//   the lifetime of borrow variable is shorter than its target variable.
void BorrowRuleChecker::CheckBorrowNLLShorterThanTarget() {
  for (const auto &BorrowAndTarget : BorrowTargetMap) {
    VarDecl *TargetVD = BorrowAndTarget.first;
    for (const auto &Borrow : BorrowAndTarget.second) {
      unsigned BorrowBegin = Borrow.Begin;
      unsigned BorrowEnd = Borrow.End;
      for (const auto &NLLOfTarget : NLLForAllVars[TargetVD]) {
        if (NLLOfTarget.Target.TargetFieldPath == "") {
          unsigned TargetBegin = NLLOfTarget.Begin;
          unsigned TargetEnd = NLLOfTarget.End;
          if (BorrowBegin <= TargetBegin || BorrowEnd >= TargetEnd) {
            BorrowCheckDiagInfo DI(Borrow.BorrowVD->getNameAsString(),
                                   LiveLonger, Borrow.BorrowVD->getLocation());
            reporter.addDiagInfo(DI);
          }
        }
      }
    }
  }
}

// Function cannot return a borrow of local variables.
// For example:
// @code
//   int global = 5;
//   const int* borrow test(const int* borrow p) {
//     int local = 5;
//     const int* borrow q = &mut local;
//     return &const global;    // right
//     // return p;             // right
//     // return &const local;  // error
//     // return q;             // error
//   }
// @endcode
void BorrowRuleChecker::CheckLifetimeOfBorrowReturnValue(unsigned NumElements) {
  for (const auto &NLLOfVar : NLLForAllVars) {
    if (NLLOfVar.first->getNameAsString() == "_ReturnVar") {
      for (const NonLexicalLifetimeRange &Range : NLLOfVar.second) {
        for (const auto &TargetNLLRange :
             NLLForAllVars[Range.Target.TargetVD]) {
          unsigned TargetNLLBegin = TargetNLLRange.Begin;
          unsigned TargetNLLEnd = TargetNLLRange.End;
          if (TargetNLLBegin > 0 || TargetNLLEnd < NumElements + 3) {
            BorrowCheckDiagInfo DI(Range.Target.TargetVD->getNameAsString(),
                                   ReturnLocal, NLLOfVar.first->getLocation());
            reporter.addDiagInfo(DI);
            return;
          }
        }
      }
    }
  }
}

static void FindBorrowFieldsOfStructDFS(
    FieldDecl *CurrFD, std::string FP,
    llvm::SmallVector<std::pair<std::string, BorrowKind>>
        &BorrowFieldsOfStruct) {
  QualType FQT = CurrFD->getType().getCanonicalType();
  if (FQT.isBorrowQualified()) {
    BorrowKind BK = FQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
    FP = FP + "." + CurrFD->getNameAsString();
    BorrowFieldsOfStruct.push_back(std::pair<std::string, BorrowKind>(FP, BK));
  } else if (FQT->hasBorrowFields()) {
    FP = FP + "." + CurrFD->getNameAsString();
    if (auto RT = dyn_cast<RecordType>(FQT)) {
      for (FieldDecl *FD : RT->getDecl()->fields())
        FindBorrowFieldsOfStructDFS(FD, FP, BorrowFieldsOfStruct);
    }
  }
}

static void
FindBorrowFieldsOfStruct(RecordDecl *RD,
                         llvm::SmallVector<std::pair<std::string, BorrowKind>>
                             &BorrowFieldsOfStruct) {
  for (FieldDecl *FD : RD->fields()) {
    QualType FQT = FD->getType().getCanonicalType();
    if (FQT.isBorrowQualified()) {
      BorrowKind BK = FQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
      std::string FP = "." + FD->getNameAsString();
      BorrowFieldsOfStruct.push_back(
          std::pair<std::string, BorrowKind>(FP, BK));
    } else if (FQT->hasBorrowFields()) {
      FindBorrowFieldsOfStructDFS(FD, "", BorrowFieldsOfStruct);
    }
  }
}

void NLLCalculator::VisitBinaryOperator(BinaryOperator *BO) {
  // A borrow variable is reassigned, we would handle it like DeclStmt.
  // For example:
  // @code
  //   int* borrow p = &mut local1;
  //   p = &mut local2;    // We handle it like `int* borrow p = &mut local2;`
  // @endcode
  if (BO->isAssignmentOp()) {
    QualType QT = BO->getLHS()->getType().getCanonicalType();
    if (QT.isBorrowQualified()) {
      BorrowKind BK = QT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
      llvm::SmallVector<BorrowTargetInfo> Targets;
      VisitInitForTargets(BO->getRHS(), Targets);
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(BO->getLHS())) {
        if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
          // p = &mut local;
          UpdateNLLWhenTargetFound(BorrowTargetInfo(VD), Targets);
          for (BorrowTargetInfo Target : Targets)
            NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
                CurElemID, FoundVars.count(VD) ? FoundVars[VD] : CurElemID, BK,
                Target));
          FoundVars.erase(VD);
        }
      } else if (MemberExpr *ME = dyn_cast<MemberExpr>(BO->getLHS())) {
        // s.a.b = &mut local;
        std::pair<VarDecl *, std::string> BorrowWithFieldPath;
        VisitMEForFieldPath(ME, BorrowWithFieldPath);
        UpdateNLLWhenTargetFound(BorrowTargetInfo(BorrowWithFieldPath.first,
                                                  BorrowWithFieldPath.second),
                                 Targets);
        for (BorrowTargetInfo Target : Targets)
          NLLForAllVars[BorrowWithFieldPath.first].push_back(
              NonLexicalLifetimeRange(
                  CurElemID,
                  FoundBorrowFields.count(BorrowWithFieldPath)
                      ? FoundBorrowFields[BorrowWithFieldPath]
                      : CurElemID,
                  BK, Target, BorrowWithFieldPath.second));
        FoundBorrowFields.erase(BorrowWithFieldPath);
      }
    } else if (QT->hasBorrowFields()) {
      if (RecordDecl *RD = dyn_cast<RecordType>(QT)->getDecl()) {
        if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(BO->getLHS())) {
          if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
            // s = s1;
            VisitInitForBorrowFieldTargets(VD, "", RD, BO->getRHS());
        } else if (MemberExpr *ME = dyn_cast<MemberExpr>(BO->getLHS())) {
          // s.a = a1;
          std::pair<VarDecl *, std::string> BorrowWithFieldPath;
          VisitMEForFieldPath(ME, BorrowWithFieldPath);
          VisitInitForBorrowFieldTargets(BorrowWithFieldPath.first,
                                         BorrowWithFieldPath.second, RD,
                                         BO->getRHS());
        }
      }
    } else {
      CurrOpIsBorrowUse = true;
      Visit(BO->getLHS());
      Visit(BO->getRHS());
      CurrOpIsBorrowUse = false;
    }
  }
}

void NLLCalculator::VisitDeclRefExpr(DeclRefExpr *DRE) {
  if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    // Add global variables to record which globals are used.
    // Also add owned variables(local or parameter) to record the last location
    // used.
    if (((!VD->isLocalVarDecl() && !isa<ParmVarDecl>(VD)) ||
         VD->getType().isOwnedQualified()) &&
        !FoundVars.count(VD))
      FoundVars[VD] = CurElemID;

    if (CurrOpIsBorrowUse) {
      QualType VQT = VD->getType().getCanonicalType();
      if (VQT.isBorrowQualified() && !FoundVars.count(VD))
        FoundVars[VD] = CurElemID;
      else if (VQT->hasBorrowFields()) {
        if (auto RT = dyn_cast<RecordType>(VQT)) {
          llvm::SmallVector<std::pair<std::string, BorrowKind>>
              BorrowFieldsOfStruct;
          FindBorrowFieldsOfStruct(RT->getDecl(), BorrowFieldsOfStruct);
          for (const auto &FieldPath : BorrowFieldsOfStruct) {
            std::pair<VarDecl *, std::string> BorrowWithFieldPath(
                VD, FieldPath.first);
            if (!FoundBorrowFields.count(BorrowWithFieldPath))
              FoundBorrowFields[BorrowWithFieldPath] = CurElemID;
          }
        }
      }
    }
  }
}

void NLLCalculator::VisitReturnStmt(ReturnStmt *RS) {
  Expr *RV = RS->getRetValue();
  if (!RV)
    return;
  QualType ReturnQT = RV->getType().getCanonicalType();
  if (!ReturnQT.hasBorrow())
    return;

  // In order to check lifetime of borrow return value, we build a virtual
  // ReturnVar.
  std::string Name = "_ReturnVar";
  IdentifierInfo *ID = &Ctx.Idents.get(Name);
  VarDecl *ReturnVD =
      VarDecl::Create(Ctx, DC, RV->getBeginLoc(), RV->getBeginLoc(), ID,
                      ReturnQT, nullptr, SC_None);
  if (ReturnQT.isBorrowQualified()) {
    BorrowKind BK =
        ReturnQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
    // ReturnVar itself only survive in current ReturnStmt,
    // but we still should get its target to check if this function returns a
    // borrow of local variable.
    llvm::SmallVector<BorrowTargetInfo> Targets;
    VisitInitForTargets(RV, Targets);
    for (BorrowTargetInfo Target : Targets)
      NLLForAllVars[ReturnVD].push_back(
          NonLexicalLifetimeRange(CurElemID, CurElemID, BK, Target));
  } else if (ReturnQT->hasBorrowFields()) {
    if (RecordDecl *RD = dyn_cast<RecordType>(ReturnQT)->getDecl())
      VisitInitForBorrowFieldTargets(ReturnVD, "", RD, RV);
  }

  // For return stmt `return p;`, `return &mut* p`, `return *p;` or `return
  // p->a` we think borrow pointer `p` is used.
  CurrOpIsBorrowUse = true;
  Visit(RV);
  CurrOpIsBorrowUse = false;
}

void NLLCalculator::VisitMemberExpr(MemberExpr *ME) {
  if (CurrOpIsBorrowUse) {
    QualType MQT = ME->getType().getCanonicalType();
    if (MQT.isBorrowQualified()) {
      // Use borrow pointer directly, such as `s.p`
      std::pair<VarDecl *, std::string> BorrowWithFieldPath;
      VisitMEForFieldPath(ME, BorrowWithFieldPath);
      if (!FoundBorrowFields.count(BorrowWithFieldPath))
        FoundBorrowFields[BorrowWithFieldPath] = CurElemID;
    } else if (MQT->hasBorrowFields()) {
      // Use borrow pointer indirectly, such as `s.a`, a is a struct type and
      // has borrow fields.
      std::pair<VarDecl *, std::string> BorrowWithFieldPath;
      VisitMEForFieldPath(ME, BorrowWithFieldPath);
      if (auto RT = dyn_cast<RecordType>(MQT)) {
        llvm::SmallVector<std::pair<std::string, BorrowKind>>
            BorrowFieldsOfStruct;
        FindBorrowFieldsOfStruct(RT->getDecl(), BorrowFieldsOfStruct);
        for (const auto &FieldPath : BorrowFieldsOfStruct) {
          std::pair<VarDecl *, std::string> BorrowWithFieldPathCopy(
              BorrowWithFieldPath);
          BorrowWithFieldPathCopy.second =
              BorrowWithFieldPathCopy.second + FieldPath.first;
          if (!FoundBorrowFields.count(BorrowWithFieldPathCopy))
            FoundBorrowFields[BorrowWithFieldPathCopy] = CurElemID;
        }
      }
    } else
      // Use borrow pointer indirectly, such as `p->a`
      Visit(ME->getBase());
  }
}

void NLLCalculator::VisitCallExpr(CallExpr *CE) {
  // For function call `use(p)`, `use(&mut* p)`, `use(*p)` or `use(p->a)`,
  // we think borrow pointer `p` is used here.
  CurrOpIsBorrowUse = true;
  for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it)
    Visit(*it);
  CurrOpIsBorrowUse = false;
}

void NLLCalculator::VisitImplicitCastExpr(ImplicitCastExpr *ICE) {
  if (CurrOpIsBorrowUse)
    Visit(ICE->getSubExpr());
}

void NLLCalculator::VisitParenExpr(ParenExpr *PE) {
  if (CurrOpIsBorrowUse)
    Visit(PE->getSubExpr());
}

void NLLCalculator::VisitUnaryOperator(UnaryOperator *UO) {
  if (CurrOpIsBorrowUse)
    Visit(UO->getSubExpr());
}

void NLLCalculator::VisitDeclStmt(DeclStmt *DS) {
  for (auto *D : DS->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      QualType VQT = VD->getType().getCanonicalType();
      if (VQT.isBorrowQualified()) {
        BorrowKind BK =
            VQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
        // VD itself is a borrow, int* borrow p = &mut local;
        llvm::SmallVector<BorrowTargetInfo> Targets;
        VisitInitForTargets(VD->getInit(), Targets);
        // When we find the target of a borrow,
        // we should update previous target in NLLForAllVars.
        UpdateNLLWhenTargetFound(BorrowTargetInfo(VD), Targets);
        for (BorrowTargetInfo Target : Targets)
          NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
              CurElemID, FoundVars.count(VD) ? FoundVars[VD] : CurElemID, BK,
              Target));
        FoundVars.erase(VD);
      } else if (VQT->hasBorrowFields()) {
        if (RecordDecl *RD = dyn_cast<RecordType>(VQT)->getDecl())
          VisitInitForBorrowFieldTargets(VD, "", RD, VD->getInit());
      } else if (VD->getInit()) {
        CurrOpIsBorrowUse = true;
        Visit(VD->getInit());
        CurrOpIsBorrowUse = false;
      }
    }
  }
}

// This function finds targets for struct with borrow fields.
void NLLCalculator::VisitInitForBorrowFieldTargets(VarDecl *VD, std::string FP,
                                                   RecordDecl *RD,
                                                   Expr *InitE) {
  if (auto InitListE = dyn_cast<InitListExpr>(InitE)) {
    // Init by InitListExpr, struct S s = { .a = &mut local };
    Expr **Inits = InitListE->getInits();
    for (const auto &FD : RD->fields()) {
      Expr *FieldInit = Inits[FD->getFieldIndex()];
      VisitFieldInitForBorrowFieldTargets(FD, VD, FieldInit);
    }
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(InitE)) {
    // Init by another struct
    llvm::SmallVector<std::pair<std::string, BorrowKind>> BorrowFieldsOfStruct;
    FindBorrowFieldsOfStruct(RD, BorrowFieldsOfStruct);
    for (const auto &FieldPath : BorrowFieldsOfStruct) {
      BorrowTargetInfo Target;
      llvm::SmallVector<BorrowTargetInfo> Targets;
      std::pair<VarDecl *, std::string> BorrowWithFieldPath(
          VD, FP + FieldPath.first);
      if (auto DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr())) {
        // For example:
        // @code
        //     struct S s = s1;
        //     s = s1;
        //     s.a = s1;
        // @endcode
        if (VarDecl *IVD = dyn_cast<VarDecl>(DRE->getDecl())) {
          Target.TargetVD = IVD;
          Target.TargetFieldPath = FieldPath.first;
        }
      } else if (auto ME = dyn_cast<MemberExpr>(ICE->getSubExpr())) {
        // For example:
        // @code        
        //     struct S s = s1.a;
        //     s = s1.a;
        //     s.a = s1.a;
        // @endcode
        std::pair<VarDecl *, std::string> TargetWithFieldPath;
        VisitMEForFieldPath(ME, TargetWithFieldPath);
        Target.TargetVD = TargetWithFieldPath.first;
        Target.TargetFieldPath = TargetWithFieldPath.second + FieldPath.first;
      }
      Targets.push_back(Target);
      UpdateNLLWhenTargetFound(BorrowTargetInfo(BorrowWithFieldPath.first,
                                                BorrowWithFieldPath.second),
                               Targets);
      NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
          CurElemID,
          FoundBorrowFields.count(BorrowWithFieldPath)
              ? FoundBorrowFields[BorrowWithFieldPath]
              : CurElemID,
          FieldPath.second, Target, BorrowWithFieldPath.second));
      FoundBorrowFields.erase(BorrowWithFieldPath);
    }
  } else if (auto CE = dyn_cast<CallExpr>(InitE)) {
    llvm::SmallVector<BorrowTargetInfo> Targets;
    VisitCallExprForBorrowFieldTargets(CE, Targets);
    llvm::SmallVector<std::pair<std::string, BorrowKind>> BorrowFieldsOfStruct;
    FindBorrowFieldsOfStruct(RD, BorrowFieldsOfStruct);
    for (const auto &FieldPath : BorrowFieldsOfStruct) {
      UpdateNLLWhenTargetFound(BorrowTargetInfo(VD, FP + FieldPath.first),
                               Targets);
      std::pair<VarDecl *, std::string> BorrowWithFieldPath;
      BorrowWithFieldPath.first = VD;
      BorrowWithFieldPath.second = FP + FieldPath.first;
      for (const auto &Target : Targets)
        NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
            CurElemID,
            FoundBorrowFields.count(BorrowWithFieldPath)
                ? FoundBorrowFields[BorrowWithFieldPath]
                : CurElemID,
            FieldPath.second, Target, FP + FieldPath.first));
      FoundBorrowFields.erase(BorrowWithFieldPath);
    }
  }
}

void NLLCalculator::VisitFieldInitForBorrowFieldTargets(FieldDecl *FD,
                                                        VarDecl *VD,
                                                        Expr *InitE) {
  QualType FQT = FD->getType().getCanonicalType();
  if (FQT.isBorrowQualified()) {
    BorrowKind BK = FQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
    llvm::SmallVector<BorrowTargetInfo> Targets;
    VisitInitForTargets(InitE, Targets);
    std::pair<VarDecl *, std::string> BorrowWithFieldPath;
    BorrowWithFieldPath.first = VD;
    BorrowWithFieldPath.second = "." + FD->getNameAsString();
    UpdateNLLWhenTargetFound(
        BorrowTargetInfo(BorrowWithFieldPath.first, BorrowWithFieldPath.second),
        Targets);
    for (BorrowTargetInfo Target : Targets)
      NLLForAllVars[VD].push_back(
          NonLexicalLifetimeRange(CurElemID,
                                  FoundBorrowFields.count(BorrowWithFieldPath)
                                      ? FoundBorrowFields[BorrowWithFieldPath]
                                      : CurElemID,
                                  BK, Target, BorrowWithFieldPath.second));
    FoundBorrowFields.erase(BorrowWithFieldPath);
  } else if (FQT->hasBorrowFields()) {
    if (auto RT = dyn_cast<RecordType>(FQT)) {
      RecordDecl *RD = RT->getDecl();
      if (auto ICE = dyn_cast<ImplicitCastExpr>(InitE)) {
        // Init by another struct
        llvm::SmallVector<std::pair<std::string, BorrowKind>>
            BorrowFieldsOfStruct;
        FindBorrowFieldsOfStruct(RD, BorrowFieldsOfStruct);
        for (const auto &FieldPath : BorrowFieldsOfStruct) {
          BorrowTargetInfo Target;
          llvm::SmallVector<BorrowTargetInfo> Targets;
          std::pair<VarDecl *, std::string> BorrowWithFieldPath;
          BorrowWithFieldPath.first = VD;
          BorrowWithFieldPath.second =
              "." + FD->getNameAsString() + FieldPath.first;
          if (auto DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr())) {
            // .a = s1;`
            if (VarDecl *IVD = dyn_cast<VarDecl>(DRE->getDecl())) {
              Target.TargetVD = IVD;
              Target.TargetFieldPath = FieldPath.first;
            }
          } else if (auto ME = dyn_cast<MemberExpr>(ICE->getSubExpr())) {
            // .a = s1.a;`
            std::pair<VarDecl *, std::string> TargetWithFieldPath;
            VisitMEForFieldPath(ME, TargetWithFieldPath);
            Target.TargetVD = TargetWithFieldPath.first;
            Target.TargetFieldPath =
                TargetWithFieldPath.second + FieldPath.first;
          }
          Targets.push_back(Target);
          UpdateNLLWhenTargetFound(BorrowTargetInfo(BorrowWithFieldPath.first,
                                                    BorrowWithFieldPath.second),
                                   Targets);
          NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
              CurElemID,
              FoundBorrowFields.count(BorrowWithFieldPath)
                  ? FoundBorrowFields[BorrowWithFieldPath]
                  : CurElemID,
              FieldPath.second, Target, BorrowWithFieldPath.second));
          FoundBorrowFields.erase(BorrowWithFieldPath);
        }
      } else if (auto CE = dyn_cast<CallExpr>(InitE)) {
        llvm::SmallVector<BorrowTargetInfo> Targets;
        VisitCallExprForBorrowFieldTargets(CE, Targets);
        llvm::SmallVector<std::pair<std::string, BorrowKind>>
            BorrowFieldsOfStruct;
        FindBorrowFieldsOfStruct(RD, BorrowFieldsOfStruct);
        for (const auto &FieldPath : BorrowFieldsOfStruct) {
          UpdateNLLWhenTargetFound(
              BorrowTargetInfo(VD,
                               "." + FD->getNameAsString() + FieldPath.first),
              Targets);
          std::pair<VarDecl *, std::string> BorrowWithFieldPath;
          BorrowWithFieldPath.first = VD;
          BorrowWithFieldPath.second =
              "." + FD->getNameAsString() + FieldPath.first;
          for (const auto &Target : Targets)
            NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(
                CurElemID,
                FoundBorrowFields.count(BorrowWithFieldPath)
                    ? FoundBorrowFields[BorrowWithFieldPath]
                    : CurElemID,
                FieldPath.second, Target,
                "." + FD->getNameAsString() + FieldPath.first));
          FoundBorrowFields.erase(BorrowWithFieldPath);
        }
      }
    }
  }
}

void NLLCalculator::VisitCallExprForBorrowFieldTargets(
    CallExpr *CE, llvm::SmallVector<BorrowTargetInfo> &Targets) {
  for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it) {
    if (Expr *ArgExpr = dyn_cast<Expr>(*it)) {
      QualType ArgQT = ArgExpr->getType().getCanonicalType();
      if (ArgQT.isBorrowQualified())
        VisitInitForTargets(ArgExpr, Targets);
      else if (ArgQT->hasBorrowFields()) {
        if (auto ICE = dyn_cast<ImplicitCastExpr>(ArgExpr)) {
          RecordDecl *ArgRD = dyn_cast<RecordType>(ArgQT)->getDecl();
          llvm::SmallVector<std::pair<std::string, BorrowKind>>
              BorrowFieldsOfStruct;
          FindBorrowFieldsOfStruct(ArgRD, BorrowFieldsOfStruct);
          for (const auto &FieldPath : BorrowFieldsOfStruct) {
            BorrowTargetInfo Target;
            if (auto DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr())) {
              if (VarDecl *ArgVD = dyn_cast<VarDecl>(DRE->getDecl())) {
                Target.TargetVD = ArgVD;
                Target.TargetFieldPath = FieldPath.first;
              }
            } else if (auto ME = dyn_cast<MemberExpr>(ICE->getSubExpr())) {
              std::pair<VarDecl *, std::string> TargetWithFieldPath;
              VisitMEForFieldPath(ME, TargetWithFieldPath);
              Target.TargetVD = TargetWithFieldPath.first;
              Target.TargetFieldPath =
                  TargetWithFieldPath.second + FieldPath.first;
            }
            Targets.push_back(Target);
          }
        }
      }
    }
  }
}

void NLLCalculator::VisitMEForFieldPath(
    MemberExpr *ME, std::pair<VarDecl *, std::string> &VDAndFP) {
  if (auto FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
    VDAndFP.second = "." + FD->getNameAsString() + VDAndFP.second;
    if (auto BaseME = dyn_cast<MemberExpr>(ME->getBase()))
      VisitMEForFieldPath(BaseME, VDAndFP);
    else if (auto BaseDRE = dyn_cast<DeclRefExpr>(ME->getBase())) {
      VarDecl *BaseVD = dyn_cast<VarDecl>(BaseDRE->getDecl());
      VDAndFP.first = BaseVD;
    }
  }
}

// This function finds targets for borrow pointer variables.
void NLLCalculator::VisitInitForTargets(
    Expr *InitE, llvm::SmallVector<BorrowTargetInfo> &Targets) {
  if (auto *DRE = dyn_cast<DeclRefExpr>(InitE)) {
    if (VarDecl *IVD = dyn_cast<VarDecl>(DRE->getDecl())) {
      QualType VQT = IVD->getType().getCanonicalType();
      if (VQT->hasBorrowFields()) {
        if (auto RT = dyn_cast<RecordType>(VQT)) {
          llvm::SmallVector<std::pair<std::string, BorrowKind>>
              BorrowFieldsOfStruct;
          FindBorrowFieldsOfStruct(RT->getDecl(), BorrowFieldsOfStruct);
          for (const auto &FieldPath : BorrowFieldsOfStruct)
            Targets.push_back(BorrowTargetInfo(IVD, FieldPath.first));
        }
      } else
        Targets.push_back(BorrowTargetInfo(IVD));
    }
    return;
  } else if (auto *ME = dyn_cast<MemberExpr>(InitE)) {
    // int* borrow p = s.a.b; `s.a.b` is MemberExpr.
    std::pair<VarDecl *, std::string> TargetWithFieldPath;
    VisitMEForFieldPath(ME, TargetWithFieldPath);
    QualType MQT = ME->getType().getCanonicalType();
    if (MQT->hasBorrowFields()) {
      if (auto RT = dyn_cast<RecordType>(MQT)) {
        llvm::SmallVector<std::pair<std::string, BorrowKind>>
            BorrowFieldsOfStruct;
        FindBorrowFieldsOfStruct(RT->getDecl(), BorrowFieldsOfStruct);
        for (const auto &FieldPath : BorrowFieldsOfStruct)
          Targets.push_back(
              BorrowTargetInfo(TargetWithFieldPath.first,
                               TargetWithFieldPath.second + FieldPath.first));
      }
    } else
      Targets.push_back(BorrowTargetInfo(TargetWithFieldPath.first,
                                         TargetWithFieldPath.second));
    return;
  }

  if (auto UO = dyn_cast<UnaryOperator>(InitE)) {
    if (UO->getOpcode() == UO_AddrMut || UO->getOpcode() == UO_AddrConst ||
        UO->getOpcode() == UO_AddrMutDeref ||
        UO->getOpcode() == UO_AddrConstDeref)
      VisitInitForTargets(UO->getSubExpr(), Targets);
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(InitE)) {
    // int* borrow p = p1;  `p1` is ImplicitCastExpr.
    VisitInitForTargets(ICE->getSubExpr(), Targets);
  } else if (auto ASE = dyn_cast<ArraySubscriptExpr>(InitE)) {
    // int* borrow p = &mut arr[0];  `arr[0]` is ArraySubscriptExpr.
    VisitInitForTargets(ASE->getBase(), Targets);
  } else if (auto CE = dyn_cast<CallExpr>(InitE)) {
    // int* borrow p = foo(&mut local1, &mut local2);
    // the lifetime of p should be smaller than the lifetime intersection of
    // local1 and local2. we add local1 and local2 to Targets.
    for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it)
      if (Expr *ArgExpr = dyn_cast<Expr>(*it))
        VisitInitForTargets(ArgExpr, Targets);
  } else if (auto CO = dyn_cast<ConditionalOperator>(InitE)) {
    VisitInitForTargets(CO->getLHS(), Targets);
    VisitInitForTargets(CO->getRHS(), Targets);
  }
}

void NLLCalculator::VisitScopeBegin(VarDecl *VD) {
  QualType QT = VD->getType();
  if (QT->isPointerType() && !QT.isOwnedQualified() &&
      !QT.isBorrowQualified()) {
    // For local naked pointer, we create virtual ParentVar as target.
    std::string Name = "_ParentVar_" + VD->getNameAsString();
    IdentifierInfo *ID = &Ctx.Idents.get(Name);
    VarDecl *VirtualVD =
        VarDecl::Create(Ctx, DC, VD->getBeginLoc(), VD->getLocation(), ID,
                        QualType(QT->getPointeeType()), nullptr, SC_None);
    // Update previous NLL whose target is this naked pointer VD to virtual
    // ParentVar
    llvm::SmallVector<BorrowTargetInfo> VirtualTarget;
    VirtualTarget.push_back(BorrowTargetInfo(VirtualVD));
    UpdateNLLWhenTargetFound(BorrowTargetInfo(VD), VirtualTarget);
    // Add NLL for virtual ParentVar.
    NLLForAllVars[VirtualVD].push_back(
        NonLexicalLifetimeRange(0, NumElements + 3));
  }
  if (!QT.isBorrowQualified()) {
    // For no-borrow local variable, its NLL is continous, and ScopeBegin marks
    // the begin of its NLL.
    NLLForAllVars[VD].push_back(
        NonLexicalLifetimeRange(CurElemID, FoundVars[VD]));
    FoundVars.erase(VD);
  }
}

void NLLCalculator::VisitScopeEnd(VarDecl *VD) {
  // For no-borrow and no-owned local variable, ScopeEnd marks the end of its
  // NLL.
  QualType QT = VD->getType();
  if (!QT.isBorrowQualified() && !QT.isOwnedQualified())
    FoundVars[VD] = CurElemID;
}

// When we find the target of a borrow,
// we should update previous target in NLLForAllVars.
// For such example:
// @code
//   int * borrow p1 = use_and_return(&mut local1, &mut local2); // #1   
//   //  p3:[3, 3]->local1, [3, 3]->local2 
//   //  p2:[2, 2]->local1, [2, 2]->local2
//   //  p1:[1, 1]->local1, [1, 1]->local2 
//   int * borrow p2 = p1; // #2   
//   //  p3:[3, 3]->p1
//   //  p2:[2, 2]->p1
//   int * borrow p3 = p2; // #3  
//   //  p3:[3, 3]->p2
// @endcode
// Because we traverse path in reverse order, we handle in order #3->#2->#1.
// When we handle #3, we know p3 targeting to p2,
// but we don't know p2 targeting to which.
// When we handle #2, we know p2 targeting to p1,
// we should update the target of p3 from p2 to p1.
// When we handle #1, we know p1 targeting to local1 and local2,
// we should update the target of p2 and p3 from p1 to local1 and local2.
void NLLCalculator::UpdateNLLWhenTargetFound(
    BorrowTargetInfo OldTarget,
    const llvm::SmallVector<BorrowTargetInfo> &NewTargets) {
  for (auto &NLLOfVar : NLLForAllVars) {
    NonLexicalLifetimeOfVar &NLLRanges = NLLOfVar.second;
    llvm::SmallVector<std::tuple<std::string, unsigned, unsigned, BorrowKind>>
        RangesNeedUpdate;
    // Remove ranges whose target is OldTarget
    for (auto it = NLLRanges.begin(); it != NLLRanges.end();) {
      if (it->Target == OldTarget) {
        RangesNeedUpdate.push_back(
            std::tuple<std::string, unsigned, unsigned, BorrowKind>(
                it->BorrowFieldPath, it->Begin, it->End, it->Kind));
        it = NLLRanges.erase(it);
      } else
        ++it;
    }
    // Build new ranges targeting to NewTarget and insert them to NLLRanges.
    for (std::tuple<std::string, unsigned, unsigned, BorrowKind>
             RangeNeedUpdate : RangesNeedUpdate) {
      for (BorrowTargetInfo NewTarget : NewTargets) {
        NonLexicalLifetimeRange NewRange(
            std::get<1>(RangeNeedUpdate), std::get<2>(RangeNeedUpdate),
            std::get<3>(RangeNeedUpdate), NewTarget,
            std::get<0>(RangeNeedUpdate));
        if (std::find(NLLRanges.begin(), NLLRanges.end(), NewRange) ==
            NLLRanges.end())
          NLLRanges.push_back(NewRange);
      }
    }
  }
}

// In order to calculate NLL for global and parameter,
// we use virtual CFGElement index to mark the begin and end of global variable
// and parameter
//   NLL begin of global/virtual ParentVar    :0
//   NLL begin of parameter                   :1
//   local variables                          :from 2 to NumElements + 1
//   NLL end of borrow or owned parameter     :last use location
//   NLL end of no-borrow-no-owned parameter  :NumElements + 2
//   NLL end of global/virtual ParentVar      :NumElements + 3
void NLLCalculator::HandleNLLAfterTraversing(
    const std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
                   BorrowTargetInfo> &ParamTarget) {
  // Handle parameter
  for (const auto &Param : ParamTarget) {
    ParmVarDecl *PVD = std::get<0>(Param.first);
    std::string PFP = std::get<1>(Param.first);
    BorrowKind PBK = std::get<2>(Param.first);
    QualType PQT = PVD->getType();
    if (PFP.size() > 0) {
      llvm::SmallVector<BorrowTargetInfo> VirtualTarget;
      VirtualTarget.push_back(Param.second);
      UpdateNLLWhenTargetFound(BorrowTargetInfo(PVD, PFP), VirtualTarget);
      // Add NLL for virtual ParentVar.
      NLLForAllVars[Param.second.TargetVD].push_back(
          NonLexicalLifetimeRange(0, NumElements + 3));
      // Add NLL for param borrow field with target.
      std::pair<VarDecl *, std::string> BorrowWithFieldPath(PVD, PFP);
      NLLForAllVars[PVD].push_back(
          NonLexicalLifetimeRange(1,
                                  FoundBorrowFields.count(BorrowWithFieldPath)
                                      ? FoundBorrowFields[BorrowWithFieldPath]
                                      : 1,
                                  PBK, Param.second, PFP));
      FoundBorrowFields.erase(BorrowWithFieldPath);
    } else {
      if (PQT->isPointerType() && !PQT.isOwnedQualified()) {
        // Handle borrow or naked pointer parameter which have virtual
        // ParentVar. Update previous NLL whose target is PVD to virtual
        // ParentVar.
        llvm::SmallVector<BorrowTargetInfo> VirtualTarget;
        VirtualTarget.push_back(Param.second);
        UpdateNLLWhenTargetFound(BorrowTargetInfo(PVD), VirtualTarget);
        // Add NLL for virtual ParentVar.
        NLLForAllVars[Param.second.TargetVD].push_back(
            NonLexicalLifetimeRange(0, NumElements + 3));
        // Add NLL for param with target.
        if (PQT.isBorrowQualified()) { // PVD is borrow pointer
          BorrowKind BK =
              PQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
          NLLForAllVars[PVD].push_back(NonLexicalLifetimeRange(
              1, FoundVars.count(PVD) ? FoundVars[PVD] : 1, BK, Param.second));
        } else // PVD is naked pointer
          NLLForAllVars[PVD].push_back(
              NonLexicalLifetimeRange(1, NumElements + 2));
      } else if (PQT.isOwnedQualified()) {
        // Add NLL for owned param.
        NLLForAllVars[PVD].push_back(
            NonLexicalLifetimeRange(1, FoundVars[PVD]));
      } else
        // Add NLL for other param.
        NLLForAllVars[PVD].push_back(
            NonLexicalLifetimeRange(1, NumElements + 2));
      FoundVars.erase(PVD);
    }
  }
  // Handle global variable
  // There are only global variables left in FoundVars at this time.
  for (const auto &globalVar : FoundVars)
    NLLForAllVars[globalVar.first].push_back(
        NonLexicalLifetimeRange(0, NumElements + 3));
}

// If the CFG is like this:
//    BB4(Entry)
//       |
//      BB3
//     /   \ 
//   BB2   BB1
//     \   /
//     BB0(Exit)
// We will compute all paths from Entry block to Exit block:
//     [4, 3, 2, 0], [4, 3, 1, 0]
// DFS is used.
static void VisitBlockForPath(CFGPathVec &CFGPaths, const CFGBlock *CurrentBB,
                              const CFGBlock *ExitBB, CFGPath Visited) {
  if (CurrentBB == ExitBB) {
    Visited.push_back(CurrentBB);
    CFGPaths.push_back(Visited); // Get a path from Entry to Exit.
    return;
  }
  for (const CFGBlock *NextBB : CurrentBB->succs()) {
    if (!NextBB ||
        (std::find(Visited.begin(), Visited.end(), CurrentBB) !=
             Visited.end() &&
         std::find(Visited.begin(), Visited.end(), NextBB) != Visited.end()))
      continue; // This path we have visited, continue and visit other NextBB.

    CFGPath TmpVisited = Visited;
    TmpVisited.push_back(CurrentBB);
    VisitBlockForPath(CFGPaths, NextBB, ExitBB, TmpVisited);
  }
}

CFGPathVec BorrowCheckImpl::FindPathOfCFG(const CFG &cfg) {
  const CFGBlock *EntryBB = &cfg.getEntry();
  const CFGBlock *ExitBB = &cfg.getExit();
  CFGPathVec CFGPaths;
  CFGPath Visited;
  VisitBlockForPath(CFGPaths, EntryBB, ExitBB, Visited);
  return CFGPaths;
}

// Compute non-lexical Lifetime for all variables in a certain path.
NonLexicalLifetime BorrowCheckImpl::CalculateNLLForPath(
    const CFGPath &Path,
    const std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
                   BorrowTargetInfo> &ParamTarget,
    unsigned NumElements) {
  NLLCalculator Calculator(Ctx, const_cast<DeclContext *>(fd.getDeclContext()),
                           NumElements);

  // To find when a borrow or owned variable is used for the last time,
  // we should traverse the path in reverse order.
  for (auto revBlockIt = Path.rbegin(); revBlockIt != Path.rend();
       revBlockIt++) {
    const CFGBlock *block = *revBlockIt;
    // we should also traverse the block in reverse order.
    for (CFGBlock::const_reverse_iterator revElemIt = block->rbegin(),
                                          revElemEI = block->rend();
         revElemIt != revElemEI; ++revElemIt) {
      const CFGElement &elem = *revElemIt;

      if (elem.getAs<CFGStmt>()) {
        const Stmt *S = elem.castAs<CFGStmt>().getStmt();
        Calculator.Visit(const_cast<Stmt *>(S));
      }
      // Local Variable in scope
      if (elem.getAs<CFGScopeBegin>()) {
        const VarDecl *VD = elem.castAs<CFGScopeBegin>().getVarDecl();
        Calculator.VisitScopeBegin(const_cast<VarDecl *>(VD));
      }

      if (elem.getAs<CFGScopeEnd>()) {
        const VarDecl *VD = elem.castAs<CFGScopeEnd>().getVarDecl();
        Calculator.VisitScopeEnd(const_cast<VarDecl *>(VD));
      }
      Calculator.CurElemID--;
    }
  }

  // After traversing all CFGElement in path, we have got NLL for all local
  // variables, here we add NLL for global variables and parameters.
  Calculator.HandleNLLAfterTraversing(ParamTarget);

  return Calculator.NLLForAllVars;
}

// For borrow or naked pointer parameter, we create virtual ParentVar as target.
// Other kind of parameter don't have target.
void BorrowCheckImpl::BuildParamTarget(
    std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>,
             BorrowTargetInfo> &ParamTarget) {
  for (ParmVarDecl *PVD : fd.parameters()) {
    QualType PQT = PVD->getType().getCanonicalType();
    if (PQT->isPointerType() && !PQT.isOwnedQualified()) {
      std::string Name = "_ParentVar_" + PVD->getNameAsString();
      IdentifierInfo *ID = &Ctx.Idents.get(Name);
      VarDecl *VD = VarDecl::Create(
          Ctx, const_cast<DeclContext *>(fd.getDeclContext()),
          PVD->getBeginLoc(), PVD->getLocation(), ID,
          QualType(PVD->getType()->getPointeeType()), nullptr, SC_None);
      if (PQT.isBorrowQualified()) {
        BorrowKind BK =
            PQT.isConstBorrow() ? BorrowKind::Immut : BorrowKind::Mut;
        ParamTarget[std::tuple<ParmVarDecl *, std::string, BorrowKind>(
            PVD, "", BK)] = BorrowTargetInfo(VD);
      } else
        ParamTarget[std::tuple<ParmVarDecl *, std::string, BorrowKind>(
            PVD, "", BorrowKind::NoBorrow)] = BorrowTargetInfo(VD);
      continue;
    } else if (PQT->hasBorrowFields()) {
      if (auto RT = dyn_cast<RecordType>(PQT)) {
        RecordDecl *RD = RT->getDecl();
        llvm::SmallVector<std::pair<std::string, BorrowKind>>
            BorrowFieldsOfStruct;
        FindBorrowFieldsOfStruct(RD, BorrowFieldsOfStruct);
        for (const auto &FieldPath : BorrowFieldsOfStruct) {
          std::tuple<ParmVarDecl *, std::string, BorrowKind>
              BorrowWithFieldPath;
          std::get<0>(BorrowWithFieldPath) = PVD;
          std::get<1>(BorrowWithFieldPath) = FieldPath.first;
          std::get<2>(BorrowWithFieldPath) = FieldPath.second;
          std::string FP = FieldPath.first;
          FP.erase(std::remove(FP.begin(), FP.end(), '.'), FP.end());
          std::string Name = "_ParentVar_" + PVD->getNameAsString() + FP;
          IdentifierInfo *ID = &Ctx.Idents.get(Name);
          VarDecl *VD = VarDecl::Create(
              Ctx, const_cast<DeclContext *>(fd.getDeclContext()),
              PVD->getBeginLoc(), PVD->getLocation(), ID, QualType(), nullptr,
              SC_None);
          ParamTarget[BorrowWithFieldPath] = BorrowTargetInfo(VD);
        }
      }
    }
    ParamTarget[std::tuple<ParmVarDecl *, std::string, BorrowKind>(
        PVD, "", BorrowKind::NoBorrow)] = BorrowTargetInfo();
  }
}

void BorrowCheckImpl::BorrowCheckForPath(
    const CFGPath &Path, BorrowCheckDiagReporter &reporter,
    const NonLexicalLifetime &NLLForAllVars, unsigned NumElements) {
  BorrowRuleChecker BRC(reporter, NLLForAllVars);
  BRC.BuildBorrowTargetMap();
  BRC.CheckBorrowNLLShorterThanTarget();
  if (fd.getReturnType().hasBorrow())
    BRC.CheckLifetimeOfBorrowReturnValue(NumElements);
  BRC.CheckBeBorrowedTarget(Path);
}

void clang::runBorrowCheck(const FunctionDecl &fd, const CFG &cfg,
                           BorrowCheckDiagReporter &reporter, ASTContext &Ctx) {
  // The analysis currently has scalability issues for very large CFGs.
  // Bail out if it looks too large.
  if (cfg.getNumBlockIDs() > 300000)
    return;

  BorrowCheckImpl BC(Ctx, fd);

  std::map<std::tuple<ParmVarDecl *, std::string, BorrowKind>, BorrowTargetInfo>
      ParamTarget;
  BC.BuildParamTarget(ParamTarget);

  // First we find all paths from entry block to exit block of a cfg.
  CFGPathVec CFGAllPaths = BC.FindPathOfCFG(cfg);

  // Calculation of NLL and borrow check will be executed for each path.
  for (CFGPath Path : CFGAllPaths) {
    // Count the number of all CFGElements in this path.
    unsigned NumElements = 0;
    for (const auto &block : Path)
      NumElements += block->size();
    // Calculate NLL for all variables in current path.
    NonLexicalLifetime NLLForAllVars =
        BC.CalculateNLLForPath(Path, ParamTarget, NumElements);
    // Check all borrow rules in current path.
    BC.BorrowCheckForPath(Path, reporter, NLLForAllVars, NumElements);
  }
}

#endif // ENABLE_BSC