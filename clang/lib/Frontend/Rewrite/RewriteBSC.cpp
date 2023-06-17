//===--- RewriteBSC.cpp - Playground for the code rewriter ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Hacks and fun related to the code rewriter.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/ASTConsumers.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {
class RewriteBSC : public ASTConsumer {
private:
  Rewriter Rewrite;
  DiagnosticsEngine &Diags;
  const LangOptions &LangOpts;
  ASTContext *Context;
  SourceManager *SM;
  TranslationUnitDecl *TUDecl;
  FileID MainFileID;
  const char *MainFileStart, *MainFileEnd;
  std::string InFileName;
  std::unique_ptr<raw_ostream> OutFile;

  unsigned RewriteFailedDiag;

public:
  RewriteBSC(std::string inFile, std::unique_ptr<raw_ostream> OS,
             DiagnosticsEngine &D, const LangOptions &LOpts);
  ~RewriteBSC() override {}

  void Initialize(ASTContext &C) override;

  void HandleTranslationUnit(ASTContext &C) override;
  bool HandleTopLevelDecl(DeclGroupRef D) override {
    for (DeclGroupRef::iterator I = D.begin(), E = D.end(); I != E; ++I) {
      HandleTopLevelSingleDecl(*I);
    }
    return true;
  }
  void HandleTopLevelSingleDecl(Decl *D);
  void HandleDeclInMainFile(Decl *D);
  void HandleTagDeclDefinition(TagDecl *D) override {
    HandleTopLevelSingleDecl(D);
  }

  void RewriteBSCMethodDecl(BSCMethodDecl *BMD);
  void RewriteMemberExpr(MemberExpr *ME);
  void RewriteCallExpr(CallExpr *CE);
  void RewriteDeclRefExpr(DeclRefExpr *CE);

  void RewriteVarDecl(VarDecl *VD);
  void RwriteBSCFunctionTemplateDecl(FunctionTemplateDecl *FTD);
  void RewriteBSCInstantialFunctionDecl(FunctionDecl *FD);
  void RwriteBSCClassTemplateDecl(ClassTemplateDecl *FTD);
  void RwriteBSCInstantialClassDecl(ClassTemplateSpecializationDecl *FD);

  void InsertText(SourceLocation Loc, StringRef Str, bool InsertAfter = true) {
    // If insertion succeeded or warning disabled return with no warning.
    if (!Rewrite.InsertText(Loc, Str, InsertAfter))
      return;

    Diags.Report(Context->getFullLoc(Loc), RewriteFailedDiag);
  }

  void ReplaceText(SourceLocation Start, unsigned OrigLength, StringRef Str) {
    // If removal succeeded or warning disabled return with no warning.
    if (!Rewrite.ReplaceText(Start, OrigLength, Str))
      return;

    Diags.Report(Context->getFullLoc(Start), RewriteFailedDiag);
  }

  void ReplaceText(SourceLocation Start, SourceRange range, StringRef NewStr) {
    // If removal succeeded or warning disabled return with no warning.
    if (!Rewrite.ReplaceText(range, NewStr))
      return;

    Diags.Report(Context->getFullLoc(Start), RewriteFailedDiag);
  }

  void RemoveText(SourceLocation Start, SourceRange range) {
    // Remove the specified text region.
    if (!Rewrite.RemoveText(range))
      return;

    Diags.Report(Context->getFullLoc(Start), RewriteFailedDiag);
  }
};

class RewriterVisitor : public StmtVisitor<RewriterVisitor> {
public:
  RewriterVisitor(RewriteBSC *rewriter) : rewriter(rewriter) {}
  RewriteBSC *rewriter;

  void VisitMemberExpr(MemberExpr *E) {
    rewriter->RewriteMemberExpr(E);
    VisitExpr(E);
  }

  void VisitCallExpr(CallExpr *E) {
    VisitExpr(E);
    rewriter->RewriteCallExpr(E);
  }

  void VisitDeclRefExpr(DeclRefExpr *DRE) {
    VisitExpr(DRE);
    rewriter->RewriteDeclRefExpr(DRE);
  }

