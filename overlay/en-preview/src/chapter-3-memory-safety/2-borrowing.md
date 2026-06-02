# Borrowing

As an important part of BiSheng C's memory safety features, borrowing is a complement to ownership. The previous section described the ownership feature; the entity that owns a resource is responsible for releasing it. This section introduces borrowing a resource.

## Feature Overview

If we only had ownership types, code would be very limited, since operations such as function calls and assignments all transfer ownership. When programming, we often need to express the concept of "borrowing a resource", as distinct from "owning a resource". Just as in real life, if a person owns something, you can borrow it from them, and when you are done using it, you must return it to its owner.

### Definition of Borrowing and the Borrow Operators

In BiSheng C, **a borrow is a pointer type that points to the memory address where the borrowed object is stored**. To express the concept of borrowing:

1. A new keyword `_Borrow` is introduced. Use `_Borrow` to qualify a pointer type `T*`, denoting a borrow type of T. `_ArrayElem` may also qualify a pointer type `T*` together with `_Borrow`, denoting a borrow type of an array member of T.
2. The borrow operators `&_Mut` and `&_Const` are introduced. Here, `&_Mut e` obtains a **mutable borrow** of expression e, and `&_Const e` obtains a **read-only borrow** of expression `e`. Here, expression `e` is required to be an lvalue. Similar to the address-of operator `&` in standard C, the borrow operators actually obtain the address of expression `e`.

For example, we can create a mutable borrow `p1` and an immutable borrow `p2` of the local variable `local`, and use them:

```C
void use_immut(const int *_Borrow p) {}
void use_mut(int *_Borrow p) {}

void foo() {
  int local = 5;
  // p1 is a mutable borrow pointer to local
  int *_Borrow p1 = &_Mut local;
  use_mut(p1);
  // p2 is an immutable borrow pointer to local
  const int *_Borrow p2 = &_Const local;
  use_immut(p2);
}

int main() {
  foo();
  return 0;
}
```

In addition, if expression `e` is a pointer dereference expression, `&_Mut *p` and `&_Const *p` can be viewed as taking a mutable borrow and an immutable borrow, respectively, of the value stored at address `p`, that is, of `*p`. **This operation does not produce a temporary variable for `*p`.** Here, `p` may be a raw pointer, an `_Owned` pointer, or another borrow pointer. For example:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

void foo() {
  int *x1 = malloc(sizeof(int));
  *x1 = 1;
  // p1 borrows *x1
  int *_Borrow p1 = &_Mut * x1;
  int *_Owned x2 = safe_malloc(2);
  // p2 borrows *x2
  int *_Borrow p2 = &_Mut * x2;
  int local = 3;
  int *_Borrow x3 = &_Mut local;
  // p3 borrows *x3
  int *_Borrow p3 = &_Mut * x3;
  safe_free((void *_Owned)x2);
}

int main() {
  foo();
  return 0;
}
```

If expression `e` is an array subscript expression, then the result type of `&_Mut e` and `&_Const e` carries the two qualifiers `_Borrow _ArrayElem`, denoting a borrow pointer that "points to an array element". Apart from being allowed to use subscript `[]` and arithmetic operations, and having extra type-conversion rules, such a borrow pointer follows the same rules as an ordinary `_Borrow` pointer. Unless the `_Borrow _ArrayElem` case is explicitly stated or listed separately, the rules for `_Borrow` pointers apply equally to `_Borrow _ArrayElem`.

```C
void f1(int *_Borrow _ArrayElem p) {}
void f2(const int *_Borrow _ArrayElem p) {}

void foo() {
  int arr[4] = {1, 2, 3, 4};
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  const int *_Borrow _ArrayElem q = &_Const arr[1];
  f1(&_Mut arr[2]);
  f2(&_Const arr[3]);
}
```

### The Purpose of Borrowing

Suppose we have a requirement like this: create a file, and call some operation functions to read from and write to the file. Without the concept of borrowing, calling a file-operation function would transfer ownership of the file pointer. So that the file pointer can still be used after the function call, we would need to return ownership back to the caller:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *_Owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *_Owned p) { safe_free((void *_Owned)p); }

MyFile *_Owned insert_str(MyFile *_Owned p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // Transfer ownership to the caller via the return value, to avoid losing ownership
  return p;
}

MyFile *_Owned other_operation(MyFile *_Owned p) {
  // some operation
  // Transfer ownership to the caller via the return value, to avoid losing ownership
  return p;
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // Ownership of p is first moved into insert_str, then transferred back to the caller via the return value
  p = insert_str(p, str);
  p = other_operation(p);
  file_safe_free(p);
  return 0;
}
```

This style causes frequent ownership transfers of the file pointer, which is error-prone when the code logic is complex, and if ownership is transferred away but not returned, the file pointer can no longer be used afterward. With borrowing, by passing a borrow of the file pointer as the argument to an operation function, the file pointer can still be used for other subsequent operations after the function returns. There is no longer any need, as in the previous example, to first pass ownership in via a function parameter and then pass it back out via the function return value, making the code more concise:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *_Owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *_Owned p) { safe_free((void *_Owned)p); }

