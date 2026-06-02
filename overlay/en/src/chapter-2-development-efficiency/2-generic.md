# Generics

## Overview

Generics are a programming technique that allows types (such as basic types like integers and strings, or user-defined types) to be passed as parameters to functions or structs. This enables code reuse, improves flexibility, and increases development efficiency while preserving type safety.

BiSheng C generics are a compile-time generic mechanism: you can define a generic function or class, and then generate different instances based on different type arguments.

Currently, BiSheng C already supports **generic functions**, **generic structs**, and **generic type aliases**.

### Motivation

Generics are implemented to improve the **efficiency** and **reusability** of code. Their advantages include:

- Avoiding code redundancy
- Improving code readability and maintainability
- Enabling type safety and compile-time checking
- ...

Generic programming enables developers to write generic algorithms that apply to arbitrary data types, without having to implement the same logic separately for different types (such as integers, strings, or custom types).

Through generics, you can define a generic template for a function or struct, allowing certain data members of a class, or the parameters and return values of member functions, to take on arbitrary types.

### Example

Take the following code as an example:

```c
int sum_int(int a, int b) {
    int c = a + b;
    return c;
}

float sum_float(float a, float b) {
    float c = a + b;
    return c;
}

int main() {
    int sum1 = sum_int(1, 2);
    float sum2 = sum_float(1.2, 2.5);
}
```

As you can see, for the sum method, if you want to handle the two cases where the return value is **float** and **int** respectively, ordinary C requires defining the same logic twice.

But with BiSheng C generic syntax, you only need to define it once and can then reuse it during instantiation, as follows:

```c
#include<stdio.h>
T sum_t<T>(T a, T b) {
    T c = a + b;
    return c;
}

int main() {
    int sum1 = sum_t<int>(1, 2);
    float sum2 = sum_t<float>(1.2, 2.5);
    printf("%d\n",sum1); // expected result: 3
    printf("%f\n",sum2); // expected result: 3.700000
    return 0;
}
```

As you can see, the introduction of generics significantly reduces the amount of code for scenarios where the same algorithm is declared.

## Grammar Rules

For BiSheng C generics, we designed the following grammar rules:

- When declaring a generic function, struct, or type alias, the generic parameter list must be wrapped in angle brackets '<>'. The types inside the angle brackets are generally identifiers such as 'T1', 'T2', and so on.

- During instantiation, you can either pass concrete types inside the angle brackets (such as int, float, struct S, etc.), or omit the angle brackets entirely, in which case the compiler performs implicit type deduction based on the arguments actually passed. In addition, if an existing type is typedef'd to another alias, that alias can likewise be used as a generic argument during instantiation.

- At the same time, BiSheng C generic functions, generic structs, and generic type aliases also support using **constant parameters** as generic formal parameters.

Below we explain in detail across five parts.

### Generic Functions

For generic functions, our grammar rule design is mainly reflected in two aspects:

1. When declaring, unlike an ordinary function declaration, we need to add a pair of angle brackets '<>' between the **function name** and the **function parameters**, and write the **generic formal parameters** of the generic function inside the angle brackets. The formal parameters may be any legal name (here "legal" means a name that will not cause a semantic conflict).

   At the same time, the type of the function return value can be an ordinary builtin type, a struct the user has already defined, or one of the generic formal parameters (such as T).

2. When instantiating, the type can either be specified explicitly inside the angle brackets, or the angle brackets and their contents can be omitted and deduced implicitly by the compiler:

   1. When specifying the type explicitly, unlike an ordinary function call, you likewise need to add a pair of angle brackets '<>' between the called **function name** and the **function parameters**, with no space in between; then pass the **generic arguments** inside the angle brackets. Here the arguments can be builtin types or structs the user has already defined.
   2. When deducing implicitly, the syntax is the same as an ordinary function call. The BiSheng C compiler automatically deduces the type of the **generic arguments** based on the argument types passed in the function call. However, for the sake of code readability and similar reasons, specifying the type explicitly is recommended.

Below are some usage examples:

