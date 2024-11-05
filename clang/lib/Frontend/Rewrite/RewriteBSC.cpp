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

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/BSC/WalkerBSC.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/ASTConsumers.h"
#include <set>
#include <unordered_set>

using namespace clang;

static unsigned ReturnArgIndexInDeclList(ClassTemplateSpecializationDecl *CTSD,
                                         const std::vector<Decl *> &DeclList) {
  const TemplateArgumentList &TAL = CTSD->getTemplateInstantiationArgs();
  unsigned InsertIndex = 0;
  for (Decl *D : DeclList) {
    for (unsigned int i = 0; i < TAL.size(); ++i) {
      TemplateArgument TA = TAL.get(i);
      if (TA.getKind() == TemplateArgument::Type &&
          (TA.getAsType()->getAsCXXRecordDecl() == (D) ||
           TA.getAsType()->getAsRecordDecl() == (D))) {
        return InsertIndex + 1;
      }
    }
    InsertIndex++;
  }
  return 0;
}

/// EndsWith - Check if a string ends with a given suffix.
static bool EndsWith(const std::string &str, const std::string &suffix) {
  if (str.length() >= suffix.length()) {
    return 0 ==
           str.compare(str.length() - suffix.length(), suffix.length(), suffix);
  }
  return false;
}

/// getInnerType - Get the inner type of a pointer type.
static QualType getInnerType(QualType QT) {
  QT = QT.IgnoreParens();
  while (QT->isPointerType())
    QT = QT->getPointeeType();
  return QT;
}

/// HasTemplateSpecType - Determine whether the QualType contains instantiated
/// generic type.
static bool
HasTemplateSpecType(QualType &QT,
                    std::unordered_set<Decl *> &HasTemplateSpecSet) {
  if (QT->getAs<TemplateSpecializationType>()) {
    return true;
  } else if (const RecordType *RT = QT->getAs<RecordType>()) {
    return HasTemplateSpecSet.count(RT->getDecl());
  }
  return false;
}

/// IsInStandardHeaderFile - Check whether a SourceLocation is in standard
/// header file.
static bool IsInStandardHeaderFile(const SourceManager *SM,
                                   const SourceLocation &Loc) {
  bool isInMainFile = SM->isWrittenInMainFile(SM->getSpellingLoc(Loc)) ||
                      SM->isWrittenInMainFile(SM->getExpansionLoc(Loc));
  bool isInHBSFile = SM->isWrittenInHBSFile(SM->getSpellingLoc(Loc)) ||
                     SM->isWrittenInHBSFile(SM->getExpansionLoc(Loc));
  return !isInMainFile && !isInHBSFile;
}

namespace {
class RewriteBSC : public ASTConsumer {
public:
  RewriteBSC(std::string inFile, std::unique_ptr<raw_ostream> OS,
             DiagnosticsEngine &D, const LangOptions &LOpts, Preprocessor &pp,
             bool insertLine);
  ~RewriteBSC() override {}

  void Initialize(ASTContext &C) override;

  void HandleTranslationUnit(ASTContext &C) override;

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
  Preprocessor &PP;
  // control whether to insert bsc code line info
  bool WithLine;
  clang::PrintingPolicy Policy;

  unsigned RewriteFailedDiag;

  llvm::SmallPtrSet<Decl *, 16> DeclsWithoutBSCFeature;

  // avoid repeatedly include of header files.
  std::set<std::string> IncludedHeaders;

  /// Save rewritten decls to aviod rewritting one decl twice.
  std::vector<Decl *> RewrittenDecls;

  // Save all rewritten texts.
  std::string SStr;
  llvm::raw_string_ostream Buf;

