/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "code_generator_mips64.h"

#include "arch/mips64/asm_support_mips64.h"
#include "art_method.h"
#include "class_table.h"
#include "code_generator_utils.h"
#include "compiled_method.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gc/accounting/card_table.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "intrinsics_mips64.h"
#include "linker/linker_patch.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "offsets.h"
#include "stack_map_stream.h"
#include "thread.h"
#include "utils/assembler.h"
#include "utils/mips64/assembler_mips64.h"
#include "utils/stack_checks.h"

namespace art {
namespace mips64 {

static constexpr int kCurrentMethodStackOffset = 0;
static constexpr GpuRegister kMethodRegisterArgument = A0;

// Flags controlling the use of thunks for Baker read barriers.
constexpr bool kBakerReadBarrierThunksEnableForFields = true;
constexpr bool kBakerReadBarrierThunksEnableForArrays = true;
constexpr bool kBakerReadBarrierThunksEnableForGcRoots = true;

Location Mips64ReturnLocation(DataType::Type return_type) {
  switch (return_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kUint32:
    case DataType::Type::kInt32:
    case DataType::Type::kReference:
    case DataType::Type::kUint64:
    case DataType::Type::kInt64:
      return Location::RegisterLocation(V0);

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      return Location::FpuRegisterLocation(F0);

    case DataType::Type::kVoid:
      return Location();
  }
  UNREACHABLE();
}

Location InvokeDexCallingConventionVisitorMIPS64::GetReturnLocation(DataType::Type type) const {
  return Mips64ReturnLocation(type);
}

Location InvokeDexCallingConventionVisitorMIPS64::GetMethodLocation() const {
  return Location::RegisterLocation(kMethodRegisterArgument);
}

Location InvokeDexCallingConventionVisitorMIPS64::GetNextLocation(DataType::Type type) {
  Location next_location;
  if (type == DataType::Type::kVoid) {
    LOG(FATAL) << "Unexpected parameter type " << type;
  }

  if (DataType::IsFloatingPointType(type) &&
      (float_index_ < calling_convention.GetNumberOfFpuRegisters())) {
    next_location = Location::FpuRegisterLocation(
        calling_convention.GetFpuRegisterAt(float_index_++));
    gp_index_++;
  } else if (!DataType::IsFloatingPointType(type) &&
             (gp_index_ < calling_convention.GetNumberOfRegisters())) {
    next_location = Location::RegisterLocation(calling_convention.GetRegisterAt(gp_index_++));
    float_index_++;
  } else {
    size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
    next_location = DataType::Is64BitType(type) ? Location::DoubleStackSlot(stack_offset)
                                                : Location::StackSlot(stack_offset);
  }

  // Space on the stack is reserved for all arguments.
  stack_index_ += DataType::Is64BitType(type) ? 2 : 1;

  return next_location;
}

Location InvokeRuntimeCallingConvention::GetReturnLocation(DataType::Type type) {
  return Mips64ReturnLocation(type);
}

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<CodeGeneratorMIPS64*>(codegen)->GetAssembler()->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMips64PointerSize, x).Int32Value()

class BoundsCheckSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit BoundsCheckSlowPathMIPS64(HBoundsCheck* instruction) : SlowPathCodeMIPS64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(locations->InAt(0),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kInt32,
                               locations->InAt(1),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kInt32);
    QuickEntrypointEnum entrypoint = instruction_->AsBoundsCheck()->IsStringCharAt()
        ? kQuickThrowStringBounds
        : kQuickThrowArrayBounds;
    mips64_codegen->InvokeRuntime(entrypoint, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowStringBounds, void, int32_t, int32_t>();
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "BoundsCheckSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathMIPS64);
};

class DivZeroCheckSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit DivZeroCheckSlowPathMIPS64(HDivZeroCheck* instruction)
      : SlowPathCodeMIPS64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    __ Bind(GetEntryLabel());
    mips64_codegen->InvokeRuntime(kQuickThrowDivZero, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowDivZero, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "DivZeroCheckSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DivZeroCheckSlowPathMIPS64);
};

class LoadClassSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  LoadClassSlowPathMIPS64(HLoadClass* cls,
                          HInstruction* at,
                          uint32_t dex_pc,
                          bool do_clinit)
      : SlowPathCodeMIPS64(at),
        cls_(cls),
        dex_pc_(dex_pc),
        do_clinit_(do_clinit) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Location out = locations->Out();
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    InvokeRuntimeCallingConvention calling_convention;
    DCHECK_EQ(instruction_->IsLoadClass(), cls_ == instruction_);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    dex::TypeIndex type_index = cls_->GetTypeIndex();
    __ LoadConst32(calling_convention.GetRegisterAt(0), type_index.index_);
    QuickEntrypointEnum entrypoint = do_clinit_ ? kQuickInitializeStaticStorage
                                                : kQuickInitializeType;
    mips64_codegen->InvokeRuntime(entrypoint, instruction_, dex_pc_, this);
    if (do_clinit_) {
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, uint32_t>();
    } else {
      CheckEntrypointTypes<kQuickInitializeType, void*, uint32_t>();
    }

    // Move the class to the desired location.
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      DataType::Type type = instruction_->GetType();
      mips64_codegen->MoveLocation(out,
                                   Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                                   type);
    }
    RestoreLiveRegisters(codegen, locations);

    __ Bc(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadClassSlowPathMIPS64"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  // The dex PC of `at_`.
  const uint32_t dex_pc_;

  // Whether to initialize the class.
  const bool do_clinit_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathMIPS64);
};

class LoadStringSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit LoadStringSlowPathMIPS64(HLoadString* instruction)
      : SlowPathCodeMIPS64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    DCHECK(instruction_->IsLoadString());
    DCHECK_EQ(instruction_->AsLoadString()->GetLoadKind(), HLoadString::LoadKind::kBssEntry);
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    const dex::StringIndex string_index = instruction_->AsLoadString()->GetStringIndex();
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    InvokeRuntimeCallingConvention calling_convention;
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    __ LoadConst32(calling_convention.GetRegisterAt(0), string_index.index_);
    mips64_codegen->InvokeRuntime(kQuickResolveString,
                                  instruction_,
                                  instruction_->GetDexPc(),
                                  this);
    CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();

    DataType::Type type = instruction_->GetType();
    mips64_codegen->MoveLocation(locations->Out(),
                                 Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                                 type);
    RestoreLiveRegisters(codegen, locations);

    __ Bc(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadStringSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadStringSlowPathMIPS64);
};

class NullCheckSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit NullCheckSlowPathMIPS64(HNullCheck* instr) : SlowPathCodeMIPS64(instr) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    mips64_codegen->InvokeRuntime(kQuickThrowNullPointer,
                                  instruction_,
                                  instruction_->GetDexPc(),
                                  this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "NullCheckSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathMIPS64);
};

class SuspendCheckSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  SuspendCheckSlowPathMIPS64(HSuspendCheck* instruction, HBasicBlock* successor)
      : SlowPathCodeMIPS64(instruction), successor_(successor) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);     // Only saves live vector registers for SIMD.
    mips64_codegen->InvokeRuntime(kQuickTestSuspend, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickTestSuspend, void, void>();
    RestoreLiveRegisters(codegen, locations);  // Only restores live vector registers for SIMD.
    if (successor_ == nullptr) {
      __ Bc(GetReturnLabel());
    } else {
      __ Bc(mips64_codegen->GetLabelOf(successor_));
    }
  }

  Mips64Label* GetReturnLabel() {
    DCHECK(successor_ == nullptr);
    return &return_label_;
  }

  const char* GetDescription() const OVERRIDE { return "SuspendCheckSlowPathMIPS64"; }

  HBasicBlock* GetSuccessor() const {
    return successor_;
  }

 private:
  // If not null, the block to branch to after the suspend check.
  HBasicBlock* const successor_;

  // If `successor_` is null, the label to branch to after the suspend check.
  Mips64Label return_label_;

  DISALLOW_COPY_AND_ASSIGN(SuspendCheckSlowPathMIPS64);
};

class TypeCheckSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit TypeCheckSlowPathMIPS64(HInstruction* instruction, bool is_fatal)
      : SlowPathCodeMIPS64(instruction), is_fatal_(is_fatal) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();

    uint32_t dex_pc = instruction_->GetDexPc();
    DCHECK(instruction_->IsCheckCast()
           || !locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);

    __ Bind(GetEntryLabel());
    if (!is_fatal_ || instruction_->CanThrowIntoCatchBlock()) {
      SaveLiveRegisters(codegen, locations);
    }

    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(locations->InAt(0),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kReference,
                               locations->InAt(1),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kReference);
    if (instruction_->IsInstanceOf()) {
      mips64_codegen->InvokeRuntime(kQuickInstanceofNonTrivial, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickInstanceofNonTrivial, size_t, mirror::Object*, mirror::Class*>();
      DataType::Type ret_type = instruction_->GetType();
      Location ret_loc = calling_convention.GetReturnLocation(ret_type);
      mips64_codegen->MoveLocation(locations->Out(), ret_loc, ret_type);
    } else {
      DCHECK(instruction_->IsCheckCast());
      mips64_codegen->InvokeRuntime(kQuickCheckInstanceOf, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickCheckInstanceOf, void, mirror::Object*, mirror::Class*>();
    }

    if (!is_fatal_) {
      RestoreLiveRegisters(codegen, locations);
      __ Bc(GetExitLabel());
    }
  }

  const char* GetDescription() const OVERRIDE { return "TypeCheckSlowPathMIPS64"; }

  bool IsFatal() const OVERRIDE { return is_fatal_; }

 private:
  const bool is_fatal_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckSlowPathMIPS64);
};

class DeoptimizationSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit DeoptimizationSlowPathMIPS64(HDeoptimize* instruction)
    : SlowPathCodeMIPS64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    __ Bind(GetEntryLabel());
      LocationSummary* locations = instruction_->GetLocations();
    SaveLiveRegisters(codegen, locations);
    InvokeRuntimeCallingConvention calling_convention;
    __ LoadConst32(calling_convention.GetRegisterAt(0),
                   static_cast<uint32_t>(instruction_->AsDeoptimize()->GetDeoptimizationKind()));
    mips64_codegen->InvokeRuntime(kQuickDeoptimize, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickDeoptimize, void, DeoptimizationKind>();
  }

  const char* GetDescription() const OVERRIDE { return "DeoptimizationSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeoptimizationSlowPathMIPS64);
};

class ArraySetSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  explicit ArraySetSlowPathMIPS64(HInstruction* instruction) : SlowPathCodeMIPS64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());
    parallel_move.AddMove(
        locations->InAt(0),
        Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
        DataType::Type::kReference,
        nullptr);
    parallel_move.AddMove(
        locations->InAt(1),
        Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
        DataType::Type::kInt32,
        nullptr);
    parallel_move.AddMove(
        locations->InAt(2),
        Location::RegisterLocation(calling_convention.GetRegisterAt(2)),
        DataType::Type::kReference,
        nullptr);
    codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);

    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    mips64_codegen->InvokeRuntime(kQuickAputObject, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickAputObject, void, mirror::Array*, int32_t, mirror::Object*>();
    RestoreLiveRegisters(codegen, locations);
    __ Bc(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ArraySetSlowPathMIPS64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArraySetSlowPathMIPS64);
};

// Slow path marking an object reference `ref` during a read
// barrier. The field `obj.field` in the object `obj` holding this
// reference does not get updated by this slow path after marking (see
// ReadBarrierMarkAndUpdateFieldSlowPathMIPS64 below for that).
//
// This means that after the execution of this slow path, `ref` will
// always be up-to-date, but `obj.field` may not; i.e., after the
// flip, `ref` will be a to-space reference, but `obj.field` will
// probably still be a from-space reference (unless it gets updated by
// another thread, or if another thread installed another object
// reference (different from `ref`) in `obj.field`).
//
// If `entrypoint` is a valid location it is assumed to already be
// holding the entrypoint. The case where the entrypoint is passed in
// is for the GcRoot read barrier.
class ReadBarrierMarkSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  ReadBarrierMarkSlowPathMIPS64(HInstruction* instruction,
                                Location ref,
                                Location entrypoint = Location::NoLocation())
      : SlowPathCodeMIPS64(instruction), ref_(ref), entrypoint_(entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathMIPS"; }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    GpuRegister ref_reg = ref_.AsRegister<GpuRegister>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_reg)) << ref_reg;
    DCHECK(instruction_->IsInstanceFieldGet() ||
           instruction_->IsStaticFieldGet() ||
           instruction_->IsArrayGet() ||
           instruction_->IsArraySet() ||
           instruction_->IsLoadClass() ||
           instruction_->IsLoadString() ||
           instruction_->IsInstanceOf() ||
           instruction_->IsCheckCast() ||
           (instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()) ||
           (instruction_->IsInvokeStaticOrDirect() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    DCHECK((V0 <= ref_reg && ref_reg <= T2) ||
           (S2 <= ref_reg && ref_reg <= S7) ||
           (ref_reg == S8)) << ref_reg;
    // "Compact" slow path, saving two moves.
    //
    // Instead of using the standard runtime calling convention (input
    // and output in A0 and V0 respectively):
    //
    //   A0 <- ref
    //   V0 <- ReadBarrierMark(A0)
    //   ref <- V0
    //
    // we just use rX (the register containing `ref`) as input and output
    // of a dedicated entrypoint:
    //
    //   rX <- ReadBarrierMarkRegX(rX)
    //
    if (entrypoint_.IsValid()) {
      mips64_codegen->ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction_, this);
      DCHECK_EQ(entrypoint_.AsRegister<GpuRegister>(), T9);
      __ Jalr(entrypoint_.AsRegister<GpuRegister>());
      __ Nop();
    } else {
      int32_t entry_point_offset =
          Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(ref_reg - 1);
      // This runtime call does not require a stack map.
      mips64_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset,
                                                          instruction_,
                                                          this);
    }
    __ Bc(GetExitLabel());
  }

 private:
  // The location (register) of the marked object reference.
  const Location ref_;

  // The location of the entrypoint if already loaded.
  const Location entrypoint_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathMIPS64);
};

// Slow path marking an object reference `ref` during a read barrier,
// and if needed, atomically updating the field `obj.field` in the
// object `obj` holding this reference after marking (contrary to
// ReadBarrierMarkSlowPathMIPS64 above, which never tries to update
// `obj.field`).
//
// This means that after the execution of this slow path, both `ref`
// and `obj.field` will be up-to-date; i.e., after the flip, both will
// hold the same to-space reference (unless another thread installed
// another object reference (different from `ref`) in `obj.field`).
class ReadBarrierMarkAndUpdateFieldSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  ReadBarrierMarkAndUpdateFieldSlowPathMIPS64(HInstruction* instruction,
                                              Location ref,
                                              GpuRegister obj,
                                              Location field_offset,
                                              GpuRegister temp1)
      : SlowPathCodeMIPS64(instruction),
        ref_(ref),
        obj_(obj),
        field_offset_(field_offset),
        temp1_(temp1) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "ReadBarrierMarkAndUpdateFieldSlowPathMIPS64";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    GpuRegister ref_reg = ref_.AsRegister<GpuRegister>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_reg)) << ref_reg;
    // This slow path is only used by the UnsafeCASObject intrinsic.
    DCHECK((instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking and field updating slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kUnsafeCASObject);
    DCHECK(field_offset_.IsRegister()) << field_offset_;

    __ Bind(GetEntryLabel());

    // Save the old reference.
    // Note that we cannot use AT or TMP to save the old reference, as those
    // are used by the code that follows, but we need the old reference after
    // the call to the ReadBarrierMarkRegX entry point.
    DCHECK_NE(temp1_, AT);
    DCHECK_NE(temp1_, TMP);
    __ Move(temp1_, ref_reg);

    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    DCHECK((V0 <= ref_reg && ref_reg <= T2) ||
           (S2 <= ref_reg && ref_reg <= S7) ||
           (ref_reg == S8)) << ref_reg;
    // "Compact" slow path, saving two moves.
    //
    // Instead of using the standard runtime calling convention (input
    // and output in A0 and V0 respectively):
    //
    //   A0 <- ref
    //   V0 <- ReadBarrierMark(A0)
    //   ref <- V0
    //
    // we just use rX (the register containing `ref`) as input and output
    // of a dedicated entrypoint:
    //
    //   rX <- ReadBarrierMarkRegX(rX)
    //
    int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(ref_reg - 1);
    // This runtime call does not require a stack map.
    mips64_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset,
                                                        instruction_,
                                                        this);

    // If the new reference is different from the old reference,
    // update the field in the holder (`*(obj_ + field_offset_)`).
    //
    // Note that this field could also hold a different object, if
    // another thread had concurrently changed it. In that case, the
    // the compare-and-set (CAS) loop below would abort, leaving the
    // field as-is.
    Mips64Label done;
    __ Beqc(temp1_, ref_reg, &done);

    // Update the the holder's field atomically.  This may fail if
    // mutator updates before us, but it's OK.  This is achieved
    // using a strong compare-and-set (CAS) operation with relaxed
    // memory synchronization ordering, where the expected value is
    // the old reference and the desired value is the new reference.

    // Convenience aliases.
    GpuRegister base = obj_;
    GpuRegister offset = field_offset_.AsRegister<GpuRegister>();
    GpuRegister expected = temp1_;
    GpuRegister value = ref_reg;
    GpuRegister tmp_ptr = TMP;      // Pointer to actual memory.
    GpuRegister tmp = AT;           // Value in memory.

    __ Daddu(tmp_ptr, base, offset);

    if (kPoisonHeapReferences) {
      __ PoisonHeapReference(expected);
      // Do not poison `value` if it is the same register as
      // `expected`, which has just been poisoned.
      if (value != expected) {
        __ PoisonHeapReference(value);
      }
    }

    // do {
    //   tmp = [r_ptr] - expected;
    // } while (tmp == 0 && failure([r_ptr] <- r_new_value));

    Mips64Label loop_head, exit_loop;
    __ Bind(&loop_head);
    __ Ll(tmp, tmp_ptr);
    // The LL instruction sign-extends the 32-bit value, but
    // 32-bit references must be zero-extended. Zero-extend `tmp`.
    __ Dext(tmp, tmp, 0, 32);
    __ Bnec(tmp, expected, &exit_loop);
    __ Move(tmp, value);
    __ Sc(tmp, tmp_ptr);
    __ Beqzc(tmp, &loop_head);
    __ Bind(&exit_loop);

    if (kPoisonHeapReferences) {
      __ UnpoisonHeapReference(expected);
      // Do not unpoison `value` if it is the same register as
      // `expected`, which has just been unpoisoned.
      if (value != expected) {
        __ UnpoisonHeapReference(value);
      }
    }

    __ Bind(&done);
    __ Bc(GetExitLabel());
  }

 private:
  // The location (register) of the marked object reference.
  const Location ref_;
  // The register containing the object holding the marked object reference field.
  const GpuRegister obj_;
  // The location of the offset of the marked reference field within `obj_`.
  Location field_offset_;

  const GpuRegister temp1_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkAndUpdateFieldSlowPathMIPS64);
};

