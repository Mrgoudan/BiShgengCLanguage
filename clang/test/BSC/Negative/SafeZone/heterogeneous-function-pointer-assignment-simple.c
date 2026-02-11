// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Simplified negative tests for heterogeneous function pointer assignment

// Test 1: Owned/borrow mismatch - no matching safe declaration
unsafe void take_owned(int* p);
safe void take_owned(int* owned p);

void test1(void) {
    safe void (*ptr_borrow)(int* borrow) = 0;
    ptr_borrow = take_owned;  // expected-error {{cannot cast}}
}

// Test 2: Borrow/owned mismatch - no matching safe declaration
unsafe void take_borrow(int* p);
safe void take_borrow(int* borrow p);

void test2(void) {
    safe void (*ptr_owned)(int* owned) = 0;
    ptr_owned = take_borrow;  // expected-error {{cannot cast}}
}

// Test 3: Parameter count mismatch
unsafe void two_params(int* p1, int* p2);
safe void two_params(int* owned p1, int* owned p2);

void test3(void) {
    safe void (*ptr_one)(int* owned) = 0;
    ptr_one = two_params;  // expected-error {{cannot cast}}
}

// Test 4: Type mismatch in parameters
unsafe void int_param(int* p);
safe void int_param(int* owned p);

void test4(void) {
    safe void (*ptr_float)(float* owned) = 0;
    ptr_float = int_param;  // expected-error {{conversion from type 'safe void (*)(int *owned)' to 'safe void (*)(float *owned)' is forbidden}} 
}