```c
typedef long int LT;

// Generic function
T max<T>(T a, T b) { // the leading 'T' is the function return type, while the 'T' in '<>' is the generic formal parameter of the generic function 'max'
    T Max = a > b ? a : b;
    return Max;
}

int main() {
    int a = 3;
    int b = 5;
    int c = 4;

    // Generic function instantiation
    int max1 = max<int>(a, b); // explicitly specify the generic argument type
    int max3 = max(a, c); // implicit deduction; the compiler automatically deduces 'T' as the int type

    return 0;
}
```

### Generic Structs

For generic structs, our grammar rule design is likewise reflected in two aspects:

1. When declaring, unlike an ordinary struct declaration, we need to add a pair of angle brackets '<>' after the **struct name**, and write the **generic formal parameters** of the generic struct inside the angle brackets.
2. When instantiating, generic structs only support explicit type specification. That is, when specifying the type explicitly, unlike ordinary struct construction, you likewise need to add a pair of angle brackets '<>' after the constructed **struct name**, with no space in between; then pass the **generic arguments** inside the angle brackets. Here the arguments can be builtin types or structs the user has already defined.
3. When using a generic struct type — such as when declaring a variable of the generic struct type, declaring a member of the generic struct type, declaring a parameter, declaring a return value, or extending member functions — the `struct` keyword can be omitted. That is, when using a type of the form `struct S<T>`, it can be abbreviated as `S<T>`, but it cannot be omitted when declaring the type.

Below are some usage examples:

```c
typedef long int LT;

// Generic struct
struct S<T, B>{
    T a;
    B b;
};

// Generic union
union MyUnion<T1, T2> {
    T1 u1;
    T2 u2;
};

// Generic function 'foo_union' whose return type is a 'generic union'
union MyUnion<T1, T2> foo_union<T1, T2>(union MyUnion<T1, T2> *this) {
    return *this;
}

int main() {
    // Generic struct instantiation
    struct S<int, LT> s1 = {.a = 42, .b = 5}; // use the typedef'd type as a generic argument

    // Generic union instantiation
    union MyUnion<int, float> p;
    foo_union(&p); // implicit type deduction for the generic function foo_union

    return 0;
}
```

### Generic Member Functions

On top of supporting the definition of generic struct and generic union types, you can also extend them with ordinary member functions.

The following is a usage example that includes declaring a generic member function, defining a generic member function, and instantiating and calling a generic member function:

```c
struct MyStruct<T> {
    T res;
};

union MyUnion<T> {
    T res;
};

T struct MyStruct<T>::foo(struct MyStruct<T>* this, T a) { // define a generic member function
    this->res = this->res + a;
    return this->res;
}

T union MyUnion<T>::foo(union MyUnion<T>* this, T a) {
    this->res = this->res + a;
    return this->res;
}

int main() {
    struct MyStruct<int> s = { .res = 1 };
    union MyUnion<int> u = { .res = 5 };
    int res1 = s.foo(2); // call a generic member function
    int res2 = u.foo(6);
    return 0;
}
```

Regarding generic member functions, there are a few more points to note:

1. A generic member function can be a static function, meaning the first parameter does not have to be `this`;
2. Overloading of generic member functions is not allowed, which is the same as the rule for member functions;

```c
struct MyStruct<T> {
    T val;
};

T static struct MyStruct<T>::static_add(T a, T b) {
    return a + b;
}


T struct MyStruct<T>::static_add(T a);  // expected result: error: conflicting types for 'static_add'


int main() {
    int sum = MyStruct<int>::static_add(10, 20);   // static generic member function
    return 0;
}
```

3. The return type of a generic member function can be a generic struct or generic union type.

```c
struct MyStruct<T> {
    T val;
};
    
struct MyStruct<T> struct MyStruct<T>::make_struct(T x) {
    struct MyStruct<T> result = { .val = x };
    return result;
}


int main() {
    MyStruct<int> s = MyStruct<int>::make_struct(42);
    return 0;
}
```

### Constant Generics

