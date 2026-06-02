# 借用

借用作为毕昇C内存安全特性的重要组成部分，是对所有权的一个补充。前面小节描述了所有权特性(ownership)，对某个资源拥有所有权的主体，有责任释放这个资源。这个小节，将介绍对资源的借用(borrow)。

## 特性简介

如果我们只有 ownership 类型，由于函数调用、赋值等操作都会转移所有权，那么代码能力会非常受限。在编程时，我们常常需要表达“对某个资源进行借用”的概念，区别于“拥有某个资源”。正如现实生活中，如果一个人拥有某样东西，你可以从他那里借来，当使用完毕后，也必须要物归原主。

### 借用的定义及借用操作符

毕昇 C 的**借用是一个指针类型，指向了被借用对象存储的内存地址**。为表达借用的概念：

1. 引入新关键字`_Borrow`，用`_Borrow`来修饰指针类型 `T*`，表示 T 的借用类型。`_ArrayElem`也可以与`_Borrow`一并修饰指针类型 `T*`, 表示对 T 数组成员的借用类型。
2. 引入借用操作符 `&_Mut` 和 `&_Const`，其中，`&_Mut e`表示获取对表达式 e 的**可变借用**，`&_Const e`表示获取对表达式`e`的**只读借用**。此处要求表达式`e`是 lvalue（左值）与标准 C 中的取地址操作符`&`类似, 借用操作符实际上获取的是表达式`e`的地址

例如，我们可以创建指向局部变量`local`的可变借用`p1`和不可变借用`p2`，并使用它们：

```C
void use_immut(const int *_Borrow p) {}
void use_mut(int *_Borrow p) {}

void foo() {
  int local = 5;
  // p1是local的可变借用指针
  int *_Borrow p1 = &_Mut local;
  use_mut(p1);
  // p2是local的不可变借用指针
  const int *_Borrow p2 = &_Const local;
  use_immut(p2);
}

int main() {
  foo();
  return 0;
}
```

另外，表达式`e`如果是指针的解引用表达式，`&_Mut *p`和`&_Const *p`分别可以看作对地址`p`中存放的值，也就是`*p`，取可变借用和不可变借用，**这一操作不为`*p`产生临时变量**。其中，`p`可以是裸指针、`_Owned`指针和其它借用指针。例如：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

void foo() {
  int *x1 = malloc(sizeof(int));
  *x1 = 1;
  // p1借用了*x1
  int *_Borrow p1 = &_Mut * x1;
  int *_Owned x2 = safe_malloc(2);
  // p2借用了*x2
  int *_Borrow p2 = &_Mut * x2;
  int local = 3;
  int *_Borrow x3 = &_Mut local;
  // p3借用了*x3
  int *_Borrow p3 = &_Mut * x3;
  safe_free((void *_Owned)x2);
}

int main() {
  foo();
  return 0;
}
```

如果表达式 `e` 是数组下标表达式，那么 `&_Mut e` 和 `&_Const e` 的结果类型会带有 `_Borrow _ArrayElem` 这两个限定符以表示“指向数组元素”的借用指针，这样的借用指针除了允许使用下标`[]`、算术运算，有额外的类型转换规则之外，其余规则与普通的 `_Borrow` 指针相同。除非明确说明或单独列出 `_Borrow _ArrayElem` 的情况，否则对 `_Borrow` 指针的规则同样适用于 `_Borrow _ArrayElem`。

```C
void f1(int *_Borrow _ArrayElem p) {}
void f2(const int *_Borrow _ArrayElem p) {}

void foo() {
  int arr[4] = {1, 2, 3, 4};
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  const int *_Borrow _ArrayElem q = &_Const arr[1];
  f1(&_Mut arr[2]);
  f2(&_Const arr[3]);
}
```

### 借用的作用

假设我们有这样的一个需求：创建一个文件，并且调用一些操作函数对文件进行读写操作。如果没有借用的概念，调用文件操作函数会导致文件指针所有权的转移，为了使文件指针在函数调用之后仍然可以被使用，我们需要再将所有权返回给调用方：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *_Owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *_Owned p) { safe_free((void *_Owned)p); }

MyFile *_Owned insert_str(MyFile *_Owned p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // 通过返回值，将所有权转移给调用方，避免所有权转移
  return p;
}

MyFile *_Owned other_operation(MyFile *_Owned p) {
  // some operation
  // 通过返回值，将所有权转移给调用方，避免所有权转移
  return p;
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // p的所有权先被移动到 insert_str 中，再通过返回值转移到调用方
  p = insert_str(p, str);
  p = other_operation(p);
  file_safe_free(p);
  return 0;
}
```

这种写法会造成文件指针所有权的频繁转移，在代码逻辑较为复杂的时候很容易出错，而且如果所有权被转移走了但是没有归还，后续将无法再使用该文件指针。有了借用，将对文件指针的借用作为参数传递给操作函数，函数返回之后文件指针仍可以用于后续其他操作，不再需要像上面那个例子一样，先通过函数参数传入所有权，然后再通过函数返回来传出所有权，代码更加简洁：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

typedef struct {
  int file_id;
} MyFile;

