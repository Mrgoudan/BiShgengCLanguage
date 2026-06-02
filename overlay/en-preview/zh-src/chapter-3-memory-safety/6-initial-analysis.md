# 初始化分析

## 概述

在 C 语言中，使用未初始化的变量是一种常见的未定义行为。编译器可能不会对此产生警告，但未初始化的值会导致不可预测的程序行为、安全漏洞或崩溃。

毕昇 C 在安全区（`_Safe`）中引入了**初始化分析**（Initialization Analysis），在编译期通过数据流分析确保每个变量在使用前都已被确定初始化。该分析支持结构体字段级别的精确追踪。

初始化分析默认在 `_Safe` 区域内生效，也可通过 `-uninit-check=all` 选项扩展到所有代码区域，或通过 `-uninit-check=none` 禁用。

## 代码示例

```c
_Safe void example(int cond) {
    int x;
    if (cond) {
        x = 42;
    }
    int y = x; // error: use of possibly uninitialized value: `x`
}

_Safe void example_ok(int cond) {
    int x;
    if (cond) {
        x = 1;
    } else {
        x = 2;
    }
    int y = x; // ok: x 在所有路径上都已初始化
}
```

## 语法及语义规则

1. 在安全区中，所有局部变量（标量、指针、结构体等）在使用前必须被确定初始化。未初始化或仅在部分路径上初始化的变量会导致编译错误。

```c
_Safe void rule1(void) {
    int x;
    int y = x; // error: use of uninitialized value: `x`
}

_Safe void rule1_ok(void) {
    int x = 0;
    int y = x; // ok
}
```

2. 对结构体类型，分析精确追踪每个字段的初始化状态。部分字段未初始化时，整体使用该结构体会报错。当所有字段都被初始化后，编译器会自动将整个结构体提升为已初始化状态。

```c
struct Pair { int a; int b; };

_Safe void rule2_partial(void) {
    struct Pair p;
    p.a = 1;
    // p.b 未初始化
    struct Pair q = p; // error: use of uninitialized value: `p`
}

_Safe void rule2_full(void) {
    struct Pair p;
    p.a = 1;
    p.b = 2;
    struct Pair q = p; // ok: p 的所有字段都已初始化，自动提升为整体已初始化
}
```

3. 控制流分支中的初始化必须覆盖所有路径。如果变量仅在部分分支中被初始化，分析会报告"可能未初始化"。

```c
_Safe void rule3(int cond) {
    int x;
    if (cond) {
        x = 1;
    }
    // else 分支未初始化 x
    int y = x; // error: use of possibly uninitialized value: `x`
}

_Safe void rule3_ok(int cond) {
    int x;
    if (cond) {
        x = 1;
    } else {
        x = 2;
    }
    int y = x; // ok
}
```

4. 取地址操作（`&_Mut`、`&_Const`、`&`）被视为对变量的使用。对未初始化的变量取地址会报错。例外：作为 `ensure_init` 参数或 `__assume_initialized` 参数的取地址表达式不受此限制（详见 3.6.4 和 3.6.5）。

```c
_Safe void rule4(void) {
    int x;
    int *_Borrow p = &_Mut x; // error: use of uninitialized value: `x`
}

_Safe void rule4_ok(void) {
    int x = 0;
    int *_Borrow p = &_Mut x; // ok
}
```

5. 有返回值的函数必须在所有路径上初始化返回值。

```c
_Safe int rule5(int cond) {
    if (cond) {
        return 1;
    }
    // 其他路径未 return
} // error: return value of `rule5` may not be initialized on all paths
```

6. 数组元素的逐个赋值**不会**将数组标记为已初始化。数组必须通过初始化列表或在 `_Unsafe` 区域中使用 `__assume_initialized` 来初始化（详见 3.6.5）。此规则同样适用于结构体中的数组字段——对数组元素的逐个写入不会将该字段标记为已初始化。

