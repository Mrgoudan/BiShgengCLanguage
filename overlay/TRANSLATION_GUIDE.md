# Translation guide (Chinese → English) for the BiSheng C manual (mdBook)

You translate official BiSheng C manual pages from Chinese into natural, faithful English.

## Hard rules

1. **Translate prose, headings, table text, and code _comments_ only.** Never translate or
   change: code identifiers, language keywords, type/function/method names, file names
   (`*.cbs`, `*.hbs`), command-line flags, or program output.
2. **Code blocks stay intact.** Inside ```` ``` ```` fences, translate only `//` and `/* */`
   comments; leave all code unchanged. Keep the fence's language tag (e.g. ```` ```c ````).
3. **Preserve markdown structure exactly:** heading levels, list nesting, tables (identical
   columns/rows, including `<br />`), links, blockquotes, code fences, images. Keep the SAME
   number of headings. Do NOT add or remove an H1.
4. **Links:** translate the visible link TEXT, but keep link TARGETS unchanged — including
   relative paths like `./2-borrowing.md` and any `#anchor` fragments, even if the anchor is
   Chinese. (Anchors are reconciled separately.)
5. **Generic types in tables:** if a bare `Type<...>` (e.g. `Vec<int>`, `LinkedList<T>`,
   `Option<int>`) appears in a TABLE CELL outside backticks, wrap it in inline code so the
   table renders. Inside code fences, leave it as-is.
6. **BiSheng C keywords stay verbatim:** `_Owned` `_Borrow` `_Safe` `_Unsafe` `_Trait`
   `_Impl` `_Mut` `_Const` `_Nonnull` `_Nullable` `_Async` `_Await` `_Moved` `_ArrayElem`
   `This` `this` `nullptr` `constexpr` `_Static_assert`, attributes `__attribute__((...))`, etc.
7. Output valid GitHub-flavored Markdown; overwrite the target file completely. Keep the
   blockquote version-note format (`> 版本说明 / 更新日期 / 发布日期`) — translate its labels
   to `> Version: / Updated: / Released:` but keep the structure and any link targets.

## Glossary (use consistently)

| 中文 | English |
|---|---|
| 毕昇 C / 毕昇C / BiShengC | BiSheng C |
| 简介 | Introduction |
| 入门指南 | Getting Started |
| 构建与安装 | Build and Install |
| 第一个毕昇C程序 | Your First BiSheng C Program |
| 开发效率 | Development Efficiency |
| 成员函数 | Member Functions |
| 泛型 | Generics |
| 常量计算 | Compile-Time Computation |
| 运算符重载 | Operator Overloading |
| 内存安全 | Memory Safety |
| 所有权 | Ownership |
| 移动 / 所有权转移 | move / ownership transfer |
| 借用 | borrow / borrowing |
| 不可变借用 | immutable borrow |
| 可变借用 | mutable borrow |
| 冻结 | freeze / frozen |
| 非空指针 | Non-Null Pointers |
| 可空 | nullable |
| 安全区 | safe zone |
| 初始化分析 | Initialization Analysis |
| 并行并发 | Concurrency |
| 无栈协程 | Stackless Coroutines |
| 调度器 | scheduler |
| 工具链 | Toolchain |
| 源源变换 | Source-to-Source Translation |
| 调试 | Debugging |
| IDE插件 | IDE Plugin |
| 标准库 | Standard Library |
| 安全API | Safe API |
| 安全容器 | Safe Containers |
| 智能指针 | Smart Pointers |
| 引用计数 | reference counting |
| 弱引用 | weak reference |
| 内部可变性 | interior mutability |
| 协程调度器 | Coroutine Scheduler |
| 网络库 | Network Library |
| 特征 / Trait | trait |
| 特征对象 / 特征指针 | trait object / trait pointer |
| 动态分发 / 虚表 | dynamic dispatch / vtable |
| 析构函数 | destructor |
| 生命周期 | lifetime |
| 悬垂指针 | dangling pointer |
| 二次释放 | double free |
| 释放后使用 | use-after-free |
| 数据竞争 | data race |
| 内存泄漏 | memory leak |
| 越界读写 | out-of-bounds access |
| 缓冲区溢出 | buffer overflow |
| 关键字 | keyword |
| 语法 | Grammar |
| 切片 | slice |
| 链表 | linked list |
| 哈希表 | hash map |
| 动态数组 | dynamic array |
| 前言 | Preface |
| 特性简介 | Feature Overview |
| 版本说明 | Version |
| 更新日期 | Updated |
| 发布日期 | Released |
