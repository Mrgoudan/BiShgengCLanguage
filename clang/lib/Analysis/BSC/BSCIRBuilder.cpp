//===- BSCIRBuilder.cpp - AST to BSCIR lowering ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Recursive AST lowering into BSCIR. Creates blocks in encounter order and
// threads control flow directly from AST structure, then runs SimplifyCfg.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIRBuilder.h"
#include "clang/AST/Attr.h"
#include "clang/AST/BSC/ExprBSC.h"
#include "clang/AST/BSC/StmtBSC.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::bscir;

//===----------------------------------------------------------------------===//
// Constructor
//===----------------------------------------------------------------------===//

BSCIRBuilder::BSCIRBuilder(ASTContext &Ctx, const FunctionDecl &FD)
    : Ctx(Ctx), FD(FD) {
  TheBody = std::make_unique<Body>();
  TheBody->SourceFD = &FD;
  TheBody->FuncSafeZone = FD.getSafeZoneSpecifier();
}

//===----------------------------------------------------------------------===//
// Safe Zone Computation
//===----------------------------------------------------------------------===//

SafeZoneSpecifier BSCIRBuilder::currentSafeZone() const {
  if (!SafeZoneStack.empty())
    return SafeZoneStack.back();
  return TheBody->FuncSafeZone;
}

//===----------------------------------------------------------------------===//
// Block Management
//===----------------------------------------------------------------------===//

BasicBlockId BSCIRBuilder::createBlock() { return TheBody->addBlock(); }

BasicBlockId BSCIRBuilder::switchToBlock(BasicBlockId NewBlock) {
  BasicBlockId Old = CurrentBlock;
  CurrentBlock = NewBlock;
  return Old;
}

void BSCIRBuilder::emit(Statement S) {
  TheBody->getBlock(CurrentBlock).Statements.push_back(std::move(S));
}

void BSCIRBuilder::setTerminator(Terminator T) {
  TheBody->getBlock(CurrentBlock).Term = std::move(T);
}

//===----------------------------------------------------------------------===//
// Local Variable Management
//===----------------------------------------------------------------------===//

LocalId BSCIRBuilder::getOrCreateLocal(const VarDecl *VD) {
  auto It = VarMap.find(VD);
  if (It != VarMap.end())
    return It->second;

  LocalId Id = TheBody->addLocal(VD->getType(), VD, VD->getName(), false,
                                 VD->getLocation());
  VarMap[VD] = Id;
  return Id;
}

//===----------------------------------------------------------------------===//
// Lowering Helpers
//===----------------------------------------------------------------------===//

bool BSCIRBuilder::shouldMove(const Expr *E) const {
  QualType Ty = E->getType();
  if (Ty.isOwnedQualified())
    return true;
  if (Ty->isRecordType()) {
    if (Ty.getTypePtr()->isOwnedStructureType() ||
        Ty->isMoveSemanticType())
      return true;
  }
  return false;
}

Place BSCIRBuilder::lowerToPlace(const Expr *E) {
  E = E->IgnoreParenImpCasts();

  if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      LocalId Id = getOrCreateLocal(VD);
      return Place(Id, VD->getType(), DRE->getLocation());
    }
  }

  if (auto *ME = dyn_cast<MemberExpr>(E)) {
    if (auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      Place Base = lowerToPlace(ME->getBase());
      if (ME->isArrow()) {
        Base = Base.project(ProjectionElem::createDeref(
            ME->getBase()->getType()->getPointeeType()),
            TheBody->getAllocator());
      }
      return Base.project(
          ProjectionElem::createField(FD->getFieldIndex(), FD, FD->getType()),
          TheBody->getAllocator());
    }
  }

  if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref) {
      Place Base = lowerToPlace(UO->getSubExpr());
      return Base.project(
          ProjectionElem::createDeref(UO->getType()),
          TheBody->getAllocator());
    }
  }

  if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
    Place Base = lowerToPlace(ASE->getBase());
    Operand Idx = lowerToOperand(ASE->getIdx());
    if (Idx.K == Operand::Constant) {
      if (Idx.getConstVal().isInt()) {
        uint64_t CI = Idx.getConstVal().getInt().getZExtValue();
        return Base.project(
            ProjectionElem::createConstantIndex(CI, ASE->getType()),
            TheBody->getAllocator());
      }
    }
    LocalId IdxLocal = TheBody->addTemp(ASE->getIdx()->getType(),
                                        ASE->getIdx()->getExprLoc());
    Place IdxPlace(IdxLocal, ASE->getIdx()->getType(),
                   ASE->getIdx()->getExprLoc());
    emit(Statement::createAssign(IdxPlace, Rvalue::createUse(Idx),
                                 currentSafeZone()));
    return Base.project(
        ProjectionElem::createIndex(IdxLocal, ASE->getType()),
        TheBody->getAllocator());
  }

  // Fallback: create a temporary for complex lvalue expressions
  Operand Op = Visit(const_cast<Expr *>(E));
  LocalId Tmp = TheBody->addTemp(E->getType(), E->getExprLoc());
  Place TmpPlace(Tmp, E->getType(), E->getExprLoc());
  emit(Statement::createAssign(TmpPlace, Rvalue::createUse(Op),
                               currentSafeZone()));
  return TmpPlace;
}

Operand BSCIRBuilder::lowerToOperand(const Expr *E) {
  return Visit(const_cast<Expr *>(E));
}

//===----------------------------------------------------------------------===//
// Parameter Lowering
//===----------------------------------------------------------------------===//

void BSCIRBuilder::lowerParams() {
  // Local 0 = return slot
  QualType RetTy = FD.getReturnType();
  TheBody->addLocal(RetTy, nullptr, "_0", false, FD.getLocation());

  // Locals 1..N = parameters
  for (unsigned I = 0; I < FD.getNumParams(); ++I) {
    const ParmVarDecl *PD = FD.getParamDecl(I);
    LocalId Id = TheBody->addLocal(PD->getType(), PD, PD->getName(), false,
                                   PD->getLocation());
    VarMap[PD] = Id;
  }
  TheBody->NumParams = FD.getNumParams();
}

//===----------------------------------------------------------------------===//
// Label/Goto Support
//===----------------------------------------------------------------------===//

BasicBlockId BSCIRBuilder::getOrCreateLabelBlock(LabelDecl *LD) {
  auto It = LabelMap.find(LD);
  if (It != LabelMap.end())
    return It->second;
  BasicBlockId BB = createBlock();
  LabelMap[LD] = BB;
  return BB;
}

//===----------------------------------------------------------------------===//
// Scope Cleanup
//===----------------------------------------------------------------------===//

