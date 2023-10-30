def err_variables_not_trait_pointer : Error<"only trait pointer type is allowed to be declared">;
def err_trait_impl : Error<"function %0 in %1 is not implemented for %2">;
def err_trait_impl_function_type_conflict : Error<
  "type of %0 conflicts with that of %1">;
def err_type_has_not_impl_trait : Error<
  "expected a pointer type which has implemented %0, found %1">;
def err_trait_member_not_func : Error<
  "only function declarations can be trait member">;
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
def err_invalid_struct_definition : Error <
  "struct definition in generic fuction is not supported in BSC">;
def warn_type_has_not_impl_trait : Warning<
  "expected a pointer type which has implemented %0, found %1">;