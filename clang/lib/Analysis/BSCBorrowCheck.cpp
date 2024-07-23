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
#include "clang/Analysis/FlowSensitive/DataflowWorklist.h"
#include <algorithm>
#include <vector>
#include <set>
#include <map>
using namespace clang;

using CFGPath = std::vector<const CFGBlock*>;
using CFGPathVec = std::vector<std::vector<const CFGBlock*>>;

class BorrowCheckImpl {
public:
  ASTContext& Ctx;
  const FunctionDecl& fd;

  NonLexicalLifetime CalculateNLLForPath(const CFGPath &Path,
                                         const llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget,
                                         unsigned NumElements);
  void BorrowCheckForPath(const CFGPath &Path, BorrowCheckDiagReporter &reporter, 
                          const NonLexicalLifetime& NLLForAllVars, unsigned NumElements);
  void BuildParamTarget(llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget);
  
  BorrowCheckImpl(ASTContext& ASTCtx, const FunctionDecl& FD)
      : Ctx(ASTCtx), fd(FD) {}
};

// If the CFG is like this:
//    BB4(Entry)
//       |
//      BB3
//     /   \ 
//   BB2   BB1
//     \   /
//     BB0(Exit)
// The Successors of all BBs in the CFG is: 
//     { 4:[3], 3:[2, 1], 2:[0], 1:[0] }
// Then we will compute all paths from Entry block to Exit block:
//     [4, 3, 2, 0], [4, 3, 1, 0]
// TODO: This class should be modified,
//       because it cannot handle the cfg with loops.
class CFGPathFinder {
  CFGPathVec CFGPaths;
  const CFGBlock* EntryBB;
  const CFGBlock* ExitBB;
public:
  CFGPathVec FindPathOfCFG(const CFG &cfg) {
    EntryBB = &cfg.getEntry();
    ExitBB = &cfg.getExit();
    // Then we use DFS to find all paths from Entry block to Exit block .
    FindPath(EntryBB, std::vector<const CFGBlock*>{ EntryBB });
    return CFGPaths;
  }

private:
  void FindPath(const CFGBlock* current, CFGPath path) {
    if (current == ExitBB)
      CFGPaths.push_back(path);
    
    for(const CFGBlock *next : current->succs()) {
      if (!next)
        continue;
      CFGPath newpath1 = path;
      if(find(path.begin(), path.end(), next) == path.end()) {
        newpath1.push_back(next);
        CFGPath newpath2 = newpath1;
        FindPath(next, newpath2);
      }
    }
  }
};

class NLLCalculator : public StmtVisitor<NLLCalculator> {
  ASTContext& Ctx;
  DeclContext* DC;
  // Record VarDecls when we traverse path in reverse order. 
  // For borrow and owned variables, 
  // we should record the location they are used for the last time in path.
  // For other local variables, 
  // we should record the location they leave current scope.
  llvm::DenseMap<VarDecl*, unsigned> FoundVars;
  unsigned NumElements;
public:
  // Record current CFGElement index we are visiting in the path.
  unsigned CurElemID;
  // Record NLL for all variables in a cfg path.
  NonLexicalLifetime NLLForAllVars;
  
  NLLCalculator(ASTContext& ASTCtx, DeclContext* DeclCtx, unsigned NumElems)
      : Ctx(ASTCtx), DC(DeclCtx), NumElements(NumElems), CurElemID(NumElems + 1) {}

  void VisitDeclStmt(DeclStmt *DS);
  void VisitBinaryOperator(BinaryOperator *BO);
  void VisitDeclRefExpr(DeclRefExpr *DRE);
  void VisitReturnStmt(ReturnStmt *RS);
  void VisitCallExpr(CallExpr *CE);
  void VisitUnaryOperator(UnaryOperator *UO);
  void VisitScopeEnd(VarDecl *VD);
  void VisitScopeBegin(VarDecl *VD);
  void HandleNLLAfterTraversing(const llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget);

private:
  void MarkBorrowLastUseLoc(Expr* E);
  void VisitInitForTargets(Expr *InitE, llvm::SmallVector<VarDecl*>& TargetSet);
  void UpdateNLLWhenTargetFound(VarDecl* OldTarget, const llvm::SmallVector<VarDecl*>& NewTargets);
};

