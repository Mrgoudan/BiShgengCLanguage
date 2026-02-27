// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// Positive tests for mixed mode function declarations (Manual sections 7.1-7.4)

_Unsafe int* process1(int* p);
_Safe int* owned process1(int* owned p);

// Manual 2500-2501: void* compatibility
_Unsafe void* get_data(void);
_Safe void* owned get_data(void);

// Manual 2508-2509: Same modifiers, identical types
_Safe int* owned bar(int* owned p);
_Safe int* owned bar(int* owned q);

// These are all compatible even with mismatched const/volatile
_Unsafe const int* get_const(void);
_Safe const int* owned get_const(void);

_Unsafe volatile int* get_volatile(void);
_Safe volatile int* owned get_volatile(void);

_Unsafe const volatile int* get_cv(void);
_Safe const volatile int* owned get_cv(void);

// Manual 2486: _Unsafe T* ⟷ _Safe T* _Borrow
_Unsafe void process_borrow(int* p);
_Safe void process_borrow(int* _Borrow p);

_Unsafe void take_borrow(void* p);
_Safe void take_borrow(void* _Borrow p);

// Manual 2474: Parameter count must be same
_Unsafe void multi_param(int a, int b, int c);
_Safe void multi_param(int a, int b, int c);

// No parameters
_Unsafe int no_params(void);
_Safe int no_params(void);

_Unsafe int** double_ptr(int** p);
_Safe int** owned double_ptr(int** owned p);

_Unsafe int*** triple_ptr(int*** p);
_Safe int*** owned triple_ptr(int*** owned p);

// Manual 2518-2524: Mixed mode function can have one definition
_Unsafe int* defined_func(int* p);
_Safe int* owned defined_func(int* owned p);
// Definition based on _Safe version
_Safe int* owned defined_func(int* owned p) { return p; }

// Definition based on _Unsafe version
_Unsafe float* defined_func2(float* p) { return p; }
_Safe float* owned defined_func2(float* owned p);

// Manual 2465: Multiple _Safe declarations must be compatible
_Safe void safe_multi1(int x);
_Safe void safe_multi1(int y);  // ok: same type, different param name

// Multiple _Unsafe declarations
_Unsafe void unsafe_multi1(int x);
_Unsafe void unsafe_multi1(int y);

// Non-pointer parameters are compatible
_Unsafe int add(int a, int b);
_Safe int add(int a, int b);

_Unsafe float compute(float x, float y);
_Safe float compute(float x, float y);

// Multiple parameters with mixed pointer and non-pointer
_Unsafe void complex1(int a, int* p, float b, int* q);
_Safe void complex1(int a, int* owned p, float b, int* owned q);

// Pointer to const with owned
_Unsafe const int* complex2(const int* p);
_Safe const int* owned complex2(const int* owned p);

// expected-no-diagnostics