```c
_Safe void rule6_error(void) {
    int arr[3];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    int x = arr[0]; // error: use of uninitialized value: `arr`
}

_Safe void rule6_ok_init_list(void) {
    int arr[3] = {1, 2, 3};
    int x = arr[0]; // ok: 通过初始化列表初始化
}

_Safe void rule6_ok_assume(void) {
    int arr[3];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    _Unsafe { __assume_initialized(&arr); }
    int x = arr[0]; // ok: 通过 __assume_initialized 标记
}

// 结构体中的数组字段同理
typedef struct { int a[2]; int b; } ArrStruct;

_Safe void rule6_struct_error(void) {
    ArrStruct s;
    s.a[0] = 1;
    s.a[1] = 2;
    s.b = 3;
    ArrStruct t = s; // error: use of uninitialized value: `s.a`
}

_Safe void rule6_struct_ok(void) {
    ArrStruct s = {{1, 2}, 3};
    ArrStruct t = s; // ok: 通过初始化列表初始化
}
```

7. 联合体（union）写入任一成员会将整个联合体标记为已初始化（符合 C 语言联合体语义：写入任一变体覆盖全部字节）。对于联合体中包含结构体变体的情况，写入结构体的某个字段也会将整个联合体标记为已初始化，跨变体读取也是允许的。

注：由于安全区内不允许直接访问联合体字段，以下示例使用非安全函数配合 `-uninit-check=all` 来展示联合体的初始化行为。

```c
// 编译选项: -uninit-check=all
union U { int a; float f; };

void rule7(void) {
    union U u;
    u.a = 42;
    float f = u.f; // ok: 联合体已通过 u.a 初始化
}

// 联合体包含结构体变体
struct S { int x; int y; };
union US { int a; struct S s; };

void rule7_struct(void) {
    union US u;
    u.s.x = 1;      // 写入结构体变体的字段 → 整个联合体标记为已初始化
    int v = u.a;     // ok: 跨变体读取，联合体已初始化
}
```

> **已知限制**：写入一个变体会将整个联合体标记为已初始化，因此跨变体读取结构体字段时，即使对应的字节实际上未被写入，编译器也不会报错。例如：
>
> ```c
> struct S2 { int b; int c; };
> union U2 { int a; struct S2 s; };
>
> void example(void) {
>     union U2 u;
>     u.a = 1;
>     int v = u.s.c; // 编译通过，但 u.s.c 的字节实际上可能未被有意义地写入
> }
> ```

8. 嵌套结构体支持任意深度的字段级追踪。当所有叶子字段都被初始化后，父字段和整个结构体会自动提升为已初始化状态。

```c
struct Inner { int x; int y; };
struct Outer { struct Inner inner; int z; };

_Safe void rule8(void) {
    struct Outer o;
    o.inner.x = 1;
    o.inner.y = 2;  // inner 的所有字段已初始化 → inner 自动提升
    o.z = 3;        // 所有字段已初始化 → o 自动提升
    struct Outer p = o; // ok
}
```

9. 全局变量和静态变量被视为隐式已初始化（由 C 语言规范保证零初始化），不需要显式初始化。

```c
static int global_count;

_Safe void rule9(void) {
    int x = global_count; // ok: 全局/静态变量隐式已初始化
}
```

## `__attribute__((ensure_init))`

`__attribute__((ensure_init))` 是一个参数属性，用于标注指针参数，建立初始化契约：

- **调用端**：调用后，被指向的变量标记为已初始化
- **被调用端**：编译器验证函数确实在所有返回路径上初始化了 `*param`

```c
void init_int(int *__attribute__((ensure_init)) out);

_Safe void caller(void) {
    int x;                    // 未初始化
    _Unsafe { init_int(&x); } // 调用后 x 被标记为已初始化
    int y = x;                // ok
}

// 编译器验证被调用端的契约
void good_init(int *__attribute__((ensure_init)) out) {
    *out = 42; // ok: 在返回前初始化了 *out
}

void bad_init(int *__attribute__((ensure_init)) out) {
} // error: __attribute__((ensure_init)) parameter 'out' not initialized at return
```

字段级别的部分初始化也受支持：

