//===- BSCOwnership.cpp - Ownership Analysis for Source CFGs -*- BSC --*------//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BSC Ownership analysis for source-level CFGs.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSCOwnership.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/FlowSensitive/DataflowWorklist.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include <algorithm>

using namespace clang;

namespace {
class OwnershipImpl {
public:
  AnalysisDeclContext &analysisContext;
  llvm::DenseMap<const CFGBlock *, Ownership::OwnershipStatus> blocksBeginStatus;
  llvm::DenseMap<const CFGBlock *, Ownership::OwnershipStatus> blocksEndStatus;

  Ownership::OwnershipStatus
  merge(Ownership::OwnershipStatus statsA,
        Ownership::OwnershipStatus statsB);

  Ownership::OwnershipStatus
  runOnBlock(const CFGBlock *block, Ownership::OwnershipStatus status,
             OwnershipDiagReporter &reporter);

  void dumpBlockStatus(const SourceManager& M);

  OwnershipImpl(AnalysisDeclContext &ac)
      : analysisContext(ac),
        blocksBeginStatus(0),
        blocksEndStatus(0) {}
};
} // namespace


//===----------------------------------------------------------------------===//
// Operations and queries on OwnershipStatus.
//===----------------------------------------------------------------------===//

bool Ownership::OwnershipStatus::empty() const {
  return status.empty() && pointer.empty() && structFields.empty();
}

// we don't need to worry we can't find VD
llvm::BitVector Ownership::OwnershipStatus::getStatus(const VarDecl *VD) const {
  return status.lookup(VD);
}

Ownership::OwnershipStatus
OwnershipImpl::merge(Ownership::OwnershipStatus statsA,
                     Ownership::OwnershipStatus statsB) {
  if (statsA.empty())
    return Ownership::OwnershipStatus(statsB.status, statsB.pointer,
                                      statsB.structFields);

  for (auto it = statsB.status.begin(), ei = statsB.status.end();
        it != ei; ++it) {
    const VarDecl *VD = it->first;
    const llvm::BitVector BV = it->second;
    if (statsA.status.count(VD)) {
      statsA.status[VD] |= BV;
    } else {
      statsA.status[VD] = BV;
    }
  }
  for (auto it = statsB.pointer.begin(), ei = statsB.pointer.end();
        it != ei; ++it) {
    const VarDecl *VD = it->first;
    unsigned value = it->second;
    if (statsA.pointer.count(VD)) {
      statsA.pointer[VD] = std::max(statsA.pointer[VD], value);
    } else {
      statsA.pointer[VD] = value;
    }
  }
  for (auto it = statsB.structFields.begin(), ei = statsB.structFields.end();
       it != ei; ++it) {
    const VarDecl *VD = it->first;
    llvm::SmallVector<llvm::SmallSet<std::string, 8>, 2> value = it->second;
    if (statsA.structFields.count(VD) && statsA.structFields[VD].size() == 2) {
      if (value.size() == 2) {
        for (auto s : value[1])
          statsA.structFields[VD][1].insert(s);
      }
    } else {
      statsA.structFields[VD] = value;
    }
  }
  return Ownership::OwnershipStatus(statsA.status, statsA.pointer,
                                    statsA.structFields);
}

bool Ownership::OwnershipStatus::equals(const OwnershipStatus &V) const {
  return status == V.status && pointer == V.pointer &&
         structFields == V.structFields;
}

static unsigned getIndex(Ownership::Status S) {
  int value = static_cast<int>(S);
  unsigned bitIndex = 0;
  while((value & 1) == 0) {
    value = value >> 1;
    bitIndex++;
  }
  return bitIndex;
}

bool Ownership::OwnershipStatus::has(const VarDecl *VD, Status S) const {
  auto it = status.find(VD);
  if (it != status.end()) {
    llvm::BitVector value = it->second;
    unsigned bitIndex = getIndex(S);

    if (value.test(bitIndex)) {
      value.reset(bitIndex);
      return value.any();
    }
  }
  return false;
}