  void CollectIncludes();
  void CollectIncludesInFile(FileID FID);
  void FindDeclsWithoutBSCFeature();
  void PrintDebugLineInfo(Decl *D);
  std::set<FileID> RetrieveFileIDsFromIncludeStack(const Decl *D);
  void RewriteMacroDirectives();
  void RewriteDecls();
  void RewriteNonGenericType(std::vector<Decl *> &DeclList,
                             std::unordered_set<Decl *> &HasTemplateSpecSet);
  void
  RewriteInstantGenericType(std::vector<Decl *> &DeclList,
                            std::unordered_set<Decl *> &HasTemplateSpecSet);
  void
  RewriteInstantFunctionDecl(std::vector<Decl *> &DeclList,
                             std::set<Decl *> &RewritedFunctionDeclarationSet);
  void RewriteNonGenericFuncAndVar(std::vector<Decl *> &DeclList);
  void RewriteInstantFunctionDef(std::vector<Decl *> &DeclList);

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

  void RemoveText(SourceLocation Start, unsigned Length) {
    // Remove the specified text region.
    if (!Rewrite.RemoveText(Start, Length))
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
                       DiagnosticsEngine &D, const LangOptions &LOpts,
                       Preprocessor &pp, bool InsertLine)
    : Diags(D), LangOpts(LOpts), InFileName(inFile), OutFile(std::move(OS)),
      PP(pp), WithLine(InsertLine), Policy(LangOpts), Buf(SStr) {
  RewriteFailedDiag = Diags.getCustomDiagID(
      DiagnosticsEngine::Warning,
      "rewriting sub-expression within a macro (may not be correct)");
  Policy.adjustForRewritingBSC();
}

std::unique_ptr<ASTConsumer>
clang::CreateBSCRewriter(const std::string &InFile,
                         std::unique_ptr<raw_ostream> OS,
                         DiagnosticsEngine &Diags, const LangOptions &LOpts,
                         Preprocessor &PP, bool InsertLine) {
  return std::make_unique<RewriteBSC>(InFile, std::move(OS), Diags, LOpts, PP,
                                      InsertLine);
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

void RewriteBSC::HandleTranslationUnit(ASTContext &C) {
  // We only rewrite translation unit without any error.
  if (Diags.hasErrorOccurred())
    return;

  // First, clear the original main file contents for rewrite.
  RemoveText(SM->getLocForStartOfFile(MainFileID), MainFileEnd - MainFileStart);

  // Second, collect all include directives.
  CollectIncludes();

  // Third, rewrite all macro directives.
  RewriteMacroDirectives();

  // Fourth, find decls without bsc features and rewrite all decls int the
  // translation unit.
  FindDeclsWithoutBSCFeature();
  RewriteDecls();

  // Finally, insert rewritten file contents into the target C file.
  InsertText(SM->getLocForStartOfFile(MainFileID), Buf.str());

  // Write the written string to OutFile.
  if (const RewriteBuffer *RewriteBuf =
          Rewrite.getRewriteBufferFor(MainFileID)) {
    *OutFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
  } else {
    *OutFile << std::string(MainFileStart, MainFileEnd);
  }

  OutFile->flush();
}

/// CollectIncludes - Collect all include macro directives in hbs and cbs files.
void RewriteBSC::CollectIncludes() {
  // Collect include directives written in cbs file.
  CollectIncludesInFile(MainFileID);

  std::set<FileID> ProcessedFiles = {MainFileID};
  // Collect include directives written in hbs files which are included by cbs
  // file directly or indirectly.
  for (const Decl *D : TUDecl->decls()) {
    std::set<FileID> FIDs = RetrieveFileIDsFromIncludeStack(D);
    for (FileID FID : FIDs) {
      if (ProcessedFiles.find(FID) == ProcessedFiles.end()) {
        CollectIncludesInFile(FID);
        ProcessedFiles.insert(FID);
      }
    }
  }

  // Insert all the include directives into the output stream.
  for (auto IH : IncludedHeaders) {
    Buf << "#include " << IH << "\n";
  }
  Buf << "\n";
}

/// CollectIncludesInFile - Given a file id, traverse the file text and collect
/// include macro directives.
/// We don't collect include macro directive which includes .hbs file.
void RewriteBSC::CollectIncludesInFile(FileID FID) {
  if (FID.isInvalid())
    return;

  StringRef FileContents = SM->getBufferData(FID);
  size_t IncludeLen = strlen("include");
  bool InBlockComment = false;
  bool InSingleComment = false;
  // Loop over the whole file, looking for all includes.
  for (const char *it = FileContents.begin(), *ei = FileContents.end();
       it != ei; ++it) {
    if (*it == '\n') {
      InSingleComment = false;
    }
    if (*it == '/' && it + 1 != ei && *(it + 1) == '/') {
      InSingleComment = true;
    }
    if (*it == '/' && it + 1 != ei && *(it + 1) == '*') {
      InBlockComment = true;
    }
    if (*it == '*' && it + 1 != ei && *(it + 1) == '/') {
      InBlockComment = false;
    }
    if (*it == '#' && !InSingleComment && !InBlockComment) {
      if (++it == ei)
        return;
      // Skip spaces and tabs.
      while (*it == ' ' || *it == '\t') {
        if (++it == ei)
          return;
      }
      if (!strncmp(it, "include", IncludeLen)) {
        it += IncludeLen;
        if (it == ei)
          return;
        // Skip spaces and tabs.
        while (*it == ' ' || *it == '\t') {
          if (++it == ei)
            return;
        }
        // Match '"' or '<'.
        if (*it == '"' || *it == '<') {
          std::string Header;
          Header += *it;
          if (++it == ei)
            return;
          while (*it != '"' && *it != '>') {
            Header += *it;
            if (++it == ei)
              return;
          }
          Header += *it;
          // If the included file is a hbs file, we just ignore it.
          // Otherwise, we reserve it.
          if (!EndsWith(Header, ".hbs\"") && !EndsWith(Header, ".hbs>")) {
            IncludedHeaders.insert(Header);
          }
        }
      }
    }
  }
}

void RewriteBSC::FindDeclsWithoutBSCFeature() {
  BSCFeatureFinder finder = BSCFeatureFinder(*Context);
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(SM, D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Enum:
    case Decl::Function:
    case Decl::Record:
    case Decl::Var: {
      if (!finder.Visit(D)) {
        DeclsWithoutBSCFeature.insert(D);
        if (RecordDecl *RD = dyn_cast<RecordDecl>(D)) {
          if (TypedefNameDecl *TND = RD->getTypedefNameForAnonDecl()) {
            DeclsWithoutBSCFeature.insert(TND);
          }
        }
        if (EnumDecl *ED = dyn_cast<EnumDecl>(D)) {
          if (TypedefNameDecl *TND = ED->getTypedefNameForAnonDecl()) {
            DeclsWithoutBSCFeature.insert(TND);
          }
        }
      }
      break;
    }
    default:
      break;
    }
  }
}

/// PrintDebugLineInfo - Print debug line info for each declaration.
/// The debug info is composed of a `#line` macro directive, which contains
/// the original line number and file name of the start of the declaration.
void RewriteBSC::PrintDebugLineInfo(Decl *D) {
  if (!WithLine)
    return;
  // Get the original line number of the declaration.
  unsigned LineNo = SM->getSpellingLineNumber(D->getBeginLoc());
  // Insert `#line` macro directive into Buf.
  Buf << "#line " << LineNo << " \"" << SM->getBufferName(D->getBeginLoc())
      << "\"\n";
}

/// RetrieveFileIDsFromIncludeStack - For a given declaration, it has a include
/// stack. Because we want to collect include macro directives in hbs files,
/// we search the stack for hbs files, record them and return.
std::set<FileID> RewriteBSC::RetrieveFileIDsFromIncludeStack(const Decl *D) {
  // Get the FileID of D.
  FileID CurFileID = SM->getFileID(SM->getSpellingLoc(D->getLocation()));
  FileID PrevFileID = CurFileID;
  std::set<FileID> FIDs;

  if (CurFileID.isInvalid())
    return FIDs;

  while (true) {
    const FileEntry *CurFileEntry = SM->getFileEntryForID(CurFileID);
    if (!CurFileEntry)
      break;
    // Because hbs file can only be included by hbs file, c file and cbs file.
    if (CurFileEntry->getName().endswith(".cbs") ||
        CurFileEntry->getName().endswith(".c"))
      break;
    if (CurFileEntry->getName().endswith(".hbs")) {
      FIDs.insert(CurFileID);
    }

    SourceLocation IncludeLoc = SM->getIncludeLoc(CurFileID);
    if (IncludeLoc.isInvalid())
      break;

    PrevFileID = CurFileID;
    CurFileID = SM->getFileID(IncludeLoc);
  }

  return FIDs;
}

/// RewriteMacroDirectives - This is called for rewriting all macro directives
/// in the translation unit. We skip macro directives which is in the header
/// files with the suffix .h. Currently we only handle the \#define and \#undef
/// directives, and we only keep the latest definition. For other macro
/// directives such as \#if, \#ifndef, \#else, \#elif, \#endif, \#pragma,
/// \#error and \#line, we may support them in future version.
/// Note: We need to preserve macro directives as much as possible for
/// readability.
void RewriteBSC::RewriteMacroDirectives() {
  using id_macro_pair = std::pair<const IdentifierInfo *, MacroInfo *>;
  llvm::SetVector<id_macro_pair> MacrosSet;

  for (Preprocessor::macro_iterator I = PP.macro_begin(false),
                                    E = PP.macro_end(false);
       I != E; ++I) {
    MacroDirective *MD = I->second.getLatest();

    if (MD && MD->isDefined()) {
      MacroInfo *MI = MD->getMacroInfo();
      SourceLocation MacroLoc = MI->getDefinitionLoc();
      if (!IsInStandardHeaderFile(SM, MacroLoc) &&
          !MI->isUsedForHeaderGuard() &&
          !MacrosSet.count(id_macro_pair(I->first, MI))) {
        MacrosSet.insert(id_macro_pair(I->first, MI));
        const char *startBuf = SM->getCharacterData(MacroLoc);
        const char *endBuf = SM->getCharacterData(MI->getDefinitionEndLoc());
        if (!MI->tokens_empty()) {
          unsigned LenOfLastTok = MI->tokens().back().getLength();
          if (MD->getKind() == MacroDirective::MD_Define) {
            Buf << "#define ";
          } else {
            Buf << "#undef ";
          }
          Buf << std::string(startBuf, endBuf - startBuf + LenOfLastTok)
              << "\n";
        }
      }
    }
  }
  Buf << "\n";
}

/// RewriteDecls - This is called for rewriting all decls in the translation
/// unit. We skip declarations of spelling location in the header file with the
/// suffix .h. For other declarations, we seperate them into declarations with
/// bsc features and declarations without bsc features.
/// For declarations with bsc features, we use the original decl string.
/// For declarations without bsc features, we use pretty printer.
void RewriteBSC::RewriteDecls() {
  std::vector<Decl *> DeclList;
  std::unordered_set<Decl *> HasTemplateSpecSet;
  // avoid repeatedly rewrite of Template Functions.
  std::set<Decl *> RewritedFunctionDeclarationSet;

  // Step 1: Collect all non-generic struct/union/enum/typedef decls into
  // DeclList.
  RewriteNonGenericType(DeclList, HasTemplateSpecSet);

  // Step 2: Insert instantiated stuct/union decls into DeclList.
  RewriteInstantGenericType(DeclList, HasTemplateSpecSet);

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
    // For decls with bsc feature, we use pretty printer.
    // For decls without bsc feature, we use original string text.
    if (DeclsWithoutBSCFeature.find(D) == DeclsWithoutBSCFeature.end()) {
      D->print(Buf, Policy);
      Buf << ";\n\n";
    } else {
      if (auto *TD = dyn_cast<TagDecl>(D)) {
        // For an anonymous tagdecl with typedef, don't print the tagdecl,
        // because the corresponding typedef will print it.
        if (!TD->getTypedefNameForAnonDecl()) {
          const char *startBuf = SM->getCharacterData(D->getBeginLoc());
          const char *endBuf = SM->getCharacterData(D->getEndLoc());
          Buf << std::string(startBuf, endBuf - startBuf + 1);
          Buf << ";\n\n";
        }
      } else {
        if (TypedefNameDecl *TND = dyn_cast<TypedefNameDecl>(D)) {
          const char *startBuf = SM->getCharacterData(D->getBeginLoc());
          const char *endBuf =
              SM->getCharacterData(D->getEndLoc().getLocWithOffset(
                  TND->getIdentifier()->getLength()));
          Buf << std::string(startBuf, endBuf - startBuf + 1);
          Buf << "\n\n";
        } else {
          const char *startBuf = SM->getCharacterData(D->getBeginLoc());
          const char *endBuf = SM->getCharacterData(D->getEndLoc());
          Buf << std::string(startBuf, endBuf - startBuf + 1);
          Buf << ";\n\n";
        }
      }
    }
  }

