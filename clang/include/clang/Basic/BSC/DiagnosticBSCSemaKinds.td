// BSC instance member warnings and errors.
def note_no_this_parameter : Note<"found the following member function %0 which doesn't have a 'this' parameter">;
def err_no_instance_member : Error<
  "no instance member named %0 in %1; did you mean to use '::' instead of '%select{.|->}2'?">;
def err_cvrqualified_member_type_unsupported : Error<
  "type of member function is wrong, CVR qualified type is unsupported, but found '%0'">;
def err_this_type_unsupported : Error<
  "type of parameter 'this' is wrong, expected 'This*' or 'This' or '%0*' or '%0', but found '%1'">;
def err_incompatible_pointer_cast : Error<
  "incompatible conversion from pointer type '%0' to non-pointer type '%1' in member function call">;
def err_incompatible_const_cast : Error<
  "incompatible conversion from const type '%0' to non-const type '%1' in member function call">;
def err_incompatible_owned_cast : Error<
  "incompatible conversion from non _Owned type '%0' to _Owned type '%1' in member function call">;

// BSC _Async/_Await warnings and errors.
def err_async_non_function : Error<
  "'_Async' can only appear on functions">;
def err_async_main : Error<"'main' is not allowed to be declared _Async">;
def err_await_invalid_scope : Error<
  "_Await expression is not allowed to appear in %0">;
def err_async_func_unsupported : Error<
  "_Async function does not support %0">;
def err_not_a_async_call : Error<
  "the _Await expression's external function declaration must be modified by _Async or return 'Future' type">;
def err_function_not_found : Error <
  "'%0' was not found, you need to include %1 before using the '_Async' keyword">;
def err_await_expression_not_found: Error <
  "_Await expression was not found in _Async function">;
def err_invalid_async_function : Error <
  "function which returns 'Future' type should not be modified by '_Async'">;

// BSC ownership warnings and errors.
def err_owned_inderictOwned_type_check : Error<
  "%select{"
  "type of %2 cannot qualified by '%1' |"
  "type of %2 cannot qualified by '%1', %3 contains '%1' type |"
  "type of %2 cannot qualified by '%1'(even indirectly), %3 contains '%1' type}0">;
def err_owned_funcPtr_incompatible : Error<"cannot cast %0 to %1">;
def err_owned_qualcheck_incompatible : Error<
  "incompatible _Owned types, cannot cast %0 to %1">;
def err_bsc_move_to_raw_not_owned : Error<
  "argument must be an _Owned pointer type (have %0)">;
def err_bsc_take_from_raw_not_raw : Error<
  "argument must be a raw pointer type (have %0)">;
def err_owned_raw_cast_disallowed : Error<
  "cannot cast between _Owned and raw pointer; use __move_to_raw or __take_from_raw for ownership transfer">;
def err_typecheck_invalid_owned_binOp : Error<
  "invalid operands to binary expression (%0 and %1)">;
def err_typecheck_invalid_owned_unaOp : Error<
  "invalid operands to unary expression (%0)">;
def err_typecheck_invalid_owned_arrsub : Error<
  "_Owned pointer type (%0) do not support ArraySubscript operate">;
def err_owned_temporary_memLeak : Error<
  "memory leak because temporary variable '%0' is _Owned or indirect _Owned type, please fix it">;
def err_funcPtr_incompatible : Error<
  "incompatible _Borrow function pointer types, cannot cast %0 to %1">;
def err_borrow_on_borrow : Error<"%0 on a '_Borrow' qualified type is not allowed">;
def err_mut_expr_unmodifiable  : Error<"the expression after '&_Mut' must be modifiable">;
def err_mut_expr_func : Error<"'&_Mut' for function pointer is not allowed">;
def err_safe_mut : Error<"global or static variables are not allowed to be mutably borrowed within the safe zone">;
def err_move_borrow : Error<"_Borrow type does not allow move ownership">;
def err_typecheck_invalid_borrow_not_pointer : Error<"_Borrow type requires a pointer or reference (%0 is invalid)">;
def err_borrow_qualcheck_incompatible : Error<
  "incompatible _Borrow types, cannot cast %0 to %1">;
def err_borrow_qualcheck_compare : Error<
  "incompatible _Borrow types, %0 and %1 cannot be compared">;
def err_typecheck_borrow_func : Error<"no _Borrow qualified type found in the function parameters, the return type is not allowed to be _Borrow qualified">;
def err_typecheck_borrow_subscript : Error<"subscript of _Borrow pointer is not allowed">;
def err_owned_and_borrow_conflict : Error<"cannot combine '_Owned' and '_Borrow' qualifiers on the same type">;
def err_owned_qualifier_non_pointer : Error<
  "type of %1 cannot be qualified by '%0'">;


// BSC trait warnings and errors.
def err_variables_not_trait_pointer : Error<"only _Trait pointer type is allowed to be declared">;
def err_trait_impl : Error<"function %0 in %1 is not implemented for %2">;
def err_trait_impl_function_type_conflict : Error<
  "type of %0 conflicts with that of %1">;
