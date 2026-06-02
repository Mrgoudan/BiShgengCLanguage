# 所有权

## 前言

C 语言作为一种系统级编程语言，
提供了对指针的高度灵活的直接操作以及利用内存管理函数使开发者手动精细控制和管理内存的能力，
因而被广泛地应用于各种需要直接与硬件或内存等系统资源交互的领域和场景。
然而，这种内存管理模式存在容易导致内存泄漏、释放后使用、空指针解引用、缓冲区溢出和越界读写等内存安全问题。
内存安全问题不仅会造成资源的浪费，也可能导致程序行为错误，甚至导致程序崩溃，对程序的稳定性造成威胁。
内存安全问题可以划分为**时间内存安全**和**空间内存安全**两大类，其中时间内存安全包含内存泄漏、释放后使用、空指针解引用等，空间内存安全包括缓冲区溢出、越界读写等。
BiShengC 语言的内存管理为解决程序的时间内存安全问题，利用了**所有权特性**在编译期对潜在的内存安全问题进行检查，识别潜在的时间内存安全错误。

## 特性简介

BiShengC 语言的所有权特性被用于确保程序中的指针及其指向内存空间能被正确地管理。
在 BiShengC 语言中，使用`_Owned`关键字用来修饰一个指针类型，表明该指针拥有其指向的内存的所有权。
拥有所有权的指针必须确保其指向的内存在指针作用域结束前被显式释放，否则存在潜在的内存泄漏错误；
此外，一块堆内存只能同时被一个`_Owned`指针所拥有，`_Owned`指针为移动语义，这样避免了释放后使用等内存安全问题的发生。
以下是一段使用了所有权特性的 BiShengC 语言代码，用于了解所有权特性：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int *_Owned foo(int *_Owned p) { return p; }

_Safe void bar(void) {
  // 通过提供的 safe_malloc 申请一块大小为 sizeof(int) 的堆内存，并将值设置为2
  int *_Owned p = safe_malloc(2);
  // 将 p 指向的堆内存转移给 q，后续不可再使用 p 访问这块内存，否则编译报错
  int *_Owned q = p;
  _Unsafe {
    // 通过函数参数转移走 q 的所有权，但通过函数返回值归还所有权
    q = foo(q);
    // 在 q 的作用域结束前调用 safe_free
    // 安全释放堆内存，此处不释放则会报内存泄漏错误
    safe_free((void *_Owned)q);
  }
  return;
}

int main() {
  bar();
  return 0;
}
```

在安全区，`_Owned`指针指向的内存一定为堆上内存（如通过`safe_malloc`函数申请出的堆内存），`_Owned`指针不可能指向栈内存，在作用域结束前必须转移所有权或释放（如`safe_free`函数进行释放）。

这两个函数的函数原型如下：

```c
T *_Owned safe_malloc<T>(T t);
void safe_free(void *_Owned);
```

可以通过`safe_malloc`申请足够存放`T`类型的堆空间，并初始化为`t`，当`_Owned`指针生命周期结束前，必须通过`safe_free`进行释放，在使用时，需要将`T *_Owned`指针转换为`void *_Owned`指针类型再释放，对于**多级指针，需要从内到外进行释放**；对于结构体内部有`_Owned`指针成员的情况，需要**先将结构体内全部`_Owned`指针成员释放后，才能释放结构体的`_Owned`指针**。

**`_Owned` 与泛型类型参数**

在泛型函数中，`_Owned` 修饰泛型类型参数（如 `T _Owned` 或 `_Owned T`）时，`_Owned` 应用于完整类型 `T`。当 `T` 实例化为 `int*` 时，`T _Owned` 和 `_Owned T` 都产生 `int* _Owned`（_Owned 指针），而非 `_Owned int*`。泛型定义时允许对类型参数使用 `_Owned`，有效性在实例化时检查。

函数使用示例：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  //变量所有权初始化
  int *_Owned pi = safe_malloc(1);
  // 结构体所有权初始化
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  // 多级指针所有权初始化
  int *_Owned p = safe_malloc(1);
  int *_Owned *_Owned pp = safe_malloc(p);
  // 变量所有权释放
  safe_free((void *_Owned)pi);
  // 结构体所有权释放(从内到外)
  safe_free((void *_Owned)s1->p);
  safe_free((void *_Owned)s1->q);
  safe_free((void *_Owned)s1);
  // 多级指针所有权释放(从内到外)
  safe_free((void *_Owned) * pp);
  safe_free((void *_Owned)pp);
}

int main() {
  foo();
  return 0;
}
```

