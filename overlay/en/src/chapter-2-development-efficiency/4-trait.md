# Trait

## Overview

A trait is a way of defining behavior; it is similar to interfaces or abstract classes in other languages, and its purpose is to define the set of behaviors that is necessary to accomplish certain goals. A trait defines a set of method signatures, and these methods can be shared by multiple structs, enums, or built-in types. Its main purpose is to achieve code reuse and abstraction, thereby improving the maintainability and extensibility of the code.

## Grammar Rules

BiSheng C introduces the keywords `_Trait` and `_Impl`. A trait is defined through the keyword `_Trait`, and through `_Impl` one or more traits can be implemented for a type. The following code examples illustrate how to use traits.

### Defining a Trait

**Syntax:**

```c
_Trait TraitName {
  // Define the method signatures in the _Trait
};
```

Here, `TraitName` is the name of the trait, followed by a pair of braces in which some method signatures can be defined. Methods defined in a trait do not support default implementations; they must be given a concrete implementation by the type that implements the trait. Let's look at a simple example:

```c
_Trait T {
    void doSomeThing1(This* this);
    void doSomeThing2(This* this);
};
```

**Rules:**

1. A trait definition can only appear at the top level

```c
void test() {
    _Trait T {}; // _Trait cannot be defined inside a function body. error: _Trait is only allowed to be defined at top-level
}

struct MyStruct {
    _Trait T {}; // _Trait cannot be defined inside a struct. error: _Trait is only allowed to be defined at top-level
};
```

2. A trait requires that the first parameter of a function, and only the first parameter, be of type `This` pointer, named `this`; `This` refers to the concrete type that implements the trait

```c
_Trait T {
    void doSomeThing1(This* this); // ok
    void doSomeThing2(This* a); // error: the first parameter of _Trait member function must be 'This* this'
    void doSomeThing3(int a, This* this); // error: the first parameter of _Trait member function must be 'This* this'
};
```

3. Only function declarations are allowed inside a trait

```c
_Trait T {
    void doSomeThing1(This* this) { // error: expected member name or ';' after declaration specifiers
    }
};
```

4. A trait may have no method signatures

```c
_Trait T {}; // ok
```

5. Extending member functions onto a trait type is not allowed

```c
void _Trait T::getArea(_Trait T* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}
```

### Implementing a Trait

**Syntax:**

```c
_Impl <_Trait> for <type>;
```

We can implement a trait for a type through the `_Impl` keyword. Intuitively, let's look at a simple example:

```c
_Trait T {
    void f(This* this);
};

struct S {};
void struct S::f(struct S* this);
void int::f(int* this);

_Impl _Trait T for int;
_Impl _Trait T for struct S;
```

**Rules:**

1. Before an `_Impl` statement, the definitions of `<_Trait>` and `<type>` must exist
2. Before an `_Impl` statement, `<type>` must have already declared and implemented all member functions in `<_Trait>`

```c
_Trait T {
    int f1(This* this);
    int f2(This* this);
};

struct S{};

int struct S::f1(struct S* this);
//_Impl _Trait T for struct S; // error: function 'f1' in '_Trait T' is not implemented for 'struct S
int struct S::f2(struct S* this);
//_Impl _Trait T for struct S; // error: function 'f1' in '_Trait T' is not implemented for 'struct S'
int struct S::f2(struct S* this){
    return 1;
};
int struct S::f1(struct S* this){
    return 1;
};

_Impl _Trait T for struct S; // ok, struct S has extended declarations for all member functions in the _Trait
```

3. The `<type>` type is not allowed to be a trait

```c
_Impl _Trait T for struct S; // ok, supports implementing a _Trait for an existing struct/union/enum or built-in type through _Impl
_Impl _Trait T for _Trait T; // error: function 'f1' in '_Trait T' is not implemented for '_Trait T'
```

4. Implementing a trait for a `typedef` type is supported

```c
typedef struct S S1;
_Impl _Trait T for S1;
```

### Variables of Trait Type

We can define variables of trait pointer type, and we can perform function calls and so on through the pointer variable. The specific usage is as follows:

