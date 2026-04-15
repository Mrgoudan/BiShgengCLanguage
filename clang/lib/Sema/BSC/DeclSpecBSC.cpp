//===--- DeclSpecBSC.cpp - Declaration Specifier Semantic Analysis
//-----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for declaration specifiers.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/Sema.h"

using namespace clang;

bool DeclSpec::setFunctionSpecAsync(SourceLocation Loc, const char *&PrevSpec,
                                    unsigned &DiagID) {
  if (FS_async_specified) {
    DiagID = diag::warn_duplicate_declspec;
    PrevSpec = "_Async";
    return true;
  }
  FS_async_specified = true;
  FS_asyncLoc = Loc;
  return false;
}

bool DeclSpec::setFunctionSafeZoneSpecifier(SourceLocation Loc,
                                            const char *&PrevSpec,
                                            unsigned &DiagID,
                                            SafeZoneSpecifier SafeZoneSpec) {
  if (FS_safe_zone_specified == SZ_None) {
    FS_safe_zone_specified = SafeZoneSpec;
    FS_safe_zone_loc = Loc;
    return false;
  } else if (FS_safe_zone_specified == SZ_Safe) {
    PrevSpec = "_Safe";
  } else if (FS_safe_zone_specified == SZ_Unsafe) {
    PrevSpec = "_Unsafe";
  } else {
    PrevSpec = "";
  }
  DiagID = diag::err_duplicate_declspec;
  return true;
}

#endif