## 语法及语义规则

为实现所有权特性，BiShengC 语言引入了`_Owned`关键字用于修饰指针类型的变量。为区分指向数组的带所有权指针，BiShengC 还引入了 `_ArrayElem` 关键字用于修饰带`_Owned`的指针类型。这两个关键字与`const`、`restrict`以及`volatile`均属于类型修饰符。

具体而言，所有权特性的语法及部分语义规则有以下几点：

1. `_Owned`与`_ArrayElem`关键字仅允许在 BiShengC 语言编译单元内使用；

2. `_Owned`关键字仅被允许用于修饰指针类型，不允许修饰非指针类型，修饰多级指针时，每级指针的类型修饰可以不一样，规则与`const`类似；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int _Owned a = 2; // error: _Owned 关键字必须修饰指针

  double *_Owned q = safe_malloc(1.1);
  double _Owned b = 1.1; // error: _Owned 关键字必须修饰指针

  int *_Owned *_Owned pp = safe_malloc(p); // ok: 可以修饰多级指针
  safe_free((void *_Owned)pp); // error: 多级指针释放时，应当从内向外释放
  safe_free((void *_Owned) * pp); 

  double *d = (double *)malloc(sizeof(double));
  double **_Owned pd = safe_malloc(d);
  free(*pd); // 也可直接 free(d);
  safe_free((void *_Owned)pd);
  safe_free((void *_Owned)q);
  return 0;
}
```

3. 允许使用`_Owned`关键字修饰结构体指针及结构体的指针成员；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int m;
  int n;
};

struct R {
  int *_Owned p;
  double *_Owned q;
};

_Safe void test(void) {
  struct S s = {.m = 1, .n = 2};
  struct S *_Owned sp = safe_malloc(s);
  struct S _Owned so = {.m = 1, .n = 2}; // error: _Owned 不允许修饰结构体变量
  
  struct R r = {.p = safe_malloc(1), .q = safe_malloc(2.5)};
  struct R *_Owned rp = safe_malloc(r);
  safe_free((void *_Owned)sp);
  // 先释放结构体内的 _Owned 指针成员
  safe_free((void *_Owned)rp->p);
  safe_free((void *_Owned)rp->q);
  // 再释放结构体 _Owned 指针
  safe_free((void *_Owned)rp);
}

int main() {
  test();
  return 0;
}
```

4. `_Owned`不允许修饰`union`类型和`union`的成员，且`union`的每个成员均不能拥有`_Owned`修饰的成员；

```c
struct S {
  int a;
  int b;
};

struct T {
  int *_Owned p;
  struct S s;
};

union A {
  int a;
  int *_Owned p; // error: 禁止 union 成员用 _Owned 修饰
  struct S s;
  struct S *_Owned sp; // error: 禁止 union 成员用 _Owned 修饰
  struct T t; // error: 禁止 union 成员结构体有 _Owned 成员
  struct T *_Owned tp; // error: 禁止 union 成员用 _Owned 修饰
  struct T *trp; // ok: 不跟踪裸指针指向的变量的 _Owned 成员
};
```

5. `_Owned`修饰的类型或拥有`_Owned`修饰的成员的类型不可以作为数组的成员、不可以作为 `_Owned _ArrayElem` 指针的指向类型或其成员；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct A {
  int *_Owned p;
};

