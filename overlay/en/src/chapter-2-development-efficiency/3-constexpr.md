# Compile-Time Computation

## constexpr

### Overview

In C, although the `const` keyword can indicate that a variable is read-only, it does **not guarantee that the value is determined at compile time**. To clearly express "this value must be computable at compile time", we introduced the `constexpr` keyword. It can be used both to define **compile-time constants** and to define **functions that can be executed at the compilation stage**, thereby supporting stronger type checking and generic capabilities.

Types that count as "compile-time computable":
bool, char (signed char, unsigned char), integer types (including int as well as the int type modified by short/signed/unsigned/long/long long, etc., but excluding enum types), and aliases of these types.

The "compile-time computation" contexts — that is, the scenarios where a constexpr-qualified variable or function can be used as a constant — are defined as:

- It can be used in static_assert, where the first condition argument belongs to a "compile-time computation" context
- It can be used to define a fixed-length array, where the array length belongs to a "compile-time computation" context
- It can be used to initialize other constexpr constants
- It can be used as an argument to a constant generic

The following example illustrates what a "compile-time computation" context is.

```c
void bar<int N>(){}

constexpr int foo() {
    return 5;
}

int main() {
    constexpr int a = 5;

    // can be used as an argument to a constant generic
    bar<a>();
    bar<foo()>();

    // can be used to define a fixed-length array
    int arr1[a] = {0};
    int arr2[foo()] = {0};

    // can be used to initialize other constexpr constants
    constexpr int b = a;
    constexpr int c = foo();

    // can be used in static_assert
    _Static_assert(a == 5, "fail");
    _Static_assert(foo() == 5, "fail");

    return 0;
}
```

### Usage Rules

#### constexpr Qualifying Variables

1. constexpr can qualify a constant definition, and it must be initialized at the point of definition, otherwise an error is reported

```c
constexpr int a = 5;
constexpr int b; //error: constexpr variable 'b' must be initialized by a constant expression
```

2. A constexpr-qualified constant cannot be modified after its definition

```c
constexpr int a = 5;
a = 10; //error: redefinition of 'a' with a different type: 'int' vs 'const int'
```

3. The type of a constexpr-qualified constant can only be one of the "compile-time computable" types described above

```c
constexpr float a = 5.0;//error: BSC constexpr variable does not support type 'const float'
```

4. The initialization expression of a constexpr-qualified constant must be evaluable at compile time, otherwise an error is reported. A constant expression that is evaluable at compile time can be:

- A literal
- A constexpr-qualified constant
- A sizeof or _Alignof expression
- A call to a constexpr function with compile-time-evaluable constant expressions as arguments
- A constant expression composed of the following operators is also a constant expression: +,-,*,/,%,>,<,==,!=,<=,>=,&,|,^,~,!,&&,||,<<,>>,?:

For example:

```c
// Scenario 1
int a = 10;
constexpr int b = a;//error
// Scenario 2
constexpr int a = 10;
constexpr int b = a;
// Scenario 3
constexpr int a = sizeof(int);
constexpr int b = sizeof(int);
// Scenario 4
constexpr int foo(int a) {
    return 5;
};
constexpr int a = 10;
constexpr int b = foo(a);
// Scenario 5
constexpr int b = 1 == 1.0;
```

5. A function pointer can be qualified with constexpr, and a function pointer can point to a constexpr function
6. constexpr can qualify a pointer variable, but it can only point to a global variable or a static variable

#### constexpr Qualifying Functions

1. constexpr can qualify a function declaration or definition, but you must ensure that both the declaration and the definition either both have the constexpr qualifier or both do not, otherwise it causes a compilation error

```c
constexpr int foo();
constexpr int foo() {
    return 5;
}
```

2. For a constexpr-qualified function, the parameter and return types can only be the "compile-time computable" types described above

```c
constexpr void foo(); //error: BSC constexpr function does not support type 'void'
```

3. constexpr can qualify a generic function

```c
constexpr int foo<T>();
```

4. All statements inside the body of a constexpr function are evaluable at compile time

- Defining static variables is not allowed inside the body of a constexpr function
- Calling non-constexpr functions is not allowed inside the body of a constexpr function
- Accessing external non-constexpr variables is not allowed inside the body of a constexpr function
- Inline assembly is not allowed inside the body of a constexpr function
- Local variables that are not qualified with constexpr are allowed inside the body of a constexpr function, but these variables can only be of the "compile-time computable" types

5. In a non-"compile-time computation" context, a constexpr-qualified function can be used as an ordinary function: the arguments do not need to be constants, and the return value does not need to be a constant. In a "compile-time computation" context, both the arguments and the return value are required to be constant expressions, otherwise an error is reported