class BorrowRuleChecker : public StmtVisitor<BorrowRuleChecker> {
  BorrowCheckDiagReporter &reporter;

public:
  NonLexicalLifetime NLLForAllVars;
  BorrowRuleChecker(BorrowCheckDiagReporter &reporter, const NonLexicalLifetime& NLLForVars)
      : reporter(reporter), NLLForAllVars(NLLForVars) {}

  // Record the correspondence of borrowed and borrow variables.
  // The key of map is borrowed variable(i.e. targets), the value is variables borrow from its key variable.
  // For example, if we have such borrow correspondence as follows:
  //    `p1` borrows from `local1` from index 5 to index 10 in the path.
  //    `p2` borrows from `local2` from index 3 to index 4 in the path.
  //    `p3` borrows from `local1` and 'local2' from index 6 to index 8 in the path.
  // Then BorrowTargetMap will be:
  //   local1 : [ (p1, 5, 10), (p3, 6, 8) ], local2 : [ (p2, 3, 4), (p3, 6, 8) ]
  llvm::DenseMap<VarDecl*, llvm::SmallVector<std::tuple<VarDecl*, unsigned, unsigned>>> BorrowTargetMap;
  void BuildBorrowTargetMap();
  void CheckBorrowNLLShorterThanTarget();
  void CheckLifetimeOfBorrowReturnValue(unsigned NumElements);
  // void VisitDeclStmt(DeclStmt *DS);
  // void VisitBinaryOperator(BinaryOperator *BO);
  // void VisitDeclRefExpr(DeclRefExpr *DRE);
};

void BorrowRuleChecker::BuildBorrowTargetMap() {
  for (auto NLLOfVar : NLLForAllVars) {
    VarDecl* VD = NLLOfVar.first;
    NonLexicalLifetimeOfVar NLLRangesOfVar = NLLOfVar.second;
    for (auto NLLRange : NLLRangesOfVar) {
      if (NLLRange.Target)
        BorrowTargetMap[NLLRange.Target].push_back(std::tuple<VarDecl*, unsigned, unsigned>(VD, NLLRange.Range.first, NLLRange.Range.second));
    }
  }
  // DEBUG PRINT
  for (auto v : BorrowTargetMap) {
    llvm::outs() << v.first->getNameAsString() << ": [ ";
    for (auto w : v.second) {
      VarDecl* V = std::get<0>(w);
      llvm::outs() << "(" << V->getNameAsString() << ", " << std::get<1>(w) << ", " << std::get<2>(w) << "), ";
    }
    llvm::outs() << "]\n";
  }
}

// Core rule of borrow:
//   the lifetime of borrow variable is shorter than its target variable.
void BorrowRuleChecker::CheckBorrowNLLShorterThanTarget() {
  for (auto BorrowAndTarget : BorrowTargetMap) {
    VarDecl* TargetVD = BorrowAndTarget.first;
    unsigned TargetVDBegin = NLLForAllVars[TargetVD][0].Range.first;
    unsigned TargetVDEnd = NLLForAllVars[TargetVD][0].Range.second;
    for (auto Borrow : BorrowAndTarget.second) {
      VarDecl* BorrowVD = std::get<0>(Borrow);
      QualType BorrowQT = BorrowVD->getType();
      if (BorrowQT.isBorrowQualified()) { // Naked pointer also have target, but we don't need to consider it.
        unsigned BorrowVDBegin = std::get<1>(Borrow);
        unsigned BorrowVDEnd = std::get<2>(Borrow);
        if (BorrowVDBegin <= TargetVDBegin || BorrowVDEnd >= TargetVDEnd) {
          BorrowCheckDiagInfo DI(BorrowVD->getNameAsString(), LiveLonger, BorrowVD->getLocation());
          reporter.addDiagInfo(DI);
        }
      }
    }
  }
}

