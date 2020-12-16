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

#include "code_generator_mips.h"

#include "arch/mips/asm_support_mips.h"
#include "arch/mips/entrypoints_direct_mips.h"
#include "arch/mips/instruction_set_features_mips.h"
#include "art_method.h"
#include "class_table.h"
#include "code_generator_utils.h"
#include "compiled_method.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gc/accounting/card_table.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "intrinsics_mips.h"
#include "linker/linker_patch.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "offsets.h"
#include "stack_map_stream.h"
#include "thread.h"
#include "utils/assembler.h"
#include "utils/mips/assembler_mips.h"
#include "utils/stack_checks.h"

namespace art {
namespace mips {

static constexpr int kCurrentMethodStackOffset = 0;
static constexpr Register kMethodRegisterArgument = A0;

// Flags controlling the use of thunks for Baker read barriers.
constexpr bool kBakerReadBarrierThunksEnableForFields = true;
constexpr bool kBakerReadBarrierThunksEnableForArrays = true;
constexpr bool kBakerReadBarrierThunksEnableForGcRoots = true;

Location MipsReturnLocation(DataType::Type return_type) {
  switch (return_type) {
    case DataType::Type::kReference:
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kUint32:
    case DataType::Type::kInt32:
      return Location::RegisterLocation(V0);

    case DataType::Type::kUint64:
    case DataType::Type::kInt64:
      return Location::RegisterPairLocation(V0, V1);

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      return Location::FpuRegisterLocation(F0);

    case DataType::Type::kVoid:
      return Location();
  }
  UNREACHABLE();
}

Location InvokeDexCallingConventionVisitorMIPS::GetReturnLocation(DataType::Type type) const {
  return MipsReturnLocation(type);
}

Location InvokeDexCallingConventionVisitorMIPS::GetMethodLocation() const {
  return Location::RegisterLocation(kMethodRegisterArgument);
}

Location InvokeDexCallingConventionVisitorMIPS::GetNextLocation(DataType::Type type) {
  Location next_location;

  switch (type) {
    case DataType::Type::kReference:
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      uint32_t gp_index = gp_index_++;
      if (gp_index < calling_convention.GetNumberOfRegisters()) {
        next_location = Location::RegisterLocation(calling_convention.GetRegisterAt(gp_index));
      } else {
        size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
        next_location = Location::StackSlot(stack_offset);
      }
      break;
    }

    case DataType::Type::kInt64: {
      uint32_t gp_index = gp_index_;
      gp_index_ += 2;
      if (gp_index + 1 < calling_convention.GetNumberOfRegisters()) {
        Register reg = calling_convention.GetRegisterAt(gp_index);
        if (reg == A1 || reg == A3) {
          gp_index_++;  // Skip A1(A3), and use A2_A3(T0_T1) instead.
          gp_index++;
        }
        Register low_even = calling_convention.GetRegisterAt(gp_index);
        Register high_odd = calling_convention.GetRegisterAt(gp_index + 1);
        DCHECK_EQ(low_even + 1, high_odd);
        next_location = Location::RegisterPairLocation(low_even, high_odd);
      } else {
        size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
        next_location = Location::DoubleStackSlot(stack_offset);
      }
      break;
    }

    // Note: both float and double types are stored in even FPU registers. On 32 bit FPU, double
    // will take up the even/odd pair, while floats are stored in even regs only.
    // On 64 bit FPU, both double and float are stored in even registers only.
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      uint32_t float_index = float_index_++;
      if (float_index < calling_convention.GetNumberOfFpuRegisters()) {
        next_location = Location::FpuRegisterLocation(
            calling_convention.GetFpuRegisterAt(float_index));
      } else {
        size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
        next_location = DataType::Is64BitType(type) ? Location::DoubleStackSlot(stack_offset)
                                                    : Location::StackSlot(stack_offset);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unexpected parameter type " << type;
      break;
  }

  // Space on the stack is reserved for all arguments.
  stack_index_ += DataType::Is64BitType(type) ? 2 : 1;

  return next_location;
}

Location InvokeRuntimeCallingConvention::GetReturnLocation(DataType::Type type) {
  return MipsReturnLocation(type);
}

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<CodeGeneratorMIPS*>(codegen)->GetAssembler()->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMipsPointerSize, x).Int32Value()

class BoundsCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit BoundsCheckSlowPathMIPS(HBoundsCheck* instruction) : SlowPathCodeMIPS(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
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
    mips_codegen->InvokeRuntime(entrypoint, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowStringBounds, void, int32_t, int32_t>();
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "BoundsCheckSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathMIPS);
};

class DivZeroCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit DivZeroCheckSlowPathMIPS(HDivZeroCheck* instruction) : SlowPathCodeMIPS(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel());
    mips_codegen->InvokeRuntime(kQuickThrowDivZero, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowDivZero, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "DivZeroCheckSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DivZeroCheckSlowPathMIPS);
};

class LoadClassSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  LoadClassSlowPathMIPS(HLoadClass* cls,
                        HInstruction* at,
                        uint32_t dex_pc,
                        bool do_clinit)
      : SlowPathCodeMIPS(at),
        cls_(cls),
        dex_pc_(dex_pc),
        do_clinit_(do_clinit) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Location out = locations->Out();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    InvokeRuntimeCallingConvention calling_convention;
    DCHECK_EQ(instruction_->IsLoadClass(), cls_ == instruction_);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    dex::TypeIndex type_index = cls_->GetTypeIndex();
    __ LoadConst32(calling_convention.GetRegisterAt(0), type_index.index_);
    QuickEntrypointEnum entrypoint = do_clinit_ ? kQuickInitializeStaticStorage
                                                : kQuickInitializeType;
    mips_codegen->InvokeRuntime(entrypoint, instruction_, dex_pc_, this);
    if (do_clinit_) {
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, uint32_t>();
    } else {
      CheckEntrypointTypes<kQuickInitializeType, void*, uint32_t>();
    }

    // Move the class to the desired location.
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      DataType::Type type = instruction_->GetType();
      mips_codegen->MoveLocation(out,
                                 Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                                 type);
    }
    RestoreLiveRegisters(codegen, locations);

    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadClassSlowPathMIPS"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  // The dex PC of `at_`.
  const uint32_t dex_pc_;

  // Whether to initialize the class.
  const bool do_clinit_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathMIPS);
};

class LoadStringSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit LoadStringSlowPathMIPS(HLoadString* instruction)
      : SlowPathCodeMIPS(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    DCHECK(instruction_->IsLoadString());
    DCHECK_EQ(instruction_->AsLoadString()->GetLoadKind(), HLoadString::LoadKind::kBssEntry);
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    const dex::StringIndex string_index = instruction_->AsLoadString()->GetStringIndex();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    InvokeRuntimeCallingConvention calling_convention;
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    __ LoadConst32(calling_convention.GetRegisterAt(0), string_index.index_);
    mips_codegen->InvokeRuntime(kQuickResolveString, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();

    DataType::Type type = instruction_->GetType();
    mips_codegen->MoveLocation(locations->Out(),
                               Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               type);
    RestoreLiveRegisters(codegen, locations);

    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadStringSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadStringSlowPathMIPS);
};

class NullCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit NullCheckSlowPathMIPS(HNullCheck* instr) : SlowPathCodeMIPS(instr) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    mips_codegen->InvokeRuntime(kQuickThrowNullPointer,
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "NullCheckSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathMIPS);
};

class SuspendCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  SuspendCheckSlowPathMIPS(HSuspendCheck* instruction, HBasicBlock* successor)
      : SlowPathCodeMIPS(instruction), successor_(successor) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);     // Only saves live vector registers for SIMD.
    mips_codegen->InvokeRuntime(kQuickTestSuspend, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickTestSuspend, void, void>();
    RestoreLiveRegisters(codegen, locations);  // Only restores live vector registers for SIMD.
    if (successor_ == nullptr) {
      __ B(GetReturnLabel());
    } else {
      __ B(mips_codegen->GetLabelOf(successor_));
    }
  }

  MipsLabel* GetReturnLabel() {
    DCHECK(successor_ == nullptr);
    return &return_label_;
  }

  const char* GetDescription() const OVERRIDE { return "SuspendCheckSlowPathMIPS"; }

  HBasicBlock* GetSuccessor() const {
    return successor_;
  }

 private:
  // If not null, the block to branch to after the suspend check.
  HBasicBlock* const successor_;

  // If `successor_` is null, the label to branch to after the suspend check.
  MipsLabel return_label_;

  DISALLOW_COPY_AND_ASSIGN(SuspendCheckSlowPathMIPS);
};

class TypeCheckSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit TypeCheckSlowPathMIPS(HInstruction* instruction, bool is_fatal)
      : SlowPathCodeMIPS(instruction), is_fatal_(is_fatal) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    uint32_t dex_pc = instruction_->GetDexPc();
    DCHECK(instruction_->IsCheckCast()
           || !locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);

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
      mips_codegen->InvokeRuntime(kQuickInstanceofNonTrivial, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickInstanceofNonTrivial, size_t, mirror::Object*, mirror::Class*>();
      DataType::Type ret_type = instruction_->GetType();
      Location ret_loc = calling_convention.GetReturnLocation(ret_type);
      mips_codegen->MoveLocation(locations->Out(), ret_loc, ret_type);
    } else {
      DCHECK(instruction_->IsCheckCast());
      mips_codegen->InvokeRuntime(kQuickCheckInstanceOf, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickCheckInstanceOf, void, mirror::Object*, mirror::Class*>();
    }

    if (!is_fatal_) {
      RestoreLiveRegisters(codegen, locations);
      __ B(GetExitLabel());
    }
  }

  const char* GetDescription() const OVERRIDE { return "TypeCheckSlowPathMIPS"; }

  bool IsFatal() const OVERRIDE { return is_fatal_; }

 private:
  const bool is_fatal_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckSlowPathMIPS);
};

class DeoptimizationSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit DeoptimizationSlowPathMIPS(HDeoptimize* instruction)
    : SlowPathCodeMIPS(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    __ Bind(GetEntryLabel());
    LocationSummary* locations = instruction_->GetLocations();
    SaveLiveRegisters(codegen, locations);
    InvokeRuntimeCallingConvention calling_convention;
    __ LoadConst32(calling_convention.GetRegisterAt(0),
                   static_cast<uint32_t>(instruction_->AsDeoptimize()->GetDeoptimizationKind()));
    mips_codegen->InvokeRuntime(kQuickDeoptimize, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickDeoptimize, void, DeoptimizationKind>();
  }

  const char* GetDescription() const OVERRIDE { return "DeoptimizationSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeoptimizationSlowPathMIPS);
};

class ArraySetSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit ArraySetSlowPathMIPS(HInstruction* instruction) : SlowPathCodeMIPS(instruction) {}

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

    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    mips_codegen->InvokeRuntime(kQuickAputObject, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickAputObject, void, mirror::Array*, int32_t, mirror::Object*>();
    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ArraySetSlowPathMIPS"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArraySetSlowPathMIPS);
};

// Slow path marking an object reference `ref` during a read
// barrier. The field `obj.field` in the object `obj` holding this
// reference does not get updated by this slow path after marking (see
// ReadBarrierMarkAndUpdateFieldSlowPathMIPS below for that).
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
class ReadBarrierMarkSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  ReadBarrierMarkSlowPathMIPS(HInstruction* instruction,
                              Location ref,
                              Location entrypoint = Location::NoLocation())
      : SlowPathCodeMIPS(instruction), ref_(ref), entrypoint_(entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathMIPS"; }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Register ref_reg = ref_.AsRegister<Register>();
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
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    DCHECK((V0 <= ref_reg && ref_reg <= T7) ||
           (S2 <= ref_reg && ref_reg <= S7) ||
           (ref_reg == FP)) << ref_reg;
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
      mips_codegen->ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction_, this);
      DCHECK_EQ(entrypoint_.AsRegister<Register>(), T9);
      __ Jalr(entrypoint_.AsRegister<Register>());
      __ NopIfNoReordering();
    } else {
      int32_t entry_point_offset =
          Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(ref_reg - 1);
      // This runtime call does not require a stack map.
      mips_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset,
                                                        instruction_,
                                                        this,
                                                        /* direct */ false);
    }
    __ B(GetExitLabel());
  }

 private:
  // The location (register) of the marked object reference.
  const Location ref_;

  // The location of the entrypoint if already loaded.
  const Location entrypoint_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathMIPS);
};

// Slow path marking an object reference `ref` during a read barrier,
// and if needed, atomically updating the field `obj.field` in the
// object `obj` holding this reference after marking (contrary to
// ReadBarrierMarkSlowPathMIPS above, which never tries to update
// `obj.field`).
//
// This means that after the execution of this slow path, both `ref`
// and `obj.field` will be up-to-date; i.e., after the flip, both will
// hold the same to-space reference (unless another thread installed
// another object reference (different from `ref`) in `obj.field`).
class ReadBarrierMarkAndUpdateFieldSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  ReadBarrierMarkAndUpdateFieldSlowPathMIPS(HInstruction* instruction,
                                            Location ref,
                                            Register obj,
                                            Location field_offset,
                                            Register temp1)
      : SlowPathCodeMIPS(instruction),
        ref_(ref),
        obj_(obj),
        field_offset_(field_offset),
        temp1_(temp1) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "ReadBarrierMarkAndUpdateFieldSlowPathMIPS";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Register ref_reg = ref_.AsRegister<Register>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_reg)) << ref_reg;
    // This slow path is only used by the UnsafeCASObject intrinsic.
    DCHECK((instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking and field updating slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kUnsafeCASObject);
    DCHECK(field_offset_.IsRegisterPair()) << field_offset_;

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
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    DCHECK((V0 <= ref_reg && ref_reg <= T7) ||
           (S2 <= ref_reg && ref_reg <= S7) ||
           (ref_reg == FP)) << ref_reg;
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
        Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(ref_reg - 1);
    // This runtime call does not require a stack map.
    mips_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset,
                                                      instruction_,
                                                      this,
                                                      /* direct */ false);

    // If the new reference is different from the old reference,
    // update the field in the holder (`*(obj_ + field_offset_)`).
    //
    // Note that this field could also hold a different object, if
    // another thread had concurrently changed it. In that case, the
    // the compare-and-set (CAS) loop below would abort, leaving the
    // field as-is.
    MipsLabel done;
    __ Beq(temp1_, ref_reg, &done);

    // Update the the holder's field atomically.  This may fail if
    // mutator updates before us, but it's OK.  This is achieved
    // using a strong compare-and-set (CAS) operation with relaxed
    // memory synchronization ordering, where the expected value is
    // the old reference and the desired value is the new reference.

    // Convenience aliases.
    Register base = obj_;
    // The UnsafeCASObject intrinsic uses a register pair as field
    // offset ("long offset"), of which only the low part contains
    // data.
    Register offset = field_offset_.AsRegisterPairLow<Register>();
    Register expected = temp1_;
    Register value = ref_reg;
    Register tmp_ptr = TMP;      // Pointer to actual memory.
    Register tmp = AT;           // Value in memory.

    __ Addu(tmp_ptr, base, offset);

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

    bool is_r6 = mips_codegen->GetInstructionSetFeatures().IsR6();
    MipsLabel loop_head, exit_loop;
    __ Bind(&loop_head);
    if (is_r6) {
      __ LlR6(tmp, tmp_ptr);
    } else {
      __ LlR2(tmp, tmp_ptr);
    }
    __ Bne(tmp, expected, &exit_loop);
    __ Move(tmp, value);
    if (is_r6) {
      __ ScR6(tmp, tmp_ptr);
    } else {
      __ ScR2(tmp, tmp_ptr);
    }
    __ Beqz(tmp, &loop_head);
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
    __ B(GetExitLabel());
  }

 private:
  // The location (register) of the marked object reference.
  const Location ref_;
  // The register containing the object holding the marked object reference field.
  const Register obj_;
  // The location of the offset of the marked reference field within `obj_`.
  Location field_offset_;

  const Register temp1_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkAndUpdateFieldSlowPathMIPS);
};

// Slow path generating a read barrier for a heap reference.
class ReadBarrierForHeapReferenceSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  ReadBarrierForHeapReferenceSlowPathMIPS(HInstruction* instruction,
                                          Location out,
                                          Location ref,
                                          Location obj,
                                          uint32_t offset,
                                          Location index)
      : SlowPathCodeMIPS(instruction),
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
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();
    Register reg_out = out_.AsRegister<Register>();
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
        Register index_reg = index_.AsRegister<Register>();
        DCHECK(locations->GetLiveRegisters()->ContainsCoreRegister(index_reg));
        if (codegen->IsCoreCalleeSaveRegister(index_reg)) {
          // We are about to change the value of `index_reg` (see the
          // calls to art::mips::MipsAssembler::Sll and
          // art::mips::MipsAssembler::Addiu32 below), but it has
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
          Register free_reg = FindAvailableCallerSaveRegister(codegen);
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
        DCHECK(index_.IsRegisterPair());
        // UnsafeGet's offset location is a register pair, the low
        // part contains the correct offset.
        index = index_.ToLow();
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
    mips_codegen->InvokeRuntime(kQuickReadBarrierSlow,
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<
        kQuickReadBarrierSlow, mirror::Object*, mirror::Object*, mirror::Object*, uint32_t>();
    mips_codegen->MoveLocation(out_,
                               calling_convention.GetReturnLocation(DataType::Type::kReference),
                               DataType::Type::kReference);

    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForHeapReferenceSlowPathMIPS"; }

 private:
  Register FindAvailableCallerSaveRegister(CodeGenerator* codegen) {
    size_t ref = static_cast<int>(ref_.AsRegister<Register>());
    size_t obj = static_cast<int>(obj_.AsRegister<Register>());
    for (size_t i = 0, e = codegen->GetNumberOfCoreRegisters(); i < e; ++i) {
      if (i != ref &&
          i != obj &&
          !codegen->IsCoreCalleeSaveRegister(i) &&
          !codegen->IsBlockedCoreRegister(i)) {
        return static_cast<Register>(i);
      }
    }
    // We shall never fail to find a free caller-save register, as
    // there are more than two core caller-save registers on MIPS
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

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForHeapReferenceSlowPathMIPS);
};

// Slow path generating a read barrier for a GC root.
class ReadBarrierForRootSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  ReadBarrierForRootSlowPathMIPS(HInstruction* instruction, Location out, Location root)
      : SlowPathCodeMIPS(instruction), out_(out), root_(root) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Register reg_out = out_.AsRegister<Register>();
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(reg_out));
    DCHECK(instruction_->IsLoadClass() || instruction_->IsLoadString())
        << "Unexpected instruction in read barrier for GC root slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen);
    mips_codegen->MoveLocation(Location::RegisterLocation(calling_convention.GetRegisterAt(0)),
                               root_,
                               DataType::Type::kReference);
    mips_codegen->InvokeRuntime(kQuickReadBarrierForRootSlow,
                                instruction_,
                                instruction_->GetDexPc(),
                                this);
    CheckEntrypointTypes<kQuickReadBarrierForRootSlow, mirror::Object*, GcRoot<mirror::Object>*>();
    mips_codegen->MoveLocation(out_,
                               calling_convention.GetReturnLocation(DataType::Type::kReference),
                               DataType::Type::kReference);

    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForRootSlowPathMIPS"; }

 private:
  const Location out_;
  const Location root_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForRootSlowPathMIPS);
};

