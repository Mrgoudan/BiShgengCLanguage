//===--- ParseDeclBSC.cpp - BSC Declaration Parsing -------------*- BSC -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements the BSC Declaration portions of the Parser interfaces.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/PrettyDeclStackTrace.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Parse/RAIIObjectsForParser.h"

using namespace clang;

void Parser::ParseBSCScopeSpecifiers(DeclSpec &DS) {
  bool BSCScopeSpecFlag = true;
  ParseDeclarationSpecifiers(DS, ParsedTemplateInfo(), AS_none,
                             DeclSpecContext::DSC_normal, nullptr,
                             BSCScopeSpecFlag);
}

bool Parser::ParseTraitMemberDeclaratorBeforeInitializer(
    Declarator &DeclaratorInfo, VirtSpecifiers &VS, ExprResult &BitfieldSize,
    LateParsedAttrList &LateParsedAttrs) {
  if (Tok.isNot(tok::colon)) {
    DeclaratorInfo.setIsTraitMember(true);
    ParseDeclarator(DeclaratorInfo);
  } else {
    DeclaratorInfo.SetIdentifier(nullptr, Tok.getLocation());
  }

  if (!DeclaratorInfo.isFunctionDeclarator() && TryConsumeToken(tok::colon)) {
    assert(DeclaratorInfo.isPastIdentifier() &&
           "don't know where identifier would go yet?");
    BitfieldSize = ParseConstantExpression();
    if (BitfieldSize.isInvalid())
      SkipUntil(tok::comma, StopAtSemi | StopBeforeMatch);
  }
  if (!DeclaratorInfo.hasName() && BitfieldSize.isUnset()) {
    // If so, skip until the semi-colon or a }.
    SkipUntil(tok::r_brace, StopAtSemi | StopBeforeMatch);
    return true;
  }
  return false;
}

Parser::DeclGroupPtrTy
Parser::ParseTraitMemberDeclaration(ParsedAttributes &AccessAttrs) {
  ParenBraceBracketBalancer BalancerRAIIObj(*this);
  ParsedAttributes attrs(AttrFactory);
  ParsedAttributesView FnAttrs;
  // we need to keep these attributes for future diagnostic
  // before they are taken over by declaration specifier
  FnAttrs.addAll(attrs.begin(), attrs.end());
  FnAttrs.Range = attrs.Range;
  LateParsedAttrList CommonLateParsedAttrs;
  // decl-specifier-seq:
  // Parse the common declaration-specifiers piece
  ParsingDeclSpec DS(*this);
  DS.takeAttributesFrom(attrs);
  ParseDeclarationSpecifiers(DS, ParsedTemplateInfo(), AS_public,
                             DeclSpecContext::DSC_class,
                             &CommonLateParsedAttrs);
  if (TryConsumeToken(tok::semi)) {
    RecordDecl *AnonRecord = nullptr;
    Decl *TheDecl = Actions.ParsedFreeStandingDeclSpec(getCurScope(), AS_public,
                                                       DS, FnAttrs, AnonRecord);
    DS.complete(TheDecl);
    if (AnonRecord) {
      Decl *decls[] = {AnonRecord, TheDecl};
      return Actions.BuildDeclaratorGroup(decls);
    }
    return Actions.ConvertDeclToDeclGroup(TheDecl);
  }
  ParsingDeclarator DeclaratorInfo(*this, DS, attrs, DeclaratorContext::Member);
  VirtSpecifiers VS;
  // Hold late-parsed attributes so we can attach a Decl to them later.
  LateParsedAttrList LateParsedAttrs;
  SmallVector<Decl *, 8> DeclsInGroup;
  ExprResult BitfieldSize;
  ExprResult TrailingRequiresClause;
  ParseTraitMemberDeclaratorBeforeInitializer(DeclaratorInfo, VS, BitfieldSize,
                                              LateParsedAttrs);
  while (1) {
    NamedDecl *ThisDecl =
        Actions.ActOnTraitMemberDeclarator(getCurScope(), DeclaratorInfo);
    if (ThisDecl) {
      Actions.ProcessDeclAttributeList(getCurScope(), ThisDecl, AccessAttrs);
      if (!ThisDecl->isInvalidDecl()) {
        // Set the Decl for any late parsed attributes
        for (unsigned i = 0, ni = CommonLateParsedAttrs.size(); i < ni; ++i)
          CommonLateParsedAttrs[i]->addDecl(ThisDecl);
        for (unsigned i = 0, ni = LateParsedAttrs.size(); i < ni; ++i)
          LateParsedAttrs[i]->addDecl(ThisDecl);
      }
      Actions.FinalizeDeclaration(ThisDecl);
      DeclsInGroup.push_back(ThisDecl); // Put each Decl inside struct Foo
      if (DeclaratorInfo.isFunctionDeclarator() &&
          DeclaratorInfo.getDeclSpec().getStorageClassSpec() !=
              DeclSpec::SCS_typedef)
        HandleMemberFunctionDeclDelays(DeclaratorInfo, ThisDecl);
    }
    LateParsedAttrs.clear();
    DeclaratorInfo.complete(ThisDecl);
    // if we dont have a comma, it is either the end of the list (a, ';')
    // or an error, bail out
    SourceLocation CommaLoc;
    if (!TryConsumeToken(tok::comma, CommaLoc))
      break;
    if (Tok.isAtStartOfLine() &&
        !MightBeDeclarator(DeclaratorContext::Member)) {
      Diag(CommaLoc, diag::err_expected_semi_declaration)
          << FixItHint::CreateReplacement(CommaLoc, ";");
      break;
    }
    // Parse the next declarator
    DeclaratorInfo.clear();
    VS.clear();
    BitfieldSize = ExprResult(false);
    DeclaratorInfo.setCommaLoc(CommaLoc);
    if (ParseTraitMemberDeclaratorBeforeInitializer(
            DeclaratorInfo, VS, BitfieldSize, LateParsedAttrs))
      break;
  }
  return Actions.FinalizeDeclaratorGroup(getCurScope(), DS, DeclsInGroup);
}

