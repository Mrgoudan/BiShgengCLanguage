// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Simplified negative tests for constraint-based heterogeneous function call selection

// Test 1: Safe context cannot call function with only _Unsafe declaration
_Unsafe void unsafe_only(void);

void test1(void) {
    _Safe {
        unsafe_only();  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
    }
}

// Test 2: Const to mut _Borrow is not allowed
_Unsafe void modify_data(int* p);
_Safe void modify_data(int* _Borrow p); // expected-note {{passing argument to parameter 'p' here}}

void test2(const int* _Borrow const_borrow) {
    modify_data(const_borrow);  // expected-error {{incompatible _Borrow types, cannot cast 'const int *_Borrow' to 'int *_Borrow'}}
}

// Test 3: Owned/_Borrow mismatch
_Unsafe void take_owned(int* p);             // expected-note {{argument 1 of type 'int *_Borrow' doesn't match parameter type 'int *'}}
_Safe void take_owned(int* _Owned p);        // expected-note {{argument 1 of type 'int *_Borrow' doesn't match parameter type 'int *_Owned'}}

void test3(int* _Borrow borrow_p) {
    take_owned(borrow_p);  // expected-error {{no matching declaration of 'take_owned' for call type 'void (int *_Borrow)'}}
}

// Test 4: Borrow/_Owned mismatch
_Unsafe void take_borrow(int* p);            // expected-note {{argument 1 of type 'int *_Owned' doesn't match parameter type 'int *'}}
_Safe void take_borrow(int* _Borrow p);      // expected-note {{argument 1 of type 'int *_Owned' doesn't match parameter type 'int *_Borrow'}}

void test4(int* _Owned owned_p) {
    take_borrow(owned_p);  // expected-error {{no matching declaration of 'take_borrow' for call type 'void (int *_Owned)'}}
}

// Test 5: Wrong argument count
_Unsafe void two_params(int* p1, int* p2);                       // expected-note {{call passes 1 argument but candidate takes 2}}
_Safe void two_params(int* _Owned p1, int* _Owned p2);           // expected-note {{call passes 1 argument but candidate takes 2}}

void test5(int* _Owned p) {
    two_params(p);  // expected-error {{no matching declaration of 'two_params' for call type 'void (int *_Owned)'}}
}

// Test 6: Type mismatch in parameters
_Unsafe void int_param(int* p);              // expected-note {{argument 1 of type 'float *_Owned' doesn't match parameter type 'int *'}}
_Safe void int_param(int* _Owned p);         // expected-note {{argument 1 of type 'float *_Owned' doesn't match parameter type 'int *_Owned'}}

void test6(float* _Owned p) {
    int_param(p);  // expected-error {{no matching declaration of 'int_param' for call type 'void (float *_Owned)'}}
}

// Test 7: Safe context with constraint mismatch (no _Safe declaration with compatible signature)
_Unsafe void process_raw(int* p);
_Safe void process_owned(int* _Owned p);

void test7(int* raw_p) {
    _Safe {
        process_raw(raw_p);  // expected-error {{_Unsafe function call is forbidden in the safe zone}}
    }
}

// Test 8: Nested pointer qualifier mismatch
_Unsafe void nested_func(int** pp);          // expected-note {{argument 1 of type 'int **_Borrow' doesn't match parameter type 'int **'}}
_Safe void nested_func(int** _Owned pp);     // expected-note {{argument 1 of type 'int **_Borrow' doesn't match parameter type 'int **_Owned'}}

void test8(int** _Borrow pp) {
    nested_func(pp);  // expected-error {{no matching declaration of 'nested_func' for call type 'void (int **_Borrow)'}}
}

// Test 9: Mixed _Owned and _Borrow with wrong combination
_Unsafe void mixed_wrong(int* p1, int* p2);                      // expected-note {{argument 1 of type 'int *_Borrow' doesn't match parameter type 'int *'}}
_Safe void mixed_wrong(int* _Owned p1, int* _Borrow p2);         // expected-note {{argument 1 of type 'int *_Borrow' doesn't match parameter type 'int *_Owned'}}

void test9(int* _Borrow borrow_p, int* _Owned owned_p) {
    // First parameter doesn't match (_Borrow vs _Owned), so error on first param
    mixed_wrong(borrow_p, owned_p);  // expected-error {{no matching declaration of 'mixed_wrong' for call type 'void (int *_Borrow, int *_Owned)'}}
}