bool Ownership::OwnershipStatus::is(const VarDecl *VD, Status S) const {
  auto it = status.find(VD);
  if (it != status.end()) {
    llvm::BitVector value = it->second;
    unsigned bitIndex = getIndex(S);

    if (value.test(bitIndex)) {
      value.reset(bitIndex);
      return !value.any();
    }
  }
  return false;
}

void Ownership::OwnershipStatus::set(const VarDecl *VD, Status S) {
  llvm::BitVector &value = status[VD];
  unsigned bitIndex = getIndex(S);
  value.set(bitIndex);
}

void Ownership::OwnershipStatus::reset(const VarDecl *VD, Status S) {
  llvm::BitVector &value = status[VD];
  unsigned bitIndex = getIndex(S);
  value.reset(bitIndex);
}

bool Ownership::OwnershipStatus::contains(const VarDecl *VD) const {
  if (status.find(VD) != status.end()) return true;
  return false;
}

void Ownership::OwnershipStatus::initOwnedFields(RecordDecl *RD,
                                                 const VarDecl *VD,
                                                 bool hasInit,
                                                 std::string fieldName) {
  for (RecordDecl::field_iterator fieldIt = RD->field_begin();
       fieldIt != RD->field_end(); ++fieldIt) {
    QualType FT = fieldIt->getType().getCanonicalType();
    std::string name = fieldName + fieldIt->getNameAsString();
    if (FT.isOwnedQualified()) {
      if (structFields[VD].empty()) {
        llvm::SmallSet<std::string, 8> s1 = {};
        llvm::SmallSet<std::string, 8> s2 = {};
        s1.insert(name);
        structFields[VD].emplace_back(s1);
        structFields[VD].emplace_back(s2);
      } else
        structFields[VD][0].insert(name);

      if (hasInit)
        structFields[VD][1].insert(name);
    }
    if (auto RT = dyn_cast<RecordType>(FT))
      initOwnedFields(RT->getDecl(), VD, hasInit, name + ".");
  }
  status[VD] = llvm::BitVector(6, 0);
  set(VD, Ownership::Status::Uninitialized);
  if (hasInit) {
    reset(VD, Ownership::Status::Uninitialized);
    set(VD, Ownership::Status::Owned);
  }
}

void Ownership::OwnershipStatus::addOwnedField(const VarDecl *VD,
                                               std::string name) {
  assert(structFields[VD].size() == 2 && "structFiendls size error.");
  llvm::SmallSet<std::string, 8> f1 = structFields[VD][0];
  if (f1.count(name)) {
    structFields[VD][1].insert(name);
  } else {
    for (auto field : f1) {
      if (strncmp(name.c_str(), field.c_str(), strlen(name.c_str())) == 0) {
        structFields[VD][1].insert(field);
      }
    }
  }
  reset(VD, Ownership::Status::AllMoved);
  reset(VD, Ownership::Status::Uninitialized);
  set(VD, Ownership::Status::Owned);
  if (f1.size() == structFields[VD][1].size())
    reset(VD, Ownership::Status::PartialMoved);
}

void Ownership::OwnershipStatus::moveOwnedField(const VarDecl *VD,
                                                std::string name) {
  assert(structFields[VD].size() == 2 && "structFiendls size error.");
  if (structFields[VD][1].count(name))
    structFields[VD][1].erase(name);
  else {
    llvm::SmallSet<std::string, 8> SF = structFields[VD][1];
    for (auto field : SF) {
      if (strncmp(name.c_str(), field.c_str(), strlen(name.c_str())) == 0)
        structFields[VD][1].erase(field);
    }
  }
  if (structFields[VD][1].empty()) {
    reset(VD, Ownership::Status::Owned);
    set(VD, Ownership::Status::AllMoved);
  } else
    set(VD, Ownership::Status::PartialMoved);
}

