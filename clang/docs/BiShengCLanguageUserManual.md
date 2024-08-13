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

目前，毕昇 C 已支持**泛型函数**、**泛型结构体**和**泛型类型别名**。

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

- 在声明泛型函数、结构体或类型别名时，泛型参数列表要用尖括号 '<>' 包裹起来，尖括号内部的类型一般是 identifier ，如 'T1', 'T2' 之类 。


- 在实例化时，既可以在尖括号内部传入具体的类型，如 int, float, struct S 等，也可以省略尖括号的书写，编译器会根据实际传入的参数进行隐式的类型推导。此外，如果将现有类型 typedef 为其他别名后，同样可以在实例化时用作泛型实参。


- 同时，毕昇 C 的泛型函数、泛型结构体和泛型类型别名也支持使用**常量参数**作为泛型的形参。

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

#### 泛型类型别名
标准 C 已有类型别名的功能，语法为：
```
typedef OldType NewType;
```
为了使类型别名能够与泛型一起配合使用，BSC 引入与标准 C 语法不同的类型别名语法：
```
typedef NewType = OldType ;
```
加上泛型参数就形成了泛型类型别名，例如：
```
typedef MyPointerType<T> = T*;
```
泛型类型别名的使用可以简化类型的书写，还可以为类型赋予更具描述性的名称，使代码更易于阅读和理解，举例来说：

我们可以使用 HashMap 来记录某年级学生的各科成绩，Key 表示学生的学号，Value 表示学生的各科成绩，根据实际情况的不同，学生的学号可以用 int、string 等类型表示，成绩可以用 int、float 等类型表示，科目数量也可能发生变化，使用泛型类型别名有助于我们根据实际需求进行定制：

```C
//辅助类及成员方法
struct HashMap<K, V> {
   //省略实现
};
void struct HashMap<K, V>::insert(This* this, K key, V value) {
   //省略实现
}
struct Array<T, int N> { 
    T a[N];
};
//使用泛型类型别名定义一个特殊的HashMap类型，它的key是T1类型，value是一个长度为N，元素类型为T2的数组类型：
typedef GenericGrade<T1, T2, int N> = struct HashMap<T1, struct Array<T2, N>>;
//继续使用类型别名，针对不同的年级进行定制：
typedef Grade1 = GenericGrade<int, int, 3>;  //一年级学生学号为int，成绩为int，科目数量为3
typedef Grade3 = GenericGrade<int, float, 4>;//三年级学生学号为int，成绩为float，科目数量为4
typedef Grade6 = GenericGrade<int, float, 5>;//六年级学生学号为int，成绩为float，科目数量为5

int main() {
    Grade1 grade1;
    grade1.insert(10, {80, 90, 95});
    grade1.insert(11, {80, 95, 90});
    Grade3 grade3;
    grade3.insert(12, {90.0, 95.5, 90.0, 85.0});
    grade3.insert(13, {80.0, 90.0, 95.5, 90.0});
    Grade6 grade6;
    grade6.insert(15, {80.0, 90.0, 95.5, 90.0, 85.0});
    grade6.insert(16, {80.0, 90.0, 95.5, 90.0, 85.0});
    return 0;
}
```

对于类型别名，有以下规则：
1. 不允许为泛型类型别名扩展成员函数。
```C
struct S<T> {};
typedef MyS<T> = struct S<T>;

void MyS<T>::foo(This* this) {} //error
```
2. 不允许在结构体类型内部定义类型别名。
```C
struct S<T> {
    typedef type = T;    //error
    typedef Int64 = int; //error
};
```
3. 不允许在函数体内定义泛型类型别名，但可以定义普通类型别名。
```C
void foo<T>() {
    typedef type = T;
    typedef Int64 = int;
    
    typedef MyType1<T> = T;   //error
    typedef MyType2<T1> = T1; //error
}
```

下面是一些用法示例：
```C
// 普通类型别名
typedef Int64 = long int;   //等价于 typedef long int Int64; 
typedef MyS = struct S<int>;//等价于 typedef struct S<int> MyS; 

// 泛型类型别名
typedef MyPointerType<T> = T*;
typedef Array_3<T> = T[3];
typedef Array_N<T, int N> = T[N]; //支持带常量泛型参数

typedef MyS_T_T<T> = struct S<T, T>; 
typedef MyS_T_int<T> = struct S<T, int>;

int main() {
    Int64 a = 5;  //等价于int a = 5;
    MyS s;        //等价于struct S<int> s = 5;
    MyPointerType<int> c = &b;  //等价于int* c = &b;
    Array_3<int> d = {1,2,3};   //等价于int d[3] = {1,2,3};
    Array_N<int, 3> e= {1,2,3}; //等价于int e[3] = {1,2,3};
    MyS_T_T<int> s2;
    MyS_T_int<int> s3;
    return 0;
}
```

##### conditional 泛型类型别名
在 bsc_conditional.hbs 这一 BSC 标准库中提供了 conditional 泛型类型别名，可以实现类型层面的“分支逻辑”：
```
// bsc_conditional.hbs
typedef conditional<int C, T, F> = __conditional(int C, T, F);
```
当 C 非 0 时，conditional 类型别名指代类型 T ，否则指代类型 F，条件表达式必须是可以编译期求值的常量表达式：
```
#include<bsc_conditional.hbs>  //使用conditional需要导入头文件
int main() {
    conditional<1, int, double> a = 1;   //等价于int a = 1;
    conditional<0, int, double> b = 1.0; //等价于double b = 1.0;
    return 0;
}
```
使用conditional，不仅可以简化书写，可以在编译时根据条件选择不同的类型，避免了运行时的条件分支，提高代码的效率，以下是一个关于选择函数返回类型的使用案例：

定义一个泛型函数，它的返回值类型取决于泛型参数 T ，如果 T 是指针类型，则返回值类型仍然是 T ，否则，返回 T 的指针类型。

