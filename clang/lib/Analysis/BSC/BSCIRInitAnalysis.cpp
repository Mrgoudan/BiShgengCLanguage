//===- BSCIRInitAnalysis.cpp - Initialization analysis on BSCIR -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// P2795-style initialization analysis. In _Safe zones, ALL variables must be
// definitely initialized before use (not just _Owned types). This is a
// forward dataflow analysis computing definite initialization.
//
//===----------------------------------------------------------------------===//

#if ENABLE_BSC

#include "clang/Analysis/Analyses/BSC/BSCIRInitAnalysis.h"
#include "clang/Analysis/Analyses/BSC/BSCIRDataflow.h"
#include "clang/AST/Attr.h"
#include "clang/Basic/Builtins.h"

using namespace clang;
using namespace clang::bscir;

/// Check if a type is the builtin va_list type.
static bool isVaListType(QualType Ty, const ASTContext &Ctx) {
  return Ctx.hasSameType(Ty, Ctx.getBuiltinVaListType());
}

/// Check if a local should be treated as implicitly initialized:
/// globals/statics or va_list type.
static bool isImplicitlyInitialized(const LocalDecl &LD, const Body &B) {
  if (LD.OriginalDecl) {
    if (LD.OriginalDecl->hasGlobalStorage())
      return true;
  }
  if (B.SourceFD)
    if (isVaListType(LD.Ty, B.SourceFD->getASTContext()))
      return true;
  return false;
}

//===----------------------------------------------------------------------===//
// InitAnalysis: Dataflow Implementation
//===----------------------------------------------------------------------===//

InitLattice InitAnalysis::entryState(const Body &B) const {
  InitLattice State;

  // Return slot (_0) starts uninitialized
  State.LocalStates[LocalId{0}] = InitState::Uninitialized;

  // Parameters (1..NumParams) start initialized, including all nested fields
  for (unsigned I = 1; I <= B.NumParams; ++I) {
    State.LocalStates[LocalId{I}] = InitState::Initialized;
    // Recursively mark all fields as initialized for struct params.
    const LocalDecl &LD = B.getLocal(LocalId{I});
    SmallVector<unsigned, 4> Prefix;
    markAllFieldsInit(State, LocalId{I}, LD.Ty, Prefix);
  }

  // All other locals start uninitialized, except globals/statics and
  // va_list types which are treated as fully initialized.
  for (unsigned I = B.NumParams + 1; I < B.Locals.size(); ++I) {
    const LocalDecl &LD = B.getLocal(LocalId{I});
    if (isImplicitlyInitialized(LD, B)) {
      State.LocalStates[LocalId{I}] = InitState::Initialized;
      SmallVector<unsigned, 4> Prefix;
      markAllFieldsInit(State, LocalId{I}, LD.Ty, Prefix);
    } else {
      State.LocalStates[LocalId{I}] = InitState::Uninitialized;
    }
  }

  // For callee-side ensure_init: *param starts uninitialized
  if (B.SourceFD) {
    for (unsigned I = 0; I < B.SourceFD->getNumParams(); ++I) {
      const ParmVarDecl *PVD = B.SourceFD->getParamDecl(I);
      if (PVD->hasAttr<EnsureInitAttr>()) {
        State.EnsureInitDerefStates[LocalId{I + 1}] = InitState::Uninitialized;
      }
    }
  }

  return State;
}