  // Step 3: Collect all instatiation function declarations.
  Policy.FunctionDeclaraionOnly = true;
  RewriteInstantFunctionDecl(DeclList, RewritedFunctionDeclarationSet);

  // Step 4: Collect non-generic function definitions and var decls.
  Policy.FunctionDeclaraionOnly = false;
  RewriteNonGenericFuncAndVar(DeclList);

  // Step 5: Collect all instatiation function definition.
  RewriteInstantFunctionDef(DeclList);
}

void RewriteBSC::RewriteNonGenericType(
    std::vector<Decl *> &DeclList,
    std::unordered_set<Decl *> &HasTemplateSpecSet) {
  for (Decl *D : TUDecl->decls()) {
    // Skip declarations in header files with suffix .h.
    if (IsInStandardHeaderFile(SM, D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Enum: {
      DeclList.push_back(D);
      break;
    }
    case Decl::Record: {
      // If the RecordDecl has template specialization type, add it to the set.
      RecordDecl *RD = cast<RecordDecl>(D);
      for (FieldDecl *Field : RD->fields()) {
        QualType FieldTy = getInnerType(Field->getType());
        if (HasTemplateSpecType(FieldTy, HasTemplateSpecSet)) {
          HasTemplateSpecSet.insert(D);
          break;
        }
      }
      DeclList.push_back(D);
      break;
    }
    case Decl::TypeAlias:
    case Decl::Typedef: {
      TypedefNameDecl *TND = cast<TypedefNameDecl>(D);
      // For typedef of trait type, we don't need it.
      if (!TND->getUnderlyingType().getCanonicalType()->isTraitType())
        DeclList.push_back(D);
      break;
    }
    default:
      break;
    }
  }
}

void RewriteBSC::RewriteInstantGenericType(
    std::vector<Decl *> &DeclList,
    std::unordered_set<Decl *> &HasTemplateSpecSet) {
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CTD = cast<ClassTemplateDecl>(*D);
      // Skip incomplete generic struct decl.
      if (RecordDecl *RD = dyn_cast<RecordDecl>(CTD->getTemplatedDecl())) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      // Insert each specialization into correct place.
      for (ClassTemplateSpecializationDecl *CTSD : CTD->specializations()) {
        SourceLocation SL = CTSD->getPointOfInstantiation();
        bool Inserted = false;
        std::vector<Decl *>::iterator It = DeclList.begin();
        while (It != DeclList.end()) {
          if (SL.isInvalid())
            break;
          // Insert specialization to vector if current Decl relies on it.
          if (auto *TD = dyn_cast<ClassTemplateSpecializationDecl>(*It)) {
            auto &TAL = TD->getTemplateInstantiationArgs();
            for (unsigned int i = 0; i < TAL.size(); i++) {
              if (TAL.get(i).getKind() == TemplateArgument::Type &&
                  TAL.get(i).getAsType()->getAsCXXRecordDecl() == CTSD) {
                It = DeclList.insert(It, CTSD);
                Inserted = true;
                break;
              }
            }
            if (Inserted) {
              break;
            }
          }
          // Push it to back if a template argument type decl is in DeclList
          if (!SM->isWrittenInMainFile(SL)) {
            unsigned InsertIndex = ReturnArgIndexInDeclList(CTSD, DeclList);
            if (InsertIndex) {
              DeclList.insert(DeclList.begin() + InsertIndex, CTSD);
              Inserted = true;
              break;
            }
          }

          if (SM->isBeforeInTranslationUnit(SL, (*It)->getBeginLoc())) {
            It = DeclList.insert(It, CTSD);
            Inserted = true;
            break;
          }
          if (SM->isBeforeInTranslationUnit((*It)->getBeginLoc(), SL) &&
              SM->isBeforeInTranslationUnit(SL, (*It)->getEndLoc())) {
            It = DeclList.insert(It, CTSD);
            Inserted = true;
            break;
          }
          if (!SM->isBeforeInTranslationUnit((*It)->getBeginLoc(), SL) &&
              !SM->isBeforeInTranslationUnit(SL, (*It)->getBeginLoc())) {
            It = DeclList.insert(It, CTSD);
            Inserted = true;
            break;
          }
          It++;
        }
        if (Inserted == false) {
          DeclList.push_back(CTSD);
        }
      }
      break;
    }
    default:
      break;
    }
  }
}

