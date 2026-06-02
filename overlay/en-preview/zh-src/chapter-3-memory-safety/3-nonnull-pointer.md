# 非空指针

为了提高指针使用的安全性，BSC 引入了指针的可空性（Nullability）概念。

根据是否可以直接解引用使用，可以使用两种属性修饰任意指针类型（裸指针、`_Owned` 和 `_Borrow` 指针）。

1. 被关键字 `_Nullable` 修饰的是可空指针。
2. 被关键字 `_Nonnull` 修饰的是非空指针。

BSC 编译器会在编译时检查指针的可空性，避免出现解引用空指针、通过空指针访问成员等不安全行为。

BSC 不允许在安全区内使用宏 `NULL`，但是 C 风格编程不可避免地使用空指针来描述逻辑：

1. 一个指针的指向需要在运行时才能确定，那么我们可以将指针初始化为空指针，在后续再根据运行状态来修改指向
2. 对于可空指针，在使用前需要判断该指针是否为空指针

BSC 引入了 `nullptr` 关键字来替代 `NULL`。
用户可以定义 Nullable 指针，并将其初始化为 `nullptr`。
需要对 Nullable 指针判空才能解引用。

我们用一个简单的例子来学习如何定义指针的 nullabiliy 并使用它：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

// 获取当前运行状态的函数
_Safe int get_current_status(void) {
  _Unsafe { return rand() % 2; }
}

_Safe void read_data<T>(T *_Borrow p) {}

struct Data {
  int value;
};

// 如果 init 成功，返回具体的地址，否则返回 nullptr
_Safe struct Data *_Owned _Nullable init_data(void) {
  if (get_current_status() == 1) {
    struct Data data = {10};
    struct Data *_Owned p = safe_malloc<struct Data>(data);
    return p;
  }
  return nullptr;
}

_Safe int main(void) {
  // 使用 _Nullable 修饰指针类型：
  struct Data *_Owned _Nullable p = init_data();
  // init 后 p 可能为空指针，因此需要先判空再使用：
  if (p != nullptr) {
    // 如果没有对 p 做判空，编译器会报 error!
    read_data(&_Mut * p);
    safe_free((void *_Owned)p);
  }
  return 0;
}
```

## 编译器可以检查哪些指针的可空性

可以用 `_Nullable` 和 `_Nonnull` 修饰任意指针类型（裸指针、`_Owned` 和 `_Borrow` 指针），
但只有指针满足以下条件时，编译器可以对它的可空性进行跟踪，从而保护空指针解引用。

1. 是左值（存在一个内存地址）
2. 不被 `volatile` 修饰（指针自己不是 volatile）
3. 不是通过访问数组得到的指针

不满足以上要求的指针无法通过判空操作修改指针的可空性。如果不可跟踪的指针 `p1` 为 `_Nullable` 且用户需要对其解引用，请先创建一个可跟踪的指针变量 `p2` 并用 `p1` 对其初始化，再对 `p2` 进行判空、解引用。

例子：

```c
// 函数的返回值不是左值
_Safe int *_Borrow _Nullable identity(int *_Borrow _Nullable p) { return p; }

