# Debugging

BiSheng C supports basic debugging functionality. After source-to-source translation to standard C, debugging of the source files is still supported, making it convenient to tune programs and locate problems.

## Debugging BiSheng C source files directly

Debugging here mainly refers to gdb debugging, which can set breakpoints and print the values of basic variables. For types that already exist in standard C, variable values can be printed correctly; for the new types added by BiSheng C, such as _Owned struct and generics, values can also be printed correctly during debugging.

Below is an example:

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

The complete debugging process is as follows:

- Generate an executable file with debug information:

  ```shell
  clang -g debug_demo.cbs -o debug_demo_cbs
  ```

  **Note:** By default clang generates debug files in gdwarf-5 format, and the installed gdb version must be no lower than 10.1, otherwise there will be compatibility issues. Alternatively, you can add the `-gdwarf-4` compile option to generate debug files in the more compatible gdwarf-4 format:

  ```shell
  clang -g -gdwarf-4 debug_demo.cbs -o debug_demo_cbs
  ```

- Run `gdb debug_demo_cbs` to enter the gdb debugging interface.

- Below are some commonly used debugging commands:

  Enter `b main` to set a breakpoint, enter `run` to run the program; when the program reaches the breakpoint, enter `info locals` to view the values of local variables, enter `p var` to print the value of variable `var`, and enter `s` or `n` to step to the next line.

In the debugging example above, some information from the debugging process was recorded, as follows:

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

  During debugging, the value of `per` of type `_Owned struct` as well as the generic `a` were printed correctly.

  **Note:** The function name is displayed as something like `_ZN_ZTS6PersonIiEgetName`. This is the result of the compiler's name mangling (`Mangle`) and differs from the original name.

## Debugging BiSheng C source files after source-to-source translation

After translating BiSheng C source-to-source into standard C, you can directly debug the generated C file. Since the association information between standard C and BiSheng C is missing, debugging cannot establish a mapping to the original BiSheng C, which makes for a poor debugging experience. Against this backdrop, we have established a line-number mapping between the source-to-source-translated C file and the original cbs file, thereby supporting the ability to debug BiSheng C source files after source-to-source translation.

**Note:** The direct object of debugging is still the translated C file, but what is displayed during debugging is the cbs file from before translation; in essence, this establishes a certain degree of line-number mapping.

### Single-file debugging

Still using the cbs file from the previous subsection as an example, let us try to help developers better understand this debugging feature.

The complete debugging process is as follows:

- Perform source-to-source translation to generate a standard C file with line-number mappings:

  ```shell
  clang -rewrite-bsc -line debug_demo.cbs -o debug_demo.c
  ```

- View the generated C file:

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

  Examining the generated C file, you can see that before each function code block, the starting line number of its corresponding part in the original cbs file is inserted, in the format:

  ```c
  #line raw_number "debug_demo.cbs"
  ```

  Here `raw_number` is the starting line number, and `"debug_demo.cbs"` is the name of the original cbs file under the relative path.

- Generate an executable file with debug information:

  ```shell
  clang -g debug_demo.c -o debug_demo_c
  ```

- Run `gdb debug_demo_c` to enter the gdb debugging interface.

  At this point, the `gdb` interactive interface displays the content and line numbers from the original cbs file, rather than the line numbers from the translated C file. With the common `gdb` debugging commands, you can navigate within the original BiSheng C code and print some variable values.
  
  **Note:** If a relative path was used during source-to-source translation, debugging must be done under the same relative path; if an absolute path was used, this problem does not arise.

- Some information from the debugging process was recorded, as follows:

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

### Multi-file debugging

When there are references across multiple files, debugging can also correctly navigate between the multiple files. For example, suppose there is a source file `basic_math.cbs` containing basic math operation functions along with its header file `basic_math.hbs`, and the functions in `basic_math` are called within `cacl_demo.cbs`.

- Source files

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

- Source-to-source translation

  ```shell
  clang -rewrite-bsc -line basic_math.cbs basic_math.hbs calc_demo.cbs
  ```

  After translation, `basic_math.c`, `basic_math.h`, and `calc_demo.c` are generated, as follows:
  
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

- Generate debug information

  ```shell
  clang -g calc_demo.c basic_math.c -o calc_demo_c
  ```

- gdb debugging

  ```shell
  gdb calc_demo_c
  ```

  During debugging, you can navigate normally into the original `cbs` and `hbs` files. A partial example of the debugging process is shown below:

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

  The features and issues supported by this debugging feature are summarized as follows:
- The object of debugging is the translated standard C; its execution logic, variable information, etc. are not changed.
- The code locations displayed during debugging point to the original cbs files, and debugging navigation across multiple source files is supported.
- When there is a difference in the number of code lines before and after source-to-source translation such that a line-by-line mapping is not possible — for example, features that generate new code such as `_Owned struct` destructors and `_Trait` — the displayed debug location may be inaccurate, which developers need to be aware of.
