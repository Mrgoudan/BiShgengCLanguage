# Stackless Coroutines

## Introduction to Stackless Coroutines

Unlike stackful coroutines, whose call stacks are explicitly allocated by the programmer, a coroutine's call stack is managed implicitly by the runtime system. A coroutine does not hold its own call stack, so when switching coroutines there is no need to save and restore the entire call stack; only the coroutine's execution state is saved. They are defined and called using the `_Async/_Await` keywords. `_Async` is used to modify an asynchronous function, and `_Await` performs an asynchronous function call.

The goal of BiSheng C's stackless coroutines is to support asynchronous, high-concurrency scenarios, such as a web server implementing high-concurrency network communication, asynchronous request handling, and database connection pools for web applications.

## Future/PollResult Standard Library Definitions

### Future/PollResult Definitions

**Future** is used to describe a computation (or task) that has not yet finished; an object proxies this unknown result, and the computation (or task) can be suspended and resumed.

A Future is executed by calling the poll function.

**PollResult** is the return value of the poll function. It describes the execution state and result of a computation (or task).

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

### Using Future/PollResult

The implementation of coroutines uses the Future/PollResult definitions. There are two ways to import them: the system imports them by default, or the user imports them explicitly.

When importing explicitly, the user needs to include the header file `future.hbs`. The file is installed under the default search path /usr/include of the system (or installed in a specified directory), in which case the path is specified at compile time using the -I compile option.

```c
#include  "future.hbs"
```

## Syntax Rules

1. Stackless coroutines are only allowed in a BiSheng C compilation unit.

2. If a function uses the `_Await` keyword, then that function must be modified with `_Async`. An `_Async` function can contain 0, 1, ... n `_Await` expressions.

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

3. The declaration and the implementation of an `_Async` function can be separated.

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

4. `_Async` functions support recursive calls.

```c
_Async int f(int n) {
    if (n == 0 || n == 1)
        return 1;
    int tmp = _Await f(n-1);
    return n*tmp;
}
```

5. The `_Async` keyword can modify a member function.

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

6. Multiple different or identical `_Await` expressions can appear in an `_Async` function.

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

7. `_Async` functions do not support variable-length arrays, that is, arrays whose dimensions contain a variable.

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

8. An `_Await` expression cannot appear in the condition of an if/while/for/do-while statement, etc.

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

9. An `_Await` expression cannot coexist with an expression that has "side effects" (such as a function).

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

10. An `_Await` expression cannot appear in a compound expression, for example an expression containing +, -, *, /, %, &, |, >>, <<, etc.

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

11. `_Await` expressions support multiple levels of nested `_Await` calls.

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

12. An `_Await` expression can appear in a return statement.

```c
_Async int read(int n) {
    // read data...
    return n;
}

_Async int f() {
    return _Await read(2);
}
```

## Code Sample

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
    // When an _Async function's return type is void, we need to use the struct Void type (which is created automatically) to instantiate _Trait Future
    _Trait Future<struct Void>* this2 = g(5);
    this2->poll();
    this2->free();
    return 0;
}
```
