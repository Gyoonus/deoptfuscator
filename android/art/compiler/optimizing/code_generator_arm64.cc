/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "code_generator_arm64.h"

#include "arch/arm64/asm_support_arm64.h"
#include "arch/arm64/instruction_set_features_arm64.h"
#include "art_method.h"
#include "base/bit_utils.h"
#include "base/bit_utils_iterator.h"
#include "class_table.h"
#include "code_generator_utils.h"
#include "compiled_method.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gc/accounting/card_table.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "intrinsics_arm64.h"
#include "linker/arm64/relative_patcher_arm64.h"
#include "linker/linker_patch.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "offsets.h"
#include "thread.h"
#include "utils/arm64/assembler_arm64.h"
#include "utils/assembler.h"
#include "utils/stack_checks.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)
using vixl::ExactAssemblyScope;
using vixl::CodeBufferCheckScope;
using vixl::EmissionCheckScope;

#ifdef __
#error "ARM64 Codegen VIXL macro-assembler macro already defined."
#endif

namespace art {

template<class MirrorType>
class GcRoot;

namespace arm64 {

using helpers::ARM64EncodableConstantOrRegister;
using helpers::ArtVixlRegCodeCoherentForRegSet;
using helpers::CPURegisterFrom;
using helpers::DRegisterFrom;
using helpers::FPRegisterFrom;
using helpers::HeapOperand;
using helpers::HeapOperandFrom;
using helpers::InputCPURegisterAt;
using helpers::InputCPURegisterOrZeroRegAt;
using helpers::InputFPRegisterAt;
using helpers::InputOperandAt;
using helpers::InputRegisterAt;
using helpers::Int64ConstantFrom;
using helpers::IsConstantZeroBitPattern;
using helpers::LocationFrom;
using helpers::OperandFromMemOperand;
using helpers::OutputCPURegister;
using helpers::OutputFPRegister;
using helpers::OutputRegister;
using helpers::QRegisterFrom;
using helpers::RegisterFrom;
using helpers::StackOperandFrom;
using helpers::VIXLRegCodeFromART;
using helpers::WRegisterFrom;
using helpers::XRegisterFrom;

// The compare/jump sequence will generate about (1.5 * num_entries + 3) instructions. While jump
// table version generates 7 instructions and num_entries literals. Compare/jump sequence will
// generates less code/data with a small num_entries.
static constexpr uint32_t kPackedSwitchCompareJumpThreshold = 7;

// Reference load (except object array loads) is using LDR Wt, [Xn, #offset] which can handle
// offset < 16KiB. For offsets >= 16KiB, the load shall be emitted as two or more instructions.
// For the Baker read barrier implementation using link-generated thunks we need to split
// the offset explicitly.
constexpr uint32_t kReferenceLoadMinFarOffset = 16 * KB;

// Flags controlling the use of link-time generated thunks for Baker read barriers.
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForFields = true;
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForArrays = true;
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForGcRoots = true;

// Some instructions have special requirements for a temporary, for example
// LoadClass/kBssEntry and LoadString/kBssEntry for Baker read barrier require
// temp that's not an R0 (to avoid an extra move) and Baker read barrier field
// loads with large offsets need a fixed register to limit the number of link-time
// thunks we generate. For these and similar cases, we want to reserve a specific
// register that's neither callee-save nor an argument register. We choose x15.
inline Location FixedTempLocation() {
  return Location::RegisterLocation(x15.GetCode());
}

inline Condition ARM64Condition(IfCondition cond) {
  switch (cond) {
    case kCondEQ: return eq;
    case kCondNE: return ne;
    case kCondLT: return lt;
    case kCondLE: return le;
    case kCondGT: return gt;
    case kCondGE: return ge;
    case kCondB:  return lo;
    case kCondBE: return ls;
    case kCondA:  return hi;
    case kCondAE: return hs;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

inline Condition ARM64FPCondition(IfCondition cond, bool gt_bias) {
  // The ARM64 condition codes can express all the necessary branches, see the
  // "Meaning (floating-point)" column in the table C1-1 in the ARMv8 reference manual.
  // There is no dex instruction or HIR that would need the missing conditions
  // "equal or unordered" or "not equal".
  switch (cond) {
    case kCondEQ: return eq;
    case kCondNE: return ne /* unordered */;
    case kCondLT: return gt_bias ? cc : lt /* unordered */;
    case kCondLE: return gt_bias ? ls : le /* unordered */;
    case kCondGT: return gt_bias ? hi /* unordered */ : gt;
    case kCondGE: return gt_bias ? cs /* unordered */ : ge;
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

Location ARM64ReturnLocation(DataType::Type return_type) {
  // Note that in practice, `LocationFrom(x0)` and `LocationFrom(w0)` create the
  // same Location object, and so do `LocationFrom(d0)` and `LocationFrom(s0)`,
  // but we use the exact registers for clarity.
  if (return_type == DataType::Type::kFloat32) {
    return LocationFrom(s0);
  } else if (return_type == DataType::Type::kFloat64) {
    return LocationFrom(d0);
  } else if (return_type == DataType::Type::kInt64) {
    return LocationFrom(x0);
  } else if (return_type == DataType::Type::kVoid) {
    return Location::NoLocation();
  } else {
    return LocationFrom(w0);
  }
}

Location InvokeRuntimeCallingConvention::GetReturnLocation(DataType::Type return_type) {
  return ARM64ReturnLocation(return_type);
}

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<CodeGeneratorARM64*>(codegen)->GetVIXLAssembler()->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kArm64PointerSize, x).Int32Value()

// Calculate memory accessing operand for save/restore live registers.
static void SaveRestoreLiveRegistersHelper(CodeGenerator* codegen,
                                           LocationSummary* locations,
                                           int64_t spill_offset,
                                           bool is_save) {
  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ true);
  const uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ false);
  DCHECK(ArtVixlRegCodeCoherentForRegSet(core_spills,
                                         codegen->GetNumberOfCoreRegisters(),
                                         fp_spills,
                                         codegen->GetNumberOfFloatingPointRegisters()));

  CPURegList core_list = CPURegList(CPURegister::kRegister, kXRegSize, core_spills);
  unsigned v_reg_size = codegen->GetGraph()->HasSIMD() ? kQRegSize : kDRegSize;
  CPURegList fp_list = CPURegList(CPURegister::kVRegister, v_reg_size, fp_spills);

  MacroAssembler* masm = down_cast<CodeGeneratorARM64*>(codegen)->GetVIXLAssembler();
  UseScratchRegisterScope temps(masm);

  Register base = masm->StackPointer();
  int64_t core_spill_size = core_list.GetTotalSizeInBytes();
  int64_t fp_spill_size = fp_list.GetTotalSizeInBytes();
  int64_t reg_size = kXRegSizeInBytes;
  int64_t max_ls_pair_offset = spill_offset + core_spill_size + fp_spill_size - 2 * reg_size;
  uint32_t ls_access_size = WhichPowerOf2(reg_size);
  if (((core_list.GetCount() > 1) || (fp_list.GetCount() > 1)) &&
      !masm->IsImmLSPair(max_ls_pair_offset, ls_access_size)) {
    // If the offset does not fit in the instruction's immediate field, use an alternate register
    // to compute the base address(float point registers spill base address).
    Register new_base = temps.AcquireSameSizeAs(base);
    __ Add(new_base, base, Operand(spill_offset + core_spill_size));
    base = new_base;
    spill_offset = -core_spill_size;
    int64_t new_max_ls_pair_offset = fp_spill_size - 2 * reg_size;
    DCHECK(masm->IsImmLSPair(spill_offset, ls_access_size));
    DCHECK(masm->IsImmLSPair(new_max_ls_pair_offset, ls_access_size));
  }

  if (is_save) {
    __ StoreCPURegList(core_list, MemOperand(base, spill_offset));
    __ StoreCPURegList(fp_list, MemOperand(base, spill_offset + core_spill_size));
  } else {
    __ LoadCPURegList(core_list, MemOperand(base, spill_offset));
    __ LoadCPURegList(fp_list, MemOperand(base, spill_offset + core_spill_size));
  }
}

void SlowPathCodeARM64::SaveLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  size_t stack_offset = codegen->GetFirstRegisterSlotInSlowPath();
  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ true);
  for (uint32_t i : LowToHighBits(core_spills)) {
    // If the register holds an object, update the stack mask.
    if (locations->RegisterContainsObject(i)) {
      locations->SetStackBit(stack_offset / kVRegSize);
    }
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_core_stack_offsets_[i] = stack_offset;
    stack_offset += kXRegSizeInBytes;
  }

  const uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ false);
  for (uint32_t i : LowToHighBits(fp_spills)) {
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_fpu_stack_offsets_[i] = stack_offset;
    stack_offset += kDRegSizeInBytes;
  }

  SaveRestoreLiveRegistersHelper(codegen,
                                 locations,
                                 codegen->GetFirstRegisterSlotInSlowPath(), true /* is_save */);
}

void SlowPathCodeARM64::RestoreLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  SaveRestoreLiveRegistersHelper(codegen,
                                 locations,
                                 codegen->GetFirstRegisterSlotInSlowPath(), false /* is_save */);
}

class BoundsCheckSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit BoundsCheckSlowPathARM64(HBoundsCheck* instruction) : SlowPathCodeARM64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);

    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(locations->InAt(0),
                               LocationFrom(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kInt32,
                               locations->InAt(1),
                               LocationFrom(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kInt32);
    QuickEntrypointEnum entrypoint = instruction_->AsBoundsCheck()->IsStringCharAt()
        ? kQuickThrowStringBounds
        : kQuickThrowArrayBounds;
    arm64_codegen->InvokeRuntime(entrypoint, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowStringBounds, void, int32_t, int32_t>();
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "BoundsCheckSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathARM64);
};

class DivZeroCheckSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit DivZeroCheckSlowPathARM64(HDivZeroCheck* instruction) : SlowPathCodeARM64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    __ Bind(GetEntryLabel());
    arm64_codegen->InvokeRuntime(kQuickThrowDivZero, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowDivZero, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "DivZeroCheckSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DivZeroCheckSlowPathARM64);
};

class LoadClassSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  LoadClassSlowPathARM64(HLoadClass* cls,
                         HInstruction* at,
                         uint32_t dex_pc,
                         bool do_clinit)
      : SlowPathCodeARM64(at),
        cls_(cls),
        dex_pc_(dex_pc),
        do_clinit_(do_clinit) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Location out = locations->Out();
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    dex::TypeIndex type_index = cls_->GetTypeIndex();
    __ Mov(calling_convention.GetRegisterAt(0).W(), type_index.index_);
    QuickEntrypointEnum entrypoint = do_clinit_ ? kQuickInitializeStaticStorage
                                                : kQuickInitializeType;
    arm64_codegen->InvokeRuntime(entrypoint, instruction_, dex_pc_, this);
    if (do_clinit_) {
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, uint32_t>();
    } else {
      CheckEntrypointTypes<kQuickInitializeType, void*, uint32_t>();
    }

    // Move the class to the desired location.
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      DataType::Type type = instruction_->GetType();
      arm64_codegen->MoveLocation(out, calling_convention.GetReturnLocation(type), type);
    }
    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadClassSlowPathARM64"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  // The dex PC of `at_`.
  const uint32_t dex_pc_;

  // Whether to initialize the class.
  const bool do_clinit_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathARM64);
};

class LoadStringSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit LoadStringSlowPathARM64(HLoadString* instruction)
      : SlowPathCodeARM64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    const dex::StringIndex string_index = instruction_->AsLoadString()->GetStringIndex();
    __ Mov(calling_convention.GetRegisterAt(0).W(), string_index.index_);
    arm64_codegen->InvokeRuntime(kQuickResolveString, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();
    DataType::Type type = instruction_->GetType();
    arm64_codegen->MoveLocation(locations->Out(), calling_convention.GetReturnLocation(type), type);

    RestoreLiveRegisters(codegen, locations);

    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadStringSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadStringSlowPathARM64);
};

class NullCheckSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit NullCheckSlowPathARM64(HNullCheck* instr) : SlowPathCodeARM64(instr) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    arm64_codegen->InvokeRuntime(kQuickThrowNullPointer,
                                 instruction_,
                                 instruction_->GetDexPc(),
                                 this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "NullCheckSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathARM64);
};

class SuspendCheckSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  SuspendCheckSlowPathARM64(HSuspendCheck* instruction, HBasicBlock* successor)
      : SlowPathCodeARM64(instruction), successor_(successor) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);  // Only saves live 128-bit regs for SIMD.
    arm64_codegen->InvokeRuntime(kQuickTestSuspend, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickTestSuspend, void, void>();
    RestoreLiveRegisters(codegen, locations);  // Only restores live 128-bit regs for SIMD.
    if (successor_ == nullptr) {
      __ B(GetReturnLabel());
    } else {
      __ B(arm64_codegen->GetLabelOf(successor_));
    }
  }

  vixl::aarch64::Label* GetReturnLabel() {
    DCHECK(successor_ == nullptr);
    return &return_label_;
  }

  HBasicBlock* GetSuccessor() const {
    return successor_;
  }

  const char* GetDescription() const OVERRIDE { return "SuspendCheckSlowPathARM64"; }

 private:
  // If not null, the block to branch to after the suspend check.
  HBasicBlock* const successor_;

  // If `successor_` is null, the label to branch to after the suspend check.
  vixl::aarch64::Label return_label_;

  DISALLOW_COPY_AND_ASSIGN(SuspendCheckSlowPathARM64);
};

class TypeCheckSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  TypeCheckSlowPathARM64(HInstruction* instruction, bool is_fatal)
      : SlowPathCodeARM64(instruction), is_fatal_(is_fatal) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();

    DCHECK(instruction_->IsCheckCast()
           || !locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    uint32_t dex_pc = instruction_->GetDexPc();

    __ Bind(GetEntryLabel());

    if (!is_fatal_ || instruction_->CanThrowIntoCatchBlock()) {
      SaveLiveRegisters(codegen, locations);
    }

    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    codegen->EmitParallelMoves(locations->InAt(0),
                               LocationFrom(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kReference,
                               locations->InAt(1),
                               LocationFrom(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kReference);
    if (instruction_->IsInstanceOf()) {
      arm64_codegen->InvokeRuntime(kQuickInstanceofNonTrivial, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickInstanceofNonTrivial, size_t, mirror::Object*, mirror::Class*>();
      DataType::Type ret_type = instruction_->GetType();
      Location ret_loc = calling_convention.GetReturnLocation(ret_type);
      arm64_codegen->MoveLocation(locations->Out(), ret_loc, ret_type);
    } else {
      DCHECK(instruction_->IsCheckCast());
      arm64_codegen->InvokeRuntime(kQuickCheckInstanceOf, instruction_, dex_pc, this);
      CheckEntrypointTypes<kQuickCheckInstanceOf, void, mirror::Object*, mirror::Class*>();
    }

    if (!is_fatal_) {
      RestoreLiveRegisters(codegen, locations);
      __ B(GetExitLabel());
    }
  }

  const char* GetDescription() const OVERRIDE { return "TypeCheckSlowPathARM64"; }
  bool IsFatal() const OVERRIDE { return is_fatal_; }

 private:
  const bool is_fatal_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckSlowPathARM64);
};

class DeoptimizationSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit DeoptimizationSlowPathARM64(HDeoptimize* instruction)
      : SlowPathCodeARM64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    __ Bind(GetEntryLabel());
    LocationSummary* locations = instruction_->GetLocations();
    SaveLiveRegisters(codegen, locations);
    InvokeRuntimeCallingConvention calling_convention;
    __ Mov(calling_convention.GetRegisterAt(0),
           static_cast<uint32_t>(instruction_->AsDeoptimize()->GetDeoptimizationKind()));
    arm64_codegen->InvokeRuntime(kQuickDeoptimize, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickDeoptimize, void, DeoptimizationKind>();
  }

  const char* GetDescription() const OVERRIDE { return "DeoptimizationSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeoptimizationSlowPathARM64);
};

class ArraySetSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit ArraySetSlowPathARM64(HInstruction* instruction) : SlowPathCodeARM64(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());
    parallel_move.AddMove(
        locations->InAt(0),
        LocationFrom(calling_convention.GetRegisterAt(0)),
        DataType::Type::kReference,
        nullptr);
    parallel_move.AddMove(
        locations->InAt(1),
        LocationFrom(calling_convention.GetRegisterAt(1)),
        DataType::Type::kInt32,
        nullptr);
    parallel_move.AddMove(
        locations->InAt(2),
        LocationFrom(calling_convention.GetRegisterAt(2)),
        DataType::Type::kReference,
        nullptr);
    codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);

    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    arm64_codegen->InvokeRuntime(kQuickAputObject, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickAputObject, void, mirror::Array*, int32_t, mirror::Object*>();
    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ArraySetSlowPathARM64"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArraySetSlowPathARM64);
};

void JumpTableARM64::EmitTable(CodeGeneratorARM64* codegen) {
  uint32_t num_entries = switch_instr_->GetNumEntries();
  DCHECK_GE(num_entries, kPackedSwitchCompareJumpThreshold);

  // We are about to use the assembler to place literals directly. Make sure we have enough
  // underlying code buffer and we have generated the jump table with right size.
  EmissionCheckScope scope(codegen->GetVIXLAssembler(),
                           num_entries * sizeof(int32_t),
                           CodeBufferCheckScope::kExactSize);

  __ Bind(&table_start_);
  const ArenaVector<HBasicBlock*>& successors = switch_instr_->GetBlock()->GetSuccessors();
  for (uint32_t i = 0; i < num_entries; i++) {
    vixl::aarch64::Label* target_label = codegen->GetLabelOf(successors[i]);
    DCHECK(target_label->IsBound());
    ptrdiff_t jump_offset = target_label->GetLocation() - table_start_.GetLocation();
    DCHECK_GT(jump_offset, std::numeric_limits<int32_t>::min());
    DCHECK_LE(jump_offset, std::numeric_limits<int32_t>::max());
    Literal<int32_t> literal(jump_offset);
    __ place(&literal);
  }
}

// Abstract base class for read barrier slow paths marking a reference
// `ref`.
//
// Argument `entrypoint` must be a register location holding the read
// barrier marking runtime entry point to be invoked or an empty
// location; in the latter case, the read barrier marking runtime
// entry point will be loaded by the slow path code itself.
class ReadBarrierMarkSlowPathBaseARM64 : public SlowPathCodeARM64 {
 protected:
  ReadBarrierMarkSlowPathBaseARM64(HInstruction* instruction, Location ref, Location entrypoint)
      : SlowPathCodeARM64(instruction), ref_(ref), entrypoint_(entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathBaseARM64"; }

  // Generate assembly code calling the read barrier marking runtime
  // entry point (ReadBarrierMarkRegX).
  void GenerateReadBarrierMarkRuntimeCall(CodeGenerator* codegen) {
    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    DCHECK_NE(ref_.reg(), LR);
    DCHECK_NE(ref_.reg(), WSP);
    DCHECK_NE(ref_.reg(), WZR);
    // IP0 is used internally by the ReadBarrierMarkRegX entry point
    // as a temporary, it cannot be the entry point's input/output.
    DCHECK_NE(ref_.reg(), IP0);
    DCHECK(0 <= ref_.reg() && ref_.reg() < kNumberOfWRegisters) << ref_.reg();
    // "Compact" slow path, saving two moves.
    //
    // Instead of using the standard runtime calling convention (input
    // and output in W0):
    //
    //   W0 <- ref
    //   W0 <- ReadBarrierMark(W0)
    //   ref <- W0
    //
    // we just use rX (the register containing `ref`) as input and output
    // of a dedicated entrypoint:
    //
    //   rX <- ReadBarrierMarkRegX(rX)
    //
    if (entrypoint_.IsValid()) {
      arm64_codegen->ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction_, this);
      __ Blr(XRegisterFrom(entrypoint_));
    } else {
      // Entrypoint is not already loaded, load from the thread.
      int32_t entry_point_offset =
          Thread::ReadBarrierMarkEntryPointsOffset<kArm64PointerSize>(ref_.reg());
      // This runtime call does not require a stack map.
      arm64_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset, instruction_, this);
    }
  }

  // The location (register) of the marked object reference.
  const Location ref_;

  // The location of the entrypoint if it is already loaded.
  const Location entrypoint_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathBaseARM64);
};

// Slow path marking an object reference `ref` during a read
// barrier. The field `obj.field` in the object `obj` holding this
// reference does not get updated by this slow path after marking.
//
// This means that after the execution of this slow path, `ref` will
// always be up-to-date, but `obj.field` may not; i.e., after the
// flip, `ref` will be a to-space reference, but `obj.field` will
// probably still be a from-space reference (unless it gets updated by
// another thread, or if another thread installed another object
// reference (different from `ref`) in `obj.field`).
//
// Argument `entrypoint` must be a register location holding the read
// barrier marking runtime entry point to be invoked or an empty
// location; in the latter case, the read barrier marking runtime
// entry point will be loaded by the slow path code itself.
class ReadBarrierMarkSlowPathARM64 : public ReadBarrierMarkSlowPathBaseARM64 {
 public:
  ReadBarrierMarkSlowPathARM64(HInstruction* instruction,
                               Location ref,
                               Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARM64(instruction, ref, entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathARM64"; }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(locations->CanCall());
    DCHECK(ref_.IsRegister()) << ref_;
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_.reg())) << ref_.reg();
    DCHECK(instruction_->IsLoadClass() || instruction_->IsLoadString())
        << "Unexpected instruction in read barrier marking slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    GenerateReadBarrierMarkRuntimeCall(codegen);
    __ B(GetExitLabel());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathARM64);
};

// Slow path loading `obj`'s lock word, loading a reference from
// object `*(obj + offset + (index << scale_factor))` into `ref`, and
// marking `ref` if `obj` is gray according to the lock word (Baker
// read barrier). The field `obj.field` in the object `obj` holding
// this reference does not get updated by this slow path after marking
// (see LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64
// below for that).
//
// This means that after the execution of this slow path, `ref` will
// always be up-to-date, but `obj.field` may not; i.e., after the
// flip, `ref` will be a to-space reference, but `obj.field` will
// probably still be a from-space reference (unless it gets updated by
// another thread, or if another thread installed another object
// reference (different from `ref`) in `obj.field`).
//
// Argument `entrypoint` must be a register location holding the read
// barrier marking runtime entry point to be invoked or an empty
// location; in the latter case, the read barrier marking runtime
// entry point will be loaded by the slow path code itself.
class LoadReferenceWithBakerReadBarrierSlowPathARM64 : public ReadBarrierMarkSlowPathBaseARM64 {
 public:
  LoadReferenceWithBakerReadBarrierSlowPathARM64(HInstruction* instruction,
                                                 Location ref,
                                                 Register obj,
                                                 uint32_t offset,
                                                 Location index,
                                                 size_t scale_factor,
                                                 bool needs_null_check,
                                                 bool use_load_acquire,
                                                 Register temp,
                                                 Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARM64(instruction, ref, entrypoint),
        obj_(obj),
        offset_(offset),
        index_(index),
        scale_factor_(scale_factor),
        needs_null_check_(needs_null_check),
        use_load_acquire_(use_load_acquire),
        temp_(temp) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "LoadReferenceWithBakerReadBarrierSlowPathARM64";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(locations->CanCall());
    DCHECK(ref_.IsRegister()) << ref_;
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_.reg())) << ref_.reg();
    DCHECK(obj_.IsW());
    DCHECK_NE(ref_.reg(), LocationFrom(temp_).reg());
    DCHECK(instruction_->IsInstanceFieldGet() ||
           instruction_->IsStaticFieldGet() ||
           instruction_->IsArrayGet() ||
           instruction_->IsArraySet() ||
           instruction_->IsInstanceOf() ||
           instruction_->IsCheckCast() ||
           (instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()) ||
           (instruction_->IsInvokeStaticOrDirect() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking slow path: "
        << instruction_->DebugName();
    // The read barrier instrumentation of object ArrayGet
    // instructions does not support the HIntermediateAddress
    // instruction.
    DCHECK(!(instruction_->IsArrayGet() &&
             instruction_->AsArrayGet()->GetArray()->IsIntermediateAddress()));

    // Temporary register `temp_`, used to store the lock word, must
    // not be IP0 nor IP1, as we may use them to emit the reference
    // load (in the call to GenerateRawReferenceLoad below), and we
    // need the lock word to still be in `temp_` after the reference
    // load.
    DCHECK_NE(LocationFrom(temp_).reg(), IP0);
    DCHECK_NE(LocationFrom(temp_).reg(), IP1);

    __ Bind(GetEntryLabel());

    // When using MaybeGenerateReadBarrierSlow, the read barrier call is
    // inserted after the original load. However, in fast path based
    // Baker's read barriers, we need to perform the load of
    // mirror::Object::monitor_ *before* the original reference load.
    // This load-load ordering is required by the read barrier.
    // The slow path (for Baker's algorithm) should look like:
    //
    //   uint32_t rb_state = Lockword(obj->monitor_).ReadBarrierState();
    //   lfence;  // Load fence or artificial data dependency to prevent load-load reordering
    //   HeapReference<mirror::Object> ref = *src;  // Original reference load.
    //   bool is_gray = (rb_state == ReadBarrier::GrayState());
    //   if (is_gray) {
    //     ref = entrypoint(ref);  // ref = ReadBarrier::Mark(ref);  // Runtime entry point call.
    //   }
    //
    // Note: the original implementation in ReadBarrier::Barrier is
    // slightly more complex as it performs additional checks that we do
    // not do here for performance reasons.

    // /* int32_t */ monitor = obj->monitor_
    uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();
    __ Ldr(temp_, HeapOperand(obj_, monitor_offset));
    if (needs_null_check_) {
      codegen->MaybeRecordImplicitNullCheck(instruction_);
    }
    // /* LockWord */ lock_word = LockWord(monitor)
    static_assert(sizeof(LockWord) == sizeof(int32_t),
                  "art::LockWord and int32_t have different sizes.");

    // Introduce a dependency on the lock_word including rb_state,
    // to prevent load-load reordering, and without using
    // a memory barrier (which would be more expensive).
    // `obj` is unchanged by this operation, but its value now depends
    // on `temp`.
    __ Add(obj_.X(), obj_.X(), Operand(temp_.X(), LSR, 32));

    // The actual reference load.
    // A possible implicit null check has already been handled above.
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    arm64_codegen->GenerateRawReferenceLoad(instruction_,
                                            ref_,
                                            obj_,
                                            offset_,
                                            index_,
                                            scale_factor_,
                                            /* needs_null_check */ false,
                                            use_load_acquire_);

    // Mark the object `ref` when `obj` is gray.
    //
    //   if (rb_state == ReadBarrier::GrayState())
    //     ref = ReadBarrier::Mark(ref);
    //
    // Given the numeric representation, it's enough to check the low bit of the rb_state.
    static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
    static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
    __ Tbz(temp_, LockWord::kReadBarrierStateShift, GetExitLabel());
    GenerateReadBarrierMarkRuntimeCall(codegen);

    __ B(GetExitLabel());
  }

 private:
  // The register containing the object holding the marked object reference field.
  Register obj_;
  // The offset, index and scale factor to access the reference in `obj_`.
  uint32_t offset_;
  Location index_;
  size_t scale_factor_;
  // Is a null check required?
  bool needs_null_check_;
  // Should this reference load use Load-Acquire semantics?
  bool use_load_acquire_;
  // A temporary register used to hold the lock word of `obj_`.
  Register temp_;

  DISALLOW_COPY_AND_ASSIGN(LoadReferenceWithBakerReadBarrierSlowPathARM64);
};