void insert_str(MyFile *_Borrow p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // No need to return ownership
}

void other_operation(MyFile *_Borrow p) {
  // some operation
  // No need to return ownership
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // Ownership is not moved
  insert_str(&_Mut * p, str);
  // Ownership is not moved
  other_operation(&_Mut * p);
  file_safe_free(p);
  return 0;
}
```

## Lifetimes of Borrow Variables and Borrowed Objects

### Lifetimes and Their Purpose

We can take a borrow of various kinds of objects: `_Owned` variables, local variables of non-`_Owned` types, global variables, temporary anonymous variables, parameters, and even part of a composite variable. To correctly represent the valid scope of borrow variables and of the various kinds of borrowed objects, we introduce the concept of lifetime.

The primary purpose of lifetime checking is to avoid dangling pointers, which cause a program to use data that it should not use. The following C code is a typical example of using a dangling pointer:

```C
int main() {
  int *p;
  {
    int local = 5;
    p = &local;
  }
  *p = 1;
  return 0;
}
```

Two things are worth noting about this C code:

1. Declaring `int *p` carries the risk of using `NULL`;
2. `p` points to the variable `local` in the inner block, but `local` is freed when the block ends. Therefore, after returning to the outer block, `p` points to an invalid address and is a dangling pointer that points to the prematurely freed variable `local`. As expected, `*p = 1` causes undefined behavior at runtime in this program. When the code logic is complex, such anomalous behavior is very hard to detect.

Regarding the second point, BiSheng C stipulates: **a borrow of a resource must not outlive the owner of that resource.** That is, the lifetime of a borrow variable must not be longer than the lifetime of the borrowed object.

Next, we rewrite the above C code using BiSheng C's borrowing feature. By checking the lifetimes of borrow variables and borrowed objects, the potential memory-safety risk can be identified at compile time:

```C
int main() {
  int local1 = 1;
  // The borrow pointer variable p must be initialized before use, otherwise an error is reported
  int *_Borrow p = &_Mut local1;
  {
    int local2 = 2;
    // After reassigning p, p no longer borrows local1 but borrows local2
    p = &_Mut local2;
  }
  *p = 3; // error: local2 does not live long enough
  return 0;
}
```

### Borrow Variables and Borrowed Objects

Every borrow variable (i.e. a _Borrow pointer variable) has one or more borrowed objects, for example:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

struct S {
  int a;
};

int *_Borrow bar(int *_Borrow, int *_Borrow);

int g = 5;
void foo(int a, int *_Owned b, int *c, struct S d) {
  // The borrowed object is an ordinary local variable
  int local = 5;
  int *_Borrow p1 = &_Mut local; // the borrowed object of p1 is local
  int *_Borrow p2 = &_Mut * p1;  // the borrowed object of p2 is *p1
  int *_Borrow p3 = p1;         // the borrowed object of p3 is *p1

  // The borrowed object is an owned variable
  int *_Owned x1 = safe_malloc<int>(2);
  int *_Borrow p4 = &_Mut * x1; // the borrowed object of p4 is *x1

  // The borrowed object is a raw pointer variable
  int *x2 = malloc(sizeof(int));
  int *_Borrow p5 = &_Mut * x2; // the borrowed object of p5 is *x2

  // The borrowed object is a field of a struct
  struct S s = {.a = 5};
  int *_Borrow p6 = &_Mut s.a; // the borrowed object of p6 is s.a

  // The borrowed object is the return value of a function; it is the same as the "borrowed objects" of the called function's borrow-type parameters
  int local1 = 10, local2 = 20;
  // The called function bar has two borrow-type parameters, so the borrowed objects of p7 are local1 and local2
  int *_Borrow p7 = bar(&_Mut local1, &_Mut local2);

  // The borrowed object is a global variable
  const int *_Borrow p8 = &_Const g; // the borrowed object of p8 is g

  // The borrowed objects are function parameters
  int *_Borrow p9 = &_Mut a;    // the borrowed object of p9 is a
  int *_Borrow p10 = &_Mut * b; // the borrowed object of p10 is *b
  int *_Borrow p11 = &_Mut * c; // the borrowed object of p11 is *c
  int *_Borrow p12 = &_Mut d.a; // the borrowed object of p12 is d.a

  safe_free((void *_Owned)b);
  safe_free((void *_Owned)x1);
}

int main() {
  int a = 42;
  int *_Owned b = safe_malloc(73);
  int *c = &a;
  struct S d = {.a = 31};
  foo(a, b, c, d);
  return 0;
}
```

Note: if a borrowed object comes from taking an address, or from casting to a raw pointer and then taking a borrow, it is not recorded as a borrowed object. For example:

```C
void f1() {
  int a = 1;
  int *p = &a;
  int *_Borrow p1 = (int *_Borrow)(int *)(&_Mut *p); // the borrowed object *p is not recorded
  int *_Borrow p2 = &_Mut *&a; // the borrowed object a is not recorded
}
```