  void VisitDeclStmt(DeclStmt *DT) {
    VisitStmt(DT);
    for (auto *D : DT->decls()) {
      if (VarDecl *VD = cast<VarDecl>(D))
        rewriter->RewriteVarDecl(VD);
    }
  }

  void VisitStmt(Stmt *S) {
    for (auto *C : S->children()) {
      if (C)
        Visit(C);
    }
  }
};
} // end anonymous namespace

RewriteBSC::RewriteBSC(std::string inFile, std::unique_ptr<raw_ostream> OS,
                       DiagnosticsEngine &D, const LangOptions &LOpts)
    : Diags(D), LangOpts(LOpts), InFileName(inFile), OutFile(std::move(OS)) {
  RewriteFailedDiag = Diags.getCustomDiagID(
      DiagnosticsEngine::Warning,
      "rewriting sub-expression within a macro (may not be correct)");
}

std::unique_ptr<ASTConsumer>
clang::CreateBSCRewriter(const std::string &InFile,
                         std::unique_ptr<raw_ostream> OS,
                         DiagnosticsEngine &Diags, const LangOptions &LOpts) {
  return std::make_unique<RewriteBSC>(InFile, std::move(OS), Diags, LOpts);
}

void RewriteBSC::Initialize(ASTContext &context) {
  Context = &context;
  SM = &Context->getSourceManager();
  TUDecl = Context->getTranslationUnitDecl();

  // Get the ID and start/end of the main file.
  MainFileID = SM->getMainFileID();
  llvm::MemoryBufferRef MainBuf = SM->getBufferOrFake(MainFileID);
  MainFileStart = MainBuf.getBufferStart();
  MainFileEnd = MainBuf.getBufferEnd();

  Rewrite.setSourceMgr(Context->getSourceManager(), Context->getLangOpts());
}

void RewriteBSC::HandleTopLevelSingleDecl(Decl *D) {
  if (Diags.hasErrorOccurred())
    return;

  SourceLocation Loc = D->getLocation();
  Loc = SM->getExpansionLoc(Loc);

  // If this is for a builtin, ignore it.
  if (Loc.isInvalid())
    return;

  // // If we have a decl in the main file, see if we should rewrite it.
  if (SM->isWrittenInMainFile(Loc))
    return HandleDeclInMainFile(D);
}

void RewriteBSC::HandleDeclInMainFile(Decl *D) {
  switch (D->getKind()) {
  case Decl::BSCMethod:
  case Decl::Function: {
    if (BSCMethodDecl *BSCMD = dyn_cast<BSCMethodDecl>(D)) {
      RewriteBSCMethodDecl(BSCMD);
    }
    FunctionDecl *FD = cast<FunctionDecl>(D);
    if (!FD->isThisDeclarationADefinition())
      break;
    // Rewrite BSC instantial template function
    if (FD->isTemplateInstantiation()) {
      RewriteBSCInstantialFunctionDecl(FD);
    } else {
      if (CompoundStmt *Body = dyn_cast_or_null<CompoundStmt>(FD->getBody())) {
        RewriterVisitor visitor(this);
        visitor.VisitStmt(Body);
      }
    }
    break;
  }
  case Decl::FunctionTemplate: {
    FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(D);
    // Remove origin BSC template function.
    RwriteBSCFunctionTemplateDecl(FTD);
    break;
  }
  case Decl::ClassTemplate: {
    ClassTemplateDecl *CTD = cast<ClassTemplateDecl>(D);
    // Remove origin BSC template struct.
    RwriteBSCClassTemplateDecl(CTD);
    break;
  }
  case Decl::ClassTemplateSpecialization: {
    // Rewrite ClassTemplateSpecializationDecl
    ClassTemplateSpecializationDecl *CTSD = cast<ClassTemplateSpecializationDecl>(D);
    RwriteBSCInstantialClassDecl(CTSD);
    break;
  }
  default:
    break;
  }
}

void RewriteBSC::HandleTranslationUnit(ASTContext &C) {
  if (Diags.hasErrorOccurred())
    return;

  if (const RewriteBuffer *RewriteBuf =
          Rewrite.getRewriteBufferFor(MainFileID)) {
    *OutFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
  } else {
    // No change.
    *OutFile << std::string(MainFileStart, MainFileEnd);
  }
  OutFile->flush();
}