void Parser::ParseTraitBody(SourceLocation TraitLoc,
                            SourceLocation AttrFixitLoc, Decl *TagDecl) {
  PrettyDeclStackTraceEntry CrashInfo(Actions.Context, TagDecl, TraitLoc,
                                      "parsing trait body");
  bool NonNestedClass = true;
  // Enter a scope for the class
  ParseScope ClassScope(this, Scope::ClassScope | Scope::DeclScope);

  // Note that we are parsing a new (potentially-nested) class definition
  ParsingClassDefinition ParsingDef(*this, TagDecl, NonNestedClass, false);
  if (TagDecl)
    Actions.ActOnTagStartDefinition(getCurScope(), TagDecl);
  if (!Tok.is(tok::l_brace)) {
    if (TagDecl)
      Actions.ActOnTagDefinitionError(getCurScope(), TagDecl);
    return;
  }
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();
  ParsedAttributes AccessAttrs(AttrFactory);
  if (TagDecl) {
    // While we still have something to read. read the member-decls
    while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
           Tok.isNot(tok::eof)) {
      // Each iter of this loop reads one member-declaration
      if (Tok.is(tok::semi)) {
        ConsumeExtraSemi(InsideStruct, DeclSpec::TST_trait);
        continue;
      }
      ParseTraitMemberDeclaration(AccessAttrs);
      MaybeDestroyTemplateIds();
      if (TryConsumeToken(tok::semi))
        continue;
    }
    T.consumeClose();
  } else {
    SkipUntil(tok::r_brace);
  }
  ParsedAttributes attrs(AttrFactory);
  if (TagDecl)
    Actions.ActOnFinishTraitMemberSpecification(TagDecl);
  if (TagDecl)
    Actions.ActOnTagFinishDefinition(getCurScope(), TagDecl, T.getRange());

  // Leave the trait scope
  ParsingDef.Pop();
  ClassScope.Exit();
}

