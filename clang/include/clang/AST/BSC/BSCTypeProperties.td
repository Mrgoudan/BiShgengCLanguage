let Class = TraitType in {
  def : Creator<[{
    auto trait = cast<TraitDecl>(declaration);
    QualType result = ctx.getTraitType(trait);
    if (dependent)
      const_cast<Type *>(result.getTypePtr())
          ->addDependence(TypeDependence::DependentInstantiation);
    return result;
  }]>;
}

let Class = InjectedTraitNameType in {
  def : Property<"declaration", DeclRef> {
    // FIXME: drilling down to the canonical declaration is what the
    // existing serialization code was doing, but it's not clear why.
    let Read = [{ node->getDecl()->getCanonicalDecl() }];
  }
  def : Property<"injectedSpecializationType", QualType> {
    let Read = [{ node->getInjectedSpecializationType() }];
  }

  def : Creator<[{
    // FIXME: ASTContext::getInjectedTraitNameType is not currently suitable
    // for AST reading, too much interdependencies.
    const Type *T = nullptr;
    auto typeDecl = cast<TraitDecl>(declaration);
    for (auto *DI = typeDecl; DI; DI = DI->getPreviousDecl()) {
      if (const Type *existing = DI->getTypeForDecl()) {
        T = existing;
        break;
      }
    }
    if (!T) {
      T = new (ctx, TypeAlignment)
            InjectedTraitNameType(typeDecl, injectedSpecializationType);
      for (auto *DI = typeDecl; DI; DI = DI->getPreviousDecl())
        DI->setTypeForDecl(T);
    }
    return QualType(T, 0);
  }]>;
}