bool InitAnalysis::transferStatement(const Statement &S,
                                     InitLattice &State) const {
  bool Changed = false;

  switch (S.K) {
  case Statement::Assign: {
    // Assignment to a place: mark destination as initialized
    if (S.getAssign().Dest.isLocal()) {
      LocalId Dest = S.getAssign().Dest.Base;
      auto It = State.LocalStates.find(Dest);
      if (It == State.LocalStates.end() ||
          It->second != InitState::Initialized) {
        State.LocalStates[Dest] = InitState::Initialized;
        Changed = true;
      }
    } else if (auto FP = getFieldPath(S.getAssign().Dest)) {
      // Array-typed fields cannot be initialized element-by-element.
      // They must be initialized via {} or __assume_initialized.
      if (getFieldType(FP->Base, FP->Indices)->isArrayType())
        break;
      unsigned UnionDepth = 0;
      if (isUnionStructFieldPath(*FP, UnionDepth)) {
        // Writing a struct field within a union variant (e.g., u.s.x).
        // Track field-level init AND mark the union path as Initialized
        // so cross-variant reads (e.g., u.f) pass.
        markFieldInit(State, *FP, Changed);
        if (UnionDepth == 0) {
          // Top-level union local: mark whole local Initialized so that
          // cross-variant reads pass the whole-local check.
          auto &LS = State.LocalStates[FP->Base];
          if (LS != InitState::Initialized) {
            LS = InitState::Initialized;
            Changed = true;
          }
        }
        // For nested unions (UnionDepth > 0), do NOT mark the union field
        // path as Init here — that would bypass field-level tracking.
        // Cross-variant reads for nested unions are handled in checkOperand.
      } else if (isUnionVariantPath(*FP, UnionDepth)) {
        // Writing a whole union variant (e.g., u.f or u.s = {...}).
        // Clear any struct-field tracking and mark union Initialized.
        llvm::SmallVector<unsigned, 4> Prefix(FP->Indices.begin(),
                                              FP->Indices.begin() + UnionDepth);
        clearUnionFieldEntries(State, FP->Base, Prefix, Changed);
        if (UnionDepth == 0) {
          // Top-level union: mark whole local Initialized.
          auto &LS = State.LocalStates[FP->Base];
          if (LS != InitState::Initialized) {
            LS = InitState::Initialized;
            Changed = true;
          }
        } else {
          // Union nested in a struct: mark the union field path as Init.
          FieldPath UnionFP;
          UnionFP.Base = FP->Base;
          UnionFP.Indices.assign(FP->Indices.begin(),
                                 FP->Indices.begin() + UnionDepth);
          markFieldInit(State, UnionFP, Changed);
        }
      } else {
        // Normal struct field init (no union involved).
        markFieldInit(State, *FP, Changed);
      }
    }
    // Note: array element writes (arr[i] = ...) do NOT mark the array as
    // initialized. Arrays must be initialized via initializer list or
    // __assume_initialized.

    // Callee-side ensure_init: detect assignments through *param
    // Pattern: Assign dest is Place{paramId, [Deref, ...]}
    if (!S.getAssign().Dest.Projections.empty() &&
        S.getAssign().Dest.Projections[0].K == ProjectionElem::Deref) {
      LocalId Base = S.getAssign().Dest.Base;
      auto It = State.EnsureInitDerefStates.find(Base);
      if (It != State.EnsureInitDerefStates.end()) {
        if (It->second != InitState::Initialized) {
          It->second = InitState::Initialized;
          Changed = true;
        }
      }
    }
    break;
  }

  case Statement::StorageLive: {
    // New variable comes into scope: starts uninitialized,
    // unless implicitly initialized (globals/statics, va_list).
    LocalId SL = S.getStorageLocal();
    const LocalDecl &SLD = B.getLocal(SL);
    if (isImplicitlyInitialized(SLD, B)) {
      auto It = State.LocalStates.find(SL);
      if (It == State.LocalStates.end() ||
          It->second != InitState::Initialized) {
        State.LocalStates[SL] = InitState::Initialized;
        SmallVector<unsigned, 4> Prefix;
        markAllFieldsInit(State, SL, SLD.Ty, Prefix);
        Changed = true;
      }
    } else {
      auto It = State.LocalStates.find(SL);
      if (It == State.LocalStates.end() ||
          It->second != InitState::Uninitialized) {
        State.LocalStates[SL] = InitState::Uninitialized;
        Changed = true;
      }
      clearFieldStates(State, SL, Changed);
    }
    break;
  }

  case Statement::StorageDead: {
    // Variable goes out of scope: mark uninitialized
    LocalId SL = S.getStorageLocal();
    auto It = State.LocalStates.find(SL);
    if (It == State.LocalStates.end() ||
        It->second != InitState::Uninitialized) {
      State.LocalStates[SL] = InitState::Uninitialized;
      Changed = true;
    }
    clearFieldStates(State, SL, Changed);
    break;
  }

  case Statement::Nop:
    break;
  }

  return Changed;
}

InitLattice InitAnalysis::transferTerminator(const Terminator &T,
                                             const InitLattice &StateBeforeTerm,
                                             BasicBlockId Target) const {
  // Call terminators: the destination is initialized after the call
  if (T.K == Terminator::Call) {
    InitLattice Result = StateBeforeTerm;
    const auto &CD = T.getCall();
    if (CD.Dest.isLocal()) {
      Result.LocalStates[CD.Dest.Base] = InitState::Initialized;
    }

    // __assume_initialized(&x): mark x as initialized at this point.
    if (CD.Decl &&
        CD.Decl->getBuiltinID() == Builtin::BI__assume_initialized) {
      if (!CD.ArgPlaces.empty() && CD.ArgPlaces[0]) {
        const Place &ArgPlace = *CD.ArgPlaces[0];
        if (ArgPlace.Projections.empty()) {
          Result.LocalStates[ArgPlace.Base] = InitState::Initialized;
          SmallVector<unsigned, 4> Prefix;
          markAllFieldsInit(Result, ArgPlace.Base,
                            B.getLocal(ArgPlace.Base).Ty, Prefix);
        } else if (auto FP = getFieldPath(ArgPlace)) {
          bool Changed = false;
          markFieldInit(Result, *FP, Changed);
        }
      }
      return Result;
    }

    // Caller side: mark places initialized for ensure_init parameters.
    // Determine which parameters have ensure_init from either:
    //   (a) the direct FunctionDecl's ParmVarDecl attrs, or
    //   (b) the FunctionProtoType's ExtParameterInfo (for indirect calls).
    {
      unsigned NumParams = 0;
      if (CD.Decl)
        NumParams = CD.Decl->getNumParams();
      else if (CD.CalleeProtoType)
        NumParams = CD.CalleeProtoType->getNumParams();

      for (unsigned I = 0; I < NumParams; ++I) {
        bool IsEnsureInit = false;
        if (CD.Decl && I < CD.Decl->getNumParams())
          IsEnsureInit = CD.Decl->getParamDecl(I)->hasAttr<EnsureInitAttr>();
        if (!IsEnsureInit && CD.CalleeProtoType &&
            CD.CalleeProtoType->hasExtParameterInfos() &&
            I < CD.CalleeProtoType->getNumParams())
          IsEnsureInit = CD.CalleeProtoType->getExtParameterInfo(I).isEnsureInit();
        if (!IsEnsureInit)
          continue;

        // Callee-side delegation: if the argument is a direct pass of an
        // ensure_init param (Copy(_N) where _N is an ensure_init param),
        // mark the deref state as Initialized.
        if (I < CD.Args.size()) {
          const Operand &Arg = CD.Args[I];
          if (Arg.K == Operand::Copy && Arg.getPlace().Projections.empty()) {
            auto DerefIt = Result.EnsureInitDerefStates.find(Arg.getPlace().Base);
            if (DerefIt != Result.EnsureInitDerefStates.end()) {
              DerefIt->second = InitState::Initialized;
            }
          }
        }

        // Caller side: mark the addressed place as initialized
        if (I >= CD.ArgPlaces.size() || !CD.ArgPlaces[I])
          continue;

        const Place &ArgPlace = *CD.ArgPlaces[I];
        if (ArgPlace.Projections.empty()) {
          // Whole local: mark Initialized + all fields
          Result.LocalStates[ArgPlace.Base] = InitState::Initialized;
          SmallVector<unsigned, 4> Prefix;
          markAllFieldsInit(Result, ArgPlace.Base,
                            B.getLocal(ArgPlace.Base).Ty, Prefix);
        } else if (auto FP = getFieldPath(ArgPlace)) {
          // Field-level: mark specific field, promote parent
          bool Changed = false;
          markFieldInit(Result, *FP, Changed);
        }
      }
    }

    return Result;
  }

  // For all other terminators, pass through unchanged
  return StateBeforeTerm;
}

