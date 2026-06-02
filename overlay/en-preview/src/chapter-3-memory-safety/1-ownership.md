# Ownership

## Preface

As a system-level programming language, C provides highly flexible direct manipulation of pointers and, through memory-management functions, the ability for developers to manually and finely control and manage memory. As a result, it is widely used in domains and scenarios that require direct interaction with system resources such as hardware or memory.
However, this memory-management model is prone to memory-safety problems such as memory leaks, use-after-free, null-pointer dereferences, buffer overflows, and out-of-bounds access.
Memory-safety problems not only waste resources but may also lead to incorrect program behavior or even crashes, threatening program stability.
Memory-safety problems can be divided into two broad categories: **temporal memory safety** and **spatial memory safety**. Temporal memory safety includes memory leaks, use-after-free, and null-pointer dereferences, while spatial memory safety includes buffer overflows and out-of-bounds access.
To solve a program's temporal memory-safety problems, BiSheng C's memory management leverages the **ownership feature** to check for potential memory-safety problems at compile time and identify potential temporal memory-safety errors.

## Feature Overview

BiSheng C's ownership feature is used to ensure that pointers and the memory they point to are managed correctly in a program.
In BiSheng C, the `_Owned` keyword is used to qualify a pointer type, indicating that the pointer owns the memory it points to.
A pointer that holds ownership must ensure that the memory it points to is explicitly released before the pointer's scope ends; otherwise there is a potential memory-leak error.
In addition, a block of heap memory can be owned by only one `_Owned` pointer at a time, and `_Owned` pointers have move semantics, which avoids memory-safety problems such as use-after-free.
The following is a piece of BiSheng C code that uses the ownership feature, to help understand it:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

int *_Owned foo(int *_Owned p) { return p; }

_Safe void bar(void) {
  // Use the provided safe_malloc to allocate a block of heap memory of size sizeof(int) and set its value to 2
  int *_Owned p = safe_malloc(2);
  // Transfer the heap memory pointed to by p to q; p can no longer be used to access this memory afterward, otherwise the compiler reports an error
  int *_Owned q = p;
  _Unsafe {
    // Transfer away q's ownership through a function parameter, but return ownership through the function return value
    q = foo(q);
    // Call safe_free before q's scope ends
    // to safely release the heap memory; not releasing it here would cause a memory-leak error
    safe_free((void *_Owned)q);
  }
  return;
}

int main() {
  bar();
  return 0;
}
```

In a safe zone, the memory pointed to by an `_Owned` pointer must be heap memory (such as heap memory allocated by the `safe_malloc` function); an `_Owned` pointer can never point to stack memory, and ownership must be transferred or the memory released (for example, released by the `safe_free` function) before its scope ends.

The prototypes of these two functions are as follows:

```c
T *_Owned safe_malloc<T>(T t);
void safe_free(void *_Owned);
```

You can use `safe_malloc` to allocate heap space large enough to hold a value of type `T` and initialize it to `t`. Before the `_Owned` pointer's lifetime ends, it must be released through `safe_free`; when using it, you need to cast the `T *_Owned` pointer to a `void *_Owned` pointer type before releasing it. For **multi-level pointers, you must release from the inside out**; for the case where a struct has `_Owned` pointer members inside it, you must **first release all the `_Owned` pointer members inside the struct before you can release the struct's `_Owned` pointer**.

**`_Owned` and generic type parameters**

In a generic function, when `_Owned` qualifies a generic type parameter (such as `T _Owned` or `_Owned T`), `_Owned` applies to the complete type `T`. When `T` is instantiated as `int*`, both `T _Owned` and `_Owned T` produce `int* _Owned` (an _Owned pointer), not `_Owned int*`. Using `_Owned` on a type parameter is permitted at generic-definition time; its validity is checked at instantiation time.

Function usage example:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  // Variable ownership initialization
  int *_Owned pi = safe_malloc(1);
  // Struct ownership initialization
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  // Multi-level pointer ownership initialization
  int *_Owned p = safe_malloc(1);
  int *_Owned *_Owned pp = safe_malloc(p);
  // Variable ownership release
  safe_free((void *_Owned)pi);
  // Struct ownership release (from inside out)
  safe_free((void *_Owned)s1->p);
  safe_free((void *_Owned)s1->q);
  safe_free((void *_Owned)s1);
  // Multi-level pointer ownership release (from inside out)
  safe_free((void *_Owned) * pp);
  safe_free((void *_Owned)pp);
}

int main() {
  foo();
  return 0;
}
```