CodeGeneratorMIPS::CodeGeneratorMIPS(HGraph* graph,
                                     const MipsInstructionSetFeatures& isa_features,
                                     const CompilerOptions& compiler_options,
                                     OptimizingCompilerStats* stats)
    : CodeGenerator(graph,
                    kNumberOfCoreRegisters,
                    kNumberOfFRegisters,
                    kNumberOfRegisterPairs,
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
      boot_image_method_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      method_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_type_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      type_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      boot_image_string_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      string_bss_entry_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_string_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_class_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      clobbered_ra_(false) {
  // Save RA (containing the return address) to mimic Quick.
  AddAllocatedRegister(Location::RegisterLocation(RA));
}

#undef __
// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<MipsAssembler*>(GetAssembler())->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kMipsPointerSize, x).Int32Value()

void CodeGeneratorMIPS::Finalize(CodeAllocator* allocator) {
  // Ensure that we fix up branches.
  __ FinalizeCode();

  // Adjust native pc offsets in stack maps.
  StackMapStream* stack_map_stream = GetStackMapStream();
  for (size_t i = 0, num = stack_map_stream->GetNumberOfStackMaps(); i != num; ++i) {
    uint32_t old_position =
        stack_map_stream->GetStackMap(i).native_pc_code_offset.Uint32Value(InstructionSet::kMips);
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

MipsAssembler* ParallelMoveResolverMIPS::GetAssembler() const {
  return codegen_->GetAssembler();
}

void ParallelMoveResolverMIPS::EmitMove(size_t index) {
  DCHECK_LT(index, moves_.size());
  MoveOperands* move = moves_[index];
  codegen_->MoveLocation(move->GetDestination(), move->GetSource(), move->GetType());
}

void ParallelMoveResolverMIPS::EmitSwap(size_t index) {
  DCHECK_LT(index, moves_.size());
  MoveOperands* move = moves_[index];
  DataType::Type type = move->GetType();
  Location loc1 = move->GetDestination();
  Location loc2 = move->GetSource();

  DCHECK(!loc1.IsConstant());
  DCHECK(!loc2.IsConstant());

  if (loc1.Equals(loc2)) {
    return;
  }

  if (loc1.IsRegister() && loc2.IsRegister()) {
    // Swap 2 GPRs.
    Register r1 = loc1.AsRegister<Register>();
    Register r2 = loc2.AsRegister<Register>();
    __ Move(TMP, r2);
    __ Move(r2, r1);
    __ Move(r1, TMP);
  } else if (loc1.IsFpuRegister() && loc2.IsFpuRegister()) {
    if (codegen_->GetGraph()->HasSIMD()) {
      __ MoveV(static_cast<VectorRegister>(FTMP), VectorRegisterFrom(loc1));
      __ MoveV(VectorRegisterFrom(loc1), VectorRegisterFrom(loc2));
      __ MoveV(VectorRegisterFrom(loc2), static_cast<VectorRegister>(FTMP));
    } else {
      FRegister f1 = loc1.AsFpuRegister<FRegister>();
      FRegister f2 = loc2.AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ MovS(FTMP, f2);
        __ MovS(f2, f1);
        __ MovS(f1, FTMP);
      } else {
        DCHECK_EQ(type, DataType::Type::kFloat64);
        __ MovD(FTMP, f2);
        __ MovD(f2, f1);
        __ MovD(f1, FTMP);
      }
    }
  } else if ((loc1.IsRegister() && loc2.IsFpuRegister()) ||
             (loc1.IsFpuRegister() && loc2.IsRegister())) {
    // Swap FPR and GPR.
    DCHECK_EQ(type, DataType::Type::kFloat32);  // Can only swap a float.
    FRegister f1 = loc1.IsFpuRegister() ? loc1.AsFpuRegister<FRegister>()
                                        : loc2.AsFpuRegister<FRegister>();
    Register r2 = loc1.IsRegister() ? loc1.AsRegister<Register>() : loc2.AsRegister<Register>();
    __ Move(TMP, r2);
    __ Mfc1(r2, f1);
    __ Mtc1(TMP, f1);
  } else if (loc1.IsRegisterPair() && loc2.IsRegisterPair()) {
    // Swap 2 GPR register pairs.
    Register r1 = loc1.AsRegisterPairLow<Register>();
    Register r2 = loc2.AsRegisterPairLow<Register>();
    __ Move(TMP, r2);
    __ Move(r2, r1);
    __ Move(r1, TMP);
    r1 = loc1.AsRegisterPairHigh<Register>();
    r2 = loc2.AsRegisterPairHigh<Register>();
    __ Move(TMP, r2);
    __ Move(r2, r1);
    __ Move(r1, TMP);
  } else if ((loc1.IsRegisterPair() && loc2.IsFpuRegister()) ||
             (loc1.IsFpuRegister() && loc2.IsRegisterPair())) {
    // Swap FPR and GPR register pair.
    DCHECK_EQ(type, DataType::Type::kFloat64);
    FRegister f1 = loc1.IsFpuRegister() ? loc1.AsFpuRegister<FRegister>()
                                        : loc2.AsFpuRegister<FRegister>();
    Register r2_l = loc1.IsRegisterPair() ? loc1.AsRegisterPairLow<Register>()
                                          : loc2.AsRegisterPairLow<Register>();
    Register r2_h = loc1.IsRegisterPair() ? loc1.AsRegisterPairHigh<Register>()
                                          : loc2.AsRegisterPairHigh<Register>();
    // Use 2 temporary registers because we can't first swap the low 32 bits of an FPR and
    // then swap the high 32 bits of the same FPR. mtc1 makes the high 32 bits of an FPR
    // unpredictable and the following mfch1 will fail.
    __ Mfc1(TMP, f1);
    __ MoveFromFpuHigh(AT, f1);
    __ Mtc1(r2_l, f1);
    __ MoveToFpuHigh(r2_h, f1);
    __ Move(r2_l, TMP);
    __ Move(r2_h, AT);
  } else if (loc1.IsStackSlot() && loc2.IsStackSlot()) {
    Exchange(loc1.GetStackIndex(), loc2.GetStackIndex(), /* double_slot */ false);
  } else if (loc1.IsDoubleStackSlot() && loc2.IsDoubleStackSlot()) {
    Exchange(loc1.GetStackIndex(), loc2.GetStackIndex(), /* double_slot */ true);
  } else if (loc1.IsSIMDStackSlot() && loc2.IsSIMDStackSlot()) {
    ExchangeQuadSlots(loc1.GetStackIndex(), loc2.GetStackIndex());
  } else if ((loc1.IsRegister() && loc2.IsStackSlot()) ||
             (loc1.IsStackSlot() && loc2.IsRegister())) {
    Register reg = loc1.IsRegister() ? loc1.AsRegister<Register>() : loc2.AsRegister<Register>();
    intptr_t offset = loc1.IsStackSlot() ? loc1.GetStackIndex() : loc2.GetStackIndex();
    __ Move(TMP, reg);
    __ LoadFromOffset(kLoadWord, reg, SP, offset);
    __ StoreToOffset(kStoreWord, TMP, SP, offset);
  } else if ((loc1.IsRegisterPair() && loc2.IsDoubleStackSlot()) ||
             (loc1.IsDoubleStackSlot() && loc2.IsRegisterPair())) {
    Register reg_l = loc1.IsRegisterPair() ? loc1.AsRegisterPairLow<Register>()
                                           : loc2.AsRegisterPairLow<Register>();
    Register reg_h = loc1.IsRegisterPair() ? loc1.AsRegisterPairHigh<Register>()
                                           : loc2.AsRegisterPairHigh<Register>();
    intptr_t offset_l = loc1.IsDoubleStackSlot() ? loc1.GetStackIndex() : loc2.GetStackIndex();
    intptr_t offset_h = loc1.IsDoubleStackSlot() ? loc1.GetHighStackIndex(kMipsWordSize)
                                                 : loc2.GetHighStackIndex(kMipsWordSize);
    __ Move(TMP, reg_l);
    __ LoadFromOffset(kLoadWord, reg_l, SP, offset_l);
    __ StoreToOffset(kStoreWord, TMP, SP, offset_l);
    __ Move(TMP, reg_h);
    __ LoadFromOffset(kLoadWord, reg_h, SP, offset_h);
    __ StoreToOffset(kStoreWord, TMP, SP, offset_h);
  } else if ((loc1.IsFpuRegister() && loc2.IsSIMDStackSlot()) ||
             (loc1.IsSIMDStackSlot() && loc2.IsFpuRegister())) {
    Location fp_loc = loc1.IsFpuRegister() ? loc1 : loc2;
    intptr_t offset = loc1.IsFpuRegister() ? loc2.GetStackIndex() : loc1.GetStackIndex();
    __ MoveV(static_cast<VectorRegister>(FTMP), VectorRegisterFrom(fp_loc));
    __ LoadQFromOffset(fp_loc.AsFpuRegister<FRegister>(), SP, offset);
    __ StoreQToOffset(FTMP, SP, offset);
  } else if (loc1.IsFpuRegister() || loc2.IsFpuRegister()) {
    FRegister reg = loc1.IsFpuRegister() ? loc1.AsFpuRegister<FRegister>()
                                         : loc2.AsFpuRegister<FRegister>();
    intptr_t offset = loc1.IsFpuRegister() ? loc2.GetStackIndex() : loc1.GetStackIndex();
    if (type == DataType::Type::kFloat32) {
      __ MovS(FTMP, reg);
      __ LoadSFromOffset(reg, SP, offset);
      __ StoreSToOffset(FTMP, SP, offset);
    } else {
      DCHECK_EQ(type, DataType::Type::kFloat64);
      __ MovD(FTMP, reg);
      __ LoadDFromOffset(reg, SP, offset);
      __ StoreDToOffset(FTMP, SP, offset);
    }
  } else {
    LOG(FATAL) << "Swap between " << loc1 << " and " << loc2 << " is unsupported";
  }
}

void ParallelMoveResolverMIPS::RestoreScratch(int reg) {
  __ Pop(static_cast<Register>(reg));
}

void ParallelMoveResolverMIPS::SpillScratch(int reg) {
  __ Push(static_cast<Register>(reg));
}

void ParallelMoveResolverMIPS::Exchange(int index1, int index2, bool double_slot) {
  // Allocate a scratch register other than TMP, if available.
  // Else, spill V0 (arbitrary choice) and use it as a scratch register (it will be
  // automatically unspilled when the scratch scope object is destroyed).
  ScratchRegisterScope ensure_scratch(this, TMP, V0, codegen_->GetNumberOfCoreRegisters());
  // If V0 spills onto the stack, SP-relative offsets need to be adjusted.
  int stack_offset = ensure_scratch.IsSpilled() ? kStackAlignment : 0;
  for (int i = 0; i <= (double_slot ? 1 : 0); i++, stack_offset += kMipsWordSize) {
    __ LoadFromOffset(kLoadWord,
                      Register(ensure_scratch.GetRegister()),
                      SP,
                      index1 + stack_offset);
    __ LoadFromOffset(kLoadWord,
                      TMP,
                      SP,
                      index2 + stack_offset);
    __ StoreToOffset(kStoreWord,
                     Register(ensure_scratch.GetRegister()),
                     SP,
                     index2 + stack_offset);
    __ StoreToOffset(kStoreWord, TMP, SP, index1 + stack_offset);
  }
}

void ParallelMoveResolverMIPS::ExchangeQuadSlots(int index1, int index2) {
  __ LoadQFromOffset(FTMP, SP, index1);
  __ LoadQFromOffset(FTMP2, SP, index2);
  __ StoreQToOffset(FTMP, SP, index2);
  __ StoreQToOffset(FTMP2, SP, index1);
}

void CodeGeneratorMIPS::ComputeSpillMask() {
  core_spill_mask_ = allocated_registers_.GetCoreRegisters() & core_callee_save_mask_;
  fpu_spill_mask_ = allocated_registers_.GetFloatingPointRegisters() & fpu_callee_save_mask_;
  DCHECK_NE(core_spill_mask_, 0u) << "At least the return address register must be saved";
  // If there're FPU callee-saved registers and there's an odd number of GPR callee-saved
  // registers, include the ZERO register to force alignment of FPU callee-saved registers
  // within the stack frame.
  if ((fpu_spill_mask_ != 0) && (POPCOUNT(core_spill_mask_) % 2 != 0)) {
    core_spill_mask_ |= (1 << ZERO);
  }
}

bool CodeGeneratorMIPS::HasAllocatedCalleeSaveRegisters() const {
  // If RA is clobbered by PC-relative operations on R2 and it's the only spilled register
  // (this can happen in leaf methods), force CodeGenerator::InitializeCodeGeneration()
  // into the path that creates a stack frame so that RA can be explicitly saved and restored.
  // RA can't otherwise be saved/restored when it's the only spilled register.
  return CodeGenerator::HasAllocatedCalleeSaveRegisters() || clobbered_ra_;
}

static dwarf::Reg DWARFReg(Register reg) {
  return dwarf::Reg::MipsCore(static_cast<int>(reg));
}

// TODO: mapping of floating-point registers to DWARF.

void CodeGeneratorMIPS::GenerateFrameEntry() {
  __ Bind(&frame_entry_label_);

  if (GetCompilerOptions().CountHotnessInCompiledCode()) {
    __ Lhu(TMP, kMethodRegisterArgument, ArtMethod::HotnessCountOffset().Int32Value());
    __ Addiu(TMP, TMP, 1);
    __ Sh(TMP, kMethodRegisterArgument, ArtMethod::HotnessCountOffset().Int32Value());
  }

  bool do_overflow_check =
      FrameNeedsStackCheck(GetFrameSize(), InstructionSet::kMips) || !IsLeafMethod();

  if (do_overflow_check) {
    __ LoadFromOffset(kLoadWord,
                      ZERO,
                      SP,
                      -static_cast<int32_t>(GetStackOverflowReservedBytes(InstructionSet::kMips)));
    RecordPcInfo(nullptr, 0);
  }

  if (HasEmptyFrame()) {
    CHECK_EQ(fpu_spill_mask_, 0u);
    CHECK_EQ(core_spill_mask_, 1u << RA);
    CHECK(!clobbered_ra_);
    return;
  }

  // Make sure the frame size isn't unreasonably large.
  if (GetFrameSize() > GetStackOverflowReservedBytes(InstructionSet::kMips)) {
    LOG(FATAL) << "Stack frame larger than "
        << GetStackOverflowReservedBytes(InstructionSet::kMips) << " bytes";
  }

  // Spill callee-saved registers.

  uint32_t ofs = GetFrameSize();
  __ IncreaseFrameSize(ofs);

  for (uint32_t mask = core_spill_mask_; mask != 0; ) {
    Register reg = static_cast<Register>(MostSignificantBit(mask));
    mask ^= 1u << reg;
    ofs -= kMipsWordSize;
    // The ZERO register is only included for alignment.
    if (reg != ZERO) {
      __ StoreToOffset(kStoreWord, reg, SP, ofs);
      __ cfi().RelOffset(DWARFReg(reg), ofs);
    }
  }

  for (uint32_t mask = fpu_spill_mask_; mask != 0; ) {
    FRegister reg = static_cast<FRegister>(MostSignificantBit(mask));
    mask ^= 1u << reg;
    ofs -= kMipsDoublewordSize;
    __ StoreDToOffset(reg, SP, ofs);
    // TODO: __ cfi().RelOffset(DWARFReg(reg), ofs);
  }

  // Save the current method if we need it. Note that we do not
  // do this in HCurrentMethod, as the instruction might have been removed
  // in the SSA graph.
  if (RequiresCurrentMethod()) {
    __ StoreToOffset(kStoreWord, kMethodRegisterArgument, SP, kCurrentMethodStackOffset);
  }

  if (GetGraph()->HasShouldDeoptimizeFlag()) {
    // Initialize should deoptimize flag to 0.
    __ StoreToOffset(kStoreWord, ZERO, SP, GetStackOffsetOfShouldDeoptimizeFlag());
  }
}

void CodeGeneratorMIPS::GenerateFrameExit() {
  __ cfi().RememberState();

  if (!HasEmptyFrame()) {
    // Restore callee-saved registers.

    // For better instruction scheduling restore RA before other registers.
    uint32_t ofs = GetFrameSize();
    for (uint32_t mask = core_spill_mask_; mask != 0; ) {
      Register reg = static_cast<Register>(MostSignificantBit(mask));
      mask ^= 1u << reg;
      ofs -= kMipsWordSize;
      // The ZERO register is only included for alignment.
      if (reg != ZERO) {
        __ LoadFromOffset(kLoadWord, reg, SP, ofs);
        __ cfi().Restore(DWARFReg(reg));
      }
    }

    for (uint32_t mask = fpu_spill_mask_; mask != 0; ) {
      FRegister reg = static_cast<FRegister>(MostSignificantBit(mask));
      mask ^= 1u << reg;
      ofs -= kMipsDoublewordSize;
      __ LoadDFromOffset(reg, SP, ofs);
      // TODO: __ cfi().Restore(DWARFReg(reg));
    }

    size_t frame_size = GetFrameSize();
    // Adjust the stack pointer in the delay slot if doing so doesn't break CFI.
    bool exchange = IsInt<16>(static_cast<int32_t>(frame_size));
    bool reordering = __ SetReorder(false);
    if (exchange) {
      __ Jr(RA);
      __ DecreaseFrameSize(frame_size);  // Single instruction in delay slot.
    } else {
      __ DecreaseFrameSize(frame_size);
      __ Jr(RA);
      __ Nop();  // In delay slot.
    }
    __ SetReorder(reordering);
  } else {
    __ Jr(RA);
    __ NopIfNoReordering();
  }

  __ cfi().RestoreState();
  __ cfi().DefCFAOffset(GetFrameSize());
}

void CodeGeneratorMIPS::Bind(HBasicBlock* block) {
  __ Bind(GetLabelOf(block));
}

VectorRegister VectorRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return static_cast<VectorRegister>(location.AsFpuRegister<FRegister>());
}

void CodeGeneratorMIPS::MoveLocation(Location destination,
                                     Location source,
                                     DataType::Type dst_type) {
  if (source.Equals(destination)) {
    return;
  }

  if (source.IsConstant()) {
    MoveConstant(destination, source.GetConstant());
  } else {
    if (destination.IsRegister()) {
      if (source.IsRegister()) {
        __ Move(destination.AsRegister<Register>(), source.AsRegister<Register>());
      } else if (source.IsFpuRegister()) {
        __ Mfc1(destination.AsRegister<Register>(), source.AsFpuRegister<FRegister>());
      } else {
        DCHECK(source.IsStackSlot()) << "Cannot move from " << source << " to " << destination;
      __ LoadFromOffset(kLoadWord, destination.AsRegister<Register>(), SP, source.GetStackIndex());
      }
    } else if (destination.IsRegisterPair()) {
      if (source.IsRegisterPair()) {
        __ Move(destination.AsRegisterPairHigh<Register>(), source.AsRegisterPairHigh<Register>());
        __ Move(destination.AsRegisterPairLow<Register>(), source.AsRegisterPairLow<Register>());
      } else if (source.IsFpuRegister()) {
        Register dst_high = destination.AsRegisterPairHigh<Register>();
        Register dst_low =  destination.AsRegisterPairLow<Register>();
        FRegister src = source.AsFpuRegister<FRegister>();
        __ Mfc1(dst_low, src);
        __ MoveFromFpuHigh(dst_high, src);
      } else {
        DCHECK(source.IsDoubleStackSlot())
            << "Cannot move from " << source << " to " << destination;
        int32_t off = source.GetStackIndex();
        Register r = destination.AsRegisterPairLow<Register>();
        __ LoadFromOffset(kLoadDoubleword, r, SP, off);
      }
    } else if (destination.IsFpuRegister()) {
      if (source.IsRegister()) {
        DCHECK(!DataType::Is64BitType(dst_type));
        __ Mtc1(source.AsRegister<Register>(), destination.AsFpuRegister<FRegister>());
      } else if (source.IsRegisterPair()) {
        DCHECK(DataType::Is64BitType(dst_type));
        FRegister dst = destination.AsFpuRegister<FRegister>();
        Register src_high = source.AsRegisterPairHigh<Register>();
        Register src_low = source.AsRegisterPairLow<Register>();
        __ Mtc1(src_low, dst);
        __ MoveToFpuHigh(src_high, dst);
      } else if (source.IsFpuRegister()) {
        if (GetGraph()->HasSIMD()) {
          __ MoveV(VectorRegisterFrom(destination),
                   VectorRegisterFrom(source));
        } else {
          if (DataType::Is64BitType(dst_type)) {
            __ MovD(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
          } else {
            DCHECK_EQ(dst_type, DataType::Type::kFloat32);
            __ MovS(destination.AsFpuRegister<FRegister>(), source.AsFpuRegister<FRegister>());
          }
        }
      } else if (source.IsSIMDStackSlot()) {
        __ LoadQFromOffset(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
      } else if (source.IsDoubleStackSlot()) {
        DCHECK(DataType::Is64BitType(dst_type));
        __ LoadDFromOffset(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
      } else {
        DCHECK(!DataType::Is64BitType(dst_type));
        DCHECK(source.IsStackSlot()) << "Cannot move from " << source << " to " << destination;
        __ LoadSFromOffset(destination.AsFpuRegister<FRegister>(), SP, source.GetStackIndex());
      }
    } else if (destination.IsSIMDStackSlot()) {
      if (source.IsFpuRegister()) {
        __ StoreQToOffset(source.AsFpuRegister<FRegister>(), SP, destination.GetStackIndex());
      } else {
        DCHECK(source.IsSIMDStackSlot());
        __ LoadQFromOffset(FTMP, SP, source.GetStackIndex());
        __ StoreQToOffset(FTMP, SP, destination.GetStackIndex());
      }
    } else if (destination.IsDoubleStackSlot()) {
      int32_t dst_offset = destination.GetStackIndex();
      if (source.IsRegisterPair()) {
        __ StoreToOffset(kStoreDoubleword, source.AsRegisterPairLow<Register>(), SP, dst_offset);
      } else if (source.IsFpuRegister()) {
        __ StoreDToOffset(source.AsFpuRegister<FRegister>(), SP, dst_offset);
      } else {
        DCHECK(source.IsDoubleStackSlot())
            << "Cannot move from " << source << " to " << destination;
        __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex());
        __ StoreToOffset(kStoreWord, TMP, SP, dst_offset);
        __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex() + 4);
        __ StoreToOffset(kStoreWord, TMP, SP, dst_offset + 4);
      }
    } else {
      DCHECK(destination.IsStackSlot()) << destination;
      int32_t dst_offset = destination.GetStackIndex();
      if (source.IsRegister()) {
        __ StoreToOffset(kStoreWord, source.AsRegister<Register>(), SP, dst_offset);
      } else if (source.IsFpuRegister()) {
        __ StoreSToOffset(source.AsFpuRegister<FRegister>(), SP, dst_offset);
      } else {
        DCHECK(source.IsStackSlot()) << "Cannot move from " << source << " to " << destination;
        __ LoadFromOffset(kLoadWord, TMP, SP, source.GetStackIndex());
        __ StoreToOffset(kStoreWord, TMP, SP, dst_offset);
      }
    }
  }
}

void CodeGeneratorMIPS::MoveConstant(Location destination, HConstant* c) {
  if (c->IsIntConstant() || c->IsNullConstant()) {
    // Move 32 bit constant.
    int32_t value = GetInt32ValueOf(c);
    if (destination.IsRegister()) {
      Register dst = destination.AsRegister<Register>();
      __ LoadConst32(dst, value);
    } else {
      DCHECK(destination.IsStackSlot())
          << "Cannot move " << c->DebugName() << " to " << destination;
      __ StoreConstToOffset(kStoreWord, value, SP, destination.GetStackIndex(), TMP);
    }
  } else if (c->IsLongConstant()) {
    // Move 64 bit constant.
    int64_t value = GetInt64ValueOf(c);
    if (destination.IsRegisterPair()) {
      Register r_h = destination.AsRegisterPairHigh<Register>();
      Register r_l = destination.AsRegisterPairLow<Register>();
      __ LoadConst64(r_h, r_l, value);
    } else {
      DCHECK(destination.IsDoubleStackSlot())
          << "Cannot move " << c->DebugName() << " to " << destination;
      __ StoreConstToOffset(kStoreDoubleword, value, SP, destination.GetStackIndex(), TMP);
    }
  } else if (c->IsFloatConstant()) {
    // Move 32 bit float constant.
    int32_t value = GetInt32ValueOf(c);
    if (destination.IsFpuRegister()) {
      __ LoadSConst32(destination.AsFpuRegister<FRegister>(), value, TMP);
    } else {
      DCHECK(destination.IsStackSlot())
          << "Cannot move " << c->DebugName() << " to " << destination;
      __ StoreConstToOffset(kStoreWord, value, SP, destination.GetStackIndex(), TMP);
    }
  } else {
    // Move 64 bit double constant.
    DCHECK(c->IsDoubleConstant()) << c->DebugName();
    int64_t value = GetInt64ValueOf(c);
    if (destination.IsFpuRegister()) {
      FRegister fd = destination.AsFpuRegister<FRegister>();
      __ LoadDConst64(fd, value, TMP);
    } else {
      DCHECK(destination.IsDoubleStackSlot())
          << "Cannot move " << c->DebugName() << " to " << destination;
      __ StoreConstToOffset(kStoreDoubleword, value, SP, destination.GetStackIndex(), TMP);
    }
  }
}

void CodeGeneratorMIPS::MoveConstant(Location destination, int32_t value) {
  DCHECK(destination.IsRegister());
  Register dst = destination.AsRegister<Register>();
  __ LoadConst32(dst, value);
}

void CodeGeneratorMIPS::AddLocationAsTemp(Location location, LocationSummary* locations) {
  if (location.IsRegister()) {
    locations->AddTemp(location);
  } else if (location.IsRegisterPair()) {
    locations->AddTemp(Location::RegisterLocation(location.AsRegisterPairLow<Register>()));
    locations->AddTemp(Location::RegisterLocation(location.AsRegisterPairHigh<Register>()));
  } else {
    UNIMPLEMENTED(FATAL) << "AddLocationAsTemp not implemented for location " << location;
  }
}

template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
inline void CodeGeneratorMIPS::EmitPcRelativeLinkerPatches(
    const ArenaDeque<PcRelativePatchInfo>& infos,
    ArenaVector<linker::LinkerPatch>* linker_patches) {
  for (const PcRelativePatchInfo& info : infos) {
    const DexFile* dex_file = info.target_dex_file;
    size_t offset_or_index = info.offset_or_index;
    DCHECK(info.label.IsBound());
    uint32_t literal_offset = __ GetLabelLocation(&info.label);
    // On R2 we use HMipsComputeBaseMethodAddress and patch relative to
    // the assembler's base label used for PC-relative addressing.
    const PcRelativePatchInfo& info_high = info.patch_info_high ? *info.patch_info_high : info;
    uint32_t pc_rel_offset = info_high.pc_rel_label.IsBound()
        ? __ GetLabelLocation(&info_high.pc_rel_label)
        : __ GetPcRelBaseLabelLocation();
    linker_patches->push_back(Factory(literal_offset, dex_file, pc_rel_offset, offset_or_index));
  }
}

void CodeGeneratorMIPS::EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) {
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

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewBootImageMethodPatch(
    MethodReference target_method,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &boot_image_method_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewMethodBssEntryPatch(
    MethodReference target_method,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, info_high, &method_bss_entry_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewBootImageTypePatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, &boot_image_type_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewTypeBssEntryPatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, type_index.index_, info_high, &type_bss_entry_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewBootImageStringPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(
      &dex_file, string_index.index_, info_high, &boot_image_string_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewStringBssEntryPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    const PcRelativePatchInfo* info_high) {
  return NewPcRelativePatch(&dex_file, string_index.index_, info_high, &string_bss_entry_patches_);
}

CodeGeneratorMIPS::PcRelativePatchInfo* CodeGeneratorMIPS::NewPcRelativePatch(
    const DexFile* dex_file,
    uint32_t offset_or_index,
    const PcRelativePatchInfo* info_high,
    ArenaDeque<PcRelativePatchInfo>* patches) {
  patches->emplace_back(dex_file, offset_or_index, info_high);
  return &patches->back();
}

Literal* CodeGeneratorMIPS::DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map) {
  return map->GetOrCreate(
      value,
      [this, value]() { return __ NewLiteral<uint32_t>(value); });
}

Literal* CodeGeneratorMIPS::DeduplicateBootImageAddressLiteral(uint32_t address) {
  return DeduplicateUint32Literal(dchecked_integral_cast<uint32_t>(address), &uint32_literals_);
}

void CodeGeneratorMIPS::EmitPcRelativeAddressPlaceholderHigh(PcRelativePatchInfo* info_high,
                                                             Register out,
                                                             Register base) {
  DCHECK(!info_high->patch_info_high);
  DCHECK_NE(out, base);
  bool reordering = __ SetReorder(false);
  if (GetInstructionSetFeatures().IsR6()) {
    DCHECK_EQ(base, ZERO);
    __ Bind(&info_high->label);
    __ Bind(&info_high->pc_rel_label);
    // Add the high half of a 32-bit offset to PC.
    __ Auipc(out, /* placeholder */ 0x1234);
    __ SetReorder(reordering);
  } else {
    // If base is ZERO, emit NAL to obtain the actual base.
    if (base == ZERO) {
      // Generate a dummy PC-relative call to obtain PC.
      __ Nal();
    }
    __ Bind(&info_high->label);
    __ Lui(out, /* placeholder */ 0x1234);
    // If we emitted the NAL, bind the pc_rel_label, otherwise base is a register holding
    // the HMipsComputeBaseMethodAddress which has its own label stored in MipsAssembler.
    if (base == ZERO) {
      __ Bind(&info_high->pc_rel_label);
    }
    __ SetReorder(reordering);
    // Add the high half of a 32-bit offset to PC.
    __ Addu(out, out, (base == ZERO) ? RA : base);
  }
  // A following instruction will add the sign-extended low half of the 32-bit
  // offset to `out` (e.g. lw, jialc, addiu).
}

CodeGeneratorMIPS::JitPatchInfo* CodeGeneratorMIPS::NewJitRootStringPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    Handle<mirror::String> handle) {
  ReserveJitStringRoot(StringReference(&dex_file, string_index), handle);
  jit_string_patches_.emplace_back(dex_file, string_index.index_);
  return &jit_string_patches_.back();
}

CodeGeneratorMIPS::JitPatchInfo* CodeGeneratorMIPS::NewJitRootClassPatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    Handle<mirror::Class> handle) {
  ReserveJitClassRoot(TypeReference(&dex_file, type_index), handle);
  jit_class_patches_.emplace_back(dex_file, type_index.index_);
  return &jit_class_patches_.back();
}

void CodeGeneratorMIPS::PatchJitRootUse(uint8_t* code,
                                        const uint8_t* roots_data,
                                        const CodeGeneratorMIPS::JitPatchInfo& info,
                                        uint64_t index_in_table) const {
  uint32_t high_literal_offset = GetAssembler().GetLabelLocation(&info.high_label);
  uint32_t low_literal_offset = GetAssembler().GetLabelLocation(&info.low_label);
  uintptr_t address =
      reinterpret_cast<uintptr_t>(roots_data) + index_in_table * sizeof(GcRoot<mirror::Object>);
  uint32_t addr32 = dchecked_integral_cast<uint32_t>(address);
  // lui reg, addr32_high
  DCHECK_EQ(code[high_literal_offset + 0], 0x34);
  DCHECK_EQ(code[high_literal_offset + 1], 0x12);
  DCHECK_EQ((code[high_literal_offset + 2] & 0xE0), 0x00);
  DCHECK_EQ(code[high_literal_offset + 3], 0x3C);
  // instr reg, reg, addr32_low
  DCHECK_EQ(code[low_literal_offset + 0], 0x78);
  DCHECK_EQ(code[low_literal_offset + 1], 0x56);
  addr32 += (addr32 & 0x8000) << 1;  // Account for sign extension in "instr reg, reg, addr32_low".
  // lui reg, addr32_high
  code[high_literal_offset + 0] = static_cast<uint8_t>(addr32 >> 16);
  code[high_literal_offset + 1] = static_cast<uint8_t>(addr32 >> 24);
  // instr reg, reg, addr32_low
  code[low_literal_offset + 0] = static_cast<uint8_t>(addr32 >> 0);
  code[low_literal_offset + 1] = static_cast<uint8_t>(addr32 >> 8);
}

void CodeGeneratorMIPS::EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) {
  for (const JitPatchInfo& info : jit_string_patches_) {
    StringReference string_reference(&info.target_dex_file, dex::StringIndex(info.index));
    uint64_t index_in_table = GetJitStringRootIndex(string_reference);
    PatchJitRootUse(code, roots_data, info, index_in_table);
  }
  for (const JitPatchInfo& info : jit_class_patches_) {
    TypeReference type_reference(&info.target_dex_file, dex::TypeIndex(info.index));
    uint64_t index_in_table = GetJitClassRootIndex(type_reference);
    PatchJitRootUse(code, roots_data, info, index_in_table);
  }
}

void CodeGeneratorMIPS::MarkGCCard(Register object,
                                   Register value,
                                   bool value_can_be_null) {
  MipsLabel done;
  Register card = AT;
  Register temp = TMP;
  if (value_can_be_null) {
    __ Beqz(value, &done);
  }
  __ LoadFromOffset(kLoadWord,
                    card,
                    TR,
                    Thread::CardTableOffset<kMipsPointerSize>().Int32Value());
  __ Srl(temp, object, gc::accounting::CardTable::kCardShift);
  __ Addu(temp, card, temp);
  __ Sb(card, temp, 0);
  if (value_can_be_null) {
    __ Bind(&done);
  }
}

void CodeGeneratorMIPS::SetupBlockedRegisters() const {
  // ZERO, K0, K1, GP, SP, RA are always reserved and can't be allocated.
  blocked_core_registers_[ZERO] = true;
  blocked_core_registers_[K0] = true;
  blocked_core_registers_[K1] = true;
  blocked_core_registers_[GP] = true;
  blocked_core_registers_[SP] = true;
  blocked_core_registers_[RA] = true;

  // AT and TMP(T8) are used as temporary/scratch registers
  // (similar to how AT is used by MIPS assemblers).
  blocked_core_registers_[AT] = true;
  blocked_core_registers_[TMP] = true;
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

  // Reserve odd-numbered FPU registers.
  for (size_t i = 1; i < kNumberOfFRegisters; i += 2) {
    blocked_fpu_registers_[i] = true;
  }

  if (GetGraph()->IsDebuggable()) {
    // Stubs do not save callee-save floating point registers. If the graph
    // is debuggable, we need to deal with these registers differently. For
    // now, just block them.
    for (size_t i = 0; i < arraysize(kFpuCalleeSaves); ++i) {
      blocked_fpu_registers_[kFpuCalleeSaves[i]] = true;
    }
  }
}

size_t CodeGeneratorMIPS::SaveCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ StoreToOffset(kStoreWord, Register(reg_id), SP, stack_index);
  return kMipsWordSize;
}

size_t CodeGeneratorMIPS::RestoreCoreRegister(size_t stack_index, uint32_t reg_id) {
  __ LoadFromOffset(kLoadWord, Register(reg_id), SP, stack_index);
  return kMipsWordSize;
}

size_t CodeGeneratorMIPS::SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  if (GetGraph()->HasSIMD()) {
    __ StoreQToOffset(FRegister(reg_id), SP, stack_index);
  } else {
    __ StoreDToOffset(FRegister(reg_id), SP, stack_index);
  }
  return GetFloatingPointSpillSlotSize();
}

size_t CodeGeneratorMIPS::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  if (GetGraph()->HasSIMD()) {
    __ LoadQFromOffset(FRegister(reg_id), SP, stack_index);
  } else {
    __ LoadDFromOffset(FRegister(reg_id), SP, stack_index);
  }
  return GetFloatingPointSpillSlotSize();
}

void CodeGeneratorMIPS::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << Register(reg);
}

void CodeGeneratorMIPS::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << FRegister(reg);
}

constexpr size_t kMipsDirectEntrypointRuntimeOffset = 16;

void CodeGeneratorMIPS::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                      HInstruction* instruction,
                                      uint32_t dex_pc,
                                      SlowPathCode* slow_path) {
  ValidateInvokeRuntime(entrypoint, instruction, slow_path);
  GenerateInvokeRuntime(GetThreadOffset<kMipsPointerSize>(entrypoint).Int32Value(),
                        IsDirectEntrypoint(entrypoint));
  if (EntrypointRequiresStackMap(entrypoint)) {
    RecordPcInfo(instruction, dex_pc, slow_path);
  }
}

void CodeGeneratorMIPS::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                            HInstruction* instruction,
                                                            SlowPathCode* slow_path,
                                                            bool direct) {
  ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction, slow_path);
  GenerateInvokeRuntime(entry_point_offset, direct);
}

void CodeGeneratorMIPS::GenerateInvokeRuntime(int32_t entry_point_offset, bool direct) {
  bool reordering = __ SetReorder(false);
  __ LoadFromOffset(kLoadWord, T9, TR, entry_point_offset);
  __ Jalr(T9);
  if (direct) {
    // Reserve argument space on stack (for $a0-$a3) for
    // entrypoints that directly reference native implementations.
    // Called function may use this space to store $a0-$a3 regs.
    __ IncreaseFrameSize(kMipsDirectEntrypointRuntimeOffset);  // Single instruction in delay slot.
    __ DecreaseFrameSize(kMipsDirectEntrypointRuntimeOffset);
  } else {
    __ Nop();  // In delay slot.
  }
  __ SetReorder(reordering);
}

void InstructionCodeGeneratorMIPS::GenerateClassInitializationCheck(SlowPathCodeMIPS* slow_path,
                                                                    Register class_reg) {
  constexpr size_t status_lsb_position = SubtypeCheckBits::BitStructSizeOf();
  const size_t status_byte_offset =
      mirror::Class::StatusOffset().SizeValue() + (status_lsb_position / kBitsPerByte);
  constexpr uint32_t shifted_initialized_value =
      enum_cast<uint32_t>(ClassStatus::kInitialized) << (status_lsb_position % kBitsPerByte);

  __ LoadFromOffset(kLoadUnsignedByte, TMP, class_reg, status_byte_offset);
  __ Sltiu(TMP, TMP, shifted_initialized_value);
  __ Bnez(TMP, slow_path->GetEntryLabel());
  // Even if the initialized flag is set, we need to ensure consistent memory ordering.
  __ Sync(0);
  __ Bind(slow_path->GetExitLabel());
}

void InstructionCodeGeneratorMIPS::GenerateMemoryBarrier(MemBarrierKind kind ATTRIBUTE_UNUSED) {
  __ Sync(0);  // Only stype 0 is supported.
}

void InstructionCodeGeneratorMIPS::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                        HBasicBlock* successor) {
  SuspendCheckSlowPathMIPS* slow_path =
      down_cast<SuspendCheckSlowPathMIPS*>(instruction->GetSlowPath());

  if (slow_path == nullptr) {
    slow_path =
        new (codegen_->GetScopedAllocator()) SuspendCheckSlowPathMIPS(instruction, successor);
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
                    Thread::ThreadFlagsOffset<kMipsPointerSize>().Int32Value());
  if (successor == nullptr) {
    __ Bnez(TMP, slow_path->GetEntryLabel());
    __ Bind(slow_path->GetReturnLabel());
  } else {
    __ Beqz(TMP, codegen_->GetLabelOf(successor));
    __ B(slow_path->GetEntryLabel());
    // slow_path will return to GetLabelOf(successor).
  }
}

InstructionCodeGeneratorMIPS::InstructionCodeGeneratorMIPS(HGraph* graph,
                                                           CodeGeneratorMIPS* codegen)
      : InstructionCodeGenerator(graph, codegen),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void LocationsBuilderMIPS::HandleBinaryOp(HBinaryOperation* instruction) {
  DCHECK_EQ(instruction->InputCount(), 2U);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  DataType::Type type = instruction->GetResultType();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  switch (type) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      HInstruction* right = instruction->InputAt(1);
      bool can_use_imm = false;
      if (right->IsConstant()) {
        int32_t imm = CodeGenerator::GetInt32ValueOf(right->AsConstant());
        if (instruction->IsAnd() || instruction->IsOr() || instruction->IsXor()) {
          can_use_imm = IsUint<16>(imm);
        } else {
          DCHECK(instruction->IsSub() || instruction->IsAdd());
          if (instruction->IsSub()) {
            imm = -imm;
          }
          if (isR6) {
            bool single_use = right->GetUses().HasExactlyOneElement();
            int16_t imm_high = High16Bits(imm);
            int16_t imm_low = Low16Bits(imm);
            if (imm_low < 0) {
              imm_high += 1;
            }
            can_use_imm = !((imm_high != 0) && (imm_low != 0)) || single_use;
          } else {
            can_use_imm = IsInt<16>(imm);
          }
        }
      }
      if (can_use_imm)
        locations->SetInAt(1, Location::ConstantLocation(right->AsConstant()));
      else
        locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      DCHECK(instruction->IsAdd() || instruction->IsSub());
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected " << instruction->DebugName() << " type " << type;
  }
}

