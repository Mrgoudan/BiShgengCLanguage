// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function declarations (Manual sections 7.1-7.4)

unsafe int* process1(int* p);
safe int* owned process1(int* owned p);

// Manual 2500-2501: void* compatibility
unsafe void* get_data(void);
safe void* owned get_data(void);

// Manual 2508-2509: Same modifiers, identical types
safe int* owned bar(int* owned p);
safe int* owned bar(int* owned q);

// These are all compatible even with mismatched const/volatile
unsafe const int* get_const(void);
safe const int* owned get_const(void);

unsafe volatile int* get_volatile(void);
safe volatile int* owned get_volatile(void);

unsafe const volatile int* get_cv(void);
safe const volatile int* owned get_cv(void);

// Manual 2486: unsafe T* ⟷ safe T* borrow
unsafe void process_borrow(int* p);
safe void process_borrow(int* borrow p);

unsafe void take_borrow(void* p);
safe void take_borrow(void* borrow p);

// Manual 2474: Parameter count must be same
unsafe void multi_param(int a, int b, int c);
safe void multi_param(int a, int b, int c);

// No parameters
unsafe int no_params(void);
safe int no_params(void);

unsafe int** double_ptr(int** p);
safe int** owned double_ptr(int** owned p);

unsafe int*** triple_ptr(int*** p);
safe int*** owned triple_ptr(int*** owned p);

// Manual 2518-2524: Mixed mode function can have one definition
unsafe int* defined_func(int* p);
safe int* owned defined_func(int* owned p);
// Definition based on safe version
safe int* owned defined_func(int* owned p) { return p; }

// Definition based on unsafe version
unsafe float* defined_func2(float* p) { return p; }
safe float* owned defined_func2(float* owned p);

// Manual 2465: Multiple safe declarations must be compatible
safe void safe_multi1(int x);
safe void safe_multi1(int y);  // ok: same type, different param name

// Multiple unsafe declarations
unsafe void unsafe_multi1(int x);
unsafe void unsafe_multi1(int y);

// Non-pointer parameters are compatible
unsafe int add(int a, int b);
safe int add(int a, int b);

unsafe float compute(float x, float y);
safe float compute(float x, float y);

// Multiple parameters with mixed pointer and non-pointer
unsafe void complex1(int a, int* p, float b, int* q);
safe void complex1(int a, int* owned p, float b, int* owned q);

// Pointer to const with owned
unsafe const int* complex2(const int* p);
safe const int* owned complex2(const int* owned p);

// expected-no-diagnostics