_Safe void test(int *_Borrow _Nullable p, int *_Borrow _Nullable volatile vp) {
  int local = 0;
  int *_Borrow _Nullable q = nullptr;
  int *_Borrow _Nullable arr[2] = {nullptr, &_Mut local};

  if ((q = p) != nullptr) {
    *q = 1; // ok：q 是左值指针，编译器可跟踪其可空性
  }

  if ((0, q) != nullptr) {
    *q = 2; // ok：逗号表达式最终提取到 q，仍可跟踪
  }

  if (identity(p) != nullptr) { // 不是左值，无法更新其可空性
    *identity(p) = 3; // error
  }
  int *_Borrow _Nullable t1 = identity(p); // 应创建可跟踪的临时变量去接
  if (t1 != nullptr) {
    *t1 = 3; // ok
  }

  if (vp != nullptr) { // volatile 指针，无法更新其可空性
    *vp = 4; // error
  }
  int *_Borrow _Nullable t2 = vp; // 应创建可跟踪的临时变量去接
  if (t2 != nullptr) {
    *t2 = 4; // ok
  }

  if (arr[1] != nullptr) { // 指针表达式中有下标运算 '[]'，无法更新指针的可空性
    *arr[1] = 5; // error
  }
  int *_Borrow _Nullable t3 = arr[1];  // 应创建可跟踪的临时变量去接
  if (t3 != nullptr) {
    *t3 = 5; // ok
  }
}
```

## 指针类型的 Nullability

指针类型的可空性标记和指针的类型以及指针的修饰符有关：

1. 裸指针是 Nullable 的， `_Owned` 和 `_Borrow` 是 Nonnull 的
2. 可以显式地使用 `_Nonnull` 和 `_Nullable` 修饰符覆盖上述规则

```C
// Nullable 指针：
int *_Nullable p1 = nullptr;
int *_Borrow _Nullable p2 = nullptr;
int *_Owned _Nullable p3 = nullptr;
int *p4 = nullptr;
int (*p5)(int) = nullptr;
// Nonnull 指针：
int *_Nonnull p5 = &a;
int *_Borrow _Nonnull p6 = &_Mut a;
int *_Owned _Nonnull p7 = safe_malloc<int>(5);
int *_Borrow p8 = &_Mut a;
int *_Owned p9 = safe_malloc<int>(5);
```

对于可空性标记为 Nonnull 的指针，它的可空性状态（Nullability）一定是 Nonnull 的。
如果在控制流语句中对其做了判空，那么在空的分支中，其被认为是空指针。

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

_Safe int test(void) {
  int * _Owned a = safe_malloc<int>(1); // _Owned 指针默认为 _Nonnull
  if (a != nullptr) {
    safe_free((void * _Owned)a);
    return 1;
  }
  return 0; // ok 不会有 error: memory leak of value: `a`
}

int main() {
  test();
  return 0;
}
```

对于可空性标记为 Nullable 的指针，它的可空性状态（Nullability）可能会发生变化：

1. 如果用非空表达式对其进行了赋值，那么在这条赋值语句之后可空性状态变为 Nonnull
2. 如果在控制流语句中对其做了判空，那么在非空的分支中可空性状态变为 Nonnull

```C
// 返回值类型为 Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }
// 返回值类型为 Nonnull
_Safe T *_Borrow bar<T>(T *_Borrow p) { return p; }

_Safe void test(void) {
  int *_Borrow _Nullable p1 = nullptr; // p1 初始化后 Nullability 为 Nullable
  *p1 = 10;                           // error

  int local = 10;
  p1 = &_Mut local;      // p1 被再赋值后 Nullability 变为 Nonnull
  *p1 = 20;             // ok

  p1 = foo(&_Mut local); // p1 被再赋值后 Nullability 变为 Nullable
  *p1 = 20;             // error
  
  p1 = bar(&_Mut local); // p1 被再赋值后 Nullability 变为 Nonnull
  *p1 = 20;             // ok

  int *_Borrow _Nullable p2 = foo(&_Mut local); // Nullable
  if (p2 != nullptr) // if 分支中 p2 的 Nullability 为 Nonnull
    *p2 = 10;        // ok
  else               // else 分支中 p2 的 Nullability 为 Nullable
    *p2 = 20;        // error
}

int main() {
  test();
  return 0;
}
```

## 什么是判空语句

在条件语句、循环语句的条件和三元表达式的条件表达式中，只有部分行为可以被视作“判断指针是否为空指针”：

1. 直接使用指针 `e`

- `if (p)`
- `while (s.p)`
- `p ? do_with(p) : do_without()`

2. 逻辑运算符 `!e` `e && f` `e || f`

- `if (!p)` `if (p && q)` `if (p || q)`
- `while (!s.p)`  `while (s.p && s.q)` `while (s.p || s.q)`

3. 显式和空指针做比较 `e == nullptr` `e != nullptr`

- 比较运算符具有对称性，因此 `nullptr == e` 和 `nullptr != e` 也支持

4. 可以出现赋值运算符 `e = ...`

- `if (p = q)` 只能判断 `p` 的可空性
- 赋值运算符可结合，但 `if (p = q = r)` 只能判断 `p` 的可空性

