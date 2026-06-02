# Safe Zone

## Overview

The C language has many rules that are too flexible, making it inconvenient for the compiler to perform static checks. We therefore introduce a new syntax so that, within a certain scope, BiSheng C code must follow stricter constraints, guaranteeing that code within this scope will never have "memory safety" problems.

The `_Safe` and `_Unsafe` keywords are allowed to modify functions, statements, and parenthesized expressions.

- `_Unsafe` indicates that this piece of code is in an unsafe zone. This code follows the rules of standard C, and its safety is guaranteed by the user.

- `_Safe` indicates that this piece of code is in a safe zone. This code must follow stricter constraints, and the compiler can guarantee memory safety.

- A global function not modified by `_Safe` or `_Unsafe` is unsafe by default.

## Code Example

```c
#include <stdlib.h>

typedef struct File {
} FileID;

// No keyword modifier, meaning it follows the default unsafe zone
FileID *_Owned create(void) {
  FileID *p = malloc(sizeof(FileID));
  return (FileID * _Owned) p;
}
FileID *_Owned foo(FileID *_Owned p) { return p; }

// Modifying a function with _Safe means the function is a safe function, and its body is a safe zone
_Safe void safely_free(FileID *_Owned p) {
  // Modifying a code block with _Unsafe means this code is in an unsafe zone. This code is an unsafe zone within a safe zone, and it also belongs to the unsafe zone
  _Unsafe { free((FileID *)p); }
}

int main(void) {
  FileID *_Owned p1 = create();
  FileID *_Owned p2 = foo(p1);
  // Modifying a statement with _Safe means this code is in a safe zone
  _Safe safely_free(p2);
  return 0;
}
```

## Grammar Rules

1. `_Safe/_Unsafe` is allowed to modify function declarations, function signatures, function definitions, function pointers, statements, and parenthesized expressions.

```c
// Modifying a function signature
_Safe int foo(int, int);
// Modifying a function declaration
_Safe int bar(int a, int b);
// Modifying a function definition
_Safe int add(int a, int b) { return a + b; }
// Modifying a function pointer
_Safe int (*p)(int, int);

_Safe int main(void) {
  // Modifying a code block
  _Safe {
    int a = 1;
  }
  _Unsafe { int b = 1; }
  // Modifying a statement
  _Safe int c = 1;
  _Unsafe c++;
  // Modifying a parenthesized expression
  char d = _Unsafe((char)c);
  return 0;
}
```

2. `_Safe/_Unsafe` cannot modify global variables, type declarations outside functions, or `typedef` declarations (modifying function pointers is allowed).

```c
_Safe int g_a; // error: cannot modify a global variable

_Safe struct b { int a; }; // error: cannot modify a type declaration outside a function

_Safe typedef int mm; // error: cannot modify a typedef

int main() { return 0; }
```

3. For a function modified by `_Safe`, the parameter types and return type are allowed to be raw pointer types, `struct` types whose members contain raw pointers, array types, and `union` types.

```c
_Safe int *foo(int a); // ok: return value is a raw pointer type

_Safe int bar(int *a); // ok: parameter type is a raw pointer type

typedef struct F {
  int *a;
} SF;

_Safe SF wrapped_foo(int a); // ok: return value is a struct type whose members contain a raw pointer type

_Safe int wrapped_bar(SF b); // ok: parameter type is a struct type whose members contain a raw pointer type
```

4. For a function modified by `_Safe`, the function parameter list cannot be omitted.

```c
_Safe void test(); // error

_Safe void test(void); // ok
```

5. For a function modified by `_Safe`, the function parameter list cannot contain variadic arguments, unless the function uses the `__attribute__((format(...)))` attribute.

```c
_Safe int foo(int a, ...); // error
__attribute__((format(printf, 1, 2))) _Safe int bar(const char *fmt, ...); // ok
```

Note: Even though declaring a variadic function with the format attribute is allowed, `va_start`, `va_arg`, `va_end`, etc. still cannot be used within the function body; these operations are forbidden inside a safe zone.

