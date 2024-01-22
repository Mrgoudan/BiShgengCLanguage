// BSC instance member warnings and errors.
def note_no_this_parameter : Note<"found the following member function %0 which doesn't have a 'this' parameter">;
def err_no_instance_member : Error<
  "no instance member named %0 in %1; did you mean to use '::' instead of '%select{.|->}2'?">;
def err_this_type_unsupported : Error<
  "type of parameter 'this' is wrong, expected 'This*' or '%0*', but found '%1'">;

// BSC attributes warnings and errors.
def warn_attribute_no_pointers : Warning<
  "(EnhanceC) %0 attribute applied to function with no pointer arguments">,
  InGroup<IgnoredAttributes>;
def err_attribute_redundant_pointers : Error<
  "(EnhanceC) %0 attribute applied to function with more than one pointer arguments, "
  "please specify the index argument in the %0 attribute">;
def warn_attribute_function_pointer : Warning<
  "(EnhanceC) %0 attribute applied to function and function type pointer">,
  InGroup<IgnoredAttributes>;
def err_attribute_zero_pointer : Error<
  "(EnhanceC) %0 attribute: The number of pointer arguments index cannot be zero">;

// BSC async/await warnings and errors.
def err_async_non_function : Error<
  "'async' can only appear on functions">;
def err_async_main : Error<"'main' is not allowed to be declared async">;
def err_await_invalid_scope : Error<
  "await expression is not allowed to appear in %0">;
def err_async_func_unsupported : Error<
  "async function does not support %0">;
def err_not_a_async_call : Error<
  "the await expression's external function declaration must be modified by async or return 'Future' type">;
def err_function_not_found : Error <
  "'%0' was not found, you need to include %1 before using the 'async' keyword">;
def err_await_expression_not_found: Error <
  "await expression was not found in async function">;
def err_invalid_async_function : Error <
  "function which returns 'Future' type should not be modified by 'async'">;

// BSC ownership warnings and errors.
def err_owned_inderictOwned_type_check : Error<
  "%select{"
  "type of %1 cannot be 'owned' |"
  "type of %1 cannot be 'owned', %2 contains 'owned' type |"
  "type of %1 cannot be 'owned'(even indirectly), %2 contains 'owned' type}0">;
def err_owned_decl: Error<"%0 cannot be qualified by 'owned'">;
def err_owned_funcPtr_incompatible : Error<"cannot cast %0 to %1">;
def err_owned_qualcheck_incompatible : Error<
  "incompatible owned types, cannot cast %0 to %1">;
def err_typecheck_invalid_owned_binOp : Error<
  "invalid operands to binary expression (%0 and %1)">;
def err_typecheck_invalid_owned_unaOp : Error<
  "invalid operands to unary expression (%0)">;
def err_typecheck_invalid_owned_arrsub : Error<
  "owned pointer type (%0) do not support ArraySubscript operate">;
def err_owned_temporary_memLeak : Error<
  "memory leak because temporary variable '%0' is owned or indirect owned type, please fix it">;


// BSC trait warnings and errors.
def err_variables_not_trait_pointer : Error<"only trait pointer type is allowed to be declared">;
def err_trait_impl : Error<"function %0 in %1 is not implemented for %2">;
def err_trait_impl_function_type_conflict : Error<
  "type of %0 conflicts with that of %1">;
def err_type_has_not_impl_trait : Error<
  "expected a pointer type which has implemented %0, found %1">;
def err_trait_member_not_func : Error<
  "only function declarations can be trait member">;
def warn_type_has_not_impl_trait : Warning<
  "expected a pointer type which has implemented %0, found %1">,
  InGroup<BSCTraitMissing>;
def err_trait_is_undefined : Error<"%0 must be defined before it can be used">;

// BSC generic warnings and errors.
def err_invalid_struct_definition : Error <
  "struct definition in generic fuction is not supported in BSC">;

// BSC constexpr warnings and errors.
def err_constexpr_func_unsupported : Error<
  "BSC constexpr function does not support %0">;
def err_constexpr_func_unsupported_type : Error<
  "BSC constexpr function does not support type %0">;
def err_constexpr_var_unsupported_type : Error<
  "BSC constexpr variable does not support type %0">;