```c
#include <stdio.h>

struct S {
    float num;
};

typedef _Trait Print {
    void print(This* this);
}P;
void struct S::print(struct S* this) {
    printf("This is a struct instance, valued %f\n", this->num);
}
void int::print(int* this) {
    printf("This is an int instance, valued %d\n", *this);
}

_Impl P for struct S;
_Impl P for int;

void test() {
    struct S s = { 0.0 };
    int a = 1;
    float b = 1.0;

    _Trait Print* p;
    p = &s; // ok, implicit conversion
    p->print(); // expected result: This is a struct instance, valued 0.000000
    p = &a;
    p->print(); // expected result: This is an int instance, valued 1
    p = &b; // error: expected a pointer type which has implemented '_Trait Print', found 'float'
}
```

**Rules:**

1. Applying `typedef` to a trait is supported

```c
typedef _Trait Print {
    ...
}P;
```

2. Only declaring variables of trait pointer type is allowed

```c
_Trait Print* p1; // ok
_Trait Print p2; // error: only _Trait pointer type is allowed to be declared
```

3. If `<type>` implements `<_Trait>`, then a pointer pointing to this `<type>` can be converted to a variable of `<_Trait>` type

```c
_Impl _Trait Print for struct S;

struct S s;
_Trait Print* t1 = &s; // implicit conversion
_Trait Print* t2 = (_Trait Print*)&s; // explicit conversion
```

4. A variable of trait pointer type can call the member methods in that trait through `->`

```c
struct S s;
_Trait Print* t = &s;
t->print();
```

5. A variable of trait pointer type may be assigned with `NULL`

```c
_Trait Print* p = NULL;
```

6. Multi-level pointers to a trait are allowed, but this type cannot directly call member functions. A second-level pointer of a struct is not allowed to be directly and implicitly converted to a second-level pointer of a trait

```c
_Trait Print* p;
p->print();
_Trait Print** q = &p; // ok
q->print(); // error: use of address-of-label extension outside of a function body
(*q)->print(); // ok
struct S s;
q = &&s; // error: tentative definition has type 'struct S' that is never completed
```

7. A variable of trait pointer type cannot be dereferenced

```c
_Trait Print *p;
int main(){
    *p; // error: use of undeclared identifier 'p'
}
```

8. A variable of trait pointer type can be modified with `const` / `volatile`

```c
const _Trait Print* p1;
volatile _Trait Print* p2;
```

9. Using a trait pointer type as a function parameter and return value type is supported

```c
_Trait Print* get(_Trait Print* t) {
    return t;
}
```

10. Comparing a trait pointer variable with `NULL` is supported (the comparison here includes only `==` and `!=`, the same below)

```c
_Trait Print* p = NULL;
if (p == NULL) {} // ok
if (p != NULL) {} // ok
```

11. Comparing a trait pointer variable with a non-trait pointer variable is supported

```c
struct S {
    float num;
};
typedef _Trait Print {
    void print(This* this);
}P;
void struct S::print(struct S* this) {
    printf("This is a struct instance, valued %f\n", this->num);
}
// the struct S type implements _Trait F<int>, but the int type does not
_Impl _Trait Print for struct S;
int main(){
    int a = 1;
    int *p1 = &a;
    struct S s;
    struct S *p2 = &s;
    _Trait Print* p;
    if (p == p1) {}; // warning: expected a pointer type which has implemented '_Trait Print', found 'int *'
    if (p == p2) {}; // ok
    if (p == a) {};  // error: expected a pointer type which has implemented '_Trait Print', found 'int'
}

```

12. Comparing a trait pointer variable with a trait pointer variable is supported

```c
_Trait Print* t1 = NULL;
_Trait Print* t2 = NULL;
_Trait G *g = NULL;
if (t1 == g) {} // warning: a warning is reported if the _Trait types differ
if (t1 == t2) {} // ok
```

### Type Conversion

A trait type conversion can only be performed between types that implement the corresponding trait, converting one type to another while preserving the original type's characteristics and methods.

**Rules:**

1. A variable of trait pointer type is not allowed to be forcibly converted to a non-pointer type

2. Forcibly converting a trait pointer type to a non-trait pointer type is supported, but implicit conversion is not supported

