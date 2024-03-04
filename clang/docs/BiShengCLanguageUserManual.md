<div>
    <div style='height:1000px;width:90%'>
        <div style='border-top:1px solid'> </div>
        <div style='margin-top:10%;font-size:28px;width:100%;heigth:80%;font-width:600;letter-spacing:2px;text-align:center'>
            <span style=''>毕昇 C 用户手册</span>            
        </div>
    </div>
</div>
<div style=“package-break-after:always;“></div>

## 编译器安装和使用

### 安装毕昇 C 编译器

首先请前往毕昇 C 版本发布渠道下载安装包，当前我们为开发者提供 Linux 平台上的 rpm 格式安装包，支持  clang + llvm 后端和 clang + BiShengC 后端。

> 注：其中 clang + llvm 支持编译和运行环境都在 X86_64 环境；而 clang + BiShengC 支持 X86_64 环境编译，aarch64 环境运行，如果用户想要在 X86_64 环境运行，则需要使用 qemu 工具。

#### clang + llvm 版本安装

以 clang+llvm_15.04_BiShengCLanguage-x-y.rpm 为例，下载到本地后，安装步骤如下 ：

- 解压

  ```shell
  $ rpm2cpio clang+llvm_15.0.4_BiShengCLanguage-x-y.rpm | cpio -div
  # 解压后会在当前目录生成 /opt/buildtools/clang+llvm_15.0.4_BiShengCLanguage 目录
  ```

- 配置环境变量

  > 注：所配置的环境变量仅在运行下列命令的当前 `shell` 会话窗口有效，重启 `shell` 后需要重新配置环境变量。若想要这些环境变量在 shell 每次启动时重新生效，你可以在 `$HOME/.bashrc` 或 `$HOME/.ashrc` (根据 shell 的种类而定) 等 `shell` 初始化配置文件中加入如下命令：

  ```shell
  $ export LLVM_HOME=/path/to/clang+llvm_15.0.4_BiShengCLanguage/bin
  $ export PATH=$LLVM_HOME:$PATH
  ```

- 验证安装，执行以下命令：

  ```shell
  $ clang --version
  ```

若安装成功，可以看到当前的版本号和安装目录，其格式如下：

`clang version xx.xx.xx`

`InstalledDir: /path/to/clang+llvm_15.0.4_BiShengCLanguage/bin`

#### clang + BiShengC 版本安装

以 bsc_host_linux_x86_64_target_aarch64-x-y.rpm 为例，下载到本地后，安装步骤如下 ：

- 解压

  ```shell
  $ rpm2cpio bsc_host_linux_x86_64_target_aarch64-x-y.rpm | cpio -div
  # 解压后会在当前目录生成 /opt/buildtools/bsc_host_linux_x86_64_target_aarch64 目录
  ```

- 下载第三方工具包（由于 rpm 不包含 GCC 交叉编译工具链及 qemu 工具，可直接下载我们提供的第三方工具包）

  ```shell
  # 下载与解压
  $ git clone https://gitee.com/bisheng_c_language_dep/ThirdParty.git
  $ cd ./ThirdParty
  $ dpkg-deb -R qemu-user_2.11+dfsg-1ubuntu7.40_amd64.deb qemu
  ```

- 配置环境变量（也可加入 `$HOME/.bashrc` 或 `$HOME/.ashrc` 等配置文件）

  ```shell
  $ export Maple_Path=/path/to/bsc_host_linux_x86_64_target_aarch64
  $ export MTP=/path/to/ThirdParty
  $ export BiShengC_GCC_Path=$MTP/gcc-linaro-7.5.0-2019.12-i686_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
  $ export PATH=$PATH:$Maple_Path/bin:$MTP/qemu/usr/bin
  
  # 设置编译时和 qemu 运行时依赖的库
  $ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MTP/gcc-linaro-7.5.0-2019.12-i686_aarch64-linux-gnu/aarch64-linux-gnu/libc/lib
  $ export QEMU_LD_PREFIX=$MTP/gcc-linaro-7.5.0-2019.12-i686_aarch64-linux-gnu/aarch64-linux-gnu/libc
  ```

### 卸载与更新

在 linux 平台，删除相应目录下的文件，移除环境变量即可完成卸载（最简单的，您可以重新开一个 shell 环境）。

如需要更新，用户需自行卸载后重新下载安装新的版本。

### 第一个毕昇 C 程序

如下示例中，我们创建一个名为 bsc_project 的目录，创建 demo.cbs 源程序文件，并键入以下内容。

注：毕昇 C 的源码文件以 .cbs 为后缀。

```shell
$ mkdir -p bsc_project & cd .bsc_project
$ touch demo.cbs
# 向demo.cbs写入如下内容：
```

```c
#include <stdio.h>

struct Foo {
    int a;
};

int struct Foo::getA(struct Foo* this) {
    return this->a;
}

int main() {
    struct Foo foo = {.a = 1};
    printf("foo.getA() = %d\n", foo.getA());
    return 0;
}
```

使用编译命令编译该文件，得到可执行文件，如下：

```shell
# clang + llvm 编译运行
$ clang demo.cbs -o demo
$ ./demo
foo.getA() = 1

# clang + maple 编译运行
$ maple demo.cbs -o demo
$ qemu-aarch64 demo
foo.getA() = 1
```

输出如上结果，说明你已经成功应用了毕昇 C 的成员函数特性。



## 毕昇 C 简介
在系统编程领域，C/C++ 是应用最广泛的编程语言。在硬件资源十分受限的嵌入式场景下，C 语言使用的最多，但使用 C 语言编码存在很多痛点问题，比如 C 语言中指针使用带来的内存安全问题，C 语言缺乏原生的并发支持，以及一些基础的编程抽象(如泛型等)。近年来，在系统编程语言领域有不少探索的工作，比如 Rust，主打内存安全(所有权，生命周期，borrow checker 等)和并行并发(无栈协程)。Rust 是一门全新的编程语言，采用了和 C/C++ 完全不同的语言设计，学习曲线陡峭，也无法解决存量代码开发的问题。

在这个背景下，毕昇 C 采用了不同的策略，它基于 C 语言做了很多增强的设计，比如更强的内存安全特性，语言层面支持并发等，且可以在存量代码中渐进式的使用这些特性而不用完全重写已有代码。可以认为，毕昇 C 是 C 语言的一个超集。这本用户手册，将从以下三个方面介绍毕昇 C ：
- 基础编程抽象：成员函数，trait，泛型
- 内存安全：所有权，借用
- 并发：无栈协程

------

## 成员函数

### 概述
在 C 语言里，如果我们想表达某个类型的数据(data)有对应的某个方法(operation)，一般使用全局函数，让这个数据类型作为入参，如下：
```c
struct Data{
  int x;
};

void print_data(struct Data* data) {
    // 提供 print data 的实现逻辑
    printf("print data\n");
}

void test() {
    struct Data data = {.x = 1};
    print_data(&data);
}
```

类似的，针对 int 类型我们可能需要 `print_int` 的函数，针对 float 类型需要 `print_float` 的函数，这不是一件令人愉快的事情，我们希望有一种更简洁的方式去表达类型与方法关联这件事，这就是为C语言引入成员函数的部分动机。引入成员函数后，上面例子的代码可以这么写：
```c
struct Data{
  int x;
};

void struct Data::print(struct Data* this) {
    // 提供 print data 的实现逻辑
    printf("print data\n");
}

void int::print(int* this) {
    // 提供 print int 的实现逻辑
    printf("print int\n");
}

void float::print(float* this) {
    // 提供 print float 的实现逻辑
    printf("print float\n");
}

void test() {
    struct Data data = {.x = 1};
    int a = 1;
    float b = 1.0;
    data.print();
    a.print();
    b.print();
}

```