## Syntax and Semantic Rules

To implement the ownership feature, BiSheng C introduces the `_Owned` keyword to qualify pointer-type variables. To distinguish owning pointers that point to arrays, BiSheng C also introduces the `_ArrayElem` keyword to qualify `_Owned` pointer types. Both keywords, like `const`, `restrict`, and `volatile`, are type qualifiers.

Specifically, the syntax and some of the semantic rules of the ownership feature are as follows:

1. The `_Owned` and `_ArrayElem` keywords are only allowed within a BiSheng C compilation unit;

2. The `_Owned` keyword is only allowed to qualify pointer types and may not qualify non-pointer types. When qualifying a multi-level pointer, the type qualification of each level of pointer may differ, with rules similar to `const`;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int _Owned a = 2; // error: the _Owned keyword must qualify a pointer

  double *_Owned q = safe_malloc(1.1);
  double _Owned b = 1.1; // error: the _Owned keyword must qualify a pointer

  int *_Owned *_Owned pp = safe_malloc(p); // ok: a multi-level pointer may be qualified
  safe_free((void *_Owned)pp); // error: when releasing a multi-level pointer, release from the inside out
  safe_free((void *_Owned) * pp); 

  double *d = (double *)malloc(sizeof(double));
  double **_Owned pd = safe_malloc(d);
  free(*pd); // free(d); also works directly
  safe_free((void *_Owned)pd);
  safe_free((void *_Owned)q);
  return 0;
}
```

3. The `_Owned` keyword is allowed to qualify struct pointers and the pointer members of a struct;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int m;
  int n;
};

struct R {
  int *_Owned p;
  double *_Owned q;
};

_Safe void test(void) {
  struct S s = {.m = 1, .n = 2};
  struct S *_Owned sp = safe_malloc(s);
  struct S _Owned so = {.m = 1, .n = 2}; // error: _Owned may not qualify a struct variable
  
  struct R r = {.p = safe_malloc(1), .q = safe_malloc(2.5)};
  struct R *_Owned rp = safe_malloc(r);
  safe_free((void *_Owned)sp);
  // First release the _Owned pointer members inside the struct
  safe_free((void *_Owned)rp->p);
  safe_free((void *_Owned)rp->q);
  // Then release the struct's _Owned pointer
  safe_free((void *_Owned)rp);
}

int main() {
  test();
  return 0;
}
```

4. `_Owned` may not qualify a `union` type or a member of a `union`, and none of a `union`'s members may have a member qualified by `_Owned`;

```c
struct S {
  int a;
  int b;
};

struct T {
  int *_Owned p;
  struct S s;
};

union A {
  int a;
  int *_Owned p; // error: a union member may not be qualified by _Owned
  struct S s;
  struct S *_Owned sp; // error: a union member may not be qualified by _Owned
  struct T t; // error: a union member struct may not have an _Owned member
  struct T *_Owned tp; // error: a union member may not be qualified by _Owned
  struct T *trp; // ok: the _Owned members of a variable pointed to by a raw pointer are not tracked
};
```

5. A type qualified by `_Owned`, or a type that has a member qualified by `_Owned`, may not be a member of an array, may not be the pointed-to type of an `_Owned _ArrayElem` pointer, nor a member thereof;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct A {
  int *_Owned p;
};

_Safe void test(void) {
  int *_Owned arr_i[2] = {safe_malloc(1), safe_malloc(2)}; // error: array members may not be qualified by _Owned
  struct A arr_a[2] = {{safe_malloc(1)}, {safe_malloc(2)}}; // error: array members may not be qualified by _Owned
}
_Safe struct A *_Owned _ArrayElem test2(void); // error: the pointed-to type of an _Owned _ArrayElem pointer may not contain an _Owned member
```

6. A pointer qualified by `_Owned` does not support subscript operations or arithmetic operations (pointer-offset operations) but does support comparison operations;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int *_Owned q = safe_malloc(3);
  
  p += 1; // error: _Owned pointers do not support arithmetic operators
  
  p[3] = 3; // error: _Owned pointers do not support the subscript operator

  if (p == q) { // ok: _Owned pointers support comparison operators
  }

  _Unsafe {
    safe_free((void *_Owned)p);
    safe_free((void *_Owned)q);
  }
  return 0;
}
```

