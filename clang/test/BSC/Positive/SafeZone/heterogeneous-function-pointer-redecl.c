// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for heterogeneous function pointer typedef redeclarations

// Test 1: Basic heterogeneous typedef redeclaration (unsafe -> safe)
typedef unsafe  int (*FuncPtr1)(int* p);
typedef safe  int (*FuncPtr1)(int* owned p);

// Test 2: Heterogeneous typedef redeclaration (safe -> unsafe)
typedef safe  void (*FuncPtr2)(int* borrow p);
typedef unsafe  void (*FuncPtr2)(int* p);

// Test 3: Return type compatibility (unsafe T* <-> safe T* owned)
typedef unsafe  int* (*FuncPtr3)(void);
typedef safe  int* owned (*FuncPtr3)(void);

// Test 4: Return type compatibility (unsafe T* <-> safe T* borrow)
typedef unsafe  char* (*FuncPtr4)(char* p);
typedef safe  char* borrow (*FuncPtr4)(char* borrow p);

// Test 5: Multiple parameters with heterogeneous redeclaration
typedef unsafe  void (*FuncPtr5)(int* p, float* q);
typedef safe  void (*FuncPtr5)(int* owned p, float* owned q);

// Test 8: Nested pointers
typedef unsafe  int** (*FuncPtr8)(int** p);
typedef safe  int** owned (*FuncPtr8)(int** owned p);

// Test 9: Homogeneous redeclaration (both safe) - should work
typedef safe  void (*FuncPtr9)(int* owned p);
typedef safe  void (*FuncPtr9)(int* owned p);

// Test 10: Homogeneous redeclaration (both unsafe) - should work
typedef unsafe  void (*FuncPtr10)(int* p);
typedef unsafe  void (*FuncPtr10)(int* p);

// Usage test
void test_usage(void) {
    FuncPtr1 fp1 = 0;
    FuncPtr2 fp2 = 0;
    FuncPtr3 fp3 = 0;
    FuncPtr4 fp4 = 0;
    FuncPtr5 fp5 = 0;
    FuncPtr8 fp8 = 0;
    FuncPtr9 fp9 = 0;
    FuncPtr10 fp10 = 0;
}