// Slow path loading `obj`'s lock word, loading a reference from
// object `*(obj + offset + (index << scale_factor))` into `ref`, and
// marking `ref` if `obj` is gray according to the lock word (Baker
// read barrier). If needed, this slow path also atomically updates
// the field `obj.field` in the object `obj` holding this reference
// after marking (contrary to
// LoadReferenceWithBakerReadBarrierSlowPathARM64 above, which never
// tries to update `obj.field`).
//
// This means that after the execution of this slow path, both `ref`
// and `obj.field` will be up-to-date; i.e., after the flip, both will
// hold the same to-space reference (unless another thread installed
// another object reference (different from `ref`) in `obj.field`).
//
// Argument `entrypoint` must be a register location holding the read
// barrier marking runtime entry point to be invoked or an empty
// location; in the latter case, the read barrier marking runtime
// entry point will be loaded by the slow path code itself.
class LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64
    : public ReadBarrierMarkSlowPathBaseARM64 {
 public:
  LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64(
      HInstruction* instruction,
      Location ref,
      Register obj,
      uint32_t offset,
      Location index,
      size_t scale_factor,
      bool needs_null_check,
      bool use_load_acquire,
      Register temp,
      Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARM64(instruction, ref, entrypoint),
        obj_(obj),
        offset_(offset),
        index_(index),
        scale_factor_(scale_factor),
        needs_null_check_(needs_null_check),
        use_load_acquire_(use_load_acquire),
        temp_(temp) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Register ref_reg = WRegisterFrom(ref_);
    DCHECK(locations->CanCall());
    DCHECK(ref_.IsRegister()) << ref_;
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_.reg())) << ref_.reg();
    DCHECK(obj_.IsW());
    DCHECK_NE(ref_.reg(), LocationFrom(temp_).reg());

    // This slow path is only used by the UnsafeCASObject intrinsic at the moment.
    DCHECK((instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking and field updating slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kUnsafeCASObject);
    DCHECK_EQ(offset_, 0u);
    DCHECK_EQ(scale_factor_, 0u);
    DCHECK_EQ(use_load_acquire_, false);
    // The location of the offset of the marked reference field within `obj_`.
    Location field_offset = index_;
    DCHECK(field_offset.IsRegister()) << field_offset;

    // Temporary register `temp_`, used to store the lock word, must
    // not be IP0 nor IP1, as we may use them to emit the reference
    // load (in the call to GenerateRawReferenceLoad below), and we
    // need the lock word to still be in `temp_` after the reference
    // load.
    DCHECK_NE(LocationFrom(temp_).reg(), IP0);
    DCHECK_NE(LocationFrom(temp_).reg(), IP1);

    __ Bind(GetEntryLabel());

    // The implementation is similar to LoadReferenceWithBakerReadBarrierSlowPathARM64's:
    //
    //   uint32_t rb_state = Lockword(obj->monitor_).ReadBarrierState();
    //   lfence;  // Load fence or artificial data dependency to prevent load-load reordering
    //   HeapReference<mirror::Object> ref = *src;  // Original reference load.
    //   bool is_gray = (rb_state == ReadBarrier::GrayState());
    //   if (is_gray) {
    //     old_ref = ref;
    //     ref = entrypoint(ref);  // ref = ReadBarrier::Mark(ref);  // Runtime entry point call.
    //     compareAndSwapObject(obj, field_offset, old_ref, ref);
    //   }

    // /* int32_t */ monitor = obj->monitor_
    uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();
    __ Ldr(temp_, HeapOperand(obj_, monitor_offset));
    if (needs_null_check_) {
      codegen->MaybeRecordImplicitNullCheck(instruction_);
    }
    // /* LockWord */ lock_word = LockWord(monitor)
    static_assert(sizeof(LockWord) == sizeof(int32_t),
                  "art::LockWord and int32_t have different sizes.");

    // Introduce a dependency on the lock_word including rb_state,
    // to prevent load-load reordering, and without using
    // a memory barrier (which would be more expensive).
    // `obj` is unchanged by this operation, but its value now depends
    // on `temp`.
    __ Add(obj_.X(), obj_.X(), Operand(temp_.X(), LSR, 32));

    // The actual reference load.
    // A possible implicit null check has already been handled above.
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    arm64_codegen->GenerateRawReferenceLoad(instruction_,
                                            ref_,
                                            obj_,
                                            offset_,
                                            index_,
                                            scale_factor_,
                                            /* needs_null_check */ false,
                                            use_load_acquire_);

    // Mark the object `ref` when `obj` is gray.
    //
    //   if (rb_state == ReadBarrier::GrayState())
    //     ref = ReadBarrier::Mark(ref);
    //
    // Given the numeric representation, it's enough to check the low bit of the rb_state.
    static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
    static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
    __ Tbz(temp_, LockWord::kReadBarrierStateShift, GetExitLabel());

    // Save the old value of the reference before marking it.
    // Note that we cannot use IP to save the old reference, as IP is
    // used internally by the ReadBarrierMarkRegX entry point, and we
    // need the old reference after the call to that entry point.
    DCHECK_NE(LocationFrom(temp_).reg(), IP0);
    __ Mov(temp_.W(), ref_reg);

    GenerateReadBarrierMarkRuntimeCall(codegen);

    // If the new reference is different from the old reference,
    // update the field in the holder (`*(obj_ + field_offset)`).
    //
    // Note that this field could also hold a different object, if
    // another thread had concurrently changed it. In that case, the
    // LDXR/CMP/BNE sequence of instructions in the compare-and-set
    // (CAS) operation below would abort the CAS, leaving the field
    // as-is.
    __ Cmp(temp_.W(), ref_reg);
    __ B(eq, GetExitLabel());

    // Update the the holder's field atomically.  This may fail if
    // mutator updates before us, but it's OK.  This is achieved
    // using a strong compare-and-set (CAS) operation with relaxed
    // memory synchronization ordering, where the expected value is
    // the old reference and the desired value is the new reference.

    MacroAssembler* masm = arm64_codegen->GetVIXLAssembler();
    UseScratchRegisterScope temps(masm);

    // Convenience aliases.
    Register base = obj_.W();
    Register offset = XRegisterFrom(field_offset);
    Register expected = temp_.W();
    Register value = ref_reg;
    Register tmp_ptr = temps.AcquireX();    // Pointer to actual memory.
    Register tmp_value = temps.AcquireW();  // Value in memory.

    __ Add(tmp_ptr, base.X(), Operand(offset));

    if (kPoisonHeapReferences) {
      arm64_codegen->GetAssembler()->PoisonHeapReference(expected);
      if (value.Is(expected)) {
        // Do not poison `value`, as it is the same register as
        // `expected`, which has just been poisoned.
      } else {
        arm64_codegen->GetAssembler()->PoisonHeapReference(value);
      }
    }

    // do {
    //   tmp_value = [tmp_ptr] - expected;
    // } while (tmp_value == 0 && failure([tmp_ptr] <- r_new_value));

    vixl::aarch64::Label loop_head, comparison_failed, exit_loop;
    __ Bind(&loop_head);
    __ Ldxr(tmp_value, MemOperand(tmp_ptr));
    __ Cmp(tmp_value, expected);
    __ B(&comparison_failed, ne);
    __ Stxr(tmp_value, value, MemOperand(tmp_ptr));
    __ Cbnz(tmp_value, &loop_head);
    __ B(&exit_loop);
    __ Bind(&comparison_failed);
    __ Clrex();
    __ Bind(&exit_loop);

    if (kPoisonHeapReferences) {
      arm64_codegen->GetAssembler()->UnpoisonHeapReference(expected);
      if (value.Is(expected)) {
        // Do not unpoison `value`, as it is the same register as
        // `expected`, which has just been unpoisoned.
      } else {
        arm64_codegen->GetAssembler()->UnpoisonHeapReference(value);
      }
    }

    __ B(GetExitLabel());
  }

 private:
  // The register containing the object holding the marked object reference field.
  const Register obj_;
  // The offset, index and scale factor to access the reference in `obj_`.
  uint32_t offset_;
  Location index_;
  size_t scale_factor_;
  // Is a null check required?
  bool needs_null_check_;
  // Should this reference load use Load-Acquire semantics?
  bool use_load_acquire_;
  // A temporary register used to hold the lock word of `obj_`; and
  // also to hold the original reference value, when the reference is
  // marked.
  const Register temp_;

  DISALLOW_COPY_AND_ASSIGN(LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64);
};

// Slow path generating a read barrier for a heap reference.
class ReadBarrierForHeapReferenceSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  ReadBarrierForHeapReferenceSlowPathARM64(HInstruction* instruction,
                                           Location out,
                                           Location ref,
                                           Location obj,
                                           uint32_t offset,
                                           Location index)
      : SlowPathCodeARM64(instruction),
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
    //   __ Ldr(out, HeapOperand(out, class_offset);
    //   codegen_->GenerateReadBarrierSlow(instruction, out_loc, out_loc, out_loc, offset);
    //
    // In that case, we have lost the information about the original
    // object, and the emitted read barrier cannot work properly.
    DCHECK(!obj.Equals(out)) << "obj=" << obj << " out=" << out;
    DCHECK(!obj.Equals(ref)) << "obj=" << obj << " ref=" << ref;
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();
    DataType::Type type = DataType::Type::kReference;
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(out_.reg()));
    DCHECK(instruction_->IsInstanceFieldGet() ||
           instruction_->IsStaticFieldGet() ||
           instruction_->IsArrayGet() ||
           instruction_->IsInstanceOf() ||
           instruction_->IsCheckCast() ||
           (instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier for heap reference slow path: "
        << instruction_->DebugName();
    // The read barrier instrumentation of object ArrayGet
    // instructions does not support the HIntermediateAddress
    // instruction.
    DCHECK(!(instruction_->IsArrayGet() &&
             instruction_->AsArrayGet()->GetArray()->IsIntermediateAddress()));

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
        Register index_reg = RegisterFrom(index_, DataType::Type::kInt32);
        DCHECK(locations->GetLiveRegisters()->ContainsCoreRegister(index_.reg()));
        if (codegen->IsCoreCalleeSaveRegister(index_.reg())) {
          // We are about to change the value of `index_reg` (see the
          // calls to vixl::MacroAssembler::Lsl and
          // vixl::MacroAssembler::Mov below), but it has
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
          __ Mov(free_reg.W(), index_reg);
          index_reg = free_reg;
          index = LocationFrom(index_reg);
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
        __ Lsl(index_reg, index_reg, DataType::SizeShift(type));
        static_assert(
            sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
            "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
        __ Add(index_reg, index_reg, Operand(offset_));
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
        DCHECK_EQ(offset_, 0u);
        DCHECK(index_.IsRegister());
      }
    }

    // We're moving two or three locations to locations that could
    // overlap, so we need a parallel move resolver.
    InvokeRuntimeCallingConvention calling_convention;
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());
    parallel_move.AddMove(ref_,
                          LocationFrom(calling_convention.GetRegisterAt(0)),
                          type,
                          nullptr);
    parallel_move.AddMove(obj_,
                          LocationFrom(calling_convention.GetRegisterAt(1)),
                          type,
                          nullptr);
    if (index.IsValid()) {
      parallel_move.AddMove(index,
                            LocationFrom(calling_convention.GetRegisterAt(2)),
                            DataType::Type::kInt32,
                            nullptr);
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
    } else {
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
      arm64_codegen->MoveConstant(LocationFrom(calling_convention.GetRegisterAt(2)), offset_);
    }
    arm64_codegen->InvokeRuntime(kQuickReadBarrierSlow,
                                 instruction_,
                                 instruction_->GetDexPc(),
                                 this);
    CheckEntrypointTypes<
        kQuickReadBarrierSlow, mirror::Object*, mirror::Object*, mirror::Object*, uint32_t>();
    arm64_codegen->MoveLocation(out_, calling_convention.GetReturnLocation(type), type);

    RestoreLiveRegisters(codegen, locations);

    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForHeapReferenceSlowPathARM64"; }

 private:
  Register FindAvailableCallerSaveRegister(CodeGenerator* codegen) {
    size_t ref = static_cast<int>(XRegisterFrom(ref_).GetCode());
    size_t obj = static_cast<int>(XRegisterFrom(obj_).GetCode());
    for (size_t i = 0, e = codegen->GetNumberOfCoreRegisters(); i < e; ++i) {
      if (i != ref && i != obj && !codegen->IsCoreCalleeSaveRegister(i)) {
        return Register(VIXLRegCodeFromART(i), kXRegSize);
      }
    }
    // We shall never fail to find a free caller-save register, as
    // there are more than two core caller-save registers on ARM64
    // (meaning it is possible to find one which is different from
    // `ref` and `obj`).
    DCHECK_GT(codegen->GetNumberOfCoreCallerSaveRegisters(), 2u);
    LOG(FATAL) << "Could not find a free register";
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

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForHeapReferenceSlowPathARM64);
};

// Slow path generating a read barrier for a GC root.
class ReadBarrierForRootSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  ReadBarrierForRootSlowPathARM64(HInstruction* instruction, Location out, Location root)
      : SlowPathCodeARM64(instruction), out_(out), root_(root) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DataType::Type type = DataType::Type::kReference;
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(out_.reg()));
    DCHECK(instruction_->IsLoadClass() || instruction_->IsLoadString())
        << "Unexpected instruction in read barrier for GC root slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConvention calling_convention;
    CodeGeneratorARM64* arm64_codegen = down_cast<CodeGeneratorARM64*>(codegen);
    // The argument of the ReadBarrierForRootSlow is not a managed
    // reference (`mirror::Object*`), but a `GcRoot<mirror::Object>*`;
    // thus we need a 64-bit move here, and we cannot use
    //
    //   arm64_codegen->MoveLocation(
    //       LocationFrom(calling_convention.GetRegisterAt(0)),
    //       root_,
    //       type);
    //
    // which would emit a 32-bit move, as `type` is a (32-bit wide)
    // reference type (`DataType::Type::kReference`).
    __ Mov(calling_convention.GetRegisterAt(0), XRegisterFrom(out_));
    arm64_codegen->InvokeRuntime(kQuickReadBarrierForRootSlow,
                                 instruction_,
                                 instruction_->GetDexPc(),
                                 this);
    CheckEntrypointTypes<kQuickReadBarrierForRootSlow, mirror::Object*, GcRoot<mirror::Object>*>();
    arm64_codegen->MoveLocation(out_, calling_convention.GetReturnLocation(type), type);

    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForRootSlowPathARM64"; }

 private:
  const Location out_;
  const Location root_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForRootSlowPathARM64);
};

#undef __

Location InvokeDexCallingConventionVisitorARM64::GetNextLocation(DataType::Type type) {
  Location next_location;
  if (type == DataType::Type::kVoid) {
    LOG(FATAL) << "Unreachable type " << type;
  }

  if (DataType::IsFloatingPointType(type) &&
      (float_index_ < calling_convention.GetNumberOfFpuRegisters())) {
    next_location = LocationFrom(calling_convention.GetFpuRegisterAt(float_index_++));
  } else if (!DataType::IsFloatingPointType(type) &&
             (gp_index_ < calling_convention.GetNumberOfRegisters())) {
    next_location = LocationFrom(calling_convention.GetRegisterAt(gp_index_++));
  } else {
    size_t stack_offset = calling_convention.GetStackOffsetOf(stack_index_);
    next_location = DataType::Is64BitType(type) ? Location::DoubleStackSlot(stack_offset)
                                                : Location::StackSlot(stack_offset);
  }

  // Space on the stack is reserved for all arguments.
  stack_index_ += DataType::Is64BitType(type) ? 2 : 1;
  return next_location;
}

Location InvokeDexCallingConventionVisitorARM64::GetMethodLocation() const {
  return LocationFrom(kArtMethodRegister);
}

CodeGeneratorARM64::CodeGeneratorARM64(HGraph* graph,
                                       const Arm64InstructionSetFeatures& isa_features,
                                       const CompilerOptions& compiler_options,
                                       OptimizingCompilerStats* stats)
    : CodeGenerator(graph,
                    kNumberOfAllocatableRegisters,
                    kNumberOfAllocatableFPRegisters,
                    kNumberOfAllocatableRegisterPairs,
                    callee_saved_core_registers.GetList(),
                    callee_saved_fp_registers.GetList(),
                    compiler_options,
                    stats),
      block_labels_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jump_tables_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      location_builder_(graph, this),
      instruction_visitor_(graph, this),
      move_resolver_(graph->GetAllocator(), this),
      assembler_(graph->GetAllocator()),
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
      baker_read_barrier_patches_(graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_string_patches_(StringReferenceValueComparator(),
                          graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)),
      jit_class_patches_(TypeReferenceValueComparator(),
                         graph->GetAllocator()->Adapter(kArenaAllocCodeGenerator)) {
  // Save the link register (containing the return address) to mimic Quick.
  AddAllocatedRegister(LocationFrom(lr));
}

#define __ GetVIXLAssembler()->

void CodeGeneratorARM64::EmitJumpTables() {
  for (auto&& jump_table : jump_tables_) {
    jump_table->EmitTable(this);
  }
}

void CodeGeneratorARM64::Finalize(CodeAllocator* allocator) {
  EmitJumpTables();
  // Ensure we emit the literal pool.
  __ FinalizeCode();

  CodeGenerator::Finalize(allocator);
}

void ParallelMoveResolverARM64::PrepareForEmitNativeCode() {
  // Note: There are 6 kinds of moves:
  // 1. constant -> GPR/FPR (non-cycle)
  // 2. constant -> stack (non-cycle)
  // 3. GPR/FPR -> GPR/FPR
  // 4. GPR/FPR -> stack
  // 5. stack -> GPR/FPR
  // 6. stack -> stack (non-cycle)
  // Case 1, 2 and 6 should never be included in a dependency cycle on ARM64. For case 3, 4, and 5
  // VIXL uses at most 1 GPR. VIXL has 2 GPR and 1 FPR temps, and there should be no intersecting
  // cycles on ARM64, so we always have 1 GPR and 1 FPR available VIXL temps to resolve the
  // dependency.
  vixl_temps_.Open(GetVIXLAssembler());
}

void ParallelMoveResolverARM64::FinishEmitNativeCode() {
  vixl_temps_.Close();
}

Location ParallelMoveResolverARM64::AllocateScratchLocationFor(Location::Kind kind) {
  DCHECK(kind == Location::kRegister || kind == Location::kFpuRegister
         || kind == Location::kStackSlot || kind == Location::kDoubleStackSlot
         || kind == Location::kSIMDStackSlot);
  kind = (kind == Location::kFpuRegister || kind == Location::kSIMDStackSlot)
      ? Location::kFpuRegister
      : Location::kRegister;
  Location scratch = GetScratchLocation(kind);
  if (!scratch.Equals(Location::NoLocation())) {
    return scratch;
  }
  // Allocate from VIXL temp registers.
  if (kind == Location::kRegister) {
    scratch = LocationFrom(vixl_temps_.AcquireX());
  } else {
    DCHECK_EQ(kind, Location::kFpuRegister);
    scratch = LocationFrom(codegen_->GetGraph()->HasSIMD()
        ? vixl_temps_.AcquireVRegisterOfSize(kQRegSize)
        : vixl_temps_.AcquireD());
  }
  AddScratchLocation(scratch);
  return scratch;
}

void ParallelMoveResolverARM64::FreeScratchLocation(Location loc) {
  if (loc.IsRegister()) {
    vixl_temps_.Release(XRegisterFrom(loc));
  } else {
    DCHECK(loc.IsFpuRegister());
    vixl_temps_.Release(codegen_->GetGraph()->HasSIMD() ? QRegisterFrom(loc) : DRegisterFrom(loc));
  }
  RemoveScratchLocation(loc);
}

void ParallelMoveResolverARM64::EmitMove(size_t index) {
  MoveOperands* move = moves_[index];
  codegen_->MoveLocation(move->GetDestination(), move->GetSource(), DataType::Type::kVoid);
}

