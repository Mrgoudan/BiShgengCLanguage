# Initialization Analysis

## Overview

In C, using an uninitialized variable is a common form of undefined behavior. The compiler may not warn about it, but an uninitialized value can lead to unpredictable program behavior, security vulnerabilities, or crashes.

BiSheng C introduces **Initialization Analysis** in safe zones (`_Safe`), using data-flow analysis at compile time to ensure that every variable is definitely initialized before use. This analysis supports precise tracking at the level of individual struct fields.

Initialization analysis takes effect within `_Safe` zones by default. It can also be extended to all code regions via the `-uninit-check=all` option, or disabled via the `-uninit-check=none` option.

## Code Example

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
    int y = x; // ok: x is initialized on all paths
}
```

## Syntax and Semantic Rules

1. In a safe zone, all local variables (scalars, pointers, structs, etc.) must be definitely initialized before use. A variable that is uninitialized, or initialized on only some paths, causes a compile error.

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

2. For struct types, the analysis precisely tracks the initialization state of each field. When some fields are uninitialized, using the struct as a whole reports an error. Once all fields are initialized, the compiler automatically promotes the entire struct to the initialized state.

```c
struct Pair { int a; int b; };

_Safe void rule2_partial(void) {
    struct Pair p;
    p.a = 1;
    // p.b is not initialized
    struct Pair q = p; // error: use of uninitialized value: `p`
}

_Safe void rule2_full(void) {
    struct Pair p;
    p.a = 1;
    p.b = 2;
    struct Pair q = p; // ok: all fields of p are initialized, automatically promoted to fully initialized
}
```

3. Initialization within control-flow branches must cover all paths. If a variable is initialized in only some branches, the analysis reports "possibly uninitialized".

```c
_Safe void rule3(int cond) {
    int x;
    if (cond) {
        x = 1;
    }
    // the else branch does not initialize x
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

4. Address-of operations (`&_Mut`, `&_Const`, `&`) are treated as uses of the variable. Taking the address of an uninitialized variable reports an error. Exception: an address-of expression used as an argument to `ensure_init` or `__assume_initialized` is not subject to this restriction (see sections 3.6.4 and 3.6.5).

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

5. A function with a return value must initialize the return value on all paths.

```c
_Safe int rule5(int cond) {
    if (cond) {
        return 1;
    }
    // other paths do not return
} // error: return value of `rule5` may not be initialized on all paths
```

6. Assigning array elements one by one does **not** mark the array as initialized. An array must be initialized through an initializer list, or by using `__assume_initialized` in an `_Unsafe` zone (see section 3.6.5). This rule also applies to array fields within a struct — writing array elements one by one does not mark that field as initialized.

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
    int x = arr[0]; // ok: initialized through an initializer list
}

_Safe void rule6_ok_assume(void) {
    int arr[3];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    _Unsafe { __assume_initialized(&arr); }
    int x = arr[0]; // ok: marked via __assume_initialized
}

// An array field within a struct works the same way
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
    ArrStruct t = s; // ok: initialized through an initializer list
}
```

7. Writing to any member of a union marks the entire union as initialized (consistent with C union semantics: writing any variant overwrites all bytes). For a union that contains a struct variant, writing to one of the struct's fields also marks the entire union as initialized, and reading across variants is allowed.

Note: since directly accessing union fields is not allowed inside a safe zone, the following example uses a non-safe function together with `-uninit-check=all` to demonstrate the union's initialization behavior.

```c
// Compiler option: -uninit-check=all
union U { int a; float f; };

void rule7(void) {
    union U u;
    u.a = 42;
    float f = u.f; // ok: the union is already initialized via u.a
}

// A union containing a struct variant
struct S { int x; int y; };
union US { int a; struct S s; };

void rule7_struct(void) {
    union US u;
    u.s.x = 1;      // writing a field of the struct variant → the entire union is marked as initialized
    int v = u.a;     // ok: reading across variants, the union is already initialized
}
```

> **Known limitation**: writing one variant marks the entire union as initialized, so when reading a struct field across variants, the compiler will not report an error even though the corresponding bytes have not actually been written. For example:
>
> ```c
> struct S2 { int b; int c; };
> union U2 { int a; struct S2 s; };
>
> void example(void) {
>     union U2 u;
>     u.a = 1;
>     int v = u.s.c; // compiles, but the bytes of u.s.c may not actually have been meaningfully written
> }
> ```

8. Nested structs support field-level tracking at arbitrary depth. Once all leaf fields are initialized, the parent field and the entire struct are automatically promoted to the initialized state.

```c
struct Inner { int x; int y; };
struct Outer { struct Inner inner; int z; };

