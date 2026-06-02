# Borrowing

Borrowing, as an important part of BiSheng C's memory safety features, complements ownership. The previous section described the ownership feature; the entity that owns a resource is responsible for releasing it. This section introduces borrowing a resource.

## Feature Overview

If we only had ownership types, code would be very limited, since operations such as function calls and assignments transfer ownership. When programming, we often need to express the concept of "borrowing a resource", as distinct from "owning a resource". Just as in real life, if someone owns something, you can borrow it from them, and when you are done using it, you must return it to its owner.

### Definition of Borrowing and the Borrow Operators

In BiSheng C, **a borrow is a pointer type that points to the memory address where the borrowed object is stored**. To express the concept of borrowing:

1. A new keyword `_Borrow` is introduced. Using `_Borrow` to qualify a pointer type T* denotes the borrow type of T.
2. The borrow operators `&_Mut` and `&_Const` are introduced. Here, `&_Mut e` obtains a **mutable borrow** of expression e, and `&_Const e` obtains a **read-only borrow** of expression `e`. Here, expression `e` is required to be an lvalue. Similar to the address-of operator `&` in standard C, the borrow operator actually obtains the address of expression `e`.

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

In addition, if expression `e` is a pointer dereference expression, `&_Mut *p` and `&_Const *p` can be viewed respectively as taking a mutable borrow and an immutable borrow of the value stored at address `p`, that is, `*p`. **This operation does not produce a temporary variable for `*p`**. Here, `p` can be a raw pointer, an `_Owned` pointer, or another borrow pointer. For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

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

### The Purpose of Borrowing

Suppose we have the following requirement: create a file and call some operation functions to read from and write to the file. Without the concept of borrowing, calling a file operation function would transfer ownership of the file pointer. To keep the file pointer usable after the function call, we would need to return ownership back to the caller:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory
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
  // Transfer ownership back to the caller via the return value, avoiding loss of ownership
  return p;
}