// Slow path generating a read barrier for a heap reference.
class ReadBarrierForHeapReferenceSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  ReadBarrierForHeapReferenceSlowPathMIPS64(HInstruction* instruction,
                                            Location out,
                                            Location ref,
                                            Location obj,
                                            uint32_t offset,
                                            Location index)
      : SlowPathCodeMIPS64(instruction),
        out_(out),
        ref_(ref),
        obj_(obj),
        offset_(offset),
        index_(index) {
    DCHECK(kEmitCompilerReadBarrier);
    // If `obj` is equal to `out` or `ref`, it means the initial object
    // has been overwritten by (or after) the heap object reference load
    // to be instrumented, e.g.:
    //
    //   __ LoadFromOffset(kLoadWord, out, out, offset);
    //   codegen_->GenerateReadBarrierSlow(instruction, out_loc, out_loc, out_loc, offset);
    //
    // In that case, we have lost the information about the original
    // object, and the emitted read barrier cannot work properly.
    DCHECK(!obj.Equals(out)) << "obj=" << obj << " out=" << out;
    DCHECK(!obj.Equals(ref)) << "obj=" << obj << " ref=" << ref;
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();
    DataType::Type type = DataType::Type::kReference;
    GpuRegister reg_out = out_.AsRegister<GpuRegister>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(reg_out));
    DCHECK(instruction_->IsInstanceFieldGet() ||
           instruction_->IsStaticFieldGet() ||
           instruction_->IsArrayGet() ||
           instruction_->IsInstanceOf() ||
           instruction_->IsCheckCast() ||
           (instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier for heap reference slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    // We may have to change the index's value, but as `index_` is a
    // constant member (like other "inputs" of this slow path),
    // introduce a copy of it, `index`.
    Location index = index_;
    if (index_.IsValid()) {
      // Handle `index_` for HArrayGet and UnsafeGetObject/UnsafeGetObjectVolatile intrinsics.
      if (instruction_->IsArrayGet()) {
        // Compute the actual memory offset and store it in `index`.
        GpuRegister index_reg = index_.AsRegister<GpuRegister>();
        DCHECK(locations->GetLiveRegisters()->ContainsCoreRegister(index_reg));
        if (codegen->IsCoreCalleeSaveRegister(index_reg)) {
          // We are about to change the value of `index_reg` (see the
          // calls to art::mips64::Mips64Assembler::Sll and
          // art::mips64::MipsAssembler::Addiu32 below), but it has
          // not been saved by the previous call to
          // art::SlowPathCode::SaveLiveRegisters, as it is a
          // callee-save register --
          // art::SlowPathCode::SaveLiveRegisters does not consider
          // callee-save registers, as it has been designed with the
          // assumption that callee-save registers are supposed to be
          // handled by the called function.  So, as a callee-save
          // register, `index_reg` _would_ eventually be saved onto
          // the stack, but it would be too late: we would have
          // changed its value earlier.  Therefore, we manually save
          // it here into another freely available register,
          // `free_reg`, chosen of course among the caller-save
          // registers (as a callee-save `free_reg` register would
          // exhibit the same problem).
          //
          // Note we could have requested a temporary register from
          // the register allocator instead; but we prefer not to, as
          // this is a slow path, and we know we can find a
          // caller-save register that is available.
          GpuRegister free_reg = FindAvailableCallerSaveRegister(codegen);
          __ Move(free_reg, index_reg);
          index_reg = free_reg;
          index = Location::RegisterLocation(index_reg);
        } else {
          // The initial register stored in `index_` has already been
          // saved in the call to art::SlowPathCode::SaveLiveRegisters
          // (as it is not a callee-save register), so we can freely
          // use it.
        }
        // Shifting the index value contained in `index_reg` by the scale
        // factor (2) cannot overflow in practice, as the runtime is
        // unable to allocate object arrays with a size larger than
        // 2^26 - 1 (that is, 2^28 - 4 bytes).
        __ Sll(index_reg, index_reg, TIMES_4);
        static_assert(
            sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
            "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
        __ Addiu32(index_reg, index_reg, offset_);
      } else {
        // In the case of the UnsafeGetObject/UnsafeGetObjectVolatile
        // intrinsics, `index_` is not shifted by a scale factor of 2
        // (as in the case of ArrayGet), as it is actually an offset
        // to an object field within an object.
        DCHECK(instruction_->IsInvoke()) << instruction_->DebugName();
        DCHECK(instruction_->GetLocations()->Intrinsified());
        DCHECK((instruction_->AsInvoke()->GetIntrinsic() == Intrinsics::kUnsafeGetObject) ||
               (instruction_->AsInvoke()->GetIntrinsic() == Intrinsics::kUnsafeGetObjectVolatile))
            << instruction_->AsInvoke()->GetIntrinsic();
        DCHECK_EQ(offset_, 0U);
        DCHECK(index_.IsRegister());
      }
    }

    // We're moving two or three locations to locations that could
    // overlap, so we need a parallel move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());
    parallel_move.AddMove(ref_,
                          Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                          DataType::Type::kReference,
                          nullptr);
    parallel_move.AddMove(obj_,
                          Location::RegisterLocation(calling_convention.GetRegisterAt(1)),
                          DataType::Type::kReference,
                          nullptr);
    if (index.IsValid()) {
      parallel_move.AddMove(index,
                            Location::RegisterLocation(calling_convention.GetRegisterAt(2)),
                            DataType::Type::kInt32,
                            nullptr);
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
    } else {
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
      __ LoadConst32(calling_convention.GetRegisterAt(2), offset_);
    }
    mips64_codegen->InvokeRuntime(kQuickReadBarrierSlow,
                                  instruction_,
                                  instruction_->GetDexPc(),
                                  this);
    CheckEntrypointTypes<
        kQuickReadBarrierSlow, mirror::Object*, mirror::Object*, mirror::Object*, uint32_t>();
    mips64_codegen->MoveLocation(out_, calling_convention.GetReturnLocation(type), type);

    RestoreLiveRegisters(codegen, locations);
    __ Bc(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE {
    return "ReadBarrierForHeapReferenceSlowPathMIPS64";
  }

 private:
  GpuRegister FindAvailableCallerSaveRegister(CodeGenerator* codegen) {
    size_t ref = static_cast<int>(ref_.AsRegister<GpuRegister>());
    size_t obj = static_cast<int>(obj_.AsRegister<GpuRegister>());
    for (size_t i = 0, e = codegen->GetNumberOfCoreRegisters(); i < e; ++i) {
      if (i != ref &&
          i != obj &&
          !codegen->IsCoreCalleeSaveRegister(i) &&
          !codegen->IsBlockedCoreRegister(i)) {
        return static_cast<GpuRegister>(i);
      }
    }
    // We shall never fail to find a free caller-save register, as
    // there are more than two core caller-save registers on MIPS64
    // (meaning it is possible to find one which is different from
    // `ref` and `obj`).
    DCHECK_GT(codegen->GetNumberOfCoreCallerSaveRegisters(), 2u);
    LOG(FATAL) << "Could not find a free caller-save register";
    UNREACHABLE();
  }

  const Location out_;
  const Location ref_;
  const Location obj_;
  const uint32_t offset_;
  // An additional location containing an index to an array.
  // Only used for HArrayGet and the UnsafeGetObject &
  // UnsafeGetObjectVolatile intrinsics.
  const Location index_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForHeapReferenceSlowPathMIPS64);
};

// Slow path generating a read barrier for a GC root.
class ReadBarrierForRootSlowPathMIPS64 : public SlowPathCodeMIPS64 {
 public:
  ReadBarrierForRootSlowPathMIPS64(HInstruction* instruction, Location out, Location root)
      : SlowPathCodeMIPS64(instruction), out_(out), root_(root) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DataType::Type type = DataType::Type::kReference;
    GpuRegister reg_out = out_.AsRegister<GpuRegister>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(reg_out));
    DCHECK(instruction_->IsLoadClass() || instruction_->IsLoadString())
        << "Unexpected instruction in read barrier for GC root slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    CodeGeneratorMIPS64* mips64_codegen = down_cast<CodeGeneratorMIPS64*>(codegen);
    mips64_codegen->MoveLocation(Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                                 root_,
                                 DataType::Type::kReference);
    mips64_codegen->InvokeRuntime(kQuickReadBarrierForRootSlow,
                                  instruction_,
                                  instruction_->GetDexPc(),
                                  this);
    CheckEntrypointTypes<kQuickReadBarrierForRootSlow, mirror::Object*, GcRoot<mirror::Object>*>();
    mips64_codegen->MoveLocation(out_, calling_convention.GetReturnLocation(type), type);

    RestoreLiveRegisters(codegen, locations);
    __ Bc(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForRootSlowPathMIPS64"; }

 private:
  const Location out_;
  const Location root_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForRootSlowPathMIPS64);
};

CodeGeneratorMIPS64::CodeGeneratorMIPS64(HGraph* graph,
                                         const Mips64InstructionSetFeatures& isa_features,
                                         const CompilerOptions& compiler_options,
                                         OptimizingCompilerStats* stats)
    : CodeGenerator(graph,
                    kNumberOfGpuRegisters,
                    kNumberOfFpuRegisters,
                    /* number_of_register_pairs */ 0,
                    ComputeRegisterMask(reinterpret_cast<const int*>(kCoreCalleeSaves),
                                        arraysize(kCoreCalleeSaves)),
                    ComputeRegisterMask(reinterpret_cast<const int*>(kFpuCalleeSaves),
                                        arraysize(kFpuCalleeSaves)),
                    compiler_options,
                    stats),
      block_labels_(nullptr),
      location_builder_(graph, this),
      instruction_visitor_(graph, this),
      move_resolver_(graph->GetAllocator(), this),
      assembler_(graph->GetAllocator(), &isa_features),
      isa_features_(isa_features),
      uint32_literals_(std::less<uint32_t>(),
                       graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      uint64_literals_(std::less<uint64_t>(),
                       graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_method_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      method_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_type_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      type_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_string_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      string_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_string_patches_(StringReferenceValueComparator(),
                          graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_class_patches_(TypeReferenceValueComparator(),
                         graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)) {
  // Save RA (containing the return address) to mimic Quick.
  AddAllocatedRegister(Location::RegisterLocation(RA));
}

#undef __
// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<Mips64Assembler*>(GetAssembler())->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMips64PointerSize, x).Int32Value()

void CodeGeneratorMIPS64::Finalize(CodeAllocator* allocator) {
  // Ensure that we fix up branches.
  __ FinalizeCode();

  // Adjust native pc offsets in stack maps.
  StackMapStream* stack_map_stream = GetStackMapStream();
  for (size_t i = 0, num = stack_map_stream->GetNumberOfStackMaps(); i != num; ++i) {
    uint32_t old_position =
        stack_map_stream->GetStackMap(i).native_pc_code_offset.Uint32Value(InstructionSet::kMips64);
    uint32_t new_position = __ GetAdjustedPosition(old_position);
    DCHECK_GE(new_position, old_position);
    stack_map_stream->SetStackMapNativePcOffset(i, new_position);
  }

  // Adjust pc offsets for the disassembly information.
  if (disasm_info_ != nullptr) {
    GeneratedCodeInterval* frame_entry_interval = disasm_info_->GetFrameEntryInterval();
    frame_entry_interval->start = __ GetAdjustedPosition(frame_entry_interval->start);
    frame_entry_interval->end = __ GetAdjustedPosition(frame_entry_interval->end);
    for (auto& it : *disasm_info_->GetInstructionIntervals()) {
      it.second.start = __ GetAdjustedPosition(it.second.start);
      it.second.end = __ GetAdjustedPosition(it.second.end);
    }
    for (auto& it : *disasm_info_->GetSlowPathIntervals()) {
      it.code_interval.start = __ GetAdjustedPosition(it.code_interval.start);
      it.code_interval.end = __ GetAdjustedPosition(it.code_interval.end);
    }
  }

  CodeGenerator::Finalize(allocator);
}

Mips64Assembler* ParallelMoveResolverMIPS64::GetAssembler() const {
  return codegen_->GetAssembler();
}

void ParallelMoveResolverMIPS64::EmitMove(size_t index) {
  MoveOperands* move = moves_[index];
  codegen_->MoveLocation(move->GetDestination(), move->GetSource(), move->GetType());
}

void ParallelMoveResolverMIPS64::EmitSwap(size_t index) {
  MoveOperands* move = moves_[index];
  codegen_->SwapLocations(move->GetDestination(), move->GetSource(), move->GetType());
}

void ParallelMoveResolverMIPS64::RestoreScratch(int reg) {
  // Pop reg
  __ Ld(GpuRegister(reg), SP, 0);
  __ DecreaseFrameSize(kMips64DoublewordSize);
}

void ParallelMoveResolverMIPS64::SpillScratch(int reg) {
  // Push reg
  __ IncreaseFrameSize(kMips64DoublewordSize);
  __ Sd(GpuRegister(reg), SP, 0);
}

void ParallelMoveResolverMIPS64::Exchange(int index1, int index2, bool double_slot) {
  LoadOperandType load_type = double_slot ? kLoadDoubleword : kLoadWord;
  StoreOperandType store_type = double_slot ? kStoreDoubleword : kStoreWord;
  // Allocate a scratch register other than TMP, if available.
  // Else, spill V0 (arbitrary choice) and use it as a scratch register (it will be
  // automatically unspilled when the scratch scope object is destroyed).
  ScratchRegisterScope ensure_scratch(this, TMP, V0, codegen_->GetNumberOfCoreRegisters());
  // If V0 spills onto the stack, SP-relative offsets need to be adjusted.
  int stack_offset = ensure_scratch.IsSpilled() ? kMips64DoublewordSize : 0;
  __ LoadFromOffset(load_type,
                    GpuRegister(ensure_scratch.GetRegister()),
                    SP,
                    index1 + stack_offset);
  __ LoadFromOffset(load_type,
                    TMP,
                    SP,
                    index2 + stack_offset);
  __ StoreToOffset(store_type,
                   GpuRegister(ensure_scratch.GetRegister()),
                   SP,
                   index2 + stack_offset);
  __ StoreToOffset(store_type, TMP, SP, index1 + stack_offset);
}

void ParallelMoveResolverMIPS64::ExchangeQuadSlots(int index1, int index2) {
  __ LoadFpuFromOffset(kLoadQuadword, FTMP, SP, index1);
  __ LoadFpuFromOffset(kLoadQuadword, FTMP2, SP, index2);
  __ StoreFpuToOffset(kStoreQuadword, FTMP, SP, index2);
  __ StoreFpuToOffset(kStoreQuadword, FTMP2, SP, index1);
}

static dwarf::Reg DWARFReg(GpuRegister reg) {
  return dwarf::Reg::Mips64Core(static_cast<int>(reg));
}

static dwarf::Reg DWARFReg(FpuRegister reg) {
  return dwarf::Reg::Mips64Fp(static_cast<int>(reg));
}

void CodeGeneratorMIPS64::GenerateFrameEntry() {
  __ Bind(&frame_entry_label_);

  if (GetCompilerOptions().CountHotnessInCompiledCode()) {
    __ Lhu(TMP, kMethodRegisterArgument, ArtMethod::HotnessCountOffset().Int32Value());
    __ Addiu(TMP, TMP, 1);
    __ Sh(TMP, kMethodRegisterArgument, ArtMethod::HotnessCountOffset().Int32Value());
  }

  bool do_overflow_check =
      FrameNeedsStackCheck(GetFrameSize(), InstructionSet::kMips64) || !IsLeafMethod();

  if (do_overflow_check) {
    __ LoadFromOffset(
        kLoadWord,
        ZERO,
        SP,
        -static_cast<int32_t>(GetStackOverflowReservedBytes(InstructionSet::kMips64)));
    RecordPcInfo(nullptr, 0);
  }

  if (HasEmptyFrame()) {
    return;
  }

  // Make sure the frame size isn't unreasonably large.
  if (GetFrameSize() > GetStackOverflowReservedBytes(InstructionSet::kMips64)) {
    LOG(FATAL) << "Stack frame larger than "
        << GetStackOverflowReservedBytes(InstructionSet::kMips64) << " bytes";
  }

  // Spill callee-saved registers.

  uint32_t ofs = GetFrameSize();
  __ IncreaseFrameSize(ofs);

  for (int i = arraysize(kCoreCalleeSaves) - 1; i >= 0; --i) {
    GpuRegister reg = kCoreCalleeSaves[i];
    if (allocated_registers_.ContainsCoreRegister(reg)) {
      ofs -= kMips64DoublewordSize;
      __ StoreToOffset(kStoreDoubleword, reg, SP, ofs);
      __ cfi().RelOffset(DWARFReg(reg), ofs);
    }
  }

  for (int i = arraysize(kFpuCalleeSaves) - 1; i >= 0; --i) {
    FpuRegister reg = kFpuCalleeSaves[i];
    if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
      ofs -= kMips64DoublewordSize;
      __ StoreFpuToOffset(kStoreDoubleword, reg, SP, ofs);
      __ cfi().RelOffset(DWARFReg(reg), ofs);
    }
  }

  // Save the current method if we need it. Note that we do not
  // do this in HCurrentMethod, as the instruction might have been removed
  // in the SSA graph.
  if (RequiresCurrentMethod()) {
    __ StoreToOffset(kStoreDoubleword, kMethodRegisterArgument, SP, kCurrentMethodStackOffset);
  }

  if (GetGraph()->HasShouldDeoptimizeFlag()) {
    // Initialize should_deoptimize flag to 0.
    __ StoreToOffset(kStoreWord, ZERO, SP, GetStackOffsetOfShouldDeoptimizeFlag());
  }
}

void CodeGeneratorMIPS64::GenerateFrameExit() {
  __ cfi().RememberState();

  if (!HasEmptyFrame()) {
    // Restore callee-saved registers.

    // For better instruction scheduling restore RA before other registers.
    uint32_t ofs = GetFrameSize();
    for (int i = arraysize(kCoreCalleeSaves) - 1; i >= 0; --i) {
      GpuRegister reg = kCoreCalleeSaves[i];
      if (allocated_registers_.ContainsCoreRegister(reg)) {
        ofs -= kMips64DoublewordSize;
        __ LoadFromOffset(kLoadDoubleword, reg, SP, ofs);
        __ cfi().Restore(DWARFReg(reg));
      }
    }

    for (int i = arraysize(kFpuCalleeSaves) - 1; i >= 0; --i) {
      FpuRegister reg = kFpuCalleeSaves[i];
      if (allocated_registers_.ContainsFloatingPointRegister(reg)) {
        ofs -= kMips64DoublewordSize;
        __ LoadFpuFromOffset(kLoadDoubleword, reg, SP, ofs);
        __ cfi().Restore(DWARFReg(reg));
      }
    }

    __ DecreaseFrameSize(GetFrameSize());
  }

  __ Jic(RA, 0);

  __ cfi().RestoreState();
  __ cfi().DefCFAOffset(GetFrameSize());
}

void CodeGeneratorMIPS64::Bind(HBasicBlock* block) {
  __ Bind(GetLabelOf(block));
}

void CodeGeneratorMIPS64::MoveLocation(Location destination,
                                       Location source,
                                       DataType::Type dst_type) {
  if (source.Equals(destination)) {
    return;
  }

  // A valid move can always be inferred from the destination and source
  // locations. When moving from and to a register, the argument type can be
  // used to generate 32bit instead of 64bit moves.
  bool unspecified_type = (dst_type == DataType::Type::kVoid);
  DCHECK_EQ(unspecified_type, false);

  if (destination.IsRegister() || destination.IsFpuRegister()) {
    if (unspecified_type) {
      HConstant* src_cst = source.IsConstant() ? source.GetConstant() : nullptr;
      if (source.IsStackSlot() ||
          (src_cst != nullptr && (src_cst->IsIntConstant()
                                  || src_cst->IsFloatConstant()
                                  || src_cst->IsNullConstant()))) {
        // For stack slots and 32bit constants, a 64bit type is appropriate.
        dst_type = destination.IsRegister() ? DataType::Type::kInt32 : DataType::Type::kFloat32;
      } else {
        // If the source is a double stack slot or a 64bit constant, a 64bit
        // type is appropriate. Else the source is a register, and since the
        // type has not been specified, we chose a 64bit type to force a 64bit
        // move.
        dst_type = destination.IsRegister() ? DataType::Type::kInt64 : DataType::Type::kFloat64;
      }
    }
    DCHECK((destination.IsFpuRegister() && DataType::IsFloatingPointType(dst_type)) ||
           (destination.IsRegister() && !DataType::IsFloatingPointType(dst_type)));
    if (source.IsStackSlot() || source.IsDoubleStackSlot()) {
      // Move to GPR/FPR from stack
      LoadOperandType load_type = source.IsStackSlot() ? kLoadWord : kLoadDoubleword;
      if (DataType::IsFloatingPointType(dst_type)) {
        __ LoadFpuFromOffset(load_type,
                             destination.AsFpuRegister<FpuRegister>(),
                             SP,
                             source.GetStackIndex());
      } else {
        // TODO: use load_type = kLoadUnsignedWord when type == DataType::Type::kReference.
        __ LoadFromOffset(load_type,
                          destination.AsRegister<GpuRegister>(),
                          SP,
                          source.GetStackIndex());
      }
    } else if (source.IsSIMDStackSlot()) {
      __ LoadFpuFromOffset(kLoadQuadword,
                           destination.AsFpuRegister<FpuRegister>(),
                           SP,
                           source.GetStackIndex());
    } else if (source.IsConstant()) {
      // Move to GPR/FPR from constant
      GpuRegister gpr = AT;
      if (!DataType::IsFloatingPointType(dst_type)) {
        gpr = destination.AsRegister<GpuRegister>();
      }
      if (dst_type == DataType::Type::kInt32 || dst_type == DataType::Type::kFloat32) {
        int32_t value = GetInt32ValueOf(source.GetConstant()->AsConstant());
        if (DataType::IsFloatingPointType(dst_type) && value == 0) {
          gpr = ZERO;
        } else {
          __ LoadConst32(gpr, value);
        }
      } else {
        int64_t value = GetInt64ValueOf(source.GetConstant()->AsConstant());
        if (DataType::IsFloatingPointType(dst_type) && value == 0) {
          gpr = ZERO;
        } else {
          __ LoadConst64(gpr, value);
        }
      }
      if (dst_type == DataType::Type::kFloat32) {
        __ Mtc1(gpr, destination.AsFpuRegister<FpuRegister>());
      } else if (dst_type == DataType::Type::kFloat64) {
        __ Dmtc1(gpr, destination.AsFpuRegister<FpuRegister>());
      }
    } else if (source.IsRegister()) {
      if (destination.IsRegister()) {
        // Move to GPR from GPR
        __ Move(destination.AsRegister<GpuRegister>(), source.AsRegister<GpuRegister>());
      } else {
        DCHECK(destination.IsFpuRegister());
        if (DataType::Is64BitType(dst_type)) {
          __ Dmtc1(source.AsRegister<GpuRegister>(), destination.AsFpuRegister<FpuRegister>());
        } else {
          __ Mtc1(source.AsRegister<GpuRegister>(), destination.AsFpuRegister<FpuRegister>());
        }
      }
    } else if (source.IsFpuRegister()) {
      if (destination.IsFpuRegister()) {
        if (GetGraph()->HasSIMD()) {
          __ MoveV(VectorRegisterFrom(destination),
                   VectorRegisterFrom(source));
        } else {
          // Move to FPR from FPR
          if (dst_type == DataType::Type::kFloat32) {
            __ MovS(destination.AsFpuRegister<FpuRegister>(), source.AsFpuRegister<FpuRegister>());
          } else {
            DCHECK_EQ(dst_type, DataType::Type::kFloat64);
            __ MovD(destination.AsFpuRegister<FpuRegister>(), source.AsFpuRegister<FpuRegister>());
          }
        }
      } else {
        DCHECK(destination.IsRegister());
        if (DataType::Is64BitType(dst_type)) {
          __ Dmfc1(destination.AsRegister<GpuRegister>(), source.AsFpuRegister<FpuRegister>());
        } else {
          __ Mfc1(destination.AsRegister<GpuRegister>(), source.AsFpuRegister<FpuRegister>());
        }
      }
    }
  } else if (destination.IsSIMDStackSlot()) {
    if (source.IsFpuRegister()) {
      __ StoreFpuToOffset(kStoreQuadword,
                          source.AsFpuRegister<FpuRegister>(),
                          SP,
                          destination.GetStackIndex());
    } else {
      DCHECK(source.IsSIMDStackSlot());
      __ LoadFpuFromOffset(kLoadQuadword,
                           FTMP,
                           SP,
                           source.GetStackIndex());
      __ StoreFpuToOffset(kStoreQuadword,
                          FTMP,
                          SP,
                          destination.GetStackIndex());
    }
  } else {  // The destination is not a register. It must be a stack slot.
    DCHECK(destination.IsStackSlot() || destination.IsDoubleStackSlot());
    if (source.IsRegister() || source.IsFpuRegister()) {
      if (unspecified_type) {
        if (source.IsRegister()) {
          dst_type = destination.IsStackSlot() ? DataType::Type::kInt32 : DataType::Type::kInt64;
        } else {
          dst_type =
              destination.IsStackSlot() ? DataType::Type::kFloat32 : DataType::Type::kFloat64;
        }
      }
      DCHECK((destination.IsDoubleStackSlot() == DataType::Is64BitType(dst_type)) &&
             (source.IsFpuRegister() == DataType::IsFloatingPointType(dst_type)));
      // Move to stack from GPR/FPR
      StoreOperandType store_type = destination.IsStackSlot() ? kStoreWord : kStoreDoubleword;
      if (source.IsRegister()) {
        __ StoreToOffset(store_type,
                         source.AsRegister<GpuRegister>(),
                         SP,
                         destination.GetStackIndex());
      } else {
        __ StoreFpuToOffset(store_type,
                            source.AsFpuRegister<FpuRegister>(),
                            SP,
                            destination.GetStackIndex());
      }
    } else if (source.IsConstant()) {
      // Move to stack from constant
      HConstant* src_cst = source.GetConstant();
      StoreOperandType store_type = destination.IsStackSlot() ? kStoreWord : kStoreDoubleword;
      GpuRegister gpr = ZERO;
      if (destination.IsStackSlot()) {
        int32_t value = GetInt32ValueOf(src_cst->AsConstant());
        if (value != 0) {
          gpr = TMP;
          __ LoadConst32(gpr, value);
        }
      } else {
        DCHECK(destination.IsDoubleStackSlot());
        int64_t value = GetInt64ValueOf(src_cst->AsConstant());
        if (value != 0) {
          gpr = TMP;
          __ LoadConst64(gpr, value);
        }
      }
      __ StoreToOffset(store_type, gpr, SP, destination.GetStackIndex());
    } else {
      DCHECK(source.IsStackSlot() || source.IsDoubleStackSlot());
      DCHECK_EQ(source.IsDoubleStackSlot(), destination.IsDoubleStackSlot());
      // Move to stack from stack
      if (destination.IsStackSlot()) {
        __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex());
        __ StoreToOffset(kStoreWord, TMP, SP, destination.GetStackIndex());
      } else {
        __ LoadFromOffset(kLoadDoubleword, TMP, SP, source.GetStackIndex());
        __ StoreToOffset(kStoreDoubleword, TMP, SP, destination.GetStackIndex());
      }
    }
  }
}

void CodeGeneratorMIPS64::SwapLocations(Location loc1, Location loc2, DataType::Type type) {
  DCHECK(!loc1.IsConstant());
  DCHECK(!loc2.IsConstant());

  if (loc1.Equals(loc2)) {
    return;
  }

  bool is_slot1 = loc1.IsStackSlot() || loc1.IsDoubleStackSlot();
  bool is_slot2 = loc2.IsStackSlot() || loc2.IsDoubleStackSlot();
  bool is_simd1 = loc1.IsSIMDStackSlot();
  bool is_simd2 = loc2.IsSIMDStackSlot();
  bool is_fp_reg1 = loc1.IsFpuRegister();
  bool is_fp_reg2 = loc2.IsFpuRegister();

  if (loc2.IsRegister() && loc1.IsRegister()) {
    // Swap 2 GPRs
    GpuRegister r1 = loc1.AsRegister<GpuRegister>();
    GpuRegister r2 = loc2.AsRegister<GpuRegister>();
    __ Move(TMP, r2);
    __ Move(r2, r1);
    __ Move(r1, TMP);
  } else if (is_fp_reg2 && is_fp_reg1) {
    // Swap 2 FPRs
    if (GetGraph()->HasSIMD()) {
      __ MoveV(static_cast<VectorRegister>(FTMP), VectorRegisterFrom(loc1));
      __ MoveV(VectorRegisterFrom(loc1), VectorRegisterFrom(loc2));
      __ MoveV(VectorRegisterFrom(loc2), static_cast<VectorRegister>(FTMP));
    } else {
      FpuRegister r1 = loc1.AsFpuRegister<FpuRegister>();
      FpuRegister r2 = loc2.AsFpuRegister<FpuRegister>();
      if (type == DataType::Type::kFloat32) {
        __ MovS(FTMP, r1);
        __ MovS(r1, r2);
        __ MovS(r2, FTMP);
      } else {
        DCHECK_EQ(type, DataType::Type::kFloat64);
        __ MovD(FTMP, r1);
        __ MovD(r1, r2);
        __ MovD(r2, FTMP);
      }
    }
  } else if (is_slot1 != is_slot2) {
    // Swap GPR/FPR and stack slot
    Location reg_loc = is_slot1 ? loc2 : loc1;
    Location mem_loc = is_slot1 ? loc1 : loc2;
    LoadOperandType load_type = mem_loc.IsStackSlot() ? kLoadWord : kLoadDoubleword;
    StoreOperandType store_type = mem_loc.IsStackSlot() ? kStoreWord : kStoreDoubleword;
    // TODO: use load_type = kLoadUnsignedWord when type == DataType::Type::kReference.
    __ LoadFromOffset(load_type, TMP, SP, mem_loc.GetStackIndex());
    if (reg_loc.IsFpuRegister()) {
      __ StoreFpuToOffset(store_type,
                          reg_loc.AsFpuRegister<FpuRegister>(),
                          SP,
                          mem_loc.GetStackIndex());
      if (mem_loc.IsStackSlot()) {
        __ Mtc1(TMP, reg_loc.AsFpuRegister<FpuRegister>());
      } else {
        DCHECK(mem_loc.IsDoubleStackSlot());
        __ Dmtc1(TMP, reg_loc.AsFpuRegister<FpuRegister>());
      }
    } else {
      __ StoreToOffset(store_type, reg_loc.AsRegister<GpuRegister>(), SP, mem_loc.GetStackIndex());
      __ Move(reg_loc.AsRegister<GpuRegister>(), TMP);
    }
  } else if (is_slot1 && is_slot2) {
    move_resolver_.Exchange(loc1.GetStackIndex(),
                            loc2.GetStackIndex(),
                            loc1.IsDoubleStackSlot());
  } else if (is_simd1 && is_simd2) {
    move_resolver_.ExchangeQuadSlots(loc1.GetStackIndex(), loc2.GetStackIndex());
  } else if ((is_fp_reg1 && is_simd2) || (is_fp_reg2 && is_simd1)) {
    Location fp_reg_loc = is_fp_reg1 ? loc1 : loc2;
    Location mem_loc = is_fp_reg1 ? loc2 : loc1;
    __ LoadFpuFromOffset(kLoadQuadword, FTMP, SP, mem_loc.GetStackIndex());
    __ StoreFpuToOffset(kStoreQuadword,
                        fp_reg_loc.AsFpuRegister<FpuRegister>(),
                        SP,
                        mem_loc.GetStackIndex());
    __ MoveV(VectorRegisterFrom(fp_reg_loc), static_cast<VectorRegister>(FTMP));
  } else {
    LOG(FATAL) << "Unimplemented swap between locations " << loc1 << " and " << loc2;
  }
}

void CodeGeneratorMIPS64::MoveConstant(Location location, int32_t value) {
  DCHECK(location.IsRegister());
  __ LoadConst32(location.AsRegister<GpuRegister>(), value);
}

void CodeGeneratorMIPS64::AddLocationAsTemp(Location location, LocationSummary* locations) {
  if (location.IsRegister()) {
    locations->AddTemp(location);
  } else {
    UNIMPLEMENTED(FATAL) << "AddLocationAsTemp not implemented for location " << location;
  }
}

void CodeGeneratorMIPS64::MarkGCCard(GpuRegister object,
                                     GpuRegister value,
                                     bool value_can_be_null) {
  Mips64Label done;
  GpuRegister card = AT;
  GpuRegister temp = TMP;
  if (value_can_be_null) {
    __ Beqzc(value, &done);
  }
  __ LoadFromOffset(kLoadDoubleword,
                    card,
                    TR,
                    Thread::CardTableOffset<kMips64PointerSize>().Int32Value());
  __ Dsrl(temp, object, gc::accounting::CardTable::kCardShift);
  __ Daddu(temp, card, temp);
  __ Sb(card, temp, 0);
  if (value_can_be_null) {
    __ Bind(&done);
  }
}

template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
inline void CodeGeneratorMIPS64::EmitPcRelativeLinkerPatches(
    const ArenaDeque<PcRelativePatchInfo>& infos,
    ArenaVector<linker::LinkerPatch>* linker_patches) {
  for (const PcRelativePatchInfo& info : infos) {
    const DexFile* dex_file = info.target_dex_file;
    size_t offset_or_index = info.offset_or_index;
    DCHECK(info.label.IsBound());
    uint32_t literal_offset = __ GetLabelLocation(&info.label);
    const PcRelativePatchInfo& info_high = info.patch_info_high ? *info.patch_info_high : info;
    uint32_t pc_rel_offset = __ GetLabelLocation(&info_high.label);
    linker_patches->push_back(Factory(literal_offset, dex_file, pc_rel_offset, offset_or_index));
  }
}

void CodeGeneratorMIPS64::EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) {
  DCHECK(linker_patches->empty());
  size_t size =
      boot_image_method_patches_.size() +
      method_bss_entry_patches_.size() +
      boot_image_type_patches_.size() +
      type_bss_entry_patches_.size() +
      boot_image_string_patches_.size() +
      string_bss_entry_patches_.size();
  linker_patches->reserve(size);
  if (GetCompilerOptions().IsBootImage()) {
    EmitPcRelativeLinkerPatches<linker::LinkerPatch::RelativeMethodPatch>(
        boot_image_method_patches_, linker_patches);
    EmitPcRelativeLinkerPatches<linker::LinkerPatch::RelativeTypePatch>(
        boot_image_type_patches_, linker_patches);
    EmitPcRelativeLinkerPatches<linker::LinkerPatch::RelativeStringPatch>(
        boot_image_string_patches_, linker_patches);
  } else {
    DCHECK(boot_image_method_patches_.empty());
    EmitPcRelativeLinkerPatches<linker::LinkerPatch::TypeClassTablePatch>(
        boot_image_type_patches_, linker_patches);
    EmitPcRelativeLinkerPatches<linker::LinkerPatch::StringInternTablePatch>(
        boot_image_string_patches_, linker_patches);
  }
  EmitPcRelativeLinkerPatches<linker::LinkerPatch::MethodBssEntryPatch>(
      method_bss_entry_patches_, linker_patches);
  EmitPcRelativeLinkerPatches<linker::LinkerPatch::TypeBssEntryPatch>(
      type_bss_entry_patches_, linker_patches);
  EmitPcRelativeLinkerPatches<linker::LinkerPatch::StringBssEntryPatch>(
      string_bss_entry_patches_, linker_patches);
  DCHECK_EQ(size, linker_patches->size());
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewBootImageMethodPatch(
    MethodReference target_method,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &boot_image_method_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewMethodBssEntryPatch(
    MethodReference target_method,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &method_bss_entry_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewBootImageTypePatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, &boot_image_type_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewTypeBssEntryPatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, &type_bss_entry_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewBootImageStringPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      &dex_file, string_index.index_, info_high, &boot_image_string_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewStringBssEntryPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, string_index.index_, info_high, &string_bss_entry_patches_);
}

CodeGeneratorMIPS64::PcRelativePatchInfo* CodeGeneratorMIPS64::NewPcRelativePatch(
    const DexFile* dex_file,
    uint32_t offset_or_index,
    const PcRelativePatchInfo* info_high,
    ArenaDeque<PcRelativePatchInfo>* patches) {
  patches->emplace_back(dex_file, offset_or_index, info_high);
  return &patches->back();
}

Literal* CodeGeneratorMIPS64::DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map) {
  return map->GetOrCreate(
      value,
      [this, value]() { return __ NewLiteral<uint32_t>(value); });
}

