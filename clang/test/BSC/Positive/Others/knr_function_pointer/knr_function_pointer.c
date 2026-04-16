// RUN: %clang_cc1 -xbsc -fsyntax-only -verify -Wno-deprecated-non-prototype %s
// expected-no-diagnostics

struct funcptr {
  int (*func)();
};

typedef int (*NoProtoFunc)();

static int func(f)
  void *f;
{
  return 0;
}

void accept(NoProtoFunc fp) {
  (void)fp;
}

NoProtoFunc passthrough(NoProtoFunc fp) {
  return fp;
}

void test_cast(void) {
  NoProtoFunc fp = (NoProtoFunc)func;
}

int main(int argc, char *argv[]) {
  struct funcptr fp;
  NoProtoFunc local_fp = func;
  NoProtoFunc local_fp_addr = &func;

  fp.func = &func;
  fp.func = func;

  accept(func);
  accept(&func);

  local_fp = passthrough(func);
  local_fp_addr = passthrough(&func);

  return fp.func == local_fp && local_fp == local_fp_addr ? 0 : argc + (argv != 0);
}