MyFile *_Owned other_operation(MyFile *_Owned p) {
  // some operation
  // Transfer ownership back to the caller via the return value, avoiding loss of ownership
  return p;
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // p's ownership is first moved into insert_str, then transferred back to the caller via the return value
  p = insert_str(p, str);
  p = other_operation(p);
  file_safe_free(p);
  return 0;
}
```

This style causes frequent ownership transfers of the file pointer, which is error-prone when the code logic is more complex. Moreover, if ownership is moved away but not returned, the file pointer can no longer be used afterwards. With borrowing, we pass a borrow of the file pointer as the argument to the operation function. After the function returns, the file pointer can still be used for subsequent operations. We no longer need, as in the example above, to first pass ownership in via a function parameter and then pass ownership out via the function's return value, making the code more concise:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory
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

We can take borrows of various kinds of objects: `_Owned` variables, local variables of non-`_Owned` types, global variables, temporary anonymous variables, parameters, and even part of a compound variable. To correctly represent the valid scopes of borrow variables and of the various kinds of borrowed objects, we introduce the concept of lifetimes.

The main purpose of lifetime checking is to avoid dangling pointers, which cause a program to use data it should not. The following C code is a typical example of using a dangling pointer:

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

There are two things worth noting about this C code:

1. Declaring `int *p` carries the risk of using `NULL`;
2. `p` points to the `local` variable in the inner block, but `local` is released when the block ends. Therefore, after returning to the outer block, `p` points to an invalid address and is a dangling pointer, pointing to the `local` variable that was released early. As one can foresee, `*p = 1` causes undefined behavior at runtime for this program. When the code logic is more complex, such abnormal behavior is very hard to detect.

For the second point, BiSheng C stipulates: **no borrow of a resource may outlive the resource's owner**. In other words, the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object.

Next, we rewrite the C code above using BiSheng C's borrowing feature. By checking the lifetimes of borrow variables and borrowed objects, potential memory safety risks can be identified at compile time:

```C
int main() {
  int local1 = 1;
  // The borrow pointer variable p must be initialized before use, otherwise an error is reported
  int *_Borrow p = &_Mut local1;
  {
    int local2 = 2;
    // After reassigning p, p no longer borrows local1, but instead borrows local2
    p = &_Mut local2;
  }
  *p = 3; // error: local2 does not live long enough
  return 0;
}
```

### Borrow Variables and Borrowed Objects

Every borrow variable (that is, a _Borrow pointer variable) has one or more borrowed objects. For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

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

  // The borrowed object is a function's return value, the same as the "borrowed objects" of the called function's borrow-type parameters
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

Note: if a borrowed object results from taking an address or casting to a raw pointer and then taking a borrow, it is not recorded as a borrowed object. For example:

```C
void f1() {
  int a = 1;
  int *p = &a;
  int *_Borrow p1 = (int *_Borrow)(int *)(&_Mut *p); // *p is not recorded as a borrowed object
  int *_Borrow p2 = &_Mut *&a; // a is not recorded as a borrowed object
}
```

### Non-Lexical Lifetime of Borrow Variables

A variable's lifetime begins at its declaration and ends at the end of the entire enclosing block. This design is called Lexical Lifetime, because the variable's lifetime is strictly bound to its scope in the lexical structure. This strategy is very simple to implement, but it can be overly conservative: in some cases the active range of a borrow variable is excessively extended, so that some code that is in fact safe is also rejected, which to some extent limits the code a programmer can write. Therefore, BiSheng C introduces Non-Lexical Lifetime (abbreviated NLL) for borrow variables, using a more fine-grained means to compute the range over which a borrow variable is truly in effect. **The NLL range of a borrow variable is: from the point of borrowing, continuing until the place where it is last used.** Specifically, it spans **from the point where the borrow variable is defined or reassigned, to the last use before it is reassigned.**

The following scenarios constitute uses of borrow variable p:

1. Function call, such as `use(p)` or `use(&_Mut *p)`
2. Function return `return p` or `return &_Mut *p`
3. Dereference `*p`
4. Member access `p->field`

For example:

```C
void use(int *_Borrow p) {}
void other_op() {}

// In this example, p's NLL is segmented; each NLL segment has one borrowed object
void foo() {
  int local1 = 1, local2 = 2;  //#1
  int *_Borrow p = &_Mut local1; //#2, p's first NLL segment begins, borrowed object is local1
  other_op();                  //#3
  use(p);                      //#4, p's first NLL segment ends
  other_op();                  //#5
  p = &_Mut local2;             //#6, p's second NLL segment begins, borrowed object is local2; since there is no further use of p afterwards, p's NLL ends
  other_op();     //#7
}
// p's NLL is: [2,4]->local1, [6,6]->local2