// Function cannot return a borrow of local variables.
// For example:
//     int global = 5; 
//     const int* borrow test(const int* borrow p) {
//         int local = 5;
//         const int* borrow q = &mut local;
//         return &const global;    // right
//         // return p;             // right
//         // return &const local;  // error 
//         // return q;             // error
// }
void BorrowRuleChecker::CheckLifetimeOfBorrowReturnValue(unsigned NumElements) {
  for (auto NLLOfVar : NLLForAllVars) {
    if (NLLOfVar.first->getNameAsString() == "_ReturnVar") {
      for (NonLexicalLifetimeRange& Range : NLLOfVar.second) {
        NonLexicalLifetimeRange TargetNLLRange = NLLForAllVars[Range.Target][0];
        unsigned TargetNLLBegin = TargetNLLRange.Range.first;
        unsigned TargetNLLEnd = TargetNLLRange.Range.second;
        if (TargetNLLBegin > 0 || TargetNLLEnd < NumElements + 3) {
          BorrowCheckDiagInfo DI(Range.Target->getNameAsString(), ReturnLocal, NLLOfVar.first->getLocation());
          reporter.addDiagInfo(DI);
        }
      }
    }
  }
}

void NLLCalculator::VisitBinaryOperator(BinaryOperator *BO) {
  // A borrow variable is reassigned, we would handle it like DeclStmt.
  // For example:
  //    int* borrow p = &mut local1;
  //    p = &mut local2;    // We handle it like `int* borrow p = &mut local2;`
  if (BO->isAssignmentOp()) {
    if (DeclRefExpr * DRE = dyn_cast<DeclRefExpr>(BO->getLHS())) {
      if (VarDecl* VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        if (VD->getType().isBorrowQualified()) {
          Expr *Init = BO->getRHS();
          llvm::SmallVector<VarDecl*> Targets;
          VisitInitForTargets(Init, Targets);
          // When we find the target of a borrow,
          // we should update previous target in NLLForAllVars.
          UpdateNLLWhenTargetFound(VD, Targets);
          for (VarDecl* Target : Targets)
            NLLForAllVars[VD].push_back(
              NonLexicalLifetimeRange(CurElemID, FoundVars.count(VD) ? FoundVars[VD] : CurElemID, Target));
          FoundVars.erase(VD);
        }
      }
    }
  }
}

void NLLCalculator::VisitDeclRefExpr(DeclRefExpr *DRE) {
  if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    // Add global variables to record which globals are used.
    // Also add owned variables(local or parameter) to record the last location used.
    if (((!VD->isLocalVarDecl() && !isa<ParmVarDecl>(VD)) || VD->getType().isOwnedQualified())
         && !FoundVars.count(VD))
      FoundVars[VD] = CurElemID;
  }
}

void NLLCalculator::VisitReturnStmt(ReturnStmt *RS) {
  Expr *RV = RS->getRetValue();
  if (!RV) 
    return;
  // For return stmt `return p;` or `return &mut* p`,
  // we think borrow pointer `p` is used. 
  MarkBorrowLastUseLoc(RV);
  QualType ReturnQT = RV->getType();
  if (ReturnQT.isBorrowQualified()) {
    // In order to check lifetime of borrow return value, we build a virtual ReturnVar.
    std::string Name = "_ReturnVar";
    IdentifierInfo *ID = &Ctx.Idents.get(Name);
    VarDecl *ReturnVD = VarDecl::Create(Ctx, DC, RV->getBeginLoc(), RV->getBeginLoc(),
                                        ID, ReturnQT, nullptr, SC_None);
    // ReturnVar itself only survive in current ReturnStmt,
    // but we still should get its target to check if this function returns a borrow of local variable.   
    llvm::SmallVector<VarDecl*> Targets;
    VisitInitForTargets(RV, Targets);
    for (VarDecl* Target : Targets)
      NLLForAllVars[ReturnVD].push_back(NonLexicalLifetimeRange(CurElemID, CurElemID, Target));
  }
}