void CodeGeneratorARM64::GenerateFrameEntry() {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Bind(&frame_entry_label_);

  if (GetCompilerOptions().CountHotnessInCompiledCode()) {
    UseScratchRegisterScope temps(masm);
    Register temp = temps.AcquireX();
    __ Ldrh(temp, MemOperand(kArtMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
    __ Add(temp, temp, 1);
    __ Strh(temp, MemOperand(kArtMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
  }

  bool do_overflow_check =
      FrameNeedsStackCheck(GetFrameSize(), InstructionSet::kArm64) || !IsLeafMethod();
  if (do_overflow_check) {
    UseScratchRegisterScope temps(masm);
    Register temp = temps.AcquireX();
    DCHECK(GetCompilerOptions().GetImplicitStackOverflowChecks());
    __ Sub(temp, sp, static_cast<int32_t>(GetStackOverflowReservedBytes(InstructionSet::kArm64)));
    {
      // Ensure that between load and RecordPcInfo there are no pools emitted.
      ExactAssemblyScope eas(GetVIXLAssembler(),
                             kInstructionSize,
                             CodeBufferCheckScope::kExactSize);
      __ ldr(wzr, MemOperand(temp, 0));
      RecordPcInfo(nullptr, 0);
    }
  }

  if (!HasEmptyFrame()) {
    int frame_size = GetFrameSize();
    // Stack layout:
    //      sp[frame_size - 8]        : lr.
    //      ...                       : other preserved core registers.
    //      ...                       : other preserved fp registers.
    //      ...                       : reserved frame space.
    //      sp[0]                     : current method.

    // Save the current method if we need it. Note that we do not
    // do this in HCurrentMethod, as the instruction might have been removed
    // in the SSA graph.
    if (RequiresCurrentMethod()) {
      __ Str(kArtMethodRegister, MemOperand(sp, -frame_size, PreIndex));
    } else {
      __ Claim(frame_size);
    }
    GetAssembler()->cfi().AdjustCFAOffset(frame_size);
    GetAssembler()->SpillRegisters(GetFramePreservedCoreRegisters(),
        frame_size - GetCoreSpillSize());
    GetAssembler()->SpillRegisters(GetFramePreservedFPRegisters(),
        frame_size - FrameEntrySpillSize());

    if (GetGraph()->HasShouldDeoptimizeFlag()) {
      // Initialize should_deoptimize flag to 0.
      Register wzr = Register(VIXLRegCodeFromART(WZR), kWRegSize);
      __ Str(wzr, MemOperand(sp, GetStackOffsetOfShouldDeoptimizeFlag()));
    }
  }

  MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void CodeGeneratorARM64::GenerateFrameExit() {
  GetAssembler()->cfi().RememberState();
  if (!HasEmptyFrame()) {
    int frame_size = GetFrameSize();
    GetAssembler()->UnspillRegisters(GetFramePreservedFPRegisters(),
        frame_size - FrameEntrySpillSize());
    GetAssembler()->UnspillRegisters(GetFramePreservedCoreRegisters(),
        frame_size - GetCoreSpillSize());
    __ Drop(frame_size);
    GetAssembler()->cfi().AdjustCFAOffset(-frame_size);
  }
  __ Ret();
  GetAssembler()->cfi().RestoreState();
  GetAssembler()->cfi().DefCFAOffset(GetFrameSize());
}

CPURegList CodeGeneratorARM64::GetFramePreservedCoreRegisters() const {
  DCHECK(ArtVixlRegCodeCoherentForRegSet(core_spill_mask_, GetNumberOfCoreRegisters(), 0, 0));
  return CPURegList(CPURegister::kRegister, kXRegSize,
                    core_spill_mask_);
}

CPURegList CodeGeneratorARM64::GetFramePreservedFPRegisters() const {
  DCHECK(ArtVixlRegCodeCoherentForRegSet(0, 0, fpu_spill_mask_,
                                         GetNumberOfFloatingPointRegisters()));
  return CPURegList(CPURegister::kFPRegister, kDRegSize,
                    fpu_spill_mask_);
}

void CodeGeneratorARM64::Bind(HBasicBlock* block) {
  __ Bind(GetLabelOf(block));
}

void CodeGeneratorARM64::MoveConstant(Location location, int32_t value) {
  DCHECK(location.IsRegister());
  __ Mov(RegisterFrom(location, DataType::Type::kInt32), value);
}

void CodeGeneratorARM64::AddLocationAsTemp(Location location, LocationSummary* locations) {
  if (location.IsRegister()) {
    locations->AddTemp(location);
  } else {
    UNIMPLEMENTED(FATAL) << "AddLocationAsTemp not implemented for location " << location;
  }
}

void CodeGeneratorARM64::MarkGCCard(Register object, Register value, bool value_can_be_null) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register card = temps.AcquireX();
  Register temp = temps.AcquireW();   // Index within the CardTable - 32bit.
  vixl::aarch64::Label done;
  if (value_can_be_null) {
    __ Cbz(value, &done);
  }
  __ Ldr(card, MemOperand(tr, Thread::CardTableOffset<kArm64PointerSize>().Int32Value()));
  __ Lsr(temp, object, gc::accounting::CardTable::kCardShift);
  __ Strb(card, MemOperand(card, temp.X()));
  if (value_can_be_null) {
    __ Bind(&done);
  }
}

void CodeGeneratorARM64::SetupBlockedRegisters() const {
  // Blocked core registers:
  //      lr        : Runtime reserved.
  //      tr        : Runtime reserved.
  //      mr        : Runtime reserved.
  //      ip1       : VIXL core temp.
  //      ip0       : VIXL core temp.
  //
  // Blocked fp registers:
  //      d31       : VIXL fp temp.
  CPURegList reserved_core_registers = vixl_reserved_core_registers;
  reserved_core_registers.Combine(runtime_reserved_core_registers);
  while (!reserved_core_registers.IsEmpty()) {
    blocked_core_registers_[reserved_core_registers.PopLowestIndex().GetCode()] = true;
  }

  CPURegList reserved_fp_registers = vixl_reserved_fp_registers;
  while (!reserved_fp_registers.IsEmpty()) {
    blocked_fpu_registers_[reserved_fp_registers.PopLowestIndex().GetCode()] = true;
  }

  if (GetGraph()->IsDebuggable()) {
    // Stubs do not save callee-save floating point registers. If the graph
    // is debuggable, we need to deal with these registers differently. For
    // now, just block them.
    CPURegList reserved_fp_registers_debuggable = callee_saved_fp_registers;
    while (!reserved_fp_registers_debuggable.IsEmpty()) {
      blocked_fpu_registers_[reserved_fp_registers_debuggable.PopLowestIndex().GetCode()] = true;
    }
  }
}

size_t CodeGeneratorARM64::SaveCoreRegister(size_t stack_index, uint32_t reg_id) {
  Register reg = Register(VIXLRegCodeFromART(reg_id), kXRegSize);
  __ Str(reg, MemOperand(sp, stack_index));
  return kArm64WordSize;
}

size_t CodeGeneratorARM64::RestoreCoreRegister(size_t stack_index, uint32_t reg_id) {
  Register reg = Register(VIXLRegCodeFromART(reg_id), kXRegSize);
  __ Ldr(reg, MemOperand(sp, stack_index));
  return kArm64WordSize;
}

size_t CodeGeneratorARM64::SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  FPRegister reg = FPRegister(reg_id, kDRegSize);
  __ Str(reg, MemOperand(sp, stack_index));
  return kArm64WordSize;
}

size_t CodeGeneratorARM64::RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) {
  FPRegister reg = FPRegister(reg_id, kDRegSize);
  __ Ldr(reg, MemOperand(sp, stack_index));
  return kArm64WordSize;
}

void CodeGeneratorARM64::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << XRegister(reg);
}

void CodeGeneratorARM64::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << DRegister(reg);
}

void CodeGeneratorARM64::MoveConstant(CPURegister destination, HConstant* constant) {
  if (constant->IsIntConstant()) {
    __ Mov(Register(destination), constant->AsIntConstant()->GetValue());
  } else if (constant->IsLongConstant()) {
    __ Mov(Register(destination), constant->AsLongConstant()->GetValue());
  } else if (constant->IsNullConstant()) {
    __ Mov(Register(destination), 0);
  } else if (constant->IsFloatConstant()) {
    __ Fmov(FPRegister(destination), constant->AsFloatConstant()->GetValue());
  } else {
    DCHECK(constant->IsDoubleConstant());
    __ Fmov(FPRegister(destination), constant->AsDoubleConstant()->GetValue());
  }
}


static bool CoherentConstantAndType(Location constant, DataType::Type type) {
  DCHECK(constant.IsConstant());
  HConstant* cst = constant.GetConstant();
  return (cst->IsIntConstant() && type == DataType::Type::kInt32) ||
         // Null is mapped to a core W register, which we associate with kPrimInt.
         (cst->IsNullConstant() && type == DataType::Type::kInt32) ||
         (cst->IsLongConstant() && type == DataType::Type::kInt64) ||
         (cst->IsFloatConstant() && type == DataType::Type::kFloat32) ||
         (cst->IsDoubleConstant() && type == DataType::Type::kFloat64);
}

// Allocate a scratch register from the VIXL pool, querying first
// the floating-point register pool, and then the core register
// pool. This is essentially a reimplementation of
// vixl::aarch64::UseScratchRegisterScope::AcquireCPURegisterOfSize
// using a different allocation strategy.
static CPURegister AcquireFPOrCoreCPURegisterOfSize(vixl::aarch64::MacroAssembler* masm,
                                                    vixl::aarch64::UseScratchRegisterScope* temps,
                                                    int size_in_bits) {
  return masm->GetScratchFPRegisterList()->IsEmpty()
      ? CPURegister(temps->AcquireRegisterOfSize(size_in_bits))
      : CPURegister(temps->AcquireVRegisterOfSize(size_in_bits));
}

void CodeGeneratorARM64::MoveLocation(Location destination,
                                      Location source,
                                      DataType::Type dst_type) {
  if (source.Equals(destination)) {
    return;
  }

  // A valid move can always be inferred from the destination and source
  // locations. When moving from and to a register, the argument type can be
  // used to generate 32bit instead of 64bit moves. In debug mode we also
  // checks the coherency of the locations and the type.
  bool unspecified_type = (dst_type == DataType::Type::kVoid);

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
    CPURegister dst = CPURegisterFrom(destination, dst_type);
    if (source.IsStackSlot() || source.IsDoubleStackSlot()) {
      DCHECK(dst.Is64Bits() == source.IsDoubleStackSlot());
      __ Ldr(dst, StackOperandFrom(source));
    } else if (source.IsSIMDStackSlot()) {
      __ Ldr(QRegisterFrom(destination), StackOperandFrom(source));
    } else if (source.IsConstant()) {
      DCHECK(CoherentConstantAndType(source, dst_type));
      MoveConstant(dst, source.GetConstant());
    } else if (source.IsRegister()) {
      if (destination.IsRegister()) {
        __ Mov(Register(dst), RegisterFrom(source, dst_type));
      } else {
        DCHECK(destination.IsFpuRegister());
        DataType::Type source_type = DataType::Is64BitType(dst_type)
            ? DataType::Type::kInt64
            : DataType::Type::kInt32;
        __ Fmov(FPRegisterFrom(destination, dst_type), RegisterFrom(source, source_type));
      }
    } else {
      DCHECK(source.IsFpuRegister());
      if (destination.IsRegister()) {
        DataType::Type source_type = DataType::Is64BitType(dst_type)
            ? DataType::Type::kFloat64
            : DataType::Type::kFloat32;
        __ Fmov(RegisterFrom(destination, dst_type), FPRegisterFrom(source, source_type));
      } else {
        DCHECK(destination.IsFpuRegister());
        if (GetGraph()->HasSIMD()) {
          __ Mov(QRegisterFrom(destination), QRegisterFrom(source));
        } else {
          __ Fmov(FPRegister(dst), FPRegisterFrom(source, dst_type));
        }
      }
    }
  } else if (destination.IsSIMDStackSlot()) {
    if (source.IsFpuRegister()) {
      __ Str(QRegisterFrom(source), StackOperandFrom(destination));
    } else {
      DCHECK(source.IsSIMDStackSlot());
      UseScratchRegisterScope temps(GetVIXLAssembler());
      if (GetVIXLAssembler()->GetScratchFPRegisterList()->IsEmpty()) {
        Register temp = temps.AcquireX();
        __ Ldr(temp, MemOperand(sp, source.GetStackIndex()));
        __ Str(temp, MemOperand(sp, destination.GetStackIndex()));
        __ Ldr(temp, MemOperand(sp, source.GetStackIndex() + kArm64WordSize));
        __ Str(temp, MemOperand(sp, destination.GetStackIndex() + kArm64WordSize));
      } else {
        FPRegister temp = temps.AcquireVRegisterOfSize(kQRegSize);
        __ Ldr(temp, StackOperandFrom(source));
        __ Str(temp, StackOperandFrom(destination));
      }
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
      __ Str(CPURegisterFrom(source, dst_type), StackOperandFrom(destination));
    } else if (source.IsConstant()) {
      DCHECK(unspecified_type || CoherentConstantAndType(source, dst_type))
          << source << " " << dst_type;
      UseScratchRegisterScope temps(GetVIXLAssembler());
      HConstant* src_cst = source.GetConstant();
      CPURegister temp;
      if (src_cst->IsZeroBitPattern()) {
        temp = (src_cst->IsLongConstant() || src_cst->IsDoubleConstant())
            ? Register(xzr)
            : Register(wzr);
      } else {
        if (src_cst->IsIntConstant()) {
          temp = temps.AcquireW();
        } else if (src_cst->IsLongConstant()) {
          temp = temps.AcquireX();
        } else if (src_cst->IsFloatConstant()) {
          temp = temps.AcquireS();
        } else {
          DCHECK(src_cst->IsDoubleConstant());
          temp = temps.AcquireD();
        }
        MoveConstant(temp, src_cst);
      }
      __ Str(temp, StackOperandFrom(destination));
    } else {
      DCHECK(source.IsStackSlot() || source.IsDoubleStackSlot());
      DCHECK(source.IsDoubleStackSlot() == destination.IsDoubleStackSlot());
      UseScratchRegisterScope temps(GetVIXLAssembler());
      // Use any scratch register (a core or a floating-point one)
      // from VIXL scratch register pools as a temporary.
      //
      // We used to only use the FP scratch register pool, but in some
      // rare cases the only register from this pool (D31) would
      // already be used (e.g. within a ParallelMove instruction, when
      // a move is blocked by a another move requiring a scratch FP
      // register, which would reserve D31). To prevent this issue, we
      // ask for a scratch register of any type (core or FP).
      //
      // Also, we start by asking for a FP scratch register first, as the
      // demand of scratch core registers is higher. This is why we
      // use AcquireFPOrCoreCPURegisterOfSize instead of
      // UseScratchRegisterScope::AcquireCPURegisterOfSize, which
      // allocates core scratch registers first.
      CPURegister temp = AcquireFPOrCoreCPURegisterOfSize(
          GetVIXLAssembler(),
          &temps,
          (destination.IsDoubleStackSlot() ? kXRegSize : kWRegSize));
      __ Ldr(temp, StackOperandFrom(source));
      __ Str(temp, StackOperandFrom(destination));
    }
  }
}

void CodeGeneratorARM64::Load(DataType::Type type,
                              CPURegister dst,
                              const MemOperand& src) {
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
      __ Ldrb(Register(dst), src);
      break;
    case DataType::Type::kInt8:
      __ Ldrsb(Register(dst), src);
      break;
    case DataType::Type::kUint16:
      __ Ldrh(Register(dst), src);
      break;
    case DataType::Type::kInt16:
      __ Ldrsh(Register(dst), src);
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kReference:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      DCHECK_EQ(dst.Is64Bits(), DataType::Is64BitType(type));
      __ Ldr(dst, src);
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
  }
}

void CodeGeneratorARM64::LoadAcquire(HInstruction* instruction,
                                     CPURegister dst,
                                     const MemOperand& src,
                                     bool needs_null_check) {
  MacroAssembler* masm = GetVIXLAssembler();
  UseScratchRegisterScope temps(masm);
  Register temp_base = temps.AcquireX();
  DataType::Type type = instruction->GetType();

  DCHECK(!src.IsPreIndex());
  DCHECK(!src.IsPostIndex());

  // TODO(vixl): Let the MacroAssembler handle MemOperand.
  __ Add(temp_base, src.GetBaseRegister(), OperandFromMemOperand(src));
  {
    // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
    MemOperand base = MemOperand(temp_base);
    switch (type) {
      case DataType::Type::kBool:
      case DataType::Type::kUint8:
      case DataType::Type::kInt8:
        {
          ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
          __ ldarb(Register(dst), base);
          if (needs_null_check) {
            MaybeRecordImplicitNullCheck(instruction);
          }
        }
        if (type == DataType::Type::kInt8) {
          __ Sbfx(Register(dst), Register(dst), 0, DataType::Size(type) * kBitsPerByte);
        }
        break;
      case DataType::Type::kUint16:
      case DataType::Type::kInt16:
        {
          ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
          __ ldarh(Register(dst), base);
          if (needs_null_check) {
            MaybeRecordImplicitNullCheck(instruction);
          }
        }
        if (type == DataType::Type::kInt16) {
          __ Sbfx(Register(dst), Register(dst), 0, DataType::Size(type) * kBitsPerByte);
        }
        break;
      case DataType::Type::kInt32:
      case DataType::Type::kReference:
      case DataType::Type::kInt64:
        DCHECK_EQ(dst.Is64Bits(), DataType::Is64BitType(type));
        {
          ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
          __ ldar(Register(dst), base);
          if (needs_null_check) {
            MaybeRecordImplicitNullCheck(instruction);
          }
        }
        break;
      case DataType::Type::kFloat32:
      case DataType::Type::kFloat64: {
        DCHECK(dst.IsFPRegister());
        DCHECK_EQ(dst.Is64Bits(), DataType::Is64BitType(type));

        Register temp = dst.Is64Bits() ? temps.AcquireX() : temps.AcquireW();
        {
          ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
          __ ldar(temp, base);
          if (needs_null_check) {
            MaybeRecordImplicitNullCheck(instruction);
          }
        }
        __ Fmov(FPRegister(dst), temp);
        break;
      }
      case DataType::Type::kUint32:
      case DataType::Type::kUint64:
      case DataType::Type::kVoid:
        LOG(FATAL) << "Unreachable type " << type;
    }
  }
}

void CodeGeneratorARM64::Store(DataType::Type type,
                               CPURegister src,
                               const MemOperand& dst) {
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      __ Strb(Register(src), dst);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      __ Strh(Register(src), dst);
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kReference:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      DCHECK_EQ(src.Is64Bits(), DataType::Is64BitType(type));
      __ Str(src, dst);
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
  }
}

void CodeGeneratorARM64::StoreRelease(HInstruction* instruction,
                                      DataType::Type type,
                                      CPURegister src,
                                      const MemOperand& dst,
                                      bool needs_null_check) {
  MacroAssembler* masm = GetVIXLAssembler();
  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register temp_base = temps.AcquireX();

  DCHECK(!dst.IsPreIndex());
  DCHECK(!dst.IsPostIndex());

  // TODO(vixl): Let the MacroAssembler handle this.
  Operand op = OperandFromMemOperand(dst);
  __ Add(temp_base, dst.GetBaseRegister(), op);
  MemOperand base = MemOperand(temp_base);
  // Ensure that between store and MaybeRecordImplicitNullCheck there are no pools emitted.
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      {
        ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
        __ stlrb(Register(src), base);
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      {
        ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
        __ stlrh(Register(src), base);
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      }
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kReference:
    case DataType::Type::kInt64:
      DCHECK_EQ(src.Is64Bits(), DataType::Is64BitType(type));
      {
        ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
        __ stlr(Register(src), base);
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      }
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      DCHECK_EQ(src.Is64Bits(), DataType::Is64BitType(type));
      Register temp_src;
      if (src.IsZero()) {
        // The zero register is used to avoid synthesizing zero constants.
        temp_src = Register(src);
      } else {
        DCHECK(src.IsFPRegister());
        temp_src = src.Is64Bits() ? temps.AcquireX() : temps.AcquireW();
        __ Fmov(temp_src, FPRegister(src));
      }
      {
        ExactAssemblyScope eas(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
        __ stlr(temp_src, base);
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      }
      break;
    }
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
  }
}

void CodeGeneratorARM64::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                       HInstruction* instruction,
                                       uint32_t dex_pc,
                                       SlowPathCode* slow_path) {
  ValidateInvokeRuntime(entrypoint, instruction, slow_path);

  __ Ldr(lr, MemOperand(tr, GetThreadOffset<kArm64PointerSize>(entrypoint).Int32Value()));
  {
    // Ensure the pc position is recorded immediately after the `blr` instruction.
    ExactAssemblyScope eas(GetVIXLAssembler(), kInstructionSize, CodeBufferCheckScope::kExactSize);
    __ blr(lr);
    if (EntrypointRequiresStackMap(entrypoint)) {
      RecordPcInfo(instruction, dex_pc, slow_path);
    }
  }
}

void CodeGeneratorARM64::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                             HInstruction* instruction,
                                                             SlowPathCode* slow_path) {
  ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction, slow_path);
  __ Ldr(lr, MemOperand(tr, entry_point_offset));
  __ Blr(lr);
}

void InstructionCodeGeneratorARM64::GenerateClassInitializationCheck(SlowPathCodeARM64* slow_path,
                                                                     Register class_reg) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register temp = temps.AcquireW();
  constexpr size_t status_lsb_position = SubtypeCheckBits::BitStructSizeOf();
  const size_t status_byte_offset =
      mirror::Class::StatusOffset().SizeValue() + (status_lsb_position / kBitsPerByte);
  constexpr uint32_t shifted_initialized_value =
      enum_cast<uint32_t>(ClassStatus::kInitialized) << (status_lsb_position % kBitsPerByte);

  // Even if the initialized flag is set, we need to ensure consistent memory ordering.
  // TODO(vixl): Let the MacroAssembler handle MemOperand.
  __ Add(temp, class_reg, status_byte_offset);
  __ Ldarb(temp, HeapOperand(temp));
  __ Cmp(temp, shifted_initialized_value);
  __ B(lo, slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorARM64::GenerateMemoryBarrier(MemBarrierKind kind) {
  BarrierType type = BarrierAll;

  switch (kind) {
    case MemBarrierKind::kAnyAny:
    case MemBarrierKind::kAnyStore: {
      type = BarrierAll;
      break;
    }
    case MemBarrierKind::kLoadAny: {
      type = BarrierReads;
      break;
    }
    case MemBarrierKind::kStoreStore: {
      type = BarrierWrites;
      break;
    }
    default:
      LOG(FATAL) << "Unexpected memory barrier " << kind;
  }
  __ Dmb(InnerShareable, type);
}

void InstructionCodeGeneratorARM64::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                         HBasicBlock* successor) {
  SuspendCheckSlowPathARM64* slow_path =
      down_cast<SuspendCheckSlowPathARM64*>(instruction->GetSlowPath());
  if (slow_path == nullptr) {
    slow_path =
        new (codegen_->GetScopedAllocator()) SuspendCheckSlowPathARM64(instruction, successor);
    instruction->SetSlowPath(slow_path);
    codegen_->AddSlowPath(slow_path);
    if (successor != nullptr) {
      DCHECK(successor->IsLoopHeader());
    }
  } else {
    DCHECK_EQ(slow_path->GetSuccessor(), successor);
  }

  UseScratchRegisterScope temps(codegen_->GetVIXLAssembler());
  Register temp = temps.AcquireW();

  __ Ldrh(temp, MemOperand(tr, Thread::ThreadFlagsOffset<kArm64PointerSize>().SizeValue()));
  if (successor == nullptr) {
    __ Cbnz(temp, slow_path->GetEntryLabel());
    __ Bind(slow_path->GetReturnLabel());
  } else {
    __ Cbz(temp, codegen_->GetLabelOf(successor));
    __ B(slow_path->GetEntryLabel());
    // slow_path will return to GetLabelOf(successor).
  }
}

InstructionCodeGeneratorARM64::InstructionCodeGeneratorARM64(HGraph* graph,
                                                             CodeGeneratorARM64* codegen)
      : InstructionCodeGenerator(graph, codegen),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void LocationsBuilderARM64::HandleBinaryOp(HBinaryOperation* instr) {
  DCHECK_EQ(instr->InputCount(), 2U);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instr);
  DataType::Type type = instr->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, ARM64EncodableConstantOrRegister(instr->InputAt(1), instr));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected " << instr->DebugName() << " type " << type;
  }
}

void LocationsBuilderARM64::HandleFieldGet(HInstruction* instruction,
                                           const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());

  bool object_field_get_with_read_barrier =
      kEmitCompilerReadBarrier && (instruction->GetType() == DataType::Type::kReference);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction,
                                                       object_field_get_with_read_barrier
                                                           ? LocationSummary::kCallOnSlowPath
                                                           : LocationSummary::kNoCall);
  if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARM64::GenerateFieldLoadWithBakerReadBarrier.
    if (kBakerReadBarrierLinkTimeThunksEnableForFields &&
        !Runtime::Current()->UseJitCompilation() &&
        !field_info.IsVolatile()) {
      // If link-time thunks for the Baker read barrier are enabled, for AOT
      // non-volatile loads we need a temporary only if the offset is too big.
      if (field_info.GetFieldOffset().Uint32Value() >= kReferenceLoadMinFarOffset) {
        locations->AddTemp(FixedTempLocation());
      }
    } else {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
  locations->SetInAt(0, Location::RequiresRegister());
  if (DataType::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister());
  } else {
    // The output overlaps for an object field get when read barriers
    // are enabled: we do not want the load to overwrite the object's
    // location, as we need it to emit the read barrier.
    locations->SetOut(
        Location::RequiresRegister(),
        object_field_get_with_read_barrier ? Location::kOutputOverlap : Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorARM64::HandleFieldGet(HInstruction* instruction,
                                                   const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());
  LocationSummary* locations = instruction->GetLocations();
  Location base_loc = locations->InAt(0);
  Location out = locations->Out();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();
  DCHECK_EQ(DataType::Size(field_info.GetFieldType()), DataType::Size(instruction->GetType()));
  DataType::Type load_type = instruction->GetType();
  MemOperand field = HeapOperand(InputRegisterAt(instruction, 0), field_info.GetFieldOffset());

  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier &&
      load_type == DataType::Type::kReference) {
    // Object FieldGet with Baker's read barrier case.
    // /* HeapReference<Object> */ out = *(base + offset)
    Register base = RegisterFrom(base_loc, DataType::Type::kReference);
    Location maybe_temp =
        (locations->GetTempCount() != 0) ? locations->GetTemp(0) : Location::NoLocation();
    // Note that potential implicit null checks are handled in this
    // CodeGeneratorARM64::GenerateFieldLoadWithBakerReadBarrier call.
    codegen_->GenerateFieldLoadWithBakerReadBarrier(
        instruction,
        out,
        base,
        offset,
        maybe_temp,
        /* needs_null_check */ true,
        field_info.IsVolatile());
  } else {
    // General case.
    if (field_info.IsVolatile()) {
      // Note that a potential implicit null check is handled in this
      // CodeGeneratorARM64::LoadAcquire call.
      // NB: LoadAcquire will record the pc info if needed.
      codegen_->LoadAcquire(
          instruction, OutputCPURegister(instruction), field, /* needs_null_check */ true);
    } else {
      // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      codegen_->Load(load_type, OutputCPURegister(instruction), field);
      codegen_->MaybeRecordImplicitNullCheck(instruction);
    }
    if (load_type == DataType::Type::kReference) {
      // If read barriers are enabled, emit read barriers other than
      // Baker's using a slow path (and also unpoison the loaded
      // reference, if heap poisoning is enabled).
      codegen_->MaybeGenerateReadBarrierSlow(instruction, out, out, base_loc, offset);
    }
  }
}