6. If a function in a `_Trait` is declared as `_Safe`, then the corresponding member function of the type implementing the `_Trait` must also be a function modified by `_Safe`. If a function in a `_Trait` is not declared as `_Safe`, the member function of the type implementing the `_Trait` is still allowed to be `_Safe`, but the compiler will issue a **warning**.

```c
_Trait G {
  _Safe int *_Owned foo(This * _Owned this);
  int *_Owned bar(This * _Owned this);
};

_Safe int *_Owned int ::foo(int *_Owned this) { return this; } // ok: the _Trait implementation function must be _Safe
// The implementation of a non-_Safe function can be _Safe
_Safe int *_Owned int ::bar(int *_Owned this) { return this; }
_Impl _Trait G for int;

int main() { return 0; }
```

7. Multiple declarations of a function and mixed mode

The same function identifier may have multiple declarations, supporting mixed `_Safe` and `_Unsafe` declarations.

- 7.1 Compatibility requirements

   **Same modifier**: Between multiple `_Safe` declarations, or between multiple `_Unsafe` declarations, the function types must be compatible

   **Mixed mode**: `_Safe` and `_Unsafe` declarations can coexist, but they must satisfy mixed-mode compatibility (see 7.3)

   **Generic functions excepted**: Generic functions do not support mixed mode; the same instantiation cannot have both a `_Safe` and an `_Unsafe` declaration

   **Member functions**: Same rules as ordinary functions

- 7.2 Function type compatibility

   **Conditions for two function types to be compatible**:
  - Compatible return types
  - Same number of parameters, with consistent use of the ellipsis (`...`)
  - Compatible corresponding parameter types (during the compatibility check, all qualifiers are removed except `_Owned`, `_Borrow`, `_ArrayElem`, `_Nonnull`, `_Nullable`)

  **Pointer compatibility**:

  - Pointer types require the same qualifiers (`_Owned`, `_Borrow`, `_ArrayElem`, `_Nonnull`, `_Nullable`) and compatible target types
  - `_Owned` and `_Borrow` pointers (and their versions with `_ArrayElem`) are pairwise incompatible with each other
  - `_Owned`/`_Borrow` pointers (and their versions with `_ArrayElem`) are incompatible with raw pointers (except in mixed mode, see 7.3)

