# Borrowing

Borrowing, as an important part of BiSheng C's memory safety features, is a complement to ownership. The previous section described the ownership feature; the entity that owns a resource is responsible for releasing that resource. This section introduces borrowing of resources.

## Feature Overview

If we only had ownership types, since operations such as function calls and assignments transfer ownership, the capability of the code would be very limited. When programming, we often need to express the concept of "borrowing a resource", as distinct from "owning a resource". Just as in real life, if a person owns something, you can borrow it from them, and when you are finished using it, you must return it to its original owner.

### Definition of Borrowing and Borrow Operators

In BiSheng C, **a borrow is a pointer type that points to the memory address where the borrowed object is stored**. To express the concept of borrowing:

1. A new keyword `_Borrow` is introduced. Using `_Borrow` to qualify the pointer type `T*` indicates the borrow type of T. `_ArrayElem` may also be used together with `_Borrow` to qualify the pointer type `T*`, indicating a borrow type of an element of a T array.
2. The borrow operators `&_Mut` and `&_Const` are introduced, where `&_Mut e` obtains a **mutable borrow** of the expression e, and `&_Const e` obtains a **read-only borrow** of the expression `e`. Here, the expression `e` is required to be an lvalue. Similar to the address-of operator `&` in standard C, the borrow operator actually obtains the address of the expression `e`.

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

In addition, if the expression `e` is a pointer dereference expression, `&_Mut *p` and `&_Const *p` can be regarded as taking a mutable borrow and an immutable borrow, respectively, of the value stored at address `p`, that is, `*p`. **This operation does not create a temporary variable for `*p`**. Here, `p` may be a raw pointer, an `_Owned` pointer, or another borrow pointer. For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

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

If the expression `e` is an array subscript expression, then the result types of `&_Mut e` and `&_Const e` will carry the two qualifiers `_Borrow _ArrayElem` to indicate a borrow pointer "pointing to an array element". Apart from allowing the use of subscript `[]`, arithmetic operations, and having additional type conversion rules, such borrow pointers follow the same rules as ordinary `_Borrow` pointers. Unless explicitly stated or the `_Borrow _ArrayElem` case is listed separately, the rules for `_Borrow` pointers also apply to `_Borrow _ArrayElem`.

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

Suppose we have a requirement: create a file and call some operation functions to read from and write to the file. Without the concept of borrowing, calling a file operation function would transfer ownership of the file pointer. In order for the file pointer to still be usable after the function call, we would need to return ownership back to the caller:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation
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
  // Transfer ownership to the caller via the return value to avoid ownership transfer
  return p;
}

