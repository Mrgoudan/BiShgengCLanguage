// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Positive tests for constraint-based heterogeneous function pointer assignment

// Declarations from bishengc_safety.hbs
_Safe T *_Owned safe_malloc<T>(T t);
_Safe void safe_free(void * _Nullable _Owned p);

// Test 1: Basic assignment - _Owned constraint matching
_Unsafe int* process(int* p);
_Safe int* _Owned process(int* _Owned p);

void test1(void) {
    _Safe int* _Owned (*safe_ptr)(int* _Owned) = 0;
    _Unsafe int* (*unsafe_ptr)(int*) = 0;

    safe_ptr = process;    // Assigns _Safe version (constraints match)
    unsafe_ptr = process;  // Assigns _Unsafe or _Safe version
}

// Test 2: Borrow constraint matching
_Unsafe void consume(int* p);
_Safe void consume(int* _Borrow p);

void test2(void) {
    _Safe void (*safe_ptr)(int* _Borrow) = 0;
    _Unsafe void (*unsafe_ptr)(int*) = 0;

    safe_ptr = consume;    // Assigns _Safe version (_Borrow constraint matches)
    unsafe_ptr = consume;  // Assigns _Unsafe or _Safe version
}

// Test 4: Multiple parameters with matching constraints
_Unsafe void process_pair(int* p1, float* p2);
_Safe void process_pair(int* _Owned p1, float* _Owned p2);

void test4(void) {
    _Safe void (*safe_ptr)(int* _Owned, float* _Owned) = 0;
    _Unsafe void (*unsafe_ptr)(int*, float*) = 0;

    safe_ptr = process_pair;    // Assigns _Safe version
    unsafe_ptr = process_pair;  // Assigns _Unsafe or _Safe version
}

// Test 5: Return type with ownership qualifiers
_Unsafe int* get_int(int* p);
_Safe int* _Owned get_int(int* _Owned p);

void test5(void) {
    _Safe int* _Owned (*safe_ptr)(int* _Owned) = 0;
    _Unsafe int* (*unsafe_ptr)(int*) = 0;

    safe_ptr = get_int;    // Assigns _Safe version
    unsafe_ptr = get_int;  // Assigns _Unsafe or _Safe version
}

// Test 6: Nested pointer with ownership qualifiers
_Unsafe void process_nested(int** pp);
_Safe void process_nested(int** _Owned pp);

void test6(void) {
    _Safe void (*safe_ptr)(int** _Owned) = 0;
    _Unsafe void (*unsafe_ptr)(int**) = 0;

    safe_ptr = process_nested;    // Assigns _Safe version
    unsafe_ptr = process_nested;  // Assigns _Unsafe or _Safe version
}

// Test 7: Mixed _Owned and _Borrow parameters
_Unsafe void mixed_func(int* p1, int* p2);
_Safe void mixed_func(int* _Owned p1, int* _Borrow p2);

void test7(void) {
    _Safe void (*safe_ptr)(int* _Owned, int* _Borrow) = 0;
    _Unsafe void (*unsafe_ptr)(int*, int*) = 0;

    safe_ptr = mixed_func;    // Assigns _Safe version
    unsafe_ptr = mixed_func;  // Assigns _Unsafe or _Safe version
}

// Test 8: Safe pointer assignment in _Safe context
void test8_safe(void) {
    _Safe {
        _Safe int* _Owned (*safe_ptr)(int* _Owned) = 0;
        safe_ptr = process;  // Must assign _Safe version (only _Safe allowed in _Safe context)
    }
}

// Test 9: Unsafe pointer assignment in _Unsafe context (prefers _Safe)
void test9_unsafe(void) {
    _Unsafe {
        _Safe int* _Owned (*safe_ptr)(int* _Owned) = 0;
        _Unsafe int* (*unsafe_ptr)(int*) = 0;

        safe_ptr = process;    // Prefers _Safe version (constraints match)
        unsafe_ptr = process;  // Can use _Unsafe or _Safe version
    }
}

// Test 10: void* _Owned parameter and return - hot fix scenario
_Unsafe void* process_void(void* p);
_Safe void* _Owned process_void(void* _Owned p);

void test10_void_owned(void) {
    _Safe void* _Owned (*safe_ptr)(void* _Owned) = 0;
    _Unsafe void* (*unsafe_ptr)(void*) = 0;

    safe_ptr = process_void;    // Assigns _Safe version
    unsafe_ptr = process_void;  // Assigns _Unsafe or _Safe version
}

// Test 11: const char* _Borrow parameter - hot fix scenario
_Unsafe void log_msg(const char* msg);
_Safe void log_msg(const char* _Borrow msg);

void test11_const_char_borrow(void) {
    _Safe void (*safe_ptr)(const char* _Borrow) = 0;
    _Unsafe void (*unsafe_ptr)(const char*) = 0;

    safe_ptr = log_msg;    // Assigns _Safe version (_Borrow constraint matches)
    unsafe_ptr = log_msg;  // Assigns _Unsafe or _Safe version
}

// Test 12: const char* _Borrow return with _Borrow parameter
_Unsafe const char* lookup(const char* key);
_Safe const char* _Borrow lookup(const char* _Borrow key);

void test12_const_borrow_return(void) {
    _Safe const char* _Borrow (*safe_ptr)(const char* _Borrow) = 0;
    _Unsafe const char* (*unsafe_ptr)(const char*) = 0;

    safe_ptr = lookup;    // Assigns _Safe version
    unsafe_ptr = lookup;  // Assigns _Unsafe or _Safe version
}

// Test 13: Mixed _Owned/_Borrow with const qualifiers
_Unsafe void process_mixed_const(int* p1, const char* p2);
_Safe void process_mixed_const(int* _Owned p1, const char* _Borrow p2);

void test13_mixed_const(void) {
    _Safe void (*safe_ptr)(int* _Owned, const char* _Borrow) = 0;
    _Unsafe void (*unsafe_ptr)(int*, const char*) = 0;

    safe_ptr = process_mixed_const;    // Assigns _Safe version
    unsafe_ptr = process_mixed_const;  // Assigns _Unsafe or _Safe version
}