/// Compute the meet of two init states (intersection semantics).
static InitState meetStates(InitState A, InitState B) {
  if (A == B)
    return A;
  // Any combination of different states produces MaybeInit:
  // Init + Uninit, Init + MaybeInit, or MaybeInit + Uninit.
  return InitState::MaybeInit;
}

bool InitAnalysis::merge(const InitLattice &Src, InitLattice &Dst) const {
  bool Changed = false;

  // Merge LocalStates
  for (const auto &Entry : Src.LocalStates) {
    auto It = Dst.LocalStates.find(Entry.first);
    if (It == Dst.LocalStates.end()) {
      Dst.LocalStates[Entry.first] = Entry.second;
      Changed = true;
    } else {
      InitState NewState = meetStates(Entry.second, It->second);
      if (NewState != It->second) {
        It->second = NewState;
        Changed = true;
      }
    }
  }

  // Merge FieldStates: missing entries on either side treated as Uninitialized.
  for (const auto &Entry : Src.FieldStates) {
    auto It = Dst.FieldStates.find(Entry.first);
    if (It == Dst.FieldStates.end()) {
      // Dst missing = Uninitialized on Dst side
      InitState NewState = meetStates(Entry.second, InitState::Uninitialized);
      Dst.FieldStates[Entry.first] = NewState;
      Changed = true;
    } else {
      InitState NewState = meetStates(Entry.second, It->second);
      if (NewState != It->second) {
        It->second = NewState;
        Changed = true;
      }
    }
  }

  // Handle field entries in Dst that are not in Src: treat Src as Uninitialized.
  for (auto &Entry : Dst.FieldStates) {
    if (Src.FieldStates.find(Entry.first) == Src.FieldStates.end()) {
      InitState NewState = meetStates(InitState::Uninitialized, Entry.second);
      if (NewState != Entry.second) {
        Entry.second = NewState;
        Changed = true;
      }
    }
  }

  // Merge EnsureInitDerefStates
  for (const auto &Entry : Src.EnsureInitDerefStates) {
    auto It = Dst.EnsureInitDerefStates.find(Entry.first);
    if (It == Dst.EnsureInitDerefStates.end()) {
      Dst.EnsureInitDerefStates[Entry.first] = Entry.second;
      Changed = true;
    } else {
      InitState NewState = meetStates(Entry.second, It->second);
      if (NewState != It->second) {
        It->second = NewState;
        Changed = true;
      }
    }
  }

  return Changed;
}

//===----------------------------------------------------------------------===//
// Diagnostic Helpers
//===----------------------------------------------------------------------===//

InitState InitAnalysis::getInitState(const InitLattice &State,
                                     LocalId Id) const {
  auto It = State.LocalStates.find(Id);
  if (It == State.LocalStates.end())
    return InitState::Uninitialized;
  return It->second;
}

void InitAnalysis::markInit(InitLattice &State, LocalId Id) const {
  State.LocalStates[Id] = InitState::Initialized;
}

void InitAnalysis::markUninit(InitLattice &State, LocalId Id) const {
  State.LocalStates[Id] = InitState::Uninitialized;
}

//===----------------------------------------------------------------------===//
// Field-Level Init Tracking Helpers (Recursive)
//===----------------------------------------------------------------------===//

unsigned InitAnalysis::getNumFields(QualType Ty) {
  const RecordDecl *RD = Ty->getAsRecordDecl();
  if (!RD || RD->isUnion())
    return 0;
  unsigned Count = 0;
  for (auto It = RD->field_begin(); It != RD->field_end(); ++It)
    ++Count;
  return Count;
}

QualType InitAnalysis::getFieldType(LocalId Id,
                                    ArrayRef<unsigned> Path) const {
  QualType Ty = B.getLocal(Id).Ty;
  for (unsigned Idx : Path) {
    const RecordDecl *RD = Ty->getAsRecordDecl();
    assert(RD && "getFieldType: expected record type along path");
    unsigned I = 0;
    for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++I) {
      if (I == Idx) {
        Ty = It->getType();
        break;
      }
    }
  }
  return Ty;
}

llvm::Optional<FieldPath> InitAnalysis::getFieldPath(const Place &P) const {
  if (P.Projections.empty())
    return llvm::None;
  if (P.Projections[0].K != ProjectionElem::Field)
    return llvm::None;

  FieldPath FP;
  FP.Base = P.Base;

  // Walk the local's type through the field/index chain.
  QualType CurTy = B.getLocal(P.Base).Ty;

  for (const auto &Proj : P.Projections) {
    if (Proj.K == ProjectionElem::Field) {
      const RecordDecl *RD = CurTy->getAsRecordDecl();
      if (!RD)
        break;
      // Continue through unions into their variants.
      FP.Indices.push_back(Proj.FieldIndex);
      CurTy = Proj.ResultTy;
    } else {
      break;
    }
  }

  if (FP.Indices.empty())
    return llvm::None;

  return FP;
}

