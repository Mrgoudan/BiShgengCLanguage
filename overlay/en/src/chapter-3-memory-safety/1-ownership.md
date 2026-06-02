# Ownership

## Preface

As a system-level programming language, C provides highly flexible direct manipulation of pointers as well as the ability for developers to manually and finely control and manage memory through memory-management functions. For this reason it is widely applied in all kinds of domains and scenarios that require direct interaction with system resources such as hardware or memory.
This memory-management model, however, is prone to memory-safety problems such as memory leaks, use-after-free, null pointer dereferences, buffer overflows, and out-of-bounds access.
Memory-safety problems not only waste resources but may also cause incorrect program behavior, and can even crash the program, posing a threat to its stability.
Memory-safety problems can be divided into two broad categories: **temporal memory safety** and **spatial memory safety**, where temporal memory safety covers memory leaks, use-after-free, null pointer dereferences, and so on, and spatial memory safety covers buffer overflows, out-of-bounds access, and so on.
To address a program's temporal memory-safety problems, the BiSheng C language's memory management uses the **ownership feature** to check potential memory-safety problems at compile time and identify potential temporal memory-safety errors.

## Feature Overview

The ownership feature of the BiSheng C language is used to ensure that pointers in a program and the memory they point to can be managed correctly.
In BiSheng C, the `_Owned` keyword is used to qualify a pointer type, indicating that the pointer holds ownership of the memory it points to.
A pointer that holds ownership must ensure that the memory it points to is explicitly released before the pointer goes out of scope; otherwise there is a potential memory-leak error.
In addition, a block of heap memory can be owned by only one `_Owned` pointer at a time. `_Owned` pointers have move semantics, which avoids memory-safety problems such as use-after-free.
The following is a piece of BiSheng C code that uses the ownership feature, to help you understand it:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

int *_Owned foo(int *_Owned p) { return p; }

_Safe void bar(void) {
  // allocate a block of heap memory of size sizeof(int) via the provided safe_malloc, and set the value to 2
  int *_Owned p = safe_malloc(2);
  // transfer the heap memory pointed to by p to q; afterwards p can no longer be used to access this memory, otherwise compilation fails
  int *_Owned q = p;
  _Unsafe {
    // transfer away q's ownership through the function parameter, but return the ownership through the function return value
    q = foo(q);
    // call safe_free before q goes out of scope
    // to safely free the heap memory; not freeing here would report a memory-leak error
    safe_free((void *_Owned)q);
  }
  return;
}

int main() {
  bar();
  return 0;
}
```

In the safe zone, the memory pointed to by an `_Owned` pointer must be heap memory (for example, heap memory obtained via the `safe_malloc` function); an `_Owned` pointer can never point to stack memory, and its ownership must be transferred or its memory released (for example, freed via the `safe_free` function) before it goes out of scope.

The prototypes of these two functions are as follows:

```c
T *_Owned safe_malloc<T>(T t);
void safe_free(void *_Owned);
```

You can use `safe_malloc` to allocate heap space large enough to hold a value of type `T`, initialized to `t`. Before the `_Owned` pointer's lifetime ends, it must be released via `safe_free`. When freeing, you must cast the `T *_Owned` pointer to a `void *_Owned` pointer type before releasing it. For **multi-level pointers, release them from the innermost to the outermost**; for a struct that has `_Owned` pointer members, you must **release all the `_Owned` pointer members inside the struct first, and only then release the struct's `_Owned` pointer**.

**`_Owned` and generic type parameters**

In a generic function, when `_Owned` qualifies a generic type parameter (such as `T _Owned` or `_Owned T`), `_Owned` applies to the complete type `T`. When `T` is instantiated as `int*`, both `T _Owned` and `_Owned T` produce `int* _Owned` (an _Owned pointer), rather than `_Owned int*`. Using `_Owned` on a type parameter is permitted at generic-definition time; its validity is checked at instantiation.

Example of function usage:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  // variable ownership initialization
  int *_Owned pi = safe_malloc(1);
  // struct ownership initialization
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  // multi-level pointer ownership initialization
  int *_Owned p = safe_malloc(1);
  int *_Owned *_Owned pp = safe_malloc(p);
  // variable ownership release
  safe_free((void *_Owned)pi);
  // struct ownership release (from inner to outer)
  safe_free((void *_Owned)s1->p);
  safe_free((void *_Owned)s1->q);
  safe_free((void *_Owned)s1);
  // multi-level pointer ownership release (from inner to outer)
  safe_free((void *_Owned) * pp);
  safe_free((void *_Owned)pp);
}

int main() {
  foo();
  return 0;
}
```