MyFile *_Owned create_file(int id) {
  MyFile f = {.file_id = id};
  return safe_malloc(f);
}
void file_safe_free(MyFile *_Owned p) { safe_free((void *_Owned)p); }

void insert_str(MyFile *_Borrow p, char *str) {
  // some operation to insert a string to file
  printf("%s to file %d\n", str, p->file_id);
  // 无需返回所有权
}

void other_operation(MyFile *_Borrow p) {
  // some operation
  // 无需返回所有权
}

int main() {
  MyFile *_Owned p = create_file(0);
  char str[] = "insert str";
  // 所有权不会被移动
  insert_str(&_Mut * p, str);
  // 所有权不会被移动
  other_operation(&_Mut * p);
  file_safe_free(p);
  return 0;
}
```

## 借用变量和被借用对象的生命周期

### 生命周期及其作用

我们可以对不同种类的对象取借用：`_Owned`变量、非`_Owned`类型的局部变量、全局变量、临时匿名变量、参数等，甚至是某个复合变量的一部分。为正确表示借用变量和不同种类被借用对象的有效作用域，我们引入生命周期的概念。

生命周期检查的主要作用是避免悬垂指针，它会导致程序使用本不该使用的数据，以下 C 代码就是一个使用了悬垂指针的典型例子：

```C
int main() {
  int *p;
  {
    int local = 5;
    p = &local;
  }
  *p = 1;
  return 0;
}
```

这段 C 代码有两点值得注意：

1. `int *p`的声明方式存在使用`NULL`的风险；
2. `p`指向了内部block中的`local` 变量，但是`local`会在block结束的时候被释放，因此回到外部block后，`p`会指向一个无效的地址，是一个悬垂指针，它指向了提前被释放的变量`local`，可以预料到，`*p = 1` 会导致该段程序运行时出现未定义行为（undefined behavior）。当代码逻辑较为复杂时，这类异常行为很难被发现。

对于第二点，毕昇 C 规定：**任何一个对资源的借用，都不能比资源的所有者的生命周期长**。也就是说：借用变量的生命周期，不能比被借用对象的生命周期长。

接下来我们使用毕昇 C 的借用特性改写上面的 C 代码，通过检查借用变量和被借用对象的生命周期，在编译期就可以识别出潜在的内存安全风险：

```C
int main() {
  int local1 = 1;
  // 借用指针变量 p 必须使用前被初始化，否则会报错
  int *_Borrow p = &_Mut local1;
  {
    int local2 = 2;
    // 对 p 进行再赋值之后，p 不再借用 local1，而是借用 local12
    p = &_Mut local2;
  }
  *p = 3; // error: local2 的生命周期不够长
  return 0;
}
```

### 借用变量和被借用对象

每个借用变量（也就是 _Borrow 指针变量）都会有一个或多个被借用对象，例如：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct S {
  int a;
};

int *_Borrow bar(int *_Borrow, int *_Borrow);

int g = 5;
void foo(int a, int *_Owned b, int *c, struct S d) {
  // 被借用对象是普通局部变量
  int local = 5;
  int *_Borrow p1 = &_Mut local; // p1的被借用对象是local
  int *_Borrow p2 = &_Mut * p1;  // p2的被借用对象是*p1
  int *_Borrow p3 = p1;         // p3的被借用对象是*p1

  // 被借用对象是owned变量
  int *_Owned x1 = safe_malloc<int>(2);
  int *_Borrow p4 = &_Mut * x1; // p4的被借用对象是*x1

  // 被借用对象是裸指针变量
  int *x2 = malloc(sizeof(int));
  int *_Borrow p5 = &_Mut * x2; // p5的被借用对象是*x2

  // 被借用对象是结构体的某个字段
  struct S s = {.a = 5};
  int *_Borrow p6 = &_Mut s.a; // p6的被借用对象是s.a

  // 被借用对象是函数的返回值，与被调用函数的借用类型入参的 “被借用对象” 一样
  int local1 = 10, local2 = 20;
  // 被调用函数bar有两个借用类型入参，因此p7的被借用对象是local1和local2
  int *_Borrow p7 = bar(&_Mut local1, &_Mut local2);

  // 被借用对象是全局变量
  const int *_Borrow p8 = &_Const g; // p8的被借用对象是g

  // 被借用对象是函数入参
  int *_Borrow p9 = &_Mut a;    // p9的被借用对象是a
  int *_Borrow p10 = &_Mut * b; // p10的被借用对象是*b
  int *_Borrow p11 = &_Mut * c; // p11的被借用对象是*c
  int *_Borrow p12 = &_Mut d.a; // p12的被借用对象是d.a

  safe_free((void *_Owned)b);
  safe_free((void *_Owned)x1);
}

int main() {
  int a = 42;
  int *_Owned b = safe_malloc(73);
  int *c = &a;
  struct S d = {.a = 31};
  foo(a, b, c, d);
  return 0;
}
```

注：如果被借用对象是来自于取地址或强转到裸指针再获取借用的结果，则不会记录其为被借用对象。如：

```C
void f1() {
  int a = 1;
  int *p = &a;
  int *_Borrow p1 = (int *_Borrow)(int *)(&_Mut *p); // 不记录被借用对象是*p
  int *_Borrow p2 = &_Mut *&a; // 不记录被借用对象是a
}
```

