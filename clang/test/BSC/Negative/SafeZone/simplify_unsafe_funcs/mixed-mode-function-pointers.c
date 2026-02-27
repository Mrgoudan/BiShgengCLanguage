// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for mixed mode function pointers (Manual section 9)
// Tests the INVALID cases that should produce errors

_Safe void safe_func(void);
_Unsafe void unsafe_func(void);

void test_safe_pointer_requires_safe_function(void) {
  // Error: Assigning _Unsafe function to _Safe pointer is forbidden (narrowing)
  _Safe void (*safe_ptr)(void) = nullptr;
  safe_ptr = unsafe_func; // expected-error {{conversion from type 'void (*)(void)' to '_Safe void (*)(void)' is forbidden}}
}