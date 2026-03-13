// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function redeclaration calls (Manual sections 8.1-8.2)

// Safe context requires _Safe declaration
_Unsafe void unsafe_only(void);
_Unsafe void func(void);

_Safe void test_safe_cannot_call_unsafe(void) {
  unsafe_only();  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
}

// Multiple _Unsafe-only functions
_Unsafe int unsafe_compute(int x);
_Unsafe float unsafe_process(float y);

_Safe void test_multiple_unsafe_calls(void) {
  int a = unsafe_compute(5);  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
  (void)a;

  float b = unsafe_process(3.14f);  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
  (void)b;
}

// Unsafe call in nested _Safe context
void test_nested_safe_context(void) {
  _Unsafe {
    _Safe {
      unsafe_only();  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
    }
  }
}

// Safe function trying to call _Unsafe-only
_Safe void safe_func_calls_unsafe(void) {
  func();  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
}