void BSCIRBuilder::emitScopeCleanup(unsigned TargetDepth) {
  // Emit StorageDead for all locals in scopes above TargetDepth, top-down
  for (unsigned I = ScopeStack.size(); I > TargetDepth; --I) {
    for (LocalId L : llvm::reverse(ScopeStack[I - 1].Locals))
      emit(Statement::createStorageDead(L, currentSafeZone()));
  }
}

//===----------------------------------------------------------------------===//
// Recursive Statement Lowering
//===----------------------------------------------------------------------===//

void BSCIRBuilder::lowerStmt(const Stmt *S) {
  if (!S)
    return;

  // SafeStmt: push safe zone, lower body, pop
  if (auto *SS = dyn_cast<SafeStmt>(S)) {
    SafeZoneStack.push_back(SS->getSafeZoneSpecifier());
    lowerStmt(SS->getSubStmt());
    SafeZoneStack.pop_back();
    return;
  }

  if (auto *CS = dyn_cast<CompoundStmt>(S)) {
    lowerCompoundStmt(CS);
    return;
  }

  if (auto *IS = dyn_cast<IfStmt>(S)) {
    lowerIfStmt(IS);
    return;
  }

  if (auto *WS = dyn_cast<WhileStmt>(S)) {
    lowerWhileStmt(WS);
    return;
  }

  if (auto *FS = dyn_cast<ForStmt>(S)) {
    lowerForStmt(FS);
    return;
  }

  if (auto *DS = dyn_cast<DoStmt>(S)) {
    lowerDoWhileStmt(DS);
    return;
  }

  if (auto *SS = dyn_cast<SwitchStmt>(S)) {
    lowerSwitchStmt(SS);
    return;
  }

  if (auto *BS = dyn_cast<BreakStmt>(S)) {
    lowerBreakStmt(BS);
    return;
  }

  if (auto *CS = dyn_cast<ContinueStmt>(S)) {
    lowerContinueStmt(CS);
    return;
  }

  if (auto *GS = dyn_cast<GotoStmt>(S)) {
    lowerGotoStmt(GS);
    return;
  }

  if (auto *LS = dyn_cast<LabelStmt>(S)) {
    lowerLabelStmt(LS);
    return;
  }

  if (auto *RS = dyn_cast<ReturnStmt>(S)) {
    lowerReturnStmt(RS);
    return;
  }

  if (auto *DS = dyn_cast<DeclStmt>(S)) {
    VisitDeclStmt(const_cast<DeclStmt *>(DS));
    return;
  }

  // NullStmt: bare semicolons — skip silently.
  if (isa<NullStmt>(S))
    return;

  // Expression statements
  if (auto *E = dyn_cast<Expr>(S)) {
    Visit(const_cast<Expr *>(E));
    return;
  }

  // Fallback: emit nop
  emit(Statement::createNop(currentSafeZone(), S->getBeginLoc()));
}

void BSCIRBuilder::lowerCompoundStmt(const CompoundStmt *CS) {
  // Push safe zone if this compound has one
  bool PushedSafeZone = false;
  SafeZoneSpecifier CompSZ = CS->getCompSafeZoneSpecifier();
  if (CompSZ != SZ_None) {
    SafeZoneStack.push_back(CompSZ);
    PushedSafeZone = true;
  }

  // Push scope for StorageDead tracking
  ScopeStack.push_back({});

  for (const Stmt *S : CS->body())
    lowerStmt(S);

  // Emit StorageDead for all locals declared in this scope
  for (LocalId L : llvm::reverse(ScopeStack.back().Locals))
    emit(Statement::createStorageDead(L, currentSafeZone()));
  ScopeStack.pop_back();

  if (PushedSafeZone)
    SafeZoneStack.pop_back();
}

void BSCIRBuilder::lowerIfStmt(const IfStmt *IS) {
  // For constexpr if (or any if with a compile-time constant condition),
  // only emit the taken branch to avoid false positives in init analysis.
  Expr::EvalResult ConstResult;
  if (IS->getCond()->EvaluateAsInt(ConstResult, Ctx)) {
    bool CondIsTrue = ConstResult.Val.getInt().getBoolValue();
    if (CondIsTrue) {
      lowerStmt(IS->getThen());
    } else if (IS->getElse()) {
      lowerStmt(IS->getElse());
    }
    return;
  }

  // Lower condition
  Operand Cond = lowerToOperand(IS->getCond());

  BasicBlockId ThenBB = createBlock();
  BasicBlockId ElseBB = createBlock();
  BasicBlockId JoinBB = createBlock();

  // SwitchInt: 1 → then, otherwise → else
  SmallVector<std::pair<llvm::APInt, BasicBlockId>, 4> Targets;
  Targets.push_back({llvm::APInt(1, 1), ThenBB});
  setTerminator(Terminator::createSwitchInt(std::move(Cond),
                                            std::move(Targets), ElseBB,
                                            currentSafeZone()));

  // Then branch
  switchToBlock(ThenBB);
  lowerStmt(IS->getThen());
  // If then didn't terminate (return/break/etc), goto join
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

  // Else branch
  switchToBlock(ElseBB);
  if (IS->getElse())
    lowerStmt(IS->getElse());
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

  // Continue from join
  switchToBlock(JoinBB);
}

void BSCIRBuilder::emitCondBranch(const Expr *Cond, BasicBlockId BodyBB,
                                      BasicBlockId ExitBB) {
  Expr::EvalResult Result;
  if (Cond->EvaluateAsInt(Result, Ctx)) {
    // Constant condition — emit direct goto, skip switchInt.
    BasicBlockId Target =
        Result.Val.getInt().getBoolValue() ? BodyBB : ExitBB;
    setTerminator(Terminator::createGoto(Target, currentSafeZone()));
  } else {
    Operand CondOp = lowerToOperand(Cond);
    SmallVector<std::pair<llvm::APInt, BasicBlockId>, 4> Targets;
    Targets.push_back({llvm::APInt(1, 1), BodyBB});
    setTerminator(Terminator::createSwitchInt(std::move(CondOp),
                                              std::move(Targets), ExitBB,
                                              currentSafeZone()));
  }
}

void BSCIRBuilder::lowerWhileStmt(const WhileStmt *WS) {
  BasicBlockId CondBB = createBlock();
  BasicBlockId BodyBB = createBlock();
  BasicBlockId ExitBB = createBlock();

  // Goto cond
  setTerminator(Terminator::createGoto(CondBB, currentSafeZone()));

  // Condition block
  switchToBlock(CondBB);
  emitCondBranch(WS->getCond(), BodyBB, ExitBB);

  // Body block
  // FIXME if cond always false no need to build body part. 
  // currently handled by simplify dead block removal. 
  switchToBlock(BodyBB);
  BreakableScopes.push_back(
      {ExitBB, CondBB, /*HasContinue=*/true,
       static_cast<unsigned>(ScopeStack.size())});
  lowerStmt(WS->getBody());
  BreakableScopes.pop_back();
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(CondBB, currentSafeZone()));

  // Exit
  switchToBlock(ExitBB);
}