MyFile *_Owned other_operation(MyFile *_Owned p) {
  // some operation
  // Transfer ownership to the caller via the return value to avoid ownership transfer
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

This approach causes frequent transfers of ownership of the file pointer, which is error-prone when the code logic is more complex. Moreover, if ownership is moved away but not returned, the file pointer can no longer be used afterward. With borrowing, we pass a borrow of the file pointer as an argument to the operation function, and after the function returns the file pointer can still be used for subsequent operations. There is no longer any need, as in the previous example, to pass ownership in via function parameters and then pass it out via function returns, making the code more concise:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation
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

We can take borrows of different kinds of objects: `_Owned` variables, local variables of non-`_Owned` types, global variables, temporary anonymous variables, parameters, and even part of a composite variable. To correctly represent the valid scope of borrow variables and various kinds of borrowed objects, we introduce the concept of lifetimes.

The main purpose of lifetime checking is to avoid dangling pointers, which would cause a program to use data that it should not use. The following C code is a typical example of using a dangling pointer:

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

Two things in this C code are worth noting:

1. The declaration `int *p` carries the risk of using `NULL`;
2. `p` points to the `local` variable in the inner block, but `local` is released when the block ends. Therefore, after returning to the outer block, `p` points to an invalid address and is a dangling pointer pointing to the prematurely released variable `local`. As expected, `*p = 1` causes undefined behavior at runtime. When the code logic is more complex, such anomalous behavior is very hard to discover.

For the second point, BiSheng C stipulates: **no borrow of a resource may outlive the lifetime of the resource's owner**. In other words, the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object.

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
  *p = 3; // error: local2's lifetime is not long enough
  return 0;
}
```

### Borrow Variables and Borrowed Objects

Every borrow variable (that is, a _Borrow pointer variable) has one or more borrowed objects, for example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

struct S {
  int a;
};

int *_Borrow bar(int *_Borrow, int *_Borrow);

int g = 5;
void foo(int a, int *_Owned b, int *c, struct S d) {
  // The borrowed object is an ordinary local variable
  int local = 5;
  int *_Borrow p1 = &_Mut local; // p1's borrowed object is local
  int *_Borrow p2 = &_Mut * p1;  // p2's borrowed object is *p1
  int *_Borrow p3 = p1;         // p3's borrowed object is *p1

  // The borrowed object is an owned variable
  int *_Owned x1 = safe_malloc<int>(2);
  int *_Borrow p4 = &_Mut * x1; // p4's borrowed object is *x1

  // The borrowed object is a raw pointer variable
  int *x2 = malloc(sizeof(int));
  int *_Borrow p5 = &_Mut * x2; // p5's borrowed object is *x2

  // The borrowed object is a field of a struct
  struct S s = {.a = 5};
  int *_Borrow p6 = &_Mut s.a; // p6's borrowed object is s.a

  // The borrowed object is the return value of a function, the same as the "borrowed objects" of the borrow-type parameters of the called function
  int local1 = 10, local2 = 20;
  // The called function bar has two borrow-type parameters, so p7's borrowed objects are local1 and local2
  int *_Borrow p7 = bar(&_Mut local1, &_Mut local2);

  // The borrowed object is a global variable
  const int *_Borrow p8 = &_Const g; // p8's borrowed object is g

  // The borrowed object is a function parameter
  int *_Borrow p9 = &_Mut a;    // p9's borrowed object is a
  int *_Borrow p10 = &_Mut * b; // p10's borrowed object is *b
  int *_Borrow p11 = &_Mut * c; // p11's borrowed object is *c
  int *_Borrow p12 = &_Mut d.a; // p12's borrowed object is d.a

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

Note: If the borrowed object results from taking an address or casting to a raw pointer and then taking a borrow, it is not recorded as a borrowed object. For example:

```C
void f1() {
  int a = 1;
  int *p = &a;
  int *_Borrow p1 = (int *_Borrow)(int *)(&_Mut *p); // The borrowed object *p is not recorded
  int *_Borrow p2 = &_Mut *&a; // The borrowed object a is not recorded
}
```

### Non-Lexical Lifetime of Borrow Variables

A variable's lifetime starts at its declaration and ends when the entire current block ends. This design is called Lexical Lifetime, because the variable's lifetime is strictly bound to its lexical scope. This strategy is very simple to implement, but it may be overly conservative. In some cases the scope of a borrow variable is excessively prolonged, to the point that some code that is actually safe is also prevented, which to some extent limits the code a programmer can write. Therefore, BiSheng C introduces Non-Lexical Lifetime (abbreviated NLL) for borrow variables, using a finer-grained means to compute the range in which a borrow variable is actually effective. **The NLL range of a borrow variable is: from the point of the borrow, continuing until its last use**. Specifically, it is **from where the borrow variable is defined or reassigned, ending at the last use before it is reassigned**.

The following scenarios constitute use of the borrow variable p:

1. Function call, such as `use(p)` or `use(&_Mut *p)`
2. Function return `return p` or `return &_Mut *p`
3. Dereference `*p`
4. Member access `p->field`

For example:

```C
void use(int *_Borrow p) {}
void other_op() {}

// In this example, p's NLL is segmented; each NLL segment has a borrowed object
void foo() {
  int local1 = 1, local2 = 2;  //#1
  int *_Borrow p = &_Mut local1; //#2, p's first NLL segment begins, borrowed object is local1
  other_op();                  //#3
  use(p);                      //#4, p's first NLL segment ends
  other_op();                  //#5
  p = &_Mut local2;             //#6, p's second NLL segment begins, borrowed object is local2; since there is no further use of p, p's NLL ends
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

| Kind of Borrowed Object | | Lifetime Definition |
| ---- | ---- | ---- |
| Global variable | | The lifetime of a global variable is the entire program, from program start to exit, always existing |
| Local variable | owned variable | From the variable definition until it is moved away (if an _Owned struct type is not moved, its lifetime ends when the current block ends) |
| | non-owned non-borrow variable | From the variable definition until the current block ends |
| Local literal | `"string literal"` | From the point of use until the current block ends |
| | `(struct S) { ... }` | From the point of use until the current block ends |
| `e->field` | | The lifetime of `*e` |
| `e.field` | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is an array) | | The lifetime of `e` |
| `e[index]` or `*e` (`e` is a pointer) | | The lifetime of `*e` |