### 借用变量的 Non-Lexical Lifetime

一个变量的生命周期从它的声明开始，到当前整个语句块结束，这个设计被称为Lexical Lifetime，因为变量的生命周期是严格和词法中的作用域范围绑定的。这个策略实现起来非常简单，但它可能过于保守了，某些情况下借用变量的作用范围被过度拉长了，以至于某些实质上是安全的代码也被阻止了，这在一定程度上限制了程序员能编写出的代码。因此，毕昇 C 为借用变量引入 Non-Lexical Lifetime（简写为NLL），用更精细的手段计算借用变量真正起作用的范围，**借用变量的 NLL 范围为：从借用处开始，一直持续到最后一次使用的地方**。具体的，它是**从借用变量定义或被再赋值开始，到被再赋值之前最后一次被使用结束**。

其中，以下场景属于对借用变量p的使用：

1. 函数调用，如`use(p)`或`use(&_Mut *p)`
2. 函数返回`return p`或`return &_Mut *p`
3. 解引用`*p`
4. 成员访问`p->field`

举例来说：

```C
void use(int *_Borrow p) {}
void other_op() {}

// 本例中p的NLL是分段的，每段NLL都有一个被借用对象
void foo() {
  int local1 = 1, local2 = 2;  //#1
  int *_Borrow p = &_Mut local1; //#2，p的第一段NLL开始，被借用对象为local1
  other_op();                  //#3
  use(p);                      //#4，p的第一段NLL结束
  other_op();                  //#5
  p = &_Mut local2;             //#6，p的第二段NLL开始，被借用对象为local2，由于后面没有再对p的使用，p的NLL结束
  other_op();     //#7
}
// p的NLL是：[2,4]->local1, [6,6]->local2

int main() {
  foo();
  return 0;
}
```

### 被借用对象的 Lexical Lifetime

与借用变量不同，被借用对象的生命周期是 Lexical Lifetime，对于不同种类被借用对象的生命周期，我们给出具体的定义：

| 被借用对象种类 | | 生命周期定义 |
| ---- | ---- | ---- |
| 全局变量 | | 全局变量的生命周期是整个程序，从程序开始到退出，一直存在 |
| 局部变量 | owned变量 | 从变量定义开始，到它被 move 走结束（_Owned struct类型如果没有被move，生命周期会在当前block结束的时候结束） |
| | 非owned非borrow变量 | 从变量定义开始，到当前 block 结束 |
| 局部字面量 | `"string literal"` | 从使用处开始，到当前 block 结束的时候结束 |
| | `(struct S) { ... }` | 从使用处开始，到当前 block 结束的时候结束 |
| `e->field` | | `*e` 的生命周期 |
| `e.field` | | `e` 的生命周期 |
| `e[index]` 或 `*e` (`e`是数组) | | `e` 的生命周期 |
| `e[index]` 或 `*e` (`e`是指针) | | `*e` 的生命周期 |

### 借用的生命周期约束

在2.1中我们提到过，对于借用，我们有这样的生命周期约束：**借用变量的生命周期，不能比被借用对象的生命周期长**。
举例来说：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

void use(int *_Borrow p) {}
int *_Borrow call(int *_Borrow p, int *_Borrow q) { return p; }

// 本例中，p的生命周期为[2,4]，被借用对象local的生命周期为[1,4]，满足生命周期约束
void test1() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  use(p);                     //#3
} //#4

// 本例中，p的生命周期有两段“
// ok: 第一段为[2,2]，被借用对象local1的生命周期为[1,8]，满足生命周期约束
// error: 二段为[5,7]，被借用对象local2的生命周期为[4,6]，不满足生命周期约束
void test2() {
  int local1 = 5;              //#1
  int *_Borrow p = &_Mut local1; //#2
  {                            //#3
    int local2 = 5;            //#4
    p = &_Mut local2;           //#5
  }                            //#6
  use(p);                      //#7
} //#8

// 本例中p的生命周期有两段：
// ok: 第一段为[2,2]，被借用对象local1的生命周期为[1, 8]，满足生命周期约束
// error: 第二段为[5,7]，被借用对象有两个，分别是local1和local2，其中local2的生命周期为[4, 6]，不满足生命周期约束
void test3() {
  int local1 = 5;                       //#1
  int *_Borrow p = &_Mut local1;          //#2
  {                                     //#3
    int local2 = 5;                     //#4
    p = call(&_Mut local1, &_Mut local2); //#5
  }                                     //#6
  use(p);                               //#7
} //#8

// 本例中，if分支对p进行了重新赋值，在#10
// error: use(p)时，p的被借用对象local2的生命周期已经结束，不满足生命周期约束
// ok" else分支满足生命周期约束
void test4() {
  int local = 5;              //#1
  int *_Borrow p = &_Mut local; //#2
  int local1 = 5;             //#3
  if (rand()) {               //#4
    int local2 = 5;           //#5
    p = &_Mut local2;          //#6
  } else {                    //#7
    p = &_Mut local1;          //#8
  }                           //#9
  use(p);                     //#10
}

