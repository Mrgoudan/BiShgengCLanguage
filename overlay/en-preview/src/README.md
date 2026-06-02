# Introduction

In the field of systems programming, C/C++ are the most widely used programming languages. In embedded scenarios where hardware resources are extremely limited, C is used the most. However, writing code in C comes with many pain points, such as the memory-safety problems caused by pointer usage in C, C's lack of native concurrency support, and the absence of some fundamental programming abstractions (such as generics). In recent years, there has been considerable exploratory work in the systems programming language space, such as Rust, which focuses on memory safety (ownership, lifetimes, the borrow checker, etc.) and concurrency (stackless coroutines). Rust is a brand-new programming language that adopts a language design completely different from C/C++, has a steep learning curve, and cannot solve the problem of developing legacy code.

Against this backdrop, BiSheng C takes a different approach: it introduces many enhanced designs on top of C, such as stronger memory-safety features and language-level support for concurrency, and these features can be adopted incrementally within legacy code without having to completely rewrite existing code. BiSheng C can be regarded as a superset of C. This user manual introduces BiSheng C from the following three aspects:

- Basic programming abstractions: member functions, _Trait, generics
- Memory safety: ownership, borrowing
- Concurrency: stackless coroutines
