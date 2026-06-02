# Safe Zone

## Overview

The C language has many rules that are too flexible, making static checking by the compiler inconvenient. We therefore introduce a new syntax so that, within a certain scope, BiSheng C code must follow stricter constraints, guaranteeing that the code within this scope will definitely not exhibit "memory safety" problems.

The `_Safe` and `_Unsafe` keywords are allowed to modify functions, statements, and parenthesized expressions.

- `_Unsafe` indicates that this piece of code is in a non-safe zone. This code follows the rules of standard C, and the safety of this code is guaranteed by the user.

- `_Safe` indicates that this piece of code is in a safe zone. This code must follow stricter constraints, and the compiler can guarantee memory safety.

- A global function not modified by the `_Safe` or `_Unsafe` keyword is non-safe by default.

## Code Example

```c
#include <stdlib.h>

typedef struct File {
} FileID;

// No keyword modifier, meaning it defaults to a non-safe zone
FileID *_Owned create(void) {
  FileID *p = malloc(sizeof(FileID));
  return (FileID * _Owned) p;
}
FileID *_Owned foo(FileID *_Owned p) { return p; }

// Modifying the function with _Safe indicates that the function is a safe function, and the function body is a safe zone
_Safe void safely_free(FileID *_Owned p) {
  // Modifying a code block with _Unsafe indicates that this code is in a non-safe zone. This code is a non-safe zone within a safe zone, and also belongs to a non-safe zone
  _Unsafe { free((FileID *)p); }
}

int main(void) {
  FileID *_Owned p1 = create();
  FileID *_Owned p2 = foo(p1);
  // Modifying the statement with _Safe indicates that this code is in a safe zone
  _Safe safely_free(p2);
  return 0;
}
```

## Grammar Rules

1. `_Safe/_Unsafe` is allowed to modify function declarations, function signatures, function definitions, function pointers, statements, and parenthesized expressions.

```c
// Modifies the function signature
_Safe int foo(int, int);
// Modifies the function declaration
_Safe int bar(int a, int b);
// Modifies the function definition
_Safe int add(int a, int b) { return a + b; }
// Modifies the function pointer
_Safe int (*p)(int, int);

_Safe int main(void) {
  // Modifies the code block
  _Safe {
    int a = 1;
  }
  _Unsafe { int b = 1; }
  // Modifies the statement
  _Safe int c = 1;
  _Unsafe c++;
  // Modifies the parenthesized expression
  char d = _Unsafe((char)c);
  return 0;
}
```

2. `_Safe/_Unsafe` cannot modify global variables, type declarations outside of functions, or `typedef` declarations (modifying function pointers is allowed).

```c
_Safe int g_a; // error: cannot modify a global variable

_Safe struct b { int a; }; // error: cannot modify a type declaration outside of a function

_Safe typedef int mm; // error: cannot modify a typedef

int main() { return 0; }
```

3. For a function modified by `_Safe`, the parameter types and return type are allowed to be raw pointer types, `struct` types whose members contain raw pointers, array types, and `union` types.

```c
_Safe int *foo(int a); // ok: the return value is a raw pointer type

_Safe int bar(int *a); // ok: the parameter type is a raw pointer type

typedef struct F {
  int *a;
} SF;

_Safe SF wrapped_foo(int a); // ok: the return value is a struct type whose members contain a raw pointer type

_Safe int wrapped_bar(SF b); // ok: the parameter type is a struct type whose members contain a raw pointer type
```

4. For a function modified by `_Safe`, the function parameter list cannot be omitted.

```c
_Safe void test(); // error

_Safe void test(void); // ok
```

5. For a function modified by `_Safe`, the function parameter list cannot contain variadic parameters, unless the function uses the `__attribute__((format(...)))` attribute.

```c
_Safe int foo(int a, ...); // error
__attribute__((format(printf, 1, 2))) _Safe int bar(const char *fmt, ...); // ok
```