//本例中，p的生命周期为[2,4]，被借用对象*x的生命周期为[1,3]，不满足生命周期约束，error
void test5() {
  int *_Owned x = safe_malloc<int>(5); //#1
  int *_Borrow p = &_Mut * x;           //#2
  safe_free((void *_Owned)x);          //#3
  use(p);                             //#4
} //#5

int main() {
  test1();
  test2();
  test3();
  test4();
  test5();
  return 0;
}
```

## 可变借用和不可变借用

毕昇 C 将借用指针的权限进行了分级，分为可变（mut）借用和不可变（immut）借用，我们可以通过操纵可变借用指针来读写被借用对象的内容，通过不可变借用指针，我们只能读取被借用对象的内容，但是不能修改它。例如：

```C
// 可变借用指针类型为 T *_Borrow
void use_mut(int *_Borrow p) {
  // 通过可变借用指针，可以修改被借用对象的值
  *p = 5;
  // 通过可变借用指针，可以读取被借用对象的值
  int a = *p;
}

// 不可变借用指针类型为 const T *_Borrow
void use_immut(const int *_Borrow p) {
  *p = 5; // error: 无法通过不可变借用指针，来修改被借用对象的值
  // 通过不可变借用指针，可以读取被借用对象的值
  int a = *p;
}

int main() {
  int i = 1;
  int *_Borrow pmi = &_Mut i;
  use_mut(pmi);
  const int *_Borrow pimi = &_Const i;
  use_immut(pimi);
  return 0;
}
```

### `&_Mut e`要求 e 是可修改的

我们在 1.1 中提到过，`&_Mut e`和`&_Const e`要求表达式 e 是 lvalue，即 e 是可以被取地址的，对于可变借用表达式`&_Mut e`，我们还要求 e 是可变的，具体的：

| lvalue表达式 | 是否可修改 |
| ---- | ---- |
| ident | 变量 ident 没有被 const 修饰，且ident 不能是函数名 |
| "string literal" | 不允许，因为字符串常量保存在常量区，不能写。尝试对字符串字面量进行可变借用（如`&_Mut "hello"`或`&_Mut * "hello"`）会导致编译错误 |
| (struct S) { ... } | 允许 |
| `e->field` | 要求 `e` 是可变借用指针，或者是指向可修改类型的 _Owned 指针，或者是指向可修改类型的裸指针，且field没有被const修饰，多级 field 的情况应该要求每一级的 field 都没有 const 修饰 |
| `e.field` | 要求 `e` 是可变的，且field没有被const修饰，多级 field 的情况应该要求每一级的 field 都没有 const 修饰 |
| `e[index]` 或 `*e` (`e`是数组) | 要求 `e` 是可变的 |
| `e[index]` 或 `*e` (`e`是指针) | 要求 `e` 是可变借用指针，或者是指向可修改类型的 _Owned 指针，或者是指向可修改类型的裸指针 |

### 可变借用同时只能存在一个

如果有两个或更多的指针同时访问同一数据，并且至少有一个指针被用来写入数据，可能会导致未定义行为，例如：

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放
#include <stdio.h>

void free_a(int *a) { free(a); }
void read_a(int *a) { printf("%d\n", *a); }

void test() {
  int *a = malloc(sizeof(int));
  *a = 42;
  int *p1 = a;
  int *p2 = a;
  // 该函数会释放 a 所指向的内存
  free_a(p1);
  // 该函数会读取 a 所指向的内存
  read_a(p2); // 打印一个脏值
}

int main() {
  test();
  return 0;
}
```

由于借用本质上也是指针，所以为了避免上述问题，毕昇 C 规定，**同一时刻，对于同一个对象，要么只能拥有一个可变借用, 要么任意多个不可变借用**。

```C
void write(int *_Borrow p) {}
void read(const int *_Borrow p) {}

void test1() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  int *_Borrow p2 = &_Mut local; // error: 同一时刻最多只能有一个指向local的可变借用变量
  write(p1);
  write(p2);
}

void test2() {
  int local = 1;
  int *_Borrow p1 = &_Mut local;
  const int *_Borrow p2 = &_Const local; // error: 指向local的可变和不可变借用不能同时存在
  write(p1);
  read(p2);
}

void test3() {
  int local = 1;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local; // error: 指向local的可变和不可变借用不能同时存在
  read(p1);
  write(p2);
}

int main() {
  test1();
  test2();
  test3();
  return 0;
}
```

如果同时存在对一个变量的可变借用和不可变借用，可能会出现通过可变借用修改被借用对象的内存状态，然后再使用不可借用访问被修改的内存，从而导致未定义行为的情况。
例如：

```C
#include <stdio.h>

struct A {
  int *p;
};

const int *_Borrow struct A::get_p(This *_Borrow this) {
  return &_Const * (this->p);
}

void struct A::free_p(This *_Borrow this) { free(this->p); }

int main() {
  struct A a = {.p = malloc(sizeof(int))};
  // q借用了a.p
  const int *_Borrow q = a.get_p();
  // a.p指向的内存被释放
  a.free_p(); // error: a 被可变借出了多次
  // *q的操作可能会导致未定义行为
  printf("%d\n", *q);
  return 0;
}
```

