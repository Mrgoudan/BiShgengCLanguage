// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for mixed mode function pointers (Manual section 9)
// Tests the INVALID cases that should produce errors

_Safe void safe_func(void);
_Unsafe void unsafe_func(void);

void test_safe_pointer_requires_safe_function(void) {
  // Error: Assigning _Unsafe function to _Safe pointer is forbidden (narrowing)
  _Safe void (*safe_ptr)(void) = nullptr;
  safe_ptr = unsafe_func; // expected-warning {{incompatible function pointer types assigning to '_Safe void (*)(void)' from 'void (void)'}}
}

_Safe void test_typedef_function_pointer_assign(void) {
  typedef _Safe void (*SFT)(void);
  typedef void (*UFT)(void);
  SFT fp1 = unsafe_func; // expected-error {{incompatible function pointer types initializing 'SFT' (aka 'void (*)(void)') with an expression of type 'void (void)'}}
  SFT fp2 = safe_func;
  UFT fp3 = unsafe_func;
  UFT fp4 = safe_func; // expected-error {{incompatible function pointer types initializing 'UFT' (aka 'void (*)(void)') with an expression of type '_Safe void (void)'}}
  SFT fp5 = &unsafe_func; // expected-error {{incompatible function pointer types initializing 'SFT' (aka 'void (*)(void)') with an expression of type 'void (*)(void)'}}
  SFT fp6 = &safe_func;
  UFT fp7 = &unsafe_func;
  UFT fp8 = &safe_func; // expected-error {{incompatible function pointer types initializing 'UFT' (aka 'void (*)(void)') with an expression of type '_Safe void (*)(void)'}}
  (void) fp1;
  (void) fp2;
  (void) fp3;
  (void) fp4;
  (void) fp5;
  (void) fp6;
  (void) fp7;
  (void) fp8;
}