Note: even though declaring a variadic function with the format attribute is allowed, `va_start`, `va_arg`, `va_end`, etc. still cannot be used within the function body; these operations are forbidden within a safe zone.

6. If a function in a `_Trait` is declared as `_Safe`, then the corresponding member function of the type that implements the `_Trait` is also required to be a function modified by `_Safe`. If a function in a `_Trait` is not declared as `_Safe`, then the member function of the type that implements the `_Trait` is also allowed to be `_Safe`, but the compiler will issue a **warning**.

```c
_Trait G {
  _Safe int *_Owned foo(This * _Owned this);
  int *_Owned bar(This * _Owned this);
};

_Safe int *_Owned int ::foo(int *_Owned this) { return this; } // ok: the _Trait implementation function must be _Safe
// The implementation of a non-_Safe function may be _Safe
_Safe int *_Owned int ::bar(int *_Owned this) { return this; }
_Impl _Trait G for int;

int main() { return 0; }
```

7. Multiple declarations of a function and mixed mode

The same function identifier can have multiple declarations, supporting mixed `_Safe` and `_Unsafe` declarations.

- 7.1 Compatibility requirements

   **Same modifier**: among multiple `_Safe` declarations, or among multiple `_Unsafe` declarations, the function types must be compatible.

   **Mixed mode**: `_Safe` and `_Unsafe` declarations can coexist, but must satisfy mixed-mode compatibility (see 7.3).

   **Generic functions excepted**: generic functions do not support mixed mode; the same instantiation cannot have both `_Safe` and `_Unsafe` declarations.

   **Member functions**: same rules as ordinary functions.

- 7.2 Function type compatibility

   **Conditions for two function types to be compatible**:
  - The return types are compatible
  - The number of parameters is the same, and the use of the ellipsis (`...`) is consistent
  - The corresponding parameter types are compatible (during the compatibility check, all qualifiers except `_Owned`, `_Borrow`, `_Nonnull`, and `_Nullable` are removed)

  **Pointer compatibility**:

  - Pointer types require the same qualifiers (`_Owned`, `_Borrow`, `_Nonnull`, `_Nullable`) and compatible target types
  - `_Owned` and `_Borrow` pointers are incompatible with each other
  - `_Owned`/`_Borrow` pointers are incompatible with raw pointers (except in mixed mode, see 7.3)

- 7.3 Mixed-mode compatibility (`_Unsafe` coexisting with `_Safe`)

   **Return type compatibility**:

  - `_Unsafe T*` → `_Safe T* _Owned`: a `_Safe` declaration may add the `_Owned` qualifier to a raw pointer
  - `_Unsafe T*` → `_Safe T* _Borrow`: a `_Safe` declaration may add the `_Borrow` qualifier to a raw pointer
  - `_Unsafe T* _Owned` → `_Safe T* _Owned`: the `_Owned` qualifier must be preserved
  - `_Unsafe T* _Borrow` → `_Safe T* _Borrow`: the `_Borrow` qualifier must be preserved
  - `_Safe T* _Owned` ⟷ `_Safe T* _Borrow`: **incompatible**

   **Parameter type compatibility**:

  - The number of parameters and the use of the ellipsis must be consistent
  - A `_Safe` declaration may only **add** the `_Owned` or `_Borrow` qualifier to a parameter; it cannot **remove** an existing qualifier
  - **An `_Owned` parameter is incompatible with a `_Borrow` parameter**

   **Examples**:

   ```c
   // ok: the _Safe declaration adds the _Owned qualifier
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);

   // ok: the _Safe declaration adds the _Borrow qualifier
   _Unsafe int* bar(int* p);
   _Safe int* _Borrow bar(int* _Borrow p);

   // error: the _Safe declaration removed the _Borrow qualifier from the unsafe declaration
   int* _Borrow baz(int* _Borrow p);
   _Safe int* baz(int* p);

   // error: the _Safe declaration removed the _Owned qualifier from the unsafe declaration
   int* _Owned qux(int* _Owned p);
   _Safe int* qux(int* p);

   // error: the owned parameter is incompatible with the borrow parameter
   _Unsafe int* bad(int* p);
   _Safe int* _Borrow bad(int* _Owned p);

   // ok: same modifier, types completely identical
   _Safe int* _Owned fiz(int* _Owned p);
   _Safe int* _Owned fiz(int* _Owned q);
   ```

