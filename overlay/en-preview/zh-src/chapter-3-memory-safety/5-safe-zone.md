# 安全区

## 概述

c 语言有很多规则过于灵活，不方便编译器做静态检查。因此我们引入一个新语法，使得在一定范围内的毕昇 c 代码必须遵循更严格的约束，保证在这个范围内的代码肯定不会出现“内存安全”问题。

允许用`_Safe`和`_Unsafe`关键字修饰函数、语句、括号表达式。

- `_Unsafe`表示这段代码在非安全区，这部分代码遵循标准 c 的规定，同时这部分代码的安全性由用户保证。

- `_Safe`表示这段代码在安全区，这部分代码必须遵循更严格的约束，同时编译器可以保证内存安全。

- 没有 `_Safe`或`_Unsafe`关键字修饰的全局函数默认是非安全的。

## 代码示例

```c
#include <stdlib.h>

typedef struct File {
} FileID;

// 无关键字修饰，表示按默认非安全区
FileID *_Owned create(void) {
  FileID *p = malloc(sizeof(FileID));
  return (FileID * _Owned) p;
}
FileID *_Owned foo(FileID *_Owned p) { return p; }

// 使用 _Safe 修饰函数，表示该函数为安全函数，函数内为安全区
_Safe void safely_free(FileID *_Owned p) {
  // 使用 _Unsafe 修饰代码块，表示这段代码在非安全区。这段代码是在安全区内的非安全区，也属于非安全区
  _Unsafe { free((FileID *)p); }
}

int main(void) {
  FileID *_Owned p1 = create();
  FileID *_Owned p2 = foo(p1);
  // 使用 _Safe 修饰语句，表示这段代码在安全区
  _Safe safely_free(p2);
  return 0;
}
```

## 语法规则

1. 允许使用`_Safe/_Unsafe`修饰函数声明、函数签名、函数定义、函数指针、语句、括号表达式。

```c
// 修饰函数签名
_Safe int foo(int, int);
// 修饰函数声明
_Safe int bar(int a, int b);
// 修饰函数定义
_Safe int add(int a, int b) { return a + b; }
// 修饰函数指针
_Safe int (*p)(int, int);

_Safe int main(void) {
  // 修饰代码块
  _Safe {
    int a = 1;
  }
  _Unsafe { int b = 1; }
  // 修饰语句
  _Safe int c = 1;
  _Unsafe c++;
  // 修饰括号表达式
  char d = _Unsafe((char)c);
  return 0;
}
```

2. 不能使用`_Safe/_Unsafe`修饰全局变量、函数外类型声明、`typedef`声明（允许修饰函数指针）。

```c
_Safe int g_a; // error: 不能修饰全局变量

_Safe struct b { int a; }; // error: 不能修饰函数外类型声明

_Safe typedef int mm; // error: 不能修饰 typedef

int main() { return 0; }
```

3. `_Safe`修饰的函数，参数类型和返回类型允许是裸指针类型、成员中包含裸指针的`struct`类型、数组类型以及`union`类型。

```c
_Safe int *foo(int a); // ok: 返回值为裸指针类型

_Safe int bar(int *a); // ok: 参数类型为裸指针类型

typedef struct F {
  int *a;
} SF;

_Safe SF wrapped_foo(int a); // ok: 返回值为成员中包含裸指针类型的 struct 类型

_Safe int wrapped_bar(SF b); // ok: 参数类型为成员中包含裸指针类型的 struct 类型
```

4. `_Safe`修饰的函数，函数参数列表不可以省略。

```c
_Safe void test(); // error

_Safe void test(void); // ok
```

5. `_Safe`修饰的函数，函数参数列表不可以包含变长参数，除非该函数使用了`__attribute__((format(...)))`属性。

```c
_Safe int foo(int a, ...); // error
__attribute__((format(printf, 1, 2))) _Safe int bar(const char *fmt, ...); // ok
```

注意：即使允许声明带format属性的变长参数函数，在函数体内仍然不能使用`va_start`、`va_arg`、`va_end`等，这些操作在安全区域内是被禁止的。

