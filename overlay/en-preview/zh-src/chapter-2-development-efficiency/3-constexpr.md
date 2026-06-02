# 常量计算

## constexpr

### 概述

在 C 语言中，虽然有 `const` 关键字可以表示变量只读，但它并**不能保证值在编译期间就确定**。为了明确表示“这个值必须在编译时就能计算出来”，我们引入了 `constexpr` 关键字。它既可以用于定义**编译时常量**，也可以用于定义**在编译阶段就能执行的函数**，从而支持更强的类型检查和泛型能力。

定义“编译时计算”的类型：
bool,char(signed char, unsigned char), 整数类型（包括 int 以及被 short/signed/unsigned/long/long long 等修饰的 int 类型，不包括 enum 类型），以及这些类型的别名。

定义“常量计算”上下文，也就是 constexpr 修饰的变量和函数可以作为常量使用的场景：

- 可以用于 static_assert 中，第一个条件参数属于“常量计算”上下文
- 可以用于定义定长数组，数组长度属于“常量计算”上下文
- 可以用于初始化其它 constexpr 常量
- 可以用于常量泛型的实参

下面举例说明什么是“常量计算”上下文。

```c
void bar<int N>(){}

constexpr int foo() {
    return 5;
}

int main() {
    constexpr int a = 5;

    //可以用于常量泛型的实参
    bar<a>();
    bar<foo()>();

    //可以用于定义定长数组
    int arr1[a] = {0};
    int arr2[foo()] = {0};

    //可以用于初始化其它 constexpr 常量
    constexpr int b = a;
    constexpr int c = foo();

    //可以用于 static_assert 中
    _Static_assert(a == 5, "fail");
    _Static_assert(foo() == 5, "fail");

    return 0;
}
```

### 使用规则

#### constexpr 修饰变量

1. constexpr 可以修饰一个常量定义，且必须在定义时被初始化，否则要报错

```c
constexpr int a = 5;
constexpr int b; //error: constexpr variable 'b' must be initialized by a constant expression
```

2. constexpr 修饰的常量在定义之后，不可被修改

```c
constexpr int a = 5;
a = 10; //error: redefinition of 'a' with a different type: 'int' vs 'const int'
```

3. constexpr 修饰常量的类型只能是上述“编译时计算”的类型

```c
constexpr float a = 5.0;//error: BSC constexpr variable does not support type 'const float'
```

4. constexpr 修饰的常量的初始化表达式必须可以在编译时求值，否则要报错。可编译时求值的常量表达式可以是：

- 字面量
- constexpr 修饰的常量
- sizeof,_Alignof 表达式
- 以可编译时求值的常量表达式作为实参，调用 constexpr 函数
- 由以下运算符组合起来的常量表达式，也是常量表达式：+,-,*,/,%,>,<,==,!=,<=,>=,&,|,^,~,!,&&,||,<<,>>,?:

举例说明：

```c
//场景1
int a = 10;
constexpr int b = a;//error
//场景2
constexpr int a = 10;
constexpr int b = a;
//场景3
constexpr int a = sizeof(int);
constexpr int b = sizeof(int);
//场景4
constexpr int foo(int a) {
    return 5;
};
constexpr int a = 10;
constexpr int b = foo(a);
//场景5
constexpr int b = 1 == 1.0;
```

5. 函数指针可以使用 constexpr 修饰，函数指针可以指向 constexpr 函数
6. constexpr 可以修饰指针变量，但其只能指向全局变量或静态变量

#### constexpr 修饰函数

1. constexpr 可以修饰一个函数声明或者定义，但必须保证声明和定义中同时有 constexpr 修饰或都没有 constexpr 修饰，否则会导致编译错误

```c
constexpr int foo();
constexpr int foo() {
    return 5;
}
```

2. constexpr 修饰的函数，参数和返回类型，只能是上述“编译时计算”的类型

```c
constexpr void foo(); //error: BSC constexpr function does not support type 'void'
```