7. A pointer qualified by `_Owned _ArrayElem` represents an owning pointer that points to an array. It supports subscript operations and comparison operations but not arithmetic operations. Its allocation and release can be done through `safe_malloc_array` and `safe_free_array`. Unless explicitly stated or listed separately for `_Owned _ArrayElem`, the rules for `_Owned` pointers also apply to `_Owned _ArrayElem`. `_ArrayElem` may not qualify a raw pointer.

```c
#include "bishengc_safety.hbs"
// _Safe T *_Owned _ArrayElem safe_malloc_array<T>(size_t size, T t);
// _Safe void safe_free_array(void *_Owned _ArrayElem);

_Safe int main(void) {
  int * _ArrayElem ptr; // error: _ArrayElem may not qualify a raw pointer
  int *_Owned _ArrayElem p = safe_malloc_array(10, 2);
  int *_Owned _ArrayElem q = safe_malloc_array(10, 3);

  p += 1; // error: _Owned _ArrayElem pointers do not support arithmetic operators

  p[3] = 3; // ok: _Owned _ArrayElem pointers support the subscript operator

  if (p == q) { // ok: _Owned _ArrayElem pointers support comparison operators
  }
  safe_free_array((void *_Owned _ArrayElem)p);
  safe_free_array((void *_Owned _ArrayElem)q);
}
```

8. Implicit type conversion is not allowed between a type qualified by `_Owned` and a type not qualified by `_Owned`;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe int main(void) {
  _Unsafe {
    int *b = (int *)malloc(sizeof(int));
    int *_Owned p = b; // error: _Owned pointers do not allow implicit type conversion
  }
  int *_Owned q = safe_malloc(3);
  int *c = q; // error: _Owned pointers do not allow implicit type conversion
  safe_free((void *_Owned)q);
  return 0;
}
```

9. Explicit casts between `_Owned` pointer types and raw pointer types are prohibited (including in non-safe zones). When a user needs to convert a raw pointer to an `_Owned` pointer, they must use the built-in function `__take_from_raw` to explicitly express the ownership transfer; when a user needs to convert an `_Owned` pointer to a raw pointer: if ownership needs to be transferred, they must use `__move_to_raw` to explicitly express the ownership transfer; if ownership does not need to be transferred, they must first take a borrow and then perform the conversion, for example `(T *)&_Mut *p`. All of the above conversions must be performed in a non-safe zone. The restriction on conversions between `_Owned` pointers and raw pointers only takes effect on the outermost pointer type; conversions of the form between `T*_Owned*` and `T**` are treated as conversions between raw pointers, which are not allowed in a safe zone but may be performed explicitly in a non-safe zone.

    When the pointed-to types are inconsistent, only an explicit cast between `void * _Owned` and `T * _Owned` is allowed. Such conversions have additional notes in [Ownership State Transfer Rules - Type Casts](#type-casts). Conversions between other `_Owned` pointers with inconsistent pointed-to types are not allowed.

    **Explicit ownership-transfer interfaces** (built-in functions):
    - `__move_to_raw(p)`: converts the `_Owned` pointer `p` to a raw pointer and transfers ownership; the returned raw pointer has the same nullability as the argument.
    - `__take_from_raw(p)`: converts the raw pointer `p` to an `_Owned` pointer and takes ownership of the object it points to; the returned `_Owned` pointer has the same nullability as the argument.

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release
int main() {
  int *pi1 = (int *)malloc(sizeof(int));
  int *_Owned pi2 = (int *_Owned)pi1;  // error: converting a raw pointer to an _Owned pointer is not allowed
  int *_Owned pi3 = __take_from_raw(pi1);  // ok: explicitly transfer ownership of a raw pointer to _Owned

  double *_Owned pd1 = safe_malloc(1.0);
  int *_Owned pi4 = (int *_Owned)pd1;   // error: pi4 and pd1 have inconsistent base types
  double *pd2 = __move_to_raw(pd1); // ok: explicitly transfer ownership of _Owned to a raw pointer

  double *_Owned pd3 = safe_malloc(1.5);
  void *_Owned pv1 = (void *_Owned)pd3; // ok: explicit conversion to void *_Owned is allowed

  void *_Owned * ppv1 = &pv1;
  void ** ppv2 = (void **) ppv1; // ok: explicit conversion from T *_Owned * to T ** is allowed
  ppv1 = (void *_Owned *) ppv2; // ok: explicit conversion from T ** to T *_Owned * is allowed
  safe_free((void *_Owned)pi3);
  safe_free(pv1);
  free((void*)pd2);
  return 0;
}
```

