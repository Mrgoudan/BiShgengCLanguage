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
#include <queue>
#include <set>
#include <unordered_set>

using namespace clang;

namespace {

//----------------- Topological sorting of type definitions -----------------//
class TypeDependencyGraph {
public:
  void addDependency(RecordDecl *type, RecordDecl *dependency) {
    typeDependencyMap[type].insert(dependency);
    addNode(type);
    addNode(dependency);
  }

  // recording appear index
  void addNode(RecordDecl *RD) {
    nodes.insert({RD, nodes.size()});
  } 

  std::vector<RecordDecl *> topologicalSort() {
    std::map<RecordDecl *, int> inDegree;
    for (auto &entry : nodes) {
      RecordDecl *node = entry.first;
      inDegree[node] = 0;
    }
    for (auto &entry : typeDependencyMap) {
      for (RecordDecl *dependency : entry.second) {
        inDegree[dependency]++;
      }
    }

    auto cmp = [this](RecordDecl *A, RecordDecl *B) {
      return nodes.at(A) > nodes.at(B);
    };

    std::priority_queue<RecordDecl *, std::vector<RecordDecl *>, decltype(cmp)>
        zeroInDegree(cmp);

    for (auto &entry : inDegree) {
      if (entry.second == 0) {
        zeroInDegree.push(entry.first);
      }
    }

    std::vector<RecordDecl *> sorted;
    while (!zeroInDegree.empty()) {
      RecordDecl *current = zeroInDegree.top();
      zeroInDegree.pop();
      sorted.push_back(current);

      for (RecordDecl *neighbor : typeDependencyMap[current]) {
        inDegree[neighbor]--;
        if (inDegree[neighbor] == 0) {
          zeroInDegree.push(neighbor);
        }
      }
    }

    assert(sorted.size() == nodes.size() &&
           "There cannot be circular between types.");

    return sorted;
  }

private:
  // @code
  // struct S { struct T x };
  // @endcode
  // For a struct S, which has a field of struct T, the S depends on T, so we
  // use T as the key and add S to its value.
  std::map<RecordDecl *, std::unordered_set<RecordDecl *>> typeDependencyMap;
  // map to memo the appear index
  std::map<RecordDecl *,int> nodes;
};

class TypeDependencyVisitor : public DeclVisitor<TypeDependencyVisitor> {
public:
  TypeDependencyVisitor() {}

  void VisitRecordDecl(RecordDecl *RD) {
    // Skip incomplete definitions and calculated definitions.
    if (!RD->isThisDeclarationADefinition() || visited.count(RD))
      return;

    graph.addNode(RD);
    visited.insert(RD);

    // Traverse all fields to calculate type dependencies.
    for (FieldDecl *Field : RD->fields()) {
      QualType FieldType = Field->getType();
      if (const RecordType *RT = FieldType->getAs<RecordType>()) {
        RecordDecl *FieldDecl = RT->getDecl();
        // Recursive for nested type definitions.
        Visit(FieldDecl);
        graph.addDependency(FieldDecl, RD);
      }
      if (const ArrayType *AT = FieldType->getAsArrayTypeUnsafe()) {
        RecordDecl *FieldDecl = ProcessArrayType(AT);
        if (FieldDecl) {
          Visit(FieldDecl);
          graph.addDependency(FieldDecl, RD);
        }
      }
      if (const PointerType *PT = FieldType->getAs<PointerType>()) {
        RecordDecl *FieldDecl = ProcessPointerType(PT);
        if (FieldDecl) {
          Visit(FieldDecl);
          graph.addDependency(FieldDecl, RD);
        }
      }
    }
  }

  void VisitClassTemplateDecl(ClassTemplateDecl *CTD) {
    for (ClassTemplateSpecializationDecl *CTSD : CTD->specializations()) {
      Visit(CTSD);
    }
  }

