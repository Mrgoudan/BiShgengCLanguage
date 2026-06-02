# Source-to-Source Translation

## Overview

For a BiSheng C source file, in the common case you only need to use the BiSheng C compiler to compile it directly into the corresponding binary code and run it.
The BiSheng C compiler also provides the ability to convert the BiSheng C source code you write into equivalent standard C code; this ability is provided by the `-rewrite-bsc` compile option.
When this compile option is added at compile time, the compilation produces a file containing the equivalent C code, which can then be compiled into target code using a compiler such as gcc or clang.
In general, source-to-source translation is suitable for the following scenarios:

1. You want to inspect the equivalent C code corresponding to the BiSheng C code;
2. Building on an existing project developed in C, you use BiSheng C for incremental development but still want to compile with the original compilation toolchain;
3. When the target platform on which the program runs is not one of the standard backends supported by the BiSheng C compiler, you also need to first convert the BiSheng C code into C code, and then use a compiler that supports the target platform to compile the C code.

## Usage

The usage of the BiSheng C compiler's `-rewrite-bsc` compile option is similar to that of clang's native `-rewrite-objc` compile option. Simply add `-rewrite-bsc` when compiling the BiSheng C source you want to convert to compile in source-to-source translation mode.

For example: if you want to perform source-to-source translation on `test.cbs`, then using the following command at compile time will produce, in the current directory, a C file named `test.c` whose content is the translated C source.

```shell
clang -rewrite-bsc test.cbs
```

If the file to be translated needs to reference certain header files, then the `-I` option must likewise be used during source-to-source translation to specify the header file search path. For example, if `test.cbs` depends on the BiSheng C standard library (which will be described in detail in a later chapter), then the corresponding compile command is:

```shell
clang -rewrite-bsc test.cbs -I/path/to/libcbs/include
```

For a BiSheng C file named `xx.cbs`, the default file name of the C file produced by source-to-source translation is `xx.c`.
Source-to-source translation also supports using `-o` to specify the output C file name. The example below names the target file produced by source-to-source translation `a.c`:

```shell
clang -rewrite-bsc test.cbs -o a.c
```

The BiSheng C compiler also supports translating multiple files at the same time; however, when multiple files are translated at once, specifying the output directory or name with `-o` is not supported. An example is shown below:

```shell
clang -rewrite-bsc boo.cbs foo.cbs
```

Regarding source-to-source translation, there are a few additional points to explain and note:

1. When compiling in source-to-source translation mode, the compiler still first performs lexical, syntactic, and semantic checks on the file to be translated, just as in normal compilation mode. Errors in the file will cause compile errors, and no C file will be produced by the source-to-source translation. Only a file that passes compilation correctly will have a corresponding C file produced in source-to-source translation mode.
2. For a file with a non-cbs suffix, using the -rewrite-bsc option causes the option to be ignored (unless `-x bsc` is added to process the file as BiSheng C), and the following warning is reported.

    ```shell
    warning: ignoring '-rewrite-bsc' option because rewriting input type 'c' is not supported [-Woption-ignored]
    ```

3. Source-to-source translation is only needed for BiSheng C source files (cbs files), not for BiSheng C header files (hbs files). The C file produced by source-to-source translation no longer depends on any hbs file during subsequent compilation; the content of the hbs files that the original cbs file depended on is already included in the generated C file. Therefore, you can directly compile the generated C file and produce binary code.
4. Source-to-source translation strictly distinguishes between standard C header files and hbs files, but for the convenience of users it does not require all hbs files to have the .hbs suffix. The specific distinction rules are:
   - A file with the .hbs suffix is an hbs file.
   - A header file with the .h suffix whose first line is `#pragma bsc` is also treated as an hbs file.
   - A header file with the .h suffix that directly or indirectly includes an hbs file is also treated as an hbs file.

## Target File Structure

For source-to-source translation, the structure of the generated C file also needs to be explained. The content of the C file is arranged in the following order:

1. Header file inclusions. This includes all standard C header files that need to be referenced, including header files referenced directly by the cbs file as well as those referenced indirectly by hbs files. Since the target file no longer depends on hbs files, the target code will no longer contain references to BiSheng C header files.
2. Macro definitions. This includes all macros defined in the cbs file as well as macros defined in the hbs files referenced by the cbs file.
3. Type aliases and enum definitions. This includes all type alias definitions and enum type definitions. If a type alias is an alias of a _Trait, it will not appear in the target file. If a type alias is an alias of an anonymous type, then in the target file a typedef type alias is added for the anonymous type as the type name. For example:

    ```c
    // In the BiSheng C file:
    typedef struct {
        char *buf;
        int len;
    } MSG;
    
    // In the generated C file:
    struct _TD_MSG;
    typedef struct _TD_MSG MSG;
    
    struct _TD_MSG {
        char *buf;
        int len;
    };
    ```

4. Type definitions. This includes all type definitions in the cbs file as well as in the hbs files referenced by the cbs file. Since BiSheng C has generic types, the ordering of type definitions in the target file must be taken into account. The source-to-source translation of the BiSheng C compiler adopts a type topological sorting scheme, ensuring that when a type is defined, the complete definitions of all the types it depends on have already been included earlier. The following example shows a `LinkedList<Vec<int>>` type constructed using the BiSheng C standard library, along with the corresponding order of type definitions in the target file:

    ```c
    // In the BiSheng C file:
    #include "list.hbs"
    #include "vec.hbs"
    int main() {
        Vec<int> v1 = Vec<int>::new();
        Vec<int> v2 = Vec<int>::new();
        Vec<int> v3 = Vec<int>::new();
        for (int i = 0; i < 10; i = i + 1) {
            v1.push(i);
        }
        for (int i = 0; i < 10; i = i + 1) {
            v2.push(i);
        }
        for (int i = 0; i < 10; i = i + 1) {
            v3.push(i);
        }
        LinkedList<Vec<int>> l = LinkedList<Vec<int>>::new();
        l.push_back(v1);
        l.push_back(v2);
        l.push_back(v3);
        Vec<int> ele = l.pop_back();
        return 0;
    }
    
    // Compile command: clang -rewrite-bsc main.cbs -I/path/to/libcbs/include
    // clang -rewrite-bsc bishengc_safety.cbs -o bishengc_safety.c
    // clang main.c bishengc_safety.c
    // Type definitions in the generated C file (other code omitted):
    struct RawVec_int {
        int *ptr;
        size_t cap;
    };
    struct LinkedList_struct_Vec_int {
        struct _BSC_ListNode_struct_Vec_int *head;
        struct _BSC_ListNode_struct_Vec_int *tail;
        size_t len;
    };
    struct Vec_int {
        struct RawVec_int buf;
        size_t len;
    };
    struct _BSC_ListNode_struct_Vec_int {
        struct _BSC_ListNode_struct_Vec_int *next;
        struct _BSC_ListNode_struct_Vec_int *prev;
        struct Vec_int element;
    };
    ```

5. Declarations of generic functions and of extension member functions of generic structs.
6. Definitions of non-generic functions.
7. Definitions of generic functions and of extension member functions of generic structs.
