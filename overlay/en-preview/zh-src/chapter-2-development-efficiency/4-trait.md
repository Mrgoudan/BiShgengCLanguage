# Trait

## 概述

Trait 是一种定义行为的方式，它类似于其它语言中的接口或抽象类，目的是定义一个实现某些目的所必须的行为集合。Trait 定义了一组方法签名，这些方法可以被多个结构体、枚举体或内置类型共享。主要作用是为了实现代码的复用和抽象，从而提高代码的可维护性和可扩展性。

## 语法规则

毕昇 C 引入关键字 `_Trait` 与 `_Impl`，通过关键字 `_Trait` 来定义 Trait，通过 `_Impl` 可以为一个类型实现一个或多个 Trait。下面通过一些代码示例来了解 Trait 的使用方法。

### Trait 的定义

**语法：**

```c
_Trait TraitName {
  // 定义 _Trait 中的方法签名
};
```

其中，`TraitName` 是 Trait 的名称，后面跟着一对花括号，里面可以定义一些方法的签名。Trait 中定义的方法不支持默认实现，必须由实现该 Trait 的类型提供具体实现。下面我们来看一个简单的例子：

```c
_Trait T {
    void doSomeThing1(This* this);
    void doSomeThing2(This* this);
};
```

**规则：**

1. Trait 定义只能出现在 top-level

```c
void test() {
    _Trait T {}; //不能在函数体中定义 _Trait. error: _Trait is only allowed to be defined at top-level
}

struct MyStruct {
    _Trait T {}; //不能在结构体中定义 _Trait. error: _Trait is only allowed to be defined at top-level
};
```

2. Trait 内要求函数首个入参且只有首个入参类型为 `This` 指针，命名为 `this`; `This` 指代实现了 Trait 的具体类型

```c
_Trait T {
    void doSomeThing1(This* this); // ok
    void doSomeThing2(This* a); // error: the first parameter of _Trait member function must be 'This* this'
    void doSomeThing3(int a, This* this); // error: the first parameter of _Trait member function must be 'This* this'
};
```

3. Trait 内只允许声明函数

```c
_Trait T {
    void doSomeThing1(This* this) { // error: expected member name or ';' after declaration specifiers
    }
};
```

4. Trait 内可以没有方法签名

```c
_Trait T {}; // ok
```

5. 不允许给 Trait 类型扩展成员函数

```c
void _Trait T::getArea(_Trait T* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}
```

### 实现 Trait

**语法：**

```c
_Impl <_Trait> for <type>;
```

我们可以通过 `_Impl` 关键字来为一个类型实现一个 Trait。直观的，我们来看一个简单的例子：

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

**规则：**

1. 在 `_Impl` 语句之前，一定存在 `<_Trait>` 和 `<type>` 的定义
2. 在 `_Impl` 语句之前，`<type>` 必须已经声明并实现 `<_Trait>` 中所有成员函数

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

_Impl _Trait T for struct S; // ok，struct S 已扩展声明了 _Trait 中所有成员函数
```

3. `<type>` 类型不允许是 Trait

```c
_Impl _Trait T for struct S; // ok，支持通过 _Impl 对已有 struct/union/enum 或内置类型实现 _Trait
_Impl _Trait T for _Trait T; // error: function 'f1' in '_Trait T' is not implemented for '_Trait T'
```

4. 支持对 `typedef` 类型实现 Trait

```c
typedef struct S S1;
_Impl _Trait T for S1;
```

### Trait 类型的变量

我们可以定义 Trait 指针类型的变量，并可以通过该指针变量进行函数调用等，具体使用方法如下：

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
    p = &s; // ok，隐式转换
    p->print(); // expected result: This is a struct instance, valued 0.000000
    p = &a;
    p->print(); // expected result: This is an int instance, valued 1
    p = &b; // error: expected a pointer type which has implemented '_Trait Print', found 'float'
}
```

**规则：**

1. 支持对 Trait 进行 `typedef`

```c
typedef _Trait Print {
    ...
}P;
```

2. 只允许声明 Trait 指针类型的变量

```c
_Trait Print* p1; // ok
_Trait Print p2; // error: only _Trait pointer type is allowed to be declared
```

3. 如果 `<type>` 实现了 `<_Trait>`，那么指向这个 `<type>` 的指针可以被转换为 `<_Trait>` 类型的变量

```c
_Impl _Trait Print for struct S;

struct S s;
_Trait Print* t1 = &s; // 隐式转换
_Trait Print* t2 = (_Trait Print*)&s; // 显式转换
```