void NLLCalculator::VisitCallExpr(CallExpr *CE) {
  // For function call `use(p)` or `use(&mut* p)`,
  // we think borrow pointer `p` is used here. 
  for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it)
    MarkBorrowLastUseLoc(*it);
}

void NLLCalculator::VisitUnaryOperator(UnaryOperator *UO) {
  // For pointer deref `*p`,
  // we think borrow pointer `p` is used here. 
  if (UO->getOpcode() == UO_Deref)
    MarkBorrowLastUseLoc(UO->getSubExpr());
}

void NLLCalculator::VisitDeclStmt(DeclStmt *DS) {
  for (auto *D : DS->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      if (VD->getType().isBorrowQualified()) {
        Expr *Init = VD->getInit();
        llvm::SmallVector<VarDecl*> Targets;
        VisitInitForTargets(Init, Targets);
        // When we find the target of a borrow,
        // we should update previous target in NLLForAllVars.
        UpdateNLLWhenTargetFound(VD, Targets);  
        for (VarDecl* Target : Targets)
          NLLForAllVars[VD].push_back(
            NonLexicalLifetimeRange(CurElemID, FoundVars.count(VD) ? FoundVars[VD] : CurElemID, Target));
        FoundVars.erase(VD);
      }
    }
  }
}

void NLLCalculator::MarkBorrowLastUseLoc(Expr* E) {
  DeclRefExpr* DRE = nullptr;
  if (auto ICE = dyn_cast<ImplicitCastExpr>(E)) {
    DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr());
  } else if (auto UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_AddrMutDeref || UO->getOpcode() == UO_AddrConstDeref)
      DRE = dyn_cast<DeclRefExpr>(UO->getSubExpr());
  }

  if (DRE)    
    if (VarDecl* VD = dyn_cast<VarDecl>(DRE->getDecl()))
      if (VD->getType().isBorrowQualified() && !FoundVars.count(VD))
        FoundVars[VD] = CurElemID;
}


void NLLCalculator::VisitInitForTargets(Expr *InitE, llvm::SmallVector<VarDecl*>& Targets) {
  if (auto UO = dyn_cast<UnaryOperator>(InitE)) {
    if (UO->getOpcode() == UO_AddrMut || UO->getOpcode() == UO_AddrConst
        || UO->getOpcode() == UO_AddrMutDeref || UO->getOpcode() == UO_AddrConstDeref) {
      if (auto* DRE = dyn_cast<DeclRefExpr>(UO->getSubExpr())) {
        // int* borrow p = &mut local;  local is DeclRefExpr.
        VarDecl* IVD = dyn_cast<VarDecl>(DRE->getDecl());
        Targets.push_back(IVD);
      } else if (auto ASE = dyn_cast<ArraySubscriptExpr>(UO->getSubExpr())) {
        // int* borrow p = &mut arr[0];  arr[0] is ArraySubscriptExpr.
        VisitInitForTargets(ASE->getBase(), Targets);
      } else if (auto ICE = dyn_cast<ImplicitCastExpr>(UO->getSubExpr())) {
        // int* borrow p = &mut *q;  q is ImplicitCastExpr.
        VisitInitForTargets(ICE, Targets);
      }
    }
  } else if (auto ICE = dyn_cast<ImplicitCastExpr>(InitE)) {
    // ImplicitCastExpr: int* borrow p2 = p1; 
    // p2 refers to p1, if p1 is borrow variable, we add p1 to Targets;
    if (auto* DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr())) {
      VarDecl* IVD = dyn_cast<VarDecl>(DRE->getDecl());
      Targets.push_back(IVD);
    }
  } else if (auto CE = dyn_cast<CallExpr>(InitE)) {
    // CallExpr: int* borrow p3 = foo(&mut local1, &mut local2); 
    // the lifetime of p3 should be smaller than the lifetime intersection of local1 and local2.
    // we add local1 and local2 to Targets.
    for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it) {
      if (Expr* ArgExpr = dyn_cast<Expr>(*it))
        VisitInitForTargets(ArgExpr, Targets);
    }
  }
}