- 7.3 Mixed-mode compatibility (`_Unsafe` coexisting with `_Safe`)

   **Return type compatibility**:

  - The return types in the `_Safe` declaration and the `_Unsafe` declaration are compatible after removing the type qualifiers introduced by BiSheng C (`_Owned`, `_Borrow`, `_ArrayElem`, etc.) while **retaining** the other C qualifiers (`const`, `volatile`, etc.).
  - The `_Safe` declaration may only **add** the `_Owned`, `_Borrow`, `_Owned _ArrayElem`, or `_Borrow _ArrayElem` qualifiers to the return type; it cannot **remove** existing qualifiers. `_Owned _ArrayElem` and `_Borrow _ArrayElem` should be treated as a whole when added.

   **Parameter type compatibility**:

  - The parameter types in the `_Safe` declaration and the `_Unsafe` declaration are compatible after removing the type qualifiers introduced by BiSheng C (`_Owned`, `_Borrow`, `_ArrayElem`, etc.) and also **removing** the other C qualifiers (`const`, `volatile`, etc.).
  - The `_Safe` declaration may only **add** the `_Owned`, `_Borrow`, `_Owned _ArrayElem`, or `_Borrow _ArrayElem` qualifiers to a parameter; it cannot **remove** existing qualifiers. `_Owned _ArrayElem` and `_Borrow _ArrayElem` should be treated as a whole when added.

   **Examples**:

   ```c
   // ok: the _Safe declaration adds the _Owned qualifier
   _Unsafe int* f1(int* p);
   _Safe int* _Owned f1(int* _Owned p);

   // ok: the _Safe declaration adds the _Borrow qualifier
      _Unsafe int* f2(int* p);
   _Safe int* _Borrow f2(int* _Borrow p);

   // ok: the _Safe declaration adds the _Owned _ArrayElem qualifier
   _Unsafe int* f3(int* p);
   _Safe int* _Owned _ArrayElem f3(int* _Owned _ArrayElem p);

   // ok: the _Safe declaration adds the _Borrow _ArrayElem qualifier
   _Unsafe int* f4(int* p);
   _Safe int* _Borrow _ArrayElem f4(int* _Borrow _ArrayElem p);

   // error: the _Safe declaration removes the _Owned qualifier from the _Unsafe declaration
   int* _Owned f5(int* _Owned p);
   _Safe int* f5(int* p);

   // error: the _Safe declaration removes the _Borrow qualifier from the _Unsafe declaration
   int* _Borrow f6(int* _Borrow p);
   _Safe int* f6(int* p);

   // error: the _Safe declaration removes the _Owned _ArrayElem qualifier from the _Unsafe declaration
   int* _Owned _ArrayElem f7(int* _Owned _ArrayElem p);
   _Safe int* f7(int* p);

   // error: the _Safe declaration removes the _Borrow _ArrayElem qualifier from the _Unsafe declaration
   int* _Borrow _ArrayElem f8(int* _Borrow _ArrayElem p);
   _Safe int* f8(int* p);

   // error: the _Safe declaration cannot add _ArrayElem when the _Unsafe declaration has no _ArrayElem qualifier
   int* _Borrow  f9(int* _Borrow p);
   _Safe int* _Borrow _ArrayElem f9(int* _Borrow _ArrayElem p);
   int* _Owned f10(int* _Owned p);
   _Safe int* _Owned _ArrayElem f10(int* _Owned _ArrayElem p);

   // error: owned and borrow are incompatible
   _Unsafe int* _Owned f11(int* _Borrow p);
   _Safe int* _Borrow f11(int* _Owned p);

   // ok: same modifier, exactly the same types
   _Safe int* _Owned f12(int* _Owned p);
   _Safe int* _Owned f12(int* _Owned q);
   ```

- 7.4 Function definition

   A mixed-mode function can only be defined **once**

   The definition can be based on the `_Unsafe` version or the `_Safe` version, but it must be compatible with all declarations

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

   In a safe context (a `_Safe` block), only `_Safe` functions can be called. If a function has only an `_Unsafe` declaration, it is a compilation error.

   ```c
   _Unsafe void foo(void);
   
   int main() {
     _Safe {
       foo();  // error: calling a non-safe function is not allowed inside a safe zone
     }
   }
   ```

- 8.2 Calls in an unsafe context

   In an unsafe context (an `_Unsafe` block or by default), overload resolution is performed:

  - The `_Safe` declaration is matched first
  - When the `_Safe` declaration does not match, the `_Unsafe` declaration is used

   ```c
   _Unsafe int* foo(int* p);
   _Safe int* _Owned foo(int* _Owned p);
   
   int *_Owned bar() {
     int* raw_p = nullptr;
     int* _Owned owned_p = nullptr;
   
     // The corresponding version is chosen based on the argument types
     foo(raw_p);      // calls the unsafe version
     return foo(owned_p);    // calls the safe version
   }
   ```

9. Function pointer assignment

Function pointer assignment rules:

**When assigning to a function pointer modified by `_Safe`**:

- If the function identifier being assigned has no `_Safe` version declaration, an error is reported; that is, an `_Unsafe` version declaration is not allowed to be assigned to a function pointer modified by `_Safe`
- If the function identifier being assigned has a `_Safe` version declaration, the type of the `_Safe` version declaration is required to be compatible with the function pointer type

**When assigning to a function pointer modified by `_Unsafe`**:

- As long as the `_Unsafe` version declaration type of the function identifier being assigned is compatible with the function pointer type, or the `_Safe` version declaration type is `_Unsafe-_Safe` compatible with the function pointer type, the assignment is allowed
- If neither is satisfied, a compilation error is reported