```c
// When not transferring ownership, first take a borrow and then convert to a raw pointer:
#include "bishengc_safety.hbs"
void foo(int *);
int main() {
  int *_Owned p = safe_malloc(1);
  int *raw_ptr = (int *)&_Mut *p; // ok: take a borrow first, then convert to a raw pointer, without transferring ownership
  foo(raw_ptr); // use raw_ptr
  safe_free((void *_Owned)p);
  return 0;
}
```

```c
// The conversion functions between _Owned and raw pointers preserve the nullability of the argument pointer
#include "bishengc_safety.hbs"
int main() {
  int * p1 = malloc(sizeof(int));
  if (p1 != nullptr) {
    int * _Owned p2 = __take_from_raw(p1); // after p1 is checked non-null, __take_from_raw(p1) can be assigned to the non-null p2
    safe_free((void*_Owned)p2);
  }
  p1 = malloc(sizeof(int));
  int * _Owned p3 = __take_from_raw(p1); // p1 is not null-checked; an error is reported here when null-pointer checking is enabled
  safe_free((void*_Owned)p3);
}
```

10. The conversion rules between `_Owned _ArrayElem` pointers and raw pointers are similar to those for `_Owned` pointers, but the array-version ownership-transfer interfaces must be used. C-style casts among `T *`, `T *_Owned`, and `T *_Owned _ArrayElem` are not allowed.

    Explicit ownership-transfer interfaces (built-in functions):
    - `__move_array_to_raw(p)`: converts an `_Owned _ArrayElem` pointer to a raw pointer and transfers ownership.
    - `__take_array_from_raw(p)`: converts the raw pointer `p` to an `_Owned _ArrayElem` pointer and takes ownership of the array it points to.

```c
#include <stdlib.h>

int main() {
  int *raw = (int *)malloc(4 * sizeof(int));
  int *_Owned _ArrayElem arr = __take_array_from_raw(raw); // ok
  int *_Owned plain = (int *_Owned)arr; // error: converting from T *_Owned _ArrayElem to T *_Owned is not allowed
  int *raw2 = __move_array_to_raw(arr); // ok
  int *_Owned _ArrayElem arr2 = (int *_Owned _ArrayElem)raw2; // error: __take_array_from_raw should be used
  free(raw2);
  return 0;
}
```

11. `_Owned` is allowed to qualify a pointer to a `_Trait`. Suppose there is a concrete type `S` that implements `_Trait T`, then:

    - The `S * _Owned` type can be implicitly converted to the `_Trait T * _Owned` type;
    - The `_Trait T * _Owned` type is allowed to be explicitly converted to the `void * _Owned` type.

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Trait T{};

_Impl _Trait T for int;

void test() {
  int *_Owned pi = safe_malloc(1);
  _Trait T *_Owned pti = pi; // ok: implicit conversion to _Trait T *_Owned
  void *_Owned pvi = (void *_Owned)pti; // ok: explicit conversion to void *_Owned
  safe_free(pvi);
}

