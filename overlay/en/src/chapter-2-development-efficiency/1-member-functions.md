# Member Functions

## Overview

In the C language, if we want to express that some data type has a corresponding operation, we generally use a global function and pass the data type as a parameter, as shown below:

```c
#include<stdio.h>
struct Data{
  int x;
};

void print_data(struct Data* data) {
    // provide the implementation logic for print data
    printf("print data\n");
}

int main() {
    struct Data data = {.x = 1};
    print_data(&data);// expected result: print data
    return 0;
}
```

Similarly, for the int type we might need a `print_int` function, and for the float type we might need a `print_float` function. This is not a pleasant thing to do, and we want a more concise way to express the association between types and methods. This is part of the motivation for introducing member functions into the C language. After introducing member functions, the code in the example above can be written like this:

```c
#include<stdio.h>
struct Data{
  int x;
};

void struct Data::print(struct Data* this) {
    // provide the implementation logic for print data
    printf("print data\n");
}

void int::print(int* this) {
    // provide the implementation logic for print int
    printf("print int\n");
}

void float::print(float* this) {
    // provide the implementation logic for print float
    printf("print float\n");
}

int main() {
    struct Data data = {.x = 1};
    int a = 1;
    float b = 1.0;
    data.print();// expected result: print data
    a.print();// expected result: print int
    b.print();// expected result: print float
}

```

If we want to express that certain types share a group of similar behaviors, such as `print` in the example above, we can define a `_Trait` and then let types such as `struct Data`, `int`, and `float` implement this `_Trait`. The combination of member functions and `_Trait` is very expressive. For an introduction to `_Trait`, refer to the later chapters.

Below, we briefly introduce some of the specific rules for BiSheng C member functions:

## Basic Syntax

When we want to add a member function to a type, we only need to add `typename::foo` before the function name (such as foo) on the basis of the ordinary function definition syntax, as shown below:

```c
void foo1() {
  // do nothing
};

void int::foo2(int* this) { // instance member function; the first parameter is the This pointer, pointing to the current int instance
  // do something
}

void int::foo3(int this) { // instance member function; the first parameter is the This instance, not the This pointer
  // do something
}

void int::foo4() { // static member function
  // do something
}

```

Here, the type-name can be a basic type such as `int` or `float`, or it can be a user-defined struct, etc., as long as it conforms to the C language's rules for defining types. In addition, `This` can be used to conveniently refer to the current type. Below are more usage examples:

```c
// case 1
void int::print(int* this); // declaration

void int::print(int* this){ // definition
    printf("int::print");
}

// case 2
struct S1{};
// incorrect use of S; in the C language, struct S is the actual type
void S1::print(struct S1* this); // error: must use 'struct' tag to refer to type 'S'
void struct S1::print(struct S1* this); // Ok, the corrected declaration

// case 3
typedef struct {
}S2;
void S2::print(S2* this); // Ok, S2 is the struct S2 after typedef

// case 4
void S2::print(This this); // Ok, This refers to the current type struct S2
void S2::print(This* this); // Ok, This* refers to a pointer to the current type struct S2

```

One benefit of this syntax design is that we can easily add member functions to existing types without intrusively modifying the source code.

## About `this`

In the member function examples above, if the function parameters begin with `this`, then the function is an instance member function, and in this case `this` must be the first parameter. It represents the instance of the type corresponding to the member function (This this), or a pointer to that instance (This* this); it is an "instance member function". If `this` is not present in the member function's parameter list, then this is a "static member function".

```c
typedef struct {/*...*/} M;
void M::f(M* this, int i) {} // instance member function

typedef struct {/*...*/} N;
void N::f() {} // static member function

int main() {
    M x;
    x.f(1); // Ok
    M::f(&x, 1); // Ok
    M* x1 = &x;
    x1->f(1); // Ok

    N y;
    y.f(); // Err: y does not have instance member function, use N::f instead.
    N::f(); // Ok
    return 0;
}

```

For instance member functions (whose first parameter is `this`), there are two ways to call them:

(1) Similar to accessing member variables: for an instance type, call with the `.` operator, such as `x.f(1)`; for a pointer type, call with the `->` operator, such as `x1->f(1)`.

(2) The ordinary function call form, such as `M::f(&x, 1)`.

For static member functions, the way to call them is similar to calling an ordinary global function; the only difference is that the function name becomes `type-name::func-name`, such as `N::f()`.

## Other Rules

- Member functions support separating declaration and definition, as shown below:

```c
// declaration
const char* int::to_string(const int* this) ;

// definition
const char* int::to_string(const int* this){
    // implementation of to_string, omitted
}

```

- Adding a member function does not affect the layout of the original type, including its size and alignment.
- Member functions do not support overloading or redefinition.
- A member function's name is not allowed to be the same as a member variable; this applies to struct, union, enum, etc.
- A member function may be assigned to a function pointer.
- It is forbidden to add member functions to a cv-qualified type (a type modified by type qualifiers, such as const int).
- It is forbidden to add member functions to "function types", "array types", or "pointer types".
- The `this` pointer is allowed to be modified by qualifiers such as const/volatile.
- It is not allowed to extend member functions for an incomplete type; an incomplete type is one that has been declared but not fully defined.
- It is not allowed to extend member functions for the `void` type.
- If two header files extend a member function with the same name for the same type, then including both header files in one compilation unit will cause a compilation error.
- Currently, it is temporarily forbidden to call member functions directly through integer literals, floating-point literals, or compound type literals.