如果，我们想表达某些类型具有一组相似的行为，比如上面例子中的 `print` ，我们可以定义一个 `trait`，然后让 `struct Data` `int` `float` 等类型实现这个 `trait` 。成员函数和 `trait` 相结合，非常有表达力。关于 `trait` 的介绍，参考后续章节。

下面，我们简单介绍下毕昇 C 成员函数的一些具体规则：

### 基本语法

当我们想为某个类型增加一个成员函数时，我们只需要在普通函数定义的语法基础上，在函数名如 foo 前，增加 `typename::foo`，如下所示：
```c
void foo1() {
  // do nothing
};

void int::foo2(int* this) { // 实例成员函数，第一个入参是 this
  // do something
}

void int::foo2() { // 静态成员函数
  // do something
}

```

其中，type-name 可以是基础类型如 `int`, `float` 等，也可以是用户自定义的结构体等，符合 C 语言对类型的定义规则。下面是更多的用法示例：

```c
// case 1
void int::print(int* this); // 声明

void int::print(int* this){ // 定义
    printf("int::print");
}

// case 2
struct S1{};
// 错误使用 S，在 C 语言里 struct S 才是一个类型
void S1::print(struct S1* this); // error: must use 'struct' tag to refer to type 'S'

// case 3
typedef struct {
}S2;
void S2::print(S2* this); // Ok, S2 是 typedef 后的 struct S2
```

采用这样的语法设计有一个好处，那就是我们很方便就可以给已有类型增加成员函数而不用侵入式修改源码。

### 关于 `this`

在上面的成员函数的例子中，参数列表中的第一个参数如果为 `this`(如果有 `this`，它也只能是第一个参数)，它表示指向该成员函数对应类型实例的指针，它是一个“实例成员函数”。如果成员函数参数列表中，没有 `this` 存在，则表示这是一个“静态成员函数”。
```c
typedef struct {/*...*/} M;
void M::f(M* this, int i) {} // 实例成员函数

typedef struct {/*...*/} N;
void N::f() {} // 静态成原函数

int main() {
    M x;
    x.f(1); // Ok
    M::f(&x, 1); // Ok
    M1 * x1 = &x;
    x1->f(1); // Ok

    N y;
    y.f(); // Err: y does not have instance member function, use N::f instead.
    N::f(); // Ok
    return 0;
}

```
对于实例成员函数(第一个入参为`this`)，其调用方式有两种：(1) 和访问成员变量类似，实例类型调用用 `.` 符号，如 `x.f(1)`；指针类型调用，用 `->` 符号，如 `x1->f(1)` (2) 普通函数调用方式，如 `M::f(&x, 1)`。

对于静态成员函数，其调用方式和调用普通全局函数类似，区别只是函数名变成 `type-name::func-name`，如 `N::f()`。

### 其他规则

- 成员函数支持声明和定义分开，如下：
```c
// 声明
const char* int::to_string(const int* this) ;

// 定义
const char* int::to_string(const int* this){
    // to_string 的实现，略
}

```
- 新增成员函数不影响原类型的 layout 包括 size 和 alignment
- 成员函数不支持重载和重定义
- 成员函数的名字不允许与成员变量相同，适用于 struct, union, enum 等
- 成员函数允许赋值给函数指针。
- 禁止对 cv-qualified type (被类型修饰符修饰的类型，如 const int 等) 添加成员函数
- 禁止对 “函数类型” "数组类型" "指针类型" 添加成员函数

------

## 泛型

### 泛型概述

泛型是一种编程技术，它可以让类型（如整数、字符串等基本类型，或者用户自定义的类型）作为参数传递给函数、类或接口，从而实现代码的复用和灵活性，并可以非常高效地实现一些方法。

毕昇 C 的泛型是一种编译时的泛型机制，它可以定义一个通用的函数或类，然后根据不同的类型参数生成不同的实例。

目前，毕昇 C 已支持**泛型函数**和**泛型结构体**。

#### 实现动机

泛型的目的是为了提高代码的**效率**和**重用性**而实现的，其优点在于：

- 避免了代码冗余
- 提高了代码的可读性和可维护性
- 实现了类型安全和编译时检查
- ...

泛型编程使程序员能够编写一个适用于所有数据类型的通用算法。它消除了如果数据类型是整数、字符串或字符，就需要创建不同算法的场景。

这就使得用户可以为类或函数声明一种通用模式，使得类中的某些数据成员或成员函数的参数、返回值取得任意类型。

#### 示例

以下方的代码为例：

```c
int sum_int(int a, int b) {
    int c = a + b;
    return c;
}

float sum_float(float a, float b) {
    float c = a + b;
    return c;
}

int main() {
    int sum1 = sum_int(1, 2);
    float sum2 = sum_float(1.2, 2.5);
}
```

可以看出，对于sum方法，如果要实现返回值分别为 **float** 和 **int** 两种情况，普通的 C 语言需要为同样实现的方法定义两次。

但是如果使用毕昇 C 的泛型语法，则只需定义一次，即可在实例化时重复使用，如下：

```c
T sum_t<T>(T a, T b) {
    T c = a + b;
    return c;
}

int main() {
    int sum1 = sum_t<int>(1, 2);
    float sum2 = sum_t<float>(1.2, 2.5);
}
```

可以看出，泛型功能的引入，对于用了相同算法的声明场景，其代码量有明显的减少。

### 语法规则

对于毕昇 C 泛型，我们设计了如下的语法规则：

- 在声明泛型函数/结构体的定义时，泛型参数列表要用尖括号 '<>' 包裹起来，尖括号内部的类型一般是 identifier ，如 'T1', 'T2' 之类 。


- 在实例化时，既可以在尖括号内部传入具体的类型，如 int, float, struct S 等，也可以省略尖括号的书写，编译器会根据实际传入的参数进行隐式的类型推导。此外，如果将现有类型 typedef 为其他别名后，同样可以在实例化时用作泛型实参。


- 同时，毕昇 C 的泛型函数和泛型结构体也支持使用**常量参数**作为泛型的形参。

下面我们将分为三个部分详细说明。

#### 泛型函数

对于泛型函数，我们对语法规则的设计，主要体现在两个方面：

1. 声明时，区别于普通函数声明，我们需要在**函数名**和**函数入参**之间，添加一对尖括号 '<>' ，并在尖括号中写明泛型函数的**泛型形参**，其中形参可以为任何合法的名字（此处的合法指的是不会导致语义冲突的情况）；

   同时，对于函数返回值的类型，可以是普通的 builtin 类型，可以是用户已经定义过的结构体，也可以是泛型形参中的一个（如 T）。

2. 实例化时，类型既可以在尖括号中显式指定，也可以省略尖括号以及其中的内容，由编译器进行隐式推导：

   1. 显式指定类型时，区别于普通的函数调用，同样需要在被调用的**函数名**与**函数入参**之间添加一对尖括号 '<>' ，且中间无空格；然后在尖括号中传入**泛型实参**，此处的实参可以是 builtin 类型，也可以是用户已经定义过的结构体。
   2. 隐式推导时，写法与普通的函数调用相同，毕昇 C 编译器会根据函数调用传入的参数类型，自动推导**泛型实参**的类型。不过为了代码可读性等，推荐使显式指定类型。

下面是一些用法示例：

```c
typedef long int LT;

// 泛型函数
T max<T>(T a, T b) {						// 开头的'T'为函数返回值类型，而'<>'中的'T'泛型函数'max'的泛型形参
    T Max = a > b ? a : b;
    return Max;
}

int main() {
    int a = 3;
    int b = 5;
    int c = 4;

    // 泛型函数 实例化
    int max1 = max<int>(a, b);				// 显示指定泛型实参类型
    int max3 = max(a, c);				    // 隐式推导，编译器自动推导'T'为 int 类型

	return 0;
}
```



#### 泛型结构体

对于泛型结构体，我们对语法规则的设计，同样体现在两个方面：