int main() {
  test();
  return 0;
}
```

12. When calling a function through a function pointer, the rules are the same as for an ordinary function call: at the call site, the compiler checks whether the parameter types match the argument types and whether the return type matches the return-value type;

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

int deref_add(int *a, int *b) { return *a + *b; }
typedef int (*FTP)(int *, int *);
typedef int (*FTOP)(int *_Owned, int *);

void foo() {
  FTP ftp = deref_add;
  int *_Owned pa = safe_malloc(1);
  int *_Owned pb = safe_malloc(2);
  
  ftp(pa, pb); // error: type mismatch
  FTOP ftop = deref_add; // error: type mismatch
  safe_free((void *_Owned)pa);
  safe_free((void *_Owned)pb);
}

int main() {
  foo();
  return 0;
}
```

13. `_Owned` can qualify a _Trait type, i.e. `_Trait T* _Owned`, which also indicates that the variable owns the data stored within it.
    This type can be used as a type declaration, a function parameter type, and a function return type. However, conversion between `_Trait T* _Owned` and `_Trait T*` is not currently supported.

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Trait T { _Safe void release(This * _Owned this); };
struct IPv4 {
  char *buf1;
};
struct IPv6 {
  char *buf1;
  char *buf2;
};
_Safe void struct IPv4::release(struct IPv4 *_Owned this) {
  _Unsafe { free(this->buf1); }
  safe_free((void *_Owned)this);
}
_Safe void struct IPv6::release(struct IPv6 *_Owned this) {
  _Unsafe {
    free(this->buf1);
    free(this->buf2);
  }
  safe_free((void *_Owned)this);
}
_Impl _Trait T for struct IPv4;
_Impl _Trait T for struct IPv6;

void cleanup(_Trait T *_Owned t) { t->release(); }

int main() {
  struct IPv4 ipv4 = {.buf1 = "192.168.1.1"};
  struct IPv6 ipv6 = {.buf1 = "2001:0db8:85a3:0000",
                      .buf2 = "0000:8a2e:0370:7334"};
  struct IPv4 *_Owned sipv4 = safe_malloc(ipv4);
  struct IPv6 *_Owned sipv6 = safe_malloc(ipv6);
  _Trait T *_Owned tipv4 = sipv4;
  _Trait T *_Owned tipv6 = sipv6;
  // Use _Trait T* _Owned as a parameter
  tipv4->release();
  tipv6->release();
  safe_free((void *_Owned)tipv4);
  safe_free((void *_Owned)tipv6);
  return 0;
}
```

14. _Owned pointers can use logical operators.

```c
void logical_not(int *_Owned _Nullable p) {
  if (!p) { // equivalent: p == nullptr
  }
}
void logical_and(int *_Owned _Nullable p, int *_Owned _Nullable q) {
  if (p && q) { // equivalent: (p != nullptr) && (q != nullptr)
  }
}
void logical_or(int *_Owned _Nullable p, int *_Owned _Nullable q) {
  if (p || q) { // equivalent: (p != nullptr) || (q != nullptr)
  }
}
```

15. _Owned pointers are allowed as the condition of if, while, do-while, and for statements and of the ternary expression, but not as the condition of switch.

```c
void foo(int *_Owned _Nullable p) {
  if (p) { // equivalent: p != nullptr
  }
  while (p) { // equivalent: p!= nullptr
  }
  do {
  } while (p); // equivalent: p != nullptr

  for (;p;) { // equivalent: p != nullptr
  }
  swtich (p) { // error
  default:
    break;
  }
  int x = p ? 2 : 1; // equivalent: p != nullptr ? 2 : 1
}
```

16. In variable initialization, variable assignment, and passing function arguments, an `_Owned` pointer is allowed to be implicitly converted to `_Bool`, which does not consume the ownership of the content it points to.

```c
void foo(int *_Owned _Nullable p) {
  _Bool flag = p; // equivalent: _Bool flag = p != nullptr;
}
void bar(int *_Owned _Nullable p, _Bool flag) {
  flag = p; // equivalent: flag = p != nullptr;
}
void use(_Bool);
void baz(int *_Owned _Nullable p) {
  use(p); // equivalent: use(p != nullptr);
}
```

## Ownership State Transfer Rules

After understanding the syntax and some of the semantics of the ownership feature, this section elaborates on the ownership state transfer rules in detail.
To better understand the guarantees that the ownership feature brings to memory safety, you first need to understand the stack-and-heap memory model during program execution.

Overall, the memory during program execution can be divided into two parts, the stack area and the heap area, which together provide memory space for the program at runtime.
The stack area is the call stack, which holds all the information that must be maintained for program execution.
Whenever a function call occurs, a corresponding stack frame is created; the function call's context, the function's parameters, and the local variables inside the function body are stored in this stack frame.
The base of a stack frame is generally pointed to by the rbp register, and the top of the stack is pointed to by the rsp register; together the two registers identify a function's stack frame.
When a function call ends, the corresponding stack frame is destroyed before the function returns, and the corresponding memory space is freed.
This process is accomplished by adjusting the rbp register's value to the caller's stack-frame base and the rsp value to the caller's stack-top address, which is why variables in the stack area do not need to be explicitly freed.
As for the heap area, it stores data whose memory is dynamically allocated at runtime.
A typical example is an operation like `int *p = malloc(sizeof(int))`, where the operating system needs to find a suitably sized block of memory in the heap area for allocation and then store the address of this block in p; the pointer p is a stack variable of the program.
Although heap-area memory allocation is more flexible, it lacks organization, and incorrect memory management can easily lead to heap-area memory leaks.
For example, when a function call completes, its local variable p is destroyed, but if the heap memory it points to is not explicitly reclaimed by calling `free(p)`, this heap memory can never be reclaimed, producing a memory-safety error.

Using the ownership feature provided by BiSheng C, the `_Owned` keyword can be used to mark the pointers that need to be managed, so that potential errors can be detected at compile time, avoiding runtime errors.
The following are the core rules of BiSheng C ownership:

1. In BiSheng C, every value is owned by an `_Owned` pointer variable, and that `_Owned` pointer variable is the value's owner;
2. A value can be owned by only one `_Owned` pointer variable at a time, i.e. a value can have only one owner;
3. When an `_Owned` pointer variable goes out of scope, the heap memory of the value it owns must be released.

Based on the above core rules, the following gives a detailed introduction together with detailed code examples.

### Transferring Ownership

**1. Assigning a variable `s1` that holds ownership to another variable `s2` is a move; after this operation, the variable `s1` loses ownership of the value, and the original variable `s1` can no longer be used.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe void foo(void) {
  int *_Owned p = safe_malloc(10);
  int *_Owned q = p;
  int *_Owned r = p; // error: p's ownership has already been transferred to q
  _Unsafe {
    safe_free((void *_Owned)q);
    safe_free((void *_Owned)r);
  }
}

int main() {
  foo();
  return 0;
}
```