### Lifetime Constraints of Borrowing

In section 2.1 we mentioned that for borrowing, we have the following lifetime constraint: **the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object**.
For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

void use(int *_Borrow p) {}
int *_Borrow call(int *_Borrow p, int *_Borrow q) { return p; }

// In this example, p's lifetime is [2,4], and the borrowed object local's lifetime is [1,4], satisfying the lifetime constraint
void test1() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  use(p);                     //#3
} //#4

// In this example, p's lifetime has two segments
// ok: the first segment is [2,2], and the borrowed object local1's lifetime is [1,8], satisfying the lifetime constraint
// error: the second segment is [5,7], and the borrowed object local2's lifetime is [4,6], not satisfying the lifetime constraint
void test2() {
  int local1 = 5;              //#1
  int *_Borrow p = &_Mut local1; //#2
  {                            //#3
    int local2 = 5;            //#4
    p = &_Mut local2;           //#5
  }                            //#6
  use(p);                      //#7
} //#8

// In this example, p's lifetime has two segments:
// ok: the first segment is [2,2], and the borrowed object local1's lifetime is [1, 8], satisfying the lifetime constraint
// error: the second segment is [5,7], with two borrowed objects, local1 and local2, where local2's lifetime is [4, 6], not satisfying the lifetime constraint
void test3() {
  int local1 = 5;                       //#1
  int *_Borrow p = &_Mut local1;          //#2
  {                                     //#3
    int local2 = 5;                     //#4
    p = call(&_Mut local1, &_Mut local2); //#5
  }                                     //#6
  use(p);                               //#7
} //#8

// In this example, the if branch reassigns p, at #10
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

// In this example, p's lifetime is [2,4], and the borrowed object *x's lifetime is [1,3], not satisfying the lifetime constraint, error
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

BiSheng C grades the permissions of borrow pointers into mutable (mut) borrows and immutable (immut) borrows. We can manipulate a mutable borrow pointer to read from and write to the contents of the borrowed object, while through an immutable borrow pointer we can only read the contents of the borrowed object but cannot modify it. For example:

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

As we mentioned in section 1.1, `&_Mut e` and `&_Const e` require the expression e to be an lvalue, that is, e can have its address taken. For the mutable borrow expression `&_Mut e`, we additionally require e to be mutable. Specifically:

| lvalue Expression | Modifiable |
| ---- | ---- |
| ident | The variable ident is not qualified by const, and ident cannot be a function name |
| "string literal" | Not allowed, because string constants are stored in the constant area and cannot be written. Attempting to take a mutable borrow of a string literal (such as `&_Mut "hello"` or `&_Mut * "hello"`) causes a compilation error |
| (struct S) { ... } | Allowed |
| `e->field` | Requires `e` to be a mutable borrow pointer, or an _Owned pointer pointing to a modifiable type, or a raw pointer pointing to a modifiable type, and field is not qualified by const. In the case of multi-level fields, every level of field must not be qualified by const |
| `e.field` | Requires `e` to be mutable, and field is not qualified by const. In the case of multi-level fields, every level of field must not be qualified by const |
| `e[index]` or `*e` (`e` is an array) | Requires `e` to be mutable |
| `e[index]` or `*e` (`e` is a pointer) | Requires `e` to be a mutable borrow pointer, or an _Owned pointer pointing to a modifiable type, or a raw pointer pointing to a modifiable type |

### Only One Mutable Borrow Can Exist at a Time

If two or more pointers access the same data at the same time, and at least one of the pointers is used to write the data, undefined behavior may result, for example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation
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

Since a borrow is essentially a pointer, in order to avoid the above problem, BiSheng C stipulates that **at the same time, for the same object, you may have either only one mutable borrow or any number of immutable borrows**.

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

If a mutable borrow and an immutable borrow of a variable exist at the same time, it may happen that the memory state of the borrowed object is modified through the mutable borrow, and then the modified memory is accessed through the immutable borrow, leading to undefined behavior.
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

In the above code, `a.free_p()` actually uses a mutable borrow pointing to a, which invalidates the borrow q that was defined before it. Since `printf("%d\n", *q)` uses the invalidated q, the BiSheng C compiler reports an error, thereby preventing the unsafe behavior from occurring.