_Safe void rule8(void) {
    struct Outer o;
    o.inner.x = 1;
    o.inner.y = 2;  // all fields of inner are initialized → inner is automatically promoted
    o.z = 3;        // all fields are initialized → o is automatically promoted
    struct Outer p = o; // ok
}
```

9. Global variables and static variables are treated as implicitly initialized (zero-initialization is guaranteed by the C language specification) and do not need to be explicitly initialized.

```c
static int global_count;

_Safe void rule9(void) {
    int x = global_count; // ok: global/static variables are implicitly initialized
}
```

## `__attribute__((ensure_init))`

`__attribute__((ensure_init))` is a parameter attribute used to annotate a pointer parameter, establishing an initialization contract:

- **Caller side**: after the call, the pointed-to variable is marked as initialized.
- **Callee side**: the compiler verifies that the function does indeed initialize `*param` on all return paths.

```c
void init_int(int *__attribute__((ensure_init)) out);

_Safe void caller(void) {
    int x;                    // uninitialized
    _Unsafe { init_int(&x); } // after the call, x is marked as initialized
    int y = x;                // ok
}

// The compiler verifies the callee's contract
void good_init(int *__attribute__((ensure_init)) out) {
    *out = 42; // ok: *out is initialized before return
}

void bad_init(int *__attribute__((ensure_init)) out) {
} // error: __attribute__((ensure_init)) parameter 'out' not initialized at return
```

Field-level partial initialization is also supported:

```c
struct Pair { int a; int b; };
void init_field(int *__attribute__((ensure_init)) p);

_Safe void field_level(void) {
    struct Pair p;
    _Unsafe {
        init_field(&p.a);  // marks only p.a as initialized
        init_field(&p.b);  // marks p.b as initialized → p as a whole is automatically promoted to initialized
    }
    struct Pair q = p; // ok
}
```

```c
_Safe void init_safe(int *_Borrow __attribute__((ensure_init)) out);

_Safe void caller_safe(void) {
    int x;
    init_safe(&_Mut x);  // ok: use &_Mut in a safe zone
    int y = x;            // ok
}
```

An `ensure_init` parameter may not be reassigned or copied to another variable (aliased) before `*param` is fully initialized:

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

After initialization is complete, it can be freely reassigned or aliased:

```c
void ok_reassign(int *__attribute__((ensure_init)) out) {
    *out = 42;         // the contract is fulfilled
    int local = 10;
    out = &local;      // ok: *out is initialized, the pointer can be used freely
}

void ok_alias(int *__attribute__((ensure_init)) out) {
    *out = 42;         // the contract is fulfilled
    int *p = out;      // ok: *out is initialized
}
```

An `ensure_init` parameter can be delegated to another `ensure_init` function, and the compiler tracks the delegation chain:

```c
void init_val(int *__attribute__((ensure_init)) out);

void init_delegated(int *__attribute__((ensure_init)) out) {
    init_val(out); // ok: delegated to another ensure_init function
}
```

**Redeclaration rules**: redeclarations at the same safety level (`_Safe`/`_Safe`, `_Unsafe`/`_Unsafe`, or default/`_Unsafe`) must keep `ensure_init` consistent — either both have it, or neither does. Redeclarations at different safety levels (`_Safe` versus non-safe) are independent overloads, and differences in `ensure_init` are allowed.

```c
// Same safety level: must be consistent
_Safe void foo(int *__attribute__((ensure_init)) _Borrow out);
_Safe void foo(int *__attribute__((ensure_init)) _Borrow out) { // ok: consistent
    *out = 1;
}

// Same safety level: inconsistent → error
// _Safe void bar(int *__attribute__((ensure_init)) _Borrow out);
// _Safe void bar(int *_Borrow out) { ... } // error: incompatible declarations

// Different safety levels: differences allowed
void init_value(int *__attribute__((ensure_init)) out);
_Safe void init_value(int *_Borrow out) { // ok: different safety levels
    // ...
}
```

**Function pointer compatibility**: `ensure_init` is part of the function type. Assigning a function that does not have `ensure_init` to a function pointer that requires `ensure_init` is not allowed:

```c
typedef _Safe void (*InitFn)(int *__attribute__((ensure_init)) _Borrow out);

