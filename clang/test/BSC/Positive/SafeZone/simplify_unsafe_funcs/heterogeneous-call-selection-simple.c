// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Simplified positive tests for constraint-based heterogeneous function call selection

// Test 1: Basic selection based on _Owned qualifier
_Unsafe void process_unsafe(int* p);
_Safe void process_safe(int* _Owned p);

void test1(int* raw_p, int* _Owned owned_p) {
    process_unsafe(raw_p);       // Calls _Unsafe version
    process_safe(owned_p);       // Calls _Safe version
}

// Test 2: Borrow argument selection
_Unsafe void consume_unsafe(int* p);
_Safe void consume_safe(int* _Borrow p);

void test2(int* raw_p, int* _Borrow borrow_p) {
    consume_unsafe(raw_p);       // Calls _Unsafe version
    consume_safe(borrow_p);      // Calls _Safe version
}

// Test 3: const compatibility - mut can go to const
_Unsafe void read_unsafe(int* p);
_Safe void read_safe(const int* _Owned p);

void test3(int* _Owned mut_owned) {
    read_safe(mut_owned);  // Calls _Safe version (mut → const OK)
}

// Test 4: Multiple parameters
_Unsafe void pair_unsafe(int* p1, float* p2);
_Safe void pair_safe(int* _Owned p1, float* _Owned p2);

void test4(int* raw_i, float* raw_f, int* _Owned owned_i, float* _Owned owned_f) {
    pair_unsafe(raw_i, raw_f);       // Calls _Unsafe version
    pair_safe(owned_i, owned_f);     // Calls _Safe version
}

// Test 5: Function-to-function-pointer decay with heterogeneous declarations
typedef int(*func)(int, int);
void f1(func f);
_Safe void f1(func f);

int add(int a, int b) {
    return a + b;
}

void test5_func_decay(void) {
    f1(add);           // Pass function (decays to function pointer)
    func fp = add;
    f1(fp);            // Pass function pointer directly
}

// Test 6: Unsafe context prefers _Safe when constraints match
void test6_unsafe(int* _Owned owned_p, int* raw_p) {
    _Unsafe {
        process_safe(owned_p);   // Prefers _Safe version (constraints match)
        process_unsafe(raw_p);   // Falls back to _Unsafe version
    }
}
