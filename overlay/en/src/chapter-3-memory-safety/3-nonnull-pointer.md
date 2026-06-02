# Non-Null Pointers

To improve the safety of pointer usage, BSC introduces the concept of pointer nullability.

Based on whether a pointer can be directly dereferenced, two attributes can be used to modify any pointer type (raw pointers, `_Owned` pointers, and `_Borrow` pointers).

1. A pointer modified by the keyword `_Nullable` is a nullable pointer.
2. A pointer modified by the keyword `_Nonnull` is a non-null pointer.

The BSC compiler checks pointer nullability at compile time, preventing unsafe behaviors such as dereferencing a null pointer or accessing members through a null pointer.

BSC does not allow the use of the macro `NULL` inside a safe zone, but C-style programming inevitably uses null pointers to express logic:

1. A pointer's target may only be determined at run time, so we can initialize the pointer to a null pointer and later modify its target according to the runtime state.
2. For a nullable pointer, you must check whether the pointer is null before using it.

BSC introduces the `nullptr` keyword to replace `NULL`.
Users can define a Nullable pointer and initialize it to `nullptr`.
A Nullable pointer must be null-checked before it can be dereferenced.

Let's use a simple example to learn how to define a pointer's nullability and use it:

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

// Function that gets the current runtime status
_Safe int get_current_status(void) {
  _Unsafe { return rand() % 2; }
}

_Safe void read_data<T>(T *_Borrow p) {}

struct Data {
  int value;
};

// If init succeeds, return a concrete address; otherwise return nullptr
_Safe struct Data *_Owned _Nullable init_data(void) {
  if (get_current_status() == 1) {
    struct Data data = {10};
    struct Data *_Owned p = safe_malloc<struct Data>(data);
    return p;
  }
  return nullptr;
}

_Safe int main(void) {
  // Use _Nullable to modify the pointer type:
  struct Data *_Owned _Nullable p = init_data();
  // After init, p may be a null pointer, so it must be null-checked before use:
  if (p != nullptr) {
    // If p is not null-checked, the compiler will report an error!
    read_data(&_Mut * p);
    safe_free((void *_Owned)p);
  }
  return 0;
}
```

## Which Pointers' Nullability Can the Compiler Check

You can use `_Nullable` and `_Nonnull` to modify any pointer type (raw pointers, `_Owned` pointers, and `_Borrow` pointers),
but the compiler can only track a pointer's nullability — and thereby protect against null pointer dereferences — when the pointer satisfies the following conditions:

1. It is an lvalue (it has a memory address).
2. It is not modified by `volatile` (the pointer itself is not volatile).
3. It is not a pointer obtained by accessing an array.

A pointer that does not meet the above requirements cannot have its nullability modified by a null check. If an untrackable pointer `p1` is `_Nullable` and the user needs to dereference it, first create a trackable pointer variable `p2`, initialize it with `p1`, and then null-check and dereference `p2`.

Example:

```c
// A function's return value is not an lvalue
_Safe int *_Borrow _Nullable identity(int *_Borrow _Nullable p) { return p; }

_Safe void test(int *_Borrow _Nullable p, int *_Borrow _Nullable volatile vp) {
  int local = 0;
  int *_Borrow _Nullable q = nullptr;
  int *_Borrow _Nullable arr[2] = {nullptr, &_Mut local};

  if ((q = p) != nullptr) {
    *q = 1; // ok: q is an lvalue pointer, so the compiler can track its nullability
  }

  if ((0, q) != nullptr) {
    *q = 2; // ok: the comma expression ultimately extracts q, which is still trackable
  }

  if (identity(p) != nullptr) { // not an lvalue, so its nullability cannot be updated
    *identity(p) = 3; // error
  }
  int *_Borrow _Nullable t1 = identity(p); // should create a trackable temporary variable to receive it
  if (t1 != nullptr) {
    *t1 = 3; // ok
  }

  if (vp != nullptr) { // volatile pointer, so its nullability cannot be updated
    *vp = 4; // error
  }
  int *_Borrow _Nullable t2 = vp; // should create a trackable temporary variable to receive it
  if (t2 != nullptr) {
    *t2 = 4; // ok
  }

  if (arr[1] != nullptr) { // the pointer expression contains a subscript operation '[]', so the pointer's nullability cannot be updated
    *arr[1] = 5; // error
  }
  int *_Borrow _Nullable t3 = arr[1];  // should create a trackable temporary variable to receive it
  if (t3 != nullptr) {
    *t3 = 5; // ok
  }
}
```

## Nullability of Pointer Types

A pointer type's nullability marker depends on the pointer's type and its modifiers:

1. Raw pointers are Nullable, while `_Owned` and `_Borrow` pointers are Nonnull.
2. You can explicitly use the `_Nonnull` and `_Nullable` modifiers to override the above rules.

```C
// Nullable pointers:
int *_Nullable p1 = nullptr;
int *_Borrow _Nullable p2 = nullptr;
int *_Owned _Nullable p3 = nullptr;
int *p4 = nullptr;
int (*p5)(int) = nullptr;
// Nonnull pointers:
int *_Nonnull p5 = &a;
int *_Borrow _Nonnull p6 = &_Mut a;
int *_Owned _Nonnull p7 = safe_malloc<int>(5);
int *_Borrow p8 = &_Mut a;
int *_Owned p9 = safe_malloc<int>(5);
```

For a pointer whose nullability marker is Nonnull, its nullability state must be Nonnull.
If a null check is performed on it in a control-flow statement, then in the null branch it is considered to be a null pointer.

```C
#include "bishengc_safety.hbs" // Header file provided by the BiSheng C language, used for safe memory allocation and deallocation

