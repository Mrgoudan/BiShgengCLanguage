# 调试

支持毕昇c的基础调试功能；源源变换到标准c后，依旧支持对源文件的调试能力，方便程序调优与问题定位。

## 直接调试毕昇c源文件

这里的调试主要指gdb调试，能够设置断点，打印基础变量的值。标准c原有的类型，支持正确打印变量的值，毕昇c新增类型，如 _Owned struct，泛型等，调试时也能正确打印。

下面是一个例子：

```c
_Owned struct Person<T>{
_Public:
    char name[50];
    T age;
    ~Person(This this) {
    }
};

char* Person<T>::getName(This *this) {
    return this->name;
}

int Person<T>::getAge(This *this) {
    T a = this->age;
    return a;
}

int main() {
    Person<int> per = {"davi", 21};
    char* name = per.getName();
    int age = per.getAge();
    return 0;
}
```

完整的调试过程如下：

- 生成带调试信息的可执行文件：

  ```shell
  clang -g debug_demo.cbs -o debug_demo_cbs
  ```

  **注意：** clang默认生成gdwarf-5格式的调试文件，安装的gdb版本需不低于10.1，否则存在兼容性问题。另外，也可以加上`-gdwarf-4`编译选项，生成兼容性更好的gdwarf-4格式的调试文件：

  ```shell
  clang -g -gdwarf-4 debug_demo.cbs -o debug_demo_cbs
  ```

- 运行`gdb debug_demo_cbs`，进入gdb调试界面。

- 下面是一些常用的调试命令：

  输入`b main`，设置断点，输入`run`，运行程序，程序运行到断点处，输入`info locals`，查看局部变量的值，输入`p var`，打印变量`var`的值，输入`s`或`n`，步进下一行。

在上面这个调试用例中，记录了一些调试过程信息，如下：

  ```shell
  (gdb) b main
  Breakpoint 1 at 0x1162: file debug_demo.cbs, line 19.
  (gdb) r
  Starting program: /path/to/debug_demo 

  Breakpoint 1, main () at debug_demo.cbs:19
  19    Person<int> per = {"davi", 21};
  (gdb) n
  20    char* name = per.getName();
  (gdb) p per
  $1 = {name = "davi", '\000' <repeats 45 times>, age = 21}
  (gdb) s
  _ZN_ZTS6PersonIiEgetName (this=0x7fffffffdce0) at debug_demo.cbs:10
  10        return this->name;
  (gdb) s
  main () at debug_demo.cbs:21
  21    int age = per.getAge();
  (gdb) s
  _ZN_ZTS6PersonIiEgetAge (this=0x7fffffffdce0) at debug_demo.cbs:14
  14        T a = this->age;
  (gdb) s
  15        return a;
  (gdb) p a
  $2 = 21
  (gdb) s
  _ZN_ZTS6PersonIiE~Person (this=...) at debug_demo.cbs:5
  5    ~Person(This this) {
  ...
  ```

  在调试中，`_Owned struct`类型的`per` 以及泛型`a` 的值被正确打印出来了。

  **注意：** 函数名称显示为诸如`_ZN_ZTS6PersonIiEgetName`，这是经过编译器名称修饰(`Mangle`)之后的，与原始名称存在差异。

## 源源变换后调试毕昇c源文件

将毕昇c源源变换为标准c后，可以直接调试生成的c文件。由于标准c与毕昇c间的关联信息缺失，调试中无法建立其与原始毕昇c之间的映射关系，调试友好性欠佳。在此背景下，我们建立了源源变换后c文件与原cbs文件之间的行号映射关系，从而支持源源变换后对毕昇c源文件的调试能力。

**注意：** 直接的调试对象仍是变换后的c文件，但调试过程中显示的是变换前的cbs文件，本质是建立了一定程度的行号映射关系。

### 单文件调试

仍以上一小节的cbs文件为例，尝试帮助开发者更好地理解这个调试特性。

完整的调试过程如下：

- 源源变换生成带行号映射关系的标准c文件：

  ```shell
  clang -rewrite-bsc -line debug_demo.cbs -o debug_demo.c
  ```

