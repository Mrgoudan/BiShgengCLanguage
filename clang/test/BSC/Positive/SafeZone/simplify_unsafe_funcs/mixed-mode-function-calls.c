// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function calls (Manual sections 8.1-8.2)

// Mixed mode declarations
unsafe void func1(void);
safe void func1(void);

unsafe int add(int a, int b);
safe int add(int a, int b);

// Manual 2531-2541: Safe context can call if safe declaration exists
safe void test_safe_context1(void) {
  // ok: safe version exists
  func1();

  int x = add(1, 2);
  (void)x;
}

// Manual 2543-2561: Non-safe context with overload resolution
unsafe void multi_param(int a, int b);
safe void multi_param(int a, int b);

void test_unsafe_context(void) {
  // ok: can call either version
  func1();
  multi_param(1, 2);
}

// Within explicit unsafe block
safe void test_explicit_unsafe_block(void) {
  unsafe {
    // ok: unsafe context can call functions
    func1();
    multi_param(3, 4);
  }
}

// Non-pointer parameters
unsafe float compute(float x);
safe float compute(float x);

safe void test_non_pointer(void) {
  float result = compute(3.14f);
  (void)result;
}

// Multiple declarations, multiple calls
unsafe void process(int x);
safe void process(int x);

safe void test_multiple_calls(void) {
  process(1);
  process(2);
  process(3);
}

// Nested contexts
safe void test_nested(void) {
  // Safe context
  func1();  // ok

  unsafe {
    // Unsafe context within safe
    func1();  // ok

    safe {
      // Safe context again
      func1();  // ok
    }
  }
}

// expected-no-diagnostics