_Safe void test(void) {
  int *_Owned arr_i[2] = {safe_malloc(1), safe_malloc(2)}; // error: 数组成员不能被 _Owned 修饰
  struct A arr_a[2] = {{safe_malloc(1)}, {safe_malloc(2)}}; // error: 数组成员不能被 _Owned 修饰
}
_Safe struct A *_Owned _ArrayElem test2(void); // error: _Owned _ArrayElem 指针指向类型不能含有 _Owned 成员
```

6. `_Owned`修饰的指针不支持下标运算、算术运算（指针偏移操作），但支持比较运算；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe int main(void) {
  int *_Owned p = safe_malloc(2);
  int *_Owned q = safe_malloc(3);
  
  p += 1; // error: _Owned 指针不支持算术运算符
  
  p[3] = 3; // error: _Owned 指针不支持下标运算符

  if (p == q) { // ok: _Owned 指针支持比较运算符
  }

  _Unsafe {
    safe_free((void *_Owned)p);
    safe_free((void *_Owned)q);
  }
  return 0;
}
```

7. `_Owned _ArrayElem`修饰的指针表示指向数组的带所有权的指针，支持下标运算与比较运算，不支持算术运算。其分配与释放可以通过 `safe_malloc_array` 与 `safe_free_array` 实现。除非明确说明或单独列出 `_Owned _ArrayElem` 的情况，否则对 `_Owned` 指针的规则同样适用于`_Owned _ArrayElem`。 `_ArrayElem`不能修饰裸指针。

```c
#include "bishengc_safety.hbs"
// _Safe T *_Owned _ArrayElem safe_malloc_array<T>(size_t size, T t);
// _Safe void safe_free_array(void *_Owned _ArrayElem);

_Safe int main(void) {
  int * _ArrayElem ptr; // error: _ArrayElem 不能修饰裸指针
  int *_Owned _ArrayElem p = safe_malloc_array(10, 2);
  int *_Owned _ArrayElem q = safe_malloc_array(10, 3);

  p += 1; // error: _Owned _ArrayElem 指针不支持算术运算符

  p[3] = 3; // ok: _Owned _ArrayElem 指针支持下标运算符

  if (p == q) { // ok: _Owned _ArrayElem 指针支持比较运算符
  }
  safe_free_array((void *_Owned _ArrayElem)p);
  safe_free_array((void *_Owned _ArrayElem)q);
}
```

8. `_Owned`修饰的类型与非`_Owned`修饰的类型之间不允许隐式类型转换；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe int main(void) {
  _Unsafe {
    int *b = (int *)malloc(sizeof(int));
    int *_Owned p = b; // error: _Owned 指针不允许隐式类型转换
  }
  int *_Owned q = safe_malloc(3);
  int *c = q; // error: _Owned 指针不允许隐式类型转换
  safe_free((void *_Owned)q);
  return 0;
}
```

9. 禁止 `_Owned` 指针类型与裸指针类型之间的显式强制类型转换（包括非安全区）。当用户需要将裸指针转换为 `_Owned` 指针时，必须使用内置函数 `__take_from_raw` 显式表达所有权转移；当用户需要将 `_Owned` 指针转换为裸指针时：若需转移所有权，必须使用 `__move_to_raw` 显式表达所有权转移；若不需转移所有权，必须先取借用再进行转换，例如 `(T *)&_Mut *p`。以上转换均需在非安全区进行。对 `_Owned` 指针与裸指针之间转换的限制只对最外层指针类型生效；形如 `T*_Owned*` 与 `T**` 之间的转换视为裸指针之间的转换，在安全区不允许、非安全区允许显式执行。

    指向类型不一致时，只允许`void * _Owned`类型与`T * _Owned`类型之间的显式强制转换。此类转换在[所有权状态转移规则-强制类型转换](#强制类型转换)有额外说明。其他指向类型不一致的 `_Owned` 指针之间不允许转换。

    **显式所有权转移接口**（内置函数）：
    - `__move_to_raw(p)`：将 `_Owned` 指针 `p` 转为裸指针并转移所有权，返回的裸指针与入参具有相同的 Nullability。
    - `__take_from_raw(p)`：将裸指针 `p` 转为 `_Owned` 指针并取得其所指对象的所有权，返回的 `_Owned` 指针与入参具有相同的 Nullability。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
int main() {
  int *pi1 = (int *)malloc(sizeof(int));
  int *_Owned pi2 = (int *_Owned)pi1;  // error: 不允许裸指针转换为 _Owned 指针
  int *_Owned pi3 = __take_from_raw(pi1);  // ok: 显式将裸指针所有权转移至 _Owned

  double *_Owned pd1 = safe_malloc(1.0);
  int *_Owned pi4 = (int *_Owned)pd1;   // error: pi4 和 pd1 基本类型不一致
  double *pd2 = __move_to_raw(pd1); // ok: 显式将 _Owned 所有权转移至裸指针

  double *_Owned pd3 = safe_malloc(1.5);
  void *_Owned pv1 = (void *_Owned)pd3; // ok: 允许显式转换为 void *_Owned

  void *_Owned * ppv1 = &pv1;
  void ** ppv2 = (void **) ppv1; // ok: 允许显式从 T *_Owned * 转为 T **
  ppv1 = (void *_Owned *) ppv2; // ok: 允许显式从 T ** 转为 T *_Owned *
  safe_free((void *_Owned)pi3);
  safe_free(pv1);
  free((void*)pd2);
  return 0;
}
```

