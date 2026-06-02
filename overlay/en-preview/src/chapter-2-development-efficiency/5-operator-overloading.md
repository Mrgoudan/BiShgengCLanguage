# Operator Overloading

## Overview

You define a function by adding the operator attribute to it, which tells the compiler that, for the operator whose operands match the function's parameter rules, this function should be called to perform the operator's functionality. Such a function is also called the operator's overload function.

## Code Example

```c
#include <assert.h>

struct square {
  int width;
  int height;
};

// Overload function for the '+' operator
__attribute__((operator+)) struct square squareAdd(struct square s1,
                                                   struct square s2) {
  struct square s = {s1.width + s2.width, s1.height + s2.height};
  return s;
}

int main() {
  struct square s1 = {100, 50};
  struct square s2 = {60, 110};
  // Previously we had to call the function explicitly to perform the struct operation.
  struct square s3 = squareAdd(s1, s2);
  assert(s3.width == 160);
  assert(s3.height == 160);
  // After marking the function as an overload, we can now operate on the struct directly.
  struct square s4 = s1 + s2;
  assert(s4.width == 160);
  assert(s4.height == 160);
  return 0;
}
```

## Syntax Rules

1. Based on an ordinary function, the `__attribute__((operator Op))` attribute marks the function as an operator overload function.

```c
__attribute__((operator OP)) function-declaration
```

Code example:

```c
// Defines an overload function for the "+" operator
__attribute__((operator +))
struct square squareAdd(struct square s1, struct square s2){
    struct square s = {s1.width + s2.width, s1.height + s2.height};
    return s;
}
```

2. An operator overload function can only be a global function; member functions and functions declared by a `_Trait` are not allowed to be marked as overload functions.

3. List of operators that support overloading.

| Category | Operators |
| --- | --- |
| Binary arithmetic operators | + (add), - (subtract), * (multiply), / (divide), % (modulo) |
| Relational operators | == (equal), != (not equal), < (less than), > (greater than), <= (less than or equal), >= (greater than or equal) |
| Bitwise operators | \| (bitwise OR), & (bitwise AND), ~ (bitwise NOT), ^ (bitwise XOR), << (left shift), >> (right shift) |
| Unary operators | \+ (positive), - (negative), * (dereference) |
| Member access operator | -> |
| Index operator | [] |

4. Requirements for the parameters and return value of an operator overload function.

| Operator | Parameter requirements | Return value requirements |
| --- | --- | --- |
| Relational operators | Only two parameters are allowed, and at least one parameter must be a user-defined type, such as a struct or enum. | The return type must be `_Bool`. |
| *(dereference), -> (member access operator) | Only one parameter is allowed, and the first parameter must be a pointer type to a user-defined type, including a raw pointer, a mutable borrow pointer, or an immutable borrow pointer. | The return type must be a pointer type, including a raw pointer, a mutable borrow pointer, an immutable borrow pointer, or an `Rc` pointer. |
| [] (index operator) | Only two parameters are allowed, and the first parameter must be a pointer type to a user-defined type, including a raw pointer, a mutable borrow pointer, or an immutable borrow pointer. | The return type must be a pointer type, including a raw pointer, a mutable borrow pointer, an immutable borrow pointer, or an `Rc` pointer. |
| Others | A unary operator allows only one parameter, and a binary operator allows only two parameters. The function must have at least one parameter of a user-defined type. | None |

- Code example for relational operator overloading

```c
#include <assert.h>

struct Foo {
  int x;
  int y;
};

// At least one parameter must be a user-defined type
__attribute__((operator>))
_Bool FooCompare(struct Foo A, struct Foo B) {
  return A.y > B.y;
};

int main() {
  struct Foo f1 = {180, 18};
  struct Foo f2 = {166, 22};
  assert(f2 > f1 && "compare error");
  return 0;
}
```

- Code example for dereference operator overloading