```c
struct Pair { int a; int b; };
void init_field(int *__attribute__((ensure_init)) p);

_Safe void field_level(void) {
    struct Pair p;
    _Unsafe {
        init_field(&p.a);  // 仅标记 p.a 为已初始化
        init_field(&p.b);  // 标记 p.b 为已初始化 → p 整体自动提升为已初始化
    }
    struct Pair q = p; // ok
}
```

```c
_Safe void init_safe(int *_Borrow __attribute__((ensure_init)) out);

_Safe void caller_safe(void) {
    int x;
    init_safe(&_Mut x);  // ok: 在安全区中使用 &_Mut
    int y = x;            // ok
}
```

`ensure_init` 参数在 `*param` 初始化完成前不允许被重新赋值或复制到其他变量（别名）：

```c
void bad_reassign(int *__attribute__((ensure_init)) out) {
    int local;
    out = &local; // error: __attribute__((ensure_init)) parameter 'out' cannot be reassigned or aliased before '*out' is initialized
} // error: __attribute__((ensure_init)) parameter 'out' not initialized at return

void bad_alias(int *__attribute__((ensure_init)) out) {
    int *p = out; // error: __attribute__((ensure_init)) parameter 'out' cannot be reassigned or aliased before '*out' is initialized
    *out = 42;
}
```

初始化完成后可以自由重新赋值或别名：

```c
void ok_reassign(int *__attribute__((ensure_init)) out) {
    *out = 42;         // 契约已履行
    int local = 10;
    out = &local;      // ok: *out 已初始化，指针可以自由使用
}

void ok_alias(int *__attribute__((ensure_init)) out) {
    *out = 42;         // 契约已履行
    int *p = out;      // ok: *out 已初始化
}
```

`ensure_init` 参数可以委托给另一个 `ensure_init` 函数，编译器会追踪委托链：

```c
void init_val(int *__attribute__((ensure_init)) out);

void init_delegated(int *__attribute__((ensure_init)) out) {
    init_val(out); // ok: 委托给另一个 ensure_init 函数
}
```

**重声明规则**：同安全级别的重声明（`_Safe`/`_Safe`、`_Unsafe`/`_Unsafe` 或 default/`_Unsafe`）必须保持 `ensure_init` 一致——要么都有，要么都没有。不同安全级别的重声明（`_Safe` 与非安全）是独立的重载，`ensure_init` 差异是允许的。

```c
// 同安全级别：必须一致
_Safe void foo(int *__attribute__((ensure_init)) _Borrow out);
_Safe void foo(int *__attribute__((ensure_init)) _Borrow out) { // ok: 一致
    *out = 1;
}

// 同安全级别：不一致 → 错误
// _Safe void bar(int *__attribute__((ensure_init)) _Borrow out);
// _Safe void bar(int *_Borrow out) { ... } // error: incompatible declarations

// 不同安全级别：allow differences
void init_value(int *__attribute__((ensure_init)) out);
_Safe void init_value(int *_Borrow out) { // ok: 不同安全级别
    // ...
}
```

**函数指针兼容性**：`ensure_init` 是函数类型的一部分。将不具有 `ensure_init` 的函数赋值给需要 `ensure_init` 的函数指针是不允许的：

```c
typedef _Safe void (*InitFn)(int *__attribute__((ensure_init)) _Borrow out);

_Safe void has_attr(int *__attribute__((ensure_init)) _Borrow out) { *out = 1; }
_Safe void no_attr(int *_Borrow out) { *out = 1; }

_Safe void test(void) {
    InitFn fn = has_attr; // ok: 签名匹配
    // InitFn fn2 = no_attr; // error: 目标需要 ensure_init 但源没有
}
```

通过函数指针的间接调用同样支持 `ensure_init` 效果验证：

```c
_Safe void indirect_call(InitFn fn) {
    int x;
    fn(&_Mut x); // ok: ensure_init 通过函数指针类型识别
    int y = x;   // ok: x 已通过 ensure_init 标记为已初始化
}
```

