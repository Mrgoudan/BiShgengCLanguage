// RUN: %clang_cc1 -xbsc -fsyntax-only -verify %s

// 30 * 1024^3 is 0x780000000. Truncated to 32-bit int, it becomes 0x80000000 (INT_MIN, -2147483648).
#define OVERFLOW_MUL 30*1024*1024*1024

// 2 * 1024^3 = 2147483648 (0x80000000), which is INT_MIN.
#define OVERFLOW_MUL_2 2*1024*1024*1024

// INT_MAX (2147483647) + 1 = -2147483648 (Overflow).
#define OVERFLOW_ADD 2147483647 + 1

// Test Group 1: Struct Members (Core Scenario)
struct Node {
  char data[OVERFLOW_MUL]; // expected-error {{declared as an array with a negative size}}
};

struct Node2 {
  int arr[OVERFLOW_ADD];   // expected-error {{declared as an array with a negative size}}
};

struct MultiDim {
  // Test multi-dimensional array: First dimension overflow
  int matrix1[OVERFLOW_MUL_2][10]; // expected-error {{declared as an array with a negative size}}

  // Test multi-dimensional array: Second dimension overflow
  int matrix2[10][OVERFLOW_MUL];   // expected-error {{declared as an array with a negative size}}
};

struct Nested {
  struct Inner {
    long field[OVERFLOW_MUL];      // expected-error {{declared as an array with a negative size}}
  } inner;
};

// Test Group 2: Union
union Data {
  char buf[OVERFLOW_MUL]; // expected-error {{declared as an array with a negative size}}
  int number;
};

// Test Group 3: Typedef
// Should report error when defining the type alias.
typedef int BadArrayType[OVERFLOW_MUL]; // expected-error {{declared as an array with a negative size}}

struct UseTypedef {
  // If the typedef above didn't trigger an error, usage here should.
  // (Usually errors at definition, but included for completeness).
  BadArrayType *ptr; 
};

// Test Group 4: Global Variables
int global_array[OVERFLOW_MUL_2]; // expected-error {{declared as an array with a negative size}}

// Test Group 5: Function Local Variables (Safe Function Context)
_Safe void test_stack_allocation(void) {
  int local_arr[OVERFLOW_MUL]; // expected-error {{declared as an array with a negative size}}
}


// Test 6: Comparison with Direct Negative Values
struct DirectNegative {
  int a[-1];            // expected-error {{declared as an array with a negative size}}
  int b[OVERFLOW_MUL];  // expected-error {{declared as an array with a negative size}}
};

// Test 1 << 31 (Usually INT_MIN, implementation defined but typically triggers negative size check).
struct ShiftOverflow {
  int c[1 << 31];       // expected-error {{declared as an array with a negative size}}
};