## Grammar and Semantic Rules

To implement the ownership feature, the BiSheng C language introduces the `_Owned` keyword to qualify pointer-type variables.
Like `const`, `restrict`, and `volatile`, this keyword is a type qualifier; its grammar is as follows:

```text
type-qualifier:
  const | restrict | volatile | ownership-qualifier

ownership-qualifier:
  _Owned
```

Specifically, the ownership feature's grammar and some of its semantic rules are as follows:

1. The `_Owned` keyword is allowed only within a BiSheng C compilation unit.

2. The `_Owned` keyword is allowed only to qualify pointer types, not non-pointer types. When qualifying multi-level pointers, the type qualification of each level may differ, with rules similar to `const`.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int _Owned a = 2; // error: the _Owned keyword must qualify a pointer

  double *_Owned q = safe_malloc(1.1);
  double _Owned b = 1.1; // error: the _Owned keyword must qualify a pointer

  int *_Owned *_Owned pp = safe_malloc(p); // ok: multi-level pointers may be qualified
  safe_free((void *_Owned)pp); // error: when freeing multi-level pointers, free from inner to outer
  safe_free((void *_Owned) * pp); 

  double *d = (double *)malloc(sizeof(double));
  double **_Owned pd = safe_malloc(d);
  free(*pd); // free(d); also works directly
  safe_free((void *_Owned)pd);
  safe_free((void *_Owned)q);
  return 0;
}
```

3. The `_Owned` keyword is allowed to qualify struct pointers and a struct's pointer members.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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
  // first release the _Owned pointer members inside the struct
  safe_free((void *_Owned)rp->p);
  safe_free((void *_Owned)rp->q);
  // then release the struct _Owned pointer
  safe_free((void *_Owned)rp);
}

int main() {
  test();
  return 0;
}
```

4. `_Owned` may not qualify a `union` type or a member of a `union`, and no member of a `union` may have an `_Owned`-qualified member.

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
  int *_Owned p; // error: a union member may not be qualified with _Owned
  struct S s;
  struct S *_Owned sp; // error: a union member may not be qualified with _Owned
  struct T t; // error: a union member struct may not have an _Owned member
  struct T *_Owned tp; // error: a union member may not be qualified with _Owned
  struct T *trp; // ok: the _Owned members of variables pointed to by raw pointers are not tracked
};
```

5. A type qualified with `_Owned`, or a type that has an `_Owned`-qualified member, may not be an array element.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct A {
  int *_Owned p;
};

_Safe void test(void) {
  int *_Owned arr_i[2] = {safe_malloc(1), safe_malloc(2)}; // error: array elements may not be qualified with _Owned
  struct A arr_a[2] = {{safe_malloc(1)}, {safe_malloc(2)}}; // error: array elements may not be qualified with _Owned
}
```

