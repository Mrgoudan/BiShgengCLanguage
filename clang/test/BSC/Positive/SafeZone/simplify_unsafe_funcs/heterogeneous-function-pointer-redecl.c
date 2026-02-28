// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for heterogeneous function pointer typedef redeclarations

// Test 1: Basic heterogeneous typedef redeclaration (_Unsafe -> _Safe)
typedef _Unsafe  int (*FuncPtr1)(int* p);
typedef _Safe  int (*FuncPtr1)(int* _Owned p);

// Test 2: Heterogeneous typedef redeclaration (_Safe -> _Unsafe)
typedef _Safe  void (*FuncPtr2)(int* _Borrow p);
typedef _Unsafe  void (*FuncPtr2)(int* p);

// Test 3: Return type compatibility (_Unsafe T* <-> _Safe T* _Owned)
typedef _Unsafe  int* (*FuncPtr3)(void);
typedef _Safe  int* _Owned (*FuncPtr3)(void);

// Test 4: Return type compatibility (_Unsafe T* <-> _Safe T* _Borrow)
typedef _Unsafe  char* (*FuncPtr4)(char* p);
typedef _Safe  char* _Borrow (*FuncPtr4)(char* _Borrow p);

// Test 5: Multiple parameters with heterogeneous redeclaration
typedef _Unsafe  void (*FuncPtr5)(int* p, float* q);
typedef _Safe  void (*FuncPtr5)(int* _Owned p, float* _Owned q);

// Test 8: Nested pointers
typedef _Unsafe  int** (*FuncPtr8)(int** p);
typedef _Safe  int** _Owned (*FuncPtr8)(int** _Owned p);

// Test 6: void* _Owned parameter and return
typedef _Unsafe  void* (*FuncPtr6)(void* p);
typedef _Safe  void* _Owned (*FuncPtr6)(void* _Owned p);

// Test 7: const char* _Borrow parameter
typedef _Unsafe  void (*FuncPtr7)(const char* msg);
typedef _Safe  void (*FuncPtr7)(const char* _Borrow msg);

// Test 9: Homogeneous redeclaration (both _Safe) - should work
typedef _Safe  void (*FuncPtr9)(int* _Owned p);
typedef _Safe  void (*FuncPtr9)(int* _Owned p);

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

// Test 11: Heterogeneous function redeclaration where a parameter is itself a
// heterogeneous function pointer pair.  The unsafe decl uses a plain comparator
// typedef and the _Safe decl uses a _Safe-qualified comparator typedef.
// This was previously rejected with "incompatible _Safe and unsafe declarations".
typedef _Safe  int (*cmp_safe)(const void * _Borrow, const void * _Borrow);
typedef        int (*cmp_unsafe)(const void *, const void *);

_Safe  void sort_safe(void * _Borrow base, unsigned int n, unsigned int sz, cmp_safe compar);
void sort_safe(void * base, unsigned int n, unsigned int sz, cmp_unsafe compar);

// Test 12: Same scenario but with the _Safe decl coming second (reverse order).
typedef _Safe  int (*cmp2_safe)(int * _Borrow, int * _Borrow);
typedef        int (*cmp2_unsafe)(int *, int *);

void sort2(void * base, unsigned int n, cmp2_unsafe compar);
_Safe  void sort2(void * _Borrow base, unsigned int n, cmp2_safe compar);

// Test 13: Typedef redeclaration where the parameter is itself a heterogeneous
// function pointer pair.
typedef _Safe  int (*inner_safe)(const void * _Borrow);
typedef        int (*inner_unsafe)(const void *);

typedef _Safe  void (*Wrapper_safe)(inner_safe cb);
typedef        void (*Wrapper_unsafe)(inner_unsafe cb);

typedef Wrapper_safe  Wrapper;
typedef Wrapper_unsafe Wrapper;

// Test 14: Calling a heterogeneous function with a func_safe argument in a _Safe
// context — the _Safe overload should be selected and no error emitted.
typedef _Safe  int (*cmp3_safe)(const void * _Borrow, const void * _Borrow);
typedef        int (*cmp3_unsafe)(const void *, const void *);

_Safe  void sort3(cmp3_safe compar);
void sort3(cmp3_unsafe compar);

_Safe int my_cmp(const void * _Borrow a, const void * _Borrow b);

_Safe void test14(void) {
    cmp3_safe cb = my_cmp;
    sort3(cb);    // _Safe overload selected; cb is func_safe
}