- 7.4 Function definition

   A mixed-mode function can be defined **only once**.

   The definition may be based on the `_Unsafe` version or the `_Safe` version, but must be compatible with all declarations.

   ```c
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);
   
   // Definition (choose one)
   _Safe int* _Owned foo(int* _Owned p) { return p; }
   // or
   _Unsafe int* foo(int* p) { return p; }
   ```

8. Function call resolution

- 8.1 Calls in a safe context

   Within a safe context (a `_Safe` block), only `_Safe` functions can be called. If a function has only an `_Unsafe` declaration, it is a compile error.

   ```c
   _Unsafe void foo(void);
   
   int main() {
     _Safe {
       foo();  // error: calling a non-safe function within a safe zone is not allowed
     }
   }
   ```

- 8.2 Calls in a non-safe context

   Within a non-safe context (an `_Unsafe` block or the default), overload resolution is performed:

  - The `_Safe` declaration is matched first
  - When the `_Safe` declaration does not match, the `_Unsafe` declaration is used

   ```c
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);
   
   int *_Owned bar() {
     int* raw_p = nullptr;
     int* _Owned owned_p = nullptr;
   
     // The corresponding version is chosen according to the argument type
     foo(raw_p);      // calls the unsafe version
     return foo(owned_p);    // calls the safe version
   }
   ```

9. Function pointer assignment

Function pointer assignment rules:

**When assigning to a function pointer modified by `_Safe`**:

- If the function identifier used for the assignment does not have a `_Safe` version declaration, an error is reported; that is, an `_Unsafe` version declaration is not allowed to be assigned to a function pointer modified by `_Safe`
- If the function identifier used for the assignment has a `_Safe` version declaration, then the `_Safe` version declaration type is required to be compatible with the function pointer type

**When assigning to a function pointer modified by `_Unsafe`**:

- As long as the `_Unsafe` version declaration type of the function identifier used for the assignment is compatible with the function pointer type, or the `_Safe` version declaration type is `_Unsafe-_Safe` compatible with the function pointer type, the assignment is allowed
- If neither is satisfied, a compile error is reported

**Examples**:

```c
_Safe void safe_foo(void);
_Unsafe void unsafe_foo(void);

// Mixed-mode declarations
_Unsafe int* bar(int* p);
_Safe int* _Owned bar(int* _Owned p);

int main() {
  _Safe void (*safe_ptr)(void) = nullptr;
  _Unsafe void (*unsafe_ptr)(void) = nullptr;

  safe_ptr = safe_foo;      // ok: assigning a safe function to a safe pointer
  safe_ptr = unsafe_foo;    // error: there is no safe version declaration

  unsafe_ptr = unsafe_foo;  // ok: assigning an unsafe function to an unsafe pointer
  unsafe_ptr = safe_foo;    // ok: the safe version is unsafe-safe compatible

  // Mixed-mode function pointer assignment
  _Safe int* _Owned (*safe_bar_ptr)(int* _Owned) = nullptr;
  _Unsafe int* (*unsafe_bar_ptr)(int*) = nullptr;

  safe_bar_ptr = bar;   // ok: uses the safe version
  unsafe_bar_ptr = bar; // ok: uses the unsafe version or the safe version (_Unsafe-safe compatible)
}
```

A formal parameter declared as an array type in a function parameter is equivalent to the corresponding pointer-type formal parameter, and the two are interchangeable during function pointer assignment:

```c
_Safe void test10(int arr[3]) {}
_Safe void test11(int *arr) {}

_Safe void (*p10)(int arr[3]) = nullptr;
_Safe void (*p11)(int *arr) = nullptr;

_Safe int main(void) {
    p10 = test11;  // ok: int arr[3] is equivalent to int *arr
    p11 = test10;  // ok: int *arr is equivalent to int arr[3]
    return 0;
}
```