3. constexpr 可以修饰泛型函数

```c
constexpr int foo<T>();
```

4. constexpr 函数体内的所有语句，都是编译期可求值的

- constexpr 函数体内不允许定义 static 变量
- constexpr 函数体内不允许调用非 constexpr 函数
- constexpr 函数体内不允许访问外部的非 constexpr 变量
- constexpr 函数体内不允许内嵌汇编
- constexpr 函数体内允许定义不使用 constexpr 修饰的局部变量，这些变量也只能是“编译时计算”的类型

5. 在非“常量计算”的上下文中，constexpr 修饰的函数可以当作普通函数使用，实参不需要是常量，返回值也不需要是常量。在“常量计算”的上下文中，实参和返回值都要求是常量表达式，否则会报错

```c
constexpr int foo(int a) {
    return 5;
};

int a = 10;
constexpr int b = foo(a);// error: constexpr variable 'b' must be initialized by a constant expression
int c = foo(a);//ok，foo 函数处于非“常量计算“上下文中
int main(){
    int a = 10;
    constexpr int b = foo(a);// error: constexpr variable 'b' must be initialized by a constant expression
    int c = foo(a);//ok，foo 函数处于非“常量计算“上下文中
    return 0;
}
```

6. constexpr 可以修饰成员函数，包括普通成员函数和静态成员函数

```c
//普通成员函数，参数 This* this 不属于编译时计算类型
constexpr int int::foo1(This* this) { //error
    return 5;
}

//静态成员函数
constexpr int int::foo2() {
    return 5;
}

int main() {
    constexpr int c = int::foo2();//ok，可编译期求值
    return 0;
}
```

6. constexpr 不允许修饰 _Async 函数
7. constexpr 不允许支持变长参数
8. 函数的形参不能用 constexpr 修饰

```c
int foo1(constexpr int a) { //error
    return 5;
}

constexpr int foo2(constexpr int a) { //error
    return 5;
}
```

## type trait

type trait 可以看作是一个编译期计算返回值的 constexpr 函数。
BSC标准库中提供了一系列 type trait 泛型函数，使用时需要导入头文件 bsc_type_traits.hbs

目前实现的 type trait 函数有：

```c
// 判断类型的分类
constexpr bool is_integral<T>();
constexpr bool is_floating_point<T>();
constexpr bool is_pointer<T>();
constexpr bool is_function<T>();
constexpr bool is_array<T>();
constexpr bool is_struct<T>();
constexpr bool is_union<T>();
constexpr bool is_enum<T>();
constexpr bool is_void<T>();
//判断类型的属性
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
//判断类型的关系
constexpr bool is_same<T1, T2>();
constexpr bool is_convertible<From, To>();
```

针对其中一些作出说明和举例：

```c
constexpr bool is_move_semantic<T>(); //判断类型 T 是否是 move 语义的类型
// 我们认为具有 move 语义的类型包括：
//   1. owned指针类型
//   2. _Owned struct类型
//   3. 含有 move 语义字段的结构体类型
is_move_semantic<int *_Owned>() == true;
_Owned struct S {};
is_move_semantic<S>() == true;
struct S1 { int a; };
struct S2 { int *_Owned p; };
is_move_semantic<struct S1>() == false;
is_move_semantic<struct S2>() == true;
// 不完整的结构体的 move 语义无意义，报错
struct I;
is_move_semantic<struct I>(); // error: incomplete type 'struct I' used in type trait expression

constexpr bool is_trivial_data<T>(); // 判断类型 T 是否是平凡数据类型
// 我们认为平凡数据类型是不包含以下内容的类型：
//   1. 裸指针、_Owned 指针、_Borrow 指针
//   2. _Owned 结构体
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
// 不完整的结构体的是否是平凡数据无意义，报错
struct I;
is_trivial_data<struct I>(); // error: incomplete type 'struct I' used in type trait expression

constexpr size_t rank<T>(); //可用于计算数组的维数，如
rank<int>() == 0;
rank<int[5]>() == 1;
rank<int[5][5]>() == 2;

constexpr size_t extent<T, size_t N>();//可用于计算数组第 N 维元素的个数，如
extent<int[3],0>() == 3;
extent<int[3],1>() == 0;
extent<int[3][4],0>() == 3;
extent<int[3][4],1>() == 4;
extent<int[3][4],2>() == 0;

constexpr bool is_same<T1, T2>();//判断类型 T1，T2 是否一样，忽略类型别名

constexpr bool is_convertible<From, To>();//判断类型 From 是否可以隐式转换为类型 To，如
is_convertible<int, float>() == true;
is_convertible<int, const int>() == true;
is_convertible<int, volatile int>() == true;
is_convertible<int, signed>() == true;
is_convertible<int, void>() == false;
is_convertible<int, int*>() == false;
is_convertible<int, void*>() == false;
is_convertible<struct S, struct G>() == false;
```