```c
// 不转移所有权时，先取借用再转换为裸指针:
#include "bishengc_safety.hbs"
void foo(int *);
int main() {
  int *_Owned p = safe_malloc(1);
  int *raw_ptr = (int *)&_Mut *p; // ok: 先取借用再转为裸指针，不转移所有权
  foo(raw_ptr); // 使用 raw_ptr
  safe_free((void *_Owned)p);
  return 0;
}
```

```c
// _Owned 与裸指针转换函数保留入参指针的 Nullability
#include "bishengc_safety.hbs"
int main() {
  int * p1 = malloc(sizeof(int));
  if (p1 != nullptr) {
    int * _Owned p2 = __take_from_raw(p1); // p1 判非空后 __take_from_raw(p1) 可以赋值给非空的 p2
    safe_free((void*_Owned)p2);
  }
  p1 = malloc(sizeof(int));
  int * _Owned p3 = __take_from_raw(p1); // p1 未判空，空指针检查启用时此处报错
  safe_free((void*_Owned)p3);
}
```

10. `_Owned _ArrayElem`指针与裸指针之间的转换规则与`_Owned`指针类似，但是必须数组版本的所有权转移接口。不允许通过 C 风格强制类型转换在 `T *`、`T *_Owned`、`T *_Owned _ArrayElem` 三者之间互转。

    显式所有权转移接口（内置函数）：
    - `__move_array_to_raw(p)`：将 `_Owned _ArrayElem` 指针转为裸指针并转移所有权。
    - `__take_array_from_raw(p)`：将裸指针 `p` 转为 `_Owned _ArrayElem` 指针，并取得其所指数组的所有权。

```c
#include <stdlib.h>

int main() {
  int *raw = (int *)malloc(4 * sizeof(int));
  int *_Owned _ArrayElem arr = __take_array_from_raw(raw); // ok
  int *_Owned plain = (int *_Owned)arr; // error: 不允许从 T *_Owned _ArrayElem 转为 T *_Owned
  int *raw2 = __move_array_to_raw(arr); // ok
  int *_Owned _ArrayElem arr2 = (int *_Owned _ArrayElem)raw2; // error: 应使用 __take_array_from_raw
  free(raw2);
  return 0;
}
```

11. `_Owned`允许修饰指向`_Trait`的指针，假设有一个具体类型`S`，它实现了`_Trait T`，则：

    - `S * _Owned`类型可以隐式转换为`_Trait T * _Owned`类型；
    - `_Trait T * _Owned`类型允许被显式转换为`void * _Owned`类型。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Trait T{};

_Impl _Trait T for int;

void test() {
  int *_Owned pi = safe_malloc(1);
  _Trait T *_Owned pti = pi; // ok: 隐式转换为 _Trait T *_Owned
  void *_Owned pvi = (void *_Owned)pti; // ok: 显式转换为 void *_Owned
  safe_free(pvi);
}

