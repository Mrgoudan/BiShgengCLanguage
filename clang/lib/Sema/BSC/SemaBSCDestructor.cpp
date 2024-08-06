#if ENABLE_BSC

#include <stack>

#include "TreeTransform.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Sema/Scope.h"
#include "clang/Sema/Template.h"

using namespace clang;
using namespace sema;

static BSCMethodDecl *buildBSCMethodDecl(ASTContext &C, DeclContext *DC,
                                         SourceLocation StartLoc,
                                         SourceLocation NLoc, DeclarationName N,
                                         QualType T, TypeSourceInfo *TInfo,
                                         StorageClass SC, QualType ET) {
  BSCMethodDecl *NewDecl =
      BSCMethodDecl::Create( // TODO: inline should be passed.
          C, DC, StartLoc, DeclarationNameInfo(N, NLoc), T, TInfo, SC, false,
          false, ConstexprSpecKind::Unspecified, NLoc);
  return NewDecl;
}

bool IsVarDeclWithOwnedStructureType(VarDecl *VD) {
  const Type *VDType = VD->getType().getCanonicalType().getTypePtr();
  if (VDType->isOwnedStructureType())
    return true;
  return false;
}

// Collect owned struct type field that has destructor.
std::stack<FieldDecl *> CollectInstanceFieldWithDestructor(RecordDecl *RD) {
  std::stack<FieldDecl *> OwnedStructFields;
  for (RecordDecl::field_iterator FieldIt = RD->field_begin();
       FieldIt != RD->field_end(); ++FieldIt) {
    const Type *FieldType = FieldIt->getType().getCanonicalType().getTypePtr();
    if (FieldType->isOwnedStructureType()) {
      RecordDecl *RD = cast<RecordType>(FieldType)->getDecl();
      if (RD->getBSCDestructor() && !RD->getBSCDestructor()->isInvalidDecl()) {
        OwnedStructFields.push(*FieldIt);
      }
    }
  }
  return OwnedStructFields;
}

BSCMethodDecl *Sema::getOrInsertBSCDestructor(RecordDecl *RD) {
  BSCMethodDecl *Destructor;
  if (RD->getBSCDestructor() == nullptr) {
    assert(RD->isOwnedDecl());
    QualType FuncRetType = getASTContext().VoidTy;
    QualType ParamType = getASTContext().getRecordType(RD);
    ParamType.addOwned();
    SmallVector<QualType, 1> ParamTys;
    ParamTys.push_back(ParamType);
    QualType FuncType =
        getASTContext().getFunctionType(FuncRetType, ParamTys, {});

    DeclarationName Name = Context.DeclarationNames.getCXXDestructorName(
        Context.getCanonicalType(Context.getTypeDeclType(RD)));

    std::string FName = "~" + RD->getDeclName().getAsString();

    Destructor = buildBSCMethodDecl(
        getASTContext(), RD, RD->getEndLoc(), RD->getEndLoc(), Name, FuncType,
        nullptr, SC_None, RD->getTypeForDecl()->getCanonicalTypeInternal());
    SmallVector<ParmVarDecl *, 1> ParmVarDecls;
    ParmVarDecl *PVD = ParmVarDecl::Create(
        getASTContext(), Destructor, RD->getEndLoc(), RD->getEndLoc(),
        &(getASTContext().Idents).get("this"), ParamType, nullptr, SC_None,
        nullptr);
    ParmVarDecls.push_back(PVD);
    Destructor->setParams(ParmVarDecls);
    RD->addDecl(Destructor);
    Destructor->setLexicalDeclContext(getASTContext().getTranslationUnitDecl());
    Destructor->setDestructor(true);

    std::vector<Stmt *> Stmts;
    CompoundStmt *CS =
        CompoundStmt::Create(getASTContext(), Stmts, FPOptionsOverride(),
                             RD->getEndLoc(), RD->getEndLoc());
    Destructor->setBody(CS);
    PushOnScopeChains(Destructor, getCurScope(), false);
  } else {
    Destructor = RD->getBSCDestructor();
  }
  return Destructor;
}