```c
int a = 0;
_Trait T *p = &a; // assume the int type implements _Trait T
int *q1 = p; // error: initializing 'int *' with an expression of incompatible type '_Trait T *'
int *q2 = (int*)p; // ok
struct S *q3 = (struct S*)p;
```

3. A trait type supports being forcibly converted to the `void *` type, but a `void *` pointer cannot be converted to a trait pointer type

```c
int a = 1;
_Trait T *p = &a; // assume the int type implements _Trait T
void * q = (void *)p; // ok: a _Trait type supports being forcibly converted to the `void *` type
_Trait T *p1 = (_Trait T *)q; // error: expected a pointer type which has implemented '_Trait T', found 'void'
_Trait T *p2 = q; // error: expected a pointer type which has implemented '_Trait T', found 'void'
```

## Generic Trait Grammar Rules

A generic trait refers to using generic type parameters in a trait, so that the trait's methods can apply to different types, avoiding repetitive code. The following code examples illustrate how to use generic traits.

### Defining a Generic Trait

**Syntax:**

```c
_Trait TraitName<T1,T2,...,Tn> {
  // Define the method signatures in the generic _Trait, which may use the generic types T1,T2,...,Tn
};
```

Similar to a trait definition, `TraitName` is the name of the generic trait, followed by a pair of angle brackets that can contain one or more generic parameters, and inside the braces some method signatures can be defined. Likewise, methods defined in a generic trait do not support default implementations; they must be given a concrete implementation by the type that implements the generic trait. Let's look at a simple example:

```c
_Trait F<T1, T2> {
    T1 doSomeThing1(This* this);
    T1 doSomeThing2(This* this, T2 param);
};
```

**Rules:**

1. A generic trait definition can only appear at the top level

```c
void test<T>() {
    _Trait F<T> {}; // error: _Trait is only allowed to be defined at top-level
}

struct MyStruct<T> {
    _Trait F<T> {}; // error: _Trait is only allowed to be defined at top-level
};
```

2. A generic trait requires that the first parameter of a function, and only the first parameter, be of type `This` pointer, named `this`; `This` refers to the concrete type that implements the trait

```c
_Trait F<T> {
    T doSomeThing1(This* this); // ok
    T doSomeThing2(This* a); // error: the first parameter of _Trait member function must be 'This* this'
    void doSomeThing3(T a, This* this); // error: the first parameter of _Trait member function must be 'This* this'
};
```

3. Only function declarations are allowed inside a generic trait

```c
_Trait F<T> {
    void doSomeThing1(This* this) { // error: expected member name or ';' after declaration specifiers
        ...
    }
};
```

4. A generic trait may have no method signatures

```c
_Trait F<T> {}; // ok
```

5. Extending member functions onto a generic trait type is not allowed

```c
void _Trait F<T>::getArea(_Trait F<T>* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}

void _Trait F<int>::getArea(_Trait F<int>* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}
```

### Implementing a Generic Trait

**Syntax:**

```c
_Impl <_Trait<SpecializationType>> for <type>;
```

We can implement `_Trait<int>`/`_Trait<float>`, etc. for existing `struct`/`union`/`enum` types and built-in types through the `_Impl` keyword. It should be noted that we currently only support performing Impl on instantiated trait types. Intuitively, let's look at a simple example:

```c
_Trait F<T> {
    T f(This* this);
};

struct S {};
int struct S::f(struct S* this);
int int::f(int* this);

_Impl _Trait F<int> for int;
_Impl _Trait F<int> for struct S;
```

**Rules:**

1. Before an `_Impl` statement, the definitions of the generic `<_Trait>` and `<type>` must exist
2. Only `_Impl` on instantiated trait types is supported

```c
_Impl _Trait F<T> for int; // error: use of undeclared identifier 'T'
```

3. Before an `_Impl` statement, `<type>` must have already declared all member functions in `<_Trait>`