int main() {
  test();
  return 0;
}
```

12. 通过函数指针调用函数的时候，规则与一般的函数调用一样，会在函数调用时检查形参类型与实参类型是否匹配及返回类型与返回值类型是否匹配；

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int deref_add(int *a, int *b) { return *a + *b; }
typedef int (*FTP)(int *, int *);
typedef int (*FTOP)(int *_Owned, int *);

void foo() {
  FTP ftp = deref_add;
  int *_Owned pa = safe_malloc(1);
  int *_Owned pb = safe_malloc(2);
  
  ftp(pa, pb); // error: 类型不匹配
  FTOP ftop = deref_add; // error: 类型不匹配
  safe_free((void *_Owned)pa);
  safe_free((void *_Owned)pb);
}

int main() {
  foo();
  return 0;
}
```

13. `_Owned`可以修饰 _Trait 类型，即`_Trait T* _Owned`，也表示该变量拥有其内部存储的数据的所有权。
    该类型可以作为类型声明、函数的入参类型及函数的返回值类型。但当前不支持`_Trait T* _Owned`与`_Trait T*`之间的类型转换。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Trait T { _Safe void release(This * _Owned this); };
struct IPv4 {
  char *buf1;
};
struct IPv6 {
  char *buf1;
  char *buf2;
};
_Safe void struct IPv4::release(struct IPv4 *_Owned this) {
  _Unsafe { free(this->buf1); }
  safe_free((void *_Owned)this);
}
_Safe void struct IPv6::release(struct IPv6 *_Owned this) {
  _Unsafe {
    free(this->buf1);
    free(this->buf2);
  }
  safe_free((void *_Owned)this);
}
_Impl _Trait T for struct IPv4;
_Impl _Trait T for struct IPv6;

void cleanup(_Trait T *_Owned t) { t->release(); }