C++ 中需要借助泛型特化和 concept 来实现：
```cpp
// T 是指针类型时，std::is_pointer_v<T> == true，匹配该版本:
template<typename T> requires std::is_pointer_v<T>  
T foo() { ... }

// T 不是指针类型时，匹配该版本:
template<typename T>                                
T* foo() { ... }
```

在 BSC 中，我们可以借助 conditional 很方便地实现这样的功能，相比C++更加方便易用：
```
typedef PointerType<T> = conditional<is_pointer<T>(), T, T*>;

PointerType<T> foo<T>() { ... } 
```

------
## 常量计算
### constexpr
#### 概述
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
#### 使用规则
##### constexpr 修饰变量
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

##### constexpr 修饰函数
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

### type trait

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

### constexpr if
#### 概述
以 if constexpr 开头的语句被称为 constexpr if 语句，允许用户使用 constexpr 表达式作为 if 语句的条件，保证条件是编译时计算的常量，使得编译器在编译时就能够做分支判断，并对 false 分支进行死代码消除，减少运行时开销。

constexpr if 语句与普通 if 语句的区别在于，cond 条件表达式之前使用 constexpr 关键字进行修饰，即：
```
if constexpr (<cond>) {
    <then-statement>
} else {
    <else-statement>
}
```

我们以计算阶乘为例来认识 constexpr if，先写出一个不使用 constexpr if 的版本来计算阶乘：
```
int factorial(int n) {
    return (n == 1) ? 1 : n * factorial(n - 1);
}

int main() {
    printf("%d", factorial(5));
    return 0;
}
```
由于 BSC 不支持泛型特化的功能，故想要实现阶乘只能使用普通函数，在运行期进行求值。使用 constexpr if，我们可以实现类似于泛型特化的功能，实现编译期计算阶乘结果。
```
constexpr int factorial<int N>() {
    if constexpr (N == 1) {
        return 1;
    } else {
        return N * factorial<N - 1>();
    }
}

int main() {
    _Static_assert(factorial<5>() == 120, "fail");
    return 0;
}
```

#### 用法
constexpr if 语句中的 cond 表达式在使用时需要满足以下约束：
1. cond 表达式必须是可编译时求值的常量表达式；
2. cond 表达式的类型必须是可以隐式转换为 bool 的类型，包括 bool、整型和 char。
例如：
```
int foo1() {
    return 5;
}

constexpr int foo2(int a) {
    return a;
}

int main() {
    int a = 1;
    if constexpr(5.0) { //error,类型不支持浮点类型
        a = 6;
    }
    if constexpr(a) {  //error,不是编译期计算的常量表达式
        a = 6;
    }
    if constexpr(foo1()) {  //error,不是编译期计算的常量表达式
        a = 6;
    }
    if constexpr(foo2(a)) {  //error,不是编译期计算的常量表达式
        a = 6;
    }
    return 0;
}
```

constexpr if 语句中，因为条件表达式是编译时计算的常量表达式，求值为 false 的分支会被定义为 “discarded statement”，会在编译期作为死代码被消除。
例如：
```
if constexpr (<cond>) {  //如果<cond>的值为true
    <true-statement>
} else {
    <false-statement>  //那么else分支内的语句成为discarded statement，会在编译期作为死代码被消除
}
```

对于 discarded statement，有如下规则：
1. 在非泛型上下文内，discarded statement 仍然需要做完整的语法语义检查。
2. 在泛型上下文内，discarded statement 不会被实例化，也就不会进行实例化之后的语义检查。
例如：
```
void foo<T>(T a) {
    if constexpr (is_pointer<T>()) {
    //如果T不是指针，实例化的时候，下面这个block会被当作死代码，不会被实例化，也就不会进行实例化之后的语义检查
        printf("T is pointer\n");
        void* p = (void*) a;
    } else {
        printf("T is a generic case\n");
    }
}

int main() {
    int b = 5;
    foo<int>(5);
    foo<int*>(&b);
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

#### 0. 前言

<!-- C是什么 => 他有什么特性 => 可以怎么样 => 因此相比于其他语言在某些领域不可替代 => 但这些特性导致了内存安全问题 => 内存安全又分为哪几种 => BSC设计了什么特性解决了哪种内存安全问题 -->

C 语言作为一种系统级编程语言，
提供了对指针的高度灵活的直接操作以及利用内存管理函数使开发者手动精细控制和管理内存的能力，
因而被广泛地应用于各种需要直接与硬件或内存等系统资源交互的领域和场景。
然而，这种内存管理模式存在容易导致内存泄漏、释放后使用、空指针解引用、缓冲区溢出和越界读写等内存安全问题。
内存安全问题不仅会造成资源的浪费，也可能导致程序行为错误，甚至导致程序崩溃，对程序的稳定性造成威胁。
内存安全问题可以划分为时间内存安全和空间内存安全两大类，其中时间内存安全包含内存泄漏、释放后使用、空指针解引用等，空间内存安全包括缓冲区溢出、越界读写等。
BiShengC 语言的内存管理主要解决的是程序的时间内存安全问题，利用**所有权特性**在编译期对潜在的内存安全问题进行检查，识别潜在的时间内存安全错误。


#### 1. 特性简介

<!-- BSC有所有权特性是什么 => 语法 => 一句话概括规则 => 简单示例 -->

BiShengC 语言的所有权特性被用于确保程序中的指针及其指向内存空间能被正确地管理。
在 BiShengC 语言中，使用`owned`关键字用来修饰一个指针类型，表明该指针拥有其指向的内存的所有权。
拥有所有权的指针必须确保其指向的内存在指针作用域结束前被显式释放，否则存在潜在的内存泄漏错误；
此外，一块堆内存只能同时被一个`owned`指针所拥有，`owned`指针为移动语义，这样避免了释放后使用等内存安全问题的发生。
以下是一段使用了所有权特性的 BiShengC 语言代码，用于了解所有权特性：

```c
#include "bishengc_safety.h" // BiShengC 语言提供的头文件，用于安全地进行内存分配及释放