**Example**:

```c
_Safe void safe_foo(void);
_Unsafe void unsafe_foo(void);

// Mixed-mode declaration
_Unsafe int* bar(int* p);
_Safe int* _Owned bar(int* _Owned p);

int main() {
  _Safe void (*safe_ptr)(void) = nullptr;
  _Unsafe void (*unsafe_ptr)(void) = nullptr;

  safe_ptr = safe_foo;      // ok: a safe function is assigned to a safe pointer
  safe_ptr = unsafe_foo;    // error: there is no safe version declaration

  unsafe_ptr = unsafe_foo;  // ok: an unsafe function is assigned to an unsafe pointer
  unsafe_ptr = safe_foo;    // ok: the safe version is unsafe-safe compatible

  // Mixed-mode function pointer assignment
  _Safe int* _Owned (*safe_bar_ptr)(int* _Owned) = nullptr;
  _Unsafe int* (*unsafe_bar_ptr)(int*) = nullptr;

  safe_bar_ptr = bar;   // ok: uses the safe version
  unsafe_bar_ptr = bar; // ok: uses the unsafe version or the safe version (_Unsafe-safe compatible)
}
```

A parameter declared as an array type in a function parameter is equivalent to the corresponding pointer-type parameter, and the two can be interchanged during function pointer assignment:

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

10. When `_Safe` modifies a generic function, the `_Safe` check is also performed for each instantiated version of the generic.

```c
#include "bishengc_safety.hbs" // a header file provided by BiSheng C, used for safely allocating and freeing memory

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

11. Member functions can also be modified by `_Safe/_Unsafe`, and the rules are the same as for global functions.

```c
struct MyStruct<T> {
  T res;
};
_Safe T struct MyStruct<T>::foo(T a) {
  return a;
}

