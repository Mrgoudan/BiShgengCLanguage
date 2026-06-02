# Borrowing

Borrowing, as an important part of BiSheng C's memory safety features, is a complement to ownership. The previous section described the ownership feature; the entity that owns a resource has the responsibility to free that resource. This section introduces borrowing of resources.

## Feature Overview

If we only had ownership types, since operations such as function calls and assignments transfer ownership, the expressiveness of code would be very limited. When programming, we often need to express the concept of "borrowing a resource," as distinct from "owning a resource." Just as in real life, if a person owns something, you can borrow it from them, and once you are done using it, you must return it to its owner.

### Definition of Borrowing and the Borrow Operators

In BiSheng C, **a borrow is a pointer type that points to the memory address where the borrowed object is stored**. To express the concept of borrowing:

1. A new keyword `_Borrow` is introduced. `_Borrow` is used to qualify a pointer type `T*`, indicating a borrow type of T. `_ArrayElem` can also qualify a pointer type `T*` together with `_Borrow`, indicating a borrow type of an element of a T array.
2. The borrow operators `&_Mut` and `&_Const` are introduced. Among them, `&_Mut e` means obtaining a **mutable borrow** of the expression e, and `&_Const e` means obtaining a **read-only borrow** of the expression `e`. Here the expression `e` is required to be an lvalue. Similar to the address-of operator `&` in standard C, the borrow operators in fact obtain the address of the expression `e`.

For example, we can create a mutable borrow `p1` and an immutable borrow `p2` pointing to the local variable `local`, and use them:

```C
void use_immut(const int *_Borrow p) {}
void use_mut(int *_Borrow p) {}

void foo() {
  int local = 5;
  // p1 is a mutable borrow pointer of local
  int *_Borrow p1 = &_Mut local;
  use_mut(p1);
  // p2 is an immutable borrow pointer of local
  const int *_Borrow p2 = &_Const local;
  use_immut(p2);
}

int main() {
  foo();
  return 0;
}
```

In addition, if the expression `e` is a pointer dereference expression, `&_Mut *p` and `&_Const *p` can be respectively regarded as taking a mutable borrow and an immutable borrow of the value stored at address `p`, that is, `*p`. **This operation does not produce a temporary variable for `*p`**. Here, `p` can be a raw pointer, an `_Owned` pointer, or another borrow pointer. For example:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

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

If the expression `e` is an array subscript expression, then the result type of `&_Mut e` and `&_Const e` will carry the two qualifiers `_Borrow _ArrayElem` to denote a borrow pointer "pointing to an array element." Apart from allowing the use of the subscript operator `[]`, arithmetic operations, and having additional type conversion rules, such a borrow pointer follows the same rules as an ordinary `_Borrow` pointer. Unless explicitly stated or separately listed for the `_Borrow _ArrayElem` case, the rules for `_Borrow` pointers also apply to `_Borrow _ArrayElem`.

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

### The Role of Borrowing

Suppose we have the following requirement: create a file, and call some operation functions to read from and write to the file. Without the concept of borrowing, calling a file operation function would cause the ownership of the file pointer to be transferred. To make the file pointer still usable after the function call, we need to return ownership back to the caller:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation
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
  // Transfer ownership back to the caller via the return value, to avoid ownership transfer
  return p;
}

MyFile *_Owned other_operation(MyFile *_Owned p) {
  // some operation
  // Transfer ownership back to the caller via the return value, to avoid ownership transfer
  return p;
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // ownership of p is first moved into insert_str, then transferred back to the caller via the return value
  p = insert_str(p, str);
  p = other_operation(p);
  file_safe_free(p);
  return 0;
}
```

This style causes frequent transfers of ownership of the file pointer, which is error-prone when the code logic is relatively complex. Moreover, if ownership is transferred away but not returned, the file pointer can no longer be used afterward. With borrowing, we pass a borrow of the file pointer as a parameter to the operation function, and after the function returns, the file pointer can still be used for subsequent operations. There is no longer a need, as in the example above, to first pass ownership in via the function parameter and then pass it back out via the function return; the code is much more concise:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation
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
  // no need to return ownership
}

void other_operation(MyFile *_Borrow p) {
  // some operation
  // no need to return ownership
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // ownership is not moved
  insert_str(&_Mut * p, str);
  // ownership is not moved
  other_operation(&_Mut * p);
  file_safe_free(p);
  return 0;
}
```