void Sema::HandleBSCDestructorBody(RecordDecl *RD, BSCMethodDecl *Destructor,
                                   std::stack<FieldDecl *> InstanceFields) {
  if (InstanceFields.empty())
    return;
  Stmt *FuncBody = Destructor->getBody();
  if (auto *CS = dyn_cast<CompoundStmt>(FuncBody)) {
    std::vector<Stmt *> Stmts;
    for (auto *C : CS->children()) {
      Stmts.push_back(C);
    }
    while (!InstanceFields.empty()) {
      FieldDecl *Field = InstanceFields.top();
      InstanceFields.pop();

      const Type *FieldType = Field->getType().getCanonicalType().getTypePtr();
      if (FieldType->isOwnedStructureType()) {
        RecordDecl *ThisRD = cast<RecordType>(FieldType)->getDecl();
        BSCMethodDecl *DestructorToCall = ThisRD->getBSCDestructor();
        ParmVarDecl *PVD = Destructor->getParamDecl(0);
        QualType ParamType = getASTContext().getRecordType(RD);
        ParamType.addOwned();

        Expr *DRE =
            BuildDeclRefExpr(PVD, ParamType, VK_LValue, CS->getRBracLoc());
        if (!ParamType->getAsCXXRecordDecl()) {
          DRE = ImplicitCastExpr::Create(Context, ParamType, CK_LValueToRValue,
                                         DRE, nullptr, VK_PRValue,
                                         FPOptionsOverride());
        }
        Expr *MemberExpr = BuildMemberExpr(
            DRE, false, SourceLocation(), NestedNameSpecifierLoc(),
            SourceLocation(), Field,
            DeclAccessPair::make(RD, Field->getAccess()), false,
            DeclarationNameInfo(), Field->getType().getNonReferenceType(),
            VK_LValue, OK_Ordinary);
        SmallVector<Expr *, 1> Args;
        Args.push_back(MemberExpr);
        Expr *DestructorRef =
            BuildDeclRefExpr(DestructorToCall, DestructorToCall->getType(),
                             VK_LValue, SourceLocation());
        DestructorRef =
            ImpCastExprToType(DestructorRef,
                              Context.getPointerType(DestructorRef->getType()),
                              CK_FunctionToPointerDecay)
                .get();
        Expr *CE = BuildCallExpr(nullptr, DestructorRef, SourceLocation(), Args,
                                 SourceLocation())
                       .get();
        Stmts.push_back(CE);
      }
    }
    CompoundStmt *NewCS =
        CompoundStmt::Create(getASTContext(), Stmts, FPOptionsOverride(),
                             CS->getLBracLoc(), CS->getRBracLoc());
    Destructor->setBody(NewCS);
  }
}

namespace {

class DeclRefFinder : public RecursiveASTVisitor<DeclRefFinder> {
  Sema &SemaRef;

public:
  DeclRefFinder(Sema &SemaRef) : SemaRef(SemaRef) {}

  bool VisitDeclRefExpr(DeclRefExpr *E) {
    const Type *VarDeclType = E->getType().getCanonicalType().getTypePtr();
    if (VarDeclType->isOwnedStructureType()) {
      RecordDecl *ThisRD = cast<RecordType>(VarDeclType)->getDecl();
      BSCMethodDecl *DestructorToCall = ThisRD->getBSCDestructor();
      if (DestructorToCall) {
        if (isa<VarDecl>(E->getDecl())) {
          MovedDecls.push_back(cast<VarDecl>(E->getDecl()));
        }
      }
    }
    return true;
  }

  bool VisitUnaryOperator(const UnaryOperator *UOp) { return false; }

  bool VisitMemberExpr(const MemberExpr *MA) { return false; }