int main() {
  foo();
  return 0;
}
```

### Lexical Lifetime of Borrowed Objects

Unlike borrow variables, the lifetime of a borrowed object is a Lexical Lifetime. For the lifetimes of the different kinds of borrowed objects, we give specific definitions:

| Kind of borrowed object | | Lifetime definition |
| ---- | ---- | ---- |
| Global variable | | A global variable's lifetime is the entire program; it exists from the start of the program until it exits |
| Local variable | owned variable | From the variable's definition until it is moved away (an `_Owned` struct type, if not moved, has a lifetime that ends at the end of the current block) |
| | non-owned, non-borrow variable | From the variable's definition until the end of the current block |
| Local literal | "string literal" | From the point of use until the end of the current block |
| | (struct S) { ... } | From the point of use until the end of the current block |
| e->field | | The lifetime of e |
| e.field | | The lifetime of e |
| e[index] | | The lifetime of e |
| *e | | The lifetime of e |

### Lifetime Constraints of Borrows

In Section 2.1 we mentioned that, for borrows, we have the following lifetime constraint: **the lifetime of a borrow variable cannot be longer than the lifetime of the borrowed object.**
For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

void use(int *_Borrow p) {}
int *_Borrow call(int *_Borrow p, int *_Borrow q) { return p; }

// In this example, p's lifetime is [2,4], and the borrowed object local's lifetime is [1,4], satisfying the lifetime constraint
void test1() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  use(p);                     //#3
} //#4

// In this example, p's lifetime has two segments:
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
// error: the second segment is [5,7], and there are two borrowed objects, local1 and local2, where local2's lifetime is [4, 6], not satisfying the lifetime constraint
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

BiSheng C ranks the permissions of borrow pointers into mutable (mut) borrows and immutable (immut) borrows. We can manipulate a mutable borrow pointer to read from and write to the contents of the borrowed object, whereas through an immutable borrow pointer we can only read the contents of the borrowed object but cannot modify it. For example:

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

We mentioned in Section 1.1 that `&_Mut e` and `&_Const e` require expression e to be an lvalue, that is, e must be addressable. For the mutable borrow expression `&_Mut e`, we additionally require that e be mutable. Specifically:

| lvalue expression | Whether modifiable |
| ---- | ---- |
| ident | The variable ident is not qualified by const, and ident cannot be a function name |
| "string literal" | Not allowed, because string constants are stored in the constant area and cannot be written. Attempting to take a mutable borrow of a string literal (such as `&_Mut "hello"` or `&_Mut * "hello"`) causes a compile error |
| (struct S) { ... } | Allowed |
| e->field | Requires that e be a mutable borrow pointer, or an `_Owned` pointer to a modifiable type, or a raw pointer to a modifiable type, and that field not be qualified by const; in the case of multi-level fields, every level of field must not be qualified by const |
| e.field | Requires that e be mutable and that field not be qualified by const; in the case of multi-level fields, every level of field must not be qualified by const |
| e[index] | Requires that e be mutable |
| *e | Requires that e be a mutable borrow, or a raw pointer to a modifiable variable |

### At Most One Mutable Borrow at a Time

If two or more pointers access the same data at the same time and at least one of them is used to write the data, undefined behavior may result. For example:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory
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

Since a borrow is essentially a pointer, to avoid the problem above, BiSheng C stipulates: **at any given moment, for the same object, there can be either exactly one mutable borrow, or any number of immutable borrows.**

```C
void write(int *_Borrow p) {}
void read(const int *_Borrow p) {}

void test1() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  int *_Borrow p2 = &_Mut local; // error: at any given moment there can be at most one mutable borrow variable pointing to local
  write(p1);
  write(p2);
}

void test2() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  const int *_Borrow p2 = &_Const local; // error: a mutable and an immutable borrow pointing to local cannot exist at the same time
  write(p1);
  read(p2);
}

void test3() {
  int local = 1;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local; // error: a mutable and an immutable borrow pointing to local cannot exist at the same time
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

If a mutable borrow and an immutable borrow of the same variable exist at the same time, it may happen that the memory state of the borrowed object is modified through the mutable borrow, and then the modified memory is accessed through the immutable borrow, resulting in undefined behavior.
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
  a.free_p(); // error: a is mutably borrowed more than once
  // the *q operation may cause undefined behavior
  printf("%d\n", *q);
  return 0;
}
```

In the code above, `a.free_p()` actually uses a mutable borrow pointing to a, and this mutable borrow invalidates the borrow q that was defined before it. Since `printf("%d\n", *q)` uses the invalidated q, the BiSheng C compiler reports an error, thereby preventing the unsafe behavior from occurring.

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

### Automatic Borrow Conversion for String Literals

To make string literals convenient to use, BiSheng C provides an automatic borrow conversion feature for string literals.

#### Automatically Inserting the `&_Const *` Operator

When a string literal is passed as an argument to a parameter of type `const char * _Borrow`, the compiler automatically inserts the `&_Const *` operator to convert the string literal into an immutable borrow pointer. This avoids having to manually add a borrow operator every time a string literal is used.

For example:

```C
void foo(const char * _Borrow str) {
  // function implementation
}

void test() {
  // The compiler automatically converts "hello" to &_Const * "hello"
  foo("hello");

  // Equivalent to the explicit form
  foo(&_Const * "hello");
}

