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

#if ENABLE_BSC

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

  SourceLocation BeginLocOfFirstDecl;
  SourceLocation EndLocOfLastDecl;

  /// Save rewritten decls to aviod rewritting one decl twice.
  std::vector<Decl *> RewrittenDecls;

public:
  RewriteBSC(std::string inFile, std::unique_ptr<raw_ostream> OS,
             DiagnosticsEngine &D, const LangOptions &LOpts);
  ~RewriteBSC() override {}

  void Initialize(ASTContext &C) override;

  void HandleTranslationUnit(ASTContext &C) override;

private:
  const std::string GetRewrittenString();
  /// Get the begin source location of the first decl and the end source
  /// location of the last decl.
  void GetSourceLocationsOfFirstDeclAndLastDecl();

  /// Rewrite include directive form `#include "xxx.hbs"` to `#include "xxx.h"`.
  void RewriteInclude();
  void RewriteDecls(DeclContext *DC);

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

void RewriteBSC::RewriteInclude() {
  SourceLocation LocStart = SM->getLocForStartOfFile(MainFileID);
  StringRef MainBuf = SM->getBufferData(MainFileID);
  const char *MainBufStart = MainBuf.begin();
  const char *MainBufEnd = MainBuf.end();
  size_t IncludeLen = strlen("include");
  auto EndsWith = [](const std::string &str, const std::string &suffix) {
    if (str.length() >= suffix.length()) {
      return 0 == str.compare(str.length() - suffix.length(), suffix.length(),
                              suffix);
    }
    return false;
  };
  // Loop over the whole file, looking for includes.
  for (const char *BufPtr = MainBufStart; BufPtr < MainBufEnd; ++BufPtr) {
    if (*BufPtr == '#') {
      if (++BufPtr == MainBufEnd)
        return;
      while (*BufPtr == ' ' || *BufPtr == '\t')
        if (++BufPtr == MainBufEnd)
          return;
      if (!strncmp(BufPtr, "include", IncludeLen)) {
        BufPtr += IncludeLen;
        if (++BufPtr == MainBufEnd)
          return;
        while (*BufPtr == ' ' || *BufPtr == '\t')
          if (++BufPtr == MainBufEnd)
            return;
        if (*BufPtr == '"') {
          if (++BufPtr == MainBufEnd)
            return;
          std::string Buf = "";
          SourceLocation StartLoc =
              LocStart.getLocWithOffset(BufPtr - MainBufStart);
          while (*BufPtr != '"') {
            Buf += *BufPtr;
            if (++BufPtr == MainBufEnd)
              return;
          }
          if (EndsWith(Buf, "hbs")) {
            ReplaceText(StartLoc.getLocWithOffset(Buf.length() - 3), 3, "h");
          }
        }
      }
    }
  }
}

void RewriteBSC::HandleTranslationUnit(ASTContext &C) {
  if (Diags.hasErrorOccurred())
    return;

  RewriteInclude(); // Rewrite include directives.

  RewriteDecls(C.getTranslationUnitDecl()); // Rewrite all decls.

  if (const RewriteBuffer *RewriteBuf =
          Rewrite.getRewriteBufferFor(MainFileID)) {
    *OutFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
  } else {
    // No change.
    *OutFile << std::string(MainFileStart, MainFileEnd);
  }
  OutFile->flush();
}

void RewriteBSC::RewriteDecls(DeclContext *DC) {

  GetSourceLocationsOfFirstDeclAndLastDecl();

  // Step 1: Get rewritten string for all decls using pretty printer.
  const std::string &Str = GetRewrittenString();

  // Step 2: Replace buffered string with original decl string.
  const char *startBuf = SM->getCharacterData(BeginLocOfFirstDecl);
  const char *endBuf = SM->getCharacterData(EndLocOfLastDecl);
  ReplaceText(BeginLocOfFirstDecl, endBuf - startBuf + 1, Str);
}