## `__assume_initialized`

`__assume_initialized(&x)` 是一个内建函数，用于在某个程序点将变量标记为已初始化。与 `ensure_init` 不同，它**不做契约验证**——由用户保证变量确实已初始化。

每次调用只能标记**一个**变量。如需标记多个变量，须分别调用。

使用 `__assume_initialized` 函数时，参数应使用 `&`。

`__assume_initialized` 只能在 `_Unsafe` 区域中使用，因为它绕过了编译器的初始化验证。

该内建函数是路径敏感的：仅在执行到该调用的 CFG 路径上生效。

```c
_Safe void example_builtin(void) {
    int x;
    _Unsafe { __assume_initialized(&x); } // 从此处起 x 被视为已初始化
    int y = x; // ok
}

// 路径敏感：仅在执行到的路径上生效
_Safe void path_sensitive(int cond) {
    int x;
    if (cond) {
        _Unsafe { __assume_initialized(&x); }
    }
    int y = x; // error: use of possibly uninitialized value: `x`
}

// 对结构体使用：所有字段都被标记为已初始化
_Safe void struct_example(void) {
    struct Pair s;
    _Unsafe { __assume_initialized(&s); }
    int a = s.a; // ok
    int b = s.b; // ok
}
```

注意：数组类型由于存在隐式数组退化（decay），需要使用 `&` 而非直接传递数组名：

```c
_Safe void array_example(void) {
    int arr[3];
    _Unsafe { __assume_initialized(&arr); } // ok: 使用 &
    // __assume_initialized(arr);           // error: 需要 & 表达式
}
```

支持的参数形式（取地址表达式的 lvalue 必须是下列之一的组合）：

| 形式 | 说明 |
| ------ | ------ |
| `&x` | 局部变量；若 `x` 是 `ensure_init` 指针参数，同时将 `*x` 标记为已初始化 |
| `&x.f.g...` | 局部变量的结构体字段（任意嵌套深度，纯字段访问） |
| `&*p` | `ensure_init` 指针参数所指向的整个对象 |
| `&p->f.g...` | `ensure_init` 指针所指向结构体的字段（任意嵌套深度，纯字段访问） |

对于 `&p->f...` 形式，当所指向结构体的所有字段都被标记为已初始化后，`*p` 会被自动提升为已初始化（满足 `ensure_init` 契约）。

```c
struct Pair { int a; int b; };

void assume_through_ptr(struct Pair *__attribute__((ensure_init)) out) {
    out->a = 1;
    _Unsafe { __assume_initialized(&out->b); } // 所有字段已覆盖 → *out 已初始化
}
```

**限制：参数路径中不能包含数组下标（`[i]`）。** 初始化分析将数组视为整体单元（不追踪单个元素的状态），因此必须对整个数组断言，而不能断言某个元素。

```c
struct WithArr { int arr[3]; };
struct Outer { struct WithArr s[2]; };

_Safe void array_subscript_example(void) {
    struct WithArr w;
    _Unsafe { __assume_initialized(&w.arr); }    // ok: 断言整个数组字段
    // _Unsafe { __assume_initialized(&w.arr[0]); } // error: 参数不能包含数组下标

    struct Outer o;
    _Unsafe { __assume_initialized(&o); }        // ok: 断言整个结构体
    // _Unsafe { __assume_initialized(&o.s[0].arr); }    // error: 路径中间含下标
    // _Unsafe { __assume_initialized(&o.s[0].arr[0]); } // error: 末端含下标
}
```

## 检查模式

通过编译选项 `-uninit-check=<mode>` 控制初始化分析的范围：

| 模式 | 行为 |
| ------ | ------ |
| `none` | 禁用初始化分析 |
| `safeonly`（默认） | 仅在 `_Safe` 区域内检查，或函数具有 `ensure_init` 参数时检查 |
| `all` | 在所有代码区域内检查（包括非安全区） |

注：只要启用了初始化分析（即模式不为 `none`）, 则`ensure_init` 契约验证生效（包括非安全区）。