void LocationsBuilderARM64::HandleFieldSet(HInstruction* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (IsConstantZeroBitPattern(instruction->InputAt(1))) {
    locations->SetInAt(1, Location::ConstantLocation(instruction->InputAt(1)->AsConstant()));
  } else if (DataType::IsFloatingPointType(instruction->InputAt(1)->GetType())) {
    locations->SetInAt(1, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(1, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARM64::HandleFieldSet(HInstruction* instruction,
                                                   const FieldInfo& field_info,
                                                   bool value_can_be_null) {
  DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());

  Register obj = InputRegisterAt(instruction, 0);
  CPURegister value = InputCPURegisterOrZeroRegAt(instruction, 1);
  CPURegister source = value;
  Offset offset = field_info.GetFieldOffset();
  DataType::Type field_type = field_info.GetFieldType();

  {
    // We use a block to end the scratch scope before the write barrier, thus
    // freeing the temporary registers so they can be used in `MarkGCCard`.
    UseScratchRegisterScope temps(GetVIXLAssembler());

    if (kPoisonHeapReferences && field_type == DataType::Type::kReference) {
      DCHECK(value.IsW());
      Register temp = temps.AcquireW();
      __ Mov(temp, value.W());
      GetAssembler()->PoisonHeapReference(temp.W());
      source = temp;
    }

    if (field_info.IsVolatile()) {
      codegen_->StoreRelease(
          instruction, field_type, source, HeapOperand(obj, offset), /* needs_null_check */ true);
    } else {
      // Ensure that between store and MaybeRecordImplicitNullCheck there are no pools emitted.
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      codegen_->Store(field_type, source, HeapOperand(obj, offset));
      codegen_->MaybeRecordImplicitNullCheck(instruction);
    }
  }

  if (CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1))) {
    codegen_->MarkGCCard(obj, Register(value), value_can_be_null);
  }
}

void InstructionCodeGeneratorARM64::HandleBinaryOp(HBinaryOperation* instr) {
  DataType::Type type = instr->GetType();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      Register dst = OutputRegister(instr);
      Register lhs = InputRegisterAt(instr, 0);
      Operand rhs = InputOperandAt(instr, 1);
      if (instr->IsAdd()) {
        __ Add(dst, lhs, rhs);
      } else if (instr->IsAnd()) {
        __ And(dst, lhs, rhs);
      } else if (instr->IsOr()) {
        __ Orr(dst, lhs, rhs);
      } else if (instr->IsSub()) {
        __ Sub(dst, lhs, rhs);
      } else if (instr->IsRor()) {
        if (rhs.IsImmediate()) {
          uint32_t shift = rhs.GetImmediate() & (lhs.GetSizeInBits() - 1);
          __ Ror(dst, lhs, shift);
        } else {
          // Ensure shift distance is in the same size register as the result. If
          // we are rotating a long and the shift comes in a w register originally,
          // we don't need to sxtw for use as an x since the shift distances are
          // all & reg_bits - 1.
          __ Ror(dst, lhs, RegisterFrom(instr->GetLocations()->InAt(1), type));
        }
      } else {
        DCHECK(instr->IsXor());
        __ Eor(dst, lhs, rhs);
      }
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      FPRegister dst = OutputFPRegister(instr);
      FPRegister lhs = InputFPRegisterAt(instr, 0);
      FPRegister rhs = InputFPRegisterAt(instr, 1);
      if (instr->IsAdd()) {
        __ Fadd(dst, lhs, rhs);
      } else if (instr->IsSub()) {
        __ Fsub(dst, lhs, rhs);
      } else {
        LOG(FATAL) << "Unexpected floating-point binary operation";
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected binary operation type " << type;
  }
}

void LocationsBuilderARM64::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr());

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

void InstructionCodeGeneratorARM64::HandleShift(HBinaryOperation* instr) {
  DCHECK(instr->IsShl() || instr->IsShr() || instr->IsUShr());

  DataType::Type type = instr->GetType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      Register dst = OutputRegister(instr);
      Register lhs = InputRegisterAt(instr, 0);
      Operand rhs = InputOperandAt(instr, 1);
      if (rhs.IsImmediate()) {
        uint32_t shift_value = rhs.GetImmediate() &
            (type == DataType::Type::kInt32 ? kMaxIntShiftDistance : kMaxLongShiftDistance);
        if (instr->IsShl()) {
          __ Lsl(dst, lhs, shift_value);
        } else if (instr->IsShr()) {
          __ Asr(dst, lhs, shift_value);
        } else {
          __ Lsr(dst, lhs, shift_value);
        }
      } else {
        Register rhs_reg = dst.IsX() ? rhs.GetRegister().X() : rhs.GetRegister().W();

        if (instr->IsShl()) {
          __ Lsl(dst, lhs, rhs_reg);
        } else if (instr->IsShr()) {
          __ Asr(dst, lhs, rhs_reg);
        } else {
          __ Lsr(dst, lhs, rhs_reg);
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected shift operation type " << type;
  }
}

void LocationsBuilderARM64::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorARM64::VisitAdd(HAdd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderARM64::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorARM64::VisitAnd(HAnd* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderARM64::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instr) {
  DCHECK(DataType::IsIntegralType(instr->GetType())) << instr->GetType();
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instr);
  locations->SetInAt(0, Location::RequiresRegister());
  // There is no immediate variant of negated bitwise instructions in AArch64.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instr) {
  Register dst = OutputRegister(instr);
  Register lhs = InputRegisterAt(instr, 0);
  Register rhs = InputRegisterAt(instr, 1);

  switch (instr->GetOpKind()) {
    case HInstruction::kAnd:
      __ Bic(dst, lhs, rhs);
      break;
    case HInstruction::kOr:
      __ Orn(dst, lhs, rhs);
      break;
    case HInstruction::kXor:
      __ Eon(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unreachable";
  }
}

void LocationsBuilderARM64::VisitDataProcWithShifterOp(
    HDataProcWithShifterOp* instruction) {
  DCHECK(instruction->GetType() == DataType::Type::kInt32 ||
         instruction->GetType() == DataType::Type::kInt64);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  if (instruction->GetInstrKind() == HInstruction::kNeg) {
    locations->SetInAt(0, Location::ConstantLocation(instruction->InputAt(0)->AsConstant()));
  } else {
    locations->SetInAt(0, Location::RequiresRegister());
  }
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitDataProcWithShifterOp(
    HDataProcWithShifterOp* instruction) {
  DataType::Type type = instruction->GetType();
  HInstruction::InstructionKind kind = instruction->GetInstrKind();
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);
  Register out = OutputRegister(instruction);
  Register left;
  if (kind != HInstruction::kNeg) {
    left = InputRegisterAt(instruction, 0);
  }
  // If this `HDataProcWithShifterOp` was created by merging a type conversion as the
  // shifter operand operation, the IR generating `right_reg` (input to the type
  // conversion) can have a different type from the current instruction's type,
  // so we manually indicate the type.
  Register right_reg = RegisterFrom(instruction->GetLocations()->InAt(1), type);
  Operand right_operand(0);

  HDataProcWithShifterOp::OpKind op_kind = instruction->GetOpKind();
  if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
    right_operand = Operand(right_reg, helpers::ExtendFromOpKind(op_kind));
  } else {
    right_operand = Operand(right_reg,
                            helpers::ShiftFromOpKind(op_kind),
                            instruction->GetShiftAmount());
  }

  // Logical binary operations do not support extension operations in the
  // operand. Note that VIXL would still manage if it was passed by generating
  // the extension as a separate instruction.
  // `HNeg` also does not support extension. See comments in `ShifterOperandSupportsExtension()`.
  DCHECK(!right_operand.IsExtendedRegister() ||
         (kind != HInstruction::kAnd && kind != HInstruction::kOr && kind != HInstruction::kXor &&
          kind != HInstruction::kNeg));
  switch (kind) {
    case HInstruction::kAdd:
      __ Add(out, left, right_operand);
      break;
    case HInstruction::kAnd:
      __ And(out, left, right_operand);
      break;
    case HInstruction::kNeg:
      DCHECK(instruction->InputAt(0)->AsConstant()->IsArithmeticZero());
      __ Neg(out, right_operand);
      break;
    case HInstruction::kOr:
      __ Orr(out, left, right_operand);
      break;
    case HInstruction::kSub:
      __ Sub(out, left, right_operand);
      break;
    case HInstruction::kXor:
      __ Eor(out, left, right_operand);
      break;
    default:
      LOG(FATAL) << "Unexpected operation kind: " << kind;
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, ARM64EncodableConstantOrRegister(instruction->GetOffset(), instruction));
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  __ Add(OutputRegister(instruction),
         InputRegisterAt(instruction, 0),
         Operand(InputOperandAt(instruction, 1)));
}

void LocationsBuilderARM64::VisitIntermediateAddressIndex(HIntermediateAddressIndex* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);

  HIntConstant* shift = instruction->GetShift()->AsIntConstant();

  locations->SetInAt(0, Location::RequiresRegister());
  // For byte case we don't need to shift the index variable so we can encode the data offset into
  // ADD instruction. For other cases we prefer the data_offset to be in register; that will hoist
  // data offset constant generation out of the loop and reduce the critical path length in the
  // loop.
  locations->SetInAt(1, shift->GetValue() == 0
                        ? Location::ConstantLocation(instruction->GetOffset()->AsIntConstant())
                        : Location::RequiresRegister());
  locations->SetInAt(2, Location::ConstantLocation(shift));
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitIntermediateAddressIndex(
    HIntermediateAddressIndex* instruction) {
  Register index_reg = InputRegisterAt(instruction, 0);
  uint32_t shift = Int64ConstantFrom(instruction->GetLocations()->InAt(2));
  uint32_t offset = instruction->GetOffset()->AsIntConstant()->GetValue();

  if (shift == 0) {
    __ Add(OutputRegister(instruction), index_reg, offset);
  } else {
    Register offset_reg = InputRegisterAt(instruction, 1);
    __ Add(OutputRegister(instruction), offset_reg, Operand(index_reg, LSL, shift));
  }
}

void LocationsBuilderARM64::VisitMultiplyAccumulate(HMultiplyAccumulate* instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instr, LocationSummary::kNoCall);
  HInstruction* accumulator = instr->InputAt(HMultiplyAccumulate::kInputAccumulatorIndex);
  if (instr->GetOpKind() == HInstruction::kSub &&
      accumulator->IsConstant() &&
      accumulator->AsConstant()->IsArithmeticZero()) {
    // Don't allocate register for Mneg instruction.
  } else {
    locations->SetInAt(HMultiplyAccumulate::kInputAccumulatorIndex,
                       Location::RequiresRegister());
  }
  locations->SetInAt(HMultiplyAccumulate::kInputMulLeftIndex, Location::RequiresRegister());
  locations->SetInAt(HMultiplyAccumulate::kInputMulRightIndex, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitMultiplyAccumulate(HMultiplyAccumulate* instr) {
  Register res = OutputRegister(instr);
  Register mul_left = InputRegisterAt(instr, HMultiplyAccumulate::kInputMulLeftIndex);
  Register mul_right = InputRegisterAt(instr, HMultiplyAccumulate::kInputMulRightIndex);

  // Avoid emitting code that could trigger Cortex A53's erratum 835769.
  // This fixup should be carried out for all multiply-accumulate instructions:
  // madd, msub, smaddl, smsubl, umaddl and umsubl.
  if (instr->GetType() == DataType::Type::kInt64 &&
      codegen_->GetInstructionSetFeatures().NeedFixCortexA53_835769()) {
    MacroAssembler* masm = down_cast<CodeGeneratorARM64*>(codegen_)->GetVIXLAssembler();
    vixl::aarch64::Instruction* prev =
        masm->GetCursorAddress<vixl::aarch64::Instruction*>() - kInstructionSize;
    if (prev->IsLoadOrStore()) {
      // Make sure we emit only exactly one nop.
      ExactAssemblyScope scope(masm, kInstructionSize, CodeBufferCheckScope::kExactSize);
      __ nop();
    }
  }

  if (instr->GetOpKind() == HInstruction::kAdd) {
    Register accumulator = InputRegisterAt(instr, HMultiplyAccumulate::kInputAccumulatorIndex);
    __ Madd(res, mul_left, mul_right, accumulator);
  } else {
    DCHECK(instr->GetOpKind() == HInstruction::kSub);
    HInstruction* accum_instr = instr->InputAt(HMultiplyAccumulate::kInputAccumulatorIndex);
    if (accum_instr->IsConstant() && accum_instr->AsConstant()->IsArithmeticZero()) {
      __ Mneg(res, mul_left, mul_right);
    } else {
      Register accumulator = InputRegisterAt(instr, HMultiplyAccumulate::kInputAccumulatorIndex);
      __ Msub(res, mul_left, mul_right, accumulator);
    }
  }
}

void LocationsBuilderARM64::VisitArrayGet(HArrayGet* instruction) {
  bool object_array_get_with_read_barrier =
      kEmitCompilerReadBarrier && (instruction->GetType() == DataType::Type::kReference);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction,
                                                       object_array_get_with_read_barrier
                                                           ? LocationSummary::kCallOnSlowPath
                                                           : LocationSummary::kNoCall);
  if (object_array_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARM64::GenerateArrayLoadWithBakerReadBarrier.
    if (kBakerReadBarrierLinkTimeThunksEnableForFields &&
        !Runtime::Current()->UseJitCompilation() &&
        instruction->GetIndex()->IsConstant()) {
      // Array loads with constant index are treated as field loads.
      // If link-time thunks for the Baker read barrier are enabled, for AOT
      // constant index loads we need a temporary only if the offset is too big.
      uint32_t offset = CodeGenerator::GetArrayDataOffset(instruction);
      uint32_t index = instruction->GetIndex()->AsIntConstant()->GetValue();
      offset += index << DataType::SizeShift(DataType::Type::kReference);
      if (offset >= kReferenceLoadMinFarOffset) {
        locations->AddTemp(FixedTempLocation());
      }
    } else {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (DataType::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    // The output overlaps in the case of an object array get with
    // read barriers enabled: we do not want the move to overwrite the
    // array's location, as we need it to emit the read barrier.
    locations->SetOut(
        Location::RequiresRegister(),
        object_array_get_with_read_barrier ? Location::kOutputOverlap : Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorARM64::VisitArrayGet(HArrayGet* instruction) {
  DataType::Type type = instruction->GetType();
  Register obj = InputRegisterAt(instruction, 0);
  LocationSummary* locations = instruction->GetLocations();
  Location index = locations->InAt(1);
  Location out = locations->Out();
  uint32_t offset = CodeGenerator::GetArrayDataOffset(instruction);
  const bool maybe_compressed_char_at = mirror::kUseStringCompression &&
                                        instruction->IsStringCharAt();
  MacroAssembler* masm = GetVIXLAssembler();
  UseScratchRegisterScope temps(masm);

  // The read barrier instrumentation of object ArrayGet instructions
  // does not support the HIntermediateAddress instruction.
  DCHECK(!((type == DataType::Type::kReference) &&
           instruction->GetArray()->IsIntermediateAddress() &&
           kEmitCompilerReadBarrier));

  if (type == DataType::Type::kReference && kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // Object ArrayGet with Baker's read barrier case.
    // Note that a potential implicit null check is handled in the
    // CodeGeneratorARM64::GenerateArrayLoadWithBakerReadBarrier call.
    DCHECK(!instruction->CanDoImplicitNullCheckOn(instruction->InputAt(0)));
    if (index.IsConstant()) {
      // Array load with a constant index can be treated as a field load.
      offset += Int64ConstantFrom(index) << DataType::SizeShift(type);
      Location maybe_temp =
          (locations->GetTempCount() != 0) ? locations->GetTemp(0) : Location::NoLocation();
      codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                      out,
                                                      obj.W(),
                                                      offset,
                                                      maybe_temp,
                                                      /* needs_null_check */ false,
                                                      /* use_load_acquire */ false);
    } else {
      Register temp = WRegisterFrom(locations->GetTemp(0));
      codegen_->GenerateArrayLoadWithBakerReadBarrier(
          instruction, out, obj.W(), offset, index, temp, /* needs_null_check */ false);
    }
  } else {
    // General case.
    MemOperand source = HeapOperand(obj);
    Register length;
    if (maybe_compressed_char_at) {
      uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
      length = temps.AcquireW();
      {
        // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
        EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);

        if (instruction->GetArray()->IsIntermediateAddress()) {
          DCHECK_LT(count_offset, offset);
          int64_t adjusted_offset =
              static_cast<int64_t>(count_offset) - static_cast<int64_t>(offset);
          // Note that `adjusted_offset` is negative, so this will be a LDUR.
          __ Ldr(length, MemOperand(obj.X(), adjusted_offset));
        } else {
          __ Ldr(length, HeapOperand(obj, count_offset));
        }
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }
    }
    if (index.IsConstant()) {
      if (maybe_compressed_char_at) {
        vixl::aarch64::Label uncompressed_load, done;
        static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                      "Expecting 0=compressed, 1=uncompressed");
        __ Tbnz(length.W(), 0, &uncompressed_load);
        __ Ldrb(Register(OutputCPURegister(instruction)),
                HeapOperand(obj, offset + Int64ConstantFrom(index)));
        __ B(&done);
        __ Bind(&uncompressed_load);
        __ Ldrh(Register(OutputCPURegister(instruction)),
                HeapOperand(obj, offset + (Int64ConstantFrom(index) << 1)));
        __ Bind(&done);
      } else {
        offset += Int64ConstantFrom(index) << DataType::SizeShift(type);
        source = HeapOperand(obj, offset);
      }
    } else {
      Register temp = temps.AcquireSameSizeAs(obj);
      if (instruction->GetArray()->IsIntermediateAddress()) {
        // We do not need to compute the intermediate address from the array: the
        // input instruction has done it already. See the comment in
        // `TryExtractArrayAccessAddress()`.
        if (kIsDebugBuild) {
          HIntermediateAddress* tmp = instruction->GetArray()->AsIntermediateAddress();
          DCHECK_EQ(tmp->GetOffset()->AsIntConstant()->GetValueAsUint64(), offset);
        }
        temp = obj;
      } else {
        __ Add(temp, obj, offset);
      }
      if (maybe_compressed_char_at) {
        vixl::aarch64::Label uncompressed_load, done;
        static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                      "Expecting 0=compressed, 1=uncompressed");
        __ Tbnz(length.W(), 0, &uncompressed_load);
        __ Ldrb(Register(OutputCPURegister(instruction)),
                HeapOperand(temp, XRegisterFrom(index), LSL, 0));
        __ B(&done);
        __ Bind(&uncompressed_load);
        __ Ldrh(Register(OutputCPURegister(instruction)),
                HeapOperand(temp, XRegisterFrom(index), LSL, 1));
        __ Bind(&done);
      } else {
        source = HeapOperand(temp, XRegisterFrom(index), LSL, DataType::SizeShift(type));
      }
    }
    if (!maybe_compressed_char_at) {
      // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      codegen_->Load(type, OutputCPURegister(instruction), source);
      codegen_->MaybeRecordImplicitNullCheck(instruction);
    }

    if (type == DataType::Type::kReference) {
      static_assert(
          sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
          "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
      Location obj_loc = locations->InAt(0);
      if (index.IsConstant()) {
        codegen_->MaybeGenerateReadBarrierSlow(instruction, out, out, obj_loc, offset);
      } else {
        codegen_->MaybeGenerateReadBarrierSlow(instruction, out, out, obj_loc, offset, index);
      }
    }
  }
}

void LocationsBuilderARM64::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitArrayLength(HArrayLength* instruction) {
  uint32_t offset = CodeGenerator::GetArrayLengthOffset(instruction);
  vixl::aarch64::Register out = OutputRegister(instruction);
  {
    // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
    EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
    __ Ldr(out, HeapOperand(InputRegisterAt(instruction, 0), offset));
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
  // Mask out compression flag from String's array length.
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    __ Lsr(out.W(), out.W(), 1u);
  }
}

void LocationsBuilderARM64::VisitArraySet(HArraySet* instruction) {
  DataType::Type value_type = instruction->GetComponentType();

  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction,
      may_need_runtime_call_for_type_check ?
          LocationSummary::kCallOnSlowPath :
          LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
  if (IsConstantZeroBitPattern(instruction->InputAt(2))) {
    locations->SetInAt(2, Location::ConstantLocation(instruction->InputAt(2)->AsConstant()));
  } else if (DataType::IsFloatingPointType(value_type)) {
    locations->SetInAt(2, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(2, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARM64::VisitArraySet(HArraySet* instruction) {
  DataType::Type value_type = instruction->GetComponentType();
  LocationSummary* locations = instruction->GetLocations();
  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());

  Register array = InputRegisterAt(instruction, 0);
  CPURegister value = InputCPURegisterOrZeroRegAt(instruction, 2);
  CPURegister source = value;
  Location index = locations->InAt(1);
  size_t offset = mirror::Array::DataOffset(DataType::Size(value_type)).Uint32Value();
  MemOperand destination = HeapOperand(array);
  MacroAssembler* masm = GetVIXLAssembler();

  if (!needs_write_barrier) {
    DCHECK(!may_need_runtime_call_for_type_check);
    if (index.IsConstant()) {
      offset += Int64ConstantFrom(index) << DataType::SizeShift(value_type);
      destination = HeapOperand(array, offset);
    } else {
      UseScratchRegisterScope temps(masm);
      Register temp = temps.AcquireSameSizeAs(array);
      if (instruction->GetArray()->IsIntermediateAddress()) {
        // We do not need to compute the intermediate address from the array: the
        // input instruction has done it already. See the comment in
        // `TryExtractArrayAccessAddress()`.
        if (kIsDebugBuild) {
          HIntermediateAddress* tmp = instruction->GetArray()->AsIntermediateAddress();
          DCHECK(tmp->GetOffset()->AsIntConstant()->GetValueAsUint64() == offset);
        }
        temp = array;
      } else {
        __ Add(temp, array, offset);
      }
      destination = HeapOperand(temp,
                                XRegisterFrom(index),
                                LSL,
                                DataType::SizeShift(value_type));
    }
    {
      // Ensure that between store and MaybeRecordImplicitNullCheck there are no pools emitted.
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      codegen_->Store(value_type, value, destination);
      codegen_->MaybeRecordImplicitNullCheck(instruction);
    }
  } else {
    DCHECK(!instruction->GetArray()->IsIntermediateAddress());
    vixl::aarch64::Label done;
    SlowPathCodeARM64* slow_path = nullptr;
    {
      // We use a block to end the scratch scope before the write barrier, thus
      // freeing the temporary registers so they can be used in `MarkGCCard`.
      UseScratchRegisterScope temps(masm);
      Register temp = temps.AcquireSameSizeAs(array);
      if (index.IsConstant()) {
        offset += Int64ConstantFrom(index) << DataType::SizeShift(value_type);
        destination = HeapOperand(array, offset);
      } else {
        destination = HeapOperand(temp,
                                  XRegisterFrom(index),
                                  LSL,
                                  DataType::SizeShift(value_type));
      }

      uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
      uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
      uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();

      if (may_need_runtime_call_for_type_check) {
        slow_path = new (codegen_->GetScopedAllocator()) ArraySetSlowPathARM64(instruction);
        codegen_->AddSlowPath(slow_path);
        if (instruction->GetValueCanBeNull()) {
          vixl::aarch64::Label non_zero;
          __ Cbnz(Register(value), &non_zero);
          if (!index.IsConstant()) {
            __ Add(temp, array, offset);
          }
          {
            // Ensure that between store and MaybeRecordImplicitNullCheck there are no pools
            // emitted.
            EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
            __ Str(wzr, destination);
            codegen_->MaybeRecordImplicitNullCheck(instruction);
          }
          __ B(&done);
          __ Bind(&non_zero);
        }

        // Note that when Baker read barriers are enabled, the type
        // checks are performed without read barriers.  This is fine,
        // even in the case where a class object is in the from-space
        // after the flip, as a comparison involving such a type would
        // not produce a false positive; it may of course produce a
        // false negative, in which case we would take the ArraySet
        // slow path.

        Register temp2 = temps.AcquireSameSizeAs(array);
        // /* HeapReference<Class> */ temp = array->klass_
        {
          // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
          EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
          __ Ldr(temp, HeapOperand(array, class_offset));
          codegen_->MaybeRecordImplicitNullCheck(instruction);
        }
        GetAssembler()->MaybeUnpoisonHeapReference(temp);

        // /* HeapReference<Class> */ temp = temp->component_type_
        __ Ldr(temp, HeapOperand(temp, component_offset));
        // /* HeapReference<Class> */ temp2 = value->klass_
        __ Ldr(temp2, HeapOperand(Register(value), class_offset));
        // If heap poisoning is enabled, no need to unpoison `temp`
        // nor `temp2`, as we are comparing two poisoned references.
        __ Cmp(temp, temp2);
        temps.Release(temp2);

        if (instruction->StaticTypeOfArrayIsObjectArray()) {
          vixl::aarch64::Label do_put;
          __ B(eq, &do_put);
          // If heap poisoning is enabled, the `temp` reference has
          // not been unpoisoned yet; unpoison it now.
          GetAssembler()->MaybeUnpoisonHeapReference(temp);

          // /* HeapReference<Class> */ temp = temp->super_class_
          __ Ldr(temp, HeapOperand(temp, super_offset));
          // If heap poisoning is enabled, no need to unpoison
          // `temp`, as we are comparing against null below.
          __ Cbnz(temp, slow_path->GetEntryLabel());
          __ Bind(&do_put);
        } else {
          __ B(ne, slow_path->GetEntryLabel());
        }
      }

      if (kPoisonHeapReferences) {
        Register temp2 = temps.AcquireSameSizeAs(array);
          DCHECK(value.IsW());
        __ Mov(temp2, value.W());
        GetAssembler()->PoisonHeapReference(temp2);
        source = temp2;
      }

      if (!index.IsConstant()) {
        __ Add(temp, array, offset);
      } else {
        // We no longer need the `temp` here so release it as the store below may
        // need a scratch register (if the constant index makes the offset too large)
        // and the poisoned `source` could be using the other scratch register.
        temps.Release(temp);
      }
      {
        // Ensure that between store and MaybeRecordImplicitNullCheck there are no pools emitted.
        EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
        __ Str(source, destination);

        if (!may_need_runtime_call_for_type_check) {
          codegen_->MaybeRecordImplicitNullCheck(instruction);
        }
      }
    }

    codegen_->MarkGCCard(array, value.W(), instruction->GetValueCanBeNull());

    if (done.IsLinked()) {
      __ Bind(&done);
    }

    if (slow_path != nullptr) {
      __ Bind(slow_path->GetExitLabel());
    }
  }
}

void LocationsBuilderARM64::VisitBoundsCheck(HBoundsCheck* instruction) {
  RegisterSet caller_saves = RegisterSet::Empty();
  InvokeRuntimeCallingConvention calling_convention;
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0).GetCode()));
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(1).GetCode()));
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction, caller_saves);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, ARM64EncodableConstantOrRegister(instruction->InputAt(1), instruction));
}