InitState InitAnalysis::getFieldInitState(const InitLattice &State,
                                          const FieldPath &FP) const {
  auto It = State.FieldStates.find(FP);
  if (It == State.FieldStates.end())
    return InitState::Uninitialized;
  return It->second;
}

void InitAnalysis::markFieldInit(InitLattice &State, const FieldPath &FP,
                                 bool &Changed) const {
  auto &FS = State.FieldStates[FP];
  if (FS != InitState::Initialized) {
    FS = InitState::Initialized;
    Changed = true;
  }
  tryPromoteParent(State, FP, Changed);
}

void InitAnalysis::tryPromoteParent(InitLattice &State, const FieldPath &FP,
                                    bool &Changed) const {
  if (FP.Indices.empty())
    return;

  // Build the parent path (everything except the last index).
  FieldPath Parent;
  Parent.Base = FP.Base;
  Parent.Indices.assign(FP.Indices.begin(), FP.Indices.end() - 1);

  // Get the parent type to count siblings.
  QualType ParentTy = Parent.Indices.empty()
                          ? B.getLocal(FP.Base).Ty
                          : getFieldType(FP.Base, Parent.Indices);

  unsigned NumSiblings = getNumFields(ParentTy);
  if (NumSiblings == 0) {
    // Check if parent is a union: promoting any single variant means the
    // whole union is fully covered (one complete variant covers all bytes).
    const RecordDecl *PRD = ParentTy->getAsRecordDecl();
    if (PRD && PRD->isUnion()) {
      if (Parent.Indices.empty()) {
        auto &LS = State.LocalStates[FP.Base];
        if (LS != InitState::Initialized) {
          LS = InitState::Initialized;
          Changed = true;
        }
      } else {
        auto &PS = State.FieldStates[Parent];
        if (PS != InitState::Initialized) {
          PS = InitState::Initialized;
          Changed = true;
        }
        tryPromoteParent(State, Parent, Changed);
      }
      // Clear field-level entries since the whole union is now promoted.
      clearUnionFieldEntries(State, FP.Base, Parent.Indices, Changed);
    }
    return;
  }

  // Check if all siblings at this level are Initialized.
  for (unsigned I = 0; I < NumSiblings; ++I) {
    FieldPath Sibling;
    Sibling.Base = FP.Base;
    Sibling.Indices = Parent.Indices;
    Sibling.Indices.push_back(I);
    if (getFieldInitState(State, Sibling) != InitState::Initialized)
      return; // Not all siblings initialized yet.
  }

  // All siblings initialized. Promote parent.
  if (Parent.Indices.empty()) {
    // Promote the whole local.
    auto &LS = State.LocalStates[FP.Base];
    if (LS != InitState::Initialized) {
      LS = InitState::Initialized;
      Changed = true;
    }
  } else {
    // Mark the parent field path as Initialized.
    auto &PS = State.FieldStates[Parent];
    if (PS != InitState::Initialized) {
      PS = InitState::Initialized;
      Changed = true;
    }
    // Recurse upward.
    tryPromoteParent(State, Parent, Changed);
  }
}

void InitAnalysis::clearFieldStates(InitLattice &State, LocalId Id,
                                    bool &Changed) const {
  // Erase all entries with matching Base using std::map's ordered iteration.
  // FieldPath ordering is Base first, so we can use lower_bound.
  FieldPath LowKey;
  LowKey.Base = Id;
  // Empty Indices sorts before any non-empty Indices.

  auto It = State.FieldStates.lower_bound(LowKey);
  while (It != State.FieldStates.end() && It->first.Base == Id) {
    It = State.FieldStates.erase(It);
    Changed = true;
  }
}

void InitAnalysis::markAllFieldsInit(InitLattice &State, LocalId Base,
                                     QualType Ty,
                                     SmallVector<unsigned, 4> &Prefix) const {
  const RecordDecl *RD = Ty->getAsRecordDecl();
  if (!RD)
    return;

  unsigned FIdx = 0;
  for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++FIdx) {
    Prefix.push_back(FIdx);

    QualType FieldTy = It->getType();
    const RecordDecl *FieldRD = FieldTy->getAsRecordDecl();
    if (FieldRD) {
      // Recurse into nested struct or union variants.
      markAllFieldsInit(State, Base, FieldTy, Prefix);
    }

    // Mark this field path as initialized (leaf or intermediate).
    FieldPath FP;
    FP.Base = Base;
    FP.Indices.assign(Prefix.begin(), Prefix.end());
    State.FieldStates[FP] = InitState::Initialized;

    Prefix.pop_back();
  }
}

std::string InitAnalysis::buildFieldName(const FieldPath &FP) const {
  const LocalDecl &LD = B.getLocal(FP.Base);
  std::string Name = LD.Name.str();

  QualType CurTy = LD.Ty;
  for (unsigned Idx : FP.Indices) {
    const RecordDecl *RD = CurTy->getAsRecordDecl();
    if (!RD)
      break;
    unsigned I = 0;
    for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++I) {
      if (I == Idx) {
        if (!It->isAnonymousStructOrUnion()) {
          Name += "." + It->getNameAsString();
        }
        CurTy = It->getType();
        break;
      }
    }
  }
  return Name;
}