### Non-Lexical Lifetime of Borrow Variables

A variable's lifetime starts at its declaration and ends at the end of the entire current statement block. This design is called Lexical Lifetime, because the variable's lifetime is strictly bound to the lexical scope. This strategy is very simple to implement, but it may be overly conservative: in some cases the scope of a borrow variable is stretched too long, so that some code that is actually safe is also rejected, which to some extent restricts the code that a programmer can write. Therefore, BiSheng C introduces Non-Lexical Lifetime (abbreviated NLL) for borrow variables, using a more refined approach to compute the range over which a borrow variable is actually in effect. **The NLL range of a borrow variable is: from the point of borrowing, lasting until the place of the last use.** Specifically, it runs **from the definition or reassignment of the borrow variable, to the end of the last use before it is reassigned**.

The following scenarios count as uses of borrow variable p:

1. A function call, such as `use(p)` or `use(&_Mut *p)`
2. A function return, `return p` or `return &_Mut *p`
3. A dereference `*p`
4. A member access `p->field`

For example:

```C
void use(int *_Borrow p) {}
void other_op() {}

// In this example, the NLL of p is segmented, and each NLL segment has a borrowed object
void foo() {
  int local1 = 1, local2 = 2;  //#1
  int *_Borrow p = &_Mut local1; //#2, the first NLL segment of p begins, with borrowed object local1
  other_op();                  //#3
  use(p);                      //#4, the first NLL segment of p ends
  other_op();                  //#5
  p = &_Mut local2;             //#6, the second NLL segment of p begins, with borrowed object local2; since p is not used again afterward, the NLL of p ends here
  other_op();     //#7
}
// The NLL of p is: [2,4]->local1, [6,6]->local2

int main() {
  foo();
  return 0;
}
```

### Lexical Lifetime of Borrowed Objects

Unlike borrow variables, the lifetime of a borrowed object is a Lexical Lifetime. For the lifetimes of the various kinds of borrowed objects, we give the following specific definitions:

| Kind of Borrowed Object | | Lifetime Definition |
| ---- | ---- | ---- |
| Global variable | | The lifetime of a global variable is the entire program; it exists from the start of the program until it exits |
| Local variable | owned variable | From the definition of the variable to when it is moved away (if an `_Owned struct`-type variable is not moved, its lifetime ends when the current block ends) |
| | non-owned non-borrow variable | From the definition of the variable to the end of the current block |
| Local literal | `"string literal"` | From the point of use to the end of the current block |
| | `(struct S) { ... }` | From the point of use to the end of the current block |
| `e->field` | | The lifetime of `*e` |
| `e.field` | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is an array) | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is a pointer) | | The lifetime of `*e` |

### Lifetime Constraints on Borrows

In section 2.1 we mentioned that for borrows we have the following lifetime constraint: **the lifetime of a borrow variable must not be longer than the lifetime of the borrowed object.**
For example:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

void use(int *_Borrow p) {}
int *_Borrow call(int *_Borrow p, int *_Borrow q) { return p; }

// In this example, the lifetime of p is [2,4], and the lifetime of the borrowed object local is [1,4], satisfying the lifetime constraint
void test1() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  use(p);                     //#3
} //#4

// In this example, the lifetime of p has two segments:
// ok: the first segment is [2,2], and the lifetime of the borrowed object local1 is [1,8], satisfying the lifetime constraint
// error: the second segment is [5,7], and the lifetime of the borrowed object local2 is [4,6], not satisfying the lifetime constraint
void test2() {
  int local1 = 5;              //#1
  int *_Borrow p = &_Mut local1; //#2
  {                            //#3
    int local2 = 5;            //#4
    p = &_Mut local2;           //#5
  }                            //#6
  use(p);                      //#7
} //#8

// In this example, the lifetime of p has two segments:
// ok: the first segment is [2,2], and the lifetime of the borrowed object local1 is [1, 8], satisfying the lifetime constraint
// error: the second segment is [5,7], and there are two borrowed objects, local1 and local2; the lifetime of local2 is [4, 6], not satisfying the lifetime constraint
void test3() {
  int local1 = 5;                       //#1
  int *_Borrow p = &_Mut local1;          //#2
  {                                     //#3
    int local2 = 5;                     //#4
    p = call(&_Mut local1, &_Mut local2); //#5
  }                                     //#6
  use(p);                               //#7
} //#8

// In this example, the if branch reassigns p at #6
// error: at use(p), the lifetime of p's borrowed object local2 has already ended, not satisfying the lifetime constraint
// ok: the else branch satisfies the lifetime constraint
void test4() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  int local1 = 5;             //#3
  if (rand()) {               //#4
    int local2 = 5;           //#5
    p = &_Mut local2;          //#6
  } else {                    //#7
    p = &_Mut local1;          //#8
  }                           //#9
  use(p);                     //#10
}

// In this example, the lifetime of p is [2,4], and the lifetime of the borrowed object *x is [1,3], not satisfying the lifetime constraint, error
void test5() {
  int *_Owned x = safe_malloc<int>(5); //#1
  int *_Borrow p = &_Mut * x;           //#2
  safe_free((void *_Owned)x);          //#3
  use(p);                             //#4
} //#5