6. 如果`_Trait`中的函数被声明为`_Safe`，那么要求实现`_Trait`的类型的对应成员函数也必须是`_Safe`修饰的函数。若`_Trait`中的函数未声明为`_Safe`，也允许实现`_Trait`中的类型的成员函数为`_Safe`，但编译器会给出**warning**。

```c
_Trait G {
  _Safe int *_Owned foo(This * _Owned this);
  int *_Owned bar(This * _Owned this);
};

_Safe int *_Owned int ::foo(int *_Owned this) { return this; } // ok: _Trait 实现函数必须为 _Safe
// 非 _Safe 函数的实现可以为 _Safe
_Safe int *_Owned int ::bar(int *_Owned this) { return this; }
_Impl _Trait G for int;

int main() { return 0; }
```

7. 函数的多声明与混合模式

同一函数标识符可以有多个声明,支持`_Safe`和`_Unsafe`混合声明。

- 7.1 兼容性要求

   **相同修饰符**: 多个`_Safe`声明之间、或多个`_Unsafe`声明之间,函数类型必须兼容

   **混合模式**: `_Safe`和`_Unsafe`声明可以共存,但必须满足混合模式兼容性(见7.3)

   **泛型函数除外**: 泛型函数不支持混合模式,同一实例化不能同时有`_Safe`和`_Unsafe`声明

   **成员函数**: 与普通函数规则相同

- 7.2 函数类型兼容性

   **两个函数类型兼容的条件**:
  - 返回类型兼容
  - 参数数量相同,省略号(`...`)使用一致
  - 对应参数类型兼容(兼容性检查时移除除`_Owned`、`_Borrow`、`_ArrayElem`、`_Nonnull`、`_Nullable`外的所有限定符)

  **指针兼容性**:

  - 指针类型需要相同限定符(`_Owned`、`_Borrow`、`_ArrayElem`、`_Nonnull`、`_Nullable`)且目标类型兼容
  - `_Owned`和`_Borrow`指针（以及带`_ArrayElem`的版本）之间两两互不兼容
  - `_Owned`/`_Borrow`指针（以及带`_ArrayElem`的版本）与裸指针不兼容(混合模式除外，见7.3)

- 7.3 混合模式兼容性(`_Unsafe`与`_Safe`共存)

   **返回类型兼容**:

  - `_Safe`声明与`_Unsafe`声明中的返回值类型在去掉毕昇C引入的类型限定符(`_Owned`,`_Borrow`,`_ArrayElem`等)、**保留**其他C的限定符(`const`,`volatile`等)之后是兼容的。
  - `_Safe`声明只能为返回类型**添加**`_Owned`，`_Borrow`，`_Owned _ArrayElem`，或`_Borrow _ArrayElem`限定符，不能**移除**已有的限定符。`_Owned _ArrayElem`与`_Borrow _ArrayElem`应当视作整体进行添加。

   **参数类型兼容**:

  - `_Safe`声明与`_Unsafe`声明中的参数类型在去掉毕昇C引入的类型限定符(`_Owned`,`_Borrow`,`_ArrayElem`等)以及**去除**其他C的限定符(`const`,`volatile`等)之后是兼容的。
  - `_Safe`声明只能为参数**添加**`_Owned`，`_Borrow`，`_Owned _ArrayElem`，或`_Borrow _ArrayElem`限定符，不能**移除**已有的限定符。`_Owned _ArrayElem`与`_Borrow _ArrayElem`应当视作整体进行添加。

   **示例**:

   ```c
   // ok: _Safe声明添加_Owned限定符
   _Unsafe int* f1(int* p);
   _Safe int* _Owned f1(int* _Owned p);

   // ok: _Safe声明添加_Borrow限定符
      _Unsafe int* f2(int* p);
   _Safe int* _Borrow f2(int* _Borrow p);

   // ok: _Safe声明添加_Owned _ArrayElem限定符
   _Unsafe int* f3(int* p);
   _Safe int* _Owned _ArrayElem f3(int* _Owned _ArrayElem p);

   // ok: _Safe声明添加_Borrow _ArrayElem限定符
   _Unsafe int* f4(int* p);
   _Safe int* _Borrow _ArrayElem f4(int* _Borrow _ArrayElem p);

   // error: _Safe声明移除了_Unsafe声明中的_Owned限定符
   int* _Owned f5(int* _Owned p);
   _Safe int* f5(int* p);

   // error: _Safe声明移除了_Unsafe声明中的_Borrow限定符
   int* _Borrow f6(int* _Borrow p);
   _Safe int* f6(int* p);

   // error: _Safe声明移除了_Unsafe声明中的_Owned _ArrayElem限定符
   int* _Owned _ArrayElem f7(int* _Owned _ArrayElem p);
   _Safe int* f7(int* p);

   // error: _Safe声明移除了_Unsafe声明中的_Borrow _ArrayElem限定符
   int* _Borrow _ArrayElem f8(int* _Borrow _ArrayElem p);
   _Safe int* f8(int* p);

   // error: _Unsafe声明中没有_ArrayElem限定符时_Safe声明不能加_ArrayElem
   int* _Borrow  f9(int* _Borrow p);
   _Safe int* _Borrow _ArrayElem f9(int* _Borrow _ArrayElem p);
   int* _Owned f10(int* _Owned p);
   _Safe int* _Owned _ArrayElem f10(int* _Owned _ArrayElem p);

   // error: owned与borrow不兼容
   _Unsafe int* _Owned f11(int* _Borrow p);
   _Safe int* _Borrow f11(int* _Owned p);

   // ok: 相同修饰符,类型完全相同
   _Safe int* _Owned f12(int* _Owned p);
   _Safe int* _Owned f12(int* _Owned q);
   ```