void InstructionCodeGeneratorMIPS::HandleBinaryOp(HBinaryOperation* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  switch (type) {
    case DataType::Type::kInt32: {
      Register dst = locations->Out().AsRegister<Register>();
      Register lhs = locations->InAt(0).AsRegister<Register>();
      Location rhs_location = locations->InAt(1);

      Register rhs_reg = ZERO;
      int32_t rhs_imm = 0;
      bool use_imm = rhs_location.IsConstant();
      if (use_imm) {
        rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
      } else {
        rhs_reg = rhs_location.AsRegister<Register>();
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
      } else {
        DCHECK(instruction->IsAdd() || instruction->IsSub());
        if (use_imm) {
          if (instruction->IsSub()) {
            rhs_imm = -rhs_imm;
          }
          if (IsInt<16>(rhs_imm)) {
            __ Addiu(dst, lhs, rhs_imm);
          } else {
            DCHECK(isR6);
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
        } else if (instruction->IsAdd()) {
          __ Addu(dst, lhs, rhs_reg);
        } else {
          DCHECK(instruction->IsSub());
          __ Subu(dst, lhs, rhs_reg);
        }
      }
      break;
    }

    case DataType::Type::kInt64: {
      Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
      Register dst_low = locations->Out().AsRegisterPairLow<Register>();
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
      Location rhs_location = locations->InAt(1);
      bool use_imm = rhs_location.IsConstant();
      if (!use_imm) {
        Register rhs_high = rhs_location.AsRegisterPairHigh<Register>();
        Register rhs_low = rhs_location.AsRegisterPairLow<Register>();
        if (instruction->IsAnd()) {
          __ And(dst_low, lhs_low, rhs_low);
          __ And(dst_high, lhs_high, rhs_high);
        } else if (instruction->IsOr()) {
          __ Or(dst_low, lhs_low, rhs_low);
          __ Or(dst_high, lhs_high, rhs_high);
        } else if (instruction->IsXor()) {
          __ Xor(dst_low, lhs_low, rhs_low);
          __ Xor(dst_high, lhs_high, rhs_high);
        } else if (instruction->IsAdd()) {
          if (lhs_low == rhs_low) {
            // Special case for lhs = rhs and the sum potentially overwriting both lhs and rhs.
            __ Slt(TMP, lhs_low, ZERO);
            __ Addu(dst_low, lhs_low, rhs_low);
          } else {
            __ Addu(dst_low, lhs_low, rhs_low);
            // If the sum overwrites rhs, lhs remains unchanged, otherwise rhs remains unchanged.
            __ Sltu(TMP, dst_low, (dst_low == rhs_low) ? lhs_low : rhs_low);
          }
          __ Addu(dst_high, lhs_high, rhs_high);
          __ Addu(dst_high, dst_high, TMP);
        } else {
          DCHECK(instruction->IsSub());
          __ Sltu(TMP, lhs_low, rhs_low);
          __ Subu(dst_low, lhs_low, rhs_low);
          __ Subu(dst_high, lhs_high, rhs_high);
          __ Subu(dst_high, dst_high, TMP);
        }
      } else {
        int64_t value = CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant()->AsConstant());
        if (instruction->IsOr()) {
          uint32_t low = Low32Bits(value);
          uint32_t high = High32Bits(value);
          if (IsUint<16>(low)) {
            if (dst_low != lhs_low || low != 0) {
              __ Ori(dst_low, lhs_low, low);
            }
          } else {
            __ LoadConst32(TMP, low);
            __ Or(dst_low, lhs_low, TMP);
          }
          if (IsUint<16>(high)) {
            if (dst_high != lhs_high || high != 0) {
              __ Ori(dst_high, lhs_high, high);
            }
          } else {
            if (high != low) {
              __ LoadConst32(TMP, high);
            }
            __ Or(dst_high, lhs_high, TMP);
          }
        } else if (instruction->IsXor()) {
          uint32_t low = Low32Bits(value);
          uint32_t high = High32Bits(value);
          if (IsUint<16>(low)) {
            if (dst_low != lhs_low || low != 0) {
              __ Xori(dst_low, lhs_low, low);
            }
          } else {
            __ LoadConst32(TMP, low);
            __ Xor(dst_low, lhs_low, TMP);
          }
          if (IsUint<16>(high)) {
            if (dst_high != lhs_high || high != 0) {
              __ Xori(dst_high, lhs_high, high);
            }
          } else {
            if (high != low) {
              __ LoadConst32(TMP, high);
            }
            __ Xor(dst_high, lhs_high, TMP);
          }
        } else if (instruction->IsAnd()) {
          uint32_t low = Low32Bits(value);
          uint32_t high = High32Bits(value);
          if (IsUint<16>(low)) {
            __ Andi(dst_low, lhs_low, low);
          } else if (low != 0xFFFFFFFF) {
            __ LoadConst32(TMP, low);
            __ And(dst_low, lhs_low, TMP);
          } else if (dst_low != lhs_low) {
            __ Move(dst_low, lhs_low);
          }
          if (IsUint<16>(high)) {
            __ Andi(dst_high, lhs_high, high);
          } else if (high != 0xFFFFFFFF) {
            if (high != low) {
              __ LoadConst32(TMP, high);
            }
            __ And(dst_high, lhs_high, TMP);
          } else if (dst_high != lhs_high) {
            __ Move(dst_high, lhs_high);
          }
        } else {
          if (instruction->IsSub()) {
            value = -value;
          } else {
            DCHECK(instruction->IsAdd());
          }
          int32_t low = Low32Bits(value);
          int32_t high = High32Bits(value);
          if (IsInt<16>(low)) {
            if (dst_low != lhs_low || low != 0) {
              __ Addiu(dst_low, lhs_low, low);
            }
            if (low != 0) {
              __ Sltiu(AT, dst_low, low);
            }
          } else {
            __ LoadConst32(TMP, low);
            __ Addu(dst_low, lhs_low, TMP);
            __ Sltu(AT, dst_low, TMP);
          }
          if (IsInt<16>(high)) {
            if (dst_high != lhs_high || high != 0) {
              __ Addiu(dst_high, lhs_high, high);
            }
          } else {
            if (high != low) {
              __ LoadConst32(TMP, high);
            }
            __ Addu(dst_high, lhs_high, TMP);
          }
          if (low != 0) {
            __ Addu(dst_high, dst_high, AT);
          }
        }
      }
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FRegister dst = locations->Out().AsFpuRegister<FRegister>();
      FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
      if (instruction->IsAdd()) {
        if (type == DataType::Type::kFloat32) {
          __ AddS(dst, lhs, rhs);
        } else {
          __ AddD(dst, lhs, rhs);
        }
      } else {
        DCHECK(instruction->IsSub());
        if (type == DataType::Type::kFloat32) {
          __ SubS(dst, lhs, rhs);
        } else {
          __ SubD(dst, lhs, rhs);
        }
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected binary operation type " << type;
  }
}

void LocationsBuilderMIPS::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr() || instr->IsRor());

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instr);
  DataType::Type type = instr->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instr->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instr->InputAt(1)));
      locations->SetOut(Location::RequiresRegister());
      break;
    default:
      LOG(FATAL) << "Unexpected shift type " << type;
  }
}

static constexpr size_t kMipsBitsPerWord = kMipsWordSize * kBitsPerByte;

void InstructionCodeGeneratorMIPS::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr() || instr->IsRor());
  LocationSummary* locations = instr->GetLocations();
  DataType::Type type = instr->GetType();

  Location rhs_location = locations->InAt(1);
  bool use_imm = rhs_location.IsConstant();
  Register rhs_reg = use_imm ? ZERO : rhs_location.AsRegister<Register>();
  int64_t rhs_imm = use_imm ? CodeGenerator::GetInt64ValueOf(rhs_location.GetConstant()) : 0;
  const uint32_t shift_mask =
      (type == DataType::Type::kInt32) ? kMaxIntShiftDistance : kMaxLongShiftDistance;
  const uint32_t shift_value = rhs_imm & shift_mask;
  // Are the INS (Insert Bit Field) and ROTR instructions supported?
  bool has_ins_rotr = codegen_->GetInstructionSetFeatures().IsMipsIsaRevGreaterThanEqual2();

  switch (type) {
    case DataType::Type::kInt32: {
      Register dst = locations->Out().AsRegister<Register>();
      Register lhs = locations->InAt(0).AsRegister<Register>();
      if (use_imm) {
        if (shift_value == 0) {
          if (dst != lhs) {
            __ Move(dst, lhs);
          }
        } else if (instr->IsShl()) {
          __ Sll(dst, lhs, shift_value);
        } else if (instr->IsShr()) {
          __ Sra(dst, lhs, shift_value);
        } else if (instr->IsUShr()) {
          __ Srl(dst, lhs, shift_value);
        } else {
          if (has_ins_rotr) {
            __ Rotr(dst, lhs, shift_value);
          } else {
            __ Sll(TMP, lhs, (kMipsBitsPerWord - shift_value) & shift_mask);
            __ Srl(dst, lhs, shift_value);
            __ Or(dst, dst, TMP);
          }
        }
      } else {
        if (instr->IsShl()) {
          __ Sllv(dst, lhs, rhs_reg);
        } else if (instr->IsShr()) {
          __ Srav(dst, lhs, rhs_reg);
        } else if (instr->IsUShr()) {
          __ Srlv(dst, lhs, rhs_reg);
        } else {
          if (has_ins_rotr) {
            __ Rotrv(dst, lhs, rhs_reg);
          } else {
            __ Subu(TMP, ZERO, rhs_reg);
            // 32-bit shift instructions use the 5 least significant bits of the shift count, so
            // shifting by `-rhs_reg` is equivalent to shifting by `(32 - rhs_reg) & 31`. The case
            // when `rhs_reg & 31 == 0` is OK even though we don't shift `lhs` left all the way out
            // by 32, because the result in this case is computed as `(lhs >> 0) | (lhs << 0)`,
            // IOW, the OR'd values are equal.
            __ Sllv(TMP, lhs, TMP);
            __ Srlv(dst, lhs, rhs_reg);
            __ Or(dst, dst, TMP);
          }
        }
      }
      break;
    }

    case DataType::Type::kInt64: {
      Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
      Register dst_low = locations->Out().AsRegisterPairLow<Register>();
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
      if (use_imm) {
          if (shift_value == 0) {
            codegen_->MoveLocation(locations->Out(), locations->InAt(0), type);
          } else if (shift_value < kMipsBitsPerWord) {
            if (has_ins_rotr) {
              if (instr->IsShl()) {
                __ Srl(dst_high, lhs_low, kMipsBitsPerWord - shift_value);
                __ Ins(dst_high, lhs_high, shift_value, kMipsBitsPerWord - shift_value);
                __ Sll(dst_low, lhs_low, shift_value);
              } else if (instr->IsShr()) {
                __ Srl(dst_low, lhs_low, shift_value);
                __ Ins(dst_low, lhs_high, kMipsBitsPerWord - shift_value, shift_value);
                __ Sra(dst_high, lhs_high, shift_value);
              } else if (instr->IsUShr()) {
                __ Srl(dst_low, lhs_low, shift_value);
                __ Ins(dst_low, lhs_high, kMipsBitsPerWord - shift_value, shift_value);
                __ Srl(dst_high, lhs_high, shift_value);
              } else {
                __ Srl(dst_low, lhs_low, shift_value);
                __ Ins(dst_low, lhs_high, kMipsBitsPerWord - shift_value, shift_value);
                __ Srl(dst_high, lhs_high, shift_value);
                __ Ins(dst_high, lhs_low, kMipsBitsPerWord - shift_value, shift_value);
              }
            } else {
              if (instr->IsShl()) {
                __ Sll(dst_low, lhs_low, shift_value);
                __ Srl(TMP, lhs_low, kMipsBitsPerWord - shift_value);
                __ Sll(dst_high, lhs_high, shift_value);
                __ Or(dst_high, dst_high, TMP);
              } else if (instr->IsShr()) {
                __ Sra(dst_high, lhs_high, shift_value);
                __ Sll(TMP, lhs_high, kMipsBitsPerWord - shift_value);
                __ Srl(dst_low, lhs_low, shift_value);
                __ Or(dst_low, dst_low, TMP);
              } else if (instr->IsUShr()) {
                __ Srl(dst_high, lhs_high, shift_value);
                __ Sll(TMP, lhs_high, kMipsBitsPerWord - shift_value);
                __ Srl(dst_low, lhs_low, shift_value);
                __ Or(dst_low, dst_low, TMP);
              } else {
                __ Srl(TMP, lhs_low, shift_value);
                __ Sll(dst_low, lhs_high, kMipsBitsPerWord - shift_value);
                __ Or(dst_low, dst_low, TMP);
                __ Srl(TMP, lhs_high, shift_value);
                __ Sll(dst_high, lhs_low, kMipsBitsPerWord - shift_value);
                __ Or(dst_high, dst_high, TMP);
              }
            }
          } else {
            const uint32_t shift_value_high = shift_value - kMipsBitsPerWord;
            if (instr->IsShl()) {
              __ Sll(dst_high, lhs_low, shift_value_high);
              __ Move(dst_low, ZERO);
            } else if (instr->IsShr()) {
              __ Sra(dst_low, lhs_high, shift_value_high);
              __ Sra(dst_high, dst_low, kMipsBitsPerWord - 1);
            } else if (instr->IsUShr()) {
              __ Srl(dst_low, lhs_high, shift_value_high);
              __ Move(dst_high, ZERO);
            } else {
              if (shift_value == kMipsBitsPerWord) {
                // 64-bit rotation by 32 is just a swap.
                __ Move(dst_low, lhs_high);
                __ Move(dst_high, lhs_low);
              } else {
                if (has_ins_rotr) {
                  __ Srl(dst_low, lhs_high, shift_value_high);
                  __ Ins(dst_low, lhs_low, kMipsBitsPerWord - shift_value_high, shift_value_high);
                  __ Srl(dst_high, lhs_low, shift_value_high);
                  __ Ins(dst_high, lhs_high, kMipsBitsPerWord - shift_value_high, shift_value_high);
                } else {
                  __ Sll(TMP, lhs_low, kMipsBitsPerWord - shift_value_high);
                  __ Srl(dst_low, lhs_high, shift_value_high);
                  __ Or(dst_low, dst_low, TMP);
                  __ Sll(TMP, lhs_high, kMipsBitsPerWord - shift_value_high);
                  __ Srl(dst_high, lhs_low, shift_value_high);
                  __ Or(dst_high, dst_high, TMP);
                }
              }
            }
          }
      } else {
        const bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
        MipsLabel done;
        if (instr->IsShl()) {
          __ Sllv(dst_low, lhs_low, rhs_reg);
          __ Nor(AT, ZERO, rhs_reg);
          __ Srl(TMP, lhs_low, 1);
          __ Srlv(TMP, TMP, AT);
          __ Sllv(dst_high, lhs_high, rhs_reg);
          __ Or(dst_high, dst_high, TMP);
          __ Andi(TMP, rhs_reg, kMipsBitsPerWord);
          if (isR6) {
            __ Beqzc(TMP, &done, /* is_bare */ true);
            __ Move(dst_high, dst_low);
            __ Move(dst_low, ZERO);
          } else {
            __ Movn(dst_high, dst_low, TMP);
            __ Movn(dst_low, ZERO, TMP);
          }
        } else if (instr->IsShr()) {
          __ Srav(dst_high, lhs_high, rhs_reg);
          __ Nor(AT, ZERO, rhs_reg);
          __ Sll(TMP, lhs_high, 1);
          __ Sllv(TMP, TMP, AT);
          __ Srlv(dst_low, lhs_low, rhs_reg);
          __ Or(dst_low, dst_low, TMP);
          __ Andi(TMP, rhs_reg, kMipsBitsPerWord);
          if (isR6) {
            __ Beqzc(TMP, &done, /* is_bare */ true);
            __ Move(dst_low, dst_high);
            __ Sra(dst_high, dst_high, 31);
          } else {
            __ Sra(AT, dst_high, 31);
            __ Movn(dst_low, dst_high, TMP);
            __ Movn(dst_high, AT, TMP);
          }
        } else if (instr->IsUShr()) {
          __ Srlv(dst_high, lhs_high, rhs_reg);
          __ Nor(AT, ZERO, rhs_reg);
          __ Sll(TMP, lhs_high, 1);
          __ Sllv(TMP, TMP, AT);
          __ Srlv(dst_low, lhs_low, rhs_reg);
          __ Or(dst_low, dst_low, TMP);
          __ Andi(TMP, rhs_reg, kMipsBitsPerWord);
          if (isR6) {
            __ Beqzc(TMP, &done, /* is_bare */ true);
            __ Move(dst_low, dst_high);
            __ Move(dst_high, ZERO);
          } else {
            __ Movn(dst_low, dst_high, TMP);
            __ Movn(dst_high, ZERO, TMP);
          }
        } else {  // Rotate.
          __ Nor(AT, ZERO, rhs_reg);
          __ Srlv(TMP, lhs_low, rhs_reg);
          __ Sll(dst_low, lhs_high, 1);
          __ Sllv(dst_low, dst_low, AT);
          __ Or(dst_low, dst_low, TMP);
          __ Srlv(TMP, lhs_high, rhs_reg);
          __ Sll(dst_high, lhs_low, 1);
          __ Sllv(dst_high, dst_high, AT);
          __ Or(dst_high, dst_high, TMP);
          __ Andi(TMP, rhs_reg, kMipsBitsPerWord);
          if (isR6) {
            __ Beqzc(TMP, &done, /* is_bare */ true);
            __ Move(TMP, dst_high);
            __ Move(dst_high, dst_low);
            __ Move(dst_low, TMP);
          } else {
            __ Movn(AT, dst_high, TMP);
            __ Movn(dst_high, dst_low, TMP);
            __ Movn(dst_low, AT, TMP);
          }
        }
        __ Bind(&done);
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected shift operation type " << type;
  }
}

void LocationsBuilderMIPS::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS::VisitArrayGet(HArrayGet* instruction) {
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
  // path in CodeGeneratorMIPS::GenerateArrayLoadWithBakerReadBarrier.
  if (object_array_get_with_read_barrier && kUseBakerReadBarrier) {
    bool temp_needed = instruction->GetIndex()->IsConstant()
        ? !kBakerReadBarrierThunksEnableForFields
        : !kBakerReadBarrierThunksEnableForArrays;
    if (temp_needed) {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
}

static auto GetImplicitNullChecker(HInstruction* instruction, CodeGeneratorMIPS* codegen) {
  auto null_checker = [codegen, instruction]() {
    codegen->MaybeRecordImplicitNullCheck(instruction);
  };
  return null_checker;
}

void InstructionCodeGeneratorMIPS::VisitArrayGet(HArrayGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = obj_loc.AsRegister<Register>();
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
      Register out = out_loc.AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadUnsignedByte, out, obj, offset, null_checker);
      } else {
        __ Addu(TMP, obj, index.AsRegister<Register>());
        __ LoadFromOffset(kLoadUnsignedByte, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt8: {
      Register out = out_loc.AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1) + data_offset;
        __ LoadFromOffset(kLoadSignedByte, out, obj, offset, null_checker);
      } else {
        __ Addu(TMP, obj, index.AsRegister<Register>());
        __ LoadFromOffset(kLoadSignedByte, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint16: {
      Register out = out_loc.AsRegister<Register>();
      if (maybe_compressed_char_at) {
        uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
        __ LoadFromOffset(kLoadWord, TMP, obj, count_offset, null_checker);
        __ Sll(TMP, TMP, 31);    // Extract compression flag into the most significant bit of TMP.
        static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                      "Expecting 0=compressed, 1=uncompressed");
      }
      if (index.IsConstant()) {
        int32_t const_index = index.GetConstant()->AsIntConstant()->GetValue();
        if (maybe_compressed_char_at) {
          MipsLabel uncompressed_load, done;
          __ Bnez(TMP, &uncompressed_load);
          __ LoadFromOffset(kLoadUnsignedByte,
                            out,
                            obj,
                            data_offset + (const_index << TIMES_1));
          __ B(&done);
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
        Register index_reg = index.AsRegister<Register>();
        if (maybe_compressed_char_at) {
          MipsLabel uncompressed_load, done;
          __ Bnez(TMP, &uncompressed_load);
          __ Addu(TMP, obj, index_reg);
          __ LoadFromOffset(kLoadUnsignedByte, out, TMP, data_offset);
          __ B(&done);
          __ Bind(&uncompressed_load);
          __ ShiftAndAdd(TMP, index_reg, obj, TIMES_2, TMP);
          __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset);
          __ Bind(&done);
        } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
          __ Addu(TMP, index_reg, obj);
          __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset, null_checker);
        } else {
          __ ShiftAndAdd(TMP, index_reg, obj, TIMES_2, TMP);
          __ LoadFromOffset(kLoadUnsignedHalfword, out, TMP, data_offset, null_checker);
        }
      }
      break;
    }

    case DataType::Type::kInt16: {
      Register out = out_loc.AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2) + data_offset;
        __ LoadFromOffset(kLoadSignedHalfword, out, obj, offset, null_checker);
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(TMP, index.AsRegister<Register>(), obj);
        __ LoadFromOffset(kLoadSignedHalfword, out, TMP, data_offset, null_checker);
      } else {
        __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_2, TMP);
        __ LoadFromOffset(kLoadSignedHalfword, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt32: {
      DCHECK_EQ(sizeof(mirror::HeapReference<mirror::Object>), sizeof(int32_t));
      Register out = out_loc.AsRegister<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadFromOffset(kLoadWord, out, obj, offset, null_checker);
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(TMP, index.AsRegister<Register>(), obj);
        __ LoadFromOffset(kLoadWord, out, TMP, data_offset, null_checker);
      } else {
        __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_4, TMP);
        __ LoadFromOffset(kLoadWord, out, TMP, data_offset, null_checker);
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
        // CodeGeneratorMIPS::GenerateArrayLoadWithBakerReadBarrier call.
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
        Register out = out_loc.AsRegister<Register>();
        if (index.IsConstant()) {
          size_t offset =
              (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
          __ LoadFromOffset(kLoadWord, out, obj, offset, null_checker);
          // If read barriers are enabled, emit read barriers other than
          // Baker's using a slow path (and also unpoison the loaded
          // reference, if heap poisoning is enabled).
          codegen_->MaybeGenerateReadBarrierSlow(instruction, out_loc, out_loc, obj_loc, offset);
        } else {
          __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_4, TMP);
          __ LoadFromOffset(kLoadWord, out, TMP, data_offset, null_checker);
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
      Register out = out_loc.AsRegisterPairLow<Register>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadFromOffset(kLoadDoubleword, out, obj, offset, null_checker);
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(TMP, index.AsRegister<Register>(), obj);
        __ LoadFromOffset(kLoadDoubleword, out, TMP, data_offset, null_checker);
      } else {
        __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_8, TMP);
        __ LoadFromOffset(kLoadDoubleword, out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      FRegister out = out_loc.AsFpuRegister<FRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4) + data_offset;
        __ LoadSFromOffset(out, obj, offset, null_checker);
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(TMP, index.AsRegister<Register>(), obj);
        __ LoadSFromOffset(out, TMP, data_offset, null_checker);
      } else {
        __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_4, TMP);
        __ LoadSFromOffset(out, TMP, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      FRegister out = out_loc.AsFpuRegister<FRegister>();
      if (index.IsConstant()) {
        size_t offset =
            (index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8) + data_offset;
        __ LoadDFromOffset(out, obj, offset, null_checker);
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(TMP, index.AsRegister<Register>(), obj);
        __ LoadDFromOffset(out, TMP, data_offset, null_checker);
      } else {
        __ ShiftAndAdd(TMP, index.AsRegister<Register>(), obj, TIMES_8, TMP);
        __ LoadDFromOffset(out, TMP, data_offset, null_checker);
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

void LocationsBuilderMIPS::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  uint32_t offset = CodeGenerator::GetArrayLengthOffset(instruction);
  Register obj = locations->InAt(0).AsRegister<Register>();
  Register out = locations->Out().AsRegister<Register>();
  __ LoadFromOffset(kLoadWord, out, obj, offset);
  codegen_->MaybeRecordImplicitNullCheck(instruction);
  // Mask out compression flag from String's array length.
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    __ Srl(out, out, 1u);
  }
}

Location LocationsBuilderMIPS::RegisterOrZeroConstant(HInstruction* instruction) {
  return (instruction->IsConstant() && instruction->AsConstant()->IsZeroBitPattern())
      ? Location::ConstantLocation(instruction->AsConstant())
      : Location::RequiresRegister();
}

Location LocationsBuilderMIPS::FpuRegisterOrConstantForStore(HInstruction* instruction) {
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

void LocationsBuilderMIPS::VisitArraySet(HArraySet* instruction) {
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

void InstructionCodeGeneratorMIPS::VisitArraySet(HArraySet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location index = locations->InAt(1);
  Location value_location = locations->InAt(2);
  DataType::Type value_type = instruction->GetComponentType();
  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());
  auto null_checker = GetImplicitNullChecker(instruction, codegen_);
  Register base_reg = index.IsConstant() ? obj : TMP;

  switch (value_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint8_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_1;
      } else {
        __ Addu(base_reg, obj, index.AsRegister<Register>());
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreByte, value, base_reg, data_offset, TMP, null_checker);
      } else {
        Register value = value_location.AsRegister<Register>();
        __ StoreToOffset(kStoreByte, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kUint16:
    case DataType::Type::kInt16: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(uint16_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_2;
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(base_reg, index.AsRegister<Register>(), obj);
      } else {
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_2, base_reg);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreHalfword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        Register value = value_location.AsRegister<Register>();
        __ StoreToOffset(kStoreHalfword, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kInt32: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(base_reg, index.AsRegister<Register>(), obj);
      } else {
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_4, base_reg);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
      } else {
        Register value = value_location.AsRegister<Register>();
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
          __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_4, base_reg);
        }
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        DCHECK_EQ(value, 0);
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
        DCHECK(!needs_write_barrier);
        DCHECK(!may_need_runtime_call_for_type_check);
        break;
      }

      DCHECK(needs_write_barrier);
      Register value = value_location.AsRegister<Register>();
      Register temp1 = locations->GetTemp(0).AsRegister<Register>();
      Register temp2 = TMP;  // Doesn't need to survive slow path.
      uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
      uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
      uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
      MipsLabel done;
      SlowPathCodeMIPS* slow_path = nullptr;

      if (may_need_runtime_call_for_type_check) {
        slow_path = new (codegen_->GetScopedAllocator()) ArraySetSlowPathMIPS(instruction);
        codegen_->AddSlowPath(slow_path);
        if (instruction->GetValueCanBeNull()) {
          MipsLabel non_zero;
          __ Bnez(value, &non_zero);
          uint32_t data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Uint32Value();
          if (index.IsConstant()) {
            data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
          } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
            __ Addu(base_reg, index.AsRegister<Register>(), obj);
          } else {
            __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_4, base_reg);
          }
          __ StoreToOffset(kStoreWord, value, base_reg, data_offset, null_checker);
          __ B(&done);
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
        __ LoadFromOffset(kLoadWord, temp1, obj, class_offset, null_checker);
        __ MaybeUnpoisonHeapReference(temp1);

        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        __ LoadFromOffset(kLoadWord, temp1, temp1, component_offset);
        // /* HeapReference<Class> */ temp2 = value->klass_
        __ LoadFromOffset(kLoadWord, temp2, value, class_offset);
        // If heap poisoning is enabled, no need to unpoison `temp1`
        // nor `temp2`, as we are comparing two poisoned references.

        if (instruction->StaticTypeOfArrayIsObjectArray()) {
          MipsLabel do_put;
          __ Beq(temp1, temp2, &do_put);
          // If heap poisoning is enabled, the `temp1` reference has
          // not been unpoisoned yet; unpoison it now.
          __ MaybeUnpoisonHeapReference(temp1);

          // /* HeapReference<Class> */ temp1 = temp1->super_class_
          __ LoadFromOffset(kLoadWord, temp1, temp1, super_offset);
          // If heap poisoning is enabled, no need to unpoison
          // `temp1`, as we are comparing against null below.
          __ Bnez(temp1, slow_path->GetEntryLabel());
          __ Bind(&do_put);
        } else {
          __ Bne(temp1, temp2, slow_path->GetEntryLabel());
        }
      }

      Register source = value;
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
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_4, base_reg);
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
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(base_reg, index.AsRegister<Register>(), obj);
      } else {
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_8, base_reg);
      }
      if (value_location.IsConstant()) {
        int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreDoubleword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        Register value = value_location.AsRegisterPairLow<Register>();
        __ StoreToOffset(kStoreDoubleword, value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(float)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_4;
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(base_reg, index.AsRegister<Register>(), obj);
      } else {
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_4, base_reg);
      }
      if (value_location.IsConstant()) {
        int32_t value = CodeGenerator::GetInt32ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreWord, value, base_reg, data_offset, TMP, null_checker);
      } else {
        FRegister value = value_location.AsFpuRegister<FRegister>();
        __ StoreSToOffset(value, base_reg, data_offset, null_checker);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      uint32_t data_offset = mirror::Array::DataOffset(sizeof(double)).Uint32Value();
      if (index.IsConstant()) {
        data_offset += index.GetConstant()->AsIntConstant()->GetValue() << TIMES_8;
      } else if (instruction->InputAt(1)->IsIntermediateArrayAddressIndex()) {
        __ Addu(base_reg, index.AsRegister<Register>(), obj);
      } else {
        __ ShiftAndAdd(base_reg, index.AsRegister<Register>(), obj, TIMES_8, base_reg);
      }
      if (value_location.IsConstant()) {
        int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
        __ StoreConstToOffset(kStoreDoubleword, value, base_reg, data_offset, TMP, null_checker);
      } else {
        FRegister value = value_location.AsFpuRegister<FRegister>();
        __ StoreDToOffset(value, base_reg, data_offset, null_checker);
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

void LocationsBuilderMIPS::VisitIntermediateArrayAddressIndex(
    HIntermediateArrayAddressIndex* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);

  HIntConstant* shift = instruction->GetShift()->AsIntConstant();

  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::ConstantLocation(shift));
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitIntermediateArrayAddressIndex(
    HIntermediateArrayAddressIndex* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register index_reg = locations->InAt(0).AsRegister<Register>();
  uint32_t shift = instruction->GetShift()->AsIntConstant()->GetValue();
  __ Sll(locations->Out().AsRegister<Register>(), index_reg, shift);
}

void LocationsBuilderMIPS::VisitBoundsCheck(HBoundsCheck* instruction) {
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

void InstructionCodeGeneratorMIPS::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location index_loc = locations->InAt(0);
  Location length_loc = locations->InAt(1);

  if (length_loc.IsConstant()) {
    int32_t length = length_loc.GetConstant()->AsIntConstant()->GetValue();
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0 || index >= length) {
        BoundsCheckSlowPathMIPS* slow_path =
            new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS(instruction);
        codegen_->AddSlowPath(slow_path);
        __ B(slow_path->GetEntryLabel());
      } else {
        // Nothing to be done.
      }
      return;
    }

    BoundsCheckSlowPathMIPS* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS(instruction);
    codegen_->AddSlowPath(slow_path);
    Register index = index_loc.AsRegister<Register>();
    if (length == 0) {
      __ B(slow_path->GetEntryLabel());
    } else if (length == 1) {
      __ Bnez(index, slow_path->GetEntryLabel());
    } else {
      DCHECK(IsUint<15>(length)) << length;
      __ Sltiu(TMP, index, length);
      __ Beqz(TMP, slow_path->GetEntryLabel());
    }
  } else {
    Register length = length_loc.AsRegister<Register>();
    BoundsCheckSlowPathMIPS* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathMIPS(instruction);
    codegen_->AddSlowPath(slow_path);
    if (index_loc.IsConstant()) {
      int32_t index = index_loc.GetConstant()->AsIntConstant()->GetValue();
      if (index < 0) {
        __ B(slow_path->GetEntryLabel());
      } else if (index == 0) {
        __ Blez(length, slow_path->GetEntryLabel());
      } else {
        DCHECK(IsInt<16>(index + 1)) << index;
        __ Sltiu(TMP, length, index + 1);
        __ Bnez(TMP, slow_path->GetEntryLabel());
      }
    } else {
      Register index = index_loc.AsRegister<Register>();
      __ Bgeu(index, length, slow_path->GetEntryLabel());
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

void LocationsBuilderMIPS::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary::CallKind call_kind = CodeGenerator::GetCheckCastCallKind(instruction);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->AddRegisterTemps(NumberOfCheckCastTemps(type_check_kind));
}

void InstructionCodeGeneratorMIPS::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = obj_loc.AsRegister<Register>();
  Register cls = locations->InAt(1).AsRegister<Register>();
  Location temp_loc = locations->GetTemp(0);
  Register temp = temp_loc.AsRegister<Register>();
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
  MipsLabel done;

  bool is_type_check_slow_path_fatal = CodeGenerator::IsTypeCheckSlowPathFatal(instruction);
  SlowPathCodeMIPS* slow_path =
      new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS(
          instruction, is_type_check_slow_path_fatal);
  codegen_->AddSlowPath(slow_path);

  // Avoid this check if we know `obj` is not null.
  if (instruction->MustDoNullCheck()) {
    __ Beqz(obj, &done);
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
      __ Bne(temp, cls, slow_path->GetEntryLabel());
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
      MipsLabel loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception.
      __ Beqz(temp, slow_path->GetEntryLabel());
      // Otherwise, compare the classes.
      __ Bne(temp, cls, &loop);
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
      MipsLabel loop;
      __ Bind(&loop);
      __ Beq(temp, cls, &done);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception. Otherwise, jump to the beginning of the loop.
      __ Bnez(temp, &loop);
      __ B(slow_path->GetEntryLabel());
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
      __ Beq(temp, cls, &done);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ temp = temp->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       component_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the component type is null, jump to the slow path to throw the exception.
      __ Beqz(temp, slow_path->GetEntryLabel());
      // Otherwise, the object is indeed an array, further check that this component
      // type is not a primitive type.
      __ LoadFromOffset(kLoadUnsignedHalfword, temp, temp, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Bnez(temp, slow_path->GetEntryLabel());
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
      __ B(slow_path->GetEntryLabel());
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
      MipsLabel loop;
      __ Bind(&loop);
      __ Addiu(temp, temp, 2 * kHeapReferenceSize);  // Possibly in delay slot on R2.
      __ Beqz(TMP, slow_path->GetEntryLabel());
      __ Lw(AT, temp, object_array_data_offset - 2 * kHeapReferenceSize);
      __ MaybeUnpoisonHeapReference(AT);
      // Go to next interface.
      __ Addiu(TMP, TMP, -2);
      // Compare the classes and continue the loop if they do not match.
      __ Bne(AT, cls, &loop);
      break;
    }
  }

  __ Bind(&done);
  __ Bind(slow_path->GetExitLabel());
}