```c
_Trait F<T> {
    T f1(This* this);
    T f2(This* this);
};

struct S{};

int struct S::f1(struct S* this){};
_Impl _Trait F<int> for struct S; // error: function 'f2' in '_Trait F<int>' is not implemented for 'struct S'
int struct S::f2(struct S* this){};
_Impl _Trait F<int> for struct S; // ok, struct S has extended declarations for all member functions in the _Trait
```

4. Implementing `_Trait<int>`/`_Trait<float>` for existing `struct`/`union`/`enum` types and built-in types through the `_Impl` keyword is supported, but the `struct`/`union` type cannot be generic

```c
struct S {};
struct G<T> {};
_Trait F<T> {};
_Impl _Trait F<int> for int; // ok
_Impl _Trait F<int> for struct S; // ok
_Impl _Trait F<int> for struct G<int>; // error: cannot _Impl _Trait for an instantiated type
```

5. The `<type>` type is not allowed to be a trait/generic trait

```c
_Impl _Trait F<int>  for _Trait S; // error: unexpected token for ImplTraitDecl
_Impl _Trait F<int>  for _Trait F<int>; // error: unexpected token for ImplTraitDecl
```

6. Implementing a generic trait for a `typedef` type is supported

```c
typedef struct S S1;
_Impl _Trait F<int> for S1;
```

### Variables of Generic Trait Type

We can define variables of the pointer type that results from instantiating a generic trait, and we can perform function calls and so on through the pointer variable. The specific usage is as follows:

```c
#include <stdio.h>

struct S {};

_Trait F<T> {
    T foo(This* this);
};
int struct S::foo(struct S* this) {
    return 1;
}
int int::foo(int* this) {
    return 0;
}

_Impl _Trait F<int> for struct S;
_Impl _Trait F<int> for int;

void test() {
    struct S s;
    int a = 1;
    float b = 1.0;

    _Trait F<int>* p;
    p = &s; // ok, implicit conversion
    p->foo(); // return 1
    p = &a;
    p->foo(); // return 0
    p = &b; // error: expected a pointer type which has implemented '_Trait F<int>', found 'float'
}
```

**Rules:**

1. Only declaring variables of the pointer type that results from instantiating a generic trait is allowed

```c
_Trait F<int>* p1; // ok
_Trait F<int> p2; // error: only _Trait pointer type is allowed to be declared
```

2. If `<type>` implements `<_Trait>`, then a pointer pointing to this `<type>` can be converted to a variable of `<_Trait>` type

```c
_Impl _Trait F<int> for struct S;

struct S s;
_Trait F<int>* t1 = &s; // implicit conversion
_Trait F<int>* t2 = (_Trait F<int>*)&s; // explicit conversion
```

3. A variable of the pointer type that results from instantiating a generic trait can call the member functions in that generic trait through `->`

```c
struct S s;
_Trait F<int>* t = &s;
t->foo();
```

4. A variable of the pointer type that results from instantiating a generic trait may be assigned with `NULL`

```c
_Trait F<int>* p = NULL;
```

5. A variable of the pointer type that results from instantiating a generic trait cannot be dereferenced

```c
_Trait F<int> *p;
*p; // error: use of undeclared identifier 'p'
```

6. Multi-level pointers to a generic trait are allowed, but this type cannot directly call member functions. A second-level pointer of a struct is not allowed to be directly and implicitly converted to a second-level pointer of a generic trait

```c
_Trait F<int>* p;
p->foo();
_Trait F<int>** q = &p; // ok
q->foo(); // error: use of address-of-label extension outside of a function body
(*q)->foo(); // ok
struct S s;
q = &&s; // error: tentative definition has type 'struct S' that is never completed
```

7. A variable of the pointer type that results from instantiating a generic trait can be modified with `const` / `volatile`

```c
const _Trait F<int>* p1;
volatile _Trait F<int>* p2;
```

8. Using the pointer type that results from instantiating a generic trait as a function parameter and return value type is supported

```c
_Trait F<int>* get(_Trait F<int>* t) {
    return t;
}
```

9. Comparing a pointer variable that results from instantiating a generic trait with `NULL` is supported (the comparison here includes only `==` and `!=`, the same below)

```c
_Trait F<int>* p = NULL;
p == NULL; // ok
p != NULL; // ok
```