void NLLCalculator::VisitScopeBegin(VarDecl *VD) {
  QualType QT = VD->getType();
  if (QT->isPointerType() && !QT.isOwnedQualified() && !QT.isBorrowQualified()) {
    // For local naked pointer, we create virtual ParentVar as target.
    std::string Name = "_ParentVar_" + VD->getNameAsString();
    IdentifierInfo *ID = &Ctx.Idents.get(Name);
    VarDecl *VirtualVD = VarDecl::Create(Ctx, DC,
                                         VD->getBeginLoc(), VD->getLocation(), ID,
                                         QualType(), nullptr, SC_None);
    // Update previous NLL whose target is this naked pointer VD to virtual ParentVar 
    llvm::SmallVector<VarDecl*> VirtualTarget;
    VirtualTarget.push_back(VirtualVD);
    UpdateNLLWhenTargetFound(VD, VirtualTarget);
    // Add NLL for virtual ParentVar.
    NLLForAllVars[VirtualVD].push_back(NonLexicalLifetimeRange(0, NumElements + 3));
  }
  if (!QT.isBorrowQualified()) {
    // For no-borrow local variable, its NLL is continous, and ScopeBegin marks the begin of its NLL.
    NLLForAllVars[VD].push_back(NonLexicalLifetimeRange(CurElemID, FoundVars[VD]));
    FoundVars.erase(VD);
  }
}

void NLLCalculator::VisitScopeEnd(VarDecl *VD) {
  // For no-borrow and no-owned local variable, ScopeEnd marks the end of its NLL. 
  QualType QT = VD->getType();
  if (!QT.isBorrowQualified() && !QT.isOwnedQualified())
    FoundVars[VD] = CurElemID;
}

// When we find the target of a borrow, 
// we should update previous target in NLLForAllVars.
// For such example:
//     int * borrow p1 = use_and_return(&mut local1, &mut local2); // 1   
//                                                                 //  p3:[3, 3]->local1, [3, 3]->local2 
//                                                                 //  p2:[2, 2]->local1, [2, 2]->local2
//                                                                 //  p1:[1, 1]->local1, [1, 1]->local2 
//     int * borrow p2 = p1;                                       // 2   
//                                                                 //  p3:[3, 3]->p1
//                                                                 //  p2:[2, 2]->p1
//     int * borrow p3 = p2;                                       // 3  
//                                                                 //  p3:[3, 3]->p2
// Because we traverse path in reverse order, we handle in order 3->2->1.          
// When we handle 3, we know p3 targeting to p2,
// but we don't know p2 targeting to which.
// When we handle 2, we know p2 targeting to p1,
// we should update the target of p3 from p2 to p1.
// When we handle 1, we know p1 targeting to local1 and local2,
// we should update the target of p2 and p3 from p1 to local1 and local2.
void NLLCalculator::UpdateNLLWhenTargetFound(VarDecl* OldTarget, const llvm::SmallVector<VarDecl*>& NewTargets) {
  for (auto& NLLOfVar : NLLForAllVars) {
    NonLexicalLifetimeOfVar& NLLRanges = NLLOfVar.second;
    llvm::SmallVector<std::pair<unsigned, unsigned>> RangesNeedUpdate;
    // Remove ranges whose target is OldTarget
    for (auto it = NLLRanges.begin(); it != NLLRanges.end();) {
      if (it->Target == OldTarget) {
        RangesNeedUpdate.push_back(it->Range);
        it = NLLRanges.erase(it);
      } else
        ++it; 
    }
    // Build new ranges targeting to NewTarget and insert them to NLLRanges.
    for (std::pair<unsigned, unsigned> RangeNeedUpdate : RangesNeedUpdate) {
      for (VarDecl* NewTarget : NewTargets) {
        NonLexicalLifetimeRange NewRange(RangeNeedUpdate, NewTarget);
        if (std::find(NLLRanges.begin(), NLLRanges.end(), NewRange) == NLLRanges.end())
          NLLRanges.push_back(NewRange);
      }
    }
  }
}