// To prefix the type by jointing '_' between types and function name.
// Arg 'isFront' determines weather to prefix '_' at the front of type or not.
static std::string GetTpyoPrefix(QualType T, bool isFront) {
  std::string ExtendedTypeStr = T.getAsString();
  for (int i = ExtendedTypeStr.length() - 1; i >= 0; i--) {
    if (ExtendedTypeStr[i] == ' ') {
      ExtendedTypeStr.replace(i, 1, "_");
    }
  }
  if (isFront) {
    ExtendedTypeStr = "_" + ExtendedTypeStr;
  } else {
    ExtendedTypeStr += "_";
  }
  return ExtendedTypeStr;
}

void RewriteBSC::RewriteBSCMethodDecl(BSCMethodDecl *BMD) {
  SourceLocation StartLoc = BMD->getExtentedTypeBeginLoc();

  DeclarationNameInfo DNI = BMD->getNameInfo();
  SourceLocation EndLoc = DNI.getBeginLoc();

  const char *startBuf = SM->getCharacterData(StartLoc);
  const char *endBuf = SM->getCharacterData(EndLoc);

  std::string ExtendedTypeStr = GetTpyoPrefix(BMD->getExtendedType(),
                                              /*isFront=*/false);
  ReplaceText(StartLoc, endBuf - startBuf, ExtendedTypeStr);
}

void RewriteBSC::RewriteCallExpr(CallExpr *CE) {
  Expr *Callee = CE->getCallee();
  if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(Callee)) {
    Expr *SE = ICE->getSubExpr();
    if (MemberExpr *Member = dyn_cast<MemberExpr>(SE)) {
      if (auto *BD = dyn_cast<BSCMethodDecl>(Member->getMemberDecl())) {
        std::string SStr;
        llvm::raw_string_ostream Buf(SStr);
        CE->getArg(0)->printPretty(Buf, nullptr, PrintingPolicy(LangOpts));
        const std::string &Str = Buf.str();
        SourceLocation StartLoc;
        if (CE->getNumArgs() == 1) {
          StartLoc = CE->getRParenLoc();
          InsertText(StartLoc, Str);
        } else {
          StartLoc = CE->getArg(1)->getBeginLoc();
          InsertText(StartLoc, Str + ", ");
        }
      }
    }
    if (DeclRefExpr *DeclRef = dyn_cast<DeclRefExpr>(SE)) {
      if (DeclRef->getFoundDecl()->isTemplateDecl()) {
        // 1.get repalce name
        auto *FD = dyn_cast<FunctionDecl>(DeclRef->getDecl());
        std::string FunctionNameStr = FD->getDeclName().getAsString();
        for (unsigned i = 0; i < FD->getTemplateSpecializationArgs()->size(); i++) {
          std::string QT = GetTpyoPrefix(FD->getTemplateSpecializationArgs()
                                         ->asArray()[i].getAsType(),
                                         /*isFront=*/true);
          FunctionNameStr += QT;
        }
        // 2.get locations to replace 
        unsigned TemplateArgNum = DeclRef->getNumTemplateArgs();
        SourceLocation LocStart = CE->getBeginLoc();
        SourceLocation LocEnd;
        // If template args '(T a)' is not zero, then use getBeginLoc() and 
        // shift to left for 1, so that we can get '>' location. If template
        // arg '()' is zero, then getBeginLoc() API is invalid， we can only 
        // find the location of ')', then shift to the left 
        // for 2 to get '<'.
        if (TemplateArgNum != 0) {
          LocEnd = CE->getArg(0)->getBeginLoc().getLocWithOffset(-1);
        } else {
          LocEnd = CE->getRParenLoc().getLocWithOffset(-2);
        }
        // 3.replace the func name
        const char *startBuf = SM->getCharacterData(LocStart);
        const char *endBuf = SM->getCharacterData(LocEnd);
        ReplaceText(LocStart, endBuf - startBuf, FunctionNameStr);
      }
    }
  }
}