  llvm::SmallVector<VarDecl *> MovedDecls;
};
llvm::DenseMap<CompoundStmt *, CompoundStmt *> ReplaceCompoundMap;

class InsertDestructorCallStmt
    : public RecursiveASTVisitor<InsertDestructorCallStmt> {
  Sema &SemaRef;
  std::vector<CompoundStmt *> CompoundStmts;
  llvm::DenseMap<VarDecl *, VarDecl *> FlagMap;
  FunctionDecl *FD;

public:
  explicit InsertDestructorCallStmt(Sema &SemaRef) : SemaRef(SemaRef) {}

  // Add if stmt for owned struct type vardecl.
  std::vector<Stmt *> AddIfStmts(Stmt *stmt) {
    std::vector<Stmt *> IfStmts;
    SmallVector<VarDecl *> VarDecls = SemaRef.Context.DestructMap[FD][stmt];
    for (auto *VD : VarDecls) {
      Stmt *Body = nullptr;
      if (IsVarDeclWithOwnedStructureType(VD)) {
        const Type *VDType = VD->getType().getCanonicalType().getTypePtr();
        RecordDecl *RD = cast<RecordType>(VDType)->getDecl();
        BSCMethodDecl *DestructorToCall = RD->getBSCDestructor();
        if (DestructorToCall == nullptr) {
          continue;
        }
        Expr *IDRE = SemaRef.BuildDeclRefExpr(
            VD, VD->getType().getCanonicalType(), VK_LValue, SourceLocation());
        SmallVector<Expr *, 1> Args;
        Args.push_back(IDRE);
        Expr *DestructorRef = SemaRef.BuildDeclRefExpr(
            DestructorToCall, DestructorToCall->getType(), VK_LValue,
            SourceLocation());
        DestructorRef = SemaRef
                            .ImpCastExprToType(DestructorRef,
                                               SemaRef.Context.getPointerType(
                                                   DestructorRef->getType()),
                                               CK_FunctionToPointerDecay)
                            .get();
        Expr *CE = SemaRef
                       .BuildCallExpr(nullptr, DestructorRef, SourceLocation(),
                                      Args, SourceLocation())
                       .get();
        Body = CE;
      }
      if (Body == nullptr)
        continue;
      Expr *FlagRefExpr = SemaRef.BuildDeclRefExpr(
          FlagMap[VD], FlagMap[VD]->getType(), VK_LValue, SourceLocation());
      FlagRefExpr =
          SemaRef.CreateBuiltinUnaryOp(SourceLocation(), UO_LNot, FlagRefExpr)
              .get();
      Sema::ConditionResult IfCond = SemaRef.ActOnCondition(
          nullptr, SourceLocation(), FlagRefExpr, Sema::ConditionKind::Boolean);
      Stmt *If =
          SemaRef
              .BuildIfStmt(SourceLocation(), IfStatementKind::Ordinary,
                           /* LPL=*/SourceLocation(), /* Init=*/nullptr, IfCond,
                           /* RPL=*/SourceLocation(), Body, SourceLocation(),
                           nullptr)
              .get();
      IfStmts.push_back(If);
    }
    return IfStmts;
  }
  // Create _Bool xx_is_moved = 0;
  DeclStmt *CreateMoveFlag(VarDecl *D) {
    std::string IName = D->getName().str() + "_is_moved";
    VarDecl *VD =
        VarDecl::Create(SemaRef.Context, FD, D->getLocation(), D->getLocation(),
                        &(SemaRef.Context.Idents).get(IName),
                        SemaRef.Context.BoolTy, nullptr, SC_None);
    llvm::APInt Zero(SemaRef.Context.getTypeSize(SemaRef.Context.IntTy), 0);
    Expr *IInit = IntegerLiteral::Create(
        SemaRef.Context, Zero, SemaRef.Context.IntTy, SourceLocation());
    IInit = SemaRef
                .ImpCastExprToType(IInit, SemaRef.Context.BoolTy,
                                   CK_IntegralToBoolean)
                .get();
    VD->setInit(IInit);
    DeclGroupRef DataDG(VD);
    DeclStmt *DataDS = new (SemaRef.Context)
        DeclStmt(DataDG, D->getLocation(), D->getLocation());
    FlagMap[D] = VD;
    return DataDS;
  }

  // Create xx_is_moved = 1;
  Stmt *MoveFlagStatusUpdate(VarDecl *D) {
    Expr *LHS = SemaRef.BuildDeclRefExpr(FlagMap[D], FlagMap[D]->getType(),
                                         VK_LValue, SourceLocation());
    llvm::APInt One(SemaRef.Context.getTypeSize(SemaRef.Context.IntTy), 1);
    Expr *IInit = IntegerLiteral::Create(
        SemaRef.Context, One, SemaRef.Context.IntTy, SourceLocation());
    IInit = SemaRef
                .ImpCastExprToType(IInit, SemaRef.Context.BoolTy,
                                   CK_IntegralToBoolean)
                .get();
    Expr *BinOpExpr =
        SemaRef.CreateBuiltinBinOp(SourceLocation(), BO_Assign, LHS, IInit)
            .get();
    return BinOpExpr;
  }

  bool VisitCompoundStmt(CompoundStmt *compoundStmt) {
    CompoundStmts.push_back(compoundStmt);
    std::vector<Stmt *> Statements;
    if (compoundStmt == FD->getBody()) {
      for (ParmVarDecl *PVD : FD->parameters()) {
        if (IsVarDeclWithOwnedStructureType(PVD)) {
          Statements.push_back(CreateMoveFlag(PVD));
        }
      }
    }
    bool HasControlTransferExpr = false;
    for (auto *C : compoundStmt->children()) {
      Stmt *SS = const_cast<Stmt *>(C);
      if (isa<ReturnStmt>(SS) || isa<BreakStmt>(SS) || isa<ContinueStmt>(SS)) {
        HasControlTransferExpr = true;
        std::vector<Stmt *> IfStmts = AddIfStmts(SS);
        Statements.insert(Statements.end(), IfStmts.begin(), IfStmts.end());
      }
      RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseStmt(C);
      if (isa<CompoundStmt>(SS)) {
        Statements.push_back(ReplaceCompoundMap[dyn_cast<CompoundStmt>(SS)]);
      } else if (isa<LabelStmt>(SS)) {
        auto labelStmt = cast<LabelStmt>(SS);
        if (!isa<CompoundStmt>(labelStmt->getSubStmt())) {
          auto stmts = CreateStatements(labelStmt->getSubStmt());
          labelStmt->setSubStmt(stmts[0]);
          Statements.push_back(labelStmt);
          Statements.insert(Statements.end(), stmts.begin() + 1, stmts.end());
        } else {
          Statements.push_back(C);
        }
      } else {
        Statements.push_back(C);
      }
      if (isa<DeclStmt>(SS)) {
        for (auto *D : cast<DeclStmt>(SS)->decls()) {
          if (isa<VarDecl>(D)) {
            VarDecl *VD = cast<VarDecl>(D);
            if (IsVarDeclWithOwnedStructureType(VD)) {
              Statements.push_back(CreateMoveFlag(VD));
            }
          }
        }
      }

      if (isa<BinaryOperator>(SS) || isa<DeclStmt>(SS) || isa<CallExpr>(SS)) {
        DeclRefFinder Finder = DeclRefFinder(SemaRef);
        Finder.TraverseStmt(SS);
        for (auto *D : Finder.MovedDecls) {
          Statements.push_back(MoveFlagStatusUpdate(D));
        }
      }
    }
    if (!HasControlTransferExpr) {
      auto IfStmts = AddIfStmts(compoundStmt);
      Statements.insert(Statements.end(), IfStmts.begin(), IfStmts.end());
    }
    auto NewCPStmt = CompoundStmt::Create(
        SemaRef.getASTContext(), Statements, FPOptionsOverride(),
        compoundStmt->getLBracLoc(), compoundStmt->getRBracLoc());
    ReplaceCompoundMap[compoundStmt] = NewCPStmt;
    return false;
  }

  std::vector<Stmt *> CreateStatements(Stmt *SS) {
    std::vector<Stmt *> Statements;
    if (isa<ReturnStmt>(SS) || isa<BreakStmt>(SS) || isa<ContinueStmt>(SS)) {
      std::vector<Stmt *> IfStmts = AddIfStmts(SS);
      Statements.insert(Statements.end(), IfStmts.begin(), IfStmts.end());
    }
    Statements.push_back(SS);
    if (isa<DeclStmt>(SS)) {
      for (auto *D : cast<DeclStmt>(SS)->decls()) {
        if (isa<VarDecl>(D)) {
          VarDecl *VD = cast<VarDecl>(D);
          if (IsVarDeclWithOwnedStructureType(VD)) {
            Statements.push_back(CreateMoveFlag(VD));
          }
        }
      }
    }
    if (isa<BinaryOperator>(SS) || isa<DeclStmt>(SS) || isa<CallExpr>(SS)) {
      DeclRefFinder Finder = DeclRefFinder(SemaRef);
      Finder.TraverseStmt(SS);
      for (auto *D : Finder.MovedDecls) {
        Statements.push_back(MoveFlagStatusUpdate(D));
      }
    }
    return Statements;
  }

  Stmt *CreateNewCompoundStmt(Stmt *SS) {
    auto Stmts = CreateStatements(SS);
    if (Stmts.size() <= 1)
      return SS;
    return CompoundStmt::Create(SemaRef.getASTContext(), Stmts,
                                FPOptionsOverride(), SourceLocation(),
                                SourceLocation());
  }

  bool TraverseWhileStmt(WhileStmt *whileStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseWhileStmt(whileStmt);
    if (auto NewCompound =
            ReplaceCompoundMap[dyn_cast<CompoundStmt>(whileStmt->getBody())]) {
      whileStmt->setBody(NewCompound);
    } else {
      whileStmt->setBody(CreateNewCompoundStmt(whileStmt->getBody()));
    }
    return false;
  }

  bool TraverseIfStmt(IfStmt *ifStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseIfStmt(ifStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(ifStmt->getThen())) {
      ifStmt->setThen(ReplaceCompoundMap[NewCompound]);
    } else {
      ifStmt->setThen(CreateNewCompoundStmt(ifStmt->getThen()));
    }
    return false;
  }
  bool TraverseDoStmt(DoStmt *doStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseDoStmt(doStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(doStmt->getBody())) {
      doStmt->setBody(ReplaceCompoundMap[NewCompound]);
    } else {
      doStmt->setBody(CreateNewCompoundStmt(doStmt->getBody()));
    }
    return false;
  }
  bool TraverseForStmt(ForStmt *forStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseForStmt(forStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(forStmt->getBody())) {
      forStmt->setBody(ReplaceCompoundMap[NewCompound]);
    } else {
      forStmt->setBody(CreateNewCompoundStmt(forStmt->getBody()));
    }
    return false;
  }

  bool TraverseLabelStmt(LabelStmt *labelStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseLabelStmt(labelStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(labelStmt->getSubStmt())) {
      labelStmt->setSubStmt(ReplaceCompoundMap[NewCompound]);
    }
    return false;
  }

  bool TraverseSwitchStmt(SwitchStmt *switchStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseSwitchStmt(
        switchStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(switchStmt->getBody())) {
      switchStmt->setBody(ReplaceCompoundMap[NewCompound]);
    }
    return false;
  }

  bool TraverseDefaultStmt(DefaultStmt *defaultStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseDefaultStmt(
        defaultStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(defaultStmt->getSubStmt())) {
      defaultStmt->setSubStmt(ReplaceCompoundMap[NewCompound]);
    }
    return false;
  }

  bool TraverseCaseStmt(CaseStmt *caseStmt) {
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseCaseStmt(caseStmt);
    if (auto NewCompound = dyn_cast<CompoundStmt>(caseStmt->getSubStmt())) {
      caseStmt->setSubStmt(ReplaceCompoundMap[NewCompound]);
    }
    return false;
  }

  bool TraverseFunctionDecl(FunctionDecl *D) {
    FD = D;
    RecursiveASTVisitor<InsertDestructorCallStmt>::TraverseFunctionDecl(D);
    if (auto NewCompound =
            ReplaceCompoundMap[dyn_cast<CompoundStmt>(D->getBody())]) {
      D->setBody(NewCompound);
    }
    return false;
  }
};

} // namespace