int main() {
  test();
  return 0;
}
```

#### Implicit Conversion from Mutable Borrow to Immutable Borrow

That is, a mutable borrow of type `T *_Borrow` can be implicitly converted to an immutable borrow of type `const T *_Borrow`, with the compiler automatically inserting the `&_Const *` operator.

This implicit type conversion is permitted in the following four positions:

1. Variable declaration statements
2. Variable assignment expressions
3. Argument passing in function call expressions
4. A function's return statement

Some examples are given below:

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

#### Restrictions and Error Checking

String literals are stored in a read-only memory region and therefore cannot be modified. BiSheng C imposes the following restrictions on borrows of string literals:

1. **Mutable borrows of string literals are forbidden**: directly using the `&_Mut` operator to take a mutable borrow of a string literal causes a compile error.

```C
void test_error1() {
  const char * _Borrow p = &_Mut "hello";  // compile error: cannot take mutable _Borrow of string literal
}
```

2. **Taking a mutable borrow of a string literal through dereference is forbidden**: using the `&_Mut *` operator to take a mutable borrow of a string literal also causes a compile error.

```C
void test_error2() {
  const char * _Borrow p = &_Mut * "hello";  // compile error: cannot take mutable _Borrow through string literal
}
```

3. **Passing a string literal to a mutable borrow parameter is forbidden**: attempting to pass a string literal as an argument to a parameter of type `char * _Borrow` (a mutable borrow) causes a compile error.

```C
void foo_mut(char * _Borrow str) {}

void test_error3() {
  foo_mut("hello");  // compile error: cannot pass string literal to parameter of type 'char *_Borrow'
}
```

These restrictions ensure the immutability of string literals, avoid illegal writes to the read-only memory region, and improve program safety.

## The Effect of Borrowing on the Borrowed Object

### The Effect of an Immutable Borrow on the Borrowed Object

When an immutable borrow of expression e is taken, that is `&_Const e`, then before this immutable borrow's lifetime ends, e can only be read and cannot be modified, and a mutable borrow of e cannot be created.

| Immutable borrow expression | State of the borrowed object |
| ---- | ---- |
| &_Const ident` | The variable ident can only be read and cannot be modified, and a mutable borrow of the variable ident can no longer be created; creating an immutable borrow of the variable ident is allowed |
| &_Const "string literal" | The temporary variable is always in a "read-only" state |
| &_Const (struct S) { ... } | The temporary variable is always in a "read-only" state |
| &_Const e->field | e->field enters a "read-only" state, and modifying *e as a whole is also not allowed. But modifying other members pointed to by e, or taking mutable borrows of other members, is allowed |
| &_Const e.field | e.field enters a "read-only" state, and modifying e as a whole is also not allowed. But modifying other members of e, or taking mutable borrows of other members, is allowed |
| &_Const e[index] | e enters a "read-only" state; modifying e and its direct or indirect members, or taking mutable borrows of other members, is not allowed |
| &_Const *e | `*e` enters a "read-only" state; modifying `*e` and its direct or indirect members, or taking mutable borrows of other members, is not allowed; if e is an _Owned pointer type, then e also enters a read-only state |

### The Effect of a Mutable Borrow on the Borrowed Object

When a mutable borrow of expression e is taken, that is `&_Mut e`, expression e enters a "frozen" state. Before this mutable borrow's lifetime ends, e cannot be read, cannot be modified (including being moved), and cannot be borrowed.