int main() {
  test1();
  test2();
  test3();
  test4();
  test5();
  return 0;
}
```

## Mutable Borrows and Immutable Borrows

BiSheng C grades the permissions of borrow pointers into mutable (mut) borrows and immutable (immut) borrows. We can read and write the contents of the borrowed object through a mutable borrow pointer; through an immutable borrow pointer, we can only read the contents of the borrowed object but cannot modify it. For example:

```C
// A mutable borrow pointer has the type T *_Borrow
void use_mut(int *_Borrow p) {
  // Through a mutable borrow pointer, the value of the borrowed object can be modified
  *p = 5;
  // Through a mutable borrow pointer, the value of the borrowed object can be read
  int a = *p;
}

// An immutable borrow pointer has the type const T *_Borrow
void use_immut(const int *_Borrow p) {
  *p = 5; // error: the value of the borrowed object cannot be modified through an immutable borrow pointer
  // Through an immutable borrow pointer, the value of the borrowed object can be read
  int a = *p;
}

int main() {
  int i = 1;
  int *_Borrow pmi = &_Mut i;
  use_mut(pmi);
  const int *_Borrow pimi = &_Const i;
  use_immut(pimi);
  return 0;
}
```

### `&_Mut e` Requires e to Be Modifiable

We mentioned in section 1.1 that `&_Mut e` and `&_Const e` require expression e to be an lvalue, i.e. e can be address-taken. For the mutable borrow expression `&_Mut e`, we additionally require e to be mutable. Specifically:

| lvalue Expression | Modifiable? |
| ---- | ---- |
| ident | The variable ident is not qualified by const, and ident cannot be a function name |
| "string literal" | Not allowed, because a string constant is stored in the constant area and cannot be written. Attempting to take a mutable borrow of a string literal (such as `&_Mut "hello"` or `&_Mut * "hello"`) causes a compile error |
| (struct S) { ... } | Allowed |
| `e->field` | Requires `e` to be a mutable borrow pointer, or an _Owned pointer to a modifiable type, or a raw pointer to a modifiable type, and that field is not qualified by const; in the case of multi-level fields, every level of field must not be qualified by const |
| `e.field` | Requires `e` to be mutable, and that field is not qualified by const; in the case of multi-level fields, every level of field must not be qualified by const |
| `e[index]` or `*e` (`e` is an array) | Requires `e` to be mutable |
| `e[index]` or `*e` (`e` is a pointer) | Requires `e` to be a mutable borrow pointer, or an _Owned pointer to a modifiable type, or a raw pointer to a modifiable type |

### At Most One Mutable Borrow at a Time

If two or more pointers access the same data at the same time and at least one of them is used to write data, it may cause undefined behavior. For example:

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation
#include <stdio.h>

void free_a(int *a) { free(a); }
void read_a(int *a) { printf("%d\n", *a); }

void test() {
  int *a = malloc(sizeof(int));
  *a = 42;
  int *p1 = a;
  int *p2 = a;
  // This function frees the memory pointed to by a
  free_a(p1);
  // This function reads the memory pointed to by a
  read_a(p2); // prints a garbage value
}

int main() {
  test();
  return 0;
}
```

Since a borrow is essentially also a pointer, to avoid the above problem, BiSheng C stipulates: **at any given moment, for the same object, there can be either only one mutable borrow, or any number of immutable borrows.**

```C
void write(int *_Borrow p) {}
void read(const int *_Borrow p) {}

void test1() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  int *_Borrow p2 = &_Mut local; // error: at most one mutable borrow variable pointing to local can exist at a time
  write(p1);
  write(p2);
}

void test2() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  const int *_Borrow p2 = &_Const local; // error: a mutable borrow and an immutable borrow pointing to local cannot exist at the same time
  write(p1);
  read(p2);
}

void test3() {
  int local = 1;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local; // error: a mutable borrow and an immutable borrow pointing to local cannot exist at the same time
  read(p1);
  write(p2);
}

int main() {
  test1();
  test2();
  test3();
  return 0;
}
```

If a mutable borrow and an immutable borrow of the same variable exist at the same time, it is possible to modify the memory state of the borrowed object through the mutable borrow and then access the modified memory through the immutable borrow, leading to undefined behavior.
For example:

```C
#include <stdio.h>

struct A {
  int *p;
};

const int *_Borrow struct A::get_p(This *_Borrow this) {
  return &_Const * (this->p);
}

void struct A::free_p(This *_Borrow this) { free(this->p); }

int main() {
  struct A a = {.p = malloc(sizeof(int))};
  // q borrows a.p
  const int *_Borrow q = a.get_p();
  // The memory pointed to by a.p is freed
  a.free_p(); // error: a is mutably borrowed more than once
  // The operation *q may cause undefined behavior
  printf("%d\n", *q);
  return 0;
}
```