class CalcDestructorMapForIns
    : public RecursiveASTVisitor<CalcDestructorMapForIns> {
  Sema &SemaRef;
  FunctionDecl *FD;
  bool IsDestructor = false;
  std::stack<CompoundStmt *> VisitCompoundStmtStack;
  llvm::DenseMap<CompoundStmt *, SmallVector<VarDecl *>> CS2Vars;

public:
  explicit CalcDestructorMapForIns(Sema &SemaRef) : SemaRef(SemaRef) {}

  bool VisitCompoundStmt(CompoundStmt *compoundStmt) {
    VisitCompoundStmtStack.push(compoundStmt);
    SmallVector<VarDecl *> &VarDecls = CS2Vars[compoundStmt];
    if (compoundStmt == FD->getBody() && !IsDestructor) {
      for (ParmVarDecl *PVD : FD->parameters()) {
        if (IsVarDeclWithOwnedStructureType(PVD)) {
          VarDecls.insert(VarDecls.begin(), PVD);
        }
      }
    }
    for (auto *C : compoundStmt->children()) {
      Stmt *SS = const_cast<Stmt *>(C);
      if (DeclStmt *StmtDecl = dyn_cast<DeclStmt>(SS)) {
        for (auto *SD : StmtDecl->decls()) {
          if (isa<VarDecl>(SD) &&
              IsVarDeclWithOwnedStructureType(cast<VarDecl>(SD))) {
            VarDecls.insert(VarDecls.begin(), cast<VarDecl>(SD));
          }
        }
      }
      if (isa<ReturnStmt>(SS) && !VisitCompoundStmtStack.empty()) {
        std::stack<CompoundStmt *> tempStack = VisitCompoundStmtStack;
        SmallVector<VarDecl *> newVarDecls;
        while (!tempStack.empty()) {
          auto CompoundStmt = tempStack.top();
          newVarDecls.insert(newVarDecls.begin(), CS2Vars[CompoundStmt].begin(),
                             CS2Vars[CompoundStmt].end());
          tempStack.pop();
        }
        if (newVarDecls.size() != 0) {
          SemaRef.Context.DestructMap[FD][SS] = newVarDecls;
        }
      }
      if (isa<BreakStmt>(SS) || isa<ContinueStmt>(SS)) {
        if (VarDecls.size() != 0) {
          SemaRef.Context.DestructMap[FD][SS] = VarDecls;
        }
      }
      RecursiveASTVisitor<CalcDestructorMapForIns>::TraverseStmt(C);
    }
    if (VarDecls.size() != 0) {
      SemaRef.Context.DestructMap[FD][compoundStmt] = VarDecls;
    }
    VisitCompoundStmtStack.pop();
    return false;
  }

  bool TraverseFunctionDecl(FunctionDecl *D) {
    FD = D;
    if (auto MD = dyn_cast<BSCMethodDecl>(D)) {
      IsDestructor = MD->isDestructor();
    }
    return RecursiveASTVisitor<CalcDestructorMapForIns>::TraverseFunctionDecl(
        D);
  }
};

