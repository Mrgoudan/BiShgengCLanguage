#pragma bsc

#include <stdlib.h>
#include <errno.h>

struct S<T> {
  T a;
  T b;
};

S<T> bsc_func1<T>(T t) {
  S<T> local = { t, t };
  return local;
}