void Ownership::OwnershipStatus::fieldsAllMoved(const VarDecl *VD) {
  if (structFields.count(VD)) {
    assert(structFields[VD].size() == 2 && "structFiendls size error.");
    structFields[VD][1].clear();
    reset(VD, Ownership::Status::Owned);
    set(VD, Ownership::Status::AllMoved);
  }
}

/*
needComplete is used to determine whether the name needs to be allMoved.
In func1, a.a3.b1 is moved, a.a3 is equivalent to partialMoved and cannot be
used as a whole anymore. In func2, a.a1.b1 is moved, a.a1 is also equivalent to
partialMoved, but cannot be reassigned.

void func1(struct A a) {
  int *p = (int *)a.a3.b1
  struct B b = a.a3;
}

void func2(struct A a, struct B b) {
  int *p = (int *)(a.a1.b1);
  a.a1 = b;
}
*/
bool Ownership::OwnershipStatus::hasMovedField(const VarDecl *VD,
                                               std::string name,
                                               bool needComplete) {
  llvm::SmallSet<std::string, 8> f1 = structFields[VD][0];
  llvm::SmallSet<std::string, 8> f2 = structFields[VD][1];
  if (f2.count(name))
    return false;
  if (f1.count(name) && !f2.count(name))
    return true;
  if (needComplete) {
    for (auto field : f1) {
      if (strncmp(name.c_str(), field.c_str(), strlen(name.c_str())) == 0) {
        if (f2.count(field))
          return false;
      }
    }
    return true;
  } else {
    for (auto field : f1) {
      if (!f2.count(field)) {
        if (strncmp(name.c_str(), field.c_str(), strlen(name.c_str())) == 0)
          return true;
      }
    }
    return false;
  }
}

//===----------------------------------------------------------------------===//

static bool IsTrackFields(QualType type) {
  if (!type->isPointerType() && type->hasOwnedFields())
    return true;
  return false;
}

static std::string WarningLog(VarDecl *VD, Ownership::OwnershipStatus &stat) {
  std::string warningName = "";
  if (VD->getType().isOwnedQualified()) {
    warningName = VD->getNameAsString();
  } else if (IsTrackFields(VD->getType())) {
    // error handling
    if (stat.structFields.count(VD)) {
      warningName = "";
      Ownership::OwnershipStatus::OwnedFields ownedFields =
          stat.structFields[VD];
      for (auto fieldName : ownedFields[1]) {
        if (warningName.size() != 0)
          warningName = warningName + ", ";
        warningName = warningName + VD->getNameAsString() + "." + fieldName;
      }
    }
  }
  return warningName;
}

//===----------------------------------------------------------------------===//
// Dataflow computation.
//===----------------------------------------------------------------------===//

namespace {
class TransferFunctions : public StmtVisitor<TransferFunctions> {
  OwnershipImpl &OS;
  Ownership::OwnershipStatus &stat;
  const CFGBlock *currentBlock;
  OwnershipDiagReporter &reporter;
  int OwnedDepth = 0;
  bool OwnedFlag = false;

  enum Operation{
    None,
    Assign,
    Use
  };
  mutable Operation op = Operation::None;
public:
  TransferFunctions(OwnershipImpl &os,
                    Ownership::OwnershipStatus &Stat,
                    const CFGBlock *CurrentBlock,
                    OwnershipDiagReporter &reporter)
  : OS(os), stat(Stat), currentBlock(CurrentBlock),
    reporter(reporter) {}

  void VisitBinaryOperator(BinaryOperator *BO);
  void VisitCallExpr(CallExpr *CE);
  void VisitCStyleCastExpr(CStyleCastExpr *CSCE);
  void VisitDeclStmt(DeclStmt *DS);
  void VisitReturnStmt(ReturnStmt *RS);
  void VisitScopeEnd(VarDecl *VD, SourceLocation SL);
  void VisitStmt(Stmt *S);

  void VisitMemberExpr(MemberExpr *ME);
  void VisitDeclRefExpr(DeclRefExpr *DRE);
  void VisitUnaryOperator(UnaryOperator *UO);