1. 声明时，区别于普通结构体声明，我们需要在**结构体名**的后面之间，添加一对尖括号 '<>' ，并在尖括号中写明泛型结构体的**泛型形参**。
2. 实例化时，泛型结构体仅支持显式指定类型，即：显示指定类型时，区别于普通的结构体的构造，同样需要在被构造的**结构体名**后面添加一对尖括号 '<>' ，且中间无空格；然后在尖括号中传入**泛型实参**，此处的实参可以是 builtin 类型，也可以是用户已经定义过的结构体。

下面是一些用法示例：

```c
typedef long int LT;

// 泛型struct
struct S<T, B>{
    T a;
    B b;
};

// 泛型union
union MyUnion<T1, T2> {
    T1 u1;
    T2 u2;
};

// 返回类型为'泛型union'的泛型函数'foo_union'
union MyUnion<T1, T2> foo_union<T1, T2>(union MyUnion<T1, T2> *this) {
    return *this;
}

int main() {
    // 泛型struct 实例化
    struct S<int, LT> s1 = {.a = 42, .b = 5};		// 使用 typedef 后的类型作为泛型实参

    // 泛型union 实例化
    union MyUnion<int, float> p;
    foo_union(&p);		// 泛型函数 foo_union 的隐式类型推导

    return 0;
}
```



#### 常量泛型

除了基础泛型的实现，毕昇 C 还引入了常量泛型的功能。具体来说，常量泛型是一种允许程序项在常量值上泛型化的特性。也就是说，常量可以作为泛型参数传递到泛型变量中，代码会根据常量参数而进行特化，从而确保无开销，并可以直接在代码中作为常量来使用。

例如，在 毕昇 C  中，可以定义一个泛型结构体，其中一个泛型参数是一个**常量泛型参数**，该参数可以用于表示结构体内定义的数组的大小。

这样，通过在实例化时传入不同的常量值，便可以生成多个不同大小的数组对象，如：

```c
struct Array<T, int N> {
    T data[N];
};

int main() {
    struct Array<int, 5> arr1();
    struct Array<int, 10> arr2();
    return 0;
}
```

如上，这里的 '10' 和 '5' 就是常量泛型参数 'int N' 的实参，它们决定了数组的大小。

目前，泛型常量的规则如下：

- 常量泛型的形参只支持“可编译时计算的类型”，目前只支持整数类型
- 常量泛型的实参只能是“编译时可计算”的常量表达式
- 语法上，如果只是 int 字面量、constexpr 常量，那么可以不需要小括号，其它常量表达式一律需要用小括号

目前，毕昇 C 对于常量泛型的实现仅限于整形的“整数常量”：

- 对于声明时，形参列表仅接受 int 及其修饰符 long、short、unsigned、signed，以及上述关键字的各种组合。同时也支持将上述关键字 typedef 成其他别名再做为形参。
- 对于实例化时，目前泛型实参列表仅支持 IntegerLiteral （即常量1，2之类），后续会逐步支持 constexpr修饰的变量与常量表达式，届时将在最新版本的文档中有说明。

下面是一些用法示例：

```c
#include <stdio.h>

typedef long long int LLInt;

// 使用了泛型常量的泛型函数
int print_dataSize<T, int B>()
{
    T data[B];
    printf("the size of data is %d\n", B);
  	return B;
}

// 使用了 typedef别名 作为泛型常量的泛型函数
void print_const<LLInt B>() {
    printf("the const is %d\n", B);
}

// 使用了泛型常量的泛型struct
struct Array_1<T, int N>
{
  	T data[N];
};

// 使用了 typedef别名 作为泛型常量的泛型struct
struct Array_2<LLint B, int C, T>
{
  	LLint data[B];
  	int data[C];
  	T a;
};

int main() {
  	int a1 = print_dataSize<int, 5>();
  	print_const<20>();

  	struct Array_1<int, 5> arr1;
  	struct Array_2<5, 6, int> arr2 = {.a = 1};

    return 0;
}
```

------
## constexpr
### 概述
constexpr 可以定义编译时常量和编译时计算返回值的函数。
在 C 语言中已经有了 const 关键字，而它的含义更多的是 readonly 的意思，不是“编译时常量”这么强的约束，所以我们需要引入 "constexpr" 这个关键字来表达编译时计算的能力。

定义“编译时计算”的类型：
bool,char(signed char, unsigned char), 整数类型（包括 int 以及被 short/signed/unsigned/long/long long 等修饰的 int 类型，不包括 enum 类型），以及这些类型的别名。

定义“常量计算”上下文，也就是 constexpr 修饰的变量和函数可以作为常量使用的场景：
- 可以用于 static_assert 中，第一个条件参数属于“常量计算”上下文
- 可以用于定义定长数组，数组长度属于“常量计算”上下文
- 可以用于初始化其它 constexpr 常量
- 可以用于常量泛型的实参

下面举例说明什么是“常量计算”上下文。
```
void bar<int N>(){}

constexpr int foo() {
    return 5;
}

int main() {
    constexpr int a = 5;   
    
    //可以用于常量泛型的实参
    bar<a>();
    bar<foo()>();
    
    //可以用于定义定长数组
    int arr1[a] = {0};
    int arr2[foo()] = {0};
    
    //可以用于初始化其它 constexpr 常量
    constexpr int b = a;
    constexpr int c = foo();
    
    //可以用于 static_assert 中
    _Static_assert(a == 5, "fail");
    _Static_assert(foo() == 5, "fail");

    return 0;
}
```
###使用规则
####constexpr 修饰变量
1. constexpr 可以修饰一个常量定义，且必须在定义时被初始化，否则要报错
```
constexpr int a = 5;
constexpr int b; //error,未初始化
```
2. constexpr 修饰的常量在定义之后，不可被修改
```
constexpr int a = 5;
a = 10; //error
```
3. constexpr 修饰常量的类型只能是上述“编译时计算”的类型
```
constexpr float a = 5.0;//error,“编译时计算”的类型不包括浮点类型
```
4. constexpr 修饰的常量的初始化表达式必须可以在编译时求值，否则要报错。可编译时求值的常量表达式可以是：
- 字面量
- constexpr 修饰的常量
- sizeof,_Alignof 表达式
- 以可编译时求值的常量表达式作为实参，调用 constexpr 函数
- 由以下运算符组合起来的常量表达式，也是常量表达式：+,-,*,/,%,>,<,==,!=,<=,>=,&,|,^,~,!,&&,||,<<,>>,?:

举例说明：
```
//场景1
int a = 10; 
constexpr int b = a;//error
//场景2
constexpr int a = 10; 
constexpr int b = a;
//场景3
constexpr int a = sizeof(int);
constexpr int b = sizeof(int);
//场景4
constexpr int foo() {
    return 5;
};
constexpr int a = 10; 
constexpr int b = foo(a); 
//场景5
constexpr int b = 1 == 1.0; 
```

####constexpr 修饰函数
1. constexpr 可以修饰一个函数声明或者定义
```
constexpr int foo();
constexpr int foo() {
    return 5;
}
```
2. constexpr 修饰的函数，参数和返回类型，只能是上述“编译时计算”的类型
```
constexpr void foo(); //error,返回值是空值，不属于“编译时计算”的类型
```
3. constexpr 可以修饰泛型函数
```
constexpr int foo<T>();
```
4. constexpr 函数体内的所有语句，都是编译期可求值的
- constexpr 函数体内不允许定义 static 变量
- constexpr 函数体内不允许调用非 constexpr 函数
- constexpr 函数体内不允许访问外部的非 constexpr 变量
- constexpr 函数体内不允许内嵌汇编
- constexpr 函数体内允许定义不使用 constexpr 修饰的局部变量，这些变量也只能是“编译时计算”的类型