10. When `_Safe` modifies a generic function, the `_Safe` check is also performed on each instantiated version of the generic.

```c
#include "bishengc_safety.hbs" // A header file provided by the BiSheng C language, used to safely perform memory allocation and deallocation

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

11. Member functions can also be modified by `_Safe/_Unsafe`, with the same rules as global functions.

```c
struct MyStruct<T> {
  T res;
};
_Safe T struct MyStruct<T>::foo(T a) {
  return a;
}

int main() { return 0; }
```

12. A function or function pointer called within a safe zone must have a `_Safe` function signature; calling a non-safe function or function pointer is not allowed.

```c
_Safe void safe_foo(void) {}
_Unsafe void unsafe_foo(void) {}
_Safe void (*safe_func_ptr)(void);
_Unsafe void (*unsafe_func_ptr)(void);
int main() {
  _Safe {
    safe_foo();
    unsafe_foo(); // error: calling a non-safe function within a safe zone is not allowed
    safe_func_ptr();
    unsafe_func_ptr(); // error: calling a non-safe function pointer within a safe zone is not allowed
  }
  _Unsafe {
    safe_foo();
    unsafe_foo(); // ok
    safe_func_ptr();
    unsafe_func_ptr(); // ok
  }
}
```

13. A safe zone is allowed to further contain statements, function pointers, and parenthesized expressions modified by `_Unsafe`, and a non-safe zone is also allowed to further contain statements, function pointers, and parenthesized expressions modified by `_Safe`.

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

14. Within a safe zone, in a `switch` statement the `case/default` labels can only exist in the first-level code block following the `switch`, and the first-level code block is not allowed to have variable definitions.

```c
_Safe void foo(int a) {
  switch (a) {
    int b = 10; // error: variable definitions are not allowed in the first-level code block
    case 0: {
        int c = 1;
        break;
    }
    {
        case 1 : { break; } // error: case can only exist in the first-level code block following the switch
    }
    {
        default: { break; } // error: default can only exist in the first-level code block following the switch
    }
  }
}

int main() {
  int a = 1;
  foo(a);
  return 0;
}
```

15. Within a safe zone, accessing members of a `union` (reading or writing) is not allowed, and a raw pointer is not allowed to access members through `->`.

    Within a safe zone, defining and declaring `union` types is allowed, and `union` is allowed as a function parameter and return type, but accessing any member of a `union` through the `.` operator is not allowed.

    Owned pointers and borrow pointers are allowed to access members through `->`.

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

    a++; // ok: the safe zone allows increment// ok: explicit conversion to an owned pointer of void type is allowed
    a--; // ok: the safe zone allows decrement

    a += 1; // ok: the safe zone allows the += operator
    a -= 1; // ok: the safe zone allows the -= operator

    union AgeOrName b = {10}; // ok: the safe zone allows declaring and initializing a union

    int c = b.age; // error: the safe zone does not allow accessing union members through "." (read operation)
    b.age = 20; // error: the safe zone does not allow accessing union members through "." (write operation)

    int g = e->age; // error: the safe zone does not allow a raw pointer to access members through "->"
    int h = f->age; // ok: an owned pointer is allowed to access members through "->"
    int j = i->age; // ok: a borrow pointer is allowed to access members through "->"
  }
}

int main() {
  foo();
  return 0;
}
```

16. Within a safe zone, the address-of operator `&` is not allowed (taking the address of a function is allowed); only `&_Const` and `&_Mut` borrows are allowed.

    Within a safe zone, dereferencing a raw pointer is not allowed, but `_Owned` pointers, `_Borrow` pointers, and non-null function pointers may be dereferenced.

