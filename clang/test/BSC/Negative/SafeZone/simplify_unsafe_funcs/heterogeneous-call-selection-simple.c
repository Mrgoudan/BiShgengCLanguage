// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Simplified negative tests for constraint-based heterogeneous function call selection

// Test 1: Safe context cannot call function with only _Unsafe declaration
_Unsafe void unsafe_only(void);

void test1(void) {
    _Safe {
        unsafe_only();  // expected-error {{unsafe function call is forbidden in the safe zone}}
    }
}

// Test 2: Const to mut borrow is not allowed
_Unsafe void modify_data(int* p);
_Safe void modify_data(int* borrow p);

void test2(const int* borrow const_borrow) {
    modify_data(const_borrow);  // expected-error {{incompatible borrow types, cannot cast 'const int *borrow' to 'int *borrow'}}
}

// Test 3: Owned/borrow mismatch
_Unsafe void take_owned(int* p);
_Safe void take_owned(int* owned p);

void test3(int* borrow borrow_p) {
    take_owned(borrow_p);  // expected-error {{no matching function for call to 'take_owned'; argument types do not match any _Safe or _Unsafe declaration}}
}

// Test 4: Borrow/owned mismatch
_Unsafe void take_borrow(int* p);
_Safe void take_borrow(int* borrow p);

void test4(int* owned owned_p) {
    take_borrow(owned_p);  // expected-error {{no matching function for call to 'take_borrow'; argument types do not match any _Safe or _Unsafe declaration}}
}

// Test 5: Wrong argument count
_Unsafe void two_params(int* p1, int* p2);
_Safe void two_params(int* owned p1, int* owned p2);

void test5(int* owned p) {
    two_params(p);  // expected-error {{no matching function for call to 'two_params'; argument types do not match any _Safe or _Unsafe declaration}}
}

// Test 6: Type mismatch in parameters
_Unsafe void int_param(int* p);
_Safe void int_param(int* owned p);

void test6(float* owned p) {
    int_param(p);  // expected-error {{no matching function for call to 'int_param'; argument types do not match any _Safe or _Unsafe declaration}}
}

// Test 7: Safe context with constraint mismatch (no _Safe declaration with compatible signature)
_Unsafe void process_raw(int* p);
_Safe void process_owned(int* owned p);

void test7(int* raw_p) {
    _Safe {
        process_raw(raw_p);  // expected-error {{unsafe function call is forbidden in the safe zone}}
    }
}

// Test 8: Nested pointer qualifier mismatch
_Unsafe void nested_func(int** pp);
_Safe void nested_func(int** owned pp);

void test8(int** borrow pp) {
    nested_func(pp);  // expected-error {{no matching function for call to 'nested_func'; argument types do not match any _Safe or _Unsafe declaration}}
}

// Test 9: Mixed owned and borrow with wrong combination
_Unsafe void mixed_wrong(int* p1, int* p2);
_Safe void mixed_wrong(int* owned p1, int* borrow p2);

void test9(int* borrow borrow_p, int* owned owned_p) {
    // First parameter doesn't match (borrow vs owned), so error on first param
    mixed_wrong(borrow_p, owned_p);  // expected-error {{no matching function for call to 'mixed_wrong'; argument types do not match any _Safe or _Unsafe declaration}}
}
