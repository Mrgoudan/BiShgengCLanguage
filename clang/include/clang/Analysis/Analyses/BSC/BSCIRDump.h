//===- BSCIRDump.h - Pretty-printing for BSCIR -*- C++ -*-====================//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pretty-printing utilities for BSCIR Body, Blocks, Statements, and
// Terminators. Used for debugging with --dump-bscir.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDUMP_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDUMP_H

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIR.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace bscir {

/// Dump a complete BSCIR Body in a human-readable format.
void dumpBody(const Body &B, llvm::raw_ostream &OS);

/// Dump a single basic block.
void dumpBlock(const BasicBlock &BB, const Body &B, llvm::raw_ostream &OS);

/// Dump a single statement.
void dumpStatement(const Statement &S, const Body &B, llvm::raw_ostream &OS);

/// Dump a terminator.
void dumpTerminator(const Terminator &T, const Body &B, llvm::raw_ostream &OS);

/// Dump a place.
void dumpPlace(const Place &P, llvm::raw_ostream &OS);

/// Dump an operand.
void dumpOperand(const Operand &O, llvm::raw_ostream &OS);

/// Dump an rvalue.
void dumpRvalue(const Rvalue &R, const Body &B, llvm::raw_ostream &OS);

} // namespace bscir
} // namespace clang

#endif // ENABLE_BSC
#endif // LLVM_CLANG_ANALYSIS_ANALYSES_BSC_BSCIRDUMP_H
