// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Negative tests for heterogeneous function redeclarations (Manual sections 7.1-7.4)
// Heterogeneous redeclarations: one safe, one unsafe declaration of the same function

unsafe int test_param_count1(int a); // expected-note {{previous declaration is here}}
safe int test_param_count1(int a, int b); // expected-error {{function 'test_param_count1' has incompatible safe and unsafe declarations}}

unsafe void test_param_count2(int a, int b, int c); // expected-note {{previous declaration is here}}
safe void test_param_count2(int a); // expected-error {{function 'test_param_count2' has incompatible safe and unsafe declarations}}

unsafe int test_return_type1(void); // expected-note {{previous declaration is here}}
safe float test_return_type1(void); // expected-error {{function 'test_return_type1' has incompatible safe and unsafe declarations}}

unsafe void* test_return_type2(void); // expected-note {{previous declaration is here}}
safe int* owned test_return_type2(void); // expected-error {{function 'test_return_type2' has incompatible safe and unsafe declarations}}

safe void test_owned_borrow(int* owned p); // expected-note {{previous declaration is here}}
safe void test_owned_borrow(int* borrow p); // expected-error {{conflicting types for 'test_owned_borrow'}}

safe int* owned test_owned_vs_borrow(int* owned p); // expected-note {{previous declaration is here}}
safe int* borrow test_owned_vs_borrow(int* borrow p); // expected-error {{conflicting types for 'test_owned_vs_borrow'}}

unsafe const int* test_const_compat1(int* p); // expected-note {{previous declaration is here}}
safe const int* owned test_const_compat1(const int* owned p); // expected-error {{function 'test_const_compat1' has incompatible safe and unsafe declarations}} 

unsafe int* test_const_compat2(const int* p); // expected-note {{previous declaration is here}}
safe int* owned test_const_compat2(int* owned p); // expected-error {{function 'test_const_compat2' has incompatible safe and unsafe declarations}} 

unsafe volatile int* test_volatile_compat(int* p);  // expected-note {{previous declaration is here}}
safe volatile int* owned test_volatile_compat(volatile int* owned p); // expected-error {{function 'test_volatile_compat' has incompatible safe and unsafe declarations}} 

unsafe T generic_func<T>(T x); // expected-note {{previous declaration is here}}
safe T generic_func<T>(T x); // expected-error {{generic function 'generic_func' cannot have both safe and unsafe declarations}}


unsafe void test_param_type(int x); // expected-note {{previous declaration is here}}
safe void test_param_type(float x); // expected-error {{function 'test_param_type' has incompatible safe and unsafe declarations}}

// Different pointer target types
unsafe int* test_ptr_target(int* p); // expected-note {{previous declaration is here}}
safe float* owned test_ptr_target(float* owned p); // expected-error {{function 'test_ptr_target' has incompatible safe and unsafe declarations}}

unsafe int* test_ptr_level1(int* p); // expected-note {{previous declaration is here}}
safe int** owned test_ptr_level1(int** owned p); // expected-error {{function 'test_ptr_level1' has incompatible safe and unsafe declarations}}

unsafe int*** test_ptr_level2(int*** p); // expected-note {{previous declaration is here}}
safe int** owned test_ptr_level2(int** owned p); // expected-error {{function 'test_ptr_level2' has incompatible safe and unsafe declarations}}