6. An `_Owned`-qualified pointer does not support arithmetic operators (pointer-offset operations), but it does support comparison operators.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int *_Owned q = safe_malloc(3);
  
  p += 1; // error: _Owned pointers do not support arithmetic operators
  
  p[3] = 3; // error: _Owned pointers do not support the [] operator

  if (p == q) { // ok: _Owned pointers support comparison operators
  }

  _Unsafe {
    safe_free((void *_Owned)p);
    safe_free((void *_Owned)q);
  }
  return 0;
}
```

7. Implicit type conversion is not allowed between `_Owned`-qualified types and non-`_Owned`-qualified types.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

8. Explicit casts between `_Owned` pointer types and raw pointer types are forbidden (including outside the safe zone). When a user needs to convert a raw pointer to an `_Owned` pointer, they must use the built-in function `__take_from_raw` to explicitly express the ownership transfer; when a user needs to convert an `_Owned` pointer to a raw pointer: if ownership is to be transferred, they must use `__move_to_raw` to explicitly express the ownership transfer; if ownership is not to be transferred, they must first take a borrow and then perform the conversion, for example `(T *)&_Mut *p`. All of the above conversions must be performed outside the safe zone. The restriction on conversions between `_Owned` pointers and raw pointers applies only to the outermost pointer type; conversions of the form `T*_Owned*` to `T**` are treated as conversions between raw pointers, which are not allowed in the safe zone but may be performed explicitly outside the safe zone.

    When the base types are inconsistent, only explicit casts between the `void * _Owned` type and a `T * _Owned` type are allowed. Such conversions are described further in [ownership state-transfer rules - Explicit Casts](#explicit-casts).

    **Explicit ownership-transfer interfaces** (built-in functions):
    - `__move_to_raw(p)`: converts the `_Owned` pointer `p` to a raw pointer and transfers ownership; the returned raw pointer has the same nullability as the argument.
    - `__take_from_raw(p)`: converts the raw pointer `p` to an `_Owned` pointer and takes ownership of the object it points to; the returned `_Owned` pointer has the same nullability as the argument.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely
int main() {
  int *pi1 = (int *)malloc(sizeof(int));
  int *_Owned pi2 = (int *_Owned)pi1;  // error: converting a raw pointer to an _Owned pointer is not allowed
  int *_Owned pi3 = __take_from_raw(pi1);  // ok: explicitly transfer ownership from the raw pointer to _Owned

  double *_Owned pd1 = safe_malloc(1.0);
  int *_Owned pi4 = (int *_Owned)pd1;   // error: pi4 and pd1 have inconsistent base types
  double *pd2 = __move_to_raw(pd1); // ok: explicitly transfer _Owned ownership to a raw pointer

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
// when not transferring ownership, take a borrow first and then convert to a raw pointer:
#include "bishengc_safety.hbs"
void foo(int *);
int main() {
  int *_Owned p = safe_malloc(1);
  int *raw_ptr = (int *)&_Mut *p; // ok: take a borrow first and then convert to a raw pointer, without transferring ownership
  foo(raw_ptr); // use raw_ptr
  safe_free((void *_Owned)p);
  return 0;
}
```

```c
// conversion functions between _Owned and raw pointers preserve the nullability of the argument pointer
#include "bishengc_safety.hbs"
int main() {
  int * p1 = malloc(sizeof(int));
  if (p1 != nullptr) {
    int * _Owned p2 = __take_from_raw(p1); // after p1 is checked non-null, __take_from_raw(p1) can be assigned to the non-null p2
    safe_free((void*_Owned)p2);
  }
  p1 = malloc(sizeof(int));
  int * _Owned p3 = __take_from_raw(p1); // p1 is not null-checked; this reports an error when null-pointer checking is enabled
  safe_free((void*_Owned)p3);
}
```

9. `_Owned` is allowed to qualify a pointer to a `_Trait`. Suppose there is a concrete type `S` that implements `_Trait T`, then:

    - The `S * _Owned` type can be implicitly converted to the `_Trait T * _Owned` type;
    - The `_Trait T * _Owned` type can be explicitly converted to the `void * _Owned` type.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

10. When a function is called through a function pointer, the rules are the same as for an ordinary function call: at the call site, it is checked whether the parameter types match the argument types and whether the return type matches the type of the return value.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

