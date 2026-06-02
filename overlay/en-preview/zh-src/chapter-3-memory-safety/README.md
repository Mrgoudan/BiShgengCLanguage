# 内存安全

- [所有权](./1-ownership.md)
- [借用](./2-borrowing.md)
- [非空指针](./3-nonnull-pointer.md)
- [Owned Struct](./4-owned-struct.md)
- [安全区](./5-safe-zone.md)
- [初始化分析](./6-initial-analysis.md)

毕昇 C 内存管理的目标是将常见的时间类内存安全问题，如悬挂引用/内存泄漏/重复释放堆内存/解引用空指针等常见的内存安全问题在编译阶段暴露出来。

为此，毕昇 C 引入了[所有权](./1-ownership.md)和[借用](./2-borrowing.md)两个新的概念，所有权用关键字`_Owned`实现，借用通过关键字`_Borrow`表示，并通过`_Safe`和`_Unsafe`两个关键字来约束所有权和借用的执行范围。