_Safe int test(void) {
  int * _Owned a = safe_malloc<int>(1); // an _Owned pointer is _Nonnull by default
  if (a != nullptr) {
    safe_free((void * _Owned)a);
    return 1;
  }
  return 0; // ok: there will be no error: memory leak of value: `a`
}

int main() {
  test();
  return 0;
}
```

For a pointer whose nullability marker is Nullable, its nullability state may change:

1. If it is assigned a non-null expression, then after this assignment statement its nullability state becomes Nonnull.
2. If a null check is performed on it in a control-flow statement, then in the non-null branch its nullability state becomes Nonnull.

```C
// Return type is Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }
// Return type is Nonnull
_Safe T *_Borrow bar<T>(T *_Borrow p) { return p; }

_Safe void test(void) {
  int *_Borrow _Nullable p1 = nullptr; // after initialization, p1's Nullability is Nullable
  *p1 = 10;                           // error

  int local = 10;
  p1 = &_Mut local;      // after p1 is reassigned, its Nullability becomes Nonnull
  *p1 = 20;             // ok

  p1 = foo(&_Mut local); // after p1 is reassigned, its Nullability becomes Nullable
  *p1 = 20;             // error
  
  p1 = bar(&_Mut local); // after p1 is reassigned, its Nullability becomes Nonnull
  *p1 = 20;             // ok

  int *_Borrow _Nullable p2 = foo(&_Mut local); // Nullable
  if (p2 != nullptr) // in the if branch, p2's Nullability is Nonnull
    *p2 = 10;        // ok
  else               // in the else branch, p2's Nullability is Nullable
    *p2 = 20;        // error
}

int main() {
  test();
  return 0;
}
```

## What Is a Null-Check Statement

In the conditions of conditional statements and loop statements, and in the condition expression of a ternary expression, only certain behaviors can be regarded as "checking whether a pointer is a null pointer":

1. Using the pointer `e` directly

- `if (p)`
- `while (s.p)`
- `p ? do_with(p) : do_without()`

2. The logical operators `!e` `e && f` `e || f`

- `if (!p)` `if (p && q)` `if (p || q)`
- `while (!s.p)`  `while (s.p && s.q)` `while (s.p || s.q)`

3. Explicit comparison against a null pointer `e == nullptr` `e != nullptr`

- The comparison operators are symmetric, so `nullptr == e` and `nullptr != e` are also supported

4. The assignment operator `e = ...` may appear

- `if (p = q)` can only check the nullability of `p`
- The assignment operator can be chained, but `if (p = q = r)` can only check the nullability of `p`

5. It can be part of a comma expression, i.e. `(..., e)`

- `if (x, p != nullptr)` can check the nullability of `p`
- `if (p != nullptr, q != nullptr)` can only check the nullability of `q`

6. The above patterns can be nested in parentheses

Here `e` must satisfy the "trackable" requirement described in section 3.3.1.

For these "null-check statements", the compiler updates the nullability state of `e` in the subsequent control flow:

- In the true branch (or loop body) of `if (e)` / `while (e)`, `e` is regarded as Nonnull
- In the false branch of `if (!e)`, `e` is regarded as Nonnull
- In the true branch of `if (e != nullptr)`, `e` is regarded as Nonnull
- In the false branch of `if (e == nullptr)`, `e` is regarded as Nonnull

```c

