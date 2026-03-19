//===- BSCIRBuilder.h - AST to BSCIR lowering -*- C++ -*--------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Recursive AST lowering into BSCIR. Creates blocks in
// encounter order and threads control flow directly from AST structure.
// A SimplifyCfg pass runs post-build to collapse goto chains and remove
// dead blocks.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRBUILDER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRBUILDER_H

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIR.h"
#include "clang/AST/Decl.h"
#include "clang/AST/StmtVisitor.h"
#include "llvm/ADT/DenseMap.h"
#include <memory>

namespace clang {
namespace bscir {

/// Builds a BSCIR Body from an AST FunctionDecl using recursive lowering.
class BSCIRBuilder : public StmtVisitor<BSCIRBuilder, Operand> {
public:
  BSCIRBuilder(ASTContext &Ctx, const FunctionDecl &FD);

  /// Build the complete BSCIR Body. Returns nullptr on failure.
  std::unique_ptr<Body> build();

  // StmtVisitor methods - return an Operand representing the result
  Operand VisitDeclRefExpr(DeclRefExpr *E);
  Operand VisitIntegerLiteral(IntegerLiteral *E);
  Operand VisitFloatingLiteral(FloatingLiteral *E);
  Operand VisitCharacterLiteral(CharacterLiteral *E);
  Operand VisitStringLiteral(StringLiteral *E);
  Operand VisitBinaryOperator(BinaryOperator *BO);
  Operand VisitUnaryOperator(UnaryOperator *UO);
  Operand VisitCallExpr(CallExpr *CE);
  Operand VisitCastExpr(CastExpr *CE);
  Operand VisitImplicitCastExpr(ImplicitCastExpr *CE);
  Operand VisitDeclStmt(DeclStmt *DS);
  Operand VisitReturnStmt(ReturnStmt *RS);
  Operand VisitMemberExpr(MemberExpr *ME);
  Operand VisitArraySubscriptExpr(ArraySubscriptExpr *ASE);
  Operand VisitVAArgExpr(VAArgExpr *E);
  Operand VisitPredefinedExpr(PredefinedExpr *E);
  Operand VisitCompoundLiteralExpr(CompoundLiteralExpr *E);
  Operand VisitAtomicExpr(AtomicExpr *E);
  Operand VisitImplicitValueInitExpr(ImplicitValueInitExpr *E);
  Operand VisitInitListExpr(InitListExpr *ILE);
  Operand VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *E);
  Operand VisitParenExpr(ParenExpr *PE);
  Operand VisitCompoundAssignOperator(CompoundAssignOperator *CAO);
  Operand VisitConditionalOperator(ConditionalOperator *CO);
  Operand VisitGNUNullExpr(GNUNullExpr *E);
  Operand VisitSafeExpr(SafeExpr *SE);
  Operand VisitStmt(Stmt *S); // fallback

private:
  ASTContext &Ctx;
  const FunctionDecl &FD;
  std::unique_ptr<Body> TheBody;

  // Current block being built
  BasicBlockId CurrentBlock = {0};

  // Map from VarDecl to LocalId
  llvm::DenseMap<const VarDecl *, LocalId> VarMap;

  // Safe zone tracking stack
  SmallVector<SafeZoneSpecifier, 4> SafeZoneStack;

  // --- Breakable scope tracking (for break/continue) ---
  struct BreakableScope {
    BasicBlockId BreakTarget;
    BasicBlockId ContinueTarget;
    bool HasContinue; // false for switch (no continue target)
    unsigned ScopeDepth; // ScopeStack depth when the loop/switch was entered
  };
  SmallVector<BreakableScope, 4> BreakableScopes;

  // --- Scope tracking for StorageDead emission ---
  struct ScopeInfo {
    SmallVector<LocalId, 4> Locals;
  };
  SmallVector<ScopeInfo, 8> ScopeStack;

  // --- Label/goto support ---
  llvm::DenseMap<LabelDecl *, BasicBlockId> LabelMap;
  llvm::DenseMap<LabelDecl *, unsigned> LabelScopeDepth;

  // --- Address-of origin tracking (for ensure_init) ---
  // Maps temp LocalId → the Place whose address was taken to produce that temp.
  llvm::DenseMap<LocalId, Place> AddrOfOrigins;

  // --- Single return block (created in build()) ---
  BasicBlockId ReturnBlock = {0};

  // Get the current safe zone from the SafeZoneStack
  SafeZoneSpecifier currentSafeZone() const;

  // Lower parameters
  void lowerParams();

  // --- Recursive statement lowering ---
  void lowerStmt(const Stmt *S);
  void lowerCompoundStmt(const CompoundStmt *CS);
  void lowerIfStmt(const IfStmt *IS);
  void lowerWhileStmt(const WhileStmt *WS);
  void lowerForStmt(const ForStmt *FS);
  void lowerDoWhileStmt(const DoStmt *DS);
  void lowerSwitchStmt(const SwitchStmt *SS);

  /// Emit a branch for a loop condition. If the condition is a compile-time
  /// constant, emits a direct goto; otherwise emits switchInt.
  void emitCondBranch(const Expr *Cond, BasicBlockId BodyBB,
                          BasicBlockId ExitBB);

  void lowerBreakStmt(const BreakStmt *BS);
  void lowerContinueStmt(const ContinueStmt *CS);
  void lowerGotoStmt(const GotoStmt *GS);
  void lowerLabelStmt(const LabelStmt *LS);
  void lowerReturnStmt(const ReturnStmt *RS);

  // Pre-scan function body to record scope depths of all labels.
  // This allows forward gotos to know the target label's scope depth
  // without needing a second pass during lowering.
  void prescanLabels(const Stmt *S, unsigned Depth);

  // Get or create a label block for a LabelDecl
  BasicBlockId getOrCreateLabelBlock(LabelDecl *LD);

  // Emit StorageDead for all locals in scopes up to (but not including)
  // the target scope depth (for break/continue/goto)
  void emitScopeCleanup(unsigned TargetDepth);

  // Emit a statement into the current block
  void emit(Statement S);

  // Set the terminator of the current block
  void setTerminator(Terminator T);

  // Start building a new block, return the old block ID
  BasicBlockId switchToBlock(BasicBlockId NewBlock);

  // Get or create a LocalId for a VarDecl
  LocalId getOrCreateLocal(const VarDecl *VD);

  // Lower an expression to a Place (for lvalues)
  Place lowerToPlace(const Expr *E);

  // Lower an expression to an Operand (for rvalues)
  Operand lowerToOperand(const Expr *E);

  // Determine if an expression should be moved (based on _Owned qualifier)
  bool shouldMove(const Expr *E) const;

  // Create a new block and return its ID
  BasicBlockId createBlock();

};

} // namespace bscir
} // namespace clang

#endif // ENABLE_BSC
#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRBUILDER_H