11. `_Owned` can qualify a _Trait type, that is, `_Trait T* _Owned`, which also indicates that the variable holds ownership of the data stored inside it.
    This type can serve as a type declaration, a function parameter type, and a function return value type. However, conversion between `_Trait T* _Owned` and `_Trait T*` is not currently supported.

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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
  // use _Trait T* _Owned as a parameter
  tipv4->release();
  tipv6->release();
  safe_free((void *_Owned)tipv4);
  safe_free((void *_Owned)tipv6);
  return 0;
}
```

12. _Owned pointers can use logical operators.

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

13. An _Owned pointer is allowed as the condition of `if`, `while`, `do-while`, and `for` statements and ternary expressions, but is not allowed as the condition of a `switch`.

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

14. In variable initialization, variable assignment, and function argument passing, an `_Owned` pointer is allowed to be implicitly converted to `_Bool`, which does not consume the ownership of the content it points to.

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

## Ownership State-Transfer Rules

After understanding the grammar and some of the semantics of the ownership feature, this section explains the ownership state-transfer rules in detail.
To better understand the guarantees that the ownership feature provides for memory safety, you first need to understand the stack/heap memory model when a program executes.

Broadly speaking, when a program executes, memory can be divided into two parts: the stack area and the heap area, which together provide memory space for the program at runtime.
The stack area is the call stack, which holds all the information that must be maintained for program execution.
Each time a function call occurs, a corresponding stack frame is created, and the function call's context, the function's parameters, and the local variables within the function body are stored in this stack frame.
The base of a stack frame is generally pointed to by the rbp register, and the top of the stack is pointed to by the rsp register; together these two registers identify a function's stack frame.
When a function call ends, the corresponding stack frame is destroyed before the function returns, and the corresponding memory space is thereby released.
This process is accomplished by adjusting the value of the rbp register to the caller's stack-frame base and the value of rsp to the caller's stack-top address; this is also why stack-area variables do not need to be released explicitly.
The heap area, by contrast, stores data whose memory is dynamically allocated at runtime.
A typical example is an operation like `int *p = malloc(sizeof(int))`, where the operating system needs to find a suitably sized block of memory in the heap area for allocation and then store the address of this memory in p; the pointer p is one of the program's stack variables.
Although heap-area memory allocation is more flexible, it lacks organization, and incorrect memory management can easily cause heap memory leaks.
For example, after a function call completes, its local variable p is destroyed, but if the heap memory it points to is not explicitly reclaimed by calling `free(p)`, then this heap memory can never be reclaimed, producing a memory-safety error.

Using the ownership feature provided by the BiSheng C language, you can use the `_Owned` keyword to mark the pointers that need to be managed, so that potential errors can be detected at compile time, avoiding errors at runtime.
The following are the core rules of BiSheng C ownership:

1. In BiSheng C, every value is owned by an `_Owned` pointer variable, and that `_Owned` pointer variable is the value's owner.
2. A value can be owned by only one `_Owned` pointer variable at a time, that is, a value can have only one owner.
3. When an `_Owned` pointer variable goes out of scope, the heap memory where the value it owns resides must be released.

Based on the core rules above, the following gives a concrete introduction with detailed code examples.

### Transferring Ownership

**1. Assigning a variable `s1` that holds ownership to another variable `s2` is a move; after this operation, `s1` loses ownership of the value, and the original variable `s1` can no longer be used.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

In this example, `p` holds ownership of a block of heap memory whose size is `sizeof(int)` and whose stored value is 10.
When `q` is declared, `p`'s ownership is transferred to `q`, and `p` no longer holds ownership of this block of heap memory.
Then when `m` is declared, `p`'s ownership can no longer be transferred to `m`, and the compiler reports an error here.
Therefore, using the ownership feature, it is guaranteed that a value can have only one owner.
(So what would happen without this rule? Three pointers would point to the same block of memory at the same time, and at the end of scope this memory would be freed three times, causing a double-free error.)

**2. When a variable `s1` that holds ownership is assigned as a whole to another variable `s2`, if `s1` still has other pointers that hold ownership inside it, they are all transferred to `s2`.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct S {
  int *p;
  int *_Owned q;
};

void test(void) {
  struct S s = {.p = (int *)malloc(sizeof(int)), .q = safe_malloc(1)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  int *_Owned p = s1->q; // error: q's ownership has been transferred to s2 along with the rest
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
  safe_free((void *_Owned)p);
}

int main() {
  test();
  return 0;
}
```