int * owned takes_and_gives_back(int * owned p) {
    return p;
}

safe void test(void) {
    int * owned p = safe_malloc(2); // 通过提供的 safe_malloc 申请一块大小为 sizeof(int) 的堆内存，并将值设置为2
    int * owned q = p; // 将 p 指向的堆内存转移给 q，后续不可再使用 p 访问这块内存，否则编译报错
    q = takes_and_gives_back(q); // 通过函数参数转移走 q 的所有权，但通过函数返回值归还所有权
    safe_free((void * owned)q); // 在 q 的作用域结束前调用 safe_free 安全释放堆内存，此处不释放则会报内存泄漏错误
    return;
}
```

注：目前 BiShengC 语言的所有权特性规则检查已经完全转移至 clang 编译器前端完成，不再依赖于原先的 MAPLE IR，因此可直接使用 clang 编译器对 BiShengC 程序进行编译。

注：在安全区，`owned`指针指向的内存一定为通过`safe_malloc`函数申请出的堆内存，`owned`指针不可能指向栈内存。


#### 2. 语法及语义规则

<!-- 引入的语法 => EBNF => 具体规则（包含代码示例） -->

为实现所有权特性，BiShengC 语言引入了`owned`关键字用于修饰指针类型的变量。
该关键字与`const`、`restrict`以及`volatile`均属于类型修饰符，其语法如下：

```
type-qualifier:
  const | restrict | volatile | ownership-qualifier

ownership-qualifier:
  owned
```

具体而言，所有权特性的语法及部分语义规则有以下几点：

1. `owned`关键字仅允许在 BiShengC 语言编译单元内使用；

2. `owned`关键字仅被允许用于修饰指针类型，不允许修饰非指针类型，修饰多级指针时，每级指针的类型修饰可以不一样，规则与`const`类似；

    ```c
    safe void test(void) {
        int * owned p = safe_malloc(2); // 允许
        // int owned a = 2; // 不允许
        double * owned q = safe_malloc(1.1); // 允许
        // double owned b = 1.1; // 不允许
        int * owned * owned pp = safe_malloc(p); // 允许
        double * d = (double *)malloc(sizeof(double));
        double * * owned qq = safe_malloc(d); // 允许
    }
    ```

3. 允许使用`owned`关键字修饰结构体指针及结构体的指针成员；

    ```c
    struct S {
        int m;
        int n;
    };

    struct R {
        int * owned p; // 允许
        double * owned q; // 允许
    }

    safe void test(void) {
        struct S s = { .m = 1, .n = 2 };
        struct S * owned sp = safe_malloc(s1); // 允许
        // struct S owned so = { .m = 1, .n = 2 }; // 不允许
        struct R r = { .p = safe_malloc(1), .q = safe_malloc(2.5) }; // 允许
        struct R * owned rp = safe_malloc(r); // 允许
        // struct R owned ro = { .p = safe_malloc(2), .q = safe_malloc(1.5) }; // 不允许
    }
    ```

4. `owned`不允许修饰`union`类型和`union`的成员，且`union`的每个成员均不能拥有`owned`修饰的成员；

    ```c
    struct S {
        int a;
        int b;
    };

    struct T {
        int * owned p;
        struct S S;
    };

    union A {
        int a;
        // int * owned p; // 不允许
        struct S s;
        // struct S * owned sp; // 不允许
        // struct T t; // 不允许
        // struct T * owned tp; // 不允许
        struct T * trp; // 允许，不跟踪裸指针指向的变量的 owned 成员
    };
    ```

5. `owned`修饰的类型或拥有`owned`修饰的成员的类型不可以作为数组的成员；

    ```c
    struct A {
        int * owned p;
    };

    safe void test(void) {
        // int * owned arr_i[2] = ...; // 不允许
        // struct A arr_a[2] = ...; // 不允许，struct A 拥有 owned 修饰的成员
    }
    ```

6. `owned`修饰的指针不支持算术运算符（指针偏移操作），但支持比较运算符；

    ```c
    safe void test(void) {
        int * owned p = safe_malloc(2);
        int * owned q = safe_malloc(3);
        // p++; // 不允许
        // p[3] = ...; // 不允许
        if (p == q) { // 允许
            ...
        }
    }
    ```

7. `owned`修饰的类型与非`owned`修饰的类型之间不允许隐式类型转换；

    ```c
    safe void test(void) {
        int *b = (int*)malloc(sizeof(int));
        // int * owned p = b; // 不允许 
        int * owned q = safe_malloc(3);
        // int *c = q; // 不允许 
    }
    ```

8. 基本类型一致时，允许`owned`指针类型与非`owned`指针类型之间的显式强制类型转换，且这种转换只能在非安全区进行；
基本类型不一致时，只允许`void * owned`类型与`T * owned`类型之间的相互强制转换；

    ```c
    void test() {
        int *b = (int *)malloc(sizeof(int));
        int * owned p = (int * owned) b; // 允许，b 和 p 的基本类型均为 int
        double * d = (double *)malloc(sizeof(double));
        // int * owned q = (int * owned)d; // 不允许，基本类型不一致
        double * owned dd = safe_malloc(1.5);
        void * owned v = (void * owned)dd; // 允许
    }
    ```

9. `owned`允许修饰指向`trait`的指针，假设有一个具体类型`S`，它实现了`trait T`，则：

    - `S * owned`类型可以隐式转换为`trait T * owned`类型；
    - `trait T * owned`类型允许被显式转换为`void * owned`类型。

10. 通过函数指针调用函数的时候，规则与一般的函数调用一样，会在函数调用时检查形参类型与实参类型是否匹配及返回类型与返回值类型是否匹配；

    ```c
    int add(int *a, int *b);
    typedef int (*FTP)(int *, int *);
    typedef int (*FTOP)(int * owned, int *);

    void test() {
        FTP ftp = add;
        int * owned ap = safe_malloc(1);
        int * owned bp = safe_malloc(2);
        // ftp(ap, bp); // 不允许，类型不匹配
        // FTOP ftop = add; // 不允许，类型不匹配
    }
    ```


#### 3. 所有权状态转移规则

在对所有权特性的语法和部分语义有了解后，本节将对所有权的状态转移规则进行详细阐述。
为更好地理解所有权特性对内存安全带来的保障，首先需要明白程序执行时的堆栈内存模型。

总体而言，程序执行时内存可分为栈区和堆区两个部分，这两部分内存一起为程序在运行时提供内存空间。
栈区即为调用栈，保存的是程序执行所需要维护的所有信息。
每当一次函数调用发生时，就会创建一个对应的栈帧，函数调用的上下文、函数的入参以及函数体内的局部变量就存在这个栈帧中。
栈帧的基址一般由 rbp 寄存器指向，而栈顶由 rsp 寄存器指向，两个寄存器共同标识了一个函数的栈帧。
当一次函数调用结束时，相应的栈帧就会在函数返回前被销毁，相应的内存空间也就得到释放。
这个过程是通过调整 rbp 寄存器的值为调用者的栈帧基址、rsp 的值为调用者的栈顶地址完成的，这也是为什么栈区的变量不需要显式释放。
对于堆区，则存放的是那些在运行时动态分配内存的数据。
一个典型的例子是对于`int *p = malloc(sizeof(int))`这种操作，需要由操作系统在堆区找到一块适合大小的内存空间用于分配，然后将这块内存的地址存在 p 中，指针 p 是程序的一个栈上变量。
虽然堆区的内存分配更为灵活，但却缺乏组织，不正确的内存管理很容易导致堆区的内存泄漏。
例如，当一次函数调用完成后，其局部变量 p 被销毁，而其指向的堆内存未被显式地调用`free(p)`进行回收，则这块堆内存将永远无法被回收，产生了内存安全错误。

<div align="center">
    <img src=./BiShengCLanguageUserManualImages/memory-leak-example.png width=60% />
</div>

利用 BiShengC 语言提供的所有权特性，可以用`owned`关键字对那些需要管理的指针进行标识，这样就可以在程序编译时检查出潜在的错误，避免在运行时出现错误。
以下是 BiShengC 语言所有权的核心规则：

1. 在 BiShengC 语言中每一个值都被一个`owned`指针变量所拥有，该`owned`指针变量即为值的所有者；
2. 一个值同时只能被一个`owned`指针变量所拥有，即一个值只能拥有一个所有者；
3. 当`owned`指针变量离开作用域范围时，需要释放其拥有的值所在的堆内存。

基于以上核心规则，接下来结合详细的代码示例进行具体介绍。

##### 3.1 转移所有权

**1. 一个拥有所有权的变量`s1`被赋值给另一个变量`s2`是一个移动语义，该操作后变量`s1`失去了对值的所有权，原先的变量`s1`无法再使用。**
以下是一段代码示例及说明：

```c
safe void test(void) {
    int * owned p = safe_malloc(10);
    int * owned q = p;
    int * owned m = p; // error
}
```

在这个例子中，`p`拥有一块堆内存的所有权，这块内存大小为`sizeof(int)`，存储的值为10。
在声明`q`时，将`p`的所有权转移给了`q`，`p`不再拥有对这块堆内存的所有权。
则在声明`m`时，便不可再将`p`的所有权转移给`m`，编译器会在此处报错。
因此，利用所有权特性，可以保证一个值只能拥有一个所有者。
（那么如果没有这条规则会出现什么后果呢？三个指针会同时指向一块内存，在作用域结束时对这块内存释放三次，出现重复释放的错误）

**2. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则会全部都转移给`s2`。**
以下是一段代码示例及说明：

```c
struct S {
    int *p;
    int * owned q;
};

