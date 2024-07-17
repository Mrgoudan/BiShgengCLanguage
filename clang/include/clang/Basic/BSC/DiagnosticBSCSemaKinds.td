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
  "type of %2 cannot qualified by '%1' |"
  "type of %2 cannot qualified by '%1', %3 contains '%1' type |"
  "type of %2 cannot qualified by '%1'(even indirectly), %3 contains '%1' type}0">;
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
def err_funcPtr_incompatible : Error<
  "incompatible borrow function pointer types, cannot cast %0 to %1">;
def err_borrow_template_args : Error<"type of template arguments cannot be qualified by 'borrow'">;
def err_borrow_on_borrow : Error<"%0 on a 'borrow' quialified type is not allowed">;
def err_mut_expr_unmodifiable  : Error<"the expression after '&mut' must be modifiable">;
def err_mut_expr_func : Error<"'&mut' for function pointer is not allowed">;
def err_safe_mut : Error<"global or static variables are not allowed to be mutably borrowed within the safe zone">;
def err_move_borrow : Error<"borrow type does not allow move ownership">;
def err_typecheck_invalid_borrow_not_pointer : Error<"borrow type requires a pointer or reference (%0 is invalid)">;
def err_borrow_qualcheck_incompatible : Error<
  "incompatible borrow types, cannot cast %0 to %1">;
def err_borrow_qualcheck_compare : Error<
  "incompatible borrow types, %0 and %1 cannot be compared">;
def err_typecheck_borrow_func : Error<"no borrow qualified type found in the function parameters, the return type is not allowed to be borrow qualified">;
def err_typecheck_borrow_subscript : Error<"subscript of borrow pointer is not allowed">;


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
def err_extended_type_not_instantiated_type : Error<
  "extended type of a BSC member function cannot be an instantiated type">;

// BSC constexpr warnings and errors.
def err_constexpr_func_unsupported : Error<
  "BSC constexpr function does not support %0">;
def err_constexpr_func_unsupported_type : Error<
  "BSC constexpr function does not support type %0">;
def err_constexpr_var_unsupported_type : Error<
  "BSC constexpr variable does not support type %0">;
def err_constexpr_if_cond_expr_unsupported_type : Error<
  "BSC constexpr if condition expression does not support type %0">;

// BSC safe scope warnings and errors.
def err_unsafe_cast : Error<"conversion from type %0 to %1 is forbidden in the safe zone">;
def err_unsafe_fun_cast : Error<"conversion from type %0 to %1 is forbidden">;
def err_safe_function : Error<"%0 is forbidden in the safe function">;
def err_safe_zone_decl : Error<"%select{|'safe'|'unsafe'}0 can only appear before on function or compound statement"> ;
def err_safe_global_var : Error<"defining mutable global variables is not allowed within the safe zone"> ;


// Ownership Analysis errors.
def err_ownership_read_invalid : Error<
  "%0 cannot be accessed, because it is uninitialized or moved">;
def err_ownership_scope_memory_leak : Error<
  "%0's memory will leak at the end of scope">;
def err_ownership_pointer_inner_moved : Error<
  "%0 cannot be moved because its pointee is moved">;
def err_ownership_pointer_inner_assign : Error<
  "pointer %0 inner assign error because inner is not moved">;
def err_ownership_cast_to_void_invalid : Error <
  "%0 cannot cast to 'void * owned' because inner is not moved">;
def err_ownership_reassign : Error <
  "cannot assign %0 because its type is owned">;
def err_ownership_read_invalid_uninit : Error<
  "cannot read %0, because it is uninitialized">;
def err_ownership_read_invalid_moved : Error<
  "cannot read %0, because it is moved at line: %1">;