// FIXME: ParseTraitSpecifier can be refactored, remove useless code
void Parser::ParseTraitSpecifier(SourceLocation StartLoc, DeclSpec &DS,
                                 const ParsedTemplateInfo &TemplateInfo,
                                 bool EnteringContext, DeclSpecContext DSC,
                                 ParsedAttributes &Attributes) {
  assert(getLangOpts().BSC &&
         "Error enter bsc trait specifier parsing function.");
  DeclSpec::TST TagType = DeclSpec::TST_trait;
  tok::TokenKind TagTokKind = tok::kw_trait;
  if (Tok.is(tok::code_completion)) {
    Actions.CodeCompleteTag(getCurScope(), TagType);
    return cutOffParsing();
  }
  ParsedAttributes attrs(AttrFactory);
  SourceLocation AttrFixitLoc = Tok.getLocation();
  IdentifierInfo *Name = nullptr;
  if (Tok.is(tok::identifier))
    Name = Tok.getIdentifierInfo();

  struct PreserveAtomicIdentifierInfoRAII {
    PreserveAtomicIdentifierInfoRAII(Token &Tok, bool Enabled)
        : AtomicII(nullptr) {
      if (!Enabled)
        return;
      assert(Tok.is(tok::kw__Atomic));
      AtomicII = Tok.getIdentifierInfo();
      AtomicII->revertTokenIDToIdentifier();
      Tok.setKind(tok::identifier);
    }
    ~PreserveAtomicIdentifierInfoRAII() {
      if (!AtomicII)
        return;
      AtomicII->revertIdentifierToTokenID(tok::kw__Atomic);
    }
    IdentifierInfo *AtomicII;
  };

  CXXScopeSpec &SS = DS.getTypeSpecScope();
  ColonProtectionRAIIObject X(*this);
  CXXScopeSpec Spec;
  bool HasValidSpec = true;

  if (ParseOptionalBSCGenericSpecifier(Spec, /*ObjectType=*/nullptr,
                                       /*ObjectHadErrors=*/false,
                                       EnteringContext)) {
    DS.SetTypeSpecError();
    HasValidSpec = false;
  }

  if (Spec.isSet())
    if (Tok.isNot(tok::identifier) && Tok.isNot(tok::annot_template_id)) {
      Diag(Tok, diag::err_expected) << tok::identifier;
      HasValidSpec = false;
    }
  if (HasValidSpec)
    SS = Spec;

  TemplateParameterLists *TemplateParams = TemplateInfo.TemplateParams;

  auto RecoverFromUndeclaredTemplateName = [&](IdentifierInfo *Name,
                                               SourceLocation NameLoc,
                                               SourceRange TemplateArgRange,
                                               bool KnownUndeclared) {
    Diag(NameLoc, diag::err_explicit_spec_non_template)
        << (TemplateInfo.Kind == ParsedTemplateInfo::ExplicitInstantiation)
        << TagTokKind << Name << TemplateArgRange << KnownUndeclared;

    // Strip off the last template parameter list if it was empty, since
    // we've removed its template argument list.
    if (TemplateParams && TemplateInfo.LastParameterListWasEmpty) {
      if (TemplateParams->size() > 1) {
        TemplateParams->pop_back();
      } else {
        TemplateParams = nullptr;
        const_cast<ParsedTemplateInfo &>(TemplateInfo).Kind =
            ParsedTemplateInfo::NonTemplate;
      }
    } else if (TemplateInfo.Kind == ParsedTemplateInfo::ExplicitInstantiation) {
      // Pretend this is just a forward declaration.
      TemplateParams = nullptr;
      const_cast<ParsedTemplateInfo &>(TemplateInfo).Kind =
          ParsedTemplateInfo::NonTemplate;
      const_cast<ParsedTemplateInfo &>(TemplateInfo).TemplateLoc =
          SourceLocation();
      const_cast<ParsedTemplateInfo &>(TemplateInfo).ExternLoc =
          SourceLocation();
    }
  };

  SourceLocation NameLoc;
  TemplateIdAnnotation *TemplateId = nullptr;
  bool isParsingBSCTemplateTrait =
      Tok.is(tok::identifier) && NextToken().is(tok::less);
  if (Tok.is(tok::identifier)) {
    Name = Tok.getIdentifierInfo();
    NameLoc = ConsumeToken();

    // BSC Trait Template Declaration may have "<T>" syntax.
    //      This param list must been parsed, skip it.
    if (isParsingBSCTemplateTrait) {
      while (Tok.getKind() != tok::greater) {
        ConsumeToken();
      }
      if (Tok.is(tok::greater)) {
        ConsumeToken();
      } else {
        Diag(Tok.getLocation(), diag::err_expected_comma_greater);
      }
    }
  } else if (Tok.is(tok::annot_template_id)) {
    TemplateId = takeTemplateIdAnnotation(Tok);
    NameLoc = ConsumeAnnotationToken();

    if (TemplateId->Kind == TNK_Undeclared_template) {
      // Try to resolve the template name to a type template. May update Kind.
      Actions.ActOnUndeclaredTypeTemplateName(
          getCurScope(), TemplateId->Template, TemplateId->Kind, NameLoc, Name);
      if (TemplateId->Kind == TNK_Undeclared_template) {
        RecoverFromUndeclaredTemplateName(
            Name, NameLoc,
            SourceRange(TemplateId->LAngleLoc, TemplateId->RAngleLoc), true);
        TemplateId = nullptr;
      }
    }

    if (TemplateId && !TemplateId->mightBeType()) {
      // The template-name in the simple-template-id refers to
      // something other than a type template. Give an appropriate
      // error message and skip to the ';'.
      SourceRange Range(NameLoc);
      if (SS.isNotEmpty())
        Range.setBegin(SS.getBeginLoc());

      // FIXME: Name may be null here.
      Diag(TemplateId->LAngleLoc, diag::err_template_spec_syntax_non_template)
          << TemplateId->Name << static_cast<int>(TemplateId->Kind) << Range;

      DS.SetTypeSpecError();
      SkipUntil(tok::semi, StopBeforeMatch);
      return;
    }
  }

  const PrintingPolicy &Policy = Actions.getASTContext().getPrintingPolicy();
  Sema::TagUseKind TUK;
  if (isDefiningTypeSpecifierContext(DSC, false) == AllowDefiningTypeSpec::No)
    TUK = Sema::TUK_Reference;
  else if (Tok.is(tok::l_brace)) {
    TUK = Sema::TUK_Definition;
    if (Actions.getCurScope()->getDepth() > 0 &&
        !(isParsingBSCTemplateTrait &&
          Actions.getCurScope()->getDepth() == 1)) {
      Diag(StartLoc, diag::err_trait_def_not_at_top_level);
      return;
    }
  } else if (!isTypeSpecifier(DSC) &&
             (Tok.is(tok::semi) ||
              (Tok.isAtStartOfLine() && !isValidAfterTypeSpecifier(false)))) {
    TUK = DS.isFriendSpecified() ? Sema::TUK_Friend : Sema::TUK_Declaration;
    if (Tok.isNot(tok::semi)) {
      const PrintingPolicy &PPol = Actions.getASTContext().getPrintingPolicy();
      // A semicolon was missing after this declaration. Diagnose and recover.
      ExpectAndConsume(tok::semi, diag::err_expected_after,
                       DeclSpec::getSpecifierName(TagType, PPol));
      PP.EnterToken(Tok, /*IsReinject*/ true);
      Tok.setKind(tok::semi);
    } else {
      std::string TraitName = "(null)";
      if (Name)
        TraitName = Name->getName().str();
      Diag(StartLoc, diag::err_invalid_trait) << TraitName;
    }
  } else
    TUK = Sema::TUK_Reference;

  if (TUK != Sema::TUK_Reference) {
    SourceRange AttrRange = Attributes.Range;
    if (AttrRange.isValid()) {
      Diag(AttrRange.getBegin(), diag::err_attributes_not_allowed)
          << AttrRange
          << FixItHint::CreateInsertionFromRange(
                 AttrFixitLoc, CharSourceRange(AttrRange, true))
          << FixItHint::CreateRemoval(AttrRange);
      attrs.takeAllFrom(Attributes);
    }
  }

  if (!Name && !TemplateId &&
      (DS.getTypeSpecType() == DeclSpec::TST_error ||
       TUK != Sema::TUK_Definition)) {
    if (DS.getTypeSpecType() != DeclSpec::TST_error) {
      Diag(StartLoc, diag::err_anon_type_definition)
          << DeclSpec::getSpecifierName(TagType, Policy);
    }
    if (TUK == Sema::TUK_Definition && Tok.is(tok::colon))
      SkipUntil(tok::semi, StopBeforeMatch);
    else
      SkipUntil(tok::comma, StopAtSemi);
    return;
  }

  DeclResult TagOrTempResult = true; // invalid
  TypeResult TypeResult = true;      // invalid
  bool Owned = false;
  Sema::SkipBodyInfo SkipBody;
  bool IsDependent = false;
  MultiTemplateParamsArg TParams;
  if (TUK != Sema::TUK_Reference && TemplateParams)
    TParams =
        MultiTemplateParamsArg(&(*TemplateParams)[0], TemplateParams->size());

  if (TemplateId) {
    ASTTemplateArgsPtr TemplateArgsPtr(TemplateId->getTemplateArgs(),
                                       TemplateId->NumArgs);
    if (TemplateId->isInvalid()) {
      // Can't build the declaration.
    } else if (TUK == Sema::TUK_Reference) {
      ProhibitCXX11Attributes(attrs, diag::err_attributes_not_allowed,
                              /*DiagnoseEmptyAttrs=*/true);
      TypeResult = Actions.ActOnTagTemplateIdType(
          TUK, TagType, StartLoc, SS, TemplateId->TemplateKWLoc,
          TemplateId->Template, TemplateId->TemplateNameLoc,
          TemplateId->LAngleLoc, TemplateArgsPtr, TemplateId->RAngleLoc);
    }
  } else {
    if (TUK != Sema::TUK_Declaration && TUK != Sema::TUK_Definition)
      ProhibitAttributes(attrs);

    stripTypeAttributesOffDeclSpec(attrs, DS, TUK);
    TagOrTempResult = Actions.ActOnTag(
        getCurScope(), TagType, TUK, StartLoc, SS, Name, NameLoc, attrs,
        AS_public, DS.getModulePrivateSpecLoc(), TParams, Owned, IsDependent,
        SourceLocation(), false, clang::TypeResult(),
        DSC == DeclSpecContext::DSC_type_specifier,
        DSC == DeclSpecContext::DSC_template_param ||
            DSC == DeclSpecContext::DSC_template_type_arg,
        &SkipBody);
    if (IsDependent) {
      assert(TUK == Sema::TUK_Reference);
      TypeResult = Actions.ActOnDependentTag(getCurScope(), TagType, TUK, SS,
                                             Name, StartLoc, NameLoc);
    }
  }

  if (TUK == Sema::TUK_Definition) {
    assert(Tok.is(tok::l_brace));
    ParseTraitBody(StartLoc, AttrFixitLoc, TagOrTempResult.get());
    if (SkipBody.CheckSameAsPrevious &&
        !Actions.ActOnDuplicateDefinition(TagOrTempResult.get(), SkipBody)) {
      DS.SetTypeSpecError();
      return;
    }
  }

  if (!TagOrTempResult.isInvalid())
    Actions.ProcessDeclAttributeDelayed(TagOrTempResult.get(), attrs);

  const char *PrevSpec = nullptr;
  unsigned DiagID;
  bool Result;
  if (!TypeResult.isInvalid()) {
    Result = DS.SetTypeSpecType(DeclSpec::TST_typename, StartLoc,
                                NameLoc.isValid() ? NameLoc : StartLoc,
                                PrevSpec, DiagID, TypeResult.get(), Policy);
  } else if (!TagOrTempResult.isInvalid()) {
    Result = DS.SetTypeSpecType(
        TagType, StartLoc, NameLoc.isValid() ? NameLoc : StartLoc, PrevSpec,
        DiagID, TagOrTempResult.get(), Owned, Policy);
  } else {
    DS.SetTypeSpecError();
    return;
  }

  if (Result)
    Diag(StartLoc, DiagID) << PrevSpec;

  // In a template-declaration which defines a class, no declarator is
  // permitted. eg. trait F<T> {}G; should report an error.
  if (TUK == Sema::TUK_Definition && !isTypeSpecifier(DSC) &&
      (TemplateInfo.Kind || !isValidAfterTypeSpecifier(false))) {
    if (Tok.isNot(tok::semi)) {
      const PrintingPolicy &PPol = Actions.getASTContext().getPrintingPolicy();
      ExpectAndConsume(tok::semi, diag::err_expected_after,
                       DeclSpec::getSpecifierName(TagType, PPol));
      PP.EnterToken(Tok, /*IsReinject=*/true);
      Tok.setKind(tok::semi);
    }
  }

  // desugar
  Decl *D = TagOrTempResult.get();
  if (TUK == Sema::TUK_Definition && D != nullptr && !D->isInvalidDecl()) {
    TraitDecl *TD = nullptr;
    if (auto *TTD = dyn_cast_or_null<TraitTemplateDecl>(D)) {
      TD = TTD->getTemplatedDecl();
    } else if (auto *TraitD = dyn_cast_or_null<TraitDecl>(D)) {
      TD = TraitD;
    }
    assert(TD && "No corresponding trait");

    RecordDecl *TraitVtableRD = Actions.ActOnDesugarVtableRecord(TD);
    RecordDecl *TraitRD = Actions.ActOnDesugarTraitRecord(TD, TraitVtableRD);
    TD->setTrait(TraitRD);
    TD->setVtable(TraitVtableRD);
  }
}

