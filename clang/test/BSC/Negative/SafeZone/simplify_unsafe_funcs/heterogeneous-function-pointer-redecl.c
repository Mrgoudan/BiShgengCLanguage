// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function pointer typedef redeclarations

// Test 1: Parameter count mismatch
_Unsafe typedef void (*BadFuncPtr1)(int* p); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr1)(int* _Owned p, int* _Owned q); // expected-error {{function 'BadFuncPtr1' has incompatible _Safe and _Unsafe declarations}}

// Test 2: Return type mismatch (different base types)
_Unsafe typedef int* (*BadFuncPtr2)(void); // expected-note {{previous definition is here}}
_Safe typedef float* _Owned (*BadFuncPtr2)(void); // expected-error {{function 'BadFuncPtr2' has incompatible _Safe and _Unsafe declarations}}

// Test 3: Parameter type mismatch (different base types)
_Unsafe typedef void (*BadFuncPtr3)(int* p); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr3)(float* _Owned p); // expected-error {{function 'BadFuncPtr3' has incompatible _Safe and _Unsafe declarations}}

// Test 4: Pointer level mismatch
_Unsafe typedef int* (*BadFuncPtr4)(void); // expected-note {{previous definition is here}}
_Safe typedef int** _Owned (*BadFuncPtr4)(void); // expected-error {{function 'BadFuncPtr4' has incompatible _Safe and _Unsafe declarations}}

// Test 5: Variadic mismatch
_Unsafe typedef void (*BadFuncPtr5)(int* p, ...); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr5)(int* _Owned p); // expected-error {{function 'BadFuncPtr5' has incompatible _Safe and _Unsafe declarations}}

// Test 6: Nested pointer target type mismatch
_Unsafe typedef int** (*BadFuncPtr6)(void); // expected-note {{previous definition is here}}
_Safe typedef float** _Owned (*BadFuncPtr6)(void); // expected-error {{function 'BadFuncPtr6' has incompatible _Safe and _Unsafe declarations}}

// Test 7: Homogeneous but incompatible (different parameter count)
_Unsafe typedef void (*BadFuncPtr7)(int* p); // expected-note {{previous definition is here}}
_Unsafe typedef void (*BadFuncPtr7)(int* q, int* r); // expected-error {{typedef redefinition with different types ('void (*)(int *, int *)' vs 'void (*)(int *)')}}

// Test 8: Heterogeneous function redeclaration where the function-pointer
// parameter types have mismatched base types (int* vs float*) — must still error.
typedef _Safe  int (*good_cmp_safe)(const void * _Borrow, const void * _Borrow);
typedef        int (*bad_cmp_unsafe)(int *, float *); // different param types

_Safe  void bad_sort(void * _Borrow base, unsigned int n, good_cmp_safe compar); // expected-note {{previous declaration is here}}
void bad_sort(void * base, unsigned int n, bad_cmp_unsafe compar); // expected-error {{function 'bad_sort' has incompatible _Safe and _Unsafe declarations}}

// Test 9: Heterogeneous function redeclaration where the function-pointer
// parameter types have different parameter counts — must still error.
typedef _Safe  int (*cmp3_safe)(const void * _Borrow, const void * _Borrow);
typedef        int (*cmp3_unsafe)(const void *); // one fewer param

_Safe  void bad_sort2(void * _Borrow base, unsigned int n, cmp3_safe compar); // expected-note {{previous declaration is here}}
void bad_sort2(void * base, unsigned int n, cmp3_unsafe compar); // expected-error {{function 'bad_sort2' has incompatible _Safe and _Unsafe declarations}}