5. 可以作为逗号表达式的一部分，即 `(..., e)`

- `if (x, p != nullptr)` 可以判断 `p` 的可空性
- `if (p != nullptr, q != nullptr)` 只能判断  `q` 的可空性

6. 上述模式可以嵌套括号

其中 `e` 需要满足 3.3.1 中“可被跟踪”的要求。

对这些“判空语句”，编译器会在在后续控制流中更新 `e` 的可空性状态：

- 在 `if (e)` / `while (e)` 的真分支（或循环体）里，`e` 视作 Nonnull
- 在 `if (!e)` 的假分支里，`e` 视作 Nonnull
- 在 `if (e != nullptr)` 的真分支里，`e` 视作 Nonnull
- 在 `if (e == nullptr)` 的假分支里，`e` 视作 Nonnull

```c

_Safe void test(int *_Borrow _Nullable p, int *_Borrow _Nullable q) {
  // 1) 直接指针条件
  if (p) {
    *p = 1; // ok: 真分支里 p 视作 nonnull
  }

  // 2) 逻辑运算符
  if (!p) {
    *p = 1; // error: 真分支里 p 视作 nullable
  }
  if (p && q) {
    *p = 1; // ok
    *q = 1; // ok
  }
  if (!p || !q) {
    // snip
  } else {
    *p = 1; // ok
    *q = 1; // ok
  }

  // 3) 与 nullptr 比较
  if (p != nullptr) {
    *p = 1; // ok
  }
  if (nullptr == p) {
    *p = 1; // error
  }

  // 4) 赋值/逗号包裹
  if ((q = p) != nullptr) {
    *q = 1; // ok
  }
  if ((0, q) != nullptr) {
    *q = 1; // ok
  }
}
```

## 指针的赋值、传参和返回

1. 不允许用可空表达式给可空性标记为 Nonnull 的指针赋值：

```C
// 返回值类型为 Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }
// 返回值类型为 Nonnull
_Safe T *_Borrow bar<T>(T *_Borrow p) { return p; }

_Safe void test(void) {
  int *_Borrow p1 = nullptr; // error: 安全区内禁止用 nullable 值给 nonnull 指针赋值

  int local = 10;
  int *_Borrow p2 = foo(&_Mut local); // error: 安全区内禁止用 nullable 值给 nonnull 指针赋值
  int *_Borrow p3 = bar(&_Mut local); // ok

  int *_Borrow _Nullable p4 = foo(&_Mut local);
  int *_Borrow p5 = p4; // error: 安全区内禁止用 nullable 值给 nonnull 指针赋值
}

int main() {
  test();
  return 0;
}
```

2. 函数调用时，如果形参类型具有 Nonnull 可空性标记，那么不能传递可空表达式作为实参：

```C
// 返回值类型为 Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }

// 接收 Nonnull 类型的指针作为参数
_Safe void bar(int *_Borrow p) {}

_Safe void test(void) {
  int local = 10;
  int *_Borrow _Nullable p = foo(&_Mut local);
  bar(p); // error: 要求 nonnull 指针，但实参是 nullable
}

int main() {
  test();
  return 0;
}
```

3. 函数返回值类型如果具有 Nonnull 可空性标记，不能返回可空表达式的结果：

```C
_Safe int *_Borrow return_nonnull(int *_Borrow p) {
  int *_Borrow _Nullable q = nullptr;
  return q; // error: 要求返回指针是 nonnull
}
int main() {
  int i = 1;
  int *_Borrow pi = return_nonnull(&_Mut i);
  return 0;
}
```

## 指针的强制类型转换

在开启编译选项`-nullability-check=all`后，在非安全区中不允许将 Nullable 指针强制类型转换为 Nonnull 类型：

```C
void foo() {
  int *p1 = nullptr;
  int *p2 = (int *_Nonnull)p1; // error: cannot cast nullable pointer to nonnull type
  int *_Owned p3 = (int *_Owned)p1; // error: cannot cast nullable pointer to nonnull type
  int *_Borrow p4 = (int *_Borrow)p1; // error: cannot cast nullable pointer to nonnull type
}

int main() {
  foo();
}
```