int main() {
  struct IPv4 ipv4 = {.buf1 = "192.168.1.1"};
  struct IPv6 ipv6 = {.buf1 = "2001:0db8:85a3:0000",
                      .buf2 = "0000:8a2e:0370:7334"};
  struct IPv4 *_Owned sipv4 = safe_malloc(ipv4);
  struct IPv6 *_Owned sipv6 = safe_malloc(ipv6);
  _Trait T *_Owned tipv4 = sipv4;
  _Trait T *_Owned tipv6 = sipv6;
  // 使用 _Trait T* _Owned 作为入参
  tipv4->release();
  tipv6->release();
  safe_free((void *_Owned)tipv4);
  safe_free((void *_Owned)tipv6);
  return 0;
}
```

14. _Owned 指针可以使用逻辑运算符。

```c
void logical_not(int *_Owned _Nullable p) {
  if (!p) { // equivalent: p == nullptr
  }
}
void logical_and(int *_Owned _Nullable p, int *_Owned _Nullable q) {
  if (p && q) { // equivalent: (p != nullptr) && (q != nullptr)
  }
}
void logical_or(int *_Owned _Nullable p, int *_Owned _Nullable q) {
  if (p || q) { // equivalent: (p != nullptr) || (q != nullptr)
  }
}
```

15. 允许 _Owned 指针作为 if while do-while for 语句和三元表达式的条件，不允许作为 switch 的条件。

```c
void foo(int *_Owned _Nullable p) {
  if (p) { // equivalent: p != nullptr
  }
  while (p) { // equivalent: p!= nullptr
  }
  do {
  } while (p); // equivalent: p != nullptr

  for (;p;) { // equivalent: p != nullptr
  }
  swtich (p) { // error
  default:
    break;
  }
  int x = p ? 2 : 1; // equivalent: p != nullptr ? 2 : 1
}
```

16. 允许在变量初始化、变量赋值和函数传参中将 `_Owned` 指针隐式转换为 `_Bool`，不会消耗其指向内容的所有权。

```c
void foo(int *_Owned _Nullable p) {
  _Bool flag = p; // equivalent: _Bool flag = p != nullptr;
}
void bar(int *_Owned _Nullable p, _Bool flag) {
  flag = p; // equivalent: flag = p != nullptr;
}
void use(_Bool);
void baz(int *_Owned _Nullable p) {
  use(p); // equivalent: use(p != nullptr);
}
```

## 所有权状态转移规则

在对所有权特性的语法和部分语义有了解后，本节将对所有权的状态转移规则进行详细阐述。
为更好地理解所有权特性对内存安全带来的保障，首先需要明白程序执行时的堆栈内存模型。

总体而言，程序执行时内存可分为栈区和堆区两个部分，这两部分内存一起为程序在运行时提供内存空间。
栈区即为调用栈，保存的是程序执行所需要维护的所有信息。
每当一次函数调用发生时，就会创建一个对应的栈帧，函数调用的上下文、函数的入参以及函数体内的局部变量就存在这个栈帧中。
栈帧的基址一般由 rbp 寄存器指向，而栈顶由 rsp 寄存器指向，两个寄存器共同标识了一个函数的栈帧。
当一次函数调用结束时，相应的栈帧就会在函数返回前被销毁，相应的内存空间也就得到释放。
这个过程是通过调整 rbp 寄存器的值为调用者的栈帧基址、rsp 的值为调用者的栈顶地址完成的，这也是为什么栈区的变量不需要显式释放。
对于堆区，则存放的是那些在运行时动态分配内存的数据。
一个典型的例子是对于`int *p = malloc(sizeof(int))`这种操作，需要由操作系统在堆区找到一块适合大小的内存空间用于分配，然后将这块内存的地址存在 p 中，指针 p 是程序的一个栈上变量。
虽然堆区的内存分配更为灵活，但却缺乏组织，不正确的内存管理很容易导致堆区的内存泄漏。
例如，当一次函数调用完成后，其局部变量 p 被销毁，而其指向的堆内存未被显式地调用`free(p)`进行回收，则这块堆内存将永远无法被回收，产生了内存安全错误。

利用 BiShengC 语言提供的所有权特性，可以用`_Owned`关键字对那些需要管理的指针进行标识，这样就可以在程序编译时检查出潜在的错误，避免在运行时出现错误。
以下是 BiShengC 语言所有权的核心规则：

1. 在 BiShengC 语言中每一个值都被一个`_Owned`指针变量所拥有，该`_Owned`指针变量即为值的所有者；
2. 一个值同时只能被一个`_Owned`指针变量所拥有，即一个值只能拥有一个所有者；
3. 当`_Owned`指针变量离开作用域范围时，需要释放其拥有的值所在的堆内存。

基于以上核心规则，接下来结合详细的代码示例进行具体介绍。

### 转移所有权

**1. 一个拥有所有权的变量`s1`被赋值给另一个变量`s2`是一个移动语义，该操作后变量`s1`失去了对值的所有权，原先的变量`s1`无法再使用。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe void foo(void) {
  int *_Owned p = safe_malloc(10);
  int *_Owned q = p;
  int *_Owned r = p; // error: p 的所有权已移交给q了
  _Unsafe {
    safe_free((void *_Owned)q);
    safe_free((void *_Owned)r);
  }
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，`p`拥有一块堆内存的所有权，这块内存大小为`sizeof(int)`，存储的值为10。
在声明`q`时，将`p`的所有权转移给了`q`，`p`不再拥有对这块堆内存的所有权。
则在声明`m`时，便不可再将`p`的所有权转移给`m`，编译器会在此处报错。
因此，利用所有权特性，可以保证一个值只能拥有一个所有者。
（那么如果没有这条规则会出现什么后果呢？三个指针会同时指向一块内存，在作用域结束时对这块内存释放三次，出现重复释放的错误）

**2. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则会全部都转移给`s2`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *p;
  int *_Owned q;
};

void test(void) {
  struct S s = {.p = (int *)malloc(sizeof(int)), .q = safe_malloc(1)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  int *_Owned p = s1->q; // error: q 的所有权被一并交给 s2 了
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
  safe_free((void *_Owned)p);
}

int main() {
  test();
  return 0;
}
```

在这个例子中，将`s1`指向堆内存的所有权转移给了`s2`，但同时`s1`内部还有一个拥有所有权的指针`s1->q`，因此也会一并将他的所有权转移给`s2->q`，后续再使用`s1->q`时便会报错。

**3. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则必须保证内部其他`_Owned`指针均拥有所有权，才能将`s1`赋值给`s2`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  int *_Owned p = s1->p;
  struct S *_Owned s2 = s1; // error: s1 已无对全部成员的所有权
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)s2->p);
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，我们先将`s1->p`的所有权转移走，然后试图整体转移`s1`的所有权给`s2`，但此时`s1->p`已经不再持有对任何一块堆内存的所有权，因此这个操作是不合法的。

