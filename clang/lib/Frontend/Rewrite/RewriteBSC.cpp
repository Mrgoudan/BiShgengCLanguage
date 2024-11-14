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
#include "clang/AST/BSC/WalkerBSC.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/ASTConsumers.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <set>

using namespace clang;

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
  Preprocessor &PP;
  std::unique_ptr<raw_ostream> OutFile;
  // control whether to insert bsc code line info
  bool WithLine;
  clang::PrintingPolicy Policy;

  unsigned RewriteFailedDiag;

  llvm::SmallPtrSet<Decl *, 16> DeclsWithoutBSCFeature;

  SourceLocation BeginLocOfFirstDecl;
  SourceLocation EndLocOfLastDecl;

  /// Save rewritten decls to aviod rewritting one decl twice.
  std::vector<Decl *> RewrittenDecls;

  void FindDeclsWithoutBSCFeature();
  const std::string GetRewrittenString();
  void printBSCLineInfo(Decl *D, llvm::raw_string_ostream &Buf);
  /// Get the begin source location of the first decl and the end source
  /// location of the last decl.
  void GetSourceLocationsOfFirstDeclAndLastDecl();

  /// Rewrite include directive form `#include "xxx.hbs"` to `#include "xxx.h"`.
  void RewriteInclude();
  void RewriteMacroDefinition();
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
                       DiagnosticsEngine &D, const LangOptions &LOpts,
                       Preprocessor &pp, bool InsertLine)
    : Diags(D), LangOpts(LOpts), InFileName(inFile), OutFile(std::move(OS)),
      PP(pp), WithLine(InsertLine), Policy(LangOpts) {
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
        if (*BufPtr == '"' || *BufPtr == '<') {
          if (++BufPtr == MainBufEnd)
            return;
          std::string Buf = "";
          SourceLocation StartLoc =
              LocStart.getLocWithOffset(BufPtr - MainBufStart);
          while (*BufPtr != '"' && *BufPtr != '>') {
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

typedef std::pair<const IdentifierInfo *, MacroInfo *> id_macro_pair;
void RewriteBSC::RewriteMacroDefinition() {
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);
  llvm::SetVector<id_macro_pair> MacrosSet;
  SourceLocation SL;

  for (Preprocessor::macro_iterator I = PP.macro_begin(false),
                                    E = PP.macro_end(false);
       I != E; ++I) {
    auto *MD = I->second.getLatest();

    if (MD && MD->isDefined()) {
      MacroInfo *MI = MD->getMacroInfo();
      SourceLocation MacroLoc = MI->getDefinitionLoc();
      if (SM->isWrittenInMainFile(MacroLoc) &&
          !MacrosSet.count(id_macro_pair(I->first, MI)) &&
          SM->isBeforeInTranslationUnit(BeginLocOfFirstDecl, MacroLoc) &&
          SM->isBeforeInTranslationUnit(MacroLoc, EndLocOfLastDecl)) {
        MacrosSet.insert(id_macro_pair(I->first, MI));
        const char *startBuf = SM->getCharacterData(MI->getDefinitionLoc());
        const char *endBuf = SM->getCharacterData(MI->getDefinitionEndLoc());
        unsigned len = MI->isFunctionLike() ? 1 : MI->getDefinitionLength(*SM);
        if (MD->getKind() == MacroDirective::MD_Define)
          Buf << "#define ";
        else if (MD->getKind() == MacroDirective::MD_Undefine)
          Buf << "#undef ";
        Buf << std::string(startBuf, endBuf - startBuf + len);
        Buf << "\n";
      }
    }
  }
  InsertText(BeginLocOfFirstDecl, Buf.str());
}

void RewriteBSC::HandleTranslationUnit(ASTContext &C) {
  if (Diags.hasErrorOccurred())
    return;

  RewriteInclude(); // Rewrite include directives.
  GetSourceLocationsOfFirstDeclAndLastDecl();
  RewriteMacroDefinition();
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
  // Step 1: Find decls without bsc features.
  FindDeclsWithoutBSCFeature();
  // Step 2: Get rewritten string
  // For decls that have bsc features, we use pretty printer.
  // For decls that do not have bsc features, we use the original decl string.
  const std::string &Str = GetRewrittenString();

  // Step 3: Replace buffered string with original decl string.
  const char *startBuf = SM->getCharacterData(BeginLocOfFirstDecl);
  const char *endBuf = SM->getCharacterData(EndLocOfLastDecl);
  ReplaceText(BeginLocOfFirstDecl, endBuf - startBuf + 1, Str);
}

void RewriteBSC::GetSourceLocationsOfFirstDeclAndLastDecl() {
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    if (IsBSCDesugared(*D, Context)) {
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

static QualType getInnerType(QualType QT) {
  QT = QT.IgnoreParens();
  while (QT->isPointerType())
    QT = QT->getPointeeType();
  return QT;
}

// Determine whether the QualType contains instantiated generic type.
static bool HasTemplateSpecType(QualType QT,
                                std::set<Decl *> HasTemplateSpecSet) {
  if (QT->getAs<TemplateSpecializationType>()) {
    return true;
  } else if (const RecordType *RT = QT->getAs<RecordType>()) {
    return HasTemplateSpecSet.count(RT->getDecl());
  }
  return false;
}

void RewriteBSC::FindDeclsWithoutBSCFeature() {
  BSCFeatureFinder finder = BSCFeatureFinder(*Context);
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::Var:
    case Decl::Record:
    case Decl::Function: {
      if (!SM->isWrittenInMainFile(SM->getSpellingLoc(D->getBeginLoc())) ||
          !SM->isWrittenInMainFile(SM->getSpellingLoc(D->getEndLoc()))) {
        break;
      }
      if (!finder.Visit(*D)) {
        DeclsWithoutBSCFeature.insert(*D);
        if (RecordDecl *RD = dyn_cast<RecordDecl>(*D)) {
          if (TypedefNameDecl *TND = RD->getTypedefNameForAnonDecl()) {
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

void RewriteBSC::printBSCLineInfo(Decl *D, llvm::raw_string_ostream &Buf) {
  if (!WithLine) return;
  unsigned LineNo = SM->getSpellingLineNumber(D->getBeginLoc()); // get bsc code line
  Buf << "#line " << LineNo << " \"" << SM->getBufferName(D->getBeginLoc()) << "\"\n"; // insert bsc code line
}

bool argumentTypeDeclInDeclList(ClassTemplateSpecializationDecl *DD,
                                const std::vector<Decl *> DeclList) {
  auto &DDA = DD->getTemplateInstantiationArgs();
  auto Ite = DeclList.begin();
  while (Ite != DeclList.end()) {
    for (unsigned int i = 0; i < DDA.size(); i++) {
      if (DDA.get(i).getKind() == TemplateArgument::Type &&
          (DDA.get(i).getAsType()->getAsCXXRecordDecl() == (*Ite) ||
           DDA.get(i).getAsType()->getAsRecordDecl() == (*Ite))) {
        return true;
      }
    }
    Ite++;
  }
  return false;
}

const std::string RewriteBSC::GetRewrittenString() {
  std::string SStr;
  llvm::raw_string_ostream Buf(SStr);

  std::vector<Decl *> DeclList;
  std::set<Decl *> HasTemplateSpecSet;
  // avoid repeatedly rewrite of Template Functions.
  std::set<Decl *> RewritedFunctionDeclarationSet;
  auto EndsWith = [](const std::string &str, const std::string &suffix) {
    if (str.length() >= suffix.length()) {
      return 0 == str.compare(str.length() - suffix.length(), suffix.length(),
                              suffix);
    }
    return false;
  };
  bool IsHBSFile = EndsWith(InFileName, "hbs");

  // Step 1: Sort all struct/union/enum/typedef decls.
  // Step 1.1: Put non-generic struct / union / enum / typedef decls into
  // DeclList.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    switch ((*D)->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = cast<RecordDecl>(*D);
      RecordDecl::field_iterator Field, FieldEnd;
      bool HasTemplateSpec = false;
      for (Field = RD->field_begin(), FieldEnd = RD->field_end();
           Field != FieldEnd; ++Field) {
        QualType FieldTy = getInnerType(Field->getType());
        if (HasTemplateSpecType(FieldTy, HasTemplateSpecSet)) {
          HasTemplateSpec = true;
          HasTemplateSpecSet.insert(*D);
          break;
        }
      }
      // In the header file, skip the RecordDecl containing instantiated type.
      // and move them to the .c file
      if (IsHBSFile && HasTemplateSpec)
        break;
      if (!SM->isWrittenInMainFile(SM->getSpellingLoc(RD->getBeginLoc())) &&
          !SM->isWrittenInMainFile(SM->getSpellingLoc(RD->getEndLoc())) &&
          !HasTemplateSpec)
        break;
      DeclList.push_back(*D);
      break;
    }
    case Decl::Enum: {
      if (SM->isWrittenInMainFile(SM->getSpellingLoc(D->getBeginLoc())) ||
          SM->isWrittenInMainFile(SM->getSpellingLoc(D->getEndLoc()))) {
        DeclList.push_back(*D);
        DeclsWithoutBSCFeature.insert(*D);
        if (TypedefNameDecl *TND =
                cast<EnumDecl>(*D)->getTypedefNameForAnonDecl()) {
          DeclsWithoutBSCFeature.insert(TND);
        }
      }
      break;
    }
    case Decl::Typedef: {
      TypedefDecl *TD = cast<TypedefDecl>(*D);
      bool HasTemplateSpec = false;
      if (const RecordType *RT = TD->getUnderlyingType()->getAs<RecordType>()) {
        RecordDecl *RD = RT->getDecl();
        HasTemplateSpec = HasTemplateSpecSet.count(RD);
      } else if (const PointerType *PT = TD->getUnderlyingType()
                                             .IgnoreParens()
                                             ->getAs<PointerType>()) {
        if (const FunctionType *FT =
                PT->getPointeeType().IgnoreParens()->getAs<FunctionType>()) {
          QualType ReturnTy = getInnerType(FT->getReturnType());
          if (HasTemplateSpecType(ReturnTy, HasTemplateSpecSet))
            HasTemplateSpec = true;
          else if (auto *FPT = dyn_cast<FunctionProtoType>(FT)) {
            for (unsigned i = 0; i < FPT->getNumParams(); i++) {
              QualType ParamTy = getInnerType(FPT->getParamType(i));
              if (HasTemplateSpecType(ParamTy, HasTemplateSpecSet)) {
                HasTemplateSpec = true;
                break;
              }
            }
          }
        }
      } else if (TD->getUnderlyingType().getCanonicalType()->isTraitType())
        break;
      // In the header file, skip the TypedefDecl containing instantiated type.
      // and move them to the .c file
      if (IsHBSFile && HasTemplateSpec)
        break;
      if (!SM->isWrittenInMainFile(SM->getSpellingLoc(TD->getBeginLoc())) &&
          !SM->isWrittenInMainFile(SM->getSpellingLoc(TD->getEndLoc())) &&
          !HasTemplateSpec)
        break;
      DeclList.push_back(*D);
    }
    default:
      break;
    }
  }

  // Step 1.2: Insert instantiated stuct / union decls into DeclList.
  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    // In the header file, skip instantiated stuct / union decls.
    if (IsHBSFile) {
      break;
    }
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
            // Insert to vector if current Decl dependent me
            if (auto *TD = dyn_cast<ClassTemplateSpecializationDecl>(*It)) {
              auto &TDA = TD->getTemplateInstantiationArgs();

              for (unsigned int i = 0; i < TDA.size(); i++) {
                if (TDA.get(i).getKind() == TemplateArgument::Type &&
                    TDA.get(i).getAsType()->getAsCXXRecordDecl() == DD) {
                  It = DeclList.insert(It, DD);
                  Inserted = true;
                  break;
                }
              }
              if (Inserted) {
                break;
              }
            }
            // Push it to back if a template argument type decl is in DeclList
            if (!SM->isWrittenInMainFile(SL) &&
                argumentTypeDeclInDeclList(DD, DeclList)) {
              break;
            }

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
    if (DeclsWithoutBSCFeature.find(D) == DeclsWithoutBSCFeature.end()) {
      D->print(Buf, Policy);
      Buf << ";\n\n";
    } else {
      if (auto *TD = dyn_cast<TagDecl>(D)) {
        if (TD->getTypedefNameForAnonDecl()) {
        } else {
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

  // Step 2: Collect all instatiation function declarations.
  Policy.FunctionDeclaraionOnly = true;

  for (DeclContext::decl_iterator D = TUDecl->decls_begin(),
                                  DEnd = TUDecl->decls_end();
       D != DEnd; ++D) {
    // In the header file, skip all instatiation function declarations.
    if (IsHBSFile) {
      break;
    }
    switch ((*D)->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = cast<RecordDecl>(*D);
      if (RD->isOwnedDecl()) {
        for (auto Func : RD->decls()) {
          if (isa<BSCMethodDecl>(Func)) { //
            Func->print(Buf, Policy);
            Buf << ";\n\n";
          }
        }
      }
      break;
    }
    case Decl::BSCMethod:
    case Decl::Function: {
      FunctionDecl *FD = cast<FunctionDecl>(*D);
      if (FD->getParent() && isa<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if ((SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getBeginLoc())) ||
           SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getEndLoc()))) &&
          FD->isTemplateInstantiation()) {
        FD->print(Buf, Policy);
        Buf << ";\n\n";
      }

      break;
    }
    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(*D);
      for (auto *DD : FTD->specializations()) {
        auto IT = RewritedFunctionDeclarationSet.find(DD);
        if (IT == RewritedFunctionDeclarationSet.end()) {
          RewritedFunctionDeclarationSet.insert(DD);
          DD->print(Buf, Policy);
          Buf << ";\n\n";
        }
      }
      break;
    }
    case Decl::ClassTemplate: {
      ClassTemplateDecl *CT = cast<ClassTemplateDecl>(*D);
      for (auto *DD : CT->specializations()) {
        for (auto *DDD : DD->decls()) {
          if (isa<FunctionDecl>(DDD) && DDD->hasBody()) {
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
      if (!FD->isTemplateInstantiation()) {
        // Check if there is a generic instantiation in the function declaration
        bool HasTemplateSpec = false;
        const FunctionType *FT = FD->getType()->getAs<FunctionType>();
        QualType ReturnTy = getInnerType(FT->getReturnType());
        if (HasTemplateSpecType(ReturnTy, HasTemplateSpecSet))
          HasTemplateSpec = true;
        else if (auto *FPT = dyn_cast<FunctionProtoType>(FT)) {
          for (unsigned i = 0; i < FPT->getNumParams(); i++) {
            QualType ParamTy = getInnerType(FPT->getParamType(i));
            if (HasTemplateSpecType(ParamTy, HasTemplateSpecSet)) {
              HasTemplateSpec = true;
              break;
            }
          }
        }
        // In the header file, skip the FunctionDecl / BSCMethodDecl containing
        // instantiated type. and move them to the .c file
        if (IsHBSFile && HasTemplateSpec)
          break;
        if (!SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getBeginLoc())) &&
            !SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getEndLoc())) &&
            !HasTemplateSpec &&
            FD->getStorageClass() == StorageClass::SC_Extern)
          break;
        if (!SM->isWrittenInMainFile(SM->getExpansionLoc(FD->getBeginLoc())) &&
            !SM->isWrittenInMainFile(SM->getExpansionLoc(FD->getEndLoc())) &&
            !HasTemplateSpec)
          break;

        // If it is BscMethod or macro expansion function, output the code
        // according to AST; Otherwise, simply retrieve the source code
        if (DeclsWithoutBSCFeature.find(FD) == DeclsWithoutBSCFeature.end() ||
            FD->getBeginLoc().isMacroID()) {
          printBSCLineInfo(FD, Buf);
          FD->print(Buf, Policy);
        } else {
          const char *startBuf = SM->getCharacterData(FD->getBeginLoc());
          const char *endBuf = SM->getCharacterData(FD->getEndLoc());
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
      VarDecl *VD = cast<VarDecl>(*D);
      QualType VarTy = getInnerType(VD->getType());
      bool HasTemplateSpec = false;
      if (HasTemplateSpecType(VarTy, HasTemplateSpecSet))
        HasTemplateSpec = true;
      // In the header file, skip the VarDecl containing instantiated type.
      // and move them to the .c file
      if (IsHBSFile && HasTemplateSpec)
        break;
      if (!SM->isWrittenInMainFile(SM->getExpansionLoc(VD->getBeginLoc())) &&
          !SM->isWrittenInMainFile(SM->getExpansionLoc(VD->getEndLoc())) &&
          !HasTemplateSpec)
        break;
      D->print(Buf, Policy);
      Buf << ";\n\n";

      break;
    }
    case Decl::FileScopeAsm: {
      if (SM->isWrittenInMainFile(SM->getSpellingLoc(D->getBeginLoc())) ||
          SM->isWrittenInMainFile(SM->getSpellingLoc(D->getEndLoc()))) {
        const char *startBuf = SM->getCharacterData(D->getBeginLoc());
        const char *endBuf = SM->getCharacterData(D->getEndLoc());
        Buf << std::string(startBuf, endBuf - startBuf + 1);
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
    // In the header file, skip all instatiation function definition.
    if (IsHBSFile) {
      break;
    }
    switch ((*D)->getKind()) {
    case Decl::Record: {
      RecordDecl *RD = cast<RecordDecl>(*D);
      if (RD->isOwnedDecl()) {
        for (auto Func : RD->decls()) {
          if (isa<BSCMethodDecl>(Func)) {
            FunctionDecl *FD = cast<FunctionDecl>(Func);
            if (FD->doesThisDeclarationHaveABody()) {
              printBSCLineInfo(FD, Buf);
              FD->print(Buf, Policy);
              Buf << "\n";
            }
          }
          if (isa<FunctionTemplateDecl>(Func)) {
            FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(Func);
            for (auto *DD : FTD->specializations()) {
              if (DD->doesThisDeclarationHaveABody()) {
                printBSCLineInfo(DD, Buf);
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
      FunctionDecl *FD = cast<FunctionDecl>(*D);
      if (FD->getParent() && isa<RecordDecl>(FD->getParent()) &&
          cast<RecordDecl>(FD->getParent())->getDescribedClassTemplate()) {
        break;
      }
      if (FD->isAsyncSpecified())
        break;
      if ((SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getBeginLoc())) ||
           SM->isWrittenInMainFile(SM->getSpellingLoc(FD->getEndLoc()))) &&
          FD->isTemplateInstantiation()) {
        if (FD->doesThisDeclarationHaveABody()) {
          printBSCLineInfo(FD, Buf);
          FD->print(Buf, Policy);
          Buf << "\n";
        }
      }
      break;
    }

    case Decl::FunctionTemplate: {
      FunctionTemplateDecl *FTD = cast<FunctionTemplateDecl>(*D);
      if (FTD->isThisDeclarationADefinition()) {
        for (auto *DD : FTD->specializations()) {
          if (DD->doesThisDeclarationHaveABody()) {
            printBSCLineInfo(DD, Buf);
            DD->print(Buf, Policy);
            Buf << "\n";
          }
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
              printBSCLineInfo(DDD, Buf);
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