void InstructionCodeGeneratorARM64::VisitBoundsCheck(HBoundsCheck* instruction) {
  BoundsCheckSlowPathARM64* slow_path =
      new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathARM64(instruction);
  codegen_->AddSlowPath(slow_path);
  __ Cmp(InputRegisterAt(instruction, 0), InputOperandAt(instruction, 1));
  __ B(slow_path->GetEntryLabel(), hs);
}

void LocationsBuilderARM64::VisitClinitCheck(HClinitCheck* check) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(check, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (check->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorARM64::VisitClinitCheck(HClinitCheck* check) {
  // We assume the class is not null.
  SlowPathCodeARM64* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathARM64(
      check->GetLoadClass(), check, check->GetDexPc(), true);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path, InputRegisterAt(check, 0));
}

static bool IsFloatingPointZeroConstant(HInstruction* inst) {
  return (inst->IsFloatConstant() && (inst->AsFloatConstant()->IsArithmeticZero()))
      || (inst->IsDoubleConstant() && (inst->AsDoubleConstant()->IsArithmeticZero()));
}

void InstructionCodeGeneratorARM64::GenerateFcmp(HInstruction* instruction) {
  FPRegister lhs_reg = InputFPRegisterAt(instruction, 0);
  Location rhs_loc = instruction->GetLocations()->InAt(1);
  if (rhs_loc.IsConstant()) {
    // 0.0 is the only immediate that can be encoded directly in
    // an FCMP instruction.
    //
    // Both the JLS (section 15.20.1) and the JVMS (section 6.5)
    // specify that in a floating-point comparison, positive zero
    // and negative zero are considered equal, so we can use the
    // literal 0.0 for both cases here.
    //
    // Note however that some methods (Float.equal, Float.compare,
    // Float.compareTo, Double.equal, Double.compare,
    // Double.compareTo, Math.max, Math.min, StrictMath.max,
    // StrictMath.min) consider 0.0 to be (strictly) greater than
    // -0.0. So if we ever translate calls to these methods into a
    // HCompare instruction, we must handle the -0.0 case with
    // care here.
    DCHECK(IsFloatingPointZeroConstant(rhs_loc.GetConstant()));
    __ Fcmp(lhs_reg, 0.0);
  } else {
    __ Fcmp(lhs_reg, InputFPRegisterAt(instruction, 1));
  }
}

void LocationsBuilderARM64::VisitCompare(HCompare* compare) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(compare, LocationSummary::kNoCall);
  DataType::Type in_type = compare->InputAt(0)->GetType();
  switch (in_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, ARM64EncodableConstantOrRegister(compare->InputAt(1), compare));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1,
                         IsFloatingPointZeroConstant(compare->InputAt(1))
                             ? Location::ConstantLocation(compare->InputAt(1)->AsConstant())
                             : Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresRegister());
      break;
    }
    default:
      LOG(FATAL) << "Unexpected type for compare operation " << in_type;
  }
}

void InstructionCodeGeneratorARM64::VisitCompare(HCompare* compare) {
  DataType::Type in_type = compare->InputAt(0)->GetType();

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
      Register result = OutputRegister(compare);
      Register left = InputRegisterAt(compare, 0);
      Operand right = InputOperandAt(compare, 1);
      __ Cmp(left, right);
      __ Cset(result, ne);          // result == +1 if NE or 0 otherwise
      __ Cneg(result, result, lt);  // result == -1 if LT or unchanged otherwise
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      Register result = OutputRegister(compare);
      GenerateFcmp(compare);
      __ Cset(result, ne);
      __ Cneg(result, result, ARM64FPCondition(kCondLT, compare->IsGtBias()));
      break;
    }
    default:
      LOG(FATAL) << "Unimplemented compare type " << in_type;
  }
}

void LocationsBuilderARM64::HandleCondition(HCondition* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);

  if (DataType::IsFloatingPointType(instruction->InputAt(0)->GetType())) {
    locations->SetInAt(0, Location::RequiresFpuRegister());
    locations->SetInAt(1,
                       IsFloatingPointZeroConstant(instruction->InputAt(1))
                           ? Location::ConstantLocation(instruction->InputAt(1)->AsConstant())
                           : Location::RequiresFpuRegister());
  } else {
    // Integer cases.
    locations->SetInAt(0, Location::RequiresRegister());
    locations->SetInAt(1, ARM64EncodableConstantOrRegister(instruction->InputAt(1), instruction));
  }

  if (!instruction->IsEmittedAtUseSite()) {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorARM64::HandleCondition(HCondition* instruction) {
  if (instruction->IsEmittedAtUseSite()) {
    return;
  }

  LocationSummary* locations = instruction->GetLocations();
  Register res = RegisterFrom(locations->Out(), instruction->GetType());
  IfCondition if_cond = instruction->GetCondition();

  if (DataType::IsFloatingPointType(instruction->InputAt(0)->GetType())) {
    GenerateFcmp(instruction);
    __ Cset(res, ARM64FPCondition(if_cond, instruction->IsGtBias()));
  } else {
    // Integer cases.
    Register lhs = InputRegisterAt(instruction, 0);
    Operand rhs = InputOperandAt(instruction, 1);
    __ Cmp(lhs, rhs);
    __ Cset(res, ARM64Condition(if_cond));
  }
}

#define FOR_EACH_CONDITION_INSTRUCTION(M)                                                \
  M(Equal)                                                                               \
  M(NotEqual)                                                                            \
  M(LessThan)                                                                            \
  M(LessThanOrEqual)                                                                     \
  M(GreaterThan)                                                                         \
  M(GreaterThanOrEqual)                                                                  \
  M(Below)                                                                               \
  M(BelowOrEqual)                                                                        \
  M(Above)                                                                               \
  M(AboveOrEqual)
#define DEFINE_CONDITION_VISITORS(Name)                                                  \
void LocationsBuilderARM64::Visit##Name(H##Name* comp) { HandleCondition(comp); }         \
void InstructionCodeGeneratorARM64::Visit##Name(H##Name* comp) { HandleCondition(comp); }
FOR_EACH_CONDITION_INSTRUCTION(DEFINE_CONDITION_VISITORS)
#undef DEFINE_CONDITION_VISITORS
#undef FOR_EACH_CONDITION_INSTRUCTION

void InstructionCodeGeneratorARM64::DivRemOneOrMinusOne(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  Register out = OutputRegister(instruction);
  Register dividend = InputRegisterAt(instruction, 0);
  int64_t imm = Int64FromConstant(second.GetConstant());
  DCHECK(imm == 1 || imm == -1);

  if (instruction->IsRem()) {
    __ Mov(out, 0);
  } else {
    if (imm == 1) {
      __ Mov(out, dividend);
    } else {
      __ Neg(out, dividend);
    }
  }
}

void InstructionCodeGeneratorARM64::DivRemByPowerOfTwo(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  Register out = OutputRegister(instruction);
  Register dividend = InputRegisterAt(instruction, 0);
  int64_t imm = Int64FromConstant(second.GetConstant());
  uint64_t abs_imm = static_cast<uint64_t>(AbsOrMin(imm));
  int ctz_imm = CTZ(abs_imm);

  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register temp = temps.AcquireSameSizeAs(out);

  if (instruction->IsDiv()) {
    __ Add(temp, dividend, abs_imm - 1);
    __ Cmp(dividend, 0);
    __ Csel(out, temp, dividend, lt);
    if (imm > 0) {
      __ Asr(out, out, ctz_imm);
    } else {
      __ Neg(out, Operand(out, ASR, ctz_imm));
    }
  } else {
    int bits = instruction->GetResultType() == DataType::Type::kInt32 ? 32 : 64;
    __ Asr(temp, dividend, bits - 1);
    __ Lsr(temp, temp, bits - ctz_imm);
    __ Add(out, dividend, temp);
    __ And(out, out, abs_imm - 1);
    __ Sub(out, out, temp);
  }
}

void InstructionCodeGeneratorARM64::GenerateDivRemWithAnyConstant(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  Register out = OutputRegister(instruction);
  Register dividend = InputRegisterAt(instruction, 0);
  int64_t imm = Int64FromConstant(second.GetConstant());

  DataType::Type type = instruction->GetResultType();
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  int64_t magic;
  int shift;
  CalculateMagicAndShiftForDivRem(
      imm, type == DataType::Type::kInt64 /* is_long */, &magic, &shift);

  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register temp = temps.AcquireSameSizeAs(out);

  // temp = get_high(dividend * magic)
  __ Mov(temp, magic);
  if (type == DataType::Type::kInt64) {
    __ Smulh(temp, dividend, temp);
  } else {
    __ Smull(temp.X(), dividend, temp);
    __ Lsr(temp.X(), temp.X(), 32);
  }

  if (imm > 0 && magic < 0) {
    __ Add(temp, temp, dividend);
  } else if (imm < 0 && magic > 0) {
    __ Sub(temp, temp, dividend);
  }

  if (shift != 0) {
    __ Asr(temp, temp, shift);
  }

  if (instruction->IsDiv()) {
    __ Sub(out, temp, Operand(temp, ASR, type == DataType::Type::kInt64 ? 63 : 31));
  } else {
    __ Sub(temp, temp, Operand(temp, ASR, type == DataType::Type::kInt64 ? 63 : 31));
    // TODO: Strength reduction for msub.
    Register temp_imm = temps.AcquireSameSizeAs(out);
    __ Mov(temp_imm, imm);
    __ Msub(out, temp, temp_imm, dividend);
  }
}

void InstructionCodeGeneratorARM64::GenerateDivRemIntegral(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DataType::Type type = instruction->GetResultType();
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  LocationSummary* locations = instruction->GetLocations();
  Register out = OutputRegister(instruction);
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
    Register dividend = InputRegisterAt(instruction, 0);
    Register divisor = InputRegisterAt(instruction, 1);
    if (instruction->IsDiv()) {
      __ Sdiv(out, dividend, divisor);
    } else {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      Register temp = temps.AcquireSameSizeAs(out);
      __ Sdiv(temp, dividend, divisor);
      __ Msub(out, temp, divisor, dividend);
    }
  }
}

void LocationsBuilderARM64::VisitDiv(HDiv* div) {
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

void InstructionCodeGeneratorARM64::VisitDiv(HDiv* div) {
  DataType::Type type = div->GetResultType();
  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      GenerateDivRemIntegral(div);
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Fdiv(OutputFPRegister(div), InputFPRegisterAt(div, 0), InputFPRegisterAt(div, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << type;
  }
}

void LocationsBuilderARM64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RegisterOrConstant(instruction->InputAt(0)));
}

void InstructionCodeGeneratorARM64::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  SlowPathCodeARM64* slow_path =
      new (codegen_->GetScopedAllocator()) DivZeroCheckSlowPathARM64(instruction);
  codegen_->AddSlowPath(slow_path);
  Location value = instruction->GetLocations()->InAt(0);

  DataType::Type type = instruction->GetType();

  if (!DataType::IsIntegralType(type)) {
    LOG(FATAL) << "Unexpected type " << type << " for DivZeroCheck.";
    return;
  }

  if (value.IsConstant()) {
    int64_t divisor = Int64ConstantFrom(value);
    if (divisor == 0) {
      __ B(slow_path->GetEntryLabel());
    } else {
      // A division by a non-null constant is valid. We don't need to perform
      // any check, so simply fall through.
    }
  } else {
    __ Cbz(InputRegisterAt(instruction, 0), slow_path->GetEntryLabel());
  }
}

void LocationsBuilderARM64::VisitDoubleConstant(HDoubleConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARM64::VisitDoubleConstant(
    HDoubleConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARM64::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitExit(HExit* exit ATTRIBUTE_UNUSED) {
}

void LocationsBuilderARM64::VisitFloatConstant(HFloatConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARM64::VisitFloatConstant(HFloatConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void InstructionCodeGeneratorARM64::HandleGoto(HInstruction* got, HBasicBlock* successor) {
  if (successor->IsExitBlock()) {
    DCHECK(got->GetPrevious()->AlwaysThrows());
    return;  // no code needed
  }

  HBasicBlock* block = got->GetBlock();
  HInstruction* previous = got->GetPrevious();
  HLoopInformation* info = block->GetLoopInformation();

  if (info != nullptr && info->IsBackEdge(*block) && info->HasSuspendCheck()) {
    if (codegen_->GetCompilerOptions().CountHotnessInCompiledCode()) {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      Register temp1 = temps.AcquireX();
      Register temp2 = temps.AcquireX();
      __ Ldr(temp1, MemOperand(sp, 0));
      __ Ldrh(temp2, MemOperand(temp1, ArtMethod::HotnessCountOffset().Int32Value()));
      __ Add(temp2, temp2, 1);
      __ Strh(temp2, MemOperand(temp1, ArtMethod::HotnessCountOffset().Int32Value()));
    }
    GenerateSuspendCheck(info->GetSuspendCheck(), successor);
    return;
  }
  if (block->IsEntryBlock() && (previous != nullptr) && previous->IsSuspendCheck()) {
    GenerateSuspendCheck(previous->AsSuspendCheck(), nullptr);
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
  }
  if (!codegen_->GoesToNextBlock(block, successor)) {
    __ B(codegen_->GetLabelOf(successor));
  }
}

void LocationsBuilderARM64::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitGoto(HGoto* got) {
  HandleGoto(got, got->GetSuccessor());
}

void LocationsBuilderARM64::VisitTryBoundary(HTryBoundary* try_boundary) {
  try_boundary->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitTryBoundary(HTryBoundary* try_boundary) {
  HBasicBlock* successor = try_boundary->GetNormalFlowSuccessor();
  if (!successor->IsExitBlock()) {
    HandleGoto(try_boundary, successor);
  }
}

void InstructionCodeGeneratorARM64::GenerateTestAndBranch(HInstruction* instruction,
                                                          size_t condition_input_index,
                                                          vixl::aarch64::Label* true_target,
                                                          vixl::aarch64::Label* false_target) {
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
      __ Cbz(InputRegisterAt(instruction, condition_input_index), false_target);
    } else {
      __ Cbnz(InputRegisterAt(instruction, condition_input_index), true_target);
    }
  } else {
    // The condition instruction has not been materialized, use its inputs as
    // the comparison and its condition as the branch condition.
    HCondition* condition = cond->AsCondition();

    DataType::Type type = condition->InputAt(0)->GetType();
    if (DataType::IsFloatingPointType(type)) {
      GenerateFcmp(condition);
      if (true_target == nullptr) {
        IfCondition opposite_condition = condition->GetOppositeCondition();
        __ B(ARM64FPCondition(opposite_condition, condition->IsGtBias()), false_target);
      } else {
        __ B(ARM64FPCondition(condition->GetCondition(), condition->IsGtBias()), true_target);
      }
    } else {
      // Integer cases.
      Register lhs = InputRegisterAt(condition, 0);
      Operand rhs = InputOperandAt(condition, 1);

      Condition arm64_cond;
      vixl::aarch64::Label* non_fallthrough_target;
      if (true_target == nullptr) {
        arm64_cond = ARM64Condition(condition->GetOppositeCondition());
        non_fallthrough_target = false_target;
      } else {
        arm64_cond = ARM64Condition(condition->GetCondition());
        non_fallthrough_target = true_target;
      }

      if ((arm64_cond == eq || arm64_cond == ne || arm64_cond == lt || arm64_cond == ge) &&
          rhs.IsImmediate() && (rhs.GetImmediate() == 0)) {
        switch (arm64_cond) {
          case eq:
            __ Cbz(lhs, non_fallthrough_target);
            break;
          case ne:
            __ Cbnz(lhs, non_fallthrough_target);
            break;
          case lt:
            // Test the sign bit and branch accordingly.
            __ Tbnz(lhs, (lhs.IsX() ? kXRegSize : kWRegSize) - 1, non_fallthrough_target);
            break;
          case ge:
            // Test the sign bit and branch accordingly.
            __ Tbz(lhs, (lhs.IsX() ? kXRegSize : kWRegSize) - 1, non_fallthrough_target);
            break;
          default:
            // Without the `static_cast` the compiler throws an error for
            // `-Werror=sign-promo`.
            LOG(FATAL) << "Unexpected condition: " << static_cast<int>(arm64_cond);
        }
      } else {
        __ Cmp(lhs, rhs);
        __ B(arm64_cond, non_fallthrough_target);
      }
    }
  }

  // If neither branch falls through (case 3), the conditional branch to `true_target`
  // was already emitted (case 2) and we need to emit a jump to `false_target`.
  if (true_target != nullptr && false_target != nullptr) {
    __ B(false_target);
  }
}

void LocationsBuilderARM64::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(if_instr);
  if (IsBooleanValueOrMaterializedCondition(if_instr->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARM64::VisitIf(HIf* if_instr) {
  HBasicBlock* true_successor = if_instr->IfTrueSuccessor();
  HBasicBlock* false_successor = if_instr->IfFalseSuccessor();
  vixl::aarch64::Label* true_target = codegen_->GetLabelOf(true_successor);
  if (codegen_->GoesToNextBlock(if_instr->GetBlock(), true_successor)) {
    true_target = nullptr;
  }
  vixl::aarch64::Label* false_target = codegen_->GetLabelOf(false_successor);
  if (codegen_->GoesToNextBlock(if_instr->GetBlock(), false_successor)) {
    false_target = nullptr;
  }
  GenerateTestAndBranch(if_instr, /* condition_input_index */ 0, true_target, false_target);
}

void LocationsBuilderARM64::VisitDeoptimize(HDeoptimize* deoptimize) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(deoptimize, LocationSummary::kCallOnSlowPath);
  InvokeRuntimeCallingConvention calling_convention;
  RegisterSet caller_saves = RegisterSet::Empty();
  caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0).GetCode()));
  locations->SetCustomSlowPathCallerSaves(caller_saves);
  if (IsBooleanValueOrMaterializedCondition(deoptimize->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARM64::VisitDeoptimize(HDeoptimize* deoptimize) {
  SlowPathCodeARM64* slow_path =
      deopt_slow_paths_.NewSlowPath<DeoptimizationSlowPathARM64>(deoptimize);
  GenerateTestAndBranch(deoptimize,
                        /* condition_input_index */ 0,
                        slow_path->GetEntryLabel(),
                        /* false_target */ nullptr);
}

void LocationsBuilderARM64::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(flag, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARM64::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  __ Ldr(OutputRegister(flag),
         MemOperand(sp, codegen_->GetStackOffsetOfShouldDeoptimizeFlag()));
}

static inline bool IsConditionOnFloatingPointValues(HInstruction* condition) {
  return condition->IsCondition() &&
         DataType::IsFloatingPointType(condition->InputAt(0)->GetType());
}

static inline Condition GetConditionForSelect(HCondition* condition) {
  IfCondition cond = condition->AsCondition()->GetCondition();
  return IsConditionOnFloatingPointValues(condition) ? ARM64FPCondition(cond, condition->IsGtBias())
                                                     : ARM64Condition(cond);
}

void LocationsBuilderARM64::VisitSelect(HSelect* select) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(select);
  if (DataType::IsFloatingPointType(select->GetType())) {
    locations->SetInAt(0, Location::RequiresFpuRegister());
    locations->SetInAt(1, Location::RequiresFpuRegister());
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    HConstant* cst_true_value = select->GetTrueValue()->AsConstant();
    HConstant* cst_false_value = select->GetFalseValue()->AsConstant();
    bool is_true_value_constant = cst_true_value != nullptr;
    bool is_false_value_constant = cst_false_value != nullptr;
    // Ask VIXL whether we should synthesize constants in registers.
    // We give an arbitrary register to VIXL when dealing with non-constant inputs.
    Operand true_op = is_true_value_constant ?
        Operand(Int64FromConstant(cst_true_value)) : Operand(x1);
    Operand false_op = is_false_value_constant ?
        Operand(Int64FromConstant(cst_false_value)) : Operand(x2);
    bool true_value_in_register = false;
    bool false_value_in_register = false;
    MacroAssembler::GetCselSynthesisInformation(
        x0, true_op, false_op, &true_value_in_register, &false_value_in_register);
    true_value_in_register |= !is_true_value_constant;
    false_value_in_register |= !is_false_value_constant;

    locations->SetInAt(1, true_value_in_register ? Location::RequiresRegister()
                                                 : Location::ConstantLocation(cst_true_value));
    locations->SetInAt(0, false_value_in_register ? Location::RequiresRegister()
                                                  : Location::ConstantLocation(cst_false_value));
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }

  if (IsBooleanValueOrMaterializedCondition(select->GetCondition())) {
    locations->SetInAt(2, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARM64::VisitSelect(HSelect* select) {
  HInstruction* cond = select->GetCondition();
  Condition csel_cond;

  if (IsBooleanValueOrMaterializedCondition(cond)) {
    if (cond->IsCondition() && cond->GetNext() == select) {
      // Use the condition flags set by the previous instruction.
      csel_cond = GetConditionForSelect(cond->AsCondition());
    } else {
      __ Cmp(InputRegisterAt(select, 2), 0);
      csel_cond = ne;
    }
  } else if (IsConditionOnFloatingPointValues(cond)) {
    GenerateFcmp(cond);
    csel_cond = GetConditionForSelect(cond->AsCondition());
  } else {
    __ Cmp(InputRegisterAt(cond, 0), InputOperandAt(cond, 1));
    csel_cond = GetConditionForSelect(cond->AsCondition());
  }

  if (DataType::IsFloatingPointType(select->GetType())) {
    __ Fcsel(OutputFPRegister(select),
             InputFPRegisterAt(select, 1),
             InputFPRegisterAt(select, 0),
             csel_cond);
  } else {
    __ Csel(OutputRegister(select),
            InputOperandAt(select, 1),
            InputOperandAt(select, 0),
            csel_cond);
  }
}

void LocationsBuilderARM64::VisitNativeDebugInfo(HNativeDebugInfo* info) {
  new (GetGraph()->GetAllocator()) LocationSummary(info);
}

void InstructionCodeGeneratorARM64::VisitNativeDebugInfo(HNativeDebugInfo*) {
  // MaybeRecordNativeDebugInfo is already called implicitly in CodeGenerator::Compile.
}

void CodeGeneratorARM64::GenerateNop() {
  __ Nop();
}

void LocationsBuilderARM64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARM64::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderARM64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction);
}

void InstructionCodeGeneratorARM64::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

// Temp is used for read barrier.
static size_t NumberOfInstanceOfTemps(TypeCheckKind type_check_kind) {
  if (kEmitCompilerReadBarrier &&
      (kUseBakerReadBarrier ||
          type_check_kind == TypeCheckKind::kAbstractClassCheck ||
          type_check_kind == TypeCheckKind::kClassHierarchyCheck ||
          type_check_kind == TypeCheckKind::kArrayObjectCheck)) {
    return 1;
  }
  return 0;
}

// Interface case has 3 temps, one for holding the number of interfaces, one for the current
// interface pointer, one for loading the current interface.
// The other checks have one temp for loading the object's class.
static size_t NumberOfCheckCastTemps(TypeCheckKind type_check_kind) {
  if (type_check_kind == TypeCheckKind::kInterfaceCheck) {
    return 3;
  }
  return 1 + NumberOfInstanceOfTemps(type_check_kind);
}

void LocationsBuilderARM64::VisitInstanceOf(HInstanceOf* instruction) {
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
  // The "out" register is used as a temporary, so it overlaps with the inputs.
  // Note that TypeCheckSlowPathARM64 uses this register too.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
  // Add temps if necessary for read barriers.
  locations->AddRegisterTemps(NumberOfInstanceOfTemps(type_check_kind));
}

void InstructionCodeGeneratorARM64::VisitInstanceOf(HInstanceOf* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = InputRegisterAt(instruction, 0);
  Register cls = InputRegisterAt(instruction, 1);
  Location out_loc = locations->Out();
  Register out = OutputRegister(instruction);
  const size_t num_temps = NumberOfInstanceOfTemps(type_check_kind);
  DCHECK_LE(num_temps, 1u);
  Location maybe_temp_loc = (num_temps >= 1) ? locations->GetTemp(0) : Location::NoLocation();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();

  vixl::aarch64::Label done, zero;
  SlowPathCodeARM64* slow_path = nullptr;

  // Return 0 if `obj` is null.
  // Avoid null check if we know `obj` is not null.
  if (instruction->MustDoNullCheck()) {
    __ Cbz(obj, &zero);
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
      __ Cmp(out, cls);
      __ Cset(out, eq);
      if (zero.IsLinked()) {
        __ B(&done);
      }
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
      vixl::aarch64::Label loop, success;
      __ Bind(&loop);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Cbz(out, &done);
      __ Cmp(out, cls);
      __ B(ne, &loop);
      __ Mov(out, 1);
      if (zero.IsLinked()) {
        __ B(&done);
      }
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
      vixl::aarch64::Label loop, success;
      __ Bind(&loop);
      __ Cmp(out, cls);
      __ B(eq, &success);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      __ Cbnz(out, &loop);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ B(&done);
      __ Bind(&success);
      __ Mov(out, 1);
      if (zero.IsLinked()) {
        __ B(&done);
      }
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
      vixl::aarch64::Label exact_check;
      __ Cmp(out, cls);
      __ B(eq, &exact_check);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ out = out->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       component_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to `done`.
      __ Cbz(out, &done);
      __ Ldrh(out, HeapOperand(out, primitive_offset));
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Cbnz(out, &zero);
      __ Bind(&exact_check);
      __ Mov(out, 1);
      __ B(&done);
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
      __ Cmp(out, cls);
      DCHECK(locations->OnlyCallsOnSlowPath());
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARM64(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ B(ne, slow_path->GetEntryLabel());
      __ Mov(out, 1);
      if (zero.IsLinked()) {
        __ B(&done);
      }
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
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARM64(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      if (zero.IsLinked()) {
        __ B(&done);
      }
      break;
    }
  }

  if (zero.IsLinked()) {
    __ Bind(&zero);
    __ Mov(out, 0);
  }

  if (done.IsLinked()) {
    __ Bind(&done);
  }

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void LocationsBuilderARM64::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary::CallKind call_kind = CodeGenerator::GetCheckCastCallKind(instruction);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  // Add temps for read barriers and other uses. One is used by TypeCheckSlowPathARM64.
  locations->AddRegisterTemps(NumberOfCheckCastTemps(type_check_kind));
}

void InstructionCodeGeneratorARM64::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  Register obj = InputRegisterAt(instruction, 0);
  Register cls = InputRegisterAt(instruction, 1);
  const size_t num_temps = NumberOfCheckCastTemps(type_check_kind);
  DCHECK_GE(num_temps, 1u);
  DCHECK_LE(num_temps, 3u);
  Location temp_loc = locations->GetTemp(0);
  Location maybe_temp2_loc = (num_temps >= 2) ? locations->GetTemp(1) : Location::NoLocation();
  Location maybe_temp3_loc = (num_temps >= 3) ? locations->GetTemp(2) : Location::NoLocation();
  Register temp = WRegisterFrom(temp_loc);
  const uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  const uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  const uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  const uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  const uint32_t iftable_offset = mirror::Class::IfTableOffset().Uint32Value();
  const uint32_t array_length_offset = mirror::Array::LengthOffset().Uint32Value();
  const uint32_t object_array_data_offset =
      mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();

  bool is_type_check_slow_path_fatal = CodeGenerator::IsTypeCheckSlowPathFatal(instruction);
  SlowPathCodeARM64* type_check_slow_path =
      new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARM64(
          instruction, is_type_check_slow_path_fatal);
  codegen_->AddSlowPath(type_check_slow_path);

  vixl::aarch64::Label done;
  // Avoid null check if we know obj is not null.
  if (instruction->MustDoNullCheck()) {
    __ Cbz(obj, &done);
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

      __ Cmp(temp, cls);
      // Jump to slow path for throwing the exception or doing a
      // more involved array check.
      __ B(ne, type_check_slow_path->GetEntryLabel());
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
      vixl::aarch64::Label loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);

      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception.
      __ Cbz(temp, type_check_slow_path->GetEntryLabel());
      // Otherwise, compare classes.
      __ Cmp(temp, cls);
      __ B(ne, &loop);
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
      vixl::aarch64::Label loop;
      __ Bind(&loop);
      __ Cmp(temp, cls);
      __ B(eq, &done);

      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);

      // If the class reference currently in `temp` is not null, jump
      // back at the beginning of the loop.
      __ Cbnz(temp, &loop);
      // Otherwise, jump to the slow path to throw the exception.
      __ B(type_check_slow_path->GetEntryLabel());
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
      __ Cmp(temp, cls);
      __ B(eq, &done);

      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ temp = temp->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       component_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);

      // If the component type is null, jump to the slow path to throw the exception.
      __ Cbz(temp, type_check_slow_path->GetEntryLabel());
      // Otherwise, the object is indeed an array. Further check that this component type is not a
      // primitive type.
      __ Ldrh(temp, HeapOperand(temp, primitive_offset));
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Cbnz(temp, type_check_slow_path->GetEntryLabel());
      break;
    }

    case TypeCheckKind::kUnresolvedCheck:
      // We always go into the type check slow path for the unresolved check cases.
      //
      // We cannot directly call the CheckCast runtime entry point
      // without resorting to a type checking slow path here (i.e. by
      // calling InvokeRuntime directly), as it would require to
      // assign fixed registers for the inputs of this HInstanceOf
      // instruction (following the runtime calling convention), which
      // might be cluttered by the potential first read barrier
      // emission at the beginning of this method.
      __ B(type_check_slow_path->GetEntryLabel());
      break;
    case TypeCheckKind::kInterfaceCheck: {
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
      __ Ldr(WRegisterFrom(maybe_temp2_loc), HeapOperand(temp.W(), array_length_offset));
      // Loop through the iftable and check if any class matches.
      vixl::aarch64::Label start_loop;
      __ Bind(&start_loop);
      __ Cbz(WRegisterFrom(maybe_temp2_loc), type_check_slow_path->GetEntryLabel());
      __ Ldr(WRegisterFrom(maybe_temp3_loc), HeapOperand(temp.W(), object_array_data_offset));
      GetAssembler()->MaybeUnpoisonHeapReference(WRegisterFrom(maybe_temp3_loc));
      // Go to next interface.
      __ Add(temp, temp, 2 * kHeapReferenceSize);
      __ Sub(WRegisterFrom(maybe_temp2_loc), WRegisterFrom(maybe_temp2_loc), 2);
      // Compare the classes and continue the loop if they do not match.
      __ Cmp(cls, WRegisterFrom(maybe_temp3_loc));
      __ B(ne, &start_loop);
      break;
    }
  }
  __ Bind(&done);

  __ Bind(type_check_slow_path->GetExitLabel());
}