- 查看生成的c文件：

  ```c
  struct Person_int {
      char name[50];
      int age;
  };
  
  static char *struct_Person_int_getName( struct Person_int *this);
  
  static int struct_Person_int_getAge( struct Person_int *this);
  
  static void struct_Person_int_D( struct Person_int this);
  
  #line 18 "debug_demo.cbs"
  int main(void) {
      struct Person_int per = {"davi", 21};
      _Bool per_is_moved = 0;
      char *name = struct_Person_int_getName(&per);
      int age = struct_Person_int_getAge(&per);
      if (!per_is_moved)
          struct_Person_int_D(per);
      return 0;
  }
  
  #line 9 "debug_demo.cbs"
  static char *struct_Person_int_getName( struct Person_int *this) {
      return this->name;
  }
  
  #line 13 "debug_demo.cbs"
  static int struct_Person_int_getAge( struct Person_int *this) {
      int a = this->age;
      return a;
   }
  
  #line 5 "debug_demo.cbs"
  static void struct_Person_int_D( struct Person_int this) {
      _Bool this_is_moved = 0;
  }
  
  ```

  观察生成的c文件，可以发现，每段函数代码块前都插入了其在原始cbs文件中对应部分的开始行号，格式为

  ```c
  #line raw_number "debug_demo.cbs"
  ```

  其中 `raw_number`为起始行号，`"debug_demo.cbs"`为相对路径下原始cbs文件名。

- 生成带调试信息的可执行文件：

  ```shell
  clang -g debug_demo.c -o debug_demo_c
  ```

- 运行`gdb debug_demo_c`，进入gdb调试界面。

  此时，`gdb` 交互界面显示的是原始cbs文件中的内容和行号，而不是变换后的c文件中的行号。借助`gdb`的常用调试命令，可以在毕昇c原始代码中跳转，并打印一些变量值。
  
  **注意：** 源源变换时若采用相对路径，在调试时需要控制在同一相对路径下；若采用绝对路径，则无此问题。

- 记录了一些调试过程信息，如下：

  ```shell
  (gdb) b main
  Breakpoint 1 at 0x1152: file /path/to/debug_demo.cbs, line 19.
  (gdb) r
  Starting program: /path/to/debug_demo_c
  
  Breakpoint 1, main () at /path/to/debug_demo.cbs:19
  19    Person<int> per = {"davi", 21};
  (gdb) s
  20    char* name = per.getName();
  (gdb) s
  21    int age = per.getAge();
  (gdb) s
  struct_Person_int_getName (this=0x7fffffffdcf0) at /path/to/debug_demo.cbs:10
  10        return this->name;
  (gdb) s
  main () at /path/to/debug_demo.cbs:22
  22    return 0;
  (gdb) s
  struct_Person_int_getAge (this=0x7fffffffdcf0) at /path/to/debug_demo.cbs:14
  14        T a = this->age;
  (gdb) s
  15        return a;
  (gdb) info locals
  a = 21
  (gdb) s
  main () at /path/to/debug_demo.cbs:23
  23    }
  (gdb) s
  24
  (gdb) info locals
  per = {name = "davi", '\000' <repeats 45 times>, age = 21}
  per_is_moved = false
  name = 0x7fffffffdcf0 "davi"
  age = 21
  (gdb) s
  struct_Person_int_D (this=...) at /path/to/debug_demo.cbs:6
  6    }
  ...
  ```

### 多文件调试

当存在多文件引用时，调试时也能在多文件间正确跳转。例如，存在一个包含基本数学运算函数的源文件`basic_math.cbs`及其头文件`basic_math.hbs`，在`cacl_demo.cbs`中调用`basic_math`中的函数。

- 源文件

  `basic_math.cbs`

  ```c
  #include "basic_math.hbs"
  
  int struct MyStruct::add(int a, int b){
      int res = a + b;
      return res;
  }
  
  int struct MyStruct::subtract(int a, int b) {
      int res = a - b;
      return res;
  }
  
  int struct MyStruct::multiply(int a, int b) {
      int res = a * b;
      return res;
  }
  
  int struct MyStruct::divide(int a, int b) {
      if (b != 0) {
          return a/b;
      } else {
          return 0;
      }
  }
  ```

  `basic_math.hbs`

  ```c
  #ifndef REWRITE_H
  #define REWRITE_H
  
  struct MyStruct {
      int a;
      int b;
  };
  
  int struct MyStruct::add(int a, int b);
  int struct MyStruct::subtract(int a, int b);
  int struct MyStruct::multiply(int a, int b);
  int struct MyStruct::divide(int a, int b);
  
  T min<T>(T a, T b) {
      return a > b ? b : a;
  }
  
  T max<T>(T a, T b) {
      return a > b ? a : b;
  }
  
  #endif
  ```

  `cacl_demo.cbs`

  ```c
  #include "basic_math.hbs"
  
  int main() {
      struct MyStruct s = {4, 2};
      int r1 = struct MyStruct::add(s.a, s.b);
      int r2 = struct MyStruct::subtract(s.a, s.b);
      int r3 = struct MyStruct::multiply(s.a, s.b);
      int r4 = struct MyStruct::divide(s.a, s.b);
      int c1 = max<int>(s.a, s.b);
      int c2 = min<int>(s.a, s.b);
      return 0;
  }
  ```

