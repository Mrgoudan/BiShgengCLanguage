// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for constraint-based heterogeneous function call selection

// Declarations from bishengc_safety.hbs
safe T *owned safe_malloc<T>(T t);
safe void safe_free(void * _Nullable owned p);

// Test 1: Basic selection - owned argument selects safe version
unsafe int* process(int* p);
safe int* owned process(int* owned p);

void test1(void) {
    int* raw_p = (int*)0;
    int* owned owned_p = safe_malloc<int>(42);

    process(raw_p);      // Calls unsafe version
    int* owned result = process(owned_p);    // Calls safe version
    safe_free((void* owned)result);
}

// Test 2: Borrow argument selection
unsafe void consume(int* p);
safe void consume(int* borrow p);

void test2(void) {
    int local = 5;
    int* raw_p = &local;
    int* borrow borrow_p = &mut local;

    consume(raw_p);      // Calls unsafe version
    consume(borrow_p);   // Calls safe version
}

// Test 4: Multiple parameters
unsafe void process_pair(int* p1, float* p2);
safe void process_pair(int* owned p1, float* owned p2);

void test4(void) {
    int* raw_i = (int*)0;
    float* raw_f = (float*)0;
    int* owned owned_i = safe_malloc<int>(1);
    float* owned owned_f = safe_malloc<float>(2.0f);

    process_pair(raw_i, raw_f);          // Calls unsafe version
    process_pair(owned_i, owned_f);      // Calls safe version
}

// Test 5: Return type doesn't affect selection (only params matter)
unsafe int* get_int(int* p);
safe int* owned get_int(int* owned p);

void test5(void) {
    int* raw_p = (int*)0;
    int* owned owned_p = safe_malloc<int>(5);

    int* result1 = get_int(raw_p);         // Calls unsafe version
    int* owned result2 = get_int(owned_p); // Calls safe version
    safe_free((void* owned)result2);
}

// Test 6: Use function parameters to avoid complex initialization
unsafe void process_nested(int** pp);
safe void process_nested(int** owned pp);

void test6(int** raw_pp, int** owned owned_pp) {
    process_nested(raw_pp);       // Calls unsafe version
    process_nested(owned_pp);     // Calls safe version
}

// Test 7: Safe context always selects safe version
void test7_safe(void) {
    int* owned p = safe_malloc<int>(7);
    safe {
        int* owned result = process(p);  // Must call safe version (only safe allowed in safe context)
        safe_free((void* owned)result);
    }
}

// Test 8: Unsafe context prefers safe when constraints match
void test8_unsafe(void) {
    int* owned owned_p = safe_malloc<int>(8);
    int* raw_p = (int*)0;

    unsafe {
        int* owned result1 = process(owned_p);  // Prefers safe version (constraints match)
        safe_free((void* owned)result1);
        process(raw_p);    // Falls back to unsafe version (safe constraints don't match)
    }
}

// Test 9: Mixed owned and borrow
unsafe void mixed_func(int* p1, int* p2);
safe void mixed_func(int* owned p1, int* borrow p2);

void test9(void) {
    int local = 5;
    int* owned owned_p = safe_malloc<int>(9);
    int* borrow borrow_p = &mut local;
    int* raw1 = (int*)0;
    int* raw2 = (int*)0;

    mixed_func(owned_p, borrow_p);  // Calls safe version
    mixed_func(raw1, raw2);         // Calls unsafe version
}