上述代码中，`a.free_p()`实际上使用了一个指向 a 的可变借用，该可变借用会使在它之前被定义的借用 q 失效，由于`printf("%d\n", *q)`使用了失效的 q，毕昇 C 编译器会报错，也就阻止了不安全行为的发生。

由于不可变借用不会导致被借用对象被修改，因此同一时刻可以拥有任意多个不可变借用，例如：

```C
void read(const int *_Borrow p) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  const int *_Borrow p2 = &_Const local;
  read(p1);
  read(p2);
}

int main() {
  test();
  return 0;
}
```

## 借用对被借用对象的影响

### 不可变借用对被借用对象的影响

对表达式 e 做不可变借用， 即`&_Const e`，在这个不可变借用的生命周期结束之前，e 只能读不能修改，也不能对 e 创建可变借用，但是仍然可以对 e 取不可变借用。

| 不可变借用表达式 | 被借用对象的状态 |
| ---- | ---- |
| `&_Const ident` | 变量 `ident` 只能被读不能被修改，也不能再对变量 `ident` 创建可变借用，允许对变量 `ident` 创建不可变借用 |
| `&_Const "string literal"` | 临时变量永远是 “只读” 状态 |
| `&_Const (struct S) { ... }` | 临时变量永远是 “只读” 状态 |
| `&_Const e->field` | `e->field` 进入 “只读” 状态，也不允许整体修改 `*e`。但允许修改 `e` 指向的其它成员，或者对其它成员做可变借用 |
| `&_Const e.field` | `e.field` 进入 “只读” 状态，也不允许整体修改 `e`。但允许修改 `e` 的其它成员，或者对其它成员做可变借用 |
| `&_Const e[index]` 或 `&_Const *e` (`e`是数组) | `e` 进入 “只读” 状态，不允许修改 `e` 及其直接或间接成员，或者对其它成员做可变借用 |
| `&_Const e[index]` 或 `&_Const *e` (`e`是指针) | `*e` 进入 “只读” 状态，不允许直接修改 `*e` 及其直接或间接成员，或者对 `*e` 及其直接或间接成员做可变借用，如果 `e` 是 _Owned 指针类型，则 `e` 也进入只读状态。如果 `e` 是 _Borrow 指针类型（即这是对 `e` 的不可变 reborrow），则允许修改 `e` 的指向，且修改指向后 `e` 的可读写属性恢复到 reborrow 之前 |

### 可变借用对被借用对象的影响

对表达式 e 做可变借用， 即`&_Mut e`，表达式 e 进入 "冻结" 状态。在这个可变借用的生命周期结束之前，e 不能读，不能修改（包含被move），也不能被借用。

| 可变借用表达式 | 被借用对象的状态 |
| ---- | ---- |
| `&_Mut ident` | 变量 `ident` 被冻结 |
| `&_Mut "string literal"` | 编译错误（字符串字面量是不可变的，不能进行可变借用） |
| `&_Mut (struct S) { ... }` | 临时变量被冻结 |
| `&_Mut e->field` | `e->field` 被冻结，不允许读写 `e->field`，不允许整体修改 `*e`，但允许修改 `e` 指向的其它成员，或者对其它成员做可变借用 |
| `&_Mut e.field` | `e.field` 被冻结，不允许读写 `e.field`，不允许整体修改 `e`，但允许修改 `e` 的其它成员，或者对其它成员做可变借用 |
| `&_Mut e[index]` 或 `&_Mut *e` (`e`是数组) | `e` 被冻结，不允许读写 `e` 以及它的成员 |
| `&_Mut e[index]` 或 `&_Mut *e` (`e`是指针) | `*e` 被冻结，不允许对 `*e` 以及它的成员进行读写、取借用。如果 `e` 是 _Owned 指针类型，则也不允许读写 `e`；如果 `e` 是 _Borrow 指针类型（即这是对 `e` 的可变 reborrow），则允许修改 `e` 的指向，且修改指向后可以对 `*e` 及其直接或间接成员进行读写、取借用 |

## 函数定义中包含借用类型

1. 不允许函数参数中没有借用类型的参数，但是函数返回是借用类型。

2. 如果函数参数中有一个借用类型的参数，函数返回是借用类型，那么我们直接认为这个返回值的借用，是来自于这个借用类型参数，返回的借用的 “被借用对象” 与这个借用类型参数的 “被借用对象” 一样，这个返回的借用也应该满足前面提到的那些借用规则。

3. 如果函数参数中有多个借用类型的参数，函数返回是借用类型，那么我们直接认为这个返回值的借用，同时包含了从多个借用类型参数传递过来的 “被借用变量”，这个返回的借用也应该满足前面提到的那些借用规则。

```C
int *_Borrow f1(int *_Borrow p) { return p; }
int *_Borrow f2(int *_Borrow p1, int *_Borrow p2) { return p1; }

void test() {
  int local = 5;
  int *_Borrow p1 = f1(&_Mut local);
  /* 函数 f1 的参数创建了一个对 local 的可变借用，这个借用被传递给了返回值 p1，
     导致 p1 相当于是对 local 的一个可变借用, 所以返回值 p1 的被借用对象是
     local, 在 p1 的生命周期结束之前，local 会一直被冻结。*/

  int local1, local2;
  int *_Borrow p2 = f2(&_Mut local1, &_Mut local2);
  /* 函数 f2 的参数创建了一个对 local1 和 local2
     的可变借用，这两个借用被传递给了返回值 p2， 导致 p2 相当于是对 local1 和
     local2 的一个可变借用, 所以返回值 p2 的被借用对象是 local1 和 local2, 在 p2
     的生命周期结束之前，local1 和 local2 一直被冻结。*/
}

int main() {
  test();
  return 0;
}
```