4. Trait 指针类型的变量可以通过 `->` 方式调用该 Trait 中的成员方法

```c
struct S s;
_Trait Print* t = &s;
t->print();
```

5. Trait 指针类型的变量，允许用 `NULL` 赋值

```c
_Trait Print* p = NULL;
```

6. 允许指向 Trait 的多级指针，但这种类型不能直接调用成员函数。不允许结构体的二级指针直接隐式转换成 Trait 的二级指针

```c
_Trait Print* p;
p->print();
_Trait Print** q = &p; // ok
q->print(); // error: use of address-of-label extension outside of a function body
(*q)->print(); // ok
struct S s;
q = &&s; // error: tentative definition has type 'struct S' that is never completed
```

7. Trait 指针类型的变量，不可以解引用

```c
_Trait Print *p;
int main(){
    *p; // error: use of undeclared identifier 'p'
}
```

8. Trait 指针类型变量可以用 `const` / `volatile` 修饰

```c
const _Trait Print* p1;
volatile _Trait Print* p2;
```

9. 支持 Trait 指针类型作为函数参数及返回值类型

```c
_Trait Print* get(_Trait Print* t) {
    return t;
}
```

10. 支持 Trait 指针变量和 `NULL` 做比较（这里的比较仅包含 `==` 和`!=`，下同）

```c
_Trait Print* p = NULL;
if (p == NULL) {} // ok
if (p != NULL) {} // ok
```

11. 支持 Trait 指针变量和非 Trait 指针变量比较

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
// struct S 类型实现 _Trait F<int>，但 int 类型没有
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

12. 支持 Trait 指针变量和 Trait 指针变量比较

```c
_Trait Print* t1 = NULL;
_Trait Print* t2 = NULL;
_Trait G *g = NULL;
if (t1 == g) {} // warning: 如果 _Trait 类型不同会报 warning
if (t1 == t2) {} // ok
```

### 类型转换

Trait 类型转换只能在实现了对应 Trait 的类型之间进行，将一个类型转换为另一个类型，同时保留原有类型的特性和方法。

**规则：**

1. Trait 指针类型的变量，不允许强制转换为非指针类型

2. 支持 Trait 指针类型强制转为非 Trait 指针类型，但不支持隐式转换

```c
int a = 0;
_Trait T *p = &a; // 假设 int 类型实现了 _Trait T
int *q1 = p; // error: initializing 'int *' with an expression of incompatible type '_Trait T *'
int *q2 = (int*)p; // ok
struct S *q3 = (struct S*)p;
```

3. Trait 类型支持强制转换为 `void *`类型，但`void *` 指针无法转换为 Trait 指针类型

```c
int a = 1;
_Trait T *p = &a; // 假设 int 类型实现了 _Trait T
void * q = (void *)p; // ok: _Trait 类型支持强制转换为 `void *`类型
_Trait T *p1 = (_Trait T *)q; // error: expected a pointer type which has implemented '_Trait T', found 'void'
_Trait T *p2 = q; // error: expected a pointer type which has implemented '_Trait T', found 'void'
```

## 泛型 Trait 语法规则

泛型 Trait 是指在 Trait 中使用泛型类型参数，从而使 Trait 的方法可以适用于不同类型，避免代码的重复编写。下面通过一些代码示例来了解泛型 Trait 的使用方法。

### 泛型 Trait 的定义

**语法：**

```c
_Trait TraitName<T1,T2,...,Tn> {
  // 定义泛型 _Trait 中的方法签名，可以使用泛型类型 T1,T2,...,Tn
};
```

与 Trait 定义类似，`TraitName` 是泛型 Trait 的名称，后面跟着一对尖括号，里面可以包含一个或多个泛型参数，在花括号里面可以定义一些方法的签名。同样，泛型 Trait 中定义的方法不支持默认实现，必须由实现该泛型 Trait 的类型提供具体实现。下面我们来看一个简单的例子：

```c
_Trait F<T1, T2> {
    T1 doSomeThing1(This* this);
    T1 doSomeThing2(This* this, T2 param);
};
```

**规则：**

1. 泛型 Trait 定义只能出现在 top-level

```c
void test<T>() {
    _Trait F<T> {}; // error: _Trait is only allowed to be defined at top-level
}

struct MyStruct<T> {
    _Trait F<T> {}; // error: _Trait is only allowed to be defined at top-level
};
```

2. 泛型 Trait 内要求函数首个入参且只有首个入参类型为 `This` 指针，命名为 `this`; `This` 指代实现了 Trait 的具体类型

