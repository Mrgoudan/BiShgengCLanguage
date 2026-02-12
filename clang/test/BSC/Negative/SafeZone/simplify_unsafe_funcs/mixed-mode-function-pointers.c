// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for mixed mode function pointers (Manual section 9)
// Tests the INVALID cases that should produce errors

safe void safe_func(void);
unsafe void unsafe_func(void);

void test_safe_pointer_requires_safe_function(void) {
  // Error: Assigning unsafe function to safe pointer is forbidden (narrowing)
  safe void (*safe_ptr)(void) = nullptr;
  safe_ptr = unsafe_func; // expected-error {{conversion from type 'void (*)(void)' to 'safe void (*)(void)' is forbidden}}
}