```c
constexpr int foo(int a) {
    return 5;
};

int a = 10;
constexpr int b = foo(a);// error: constexpr variable 'b' must be initialized by a constant expression
int c = foo(a);//ok, the foo function is in a non-"compile-time computation" context
int main(){
    int a = 10;
    constexpr int b = foo(a);// error: constexpr variable 'b' must be initialized by a constant expression
    int c = foo(a);//ok, the foo function is in a non-"compile-time computation" context
    return 0;
}
```

6. constexpr can qualify member functions, including ordinary member functions and static member functions

```c
// Ordinary member function; the parameter This* this is not a compile-time computable type
constexpr int int::foo1(This* this) { //error
    return 5;
}

// Static member function
constexpr int int::foo2() {
    return 5;
}

int main() {
    constexpr int c = int::foo2();//ok, evaluable at compile time
    return 0;
}
```

6. constexpr is not allowed to qualify _Async functions
7. constexpr does not support variadic arguments
8. Function formal parameters cannot be qualified with constexpr

```c
int foo1(constexpr int a) { //error
    return 5;
}

constexpr int foo2(constexpr int a) { //error
    return 5;
}
```

## type trait

A type trait can be regarded as a constexpr function that computes its return value at compile time.
The BSC standard library provides a series of type trait generic functions; to use them you need to import the header file bsc_type_traits.hbs

The currently implemented type trait functions are:

```c
// Determine the category of a type
constexpr bool is_integral<T>();
constexpr bool is_floating_point<T>();
constexpr bool is_pointer<T>();
constexpr bool is_function<T>();
constexpr bool is_array<T>();
constexpr bool is_struct<T>();
constexpr bool is_union<T>();
constexpr bool is_enum<T>();
constexpr bool is_void<T>();
// Determine the properties of a type
constexpr bool is_signed<T>();
constexpr bool is_unsigned<T>();
constexpr bool is_const<T>();
constexpr bool is_volatile<T>();
constexpr bool is_move_semantic<T>();
constexpr bool is_owned_pointer<T>();
constexpr bool is_owned_struct<T>();
constexpr bool is_borrow<T>();
constexpr bool is_immut_borrow<T>();
constexpr bool is_mut_borrow<T>();
constexpr bool is_trivial_data<T>();
constexpr size_t rank<T>();
constexpr size_t extent<T, size_t N>();
// Determine the relationship between types
constexpr bool is_same<T1, T2>();
constexpr bool is_convertible<From, To>();
```

Explanations and examples for some of them:

```c
constexpr bool is_move_semantic<T>(); // determine whether type T is a move-semantic type
// We consider types with move semantics to include:
//   1. owned pointer types
//   2. _Owned struct types
//   3. struct types containing fields with move semantics
is_move_semantic<int *_Owned>() == true;
_Owned struct S {};
is_move_semantic<S>() == true;
struct S1 { int a; };
struct S2 { int *_Owned p; };
is_move_semantic<struct S1>() == false;
is_move_semantic<struct S2>() == true;
// The move semantics of an incomplete struct are meaningless; this reports an error
struct I;
is_move_semantic<struct I>(); // error: incomplete type 'struct I' used in type trait expression

constexpr bool is_trivial_data<T>(); // determine whether type T is a trivial data type
// We consider a trivial data type to be a type that does not contain the following:
//   1. raw pointers, _Owned pointers, _Borrow pointers
//   2. _Owned structs
is_trivial_data<int *_Owned>() == false;
_Owned struct S {};
is_trivial_data<S>() == false;
struct S1 { int a; };
struct S2 { int *_Owned p; };
struct S3 { int *_Borrow p; };
struct S4 { int * p; };
is_trivial_data<struct S1>() == true;
is_trivial_data<struct S2>() == false;
is_trivial_data<struct S3>() == false;
is_trivial_data<struct S4>() == false;
// Whether an incomplete struct is trivial data is meaningless; this reports an error
struct I;
is_trivial_data<struct I>(); // error: incomplete type 'struct I' used in type trait expression

constexpr size_t rank<T>(); // can be used to compute the number of dimensions of an array, e.g.
rank<int>() == 0;
rank<int[5]>() == 1;
rank<int[5][5]>() == 2;

constexpr size_t extent<T, size_t N>();// can be used to compute the number of elements in the Nth dimension of an array, e.g.
extent<int[3],0>() == 3;
extent<int[3],1>() == 0;
extent<int[3][4],0>() == 3;
extent<int[3][4],1>() == 4;
extent<int[3][4],2>() == 0;

constexpr bool is_same<T1, T2>();// determine whether types T1 and T2 are the same, ignoring type aliases

constexpr bool is_convertible<From, To>();// determine whether type From can be implicitly converted to type To, e.g.
is_convertible<int, float>() == true;
is_convertible<int, const int>() == true;
is_convertible<int, volatile int>() == true;
is_convertible<int, signed>() == true;
is_convertible<int, void>() == false;
is_convertible<int, int*>() == false;
is_convertible<int, void*>() == false;
is_convertible<struct S, struct G>() == false;
```

