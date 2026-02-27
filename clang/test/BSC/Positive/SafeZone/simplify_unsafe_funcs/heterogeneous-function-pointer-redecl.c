// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for heterogeneous function pointer typedef redeclarations

// Test 1: Basic heterogeneous typedef redeclaration (_Unsafe -> _Safe)
typedef _Unsafe  int (*FuncPtr1)(int* p);
typedef _Safe  int (*FuncPtr1)(int* owned p);

// Test 2: Heterogeneous typedef redeclaration (_Safe -> _Unsafe)
typedef _Safe  void (*FuncPtr2)(int* borrow p);
typedef _Unsafe  void (*FuncPtr2)(int* p);

// Test 3: Return type compatibility (_Unsafe T* <-> _Safe T* owned)
typedef _Unsafe  int* (*FuncPtr3)(void);
typedef _Safe  int* owned (*FuncPtr3)(void);

// Test 4: Return type compatibility (_Unsafe T* <-> _Safe T* borrow)
typedef _Unsafe  char* (*FuncPtr4)(char* p);
typedef _Safe  char* borrow (*FuncPtr4)(char* borrow p);

// Test 5: Multiple parameters with heterogeneous redeclaration
typedef _Unsafe  void (*FuncPtr5)(int* p, float* q);
typedef _Safe  void (*FuncPtr5)(int* owned p, float* owned q);

// Test 8: Nested pointers
typedef _Unsafe  int** (*FuncPtr8)(int** p);
typedef _Safe  int** owned (*FuncPtr8)(int** owned p);

// Test 6: void* owned parameter and return
typedef _Unsafe  void* (*FuncPtr6)(void* p);
typedef _Safe  void* owned (*FuncPtr6)(void* owned p);

// Test 7: const char* borrow parameter
typedef _Unsafe  void (*FuncPtr7)(const char* msg);
typedef _Safe  void (*FuncPtr7)(const char* borrow msg);

// Test 9: Homogeneous redeclaration (both _Safe) - should work
typedef _Safe  void (*FuncPtr9)(int* owned p);
typedef _Safe  void (*FuncPtr9)(int* owned p);

// Test 10: Homogeneous redeclaration (both _Unsafe) - should work
typedef _Unsafe  void (*FuncPtr10)(int* p);
typedef _Unsafe  void (*FuncPtr10)(int* p);

// Usage test
void test_usage(void) {
    FuncPtr1 fp1 = 0;
    FuncPtr2 fp2 = 0;
    FuncPtr3 fp3 = 0;
    FuncPtr4 fp4 = 0;
    FuncPtr5 fp5 = 0;
    FuncPtr6 fp6 = 0;
    FuncPtr7 fp7 = 0;
    FuncPtr8 fp8 = 0;
    FuncPtr9 fp9 = 0;
    FuncPtr10 fp10 = 0;
}