void BSCIRBuilder::lowerForStmt(const ForStmt *FS) {
  bool PushedInitScope = false;
  if (isa_and_nonnull<DeclStmt>(FS->getInit())) {
    ScopeStack.push_back({});
    PushedInitScope = true;
  }

  // Lower init in current block
  if (FS->getInit())
    lowerStmt(FS->getInit());

  BasicBlockId CondBB = createBlock();
  BasicBlockId BodyBB = createBlock();
  BasicBlockId IncrBB = createBlock();
  BasicBlockId ExitBB = createBlock();

  // Goto cond
  setTerminator(Terminator::createGoto(CondBB, currentSafeZone()));

  // Condition block
  switchToBlock(CondBB);
  if (!FS->getCond()) {
    // No condition = always true (for(;;))
    setTerminator(Terminator::createGoto(BodyBB, currentSafeZone()));
  } else {
    emitCondBranch(FS->getCond(), BodyBB, ExitBB);
  }

  // Body block
  switchToBlock(BodyBB);
  BreakableScopes.push_back(
      {ExitBB, IncrBB, /*HasContinue=*/true,
       static_cast<unsigned>(ScopeStack.size())});
  lowerStmt(FS->getBody());
  BreakableScopes.pop_back();
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(IncrBB, currentSafeZone()));

  // Increment block
  switchToBlock(IncrBB);
  if (FS->getInc())
    Visit(const_cast<Expr *>(FS->getInc()));
  setTerminator(Terminator::createGoto(CondBB, currentSafeZone()));

  // Exit
  switchToBlock(ExitBB);

  // Emit StorageDead for init-declaration variables at loop exit
  if (PushedInitScope) {
    for (LocalId L : llvm::reverse(ScopeStack.back().Locals))
      emit(Statement::createStorageDead(L, currentSafeZone()));
    ScopeStack.pop_back();
  }
}

void BSCIRBuilder::lowerDoWhileStmt(const DoStmt *DS) {
  BasicBlockId BodyBB = createBlock();
  BasicBlockId CondBB = createBlock();
  BasicBlockId ExitBB = createBlock();

  // Goto body
  setTerminator(Terminator::createGoto(BodyBB, currentSafeZone()));

  // Body block
  switchToBlock(BodyBB);
  BreakableScopes.push_back(
      {ExitBB, CondBB, /*HasContinue=*/true,
       static_cast<unsigned>(ScopeStack.size())});
  lowerStmt(DS->getBody());
  BreakableScopes.pop_back();
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(CondBB, currentSafeZone()));

  // Condition block
  switchToBlock(CondBB);
  emitCondBranch(DS->getCond(), BodyBB, ExitBB);

  // Exit
  switchToBlock(ExitBB);
}

void BSCIRBuilder::lowerSwitchStmt(const SwitchStmt *SS) {
  // Lower discriminant
  Operand Discr = lowerToOperand(SS->getCond());

  BasicBlockId ExitBB = createBlock();

  // A case region groups one or more case/default labels with the body
  // statements that follow them until the next case/default label.
  struct CaseRegion {
    SmallVector<llvm::APInt, 2> CaseValues; // integer values for case labels
    bool IsDefault = false;                  // true if this region has a default
    SmallVector<const Stmt *, 4> Body;       // statements in this region
  };

  SmallVector<CaseRegion, 8> Regions;

  // Helper: unwrap nested CaseStmt/DefaultStmt labels and collect their values,
  // returning the innermost non-case/default sub-statement.
  std::function<const Stmt *(const Stmt *, CaseRegion &)> unwrapLabels =
      [&](const Stmt *S, CaseRegion &R) -> const Stmt * {
    if (auto *CS = dyn_cast<CaseStmt>(S)) {
      Expr::EvalResult Result;
      if (CS->getLHS()->EvaluateAsInt(Result, Ctx))
        R.CaseValues.push_back(Result.Val.getInt());
      return unwrapLabels(CS->getSubStmt(), R);
    }
    if (auto *DS = dyn_cast<DefaultStmt>(S)) {
      R.IsDefault = true;
      return unwrapLabels(DS->getSubStmt(), R);
    }
    return S; // not a label
  };

  // Walk the CompoundStmt body linearly to build case regions.
  const Stmt *Body = SS->getBody();
  // FIXME: handle brace-less switch body (e.g., "switch (x) case 1: foo();")
  if (auto *CS = dyn_cast<CompoundStmt>(Body)) {
    for (const Stmt *S : CS->body()) {
      if (isa<CaseStmt>(S) || isa<DefaultStmt>(S)) {
        // Start a new region
        Regions.push_back({});
        CaseRegion &R = Regions.back();
        const Stmt *Inner = unwrapLabels(S, R);
        // The innermost sub-statement is the first body statement
        if (Inner)
          R.Body.push_back(Inner);
      } else {
        // Non-label statement: add to current region's body
        if (!Regions.empty())
          Regions.back().Body.push_back(S);
        // else: statements before any case label — unreachable in valid C
      }
    }
  }

  // Create a block for each region + find default
  SmallVector<BasicBlockId, 8> RegionBlocks;
  BasicBlockId DefaultBB = ExitBB; // fallback if no default label
  SmallVector<std::pair<llvm::APInt, BasicBlockId>, 4> SwitchTargets;

  for (unsigned I = 0; I < Regions.size(); ++I) {
    BasicBlockId BB = createBlock();
    RegionBlocks.push_back(BB);

    for (auto &Val : Regions[I].CaseValues)
      SwitchTargets.push_back({Val, BB});

    if (Regions[I].IsDefault)
      DefaultBB = BB;
  }

  // Set switch terminator on the current block
  setTerminator(Terminator::createSwitchInt(std::move(Discr),
                                            std::move(SwitchTargets), DefaultBB,
                                            currentSafeZone()));

  // Push a scope for the switch body compound statement. Declarations in
  // case regions (without explicit braces) are scoped to the switch body;
  // StorageDead for these locals must appear at switch exit, not function exit.
  ScopeStack.push_back({});

  // Lower each region's body
  for (unsigned I = 0; I < Regions.size(); ++I) {
    switchToBlock(RegionBlocks[I]);
    BreakableScopes.push_back(
        {ExitBB, {0}, /*HasContinue=*/false,
         static_cast<unsigned>(ScopeStack.size())});

    for (const Stmt *S : Regions[I].Body)
      lowerStmt(S);

    BreakableScopes.pop_back();

    // If unterminated, fall through to next region (or exit for last)
    if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable) {
      BasicBlockId FallTarget =
          (I + 1 < Regions.size()) ? RegionBlocks[I + 1] : ExitBB;
      setTerminator(Terminator::createGoto(FallTarget, currentSafeZone()));
    }
  }

  // Exit: emit StorageDead for switch-body locals, then pop the scope.
  switchToBlock(ExitBB);
  for (LocalId L : llvm::reverse(ScopeStack.back().Locals))
    emit(Statement::createStorageDead(L, currentSafeZone()));
  ScopeStack.pop_back();
}