_Safe void test(int *_Borrow _Nullable p, int *_Borrow _Nullable q) {
  // 1) Direct pointer condition
  if (p) {
    *p = 1; // ok: p is regarded as nonnull in the true branch
  }

  // 2) Logical operators
  if (!p) {
    *p = 1; // error: p is regarded as nullable in the true branch
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

  // 3) Comparison against nullptr
  if (p != nullptr) {
    *p = 1; // ok
  }
  if (nullptr == p) {
    *p = 1; // error
  }

  // 4) Assignment / comma wrapping
  if ((q = p) != nullptr) {
    *q = 1; // ok
  }
  if ((0, q) != nullptr) {
    *q = 1; // ok
  }
}
```

## Pointer Assignment, Argument Passing, and Return

1. A pointer whose nullability marker is Nonnull may not be assigned a nullable expression:

```C
// Return type is Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }
// Return type is Nonnull
_Safe T *_Borrow bar<T>(T *_Borrow p) { return p; }

_Safe void test(void) {
  int *_Borrow p1 = nullptr; // error: assigning a nullable value to a nonnull pointer is forbidden in a safe zone

  int local = 10;
  int *_Borrow p2 = foo(&_Mut local); // error: assigning a nullable value to a nonnull pointer is forbidden in a safe zone
  int *_Borrow p3 = bar(&_Mut local); // ok

  int *_Borrow _Nullable p4 = foo(&_Mut local);
  int *_Borrow p5 = p4; // error: assigning a nullable value to a nonnull pointer is forbidden in a safe zone
}

int main() {
  test();
  return 0;
}
```

2. When calling a function, if the parameter type has a Nonnull nullability marker, you cannot pass a nullable expression as the argument:

```C
// Return type is Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }

// Takes a Nonnull pointer as a parameter
_Safe void bar(int *_Borrow p) {}

_Safe void test(void) {
  int local = 10;
  int *_Borrow _Nullable p = foo(&_Mut local);
  bar(p); // error: a nonnull pointer is required, but the argument is nullable
}

int main() {
  test();
  return 0;
}
```

3. If a function's return type has a Nonnull nullability marker, it cannot return the result of a nullable expression:

```C
_Safe int *_Borrow return_nonnull(int *_Borrow p) {
  int *_Borrow _Nullable q = nullptr;
  return q; // error: the returned pointer is required to be nonnull
}
int main() {
  int i = 1;
  int *_Borrow pi = return_nonnull(&_Mut i);
  return 0;
}
```

## Pointer Type Casts

After enabling the compiler option `-nullability-check=all`, casting a Nullable pointer to a Nonnull type is not allowed outside a safe zone:

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

If you null-check a Nullable pointer, then in the non-null branch you can cast it to a Nonnull type:

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

## Pointer Dereference and Member Access

The nullability state of a pointer whose nullability marker is Nullable changes with assignments and control flow.
The BSC compiler tracks these changes to guarantee the safety of operations such as pointer dereference and member access.

```C
// Return type is Nullable
_Safe T *_Borrow _Nullable foo<T>(T *_Borrow p) { return p; }

struct Data {
  int value;
};

_Safe void test(void) {
  struct Data data = {.value = 10};
  struct Data *_Borrow _Nullable p = foo(&_Mut data);
  if (p != nullptr) {
    // after the null check, nullable is converted to nonnull
    p->value = 10;
  }
}

int main() {
  test();
  return 0;
}
```

## Struct Members That Are Nullable Pointers

When initializing a struct variable that has Nullable pointer members:

1. If it is initialized through an initializer list, the BSC compiler initializes the Nullability of the Nullable pointer members according to the initializer expressions.
2. For other initialization methods, the Nullability of the Nullable pointer members is directly assumed to be Nullable. This may cause code that is otherwise correct to fail to compile; in this case you need to reassign the Nullable pointer member or use a null-check statement, either of which can change its Nullability.

```C
// Return type is Nonnull
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
  // Initialized using an initializer list: inferred as nonnull
  struct Data data1 = {.value = bar(&_Mut local)};
  *data1.value = 10;

  // Initialized using a function return value: nullability cannot be inferred, defaults to nullable
  struct Data data2 = init_data(&_Mut local);
  *data2.value = 10; // error: directly using a nullable pointer

  // Initialized by assignment from a variable: nullability cannot be inferred, defaults to nullable
  struct Data data3 = data1;
  *data3.value = 10; // error: directly using a nullable pointer

  // Reassigning the Nullable pointer member can change its Nullability:
  data2.value = bar(&_Mut local);
  *data2.value = 10;

  // A pointer null-check statement can also change its Nullability:
  if (data3.value != nullptr)
    *data2.value = 10;
}

int main() {
  test();
  return 0;
}
```

## Scope and Control Options for Non-Null Pointer Checking

Non-null pointer checking is a powerful feature that helps developers identify potentially dangerous behaviors at compile time. At the same time, it incurs some compilation performance overhead and imposes restrictions on coding behavior. **By default, non-null pointer checking only takes effect in safe zones**; for the definition of safe zones, see the [Safe Zone](../chapter-3-memory-safety/5-safe-zone.md) chapter.

In non-safe zones, we leave the choice of whether to enable non-null pointer checking to the developer, that is, the scope of this check is controlled through the compiler option `-nullability-check=value`. Here `value` is an enumerated value with two options, `safeonly` and `all`:

1. The default value of `value` is `safeonly`, which controls the check to take effect only in safe zones; when this compiler option is absent, it is equivalent to `-nullability-check=safeonly`.
2. When the value of `value` is `all`, non-null pointer checking is enabled overall, taking effect in both safe zones and non-safe zones.

Here is an example for illustration:

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

For the example above, when the compiler option `-nullability-check` is absent or is `-nullability-check=safeonly`, only `error2` in the `_Safe` zone is reported; when `-nullability-check=all`, both `error1` in the non-safe zone and `error2` in the safe zone are reported.