class ConstructorCompound : public RecursiveASTVisitor<ConstructorCompound> {
  Sema &SemaRef;

public:
  explicit ConstructorCompound(Sema &SemaRef) : SemaRef(SemaRef) {}

  Stmt *CreateNewCompoundStmt(Stmt *SS) {
    return CompoundStmt::Create(SemaRef.getASTContext(), {SS},
                                FPOptionsOverride(), SourceLocation(),
                                SourceLocation());
  }

  bool TraverseWhileStmt(WhileStmt *whileStmt) {
    RecursiveASTVisitor<ConstructorCompound>::TraverseWhileStmt(whileStmt);
    if (!isa<CompoundStmt>(whileStmt->getBody())) {
      whileStmt->setBody(CreateNewCompoundStmt(whileStmt->getBody()));
    }
    return false;
  }

  bool TraverseIfStmt(IfStmt *ifStmt) {
    RecursiveASTVisitor<ConstructorCompound>::TraverseIfStmt(ifStmt);
    if (!isa<CompoundStmt>(ifStmt->getThen())) {
      ifStmt->setThen(CreateNewCompoundStmt(ifStmt->getThen()));
    }
    return false;
  }
  bool TraverseDoStmt(DoStmt *doStmt) {
    RecursiveASTVisitor<ConstructorCompound>::TraverseDoStmt(doStmt);
    if (!isa<CompoundStmt>(doStmt->getBody())) {
      doStmt->setBody(CreateNewCompoundStmt(doStmt->getBody()));
    }
    return false;
  }
  bool TraverseForStmt(ForStmt *forStmt) {
    RecursiveASTVisitor<ConstructorCompound>::TraverseForStmt(forStmt);
    if (!isa<CompoundStmt>(forStmt->getBody())) {
      forStmt->setBody(CreateNewCompoundStmt(forStmt->getBody()));
    }
    return false;
  }
};