## struct定义中包含借用类型

1. 结构体内如果包含多个借用成员，那么这个结构体同时存在多个 “被借用对象”，这些借用成员也应该满足前面提到的那些借用规则。

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct R {
  int *_Borrow m1;
  int *_Borrow m2;
};

void test() {
  int local1, local2;
  struct R r = {.m1 = &_Mut local1, .m2 = &_Mut local2};
  // 在 r 的生命周期结束之前，local1 和 local2 一直被冻结。
  // 因为变量 r 在初始化的时候创建了一个对 local1 和 local2 的可变借用，
  // 导致 r 同时包含对 local1 的一个可变借用，也包含对 local2 的可变借用。
}

int main() {
  test();
  return 0;
}
```

## 借用变量的解引用操作

允许对借用指针变量解引用，与标准 C 的解引用操作一致：对借用指针变量`p`解引用的语法为 `*p`。
对`const T * _Borrow`类型的借用变量`e`解引用 `*e`，结果为`T`类型
对`T * _Borrow`类型的借用变量`e`解引用 `*e`，结果为`T`类型
如果`p`是指向`T`类型的借用，`o`是`T`类型的 lvalue，对于`*p`表达式，有如下限制：

| | T是Copy语义 | T是Move语义 |
| ---- | ---- | ---- |
| p是immut借用 | *p = expr; 不允许 | *p = expr; 不允许 |
| | o = *p; 允许 | o = *p; 不允许 |
| p是mut借用 | *p = expr; 允许 | *p = expr; 允许 |
| | o = *p; 允许 | o = *p; 不允许 |

上表中 move / copy 语义分别指： `T`是`_Owned`修饰的类型和`T`是其它类型。

注：上表中的赋值操作的权限，可同样应用于函数的传参和返回场景。

## 借用变量的成员访问

允许借用指针变量访问成员变量或调用成员函数，与标准 C 的箭头运算符一致：访问指针变量`p`的成员变量`field`的语法为`p->field`，调用指针变量`p`的成员方法`method()`的语法为`p->method()`。

### 访问成员变量

通过借用访问成员变量，表达式的类型取决于成员变量本身的类型。`p->field` 表达式的类型与`field`成员定义的类型相同。
如果 `p->field` 的类型是`T`，`o`是`T`类型的 lvalue，对于 `p->field` 表达式，有如下限制：

| | T是Copy语义 | T是Move语义 |
| ---- | ---- | ---- |
| p是immut借用 | p->field = expr; 不允许 | p->field = expr; 不允许 |
| | o = p->field; 允许 | o = p->field; 不允许 |
| p是mut借用 | p->field = expr; 允许 | p->field = expr; 允许 |
| | o = p->field; 允许 | o = p->field; 不允许 |

上表中 move / copy 语义分别指： T 是 _Owned 修饰的类型和 T 是其它类型。

注：上表中的赋值操作的权限，可同样应用于函数的传参和返回场景。

### 调用成员函数

通过借用调用成员函数，即 `p->method()` 的场景，实参和形参之间的规则如下：

| | void method(const This * _Borrow this) | void method(This * _Borrow this) | |
| --- | ---- | ---- | --- |
| p是immut借用 | 允许 | 不允许，immut借用不能创建出mut借用 | |
| p是mut借用 | 允许，允许从mut借用创建immut借用 | 允许 | |

举例来说：

```C
void int ::method1(const This *_Borrow this) {}
void int ::method2(This *_Borrow this) {}

void test() {
  int local = 5;
  const int *_Borrow p1 = &_Const local;
  int *_Borrow p2 = &_Mut local;
  p1->method1(); // ok: 形参类型和实参类型一致，都是不可变借用
  p1->method2(); // error: 形参是可变借用类型，实参是不可变借用类型，不可变借用不能创建出可变借用
  p2->method1(); // ok: 形参是不可变借用类型，实参是可变借用类型，允许从可变借用创建出不可变借用
  p2->method2(); // ok: 形参类型和实参类型一致，都是可变借用
}

int main() {
  test();
  return 0;
}
```

## 借用的类型转换

1. 对于任意类型 T，如果 T 实现了 _Trait TR，则允许指向类型 T 的借用向上转型为指向类型 TR 的借用；反过来，从类型 TR 的借用往类型 T 借用的转换，是不允许的。

```C
#include <stdio.h>

_Trait TR { void print(This * _Borrow this); };
void int ::print(int *this) { printf("%d\n", *this); }

_Impl _Trait TR for int;

void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  _Trait TR *_Borrow p = r; // ok: 支持 int* 类型的借用向上转型为 _Trait TR* 类型的借用
  p->print();
  int *_Borrow px = (int *_Borrow)p; // error: 禁止 _Trait TR* 向下转型
}

