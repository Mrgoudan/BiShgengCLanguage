// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Simplified negative tests for heterogeneous function pointer assignment

// Test 1: Owned/borrow mismatch - no matching _Safe declaration
_Unsafe void take_owned(int* p);
_Safe void take_owned(int* owned p);

void test1(void) {
    _Safe void (*ptr_borrow)(int* borrow) = 0;
    ptr_borrow = take_owned;  // expected-error {{cannot cast}}
}

// Test 2: Borrow/owned mismatch - no matching _Safe declaration
_Unsafe void take_borrow(int* p);
_Safe void take_borrow(int* borrow p);

void test2(void) {
    _Safe void (*ptr_owned)(int* owned) = 0;
    ptr_owned = take_borrow;  // expected-error {{cannot cast}}
}

// Test 3: Parameter count mismatch
_Unsafe void two_params(int* p1, int* p2);
_Safe void two_params(int* owned p1, int* owned p2);

void test3(void) {
    _Safe void (*ptr_one)(int* owned) = 0;
    ptr_one = two_params;  // expected-error {{cannot cast}}
}

// Test 4: Type mismatch in parameters
_Unsafe void int_param(int* p);
_Safe void int_param(int* owned p);

void test4(void) {
    _Safe void (*ptr_float)(float* owned) = 0;
    ptr_float = int_param;  // expected-error {{conversion from type '_Safe void (*)(int *owned)' to '_Safe void (*)(float *owned)' is forbidden}}
}

// Test 5: void* owned/borrow mismatch in function pointer
_Unsafe void void_owned(void* p);
_Safe void void_owned(void* owned p);

void test5(void) {
    _Safe void (*ptr_borrow)(void* borrow) = 0;
    ptr_borrow = void_owned;  // expected-error {{cannot cast}}
}

// Test 6: Const mismatch - char* borrow vs const char* borrow
_Unsafe void mut_char(char* p);
_Safe void mut_char(char* borrow p);

void test6(void) {
    _Safe void (*ptr_const)(const char* borrow) = 0;
    ptr_const = mut_char;  // expected-error {{cannot cast}}
}