  void HandleDREAssign(const DeclRefExpr *DRE, unsigned OwnedDepth = 0);
  void HandleDREUse(const DeclRefExpr *DRE, std::string name = "",
                    unsigned OwnedDepth = 0);
  void HandleMEAssign(const MemberExpr *ME, unsigned OwnedDepth = 0);
  void HandleMEUse(const MemberExpr *ME, unsigned OwnedDepth = 0);
};
} // namespace

void TransferFunctions::VisitStmt(Stmt *S) {
  for (auto *C : S->children()) {
    if (C) {
      Visit(C);
    }
  }
}

void TransferFunctions::VisitMemberExpr(MemberExpr *ME) {
  if (op == Assign)
    HandleMEAssign(ME);
  else if (op == Use)
    HandleMEUse(ME);
}


void TransferFunctions::VisitDeclRefExpr(DeclRefExpr *DRE) {
  if (op == Assign)
    HandleDREAssign(DRE, OwnedDepth);
  else if (op == Use)
    HandleDREUse(DRE, "", OwnedDepth);
}

void TransferFunctions::VisitUnaryOperator(UnaryOperator *UO) {
  const Expr *Sub = UO->getSubExpr();
  OwnedFlag = UO->getType().isOwnedQualified();
  if (OwnedDepth == 0) {
    OwnedDepth = 1;
    while(true) {
      if (const auto *CE = dyn_cast<CastExpr>(Sub)) {
        Sub = CE->getSubExpr();
      } else if (const auto *U = dyn_cast<UnaryOperator>(Sub)) {
        OwnedFlag &= U->getType().isOwnedQualified();
        OwnedDepth++;
        Sub = U->getSubExpr();
      } else {
        break;
      }
    }
  }
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Sub)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      if (stat.is(VD, Ownership::Status::Uninitialized)) {
        DiagInfo DI(VD->getNameAsString(), InvalidUseUninit, DRE->getLocation());
        reporter.addDiagInfo(DI);
      }
    }
  }
  if (OwnedFlag)
    Visit(UO->getSubExpr());
  OwnedDepth = 0;
}

void TransferFunctions::VisitBinaryOperator(BinaryOperator *BO) {
  op = Assign;
  if (BO->getOpcode() == BO_Assign)
    Visit(BO->getLHS());
  Expr *RHS = BO->getRHS();
  bool IsCall = dyn_cast_or_null<CallExpr>(RHS);
  if (!IsCall) {
    op = Use;
    Visit(RHS);
  }
  op = None;
}

void TransferFunctions::VisitCallExpr(CallExpr *CE) {
  op = Use;
  for (auto it = CE->arg_begin(), ei = CE->arg_end(); it != ei; ++it) {
    Visit(*it);
  }
  op = None;
}

void TransferFunctions::VisitCStyleCastExpr(CStyleCastExpr *CSCE) {
  // @code
  // (void * owned)p
  // @endcode
  if (CSCE->getType()->isVoidPointerType() && CSCE->getType().isOwnedQualified() && op != None) {
    if (const ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(CSCE->getSubExpr())) {
      if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(ICE->getSubExpr())) {
        const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl());
        if (VD->getType().isOwnedQualified() && VD->getType()->isPointerType()) {
          if (!VD->getType()->getPointeeType().isOwnedQualified()) {
            // legal cast, just do nothing
          } else if (stat.pointer.count(VD)) {
            if (stat.pointer[VD] != 1) {
              DiagInfo DI(VD->getNameAsString(), PointerCast, DRE->getLocation());
              reporter.addDiagInfo(DI);
            }
            stat.pointer.erase(VD);
          } else {
            DiagInfo DI(VD->getNameAsString(), PointerCast, DRE->getLocation());
            reporter.addDiagInfo(DI);
          }
        }
      }
    }
  }
  Visit(CSCE->getSubExpr());
}