In this example, `p` holds ownership of a block of heap memory of size `sizeof(int)` that stores the value 10.
When declaring `q`, `p`'s ownership is transferred to `q`, and `p` no longer holds ownership of this heap memory.
Then when declaring `m`, `p`'s ownership can no longer be transferred to `m`, and the compiler reports an error here.
Therefore, using the ownership feature, it can be guaranteed that a value can have only one owner.
(So what would be the consequences without this rule? Three pointers would point to the same block of memory at the same time, and this memory would be released three times at the end of the scope, causing a double-free error.)

**2. When a variable `s1` that holds ownership is assigned as a whole to another variable `s2`, if `s1` contains other pointers that hold ownership, all of them are transferred to `s2`.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *p;
  int *_Owned q;
};

void test(void) {
  struct S s = {.p = (int *)malloc(sizeof(int)), .q = safe_malloc(1)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  int *_Owned p = s1->q; // error: q's ownership has been transferred along to s2
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
  safe_free((void *_Owned)p);
}

int main() {
  test();
  return 0;
}
```

In this example, ownership of the heap memory pointed to by `s1` is transferred to `s2`, but `s1` also contains a pointer `s1->q` that holds ownership, so its ownership is transferred along to `s2->q` as well, and subsequently using `s1->q` will report an error.

**3. When a variable `s1` that holds ownership is assigned as a whole to another variable `s2`, if `s1` contains other pointers that hold ownership, you must ensure that all other internal `_Owned` pointers hold ownership before `s1` can be assigned to `s2`.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  int *_Owned p = s1->p;
  struct S *_Owned s2 = s1; // error: s1 no longer has ownership of all its members
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)s2->p);
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
}

int main() {
  foo();
  return 0;
}
```

In this example, we first transfer away the ownership of `s1->p`, then attempt to transfer the ownership of `s1` as a whole to `s2`, but at this point `s1->p` no longer holds ownership of any block of heap memory, so this operation is illegal.

**4. After a variable `s1` that holds ownership loses its ownership, it can be made to hold ownership of some block of heap memory again through assignment, so that `s1` can be used again.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe void foo(void) {
  int *_Owned p = safe_malloc(10);
  // move p away
  int *_Owned q = p;
  // obtain ownership of a new element again
  p = safe_malloc(4);
  // can still transfer ownership to another pointer
  int *_Owned m = p;
  safe_free((void *_Owned)q);
  safe_free((void *_Owned)m);
}

int main() {
  foo();
  return 0;
}
```

In this example, after `p`'s ownership is transferred to `q`, the `safe_malloc` function is called again to give it ownership of a block of heap memory anew, so `p`'s ownership can still subsequently be transferred to `m`.

**5. Transferring ownership to a variable that already holds ownership is not allowed.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

_Safe void foo(void) {
  int *_Owned p = safe_malloc(12);
  int *_Owned q = safe_malloc(67);
  q = p; // error: q already holds ownership of another pointer
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)q);
}

int main() {
  foo();
  return 0;
}
```

In this example, an attempt is made to transfer `p`'s ownership to `q`, but `q` already holds ownership at this point; attempting another transfer would leak the heap memory that `q` originally pointed to, so the transfer cannot be performed and the compiler reports an error.

**6. If a variable `s1` holds ownership while the ownership of an internal `_Owned` pointer variable has been transferred away, then to give the internal variable ownership again, you must ensure that all parent `_Owned` pointer variables of the internal variable hold ownership.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  s1->p = safe_malloc(5); // error: s1 no longer holds ownership, so it cannot give ownership to an internal member
  safe_free((void *_Owned)s2->p);
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
}