## Lifetimes of Borrow Variables and Borrowed Objects

### Lifetimes and Their Role

We can take borrows of various kinds of objects: `_Owned` variables, local variables of non-`_Owned` types, global variables, temporary anonymous variables, parameters, etc., and even part of a compound variable. To correctly represent the valid scopes of borrow variables and various kinds of borrowed objects, we introduce the concept of lifetimes.

The main role of lifetime checking is to avoid dangling pointers, which can cause a program to use data that should not be used. The following C code is a typical example that uses a dangling pointer:

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

There are two points worth noting in this C code:

1. The declaration `int *p` carries the risk of using `NULL`;
2. `p` points to the `local` variable inside the inner block, but `local` is freed when the block ends. Therefore, after returning to the outer block, `p` points to an invalid address and is a dangling pointer that points to the prematurely freed variable `local`. As one can expect, `*p = 1` will cause undefined behavior at runtime in this program. When the code logic is relatively complex, this kind of abnormal behavior is very hard to discover.

For the second point, BiSheng C stipulates: **no borrow of a resource may live longer than the lifetime of the resource's owner**. In other words, the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object.

Next, we rewrite the above C code using BiSheng C's borrowing feature. By checking the lifetimes of borrow variables and borrowed objects, potential memory safety risks can be identified at compile time:

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

Each borrow variable (that is, a _Borrow pointer variable) has one or more borrowed objects, for example:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

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

  // The borrowed object is the return value of a function, which is the same as the "borrowed objects" of the borrow-type parameters of the called function
  int local1 = 10, local2 = 20;
  // The called function bar has two borrow-type parameters, so the borrowed objects of p7 are local1 and local2
  int *_Borrow p7 = bar(&_Mut local1, &_Mut local2);

  // The borrowed object is a global variable
  const int *_Borrow p8 = &_Const g; // the borrowed object of p8 is g

  // The borrowed object is a function parameter
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

Note: if the borrowed object comes from taking an address or casting to a raw pointer and then obtaining a borrow, it is not recorded as a borrowed object. For example:

```C
void f1() {
  int a = 1;
  int *p = &a;
  int *_Borrow p1 = (int *_Borrow)(int *)(&_Mut *p); // *p is not recorded as a borrowed object
  int *_Borrow p2 = &_Mut *&a; // a is not recorded as a borrowed object
}
```

### Non-Lexical Lifetime of Borrow Variables

A variable's lifetime begins at its declaration and ends at the end of the current whole block. This design is called Lexical Lifetime, because the variable's lifetime is strictly bound to the lexical scope. This strategy is very simple to implement, but it may be too conservative. In some cases the active range of a borrow variable is excessively extended, so that some code that is actually safe is also rejected, which limits to some extent the code that programmers can write. Therefore, BiSheng C introduces the Non-Lexical Lifetime (abbreviated as NLL) for borrow variables, computing the range in which a borrow variable is truly in effect with a finer-grained approach. **The NLL range of a borrow variable is: from the point of borrowing, lasting until the last point of use**. Specifically, it is **from the definition or reassignment of the borrow variable, to the last use before it is reassigned**.

Among them, the following scenarios count as uses of the borrow variable p:

1. Function call, such as `use(p)` or `use(&_Mut *p)`
2. Function return `return p` or `return &_Mut *p`
3. Dereference `*p`
4. Member access `p->field`

For example:

```C
void use(int *_Borrow p) {}
void other_op() {}

// In this example p's NLL is segmented; each NLL segment has a borrowed object
void foo() {
  int local1 = 1, local2 = 2;  //#1
  int *_Borrow p = &_Mut local1; //#2, p's first NLL segment begins, borrowed object is local1
  other_op();                  //#3
  use(p);                      //#4, p's first NLL segment ends
  other_op();                  //#5
  p = &_Mut local2;             //#6, p's second NLL segment begins, borrowed object is local2; since there is no further use of p afterward, p's NLL ends
  other_op();     //#7
}
// p's NLL is: [2,4]->local1, [6,6]->local2

int main() {
  foo();
  return 0;
}
```