| Mutable borrow expression | State of the borrowed object |
| ---- | ---- |
| &_Mut ident` | The variable ident is frozen |
| &_Mut "string literal" | Compile error (string literals are immutable and cannot be mutably borrowed) |
| &_Mut (struct S) { ... } | The temporary variable is frozen |
| &_Mut e->field | e->field is frozen; reading or writing e->field is not allowed, and modifying *e as a whole is not allowed, but modifying other members pointed to by e, or taking mutable borrows of other members, is allowed |
| &_Mut e.field | e.field is frozen; reading or writing e.field is not allowed, and modifying e as a whole is not allowed, but modifying other members of e, or taking mutable borrows of other members, is allowed |
| &_Mut e[index] | e is frozen; reading or writing e and its members is not allowed |
| &_Mut *e | `*e` is frozen; reading or writing `*e` and its members is not allowed; if e is an _Owned pointer type, then reading or writing e is also not allowed |

## Borrow Types in Function Definitions

1. It is not allowed for a function to have no borrow-type parameter while having a borrow-type return value.

2. If a function has one borrow-type parameter and its return is a borrow type, then we directly consider that the borrow of this return value comes from this borrow-type parameter; the "borrowed object" of the returned borrow is the same as the "borrowed object" of this borrow-type parameter, and this returned borrow must also satisfy the borrowing rules mentioned earlier.

3. If a function has multiple borrow-type parameters and its return is a borrow type, then we directly consider that the borrow of this return value simultaneously contains the "borrowed variables" passed from the multiple borrow-type parameters, and this returned borrow must also satisfy the borrowing rules mentioned earlier.

```C
int *_Borrow f1(int *_Borrow p) { return p; }
int *_Borrow f2(int *_Borrow p1, int *_Borrow p2) { return p1; }

void test() {
  int local = 5;
  int *_Borrow p1 = f1(&_Mut local);
  /* The parameter of function f1 creates a mutable borrow of local, and this borrow
     is passed to the return value p1, making p1 effectively a mutable borrow of local.
     So the borrowed object of the return value p1 is local, and local stays frozen
     until p1's lifetime ends. */

  int local1, local2;
  int *_Borrow p2 = f2(&_Mut local1, &_Mut local2);
  /* The parameters of function f2 create mutable borrows of local1 and local2,
     and these two borrows are passed to the return value p2, making p2 effectively a
     mutable borrow of local1 and local2. So the borrowed objects of the return value p2
     are local1 and local2, and local1 and local2 stay frozen until p2's lifetime ends. */
}

int main() {
  test();
  return 0;
}
```

## Borrow Types in struct Definitions

1. If a struct contains multiple borrow members, then this struct simultaneously has multiple "borrowed objects", and these borrow members must also satisfy the borrowing rules mentioned earlier.

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

struct R {
  int *_Borrow m1;
  int *_Borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &_Mut local1, .m2 = &_Mut local2};
  // local1 and local2 stay frozen until r's lifetime ends.
  // Because when the variable r was initialized it created a mutable borrow of local1 and local2,
  // r simultaneously contains a mutable borrow of local1 and a mutable borrow of local2.
}

int main() {
  test();
  return 0;
}
```

## Dereferencing a Borrow Variable

A borrow pointer variable may be dereferenced, consistent with the dereference operation in standard C: the syntax for dereferencing a borrow pointer variable `p` is `*p`.
Dereferencing a borrow variable `e` of type `const T * _Borrow`, `*e`, yields a result of type `T`.
Dereferencing a borrow variable `e` of type `T * _Borrow`, `*e`, yields a result of type `T`.
If `p` is a borrow pointing to type `T`, and `o` is an lvalue of type `T`, then for the `*p` expression, the following restrictions apply:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | *p = expr; not allowed | *p = expr; not allowed |
| | o = *p; allowed | o = *p; not allowed |
| p is a mut borrow | *p = expr; allowed | *p = expr; allowed |
| | o = *p; allowed | o = *p; not allowed |

In the table above, Move / Copy semantics refer respectively to: `T` being a type qualified by `_Owned` and `T` being any other type.

Note: the assignment permissions in the table above apply equally to function argument passing and return scenarios.

## Member Access on a Borrow Variable

A borrow pointer variable may access member variables or call member functions, consistent with the arrow operator in standard C: the syntax for accessing the member variable `field` of a pointer variable `p` is `p->field`, and the syntax for calling the member method `method()` of a pointer variable `p` is `p->method()`.

### Accessing Member Variables