int main() {
  foo();
  return 0;
}
```

In this example, the ownership of `s1`, `s1->p`, and `s1->q` is all transferred to `s2`; subsequently attempting to give `s1->p` ownership again is illegal because its parent `_Owned` pointer variable `s1` does not yet hold ownership, so the compiler reports an error.

### Memory Release at the End of Scope

**1. For all `_Owned` pointer variables, the compiler checks at the end of their lexical scope whether they still hold ownership of heap memory; if they still do, a memory-leak error exists.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  int *_Owned p = safe_malloc(2);
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  // safe_free((void *_Owned)p);
  // safe_free((void *_Owned)s2->p);
  // safe_free((void *_Owned)s2->q);
  // safe_free((void *_Owned)s2);
} // error: s2's internal members' ownership and s2's ownership are not released; p's ownership is not released

int main() {
  foo();
  return 0;
}
```

In this example, at the end of the scope the compiler finds that `p`, `s2`, `s2->p`, and `s2->q` still hold ownership of the heap memory they point to, i.e. none of this heap memory has been released, so compilation fails and a memory leak is reported.

### Type Casts

**1. A variable of type `T * _Owned` is allowed to be cast to type `void * _Owned`, but the condition for a successful conversion is that the variable still holds ownership and that none of its internal `_Owned` pointer variables hold ownership.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  int *_Owned p = s1->p;
  _Unsafe {
    safe_free((void *_Owned)s1); // error: s1 still holds ownership of an internal member (q), so it cannot be converted to void *_Owned
    safe_free((void *_Owned)p);
  }
}