bool InitAnalysis::isUnionStructFieldPath(const FieldPath &FP,
                                          unsigned &UnionDepth) const {
  QualType CurTy = B.getLocal(FP.Base).Ty;
  for (unsigned I = 0; I < FP.Indices.size(); ++I) {
    const RecordDecl *RD = CurTy->getAsRecordDecl();
    if (!RD)
      return false;
    if (RD->isUnion()) {
      // We're at a union level. The current index selects a variant.
      // Check if that variant is a struct and the path continues deeper.
      unsigned Idx = FP.Indices[I];
      unsigned F = 0;
      for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++F) {
        if (F == Idx) {
          QualType VariantTy = It->getType();
          const RecordDecl *VariantRD = VariantTy->getAsRecordDecl();
          if (VariantRD && !VariantRD->isUnion() && I + 1 < FP.Indices.size()) {
            // Found: union → struct variant → continues into struct fields.
            UnionDepth = I;
            return true;
          }
          // Variant is scalar, another union, or path ends here. Continue
          // walking to check deeper levels.
          CurTy = VariantTy;
          break;
        }
      }
      continue;
    }
    // Walk into the struct field.
    unsigned Idx = FP.Indices[I];
    unsigned F = 0;
    for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++F) {
      if (F == Idx) {
        CurTy = It->getType();
        break;
      }
    }
  }
  return false;
}

bool InitAnalysis::isUnionVariantPath(const FieldPath &FP,
                                      unsigned &UnionDepth) const {
  QualType CurTy = B.getLocal(FP.Base).Ty;
  for (unsigned I = 0; I < FP.Indices.size(); ++I) {
    const RecordDecl *RD = CurTy->getAsRecordDecl();
    if (!RD)
      return false;
    if (RD->isUnion()) {
      // The path enters this union. If it ends here (last index), it's a
      // union variant path (e.g., u.f or u.s as a whole).
      if (I + 1 == FP.Indices.size()) {
        UnionDepth = I;
        return true;
      }
      // It continues deeper — walk into the variant.
      unsigned Idx = FP.Indices[I];
      unsigned F = 0;
      for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++F) {
        if (F == Idx) {
          CurTy = It->getType();
          break;
        }
      }
      continue;
    }
    unsigned Idx = FP.Indices[I];
    unsigned F = 0;
    for (auto It = RD->field_begin(); It != RD->field_end(); ++It, ++F) {
      if (F == Idx) {
        CurTy = It->getType();
        break;
      }
    }
  }
  return false;
}

bool InitAnalysis::hasUnionFieldEntries(const InitLattice &State, LocalId Base,
                                        ArrayRef<unsigned> UnionPrefix) const {
  // Build a low key with the prefix and scan for entries that extend beyond it.
  FieldPath LowKey;
  LowKey.Base = Base;
  LowKey.Indices.assign(UnionPrefix.begin(), UnionPrefix.end());

  auto It = State.FieldStates.lower_bound(LowKey);
  while (It != State.FieldStates.end() && It->first.Base == Base) {
    const auto &Indices = It->first.Indices;
    if (Indices.size() > UnionPrefix.size()) {
      // Check prefix match.
      bool Match = true;
      for (unsigned I = 0; I < UnionPrefix.size(); ++I) {
        if (Indices[I] != UnionPrefix[I]) {
          Match = false;
          break;
        }
      }
      if (Match)
        return true;
    }
    ++It;
  }
  return false;
}

void InitAnalysis::clearUnionFieldEntries(InitLattice &State, LocalId Base,
                                          ArrayRef<unsigned> UnionPrefix,
                                          bool &Changed) const {
  FieldPath LowKey;
  LowKey.Base = Base;
  LowKey.Indices.assign(UnionPrefix.begin(), UnionPrefix.end());

  auto It = State.FieldStates.lower_bound(LowKey);
  while (It != State.FieldStates.end() && It->first.Base == Base) {
    const auto &Indices = It->first.Indices;
    // Check prefix match (strictly extending, not exact match).
    bool Match = true;
    if (Indices.size() > UnionPrefix.size()) {
      for (unsigned I = 0; I < UnionPrefix.size(); ++I) {
        if (Indices[I] != UnionPrefix[I]) {
          Match = false;
          break;
        }
      }
    } else {
      Match = false;
    }
    if (Match) {
      It = State.FieldStates.erase(It);
      Changed = true;
    } else if (Indices > LowKey.Indices) {
      break; // Past the prefix range due to ordering.
    } else {
      ++It;
    }
  }
}