10. Comparing a pointer variable that results from instantiating a generic trait with a non-trait pointer variable is supported

```c
int a = 1;
int *p1 = &a;
struct S s;
struct S *p2 = &s;
_Trait F<int> *t = NULL;
// assume the struct S type implements _Trait F<int>, but the int type does not
if (t == p1) {}; // warning
if (t == p2) {}; // ok
if (t == a) {}; // error: expected a pointer type which has implemented '_Trait F<int>', found 'int'
```

11. Comparing a pointer variable with a trait pointer variable is supported

```c
_Trait F<int> *t1 = NULL;
_Trait F<int> *t2 = NULL;
_Trait F<float> *t3 = NULL;
if (t1 == t2) {} // ok
if (t1 == t3) {} // warning: comparison of distinct pointer types ('_Trait F<int> *' and '_Trait F<float> *')
```

### Type Conversion

A generic trait type conversion can only be performed between types that implement the corresponding generic trait, converting one type to another while preserving the original type's characteristics and methods.

**Rules:**

1. A pointer variable that results from instantiating a generic trait is not allowed to be forcibly converted to a non-pointer type

```c
struct S s;
_Trait F<int> *p = &s; // assume the struct S type implements _Trait F<int>
(struct S)p; // error: used type 'struct S' where arithmetic or pointer type is required
```

2. Forcibly converting a generic trait pointer type to a non-trait pointer type is supported, but implicit conversion is not supported

```c
struct S s;
_Trait F<int> *p = &s; // assume the struct S type implements _Trait F<int>
struct S *q1 = p; // error: initializing 'struct S *' with an expression of incompatible type '_Trait F<int> *'
struct S *q2 = (struct S*)p; // ok
int *q3 = (int*)p; // ok
```

3. A generic trait type supports being forcibly converted to the `void *` type, but a `void *` pointer cannot be converted to a generic trait pointer type

```c
struct S s;
_Trait F<int> *p = &s;
void * q = (void *)p; // ok: a generic _Trait type supports being forcibly converted to the `void *` type
_Trait F<int> *p1 = (_Trait F<int> *)q; // // error: expected a pointer type which has implemented '_Trait F<int>'', found 'void'
_Trait F<int> *p2 = q; // // error: expected a pointer type which has implemented '_Trait F<int>'', found 'void'
```

## Application

```c
#include <stdio.h>

// Define a _Trait
_Trait Shape {
    int getArea(This* this);
    int getSideLen(This* this);
};

struct Square {
    int side;
};

struct Rectangle {
    int width;
    int height;
};

int struct Square::getArea(struct Square* this) {
    int area = this->side * this->side;
    printf("the area of this square is %d.\n", area);
    return area;
}

int struct Square::getSideLen(struct Square* this) {
    int length = this->side * 4;
    printf("the side length of this square is %d.\n", length);
    return length;
}

int struct Rectangle::getArea(struct Rectangle* this) {
    int area = this->width * this->height;
    printf("the area of this rectangle is %d.\n", area);
    return area;
}

int struct Rectangle::getSideLen(struct Rectangle* this) {
    int length = (this->width + this->height) * 2;
    printf("the side length of this rectangle is %d.\n", length);
    return length;
}

// Implement the _Trait for the struct types
_Impl _Trait Shape for struct Square;
_Impl _Trait Shape for struct Rectangle;

// _Trait pointer type as a function parameter and return value type
_Trait Shape* get(_Trait Shape* s) {
    return s;
}

void test() {
    struct Square s = {.side = 5};
    struct Rectangle r = {.width = 2, .height = 3};
    _Trait Shape* shape = &s;
    // A _Trait pointer variable calls a method
    shape->getArea(); // the area of this square is 25.
    // Explicit conversion
    ((_Trait Shape*)&s)->getSideLen(); // the side length of this square is 20.
    // Assign the pointer to a variable of _Trait pointer type
    shape = &r;
    shape->getArea(); // the area of this rectangle is 6.
    // Implicit conversion, converting struct Rectangle* to _Trait Shape*
    _Trait Shape* shape2 = get(&r);
    shape2->getSideLen(); // the side length of this rectangle is 10
}
```