int main() {
  foo();
  return 0;
}
```

In this example, an attempt is made to cast `s1` to type `void * _Owned`, but `s1->q` still holds ownership, so the conversion fails.

Note that when converting `T * _Owned` to `void *`, different conversion orders lead to different semantics:

1. First convert the pointed-to type of the `_Owned` pointer to `void`, then use `__move_to_raw` to convert the `_Owned` pointer to a raw pointer (i.e. `T * _Owned` to `void * _Owned` to `void *`): the `T * _Owned` pointer must hold ownership, but all `_Owned` pointer variables inside the `T` it points to must **not hold ownership** for the conversion to be performed.
2. First use `__move_to_raw` to convert the `_Owned` pointer to a raw pointer, then change the pointed-to type (i.e. `T * _Owned` to `T *` to `void *`): the `T * _Owned` pointer must hold ownership, and all `_Owned` pointer variables inside the `T` it points to must **still hold ownership** for the conversion to be performed.

When a user needs to convert `T * _Owned` to a raw pointer for release, they should first release its members and then use the first conversion order, converting `T * _Owned` to `void * _Owned` and then to a raw pointer for release. The compiler checks, when converting `T * _Owned` to `void * _Owned`, that the `_Owned` pointers inside `T` have all transferred their ownership, avoiding memory leaks. Except for cases where the object needs to be released, all other scenarios should use the second conversion order, retaining the ownership of the `_Owned` pointers inside `T * _Owned` so as to avoid subsequently using an already-released pointer.

The following is example code:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

void myfree(struct S *_Owned ptr) {
  // Release scenario: use the first order (T * _Owned to void * _Owned to void *)
  safe_free((void *_Owned) ptr->p);
  safe_free((void *_Owned) ptr->q);

  // free((void *)__move_to_raw(ptr)); // error: ptr no longer has internal ownership, so __move_to_raw cannot be used
  free(__move_to_raw((void *_Owned)ptr)); // ok
}
void *mymove(struct S *_Owned ptr) {
  // Other scenarios: use the second order (T * _Owned to T * to void *)
  // return __move_to_raw((void *_Owned)ptr); // error: ptr still has internal ownership, so it cannot be converted to void *_Owned
  return (void *)__move_to_raw(ptr); // ok
}
```

**2. A variable of type `void * _Owned` is allowed to be cast to type `T * _Owned`. The condition for a successful conversion is that the variable still holds ownership, and after a successful conversion, none of the `_Owned` pointer variables inside the resulting `T *_Owned` type hold ownership.**
The following is a code example with explanation:

```c
struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void *_Owned memAlloc(unsigned long);
_Safe void memFree(void *_Owned);

_Safe void foo(void) {
  struct S *_Owned sp = _Unsafe((struct S *_Owned)memAlloc(sizeof(struct S)));
  int *_Owned p = sp->p; // error: sp->p does not hold ownership at this point
  sp->q = _Unsafe((int *_Owned)memAlloc(sizeof(int)));
  *(sp->q) = 2;
  _Unsafe {
    memFree((void *_Owned)sp->q);
    memFree((void *_Owned)sp);
  }
}

int main() {
  foo();
  return 0;
}
```

In this example, an attempt is made to directly transfer the ownership of `sp->p` to `p`, but `sp->p` does not hold ownership at this point, so the compiler reports an error.

### Function Calls and Returns

**1. On function calls and returns, if a function's parameter or return value is of `_Owned` pointer type, the argument passed in and the return value are required to hold ownership of heap memory.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // A header file provided by BiSheng C for safe memory allocation and release

struct S {
  int *_Owned p;
  int *_Owned q;
};

struct S *_Owned foo(struct S s) {
  struct S *_Owned ret = safe_malloc(s);
  return ret;
}

void bar(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  int *_Owned p = s.p;
  struct S *_Owned s1 = foo(s); // error: s no longer holds ownership of all its internal members
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)s1->p);
  safe_free((void *_Owned)s1->q);
  safe_free((void *_Owned)s1);
}

int main() {
  bar();
  return 0;
}
```

In this example, the struct variable `s` passed into the `F` function has two `_Owned` pointer variables inside it, and `s.p` has already been transferred away, so this function call is illegal and the compiler reports an error.

## Source-to-Source Translation

The BiSheng C clang compiler supports source-to-source translation, i.e. converting a `.cbs` file into an equivalent `.c` file.
The ownership feature only introduces the `_Owned` keyword to represent ownership; during source-to-source translation, all `_Owned` keywords are simply removed and the corresponding `.c` code is generated.
For detailed information about source-to-source translation, please refer to the Source-to-Source Translation chapter of this manual.