**4. 一个拥有所有权的变量`s1`在失去其所有权后，可以通过赋值的方式使其再次拥有指向某块堆内存的所有权，这样就可以再次使用`s1`。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe void foo(void) {
  int *_Owned p = safe_malloc(10);
  // 移走 p
  int *_Owned q = p;
  // 重新拿到新元素的所有权
  p = safe_malloc(4);
  // 仍然可以将所有权交给其他指针
  int *_Owned m = p;
  safe_free((void *_Owned)q);
  safe_free((void *_Owned)m);
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，`p`的所有权转移给`q`后，再次调用了`safe_malloc`函数为其重新赋予了一块堆内存的所有权，因此后续仍可将`p`的所有权转移给`m`。

**5. 不允许将所有权转移给一个已经拥有所有权的变量。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe void foo(void) {
  int *_Owned p = safe_malloc(12);
  int *_Owned q = safe_malloc(67);
  q = p; // error: p 有拥有其他指针的所有权
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)q);
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，试图将`p`的所有权转移给`q`，但`q`此时已经拥有所有权，再试图转移的话会使`q`原先指向的堆内存泄漏，因此无法进行转移，会在编译时报错。

**6. 如果一个变量`s1`拥有所有权，而其内部的`_Owned`指针变量的所有权已被转移，如果想再次赋予内部变量所有权，需要保证内部变量的所有父`_Owned`指针变量均拥有所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  s1->p = safe_malloc(5); // error: s1 已不再拥有所有权，无法给内部成员赋予所有权
  safe_free((void *_Owned)s2->p);
  safe_free((void *_Owned)s2->q);
  safe_free((void *_Owned)s2);
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，`s1`、`s1->p`以及`s1->q`的所有权均被转移给了`s2`，后续再试图赋予`s1->p`所有权时，其父`_Owned`变量指针`s1`尚未拥有所有权，因此此次操作是非法的，会在编译时报错。

### 作用域结束时的内存释放

**1. 对于所有的`_Owned`指针变量，会在其词法作用域结束时检查其是否依然拥有堆内存的所有权，如果依然拥有，则存在内存泄漏错误。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  int *_Owned p = safe_malloc(2);
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  struct S *_Owned s2 = s1;
  // safe_free((void *_Owned)p);
  // safe_free((void *_Owned)s2->p);
  // safe_free((void *_Owned)s2->q);
  // safe_free((void *_Owned)s2);
} // error: 未释放 s2 内部成员所有权及 s2 所有权；未释放 p 所有权