### Lexical Lifetime of Borrowed Objects

Unlike borrow variables, the lifetime of a borrowed object is a Lexical Lifetime. For the lifetimes of different kinds of borrowed objects, we give specific definitions:

| Kind of borrowed object | | Lifetime definition |
| ---- | ---- | ---- |
| Global variable | | The lifetime of a global variable is the entire program; it exists from the start of the program to its exit |
| Local variable | owned variable | From the definition of the variable to the point where it is moved away (if an `_Owned struct` type is not moved, its lifetime ends at the end of the current block) |
| | non-owned non-borrow variable | From the definition of the variable to the end of the current block |
| Local literal | `"string literal"` | From the point of use to the end of the current block |
| | `(struct S) { ... }` | From the point of use to the end of the current block |
| `e->field` | | The lifetime of `*e` |
| `e.field` | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is an array) | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is a pointer) | | The lifetime of `*e` |

### Lifetime Constraints of Borrows

In section 2.1 we mentioned that for borrows we have the following lifetime constraint: **the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object**.
For example:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

void use(int *_Borrow p) {}
int *_Borrow call(int *_Borrow p, int *_Borrow q) { return p; }

// In this example, p's lifetime is [2,4], and the borrowed object local's lifetime is [1,4], which satisfies the lifetime constraint
void test1() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  use(p);                     //#3
} //#4

// In this example, p's lifetime has two segments:
// ok: the first segment is [2,2], and the borrowed object local1's lifetime is [1,8], which satisfies the lifetime constraint
// error: the second segment is [5,7], and the borrowed object local2's lifetime is [4,6], which does not satisfy the lifetime constraint
void test2() {
  int local1 = 5;              //#1
  int *_Borrow p = &_Mut local1; //#2
  {                            //#3
    int local2 = 5;            //#4
    p = &_Mut local2;           //#5
  }                            //#6
  use(p);                      //#7
} //#8

// In this example p's lifetime has two segments:
// ok: the first segment is [2,2], and the borrowed object local1's lifetime is [1, 8], which satisfies the lifetime constraint
// error: the second segment is [5,7], and there are two borrowed objects, local1 and local2, where local2's lifetime is [4, 6], which does not satisfy the lifetime constraint
void test3() {
  int local1 = 5;                       //#1
  int *_Borrow p = &_Mut local1;          //#2
  {                                     //#3
    int local2 = 5;                     //#4
    p = call(&_Mut local1, &_Mut local2); //#5
  }                                     //#6
  use(p);                               //#7
} //#8

// In this example, the if branch reassigns p, at #6
// error: at use(p), the lifetime of p's borrowed object local2 has already ended, which does not satisfy the lifetime constraint
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

// In this example, p's lifetime is [2,4], and the borrowed object *x's lifetime is [1,3], which does not satisfy the lifetime constraint, error
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

BiSheng C classifies the permissions of borrow pointers into levels: mutable (mut) borrows and immutable (immut) borrows. We can manipulate a mutable borrow pointer to read from and write to the contents of the borrowed object; through an immutable borrow pointer, we can only read the contents of the borrowed object but cannot modify it. For example:

```C
// The mutable borrow pointer type is T *_Borrow
void use_mut(int *_Borrow p) {
  // Through a mutable borrow pointer, the value of the borrowed object can be modified
  *p = 5;
  // Through a mutable borrow pointer, the value of the borrowed object can be read
  int a = *p;
}

// The immutable borrow pointer type is const T *_Borrow
void use_immut(const int *_Borrow p) {
  *p = 5; // error: cannot modify the value of the borrowed object through an immutable borrow pointer
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

We mentioned in 1.1 that `&_Mut e` and `&_Const e` require the expression e to be an lvalue, that is, e can have its address taken. For a mutable borrow expression `&_Mut e`, we additionally require e to be mutable. Specifically:

| lvalue expression | Whether modifiable |
| ---- | ---- |
| ident | The variable ident is not qualified by const, and ident cannot be a function name |
| "string literal" | Not allowed, because string literals are stored in the constant region and cannot be written. Attempting a mutable borrow of a string literal (such as `&_Mut "hello"` or `&_Mut * "hello"`) causes a compile error |
| (struct S) { ... } | Allowed |
| `e->field` | `e` is required to be a mutable borrow pointer, or an _Owned pointer to a modifiable type, or a raw pointer to a modifiable type, and field is not qualified by const; for multi-level fields, every level of field is required to be not qualified by const |
| `e.field` | `e` is required to be mutable, and field is not qualified by const; for multi-level fields, every level of field is required to be not qualified by const |
| `e[index]` or `*e` (`e` is an array) | `e` is required to be mutable |
| `e[index]` or `*e` (`e` is a pointer) | `e` is required to be a mutable borrow pointer, or an _Owned pointer to a modifiable type, or a raw pointer to a modifiable type |

### Only One Mutable Borrow Can Exist at a Time

If two or more pointers access the same data at the same time, and at least one of the pointers is used to write data, it may cause undefined behavior, for example:

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation
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

Since a borrow is essentially a pointer too, to avoid the above problem, BiSheng C stipulates that **at the same time, for the same object, you can either have only one mutable borrow, or any number of immutable borrows**.

```C
void write(int *_Borrow p) {}
void read(const int *_Borrow p) {}

void test1() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  int *_Borrow p2 = &_Mut local; // error: at the same time there can be at most one mutable borrow variable pointing to local
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