5. 在非“常量计算”的上下文中，constexpr 修饰的函数可以当作普通函数使用，实参不需要是常量，返回值也不需要是常量。在“常量计算”的上下文中，实参和返回值都要求是常量表达式，否则会报错
```
constexpr int foo() {
    return 5;
};

int a = 10; 
constexpr int b = foo(a);//error，foo 函数处于“常量计算“上下文中
int c = foo(a);//ok，foo 函数处于非“常量计算“上下文中
```
6. constexpr 可以修饰成员函数
```
//普通成员函数，参数 This* this 不属于编译时计算类型
constexpr int int::foo1(This* this) { //error
    return 5;
}

//静态成员函数
constexpr int int::foo2() {
    return 5;
}

int main() {
    constexpr int c = int::foo2();//ok，可编译期求值
    return 0;
}
```
6. constexpr 不允许修饰 async 函数
7. constexpr 不允许支持变长参数
8. 函数的形参不能用 constexpr 修饰
```
int foo1(constexpr int a) { //error
    return 5;
}

constexpr int foo2(constexpr int a) { //error
    return 5;
}
```
------

## type trait

type trait 可以看作是一个编译期计算返回值的 constexpr 函数。
BSC标准库中提供了一系列 type trait 泛型函数，使用时需要导入头文件 bsc_type_traits.hbs

目前实现的 type trait 函数有：
```
// 判断类型的分类
constexpr bool is_integral<T>();
constexpr bool is_floating_point<T>();
constexpr bool is_pointer<T>();
constexpr bool is_function<T>();
constexpr bool is_array<T>();
constexpr bool is_struct<T>();
constexpr bool is_union<T>();
constexpr bool is_enum<T>();
constexpr bool is_void<T>();
//判断类型的属性
constexpr bool is_signed<T>();
constexpr bool is_unsigned<T>();
constexpr bool is_const<T>();
constexpr bool is_volatile<T>();
constexpr size_t rank<T>();
constexpr size_t extent<T, size_t N>();
//判断类型的关系
constexpr bool is_same<T1, T2>();
constexpr bool is_convertible<From, To>();
```
针对其中一些作出说明和举例：
```
constexpr size_t rank<T>(); //可用于计算数组的维数，如
rank<int>() == 0;
rank<int[5]>() == 1;
rank<int[5][5]>() == 2;

constexpr size_t extent<T, size_t N>();//可用于计算数组第 N 维元素的个数，如
extent<int[3],0>() == 3;
extent<int[3],1>() == 0;
extent<int[3][4],0>() == 3;
extent<int[3][4],1>() == 4;
extent<int[3][4],2>() == 0;

constexpr bool is_same<T1, T2>();//判断类型 T1，T2 是否一样，忽略类型别名

constexpr bool is_convertible<From, To>();//判断类型 From 是否可以隐式转换为类型 To，如
is_convertible<int, float>() == true;
is_convertible<int, const int>() == true;
is_convertible<int, volatile int>() == true;
is_convertible<int, signed>() == true;
is_convertible<int, void>() == false;
is_convertible<int, int*>() == false;
is_convertible<int, void*>() == false;
is_convertible<struct S, struct G>() == false;
```

使用时就像普通泛型函数一样
```
#include<stdio.h>
#include<bsc_type_traits.hbs> 

int main() {
    printf("%d\n",is_integral<int>()); //1
    printf("%d\n",is_integral<float>()); //0
    return 0;
}
```

type trait 函数可以在泛型函数和泛型结构体的成员函数中使用
```
#include<stdio.h>
#include<bsc_type_traits.hbs> 

struct S<T> {};
void struct S<T>::foo(struct S<T>* this) {
    if (is_integral<T>()) {
        printf("integral\n");
    } else {
        printf("not integral\n");
    }
}

void bar<T>() {
    if (is_integral<T>()) {
        printf("integral\n");
    } else {
        printf("not integral\n");
    }
}

int main() {
    struct S<int> s1;
    struct S<float> s2;
    s1.foo(); //print "integral"
    s2.foo(); //print "not integral"
    bar<int>();  //print "integral"
    bar<float>(); //print "not integral"
    return 0;
}
```

type trait 函数也可用于静态断言中
```
#include<bsc_type_traits.hbs> 

int main() {
    _Static_assert(is_integral<int>() == true, "fail");
    _Static_assert(is_integral<float>() == false, "fail");
    return 0;
}
```

------

## trait

### 概述

trait 是一种定义行为的方式，它类似于其它语言中的接口或抽象类，目的是定义一个实现某些目的所必须的行为集合。trait 定义了一组方法签名，这些方法可以被多个结构体、枚举体或内置类型共享。主要作用是为了实现代码的复用和抽象，从而提高代码的可维护性和可扩展性。

### trait 语法规则

毕昇 C 引入关键字 `trait` 与 `impl`，通过关键字 `trait` 来定义，通过 `impl` 可以为一个类型实现一个或多个 trait。下面通过一些代码示例来了解 trait 的使用方法。

#### trait 的定义

**语法：**

```c
trait TraitName {
  // 定义 trait 中的方法签名
};
```

其中，`TraitName` 是 trait 的名称，后面跟着一对花括号，里面可以定义一些方法的签名。trait 中定义的方法不支持默认实现，必须由实现该 trait 的类型提供具体实现。下面我们来看一个简单的例子：

```c
trait T {
    void doSomeThing1(This* this);
    void doSomeThing2(This* this);
};
```

**规则：**

1. trait 定义只能出现在 top-level

```c
void test() {
    trait T {}; // error: 不能在函数体中定义 trait
}

struct MyStruct {
    trait T {}; // error: 不能在结构体中定义 trait
};
```

2. trait 内要求函数首个入参且只有首个入参类型为 `This` 指针，命名为 `this`; `This` 指代实现了 trait 的具体类型

```c
trait T {
    void doSomeThing1(This* this); // ok
    void doSomeThing2(This* a); // error: 第一个参数名必须为 this
    void doSomeThing3(int a, This* this); // error: 第一个参数类型必须为 This
};
```

3. trait 内只允许声明函数

```c
trait T {
    void doSomeThing1(This* this) { // error: trait 内的成员函数不支持默认实现
        ...
    }
};
```

4. trait 内可以没有方法签名

```c
trait T {}; // ok
```

5. 不允许给 trait 类型扩展成员函数

```c
void trait T::getArea(trait T* this) { // error: 不允许给 trait 类型扩展成员函数
	...
}
```



#### 实现 trait

**语法：**

```c
impl <trait> for <type>;
```

我们可以通过 `impl` 关键字来为一个类型实现一个 trait。直观的，我们来看一个简单的例子：

```c
trait T {
    void f(This* this);
};

struct S {};
void struct S::f(struct S* this);
void int::f(struct S* this);

impl trait T for int;
impl trait T for struct S;
```

**规则：**

1. 在 impl 语句之前，一定存在 `<trait>` 和 `<type>` 的定义
2. 在 impl 语句之前，`<type>` 必须已经声明了 `<trait>` 中所有成员函数

```c
trait T {
    int f1(This* this);
    int f2(This* this);
};

struct S{};

int struct S::f1(struct S* this);
impl trait T for struct S; // error: struct S 未扩展声明函数 f2
int struct S::f2(struct S* this);
impl trait T for struct S; // ok，struct S 已扩展声明了 trait 中所有成员函数
```

3. `<type>` 类型不允许是 trait

```c
impl trait T for struct S; // ok，支持通过 impl 对已有 struct/union/enum 或内置类型实现 trait
impl trait T for trait T; // error: 不允许给 trait 类型实现 trait
```

4. 支持对 `typedef` 类型实现 trait

```c
typedef struct S S1;
impl trait T for S1;
```



#### trait 类型的变量

我们可以定义 trait 指针类型的变量，并可以通过该指针变量进行函数调用等，具体使用方法如下：