When accessing a member variable through a borrow, the type of the expression depends on the type of the member variable itself. The type of the `p->field` expression is the same as the type defined for the member `field`.
If the type of `p->field` is `T`, and `o` is an lvalue of type `T`, then for the `p->field` expression, the following restrictions apply:

| | T has Copy semantics | T has Move semantics |
| ---- | ---- | ---- |
| p is an immut borrow | p->field = expr; not allowed | p->field = expr; not allowed |
| | o = p->field; allowed | o = p->field; not allowed |
| p is a mut borrow | p->field = expr; allowed | p->field = expr; allowed |
| | o = p->field; allowed | o = p->field; not allowed |

In the table above, Move / Copy semantics refer respectively to: T being a type qualified by _Owned and T being any other type.

Note: the assignment permissions in the table above apply equally to function argument passing and return scenarios.

### Calling Member Functions

When calling a member function through a borrow, that is the `p->method()` scenario, the rules between arguments and parameters are as follows:

| | void method(const This * _Borrow this) | void method(This * _Borrow this) | |
| --- | ---- | ---- | --- |
| p is an immut borrow | allowed | not allowed; an immut borrow cannot create a mut borrow | |
| p is a mut borrow | allowed; creating an immut borrow from a mut borrow is allowed | allowed | |

For example:

```C
void int ::method1(const This *_Borrow this) {}
void int ::method2(This *_Borrow this) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local;
  p1->method1(); // ok: the parameter type and argument type match, both immutable borrows
  p1->method2(); // error: the parameter is a mutable borrow type, the argument is an immutable borrow type; an immutable borrow cannot create a mutable borrow
  p2->method1(); // ok: the parameter is an immutable borrow type, the argument is a mutable borrow type; creating an immutable borrow from a mutable borrow is allowed
  p2->method2(); // ok: the parameter type and argument type match, both mutable borrows
}

int main() {
  test();
  return 0;
}
```

## Borrow Type Conversions

1. For any type T, if T implements `_Trait` TR, then a borrow pointing to type T is allowed to be upcast to a borrow pointing to type TR; conversely, converting from a borrow of type TR to a borrow of type T is not allowed.

```C
#include <stdio.h>

_Trait TR { void print(This * _Borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

_Impl _Trait TR for int;

void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  _Trait TR *_Borrow p = r; // ok: upcasting an int* borrow to a _Trait TR* borrow is supported
  p->print();
  int *_Borrow px = (int *_Borrow)p; // error: downcasting from _Trait TR* is forbidden
}

int main() {
  test();
  return 0;
}
```

2. A borrow pointing to type T is allowed to be implicitly converted to a borrow pointing to type void; conversely, implicitly converting from a borrow of type void to a borrow of type T is not allowed, but an explicit cast is allowed.

```C
void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  void *_Borrow p = r;
  int *_Borrow t1 = p; // error: implicit conversion to void *_Borrow type is not allowed
  int *_Borrow t2 = (int *_Borrow)p; // ok: an explicit cast to void *_Borrow type is allowed
}

int main() {
  test();
  return 0;
}
```

3. Conversions between `T * _Borrow` and `T *` are allowed only outside the safe zone.

```C
int main() {
  int *_Borrow p = (int *_Borrow)NULL; // ok: outside the safe zone, conversion between T * _Borrow and T * is allowed
  int *q = p; // error: type conversions must be explicit; implicit type conversions are forbidden
  _Safe { int *_Borrow p = (int *_Borrow)NULL; } // error: inside the safe zone, conversion between T * _Borrow and T * is forbidden
  return 0;
}
```

4. C-style casts between `T *_Owned` and `T *_Borrow` pointers are not allowed.

```C
int *_Owned test(int *_Owned p) {
  int *_Borrow q = (int *_Borrow) p; // error: &_Mut *p should be used instead of a cast
  int *_Owned r = (int *_Owned) q; // error: a cast cannot create a T *_Owned copy from a T *_Borrow
  return r;
}
```