void RewriteBSC::RewriteInstantFunctionDecl(
    std::vector<Decl *> &DeclList,
    std::set<Decl *> &RewritedFunctionDeclarationSet) {
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(SM, D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = cast<RecordDecl>(D);
      // For an owned struct type, print all member function decls.
      if (RD->isOwnedDecl()) {
        for (Decl *decl : RD->decls()) {
          if (isa<BSCMethodDecl>(decl)) {
            decl->print(Buf, Policy);
            Buf << ";\n\n";
          }
        }
      }
      break;
    }
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(D);
      // Don't print the declaration of a function of a generic class.
      // It will be printed when manipulating ClassTemplateDecl.
      if (isa_and_nonnull<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (FD->isTemplateInstantiation()) {
        FD->print(Buf, Policy);
        Buf << ";\n\n";
      }
      break;
    }
    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(D);
      for (FunctionDecl *FD : FTD->specializations()) {
        if (RewritedFunctionDeclarationSet.find(FD) ==
            RewritedFunctionDeclarationSet.end()) {
          RewritedFunctionDeclarationSet.insert(FD);
          FD->print(Buf, Policy);
          Buf << ";\n\n";
        }
      }
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CTD = cast<ClassTemplateDecl>(D);
      // Skip incomplete generic struct decl.
      if (RecordDecl *RD = dyn_cast<RecordDecl>(CTD->getTemplatedDecl())) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      for (ClassTemplateSpecializationDecl *CTSD : CTD->specializations()) {
        for (Decl *decl : CTSD->decls()) {
          if (isa<FunctionDecl>(decl) && decl->hasBody()) {
            decl->print(Buf, Policy);
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
}

void RewriteBSC::RewriteNonGenericFuncAndVar(std::vector<Decl *> &DeclList) {
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(SM, D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(D);
      // Don't print the declaration of a function of a generic class.
      // It will be printed when manipulating ClassTemplateDecl.
      if (isa_and_nonnull<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (!FD->isTemplateInstantiation()) {
        if (!SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getBeginLoc())) &&
            !SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getEndLoc())) &&
            FD->getStorageClass() == StorageClass::SC_Extern)
          break;

        // If it is BscMethod or macro expansion function, output the code
        // according to AST; Otherwise, simply retrieve the source code
        if (DeclsWithoutBSCFeature.find(FD) == DeclsWithoutBSCFeature.end() ||
            FD->getBeginLoc().isMacroID()) {
          PrintDebugLineInfo(FD);
          FD->print(Buf, Policy);
        } else {
          const char *startBuf = SM->getCharacterData(FD->getBeginLoc());
          const char *endBuf = SM->getCharacterData(FD->getEndLoc());
          if (startBuf == endBuf)
            break;
          Buf << std::string(startBuf, endBuf - startBuf + 1);
          if (FD->isThisDeclarationADefinition()) {
            Buf << "\n";
          }
        }

        if (!isa<FunctionDecl>(FD) || !FD->isThisDeclarationADefinition()) {
          Buf << ";\n";
        }
        Buf << "\n";
      }

      break;
    }
    case Decl::Var: {
      D->print(Buf, Policy);
      Buf << ";\n\n";
      break;
    }
    case Decl::FileScopeAsm: {
      const char *startBuf = SM->getCharacterData(D->getBeginLoc());
      const char *endBuf = SM->getCharacterData(D->getEndLoc());
      Buf << std::string(startBuf, endBuf - startBuf + 1);
      Buf << ";\n\n";
      break;
    }
    default:
      break;
    }
  }
}

