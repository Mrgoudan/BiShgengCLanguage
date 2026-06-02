# 安全API

## `safe_malloc`

`safe_malloc`是 BiShengC 语言提供的一个安全的内存分配函数。
该函数接收一个泛型类型`T`的变量，表示要分配的内存的大小以及分配后对内存的初始化。
该函数的返回值为`T * _Owned`类型，即指向分配好的堆内存的`_Owned`指针。
一些具体的使用例子如下。

在 C 语言中，如果我们需要申请一段堆内存，我们可以使用`malloc`函数进行分配，然后给该内存赋值，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
}
```

然而，这样分配的内存不会在`p`的作用域结束时检查是否调用了`free`进行释放，会造成内存泄漏。
此外，如果再使用一个指针也指向该内存，并在作用域结束时释放，则会出现重复释放的问题，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
    int *q = p;
    free(p);
    free(q); // error: double free!
}
```

使用 BiShengC 语言即可解决这种问题，相应的代码如下：

```c
_Safe void example(void) {
    int * _Owned p = safe_malloc(2);
    int * _Owned q = p;
    _Unsafe {
        safe_free((void * _Owned)q);
    }
}
```

在使用 BiShengC 语言改写后的代码中，如果我们在函数退出前什么都不做，则会出现编译错误`"memory leak of value: q"`，避免了内存泄漏问题的发生；
如果我们在函数退出前同时调用`safe_free((void * _Owned)p)`和`safe_free((void * _Owned)q)`，则会出现编译错误`"use of moved value: p"`，避免了重复释放问题的发生。

那么对于更为复杂的结构体类型，该如何正确使用`safe_malloc`进行内存分配呢？
对于结构体类型，需要首先在栈上构造出相应的变量，然后传给`safe_malloc`在堆上完成相应内存的分配，以下代码为具体示例：

```c
struct S {
    int * _Owned p;
    int * _Owned q;
};

_Safe void example(void) {
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * _Owned sp = safe_malloc(s);
    ...
}
```

## `safe_free`

`safe_free`是 BiShengC 语言提供的一个安全的内存释放函数。
该函数接收一个`void * _Owned`类型的指针，表示要释放的内存的地址。
该函数的返回值为`void`类型。
因此，在调用`safe_free`进行释放前需要将`_Owned`指针显式地强制转换为`void * _Owned`类型，具体的转换规则可参考[所有权状态转移规则-强制类型转换](../chapter-3-memory-safety/1-ownership.md#强制类型转换)。
一些具体的使用例子如下。

```c
struct S {
    int * _Owned p;
    int * _Owned q;
};

_Safe void example(void) {
    int * _Owned pa = safe_malloc(199);
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * _Owned sp = safe_malloc(s);
    _Unsafe {
        safe_free((void * _Owned)pa);
        safe_free((void * _Owned)sp->p);
        safe_free((void * _Owned)sp->q);
        safe_free((void * _Owned)sp); // 必须先释放 sp->p 和 sp->q，才能释放 sp
    }
}
```

## `safe_swap`

`safe_swap`是 BiShengC 语言提供的一个安全交换两个变量的值的函数。
该函数是一个泛型函数,接收两个类型为`T* _Borrow`类型的参数,即需要交换的变量的值的借用。
该函数的返回值为`void`类型,该 API 的主要作用为在交换两个变量的值时,同时能交换两个变量所拥有的所有权.
一个具体的使用例子如下。

```c
_Owned struct S {
_Public:
    int* _Owned p;
    int* _Owned q;
    ~S(S this) {
        safe_free((void* _Owned)this.p);
        safe_free((void* _Owned)this.q);
    }
};

_Safe void example(void) {
    S s1 = { .p = safe_malloc(1), .q = safe_malloc(2) };
    S s2 = { .p = safe_malloc(3), .q = safe_malloc(4) };
    safe_swap(&_Mut s1, &_Mut s2); // 交换后,s1.p为3,s1.q为4
}
```

## `forget`

`forget` 主要用于获取变量的所有权并且“忘记”它，该函数是一个泛型函数，接收一个类型为泛型类型`T`的变量，表示要“忘记”的值：

1. 如果该变量是 _Owned 指针，那么该指针指向的内存不会被释放；
2. 如果该变量是 _Owned struct 类型，那么不会调用其析构函数。

在一些特殊的场景，用户希望取得变量的所有权而不通过该变量来释放管理的底层资源（如堆内存或文件句柄，这些资源可能已经通过裸指针操作被转移或释放），例如：

```c
#include "bishengc_safety.hbs"
#include <string.h>
_Owned struct Resource {
_Public:
    char *_Owned s;
    ~Resource(This this) {
        safe_free((void *_Owned)this.s);
    }
};

void get_resource(char* val) {
    Resource r = { .s = safe_malloc<char>(100) };
    memcpy(val, (const void *)&r, sizeof(Resource)); // Resource中的资源被转移
    forget<Resource>(r); //此时 forget 函数会获取 r 的所有权，但是并不会调用 Resource 的析构函数来释放堆内存
}

int main() {
    char val[sizeof(Resource)];
    get_resource(val);
    return 0;
}
```
