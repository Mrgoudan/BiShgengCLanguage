# Memory Safety

- [Ownership](./1-ownership.md)
- [Borrowing](./2-borrowing.md)
- [Non-Null Pointers](./3-nonnull-pointer.md)
- [Owned Struct](./4-owned-struct.md)
- [Safe Zone](./5-safe-zone.md)
- [Initialization Analysis](./6-initial-analysis.md)

The goal of BiSheng C memory management is to surface common temporal memory-safety problems at compile time, such as dangling references, memory leaks, double-freeing heap memory, and dereferencing null pointers.

To this end, BiSheng C introduces two new concepts, [ownership](./1-ownership.md) and [borrowing](./2-borrowing.md). Ownership is implemented with the `_Owned` keyword, borrowing is expressed with the `_Borrow` keyword, and the `_Safe` and `_Unsafe` keywords are used to constrain the scope in which ownership and borrowing are enforced.