void RewriteBSC::RewriteInstantFunctionDef(std::vector<Decl *> &DeclList) {
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(SM, D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = cast<RecordDecl>(D);
      if (RD->isOwnedDecl()) {
        for (Decl *decl : RD->decls()) {
          if (isa<BSCMethodDecl>(decl)) {
            FunctionDecl *FD = cast<FunctionDecl>(decl);
            if (FD->doesThisDeclarationHaveABody()) {
              PrintDebugLineInfo(FD);
              FD->print(Buf, Policy);
              Buf << "\n";
            }
          }
          if (isa<FunctionTemplateDecl>(decl)) {
            FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(decl);
            for (auto *DD : FTD->specializations()) {
              if (DD->doesThisDeclarationHaveABody()) {
                PrintDebugLineInfo(DD);
                DD->print(Buf, Policy);
                Buf << "\n";
              }
            }
          }
        }
      }
      break;
    }
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(D);
      // Don't print the declaration of a function of a generic class.
      // It will be printed when manipulating ClassTemplateDecl.
      if (isa_and_nonnull<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if (FD->isTemplateInstantiation()) {
        if (FD->doesThisDeclarationHaveABody()) {
          PrintDebugLineInfo(FD);
          FD->print(Buf, Policy);
          Buf << "\n";
        }
      }
      break;
    }
    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(D);
      if (FTD->isThisDeclarationADefinition()) {
        for (FunctionDecl *FD : FTD->specializations()) {
          if (FD->doesThisDeclarationHaveABody()) {
            PrintDebugLineInfo(FD);
            FD->print(Buf, Policy);
            Buf << "\n";
          }
        }
      }
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CT = cast<ClassTemplateDecl>(D);
      // Skip incomplete generic struct decl.
      if (RecordDecl *RD = dyn_cast<RecordDecl>(CT->getTemplatedDecl())) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      for (ClassTemplateSpecializationDecl *CTSD : CT->specializations()) {
        for (Decl *decl : CTSD->decls()) {
          if (isa<FunctionDecl>(decl)) {
            FunctionDecl *FD = cast<FunctionDecl>(decl);
            if (FD->doesThisDeclarationHaveABody()) {
              PrintDebugLineInfo(decl);
              decl->print(Buf, Policy);
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
}

#endif