If a mutable borrow and an immutable borrow of a variable exist at the same time, it may happen that the memory state of the borrowed object is modified through the mutable borrow, and then the modified memory is accessed through the immutable borrow, thereby causing undefined behavior.
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
  // the memory pointed to by a.p is freed
  a.free_p(); // error: a is mutably borrowed multiple times
  // the operation *q may cause undefined behavior
  printf("%d\n", *q);
  return 0;
}
```

In the above code, `a.free_p()` actually uses a mutable borrow pointing to a, and this mutable borrow invalidates the borrow q that was defined before it. Since `printf("%d\n", *q)` uses the invalidated q, the BiSheng C compiler reports an error, thereby preventing the unsafe behavior from occurring.

Since immutable borrows do not cause the borrowed object to be modified, any number of immutable borrows can exist at the same time, for example:

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

### The Effect of Immutable Borrowing on the Borrowed Object

When an immutable borrow is taken of expression e, that is `&_Const e`, before the lifetime of this immutable borrow ends, e can only be read and cannot be modified, and you also cannot create a mutable borrow of e, but you can still take an immutable borrow of e.

| Immutable borrow expression | State of the borrowed object |
| ---- | ---- |
| `&_Const ident` | The variable `ident` can only be read and cannot be modified, and a mutable borrow can no longer be created of the variable `ident`; creating an immutable borrow of the variable `ident` is allowed |
| `&_Const "string literal"` | The temporary variable is always in a "read-only" state |
| `&_Const (struct S) { ... }` | The temporary variable is always in a "read-only" state |
| `&_Const e->field` | `e->field` enters a "read-only" state, and modifying `*e` as a whole is also not allowed. But modifying other members pointed to by `e`, or taking a mutable borrow of other members, is allowed |
| `&_Const e.field` | `e.field` enters a "read-only" state, and modifying `e` as a whole is also not allowed. But modifying other members of `e`, or taking a mutable borrow of other members, is allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is an array) | `e` enters a "read-only" state; modifying `e` and its direct or indirect members, or taking a mutable borrow of other members, is not allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is a pointer) | `*e` enters a "read-only" state; directly modifying `*e` and its direct or indirect members, or taking a mutable borrow of `*e` and its direct or indirect members, is not allowed; if `e` is an _Owned pointer type, then `e` also enters a read-only state. If `e` is a _Borrow pointer type (that is, this is an immutable reborrow of `e`), then modifying the target that `e` points to is allowed, and after modifying the target, the read-write attribute of `e` is restored to that before the reborrow |

### The Effect of Mutable Borrowing on the Borrowed Object

When a mutable borrow is taken of expression e, that is `&_Mut e`, the expression e enters a "frozen" state. Before the lifetime of this mutable borrow ends, e cannot be read, cannot be modified (including being moved), and cannot be borrowed.

| Mutable borrow expression | State of the borrowed object |
| ---- | ---- |
| `&_Mut ident` | The variable `ident` is frozen |
| `&_Mut "string literal"` | Compile error (string literals are immutable and cannot be mutably borrowed) |
| `&_Mut (struct S) { ... }` | The temporary variable is frozen |
| `&_Mut e->field` | `e->field` is frozen; reading or writing `e->field` is not allowed, modifying `*e` as a whole is not allowed, but modifying other members pointed to by `e`, or taking a mutable borrow of other members, is allowed |
| `&_Mut e.field` | `e.field` is frozen; reading or writing `e.field` is not allowed, modifying `e` as a whole is not allowed, but modifying other members of `e`, or taking a mutable borrow of other members, is allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is an array) | `e` is frozen; reading or writing `e` and its members is not allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is a pointer) | `*e` is frozen; reading, writing, or borrowing `*e` and its members is not allowed. If `e` is an _Owned pointer type, then reading or writing `e` is also not allowed; if `e` is a _Borrow pointer type (that is, this is a mutable reborrow of `e`), then modifying the target that `e` points to is allowed, and after modifying the target, reading, writing, and borrowing `*e` and its direct or indirect members is allowed |

## Borrow Types in Function Definitions

1. It is not allowed for a function to have no borrow-type parameter among its parameters but a borrow-type return.

2. If a function has one borrow-type parameter among its parameters and the function returns a borrow type, then we directly consider that the borrow of this return value comes from this borrow-type parameter; the "borrowed object" of the returned borrow is the same as the "borrowed object" of this borrow-type parameter, and this returned borrow should also satisfy the borrow rules mentioned earlier.

3. If a function has multiple borrow-type parameters among its parameters and the function returns a borrow type, then we directly consider that the borrow of this return value simultaneously includes the "borrowed variables" passed in from the multiple borrow-type parameters, and this returned borrow should also satisfy the borrow rules mentioned earlier.

```C
int *_Borrow f1(int *_Borrow p) { return p; }
int *_Borrow f2(int *_Borrow p1, int *_Borrow p2) { return p1; }

void test() {
  int local = 5;
  int *_Borrow p1 = f1(&_Mut local);
  /* The parameter of function f1 creates a mutable borrow of local, and this borrow is passed to the return value p1,
     causing p1 to be equivalent to a mutable borrow of local. So the borrowed object of the return value p1 is
     local, and before p1's lifetime ends, local will be frozen the whole time. */

  int local1, local2;
  int *_Borrow p2 = f2(&_Mut local1, &_Mut local2);
  /* The parameters of function f2 create a mutable borrow of local1 and
     local2, and these two borrows are passed to the return value p2, causing p2 to be equivalent to a mutable borrow of local1 and
     local2. So the borrowed objects of the return value p2 are local1 and local2, and before p2's
     lifetime ends, local1 and local2 are frozen the whole time. */
}

int main() {
  test();
  return 0;
}
```

## Borrow Types in struct Definitions

1. If a struct contains multiple borrow members, then this struct simultaneously has multiple "borrowed objects", and these borrow members should also satisfy the borrow rules mentioned earlier.

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

struct R {
  int *_Borrow m1;
  int *_Borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &_Mut local1, .m2 = &_Mut local2};
  // Before r's lifetime ends, local1 and local2 are frozen the whole time.
  // Because the variable r created a mutable borrow of local1 and local2 at initialization,
  // r simultaneously contains a mutable borrow of local1 and also contains a mutable borrow of local2.
}

int main() {
  test();
  return 0;
}
```

## Dereference Operations on Borrow Variables