/// ParseImplTraitDeclaration - Parse ImplTraitDecl, for example:
/// "impl trait T for int;"
/// or "impl trait Future<T> for int;"
Parser::DeclGroupPtrTy Parser::ParseImplTraitDeclaration() {
  ConsumeToken(); // Eat the "impl"
  SourceLocation TraitLoc = Tok.getLocation();
  TraitDecl *Trait = nullptr;
  IdentifierInfo *TraitII = nullptr;

  if (Tok.is(tok::kw_trait) && PP.LookAhead(0).is(tok::identifier)) {
    TraitII = PP.LookAhead(0).getIdentifierInfo();
    DeclContext::lookup_result Decls =
        Actions.getASTContext().getTranslationUnitDecl()->lookup(TraitII);
    for (DeclContext::lookup_result::iterator I = Decls.begin(),
                                              E = Decls.end();
         I != E; ++I) {
      if (isa<TraitDecl>(*I)) {
        Trait = dyn_cast<TraitDecl>(*I);
      } else if (isa<TraitTemplateDecl>(*I)) {
        TraitTemplateDecl *TTD = dyn_cast<TraitTemplateDecl>(*I);
        Trait = TTD->getTemplatedDecl();
      }
    }
  } else { // for typedef
    ParsedType TypeRep = Actions.getTypeName(
        *Tok.getIdentifierInfo(), Tok.getLocation(), getCurScope(), nullptr,
        false, false, nullptr, false, false, true);
    if (TypeRep) {
      QualType QT = TypeRep.get().getCanonicalType();
      if (TagDecl *TD = QT.getTypePtr()->getAsTagDecl()) {
        Trait = dyn_cast<TraitDecl>(TD);
        TraitII = Trait->getIdentifier();
      }
    }
  }

  if (!Trait) {
    Diag(Tok.getLocation(), diag::err_unexpected_token_for_impl_trait_decl);
    SkipUntil(tok::semi);
    return nullptr;
  }
  ParsingDeclSpec TraitDS(*this);
  ParseDeclarationSpecifiers(TraitDS);
  ParsedAttributes EmptyDeclSpecAttrs(AttrFactory);
  ParsingDeclarator TraitDeclarator(*this, TraitDS, EmptyDeclSpecAttrs,
                                    DeclaratorContext::File);

  if (Tok.is(tok::kw_for))
    ConsumeToken(); // eat token kw_for
  else {
    Diag(Tok.getLocation(), diag::err_unexpected_token_for_impl_trait_decl);
    SkipUntil(tok::semi);
    return nullptr;
  }

  ParsingDeclSpec TypeDS(*this);
  ParseDeclarationSpecifiers(TypeDS);
  ParsingDeclarator TypeDeclarator(*this, TypeDS, EmptyDeclSpecAttrs,
                                   DeclaratorContext::File);
  ExpectAndConsumeSemi(diag::err_expected_semi_declaration);

  TraitDeclarator.SetIdentifier(TraitII, TraitLoc);
  TraitDeclarator.SetRangeEnd(TraitLoc);

  SmallVector<Decl *, 8> DeclsInGroup;
  // Step 1: Always produce ImplTraitDecl
  ImplTraitDecl *ITD = Actions.BuildImplTraitDecl(getCurScope(), TypeDeclarator,
                                                  TraitDeclarator, Trait);

  // Step 2: Desugar the ImplTraitDecl if we see it for the first time.
  // @code
  // impl trait TR for int;
  // impl trait TR for int; // OK, but useless, we don't desugar this line.
  // @endcode
  if (TraitDS.getTypeSpecType() == TST_error ||
      TypeDS.getTypeSpecType() == TST_error)
    return Actions.FinalizeDeclaratorGroup(getCurScope(), TypeDS, DeclsInGroup);

  VarDecl *DesugaredDecl = Actions.DesugarImplTrait(
      ITD, TypeDeclarator, TraitDeclarator, TypeDS.getBeginLoc());
  if (DesugaredDecl == nullptr) {
    return Actions.FinalizeDeclaratorGroup(getCurScope(), TypeDS, DeclsInGroup);
  }
  Actions.FinalizeDeclaration(DesugaredDecl);
  TypeDeclarator.complete(DesugaredDecl);
  DeclsInGroup.push_back(DesugaredDecl);

  return Actions.FinalizeDeclaratorGroup(getCurScope(), TypeDS, DeclsInGroup);
}

#endif