safe void test(void) {
    struct S s = {
            .p = (int *)malloc(sizeof(int)),
            .q = safe_malloc(1)
        };
    struct S * owned s1 = safe_malloc(s);
    struct S * owned s2 = s2;
    int * owned p = s1->q; // error
}
```

在这个例子中，将`s1`指向堆内存的所有权转移给了`s2`，但同时`s1`内部还有一个拥有所有权的指针`s1->q`，因此也会一并将他的所有权转移给`s2->q`，后续再使用`s1->q`时便会报错。

**3. 一个拥有所有权的变量`s1`作为整体被赋值给另一个变量`s2`时，如果`s1`内部还有其他拥有所有权的指针，则必须保证内部其他`owned`指针均拥有所有权，才能将`s1`赋值给`s2`。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void test(void) {
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    struct S * owned s1 = safe_malloc(s);
    int * owned p = s1->p;
    struct S * owned s2 = s1; // error
}
```

在这个例子中，我们先将`s1->p`的所有权转移走，然后试图整体转移`s1`的所有权给`s2`，但此时`s1->p`已经不再持有对任何一块堆内存的所有权，因此这个操作是不合法的。

**4. 一个拥有所有权的变量`s1`在失去其所有权后，可以通过赋值的方式使其再次拥有指向某块堆内存的所有权，这样就可以再次使用`s1`。**
以下是一段代码示例及说明：

```c
safe void test(void) {
    int * owned p = safe_malloc(10);
    int * owned q = p;
    p = safe_malloc(4);
    int * owned m = p;
}
```

在这个例子中，`p`的所有权转移给`q`后，再次调用了`safe_malloc`函数为其重新赋予了一块堆内存的所有权，因此后续仍可将`p`的所有权转移给`m`。