void TransferFunctions::VisitDeclStmt(DeclStmt *DS) {
  for (auto *D : DS->decls()) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      Expr *Init = VD->getInit();
      if (VD->getType().isOwnedQualified() || IsTrackFields(VD->getType())) {
        if (!stat.contains(VD)) {
          stat.status[VD] = llvm::BitVector(6, 0);
          stat.set(VD, Ownership::Status::Uninitialized);
          if (IsTrackFields(VD->getType())) {
            if (auto RT =
                    dyn_cast<RecordType>(VD->getType().getCanonicalType())) {
              stat.initOwnedFields(RT->getDecl(), VD, false);
            }
          }
        }
        if (Init) {
          stat.reset(VD, Ownership::Status::Uninitialized);
          stat.set(VD, Ownership::Status::Owned);
          if (IsTrackFields(VD->getType())) {
            if (auto RT =
                    dyn_cast<RecordType>(VD->getType().getCanonicalType())) {
              stat.initOwnedFields(RT->getDecl(), VD, true);
            }
          }
        }
      }
      if (Init) {
        bool IsCall = dyn_cast_or_null<CallExpr>(Init);
        if (!IsCall) {
          op = Use;
          Visit(Init);
          op = None;
        }
      }
    }
  }
}

void TransferFunctions::VisitReturnStmt(ReturnStmt *RS) {
  Expr *RV = RS->getRetValue();
  bool IsCall = dyn_cast_or_null<CallExpr>(RV);
  if (RV && !IsCall) {
    op = Use;
    Visit(RV);
    op = None;
  }
}

void TransferFunctions::VisitScopeEnd(VarDecl *VD, SourceLocation SL) {
  if (stat.is(VD, Ownership::Status::Owned) ||
      stat.has(VD, Ownership::Status::Owned)) {
    // error handling
    DiagInfo DI(WarningLog(VD, stat), MemoryLeak, SL);
    reporter.addDiagInfo(DI);
    // it's important in for/while
    stat.reset(VD, Ownership::Status::Owned);
  }
}

void TransferFunctions::HandleDREAssign(const DeclRefExpr *DRE, unsigned OwnedDepth) {
  if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    if (VD->getType().isOwnedQualified()) {
      if (VD->getType()->isPointerType() && OwnedDepth) {
        // @code
        // *p = ...;
        // @endcode
        if (OwnedDepth == stat.pointer[VD]) {
          stat.pointer.erase(VD);
        } else {
          DiagInfo DI(VD->getNameAsString(), PointerInnerAssign, DRE->getLocation());
          reporter.addDiagInfo(DI);
        }
      } else {
        stat.reset(VD, Ownership::Status::Uninitialized);
        stat.reset(VD, Ownership::Status::Moved);
        if (stat.is(VD, Ownership::Status::Owned)) {
          DiagInfo DI(VD->getNameAsString(), Reassign, DRE->getLocation());
          reporter.addDiagInfo(DI);
        }
        stat.set(VD, Ownership::Status::Owned);
      }
    }
    // @code
    // void func(struct A a) {
    //   struct A p;
    //   p = a;
    // }
    // @endcode
    else if (IsTrackFields(VD->getType())) {
      if (auto RT = dyn_cast<RecordType>(VD->getType().getCanonicalType()))
        stat.initOwnedFields(RT->getDecl(), VD, true);
    }
  }
}

void TransferFunctions::HandleMEAssign(const MemberExpr *ME,
                                       unsigned OwnedDepth) {
  Expr *base = ME->getBase();
  std::string name = ME->getMemberNameInfo().getAsString();
  while (const MemberExpr *tmp = dyn_cast<MemberExpr>(base)) {
    name = tmp->getMemberNameInfo().getAsString() + "." + name;
    base = tmp->getBase();
  }
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(base)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      if (ME->getType().isOwnedQualified() || IsTrackFields(VD->getType())) {
        assert(stat.structFields[VD].size() == 2 &&
               "structFiendls size error.");
        if (!stat.hasMovedField(VD, name, true)) {
          DiagInfo DI(VD->getNameAsString() + "." + name, Reassign, DRE->getLocation());
          reporter.addDiagInfo(DI);
        }
        else
          stat.addOwnedField(VD, name);
      }
    }
  }
}