void LocationsBuilderARM64::VisitIntConstant(HIntConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARM64::VisitIntConstant(HIntConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARM64::VisitNullConstant(HNullConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARM64::VisitNullConstant(HNullConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARM64::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  // The trampoline uses the same calling convention as dex calling conventions,
  // except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
  // the method_idx.
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARM64::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  codegen_->GenerateInvokeUnresolvedRuntimeCall(invoke);
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::HandleInvoke(HInvoke* invoke) {
  InvokeDexCallingConventionVisitorARM64 calling_convention_visitor;
  CodeGenerator::CreateCommonInvokeLocationSummary(invoke, &calling_convention_visitor);
}

void LocationsBuilderARM64::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARM64::VisitInvokeInterface(HInvokeInterface* invoke) {
  // TODO: b/18116999, our IMTs can miss an IncompatibleClassChangeError.
  LocationSummary* locations = invoke->GetLocations();
  Register temp = XRegisterFrom(locations->GetTemp(0));
  Location receiver = locations->InAt(0);
  Offset class_offset = mirror::Object::ClassOffset();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArm64PointerSize);

  // The register ip1 is required to be used for the hidden argument in
  // art_quick_imt_conflict_trampoline, so prevent VIXL from using it.
  MacroAssembler* masm = GetVIXLAssembler();
  UseScratchRegisterScope scratch_scope(masm);
  scratch_scope.Exclude(ip1);
  __ Mov(ip1, invoke->GetDexMethodIndex());

  // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
  if (receiver.IsStackSlot()) {
    __ Ldr(temp.W(), StackOperandFrom(receiver));
    {
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      // /* HeapReference<Class> */ temp = temp->klass_
      __ Ldr(temp.W(), HeapOperand(temp.W(), class_offset));
      codegen_->MaybeRecordImplicitNullCheck(invoke);
    }
  } else {
    EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
    // /* HeapReference<Class> */ temp = receiver->klass_
    __ Ldr(temp.W(), HeapOperandFrom(receiver, class_offset));
    codegen_->MaybeRecordImplicitNullCheck(invoke);
  }

  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  GetAssembler()->MaybeUnpoisonHeapReference(temp.W());
  __ Ldr(temp,
      MemOperand(temp, mirror::Class::ImtPtrOffset(kArm64PointerSize).Uint32Value()));
  uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
      invoke->GetImtIndex(), kArm64PointerSize));
  // temp = temp->GetImtEntryAt(method_offset);
  __ Ldr(temp, MemOperand(temp, method_offset));
  // lr = temp->GetEntryPoint();
  __ Ldr(lr, MemOperand(temp, entry_point.Int32Value()));

  {
    // Ensure the pc position is recorded immediately after the `blr` instruction.
    ExactAssemblyScope eas(GetVIXLAssembler(), kInstructionSize, CodeBufferCheckScope::kExactSize);

    // lr();
    __ blr(lr);
    DCHECK(!codegen_->IsLeafMethod());
    codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
  }

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  IntrinsicLocationsBuilderARM64 intrinsic(GetGraph()->GetAllocator(), codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

void LocationsBuilderARM64::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  IntrinsicLocationsBuilderARM64 intrinsic(GetGraph()->GetAllocator(), codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

static bool TryGenerateIntrinsicCode(HInvoke* invoke, CodeGeneratorARM64* codegen) {
  if (invoke->GetLocations()->Intrinsified()) {
    IntrinsicCodeGeneratorARM64 intrinsic(codegen);
    intrinsic.Dispatch(invoke);
    return true;
  }
  return false;
}

HInvokeStaticOrDirect::DispatchInfo CodeGeneratorARM64::GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
  // On ARM64 we support all dispatch types.
  return desired_dispatch_info;
}

void CodeGeneratorARM64::GenerateStaticOrDirectCall(
    HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path) {
  // Make sure that ArtMethod* is passed in kArtMethodRegister as per the calling convention.
  Location callee_method = temp;  // For all kinds except kRecursive, callee will be in temp.
  switch (invoke->GetMethodLoadKind()) {
    case HInvokeStaticOrDirect::MethodLoadKind::kStringInit: {
      uint32_t offset =
          GetThreadOffset<kArm64PointerSize>(invoke->GetStringInitEntryPoint()).Int32Value();
      // temp = thread->string_init_entrypoint
      __ Ldr(XRegisterFrom(temp), MemOperand(tr, offset));
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRecursive:
      callee_method = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(GetCompilerOptions().IsBootImage());
      // Add ADRP with its PC-relative method patch.
      vixl::aarch64::Label* adrp_label = NewBootImageMethodPatch(invoke->GetTargetMethod());
      EmitAdrpPlaceholder(adrp_label, XRegisterFrom(temp));
      // Add ADD with its PC-relative method patch.
      vixl::aarch64::Label* add_label =
          NewBootImageMethodPatch(invoke->GetTargetMethod(), adrp_label);
      EmitAddPlaceholder(add_label, XRegisterFrom(temp), XRegisterFrom(temp));
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kDirectAddress:
      // Load method address from literal pool.
      __ Ldr(XRegisterFrom(temp), DeduplicateUint64Literal(invoke->GetMethodAddress()));
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBssEntry: {
      // Add ADRP with its PC-relative DexCache access patch.
      MethodReference target_method(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex());
      vixl::aarch64::Label* adrp_label = NewMethodBssEntryPatch(target_method);
      EmitAdrpPlaceholder(adrp_label, XRegisterFrom(temp));
      // Add LDR with its PC-relative DexCache access patch.
      vixl::aarch64::Label* ldr_label =
          NewMethodBssEntryPatch(target_method, adrp_label);
      EmitLdrOffsetPlaceholder(ldr_label, XRegisterFrom(temp), XRegisterFrom(temp));
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRuntimeCall: {
      GenerateInvokeStaticOrDirectRuntimeCall(invoke, temp, slow_path);
      return;  // No code pointer retrieval; the runtime performs the call directly.
    }
  }

  switch (invoke->GetCodePtrLocation()) {
    case HInvokeStaticOrDirect::CodePtrLocation::kCallSelf:
      {
        // Use a scope to help guarantee that `RecordPcInfo()` records the correct pc.
        ExactAssemblyScope eas(GetVIXLAssembler(),
                               kInstructionSize,
                               CodeBufferCheckScope::kExactSize);
        __ bl(&frame_entry_label_);
        RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      }
      break;
    case HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod:
      // LR = callee_method->entry_point_from_quick_compiled_code_;
      __ Ldr(lr, MemOperand(
          XRegisterFrom(callee_method),
          ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArm64PointerSize).Int32Value()));
      {
        // Use a scope to help guarantee that `RecordPcInfo()` records the correct pc.
        ExactAssemblyScope eas(GetVIXLAssembler(),
                               kInstructionSize,
                               CodeBufferCheckScope::kExactSize);
        // lr()
        __ blr(lr);
        RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      }
      break;
  }

  DCHECK(!IsLeafMethod());
}

void CodeGeneratorARM64::GenerateVirtualCall(
    HInvokeVirtual* invoke, Location temp_in, SlowPathCode* slow_path) {
  // Use the calling convention instead of the location of the receiver, as
  // intrinsics may have put the receiver in a different register. In the intrinsics
  // slow path, the arguments have been moved to the right place, so here we are
  // guaranteed that the receiver is the first register of the calling convention.
  InvokeDexCallingConvention calling_convention;
  Register receiver = calling_convention.GetRegisterAt(0);
  Register temp = XRegisterFrom(temp_in);
  size_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), kArm64PointerSize).SizeValue();
  Offset class_offset = mirror::Object::ClassOffset();
  Offset entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArm64PointerSize);

  DCHECK(receiver.IsRegister());

  {
    // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
    EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
    // /* HeapReference<Class> */ temp = receiver->klass_
    __ Ldr(temp.W(), HeapOperandFrom(LocationFrom(receiver), class_offset));
    MaybeRecordImplicitNullCheck(invoke);
  }
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  GetAssembler()->MaybeUnpoisonHeapReference(temp.W());
  // temp = temp->GetMethodAt(method_offset);
  __ Ldr(temp, MemOperand(temp, method_offset));
  // lr = temp->GetEntryPoint();
  __ Ldr(lr, MemOperand(temp, entry_point.SizeValue()));
  {
    // Use a scope to help guarantee that `RecordPcInfo()` records the correct pc.
    ExactAssemblyScope eas(GetVIXLAssembler(), kInstructionSize, CodeBufferCheckScope::kExactSize);
    // lr();
    __ blr(lr);
    RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
  }
}

void LocationsBuilderARM64::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARM64::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  codegen_->GenerateInvokePolymorphicCall(invoke);
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewBootImageMethodPatch(
    MethodReference target_method,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, adrp_label, &boot_image_method_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewMethodBssEntryPatch(
    MethodReference target_method,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, adrp_label, &method_bss_entry_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewBootImageTypePatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(&dex_file, type_index.index_, adrp_label, &boot_image_type_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewBssEntryTypePatch(
    const DexFile& dex_file,
    dex::TypeIndex type_index,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(&dex_file, type_index.index_, adrp_label, &type_bss_entry_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewBootImageStringPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(
      &dex_file, string_index.index_, adrp_label, &boot_image_string_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewStringBssEntryPatch(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    vixl::aarch64::Label* adrp_label) {
  return NewPcRelativePatch(&dex_file, string_index.index_, adrp_label, &string_bss_entry_patches_);
}

vixl::aarch64::Label* CodeGeneratorARM64::NewBakerReadBarrierPatch(uint32_t custom_data) {
  baker_read_barrier_patches_.emplace_back(custom_data);
  return &baker_read_barrier_patches_.back().label;
}

vixl::aarch64::Label* CodeGeneratorARM64::NewPcRelativePatch(
    const DexFile* dex_file,
    uint32_t offset_or_index,
    vixl::aarch64::Label* adrp_label,
    ArenaDeque<PcRelativePatchInfo>* patches) {
  // Add a patch entry and return the label.
  patches->emplace_back(dex_file, offset_or_index);
  PcRelativePatchInfo* info = &patches->back();
  vixl::aarch64::Label* label = &info->label;
  // If adrp_label is null, this is the ADRP patch and needs to point to its own label.
  info->pc_insn_label = (adrp_label != nullptr) ? adrp_label : label;
  return label;
}

vixl::aarch64::Literal<uint32_t>* CodeGeneratorARM64::DeduplicateBootImageAddressLiteral(
    uint64_t address) {
  return DeduplicateUint32Literal(dchecked_integral_cast<uint32_t>(address));
}

vixl::aarch64::Literal<uint32_t>* CodeGeneratorARM64::DeduplicateJitStringLiteral(
    const DexFile& dex_file, dex::StringIndex string_index, Handle<mirror::String> handle) {
  ReserveJitStringRoot(StringReference(&dex_file, string_index), handle);
  return jit_string_patches_.GetOrCreate(
      StringReference(&dex_file, string_index),
      [this]() { return __ CreateLiteralDestroyedWithPool<uint32_t>(/* placeholder */ 0u); });
}

vixl::aarch64::Literal<uint32_t>* CodeGeneratorARM64::DeduplicateJitClassLiteral(
    const DexFile& dex_file, dex::TypeIndex type_index, Handle<mirror::Class> handle) {
  ReserveJitClassRoot(TypeReference(&dex_file, type_index), handle);
  return jit_class_patches_.GetOrCreate(
      TypeReference(&dex_file, type_index),
      [this]() { return __ CreateLiteralDestroyedWithPool<uint32_t>(/* placeholder */ 0u); });
}

void CodeGeneratorARM64::EmitAdrpPlaceholder(vixl::aarch64::Label* fixup_label,
                                             vixl::aarch64::Register reg) {
  DCHECK(reg.IsX());
  SingleEmissionCheckScope guard(GetVIXLAssembler());
  __ Bind(fixup_label);
  __ adrp(reg, /* offset placeholder */ static_cast<int64_t>(0));
}

void CodeGeneratorARM64::EmitAddPlaceholder(vixl::aarch64::Label* fixup_label,
                                            vixl::aarch64::Register out,
                                            vixl::aarch64::Register base) {
  DCHECK(out.IsX());
  DCHECK(base.IsX());
  SingleEmissionCheckScope guard(GetVIXLAssembler());
  __ Bind(fixup_label);
  __ add(out, base, Operand(/* offset placeholder */ 0));
}

void CodeGeneratorARM64::EmitLdrOffsetPlaceholder(vixl::aarch64::Label* fixup_label,
                                                  vixl::aarch64::Register out,
                                                  vixl::aarch64::Register base) {
  DCHECK(base.IsX());
  SingleEmissionCheckScope guard(GetVIXLAssembler());
  __ Bind(fixup_label);
  __ ldr(out, MemOperand(base, /* offset placeholder */ 0));
}

template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
inline void CodeGeneratorARM64::EmitPcRelativeLinkerPatches(
    const ArenaDeque<PcRelativePatchInfo>& infos,
    ArenaVector<linker::LinkerPatch>* linker_patches) {
  for (const PcRelativePatchInfo& info : infos) {
    linker_patches->push_back(Factory(info.label.GetLocation(),
                                      info.target_dex_file,
                                      info.pc_insn_label->GetLocation(),
                                      info.offset_or_index));
  }
}

void CodeGeneratorARM64::EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) {
  DCHECK(linker_patches->empty());
  size_t size =
      boot_image_method_patches_.size() +
      method_bss_entry_patches_.size() +
      boot_image_type_patches_.size() +
      type_bss_entry_patches_.size() +
      boot_image_string_patches_.size() +
      string_bss_entry_patches_.size() +
      baker_read_barrier_patches_.size();
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
  for (const BakerReadBarrierPatchInfo& info : baker_read_barrier_patches_) {
    linker_patches->push_back(linker::LinkerPatch::BakerReadBarrierBranchPatch(
        info.label.GetLocation(), info.custom_data));
  }
  DCHECK_EQ(size, linker_patches->size());
}

vixl::aarch64::Literal<uint32_t>* CodeGeneratorARM64::DeduplicateUint32Literal(uint32_t value) {
  return uint32_literals_.GetOrCreate(
      value,
      [this, value]() { return __ CreateLiteralDestroyedWithPool<uint32_t>(value); });
}

vixl::aarch64::Literal<uint64_t>* CodeGeneratorARM64::DeduplicateUint64Literal(uint64_t value) {
  return uint64_literals_.GetOrCreate(
      value,
      [this, value]() { return __ CreateLiteralDestroyedWithPool<uint64_t>(value); });
}

void InstructionCodeGeneratorARM64::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
    return;
  }

  {
    // Ensure that between the BLR (emitted by GenerateStaticOrDirectCall) and RecordPcInfo there
    // are no pools emitted.
    EmissionCheckScope guard(GetVIXLAssembler(), kInvokeCodeMarginSizeInBytes);
    LocationSummary* locations = invoke->GetLocations();
    codegen_->GenerateStaticOrDirectCall(
        invoke, locations->HasTemps() ? locations->GetTemp(0) : Location::NoLocation());
  }

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void InstructionCodeGeneratorARM64::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
    return;
  }

  {
    // Ensure that between the BLR (emitted by GenerateVirtualCall) and RecordPcInfo there
    // are no pools emitted.
    EmissionCheckScope guard(GetVIXLAssembler(), kInvokeCodeMarginSizeInBytes);
    codegen_->GenerateVirtualCall(invoke, invoke->GetLocations()->GetTemp(0));
    DCHECK(!codegen_->IsLeafMethod());
  }

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

HLoadClass::LoadKind CodeGeneratorARM64::GetSupportedLoadClassKind(
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

void LocationsBuilderARM64::VisitLoadClass(HLoadClass* cls) {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConvention calling_convention;
    CodeGenerator::CreateLoadClassRuntimeCallLocationSummary(
        cls,
        LocationFrom(calling_convention.GetRegisterAt(0)),
        LocationFrom(vixl::aarch64::x0));
    DCHECK(calling_convention.GetRegisterAt(0).Is(vixl::aarch64::x0));
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
  if (cls->GetLoadKind() == HLoadClass::LoadKind::kBssEntry) {
    if (!kUseReadBarrier || kUseBakerReadBarrier) {
      // Rely on the type resolution or initialization and marking to save everything we need.
      RegisterSet caller_saves = RegisterSet::Empty();
      InvokeRuntimeCallingConvention calling_convention;
      caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0).GetCode()));
      DCHECK_EQ(calling_convention.GetRegisterAt(0).GetCode(),
                RegisterFrom(calling_convention.GetReturnLocation(DataType::Type::kReference),
                             DataType::Type::kReference).GetCode());
      locations->SetCustomSlowPathCallerSaves(caller_saves);
    } else {
      // For non-Baker read barrier we have a temp-clobbering call.
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorARM64::VisitLoadClass(HLoadClass* cls) NO_THREAD_SAFETY_ANALYSIS {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    codegen_->GenerateLoadClassRuntimeCall(cls);
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());

  Location out_loc = cls->GetLocations()->Out();
  Register out = OutputRegister(cls);

  const ReadBarrierOption read_barrier_option = cls->IsInBootImage()
      ? kWithoutReadBarrier
      : kCompilerReadBarrierOption;
  bool generate_null_check = false;
  switch (load_kind) {
    case HLoadClass::LoadKind::kReferrersClass: {
      DCHECK(!cls->CanCallRuntime());
      DCHECK(!cls->MustGenerateClinitCheck());
      // /* GcRoot<mirror::Class> */ out = current_method->declaring_class_
      Register current_method = InputRegisterAt(cls, 0);
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              current_method,
                              ArtMethod::DeclaringClassOffset().Int32Value(),
                              /* fixup_label */ nullptr,
                              read_barrier_option);
      break;
    }
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      // Add ADRP with its PC-relative type patch.
      const DexFile& dex_file = cls->GetDexFile();
      dex::TypeIndex type_index = cls->GetTypeIndex();
      vixl::aarch64::Label* adrp_label = codegen_->NewBootImageTypePatch(dex_file, type_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, out.X());
      // Add ADD with its PC-relative type patch.
      vixl::aarch64::Label* add_label =
          codegen_->NewBootImageTypePatch(dex_file, type_index, adrp_label);
      codegen_->EmitAddPlaceholder(add_label, out.X(), out.X());
      break;
    }
    case HLoadClass::LoadKind::kBootImageAddress: {
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(cls->GetClass().Get()));
      DCHECK_NE(address, 0u);
      __ Ldr(out.W(), codegen_->DeduplicateBootImageAddressLiteral(address));
      break;
    }
    case HLoadClass::LoadKind::kBootImageClassTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      // Add ADRP with its PC-relative type patch.
      const DexFile& dex_file = cls->GetDexFile();
      dex::TypeIndex type_index = cls->GetTypeIndex();
      vixl::aarch64::Label* adrp_label = codegen_->NewBootImageTypePatch(dex_file, type_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, out.X());
      // Add LDR with its PC-relative type patch.
      vixl::aarch64::Label* ldr_label =
          codegen_->NewBootImageTypePatch(dex_file, type_index, adrp_label);
      codegen_->EmitLdrOffsetPlaceholder(ldr_label, out.W(), out.X());
      // Extract the reference from the slot data, i.e. clear the hash bits.
      int32_t masked_hash = ClassTable::TableSlot::MaskHash(
          ComputeModifiedUtf8Hash(dex_file.StringByTypeIdx(type_index)));
      if (masked_hash != 0) {
        __ Sub(out.W(), out.W(), Operand(masked_hash));
      }
      break;
    }
    case HLoadClass::LoadKind::kBssEntry: {
      // Add ADRP with its PC-relative Class .bss entry patch.
      const DexFile& dex_file = cls->GetDexFile();
      dex::TypeIndex type_index = cls->GetTypeIndex();
      vixl::aarch64::Register temp = XRegisterFrom(out_loc);
      vixl::aarch64::Label* adrp_label = codegen_->NewBssEntryTypePatch(dex_file, type_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, temp);
      // Add LDR with its PC-relative Class patch.
      vixl::aarch64::Label* ldr_label =
          codegen_->NewBssEntryTypePatch(dex_file, type_index, adrp_label);
      // /* GcRoot<mirror::Class> */ out = *(base_address + offset)  /* PC-relative */
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              temp,
                              /* offset placeholder */ 0u,
                              ldr_label,
                              read_barrier_option);
      generate_null_check = true;
      break;
    }
    case HLoadClass::LoadKind::kJitTableAddress: {
      __ Ldr(out, codegen_->DeduplicateJitClassLiteral(cls->GetDexFile(),
                                                       cls->GetTypeIndex(),
                                                       cls->GetClass()));
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              out.X(),
                              /* offset */ 0,
                              /* fixup_label */ nullptr,
                              read_barrier_option);
      break;
    }
    case HLoadClass::LoadKind::kRuntimeCall:
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }

  bool do_clinit = cls->MustGenerateClinitCheck();
  if (generate_null_check || do_clinit) {
    DCHECK(cls->CanCallRuntime());
    SlowPathCodeARM64* slow_path = new (codegen_->GetScopedAllocator()) LoadClassSlowPathARM64(
        cls, cls, cls->GetDexPc(), do_clinit);
    codegen_->AddSlowPath(slow_path);
    if (generate_null_check) {
      __ Cbz(out, slow_path->GetEntryLabel());
    }
    if (cls->MustGenerateClinitCheck()) {
      GenerateClassInitializationCheck(slow_path, out);
    } else {
      __ Bind(slow_path->GetExitLabel());
    }
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
  }
}

