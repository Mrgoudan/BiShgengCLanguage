// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Tests for mixed mode function calls in various contexts

// Basic calls
unsafe int compute(int x);
safe int compute(int x);

void test_basic(void) {
  int result = compute(42);
  (void)result;
}

safe void test_from_safe_context(void) {
  int result = compute(42);
  (void)result;
}

// Multiple parameters
unsafe void multi_param(int a, char b, float c);
safe void multi_param(int a, char b, float c);

void test_multi_params(void) {
  multi_param(1, 'a', 3.14f);
}

// Return types
unsafe float get_float(void);
safe float get_float(void);

void test_return_type(void) {
  float f = get_float();
  (void)f;
}

// Safe-only and unsafe-only functions
safe void safe_only(void);
unsafe void unsafe_only(void);

void test_safe_from_unsafe(void) {
  safe_only();
}

void test_unsafe_from_unsafe(void) {
  unsafe_only();
}

// Calls in unsafe blocks
safe void test_in_unsafe_block(void) {
  unsafe {
    compute(10);
  }
}

// Recursive calls
unsafe int factorial_unsafe(int n);
safe int factorial_safe(int n);

unsafe int factorial_unsafe(int n) {
  if (n <= 1) return 1;
  return n * factorial_unsafe(n - 1);
}

safe int factorial_safe(int n) {
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
unsafe int even(int n);
unsafe int odd(int n);
safe int even(int n);
safe int odd(int n);

unsafe int even(int n) {
  if (n == 0) return 1;
  return odd(n - 1);
}

unsafe int odd(int n) {
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
unsafe int step1(int x);
safe int step1(int x);

unsafe int step2(int x);
safe int step2(int x);

unsafe int step3(int x);
safe int step3(int x);

void test_chained(void) {
  int result = step3(step2(step1(10)));
  (void)result;
}

// Conditional expressions
unsafe int check_condition(void);
safe int check_condition(void);

unsafe int action_a(void);
safe int action_a(void);

unsafe int action_b(void);
safe int action_b(void);

void test_conditional(void) {
  int result = check_condition() ? action_a() : action_b();
  (void)result;
}

// Loop conditions
unsafe int has_more(void);
safe int has_more(void);

unsafe void process_next(void);
safe void process_next(void);

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
unsafe int get_code(void);
safe int get_code(void);

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
unsafe long get_long(void);
safe long get_long(void);

void test_cast(void) {
  int x = (int)get_long();
  (void)x;
}

// Compound assignments
unsafe int increment(void);
safe int increment(void);

void test_compound_assignment(void) {
  int counter = 0;
  counter += increment();
  counter -= increment();
  counter *= increment();
  counter /= increment();
  (void)counter;
}

// Forward declarations
unsafe int forward_declared(int x);
safe int forward_declared(int x);

void test_forward(void) {
  int result = forward_declared(40);
  (void)result;
}

unsafe int forward_declared(int x) { return x; }

// Static functions
static unsafe int static_func(int x);
static safe int static_func(int x);
static unsafe int static_func(int x) { return x + 1; }

void test_static(void) {
  int result = static_func(50);
  (void)result;
}

// Inline functions
unsafe inline int inline_compute(int x) { return x * 2; }

void test_inline(void) {
  int result = inline_compute(25);
  (void)result;
}

// Comma expressions
unsafe int comma1(void);
safe int comma1(void);

unsafe int comma2(void);
safe int comma2(void);

unsafe int comma3(void);
safe int comma3(void);

void test_comma(void) {
  int result = (comma1(), comma2(), comma3());
  (void)result;
}

// Ternary operator
unsafe int ternary_a(void);
safe int ternary_a(void);

unsafe int ternary_b(void);
safe int ternary_b(void);

void test_ternary(int condition) {
  int result = condition ? ternary_a() : ternary_b();
  (void)result;
}

// Complex nested expressions
unsafe int op1(int x);
safe int op1(int x);

unsafe int op2(int x);
safe int op2(int x);

unsafe int op3(int x);
safe int op3(int x);

void test_complex(void) {
  int result = op1(op2(10) + op3(20)) * op2(op1(5));
  (void)result;
}

// Array subscripts
unsafe int get_index(void);
safe int get_index(void);

void test_array_subscript(void) {
  int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  int value = arr[get_index()];
  (void)value;
}

// Struct member access
struct Container {
  int value;
};

unsafe struct Container get_container(void);
safe struct Container get_container(void);

void test_member_access(void) {
  int val = get_container().value;
  (void)val;
}

// Function arguments
unsafe int producer(void);
safe int producer(void);

unsafe void consumer(int x);
safe void consumer(int x);

void test_as_argument(void) {
  consumer(producer());
}

// Return statements
unsafe int get_return_value(void);
safe int get_return_value(void);

int test_return(void) {
  return get_return_value();
}

// Initializers
unsafe int get_init_value(void);
safe int get_init_value(void);

void test_initializer(void) {
  int x = get_init_value();
  int arr[] = {get_init_value(), get_init_value()};
  (void)x;
  (void)arr;
}

// Calls within safe/unsafe blocks
unsafe int block_func(void);
safe int block_func(void);

void test_nested_blocks(void) {
  safe {
    unsafe {
      int x = block_func();
      (void)x;
    }
  }
}

safe void test_in_safe(void) {
  int x = block_func();
  (void)x;
}

// Extern declarations
extern unsafe int external_func(int x);
extern safe int external_func(int x);

void test_extern(void) {
  int result = external_func(100);
  (void)result;
}

// Macro expansions
unsafe int macro_func(int x);
safe int macro_func(int x);

#define CALL_MACRO_FUNC(x) macro_func(x)

void test_macro(void) {
  int result = CALL_MACRO_FUNC(123);
  (void)result;
}

// Multiple calls in one statement
unsafe int multi_call_1(void);
safe int multi_call_1(void);

unsafe int multi_call_2(void);
safe int multi_call_2(void);

unsafe int multi_call_3(void);
safe int multi_call_3(void);

void test_multiple_calls(void) {
  int result = multi_call_1() + multi_call_2() + multi_call_3();
  (void)result;
}

// expected-no-diagnostics