def err_type_has_not_impl_trait : Error<
  "expected a pointer type which has implemented %0, found %1">;
def err_trait_member_not_func : Error<
  "only function declarations can be _Trait member">;
def warn_type_has_not_impl_trait : Warning<
  "expected a pointer type which has implemented %0, found %1">,
  InGroup<BSCTraitMissing>;
def err_trait_is_undefined : Error<"%0 must be defined before it can be used">;
def err_impl_trait_for_instantiated_type : Error<
  "cannot _Impl _Trait for an instantiated type">;

// BSC generic warnings and errors.
def err_invalid_struct_definition : Error <
  "struct definition in generic fuction is not supported in BSC">;
def err_extended_type_not_instantiated_type : Error<
  "extended type of a BSC member function cannot be an instantiated type">;
def err_conflicting_function_declarations : Error<
  "conflicting function declarations for %0">;
  
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
def err_unsafe_implicit_cast : Error<"implicit conversion from type %0 to %1 is forbidden in the safe zone; please use explicit cast or other means instead">;
def note_unsafe_cast_implicit_conversion : Note<
  "source type %0 is implicit converted from type %1">;
def note_inc_dec_void_in_safe_zone : Note<
  "prefix/postfix '++' and '--' in safe zone produce void; use only for side effect">;
def err_return_inc_dec_void_in_safe_zone : Error<
  "result of '++' or '--' cannot be used as return value in safe zone; move the increment/decrement out of the return statement (or add an explicit cast to void to suppress)">;
def err_unsafe_fun_cast : Error<"conversion from type %0 to %1 is forbidden">;
def note_unsafe_to_safe_function_pointer : Note<
  "assigning an unsafe function pointer to a safe function pointer type is not allowed">;
def err_safe_function : Error<"%0 is forbidden in the _Safe function">;
def err_safe_global_var : Error<"defining mutable global variables is not allowed within the safe zone"> ;
def err_bsc_generic_heterogeneous_redecl : Error<"generic function %0 cannot have both _Safe and _Unsafe declarations">;
def err_bsc_incompatible_heterogeneous_redecl : Error<"function %0 has incompatible _Safe and _Unsafe declarations">;
def err_bsc_no_matching_heterogeneous_function : Error<"no matching function for call to %0; argument types do not match any _Safe or _Unsafe declaration">;

// BSC _Owned struct
def err_invalid_member_use_in_class_method : Error <
  "invalid implicit use of member %0 in struct member function">;
def err_static_class_func : Error<
  "function declared in struct scope cannot have 'static' storage class">;
  def err_need_explicit_constructor_owned_struct : Error<"need explicit constructor because %0 has private field">;
def err_struct_member_redeclared : Error<"struct member cannot be redeclared">;

// BSC Destructor
def invalid_param_for_destructor : Error<"the parameter of destructor must be '%0 this'">;
def invalid_param_num_for_destructor : Error<"destructor can only have one parameter">;
def err_destructor_call : Error<"destructor %0 cannot be called directly">;
def err_assignment_in_destructor : Error<"'this' cannot be moved in destructor">;
def warn_destructor_execute : Warning<"the destructor may be not executed">,
InGroup<BSCOwnedStruct>;
def err_owned_struct_destructor_name : Error<"expected the _Owned struct name after '~' to name the enclosing _Owned struct">;
def err_owned_struct_destructor_body : Error<"destructor must have a function body">;

// BSC Ownership Analysis errors.
// use
def err_ownership_use_moved : Error<
  "use of moved value: `%0`">;
def err_ownership_use_partially_moved : Error<
  "use of partially moved value: `%0`, %1 moved">;
def err_ownership_use_all_moved: Error<
  "use of all moved value: `%0`">;
def err_ownership_use_uninit : Error<
  "use of uninitialized value: `%0`">;
def err_ownership_use_possibly_uninit : Error<
  "use of possibly uninitialized value: `%0`">;

// return value initialization
def err_return_uninit : Error<
  "return value of `%0` is not initialized on all paths">;
def err_return_possibly_uninit : Error<
  "return value of `%0` may not be initialized on all paths">;

// ensure_init contract verification
def err_ensure_init_not_init : Error<
  "'*%0' not initialized at return in __attribute__((ensure_init)) function">;
def err_ensure_init_maybe_not_init : Error<
  "'*%0' may not be initialized on all paths at return in __attribute__((ensure_init)) function">;
def warn_ensure_init_not_addressof : Warning<
  "ensure_init effect cannot be verified when argument is not an address-of expression">,
  InGroup<BSCEnsureInitAttr>;
def err_ensure_init_ptr_aliased : Error<
  "__attribute__((ensure_init)) parameter '%0' cannot be reassigned or aliased before '*%0' is initialized">;
def err_ensure_init_deref_read_uninit : Error<
  "use of uninitialized '*%0' in __attribute__((ensure_init)) function">;