int main() {
  test();
  return 0;
}
```

2. 指向类型不同时，允许指向 T 的借用隐式转换为指向 void 类型的借用，反过来从类型 void 的借用往类型 T 借用的转换必须显式进行。其他指向类型不同的借用指针之间的类型转换均不允许。

```C
void test() {
  int x = 10;
  int *_Borrow r = &_Mut x;
  void *_Borrow p = r;
  int *_Borrow t1 = p; // error: 不允许隐式转为 void *_Borrow 类型
  int *_Borrow t2 = (int *_Borrow)p; // ok：允许强制转换为 void *_Borrow 类型
}

int main() {
  test();
  return 0;
}
```

3. 只允许在非安全区进行`T * _Borrow`和`T *`之间的转换。

```C
int main() {
  int *_Borrow p = (int *_Borrow)NULL; // ok: 非安全区允许 T * _Borrow 和 T * 之间的转换
  int *q = p; // error: 类型转换必须是显式的，禁止隐式类型转换
  _Safe { int *_Borrow p = (int *_Borrow)NULL; } // error: 安全区禁止 T * _Borrow 和 T * 之间的转换
  return 0;
}
```

4. 不允许在 `T *_Owned` 和 `T *_Borrow` 指针之间使用 C 风格强制类型转换

```C
int *_Owned test(int *_Owned p) {
  int *_Borrow q = (int *_Borrow) p; // error: 应当使用 &_Mut *p 代替强制类型转换
  int *_Owned r = (int *_Owned) q; // error: 不能通过强制类型转换从 T *_Borrow 创建一个 T *_Owned 副本
  return r;
}
```

5. 可变借用 `T *_Borrow` 类型可隐式转换为不可变借用 `const T *_Borrow` 类型，由编译器自动插入 `&_Const *` 操作符。不允许在可变借用和只读借用之间进行强制类型转换

   以下场景可发生可变借用到不可变借用的隐式转换：
   1. 变量的初始化与赋值
   2. 函数调用表达式传参
   3. 函数的返回语句

```C
void foo(const int *_Borrow);

void test1() {
  int a = 1;
  int *_Borrow p = &_Mut a;
  const int *_Borrow q = p; // ok
  q = p; // ok
  foo(p); // ok
}

const int *_Borrow test2(int *_Borrow p) {
  return p; // ok
}
```

```C
#include <stdio.h>

int main() {
  int local = 10;
  int *_Borrow p = &_Mut local;
  const int *_Borrow b = (const int *_Borrow)p; // error: 不允许将可变借用强制转换为只读借用
  printf("%d\n", *b);  // 读取 b
  *p = 1;              // 修改 p (如果转换被允许,这会违反借用规则)
  printf("%d\n", *b);  // 再次读取 b

  const int *_Borrow c = &_Const local;
  int *_Borrow m = (int *_Borrow)c; // error: 不允许将只读借用强制转换为可变借用
  *m = 20;  // 如果转换被允许,这会违反const安全性

  return 0;
}
```

对于 `_ArrayElem` 借用，还允许将 `T *_Borrow _ArrayElem` 隐式转换为 `T *_Borrow`，这等价于“在当前位置重新取借用、获得一个普通借用指针”；反方向的 `T *_Borrow -> T *_Borrow _ArrayElem` 强制类型转换不允许。

```C
void test(int arr[4], int local) {
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  int *_Borrow q = p; // ok，等价于 &_Mut *p

  int *_Borrow plain = &_Mut local;
  int *_Borrow _ArrayElem bad = (int *_Borrow _ArrayElem)plain; // error: 不允许反向转换
}
```

6. 允许在变量初始化、变量赋值、函数传参和返回中将 `_Borrow` 指针隐式转换为 `_Bool`

```c
void foo(int *_Borrow _Nullable p) {
  _Bool flag = p; // equivalent: _Bool flag = p != nullptr;
}
void bar(int *_Borrow _Nullable p, _Bool flag) {
  flag = p; // equivalent: flag = p != nullptr;
}
void use(_Bool);
void baz(int *_Borrow _Nullable p) {
  use(p); // equivalent: use(p != nullptr);
}
_Bool foobar (int *_Borrow _Nullable p) {
  return p; // equivalent: return p != nullptr;
}
```

## 借用的其它规则

除了上面的那些规则，对于借用，我们还有如下规则：

1. 对于全局变量，我们无法在函数签名中跟踪哪个函数读取了全局变量，哪个函数修改了全局变量。为了保证安全性，毕昇 C 规定：在安全区内，只允许对全局变量取只读借用，不允许取可变借用。如果是对函数名做借用，从生命周期的角度，可以当做是对全局变量做借用。

2. 借用变量在使用前必须初始化。

```C
void test() {
  int *_Borrow p; 
  use(p); // error: 必须初始化
}

int main() {
  test();
  return 0;
}
```

3. 用一个借用类型的表达式给另外一个借用类型的 lvalue作初始化或再赋值，即`p = e`，`p`和`e`必须是同类型的借用类型，而且要求`e`的生命周期必须大于 p 的生命周期。

```C
#include <stdio.h>