void BSCIRBuilder::lowerBreakStmt(const BreakStmt *BS) {
  if (BreakableScopes.empty())
    return;
  emitScopeCleanup(BreakableScopes.back().ScopeDepth);
  setTerminator(Terminator::createGoto(BreakableScopes.back().BreakTarget,
                                       currentSafeZone()));
  // Create fresh unreachable block for dead code after break
  BasicBlockId DeadBB = createBlock();
  switchToBlock(DeadBB);
}

void BSCIRBuilder::lowerContinueStmt(const ContinueStmt *CS) {
  // Walk up the breakable scope stack to find the nearest loop (HasContinue).
  // Switch statements push scopes with HasContinue=false, so we must skip them.
  for (int I = BreakableScopes.size() - 1; I >= 0; --I) {
    const auto &Scope = BreakableScopes[I];
    if (Scope.HasContinue) {
      emitScopeCleanup(Scope.ScopeDepth);
      setTerminator(Terminator::createGoto(Scope.ContinueTarget,
                                           currentSafeZone()));
      break;
    }
  }
  // Create fresh unreachable block for dead code after continue
  BasicBlockId DeadBB = createBlock();
  switchToBlock(DeadBB);
}

void BSCIRBuilder::lowerGotoStmt(const GotoStmt *GS) {
  BasicBlockId TargetBB = getOrCreateLabelBlock(GS->getLabel());
  // Emit StorageDead for locals between current scope and the label's scope.
  // prescanLabels() has already recorded every label's depth, so the lookup
  // always succeeds for valid programs.
  auto It = LabelScopeDepth.find(GS->getLabel());
  unsigned TargetDepth = (It != LabelScopeDepth.end())
                             ? It->second
                             : ScopeStack.size(); // no cleanup if unknown
  emitScopeCleanup(TargetDepth);
  setTerminator(Terminator::createGoto(TargetBB, currentSafeZone()));
  // Create fresh block for dead code after goto
  BasicBlockId DeadBB = createBlock();
  switchToBlock(DeadBB);
}

void BSCIRBuilder::lowerLabelStmt(const LabelStmt *LS) {
  BasicBlockId LabelBB = getOrCreateLabelBlock(LS->getDecl());
  // Record scope depth at the label for goto cleanup
  LabelScopeDepth[LS->getDecl()] = ScopeStack.size();
  // Terminate current block with goto to label block
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(LabelBB, currentSafeZone()));
  switchToBlock(LabelBB);
  // Lower the sub-statement
  lowerStmt(LS->getSubStmt());
}

void BSCIRBuilder::lowerReturnStmt(const ReturnStmt *RS) {
  // Lower return value and assign to _0
  VisitReturnStmt(const_cast<ReturnStmt *>(RS));
  // Emit StorageDead for all locals in all scopes
  emitScopeCleanup(0);
  // Goto return block
  setTerminator(Terminator::createGoto(ReturnBlock, currentSafeZone()));
  // Create fresh block for dead code after return
  BasicBlockId DeadBB = createBlock();
  switchToBlock(DeadBB);
}

//===----------------------------------------------------------------------===//
// StmtVisitor Methods
//===----------------------------------------------------------------------===//

Operand BSCIRBuilder::VisitDeclRefExpr(DeclRefExpr *E) {
  if (auto *VD = dyn_cast<VarDecl>(E->getDecl())) {
    LocalId Id = getOrCreateLocal(VD);
    Place P(Id, VD->getType(), E->getLocation());
    if (shouldMove(E))
      return Operand::createMove(P);
    return Operand::createCopy(P);
  }
  return Operand::createConstant(APValue(), E->getType());
}

Operand BSCIRBuilder::VisitIntegerLiteral(IntegerLiteral *E) {
  return Operand::createConstant(APValue(llvm::APSInt(E->getValue(),
                                                E->getType()->isUnsignedIntegerType())),
                                 E->getType());
}

Operand BSCIRBuilder::VisitFloatingLiteral(FloatingLiteral *E) {
  return Operand::createConstant(APValue(E->getValue()), E->getType());
}

Operand BSCIRBuilder::VisitCharacterLiteral(CharacterLiteral *E) {
  return Operand::createConstant(
      APValue(llvm::APSInt(llvm::APInt(32, E->getValue()), true)), E->getType());
}

Operand BSCIRBuilder::VisitStringLiteral(StringLiteral *E) {
  return Operand::createStringConstant(E->getString(), E->getType());
}

