// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function calls (Manual sections 8.1-8.2)

// Mixed mode declarations
_Unsafe void func1(void);
_Safe void func1(void);

_Unsafe int add(int a, int b);
_Safe int add(int a, int b);

// Manual 2531-2541: Safe context can call if _Safe declaration exists
_Safe void test_safe_context1(void) {
  // ok: _Safe version exists
  func1();

  int x = add(1, 2);
  (void)x;
}

// Manual 2543-2561: Non-_Safe context with overload resolution
_Unsafe void multi_param(int a, int b);
_Safe void multi_param(int a, int b);

void test_unsafe_context(void) {
  // ok: can call either version
  func1();
  multi_param(1, 2);
}

// Within explicit _Unsafe block
_Safe void test_explicit_unsafe_block(void) {
  _Unsafe {
    // ok: _Unsafe context can call functions
    func1();
    multi_param(3, 4);
  }
}

// Non-pointer parameters
_Unsafe float compute(float x);
_Safe float compute(float x);

_Safe void test_non_pointer(void) {
  float result = compute(3.14f);
  (void)result;
}

// Multiple declarations, multiple calls
_Unsafe void process(int x);
_Safe void process(int x);

_Safe void test_multiple_calls(void) {
  process(1);
  process(2);
  process(3);
}

// Nested contexts
_Safe void test_nested(void) {
  // Safe context
  func1();  // ok

  _Unsafe {
    // Unsafe context within _Safe
    func1();  // ok

    _Safe {
      // Safe context again
      func1();  // ok
    }
  }
}

// expected-no-diagnostics