In the code above, `a.free_p()` actually uses a mutable borrow of a; this mutable borrow invalidates the borrow q defined before it. Since `printf("%d\n", *q)` uses the invalidated q, the BiSheng C compiler reports an error, thereby preventing the unsafe behavior.

Since immutable borrows do not cause the borrowed object to be modified, any number of immutable borrows can exist at the same time. For example:

```C
void read(const int *_Borrow p) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  const int *_Borrow p2 = &_Const local;
  read(p1);
  read(p2);
}

int main() {
  test();
  return 0;
}
```

## The Effect of Borrowing on the Borrowed Object

### The Effect of an Immutable Borrow on the Borrowed Object

When an immutable borrow is taken of expression e, i.e. `&_Const e`, then before the lifetime of this immutable borrow ends, e can only be read, not modified, and no mutable borrow of e can be created, but immutable borrows of e can still be taken.

| Immutable Borrow Expression | State of the Borrowed Object |
| ---- | ---- |
| `&_Const ident` | The variable `ident` can only be read, not modified; no mutable borrow of the variable `ident` can be created; immutable borrows of the variable `ident` are allowed |
| `&_Const "string literal"` | The temporary variable is always "read-only" |
| `&_Const (struct S) { ... }` | The temporary variable is always "read-only" |
| `&_Const e->field` | `e->field` enters the "read-only" state, and modifying `*e` as a whole is also not allowed. However, modifying the other members that `e` points to, or taking mutable borrows of those other members, is allowed |
| `&_Const e.field` | `e.field` enters the "read-only" state, and modifying `e` as a whole is also not allowed. However, modifying the other members of `e`, or taking mutable borrows of those other members, is allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is an array) | `e` enters the "read-only" state; modifying `e` and its direct or indirect members, or taking mutable borrows of those other members, is not allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is a pointer) | `*e` enters the "read-only" state; directly modifying `*e` and its direct or indirect members, or taking mutable borrows of `*e` and its direct or indirect members, is not allowed. If `e` is an _Owned pointer type, then `e` also enters the read-only state. If `e` is a _Borrow pointer type (i.e. this is an immutable reborrow of `e`), then modifying what `e` points to is allowed, and after the pointee is modified, the read-write attribute of `e` reverts to what it was before the reborrow |

### The Effect of a Mutable Borrow on the Borrowed Object

When a mutable borrow is taken of expression e, i.e. `&_Mut e`, expression e enters the "frozen" state. Before the lifetime of this mutable borrow ends, e cannot be read, cannot be modified (including being moved), and cannot be borrowed.

| Mutable Borrow Expression | State of the Borrowed Object |
| ---- | ---- |
| `&_Mut ident` | The variable `ident` is frozen |
| `&_Mut "string literal"` | Compile error (string literals are immutable and cannot be mutably borrowed) |
| `&_Mut (struct S) { ... }` | The temporary variable is frozen |
| `&_Mut e->field` | `e->field` is frozen; reading or writing `e->field` is not allowed, and modifying `*e` as a whole is not allowed, but modifying the other members that `e` points to, or taking mutable borrows of those other members, is allowed |
| `&_Mut e.field` | `e.field` is frozen; reading or writing `e.field` is not allowed, and modifying `e` as a whole is not allowed, but modifying the other members of `e`, or taking mutable borrows of those other members, is allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is an array) | `e` is frozen; reading or writing `e` and its members is not allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is a pointer) | `*e` is frozen; reading, writing, or borrowing `*e` and its members is not allowed. If `e` is an _Owned pointer type, then reading or writing `e` is also not allowed; if `e` is a _Borrow pointer type (i.e. this is a mutable reborrow of `e`), then modifying what `e` points to is allowed, and after the pointee is modified, `*e` and its direct or indirect members can be read, written, and borrowed |

## Borrow Types in Function Definitions

1. It is not allowed for a function to have no borrow-type parameters while having a borrow-type return.

2. If a function has one borrow-type parameter and the function returns a borrow type, then we directly regard this returned borrow as coming from that borrow-type parameter; the "borrowed object" of the returned borrow is the same as the "borrowed object" of that borrow-type parameter, and this returned borrow must also satisfy the borrow rules mentioned earlier.

3. If a function has multiple borrow-type parameters and the function returns a borrow type, then we directly regard this returned borrow as simultaneously containing the "borrowed variables" passed in from the multiple borrow-type parameters, and this returned borrow must also satisfy the borrow rules mentioned earlier.

```C
int *_Borrow f1(int *_Borrow p) { return p; }
int *_Borrow f2(int *_Borrow p1, int *_Borrow p2) { return p1; }

void test() {
  int local = 5;
  int *_Borrow p1 = f1(&_Mut local);
  /* The parameter of function f1 creates a mutable borrow of local, and this borrow is
     passed to the return value p1, making p1 effectively a mutable borrow of local. So
     the borrowed object of the return value p1 is local, and local stays frozen until the
     lifetime of p1 ends. */

  int local1, local2;
  int *_Borrow p2 = f2(&_Mut local1, &_Mut local2);
  /* The parameters of function f2 create mutable borrows of local1 and local2, and these
     two borrows are passed to the return value p2, making p2 effectively a mutable borrow
     of local1 and local2. So the borrowed objects of the return value p2 are local1 and
     local2, and local1 and local2 stay frozen until the lifetime of p2 ends. */
}

int main() {
  test();
  return 0;
}
```

