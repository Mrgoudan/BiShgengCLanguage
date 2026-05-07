// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function pointers (Manual section 9)
// Tests the VALID cases that should compile without errors. Non-safe
// incompatible function pointer conversions follow C and produce warnings.

// Mixed mode declarations
_Unsafe void func1(void);
_Safe void func1(void);

_Unsafe int compute(int x);
_Safe int compute(int x);

_Safe void safe_only_func(void);
_Unsafe void unsafe_only_func(void);

void test_valid_assignments(void) {
  // Valid: unqualified pointers accept both _Safe and _Unsafe functions
  void (*ptr1)(void) = safe_only_func;    // expected-warning {{incompatible function pointer types initializing 'void (*)(void)' with an expression of type '_Safe void (void)'}}
  void (*ptr2)(void) = unsafe_only_func;  // OK: _Unsafe -> unqualified
  ptr2 = safe_only_func;                  // expected-warning {{incompatible function pointer types assigning to 'void (*)(void)' from '_Safe void (void)'}}

  // Valid: _Unsafe pointers accept both _Safe and _Unsafe functions
  _Unsafe void (*unsafe_ptr)(void) = nullptr;
  unsafe_ptr = unsafe_only_func;          // OK: _Unsafe -> _Unsafe
  unsafe_ptr = safe_only_func;            // expected-warning {{incompatible function pointer types assigning to 'void (*)(void)' from '_Safe void (void)'}}

  // Valid: _Safe pointers accept _Safe functions
  _Safe void (*safe_ptr)(void) = nullptr;
  safe_ptr = safe_only_func;              // OK: _Safe -> _Safe

  (void)ptr1;
  (void)ptr2;
  (void)unsafe_ptr;
  (void)safe_ptr;
}
