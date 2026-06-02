# Your First BiSheng C Program

In the following example, we create a directory named bsc_project, create the source file demo.cbs, and type in the following content.

Note: BiSheng C source files use the .cbs suffix.

```shell
$ mkdir -p bsc_project && cd bsc_project
$ touch demo.cbs
# Write the following content into demo.cbs:
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
    printf("foo.getA() = %d\n", foo.getA());// expected result: 3
    return 0;
}
```

Use the compile command to compile this file and obtain an executable, as follows:

```shell
# clang + llvm compile and run
$ clang demo.cbs -o demo
$ ./demo
foo.getA() = 1

# clang + maple compile and run
$ maple demo.cbs -o demo
$ qemu-aarch64 demo
foo.getA() = 1
```

If the output matches the result above, it means you have successfully applied the member-function feature of BiSheng C.