5. Casts between a mutable borrow (`T *_Borrow`) and a read-only borrow (`const T *_Borrow`) are not allowed.

Such a conversion would break the safety of borrow checking:

- Converting a mutable borrow to a read-only borrow would create two independent borrows (one mutable, one read-only) pointing to the same object, thereby allowing modification while the read-only borrow exists
- Converting a read-only borrow to a mutable borrow would violate const safety, allowing modification through a borrow that was originally read-only

```C
#include <stdio.h>

int main() {
  int local = 10;
  int *_Borrow p = &_Mut local;
  const int *_Borrow b = (const int *_Borrow)p; // error: casting a mutable borrow to a read-only borrow is not allowed
  printf("%d\n", *b);  // read b
  *p = 1;              // modify p (if the cast were allowed, this would violate the borrowing rules)
  printf("%d\n", *b);  // read b again

  const int *_Borrow c = &_Const local;
  int *_Borrow m = (int *_Borrow)c; // error: casting a read-only borrow to a mutable borrow is not allowed
  *m = 20;  // if the cast were allowed, this would violate const safety

  return 0;
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

Note: conversions between borrows of the same kind (mutable to mutable, read-only to read-only) are allowed.

## Other Rules for Borrowing

In addition to the rules above, we also have the following rules for borrowing:

1. For global variables, we cannot track in a function signature which function reads a global variable and which function modifies it. To ensure safety, BiSheng C stipulates: inside the safe zone, only read-only borrows of global variables are allowed; mutable borrows are not allowed. If a borrow is taken of a function name, then from a lifetime perspective, it can be treated as borrowing a global variable.

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

3. When initializing or reassigning one borrow-type lvalue with another borrow-type expression, that is `p = e`, `p` and `e` must be the same kind of borrow type, and the lifetime of `e` must be longer than the lifetime of p.

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

Based on this rule, a `_Borrow` pointer member inside a `struct` cannot take a borrow of this `struct` or any of its other members.

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
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

int g = 5
int *_Borrow p = &_Mut g; // error: a borrow variable is not allowed to be a global variable
void test() { int *_Borrow p = &_Mut g; }

int main() {
  test();
  return 0;
}
```

5. Taking a borrow of an expression that contains a borrow is not allowed. Likewise, in a borrow type `T* _Borrow`, `T` itself and its members cannot be borrow types.

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language for safely allocating and freeing memory

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

6. Implementing a `_Trait` for a borrow type is not allowed.

```C
_Trait TR{};

_Impl _Trait TR for int *_Borrow; // error: implementing a _Trait for a borrow type is not allowed

int main() { return 0; }
```

7. Adding member functions to a borrow type is not allowed.

```C
void int *_Borrow::f() {} // error: adding member functions to a borrow type is not allowed

int main() { return 0; }
```

8. A union member is not allowed to be a borrow type.

```C
union U {
  int *_Borrow p; // error: a borrow pointer is not allowed as a union member
};

int main() { return 0; }
```

9. A borrow pointer type cannot be a generic argument.

10. Borrow pointer variables do not support indexing.

11. Borrow pointer variables do not support arithmetic operations.

12. Comparison operators such as `==`, `!=`, `>`, `<`, `<=`, `>=` are allowed between borrow variables of the same kind.

13. The `sizeof` and `alignof` operators are allowed on borrow types, and we have:
`sizeof(T* _Borrow) == sizeof(T*)`
`_Alignof(T* _Borrow) == _Alignof(T*)`

14. The unary `&`, `!` and binary `&&`, `||` operators are allowed on borrow types.

15. The unary `-`, `~`, `&_Const`, `&_Mut`, `[]`, `++`, `--` operators are not allowed on borrow types, and the binary `*`, `/`, `%`, `&`, `|`, `<<`, `>>`, `+`, `-` operators are also not allowed on borrow types.

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

18. A `_Borrow` pointer is allowed as the condition of `if`, `while`, `do-while`, and `for` statements and ternary expressions, but is not allowed as the condition of a `switch` statement.

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