```c
#include <assert.h>

struct MyPoint<T> {
  T *data;
};

// The parameter and return value must be pointer types
__attribute__((operator*))
T * derefMyPoint<T>(struct MyPoint<T> *_Borrow p) {
  return p->data;
}

int main() {
  int data = 100;
  struct MyPoint<int> p = {
    &data
  };
  // After the compiler matches the overload function, it automatically takes the address of p, passes it to the overload function, and dereferences the function's return value. This is equivalent to (*derefMypoint(&_Mut p)) == 100
  assert(*p == 100);
  *p = 10;
  assert(*p == 10);
  return 0;
}
```

- Code example for member access operator overloading

```c
#include <assert.h>

struct MyData<T> {
  T a;
};
struct MyPoint<T> {
  MyData<T> *data;
};

// The parameter and return value must be pointer types
__attribute__((operator->))
MyData<T> * mDerefMyPoint<T>(struct MyPoint<T> *_Borrow p) {
  return p->data;
}

int main() {
  struct MyData<int> d = {
    100
  };
  struct MyPoint<int> p = {
    &d
  };
  // After the compiler matches the overload function, it automatically takes the address of p and passes it to the overload function. This is equivalent to mDerefMyPoint(&_Mut p)->a == 100
  assert(p->a == 100);
  p->a = 10;
  assert(p->a == 10);
  return 0;
}
```

- Code example for index operator overloading

```c
#include <assert.h>
#define ARRLEN 10

struct MyArray<T> {
  T data[ARRLEN];
};

__attribute__((operator[]))
T *GetMyArrayData<T>(struct MyArray<T> *p, int index) {
  return &p->data[index];
}

int main() {
  struct MyArray<int> array;
  for (int i = 0; i < ARRLEN; i++) {
    // After the compiler matches the overload function, it automatically takes the address of p, passes it to the overload function, and dereferences the function's return value. This is equivalent to *GetMyArrayData(&_Mut p, i) = i
    array[i] = i;
  }
  assert(array[5] == 5);
  return 0;
}
```

- Code example of an operator overloading error

```c
// error: Defining an operator overload function with no user-defined parameter type is not allowed.
__attribute__((operator+))
int squareAdd(int a, int b) { return a + b; }
```

5. The name of an operator overload function must not conflict with the name of an ordinary function. When defining multiple overload functions for the same operator, they are allowed to coexist if the function names differ and the parameter types are not completely identical.

```c
__attribute__((operator +))
struct square squareAdd(struct square s1, struct square s2) {
  /* code */
}
// Overloading the same operator multiple times is supported
__attribute__((operator +))
struct oblong oblongAdd(struct oblong s1, struct oblong s2) {
  /* code */
}
```

6. An operator overload function can be a generic function.

```c
#include <stdio.h>

struct Point<T> {
  T x;
  T y;
};

__attribute__((operator+))
struct Point<T> Add<T>(struct Point<T> lhs, struct Point<T> rhs) {
  T x1 = lhs.x + rhs.x;
  T y1 = lhs.y + rhs.y;
  struct Point<T> p = {
    .x = x1, .y = y1
  };
  return p;
}

__attribute__((operator*)) struct Point<T>
Mul<T>(struct Point<T> lhs, struct Point<T> rhs) {
  T x1 = lhs.x * rhs.x;
  T y1 = lhs.y * rhs.y;
  struct Point<T> p = {
    .x = x1, .y = y1
  };
  return p;
}

void test1() {
  struct Point<int> p1 = {
    .x = 1, .y = 2
  };
  struct Point<int> p2 = {
    .x = 3, .y = 4
  };
  // {.x = 4, .y = 6}
  struct Point<int> p3 = p1 + p2;
  printf("p3.x: %d, p3.y :%d\n", p3.x, p3.y);
}

void test2() {
  struct Point<int> p1 = {
    .x = 1, .y = 2
  };
  struct Point<int> p2 = {
    .x = 3, .y = 4
  };
  // {.x = 3, .y = 8}
  struct Point<int> p3 = p1 * p2;
  printf("p3.x: %d, p3.y :%d\n", p3.x, p3.y);
}

int main() {
  test1();
  test2();
  return 0;
}
```
