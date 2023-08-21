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
  clang::PrintingPolicy Policy;

  unsigned RewriteFailedDiag;

  /// Save rewritten decls to aviod rewritting one decl twice.
  std::vector<Decl *> RewrittenDecls;

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

  void ReplaceDecl(Decl *D);
  void RemoveDecl(Decl *D);
  void InsertDecl(Decl *D);

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
} // namespace

RewriteBSC::RewriteBSC(std::string inFile, std::unique_ptr<raw_ostream> OS,
                       DiagnosticsEngine &D, const LangOptions &LOpts)
    : Diags(D), LangOpts(LOpts), InFileName(inFile), OutFile(std::move(OS)),
      Policy(LangOpts) {
  RewriteFailedDiag = Diags.getCustomDiagID(
      DiagnosticsEngine::Warning,
      "rewriting sub-expression within a macro (may not be correct)");
  Policy.adjustForRewritingBSC();
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

  // If we have a decl in the main file, see if we should rewrite it.
  if (SM->isWrittenInMainFile(Loc))
    return HandleDeclInMainFile(D);
}

void RewriteBSC::HandleDeclInMainFile(Decl *D) {
  if (std::find(RewrittenDecls.begin(), RewrittenDecls.end(), D) !=
      RewrittenDecls.end()) {
    return;
  }
  switch (D->getKind()) {
  case Decl::Enum:
  case Decl::Typedef:
    break;
  case Decl::BSCMethod:
  case Decl::Function: {
    FunctionDecl *FD = cast<FunctionDecl>(D);
    if (FD->getParent() && isa<RecordDecl>(FD->getParent())) {
      if (cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        // Remove origin generic BSC member function.
        RemoveDecl(FD);
        break;
      }
    }
    if (FD->isAsyncSpecified()) {
      ReplaceDecl(FD);
      break;
    }
    if (FD->isTemplateInstantiation()) {
      InsertDecl(FD);
    } else {
      ReplaceDecl(FD);
    }
    break;
  }
  case Decl::Record:
  case Decl::Var: {
    ReplaceDecl(D);
    break;
  }
  case Decl::FunctionTemplate:
  case Decl::ClassTemplate: {
    RemoveDecl(D);
    break;
  }
  case Decl::ClassTemplateSpecialization: {
    InsertDecl(D);
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

void RewriteBSC::ReplaceDecl(Decl *D) {
  RewrittenDecls.push_back(D);

  SourceRange Range = D->getSourceRange();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  if (Context->BSCDesugaredMap.find(D) != Context->BSCDesugaredMap.end()) {
    for (auto &DesugaredDecl : Context->BSCDesugaredMap[D]) {
      DesugaredDecl->print(Buf, Policy, /*PrintInstantiation=*/true);
      if (!isa<FunctionDecl>(DesugaredDecl) || !DesugaredDecl->hasBody()) {
        Buf << ";\n";
      }
      Buf << "\n";
      RewrittenDecls.push_back(DesugaredDecl);
    }
  } else {
    D->print(Buf, Policy, /*PrintInstantiation=*/true);
  }
  const std::string &Str = Buf.str();
  ReplaceText(D->getBeginLoc(), Range, Str);
}

void RewriteBSC::RemoveDecl(Decl *D) {
  RewrittenDecls.push_back(D);

  SourceRange Range = D->getSourceRange();
  if (isa<ClassTemplateDecl>(D)) {
    // Somehow, the range doesn`t include the end semi ';' of the
    // struct, so we need to shift end loc to right for 1.
    Range.setEnd(Range.getEnd().getLocWithOffset(1));
  }
  RemoveText(D->getBeginLoc(), Range);
}

void RewriteBSC::InsertDecl(Decl *D) {
  RewrittenDecls.push_back(D);

  SourceLocation BLoc = D->getBeginLoc();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  D->print(Buf, Policy, /*PrintInstantiation=*/true);

  if (!isa<FunctionDecl>(D) || !D->hasBody()) {
    Buf << ";\n";
  }
  Buf << "\n";
  const std::string &Str = Buf.str();
  InsertText(BLoc, Str);
}