void test() {
  int x = 1;
  int *_Borrow p = &_Mut x;
  {
    int y = 2;
    int *_Borrow pp = &_Mut y;
    p = pp; // error: pp 生命周期小于 p
    printf("%d\n", *p);
  }
  printf("%d\n", *p);
}

int main() {
  test();
  return 0;
}
```

基于此规则，一个`struct`内部的`_Borrow`指针成员，是不可以对这个`struct`或者它的其它成员做借用的。

```C
struct S {
  int m;
  const int *_Borrow p;
};

void test() {
  struct S s = {.m = 0, .p = &_Const s.m}; // error: 因为s.p的生命周期与s.m的生命周期相同
}

int main() {
  test();
  return 0;
}
```

4. 借用变量不允许是全局变量，只能是局部变量。

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int g = 5
int *_Borrow p = &_Mut g; // error: 借用变量不允许是全局变量
void test() { int *_Borrow p = &_Mut g; }

int main() {
  test();
  return 0;
}
```

5. 不允许对包含借用的表达式，再取借用。同理，借用类型`T* _Borrow`中，`T`本身及其成员不能是借用类型。

```C
#include "bishengc_safety.hbs" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

struct R {
  int *_Borrow p;
};

void test() {
  int local = 5;
  int *_Borrow *_Borrow p = &_Mut(&_Mut local); // error: 不允许多级借用指针

  struct R r1 = {.p = &_Mut local};
  struct R *_Borrow r2 = &_Mut r1; // error: r1 中已经包含了借用
}

int main() {
  test();
  return 0;
}
```

6. 不允许为借用类型实现 _Trait。

```C
_Trait TR{};

_Impl _Trait TR for int *_Borrow; // error: 不允许为借用类型实现 _Trait

int main() { return 0; }
```

7. 不允许为借用类型添加成员函数。

```C
void int *_Borrow::f() {} // error: 不允许为借用类型添加成员函数

int main() { return 0; }
```

8. union 的成员不允许是借用类型。

```C
union U {
  int *_Borrow p; // error: 借用指针不允许作为 union 成员
};

int main() { return 0; }
```

9. 借用指针类型不能是泛型实参。

10. 普通借用指针变量不支持下标运算；`_Borrow _ArrayElem` 指针支持下标运算。

11. 普通借用指针变量不支持算术运算；`_Borrow _ArrayElem` 指针支持 `+`、`-`、`+=`、`-=`、`++`、`--` 运算。

12. 允许同类型的借用变量之间，使用 `==`、`!=`、`>`、`<`、`<=`、`>=` 等比较运算符操作。

13. 允许对借用类型使用 `sizeof`、`alignof`操作符，并且有：
    `sizeof(T* _Borrow) == sizeof(T*)`
    `_Alignof(T* _Borrow) == _Alignof(T*)`
    `sizeof(T* _Borrow _ArrayElem) == sizeof(T*)`
    `_Alignof(T* _Borrow _ArrayElem) == _Alignof(T*)`

14. 允许对借用类型使用一元的`&`、`!`及二元的`&&`、`||`运算符。

15. 不允许对普通借用类型使用一元的`-`、`~`、`&_Const`、`&_Mut`、`[]`、`++`、`--`运算符，也不允许对普通借用类型使用二元的`*`、`/`、`%`、`&`、`|`、`<<`、`>>`、`+`、`-`运算符。对于 `_Borrow _ArrayElem`借用，其中的 `[]`、`+`、`-`、`+=`、`-=`、`++`、`--` 允许使用，其余限制不变。

```C
_Safe int foo(void) {
  int arr[4] = {1, 2, 3, 4};
  int *_Borrow _ArrayElem p = &_Mut arr[0];
  p = p + 1; // ok: _Borrow _ArrayElem 支持 +
  p += 1; // ok: _Borrow _ArrayElem 支持 +=
  ++p; // ok: _Borrow _ArrayElem 支持 ++
  int x = p[0]; // ok: _Borrow _ArrayElem 支持 []

  int *_Borrow q = p; // ok: 降级为普通借用
  // q = q + 1; // error: 普通借用不支持 +
  // q += 1; // error: 普通借用不支持 +=
  // ++q; // error: 普通借用不支持 ++
  // int y = q[0]; // error: 普通借用不支持 []
  return x;
}
```

16. 如果一个借用指针变量指向的是函数，那么可以通过这个借用指针变量来调用函数。

```C
#include <stdio.h>

void f() { printf("f()\n"); }

void test() {
  void (*_Borrow const p)() = &_Const f; // ok: 对函数取不可变借用
  p();
}

int main() {
  test();
  return 0;
}
```

17. 不允许对函数做可变借用，只能做只读借用。

18. 允许 _Borrow 指针作为 `if` `while` `do-while` `for` 语句和三元表达式的条件，不允许作为 `switch` 语句的条件

```c
void foo(int *_Borrow _Nullable p) {
  if (p) { // equivalent: p != nullptr
  }
  while (p) { // equivalent: p != nullptr
  }
  do {
  } while (p); // equivalent: p != nullptr

  for (;p;) { // equivalent: p != nullptr
  }
  switch (p) { // error
  default: 
    break;
  }
  int x = p ? 2 : 1; // equivalent: p != nullptr ? 2 : 1;
}
```