```c
#include <stdio.h>

struct S {
    float num;
};

typedef trait Print {
    void print(This* this);
}P;
void struct S::print(struct S* this) {
    printf("This is a struct instance, valued %f\n", this->num);
}
void int::print(int* this) {
    printf("This is an int instance, valued %d\n", *this);
}

impl P for struct S;
impl P for int;

void test() {
    struct S s = { 0.0 };
    int a = 1;
    float b = 1.0;

    trait Print* p;
    p = &s; // ok，隐式转换
    p->print(); // This is a struct instance, valued 0.000000
    p = &a;
    p->print(); // This is an int instance, valued 1
    p = &b; // error: float 类型未实现 trait Print，不能进行赋值
}
```

**规则：**

1. 支持对 trait 进行 `typedef`

```c
typedef trait Print {
    ...
}P;
```

2. 只允许声明 trait 指针类型的变量

```c
trait Print* p1; // ok
trait Print p2; // error: 不允许定义 trait 类型的变量
```

3. 如果 `<type>` 实现了 `<trait>`，那么指向这个 `<type>` 的指针可以被转换为 `<trait>` 类型的变量

```c
impl trait Print for struct S;

struct S s;
trait Print* t1 = &s; // 隐式转换
trait Print* t2 = (trait Print*)&s; // 显式转换
```

4. trait 指针类型的变量可以通过 `->` 方式调用该 trait 中的成员方法

```c
struct S s;
trait Print* t = &s;
t->print();
```

5. trait 指针类型的变量，允许用 `NULL` 赋值

```c
trait Print* p = NULL;
```

6. 允许指向 trait 的多级指针，但这种类型不能直接调用成员函数。不允许结构体的二级指针直接隐式转换成 trait 的二级指针

```c
trait Print* p;
p->print();
trait Print** q = &p; // ok
q->print(); // error: 多级指针不能直接调用成员函数
(*q)->print(); // ok
struct S s;
q = &&s // error: 不允许结构体的二级指针直接隐式转换成 trait 的二级指针
```

7. trait 指针类型的变量，不可以解引用

```
trait Print *p;
*p; // error: trait 指针类型的变量，不可以解引用
```

8. trait 指针类型变量可以用 `const` / `volatile` 修饰

```c
const trait Print* p1;
volatile trait Print* p2;
```

9. 支持 trait 指针类型作为函数参数及返回值类型

```c
trait Print* get(trait Print* t) {
    return t;
}
```

10. 支持 trait 指针变量和 `NULL` 做比较（这里的比较仅包含 `==` 和`!=`，下同）

```c
trait Print* p = NULL;
if (p == NULL) {} // ok
if (p != NULL) {} // ok
```

11. 支持 trait 指针变量和非 trait 指针变量比较

```c
int a = 1;
int *p1 = &a;
struct S s;
struct S *p2 = &s;
trait Print* p = NULL;
// 假设struct S 类型实现 trait F<int>，但 int 类型没有
if (p == p1) {}; // warning
if (p == p2) {}; // ok
if (p == a) {};  // error: 与非指针类型比较报 error
```

12. 支持 trait 指针变量和 trait 指针变量比较

```c
trait Print* t1 = NULL;
trait Print* t2 = NULL;
trait G *g = NULL;
if (t1 == g) {} // warning: 如果 trait 类型不同会报 warning
if (t1 == t2) {} // ok
```



#### 类型转换

trati 类型转换只能在实现了对应 trait 的类型之间进行，将一个类型转换为另一个类型，同时保留原有类型的特性和方法。

**规则：**

1. trait 指针类型的变量，不允许强制转换为非指针类型

2. 支持 trait 指针类型强制转为非 trait 指针类型，但不支持隐式转换

```c
int a = 0;  
trait T *p = &s; // 假设 int 类型实现了 trait T
int *q1 = p; // error: 不支持 trait 指针类型隐式转换为具体类型
int *q2 = (int*)p; // ok
struct S *q3 = (struct S*)p;
```

3.  trait 类型支持强制转换为 `void *`类型，但`void *` 指针无法转换为 trait 指针类型

```c
int a = 1;
trait T *p = &a; // 假设 int 类型实现了 trait T
void * q = (void *)p; // ok: trait 类型支持强制转换为 `void *`类型
trait T *p1 = (trait T *)q; // error: `void *` 指针无法强制转换为 trait 指针类型
trait T *p2 = q; // error: `void *` 指针无法隐式转换为 trait 指针类型
```



### 泛型 trait 语法规则

泛型 trait 是指在 trait 中使用泛型类型参数，从而使 trait 的方法可以适用于不同类型，避免代码的重复编写。下面通过一些代码示例来了解泛型 trait 的使用方法。

#### 泛型 trait 的定义

**语法：**

```c
trait TraitName<T1,T2,...,Tn> {
  // 定义泛型 trait 中的方法签名，可以使用泛型类型 T1,T2,...,Tn
};
```

与 trait 定义类似，`TraitName` 是泛型 trait 的名称，后面跟着一对尖括号，里面可以包含一个或多个泛型参数，在花括号里面可以定义一些方法的签名。同样，泛型 trait 中定义的方法不支持默认实现，必须由实现该泛型 trait 的类型提供具体实现。下面我们来看一个简单的例子：

```c
trait F<T1, T2> {
    T1 doSomeThing1(This* this);
    T1 doSomeThing2(This* this, T2 param);
};
```



**规则：**

1. 泛型 trait 定义只能出现在 top-level

```c
void test<T>() {
    trait F<T> {}; // error: 不能在函数体中定义泛型 trait
}

struct MyStruct<T> {
    trait F<T> {}; // error: 不能在结构体中定义泛型 trait
}
```

2. 泛型 trait 内要求函数首个入参且只有首个入参类型为 `This` 指针，命名为 `this`; `This` 指代实现了 trait 的具体类型

```c
trait F<T> {
    T doSomeThing1(This* this); // ok
    T doSomeThing2(This* a); // error: 第一个参数名必须为 this
    void doSomeThing3(T a, This* this); // error: 第一个参数类型必须为 This
};
```

3. 泛型 trait 内只允许声明函数

```c
trait F<T> {
    void doSomeThing1(This* this) { // error: 泛型 trait 内的成员函数不支持默认实现
        ...
    }
};
```

4. 泛型 trait 内可以没有方法签名

```c
trait F<T> {}; // ok
```

5. 不允许给泛型 trait 类型扩展成员函数

```c
void trait F<T>::getArea(trait F<T>* this) { // error: 不允许给泛型 trait 类型扩展成员函数
	...
}

void trait F<int>::getArea(trait F<int>* this) { // error: 不允许给泛型 trait 类型扩展成员函数
	...
}
```



#### 实现泛型 trait

**语法：**

```c
impl <trait<SpecializationType>> for <type>;
```

我们可以通过 `impl` 关键字对已有 `struct`/`union`/`enum` 类型和内置类型实现 `trait<int>`/`trait<float>`等，需要注意的是，我们目前仅支持对实例化的 trait 类型进行 impl。直观的，我们来看一个简单的例子：

```c
trait F<T> {
    T f(This* this);
};

struct S {};
int struct S::f(struct S* this);
int int::f(int* this);

impl trait F<int> for int;
impl trait F<int> for struct S;
```

**规则：**

1. 在 impl 语句之前，一定存在泛型 `<trait>` 和 `<type>` 的定义
2. 仅支持 `impl` 实例化的 trait 类型

```c
impl trait F<T> for int; // error: 不允许对泛型 trait 类型进行 impl
```

3. 在 impl 语句之前，`<type>` 必须已经声明了 `<trait>` 中所有成员函数

```c
trait F<T> {
    T f1(This* this);
    T f2(This* this);
};

struct S{};

int struct S::f1(struct S* this);
impl trait F<int> for struct S; // error: struct S 未扩展声明函数 f2
int struct S::f2(struct S* this);
impl trait F<int> for struct S; // ok，struct S 已扩展声明了 trait 中所有成员函数
```