Using them is just like using ordinary generic functions

```c
#include<stdio.h>
#include<bsc_type_traits.hbs>

int main() {
    printf("%d\n",is_integral<int>()); //expected result: 1
    printf("%d\n",is_integral<float>()); //expected result: 0
    return 0;
}
```

type trait functions can be used in generic functions and in the member functions of generic structs

```c
#include<stdio.h>
#include<bsc_type_traits.hbs>

struct S<T> {};
void struct S<T>::foo(struct S<T>* this) {
    if (is_integral<T>()) {
        printf("integral\n");
    } else {
        printf("not integral\n");
    }
}

void bar<T>() {
    if (is_integral<T>()) {
        printf("integral\n");
    } else {
        printf("not integral\n");
    }
}

int main() {
    struct S<int> s1;
    struct S<float> s2;
    s1.foo(); //print "integral"
    s2.foo(); //print "not integral"
    bar<int>();  //print "integral"
    bar<float>(); //print "not integral"
    return 0;
}
```

type trait functions can also be used in static assertions

```c
#include<bsc_type_traits.hbs>

int main() {
    _Static_assert(is_integral<int>() == true, "fail");
    _Static_assert(is_integral<float>() == false, "fail");
    return 0;
}
```

## constexpr if

### Overview

A statement that begins with if constexpr is called a constexpr if statement. It allows the user to use a constexpr expression as the condition of an if statement, guaranteeing that the condition is a compile-time-computed constant. This enables the compiler to make the branch decision at compile time and perform dead-code elimination on the false branch, reducing runtime overhead.

The difference between a constexpr if statement and an ordinary if statement is that the cond conditional expression is qualified with the constexpr keyword, that is:

```c
if constexpr (<cond>) {
    <then-statement>
} else {
    <else-statement>
}
```

Let's get to know constexpr if using factorial computation as an example. First, here is a version that computes the factorial without using constexpr if:

```c
int factorial(int n) {
    return (n == 1) ? 1 : n * factorial(n - 1);
}

int main() {
    printf("%d", factorial(5));
    return 0;
}
```

Since BSC does not support generic specialization, the only way to implement the factorial is with an ordinary function, evaluated at runtime. Using constexpr if, we can implement functionality similar to generic specialization and compute the factorial result at compile time.

```c
constexpr int factorial<int N>() {
    if constexpr (N == 1) {
        return 1;
    } else {
        return N * factorial<N - 1>();
    }
}

int main() {
    _Static_assert(factorial<5>() == 120, "fail");
    return 0;
}
```

### Usage

The cond expression in a constexpr if statement must satisfy the following constraints when used:

1. The cond expression must be a constant expression that is evaluable at compile time;
2. The type of the cond expression must be a type that is implicitly convertible to bool, including bool, integer types, and char.
For example:

```c
int foo1() {
    return 5;
}

constexpr int foo2(int a) {
    return a;
}

int main() {
    int a = 1;
    if constexpr(5.0) { //error: BSC constexpr if condition expression does not support type 'double'
        a = 6;
    }
    if constexpr(a) {  //error: constexpr if condition is not a constant expression
        a = 6;
    }
    if constexpr(foo1()) {  //error: constexpr if condition is not a constant expression
        a = 6;
    }
    if constexpr(foo2(a)) {  //error: constexpr if condition is not a constant expression
        a = 6;
    }
    return 0;
}
```

In a constexpr if statement, because the conditional expression is a constant expression computed at compile time, the branch that evaluates to false is defined as a "discarded statement" and is eliminated as dead code at compile time.
For example:

```c
if constexpr (<cond>) {  // if the value of <cond> is true
    <true-statement>
} else {
    <false-statement>  // then the statements inside the else branch become discarded statements and are eliminated as dead code at compile time
}
```

For discarded statements, the following rules apply:

1. In a non-generic context, the discarded statement still needs to undergo complete syntactic and semantic checking.
2. In a generic context, the discarded statement is not instantiated, and therefore does not undergo the semantic checking that follows instantiation.
For example:

```c
#include <stdio.h>
#include <bsc_type_traits.hbs>

void foo<T>(T a) {
    if constexpr (is_pointer<T>()) {
    // If T is not a pointer, during instantiation the block below is treated as dead code, is not instantiated, and therefore does not undergo the semantic checking that follows instantiation
        printf("T is pointer\n");
        void* p = (void*) a;
    } else {
        printf("T is a generic case\n");
    }
}

int main() {
    int b = 5;
    foo<int>(5);
    foo<int*>(&b);
    return 0;
}
```
