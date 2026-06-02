# Owned Struct Types

`_Owned struct` is a user-defined type that, unlike `struct`, primarily differs in its non-copy semantics: it is always tracked as a whole under `move` semantics. This means that ownership of the resources in the original variable is transferred to the new variable or parameter. This section explains, in turn, how to define an `_Owned struct` type and how to create an `_Owned struct` instance.

## Defining owned struct types

The definition of an `_Owned struct` type consists of the keyword `_Owned struct` together with a user-defined name, followed by the `_Owned struct` definition body enclosed in a pair of braces. Within the definition body you can define member variables, a destructor, member functions, and access modifiers. Generic `_Owned struct` types are allowed.
Example:

```c
#include <stdio.h>

_Owned struct Person {
_Public:
  char name[50];
  int age;
  char *getName(Person *this) { return this->name; }
  int getAge(Person *this) { return this->age; }
  ~Person(Person this) {
    printf("Name = %s\n", this.getName());
    printf("Age = %d\n", this.getAge());
  }
};

int main() {
  Person p = {.name = "Tom", .age = 18};
  return 0;
}
```

The example above defines an `_Owned struct` type named `Person`, which has two member variables `name` and `age`, member functions `getName` and `getAge`, and a destructor `~Person`.

Note: `_Owned struct` is similar to `C struct` and allows struct nesting.

### Member variables

The member variables of an `_Owned struct` are divided into instance member variables and static member variables (modified by `static`). As with `struct`, member variables may not be given an initializer at the point of definition.

```c
#include <stdio.h>

_Owned struct Person{
_Public:
    char name[50];
    static int age;
};

int main() {
  Person::age = 18;
  printf("%d\n", Person::age);
  return 0;
}
```

### Destructors

When an object's lifetime ends, the destructor is called automatically. A destructor has the same name as the `_Owned struct` type, preceded by `~`, but it may not take generic parameters. When the user does not define a destructor, the compiler automatically provides a default one.

```c
#include <stdio.h>

_Owned struct S {
  ~S(S this) { printf("S destructor\n"); }
};

int main() {
  S s = {};
  return 0;
}
```

When an `_Owned struct` contains a pointer modified by owned, that pointer must be freed manually inside the destructor.

```c
#include "bishengc_safety.hbs" // A header file provided by the BiShengC language, used for safely allocating and freeing memory
#include <stdio.h>

_Owned struct Person {
_Public:
  int *_Owned age;
  ~Person(Person this) {
    printf("%d\n", *this.age);
    safe_free((void *_Owned)this.age);
  }
};

int main() {
  Person p = {.age = safe_malloc(18)};
  return 0;
}
```

The use of destructors is subject to the following restrictions:

+ An `_Owned struct` can have at most one destructor.
+ A destructor may not be an extension definition, may not declare a return type, takes only a single parameter `this` of the `_Owned struct` type, and may not be called explicitly by the user.
+ Inside the destructor, all of `this`'s `_Owned` members must be freed, following the same checking rules as `ownership`. Note that if a member of `this` is of an `_Owned struct` type, the release of that member is added automatically by the compiler.
+ Destructors of global variables are not called, including `static` variables defined inside functions.
+ For destructors of local variables, if they are not `move`d away within the current function, the destruction order is such that the variable defined earlier is destroyed later.
+ Temporary variables do not support destruction; doing so reports a memory leak.
+ When a member inside a user-defined type has a destructor, the order in which destructors are called is:
  + The destructor of the outer type is called first, then the destructors of the member variables.
  + The order in which the individual members' destructors are called is not guaranteed.
+ At the end of its scope, an `_Owned struct` and its inner member variables must be in one of the following two states:
  + `_Owned` state: neither the `_Owned struct` nor any of its `_Owned`-modified members (1. an `_Owned struct` is itself an `_Owned`-modified member; 2. recursively included) have been moved.
  + `moved` state: the `_Owned struct` as a whole has been explicitly moved.
+ At the end of its scope, if an `_Owned struct` and its inner member variables are in the following state, an error is reported. The error message template is partially moved `_Owned struct`:`%0` at scope end, `%1` moved" (`%0` is the `_Owned struct` variable name, and `%1` is the moved member variable name).
  1. A state in which some member variable has had its ownership transferred: the `_Owned struct` (recursively including nested members) has at least one `_Owned`-modified member that has been moved, while the struct itself has not been moved as a whole.
+ An `_Owned`-modified pointer inside an `_Owned struct` must be freed manually inside the destructor. If it is not freed manually, an error is reported. The error message template is destructor for `%0` incorrect, %1 of _Owned type and needs to be handled manually (`%0` is the `_Owned struct` variable name).

### Member functions

The member functions of an `_Owned struct` are divided into instance member functions and static member functions (the `static` modifier is not allowed). An instance member function requires an explicit `this` parameter; assuming the current `_Owned struct` type is `C`, the type of `this` can be `C*`, `const C*`, `C* _Borrow`, `const C * _Borrow`, or `C * _Owned`. A static member function has no `this`. Access to member functions is the same as access to `struct` extension member functions (see the Member Functions chapter for details).

Note: defining generic functions inside an `_Owned struct` is currently not supported.

```c
#include <stdio.h>

_Owned struct Person {
_Public:
  char name[50];
  int age;
  char *getName(Person *this) { return this->name; }
  char *getTypeName() { return "Preson"; }
};

int main() {
  Person p = {.name = "Tom", .age = 18};
  printf("TypeName: %s\nInstance Name: %s\n", Person::getTypeName(),
         p.getName());
  return 0;
}
```

The example above shows the instance member function `getName` and the static member function `getTypeName` inside the `_Owned struct` definition body.

The `_Owned struct` definition body may contain only function declarations, with the function bodies placed outside. This is equivalent to member functions defined inside the `_Owned struct` definition body.
Example:

```c
#include <stdio.h>

_Owned struct Person {
_Public:
  char name[50];
  int age;
  char *getName(Person *this);
  char *getTypeName();
};
char *Person::getName(Person *this) { return this->name; }
char *Person::getTypeName() { return "Preson"; }

int main() {
  Person p = {.name = "Tom", .age = 18};
  printf("TypeName: %s\nInstance Name: %s\n", Person::getTypeName(),
         p.getName());
  return 0;
}
```

Like `struct`, `_Owned struct` allows extension member functions (see the Member Functions chapter for details).

### Access control permissions

Within the `_Owned struct` definition body, members may be assigned a visibility of either `_Public` or `_Private`, with `_Private` being the default. Only member functions inside the `_Owned struct` definition body have the right to access both `_Private` and `_Public` members; outside the `_Owned struct` (including in extension member functions), only `_Public` members can be accessed. Example:

```c
_Owned struct A{
    _Private:
    int a;
    _Public:
    int b;
};
int A::f(A* this) {
    this->a; // error
    this->b; // ok
    return 0;
}
```

## Creating owned struct instances

An `_Owned struct` allows instances to be created using the `struct initializer` syntax, and also allows each member variable to be initialized individually (if the member variable is `_Public`, in which case, as with `struct`, the initialization state of each member is tracked individually; however, in the safe-zone state it must be guaranteed that the variable is already fully initialized before scenarios such as a `move`, argument passing, destruction, or return occur, while no such guarantee is made outside the safe zone). At the same time, for convenience, **an `_Owned struct` type does not carry the `_Owned struct` keyword when it is declared or defined**.