```C

_Safe int inc(int a) { return a + 1; }

_Safe int test1(unary_f _Nonnull f) {
  return (*f)(1);  // ok: a _Nonnull function pointer
}

_Safe int test2(unary_f f) {
  if (f != nullptr) {
    return (*f)(1);  // ok: confirmed non-null after the null check
  }
  return 0;
}
```

```c
#include "bishengc_safety.hbs" // A header file provided by the BiSheng C language, used to safely perform memory allocation and deallocation

typedef _Safe int (*unary_f)(int);
_Safe int inc(int a) { return a + 1; }

void test(unary_f _Nonnull fn) {
  _Safe {
    int a = 10;
    
    int *b = &a; // error: the safe zone does not allow the address-of operator
    int c = *b; // error: the safe zone does not allow dereferencing a raw pointer

    int *_Owned d = safe_malloc(2);
    int e = *d; // ok: dereferencing an owned pointer is allowed
    safe_free((void *_Owned)d);

    int *_Borrow f = &_Mut a;
    int g = *f; // ok: dereferencing a borrow pointer is allowed

    int h = (*fn)(1);  // ok: dereferencing a function pointer is allowed
  }
}

int main() {
  test(inc);
  return 0;
}
```

17. Within a safe zone, conversion between pointer types pointing to different types is not allowed, but explicit conversion of an owned pointer of another type to an owned pointer of void type is allowed.

    Within a safe zone, conversion between a pointer type and a non-pointer type is not allowed (array decay to a raw pointer is allowed), and conversion among `_Owned`/`_Borrow`/raw pointers is not allowed.

```c
void test() {
  int *pa;
  double *pb;
  _Safe {
    pb = pa; // error: conversion between pointer types pointing to different types is not allowed
    pa = pb; // error: conversion between pointer types pointing to different types is not allowed
    pb = (double *)pa; // error: conversion between pointer types pointing to different types is not allowed
  }
  int i;
  _Safe {
    pa = i; // error: conversion between a pointer and a non-pointer type is not allowed
    i = pa; // error: conversion between a pointer and a non-pointer type is not allowed
  }
  int *_Owned pd = (int *_Owned)pa;
  _Safe {
    pa = pd; // error: conversion between _Owned/raw pointers is not allowed
    pd = pa; // error: conversion between _Owned/raw pointers is not allowed

    void *_Owned pe = (void *_Owned)pd; // ok: explicit conversion to an owned pointer of void type is allowed
  }
}

int main() {
  test();
  return 0;
}
```