Beyond the implementation of basic generics, BiSheng C also introduces constant generics. Specifically, constant generics are a feature that allows program items to be generic over constant values. In other words, constants can be passed as generic parameters into generic variables, and the code is specialized according to the constant parameters, thereby ensuring zero overhead and allowing them to be used directly as constants in the code.

For example, in BiSheng C you can define a generic struct in which one generic parameter is a **constant generic parameter** that can be used to represent the size of an array defined inside the struct.

In this way, by passing different constant values during instantiation, you can generate multiple array objects of different sizes, for example:

```c
struct Array<T, int N> {
    T data[N];
};

int main() {
    struct Array<int, 5> arr1;
    struct Array<int, 10> arr2;
    return 0;
}
```

As above, here '10' and '5' are the arguments for the constant generic parameter 'int N'; they determine the size of the array.

Currently, the rules for generic constants are as follows:

- The formal parameters of constant generics only support "compile-time computable types"; currently only integer types are supported.
- The arguments of constant generics can only be "compile-time computable" constant expressions.
- Syntactically, if it is only an int literal or a constexpr constant, then parentheses are not required; all other constant expressions must be wrapped in parentheses.

Currently, BiSheng C's implementation of constant generics is limited to integer "integer constants":

- For declarations, the formal parameter list only accepts int and its modifiers long, short, unsigned, signed, and various combinations of the above keywords. It also supports typedef'ing the above keywords into other aliases and then using them as formal parameters.
- For instantiation, the generic argument list currently supports IntegerLiteral (i.e. constants such as 1, 2), and also supports constexpr-qualified variables and constant expressions.

Below are some usage examples:

```c
#include <stdio.h>

typedef long long int LLInt;

// Generic function using a generic constant
int print_dataSize<T, int B>()
{
    T data[B];
    printf("the size of data is %d\n", B);
    return B;
}

// Generic function using a typedef alias as a generic constant
void print_const<LLInt B>() {
    printf("the const is %d\n", B);
}

// Generic struct using a generic constant
struct Array_1<T, int N>
{
    T data[N];
};

// Generic struct using a typedef alias as a generic constant
struct Array_2<LLInt B, int C, T>
{
    LLInt data1[B];
    int data2[C];
    T a;
};

int main() {
    int a1 = print_dataSize<int, 5>();
    print_const<20>();

    struct Array_1<int, 5> arr1;
    struct Array_2<5, 6, int> arr2 = {.a = 1};

    return 0;
}
```

### Generic Type Aliases

Standard C already has a type alias feature, with the syntax:

```c
typedef OldType NewType;
```

To make type aliases work together with generics, BSC introduces a type alias syntax that differs from standard C:

```c
typedef NewType = OldType ;
```

Adding generic parameters forms a generic type alias, for example:

```c
typedef MyPointerType<T> = T*;
```

Using generic type aliases can simplify how types are written, and can also give types more descriptive names, making code easier to read and understand. For example:

We can use a HashMap to record the grades of students in a certain grade level for each subject, where Key represents the student's ID and Value represents the student's grades in each subject. Depending on the actual situation, a student's ID can be represented by types such as int or string, grades can be represented by types such as int or float, and the number of subjects may also change. Using generic type aliases helps us customize according to actual needs:

```C
// Helper class and member methods
struct HashMap<K, V> {
   // implementation omitted
};
void struct HashMap<K, V>::insert(This* this, K key, V value) {
   // implementation omitted
}
struct Array<T, int N> {
    T a[N];
};
// Use a generic type alias to define a special HashMap type whose key is of type T1 and whose value is an array of length N with element type T2:
typedef GenericGrade<T1, T2, int N> = struct HashMap<T1, struct Array<T2, N>>;
// Continue using type aliases, customizing for different grade levels:
typedef Grade1 = GenericGrade<int, int, 3>;  // first-grade students have an int ID, int grades, and 3 subjects
typedef Grade3 = GenericGrade<int, float, 4>;// third-grade students have an int ID, float grades, and 4 subjects
typedef Grade6 = GenericGrade<int, float, 5>;// sixth-grade students have an int ID, float grades, and 5 subjects

int main() {
    Grade1 grade1;
    grade1.insert(10, {80, 90, 95});
    grade1.insert(11, {80, 95, 90});
    Grade3 grade3;
    grade3.insert(12, {90.0, 95.5, 90.0, 85.0});
    grade3.insert(13, {80.0, 90.0, 95.5, 90.0});
    Grade6 grade6;
    grade6.insert(15, {80.0, 90.0, 95.5, 90.0, 85.0});
    grade6.insert(16, {80.0, 90.0, 95.5, 90.0, 85.0});
    return 0;
}
```