## Borrow Types in struct Definitions

1. If a struct contains multiple borrow members, then this struct simultaneously has multiple "borrowed objects", and these borrow members must also satisfy the borrow rules mentioned earlier.

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

struct R {
  int *_Borrow m1;
  int *_Borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &_Mut local1, .m2 = &_Mut local2};
  // local1 and local2 stay frozen until the lifetime of r ends.
  // Because the variable r, upon initialization, created a mutable borrow of local1 and local2,
  // r simultaneously contains a mutable borrow of local1 and a mutable borrow of local2.
}

int main() {
  test();
  return 0;
}
```

## Dereferencing Borrow Variables

Dereferencing a borrow pointer variable is allowed, consistent with the dereference operation in standard C: the syntax for dereferencing a borrow pointer variable `p` is `*p`.
Dereferencing a borrow variable `e` of type `const T * _Borrow`, i.e. `*e`, yields a result of type `T`.
Dereferencing a borrow variable `e` of type `T * _Borrow`, i.e. `*e`, yields a result of type `T`.
If `p` is a borrow pointing to type `T`, and `o` is an lvalue of type `T`, then for the expression `*p` there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | *p = expr; not allowed | *p = expr; not allowed |
| | o = *p; allowed | o = *p; not allowed |
| p is a mut borrow | *p = expr; allowed | *p = expr; allowed |
| | o = *p; allowed | o = *p; not allowed |

In the table above, Move / Copy semantics respectively refer to: `T` being a type qualified by `_Owned`, and `T` being any other type.

Note: the assignment permissions in the table above apply equally to function argument-passing and return scenarios.

## Member Access via Borrow Variables

A borrow pointer variable is allowed to access member variables or call member functions, consistent with the arrow operator in standard C: the syntax for accessing the member variable `field` of a pointer variable `p` is `p->field`, and the syntax for calling the member method `method()` of a pointer variable `p` is `p->method()`.

### Accessing Member Variables

When accessing a member variable through a borrow, the type of the expression depends on the type of the member variable itself. The type of the `p->field` expression is the same as the type defined for the `field` member.
If the type of `p->field` is `T`, and `o` is an lvalue of type `T`, then for the `p->field` expression there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | p->field = expr; not allowed | p->field = expr; not allowed |
| | o = p->field; allowed | o = p->field; not allowed |
| p is a mut borrow | p->field = expr; allowed | p->field = expr; allowed |
| | o = p->field; allowed | o = p->field; not allowed |

In the table above, Move / Copy semantics respectively refer to: T being a type qualified by _Owned, and T being any other type.

Note: the assignment permissions in the table above apply equally to function argument-passing and return scenarios.

### Calling Member Functions

When calling a member function through a borrow, i.e. the `p->method()` scenario, the rules between arguments and parameters are as follows:

| | void method(const This * _Borrow this) | void method(This * _Borrow this) | |
| --- | ---- | ---- | --- |
| p is an immut borrow | allowed | not allowed; an immut borrow cannot create a mut borrow | |
| p is a mut borrow | allowed; a mut borrow is allowed to create an immut borrow | allowed | |

For example:

```C
void int ::method1(const This *_Borrow this) {}
void int ::method2(This *_Borrow this) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local;
  p1->method1(); // ok: the parameter type and argument type match, both immutable borrows
  p1->method2(); // error: the parameter is a mutable borrow type and the argument is an immutable borrow type; an immutable borrow cannot create a mutable borrow
  p2->method1(); // ok: the parameter is an immutable borrow type and the argument is a mutable borrow type; a mutable borrow is allowed to create an immutable borrow
  p2->method2(); // ok: the parameter type and argument type match, both mutable borrows
}

int main() {
  test();
  return 0;
}
```

## Type Conversions of Borrows

1. For any type T, if T implements `_Trait TR`, then a borrow pointing to type T is allowed to be upcast to a borrow pointing to type TR; conversely, converting from a borrow of type TR to a borrow of type T is not allowed.

```C
#include <stdio.h>

_Trait TR { void print(This * _Borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

_Impl _Trait TR for int;

void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  _Trait TR *_Borrow p = r; // ok: a borrow of type int* is supported to be upcast to a borrow of type _Trait TR*
  p->print();
  int *_Borrow px = (int *_Borrow)p; // error: downcasting from _Trait TR* is forbidden
}

int main() {
  test();
  return 0;
}
```

2. When the pointed-to types differ, a borrow pointing to T is allowed to be implicitly converted to a borrow pointing to void type; conversely, converting from a borrow of type void to a borrow of type T must be done explicitly. Type conversions between other borrow pointers with different pointed-to types are all disallowed.

```C
void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  void *_Borrow p = r;
  int *_Borrow t1 = p; // error: implicit conversion from void *_Borrow type is not allowed
  int *_Borrow t2 = (int *_Borrow)p; // ok: an explicit cast from void *_Borrow type is allowed
}