void Sema::DesugarDestructorCall(FunctionDecl *FD) {
  if (!getLangOpts().BSC)
    return;
  // Skip function template and class template.
  if (auto RD = dyn_cast<RecordDecl>(FD->getParent())) {
    if (RD->getDescribedClassTemplate() != nullptr)
      return;
  }
  if (FD->getTemplatedKind() == FunctionDecl::TK_FunctionTemplate)
    return;
  CollDestructMapInFuncInstantiation(FD);
  if (Context.DestructMap[FD].empty())
    return;
  // Insert destructor function calls.
  InsertDestructorCallStmt IDCS(*this);
  IDCS.TraverseFunctionDecl(FD);
}

void Sema::DesugarDestructor(RecordDecl *RD) {
  if (!getLangOpts().BSC || !RD->isOwnedDecl() || !RD->isCompleteDefinition()) {
    return;
  }
  BSCMethodDecl *Destructor = getOrInsertBSCDestructor(RD);
  if (Destructor->isInvalidDecl())
    return;
  std::stack<FieldDecl *> Fields = CollectInstanceFieldWithDestructor(RD);
  HandleBSCDestructorBody(RD, Destructor, Fields);
  CheckBSCOwnership(Destructor);
}

void Sema::CheckBSCDestructorDeclarator(FunctionDecl *NewFD) {
  RecordDecl *Record = cast<RecordDecl>(NewFD->getParent());
  if (Record->isInvalidDecl())
    return;
  QualType ClassType = Context.getTypeDeclType(Record);
  DeclarationName Name = Context.DeclarationNames.getCXXDestructorName(
      Context.getCanonicalType(ClassType));
  if (NewFD->getDeclName() != Name) {
    Diag(NewFD->getLocation(), diag::err_owned_struct_destructor_name);
    NewFD->setInvalidDecl();
    return;
  }

  std::string TypeName;
  if (isa<InjectedClassNameType>(ClassType)) {
    // handle generic owned struct type
    TypeName = ClassType.getAsString();
  } else {
    TypeName = ClassType.getBaseTypeIdentifier()->getName().str();
  }

  if (NewFD->getNumParams() == 0) {
    // Diag(NewFD->getLocation(), diag::err_destructor_no_param);
    Diag(NewFD->getLocation(), diag::invalid_param_for_destructor) << TypeName;
    NewFD->setInvalidDecl();
    return;
  }

  bool IsEqualType = true;
  if (isa<InjectedClassNameType>(ClassType)) {
    IsEqualType = ("owned " + ClassType.getAsString()) ==
                  NewFD->getParamDecl(0)->getType().getAsString();
  } else {
    auto ParamType = NewFD->getParamDecl(0)->getType().getTypePtrOrNull();
    if (auto CT = ClassType.getTypePtrOrNull()) {
      IsEqualType = CT->getCanonicalTypeUnqualified().getTypePtrOrNull() ==
                    ParamType->getCanonicalTypeUnqualified().getTypePtrOrNull();
    }
  }

  if ((!IsEqualType || NewFD->getParamDecl(0)->getName() != "this")) {
    Diag(NewFD->getParamDecl(0)->getLocation(),
         diag::invalid_param_for_destructor)
        << TypeName;
    NewFD->setInvalidDecl();
  }
}