int main() {
  foo();
  return 0;
}
```

在这个例子中，当作用域结束时，编译器会发现`p`、`s2`、`s2->p`以及`s2->q`依然拥有其指向的堆内存的所有权，即这些堆内存都没有被释放，因此会编译失败并报告内存泄漏。

### 强制类型转换

**1. 允许将`T * _Owned`类型的变量通过强制类型转换转为`void * _Owned`类型，但转换成功的条件为变量依然拥有所有权且其内部的`_Owned`指针变量均已不拥有所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void foo(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  struct S *_Owned s1 = safe_malloc(s);
  int *_Owned p = s1->p;
  _Unsafe {
    safe_free((void *_Owned)s1); // error: s1 仍然拥有内部成员(q)所有权，无法转为 void *_Owned
    safe_free((void *_Owned)p);
  }
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，试图将`s1`强制类型转换为`void * _Owned`类型，但`s1->q`依然拥有所有权，因此转换失败。

注意，在涉及 `T * _Owned` 到 `void *` 的转换时，转换的顺序不同会导致语义不同:

1. 先将 `_Owned` 指针的指向类型转为 `void`，再使用 `__move_to_raw` 将 `_Owned` 指针转换为裸指针 (即`T * _Owned` 转 `void * _Owned` 转 `void *`): `T * _Owned` 指针必须具有所有权，但其指向的 `T` 内部所有 `_Owned` 指针变量必须**均不拥有所有权**才能进行转换。
2. 先使用 `__move_to_raw` 将 `_Owned` 指针转换为裸指针，再改变指向类型 (即`T * _Owned` 转 `T *` 转 `void *`): `T * _Owned` 指针必须具有所有权、其指向的 `T` 内部所有 `_Owned` 指针变量仍**拥有所有权**才能进行转换。

用户在需要将 `T * _Owned` 转成裸指针进行释放时，应先将其成员释放，再采用第一种转换顺序，将 `T * _Owned` 转为 `void * _Owned` 再转为裸指针进行释放。编译器会在 `T * _Owned` 转为 `void * _Owned` 时检查 `T` 内的 `_Owned` 指针均已转移所有权，避免内存泄露。除了需要释放对象的情况，其余场景均应采用第二种转换顺序，将 `T * _Owned` 内 `_Owned` 指针的所有权保留下来，以免后续使用已释放的指针。

以下是示例代码：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

void myfree(struct S *_Owned ptr) {
  // 释放场景：采用第一种顺序 (T * _Owned 转 void * _Owned 转 void *)
  safe_free((void *_Owned) ptr->p);
  safe_free((void *_Owned) ptr->q);

  // free((void *)__move_to_raw(ptr)); // error: ptr内部已经没有所有权， 不能使用 __move_to_raw
  free(__move_to_raw((void *_Owned)ptr)); // ok
}
void *mymove(struct S *_Owned ptr) {
  // 其余场景：采用第二种顺序 (T * _Owned 转 T * 转 void *)
  // return __move_to_raw((void *_Owned)ptr); // error: ptr内部仍有所有权，不能转为 void *_Owned
  return (void *)__move_to_raw(ptr); // ok
}
```

**2. 允许将`void * _Owned`类型的变量通过强制类型转换转为`T * _Owned`类型，转换成功的条件为变量依然拥有所有权，转换成功后，得到的`T *_Owned`类型内部的`_Owned`指针变量均不拥有所有权。**
以下是一段代码示例及说明：

```c
struct S {
  int *_Owned p;
  int *_Owned q;
};

_Safe void *_Owned memAlloc(unsigned long);
_Safe void memFree(void *_Owned);

_Safe void foo(void) {
  struct S *_Owned sp = _Unsafe((struct S *_Owned)memAlloc(sizeof(struct S)));
  int *_Owned p = sp->p; // error: sp->p 此时未拥有所有权
  sp->q = _Unsafe((int *_Owned)memAlloc(sizeof(int)));
  *(sp->q) = 2;
  _Unsafe {
    memFree((void *_Owned)sp->q);
    memFree((void *_Owned)sp);
  }
}

int main() {
  foo();
  return 0;
}
```

在这个例子中，试图直接转移`sp->p`的所有权给`p`，但此时`sp->p`还未拥有所有权，因此编译报错。

### 函数调用与返回

**1. 函数调用和返回时，如果函数的形参或函数的返回值为`_Owned`指针类型，则要求传入的实参以及返回值必须拥有堆内存的所有权。**
以下是一段代码示例及说明：

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int *_Owned p;
  int *_Owned q;
};

struct S *_Owned foo(struct S s) {
  struct S *_Owned ret = safe_malloc(s);
  return ret;
}

void bar(void) {
  struct S s = {.p = safe_malloc(2), .q = safe_malloc(3)};
  int *_Owned p = s.p;
  struct S *_Owned s1 = foo(s); // error: s 已不再拥有全部内部成员的所有权
  safe_free((void *_Owned)p);
  safe_free((void *_Owned)s1->p);
  safe_free((void *_Owned)s1->q);
  safe_free((void *_Owned)s1);
}

int main() {
  bar();
  return 0;
}
```

在这个例子中，传入`F`函数的结构体变量`s`内部有两个`_Owned`指针变量，而`s.p`已经被转移走，因此这次函数调用是非法的，会编译报错。

## 源源变换

BiShengC 语言的 clang 编译器支持源源变换功能，即将`.cbs`文件转换为等价的`.c`文件。
所有权特性仅引入了`_Owned`关键字表示所有权，在源源变换时只会去掉所有的`_Owned`关键字，然后生成相应的`.c`代码。
关于源源变换的详细细节，请参考手册的源源变换章节。