For type aliases, there are the following rules:

1. Extending member functions for a generic type alias is not allowed.

```C
struct S<T> {};
typedef MyS<T> = struct S<T>;

void MyS<T>::foo(This* this) {} //error: extended type of a BSC member function cannot be a generic typealias
```

2. Defining a type alias inside a struct type is not allowed.

```C
struct S<T> {
    typedef type = T;    //error: type name does not allow storage class to be specified
    typedef Int64 = int; //error: expected ';' at end of declaration list
};
```

3. Defining a generic type alias inside a function body is not allowed, but defining an ordinary type alias is allowed.

```C
void foo<T>() {
    typedef type = T;
    typedef Int64 = int;

    typedef MyType1<T> = T;   //error: no template named 'MyType1'
    typedef MyType2<T1> = T1; //error: BSC generic typealias cannot be declared within a function
}
```

Below are some usage examples:

```C
struct S<T> {};
struct V<T1, T2> {
    T1 a;
    T2 b;
};

// Ordinary type aliases
typedef Int64 = long int;   // equivalent to typedef long int Int64;
typedef MyS = struct S<int>;// equivalent to typedef struct S<int> MyS;

// Generic type aliases
typedef MyPointerType<T> = T*;
typedef Array_3<T> = T[3];
typedef Array_N<T, int N> = T[N]; // supports constant generic parameters

typedef MyS_T_T<T> = struct V<T, T>;
typedef MyS_T_int<T> = struct V<T, int>;

int main() {
    Int64 a = 5;  // equivalent to int a = 5;
    MyS s;        // equivalent to struct S<int> s;
    int b = 2;
    MyPointerType<int> c = &b;  // equivalent to int* c = &b;
    Array_3<int> d = {1,2,3};   // equivalent to int d[3] = {1,2,3};
    Array_N<int, 3> e= {1,2,3}; // equivalent to int e[3] = {1,2,3};
    MyS_T_T<int> s2;
    MyS_T_int<int> s3;
    return 0;
}
```

#### conditional Generic Type Alias

The BSC standard library bsc_conditional.hbs provides the conditional generic type alias, which enables type-level "branching logic":

```c
// bsc_conditional.hbs
typedef conditional<int C, T, F> = __conditional(int C, T, F);
```

When C is non-zero, the conditional type alias refers to type T; otherwise it refers to type F. The conditional expression must be a constant expression that can be evaluated at compile time:

```c
#include<bsc_conditional.hbs>  // using conditional requires importing the header file
int main() {
    conditional<1, int, double> a = 1;   // equivalent to int a = 1;
    conditional<0, int, double> b = 1.0; // equivalent to double b = 1.0;
    return 0;
}
```

Using conditional not only simplifies how code is written, but also lets you choose different types at compile time based on a condition, avoiding runtime conditional branching and improving code efficiency. The following is a use case for choosing a function's return type:

Define a generic function whose return type depends on the generic parameter T: if T is a pointer type, the return type is still T; otherwise, return the pointer type of T.

In C++ this needs to be implemented with generic specialization and concepts:

```cpp
// When T is a pointer type, std::is_pointer_v<T> == true, this version is matched:
template<typename T> requires std::is_pointer_v<T>
T foo() { ... }

// When T is not a pointer type, this version is matched:
template<typename T>
T* foo() { ... }
```

In BSC, we can implement this functionality very conveniently with conditional, which is easier to use than C++:

```c
typedef PointerType<T> = conditional<is_pointer<T>(), T, T*>;

PointerType<T> foo<T>() { ... }
```
