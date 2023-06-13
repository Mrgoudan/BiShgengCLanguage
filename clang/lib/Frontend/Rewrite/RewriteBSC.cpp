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

  void RewriteBSCMethodDecl(BSCMethodDecl *BMD);
  void RewriteMemberExpr(MemberExpr *ME);
  void RewriteCallExpr(CallExpr *CE);
  void RewriteDeclRefExpr(DeclRefExpr *CE);

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
    if (CompoundStmt *Body = dyn_cast_or_null<CompoundStmt>(FD->getBody())) {
      RewriterVisitor visitor(this);
      visitor.VisitStmt(Body);
    }
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

std::string GetPrefix(QualType T) {
  std::string ExtendedTypeStr = T.getAsString();
  for (int i = ExtendedTypeStr.length() - 1; i >= 0; i--) {
    if (ExtendedTypeStr[i] == ' ') {
      ExtendedTypeStr.replace(i, 1, "_");
    }
  }
  ExtendedTypeStr += "_";
  return ExtendedTypeStr;
}

void RewriteBSC::RewriteBSCMethodDecl(BSCMethodDecl *BMD) {
  SourceLocation StartLoc = BMD->getExtentedTypeBeginLoc();

  DeclarationNameInfo DNI = BMD->getNameInfo();
  SourceLocation EndLoc = DNI.getBeginLoc();

  const char *startBuf = SM->getCharacterData(StartLoc);
  const char *endBuf = SM->getCharacterData(EndLoc);

  std::string ExtendedTypeStr = GetPrefix(BMD->getExtendedType());
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
  }
}

void RewriteBSC::RewriteMemberExpr(MemberExpr *ME) {
  if (auto *BD = dyn_cast<BSCMethodDecl>(ME->getMemberDecl())) {
    SourceLocation LocStart = ME->getBeginLoc();
    SourceLocation MemberStart = ME->getMemberLoc();
    const char *startBuf = SM->getCharacterData(LocStart);
    const char *endBuf = SM->getCharacterData(MemberStart);
    std::string ExtendedTypeStr = GetPrefix(BD->getExtendedType());
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
      std::string ExtendedTypeStr = GetPrefix(BD->getExtendedType());
      ReplaceText(LocStart, endBuf - startBuf, ExtendedTypeStr);
    }
  }
}