void TransferFunctions::HandleDREUse(const DeclRefExpr *DRE, std::string name,
                                     unsigned OwnedDepth) {
  if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
    if (VD->getType().isOwnedQualified() && !IsTrackFields(VD->getType())) {
      if (name == "*") {
        if (stat.is(VD, Ownership::Status::Uninitialized) ||
            stat.is(VD, Ownership::Status::Moved)) {
          DiagInfo DI(VD->getNameAsString(), InvalidUse, DRE->getLocation());
          reporter.addDiagInfo(DI);
        }
      } else if (stat.is(VD, Ownership::Status::Owned)) {
        if (VD->getType()->isPointerType()) {
          if (stat.pointer.count(VD)) {
            DiagInfo DI(VD->getNameAsString(), PointerInnerMoved, DRE->getLocation());
            reporter.addDiagInfo(DI);
          }
          if (OwnedDepth) {
            stat.pointer[VD] = std::max(stat.pointer[VD], OwnedDepth);
          } else {
            stat.reset(VD, Ownership::Status::Owned);
            stat.set(VD, Ownership::Status::Moved);
            stat.location[VD] = DRE->getLocation();
          }
        } else {
          stat.reset(VD, Ownership::Status::Owned);
          stat.set(VD, Ownership::Status::Moved);
          stat.location[VD] = DRE->getLocation();
        }
      } else if (stat.has(VD, Ownership::Status::Owned)) {
        // VD maybe owned
        DiagInfo DI(VD->getNameAsString(), InvalidUse, DRE->getLocation());
        reporter.addDiagInfo(DI);
        stat.reset(VD, Ownership::Status::Owned);
      } else {
        // VD don't have owned
        if (stat.is(VD, Ownership::Status::Uninitialized)) {
          DiagInfo DI(VD->getNameAsString(), InvalidUseUninit, DRE->getLocation());
          reporter.addDiagInfo(DI);
        } else if (stat.is(VD, Ownership::Status::Moved)) {
          DiagInfo DI(VD->getNameAsString(), InvalidUseMoved, DRE->getLocation(), stat.location[VD]);
          reporter.addDiagInfo(DI);
        } else {
          DiagInfo DI(VD->getNameAsString(), InvalidUse, DRE->getLocation());
          reporter.addDiagInfo(DI);
        }
      }
    } else if (IsTrackFields(VD->getType())) {
      if (stat.is(VD, Ownership::Status::Owned)) {
        if (name.size())
          stat.moveOwnedField(VD, name);
        else
          stat.fieldsAllMoved(VD);
      } else if (stat.has(VD, Ownership::Status::Owned)) {
        if (name.size()) {
          if (!stat.hasMovedField(VD, name, false))
            stat.moveOwnedField(VD, name);
          else {
            DiagInfo DI(VD->getNameAsString() + "." + name, InvalidUse, DRE->getLocation());
            reporter.addDiagInfo(DI);
          }
        } else if (name != "*") {
          DiagInfo DI(VD->getNameAsString(), InvalidUse, DRE->getLocation());
          reporter.addDiagInfo(DI);
          stat.fieldsAllMoved(VD);
        }
      } else {
        DiagInfo DI(VD->getNameAsString(), InvalidUse, DRE->getLocation());
        reporter.addDiagInfo(DI);
      }
    }
  }
}

void TransferFunctions::HandleMEUse(const MemberExpr *ME, unsigned OwnedDepth) {
  Expr *base = ME->getBase();
  std::string name = ME->getMemberNameInfo().getAsString();

  bool isPointer = false;
  while (base) {
    if (const MemberExpr *tmp = dyn_cast<MemberExpr>(base)) {
      if (!isPointer)
        name = tmp->getMemberNameInfo().getAsString() + "." + name;
      base = tmp->getBase();
    } else if (ImplicitCastExpr *tmp = dyn_cast<ImplicitCastExpr>(base)) {
      base = tmp->getSubExpr();
      isPointer = true;
      name = "*";
    } else {
      break;
    }
  }

  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(base))
    HandleDREUse(DRE, name);
}