4. 支持通过 `impl` 关键字对已有 `struct`/`union`/`enum` 类型和内置类型实现 `trait<int>`/`trait<float>`，但`struct`/`union` 类型类型不能是泛型的

```c
struct S {};
struct G<T> {};
trait F<T> {};
impl trait F<int> for int; // ok
impl trait F<int> for struct S; // ok
impl trait F<int> for struct G<int>; // error: 被 impl 的 struct/union 类型类型不能是泛型的
```

5. `<type>` 类型不允许是 trait/泛型 trait

```c
impl trait F<int>  for trait S; // error: 不允许给泛型 trait 类型实现 trait
impl trait F<int>  for trait F<int>; // error: 不允许给泛型 trait 类型实现泛型 trait
```

6. 支持对 `typedef` 类型实现泛型 trait

```c
typedef struct S S1;
impl trait F<int> for S1;
```



#### 泛型 trait 类型的变量

我们可以定义泛型 trait 实例化后的指针类型变量，并可以通过该指针变量进行函数调用等，具体使用方法如下：

```c
#include <stdio.h>

struct S {};

trait F<T> {
    T foo(This* this);
};
int struct S::foo(struct S* this) {
    return 1;
}
int int::foo(int* this) {
    return 0;
}

impl trait F<int> for struct S;
impl trait F<int> for int;

void test() {
    struct S s;
    int a = 1;
    float b = 1.0;

    trait F<int>* p;
    p = &s; // ok，隐式转换
    p->foo(); // return 1
    p = &a;
    p->foo(); // return 0
    p = &b; // error: float 类型未实现 trait F<int>，不能进行赋值
}
```

**规则：**

1. 只允许声明泛型 trait 实例化后的指针类型变量

```c
trait F<int>* p1; // ok
trait F<int> p2; // error: 不允许定义 trait<int> 类型的变量
```

2. 如果 `<type>` 实现了 `<trait>`，那么指向这个 `<type>` 的指针可以被转换为 `<trait>` 类型的变量

```c
impl trait F<int> for struct S;

struct S s;
trait F<int>* t1 = &s; // 隐式转换
trait F<int>* t2 = (trait F<int>*)&s; // 显式转换
```

3. 泛型 trait 实例化后的指针类型变量可以通过 `->` 方式调用该泛型 trait 中的成员函数

```c
struct S s;
trait F<int>* t = &s;
t->foo();
```

4. 泛型 trait 实例化后的指针类型变量，允许用 `NULL` 赋值

```c
trait F<int>* p = NULL;
```

5. 泛型 trait 实例化后的指针类型变量，不可以解引用

```c
trait F<int> *p;
*p; // error: 泛型 trait 实例化后的指针类型变量，不可以解引用
```

6. 允许指向泛型 trait 的多级指针，但这种类型不能直接调用成员函数。不允许结构体的二级指针直接隐式转换成泛型 trait 的二级指针

```c
trait F<int>* p;
p->foo();
trait F<int>** q = &p; // ok
q->foo(); // error: 多级指针不能直接调用成员函数
(*q)->foo(); // ok
struct S s;
q = &&s // error: 不允许结构体的二级指针直接隐式转换成泛型 trait 的二级指针
```

7. 泛型 trait 实例化后的指针类型变量可以用 `const` / `volatile` 修饰

```c
const trait F<int>* p1;
volatile trait F<int>* p2;
```

8. 支持泛型 trait 实例化后的指针类型作为函数参数及返回值类型

```c
trait F<int>* get(trait F<int>* t) {
    return t;
}
```

9. 支持泛型 trait 实例化后的指针变量和 `NULL` 做比较（这里的比较仅包含 `==` 和`!=`，下同）

```c
trait F<int>* p = NULL;
p == NULL; // ok
p != NULL; // ok
```

10. 支持泛型 trait 实例化后的指针变量和非 trait 指针变量比较

```c
int a = 1;
int *p1 = &a;
struct S s;
struct S *p2 = &s;
trait F<int> *t = NULL;
// 假设struct S 类型实现 trait F<int>，但 int 类型没有
if (t == p1) {}; // warning
if (t == p2) {}; // ok
if (t == a) {}; // error: 与非指针类型比较报 error
```

11. 支持指针变量和 trait 指针变量比较

```c
trait F<int> *t1 = NULL;
trait F<int> *t2 = NULL;
trait F<float> *t3 = NULL;
if (t1 == t2) {} // ok
if (t1 == t3) {} // warning: 如果trait类型不同会报warning
```



#### 类型转换

泛型 trati 类型转换只能在实现了对应泛型 trait 的类型之间进行，将一个类型转换为另一个类型，同时保留原有类型的特性和方法。

**规则：**

1. 泛型 trait 实例化后的指针变量，不允许强制转换为非指针类型

```c
struct S s;
trait F<int> *p = &s; // 假设 struct S 类型实现了 trait F<int>
(struct S)p; // error: 不允许强制转换为非指针类型
```

2. 支持泛型 trait 指针类型强制转为非 trait 指针类型，但不支持隐式转换

```c
struct S s;
trait F<int> *p = &s; // 假设 struct S 类型实现了 trait F<int>
struct S *q1 = p; // error: 不支持泛型 trait 指针类型隐式转换为具体类型
struct S *q2 = (struct S*)p; // ok
int *q3 = (int*)p; // ok
```

3. 泛型 trait 类型支持强制转换为 `void *`类型，但`void *` 指针无法转换为泛型 trait 指针类型

```c
struct S s;
trait F<int> *p = &s;
void * q = (void *)p; // ok: 泛型 trait 类型支持强制转换为 `void *`类型
trait F<int> *p1 = (trait F<int> *)q; // error: `void *` 指针无法强制转换为泛型 trait 指针类型
trait F<int> *p2 = q; // error: `void *` 指针无法隐式转换为泛型 trait 指针类型
```



### 应用

```c
#include <stdio.h>

// 定义 trait
trait Shape {
    int getArea(This* this);
    int getSideLen(This* this);
};

struct Square {
    int side;
};

struct Rectangle {
    int width;
    int hight;
};

int struct Square::getArea(struct Square* this) {
    int area = this->side * this->side;
    printf("the area of this square is %d.\n", area);
    return area;
}

int struct Square::getSideLen(struct Square* this) {
    int length = this->side * 4;
    printf("the side length of this square is %d.\n", length);
    return length;
}

int struct Rectangle::getArea(struct Rectangle* this) {
    int area = this->width * this->hight;
    printf("the area of this rectangle is %d.\n", area);
    return area;
}

int struct Rectangle::getSideLen(struct Rectangle* this) {
    int length = (this->width + this->hight) * 2;
    printf("the side length of this rectangle is %d.\n", length);
    return length;
}

// 为结构体类型实现 trait
impl trait Shape for struct Square;
impl trait Shape for struct Rectangle;

// trait 指针类型作为函数参数及返回值类型
trait Shape* get(trait Shape* s) {
    return s;
}

void test() {
    struct Square s = {.side = 5};
    struct Rectangle r = {.width = 2, .hight = 3};
    trait Shape* shape = &s;
    // trait 指针变量调用方法
    shape->getArea(); // the area of this square is 25.
    // 强制转换
    ((trait Shape*)&s)->getSideLen(); // the side length of this square is 20.
    // 将指针赋值给 trait 指针类型的变量
    shape = &r;
    shape->getArea(); // the area of this rectangle is 6.
    // 隐式转换，将 struct Rectangle* 转为 trait Shape*
    trait Shape* shape2 = get(&r);
    shape2->getSideLen(); // the side length of this rectangle is 10
}
```



------

## 无栈协程

### 无栈协程简介