Since immutable borrows do not cause the borrowed object to be modified, you can have any number of immutable borrows at the same time, for example:

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

When taking an immutable borrow of an expression e, that is, `&_Const e`, before the lifetime of this immutable borrow ends, e can only be read and cannot be modified, nor can a mutable borrow of e be created, but an immutable borrow of e can still be taken.

| Immutable Borrow Expression | State of the Borrowed Object |
| ---- | ---- |
| `&_Const ident` | The variable `ident` can only be read and cannot be modified, nor can a mutable borrow of the variable `ident` be created; creating an immutable borrow of the variable `ident` is allowed |
| `&_Const "string literal"` | The temporary variable is always in a "read-only" state |
| `&_Const (struct S) { ... }` | The temporary variable is always in a "read-only" state |
| `&_Const e->field` | `e->field` enters a "read-only" state, and modifying `*e` as a whole is not allowed either. But modifying other members pointed to by `e`, or taking a mutable borrow of other members, is allowed |
| `&_Const e.field` | `e.field` enters a "read-only" state, and modifying `e` as a whole is not allowed either. But modifying other members of `e`, or taking a mutable borrow of other members, is allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is an array) | `e` enters a "read-only" state; modifying `e` and its direct or indirect members, or taking a mutable borrow of other members, is not allowed |
| `&_Const e[index]` or `&_Const *e` (`e` is a pointer) | `*e` enters a "read-only" state; directly modifying `*e` and its direct or indirect members, or taking a mutable borrow of `*e` and its direct or indirect members, is not allowed. If `e` is an _Owned pointer type, then `e` also enters the read-only state. If `e` is a _Borrow pointer type (i.e. this is an immutable reborrow of `e`), then modifying what `e` points to is allowed, and after modifying the pointer, `e`'s read-write attribute is restored to what it was before the reborrow |

### The Effect of a Mutable Borrow on the Borrowed Object

When taking a mutable borrow of an expression e, that is, `&_Mut e`, the expression e enters a "frozen" state. Before the lifetime of this mutable borrow ends, e cannot be read, cannot be modified (including being moved), and cannot be borrowed.

| Mutable Borrow Expression | State of the Borrowed Object |
| ---- | ---- |
| `&_Mut ident` | The variable `ident` is frozen |
| `&_Mut "string literal"` | Compilation error (string literals are immutable and cannot be mutably borrowed) |
| `&_Mut (struct S) { ... }` | The temporary variable is frozen |
| `&_Mut e->field` | `e->field` is frozen, reading or writing `e->field` is not allowed, modifying `*e` as a whole is not allowed, but modifying other members pointed to by `e`, or taking a mutable borrow of other members, is allowed |
| `&_Mut e.field` | `e.field` is frozen, reading or writing `e.field` is not allowed, modifying `e` as a whole is not allowed, but modifying other members of `e`, or taking a mutable borrow of other members, is allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is an array) | `e` is frozen, reading or writing `e` and its members is not allowed |
| `&_Mut e[index]` or `&_Mut *e` (`e` is a pointer) | `*e` is frozen, reading, writing, or borrowing `*e` and its members is not allowed. If `e` is an _Owned pointer type, then reading and writing `e` is also not allowed; if `e` is a _Borrow pointer type (i.e. this is a mutable reborrow of `e`), then modifying what `e` points to is allowed, and after modifying the pointer, reading, writing, and borrowing `*e` and its direct or indirect members is allowed |

## Function Definitions Containing Borrow Types

1. It is not allowed for a function to have no borrow-type parameter among its parameters while its return is a borrow type.

2. If a function has one borrow-type parameter and its return is a borrow type, then we directly consider that this return value's borrow comes from this borrow-type parameter; the "borrowed object" of the returned borrow is the same as the "borrowed object" of this borrow-type parameter, and this returned borrow should also satisfy the borrow rules mentioned earlier.

3. If a function has multiple borrow-type parameters and its return is a borrow type, then we directly consider that this return value's borrow simultaneously contains the "borrowed variables" passed from the multiple borrow-type parameters, and this returned borrow should also satisfy the borrow rules mentioned earlier.