Dereferencing a borrow pointer variable is allowed, consistent with the dereference operation in standard C: the syntax for dereferencing a borrow pointer variable `p` is `*p`.
Dereferencing `*e` of a borrow variable `e` of type `const T * _Borrow` yields a result of type `T`.
Dereferencing `*e` of a borrow variable `e` of type `T * _Borrow` yields a result of type `T`.
If `p` is a borrow pointing to type `T`, and `o` is an lvalue of type `T`, then for the `*p` expression, there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | *p = expr; not allowed | *p = expr; not allowed |
| | o = *p; allowed | o = *p; not allowed |
| p is a mut borrow | *p = expr; allowed | *p = expr; allowed |
| | o = *p; allowed | o = *p; not allowed |

In the above table, move / copy semantics respectively mean: `T` is a type qualified by `_Owned`, and `T` is another type.

Note: the permissions for the assignment operations in the above table can equally be applied to the scenarios of passing function arguments and returning.

## Member Access of Borrow Variables

A borrow pointer variable is allowed to access member variables or call member functions, consistent with the arrow operator in standard C: the syntax for accessing the member variable `field` of a pointer variable `p` is `p->field`, and the syntax for calling the member method `method()` of a pointer variable `p` is `p->method()`.

### Accessing Member Variables

When accessing a member variable through a borrow, the type of the expression depends on the type of the member variable itself. The type of the `p->field` expression is the same as the type defined for the `field` member.
If the type of `p->field` is `T`, and `o` is an lvalue of type `T`, then for the `p->field` expression, there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | p->field = expr; not allowed | p->field = expr; not allowed |
| | o = p->field; allowed | o = p->field; not allowed |
| p is a mut borrow | p->field = expr; allowed | p->field = expr; allowed |
| | o = p->field; allowed | o = p->field; not allowed |

In the above table, move / copy semantics respectively mean: T is a type qualified by _Owned, and T is another type.

Note: the permissions for the assignment operations in the above table can equally be applied to the scenarios of passing function arguments and returning.

### Calling Member Functions

When calling a member function through a borrow, that is, the `p->method()` scenario, the rules between actual arguments and formal parameters are as follows:

| | void method(const This * _Borrow this) | void method(This * _Borrow this) | |
| --- | ---- | ---- | --- |
| p is an immut borrow | allowed | not allowed, an immut borrow cannot create a mut borrow | |
| p is a mut borrow | allowed, creating an immut borrow from a mut borrow is allowed | allowed | |

For example:

```C
void int ::method1(const This *_Borrow this) {}
void int ::method2(This *_Borrow this) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local;
  p1->method1(); // ok: the formal parameter type and actual argument type are consistent, both immutable borrows
  p1->method2(); // error: the formal parameter is a mutable borrow type, the actual argument is an immutable borrow type, an immutable borrow cannot create a mutable borrow
  p2->method1(); // ok: the formal parameter is an immutable borrow type, the actual argument is a mutable borrow type, creating an immutable borrow from a mutable borrow is allowed
  p2->method2(); // ok: the formal parameter type and actual argument type are consistent, both mutable borrows
}

int main() {
  test();
  return 0;
}
```

## Type Conversions of Borrows

1. For any type T, if T implements `_Trait` TR, then a borrow pointing to type T is allowed to be upcast to a borrow pointing to type TR; conversely, conversion from a borrow of type TR to a borrow of type T is not allowed.

```C
#include <stdio.h>

_Trait TR { void print(This * _Borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

_Impl _Trait TR for int;

void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  _Trait TR *_Borrow p = r; // ok: a borrow of type int* can be upcast to a borrow of type _Trait TR*
  p->print();
  int *_Borrow px = (int *_Borrow)p; // error: downcasting _Trait TR* is prohibited
}

int main() {
  test();
  return 0;
}
```

2. When the pointed-to types are different, a borrow pointing to T is allowed to be implicitly converted to a borrow pointing to type void; conversely, conversion from a borrow of type void to a borrow of type T must be done explicitly. Type conversions between borrow pointers with other different pointed-to types are all not allowed.

