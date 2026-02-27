// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Tests for mixed mode function calls in various contexts

// Basic calls
_Unsafe int compute(int x);
_Safe int compute(int x);

void test_basic(void) {
  int result = compute(42);
  (void)result;
}

_Safe void test_from_safe_context(void) {
  int result = compute(42);
  (void)result;
}

// Multiple parameters
_Unsafe void multi_param(int a, char b, float c);
_Safe void multi_param(int a, char b, float c);

void test_multi_params(void) {
  multi_param(1, 'a', 3.14f);
}

// Return types
_Unsafe float get_float(void);
_Safe float get_float(void);

void test_return_type(void) {
  float f = get_float();
  (void)f;
}

// Safe-only and _Unsafe-only functions
_Safe void safe_only(void);
_Unsafe void unsafe_only(void);

void test_safe_from_unsafe(void) {
  safe_only();
}

void test_unsafe_from_unsafe(void) {
  unsafe_only();
}

// Calls in _Unsafe blocks
_Safe void test_in_unsafe_block(void) {
  _Unsafe {
    compute(10);
  }
}

// Recursive calls
_Unsafe int factorial_unsafe(int n);
_Safe int factorial_safe(int n);

_Unsafe int factorial_unsafe(int n) {
  if (n <= 1) return 1;
  return n * factorial_unsafe(n - 1);
}

_Safe int factorial_safe(int n) {
  if (n <= 1) return 1;
  return n * factorial_safe(n - 1);
}

void test_recursive(void) {
  int a = factorial_unsafe(5);
  int b = factorial_safe(5);
  (void)a;
  (void)b;
}

// Mutually recursive
_Unsafe int even(int n);
_Unsafe int odd(int n);
_Safe int even(int n);
_Safe int odd(int n);

_Unsafe int even(int n) {
  if (n == 0) return 1;
  return odd(n - 1);
}

_Unsafe int odd(int n) {
  if (n == 0) return 0;
  return even(n - 1);
}

void test_mutual_recursion(void) {
  int a = even(10);
  int b = odd(11);
  (void)a;
  (void)b;
}

// Chained calls
_Unsafe int step1(int x);
_Safe int step1(int x);

_Unsafe int step2(int x);
_Safe int step2(int x);

_Unsafe int step3(int x);
_Safe int step3(int x);

void test_chained(void) {
  int result = step3(step2(step1(10)));
  (void)result;
}

// Conditional expressions
_Unsafe int check_condition(void);
_Safe int check_condition(void);

_Unsafe int action_a(void);
_Safe int action_a(void);

_Unsafe int action_b(void);
_Safe int action_b(void);

void test_conditional(void) {
  int result = check_condition() ? action_a() : action_b();
  (void)result;
}

// Loop conditions
_Unsafe int has_more(void);
_Safe int has_more(void);

_Unsafe void process_next(void);
_Safe void process_next(void);

void test_loops(void) {
  while (has_more()) {
    process_next();
  }

  for (int i = 0; i < has_more(); i++) {
    process_next();
  }

  do {
    process_next();
  } while (has_more());
}

// Switch statements
_Unsafe int get_code(void);
_Safe int get_code(void);

void test_switch(void) {
  switch (get_code()) {
    case 1:
      process_next();
      break;
    case 2:
      action_a();
      break;
    default:
      action_b();
      break;
  }
}

// Casts
_Unsafe long get_long(void);
_Safe long get_long(void);

void test_cast(void) {
  int x = (int)get_long();
  (void)x;
}

// Compound assignments
_Unsafe int increment(void);
_Safe int increment(void);

void test_compound_assignment(void) {
  int counter = 0;
  counter += increment();
  counter -= increment();
  counter *= increment();
  counter /= increment();
  (void)counter;
}

// Forward declarations
_Unsafe int forward_declared(int x);
_Safe int forward_declared(int x);

void test_forward(void) {
  int result = forward_declared(40);
  (void)result;
}

_Unsafe int forward_declared(int x) { return x; }

// Static functions
static _Unsafe int static_func(int x);
static _Safe int static_func(int x);
static _Unsafe int static_func(int x) { return x + 1; }

void test_static(void) {
  int result = static_func(50);
  (void)result;
}

// Inline functions
_Unsafe inline int inline_compute(int x) { return x * 2; }

void test_inline(void) {
  int result = inline_compute(25);
  (void)result;
}

// Comma expressions
_Unsafe int comma1(void);
_Safe int comma1(void);

_Unsafe int comma2(void);
_Safe int comma2(void);

_Unsafe int comma3(void);
_Safe int comma3(void);

void test_comma(void) {
  int result = (comma1(), comma2(), comma3());
  (void)result;
}

// Ternary operator
_Unsafe int ternary_a(void);
_Safe int ternary_a(void);

_Unsafe int ternary_b(void);
_Safe int ternary_b(void);

void test_ternary(int condition) {
  int result = condition ? ternary_a() : ternary_b();
  (void)result;
}

// Complex nested expressions
_Unsafe int op1(int x);
_Safe int op1(int x);

_Unsafe int op2(int x);
_Safe int op2(int x);

_Unsafe int op3(int x);
_Safe int op3(int x);

void test_complex(void) {
  int result = op1(op2(10) + op3(20)) * op2(op1(5));
  (void)result;
}

// Array subscripts
_Unsafe int get_index(void);
_Safe int get_index(void);

void test_array_subscript(void) {
  int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  int value = arr[get_index()];
  (void)value;
}

// Struct member access
struct Container {
  int value;
};

_Unsafe struct Container get_container(void);
_Safe struct Container get_container(void);

void test_member_access(void) {
  int val = get_container().value;
  (void)val;
}

// Function arguments
_Unsafe int producer(void);
_Safe int producer(void);

_Unsafe void consumer(int x);
_Safe void consumer(int x);

void test_as_argument(void) {
  consumer(producer());
}

// Return statements
_Unsafe int get_return_value(void);
_Safe int get_return_value(void);

int test_return(void) {
  return get_return_value();
}

// Initializers
_Unsafe int get_init_value(void);
_Safe int get_init_value(void);

void test_initializer(void) {
  int x = get_init_value();
  int arr[] = {get_init_value(), get_init_value()};
  (void)x;
  (void)arr;
}

// Calls within _Safe/_Unsafe blocks
_Unsafe int block_func(void);
_Safe int block_func(void);

void test_nested_blocks(void) {
  _Safe {
    _Unsafe {
      int x = block_func();
      (void)x;
    }
  }
}

_Safe void test_in_safe(void) {
  int x = block_func();
  (void)x;
}

// Extern declarations
extern _Unsafe int external_func(int x);
extern _Safe int external_func(int x);

void test_extern(void) {
  int result = external_func(100);
  (void)result;
}

// Macro expansions
_Unsafe int macro_func(int x);
_Safe int macro_func(int x);

#define CALL_MACRO_FUNC(x) macro_func(x)

void test_macro(void) {
  int result = CALL_MACRO_FUNC(123);
  (void)result;
}

// Multiple calls in one statement
_Unsafe int multi_call_1(void);
_Safe int multi_call_1(void);

_Unsafe int multi_call_2(void);
_Safe int multi_call_2(void);

_Unsafe int multi_call_3(void);
_Safe int multi_call_3(void);

void test_multiple_calls(void) {
  int result = multi_call_1() + multi_call_2() + multi_call_3();
  (void)result;
}

// expected-no-diagnostics