**5. 不允许将所有权转移给一个已经拥有所有权的变量。**
以下是一段代码示例及说明：

```c
safe void test(void) {
    int * owned p = safe_malloc(12);
    int * owned q = safe_malloc(67);
    q = p; // error
}
```

在这个例子中，试图将`p`的所有权转移给`q`，但`q`此时已经拥有所有权，再试图转移的话会使`q`原先指向的堆内存泄漏，因此无法进行转移，会在编译时报错。

**6. 如果一个变量`s1`拥有所有权，而其内部的`owned`指针变量的所有权已被转移，如果想再次赋予内部变量所有权，需要保证内部变量的所有父`owned`指针变量均拥有所有权。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void test(void) {
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    struct S * owned s1 = safe_malloc(s);
    struct S * owned s2 = s1;
    s1->p = safe_malloc(5); // error
}
```

在这个例子中，`s1`、`s1->p`以及`s1->q`的所有权均被转移给了`s2`，后续再试图赋予`s1->p`所有权时，其父`owned`变量指针`s1`尚未拥有所有权，因此此次操作是非法的，会在编译时报错。

##### 3.2 作用域结束时的内存释放

**1. 对于所有的`owned`指针变量，会在其词法作用域结束时检查其是否依然拥有堆内存的所有权，如果依然拥有，则存在内存泄漏错误。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void test(void) {
    int * owned p = safe_malloc(2);
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    struct S * owned s1 = safe_malloc(s);
    struct S * owned s2 = s1;
} // error
```

在这个例子中，当作用域结束时，编译器会发现`p`、`s2`、`s2->p`以及`s2->q`依然拥有其指向的堆内存的所有权，即这些堆内存都没有被释放，因此会编译失败并报告内存泄漏。

##### 3.3 强制类型转换

**1. 允许将`T * owned`类型的变量通过强制类型转换转为`void * owned`类型，但转换成功的条件为变量依然拥有所有权且其内部的`owned`指针变量均已不拥有所有权。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void test(void) {
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    struct S * owned s1 = safe_malloc(s);
    int * owned p = s1->p;
    safe_free((void * owned)s1); // error
}
```

在这个例子中，试图将`s1`强制类型转换为`void * owned`类型，但`s1->q`依然拥有所有权，因此转换失败。

**2. BiShengC 语言提供了`safe_free`函数用于释放`owned`指针变量指向的堆内存，该函数的入参要求为`void * owned`类型。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void test(void) {
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    struct S * owned s1 = safe_malloc(s);
    int * owned p = s1->p;
    safe_free((void * owned)p);
    safe_free((void * owned)s1->q);
    safe_free((void * owned)s1);
}
```

在这个例子中，分别释放了`p`、`s1->q`以及`s1`指向的堆内存，不存在任何内存泄漏问题，使用`safe_free`完成了正确的堆内存释放。

##### 3.4 函数调用与返回

**1. 函数调用和返回时，如果函数的形参或函数的返回值为`owned`指针类型，则要求传入的实参以及返回值必须拥有堆内存的所有权。**
以下是一段代码示例及说明：

```c
struct S {
    int * owned p;
    int * owned q;
};

struct S * owned foo(struct S s) {
    struct S * ret = safe_malloc(s);
    return ret;
}

safe void test(void) {
    struct S s = {
            .p = safe_malloc(2),
            .q = safe_malloc(3)
        };
    int * owned p = s.p;
    struct S * owned s1 = foo(s); // error
}
```

在这个例子中，传入`foo`函数的结构体变量`s`内部有两个`owned`指针变量，而`s.p`已经被转移走，因此这次函数调用是非法的，会编译报错。

#### 4. 相关库和 API

##### 4.1 `safe_malloc`

`safe_malloc`是 BiShengC 语言提供的一个安全的内存分配函数。
该函数接收一个泛型类型`T`的变量，表示要分配的内存的大小以及分配后对内存的初始化。
该函数的返回值为`T * owned`类型，即指向分配好的堆内存的`owned`指针。
一些具体的使用例子如下。

在 C 语言中，如果我们需要申请一段堆内存，我们可以使用`malloc`函数进行分配，然后给该内存赋值，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
}
```

然而，这样分配的内存不会在`p`的作用域结束时检查是否调用了`free`进行释放，会造成内存泄漏。
此外，如果再使用一个指针也指向该内存，并在作用域结束时释放，则会出现重复释放的问题，如：

```c
void example() {
    int *p = (int *)malloc(sizeof(int));
    *p = 2;
    int *q = p;
    free(p);
    free(q); // error: double free!
}
```

使用 BiShengC 语言即可解决这种问题，相应的代码如下：

```c
safe void example(void) {
    int * owned p = (int * owned)safe_malloc(2);
    int * owned q = p;
    safe_free((void * owned)q);
}
```

在使用 BiShengC 语言改写后的代码中，如果我们在函数退出前什么都不做，则会出现编译错误`"memory leak of value: `q`"`，避免了内存泄漏问题的发生；
如果我们在函数退出前同时调用`safe_free((void * owned)p)`和`safe_free((void * owned)q)`，则会出现编译错误`"use of moved value: `p`"`，避免了重复释放问题的发生。

那么对于更为复杂的结构体类型，该如何正确使用`safe_malloc`进行内存分配呢？
对于结构体类型，需要首先在栈上构造出相应的变量，然后传给`safe_malloc`在堆上完成相应内存的分配，以下代码为具体示例：

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void example(void) {
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * owned sp = safe_malloc(s);
    ...
}
```

##### 4.2 `safe_free`