- 7.4 函数定义

   混合模式函数只能定义**一次**

   定义可基于`_Unsafe`版本或`_Safe`版本,但必须与所有声明兼容

   ```c
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);
   
   // 定义(选择其一)
   _Safe int* _Owned foo(int* _Owned p) { return p; }
   // 或
   _Unsafe int* foo(int* p) { return p; }
   ```

8. 函数调用解析

- 8.1 安全上下文调用

   安全上下文(`_Safe`块)内只能调用`_Safe`函数。如果函数只有`_Unsafe`声明,编译错误。

   ```c
   _Unsafe void foo(void);
   
   int main() {
     _Safe {
       foo();  // error: 安全区内不允许调用非安全函数
     }
   }
   ```

- 8.2 非安全上下文调用

   非安全上下文(`_Unsafe`块或默认)内执行重载解析:

  - 优先匹配`_Safe`声明
  - `_Safe`不匹配时使用`_Unsafe`声明

   ```c
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);
   
   int *_Owned bar() {
     int* raw_p = nullptr;
     int* _Owned owned_p = nullptr;
   
     // 根据参数类型选择对应版本
     foo(raw_p);      // 调用unsafe版本
     return foo(owned_p);    // 调用safe版本
   }
   ```

9. 函数指针赋值

函数指针赋值规则:

**给`_Safe`修饰的函数指针赋值时**:

- 如果被用于赋值的函数标识符没有`_Safe`版本的声明,则报错,即不允许`_Unsafe`版本的声明赋值给`_Safe`修饰的函数指针
- 如果被用于赋值的函数标识符有`_Safe`版本的声明,则要求`_Safe`版本的声明类型与函数指针类型是兼容的

**给`_Unsafe`修饰的函数指针赋值时**:

- 只要被用于赋值的函数标识符的`_Unsafe`版本声明类型与函数指针类型是兼容的,或`_Safe`版本声明类型与函数指针类型是`_Unsafe-_Safe`兼容的,即允许赋值
- 都不满足则编译报错

**示例**:

```c
_Safe void safe_foo(void);
_Unsafe void unsafe_foo(void);

// 混合模式声明
_Unsafe int* bar(int* p);
_Safe int* _Owned bar(int* _Owned p);

int main() {
  _Safe void (*safe_ptr)(void) = nullptr;
  _Unsafe void (*unsafe_ptr)(void) = nullptr;

  safe_ptr = safe_foo;      // ok: safe函数赋值给safe指针
  safe_ptr = unsafe_foo;    // error: 没有safe版本的声明

  unsafe_ptr = unsafe_foo;  // ok: unsafe函数赋值给unsafe指针
  unsafe_ptr = safe_foo;    // ok: safe版本与unsafe-safe兼容

  // 混合模式函数指针赋值
  _Safe int* _Owned (*safe_bar_ptr)(int* _Owned) = nullptr;
  _Unsafe int* (*unsafe_bar_ptr)(int*) = nullptr;

  safe_bar_ptr = bar;   // ok: 使用safe版本
  unsafe_bar_ptr = bar; // ok: 使用unsafe版本或safe版本(_Unsafe-safe兼容)
}
```

函数参数中声明为数组类型的形参与对应的指针类型形参等价，函数指针赋值时两者可以互换：

```c
_Safe void test10(int arr[3]) {}
_Safe void test11(int *arr) {}

_Safe void (*p10)(int arr[3]) = nullptr;
_Safe void (*p11)(int *arr) = nullptr;

_Safe int main(void) {
    p10 = test11;  // ok: int arr[3] 与 int *arr 等价
    p11 = test10;  // ok: int *arr 与 int arr[3] 等价
    return 0;
}
```

10. `_Safe`修饰泛型函数时，会对泛型每个实例化版本也做`_Safe`检查。

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe T identity<T>(T a) { return a; }

void foo() {
  int a = 1;
  int b = identity<int>(a);
  int *_Owned c = (int *_Owned)safe_malloc(1);
  int *_Owned d = identity<int * _Owned>(c); // ok
  int *e = identity<void *>((void *)0); // ok
  safe_free((void *_Owned)d);
}

int main() {
  foo();
  return 0;
}
```

11. 成员函数也可以被`_Safe/_Unsafe`修饰，其规则和全局函数一样。

```c
struct MyStruct<T> {
  T res;
};
_Safe T struct MyStruct<T>::foo(T a) {
  return a;
}

int main() { return 0; }
```

12. 安全区内被调用的函数或函数指针必须是`_Safe`的函数签名，不允许调用非安全函数或函数指针。

```c
_Safe void safe_foo(void) {}
_Unsafe void unsafe_foo(void) {}
_Safe void (*safe_func_ptr)(void);
_Unsafe void (*unsafe_func_ptr)(void);
int main() {
  _Safe {
    safe_foo();
    unsafe_foo(); // error: 安全区内不允许调用非安全函数
    safe_func_ptr();
    unsafe_func_ptr(); // error: 安全区内不允许调用非安全函数指针
  }
  _Unsafe {
    safe_foo();
    unsafe_foo(); // ok
    safe_func_ptr();
    unsafe_func_ptr(); // ok
  }
}
```

13. 安全区内允许再包含`_Unsafe`修饰的语句、函数指针、括号表达式，非安全区内也允许再包含`_Safe`修饰的语句、函数指针、括号表达式。

```c
int add(int a, int b) { return a + b; }
_Safe int max(int a, int b) { return a > b ? a : b; }
int main() {
  _Safe {
    int a = 0;
    _Unsafe a++;
    _Unsafe {
      a = add(1, 3);
      _Safe a = max(3, 5);
    }
  }
}
```

14. 安全区内`switch`语句中的`case/default`只能存在于`switch`后面的第一层代码块中，且第一层代码块不允许有变量定义。

```c
_Safe void foo(int a) {
  switch (a) {
    int b = 10; // error: 第一层代码块不允许有变量定义
    case 0: {
        int c = 1;
        break;
    }
    {
        case 1 : { break; } // error: case 只能存在于 switch 后面的第一层代码块中
    }
    {
        default: { break; } // error: default 只能存在于 switch 后面的第一层代码块中
    }
  }
}

int main() {
  int a = 1;
  foo(a);
  return 0;
}
```

15. 安全区内不允许访问`union`的成员（读或写），不允许裸指针通过`->`访问成员。

    安全区内允许定义和声明`union`类型，允许`union`作为函数参数和返回类型，但不允许通过`.`操作符访问`union`的任何成员。

    允许owned指针和borrow指针通过`->`访问成员。

```c
union AgeOrName {
  int age;
  char name[16];
};
struct AgeWrapper {
  int age;
};