无栈协程与有栈协程调用栈由程序员显示分配的不同，协程的调用栈由运行时系统隐式管理的，协程本身不持有自己的调用栈，在切换协程时不需要保存和恢复整个调用栈，只保存协程执行状态。它是通过 `async/await` 关键字进行定义和调用。`async` 用来修饰异步函数，`await` 实现异步函数调用。

毕昇 C 的无栈协程目标是支持异步、高并发场景，例如 Web 服务器实现高并发网络通信、异步处理请求、 Web 应用程序的数据库连接池等。

### Future/PollResult标准库定义

#### Future/PollResult定义

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

trait Future<T> {
    struct PollResult<T> poll(This* this);
    void free(This* this);
};
```

#### Future/PollResult使用

协程的实现使用到 Future/PollResult 定义，其导入方式有系统默认导入和用户显式导入两种。

用户显式导入时需要引用头文件 `future.hbs`。文件安装在系统 /usr/include 默认搜索路径下（或者安装在指定目录）在编译时使用 -I 编译选项指定路径。

```
#include  "future.hbs"
```

### 语法规则

1. 无栈协程只允许在毕昇 C 编译单元使用

2. 如果函数中使用了 `await` 关键字，那么这个函数必须用 `async` 修饰。`async` 函数内可以有 0,1...n 个 `await` 表达式

```c
async int TimeOut(int t) {
    return t;
}

async int getData1() {
    int t = await TimeOut(1000);
    return t;
}

async int getData2() {
    await getData1();
    await TimeOut(1000);
    await TimeOut(2000);
    return 0;
}
```

3. `async` 函数声明和实现可以分开

```c
#include "string.h"

async int ReadBuffer(char *str);

async int GetBufferSize() {
    char* Content;
    char ContentCopy[12] = "hello,word!";
    Content = ContentCopy;
    int size = await ReadBuffer(Content);
    return size;
}

async int ReadBuffer(char *str) {
    char *cstr = "hello,word!";
    if (strcmp(str, cstr) == 0)
      return sizeof(str);
    else return 0;
}
```

4. `async` 函数支持递归调用

```c
async int f(int n) {
    if (n == 0 || n == 1)
        return 1;
    int tmp = await f(n-1);
    return n*tmp;
}
```

5. `async` 关键字可以修饰成员函数

```c
async void int::g(int* this);

async int int::f() {
    int i = 1;
    await int::g(&i);
    await i.g();
    return 0;
}

async void int::g(int* this) {
    trait Future<int> a = read(1);
    await a;
}
```

6. `async` 函数中可以出现多个不同或相同 `await` 表达式

```c
async void client1() {
    // client1 send message...
}

async void client2() {
    // client2 send message...
}

async int Server(int start) {
    // server receive message
    await client1();
    if (start < 20)
        await client2();
    return start;
}
```

7. `async` 函数不支持变量数组，即数组中含有变量

```c
async int f() {
    int *VarArray1[n]; // expected-error {{async function does not support VariableArrayType}}
    int VarArray2[3][2][n]; // expected-error {{async function does not support VariableArrayType}}
    int *VarArrayPtr[n][2][n][5]; // expected-error {{async function does not support VariableArrayType}}
    int Array[3]; // support
    int MultiArray[2][3][4][5]; // support
    return 0;
}
```

8. `await` 表达式不能出现在 if/while/for/do-while 等判断条件中

```c
async int read(int n) {
    // read data...
    return n;
}

async int getData() {
    int res = 0;
    if (await read(1)) { // expected-error {{await expression is not allowed to appear in condition statement of if statement}}
        res = await read(1);
    }

    if (res == 2) { // support
        res = await read(1);
    }
    return res;
}
```

9. `await` 表达式不能和“有副作用”的表达式（例如函数）并存

```c
async int read(int n) {
    // read data...
    return n;
}

int test(int a, int b) {
    return 42;
}

async int f() {
    test(await read(2), await read(2)); // expected-error {{await expression is not allowed to appear in function parameters}}
    test(t(), await read(2)); // expected-error {{await expression is not allowed to appear in function parameters}}
    test(3, await read(2)); // support
    return 0;
}
```

10. `await` 表达式不能出现在复合表达式中，例如：表达式中含有 +、-、*、/、%、&、|、>>、<< 等

```c
async int read(int n) {
    // read data...
    return n;
}

async int f() {
    int x = await read(2) + 3; // expected-error {{await expression is not allowed to appear in binary operator expression}}
    int y = await read(2); // support
    return 0;
}
```

11. `await` 表达式支持 `await` 多层嵌套调用

```c
async int test1(int n) {
    // read data...
    return n;
}

async int test1(int n) {
    // ...
    return n;
}

async int test2(int n) {
    // ...
    return n;
}

async int test3(int n1, int n2) {
    // ...
    return 0;
}

async int f() {
    int result1 = await test1(await test1(start));
    int result2 = await test1(await test2(start));
    int result3 = await test3(2, await test1(await test2(start)));
    return result1 + result2 + result3;
}
```

12. `await` 表达式可以出现在 return 语句中

```c
async int read(int n) {
    // read data...
    return n;
}

async int f() {
    return await read(2);
}
```

### 代码样例

```c
# include “future.hbs”
const int MAX = 3;

async int f() {
    int *nptr = NULL;
    int  var[] = {10, 100, 200};
    int  i, *ptr;

    ptr = &var[MAX-1];
    for ( i = MAX; i > 0; i--)
    {
    ptr--;
    }
    int result = await read(1);
    result += *ptr;
    return result;
}

async void g(int start) {
    int result = start;
    for (int i = 0; i< start; i++) {
        int a = await f();
    }
}

int main() {
    trait Future<int> this1 = f();
    this1->poll();
    this1->free();
    // 当 async 函数的返回类型是 void 时，我们需要用 struct Void 类型（会自动创建）来对 trait Future 实例化
    trait Future<struct Void> this2 = g(5);
    this2->poll();
    this2->free();
    return 0;
}
```



------

## 源源变换

### 概述

毕昇 C 支持将毕昇 C 源码转换为标准 C 源码的源源变换能力。该能力由编译选项 `-rewrite-bsc` 提供。

### 使用方式

`-rewrite-bsc` 选项和 clang 原生的 `-rewrite-objc` 选项用法相似。在编译想要被转换的毕昇 C 源码时加上 `-rewrite-bsc` 即可。

例如：如果你想要对 `test.cbs` 进行源源变换，那么在编译的时候使用如下命令即可在当前目录下得到一个名为 `test.c` 的 C 文件，文件的内容即为变换后的 C 源码。

```
clang -rewrite-bsc test.cbs
```

你也可以使用 `-o` 指定输出的 C 文件名，示例如下：

```
clang -rewrite-bsc test.cbs -o a.c
```

也支持同时对多个文件进行变换，不过多个文件同时变换时不能通过 `-o` 指定输出的目录或名称，示例如下：

```
clang -rewrite-bsc boo.cbs foo.cbs
```

需要注意的是：

1. 对于有编译报错的 cbs 文件，使用 -rewrite-bsc 选项会正常编译报错，不会进行源源变换。
2. 对于非 cbs 后缀文件使用 -rewrite-bsc 选项，该选项会被忽略，并报如下 warning 。
```
warning: ignoring '-rewrite-bsc' option because rewriting input type 'c' is not supported [-Woption-ignored]
```



------

## 内存安全

毕昇 C 内存管理的目标是将常见的时间类内存安全问题，如悬挂引用/内存泄漏/重复释放堆内存/解引用空指针等常见的内存安全问题在编译阶段暴露出来。

为此，毕昇 C 引入了所有权和借用两个新的概念。

注：所有权和借用的检查都在 maple ir 上进行，因此需要借助 maple 编译器， 关于 maple 的使用方法见 XXX 。

### 所有权

#### 概述：

大多数时间类内存安全问题都是由多个指针指向同一块内存空间引起的，毕昇 C 所有权的几条基本原则是：

- 同一内存空间在同一时间只能存在一个变量拥有其所有权
- 只有具备所有权才能对内存空间进行释放操作
- 所有权可以通过赋值/函数传参/函数返回进行转移，而所有权被转移后的变量不可以再使用
- 在一个函数内，具有所有权的变量在函数结束前只能通过函数调用转移所有权（包括释放内存）或通过函数返回转移所有权

毕昇 C 通过关键字 `owned` 来声明一个具有所有权的变量，`owned` 作为类型修饰符引入，与 `const/volitile` 等类型修饰符的使用方法基本一致，下面通过一段代码示例来了解所有权系统。

```c
#include "bishengc_safety.h"	//在 bishengc_safety.cbs 中包含对如下函数声明的定义

