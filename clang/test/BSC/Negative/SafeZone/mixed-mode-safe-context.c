// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Tests for safe context restrictions with mixed mode

// Unsafe-only cannot be called from safe
unsafe void unsafe_only(void);

safe void test_safe_calls_unsafe(void) {
  unsafe_only();  // expected-error {{unsafe function call is forbidden in the safe zone}}
}

// Multiple unsafe-only calls
unsafe int unsafe_compute(int x);
unsafe float unsafe_process(float y);
unsafe void unsafe_action(void);

safe void test_multiple_unsafe(void) {
  int a = unsafe_compute(10);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)a;

  float b = unsafe_process(3.14f);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)b;

  unsafe_action();  // expected-error {{unsafe function call is forbidden in the safe zone}}
}

// Nested safe contexts
unsafe void nested_unsafe(void);

void test_nested_safe(void) {
  unsafe {
    safe {
      nested_unsafe();  // expected-error {{unsafe function call is forbidden in the safe zone}}
    }
  }
}

safe void test_safe_with_safe_block(void) {
  safe {
    nested_unsafe();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  }
}

// Unsafe in conditionals
unsafe int unsafe_get_value(void);

safe void test_unsafe_conditional(int flag) {
  int result = flag ? unsafe_get_value() : 0;  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Unsafe in loops
unsafe int unsafe_has_more(void);
unsafe void unsafe_process_next(void);

safe void test_unsafe_while(void) {
  while (unsafe_has_more()) {  // expected-error {{unsafe function call is forbidden in the safe zone}}
    unsafe_process_next();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  }
}

safe void test_unsafe_for(void) {
  for (int i = 0; i < unsafe_has_more(); i++) {  // expected-error {{unsafe function call is forbidden in the safe zone}}
    unsafe_process_next();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  }
}

// Unsafe in switch
unsafe int unsafe_get_code(void);

safe void test_unsafe_switch(void) {
  switch (unsafe_get_code()) {  // expected-error {{unsafe function call is forbidden in the safe zone}}
    case 1:
      unsafe_action();  // expected-error {{unsafe function call is forbidden in the safe zone}}
      break;
    default:
      break;
  }
}

// Unsafe with casts
unsafe long unsafe_get_long(void);

safe void test_unsafe_cast(void) {
  int x = (int)unsafe_get_long();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)x;
}

// Unsafe in compound assignments
unsafe int unsafe_increment(void);

safe void test_unsafe_compound(void) {
  int counter = 0;
  counter += unsafe_increment();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  counter -= unsafe_increment();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)counter;
}

// Unsafe in complex expressions
unsafe int unsafe_op1(int x);
unsafe int unsafe_op2(int x);

safe void test_unsafe_complex(void) {
  int result = unsafe_op1(10) + unsafe_op2(20);  // expected-error {{unsafe function call is forbidden in the safe zone}} expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Unsafe indirect calls
unsafe int unsafe_compute_fp(int x);

safe void test_unsafe_indirect(void) {
  int (*fp)(int) = unsafe_compute_fp;
  int result = fp(42);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Unsafe in macros
unsafe int unsafe_macro_func(int x);

#define CALL_UNSAFE_MACRO(x) unsafe_macro_func(x)

safe void test_unsafe_macro(void) {
  int result = CALL_UNSAFE_MACRO(123);  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Unsafe in array subscripts
unsafe int unsafe_get_index(void);

safe void test_unsafe_subscript(void) {
  int arr[10] = {0};
  int value = arr[unsafe_get_index()];  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)value;
}

// Unsafe in pointer arithmetic
unsafe int unsafe_get_offset(void);

safe void test_unsafe_ptr_arith(void) {
  int arr[10] = {0};
  int* p = arr + unsafe_get_offset();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)p;
}

// Unsafe in sizeof (still checked)
unsafe int unsafe_sizeof_func(void);

safe void test_unsafe_sizeof(void) {
  int size = sizeof(unsafe_sizeof_func());  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)size;
}

// Unsafe as arguments
safe void safe_consumer(int x);
unsafe int unsafe_producer(void);

safe void test_unsafe_argument(void) {
  safe_consumer(unsafe_producer());  // expected-error {{unsafe function call is forbidden in the safe zone}}
}

// Unsafe in return
unsafe int unsafe_return_value(void);

safe int test_unsafe_return(void) {
  return unsafe_return_value();  // expected-error {{unsafe function call is forbidden in the safe zone}}
}

// Unsafe in initializers
unsafe int unsafe_initializer_value(void);

safe void test_unsafe_initializer(void) {
  int x = unsafe_initializer_value();  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)x;
}

// Unsafe in struct member access
struct Container {
  int value;
};

unsafe struct Container unsafe_get_container(void);

safe void test_unsafe_member(void) {
  int val = unsafe_get_container().value;  // expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)val;
}

// Chained unsafe calls
unsafe int unsafe_chain1(int x);
unsafe int unsafe_chain2(int x);
unsafe int unsafe_chain3(int x);

safe void test_chained_unsafe(void) {
  int result = unsafe_chain3(unsafe_chain2(unsafe_chain1(10)));  // expected-error {{unsafe function call is forbidden in the safe zone}} expected-error {{unsafe function call is forbidden in the safe zone}} expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Unsafe in comma expressions
unsafe int unsafe_comma1(void);
unsafe int unsafe_comma2(void);

safe void test_unsafe_comma(void) {
  int result = (unsafe_comma1(), unsafe_comma2());  // expected-error {{unsafe function call is forbidden in the safe zone}} expected-error {{unsafe function call is forbidden in the safe zone}}
  (void)result;
}

// Safe-only functions work
safe void safe_only_func(void);

safe void test_safe_calls_safe(void) {
  safe_only_func();
}

// Mixed mode works
unsafe void mixed_func(void);
safe void mixed_func(void);

safe void test_safe_calls_mixed(void) {
  mixed_func();
}