使用时就像普通泛型函数一样

```c
#include<stdio.h>
#include<bsc_type_traits.hbs>

int main() {
    printf("%d\n",is_integral<int>()); //expected result: 1
    printf("%d\n",is_integral<float>()); //expected result: 0
    return 0;
}
```

type trait 函数可以在泛型函数和泛型结构体的成员函数中使用

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

type trait 函数也可用于静态断言中

```c
#include<bsc_type_traits.hbs>

int main() {
    _Static_assert(is_integral<int>() == true, "fail");
    _Static_assert(is_integral<float>() == false, "fail");
    return 0;
}
```

## constexpr if

### 概述

以 if constexpr 开头的语句被称为 constexpr if 语句，允许用户使用 constexpr 表达式作为 if 语句的条件，保证条件是编译时计算的常量，使得编译器在编译时就能够做分支判断，并对 false 分支进行死代码消除，减少运行时开销。

constexpr if 语句与普通 if 语句的区别在于，cond 条件表达式之前使用 constexpr 关键字进行修饰，即：

```c
if constexpr (<cond>) {
    <then-statement>
} else {
    <else-statement>
}
```

我们以计算阶乘为例来认识 constexpr if，先写出一个不使用 constexpr if 的版本来计算阶乘：

```c
int factorial(int n) {
    return (n == 1) ? 1 : n * factorial(n - 1);
}

int main() {
    printf("%d", factorial(5));
    return 0;
}
```

由于 BSC 不支持泛型特化的功能，故想要实现阶乘只能使用普通函数，在运行期进行求值。使用 constexpr if，我们可以实现类似于泛型特化的功能，实现编译期计算阶乘结果。

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

### 用法

constexpr if 语句中的 cond 表达式在使用时需要满足以下约束：

1. cond 表达式必须是可编译时求值的常量表达式；
2. cond 表达式的类型必须是可以隐式转换为 bool 的类型，包括 bool、整型和 char。
例如：

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

constexpr if 语句中，因为条件表达式是编译时计算的常量表达式，求值为 false 的分支会被定义为 “discarded statement”，会在编译期作为死代码被消除。
例如：

```c
if constexpr (<cond>) {  //如果<cond>的值为true
    <true-statement>
} else {
    <false-statement>  //那么else分支内的语句成为discarded statement，会在编译期作为死代码被消除
}
```

对于 discarded statement，有如下规则：

1. 在非泛型上下文内，discarded statement 仍然需要做完整的语法语义检查。
2. 在泛型上下文内，discarded statement 不会被实例化，也就不会进行实例化之后的语义检查。
例如：

```c
#include <stdio.h>
#include <bsc_type_traits.hbs>

void foo<T>(T a) {
    if constexpr (is_pointer<T>()) {
    //如果T不是指针，实例化的时候，下面这个block会被当作死代码，不会被实例化，也就不会进行实例化之后的语义检查
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