int main() {
  test();
  return 0;
}
```

3. Conversion between `T * _Borrow` and `T *` is only allowed outside the safe zone.

```C
int main() {
  int *_Borrow p = (int *_Borrow)NULL; // ok: conversion between T * _Borrow and T * is allowed outside the safe zone
  int *q = p; // error: type conversion must be explicit; implicit type conversion is forbidden
  _Safe { int *_Borrow p = (int *_Borrow)NULL; } // error: conversion between T * _Borrow and T * is forbidden inside the safe zone
  return 0;
}
```

4. C-style casts between `T *_Owned` and `T *_Borrow` pointers are not allowed.

```C
int *_Owned test(int *_Owned p) {
  int *_Borrow q = (int *_Borrow) p; // error: &_Mut *p should be used instead of a cast
  int *_Owned r = (int *_Owned) q; // error: a T *_Owned copy cannot be created from a T *_Borrow via a cast
  return r;
}
```

5. A mutable borrow `T *_Borrow` type can be implicitly converted to an immutable borrow `const T *_Borrow` type, with the compiler automatically inserting the `&_Const *` operator. Casts between a mutable borrow and a read-only borrow are not allowed.

   The implicit conversion from a mutable borrow to an immutable borrow can occur in the following scenarios:
   1. Variable initialization and assignment
   2. Argument passing in a function call expression
   3. A function's return statement

```C
void foo(const int *_Borrow);

void test1() {
  int a = 1;
  int *_Borrow p = &_Mut a;
  const int *_Borrow q = p; // ok
  q = p; // ok
  foo(p); // ok
}

const int *_Borrow test2(int *_Borrow p) {
  return p; // ok
}
```

```C
#include <stdio.h>

int main() {
  int local = 10;
  int *_Borrow p = &_Mut local;
  const int *_Borrow b = (const int *_Borrow)p; // error: casting a mutable borrow to a read-only borrow is not allowed
  printf("%d\n", *b);  // read b
  *p = 1;              // modify p (if the conversion were allowed, this would violate the borrow rules)
  printf("%d\n", *b);  // read b again

  const int *_Borrow c = &_Const local;
  int *_Borrow m = (int *_Borrow)c; // error: casting a read-only borrow to a mutable borrow is not allowed
  *m = 20;  // if the conversion were allowed, this would violate const safety

  return 0;
}
```

For `_ArrayElem` borrows, it is also allowed to implicitly convert `T *_Borrow _ArrayElem` to `T *_Borrow`, which is equivalent to "re-taking a borrow at the current location, obtaining an ordinary borrow pointer"; the reverse cast `T *_Borrow -> T *_Borrow _ArrayElem` is not allowed.

```C
void test(int arr[4], int local) {
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  int *_Borrow q = p; // ok, equivalent to &_Mut *p

  int *_Borrow plain = &_Mut local;
  int *_Borrow _ArrayElem bad = (int *_Borrow _ArrayElem)plain; // error: the reverse conversion is not allowed
}
```

6. Implicitly converting a `_Borrow` pointer to `_Bool` is allowed in variable initialization, variable assignment, function argument passing, and return.

```c
void foo(int *_Borrow _Nullable p) {
  _Bool flag = p; // equivalent: _Bool flag = p != nullptr;
}
void bar(int *_Borrow _Nullable p, _Bool flag) {
  flag = p; // equivalent: flag = p != nullptr;
}
void use(_Bool);
void baz(int *_Borrow _Nullable p) {
  use(p); // equivalent: use(p != nullptr);
}
_Bool foobar (int *_Borrow _Nullable p) {
  return p; // equivalent: return p != nullptr;
}
```

## Other Rules of Borrowing

In addition to the rules above, we have the following rules for borrowing:

1. For global variables, we cannot track in the function signature which function reads a global variable and which function modifies it. To ensure safety, BiSheng C stipulates: inside the safe zone, only read-only borrows of global variables are allowed; mutable borrows are not allowed. If a borrow is taken of a function name, then from the lifetime perspective it can be treated as a borrow of a global variable.

2. A borrow variable must be initialized before use.

```C
void test() {
  int *_Borrow p; 
  use(p); // error: must be initialized
}

int main() {
  test();
  return 0;
}
```

3. When using an expression of borrow type to initialize or reassign another lvalue of borrow type, i.e. `p = e`, `p` and `e` must be of the same borrow type, and `e`'s lifetime is required to be greater than `p`'s lifetime.

```C
#include <stdio.h>

void test() {
  int x = 1;
  int *_Borrow p = &_Mut x;
  {
    int y = 2;
    int *_Borrow pp = &_Mut y;
    p = pp; // error: pp has a shorter lifetime than p
    printf("%d\n", *p);
  }
  printf("%d\n", *p);
}