void LocationsBuilderMIPS::VisitClinitCheck(HClinitCheck* check) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(check, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (check->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorMIPS::VisitClinitCheck(HClinitCheck* check) {
  // We assume the class is not null.
  SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathMIPS(
      check->GetLoadClass(),
      check,
      check->GetDexPc(),
      true);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path,
                                   check->GetLocations()->InAt(0).AsRegister<Register>());
}

void LocationsBuilderMIPS::VisitCompare(HCompare* compare) {
  DataType::Type in_type = compare->InputAt(0)->GetType();

  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(compare, LocationSummary::kNoCall);

  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      // Output overlaps because it is written before doing the low comparison.
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
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

void InstructionCodeGeneratorMIPS::VisitCompare(HCompare* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Register res = locations->Out().AsRegister<Register>();
  DataType::Type in_type = instruction->InputAt(0)->GetType();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  //  0 if: left == right
  //  1 if: left  > right
  // -1 if: left  < right
  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      Register lhs = locations->InAt(0).AsRegister<Register>();
      Register rhs = locations->InAt(1).AsRegister<Register>();
      __ Slt(TMP, lhs, rhs);
      __ Slt(res, rhs, lhs);
      __ Subu(res, res, TMP);
      break;
    }
    case DataType::Type::kInt64: {
      MipsLabel done;
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register lhs_low  = locations->InAt(0).AsRegisterPairLow<Register>();
      Register rhs_high = locations->InAt(1).AsRegisterPairHigh<Register>();
      Register rhs_low  = locations->InAt(1).AsRegisterPairLow<Register>();
      // TODO: more efficient (direct) comparison with a constant.
      __ Slt(TMP, lhs_high, rhs_high);
      __ Slt(AT, rhs_high, lhs_high);  // Inverted: is actually gt.
      __ Subu(res, AT, TMP);           // Result -1:1:0 for [ <, >, == ].
      __ Bnez(res, &done);             // If we compared ==, check if lower bits are also equal.
      __ Sltu(TMP, lhs_low, rhs_low);
      __ Sltu(AT, rhs_low, lhs_low);   // Inverted: is actually gt.
      __ Subu(res, AT, TMP);           // Result -1:1:0 for [ <, >, == ].
      __ Bind(&done);
      break;
    }

    case DataType::Type::kFloat32: {
      bool gt_bias = instruction->IsGtBias();
      FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
      MipsLabel done;
      if (isR6) {
        __ CmpEqS(FTMP, lhs, rhs);
        __ LoadConst32(res, 0);
        __ Bc1nez(FTMP, &done);
        if (gt_bias) {
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
      } else {
        if (gt_bias) {
          __ ColtS(0, lhs, rhs);
          __ LoadConst32(res, -1);
          __ Bc1t(0, &done);
          __ CeqS(0, lhs, rhs);
          __ LoadConst32(res, 1);
          __ Movt(res, ZERO, 0);
        } else {
          __ ColtS(0, rhs, lhs);
          __ LoadConst32(res, 1);
          __ Bc1t(0, &done);
          __ CeqS(0, lhs, rhs);
          __ LoadConst32(res, -1);
          __ Movt(res, ZERO, 0);
        }
      }
      __ Bind(&done);
      break;
    }
    case DataType::Type::kFloat64: {
      bool gt_bias = instruction->IsGtBias();
      FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
      MipsLabel done;
      if (isR6) {
        __ CmpEqD(FTMP, lhs, rhs);
        __ LoadConst32(res, 0);
        __ Bc1nez(FTMP, &done);
        if (gt_bias) {
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
      } else {
        if (gt_bias) {
          __ ColtD(0, lhs, rhs);
          __ LoadConst32(res, -1);
          __ Bc1t(0, &done);
          __ CeqD(0, lhs, rhs);
          __ LoadConst32(res, 1);
          __ Movt(res, ZERO, 0);
        } else {
          __ ColtD(0, rhs, lhs);
          __ LoadConst32(res, 1);
          __ Bc1t(0, &done);
          __ CeqD(0, lhs, rhs);
          __ LoadConst32(res, -1);
          __ Movt(res, ZERO, 0);
        }
      }
      __ Bind(&done);
      break;
    }

    default:
      LOG(FATAL) << "Unimplemented compare type " << in_type;
  }
}

void LocationsBuilderMIPS::HandleCondition(HCondition* instruction) {
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

void InstructionCodeGeneratorMIPS::HandleCondition(HCondition* instruction) {
  if (instruction->IsEmittedAtUseSite()) {
    return;
  }

  DataType::Type type = instruction->InputAt(0)->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    default:
      // Integer case.
      GenerateIntCompare(instruction->GetCondition(), locations);
      return;

    case DataType::Type::kInt64:
      GenerateLongCompare(instruction->GetCondition(), locations);
      return;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      GenerateFpCompare(instruction->GetCondition(), instruction->IsGtBias(), type, locations);
      return;
  }
}

void InstructionCodeGeneratorMIPS::DivRemOneOrMinusOne(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());
  int64_t imm = Int64FromConstant(second.GetConstant());
  DCHECK(imm == 1 || imm == -1);

  if (instruction->GetResultType() == DataType::Type::kInt32) {
    Register out = locations->Out().AsRegister<Register>();
    Register dividend = locations->InAt(0).AsRegister<Register>();

    if (instruction->IsRem()) {
      __ Move(out, ZERO);
    } else {
      if (imm == -1) {
        __ Subu(out, ZERO, dividend);
      } else if (out != dividend) {
        __ Move(out, dividend);
      }
    }
  } else {
    DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt64);
    Register out_high = locations->Out().AsRegisterPairHigh<Register>();
    Register out_low = locations->Out().AsRegisterPairLow<Register>();
    Register in_high = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register in_low = locations->InAt(0).AsRegisterPairLow<Register>();

    if (instruction->IsRem()) {
      __ Move(out_high, ZERO);
      __ Move(out_low, ZERO);
    } else {
      if (imm == -1) {
        __ Subu(out_low, ZERO, in_low);
        __ Sltu(AT, ZERO, out_low);
        __ Subu(out_high, ZERO, in_high);
        __ Subu(out_high, out_high, AT);
      } else {
        __ Move(out_low, in_low);
        __ Move(out_high, in_high);
      }
    }
  }
}

void InstructionCodeGeneratorMIPS::DivRemByPowerOfTwo(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  const bool is_r2_or_newer = codegen_->GetInstructionSetFeatures().IsMipsIsaRevGreaterThanEqual2();
  const bool is_r6 = codegen_->GetInstructionSetFeatures().IsR6();
  DCHECK(second.IsConstant());

  if (instruction->GetResultType() == DataType::Type::kInt32) {
    Register out = locations->Out().AsRegister<Register>();
    Register dividend = locations->InAt(0).AsRegister<Register>();
    int32_t imm = second.GetConstant()->AsIntConstant()->GetValue();
    uint32_t abs_imm = static_cast<uint32_t>(AbsOrMin(imm));
    int ctz_imm = CTZ(abs_imm);

    if (instruction->IsDiv()) {
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
        if (IsUint<16>(abs_imm - 1)) {
          __ Andi(out, out, abs_imm - 1);
        } else {
          if (is_r2_or_newer) {
            __ Ins(out, ZERO, ctz_imm, 32 - ctz_imm);
          } else {
            __ Sll(out, out, 32 - ctz_imm);
            __ Srl(out, out, 32 - ctz_imm);
          }
        }
        __ Subu(out, out, TMP);
      }
    }
  } else {
    DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt64);
    Register out_high = locations->Out().AsRegisterPairHigh<Register>();
    Register out_low = locations->Out().AsRegisterPairLow<Register>();
    Register in_high = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register in_low = locations->InAt(0).AsRegisterPairLow<Register>();
    int64_t imm = Int64FromConstant(second.GetConstant());
    uint64_t abs_imm = static_cast<uint64_t>(AbsOrMin(imm));
    int ctz_imm = CTZ(abs_imm);

    if (instruction->IsDiv()) {
      if (ctz_imm < 32) {
        if (ctz_imm == 1) {
          __ Srl(AT, in_high, 31);
        } else {
          __ Sra(AT, in_high, 31);
          __ Srl(AT, AT, 32 - ctz_imm);
        }
        __ Addu(AT, AT, in_low);
        __ Sltu(TMP, AT, in_low);
        __ Addu(out_high, in_high, TMP);
        __ Srl(out_low, AT, ctz_imm);
        if (is_r2_or_newer) {
          __ Ins(out_low, out_high, 32 - ctz_imm, ctz_imm);
          __ Sra(out_high, out_high, ctz_imm);
        } else {
          __ Sll(AT, out_high, 32 - ctz_imm);
          __ Sra(out_high, out_high, ctz_imm);
          __ Or(out_low, out_low, AT);
        }
        if (imm < 0) {
          __ Subu(out_low, ZERO, out_low);
          __ Sltu(AT, ZERO, out_low);
          __ Subu(out_high, ZERO, out_high);
          __ Subu(out_high, out_high, AT);
        }
      } else if (ctz_imm == 32) {
        __ Sra(AT, in_high, 31);
        __ Addu(AT, AT, in_low);
        __ Sltu(AT, AT, in_low);
        __ Addu(out_low, in_high, AT);
        if (imm < 0) {
          __ Srl(TMP, out_low, 31);
          __ Subu(out_low, ZERO, out_low);
          __ Sltu(AT, ZERO, out_low);
          __ Subu(out_high, TMP, AT);
        } else {
          __ Sra(out_high, out_low, 31);
        }
      } else if (ctz_imm < 63) {
        __ Sra(AT, in_high, 31);
        __ Srl(TMP, AT, 64 - ctz_imm);
        __ Addu(AT, AT, in_low);
        __ Sltu(AT, AT, in_low);
        __ Addu(out_low, in_high, AT);
        __ Addu(out_low, out_low, TMP);
        __ Sra(out_low, out_low, ctz_imm - 32);
        if (imm < 0) {
          __ Subu(out_low, ZERO, out_low);
        }
        __ Sra(out_high, out_low, 31);
      } else {
        DCHECK_LT(imm, 0);
        if (is_r6) {
          __ Aui(AT, in_high, 0x8000);
        } else {
          __ Lui(AT, 0x8000);
          __ Xor(AT, AT, in_high);
        }
        __ Or(AT, AT, in_low);
        __ Sltiu(out_low, AT, 1);
        __ Move(out_high, ZERO);
      }
    } else {
      if ((ctz_imm == 1) && !is_r6) {
        __ Andi(AT, in_low, 1);
        __ Sll(TMP, in_low, 31);
        __ And(TMP, in_high, TMP);
        __ Sra(out_high, TMP, 31);
        __ Or(out_low, out_high, AT);
      } else if (ctz_imm < 32) {
        __ Sra(AT, in_high, 31);
        if (ctz_imm <= 16) {
          __ Andi(out_low, in_low, abs_imm - 1);
        } else if (is_r2_or_newer) {
          __ Ext(out_low, in_low, 0, ctz_imm);
        } else {
          __ Sll(out_low, in_low, 32 - ctz_imm);
          __ Srl(out_low, out_low, 32 - ctz_imm);
        }
        if (is_r6) {
          __ Selnez(out_high, AT, out_low);
        } else {
          __ Movz(AT, ZERO, out_low);
          __ Move(out_high, AT);
        }
        if (is_r2_or_newer) {
          __ Ins(out_low, out_high, ctz_imm, 32 - ctz_imm);
        } else {
          __ Sll(AT, out_high, ctz_imm);
          __ Or(out_low, out_low, AT);
        }
      } else if (ctz_imm == 32) {
        __ Sra(AT, in_high, 31);
        __ Move(out_low, in_low);
        if (is_r6) {
          __ Selnez(out_high, AT, out_low);
        } else {
          __ Movz(AT, ZERO, out_low);
          __ Move(out_high, AT);
        }
      } else if (ctz_imm < 63) {
        __ Sra(AT, in_high, 31);
        __ Move(TMP, in_low);
        if (ctz_imm - 32 <= 16) {
          __ Andi(out_high, in_high, (1 << (ctz_imm - 32)) - 1);
        } else if (is_r2_or_newer) {
          __ Ext(out_high, in_high, 0, ctz_imm - 32);
        } else {
          __ Sll(out_high, in_high, 64 - ctz_imm);
          __ Srl(out_high, out_high, 64 - ctz_imm);
        }
        __ Move(out_low, TMP);
        __ Or(TMP, TMP, out_high);
        if (is_r6) {
          __ Selnez(AT, AT, TMP);
        } else {
          __ Movz(AT, ZERO, TMP);
        }
        if (is_r2_or_newer) {
          __ Ins(out_high, AT, ctz_imm - 32, 64 - ctz_imm);
        } else {
          __ Sll(AT, AT, ctz_imm - 32);
          __ Or(out_high, out_high, AT);
        }
      } else {
        if (is_r6) {
          __ Aui(AT, in_high, 0x8000);
        } else {
          __ Lui(AT, 0x8000);
          __ Xor(AT, AT, in_high);
        }
        __ Or(AT, AT, in_low);
        __ Sltiu(AT, AT, 1);
        __ Sll(AT, AT, 31);
        __ Move(out_low, in_low);
        __ Xor(out_high, in_high, AT);
      }
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateDivRemWithAnyConstant(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt32);

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  Register out = locations->Out().AsRegister<Register>();
  Register dividend = locations->InAt(0).AsRegister<Register>();
  int32_t imm = second.GetConstant()->AsIntConstant()->GetValue();

  int64_t magic;
  int shift;
  CalculateMagicAndShiftForDivRem(imm, false /* is_long */, &magic, &shift);

  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  __ LoadConst32(TMP, magic);
  if (isR6) {
    __ MuhR6(TMP, dividend, TMP);
  } else {
    __ MultR2(dividend, TMP);
    __ Mfhi(TMP);
  }
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
    if (isR6) {
      __ MulR6(TMP, AT, TMP);
    } else {
      __ MulR2(TMP, AT, TMP);
    }
    __ Subu(out, dividend, TMP);
  }
}

void InstructionCodeGeneratorMIPS::GenerateDivRemIntegral(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt32);

  LocationSummary* locations = instruction->GetLocations();
  Register out = locations->Out().AsRegister<Register>();
  Location second = locations->InAt(1);

  if (second.IsConstant()) {
    int32_t imm = second.GetConstant()->AsIntConstant()->GetValue();
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
    Register dividend = locations->InAt(0).AsRegister<Register>();
    Register divisor = second.AsRegister<Register>();
    bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
    if (instruction->IsDiv()) {
      if (isR6) {
        __ DivR6(out, dividend, divisor);
      } else {
        __ DivR2(out, dividend, divisor);
      }
    } else {
      if (isR6) {
        __ ModR6(out, dividend, divisor);
      } else {
        __ ModR2(out, dividend, divisor);
      }
    }
  }
}

void LocationsBuilderMIPS::VisitDiv(HDiv* div) {
  DataType::Type type = div->GetResultType();
  bool call_long_div = false;
  if (type == DataType::Type::kInt64) {
    if (div->InputAt(1)->IsConstant()) {
      int64_t imm = CodeGenerator::GetInt64ValueOf(div->InputAt(1)->AsConstant());
      call_long_div = (imm != 0) && !IsPowerOfTwo(static_cast<uint64_t>(AbsOrMin(imm)));
    } else {
      call_long_div = true;
    }
  }
  LocationSummary::CallKind call_kind = call_long_div
      ? LocationSummary::kCallOnMainOnly
      : LocationSummary::kNoCall;

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(div, call_kind);

  switch (type) {
    case DataType::Type::kInt32:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(div->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kInt64: {
      if (call_long_div) {
        InvokeRuntimeCallingConvention calling_convention;
        locations->SetInAt(0, Location::RegisterPairLocation(
            calling_convention.GetRegisterAt(0), calling_convention.GetRegisterAt(1)));
        locations->SetInAt(1, Location::RegisterPairLocation(
            calling_convention.GetRegisterAt(2), calling_convention.GetRegisterAt(3)));
        locations->SetOut(calling_convention.GetReturnLocation(type));
      } else {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::ConstantLocation(div->InputAt(1)->AsConstant()));
        locations->SetOut(Location::RequiresRegister());
      }
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << type;
  }
}

void InstructionCodeGeneratorMIPS::VisitDiv(HDiv* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
      GenerateDivRemIntegral(instruction);
      break;
    case DataType::Type::kInt64: {
      if (locations->InAt(1).IsConstant()) {
        int64_t imm = locations->InAt(1).GetConstant()->AsLongConstant()->GetValue();
        if (imm == 0) {
          // Do not generate anything. DivZeroCheck would prevent any code to be executed.
        } else if (imm == 1 || imm == -1) {
          DivRemOneOrMinusOne(instruction);
        } else {
          DCHECK(IsPowerOfTwo(static_cast<uint64_t>(AbsOrMin(imm))));
          DivRemByPowerOfTwo(instruction);
        }
      } else {
        codegen_->InvokeRuntime(kQuickLdiv, instruction, instruction->GetDexPc());
        CheckEntrypointTypes<kQuickLdiv, int64_t, int64_t, int64_t>();
      }
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FRegister dst = locations->Out().AsFpuRegister<FRegister>();
      FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ DivS(dst, lhs, rhs);
      } else {
        __ DivD(dst, lhs, rhs);
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected div type " << type;
  }
}

void LocationsBuilderMIPS::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RegisterOrConstant(instruction->InputAt(0)));
}

void InstructionCodeGeneratorMIPS::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  SlowPathCodeMIPS* slow_path =
      new (codegen_->GetScopedAllocator()) DivZeroCheckSlowPathMIPS(instruction);
  codegen_->AddSlowPath(slow_path);
  Location value = instruction->GetLocations()->InAt(0);
  DataType::Type type = instruction->GetType();

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      if (value.IsConstant()) {
        if (value.GetConstant()->AsIntConstant()->GetValue() == 0) {
          __ B(slow_path->GetEntryLabel());
        } else {
          // A division by a non-null constant is valid. We don't need to perform
          // any check, so simply fall through.
        }
      } else {
        DCHECK(value.IsRegister()) << value;
        __ Beqz(value.AsRegister<Register>(), slow_path->GetEntryLabel());
      }
      break;
    }
    case DataType::Type::kInt64: {
      if (value.IsConstant()) {
        if (value.GetConstant()->AsLongConstant()->GetValue() == 0) {
          __ B(slow_path->GetEntryLabel());
        } else {
          // A division by a non-null constant is valid. We don't need to perform
          // any check, so simply fall through.
        }
      } else {
        DCHECK(value.IsRegisterPair()) << value;
        __ Or(TMP, value.AsRegisterPairHigh<Register>(), value.AsRegisterPairLow<Register>());
        __ Beqz(TMP, slow_path->GetEntryLabel());
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected type " << type << " for DivZeroCheck.";
  }
}