```C
int *_Borrow f1(int *_Borrow p) { return p; }
int *_Borrow f2(int *_Borrow p1, int *_Borrow p2) { return p1; }

void test() {
  int local = 5;
  int *_Borrow p1 = f1(&_Mut local);
  /* The parameter of function f1 creates a mutable borrow of local, and this borrow is passed to the return value p1,
     making p1 equivalent to a mutable borrow of local, so the borrowed object of the return value p1 is
     local; before p1's lifetime ends, local will remain frozen. */

  int local1, local2;
  int *_Borrow p2 = f2(&_Mut local1, &_Mut local2);
  /* The parameters of function f2 create mutable borrows of local1 and local2,
     and these two borrows are passed to the return value p2, making p2 equivalent to a mutable borrow of local1 and
     local2, so the borrowed objects of the return value p2 are local1 and local2; before p2's
     lifetime ends, local1 and local2 remain frozen. */
}

int main() {
  test();
  return 0;
}
```

## Struct Definitions Containing Borrow Types

1. If a struct contains multiple borrow members, then this struct simultaneously has multiple "borrowed objects", and these borrow members should also satisfy the borrow rules mentioned earlier.

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

struct R {
  int *_Borrow m1;
  int *_Borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &_Mut local1, .m2 = &_Mut local2};
  // Before r's lifetime ends, local1 and local2 remain frozen.
  // Because the variable r created a mutable borrow of local1 and local2 during initialization,
  // making r simultaneously contain a mutable borrow of local1 and a mutable borrow of local2.
}

int main() {
  test();
  return 0;
}
```

## Dereference Operations on Borrow Variables

Dereferencing a borrow pointer variable is allowed, consistent with standard C dereference operations: the syntax for dereferencing a borrow pointer variable `p` is `*p`.
Dereferencing a borrow variable `e` of type `const T * _Borrow` as `*e` yields a result of type `T`.
Dereferencing a borrow variable `e` of type `T * _Borrow` as `*e` yields a result of type `T`.
If `p` is a borrow pointing to type `T`, and `o` is an lvalue of type `T`, then for the `*p` expression there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | *p = expr; not allowed | *p = expr; not allowed |
| | o = *p; allowed | o = *p; not allowed |
| p is a mut borrow | *p = expr; allowed | *p = expr; allowed |
| | o = *p; allowed | o = *p; not allowed |

In the table above, move / copy semantics respectively refer to: `T` being a type qualified by `_Owned` and `T` being other types.

Note: The permissions for the assignment operations in the table above also apply to function argument-passing and return scenarios.

## Member Access on Borrow Variables

A borrow pointer variable is allowed to access member variables or call member functions, consistent with the standard C arrow operator: the syntax for accessing the member variable `field` of pointer variable `p` is `p->field`, and the syntax for calling the member method `method()` of pointer variable `p` is `p->method()`.

### Accessing Member Variables

When accessing a member variable through a borrow, the type of the expression depends on the type of the member variable itself. The type of the `p->field` expression is the same as the type defined for the `field` member.
If the type of `p->field` is `T`, and `o` is an lvalue of type `T`, then for the `p->field` expression there are the following restrictions:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | p->field = expr; not allowed | p->field = expr; not allowed |
| | o = p->field; allowed | o = p->field; not allowed |
| p is a mut borrow | p->field = expr; allowed | p->field = expr; allowed |
| | o = p->field; allowed | o = p->field; not allowed |

In the table above, move / copy semantics respectively refer to: T being a type qualified by _Owned and T being other types.

Note: The permissions for the assignment operations in the table above also apply to function argument-passing and return scenarios.

### Calling Member Functions

When calling a member function through a borrow, that is, the `p->method()` scenario, the rules between the argument and the parameter are as follows:

| | void method(const This * _Borrow this) | void method(This * _Borrow this) | |
| --- | ---- | ---- | --- |
| p is an immut borrow | allowed | not allowed, an immut borrow cannot create a mut borrow | |
| p is a mut borrow | allowed, an immut borrow can be created from a mut borrow | allowed | |

For example:

```C
void int ::method1(const This *_Borrow this) {}
void int ::method2(This *_Borrow this) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local;
  p1->method1(); // ok: parameter type and argument type are consistent, both immutable borrows
  p1->method2(); // error: parameter is a mutable borrow type, argument is an immutable borrow type, an immutable borrow cannot create a mutable borrow
  p2->method1(); // ok: parameter is an immutable borrow type, argument is a mutable borrow type, creating an immutable borrow from a mutable borrow is allowed
  p2->method2(); // ok: parameter type and argument type are consistent, both mutable borrows
}