```c
_Trait F<T> {
    T doSomeThing1(This* this); // ok
    T doSomeThing2(This* a); // error: the first parameter of _Trait member function must be 'This* this'
    void doSomeThing3(T a, This* this); // error: the first parameter of _Trait member function must be 'This* this'
};
```

3. 泛型 Trait 内只允许声明函数

```c
_Trait F<T> {
    void doSomeThing1(This* this) { // error: expected member name or ';' after declaration specifiers
        ...
    }
};
```

4. 泛型 Trait 内可以没有方法签名

```c
_Trait F<T> {}; // ok
```

5. 不允许给泛型 Trait 类型扩展成员函数

```c
void _Trait F<T>::getArea(_Trait F<T>* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}

void _Trait F<int>::getArea(_Trait F<int>* this) { // error: cannot combine with previous 'void' declaration specifier
    ...
}
```

### 实现泛型 Trait

**语法：**

```c
_Impl <_Trait<SpecializationType>> for <type>;
```

我们可以通过 `_Impl` 关键字对已有 `struct`/`union`/`enum` 类型和内置类型实现 `_Trait<int>`/`_Trait<float>`等，需要注意的是，我们目前仅支持对实例化的 Trait 类型进行 Impl。直观的，我们来看一个简单的例子：

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

**规则：**

1. 在 `_Impl` 语句之前，一定存在泛型 `<_Trait>` 和 `<type>` 的定义
2. 仅支持 `_Impl` 实例化的 Trait 类型

```c
_Impl _Trait F<T> for int; // error: use of undeclared identifier 'T'
```

3. 在 `_Impl` 语句之前，`<type>` 必须已经声明了 `<_Trait>` 中所有成员函数

```c
_Trait F<T> {
    T f1(This* this);
    T f2(This* this);
};

struct S{};

int struct S::f1(struct S* this){};
_Impl _Trait F<int> for struct S; // error: function 'f2' in '_Trait F<int>' is not implemented for 'struct S'
int struct S::f2(struct S* this){};
_Impl _Trait F<int> for struct S; // ok，struct S 已扩展声明了 _Trait 中所有成员函数
```

4. 支持通过 `_Impl` 关键字对已有 `struct`/`union`/`enum` 类型和内置类型实现 `_Trait<int>`/`_Trait<float>`，但`struct`/`union` 类型类型不能是泛型的

```c
struct S {};
struct G<T> {};
_Trait F<T> {};
_Impl _Trait F<int> for int; // ok
_Impl _Trait F<int> for struct S; // ok
_Impl _Trait F<int> for struct G<int>; // error: cannot _Impl _Trait for an instantiated type
```

5. `<type>` 类型不允许是 Trait/泛型 Trait

```c
_Impl _Trait F<int>  for _Trait S; // error: unexpected token for ImplTraitDecl
_Impl _Trait F<int>  for _Trait F<int>; // error: unexpected token for ImplTraitDecl
```

6. 支持对 `typedef` 类型实现泛型 Trait

```c
typedef struct S S1;
_Impl _Trait F<int> for S1;
```

### 泛型 Trait 类型的变量

我们可以定义泛型 Trait 实例化后的指针类型变量，并可以通过该指针变量进行函数调用等，具体使用方法如下：

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
    p = &s; // ok，隐式转换
    p->foo(); // return 1
    p = &a;
    p->foo(); // return 0
    p = &b; // error: expected a pointer type which has implemented '_Trait F<int>', found 'float'
}
```

**规则：**

1. 只允许声明泛型 Trait 实例化后的指针类型变量

```c
_Trait F<int>* p1; // ok
_Trait F<int> p2; // error: only _Trait pointer type is allowed to be declared
```

2. 如果 `<type>` 实现了 `<_Trait>`，那么指向这个 `<type>` 的指针可以被转换为 `<_Trait>` 类型的变量

```c
_Impl _Trait F<int> for struct S;