def err_ensure_init_funcptr_incompatible : Error<
  "incompatible function pointer types: target expects __attribute__((ensure_init)) on parameter %0 but source does not have it">;

// __assume_initialized
def err_assume_init_bad_arg : Error<
  "__assume_initialized requires '&' expression as argument">;

// assign
def err_ownership_assign_owned : Error<
  "assign to _Owned value: `%0`">;
def err_ownership_assign_partially_moved : Error<
  "assign to partially moved value: `%0`, %1 moved">;
def err_ownership_assign_possibly_partially_moved : Error<
  "assign to possibly partially moved value: `%0`, %1 possibly moved">;
def err_ownership_assign_all_moved : Error<
  "assign to all moved value: `%0`">;
// field assign
def err_ownership_assign_field_uninit : Error<
  "assign to part of uninitialized value: `%0`">;
def err_ownership_assign_field_owned : Error<
  "assign to part of _Owned value: `%0`">;
def err_ownership_assign_field_moved : Error<
  "assign to part of moved value: `%0`">;
def err_ownership_assign_field_subfield_owned : Error<
  "assign to subfield _Owned value: `%0`, %1 _Owned">;

// cast to `void * _Owned`
def err_ownership_cast_moved : Error<
  "invalid cast to `void * _Owned` of moved or uninitialized value: `%0`">;
def err_ownership_cast_owned : Error<
  "invalid cast to `void * _Owned` of _Owned value: `%0`">;
def err_ownership_cast_uninit: Error<
  "invalid cast to `void * _Owned` of uninit value: `%0`">;
def err_ownership_cast_subfield_owned : Error<
  "invalid cast to `void * _Owned` of not all moved value: `%0`, %1 _Owned">;
// memory leak
def err_ownership_memory_leak_field : Error<
  "field memory leak of value: `%0`, %1 leak">;
def err_ownership_memory_leak : Error<
  "memory leak of value: `%0`">;
def err_ownership_owned_struct_patially_moved : Error<
  "partially moved _Owned struct:`%0` at scope end, %1 moved">;
def err_ownership_owned_struct_not_properly_freed : Error<
  "destructor for`%0` incorrect, %1 of _Owned type and needs to be handled manually">;

// New BSC Borrow Checker errors.
def err_borrow_assign_when_borrowed : Error<
  "cannot assign to `%0` because it is borrowed">;
def err_borrow_move_when_borrowed : Error<
  "cannot move out of `%0` because it is borrowed">;
def err_borrow_use_when_mut_borrowed : Error<
  "cannot use `%0` because it was mutably borrowed">;
def note_borrowed_here : Note<"`%0` is borrowed here">;
def err_borrow_mut_borrow_more_than_once : Error<
  "cannot borrow `%0` as mutable more than once at a time">;
def note_first_mut_borrow_occurs_here : Note<"first mut borrow occurs here">;
def err_borrow_return_local_borrow : Error<
  "cannot return reference to local variable `%0`">;
def err_borrow_not_live_long : Error<
  "`%0` does not live long enough">;
def note_dropped_while_borrowed: Note<"`%0` dropped here while still borrowed">;
def err_borrow_immut_borrow_when_mut_borrowed : Error<
  "cannot borrow `%0` as immutable because it is also borrowed as mutable">;
def note_mutable_borrow_occurs_here : Note<"mutable borrow occurs here">;
def err_borrow_mut_borrow_when_immut_borrowed : Error<
  "cannot borrow `%0` as mutable because it is also borrowed as immutable">;
def note_immutable_borrow_occurs_here : Note<"immutable borrow occurs here">;

// BSC operator overload errors.
def err_unsupport_overload_fun : Error<"function unsupport overload">;
def err_operator_overload_needs_point : Error<
  "overloaded function %0 first parameter or return type must be struct or enumeration pointer type">;

// BSC Pointer Nullability Check errors.
def err_nullable_pointer_dereference : Error<
  "nullable pointer cannot be dereferenced">;
def err_pass_nullable_argument : Error<
  "cannot pass nullable pointer argument">;
def err_return_nullable : Error<
  "cannot return nullable pointer type">;
def err_nullable_cast_nonnull : Error<
  "cannot cast nullable pointer to nonnull type">;
def err_nullable_pointer_access_member : Error<
  "cannot access member through nullable pointer">;
def err_nonnull_assigned_by_nullable : Error<
  "nonnull pointer cannot be assigned by nullable pointer">;
def err_nonnull_init_by_default : Error<
  "`%0` is a _Nonnull pointer and must be properly initialized.">;

// BSC string literal borrow errors.
def err_mut_borrow_string_literal : Error<
  "cannot take mutable borrow of string literal with '&_Mut'; string literals are immutable">;
def err_mut_borrow_string_literal_indirect : Error<
  "cannot take mutable borrow through string literal with '&_Mut *'; string literals are immutable">;
def err_pass_string_literal_to_mut_borrow : Error<
  "cannot pass string literal to parameter of type %0; string literals are immutable">;