void InitAnalysis::checkOperand(const Operand &Op, const InitLattice &State,
                                SourceLocation Loc,
                                SmallVectorImpl<InitDiagInfo> &Diags) const {
  if (Op.K == Operand::Constant)
    return;

  if (Op.getPlace().Loc.isValid())
    Loc = Op.getPlace().Loc;

  LocalId Id = Op.getPlace().Base;
  InitState IS = getInitState(State, Id);

  // If the whole local is Initialized, most field accesses are fine.
  // Exception: reading a struct field within a union variant when there is
  // active field-level tracking (meaning a partial struct write happened).
  if (IS == InitState::Initialized) {
    if (auto FP = getFieldPath(Op.getPlace())) {
      unsigned UnionDepth = 0;
      if (isUnionStructFieldPath(*FP, UnionDepth)) {
        llvm::SmallVector<unsigned, 4> Prefix(FP->Indices.begin(),
                                              FP->Indices.begin() + UnionDepth);
        if (hasUnionFieldEntries(State, Id, Prefix)) {
          InitState FS = getFieldInitState(State, *FP);
          if (FS == InitState::Initialized)
            return;
          const LocalDecl &LD = B.getLocal(Id);
          if (LD.IsTemp || LD.Name.empty())
            return;
          std::string FieldName = buildFieldName(*FP);
          if (FS == InitState::Uninitialized)
            Diags.emplace_back(InitDiagKind::UseOfUninit,
                               Loc.isValid() ? Loc : LD.DeclLoc, FieldName);
          else
            Diags.emplace_back(InitDiagKind::UseOfMaybeUninit,
                               Loc.isValid() ? Loc : LD.DeclLoc, FieldName);
          return;
        }
      }
    }
    return;
  }

  // Check field-level state if the operand has a field projection.
  if (auto FP = getFieldPath(Op.getPlace())) {
    InitState FS = getFieldInitState(State, *FP);
    if (FS == InitState::Initialized)
      return;

    // Cross-variant read of a nested union: if this path ends at a union
    // variant and there are any field entries under that union prefix,
    // some variant was written — cross-variant read is OK.
    {
      unsigned VUnionDepth = 0;
      if (isUnionVariantPath(*FP, VUnionDepth)) {
        llvm::SmallVector<unsigned, 4> Prefix(FP->Indices.begin(),
                                              FP->Indices.begin() + VUnionDepth);
        if (hasUnionFieldEntries(State, Id, Prefix))
          return;
      }
    }

    // Check if any ancestor field path is initialized (e.g., whole struct
    // or union field assigned covers all sub-paths).
    {
      FieldPath Ancestor;
      Ancestor.Base = FP->Base;
      bool AncestorInit = false;
      for (unsigned i = 0; i + 1 < FP->Indices.size(); ++i) {
        Ancestor.Indices.push_back(FP->Indices[i]);
        if (getFieldInitState(State, Ancestor) == InitState::Initialized) {
          AncestorInit = true;
          break;
        }
      }
      if (AncestorInit)
        return;
    }

    const LocalDecl &LD = B.getLocal(Id);
    if (LD.IsTemp || LD.Name.empty())
      return;

    // Build field-qualified name: "o.inner.b"
    std::string FieldName = buildFieldName(*FP);

    if (FS == InitState::Uninitialized) {
      Diags.emplace_back(InitDiagKind::UseOfUninit,
                         Loc.isValid() ? Loc : LD.DeclLoc, FieldName);
    } else {
      Diags.emplace_back(InitDiagKind::UseOfMaybeUninit,
                         Loc.isValid() ? Loc : LD.DeclLoc, FieldName);
    }
    return;
  }

  // Whole-local check (existing logic).
  if (IS == InitState::Uninitialized) {
    const LocalDecl &LD = B.getLocal(Id);
    if (!LD.IsTemp && !LD.Name.empty()) {
      Diags.emplace_back(InitDiagKind::UseOfUninit,
                         Loc.isValid() ? Loc : LD.DeclLoc, LD.Name);
    }
  } else if (IS == InitState::MaybeInit) {
    const LocalDecl &LD = B.getLocal(Id);
    if (!LD.IsTemp && !LD.Name.empty()) {
      Diags.emplace_back(InitDiagKind::UseOfMaybeUninit,
                         Loc.isValid() ? Loc : LD.DeclLoc, LD.Name);
    }
  }
}

//===----------------------------------------------------------------------===//
// Run Analysis with Diagnostic Collection
//===----------------------------------------------------------------------===//

