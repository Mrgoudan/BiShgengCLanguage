//===-- ClangBSCFuzzer.cpp - Fuzz Clang -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements a function that runs BiSheng C Clang on a
///  single input. This function is then linked into the Fuzzer library.
///
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "handle-cxx/handle_cxx.h"
#include <cstring>

using namespace clang_fuzzer;

namespace clang_fuzzer {

static std::vector<const char *> CLArgs;

const std::vector<const char *>& GetCLArgs() {
  return CLArgs;
}

} // namespace clang_fuzzer

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  CLArgs.push_back("-O2");
  for (int I = 1; I < *argc; I++) {
    if (strcmp((*argv)[I], "-ignore_remaining_args=1") == 0) {
      for (I++; I < *argc; I++)
        CLArgs.push_back((*argv)[I]);
      break;
    }
  }
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t *data, size_t size) {
  std::string s(reinterpret_cast<const char *>(data), size);
  HandleCXX(s, "./test.cbs", CLArgs);
  return 0;
}

#endif
