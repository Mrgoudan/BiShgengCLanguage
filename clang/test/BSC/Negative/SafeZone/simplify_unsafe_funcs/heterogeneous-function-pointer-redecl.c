// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function pointer typedef redeclarations

// Test 1: Parameter count mismatch
_Unsafe typedef void (*BadFuncPtr1)(int* p); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr1)(int* owned p, int* owned q); // expected-error {{function 'BadFuncPtr1' has incompatible _Safe and _Unsafe declarations}}

// Test 2: Return type mismatch (different base types)
_Unsafe typedef int* (*BadFuncPtr2)(void); // expected-note {{previous definition is here}}
_Safe typedef float* owned (*BadFuncPtr2)(void); // expected-error {{function 'BadFuncPtr2' has incompatible _Safe and _Unsafe declarations}}

// Test 3: Parameter type mismatch (different base types)
_Unsafe typedef void (*BadFuncPtr3)(int* p); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr3)(float* owned p); // expected-error {{function 'BadFuncPtr3' has incompatible _Safe and _Unsafe declarations}}

// Test 4: Pointer level mismatch
_Unsafe typedef int* (*BadFuncPtr4)(void); // expected-note {{previous definition is here}}
_Safe typedef int** owned (*BadFuncPtr4)(void); // expected-error {{function 'BadFuncPtr4' has incompatible _Safe and _Unsafe declarations}}

// Test 5: Variadic mismatch
_Unsafe typedef void (*BadFuncPtr5)(int* p, ...); // expected-note {{previous definition is here}}
_Safe typedef void (*BadFuncPtr5)(int* owned p); // expected-error {{function 'BadFuncPtr5' has incompatible _Safe and _Unsafe declarations}}

// Test 6: Nested pointer target type mismatch
_Unsafe typedef int** (*BadFuncPtr6)(void); // expected-note {{previous definition is here}}
_Safe typedef float** owned (*BadFuncPtr6)(void); // expected-error {{function 'BadFuncPtr6' has incompatible _Safe and _Unsafe declarations}}

// Test 7: Homogeneous but incompatible (different parameter count)
_Unsafe typedef void (*BadFuncPtr7)(int* p); // expected-note {{previous definition is here}}
_Unsafe typedef void (*BadFuncPtr7)(int* q, int* r); // expected-error {{typedef redefinition with different types ('void (*)(int *, int *)' vs 'void (*)(int *)')}}