void RewriteBSC::RewriteMemberExpr(MemberExpr *ME) {
  if (auto *BD = dyn_cast<BSCMethodDecl>(ME->getMemberDecl())) {
    SourceLocation LocStart = ME->getBeginLoc();
    SourceLocation MemberStart = ME->getMemberLoc();
    const char *startBuf = SM->getCharacterData(LocStart);
    const char *endBuf = SM->getCharacterData(MemberStart);
    std::string ExtendedTypeStr = GetTpyoPrefix(BD->getExtendedType(),
                                                /*isFront=*/false);
    ReplaceText(LocStart, endBuf - startBuf, ExtendedTypeStr);
  }
}

void RewriteBSC::RewriteDeclRefExpr(DeclRefExpr *DRE) {
  if (DRE->HasBSCScopeSpec) {
    if (auto *BD = dyn_cast<BSCMethodDecl>(DRE->getFoundDecl())) {
      SourceLocation LocStart = DRE->getExtendedTypeBeginLoc();
      SourceLocation LocEnd = DRE->getBeginLoc();
      const char *startBuf = SM->getCharacterData(LocStart);
      const char *endBuf = SM->getCharacterData(LocEnd);
      std::string ExtendedTypeStr = GetTpyoPrefix(BD->getExtendedType(),
                                                  /*isFront=*/false);
      ReplaceText(LocStart, endBuf - startBuf, ExtendedTypeStr);
    }
  }
}

void RewriteBSC::RewriteVarDecl(VarDecl *VD) {
  QualType T = VD->getTypeSourceInfo()
    ? VD->getTypeSourceInfo()->getType()
    : VD->getASTContext().getUnqualifiedObjCPointerType(VD->getType());

  // rewrite BSC template struct instantial statement.
  if (const auto *ET = dyn_cast<ElaboratedType>(T)) {
    QualType QT = ET->getNamedType();
    if (const auto *TST = dyn_cast<TemplateSpecializationType>(QT)) {
      std::string SStr;
      llvm::raw_string_ostream Buf(SStr);
      VD->print(Buf, PrintingPolicy(LangOpts), /*PrintInstantiation=*/true);
      const std::string &Str = Buf.str();
      
      SourceLocation StartLoc = VD->getBeginLoc();
      SourceRange VDRange = VD->getSourceRange();
      ReplaceText(StartLoc, VDRange, Str);
    }
  }
}

void RewriteBSC::RwriteBSCFunctionTemplateDecl(FunctionTemplateDecl *FTD) {
  // Remove the BSC template function source code;
  SourceRange range = FTD->getSourceRange();
  RemoveText(FTD->getBeginLoc(), range);
}

void RewriteBSC::RewriteBSCInstantialFunctionDecl(FunctionDecl *FD) {
  // Rename the BSC instantial function template decl, and insert it
  // at the end of the origin function template decl.
  SourceLocation EndLoc = FD->getEndLoc();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  FD->print(Buf, PrintingPolicy(LangOpts), /*PrintInstantiation=*/true);
  const std::string &Str = Buf.str();
  // Insert instantial function decl at the end of origin template function
  // shift 2, in case the origin template function is at the beginning of
  // source code.
  InsertText(EndLoc.getLocWithOffset(2), Str + "\n");
}

void RewriteBSC::RwriteBSCClassTemplateDecl(ClassTemplateDecl *CTD){
  // Remove the BSC template struct source code;
  SourceRange range = CTD->getSourceRange();
  // somehow, the range doesn`t include the end semi ';' of the 
  // struct, so we need to shift end loc to right for 1.
  range.setEnd(range.getEnd().getLocWithOffset(1));
  RemoveText(CTD->getBeginLoc(), range);
}

void RewriteBSC::RwriteBSCInstantialClassDecl(ClassTemplateSpecializationDecl *CTSD) {
  // Rename the BSC instantial struct template decl, and insert it
  // at the end of the origin struct template decl.
  SourceLocation EndLoc = CTSD->getEndLoc();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  CTSD->print(Buf, PrintingPolicy(LangOpts), /*PrintInstantiation=*/true);
  const std::string &Str = Buf.str();
  // Insert instantial struct decl at the end of origin template struct
  // shift 2, in case the origin template struct is at the beginning of
  // source code.
  InsertText(EndLoc.getLocWithOffset(2), Str + ";" + "\n");
}