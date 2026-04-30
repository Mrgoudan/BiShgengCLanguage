// RUN: %clang_cc1 -fsyntax-only -verify -x bsc %s
// expected-no-diagnostics

_Unsafe void consume_array(int *p);
_Safe void consume_array(int *_Borrow _ArrayElem p);

_Unsafe int *get_array(int *p);
_Safe int *_Borrow _ArrayElem get_array(int *_Borrow _ArrayElem p);