static bool IsDesugared(Decl *D, ASTContext *Context) {
  for (auto DD : Context->BSCDesugaredMap) {
    for (auto &DesugaredDecl : DD.second) {
      if (isa<NamedDecl>(D)) {
        NamedDecl *ND = cast<NamedDecl>(D);
        if (ND == DesugaredDecl) {
          return true;
        }
      }
    }
  }
  return false;
}

void RewriteBSC::GetSourceLocationsOfFirstDeclAndLastDecl() {
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    if (IsDesugared(*D, Context)) {
      continue;
    }

    CharSourceRange CSRange = SM->getExpansionRange((*D)->getSourceRange());
    SourceRange Range = CSRange.getAsRange();
    SourceLocation StartLoc = Range.getBegin();
    SourceLocation EndLoc = Range.getEnd();
    if (isa<ClassTemplateDecl>(*D) || isa<RecordDecl>(*D) ||
        isa<EnumDecl>(*D) || isa<VarDecl>(*D) ||
        (isa<FunctionDecl>(*D) &&
         !cast<FunctionDecl>(*D)->isThisDeclarationADefinition())) {
      // For several decls, the range doesn`t include the end semi ';' of the
      // struct, so we need to shift end loc to right for 1.
      // Note: here we make an assumption that there is no space between
      // semicolon and decls.
      EndLoc = EndLoc.getLocWithOffset(1);
    }

    if (SM->isWrittenInMainFile(StartLoc)) {
      if (BeginLocOfFirstDecl.isInvalid()) {
        BeginLocOfFirstDecl = StartLoc;
      } else if (SM->isBeforeInTranslationUnit(StartLoc, BeginLocOfFirstDecl)) {
        BeginLocOfFirstDecl = StartLoc;
      }
    }

    if (SM->isWrittenInMainFile(EndLoc)) {
      if (EndLocOfLastDecl.isInvalid()) {
        EndLocOfLastDecl = EndLoc;
      } else if (SM->isBeforeInTranslationUnit(EndLocOfLastDecl, EndLoc)) {
        EndLocOfLastDecl = EndLoc;
      }
    }
  }
}