Literal* CodeGeneratorMIPS64::DeduplicateUint64Literal(uint64_t value) {
  return uint64_literals_.GetOrCreate(
      value,
      [this, value]() { return __ NewLiteral<uint64_t>(value); });
}

Literal* CodeGeneratorMIPS64::DeduplicateBootImageAddressLiteral(uint64_t address) {
  return DeduplicateUint32Literal(dchecked_integral_cast<uint32_t>(address), &uint32_literals_);
}

void CodeGeneratorMIPS64::EmitPcRelativeAddressPlaceholderHigh(PcRelativePatchInfo* info_high,
                                                               GpuRegister out,
                                                               PcRelativePatchInfo* info_low) {
  DCHECK(!info_high->patch_info_high);
  __ Bind(&info_high->label);
  // Add the high half of a 32-bit offset to PC.
  __ Auipc(out, /* placeholder */ 0x1234);
  // A following instruction will add the sign-extended low half of the 32-bit
  // offset to `out` (e.g. ld, jialc, daddiu).
  if (info_low != nullptr) {
    DCHECK_EQ(info_low->patch_info_high, info_high);
    __ Bind(&info_low->label);
  }
}

Literal* CodeGeneratorMIPS64::DeduplicateJitStringLiteral(const DexFile& dex_file,
                                                          dex::StringIndex string_index,
                                                          Handle<mirror::String> handle) {
  ReserveJitStringRoot(StringReference(&dex_file, string_index), handle);
  return jit_string_patches_.GetOrCreate(
      StringReference(&dex_file, string_index),
      [this]() { return __ NewLiteral<uint32_t>(/* placeholder */ 0u); });
}

Literal* CodeGeneratorMIPS64::DeduplicateJitClassLiteral(const DexFile& dex_file,
                                                         dex::TypeIndex type_index,
                                                         Handle<mirror::Class> handle) {
  ReserveJitClassRoot(TypeReference(&dex_file, type_index), handle);
  return jit_class_patches_.GetOrCreate(
      TypeReference(&dex_file, type_index),
      [this]() { return __ NewLiteral<uint32_t>(/* placeholder */ 0u); });
}

void CodeGeneratorMIPS64::PatchJitRootUse(uint8_t* code,
                                          const uint8_t* roots_data,
                                          const Literal* literal,
                                          uint64_t index_in_table) const {
  uint32_t literal_offset = GetAssembler().GetLabelLocation(literal->GetLabel());
  uintptr_t address =
      reinterpret_cast<uintptr_t>(roots_data) + index_in_table * sizeof(GcRoot<mirror::Object>);
  reinterpret_cast<uint32_t*>(code + literal_offset)[0] = dchecked_integral_cast<uint32_t>(address);
}

void CodeGeneratorMIPS64::EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) {
  for (const auto& entry : jit_string_patches_) {
    const StringReference& string_reference = entry.first;
    Literal* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitStringRootIndex(string_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
  for (const auto& entry : jit_class_patches_) {
    const TypeReference& type_reference = entry.first;
    Literal* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitClassRootIndex(type_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
}

void CodeGeneratorMIPS64::SetupBlockedRegisters() const {
  // ZERO, K0, K1, GP, SP, RA are always reserved and can't be allocated.
  blocked_core_registers_[ZERO] = true;
  blocked_core_registers_[K0] = true;
  blocked_core_registers_[K1] = true;
  blocked_core_registers_[GP] = true;
  blocked_core_registers_[SP] = true;
  blocked_core_registers_[RA] = true;

  // AT, TMP(T8) and TMP2(T3) are used as temporary/scratch
  // registers (similar to how AT is used by MIPS assemblers).
  blocked_core_registers_[AT] = true;
  blocked_core_registers_[TMP] = true;
  blocked_core_registers_[TMP2] = true;
  blocked_fpu_registers_[FTMP] = true;

  if (GetInstructionSetFeatures().HasMsa()) {
    // To be used just for MSA instructions.
    blocked_fpu_registers_[FTMP2] = true;
  }

  // Reserve suspend and thread registers.
  blocked_core_registers_[S0] = true;
  blocked_core_registers_[TR] = true;

  // Reserve T9 for function calls
  blocked_core_registers_[T9] = true;

  if (GetGraph()->IsDebuggable()) {
    // Stubs do not save callee-save floating point registers. If the graph
    // is debuggable, we need to deal with these registers differently. For
    // now, just block them.
    for (size_t i = 0; i < arraysize(kFpuCalleeSaves); ++i) {
      blocked_fpu_registers_[kFpuCalleeSaves[i]] = true;
    }
  }
}

size_t CodeGeneratorMIPS64::SaveCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ StoreToOffset(kStoreDoubleword, GpuRegister(reg_id), SP, stack_index);
  return kMips64DoublewordSize;
}

size_t CodeGeneratorMIPS64::RestoreCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ LoadFromOffset(kLoadDoubleword, GpuRegister(reg_id), SP, stack_index);
  return kMips64DoublewordSize;
}

size_t CodeGeneratorMIPS64::SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  __ StoreFpuToOffset(GetGraph()->HasSIMD() ? kStoreQuadword : kStoreDoubleword,
                      FpuRegister(reg_id),
                      SP,
                      stack_index);
  return GetFloatingPointSpillSlotSize();
}

size_t CodeGeneratorMIPS64::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  __ LoadFpuFromOffset(GetGraph()->HasSIMD() ? kLoadQuadword : kLoadDoubleword,
                       FpuRegister(reg_id),
                       SP,
                       stack_index);
  return GetFloatingPointSpillSlotSize();
}

void CodeGeneratorMIPS64::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << GpuRegister(reg);
}

void CodeGeneratorMIPS64::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << FpuRegister(reg);
}

void CodeGeneratorMIPS64::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                        HInstruction* instruction,
                                        uint32_t dex_pc,
                                        SlowPathCode* slow_path) {
  ValidateInvokeRuntime(entrypoint, instruction, slow_path);
  GenerateInvokeRuntime(GetThreadOffset<kMips64PointerSize>(entrypoint).Int32Value());
  if (EntrypointRequiresStackMap(entrypoint)) {
    RecordPcInfo(instruction, dex_pc, slow_path);
  }
}

void CodeGeneratorMIPS64::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                              HInstruction* instruction,
                                                              SlowPathCode* slow_path) {
  ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction, slow_path);
  GenerateInvokeRuntime(entry_point_offset);
}

void CodeGeneratorMIPS64::GenerateInvokeRuntime(int32_t entry_point_offset) {
  __ LoadFromOffset(kLoadDoubleword, T9, TR, entry_point_offset);
  __ Jalr(T9);
  __ Nop();
}

void InstructionCodeGeneratorMIPS64::GenerateClassInitializationCheck(SlowPathCodeMIPS64* slow_path,
                                                                      GpuRegister class_reg) {
  constexpr size_t status_lsb_position = SubtypeCheckBits::BitStructSizeOf();
  const size_t status_byte_offset =
      mirror::Class::StatusOffset().SizeValue() + (status_lsb_position / kBitsPerByte);
  constexpr uint32_t shifted_initialized_value =
      enum_cast<uint32_t>(ClassStatus::kInitialized) << (status_lsb_position % kBitsPerByte);

  __ LoadFromOffset(kLoadUnsignedByte, TMP, class_reg, status_byte_offset);
  __ Sltiu(TMP, TMP, shifted_initialized_value);
  __ Bnezc(TMP, slow_path->GetEntryLabel());
  // Even if the initialized flag is set, we need to ensure consistent memory ordering.
  __ Sync(0);
  __ Bind(slow_path->GetExitLabel());
}

void InstructionCodeGeneratorMIPS64::GenerateMemoryBarrier(MemBarrierKind kind ATTRIBUTE_UNUSED) {
  __ Sync(0);  // only stype 0 is supported
}

void InstructionCodeGeneratorMIPS64::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                          HBasicBlock* successor) {
  SuspendCheckSlowPathMIPS64* slow_path =
      down_cast<SuspendCheckSlowPathMIPS64*>(instruction->GetSlowPath());

  if (slow_path == nullptr) {
    slow_path =
        new (codegen_->GetScopedAllocator()) SuspendCheckSlowPathMIPS64(instruction, successor);
    instruction->SetSlowPath(slow_path);
    codegen_->AddSlowPath(slow_path);
    if (successor != nullptr) {
      DCHECK(successor->IsLoopHeader());
    }
  } else {
    DCHECK_EQ(slow_path->GetSuccessor(), successor);
  }

  __ LoadFromOffset(kLoadUnsignedHalfword,
                    TMP,
                    TR,
                    Thread::ThreadFlagsOffset<kMips64PointerSize>().Int32Value());
  if (successor == nullptr) {
    __ Bnezc(TMP, slow_path->GetEntryLabel());
    __ Bind(slow_path->GetReturnLabel());
  } else {
    __ Beqzc(TMP, codegen_->GetLabelOf(successor));
    __ Bc(slow_path->GetEntryLabel());
    // slow_path will return to GetLabelOf(successor).
  }
}

InstructionCodeGeneratorMIPS64::InstructionCodeGeneratorMIPS64(HGraph* graph,
                                                               CodeGeneratorMIPS64* codegen)
      : InstructionCodeGenerator(graph, codegen),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void LocationsBuilderMIPS64::HandleBinaryOp(HBinaryOperation* instruction) {
  DCHECK_EQ(instruction->InputCount(), 2U);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  DataType::Type type = instruction->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* right = instruction->InputAt(1);
      bool can_use_imm = false;
      if (right->IsConstant()) {
        int64_t imm = CodeGenerator::GetInt64ValueOf(right->AsConstant());
        if (instruction->IsAnd() || instruction->IsOr() || instruction->IsXor()) {
          can_use_imm = IsUint<16>(imm);
        } else {
          DCHECK(instruction->IsAdd() || instruction->IsSub());
          bool single_use = right->GetUses().HasExactlyOneElement();
          if (instruction->IsSub()) {
            if (!(type == DataType::Type::kInt32 && imm == INT32_MIN)) {
              imm = -imm;
            }
          }
          if (type == DataType::Type::kInt32) {
            can_use_imm = IsInt<16>(imm) || (Low16Bits(imm) == 0) || single_use;
          } else {
            can_use_imm = IsInt<16>(imm) || (IsInt<32>(imm) && (Low16Bits(imm) == 0)) || single_use;
          }
        }
      }
      if (can_use_imm)
        locations->SetInAt(1, Location::ConstantLocation(right->AsConstant()));
      else
        locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      }
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected " << instruction->DebugName() << " type " << type;
  }
}

void InstructionCodeGeneratorMIPS64::HandleBinaryOp(HBinaryOperation* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
      GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
      Location rhs_location = locations->InAt(1);

      GpuRegister rhs_reg = ZERO;
      int64_t rhs_imm = 0;
      bool use_imm = rhs_location.IsConstant();
      if (use_imm) {
        rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
      } else {
        rhs_reg = rhs_location.AsRegister<GpuRegister>();
      }

      if (instruction->IsAnd()) {
        if (use_imm)
          __ Andi(dst, lhs, rhs_imm);
        else
          __ And(dst, lhs, rhs_reg);
      } else if (instruction->IsOr()) {
        if (use_imm)
          __ Ori(dst, lhs, rhs_imm);
        else
          __ Or(dst, lhs, rhs_reg);
      } else if (instruction->IsXor()) {
        if (use_imm)
          __ Xori(dst, lhs, rhs_imm);
        else
          __ Xor(dst, lhs, rhs_reg);
      } else if (instruction->IsAdd() || instruction->IsSub()) {
        if (instruction->IsSub()) {
          rhs_imm = -rhs_imm;
        }
        if (type == DataType::Type::kInt32) {
          if (use_imm) {
            if (IsInt<16>(rhs_imm)) {
              __ Addiu(dst, lhs, rhs_imm);
            } else {
              int16_t rhs_imm_high = High16Bits(rhs_imm);
              int16_t rhs_imm_low = Low16Bits(rhs_imm);
              if (rhs_imm_low < 0) {
                rhs_imm_high += 1;
              }
              __ Aui(dst, lhs, rhs_imm_high);
              if (rhs_imm_low != 0) {
                __ Addiu(dst, dst, rhs_imm_low);
              }
            }
          } else {
            if (instruction->IsAdd()) {
              __ Addu(dst, lhs, rhs_reg);
            } else {
              DCHECK(instruction->IsSub());
              __ Subu(dst, lhs, rhs_reg);
            }
          }
        } else {
          if (use_imm) {
            if (IsInt<16>(rhs_imm)) {
              __ Daddiu(dst, lhs, rhs_imm);
            } else if (IsInt<32>(rhs_imm)) {
              int16_t rhs_imm_high = High16Bits(rhs_imm);
              int16_t rhs_imm_low = Low16Bits(rhs_imm);
              bool overflow_hi16 = false;
              if (rhs_imm_low < 0) {
                rhs_imm_high += 1;
                overflow_hi16 = (rhs_imm_high == -32768);
              }
              __ Daui(dst, lhs, rhs_imm_high);
              if (rhs_imm_low != 0) {
                __ Daddiu(dst, dst, rhs_imm_low);
              }
              if (overflow_hi16) {
                __ Dahi(dst, 1);
              }
            } else {
              int16_t rhs_imm_low = Low16Bits(Low32Bits(rhs_imm));
              if (rhs_imm_low < 0) {
                rhs_imm += (INT64_C(1) << 16);
              }
              int16_t rhs_imm_upper = High16Bits(Low32Bits(rhs_imm));
              if (rhs_imm_upper < 0) {
                rhs_imm += (INT64_C(1) << 32);
              }
              int16_t rhs_imm_high = Low16Bits(High32Bits(rhs_imm));
              if (rhs_imm_high < 0) {
                rhs_imm += (INT64_C(1) << 48);
              }
              int16_t rhs_imm_top = High16Bits(High32Bits(rhs_imm));
              GpuRegister tmp = lhs;
              if (rhs_imm_low != 0) {
                __ Daddiu(dst, tmp, rhs_imm_low);
                tmp = dst;
              }
              // Dahi and Dati must use the same input and output register, so we have to initialize
              // the dst register using Daddiu or Daui, even when the intermediate value is zero:
              // Daui(dst, lhs, 0).
              if ((rhs_imm_upper != 0) || (rhs_imm_low == 0)) {
                __ Daui(dst, tmp, rhs_imm_upper);
              }
              if (rhs_imm_high != 0) {
                __ Dahi(dst, rhs_imm_high);
              }
              if (rhs_imm_top != 0) {
                __ Dati(dst, rhs_imm_top);
              }
            }
          } else if (instruction->IsAdd()) {
            __ Daddu(dst, lhs, rhs_reg);
          } else {
            DCHECK(instruction->IsSub());
            __ Dsubu(dst, lhs, rhs_reg);
          }
        }
      }
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
      FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
      FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
      if (instruction->IsAdd()) {
        if (type == DataType::Type::kFloat32)
          __ AddS(dst, lhs, rhs);
        else
          __ AddD(dst, lhs, rhs);
      } else if (instruction->IsSub()) {
        if (type == DataType::Type::kFloat32)
          __ SubS(dst, lhs, rhs);
        else
          __ SubD(dst, lhs, rhs);
      } else {
        LOG(FATAL) << "Unexpected floating-point binary operation";
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected binary operation type " << type;
  }
}

void LocationsBuilderMIPS64::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr() || instr->IsRor());

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instr);
  DataType::Type type = instr->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instr->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected shift type " << type;
  }
}

void InstructionCodeGeneratorMIPS64::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr() || instr->IsRor());
  LocationSummary* locations = instr->GetLocations();
  DataType::Type type = instr->GetType();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
      GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
      Location rhs_location = locations->InAt(1);

      GpuRegister rhs_reg = ZERO;
      int64_t rhs_imm = 0;
      bool use_imm = rhs_location.IsConstant();
      if (use_imm) {
        rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
      } else {
        rhs_reg = rhs_location.AsRegister<GpuRegister>();
      }

      if (use_imm) {
        uint32_t shift_value = rhs_imm &
            (type == DataType::Type::kInt32 ? kMaxIntShiftDistance : kMaxLongShiftDistance);

        if (shift_value == 0) {
          if (dst != lhs) {
            __ Move(dst, lhs);
          }
        } else if (type == DataType::Type::kInt32) {
          if (instr->IsShl()) {
            __ Sll(dst, lhs, shift_value);
          } else if (instr->IsShr()) {
            __ Sra(dst, lhs, shift_value);
          } else if (instr->IsUShr()) {
            __ Srl(dst, lhs, shift_value);
          } else {
            __ Rotr(dst, lhs, shift_value);
          }
        } else {
          if (shift_value < 32) {
            if (instr->IsShl()) {
              __ Dsll(dst, lhs, shift_value);
            } else if (instr->IsShr()) {
              __ Dsra(dst, lhs, shift_value);
            } else if (instr->IsUShr()) {
              __ Dsrl(dst, lhs, shift_value);
            } else {
              __ Drotr(dst, lhs, shift_value);
            }
          } else {
            shift_value -= 32;
            if (instr->IsShl()) {
              __ Dsll32(dst, lhs, shift_value);
            } else if (instr->IsShr()) {
              __ Dsra32(dst, lhs, shift_value);
            } else if (instr->IsUShr()) {
              __ Dsrl32(dst, lhs, shift_value);
            } else {
              __ Drotr32(dst, lhs, shift_value);
            }
          }
        }
      } else {
        if (type == DataType::Type::kInt32) {
          if (instr->IsShl()) {
            __ Sllv(dst, lhs, rhs_reg);
          } else if (instr->IsShr()) {
            __ Srav(dst, lhs, rhs_reg);
          } else if (instr->IsUShr()) {
            __ Srlv(dst, lhs, rhs_reg);
          } else {
            __ Rotrv(dst, lhs, rhs_reg);
          }
        } else {
          if (instr->IsShl()) {
            __ Dsllv(dst, lhs, rhs_reg);
          } else if (instr->IsShr()) {
            __ Dsrav(dst, lhs, rhs_reg);
          } else if (instr->IsUShr()) {
            __ Dsrlv(dst, lhs, rhs_reg);
          } else {
            __ Drotrv(dst, lhs, rhs_reg);
          }
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected shift operation type " << type;
  }
}