In this example, the ownership of the heap memory pointed to by `s1` is transferred to `s2`, but `s1` also has a pointer `s1->q` that holds ownership inside it, so its ownership is also transferred together to `s2->q`. Subsequently using `s1->q` will report an error.

**3. When a variable `s1` that holds ownership is assigned as a whole to another variable `s2`, if `s1` still has other pointers that hold ownership inside it, it can only be assigned to `s2` if all the other internal `_Owned` pointers hold ownership.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

In this example, we first transfer away the ownership of `s1->p`, and then attempt to transfer the ownership of `s1` as a whole to `s2`, but at this point `s1->p` no longer holds ownership of any block of heap memory, so this operation is illegal.

**4. After a variable `s1` that holds ownership loses its ownership, it can be given ownership of some block of heap memory again through assignment, so that `s1` can be used again.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

_Safe void foo(void) {
  int *_Owned p = safe_malloc(10);
  // move p away
  int *_Owned q = p;
  // obtain ownership of a new element again
  p = safe_malloc(4);
  // ownership can still be given to another pointer
  int *_Owned m = p;
  safe_free((void *_Owned)q);
  safe_free((void *_Owned)m);
}

int main() {
  foo();
  return 0;
}
```

In this example, after `p`'s ownership is transferred to `q`, the `safe_malloc` function is called again to give it ownership of a new block of heap memory, so `p`'s ownership can still be transferred to `m` afterwards.

**5. Transferring ownership to a variable that already holds ownership is not allowed.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

In this example, an attempt is made to transfer `p`'s ownership to `q`, but `q` already holds ownership at this point; attempting the transfer would leak the heap memory `q` originally pointed to, so the transfer cannot be performed and an error is reported at compile time.

**6. If a variable `s1` holds ownership but its internal `_Owned` pointer variable's ownership has been transferred away, then to give the internal variable ownership again, you must ensure that all of the internal variable's parent `_Owned` pointer variables hold ownership.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  s1->p = safe_malloc(5); // error: s1 no longer holds ownership, so its internal member cannot be given ownership
  safe_free((void *_Owned)s2->p);
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
}

int main() {
  foo();
  return 0;
}
```

In this example, the ownership of `s1`, `s1->p`, and `s1->q` is all transferred to `s2`. Subsequently attempting to give `s1->p` ownership is illegal because its parent `_Owned` variable pointer `s1` does not yet hold ownership, so an error is reported at compile time.

### Memory Release at End of Scope

**1. For every `_Owned` pointer variable, when its lexical scope ends, it is checked whether it still holds ownership of heap memory; if it still holds ownership, there is a memory-leak error.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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
} // error: the ownership of s2's internal members and of s2 itself is not released; p's ownership is not released

int main() {
  foo();
  return 0;
}
```

In this example, when the scope ends, the compiler will find that `p`, `s2`, `s2->p`, and `s2->q` still hold ownership of the heap memory they point to, that is, none of this heap memory has been released, so compilation fails and a memory leak is reported.

### Explicit Casts

**1. A variable of type `T * _Owned` may be converted to type `void * _Owned` via an explicit cast, but the condition for a successful conversion is that the variable still holds ownership and that all of its internal `_Owned` pointer variables no longer hold ownership.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  int *_Owned p = s1->p;
  _Unsafe {
    safe_free((void *_Owned)s1); // error: s1 still holds ownership of its internal member (q), so it cannot be converted to void *_Owned
    safe_free((void *_Owned)p);
  }
}

int main() {
  foo();
  return 0;
}
```

