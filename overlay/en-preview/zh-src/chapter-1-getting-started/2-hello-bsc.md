# 第一个毕昇C程序

如下示例中，我们创建一个名为 bsc_project 的目录，创建 demo.cbs 源程序文件，并键入以下内容。

注：毕昇 C 的源码文件以 .cbs 为后缀。

```shell
$ mkdir -p bsc_project && cd bsc_project
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
    printf("foo.getA() = %d\n", foo.getA());// expected result: 3
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