- 源源变换

  ```shell
  clang -rewrite-bsc -line basic_math.cbs basic_math.hbs calc_demo.cbs
  ```

  变换后，生成了`basic_math.c`、`basic_math.h`与`calc_demo.c`，分别如下：
  
  `basic_math.c`

  ```c
  #include "basic_math.h"
  
  #line 3 "basic_math.cbs"
  int struct_MyStruct_add(int a, int b) {
      int res = a + b;
      return res;
  }
  
  #line 8 "basic_math.cbs"
  int struct_MyStruct_subtract(int a, int b) {
      int res = a - b;
      return res;
  }
  
  #line 13 "basic_math.cbs"
  int struct_MyStruct_multiply(int a, int b) {
      int res = a * b;
      return res;
  }
  
  #line 18 "basic_math.cbs"
  int struct_MyStruct_divide(int a, int b) {
      if (b != 0) {
          return a / b;
      } else {
          return 0;
      }   
  }
  ```

  `basic_math.h`

  ```c
  #ifndef REWRITE_H
  #define REWRITE_H
  
  struct MyStruct {
      int a;
      int b;
  };
  
  #line 9 "basic_math.hbs"
  int struct_MyStruct_add(int a, int b);
  
  #line 10 "basic_math.hbs"
  int struct_MyStruct_subtract(int a, int b);
  
  #line 11 "basic_math.hbs"
  int struct_MyStruct_multiply(int a, int b);
  
  #line 12 "basic_math.hbs"
  int struct_MyStruct_divide(int a, int b);
  #endif
  ```

  `calc_demo.c`

  ```c
  #include "basic_math.h"
  
  static int min_int(int a, int b); 
  
  static int max_int(int a, int b); 
  
  #line 3 "calc_demo.cbs"
  int main(void) {
      struct MyStruct s = {4, 2}; 
      int r1 = struct_MyStruct_add(s.a, s.b);
      int r2 = struct_MyStruct_subtract(s.a, s.b);
      int r3 = struct_MyStruct_multiply(s.a, s.b);
      int r4 = struct_MyStruct_divide(s.a, s.b);
      int c1 = max_int(s.a, s.b);
      int c2 = min_int(s.a, s.b);
      return 0;
  }
  
  #line 14 "./basic_math.hbs"
  static int min_int(int a, int b) {
      return a > b ? b : a;
  }
  
  #line 18 "./basic_math.hbs"
  static int max_int(int a, int b) {
      return a > b ? a : b;
  }
  ```

- 生成调试信息

  ```shell
  clang -g calc_demo.c basic_math.c -o calc_demo_c
  ```

- gdb调试

  ```shell
  gdb calc_demo_c
  ```

  调试中，可以正常跳转到原始`cbs`和`hbs`文件，部分调试过程示例如下：

  ```shell
  Breakpoint 1, main () at /path/go/calc_demo.cbs:4
  4    struct MyStruct s = {4, 2};
  (gdb) n
  5    int r1 = struct MyStruct::add(s.a, s.b);
  (gdb) s
  struct_MyStruct_add (a=4, b=2) at /path/go/basic_math.cbs:4
  4    int res = a + b;
  (gdb) s
  5    return res;
  (gdb) p res
  $1 = 6
  (gdb) c
  Continuing.
  
  Breakpoint 2, max_int (a=4, b=2) at ./basic_math.hbs:19
  19    return a > b ? a : b;
  ...
  ```

  该调试特性支持的功能与问题，汇总如下：
- 调试的对象是变换后的标准c，不改其运行逻辑、变量信息等。
- 调试中显示的代码位置指向原始cbs文件，支持多源文件的调试跳转。
- 当源源变换前后代码行数存在差异、无法逐行映射时，例如 `_Owned struct` 析构函数、 `_Trait` 等会生成新代码的特性，显示的调试位置可能不准确，需要开发者注意。