int main() {
  test();
  return 0;
}
```

## Type Conversion of Borrows

1. For any type T, if T implements `_Trait TR`, then a borrow pointing to type T is allowed to be upcast to a borrow pointing to type TR; conversely, conversion from a borrow of type TR to a borrow of type T is not allowed.

```C
#include <stdio.h>

_Trait TR { void print(This * _Borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

_Impl _Trait TR for int;

void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  _Trait TR *_Borrow p = r; // ok: supports upcasting a borrow of type int* to a borrow of type _Trait TR*
  p->print();
  int *_Borrow px = (int *_Borrow)p; // error: downcasting of _Trait TR* is prohibited
}

int main() {
  test();
  return 0;
}
```

2. When the pointed-to types differ, a borrow pointing to T is allowed to be implicitly converted to a borrow pointing to type void; conversely, conversion from a borrow of type void to a borrow of type T must be performed explicitly. Type conversions between other borrow pointers with different pointed-to types are not allowed.

```C
void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  void *_Borrow p = r;
  int *_Borrow t1 = p; // error: implicit conversion from void *_Borrow type is not allowed
  int *_Borrow t2 = (int *_Borrow)p; // ok: explicit conversion from void *_Borrow type is allowed
}

int main() {
  test();
  return 0;
}
```

3. Conversion between `T * _Borrow` and `T *` is only allowed in unsafe zones.

```C
int main() {
  int *_Borrow p = (int *_Borrow)NULL; // ok: unsafe zone allows conversion between T * _Borrow and T *
  int *q = p; // error: type conversion must be explicit, implicit type conversion is prohibited
  _Safe { int *_Borrow p = (int *_Borrow)NULL; } // error: safe zone prohibits conversion between T * _Borrow and T *
  return 0;
}
```

4. C-style casting between `T *_Owned` and `T *_Borrow` pointers is not allowed.

```C
int *_Owned test(int *_Owned p) {
  int *_Borrow q = (int *_Borrow) p; // error: &_Mut *p should be used instead of a cast
  int *_Owned r = (int *_Owned) q; // error: a T *_Owned copy cannot be created from a T *_Borrow via a cast
  return r;
}
```

5. The mutable borrow type `T *_Borrow` can be implicitly converted to the immutable borrow type `const T *_Borrow`, with the compiler automatically inserting the `&_Const *` operator. Casting between mutable borrows and read-only borrows is not allowed.

   The following scenarios can trigger implicit conversion from a mutable borrow to an immutable borrow:
   1. Variable initialization and assignment
   2. Function call expression argument passing
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

For `_ArrayElem` borrows, it is also allowed to implicitly convert `T *_Borrow _ArrayElem` to `T *_Borrow`, which is equivalent to "re-taking the borrow at the current location, obtaining an ordinary borrow pointer"; the reverse `T *_Borrow -> T *_Borrow _ArrayElem` cast is not allowed.

```C
void test(int arr[4], int local) {
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  int *_Borrow q = p; // ok, equivalent to &_Mut *p

  int *_Borrow plain = &_Mut local;
  int *_Borrow _ArrayElem bad = (int *_Borrow _ArrayElem)plain; // error: reverse conversion is not allowed
}
```

6. Implicit conversion of a `_Borrow` pointer to `_Bool` is allowed in variable initialization, variable assignment, function argument passing, and return.

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

In addition to the rules above, for borrowing we also have the following rules:

1. For global variables, we cannot track in function signatures which function reads a global variable and which function modifies a global variable. To ensure safety, BiSheng C stipulates: within a safe zone, only read-only borrows of global variables are allowed, and mutable borrows are not allowed. If a borrow is taken of a function name, from a lifetime perspective it can be regarded as a borrow of a global variable.

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

3. When using a borrow-type expression to initialize or reassign another borrow-type lvalue, that is, `p = e`, `p` and `e` must be borrow types of the same kind, and `e`'s lifetime must be longer than p's lifetime.

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
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

int g = 5
int *_Borrow p = &_Mut g; // error: a borrow variable is not allowed to be a global variable
void test() { int *_Borrow p = &_Mut g; }

int main() {
  test();
  return 0;
}
```

5. Taking a borrow of an expression that already contains a borrow is not allowed. Similarly, in the borrow type `T* _Borrow`, `T` itself and its members cannot be borrow types.

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

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

7. Adding member functions to a borrow type is not allowed.

```C
void int *_Borrow::f() {