Operand BSCIRBuilder::VisitBinaryOperator(BinaryOperator *BO) {
  if (BO->isAssignmentOp()) {
    Place Dest = lowerToPlace(BO->getLHS());
    Operand Src = lowerToOperand(BO->getRHS());

    if (BO->isCompoundAssignmentOp()) {
      Operand DestOp = Operand::createCopy(Dest);
      BinaryOperatorKind CompOp =
          BinaryOperator::getOpForCompoundAssignment(BO->getOpcode());
      LocalId Tmp = TheBody->addTemp(BO->getType(), BO->getExprLoc());
      Place TmpPlace(Tmp, BO->getType(), BO->getExprLoc());
      emit(Statement::createAssign(
          TmpPlace, Rvalue::createBinaryOp(CompOp, DestOp, Src),
          currentSafeZone(), BO, BO->getExprLoc()));
      emit(Statement::createAssign(Dest, Rvalue::createUse(
                                              Operand::createCopy(TmpPlace)),
                                   currentSafeZone(), BO, BO->getExprLoc()));
      return Operand::createCopy(Dest);
    }

    emit(Statement::createAssign(Dest, Rvalue::createUse(Src),
                                 currentSafeZone(), BO, BO->getExprLoc()));
    return Operand::createCopy(Dest);
  }

  // Short-circuit logical operators: && and ||
  if (BO->getOpcode() == BO_LAnd || BO->getOpcode() == BO_LOr) {
    bool IsAnd = (BO->getOpcode() == BO_LAnd);

    // Eval LHS in current block
    Operand LHS = lowerToOperand(BO->getLHS());

    // Result temporary
    LocalId Result = TheBody->addTemp(BO->getType(), BO->getExprLoc());
    Place ResultPlace(Result, BO->getType(), BO->getExprLoc());

    BasicBlockId ShortBB = createBlock();  // short-circuit block
    BasicBlockId RhsBB = createBlock();    // evaluate RHS block
    BasicBlockId JoinBB = createBlock();   // join block

    // For &&: if LHS truthy → eval RHS, otherwise short-circuit to 0
    // For ||: if LHS truthy → short-circuit to 1, otherwise eval RHS
    SmallVector<std::pair<llvm::APInt, BasicBlockId>, 4> Targets;
    Targets.push_back({llvm::APInt(1, 1), IsAnd ? RhsBB : ShortBB});
    setTerminator(Terminator::createSwitchInt(std::move(LHS),
                                              std::move(Targets),
                                              IsAnd ? ShortBB : RhsBB,
                                              currentSafeZone()));

    // Short-circuit block: result = 0 (&&) or 1 (||)
    switchToBlock(ShortBB);
    int ShortVal = IsAnd ? 0 : 1;
    Operand ShortOp = Operand::createConstant(
        APValue(llvm::APSInt(llvm::APInt(32, ShortVal), false)), BO->getType());
    emit(Statement::createAssign(ResultPlace, Rvalue::createUse(ShortOp),
                                 currentSafeZone(), BO, BO->getExprLoc()));
    setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

    // RHS block: eval RHS, assign to result
    switchToBlock(RhsBB);
    Operand RHS = lowerToOperand(BO->getRHS());
    emit(Statement::createAssign(ResultPlace, Rvalue::createUse(RHS),
                                 currentSafeZone(), BO, BO->getExprLoc()));
    setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

    // Continue in join block
    switchToBlock(JoinBB);
    return Operand::createCopy(ResultPlace);
  }

  Operand LHS = lowerToOperand(BO->getLHS());
  Operand RHS = lowerToOperand(BO->getRHS());

  LocalId Tmp = TheBody->addTemp(BO->getType(), BO->getExprLoc());
  Place TmpPlace(Tmp, BO->getType(), BO->getExprLoc());
  emit(Statement::createAssign(
      TmpPlace, Rvalue::createBinaryOp(BO->getOpcode(), LHS, RHS),
      currentSafeZone(), BO, BO->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitUnaryOperator(UnaryOperator *UO) {
  UnaryOperatorKind Op = UO->getOpcode();

  // BSC borrow operators
  if (Op == UO_AddrMut || Op == UO_AddrMutDeref) {
    Place P = lowerToPlace(UO->getSubExpr());
    if (Op == UO_AddrMutDeref) {
      LocalId Tmp = TheBody->addTemp(UO->getType(), UO->getExprLoc());
      Place TmpPlace(Tmp, UO->getType(), UO->getExprLoc());
      emit(Statement::createAssign(
          TmpPlace, Rvalue::createRef(BorrowKind::Mut, P, /*IsReborrow=*/true),
          currentSafeZone(), UO, UO->getExprLoc()));
      return Operand::createCopy(TmpPlace);
    }
    LocalId Tmp = TheBody->addTemp(UO->getType(), UO->getExprLoc());
    Place TmpPlace(Tmp, UO->getType(), UO->getExprLoc());
    AddrOfOrigins[Tmp] = P;
    emit(Statement::createAssign(
        TmpPlace, Rvalue::createRef(BorrowKind::Mut, P),
        currentSafeZone(), UO, UO->getExprLoc()));
    return Operand::createCopy(TmpPlace);
  }

  if (Op == UO_AddrConst || Op == UO_AddrConstDeref) {
    Place P = lowerToPlace(UO->getSubExpr());
    bool IsReborrow = (Op == UO_AddrConstDeref);
    LocalId Tmp = TheBody->addTemp(UO->getType(), UO->getExprLoc());
    Place TmpPlace(Tmp, UO->getType(), UO->getExprLoc());
    if (!IsReborrow)
      AddrOfOrigins[Tmp] = P;
    emit(Statement::createAssign(
        TmpPlace, Rvalue::createRef(BorrowKind::Shared, P, IsReborrow),
        currentSafeZone(), UO, UO->getExprLoc()));
    return Operand::createCopy(TmpPlace);
  }

  // C-style address-of
  if (Op == UO_AddrOf) {
    Place P = lowerToPlace(UO->getSubExpr());
    LocalId Tmp = TheBody->addTemp(UO->getType(), UO->getExprLoc());
    Place TmpPlace(Tmp, UO->getType(), UO->getExprLoc());
    AddrOfOrigins[Tmp] = P;
    emit(Statement::createAssign(
        TmpPlace, Rvalue::createAddressOf(P),
        currentSafeZone(), UO, UO->getExprLoc()));
    return Operand::createCopy(TmpPlace);
  }

  // Dereference
  if (Op == UO_Deref) {
    Operand Sub = lowerToOperand(UO->getSubExpr());
    LocalId Tmp = TheBody->addTemp(UO->getSubExpr()->getType(),
                                   UO->getExprLoc());
    Place TmpPlace(Tmp, UO->getSubExpr()->getType(), UO->getExprLoc());
    emit(Statement::createAssign(TmpPlace, Rvalue::createUse(Sub),
                                 currentSafeZone(), UO, UO->getExprLoc()));
    Place Derefed = TmpPlace.project(
        ProjectionElem::createDeref(UO->getType()),
        TheBody->getAllocator());
    return Operand::createCopy(Derefed);
  }

  // Pre/Post increment/decrement: decompose into BinOp + write-back
  if (Op == UO_PreInc || Op == UO_PreDec ||
      Op == UO_PostInc || Op == UO_PostDec) {
    bool IsInc = (Op == UO_PreInc || Op == UO_PostInc);
    bool IsPre = (Op == UO_PreInc || Op == UO_PreDec);

    Place Dest = lowerToPlace(UO->getSubExpr());

    // Build the "1" constant with the correct bit-width for the operand type.
    // For pointer types, use the target pointer width; for integers, use the
    // type's actual width so downstream analyses see consistent bit-widths.
    QualType OpTy = UO->getSubExpr()->getType();
    unsigned BitWidth = OpTy->isPointerType()
        ? Ctx.getTargetInfo().getPointerWidth(static_cast<unsigned>(LangAS::Default))
        : Ctx.getTypeSize(OpTy);
    bool IsUnsigned = OpTy->isUnsignedIntegerType() || OpTy->isPointerType();
    Operand One = Operand::createConstant(
        APValue(llvm::APSInt(llvm::APInt(BitWidth, 1), IsUnsigned)),
        UO->getType());

    if (!IsPre) {
      // Post: save old value first
      LocalId Old = TheBody->addTemp(UO->getType(), UO->getExprLoc());
      Place OldPlace(Old, UO->getType(), UO->getExprLoc());
      emit(Statement::createAssign(OldPlace,
                                   Rvalue::createUse(Operand::createCopy(Dest)),
                                   currentSafeZone(), UO, UO->getExprLoc()));

      // new = old +/- 1
      LocalId New = TheBody->addTemp(UO->getType(), UO->getExprLoc());
      Place NewPlace(New, UO->getType(), UO->getExprLoc());
      emit(Statement::createAssign(
          NewPlace,
          Rvalue::createBinaryOp(IsInc ? BO_Add : BO_Sub,
                                 Operand::createCopy(Dest), One),
          currentSafeZone(), UO, UO->getExprLoc()));

      // write back: dest = new
      emit(Statement::createAssign(Dest,
                                   Rvalue::createUse(Operand::createCopy(NewPlace)),
                                   currentSafeZone(), UO, UO->getExprLoc()));

      // Post returns the old value
      return Operand::createCopy(OldPlace);
    }

    // Pre: new = old +/- 1, write back, return new
    LocalId New = TheBody->addTemp(UO->getType(), UO->getExprLoc());
    Place NewPlace(New, UO->getType(), UO->getExprLoc());
    emit(Statement::createAssign(
        NewPlace,
        Rvalue::createBinaryOp(IsInc ? BO_Add : BO_Sub,
                               Operand::createCopy(Dest), One),
        currentSafeZone(), UO, UO->getExprLoc()));

    // write back: dest = new
    emit(Statement::createAssign(Dest,
                                 Rvalue::createUse(Operand::createCopy(NewPlace)),
                                 currentSafeZone(), UO, UO->getExprLoc()));

    return Operand::createCopy(NewPlace);
  }

  // Other unary ops
  Operand Sub = lowerToOperand(UO->getSubExpr());
  LocalId Tmp = TheBody->addTemp(UO->getType(), UO->getExprLoc());
  Place TmpPlace(Tmp, UO->getType(), UO->getExprLoc());
  emit(Statement::createAssign(
      TmpPlace, Rvalue::createUnaryOp(Op, Sub),
      currentSafeZone(), UO, UO->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitCallExpr(CallExpr *CE) {
  const FunctionDecl *CalleeDecl = CE->getDirectCallee();

  // For direct calls, the callee is a constant placeholder (the Decl carries
  // the identity). For indirect calls (function pointers), lower the callee
  // expression so that the function pointer local is preserved in the IR.
  Operand Callee = CalleeDecl
      ? Operand::createConstant(APValue(), CE->getCallee()->getType())
      : lowerToOperand(CE->getCallee());

  // Extract FunctionProtoType from callee for indirect call ensure_init support.
  const FunctionProtoType *CalleeProtoType = nullptr;
  {
    QualType CalleeType = CE->getCallee()->getType();
    if (const auto *FnPtr = CalleeType->getAs<PointerType>())
      CalleeProtoType = FnPtr->getPointeeType()->getAs<FunctionProtoType>();
    else
      CalleeProtoType = CalleeType->getAs<FunctionProtoType>();
  }

  SmallVector<Operand, 4> Args;
  SmallVector<llvm::Optional<Place>, 4> ArgPlaces;
  for (unsigned I = 0; I < CE->getNumArgs(); ++I) {
    Operand Arg = lowerToOperand(CE->getArg(I));
    if (shouldMove(CE->getArg(I)) && Arg.K == Operand::Copy) {
      Arg.K = Operand::Move;
    }
    // Capture address-of origin for ensure_init support
    if (Arg.K == Operand::Copy || Arg.K == Operand::Move) {
      auto It = AddrOfOrigins.find(Arg.getPlace().Base);
      if (It != AddrOfOrigins.end()) {
        ArgPlaces.push_back(It->second);
        Args.push_back(std::move(Arg));
        continue;
      }
    }
    ArgPlaces.push_back(llvm::None);
    // Warn if this param has ensure_init but we can't track the origin.
    // Suppress if the argument is itself an ensure_init parameter (delegation).
    // Check both direct decl attrs and ExtParameterInfo (for indirect calls).
    bool ParamIsEnsureInit = false;
    if (CalleeDecl && I < CalleeDecl->getNumParams() &&
        CalleeDecl->getParamDecl(I)->hasAttr<EnsureInitAttr>())
      ParamIsEnsureInit = true;
    if (!ParamIsEnsureInit && CalleeProtoType &&
        CalleeProtoType->hasExtParameterInfos() &&
        I < CalleeProtoType->getNumParams() &&
        CalleeProtoType->getExtParameterInfo(I).isEnsureInit())
      ParamIsEnsureInit = true;
    if (ParamIsEnsureInit) {
      bool IsDelegation = false;
      if (auto *DRE = dyn_cast<DeclRefExpr>(CE->getArg(I)->IgnoreParenCasts()))
        if (auto *PVD = dyn_cast<ParmVarDecl>(DRE->getDecl()))
          if (PVD->hasAttr<EnsureInitAttr>())
            IsDelegation = true;
      if (!IsDelegation)
        Ctx.getDiagnostics().Report(
            CE->getArg(I)->getExprLoc(),
            diag::warn_ensure_init_not_addressof);
    }
    Args.push_back(std::move(Arg));
  }

  LocalId Dest = TheBody->addTemp(CE->getType(), CE->getExprLoc());
  Place DestPlace(Dest, CE->getType(), CE->getExprLoc());

  bool Diverges = false;
  if (CalleeDecl && CalleeDecl->isNoReturn())
    Diverges = true;
  if (!Diverges && CE->getCallee()->getType()->getAs<FunctionProtoType>()) {
    if (CE->getCallee()->getType()->getAs<FunctionProtoType>()->getNoReturnAttr())
      Diverges = true;
  }

  BasicBlockId Successor = createBlock();

  setTerminator(Terminator::createCall(
      std::move(Callee), std::move(Args), DestPlace, Successor,
      CalleeDecl, currentSafeZone(), CE, CE->getExprLoc(), Diverges,
      std::move(ArgPlaces), CalleeProtoType));

  switchToBlock(Successor);

  return Operand::createCopy(DestPlace);
}

Operand BSCIRBuilder::VisitCastExpr(CastExpr *CE) {
  if (CE->getCastKind() == CK_NoOp ||
      CE->getCastKind() == CK_LValueToRValue) {
    return lowerToOperand(CE->getSubExpr());
  }

  Operand Sub = lowerToOperand(CE->getSubExpr());
  LocalId Tmp = TheBody->addTemp(CE->getType(), CE->getExprLoc());
  Place TmpPlace(Tmp, CE->getType(), CE->getExprLoc());
  // Propagate AddrOfOrigins through pointer casts (e.g., int* -> void*)
  // so that ensure_init tracking can see through implicit casts.
  if (Sub.K == Operand::Copy || Sub.K == Operand::Move) {
    auto It = AddrOfOrigins.find(Sub.getPlace().Base);
    if (It != AddrOfOrigins.end())
      AddrOfOrigins[Tmp] = It->second;
  }
  emit(Statement::createAssign(
      TmpPlace, Rvalue::createCast(CE->getCastKind(), Sub, CE->getType()),
      currentSafeZone(), CE, CE->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitImplicitCastExpr(ImplicitCastExpr *CE) {
  return VisitCastExpr(CE);
}

Operand BSCIRBuilder::VisitDeclStmt(DeclStmt *DS) {
  for (auto *D : DS->decls()) {
    if (auto *VD = dyn_cast<VarDecl>(D)) {
      LocalId Id = getOrCreateLocal(VD);
      emit(Statement::createStorageLive(Id, currentSafeZone(),
                                        VD->getLocation()));

      // Track this local in the current scope for StorageDead
      if (!ScopeStack.empty())
        ScopeStack.back().Locals.push_back(Id);

      if (VD->hasInit()) {
        const Expr *Init = VD->getInit();
        Operand InitOp = lowerToOperand(Init);
        Place Dest(Id, VD->getType(), VD->getLocation());
        emit(Statement::createAssign(Dest, Rvalue::createUse(InitOp),
                                     currentSafeZone(), DS,
                                     VD->getLocation()));
      }
    }
  }
  return Operand::createConstant(APValue(), Ctx.VoidTy);
}

Operand BSCIRBuilder::VisitReturnStmt(ReturnStmt *RS) {
  if (RS->getRetValue()) {
    Operand RetVal = lowerToOperand(RS->getRetValue());
    Place RetPlace(LocalId{0}, FD.getReturnType(), RS->getReturnLoc());
    emit(Statement::createAssign(RetPlace, Rvalue::createUse(RetVal),
                                 currentSafeZone(), RS,
                                 RS->getReturnLoc()));
  }
  return Operand::createConstant(APValue(), Ctx.VoidTy);
}

Operand BSCIRBuilder::VisitMemberExpr(MemberExpr *ME) {
  if (isa<FieldDecl>(ME->getMemberDecl())) {
    Place P = lowerToPlace(ME);
    if (shouldMove(ME))
      return Operand::createMove(P);
    return Operand::createCopy(P);
  }
  return Operand::createConstant(APValue(), ME->getType());
}

Operand BSCIRBuilder::VisitArraySubscriptExpr(ArraySubscriptExpr *ASE) {
  Place P = lowerToPlace(ASE);
  return Operand::createCopy(P);
}

Operand BSCIRBuilder::VisitVAArgExpr(VAArgExpr *E) {
  // va_arg(args, type): lower the va_list sub-expression, produce a typed temp.
  lowerToOperand(E->getSubExpr());
  LocalId Tmp = TheBody->addTemp(E->getType(), E->getExprLoc());
  Place TmpPlace(Tmp, E->getType(), E->getExprLoc());
  emit(Statement::createAssign(
      TmpPlace, Rvalue::createUse(Operand::createConstant(APValue(), E->getType())),
      currentSafeZone(), E, E->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitPredefinedExpr(PredefinedExpr *E) {
  // __func__, __FUNCTION__, __PRETTY_FUNCTION__ — treat as string constant.
  if (auto *SL = E->getFunctionName())
    return VisitStringLiteral(const_cast<StringLiteral *>(SL));
  return Operand::createConstant(APValue(), E->getType());
}

Operand BSCIRBuilder::VisitCompoundLiteralExpr(CompoundLiteralExpr *E) {
  // (struct S){1, 2} — lower through the inner initializer.
  return lowerToOperand(E->getInitializer());
}

Operand BSCIRBuilder::VisitAtomicExpr(AtomicExpr *E) {
  // __atomic_load_n etc. — model as a Call terminator.
  SmallVector<Operand, 4> Args;
  for (unsigned I = 0; I < E->getNumSubExprs(); ++I) {
    Operand Arg = lowerToOperand(E->getSubExprs()[I]);
    Args.push_back(std::move(Arg));
  }

  LocalId Dest = TheBody->addTemp(E->getType(), E->getExprLoc());
  Place DestPlace(Dest, E->getType(), E->getExprLoc());
  BasicBlockId Successor = createBlock();

  // Build callee name from the AtomicOp enum.
  static const char *AtomicOpNames[] = {
#define BUILTIN(ID, TYPE, ATTRS)
#define ATOMIC_BUILTIN(ID, TYPE, ATTRS) #ID,
#include "clang/Basic/Builtins.def"
  };
  std::string CalleeName = AtomicOpNames[E->getOp()];

  Operand Callee = Operand::createConstant(APValue(), E->getType());
  auto T = Terminator::createCall(
      std::move(Callee), std::move(Args), DestPlace, Successor,
      /*FD=*/nullptr, currentSafeZone(), E, E->getExprLoc());
  T.getCall().CalleeName = std::move(CalleeName);
  setTerminator(std::move(T));

  switchToBlock(Successor);
  return Operand::createCopy(DestPlace);
}

Operand BSCIRBuilder::VisitImplicitValueInitExpr(ImplicitValueInitExpr *E) {
  // Zero-initialized field in an aggregate — just emit a constant, no nop.
  return Operand::createConstant(APValue(), E->getType());
}

Operand BSCIRBuilder::VisitInitListExpr(InitListExpr *ILE) {
  SmallVector<Operand, 4> Fields;
  for (unsigned I = 0; I < ILE->getNumInits(); ++I) {
    Operand FieldOp = lowerToOperand(ILE->getInit(I));
    if (shouldMove(ILE->getInit(I)) && FieldOp.K == Operand::Copy)
      FieldOp.K = Operand::Move;
    Fields.push_back(std::move(FieldOp));
  }
  LocalId Tmp = TheBody->addTemp(ILE->getType(), ILE->getExprLoc());
  Place TmpPlace(Tmp, ILE->getType(), ILE->getExprLoc());
  Rvalue RV;
  if (auto *RD = ILE->getType()->getAsRecordDecl()) {
    RV = Rvalue::createAggregate(RD, std::move(Fields));
  } else {
    QualType ElemTy;
    if (const auto *AT = ILE->getType()->getAsArrayTypeUnsafe())
      ElemTy = AT->getElementType();
    else
      ElemTy = ILE->getType();
    RV = Rvalue::createArray(ElemTy, std::move(Fields));
  }
  emit(Statement::createAssign(TmpPlace, std::move(RV), currentSafeZone(), ILE,
                               ILE->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitUnaryExprOrTypeTraitExpr(
    UnaryExprOrTypeTraitExpr *E) {
  if (E->getKind() == UETT_SizeOf) {
    QualType ArgTy;
    if (E->isArgumentType()) {
      ArgTy = E->getArgumentType();
    } else {
      ArgTy = E->getArgumentExpr()->getType();
    }
    LocalId Tmp = TheBody->addTemp(E->getType(), E->getExprLoc());
    Place TmpPlace(Tmp, E->getType(), E->getExprLoc());
    emit(Statement::createAssign(
        TmpPlace, Rvalue::createSizeOf(ArgTy),
        currentSafeZone(), E, E->getExprLoc()));
    return Operand::createCopy(TmpPlace);
  }
  return Operand::createConstant(APValue(), E->getType());
}

Operand BSCIRBuilder::VisitParenExpr(ParenExpr *PE) {
  return lowerToOperand(PE->getSubExpr());
}

Operand BSCIRBuilder::VisitCompoundAssignOperator(CompoundAssignOperator *CAO) {
  return VisitBinaryOperator(CAO);
}

Operand BSCIRBuilder::VisitConditionalOperator(ConditionalOperator *CO) {
  // Lower condition in current block
  Operand Cond = lowerToOperand(CO->getCond());

  // Result temporary — shared across both branches
  LocalId Result = TheBody->addTemp(CO->getType(), CO->getExprLoc());
  Place ResultPlace(Result, CO->getType(), CO->getExprLoc());

  BasicBlockId ThenBB = createBlock();
  BasicBlockId ElseBB = createBlock();
  BasicBlockId JoinBB = createBlock();

  // SwitchInt: 1 → then, otherwise → else
  SmallVector<std::pair<llvm::APInt, BasicBlockId>, 4> Targets;
  Targets.push_back({llvm::APInt(1, 1), ThenBB});
  setTerminator(Terminator::createSwitchInt(std::move(Cond),
                                            std::move(Targets), ElseBB,
                                            currentSafeZone()));

  // Then branch: eval true expr, assign to result
  switchToBlock(ThenBB);
  Operand TrueOp = lowerToOperand(CO->getTrueExpr());
  emit(Statement::createAssign(ResultPlace, Rvalue::createUse(TrueOp),
                               currentSafeZone(), CO, CO->getExprLoc()));
  setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

  // Else branch: eval false expr, assign to result
  switchToBlock(ElseBB);
  Operand FalseOp = lowerToOperand(CO->getFalseExpr());
  emit(Statement::createAssign(ResultPlace, Rvalue::createUse(FalseOp),
                               currentSafeZone(), CO, CO->getExprLoc()));
  setTerminator(Terminator::createGoto(JoinBB, currentSafeZone()));

  // Continue in join block
  switchToBlock(JoinBB);
  return Operand::createCopy(ResultPlace);
}

Operand BSCIRBuilder::VisitSafeExpr(SafeExpr *SE) {
  SafeZoneStack.push_back(SE->getSafeZoneSpecifier());
  Operand Result = lowerToOperand(SE->getSubExpr());
  SafeZoneStack.pop_back();
  return Result;
}

Operand BSCIRBuilder::VisitGNUNullExpr(GNUNullExpr *E) {
  LocalId Tmp = TheBody->addTemp(E->getType(), E->getExprLoc());
  Place TmpPlace(Tmp, E->getType(), E->getExprLoc());
  emit(Statement::createAssign(TmpPlace, Rvalue::createNullPtr(E->getType()),
                               currentSafeZone(), E, E->getExprLoc()));
  return Operand::createCopy(TmpPlace);
}

Operand BSCIRBuilder::VisitStmt(Stmt *S) {
  if (S)
    emit(Statement::createNop(currentSafeZone(), S->getBeginLoc()));
  return Operand::createConstant(APValue(), Ctx.VoidTy);
}

//===----------------------------------------------------------------------===//
// Label Pre-scan
//===----------------------------------------------------------------------===//

/// Walk the AST before lowering to record the scope depth of every label.
/// This mirrors the scope-push behavior of lowerCompoundStmt, lowerForStmt,
/// and lowerSwitchStmt so that forward gotos can compute the correct
/// StorageDead cleanup depth instead of falling back to 0.
void BSCIRBuilder::prescanLabels(const Stmt *S, unsigned Depth) {
  if (!S)
    return;

  if (const auto *CS = dyn_cast<CompoundStmt>(S)) {
    unsigned Inner = Depth + 1;
    for (const Stmt *Child : CS->body())
      prescanLabels(Child, Inner);
    return;
  }

  if (const auto *LS = dyn_cast<LabelStmt>(S)) {
    LabelScopeDepth[LS->getDecl()] = Depth;
    prescanLabels(LS->getSubStmt(), Depth);
    return;
  }

  if (const auto *FS = dyn_cast<ForStmt>(S)) {
    unsigned ForDepth = Depth;
    // ForStmt pushes an extra scope when the init clause is a DeclStmt.
    if (isa_and_nonnull<DeclStmt>(FS->getInit()))
      ++ForDepth;
    prescanLabels(FS->getInit(), ForDepth);
    prescanLabels(FS->getCond(), ForDepth);
    prescanLabels(FS->getInc(), ForDepth);
    prescanLabels(FS->getBody(), ForDepth);
    return;
  }

  if (const auto *SS = dyn_cast<SwitchStmt>(S)) {
    // SwitchStmt pushes a dedicated scope for the body.
    prescanLabels(SS->getCond(), Depth);
    prescanLabels(SS->getBody(), Depth + 1);
    return;
  }

  // Default: recurse into all children at the same depth.
  for (const Stmt *Child : S->children())
    prescanLabels(Child, Depth);
}

//===----------------------------------------------------------------------===//
// Main Build Entry Point
//===----------------------------------------------------------------------===//

std::unique_ptr<Body> BSCIRBuilder::build() {
  // Create return slot and parameter locals
  lowerParams();

  // Create entry block (bb0) — first real code goes here
  BasicBlockId EntryBB = TheBody->addBlock();
  CurrentBlock = EntryBB;

  // Create return block (will be populated at end)
  ReturnBlock = TheBody->addBlock();

  // Pre-scan to record scope depths of all labels before lowering.
  // This lets forward gotos emit correct StorageDead cleanup.
  const Stmt *FnBody = FD.getBody();
  if (FnBody)
    prescanLabels(FnBody, 0);

  // Lower the function body recursively
  if (FnBody)
    lowerStmt(FnBody);

  // If current block hasn't been terminated, goto return block
  if (TheBody->getBlock(CurrentBlock).Term.K == Terminator::Unreachable)
    setTerminator(Terminator::createGoto(ReturnBlock, currentSafeZone()));

  // Set return block terminator
  switchToBlock(ReturnBlock);
  setTerminator(Terminator::createReturn(TheBody->FuncSafeZone));

  // Simplify: collapse goto chains, merge blocks, remove dead blocks
  TheBody->simplify();

  // Compute predecessor map
  TheBody->computePredecessors();

  return std::move(TheBody);
}

#endif // ENABLE_BSC