void LocationsBuilderMIPS64::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS64::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS64::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS64::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS64::VisitArrayGet(HArrayGet* instruction) {
  DataType::Type type = instruction->GetType();
  bool object_array_get_with_read_barrier =
      kEmitCompilerReadBarrier && (type == DataType::Type::kReference);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction,
                                                       object_array_get_with_read_barrier
                                                           ? LocationSummary::kCallOnSlowPath
                                                           : LocationSummary::kNoCall);
  if (object_array_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (DataType::IsFloatingPointType(type)) {
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    // The output overlaps in the case of an object array get with
    // read barriers enabled: we do not want the move to overwrite the
    // array's location, as we need it to emit the read barrier.
    locations->SetOut(Location::RequiresRegister(),
                      object_array_get_with_read_barrier
                          ? Location::kOutputOverlap
                          : Location::kNoOutputOverlap);
  }
  // We need a temporary register for the read barrier marking slow
  // path in CodeGeneratorMIPS64::GenerateArrayLoadWithBakerReadBarrier.
  if (object_array_get_with_read_barrier && kUseBakerReadBarrier) {
    bool temp_needed = instruction->GetIndex()->IsConstant()
        ? !kBakerReadBarrierThunksEnableForFields
        : !kBakerReadBarrierThunksEnableForArrays;
    if (temp_needed) {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
}

static auto GetImplicitNullChecker(HInstruction* instruction, CodeGeneratorMIPS64* codegen) {
  auto null_checker = [codegen, instruction]() {
    codegen->MaybeRecordImplicitNullCheck(instruction);
  };
  return null_checker;
}

void InstructionCodeGeneratorMIPS64::VisitArrayGet(HArrayGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  GpuRegister obj = obj_loc.AsRegister<GpuRegister>();
  Location out_loc = locations->Out();
  Location index = locations->InAt(1);
  uint32_t data_offset = CodeGenerator::GetArrayDataOffset(instruction);
  auto null_checker = GetImplicitNullChecker(instruction, codegen_);

  DataType::Type type = instruction->GetType();
  const bool maybe_compressed_char_at = mirror::kUseStringCompression &&
                                        instruction->IsStringCharAt();
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8: {
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadUnsignedByte, out, obj, offset, null_checker);
      } else {
        __ Daddu(TMP, obj, index.AsRegister<GpuRegister>());
        __ LoadFromOffset(kLoadUnsignedByte, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt8: {
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadSignedByte, out, obj, offset, null_checker);
      } else {
        __ Daddu(TMP, obj, index.AsRegister<GpuRegister>());
        __ LoadFromOffset(kLoadSignedByte, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint16: {
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      if (maybe_compressed_char_at) {
        uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
        __ LoadFromOffset(kLoadWord, TMP, obj, count_offset, null_checker);
        __ Dext(TMP, TMP, 0, 1);
        static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                      "Expecting 0=compressed, 1=uncompressed");
      }
      if (index.IsConstant()) {
        int32_t const_index = index.GetConstant()->AsIntConstant()->GetValue();
        if (maybe_compressed_char_at) {
          Mips64Label uncompressed_load, done;
          __ Bnezc(TMP, &uncompressed_load);
          __ LoadFromOffset(kLoadUnsignedByte,
                            out,
                            obj,
                            data_offset + (const_index << TIMES_1));
          __ Bc(&done);
          __ Bind(&uncompressed_load);
          __ LoadFromOffset(kLoadUnsignedHalfword,
                            out,
                            obj,
                            data_offset + (const_index << TIMES_2));
          __ Bind(&done);
        } else {
          __ LoadFromOffset(kLoadUnsignedHalfword,
                            out,
                            obj,
                            data_offset + (const_index << TIMES_2),
                            null_checker);
        }
      } else {
        GpuRegister index_reg = index.AsRegister<GpuRegister>();
        if (maybe_compressed_char_at) {
          Mips64Label uncompressed_load, done;
          __ Bnezc(TMP, &uncompressed_load);
          __ Daddu(TMP, obj, index_reg);
          __ LoadFromOffset(kLoadUnsignedByte, out, TMP, data_offset);
          __ Bc(&done);
          __ Bind(&uncompressed_load);
          __ Dlsa(TMP, index_reg, obj, TIMES_2);
          __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset);
          __ Bind(&done);
        } else {
          __ Dlsa(TMP, index_reg, obj, TIMES_2);
          __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset, null_checker);
        }
      }
      break;
    }

    case DataType::Type::kInt16: {
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2) + data_offset;
        __ LoadFromOffset(kLoadSignedHalfword, out, obj, offset, null_checker);
      } else {
        __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_2);
        __ LoadFromOffset(kLoadSignedHalfword, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt32: {
      DCHECK_EQ(sizeof(mirror::HeapReference<mirror::Object>), sizeof(int32_t));
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      LoadOperandType load_type =
          (type == DataType::Type::kReference) ? kLoadUnsignedWord : kLoadWord;
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadFromOffset(load_type, out, obj, offset, null_checker);
      } else {
        __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_4);
        __ LoadFromOffset(load_type, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kReference: {
      static_assert(
          sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
          "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
      // /* HeapReference<Object> */ out =
      //     *(obj + data_offset + index * sizeof(HeapReference<Object>))
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        bool temp_needed = index.IsConstant()
            ? !kBakerReadBarrierThunksEnableForFields
            : !kBakerReadBarrierThunksEnableForArrays;
        Location temp = temp_needed ? locations->GetTemp(0) : Location::NoLocation();
        // Note that a potential implicit null check is handled in this
        // CodeGeneratorMIPS64::GenerateArrayLoadWithBakerReadBarrier call.
        DCHECK(!instruction->CanDoImplicitNullCheckOn(instruction->InputAt(0)));
        if (index.IsConstant()) {
          // Array load with a constant index can be treated as a field load.
          size_t offset =
              (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
          codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                          out_loc,
                                                          obj,
                                                          offset,
                                                          temp,
                                                          /* needs_null_check */ false);
        } else {
          codegen_->GenerateArrayLoadWithBakerReadBarrier(instruction,
                                                          out_loc,
                                                          obj,
                                                          data_offset,
                                                          index,
                                                          temp,
                                                          /* needs_null_check */ false);
        }
      } else {
        GpuRegister out = out_loc.AsRegister<GpuRegister>();
        if (index.IsConstant()) {
          size_t offset =
              (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
          __ LoadFromOffset(kLoadUnsignedWord, out, obj, offset, null_checker);
          // If read barriers are enabled, emit read barriers other than
          // Baker's using a slow path (and also unpoison the loaded
          // reference, if heap poisoning is enabled).
          codegen_->MaybeGenerateReadBarrierSlow(instruction, out_loc, out_loc, obj_loc, offset);
        } else {
          __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_4);
          __ LoadFromOffset(kLoadUnsignedWord, out, TMP, data_offset, null_checker);
          // If read barriers are enabled, emit read barriers other than
          // Baker's using a slow path (and also unpoison the loaded
          // reference, if heap poisoning is enabled).
          codegen_->MaybeGenerateReadBarrierSlow(instruction,
                                                 out_loc,
                                                 out_loc,
                                                 obj_loc,
                                                 data_offset,
                                                 index);
        }
      }
      break;
    }

    case DataType::Type::kInt64: {
      GpuRegister out = out_loc.AsRegister<GpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadFromOffset(kLoadDoubleword, out, obj, offset, null_checker);
      } else {
        __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_8);
        __ LoadFromOffset(kLoadDoubleword, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      FpuRegister out = out_loc.AsFpuRegister<FpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadFpuFromOffset(kLoadWord, out, obj, offset, null_checker);
      } else {
        __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_4);
        __ LoadFpuFromOffset(kLoadWord, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      FpuRegister out = out_loc.AsFpuRegister<FpuRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadFpuFromOffset(kLoadDoubleword, out, obj, offset, null_checker);
      } else {
        __ Dlsa(TMP, index.AsRegister<GpuRegister>(), obj, TIMES_8);
        __ LoadFpuFromOffset(kLoadDoubleword, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << instruction->GetType();
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS64::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  uint32_t offset = CodeGenerator::GetArrayLengthOffset(instruction);
  GpuRegister obj = locations->InAt(0).AsRegister<GpuRegister>();
  GpuRegister out = locations->Out().AsRegister<GpuRegister>();
  __ LoadFromOffset(kLoadWord, out, obj, offset);
  codegen_->MaybeRecordImplicitNullCheck(instruction);
  // Mask out compression flag from String's array length.
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    __ Srl(out, out, 1u);
  }
}

Location LocationsBuilderMIPS64::RegisterOrZeroConstant(HInstruction* instruction) {
  return (instruction->IsConstant() && instruction->AsConstant()->IsZeroBitPattern())
      ? Location::ConstantLocation(instruction->AsConstant())
      : Location::RequiresRegister();
}

Location LocationsBuilderMIPS64::FpuRegisterOrConstantForStore(HInstruction* instruction) {
  // We can store 0.0 directly (from the ZERO register) without loading it into an FPU register.
  // We can store a non-zero float or double constant without first loading it into the FPU,
  // but we should only prefer this if the constant has a single use.
  if (instruction->IsConstant() &&
      (instruction->AsConstant()->IsZeroBitPattern() ||
       instruction->GetUses().HasExactlyOneElement())) {
    return Location::ConstantLocation(instruction->AsConstant());
    // Otherwise fall through and require an FPU register for the constant.
  }
  return Location::RequiresFpuRegister();
}

void LocationsBuilderMIPS64::VisitArraySet(HArraySet* instruction) {
  DataType::Type value_type = instruction->GetComponentType();

  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());
  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction,
      may_need_runtime_call_for_type_check ?
          LocationSummary::kCallOnSlowPath :
          LocationSummary::kNoCall);

  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (DataType::IsFloatingPointType(instruction->InputAt(2)->GetType())) {
    locations->SetInAt(2, FpuRegisterOrConstantForStore(instruction->InputAt(2)));
  } else {
    locations->SetInAt(2, RegisterOrZeroConstant(instruction->InputAt(2)));
  }
  if (needs_write_barrier) {
    // Temporary register for the write barrier.
    locations->AddTemp(Location::RequiresRegister());  // Possibly used for ref. poisoning too.
  }
}

void InstructionCodeGeneratorMIPS64::VisitArraySet(HArraySet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  GpuRegister obj = locations->InAt(0).AsRegister<GpuRegister>();
  Location index = locations->InAt(1);
  Location value_location = locations->InAt(2);
  DataType::Type value_type = instruction->GetComponentType();
  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());
  auto null_checker = GetImplicitNullChecker(instruction, codegen_);
  GpuRegister base_reg = index.IsConstant() ? obj : TMP;

  switch (value_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint8_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1;
      } else {
        __ Daddu(base_reg, obj, index.AsRegister<GpuRegister>());
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreByte, value, base_reg, data_offset, TMP, null_checker);
      } else {
        GpuRegister value = value_location.AsRegister<GpuRegister>();
        __ StoreToOffset(kStoreByte, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint16:
    case DataType::Type::kInt16: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_2);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreHalfword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        GpuRegister value = value_location.AsRegister<GpuRegister>();
        __ StoreToOffset(kStoreHalfword, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt32: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_4);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
      } else {
        GpuRegister value = value_location.AsRegister<GpuRegister>();
        __ StoreToOffset(kStoreWord, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kReference: {
      if (value_location.IsConstant()) {
        // Just setting null.
        uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
        if (index.IsConstant()) {
          data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
        } else {
          __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_4);
        }
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        DCHECK_EQ(value, 0);
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
        DCHECK(!needs_write_barrier);
        DCHECK(!may_need_runtime_call_for_type_check);
        break;
      }

      DCHECK(needs_write_barrier);
      GpuRegister value = value_location.AsRegister<GpuRegister>();
      GpuRegister temp1 = locations->GetTemp(0).AsRegister<GpuRegister>();
      GpuRegister temp2 = TMP;  // Doesn't need to survive slow path.
      uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
      uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
      uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
      Mips64Label done;
      SlowPathCodeMIPS64* slow_path = nullptr;

      if (may_need_runtime_call_for_type_check) {
        slow_path = new (codegen_->GetScopedAllocator()) ArraySetSlowPathMIPS64(instruction);
        codegen_->AddSlowPath(slow_path);
        if (instruction->GetValueCanBeNull()) {
          Mips64Label non_zero;
          __ Bnezc(value, &non_zero);
          uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
          if (index.IsConstant()) {
            data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
          } else {
            __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_4);
          }
          __ StoreToOffset(kStoreWord, value, base_reg, data_offset, null_checker);
          __ Bc(&done);
          __ Bind(&non_zero);
        }

        // Note that when read barriers are enabled, the type checks
        // are performed without read barriers.  This is fine, even in
        // the case where a class object is in the from-space after
        // the flip, as a comparison involving such a type would not
        // produce a false positive; it may of course produce a false
        // negative, in which case we would take the ArraySet slow
        // path.

        // /* HeapReference<Class> */ temp1 = obj->klass_
        __ LoadFromOffset(kLoadUnsignedWord, temp1, obj, class_offset, null_checker);
        __ MaybeUnpoisonHeapReference(temp1);

        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        __ LoadFromOffset(kLoadUnsignedWord, temp1, temp1, component_offset);
        // /* HeapReference<Class> */ temp2 = value->klass_
        __ LoadFromOffset(kLoadUnsignedWord, temp2, value, class_offset);
        // If heap poisoning is enabled, no need to unpoison `temp1`
        // nor `temp2`, as we are comparing two poisoned references.

        if (instruction->StaticTypeOfArrayIsObjectArray()) {
          Mips64Label do_put;
          __ Beqc(temp1, temp2, &do_put);
          // If heap poisoning is enabled, the `temp1` reference has
          // not been unpoisoned yet; unpoison it now.
          __ MaybeUnpoisonHeapReference(temp1);

          // /* HeapReference<Class> */ temp1 = temp1->super_class_
          __ LoadFromOffset(kLoadUnsignedWord, temp1, temp1, super_offset);
          // If heap poisoning is enabled, no need to unpoison
          // `temp1`, as we are comparing against null below.
          __ Bnezc(temp1, slow_path->GetEntryLabel());
          __ Bind(&do_put);
        } else {
          __ Bnec(temp1, temp2, slow_path->GetEntryLabel());
        }
      }

      GpuRegister source = value;
      if (kPoisonHeapReferences) {
        // Note that in the case where `value` is a null reference,
        // we do not enter this block, as a null reference does not
        // need poisoning.
        __ Move(temp1, value);
        __ PoisonHeapReference(temp1);
        source = temp1;
      }

      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_4);
      }
      __ StoreToOffset(kStoreWord, source, base_reg, data_offset);

      if (!may_need_runtime_call_for_type_check) {
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }

      codegen_->MarkGCCard(obj, value, instruction->GetValueCanBeNull());

      if (done.IsLinked()) {
        __ Bind(&done);
      }

      if (slow_path != nullptr) {
        __ Bind(slow_path->GetExitLabel());
      }
      break;
    }

    case DataType::Type::kInt64: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_8);
      }
      if (value_location.IsConstant()) {
        int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreDoubleword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        GpuRegister value = value_location.AsRegister<GpuRegister>();
        __ StoreToOffset(kStoreDoubleword, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(float)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_4);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
      } else {
        FpuRegister value = value_location.AsFpuRegister<FpuRegister>();
        __ StoreFpuToOffset(kStoreWord, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(double)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8;
      } else {
        __ Dlsa(base_reg, index.AsRegister<GpuRegister>(), obj, TIMES_8);
      }
      if (value_location.IsConstant()) {
        int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreDoubleword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        FpuRegister value = value_location.AsFpuRegister<FpuRegister>();
        __ StoreFpuToOffset(kStoreDoubleword, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << instruction->GetType();
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitBoundsCheck(HBoundsCheck* instruction) {
  RegisterSet caller_saves = RegisterSet::Empty();
  InvokeRuntimeCallingConvention calling_convention;
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction, caller_saves);

  HInstruction* index = instruction->InputAt(0);
  HInstruction* length = instruction->InputAt(1);

  bool const_index = false;
  bool const_length = false;

  if (index->IsConstant()) {
    if (length->IsConstant()) {
      const_index = true;
      const_length = true;
    } else {
      int32_t index_value = index->AsIntConstant()->GetValue();
      if (index_value < 0 || IsInt<16>(index_value + 1)) {
        const_index = true;
      }
    }
  } else if (length->IsConstant()) {
    int32_t length_value = length->AsIntConstant()->GetValue();
    if (IsUint<15>(length_value)) {
      const_length = true;
    }
  }

  locations->SetInAt(0, const_index
      ? Location::ConstantLocation(index->AsConstant())
      : Location::RequiresRegister());
  locations->SetInAt(1, const_length
      ? Location::ConstantLocation(length->AsConstant())
      : Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS64::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location index_loc = locations->InAt(0);
  Location length_loc = locations->InAt(1);

  if (length_loc.IsConstant()) {
    int32_t length = length_loc.GetConstant()->AsIntConstant()->GetValue();
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0 || index >= length) {
        BoundsCheckSlowPathMIPS64* slow_path =
            new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS64(instruction);
        codegen_->AddSlowPath(slow_path);
        __ Bc(slow_path->GetEntryLabel());
      } else {
        // Nothing to be done.
      }
      return;
    }

    BoundsCheckSlowPathMIPS64* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS64(instruction);
    codegen_->AddSlowPath(slow_path);
    GpuRegister index = index_loc.AsRegister<GpuRegister>();
    if (length == 0) {
      __ Bc(slow_path->GetEntryLabel());
    } else if (length == 1) {
      __ Bnezc(index, slow_path->GetEntryLabel());
    } else {
      DCHECK(IsUint<15>(length)) << length;
      __ Sltiu(TMP, index, length);
      __ Beqzc(TMP, slow_path->GetEntryLabel());
    }
  } else {
    GpuRegister length = length_loc.AsRegister<GpuRegister>();
    BoundsCheckSlowPathMIPS64* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS64(instruction);
    codegen_->AddSlowPath(slow_path);
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0) {
        __ Bc(slow_path->GetEntryLabel());
      } else if (index == 0) {
        __ Blezc(length, slow_path->GetEntryLabel());
      } else {
        DCHECK(IsInt<16>(index + 1)) << index;
        __ Sltiu(TMP, length, index + 1);
        __ Bnezc(TMP, slow_path->GetEntryLabel());
      }
    } else {
      GpuRegister index = index_loc.AsRegister<GpuRegister>();
      __ Bgeuc(index, length, slow_path->GetEntryLabel());
    }
  }
}

// Temp is used for read barrier.
static size_t NumberOfInstanceOfTemps(TypeCheckKind type_check_kind) {
  if (kEmitCompilerReadBarrier &&
      !(kUseBakerReadBarrier && kBakerReadBarrierThunksEnableForFields) &&
      (kUseBakerReadBarrier ||
       type_check_kind == TypeCheckKind::kAbstractClassCheck ||
       type_check_kind == TypeCheckKind::kClassHierarchyCheck ||
       type_check_kind == TypeCheckKind::kArrayObjectCheck)) {
    return 1;
  }
  return 0;
}

// Extra temp is used for read barrier.
static size_t NumberOfCheckCastTemps(TypeCheckKind type_check_kind) {
  return 1 + NumberOfInstanceOfTemps(type_check_kind);
}

void LocationsBuilderMIPS64::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary::CallKind call_kind = CodeGenerator::GetCheckCastCallKind(instruction);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->AddRegisterTemps(NumberOfCheckCastTemps(type_check_kind));
}

void InstructionCodeGeneratorMIPS64::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  GpuRegister obj = obj_loc.AsRegister<GpuRegister>();
  GpuRegister cls = locations->InAt(1).AsRegister<GpuRegister>();
  Location temp_loc = locations->GetTemp(0);
  GpuRegister temp = temp_loc.AsRegister<GpuRegister>();
  const size_t num_temps = NumberOfCheckCastTemps(type_check_kind);
  DCHECK_LE(num_temps, 2u);
  Location maybe_temp2_loc = (num_temps >= 2) ? locations->GetTemp(1) : Location::NoLocation();
  const uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  const uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  const uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  const uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  const uint32_t iftable_offset = mirror::Class::IfTableOffset().Uint32Value();
  const uint32_t array_length_offset = mirror::Array::LengthOffset().Uint32Value();
  const uint32_t object_array_data_offset =
      mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();
  Mips64Label done;

  bool is_type_check_slow_path_fatal = CodeGenerator::IsTypeCheckSlowPathFatal(instruction);
  SlowPathCodeMIPS64* slow_path =
      new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS64(
          instruction, is_type_check_slow_path_fatal);
  codegen_->AddSlowPath(slow_path);

  // Avoid this check if we know `obj` is not null.
  if (instruction->MustDoNullCheck()) {
    __ Beqzc(obj, &done);
  }

  switch (type_check_kind) {
    case TypeCheckKind::kExactCheck:
    case TypeCheckKind::kArrayCheck: {
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // Jump to slow path for throwing the exception or doing a
      // more involved array check.
      __ Bnec(temp, cls, slow_path->GetEntryLabel());
      break;
    }

    case TypeCheckKind::kAbstractClassCheck: {
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // If the class is abstract, we eagerly fetch the super class of the
      // object to avoid doing a comparison we know will fail.
      Mips64Label loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception.
      __ Beqzc(temp, slow_path->GetEntryLabel());
      // Otherwise, compare the classes.
      __ Bnec(temp, cls, &loop);
      break;
    }

    case TypeCheckKind::kClassHierarchyCheck: {
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // Walk over the class hierarchy to find a match.
      Mips64Label loop;
      __ Bind(&loop);
      __ Beqc(temp, cls, &done);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception. Otherwise, jump to the beginning of the loop.
      __ Bnezc(temp, &loop);
      __ Bc(slow_path->GetEntryLabel());
      break;
    }

    case TypeCheckKind::kArrayObjectCheck: {
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // Do an exact check.
      __ Beqc(temp, cls, &done);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ temp = temp->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       component_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the component type is null, jump to the slow path to throw the exception.
      __ Beqzc(temp, slow_path->GetEntryLabel());
      // Otherwise, the object is indeed an array, further check that this component
      // type is not a primitive type.
      __ LoadFromOffset(kLoadUnsignedHalfword, temp, temp, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Bnezc(temp, slow_path->GetEntryLabel());
      break;
    }

    case TypeCheckKind::kUnresolvedCheck:
      // We always go into the type check slow path for the unresolved check case.
      // We cannot directly call the CheckCast runtime entry point
      // without resorting to a type checking slow path here (i.e. by
      // calling InvokeRuntime directly), as it would require to
      // assign fixed registers for the inputs of this HInstanceOf
      // instruction (following the runtime calling convention), which
      // might be cluttered by the potential first read barrier
      // emission at the beginning of this method.
      __ Bc(slow_path->GetEntryLabel());
      break;

    case TypeCheckKind::kInterfaceCheck: {
      // Avoid read barriers to improve performance of the fast path. We can not get false
      // positives by doing this.
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // /* HeapReference<Class> */ temp = temp->iftable_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        temp_loc,
                                        iftable_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);
      // Iftable is never null.
      __ Lw(TMP, temp, array_length_offset);
      // Loop through the iftable and check if any class matches.
      Mips64Label loop;
      __ Bind(&loop);
      __ Beqzc(TMP, slow_path->GetEntryLabel());
      __ Lwu(AT, temp, object_array_data_offset);
      __ MaybeUnpoisonHeapReference(AT);
      // Go to next interface.
      __ Daddiu(temp, temp, 2 * kHeapReferenceSize);
      __ Addiu(TMP, TMP, -2);
      // Compare the classes and continue the loop if they do not match.
      __ Bnec(AT, cls, &loop);
      break;
    }
  }

  __ Bind(&done);
  __ Bind(slow_path->GetExitLabel());
}

void LocationsBuilderMIPS64::VisitClinitCheck(HClinitCheck* check) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(check, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (check->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS64::VisitClinitCheck(HClinitCheck* check) {
  // We assume the class is not null.
  SlowPathCodeMIPS64* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathMIPS64(
      check->GetLoadClass(),
      check,
      check->GetDexPc(),
      true);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path,
                                   check->GetLocations()->InAt(0).AsRegister<GpuRegister>());
}

void LocationsBuilderMIPS64::VisitCompare(HCompare* compare) {
  DataType::Type in_type = compare->InputAt(0)->GetType();

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(compare);

  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(compare->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected type for compare operation " << in_type;
  }
}

void InstructionCodeGeneratorMIPS64::VisitCompare(HCompare* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  GpuRegister res = locations->Out().AsRegister<GpuRegister>();
  DataType::Type in_type = instruction->InputAt(0)->GetType();

  //  0 if: left == right
  //  1 if: left  > right
  // -1 if: left  < right
  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
      Location rhs_location = locations->InAt(1);
      bool use_imm = rhs_location.IsConstant();
      GpuRegister rhs = ZERO;
      if (use_imm) {
        if (in_type == DataType::Type::kInt64) {
          int64_t value = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant()->AsConstant());
          if (value != 0) {
            rhs = AT;
            __ LoadConst64(rhs, value);
          }
        } else {
          int32_t value = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant()->AsConstant());
          if (value != 0) {
            rhs = AT;
            __ LoadConst32(rhs, value);
          }
        }
      } else {
        rhs = rhs_location.AsRegister<GpuRegister>();
      }
      __ Slt(TMP, lhs, rhs);
      __ Slt(res, rhs, lhs);
      __ Subu(res, res, TMP);
      break;
    }

    case DataType::Type::kFloat32: {
      FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
      FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
      Mips64Label done;
      __ CmpEqS(FTMP, lhs, rhs);
      __ LoadConst32(res, 0);
      __ Bc1nez(FTMP, &done);
      if (instruction->IsGtBias()) {
        __ CmpLtS(FTMP, lhs, rhs);
        __ LoadConst32(res, -1);
        __ Bc1nez(FTMP, &done);
        __ LoadConst32(res, 1);
      } else {
        __ CmpLtS(FTMP, rhs, lhs);
        __ LoadConst32(res, 1);
        __ Bc1nez(FTMP, &done);
        __ LoadConst32(res, -1);
      }
      __ Bind(&done);
      break;
    }

    case DataType::Type::kFloat64: {
      FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
      FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
      Mips64Label done;
      __ CmpEqD(FTMP, lhs, rhs);
      __ LoadConst32(res, 0);
      __ Bc1nez(FTMP, &done);
      if (instruction->IsGtBias()) {
        __ CmpLtD(FTMP, lhs, rhs);
        __ LoadConst32(res, -1);
        __ Bc1nez(FTMP, &done);
        __ LoadConst32(res, 1);
      } else {
        __ CmpLtD(FTMP, rhs, lhs);
        __ LoadConst32(res, 1);
        __ Bc1nez(FTMP, &done);
        __ LoadConst32(res, -1);
      }
      __ Bind(&done);
      break;
    }

    default:
      LOG(FATAL) << "Unimplemented compare type " << in_type;
  }
}