// 返回一个有所有权的指针
int* owned create();
// 输入一个有所有权的指针，返回一个有所有权的指针
int* owned consume_and_return(int* owned p);

void test() {
    int* owned p = create();	//创建资源，p具备所有权，需要在某个地方转移或者释放
    int* owned p2 = consume_and_return(p1)；	//p通过函数传参的形式转移了所有权，后面不可再使用，否则编译器报错
    free(p2);	//free 的签名为 void free(int* owned), p2的所有权被转移（在free中完成释放），如果没有这个调用，编译器会报错
}
```

#### 语法规则：

1. `owned` 只允许在毕昇 C 编译单元使用

2. owned 使用方法与 `const` 基本一致，允许修饰变量/函数入参/函数返回/struct 成员/多级指针等

3. owned 传染性：包含 `owned` 修饰成员的 `struct` 或者指向 `owned` 单元的指针间接拥有 `owned` 属性

   ```c
   struct A {int* owned a}; // struct A 拥有简介 owned 属性
   void test() {
       int* owned * p;	// p 拥有间接 owned 属性
   }
   ```

4. `owned` 类型或间接拥有 `owned` 属性的类型不能声明为全局变量

   ```c
   struct A {int* owned a};
   struct A a;		// error: struct A 拥有间接 owned 属性，不能声明为全局变量
   int* owned * p;	// error: p 拥有间接 owned 属性，不能声明为全局变量
   ```

5. `owned` 不能修饰 `union` 类型和 `union` 成员，且 `union` 成员不能间接有 `owned` 属性

   ```c
   union A {int a};
   void test() {
   	owned union A a;	// error: owned 不能直接修饰 union 类型
   }
   struct B {int* owned a;}
   union C {
   	int* owned a;	// error: owned 不能修饰 union 成员
   	struct B b; 	// error: union 成员不能间接有 owned 属性，struct B 中包含了 owned
   }
   ```

6. `owned` 类型或间接拥有 `owned` 属性的类型不能作为数组成员

   ```c
   struct A {int* owned a};
   void test() {
       struct A arr1[2];	// error: struct A 拥有间接 owned 属性，不能作为数组成员
       owned int arr2[3];	// error: owned 类型不能作为数组成员
   }
   ```

7. `owned` 修饰的指针或间接拥有 `owned` 属性的指针不支持指针偏移操作（自增/自减/与整数加减/数组索引等）

   ```c
   void test() {
       int* owned p;
       p++;	// error: owned 修饰的指针不支持++操作
       p[3]; 	// error: owned 修饰的指针不支持数组索引操作
   }
   ```

8. `owned` 类型与非 `owned` 类型不允许隐式转换

   ```c
   void test() {
       int* b;
       int* owned a = b;	// error: b 是非 owned 类型，不能转换为 owned 类型
   }
   ```

9. 基础类型一致的情况下允许 `owned` 类型与非 `owned` 类型之间强制转换；基础类型不一致的情况下只允许`void* owned` 和 `T* owned` 之间相互转换

   ```c
   void test() {
       int b;
       owned int a = (owned int)b; 	// 合法，基础类型一致，支持强转
       owned float c = (owned float)b;	// error: 基础类型不一致
       int* owned p1;
       void* owned p2 = (void* owned)p1;	// 合法，允许void* owned 和 T* owned 之间相互转换
   }
   ```

10. 入参或返回类型包含 `owned` 类型或间接 `owned` 类型的函数指针，不支持隐式转换

    ```c
    typedef int (*FTP)(int*, int*);
    typedef int (*FTPO)(int* owned, int*);
    void test() {
        FTP ftp1;
        FTPO ftpo1 = ftp1;	// error: FTPO 的一个入参为 owned 类型， 不能与 FTP 隐式转换
        FTPO ftpo2 = (FTPO)ftp1;	// 合法， 支持强转
    }
    ```

12. 与 `const` 等其它类型修饰符不同，`struct` 成员不能从整体继承 `owned` 属性

    ```c
    strcut A {
        int* owned b;
        int c;
    }
    void test() {
        owned struct A a;
        int* owned num1 = a.b;	// a.b具有 owned 属性
        int num2 = a.c;			// a.c 无 owned 属性， 即使 a 具有 owned 属性，这与 const 是不一样的
    }
    ```



#### 代码样例：

解引用空指针：

```c
void consume(int* owned p) {
    int* owned p1 = p;
    free(p1);
}
 void test() {
     int* owned p1 = (int* owned)malloc(sizeof(int));
     consume(p1);
     int res = *p1;	// error p1 所有权通过函数传参转移了，此处不允许再使用p1
 }
```

内存泄漏：

```c
int* owned consume_and_return(int* owned p) {
    int* owned p1 = p;
    return p1;
}
void test() {
    int* owned p1 = (int* owned)malloc(sizeof(int));
    int* owned p2 = consum_and_return(p1);
    return;
}	//error p2 内存泄漏
```



### 借用









# 附录

## 成员函数
BNF 基于 C11 标准附录 A2.2 修改，对于 `direct-declarator` 新增一条：
```
direct-declarator:
    type-name :: identifier (parameter-type=list)
```

在表达式那里，需要给 `postfix-expression` 新增一条：
```
postfix-expression:
    type-name :: identifier opt-generic-param
```
其中 `opt-generic-param` 是可选的，如果这个成员函数是泛型函数，那么需要带泛型参数，比如 `string::convert_to<int>()` 。

------

## 泛型

### 泛型BNF

```c
template-parameter-list:
	identifier
	template-parameter-list , identifier

template-declaration:
	< template-parameter-list >

// 泛型类型的定义
struct-or-union-specifier:
	struct-or-union identifieropt template-declaration-opt { struct-declaration-list }
	struct-or-union identifier template-declaration-opt

// 泛型函数的定义
direct-declarator:
    identifier template-declaration-opt (parameter-type-list) // 新增
```

### 常量泛型BNF

对于泛型定义处的语法：

```c
template-parameter-list:
        template-parameter-item, template-parameter-list

template-parameter-item:
        type-name identifier
        identifier
```

对于泛型实例化处的语法：

```c
postfix-expression:
    postfix-expression < generic-parameter-list > ( argument-expression-list-opt )

generic-parameter-list:
    generic-parameter-item, generic-parameter-list

generic-parameter-item:
    type-name
    int-literal  // int 字面量
    identifier  // constexpr 常量
    ( constant-expression ) // 常量表达式，必须包含在小括号里面
```



------

## trait
```c
translateion-unit:
    external-declaration
    translation-unit external-declaration

external-declaration:
	function-definition
    trait-definition // 新增
    impl-definition  // 新增
    declaration

trait-definition:
    trait identifier generic-parameters? {
        function-declaration-list;
    };

impl-declaration:
	impl identifier generic-parameters? for type-name generic-parameters? ;
```

------

## 无栈协程


------

## 内存安全

```c
type-qualifier:
    const
    volatile
    _Atomic
    owned  //新增
```

