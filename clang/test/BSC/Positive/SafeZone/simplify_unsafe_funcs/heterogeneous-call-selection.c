// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for constraint-based heterogeneous function call selection

// Declarations from bishengc_safety.hbs
_Safe T *_Owned safe_malloc<T>(T t);
_Safe void safe_free(void * _Nullable _Owned p);

// Test 1: Basic selection - _Owned argument selects _Safe version
_Unsafe int* process(int* p);
_Safe int* _Owned process(int* _Owned p);

void test1(void) {
    int* raw_p = (int*)0;
    int* _Owned owned_p = safe_malloc<int>(42);

    process(raw_p);      // Calls _Unsafe version
    int* _Owned result = process(owned_p);    // Calls _Safe version
    safe_free((void* _Owned)result);
}

// Test 2: Borrow argument selection
_Unsafe void consume(int* p);
_Safe void consume(int* _Borrow p);

void test2(void) {
    int local = 5;
    int* raw_p = &local;
    int* _Borrow borrow_p = &_Mut local;

    consume(raw_p);      // Calls _Unsafe version
    consume(borrow_p);   // Calls _Safe version
}

// Test 4: Multiple parameters
_Unsafe void process_pair(int* p1, float* p2);
_Safe void process_pair(int* _Owned p1, float* _Owned p2);

void test4(void) {
    int* raw_i = (int*)0;
    float* raw_f = (float*)0;
    int* _Owned owned_i = safe_malloc<int>(1);
    float* _Owned owned_f = safe_malloc<float>(2.0f);

    process_pair(raw_i, raw_f);          // Calls _Unsafe version
    process_pair(owned_i, owned_f);      // Calls _Safe version
}

// Test 5: Return type doesn't affect selection (only params matter)
_Unsafe int* get_int(int* p);
_Safe int* _Owned get_int(int* _Owned p);

void test5(void) {
    int* raw_p = (int*)0;
    int* _Owned owned_p = safe_malloc<int>(5);

    int* result1 = get_int(raw_p);         // Calls _Unsafe version
    int* _Owned result2 = get_int(owned_p); // Calls _Safe version
    safe_free((void* _Owned)result2);
}

// Test 6: Use function parameters to avoid complex initialization
_Unsafe void process_nested(int** pp);
_Safe void process_nested(int** _Owned pp);

void test6(int** raw_pp, int** _Owned owned_pp) {
    process_nested(raw_pp);       // Calls _Unsafe version
    process_nested(owned_pp);     // Calls _Safe version
}

// Test 7: Safe context always selects _Safe version
void test7_safe(void) {
    int* _Owned p = safe_malloc<int>(7);
    _Safe {
        int* _Owned result = process(p);  // Must call _Safe version (only _Safe allowed in _Safe context)
        safe_free((void* _Owned)result);
    }
}

// Test 8: Unsafe context prefers _Safe when constraints match
void test8_unsafe(void) {
    int* _Owned owned_p = safe_malloc<int>(8);
    int* raw_p = (int*)0;

    _Unsafe {
        int* _Owned result1 = process(owned_p);  // Prefers _Safe version (constraints match)
        safe_free((void* _Owned)result1);
        process(raw_p);    // Falls back to _Unsafe version (_Safe constraints don't match)
    }
}

// Test 9: Mixed _Owned and _Borrow
_Unsafe void mixed_func(int* p1, int* p2);
_Safe void mixed_func(int* _Owned p1, int* _Borrow p2);

void test9(void) {
    int local = 5;
    int* _Owned owned_p = safe_malloc<int>(9);
    int* _Borrow borrow_p = &_Mut local;
    int* raw1 = (int*)0;
    int* raw2 = (int*)0;

    mixed_func(owned_p, borrow_p);  // Calls _Safe version
    mixed_func(raw1, raw2);         // Calls _Unsafe version
}