void LocationsBuilderMIPS64::HandleCondition(HCondition* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  switch (instruction->InputAt(0)->GetType()) {
    default:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      break;
  }
  if (!instruction->IsEmittedAtUseSite()) {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorMIPS64::HandleCondition(HCondition* instruction) {
  if (instruction->IsEmittedAtUseSite()) {
    return;
  }

  DataType::Type type = instruction->InputAt(0)->GetType();
  LocationSummary* locations = instruction->GetLocations();
  switch (type) {
    default:
      // Integer case.
      GenerateIntLongCompare(instruction->GetCondition(), /* is64bit */ false, locations);
      return;
    case DataType::Type::kInt64:
      GenerateIntLongCompare(instruction->GetCondition(), /* is64bit */ true, locations);
      return;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      GenerateFpCompare(instruction->GetCondition(), instruction->IsGtBias(), type, locations);
     return;
  }
}

void InstructionCodeGeneratorMIPS64::DivRemOneOrMinusOne(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DataType::Type type = instruction->GetResultType();

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  GpuRegister out = locations->Out().AsRegister<GpuRegister>();
  GpuRegister dividend = locations->InAt(0).AsRegister<GpuRegister>();
  int64_t imm = Int64FromConstant(second.GetConstant());
  DCHECK(imm == 1 || imm == -1);

  if (instruction->IsRem()) {
    __ Move(out, ZERO);
  } else {
    if (imm == -1) {
      if (type == DataType::Type::kInt32) {
        __ Subu(out, ZERO, dividend);
      } else {
        DCHECK_EQ(type, DataType::Type::kInt64);
        __ Dsubu(out, ZERO, dividend);
      }
    } else if (out != dividend) {
      __ Move(out, dividend);
    }
  }
}

void InstructionCodeGeneratorMIPS64::DivRemByPowerOfTwo(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DataType::Type type = instruction->GetResultType();

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  GpuRegister out = locations->Out().AsRegister<GpuRegister>();
  GpuRegister dividend = locations->InAt(0).AsRegister<GpuRegister>();
  int64_t imm = Int64FromConstant(second.GetConstant());
  uint64_t abs_imm = static_cast<uint64_t>(AbsOrMin(imm));
  int ctz_imm = CTZ(abs_imm);

  if (instruction->IsDiv()) {
    if (type == DataType::Type::kInt32) {
      if (ctz_imm == 1) {
        // Fast path for division by +/-2, which is very common.
        __ Srl(TMP, dividend, 31);
      } else {
        __ Sra(TMP, dividend, 31);
        __ Srl(TMP, TMP, 32 - ctz_imm);
      }
      __ Addu(out, dividend, TMP);
      __ Sra(out, out, ctz_imm);
      if (imm < 0) {
        __ Subu(out, ZERO, out);
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kInt64);
      if (ctz_imm == 1) {
        // Fast path for division by +/-2, which is very common.
        __ Dsrl32(TMP, dividend, 31);
      } else {
        __ Dsra32(TMP, dividend, 31);
        if (ctz_imm > 32) {
          __ Dsrl(TMP, TMP, 64 - ctz_imm);
        } else {
          __ Dsrl32(TMP, TMP, 32 - ctz_imm);
        }
      }
      __ Daddu(out, dividend, TMP);
      if (ctz_imm < 32) {
        __ Dsra(out, out, ctz_imm);
      } else {
        __ Dsra32(out, out, ctz_imm - 32);
      }
      if (imm < 0) {
        __ Dsubu(out, ZERO, out);
      }
    }
  } else {
    if (type == DataType::Type::kInt32) {
      if (ctz_imm == 1) {
        // Fast path for modulo +/-2, which is very common.
        __ Sra(TMP, dividend, 31);
        __ Subu(out, dividend, TMP);
        __ Andi(out, out, 1);
        __ Addu(out, out, TMP);
      } else {
        __ Sra(TMP, dividend, 31);
        __ Srl(TMP, TMP, 32 - ctz_imm);
        __ Addu(out, dividend, TMP);
        __ Ins(out, ZERO, ctz_imm, 32 - ctz_imm);
        __ Subu(out, out, TMP);
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kInt64);
      if (ctz_imm == 1) {
        // Fast path for modulo +/-2, which is very common.
        __ Dsra32(TMP, dividend, 31);
        __ Dsubu(out, dividend, TMP);
        __ Andi(out, out, 1);
        __ Daddu(out, out, TMP);
      } else {
        __ Dsra32(TMP, dividend, 31);
        if (ctz_imm > 32) {
          __ Dsrl(TMP, TMP, 64 - ctz_imm);
        } else {
          __ Dsrl32(TMP, TMP, 32 - ctz_imm);
        }
        __ Daddu(out, dividend, TMP);
        __ DblIns(out, ZERO, ctz_imm, 64 - ctz_imm);
        __ Dsubu(out, out, TMP);
      }
    }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateDivRemWithAnyConstant(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  GpuRegister out = locations->Out().AsRegister<GpuRegister>();
  GpuRegister dividend = locations->InAt(0).AsRegister<GpuRegister>();
  int64_t imm = Int64FromConstant(second.GetConstant());

  DataType::Type type = instruction->GetResultType();
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64) << type;

  int64_t magic;
  int shift;
  CalculateMagicAndShiftForDivRem(imm,
                                  (type == DataType::Type::kInt64),
                                  &magic,
                                  &shift);

  if (type == DataType::Type::kInt32) {
    __ LoadConst32(TMP, magic);
    __ MuhR6(TMP, dividend, TMP);

    if (imm > 0 && magic < 0) {
      __ Addu(TMP, TMP, dividend);
    } else if (imm < 0 && magic > 0) {
      __ Subu(TMP, TMP, dividend);
    }

    if (shift != 0) {
      __ Sra(TMP, TMP, shift);
    }

    if (instruction->IsDiv()) {
      __ Sra(out, TMP, 31);
      __ Subu(out, TMP, out);
    } else {
      __ Sra(AT, TMP, 31);
      __ Subu(AT, TMP, AT);
      __ LoadConst32(TMP, imm);
      __ MulR6(TMP, AT, TMP);
      __ Subu(out, dividend, TMP);
    }
  } else {
    __ LoadConst64(TMP, magic);
    __ Dmuh(TMP, dividend, TMP);

    if (imm > 0 && magic < 0) {
      __ Daddu(TMP, TMP, dividend);
    } else if (imm < 0 && magic > 0) {
      __ Dsubu(TMP, TMP, dividend);
    }

    if (shift >= 32) {
      __ Dsra32(TMP, TMP, shift - 32);
    } else if (shift > 0) {
      __ Dsra(TMP, TMP, shift);
    }

    if (instruction->IsDiv()) {
      __ Dsra32(out, TMP, 31);
      __ Dsubu(out, TMP, out);
    } else {
      __ Dsra32(AT, TMP, 31);
      __ Dsubu(AT, TMP, AT);
      __ LoadConst64(TMP, imm);
      __ Dmul(TMP, AT, TMP);
      __ Dsubu(out, dividend, TMP);
    }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateDivRemIntegral(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DataType::Type type = instruction->GetResultType();
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64) << type;

  LocationSummary* locations = instruction->GetLocations();
  GpuRegister out = locations->Out().AsRegister<GpuRegister>();
  Location second = locations->InAt(1);

  if (second.IsConstant()) {
    int64_t imm = Int64FromConstant(second.GetConstant());
    if (imm == 0) {
      // Do not generate anything. DivZeroCheck would prevent any code to be executed.
    } else if (imm == 1 || imm == -1) {
      DivRemOneOrMinusOne(instruction);
    } else if (IsPowerOfTwo(AbsOrMin(imm))) {
      DivRemByPowerOfTwo(instruction);
    } else {
      DCHECK(imm <= -2 || imm >= 2);
      GenerateDivRemWithAnyConstant(instruction);
    }
  } else {
    GpuRegister dividend = locations->InAt(0).AsRegister<GpuRegister>();
    GpuRegister divisor = second.AsRegister<GpuRegister>();
    if (instruction->IsDiv()) {
      if (type == DataType::Type::kInt32)
        __ DivR6(out, dividend, divisor);
      else
        __ Ddiv(out, dividend, divisor);
    } else {
      if (type == DataType::Type::kInt32)
        __ ModR6(out, dividend, divisor);
      else
        __ Dmod(out, dividend, divisor);
    }
  }
}

void LocationsBuilderMIPS64::VisitDiv(HDiv* div) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(div, LocationSummary::kNoCall);
  switch (div->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(div->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << div->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS64::VisitDiv(HDiv* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      GenerateDivRemIntegral(instruction);
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
      FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
      FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
      if (type == DataType::Type::kFloat32)
        __ DivS(dst, lhs, rhs);
      else
        __ DivD(dst, lhs, rhs);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected div type " << type;
  }
}

void LocationsBuilderMIPS64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RegisterOrConstant(instruction->InputAt(0)));
}

void InstructionCodeGeneratorMIPS64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  SlowPathCodeMIPS64* slow_path =
      new (codegen_->GetScopedAllocator()) DivZeroCheckSlowPathMIPS64(instruction);
  codegen_->AddSlowPath(slow_path);
  Location value = instruction->GetLocations()->InAt(0);

  DataType::Type type = instruction->GetType();

  if (!DataType::IsIntegralType(type)) {
    LOG(FATAL) << "Unexpected type " << type << " for DivZeroCheck.";
    return;
  }

  if (value.IsConstant()) {
    int64_t divisor = codegen_->GetInt64ValueOf(value.GetConstant()->AsConstant());
    if (divisor == 0) {
      __ Bc(slow_path->GetEntryLabel());
    } else {
      // A division by a non-null constant is valid. We don't need to perform
      // any check, so simply fall through.
    }
  } else {
    __ Beqzc(value.AsRegister<GpuRegister>(), slow_path->GetEntryLabel());
  }
}

void LocationsBuilderMIPS64::VisitDoubleConstant(HDoubleConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS64::VisitDoubleConstant(HDoubleConstant* cst ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS64::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitExit(HExit* exit ATTRIBUTE_UNUSED) {
}

void LocationsBuilderMIPS64::VisitFloatConstant(HFloatConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS64::VisitFloatConstant(HFloatConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void InstructionCodeGeneratorMIPS64::HandleGoto(HInstruction* got, HBasicBlock* successor) {
  if (successor->IsExitBlock()) {
    DCHECK(got->GetPrevious()->AlwaysThrows());
    return;  // no code needed
  }

  HBasicBlock* block = got->GetBlock();
  HInstruction* previous = got->GetPrevious();
  HLoopInformation* info = block->GetLoopInformation();

  if (info != nullptr && info->IsBackEdge(*block) && info->HasSuspendCheck()) {
    if (codegen_->GetCompilerOptions().CountHotnessInCompiledCode()) {
      __ Ld(AT, SP, kCurrentMethodStackOffset);
      __ Lhu(TMP, AT, ArtMethod::HotnessCountOffset().Int32Value());
      __ Addiu(TMP, TMP, 1);
      __ Sh(TMP, AT, ArtMethod::HotnessCountOffset().Int32Value());
    }
    GenerateSuspendCheck(info->GetSuspendCheck(), successor);
    return;
  }
  if (block->IsEntryBlock() && (previous != nullptr) && previous->IsSuspendCheck()) {
    GenerateSuspendCheck(previous->AsSuspendCheck(), nullptr);
  }
  if (!codegen_->GoesToNextBlock(block, successor)) {
    __ Bc(codegen_->GetLabelOf(successor));
  }
}

void LocationsBuilderMIPS64::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitGoto(HGoto* got) {
  HandleGoto(got, got->GetSuccessor());
}

void LocationsBuilderMIPS64::VisitTryBoundary(HTryBoundary* try_boundary) {
  try_boundary->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitTryBoundary(HTryBoundary* try_boundary) {
  HBasicBlock* successor = try_boundary->GetNormalFlowSuccessor();
  if (!successor->IsExitBlock()) {
    HandleGoto(try_boundary, successor);
  }
}

void InstructionCodeGeneratorMIPS64::GenerateIntLongCompare(IfCondition cond,
                                                            bool is64bit,
                                                            LocationSummary* locations) {
  GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
  GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
  Location rhs_location = locations->InAt(1);
  GpuRegister rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    if (is64bit) {
      rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
    } else {
      rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
    }
  } else {
    rhs_reg = rhs_location.AsRegister<GpuRegister>();
  }
  int64_t rhs_imm_plus_one = rhs_imm + UINT64_C(1);

  switch (cond) {
    case kCondEQ:
    case kCondNE:
      if (use_imm && IsInt<16>(-rhs_imm)) {
        if (rhs_imm == 0) {
          if (cond == kCondEQ) {
            __ Sltiu(dst, lhs, 1);
          } else {
            __ Sltu(dst, ZERO, lhs);
          }
        } else {
          if (is64bit) {
            __ Daddiu(dst, lhs, -rhs_imm);
          } else {
            __ Addiu(dst, lhs, -rhs_imm);
          }
          if (cond == kCondEQ) {
            __ Sltiu(dst, dst, 1);
          } else {
            __ Sltu(dst, ZERO, dst);
          }
        }
      } else {
        if (use_imm && IsUint<16>(rhs_imm)) {
          __ Xori(dst, lhs, rhs_imm);
        } else {
          if (use_imm) {
            rhs_reg = TMP;
            __ LoadConst64(rhs_reg, rhs_imm);
          }
          __ Xor(dst, lhs, rhs_reg);
        }
        if (cond == kCondEQ) {
          __ Sltiu(dst, dst, 1);
        } else {
          __ Sltu(dst, ZERO, dst);
        }
      }
      break;

    case kCondLT:
    case kCondGE:
      if (use_imm && IsInt<16>(rhs_imm)) {
        __ Slti(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Slt(dst, lhs, rhs_reg);
      }
      if (cond == kCondGE) {
        // Simulate lhs >= rhs via !(lhs < rhs) since there's
        // only the slt instruction but no sge.
        __ Xori(dst, dst, 1);
      }
      break;

    case kCondLE:
    case kCondGT:
      if (use_imm && IsInt<16>(rhs_imm_plus_one)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        __ Slti(dst, lhs, rhs_imm_plus_one);
        if (cond == kCondGT) {
          // Simulate lhs > rhs via !(lhs <= rhs) since there's
          // only the slti instruction but no sgti.
          __ Xori(dst, dst, 1);
        }
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Slt(dst, rhs_reg, lhs);
        if (cond == kCondLE) {
          // Simulate lhs <= rhs via !(rhs < lhs) since there's
          // only the slt instruction but no sle.
          __ Xori(dst, dst, 1);
        }
      }
      break;

    case kCondB:
    case kCondAE:
      if (use_imm && IsInt<16>(rhs_imm)) {
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0x[ffffffff]ffff8000, 0x[ffffffff]ffffffff].
        __ Sltiu(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, lhs, rhs_reg);
      }
      if (cond == kCondAE) {
        // Simulate lhs >= rhs via !(lhs < rhs) since there's
        // only the sltu instruction but no sgeu.
        __ Xori(dst, dst, 1);
      }
      break;

    case kCondBE:
    case kCondA:
      if (use_imm && (rhs_imm_plus_one != 0) && IsInt<16>(rhs_imm_plus_one)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        // Note that this only works if rhs + 1 does not overflow
        // to 0, hence the check above.
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0x[ffffffff]ffff8000, 0x[ffffffff]ffffffff].
        __ Sltiu(dst, lhs, rhs_imm_plus_one);
        if (cond == kCondA) {
          // Simulate lhs > rhs via !(lhs <= rhs) since there's
          // only the sltiu instruction but no sgtiu.
          __ Xori(dst, dst, 1);
        }
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, rhs_reg, lhs);
        if (cond == kCondBE) {
          // Simulate lhs <= rhs via !(rhs < lhs) since there's
          // only the sltu instruction but no sleu.
          __ Xori(dst, dst, 1);
        }
      }
      break;
  }
}

bool InstructionCodeGeneratorMIPS64::MaterializeIntLongCompare(IfCondition cond,
                                                               bool is64bit,
                                                               LocationSummary* input_locations,
                                                               GpuRegister dst) {
  GpuRegister lhs = input_locations->InAt(0).AsRegister<GpuRegister>();
  Location rhs_location = input_locations->InAt(1);
  GpuRegister rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    if (is64bit) {
      rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
    } else {
      rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
    }
  } else {
    rhs_reg = rhs_location.AsRegister<GpuRegister>();
  }
  int64_t rhs_imm_plus_one = rhs_imm + UINT64_C(1);

  switch (cond) {
    case kCondEQ:
    case kCondNE:
      if (use_imm && IsInt<16>(-rhs_imm)) {
        if (is64bit) {
          __ Daddiu(dst, lhs, -rhs_imm);
        } else {
          __ Addiu(dst, lhs, -rhs_imm);
        }
      } else if (use_imm && IsUint<16>(rhs_imm)) {
        __ Xori(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Xor(dst, lhs, rhs_reg);
      }
      return (cond == kCondEQ);

    case kCondLT:
    case kCondGE:
      if (use_imm && IsInt<16>(rhs_imm)) {
        __ Slti(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Slt(dst, lhs, rhs_reg);
      }
      return (cond == kCondGE);

    case kCondLE:
    case kCondGT:
      if (use_imm && IsInt<16>(rhs_imm_plus_one)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        __ Slti(dst, lhs, rhs_imm_plus_one);
        return (cond == kCondGT);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Slt(dst, rhs_reg, lhs);
        return (cond == kCondLE);
      }

    case kCondB:
    case kCondAE:
      if (use_imm && IsInt<16>(rhs_imm)) {
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0x[ffffffff]ffff8000, 0x[ffffffff]ffffffff].
        __ Sltiu(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, lhs, rhs_reg);
      }
      return (cond == kCondAE);

    case kCondBE:
    case kCondA:
      if (use_imm && (rhs_imm_plus_one != 0) && IsInt<16>(rhs_imm_plus_one)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        // Note that this only works if rhs + 1 does not overflow
        // to 0, hence the check above.
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0x[ffffffff]ffff8000, 0x[ffffffff]ffffffff].
        __ Sltiu(dst, lhs, rhs_imm_plus_one);
        return (cond == kCondA);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst64(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, rhs_reg, lhs);
        return (cond == kCondBE);
      }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateIntLongCompareAndBranch(IfCondition cond,
                                                                     bool is64bit,
                                                                     LocationSummary* locations,
                                                                     Mips64Label* label) {
  GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
  Location rhs_location = locations->InAt(1);
  GpuRegister rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    if (is64bit) {
      rhs_imm = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant());
    } else {
      rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
    }
  } else {
    rhs_reg = rhs_location.AsRegister<GpuRegister>();
  }

  if (use_imm && rhs_imm == 0) {
    switch (cond) {
      case kCondEQ:
      case kCondBE:  // <= 0 if zero
        __ Beqzc(lhs, label);
        break;
      case kCondNE:
      case kCondA:  // > 0 if non-zero
        __ Bnezc(lhs, label);
        break;
      case kCondLT:
        __ Bltzc(lhs, label);
        break;
      case kCondGE:
        __ Bgezc(lhs, label);
        break;
      case kCondLE:
        __ Blezc(lhs, label);
        break;
      case kCondGT:
        __ Bgtzc(lhs, label);
        break;
      case kCondB:  // always false
        break;
      case kCondAE:  // always true
        __ Bc(label);
        break;
    }
  } else {
    if (use_imm) {
      rhs_reg = TMP;
      __ LoadConst64(rhs_reg, rhs_imm);
    }
    switch (cond) {
      case kCondEQ:
        __ Beqc(lhs, rhs_reg, label);
        break;
      case kCondNE:
        __ Bnec(lhs, rhs_reg, label);
        break;
      case kCondLT:
        __ Bltc(lhs, rhs_reg, label);
        break;
      case kCondGE:
        __ Bgec(lhs, rhs_reg, label);
        break;
      case kCondLE:
        __ Bgec(rhs_reg, lhs, label);
        break;
      case kCondGT:
        __ Bltc(rhs_reg, lhs, label);
        break;
      case kCondB:
        __ Bltuc(lhs, rhs_reg, label);
        break;
      case kCondAE:
        __ Bgeuc(lhs, rhs_reg, label);
        break;
      case kCondBE:
        __ Bgeuc(rhs_reg, lhs, label);
        break;
      case kCondA:
        __ Bltuc(rhs_reg, lhs, label);
        break;
    }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateFpCompare(IfCondition cond,
                                                       bool gt_bias,
                                                       DataType::Type type,
                                                       LocationSummary* locations) {
  GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
  FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
  if (type == DataType::Type::kFloat32) {
    switch (cond) {
      case kCondEQ:
        __ CmpEqS(FTMP, lhs, rhs);
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondNE:
        __ CmpEqS(FTMP, lhs, rhs);
        __ Mfc1(dst, FTMP);
        __ Addiu(dst, dst, 1);
        break;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtS(FTMP, lhs, rhs);
        } else {
          __ CmpUltS(FTMP, lhs, rhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeS(FTMP, lhs, rhs);
        } else {
          __ CmpUleS(FTMP, lhs, rhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltS(FTMP, rhs, lhs);
        } else {
          __ CmpLtS(FTMP, rhs, lhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleS(FTMP, rhs, lhs);
        } else {
          __ CmpLeS(FTMP, rhs, lhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    switch (cond) {
      case kCondEQ:
        __ CmpEqD(FTMP, lhs, rhs);
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondNE:
        __ CmpEqD(FTMP, lhs, rhs);
        __ Mfc1(dst, FTMP);
        __ Addiu(dst, dst, 1);
        break;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtD(FTMP, lhs, rhs);
        } else {
          __ CmpUltD(FTMP, lhs, rhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeD(FTMP, lhs, rhs);
        } else {
          __ CmpUleD(FTMP, lhs, rhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltD(FTMP, rhs, lhs);
        } else {
          __ CmpLtD(FTMP, rhs, lhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleD(FTMP, rhs, lhs);
        } else {
          __ CmpLeD(FTMP, rhs, lhs);
        }
        __ Mfc1(dst, FTMP);
        __ Andi(dst, dst, 1);
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  }
}

bool InstructionCodeGeneratorMIPS64::MaterializeFpCompare(IfCondition cond,
                                                          bool gt_bias,
                                                          DataType::Type type,
                                                          LocationSummary* input_locations,
                                                          FpuRegister dst) {
  FpuRegister lhs = input_locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister rhs = input_locations->InAt(1).AsFpuRegister<FpuRegister>();
  if (type == DataType::Type::kFloat32) {
    switch (cond) {
      case kCondEQ:
        __ CmpEqS(dst, lhs, rhs);
        return false;
      case kCondNE:
        __ CmpEqS(dst, lhs, rhs);
        return true;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtS(dst, lhs, rhs);
        } else {
          __ CmpUltS(dst, lhs, rhs);
        }
        return false;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeS(dst, lhs, rhs);
        } else {
          __ CmpUleS(dst, lhs, rhs);
        }
        return false;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltS(dst, rhs, lhs);
        } else {
          __ CmpLtS(dst, rhs, lhs);
        }
        return false;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleS(dst, rhs, lhs);
        } else {
          __ CmpLeS(dst, rhs, lhs);
        }
        return false;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    switch (cond) {
      case kCondEQ:
        __ CmpEqD(dst, lhs, rhs);
        return false;
      case kCondNE:
        __ CmpEqD(dst, lhs, rhs);
        return true;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtD(dst, lhs, rhs);
        } else {
          __ CmpUltD(dst, lhs, rhs);
        }
        return false;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeD(dst, lhs, rhs);
        } else {
          __ CmpUleD(dst, lhs, rhs);
        }
        return false;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltD(dst, rhs, lhs);
        } else {
          __ CmpLtD(dst, rhs, lhs);
        }
        return false;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleD(dst, rhs, lhs);
        } else {
          __ CmpLeD(dst, rhs, lhs);
        }
        return false;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
        UNREACHABLE();
    }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateFpCompareAndBranch(IfCondition cond,
                                                                bool gt_bias,
                                                                DataType::Type type,
                                                                LocationSummary* locations,
                                                                Mips64Label* label) {
  FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
  FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
  if (type == DataType::Type::kFloat32) {
    switch (cond) {
      case kCondEQ:
        __ CmpEqS(FTMP, lhs, rhs);
        __ Bc1nez(FTMP, label);
        break;
      case kCondNE:
        __ CmpEqS(FTMP, lhs, rhs);
        __ Bc1eqz(FTMP, label);
        break;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtS(FTMP, lhs, rhs);
        } else {
          __ CmpUltS(FTMP, lhs, rhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeS(FTMP, lhs, rhs);
        } else {
          __ CmpUleS(FTMP, lhs, rhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltS(FTMP, rhs, lhs);
        } else {
          __ CmpLtS(FTMP, rhs, lhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleS(FTMP, rhs, lhs);
        } else {
          __ CmpLeS(FTMP, rhs, lhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition";
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    switch (cond) {
      case kCondEQ:
        __ CmpEqD(FTMP, lhs, rhs);
        __ Bc1nez(FTMP, label);
        break;
      case kCondNE:
        __ CmpEqD(FTMP, lhs, rhs);
        __ Bc1eqz(FTMP, label);
        break;
      case kCondLT:
        if (gt_bias) {
          __ CmpLtD(FTMP, lhs, rhs);
        } else {
          __ CmpUltD(FTMP, lhs, rhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondLE:
        if (gt_bias) {
          __ CmpLeD(FTMP, lhs, rhs);
        } else {
          __ CmpUleD(FTMP, lhs, rhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondGT:
        if (gt_bias) {
          __ CmpUltD(FTMP, rhs, lhs);
        } else {
          __ CmpLtD(FTMP, rhs, lhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      case kCondGE:
        if (gt_bias) {
          __ CmpUleD(FTMP, rhs, lhs);
        } else {
          __ CmpLeD(FTMP, rhs, lhs);
        }
        __ Bc1nez(FTMP, label);
        break;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition";
        UNREACHABLE();
    }
  }
}

void InstructionCodeGeneratorMIPS64::GenerateTestAndBranch(HInstruction* instruction,
                                                           size_t condition_input_index,
                                                           Mips64Label* true_target,
                                                           Mips64Label* false_target) {
  HInstruction* cond = instruction->InputAt(condition_input_index);

  if (true_target == nullptr && false_target == nullptr) {
    // Nothing to do. The code always falls through.
    return;
  } else if (cond->IsIntConstant()) {
    // Constant condition, statically compared against "true" (integer value 1).
    if (cond->AsIntConstant()->IsTrue()) {
      if (true_target != nullptr) {
        __ Bc(true_target);
      }
    } else {
      DCHECK(cond->AsIntConstant()->IsFalse()) << cond->AsIntConstant()->GetValue();
      if (false_target != nullptr) {
        __ Bc(false_target);
      }
    }
    return;
  }

  // The following code generates these patterns:
  //  (1) true_target == nullptr && false_target != nullptr
  //        - opposite condition true => branch to false_target
  //  (2) true_target != nullptr && false_target == nullptr
  //        - condition true => branch to true_target
  //  (3) true_target != nullptr && false_target != nullptr
  //        - condition true => branch to true_target
  //        - branch to false_target
  if (IsBooleanValueOrMaterializedCondition(cond)) {
    // The condition instruction has been materialized, compare the output to 0.
    Location cond_val = instruction->GetLocations()->InAt(condition_input_index);
    DCHECK(cond_val.IsRegister());
    if (true_target == nullptr) {
      __ Beqzc(cond_val.AsRegister<GpuRegister>(), false_target);
    } else {
      __ Bnezc(cond_val.AsRegister<GpuRegister>(), true_target);
    }
  } else {
    // The condition instruction has not been materialized, use its inputs as
    // the comparison and its condition as the branch condition.
    HCondition* condition = cond->AsCondition();
    DataType::Type type = condition->InputAt(0)->GetType();
    LocationSummary* locations = cond->GetLocations();
    IfCondition if_cond = condition->GetCondition();
    Mips64Label* branch_target = true_target;

    if (true_target == nullptr) {
      if_cond = condition->GetOppositeCondition();
      branch_target = false_target;
    }

    switch (type) {
      default:
        GenerateIntLongCompareAndBranch(if_cond, /* is64bit */ false, locations, branch_target);
        break;
      case DataType::Type::kInt64:
        GenerateIntLongCompareAndBranch(if_cond, /* is64bit */ true, locations, branch_target);
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64:
        GenerateFpCompareAndBranch(if_cond, condition->IsGtBias(), type, locations, branch_target);
        break;
    }
  }

  // If neither branch falls through (case 3), the conditional branch to `true_target`
  // was already emitted (case 2) and we need to emit a jump to `false_target`.
  if (true_target != nullptr && false_target != nullptr) {
    __ Bc(false_target);
  }
}

void LocationsBuilderMIPS64::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(if_instr);
  if (IsBooleanValueOrMaterializedCondition(if_instr->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS64::VisitIf(HIf* if_instr) {
  HBasicBlock* true_successor = if_instr->IfTrueSuccessor();
  HBasicBlock* false_successor = if_instr->IfFalseSuccessor();
  Mips64Label* true_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), true_successor) ?
      nullptr : codegen_->GetLabelOf(true_successor);
  Mips64Label* false_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), false_successor) ?
      nullptr : codegen_->GetLabelOf(false_successor);
  GenerateTestAndBranch(if_instr, /* condition_input_index */ 0, true_target, false_target);
}

void LocationsBuilderMIPS64::VisitDeoptimize(HDeoptimize* deoptimize) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(deoptimize, LocationSummary::kCallOnSlowPath);
  InvokeRuntimeCallingConvention calling_convention;
  RegisterSet caller_saves = RegisterSet::Empty();
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetCustomSlowPathCallerSaves(caller_saves);
  if (IsBooleanValueOrMaterializedCondition(deoptimize->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS64::VisitDeoptimize(HDeoptimize* deoptimize) {
  SlowPathCodeMIPS64* slow_path =
      deopt_slow_paths_.NewSlowPath<DeoptimizationSlowPathMIPS64>(deoptimize);
  GenerateTestAndBranch(deoptimize,
                        /* condition_input_index */ 0,
                        slow_path->GetEntryLabel(),
                        /* false_target */ nullptr);
}

// This function returns true if a conditional move can be generated for HSelect.
// Otherwise it returns false and HSelect must be implemented in terms of conditonal
// branches and regular moves.
//
// If `locations_to_set` isn't nullptr, its inputs and outputs are set for HSelect.
//
// While determining feasibility of a conditional move and setting inputs/outputs
// are two distinct tasks, this function does both because they share quite a bit
// of common logic.
static bool CanMoveConditionally(HSelect* select, LocationSummary* locations_to_set) {
  bool materialized = IsBooleanValueOrMaterializedCondition(select->GetCondition());
  HInstruction* cond = select->InputAt(/* condition_input_index */ 2);
  HCondition* condition = cond->AsCondition();

  DataType::Type cond_type =
      materialized ? DataType::Type::kInt32 : condition->InputAt(0)->GetType();
  DataType::Type dst_type = select->GetType();

  HConstant* cst_true_value = select->GetTrueValue()->AsConstant();
  HConstant* cst_false_value = select->GetFalseValue()->AsConstant();
  bool is_true_value_zero_constant =
      (cst_true_value != nullptr && cst_true_value->IsZeroBitPattern());
  bool is_false_value_zero_constant =
      (cst_false_value != nullptr && cst_false_value->IsZeroBitPattern());

  bool can_move_conditionally = false;
  bool use_const_for_false_in = false;
  bool use_const_for_true_in = false;

  if (!cond->IsConstant()) {
    if (!DataType::IsFloatingPointType(cond_type)) {
      if (!DataType::IsFloatingPointType(dst_type)) {
        // Moving int/long on int/long condition.
        if (is_true_value_zero_constant) {
          // seleqz out_reg, false_reg, cond_reg
          can_move_conditionally = true;
          use_const_for_true_in = true;
        } else if (is_false_value_zero_constant) {
          // selnez out_reg, true_reg, cond_reg
          can_move_conditionally = true;
          use_const_for_false_in = true;
        } else if (materialized) {
          // Not materializing unmaterialized int conditions
          // to keep the instruction count low.
          // selnez AT, true_reg, cond_reg
          // seleqz TMP, false_reg, cond_reg
          // or out_reg, AT, TMP
          can_move_conditionally = true;
        }
      } else {
        // Moving float/double on int/long condition.
        if (materialized) {
          // Not materializing unmaterialized int conditions
          // to keep the instruction count low.
          can_move_conditionally = true;
          if (is_true_value_zero_constant) {
            // sltu TMP, ZERO, cond_reg
            // mtc1 TMP, temp_cond_reg
            // seleqz.fmt out_reg, false_reg, temp_cond_reg
            use_const_for_true_in = true;
          } else if (is_false_value_zero_constant) {
            // sltu TMP, ZERO, cond_reg
            // mtc1 TMP, temp_cond_reg
            // selnez.fmt out_reg, true_reg, temp_cond_reg
            use_const_for_false_in = true;
          } else {
            // sltu TMP, ZERO, cond_reg
            // mtc1 TMP, temp_cond_reg
            // sel.fmt temp_cond_reg, false_reg, true_reg
            // mov.fmt out_reg, temp_cond_reg
          }
        }
      }
    } else {
      if (!DataType::IsFloatingPointType(dst_type)) {
        // Moving int/long on float/double condition.
        can_move_conditionally = true;
        if (is_true_value_zero_constant) {
          // mfc1 TMP, temp_cond_reg
          // seleqz out_reg, false_reg, TMP
          use_const_for_true_in = true;
        } else if (is_false_value_zero_constant) {
          // mfc1 TMP, temp_cond_reg
          // selnez out_reg, true_reg, TMP
          use_const_for_false_in = true;
        } else {
          // mfc1 TMP, temp_cond_reg
          // selnez AT, true_reg, TMP
          // seleqz TMP, false_reg, TMP
          // or out_reg, AT, TMP
        }
      } else {
        // Moving float/double on float/double condition.
        can_move_conditionally = true;
        if (is_true_value_zero_constant) {
          // seleqz.fmt out_reg, false_reg, temp_cond_reg
          use_const_for_true_in = true;
        } else if (is_false_value_zero_constant) {
          // selnez.fmt out_reg, true_reg, temp_cond_reg
          use_const_for_false_in = true;
        } else {
          // sel.fmt temp_cond_reg, false_reg, true_reg
          // mov.fmt out_reg, temp_cond_reg
        }
      }
    }
  }

  if (can_move_conditionally) {
    DCHECK(!use_const_for_false_in || !use_const_for_true_in);
  } else {
    DCHECK(!use_const_for_false_in);
    DCHECK(!use_const_for_true_in);
  }

  if (locations_to_set != nullptr) {
    if (use_const_for_false_in) {
      locations_to_set->SetInAt(0, Location::ConstantLocation(cst_false_value));
    } else {
      locations_to_set->SetInAt(0,
                                DataType::IsFloatingPointType(dst_type)
                                    ? Location::RequiresFpuRegister()
                                    : Location::RequiresRegister());
    }
    if (use_const_for_true_in) {
      locations_to_set->SetInAt(1, Location::ConstantLocation(cst_true_value));
    } else {
      locations_to_set->SetInAt(1,
                                DataType::IsFloatingPointType(dst_type)
                                    ? Location::RequiresFpuRegister()
                                    : Location::RequiresRegister());
    }
    if (materialized) {
      locations_to_set->SetInAt(2, Location::RequiresRegister());
    }

    if (can_move_conditionally) {
      locations_to_set->SetOut(DataType::IsFloatingPointType(dst_type)
                                   ? Location::RequiresFpuRegister()
                                   : Location::RequiresRegister());
    } else {
      locations_to_set->SetOut(Location::SameAsFirstInput());
    }
  }

  return can_move_conditionally;
}


void InstructionCodeGeneratorMIPS64::GenConditionalMove(HSelect* select) {
  LocationSummary* locations = select->GetLocations();
  Location dst = locations->Out();
  Location false_src = locations->InAt(0);
  Location true_src = locations->InAt(1);
  HInstruction* cond = select->InputAt(/* condition_input_index */ 2);
  GpuRegister cond_reg = TMP;
  FpuRegister fcond_reg = FTMP;
  DataType::Type cond_type = DataType::Type::kInt32;
  bool cond_inverted = false;
  DataType::Type dst_type = select->GetType();

  if (IsBooleanValueOrMaterializedCondition(cond)) {
    cond_reg = locations->InAt(/* condition_input_index */ 2).AsRegister<GpuRegister>();
  } else {
    HCondition* condition = cond->AsCondition();
    LocationSummary* cond_locations = cond->GetLocations();
    IfCondition if_cond = condition->GetCondition();
    cond_type = condition->InputAt(0)->GetType();
    switch (cond_type) {
      default:
        cond_inverted = MaterializeIntLongCompare(if_cond,
                                                  /* is64bit */ false,
                                                  cond_locations,
                                                  cond_reg);
        break;
      case DataType::Type::kInt64:
        cond_inverted = MaterializeIntLongCompare(if_cond,
                                                  /* is64bit */ true,
                                                  cond_locations,
                                                  cond_reg);
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64:
        cond_inverted = MaterializeFpCompare(if_cond,
                                             condition->IsGtBias(),
                                             cond_type,
                                             cond_locations,
                                             fcond_reg);
        break;
    }
  }

  if (true_src.IsConstant()) {
    DCHECK(true_src.GetConstant()->IsZeroBitPattern());
  }
  if (false_src.IsConstant()) {
    DCHECK(false_src.GetConstant()->IsZeroBitPattern());
  }

  switch (dst_type) {
    default:
      if (DataType::IsFloatingPointType(cond_type)) {
        __ Mfc1(cond_reg, fcond_reg);
      }
      if (true_src.IsConstant()) {
        if (cond_inverted) {
          __ Selnez(dst.AsRegister<GpuRegister>(), false_src.AsRegister<GpuRegister>(), cond_reg);
        } else {
          __ Seleqz(dst.AsRegister<GpuRegister>(), false_src.AsRegister<GpuRegister>(), cond_reg);
        }
      } else if (false_src.IsConstant()) {
        if (cond_inverted) {
          __ Seleqz(dst.AsRegister<GpuRegister>(), true_src.AsRegister<GpuRegister>(), cond_reg);
        } else {
          __ Selnez(dst.AsRegister<GpuRegister>(), true_src.AsRegister<GpuRegister>(), cond_reg);
        }
      } else {
        DCHECK_NE(cond_reg, AT);
        if (cond_inverted) {
          __ Seleqz(AT, true_src.AsRegister<GpuRegister>(), cond_reg);
          __ Selnez(TMP, false_src.AsRegister<GpuRegister>(), cond_reg);
        } else {
          __ Selnez(AT, true_src.AsRegister<GpuRegister>(), cond_reg);
          __ Seleqz(TMP, false_src.AsRegister<GpuRegister>(), cond_reg);
        }
        __ Or(dst.AsRegister<GpuRegister>(), AT, TMP);
      }
      break;
    case DataType::Type::kFloat32: {
      if (!DataType::IsFloatingPointType(cond_type)) {
        // sel*.fmt tests bit 0 of the condition register, account for that.
        __ Sltu(TMP, ZERO, cond_reg);
        __ Mtc1(TMP, fcond_reg);
      }
      FpuRegister dst_reg = dst.AsFpuRegister<FpuRegister>();
      if (true_src.IsConstant()) {
        FpuRegister src_reg = false_src.AsFpuRegister<FpuRegister>();
        if (cond_inverted) {
          __ SelnezS(dst_reg, src_reg, fcond_reg);
        } else {
          __ SeleqzS(dst_reg, src_reg, fcond_reg);
        }
      } else if (false_src.IsConstant()) {
        FpuRegister src_reg = true_src.AsFpuRegister<FpuRegister>();
        if (cond_inverted) {
          __ SeleqzS(dst_reg, src_reg, fcond_reg);
        } else {
          __ SelnezS(dst_reg, src_reg, fcond_reg);
        }
      } else {
        if (cond_inverted) {
          __ SelS(fcond_reg,
                  true_src.AsFpuRegister<FpuRegister>(),
                  false_src.AsFpuRegister<FpuRegister>());
        } else {
          __ SelS(fcond_reg,
                  false_src.AsFpuRegister<FpuRegister>(),
                  true_src.AsFpuRegister<FpuRegister>());
        }
        __ MovS(dst_reg, fcond_reg);
      }
      break;
    }
    case DataType::Type::kFloat64: {
      if (!DataType::IsFloatingPointType(cond_type)) {
        // sel*.fmt tests bit 0 of the condition register, account for that.
        __ Sltu(TMP, ZERO, cond_reg);
        __ Mtc1(TMP, fcond_reg);
      }
      FpuRegister dst_reg = dst.AsFpuRegister<FpuRegister>();
      if (true_src.IsConstant()) {
        FpuRegister src_reg = false_src.AsFpuRegister<FpuRegister>();
        if (cond_inverted) {
          __ SelnezD(dst_reg, src_reg, fcond_reg);
        } else {
          __ SeleqzD(dst_reg, src_reg, fcond_reg);
        }
      } else if (false_src.IsConstant()) {
        FpuRegister src_reg = true_src.AsFpuRegister<FpuRegister>();
        if (cond_inverted) {
          __ SeleqzD(dst_reg, src_reg, fcond_reg);
        } else {
          __ SelnezD(dst_reg, src_reg, fcond_reg);
        }
      } else {
        if (cond_inverted) {
          __ SelD(fcond_reg,
                  true_src.AsFpuRegister<FpuRegister>(),
                  false_src.AsFpuRegister<FpuRegister>());
        } else {
          __ SelD(fcond_reg,
                  false_src.AsFpuRegister<FpuRegister>(),
                  true_src.AsFpuRegister<FpuRegister>());
        }
        __ MovD(dst_reg, fcond_reg);
      }
      break;
    }
  }
}

void LocationsBuilderMIPS64::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(flag, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS64::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  __ LoadFromOffset(kLoadWord,
                    flag->GetLocations()->Out().AsRegister<GpuRegister>(),
                    SP,
                    codegen_->GetStackOffsetOfShouldDeoptimizeFlag());
}

void LocationsBuilderMIPS64::VisitSelect(HSelect* select) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(select);
  CanMoveConditionally(select, locations);
}

void InstructionCodeGeneratorMIPS64::VisitSelect(HSelect* select) {
  if (CanMoveConditionally(select, /* locations_to_set */ nullptr)) {
    GenConditionalMove(select);
  } else {
    LocationSummary* locations = select->GetLocations();
    Mips64Label false_target;
    GenerateTestAndBranch(select,
                          /* condition_input_index */ 2,
                          /* true_target */ nullptr,
                          &false_target);
    codegen_->MoveLocation(locations->Out(), locations->InAt(1), select->GetType());
    __ Bind(&false_target);
  }
}

void LocationsBuilderMIPS64::VisitNativeDebugInfo(HNativeDebugInfo* info) {
  new (GetGraph()->GetAllocator()) LocationSummary(info);
}

void InstructionCodeGeneratorMIPS64::VisitNativeDebugInfo(HNativeDebugInfo*) {
  // MaybeRecordNativeDebugInfo is already called implicitly in CodeGenerator::Compile.
}

void CodeGeneratorMIPS64::GenerateNop() {
  __ Nop();
}

void LocationsBuilderMIPS64::HandleFieldGet(HInstruction* instruction,
                                            const FieldInfo& field_info) {
  DataType::Type field_type = field_info.GetFieldType();
  bool object_field_get_with_read_barrier =
      kEmitCompilerReadBarrier && (field_type == DataType::Type::kReference);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction,
      object_field_get_with_read_barrier
          ? LocationSummary::kCallOnSlowPath
          : LocationSummary::kNoCall);
  if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  locations->SetInAt(0, Location::RequiresRegister());
  if (DataType::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister());
  } else {
    // The output overlaps in the case of an object field get with
    // read barriers enabled: we do not want the move to overwrite the
    // object's location, as we need it to emit the read barrier.
    locations->SetOut(Location::RequiresRegister(),
                      object_field_get_with_read_barrier
                          ? Location::kOutputOverlap
                          : Location::kNoOutputOverlap);
  }
  if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorMIPS64::GenerateFieldLoadWithBakerReadBarrier.
    if (!kBakerReadBarrierThunksEnableForFields) {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
}

void InstructionCodeGeneratorMIPS64::HandleFieldGet(HInstruction* instruction,
                                                    const FieldInfo& field_info) {
  DCHECK_EQ(DataType::Size(field_info.GetFieldType()), DataType::Size(instruction->GetType()));
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  GpuRegister obj = obj_loc.AsRegister<GpuRegister>();
  Location dst_loc = locations->Out();
  LoadOperandType load_type = kLoadUnsignedByte;
  bool is_volatile = field_info.IsVolatile();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();
  auto null_checker = GetImplicitNullChecker(instruction, codegen_);

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
      load_type = kLoadUnsignedByte;
      break;
    case DataType::Type::kInt8:
      load_type = kLoadSignedByte;
      break;
    case DataType::Type::kUint16:
      load_type = kLoadUnsignedHalfword;
      break;
    case DataType::Type::kInt16:
      load_type = kLoadSignedHalfword;
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
      load_type = kLoadWord;
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      load_type = kLoadDoubleword;
      break;
    case DataType::Type::kReference:
      load_type = kLoadUnsignedWord;
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
  if (!DataType::IsFloatingPointType(type)) {
    DCHECK(dst_loc.IsRegister());
    GpuRegister dst = dst_loc.AsRegister<GpuRegister>();
    if (type == DataType::Type::kReference) {
      // /* HeapReference<Object> */ dst = *(obj + offset)
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        Location temp_loc =
            kBakerReadBarrierThunksEnableForFields ? Location::NoLocation() : locations->GetTemp(0);
        // Note that a potential implicit null check is handled in this
        // CodeGeneratorMIPS64::GenerateFieldLoadWithBakerReadBarrier call.
        codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                        dst_loc,
                                                        obj,
                                                        offset,
                                                        temp_loc,
                                                        /* needs_null_check */ true);
        if (is_volatile) {
          GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
        }
      } else {
        __ LoadFromOffset(kLoadUnsignedWord, dst, obj, offset, null_checker);
        if (is_volatile) {
          GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
        }
        // If read barriers are enabled, emit read barriers other than
        // Baker's using a slow path (and also unpoison the loaded
        // reference, if heap poisoning is enabled).
        codegen_->MaybeGenerateReadBarrierSlow(instruction, dst_loc, dst_loc, obj_loc, offset);
      }
    } else {
      __ LoadFromOffset(load_type, dst, obj, offset, null_checker);
    }
  } else {
    DCHECK(dst_loc.IsFpuRegister());
    FpuRegister dst = dst_loc.AsFpuRegister<FpuRegister>();
    __ LoadFpuFromOffset(load_type, dst, obj, offset, null_checker);
  }

  // Memory barriers, in the case of references, are handled in the
  // previous switch statement.
  if (is_volatile && (type != DataType::Type::kReference)) {
    GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
  }
}

void LocationsBuilderMIPS64::HandleFieldSet(HInstruction* instruction,
                                            const FieldInfo& field_info ATTRIBUTE_UNUSED) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (DataType::IsFloatingPointType(instruction->InputAt(1)->GetType())) {
    locations->SetInAt(1, FpuRegisterOrConstantForStore(instruction->InputAt(1)));
  } else {
    locations->SetInAt(1, RegisterOrZeroConstant(instruction->InputAt(1)));
  }
}

void InstructionCodeGeneratorMIPS64::HandleFieldSet(HInstruction* instruction,
                                                    const FieldInfo& field_info,
                                                    bool value_can_be_null) {
  DataType::Type type = field_info.GetFieldType();
  LocationSummary* locations = instruction->GetLocations();
  GpuRegister obj = locations->InAt(0).AsRegister<GpuRegister>();
  Location value_location = locations->InAt(1);
  StoreOperandType store_type = kStoreByte;
  bool is_volatile = field_info.IsVolatile();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();
  bool needs_write_barrier = CodeGenerator::StoreNeedsWriteBarrier(type, instruction->InputAt(1));
  auto null_checker = GetImplicitNullChecker(instruction, codegen_);

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      store_type = kStoreByte;
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      store_type = kStoreHalfword;
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
    case DataType::Type::kReference:
      store_type = kStoreWord;
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      store_type = kStoreDoubleword;
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }

  if (is_volatile) {
    GenerateMemoryBarrier(MemBarrierKind::kAnyStore);
  }

  if (value_location.IsConstant()) {
    int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
    __ StoreConstToOffset(store_type, value, obj, offset, TMP, null_checker);
  } else {
    if (!DataType::IsFloatingPointType(type)) {
      DCHECK(value_location.IsRegister());
      GpuRegister src = value_location.AsRegister<GpuRegister>();
      if (kPoisonHeapReferences && needs_write_barrier) {
        // Note that in the case where `value` is a null reference,
        // we do not enter this block, as a null reference does not
        // need poisoning.
        DCHECK_EQ(type, DataType::Type::kReference);
        __ PoisonHeapReference(TMP, src);
        __ StoreToOffset(store_type, TMP, obj, offset, null_checker);
      } else {
        __ StoreToOffset(store_type, src, obj, offset, null_checker);
      }
    } else {
      DCHECK(value_location.IsFpuRegister());
      FpuRegister src = value_location.AsFpuRegister<FpuRegister>();
      __ StoreFpuToOffset(store_type, src, obj, offset, null_checker);
    }
  }

  if (needs_write_barrier) {
    DCHECK(value_location.IsRegister());
    GpuRegister src = value_location.AsRegister<GpuRegister>();
    codegen_->MarkGCCard(obj, src, value_can_be_null);
  }

  if (is_volatile) {
    GenerateMemoryBarrier(MemBarrierKind::kAnyAny);
  }
}

void LocationsBuilderMIPS64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void InstructionCodeGeneratorMIPS64::GenerateReferenceLoadOneRegister(
    HInstruction* instruction,
    Location out,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  GpuRegister out_reg = out.AsRegister<GpuRegister>();
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    if (!kUseBakerReadBarrier || !kBakerReadBarrierThunksEnableForFields) {
      DCHECK(maybe_temp.IsRegister()) << maybe_temp;
    }
    if (kUseBakerReadBarrier) {
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(out + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                      out,
                                                      out_reg,
                                                      offset,
                                                      maybe_temp,
                                                      /* needs_null_check */ false);
    } else {
      // Load with slow path based read barrier.
      // Save the value of `out` into `maybe_temp` before overwriting it
      // in the following move operation, as we will need it for the
      // read barrier below.
      __ Move(maybe_temp.AsRegister<GpuRegister>(), out_reg);
      // /* HeapReference<Object> */ out = *(out + offset)
      __ LoadFromOffset(kLoadUnsignedWord, out_reg, out_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, maybe_temp, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(out + offset)
    __ LoadFromOffset(kLoadUnsignedWord, out_reg, out_reg, offset);
    __ MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorMIPS64::GenerateReferenceLoadTwoRegisters(
    HInstruction* instruction,
    Location out,
    Location obj,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  GpuRegister out_reg = out.AsRegister<GpuRegister>();
  GpuRegister obj_reg = obj.AsRegister<GpuRegister>();
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      if (!kBakerReadBarrierThunksEnableForFields) {
        DCHECK(maybe_temp.IsRegister()) << maybe_temp;
      }
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                      out,
                                                      obj_reg,
                                                      offset,
                                                      maybe_temp,
                                                      /* needs_null_check */ false);
    } else {
      // Load with slow path based read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      __ LoadFromOffset(kLoadUnsignedWord, out_reg, obj_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, obj, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(obj + offset)
    __ LoadFromOffset(kLoadUnsignedWord, out_reg, obj_reg, offset);
    __ MaybeUnpoisonHeapReference(out_reg);
  }
}

static inline int GetBakerMarkThunkNumber(GpuRegister reg) {
  static_assert(BAKER_MARK_INTROSPECTION_REGISTER_COUNT == 20, "Expecting equal");
  if (reg >= V0 && reg <= T2) {  // 13 consequtive regs.
    return reg - V0;
  } else if (reg >= S2 && reg <= S7) {  // 6 consequtive regs.
    return 13 + (reg - S2);
  } else if (reg == S8) {  // One more.
    return 19;
  }
  LOG(FATAL) << "Unexpected register " << reg;
  UNREACHABLE();
}

static inline int GetBakerMarkFieldArrayThunkDisplacement(GpuRegister reg, bool short_offset) {
  int num = GetBakerMarkThunkNumber(reg) +
      (short_offset ? BAKER_MARK_INTROSPECTION_REGISTER_COUNT : 0);
  return num * BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE;
}

static inline int GetBakerMarkGcRootThunkDisplacement(GpuRegister reg) {
  return GetBakerMarkThunkNumber(reg) * BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE +
      BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET;
}

void InstructionCodeGeneratorMIPS64::GenerateGcRootFieldLoad(HInstruction* instruction,
                                                             Location root,
                                                             GpuRegister obj,
                                                             uint32_t offset,
                                                             ReadBarrierOption read_barrier_option,
                                                             Mips64Label* label_low) {
  if (label_low != nullptr) {
    DCHECK_EQ(offset, 0x5678u);
  }
  GpuRegister root_reg = root.AsRegister<GpuRegister>();
  if (read_barrier_option == kWithReadBarrier) {
    DCHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      // Fast path implementation of art::ReadBarrier::BarrierForRoot when
      // Baker's read barrier are used:
      if (kBakerReadBarrierThunksEnableForGcRoots) {
        // Note that we do not actually check the value of `GetIsGcMarking()`
        // to decide whether to mark the loaded GC root or not.  Instead, we
        // load into `temp` (T9) the read barrier mark introspection entrypoint.
        // If `temp` is null, it means that `GetIsGcMarking()` is false, and
        // vice versa.
        //
        // We use thunks for the slow path. That thunk checks the reference
        // and jumps to the entrypoint if needed.
        //
        //     temp = Thread::Current()->pReadBarrierMarkReg00
        //     // AKA &art_quick_read_barrier_mark_introspection.
        //     GcRoot<mirror::Object> root = *(obj+offset);  // Original reference load.
        //     if (temp != nullptr) {
        //        temp = &gc_root_thunk<root_reg>
        //        root = temp(root)
        //     }

        const int32_t entry_point_offset =
            Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(0);
        const int thunk_disp = GetBakerMarkGcRootThunkDisplacement(root_reg);
        int16_t offset_low = Low16Bits(offset);
        int16_t offset_high = High16Bits(offset - offset_low);  // Accounts for sign
                                                                // extension in lwu.
        bool short_offset = IsInt<16>(static_cast<int32_t>(offset));
        GpuRegister base = short_offset ? obj : TMP;
        // Loading the entrypoint does not require a load acquire since it is only changed when
        // threads are suspended or running a checkpoint.
        __ LoadFromOffset(kLoadDoubleword, T9, TR, entry_point_offset);
        if (!short_offset) {
          DCHECK(!label_low);
          __ Daui(base, obj, offset_high);
        }
        Mips64Label skip_call;
        __ Beqz(T9, &skip_call, /* is_bare */ true);
        if (label_low != nullptr) {
          DCHECK(short_offset);
          __ Bind(label_low);
        }
        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        __ LoadFromOffset(kLoadUnsignedWord, root_reg, base, offset_low);  // Single instruction
                                                                           // in delay slot.
        __ Jialc(T9, thunk_disp);
        __ Bind(&skip_call);
      } else {
        // Note that we do not actually check the value of `GetIsGcMarking()`
        // to decide whether to mark the loaded GC root or not.  Instead, we
        // load into `temp` (T9) the read barrier mark entry point corresponding
        // to register `root`. If `temp` is null, it means that `GetIsGcMarking()`
        // is false, and vice versa.
        //
        //     GcRoot<mirror::Object> root = *(obj+offset);  // Original reference load.
        //     temp = Thread::Current()->pReadBarrierMarkReg ## root.reg()
        //     if (temp != null) {
        //       root = temp(root)
        //     }

        if (label_low != nullptr) {
          __ Bind(label_low);
        }
        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        __ LoadFromOffset(kLoadUnsignedWord, root_reg, obj, offset);
        static_assert(
            sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(GcRoot<mirror::Object>),
            "art::mirror::CompressedReference<mirror::Object> and art::GcRoot<mirror::Object> "
            "have different sizes.");
        static_assert(sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(int32_t),
                      "art::mirror::CompressedReference<mirror::Object> and int32_t "
                      "have different sizes.");

        // Slow path marking the GC root `root`.
        Location temp = Location::RegisterLocation(T9);
        SlowPathCodeMIPS64* slow_path =
            new (codegen_->GetScopedAllocator()) ReadBarrierMarkSlowPathMIPS64(
                instruction,
                root,
                /*entrypoint*/ temp);
        codegen_->AddSlowPath(slow_path);

        const int32_t entry_point_offset =
            Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(root.reg() - 1);
        // Loading the entrypoint does not require a load acquire since it is only changed when
        // threads are suspended or running a checkpoint.
        __ LoadFromOffset(kLoadDoubleword, temp.AsRegister<GpuRegister>(), TR, entry_point_offset);
        __ Bnezc(temp.AsRegister<GpuRegister>(), slow_path->GetEntryLabel());
        __ Bind(slow_path->GetExitLabel());
      }
    } else {
      if (label_low != nullptr) {
        __ Bind(label_low);
      }
      // GC root loaded through a slow path for read barriers other
      // than Baker's.
      // /* GcRoot<mirror::Object>* */ root = obj + offset
      __ Daddiu64(root_reg, obj, static_cast<int32_t>(offset));
      // /* mirror::Object* */ root = root->Read()
      codegen_->GenerateReadBarrierForRootSlow(instruction, root, root);
    }
  } else {
    if (label_low != nullptr) {
      __ Bind(label_low);
    }
    // Plain GC root load with no read barrier.
    // /* GcRoot<mirror::Object> */ root = *(obj + offset)
    __ LoadFromOffset(kLoadUnsignedWord, root_reg, obj, offset);
    // Note that GC roots are not affected by heap poisoning, thus we
    // do not have to unpoison `root_reg` here.
  }
}

void CodeGeneratorMIPS64::GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                Location ref,
                                                                GpuRegister obj,
                                                                uint32_t offset,
                                                                Location temp,
                                                                bool needs_null_check) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  if (kBakerReadBarrierThunksEnableForFields) {
    // Note that we do not actually check the value of `GetIsGcMarking()`
    // to decide whether to mark the loaded reference or not.  Instead, we
    // load into `temp` (T9) the read barrier mark introspection entrypoint.
    // If `temp` is null, it means that `GetIsGcMarking()` is false, and
    // vice versa.
    //
    // We use thunks for the slow path. That thunk checks the reference
    // and jumps to the entrypoint if needed. If the holder is not gray,
    // it issues a load-load memory barrier and returns to the original
    // reference load.
    //
    //     temp = Thread::Current()->pReadBarrierMarkReg00
    //     // AKA &art_quick_read_barrier_mark_introspection.
    //     if (temp != nullptr) {
    //        temp = &field_array_thunk<holder_reg>
    //        temp()
    //     }
    //   not_gray_return_address:
    //     // If the offset is too large to fit into the lw instruction, we
    //     // use an adjusted base register (TMP) here. This register
    //     // receives bits 16 ... 31 of the offset before the thunk invocation
    //     // and the thunk benefits from it.
    //     HeapReference<mirror::Object> reference = *(obj+offset);  // Original reference load.
    //   gray_return_address:

    DCHECK(temp.IsInvalid());
    bool short_offset = IsInt<16>(static_cast<int32_t>(offset));
    const int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(0);
    // There may have or may have not been a null check if the field offset is smaller than
    // the page size.
    // There must've been a null check in case it's actually a load from an array.
    // We will, however, perform an explicit null check in the thunk as it's easier to
    // do it than not.
    if (instruction->IsArrayGet()) {
      DCHECK(!needs_null_check);
    }
    const int thunk_disp = GetBakerMarkFieldArrayThunkDisplacement(obj, short_offset);
    // Loading the entrypoint does not require a load acquire since it is only changed when
    // threads are suspended or running a checkpoint.
    __ LoadFromOffset(kLoadDoubleword, T9, TR, entry_point_offset);
    GpuRegister ref_reg = ref.AsRegister<GpuRegister>();
    Mips64Label skip_call;
    if (short_offset) {
      __ Beqzc(T9, &skip_call, /* is_bare */ true);
      __ Nop();  // In forbidden slot.
      __ Jialc(T9, thunk_disp);
      __ Bind(&skip_call);
      // /* HeapReference<Object> */ ref = *(obj + offset)
      __ LoadFromOffset(kLoadUnsignedWord, ref_reg, obj, offset);  // Single instruction.
    } else {
      int16_t offset_low = Low16Bits(offset);
      int16_t offset_high = High16Bits(offset - offset_low);  // Accounts for sign extension in lwu.
      __ Beqz(T9, &skip_call, /* is_bare */ true);
      __ Daui(TMP, obj, offset_high);  // In delay slot.
      __ Jialc(T9, thunk_disp);
      __ Bind(&skip_call);
      // /* HeapReference<Object> */ ref = *(obj + offset)
      __ LoadFromOffset(kLoadUnsignedWord, ref_reg, TMP, offset_low);  // Single instruction.
    }
    if (needs_null_check) {
      MaybeRecordImplicitNullCheck(instruction);
    }
    __ MaybeUnpoisonHeapReference(ref_reg);
    return;
  }

  // /* HeapReference<Object> */ ref = *(obj + offset)
  Location no_index = Location::NoLocation();
  ScaleFactor no_scale_factor = TIMES_1;
  GenerateReferenceLoadWithBakerReadBarrier(instruction,
                                            ref,
                                            obj,
                                            offset,
                                            no_index,
                                            no_scale_factor,
                                            temp,
                                            needs_null_check);
}

void CodeGeneratorMIPS64::GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                Location ref,
                                                                GpuRegister obj,
                                                                uint32_t data_offset,
                                                                Location index,
                                                                Location temp,
                                                                bool needs_null_check) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  static_assert(
      sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
      "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
  ScaleFactor scale_factor = TIMES_4;

  if (kBakerReadBarrierThunksEnableForArrays) {
    // Note that we do not actually check the value of `GetIsGcMarking()`
    // to decide whether to mark the loaded reference or not.  Instead, we
    // load into `temp` (T9) the read barrier mark introspection entrypoint.
    // If `temp` is null, it means that `GetIsGcMarking()` is false, and
    // vice versa.
    //
    // We use thunks for the slow path. That thunk checks the reference
    // and jumps to the entrypoint if needed. If the holder is not gray,
    // it issues a load-load memory barrier and returns to the original
    // reference load.
    //
    //     temp = Thread::Current()->pReadBarrierMarkReg00
    //     // AKA &art_quick_read_barrier_mark_introspection.
    //     if (temp != nullptr) {
    //        temp = &field_array_thunk<holder_reg>
    //        temp()
    //     }
    //   not_gray_return_address:
    //     // The element address is pre-calculated in the TMP register before the
    //     // thunk invocation and the thunk benefits from it.
    //     HeapReference<mirror::Object> reference = data[index];  // Original reference load.
    //   gray_return_address:

    DCHECK(temp.IsInvalid());
    DCHECK(index.IsValid());
    const int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kMips64PointerSize>(0);
    // We will not do the explicit null check in the thunk as some form of a null check
    // must've been done earlier.
    DCHECK(!needs_null_check);
    const int thunk_disp = GetBakerMarkFieldArrayThunkDisplacement(obj, /* short_offset */ false);
    // Loading the entrypoint does not require a load acquire since it is only changed when
    // threads are suspended or running a checkpoint.
    __ LoadFromOffset(kLoadDoubleword, T9, TR, entry_point_offset);
    Mips64Label skip_call;
    __ Beqz(T9, &skip_call, /* is_bare */ true);
    GpuRegister ref_reg = ref.AsRegister<GpuRegister>();
    GpuRegister index_reg = index.AsRegister<GpuRegister>();
    __ Dlsa(TMP, index_reg, obj, scale_factor);  // In delay slot.
    __ Jialc(T9, thunk_disp);
    __ Bind(&skip_call);
    // /* HeapReference<Object> */ ref = *(obj + data_offset + (index << scale_factor))
    DCHECK(IsInt<16>(static_cast<int32_t>(data_offset))) << data_offset;
    __ LoadFromOffset(kLoadUnsignedWord, ref_reg, TMP, data_offset);  // Single instruction.
    __ MaybeUnpoisonHeapReference(ref_reg);
    return;
  }

  // /* HeapReference<Object> */ ref =
  //     *(obj + data_offset + index * sizeof(HeapReference<Object>))
  GenerateReferenceLoadWithBakerReadBarrier(instruction,
                                            ref,
                                            obj,
                                            data_offset,
                                            index,
                                            scale_factor,
                                            temp,
                                            needs_null_check);
}

void CodeGeneratorMIPS64::GenerateReferenceLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                    Location ref,
                                                                    GpuRegister obj,
                                                                    uint32_t offset,
                                                                    Location index,
                                                                    ScaleFactor scale_factor,
                                                                    Location temp,
                                                                    bool needs_null_check,
                                                                    bool always_update_field) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  // In slow path based read barriers, the read barrier call is
  // inserted after the original load. However, in fast path based
  // Baker's read barriers, we need to perform the load of
  // mirror::Object::monitor_ *before* the original reference load.
  // This load-load ordering is required by the read barrier.
  // The fast path/slow path (for Baker's algorithm) should look like:
  //
  //   uint32_t rb_state = Lockword(obj->monitor_).ReadBarrierState();
  //   lfence;  // Load fence or artificial data dependency to prevent load-load reordering
  //   HeapReference<Object> ref = *src;  // Original reference load.
  //   bool is_gray = (rb_state == ReadBarrier::GrayState());
  //   if (is_gray) {
  //     ref = ReadBarrier::Mark(ref);  // Performed by runtime entrypoint slow path.
  //   }
  //
  // Note: the original implementation in ReadBarrier::Barrier is
  // slightly more complex as it performs additional checks that we do
  // not do here for performance reasons.

  GpuRegister ref_reg = ref.AsRegister<GpuRegister>();
  GpuRegister temp_reg = temp.AsRegister<GpuRegister>();
  uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();

  // /* int32_t */ monitor = obj->monitor_
  __ LoadFromOffset(kLoadWord, temp_reg, obj, monitor_offset);
  if (needs_null_check) {
    MaybeRecordImplicitNullCheck(instruction);
  }
  // /* LockWord */ lock_word = LockWord(monitor)
  static_assert(sizeof(LockWord) == sizeof(int32_t),
                "art::LockWord and int32_t have different sizes.");

  __ Sync(0);  // Barrier to prevent load-load reordering.

  // The actual reference load.
  if (index.IsValid()) {
    // Load types involving an "index": ArrayGet,
    // UnsafeGetObject/UnsafeGetObjectVolatile and UnsafeCASObject
    // intrinsics.
    // /* HeapReference<Object> */ ref = *(obj + offset + (index << scale_factor))
    if (index.IsConstant()) {
      size_t computed_offset =
          (index.GetConstant()->AsIntConstant()->GetValue() << scale_factor) + offset;
      __ LoadFromOffset(kLoadUnsignedWord, ref_reg, obj, computed_offset);
    } else {
      GpuRegister index_reg = index.AsRegister<GpuRegister>();
      if (scale_factor == TIMES_1) {
        __ Daddu(TMP, index_reg, obj);
      } else {
        __ Dlsa(TMP, index_reg, obj, scale_factor);
      }
      __ LoadFromOffset(kLoadUnsignedWord, ref_reg, TMP, offset);
    }
  } else {
    // /* HeapReference<Object> */ ref = *(obj + offset)
    __ LoadFromOffset(kLoadUnsignedWord, ref_reg, obj, offset);
  }

  // Object* ref = ref_addr->AsMirrorPtr()
  __ MaybeUnpoisonHeapReference(ref_reg);

  // Slow path marking the object `ref` when it is gray.
  SlowPathCodeMIPS64* slow_path;
  if (always_update_field) {
    // ReadBarrierMarkAndUpdateFieldSlowPathMIPS64 only supports address
    // of the form `obj + field_offset`, where `obj` is a register and
    // `field_offset` is a register. Thus `offset` and `scale_factor`
    // above are expected to be null in this code path.
    DCHECK_EQ(offset, 0u);
    DCHECK_EQ(scale_factor, ScaleFactor::TIMES_1);
    slow_path = new (GetScopedAllocator())
        ReadBarrierMarkAndUpdateFieldSlowPathMIPS64(instruction,
                                                    ref,
                                                    obj,
                                                    /* field_offset */ index,
                                                    temp_reg);
  } else {
    slow_path = new (GetScopedAllocator()) ReadBarrierMarkSlowPathMIPS64(instruction, ref);
  }
  AddSlowPath(slow_path);

  // if (rb_state == ReadBarrier::GrayState())
  //   ref = ReadBarrier::Mark(ref);
  // Given the numeric representation, it's enough to check the low bit of the
  // rb_state. We do that by shifting the bit into the sign bit (31) and
  // performing a branch on less than zero.
  static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
  static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
  static_assert(LockWord::kReadBarrierStateSize == 1, "Expecting 1-bit read barrier state size");
  __ Sll(temp_reg, temp_reg, 31 - LockWord::kReadBarrierStateShift);
  __ Bltzc(temp_reg, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorMIPS64::GenerateReadBarrierSlow(HInstruction* instruction,
                                                  Location out,
                                                  Location ref,
                                                  Location obj,
                                                  uint32_t offset,
                                                  Location index) {
  DCHECK(kEmitCompilerReadBarrier);

  // Insert a slow path based read barrier *after* the reference load.
  //
  // If heap poisoning is enabled, the unpoisoning of the loaded
  // reference will be carried out by the runtime within the slow
  // path.
  //
  // Note that `ref` currently does not get unpoisoned (when heap
  // poisoning is enabled), which is alright as the `ref` argument is
  // not used by the artReadBarrierSlow entry point.
  //
  // TODO: Unpoison `ref` when it is used by artReadBarrierSlow.
  SlowPathCodeMIPS64* slow_path = new (GetScopedAllocator())
      ReadBarrierForHeapReferenceSlowPathMIPS64(instruction, out, ref, obj, offset, index);
  AddSlowPath(slow_path);

  __ Bc(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorMIPS64::MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                                       Location out,
                                                       Location ref,
                                                       Location obj,
                                                       uint32_t offset,
                                                       Location index) {
  if (kEmitCompilerReadBarrier) {
    // Baker's read barriers shall be handled by the fast path
    // (CodeGeneratorMIPS64::GenerateReferenceLoadWithBakerReadBarrier).
    DCHECK(!kUseBakerReadBarrier);
    // If heap poisoning is enabled, unpoisoning will be taken care of
    // by the runtime within the slow path.
    GenerateReadBarrierSlow(instruction, out, ref, obj, offset, index);
  } else if (kPoisonHeapReferences) {
    __ UnpoisonHeapReference(out.AsRegister<GpuRegister>());
  }
}

void CodeGeneratorMIPS64::GenerateReadBarrierForRootSlow(HInstruction* instruction,
                                                         Location out,
                                                         Location root) {
  DCHECK(kEmitCompilerReadBarrier);

  // Insert a slow path based read barrier *after* the GC root load.
  //
  // Note that GC roots are not affected by heap poisoning, so we do
  // not need to do anything special for this here.
  SlowPathCodeMIPS64* slow_path =
      new (GetScopedAllocator()) ReadBarrierForRootSlowPathMIPS64(instruction, out, root);
  AddSlowPath(slow_path);

  __ Bc(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void LocationsBuilderMIPS64::VisitInstanceOf(HInstanceOf* instruction) {
  LocationSummary::CallKind call_kind = LocationSummary::kNoCall;
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  bool baker_read_barrier_slow_path = false;
  switch (type_check_kind) {
    case TypeCheckKind::kExactCheck:
    case TypeCheckKind::kAbstractClassCheck:
    case TypeCheckKind::kClassHierarchyCheck:
    case TypeCheckKind::kArrayObjectCheck: {
      bool needs_read_barrier = CodeGenerator::InstanceOfNeedsReadBarrier(instruction);
      call_kind = needs_read_barrier ? LocationSummary::kCallOnSlowPath : LocationSummary::kNoCall;
      baker_read_barrier_slow_path = kUseBakerReadBarrier && needs_read_barrier;
      break;
    }
    case TypeCheckKind::kArrayCheck:
    case TypeCheckKind::kUnresolvedCheck:
    case TypeCheckKind::kInterfaceCheck:
      call_kind = LocationSummary::kCallOnSlowPath;
      break;
  }

  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  if (baker_read_barrier_slow_path) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  // The output does overlap inputs.
  // Note that TypeCheckSlowPathMIPS64 uses this register too.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
  locations->AddRegisterTemps(NumberOfInstanceOfTemps(type_check_kind));
}

void InstructionCodeGeneratorMIPS64::VisitInstanceOf(HInstanceOf* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  GpuRegister obj = obj_loc.AsRegister<GpuRegister>();
  GpuRegister cls = locations->InAt(1).AsRegister<GpuRegister>();
  Location out_loc = locations->Out();
  GpuRegister out = out_loc.AsRegister<GpuRegister>();
  const size_t num_temps = NumberOfInstanceOfTemps(type_check_kind);
  DCHECK_LE(num_temps, 1u);
  Location maybe_temp_loc = (num_temps >= 1) ? locations->GetTemp(0) : Location::NoLocation();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  Mips64Label done;
  SlowPathCodeMIPS64* slow_path = nullptr;

  // Return 0 if `obj` is null.
  // Avoid this check if we know `obj` is not null.
  if (instruction->MustDoNullCheck()) {
    __ Move(out, ZERO);
    __ Beqzc(obj, &done);
  }

  switch (type_check_kind) {
    case TypeCheckKind::kExactCheck: {
      ReadBarrierOption read_barrier_option =
          CodeGenerator::ReadBarrierOptionForInstanceOf(instruction);
      // /* HeapReference<Class> */ out = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        out_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp_loc,
                                        read_barrier_option);
      // Classes must be equal for the instanceof to succeed.
      __ Xor(out, out, cls);
      __ Sltiu(out, out, 1);
      break;
    }

    case TypeCheckKind::kAbstractClassCheck: {
      ReadBarrierOption read_barrier_option =
          CodeGenerator::ReadBarrierOptionForInstanceOf(instruction);
      // /* HeapReference<Class> */ out = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        out_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp_loc,
                                        read_barrier_option);
      // If the class is abstract, we eagerly fetch the super class of the
      // object to avoid doing a comparison we know will fail.
      Mips64Label loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Beqzc(out, &done);
      __ Bnec(out, cls, &loop);
      __ LoadConst32(out, 1);
      break;
    }

    case TypeCheckKind::kClassHierarchyCheck: {
      ReadBarrierOption read_barrier_option =
          CodeGenerator::ReadBarrierOptionForInstanceOf(instruction);
      // /* HeapReference<Class> */ out = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        out_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp_loc,
                                        read_barrier_option);
      // Walk over the class hierarchy to find a match.
      Mips64Label loop, success;
      __ Bind(&loop);
      __ Beqc(out, cls, &success);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      __ Bnezc(out, &loop);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Bc(&done);
      __ Bind(&success);
      __ LoadConst32(out, 1);
      break;
    }

    case TypeCheckKind::kArrayObjectCheck: {
      ReadBarrierOption read_barrier_option =
          CodeGenerator::ReadBarrierOptionForInstanceOf(instruction);
      // /* HeapReference<Class> */ out = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        out_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp_loc,
                                        read_barrier_option);
      // Do an exact check.
      Mips64Label success;
      __ Beqc(out, cls, &success);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ out = out->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       component_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Beqzc(out, &done);
      __ LoadFromOffset(kLoadUnsignedHalfword, out, out, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Sltiu(out, out, 1);
      __ Bc(&done);
      __ Bind(&success);
      __ LoadConst32(out, 1);
      break;
    }

    case TypeCheckKind::kArrayCheck: {
      // No read barrier since the slow path will retry upon failure.
      // /* HeapReference<Class> */ out = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        out_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp_loc,
                                        kWithoutReadBarrier);
      DCHECK(locations->OnlyCallsOnSlowPath());
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS64(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ Bnec(out, cls, slow_path->GetEntryLabel());
      __ LoadConst32(out, 1);
      break;
    }

    case TypeCheckKind::kUnresolvedCheck:
    case TypeCheckKind::kInterfaceCheck: {
      // Note that we indeed only call on slow path, but we always go
      // into the slow path for the unresolved and interface check
      // cases.
      //
      // We cannot directly call the InstanceofNonTrivial runtime
      // entry point without resorting to a type checking slow path
      // here (i.e. by calling InvokeRuntime directly), as it would
      // require to assign fixed registers for the inputs of this
      // HInstanceOf instruction (following the runtime calling
      // convention), which might be cluttered by the potential first
      // read barrier emission at the beginning of this method.
      //
      // TODO: Introduce a new runtime entry point taking the object
      // to test (instead of its class) as argument, and let it deal
      // with the read barrier issues. This will let us refactor this
      // case of the `switch` code as it was previously (with a direct
      // call to the runtime not using a type checking slow path).
      // This should also be beneficial for the other cases above.
      DCHECK(locations->OnlyCallsOnSlowPath());
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS64(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ Bc(slow_path->GetEntryLabel());
      break;
    }
  }

  __ Bind(&done);

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void LocationsBuilderMIPS64::VisitIntConstant(HIntConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS64::VisitIntConstant(HIntConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS64::VisitNullConstant(HNullConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS64::VisitNullConstant(HNullConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS64::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  // The trampoline uses the same calling convention as dex calling conventions,
  // except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
  // the method_idx.
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorMIPS64::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  codegen_->GenerateInvokeUnresolvedRuntimeCall(invoke);
}

void LocationsBuilderMIPS64::HandleInvoke(HInvoke* invoke) {
  InvokeDexCallingConventionVisitorMIPS64 calling_convention_visitor;
  CodeGenerator::CreateCommonInvokeLocationSummary(invoke, &calling_convention_visitor);
}

void LocationsBuilderMIPS64::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
  // The register T0 is required to be used for the hidden argument in
  // art_quick_imt_conflict_trampoline, so add the hidden argument.
  invoke->GetLocations()->AddTemp(Location::RegisterLocation(T0));
}

void InstructionCodeGeneratorMIPS64::VisitInvokeInterface(HInvokeInterface* invoke) {
  // TODO: b/18116999, our IMTs can miss an IncompatibleClassChangeError.
  GpuRegister temp = invoke->GetLocations()->GetTemp(0).AsRegister<GpuRegister>();
  Location receiver = invoke->GetLocations()->InAt(0);
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMips64PointerSize);

  // Set the hidden argument.
  __ LoadConst32(invoke->GetLocations()->GetTemp(1).AsRegister<GpuRegister>(),
                 invoke->GetDexMethodIndex());

  // temp = object->GetClass();
  if (receiver.IsStackSlot()) {
    __ LoadFromOffset(kLoadUnsignedWord, temp, SP, receiver.GetStackIndex());
    __ LoadFromOffset(kLoadUnsignedWord, temp, temp, class_offset);
  } else {
    __ LoadFromOffset(kLoadUnsignedWord, temp, receiver.AsRegister<GpuRegister>(), class_offset);
  }
  codegen_->MaybeRecordImplicitNullCheck(invoke);
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  __ MaybeUnpoisonHeapReference(temp);
  __ LoadFromOffset(kLoadDoubleword, temp, temp,
      mirror::Class::ImtPtrOffset(kMips64PointerSize).Uint32Value());
  uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
      invoke->GetImtIndex(), kMips64PointerSize));
  // temp = temp->GetImtEntryAt(method_offset);
  __ LoadFromOffset(kLoadDoubleword, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadDoubleword, T9, temp, entry_point.Int32Value());
  // T9();
  __ Jalr(T9);
  __ Nop();
  DCHECK(!codegen_->IsLeafMethod());
  codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
}

void LocationsBuilderMIPS64::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  IntrinsicLocationsBuilderMIPS64 intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

void LocationsBuilderMIPS64::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  IntrinsicLocationsBuilderMIPS64 intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

void LocationsBuilderMIPS64::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorMIPS64::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  codegen_->GenerateInvokePolymorphicCall(invoke);
}

static bool TryGenerateIntrinsicCode(HInvoke* invoke, CodeGeneratorMIPS64* codegen) {
  if (invoke->GetLocations()->Intrinsified()) {
    IntrinsicCodeGeneratorMIPS64 intrinsic(codegen);
    intrinsic.Dispatch(invoke);
    return true;
  }
  return false;
}

HLoadString::LoadKind CodeGeneratorMIPS64::GetSupportedLoadStringKind(
    HLoadString::LoadKind desired_string_load_kind) {
  bool fallback_load = false;
  switch (desired_string_load_kind) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadString::LoadKind::kBootImageInternTable:
    case HLoadString::LoadKind::kBssEntry:
      DCHECK(!Runtime::Current()->UseJitCompilation());
      break;
    case HLoadString::LoadKind::kJitTableAddress:
      DCHECK(Runtime::Current()->UseJitCompilation());
      break;
    case HLoadString::LoadKind::kBootImageAddress:
    case HLoadString::LoadKind::kRuntimeCall:
      break;
  }
  if (fallback_load) {
    desired_string_load_kind = HLoadString::LoadKind::kRuntimeCall;
  }
  return desired_string_load_kind;
}

HLoadClass::LoadKind CodeGeneratorMIPS64::GetSupportedLoadClassKind(
    HLoadClass::LoadKind desired_class_load_kind) {
  bool fallback_load = false;
  switch (desired_class_load_kind) {
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
    case HLoadClass::LoadKind::kReferrersClass:
      break;
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadClass::LoadKind::kBootImageClassTable:
    case HLoadClass::LoadKind::kBssEntry:
      DCHECK(!Runtime::Current()->UseJitCompilation());
      break;
    case HLoadClass::LoadKind::kJitTableAddress:
      DCHECK(Runtime::Current()->UseJitCompilation());
      break;
    case HLoadClass::LoadKind::kBootImageAddress:
    case HLoadClass::LoadKind::kRuntimeCall:
      break;
  }
  if (fallback_load) {
    desired_class_load_kind = HLoadClass::LoadKind::kRuntimeCall;
  }
  return desired_class_load_kind;
}

HInvokeStaticOrDirect::DispatchInfo CodeGeneratorMIPS64::GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
  // On MIPS64 we support all dispatch types.
  return desired_dispatch_info;
}

void CodeGeneratorMIPS64::GenerateStaticOrDirectCall(
    HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path) {
  // All registers are assumed to be correctly set up per the calling convention.
  Location callee_method = temp;  // For all kinds except kRecursive, callee will be in temp.
  HInvokeStaticOrDirect::MethodLoadKind method_load_kind = invoke->GetMethodLoadKind();
  HInvokeStaticOrDirect::CodePtrLocation code_ptr_location = invoke->GetCodePtrLocation();

  switch (method_load_kind) {
    case HInvokeStaticOrDirect::MethodLoadKind::kStringInit: {
      // temp = thread->string_init_entrypoint
      uint32_t offset =
          GetThreadOffset<kMips64PointerSize>(invoke->GetStringInitEntryPoint()).Int32Value();
      __ LoadFromOffset(kLoadDoubleword,
                        temp.AsRegister<GpuRegister>(),
                        TR,
                        offset);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRecursive:
      callee_method = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          NewBootImageMethodPatch(invoke->GetTargetMethod());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          NewBootImageMethodPatch(invoke->GetTargetMethod(), info_high);
      EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Daddiu(temp.AsRegister<GpuRegister>(), AT, /* placeholder */ 0x5678);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kDirectAddress:
      __ LoadLiteral(temp.AsRegister<GpuRegister>(),
                     kLoadDoubleword,
                     DeduplicateUint64Literal(invoke->GetMethodAddress()));
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBssEntry: {
      PcRelativePatchInfo* info_high = NewMethodBssEntryPatch(
          MethodReference(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex()));
      PcRelativePatchInfo* info_low = NewMethodBssEntryPatch(
          MethodReference(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex()), info_high);
      EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Ld(temp.AsRegister<GpuRegister>(), AT, /* placeholder */ 0x5678);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRuntimeCall: {
      GenerateInvokeStaticOrDirectRuntimeCall(invoke, temp, slow_path);
      return;  // No code pointer retrieval; the runtime performs the call directly.
    }
  }

  switch (code_ptr_location) {
    case HInvokeStaticOrDirect::CodePtrLocation::kCallSelf:
      __ Balc(&frame_entry_label_);
      break;
    case HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod:
      // T9 = callee_method->entry_point_from_quick_compiled_code_;
      __ LoadFromOffset(kLoadDoubleword,
                        T9,
                        callee_method.AsRegister<GpuRegister>(),
                        ArtMethod::EntryPointFromQuickCompiledCodeOffset(
                            kMips64PointerSize).Int32Value());
      // T9()
      __ Jalr(T9);
      __ Nop();
      break;
  }
  RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);

  DCHECK(!IsLeafMethod());
}

void InstructionCodeGeneratorMIPS64::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    return;
  }

  LocationSummary* locations = invoke->GetLocations();
  codegen_->GenerateStaticOrDirectCall(invoke,
                                       locations->HasTemps()
                                           ? locations->GetTemp(0)
                                           : Location::NoLocation());
}

void CodeGeneratorMIPS64::GenerateVirtualCall(
    HInvokeVirtual* invoke, Location temp_location, SlowPathCode* slow_path) {
  // Use the calling convention instead of the location of the receiver, as
  // intrinsics may have put the receiver in a different register. In the intrinsics
  // slow path, the arguments have been moved to the right place, so here we are
  // guaranteed that the receiver is the first register of the calling convention.
  InvokeDexCallingConvention calling_convention;
  GpuRegister receiver = calling_convention.GetRegisterAt(0);

  GpuRegister temp = temp_location.AsRegister<GpuRegister>();
  size_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), kMips64PointerSize).SizeValue();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMips64PointerSize);

  // temp = object->GetClass();
  __ LoadFromOffset(kLoadUnsignedWord, temp, receiver, class_offset);
  MaybeRecordImplicitNullCheck(invoke);
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  __ MaybeUnpoisonHeapReference(temp);
  // temp = temp->GetMethodAt(method_offset);
  __ LoadFromOffset(kLoadDoubleword, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadDoubleword, T9, temp, entry_point.Int32Value());
  // T9();
  __ Jalr(T9);
  __ Nop();
  RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
}

void InstructionCodeGeneratorMIPS64::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    return;
  }

  codegen_->GenerateVirtualCall(invoke, invoke->GetLocations()->GetTemp(0));
  DCHECK(!codegen_->IsLeafMethod());
}

void LocationsBuilderMIPS64::VisitLoadClass(HLoadClass* cls) {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConvention calling_convention;
    Location loc = Location::RegisterLocation(calling_convention.GetRegisterAt(0));
    CodeGenerator::CreateLoadClassRuntimeCallLocationSummary(cls, loc, loc);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());

  const bool requires_read_barrier = kEmitCompilerReadBarrier && !cls->IsInBootImage();
  LocationSummary::CallKind call_kind = (cls->NeedsEnvironment() || requires_read_barrier)
      ? LocationSummary::kCallOnSlowPath
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(cls, call_kind);
  if (kUseBakerReadBarrier && requires_read_barrier && !cls->NeedsEnvironment()) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  if (load_kind == HLoadClass::LoadKind::kReferrersClass) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
  locations->SetOut(Location::RequiresRegister());
  if (load_kind == HLoadClass::LoadKind::kBssEntry) {
    if (!kUseReadBarrier || kUseBakerReadBarrier) {
      // Rely on the type resolution or initialization and marking to save everything we need.
      RegisterSet caller_saves = RegisterSet::Empty();
      InvokeRuntimeCallingConvention calling_convention;
      caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
      locations->SetCustomSlowPathCallerSaves(caller_saves);
    } else {
      // For non-Baker read barriers we have a temp-clobbering call.
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorMIPS64::VisitLoadClass(HLoadClass* cls) NO_THREAD_SAFETY_ANALYSIS {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    codegen_->GenerateLoadClassRuntimeCall(cls);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());

  LocationSummary* locations = cls->GetLocations();
  Location out_loc = locations->Out();
  GpuRegister out = out_loc.AsRegister<GpuRegister>();
  GpuRegister current_method_reg = ZERO;
  if (load_kind == HLoadClass::LoadKind::kReferrersClass ||
      load_kind == HLoadClass::LoadKind::kRuntimeCall) {
      current_method_reg = locations->InAt(0).AsRegister<GpuRegister>();
  }

  const ReadBarrierOption read_barrier_option = cls->IsInBootImage()
      ? kWithoutReadBarrier
      : kCompilerReadBarrierOption;
  bool generate_null_check = false;
  switch (load_kind) {
    case HLoadClass::LoadKind::kReferrersClass:
      DCHECK(!cls->CanCallRuntime());
      DCHECK(!cls->MustGenerateClinitCheck());
      // /* GcRoot<mirror::Class> */ out = current_method->declaring_class_
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              current_method_reg,
                              ArtMethod::DeclaringClassOffset().Int32Value(),
                              read_barrier_option);
      break;
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Daddiu(out, AT, /* placeholder */ 0x5678);
      break;
    }
    case HLoadClass::LoadKind::kBootImageAddress: {
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(cls->GetClass().Get()));
      DCHECK_NE(address, 0u);
      __ LoadLiteral(out,
                     kLoadUnsignedWord,
                     codegen_->DeduplicateBootImageAddressLiteral(address));
      break;
    }
    case HLoadClass::LoadKind::kBootImageClassTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Lwu(out, AT, /* placeholder */ 0x5678);
      // Extract the reference from the slot data, i.e. clear the hash bits.
      int32_t masked_hash = ClassTable::TableSlot::MaskHash(
          ComputeModifiedUtf8Hash(cls->GetDexFile().StringByTypeIdx(cls->GetTypeIndex())));
      if (masked_hash != 0) {
        __ Daddiu(out, out, -masked_hash);
      }
      break;
    }
    case HLoadClass::LoadKind::kBssEntry: {
      CodeGeneratorMIPS64::PcRelativePatchInfo* bss_info_high =
          codegen_->NewTypeBssEntryPatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewTypeBssEntryPatch(cls->GetDexFile(), cls->GetTypeIndex(), bss_info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(bss_info_high, out);
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              read_barrier_option,
                              &info_low->label);
      generate_null_check = true;
      break;
    }
    case HLoadClass::LoadKind::kJitTableAddress:
      __ LoadLiteral(out,
                     kLoadUnsignedWord,
                     codegen_->DeduplicateJitClassLiteral(cls->GetDexFile(),
                                                          cls->GetTypeIndex(),
                                                          cls->GetClass()));
      GenerateGcRootFieldLoad(cls, out_loc, out, 0, read_barrier_option);
      break;
    case HLoadClass::LoadKind::kRuntimeCall:
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }

  if (generate_null_check || cls->MustGenerateClinitCheck()) {
    DCHECK(cls->CanCallRuntime());
    SlowPathCodeMIPS64* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathMIPS64(
        cls, cls, cls->GetDexPc(), cls->MustGenerateClinitCheck());
    codegen_->AddSlowPath(slow_path);
    if (generate_null_check) {
      __ Beqzc(out, slow_path->GetEntryLabel());
    }
    if (cls->MustGenerateClinitCheck()) {
      GenerateClassInitializationCheck(slow_path, out);
    } else {
      __ Bind(slow_path->GetExitLabel());
    }
  }
}