// In order to calculate NLL for global and parameter,
// we use virtual CFGElement index to mark the begin and end of global variable and parameter
//   NLL begin of global/virtual ParentVar    :0
//   NLL begin of parameter                   :1
//   local variables                          :from 2 to NumElements + 1
//   NLL end of borrow or owned parameter     :last use location
//   NLL end of no-borrow-no-owned parameter  :NumElements + 2
//   NLL end of global/virtual ParentVar      :NumElements + 3
void NLLCalculator::HandleNLLAfterTraversing(const llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget) {
  // Handle parameter
  for (auto Param : ParamTarget) {
    ParmVarDecl* PVD = Param.first;
    QualType PQT = PVD->getType();
    // Handle borrow or naked pointer parameter which have virtual ParentVar.
    if (PQT->isPointerType() && !PQT.isOwnedQualified()) {
      // Update previous NLL whose target is PVD to virtual ParentVar 
      llvm::SmallVector<VarDecl*> VirtualTarget;
      VirtualTarget.push_back(Param.second);
      UpdateNLLWhenTargetFound(PVD, VirtualTarget);
      // Add NLL for virtual ParentVar.
      NLLForAllVars[Param.second].push_back(NonLexicalLifetimeRange(0, NumElements + 3));
      // Add NLL for param with target.
      if (PQT.isBorrowQualified())  // PVD is borrow pointer
        NLLForAllVars[PVD].push_back(
          NonLexicalLifetimeRange(1, FoundVars.count(PVD) ? FoundVars[PVD] : 1, Param.second));
      else                          // PVD is naked pointer
        NLLForAllVars[PVD].push_back(
          NonLexicalLifetimeRange(1, NumElements + 2, Param.second));
    } else if (PQT.isOwnedQualified()) {
      // Add NLL for owned param.
      NLLForAllVars[PVD].push_back(NonLexicalLifetimeRange(1, FoundVars[PVD]));
    } else
      // Add NLL for other param.
      NLLForAllVars[PVD].push_back(NonLexicalLifetimeRange(1, NumElements + 2));
    FoundVars.erase(PVD);
  }
  // Handle global variable
  // There are only global variables left in FoundVars at this time.
  for (auto globalVar : FoundVars)
    NLLForAllVars[globalVar.first].push_back(NonLexicalLifetimeRange(0, NumElements + 3));
}

// Compute non-lexical Lifetime for all variables in a certain path.
NonLexicalLifetime BorrowCheckImpl::CalculateNLLForPath(const CFGPath &Path, const llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget,
                                                        unsigned NumElements) {
  NLLCalculator Calculator(Ctx, const_cast<DeclContext*>(fd.getDeclContext()), NumElements);

  // To find when a borrow or owned variable is used for the last time, 
  // we should traverse the path in reverse order.
  for (auto revBlockIt = Path.rbegin(); revBlockIt != Path.rend(); revBlockIt++) {
    const CFGBlock* block = *revBlockIt;
    llvm::outs() << "Current block id: " << block->getBlockID() << "    ";
    // we should also traverse the block in reverse order.
    for (CFGBlock::const_reverse_iterator revElemIt = block->rbegin(),
                                          revElemEI = block->rend();
         revElemIt != revElemEI; ++revElemIt) {
      const CFGElement &elem = *revElemIt;

      if (elem.getAs<CFGStmt>()) {
        const Stmt *S = elem.castAs<CFGStmt>().getStmt();
        Calculator.Visit(const_cast<Stmt*>(S));
      }
      // Local Variable in scope
      if (elem.getAs<CFGScopeBegin>()) {
        const VarDecl *VD = elem.castAs<CFGScopeBegin>().getVarDecl();
        Calculator.VisitScopeBegin(const_cast<VarDecl*>(VD));
      }

      if (elem.getAs<CFGScopeEnd>()) {
        const VarDecl *VD = elem.castAs<CFGScopeEnd>().getVarDecl();
        Calculator.VisitScopeEnd(const_cast<VarDecl*>(VD));
      }
      llvm::outs() << Calculator.CurElemID << " ";
      Calculator.CurElemID--;
    }
    llvm::outs() << "\n";
  }

  // After traversing all CFGElement in path, we have got NLL for all local variables,
  // here we add NLL for global variables and parameters. 
  Calculator.HandleNLLAfterTraversing(ParamTarget);

  // DEBUG PRINT
  llvm::outs() << "---------print NLL-----------\n";
  for (auto u : Calculator.NLLForAllVars) {
    llvm::outs() << u.first->getNameAsString() << "\n";   // VarDecl
    for (auto v : u.second) {   // NLL
      llvm::outs() << "  " << v.Range.first << "->" << v.Range.second;
      if (v.Target)
        llvm::outs() << " " << v.Target->getNameAsString() << "\n";
      else
        llvm::outs() << "\n";
    }
  }
  llvm::outs() << "\n";

  return Calculator.NLLForAllVars;
}