In this example, an attempt is made to explicitly cast `s1` to the `void * _Owned` type, but `s1->q` still holds ownership, so the conversion fails.

Note that when a `T * _Owned` to `void *` conversion is involved, different conversion orders lead to different semantics:

1. First convert the `_Owned` pointer's pointee type to `void`, then use `__move_to_raw` to convert the `_Owned` pointer to a raw pointer (i.e. `T * _Owned` to `void * _Owned` to `void *`): the `T * _Owned` pointer must hold ownership, but all the `_Owned` pointer variables inside the `T` it points to must **not hold ownership** for the conversion to proceed.
2. First use `__move_to_raw` to convert the `_Owned` pointer to a raw pointer, then change the pointee type (i.e. `T * _Owned` to `T *` to `void *`): the `T * _Owned` pointer must hold ownership, and all the `_Owned` pointer variables inside the `T` it points to must **still hold ownership** for the conversion to proceed.

When a user needs to convert a `T * _Owned` to a raw pointer in order to free it, they should first free its members, then use the first conversion order, converting `T * _Owned` to `void * _Owned` and then to a raw pointer for freeing. The compiler checks, at the point of converting `T * _Owned` to `void * _Owned`, that all `_Owned` pointers inside `T` have transferred their ownership, avoiding memory leaks. Except for the case where an object needs to be freed, all other scenarios should use the second conversion order, preserving the ownership of the `_Owned` pointers inside the `T * _Owned`, so as to avoid later use of a pointer that has already been freed.

The following is the example code:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

struct S {
  int *_Owned p;
  int *_Owned q;
};

void myfree(struct S *_Owned ptr) {
  // free scenario: use the first order (T * _Owned to void * _Owned to void *)
  safe_free((void *_Owned) ptr->p);
  safe_free((void *_Owned) ptr->q);

  // free((void *)__move_to_raw(ptr)); // error: ptr no longer holds internal ownership, so __move_to_raw cannot be used
  free(__move_to_raw((void *_Owned)ptr)); // ok
}
void *mymove(struct S *_Owned ptr) {
  // other scenarios: use the second order (T * _Owned to T * to void *)
  // return __move_to_raw((void *_Owned)ptr); // error: ptr still has internal ownership, so it cannot be converted to void *_Owned
  return (void *)__move_to_raw(ptr); // ok
}
```

**2. A variable of type `void * _Owned` may be converted to type `T * _Owned` via an explicit cast, where the condition for a successful conversion is that the variable still holds ownership; after a successful conversion, none of the `_Owned` pointer variables inside the resulting `T *_Owned` type hold ownership.**
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

In this example, an attempt is made to directly transfer the ownership of `sp->p` to `p`, but at this point `sp->p` does not yet hold ownership, so compilation reports an error.

### Function Calls and Returns

**1. On a function call and return, if the function's parameter or the function's return value is of `_Owned` pointer type, then the argument passed in and the return value must hold ownership of heap memory.**
The following is a code example with explanation:

```c
#include "bishengc_safety.hbs" // header file provided by the BiSheng C language, used to allocate and free memory safely

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

In this example, the struct variable `s` passed into the `F` function has two `_Owned` pointer variables inside it, and `s.p` has already been transferred away, so this function call is illegal and compilation reports an error.

## Source-to-Source Translation

The clang compiler of the BiSheng C language supports source-to-source translation, that is, converting a `.cbs` file into an equivalent `.c` file.
The ownership feature only introduces the `_Owned` keyword to represent ownership; during source-to-source translation, all `_Owned` keywords are simply removed, and the corresponding `.c` code is generated.
For details on source-to-source translation, please refer to the source-to-source translation chapter of this manual.