`safe_free`是 BiShengC 语言提供的一个安全的内存释放函数。
该函数接收一个`void * owned`类型的指针，表示要释放的内存的地址。
该函数的返回值为`void`类型。
因此，在调用`safe_free`进行释放前需要将`owned`指针显式地强制转换为`void * owned`类型，具体的转换规则可参考 [3.3](#33-强制类型转换) 节。
一些具体的使用例子如下。

```c
struct S {
    int * owned p;
    int * owned q;
};

safe void example(void) {
    int * owned pa = safe_malloc(199);
    struct S s = { .p = safe_malloc(1), .q = safe_malloc(2) };
    struct S * owned sp = safe_malloc(s);
    safe_free((void * owned)pa);
    safe_free((void * owned)sp->p);
    safe_free((void * owned)sp->q);
    safe_free((void * owned)sp); // 必须先释放 sp->p 和 sp->q，才能释放 sp
}
```

##### 4.3 TODO

#### 5. 源源变换

BiShengC 语言的 clang 编译器支持源源变换功能，即将`.cbs`文件转换为等价的`.c`文件。
所有权特性仅引入了`owned`关键字表示所有权，在源源变换时只会去掉所有的`owned`关键字，然后生成相应的`.c`代码。
关于源源变换的详细细节，请参考手册的源源变换章节。


#### 6. 代码示例

以下代码为利用 BiShengC 语言的所有权特性实现的一段代码，其功能为...TODO



### 借用
在编程时，我们常常需要表达“对某个资源进行借用”的概念，区别于“拥有某个资源”。前面小节描述了所有权特性(ownership)，对某个资源拥有所有权的主体，有责任释放这个资源。这个小节，将介绍对资源的借用(borrow)，对于借用，我们有几个原则：
* 任何一个对资源的借用，都不能比资源的所有者的生命周期长
* 借用分为可变借用、不可变借用；可以同时存在多个不可变借用，但最多只能有一个可变借用

毕昇C为表达借用的概念：1、类型定义处引入新关键字 borrow 修饰类型 T 表示一个借用类型 2、区别于标准C的取地址操作符 `&` 引入借用操作符  `&mut` 和 `&const`，表示可变借用和只读借用

#### 借用类型和借用操作符

#### 借用类型的定义、赋值等规则

#### 其它

#### BNF 设计

### 安全区

#### 概述：

c 语言有很多规则过于灵活，不方便编译器做静态检查。因此我们引入一个新语法，使得在一定范围内的毕昇 c 代码必须遵循更严格的约束，保证在这个范围内的代码肯定不会出现“内存安全”问题。

允许用 safe/unsafe 关键字修饰一个代码块或者一个函数。

​    unsafe 表示这段代码在非安全区，这部分代码遵循标准 c 的规定，同时这部分代码的安全性由用户保证。

​    safe 表示这段代码在安全区，这部分代码必须遵循更严格的约束，同时编译器可以保证内存安全。

​    没有 safe/unsafe 关键字修饰的全局函数默认是非安全的。

#### 代码示例：

```c
#include <stdlib.h>

typedef struct File{
} FileID;

FileID* owned create(void) {
  FileID *p = malloc(sizeof(FileID));
  return (FileID* owned)p;
}

FileID* owned consume_and_return(FileID* owned p) {
  return p;
}
// 使用 safe 修饰函数，表示该函数为安全函数，函数内为安全区
safe void safe_free(FileID* owned p) {
    // 使用 unsafe 修饰代码块，表示这段代码在非安全区。这段代码是在安全区内的非安全区，也属于非安全区
    unsafe {free((FileID*)p);}
}

int main(void) {
  FileID* owned p1 = create();
  FileID* owned p2 = consume_and_return(p1);
  // 使用 safe 修饰代码块，表示这段代码在安全区
  safe {
  	safe_free(p2);
  }
  return 0;
}
```



#### 语法规则：

1、允许使用 safe/unsafe 修饰代码块、函数声明、函数签名、函数定义。

```c
// 修饰函数签名
safe int test(int, int);
// 修饰函数定义
safe int test(int a, int b);
// 修饰函数声明
safe int test(int a, int b) {
	return a + b;
}
// 修饰函数指针
safe int (*p)(int, int);

int main() {
    safe {  // 修饰代码块
        int a = 1;
    }
    unsafe {
        int b = 1;
    }
    return 0;
}
```

2、不允许使用 safe/unsafe 修饰全局变量、类型声明、typedef 声明（允许修饰函数指针)、static_assert。

```c
#include <assert.h>

safe int g_a; // error: 不允许修饰全局变量
safe struct b {   // error: 不允许修饰类型声明
    int a;
};
safe typedef int mm; // error: 不允许修饰 typedef
safe typedef int (*p)(int a); // ok: 允许修饰函数指针
int main() {
    safe int a;  // error: 不允许修饰类型声明
    safe static_assert(1, "error"); // error: 不允许修饰 static_assert
    return 0;
}
```

3、safe 修饰的函数，参数类型和返回类型必须是 safe 类型。

非 safe 类型包括：裸指针类型、union 类型、成员中包含不安全类型的 struct 类型、成员中包含不安全类型的数组类型。

```c
safe int* test1(int a);  // error: 返回值为非安全类型的裸指针类型
safe int test2(int *a);  // error: 参数类型为非安全类型的裸指针类型

typedef struct F {
    int* a;
} SF;
safe SF test3(int a); // error: 返回值为成员中包含不安全裸指针类型的 struct 类型
safe int test4(SF b); // error: 参数类型为成员中包含不安全裸指针类型的 struct 类型
```

4、safe 修饰的函数，函数参数列表不可以省略。`safe void test(); `是不允许的， `safe void test(void); `是允许的。

5、safe 修饰的函数，函数参数列表不可以包含变长参数。`safe int test(int a,  ...); `是不允许的。

6、如果 trait 中的函数被声明为 safe，那么要求实现 trait 的类型的对应成员函数也必须是 safe 修饰的函数。若 trait 中的函数未声明为 safe，也允许实现 trait 中的类型的成员函数为 safe。

