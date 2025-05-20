#include <stdlib.h>

struct S<T> {
  T a;
  int b;
};

T S<T>::member(This *this) {
  return this->a;
}