18. Within a safe zone, **implicitly performing** a type conversion from a larger expressible range to a smaller one (for example, from `long` to `int`, from `int` to `_Bool`, from `int` to `enum`) is not allowed, and **implicitly performing** a type conversion from higher precision to lower precision (for example, from `double` to `float`) is not allowed. Within a safe zone, converting an arithmetic type to an enum type is not allowed, except for an explicit conversion from an enum type to an enum type with an equal or larger expressible range. Within a safe zone, conversion from a floating-point type to an integer type is not allowed. For a type conversion of a **constant whose value can be determined at compile time**, if the target type can represent this value, then the type conversion may be performed implicitly within the safe zone; otherwise, such a conversion must be performed explicitly and must conform to the above rules.

    **Semantic rules and detailed explanation:**
    Excluding enum types, the type conversions allowed within a safe zone are shown in the figure below: (if implicit conversion is supported in a direction, then explicit conversion is certainly supported; if only explicit conversion is required, it means implicit conversion is not allowed. When an edge representing an allowed conversion has one end connected to a subgraph, it is equivalent to connecting to all nodes of that subgraph; for example, `_Bool` has an edge pointing to integers, meaning `_Bool` is allowed to be converted to any integer type by the same rule.)

    ``` mermaid
    graph TB
      bool["_Bool"]
      subgraph int["Integer"]
        direction LR
        subgraph signedint["Signed integer"]
          direction TB
          signedint_s["small type"]
          signedint_l["large type"]
          signedint_s -- forward implicit, reverse explicit --> signedint_l
        end
        subgraph unsignedint["Unsigned integer"]
          direction TB
          unsignedint_s["small type"]
          unsignedint_l["large type"]
          unsignedint_s -- forward implicit, reverse explicit --> unsignedint_l
        end
        signedint <-- explicit conversion required both ways --> unsignedint
      end
      bool -- forward implicit, reverse explicit --> int
      subgraph fp["Floating-point"]
      direction TB
        fp_s["small type"]
        fp_l["large type"]
        fp_s -- forward implicit, reverse explicit --> fp_l
      end
      int -- forward explicit, reverse not allowed --> fp
    ```

    18.1. When converting an arithmetic type to `_Bool`, if the value of the original type is zero, the converted value is 0; otherwise, the converted value is 1 (consistent with C).

    ```c
    int b = 0;
    _Bool c = (_Bool)b; // ok: int -> _Bool; c = 0
    c = (_Bool)1;       // ok: int -> _Bool; c = 1
    c = (_Bool)2;       // ok: int -> _Bool; c = 1
    ```

    18.2. When converting a signed integer type or an unsigned integer type with a larger expressible range to an **unsigned** integer type, if the magnitude of the value of the original type is within the range of the target type, the magnitude of the converted value is unchanged; otherwise, the magnitude of the converted value is "the value obtained by repeatedly adding or subtracting (1 + the maximum value of the target type) to the original value until the result falls within the range of the target type" (consistent with C). If the expressible range of the target unsigned integer type is from 0 to $2^N-1$ and the value of the original type is $X$, then the magnitude of the converted value is $X$ repeatedly adding or subtracting $2^N$ until the result $Y$ falls between 0 and $2^N-1$, and the converted value is $Y$. The above is equivalent to truncating a large integer type to a small unsigned integer type, or converting a small signed integer type to an unsigned integer type via sign-bit extension.

    ```c
    unsigned long a = 2;
    unsigned b = (unsigned) a; // ok: unsigned long -> unsigned int
    int c = -1;
    unsigned d = (unsigned) c; // ok: int -> unsigned int; d = 2^32 - 1
    unsigned long e = (unsigned long) c; // ok: int -> unsigned long; e = 2^64 - 1
    ```

    18.3. When converting an integer type with a larger expressible range to a **signed** integer type with a smaller expressible range (not including enum types), if the magnitude of the value of the original type is within the range of the target type, the magnitude of the converted value is unchanged; otherwise, the magnitude of the converted value is implementation-defined. In the current BiSheng C compiler implementation, when the original value exceeds the representable range of the target type, the large integer type is truncated to the small signed integer type.

    ```c
    long a = 2;
    int b = (int) a; // ok: long -> int
    unsigned c = 4294967295; // c = 2^32 - 1
    int d = (int) c; // ok: unsigned int -> int; d = -1
    ```

    18.4. Regarding enum types: a safe zone only allows an explicit conversion from one enum type to another enum type (with restrictions); no other type can be converted to an enum type. An implicit conversion from an enum type to its corresponding integer type is allowed (for example, given `enum E : unsigned {...}`, `enum E` can be implicitly converted to `unsigned`). When using an explicit conversion to convert an enum type `E1` to another enum type `E2`, such an explicit conversion is allowed if and only if `E2` contains all enum values of `E1`; the converted value is unchanged, otherwise a compile-time error is reported. Within a safe zone, such a conversion cannot be performed implicitly. Within a safe zone, the operation of converting any type with a larger expressible range to an enum type is not allowed. In a non-safe zone, the behavior is consistent with C, and no additional checks are performed on enum type conversions.

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

    18.5. When using an explicit conversion within a safe zone to convert a floating-point type to another floating-point type with lower precision, if the original value can be represented exactly in the target type, the converted value is unchanged. If the original value is within the representable range of the target type but cannot be represented exactly, then the converted result is the nearest value approximated upward or downward, depending on the specific implementation. If the original value is outside the representable range of the target type, the specific value depends on the specific implementation. The current BiSheng C compiler implementation conforms to the IEEE 754 specification. Within a safe zone, such a type conversion cannot be performed implicitly. In a non-safe zone, the behavior is consistent with C, with the same semantics and support for performing such a type conversion implicitly.

    ```c
    _Safe {
      double a = 1.0;
      float b = 0.0f;
      b = (float)a; // ok: double -> float
      b = a; // error: cannot implicitly cast double to float
      a = b; // ok
    }
    ```

    18.6. When using an explicit conversion within a safe zone to convert an integer type to a floating-point type, if the original value can be represented exactly in the target type, the converted value is unchanged. If the original value is within the representable range of the target type but cannot be represented exactly, then the converted result is the nearest value approximated upward or downward, depending on the specific implementation. The current BiSheng C compiler implementation conforms to the IEEE 754 specification and uses the default rounding rule in IEEE 754. Within a safe zone, such a type conversion cannot be performed implicitly. Within a safe zone, conversion from a floating-point type to an integer type is not allowed. In a non-safe zone, the behavior is consistent with C, with the same semantics and support for performing such a type conversion implicitly.

    ```c
    _Safe {
      int a = -1;
      double b = a; // error: cannot implicitly cast int to double in safe zone
      b = (double) a; // ok: int -> double
      a = (int) b; // error: cannot cast double to int in safe zone
    }
    ```

    18.7. The result of the comparison operators (`==`, `!=`, `>=`, `<=`, `>`, `<`) and the logical operators (`&&`, `||`, `!`) is 0 or 1 of type `int`; these values are allowed to be implicitly converted to other integer types within a safe zone (because any basic integer type can represent 0 and 1).

    ```c
    _Safe {
      int a = 0, b = 1;
      _Bool c = (a == 0) || (b == 0); // ok: int (0/1) -> _Bool
      char x = a < 3 || a >= 6; // ok: int (0/1) -> char
    }
    ```

    18.8. For a type conversion of a **constant whose value can be determined at compile time**, if the target type can represent the value exactly, implicit conversion is allowed; otherwise an explicit conversion is required and must conform to the above rules.

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

    18.9. The type requirements for the conditions of `if/while` are currently consistent with C; any arithmetic type can serve as a condition.

    ```c
    int a = 2;
    _Safe {
      if (a) { // ok: if can accept int
        a += 1;
      } else {
        a -= 1;
      }
    }
    ```