```c
trait G {
    safe int* owned test1(This* owned this);
    int* owned test2(This* owned this);
};
safe int* owned int::test1(int* owned this) {
    return this;
}
safe int* owned int::test2(int* owned this) {
    return this;
}
impl trait G for int;
```

 7、多个同名函数声明必须有同样的 safe/unsafe 修饰。

```c
safe int test(int a);
unsafe int test(int a); // error: 多个函数声明中 safe/unsafe 不一致
```

8、多个同名函数声明排除 safe/unsafe 修饰后，是完全一致的。

```c
safe int test(int a);
safe int test(int a, int b); // error: 函数声明不完全一致
```

9、safe 修饰泛型函数时，会对泛型每个实例化版本也做 safe 检查。

```c
safe T test<T>(T a) {
    return a;
}

void test2() {
    int a = 1;
    int b = test<int>(a);
    int* owned c = (int* owned)&a;
    int* owned d = test<int* owned>(c);
    int* e = test<void*>((void*)0);  // error: 实列化函数入参和返回值为非安全的裸指针类型
}
```

10、成员函数也可以被 safe/unsafe 修饰，其规则和全局函数一样。

```c
struct MyStruct<T> {
    T res;
};
safe T struct MyStruct<T>::foo_a(T a) {
    return a;
}
```

11、对于 safe 修饰的函数指针类型，与赋值的函数参数和返回值类型必须是一致的。

```c
safe void test1(int a) {}
safe void test2(void) {}
int main() {
	safe void (*p)(int a);
	p = test1; // ok
	p = test2; // error: 参数类型不一致，不允许赋值
}
```

12、安全区内被调用的函数或函数指针必须是 safe 的函数签名，不允许调用非安全函数或函数指针。

```c
safe void test1(void) {}
unsafe void test2(void) {}
safe void (*test3)(void);
unsafe void (*test4)(void);
int main() {
    safe {
        test1();
        test2(); // error: 安全区内不允许调用非安全函数
        test3();
        test4(); // error: 安全区内不允许调用非安全函数指针
    }
    unsafe {
        test1();
        test2();
        test3();
        test4();
    }
}
```

13、安全区内允许再包含 unsafe 修饰的代码块、非安全区内也允许再包含 safe 修饰的代码块。

```c
int test1(int a, int b) {
    return a + b;
}
safe int test2(int a, int b) {
    return a > b ? a : b;
}
int main() {
    safe {
        int a = 0;
        unsafe {
            a = test1(1, 3);
            safe {
                a = test2(3, 5);
            }
        }
    }
}
```

14、安全区内不允许使用标签和 goto 语句。

```c
void test(int a) {
    safe {
        if (a < 10) {
            goto error; // error: 安全区不允许使用 goto 语句
        }
        error:  // error: 安全区内不允许使用标签
        return;
    }
}
```

15、安全区内不允许无初始化或初始化不完整的变量声明。

```c
struct F {
	int age;
	char name[20];
};
void test() {
	safe {
		int a; // error: 安全区内不允许无初始化的变量声明
		struct F tom = {10}; // error：安全区不允许部分初始化
		struct F tony = {10, "tony"};
	}
}
```

16、安全区内 switch 语句中的 case/default 只能存在于 switch 后面的第一层代码块中，且第一层代码块不允许有变量定义。

```c
safe void test(int a) {
    switch (a) {
    	int b = 10;  // error: 第一层代码块不允许有变量定义
        case 0: {
            int c = 1;  // ok
            break;
        }
        {
            case 1: {    // error: case 只能存在于 switch 后面的第一层代码块中
                break;
            }
        }
        {
            default:{   // error: default 只能存在于 switch 后面的第一层代码块中
                break;
            }
        }
    }
}
```

17、安全区内不允许使用自增 （++）、自减（--）操作符，不允许 union 类型通过“.”访问成员，不允许裸指针通过“->”访问成员。

```c
union un {
    int age;
    char name[16];
};
struct F {
    int age;
};

void test(void) {
    struct F d = {10};
    struct F *e = &d;
    struct F* owned f = (struct F* owned)&d;
    safe {
        int a = 1;
        a++;    // error: 安全区不允许自增
        a--;    // error: 安全区不允许自减
        union un b = {10};
        int c = b.age;   // error: 安全区不允许 union 通过“.”访问成员
        int g = e->age;  // error: 安全区不允许裸指针通过“->”访问成员
        int h = f->age;  // ok
    }
}
```

18、安全区内不允许使用取地址符“&”（允许对函数取地址），不允许解引用裸指针类型。

```c
void test() {
	safe {
		int a = 10;
		int *b = &a;  // error: 安全区不允许取地址符号
		int c = *b;	  // error: 安全区不允许解引用裸指针 
	}
}
```

19、安全区内不允许指向类型不同的指针类型之间转换，不允许指针和非指针类型之间的转换，不允许 owned/raw 指针之间的转换。

```c
void test() {
	int *a;
	double *b;
	safe {
		b = a;  // error：不允许指向类型不同的指针类型之间转换
		a = b;  // error：不允许指向类型不同的指针类型之间转换
		b = (double *)a;  // error：不允许指向类型不同的指针类型之间转换
	}
	int c;
	safe {
		a = c;  // error：不允许指针和非指针类型之间的转换
		c = a;  // error：不允许指针和非指针类型之间的转换
	}
	int* owned d = (int* owned)&c;
	safe {
		a = d;  // error：不允许 owned/raw 指针之间的转换
		d = a;  // error：不允许 owned/raw 指针之间的转换
	}
}
```

20、安全区内不允许表达范围从大向小的类型转换（比如从 long 转换为 int，从 int 转换为 _Bool，从 int 转换为 enum）。不允许表达精度从高向低的类型转换（比如从 double 转换为 float）。对于基础类型的常量发生类型转换，如果目标类型可以描述这个值，那么该类型转换是允许的。