  RecordDecl *ProcessArrayType(const ArrayType *AT) {
    // Recursive to get innermost element type.
    QualType ElementType = AT->getElementType();
    while (const ArrayType *at = ElementType->getAsArrayTypeUnsafe()) {
      ElementType = at->getElementType();
    }
    if (const RecordType *RT = ElementType->getAs<RecordType>()) {
      return RT->getDecl();
    }
    if (const PointerType *PT = ElementType->getAs<PointerType>()) {
      return ProcessPointerType(PT);
    }
    return nullptr;
  }

  RecordDecl *ProcessPointerType(const PointerType *PT) {
    // Recursive to get innermost pointee type.
    QualType PointeeType = PT->getPointeeType();
    while (const PointerType *InnerPT = PointeeType->getAs<PointerType>()) {
      PointeeType = InnerPT->getPointeeType();
    }
    if (const ArrayType *AT = PointeeType->getAsArrayTypeUnsafe()) {
      return ProcessArrayType(AT);
    }
    return nullptr;
  }

  std::vector<RecordDecl *> Sort() { return graph.topologicalSort(); }

private:
  TypeDependencyGraph graph;
  std::set<RecordDecl *> visited;
};

//------------------------------- RewriteBSC -------------------------------//

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

  // Avoid repeatedly include of header files.
  std::set<std::string> IncludedHeaders;

  // Record all FileIDs of bishengc files.
  std::set<FileID> BSCFiles;

  // Save all rewritten texts.
  std::string SStr;
  llvm::raw_string_ostream Buf;

  void CollectIncludes();
  void CollectIncludesInFile(FileID FID);
  void FindDeclsWithoutBSCFeature();
  bool IsInStandardHeaderFile(SourceLocation Loc);
  void PrintDebugLineInfo(Decl *D);
  void ResolveFileIDsFromIncludeStack(const Decl *D);
  void RewriteMacroDirectives();
  void RewriteDecls();
  void RewriteRecordDeclaration(std::vector<Decl *> &DeclList);
  void RewriteTypedefAndEnum(std::vector<Decl *> &DeclList);
  void RewriteTypeDefinitions(std::vector<Decl *> &DeclList);
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
  // Traverse all Decls in the TranslationUnit, and collect all the hbs and cbs
  // files that need to be processed by resolving the include stack.
  for (const Decl *D : TUDecl->decls()) {
    ResolveFileIDsFromIncludeStack(D);
  }

  // Collect include directives written in cbs file and hbs files which are
  // included by cbs file directly or indirectly.
  for (const FileID &FID : BSCFiles) {
    CollectIncludesInFile(FID);
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
          StringRef HeaderRef = StringRef(Header).substr(1, Header.size() - 2);
          // If the included file is a hbs file, we just ignore it.
          // Otherwise, we reserve it.
          bool NotFound = true;
          for (const FileID &fid : BSCFiles) {
            const FileEntry *FE = SM->getFileEntryForID(fid);

            if (!FE)
              continue;

            if (FE->getName().endswith(HeaderRef)) {
              NotFound = false;
              break;
            }
          }
          if (NotFound)
            IncludedHeaders.insert(Header);
        }
      }
    }
  }
}

void RewriteBSC::FindDeclsWithoutBSCFeature() {
  BSCFeatureFinder finder = BSCFeatureFinder(*Context);
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Enum:
    case Decl::Function:
    case Decl::Record:
    case Decl::Var: {
      if (!finder.Visit(D)) {
        DeclsWithoutBSCFeature.insert(D);
      }
      break;
    }
    default:
      break;
    }
  }
}

