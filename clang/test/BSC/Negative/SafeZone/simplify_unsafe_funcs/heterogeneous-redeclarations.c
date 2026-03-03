// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function redeclarations (Manual sections 7.1-7.4)
// Heterogeneous redeclarations: one _Safe, one _Unsafe declaration of the same function

_Unsafe int test_param_count1(int a); // expected-note {{previous declaration is here}}
_Safe int test_param_count1(int a, int b); // expected-error {{function 'test_param_count1' has incompatible _Safe and _Unsafe declarations}}

_Unsafe void test_param_count2(int a, int b, int c); // expected-note {{previous declaration is here}}
_Safe void test_param_count2(int a); // expected-error {{function 'test_param_count2' has incompatible _Safe and _Unsafe declarations}}

_Unsafe int test_return_type1(void); // expected-note {{previous declaration is here}}
_Safe float test_return_type1(void); // expected-error {{function 'test_return_type1' has incompatible _Safe and _Unsafe declarations}}

_Unsafe void* test_return_type2(void); // expected-note {{previous declaration is here}}
_Safe int* _Owned test_return_type2(void); // expected-error {{function 'test_return_type2' has incompatible _Safe and _Unsafe declarations}}

_Safe void test_owned_borrow(int* _Owned p); // expected-note {{previous declaration is here}}
_Safe void test_owned_borrow(int* _Borrow p); // expected-error {{conflicting types for 'test_owned_borrow'}}

_Safe int* _Owned test_owned_vs_borrow(int* _Owned p); // expected-note {{previous declaration is here}}
_Safe int* _Borrow test_owned_vs_borrow(int* _Borrow p); // expected-error {{conflicting types for 'test_owned_vs_borrow'}}

_Unsafe const int* test_const_compat1(int* p); // expected-note {{previous declaration is here}}
_Safe const int* _Owned test_const_compat1(const int* _Owned p); // expected-error {{function 'test_const_compat1' has incompatible _Safe and _Unsafe declarations}} 

_Unsafe int* test_const_compat2(const int* p); // expected-note {{previous declaration is here}}
_Safe int* _Owned test_const_compat2(int* _Owned p); // expected-error {{function 'test_const_compat2' has incompatible _Safe and _Unsafe declarations}} 

_Unsafe volatile int* test_volatile_compat(int* p);  // expected-note {{previous declaration is here}}
_Safe volatile int* _Owned test_volatile_compat(volatile int* _Owned p); // expected-error {{function 'test_volatile_compat' has incompatible _Safe and _Unsafe declarations}} 

_Unsafe T generic_func<T>(T x); // expected-note {{previous declaration is here}}
_Safe T generic_func<T>(T x); // expected-error {{generic function 'generic_func' cannot have both _Safe and _Unsafe declarations}}


_Unsafe void test_param_type(int x); // expected-note {{previous declaration is here}}
_Safe void test_param_type(float x); // expected-error {{function 'test_param_type' has incompatible _Safe and _Unsafe declarations}}

// Different pointer target types
_Unsafe int* test_ptr_target(int* p); // expected-note {{previous declaration is here}}
_Safe float* _Owned test_ptr_target(float* _Owned p); // expected-error {{function 'test_ptr_target' has incompatible _Safe and _Unsafe declarations}}

_Unsafe int* test_ptr_level1(int* p); // expected-note {{previous declaration is here}}
_Safe int** _Owned test_ptr_level1(int** _Owned p); // expected-error {{function 'test_ptr_level1' has incompatible _Safe and _Unsafe declarations}}

_Unsafe int*** test_ptr_level2(int*** p); // expected-note {{previous declaration is here}}
_Safe int** _Owned test_ptr_level2(int** _Owned p); // expected-error {{function 'test_ptr_level2' has incompatible _Safe and _Unsafe declarations}}

// _Safe redeclaration may not drop qualifiers present in the unsafe declaration.
int* _Borrow test_drop_borrow_param(int* _Borrow a); // expected-note {{previous declaration is here}}
_Safe int* test_drop_borrow_param(int* a); // expected-error {{function 'test_drop_borrow_param' has incompatible _Safe and _Unsafe declarations}}

int* _Owned test_drop_owned_param(int* _Owned a); // expected-note {{previous declaration is here}}
_Safe int* test_drop_owned_param(int* a); // expected-error {{function 'test_drop_owned_param' has incompatible _Safe and _Unsafe declarations}}

int* _Borrow test_drop_borrow_ret(int* _Borrow a); // expected-note {{previous declaration is here}}
_Safe int* test_drop_borrow_ret(int* _Borrow a); // expected-error {{function 'test_drop_borrow_ret' has incompatible _Safe and _Unsafe declarations}}

int* _Owned test_drop_owned_ret(int* _Owned a); // expected-note {{previous declaration is here}}
_Safe int* test_drop_owned_ret(int* _Owned a); // expected-error {{function 'test_drop_owned_ret' has incompatible _Safe and _Unsafe declarations}}