static int32_t GetExceptionTlsOffset() {
  return Thread::ExceptionOffset<kMips64PointerSize>().Int32Value();
}

void LocationsBuilderMIPS64::VisitLoadException(HLoadException* load) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(load, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS64::VisitLoadException(HLoadException* load) {
  GpuRegister out = load->GetLocations()->Out().AsRegister<GpuRegister>();
  __ LoadFromOffset(kLoadUnsignedWord, out, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS64::VisitClearException(HClearException* clear) {
  new (GetGraph()->GetAllocator()) LocationSummary(clear, LocationSummary::kNoCall);
}

void InstructionCodeGeneratorMIPS64::VisitClearException(HClearException* clear ATTRIBUTE_UNUSED) {
  __ StoreToOffset(kStoreWord, ZERO, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS64::VisitLoadString(HLoadString* load) {
  HLoadString::LoadKind load_kind = load->GetLoadKind();
  LocationSummary::CallKind call_kind = CodeGenerator::GetLoadStringCallKind(load);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(load, call_kind);
  if (load_kind == HLoadString::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConvention calling_convention;
    locations->SetOut(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  } else {
    locations->SetOut(Location::RequiresRegister());
    if (load_kind == HLoadString::LoadKind::kBssEntry) {
      if (!kUseReadBarrier || kUseBakerReadBarrier) {
        // Rely on the pResolveString and marking to save everything we need.
        RegisterSet caller_saves = RegisterSet::Empty();
        InvokeRuntimeCallingConvention calling_convention;
        caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
        locations->SetCustomSlowPathCallerSaves(caller_saves);
      } else {
        // For non-Baker read barriers we have a temp-clobbering call.
      }
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorMIPS64::VisitLoadString(HLoadString* load) NO_THREAD_SAFETY_ANALYSIS {
  HLoadString::LoadKind load_kind = load->GetLoadKind();
  LocationSummary* locations = load->GetLocations();
  Location out_loc = locations->Out();
  GpuRegister out = out_loc.AsRegister<GpuRegister>();

  switch (load_kind) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Daddiu(out, AT, /* placeholder */ 0x5678);
      return;
    }
    case HLoadString::LoadKind::kBootImageAddress: {
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(load->GetString().Get()));
      DCHECK_NE(address, 0u);
      __ LoadLiteral(out,
                     kLoadUnsignedWord,
                     codegen_->DeduplicateBootImageAddressLiteral(address));
      return;
    }
    case HLoadString::LoadKind::kBootImageInternTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high, AT, info_low);
      __ Lwu(out, AT, /* placeholder */ 0x5678);
      return;
    }
    case HLoadString::LoadKind::kBssEntry: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_high =
          codegen_->NewStringBssEntryPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS64::PcRelativePatchInfo* info_low =
          codegen_->NewStringBssEntryPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high, out);
      GenerateGcRootFieldLoad(load,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              kCompilerReadBarrierOption,
                              &info_low->label);
      SlowPathCodeMIPS64* slow_path =
          new (codegen_->GetScopedAllocator()) LoadStringSlowPathMIPS64(load);
      codegen_->AddSlowPath(slow_path);
      __ Beqzc(out, slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
    case HLoadString::LoadKind::kJitTableAddress:
      __ LoadLiteral(out,
                     kLoadUnsignedWord,
                     codegen_->DeduplicateJitStringLiteral(load->GetDexFile(),
                                                           load->GetStringIndex(),
                                                           load->GetString()));
      GenerateGcRootFieldLoad(load, out_loc, out, 0, kCompilerReadBarrierOption);
      return;
    default:
      break;
  }

  // TODO: Re-add the compiler code to do string dex cache lookup again.
  DCHECK(load_kind == HLoadString::LoadKind::kRuntimeCall);
  InvokeRuntimeCallingConvention calling_convention;
  DCHECK_EQ(calling_convention.GetRegisterAt(0), out);
  __ LoadConst32(calling_convention.GetRegisterAt(0), load->GetStringIndex().index_);
  codegen_->InvokeRuntime(kQuickResolveString, load, load->GetDexPc());
  CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();
}

void LocationsBuilderMIPS64::VisitLongConstant(HLongConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS64::VisitLongConstant(HLongConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS64::VisitMonitorOperation(HMonitorOperation* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS64::VisitMonitorOperation(HMonitorOperation* instruction) {
  codegen_->InvokeRuntime(instruction->IsEnter() ? kQuickLockObject : kQuickUnlockObject,
                          instruction,
                          instruction->GetDexPc());
  if (instruction->IsEnter()) {
    CheckEntrypointTypes<kQuickLockObject, void, mirror::Object*>();
  } else {
    CheckEntrypointTypes<kQuickUnlockObject, void, mirror::Object*>();
  }
}

void LocationsBuilderMIPS64::VisitMul(HMul* mul) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(mul, LocationSummary::kNoCall);
  switch (mul->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected mul type " << mul->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS64::VisitMul(HMul* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
      GpuRegister lhs = locations->InAt(0).AsRegister<GpuRegister>();
      GpuRegister rhs = locations->InAt(1).AsRegister<GpuRegister>();
      if (type == DataType::Type::kInt32)
        __ MulR6(dst, lhs, rhs);
      else
        __ Dmul(dst, lhs, rhs);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
      FpuRegister lhs = locations->InAt(0).AsFpuRegister<FpuRegister>();
      FpuRegister rhs = locations->InAt(1).AsFpuRegister<FpuRegister>();
      if (type == DataType::Type::kFloat32)
        __ MulS(dst, lhs, rhs);
      else
        __ MulD(dst, lhs, rhs);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected mul type " << type;
  }
}

void LocationsBuilderMIPS64::VisitNeg(HNeg* neg) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(neg, LocationSummary::kNoCall);
  switch (neg->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << neg->GetResultType();
  }
}

void InstructionCodeGeneratorMIPS64::VisitNeg(HNeg* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
      GpuRegister src = locations->InAt(0).AsRegister<GpuRegister>();
      if (type == DataType::Type::kInt32)
        __ Subu(dst, ZERO, src);
      else
        __ Dsubu(dst, ZERO, src);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
      FpuRegister src = locations->InAt(0).AsFpuRegister<FpuRegister>();
      if (type == DataType::Type::kFloat32)
        __ NegS(dst, src);
      else
        __ NegD(dst, src);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected neg type " << type;
  }
}

void LocationsBuilderMIPS64::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
}

void InstructionCodeGeneratorMIPS64::VisitNewArray(HNewArray* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes care
  // of poisoning the reference.
  QuickEntrypointEnum entrypoint =
      CodeGenerator::GetArrayAllocationEntrypoint(instruction->GetLoadClass()->GetClass());
  codegen_->InvokeRuntime(entrypoint, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickAllocArrayResolved, void*, mirror::Class*, int32_t>();
  DCHECK(!codegen_->IsLeafMethod());
}

void LocationsBuilderMIPS64::VisitNewInstance(HNewInstance* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  if (instruction->IsStringAlloc()) {
    locations->AddTemp(Location::RegisterLocation(kMethodRegisterArgument));
  } else {
    locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  }
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
}

void InstructionCodeGeneratorMIPS64::VisitNewInstance(HNewInstance* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes care
  // of poisoning the reference.
  if (instruction->IsStringAlloc()) {
    // String is allocated through StringFactory. Call NewEmptyString entry point.
    GpuRegister temp = instruction->GetLocations()->GetTemp(0).AsRegister<GpuRegister>();
    MemberOffset code_offset =
        ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMips64PointerSize);
    __ LoadFromOffset(kLoadDoubleword, temp, TR, QUICK_ENTRY_POINT(pNewEmptyString));
    __ LoadFromOffset(kLoadDoubleword, T9, temp, code_offset.Int32Value());
    __ Jalr(T9);
    __ Nop();
    codegen_->RecordPcInfo(instruction, instruction->GetDexPc());
  } else {
    codegen_->InvokeRuntime(instruction->GetEntrypoint(), instruction, instruction->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
  }
}

void LocationsBuilderMIPS64::VisitNot(HNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS64::VisitNot(HNot* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
      GpuRegister src = locations->InAt(0).AsRegister<GpuRegister>();
      __ Nor(dst, src, ZERO);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected type for not operation " << instruction->GetResultType();
  }
}

void LocationsBuilderMIPS64::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS64::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  __ Xori(locations->Out().AsRegister<GpuRegister>(),
          locations->InAt(0).AsRegister<GpuRegister>(),
          1);
}

void LocationsBuilderMIPS64::VisitNullCheck(HNullCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
}

void CodeGeneratorMIPS64::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (CanMoveNullCheckToUser(instruction)) {
    return;
  }
  Location obj = instruction->GetLocations()->InAt(0);

  __ Lw(ZERO, obj.AsRegister<GpuRegister>(), 0);
  RecordPcInfo(instruction, instruction->GetDexPc());
}

void CodeGeneratorMIPS64::GenerateExplicitNullCheck(HNullCheck* instruction) {
  SlowPathCodeMIPS64* slow_path =
      new (GetScopedAllocator()) NullCheckSlowPathMIPS64(instruction);
  AddSlowPath(slow_path);

  Location obj = instruction->GetLocations()->InAt(0);

  __ Beqzc(obj.AsRegister<GpuRegister>(), slow_path->GetEntryLabel());
}

void InstructionCodeGeneratorMIPS64::VisitNullCheck(HNullCheck* instruction) {
  codegen_->GenerateNullCheck(instruction);
}

void LocationsBuilderMIPS64::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS64::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS64::VisitParallelMove(HParallelMove* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS64::VisitParallelMove(HParallelMove* instruction) {
  if (instruction->GetNext()->IsSuspendCheck() &&
      instruction->GetBlock()->GetLoopInformation() != nullptr) {
    HSuspendCheck* suspend_check = instruction->GetNext()->AsSuspendCheck();
    // The back edge will generate the suspend check.
    codegen_->ClearSpillSlotsFromLoopPhisInStackMap(suspend_check, instruction);
  }

  codegen_->GetMoveResolver()->EmitNativeCode(instruction);
}

void LocationsBuilderMIPS64::VisitParameterValue(HParameterValue* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  Location location = parameter_visitor_.GetNextLocation(instruction->GetType());
  if (location.IsStackSlot()) {
    location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  } else if (location.IsDoubleStackSlot()) {
    location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  }
  locations->SetOut(location);
}

void InstructionCodeGeneratorMIPS64::VisitParameterValue(HParameterValue* instruction
                                                         ATTRIBUTE_UNUSED) {
  // Nothing to do, the parameter is already at its location.
}

void LocationsBuilderMIPS64::VisitCurrentMethod(HCurrentMethod* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(Location::RegisterLocation(kMethodRegisterArgument));
}

void InstructionCodeGeneratorMIPS64::VisitCurrentMethod(HCurrentMethod* instruction
                                                        ATTRIBUTE_UNUSED) {
  // Nothing to do, the method is already at its location.
}

void LocationsBuilderMIPS64::VisitPhi(HPhi* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    locations->SetInAt(i, Location::Any());
  }
  locations->SetOut(Location::Any());
}

void InstructionCodeGeneratorMIPS64::VisitPhi(HPhi* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS64::VisitRem(HRem* rem) {
  DataType::Type type = rem->GetResultType();
  LocationSummary::CallKind call_kind =
      DataType::IsFloatingPointType(type) ? LocationSummary::kCallOnMainOnly
                                          : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(rem, call_kind);

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(rem->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      InvokeRuntimeCallingConvention calling_convention;
      locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
      locations->SetInAt(1, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(1)));
      locations->SetOut(calling_convention.GetReturnLocation(type));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void InstructionCodeGeneratorMIPS64::VisitRem(HRem* instruction) {
  DataType::Type type = instruction->GetType();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      GenerateDivRemIntegral(instruction);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      QuickEntrypointEnum entrypoint =
          (type == DataType::Type::kFloat32) ? kQuickFmodf : kQuickFmod;
      codegen_->InvokeRuntime(entrypoint, instruction, instruction->GetDexPc());
      if (type == DataType::Type::kFloat32) {
        CheckEntrypointTypes<kQuickFmodf, float, float, float>();
      } else {
        CheckEntrypointTypes<kQuickFmod, double, double, double>();
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void LocationsBuilderMIPS64::VisitConstructorFence(HConstructorFence* constructor_fence) {
  constructor_fence->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitConstructorFence(
    HConstructorFence* constructor_fence ATTRIBUTE_UNUSED) {
  GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
}

void LocationsBuilderMIPS64::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  memory_barrier->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  GenerateMemoryBarrier(memory_barrier->GetBarrierKind());
}

void LocationsBuilderMIPS64::VisitReturn(HReturn* ret) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(ret);
  DataType::Type return_type = ret->InputAt(0)->GetType();
  locations->SetInAt(0, Mips64ReturnLocation(return_type));
}

void InstructionCodeGeneratorMIPS64::VisitReturn(HReturn* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS64::VisitReturnVoid(HReturnVoid* ret) {
  ret->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS64::VisitReturnVoid(HReturnVoid* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS64::VisitRor(HRor* ror) {
  HandleShift(ror);
}

void InstructionCodeGeneratorMIPS64::VisitRor(HRor* ror) {
  HandleShift(ror);
}

void LocationsBuilderMIPS64::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void InstructionCodeGeneratorMIPS64::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void LocationsBuilderMIPS64::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void InstructionCodeGeneratorMIPS64::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void LocationsBuilderMIPS64::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS64::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderMIPS64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LocationsBuilderMIPS64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorMIPS64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorMIPS64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorMIPS64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorMIPS64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionMIPS64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS64::VisitSuspendCheck(HSuspendCheck* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnSlowPath);
  // In suspend check slow path, usually there are no caller-save registers at all.
  // If SIMD instructions are present, however, we force spilling all live SIMD
  // registers in full width (since the runtime only saves/restores lower part).
  locations->SetCustomSlowPathCallerSaves(
      GetGraph()->HasSIMD() ? RegisterSet::AllFpu() : RegisterSet::Empty());
}

void InstructionCodeGeneratorMIPS64::VisitSuspendCheck(HSuspendCheck* instruction) {
  HBasicBlock* block = instruction->GetBlock();
  if (block->GetLoopInformation() != nullptr) {
    DCHECK(block->GetLoopInformation()->GetSuspendCheck() == instruction);
    // The back edge will generate the suspend check.
    return;
  }
  if (block->IsEntryBlock() && instruction->GetNext()->IsGoto()) {
    // The goto will generate the suspend check.
    return;
  }
  GenerateSuspendCheck(instruction, nullptr);
}

void LocationsBuilderMIPS64::VisitThrow(HThrow* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS64::VisitThrow(HThrow* instruction) {
  codegen_->InvokeRuntime(kQuickDeliverException, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
}

void LocationsBuilderMIPS64::VisitTypeConversion(HTypeConversion* conversion) {
  DataType::Type input_type = conversion->GetInputType();
  DataType::Type result_type = conversion->GetResultType();
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;

  if ((input_type == DataType::Type::kReference) || (input_type == DataType::Type::kVoid) ||
      (result_type == DataType::Type::kReference) || (result_type == DataType::Type::kVoid)) {
    LOG(FATAL) << "Unexpected type conversion from " << input_type << " to " << result_type;
  }

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(conversion);

  if (DataType::IsFloatingPointType(input_type)) {
    locations->SetInAt(0, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(0, Location::RequiresRegister());
  }

  if (DataType::IsFloatingPointType(result_type)) {
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorMIPS64::VisitTypeConversion(HTypeConversion* conversion) {
  LocationSummary* locations = conversion->GetLocations();
  DataType::Type result_type = conversion->GetResultType();
  DataType::Type input_type = conversion->GetInputType();

  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;

  if (DataType::IsIntegralType(result_type) && DataType::IsIntegralType(input_type)) {
    GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
    GpuRegister src = locations->InAt(0).AsRegister<GpuRegister>();

    switch (result_type) {
      case DataType::Type::kUint8:
        __ Andi(dst, src, 0xFF);
        break;
      case DataType::Type::kInt8:
        if (input_type == DataType::Type::kInt64) {
          // Type conversion from long to types narrower than int is a result of code
          // transformations. To avoid unpredictable results for SEB and SEH, we first
          // need to sign-extend the low 32-bit value into bits 32 through 63.
          __ Sll(dst, src, 0);
          __ Seb(dst, dst);
        } else {
          __ Seb(dst, src);
        }
        break;
      case DataType::Type::kUint16:
        __ Andi(dst, src, 0xFFFF);
        break;
      case DataType::Type::kInt16:
        if (input_type == DataType::Type::kInt64) {
          // Type conversion from long to types narrower than int is a result of code
          // transformations. To avoid unpredictable results for SEB and SEH, we first
          // need to sign-extend the low 32-bit value into bits 32 through 63.
          __ Sll(dst, src, 0);
          __ Seh(dst, dst);
        } else {
          __ Seh(dst, src);
        }
        break;
      case DataType::Type::kInt32:
      case DataType::Type::kInt64:
        // Sign-extend 32-bit int into bits 32 through 63 for int-to-long and long-to-int
        // conversions, except when the input and output registers are the same and we are not
        // converting longs to shorter types. In these cases, do nothing.
        if ((input_type == DataType::Type::kInt64) || (dst != src)) {
          __ Sll(dst, src, 0);
        }
        break;

      default:
        LOG(FATAL) << "Unexpected type conversion from " << input_type
                   << " to " << result_type;
    }
  } else if (DataType::IsFloatingPointType(result_type) && DataType::IsIntegralType(input_type)) {
    FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
    GpuRegister src = locations->InAt(0).AsRegister<GpuRegister>();
    if (input_type == DataType::Type::kInt64) {
      __ Dmtc1(src, FTMP);
      if (result_type == DataType::Type::kFloat32) {
        __ Cvtsl(dst, FTMP);
      } else {
        __ Cvtdl(dst, FTMP);
      }
    } else {
      __ Mtc1(src, FTMP);
      if (result_type == DataType::Type::kFloat32) {
        __ Cvtsw(dst, FTMP);
      } else {
        __ Cvtdw(dst, FTMP);
      }
    }
  } else if (DataType::IsIntegralType(result_type) && DataType::IsFloatingPointType(input_type)) {
    CHECK(result_type == DataType::Type::kInt32 || result_type == DataType::Type::kInt64);
    GpuRegister dst = locations->Out().AsRegister<GpuRegister>();
    FpuRegister src = locations->InAt(0).AsFpuRegister<FpuRegister>();

    if (result_type == DataType::Type::kInt64) {
      if (input_type == DataType::Type::kFloat32) {
        __ TruncLS(FTMP, src);
      } else {
        __ TruncLD(FTMP, src);
      }
      __ Dmfc1(dst, FTMP);
    } else {
      if (input_type == DataType::Type::kFloat32) {
        __ TruncWS(FTMP, src);
      } else {
        __ TruncWD(FTMP, src);
      }
      __ Mfc1(dst, FTMP);
    }
  } else if (DataType::IsFloatingPointType(result_type) &&
             DataType::IsFloatingPointType(input_type)) {
    FpuRegister dst = locations->Out().AsFpuRegister<FpuRegister>();
    FpuRegister src = locations->InAt(0).AsFpuRegister<FpuRegister>();
    if (result_type == DataType::Type::kFloat32) {
      __ Cvtsd(dst, src);
    } else {
      __ Cvtds(dst, src);
    }
  } else {
    LOG(FATAL) << "Unexpected or unimplemented type conversion from " << input_type
                << " to " << result_type;
  }
}

void LocationsBuilderMIPS64::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void InstructionCodeGeneratorMIPS64::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void LocationsBuilderMIPS64::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS64::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS64::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS64::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS64::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS64::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS64::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

// Simple implementation of packed switch - generate cascaded compare/jumps.
void LocationsBuilderMIPS64::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(switch_instr, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS64::GenPackedSwitchWithCompares(GpuRegister value_reg,
                                                                 int32_t lower_bound,
                                                                 uint32_t num_entries,
                                                                 HBasicBlock* switch_block,
                                                                 HBasicBlock* default_block) {
  // Create a set of compare/jumps.
  GpuRegister temp_reg = TMP;
  __ Addiu32(temp_reg, value_reg, -lower_bound);
  // Jump to default if index is negative
  // Note: We don't check the case that index is positive while value < lower_bound, because in
  // this case, index >= num_entries must be true. So that we can save one branch instruction.
  __ Bltzc(temp_reg, codegen_->GetLabelOf(default_block));

  const ArenaVector<HBasicBlock*>& successors = switch_block->GetSuccessors();
  // Jump to successors[0] if value == lower_bound.
  __ Beqzc(temp_reg, codegen_->GetLabelOf(successors[0]));
  int32_t last_index = 0;
  for (; num_entries - last_index > 2; last_index += 2) {
    __ Addiu(temp_reg, temp_reg, -2);
    // Jump to successors[last_index + 1] if value < case_value[last_index + 2].
    __ Bltzc(temp_reg, codegen_->GetLabelOf(successors[last_index + 1]));
    // Jump to successors[last_index + 2] if value == case_value[last_index + 2].
    __ Beqzc(temp_reg, codegen_->GetLabelOf(successors[last_index + 2]));
  }
  if (num_entries - last_index == 2) {
    // The last missing case_value.
    __ Addiu(temp_reg, temp_reg, -1);
    __ Beqzc(temp_reg, codegen_->GetLabelOf(successors[last_index + 1]));
  }

  // And the default for any other value.
  if (!codegen_->GoesToNextBlock(switch_block, default_block)) {
    __ Bc(codegen_->GetLabelOf(default_block));
  }
}

void InstructionCodeGeneratorMIPS64::GenTableBasedPackedSwitch(GpuRegister value_reg,
                                                               int32_t lower_bound,
                                                               uint32_t num_entries,
                                                               HBasicBlock* switch_block,
                                                               HBasicBlock* default_block) {
  // Create a jump table.
  std::vector<Mips64Label*> labels(num_entries);
  const ArenaVector<HBasicBlock*>& successors = switch_block->GetSuccessors();
  for (uint32_t i = 0; i < num_entries; i++) {
    labels[i] = codegen_->GetLabelOf(successors[i]);
  }
  JumpTable* table = __ CreateJumpTable(std::move(labels));

  // Is the value in range?
  __ Addiu32(TMP, value_reg, -lower_bound);
  __ LoadConst32(AT, num_entries);
  __ Bgeuc(TMP, AT, codegen_->GetLabelOf(default_block));

  // We are in the range of the table.
  // Load the target address from the jump table, indexing by the value.
  __ LoadLabelAddress(AT, table->GetLabel());
  __ Dlsa(TMP, TMP, AT, 2);
  __ Lw(TMP, TMP, 0);
  // Compute the absolute target address by adding the table start address
  // (the table contains offsets to targets relative to its start).
  __ Daddu(TMP, TMP, AT);
  // And jump.
  __ Jr(TMP);
  __ Nop();
}

void InstructionCodeGeneratorMIPS64::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  int32_t lower_bound = switch_instr->GetStartValue();
  uint32_t num_entries = switch_instr->GetNumEntries();
  LocationSummary* locations = switch_instr->GetLocations();
  GpuRegister value_reg = locations->InAt(0).AsRegister<GpuRegister>();
  HBasicBlock* switch_block = switch_instr->GetBlock();
  HBasicBlock* default_block = switch_instr->GetDefaultBlock();

  if (num_entries > kPackedSwitchJumpTableThreshold) {
    GenTableBasedPackedSwitch(value_reg,
                              lower_bound,
                              num_entries,
                              switch_block,
                              default_block);
  } else {
    GenPackedSwitchWithCompares(value_reg,
                                lower_bound,
                                num_entries,
                                switch_block,
                                default_block);
  }
}

void LocationsBuilderMIPS64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (instruction->GetTableKind() == HClassTableGet::TableKind::kVTable) {
    uint32_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
        instruction->GetIndex(), kMips64PointerSize).SizeValue();
    __ LoadFromOffset(kLoadDoubleword,
                      locations->Out().AsRegister<GpuRegister>(),
                      locations->InAt(0).AsRegister<GpuRegister>(),
                      method_offset);
  } else {
    uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
        instruction->GetIndex(), kMips64PointerSize));
    __ LoadFromOffset(kLoadDoubleword,
                      locations->Out().AsRegister<GpuRegister>(),
                      locations->InAt(0).AsRegister<GpuRegister>(),
                      mirror::Class::ImtPtrOffset(kMips64PointerSize).Uint32Value());
    __ LoadFromOffset(kLoadDoubleword,
                      locations->Out().AsRegister<GpuRegister>(),
                      locations->Out().AsRegister<GpuRegister>(),
                      method_offset);
  }
}

void LocationsBuilderMIPS64::VisitIntermediateAddress(HIntermediateAddress* instruction
                                                      ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS64::VisitIntermediateAddress(HIntermediateAddress* instruction
                                                              ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

}  // namespace mips64
}  // namespace art
