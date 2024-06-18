//===--- AccessSpecificTypeCheck.h - clang-tidy -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BSC_ACCESSSPECIFICTYPECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BSC_ACCESSSPECIFICTYPECHECK_H

#include "../ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace bsc {

/// This check is to give warning when access target 
/// types, which are writing in config file.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/bsc/access-specific-type.html
class AccessSpecificTypeCheck : public ClangTidyCheck {
public:
  AccessSpecificTypeCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context),
      TargetTypeList(Options.get("TargetTypes", "TEMP_FAILURE_RETRY")) {
        StringRef(TargetTypeList).split(TargetTypes, ",", -1, false);
      }
  void storeOptions(ClangTidyOptions::OptionMap &Opts) {
    Options.store(Opts, "TargetTypes", TargetTypeList);
  }  
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
private:
  const StringRef TargetTypeList;
  SmallVector<StringRef, 5> TargetTypes;
};

} // namespace bsc
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BSC_ACCESSSPECIFICTYPECHECK_H