_Safe void has_attr(int *__attribute__((ensure_init)) _Borrow out) { *out = 1; }
_Safe void no_attr(int *_Borrow out) { *out = 1; }

_Safe void test(void) {
    InitFn fn = has_attr; // ok: the signature matches
    // InitFn fn2 = no_attr; // error: the target requires ensure_init but the source does not have it
}
```

Indirect calls through a function pointer also support `ensure_init` effect verification:

```c
_Safe void indirect_call(InitFn fn) {
    int x;
    fn(&_Mut x); // ok: ensure_init is recognized through the function pointer type
    int y = x;   // ok: x has been marked as initialized via ensure_init
}
```

## `__assume_initialized`

`__assume_initialized(&x)` is a builtin function used to mark a variable as initialized at a particular program point. Unlike `ensure_init`, it does **not** perform contract verification — the user guarantees that the variable is indeed initialized.

Each call can mark only **one** variable. To mark multiple variables, call it separately for each.

When using the `__assume_initialized` function, the argument should use `&`.

`__assume_initialized` can only be used in an `_Unsafe` zone, because it bypasses the compiler's initialization verification.

This builtin function is path-sensitive: it only takes effect on the CFG path on which the call executes.

```c
_Safe void example_builtin(void) {
    int x;
    _Unsafe { __assume_initialized(&x); } // from this point on, x is regarded as initialized
    int y = x; // ok
}

// Path-sensitive: only takes effect on the path that is executed
_Safe void path_sensitive(int cond) {
    int x;
    if (cond) {
        _Unsafe { __assume_initialized(&x); }
    }
    int y = x; // error: use of possibly uninitialized value: `x`
}

// Used on a struct: all fields are marked as initialized
_Safe void struct_example(void) {
    struct Pair s;
    _Unsafe { __assume_initialized(&s); }
    int a = s.a; // ok
    int b = s.b; // ok
}
```

Note: because of implicit array decay, array types require `&` rather than passing the array name directly:

```c
_Safe void array_example(void) {
    int arr[3];
    _Unsafe { __assume_initialized(&arr); } // ok: using &
    // __assume_initialized(arr);           // error: a & expression is required
}
```

Supported argument forms (the lvalue of the address-of expression must be a combination of one of the following):

| Form | Description |
| ------ | ------ |
| `&x` | a local variable; if `x` is an `ensure_init` pointer parameter, this also marks `*x` as initialized |
| `&x.f.g...` | a struct field of a local variable (arbitrary nesting depth, pure field access) |
| `&*p` | the entire object pointed to by an `ensure_init` pointer parameter |
| `&p->f.g...` | a field of the struct pointed to by an `ensure_init` pointer (arbitrary nesting depth, pure field access) |

For the `&p->f...` form, once all fields of the pointed-to struct are marked as initialized, `*p` is automatically promoted to initialized (satisfying the `ensure_init` contract).

```c
struct Pair { int a; int b; };

void assume_through_ptr(struct Pair *__attribute__((ensure_init)) out) {
    out->a = 1;
    _Unsafe { __assume_initialized(&out->b); } // all fields are covered → *out is initialized
}
```

**Restriction: the argument path cannot contain an array subscript (`[i]`).** Initialization analysis treats an array as a single unit (it does not track the state of individual elements), so you must assert on the entire array rather than on a particular element.

```c
struct WithArr { int arr[3]; };
struct Outer { struct WithArr s[2]; };

_Safe void array_subscript_example(void) {
    struct WithArr w;
    _Unsafe { __assume_initialized(&w.arr); }    // ok: asserting on the entire array field
    // _Unsafe { __assume_initialized(&w.arr[0]); } // error: the argument cannot contain an array subscript

    struct Outer o;
    _Unsafe { __assume_initialized(&o); }        // ok: asserting on the entire struct
    // _Unsafe { __assume_initialized(&o.s[0].arr); }    // error: a subscript appears in the middle of the path
    // _Unsafe { __assume_initialized(&o.s[0].arr[0]); } // error: a subscript appears at the end
}
```

## Check Modes

The scope of initialization analysis is controlled through the compiler option `-uninit-check=<mode>`:

| Mode | Behavior |
| ------ | ------ |
| `none` | disables initialization analysis |
| `safeonly` (default) | checks only within `_Safe` zones, or when the function has an `ensure_init` parameter |
| `all` | checks within all code regions (including non-safe zones) |

Note: as long as initialization analysis is enabled (i.e. the mode is not `none`), `ensure_init` contract verification takes effect (including in non-safe zones).