```C
void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  void *_Borrow p = r;
  int *_Borrow t1 = p; // error: implicit conversion to int *_Borrow type from void *_Borrow is not allowed
  int *_Borrow t2 = (int *_Borrow)p; // ok: explicit cast from void *_Borrow type is allowed
}

int main() {
  test();
  return 0;
}
```

3. Conversion between `T * _Borrow` and `T *` is only allowed in the unsafe zone.

```C
int main() {
  int *_Borrow p = (int *_Borrow)NULL; // ok: the unsafe zone allows conversion between T * _Borrow and T *
  int *q = p; // error: type conversion must be explicit, implicit type conversion is prohibited
  _Safe { int *_Borrow p = (int *_Borrow)NULL; } // error: the safe zone prohibits conversion between T * _Borrow and T *
  return 0;
}
```

4. C-style casts between `T *_Owned` and `T *_Borrow` pointers are not allowed

```C
int *_Owned test(int *_Owned p) {
  int *_Borrow q = (int *_Borrow) p; // error: &_Mut *p should be used instead of a cast
  int *_Owned r = (int *_Owned) q; // error: a T *_Owned copy cannot be created from a T *_Borrow via a cast
  return r;
}
```

5. A mutable borrow type `T *_Borrow` can be implicitly converted to an immutable borrow type `const T *_Borrow`, with the compiler automatically inserting the `&_Const *` operator. Casts between a mutable borrow and a read-only borrow are not allowed.

   The implicit conversion from a mutable borrow to an immutable borrow can occur in the following scenarios:
   1. Variable initialization and assignment
   2. Argument passing in function call expressions
   3. Function return statements

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

For `_ArrayElem` borrows, it is additionally allowed to implicitly convert `T *_Borrow _ArrayElem` to `T *_Borrow`, which is equivalent to "re-taking the borrow at the current location, obtaining an ordinary borrow pointer"; the reverse `T *_Borrow -> T *_Borrow _ArrayElem` cast is not allowed.

```C
void test(int arr[4], int local) {
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  int *_Borrow q = p; // ok, equivalent to &_Mut *p

  int *_Borrow plain = &_Mut local;
  int *_Borrow _ArrayElem bad = (int *_Borrow _ArrayElem)plain; // error: the reverse conversion is not allowed
}
```

6. Implicit conversion of a `_Borrow` pointer to `_Bool` is allowed in variable initialization, variable assignment, function argument passing, and returning

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

In addition to the above rules, we have the following rules for borrows:

1. For global variables, we cannot track in the function signature which function reads a global variable and which function modifies a global variable. To ensure safety, BiSheng C stipulates: in the safe zone, only read-only borrows of global variables are allowed; mutable borrows are not allowed. If a borrow is taken of a function name, from the lifetime perspective, it can be treated as a borrow of a global variable.

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

3. When initializing or reassigning another borrow-type lvalue with a borrow-type expression, that is `p = e`, `p` and `e` must be borrow types of the same kind, and `e`'s lifetime is required to be greater than p's lifetime.

```C
#include <stdio.h>

void test() {
  int x = 1;
  int *_Borrow p = &_Mut x;
  {
    int y = 2;
    int *_Borrow pp = &_Mut y;
    p = pp; // error: pp's lifetime is shorter than p's
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
  struct S s = {.m = 0, .p = &_Const s.m}; // error: because s.p's lifetime is the same as s.m's lifetime
}

int main() {
  test();
  return 0;
}
```

4. A borrow variable is not allowed to be a global variable; it can only be a local variable.

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

int g = 5
int *_Borrow p = &_Mut g; // error: a borrow variable is not allowed to be a global variable
void test() { int *_Borrow p = &_Mut g; }

int main() {
  test();
  return 0;
}
```

5. Taking a borrow of an expression that contains a borrow is not allowed. Similarly, in the borrow type `T* _Borrow`, `T` itself and its members cannot be borrow types.

```C
#include "bishengc_safety.hbs" // Header file provided by BiSheng C for safe memory allocation and deallocation

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

6. Impl