int main() {
  test();
  return 0;
}
```

Based on this rule, a `_Borrow` pointer member inside a `struct` cannot borrow this `struct` or its other members.

```C
struct S {
  int m;
  const int *_Borrow p;
};

void test() {
  struct S s = {.m = 0, .p = &_Const s.m}; // error: because the lifetime of s.p is the same as the lifetime of s.m
}

int main() {
  test();
  return 0;
}
```

4. A borrow variable cannot be a global variable; it can only be a local variable.

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

int g = 5
int *_Borrow p = &_Mut g; // error: a borrow variable cannot be a global variable
void test() { int *_Borrow p = &_Mut g; }

int main() {
  test();
  return 0;
}
```

5. Taking a borrow of an expression that contains a borrow is not allowed. Likewise, in a borrow type `T* _Borrow`, `T` itself and its members cannot be borrow types.

```C
#include "bishengc_safety.hbs" // Header provided by the BiSheng C language for safe memory allocation and deallocation

struct R {
  int *_Borrow p;
};

void test() {
  int local = 5;
  int *_Borrow *_Borrow p = &_Mut(&_Mut local); // error: multi-level borrow pointers are not allowed

  struct R r1 = {.p = &_Mut local};
  struct R *_Borrow r2 = &_Mut r1; // error: r1 already contains a borrow
}

int main() {
  test();
  return 0;
}
```

6. Implementing a _Trait for a borrow type is not allowed.

```C
_Trait TR{};

_Impl _Trait TR for int *_Borrow; // error: implementing a _Trait for a borrow type is not allowed

int main() { return 0; }
```

7. Adding member functions for a borrow type is not allowed.

```C
void int *_Borrow::f() {} // error: adding member functions for a borrow type is not allowed

int main() { return 0; }
```

8. A member of a union cannot be a borrow type.

```C
union U {
  int *_Borrow p; // error: a borrow pointer is not allowed as a union member
};

int main() { return 0; }
```

9. A borrow pointer type cannot be a generic argument.

10. Ordinary borrow pointer variables do not support subscript operations; `_Borrow _ArrayElem` pointers support subscript operations.

11. Ordinary borrow pointer variables do not support arithmetic operations; `_Borrow _ArrayElem` pointers support the `+`, `-`, `+=`, `-=`, `++`, `--` operations.

12. The comparison operators `==`, `!=`, `>`, `<`, `<=`, `>=`, etc. are allowed between borrow variables of the same type.

13. The `sizeof` and `alignof` operators are allowed on borrow types, with:
    `sizeof(T* _Borrow) == sizeof(T*)`
    `_Alignof(T* _Borrow) == _Alignof(T*)`
    `sizeof(T* _Borrow _ArrayElem) == sizeof(T*)`
    `_Alignof(T* _Borrow _ArrayElem) == _Alignof(T*)`

14. The unary `&`, `!` and binary `&&`, `||` operators are allowed on borrow types.

15. The unary `-`, `~`, `&_Const`, `&_Mut`, `[]`, `++`, `--` operators are not allowed on ordinary borrow types, and the binary `*`, `/`, `%`, `&`, `|`, `<<`, `>>`, `+`, `-` operators are also not allowed on ordinary borrow types. For `_Borrow _ArrayElem` borrows, the `[]`, `+`, `-`, `+=`, `-=`, `++`, `--` among these are allowed; the other restrictions remain unchanged.

```C
_Safe int foo(void) {
  int arr[4] = {1, 2, 3, 4};
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  p = p + 1; // ok: _Borrow _ArrayElem supports +
  p += 1; // ok: _Borrow _ArrayElem supports +=
  ++p; // ok: _Borrow _ArrayElem supports ++
  int x = p[0]; // ok: _Borrow _ArrayElem supports []

  int *_Borrow q = p; // ok: downgraded to an ordinary borrow
  // q = q + 1; // error: ordinary borrows do not support +
  // q += 1; // error: ordinary borrows do not support +=
  // ++q; // error: ordinary borrows do not support ++
  // int y = q[0]; // error: ordinary borrows do not support []
  return x;
}
```

16. If a borrow pointer variable points to a function, then the function can be called through this borrow pointer variable.

```C
#include <stdio.h>

void f() { printf("f()\n"); }

void test() {
  void (*_Borrow const p)() = &_Const f; // ok: taking an immutable borrow of a function
  p();
}

int main() {
  test();
  return 0;
}
```

17. Taking a mutable borrow of a function is not allowed; only a read-only borrow is allowed.

18. A _Borrow pointer is allowed as the condition of `if`, `while`, `do-while`, `for` statements and the ternary expression; it is not allowed as the condition of a `switch` statement.

```c
void foo(int *_Borrow _Nullable p) {
  if (p) { // equivalent: p != nullptr
  }
  while (p) { // equivalent: p != nullptr
  }
  do {
  } while (p); // equivalent: p != nullptr

  for (;p;) { // equivalent: p != nullptr
  }
  switch (p) { // error
  default: 
    break;
  }
  int x = p ? 2 : 1; // equivalent: p != nullptr ? 2 : 1;
}
```
