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

  void RewriteBSCMethodDecl(FunctionDecl *BMD);
  void ReplaceDecl(Decl *D);
  void RemoveDecl(Decl *D);
  void RewriteBSCInstantialFunctionDecl(FunctionDecl *FD);
  void RewriteBSCClassTemplateDecl(ClassTemplateDecl *FTD);
  void RewriteBSCInstantialClassDecl(ClassTemplateSpecializationDecl *FD);

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
        RewrittenDecls.push_back(FD);
        break;
      }
    }
    if (FD->isAsyncSpecified()) {
      RewrittenDecls.push_back(FD);
      ReplaceDecl(FD);
      break;
    }
    if (FD->isTemplateInstantiation()) {
      RewriteBSCInstantialFunctionDecl(FD);
    } else {
      RewriteBSCMethodDecl(FD);
    }
    RewrittenDecls.push_back(FD);
    break;
  }
  case Decl::Record:
  case Decl::Var: {
    RewrittenDecls.push_back(D);
    ReplaceDecl(D);
    break;
  }
  case Decl::FunctionTemplate: {
    FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(D);
    // Remove origin BSC template function.
    RewrittenDecls.push_back(FTD);
    RemoveDecl(FTD);
    break;
  }
  case Decl::ClassTemplate: {
    ClassTemplateDecl *CTD = cast<ClassTemplateDecl>(D);
    // Remove origin BSC template struct.
    RewrittenDecls.push_back(CTD);
    RewriteBSCClassTemplateDecl(CTD);
    break;
  }
  case Decl::ClassTemplateSpecialization: {
    // Rewrite ClassTemplateSpecializationDecl
    ClassTemplateSpecializationDecl *CTSD =
        cast<ClassTemplateSpecializationDecl>(D);
    RewrittenDecls.push_back(CTSD);
    RewriteBSCInstantialClassDecl(CTSD);
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

void RewriteBSC::RewriteBSCMethodDecl(FunctionDecl *BMD) {
  SourceRange range = BMD->getSourceRange();
  RemoveText(BMD->getBeginLoc(), range);
  // Rename the BSC instantial function template decl, and insert it
  // at the end of the origin function template decl.
  SourceLocation StartLoc = BMD->getBeginLoc();

  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  BMD->print(Buf, Policy, /*PrintInstantiation=*/true);
  const std::string &Str = Buf.str();
  InsertText(StartLoc, Str);
}

void RewriteBSC::ReplaceDecl(Decl *D) {
  SourceRange range = D->getSourceRange();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  if (Context->BSCDesugaredMap.find(D) != Context->BSCDesugaredMap.end()) {
    for (auto &DesugaredDecl : Context->BSCDesugaredMap[D]) {
      DesugaredDecl->print(Buf, Policy, /*PrintInstantiation=*/true);
      if (!isa<FunctionDecl>(DesugaredDecl) ||
          (isa<FunctionDecl>(DesugaredDecl) && !DesugaredDecl->hasBody())) {
        Buf << ";\n";
      }
      Buf << "\n";
      RewrittenDecls.push_back(DesugaredDecl);
    }
  } else {
    D->print(Buf, Policy, /*PrintInstantiation=*/true);
  }
  const std::string &Str = Buf.str();
  ReplaceText(D->getBeginLoc(), range, Str);
}

void RewriteBSC::RemoveDecl(Decl *D) {
  SourceRange range = D->getSourceRange();
  RemoveText(D->getBeginLoc(), range);
}

void RewriteBSC::RewriteBSCInstantialFunctionDecl(FunctionDecl *FD) {
  // Rename the BSC instantial function template decl, and insert it
  // at the end of the origin function template decl.
  SourceLocation EndLoc = FD->getBeginLoc();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  FD->print(Buf, Policy, /*PrintInstantiation=*/true);
  const std::string &Str = Buf.str();
  // Insert instantial function decl at the end of origin template function
  // shift 2, in case the origin template function is at the beginning of
  // source code.
  InsertText(EndLoc, Str + "\n");
}

void RewriteBSC::RewriteBSCClassTemplateDecl(ClassTemplateDecl *CTD) {
  // Remove the BSC template struct source code;
  SourceRange range = CTD->getSourceRange();
  // somehow, the range doesn`t include the end semi ';' of the
  // struct, so we need to shift end loc to right for 1.
  range.setEnd(range.getEnd().getLocWithOffset(1));
  RemoveText(CTD->getBeginLoc(), range);
}

void RewriteBSC::RewriteBSCInstantialClassDecl(
    ClassTemplateSpecializationDecl *CTSD) {
  // Rename the BSC instantial struct template decl, and insert it
  // at the end of the origin struct template decl.
  SourceLocation EndLoc = CTSD->getEndLoc();
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  CTSD->print(Buf, Policy, /*PrintInstantiation=*/true);
  const std::string &Str = Buf.str();
  // Insert instantial struct decl at the end of origin template struct
  // shift 2, in case the origin template struct is at the beginning of
  // source code.
  InsertText(EndLoc.getLocWithOffset(2), Str + ";" + "\n");
}