void Sema::CheckBSCDestructorBody(FunctionDecl *NewFD) {
  if (NewFD->getBody() == nullptr) {
    Diag(NewFD->getLocation(), diag::err_owned_struct_destructor_body);
    NewFD->setInvalidDecl();
  }
}

// Collect the destructor map of non-instantiated functions.
void Sema::CollectDestructMap(StmtResult Res, Scope *BeginScope,
                              Scope *EndScope) {
  if (!getLangOpts().BSC || Res.isInvalid() || !Res.get())
    return;
  llvm::SmallVector<VarDecl *> MovedDecls;
  if (isa<ReturnStmt>(Res.get())) {
    ReturnStmt *Value = cast<ReturnStmt>(Res.get());
    // should find var
    DeclRefFinder Finder = DeclRefFinder(*this);
    Finder.TraverseStmt(Value);
    MovedDecls = Finder.MovedDecls;
  }
  SmallVector<VarDecl *> ReturnStmts;
  while (BeginScope != EndScope->getParent()) {
    std::stack<VarDecl *> VarDeclStack =
        BeginScope->DeclsInScopeToEmitDestructorCall;
    while (!VarDeclStack.empty()) {
      VarDecl *D = VarDeclStack.top();
      bool IsTopLevelSwitchBlock =
          BeginScope->getParent()->getFlags() & Scope::SwitchScope;
      VarDeclStack.pop();
      bool IsDestructorParam = false;
      if (auto MD = dyn_cast<BSCMethodDecl>(getCurFunctionOrMethodDecl())) {
        IsDestructorParam = MD->isDestructor() && isa<ParmVarDecl>(D);
      }
      if ((std::find(MovedDecls.begin(), MovedDecls.end(), D) ==
           MovedDecls.end()) &&
          !IsTopLevelSwitchBlock && !IsDestructorParam) {
        ReturnStmts.push_back(D);
      }
    }
    BeginScope = BeginScope->getParent();
  }

  if (ReturnStmts.size() != 0) {
    Context.DestructMap[getCurFunctionOrMethodDecl()][Res.get()] = ReturnStmts;
  }
}

// Collect destructor map during function instantiation,the destructor map of
// non-instantiated functions has already been collected in the parser phase.
void Sema::CollDestructMapInFuncInstantiation(FunctionDecl *FD) {
  if (FD->getTemplatedKind() == FunctionDecl::TK_NonTemplate)
    return;
  ConstructorCompound CC(*this);
  CC.TraverseFunctionDecl(FD);

  CalcDestructorMapForIns CalcDestructorMapForInsObj(*this);
  CalcDestructorMapForInsObj.TraverseFunctionDecl(FD);
}

#endif