19. Inline assembly statements are not allowed within a safe zone.

```c
void test() {
  _Safe {
    int ret = 0;
    int src = 1;
    asm("move %0, %1\n\t" : "=r"(ret) : "r"(src)); // error: inline assembly is not allowed in the safe zone
  }
}
int main() { return 0; }
```

21. Within a safe zone, the result type of the prefix/postfix increment (`++`) and decrement (`--`) expressions is `void`; that is, only their side effect (incrementing or decrementing by 1) is allowed to be used, and the value of the `++`/`--` expression must not be used. In a safe function with a return type of `void`, the result of `++`/`--` must not be used directly as the return value of a return statement.

```c
safe void take_int(int x);
safe void foo(void) {
  int a = 0;
  a++; // ok: the safe zone allows increment and decrement (side effect only, the expression result is void)
  a--;
  int x = a++; // error: the result of increment/decrement is of type void and cannot be used to initialize an int variable
  int arr[5] = {1, 2, 3, 4, 5};
  arr[a++] = 0; // error: the result of increment/decrement is of type void and cannot be used as an arr[] subscript
  take_int(a++); // error: the result of increment/decrement is of type void and cannot be used as an argument to take_int()
  unsafe {take_int(a++);} // ok: the non-safe zone is not affected
  for (int i = 0; i < 10; i++) {} // ok: the iteration part of a for loop can use ++/--
  int y = 0;
  y = (a++, a); // ok: a++ is evaluated first and the increment is completed, then the a on the right side of the comma is evaluated
  return a++; // error: the result of ++/-- must not be used directly as the return value
  return (void)a++; // ok
}

```