int main() { return 0; }
```

12. A function or function pointer called inside a safe zone must have a `_Safe` function signature; calling a non-safe function or function pointer is not allowed.

```c
_Safe void safe_foo(void) {}
_Unsafe void unsafe_foo(void) {}
_Safe void (*safe_func_ptr)(void);
_Unsafe void (*unsafe_func_ptr)(void);
int main() {
  _Safe {
    safe_foo();
    unsafe_foo(); // error: calling a non-safe function is not allowed inside a safe zone
    safe_func_ptr();
    unsafe_func_ptr(); // error: calling a non-safe function pointer is not allowed inside a safe zone
  }
  _Unsafe {
    safe_foo();
    unsafe_foo(); // ok
    safe_func_ptr();
    unsafe_func_ptr(); // ok
  }
}
```

13. A safe zone is allowed to further contain statements, function pointers, and parenthesized expressions modified by `_Unsafe`, and an unsafe zone is also allowed to further contain statements, function pointers, and parenthesized expressions modified by `_Safe`.

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

14. Inside a safe zone, the `case/default` labels in a `switch` statement can only exist in the first-level code block following the `switch`, and the first-level code block is not allowed to have variable definitions.

```c
_Safe void foo(int a) {
  switch (a) {
    int b = 10; // error: the first-level code block is not allowed to have variable definitions
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

15. Inside a safe zone, accessing the members of a `union` (reading or writing) is not allowed, and accessing members through `->` on a raw pointer is not allowed.

    Inside a safe zone, defining and declaring `union` types is allowed, and a `union` is allowed as a function parameter and return type, but accessing any member of a `union` through the `.` operator is not allowed.

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

    a++; // ok: the safe zone allows increment// ok : explicit conversion to a void-type owned pointer is allowed
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

16. Inside a safe zone, using the address-of operator `&` is not allowed (taking the address of a function is allowed); only `&_Const` and `&_Mut` are allowed for taking a borrow.

    Inside a safe zone, dereferencing a raw pointer is not allowed, but `_Owned` pointers, `_Borrow` pointers, and non-null function pointers can be dereferenced.

```C

_Safe int inc(int a) { return a + 1; }

_Safe int test1(unary_f _Nonnull f) {
  return (*f)(1);  // ok: _Nonnull function pointer
}

_Safe int test2(unary_f f) {
  if (f != nullptr) {
    return (*f)(1);  // ok: confirmed non-null after the null check
  }
  return 0;
}
```

```c
#include "bishengc_safety.hbs" // a header file provided by BiSheng C, used for safely allocating and freeing memory

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

17. Inside a safe zone, conversion between pointer types pointing to different types is not allowed, with the following exceptions:
    1. An owned pointer pointing to another type is allowed to be explicitly converted to an owned pointer pointing to the void type; this conversion must comply with the rules of [Ownership State Transfer Rules - Explicit Type Conversion](../chapter-3-memory-safety/1-ownership.md#type-casts).
    2. A borrow pointer pointing to another type is allowed to be implicitly converted to a borrow pointer pointing to the void type, but the original type must satisfy the is_trivial_data condition (containing no pointers and no _Owned struct); otherwise the conversion is not allowed.

    Inside a safe zone, conversion between pointer and non-pointer types is not allowed. An array can decay to a raw pointer by default; in assignment, function argument passing, and return value scenarios, if the target type is a `T *_Borrow` or `T *_Borrow _ArrayElem` matching the array element type, the array is also allowed to decay to the corresponding borrow pointer. Apart from this array decay rule, conversion between `_Owned/_Borrow/raw` pointers is not allowed inside a safe zone; in addition, `T *_Borrow _ArrayElem` is allowed to be converted to `T *_Borrow`.

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
    pa = i; // error: conversion between pointer and non-pointer types is not allowed
    i = pa; // error: conversion between pointer and non-pointer types is not allowed
  }
  int *_Owned pd = (int *_Owned)pa;
  _Safe {
    pa = pd; // error: conversion between _Owned/raw pointers is not allowed
    pd = pa; // error: conversion between _Owned/raw pointers is not allowed

    void *_Owned pe = (void *_Owned)pd; // ok: explicit conversion to a void-type owned pointer is allowed
  }
  struct S {int *ptr;};
  struct S s = {.ptr = nullptr};
  int a = 0;
  _Safe {
    int *_Borrow p1 = &_Mut a;
    void *_Borrow p2 = p1; // ok: int satisfies the is_trivial_data constraint, so int *_Borrow is allowed to be implicitly converted to void *_Borrow
    struct S *_Borrow p3 = &_Mut s;
    void *_Borrow p4 = (void *_Borrow)p3; // error: struct S does not satisfy is_trivial_data, so struct S *_Borrow is not allowed to be converted to void *_Borrow
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
  return s->data; // ok: the array decays to T *_Borrow _ArrayElem
}

_Safe int *_Borrow f5(S *_Borrow s) {
  return s->data; // ok: the array decays to T *_Borrow
}

_Safe int * f6(S *_Borrow s) {
  return s->data; // ok: the array decays to T *
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

18. Inside a safe zone, **implicitly performing** a type conversion that narrows the range of representation (for example, from `long` to `int`, from `int` to `_Bool`, from `int` to `enum`) is not allowed, and **implicitly performing** a type conversion that lowers precision (for example, from `double` to `float`) is not allowed. Inside a safe zone, converting an arithmetic type to an enumeration type is not allowed, except for an explicit conversion from one enumeration type to an enumeration type with an equal or larger range of representation. Inside a safe zone, conversion from a floating-point type to an integer type is not allowed. For a type conversion of a **constant whose value can be determined at compile time**, if the target type can represent this value, then the type conversion can be performed implicitly inside the safe zone; otherwise such a conversion must be performed explicitly and must comply with the above rules.

    **Semantic rules and detailed explanation:**
    Excluding enumeration types, the type conversions allowed inside the safe zone are shown in the diagram below: (If implicit conversion is supported in a given direction, explicit conversion is certainly supported; if only explicit conversion is required, it means implicit conversion is not allowed. When an edge representing an allowed conversion has one end connected to a subgraph, it is equivalent to connecting to all nodes of that subgraph; for example, `_Bool` has an edge pointing to integers, meaning `_Bool` is allowed to be converted to any integer type under the same rules.)

    ``` mermaid
    graph TB
      bool["_Bool"]
      subgraph int["Integer"]
        direction LR
        subgraph signedint["Signed integer"]
          direction TB
          signedint_s["Small type"]
          signedint_l["Large type"]
          signedint_s -- forward implicit, reverse explicit --> signedint_l
        end
        subgraph unsignedint["Unsigned integer"]
          direction TB
          unsignedint_s["Small type"]
          unsignedint_l["Large type"]
          unsignedint_s -- forward implicit, reverse explicit --> unsignedint_l
        end
        signedint <-- explicit conversion required both ways --> unsignedint
      end
      bool -- forward implicit, reverse explicit --> int
      subgraph fp["Floating-point"]
      direction TB
        fp_s["Small type"]
        fp_l["Large type"]
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

    18.2. When converting a signed integer type or an unsigned integer type with a larger range of representation to an **unsigned** integer type, if the magnitude of the value of the original type is within the range of the target type, the magnitude of the converted value is unchanged; otherwise, the magnitude of the converted value is "the value obtained by repeatedly adding or subtracting (1 + the maximum value of the target type) to the original value until the result falls within the range of the target type" (consistent with C). If the range of representation of the target unsigned integer type is from 0 to $2^N-1$ and the value of the original type is $X$, then the magnitude of the converted value is $X$ repeatedly added or subtracted by $2^N$ until the result $Y$ falls between 0 and $2^N-1$, and the converted value is $Y$. The above is equivalent to truncating the large integer type to the small unsigned integer type, or converting the small signed integer type to the unsigned integer type via sign-bit extension.

    ```c
    unsigned long a = 2;
    unsigned b = (unsigned) a; // ok: unsigned long -> unsigned int
    int c = -1;
    unsigned d = (unsigned) c; // ok: int -> unsigned int; d = 2^32 - 1
    unsigned long e = (unsigned long) c; // ok: int -> unsigned long; e = 2^64 - 1
    ```

    18.3. When converting an integer type with a larger range of representation to a **signed** integer type with a smaller range of representation (not including enumeration types), if the magnitude of the value of the original type is within the range of the target type, the magnitude of the converted value is unchanged; otherwise, the magnitude of the converted value is implementation-defined. In the current BiSheng C compiler implementation, when the original value exceeds the representable range of the target type, the large integer type is truncated to the small signed integer type.

    ```c
    long a = 2;
    int b = (int) a; // ok: long -> int
    unsigned c = 4294967295; // c = 2^32 - 1
    int d = (int) c; // ok: unsigned int -> int; d = -1
    ```

    18.4. Regarding enumeration types: the safe zone only allows an explicit conversion from one enumeration type to another enumeration type (with restricting conditions); no other type can be converted to an enumeration type. An enumeration type can be implicitly converted to its corresponding integer type (for example, given `enum E : unsigned {...}`, `enum E` can be implicitly converted to `unsigned`). When using an explicit conversion to convert an enumeration type `E1` to another enumeration type `E2`, such an explicit conversion is allowed if and only if `E2` contains all the enumeration values of `E1`, in which case the converted value is unchanged; otherwise a compile-time error is reported. Inside a safe zone, such a conversion cannot be performed implicitly. Any operation that converts another type with a larger range of representation to an enumeration type is not allowed inside the safe zone. In an unsafe zone, the behavior is consistent with C, performing no additional checks on enumeration type conversions.

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

    18.5. When using an explicit conversion inside a safe zone to convert a floating-point type to another floating-point type of lower precision, if the original value can be represented exactly in the target type, the converted value is unchanged. If the original value is within the representable range of the target type but cannot be represented exactly, the converted result is the nearest approximation rounded up or down, depending on the implementation. If the original value is outside the representable range of the target type, the specific value depends on the implementation. The current BiSheng C compiler implementation should comply with the IEEE 754 specification. Inside a safe zone, such a type conversion cannot be performed implicitly. In an unsafe zone, the behavior is consistent with C: the semantics are the same and such a type conversion can be performed implicitly.

    ```c
    _Safe {
      double a = 1.0;
      float b = 0.0f;
      b = (float)a; // ok: double -> float
      b = a; // error: cannot implicitly cast double to float
      a = b; // ok
    }
    ```

    18.6. When using an explicit conversion inside a safe zone to convert an integer type to a floating-point type, if the original value can be represented exactly in the target type, the converted value is unchanged. If the original value is within the representable range of the target type but cannot be represented exactly, the converted result is the nearest approximation rounded up or down, depending on the implementation. The current BiSheng C compiler implementation should comply with the IEEE 754 specification and use the default rounding rule in IEEE 754. Inside a safe zone, such a type conversion cannot be performed implicitly. Inside a safe zone, conversion from a floating-point type to an integer type is not allowed. In an unsafe zone, the behavior is consistent with C: the semantics are the same and such a type conversion can be performed implicitly.

    ```c
    _Safe {
      int a = -1;
      double b = a; // error: cannot implicitly cast int to double in safe zone
      b = (double) a; // ok: int -> double
      a = (int) b; // error: cannot cast double to int in safe zone
    }
    ```

    18.7. The result of comparison operators (`==`, `!=`, `>=`, `<=`, `>`, `<`) and logical operators (`&&`, `||`, `!`) is an `int` of value 0 or 1; these values are allowed to be implicitly converted to other integer types inside a safe zone (because any basic integer type can represent 0 and 1).

    ```c
    _Safe {
      int a = 0, b = 1;
      _Bool c = (a == 0) || (b == 0); // ok: int (0/1) -> _Bool
      char x = a < 3 || a >= 6; // ok: int (0/1) -> char
    }
    ```

    18.8. For a type conversion of a **constant whose value can be determined at compile time**, if the target type can represent the value exactly, implicit conversion is allowed; otherwise an explicit conversion is required and it must comply with the above rules.

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

    18.9.  The type requirements of the `if/while` conditions are currently consistent with C; any arithmetic type can serve as a condition.

    ```c
    int a = 2;
    _Safe {
      if (a) { // ok: if can accept an int
        a += 1;
      } else {
        a -= 1;
      }
    }
    ```

2.  Inline assembly statements are not allowed inside a safe zone.

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

21. Inside a safe zone, the result type of prefix/postfix increment (`++`) and decrement (`--`) expressions is `void`; that is, only their side effects (increment or decrement by 1) are allowed, and the value of the `++`/`--` expression must not be used. In a safe function whose return type is `void`, the result of `++`/`--` must not be directly used as the return value of a return statement.

```c
safe void take_int(int x);
safe void foo(void) {
  int a = 0;
  a++; // ok: the safe zone allows increment and decrement (side effect only, the expression result is void)
  a--;
  int x = a++; // error: the result of increment/decrement is of void type and cannot be used to initialize the int variable x
  int arr[5] = {1, 2, 3, 4, 5};
  arr[a++] = 0; // error: the result of increment/decrement is of void type and cannot be used as the arr[] subscript
  take_int(a++); // error: the result of increment/decrement is of void type and cannot be used as an argument to take_int()
  unsafe {take_int(a++);} // ok: the unsafe zone is not affected
  for (int i = 0; i < 10; i++) {} // ok: the iteration part of a for loop can use ++/--
  int y = 0;
  y = (a++, a); // ok: a++ is evaluated first and the increment completes, then a on the right side of the comma is evaluated
  return a++; // error: the result of ++/-- must not be directly used as the return value
  return (void)a++; // ok
}

```
