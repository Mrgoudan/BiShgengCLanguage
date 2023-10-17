def invalid_param_for_trait_member : Error<"the first parameter of trait member function must be 'This* this'">;
def err_unexpected_token_for_impl_trait_decl : Error<"unexpected token for ImplTraitDecl">;
def err_expected_identifier_after_trait : Error<"expected identifier after trait">;
def err_trait_def_not_at_top_level : Error<"trait is only allowed to be defined at top-level">;
def err_invalid_trait : Error<"invalid syntax, semi not expected after 'trait %0'">;