void LocationsBuilderMIPS::VisitDoubleConstant(HDoubleConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitDoubleConstant(HDoubleConstant* cst ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitExit(HExit* exit ATTRIBUTE_UNUSED) {
}

void LocationsBuilderMIPS::VisitFloatConstant(HFloatConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitFloatConstant(HFloatConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::HandleGoto(HInstruction* got, HBasicBlock* successor) {
  if (successor->IsExitBlock()) {
    DCHECK(got->GetPrevious()->AlwaysThrows());
    return;  // no code needed
  }

  HBasicBlock* block = got->GetBlock();
  HInstruction* previous = got->GetPrevious();
  HLoopInformation* info = block->GetLoopInformation();

  if (info != nullptr && info->IsBackEdge(*block) && info->HasSuspendCheck()) {
    if (codegen_->GetCompilerOptions().CountHotnessInCompiledCode()) {
      __ Lw(AT, SP, kCurrentMethodStackOffset);
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
    __ B(codegen_->GetLabelOf(successor));
  }
}

void InstructionCodeGeneratorMIPS::VisitGoto(HGoto* got) {
  HandleGoto(got, got->GetSuccessor());
}

void LocationsBuilderMIPS::VisitTryBoundary(HTryBoundary* try_boundary) {
  try_boundary->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitTryBoundary(HTryBoundary* try_boundary) {
  HBasicBlock* successor = try_boundary->GetNormalFlowSuccessor();
  if (!successor->IsExitBlock()) {
    HandleGoto(try_boundary, successor);
  }
}

void InstructionCodeGeneratorMIPS::GenerateIntCompare(IfCondition cond,
                                                      LocationSummary* locations) {
  Register dst = locations->Out().AsRegister<Register>();
  Register lhs = locations->InAt(0).AsRegister<Register>();
  Location rhs_location = locations->InAt(1);
  Register rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
  } else {
    rhs_reg = rhs_location.AsRegister<Register>();
  }

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
          __ Addiu(dst, lhs, -rhs_imm);
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
            __ LoadConst32(rhs_reg, rhs_imm);
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
          __ LoadConst32(rhs_reg, rhs_imm);
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
      if (use_imm && IsInt<16>(rhs_imm + 1)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        __ Slti(dst, lhs, rhs_imm + 1);
        if (cond == kCondGT) {
          // Simulate lhs > rhs via !(lhs <= rhs) since there's
          // only the slti instruction but no sgti.
          __ Xori(dst, dst, 1);
        }
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
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
        // [0xffff8000, 0xffffffff].
        __ Sltiu(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
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
      if (use_imm && (rhs_imm != -1) && IsInt<16>(rhs_imm + 1)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        // Note that this only works if rhs + 1 does not overflow
        // to 0, hence the check above.
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0xffff8000, 0xffffffff].
        __ Sltiu(dst, lhs, rhs_imm + 1);
        if (cond == kCondA) {
          // Simulate lhs > rhs via !(lhs <= rhs) since there's
          // only the sltiu instruction but no sgtiu.
          __ Xori(dst, dst, 1);
        }
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
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

bool InstructionCodeGeneratorMIPS::MaterializeIntCompare(IfCondition cond,
                                                         LocationSummary* input_locations,
                                                         Register dst) {
  Register lhs = input_locations->InAt(0).AsRegister<Register>();
  Location rhs_location = input_locations->InAt(1);
  Register rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
  } else {
    rhs_reg = rhs_location.AsRegister<Register>();
  }

  switch (cond) {
    case kCondEQ:
    case kCondNE:
      if (use_imm && IsInt<16>(-rhs_imm)) {
        __ Addiu(dst, lhs, -rhs_imm);
      } else if (use_imm && IsUint<16>(rhs_imm)) {
        __ Xori(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
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
          __ LoadConst32(rhs_reg, rhs_imm);
        }
        __ Slt(dst, lhs, rhs_reg);
      }
      return (cond == kCondGE);

    case kCondLE:
    case kCondGT:
      if (use_imm && IsInt<16>(rhs_imm + 1)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        __ Slti(dst, lhs, rhs_imm + 1);
        return (cond == kCondGT);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
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
        // [0xffff8000, 0xffffffff].
        __ Sltiu(dst, lhs, rhs_imm);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, lhs, rhs_reg);
      }
      return (cond == kCondAE);

    case kCondBE:
    case kCondA:
      if (use_imm && (rhs_imm != -1) && IsInt<16>(rhs_imm + 1)) {
        // Simulate lhs <= rhs via lhs < rhs + 1.
        // Note that this only works if rhs + 1 does not overflow
        // to 0, hence the check above.
        // Sltiu sign-extends its 16-bit immediate operand before
        // the comparison and thus lets us compare directly with
        // unsigned values in the ranges [0, 0x7fff] and
        // [0xffff8000, 0xffffffff].
        __ Sltiu(dst, lhs, rhs_imm + 1);
        return (cond == kCondA);
      } else {
        if (use_imm) {
          rhs_reg = TMP;
          __ LoadConst32(rhs_reg, rhs_imm);
        }
        __ Sltu(dst, rhs_reg, lhs);
        return (cond == kCondBE);
      }
  }
}

void InstructionCodeGeneratorMIPS::GenerateIntCompareAndBranch(IfCondition cond,
                                                               LocationSummary* locations,
                                                               MipsLabel* label) {
  Register lhs = locations->InAt(0).AsRegister<Register>();
  Location rhs_location = locations->InAt(1);
  Register rhs_reg = ZERO;
  int64_t rhs_imm = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    rhs_imm = CodeGenerator::GetInt32ValueOf(rhs_location.GetConstant());
  } else {
    rhs_reg = rhs_location.AsRegister<Register>();
  }

  if (use_imm && rhs_imm == 0) {
    switch (cond) {
      case kCondEQ:
      case kCondBE:  // <= 0 if zero
        __ Beqz(lhs, label);
        break;
      case kCondNE:
      case kCondA:  // > 0 if non-zero
        __ Bnez(lhs, label);
        break;
      case kCondLT:
        __ Bltz(lhs, label);
        break;
      case kCondGE:
        __ Bgez(lhs, label);
        break;
      case kCondLE:
        __ Blez(lhs, label);
        break;
      case kCondGT:
        __ Bgtz(lhs, label);
        break;
      case kCondB:  // always false
        break;
      case kCondAE:  // always true
        __ B(label);
        break;
    }
  } else {
    bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
    if (isR6 || !use_imm) {
      if (use_imm) {
        rhs_reg = TMP;
        __ LoadConst32(rhs_reg, rhs_imm);
      }
      switch (cond) {
        case kCondEQ:
          __ Beq(lhs, rhs_reg, label);
          break;
        case kCondNE:
          __ Bne(lhs, rhs_reg, label);
          break;
        case kCondLT:
          __ Blt(lhs, rhs_reg, label);
          break;
        case kCondGE:
          __ Bge(lhs, rhs_reg, label);
          break;
        case kCondLE:
          __ Bge(rhs_reg, lhs, label);
          break;
        case kCondGT:
          __ Blt(rhs_reg, lhs, label);
          break;
        case kCondB:
          __ Bltu(lhs, rhs_reg, label);
          break;
        case kCondAE:
          __ Bgeu(lhs, rhs_reg, label);
          break;
        case kCondBE:
          __ Bgeu(rhs_reg, lhs, label);
          break;
        case kCondA:
          __ Bltu(rhs_reg, lhs, label);
          break;
      }
    } else {
      // Special cases for more efficient comparison with constants on R2.
      switch (cond) {
        case kCondEQ:
          __ LoadConst32(TMP, rhs_imm);
          __ Beq(lhs, TMP, label);
          break;
        case kCondNE:
          __ LoadConst32(TMP, rhs_imm);
          __ Bne(lhs, TMP, label);
          break;
        case kCondLT:
          if (IsInt<16>(rhs_imm)) {
            __ Slti(TMP, lhs, rhs_imm);
            __ Bnez(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Blt(lhs, TMP, label);
          }
          break;
        case kCondGE:
          if (IsInt<16>(rhs_imm)) {
            __ Slti(TMP, lhs, rhs_imm);
            __ Beqz(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bge(lhs, TMP, label);
          }
          break;
        case kCondLE:
          if (IsInt<16>(rhs_imm + 1)) {
            // Simulate lhs <= rhs via lhs < rhs + 1.
            __ Slti(TMP, lhs, rhs_imm + 1);
            __ Bnez(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bge(TMP, lhs, label);
          }
          break;
        case kCondGT:
          if (IsInt<16>(rhs_imm + 1)) {
            // Simulate lhs > rhs via !(lhs < rhs + 1).
            __ Slti(TMP, lhs, rhs_imm + 1);
            __ Beqz(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Blt(TMP, lhs, label);
          }
          break;
        case kCondB:
          if (IsInt<16>(rhs_imm)) {
            __ Sltiu(TMP, lhs, rhs_imm);
            __ Bnez(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bltu(lhs, TMP, label);
          }
          break;
        case kCondAE:
          if (IsInt<16>(rhs_imm)) {
            __ Sltiu(TMP, lhs, rhs_imm);
            __ Beqz(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bgeu(lhs, TMP, label);
          }
          break;
        case kCondBE:
          if ((rhs_imm != -1) && IsInt<16>(rhs_imm + 1)) {
            // Simulate lhs <= rhs via lhs < rhs + 1.
            // Note that this only works if rhs + 1 does not overflow
            // to 0, hence the check above.
            __ Sltiu(TMP, lhs, rhs_imm + 1);
            __ Bnez(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bgeu(TMP, lhs, label);
          }
          break;
        case kCondA:
          if ((rhs_imm != -1) && IsInt<16>(rhs_imm + 1)) {
            // Simulate lhs > rhs via !(lhs < rhs + 1).
            // Note that this only works if rhs + 1 does not overflow
            // to 0, hence the check above.
            __ Sltiu(TMP, lhs, rhs_imm + 1);
            __ Beqz(TMP, label);
          } else {
            __ LoadConst32(TMP, rhs_imm);
            __ Bltu(TMP, lhs, label);
          }
          break;
      }
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateLongCompare(IfCondition cond,
                                                       LocationSummary* locations) {
  Register dst = locations->Out().AsRegister<Register>();
  Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
  Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
  Location rhs_location = locations->InAt(1);
  Register rhs_high = ZERO;
  Register rhs_low = ZERO;
  int64_t imm = 0;
  uint32_t imm_high = 0;
  uint32_t imm_low = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    imm = rhs_location.GetConstant()->AsLongConstant()->GetValue();
    imm_high = High32Bits(imm);
    imm_low = Low32Bits(imm);
  } else {
    rhs_high = rhs_location.AsRegisterPairHigh<Register>();
    rhs_low = rhs_location.AsRegisterPairLow<Register>();
  }
  if (use_imm && imm == 0) {
    switch (cond) {
      case kCondEQ:
      case kCondBE:  // <= 0 if zero
        __ Or(dst, lhs_high, lhs_low);
        __ Sltiu(dst, dst, 1);
        break;
      case kCondNE:
      case kCondA:  // > 0 if non-zero
        __ Or(dst, lhs_high, lhs_low);
        __ Sltu(dst, ZERO, dst);
        break;
      case kCondLT:
        __ Slt(dst, lhs_high, ZERO);
        break;
      case kCondGE:
        __ Slt(dst, lhs_high, ZERO);
        __ Xori(dst, dst, 1);
        break;
      case kCondLE:
        __ Or(TMP, lhs_high, lhs_low);
        __ Sra(AT, lhs_high, 31);
        __ Sltu(dst, AT, TMP);
        __ Xori(dst, dst, 1);
        break;
      case kCondGT:
        __ Or(TMP, lhs_high, lhs_low);
        __ Sra(AT, lhs_high, 31);
        __ Sltu(dst, AT, TMP);
        break;
      case kCondB:  // always false
        __ Andi(dst, dst, 0);
        break;
      case kCondAE:  // always true
        __ Ori(dst, ZERO, 1);
        break;
    }
  } else if (use_imm) {
    // TODO: more efficient comparison with constants without loading them into TMP/AT.
    switch (cond) {
      case kCondEQ:
        __ LoadConst32(TMP, imm_high);
        __ Xor(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Xor(AT, AT, lhs_low);
        __ Or(dst, TMP, AT);
        __ Sltiu(dst, dst, 1);
        break;
      case kCondNE:
        __ LoadConst32(TMP, imm_high);
        __ Xor(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Xor(AT, AT, lhs_low);
        __ Or(dst, TMP, AT);
        __ Sltu(dst, ZERO, dst);
        break;
      case kCondLT:
      case kCondGE:
        if (dst == lhs_low) {
          __ LoadConst32(TMP, imm_low);
          __ Sltu(dst, lhs_low, TMP);
        }
        __ LoadConst32(TMP, imm_high);
        __ Slt(AT, lhs_high, TMP);
        __ Slt(TMP, TMP, lhs_high);
        if (dst != lhs_low) {
          __ LoadConst32(dst, imm_low);
          __ Sltu(dst, lhs_low, dst);
        }
        __ Slt(dst, TMP, dst);
        __ Or(dst, dst, AT);
        if (cond == kCondGE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondGT:
      case kCondLE:
        if (dst == lhs_low) {
          __ LoadConst32(TMP, imm_low);
          __ Sltu(dst, TMP, lhs_low);
        }
        __ LoadConst32(TMP, imm_high);
        __ Slt(AT, TMP, lhs_high);
        __ Slt(TMP, lhs_high, TMP);
        if (dst != lhs_low) {
          __ LoadConst32(dst, imm_low);
          __ Sltu(dst, dst, lhs_low);
        }
        __ Slt(dst, TMP, dst);
        __ Or(dst, dst, AT);
        if (cond == kCondLE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondB:
      case kCondAE:
        if (dst == lhs_low) {
          __ LoadConst32(TMP, imm_low);
          __ Sltu(dst, lhs_low, TMP);
        }
        __ LoadConst32(TMP, imm_high);
        __ Sltu(AT, lhs_high, TMP);
        __ Sltu(TMP, TMP, lhs_high);
        if (dst != lhs_low) {
          __ LoadConst32(dst, imm_low);
          __ Sltu(dst, lhs_low, dst);
        }
        __ Slt(dst, TMP, dst);
        __ Or(dst, dst, AT);
        if (cond == kCondAE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondA:
      case kCondBE:
        if (dst == lhs_low) {
          __ LoadConst32(TMP, imm_low);
          __ Sltu(dst, TMP, lhs_low);
        }
        __ LoadConst32(TMP, imm_high);
        __ Sltu(AT, TMP, lhs_high);
        __ Sltu(TMP, lhs_high, TMP);
        if (dst != lhs_low) {
          __ LoadConst32(dst, imm_low);
          __ Sltu(dst, dst, lhs_low);
        }
        __ Slt(dst, TMP, dst);
        __ Or(dst, dst, AT);
        if (cond == kCondBE) {
          __ Xori(dst, dst, 1);
        }
        break;
    }
  } else {
    switch (cond) {
      case kCondEQ:
        __ Xor(TMP, lhs_high, rhs_high);
        __ Xor(AT, lhs_low, rhs_low);
        __ Or(dst, TMP, AT);
        __ Sltiu(dst, dst, 1);
        break;
      case kCondNE:
        __ Xor(TMP, lhs_high, rhs_high);
        __ Xor(AT, lhs_low, rhs_low);
        __ Or(dst, TMP, AT);
        __ Sltu(dst, ZERO, dst);
        break;
      case kCondLT:
      case kCondGE:
        __ Slt(TMP, rhs_high, lhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Slt(TMP, TMP, AT);
        __ Slt(AT, lhs_high, rhs_high);
        __ Or(dst, AT, TMP);
        if (cond == kCondGE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondGT:
      case kCondLE:
        __ Slt(TMP, lhs_high, rhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Slt(TMP, TMP, AT);
        __ Slt(AT, rhs_high, lhs_high);
        __ Or(dst, AT, TMP);
        if (cond == kCondLE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondB:
      case kCondAE:
        __ Sltu(TMP, rhs_high, lhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Slt(TMP, TMP, AT);
        __ Sltu(AT, lhs_high, rhs_high);
        __ Or(dst, AT, TMP);
        if (cond == kCondAE) {
          __ Xori(dst, dst, 1);
        }
        break;
      case kCondA:
      case kCondBE:
        __ Sltu(TMP, lhs_high, rhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Slt(TMP, TMP, AT);
        __ Sltu(AT, rhs_high, lhs_high);
        __ Or(dst, AT, TMP);
        if (cond == kCondBE) {
          __ Xori(dst, dst, 1);
        }
        break;
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateLongCompareAndBranch(IfCondition cond,
                                                                LocationSummary* locations,
                                                                MipsLabel* label) {
  Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
  Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
  Location rhs_location = locations->InAt(1);
  Register rhs_high = ZERO;
  Register rhs_low = ZERO;
  int64_t imm = 0;
  uint32_t imm_high = 0;
  uint32_t imm_low = 0;
  bool use_imm = rhs_location.IsConstant();
  if (use_imm) {
    imm = rhs_location.GetConstant()->AsLongConstant()->GetValue();
    imm_high = High32Bits(imm);
    imm_low = Low32Bits(imm);
  } else {
    rhs_high = rhs_location.AsRegisterPairHigh<Register>();
    rhs_low = rhs_location.AsRegisterPairLow<Register>();
  }

  if (use_imm && imm == 0) {
    switch (cond) {
      case kCondEQ:
      case kCondBE:  // <= 0 if zero
        __ Or(TMP, lhs_high, lhs_low);
        __ Beqz(TMP, label);
        break;
      case kCondNE:
      case kCondA:  // > 0 if non-zero
        __ Or(TMP, lhs_high, lhs_low);
        __ Bnez(TMP, label);
        break;
      case kCondLT:
        __ Bltz(lhs_high, label);
        break;
      case kCondGE:
        __ Bgez(lhs_high, label);
        break;
      case kCondLE:
        __ Or(TMP, lhs_high, lhs_low);
        __ Sra(AT, lhs_high, 31);
        __ Bgeu(AT, TMP, label);
        break;
      case kCondGT:
        __ Or(TMP, lhs_high, lhs_low);
        __ Sra(AT, lhs_high, 31);
        __ Bltu(AT, TMP, label);
        break;
      case kCondB:  // always false
        break;
      case kCondAE:  // always true
        __ B(label);
        break;
    }
  } else if (use_imm) {
    // TODO: more efficient comparison with constants without loading them into TMP/AT.
    switch (cond) {
      case kCondEQ:
        __ LoadConst32(TMP, imm_high);
        __ Xor(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Xor(AT, AT, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondNE:
        __ LoadConst32(TMP, imm_high);
        __ Xor(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Xor(AT, AT, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Bnez(TMP, label);
        break;
      case kCondLT:
        __ LoadConst32(TMP, imm_high);
        __ Blt(lhs_high, TMP, label);
        __ Slt(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, lhs_low, AT);
        __ Blt(TMP, AT, label);
        break;
      case kCondGE:
        __ LoadConst32(TMP, imm_high);
        __ Blt(TMP, lhs_high, label);
        __ Slt(TMP, lhs_high, TMP);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, lhs_low, AT);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondLE:
        __ LoadConst32(TMP, imm_high);
        __ Blt(lhs_high, TMP, label);
        __ Slt(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, AT, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondGT:
        __ LoadConst32(TMP, imm_high);
        __ Blt(TMP, lhs_high, label);
        __ Slt(TMP, lhs_high, TMP);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, AT, lhs_low);
        __ Blt(TMP, AT, label);
        break;
      case kCondB:
        __ LoadConst32(TMP, imm_high);
        __ Bltu(lhs_high, TMP, label);
        __ Sltu(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, lhs_low, AT);
        __ Blt(TMP, AT, label);
        break;
      case kCondAE:
        __ LoadConst32(TMP, imm_high);
        __ Bltu(TMP, lhs_high, label);
        __ Sltu(TMP, lhs_high, TMP);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, lhs_low, AT);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondBE:
        __ LoadConst32(TMP, imm_high);
        __ Bltu(lhs_high, TMP, label);
        __ Sltu(TMP, TMP, lhs_high);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, AT, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondA:
        __ LoadConst32(TMP, imm_high);
        __ Bltu(TMP, lhs_high, label);
        __ Sltu(TMP, lhs_high, TMP);
        __ LoadConst32(AT, imm_low);
        __ Sltu(AT, AT, lhs_low);
        __ Blt(TMP, AT, label);
        break;
    }
  } else {
    switch (cond) {
      case kCondEQ:
        __ Xor(TMP, lhs_high, rhs_high);
        __ Xor(AT, lhs_low, rhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondNE:
        __ Xor(TMP, lhs_high, rhs_high);
        __ Xor(AT, lhs_low, rhs_low);
        __ Or(TMP, TMP, AT);
        __ Bnez(TMP, label);
        break;
      case kCondLT:
        __ Blt(lhs_high, rhs_high, label);
        __ Slt(TMP, rhs_high, lhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Blt(TMP, AT, label);
        break;
      case kCondGE:
        __ Blt(rhs_high, lhs_high, label);
        __ Slt(TMP, lhs_high, rhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondLE:
        __ Blt(lhs_high, rhs_high, label);
        __ Slt(TMP, rhs_high, lhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondGT:
        __ Blt(rhs_high, lhs_high, label);
        __ Slt(TMP, lhs_high, rhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Blt(TMP, AT, label);
        break;
      case kCondB:
        __ Bltu(lhs_high, rhs_high, label);
        __ Sltu(TMP, rhs_high, lhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Blt(TMP, AT, label);
        break;
      case kCondAE:
        __ Bltu(rhs_high, lhs_high, label);
        __ Sltu(TMP, lhs_high, rhs_high);
        __ Sltu(AT, lhs_low, rhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondBE:
        __ Bltu(lhs_high, rhs_high, label);
        __ Sltu(TMP, rhs_high, lhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Or(TMP, TMP, AT);
        __ Beqz(TMP, label);
        break;
      case kCondA:
        __ Bltu(rhs_high, lhs_high, label);
        __ Sltu(TMP, lhs_high, rhs_high);
        __ Sltu(AT, rhs_low, lhs_low);
        __ Blt(TMP, AT, label);
        break;
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateFpCompare(IfCondition cond,
                                                     bool gt_bias,
                                                     DataType::Type type,
                                                     LocationSummary* locations) {
  Register dst = locations->Out().AsRegister<Register>();
  FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  if (type == DataType::Type::kFloat32) {
    if (isR6) {
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
      switch (cond) {
        case kCondEQ:
          __ CeqS(0, lhs, rhs);
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondNE:
          __ CeqS(0, lhs, rhs);
          __ LoadConst32(dst, 1);
          __ Movt(dst, ZERO, 0);
          break;
        case kCondLT:
          if (gt_bias) {
            __ ColtS(0, lhs, rhs);
          } else {
            __ CultS(0, lhs, rhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondLE:
          if (gt_bias) {
            __ ColeS(0, lhs, rhs);
          } else {
            __ CuleS(0, lhs, rhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondGT:
          if (gt_bias) {
            __ CultS(0, rhs, lhs);
          } else {
            __ ColtS(0, rhs, lhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondGE:
          if (gt_bias) {
            __ CuleS(0, rhs, lhs);
          } else {
            __ ColeS(0, rhs, lhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        default:
          LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
          UNREACHABLE();
      }
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    if (isR6) {
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
    } else {
      switch (cond) {
        case kCondEQ:
          __ CeqD(0, lhs, rhs);
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondNE:
          __ CeqD(0, lhs, rhs);
          __ LoadConst32(dst, 1);
          __ Movt(dst, ZERO, 0);
          break;
        case kCondLT:
          if (gt_bias) {
            __ ColtD(0, lhs, rhs);
          } else {
            __ CultD(0, lhs, rhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondLE:
          if (gt_bias) {
            __ ColeD(0, lhs, rhs);
          } else {
            __ CuleD(0, lhs, rhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondGT:
          if (gt_bias) {
            __ CultD(0, rhs, lhs);
          } else {
            __ ColtD(0, rhs, lhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        case kCondGE:
          if (gt_bias) {
            __ CuleD(0, rhs, lhs);
          } else {
            __ ColeD(0, rhs, lhs);
          }
          __ LoadConst32(dst, 1);
          __ Movf(dst, ZERO, 0);
          break;
        default:
          LOG(FATAL) << "Unexpected non-floating-point condition " << cond;
          UNREACHABLE();
      }
    }
  }
}

bool InstructionCodeGeneratorMIPS::MaterializeFpCompareR2(IfCondition cond,
                                                          bool gt_bias,
                                                          DataType::Type type,
                                                          LocationSummary* input_locations,
                                                          int cc) {
  FRegister lhs = input_locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister rhs = input_locations->InAt(1).AsFpuRegister<FRegister>();
  CHECK(!codegen_->GetInstructionSetFeatures().IsR6());
  if (type == DataType::Type::kFloat32) {
    switch (cond) {
      case kCondEQ:
        __ CeqS(cc, lhs, rhs);
        return false;
      case kCondNE:
        __ CeqS(cc, lhs, rhs);
        return true;
      case kCondLT:
        if (gt_bias) {
          __ ColtS(cc, lhs, rhs);
        } else {
          __ CultS(cc, lhs, rhs);
        }
        return false;
      case kCondLE:
        if (gt_bias) {
          __ ColeS(cc, lhs, rhs);
        } else {
          __ CuleS(cc, lhs, rhs);
        }
        return false;
      case kCondGT:
        if (gt_bias) {
          __ CultS(cc, rhs, lhs);
        } else {
          __ ColtS(cc, rhs, lhs);
        }
        return false;
      case kCondGE:
        if (gt_bias) {
          __ CuleS(cc, rhs, lhs);
        } else {
          __ ColeS(cc, rhs, lhs);
        }
        return false;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition";
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    switch (cond) {
      case kCondEQ:
        __ CeqD(cc, lhs, rhs);
        return false;
      case kCondNE:
        __ CeqD(cc, lhs, rhs);
        return true;
      case kCondLT:
        if (gt_bias) {
          __ ColtD(cc, lhs, rhs);
        } else {
          __ CultD(cc, lhs, rhs);
        }
        return false;
      case kCondLE:
        if (gt_bias) {
          __ ColeD(cc, lhs, rhs);
        } else {
          __ CuleD(cc, lhs, rhs);
        }
        return false;
      case kCondGT:
        if (gt_bias) {
          __ CultD(cc, rhs, lhs);
        } else {
          __ ColtD(cc, rhs, lhs);
        }
        return false;
      case kCondGE:
        if (gt_bias) {
          __ CuleD(cc, rhs, lhs);
        } else {
          __ ColeD(cc, rhs, lhs);
        }
        return false;
      default:
        LOG(FATAL) << "Unexpected non-floating-point condition";
        UNREACHABLE();
    }
  }
}

bool InstructionCodeGeneratorMIPS::MaterializeFpCompareR6(IfCondition cond,
                                                          bool gt_bias,
                                                          DataType::Type type,
                                                          LocationSummary* input_locations,
                                                          FRegister dst) {
  FRegister lhs = input_locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister rhs = input_locations->InAt(1).AsFpuRegister<FRegister>();
  CHECK(codegen_->GetInstructionSetFeatures().IsR6());
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
        LOG(FATAL) << "Unexpected non-floating-point condition";
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
        LOG(FATAL) << "Unexpected non-floating-point condition";
        UNREACHABLE();
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateFpCompareAndBranch(IfCondition cond,
                                                              bool gt_bias,
                                                              DataType::Type type,
                                                              LocationSummary* locations,
                                                              MipsLabel* label) {
  FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  if (type == DataType::Type::kFloat32) {
    if (isR6) {
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
      switch (cond) {
        case kCondEQ:
          __ CeqS(0, lhs, rhs);
          __ Bc1t(0, label);
          break;
        case kCondNE:
          __ CeqS(0, lhs, rhs);
          __ Bc1f(0, label);
          break;
        case kCondLT:
          if (gt_bias) {
            __ ColtS(0, lhs, rhs);
          } else {
            __ CultS(0, lhs, rhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondLE:
          if (gt_bias) {
            __ ColeS(0, lhs, rhs);
          } else {
            __ CuleS(0, lhs, rhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondGT:
          if (gt_bias) {
            __ CultS(0, rhs, lhs);
          } else {
            __ ColtS(0, rhs, lhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondGE:
          if (gt_bias) {
            __ CuleS(0, rhs, lhs);
          } else {
            __ ColeS(0, rhs, lhs);
          }
          __ Bc1t(0, label);
          break;
        default:
          LOG(FATAL) << "Unexpected non-floating-point condition";
          UNREACHABLE();
      }
    }
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat64);
    if (isR6) {
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
    } else {
      switch (cond) {
        case kCondEQ:
          __ CeqD(0, lhs, rhs);
          __ Bc1t(0, label);
          break;
        case kCondNE:
          __ CeqD(0, lhs, rhs);
          __ Bc1f(0, label);
          break;
        case kCondLT:
          if (gt_bias) {
            __ ColtD(0, lhs, rhs);
          } else {
            __ CultD(0, lhs, rhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondLE:
          if (gt_bias) {
            __ ColeD(0, lhs, rhs);
          } else {
            __ CuleD(0, lhs, rhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondGT:
          if (gt_bias) {
            __ CultD(0, rhs, lhs);
          } else {
            __ ColtD(0, rhs, lhs);
          }
          __ Bc1t(0, label);
          break;
        case kCondGE:
          if (gt_bias) {
            __ CuleD(0, rhs, lhs);
          } else {
            __ ColeD(0, rhs, lhs);
          }
          __ Bc1t(0, label);
          break;
        default:
          LOG(FATAL) << "Unexpected non-floating-point condition";
          UNREACHABLE();
      }
    }
  }
}

void InstructionCodeGeneratorMIPS::GenerateTestAndBranch(HInstruction* instruction,
                                                         size_t condition_input_index,
                                                         MipsLabel* true_target,
                                                         MipsLabel* false_target) {
  HInstruction* cond = instruction->InputAt(condition_input_index);

  if (true_target == nullptr && false_target == nullptr) {
    // Nothing to do. The code always falls through.
    return;
  } else if (cond->IsIntConstant()) {
    // Constant condition, statically compared against "true" (integer value 1).
    if (cond->AsIntConstant()->IsTrue()) {
      if (true_target != nullptr) {
        __ B(true_target);
      }
    } else {
      DCHECK(cond->AsIntConstant()->IsFalse()) << cond->AsIntConstant()->GetValue();
      if (false_target != nullptr) {
        __ B(false_target);
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
      __ Beqz(cond_val.AsRegister<Register>(), false_target);
    } else {
      __ Bnez(cond_val.AsRegister<Register>(), true_target);
    }
  } else {
    // The condition instruction has not been materialized, use its inputs as
    // the comparison and its condition as the branch condition.
    HCondition* condition = cond->AsCondition();
    DataType::Type type = condition->InputAt(0)->GetType();
    LocationSummary* locations = cond->GetLocations();
    IfCondition if_cond = condition->GetCondition();
    MipsLabel* branch_target = true_target;

    if (true_target == nullptr) {
      if_cond = condition->GetOppositeCondition();
      branch_target = false_target;
    }

    switch (type) {
      default:
        GenerateIntCompareAndBranch(if_cond, locations, branch_target);
        break;
      case DataType::Type::kInt64:
        GenerateLongCompareAndBranch(if_cond, locations, branch_target);
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
    __ B(false_target);
  }
}

void LocationsBuilderMIPS::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(if_instr);
  if (IsBooleanValueOrMaterializedCondition(if_instr->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorMIPS::VisitIf(HIf* if_instr) {
  HBasicBlock* true_successor = if_instr->IfTrueSuccessor();
  HBasicBlock* false_successor = if_instr->IfFalseSuccessor();
  MipsLabel* true_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), true_successor) ?
      nullptr : codegen_->GetLabelOf(true_successor);
  MipsLabel* false_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), false_successor) ?
      nullptr : codegen_->GetLabelOf(false_successor);
  GenerateTestAndBranch(if_instr, /* condition_input_index */ 0, true_target, false_target);
}

void LocationsBuilderMIPS::VisitDeoptimize(HDeoptimize* deoptimize) {
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

void InstructionCodeGeneratorMIPS::VisitDeoptimize(HDeoptimize* deoptimize) {
  SlowPathCodeMIPS* slow_path =
      deopt_slow_paths_.NewSlowPath<DeoptimizationSlowPathMIPS>(deoptimize);
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
static bool CanMoveConditionally(HSelect* select, bool is_r6, LocationSummary* locations_to_set) {
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
    switch (cond_type) {
      default:
        switch (dst_type) {
          default:
            // Moving int on int condition.
            if (is_r6) {
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
              // movn out_reg, true_reg/ZERO, cond_reg
              can_move_conditionally = true;
              use_const_for_true_in = is_true_value_zero_constant;
            }
            break;
          case DataType::Type::kInt64:
            // Moving long on int condition.
            if (is_r6) {
              if (is_true_value_zero_constant) {
                // seleqz out_reg_lo, false_reg_lo, cond_reg
                // seleqz out_reg_hi, false_reg_hi, cond_reg
                can_move_conditionally = true;
                use_const_for_true_in = true;
              } else if (is_false_value_zero_constant) {
                // selnez out_reg_lo, true_reg_lo, cond_reg
                // selnez out_reg_hi, true_reg_hi, cond_reg
                can_move_conditionally = true;
                use_const_for_false_in = true;
              }
              // Other long conditional moves would generate 6+ instructions,
              // which is too many.
            } else {
              // movn out_reg_lo, true_reg_lo/ZERO, cond_reg
              // movn out_reg_hi, true_reg_hi/ZERO, cond_reg
              can_move_conditionally = true;
              use_const_for_true_in = is_true_value_zero_constant;
            }
            break;
          case DataType::Type::kFloat32:
          case DataType::Type::kFloat64:
            // Moving float/double on int condition.
            if (is_r6) {
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
            } else {
              // movn.fmt out_reg, true_reg, cond_reg
              can_move_conditionally = true;
            }
            break;
        }
        break;
      case DataType::Type::kInt64:
        // We don't materialize long comparison now
        // and use conditional branches instead.
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64:
        switch (dst_type) {
          default:
            // Moving int on float/double condition.
            if (is_r6) {
              if (is_true_value_zero_constant) {
                // mfc1 TMP, temp_cond_reg
                // seleqz out_reg, false_reg, TMP
                can_move_conditionally = true;
                use_const_for_true_in = true;
              } else if (is_false_value_zero_constant) {
                // mfc1 TMP, temp_cond_reg
                // selnez out_reg, true_reg, TMP
                can_move_conditionally = true;
                use_const_for_false_in = true;
              } else {
                // mfc1 TMP, temp_cond_reg
                // selnez AT, true_reg, TMP
                // seleqz TMP, false_reg, TMP
                // or out_reg, AT, TMP
                can_move_conditionally = true;
              }
            } else {
              // movt out_reg, true_reg/ZERO, cc
              can_move_conditionally = true;
              use_const_for_true_in = is_true_value_zero_constant;
            }
            break;
          case DataType::Type::kInt64:
            // Moving long on float/double condition.
            if (is_r6) {
              if (is_true_value_zero_constant) {
                // mfc1 TMP, temp_cond_reg
                // seleqz out_reg_lo, false_reg_lo, TMP
                // seleqz out_reg_hi, false_reg_hi, TMP
                can_move_conditionally = true;
                use_const_for_true_in = true;
              } else if (is_false_value_zero_constant) {
                // mfc1 TMP, temp_cond_reg
                // selnez out_reg_lo, true_reg_lo, TMP
                // selnez out_reg_hi, true_reg_hi, TMP
                can_move_conditionally = true;
                use_const_for_false_in = true;
              }
              // Other long conditional moves would generate 6+ instructions,
              // which is too many.
            } else {
              // movt out_reg_lo, true_reg_lo/ZERO, cc
              // movt out_reg_hi, true_reg_hi/ZERO, cc
              can_move_conditionally = true;
              use_const_for_true_in = is_true_value_zero_constant;
            }
            break;
          case DataType::Type::kFloat32:
          case DataType::Type::kFloat64:
            // Moving float/double on float/double condition.
            if (is_r6) {
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
            } else {
              // movt.fmt out_reg, true_reg, cc
              can_move_conditionally = true;
            }
            break;
        }
        break;
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
    // On R6 we don't require the output to be the same as the
    // first input for conditional moves unlike on R2.
    bool is_out_same_as_first_in = !can_move_conditionally || !is_r6;
    if (is_out_same_as_first_in) {
      locations_to_set->SetOut(Location::SameAsFirstInput());
    } else {
      locations_to_set->SetOut(DataType::IsFloatingPointType(dst_type)
                                   ? Location::RequiresFpuRegister()
                                   : Location::RequiresRegister());
    }
  }

  return can_move_conditionally;
}

void InstructionCodeGeneratorMIPS::GenConditionalMoveR2(HSelect* select) {
  LocationSummary* locations = select->GetLocations();
  Location dst = locations->Out();
  Location src = locations->InAt(1);
  Register src_reg = ZERO;
  Register src_reg_high = ZERO;
  HInstruction* cond = select->InputAt(/* condition_input_index */ 2);
  Register cond_reg = TMP;
  int cond_cc = 0;
  DataType::Type cond_type = DataType::Type::kInt32;
  bool cond_inverted = false;
  DataType::Type dst_type = select->GetType();

  if (IsBooleanValueOrMaterializedCondition(cond)) {
    cond_reg = locations->InAt(/* condition_input_index */ 2).AsRegister<Register>();
  } else {
    HCondition* condition = cond->AsCondition();
    LocationSummary* cond_locations = cond->GetLocations();
    IfCondition if_cond = condition->GetCondition();
    cond_type = condition->InputAt(0)->GetType();
    switch (cond_type) {
      default:
        DCHECK_NE(cond_type, DataType::Type::kInt64);
        cond_inverted = MaterializeIntCompare(if_cond, cond_locations, cond_reg);
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64:
        cond_inverted = MaterializeFpCompareR2(if_cond,
                                               condition->IsGtBias(),
                                               cond_type,
                                               cond_locations,
                                               cond_cc);
        break;
    }
  }

  DCHECK(dst.Equals(locations->InAt(0)));
  if (src.IsRegister()) {
    src_reg = src.AsRegister<Register>();
  } else if (src.IsRegisterPair()) {
    src_reg = src.AsRegisterPairLow<Register>();
    src_reg_high = src.AsRegisterPairHigh<Register>();
  } else if (src.IsConstant()) {
    DCHECK(src.GetConstant()->IsZeroBitPattern());
  }

  switch (cond_type) {
    default:
      switch (dst_type) {
        default:
          if (cond_inverted) {
            __ Movz(dst.AsRegister<Register>(), src_reg, cond_reg);
          } else {
            __ Movn(dst.AsRegister<Register>(), src_reg, cond_reg);
          }
          break;
        case DataType::Type::kInt64:
          if (cond_inverted) {
            __ Movz(dst.AsRegisterPairLow<Register>(), src_reg, cond_reg);
            __ Movz(dst.AsRegisterPairHigh<Register>(), src_reg_high, cond_reg);
          } else {
            __ Movn(dst.AsRegisterPairLow<Register>(), src_reg, cond_reg);
            __ Movn(dst.AsRegisterPairHigh<Register>(), src_reg_high, cond_reg);
          }
          break;
        case DataType::Type::kFloat32:
          if (cond_inverted) {
            __ MovzS(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_reg);
          } else {
            __ MovnS(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_reg);
          }
          break;
        case DataType::Type::kFloat64:
          if (cond_inverted) {
            __ MovzD(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_reg);
          } else {
            __ MovnD(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_reg);
          }
          break;
      }
      break;
    case DataType::Type::kInt64:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      switch (dst_type) {
        default:
          if (cond_inverted) {
            __ Movf(dst.AsRegister<Register>(), src_reg, cond_cc);
          } else {
            __ Movt(dst.AsRegister<Register>(), src_reg, cond_cc);
          }
          break;
        case DataType::Type::kInt64:
          if (cond_inverted) {
            __ Movf(dst.AsRegisterPairLow<Register>(), src_reg, cond_cc);
            __ Movf(dst.AsRegisterPairHigh<Register>(), src_reg_high, cond_cc);
          } else {
            __ Movt(dst.AsRegisterPairLow<Register>(), src_reg, cond_cc);
            __ Movt(dst.AsRegisterPairHigh<Register>(), src_reg_high, cond_cc);
          }
          break;
        case DataType::Type::kFloat32:
          if (cond_inverted) {
            __ MovfS(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_cc);
          } else {
            __ MovtS(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_cc);
          }
          break;
        case DataType::Type::kFloat64:
          if (cond_inverted) {
            __ MovfD(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_cc);
          } else {
            __ MovtD(dst.AsFpuRegister<FRegister>(), src.AsFpuRegister<FRegister>(), cond_cc);
          }
          break;
      }
      break;
  }
}

void InstructionCodeGeneratorMIPS::GenConditionalMoveR6(HSelect* select) {
  LocationSummary* locations = select->GetLocations();
  Location dst = locations->Out();
  Location false_src = locations->InAt(0);
  Location true_src = locations->InAt(1);
  HInstruction* cond = select->InputAt(/* condition_input_index */ 2);
  Register cond_reg = TMP;
  FRegister fcond_reg = FTMP;
  DataType::Type cond_type = DataType::Type::kInt32;
  bool cond_inverted = false;
  DataType::Type dst_type = select->GetType();

  if (IsBooleanValueOrMaterializedCondition(cond)) {
    cond_reg = locations->InAt(/* condition_input_index */ 2).AsRegister<Register>();
  } else {
    HCondition* condition = cond->AsCondition();
    LocationSummary* cond_locations = cond->GetLocations();
    IfCondition if_cond = condition->GetCondition();
    cond_type = condition->InputAt(0)->GetType();
    switch (cond_type) {
      default:
        DCHECK_NE(cond_type, DataType::Type::kInt64);
        cond_inverted = MaterializeIntCompare(if_cond, cond_locations, cond_reg);
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64:
        cond_inverted = MaterializeFpCompareR6(if_cond,
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
          __ Selnez(dst.AsRegister<Register>(), false_src.AsRegister<Register>(), cond_reg);
        } else {
          __ Seleqz(dst.AsRegister<Register>(), false_src.AsRegister<Register>(), cond_reg);
        }
      } else if (false_src.IsConstant()) {
        if (cond_inverted) {
          __ Seleqz(dst.AsRegister<Register>(), true_src.AsRegister<Register>(), cond_reg);
        } else {
          __ Selnez(dst.AsRegister<Register>(), true_src.AsRegister<Register>(), cond_reg);
        }
      } else {
        DCHECK_NE(cond_reg, AT);
        if (cond_inverted) {
          __ Seleqz(AT, true_src.AsRegister<Register>(), cond_reg);
          __ Selnez(TMP, false_src.AsRegister<Register>(), cond_reg);
        } else {
          __ Selnez(AT, true_src.AsRegister<Register>(), cond_reg);
          __ Seleqz(TMP, false_src.AsRegister<Register>(), cond_reg);
        }
        __ Or(dst.AsRegister<Register>(), AT, TMP);
      }
      break;
    case DataType::Type::kInt64: {
      if (DataType::IsFloatingPointType(cond_type)) {
        __ Mfc1(cond_reg, fcond_reg);
      }
      Register dst_lo = dst.AsRegisterPairLow<Register>();
      Register dst_hi = dst.AsRegisterPairHigh<Register>();
      if (true_src.IsConstant()) {
        Register src_lo = false_src.AsRegisterPairLow<Register>();
        Register src_hi = false_src.AsRegisterPairHigh<Register>();
        if (cond_inverted) {
          __ Selnez(dst_lo, src_lo, cond_reg);
          __ Selnez(dst_hi, src_hi, cond_reg);
        } else {
          __ Seleqz(dst_lo, src_lo, cond_reg);
          __ Seleqz(dst_hi, src_hi, cond_reg);
        }
      } else {
        DCHECK(false_src.IsConstant());
        Register src_lo = true_src.AsRegisterPairLow<Register>();
        Register src_hi = true_src.AsRegisterPairHigh<Register>();
        if (cond_inverted) {
          __ Seleqz(dst_lo, src_lo, cond_reg);
          __ Seleqz(dst_hi, src_hi, cond_reg);
        } else {
          __ Selnez(dst_lo, src_lo, cond_reg);
          __ Selnez(dst_hi, src_hi, cond_reg);
        }
      }
      break;
    }
    case DataType::Type::kFloat32: {
      if (!DataType::IsFloatingPointType(cond_type)) {
        // sel*.fmt tests bit 0 of the condition register, account for that.
        __ Sltu(TMP, ZERO, cond_reg);
        __ Mtc1(TMP, fcond_reg);
      }
      FRegister dst_reg = dst.AsFpuRegister<FRegister>();
      if (true_src.IsConstant()) {
        FRegister src_reg = false_src.AsFpuRegister<FRegister>();
        if (cond_inverted) {
          __ SelnezS(dst_reg, src_reg, fcond_reg);
        } else {
          __ SeleqzS(dst_reg, src_reg, fcond_reg);
        }
      } else if (false_src.IsConstant()) {
        FRegister src_reg = true_src.AsFpuRegister<FRegister>();
        if (cond_inverted) {
          __ SeleqzS(dst_reg, src_reg, fcond_reg);
        } else {
          __ SelnezS(dst_reg, src_reg, fcond_reg);
        }
      } else {
        if (cond_inverted) {
          __ SelS(fcond_reg,
                  true_src.AsFpuRegister<FRegister>(),
                  false_src.AsFpuRegister<FRegister>());
        } else {
          __ SelS(fcond_reg,
                  false_src.AsFpuRegister<FRegister>(),
                  true_src.AsFpuRegister<FRegister>());
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
      FRegister dst_reg = dst.AsFpuRegister<FRegister>();
      if (true_src.IsConstant()) {
        FRegister src_reg = false_src.AsFpuRegister<FRegister>();
        if (cond_inverted) {
          __ SelnezD(dst_reg, src_reg, fcond_reg);
        } else {
          __ SeleqzD(dst_reg, src_reg, fcond_reg);
        }
      } else if (false_src.IsConstant()) {
        FRegister src_reg = true_src.AsFpuRegister<FRegister>();
        if (cond_inverted) {
          __ SeleqzD(dst_reg, src_reg, fcond_reg);
        } else {
          __ SelnezD(dst_reg, src_reg, fcond_reg);
        }
      } else {
        if (cond_inverted) {
          __ SelD(fcond_reg,
                  true_src.AsFpuRegister<FRegister>(),
                  false_src.AsFpuRegister<FRegister>());
        } else {
          __ SelD(fcond_reg,
                  false_src.AsFpuRegister<FRegister>(),
                  true_src.AsFpuRegister<FRegister>());
        }
        __ MovD(dst_reg, fcond_reg);
      }
      break;
    }
  }
}

void LocationsBuilderMIPS::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(flag, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  __ LoadFromOffset(kLoadWord,
                    flag->GetLocations()->Out().AsRegister<Register>(),
                    SP,
                    codegen_->GetStackOffsetOfShouldDeoptimizeFlag());
}

void LocationsBuilderMIPS::VisitSelect(HSelect* select) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(select);
  CanMoveConditionally(select, codegen_->GetInstructionSetFeatures().IsR6(), locations);
}

void InstructionCodeGeneratorMIPS::VisitSelect(HSelect* select) {
  bool is_r6 = codegen_->GetInstructionSetFeatures().IsR6();
  if (CanMoveConditionally(select, is_r6, /* locations_to_set */ nullptr)) {
    if (is_r6) {
      GenConditionalMoveR6(select);
    } else {
      GenConditionalMoveR2(select);
    }
  } else {
    LocationSummary* locations = select->GetLocations();
    MipsLabel false_target;
    GenerateTestAndBranch(select,
                          /* condition_input_index */ 2,
                          /* true_target */ nullptr,
                          &false_target);
    codegen_->MoveLocation(locations->Out(), locations->InAt(1), select->GetType());
    __ Bind(&false_target);
  }
}

void LocationsBuilderMIPS::VisitNativeDebugInfo(HNativeDebugInfo* info) {
  new (GetGraph()->GetAllocator()) LocationSummary(info);
}

void InstructionCodeGeneratorMIPS::VisitNativeDebugInfo(HNativeDebugInfo*) {
  // MaybeRecordNativeDebugInfo is already called implicitly in CodeGenerator::Compile.
}

void CodeGeneratorMIPS::GenerateNop() {
  __ Nop();
}

void LocationsBuilderMIPS::HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info) {
  DataType::Type field_type = field_info.GetFieldType();
  bool is_wide = (field_type == DataType::Type::kInt64) || (field_type == DataType::Type::kFloat64);
  bool generate_volatile = field_info.IsVolatile() && is_wide;
  bool object_field_get_with_read_barrier =
      kEmitCompilerReadBarrier && (field_type == DataType::Type::kReference);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction,
      generate_volatile
          ? LocationSummary::kCallOnMainOnly
          : (object_field_get_with_read_barrier
              ? LocationSummary::kCallOnSlowPath
              : LocationSummary::kNoCall));

  if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  locations->SetInAt(0, Location::RequiresRegister());
  if (generate_volatile) {
    InvokeRuntimeCallingConvention calling_convention;
    // need A0 to hold base + offset
    locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
    if (field_type == DataType::Type::kInt64) {
      locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kInt64));
    } else {
      // Use Location::Any() to prevent situations when running out of available fp registers.
      locations->SetOut(Location::Any());
      // Need some temp core regs since FP results are returned in core registers
      Location reg = calling_convention.GetReturnLocation(DataType::Type::kInt64);
      locations->AddTemp(Location::RegisterLocation(reg.AsRegisterPairLow<Register>()));
      locations->AddTemp(Location::RegisterLocation(reg.AsRegisterPairHigh<Register>()));
    }
  } else {
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
      // path in CodeGeneratorMIPS::GenerateFieldLoadWithBakerReadBarrier.
      if (!kBakerReadBarrierThunksEnableForFields) {
        locations->AddTemp(Location::RequiresRegister());
      }
    }
  }
}

void InstructionCodeGeneratorMIPS::HandleFieldGet(HInstruction* instruction,
                                                  const FieldInfo& field_info,
                                                  uint32_t dex_pc) {
  DCHECK_EQ(DataType::Size(field_info.GetFieldType()), DataType::Size(instruction->GetType()));
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = obj_loc.AsRegister<Register>();
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
    case DataType::Type::kReference:
      load_type = kLoadWord;
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      load_type = kLoadDoubleword;
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }

  if (is_volatile && load_type == kLoadDoubleword) {
    InvokeRuntimeCallingConvention calling_convention;
    __ Addiu32(locations->GetTemp(0).AsRegister<Register>(), obj, offset);
    // Do implicit Null check
    __ LoadFromOffset(kLoadWord,
                      ZERO,
                      locations->GetTemp(0).AsRegister<Register>(),
                      0,
                      null_checker);
    codegen_->InvokeRuntime(kQuickA64Load, instruction, dex_pc);
    CheckEntrypointTypes<kQuickA64Load, int64_t, volatile const int64_t*>();
    if (type == DataType::Type::kFloat64) {
      // FP results are returned in core registers. Need to move them.
      if (dst_loc.IsFpuRegister()) {
        __ Mtc1(locations->GetTemp(1).AsRegister<Register>(), dst_loc.AsFpuRegister<FRegister>());
        __ MoveToFpuHigh(locations->GetTemp(2).AsRegister<Register>(),
                         dst_loc.AsFpuRegister<FRegister>());
      } else {
        DCHECK(dst_loc.IsDoubleStackSlot());
        __ StoreToOffset(kStoreWord,
                         locations->GetTemp(1).AsRegister<Register>(),
                         SP,
                         dst_loc.GetStackIndex());
        __ StoreToOffset(kStoreWord,
                         locations->GetTemp(2).AsRegister<Register>(),
                         SP,
                         dst_loc.GetStackIndex() + 4);
      }
    }
  } else {
    if (type == DataType::Type::kReference) {
      // /* HeapReference<Object> */ dst = *(obj + offset)
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        Location temp_loc =
            kBakerReadBarrierThunksEnableForFields ? Location::NoLocation() : locations->GetTemp(0);
        // Note that a potential implicit null check is handled in this
        // CodeGeneratorMIPS::GenerateFieldLoadWithBakerReadBarrier call.
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
        __ LoadFromOffset(kLoadWord, dst_loc.AsRegister<Register>(), obj, offset, null_checker);
        if (is_volatile) {
          GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
        }
        // If read barriers are enabled, emit read barriers other than
        // Baker's using a slow path (and also unpoison the loaded
        // reference, if heap poisoning is enabled).
        codegen_->MaybeGenerateReadBarrierSlow(instruction, dst_loc, dst_loc, obj_loc, offset);
      }
    } else if (!DataType::IsFloatingPointType(type)) {
      Register dst;
      if (type == DataType::Type::kInt64) {
        DCHECK(dst_loc.IsRegisterPair());
        dst = dst_loc.AsRegisterPairLow<Register>();
      } else {
        DCHECK(dst_loc.IsRegister());
        dst = dst_loc.AsRegister<Register>();
      }
      __ LoadFromOffset(load_type, dst, obj, offset, null_checker);
    } else {
      DCHECK(dst_loc.IsFpuRegister());
      FRegister dst = dst_loc.AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ LoadSFromOffset(dst, obj, offset, null_checker);
      } else {
        __ LoadDFromOffset(dst, obj, offset, null_checker);
      }
    }
  }

  // Memory barriers, in the case of references, are handled in the
  // previous switch statement.
  if (is_volatile && (type != DataType::Type::kReference)) {
    GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
  }
}

void LocationsBuilderMIPS::HandleFieldSet(HInstruction* instruction, const FieldInfo& field_info) {
  DataType::Type field_type = field_info.GetFieldType();
  bool is_wide = (field_type == DataType::Type::kInt64) || (field_type == DataType::Type::kFloat64);
  bool generate_volatile = field_info.IsVolatile() && is_wide;
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, generate_volatile ? LocationSummary::kCallOnMainOnly : LocationSummary::kNoCall);

  locations->SetInAt(0, Location::RequiresRegister());
  if (generate_volatile) {
    InvokeRuntimeCallingConvention calling_convention;
    // need A0 to hold base + offset
    locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
    if (field_type == DataType::Type::kInt64) {
      locations->SetInAt(1, Location::RegisterPairLocation(
          calling_convention.GetRegisterAt(2), calling_convention.GetRegisterAt(3)));
    } else {
      // Use Location::Any() to prevent situations when running out of available fp registers.
      locations->SetInAt(1, Location::Any());
      // Pass FP parameters in core registers.
      locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
      locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(3)));
    }
  } else {
    if (DataType::IsFloatingPointType(field_type)) {
      locations->SetInAt(1, FpuRegisterOrConstantForStore(instruction->InputAt(1)));
    } else {
      locations->SetInAt(1, RegisterOrZeroConstant(instruction->InputAt(1)));
    }
  }
}

void InstructionCodeGeneratorMIPS::HandleFieldSet(HInstruction* instruction,
                                                  const FieldInfo& field_info,
                                                  uint32_t dex_pc,
                                                  bool value_can_be_null) {
  DataType::Type type = field_info.GetFieldType();
  LocationSummary* locations = instruction->GetLocations();
  Register obj = locations->InAt(0).AsRegister<Register>();
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

  if (is_volatile && store_type == kStoreDoubleword) {
    InvokeRuntimeCallingConvention calling_convention;
    __ Addiu32(locations->GetTemp(0).AsRegister<Register>(), obj, offset);
    // Do implicit Null check.
    __ LoadFromOffset(kLoadWord,
                      ZERO,
                      locations->GetTemp(0).AsRegister<Register>(),
                      0,
                      null_checker);
    if (type == DataType::Type::kFloat64) {
      // Pass FP parameters in core registers.
      if (value_location.IsFpuRegister()) {
        __ Mfc1(locations->GetTemp(1).AsRegister<Register>(),
                value_location.AsFpuRegister<FRegister>());
        __ MoveFromFpuHigh(locations->GetTemp(2).AsRegister<Register>(),
                           value_location.AsFpuRegister<FRegister>());
      } else if (value_location.IsDoubleStackSlot()) {
        __ LoadFromOffset(kLoadWord,
                          locations->GetTemp(1).AsRegister<Register>(),
                          SP,
                          value_location.GetStackIndex());
        __ LoadFromOffset(kLoadWord,
                          locations->GetTemp(2).AsRegister<Register>(),
                          SP,
                          value_location.GetStackIndex() + 4);
      } else {
        DCHECK(value_location.IsConstant());
        DCHECK(value_location.GetConstant()->IsDoubleConstant());
        int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
        __ LoadConst64(locations->GetTemp(2).AsRegister<Register>(),
                       locations->GetTemp(1).AsRegister<Register>(),
                       value);
      }
    }
    codegen_->InvokeRuntime(kQuickA64Store, instruction, dex_pc);
    CheckEntrypointTypes<kQuickA64Store, void, volatile int64_t *, int64_t>();
  } else {
    if (value_location.IsConstant()) {
      int64_t value = CodeGenerator::GetInt64ValueOf(value_location.GetConstant());
      __ StoreConstToOffset(store_type, value, obj, offset, TMP, null_checker);
    } else if (!DataType::IsFloatingPointType(type)) {
      Register src;
      if (type == DataType::Type::kInt64) {
        src = value_location.AsRegisterPairLow<Register>();
      } else {
        src = value_location.AsRegister<Register>();
      }
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
      FRegister src = value_location.AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ StoreSToOffset(src, obj, offset, null_checker);
      } else {
        __ StoreDToOffset(src, obj, offset, null_checker);
      }
    }
  }

  if (needs_write_barrier) {
    Register src = value_location.AsRegister<Register>();
    codegen_->MarkGCCard(obj, src, value_can_be_null);
  }

  if (is_volatile) {
    GenerateMemoryBarrier(MemBarrierKind::kAnyAny);
  }
}

void LocationsBuilderMIPS::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo(), instruction->GetDexPc());
}

void LocationsBuilderMIPS::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction,
                 instruction->GetFieldInfo(),
                 instruction->GetDexPc(),
                 instruction->GetValueCanBeNull());
}

void InstructionCodeGeneratorMIPS::GenerateReferenceLoadOneRegister(
    HInstruction* instruction,
    Location out,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  Register out_reg = out.AsRegister<Register>();
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
      __ Move(maybe_temp.AsRegister<Register>(), out_reg);
      // /* HeapReference<Object> */ out = *(out + offset)
      __ LoadFromOffset(kLoadWord, out_reg, out_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, maybe_temp, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(out + offset)
    __ LoadFromOffset(kLoadWord, out_reg, out_reg, offset);
    __ MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorMIPS::GenerateReferenceLoadTwoRegisters(
    HInstruction* instruction,
    Location out,
    Location obj,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  Register out_reg = out.AsRegister<Register>();
  Register obj_reg = obj.AsRegister<Register>();
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
      __ LoadFromOffset(kLoadWord, out_reg, obj_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, obj, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(obj + offset)
    __ LoadFromOffset(kLoadWord, out_reg, obj_reg, offset);
    __ MaybeUnpoisonHeapReference(out_reg);
  }
}

static inline int GetBakerMarkThunkNumber(Register reg) {
  static_assert(BAKER_MARK_INTROSPECTION_REGISTER_COUNT == 21, "Expecting equal");
  if (reg >= V0 && reg <= T7) {  // 14 consequtive regs.
    return reg - V0;
  } else if (reg >= S2 && reg <= S7) {  // 6 consequtive regs.
    return 14 + (reg - S2);
  } else if (reg == FP) {  // One more.
    return 20;
  }
  LOG(FATAL) << "Unexpected register " << reg;
  UNREACHABLE();
}

static inline int GetBakerMarkFieldArrayThunkDisplacement(Register reg, bool short_offset) {
  int num = GetBakerMarkThunkNumber(reg) +
      (short_offset ? BAKER_MARK_INTROSPECTION_REGISTER_COUNT : 0);
  return num * BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE;
}

static inline int GetBakerMarkGcRootThunkDisplacement(Register reg) {
  return GetBakerMarkThunkNumber(reg) * BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE +
      BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET;
}

void InstructionCodeGeneratorMIPS::GenerateGcRootFieldLoad(HInstruction* instruction,
                                                           Location root,
                                                           Register obj,
                                                           uint32_t offset,
                                                           ReadBarrierOption read_barrier_option,
                                                           MipsLabel* label_low) {
  bool reordering;
  if (label_low != nullptr) {
    DCHECK_EQ(offset, 0x5678u);
  }
  Register root_reg = root.AsRegister<Register>();
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

        bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
        const int32_t entry_point_offset =
            Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(0);
        const int thunk_disp = GetBakerMarkGcRootThunkDisplacement(root_reg);
        int16_t offset_low = Low16Bits(offset);
        int16_t offset_high = High16Bits(offset - offset_low);  // Accounts for sign
                                                                // extension in lw.
        bool short_offset = IsInt<16>(static_cast<int32_t>(offset));
        Register base = short_offset ? obj : TMP;
        // Loading the entrypoint does not require a load acquire since it is only changed when
        // threads are suspended or running a checkpoint.
        __ LoadFromOffset(kLoadWord, T9, TR, entry_point_offset);
        reordering = __ SetReorder(false);
        if (!short_offset) {
          DCHECK(!label_low);
          __ AddUpper(base, obj, offset_high);
        }
        MipsLabel skip_call;
        __ Beqz(T9, &skip_call, /* is_bare */ true);
        if (label_low != nullptr) {
          DCHECK(short_offset);
          __ Bind(label_low);
        }
        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        __ LoadFromOffset(kLoadWord, root_reg, base, offset_low);  // Single instruction
                                                                   // in delay slot.
        if (isR6) {
          __ Jialc(T9, thunk_disp);
        } else {
          __ Addiu(T9, T9, thunk_disp);
          __ Jalr(T9);
          __ Nop();
        }
        __ Bind(&skip_call);
        __ SetReorder(reordering);
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
          reordering = __ SetReorder(false);
          __ Bind(label_low);
        }
        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        __ LoadFromOffset(kLoadWord, root_reg, obj, offset);
        if (label_low != nullptr) {
          __ SetReorder(reordering);
        }
        static_assert(
            sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(GcRoot<mirror::Object>),
            "art::mirror::CompressedReference<mirror::Object> and art::GcRoot<mirror::Object> "
            "have different sizes.");
        static_assert(sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(int32_t),
                      "art::mirror::CompressedReference<mirror::Object> and int32_t "
                      "have different sizes.");

        // Slow path marking the GC root `root`.
        Location temp = Location::RegisterLocation(T9);
        SlowPathCodeMIPS* slow_path =
            new (codegen_->GetScopedAllocator()) ReadBarrierMarkSlowPathMIPS(
                instruction,
                root,
                /*entrypoint*/ temp);
        codegen_->AddSlowPath(slow_path);

        const int32_t entry_point_offset =
            Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(root.reg() - 1);
        // Loading the entrypoint does not require a load acquire since it is only changed when
        // threads are suspended or running a checkpoint.
        __ LoadFromOffset(kLoadWord, temp.AsRegister<Register>(), TR, entry_point_offset);
        __ Bnez(temp.AsRegister<Register>(), slow_path->GetEntryLabel());
        __ Bind(slow_path->GetExitLabel());
      }
    } else {
      if (label_low != nullptr) {
        reordering = __ SetReorder(false);
        __ Bind(label_low);
      }
      // GC root loaded through a slow path for read barriers other
      // than Baker's.
      // /* GcRoot<mirror::Object>* */ root = obj + offset
      __ Addiu32(root_reg, obj, offset);
      if (label_low != nullptr) {
        __ SetReorder(reordering);
      }
      // /* mirror::Object* */ root = root->Read()
      codegen_->GenerateReadBarrierForRootSlow(instruction, root, root);
    }
  } else {
    if (label_low != nullptr) {
      reordering = __ SetReorder(false);
      __ Bind(label_low);
    }
    // Plain GC root load with no read barrier.
    // /* GcRoot<mirror::Object> */ root = *(obj + offset)
    __ LoadFromOffset(kLoadWord, root_reg, obj, offset);
    // Note that GC roots are not affected by heap poisoning, thus we
    // do not have to unpoison `root_reg` here.
    if (label_low != nullptr) {
      __ SetReorder(reordering);
    }
  }
}

void CodeGeneratorMIPS::GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                                              Location ref,
                                                              Register obj,
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
    bool isR6 = GetInstructionSetFeatures().IsR6();
    int16_t offset_low = Low16Bits(offset);
    int16_t offset_high = High16Bits(offset - offset_low);  // Accounts for sign extension in lw.
    bool short_offset = IsInt<16>(static_cast<int32_t>(offset));
    bool reordering = __ SetReorder(false);
    const int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(0);
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
    __ LoadFromOffset(kLoadWord, T9, TR, entry_point_offset);
    Register ref_reg = ref.AsRegister<Register>();
    Register base = short_offset ? obj : TMP;
    MipsLabel skip_call;
    if (short_offset) {
      if (isR6) {
        __ Beqzc(T9, &skip_call, /* is_bare */ true);
        __ Nop();  // In forbidden slot.
        __ Jialc(T9, thunk_disp);
      } else {
        __ Beqz(T9, &skip_call, /* is_bare */ true);
        __ Addiu(T9, T9, thunk_disp);  // In delay slot.
        __ Jalr(T9);
        __ Nop();  // In delay slot.
      }
      __ Bind(&skip_call);
    } else {
      if (isR6) {
        __ Beqz(T9, &skip_call, /* is_bare */ true);
        __ Aui(base, obj, offset_high);  // In delay slot.
        __ Jialc(T9, thunk_disp);
        __ Bind(&skip_call);
      } else {
        __ Lui(base, offset_high);
        __ Beqz(T9, &skip_call, /* is_bare */ true);
        __ Addiu(T9, T9, thunk_disp);  // In delay slot.
        __ Jalr(T9);
        __ Bind(&skip_call);
        __ Addu(base, base, obj);  // In delay slot.
      }
    }
    // /* HeapReference<Object> */ ref = *(obj + offset)
    __ LoadFromOffset(kLoadWord, ref_reg, base, offset_low);  // Single instruction.
    if (needs_null_check) {
      MaybeRecordImplicitNullCheck(instruction);
    }
    __ MaybeUnpoisonHeapReference(ref_reg);
    __ SetReorder(reordering);
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

void CodeGeneratorMIPS::GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                                              Location ref,
                                                              Register obj,
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
    bool reordering = __ SetReorder(false);
    const int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kMipsPointerSize>(0);
    // We will not do the explicit null check in the thunk as some form of a null check
    // must've been done earlier.
    DCHECK(!needs_null_check);
    const int thunk_disp = GetBakerMarkFieldArrayThunkDisplacement(obj, /* short_offset */ false);
    // Loading the entrypoint does not require a load acquire since it is only changed when
    // threads are suspended or running a checkpoint.
    __ LoadFromOffset(kLoadWord, T9, TR, entry_point_offset);
    Register ref_reg = ref.AsRegister<Register>();
    Register index_reg = index.IsRegisterPair()
        ? index.AsRegisterPairLow<Register>()
        : index.AsRegister<Register>();
    MipsLabel skip_call;
    if (GetInstructionSetFeatures().IsR6()) {
      __ Beqz(T9, &skip_call, /* is_bare */ true);
      __ Lsa(TMP, index_reg, obj, scale_factor);  // In delay slot.
      __ Jialc(T9, thunk_disp);
      __ Bind(&skip_call);
    } else {
      __ Sll(TMP, index_reg, scale_factor);
      __ Beqz(T9, &skip_call, /* is_bare */ true);
      __ Addiu(T9, T9, thunk_disp);  // In delay slot.
      __ Jalr(T9);
      __ Bind(&skip_call);
      __ Addu(TMP, TMP, obj);  // In delay slot.
    }
    // /* HeapReference<Object> */ ref = *(obj + data_offset + (index << scale_factor))
    DCHECK(IsInt<16>(static_cast<int32_t>(data_offset))) << data_offset;
    __ LoadFromOffset(kLoadWord, ref_reg, TMP, data_offset);  // Single instruction.
    __ MaybeUnpoisonHeapReference(ref_reg);
    __ SetReorder(reordering);
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

void CodeGeneratorMIPS::GenerateReferenceLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                  Location ref,
                                                                  Register obj,
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

  Register ref_reg = ref.AsRegister<Register>();
  Register temp_reg = temp.AsRegister<Register>();
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
      __ LoadFromOffset(kLoadWord, ref_reg, obj, computed_offset);
    } else {
      // Handle the special case of the
      // UnsafeGetObject/UnsafeGetObjectVolatile and UnsafeCASObject
      // intrinsics, which use a register pair as index ("long
      // offset"), of which only the low part contains data.
      Register index_reg = index.IsRegisterPair()
          ? index.AsRegisterPairLow<Register>()
          : index.AsRegister<Register>();
      __ ShiftAndAdd(TMP, index_reg, obj, scale_factor, TMP);
      __ LoadFromOffset(kLoadWord, ref_reg, TMP, offset);
    }
  } else {
    // /* HeapReference<Object> */ ref = *(obj + offset)
    __ LoadFromOffset(kLoadWord, ref_reg, obj, offset);
  }

  // Object* ref = ref_addr->AsMirrorPtr()
  __ MaybeUnpoisonHeapReference(ref_reg);

  // Slow path marking the object `ref` when it is gray.
  SlowPathCodeMIPS* slow_path;
  if (always_update_field) {
    // ReadBarrierMarkAndUpdateFieldSlowPathMIPS only supports address
    // of the form `obj + field_offset`, where `obj` is a register and
    // `field_offset` is a register pair (of which only the lower half
    // is used). Thus `offset` and `scale_factor` above are expected
    // to be null in this code path.
    DCHECK_EQ(offset, 0u);
    DCHECK_EQ(scale_factor, ScaleFactor::TIMES_1);
    slow_path = new (GetScopedAllocator())
        ReadBarrierMarkAndUpdateFieldSlowPathMIPS(instruction,
                                                  ref,
                                                  obj,
                                                  /* field_offset */ index,
                                                  temp_reg);
  } else {
    slow_path = new (GetScopedAllocator()) ReadBarrierMarkSlowPathMIPS(instruction, ref);
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
  __ Bltz(temp_reg, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorMIPS::GenerateReadBarrierSlow(HInstruction* instruction,
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
  SlowPathCodeMIPS* slow_path = new (GetScopedAllocator())
      ReadBarrierForHeapReferenceSlowPathMIPS(instruction, out, ref, obj, offset, index);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorMIPS::MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                                     Location out,
                                                     Location ref,
                                                     Location obj,
                                                     uint32_t offset,
                                                     Location index) {
  if (kEmitCompilerReadBarrier) {
    // Baker's read barriers shall be handled by the fast path
    // (CodeGeneratorMIPS::GenerateReferenceLoadWithBakerReadBarrier).
    DCHECK(!kUseBakerReadBarrier);
    // If heap poisoning is enabled, unpoisoning will be taken care of
    // by the runtime within the slow path.
    GenerateReadBarrierSlow(instruction, out, ref, obj, offset, index);
  } else if (kPoisonHeapReferences) {
    __ UnpoisonHeapReference(out.AsRegister<Register>());
  }
}

void CodeGeneratorMIPS::GenerateReadBarrierForRootSlow(HInstruction* instruction,
                                                       Location out,
                                                       Location root) {
  DCHECK(kEmitCompilerReadBarrier);

  // Insert a slow path based read barrier *after* the GC root load.
  //
  // Note that GC roots are not affected by heap poisoning, so we do
  // not need to do anything special for this here.
  SlowPathCodeMIPS* slow_path =
      new (GetScopedAllocator()) ReadBarrierForRootSlowPathMIPS(instruction, out, root);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void LocationsBuilderMIPS::VisitInstanceOf(HInstanceOf* instruction) {
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
  // Note that TypeCheckSlowPathMIPS uses this register too.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
  locations->AddRegisterTemps(NumberOfInstanceOfTemps(type_check_kind));
}

void InstructionCodeGeneratorMIPS::VisitInstanceOf(HInstanceOf* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = obj_loc.AsRegister<Register>();
  Register cls = locations->InAt(1).AsRegister<Register>();
  Location out_loc = locations->Out();
  Register out = out_loc.AsRegister<Register>();
  const size_t num_temps = NumberOfInstanceOfTemps(type_check_kind);
  DCHECK_LE(num_temps, 1u);
  Location maybe_temp_loc = (num_temps >= 1) ? locations->GetTemp(0) : Location::NoLocation();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  MipsLabel done;
  SlowPathCodeMIPS* slow_path = nullptr;

  // Return 0 if `obj` is null.
  // Avoid this check if we know `obj` is not null.
  if (instruction->MustDoNullCheck()) {
    __ Move(out, ZERO);
    __ Beqz(obj, &done);
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
      MipsLabel loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Beqz(out, &done);
      __ Bne(out, cls, &loop);
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
      MipsLabel loop, success;
      __ Bind(&loop);
      __ Beq(out, cls, &success);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      __ Bnez(out, &loop);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ B(&done);
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
      MipsLabel success;
      __ Beq(out, cls, &success);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ out = out->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       component_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Beqz(out, &done);
      __ LoadFromOffset(kLoadUnsignedHalfword, out, out, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Sltiu(out, out, 1);
      __ B(&done);
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
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ Bne(out, cls, slow_path->GetEntryLabel());
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
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathMIPS(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      break;
    }
  }

  __ Bind(&done);

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void LocationsBuilderMIPS::VisitIntConstant(HIntConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitIntConstant(HIntConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitNullConstant(HNullConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitNullConstant(HNullConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::HandleInvoke(HInvoke* invoke) {
  InvokeDexCallingConventionVisitorMIPS calling_convention_visitor;
  CodeGenerator::CreateCommonInvokeLocationSummary(invoke, &calling_convention_visitor);
}

void LocationsBuilderMIPS::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
  // The register T7 is required to be used for the hidden argument in
  // art_quick_imt_conflict_trampoline, so add the hidden argument.
  invoke->GetLocations()->AddTemp(Location::RegisterLocation(T7));
}

void InstructionCodeGeneratorMIPS::VisitInvokeInterface(HInvokeInterface* invoke) {
  // TODO: b/18116999, our IMTs can miss an IncompatibleClassChangeError.
  Register temp = invoke->GetLocations()->GetTemp(0).AsRegister<Register>();
  Location receiver = invoke->GetLocations()->InAt(0);
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsPointerSize);

  // temp = object->GetClass();
  if (receiver.IsStackSlot()) {
    __ LoadFromOffset(kLoadWord, temp, SP, receiver.GetStackIndex());
    __ LoadFromOffset(kLoadWord, temp, temp, class_offset);
  } else {
    __ LoadFromOffset(kLoadWord, temp, receiver.AsRegister<Register>(), class_offset);
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
  __ LoadFromOffset(kLoadWord, temp, temp,
      mirror::Class::ImtPtrOffset(kMipsPointerSize).Uint32Value());
  uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
      invoke->GetImtIndex(), kMipsPointerSize));
  // temp = temp->GetImtEntryAt(method_offset);
  __ LoadFromOffset(kLoadWord, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadWord, T9, temp, entry_point.Int32Value());
  // Set the hidden argument.
  __ LoadConst32(invoke->GetLocations()->GetTemp(1).AsRegister<Register>(),
                 invoke->GetDexMethodIndex());
  // T9();
  __ Jalr(T9);
  __ NopIfNoReordering();
  DCHECK(!codegen_->IsLeafMethod());
  codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
}

void LocationsBuilderMIPS::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  IntrinsicLocationsBuilderMIPS intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

void LocationsBuilderMIPS::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  bool is_r6 = codegen_->GetInstructionSetFeatures().IsR6();
  bool has_irreducible_loops = codegen_->GetGraph()->HasIrreducibleLoops();
  bool has_extra_input = invoke->HasPcRelativeMethodLoadKind() && !is_r6 && !has_irreducible_loops;

  IntrinsicLocationsBuilderMIPS intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    if (invoke->GetLocations()->CanCall() && has_extra_input) {
      invoke->GetLocations()->SetInAt(invoke->GetSpecialInputIndex(), Location::Any());
    }
    return;
  }

  HandleInvoke(invoke);

  // Add the extra input register if either the dex cache array base register
  // or the PC-relative base register for accessing literals is needed.
  if (has_extra_input) {
    invoke->GetLocations()->SetInAt(invoke->GetSpecialInputIndex(), Location::RequiresRegister());
  }
}

void LocationsBuilderMIPS::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorMIPS::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  codegen_->GenerateInvokePolymorphicCall(invoke);
}

static bool TryGenerateIntrinsicCode(HInvoke* invoke, CodeGeneratorMIPS* codegen) {
  if (invoke->GetLocations()->Intrinsified()) {
    IntrinsicCodeGeneratorMIPS intrinsic(codegen);
    intrinsic.Dispatch(invoke);
    return true;
  }
  return false;
}

HLoadString::LoadKind CodeGeneratorMIPS::GetSupportedLoadStringKind(
    HLoadString::LoadKind desired_string_load_kind) {
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
  return desired_string_load_kind;
}

HLoadClass::LoadKind CodeGeneratorMIPS::GetSupportedLoadClassKind(
    HLoadClass::LoadKind desired_class_load_kind) {
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
  return desired_class_load_kind;
}

Register CodeGeneratorMIPS::GetInvokeStaticOrDirectExtraParameter(HInvokeStaticOrDirect* invoke,
                                                                  Register temp) {
  CHECK(!GetInstructionSetFeatures().IsR6());
  CHECK(!GetGraph()->HasIrreducibleLoops());
  CHECK_EQ(invoke->InputCount(), invoke->GetNumberOfArguments() + 1u);
  Location location = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
  if (!invoke->GetLocations()->Intrinsified()) {
    return location.AsRegister<Register>();
  }
  // For intrinsics we allow any location, so it may be on the stack.
  if (!location.IsRegister()) {
    __ LoadFromOffset(kLoadWord, temp, SP, location.GetStackIndex());
    return temp;
  }
  // For register locations, check if the register was saved. If so, get it from the stack.
  // Note: There is a chance that the register was saved but not overwritten, so we could
  // save one load. However, since this is just an intrinsic slow path we prefer this
  // simple and more robust approach rather that trying to determine if that's the case.
  SlowPathCode* slow_path = GetCurrentSlowPath();
  DCHECK(slow_path != nullptr);  // For intrinsified invokes the call is emitted on the slow path.
  if (slow_path->IsCoreRegisterSaved(location.AsRegister<Register>())) {
    int stack_offset = slow_path->GetStackOffsetOfCoreRegister(location.AsRegister<Register>());
    __ LoadFromOffset(kLoadWord, temp, SP, stack_offset);
    return temp;
  }
  return location.AsRegister<Register>();
}

HInvokeStaticOrDirect::DispatchInfo CodeGeneratorMIPS::GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
  return desired_dispatch_info;
}

void CodeGeneratorMIPS::GenerateStaticOrDirectCall(
    HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path) {
  // All registers are assumed to be correctly set up per the calling convention.
  Location callee_method = temp;  // For all kinds except kRecursive, callee will be in temp.
  HInvokeStaticOrDirect::MethodLoadKind method_load_kind = invoke->GetMethodLoadKind();
  HInvokeStaticOrDirect::CodePtrLocation code_ptr_location = invoke->GetCodePtrLocation();
  bool is_r6 = GetInstructionSetFeatures().IsR6();
  bool has_irreducible_loops = GetGraph()->HasIrreducibleLoops();
  Register base_reg = (invoke->HasPcRelativeMethodLoadKind() && !is_r6 && !has_irreducible_loops)
      ? GetInvokeStaticOrDirectExtraParameter(invoke, temp.AsRegister<Register>())
      : ZERO;

  switch (method_load_kind) {
    case HInvokeStaticOrDirect::MethodLoadKind::kStringInit: {
      // temp = thread->string_init_entrypoint
      uint32_t offset =
          GetThreadOffset<kMipsPointerSize>(invoke->GetStringInitEntryPoint()).Int32Value();
      __ LoadFromOffset(kLoadWord,
                        temp.AsRegister<Register>(),
                        TR,
                        offset);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRecursive:
      callee_method = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(GetCompilerOptions().IsBootImage());
      PcRelativePatchInfo* info_high = NewBootImageMethodPatch(invoke->GetTargetMethod());
      PcRelativePatchInfo* info_low =
          NewBootImageMethodPatch(invoke->GetTargetMethod(), info_high);
      Register temp_reg = temp.AsRegister<Register>();
      EmitPcRelativeAddressPlaceholderHigh(info_high, TMP, base_reg);
      __ Addiu(temp_reg, TMP, /* placeholder */ 0x5678, &info_low->label);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kDirectAddress:
      __ LoadConst32(temp.AsRegister<Register>(), invoke->GetMethodAddress());
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBssEntry: {
      PcRelativePatchInfo* info_high = NewMethodBssEntryPatch(
          MethodReference(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex()));
      PcRelativePatchInfo* info_low = NewMethodBssEntryPatch(
          MethodReference(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex()), info_high);
      Register temp_reg = temp.AsRegister<Register>();
      EmitPcRelativeAddressPlaceholderHigh(info_high, TMP, base_reg);
      __ Lw(temp_reg, TMP, /* placeholder */ 0x5678, &info_low->label);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRuntimeCall: {
      GenerateInvokeStaticOrDirectRuntimeCall(invoke, temp, slow_path);
      return;  // No code pointer retrieval; the runtime performs the call directly.
    }
  }

  switch (code_ptr_location) {
    case HInvokeStaticOrDirect::CodePtrLocation::kCallSelf:
      __ Bal(&frame_entry_label_);
      break;
    case HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod:
      // T9 = callee_method->entry_point_from_quick_compiled_code_;
      __ LoadFromOffset(kLoadWord,
                        T9,
                        callee_method.AsRegister<Register>(),
                        ArtMethod::EntryPointFromQuickCompiledCodeOffset(
                            kMipsPointerSize).Int32Value());
      // T9()
      __ Jalr(T9);
      __ NopIfNoReordering();
      break;
  }
  RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);

  DCHECK(!IsLeafMethod());
}

void InstructionCodeGeneratorMIPS::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
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

void CodeGeneratorMIPS::GenerateVirtualCall(
    HInvokeVirtual* invoke, Location temp_location, SlowPathCode* slow_path) {
  // Use the calling convention instead of the location of the receiver, as
  // intrinsics may have put the receiver in a different register. In the intrinsics
  // slow path, the arguments have been moved to the right place, so here we are
  // guaranteed that the receiver is the first register of the calling convention.
  InvokeDexCallingConvention calling_convention;
  Register receiver = calling_convention.GetRegisterAt(0);

  Register temp = temp_location.AsRegister<Register>();
  size_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), kMipsPointerSize).SizeValue();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsPointerSize);

  // temp = object->GetClass();
  __ LoadFromOffset(kLoadWord, temp, receiver, class_offset);
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
  __ LoadFromOffset(kLoadWord, temp, temp, method_offset);
  // T9 = temp->GetEntryPoint();
  __ LoadFromOffset(kLoadWord, T9, temp, entry_point.Int32Value());
  // T9();
  __ Jalr(T9);
  __ NopIfNoReordering();
  RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
}

void InstructionCodeGeneratorMIPS::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    return;
  }

  codegen_->GenerateVirtualCall(invoke, invoke->GetLocations()->GetTemp(0));
  DCHECK(!codegen_->IsLeafMethod());
}

void LocationsBuilderMIPS::VisitLoadClass(HLoadClass* cls) {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConvention calling_convention;
    Location loc = Location::RegisterLocation(calling_convention.GetRegisterAt(0));
    CodeGenerator::CreateLoadClassRuntimeCallLocationSummary(cls, loc, loc);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());
  const bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  const bool has_irreducible_loops = codegen_->GetGraph()->HasIrreducibleLoops();
  const bool requires_read_barrier = kEmitCompilerReadBarrier && !cls->IsInBootImage();
  LocationSummary::CallKind call_kind = (cls->NeedsEnvironment() || requires_read_barrier)
      ? LocationSummary::kCallOnSlowPath
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(cls, call_kind);
  if (kUseBakerReadBarrier && requires_read_barrier && !cls->NeedsEnvironment()) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  switch (load_kind) {
    // We need an extra register for PC-relative literals on R2.
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadClass::LoadKind::kBootImageAddress:
    case HLoadClass::LoadKind::kBootImageClassTable:
    case HLoadClass::LoadKind::kBssEntry:
      if (isR6) {
        break;
      }
      if (has_irreducible_loops) {
        if (load_kind != HLoadClass::LoadKind::kBootImageAddress) {
          codegen_->ClobberRA();
        }
        break;
      }
      FALLTHROUGH_INTENDED;
    case HLoadClass::LoadKind::kReferrersClass:
      locations->SetInAt(0, Location::RequiresRegister());
      break;
    default:
      break;
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
void InstructionCodeGeneratorMIPS::VisitLoadClass(HLoadClass* cls) NO_THREAD_SAFETY_ANALYSIS {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    codegen_->GenerateLoadClassRuntimeCall(cls);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());

  LocationSummary* locations = cls->GetLocations();
  Location out_loc = locations->Out();
  Register out = out_loc.AsRegister<Register>();
  Register base_or_current_method_reg;
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  bool has_irreducible_loops = GetGraph()->HasIrreducibleLoops();
  switch (load_kind) {
    // We need an extra register for PC-relative literals on R2.
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadClass::LoadKind::kBootImageAddress:
    case HLoadClass::LoadKind::kBootImageClassTable:
    case HLoadClass::LoadKind::kBssEntry:
      base_or_current_method_reg =
          (isR6 || has_irreducible_loops) ? ZERO : locations->InAt(0).AsRegister<Register>();
      break;
    case HLoadClass::LoadKind::kReferrersClass:
    case HLoadClass::LoadKind::kRuntimeCall:
      base_or_current_method_reg = locations->InAt(0).AsRegister<Register>();
      break;
    default:
      base_or_current_method_reg = ZERO;
      break;
  }

  const ReadBarrierOption read_barrier_option = cls->IsInBootImage()
      ? kWithoutReadBarrier
      : kCompilerReadBarrierOption;
  bool generate_null_check = false;
  switch (load_kind) {
    case HLoadClass::LoadKind::kReferrersClass: {
      DCHECK(!cls->CanCallRuntime());
      DCHECK(!cls->MustGenerateClinitCheck());
      // /* GcRoot<mirror::Class> */ out = current_method->declaring_class_
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              base_or_current_method_reg,
                              ArtMethod::DeclaringClassOffset().Int32Value(),
                              read_barrier_option);
      break;
    }
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      CodeGeneratorMIPS::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high,
                                                     out,
                                                     base_or_current_method_reg);
      __ Addiu(out, out, /* placeholder */ 0x5678, &info_low->label);
      break;
    }
    case HLoadClass::LoadKind::kBootImageAddress: {
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(cls->GetClass().Get()));
      DCHECK_NE(address, 0u);
      if (isR6 || !has_irreducible_loops) {
        __ LoadLiteral(out,
                       base_or_current_method_reg,
                       codegen_->DeduplicateBootImageAddressLiteral(address));
      } else {
        __ LoadConst32(out, address);
      }
      break;
    }
    case HLoadClass::LoadKind::kBootImageClassTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high,
                                                     out,
                                                     base_or_current_method_reg);
      __ Lw(out, out, /* placeholder */ 0x5678, &info_low->label);
      // Extract the reference from the slot data, i.e. clear the hash bits.
      int32_t masked_hash = ClassTable::TableSlot::MaskHash(
          ComputeModifiedUtf8Hash(cls->GetDexFile().StringByTypeIdx(cls->GetTypeIndex())));
      if (masked_hash != 0) {
        __ Addiu(out, out, -masked_hash);
      }
      break;
    }
    case HLoadClass::LoadKind::kBssEntry: {
      CodeGeneratorMIPS::PcRelativePatchInfo* bss_info_high =
          codegen_->NewTypeBssEntryPatch(cls->GetDexFile(), cls->GetTypeIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewTypeBssEntryPatch(cls->GetDexFile(), cls->GetTypeIndex(), bss_info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(bss_info_high,
                                                     out,
                                                     base_or_current_method_reg);
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              read_barrier_option,
                              &info_low->label);
      generate_null_check = true;
      break;
    }
    case HLoadClass::LoadKind::kJitTableAddress: {
      CodeGeneratorMIPS::JitPatchInfo* info = codegen_->NewJitRootClassPatch(cls->GetDexFile(),
                                                                             cls->GetTypeIndex(),
                                                                             cls->GetClass());
      bool reordering = __ SetReorder(false);
      __ Bind(&info->high_label);
      __ Lui(out, /* placeholder */ 0x1234);
      __ SetReorder(reordering);
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              read_barrier_option,
                              &info->low_label);
      break;
    }
    case HLoadClass::LoadKind::kRuntimeCall:
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }

  if (generate_null_check || cls->MustGenerateClinitCheck()) {
    DCHECK(cls->CanCallRuntime());
    SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathMIPS(
        cls, cls, cls->GetDexPc(), cls->MustGenerateClinitCheck());
    codegen_->AddSlowPath(slow_path);
    if (generate_null_check) {
      __ Beqz(out, slow_path->GetEntryLabel());
    }
    if (cls->MustGenerateClinitCheck()) {
      GenerateClassInitializationCheck(slow_path, out);
    } else {
      __ Bind(slow_path->GetExitLabel());
    }
  }
}

static int32_t GetExceptionTlsOffset() {
  return Thread::ExceptionOffset<kMipsPointerSize>().Int32Value();
}

void LocationsBuilderMIPS::VisitLoadException(HLoadException* load) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(load, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitLoadException(HLoadException* load) {
  Register out = load->GetLocations()->Out().AsRegister<Register>();
  __ LoadFromOffset(kLoadWord, out, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS::VisitClearException(HClearException* clear) {
  new (GetGraph()->GetAllocator()) LocationSummary(clear, LocationSummary::kNoCall);
}

void InstructionCodeGeneratorMIPS::VisitClearException(HClearException* clear ATTRIBUTE_UNUSED) {
  __ StoreToOffset(kStoreWord, ZERO, TR, GetExceptionTlsOffset());
}

void LocationsBuilderMIPS::VisitLoadString(HLoadString* load) {
  LocationSummary::CallKind call_kind = CodeGenerator::GetLoadStringCallKind(load);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(load, call_kind);
  HLoadString::LoadKind load_kind = load->GetLoadKind();
  const bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  const bool has_irreducible_loops = codegen_->GetGraph()->HasIrreducibleLoops();
  switch (load_kind) {
    // We need an extra register for PC-relative literals on R2.
    case HLoadString::LoadKind::kBootImageAddress:
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadString::LoadKind::kBootImageInternTable:
    case HLoadString::LoadKind::kBssEntry:
      if (isR6) {
        break;
      }
      if (has_irreducible_loops) {
        if (load_kind != HLoadString::LoadKind::kBootImageAddress) {
          codegen_->ClobberRA();
        }
        break;
      }
      FALLTHROUGH_INTENDED;
    // We need an extra register for PC-relative dex cache accesses.
    case HLoadString::LoadKind::kRuntimeCall:
      locations->SetInAt(0, Location::RequiresRegister());
      break;
    default:
      break;
  }
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
void InstructionCodeGeneratorMIPS::VisitLoadString(HLoadString* load) NO_THREAD_SAFETY_ANALYSIS {
  HLoadString::LoadKind load_kind = load->GetLoadKind();
  LocationSummary* locations = load->GetLocations();
  Location out_loc = locations->Out();
  Register out = out_loc.AsRegister<Register>();
  Register base_or_current_method_reg;
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();
  bool has_irreducible_loops = GetGraph()->HasIrreducibleLoops();
  switch (load_kind) {
    // We need an extra register for PC-relative literals on R2.
    case HLoadString::LoadKind::kBootImageAddress:
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative:
    case HLoadString::LoadKind::kBootImageInternTable:
    case HLoadString::LoadKind::kBssEntry:
      base_or_current_method_reg =
          (isR6 || has_irreducible_loops) ? ZERO : locations->InAt(0).AsRegister<Register>();
      break;
    default:
      base_or_current_method_reg = ZERO;
      break;
  }

  switch (load_kind) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high,
                                                     out,
                                                     base_or_current_method_reg);
      __ Addiu(out, out, /* placeholder */ 0x5678, &info_low->label);
      return;
    }
    case HLoadString::LoadKind::kBootImageAddress: {
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(load->GetString().Get()));
      DCHECK_NE(address, 0u);
      if (isR6 || !has_irreducible_loops) {
        __ LoadLiteral(out,
                       base_or_current_method_reg,
                       codegen_->DeduplicateBootImageAddressLiteral(address));
      } else {
        __ LoadConst32(out, address);
      }
      return;
    }
    case HLoadString::LoadKind::kBootImageInternTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_high =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high,
                                                     out,
                                                     base_or_current_method_reg);
      __ Lw(out, out, /* placeholder */ 0x5678, &info_low->label);
      return;
    }
    case HLoadString::LoadKind::kBssEntry: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_high =
          codegen_->NewStringBssEntryPatch(load->GetDexFile(), load->GetStringIndex());
      CodeGeneratorMIPS::PcRelativePatchInfo* info_low =
          codegen_->NewStringBssEntryPatch(load->GetDexFile(), load->GetStringIndex(), info_high);
      codegen_->EmitPcRelativeAddressPlaceholderHigh(info_high,
                                                     out,
                                                     base_or_current_method_reg);
      GenerateGcRootFieldLoad(load,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              kCompilerReadBarrierOption,
                              &info_low->label);
      SlowPathCodeMIPS* slow_path =
          new (codegen_->GetScopedAllocator()) LoadStringSlowPathMIPS(load);
      codegen_->AddSlowPath(slow_path);
      __ Beqz(out, slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
    case HLoadString::LoadKind::kJitTableAddress: {
      CodeGeneratorMIPS::JitPatchInfo* info =
          codegen_->NewJitRootStringPatch(load->GetDexFile(),
                                          load->GetStringIndex(),
                                          load->GetString());
      bool reordering = __ SetReorder(false);
      __ Bind(&info->high_label);
      __ Lui(out, /* placeholder */ 0x1234);
      __ SetReorder(reordering);
      GenerateGcRootFieldLoad(load,
                              out_loc,
                              out,
                              /* placeholder */ 0x5678,
                              kCompilerReadBarrierOption,
                              &info->low_label);
      return;
    }
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

void LocationsBuilderMIPS::VisitLongConstant(HLongConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorMIPS::VisitLongConstant(HLongConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderMIPS::VisitMonitorOperation(HMonitorOperation* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS::VisitMonitorOperation(HMonitorOperation* instruction) {
  if (instruction->IsEnter()) {
    codegen_->InvokeRuntime(kQuickLockObject, instruction, instruction->GetDexPc());
    CheckEntrypointTypes<kQuickLockObject, void, mirror::Object*>();
  } else {
    codegen_->InvokeRuntime(kQuickUnlockObject, instruction, instruction->GetDexPc());
  }
  CheckEntrypointTypes<kQuickUnlockObject, void, mirror::Object*>();
}

void LocationsBuilderMIPS::VisitMul(HMul* mul) {
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

void InstructionCodeGeneratorMIPS::VisitMul(HMul* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  switch (type) {
    case DataType::Type::kInt32: {
      Register dst = locations->Out().AsRegister<Register>();
      Register lhs = locations->InAt(0).AsRegister<Register>();
      Register rhs = locations->InAt(1).AsRegister<Register>();

      if (isR6) {
        __ MulR6(dst, lhs, rhs);
      } else {
        __ MulR2(dst, lhs, rhs);
      }
      break;
    }
    case DataType::Type::kInt64: {
      Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
      Register dst_low = locations->Out().AsRegisterPairLow<Register>();
      Register lhs_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register lhs_low = locations->InAt(0).AsRegisterPairLow<Register>();
      Register rhs_high = locations->InAt(1).AsRegisterPairHigh<Register>();
      Register rhs_low = locations->InAt(1).AsRegisterPairLow<Register>();

      // Extra checks to protect caused by the existance of A1_A2.
      // The algorithm is wrong if dst_high is either lhs_lo or rhs_lo:
      // (e.g. lhs=a0_a1, rhs=a2_a3 and dst=a1_a2).
      DCHECK_NE(dst_high, lhs_low);
      DCHECK_NE(dst_high, rhs_low);

      // A_B * C_D
      // dst_hi:  [ low(A*D) + low(B*C) + hi(B*D) ]
      // dst_lo:  [ low(B*D) ]
      // Note: R2 and R6 MUL produce the low 32 bit of the multiplication result.

      if (isR6) {
        __ MulR6(TMP, lhs_high, rhs_low);
        __ MulR6(dst_high, lhs_low, rhs_high);
        __ Addu(dst_high, dst_high, TMP);
        __ MuhuR6(TMP, lhs_low, rhs_low);
        __ Addu(dst_high, dst_high, TMP);
        __ MulR6(dst_low, lhs_low, rhs_low);
      } else {
        __ MulR2(TMP, lhs_high, rhs_low);
        __ MulR2(dst_high, lhs_low, rhs_high);
        __ Addu(dst_high, dst_high, TMP);
        __ MultuR2(lhs_low, rhs_low);
        __ Mfhi(TMP);
        __ Addu(dst_high, dst_high, TMP);
        __ Mflo(dst_low);
      }
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FRegister dst = locations->Out().AsFpuRegister<FRegister>();
      FRegister lhs = locations->InAt(0).AsFpuRegister<FRegister>();
      FRegister rhs = locations->InAt(1).AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ MulS(dst, lhs, rhs);
      } else {
        __ MulD(dst, lhs, rhs);
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected mul type " << type;
  }
}

void LocationsBuilderMIPS::VisitNeg(HNeg* neg) {
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

void InstructionCodeGeneratorMIPS::VisitNeg(HNeg* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32: {
      Register dst = locations->Out().AsRegister<Register>();
      Register src = locations->InAt(0).AsRegister<Register>();
      __ Subu(dst, ZERO, src);
      break;
    }
    case DataType::Type::kInt64: {
      Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
      Register dst_low = locations->Out().AsRegisterPairLow<Register>();
      Register src_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register src_low = locations->InAt(0).AsRegisterPairLow<Register>();
      __ Subu(dst_low, ZERO, src_low);
      __ Sltu(TMP, ZERO, dst_low);
      __ Subu(dst_high, ZERO, src_high);
      __ Subu(dst_high, dst_high, TMP);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FRegister dst = locations->Out().AsFpuRegister<FRegister>();
      FRegister src = locations->InAt(0).AsFpuRegister<FRegister>();
      if (type == DataType::Type::kFloat32) {
        __ NegS(dst, src);
      } else {
        __ NegD(dst, src);
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected neg type " << type;
  }
}

void LocationsBuilderMIPS::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
}

void InstructionCodeGeneratorMIPS::VisitNewArray(HNewArray* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes care
  // of poisoning the reference.
  QuickEntrypointEnum entrypoint =
      CodeGenerator::GetArrayAllocationEntrypoint(instruction->GetLoadClass()->GetClass());
  codegen_->InvokeRuntime(entrypoint, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickAllocArrayResolved, void*, mirror::Class*, int32_t>();
  DCHECK(!codegen_->IsLeafMethod());
}

void LocationsBuilderMIPS::VisitNewInstance(HNewInstance* instruction) {
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

void InstructionCodeGeneratorMIPS::VisitNewInstance(HNewInstance* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes care
  // of poisoning the reference.
  if (instruction->IsStringAlloc()) {
    // String is allocated through StringFactory. Call NewEmptyString entry point.
    Register temp = instruction->GetLocations()->GetTemp(0).AsRegister<Register>();
    MemberOffset code_offset = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kMipsPointerSize);
    __ LoadFromOffset(kLoadWord, temp, TR, QUICK_ENTRY_POINT(pNewEmptyString));
    __ LoadFromOffset(kLoadWord, T9, temp, code_offset.Int32Value());
    __ Jalr(T9);
    __ NopIfNoReordering();
    codegen_->RecordPcInfo(instruction, instruction->GetDexPc());
  } else {
    codegen_->InvokeRuntime(instruction->GetEntrypoint(), instruction, instruction->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
  }
}

void LocationsBuilderMIPS::VisitNot(HNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitNot(HNot* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32: {
      Register dst = locations->Out().AsRegister<Register>();
      Register src = locations->InAt(0).AsRegister<Register>();
      __ Nor(dst, src, ZERO);
      break;
    }

    case DataType::Type::kInt64: {
      Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
      Register dst_low = locations->Out().AsRegisterPairLow<Register>();
      Register src_high = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register src_low = locations->InAt(0).AsRegisterPairLow<Register>();
      __ Nor(dst_high, src_high, ZERO);
      __ Nor(dst_low, src_low, ZERO);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected type for not operation " << instruction->GetResultType();
  }
}

void LocationsBuilderMIPS::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorMIPS::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  __ Xori(locations->Out().AsRegister<Register>(),
          locations->InAt(0).AsRegister<Register>(),
          1);
}

void LocationsBuilderMIPS::VisitNullCheck(HNullCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
}

void CodeGeneratorMIPS::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (CanMoveNullCheckToUser(instruction)) {
    return;
  }
  Location obj = instruction->GetLocations()->InAt(0);

  __ Lw(ZERO, obj.AsRegister<Register>(), 0);
  RecordPcInfo(instruction, instruction->GetDexPc());
}

void CodeGeneratorMIPS::GenerateExplicitNullCheck(HNullCheck* instruction) {
  SlowPathCodeMIPS* slow_path = new (GetScopedAllocator()) NullCheckSlowPathMIPS(instruction);
  AddSlowPath(slow_path);

  Location obj = instruction->GetLocations()->InAt(0);

  __ Beqz(obj.AsRegister<Register>(), slow_path->GetEntryLabel());
}

void InstructionCodeGeneratorMIPS::VisitNullCheck(HNullCheck* instruction) {
  codegen_->GenerateNullCheck(instruction);
}

void LocationsBuilderMIPS::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS::VisitParallelMove(HParallelMove* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS::VisitParallelMove(HParallelMove* instruction) {
  if (instruction->GetNext()->IsSuspendCheck() &&
      instruction->GetBlock()->GetLoopInformation() != nullptr) {
    HSuspendCheck* suspend_check = instruction->GetNext()->AsSuspendCheck();
    // The back edge will generate the suspend check.
    codegen_->ClearSpillSlotsFromLoopPhisInStackMap(suspend_check, instruction);
  }

  codegen_->GetMoveResolver()->EmitNativeCode(instruction);
}

void LocationsBuilderMIPS::VisitParameterValue(HParameterValue* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  Location location = parameter_visitor_.GetNextLocation(instruction->GetType());
  if (location.IsStackSlot()) {
    location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  } else if (location.IsDoubleStackSlot()) {
    location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  }
  locations->SetOut(location);
}

void InstructionCodeGeneratorMIPS::VisitParameterValue(HParameterValue* instruction
                                                         ATTRIBUTE_UNUSED) {
  // Nothing to do, the parameter is already at its location.
}

void LocationsBuilderMIPS::VisitCurrentMethod(HCurrentMethod* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(Location::RegisterLocation(kMethodRegisterArgument));
}

void InstructionCodeGeneratorMIPS::VisitCurrentMethod(HCurrentMethod* instruction
                                                        ATTRIBUTE_UNUSED) {
  // Nothing to do, the method is already at its location.
}

void LocationsBuilderMIPS::VisitPhi(HPhi* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    locations->SetInAt(i, Location::Any());
  }
  locations->SetOut(Location::Any());
}

void InstructionCodeGeneratorMIPS::VisitPhi(HPhi* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS::VisitRem(HRem* rem) {
  DataType::Type type = rem->GetResultType();
  bool call_rem;
  if ((type == DataType::Type::kInt64) && rem->InputAt(1)->IsConstant()) {
    int64_t imm = CodeGenerator::GetInt64ValueOf(rem->InputAt(1)->AsConstant());
    call_rem = (imm != 0) && !IsPowerOfTwo(static_cast<uint64_t>(AbsOrMin(imm)));
  } else {
    call_rem = (type != DataType::Type::kInt32);
  }
  LocationSummary::CallKind call_kind = call_rem
      ? LocationSummary::kCallOnMainOnly
      : LocationSummary::kNoCall;
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(rem, call_kind);

  switch (type) {
    case DataType::Type::kInt32:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(rem->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kInt64: {
      if (call_rem) {
        InvokeRuntimeCallingConvention calling_convention;
        locations->SetInAt(0, Location::RegisterPairLocation(
            calling_convention.GetRegisterAt(0), calling_convention.GetRegisterAt(1)));
        locations->SetInAt(1, Location::RegisterPairLocation(
            calling_convention.GetRegisterAt(2), calling_convention.GetRegisterAt(3)));
        locations->SetOut(calling_convention.GetReturnLocation(type));
      } else {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::ConstantLocation(rem->InputAt(1)->AsConstant()));
        locations->SetOut(Location::RequiresRegister());
      }
      break;
    }

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

void InstructionCodeGeneratorMIPS::VisitRem(HRem* instruction) {
  DataType::Type type = instruction->GetType();
  LocationSummary* locations = instruction->GetLocations();

  switch (type) {
    case DataType::Type::kInt32:
      GenerateDivRemIntegral(instruction);
      break;
    case DataType::Type::kInt64: {
      if (locations->InAt(1).IsConstant()) {
        int64_t imm = locations->InAt(1).GetConstant()->AsLongConstant()->GetValue();
        if (imm == 0) {
          // Do not generate anything. DivZeroCheck would prevent any code to be executed.
        } else if (imm == 1 || imm == -1) {
          DivRemOneOrMinusOne(instruction);
        } else {
          DCHECK(IsPowerOfTwo(static_cast<uint64_t>(AbsOrMin(imm))));
          DivRemByPowerOfTwo(instruction);
        }
      } else {
        codegen_->InvokeRuntime(kQuickLmod, instruction, instruction->GetDexPc());
        CheckEntrypointTypes<kQuickLmod, int64_t, int64_t, int64_t>();
      }
      break;
    }
    case DataType::Type::kFloat32: {
      codegen_->InvokeRuntime(kQuickFmodf, instruction, instruction->GetDexPc());
      CheckEntrypointTypes<kQuickFmodf, float, float, float>();
      break;
    }
    case DataType::Type::kFloat64: {
      codegen_->InvokeRuntime(kQuickFmod, instruction, instruction->GetDexPc());
      CheckEntrypointTypes<kQuickFmod, double, double, double>();
      break;
    }
    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void LocationsBuilderMIPS::VisitConstructorFence(HConstructorFence* constructor_fence) {
  constructor_fence->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitConstructorFence(
    HConstructorFence* constructor_fence ATTRIBUTE_UNUSED) {
  GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
}

void LocationsBuilderMIPS::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  memory_barrier->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  GenerateMemoryBarrier(memory_barrier->GetBarrierKind());
}

void LocationsBuilderMIPS::VisitReturn(HReturn* ret) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(ret);
  DataType::Type return_type = ret->InputAt(0)->GetType();
  locations->SetInAt(0, MipsReturnLocation(return_type));
}

void InstructionCodeGeneratorMIPS::VisitReturn(HReturn* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS::VisitReturnVoid(HReturnVoid* ret) {
  ret->SetLocations(nullptr);
}

void InstructionCodeGeneratorMIPS::VisitReturnVoid(HReturnVoid* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderMIPS::VisitRor(HRor* ror) {
  HandleShift(ror);
}

void InstructionCodeGeneratorMIPS::VisitRor(HRor* ror) {
  HandleShift(ror);
}

void LocationsBuilderMIPS::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void InstructionCodeGeneratorMIPS::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void LocationsBuilderMIPS::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void InstructionCodeGeneratorMIPS::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void LocationsBuilderMIPS::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo(), instruction->GetDexPc());
}

void LocationsBuilderMIPS::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorMIPS::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction,
                 instruction->GetFieldInfo(),
                 instruction->GetDexPc(),
                 instruction->GetValueCanBeNull());
}

void LocationsBuilderMIPS::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(instruction,
                                                 instruction->GetFieldType(),
                                                 calling_convention);
}

void InstructionCodeGeneratorMIPS::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(instruction,
                                                 instruction->GetFieldType(),
                                                 calling_convention);
}

void InstructionCodeGeneratorMIPS::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(instruction,
                                                 instruction->GetFieldType(),
                                                 calling_convention);
}

void InstructionCodeGeneratorMIPS::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(instruction,
                                                 instruction->GetFieldType(),
                                                 calling_convention);
}

void InstructionCodeGeneratorMIPS::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionMIPS calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderMIPS::VisitSuspendCheck(HSuspendCheck* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnSlowPath);
  // In suspend check slow path, usually there are no caller-save registers at all.
  // If SIMD instructions are present, however, we force spilling all live SIMD
  // registers in full width (since the runtime only saves/restores lower part).
  locations->SetCustomSlowPathCallerSaves(
      GetGraph()->HasSIMD() ? RegisterSet::AllFpu() : RegisterSet::Empty());
}

void InstructionCodeGeneratorMIPS::VisitSuspendCheck(HSuspendCheck* instruction) {
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

void LocationsBuilderMIPS::VisitThrow(HThrow* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorMIPS::VisitThrow(HThrow* instruction) {
  codegen_->InvokeRuntime(kQuickDeliverException, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
}

void LocationsBuilderMIPS::VisitTypeConversion(HTypeConversion* conversion) {
  DataType::Type input_type = conversion->GetInputType();
  DataType::Type result_type = conversion->GetResultType();
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  if ((input_type == DataType::Type::kReference) || (input_type == DataType::Type::kVoid) ||
      (result_type == DataType::Type::kReference) || (result_type == DataType::Type::kVoid)) {
    LOG(FATAL) << "Unexpected type conversion from " << input_type << " to " << result_type;
  }

  LocationSummary::CallKind call_kind = LocationSummary::kNoCall;
  if (!isR6 &&
      ((DataType::IsFloatingPointType(result_type) && input_type == DataType::Type::kInt64) ||
       (result_type == DataType::Type::kInt64 && DataType::IsFloatingPointType(input_type)))) {
    call_kind = LocationSummary::kCallOnMainOnly;
  }

  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(conversion, call_kind);

  if (call_kind == LocationSummary::kNoCall) {
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
  } else {
    InvokeRuntimeCallingConvention calling_convention;

    if (DataType::IsFloatingPointType(input_type)) {
      locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
    } else {
      DCHECK_EQ(input_type, DataType::Type::kInt64);
      locations->SetInAt(0, Location::RegisterPairLocation(
                 calling_convention.GetRegisterAt(0), calling_convention.GetRegisterAt(1)));
    }

    locations->SetOut(calling_convention.GetReturnLocation(result_type));
  }
}

void InstructionCodeGeneratorMIPS::VisitTypeConversion(HTypeConversion* conversion) {
  LocationSummary* locations = conversion->GetLocations();
  DataType::Type result_type = conversion->GetResultType();
  DataType::Type input_type = conversion->GetInputType();
  bool has_sign_extension = codegen_->GetInstructionSetFeatures().IsMipsIsaRevGreaterThanEqual2();
  bool isR6 = codegen_->GetInstructionSetFeatures().IsR6();

  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;

  if (result_type == DataType::Type::kInt64 && DataType::IsIntegralType(input_type)) {
    Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
    Register dst_low = locations->Out().AsRegisterPairLow<Register>();
    Register src = locations->InAt(0).AsRegister<Register>();

    if (dst_low != src) {
      __ Move(dst_low, src);
    }
    __ Sra(dst_high, src, 31);
  } else if (DataType::IsIntegralType(result_type) && DataType::IsIntegralType(input_type)) {
    Register dst = locations->Out().AsRegister<Register>();
    Register src = (input_type == DataType::Type::kInt64)
        ? locations->InAt(0).AsRegisterPairLow<Register>()
        : locations->InAt(0).AsRegister<Register>();

    switch (result_type) {
      case DataType::Type::kUint8:
        __ Andi(dst, src, 0xFF);
        break;
      case DataType::Type::kInt8:
        if (has_sign_extension) {
          __ Seb(dst, src);
        } else {
          __ Sll(dst, src, 24);
          __ Sra(dst, dst, 24);
        }
        break;
      case DataType::Type::kUint16:
        __ Andi(dst, src, 0xFFFF);
        break;
      case DataType::Type::kInt16:
        if (has_sign_extension) {
          __ Seh(dst, src);
        } else {
          __ Sll(dst, src, 16);
          __ Sra(dst, dst, 16);
        }
        break;
      case DataType::Type::kInt32:
        if (dst != src) {
          __ Move(dst, src);
        }
        break;

      default:
        LOG(FATAL) << "Unexpected type conversion from " << input_type
                   << " to " << result_type;
    }
  } else if (DataType::IsFloatingPointType(result_type) && DataType::IsIntegralType(input_type)) {
    if (input_type == DataType::Type::kInt64) {
      if (isR6) {
        // cvt.s.l/cvt.d.l requires MIPSR2+ with FR=1. MIPS32R6 is implemented as a secondary
        // architecture on top of MIPS64R6, which has FR=1, and therefore can use the instruction.
        Register src_high = locations->InAt(0).AsRegisterPairHigh<Register>();
        Register src_low = locations->InAt(0).AsRegisterPairLow<Register>();
        FRegister dst = locations->Out().AsFpuRegister<FRegister>();
        __ Mtc1(src_low, FTMP);
        __ Mthc1(src_high, FTMP);
        if (result_type == DataType::Type::kFloat32) {
          __ Cvtsl(dst, FTMP);
        } else {
          __ Cvtdl(dst, FTMP);
        }
      } else {
        QuickEntrypointEnum entrypoint =
            (result_type == DataType::Type::kFloat32) ? kQuickL2f : kQuickL2d;
        codegen_->InvokeRuntime(entrypoint, conversion, conversion->GetDexPc());
        if (result_type == DataType::Type::kFloat32) {
          CheckEntrypointTypes<kQuickL2f, float, int64_t>();
        } else {
          CheckEntrypointTypes<kQuickL2d, double, int64_t>();
        }
      }
    } else {
      Register src = locations->InAt(0).AsRegister<Register>();
      FRegister dst = locations->Out().AsFpuRegister<FRegister>();
      __ Mtc1(src, FTMP);
      if (result_type == DataType::Type::kFloat32) {
        __ Cvtsw(dst, FTMP);
      } else {
        __ Cvtdw(dst, FTMP);
      }
    }
  } else if (DataType::IsIntegralType(result_type) && DataType::IsFloatingPointType(input_type)) {
    CHECK(result_type == DataType::Type::kInt32 || result_type == DataType::Type::kInt64);

    // When NAN2008=1 (R6), the truncate instruction caps the output at the minimum/maximum
    // value of the output type if the input is outside of the range after the truncation or
    // produces 0 when the input is a NaN. IOW, the three special cases produce three distinct
    // results. This matches the desired float/double-to-int/long conversion exactly.
    //
    // When NAN2008=0 (R2 and before), the truncate instruction produces the maximum positive
    // value when the input is either a NaN or is outside of the range of the output type
    // after the truncation. IOW, the three special cases (NaN, too small, too big) produce
    // the same result.
    //
    // The code takes care of the different behaviors by first comparing the input to the
    // minimum output value (-2**-63 for truncating to long, -2**-31 for truncating to int).
    // If the input is greater than or equal to the minimum, it procedes to the truncate
    // instruction, which will handle such an input the same way irrespective of NAN2008.
    // Otherwise the input is compared to itself to determine whether it is a NaN or not
    // in order to return either zero or the minimum value.
    if (result_type == DataType::Type::kInt64) {
      if (isR6) {
        // trunc.l.s/trunc.l.d requires MIPSR2+ with FR=1. MIPS32R6 is implemented as a secondary
        // architecture on top of MIPS64R6, which has FR=1, and therefore can use the instruction.
        FRegister src = locations->InAt(0).AsFpuRegister<FRegister>();
        Register dst_high = locations->Out().AsRegisterPairHigh<Register>();
        Register dst_low = locations->Out().AsRegisterPairLow<Register>();

        if (input_type == DataType::Type::kFloat32) {
          __ TruncLS(FTMP, src);
        } else {
          __ TruncLD(FTMP, src);
        }
        __ Mfc1(dst_low, FTMP);
        __ Mfhc1(dst_high, FTMP);
      } else {
        QuickEntrypointEnum entrypoint =
            (input_type == DataType::Type::kFloat32) ? kQuickF2l : kQuickD2l;
        codegen_->InvokeRuntime(entrypoint, conversion, conversion->GetDexPc());
        if (input_type == DataType::Type::kFloat32) {
          CheckEntrypointTypes<kQuickF2l, int64_t, float>();
        } else {
          CheckEntrypointTypes<kQuickD2l, int64_t, double>();
        }
      }
    } else {
      FRegister src = locations->InAt(0).AsFpuRegister<FRegister>();
      Register dst = locations->Out().AsRegister<Register>();
      MipsLabel truncate;
      MipsLabel done;

      if (!isR6) {
        if (input_type == DataType::Type::kFloat32) {
          uint32_t min_val = bit_cast<uint32_t, float>(std::numeric_limits<int32_t>::min());
          __ LoadConst32(TMP, min_val);
          __ Mtc1(TMP, FTMP);
        } else {
          uint64_t min_val = bit_cast<uint64_t, double>(std::numeric_limits<int32_t>::min());
          __ LoadConst32(TMP, High32Bits(min_val));
          __ Mtc1(ZERO, FTMP);
          __ MoveToFpuHigh(TMP, FTMP);
        }

        if (input_type == DataType::Type::kFloat32) {
          __ ColeS(0, FTMP, src);
        } else {
          __ ColeD(0, FTMP, src);
        }
        __ Bc1t(0, &truncate);

        if (input_type == DataType::Type::kFloat32) {
          __ CeqS(0, src, src);
        } else {
          __ CeqD(0, src, src);
        }
        __ LoadConst32(dst, std::numeric_limits<int32_t>::min());
        __ Movf(dst, ZERO, 0);

        __ B(&done);

        __ Bind(&truncate);
      }

      if (input_type == DataType::Type::kFloat32) {
        __ TruncWS(FTMP, src);
      } else {
        __ TruncWD(FTMP, src);
      }
      __ Mfc1(dst, FTMP);

      if (!isR6) {
        __ Bind(&done);
      }
    }
  } else if (DataType::IsFloatingPointType(result_type) &&
             DataType::IsFloatingPointType(input_type)) {
    FRegister dst = locations->Out().AsFpuRegister<FRegister>();
    FRegister src = locations->InAt(0).AsFpuRegister<FRegister>();
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

void LocationsBuilderMIPS::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void InstructionCodeGeneratorMIPS::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void LocationsBuilderMIPS::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorMIPS::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderMIPS::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderMIPS::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorMIPS::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderMIPS::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(switch_instr, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (!codegen_->GetInstructionSetFeatures().IsR6()) {
    uint32_t num_entries = switch_instr->GetNumEntries();
    if (num_entries > InstructionCodeGeneratorMIPS::kPackedSwitchJumpTableThreshold) {
      // When there's no HMipsComputeBaseMethodAddress input, R2 uses the NAL
      // instruction to simulate PC-relative addressing when accessing the jump table.
      // NAL clobbers RA. Make sure RA is preserved.
      codegen_->ClobberRA();
    }
  }
}

void InstructionCodeGeneratorMIPS::GenPackedSwitchWithCompares(Register value_reg,
                                                               int32_t lower_bound,
                                                               uint32_t num_entries,
                                                               HBasicBlock* switch_block,
                                                               HBasicBlock* default_block) {
  // Create a set of compare/jumps.
  Register temp_reg = TMP;
  __ Addiu32(temp_reg, value_reg, -lower_bound);
  // Jump to default if index is negative
  // Note: We don't check the case that index is positive while value < lower_bound, because in
  // this case, index >= num_entries must be true. So that we can save one branch instruction.
  __ Bltz(temp_reg, codegen_->GetLabelOf(default_block));

  const ArenaVector<HBasicBlock*>& successors = switch_block->GetSuccessors();
  // Jump to successors[0] if value == lower_bound.
  __ Beqz(temp_reg, codegen_->GetLabelOf(successors[0]));
  int32_t last_index = 0;
  for (; num_entries - last_index > 2; last_index += 2) {
    __ Addiu(temp_reg, temp_reg, -2);
    // Jump to successors[last_index + 1] if value < case_value[last_index + 2].
    __ Bltz(temp_reg, codegen_->GetLabelOf(successors[last_index + 1]));
    // Jump to successors[last_index + 2] if value == case_value[last_index + 2].
    __ Beqz(temp_reg, codegen_->GetLabelOf(successors[last_index + 2]));
  }
  if (num_entries - last_index == 2) {
    // The last missing case_value.
    __ Addiu(temp_reg, temp_reg, -1);
    __ Beqz(temp_reg, codegen_->GetLabelOf(successors[last_index + 1]));
  }

  // And the default for any other value.
  if (!codegen_->GoesToNextBlock(switch_block, default_block)) {
    __ B(codegen_->GetLabelOf(default_block));
  }
}

void InstructionCodeGeneratorMIPS::GenTableBasedPackedSwitch(Register value_reg,
                                                             Register constant_area,
                                                             int32_t lower_bound,
                                                             uint32_t num_entries,
                                                             HBasicBlock* switch_block,
                                                             HBasicBlock* default_block) {
  // Create a jump table.
  std::vector<MipsLabel*> labels(num_entries);
  const ArenaVector<HBasicBlock*>& successors = switch_block->GetSuccessors();
  for (uint32_t i = 0; i < num_entries; i++) {
    labels[i] = codegen_->GetLabelOf(successors[i]);
  }
  JumpTable* table = __ CreateJumpTable(std::move(labels));

  // Is the value in range?
  __ Addiu32(TMP, value_reg, -lower_bound);
  if (IsInt<16>(static_cast<int32_t>(num_entries))) {
    __ Sltiu(AT, TMP, num_entries);
    __ Beqz(AT, codegen_->GetLabelOf(default_block));
  } else {
    __ LoadConst32(AT, num_entries);
    __ Bgeu(TMP, AT, codegen_->GetLabelOf(default_block));
  }

  // We are in the range of the table.
  // Load the target address from the jump table, indexing by the value.
  __ LoadLabelAddress(AT, constant_area, table->GetLabel());
  __ ShiftAndAdd(TMP, TMP, AT, 2, TMP);
  __ Lw(TMP, TMP, 0);
  // Compute the absolute target address by adding the table start address
  // (the table contains offsets to targets relative to its start).
  __ Addu(TMP, TMP, AT);
  // And jump.
  __ Jr(TMP);
  __ NopIfNoReordering();
}

void InstructionCodeGeneratorMIPS::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  int32_t lower_bound = switch_instr->GetStartValue();
  uint32_t num_entries = switch_instr->GetNumEntries();
  LocationSummary* locations = switch_instr->GetLocations();
  Register value_reg = locations->InAt(0).AsRegister<Register>();
  HBasicBlock* switch_block = switch_instr->GetBlock();
  HBasicBlock* default_block = switch_instr->GetDefaultBlock();

  if (num_entries > kPackedSwitchJumpTableThreshold) {
    // R6 uses PC-relative addressing to access the jump table.
    //
    // R2, OTOH, uses an HMipsComputeBaseMethodAddress input (when available)
    // to access the jump table and it is implemented by changing HPackedSwitch to
    // HMipsPackedSwitch, which bears HMipsComputeBaseMethodAddress (see
    // VisitMipsPackedSwitch()).
    //
    // When there's no HMipsComputeBaseMethodAddress input (e.g. in presence of
    // irreducible loops), R2 uses the NAL instruction to simulate PC-relative
    // addressing.
    GenTableBasedPackedSwitch(value_reg,
                              ZERO,
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

void LocationsBuilderMIPS::VisitMipsPackedSwitch(HMipsPackedSwitch* switch_instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(switch_instr, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  // Constant area pointer (HMipsComputeBaseMethodAddress).
  locations->SetInAt(1, Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitMipsPackedSwitch(HMipsPackedSwitch* switch_instr) {
  int32_t lower_bound = switch_instr->GetStartValue();
  uint32_t num_entries = switch_instr->GetNumEntries();
  LocationSummary* locations = switch_instr->GetLocations();
  Register value_reg = locations->InAt(0).AsRegister<Register>();
  Register constant_area = locations->InAt(1).AsRegister<Register>();
  HBasicBlock* switch_block = switch_instr->GetBlock();
  HBasicBlock* default_block = switch_instr->GetDefaultBlock();

  // This is an R2-only path. HPackedSwitch has been changed to
  // HMipsPackedSwitch, which bears HMipsComputeBaseMethodAddress
  // required to address the jump table relative to PC.
  GenTableBasedPackedSwitch(value_reg,
                            constant_area,
                            lower_bound,
                            num_entries,
                            switch_block,
                            default_block);
}

void LocationsBuilderMIPS::VisitMipsComputeBaseMethodAddress(
    HMipsComputeBaseMethodAddress* insn) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(insn, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitMipsComputeBaseMethodAddress(
    HMipsComputeBaseMethodAddress* insn) {
  LocationSummary* locations = insn->GetLocations();
  Register reg = locations->Out().AsRegister<Register>();

  CHECK(!codegen_->GetInstructionSetFeatures().IsR6());

  // Generate a dummy PC-relative call to obtain PC.
  __ Nal();
  // Grab the return address off RA.
  __ Move(reg, RA);

  // Remember this offset (the obtained PC value) for later use with constant area.
  __ BindPcRelBaseLabel();
}

void LocationsBuilderMIPS::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  // The trampoline uses the same calling convention as dex calling conventions,
  // except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
  // the method_idx.
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorMIPS::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  codegen_->GenerateInvokeUnresolvedRuntimeCall(invoke);
}

void LocationsBuilderMIPS::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorMIPS::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (instruction->GetTableKind() == HClassTableGet::TableKind::kVTable) {
    uint32_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
        instruction->GetIndex(), kMipsPointerSize).SizeValue();
    __ LoadFromOffset(kLoadWord,
                      locations->Out().AsRegister<Register>(),
                      locations->InAt(0).AsRegister<Register>(),
                      method_offset);
  } else {
    uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
        instruction->GetIndex(), kMipsPointerSize));
    __ LoadFromOffset(kLoadWord,
                      locations->Out().AsRegister<Register>(),
                      locations->InAt(0).AsRegister<Register>(),
                      mirror::Class::ImtPtrOffset(kMipsPointerSize).Uint32Value());
    __ LoadFromOffset(kLoadWord,
                      locations->Out().AsRegister<Register>(),
                      locations->Out().AsRegister<Register>(),
                      method_offset);
  }
}

void LocationsBuilderMIPS::VisitIntermediateAddress(HIntermediateAddress* instruction
                                                    ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorMIPS::VisitIntermediateAddress(HIntermediateAddress* instruction
                                                            ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

#undef __
#undef QUICK_ENTRY_POINT

}  // namespace mips
}  // namespace art
