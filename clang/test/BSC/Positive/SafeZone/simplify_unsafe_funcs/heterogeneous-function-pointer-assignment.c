// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for constraint-based heterogeneous function pointer assignment

// Declarations from bishengc_safety.hbs
safe T *owned safe_malloc<T>(T t);
safe void safe_free(void * _Nullable owned p);

// Test 1: Basic assignment - owned constraint matching
unsafe int* process(int* p);
safe int* owned process(int* owned p);

void test1(void) {
    safe int* owned (*safe_ptr)(int* owned) = 0;
    unsafe int* (*unsafe_ptr)(int*) = 0;

    safe_ptr = process;    // Assigns safe version (constraints match)
    unsafe_ptr = process;  // Assigns unsafe or safe version
}

// Test 2: Borrow constraint matching
unsafe void consume(int* p);
safe void consume(int* borrow p);

void test2(void) {
    safe void (*safe_ptr)(int* borrow) = 0;
    unsafe void (*unsafe_ptr)(int*) = 0;

    safe_ptr = consume;    // Assigns safe version (borrow constraint matches)
    unsafe_ptr = consume;  // Assigns unsafe or safe version
}

// Test 4: Multiple parameters with matching constraints
unsafe void process_pair(int* p1, float* p2);
safe void process_pair(int* owned p1, float* owned p2);

void test4(void) {
    safe void (*safe_ptr)(int* owned, float* owned) = 0;
    unsafe void (*unsafe_ptr)(int*, float*) = 0;

    safe_ptr = process_pair;    // Assigns safe version
    unsafe_ptr = process_pair;  // Assigns unsafe or safe version
}

// Test 5: Return type with ownership qualifiers
unsafe int* get_int(int* p);
safe int* owned get_int(int* owned p);

void test5(void) {
    safe int* owned (*safe_ptr)(int* owned) = 0;
    unsafe int* (*unsafe_ptr)(int*) = 0;

    safe_ptr = get_int;    // Assigns safe version
    unsafe_ptr = get_int;  // Assigns unsafe or safe version
}

// Test 6: Nested pointer with ownership qualifiers
unsafe void process_nested(int** pp);
safe void process_nested(int** owned pp);

void test6(void) {
    safe void (*safe_ptr)(int** owned) = 0;
    unsafe void (*unsafe_ptr)(int**) = 0;

    safe_ptr = process_nested;    // Assigns safe version
    unsafe_ptr = process_nested;  // Assigns unsafe or safe version
}

// Test 7: Mixed owned and borrow parameters
unsafe void mixed_func(int* p1, int* p2);
safe void mixed_func(int* owned p1, int* borrow p2);

void test7(void) {
    safe void (*safe_ptr)(int* owned, int* borrow) = 0;
    unsafe void (*unsafe_ptr)(int*, int*) = 0;

    safe_ptr = mixed_func;    // Assigns safe version
    unsafe_ptr = mixed_func;  // Assigns unsafe or safe version
}

// Test 8: Safe pointer assignment in safe context
void test8_safe(void) {
    safe {
        safe int* owned (*safe_ptr)(int* owned) = 0;
        safe_ptr = process;  // Must assign safe version (only safe allowed in safe context)
    }
}

// Test 9: Unsafe pointer assignment in unsafe context (prefers safe)
void test9_unsafe(void) {
    unsafe {
        safe int* owned (*safe_ptr)(int* owned) = 0;
        unsafe int* (*unsafe_ptr)(int*) = 0;

        safe_ptr = process;    // Prefers safe version (constraints match)
        unsafe_ptr = process;  // Can use unsafe or safe version
    }
}

// Test 10: void* owned parameter and return - hot fix scenario
unsafe void* process_void(void* p);
safe void* owned process_void(void* owned p);

void test10_void_owned(void) {
    safe void* owned (*safe_ptr)(void* owned) = 0;
    unsafe void* (*unsafe_ptr)(void*) = 0;

    safe_ptr = process_void;    // Assigns safe version
    unsafe_ptr = process_void;  // Assigns unsafe or safe version
}

// Test 11: const char* borrow parameter - hot fix scenario
unsafe void log_msg(const char* msg);
safe void log_msg(const char* borrow msg);

void test11_const_char_borrow(void) {
    safe void (*safe_ptr)(const char* borrow) = 0;
    unsafe void (*unsafe_ptr)(const char*) = 0;

    safe_ptr = log_msg;    // Assigns safe version (borrow constraint matches)
    unsafe_ptr = log_msg;  // Assigns unsafe or safe version
}

// Test 12: const char* borrow return with borrow parameter
unsafe const char* lookup(const char* key);
safe const char* borrow lookup(const char* borrow key);

void test12_const_borrow_return(void) {
    safe const char* borrow (*safe_ptr)(const char* borrow) = 0;
    unsafe const char* (*unsafe_ptr)(const char*) = 0;

    safe_ptr = lookup;    // Assigns safe version
    unsafe_ptr = lookup;  // Assigns unsafe or safe version
}

// Test 13: Mixed owned/borrow with const qualifiers
unsafe void process_mixed_const(int* p1, const char* p2);
safe void process_mixed_const(int* owned p1, const char* borrow p2);

void test13_mixed_const(void) {
    safe void (*safe_ptr)(int* owned, const char* borrow) = 0;
    unsafe void (*unsafe_ptr)(int*, const char*) = 0;

    safe_ptr = process_mixed_const;    // Assigns safe version
    unsafe_ptr = process_mixed_const;  // Assigns unsafe or safe version
}