void foo(void) {
  struct AgeWrapper d = {10};
  struct AgeWrapper *e = &d;
  struct AgeWrapper *_Owned f = (struct AgeWrapper * _Owned) & d;
  struct AgeWrapper *_Borrow i = &_Mut d;
  _Safe {
    int a = 1;

    a++; // ok: 安全区允许自增// ok : 允许显式转换为void类型的owned指针
    a--; // ok: 安全区允许自减

    a += 1; // ok: 安全区允许 += 运算符
    a -= 1; // ok: 安全区允许 -= 运算符

    union AgeOrName b = {10}; // ok: 安全区允许声明和初始化 union

    int c = b.age; // error: 安全区不允许通过"."访问 union 成员（读操作）
    b.age = 20; // error: 安全区不允许通过"."访问 union 成员（写操作）

    int g = e->age; // error: 安全区不允许裸指针通过"->"访问成员
    int h = f->age; // ok: 允许owned指针通过"->"访问成员
    int j = i->age; // ok: 允许borrow指针通过"->"访问成员
  }
}

int main() {
  foo();
  return 0;
}
```

16. 安全区内不允许使用取地址符`&`（允许对函数取地址），只允许`&_Const`，`&_Mut`取借用。

    安全区内不允许解引用裸指针，但可以解引用`_Owned`指针、`_Borrow`指针和非空函数指针。

```C

_Safe int inc(int a) { return a + 1; }

_Safe int test1(unary_f _Nonnull f) {
  return (*f)(1);  // ok: _Nonnull 函数指针
}

_Safe int test2(unary_f f) {
  if (f != nullptr) {
    return (*f)(1);  // ok: 判空后确认为非空
  }
  return 0;
}
```

```c
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

typedef _Safe int (*unary_f)(int);
_Safe int inc(int a) { return a + 1; }

void test(unary_f _Nonnull fn) {
  _Safe {
    int a = 10;
    
    int *b = &a; // error: 安全区不允许取地址符号
    int c = *b; // error: 安全区不允许解引用裸指针

    int *_Owned d = safe_malloc(2);
    int e = *d; // ok: 允许解引用owned指针
    safe_free((void *_Owned)d);

    int *_Borrow f = &_Mut a;
    int g = *f; // ok: 允许解引用borrow指针

    int h = (*fn)(1);  // ok: 允许解引用函数指针
  }
}

int main() {
  test(inc);
  return 0;
}
```

17. 安全区内不允许指向类型不同的指针类型之间转换，但有以下例外：
    1. 允许指向其他类型的owned指针显式转换为指向void类型的owned指针，该转换需要符合[所有权状态转移规则-强制类型转换](../chapter-3-memory-safety/1-ownership.md#强制类型转换)的规则。
    2. 允许指向其他类型的borrow指针隐式转换为指向void类型的borrow指针，但原类型必须满足 is_trivial_data 的条件（不含指针和 _Owned struct），否则不允许转换。

    安全区内不允许指针和非指针类型之间的转换。数组默认可以退化到裸指针；在赋值、函数传参和返回值场景中，如果目标类型是与数组元素类型匹配的 `T *_Borrow` 或 `T *_Borrow _ArrayElem`，则数组也允许退化到对应的借用指针。除这一数组退化规则外，安全区内不允许`_Owned/_Borrow/raw`指针之间的转换；另外，允许 `T *_Borrow _ArrayElem` 转换为 `T *_Borrow`。

```c
void test() {
  int *pa;
  double *pb;
  _Safe {
    pb = pa; // error：不允许指向类型不同的指针类型之间转换
    pa = pb; // error：不允许指向类型不同的指针类型之间转换
    pb = (double *)pa; // error：不允许指向类型不同的指针类型之间转换
  }
  int i;
  _Safe {
    pa = i; // error：不允许指针和非指针类型之间的转换
    i = pa; // error：不允许指针和非指针类型之间的转换
  }
  int *_Owned pd = (int *_Owned)pa;
  _Safe {
    pa = pd; // error：不允许 _Owned/raw 指针之间的转换
    pd = pa; // error：不允许 _Owned/raw 指针之间的转换

    void *_Owned pe = (void *_Owned)pd; // ok: 允许显式转换为void类型的owned指针
  }
  struct S {int *ptr;};
  struct S s = {.ptr = nullptr};
  int a = 0;
  _Safe {
    int *_Borrow p1 = &_Mut a;
    void *_Borrow p2 = p1; // ok: int 满足 is_trivial_data 约束，允许 int *_Borrow 隐式转换到 void *_Borrow
    struct S *_Borrow p3 = &_Mut s;
    void *_Borrow p4 = (void *_Borrow)p3; // error: struct S 不满足 is_trivial_data，不允许 struct S *_Borrow 转换到 void *_Borrow
  }
}