void InitAnalysis::run(SmallVectorImpl<InitDiagInfo> &Diags) const {
  // Run forward dataflow analysis
  DataflowResult<InitLattice> Result = runForwardAnalysis(B, *this);

  // Walk through all blocks and check uses against computed states
  for (const BasicBlock &BB : B.Blocks) {
    auto EntryIt = Result.EntryStates.find(BB.Id);
    if (EntryIt == Result.EntryStates.end())
      continue;

    InitLattice State = EntryIt->second;

    // Exempt address-of statements that produce temps feeding ensure_init or
    // __assume_initialized call args. We identify by dest temp, not
    // source local, so only the specific "_tmp = &x" preparing the call arg
    // is exempted — other uses of &x in the same block are still checked.
    //
    // Steps:
    // 1. Collect temp bases from the terminator's exempt arg operands.
    // 2. Match those temps to AddressOf/Ref statements in the block.
    // 3. At check time, skip Ref/AddressOf rvalues whose dest is in the set.
    llvm::DenseSet<LocalId> EnsureInitArgTemps;
    if (BB.Term.K == Terminator::Call) {
      const auto &CD = BB.Term.getCall();
      llvm::DenseSet<LocalId> ExemptArgBases;
      if (CD.Decl &&
          CD.Decl->getBuiltinID() == Builtin::BI__assume_initialized) {
        for (const Operand &Arg : CD.Args)
          if (Arg.K == Operand::Copy || Arg.K == Operand::Move)
            ExemptArgBases.insert(Arg.getPlace().Base);
      }
      unsigned NumParams = CD.Decl
                               ? CD.Decl->getNumParams()
                               : (CD.CalleeProtoType
                                      ? CD.CalleeProtoType->getNumParams()
                                      : 0);
      for (unsigned I = 0; I < NumParams && I < CD.Args.size(); ++I) {
        bool IsAI = false;
        if (CD.Decl && I < CD.Decl->getNumParams())
          IsAI = CD.Decl->getParamDecl(I)->hasAttr<EnsureInitAttr>();
        if (!IsAI && CD.CalleeProtoType &&
            CD.CalleeProtoType->hasExtParameterInfos() &&
            I < CD.CalleeProtoType->getNumParams())
          IsAI = CD.CalleeProtoType->getExtParameterInfo(I).isEnsureInit();
        if (IsAI && (CD.Args[I].K == Operand::Copy ||
                     CD.Args[I].K == Operand::Move))
          ExemptArgBases.insert(CD.Args[I].getPlace().Base);
      }
      // Now find statements that are AddressOf/Ref producing these temps.
      for (const Statement &S : BB.Statements) {
        if (S.K != Statement::Assign || !S.getAssign().Dest.isLocal())
          continue;
        LocalId DestId = S.getAssign().Dest.Base;
        if (!ExemptArgBases.count(DestId))
          continue;
        const Rvalue &Src = S.getAssign().Src;
        if (Src.K == Rvalue::AddressOf || Src.K == Rvalue::Ref)
          EnsureInitArgTemps.insert(DestId);
      }
    }

    // Track block-local temp aliases of ensure_init params for deref checking.
    // Maps temp local → original ensure_init param.
    llvm::DenseMap<LocalId, LocalId> TempToEnsureInitParam;

    for (const Statement &S : BB.Statements) {
      // Check operands used in this statement
      if (S.K == Statement::Assign) {
        // Only check uses in _Safe zones
        if (CheckAllZones || S.SafeZone == SZ_Safe) {
          // Check the source operands
          const Rvalue &Src = S.getAssign().Src;
          switch (Src.K) {
          case Rvalue::Use:
            checkOperand(Src.getUse().Op, State, S.Loc, Diags);
            break;
          case Rvalue::BinaryOp:
            checkOperand(Src.getBinOp().LHS, State, S.Loc, Diags);
            checkOperand(Src.getBinOp().RHS, State, S.Loc, Diags);
            break;
          case Rvalue::UnaryOp:
            checkOperand(Src.getUnOp().Sub, State, S.Loc, Diags);
            break;
          case Rvalue::Cast:
            checkOperand(Src.getCast().Op, State, S.Loc, Diags);
            break;
          case Rvalue::Aggregate:
            for (const Operand &Field : Src.getAgg().Fields)
              checkOperand(Field, State, S.Loc, Diags);
            break;
          case Rvalue::Array:
            for (const Operand &El : Src.getArray().Elements)
              checkOperand(El, State, S.Loc, Diags);
            break;
          case Rvalue::Ref: {
            const Place &P = Src.getRef().P;
            // Exempt if dest temp feeds an ensure_init/__assume_initialized arg.
            if (!S.getAssign().Dest.isLocal() ||
                !EnsureInitArgTemps.count(S.getAssign().Dest.Base))
              checkOperand(Operand::createCopy(P), State, S.Loc, Diags);
            break;
          }
          case Rvalue::AddressOf: {
            const Place &P = Src.getAddrOf().P;
            if (!S.getAssign().Dest.isLocal() ||
                !EnsureInitArgTemps.count(S.getAssign().Dest.Base))
              checkOperand(Operand::createCopy(P), State, S.Loc, Diags);
            break;
          }
          case Rvalue::NullPtr:
          case Rvalue::SizeOf:
            break;
          }
        }

        // Callee-side ensure_init: check pointer reassignment (zone-independent).
        // If dest is a bare local that is an ensure_init param, it's being
        // reassigned. Only error if *param has not yet been initialized —
        // once the contract is fulfilled, the pointer can be freely reused.
        if (S.getAssign().Dest.isLocal()) {
          LocalId DestId = S.getAssign().Dest.Base;
          auto DerefIt = State.EnsureInitDerefStates.find(DestId);
          if (DerefIt != State.EnsureInitDerefStates.end() &&
              DerefIt->second != InitState::Initialized) {
            const LocalDecl &LD = B.getLocal(DestId);
            if (!LD.IsTemp && !LD.Name.empty()) {
              Diags.emplace_back(InitDiagKind::EnsureInitPtrAliased,
                                 S.Loc.isValid() ? S.Loc : LD.DeclLoc,
                                 LD.Name);
            }
          }
        }

        // Callee-side ensure_init: reject aliasing the pointer before *param
        // is initialized. Copying an ensure_init param into a named variable
        // creates an alias the analysis cannot track across blocks.
        // Temp copies (compiler-generated) are tracked for same-block deref
        // checking but not rejected.
        if (S.getAssign().Dest.isLocal()) {
          LocalId DestId = S.getAssign().Dest.Base;
          const Rvalue &Src = S.getAssign().Src;
          if (Src.K == Rvalue::Use) {
            const Operand &Op = Src.getUse().Op;
            if (Op.K == Operand::Copy && Op.getPlace().Projections.empty()) {
              LocalId SrcId = Op.getPlace().Base;
              // Check direct ensure_init param or transitive temp alias.
              LocalId ParamId = SrcId;
              auto AliasIt = TempToEnsureInitParam.find(SrcId);
              if (AliasIt != TempToEnsureInitParam.end())
                ParamId = AliasIt->second;
              auto DerefIt = State.EnsureInitDerefStates.find(ParamId);
              if (DerefIt != State.EnsureInitDerefStates.end() &&
                  DerefIt->second != InitState::Initialized) {
                const LocalDecl &DestLD = B.getLocal(DestId);
                if (DestLD.IsTemp) {
                  // Track temp alias for same-block deref checking.
                  TempToEnsureInitParam[DestId] = ParamId;
                } else if (!DestLD.Name.empty()) {
                  // Named variable: reject aliasing before init.
                  const LocalDecl &ParamLD = B.getLocal(ParamId);
                  if (!ParamLD.Name.empty()) {
                    Diags.emplace_back(InitDiagKind::EnsureInitPtrAliased,
                                       S.Loc.isValid() ? S.Loc : DestLD.DeclLoc,
                                       ParamLD.Name);
                  }
                }
              }
            }
          }
        }

        // Check deref reads of ensure_init params (*out or *_tmp before init).
        // Handles both direct derefs and same-block temp alias derefs.
        {
          auto checkDerefRead = [&](const Operand &Op) {
            if (Op.K == Operand::Constant)
              return;
            const Place &P = Op.getPlace();
            if (P.Projections.empty() ||
                P.Projections[0].K != ProjectionElem::Deref)
              return;
            // Resolve temp alias to the original ensure_init param.
            LocalId Base = P.Base;
            auto AliasIt = TempToEnsureInitParam.find(Base);
            LocalId ParamId = (AliasIt != TempToEnsureInitParam.end())
                                  ? AliasIt->second
                                  : Base;
            auto DerefIt = State.EnsureInitDerefStates.find(ParamId);
            if (DerefIt == State.EnsureInitDerefStates.end())
              return;
            if (DerefIt->second != InitState::Initialized) {
              const LocalDecl &ParamLD = B.getLocal(ParamId);
              if (!ParamLD.Name.empty()) {
                Diags.emplace_back(InitDiagKind::EnsureInitDerefReadUninit,
                                   S.Loc.isValid() ? S.Loc : ParamLD.DeclLoc,
                                   ParamLD.Name);
              }
            }
          };

          const Rvalue &Src = S.getAssign().Src;
          switch (Src.K) {
          case Rvalue::Use:
            checkDerefRead(Src.getUse().Op);
            break;
          case Rvalue::BinaryOp:
            checkDerefRead(Src.getBinOp().LHS);
            checkDerefRead(Src.getBinOp().RHS);
            break;
          case Rvalue::UnaryOp:
            checkDerefRead(Src.getUnOp().Sub);
            break;
          case Rvalue::Cast:
            checkDerefRead(Src.getCast().Op);
            break;
          case Rvalue::Aggregate:
            for (const Operand &Field : Src.getAgg().Fields)
              checkDerefRead(Field);
            break;
          case Rvalue::Array:
            for (const Operand &El : Src.getArray().Elements)
              checkDerefRead(El);
            break;
          default:
            break;
          }
        }
      }

      // Apply transfer function to update state
      transferStatement(S, State);
    }

    // Check terminator operands
    const Terminator &T = BB.Term;
    if (T.K == Terminator::Call && (CheckAllZones || T.SafeZone == SZ_Safe)) {
      const auto &CD = T.getCall();

      // Exempt ensure_init and __assume_initialized args from
      // the uninit check — these accept uninitialized addresses by design.
      llvm::DenseSet<unsigned> ExemptArgIndices;
      if (CD.Decl &&
          CD.Decl->getBuiltinID() == Builtin::BI__assume_initialized) {
        ExemptArgIndices.insert(0);
      }
      unsigned NumParams = CD.Decl
                               ? CD.Decl->getNumParams()
                               : (CD.CalleeProtoType
                                      ? CD.CalleeProtoType->getNumParams()
                                      : 0);
      for (unsigned I = 0; I < NumParams; ++I) {
        bool IsAI = false;
        if (CD.Decl && I < CD.Decl->getNumParams())
          IsAI = CD.Decl->getParamDecl(I)->hasAttr<EnsureInitAttr>();
        if (!IsAI && CD.CalleeProtoType &&
            CD.CalleeProtoType->hasExtParameterInfos() &&
            I < CD.CalleeProtoType->getNumParams())
          IsAI = CD.CalleeProtoType->getExtParameterInfo(I).isEnsureInit();
        if (IsAI)
          ExemptArgIndices.insert(I);
      }

      for (unsigned I = 0; I < CD.Args.size(); ++I) {
        if (!ExemptArgIndices.count(I))
          checkOperand(CD.Args[I], State, T.Loc, Diags);
      }
    }

    // Check return slot at Return terminator 
    if (T.K == Terminator::Return && (CheckAllZones || T.SafeZone == SZ_Safe)) {
      if (!B.Locals[0].Ty->isVoidType()) {
        InitState RetState = getInitState(State, LocalId{0});
        if (RetState == InitState::Uninitialized) {
          Diags.emplace_back(InitDiagKind::ReturnUninit, T.Loc,
                             B.SourceFD ? B.SourceFD->getNameAsString()
                                        : "<return>");
        } else if (RetState == InitState::MaybeInit) {
          Diags.emplace_back(InitDiagKind::ReturnMaybeUninit, T.Loc,
                             B.SourceFD ? B.SourceFD->getNameAsString()
                                        : "<return>");
        }
      }

      // Callee-side ensure_init: verify *param is initialized at return
      for (const auto &Entry : State.EnsureInitDerefStates) {
        const LocalDecl &LD = B.getLocal(Entry.first);
        SourceLocation DiagLoc = T.Loc.isValid() ? T.Loc
            : (B.SourceFD ? B.SourceFD->getBodyRBrace() : SourceLocation());
        if (Entry.second == InitState::Uninitialized) {
          Diags.emplace_back(InitDiagKind::EnsureInitNotInit, DiagLoc,
                             LD.Name);
        } else if (Entry.second == InitState::MaybeInit) {
          Diags.emplace_back(InitDiagKind::EnsureInitMaybeNotInit, DiagLoc,
                             LD.Name);
        }
      }
    }

    // Also check ensure_init contract at Return outside _Safe zones
    if (T.K == Terminator::Return && T.SafeZone != SZ_Safe) {
      for (const auto &Entry : State.EnsureInitDerefStates) {
        const LocalDecl &LD = B.getLocal(Entry.first);
        SourceLocation DiagLoc = T.Loc.isValid() ? T.Loc
            : (B.SourceFD ? B.SourceFD->getBodyRBrace() : SourceLocation());
        if (Entry.second == InitState::Uninitialized) {
          Diags.emplace_back(InitDiagKind::EnsureInitNotInit, DiagLoc,
                             LD.Name);
        } else if (Entry.second == InitState::MaybeInit) {
          Diags.emplace_back(InitDiagKind::EnsureInitMaybeNotInit, DiagLoc,
                             LD.Name);
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// Entry Point
//===----------------------------------------------------------------------===//

void bscir::runInitAnalysis(const Body &B,
                            SmallVectorImpl<InitDiagInfo> &Diags,
                            bool CheckAllZones) {
  InitAnalysis Analysis(B, CheckAllZones);
  Analysis.run(Diags);
}

#endif // ENABLE_BSC