```c
void test() {
    long a;
    int b;
    _Bool c;
    double e;
    float f;
    safe {
        a = b;    // ok
        b = a;    // error: 不允许表达范围从大向小的类型转换
        a = 1;    // ok
        c = 1;    // ok
        c = 2;    // error: 目标类型不可以描述这个值，不允许转换
        e = f;    // ok
        f = e;    // error：不允许表达精度从高向低的类型转换
        f = 1.0;  // ok
        f = 1.2f; // ok
    }
}
```

21、安全区内不允许内嵌汇编语句。

```c
void test() {
    safe {
        int ret = 0;
        int src = 1;
        asm("move %0, %1\n\t" :"=r"(ret) :"r"(src)); // error: 安全区不允许内嵌汇编
    }
}
```

## owned struct 类型
`owned struct` 是一种自定义类型，与 `struct` 不同，主要体现在非拷贝语义上，一律是 `move` 语义整体跟踪。这意味着原变量中的资源所有权会转移到新变量或参数中。本节依次介绍如何定义 `owned struct` 类型，如何创建 `owned struct` 实例。

### 定义 `owned struct` 类型
`owned struct` 类型的定义由关键字 `owned struct` 和自定义的名字组成，紧跟着是定义在一对花括号中 `owned struct` 定义体，在定义体中可以定义成员变量、析构函数、成员函数和访问修饰符。允许定义泛型 `owned struct`。

```
owned struct Person{
public:
    char name[50];
    int age;
    char* getName(Person *this) {
        return this->name;
    }
    int getAge(Person *this) {
        return this->age;
    }
    ~Person(Person this) {
        printf("Name = %s\n", this.getName());
        printf("Age = %d\n", this.getAge());
    }
};
```
上例定义了名为 `Person` 的 `owned struct` 类型，它有两个成员变量 `name` 和 `age`、成员函数`getName` 和 `getAge`、以及析构函数 `~Person`。

注：`owned struct` 与 `C struct`类似，允许结构体嵌套。
#### 成员变量

`owned struct` 的成员变量分为实例成员变量和静态成员变量(`static` 修饰)，和 `struct` 一样，成员变量定义的时候不允许写初始化。
```
owned struct Person{
public:
    char name[50];
    static int age;
};
int Person::age = 20;
```

#### 析构函数
在一个对象生命周期结束时，会自动调用析构函数。析构函数与 `owned struct` 类型同名，前面带有 `~`, 但是不允许写泛型参数。当用户未定义析构函数时，编译器会自动提供默认的析构函数。
```
owned struct S {
   ~S(S this) {
    printf("S destructor\n");
   }
};
```
析构函数的使用有以下限制：

+ 一个 `owned struct` 最多只能有一个析构函数。
+ 不允许作为扩展定义，不允许写返回类型，参数只有一个 `owned struct` 类型的 `this`, 不允许用户主动调用。
+ 析构函数内部需要将 `this` 的所有 `owned` 成员释放，规则同 `ownership` 的检查规则。需要注意的是如果 `this` 的成员是 `owned struct`类型，那么成员的释放是由编译器自动添加的。
+ 全局变量的析构函数不会被调用，包括函数内定义的 `static` 变量。
+ 局部变量的析构函数, 如果在本函数内没有被 `move` 走，析构的顺序为先定义的变量后析构。
+ 临时变量是不支持析构的，会报内存泄漏。
+ 自定义类型内部的成员带有析构函数的时候，析构函数调用顺序：
  + 先调用外层类型的析构函数，再调用成员变量的析构函数
  + 不保证各个成员的析构函数的调用顺序
  
#### 成员函数
`owned struct` 的成员函数分为实例成员函数和静态成员函数(不允许 `static` 修饰)。实例成员函数需要显式参数 `this`, 假设当前 `owned struct` 类型是 `C`, `this` 的类型可以是 `C*`、`const C*`、`C* borrow`、`const C * borrow`、`C * owned`。静态成员函数是没有 `this`。成员函数的访问与 `struct` 扩展成员函数访问一样(详见成员函数章节)。

```
owned struct Person{
public:
    char name[50];
    int age;
    char* getName(Person *this) {
        return this->name;
    }
    char* getTypeName(){
        return "Preson";
    }
};
```

上例是 `owned struct` 定义体内部实例成员函数 `getName` 和静态成员函数 `getTypeName`。

`owned struct` 定义体内部可以只有函数声明，把函数体外置。等同于 `owned struct` 定义体内部的成员函数。
示例：
```
owned struct Person{
public:
    char name[50];
    int age;
    char* getName(Person *this);
    char* getTypeName();

};
char* Person::getName(Person *this) {
    return this->name;
}
char* Person::getTypeName(){
    return "Preson";
}
```
`owned struct` 与 `struct` 类似，允许扩展成员函数（详见成员函数章节）。

#### 访问控制控制权限
`owned struct` 定义体内，允许成员指定可见性分为 `public` 和 `private`, 默认为 `private`。只有 `owned struct` 定义体内部的成员函数有权访问 `private` 和 `public`成员，在`owned struct` 外部（包括扩展成员函数）, 只能访问 `public` 成员。示例：

```
owned struct A{
    private:
    int a;
    public:
    int b ;
};
int A::f(A* this) {
    this->a; // error
    this->b; // ok
    return 0;
}
```

### 创建 `owned struct`实例
`owned struct` 允许使用 `struct initializer` 语法创建实例，也允许单独对每个成员变量初始化（如果成员变量是 `public`, 此时与 `struct` 一样单独跟踪每个成员的初始化状态，但是需要在安全区状态下保证在发生 `move`、传参、析构和返回等场景下该变量一定已经完整初始化, 非安全区不做保证。同时为了方便 `owned struct` 类型在使用的时候不携带 `owned struct` 关键字。



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

