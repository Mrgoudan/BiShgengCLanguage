// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function redeclaration calls (Manual sections 8.1-8.2)

// Safe context requires safe declaration
unsafe void unsafe_only(void);
unsafe void func(void);

safe void test_safe_cannot_call_unsafe(void) {
  unsafe_only();  // expected-error {{unsafe function call is forbidden in the safe zone}}
}

// Multiple unsafe-only functions
unsafe int unsafe_compute(int x);
unsafe float unsafe_process(float y);

safe void test_multiple_unsafe_calls(void) {
  int a = unsafe_compute(5);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)a;

  float b = unsafe_process(3.14f);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)b;
}

// Unsafe call in nested safe context
void test_nested_safe_context(void) {
  unsafe {
    safe {
      unsafe_only();  // expected-error {{unsafe function call is forbidden in the safe zone}}
    }
  }
}

// Safe function trying to call unsafe-only
safe void safe_func_calls_unsafe(void) {
  func();  // expected-error {{unsafe function call is forbidden in the safe zone}}
}