static MemOperand GetExceptionTlsAddress() {
  return MemOperand(tr, Thread::ExceptionOffset<kArm64PointerSize>().Int32Value());
}

void LocationsBuilderARM64::VisitLoadException(HLoadException* load) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(load, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARM64::VisitLoadException(HLoadException* instruction) {
  __ Ldr(OutputRegister(instruction), GetExceptionTlsAddress());
}

void LocationsBuilderARM64::VisitClearException(HClearException* clear) {
  new (GetGraph()->GetAllocator()) LocationSummary(clear, LocationSummary::kNoCall);
}

void InstructionCodeGeneratorARM64::VisitClearException(HClearException* clear ATTRIBUTE_UNUSED) {
  __ Str(wzr, GetExceptionTlsAddress());
}

HLoadString::LoadKind CodeGeneratorARM64::GetSupportedLoadStringKind(
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

void LocationsBuilderARM64::VisitLoadString(HLoadString* load) {
  LocationSummary::CallKind call_kind = CodeGenerator::GetLoadStringCallKind(load);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(load, call_kind);
  if (load->GetLoadKind() == HLoadString::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConvention calling_convention;
    locations->SetOut(calling_convention.GetReturnLocation(load->GetType()));
  } else {
    locations->SetOut(Location::RequiresRegister());
    if (load->GetLoadKind() == HLoadString::LoadKind::kBssEntry) {
      if (!kUseReadBarrier || kUseBakerReadBarrier) {
        // Rely on the pResolveString and marking to save everything we need.
        RegisterSet caller_saves = RegisterSet::Empty();
        InvokeRuntimeCallingConvention calling_convention;
        caller_saves.Add(Location::RegisterLocation(calling_convention.GetRegisterAt(0).GetCode()));
        DCHECK_EQ(calling_convention.GetRegisterAt(0).GetCode(),
                  RegisterFrom(calling_convention.GetReturnLocation(DataType::Type::kReference),
                               DataType::Type::kReference).GetCode());
        locations->SetCustomSlowPathCallerSaves(caller_saves);
      } else {
        // For non-Baker read barrier we have a temp-clobbering call.
      }
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorARM64::VisitLoadString(HLoadString* load) NO_THREAD_SAFETY_ANALYSIS {
  Register out = OutputRegister(load);
  Location out_loc = load->GetLocations()->Out();

  switch (load->GetLoadKind()) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      // Add ADRP with its PC-relative String patch.
      const DexFile& dex_file = load->GetDexFile();
      const dex::StringIndex string_index = load->GetStringIndex();
      vixl::aarch64::Label* adrp_label = codegen_->NewBootImageStringPatch(dex_file, string_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, out.X());
      // Add ADD with its PC-relative String patch.
      vixl::aarch64::Label* add_label =
          codegen_->NewBootImageStringPatch(dex_file, string_index, adrp_label);
      codegen_->EmitAddPlaceholder(add_label, out.X(), out.X());
      return;
    }
    case HLoadString::LoadKind::kBootImageAddress: {
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(load->GetString().Get()));
      DCHECK_NE(address, 0u);
      __ Ldr(out.W(), codegen_->DeduplicateBootImageAddressLiteral(address));
      return;
    }
    case HLoadString::LoadKind::kBootImageInternTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      // Add ADRP with its PC-relative String patch.
      const DexFile& dex_file = load->GetDexFile();
      const dex::StringIndex string_index = load->GetStringIndex();
      vixl::aarch64::Label* adrp_label = codegen_->NewBootImageStringPatch(dex_file, string_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, out.X());
      // Add LDR with its PC-relative String patch.
      vixl::aarch64::Label* ldr_label =
          codegen_->NewBootImageStringPatch(dex_file, string_index, adrp_label);
      codegen_->EmitLdrOffsetPlaceholder(ldr_label, out.W(), out.X());
      return;
    }
    case HLoadString::LoadKind::kBssEntry: {
      // Add ADRP with its PC-relative String .bss entry patch.
      const DexFile& dex_file = load->GetDexFile();
      const dex::StringIndex string_index = load->GetStringIndex();
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      Register temp = XRegisterFrom(out_loc);
      vixl::aarch64::Label* adrp_label = codegen_->NewStringBssEntryPatch(dex_file, string_index);
      codegen_->EmitAdrpPlaceholder(adrp_label, temp);
      // Add LDR with its .bss entry String patch.
      vixl::aarch64::Label* ldr_label =
          codegen_->NewStringBssEntryPatch(dex_file, string_index, adrp_label);
      // /* GcRoot<mirror::String> */ out = *(base_address + offset)  /* PC-relative */
      GenerateGcRootFieldLoad(load,
                              out_loc,
                              temp,
                              /* offset placeholder */ 0u,
                              ldr_label,
                              kCompilerReadBarrierOption);
      SlowPathCodeARM64* slow_path =
          new (codegen_->GetScopedAllocator()) LoadStringSlowPathARM64(load);
      codegen_->AddSlowPath(slow_path);
      __ Cbz(out.X(), slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
      return;
    }
    case HLoadString::LoadKind::kJitTableAddress: {
      __ Ldr(out, codegen_->DeduplicateJitStringLiteral(load->GetDexFile(),
                                                        load->GetStringIndex(),
                                                        load->GetString()));
      GenerateGcRootFieldLoad(load,
                              out_loc,
                              out.X(),
                              /* offset */ 0,
                              /* fixup_label */ nullptr,
                              kCompilerReadBarrierOption);
      return;
    }
    default:
      break;
  }

  // TODO: Re-add the compiler code to do string dex cache lookup again.
  InvokeRuntimeCallingConvention calling_convention;
  DCHECK_EQ(calling_convention.GetRegisterAt(0).GetCode(), out.GetCode());
  __ Mov(calling_convention.GetRegisterAt(0).W(), load->GetStringIndex().index_);
  codegen_->InvokeRuntime(kQuickResolveString, load, load->GetDexPc());
  CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitLongConstant(HLongConstant* constant) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(constant);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARM64::VisitLongConstant(HLongConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARM64::VisitMonitorOperation(HMonitorOperation* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorARM64::VisitMonitorOperation(HMonitorOperation* instruction) {
  codegen_->InvokeRuntime(instruction->IsEnter() ? kQuickLockObject : kQuickUnlockObject,
                          instruction,
                          instruction->GetDexPc());
  if (instruction->IsEnter()) {
    CheckEntrypointTypes<kQuickLockObject, void, mirror::Object*>();
  } else {
    CheckEntrypointTypes<kQuickUnlockObject, void, mirror::Object*>();
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitMul(HMul* mul) {
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

void InstructionCodeGeneratorARM64::VisitMul(HMul* mul) {
  switch (mul->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      __ Mul(OutputRegister(mul), InputRegisterAt(mul, 0), InputRegisterAt(mul, 1));
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Fmul(OutputFPRegister(mul), InputFPRegisterAt(mul, 0), InputFPRegisterAt(mul, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected mul type " << mul->GetResultType();
  }
}

void LocationsBuilderARM64::VisitNeg(HNeg* neg) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(neg, LocationSummary::kNoCall);
  switch (neg->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, ARM64EncodableConstantOrRegister(neg->InputAt(0), neg));
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

void InstructionCodeGeneratorARM64::VisitNeg(HNeg* neg) {
  switch (neg->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      __ Neg(OutputRegister(neg), InputOperandAt(neg, 0));
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Fneg(OutputFPRegister(neg), InputFPRegisterAt(neg, 0));
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << neg->GetResultType();
  }
}

void LocationsBuilderARM64::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetOut(LocationFrom(x0));
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
}

void InstructionCodeGeneratorARM64::VisitNewArray(HNewArray* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes cares
  // of poisoning the reference.
  QuickEntrypointEnum entrypoint =
      CodeGenerator::GetArrayAllocationEntrypoint(instruction->GetLoadClass()->GetClass());
  codegen_->InvokeRuntime(entrypoint, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickAllocArrayResolved, void*, mirror::Class*, int32_t>();
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitNewInstance(HNewInstance* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  if (instruction->IsStringAlloc()) {
    locations->AddTemp(LocationFrom(kArtMethodRegister));
  } else {
    locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  }
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
}

void InstructionCodeGeneratorARM64::VisitNewInstance(HNewInstance* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes cares
  // of poisoning the reference.
  if (instruction->IsStringAlloc()) {
    // String is allocated through StringFactory. Call NewEmptyString entry point.
    Location temp = instruction->GetLocations()->GetTemp(0);
    MemberOffset code_offset = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArm64PointerSize);
    __ Ldr(XRegisterFrom(temp), MemOperand(tr, QUICK_ENTRY_POINT(pNewEmptyString)));
    __ Ldr(lr, MemOperand(XRegisterFrom(temp), code_offset.Int32Value()));

    {
      // Ensure the pc position is recorded immediately after the `blr` instruction.
      ExactAssemblyScope eas(GetVIXLAssembler(),
                             kInstructionSize,
                             CodeBufferCheckScope::kExactSize);
      __ blr(lr);
      codegen_->RecordPcInfo(instruction, instruction->GetDexPc());
    }
  } else {
    codegen_->InvokeRuntime(instruction->GetEntrypoint(), instruction, instruction->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitNot(HNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitNot(HNot* instruction) {
  switch (instruction->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      __ Mvn(OutputRegister(instruction), InputOperandAt(instruction, 0));
      break;

    default:
      LOG(FATAL) << "Unexpected type for not operation " << instruction->GetResultType();
  }
}

void LocationsBuilderARM64::VisitBooleanNot(HBooleanNot* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARM64::VisitBooleanNot(HBooleanNot* instruction) {
  __ Eor(OutputRegister(instruction), InputRegisterAt(instruction, 0), vixl::aarch64::Operand(1));
}

void LocationsBuilderARM64::VisitNullCheck(HNullCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
}

void CodeGeneratorARM64::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (CanMoveNullCheckToUser(instruction)) {
    return;
  }
  {
    // Ensure that between load and MaybeRecordImplicitNullCheck there are no pools emitted.
    EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
    Location obj = instruction->GetLocations()->InAt(0);
    __ Ldr(wzr, HeapOperandFrom(obj, Offset(0)));
    RecordPcInfo(instruction, instruction->GetDexPc());
  }
}

void CodeGeneratorARM64::GenerateExplicitNullCheck(HNullCheck* instruction) {
  SlowPathCodeARM64* slow_path = new (GetScopedAllocator()) NullCheckSlowPathARM64(instruction);
  AddSlowPath(slow_path);

  LocationSummary* locations = instruction->GetLocations();
  Location obj = locations->InAt(0);

  __ Cbz(RegisterFrom(obj, instruction->InputAt(0)->GetType()), slow_path->GetEntryLabel());
}

void InstructionCodeGeneratorARM64::VisitNullCheck(HNullCheck* instruction) {
  codegen_->GenerateNullCheck(instruction);
}

void LocationsBuilderARM64::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorARM64::VisitOr(HOr* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderARM64::VisitParallelMove(HParallelMove* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorARM64::VisitParallelMove(HParallelMove* instruction) {
  if (instruction->GetNext()->IsSuspendCheck() &&
      instruction->GetBlock()->GetLoopInformation() != nullptr) {
    HSuspendCheck* suspend_check = instruction->GetNext()->AsSuspendCheck();
    // The back edge will generate the suspend check.
    codegen_->ClearSpillSlotsFromLoopPhisInStackMap(suspend_check, instruction);
  }

  codegen_->GetMoveResolver()->EmitNativeCode(instruction);
}

void LocationsBuilderARM64::VisitParameterValue(HParameterValue* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  Location location = parameter_visitor_.GetNextLocation(instruction->GetType());
  if (location.IsStackSlot()) {
    location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  } else if (location.IsDoubleStackSlot()) {
    location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  }
  locations->SetOut(location);
}

void InstructionCodeGeneratorARM64::VisitParameterValue(
    HParameterValue* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, the parameter is already at its location.
}

void LocationsBuilderARM64::VisitCurrentMethod(HCurrentMethod* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(LocationFrom(kArtMethodRegister));
}

void InstructionCodeGeneratorARM64::VisitCurrentMethod(
    HCurrentMethod* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, the method is already at its location.
}

void LocationsBuilderARM64::VisitPhi(HPhi* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    locations->SetInAt(i, Location::Any());
  }
  locations->SetOut(Location::Any());
}

void InstructionCodeGeneratorARM64::VisitPhi(HPhi* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void LocationsBuilderARM64::VisitRem(HRem* rem) {
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
      locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0)));
      locations->SetInAt(1, LocationFrom(calling_convention.GetFpuRegisterAt(1)));
      locations->SetOut(calling_convention.GetReturnLocation(type));

      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void InstructionCodeGeneratorARM64::VisitRem(HRem* rem) {
  DataType::Type type = rem->GetResultType();

  switch (type) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      GenerateDivRemIntegral(rem);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      QuickEntrypointEnum entrypoint =
          (type == DataType::Type::kFloat32) ? kQuickFmodf : kQuickFmod;
      codegen_->InvokeRuntime(entrypoint, rem, rem->GetDexPc());
      if (type == DataType::Type::kFloat32) {
        CheckEntrypointTypes<kQuickFmodf, float, float, float>();
      } else {
        CheckEntrypointTypes<kQuickFmod, double, double, double>();
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitConstructorFence(HConstructorFence* constructor_fence) {
  constructor_fence->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitConstructorFence(
    HConstructorFence* constructor_fence ATTRIBUTE_UNUSED) {
  codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
}

void LocationsBuilderARM64::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  memory_barrier->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  codegen_->GenerateMemoryBarrier(memory_barrier->GetBarrierKind());
}

void LocationsBuilderARM64::VisitReturn(HReturn* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  DataType::Type return_type = instruction->InputAt(0)->GetType();
  locations->SetInAt(0, ARM64ReturnLocation(return_type));
}

void InstructionCodeGeneratorARM64::VisitReturn(HReturn* instruction ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderARM64::VisitReturnVoid(HReturnVoid* instruction) {
  instruction->SetLocations(nullptr);
}

void InstructionCodeGeneratorARM64::VisitReturnVoid(HReturnVoid* instruction ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderARM64::VisitRor(HRor* ror) {
  HandleBinaryOp(ror);
}

void InstructionCodeGeneratorARM64::VisitRor(HRor* ror) {
  HandleBinaryOp(ror);
}

void LocationsBuilderARM64::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void InstructionCodeGeneratorARM64::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void LocationsBuilderARM64::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void InstructionCodeGeneratorARM64::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void LocationsBuilderARM64::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorARM64::VisitSub(HSub* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderARM64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARM64::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderARM64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction);
}

void InstructionCodeGeneratorARM64::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LocationsBuilderARM64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARM64::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARM64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARM64::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARM64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARM64::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARM64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARM64::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionARM64 calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARM64::VisitSuspendCheck(HSuspendCheck* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnSlowPath);
  // In suspend check slow path, usually there are no caller-save registers at all.
  // If SIMD instructions are present, however, we force spilling all live SIMD
  // registers in full width (since the runtime only saves/restores lower part).
  locations->SetCustomSlowPathCallerSaves(
      GetGraph()->HasSIMD() ? RegisterSet::AllFpu() : RegisterSet::Empty());
}

void InstructionCodeGeneratorARM64::VisitSuspendCheck(HSuspendCheck* instruction) {
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
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void LocationsBuilderARM64::VisitThrow(HThrow* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorARM64::VisitThrow(HThrow* instruction) {
  codegen_->InvokeRuntime(kQuickDeliverException, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
}

void LocationsBuilderARM64::VisitTypeConversion(HTypeConversion* conversion) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(conversion, LocationSummary::kNoCall);
  DataType::Type input_type = conversion->GetInputType();
  DataType::Type result_type = conversion->GetResultType();
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;
  if ((input_type == DataType::Type::kReference) || (input_type == DataType::Type::kVoid) ||
      (result_type == DataType::Type::kReference) || (result_type == DataType::Type::kVoid)) {
    LOG(FATAL) << "Unexpected type conversion from " << input_type << " to " << result_type;
  }

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

void InstructionCodeGeneratorARM64::VisitTypeConversion(HTypeConversion* conversion) {
  DataType::Type result_type = conversion->GetResultType();
  DataType::Type input_type = conversion->GetInputType();

  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;

  if (DataType::IsIntegralType(result_type) && DataType::IsIntegralType(input_type)) {
    int result_size = DataType::Size(result_type);
    int input_size = DataType::Size(input_type);
    int min_size = std::min(result_size, input_size);
    Register output = OutputRegister(conversion);
    Register source = InputRegisterAt(conversion, 0);
    if (result_type == DataType::Type::kInt32 && input_type == DataType::Type::kInt64) {
      // 'int' values are used directly as W registers, discarding the top
      // bits, so we don't need to sign-extend and can just perform a move.
      // We do not pass the `kDiscardForSameWReg` argument to force clearing the
      // top 32 bits of the target register. We theoretically could leave those
      // bits unchanged, but we would have to make sure that no code uses a
      // 32bit input value as a 64bit value assuming that the top 32 bits are
      // zero.
      __ Mov(output.W(), source.W());
    } else if (DataType::IsUnsignedType(result_type) ||
               (DataType::IsUnsignedType(input_type) && input_size < result_size)) {
      __ Ubfx(output, output.IsX() ? source.X() : source.W(), 0, result_size * kBitsPerByte);
    } else {
      __ Sbfx(output, output.IsX() ? source.X() : source.W(), 0, min_size * kBitsPerByte);
    }
  } else if (DataType::IsFloatingPointType(result_type) && DataType::IsIntegralType(input_type)) {
    __ Scvtf(OutputFPRegister(conversion), InputRegisterAt(conversion, 0));
  } else if (DataType::IsIntegralType(result_type) && DataType::IsFloatingPointType(input_type)) {
    CHECK(result_type == DataType::Type::kInt32 || result_type == DataType::Type::kInt64);
    __ Fcvtzs(OutputRegister(conversion), InputFPRegisterAt(conversion, 0));
  } else if (DataType::IsFloatingPointType(result_type) &&
             DataType::IsFloatingPointType(input_type)) {
    __ Fcvt(OutputFPRegister(conversion), InputFPRegisterAt(conversion, 0));
  } else {
    LOG(FATAL) << "Unexpected or unimplemented type conversion from " << input_type
                << " to " << result_type;
  }
}

void LocationsBuilderARM64::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void InstructionCodeGeneratorARM64::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void LocationsBuilderARM64::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void InstructionCodeGeneratorARM64::VisitXor(HXor* instruction) {
  HandleBinaryOp(instruction);
}

void LocationsBuilderARM64::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorARM64::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

// Simple implementation of packed switch - generate cascaded compare/jumps.
void LocationsBuilderARM64::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(switch_instr, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
}

void InstructionCodeGeneratorARM64::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  int32_t lower_bound = switch_instr->GetStartValue();
  uint32_t num_entries = switch_instr->GetNumEntries();
  Register value_reg = InputRegisterAt(switch_instr, 0);
  HBasicBlock* default_block = switch_instr->GetDefaultBlock();

  // Roughly set 16 as max average assemblies generated per HIR in a graph.
  static constexpr int32_t kMaxExpectedSizePerHInstruction = 16 * kInstructionSize;
  // ADR has a limited range(+/-1MB), so we set a threshold for the number of HIRs in the graph to
  // make sure we don't emit it if the target may run out of range.
  // TODO: Instead of emitting all jump tables at the end of the code, we could keep track of ADR
  // ranges and emit the tables only as required.
  static constexpr int32_t kJumpTableInstructionThreshold = 1* MB / kMaxExpectedSizePerHInstruction;

  if (num_entries <= kPackedSwitchCompareJumpThreshold ||
      // Current instruction id is an upper bound of the number of HIRs in the graph.
      GetGraph()->GetCurrentInstructionId() > kJumpTableInstructionThreshold) {
    // Create a series of compare/jumps.
    UseScratchRegisterScope temps(codegen_->GetVIXLAssembler());
    Register temp = temps.AcquireW();
    __ Subs(temp, value_reg, Operand(lower_bound));

    const ArenaVector<HBasicBlock*>& successors = switch_instr->GetBlock()->GetSuccessors();
    // Jump to successors[0] if value == lower_bound.
    __ B(eq, codegen_->GetLabelOf(successors[0]));
    int32_t last_index = 0;
    for (; num_entries - last_index > 2; last_index += 2) {
      __ Subs(temp, temp, Operand(2));
      // Jump to successors[last_index + 1] if value < case_value[last_index + 2].
      __ B(lo, codegen_->GetLabelOf(successors[last_index + 1]));
      // Jump to successors[last_index + 2] if value == case_value[last_index + 2].
      __ B(eq, codegen_->GetLabelOf(successors[last_index + 2]));
    }
    if (num_entries - last_index == 2) {
      // The last missing case_value.
      __ Cmp(temp, Operand(1));
      __ B(eq, codegen_->GetLabelOf(successors[last_index + 1]));
    }

    // And the default for any other value.
    if (!codegen_->GoesToNextBlock(switch_instr->GetBlock(), default_block)) {
      __ B(codegen_->GetLabelOf(default_block));
    }
  } else {
    JumpTableARM64* jump_table = codegen_->CreateJumpTable(switch_instr);

    UseScratchRegisterScope temps(codegen_->GetVIXLAssembler());

    // Below instructions should use at most one blocked register. Since there are two blocked
    // registers, we are free to block one.
    Register temp_w = temps.AcquireW();
    Register index;
    // Remove the bias.
    if (lower_bound != 0) {
      index = temp_w;
      __ Sub(index, value_reg, Operand(lower_bound));
    } else {
      index = value_reg;
    }

    // Jump to default block if index is out of the range.
    __ Cmp(index, Operand(num_entries));
    __ B(hs, codegen_->GetLabelOf(default_block));

    // In current VIXL implementation, it won't require any blocked registers to encode the
    // immediate value for Adr. So we are free to use both VIXL blocked registers to reduce the
    // register pressure.
    Register table_base = temps.AcquireX();
    // Load jump offset from the table.
    __ Adr(table_base, jump_table->GetTableStartLabel());
    Register jump_offset = temp_w;
    __ Ldr(jump_offset, MemOperand(table_base, index, UXTW, 2));

    // Jump to target block by branching to table_base(pc related) + offset.
    Register target_address = table_base;
    __ Add(target_address, table_base, Operand(jump_offset, SXTW));
    __ Br(target_address);
  }
}

void InstructionCodeGeneratorARM64::GenerateReferenceLoadOneRegister(
    HInstruction* instruction,
    Location out,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  DataType::Type type = DataType::Type::kReference;
  Register out_reg = RegisterFrom(out, type);
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(out + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                      out,
                                                      out_reg,
                                                      offset,
                                                      maybe_temp,
                                                      /* needs_null_check */ false,
                                                      /* use_load_acquire */ false);
    } else {
      // Load with slow path based read barrier.
      // Save the value of `out` into `maybe_temp` before overwriting it
      // in the following move operation, as we will need it for the
      // read barrier below.
      Register temp_reg = RegisterFrom(maybe_temp, type);
      __ Mov(temp_reg, out_reg);
      // /* HeapReference<Object> */ out = *(out + offset)
      __ Ldr(out_reg, HeapOperand(out_reg, offset));
      codegen_->GenerateReadBarrierSlow(instruction, out, out, maybe_temp, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(out + offset)
    __ Ldr(out_reg, HeapOperand(out_reg, offset));
    GetAssembler()->MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorARM64::GenerateReferenceLoadTwoRegisters(
    HInstruction* instruction,
    Location out,
    Location obj,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  DataType::Type type = DataType::Type::kReference;
  Register out_reg = RegisterFrom(out, type);
  Register obj_reg = RegisterFrom(obj, type);
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                      out,
                                                      obj_reg,
                                                      offset,
                                                      maybe_temp,
                                                      /* needs_null_check */ false,
                                                      /* use_load_acquire */ false);
    } else {
      // Load with slow path based read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      __ Ldr(out_reg, HeapOperand(obj_reg, offset));
      codegen_->GenerateReadBarrierSlow(instruction, out, out, obj, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(obj + offset)
    __ Ldr(out_reg, HeapOperand(obj_reg, offset));
    GetAssembler()->MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorARM64::GenerateGcRootFieldLoad(
    HInstruction* instruction,
    Location root,
    Register obj,
    uint32_t offset,
    vixl::aarch64::Label* fixup_label,
    ReadBarrierOption read_barrier_option) {
  DCHECK(fixup_label == nullptr || offset == 0u);
  Register root_reg = RegisterFrom(root, DataType::Type::kReference);
  if (read_barrier_option == kWithReadBarrier) {
    DCHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      // Fast path implementation of art::ReadBarrier::BarrierForRoot when
      // Baker's read barrier are used.
      if (kBakerReadBarrierLinkTimeThunksEnableForGcRoots &&
          !Runtime::Current()->UseJitCompilation()) {
        // Query `art::Thread::Current()->GetIsGcMarking()` (stored in
        // the Marking Register) to decide whether we need to enter
        // the slow path to mark the GC root.
        //
        // We use link-time generated thunks for the slow path. That thunk
        // checks the reference and jumps to the entrypoint if needed.
        //
        //     lr = &return_address;
        //     GcRoot<mirror::Object> root = *(obj+offset);  // Original reference load.
        //     if (mr) {  // Thread::Current()->GetIsGcMarking()
        //       goto gc_root_thunk<root_reg>(lr)
        //     }
        //   return_address:

        UseScratchRegisterScope temps(GetVIXLAssembler());
        DCHECK(temps.IsAvailable(ip0));
        DCHECK(temps.IsAvailable(ip1));
        temps.Exclude(ip0, ip1);
        uint32_t custom_data =
            linker::Arm64RelativePatcher::EncodeBakerReadBarrierGcRootData(root_reg.GetCode());
        vixl::aarch64::Label* cbnz_label = codegen_->NewBakerReadBarrierPatch(custom_data);

        EmissionCheckScope guard(GetVIXLAssembler(), 3 * vixl::aarch64::kInstructionSize);
        vixl::aarch64::Label return_address;
        __ adr(lr, &return_address);
        if (fixup_label != nullptr) {
          __ Bind(fixup_label);
        }
        static_assert(BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_OFFSET == -8,
                      "GC root LDR must be 2 instruction (8B) before the return address label.");
        __ ldr(root_reg, MemOperand(obj.X(), offset));
        __ Bind(cbnz_label);
        __ cbnz(mr, static_cast<int64_t>(0));  // Placeholder, patched at link-time.
        __ Bind(&return_address);
      } else {
        // Query `art::Thread::Current()->GetIsGcMarking()` (stored in
        // the Marking Register) to decide whether we need to enter
        // the slow path to mark the GC root.
        //
        //   GcRoot<mirror::Object> root = *(obj+offset);  // Original reference load.
        //   if (mr) {  // Thread::Current()->GetIsGcMarking()
        //     // Slow path.
        //     entrypoint = Thread::Current()->pReadBarrierMarkReg ## root.reg()
        //     root = entrypoint(root);  // root = ReadBarrier::Mark(root);  // Entry point call.
        //   }

        // Slow path marking the GC root `root`. The entrypoint will
        // be loaded by the slow path code.
        SlowPathCodeARM64* slow_path =
            new (codegen_->GetScopedAllocator()) ReadBarrierMarkSlowPathARM64(instruction, root);
        codegen_->AddSlowPath(slow_path);

        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        if (fixup_label == nullptr) {
          __ Ldr(root_reg, MemOperand(obj, offset));
        } else {
          codegen_->EmitLdrOffsetPlaceholder(fixup_label, root_reg, obj);
        }
        static_assert(
            sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(GcRoot<mirror::Object>),
            "art::mirror::CompressedReference<mirror::Object> and art::GcRoot<mirror::Object> "
            "have different sizes.");
        static_assert(sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(int32_t),
                      "art::mirror::CompressedReference<mirror::Object> and int32_t "
                      "have different sizes.");

        __ Cbnz(mr, slow_path->GetEntryLabel());
        __ Bind(slow_path->GetExitLabel());
      }
    } else {
      // GC root loaded through a slow path for read barriers other
      // than Baker's.
      // /* GcRoot<mirror::Object>* */ root = obj + offset
      if (fixup_label == nullptr) {
        __ Add(root_reg.X(), obj.X(), offset);
      } else {
        codegen_->EmitAddPlaceholder(fixup_label, root_reg.X(), obj.X());
      }
      // /* mirror::Object* */ root = root->Read()
      codegen_->GenerateReadBarrierForRootSlow(instruction, root, root);
    }
  } else {
    // Plain GC root load with no read barrier.
    // /* GcRoot<mirror::Object> */ root = *(obj + offset)
    if (fixup_label == nullptr) {
      __ Ldr(root_reg, MemOperand(obj, offset));
    } else {
      codegen_->EmitLdrOffsetPlaceholder(fixup_label, root_reg, obj.X());
    }
    // Note that GC roots are not affected by heap poisoning, thus we
    // do not have to unpoison `root_reg` here.
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void CodeGeneratorARM64::GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                                               Location ref,
                                                               Register obj,
                                                               uint32_t offset,
                                                               Location maybe_temp,
                                                               bool needs_null_check,
                                                               bool use_load_acquire) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  if (kBakerReadBarrierLinkTimeThunksEnableForFields &&
      !use_load_acquire &&
      !Runtime::Current()->UseJitCompilation()) {
    // Query `art::Thread::Current()->GetIsGcMarking()` (stored in the
    // Marking Register) to decide whether we need to enter the slow
    // path to mark the reference. Then, in the slow path, check the
    // gray bit in the lock word of the reference's holder (`obj`) to
    // decide whether to mark `ref` or not.
    //
    // We use link-time generated thunks for the slow path. That thunk checks
    // the holder and jumps to the entrypoint if needed. If the holder is not
    // gray, it creates a fake dependency and returns to the LDR instruction.
    //
    //     lr = &gray_return_address;
    //     if (mr) {  // Thread::Current()->GetIsGcMarking()
    //       goto field_thunk<holder_reg, base_reg>(lr)
    //     }
    //   not_gray_return_address:
    //     // Original reference load. If the offset is too large to fit
    //     // into LDR, we use an adjusted base register here.
    //     HeapReference<mirror::Object> reference = *(obj+offset);
    //   gray_return_address:

    DCHECK_ALIGNED(offset, sizeof(mirror::HeapReference<mirror::Object>));
    Register base = obj;
    if (offset >= kReferenceLoadMinFarOffset) {
      DCHECK(maybe_temp.IsRegister());
      base = WRegisterFrom(maybe_temp);
      static_assert(IsPowerOfTwo(kReferenceLoadMinFarOffset), "Expecting a power of 2.");
      __ Add(base, obj, Operand(offset & ~(kReferenceLoadMinFarOffset - 1u)));
      offset &= (kReferenceLoadMinFarOffset - 1u);
    }
    UseScratchRegisterScope temps(GetVIXLAssembler());
    DCHECK(temps.IsAvailable(ip0));
    DCHECK(temps.IsAvailable(ip1));
    temps.Exclude(ip0, ip1);
    uint32_t custom_data = linker::Arm64RelativePatcher::EncodeBakerReadBarrierFieldData(
        base.GetCode(),
        obj.GetCode());
    vixl::aarch64::Label* cbnz_label = NewBakerReadBarrierPatch(custom_data);

    {
      EmissionCheckScope guard(GetVIXLAssembler(),
                               (kPoisonHeapReferences ? 4u : 3u) * vixl::aarch64::kInstructionSize);
      vixl::aarch64::Label return_address;
      __ adr(lr, &return_address);
      __ Bind(cbnz_label);
      __ cbnz(mr, static_cast<int64_t>(0));  // Placeholder, patched at link-time.
      static_assert(BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET == (kPoisonHeapReferences ? -8 : -4),
                    "Field LDR must be 1 instruction (4B) before the return address label; "
                    " 2 instructions (8B) for heap poisoning.");
      Register ref_reg = RegisterFrom(ref, DataType::Type::kReference);
      __ ldr(ref_reg, MemOperand(base.X(), offset));
      if (needs_null_check) {
        MaybeRecordImplicitNullCheck(instruction);
      }
      GetAssembler()->MaybeUnpoisonHeapReference(ref_reg);
      __ Bind(&return_address);
    }
    MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__, /* temp_loc */ LocationFrom(ip1));
    return;
  }

  // /* HeapReference<Object> */ ref = *(obj + offset)
  Register temp = WRegisterFrom(maybe_temp);
  Location no_index = Location::NoLocation();
  size_t no_scale_factor = 0u;
  GenerateReferenceLoadWithBakerReadBarrier(instruction,
                                            ref,
                                            obj,
                                            offset,
                                            no_index,
                                            no_scale_factor,
                                            temp,
                                            needs_null_check,
                                            use_load_acquire);
}

void CodeGeneratorARM64::GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                                               Location ref,
                                                               Register obj,
                                                               uint32_t data_offset,
                                                               Location index,
                                                               Register temp,
                                                               bool needs_null_check) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  static_assert(
      sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
      "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
  size_t scale_factor = DataType::SizeShift(DataType::Type::kReference);

  if (kBakerReadBarrierLinkTimeThunksEnableForArrays &&
      !Runtime::Current()->UseJitCompilation()) {
    // Query `art::Thread::Current()->GetIsGcMarking()` (stored in the
    // Marking Register) to decide whether we need to enter the slow
    // path to mark the reference. Then, in the slow path, check the
    // gray bit in the lock word of the reference's holder (`obj`) to
    // decide whether to mark `ref` or not.
    //
    // We use link-time generated thunks for the slow path. That thunk checks
    // the holder and jumps to the entrypoint if needed. If the holder is not
    // gray, it creates a fake dependency and returns to the LDR instruction.
    //
    //     lr = &gray_return_address;
    //     if (mr) {  // Thread::Current()->GetIsGcMarking()
    //       goto array_thunk<base_reg>(lr)
    //     }
    //   not_gray_return_address:
    //     // Original reference load. If the offset is too large to fit
    //     // into LDR, we use an adjusted base register here.
    //     HeapReference<mirror::Object> reference = data[index];
    //   gray_return_address:

    DCHECK(index.IsValid());
    Register index_reg = RegisterFrom(index, DataType::Type::kInt32);
    Register ref_reg = RegisterFrom(ref, DataType::Type::kReference);

    UseScratchRegisterScope temps(GetVIXLAssembler());
    DCHECK(temps.IsAvailable(ip0));
    DCHECK(temps.IsAvailable(ip1));
    temps.Exclude(ip0, ip1);
    uint32_t custom_data =
        linker::Arm64RelativePatcher::EncodeBakerReadBarrierArrayData(temp.GetCode());
    vixl::aarch64::Label* cbnz_label = NewBakerReadBarrierPatch(custom_data);

    __ Add(temp.X(), obj.X(), Operand(data_offset));
    {
      EmissionCheckScope guard(GetVIXLAssembler(),
                               (kPoisonHeapReferences ? 4u : 3u) * vixl::aarch64::kInstructionSize);
      vixl::aarch64::Label return_address;
      __ adr(lr, &return_address);
      __ Bind(cbnz_label);
      __ cbnz(mr, static_cast<int64_t>(0));  // Placeholder, patched at link-time.
      static_assert(BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET == (kPoisonHeapReferences ? -8 : -4),
                    "Array LDR must be 1 instruction (4B) before the return address label; "
                    " 2 instructions (8B) for heap poisoning.");
      __ ldr(ref_reg, MemOperand(temp.X(), index_reg.X(), LSL, scale_factor));
      DCHECK(!needs_null_check);  // The thunk cannot handle the null check.
      GetAssembler()->MaybeUnpoisonHeapReference(ref_reg);
      __ Bind(&return_address);
    }
    MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__, /* temp_loc */ LocationFrom(ip1));
    return;
  }

  // Array cells are never volatile variables, therefore array loads
  // never use Load-Acquire instructions on ARM64.
  const bool use_load_acquire = false;

  // /* HeapReference<Object> */ ref =
  //     *(obj + data_offset + index * sizeof(HeapReference<Object>))
  GenerateReferenceLoadWithBakerReadBarrier(instruction,
                                            ref,
                                            obj,
                                            data_offset,
                                            index,
                                            scale_factor,
                                            temp,
                                            needs_null_check,
                                            use_load_acquire);
}

void CodeGeneratorARM64::GenerateReferenceLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                   Location ref,
                                                                   Register obj,
                                                                   uint32_t offset,
                                                                   Location index,
                                                                   size_t scale_factor,
                                                                   Register temp,
                                                                   bool needs_null_check,
                                                                   bool use_load_acquire) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);
  // If we are emitting an array load, we should not be using a
  // Load Acquire instruction.  In other words:
  // `instruction->IsArrayGet()` => `!use_load_acquire`.
  DCHECK(!instruction->IsArrayGet() || !use_load_acquire);

  // Query `art::Thread::Current()->GetIsGcMarking()` (stored in the
  // Marking Register) to decide whether we need to enter the slow
  // path to mark the reference. Then, in the slow path, check the
  // gray bit in the lock word of the reference's holder (`obj`) to
  // decide whether to mark `ref` or not.
  //
  //   if (mr) {  // Thread::Current()->GetIsGcMarking()
  //     // Slow path.
  //     uint32_t rb_state = Lockword(obj->monitor_).ReadBarrierState();
  //     lfence;  // Load fence or artificial data dependency to prevent load-load reordering
  //     HeapReference<mirror::Object> ref = *src;  // Original reference load.
  //     bool is_gray = (rb_state == ReadBarrier::GrayState());
  //     if (is_gray) {
  //       entrypoint = Thread::Current()->pReadBarrierMarkReg ## root.reg()
  //       ref = entrypoint(ref);  // ref = ReadBarrier::Mark(ref);  // Runtime entry point call.
  //     }
  //   } else {
  //     HeapReference<mirror::Object> ref = *src;  // Original reference load.
  //   }

  // Slow path marking the object `ref` when the GC is marking. The
  // entrypoint will be loaded by the slow path code.
  SlowPathCodeARM64* slow_path =
      new (GetScopedAllocator()) LoadReferenceWithBakerReadBarrierSlowPathARM64(
          instruction,
          ref,
          obj,
          offset,
          index,
          scale_factor,
          needs_null_check,
          use_load_acquire,
          temp);
  AddSlowPath(slow_path);

  __ Cbnz(mr, slow_path->GetEntryLabel());
  // Fast path: the GC is not marking: just load the reference.
  GenerateRawReferenceLoad(
      instruction, ref, obj, offset, index, scale_factor, needs_null_check, use_load_acquire);
  __ Bind(slow_path->GetExitLabel());
  MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void CodeGeneratorARM64::UpdateReferenceFieldWithBakerReadBarrier(HInstruction* instruction,
                                                                  Location ref,
                                                                  Register obj,
                                                                  Location field_offset,
                                                                  Register temp,
                                                                  bool needs_null_check,
                                                                  bool use_load_acquire) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);
  // If we are emitting an array load, we should not be using a
  // Load Acquire instruction.  In other words:
  // `instruction->IsArrayGet()` => `!use_load_acquire`.
  DCHECK(!instruction->IsArrayGet() || !use_load_acquire);

  // Query `art::Thread::Current()->GetIsGcMarking()` (stored in the
  // Marking Register) to decide whether we need to enter the slow
  // path to update the reference field within `obj`. Then, in the
  // slow path, check the gray bit in the lock word of the reference's
  // holder (`obj`) to decide whether to mark `ref` and update the
  // field or not.
  //
  //   if (mr) {  // Thread::Current()->GetIsGcMarking()
  //     // Slow path.
  //     uint32_t rb_state = Lockword(obj->monitor_).ReadBarrierState();
  //     lfence;  // Load fence or artificial data dependency to prevent load-load reordering
  //     HeapReference<mirror::Object> ref = *(obj + field_offset);  // Reference load.
  //     bool is_gray = (rb_state == ReadBarrier::GrayState());
  //     if (is_gray) {
  //       old_ref = ref;
  //       entrypoint = Thread::Current()->pReadBarrierMarkReg ## root.reg()
  //       ref = entrypoint(ref);  // ref = ReadBarrier::Mark(ref);  // Runtime entry point call.
  //       compareAndSwapObject(obj, field_offset, old_ref, ref);
  //     }
  //   }

  // Slow path updating the object reference at address `obj + field_offset`
  // when the GC is marking. The entrypoint will be loaded by the slow path code.
  SlowPathCodeARM64* slow_path =
      new (GetScopedAllocator()) LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARM64(
          instruction,
          ref,
          obj,
          /* offset */ 0u,
          /* index */ field_offset,
          /* scale_factor */ 0u /* "times 1" */,
          needs_null_check,
          use_load_acquire,
          temp);
  AddSlowPath(slow_path);

  __ Cbnz(mr, slow_path->GetEntryLabel());
  // Fast path: the GC is not marking: nothing to do (the field is
  // up-to-date, and we don't need to load the reference).
  __ Bind(slow_path->GetExitLabel());
  MaybeGenerateMarkingRegisterCheck(/* code */ __LINE__);
}

void CodeGeneratorARM64::GenerateRawReferenceLoad(HInstruction* instruction,
                                                  Location ref,
                                                  Register obj,
                                                  uint32_t offset,
                                                  Location index,
                                                  size_t scale_factor,
                                                  bool needs_null_check,
                                                  bool use_load_acquire) {
  DCHECK(obj.IsW());
  DataType::Type type = DataType::Type::kReference;
  Register ref_reg = RegisterFrom(ref, type);

  // If needed, vixl::EmissionCheckScope guards are used to ensure
  // that no pools are emitted between the load (macro) instruction
  // and MaybeRecordImplicitNullCheck.

  if (index.IsValid()) {
    // Load types involving an "index": ArrayGet,
    // UnsafeGetObject/UnsafeGetObjectVolatile and UnsafeCASObject
    // intrinsics.
    if (use_load_acquire) {
      // UnsafeGetObjectVolatile intrinsic case.
      // Register `index` is not an index in an object array, but an
      // offset to an object reference field within object `obj`.
      DCHECK(instruction->IsInvoke()) << instruction->DebugName();
      DCHECK(instruction->GetLocations()->Intrinsified());
      DCHECK(instruction->AsInvoke()->GetIntrinsic() == Intrinsics::kUnsafeGetObjectVolatile)
          << instruction->AsInvoke()->GetIntrinsic();
      DCHECK_EQ(offset, 0u);
      DCHECK_EQ(scale_factor, 0u);
      DCHECK_EQ(needs_null_check, false);
      // /* HeapReference<mirror::Object> */ ref = *(obj + index)
      MemOperand field = HeapOperand(obj, XRegisterFrom(index));
      LoadAcquire(instruction, ref_reg, field, /* needs_null_check */ false);
    } else {
      // ArrayGet and UnsafeGetObject and UnsafeCASObject intrinsics cases.
      // /* HeapReference<mirror::Object> */ ref = *(obj + offset + (index << scale_factor))
      if (index.IsConstant()) {
        uint32_t computed_offset = offset + (Int64ConstantFrom(index) << scale_factor);
        EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
        Load(type, ref_reg, HeapOperand(obj, computed_offset));
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        Register temp = temps.AcquireW();
        __ Add(temp, obj, offset);
        {
          EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
          Load(type, ref_reg, HeapOperand(temp, XRegisterFrom(index), LSL, scale_factor));
          if (needs_null_check) {
            MaybeRecordImplicitNullCheck(instruction);
          }
        }
      }
    }
  } else {
    // /* HeapReference<mirror::Object> */ ref = *(obj + offset)
    MemOperand field = HeapOperand(obj, offset);
    if (use_load_acquire) {
      // Implicit null checks are handled by CodeGeneratorARM64::LoadAcquire.
      LoadAcquire(instruction, ref_reg, field, needs_null_check);
    } else {
      EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      Load(type, ref_reg, field);
      if (needs_null_check) {
        MaybeRecordImplicitNullCheck(instruction);
      }
    }
  }

  // Object* ref = ref_addr->AsMirrorPtr()
  GetAssembler()->MaybeUnpoisonHeapReference(ref_reg);
}

void CodeGeneratorARM64::MaybeGenerateMarkingRegisterCheck(int code, Location temp_loc) {
  // The following condition is a compile-time one, so it does not have a run-time cost.
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier && kIsDebugBuild) {
    // The following condition is a run-time one; it is executed after the
    // previous compile-time test, to avoid penalizing non-debug builds.
    if (GetCompilerOptions().EmitRunTimeChecksInDebugMode()) {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      Register temp = temp_loc.IsValid() ? WRegisterFrom(temp_loc) : temps.AcquireW();
      GetAssembler()->GenerateMarkingRegisterCheck(temp, code);
    }
  }
}

void CodeGeneratorARM64::GenerateReadBarrierSlow(HInstruction* instruction,
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
  SlowPathCodeARM64* slow_path = new (GetScopedAllocator())
      ReadBarrierForHeapReferenceSlowPathARM64(instruction, out, ref, obj, offset, index);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorARM64::MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                                      Location out,
                                                      Location ref,
                                                      Location obj,
                                                      uint32_t offset,
                                                      Location index) {
  if (kEmitCompilerReadBarrier) {
    // Baker's read barriers shall be handled by the fast path
    // (CodeGeneratorARM64::GenerateReferenceLoadWithBakerReadBarrier).
    DCHECK(!kUseBakerReadBarrier);
    // If heap poisoning is enabled, unpoisoning will be taken care of
    // by the runtime within the slow path.
    GenerateReadBarrierSlow(instruction, out, ref, obj, offset, index);
  } else if (kPoisonHeapReferences) {
    GetAssembler()->UnpoisonHeapReference(WRegisterFrom(out));
  }
}

void CodeGeneratorARM64::GenerateReadBarrierForRootSlow(HInstruction* instruction,
                                                        Location out,
                                                        Location root) {
  DCHECK(kEmitCompilerReadBarrier);

  // Insert a slow path based read barrier *after* the GC root load.
  //
  // Note that GC roots are not affected by heap poisoning, so we do
  // not need to do anything special for this here.
  SlowPathCodeARM64* slow_path =
      new (GetScopedAllocator()) ReadBarrierForRootSlowPathARM64(instruction, out, root);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void LocationsBuilderARM64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARM64::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (instruction->GetTableKind() == HClassTableGet::TableKind::kVTable) {
    uint32_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
        instruction->GetIndex(), kArm64PointerSize).SizeValue();
    __ Ldr(XRegisterFrom(locations->Out()),
           MemOperand(XRegisterFrom(locations->InAt(0)), method_offset));
  } else {
    uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
        instruction->GetIndex(), kArm64PointerSize));
    __ Ldr(XRegisterFrom(locations->Out()), MemOperand(XRegisterFrom(locations->InAt(0)),
        mirror::Class::ImtPtrOffset(kArm64PointerSize).Uint32Value()));
    __ Ldr(XRegisterFrom(locations->Out()),
           MemOperand(XRegisterFrom(locations->Out()), method_offset));
  }
}

static void PatchJitRootUse(uint8_t* code,
                            const uint8_t* roots_data,
                            vixl::aarch64::Literal<uint32_t>* literal,
                            uint64_t index_in_table) {
  uint32_t literal_offset = literal->GetOffset();
  uintptr_t address =
      reinterpret_cast<uintptr_t>(roots_data) + index_in_table * sizeof(GcRoot<mirror::Object>);
  uint8_t* data = code + literal_offset;
  reinterpret_cast<uint32_t*>(data)[0] = dchecked_integral_cast<uint32_t>(address);
}

void CodeGeneratorARM64::EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) {
  for (const auto& entry : jit_string_patches_) {
    const StringReference& string_reference = entry.first;
    vixl::aarch64::Literal<uint32_t>* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitStringRootIndex(string_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
  for (const auto& entry : jit_class_patches_) {
    const TypeReference& type_reference = entry.first;
    vixl::aarch64::Literal<uint32_t>* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitClassRootIndex(type_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
}

#undef __
#undef QUICK_ENTRY_POINT

}  // namespace arm64
}  // namespace art
