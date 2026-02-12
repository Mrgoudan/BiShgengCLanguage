// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics
// Simplified positive tests for constraint-based heterogeneous function call selection

// Test 1: Basic selection based on owned qualifier
unsafe void process_unsafe(int* p);
safe void process_safe(int* owned p);

void test1(int* raw_p, int* owned owned_p) {
    process_unsafe(raw_p);       // Calls unsafe version
    process_safe(owned_p);       // Calls safe version
}

// Test 2: Borrow argument selection
unsafe void consume_unsafe(int* p);
safe void consume_safe(int* borrow p);

void test2(int* raw_p, int* borrow borrow_p) {
    consume_unsafe(raw_p);       // Calls unsafe version
    consume_safe(borrow_p);      // Calls safe version
}

// Test 3: const compatibility - mut can go to const
unsafe void read_unsafe(int* p);
safe void read_safe(const int* owned p);

void test3(int* owned mut_owned) {
    read_safe(mut_owned);  // Calls safe version (mut → const OK)
}

// Test 4: Multiple parameters
unsafe void pair_unsafe(int* p1, float* p2);
safe void pair_safe(int* owned p1, float* owned p2);

void test4(int* raw_i, float* raw_f, int* owned owned_i, float* owned owned_f) {
    pair_unsafe(raw_i, raw_f);       // Calls unsafe version
    pair_safe(owned_i, owned_f);     // Calls safe version
}

// Test 5: Unsafe context prefers safe when constraints match
void test5_unsafe(int* owned owned_p, int* raw_p) {
    unsafe {
        process_safe(owned_p);   // Prefers safe version (constraints match)
        process_unsafe(raw_p);   // Falls back to unsafe version
    }
}