bool RewriteBSC::IsInStandardHeaderFile(SourceLocation Loc) {
  FileID FID = SM->getFileID(SM->getFileLoc(Loc));
  return BSCFiles.find(FID) == BSCFiles.end();
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

/// ResolveFileIDsFromIncludeStack - For a given declaration, it has a include
/// stack. Because we want to collect include macro directives in hbs files,
/// we search the stack for hbs files, record them and save in BSCFiles.
void RewriteBSC::ResolveFileIDsFromIncludeStack(const Decl *D) {
  // Get the FileID of D.
  FileID CurFileID = SM->getFileID(SM->getSpellingLoc(D->getLocation()));
  FileID PrevFileID = CurFileID;
  std::set<FileID> FIDs;
  bool IsHBSFile = false;

  if (CurFileID.isInvalid())
    return;

  while (true) {
    const FileEntry *CurFileEntry = SM->getFileEntryForID(CurFileID);
    if (!CurFileEntry)
      break;
    if (CurFileEntry->getName().endswith(".cbs") ||
        CurFileEntry->getName().endswith(".c")) {
      FIDs.insert(CurFileID);
      break;
    }
    if (CurFileEntry->getName().endswith(".hbs") ||
        SM->getBufferData(CurFileID).startswith("#pragma bsc") || IsHBSFile) {
      IsHBSFile |= true;
      FIDs.insert(CurFileID);
    }

    SourceLocation IncludeLoc = SM->getIncludeLoc(CurFileID);
    if (IncludeLoc.isInvalid())
      break;

    PrevFileID = CurFileID;
    CurFileID = SM->getFileID(IncludeLoc);
  }

  BSCFiles.insert(FIDs.begin(), FIDs.end());
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
      if (!IsInStandardHeaderFile(MacroLoc) && !MI->isUsedForHeaderGuard() &&
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
  // avoid repeatedly rewrite of Template Functions.
  std::set<Decl *> RewritedFunctionDeclarationSet;

  // Step 1: Collect all record decls and instatiation struct decls.
  RewriteRecordDeclaration(DeclList);

  // Step 2: Collect all enum decls and typedef decls into DeclList.
  RewriteTypedefAndEnum(DeclList);

  // Step 3: Rewrite type definitions of user defined types, which is sorted by
  // type dependencies.
  RewriteTypeDefinitions(DeclList);

  // Print all TagDecls and TypedefNamDecls to Buf.
  for (Decl *D : DeclList) {
    // For decls with bsc feature, we use pretty printer.
    // For decls without bsc feature, we use original string text.
    if (DeclsWithoutBSCFeature.find(D) == DeclsWithoutBSCFeature.end()) {
      D->print(Buf, Policy);
      Buf << ";\n\n";
    } else {
      if (TagDecl *TD = dyn_cast<TagDecl>(D)) {
        // For an anonymous tagdecl with typedef, use pretty printer. Otherwise,
        // use original string text.
        if (!TD->getTypedefNameForAnonDecl()) {
          const char *startBuf = SM->getCharacterData(D->getBeginLoc());
          const char *endBuf = SM->getCharacterData(D->getEndLoc());
          Buf << std::string(startBuf, endBuf - startBuf + 1);
        } else {
          D->print(Buf, Policy);
        }
        Buf << ";\n\n";
      } else {
        llvm_unreachable("Unreachable branch");
      }
    }
  }

  // Step 4: Collect all instatiation function declarations.
  Policy.FunctionDeclarationOnly = true;
  RewriteInstantFunctionDecl(DeclList, RewritedFunctionDeclarationSet);

  // Step 5: Collect non-generic function definitions and var decls.
  Policy.FunctionDeclarationOnly = false;
  RewriteNonGenericFuncAndVar(DeclList);

  // Step 6: Collect all instatiation function definition.
  RewriteInstantFunctionDef(DeclList);
}

void RewriteBSC::RewriteRecordDeclaration(std::vector<Decl *> &DeclList) {
  for (Decl *D : TUDecl->decls()) {
    // Skip declarations in header files with suffix .h.
    if (IsInStandardHeaderFile(D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = dyn_cast<RecordDecl>(D);
      // Skip incomplete record decl.
      if (!RD->isCompleteDefinition())
        break;
      // Because we only want to print the declaration of the struct,
      // we set it to incomplete before printing, and then restore it after
      // printing.
      RD->setCompleteDefinition(false);
      RD->print(Buf, Policy);
      Buf << ";\n";
      RD->setCompleteDefinition(true);
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CTD = cast<ClassTemplateDecl>(D);
      // Skip incomplete generic struct decl.
      RecordDecl *RD = NULL;
      if ((RD = dyn_cast<RecordDecl>(CTD->getTemplatedDecl()))) {
        if (!RD->isCompleteDefinition()) {
          break;
        }
      }
      for (ClassTemplateSpecializationDecl *CTSD : CTD->specializations()) {
        // Because we only want to print the declaration of the generic struct,
        // we set it to incomplete before printing, and then restore it after
        // printing.
        RD->setCompleteDefinition(false);
        bool isCompleted = CTSD->isCompleteDefinition();
        if (isCompleted)
          CTSD->setCompleteDefinition(false);
        CTSD->print(Buf, Policy);
        Buf << ";\n";
        RD->setCompleteDefinition(true);
        CTSD->setCompleteDefinition(isCompleted);
      }
      break;
    }
    default:
      break;
    }
  }
}

void RewriteBSC::RewriteTypedefAndEnum(std::vector<Decl *> &DeclList) {
  for (Decl *D : TUDecl->decls()) {
    // Skip declarations in header files with suffix .h.
    if (IsInStandardHeaderFile(D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::Enum: {
      DeclList.push_back(D);
      break;
    }
    case Decl::TypeAlias:
    case Decl::Typedef: {
      TypedefNameDecl *TND = cast<TypedefNameDecl>(D);
      // For typedef of trait type, we don't need it.
      QualType UnderlyingType = TND->getUnderlyingType();
      if (UnderlyingType.getCanonicalType()->isTraitType())
        break;
      DeclList.push_back(D);
      break;
    }
    default:
      break;
    }
  }
}

/// RewriteTypeDefinitions - Rewrite type definitions of user defined types.
/// It is topologically sorted by type dependencies.
void RewriteBSC::RewriteTypeDefinitions(std::vector<Decl *> &DeclList) {
  TypeDependencyVisitor TDV = TypeDependencyVisitor();
  for (Decl *D : TUDecl->decls()) {
    switch (D->getKind()) {
    case Decl::ClassTemplate:
    case Decl::Record:
      TDV.Visit(D);
      break;
    default:
      break;
    }
  }

  std::vector<RecordDecl *> sorted = TDV.Sort();
  for (RecordDecl *D : sorted) {
    if (IsInStandardHeaderFile(D->getLocation())) {
      continue;
    }
    // Skip nested RecordDecl.
    if (!isa<TranslationUnitDecl>(D->getLexicalDeclContext())) {
      continue;
    }
    DeclList.push_back(D);
  }
}

void RewriteBSC::RewriteInstantFunctionDecl(
    std::vector<Decl *> &DeclList,
    std::set<Decl *> &RewritedFunctionDeclarationSet) {
  for (Decl *D : TUDecl->decls()) {
    if (IsInStandardHeaderFile(D->getLocation())) {
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
    if (IsInStandardHeaderFile(D->getLocation())) {
      continue;
    }
    switch (D->getKind()) {
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(D);
      // Don't print the declaration of a function of a generic class.
      // It will be printed when manipulating ClassTemplateDecl.
      if (FD->getBuiltinID()) {
        break;
      }
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
          bool Invalid = false;
          Lexer::getSourceText(
              CharSourceRange::getCharRange(FD->getSourceRange()), *SM,
              LangOpts, &Invalid);
          if (Invalid) {
            FD->print(Buf, Policy);
          } else {
            const char *startBuf = SM->getCharacterData(FD->getBeginLoc());
            const char *endBuf = SM->getCharacterData(FD->getEndLoc());
            if (startBuf == endBuf)
              break;
            Buf << std::string(startBuf, endBuf - startBuf + 1);
          }
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
    if (IsInStandardHeaderFile(D->getLocation())) {
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
            for (FunctionDecl *FD : FTD->specializations()) {
              if (FD->doesThisDeclarationHaveABody()) {
                PrintDebugLineInfo(FD);
                FD->print(Buf, Policy);
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