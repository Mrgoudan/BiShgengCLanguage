// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function pointers (Manual section 9)
// Tests the VALID cases that should compile without errors

// Mixed mode declarations
unsafe void func1(void);
safe void func1(void);

unsafe int compute(int x);
safe int compute(int x);

safe void safe_only_func(void);
unsafe void unsafe_only_func(void);

void test_valid_assignments(void) {
  // Valid: unqualified pointers accept both safe and unsafe functions
  void (*ptr1)(void) = safe_only_func;    // OK: safe -> unqualified (widening)
  void (*ptr2)(void) = unsafe_only_func;  // OK: unsafe -> unqualified
  ptr2 = safe_only_func;                  // OK: safe -> unqualified (widening)

  // Valid: unsafe pointers accept both safe and unsafe functions
  unsafe void (*unsafe_ptr)(void) = nullptr;
  unsafe_ptr = unsafe_only_func;          // OK: unsafe -> unsafe
  unsafe_ptr = safe_only_func;            // OK: safe -> unsafe (widening)

  // Valid: safe pointers accept safe functions
  safe void (*safe_ptr)(void) = nullptr;
  safe_ptr = safe_only_func;              // OK: safe -> safe

  (void)ptr1;
  (void)ptr2;
  (void)unsafe_ptr;
  (void)safe_ptr;
}

// expected-no-diagnostics