// For borrow or naked pointer parameter, we create virtual ParentVar as target.
// Other kind of parameter don't have target. 
void BorrowCheckImpl::BuildParamTarget(llvm::DenseMap<ParmVarDecl*, VarDecl*>& ParamTarget) {
  for (ParmVarDecl *PVD : fd.parameters()) {
    if (PVD->getType()->isPointerType() && !PVD->getType().isOwnedQualified()) {
      std::string Name = "_ParentVar_" + PVD->getNameAsString();
      IdentifierInfo *ID = &Ctx.Idents.get(Name);
      VarDecl *VD = VarDecl::Create(Ctx, const_cast<DeclContext*>(fd.getDeclContext()), 
                                    PVD->getBeginLoc(), PVD->getLocation(), ID,
                                    QualType(), nullptr, SC_None);
      ParamTarget[PVD] = VD;
    } else
      ParamTarget[PVD] = nullptr;
  }
}

void BorrowCheckImpl::BorrowCheckForPath(const CFGPath &Path, BorrowCheckDiagReporter &reporter,
                                         const NonLexicalLifetime& NLLForAllVars, unsigned NumElements) {
  BorrowRuleChecker BRC(reporter, NLLForAllVars);
  BRC.BuildBorrowTargetMap();
  BRC.CheckBorrowNLLShorterThanTarget();
  if (fd.getReturnType().isBorrowQualified())
    BRC.CheckLifetimeOfBorrowReturnValue(NumElements);
}

void clang::runBorrowCheck(const FunctionDecl &fd, const CFG &cfg,
                           BorrowCheckDiagReporter &reporter, ASTContext& Ctx) {
  // The analysis currently has scalability issues for very large CFGs.
  // Bail out if it looks too large.
  if (cfg.getNumBlockIDs() > 300000)
    return;

  BorrowCheckImpl BC(Ctx, fd);

  llvm::DenseMap<ParmVarDecl*, VarDecl*> ParamTarget;
  BC.BuildParamTarget(ParamTarget);
  
  // First we find all paths from entry block to exit block of a cfg.
  CFGPathFinder PathFinder;
  CFGPathVec CFGAllPaths = PathFinder.FindPathOfCFG(cfg);

  // DEBUG PRINT
  llvm::outs() << "----------print PathSet----------" << "\n";
  for (CFGPath Path : CFGAllPaths) {
    for (const CFGBlock* bb : Path) {
      llvm::outs() << bb->getBlockID() << "->";
    }
    llvm::outs() << "\n";
  }
  for (const CFGBlock *BB : cfg.const_reverse_nodes())
    BB->dump();
  
  // Calculation of NLL and borrow check will be executed for each path.
  for (CFGPath Path : CFGAllPaths) {
    // Count the number of all CFGElements in this path. 
    unsigned NumElements = 0;
    for (auto block: Path)
      NumElements += block->size();
    // Calculate NLL for all variables in current path.
    NonLexicalLifetime NLLForAllVars = BC.CalculateNLLForPath(Path, ParamTarget, NumElements);
    // Check all borrow rules in current path.
    BC.BorrowCheckForPath(Path, reporter, NLLForAllVars, NumElements);
  }
}

#endif // ENABLE_BSC