Ownership::OwnershipStatus
OwnershipImpl::runOnBlock(const CFGBlock *block,
                          Ownership::OwnershipStatus status,
                          OwnershipDiagReporter &reporter) {
  TransferFunctions TF(*this, status, block, reporter);

  for (CFGBlock::const_iterator it = block->begin(),
                                ei = block->end(); it != ei; ++it) {
    const CFGElement &elem = *it;

    if (elem.getAs<CFGStmt>()) {
      const Stmt *S = elem.castAs<CFGStmt>().getStmt();
      TF.Visit(const_cast<Stmt*>(S));
    }

    if (elem.getAs<CFGScopeEnd>()) {
      const Stmt *S = elem.castAs<CFGScopeEnd>().getTriggerStmt();
      const VarDecl *VD = elem.castAs<CFGScopeEnd>().getVarDecl();
      TF.VisitScopeEnd(const_cast<VarDecl*>(VD), S->getEndLoc());
    }
  }

  return status;
}

void clang::runOwnershipAnalysis(const FunctionDecl &fd, const CFG &cfg,
                                 AnalysisDeclContext &ac,
                                 OwnershipDiagReporter &reporter) {

  // The analysis currently has scalability issues for very large CFGs.
  // Bail out if it looks too large.
  if (cfg.getNumBlockIDs() > 300000)
    return;

  OwnershipImpl *OS = new OwnershipImpl(ac);

  // Proceed with the worklist.
  ForwardDataflowWorklist worklist(cfg, ac);

  // Mark all owned parameter Owned at the entry
  const CFGBlock *entry = &cfg.getEntry();
  for (ParmVarDecl *PVD : fd.parameters()) {
    if (PVD->getType().isOwnedQualified()) {
      const VarDecl *VD = dyn_cast<VarDecl>(PVD);
      OS->blocksEndStatus[entry].status[VD] = llvm::BitVector(6, 0);
      OS->blocksEndStatus[entry].set(VD, Ownership::Status::Owned);
    }
    if (auto RT = dyn_cast<RecordType>(PVD->getType().getCanonicalType())) {
      if (IsTrackFields(QualType(RT, 0))) {
        OS->blocksEndStatus[entry].initOwnedFields(RT->getDecl(), PVD, true);
      }
    }
  }

  for (const CFGBlock *B : cfg.const_reverse_nodes())
    if (B != entry && !B->pred_empty())
      worklist.enqueueBlock(B);

  while (const CFGBlock *block = worklist.dequeue()) {
    Ownership::OwnershipStatus &preVal = OS->blocksBeginStatus[block];

    // meet operator
    Ownership::OwnershipStatus val;
    for (CFGBlock::const_pred_iterator it = block->pred_begin(),
                                       ei = block->pred_end(); it != ei; ++it) {
      if (const CFGBlock *pred = *it) {
        val = OS->merge(val, OS->blocksEndStatus[pred]);
      }
    }

    OS->blocksEndStatus[block] = OS->runOnBlock(block, val, reporter);

    if (preVal.equals(val))
      continue;

    preVal = val;

    // Enqueue the value to the successors.
    worklist.enqueueSuccessors(block);
  }

  // check ownership rules of function parameters
  const CFGBlock *exit = &cfg.getExit();
  for (ParmVarDecl *PVD : fd.parameters()) {
    if (OS->blocksEndStatus[exit].is(PVD, Ownership::Status::Owned) ||
        OS->blocksEndStatus[exit].has(PVD, Ownership::Status::Owned)) {
      DiagInfo DI(WarningLog(PVD, OS->blocksEndStatus[exit]), MemoryLeak, fd.getSourceRange().getEnd());
      reporter.addDiagInfo(DI);
    }
  }
}

#endif // ENABLE_BSC