int main() {
  test();
  return 0;
}
```

```c
_Safe void f1(int *_Borrow _ArrayElem p) {}
_Safe void f2(int *_Borrow p) {}
_Safe void f3(int *) {}

typedef struct {
  int data[4];
} S;

_Safe int *_Borrow _ArrayElem f4(S *_Borrow s) {
  return s->data; // ok: 数组退化为 T *_Borrow _ArrayElem
}

_Safe int *_Borrow f5(S *_Borrow s) {
  return s->data; // ok: 数组退化为 T *_Borrow
}

_Safe int * f6(S *_Borrow s) {
  return s->data; // ok: 数组退化为 T *
}

_Safe void test_decay(void) {
  int arr[4] = {1, 2, 3, 4};
  f1(arr); // ok
  f2(arr); // ok
  f3(arr); // ok
  int *_Borrow _ArrayElem p = arr; // ok
  int *_Borrow q = arr; // ok
  int * r = arr; // ok
}
```

18. 安全区内不允许**隐式执行**表达范围从大向小的类型转换（比如从`long`转换为`int`，从`int`转换为`_Bool`，从`int`转换为`enum`），不允许**隐式执行**表达精度从高向低的类型转换（比如从`double`转换为`float`）。安全区内不允许将一个算术类型转换为一个枚举类型，除非是从一个枚举类型显式转换为一个表达范围更大或相等的枚举类型。安全区内不允许从浮点类型转换到整数类型。对于**编译期能确定值的常量**发生类型转换，如果目标类型可以描述这个值，那么在安全区内该类型转换可以隐式执行，否则此类转换必须显式执行且需符合上述规则。

    **语义规则与具体说明：**
    排除枚举类型时，安全区允许执行的类型转换如下图所示：（若同方向支持隐式转换则一定支持显式转换；只写需显式转换则说明不允许隐式转换。表示允许转换的边有一头连着子图时，等效于连接该子图的所有结点，比如 `_Bool` 有条边指向整数，表示允许 `_Bool` 以相同规则转换为任一整数类型。）

    ``` mermaid
    graph TB
      bool["_Bool"]
      subgraph int["整数"]
        direction LR
        subgraph signedint["有符号整数"]
          direction TB
          signedint_s["小类型"]
          signedint_l["大类型"]
          signedint_s -- 正向隐式，反向显式 --> signedint_l
        end
        subgraph unsignedint["无符号整数"]
          direction TB
          unsignedint_s["小类型"]
          unsignedint_l["大类型"]
          unsignedint_s -- 正向隐式，反向显式 --> unsignedint_l
        end
        signedint <-- 需显式互转 --> unsignedint
      end
      bool -- 正向隐式，反向显式 --> int
      subgraph fp["浮点数"]
      direction TB
        fp_s["小类型"]
        fp_l["大类型"]
        fp_s -- 正向隐式，反向显式 --> fp_l
      end
      int -- 正向显式，反向不允许 --> fp
    ```

    18.1. 当将一个算术类型转换为 `_Bool` 时，当原类型的值为零，则转换后值为 0；否则，转换后值为 1 （与C一致）。

    ```c
    int b = 0;
    _Bool c = (_Bool)b; // ok: int -> _Bool; c = 0
    c = (_Bool)1;       // ok: int -> _Bool; c = 1
    c = (_Bool)2;       // ok: int -> _Bool; c = 1
    ```

    18.2. 将一个有符号整数类型或一个表达范围更大的无符号整数类型转换为一个**无符号**整数类型时，如果原类型的值的大小在目标类型的范围内，则转换后值的大小不变；否则，转换后值的大小为"不断对原值加或减(1+目标类型的最大值)直到结果落在目标类型的范围内的值" （与C一致）。如果目标无符号整数类型的表达范围是从 0 到 $2^N-1$ 且原类型的值为 $X$，那么转换后值的大小为 $X$ 不断加或减 $2^N$ 直到结果 $Y$ 落入 0 到 $2^N-1$ 之间，转换后的值即为 $Y$。上述表示等效于将大的整数类型截断为小的无符号整数类型，或是将小的有符号整数类型通过符号位拓展转为无符号整数类型。

    ```c
    unsigned long a = 2;
    unsigned b = (unsigned) a; // ok: unsigned long -> unsigned int
    int c = -1;
    unsigned d = (unsigned) c; // ok: int -> unsigned int; d = 2^32 - 1
    unsigned long e = (unsigned long) c; // ok: int -> unsigned long; e = 2^64 - 1
    ```

    18.3. 将一个表达范围更大的整数类型转换为表达范围更小的**有符号**整数类型（不包括枚举类型）时，如果原类型的值的大小在目标类型的范围内，则转换后值的大小不变；否则，转换后值的大小由实现定义。在当前毕昇C编译器的实现中，原值超过目标类型的表示范围时，大的整数类型会截断为小的有符号整数类型。

    ```c
    long a = 2;
    int b = (int) a; // ok: long -> int
    unsigned c = 4294967295; // c = 2^32 - 1
    int d = (int) c; // ok: unsigned int -> int; d = -1
    ```

    18.4. 关于枚举类型：安全区只允许从一个枚举类型显式转换为另一个枚举类型（有限定条件），其他任何类型都不能转换为枚举类型。可以从枚举类型隐式转换为其对应的整数类型（比如有 `enum E : unsigned {...}` 就可以把 `enum E` 隐式转换为 `unsigned`）。使用显式转换将枚举类型 `E1` 转换为另一个枚举类型 `E2` 时，当且仅当 `E2` 中包含 `E1` 所有枚举值时允许这样的显式转换，转换后的值不变，否则会在编译时报错。安全区内不能隐式执行这样的转换。其他任何表达范围更大的类型转换到枚举类型的操作在安全区都不允许。在非安全区则与 C 一致，不对枚举类型的转换做额外检查。

    ```c
    _Safe void foo(void) {
      enum E {ZERO, ONE, TWO}; // represented by int
      enum F {SUN, MON, TUES}; // represented by int
      enum F day = SUN;
      enum E num = (enum E)day; // ok: enum F -> enum E
      num = (enum E)SUN;        // ok: enum F -> enum E
      int a = num; // ok: enum E -> int
      a = ZERO;    // ok: enum E -> int
      num = 0; // error: cannot cast int to enum E in safe zone
      enum G {G1, G2};
      enum G g1 = (enum G) num; // error: cannot cast enum E to enum G in safe zone
      g1 = (enum G) ZERO; // error: cannot cast enum E to enum G in safe zone
    }
    ```

    18.5. 在安全区使用显式转换将一个浮点类型转换为精度更低的另一个浮点类型时，如果原值能在目标类型中精确表示，则转换后的值不变。如果原值在目标类型的表示范围内但是无法精确表示，则转换后的结果是最近的向上或向下近似的值，取决于具体实现。如果原值在目标类型的表示范围外，则具体值取决于具体实现。当前毕昇C编译器的实现应符合 IEEE 754 规范。安全区内不能隐式执行这样的类型转换。在非安全区则行为与 C 一致，语义相同且支持隐式执行这样的类型转换。

    ```c
    _Safe {
      double a = 1.0;
      float b = 0.0f;
      b = (float)a; // ok: double -> float
      b = a; // error: cannot implicitly cast double to float
      a = b; // ok
    }
    ```

    18.6. 在安全区使用显式转换将一个整数类型转换为浮点类型时，如果原值能在目标类型中精确表示，则转换后的值不变。如果原值在目标类型的表示范围内但无法精确表示，则转换后的结果是最近的向上或向下近似的值，取决于具体实现。当前毕昇C编译器的实现应符合 IEEE 754 规范、使用 IEEE 754 中的默认舍入规则。安全区内不能隐式执行这样的类型转换。安全区内不允许从浮点类型转换为整数类型。在非安全区则行为与 C 一致，语义相同且支持隐式执行这样的类型转换。

    ```c
    _Safe {
      int a = -1;
      double b = a; // error: cannot implicitly cast int to double in safe zone
      b = (double) a; // ok: int -> double
      a = (int) b; // error: cannot cast double to int in safe zone
    }
    ```

    18.7. 比较运算符（`==`, `!=`, `>=`, `<=`, `>`, `<`）、逻辑运算符（`&&`, `||`, `!`）的运算结果为 `int` 类型的 0 或 1，这些值允许在安全区隐式转换到其他整数类型（因为任何基本整数类型都可以表示 0 和 1）。

    ```c
    _Safe {
      int a = 0, b = 1;
      _Bool c = (a == 0) || (b == 0); // ok: int (0/1) -> _Bool
      char x = a < 3 || a >= 6; // ok: int (0/1) -> char
    }
    ```

    18.8. 对于**编译期能确定值的常量**发生类型转换，若目标类型可精确表示该值，则允许隐式转换；否则需显式转换并符合上述规则。

    ```c
    _Safe {
      int a = 10l; // ok: long -> int
      unsigned long b = 2*2; // ok: int -> unsigned long

      // int -2147483648~2147483647
      int c = 2147483648; // error: 2147483648 out of range for int
      int d = (int)2147483648; // ok: long -> int; d = -2147483648
      int e = 2147483647; // ok

      // unsigned int 0~4294967295
      unsigned int f = 0;
      f = 4294967296; // error: 4294967296 out of range for unsigned int
      f = (unsigned int) 4294967296; // ok: long -> unsigned int; f = 0

      unsigned long g = (unsigned long)-1; // ok: int -> unsigned long; g = 2^64-1
      int h = (int) 1.2; // error: cannot cast double to int in safe zone
      float k = 1.0; // ok: double -> float
      float l = 1; // ok: int -> float
    }
    ```

    18.9.  `if/while` 的条件对类型的要求目前与C一致，任何算术类型都可以作为条件。

    ```c
    int a = 2;
    _Safe {
      if (a) { // ok: if 可以接受 int
        a += 1;
      } else {
        a -= 1;
      }
    }
    ```

2.  安全区内不允许内嵌汇编语句。

```c
void test() {
  _Safe {
    int ret = 0;
    int src = 1;
    asm("move %0, %1\n\t" : "=r"(ret) : "r"(src)); // error: 安全区不允许内嵌汇编
  }
}
int main() { return 0; }
```

21. 安全区内，前缀/后缀自增（`++`）和自减（`--`）表达式的结果类型为 `void`，即仅允许使用其副作用（自增或自减1），不得使用 `++`/`--` 表达式的值。在返回类型为 `void` 的安全函数中，不得将 `++`/`--` 的结果直接作为 return 语句的返回值。

```c
safe void take_int(int x);
safe void foo(void) {
  int a = 0;
  a++; // ok: 安全区允许自增自减（仅副作用，表达式结果为 void）
  a--;
  int x = a++; // error: 自增自减的结果为 void 类型，不能使用其值初始化 int 变量
  int arr[5] = {1, 2, 3, 4, 5};
  arr[a++] = 0; // error: 自增自减的结果为 void 类型，不能使用其值做为 arr[] 下标
  take_int(a++); // error: 自增自减的结果为 void 类型，不能使用其值做为 take_int() 入参
  unsafe {take_int(a++);} // ok: 非安全区不受影响
  for (int i = 0; i < 10; i++) {} // ok: for 循环的迭代部分可以使用 ++/--
  int y = 0;
  y = (a++, a); // ok: a++ 先求值、完成自增，再对逗号右侧的 a 求值
  return a++; // error: 不得将 ++/-- 的结果直接作为返回值
  return (void)a++; // ok
}

```