对 Nullable 指针进行判空，那么在非空的分支中，可以强制类型转换为 Nonnull 类型：

```C
void foo() {
  int *p1 = nullptr;
  if (p1 != nullptr) {
    int *p2 = (int *_Nonnull)p1;
    int *_Owned p3 = (int *_Owned)p1;
    int *_Borrow p4 = (int *_Borrow)p1;
    safe_free((void *_Owned)p3);
  }
}

int main() {
  foo();
}
```

## 指针的解引用、成员访问

可空性标记为 Nullable 的指针的可空性状态（Nullability）会随赋值和控制流发生变化，
BSC 编译器会跟踪这些变换，保证指针解引用、成员访问等操作的安全性。

```C
// 返回值类型为 Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }

struct Data {
  int value;
};

_Safe void test(void) {
  struct Data data = {.value = 10};
  struct Data *_Borrow _Nullable p = foo(&_Mut data);
  if (p != nullptr) {
    // 经过判空后， nullable 会转换为 nonnull
    p->value = 10;
  }
}

int main() {
  test();
  return 0;
}
```

## 结构体成员是 Nullable 指针

初始化有 Nullable 指针成员的结构体变量时：

1. 如果是通过初始化列表进行初始化，BSC 编译器会根据初始化表达式来初始化 Nullable 指针成员的 Nullability
2. 对于其它初始化方式，直接认为 Nullable 指针成员的 Nullability 为 Nullable 的，这可能会导致本身没有问题的代码无法通过编译，此时需要对 Nullable 指针成员作再赋值，或使用判空语句，即可改变 Nullability。

```C
// 返回值类型为 Nonnull
_Safe T *_Borrow bar<T>(T *_Borrow p) { return p; }

struct Data {
  int *_Borrow _Nullable value;
};

_Safe struct Data init_data(int *_Borrow p) {
  struct Data d = {.value = p};
  return d;
}

_Safe void test(void) {
  int local = 10;
  // 使用初始化列表做初始化： 推断为 nonnull
  struct Data data1 = {.value = bar(&_Mut local)};
  *data1.value = 10;

  // 使用函数返回值做初始化： 无法推断 nullability 默认为 nullable
  struct Data data2 = init_data(&_Mut local);
  *data2.value = 10; // error: 直接使用 nullable 指针

  // 使用变量赋值做初始化： 无法推断 nullability 默认为 nullable
  struct Data data3 = data1;
  *data3.value = 10; // error: 直接使用 nullable 指针

  // 对 Nullable 指针成员作再赋值，可以改变 Nullability：
  data2.value = bar(&_Mut local);
  *data2.value = 10;

  // 通过指针判空语句，也可以改变 Nullability：
  if (data3.value != nullptr)
    *data2.value = 10;
}

int main() {
  test();
  return 0;
}
```

## 非空指针检查的范围和控制选项

非空指针检查是一项强大的功能，能帮助开发者在编译期识别出潜在的危险行为。同时，这会带来一定的编译性能开销和编码行为限制。**默认情况下，对非空指针的检查仅在安全区生效**，安全区的定义详见[安全区](../chapter-3-memory-safety/5-safe-zone.md)章节。

在非安全区，我们将是否开启非空指针检查的选择权交给开发者，即通过编译选项`-nullability-check=value`来控制该项检查的作用域。其中`value`是一个枚举值，有2种选项`safeonly`, `all`：

1. `value`的默认值为`safeonly`，控制着检查只在安全区生效；无该编译选项时，等同`-nullability-check=safeonly`。
2. `value`值为`all`时，将整体使能非空指针检查，安全区与非安全区均开启检查。

提供一个示例说明：

```C
_Safe void test(void) {
  int *_Borrow _Nullable p = nullptr;
  _Unsafe {
    *p = 10; // error1: nullable pointer cannot be dereferenced
  }
  *p = 5; // error2: nullable pointer cannot be dereferenced
}

int main() {
  test();
  return 0;
}
```

对于上面这个示例，当编译选项`-nullability-check`不存在或者`-nullability-check=safeonly`时，只有在`_Safe`区的`error2`会被报告；当`-nullability-check=all`时，非安全区的`error1`和安全区`error2`均会被报告。
