# 无栈协程

## 无栈协程简介

无栈协程与有栈协程调用栈由程序员显示分配的不同，协程的调用栈由运行时系统隐式管理的，协程本身不持有自己的调用栈，在切换协程时不需要保存和恢复整个调用栈，只保存协程执行状态。它是通过 `_Async/_Await` 关键字进行定义和调用。`_Async` 用来修饰异步函数，`_Await` 实现异步函数调用。

毕昇 C 的无栈协程目标是支持异步、高并发场景，例如 Web 服务器实现高并发网络通信、异步处理请求、 Web 应用程序的数据库连接池等。

## Future/PollResult标准库定义

### Future/PollResult定义

**Future** 用于描述某个计算（或任务）尚未结束，用一个对象来代理这个未知的结果，这个计算（或任务）可以暂停和恢复。

Future 的执行是通过调用 poll 函数实现的。

**PollResult** 即 poll 函数返回值，用于描述计算（或任务）执行状态以及结果。

```c
struct PollResult<T> {
    _Bool isPending; //isCompleted
    T res;
};

T struct PollResult<T>::is_pending(struct PollResult<T>* this) { ... }
struct PollResult<T> struct PollResult<T>::pending() { ... }
_Bool struct PollResult<T>::is_completed(struct PollResult<T> *this, T *out) { ... }
struct PollResult<T> struct PollResult<T>::completed(T result) { ... }

_Trait Future<T> {
    struct PollResult<T> poll(This* this);
    void free(This* this);
};
```

### Future/PollResult使用

协程的实现使用到 Future/PollResult 定义，其导入方式有系统默认导入和用户显式导入两种。

用户显式导入时需要引用头文件 `future.hbs`。文件安装在系统 /usr/include 默认搜索路径下（或者安装在指定目录）在编译时使用 -I 编译选项指定路径。

```c
#include  "future.hbs"
```

## 语法规则

1. 无栈协程只允许在毕昇 C 编译单元使用

2. 如果函数中使用了 `_Await` 关键字，那么这个函数必须用 `_Async` 修饰。`_Async` 函数内可以有 0,1...n 个 `_Await` 表达式

```c
_Async int TimeOut(int t) {
    return t;
}

_Async int getData1() {
    int t = _Await TimeOut(1000);
    return t;
}

_Async int getData2() {
    _Await getData1();
    _Await TimeOut(1000);
    _Await TimeOut(2000);
    return 0;
}
```

3. `_Async` 函数声明和实现可以分开

```c
#include "string.h"

_Async int ReadBuffer(char *str);

_Async int GetBufferSize() {
    char* Content;
    char ContentCopy[12] = "hello,word!";
    Content = ContentCopy;
    int size = _Await ReadBuffer(Content);
    return size;
}

_Async int ReadBuffer(char *str) {
    char *cstr = "hello,word!";
    if (strcmp(str, cstr) == 0)
      return sizeof(str);
    else return 0;
}
```

4. `_Async` 函数支持递归调用

```c
_Async int f(int n) {
    if (n == 0 || n == 1)
        return 1;
    int tmp = _Await f(n-1);
    return n*tmp;
}
```

5. `_Async` 关键字可以修饰成员函数

```c
_Async void int::g(int* this);

_Async int int::f() {
    int i = 1;
    _Await int::g(&i);
    _Await i.g();
    return 0;
}

_Async void int::g(int* this) {
    _Trait Future<int>* a = read(1);
    _Await a;
}
```

6. `_Async` 函数中可以出现多个不同或相同 `_Await` 表达式

```c
_Async void client1() {
    // client1 send message...
}

_Async void client2() {
    // client2 send message...
}

_Async int Server(int start) {
    // server receive message
    _Await client1();
    if (start < 20)
        _Await client2();
    return start;
}
```

7. `_Async` 函数不支持变量数组，即数组中含有变量

```c
_Async int f() {
    int *VarArray1[n]; // expected-error {{_Async function does not support VariableArrayType}}
    int VarArray2[3][2][n]; // expected-error {{_Async function does not support VariableArrayType}}
    int *VarArrayPtr[n][2][n][5]; // expected-error {{_Async function does not support VariableArrayType}}
    int Array[3]; // support
    int MultiArray[2][3][4][5]; // support
    return 0;
}
```

8. `_Await` 表达式不能出现在 if/while/for/do-while 等判断条件中

```c
_Async int read(int n) {
    // read data...
    return n;
}

_Async int getData() {
    int res = 0;
    if (_Await read(1)) { // expected-error {{_Await expression is not allowed to appear in condition statement of if statement}}
        res = _Await read(1);
    }

    if (res == 2) { // support
        res = _Await read(1);
    }
    return res;
}
```

9. `_Await` 表达式不能和“有副作用”的表达式（例如函数）并存

```c
_Async int read(int n) {
    // read data...
    return n;
}

int test(int a, int b) {
    return 42;
}

_Async int f() {
    test(_Await read(2), _Await read(2)); // expected-error {{_Await expression is not allowed to appear in function parameters}}
    test(t(), _Await read(2)); // expected-error {{_Await expression is not allowed to appear in function parameters}}
    test(3, _Await read(2)); // support
    return 0;
}
```

10. `_Await` 表达式不能出现在复合表达式中，例如：表达式中含有 +、-、*、/、%、&、|、>>、<< 等

```c
_Async int read(int n) {
    // read data...
    return n;
}

_Async int f() {
    int x = _Await read(2) + 3; // expected-error {{_Await expression is not allowed to appear in binary operator expression}}
    int y = _Await read(2); // support
    return 0;
}
```

11. `_Await` 表达式支持 `_Await` 多层嵌套调用

```c
_Async int test0(int n) {
    // read data...
    return n;
}

_Async int test1(int n) {
    // ...
    return n;
}

_Async int test2(int n) {
    // ...
    return n;
}

_Async int test3(int n1, int n2) {
    // ...
    return 0;
}

_Async int f() {
    int start = 0;
    int result1 = _Await test1(_Await test1(start));
    int result2 = _Await test1(_Await test2(start));
    int result3 = _Await test3(2, _Await test1(_Await test2(start)));
    return result1 + result2 + result3;
}
```

12. `_Await` 表达式可以出现在 return 语句中

```c
_Async int read(int n) {
    // read data...
    return n;
}

_Async int f() {
    return _Await read(2);
}
```

## 代码样例

```c
# include "future.hbs"
const int MAX = 3;

_Async int read(int a) {
    return 0;
}

_Async int f() {
    int *nptr = NULL;
    int  var[] = {10, 100, 200};
    int  i, *ptr;

    ptr = &var[MAX-1];
    for ( i = MAX; i > 0; i--)
    {
    ptr--;
    }
    int result = _Await read(1);
    result += *ptr;
    return result;
}

_Async void g(int start) {
    int result = start;
    for (int i = 0; i< start; i++) {
        int a = _Await f();
    }
}

int main() {
    _Trait Future<int>* this1 = f();
    this1->poll();
    this1->free();
    // 当 _Async 函数的返回类型是 void 时，我们需要用 struct Void 类型（会自动创建）来对 _Trait Future 实例化
    _Trait Future<struct Void>* this2 = g(5);
    this2->poll();
    this2->free();
    return 0;
}
```
