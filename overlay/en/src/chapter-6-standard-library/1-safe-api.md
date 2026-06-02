# Safe API

## `safe_malloc`

`safe_malloc` is a safe memory allocation function provided by the BiShengC language.
The function takes a variable of a generic type `T`, which represents the size of the memory to allocate as well as the initialization of the memory after allocation.
The function's return value is of type `T * _Owned`, i.e. an `_Owned` pointer to the allocated heap memory.
Some specific examples of usage are given below.

In C, if we need to request a block of heap memory, we can use the `malloc` function to allocate it and then assign a value to that memory, for example:

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
}
```

However, memory allocated this way is not checked at the end of `p`'s scope to see whether `free` was called to release it, which causes a memory leak.
In addition, if another pointer is also made to point to that memory and it is freed at the end of its scope, a double free problem arises, for example:

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
    int *q = p;
    free(p);
    free(q); // error: double free!
}
```

Using the BiShengC language solves this kind of problem. The corresponding code is as follows:

```c
_Safe void example(void) {
    int * _Owned p = safe_malloc(2);
    int * _Owned q = p;
    _Unsafe {
        safe_free((void * _Owned)q);
    }
}
```

In the code rewritten using the BiShengC language, if we do nothing before the function exits, the compile error `"memory leak of value: q"` occurs, which avoids the memory leak problem;
if we call both `safe_free((void * _Owned)p)` and `safe_free((void * _Owned)q)` before the function exits, the compile error `"use of moved value: p"` occurs, which avoids the double free problem.

So, for more complex struct types, how should `safe_malloc` be used correctly for memory allocation?
For struct types, you first need to construct the corresponding variable on the stack, then pass it to `safe_malloc` to complete the corresponding memory allocation on the heap. The following code is a specific example:

```c
struct S {
    int * _Owned p;
    int * _Owned q;
};

_Safe void example(void) {
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * _Owned sp = safe_malloc(s);
    ...
}
```

## `safe_free`

`safe_free` is a safe memory release function provided by the BiShengC language.
The function takes a pointer of type `void * _Owned`, representing the address of the memory to be released.
The function's return value is of type `void`.
Therefore, before calling `safe_free` to release memory, you need to explicitly cast the `_Owned` pointer to the `void * _Owned` type. For the specific casting rules, see [Ownership State Transfer Rules - Type Casting](../chapter-3-memory-safety/1-ownership.md#explicit-casts).
Some specific examples of usage are given below.

```c
struct S {
    int * _Owned p;
    int * _Owned q;
};

_Safe void example(void) {
    int * _Owned pa = safe_malloc(199);
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * _Owned sp = safe_malloc(s);
    _Unsafe {
        safe_free((void * _Owned)pa);
        safe_free((void * _Owned)sp->p);
        safe_free((void * _Owned)sp->q);
        safe_free((void * _Owned)sp); // sp->p and sp->q must be freed first before sp can be freed
    }
}
```

## `safe_swap`

`safe_swap` is a function provided by the BiShengC language that safely swaps the values of two variables.
This function is a generic function that takes two parameters of type `T* _Borrow`, i.e. borrows of the values of the variables to be swapped.
The function's return value is of type `void`. The main purpose of this API is that, when swapping the values of two variables, it can simultaneously swap the ownership held by the two variables.
A specific example of usage is given below.

```c
_Owned struct S {
_Public:
    int* _Owned p;
    int* _Owned q;
    ~S(S this) {
        safe_free((void* _Owned)this.p);
        safe_free((void* _Owned)this.q);
    }
};

_Safe void example(void) {
    S s1 = { .p = safe_malloc(1), .q = safe_malloc(2) };
    S s2 = { .p = safe_malloc(3), .q = safe_malloc(4) };
    safe_swap(&_Mut s1, &_Mut s2); // after the swap, s1.p is 3 and s1.q is 4
}
```

## `forget`

`forget` is mainly used to take ownership of a variable and "forget" it. This function is a generic function that takes a variable of the generic type `T`, representing the value to be "forgotten":

1. If the variable is an _Owned pointer, then the memory the pointer points to will not be released;
2. If the variable is of an _Owned struct type, then its destructor will not be called.

In some special scenarios, a user wants to take ownership of a variable without releasing, via that variable, the underlying resource it manages (such as heap memory or a file handle, which may already have been transferred or released through raw pointer operations), for example:

```c
#include "bishengc_safety.hbs"
#include <string.h>
_Owned struct Resource {
_Public:
    char *_Owned s;
    ~Resource(This this) {
        safe_free((void *_Owned)this.s);
    }
};

void get_resource(char* val) {
    Resource r = { .s = safe_malloc<char>(100) };
    memcpy(val, (const void *)&r, sizeof(Resource)); // the resource in Resource is transferred
    forget<Resource>(r); // here the forget function takes ownership of r, but does not call Resource's destructor to release the heap memory
}

int main() {
    char val[sizeof(Resource)];
    get_resource(val);
    return 0;
}
```