struct S s;
_Trait F<int>* t1 = &s; // 隐式转换
_Trait F<int>* t2 = (_Trait F<int>*)&s; // 显式转换
```

3. 泛型 Trait 实例化后的指针类型变量可以通过 `->` 方式调用该泛型 Trait 中的成员函数

```c
struct S s;
_Trait F<int>* t = &s;
t->foo();
```

4. 泛型 Trait 实例化后的指针类型变量，允许用 `NULL` 赋值

```c
_Trait F<int>* p = NULL;
```

5. 泛型 Trait 实例化后的指针类型变量，不可以解引用

```c
_Trait F<int> *p;
*p; // error: use of undeclared identifier 'p'
```

6. 允许指向泛型 Trait 的多级指针，但这种类型不能直接调用成员函数。不允许结构体的二级指针直接隐式转换成泛型 Trait 的二级指针

```c
_Trait F<int>* p;
p->foo();
_Trait F<int>** q = &p; // ok
q->foo(); // error: use of address-of-label extension outside of a function body
(*q)->foo(); // ok
struct S s;
q = &&s; // error: tentative definition has type 'struct S' that is never completed
```

7. 泛型 Trait 实例化后的指针类型变量可以用 `const` / `volatile` 修饰

```c
const _Trait F<int>* p1;
volatile _Trait F<int>* p2;
```

8. 支持泛型 Trait 实例化后的指针类型作为函数参数及返回值类型

```c
_Trait F<int>* get(_Trait F<int>* t) {
    return t;
}
```

9. 支持泛型 Trait 实例化后的指针变量和 `NULL` 做比较（这里的比较仅包含 `==` 和`!=`，下同）

```c
_Trait F<int>* p = NULL;
p == NULL; // ok
p != NULL; // ok
```

10. 支持泛型 Trait 实例化后的指针变量和非 Trait 指针变量比较

```c
int a = 1;
int *p1 = &a;
struct S s;
struct S *p2 = &s;
_Trait F<int> *t = NULL;
// 假设struct S 类型实现 _Trait F<int>，但 int 类型没有
if (t == p1) {}; // warning
if (t == p2) {}; // ok
if (t == a) {}; // error: expected a pointer type which has implemented '_Trait F<int>', found 'int'
```

11. 支持指针变量和 Trait 指针变量比较

```c
_Trait F<int> *t1 = NULL;
_Trait F<int> *t2 = NULL;
_Trait F<float> *t3 = NULL;
if (t1 == t2) {} // ok
if (t1 == t3) {} // warning: comparison of distinct pointer types ('_Trait F<int> *' and '_Trait F<float> *')
```

### 类型转换

泛型 Trait 类型转换只能在实现了对应泛型 Trait 的类型之间进行，将一个类型转换为另一个类型，同时保留原有类型的特性和方法。

**规则：**

1. 泛型 Trait 实例化后的指针变量，不允许强制转换为非指针类型

```c
struct S s;
_Trait F<int> *p = &s; // 假设 struct S 类型实现了 _Trait F<int>
(struct S)p; // error: used type 'struct S' where arithmetic or pointer type is required
```

2. 支持泛型 Trait 指针类型强制转为非 Trait 指针类型，但不支持隐式转换

```c
struct S s;
_Trait F<int> *p = &s; // 假设 struct S 类型实现了 _Trait F<int>
struct S *q1 = p; // error: initializing 'struct S *' with an expression of incompatible type '_Trait F<int> *'
struct S *q2 = (struct S*)p; // ok
int *q3 = (int*)p; // ok
```

3. 泛型 Trait 类型支持强制转换为 `void *`类型，但`void *` 指针无法转换为泛型 Trait 指针类型

```c
struct S s;
_Trait F<int> *p = &s;
void * q = (void *)p; // ok: 泛型 _Trait 类型支持强制转换为 `void *`类型
_Trait F<int> *p1 = (_Trait F<int> *)q; // // error: expected a pointer type which has implemented '_Trait F<int>'', found 'void'
_Trait F<int> *p2 = q; // // error: expected a pointer type which has implemented '_Trait F<int>'', found 'void'
```

## 应用

```c
#include <stdio.h>

// 定义 _Trait
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

// 为结构体类型实现 _Trait
_Impl _Trait Shape for struct Square;
_Impl _Trait Shape for struct Rectangle;

// _Trait 指针类型作为函数参数及返回值类型
_Trait Shape* get(_Trait Shape* s) {
    return s;
}

void test() {
    struct Square s = {.side = 5};
    struct Rectangle r = {.width = 2, .height = 3};
    _Trait Shape* shape = &s;
    // _Trait 指针变量调用方法
    shape->getArea(); // the area of this square is 25.
    // 强制转换
    ((_Trait Shape*)&s)->getSideLen(); // the side length of this square is 20.
    // 将指针赋值给 _Trait 指针类型的变量
    shape = &r;
    shape->getArea(); // the area of this rectangle is 6.
    // 隐式转换，将 struct Rectangle* 转为 _Trait Shape*
    _Trait Shape* shape2 = get(&r);
    shape2->getSideLen(); // the side length of this rectangle is 10
}
```