const std::string RewriteBSC::GetRewrittenString() {
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);

  std::vector<Decl *> DeclList;

  // Step 1: Sort all struct/union/enum/typedef decls.
  // Step 1.1: Put non-generic struct / union / enum / typedef decls into
  // DeclList.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::Record:
    case Decl::Enum:
    case Decl::Typedef: {
      if (SM->isWrittenInMainFile(SM->getExpansionLoc(D->getBeginLoc()))) {
        DeclList.push_back(*D);
      }
      break;
    }
    default:
      break;
    }
  }

  // Step 1.2: Insert instantiated stuct / enum decls into DeclList.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CT = cast<ClassTemplateDecl>(*D);
      if (auto RD = dyn_cast<RecordDecl>(CT->getTemplatedDecl())) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      for (auto *DD : CT->specializations()) {
        SourceLocation SL = DD->getPointOfInstantiation();
        std::vector<Decl *>::iterator It = DeclList.begin();
        bool Inserted = false;
        while (It != DeclList.end()) {
          if (!SL.isInvalid()) {
            if (SM->isBeforeInTranslationUnit(SL, (*It)->getBeginLoc())) {
              It = DeclList.insert(It, DD);
              Inserted = true;
              break;
            } else if (SM->isBeforeInTranslationUnit((*It)->getBeginLoc(),
                                                     SL) &&
                       SM->isBeforeInTranslationUnit(SL, (*It)->getEndLoc())) {
              It = DeclList.insert(It, DD);
              Inserted = true;
              break;
            } else if (!SM->isBeforeInTranslationUnit((*It)->getBeginLoc(),
                                                      SL) &&
                       !SM->isBeforeInTranslationUnit(SL,
                                                      (*It)->getBeginLoc())) {
              It = DeclList.insert(It, DD);
              Inserted = true;
              break;
            }
          }
          It++;
        }
        if (Inserted == false) {
          DeclList.push_back(DD);
        }
      }
      break;
    }
    default:
      break;
    }
  }
  // For struct Void which is desugared, we have to handle it specially.
  Decl *VoidStruct = nullptr;
  for (Decl *D : DeclList) {
    if (isa<RecordDecl>(D)) {
      RecordDecl *RD = cast<RecordDecl>(D);
      if (RD->getNameAsString() == "Void") {
        for (auto DD : Context->BSCDesugaredMap) {
          for (auto &DesugaredDecl : DD.second) {
            if (RD == DesugaredDecl) {
              VoidStruct = RD;
              break;
            }
          }
        }
      }
    }
  }

  if (VoidStruct) {
    VoidStruct->print(Buf, Policy);
    Buf << ";\n\n";
  }

  for (Decl *D : DeclList) {
    if (D == VoidStruct) {
      continue;
    }
    D->print(Buf, Policy);
    Buf << ";\n\n";
  }

  // Step 2: Collect all instatiation function declarations.
  Policy.FunctionDeclaraionOnly = true;

  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(*D);
      if (FD->getParent() && isa<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (SM->isWrittenInMainFile(SM->getExpansionLoc(FD->getBeginLoc())) &&
          FD->isTemplateInstantiation()) {
        FD->print(Buf, Policy);
        Buf << ";\n\n";
      }

      break;
    }
    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(*D);
      for (auto *DD : FTD->specializations()) {
        DD->print(Buf, Policy);
        Buf << ";\n\n";
      }
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CT = cast<ClassTemplateDecl>(*D);
      for (auto *DD : CT->specializations()) {
        for (auto *DDD : DD->decls()) {
          if (isa<FunctionDecl>(DDD)) {
            DDD->print(Buf, Policy);
            Buf << ";\n\n";
          }
        }
      }
      break;
    }
    default: {
      break;
    }
    }
  }

  Policy.FunctionDeclaraionOnly = false;

  // Step 3: Collect non-generic function definitions and var decls.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(*D);
      if (FD->getParent() && isa<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (SM->isWrittenInMainFile(SM->getExpansionLoc(FD->getBeginLoc())) &&
          !FD->isTemplateInstantiation()) {
        FD->print(Buf, Policy);
        if (!isa<FunctionDecl>(FD) || !FD->isThisDeclarationADefinition()) {
          Buf << ";\n";
        }
        Buf << "\n";
      }

      break;
    }
    case Decl::Var:
    case Decl::FileScopeAsm: {
      if (SM->isWrittenInMainFile(SM->getExpansionLoc(D->getBeginLoc()))) {
        D->print(Buf, Policy);
        Buf << ";\n\n";
      }
      break;
    }
    default:
      break;
    }
  }

  // Step 4: Collect all instatiation function definition.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(*D);
      if (FD->getParent() && isa<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (SM->isWrittenInMainFile(SM->getExpansionLoc(FD->getBeginLoc())) &&
          FD->isTemplateInstantiation()) {
        if (FD->doesThisDeclarationHaveABody()) {
          FD->print(Buf, Policy);
          Buf << "\n";
        }
      }
      break;
    }

    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(*D);
      for (auto *DD : FTD->specializations()) {
        if (DD->doesThisDeclarationHaveABody()) {
          DD->print(Buf, Policy);
          Buf << "\n";
        }
      }
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CT = cast<ClassTemplateDecl>(*D);
      if (auto RD = dyn_cast<RecordDecl>(CT->getTemplatedDecl())) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      for (auto *DD : CT->specializations()) {
        for (auto *DDD : DD->decls()) {
          if (isa<FunctionDecl>(DDD)) {
            FunctionDecl *FDDD = cast<FunctionDecl>(DDD);
            if (FDDD->doesThisDeclarationHaveABody()) {
              DDD->print(Buf, Policy);
              Buf << "\n";
            }
          }
        }
      }
      break;
    }
    default:
      break;
    }
  }
  return Buf.str();
}

#endif