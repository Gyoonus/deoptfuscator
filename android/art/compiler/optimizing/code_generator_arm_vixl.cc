/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "code_generator_arm_vixl.h"

#include "arch/arm/asm_support_arm.h"
#include "arch/arm/instruction_set_features_arm.h"
#include "art_method.h"
#include "base/bit_utils.h"
#include "base/bit_utils_iterator.h"
#include "class_table.h"
#include "code_generator_utils.h"
#include "common_arm.h"
#include "compiled_method.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "gc/accounting/card_table.h"
#include "heap_poisoning.h"
#include "intrinsics_arm_vixl.h"
#include "linker/arm/relative_patcher_thumb2.h"
#include "linker/linker_patch.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "thread.h"
#include "utils/arm/assembler_arm_vixl.h"
#include "utils/arm/managed_register_arm.h"
#include "utils/assembler.h"
#include "utils/stack_checks.h"

namespace art {
namespace arm {

namespace vixl32 = vixl::aarch32;
using namespace vixl32;  // NOLINT(build/namespaces)

using helpers::DRegisterFrom;
using helpers::DWARFReg;
using helpers::HighDRegisterFrom;
using helpers::HighRegisterFrom;
using helpers::InputDRegisterAt;
using helpers::InputOperandAt;
using helpers::InputRegister;
using helpers::InputRegisterAt;
using helpers::InputSRegisterAt;
using helpers::InputVRegister;
using helpers::InputVRegisterAt;
using helpers::Int32ConstantFrom;
using helpers::Int64ConstantFrom;
using helpers::LocationFrom;
using helpers::LowRegisterFrom;
using helpers::LowSRegisterFrom;
using helpers::OperandFrom;
using helpers::OutputRegister;
using helpers::OutputSRegister;
using helpers::OutputVRegister;
using helpers::RegisterFrom;
using helpers::SRegisterFrom;
using helpers::Uint64ConstantFrom;

using vixl::ExactAssemblyScope;
using vixl::CodeBufferCheckScope;

using RegisterList = vixl32::RegisterList;

static bool ExpectedPairLayout(Location location) {
  // We expected this for both core and fpu register pairs.
  return ((location.low() & 1) == 0) && (location.low() + 1 == location.high());
}
// Use a local definition to prevent copying mistakes.
static constexpr size_t kArmWordSize = static_cast<size_t>(kArmPointerSize);
static constexpr size_t kArmBitsPerWord = kArmWordSize * kBitsPerByte;
static constexpr uint32_t kPackedSwitchCompareJumpThreshold = 7;

// Reference load (except object array loads) is using LDR Rt, [Rn, #offset] which can handle
// offset < 4KiB. For offsets >= 4KiB, the load shall be emitted as two or more instructions.
// For the Baker read barrier implementation using link-generated thunks we need to split
// the offset explicitly.
constexpr uint32_t kReferenceLoadMinFarOffset = 4 * KB;

// Flags controlling the use of link-time generated thunks for Baker read barriers.
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForFields = true;
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForArrays = true;
constexpr bool kBakerReadBarrierLinkTimeThunksEnableForGcRoots = true;

// The reserved entrypoint register for link-time generated thunks.
const vixl32::Register kBakerCcEntrypointRegister = r4;

// Using a base helps identify when we hit Marking Register check breakpoints.
constexpr int kMarkingRegisterCheckBreakCodeBaseCode = 0x10;

#ifdef __
#error "ARM Codegen VIXL macro-assembler macro already defined."
#endif

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<CodeGeneratorARMVIXL*>(codegen)->GetVIXLAssembler()->  // NOLINT
#define QUICK_ENTRY_POINT(x) QUICK_ENTRYPOINT_OFFSET(kArmPointerSize, x).Int32Value()

// Marker that code is yet to be, and must, be implemented.
#define TODO_VIXL32(level) LOG(level) << __PRETTY_FUNCTION__ << " unimplemented "

static inline void ExcludeIPAndBakerCcEntrypointRegister(UseScratchRegisterScope* temps,
                                                         HInstruction* instruction) {
  DCHECK(temps->IsAvailable(ip));
  temps->Exclude(ip);
  DCHECK(!temps->IsAvailable(kBakerCcEntrypointRegister));
  DCHECK_EQ(kBakerCcEntrypointRegister.GetCode(),
            linker::Thumb2RelativePatcher::kBakerCcEntrypointRegister);
  DCHECK_NE(instruction->GetLocations()->GetTempCount(), 0u);
  DCHECK(RegisterFrom(instruction->GetLocations()->GetTemp(
      instruction->GetLocations()->GetTempCount() - 1u)).Is(kBakerCcEntrypointRegister));
}

static inline void EmitPlaceholderBne(CodeGeneratorARMVIXL* codegen, vixl32::Label* patch_label) {
  ExactAssemblyScope eas(codegen->GetVIXLAssembler(), kMaxInstructionSizeInBytes);
  __ bind(patch_label);
  vixl32::Label placeholder_label;
  __ b(ne, EncodingSize(Wide), &placeholder_label);  // Placeholder, patched at link-time.
  __ bind(&placeholder_label);
}

static inline bool CanEmitNarrowLdr(vixl32::Register rt, vixl32::Register rn, uint32_t offset) {
  return rt.IsLow() && rn.IsLow() && offset < 32u;
}

class EmitAdrCode {
 public:
  EmitAdrCode(ArmVIXLMacroAssembler* assembler, vixl32::Register rd, vixl32::Label* label)
      : assembler_(assembler), rd_(rd), label_(label) {
    ExactAssemblyScope aas(assembler, kMaxInstructionSizeInBytes);
    adr_location_ = assembler->GetCursorOffset();
    assembler->adr(EncodingSize(Wide), rd, label);
  }

  ~EmitAdrCode() {
    DCHECK(label_->IsBound());
    // The ADR emitted by the assembler does not set the Thumb mode bit we need.
    // TODO: Maybe extend VIXL to allow ADR for return address?
    uint8_t* raw_adr = assembler_->GetBuffer()->GetOffsetAddress<uint8_t*>(adr_location_);
    // Expecting ADR encoding T3 with `(offset & 1) == 0`.
    DCHECK_EQ(raw_adr[1] & 0xfbu, 0xf2u);           // Check bits 24-31, except 26.
    DCHECK_EQ(raw_adr[0] & 0xffu, 0x0fu);           // Check bits 16-23.
    DCHECK_EQ(raw_adr[3] & 0x8fu, rd_.GetCode());   // Check bits 8-11 and 15.
    DCHECK_EQ(raw_adr[2] & 0x01u, 0x00u);           // Check bit 0, i.e. the `offset & 1`.
    // Add the Thumb mode bit.
    raw_adr[2] |= 0x01u;
  }

 private:
  ArmVIXLMacroAssembler* const assembler_;
  vixl32::Register rd_;
  vixl32::Label* const label_;
  int32_t adr_location_;
};

// SaveLiveRegisters and RestoreLiveRegisters from SlowPathCodeARM operate on sets of S registers,
// for each live D registers they treat two corresponding S registers as live ones.
//
// Two following functions (SaveContiguousSRegisterList, RestoreContiguousSRegisterList) build
// from a list of contiguous S registers a list of contiguous D registers (processing first/last
// S registers corner cases) and save/restore this new list treating them as D registers.
// - decreasing code size
// - avoiding hazards on Cortex-A57, when a pair of S registers for an actual live D register is
//   restored and then used in regular non SlowPath code as D register.
//
// For the following example (v means the S register is live):
//   D names: |    D0   |    D1   |    D2   |    D4   | ...
//   S names: | S0 | S1 | S2 | S3 | S4 | S5 | S6 | S7 | ...
//   Live?    |    |  v |  v |  v |  v |  v |  v |    | ...
//
// S1 and S6 will be saved/restored independently; D registers list (D1, D2) will be processed
// as D registers.
//
// TODO(VIXL): All this code should be unnecessary once the VIXL AArch32 backend provides helpers
// for lists of floating-point registers.
static size_t SaveContiguousSRegisterList(size_t first,
                                          size_t last,
                                          CodeGenerator* codegen,
                                          size_t stack_offset) {
  static_assert(kSRegSizeInBytes == kArmWordSize, "Broken assumption on reg/word sizes.");
  static_assert(kDRegSizeInBytes == 2 * kArmWordSize, "Broken assumption on reg/word sizes.");
  DCHECK_LE(first, last);
  if ((first == last) && (first == 0)) {
    __ Vstr(vixl32::SRegister(first), MemOperand(sp, stack_offset));
    return stack_offset + kSRegSizeInBytes;
  }
  if (first % 2 == 1) {
    __ Vstr(vixl32::SRegister(first++), MemOperand(sp, stack_offset));
    stack_offset += kSRegSizeInBytes;
  }

  bool save_last = false;
  if (last % 2 == 0) {
    save_last = true;
    --last;
  }

  if (first < last) {
    vixl32::DRegister d_reg = vixl32::DRegister(first / 2);
    DCHECK_EQ((last - first + 1) % 2, 0u);
    size_t number_of_d_regs = (last - first + 1) / 2;

    if (number_of_d_regs == 1) {
      __ Vstr(d_reg, MemOperand(sp, stack_offset));
    } else if (number_of_d_regs > 1) {
      UseScratchRegisterScope temps(down_cast<CodeGeneratorARMVIXL*>(codegen)->GetVIXLAssembler());
      vixl32::Register base = sp;
      if (stack_offset != 0) {
        base = temps.Acquire();
        __ Add(base, sp, Operand::From(stack_offset));
      }
      __ Vstm(F64, base, NO_WRITE_BACK, DRegisterList(d_reg, number_of_d_regs));
    }
    stack_offset += number_of_d_regs * kDRegSizeInBytes;
  }

  if (save_last) {
    __ Vstr(vixl32::SRegister(last + 1), MemOperand(sp, stack_offset));
    stack_offset += kSRegSizeInBytes;
  }

  return stack_offset;
}

static size_t RestoreContiguousSRegisterList(size_t first,
                                             size_t last,
                                             CodeGenerator* codegen,
                                             size_t stack_offset) {
  static_assert(kSRegSizeInBytes == kArmWordSize, "Broken assumption on reg/word sizes.");
  static_assert(kDRegSizeInBytes == 2 * kArmWordSize, "Broken assumption on reg/word sizes.");
  DCHECK_LE(first, last);
  if ((first == last) && (first == 0)) {
    __ Vldr(vixl32::SRegister(first), MemOperand(sp, stack_offset));
    return stack_offset + kSRegSizeInBytes;
  }
  if (first % 2 == 1) {
    __ Vldr(vixl32::SRegister(first++), MemOperand(sp, stack_offset));
    stack_offset += kSRegSizeInBytes;
  }

  bool restore_last = false;
  if (last % 2 == 0) {
    restore_last = true;
    --last;
  }

  if (first < last) {
    vixl32::DRegister d_reg = vixl32::DRegister(first / 2);
    DCHECK_EQ((last - first + 1) % 2, 0u);
    size_t number_of_d_regs = (last - first + 1) / 2;
    if (number_of_d_regs == 1) {
      __ Vldr(d_reg, MemOperand(sp, stack_offset));
    } else if (number_of_d_regs > 1) {
      UseScratchRegisterScope temps(down_cast<CodeGeneratorARMVIXL*>(codegen)->GetVIXLAssembler());
      vixl32::Register base = sp;
      if (stack_offset != 0) {
        base = temps.Acquire();
        __ Add(base, sp, Operand::From(stack_offset));
      }
      __ Vldm(F64, base, NO_WRITE_BACK, DRegisterList(d_reg, number_of_d_regs));
    }
    stack_offset += number_of_d_regs * kDRegSizeInBytes;
  }

  if (restore_last) {
    __ Vldr(vixl32::SRegister(last + 1), MemOperand(sp, stack_offset));
    stack_offset += kSRegSizeInBytes;
  }

  return stack_offset;
}

static LoadOperandType GetLoadOperandType(DataType::Type type) {
  switch (type) {
    case DataType::Type::kReference:
      return kLoadWord;
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
      return kLoadUnsignedByte;
    case DataType::Type::kInt8:
      return kLoadSignedByte;
    case DataType::Type::kUint16:
      return kLoadUnsignedHalfword;
    case DataType::Type::kInt16:
      return kLoadSignedHalfword;
    case DataType::Type::kInt32:
      return kLoadWord;
    case DataType::Type::kInt64:
      return kLoadWordPair;
    case DataType::Type::kFloat32:
      return kLoadSWord;
    case DataType::Type::kFloat64:
      return kLoadDWord;
    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

static StoreOperandType GetStoreOperandType(DataType::Type type) {
  switch (type) {
    case DataType::Type::kReference:
      return kStoreWord;
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      return kStoreByte;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      return kStoreHalfword;
    case DataType::Type::kInt32:
      return kStoreWord;
    case DataType::Type::kInt64:
      return kStoreWordPair;
    case DataType::Type::kFloat32:
      return kStoreSWord;
    case DataType::Type::kFloat64:
      return kStoreDWord;
    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

void SlowPathCodeARMVIXL::SaveLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  size_t stack_offset = codegen->GetFirstRegisterSlotInSlowPath();
  size_t orig_offset = stack_offset;

  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ true);
  for (uint32_t i : LowToHighBits(core_spills)) {
    // If the register holds an object, update the stack mask.
    if (locations->RegisterContainsObject(i)) {
      locations->SetStackBit(stack_offset / kVRegSize);
    }
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_core_stack_offsets_[i] = stack_offset;
    stack_offset += kArmWordSize;
  }

  CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
  arm_codegen->GetAssembler()->StoreRegisterList(core_spills, orig_offset);

  uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ false);
  orig_offset = stack_offset;
  for (uint32_t i : LowToHighBits(fp_spills)) {
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_fpu_stack_offsets_[i] = stack_offset;
    stack_offset += kArmWordSize;
  }

  stack_offset = orig_offset;
  while (fp_spills != 0u) {
    uint32_t begin = CTZ(fp_spills);
    uint32_t tmp = fp_spills + (1u << begin);
    fp_spills &= tmp;  // Clear the contiguous range of 1s.
    uint32_t end = (tmp == 0u) ? 32u : CTZ(tmp);  // CTZ(0) is undefined.
    stack_offset = SaveContiguousSRegisterList(begin, end - 1, codegen, stack_offset);
  }
  DCHECK_LE(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
}

void SlowPathCodeARMVIXL::RestoreLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  size_t stack_offset = codegen->GetFirstRegisterSlotInSlowPath();
  size_t orig_offset = stack_offset;

  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ true);
  for (uint32_t i : LowToHighBits(core_spills)) {
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    stack_offset += kArmWordSize;
  }

  // TODO(VIXL): Check the coherency of stack_offset after this with a test.
  CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
  arm_codegen->GetAssembler()->LoadRegisterList(core_spills, orig_offset);

  uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers */ false);
  while (fp_spills != 0u) {
    uint32_t begin = CTZ(fp_spills);
    uint32_t tmp = fp_spills + (1u << begin);
    fp_spills &= tmp;  // Clear the contiguous range of 1s.
    uint32_t end = (tmp == 0u) ? 32u : CTZ(tmp);  // CTZ(0) is undefined.
    stack_offset = RestoreContiguousSRegisterList(begin, end - 1, codegen, stack_offset);
  }
  DCHECK_LE(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
}

class NullCheckSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit NullCheckSlowPathARMVIXL(HNullCheck* instruction) : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    arm_codegen->InvokeRuntime(kQuickThrowNullPointer,
                               instruction_,
                               instruction_->GetDexPc(),
                               this);
    CheckEntrypointTypes<kQuickThrowNullPointer, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "NullCheckSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCheckSlowPathARMVIXL);
};

class DivZeroCheckSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit DivZeroCheckSlowPathARMVIXL(HDivZeroCheck* instruction)
      : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
    arm_codegen->InvokeRuntime(kQuickThrowDivZero, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowDivZero, void, void>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "DivZeroCheckSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DivZeroCheckSlowPathARMVIXL);
};

class SuspendCheckSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  SuspendCheckSlowPathARMVIXL(HSuspendCheck* instruction, HBasicBlock* successor)
      : SlowPathCodeARMVIXL(instruction), successor_(successor) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
    arm_codegen->InvokeRuntime(kQuickTestSuspend, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickTestSuspend, void, void>();
    if (successor_ == nullptr) {
      __ B(GetReturnLabel());
    } else {
      __ B(arm_codegen->GetLabelOf(successor_));
    }
  }

  vixl32::Label* GetReturnLabel() {
    DCHECK(successor_ == nullptr);
    return &return_label_;
  }

  HBasicBlock* GetSuccessor() const {
    return successor_;
  }

  const char* GetDescription() const OVERRIDE { return "SuspendCheckSlowPathARMVIXL"; }

 private:
  // If not null, the block to branch to after the suspend check.
  HBasicBlock* const successor_;

  // If `successor_` is null, the label to branch to after the suspend check.
  vixl32::Label return_label_;

  DISALLOW_COPY_AND_ASSIGN(SuspendCheckSlowPathARMVIXL);
};

class BoundsCheckSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit BoundsCheckSlowPathARMVIXL(HBoundsCheck* instruction)
      : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();

    __ Bind(GetEntryLabel());
    if (instruction_->CanThrowIntoCatchBlock()) {
      // Live registers will be restored in the catch block if caught.
      SaveLiveRegisters(codegen, instruction_->GetLocations());
    }
    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    codegen->EmitParallelMoves(
        locations->InAt(0),
        LocationFrom(calling_convention.GetRegisterAt(0)),
        DataType::Type::kInt32,
        locations->InAt(1),
        LocationFrom(calling_convention.GetRegisterAt(1)),
        DataType::Type::kInt32);
    QuickEntrypointEnum entrypoint = instruction_->AsBoundsCheck()->IsStringCharAt()
        ? kQuickThrowStringBounds
        : kQuickThrowArrayBounds;
    arm_codegen->InvokeRuntime(entrypoint, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickThrowStringBounds, void, int32_t, int32_t>();
    CheckEntrypointTypes<kQuickThrowArrayBounds, void, int32_t, int32_t>();
  }

  bool IsFatal() const OVERRIDE { return true; }

  const char* GetDescription() const OVERRIDE { return "BoundsCheckSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsCheckSlowPathARMVIXL);
};

class LoadClassSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  LoadClassSlowPathARMVIXL(HLoadClass* cls, HInstruction* at, uint32_t dex_pc, bool do_clinit)
      : SlowPathCodeARMVIXL(at), cls_(cls), dex_pc_(dex_pc), do_clinit_(do_clinit) {
    DCHECK(at->IsLoadClass() || at->IsClinitCheck());
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    Location out = locations->Out();

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    dex::TypeIndex type_index = cls_->GetTypeIndex();
    __ Mov(calling_convention.GetRegisterAt(0), type_index.index_);
    QuickEntrypointEnum entrypoint = do_clinit_ ? kQuickInitializeStaticStorage
                                                : kQuickInitializeType;
    arm_codegen->InvokeRuntime(entrypoint, instruction_, dex_pc_, this);
    if (do_clinit_) {
      CheckEntrypointTypes<kQuickInitializeStaticStorage, void*, uint32_t>();
    } else {
      CheckEntrypointTypes<kQuickInitializeType, void*, uint32_t>();
    }

    // Move the class to the desired location.
    if (out.IsValid()) {
      DCHECK(out.IsRegister() && !locations->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      arm_codegen->Move32(locations->Out(), LocationFrom(r0));
    }
    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadClassSlowPathARMVIXL"; }

 private:
  // The class this slow path will load.
  HLoadClass* const cls_;

  // The dex PC of `at_`.
  const uint32_t dex_pc_;

  // Whether to initialize the class.
  const bool do_clinit_;

  DISALLOW_COPY_AND_ASSIGN(LoadClassSlowPathARMVIXL);
};

class LoadStringSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit LoadStringSlowPathARMVIXL(HLoadString* instruction)
      : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    DCHECK(instruction_->IsLoadString());
    DCHECK_EQ(instruction_->AsLoadString()->GetLoadKind(), HLoadString::LoadKind::kBssEntry);
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));
    const dex::StringIndex string_index = instruction_->AsLoadString()->GetStringIndex();

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    __ Mov(calling_convention.GetRegisterAt(0), string_index.index_);
    arm_codegen->InvokeRuntime(kQuickResolveString, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();

    arm_codegen->Move32(locations->Out(), LocationFrom(r0));
    RestoreLiveRegisters(codegen, locations);

    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "LoadStringSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadStringSlowPathARMVIXL);
};

class TypeCheckSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  TypeCheckSlowPathARMVIXL(HInstruction* instruction, bool is_fatal)
      : SlowPathCodeARMVIXL(instruction), is_fatal_(is_fatal) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(instruction_->IsCheckCast()
           || !locations->GetLiveRegisters()->ContainsCoreRegister(locations->Out().reg()));

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());

    if (!is_fatal_ || instruction_->CanThrowIntoCatchBlock()) {
      SaveLiveRegisters(codegen, locations);
    }

    // We're moving two locations to locations that could overlap, so we need a parallel
    // move resolver.
    InvokeRuntimeCallingConventionARMVIXL calling_convention;

    codegen->EmitParallelMoves(locations->InAt(0),
                               LocationFrom(calling_convention.GetRegisterAt(0)),
                               DataType::Type::kReference,
                               locations->InAt(1),
                               LocationFrom(calling_convention.GetRegisterAt(1)),
                               DataType::Type::kReference);
    if (instruction_->IsInstanceOf()) {
      arm_codegen->InvokeRuntime(kQuickInstanceofNonTrivial,
                                 instruction_,
                                 instruction_->GetDexPc(),
                                 this);
      CheckEntrypointTypes<kQuickInstanceofNonTrivial, size_t, mirror::Object*, mirror::Class*>();
      arm_codegen->Move32(locations->Out(), LocationFrom(r0));
    } else {
      DCHECK(instruction_->IsCheckCast());
      arm_codegen->InvokeRuntime(kQuickCheckInstanceOf,
                                 instruction_,
                                 instruction_->GetDexPc(),
                                 this);
      CheckEntrypointTypes<kQuickCheckInstanceOf, void, mirror::Object*, mirror::Class*>();
    }

    if (!is_fatal_) {
      RestoreLiveRegisters(codegen, locations);
      __ B(GetExitLabel());
    }
  }

  const char* GetDescription() const OVERRIDE { return "TypeCheckSlowPathARMVIXL"; }

  bool IsFatal() const OVERRIDE { return is_fatal_; }

 private:
  const bool is_fatal_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckSlowPathARMVIXL);
};

class DeoptimizationSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit DeoptimizationSlowPathARMVIXL(HDeoptimize* instruction)
      : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    __ Bind(GetEntryLabel());
        LocationSummary* locations = instruction_->GetLocations();
    SaveLiveRegisters(codegen, locations);
    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    __ Mov(calling_convention.GetRegisterAt(0),
           static_cast<uint32_t>(instruction_->AsDeoptimize()->GetDeoptimizationKind()));

    arm_codegen->InvokeRuntime(kQuickDeoptimize, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickDeoptimize, void, DeoptimizationKind>();
  }

  const char* GetDescription() const OVERRIDE { return "DeoptimizationSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeoptimizationSlowPathARMVIXL);
};

class ArraySetSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit ArraySetSlowPathARMVIXL(HInstruction* instruction) : SlowPathCodeARMVIXL(instruction) {}

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConventionARMVIXL calling_convention;
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

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    arm_codegen->InvokeRuntime(kQuickAputObject, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<kQuickAputObject, void, mirror::Array*, int32_t, mirror::Object*>();
    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ArraySetSlowPathARMVIXL"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArraySetSlowPathARMVIXL);
};

// Abstract base class for read barrier slow paths marking a reference
// `ref`.
//
// Argument `entrypoint` must be a register location holding the read
// barrier marking runtime entry point to be invoked or an empty
// location; in the latter case, the read barrier marking runtime
// entry point will be loaded by the slow path code itself.
class ReadBarrierMarkSlowPathBaseARMVIXL : public SlowPathCodeARMVIXL {
 protected:
  ReadBarrierMarkSlowPathBaseARMVIXL(HInstruction* instruction, Location ref, Location entrypoint)
      : SlowPathCodeARMVIXL(instruction), ref_(ref), entrypoint_(entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathBaseARMVIXL"; }

  // Generate assembly code calling the read barrier marking runtime
  // entry point (ReadBarrierMarkRegX).
  void GenerateReadBarrierMarkRuntimeCall(CodeGenerator* codegen) {
    vixl32::Register ref_reg = RegisterFrom(ref_);

    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    DCHECK(!ref_reg.Is(sp));
    DCHECK(!ref_reg.Is(lr));
    DCHECK(!ref_reg.Is(pc));
    // IP is used internally by the ReadBarrierMarkRegX entry point
    // as a temporary, it cannot be the entry point's input/output.
    DCHECK(!ref_reg.Is(ip));
    DCHECK(ref_reg.IsRegister()) << ref_reg;
    // "Compact" slow path, saving two moves.
    //
    // Instead of using the standard runtime calling convention (input
    // and output in R0):
    //
    //   R0 <- ref
    //   R0 <- ReadBarrierMark(R0)
    //   ref <- R0
    //
    // we just use rX (the register containing `ref`) as input and output
    // of a dedicated entrypoint:
    //
    //   rX <- ReadBarrierMarkRegX(rX)
    //
    if (entrypoint_.IsValid()) {
      arm_codegen->ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction_, this);
      __ Blx(RegisterFrom(entrypoint_));
    } else {
      // Entrypoint is not already loaded, load from the thread.
      int32_t entry_point_offset =
          Thread::ReadBarrierMarkEntryPointsOffset<kArmPointerSize>(ref_reg.GetCode());
      // This runtime call does not require a stack map.
      arm_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset, instruction_, this);
    }
  }

  // The location (register) of the marked object reference.
  const Location ref_;

  // The location of the entrypoint if already loaded.
  const Location entrypoint_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathBaseARMVIXL);
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
class ReadBarrierMarkSlowPathARMVIXL : public ReadBarrierMarkSlowPathBaseARMVIXL {
 public:
  ReadBarrierMarkSlowPathARMVIXL(HInstruction* instruction,
                                 Location ref,
                                 Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARMVIXL(instruction, ref, entrypoint) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierMarkSlowPathARMVIXL"; }

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
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierMarkSlowPathARMVIXL);
};

// Slow path loading `obj`'s lock word, loading a reference from
// object `*(obj + offset + (index << scale_factor))` into `ref`, and
// marking `ref` if `obj` is gray according to the lock word (Baker
// read barrier). The field `obj.field` in the object `obj` holding
// this reference does not get updated by this slow path after marking
// (see LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL
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
class LoadReferenceWithBakerReadBarrierSlowPathARMVIXL : public ReadBarrierMarkSlowPathBaseARMVIXL {
 public:
  LoadReferenceWithBakerReadBarrierSlowPathARMVIXL(HInstruction* instruction,
                                                   Location ref,
                                                   vixl32::Register obj,
                                                   uint32_t offset,
                                                   Location index,
                                                   ScaleFactor scale_factor,
                                                   bool needs_null_check,
                                                   vixl32::Register temp,
                                                   Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARMVIXL(instruction, ref, entrypoint),
        obj_(obj),
        offset_(offset),
        index_(index),
        scale_factor_(scale_factor),
        needs_null_check_(needs_null_check),
        temp_(temp) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "LoadReferenceWithBakerReadBarrierSlowPathARMVIXL";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    vixl32::Register ref_reg = RegisterFrom(ref_);
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_reg.GetCode())) << ref_reg;
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
    // not be IP, as we may use it to emit the reference load (in the
    // call to GenerateRawReferenceLoad below), and we need the lock
    // word to still be in `temp_` after the reference load.
    DCHECK(!temp_.Is(ip));

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

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);

    // /* int32_t */ monitor = obj->monitor_
    uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();
    arm_codegen->GetAssembler()->LoadFromOffset(kLoadWord, temp_, obj_, monitor_offset);
    if (needs_null_check_) {
      codegen->MaybeRecordImplicitNullCheck(instruction_);
    }
    // /* LockWord */ lock_word = LockWord(monitor)
    static_assert(sizeof(LockWord) == sizeof(int32_t),
                  "art::LockWord and int32_t have different sizes.");

    // Introduce a dependency on the lock_word including the rb_state,
    // which shall prevent load-load reordering without using
    // a memory barrier (which would be more expensive).
    // `obj` is unchanged by this operation, but its value now depends
    // on `temp`.
    __ Add(obj_, obj_, Operand(temp_, ShiftType::LSR, 32));

    // The actual reference load.
    // A possible implicit null check has already been handled above.
    arm_codegen->GenerateRawReferenceLoad(
        instruction_, ref_, obj_, offset_, index_, scale_factor_, /* needs_null_check */ false);

    // Mark the object `ref` when `obj` is gray.
    //
    //   if (rb_state == ReadBarrier::GrayState())
    //     ref = ReadBarrier::Mark(ref);
    //
    // Given the numeric representation, it's enough to check the low bit of the
    // rb_state. We do that by shifting the bit out of the lock word with LSRS
    // which can be a 16-bit instruction unlike the TST immediate.
    static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
    static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
    __ Lsrs(temp_, temp_, LockWord::kReadBarrierStateShift + 1);
    __ B(cc, GetExitLabel());  // Carry flag is the last bit shifted out by LSRS.
    GenerateReadBarrierMarkRuntimeCall(codegen);

    __ B(GetExitLabel());
  }

 private:
  // The register containing the object holding the marked object reference field.
  vixl32::Register obj_;
  // The offset, index and scale factor to access the reference in `obj_`.
  uint32_t offset_;
  Location index_;
  ScaleFactor scale_factor_;
  // Is a null check required?
  bool needs_null_check_;
  // A temporary register used to hold the lock word of `obj_`.
  vixl32::Register temp_;

  DISALLOW_COPY_AND_ASSIGN(LoadReferenceWithBakerReadBarrierSlowPathARMVIXL);
};

// Slow path loading `obj`'s lock word, loading a reference from
// object `*(obj + offset + (index << scale_factor))` into `ref`, and
// marking `ref` if `obj` is gray according to the lock word (Baker
// read barrier). If needed, this slow path also atomically updates
// the field `obj.field` in the object `obj` holding this reference
// after marking (contrary to
// LoadReferenceWithBakerReadBarrierSlowPathARMVIXL above, which never
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
class LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL
    : public ReadBarrierMarkSlowPathBaseARMVIXL {
 public:
  LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL(
      HInstruction* instruction,
      Location ref,
      vixl32::Register obj,
      uint32_t offset,
      Location index,
      ScaleFactor scale_factor,
      bool needs_null_check,
      vixl32::Register temp1,
      vixl32::Register temp2,
      Location entrypoint = Location::NoLocation())
      : ReadBarrierMarkSlowPathBaseARMVIXL(instruction, ref, entrypoint),
        obj_(obj),
        offset_(offset),
        index_(index),
        scale_factor_(scale_factor),
        needs_null_check_(needs_null_check),
        temp1_(temp1),
        temp2_(temp2) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  const char* GetDescription() const OVERRIDE {
    return "LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL";
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    vixl32::Register ref_reg = RegisterFrom(ref_);
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(ref_reg.GetCode())) << ref_reg;
    DCHECK_NE(ref_.reg(), LocationFrom(temp1_).reg());

    // This slow path is only used by the UnsafeCASObject intrinsic at the moment.
    DCHECK((instruction_->IsInvokeVirtual() && instruction_->GetLocations()->Intrinsified()))
        << "Unexpected instruction in read barrier marking and field updating slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kUnsafeCASObject);
    DCHECK_EQ(offset_, 0u);
    DCHECK_EQ(scale_factor_, ScaleFactor::TIMES_1);
    Location field_offset = index_;
    DCHECK(field_offset.IsRegisterPair()) << field_offset;

    // Temporary register `temp1_`, used to store the lock word, must
    // not be IP, as we may use it to emit the reference load (in the
    // call to GenerateRawReferenceLoad below), and we need the lock
    // word to still be in `temp1_` after the reference load.
    DCHECK(!temp1_.Is(ip));

    __ Bind(GetEntryLabel());

    // The implementation is similar to LoadReferenceWithBakerReadBarrierSlowPathARMVIXL's:
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

    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);

    // /* int32_t */ monitor = obj->monitor_
    uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();
    arm_codegen->GetAssembler()->LoadFromOffset(kLoadWord, temp1_, obj_, monitor_offset);
    if (needs_null_check_) {
      codegen->MaybeRecordImplicitNullCheck(instruction_);
    }
    // /* LockWord */ lock_word = LockWord(monitor)
    static_assert(sizeof(LockWord) == sizeof(int32_t),
                  "art::LockWord and int32_t have different sizes.");

    // Introduce a dependency on the lock_word including the rb_state,
    // which shall prevent load-load reordering without using
    // a memory barrier (which would be more expensive).
    // `obj` is unchanged by this operation, but its value now depends
    // on `temp`.
    __ Add(obj_, obj_, Operand(temp1_, ShiftType::LSR, 32));

    // The actual reference load.
    // A possible implicit null check has already been handled above.
    arm_codegen->GenerateRawReferenceLoad(
        instruction_, ref_, obj_, offset_, index_, scale_factor_, /* needs_null_check */ false);

    // Mark the object `ref` when `obj` is gray.
    //
    //   if (rb_state == ReadBarrier::GrayState())
    //     ref = ReadBarrier::Mark(ref);
    //
    // Given the numeric representation, it's enough to check the low bit of the
    // rb_state. We do that by shifting the bit out of the lock word with LSRS
    // which can be a 16-bit instruction unlike the TST immediate.
    static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
    static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
    __ Lsrs(temp1_, temp1_, LockWord::kReadBarrierStateShift + 1);
    __ B(cc, GetExitLabel());  // Carry flag is the last bit shifted out by LSRS.

    // Save the old value of the reference before marking it.
    // Note that we cannot use IP to save the old reference, as IP is
    // used internally by the ReadBarrierMarkRegX entry point, and we
    // need the old reference after the call to that entry point.
    DCHECK(!temp1_.Is(ip));
    __ Mov(temp1_, ref_reg);

    GenerateReadBarrierMarkRuntimeCall(codegen);

    // If the new reference is different from the old reference,
    // update the field in the holder (`*(obj_ + field_offset)`).
    //
    // Note that this field could also hold a different object, if
    // another thread had concurrently changed it. In that case, the
    // LDREX/CMP/BNE sequence of instructions in the compare-and-set
    // (CAS) operation below would abort the CAS, leaving the field
    // as-is.
    __ Cmp(temp1_, ref_reg);
    __ B(eq, GetExitLabel());

    // Update the the holder's field atomically.  This may fail if
    // mutator updates before us, but it's OK.  This is achieved
    // using a strong compare-and-set (CAS) operation with relaxed
    // memory synchronization ordering, where the expected value is
    // the old reference and the desired value is the new reference.

    UseScratchRegisterScope temps(arm_codegen->GetVIXLAssembler());
    // Convenience aliases.
    vixl32::Register base = obj_;
    // The UnsafeCASObject intrinsic uses a register pair as field
    // offset ("long offset"), of which only the low part contains
    // data.
    vixl32::Register offset = LowRegisterFrom(field_offset);
    vixl32::Register expected = temp1_;
    vixl32::Register value = ref_reg;
    vixl32::Register tmp_ptr = temps.Acquire();       // Pointer to actual memory.
    vixl32::Register tmp = temp2_;                    // Value in memory.

    __ Add(tmp_ptr, base, offset);

    if (kPoisonHeapReferences) {
      arm_codegen->GetAssembler()->PoisonHeapReference(expected);
      if (value.Is(expected)) {
        // Do not poison `value`, as it is the same register as
        // `expected`, which has just been poisoned.
      } else {
        arm_codegen->GetAssembler()->PoisonHeapReference(value);
      }
    }

    // do {
    //   tmp = [r_ptr] - expected;
    // } while (tmp == 0 && failure([r_ptr] <- r_new_value));

    vixl32::Label loop_head, comparison_failed, exit_loop;
    __ Bind(&loop_head);
    __ Ldrex(tmp, MemOperand(tmp_ptr));
    __ Cmp(tmp, expected);
    __ B(ne, &comparison_failed, /* far_target */ false);
    __ Strex(tmp, value, MemOperand(tmp_ptr));
    __ CompareAndBranchIfZero(tmp, &exit_loop, /* far_target */ false);
    __ B(&loop_head);
    __ Bind(&comparison_failed);
    __ Clrex();
    __ Bind(&exit_loop);

    if (kPoisonHeapReferences) {
      arm_codegen->GetAssembler()->UnpoisonHeapReference(expected);
      if (value.Is(expected)) {
        // Do not unpoison `value`, as it is the same register as
        // `expected`, which has just been unpoisoned.
      } else {
        arm_codegen->GetAssembler()->UnpoisonHeapReference(value);
      }
    }

    __ B(GetExitLabel());
  }

 private:
  // The register containing the object holding the marked object reference field.
  const vixl32::Register obj_;
  // The offset, index and scale factor to access the reference in `obj_`.
  uint32_t offset_;
  Location index_;
  ScaleFactor scale_factor_;
  // Is a null check required?
  bool needs_null_check_;
  // A temporary register used to hold the lock word of `obj_`; and
  // also to hold the original reference value, when the reference is
  // marked.
  const vixl32::Register temp1_;
  // A temporary register used in the implementation of the CAS, to
  // update the object's reference field.
  const vixl32::Register temp2_;

  DISALLOW_COPY_AND_ASSIGN(LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL);
};

// Slow path generating a read barrier for a heap reference.
class ReadBarrierForHeapReferenceSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  ReadBarrierForHeapReferenceSlowPathARMVIXL(HInstruction* instruction,
                                             Location out,
                                             Location ref,
                                             Location obj,
                                             uint32_t offset,
                                             Location index)
      : SlowPathCodeARMVIXL(instruction),
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
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();
    vixl32::Register reg_out = RegisterFrom(out_);
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(reg_out.GetCode()));
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
        vixl32::Register index_reg = RegisterFrom(index_);
        DCHECK(locations->GetLiveRegisters()->ContainsCoreRegister(index_reg.GetCode()));
        if (codegen->IsCoreCalleeSaveRegister(index_reg.GetCode())) {
          // We are about to change the value of `index_reg` (see the
          // calls to art::arm::ArmVIXLMacroAssembler::Lsl and
          // art::arm::ArmVIXLMacroAssembler::Add below), but it has
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
          vixl32::Register free_reg = FindAvailableCallerSaveRegister(codegen);
          __ Mov(free_reg, index_reg);
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
        __ Lsl(index_reg, index_reg, TIMES_4);
        static_assert(
            sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
            "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
        __ Add(index_reg, index_reg, offset_);
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
    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    HParallelMove parallel_move(codegen->GetGraph()->GetAllocator());
    parallel_move.AddMove(ref_,
                          LocationFrom(calling_convention.GetRegisterAt(0)),
                          DataType::Type::kReference,
                          nullptr);
    parallel_move.AddMove(obj_,
                          LocationFrom(calling_convention.GetRegisterAt(1)),
                          DataType::Type::kReference,
                          nullptr);
    if (index.IsValid()) {
      parallel_move.AddMove(index,
                            LocationFrom(calling_convention.GetRegisterAt(2)),
                            DataType::Type::kInt32,
                            nullptr);
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
    } else {
      codegen->GetMoveResolver()->EmitNativeCode(&parallel_move);
      __ Mov(calling_convention.GetRegisterAt(2), offset_);
    }
    arm_codegen->InvokeRuntime(kQuickReadBarrierSlow, instruction_, instruction_->GetDexPc(), this);
    CheckEntrypointTypes<
        kQuickReadBarrierSlow, mirror::Object*, mirror::Object*, mirror::Object*, uint32_t>();
    arm_codegen->Move32(out_, LocationFrom(r0));

    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE {
    return "ReadBarrierForHeapReferenceSlowPathARMVIXL";
  }

 private:
  vixl32::Register FindAvailableCallerSaveRegister(CodeGenerator* codegen) {
    uint32_t ref = RegisterFrom(ref_).GetCode();
    uint32_t obj = RegisterFrom(obj_).GetCode();
    for (uint32_t i = 0, e = codegen->GetNumberOfCoreRegisters(); i < e; ++i) {
      if (i != ref && i != obj && !codegen->IsCoreCalleeSaveRegister(i)) {
        return vixl32::Register(i);
      }
    }
    // We shall never fail to find a free caller-save register, as
    // there are more than two core caller-save registers on ARM
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

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForHeapReferenceSlowPathARMVIXL);
};

// Slow path generating a read barrier for a GC root.
class ReadBarrierForRootSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  ReadBarrierForRootSlowPathARMVIXL(HInstruction* instruction, Location out, Location root)
      : SlowPathCodeARMVIXL(instruction), out_(out), root_(root) {
    DCHECK(kEmitCompilerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    LocationSummary* locations = instruction_->GetLocations();
    vixl32::Register reg_out = RegisterFrom(out_);
    DCHECK(locations->CanCall());
    DCHECK(!locations->GetLiveRegisters()->ContainsCoreRegister(reg_out.GetCode()));
    DCHECK(instruction_->IsLoadClass() || instruction_->IsLoadString())
        << "Unexpected instruction in read barrier for GC root slow path: "
        << instruction_->DebugName();

    __ Bind(GetEntryLabel());
    SaveLiveRegisters(codegen, locations);

    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    arm_codegen->Move32(LocationFrom(calling_convention.GetRegisterAt(0)), root_);
    arm_codegen->InvokeRuntime(kQuickReadBarrierForRootSlow,
                               instruction_,
                               instruction_->GetDexPc(),
                               this);
    CheckEntrypointTypes<kQuickReadBarrierForRootSlow, mirror::Object*, GcRoot<mirror::Object>*>();
    arm_codegen->Move32(out_, LocationFrom(r0));

    RestoreLiveRegisters(codegen, locations);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierForRootSlowPathARMVIXL"; }

 private:
  const Location out_;
  const Location root_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierForRootSlowPathARMVIXL);
};

inline vixl32::Condition ARMCondition(IfCondition cond) {
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

// Maps signed condition to unsigned condition.
inline vixl32::Condition ARMUnsignedCondition(IfCondition cond) {
  switch (cond) {
    case kCondEQ: return eq;
    case kCondNE: return ne;
    // Signed to unsigned.
    case kCondLT: return lo;
    case kCondLE: return ls;
    case kCondGT: return hi;
    case kCondGE: return hs;
    // Unsigned remain unchanged.
    case kCondB:  return lo;
    case kCondBE: return ls;
    case kCondA:  return hi;
    case kCondAE: return hs;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

inline vixl32::Condition ARMFPCondition(IfCondition cond, bool gt_bias) {
  // The ARM condition codes can express all the necessary branches, see the
  // "Meaning (floating-point)" column in the table A8-1 of the ARMv7 reference manual.
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

inline ShiftType ShiftFromOpKind(HDataProcWithShifterOp::OpKind op_kind) {
  switch (op_kind) {
    case HDataProcWithShifterOp::kASR: return ShiftType::ASR;
    case HDataProcWithShifterOp::kLSL: return ShiftType::LSL;
    case HDataProcWithShifterOp::kLSR: return ShiftType::LSR;
    default:
      LOG(FATAL) << "Unexpected op kind " << op_kind;
      UNREACHABLE();
  }
}

void CodeGeneratorARMVIXL::DumpCoreRegister(std::ostream& stream, int reg) const {
  stream << vixl32::Register(reg);
}

void CodeGeneratorARMVIXL::DumpFloatingPointRegister(std::ostream& stream, int reg) const {
  stream << vixl32::SRegister(reg);
}

static uint32_t ComputeSRegisterListMask(const SRegisterList& regs) {
  uint32_t mask = 0;
  for (uint32_t i = regs.GetFirstSRegister().GetCode();
       i <= regs.GetLastSRegister().GetCode();
       ++i) {
    mask |= (1 << i);
  }
  return mask;
}

// Saves the register in the stack. Returns the size taken on stack.
size_t CodeGeneratorARMVIXL::SaveCoreRegister(size_t stack_index ATTRIBUTE_UNUSED,
                                              uint32_t reg_id ATTRIBUTE_UNUSED) {
  TODO_VIXL32(FATAL);
  return 0;
}

// Restores the register from the stack. Returns the size taken on stack.
size_t CodeGeneratorARMVIXL::RestoreCoreRegister(size_t stack_index ATTRIBUTE_UNUSED,
                                                 uint32_t reg_id ATTRIBUTE_UNUSED) {
  TODO_VIXL32(FATAL);
  return 0;
}

size_t CodeGeneratorARMVIXL::SaveFloatingPointRegister(size_t stack_index ATTRIBUTE_UNUSED,
                                                       uint32_t reg_id ATTRIBUTE_UNUSED) {
  TODO_VIXL32(FATAL);
  return 0;
}

size_t CodeGeneratorARMVIXL::RestoreFloatingPointRegister(size_t stack_index ATTRIBUTE_UNUSED,
                                                          uint32_t reg_id ATTRIBUTE_UNUSED) {
  TODO_VIXL32(FATAL);
  return 0;
}

static void GenerateDataProcInstruction(HInstruction::InstructionKind kind,
                                        vixl32::Register out,
                                        vixl32::Register first,
                                        const Operand& second,
                                        CodeGeneratorARMVIXL* codegen) {
  if (second.IsImmediate() && second.GetImmediate() == 0) {
    const Operand in = kind == HInstruction::kAnd
        ? Operand(0)
        : Operand(first);

    __ Mov(out, in);
  } else {
    switch (kind) {
      case HInstruction::kAdd:
        __ Add(out, first, second);
        break;
      case HInstruction::kAnd:
        __ And(out, first, second);
        break;
      case HInstruction::kOr:
        __ Orr(out, first, second);
        break;
      case HInstruction::kSub:
        __ Sub(out, first, second);
        break;
      case HInstruction::kXor:
        __ Eor(out, first, second);
        break;
      default:
        LOG(FATAL) << "Unexpected instruction kind: " << kind;
        UNREACHABLE();
    }
  }
}

static void GenerateDataProc(HInstruction::InstructionKind kind,
                             const Location& out,
                             const Location& first,
                             const Operand& second_lo,
                             const Operand& second_hi,
                             CodeGeneratorARMVIXL* codegen) {
  const vixl32::Register first_hi = HighRegisterFrom(first);
  const vixl32::Register first_lo = LowRegisterFrom(first);
  const vixl32::Register out_hi = HighRegisterFrom(out);
  const vixl32::Register out_lo = LowRegisterFrom(out);

  if (kind == HInstruction::kAdd) {
    __ Adds(out_lo, first_lo, second_lo);
    __ Adc(out_hi, first_hi, second_hi);
  } else if (kind == HInstruction::kSub) {
    __ Subs(out_lo, first_lo, second_lo);
    __ Sbc(out_hi, first_hi, second_hi);
  } else {
    GenerateDataProcInstruction(kind, out_lo, first_lo, second_lo, codegen);
    GenerateDataProcInstruction(kind, out_hi, first_hi, second_hi, codegen);
  }
}

static Operand GetShifterOperand(vixl32::Register rm, ShiftType shift, uint32_t shift_imm) {
  return shift_imm == 0 ? Operand(rm) : Operand(rm, shift, shift_imm);
}

static void GenerateLongDataProc(HDataProcWithShifterOp* instruction,
                                 CodeGeneratorARMVIXL* codegen) {
  DCHECK_EQ(instruction->GetType(), DataType::Type::kInt64);
  DCHECK(HDataProcWithShifterOp::IsShiftOp(instruction->GetOpKind()));

  const LocationSummary* const locations = instruction->GetLocations();
  const uint32_t shift_value = instruction->GetShiftAmount();
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();
  const Location first = locations->InAt(0);
  const Location second = locations->InAt(1);
  const Location out = locations->Out();
  const vixl32::Register first_hi = HighRegisterFrom(first);
  const vixl32::Register first_lo = LowRegisterFrom(first);
  const vixl32::Register out_hi = HighRegisterFrom(out);
  const vixl32::Register out_lo = LowRegisterFrom(out);
  const vixl32::Register second_hi = HighRegisterFrom(second);
  const vixl32::Register second_lo = LowRegisterFrom(second);
  const ShiftType shift = ShiftFromOpKind(instruction->GetOpKind());

  if (shift_value >= 32) {
    if (shift == ShiftType::LSL) {
      GenerateDataProcInstruction(kind,
                                  out_hi,
                                  first_hi,
                                  Operand(second_lo, ShiftType::LSL, shift_value - 32),
                                  codegen);
      GenerateDataProcInstruction(kind, out_lo, first_lo, 0, codegen);
    } else if (shift == ShiftType::ASR) {
      GenerateDataProc(kind,
                       out,
                       first,
                       GetShifterOperand(second_hi, ShiftType::ASR, shift_value - 32),
                       Operand(second_hi, ShiftType::ASR, 31),
                       codegen);
    } else {
      DCHECK_EQ(shift, ShiftType::LSR);
      GenerateDataProc(kind,
                       out,
                       first,
                       GetShifterOperand(second_hi, ShiftType::LSR, shift_value - 32),
                       0,
                       codegen);
    }
  } else {
    DCHECK_GT(shift_value, 1U);
    DCHECK_LT(shift_value, 32U);

    UseScratchRegisterScope temps(codegen->GetVIXLAssembler());

    if (shift == ShiftType::LSL) {
      // We are not doing this for HInstruction::kAdd because the output will require
      // Location::kOutputOverlap; not applicable to other cases.
      if (kind == HInstruction::kOr || kind == HInstruction::kXor) {
        GenerateDataProcInstruction(kind,
                                    out_hi,
                                    first_hi,
                                    Operand(second_hi, ShiftType::LSL, shift_value),
                                    codegen);
        GenerateDataProcInstruction(kind,
                                    out_hi,
                                    out_hi,
                                    Operand(second_lo, ShiftType::LSR, 32 - shift_value),
                                    codegen);
        GenerateDataProcInstruction(kind,
                                    out_lo,
                                    first_lo,
                                    Operand(second_lo, ShiftType::LSL, shift_value),
                                    codegen);
      } else {
        const vixl32::Register temp = temps.Acquire();

        __ Lsl(temp, second_hi, shift_value);
        __ Orr(temp, temp, Operand(second_lo, ShiftType::LSR, 32 - shift_value));
        GenerateDataProc(kind,
                         out,
                         first,
                         Operand(second_lo, ShiftType::LSL, shift_value),
                         temp,
                         codegen);
      }
    } else {
      DCHECK(shift == ShiftType::ASR || shift == ShiftType::LSR);

      // We are not doing this for HInstruction::kAdd because the output will require
      // Location::kOutputOverlap; not applicable to other cases.
      if (kind == HInstruction::kOr || kind == HInstruction::kXor) {
        GenerateDataProcInstruction(kind,
                                    out_lo,
                                    first_lo,
                                    Operand(second_lo, ShiftType::LSR, shift_value),
                                    codegen);
        GenerateDataProcInstruction(kind,
                                    out_lo,
                                    out_lo,
                                    Operand(second_hi, ShiftType::LSL, 32 - shift_value),
                                    codegen);
        GenerateDataProcInstruction(kind,
                                    out_hi,
                                    first_hi,
                                    Operand(second_hi, shift, shift_value),
                                    codegen);
      } else {
        const vixl32::Register temp = temps.Acquire();

        __ Lsr(temp, second_lo, shift_value);
        __ Orr(temp, temp, Operand(second_hi, ShiftType::LSL, 32 - shift_value));
        GenerateDataProc(kind,
                         out,
                         first,
                         temp,
                         Operand(second_hi, shift, shift_value),
                         codegen);
      }
    }
  }
}

static void GenerateVcmp(HInstruction* instruction, CodeGeneratorARMVIXL* codegen) {
  const Location rhs_loc = instruction->GetLocations()->InAt(1);
  if (rhs_loc.IsConstant()) {
    // 0.0 is the only immediate that can be encoded directly in
    // a VCMP instruction.
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
    DCHECK(rhs_loc.GetConstant()->IsArithmeticZero());

    const DataType::Type type = instruction->InputAt(0)->GetType();

    if (type == DataType::Type::kFloat32) {
      __ Vcmp(F32, InputSRegisterAt(instruction, 0), 0.0);
    } else {
      DCHECK_EQ(type, DataType::Type::kFloat64);
      __ Vcmp(F64, InputDRegisterAt(instruction, 0), 0.0);
    }
  } else {
    __ Vcmp(InputVRegisterAt(instruction, 0), InputVRegisterAt(instruction, 1));
  }
}

static int64_t AdjustConstantForCondition(int64_t value,
                                          IfCondition* condition,
                                          IfCondition* opposite) {
  if (value == 1) {
    if (*condition == kCondB) {
      value = 0;
      *condition = kCondEQ;
      *opposite = kCondNE;
    } else if (*condition == kCondAE) {
      value = 0;
      *condition = kCondNE;
      *opposite = kCondEQ;
    }
  } else if (value == -1) {
    if (*condition == kCondGT) {
      value = 0;
      *condition = kCondGE;
      *opposite = kCondLT;
    } else if (*condition == kCondLE) {
      value = 0;
      *condition = kCondLT;
      *opposite = kCondGE;
    }
  }

  return value;
}

static std::pair<vixl32::Condition, vixl32::Condition> GenerateLongTestConstant(
    HCondition* condition,
    bool invert,
    CodeGeneratorARMVIXL* codegen) {
  DCHECK_EQ(condition->GetLeft()->GetType(), DataType::Type::kInt64);

  const LocationSummary* const locations = condition->GetLocations();
  IfCondition cond = condition->GetCondition();
  IfCondition opposite = condition->GetOppositeCondition();

  if (invert) {
    std::swap(cond, opposite);
  }

  std::pair<vixl32::Condition, vixl32::Condition> ret(eq, ne);
  const Location left = locations->InAt(0);
  const Location right = locations->InAt(1);

  DCHECK(right.IsConstant());

  const vixl32::Register left_high = HighRegisterFrom(left);
  const vixl32::Register left_low = LowRegisterFrom(left);
  int64_t value = AdjustConstantForCondition(Int64ConstantFrom(right), &cond, &opposite);
  UseScratchRegisterScope temps(codegen->GetVIXLAssembler());

  // Comparisons against 0 are common enough to deserve special attention.
  if (value == 0) {
    switch (cond) {
      case kCondNE:
      // x > 0 iff x != 0 when the comparison is unsigned.
      case kCondA:
        ret = std::make_pair(ne, eq);
        FALLTHROUGH_INTENDED;
      case kCondEQ:
      // x <= 0 iff x == 0 when the comparison is unsigned.
      case kCondBE:
        __ Orrs(temps.Acquire(), left_low, left_high);
        return ret;
      case kCondLT:
      case kCondGE:
        __ Cmp(left_high, 0);
        return std::make_pair(ARMCondition(cond), ARMCondition(opposite));
      // Trivially true or false.
      case kCondB:
        ret = std::make_pair(ne, eq);
        FALLTHROUGH_INTENDED;
      case kCondAE:
        __ Cmp(left_low, left_low);
        return ret;
      default:
        break;
    }
  }

  switch (cond) {
    case kCondEQ:
    case kCondNE:
    case kCondB:
    case kCondBE:
    case kCondA:
    case kCondAE: {
      const uint32_t value_low = Low32Bits(value);
      Operand operand_low(value_low);

      __ Cmp(left_high, High32Bits(value));

      // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
      // we must ensure that the operands corresponding to the least significant
      // halves of the inputs fit into a 16-bit CMP encoding.
      if (!left_low.IsLow() || !IsUint<8>(value_low)) {
        operand_low = Operand(temps.Acquire());
        __ Mov(LeaveFlags, operand_low.GetBaseRegister(), value_low);
      }

      // We use the scope because of the IT block that follows.
      ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                               2 * vixl32::k16BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kExactSize);

      __ it(eq);
      __ cmp(eq, left_low, operand_low);
      ret = std::make_pair(ARMUnsignedCondition(cond), ARMUnsignedCondition(opposite));
      break;
    }
    case kCondLE:
    case kCondGT:
      // Trivially true or false.
      if (value == std::numeric_limits<int64_t>::max()) {
        __ Cmp(left_low, left_low);
        ret = cond == kCondLE ? std::make_pair(eq, ne) : std::make_pair(ne, eq);
        break;
      }

      if (cond == kCondLE) {
        DCHECK_EQ(opposite, kCondGT);
        cond = kCondLT;
        opposite = kCondGE;
      } else {
        DCHECK_EQ(cond, kCondGT);
        DCHECK_EQ(opposite, kCondLE);
        cond = kCondGE;
        opposite = kCondLT;
      }

      value++;
      FALLTHROUGH_INTENDED;
    case kCondGE:
    case kCondLT: {
      __ Cmp(left_low, Low32Bits(value));
      __ Sbcs(temps.Acquire(), left_high, High32Bits(value));
      ret = std::make_pair(ARMCondition(cond), ARMCondition(opposite));
      break;
    }
    default:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
  }

  return ret;
}

static std::pair<vixl32::Condition, vixl32::Condition> GenerateLongTest(
    HCondition* condition,
    bool invert,
    CodeGeneratorARMVIXL* codegen) {
  DCHECK_EQ(condition->GetLeft()->GetType(), DataType::Type::kInt64);

  const LocationSummary* const locations = condition->GetLocations();
  IfCondition cond = condition->GetCondition();
  IfCondition opposite = condition->GetOppositeCondition();

  if (invert) {
    std::swap(cond, opposite);
  }

  std::pair<vixl32::Condition, vixl32::Condition> ret(eq, ne);
  Location left = locations->InAt(0);
  Location right = locations->InAt(1);

  DCHECK(right.IsRegisterPair());

  switch (cond) {
    case kCondEQ:
    case kCondNE:
    case kCondB:
    case kCondBE:
    case kCondA:
    case kCondAE: {
      __ Cmp(HighRegisterFrom(left), HighRegisterFrom(right));

      // We use the scope because of the IT block that follows.
      ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                               2 * vixl32::k16BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kExactSize);

      __ it(eq);
      __ cmp(eq, LowRegisterFrom(left), LowRegisterFrom(right));
      ret = std::make_pair(ARMUnsignedCondition(cond), ARMUnsignedCondition(opposite));
      break;
    }
    case kCondLE:
    case kCondGT:
      if (cond == kCondLE) {
        DCHECK_EQ(opposite, kCondGT);
        cond = kCondGE;
        opposite = kCondLT;
      } else {
        DCHECK_EQ(cond, kCondGT);
        DCHECK_EQ(opposite, kCondLE);
        cond = kCondLT;
        opposite = kCondGE;
      }

      std::swap(left, right);
      FALLTHROUGH_INTENDED;
    case kCondGE:
    case kCondLT: {
      UseScratchRegisterScope temps(codegen->GetVIXLAssembler());

      __ Cmp(LowRegisterFrom(left), LowRegisterFrom(right));
      __ Sbcs(temps.Acquire(), HighRegisterFrom(left), HighRegisterFrom(right));
      ret = std::make_pair(ARMCondition(cond), ARMCondition(opposite));
      break;
    }
    default:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
  }

  return ret;
}

static std::pair<vixl32::Condition, vixl32::Condition> GenerateTest(HCondition* condition,
                                                                    bool invert,
                                                                    CodeGeneratorARMVIXL* codegen) {
  const DataType::Type type = condition->GetLeft()->GetType();
  IfCondition cond = condition->GetCondition();
  IfCondition opposite = condition->GetOppositeCondition();
  std::pair<vixl32::Condition, vixl32::Condition> ret(eq, ne);

  if (invert) {
    std::swap(cond, opposite);
  }

  if (type == DataType::Type::kInt64) {
    ret = condition->GetLocations()->InAt(1).IsConstant()
        ? GenerateLongTestConstant(condition, invert, codegen)
        : GenerateLongTest(condition, invert, codegen);
  } else if (DataType::IsFloatingPointType(type)) {
    GenerateVcmp(condition, codegen);
    __ Vmrs(RegisterOrAPSR_nzcv(kPcCode), FPSCR);
    ret = std::make_pair(ARMFPCondition(cond, condition->IsGtBias()),
                         ARMFPCondition(opposite, condition->IsGtBias()));
  } else {
    DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;
    __ Cmp(InputRegisterAt(condition, 0), InputOperandAt(condition, 1));
    ret = std::make_pair(ARMCondition(cond), ARMCondition(opposite));
  }

  return ret;
}

static void GenerateConditionGeneric(HCondition* cond, CodeGeneratorARMVIXL* codegen) {
  const vixl32::Register out = OutputRegister(cond);
  const auto condition = GenerateTest(cond, false, codegen);

  __ Mov(LeaveFlags, out, 0);

  if (out.IsLow()) {
    // We use the scope because of the IT block that follows.
    ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                             2 * vixl32::k16BitT32InstructionSizeInBytes,
                             CodeBufferCheckScope::kExactSize);

    __ it(condition.first);
    __ mov(condition.first, out, 1);
  } else {
    vixl32::Label done_label;
    vixl32::Label* const final_label = codegen->GetFinalLabel(cond, &done_label);

    __ B(condition.second, final_label, /* far_target */ false);
    __ Mov(out, 1);

    if (done_label.IsReferenced()) {
      __ Bind(&done_label);
    }
  }
}

static void GenerateEqualLong(HCondition* cond, CodeGeneratorARMVIXL* codegen) {
  DCHECK_EQ(cond->GetLeft()->GetType(), DataType::Type::kInt64);

  const LocationSummary* const locations = cond->GetLocations();
  IfCondition condition = cond->GetCondition();
  const vixl32::Register out = OutputRegister(cond);
  const Location left = locations->InAt(0);
  const Location right = locations->InAt(1);
  vixl32::Register left_high = HighRegisterFrom(left);
  vixl32::Register left_low = LowRegisterFrom(left);
  vixl32::Register temp;
  UseScratchRegisterScope temps(codegen->GetVIXLAssembler());

  if (right.IsConstant()) {
    IfCondition opposite = cond->GetOppositeCondition();
    const int64_t value = AdjustConstantForCondition(Int64ConstantFrom(right),
                                                     &condition,
                                                     &opposite);
    Operand right_high = High32Bits(value);
    Operand right_low = Low32Bits(value);

    // The output uses Location::kNoOutputOverlap.
    if (out.Is(left_high)) {
      std::swap(left_low, left_high);
      std::swap(right_low, right_high);
    }

    __ Sub(out, left_low, right_low);
    temp = temps.Acquire();
    __ Sub(temp, left_high, right_high);
  } else {
    DCHECK(right.IsRegisterPair());
    temp = temps.Acquire();
    __ Sub(temp, left_high, HighRegisterFrom(right));
    __ Sub(out, left_low, LowRegisterFrom(right));
  }

  // Need to check after calling AdjustConstantForCondition().
  DCHECK(condition == kCondEQ || condition == kCondNE) << condition;

  if (condition == kCondNE && out.IsLow()) {
    __ Orrs(out, out, temp);

    // We use the scope because of the IT block that follows.
    ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                             2 * vixl32::k16BitT32InstructionSizeInBytes,
                             CodeBufferCheckScope::kExactSize);

    __ it(ne);
    __ mov(ne, out, 1);
  } else {
    __ Orr(out, out, temp);
    codegen->GenerateConditionWithZero(condition, out, out, temp);
  }
}

static void GenerateConditionLong(HCondition* cond, CodeGeneratorARMVIXL* codegen) {
  DCHECK_EQ(cond->GetLeft()->GetType(), DataType::Type::kInt64);

  const LocationSummary* const locations = cond->GetLocations();
  IfCondition condition = cond->GetCondition();
  const vixl32::Register out = OutputRegister(cond);
  const Location left = locations->InAt(0);
  const Location right = locations->InAt(1);

  if (right.IsConstant()) {
    IfCondition opposite = cond->GetOppositeCondition();

    // Comparisons against 0 are common enough to deserve special attention.
    if (AdjustConstantForCondition(Int64ConstantFrom(right), &condition, &opposite) == 0) {
      switch (condition) {
        case kCondNE:
        case kCondA:
          if (out.IsLow()) {
            // We only care if both input registers are 0 or not.
            __ Orrs(out, LowRegisterFrom(left), HighRegisterFrom(left));

            // We use the scope because of the IT block that follows.
            ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                                     2 * vixl32::k16BitT32InstructionSizeInBytes,
                                     CodeBufferCheckScope::kExactSize);

            __ it(ne);
            __ mov(ne, out, 1);
            return;
          }

          FALLTHROUGH_INTENDED;
        case kCondEQ:
        case kCondBE:
          // We only care if both input registers are 0 or not.
          __ Orr(out, LowRegisterFrom(left), HighRegisterFrom(left));
          codegen->GenerateConditionWithZero(condition, out, out);
          return;
        case kCondLT:
        case kCondGE:
          // We only care about the sign bit.
          FALLTHROUGH_INTENDED;
        case kCondAE:
        case kCondB:
          codegen->GenerateConditionWithZero(condition, out, HighRegisterFrom(left));
          return;
        case kCondLE:
        case kCondGT:
        default:
          break;
      }
    }
  }

  // If `out` is a low register, then the GenerateConditionGeneric()
  // function generates a shorter code sequence that is still branchless.
  if ((condition == kCondEQ || condition == kCondNE) && !out.IsLow()) {
    GenerateEqualLong(cond, codegen);
    return;
  }

  GenerateConditionGeneric(cond, codegen);
}

static void GenerateConditionIntegralOrNonPrimitive(HCondition* cond,
                                                    CodeGeneratorARMVIXL* codegen) {
  const DataType::Type type = cond->GetLeft()->GetType();

  DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;

  if (type == DataType::Type::kInt64) {
    GenerateConditionLong(cond, codegen);
    return;
  }

  IfCondition condition = cond->GetCondition();
  vixl32::Register in = InputRegisterAt(cond, 0);
  const vixl32::Register out = OutputRegister(cond);
  const Location right = cond->GetLocations()->InAt(1);
  int64_t value;

  if (right.IsConstant()) {
    IfCondition opposite = cond->GetOppositeCondition();

    value = AdjustConstantForCondition(Int64ConstantFrom(right), &condition, &opposite);

    // Comparisons against 0 are common enough to deserve special attention.
    if (value == 0) {
      switch (condition) {
        case kCondNE:
        case kCondA:
          if (out.IsLow() && out.Is(in)) {
            __ Cmp(out, 0);

            // We use the scope because of the IT block that follows.
            ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                                     2 * vixl32::k16BitT32InstructionSizeInBytes,
                                     CodeBufferCheckScope::kExactSize);

            __ it(ne);
            __ mov(ne, out, 1);
            return;
          }

          FALLTHROUGH_INTENDED;
        case kCondEQ:
        case kCondBE:
        case kCondLT:
        case kCondGE:
        case kCondAE:
        case kCondB:
          codegen->GenerateConditionWithZero(condition, out, in);
          return;
        case kCondLE:
        case kCondGT:
        default:
          break;
      }
    }
  }

  if (condition == kCondEQ || condition == kCondNE) {
    Operand operand(0);

    if (right.IsConstant()) {
      operand = Operand::From(value);
    } else if (out.Is(RegisterFrom(right))) {
      // Avoid 32-bit instructions if possible.
      operand = InputOperandAt(cond, 0);
      in = RegisterFrom(right);
    } else {
      operand = InputOperandAt(cond, 1);
    }

    if (condition == kCondNE && out.IsLow()) {
      __ Subs(out, in, operand);

      // We use the scope because of the IT block that follows.
      ExactAssemblyScope guard(codegen->GetVIXLAssembler(),
                               2 * vixl32::k16BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kExactSize);

      __ it(ne);
      __ mov(ne, out, 1);
    } else {
      __ Sub(out, in, operand);
      codegen->GenerateConditionWithZero(condition, out, out);
    }

    return;
  }

  GenerateConditionGeneric(cond, codegen);
}

static bool CanEncodeConstantAs8BitImmediate(HConstant* constant) {
  const DataType::Type type = constant->GetType();
  bool ret = false;

  DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;

  if (type == DataType::Type::kInt64) {
    const uint64_t value = Uint64ConstantFrom(constant);

    ret = IsUint<8>(Low32Bits(value)) && IsUint<8>(High32Bits(value));
  } else {
    ret = IsUint<8>(Int32ConstantFrom(constant));
  }

  return ret;
}

static Location Arm8BitEncodableConstantOrRegister(HInstruction* constant) {
  DCHECK(!DataType::IsFloatingPointType(constant->GetType()));

  if (constant->IsConstant() && CanEncodeConstantAs8BitImmediate(constant->AsConstant())) {
    return Location::ConstantLocation(constant->AsConstant());
  }

  return Location::RequiresRegister();
}

static bool CanGenerateConditionalMove(const Location& out, const Location& src) {
  // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
  // we check that we are not dealing with floating-point output (there is no
  // 16-bit VMOV encoding).
  if (!out.IsRegister() && !out.IsRegisterPair()) {
    return false;
  }

  // For constants, we also check that the output is in one or two low registers,
  // and that the constants fit in an 8-bit unsigned integer, so that a 16-bit
  // MOV encoding can be used.
  if (src.IsConstant()) {
    if (!CanEncodeConstantAs8BitImmediate(src.GetConstant())) {
      return false;
    }

    if (out.IsRegister()) {
      if (!RegisterFrom(out).IsLow()) {
        return false;
      }
    } else {
      DCHECK(out.IsRegisterPair());

      if (!HighRegisterFrom(out).IsLow()) {
        return false;
      }
    }
  }

  return true;
}

#undef __

vixl32::Label* CodeGeneratorARMVIXL::GetFinalLabel(HInstruction* instruction,
                                                   vixl32::Label* final_label) {
  DCHECK(!instruction->IsControlFlow() && !instruction->IsSuspendCheck());
  DCHECK(!instruction->IsInvoke() || !instruction->GetLocations()->CanCall());

  const HBasicBlock* const block = instruction->GetBlock();
  const HLoopInformation* const info = block->GetLoopInformation();
  HInstruction* const next = instruction->GetNext();

  // Avoid a branch to a branch.
  if (next->IsGoto() && (info == nullptr ||
                         !info->IsBackEdge(*block) ||
                         !info->HasSuspendCheck())) {
    final_label = GetLabelOf(next->AsGoto()->GetSuccessor());
  }

  return final_label;
}

CodeGeneratorARMVIXL::CodeGeneratorARMVIXL(HGraph* graph,
                                           const ArmInstructionSetFeatures& isa_features,
                                           const CompilerOptions& compiler_options,
                                           OptimizingCompilerStats* stats)
    : CodeGenerator(graph,
                    kNumberOfCoreRegisters,
                    kNumberOfSRegisters,
                    kNumberOfRegisterPairs,
                    kCoreCalleeSaves.GetList(),
                    ComputeSRegisterListMask(kFpuCalleeSaves),
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
  // Always save the LR register to mimic Quick.
  AddAllocatedRegister(Location::RegisterLocation(LR));
  // Give D30 and D31 as scratch register to VIXL. The register allocator only works on
  // S0-S31, which alias to D0-D15.
  GetVIXLAssembler()->GetScratchVRegisterList()->Combine(d31);
  GetVIXLAssembler()->GetScratchVRegisterList()->Combine(d30);
}

void JumpTableARMVIXL::EmitTable(CodeGeneratorARMVIXL* codegen) {
  uint32_t num_entries = switch_instr_->GetNumEntries();
  DCHECK_GE(num_entries, kPackedSwitchCompareJumpThreshold);

  // We are about to use the assembler to place literals directly. Make sure we have enough
  // underlying code buffer and we have generated a jump table of the right size, using
  // codegen->GetVIXLAssembler()->GetBuffer().Align();
  ExactAssemblyScope aas(codegen->GetVIXLAssembler(),
                         num_entries * sizeof(int32_t),
                         CodeBufferCheckScope::kMaximumSize);
  // TODO(VIXL): Check that using lower case bind is fine here.
  codegen->GetVIXLAssembler()->bind(&table_start_);
  for (uint32_t i = 0; i < num_entries; i++) {
    codegen->GetVIXLAssembler()->place(bb_addresses_[i].get());
  }
}

void JumpTableARMVIXL::FixTable(CodeGeneratorARMVIXL* codegen) {
  uint32_t num_entries = switch_instr_->GetNumEntries();
  DCHECK_GE(num_entries, kPackedSwitchCompareJumpThreshold);

  const ArenaVector<HBasicBlock*>& successors = switch_instr_->GetBlock()->GetSuccessors();
  for (uint32_t i = 0; i < num_entries; i++) {
    vixl32::Label* target_label = codegen->GetLabelOf(successors[i]);
    DCHECK(target_label->IsBound());
    int32_t jump_offset = target_label->GetLocation() - table_start_.GetLocation();
    // When doing BX to address we need to have lower bit set to 1 in T32.
    if (codegen->GetVIXLAssembler()->IsUsingT32()) {
      jump_offset++;
    }
    DCHECK_GT(jump_offset, std::numeric_limits<int32_t>::min());
    DCHECK_LE(jump_offset, std::numeric_limits<int32_t>::max());

    bb_addresses_[i].get()->UpdateValue(jump_offset, codegen->GetVIXLAssembler()->GetBuffer());
  }
}

void CodeGeneratorARMVIXL::FixJumpTables() {
  for (auto&& jump_table : jump_tables_) {
    jump_table->FixTable(this);
  }
}

#define __ reinterpret_cast<ArmVIXLAssembler*>(GetAssembler())->GetVIXLAssembler()->  // NOLINT

void CodeGeneratorARMVIXL::Finalize(CodeAllocator* allocator) {
  FixJumpTables();
  GetAssembler()->FinalizeCode();
  CodeGenerator::Finalize(allocator);
}

void CodeGeneratorARMVIXL::SetupBlockedRegisters() const {
  // Stack register, LR and PC are always reserved.
  blocked_core_registers_[SP] = true;
  blocked_core_registers_[LR] = true;
  blocked_core_registers_[PC] = true;

  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // Reserve marking register.
    blocked_core_registers_[MR] = true;
  }

  // Reserve thread register.
  blocked_core_registers_[TR] = true;

  // Reserve temp register.
  blocked_core_registers_[IP] = true;

  if (GetGraph()->IsDebuggable()) {
    // Stubs do not save callee-save floating point registers. If the graph
    // is debuggable, we need to deal with these registers differently. For
    // now, just block them.
    for (uint32_t i = kFpuCalleeSaves.GetFirstSRegister().GetCode();
         i <= kFpuCalleeSaves.GetLastSRegister().GetCode();
         ++i) {
      blocked_fpu_registers_[i] = true;
    }
  }
}

InstructionCodeGeneratorARMVIXL::InstructionCodeGeneratorARMVIXL(HGraph* graph,
                                                                 CodeGeneratorARMVIXL* codegen)
      : InstructionCodeGenerator(graph, codegen),
        assembler_(codegen->GetAssembler()),
        codegen_(codegen) {}

void CodeGeneratorARMVIXL::ComputeSpillMask() {
  core_spill_mask_ = allocated_registers_.GetCoreRegisters() & core_callee_save_mask_;
  DCHECK_NE(core_spill_mask_, 0u) << "At least the return address register must be saved";
  // There is no easy instruction to restore just the PC on thumb2. We spill and
  // restore another arbitrary register.
  core_spill_mask_ |= (1 << kCoreAlwaysSpillRegister.GetCode());
  fpu_spill_mask_ = allocated_registers_.GetFloatingPointRegisters() & fpu_callee_save_mask_;
  // We use vpush and vpop for saving and restoring floating point registers, which take
  // a SRegister and the number of registers to save/restore after that SRegister. We
  // therefore update the `fpu_spill_mask_` to also contain those registers not allocated,
  // but in the range.
  if (fpu_spill_mask_ != 0) {
    uint32_t least_significant_bit = LeastSignificantBit(fpu_spill_mask_);
    uint32_t most_significant_bit = MostSignificantBit(fpu_spill_mask_);
    for (uint32_t i = least_significant_bit + 1 ; i < most_significant_bit; ++i) {
      fpu_spill_mask_ |= (1 << i);
    }
  }
}

void CodeGeneratorARMVIXL::GenerateFrameEntry() {
  bool skip_overflow_check =
      IsLeafMethod() && !FrameNeedsStackCheck(GetFrameSize(), InstructionSet::kArm);
  DCHECK(GetCompilerOptions().GetImplicitStackOverflowChecks());
  __ Bind(&frame_entry_label_);

  if (GetCompilerOptions().CountHotnessInCompiledCode()) {
    UseScratchRegisterScope temps(GetVIXLAssembler());
    vixl32::Register temp = temps.Acquire();
    __ Ldrh(temp, MemOperand(kMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
    __ Add(temp, temp, 1);
    __ Strh(temp, MemOperand(kMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
  }

  if (HasEmptyFrame()) {
    return;
  }

  if (!skip_overflow_check) {
    // Using r4 instead of IP saves 2 bytes.
    UseScratchRegisterScope temps(GetVIXLAssembler());
    vixl32::Register temp;
    // TODO: Remove this check when R4 is made a callee-save register
    // in ART compiled code (b/72801708). Currently we need to make
    // sure r4 is not blocked, e.g. in special purpose
    // TestCodeGeneratorARMVIXL; also asserting that r4 is available
    // here.
    if (!blocked_core_registers_[R4]) {
      for (vixl32::Register reg : kParameterCoreRegistersVIXL) {
        DCHECK(!reg.Is(r4));
      }
      DCHECK(!kCoreCalleeSaves.Includes(r4));
      temp = r4;
    } else {
      temp = temps.Acquire();
    }
    __ Sub(temp, sp, Operand::From(GetStackOverflowReservedBytes(InstructionSet::kArm)));
    // The load must immediately precede RecordPcInfo.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    __ ldr(temp, MemOperand(temp));
    RecordPcInfo(nullptr, 0);
  }

  __ Push(RegisterList(core_spill_mask_));
  GetAssembler()->cfi().AdjustCFAOffset(kArmWordSize * POPCOUNT(core_spill_mask_));
  GetAssembler()->cfi().RelOffsetForMany(DWARFReg(kMethodRegister),
                                         0,
                                         core_spill_mask_,
                                         kArmWordSize);
  if (fpu_spill_mask_ != 0) {
    uint32_t first = LeastSignificantBit(fpu_spill_mask_);

    // Check that list is contiguous.
    DCHECK_EQ(fpu_spill_mask_ >> CTZ(fpu_spill_mask_), ~0u >> (32 - POPCOUNT(fpu_spill_mask_)));

    __ Vpush(SRegisterList(vixl32::SRegister(first), POPCOUNT(fpu_spill_mask_)));
    GetAssembler()->cfi().AdjustCFAOffset(kArmWordSize * POPCOUNT(fpu_spill_mask_));
    GetAssembler()->cfi().RelOffsetForMany(DWARFReg(s0), 0, fpu_spill_mask_, kArmWordSize);
  }

  int adjust = GetFrameSize() - FrameEntrySpillSize();
  __ Sub(sp, sp, adjust);
  GetAssembler()->cfi().AdjustCFAOffset(adjust);

  // Save the current method if we need it. Note that we do not
  // do this in HCurrentMethod, as the instruction might have been removed
  // in the SSA graph.
  if (RequiresCurrentMethod()) {
    GetAssembler()->StoreToOffset(kStoreWord, kMethodRegister, sp, 0);
  }

  if (GetGraph()->HasShouldDeoptimizeFlag()) {
    UseScratchRegisterScope temps(GetVIXLAssembler());
    vixl32::Register temp = temps.Acquire();
    // Initialize should_deoptimize flag to 0.
    __ Mov(temp, 0);
    GetAssembler()->StoreToOffset(kStoreWord, temp, sp, GetStackOffsetOfShouldDeoptimizeFlag());
  }

  MaybeGenerateMarkingRegisterCheck(/* code */ 1);
}

void CodeGeneratorARMVIXL::GenerateFrameExit() {
  if (HasEmptyFrame()) {
    __ Bx(lr);
    return;
  }
  GetAssembler()->cfi().RememberState();
  int adjust = GetFrameSize() - FrameEntrySpillSize();
  __ Add(sp, sp, adjust);
  GetAssembler()->cfi().AdjustCFAOffset(-adjust);
  if (fpu_spill_mask_ != 0) {
    uint32_t first = LeastSignificantBit(fpu_spill_mask_);

    // Check that list is contiguous.
    DCHECK_EQ(fpu_spill_mask_ >> CTZ(fpu_spill_mask_), ~0u >> (32 - POPCOUNT(fpu_spill_mask_)));

    __ Vpop(SRegisterList(vixl32::SRegister(first), POPCOUNT(fpu_spill_mask_)));
    GetAssembler()->cfi().AdjustCFAOffset(
        -static_cast<int>(kArmWordSize) * POPCOUNT(fpu_spill_mask_));
    GetAssembler()->cfi().RestoreMany(DWARFReg(vixl32::SRegister(0)), fpu_spill_mask_);
  }
  // Pop LR into PC to return.
  DCHECK_NE(core_spill_mask_ & (1 << kLrCode), 0U);
  uint32_t pop_mask = (core_spill_mask_ & (~(1 << kLrCode))) | 1 << kPcCode;
  __ Pop(RegisterList(pop_mask));
  GetAssembler()->cfi().RestoreState();
  GetAssembler()->cfi().DefCFAOffset(GetFrameSize());
}

void CodeGeneratorARMVIXL::Bind(HBasicBlock* block) {
  __ Bind(GetLabelOf(block));
}

Location InvokeDexCallingConventionVisitorARMVIXL::GetNextLocation(DataType::Type type) {
  switch (type) {
    case DataType::Type::kReference:
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      uint32_t index = gp_index_++;
      uint32_t stack_index = stack_index_++;
      if (index < calling_convention.GetNumberOfRegisters()) {
        return LocationFrom(calling_convention.GetRegisterAt(index));
      } else {
        return Location::StackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case DataType::Type::kInt64: {
      uint32_t index = gp_index_;
      uint32_t stack_index = stack_index_;
      gp_index_ += 2;
      stack_index_ += 2;
      if (index + 1 < calling_convention.GetNumberOfRegisters()) {
        if (calling_convention.GetRegisterAt(index).Is(r1)) {
          // Skip R1, and use R2_R3 instead.
          gp_index_++;
          index++;
        }
      }
      if (index + 1 < calling_convention.GetNumberOfRegisters()) {
        DCHECK_EQ(calling_convention.GetRegisterAt(index).GetCode() + 1,
                  calling_convention.GetRegisterAt(index + 1).GetCode());

        return LocationFrom(calling_convention.GetRegisterAt(index),
                            calling_convention.GetRegisterAt(index + 1));
      } else {
        return Location::DoubleStackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case DataType::Type::kFloat32: {
      uint32_t stack_index = stack_index_++;
      if (float_index_ % 2 == 0) {
        float_index_ = std::max(double_index_, float_index_);
      }
      if (float_index_ < calling_convention.GetNumberOfFpuRegisters()) {
        return LocationFrom(calling_convention.GetFpuRegisterAt(float_index_++));
      } else {
        return Location::StackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case DataType::Type::kFloat64: {
      double_index_ = std::max(double_index_, RoundUp(float_index_, 2));
      uint32_t stack_index = stack_index_;
      stack_index_ += 2;
      if (double_index_ + 1 < calling_convention.GetNumberOfFpuRegisters()) {
        uint32_t index = double_index_;
        double_index_ += 2;
        Location result = LocationFrom(
          calling_convention.GetFpuRegisterAt(index),
          calling_convention.GetFpuRegisterAt(index + 1));
        DCHECK(ExpectedPairLayout(result));
        return result;
      } else {
        return Location::DoubleStackSlot(calling_convention.GetStackOffsetOf(stack_index));
      }
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unexpected parameter type " << type;
      break;
  }
  return Location::NoLocation();
}

Location InvokeDexCallingConventionVisitorARMVIXL::GetReturnLocation(DataType::Type type) const {
  switch (type) {
    case DataType::Type::kReference:
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kUint32:
    case DataType::Type::kInt32: {
      return LocationFrom(r0);
    }

    case DataType::Type::kFloat32: {
      return LocationFrom(s0);
    }

    case DataType::Type::kUint64:
    case DataType::Type::kInt64: {
      return LocationFrom(r0, r1);
    }

    case DataType::Type::kFloat64: {
      return LocationFrom(s0, s1);
    }

    case DataType::Type::kVoid:
      return Location::NoLocation();
  }

  UNREACHABLE();
}

Location InvokeDexCallingConventionVisitorARMVIXL::GetMethodLocation() const {
  return LocationFrom(kMethodRegister);
}

void CodeGeneratorARMVIXL::Move32(Location destination, Location source) {
  if (source.Equals(destination)) {
    return;
  }
  if (destination.IsRegister()) {
    if (source.IsRegister()) {
      __ Mov(RegisterFrom(destination), RegisterFrom(source));
    } else if (source.IsFpuRegister()) {
      __ Vmov(RegisterFrom(destination), SRegisterFrom(source));
    } else {
      GetAssembler()->LoadFromOffset(kLoadWord,
                                     RegisterFrom(destination),
                                     sp,
                                     source.GetStackIndex());
    }
  } else if (destination.IsFpuRegister()) {
    if (source.IsRegister()) {
      __ Vmov(SRegisterFrom(destination), RegisterFrom(source));
    } else if (source.IsFpuRegister()) {
      __ Vmov(SRegisterFrom(destination), SRegisterFrom(source));
    } else {
      GetAssembler()->LoadSFromOffset(SRegisterFrom(destination), sp, source.GetStackIndex());
    }
  } else {
    DCHECK(destination.IsStackSlot()) << destination;
    if (source.IsRegister()) {
      GetAssembler()->StoreToOffset(kStoreWord,
                                    RegisterFrom(source),
                                    sp,
                                    destination.GetStackIndex());
    } else if (source.IsFpuRegister()) {
      GetAssembler()->StoreSToOffset(SRegisterFrom(source), sp, destination.GetStackIndex());
    } else {
      DCHECK(source.IsStackSlot()) << source;
      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register temp = temps.Acquire();
      GetAssembler()->LoadFromOffset(kLoadWord, temp, sp, source.GetStackIndex());
      GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
    }
  }
}

void CodeGeneratorARMVIXL::MoveConstant(Location location, int32_t value) {
  DCHECK(location.IsRegister());
  __ Mov(RegisterFrom(location), value);
}

void CodeGeneratorARMVIXL::MoveLocation(Location dst, Location src, DataType::Type dst_type) {
  // TODO(VIXL): Maybe refactor to have the 'move' implementation here and use it in
  // `ParallelMoveResolverARMVIXL::EmitMove`, as is done in the `arm64` backend.
  HParallelMove move(GetGraph()->GetAllocator());
  move.AddMove(src, dst, dst_type, nullptr);
  GetMoveResolver()->EmitNativeCode(&move);
}

void CodeGeneratorARMVIXL::AddLocationAsTemp(Location location, LocationSummary* locations) {
  if (location.IsRegister()) {
    locations->AddTemp(location);
  } else if (location.IsRegisterPair()) {
    locations->AddTemp(LocationFrom(LowRegisterFrom(location)));
    locations->AddTemp(LocationFrom(HighRegisterFrom(location)));
  } else {
    UNIMPLEMENTED(FATAL) << "AddLocationAsTemp not implemented for location " << location;
  }
}

void CodeGeneratorARMVIXL::InvokeRuntime(QuickEntrypointEnum entrypoint,
                                         HInstruction* instruction,
                                         uint32_t dex_pc,
                                         SlowPathCode* slow_path) {
  ValidateInvokeRuntime(entrypoint, instruction, slow_path);
  __ Ldr(lr, MemOperand(tr, GetThreadOffset<kArmPointerSize>(entrypoint).Int32Value()));
  // Ensure the pc position is recorded immediately after the `blx` instruction.
  // blx in T32 has only 16bit encoding that's why a stricter check for the scope is used.
  ExactAssemblyScope aas(GetVIXLAssembler(),
                         vixl32::k16BitT32InstructionSizeInBytes,
                         CodeBufferCheckScope::kExactSize);
  __ blx(lr);
  if (EntrypointRequiresStackMap(entrypoint)) {
    RecordPcInfo(instruction, dex_pc, slow_path);
  }
}

void CodeGeneratorARMVIXL::InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                                               HInstruction* instruction,
                                                               SlowPathCode* slow_path) {
  ValidateInvokeRuntimeWithoutRecordingPcInfo(instruction, slow_path);
  __ Ldr(lr, MemOperand(tr, entry_point_offset));
  __ Blx(lr);
}

void InstructionCodeGeneratorARMVIXL::HandleGoto(HInstruction* got, HBasicBlock* successor) {
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
      vixl32::Register temp = temps.Acquire();
      __ Push(vixl32::Register(kMethodRegister));
      GetAssembler()->LoadFromOffset(kLoadWord, kMethodRegister, sp, kArmWordSize);
      __ Ldrh(temp, MemOperand(kMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
      __ Add(temp, temp, 1);
      __ Strh(temp, MemOperand(kMethodRegister, ArtMethod::HotnessCountOffset().Int32Value()));
      __ Pop(vixl32::Register(kMethodRegister));
    }
    GenerateSuspendCheck(info->GetSuspendCheck(), successor);
    return;
  }
  if (block->IsEntryBlock() && (previous != nullptr) && previous->IsSuspendCheck()) {
    GenerateSuspendCheck(previous->AsSuspendCheck(), nullptr);
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 2);
  }
  if (!codegen_->GoesToNextBlock(block, successor)) {
    __ B(codegen_->GetLabelOf(successor));
  }
}

void LocationsBuilderARMVIXL::VisitGoto(HGoto* got) {
  got->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitGoto(HGoto* got) {
  HandleGoto(got, got->GetSuccessor());
}

void LocationsBuilderARMVIXL::VisitTryBoundary(HTryBoundary* try_boundary) {
  try_boundary->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitTryBoundary(HTryBoundary* try_boundary) {
  HBasicBlock* successor = try_boundary->GetNormalFlowSuccessor();
  if (!successor->IsExitBlock()) {
    HandleGoto(try_boundary, successor);
  }
}

void LocationsBuilderARMVIXL::VisitExit(HExit* exit) {
  exit->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitExit(HExit* exit ATTRIBUTE_UNUSED) {
}

void InstructionCodeGeneratorARMVIXL::GenerateCompareTestAndBranch(HCondition* condition,
                                                                   vixl32::Label* true_target,
                                                                   vixl32::Label* false_target,
                                                                   bool is_far_target) {
  if (true_target == false_target) {
    DCHECK(true_target != nullptr);
    __ B(true_target);
    return;
  }

  vixl32::Label* non_fallthrough_target;
  bool invert;
  bool emit_both_branches;

  if (true_target == nullptr) {
    // The true target is fallthrough.
    DCHECK(false_target != nullptr);
    non_fallthrough_target = false_target;
    invert = true;
    emit_both_branches = false;
  } else {
    non_fallthrough_target = true_target;
    invert = false;
    // Either the false target is fallthrough, or there is no fallthrough
    // and both branches must be emitted.
    emit_both_branches = (false_target != nullptr);
  }

  const auto cond = GenerateTest(condition, invert, codegen_);

  __ B(cond.first, non_fallthrough_target, is_far_target);

  if (emit_both_branches) {
    // No target falls through, we need to branch.
    __ B(false_target);
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateTestAndBranch(HInstruction* instruction,
                                                            size_t condition_input_index,
                                                            vixl32::Label* true_target,
                                                            vixl32::Label* false_target,
                                                            bool far_target) {
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
      DCHECK(cond->AsIntConstant()->IsFalse()) << Int32ConstantFrom(cond);
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
    // Condition has been materialized, compare the output to 0.
    if (kIsDebugBuild) {
      Location cond_val = instruction->GetLocations()->InAt(condition_input_index);
      DCHECK(cond_val.IsRegister());
    }
    if (true_target == nullptr) {
      __ CompareAndBranchIfZero(InputRegisterAt(instruction, condition_input_index),
                                false_target,
                                far_target);
    } else {
      __ CompareAndBranchIfNonZero(InputRegisterAt(instruction, condition_input_index),
                                   true_target,
                                   far_target);
    }
  } else {
    // Condition has not been materialized. Use its inputs as the comparison and
    // its condition as the branch condition.
    HCondition* condition = cond->AsCondition();

    // If this is a long or FP comparison that has been folded into
    // the HCondition, generate the comparison directly.
    DataType::Type type = condition->InputAt(0)->GetType();
    if (type == DataType::Type::kInt64 || DataType::IsFloatingPointType(type)) {
      GenerateCompareTestAndBranch(condition, true_target, false_target, far_target);
      return;
    }

    vixl32::Label* non_fallthrough_target;
    vixl32::Condition arm_cond = vixl32::Condition::None();
    const vixl32::Register left = InputRegisterAt(cond, 0);
    const Operand right = InputOperandAt(cond, 1);

    if (true_target == nullptr) {
      arm_cond = ARMCondition(condition->GetOppositeCondition());
      non_fallthrough_target = false_target;
    } else {
      arm_cond = ARMCondition(condition->GetCondition());
      non_fallthrough_target = true_target;
    }

    if (right.IsImmediate() && right.GetImmediate() == 0 && (arm_cond.Is(ne) || arm_cond.Is(eq))) {
      if (arm_cond.Is(eq)) {
        __ CompareAndBranchIfZero(left, non_fallthrough_target, far_target);
      } else {
        DCHECK(arm_cond.Is(ne));
        __ CompareAndBranchIfNonZero(left, non_fallthrough_target, far_target);
      }
    } else {
      __ Cmp(left, right);
      __ B(arm_cond, non_fallthrough_target, far_target);
    }
  }

  // If neither branch falls through (case 3), the conditional branch to `true_target`
  // was already emitted (case 2) and we need to emit a jump to `false_target`.
  if (true_target != nullptr && false_target != nullptr) {
    __ B(false_target);
  }
}

void LocationsBuilderARMVIXL::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(if_instr);
  if (IsBooleanValueOrMaterializedCondition(if_instr->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARMVIXL::VisitIf(HIf* if_instr) {
  HBasicBlock* true_successor = if_instr->IfTrueSuccessor();
  HBasicBlock* false_successor = if_instr->IfFalseSuccessor();
  vixl32::Label* true_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), true_successor) ?
      nullptr : codegen_->GetLabelOf(true_successor);
  vixl32::Label* false_target = codegen_->GoesToNextBlock(if_instr->GetBlock(), false_successor) ?
      nullptr : codegen_->GetLabelOf(false_successor);
  GenerateTestAndBranch(if_instr, /* condition_input_index */ 0, true_target, false_target);
}

void LocationsBuilderARMVIXL::VisitDeoptimize(HDeoptimize* deoptimize) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(deoptimize, LocationSummary::kCallOnSlowPath);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  RegisterSet caller_saves = RegisterSet::Empty();
  caller_saves.Add(LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetCustomSlowPathCallerSaves(caller_saves);
  if (IsBooleanValueOrMaterializedCondition(deoptimize->InputAt(0))) {
    locations->SetInAt(0, Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARMVIXL::VisitDeoptimize(HDeoptimize* deoptimize) {
  SlowPathCodeARMVIXL* slow_path =
      deopt_slow_paths_.NewSlowPath<DeoptimizationSlowPathARMVIXL>(deoptimize);
  GenerateTestAndBranch(deoptimize,
                        /* condition_input_index */ 0,
                        slow_path->GetEntryLabel(),
                        /* false_target */ nullptr);
}

void LocationsBuilderARMVIXL::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  LocationSummary* locations = new (GetGraph()->GetAllocator())
      LocationSummary(flag, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARMVIXL::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  GetAssembler()->LoadFromOffset(kLoadWord,
                                 OutputRegister(flag),
                                 sp,
                                 codegen_->GetStackOffsetOfShouldDeoptimizeFlag());
}

void LocationsBuilderARMVIXL::VisitSelect(HSelect* select) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(select);
  const bool is_floating_point = DataType::IsFloatingPointType(select->GetType());

  if (is_floating_point) {
    locations->SetInAt(0, Location::RequiresFpuRegister());
    locations->SetInAt(1, Location::FpuRegisterOrConstant(select->GetTrueValue()));
  } else {
    locations->SetInAt(0, Location::RequiresRegister());
    locations->SetInAt(1, Arm8BitEncodableConstantOrRegister(select->GetTrueValue()));
  }

  if (IsBooleanValueOrMaterializedCondition(select->GetCondition())) {
    locations->SetInAt(2, Location::RegisterOrConstant(select->GetCondition()));
    // The code generator handles overlap with the values, but not with the condition.
    locations->SetOut(Location::SameAsFirstInput());
  } else if (is_floating_point) {
    locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
  } else {
    if (!locations->InAt(1).IsConstant()) {
      locations->SetInAt(0, Arm8BitEncodableConstantOrRegister(select->GetFalseValue()));
    }

    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorARMVIXL::VisitSelect(HSelect* select) {
  HInstruction* const condition = select->GetCondition();
  const LocationSummary* const locations = select->GetLocations();
  const DataType::Type type = select->GetType();
  const Location first = locations->InAt(0);
  const Location out = locations->Out();
  const Location second = locations->InAt(1);

  // In the unlucky case the output of this instruction overlaps
  // with an input of an "emitted-at-use-site" condition, and
  // the output of this instruction is not one of its inputs, we'll
  // need to fallback to branches instead of conditional ARM instructions.
  bool output_overlaps_with_condition_inputs =
      !IsBooleanValueOrMaterializedCondition(condition) &&
      !out.Equals(first) &&
      !out.Equals(second) &&
      (condition->GetLocations()->InAt(0).Equals(out) ||
       condition->GetLocations()->InAt(1).Equals(out));
  DCHECK(!output_overlaps_with_condition_inputs || condition->IsCondition());
  Location src;

  if (condition->IsIntConstant()) {
    if (condition->AsIntConstant()->IsFalse()) {
      src = first;
    } else {
      src = second;
    }

    codegen_->MoveLocation(out, src, type);
    return;
  }

  if (!DataType::IsFloatingPointType(type) && !output_overlaps_with_condition_inputs) {
    bool invert = false;

    if (out.Equals(second)) {
      src = first;
      invert = true;
    } else if (out.Equals(first)) {
      src = second;
    } else if (second.IsConstant()) {
      DCHECK(CanEncodeConstantAs8BitImmediate(second.GetConstant()));
      src = second;
    } else if (first.IsConstant()) {
      DCHECK(CanEncodeConstantAs8BitImmediate(first.GetConstant()));
      src = first;
      invert = true;
    } else {
      src = second;
    }

    if (CanGenerateConditionalMove(out, src)) {
      if (!out.Equals(first) && !out.Equals(second)) {
        codegen_->MoveLocation(out, src.Equals(first) ? second : first, type);
      }

      std::pair<vixl32::Condition, vixl32::Condition> cond(eq, ne);

      if (IsBooleanValueOrMaterializedCondition(condition)) {
        __ Cmp(InputRegisterAt(select, 2), 0);
        cond = invert ? std::make_pair(eq, ne) : std::make_pair(ne, eq);
      } else {
        cond = GenerateTest(condition->AsCondition(), invert, codegen_);
      }

      const size_t instr_count = out.IsRegisterPair() ? 4 : 2;
      // We use the scope because of the IT block that follows.
      ExactAssemblyScope guard(GetVIXLAssembler(),
                               instr_count * vixl32::k16BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kExactSize);

      if (out.IsRegister()) {
        __ it(cond.first);
        __ mov(cond.first, RegisterFrom(out), OperandFrom(src, type));
      } else {
        DCHECK(out.IsRegisterPair());

        Operand operand_high(0);
        Operand operand_low(0);

        if (src.IsConstant()) {
          const int64_t value = Int64ConstantFrom(src);

          operand_high = High32Bits(value);
          operand_low = Low32Bits(value);
        } else {
          DCHECK(src.IsRegisterPair());
          operand_high = HighRegisterFrom(src);
          operand_low = LowRegisterFrom(src);
        }

        __ it(cond.first);
        __ mov(cond.first, LowRegisterFrom(out), operand_low);
        __ it(cond.first);
        __ mov(cond.first, HighRegisterFrom(out), operand_high);
      }

      return;
    }
  }

  vixl32::Label* false_target = nullptr;
  vixl32::Label* true_target = nullptr;
  vixl32::Label select_end;
  vixl32::Label other_case;
  vixl32::Label* const target = codegen_->GetFinalLabel(select, &select_end);

  if (out.Equals(second)) {
    true_target = target;
    src = first;
  } else {
    false_target = target;
    src = second;

    if (!out.Equals(first)) {
      if (output_overlaps_with_condition_inputs) {
        false_target = &other_case;
      } else {
        codegen_->MoveLocation(out, first, type);
      }
    }
  }

  GenerateTestAndBranch(select, 2, true_target, false_target, /* far_target */ false);
  codegen_->MoveLocation(out, src, type);
  if (output_overlaps_with_condition_inputs) {
    __ B(target);
    __ Bind(&other_case);
    codegen_->MoveLocation(out, first, type);
  }

  if (select_end.IsReferenced()) {
    __ Bind(&select_end);
  }
}

void LocationsBuilderARMVIXL::VisitNativeDebugInfo(HNativeDebugInfo* info) {
  new (GetGraph()->GetAllocator()) LocationSummary(info);
}

void InstructionCodeGeneratorARMVIXL::VisitNativeDebugInfo(HNativeDebugInfo*) {
  // MaybeRecordNativeDebugInfo is already called implicitly in CodeGenerator::Compile.
}

void CodeGeneratorARMVIXL::GenerateNop() {
  __ Nop();
}

// `temp` is an extra temporary register that is used for some conditions;
// callers may not specify it, in which case the method will use a scratch
// register instead.
void CodeGeneratorARMVIXL::GenerateConditionWithZero(IfCondition condition,
                                                     vixl32::Register out,
                                                     vixl32::Register in,
                                                     vixl32::Register temp) {
  switch (condition) {
    case kCondEQ:
    // x <= 0 iff x == 0 when the comparison is unsigned.
    case kCondBE:
      if (!temp.IsValid() || (out.IsLow() && !out.Is(in))) {
        temp = out;
      }

      // Avoid 32-bit instructions if possible; note that `in` and `temp` must be
      // different as well.
      if (in.IsLow() && temp.IsLow() && !in.Is(temp)) {
        // temp = - in; only 0 sets the carry flag.
        __ Rsbs(temp, in, 0);

        if (out.Is(in)) {
          std::swap(in, temp);
        }

        // out = - in + in + carry = carry
        __ Adc(out, temp, in);
      } else {
        // If `in` is 0, then it has 32 leading zeros, and less than that otherwise.
        __ Clz(out, in);
        // Any number less than 32 logically shifted right by 5 bits results in 0;
        // the same operation on 32 yields 1.
        __ Lsr(out, out, 5);
      }

      break;
    case kCondNE:
    // x > 0 iff x != 0 when the comparison is unsigned.
    case kCondA: {
      UseScratchRegisterScope temps(GetVIXLAssembler());

      if (out.Is(in)) {
        if (!temp.IsValid() || in.Is(temp)) {
          temp = temps.Acquire();
        }
      } else if (!temp.IsValid() || !temp.IsLow()) {
        temp = out;
      }

      // temp = in - 1; only 0 does not set the carry flag.
      __ Subs(temp, in, 1);
      // out = in + ~temp + carry = in + (-(in - 1) - 1) + carry = in - in + 1 - 1 + carry = carry
      __ Sbc(out, in, temp);
      break;
    }
    case kCondGE:
      __ Mvn(out, in);
      in = out;
      FALLTHROUGH_INTENDED;
    case kCondLT:
      // We only care about the sign bit.
      __ Lsr(out, in, 31);
      break;
    case kCondAE:
      // Trivially true.
      __ Mov(out, 1);
      break;
    case kCondB:
      // Trivially false.
      __ Mov(out, 0);
      break;
    default:
      LOG(FATAL) << "Unexpected condition " << condition;
      UNREACHABLE();
  }
}

void LocationsBuilderARMVIXL::HandleCondition(HCondition* cond) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(cond, LocationSummary::kNoCall);
  const DataType::Type type = cond->InputAt(0)->GetType();
  if (DataType::IsFloatingPointType(type)) {
    locations->SetInAt(0, Location::RequiresFpuRegister());
    locations->SetInAt(1, ArithmeticZeroOrFpuRegister(cond->InputAt(1)));
  } else {
    locations->SetInAt(0, Location::RequiresRegister());
    locations->SetInAt(1, Location::RegisterOrConstant(cond->InputAt(1)));
  }
  if (!cond->IsEmittedAtUseSite()) {
    locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
  }
}

void InstructionCodeGeneratorARMVIXL::HandleCondition(HCondition* cond) {
  if (cond->IsEmittedAtUseSite()) {
    return;
  }

  const DataType::Type type = cond->GetLeft()->GetType();

  if (DataType::IsFloatingPointType(type)) {
    GenerateConditionGeneric(cond, codegen_);
    return;
  }

  DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;

  const IfCondition condition = cond->GetCondition();

  // A condition with only one boolean input, or two boolean inputs without being equality or
  // inequality results from transformations done by the instruction simplifier, and is handled
  // as a regular condition with integral inputs.
  if (type == DataType::Type::kBool &&
      cond->GetRight()->GetType() == DataType::Type::kBool &&
      (condition == kCondEQ || condition == kCondNE)) {
    vixl32::Register left = InputRegisterAt(cond, 0);
    const vixl32::Register out = OutputRegister(cond);
    const Location right_loc = cond->GetLocations()->InAt(1);

    // The constant case is handled by the instruction simplifier.
    DCHECK(!right_loc.IsConstant());

    vixl32::Register right = RegisterFrom(right_loc);

    // Avoid 32-bit instructions if possible.
    if (out.Is(right)) {
      std::swap(left, right);
    }

    __ Eor(out, left, right);

    if (condition == kCondEQ) {
      __ Eor(out, out, 1);
    }

    return;
  }

  GenerateConditionIntegralOrNonPrimitive(cond, codegen_);
}

void LocationsBuilderARMVIXL::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitEqual(HEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitNotEqual(HNotEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitLessThan(HLessThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitLessThanOrEqual(HLessThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitGreaterThan(HGreaterThan* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitGreaterThanOrEqual(HGreaterThanOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitBelow(HBelow* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitBelowOrEqual(HBelowOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitAbove(HAbove* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

void InstructionCodeGeneratorARMVIXL::VisitAboveOrEqual(HAboveOrEqual* comp) {
  HandleCondition(comp);
}

void LocationsBuilderARMVIXL::VisitIntConstant(HIntConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARMVIXL::VisitIntConstant(HIntConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARMVIXL::VisitNullConstant(HNullConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARMVIXL::VisitNullConstant(HNullConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARMVIXL::VisitLongConstant(HLongConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARMVIXL::VisitLongConstant(HLongConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARMVIXL::VisitFloatConstant(HFloatConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARMVIXL::VisitFloatConstant(
    HFloatConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARMVIXL::VisitDoubleConstant(HDoubleConstant* constant) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(constant, LocationSummary::kNoCall);
  locations->SetOut(Location::ConstantLocation(constant));
}

void InstructionCodeGeneratorARMVIXL::VisitDoubleConstant(
    HDoubleConstant* constant ATTRIBUTE_UNUSED) {
  // Will be generated at use site.
}

void LocationsBuilderARMVIXL::VisitConstructorFence(HConstructorFence* constructor_fence) {
  constructor_fence->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitConstructorFence(
    HConstructorFence* constructor_fence ATTRIBUTE_UNUSED) {
  codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
}

void LocationsBuilderARMVIXL::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  memory_barrier->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitMemoryBarrier(HMemoryBarrier* memory_barrier) {
  codegen_->GenerateMemoryBarrier(memory_barrier->GetBarrierKind());
}

void LocationsBuilderARMVIXL::VisitReturnVoid(HReturnVoid* ret) {
  ret->SetLocations(nullptr);
}

void InstructionCodeGeneratorARMVIXL::VisitReturnVoid(HReturnVoid* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderARMVIXL::VisitReturn(HReturn* ret) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(ret, LocationSummary::kNoCall);
  locations->SetInAt(0, parameter_visitor_.GetReturnLocation(ret->InputAt(0)->GetType()));
}

void InstructionCodeGeneratorARMVIXL::VisitReturn(HReturn* ret ATTRIBUTE_UNUSED) {
  codegen_->GenerateFrameExit();
}

void LocationsBuilderARMVIXL::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  // The trampoline uses the same calling convention as dex calling conventions,
  // except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
  // the method_idx.
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARMVIXL::VisitInvokeUnresolved(HInvokeUnresolved* invoke) {
  codegen_->GenerateInvokeUnresolvedRuntimeCall(invoke);
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 3);
}

void LocationsBuilderARMVIXL::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  IntrinsicLocationsBuilderARMVIXL intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

static bool TryGenerateIntrinsicCode(HInvoke* invoke, CodeGeneratorARMVIXL* codegen) {
  if (invoke->GetLocations()->Intrinsified()) {
    IntrinsicCodeGeneratorARMVIXL intrinsic(codegen);
    intrinsic.Dispatch(invoke);
    return true;
  }
  return false;
}

void InstructionCodeGeneratorARMVIXL::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  // Explicit clinit checks triggered by static invokes must have been pruned by
  // art::PrepareForRegisterAllocation.
  DCHECK(!invoke->IsStaticWithExplicitClinitCheck());

  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 4);
    return;
  }

  LocationSummary* locations = invoke->GetLocations();
  codegen_->GenerateStaticOrDirectCall(
      invoke, locations->HasTemps() ? locations->GetTemp(0) : Location::NoLocation());

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 5);
}

void LocationsBuilderARMVIXL::HandleInvoke(HInvoke* invoke) {
  InvokeDexCallingConventionVisitorARMVIXL calling_convention_visitor;
  CodeGenerator::CreateCommonInvokeLocationSummary(invoke, &calling_convention_visitor);
}

void LocationsBuilderARMVIXL::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  IntrinsicLocationsBuilderARMVIXL intrinsic(codegen_);
  if (intrinsic.TryDispatch(invoke)) {
    return;
  }

  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARMVIXL::VisitInvokeVirtual(HInvokeVirtual* invoke) {
  if (TryGenerateIntrinsicCode(invoke, codegen_)) {
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 6);
    return;
  }

  codegen_->GenerateVirtualCall(invoke, invoke->GetLocations()->GetTemp(0));
  DCHECK(!codegen_->IsLeafMethod());

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 7);
}

void LocationsBuilderARMVIXL::VisitInvokeInterface(HInvokeInterface* invoke) {
  HandleInvoke(invoke);
  // Add the hidden argument.
  invoke->GetLocations()->AddTemp(LocationFrom(r12));
}

void InstructionCodeGeneratorARMVIXL::VisitInvokeInterface(HInvokeInterface* invoke) {
  // TODO: b/18116999, our IMTs can miss an IncompatibleClassChangeError.
  LocationSummary* locations = invoke->GetLocations();
  vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
  vixl32::Register hidden_reg = RegisterFrom(locations->GetTemp(1));
  Location receiver = locations->InAt(0);
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();

  DCHECK(!receiver.IsStackSlot());

  // Ensure the pc position is recorded immediately after the `ldr` instruction.
  {
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    // /* HeapReference<Class> */ temp = receiver->klass_
    __ ldr(temp, MemOperand(RegisterFrom(receiver), class_offset));
    codegen_->MaybeRecordImplicitNullCheck(invoke);
  }
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  GetAssembler()->MaybeUnpoisonHeapReference(temp);
  GetAssembler()->LoadFromOffset(kLoadWord,
                                 temp,
                                 temp,
                                 mirror::Class::ImtPtrOffset(kArmPointerSize).Uint32Value());
  uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
      invoke->GetImtIndex(), kArmPointerSize));
  // temp = temp->GetImtEntryAt(method_offset);
  GetAssembler()->LoadFromOffset(kLoadWord, temp, temp, method_offset);
  uint32_t entry_point =
      ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArmPointerSize).Int32Value();
  // LR = temp->GetEntryPoint();
  GetAssembler()->LoadFromOffset(kLoadWord, lr, temp, entry_point);

  // Set the hidden (in r12) argument. It is done here, right before a BLX to prevent other
  // instruction from clobbering it as they might use r12 as a scratch register.
  DCHECK(hidden_reg.Is(r12));

  {
    // The VIXL macro assembler may clobber any of the scratch registers that are available to it,
    // so it checks if the application is using them (by passing them to the macro assembler
    // methods). The following application of UseScratchRegisterScope corrects VIXL's notion of
    // what is available, and is the opposite of the standard usage: Instead of requesting a
    // temporary location, it imposes an external constraint (i.e. a specific register is reserved
    // for the hidden argument). Note that this works even if VIXL needs a scratch register itself
    // (to materialize the constant), since the destination register becomes available for such use
    // internally for the duration of the macro instruction.
    UseScratchRegisterScope temps(GetVIXLAssembler());
    temps.Exclude(hidden_reg);
    __ Mov(hidden_reg, invoke->GetDexMethodIndex());
  }
  {
    // Ensure the pc position is recorded immediately after the `blx` instruction.
    // blx in T32 has only 16bit encoding that's why a stricter check for the scope is used.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::k16BitT32InstructionSizeInBytes,
                           CodeBufferCheckScope::kExactSize);
    // LR();
    __ blx(lr);
    codegen_->RecordPcInfo(invoke, invoke->GetDexPc());
    DCHECK(!codegen_->IsLeafMethod());
  }

  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 8);
}

void LocationsBuilderARMVIXL::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  HandleInvoke(invoke);
}

void InstructionCodeGeneratorARMVIXL::VisitInvokePolymorphic(HInvokePolymorphic* invoke) {
  codegen_->GenerateInvokePolymorphicCall(invoke);
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 9);
}

void LocationsBuilderARMVIXL::VisitNeg(HNeg* neg) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(neg, LocationSummary::kNoCall);
  switch (neg->GetResultType()) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << neg->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitNeg(HNeg* neg) {
  LocationSummary* locations = neg->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);
  switch (neg->GetResultType()) {
    case DataType::Type::kInt32:
      __ Rsb(OutputRegister(neg), InputRegisterAt(neg, 0), 0);
      break;

    case DataType::Type::kInt64:
      // out.lo = 0 - in.lo (and update the carry/borrow (C) flag)
      __ Rsbs(LowRegisterFrom(out), LowRegisterFrom(in), 0);
      // We cannot emit an RSC (Reverse Subtract with Carry)
      // instruction here, as it does not exist in the Thumb-2
      // instruction set.  We use the following approach
      // using SBC and SUB instead.
      //
      // out.hi = -C
      __ Sbc(HighRegisterFrom(out), HighRegisterFrom(out), HighRegisterFrom(out));
      // out.hi = out.hi - in.hi
      __ Sub(HighRegisterFrom(out), HighRegisterFrom(out), HighRegisterFrom(in));
      break;

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Vneg(OutputVRegister(neg), InputVRegister(neg));
      break;

    default:
      LOG(FATAL) << "Unexpected neg type " << neg->GetResultType();
  }
}

void LocationsBuilderARMVIXL::VisitTypeConversion(HTypeConversion* conversion) {
  DataType::Type result_type = conversion->GetResultType();
  DataType::Type input_type = conversion->GetInputType();
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;

  // The float-to-long, double-to-long and long-to-float type conversions
  // rely on a call to the runtime.
  LocationSummary::CallKind call_kind =
      (((input_type == DataType::Type::kFloat32 || input_type == DataType::Type::kFloat64)
        && result_type == DataType::Type::kInt64)
       || (input_type == DataType::Type::kInt64 && result_type == DataType::Type::kFloat32))
      ? LocationSummary::kCallOnMainOnly
      : LocationSummary::kNoCall;
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(conversion, call_kind);

  switch (result_type) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK(DataType::IsIntegralType(input_type)) << input_type;
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;

    case DataType::Type::kInt32:
      switch (input_type) {
        case DataType::Type::kInt64:
          locations->SetInAt(0, Location::Any());
          locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
          break;

        case DataType::Type::kFloat32:
          locations->SetInAt(0, Location::RequiresFpuRegister());
          locations->SetOut(Location::RequiresRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          break;

        case DataType::Type::kFloat64:
          locations->SetInAt(0, Location::RequiresFpuRegister());
          locations->SetOut(Location::RequiresRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kInt64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          locations->SetInAt(0, Location::RequiresRegister());
          locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
          break;

        case DataType::Type::kFloat32: {
          InvokeRuntimeCallingConventionARMVIXL calling_convention;
          locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0)));
          locations->SetOut(LocationFrom(r0, r1));
          break;
        }

        case DataType::Type::kFloat64: {
          InvokeRuntimeCallingConventionARMVIXL calling_convention;
          locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0),
                                             calling_convention.GetFpuRegisterAt(1)));
          locations->SetOut(LocationFrom(r0, r1));
          break;
        }

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kFloat32:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          locations->SetInAt(0, Location::RequiresRegister());
          locations->SetOut(Location::RequiresFpuRegister());
          break;

        case DataType::Type::kInt64: {
          InvokeRuntimeCallingConventionARMVIXL calling_convention;
          locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0),
                                             calling_convention.GetRegisterAt(1)));
          locations->SetOut(LocationFrom(calling_convention.GetFpuRegisterAt(0)));
          break;
        }

        case DataType::Type::kFloat64:
          locations->SetInAt(0, Location::RequiresFpuRegister());
          locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kFloat64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          locations->SetInAt(0, Location::RequiresRegister());
          locations->SetOut(Location::RequiresFpuRegister());
          break;

        case DataType::Type::kInt64:
          locations->SetInAt(0, Location::RequiresRegister());
          locations->SetOut(Location::RequiresFpuRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          break;

        case DataType::Type::kFloat32:
          locations->SetInAt(0, Location::RequiresFpuRegister());
          locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    default:
      LOG(FATAL) << "Unexpected type conversion from " << input_type
                 << " to " << result_type;
  }
}

void InstructionCodeGeneratorARMVIXL::VisitTypeConversion(HTypeConversion* conversion) {
  LocationSummary* locations = conversion->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);
  DataType::Type result_type = conversion->GetResultType();
  DataType::Type input_type = conversion->GetInputType();
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << " -> " << result_type;
  switch (result_type) {
    case DataType::Type::kUint8:
      switch (input_type) {
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          __ Ubfx(OutputRegister(conversion), InputRegisterAt(conversion, 0), 0, 8);
          break;
        case DataType::Type::kInt64:
          __ Ubfx(OutputRegister(conversion), LowRegisterFrom(in), 0, 8);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kInt8:
      switch (input_type) {
        case DataType::Type::kUint8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          __ Sbfx(OutputRegister(conversion), InputRegisterAt(conversion, 0), 0, 8);
          break;
        case DataType::Type::kInt64:
          __ Sbfx(OutputRegister(conversion), LowRegisterFrom(in), 0, 8);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kUint16:
      switch (input_type) {
        case DataType::Type::kInt8:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          __ Ubfx(OutputRegister(conversion), InputRegisterAt(conversion, 0), 0, 16);
          break;
        case DataType::Type::kInt64:
          __ Ubfx(OutputRegister(conversion), LowRegisterFrom(in), 0, 16);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kInt16:
      switch (input_type) {
        case DataType::Type::kUint16:
        case DataType::Type::kInt32:
          __ Sbfx(OutputRegister(conversion), InputRegisterAt(conversion, 0), 0, 16);
          break;
        case DataType::Type::kInt64:
          __ Sbfx(OutputRegister(conversion), LowRegisterFrom(in), 0, 16);
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kInt32:
      switch (input_type) {
        case DataType::Type::kInt64:
          DCHECK(out.IsRegister());
          if (in.IsRegisterPair()) {
            __ Mov(OutputRegister(conversion), LowRegisterFrom(in));
          } else if (in.IsDoubleStackSlot()) {
            GetAssembler()->LoadFromOffset(kLoadWord,
                                           OutputRegister(conversion),
                                           sp,
                                           in.GetStackIndex());
          } else {
            DCHECK(in.IsConstant());
            DCHECK(in.GetConstant()->IsLongConstant());
            int64_t value = in.GetConstant()->AsLongConstant()->GetValue();
            __ Mov(OutputRegister(conversion), static_cast<int32_t>(value));
          }
          break;

        case DataType::Type::kFloat32: {
          vixl32::SRegister temp = LowSRegisterFrom(locations->GetTemp(0));
          __ Vcvt(S32, F32, temp, InputSRegisterAt(conversion, 0));
          __ Vmov(OutputRegister(conversion), temp);
          break;
        }

        case DataType::Type::kFloat64: {
          vixl32::SRegister temp_s = LowSRegisterFrom(locations->GetTemp(0));
          __ Vcvt(S32, F64, temp_s, DRegisterFrom(in));
          __ Vmov(OutputRegister(conversion), temp_s);
          break;
        }

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kInt64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          DCHECK(out.IsRegisterPair());
          DCHECK(in.IsRegister());
          __ Mov(LowRegisterFrom(out), InputRegisterAt(conversion, 0));
          // Sign extension.
          __ Asr(HighRegisterFrom(out), LowRegisterFrom(out), 31);
          break;

        case DataType::Type::kFloat32:
          codegen_->InvokeRuntime(kQuickF2l, conversion, conversion->GetDexPc());
          CheckEntrypointTypes<kQuickF2l, int64_t, float>();
          break;

        case DataType::Type::kFloat64:
          codegen_->InvokeRuntime(kQuickD2l, conversion, conversion->GetDexPc());
          CheckEntrypointTypes<kQuickD2l, int64_t, double>();
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kFloat32:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          __ Vmov(OutputSRegister(conversion), InputRegisterAt(conversion, 0));
          __ Vcvt(F32, S32, OutputSRegister(conversion), OutputSRegister(conversion));
          break;

        case DataType::Type::kInt64:
          codegen_->InvokeRuntime(kQuickL2f, conversion, conversion->GetDexPc());
          CheckEntrypointTypes<kQuickL2f, float, int64_t>();
          break;

        case DataType::Type::kFloat64:
          __ Vcvt(F32, F64, OutputSRegister(conversion), DRegisterFrom(in));
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    case DataType::Type::kFloat64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          __ Vmov(LowSRegisterFrom(out), InputRegisterAt(conversion, 0));
          __ Vcvt(F64, S32, DRegisterFrom(out), LowSRegisterFrom(out));
          break;

        case DataType::Type::kInt64: {
          vixl32::Register low = LowRegisterFrom(in);
          vixl32::Register high = HighRegisterFrom(in);
          vixl32::SRegister out_s = LowSRegisterFrom(out);
          vixl32::DRegister out_d = DRegisterFrom(out);
          vixl32::SRegister temp_s = LowSRegisterFrom(locations->GetTemp(0));
          vixl32::DRegister temp_d = DRegisterFrom(locations->GetTemp(0));
          vixl32::DRegister constant_d = DRegisterFrom(locations->GetTemp(1));

          // temp_d = int-to-double(high)
          __ Vmov(temp_s, high);
          __ Vcvt(F64, S32, temp_d, temp_s);
          // constant_d = k2Pow32EncodingForDouble
          __ Vmov(constant_d, bit_cast<double, int64_t>(k2Pow32EncodingForDouble));
          // out_d = unsigned-to-double(low)
          __ Vmov(out_s, low);
          __ Vcvt(F64, U32, out_d, out_s);
          // out_d += temp_d * constant_d
          __ Vmla(F64, out_d, temp_d, constant_d);
          break;
        }

        case DataType::Type::kFloat32:
          __ Vcvt(F64, F32, DRegisterFrom(out), InputSRegisterAt(conversion, 0));
          break;

        default:
          LOG(FATAL) << "Unexpected type conversion from " << input_type
                     << " to " << result_type;
      }
      break;

    default:
      LOG(FATAL) << "Unexpected type conversion from " << input_type
                 << " to " << result_type;
  }
}

void LocationsBuilderARMVIXL::VisitAdd(HAdd* add) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(add, LocationSummary::kNoCall);
  switch (add->GetResultType()) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(add->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, ArmEncodableConstantOrRegister(add->InputAt(1), ADD));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected add type " << add->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitAdd(HAdd* add) {
  LocationSummary* locations = add->GetLocations();
  Location out = locations->Out();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);

  switch (add->GetResultType()) {
    case DataType::Type::kInt32: {
      __ Add(OutputRegister(add), InputRegisterAt(add, 0), InputOperandAt(add, 1));
      }
      break;

    case DataType::Type::kInt64: {
      if (second.IsConstant()) {
        uint64_t value = static_cast<uint64_t>(Int64FromConstant(second.GetConstant()));
        GenerateAddLongConst(out, first, value);
      } else {
        DCHECK(second.IsRegisterPair());
        __ Adds(LowRegisterFrom(out), LowRegisterFrom(first), LowRegisterFrom(second));
        __ Adc(HighRegisterFrom(out), HighRegisterFrom(first), HighRegisterFrom(second));
      }
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Vadd(OutputVRegister(add), InputVRegisterAt(add, 0), InputVRegisterAt(add, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected add type " << add->GetResultType();
  }
}

void LocationsBuilderARMVIXL::VisitSub(HSub* sub) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(sub, LocationSummary::kNoCall);
  switch (sub->GetResultType()) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(sub->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, ArmEncodableConstantOrRegister(sub->InputAt(1), SUB));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected sub type " << sub->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitSub(HSub* sub) {
  LocationSummary* locations = sub->GetLocations();
  Location out = locations->Out();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);
  switch (sub->GetResultType()) {
    case DataType::Type::kInt32: {
      __ Sub(OutputRegister(sub), InputRegisterAt(sub, 0), InputOperandAt(sub, 1));
      break;
    }

    case DataType::Type::kInt64: {
      if (second.IsConstant()) {
        uint64_t value = static_cast<uint64_t>(Int64FromConstant(second.GetConstant()));
        GenerateAddLongConst(out, first, -value);
      } else {
        DCHECK(second.IsRegisterPair());
        __ Subs(LowRegisterFrom(out), LowRegisterFrom(first), LowRegisterFrom(second));
        __ Sbc(HighRegisterFrom(out), HighRegisterFrom(first), HighRegisterFrom(second));
      }
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Vsub(OutputVRegister(sub), InputVRegisterAt(sub, 0), InputVRegisterAt(sub, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected sub type " << sub->GetResultType();
  }
}

void LocationsBuilderARMVIXL::VisitMul(HMul* mul) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(mul, LocationSummary::kNoCall);
  switch (mul->GetResultType()) {
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:  {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected mul type " << mul->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitMul(HMul* mul) {
  LocationSummary* locations = mul->GetLocations();
  Location out = locations->Out();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);
  switch (mul->GetResultType()) {
    case DataType::Type::kInt32: {
      __ Mul(OutputRegister(mul), InputRegisterAt(mul, 0), InputRegisterAt(mul, 1));
      break;
    }
    case DataType::Type::kInt64: {
      vixl32::Register out_hi = HighRegisterFrom(out);
      vixl32::Register out_lo = LowRegisterFrom(out);
      vixl32::Register in1_hi = HighRegisterFrom(first);
      vixl32::Register in1_lo = LowRegisterFrom(first);
      vixl32::Register in2_hi = HighRegisterFrom(second);
      vixl32::Register in2_lo = LowRegisterFrom(second);

      // Extra checks to protect caused by the existence of R1_R2.
      // The algorithm is wrong if out.hi is either in1.lo or in2.lo:
      // (e.g. in1=r0_r1, in2=r2_r3 and out=r1_r2);
      DCHECK(!out_hi.Is(in1_lo));
      DCHECK(!out_hi.Is(in2_lo));

      // input: in1 - 64 bits, in2 - 64 bits
      // output: out
      // formula: out.hi : out.lo = (in1.lo * in2.hi + in1.hi * in2.lo)* 2^32 + in1.lo * in2.lo
      // parts: out.hi = in1.lo * in2.hi + in1.hi * in2.lo + (in1.lo * in2.lo)[63:32]
      // parts: out.lo = (in1.lo * in2.lo)[31:0]

      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register temp = temps.Acquire();
      // temp <- in1.lo * in2.hi
      __ Mul(temp, in1_lo, in2_hi);
      // out.hi <- in1.lo * in2.hi + in1.hi * in2.lo
      __ Mla(out_hi, in1_hi, in2_lo, temp);
      // out.lo <- (in1.lo * in2.lo)[31:0];
      __ Umull(out_lo, temp, in1_lo, in2_lo);
      // out.hi <- in2.hi * in1.lo +  in2.lo * in1.hi + (in1.lo * in2.lo)[63:32]
      __ Add(out_hi, out_hi, temp);
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Vmul(OutputVRegister(mul), InputVRegisterAt(mul, 0), InputVRegisterAt(mul, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected mul type " << mul->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::DivRemOneOrMinusOne(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32);

  Location second = instruction->GetLocations()->InAt(1);
  DCHECK(second.IsConstant());

  vixl32::Register out = OutputRegister(instruction);
  vixl32::Register dividend = InputRegisterAt(instruction, 0);
  int32_t imm = Int32ConstantFrom(second);
  DCHECK(imm == 1 || imm == -1);

  if (instruction->IsRem()) {
    __ Mov(out, 0);
  } else {
    if (imm == 1) {
      __ Mov(out, dividend);
    } else {
      __ Rsb(out, dividend, 0);
    }
  }
}

void InstructionCodeGeneratorARMVIXL::DivRemByPowerOfTwo(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32);

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  vixl32::Register out = OutputRegister(instruction);
  vixl32::Register dividend = InputRegisterAt(instruction, 0);
  vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
  int32_t imm = Int32ConstantFrom(second);
  uint32_t abs_imm = static_cast<uint32_t>(AbsOrMin(imm));
  int ctz_imm = CTZ(abs_imm);

  if (ctz_imm == 1) {
    __ Lsr(temp, dividend, 32 - ctz_imm);
  } else {
    __ Asr(temp, dividend, 31);
    __ Lsr(temp, temp, 32 - ctz_imm);
  }
  __ Add(out, temp, dividend);

  if (instruction->IsDiv()) {
    __ Asr(out, out, ctz_imm);
    if (imm < 0) {
      __ Rsb(out, out, 0);
    }
  } else {
    __ Ubfx(out, out, 0, ctz_imm);
    __ Sub(out, out, temp);
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateDivRemWithAnyConstant(HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32);

  LocationSummary* locations = instruction->GetLocations();
  Location second = locations->InAt(1);
  DCHECK(second.IsConstant());

  vixl32::Register out = OutputRegister(instruction);
  vixl32::Register dividend = InputRegisterAt(instruction, 0);
  vixl32::Register temp1 = RegisterFrom(locations->GetTemp(0));
  vixl32::Register temp2 = RegisterFrom(locations->GetTemp(1));
  int32_t imm = Int32ConstantFrom(second);

  int64_t magic;
  int shift;
  CalculateMagicAndShiftForDivRem(imm, false /* is_long */, &magic, &shift);

  // TODO(VIXL): Change the static cast to Operand::From() after VIXL is fixed.
  __ Mov(temp1, static_cast<int32_t>(magic));
  __ Smull(temp2, temp1, dividend, temp1);

  if (imm > 0 && magic < 0) {
    __ Add(temp1, temp1, dividend);
  } else if (imm < 0 && magic > 0) {
    __ Sub(temp1, temp1, dividend);
  }

  if (shift != 0) {
    __ Asr(temp1, temp1, shift);
  }

  if (instruction->IsDiv()) {
    __ Sub(out, temp1, Operand(temp1, vixl32::Shift(ASR), 31));
  } else {
    __ Sub(temp1, temp1, Operand(temp1, vixl32::Shift(ASR), 31));
    // TODO: Strength reduction for mls.
    __ Mov(temp2, imm);
    __ Mls(out, temp1, temp2, dividend);
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateDivRemConstantIntegral(
    HBinaryOperation* instruction) {
  DCHECK(instruction->IsDiv() || instruction->IsRem());
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32);

  Location second = instruction->GetLocations()->InAt(1);
  DCHECK(second.IsConstant());

  int32_t imm = Int32ConstantFrom(second);
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
}

void LocationsBuilderARMVIXL::VisitDiv(HDiv* div) {
  LocationSummary::CallKind call_kind = LocationSummary::kNoCall;
  if (div->GetResultType() == DataType::Type::kInt64) {
    // pLdiv runtime call.
    call_kind = LocationSummary::kCallOnMainOnly;
  } else if (div->GetResultType() == DataType::Type::kInt32 && div->InputAt(1)->IsConstant()) {
    // sdiv will be replaced by other instruction sequence.
  } else if (div->GetResultType() == DataType::Type::kInt32 &&
             !codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
    // pIdivmod runtime call.
    call_kind = LocationSummary::kCallOnMainOnly;
  }

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(div, call_kind);

  switch (div->GetResultType()) {
    case DataType::Type::kInt32: {
      if (div->InputAt(1)->IsConstant()) {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::ConstantLocation(div->InputAt(1)->AsConstant()));
        locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
        int32_t value = Int32ConstantFrom(div->InputAt(1));
        if (value == 1 || value == 0 || value == -1) {
          // No temp register required.
        } else {
          locations->AddTemp(Location::RequiresRegister());
          if (!IsPowerOfTwo(AbsOrMin(value))) {
            locations->AddTemp(Location::RequiresRegister());
          }
        }
      } else if (codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::RequiresRegister());
        locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      } else {
        InvokeRuntimeCallingConventionARMVIXL calling_convention;
        locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
        locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
        // Note: divmod will compute both the quotient and the remainder as the pair R0 and R1, but
        //       we only need the former.
        locations->SetOut(LocationFrom(r0));
      }
      break;
    }
    case DataType::Type::kInt64: {
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      locations->SetInAt(0, LocationFrom(
          calling_convention.GetRegisterAt(0), calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, LocationFrom(
          calling_convention.GetRegisterAt(2), calling_convention.GetRegisterAt(3)));
      locations->SetOut(LocationFrom(r0, r1));
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected div type " << div->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitDiv(HDiv* div) {
  Location lhs = div->GetLocations()->InAt(0);
  Location rhs = div->GetLocations()->InAt(1);

  switch (div->GetResultType()) {
    case DataType::Type::kInt32: {
      if (rhs.IsConstant()) {
        GenerateDivRemConstantIntegral(div);
      } else if (codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
        __ Sdiv(OutputRegister(div), InputRegisterAt(div, 0), InputRegisterAt(div, 1));
      } else {
        InvokeRuntimeCallingConventionARMVIXL calling_convention;
        DCHECK(calling_convention.GetRegisterAt(0).Is(RegisterFrom(lhs)));
        DCHECK(calling_convention.GetRegisterAt(1).Is(RegisterFrom(rhs)));
        DCHECK(r0.Is(OutputRegister(div)));

        codegen_->InvokeRuntime(kQuickIdivmod, div, div->GetDexPc());
        CheckEntrypointTypes<kQuickIdivmod, int32_t, int32_t, int32_t>();
      }
      break;
    }

    case DataType::Type::kInt64: {
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      DCHECK(calling_convention.GetRegisterAt(0).Is(LowRegisterFrom(lhs)));
      DCHECK(calling_convention.GetRegisterAt(1).Is(HighRegisterFrom(lhs)));
      DCHECK(calling_convention.GetRegisterAt(2).Is(LowRegisterFrom(rhs)));
      DCHECK(calling_convention.GetRegisterAt(3).Is(HighRegisterFrom(rhs)));
      DCHECK(LowRegisterFrom(div->GetLocations()->Out()).Is(r0));
      DCHECK(HighRegisterFrom(div->GetLocations()->Out()).Is(r1));

      codegen_->InvokeRuntime(kQuickLdiv, div, div->GetDexPc());
      CheckEntrypointTypes<kQuickLdiv, int64_t, int64_t, int64_t>();
      break;
    }

    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Vdiv(OutputVRegister(div), InputVRegisterAt(div, 0), InputVRegisterAt(div, 1));
      break;

    default:
      LOG(FATAL) << "Unexpected div type " << div->GetResultType();
  }
}

void LocationsBuilderARMVIXL::VisitRem(HRem* rem) {
  DataType::Type type = rem->GetResultType();

  // Most remainders are implemented in the runtime.
  LocationSummary::CallKind call_kind = LocationSummary::kCallOnMainOnly;
  if (rem->GetResultType() == DataType::Type::kInt32 && rem->InputAt(1)->IsConstant()) {
    // sdiv will be replaced by other instruction sequence.
    call_kind = LocationSummary::kNoCall;
  } else if ((rem->GetResultType() == DataType::Type::kInt32)
             && codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
    // Have hardware divide instruction for int, do it with three instructions.
    call_kind = LocationSummary::kNoCall;
  }

  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(rem, call_kind);

  switch (type) {
    case DataType::Type::kInt32: {
      if (rem->InputAt(1)->IsConstant()) {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::ConstantLocation(rem->InputAt(1)->AsConstant()));
        locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
        int32_t value = Int32ConstantFrom(rem->InputAt(1));
        if (value == 1 || value == 0 || value == -1) {
          // No temp register required.
        } else {
          locations->AddTemp(Location::RequiresRegister());
          if (!IsPowerOfTwo(AbsOrMin(value))) {
            locations->AddTemp(Location::RequiresRegister());
          }
        }
      } else if (codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
        locations->SetInAt(0, Location::RequiresRegister());
        locations->SetInAt(1, Location::RequiresRegister());
        locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
        locations->AddTemp(Location::RequiresRegister());
      } else {
        InvokeRuntimeCallingConventionARMVIXL calling_convention;
        locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
        locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
        // Note: divmod will compute both the quotient and the remainder as the pair R0 and R1, but
        //       we only need the latter.
        locations->SetOut(LocationFrom(r1));
      }
      break;
    }
    case DataType::Type::kInt64: {
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      locations->SetInAt(0, LocationFrom(
          calling_convention.GetRegisterAt(0), calling_convention.GetRegisterAt(1)));
      locations->SetInAt(1, LocationFrom(
          calling_convention.GetRegisterAt(2), calling_convention.GetRegisterAt(3)));
      // The runtime helper puts the output in R2,R3.
      locations->SetOut(LocationFrom(r2, r3));
      break;
    }
    case DataType::Type::kFloat32: {
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0)));
      locations->SetInAt(1, LocationFrom(calling_convention.GetFpuRegisterAt(1)));
      locations->SetOut(LocationFrom(s0));
      break;
    }

    case DataType::Type::kFloat64: {
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      locations->SetInAt(0, LocationFrom(
          calling_convention.GetFpuRegisterAt(0), calling_convention.GetFpuRegisterAt(1)));
      locations->SetInAt(1, LocationFrom(
          calling_convention.GetFpuRegisterAt(2), calling_convention.GetFpuRegisterAt(3)));
      locations->SetOut(LocationFrom(s0, s1));
      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}

void InstructionCodeGeneratorARMVIXL::VisitRem(HRem* rem) {
  LocationSummary* locations = rem->GetLocations();
  Location second = locations->InAt(1);

  DataType::Type type = rem->GetResultType();
  switch (type) {
    case DataType::Type::kInt32: {
        vixl32::Register reg1 = InputRegisterAt(rem, 0);
        vixl32::Register out_reg = OutputRegister(rem);
        if (second.IsConstant()) {
          GenerateDivRemConstantIntegral(rem);
        } else if (codegen_->GetInstructionSetFeatures().HasDivideInstruction()) {
        vixl32::Register reg2 = RegisterFrom(second);
        vixl32::Register temp = RegisterFrom(locations->GetTemp(0));

        // temp = reg1 / reg2  (integer division)
        // dest = reg1 - temp * reg2
        __ Sdiv(temp, reg1, reg2);
        __ Mls(out_reg, temp, reg2, reg1);
      } else {
        InvokeRuntimeCallingConventionARMVIXL calling_convention;
        DCHECK(reg1.Is(calling_convention.GetRegisterAt(0)));
        DCHECK(RegisterFrom(second).Is(calling_convention.GetRegisterAt(1)));
        DCHECK(out_reg.Is(r1));

        codegen_->InvokeRuntime(kQuickIdivmod, rem, rem->GetDexPc());
        CheckEntrypointTypes<kQuickIdivmod, int32_t, int32_t, int32_t>();
      }
      break;
    }

    case DataType::Type::kInt64: {
      codegen_->InvokeRuntime(kQuickLmod, rem, rem->GetDexPc());
        CheckEntrypointTypes<kQuickLmod, int64_t, int64_t, int64_t>();
      break;
    }

    case DataType::Type::kFloat32: {
      codegen_->InvokeRuntime(kQuickFmodf, rem, rem->GetDexPc());
      CheckEntrypointTypes<kQuickFmodf, float, float, float>();
      break;
    }

    case DataType::Type::kFloat64: {
      codegen_->InvokeRuntime(kQuickFmod, rem, rem->GetDexPc());
      CheckEntrypointTypes<kQuickFmod, double, double, double>();
      break;
    }

    default:
      LOG(FATAL) << "Unexpected rem type " << type;
  }
}


void LocationsBuilderARMVIXL::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RegisterOrConstant(instruction->InputAt(0)));
}

void InstructionCodeGeneratorARMVIXL::VisitDivZeroCheck(HDivZeroCheck* instruction) {
  DivZeroCheckSlowPathARMVIXL* slow_path =
      new (codegen_->GetScopedAllocator()) DivZeroCheckSlowPathARMVIXL(instruction);
  codegen_->AddSlowPath(slow_path);

  LocationSummary* locations = instruction->GetLocations();
  Location value = locations->InAt(0);

  switch (instruction->GetType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      if (value.IsRegister()) {
        __ CompareAndBranchIfZero(InputRegisterAt(instruction, 0), slow_path->GetEntryLabel());
      } else {
        DCHECK(value.IsConstant()) << value;
        if (Int32ConstantFrom(value) == 0) {
          __ B(slow_path->GetEntryLabel());
        }
      }
      break;
    }
    case DataType::Type::kInt64: {
      if (value.IsRegisterPair()) {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Orrs(temp, LowRegisterFrom(value), HighRegisterFrom(value));
        __ B(eq, slow_path->GetEntryLabel());
      } else {
        DCHECK(value.IsConstant()) << value;
        if (Int64ConstantFrom(value) == 0) {
          __ B(slow_path->GetEntryLabel());
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected type for HDivZeroCheck " << instruction->GetType();
  }
}

void InstructionCodeGeneratorARMVIXL::HandleIntegerRotate(HRor* ror) {
  LocationSummary* locations = ror->GetLocations();
  vixl32::Register in = InputRegisterAt(ror, 0);
  Location rhs = locations->InAt(1);
  vixl32::Register out = OutputRegister(ror);

  if (rhs.IsConstant()) {
    // Arm32 and Thumb2 assemblers require a rotation on the interval [1,31],
    // so map all rotations to a +ve. equivalent in that range.
    // (e.g. left *or* right by -2 bits == 30 bits in the same direction.)
    uint32_t rot = CodeGenerator::GetInt32ValueOf(rhs.GetConstant()) & 0x1F;
    if (rot) {
      // Rotate, mapping left rotations to right equivalents if necessary.
      // (e.g. left by 2 bits == right by 30.)
      __ Ror(out, in, rot);
    } else if (!out.Is(in)) {
      __ Mov(out, in);
    }
  } else {
    __ Ror(out, in, RegisterFrom(rhs));
  }
}

// Gain some speed by mapping all Long rotates onto equivalent pairs of Integer
// rotates by swapping input regs (effectively rotating by the first 32-bits of
// a larger rotation) or flipping direction (thus treating larger right/left
// rotations as sub-word sized rotations in the other direction) as appropriate.
void InstructionCodeGeneratorARMVIXL::HandleLongRotate(HRor* ror) {
  LocationSummary* locations = ror->GetLocations();
  vixl32::Register in_reg_lo = LowRegisterFrom(locations->InAt(0));
  vixl32::Register in_reg_hi = HighRegisterFrom(locations->InAt(0));
  Location rhs = locations->InAt(1);
  vixl32::Register out_reg_lo = LowRegisterFrom(locations->Out());
  vixl32::Register out_reg_hi = HighRegisterFrom(locations->Out());

  if (rhs.IsConstant()) {
    uint64_t rot = CodeGenerator::GetInt64ValueOf(rhs.GetConstant());
    // Map all rotations to +ve. equivalents on the interval [0,63].
    rot &= kMaxLongShiftDistance;
    // For rotates over a word in size, 'pre-rotate' by 32-bits to keep rotate
    // logic below to a simple pair of binary orr.
    // (e.g. 34 bits == in_reg swap + 2 bits right.)
    if (rot >= kArmBitsPerWord) {
      rot -= kArmBitsPerWord;
      std::swap(in_reg_hi, in_reg_lo);
    }
    // Rotate, or mov to out for zero or word size rotations.
    if (rot != 0u) {
      __ Lsr(out_reg_hi, in_reg_hi, Operand::From(rot));
      __ Orr(out_reg_hi, out_reg_hi, Operand(in_reg_lo, ShiftType::LSL, kArmBitsPerWord - rot));
      __ Lsr(out_reg_lo, in_reg_lo, Operand::From(rot));
      __ Orr(out_reg_lo, out_reg_lo, Operand(in_reg_hi, ShiftType::LSL, kArmBitsPerWord - rot));
    } else {
      __ Mov(out_reg_lo, in_reg_lo);
      __ Mov(out_reg_hi, in_reg_hi);
    }
  } else {
    vixl32::Register shift_right = RegisterFrom(locations->GetTemp(0));
    vixl32::Register shift_left = RegisterFrom(locations->GetTemp(1));
    vixl32::Label end;
    vixl32::Label shift_by_32_plus_shift_right;
    vixl32::Label* final_label = codegen_->GetFinalLabel(ror, &end);

    __ And(shift_right, RegisterFrom(rhs), 0x1F);
    __ Lsrs(shift_left, RegisterFrom(rhs), 6);
    __ Rsb(LeaveFlags, shift_left, shift_right, Operand::From(kArmBitsPerWord));
    __ B(cc, &shift_by_32_plus_shift_right, /* far_target */ false);

    // out_reg_hi = (reg_hi << shift_left) | (reg_lo >> shift_right).
    // out_reg_lo = (reg_lo << shift_left) | (reg_hi >> shift_right).
    __ Lsl(out_reg_hi, in_reg_hi, shift_left);
    __ Lsr(out_reg_lo, in_reg_lo, shift_right);
    __ Add(out_reg_hi, out_reg_hi, out_reg_lo);
    __ Lsl(out_reg_lo, in_reg_lo, shift_left);
    __ Lsr(shift_left, in_reg_hi, shift_right);
    __ Add(out_reg_lo, out_reg_lo, shift_left);
    __ B(final_label);

    __ Bind(&shift_by_32_plus_shift_right);  // Shift by 32+shift_right.
    // out_reg_hi = (reg_hi >> shift_right) | (reg_lo << shift_left).
    // out_reg_lo = (reg_lo >> shift_right) | (reg_hi << shift_left).
    __ Lsr(out_reg_hi, in_reg_hi, shift_right);
    __ Lsl(out_reg_lo, in_reg_lo, shift_left);
    __ Add(out_reg_hi, out_reg_hi, out_reg_lo);
    __ Lsr(out_reg_lo, in_reg_lo, shift_right);
    __ Lsl(shift_right, in_reg_hi, shift_left);
    __ Add(out_reg_lo, out_reg_lo, shift_right);

    if (end.IsReferenced()) {
      __ Bind(&end);
    }
  }
}

void LocationsBuilderARMVIXL::VisitRor(HRor* ror) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(ror, LocationSummary::kNoCall);
  switch (ror->GetResultType()) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(ror->InputAt(1)));
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      break;
    }
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      if (ror->InputAt(1)->IsConstant()) {
        locations->SetInAt(1, Location::ConstantLocation(ror->InputAt(1)->AsConstant()));
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
        locations->AddTemp(Location::RequiresRegister());
        locations->AddTemp(Location::RequiresRegister());
      }
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << ror->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitRor(HRor* ror) {
  DataType::Type type = ror->GetResultType();
  switch (type) {
    case DataType::Type::kInt32: {
      HandleIntegerRotate(ror);
      break;
    }
    case DataType::Type::kInt64: {
      HandleLongRotate(ror);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << type;
      UNREACHABLE();
  }
}

void LocationsBuilderARMVIXL::HandleShift(HBinaryOperation* op) {
  DCHECK(op->IsShl() || op->IsShr() || op->IsUShr());

  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(op, LocationSummary::kNoCall);

  switch (op->GetResultType()) {
    case DataType::Type::kInt32: {
      locations->SetInAt(0, Location::RequiresRegister());
      if (op->InputAt(1)->IsConstant()) {
        locations->SetInAt(1, Location::ConstantLocation(op->InputAt(1)->AsConstant()));
        locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
        // Make the output overlap, as it will be used to hold the masked
        // second input.
        locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      }
      break;
    }
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      if (op->InputAt(1)->IsConstant()) {
        locations->SetInAt(1, Location::ConstantLocation(op->InputAt(1)->AsConstant()));
        // For simplicity, use kOutputOverlap even though we only require that low registers
        // don't clash with high registers which the register allocator currently guarantees.
        locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      } else {
        locations->SetInAt(1, Location::RequiresRegister());
        locations->AddTemp(Location::RequiresRegister());
        locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << op->GetResultType();
  }
}

void InstructionCodeGeneratorARMVIXL::HandleShift(HBinaryOperation* op) {
  DCHECK(op->IsShl() || op->IsShr() || op->IsUShr());

  LocationSummary* locations = op->GetLocations();
  Location out = locations->Out();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);

  DataType::Type type = op->GetResultType();
  switch (type) {
    case DataType::Type::kInt32: {
      vixl32::Register out_reg = OutputRegister(op);
      vixl32::Register first_reg = InputRegisterAt(op, 0);
      if (second.IsRegister()) {
        vixl32::Register second_reg = RegisterFrom(second);
        // ARM doesn't mask the shift count so we need to do it ourselves.
        __ And(out_reg, second_reg, kMaxIntShiftDistance);
        if (op->IsShl()) {
          __ Lsl(out_reg, first_reg, out_reg);
        } else if (op->IsShr()) {
          __ Asr(out_reg, first_reg, out_reg);
        } else {
          __ Lsr(out_reg, first_reg, out_reg);
        }
      } else {
        int32_t cst = Int32ConstantFrom(second);
        uint32_t shift_value = cst & kMaxIntShiftDistance;
        if (shift_value == 0) {  // ARM does not support shifting with 0 immediate.
          __ Mov(out_reg, first_reg);
        } else if (op->IsShl()) {
          __ Lsl(out_reg, first_reg, shift_value);
        } else if (op->IsShr()) {
          __ Asr(out_reg, first_reg, shift_value);
        } else {
          __ Lsr(out_reg, first_reg, shift_value);
        }
      }
      break;
    }
    case DataType::Type::kInt64: {
      vixl32::Register o_h = HighRegisterFrom(out);
      vixl32::Register o_l = LowRegisterFrom(out);

      vixl32::Register high = HighRegisterFrom(first);
      vixl32::Register low = LowRegisterFrom(first);

      if (second.IsRegister()) {
        vixl32::Register temp = RegisterFrom(locations->GetTemp(0));

        vixl32::Register second_reg = RegisterFrom(second);

        if (op->IsShl()) {
          __ And(o_l, second_reg, kMaxLongShiftDistance);
          // Shift the high part
          __ Lsl(o_h, high, o_l);
          // Shift the low part and `or` what overflew on the high part
          __ Rsb(temp, o_l, Operand::From(kArmBitsPerWord));
          __ Lsr(temp, low, temp);
          __ Orr(o_h, o_h, temp);
          // If the shift is > 32 bits, override the high part
          __ Subs(temp, o_l, Operand::From(kArmBitsPerWord));
          {
            ExactAssemblyScope guard(GetVIXLAssembler(),
                                     2 * vixl32::kMaxInstructionSizeInBytes,
                                     CodeBufferCheckScope::kMaximumSize);
            __ it(pl);
            __ lsl(pl, o_h, low, temp);
          }
          // Shift the low part
          __ Lsl(o_l, low, o_l);
        } else if (op->IsShr()) {
          __ And(o_h, second_reg, kMaxLongShiftDistance);
          // Shift the low part
          __ Lsr(o_l, low, o_h);
          // Shift the high part and `or` what underflew on the low part
          __ Rsb(temp, o_h, Operand::From(kArmBitsPerWord));
          __ Lsl(temp, high, temp);
          __ Orr(o_l, o_l, temp);
          // If the shift is > 32 bits, override the low part
          __ Subs(temp, o_h, Operand::From(kArmBitsPerWord));
          {
            ExactAssemblyScope guard(GetVIXLAssembler(),
                                     2 * vixl32::kMaxInstructionSizeInBytes,
                                     CodeBufferCheckScope::kMaximumSize);
            __ it(pl);
            __ asr(pl, o_l, high, temp);
          }
          // Shift the high part
          __ Asr(o_h, high, o_h);
        } else {
          __ And(o_h, second_reg, kMaxLongShiftDistance);
          // same as Shr except we use `Lsr`s and not `Asr`s
          __ Lsr(o_l, low, o_h);
          __ Rsb(temp, o_h, Operand::From(kArmBitsPerWord));
          __ Lsl(temp, high, temp);
          __ Orr(o_l, o_l, temp);
          __ Subs(temp, o_h, Operand::From(kArmBitsPerWord));
          {
            ExactAssemblyScope guard(GetVIXLAssembler(),
                                     2 * vixl32::kMaxInstructionSizeInBytes,
                                     CodeBufferCheckScope::kMaximumSize);
          __ it(pl);
          __ lsr(pl, o_l, high, temp);
          }
          __ Lsr(o_h, high, o_h);
        }
      } else {
        // Register allocator doesn't create partial overlap.
        DCHECK(!o_l.Is(high));
        DCHECK(!o_h.Is(low));
        int32_t cst = Int32ConstantFrom(second);
        uint32_t shift_value = cst & kMaxLongShiftDistance;
        if (shift_value > 32) {
          if (op->IsShl()) {
            __ Lsl(o_h, low, shift_value - 32);
            __ Mov(o_l, 0);
          } else if (op->IsShr()) {
            __ Asr(o_l, high, shift_value - 32);
            __ Asr(o_h, high, 31);
          } else {
            __ Lsr(o_l, high, shift_value - 32);
            __ Mov(o_h, 0);
          }
        } else if (shift_value == 32) {
          if (op->IsShl()) {
            __ Mov(o_h, low);
            __ Mov(o_l, 0);
          } else if (op->IsShr()) {
            __ Mov(o_l, high);
            __ Asr(o_h, high, 31);
          } else {
            __ Mov(o_l, high);
            __ Mov(o_h, 0);
          }
        } else if (shift_value == 1) {
          if (op->IsShl()) {
            __ Lsls(o_l, low, 1);
            __ Adc(o_h, high, high);
          } else if (op->IsShr()) {
            __ Asrs(o_h, high, 1);
            __ Rrx(o_l, low);
          } else {
            __ Lsrs(o_h, high, 1);
            __ Rrx(o_l, low);
          }
        } else {
          DCHECK(2 <= shift_value && shift_value < 32) << shift_value;
          if (op->IsShl()) {
            __ Lsl(o_h, high, shift_value);
            __ Orr(o_h, o_h, Operand(low, ShiftType::LSR, 32 - shift_value));
            __ Lsl(o_l, low, shift_value);
          } else if (op->IsShr()) {
            __ Lsr(o_l, low, shift_value);
            __ Orr(o_l, o_l, Operand(high, ShiftType::LSL, 32 - shift_value));
            __ Asr(o_h, high, shift_value);
          } else {
            __ Lsr(o_l, low, shift_value);
            __ Orr(o_l, o_l, Operand(high, ShiftType::LSL, 32 - shift_value));
            __ Lsr(o_h, high, shift_value);
          }
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << type;
      UNREACHABLE();
  }
}

void LocationsBuilderARMVIXL::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void InstructionCodeGeneratorARMVIXL::VisitShl(HShl* shl) {
  HandleShift(shl);
}

void LocationsBuilderARMVIXL::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void InstructionCodeGeneratorARMVIXL::VisitShr(HShr* shr) {
  HandleShift(shr);
}

void LocationsBuilderARMVIXL::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void InstructionCodeGeneratorARMVIXL::VisitUShr(HUShr* ushr) {
  HandleShift(ushr);
}

void LocationsBuilderARMVIXL::VisitNewInstance(HNewInstance* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  if (instruction->IsStringAlloc()) {
    locations->AddTemp(LocationFrom(kMethodRegister));
  } else {
    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  }
  locations->SetOut(LocationFrom(r0));
}

void InstructionCodeGeneratorARMVIXL::VisitNewInstance(HNewInstance* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes cares
  // of poisoning the reference.
  if (instruction->IsStringAlloc()) {
    // String is allocated through StringFactory. Call NewEmptyString entry point.
    vixl32::Register temp = RegisterFrom(instruction->GetLocations()->GetTemp(0));
    MemberOffset code_offset = ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArmPointerSize);
    GetAssembler()->LoadFromOffset(kLoadWord, temp, tr, QUICK_ENTRY_POINT(pNewEmptyString));
    GetAssembler()->LoadFromOffset(kLoadWord, lr, temp, code_offset.Int32Value());
    // blx in T32 has only 16bit encoding that's why a stricter check for the scope is used.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::k16BitT32InstructionSizeInBytes,
                           CodeBufferCheckScope::kExactSize);
    __ blx(lr);
    codegen_->RecordPcInfo(instruction, instruction->GetDexPc());
  } else {
    codegen_->InvokeRuntime(instruction->GetEntrypoint(), instruction, instruction->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 10);
}

void LocationsBuilderARMVIXL::VisitNewArray(HNewArray* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetOut(LocationFrom(r0));
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
}

void InstructionCodeGeneratorARMVIXL::VisitNewArray(HNewArray* instruction) {
  // Note: if heap poisoning is enabled, the entry point takes cares
  // of poisoning the reference.
  QuickEntrypointEnum entrypoint =
      CodeGenerator::GetArrayAllocationEntrypoint(instruction->GetLoadClass()->GetClass());
  codegen_->InvokeRuntime(entrypoint, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickAllocArrayResolved, void*, mirror::Class*, int32_t>();
  DCHECK(!codegen_->IsLeafMethod());
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 11);
}

void LocationsBuilderARMVIXL::VisitParameterValue(HParameterValue* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  Location location = parameter_visitor_.GetNextLocation(instruction->GetType());
  if (location.IsStackSlot()) {
    location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  } else if (location.IsDoubleStackSlot()) {
    location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
  }
  locations->SetOut(location);
}

void InstructionCodeGeneratorARMVIXL::VisitParameterValue(
    HParameterValue* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, the parameter is already at its location.
}

void LocationsBuilderARMVIXL::VisitCurrentMethod(HCurrentMethod* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetOut(LocationFrom(kMethodRegister));
}

void InstructionCodeGeneratorARMVIXL::VisitCurrentMethod(
    HCurrentMethod* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, the method is already at its location.
}

void LocationsBuilderARMVIXL::VisitNot(HNot* not_) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(not_, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitNot(HNot* not_) {
  LocationSummary* locations = not_->GetLocations();
  Location out = locations->Out();
  Location in = locations->InAt(0);
  switch (not_->GetResultType()) {
    case DataType::Type::kInt32:
      __ Mvn(OutputRegister(not_), InputRegisterAt(not_, 0));
      break;

    case DataType::Type::kInt64:
      __ Mvn(LowRegisterFrom(out), LowRegisterFrom(in));
      __ Mvn(HighRegisterFrom(out), HighRegisterFrom(in));
      break;

    default:
      LOG(FATAL) << "Unimplemented type for not operation " << not_->GetResultType();
  }
}

void LocationsBuilderARMVIXL::VisitBooleanNot(HBooleanNot* bool_not) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(bool_not, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitBooleanNot(HBooleanNot* bool_not) {
  __ Eor(OutputRegister(bool_not), InputRegister(bool_not), 1);
}

void LocationsBuilderARMVIXL::VisitCompare(HCompare* compare) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(compare, LocationSummary::kNoCall);
  switch (compare->InputAt(0)->GetType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64: {
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RequiresRegister());
      // Output overlaps because it is written before doing the low comparison.
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, ArithmeticZeroOrFpuRegister(compare->InputAt(1)));
      locations->SetOut(Location::RequiresRegister());
      break;
    }
    default:
      LOG(FATAL) << "Unexpected type for compare operation " << compare->InputAt(0)->GetType();
  }
}

void InstructionCodeGeneratorARMVIXL::VisitCompare(HCompare* compare) {
  LocationSummary* locations = compare->GetLocations();
  vixl32::Register out = OutputRegister(compare);
  Location left = locations->InAt(0);
  Location right = locations->InAt(1);

  vixl32::Label less, greater, done;
  vixl32::Label* final_label = codegen_->GetFinalLabel(compare, &done);
  DataType::Type type = compare->InputAt(0)->GetType();
  vixl32::Condition less_cond = vixl32::Condition::None();
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      // Emit move to `out` before the `Cmp`, as `Mov` might affect the status flags.
      __ Mov(out, 0);
      __ Cmp(RegisterFrom(left), RegisterFrom(right));  // Signed compare.
      less_cond = lt;
      break;
    }
    case DataType::Type::kInt64: {
      __ Cmp(HighRegisterFrom(left), HighRegisterFrom(right));  // Signed compare.
      __ B(lt, &less, /* far_target */ false);
      __ B(gt, &greater, /* far_target */ false);
      // Emit move to `out` before the last `Cmp`, as `Mov` might affect the status flags.
      __ Mov(out, 0);
      __ Cmp(LowRegisterFrom(left), LowRegisterFrom(right));  // Unsigned compare.
      less_cond = lo;
      break;
    }
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64: {
      __ Mov(out, 0);
      GenerateVcmp(compare, codegen_);
      // To branch on the FP compare result we transfer FPSCR to APSR (encoded as PC in VMRS).
      __ Vmrs(RegisterOrAPSR_nzcv(kPcCode), FPSCR);
      less_cond = ARMFPCondition(kCondLT, compare->IsGtBias());
      break;
    }
    default:
      LOG(FATAL) << "Unexpected compare type " << type;
      UNREACHABLE();
  }

  __ B(eq, final_label, /* far_target */ false);
  __ B(less_cond, &less, /* far_target */ false);

  __ Bind(&greater);
  __ Mov(out, 1);
  __ B(final_label);

  __ Bind(&less);
  __ Mov(out, -1);

  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

void LocationsBuilderARMVIXL::VisitPhi(HPhi* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  for (size_t i = 0, e = locations->GetInputCount(); i < e; ++i) {
    locations->SetInAt(i, Location::Any());
  }
  locations->SetOut(Location::Any());
}

void InstructionCodeGeneratorARMVIXL::VisitPhi(HPhi* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void CodeGeneratorARMVIXL::GenerateMemoryBarrier(MemBarrierKind kind) {
  // TODO (ported from quick): revisit ARM barrier kinds.
  DmbOptions flavor = DmbOptions::ISH;  // Quiet C++ warnings.
  switch (kind) {
    case MemBarrierKind::kAnyStore:
    case MemBarrierKind::kLoadAny:
    case MemBarrierKind::kAnyAny: {
      flavor = DmbOptions::ISH;
      break;
    }
    case MemBarrierKind::kStoreStore: {
      flavor = DmbOptions::ISHST;
      break;
    }
    default:
      LOG(FATAL) << "Unexpected memory barrier " << kind;
  }
  __ Dmb(flavor);
}

void InstructionCodeGeneratorARMVIXL::GenerateWideAtomicLoad(vixl32::Register addr,
                                                             uint32_t offset,
                                                             vixl32::Register out_lo,
                                                             vixl32::Register out_hi) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  if (offset != 0) {
    vixl32::Register temp = temps.Acquire();
    __ Add(temp, addr, offset);
    addr = temp;
  }
  __ Ldrexd(out_lo, out_hi, MemOperand(addr));
}

void InstructionCodeGeneratorARMVIXL::GenerateWideAtomicStore(vixl32::Register addr,
                                                              uint32_t offset,
                                                              vixl32::Register value_lo,
                                                              vixl32::Register value_hi,
                                                              vixl32::Register temp1,
                                                              vixl32::Register temp2,
                                                              HInstruction* instruction) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  vixl32::Label fail;
  if (offset != 0) {
    vixl32::Register temp = temps.Acquire();
    __ Add(temp, addr, offset);
    addr = temp;
  }
  __ Bind(&fail);
  {
    // Ensure the pc position is recorded immediately after the `ldrexd` instruction.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    // We need a load followed by store. (The address used in a STREX instruction must
    // be the same as the address in the most recently executed LDREX instruction.)
    __ ldrexd(temp1, temp2, MemOperand(addr));
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
  __ Strexd(temp1, value_lo, value_hi, MemOperand(addr));
  __ CompareAndBranchIfNonZero(temp1, &fail);
}

void LocationsBuilderARMVIXL::HandleFieldSet(
    HInstruction* instruction, const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());

  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());

  DataType::Type field_type = field_info.GetFieldType();
  if (DataType::IsFloatingPointType(field_type)) {
    locations->SetInAt(1, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(1, Location::RequiresRegister());
  }

  bool is_wide = field_type == DataType::Type::kInt64 || field_type == DataType::Type::kFloat64;
  bool generate_volatile = field_info.IsVolatile()
      && is_wide
      && !codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1));
  // Temporary registers for the write barrier.
  // TODO: consider renaming StoreNeedsWriteBarrier to StoreNeedsGCMark.
  if (needs_write_barrier) {
    locations->AddTemp(Location::RequiresRegister());  // Possibly used for reference poisoning too.
    locations->AddTemp(Location::RequiresRegister());
  } else if (generate_volatile) {
    // ARM encoding have some additional constraints for ldrexd/strexd:
    // - registers need to be consecutive
    // - the first register should be even but not R14.
    // We don't test for ARM yet, and the assertion makes sure that we
    // revisit this if we ever enable ARM encoding.
    DCHECK_EQ(InstructionSet::kThumb2, codegen_->GetInstructionSet());

    locations->AddTemp(Location::RequiresRegister());
    locations->AddTemp(Location::RequiresRegister());
    if (field_type == DataType::Type::kFloat64) {
      // For doubles we need two more registers to copy the value.
      locations->AddTemp(LocationFrom(r2));
      locations->AddTemp(LocationFrom(r3));
    }
  }
}

void InstructionCodeGeneratorARMVIXL::HandleFieldSet(HInstruction* instruction,
                                                     const FieldInfo& field_info,
                                                     bool value_can_be_null) {
  DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());

  LocationSummary* locations = instruction->GetLocations();
  vixl32::Register base = InputRegisterAt(instruction, 0);
  Location value = locations->InAt(1);

  bool is_volatile = field_info.IsVolatile();
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();
  DataType::Type field_type = field_info.GetFieldType();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1));

  if (is_volatile) {
    codegen_->GenerateMemoryBarrier(MemBarrierKind::kAnyStore);
  }

  switch (field_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      StoreOperandType operand_type = GetStoreOperandType(field_type);
      GetAssembler()->StoreToOffset(operand_type, RegisterFrom(value), base, offset);
      break;
    }

    case DataType::Type::kReference: {
      if (kPoisonHeapReferences && needs_write_barrier) {
        // Note that in the case where `value` is a null reference,
        // we do not enter this block, as a null reference does not
        // need poisoning.
        DCHECK_EQ(field_type, DataType::Type::kReference);
        vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
        __ Mov(temp, RegisterFrom(value));
        GetAssembler()->PoisonHeapReference(temp);
        GetAssembler()->StoreToOffset(kStoreWord, temp, base, offset);
      } else {
        GetAssembler()->StoreToOffset(kStoreWord, RegisterFrom(value), base, offset);
      }
      break;
    }

    case DataType::Type::kInt64: {
      if (is_volatile && !atomic_ldrd_strd) {
        GenerateWideAtomicStore(base,
                                offset,
                                LowRegisterFrom(value),
                                HighRegisterFrom(value),
                                RegisterFrom(locations->GetTemp(0)),
                                RegisterFrom(locations->GetTemp(1)),
                                instruction);
      } else {
        GetAssembler()->StoreToOffset(kStoreWordPair, LowRegisterFrom(value), base, offset);
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      GetAssembler()->StoreSToOffset(SRegisterFrom(value), base, offset);
      break;
    }

    case DataType::Type::kFloat64: {
      vixl32::DRegister value_reg = DRegisterFrom(value);
      if (is_volatile && !atomic_ldrd_strd) {
        vixl32::Register value_reg_lo = RegisterFrom(locations->GetTemp(0));
        vixl32::Register value_reg_hi = RegisterFrom(locations->GetTemp(1));

        __ Vmov(value_reg_lo, value_reg_hi, value_reg);

        GenerateWideAtomicStore(base,
                                offset,
                                value_reg_lo,
                                value_reg_hi,
                                RegisterFrom(locations->GetTemp(2)),
                                RegisterFrom(locations->GetTemp(3)),
                                instruction);
      } else {
        GetAssembler()->StoreDToOffset(value_reg, base, offset);
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << field_type;
      UNREACHABLE();
  }

  // Longs and doubles are handled in the switch.
  if (field_type != DataType::Type::kInt64 && field_type != DataType::Type::kFloat64) {
    // TODO(VIXL): Here and for other calls to `MaybeRecordImplicitNullCheck` in this method, we
    // should use a scope and the assembler to emit the store instruction to guarantee that we
    // record the pc at the correct position. But the `Assembler` does not automatically handle
    // unencodable offsets. Practically, everything is fine because the helper and VIXL, at the time
    // of writing, do generate the store instruction last.
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }

  if (CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1))) {
    vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
    vixl32::Register card = RegisterFrom(locations->GetTemp(1));
    codegen_->MarkGCCard(temp, card, base, RegisterFrom(value), value_can_be_null);
  }

  if (is_volatile) {
    codegen_->GenerateMemoryBarrier(MemBarrierKind::kAnyAny);
  }
}

void LocationsBuilderARMVIXL::HandleFieldGet(HInstruction* instruction,
                                             const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());

  bool object_field_get_with_read_barrier =
      kEmitCompilerReadBarrier && (field_info.GetFieldType() == DataType::Type::kReference);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction,
                                                       object_field_get_with_read_barrier
                                                           ? LocationSummary::kCallOnSlowPath
                                                           : LocationSummary::kNoCall);
  if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
  }
  locations->SetInAt(0, Location::RequiresRegister());

  bool volatile_for_double = field_info.IsVolatile()
      && (field_info.GetFieldType() == DataType::Type::kFloat64)
      && !codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();
  // The output overlaps in case of volatile long: we don't want the
  // code generated by GenerateWideAtomicLoad to overwrite the
  // object's location.  Likewise, in the case of an object field get
  // with read barriers enabled, we do not want the load to overwrite
  // the object's location, as we need it to emit the read barrier.
  bool overlap =
      (field_info.IsVolatile() && (field_info.GetFieldType() == DataType::Type::kInt64)) ||
      object_field_get_with_read_barrier;

  if (DataType::IsFloatingPointType(instruction->GetType())) {
    locations->SetOut(Location::RequiresFpuRegister());
  } else {
    locations->SetOut(Location::RequiresRegister(),
                      (overlap ? Location::kOutputOverlap : Location::kNoOutputOverlap));
  }
  if (volatile_for_double) {
    // ARM encoding have some additional constraints for ldrexd/strexd:
    // - registers need to be consecutive
    // - the first register should be even but not R14.
    // We don't test for ARM yet, and the assertion makes sure that we
    // revisit this if we ever enable ARM encoding.
    DCHECK_EQ(InstructionSet::kThumb2, codegen_->GetInstructionSet());
    locations->AddTemp(Location::RequiresRegister());
    locations->AddTemp(Location::RequiresRegister());
  } else if (object_field_get_with_read_barrier && kUseBakerReadBarrier) {
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARMVIXL::GenerateFieldLoadWithBakerReadBarrier.
    if (kBakerReadBarrierLinkTimeThunksEnableForFields &&
        !Runtime::Current()->UseJitCompilation()) {
      // If link-time thunks for the Baker read barrier are enabled, for AOT
      // loads we need a temporary only if the offset is too big.
      if (field_info.GetFieldOffset().Uint32Value() >= kReferenceLoadMinFarOffset) {
        locations->AddTemp(Location::RequiresRegister());
      }
      // And we always need the reserved entrypoint register.
      locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
    } else {
      locations->AddTemp(Location::RequiresRegister());
    }
  }
}

Location LocationsBuilderARMVIXL::ArithmeticZeroOrFpuRegister(HInstruction* input) {
  DCHECK(DataType::IsFloatingPointType(input->GetType())) << input->GetType();
  if ((input->IsFloatConstant() && (input->AsFloatConstant()->IsArithmeticZero())) ||
      (input->IsDoubleConstant() && (input->AsDoubleConstant()->IsArithmeticZero()))) {
    return Location::ConstantLocation(input->AsConstant());
  } else {
    return Location::RequiresFpuRegister();
  }
}

Location LocationsBuilderARMVIXL::ArmEncodableConstantOrRegister(HInstruction* constant,
                                                                 Opcode opcode) {
  DCHECK(!DataType::IsFloatingPointType(constant->GetType()));
  if (constant->IsConstant() &&
      CanEncodeConstantAsImmediate(constant->AsConstant(), opcode)) {
    return Location::ConstantLocation(constant->AsConstant());
  }
  return Location::RequiresRegister();
}

static bool CanEncode32BitConstantAsImmediate(
    CodeGeneratorARMVIXL* codegen,
    uint32_t value,
    Opcode opcode,
    vixl32::FlagsUpdate flags_update = vixl32::FlagsUpdate::DontCare) {
  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  if (assembler->ShifterOperandCanHold(opcode, value, flags_update)) {
    return true;
  }
  Opcode neg_opcode = kNoOperand;
  uint32_t neg_value = 0;
  switch (opcode) {
    case AND: neg_opcode = BIC; neg_value = ~value; break;
    case ORR: neg_opcode = ORN; neg_value = ~value; break;
    case ADD: neg_opcode = SUB; neg_value = -value; break;
    case ADC: neg_opcode = SBC; neg_value = ~value; break;
    case SUB: neg_opcode = ADD; neg_value = -value; break;
    case SBC: neg_opcode = ADC; neg_value = ~value; break;
    case MOV: neg_opcode = MVN; neg_value = ~value; break;
    default:
      return false;
  }

  if (assembler->ShifterOperandCanHold(neg_opcode, neg_value, flags_update)) {
    return true;
  }

  return opcode == AND && IsPowerOfTwo(value + 1);
}

bool LocationsBuilderARMVIXL::CanEncodeConstantAsImmediate(HConstant* input_cst, Opcode opcode) {
  uint64_t value = static_cast<uint64_t>(Int64FromConstant(input_cst));
  if (DataType::Is64BitType(input_cst->GetType())) {
    Opcode high_opcode = opcode;
    vixl32::FlagsUpdate low_flags_update = vixl32::FlagsUpdate::DontCare;
    switch (opcode) {
      case SUB:
        // Flip the operation to an ADD.
        value = -value;
        opcode = ADD;
        FALLTHROUGH_INTENDED;
      case ADD:
        if (Low32Bits(value) == 0u) {
          return CanEncode32BitConstantAsImmediate(codegen_, High32Bits(value), opcode);
        }
        high_opcode = ADC;
        low_flags_update = vixl32::FlagsUpdate::SetFlags;
        break;
      default:
        break;
    }
    return CanEncode32BitConstantAsImmediate(codegen_, High32Bits(value), high_opcode) &&
           CanEncode32BitConstantAsImmediate(codegen_, Low32Bits(value), opcode, low_flags_update);
  } else {
    return CanEncode32BitConstantAsImmediate(codegen_, Low32Bits(value), opcode);
  }
}

void InstructionCodeGeneratorARMVIXL::HandleFieldGet(HInstruction* instruction,
                                                     const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());

  LocationSummary* locations = instruction->GetLocations();
  vixl32::Register base = InputRegisterAt(instruction, 0);
  Location out = locations->Out();
  bool is_volatile = field_info.IsVolatile();
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();
  DCHECK_EQ(DataType::Size(field_info.GetFieldType()), DataType::Size(instruction->GetType()));
  DataType::Type load_type = instruction->GetType();
  uint32_t offset = field_info.GetFieldOffset().Uint32Value();

  switch (load_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      LoadOperandType operand_type = GetLoadOperandType(load_type);
      GetAssembler()->LoadFromOffset(operand_type, RegisterFrom(out), base, offset);
      break;
    }

    case DataType::Type::kReference: {
      // /* HeapReference<Object> */ out = *(base + offset)
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        Location temp_loc = locations->GetTemp(0);
        // Note that a potential implicit null check is handled in this
        // CodeGeneratorARMVIXL::GenerateFieldLoadWithBakerReadBarrier call.
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            instruction, out, base, offset, temp_loc, /* needs_null_check */ true);
        if (is_volatile) {
          codegen_->GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
        }
      } else {
        GetAssembler()->LoadFromOffset(kLoadWord, RegisterFrom(out), base, offset);
        codegen_->MaybeRecordImplicitNullCheck(instruction);
        if (is_volatile) {
          codegen_->GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
        }
        // If read barriers are enabled, emit read barriers other than
        // Baker's using a slow path (and also unpoison the loaded
        // reference, if heap poisoning is enabled).
        codegen_->MaybeGenerateReadBarrierSlow(instruction, out, out, locations->InAt(0), offset);
      }
      break;
    }

    case DataType::Type::kInt64:
      if (is_volatile && !atomic_ldrd_strd) {
        GenerateWideAtomicLoad(base, offset, LowRegisterFrom(out), HighRegisterFrom(out));
      } else {
        GetAssembler()->LoadFromOffset(kLoadWordPair, LowRegisterFrom(out), base, offset);
      }
      break;

    case DataType::Type::kFloat32:
      GetAssembler()->LoadSFromOffset(SRegisterFrom(out), base, offset);
      break;

    case DataType::Type::kFloat64: {
      vixl32::DRegister out_dreg = DRegisterFrom(out);
      if (is_volatile && !atomic_ldrd_strd) {
        vixl32::Register lo = RegisterFrom(locations->GetTemp(0));
        vixl32::Register hi = RegisterFrom(locations->GetTemp(1));
        GenerateWideAtomicLoad(base, offset, lo, hi);
        // TODO(VIXL): Do we need to be immediately after the ldrexd instruction? If so we need a
        // scope.
        codegen_->MaybeRecordImplicitNullCheck(instruction);
        __ Vmov(out_dreg, lo, hi);
      } else {
        GetAssembler()->LoadDFromOffset(out_dreg, base, offset);
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << load_type;
      UNREACHABLE();
  }

  if (load_type == DataType::Type::kReference || load_type == DataType::Type::kFloat64) {
    // Potential implicit null checks, in the case of reference or
    // double fields, are handled in the previous switch statement.
  } else {
    // Address cases other than reference and double that may require an implicit null check.
    // TODO(VIXL): Here and for other calls to `MaybeRecordImplicitNullCheck` in this method, we
    // should use a scope and the assembler to emit the load instruction to guarantee that we
    // record the pc at the correct position. But the `Assembler` does not automatically handle
    // unencodable offsets. Practically, everything is fine because the helper and VIXL, at the time
    // of writing, do generate the store instruction last.
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }

  if (is_volatile) {
    if (load_type == DataType::Type::kReference) {
      // Memory barriers, in the case of references, are also handled
      // in the previous switch statement.
    } else {
      codegen_->GenerateMemoryBarrier(MemBarrierKind::kLoadAny);
    }
  }
}

void LocationsBuilderARMVIXL::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARMVIXL::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LocationsBuilderARMVIXL::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARMVIXL::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderARMVIXL::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARMVIXL::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LocationsBuilderARMVIXL::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo());
}

void InstructionCodeGeneratorARMVIXL::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LocationsBuilderARMVIXL::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARMVIXL::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARMVIXL::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARMVIXL::VisitUnresolvedInstanceFieldSet(
    HUnresolvedInstanceFieldSet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARMVIXL::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARMVIXL::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARMVIXL::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->CreateUnresolvedFieldLocationSummary(
      instruction, instruction->GetFieldType(), calling_convention);
}

void InstructionCodeGeneratorARMVIXL::VisitUnresolvedStaticFieldSet(
    HUnresolvedStaticFieldSet* instruction) {
  FieldAccessCallingConventionARMVIXL calling_convention;
  codegen_->GenerateUnresolvedFieldAccess(instruction,
                                          instruction->GetFieldType(),
                                          instruction->GetFieldIndex(),
                                          instruction->GetDexPc(),
                                          calling_convention);
}

void LocationsBuilderARMVIXL::VisitNullCheck(HNullCheck* instruction) {
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction);
  locations->SetInAt(0, Location::RequiresRegister());
}

void CodeGeneratorARMVIXL::GenerateImplicitNullCheck(HNullCheck* instruction) {
  if (CanMoveNullCheckToUser(instruction)) {
    return;
  }

  UseScratchRegisterScope temps(GetVIXLAssembler());
  // Ensure the pc position is recorded immediately after the `ldr` instruction.
  ExactAssemblyScope aas(GetVIXLAssembler(),
                         vixl32::kMaxInstructionSizeInBytes,
                         CodeBufferCheckScope::kMaximumSize);
  __ ldr(temps.Acquire(), MemOperand(InputRegisterAt(instruction, 0)));
  RecordPcInfo(instruction, instruction->GetDexPc());
}

void CodeGeneratorARMVIXL::GenerateExplicitNullCheck(HNullCheck* instruction) {
  NullCheckSlowPathARMVIXL* slow_path =
      new (GetScopedAllocator()) NullCheckSlowPathARMVIXL(instruction);
  AddSlowPath(slow_path);
  __ CompareAndBranchIfZero(InputRegisterAt(instruction, 0), slow_path->GetEntryLabel());
}

void InstructionCodeGeneratorARMVIXL::VisitNullCheck(HNullCheck* instruction) {
  codegen_->GenerateNullCheck(instruction);
}

void CodeGeneratorARMVIXL::LoadFromShiftedRegOffset(DataType::Type type,
                                                    Location out_loc,
                                                    vixl32::Register base,
                                                    vixl32::Register reg_index,
                                                    vixl32::Condition cond) {
  uint32_t shift_count = DataType::SizeShift(type);
  MemOperand mem_address(base, reg_index, vixl32::LSL, shift_count);

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
      __ Ldrb(cond, RegisterFrom(out_loc), mem_address);
      break;
    case DataType::Type::kInt8:
      __ Ldrsb(cond, RegisterFrom(out_loc), mem_address);
      break;
    case DataType::Type::kUint16:
      __ Ldrh(cond, RegisterFrom(out_loc), mem_address);
      break;
    case DataType::Type::kInt16:
      __ Ldrsh(cond, RegisterFrom(out_loc), mem_address);
      break;
    case DataType::Type::kReference:
    case DataType::Type::kInt32:
      __ Ldr(cond, RegisterFrom(out_loc), mem_address);
      break;
    // T32 doesn't support LoadFromShiftedRegOffset mem address mode for these types.
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

void CodeGeneratorARMVIXL::StoreToShiftedRegOffset(DataType::Type type,
                                                   Location loc,
                                                   vixl32::Register base,
                                                   vixl32::Register reg_index,
                                                   vixl32::Condition cond) {
  uint32_t shift_count = DataType::SizeShift(type);
  MemOperand mem_address(base, reg_index, vixl32::LSL, shift_count);

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      __ Strb(cond, RegisterFrom(loc), mem_address);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      __ Strh(cond, RegisterFrom(loc), mem_address);
      break;
    case DataType::Type::kReference:
    case DataType::Type::kInt32:
      __ Str(cond, RegisterFrom(loc), mem_address);
      break;
    // T32 doesn't support StoreToShiftedRegOffset mem address mode for these types.
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

void LocationsBuilderARMVIXL::VisitArrayGet(HArrayGet* instruction) {
  bool object_array_get_with_read_barrier =
      kEmitCompilerReadBarrier && (instruction->GetType() == DataType::Type::kReference);
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
  if (object_array_get_with_read_barrier && kUseBakerReadBarrier) {
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARMVIXL::GenerateArrayLoadWithBakerReadBarrier.
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
        locations->AddTemp(Location::RequiresRegister());
      }
      // And we always need the reserved entrypoint register.
      locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
    } else if (kBakerReadBarrierLinkTimeThunksEnableForArrays &&
               !Runtime::Current()->UseJitCompilation() &&
               !instruction->GetIndex()->IsConstant()) {
      // We need a non-scratch temporary for the array data pointer.
      locations->AddTemp(Location::RequiresRegister());
      // And we always need the reserved entrypoint register.
      locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
    } else {
      locations->AddTemp(Location::RequiresRegister());
    }
  } else if (mirror::kUseStringCompression && instruction->IsStringCharAt()) {
    // Also need a temporary for String compression feature.
    locations->AddTemp(Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARMVIXL::VisitArrayGet(HArrayGet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  vixl32::Register obj = InputRegisterAt(instruction, 0);
  Location index = locations->InAt(1);
  Location out_loc = locations->Out();
  uint32_t data_offset = CodeGenerator::GetArrayDataOffset(instruction);
  DataType::Type type = instruction->GetType();
  const bool maybe_compressed_char_at = mirror::kUseStringCompression &&
                                        instruction->IsStringCharAt();
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      vixl32::Register length;
      if (maybe_compressed_char_at) {
        length = RegisterFrom(locations->GetTemp(0));
        uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
        GetAssembler()->LoadFromOffset(kLoadWord, length, obj, count_offset);
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }
      if (index.IsConstant()) {
        int32_t const_index = Int32ConstantFrom(index);
        if (maybe_compressed_char_at) {
          vixl32::Label uncompressed_load, done;
          vixl32::Label* final_label = codegen_->GetFinalLabel(instruction, &done);
          __ Lsrs(length, length, 1u);  // LSRS has a 16-bit encoding, TST (immediate) does not.
          static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                        "Expecting 0=compressed, 1=uncompressed");
          __ B(cs, &uncompressed_load, /* far_target */ false);
          GetAssembler()->LoadFromOffset(kLoadUnsignedByte,
                                         RegisterFrom(out_loc),
                                         obj,
                                         data_offset + const_index);
          __ B(final_label);
          __ Bind(&uncompressed_load);
          GetAssembler()->LoadFromOffset(GetLoadOperandType(DataType::Type::kUint16),
                                         RegisterFrom(out_loc),
                                         obj,
                                         data_offset + (const_index << 1));
          if (done.IsReferenced()) {
            __ Bind(&done);
          }
        } else {
          uint32_t full_offset = data_offset + (const_index << DataType::SizeShift(type));

          LoadOperandType load_type = GetLoadOperandType(type);
          GetAssembler()->LoadFromOffset(load_type, RegisterFrom(out_loc), obj, full_offset);
        }
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();

        if (has_intermediate_address) {
          // We do not need to compute the intermediate address from the array: the
          // input instruction has done it already. See the comment in
          // `TryExtractArrayAccessAddress()`.
          if (kIsDebugBuild) {
            HIntermediateAddress* tmp = array_instr->AsIntermediateAddress();
            DCHECK_EQ(Uint64ConstantFrom(tmp->GetOffset()), data_offset);
          }
          temp = obj;
        } else {
          __ Add(temp, obj, data_offset);
        }
        if (maybe_compressed_char_at) {
          vixl32::Label uncompressed_load, done;
          vixl32::Label* final_label = codegen_->GetFinalLabel(instruction, &done);
          __ Lsrs(length, length, 1u);  // LSRS has a 16-bit encoding, TST (immediate) does not.
          static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                        "Expecting 0=compressed, 1=uncompressed");
          __ B(cs, &uncompressed_load, /* far_target */ false);
          __ Ldrb(RegisterFrom(out_loc), MemOperand(temp, RegisterFrom(index), vixl32::LSL, 0));
          __ B(final_label);
          __ Bind(&uncompressed_load);
          __ Ldrh(RegisterFrom(out_loc), MemOperand(temp, RegisterFrom(index), vixl32::LSL, 1));
          if (done.IsReferenced()) {
            __ Bind(&done);
          }
        } else {
          codegen_->LoadFromShiftedRegOffset(type, out_loc, temp, RegisterFrom(index));
        }
      }
      break;
    }

    case DataType::Type::kReference: {
      // The read barrier instrumentation of object ArrayGet
      // instructions does not support the HIntermediateAddress
      // instruction.
      DCHECK(!(has_intermediate_address && kEmitCompilerReadBarrier));

      static_assert(
          sizeof(mirror::HeapReference<mirror::Object>) == sizeof(int32_t),
          "art::mirror::HeapReference<art::mirror::Object> and int32_t have different sizes.");
      // /* HeapReference<Object> */ out =
      //     *(obj + data_offset + index * sizeof(HeapReference<Object>))
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        Location temp = locations->GetTemp(0);
        // Note that a potential implicit null check is handled in this
        // CodeGeneratorARMVIXL::GenerateArrayLoadWithBakerReadBarrier call.
        DCHECK(!instruction->CanDoImplicitNullCheckOn(instruction->InputAt(0)));
        if (index.IsConstant()) {
          // Array load with a constant index can be treated as a field load.
          data_offset += Int32ConstantFrom(index) << DataType::SizeShift(type);
          codegen_->GenerateFieldLoadWithBakerReadBarrier(instruction,
                                                          out_loc,
                                                          obj,
                                                          data_offset,
                                                          locations->GetTemp(0),
                                                          /* needs_null_check */ false);
        } else {
          codegen_->GenerateArrayLoadWithBakerReadBarrier(
              instruction, out_loc, obj, data_offset, index, temp, /* needs_null_check */ false);
        }
      } else {
        vixl32::Register out = OutputRegister(instruction);
        if (index.IsConstant()) {
          size_t offset =
              (Int32ConstantFrom(index) << TIMES_4) + data_offset;
          GetAssembler()->LoadFromOffset(kLoadWord, out, obj, offset);
          // TODO(VIXL): Here and for other calls to `MaybeRecordImplicitNullCheck` in this method,
          // we should use a scope and the assembler to emit the load instruction to guarantee that
          // we record the pc at the correct position. But the `Assembler` does not automatically
          // handle unencodable offsets. Practically, everything is fine because the helper and
          // VIXL, at the time of writing, do generate the store instruction last.
          codegen_->MaybeRecordImplicitNullCheck(instruction);
          // If read barriers are enabled, emit read barriers other than
          // Baker's using a slow path (and also unpoison the loaded
          // reference, if heap poisoning is enabled).
          codegen_->MaybeGenerateReadBarrierSlow(instruction, out_loc, out_loc, obj_loc, offset);
        } else {
          UseScratchRegisterScope temps(GetVIXLAssembler());
          vixl32::Register temp = temps.Acquire();

          if (has_intermediate_address) {
            // We do not need to compute the intermediate address from the array: the
            // input instruction has done it already. See the comment in
            // `TryExtractArrayAccessAddress()`.
            if (kIsDebugBuild) {
              HIntermediateAddress* tmp = array_instr->AsIntermediateAddress();
              DCHECK_EQ(Uint64ConstantFrom(tmp->GetOffset()), data_offset);
            }
            temp = obj;
          } else {
            __ Add(temp, obj, data_offset);
          }
          codegen_->LoadFromShiftedRegOffset(type, out_loc, temp, RegisterFrom(index));
          temps.Close();
          // TODO(VIXL): Use a scope to ensure that we record the pc position immediately after the
          // load instruction. Practically, everything is fine because the helper and VIXL, at the
          // time of writing, do generate the store instruction last.
          codegen_->MaybeRecordImplicitNullCheck(instruction);
          // If read barriers are enabled, emit read barriers other than
          // Baker's using a slow path (and also unpoison the loaded
          // reference, if heap poisoning is enabled).
          codegen_->MaybeGenerateReadBarrierSlow(
              instruction, out_loc, out_loc, obj_loc, data_offset, index);
        }
      }
      break;
    }

    case DataType::Type::kInt64: {
      if (index.IsConstant()) {
        size_t offset =
            (Int32ConstantFrom(index) << TIMES_8) + data_offset;
        GetAssembler()->LoadFromOffset(kLoadWordPair, LowRegisterFrom(out_loc), obj, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, obj, Operand(RegisterFrom(index), vixl32::LSL, TIMES_8));
        GetAssembler()->LoadFromOffset(kLoadWordPair, LowRegisterFrom(out_loc), temp, data_offset);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      vixl32::SRegister out = SRegisterFrom(out_loc);
      if (index.IsConstant()) {
        size_t offset = (Int32ConstantFrom(index) << TIMES_4) + data_offset;
        GetAssembler()->LoadSFromOffset(out, obj, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, obj, Operand(RegisterFrom(index), vixl32::LSL, TIMES_4));
        GetAssembler()->LoadSFromOffset(out, temp, data_offset);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      if (index.IsConstant()) {
        size_t offset = (Int32ConstantFrom(index) << TIMES_8) + data_offset;
        GetAssembler()->LoadDFromOffset(DRegisterFrom(out_loc), obj, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, obj, Operand(RegisterFrom(index), vixl32::LSL, TIMES_8));
        GetAssembler()->LoadDFromOffset(DRegisterFrom(out_loc), temp, data_offset);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }

  if (type == DataType::Type::kReference) {
    // Potential implicit null checks, in the case of reference
    // arrays, are handled in the previous switch statement.
  } else if (!maybe_compressed_char_at) {
    // TODO(VIXL): Use a scope to ensure we record the pc info immediately after
    // the preceding load instruction.
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
}

void LocationsBuilderARMVIXL::VisitArraySet(HArraySet* instruction) {
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
  if (DataType::IsFloatingPointType(value_type)) {
    locations->SetInAt(2, Location::RequiresFpuRegister());
  } else {
    locations->SetInAt(2, Location::RequiresRegister());
  }
  if (needs_write_barrier) {
    // Temporary registers for the write barrier.
    locations->AddTemp(Location::RequiresRegister());  // Possibly used for ref. poisoning too.
    locations->AddTemp(Location::RequiresRegister());
  }
}

void InstructionCodeGeneratorARMVIXL::VisitArraySet(HArraySet* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  vixl32::Register array = InputRegisterAt(instruction, 0);
  Location index = locations->InAt(1);
  DataType::Type value_type = instruction->GetComponentType();
  bool may_need_runtime_call_for_type_check = instruction->NeedsTypeCheck();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(value_type, instruction->GetValue());
  uint32_t data_offset =
      mirror::Array::DataOffset(DataType::Size(value_type)).Uint32Value();
  Location value_loc = locations->InAt(2);
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();

  switch (value_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      if (index.IsConstant()) {
        int32_t const_index = Int32ConstantFrom(index);
        uint32_t full_offset =
            data_offset + (const_index << DataType::SizeShift(value_type));
        StoreOperandType store_type = GetStoreOperandType(value_type);
        GetAssembler()->StoreToOffset(store_type, RegisterFrom(value_loc), array, full_offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();

        if (has_intermediate_address) {
          // We do not need to compute the intermediate address from the array: the
          // input instruction has done it already. See the comment in
          // `TryExtractArrayAccessAddress()`.
          if (kIsDebugBuild) {
            HIntermediateAddress* tmp = array_instr->AsIntermediateAddress();
            DCHECK_EQ(Uint64ConstantFrom(tmp->GetOffset()), data_offset);
          }
          temp = array;
        } else {
          __ Add(temp, array, data_offset);
        }
        codegen_->StoreToShiftedRegOffset(value_type, value_loc, temp, RegisterFrom(index));
      }
      break;
    }

    case DataType::Type::kReference: {
      vixl32::Register value = RegisterFrom(value_loc);
      // TryExtractArrayAccessAddress optimization is never applied for non-primitive ArraySet.
      // See the comment in instruction_simplifier_shared.cc.
      DCHECK(!has_intermediate_address);

      if (instruction->InputAt(2)->IsNullConstant()) {
        // Just setting null.
        if (index.IsConstant()) {
          size_t offset =
              (Int32ConstantFrom(index) << TIMES_4) + data_offset;
          GetAssembler()->StoreToOffset(kStoreWord, value, array, offset);
        } else {
          DCHECK(index.IsRegister()) << index;
          UseScratchRegisterScope temps(GetVIXLAssembler());
          vixl32::Register temp = temps.Acquire();
          __ Add(temp, array, data_offset);
          codegen_->StoreToShiftedRegOffset(value_type, value_loc, temp, RegisterFrom(index));
        }
        // TODO(VIXL): Use a scope to ensure we record the pc info immediately after the preceding
        // store instruction.
        codegen_->MaybeRecordImplicitNullCheck(instruction);
        DCHECK(!needs_write_barrier);
        DCHECK(!may_need_runtime_call_for_type_check);
        break;
      }

      DCHECK(needs_write_barrier);
      Location temp1_loc = locations->GetTemp(0);
      vixl32::Register temp1 = RegisterFrom(temp1_loc);
      Location temp2_loc = locations->GetTemp(1);
      vixl32::Register temp2 = RegisterFrom(temp2_loc);
      uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
      uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
      uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
      vixl32::Label done;
      vixl32::Label* final_label = codegen_->GetFinalLabel(instruction, &done);
      SlowPathCodeARMVIXL* slow_path = nullptr;

      if (may_need_runtime_call_for_type_check) {
        slow_path = new (codegen_->GetScopedAllocator()) ArraySetSlowPathARMVIXL(instruction);
        codegen_->AddSlowPath(slow_path);
        if (instruction->GetValueCanBeNull()) {
          vixl32::Label non_zero;
          __ CompareAndBranchIfNonZero(value, &non_zero);
          if (index.IsConstant()) {
            size_t offset =
               (Int32ConstantFrom(index) << TIMES_4) + data_offset;
            GetAssembler()->StoreToOffset(kStoreWord, value, array, offset);
          } else {
            DCHECK(index.IsRegister()) << index;
            UseScratchRegisterScope temps(GetVIXLAssembler());
            vixl32::Register temp = temps.Acquire();
            __ Add(temp, array, data_offset);
            codegen_->StoreToShiftedRegOffset(value_type, value_loc, temp, RegisterFrom(index));
          }
          // TODO(VIXL): Use a scope to ensure we record the pc info immediately after the preceding
          // store instruction.
          codegen_->MaybeRecordImplicitNullCheck(instruction);
          __ B(final_label);
          __ Bind(&non_zero);
        }

        // Note that when read barriers are enabled, the type checks
        // are performed without read barriers.  This is fine, even in
        // the case where a class object is in the from-space after
        // the flip, as a comparison involving such a type would not
        // produce a false positive; it may of course produce a false
        // negative, in which case we would take the ArraySet slow
        // path.

        {
          // Ensure we record the pc position immediately after the `ldr` instruction.
          ExactAssemblyScope aas(GetVIXLAssembler(),
                                 vixl32::kMaxInstructionSizeInBytes,
                                 CodeBufferCheckScope::kMaximumSize);
          // /* HeapReference<Class> */ temp1 = array->klass_
          __ ldr(temp1, MemOperand(array, class_offset));
          codegen_->MaybeRecordImplicitNullCheck(instruction);
        }
        GetAssembler()->MaybeUnpoisonHeapReference(temp1);

        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        GetAssembler()->LoadFromOffset(kLoadWord, temp1, temp1, component_offset);
        // /* HeapReference<Class> */ temp2 = value->klass_
        GetAssembler()->LoadFromOffset(kLoadWord, temp2, value, class_offset);
        // If heap poisoning is enabled, no need to unpoison `temp1`
        // nor `temp2`, as we are comparing two poisoned references.
        __ Cmp(temp1, temp2);

        if (instruction->StaticTypeOfArrayIsObjectArray()) {
          vixl32::Label do_put;
          __ B(eq, &do_put, /* far_target */ false);
          // If heap poisoning is enabled, the `temp1` reference has
          // not been unpoisoned yet; unpoison it now.
          GetAssembler()->MaybeUnpoisonHeapReference(temp1);

          // /* HeapReference<Class> */ temp1 = temp1->super_class_
          GetAssembler()->LoadFromOffset(kLoadWord, temp1, temp1, super_offset);
          // If heap poisoning is enabled, no need to unpoison
          // `temp1`, as we are comparing against null below.
          __ CompareAndBranchIfNonZero(temp1, slow_path->GetEntryLabel());
          __ Bind(&do_put);
        } else {
          __ B(ne, slow_path->GetEntryLabel());
        }
      }

      vixl32::Register source = value;
      if (kPoisonHeapReferences) {
        // Note that in the case where `value` is a null reference,
        // we do not enter this block, as a null reference does not
        // need poisoning.
        DCHECK_EQ(value_type, DataType::Type::kReference);
        __ Mov(temp1, value);
        GetAssembler()->PoisonHeapReference(temp1);
        source = temp1;
      }

      if (index.IsConstant()) {
        size_t offset =
            (Int32ConstantFrom(index) << TIMES_4) + data_offset;
        GetAssembler()->StoreToOffset(kStoreWord, source, array, offset);
      } else {
        DCHECK(index.IsRegister()) << index;

        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, array, data_offset);
        codegen_->StoreToShiftedRegOffset(value_type,
                                          LocationFrom(source),
                                          temp,
                                          RegisterFrom(index));
      }

      if (!may_need_runtime_call_for_type_check) {
        // TODO(VIXL): Ensure we record the pc position immediately after the preceding store
        // instruction.
        codegen_->MaybeRecordImplicitNullCheck(instruction);
      }

      codegen_->MarkGCCard(temp1, temp2, array, value, instruction->GetValueCanBeNull());

      if (done.IsReferenced()) {
        __ Bind(&done);
      }

      if (slow_path != nullptr) {
        __ Bind(slow_path->GetExitLabel());
      }

      break;
    }

    case DataType::Type::kInt64: {
      Location value = locations->InAt(2);
      if (index.IsConstant()) {
        size_t offset =
            (Int32ConstantFrom(index) << TIMES_8) + data_offset;
        GetAssembler()->StoreToOffset(kStoreWordPair, LowRegisterFrom(value), array, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, array, Operand(RegisterFrom(index), vixl32::LSL, TIMES_8));
        GetAssembler()->StoreToOffset(kStoreWordPair, LowRegisterFrom(value), temp, data_offset);
      }
      break;
    }

    case DataType::Type::kFloat32: {
      Location value = locations->InAt(2);
      DCHECK(value.IsFpuRegister());
      if (index.IsConstant()) {
        size_t offset = (Int32ConstantFrom(index) << TIMES_4) + data_offset;
        GetAssembler()->StoreSToOffset(SRegisterFrom(value), array, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, array, Operand(RegisterFrom(index), vixl32::LSL, TIMES_4));
        GetAssembler()->StoreSToOffset(SRegisterFrom(value), temp, data_offset);
      }
      break;
    }

    case DataType::Type::kFloat64: {
      Location value = locations->InAt(2);
      DCHECK(value.IsFpuRegisterPair());
      if (index.IsConstant()) {
        size_t offset = (Int32ConstantFrom(index) << TIMES_8) + data_offset;
        GetAssembler()->StoreDToOffset(DRegisterFrom(value), array, offset);
      } else {
        UseScratchRegisterScope temps(GetVIXLAssembler());
        vixl32::Register temp = temps.Acquire();
        __ Add(temp, array, Operand(RegisterFrom(index), vixl32::LSL, TIMES_8));
        GetAssembler()->StoreDToOffset(DRegisterFrom(value), temp, data_offset);
      }
      break;
    }

    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unreachable type " << value_type;
      UNREACHABLE();
  }

  // Objects are handled in the switch.
  if (value_type != DataType::Type::kReference) {
    // TODO(VIXL): Ensure we record the pc position immediately after the preceding store
    // instruction.
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
}

void LocationsBuilderARMVIXL::VisitArrayLength(HArrayLength* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitArrayLength(HArrayLength* instruction) {
  uint32_t offset = CodeGenerator::GetArrayLengthOffset(instruction);
  vixl32::Register obj = InputRegisterAt(instruction, 0);
  vixl32::Register out = OutputRegister(instruction);
  {
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    __ ldr(out, MemOperand(obj, offset));
    codegen_->MaybeRecordImplicitNullCheck(instruction);
  }
  // Mask out compression flag from String's array length.
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    __ Lsr(out, out, 1u);
  }
}

void LocationsBuilderARMVIXL::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);

  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(instruction->GetOffset()));
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitIntermediateAddress(HIntermediateAddress* instruction) {
  vixl32::Register out = OutputRegister(instruction);
  vixl32::Register first = InputRegisterAt(instruction, 0);
  Location second = instruction->GetLocations()->InAt(1);

  if (second.IsRegister()) {
    __ Add(out, first, RegisterFrom(second));
  } else {
    __ Add(out, first, Int32ConstantFrom(second));
  }
}

void LocationsBuilderARMVIXL::VisitIntermediateAddressIndex(
    HIntermediateAddressIndex* instruction) {
  LOG(FATAL) << "Unreachable " << instruction->GetId();
}

void InstructionCodeGeneratorARMVIXL::VisitIntermediateAddressIndex(
    HIntermediateAddressIndex* instruction) {
  LOG(FATAL) << "Unreachable " << instruction->GetId();
}

void LocationsBuilderARMVIXL::VisitBoundsCheck(HBoundsCheck* instruction) {
  RegisterSet caller_saves = RegisterSet::Empty();
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  caller_saves.Add(LocationFrom(calling_convention.GetRegisterAt(0)));
  caller_saves.Add(LocationFrom(calling_convention.GetRegisterAt(1)));
  LocationSummary* locations = codegen_->CreateThrowingSlowPathLocations(instruction, caller_saves);

  HInstruction* index = instruction->InputAt(0);
  HInstruction* length = instruction->InputAt(1);
  // If both index and length are constants we can statically check the bounds. But if at least one
  // of them is not encodable ArmEncodableConstantOrRegister will create
  // Location::RequiresRegister() which is not desired to happen. Instead we create constant
  // locations.
  bool both_const = index->IsConstant() && length->IsConstant();
  locations->SetInAt(0, both_const
      ? Location::ConstantLocation(index->AsConstant())
      : ArmEncodableConstantOrRegister(index, CMP));
  locations->SetInAt(1, both_const
      ? Location::ConstantLocation(length->AsConstant())
      : ArmEncodableConstantOrRegister(length, CMP));
}

void InstructionCodeGeneratorARMVIXL::VisitBoundsCheck(HBoundsCheck* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location index_loc = locations->InAt(0);
  Location length_loc = locations->InAt(1);

  if (length_loc.IsConstant()) {
    int32_t length = Int32ConstantFrom(length_loc);
    if (index_loc.IsConstant()) {
      // BCE will remove the bounds check if we are guaranteed to pass.
      int32_t index = Int32ConstantFrom(index_loc);
      if (index < 0 || index >= length) {
        SlowPathCodeARMVIXL* slow_path =
            new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathARMVIXL(instruction);
        codegen_->AddSlowPath(slow_path);
        __ B(slow_path->GetEntryLabel());
      } else {
        // Some optimization after BCE may have generated this, and we should not
        // generate a bounds check if it is a valid range.
      }
      return;
    }

    SlowPathCodeARMVIXL* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathARMVIXL(instruction);
    __ Cmp(RegisterFrom(index_loc), length);
    codegen_->AddSlowPath(slow_path);
    __ B(hs, slow_path->GetEntryLabel());
  } else {
    SlowPathCodeARMVIXL* slow_path =
        new (codegen_->GetScopedAllocator()) BoundsCheckSlowPathARMVIXL(instruction);
    __ Cmp(RegisterFrom(length_loc), InputOperandAt(instruction, 0));
    codegen_->AddSlowPath(slow_path);
    __ B(ls, slow_path->GetEntryLabel());
  }
}

void CodeGeneratorARMVIXL::MarkGCCard(vixl32::Register temp,
                                      vixl32::Register card,
                                      vixl32::Register object,
                                      vixl32::Register value,
                                      bool can_be_null) {
  vixl32::Label is_null;
  if (can_be_null) {
    __ CompareAndBranchIfZero(value, &is_null);
  }
  GetAssembler()->LoadFromOffset(
      kLoadWord, card, tr, Thread::CardTableOffset<kArmPointerSize>().Int32Value());
  __ Lsr(temp, object, Operand::From(gc::accounting::CardTable::kCardShift));
  __ Strb(card, MemOperand(card, temp));
  if (can_be_null) {
    __ Bind(&is_null);
  }
}

void LocationsBuilderARMVIXL::VisitParallelMove(HParallelMove* instruction ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorARMVIXL::VisitParallelMove(HParallelMove* instruction) {
  if (instruction->GetNext()->IsSuspendCheck() &&
      instruction->GetBlock()->GetLoopInformation() != nullptr) {
    HSuspendCheck* suspend_check = instruction->GetNext()->AsSuspendCheck();
    // The back edge will generate the suspend check.
    codegen_->ClearSpillSlotsFromLoopPhisInStackMap(suspend_check, instruction);
  }

  codegen_->GetMoveResolver()->EmitNativeCode(instruction);
}

void LocationsBuilderARMVIXL::VisitSuspendCheck(HSuspendCheck* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnSlowPath);
  locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
}

void InstructionCodeGeneratorARMVIXL::VisitSuspendCheck(HSuspendCheck* instruction) {
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
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 12);
}

void InstructionCodeGeneratorARMVIXL::GenerateSuspendCheck(HSuspendCheck* instruction,
                                                           HBasicBlock* successor) {
  SuspendCheckSlowPathARMVIXL* slow_path =
      down_cast<SuspendCheckSlowPathARMVIXL*>(instruction->GetSlowPath());
  if (slow_path == nullptr) {
    slow_path =
        new (codegen_->GetScopedAllocator()) SuspendCheckSlowPathARMVIXL(instruction, successor);
    instruction->SetSlowPath(slow_path);
    codegen_->AddSlowPath(slow_path);
    if (successor != nullptr) {
      DCHECK(successor->IsLoopHeader());
    }
  } else {
    DCHECK_EQ(slow_path->GetSuccessor(), successor);
  }

  UseScratchRegisterScope temps(GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  GetAssembler()->LoadFromOffset(
      kLoadUnsignedHalfword, temp, tr, Thread::ThreadFlagsOffset<kArmPointerSize>().Int32Value());
  if (successor == nullptr) {
    __ CompareAndBranchIfNonZero(temp, slow_path->GetEntryLabel());
    __ Bind(slow_path->GetReturnLabel());
  } else {
    __ CompareAndBranchIfZero(temp, codegen_->GetLabelOf(successor));
    __ B(slow_path->GetEntryLabel());
  }
}

ArmVIXLAssembler* ParallelMoveResolverARMVIXL::GetAssembler() const {
  return codegen_->GetAssembler();
}

void ParallelMoveResolverARMVIXL::EmitMove(size_t index) {
  UseScratchRegisterScope temps(GetAssembler()->GetVIXLAssembler());
  MoveOperands* move = moves_[index];
  Location source = move->GetSource();
  Location destination = move->GetDestination();

  if (source.IsRegister()) {
    if (destination.IsRegister()) {
      __ Mov(RegisterFrom(destination), RegisterFrom(source));
    } else if (destination.IsFpuRegister()) {
      __ Vmov(SRegisterFrom(destination), RegisterFrom(source));
    } else {
      DCHECK(destination.IsStackSlot());
      GetAssembler()->StoreToOffset(kStoreWord,
                                    RegisterFrom(source),
                                    sp,
                                    destination.GetStackIndex());
    }
  } else if (source.IsStackSlot()) {
    if (destination.IsRegister()) {
      GetAssembler()->LoadFromOffset(kLoadWord,
                                     RegisterFrom(destination),
                                     sp,
                                     source.GetStackIndex());
    } else if (destination.IsFpuRegister()) {
      GetAssembler()->LoadSFromOffset(SRegisterFrom(destination), sp, source.GetStackIndex());
    } else {
      DCHECK(destination.IsStackSlot());
      vixl32::Register temp = temps.Acquire();
      GetAssembler()->LoadFromOffset(kLoadWord, temp, sp, source.GetStackIndex());
      GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
    }
  } else if (source.IsFpuRegister()) {
    if (destination.IsRegister()) {
      __ Vmov(RegisterFrom(destination), SRegisterFrom(source));
    } else if (destination.IsFpuRegister()) {
      __ Vmov(SRegisterFrom(destination), SRegisterFrom(source));
    } else {
      DCHECK(destination.IsStackSlot());
      GetAssembler()->StoreSToOffset(SRegisterFrom(source), sp, destination.GetStackIndex());
    }
  } else if (source.IsDoubleStackSlot()) {
    if (destination.IsDoubleStackSlot()) {
      vixl32::DRegister temp = temps.AcquireD();
      GetAssembler()->LoadDFromOffset(temp, sp, source.GetStackIndex());
      GetAssembler()->StoreDToOffset(temp, sp, destination.GetStackIndex());
    } else if (destination.IsRegisterPair()) {
      DCHECK(ExpectedPairLayout(destination));
      GetAssembler()->LoadFromOffset(
          kLoadWordPair, LowRegisterFrom(destination), sp, source.GetStackIndex());
    } else {
      DCHECK(destination.IsFpuRegisterPair()) << destination;
      GetAssembler()->LoadDFromOffset(DRegisterFrom(destination), sp, source.GetStackIndex());
    }
  } else if (source.IsRegisterPair()) {
    if (destination.IsRegisterPair()) {
      __ Mov(LowRegisterFrom(destination), LowRegisterFrom(source));
      __ Mov(HighRegisterFrom(destination), HighRegisterFrom(source));
    } else if (destination.IsFpuRegisterPair()) {
      __ Vmov(DRegisterFrom(destination), LowRegisterFrom(source), HighRegisterFrom(source));
    } else {
      DCHECK(destination.IsDoubleStackSlot()) << destination;
      DCHECK(ExpectedPairLayout(source));
      GetAssembler()->StoreToOffset(kStoreWordPair,
                                    LowRegisterFrom(source),
                                    sp,
                                    destination.GetStackIndex());
    }
  } else if (source.IsFpuRegisterPair()) {
    if (destination.IsRegisterPair()) {
      __ Vmov(LowRegisterFrom(destination), HighRegisterFrom(destination), DRegisterFrom(source));
    } else if (destination.IsFpuRegisterPair()) {
      __ Vmov(DRegisterFrom(destination), DRegisterFrom(source));
    } else {
      DCHECK(destination.IsDoubleStackSlot()) << destination;
      GetAssembler()->StoreDToOffset(DRegisterFrom(source), sp, destination.GetStackIndex());
    }
  } else {
    DCHECK(source.IsConstant()) << source;
    HConstant* constant = source.GetConstant();
    if (constant->IsIntConstant() || constant->IsNullConstant()) {
      int32_t value = CodeGenerator::GetInt32ValueOf(constant);
      if (destination.IsRegister()) {
        __ Mov(RegisterFrom(destination), value);
      } else {
        DCHECK(destination.IsStackSlot());
        vixl32::Register temp = temps.Acquire();
        __ Mov(temp, value);
        GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
      }
    } else if (constant->IsLongConstant()) {
      int64_t value = Int64ConstantFrom(source);
      if (destination.IsRegisterPair()) {
        __ Mov(LowRegisterFrom(destination), Low32Bits(value));
        __ Mov(HighRegisterFrom(destination), High32Bits(value));
      } else {
        DCHECK(destination.IsDoubleStackSlot()) << destination;
        vixl32::Register temp = temps.Acquire();
        __ Mov(temp, Low32Bits(value));
        GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
        __ Mov(temp, High32Bits(value));
        GetAssembler()->StoreToOffset(kStoreWord,
                                      temp,
                                      sp,
                                      destination.GetHighStackIndex(kArmWordSize));
      }
    } else if (constant->IsDoubleConstant()) {
      double value = constant->AsDoubleConstant()->GetValue();
      if (destination.IsFpuRegisterPair()) {
        __ Vmov(DRegisterFrom(destination), value);
      } else {
        DCHECK(destination.IsDoubleStackSlot()) << destination;
        uint64_t int_value = bit_cast<uint64_t, double>(value);
        vixl32::Register temp = temps.Acquire();
        __ Mov(temp, Low32Bits(int_value));
        GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
        __ Mov(temp, High32Bits(int_value));
        GetAssembler()->StoreToOffset(kStoreWord,
                                      temp,
                                      sp,
                                      destination.GetHighStackIndex(kArmWordSize));
      }
    } else {
      DCHECK(constant->IsFloatConstant()) << constant->DebugName();
      float value = constant->AsFloatConstant()->GetValue();
      if (destination.IsFpuRegister()) {
        __ Vmov(SRegisterFrom(destination), value);
      } else {
        DCHECK(destination.IsStackSlot());
        vixl32::Register temp = temps.Acquire();
        __ Mov(temp, bit_cast<int32_t, float>(value));
        GetAssembler()->StoreToOffset(kStoreWord, temp, sp, destination.GetStackIndex());
      }
    }
  }
}

void ParallelMoveResolverARMVIXL::Exchange(vixl32::Register reg, int mem) {
  UseScratchRegisterScope temps(GetAssembler()->GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  __ Mov(temp, reg);
  GetAssembler()->LoadFromOffset(kLoadWord, reg, sp, mem);
  GetAssembler()->StoreToOffset(kStoreWord, temp, sp, mem);
}

void ParallelMoveResolverARMVIXL::Exchange(int mem1, int mem2) {
  // TODO(VIXL32): Double check the performance of this implementation.
  UseScratchRegisterScope temps(GetAssembler()->GetVIXLAssembler());
  vixl32::Register temp1 = temps.Acquire();
  ScratchRegisterScope ensure_scratch(
      this, temp1.GetCode(), r0.GetCode(), codegen_->GetNumberOfCoreRegisters());
  vixl32::Register temp2(ensure_scratch.GetRegister());

  int stack_offset = ensure_scratch.IsSpilled() ? kArmWordSize : 0;
  GetAssembler()->LoadFromOffset(kLoadWord, temp1, sp, mem1 + stack_offset);
  GetAssembler()->LoadFromOffset(kLoadWord, temp2, sp, mem2 + stack_offset);
  GetAssembler()->StoreToOffset(kStoreWord, temp1, sp, mem2 + stack_offset);
  GetAssembler()->StoreToOffset(kStoreWord, temp2, sp, mem1 + stack_offset);
}

void ParallelMoveResolverARMVIXL::EmitSwap(size_t index) {
  MoveOperands* move = moves_[index];
  Location source = move->GetSource();
  Location destination = move->GetDestination();
  UseScratchRegisterScope temps(GetAssembler()->GetVIXLAssembler());

  if (source.IsRegister() && destination.IsRegister()) {
    vixl32::Register temp = temps.Acquire();
    DCHECK(!RegisterFrom(source).Is(temp));
    DCHECK(!RegisterFrom(destination).Is(temp));
    __ Mov(temp, RegisterFrom(destination));
    __ Mov(RegisterFrom(destination), RegisterFrom(source));
    __ Mov(RegisterFrom(source), temp);
  } else if (source.IsRegister() && destination.IsStackSlot()) {
    Exchange(RegisterFrom(source), destination.GetStackIndex());
  } else if (source.IsStackSlot() && destination.IsRegister()) {
    Exchange(RegisterFrom(destination), source.GetStackIndex());
  } else if (source.IsStackSlot() && destination.IsStackSlot()) {
    Exchange(source.GetStackIndex(), destination.GetStackIndex());
  } else if (source.IsFpuRegister() && destination.IsFpuRegister()) {
    vixl32::Register temp = temps.Acquire();
    __ Vmov(temp, SRegisterFrom(source));
    __ Vmov(SRegisterFrom(source), SRegisterFrom(destination));
    __ Vmov(SRegisterFrom(destination), temp);
  } else if (source.IsRegisterPair() && destination.IsRegisterPair()) {
    vixl32::DRegister temp = temps.AcquireD();
    __ Vmov(temp, LowRegisterFrom(source), HighRegisterFrom(source));
    __ Mov(LowRegisterFrom(source), LowRegisterFrom(destination));
    __ Mov(HighRegisterFrom(source), HighRegisterFrom(destination));
    __ Vmov(LowRegisterFrom(destination), HighRegisterFrom(destination), temp);
  } else if (source.IsRegisterPair() || destination.IsRegisterPair()) {
    vixl32::Register low_reg = LowRegisterFrom(source.IsRegisterPair() ? source : destination);
    int mem = source.IsRegisterPair() ? destination.GetStackIndex() : source.GetStackIndex();
    DCHECK(ExpectedPairLayout(source.IsRegisterPair() ? source : destination));
    vixl32::DRegister temp = temps.AcquireD();
    __ Vmov(temp, low_reg, vixl32::Register(low_reg.GetCode() + 1));
    GetAssembler()->LoadFromOffset(kLoadWordPair, low_reg, sp, mem);
    GetAssembler()->StoreDToOffset(temp, sp, mem);
  } else if (source.IsFpuRegisterPair() && destination.IsFpuRegisterPair()) {
    vixl32::DRegister first = DRegisterFrom(source);
    vixl32::DRegister second = DRegisterFrom(destination);
    vixl32::DRegister temp = temps.AcquireD();
    __ Vmov(temp, first);
    __ Vmov(first, second);
    __ Vmov(second, temp);
  } else if (source.IsFpuRegisterPair() || destination.IsFpuRegisterPair()) {
    vixl32::DRegister reg = source.IsFpuRegisterPair()
        ? DRegisterFrom(source)
        : DRegisterFrom(destination);
    int mem = source.IsFpuRegisterPair()
        ? destination.GetStackIndex()
        : source.GetStackIndex();
    vixl32::DRegister temp = temps.AcquireD();
    __ Vmov(temp, reg);
    GetAssembler()->LoadDFromOffset(reg, sp, mem);
    GetAssembler()->StoreDToOffset(temp, sp, mem);
  } else if (source.IsFpuRegister() || destination.IsFpuRegister()) {
    vixl32::SRegister reg = source.IsFpuRegister()
        ? SRegisterFrom(source)
        : SRegisterFrom(destination);
    int mem = source.IsFpuRegister()
        ? destination.GetStackIndex()
        : source.GetStackIndex();
    vixl32::Register temp = temps.Acquire();
    __ Vmov(temp, reg);
    GetAssembler()->LoadSFromOffset(reg, sp, mem);
    GetAssembler()->StoreToOffset(kStoreWord, temp, sp, mem);
  } else if (source.IsDoubleStackSlot() && destination.IsDoubleStackSlot()) {
    vixl32::DRegister temp1 = temps.AcquireD();
    vixl32::DRegister temp2 = temps.AcquireD();
    __ Vldr(temp1, MemOperand(sp, source.GetStackIndex()));
    __ Vldr(temp2, MemOperand(sp, destination.GetStackIndex()));
    __ Vstr(temp1, MemOperand(sp, destination.GetStackIndex()));
    __ Vstr(temp2, MemOperand(sp, source.GetStackIndex()));
  } else {
    LOG(FATAL) << "Unimplemented" << source << " <-> " << destination;
  }
}

void ParallelMoveResolverARMVIXL::SpillScratch(int reg) {
  __ Push(vixl32::Register(reg));
}

void ParallelMoveResolverARMVIXL::RestoreScratch(int reg) {
  __ Pop(vixl32::Register(reg));
}

HLoadClass::LoadKind CodeGeneratorARMVIXL::GetSupportedLoadClassKind(
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

void LocationsBuilderARMVIXL::VisitLoadClass(HLoadClass* cls) {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    InvokeRuntimeCallingConventionARMVIXL calling_convention;
    CodeGenerator::CreateLoadClassRuntimeCallLocationSummary(
        cls,
        LocationFrom(calling_convention.GetRegisterAt(0)),
        LocationFrom(r0));
    DCHECK(calling_convention.GetRegisterAt(0).Is(r0));
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
      InvokeRuntimeCallingConventionARMVIXL calling_convention;
      caller_saves.Add(LocationFrom(calling_convention.GetRegisterAt(0)));
      // TODO: Add GetReturnLocation() to the calling convention so that we can DCHECK()
      // that the the kPrimNot result register is the same as the first argument register.
      locations->SetCustomSlowPathCallerSaves(caller_saves);
    } else {
      // For non-Baker read barrier we have a temp-clobbering call.
    }
  }
  if (kUseBakerReadBarrier && kBakerReadBarrierLinkTimeThunksEnableForGcRoots) {
    if (load_kind == HLoadClass::LoadKind::kBssEntry ||
        (load_kind == HLoadClass::LoadKind::kReferrersClass &&
            !Runtime::Current()->UseJitCompilation())) {
      locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorARMVIXL::VisitLoadClass(HLoadClass* cls) NO_THREAD_SAFETY_ANALYSIS {
  HLoadClass::LoadKind load_kind = cls->GetLoadKind();
  if (load_kind == HLoadClass::LoadKind::kRuntimeCall) {
    codegen_->GenerateLoadClassRuntimeCall(cls);
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 13);
    return;
  }
  DCHECK(!cls->NeedsAccessCheck());

  LocationSummary* locations = cls->GetLocations();
  Location out_loc = locations->Out();
  vixl32::Register out = OutputRegister(cls);

  const ReadBarrierOption read_barrier_option = cls->IsInBootImage()
      ? kWithoutReadBarrier
      : kCompilerReadBarrierOption;
  bool generate_null_check = false;
  switch (load_kind) {
    case HLoadClass::LoadKind::kReferrersClass: {
      DCHECK(!cls->CanCallRuntime());
      DCHECK(!cls->MustGenerateClinitCheck());
      // /* GcRoot<mirror::Class> */ out = current_method->declaring_class_
      vixl32::Register current_method = InputRegisterAt(cls, 0);
      GenerateGcRootFieldLoad(cls,
                              out_loc,
                              current_method,
                              ArtMethod::DeclaringClassOffset().Int32Value(),
                              read_barrier_option);
      break;
    }
    case HLoadClass::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      break;
    }
    case HLoadClass::LoadKind::kBootImageAddress: {
      DCHECK_EQ(read_barrier_option, kWithoutReadBarrier);
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(cls->GetClass().Get()));
      DCHECK_NE(address, 0u);
      __ Ldr(out, codegen_->DeduplicateBootImageAddressLiteral(address));
      break;
    }
    case HLoadClass::LoadKind::kBootImageClassTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewBootImageTypePatch(cls->GetDexFile(), cls->GetTypeIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      __ Ldr(out, MemOperand(out, /* offset */ 0));
      // Extract the reference from the slot data, i.e. clear the hash bits.
      int32_t masked_hash = ClassTable::TableSlot::MaskHash(
          ComputeModifiedUtf8Hash(cls->GetDexFile().StringByTypeIdx(cls->GetTypeIndex())));
      if (masked_hash != 0) {
        __ Sub(out, out, Operand(masked_hash));
      }
      break;
    }
    case HLoadClass::LoadKind::kBssEntry: {
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewTypeBssEntryPatch(cls->GetDexFile(), cls->GetTypeIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      GenerateGcRootFieldLoad(cls, out_loc, out, /* offset */ 0, read_barrier_option);
      generate_null_check = true;
      break;
    }
    case HLoadClass::LoadKind::kJitTableAddress: {
      __ Ldr(out, codegen_->DeduplicateJitClassLiteral(cls->GetDexFile(),
                                                       cls->GetTypeIndex(),
                                                       cls->GetClass()));
      // /* GcRoot<mirror::Class> */ out = *out
      GenerateGcRootFieldLoad(cls, out_loc, out, /* offset */ 0, read_barrier_option);
      break;
    }
    case HLoadClass::LoadKind::kRuntimeCall:
    case HLoadClass::LoadKind::kInvalid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }

  if (generate_null_check || cls->MustGenerateClinitCheck()) {
    DCHECK(cls->CanCallRuntime());
    LoadClassSlowPathARMVIXL* slow_path =
        new (codegen_->GetScopedAllocator()) LoadClassSlowPathARMVIXL(
            cls, cls, cls->GetDexPc(), cls->MustGenerateClinitCheck());
    codegen_->AddSlowPath(slow_path);
    if (generate_null_check) {
      __ CompareAndBranchIfZero(out, slow_path->GetEntryLabel());
    }
    if (cls->MustGenerateClinitCheck()) {
      GenerateClassInitializationCheck(slow_path, out);
    } else {
      __ Bind(slow_path->GetExitLabel());
    }
    codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 14);
  }
}

void LocationsBuilderARMVIXL::VisitClinitCheck(HClinitCheck* check) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(check, LocationSummary::kCallOnSlowPath);
  locations->SetInAt(0, Location::RequiresRegister());
  if (check->HasUses()) {
    locations->SetOut(Location::SameAsFirstInput());
  }
}

void InstructionCodeGeneratorARMVIXL::VisitClinitCheck(HClinitCheck* check) {
  // We assume the class is not null.
  LoadClassSlowPathARMVIXL* slow_path =
      new (codegen_->GetScopedAllocator()) LoadClassSlowPathARMVIXL(check->GetLoadClass(),
                                                                    check,
                                                                    check->GetDexPc(),
                                                                    /* do_clinit */ true);
  codegen_->AddSlowPath(slow_path);
  GenerateClassInitializationCheck(slow_path, InputRegisterAt(check, 0));
}

void InstructionCodeGeneratorARMVIXL::GenerateClassInitializationCheck(
    LoadClassSlowPathARMVIXL* slow_path, vixl32::Register class_reg) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  constexpr size_t status_lsb_position = SubtypeCheckBits::BitStructSizeOf();
  const size_t status_byte_offset =
      mirror::Class::StatusOffset().SizeValue() + (status_lsb_position / kBitsPerByte);
  constexpr uint32_t shifted_initialized_value =
      enum_cast<uint32_t>(ClassStatus::kInitialized) << (status_lsb_position % kBitsPerByte);

  GetAssembler()->LoadFromOffset(kLoadUnsignedByte, temp, class_reg, status_byte_offset);
  __ Cmp(temp, shifted_initialized_value);
  __ B(lo, slow_path->GetEntryLabel());
  // Even if the initialized flag is set, we may be in a situation where caches are not synced
  // properly. Therefore, we do a memory fence.
  __ Dmb(ISH);
  __ Bind(slow_path->GetExitLabel());
}

HLoadString::LoadKind CodeGeneratorARMVIXL::GetSupportedLoadStringKind(
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

void LocationsBuilderARMVIXL::VisitLoadString(HLoadString* load) {
  LocationSummary::CallKind call_kind = CodeGenerator::GetLoadStringCallKind(load);
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(load, call_kind);
  HLoadString::LoadKind load_kind = load->GetLoadKind();
  if (load_kind == HLoadString::LoadKind::kRuntimeCall) {
    locations->SetOut(LocationFrom(r0));
  } else {
    locations->SetOut(Location::RequiresRegister());
    if (load_kind == HLoadString::LoadKind::kBssEntry) {
      if (!kUseReadBarrier || kUseBakerReadBarrier) {
        // Rely on the pResolveString and marking to save everything we need, including temps.
        RegisterSet caller_saves = RegisterSet::Empty();
        InvokeRuntimeCallingConventionARMVIXL calling_convention;
        caller_saves.Add(LocationFrom(calling_convention.GetRegisterAt(0)));
        // TODO: Add GetReturnLocation() to the calling convention so that we can DCHECK()
        // that the the kPrimNot result register is the same as the first argument register.
        locations->SetCustomSlowPathCallerSaves(caller_saves);
        if (kUseBakerReadBarrier && kBakerReadBarrierLinkTimeThunksEnableForGcRoots) {
          locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
        }
      } else {
        // For non-Baker read barrier we have a temp-clobbering call.
      }
    }
  }
}

// NO_THREAD_SAFETY_ANALYSIS as we manipulate handles whose internal object we know does not
// move.
void InstructionCodeGeneratorARMVIXL::VisitLoadString(HLoadString* load) NO_THREAD_SAFETY_ANALYSIS {
  LocationSummary* locations = load->GetLocations();
  Location out_loc = locations->Out();
  vixl32::Register out = OutputRegister(load);
  HLoadString::LoadKind load_kind = load->GetLoadKind();

  switch (load_kind) {
    case HLoadString::LoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      return;
    }
    case HLoadString::LoadKind::kBootImageAddress: {
      uint32_t address = dchecked_integral_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(load->GetString().Get()));
      DCHECK_NE(address, 0u);
      __ Ldr(out, codegen_->DeduplicateBootImageAddressLiteral(address));
      return;
    }
    case HLoadString::LoadKind::kBootImageInternTable: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewBootImageStringPatch(load->GetDexFile(), load->GetStringIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      __ Ldr(out, MemOperand(out, /* offset */ 0));
      return;
    }
    case HLoadString::LoadKind::kBssEntry: {
      DCHECK(!codegen_->GetCompilerOptions().IsBootImage());
      CodeGeneratorARMVIXL::PcRelativePatchInfo* labels =
          codegen_->NewStringBssEntryPatch(load->GetDexFile(), load->GetStringIndex());
      codegen_->EmitMovwMovtPlaceholder(labels, out);
      GenerateGcRootFieldLoad(load, out_loc, out, /* offset */ 0, kCompilerReadBarrierOption);
      LoadStringSlowPathARMVIXL* slow_path =
          new (codegen_->GetScopedAllocator()) LoadStringSlowPathARMVIXL(load);
      codegen_->AddSlowPath(slow_path);
      __ CompareAndBranchIfZero(out, slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 15);
      return;
    }
    case HLoadString::LoadKind::kJitTableAddress: {
      __ Ldr(out, codegen_->DeduplicateJitStringLiteral(load->GetDexFile(),
                                                        load->GetStringIndex(),
                                                        load->GetString()));
      // /* GcRoot<mirror::String> */ out = *out
      GenerateGcRootFieldLoad(load, out_loc, out, /* offset */ 0, kCompilerReadBarrierOption);
      return;
    }
    default:
      break;
  }

  // TODO: Re-add the compiler code to do string dex cache lookup again.
  DCHECK_EQ(load->GetLoadKind(), HLoadString::LoadKind::kRuntimeCall);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  __ Mov(calling_convention.GetRegisterAt(0), load->GetStringIndex().index_);
  codegen_->InvokeRuntime(kQuickResolveString, load, load->GetDexPc());
  CheckEntrypointTypes<kQuickResolveString, void*, uint32_t>();
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 16);
}

static int32_t GetExceptionTlsOffset() {
  return Thread::ExceptionOffset<kArmPointerSize>().Int32Value();
}

void LocationsBuilderARMVIXL::VisitLoadException(HLoadException* load) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(load, LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARMVIXL::VisitLoadException(HLoadException* load) {
  vixl32::Register out = OutputRegister(load);
  GetAssembler()->LoadFromOffset(kLoadWord, out, tr, GetExceptionTlsOffset());
}


void LocationsBuilderARMVIXL::VisitClearException(HClearException* clear) {
  new (GetGraph()->GetAllocator()) LocationSummary(clear, LocationSummary::kNoCall);
}

void InstructionCodeGeneratorARMVIXL::VisitClearException(HClearException* clear ATTRIBUTE_UNUSED) {
  UseScratchRegisterScope temps(GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  __ Mov(temp, 0);
  GetAssembler()->StoreToOffset(kStoreWord, temp, tr, GetExceptionTlsOffset());
}

void LocationsBuilderARMVIXL::VisitThrow(HThrow* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorARMVIXL::VisitThrow(HThrow* instruction) {
  codegen_->InvokeRuntime(kQuickDeliverException, instruction, instruction->GetDexPc());
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
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

void LocationsBuilderARMVIXL::VisitInstanceOf(HInstanceOf* instruction) {
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
  // Note that TypeCheckSlowPathARM uses this register too.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
  locations->AddRegisterTemps(NumberOfInstanceOfTemps(type_check_kind));
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    codegen_->MaybeAddBakerCcEntrypointTempForFields(locations);
  }
}

void InstructionCodeGeneratorARMVIXL::VisitInstanceOf(HInstanceOf* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  vixl32::Register obj = InputRegisterAt(instruction, 0);
  vixl32::Register cls = InputRegisterAt(instruction, 1);
  Location out_loc = locations->Out();
  vixl32::Register out = OutputRegister(instruction);
  const size_t num_temps = NumberOfInstanceOfTemps(type_check_kind);
  DCHECK_LE(num_temps, 1u);
  Location maybe_temp_loc = (num_temps >= 1) ? locations->GetTemp(0) : Location::NoLocation();
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  vixl32::Label done;
  vixl32::Label* const final_label = codegen_->GetFinalLabel(instruction, &done);
  SlowPathCodeARMVIXL* slow_path = nullptr;

  // Return 0 if `obj` is null.
  // avoid null check if we know obj is not null.
  if (instruction->MustDoNullCheck()) {
    DCHECK(!out.Is(obj));
    __ Mov(out, 0);
    __ CompareAndBranchIfZero(obj, final_label, /* far_target */ false);
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
      __ Cmp(out, cls);
      // We speculatively set the result to false without changing the condition
      // flags, which allows us to avoid some branching later.
      __ Mov(LeaveFlags, out, 0);

      // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
      // we check that the output is in a low register, so that a 16-bit MOV
      // encoding can be used.
      if (out.IsLow()) {
        // We use the scope because of the IT block that follows.
        ExactAssemblyScope guard(GetVIXLAssembler(),
                                 2 * vixl32::k16BitT32InstructionSizeInBytes,
                                 CodeBufferCheckScope::kExactSize);

        __ it(eq);
        __ mov(eq, out, 1);
      } else {
        __ B(ne, final_label, /* far_target */ false);
        __ Mov(out, 1);
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
      vixl32::Label loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to the final label.
      __ CompareAndBranchIfZero(out, final_label, /* far_target */ false);
      __ Cmp(out, cls);
      __ B(ne, &loop, /* far_target */ false);
      __ Mov(out, 1);
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
      vixl32::Label loop, success;
      __ Bind(&loop);
      __ Cmp(out, cls);
      __ B(eq, &success, /* far_target */ false);
      // /* HeapReference<Class> */ out = out->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       super_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // This is essentially a null check, but it sets the condition flags to the
      // proper value for the code that follows the loop, i.e. not `eq`.
      __ Cmp(out, 1);
      __ B(hs, &loop, /* far_target */ false);

      // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
      // we check that the output is in a low register, so that a 16-bit MOV
      // encoding can be used.
      if (out.IsLow()) {
        // If `out` is null, we use it for the result, and the condition flags
        // have already been set to `ne`, so the IT block that comes afterwards
        // (and which handles the successful case) turns into a NOP (instead of
        // overwriting `out`).
        __ Bind(&success);

        // We use the scope because of the IT block that follows.
        ExactAssemblyScope guard(GetVIXLAssembler(),
                                 2 * vixl32::k16BitT32InstructionSizeInBytes,
                                 CodeBufferCheckScope::kExactSize);

        // There is only one branch to the `success` label (which is bound to this
        // IT block), and it has the same condition, `eq`, so in that case the MOV
        // is executed.
        __ it(eq);
        __ mov(eq, out, 1);
      } else {
        // If `out` is null, we use it for the result, and jump to the final label.
        __ B(final_label);
        __ Bind(&success);
        __ Mov(out, 1);
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
      vixl32::Label exact_check;
      __ Cmp(out, cls);
      __ B(eq, &exact_check, /* far_target */ false);
      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ out = out->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       out_loc,
                                       component_offset,
                                       maybe_temp_loc,
                                       read_barrier_option);
      // If `out` is null, we use it for the result, and jump to the final label.
      __ CompareAndBranchIfZero(out, final_label, /* far_target */ false);
      GetAssembler()->LoadFromOffset(kLoadUnsignedHalfword, out, out, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Cmp(out, 0);
      // We speculatively set the result to false without changing the condition
      // flags, which allows us to avoid some branching later.
      __ Mov(LeaveFlags, out, 0);

      // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
      // we check that the output is in a low register, so that a 16-bit MOV
      // encoding can be used.
      if (out.IsLow()) {
        __ Bind(&exact_check);

        // We use the scope because of the IT block that follows.
        ExactAssemblyScope guard(GetVIXLAssembler(),
                                 2 * vixl32::k16BitT32InstructionSizeInBytes,
                                 CodeBufferCheckScope::kExactSize);

        __ it(eq);
        __ mov(eq, out, 1);
      } else {
        __ B(ne, final_label, /* far_target */ false);
        __ Bind(&exact_check);
        __ Mov(out, 1);
      }

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
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARMVIXL(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ B(ne, slow_path->GetEntryLabel());
      __ Mov(out, 1);
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
      slow_path = new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARMVIXL(
          instruction, /* is_fatal */ false);
      codegen_->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      break;
    }
  }

  if (done.IsReferenced()) {
    __ Bind(&done);
  }

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void LocationsBuilderARMVIXL::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary::CallKind call_kind = CodeGenerator::GetCheckCastCallKind(instruction);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->AddRegisterTemps(NumberOfCheckCastTemps(type_check_kind));
}

void InstructionCodeGeneratorARMVIXL::VisitCheckCast(HCheckCast* instruction) {
  TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
  LocationSummary* locations = instruction->GetLocations();
  Location obj_loc = locations->InAt(0);
  vixl32::Register obj = InputRegisterAt(instruction, 0);
  vixl32::Register cls = InputRegisterAt(instruction, 1);
  Location temp_loc = locations->GetTemp(0);
  vixl32::Register temp = RegisterFrom(temp_loc);
  const size_t num_temps = NumberOfCheckCastTemps(type_check_kind);
  DCHECK_LE(num_temps, 3u);
  Location maybe_temp2_loc = (num_temps >= 2) ? locations->GetTemp(1) : Location::NoLocation();
  Location maybe_temp3_loc = (num_temps >= 3) ? locations->GetTemp(2) : Location::NoLocation();
  const uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  const uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  const uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  const uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  const uint32_t iftable_offset = mirror::Class::IfTableOffset().Uint32Value();
  const uint32_t array_length_offset = mirror::Array::LengthOffset().Uint32Value();
  const uint32_t object_array_data_offset =
      mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();

  bool is_type_check_slow_path_fatal = CodeGenerator::IsTypeCheckSlowPathFatal(instruction);
  SlowPathCodeARMVIXL* type_check_slow_path =
      new (codegen_->GetScopedAllocator()) TypeCheckSlowPathARMVIXL(
          instruction, is_type_check_slow_path_fatal);
  codegen_->AddSlowPath(type_check_slow_path);

  vixl32::Label done;
  vixl32::Label* final_label = codegen_->GetFinalLabel(instruction, &done);
  // Avoid null check if we know obj is not null.
  if (instruction->MustDoNullCheck()) {
    __ CompareAndBranchIfZero(obj, final_label, /* far_target */ false);
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
      vixl32::Label loop;
      __ Bind(&loop);
      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);

      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception.
      __ CompareAndBranchIfZero(temp, type_check_slow_path->GetEntryLabel());

      // Otherwise, compare the classes.
      __ Cmp(temp, cls);
      __ B(ne, &loop, /* far_target */ false);
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
      vixl32::Label loop;
      __ Bind(&loop);
      __ Cmp(temp, cls);
      __ B(eq, final_label, /* far_target */ false);

      // /* HeapReference<Class> */ temp = temp->super_class_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       super_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);

      // If the class reference currently in `temp` is null, jump to the slow path to throw the
      // exception.
      __ CompareAndBranchIfZero(temp, type_check_slow_path->GetEntryLabel());
      // Otherwise, jump to the beginning of the loop.
      __ B(&loop);
      break;
    }

    case TypeCheckKind::kArrayObjectCheck:  {
      // /* HeapReference<Class> */ temp = obj->klass_
      GenerateReferenceLoadTwoRegisters(instruction,
                                        temp_loc,
                                        obj_loc,
                                        class_offset,
                                        maybe_temp2_loc,
                                        kWithoutReadBarrier);

      // Do an exact check.
      __ Cmp(temp, cls);
      __ B(eq, final_label, /* far_target */ false);

      // Otherwise, we need to check that the object's class is a non-primitive array.
      // /* HeapReference<Class> */ temp = temp->component_type_
      GenerateReferenceLoadOneRegister(instruction,
                                       temp_loc,
                                       component_offset,
                                       maybe_temp2_loc,
                                       kWithoutReadBarrier);
      // If the component type is null, jump to the slow path to throw the exception.
      __ CompareAndBranchIfZero(temp, type_check_slow_path->GetEntryLabel());
      // Otherwise,the object is indeed an array, jump to label `check_non_primitive_component_type`
      // to further check that this component type is not a primitive type.
      GetAssembler()->LoadFromOffset(kLoadUnsignedHalfword, temp, temp, primitive_offset);
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ CompareAndBranchIfNonZero(temp, type_check_slow_path->GetEntryLabel());
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

      __ B(type_check_slow_path->GetEntryLabel());
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
      __ Ldr(RegisterFrom(maybe_temp2_loc), MemOperand(temp, array_length_offset));
      // Loop through the iftable and check if any class matches.
      vixl32::Label start_loop;
      __ Bind(&start_loop);
      __ CompareAndBranchIfZero(RegisterFrom(maybe_temp2_loc),
                                type_check_slow_path->GetEntryLabel());
      __ Ldr(RegisterFrom(maybe_temp3_loc), MemOperand(temp, object_array_data_offset));
      GetAssembler()->MaybeUnpoisonHeapReference(RegisterFrom(maybe_temp3_loc));
      // Go to next interface.
      __ Add(temp, temp, Operand::From(2 * kHeapReferenceSize));
      __ Sub(RegisterFrom(maybe_temp2_loc), RegisterFrom(maybe_temp2_loc), 2);
      // Compare the classes and continue the loop if they do not match.
      __ Cmp(cls, RegisterFrom(maybe_temp3_loc));
      __ B(ne, &start_loop, /* far_target */ false);
      break;
    }
  }
  if (done.IsReferenced()) {
    __ Bind(&done);
  }

  __ Bind(type_check_slow_path->GetExitLabel());
}

void LocationsBuilderARMVIXL::VisitMonitorOperation(HMonitorOperation* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(
      instruction, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
}

void InstructionCodeGeneratorARMVIXL::VisitMonitorOperation(HMonitorOperation* instruction) {
  codegen_->InvokeRuntime(instruction->IsEnter() ? kQuickLockObject : kQuickUnlockObject,
                          instruction,
                          instruction->GetDexPc());
  if (instruction->IsEnter()) {
    CheckEntrypointTypes<kQuickLockObject, void, mirror::Object*>();
  } else {
    CheckEntrypointTypes<kQuickUnlockObject, void, mirror::Object*>();
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 17);
}

void LocationsBuilderARMVIXL::VisitAnd(HAnd* instruction) {
  HandleBitwiseOperation(instruction, AND);
}

void LocationsBuilderARMVIXL::VisitOr(HOr* instruction) {
  HandleBitwiseOperation(instruction, ORR);
}

void LocationsBuilderARMVIXL::VisitXor(HXor* instruction) {
  HandleBitwiseOperation(instruction, EOR);
}

void LocationsBuilderARMVIXL::HandleBitwiseOperation(HBinaryOperation* instruction, Opcode opcode) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32
         || instruction->GetResultType() == DataType::Type::kInt64);
  // Note: GVN reorders commutative operations to have the constant on the right hand side.
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, ArmEncodableConstantOrRegister(instruction->InputAt(1), opcode));
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitAnd(HAnd* instruction) {
  HandleBitwiseOperation(instruction);
}

void InstructionCodeGeneratorARMVIXL::VisitOr(HOr* instruction) {
  HandleBitwiseOperation(instruction);
}

void InstructionCodeGeneratorARMVIXL::VisitXor(HXor* instruction) {
  HandleBitwiseOperation(instruction);
}

void LocationsBuilderARMVIXL::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  DCHECK(instruction->GetResultType() == DataType::Type::kInt32
         || instruction->GetResultType() == DataType::Type::kInt64);

  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);
  Location out = locations->Out();

  if (instruction->GetResultType() == DataType::Type::kInt32) {
    vixl32::Register first_reg = RegisterFrom(first);
    vixl32::Register second_reg = RegisterFrom(second);
    vixl32::Register out_reg = RegisterFrom(out);

    switch (instruction->GetOpKind()) {
      case HInstruction::kAnd:
        __ Bic(out_reg, first_reg, second_reg);
        break;
      case HInstruction::kOr:
        __ Orn(out_reg, first_reg, second_reg);
        break;
      // There is no EON on arm.
      case HInstruction::kXor:
      default:
        LOG(FATAL) << "Unexpected instruction " << instruction->DebugName();
        UNREACHABLE();
    }
    return;

  } else {
    DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt64);
    vixl32::Register first_low = LowRegisterFrom(first);
    vixl32::Register first_high = HighRegisterFrom(first);
    vixl32::Register second_low = LowRegisterFrom(second);
    vixl32::Register second_high = HighRegisterFrom(second);
    vixl32::Register out_low = LowRegisterFrom(out);
    vixl32::Register out_high = HighRegisterFrom(out);

    switch (instruction->GetOpKind()) {
      case HInstruction::kAnd:
        __ Bic(out_low, first_low, second_low);
        __ Bic(out_high, first_high, second_high);
        break;
      case HInstruction::kOr:
        __ Orn(out_low, first_low, second_low);
        __ Orn(out_high, first_high, second_high);
        break;
      // There is no EON on arm.
      case HInstruction::kXor:
      default:
        LOG(FATAL) << "Unexpected instruction " << instruction->DebugName();
        UNREACHABLE();
    }
  }
}

void LocationsBuilderARMVIXL::VisitDataProcWithShifterOp(
    HDataProcWithShifterOp* instruction) {
  DCHECK(instruction->GetType() == DataType::Type::kInt32 ||
         instruction->GetType() == DataType::Type::kInt64);
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  const bool overlap = instruction->GetType() == DataType::Type::kInt64 &&
                       HDataProcWithShifterOp::IsExtensionOp(instruction->GetOpKind());

  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(),
                    overlap ? Location::kOutputOverlap : Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitDataProcWithShifterOp(
    HDataProcWithShifterOp* instruction) {
  const LocationSummary* const locations = instruction->GetLocations();
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();
  const HDataProcWithShifterOp::OpKind op_kind = instruction->GetOpKind();

  if (instruction->GetType() == DataType::Type::kInt32) {
    const vixl32::Register first = InputRegisterAt(instruction, 0);
    const vixl32::Register output = OutputRegister(instruction);
    const vixl32::Register second = instruction->InputAt(1)->GetType() == DataType::Type::kInt64
        ? LowRegisterFrom(locations->InAt(1))
        : InputRegisterAt(instruction, 1);

    if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
      DCHECK_EQ(kind, HInstruction::kAdd);

      switch (op_kind) {
        case HDataProcWithShifterOp::kUXTB:
          __ Uxtab(output, first, second);
          break;
        case HDataProcWithShifterOp::kUXTH:
          __ Uxtah(output, first, second);
          break;
        case HDataProcWithShifterOp::kSXTB:
          __ Sxtab(output, first, second);
          break;
        case HDataProcWithShifterOp::kSXTH:
          __ Sxtah(output, first, second);
          break;
        default:
          LOG(FATAL) << "Unexpected operation kind: " << op_kind;
          UNREACHABLE();
      }
    } else {
      GenerateDataProcInstruction(kind,
                                  output,
                                  first,
                                  Operand(second,
                                          ShiftFromOpKind(op_kind),
                                          instruction->GetShiftAmount()),
                                  codegen_);
    }
  } else {
    DCHECK_EQ(instruction->GetType(), DataType::Type::kInt64);

    if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
      const vixl32::Register second = InputRegisterAt(instruction, 1);

      DCHECK(!LowRegisterFrom(locations->Out()).Is(second));
      GenerateDataProc(kind,
                       locations->Out(),
                       locations->InAt(0),
                       second,
                       Operand(second, ShiftType::ASR, 31),
                       codegen_);
    } else {
      GenerateLongDataProc(instruction, codegen_);
    }
  }
}

// TODO(VIXL): Remove optimizations in the helper when they are implemented in vixl.
void InstructionCodeGeneratorARMVIXL::GenerateAndConst(vixl32::Register out,
                                                       vixl32::Register first,
                                                       uint32_t value) {
  // Optimize special cases for individual halfs of `and-long` (`and` is simplified earlier).
  if (value == 0xffffffffu) {
    if (!out.Is(first)) {
      __ Mov(out, first);
    }
    return;
  }
  if (value == 0u) {
    __ Mov(out, 0);
    return;
  }
  if (GetAssembler()->ShifterOperandCanHold(AND, value)) {
    __ And(out, first, value);
  } else if (GetAssembler()->ShifterOperandCanHold(BIC, ~value)) {
    __ Bic(out, first, ~value);
  } else {
    DCHECK(IsPowerOfTwo(value + 1));
    __ Ubfx(out, first, 0, WhichPowerOf2(value + 1));
  }
}

// TODO(VIXL): Remove optimizations in the helper when they are implemented in vixl.
void InstructionCodeGeneratorARMVIXL::GenerateOrrConst(vixl32::Register out,
                                                       vixl32::Register first,
                                                       uint32_t value) {
  // Optimize special cases for individual halfs of `or-long` (`or` is simplified earlier).
  if (value == 0u) {
    if (!out.Is(first)) {
      __ Mov(out, first);
    }
    return;
  }
  if (value == 0xffffffffu) {
    __ Mvn(out, 0);
    return;
  }
  if (GetAssembler()->ShifterOperandCanHold(ORR, value)) {
    __ Orr(out, first, value);
  } else {
    DCHECK(GetAssembler()->ShifterOperandCanHold(ORN, ~value));
    __ Orn(out, first, ~value);
  }
}

// TODO(VIXL): Remove optimizations in the helper when they are implemented in vixl.
void InstructionCodeGeneratorARMVIXL::GenerateEorConst(vixl32::Register out,
                                                       vixl32::Register first,
                                                       uint32_t value) {
  // Optimize special case for individual halfs of `xor-long` (`xor` is simplified earlier).
  if (value == 0u) {
    if (!out.Is(first)) {
      __ Mov(out, first);
    }
    return;
  }
  __ Eor(out, first, value);
}

void InstructionCodeGeneratorARMVIXL::GenerateAddLongConst(Location out,
                                                           Location first,
                                                           uint64_t value) {
  vixl32::Register out_low = LowRegisterFrom(out);
  vixl32::Register out_high = HighRegisterFrom(out);
  vixl32::Register first_low = LowRegisterFrom(first);
  vixl32::Register first_high = HighRegisterFrom(first);
  uint32_t value_low = Low32Bits(value);
  uint32_t value_high = High32Bits(value);
  if (value_low == 0u) {
    if (!out_low.Is(first_low)) {
      __ Mov(out_low, first_low);
    }
    __ Add(out_high, first_high, value_high);
    return;
  }
  __ Adds(out_low, first_low, value_low);
  if (GetAssembler()->ShifterOperandCanHold(ADC, value_high)) {
    __ Adc(out_high, first_high, value_high);
  } else {
    DCHECK(GetAssembler()->ShifterOperandCanHold(SBC, ~value_high));
    __ Sbc(out_high, first_high, ~value_high);
  }
}

void InstructionCodeGeneratorARMVIXL::HandleBitwiseOperation(HBinaryOperation* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location first = locations->InAt(0);
  Location second = locations->InAt(1);
  Location out = locations->Out();

  if (second.IsConstant()) {
    uint64_t value = static_cast<uint64_t>(Int64FromConstant(second.GetConstant()));
    uint32_t value_low = Low32Bits(value);
    if (instruction->GetResultType() == DataType::Type::kInt32) {
      vixl32::Register first_reg = InputRegisterAt(instruction, 0);
      vixl32::Register out_reg = OutputRegister(instruction);
      if (instruction->IsAnd()) {
        GenerateAndConst(out_reg, first_reg, value_low);
      } else if (instruction->IsOr()) {
        GenerateOrrConst(out_reg, first_reg, value_low);
      } else {
        DCHECK(instruction->IsXor());
        GenerateEorConst(out_reg, first_reg, value_low);
      }
    } else {
      DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt64);
      uint32_t value_high = High32Bits(value);
      vixl32::Register first_low = LowRegisterFrom(first);
      vixl32::Register first_high = HighRegisterFrom(first);
      vixl32::Register out_low = LowRegisterFrom(out);
      vixl32::Register out_high = HighRegisterFrom(out);
      if (instruction->IsAnd()) {
        GenerateAndConst(out_low, first_low, value_low);
        GenerateAndConst(out_high, first_high, value_high);
      } else if (instruction->IsOr()) {
        GenerateOrrConst(out_low, first_low, value_low);
        GenerateOrrConst(out_high, first_high, value_high);
      } else {
        DCHECK(instruction->IsXor());
        GenerateEorConst(out_low, first_low, value_low);
        GenerateEorConst(out_high, first_high, value_high);
      }
    }
    return;
  }

  if (instruction->GetResultType() == DataType::Type::kInt32) {
    vixl32::Register first_reg = InputRegisterAt(instruction, 0);
    vixl32::Register second_reg = InputRegisterAt(instruction, 1);
    vixl32::Register out_reg = OutputRegister(instruction);
    if (instruction->IsAnd()) {
      __ And(out_reg, first_reg, second_reg);
    } else if (instruction->IsOr()) {
      __ Orr(out_reg, first_reg, second_reg);
    } else {
      DCHECK(instruction->IsXor());
      __ Eor(out_reg, first_reg, second_reg);
    }
  } else {
    DCHECK_EQ(instruction->GetResultType(), DataType::Type::kInt64);
    vixl32::Register first_low = LowRegisterFrom(first);
    vixl32::Register first_high = HighRegisterFrom(first);
    vixl32::Register second_low = LowRegisterFrom(second);
    vixl32::Register second_high = HighRegisterFrom(second);
    vixl32::Register out_low = LowRegisterFrom(out);
    vixl32::Register out_high = HighRegisterFrom(out);
    if (instruction->IsAnd()) {
      __ And(out_low, first_low, second_low);
      __ And(out_high, first_high, second_high);
    } else if (instruction->IsOr()) {
      __ Orr(out_low, first_low, second_low);
      __ Orr(out_high, first_high, second_high);
    } else {
      DCHECK(instruction->IsXor());
      __ Eor(out_low, first_low, second_low);
      __ Eor(out_high, first_high, second_high);
    }
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateReferenceLoadOneRegister(
    HInstruction* instruction,
    Location out,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  vixl32::Register out_reg = RegisterFrom(out);
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    DCHECK(maybe_temp.IsRegister()) << maybe_temp;
    if (kUseBakerReadBarrier) {
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(out + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          instruction, out, out_reg, offset, maybe_temp, /* needs_null_check */ false);
    } else {
      // Load with slow path based read barrier.
      // Save the value of `out` into `maybe_temp` before overwriting it
      // in the following move operation, as we will need it for the
      // read barrier below.
      __ Mov(RegisterFrom(maybe_temp), out_reg);
      // /* HeapReference<Object> */ out = *(out + offset)
      GetAssembler()->LoadFromOffset(kLoadWord, out_reg, out_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, maybe_temp, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(out + offset)
    GetAssembler()->LoadFromOffset(kLoadWord, out_reg, out_reg, offset);
    GetAssembler()->MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateReferenceLoadTwoRegisters(
    HInstruction* instruction,
    Location out,
    Location obj,
    uint32_t offset,
    Location maybe_temp,
    ReadBarrierOption read_barrier_option) {
  vixl32::Register out_reg = RegisterFrom(out);
  vixl32::Register obj_reg = RegisterFrom(obj);
  if (read_barrier_option == kWithReadBarrier) {
    CHECK(kEmitCompilerReadBarrier);
    if (kUseBakerReadBarrier) {
      DCHECK(maybe_temp.IsRegister()) << maybe_temp;
      // Load with fast path based Baker's read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          instruction, out, obj_reg, offset, maybe_temp, /* needs_null_check */ false);
    } else {
      // Load with slow path based read barrier.
      // /* HeapReference<Object> */ out = *(obj + offset)
      GetAssembler()->LoadFromOffset(kLoadWord, out_reg, obj_reg, offset);
      codegen_->GenerateReadBarrierSlow(instruction, out, out, obj, offset);
    }
  } else {
    // Plain load with no read barrier.
    // /* HeapReference<Object> */ out = *(obj + offset)
    GetAssembler()->LoadFromOffset(kLoadWord, out_reg, obj_reg, offset);
    GetAssembler()->MaybeUnpoisonHeapReference(out_reg);
  }
}

void InstructionCodeGeneratorARMVIXL::GenerateGcRootFieldLoad(
    HInstruction* instruction,
    Location root,
    vixl32::Register obj,
    uint32_t offset,
    ReadBarrierOption read_barrier_option) {
  vixl32::Register root_reg = RegisterFrom(root);
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
        ExcludeIPAndBakerCcEntrypointRegister(&temps, instruction);
        bool narrow = CanEmitNarrowLdr(root_reg, obj, offset);
        uint32_t custom_data = linker::Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(
            root_reg.GetCode(), narrow);
        vixl32::Label* bne_label = codegen_->NewBakerReadBarrierPatch(custom_data);

        vixl::EmissionCheckScope guard(GetVIXLAssembler(), 4 * vixl32::kMaxInstructionSizeInBytes);
        vixl32::Label return_address;
        EmitAdrCode adr(GetVIXLAssembler(), lr, &return_address);
        __ cmp(mr, Operand(0));
        // Currently the offset is always within range. If that changes,
        // we shall have to split the load the same way as for fields.
        DCHECK_LT(offset, kReferenceLoadMinFarOffset);
        ptrdiff_t old_offset = GetVIXLAssembler()->GetBuffer()->GetCursorOffset();
        __ ldr(EncodingSize(narrow ? Narrow : Wide), root_reg, MemOperand(obj, offset));
        EmitPlaceholderBne(codegen_, bne_label);
        __ Bind(&return_address);
        DCHECK_EQ(old_offset - GetVIXLAssembler()->GetBuffer()->GetCursorOffset(),
                  narrow ? BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_OFFSET
                         : BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_OFFSET);
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
        SlowPathCodeARMVIXL* slow_path =
            new (codegen_->GetScopedAllocator()) ReadBarrierMarkSlowPathARMVIXL(instruction, root);
        codegen_->AddSlowPath(slow_path);

        // /* GcRoot<mirror::Object> */ root = *(obj + offset)
        GetAssembler()->LoadFromOffset(kLoadWord, root_reg, obj, offset);
        static_assert(
            sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(GcRoot<mirror::Object>),
            "art::mirror::CompressedReference<mirror::Object> and art::GcRoot<mirror::Object> "
            "have different sizes.");
        static_assert(sizeof(mirror::CompressedReference<mirror::Object>) == sizeof(int32_t),
                      "art::mirror::CompressedReference<mirror::Object> and int32_t "
                      "have different sizes.");

        __ CompareAndBranchIfNonZero(mr, slow_path->GetEntryLabel());
        __ Bind(slow_path->GetExitLabel());
      }
    } else {
      // GC root loaded through a slow path for read barriers other
      // than Baker's.
      // /* GcRoot<mirror::Object>* */ root = obj + offset
      __ Add(root_reg, obj, offset);
      // /* mirror::Object* */ root = root->Read()
      codegen_->GenerateReadBarrierForRootSlow(instruction, root, root);
    }
  } else {
    // Plain GC root load with no read barrier.
    // /* GcRoot<mirror::Object> */ root = *(obj + offset)
    GetAssembler()->LoadFromOffset(kLoadWord, root_reg, obj, offset);
    // Note that GC roots are not affected by heap poisoning, thus we
    // do not have to unpoison `root_reg` here.
  }
  codegen_->MaybeGenerateMarkingRegisterCheck(/* code */ 18);
}

void CodeGeneratorARMVIXL::MaybeAddBakerCcEntrypointTempForFields(LocationSummary* locations) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);
  if (kBakerReadBarrierLinkTimeThunksEnableForFields) {
    if (!Runtime::Current()->UseJitCompilation()) {
      locations->AddTemp(Location::RegisterLocation(kBakerCcEntrypointRegister.GetCode()));
    }
  }
}

void CodeGeneratorARMVIXL::GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                 Location ref,
                                                                 vixl32::Register obj,
                                                                 uint32_t offset,
                                                                 Location temp,
                                                                 bool needs_null_check) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  if (kBakerReadBarrierLinkTimeThunksEnableForFields &&
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
    vixl32::Register ref_reg = RegisterFrom(ref, DataType::Type::kReference);
    bool narrow = CanEmitNarrowLdr(ref_reg, obj, offset);
    vixl32::Register base = obj;
    if (offset >= kReferenceLoadMinFarOffset) {
      base = RegisterFrom(temp);
      DCHECK(!base.Is(kBakerCcEntrypointRegister));
      static_assert(IsPowerOfTwo(kReferenceLoadMinFarOffset), "Expecting a power of 2.");
      __ Add(base, obj, Operand(offset & ~(kReferenceLoadMinFarOffset - 1u)));
      offset &= (kReferenceLoadMinFarOffset - 1u);
      // Use narrow LDR only for small offsets. Generating narrow encoding LDR for the large
      // offsets with `(offset & (kReferenceLoadMinFarOffset - 1u)) < 32u` would most likely
      // increase the overall code size when taking the generated thunks into account.
      DCHECK(!narrow);
    }
    UseScratchRegisterScope temps(GetVIXLAssembler());
    ExcludeIPAndBakerCcEntrypointRegister(&temps, instruction);
    uint32_t custom_data = linker::Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
        base.GetCode(), obj.GetCode(), narrow);
    vixl32::Label* bne_label = NewBakerReadBarrierPatch(custom_data);

    {
      vixl::EmissionCheckScope guard(
          GetVIXLAssembler(),
          (kPoisonHeapReferences ? 5u : 4u) * vixl32::kMaxInstructionSizeInBytes);
      vixl32::Label return_address;
      EmitAdrCode adr(GetVIXLAssembler(), lr, &return_address);
      __ cmp(mr, Operand(0));
      EmitPlaceholderBne(this, bne_label);
      ptrdiff_t old_offset = GetVIXLAssembler()->GetBuffer()->GetCursorOffset();
      __ ldr(EncodingSize(narrow ? Narrow : Wide), ref_reg, MemOperand(base, offset));
      if (needs_null_check) {
        MaybeRecordImplicitNullCheck(instruction);
      }
      // Note: We need a specific width for the unpoisoning NEG.
      if (kPoisonHeapReferences) {
        if (narrow) {
          // The only 16-bit encoding is T1 which sets flags outside IT block (i.e. RSBS, not RSB).
          __ rsbs(EncodingSize(Narrow), ref_reg, ref_reg, Operand(0));
        } else {
          __ rsb(EncodingSize(Wide), ref_reg, ref_reg, Operand(0));
        }
      }
      __ Bind(&return_address);
      DCHECK_EQ(old_offset - GetVIXLAssembler()->GetBuffer()->GetCursorOffset(),
                narrow ? BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET
                       : BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET);
    }
    MaybeGenerateMarkingRegisterCheck(/* code */ 19, /* temp_loc */ LocationFrom(ip));
    return;
  }

  // /* HeapReference<Object> */ ref = *(obj + offset)
  Location no_index = Location::NoLocation();
  ScaleFactor no_scale_factor = TIMES_1;
  GenerateReferenceLoadWithBakerReadBarrier(
      instruction, ref, obj, offset, no_index, no_scale_factor, temp, needs_null_check);
}

void CodeGeneratorARMVIXL::GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                 Location ref,
                                                                 vixl32::Register obj,
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
    vixl32::Register index_reg = RegisterFrom(index, DataType::Type::kInt32);
    vixl32::Register ref_reg = RegisterFrom(ref, DataType::Type::kReference);
    vixl32::Register data_reg = RegisterFrom(temp, DataType::Type::kInt32);  // Raw pointer.
    DCHECK(!data_reg.Is(kBakerCcEntrypointRegister));

    UseScratchRegisterScope temps(GetVIXLAssembler());
    ExcludeIPAndBakerCcEntrypointRegister(&temps, instruction);
    uint32_t custom_data =
        linker::Thumb2RelativePatcher::EncodeBakerReadBarrierArrayData(data_reg.GetCode());
    vixl32::Label* bne_label = NewBakerReadBarrierPatch(custom_data);

    __ Add(data_reg, obj, Operand(data_offset));
    {
      vixl::EmissionCheckScope guard(
          GetVIXLAssembler(),
          (kPoisonHeapReferences ? 5u : 4u) * vixl32::kMaxInstructionSizeInBytes);
      vixl32::Label return_address;
      EmitAdrCode adr(GetVIXLAssembler(), lr, &return_address);
      __ cmp(mr, Operand(0));
      EmitPlaceholderBne(this, bne_label);
      ptrdiff_t old_offset = GetVIXLAssembler()->GetBuffer()->GetCursorOffset();
      __ ldr(ref_reg, MemOperand(data_reg, index_reg, vixl32::LSL, scale_factor));
      DCHECK(!needs_null_check);  // The thunk cannot handle the null check.
      // Note: We need a Wide NEG for the unpoisoning.
      if (kPoisonHeapReferences) {
        __ rsb(EncodingSize(Wide), ref_reg, ref_reg, Operand(0));
      }
      __ Bind(&return_address);
      DCHECK_EQ(old_offset - GetVIXLAssembler()->GetBuffer()->GetCursorOffset(),
                BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET);
    }
    MaybeGenerateMarkingRegisterCheck(/* code */ 20, /* temp_loc */ LocationFrom(ip));
    return;
  }

  // /* HeapReference<Object> */ ref =
  //     *(obj + data_offset + index * sizeof(HeapReference<Object>))
  GenerateReferenceLoadWithBakerReadBarrier(
      instruction, ref, obj, data_offset, index, scale_factor, temp, needs_null_check);
}

void CodeGeneratorARMVIXL::GenerateReferenceLoadWithBakerReadBarrier(HInstruction* instruction,
                                                                     Location ref,
                                                                     vixl32::Register obj,
                                                                     uint32_t offset,
                                                                     Location index,
                                                                     ScaleFactor scale_factor,
                                                                     Location temp,
                                                                     bool needs_null_check) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

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

  vixl32::Register temp_reg = RegisterFrom(temp);

  // Slow path marking the object `ref` when the GC is marking. The
  // entrypoint will be loaded by the slow path code.
  SlowPathCodeARMVIXL* slow_path =
      new (GetScopedAllocator()) LoadReferenceWithBakerReadBarrierSlowPathARMVIXL(
          instruction, ref, obj, offset, index, scale_factor, needs_null_check, temp_reg);
  AddSlowPath(slow_path);

  __ CompareAndBranchIfNonZero(mr, slow_path->GetEntryLabel());
  // Fast path: the GC is not marking: just load the reference.
  GenerateRawReferenceLoad(instruction, ref, obj, offset, index, scale_factor, needs_null_check);
  __ Bind(slow_path->GetExitLabel());
  MaybeGenerateMarkingRegisterCheck(/* code */ 21);
}

void CodeGeneratorARMVIXL::UpdateReferenceFieldWithBakerReadBarrier(HInstruction* instruction,
                                                                    Location ref,
                                                                    vixl32::Register obj,
                                                                    Location field_offset,
                                                                    Location temp,
                                                                    bool needs_null_check,
                                                                    vixl32::Register temp2) {
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

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

  vixl32::Register temp_reg = RegisterFrom(temp);

  // Slow path updating the object reference at address `obj + field_offset`
  // when the GC is marking. The entrypoint will be loaded by the slow path code.
  SlowPathCodeARMVIXL* slow_path =
      new (GetScopedAllocator()) LoadReferenceWithBakerReadBarrierAndUpdateFieldSlowPathARMVIXL(
          instruction,
          ref,
          obj,
          /* offset */ 0u,
          /* index */ field_offset,
          /* scale_factor */ ScaleFactor::TIMES_1,
          needs_null_check,
          temp_reg,
          temp2);
  AddSlowPath(slow_path);

  __ CompareAndBranchIfNonZero(mr, slow_path->GetEntryLabel());
  // Fast path: the GC is not marking: nothing to do (the field is
  // up-to-date, and we don't need to load the reference).
  __ Bind(slow_path->GetExitLabel());
  MaybeGenerateMarkingRegisterCheck(/* code */ 22);
}

void CodeGeneratorARMVIXL::GenerateRawReferenceLoad(HInstruction* instruction,
                                                    Location ref,
                                                    vixl::aarch32::Register obj,
                                                    uint32_t offset,
                                                    Location index,
                                                    ScaleFactor scale_factor,
                                                    bool needs_null_check) {
  DataType::Type type = DataType::Type::kReference;
  vixl32::Register ref_reg = RegisterFrom(ref, type);

  // If needed, vixl::EmissionCheckScope guards are used to ensure
  // that no pools are emitted between the load (macro) instruction
  // and MaybeRecordImplicitNullCheck.

  if (index.IsValid()) {
    // Load types involving an "index": ArrayGet,
    // UnsafeGetObject/UnsafeGetObjectVolatile and UnsafeCASObject
    // intrinsics.
    // /* HeapReference<mirror::Object> */ ref = *(obj + offset + (index << scale_factor))
    if (index.IsConstant()) {
      size_t computed_offset =
          (Int32ConstantFrom(index) << scale_factor) + offset;
      vixl::EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
      GetAssembler()->LoadFromOffset(kLoadWord, ref_reg, obj, computed_offset);
      if (needs_null_check) {
        MaybeRecordImplicitNullCheck(instruction);
      }
    } else {
      // Handle the special case of the
      // UnsafeGetObject/UnsafeGetObjectVolatile and UnsafeCASObject
      // intrinsics, which use a register pair as index ("long
      // offset"), of which only the low part contains data.
      vixl32::Register index_reg = index.IsRegisterPair()
          ? LowRegisterFrom(index)
          : RegisterFrom(index);
      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register temp = temps.Acquire();
      __ Add(temp, obj, Operand(index_reg, ShiftType::LSL, scale_factor));
      {
        vixl::EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
        GetAssembler()->LoadFromOffset(kLoadWord, ref_reg, temp, offset);
        if (needs_null_check) {
          MaybeRecordImplicitNullCheck(instruction);
        }
      }
    }
  } else {
    // /* HeapReference<mirror::Object> */ ref = *(obj + offset)
    vixl::EmissionCheckScope guard(GetVIXLAssembler(), kMaxMacroInstructionSizeInBytes);
    GetAssembler()->LoadFromOffset(kLoadWord, ref_reg, obj, offset);
    if (needs_null_check) {
      MaybeRecordImplicitNullCheck(instruction);
    }
  }

  // Object* ref = ref_addr->AsMirrorPtr()
  GetAssembler()->MaybeUnpoisonHeapReference(ref_reg);
}

void CodeGeneratorARMVIXL::MaybeGenerateMarkingRegisterCheck(int code, Location temp_loc) {
  // The following condition is a compile-time one, so it does not have a run-time cost.
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier && kIsDebugBuild) {
    // The following condition is a run-time one; it is executed after the
    // previous compile-time test, to avoid penalizing non-debug builds.
    if (GetCompilerOptions().EmitRunTimeChecksInDebugMode()) {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register temp = temp_loc.IsValid() ? RegisterFrom(temp_loc) : temps.Acquire();
      GetAssembler()->GenerateMarkingRegisterCheck(temp,
                                                   kMarkingRegisterCheckBreakCodeBaseCode + code);
    }
  }
}

void CodeGeneratorARMVIXL::GenerateReadBarrierSlow(HInstruction* instruction,
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
  SlowPathCodeARMVIXL* slow_path = new (GetScopedAllocator())
      ReadBarrierForHeapReferenceSlowPathARMVIXL(instruction, out, ref, obj, offset, index);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

void CodeGeneratorARMVIXL::MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                                        Location out,
                                                        Location ref,
                                                        Location obj,
                                                        uint32_t offset,
                                                        Location index) {
  if (kEmitCompilerReadBarrier) {
    // Baker's read barriers shall be handled by the fast path
    // (CodeGeneratorARMVIXL::GenerateReferenceLoadWithBakerReadBarrier).
    DCHECK(!kUseBakerReadBarrier);
    // If heap poisoning is enabled, unpoisoning will be taken care of
    // by the runtime within the slow path.
    GenerateReadBarrierSlow(instruction, out, ref, obj, offset, index);
  } else if (kPoisonHeapReferences) {
    GetAssembler()->UnpoisonHeapReference(RegisterFrom(out));
  }
}

void CodeGeneratorARMVIXL::GenerateReadBarrierForRootSlow(HInstruction* instruction,
                                                          Location out,
                                                          Location root) {
  DCHECK(kEmitCompilerReadBarrier);

  // Insert a slow path based read barrier *after* the GC root load.
  //
  // Note that GC roots are not affected by heap poisoning, so we do
  // not need to do anything special for this here.
  SlowPathCodeARMVIXL* slow_path =
      new (GetScopedAllocator()) ReadBarrierForRootSlowPathARMVIXL(instruction, out, root);
  AddSlowPath(slow_path);

  __ B(slow_path->GetEntryLabel());
  __ Bind(slow_path->GetExitLabel());
}

// Check if the desired_dispatch_info is supported. If it is, return it,
// otherwise return a fall-back info that should be used instead.
HInvokeStaticOrDirect::DispatchInfo CodeGeneratorARMVIXL::GetSupportedInvokeStaticOrDirectDispatch(
    const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
    HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
  return desired_dispatch_info;
}

vixl32::Register CodeGeneratorARMVIXL::GetInvokeStaticOrDirectExtraParameter(
    HInvokeStaticOrDirect* invoke, vixl32::Register temp) {
  DCHECK_EQ(invoke->InputCount(), invoke->GetNumberOfArguments() + 1u);
  Location location = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
  if (!invoke->GetLocations()->Intrinsified()) {
    return RegisterFrom(location);
  }
  // For intrinsics we allow any location, so it may be on the stack.
  if (!location.IsRegister()) {
    GetAssembler()->LoadFromOffset(kLoadWord, temp, sp, location.GetStackIndex());
    return temp;
  }
  // For register locations, check if the register was saved. If so, get it from the stack.
  // Note: There is a chance that the register was saved but not overwritten, so we could
  // save one load. However, since this is just an intrinsic slow path we prefer this
  // simple and more robust approach rather that trying to determine if that's the case.
  SlowPathCode* slow_path = GetCurrentSlowPath();
  if (slow_path != nullptr && slow_path->IsCoreRegisterSaved(RegisterFrom(location).GetCode())) {
    int stack_offset = slow_path->GetStackOffsetOfCoreRegister(RegisterFrom(location).GetCode());
    GetAssembler()->LoadFromOffset(kLoadWord, temp, sp, stack_offset);
    return temp;
  }
  return RegisterFrom(location);
}

void CodeGeneratorARMVIXL::GenerateStaticOrDirectCall(
    HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path) {
  Location callee_method = temp;  // For all kinds except kRecursive, callee will be in temp.
  switch (invoke->GetMethodLoadKind()) {
    case HInvokeStaticOrDirect::MethodLoadKind::kStringInit: {
      uint32_t offset =
          GetThreadOffset<kArmPointerSize>(invoke->GetStringInitEntryPoint()).Int32Value();
      // temp = thread->string_init_entrypoint
      GetAssembler()->LoadFromOffset(kLoadWord, RegisterFrom(temp), tr, offset);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kRecursive:
      callee_method = invoke->GetLocations()->InAt(invoke->GetSpecialInputIndex());
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBootImageLinkTimePcRelative: {
      DCHECK(GetCompilerOptions().IsBootImage());
      PcRelativePatchInfo* labels = NewBootImageMethodPatch(invoke->GetTargetMethod());
      vixl32::Register temp_reg = RegisterFrom(temp);
      EmitMovwMovtPlaceholder(labels, temp_reg);
      break;
    }
    case HInvokeStaticOrDirect::MethodLoadKind::kDirectAddress:
      __ Mov(RegisterFrom(temp), Operand::From(invoke->GetMethodAddress()));
      break;
    case HInvokeStaticOrDirect::MethodLoadKind::kBssEntry: {
      PcRelativePatchInfo* labels = NewMethodBssEntryPatch(
          MethodReference(&GetGraph()->GetDexFile(), invoke->GetDexMethodIndex()));
      vixl32::Register temp_reg = RegisterFrom(temp);
      EmitMovwMovtPlaceholder(labels, temp_reg);
      GetAssembler()->LoadFromOffset(kLoadWord, temp_reg, temp_reg, /* offset*/ 0);
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
        ExactAssemblyScope aas(GetVIXLAssembler(),
                               vixl32::k32BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kMaximumSize);
        __ bl(GetFrameEntryLabel());
        RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      }
      break;
    case HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod:
      // LR = callee_method->entry_point_from_quick_compiled_code_
      GetAssembler()->LoadFromOffset(
            kLoadWord,
            lr,
            RegisterFrom(callee_method),
            ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArmPointerSize).Int32Value());
      {
        // Use a scope to help guarantee that `RecordPcInfo()` records the correct pc.
        // blx in T32 has only 16bit encoding that's why a stricter check for the scope is used.
        ExactAssemblyScope aas(GetVIXLAssembler(),
                               vixl32::k16BitT32InstructionSizeInBytes,
                               CodeBufferCheckScope::kExactSize);
        // LR()
        __ blx(lr);
        RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
      }
      break;
  }

  DCHECK(!IsLeafMethod());
}

void CodeGeneratorARMVIXL::GenerateVirtualCall(
    HInvokeVirtual* invoke, Location temp_location, SlowPathCode* slow_path) {
  vixl32::Register temp = RegisterFrom(temp_location);
  uint32_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), kArmPointerSize).Uint32Value();

  // Use the calling convention instead of the location of the receiver, as
  // intrinsics may have put the receiver in a different register. In the intrinsics
  // slow path, the arguments have been moved to the right place, so here we are
  // guaranteed that the receiver is the first register of the calling convention.
  InvokeDexCallingConventionARMVIXL calling_convention;
  vixl32::Register receiver = calling_convention.GetRegisterAt(0);
  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  {
    // Make sure the pc is recorded immediately after the `ldr` instruction.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    // /* HeapReference<Class> */ temp = receiver->klass_
    __ ldr(temp, MemOperand(receiver, class_offset));
    MaybeRecordImplicitNullCheck(invoke);
  }
  // Instead of simply (possibly) unpoisoning `temp` here, we should
  // emit a read barrier for the previous class reference load.
  // However this is not required in practice, as this is an
  // intermediate/temporary reference and because the current
  // concurrent copying collector keeps the from-space memory
  // intact/accessible until the end of the marking phase (the
  // concurrent copying collector may not in the future).
  GetAssembler()->MaybeUnpoisonHeapReference(temp);

  // temp = temp->GetMethodAt(method_offset);
  uint32_t entry_point = ArtMethod::EntryPointFromQuickCompiledCodeOffset(
      kArmPointerSize).Int32Value();
  GetAssembler()->LoadFromOffset(kLoadWord, temp, temp, method_offset);
  // LR = temp->GetEntryPoint();
  GetAssembler()->LoadFromOffset(kLoadWord, lr, temp, entry_point);
  {
    // Use a scope to help guarantee that `RecordPcInfo()` records the correct pc.
    // blx in T32 has only 16bit encoding that's why a stricter check for the scope is used.
    ExactAssemblyScope aas(GetVIXLAssembler(),
                           vixl32::k16BitT32InstructionSizeInBytes,
                           CodeBufferCheckScope::kExactSize);
    // LR();
    __ blx(lr);
    RecordPcInfo(invoke, invoke->GetDexPc(), slow_path);
  }
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewBootImageMethodPatch(
    MethodReference target_method) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, &boot_image_method_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewMethodBssEntryPatch(
    MethodReference target_method) {
  return NewPcRelativePatch(
      target_method.dex_file, target_method.index, &method_bss_entry_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewBootImageTypePatch(
    const DexFile& dex_file, dex::TypeIndex type_index) {
  return NewPcRelativePatch(&dex_file, type_index.index_, &boot_image_type_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewTypeBssEntryPatch(
    const DexFile& dex_file, dex::TypeIndex type_index) {
  return NewPcRelativePatch(&dex_file, type_index.index_, &type_bss_entry_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewBootImageStringPatch(
    const DexFile& dex_file, dex::StringIndex string_index) {
  return NewPcRelativePatch(&dex_file, string_index.index_, &boot_image_string_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewStringBssEntryPatch(
    const DexFile& dex_file, dex::StringIndex string_index) {
  return NewPcRelativePatch(&dex_file, string_index.index_, &string_bss_entry_patches_);
}

CodeGeneratorARMVIXL::PcRelativePatchInfo* CodeGeneratorARMVIXL::NewPcRelativePatch(
    const DexFile* dex_file, uint32_t offset_or_index, ArenaDeque<PcRelativePatchInfo>* patches) {
  patches->emplace_back(dex_file, offset_or_index);
  return &patches->back();
}

vixl::aarch32::Label* CodeGeneratorARMVIXL::NewBakerReadBarrierPatch(uint32_t custom_data) {
  baker_read_barrier_patches_.emplace_back(custom_data);
  return &baker_read_barrier_patches_.back().label;
}

VIXLUInt32Literal* CodeGeneratorARMVIXL::DeduplicateBootImageAddressLiteral(uint32_t address) {
  return DeduplicateUint32Literal(dchecked_integral_cast<uint32_t>(address), &uint32_literals_);
}

VIXLUInt32Literal* CodeGeneratorARMVIXL::DeduplicateJitStringLiteral(
    const DexFile& dex_file,
    dex::StringIndex string_index,
    Handle<mirror::String> handle) {
  ReserveJitStringRoot(StringReference(&dex_file, string_index), handle);
  return jit_string_patches_.GetOrCreate(
      StringReference(&dex_file, string_index),
      [this]() {
        return GetAssembler()->CreateLiteralDestroyedWithPool<uint32_t>(/* placeholder */ 0u);
      });
}

VIXLUInt32Literal* CodeGeneratorARMVIXL::DeduplicateJitClassLiteral(const DexFile& dex_file,
                                                      dex::TypeIndex type_index,
                                                      Handle<mirror::Class> handle) {
  ReserveJitClassRoot(TypeReference(&dex_file, type_index), handle);
  return jit_class_patches_.GetOrCreate(
      TypeReference(&dex_file, type_index),
      [this]() {
        return GetAssembler()->CreateLiteralDestroyedWithPool<uint32_t>(/* placeholder */ 0u);
      });
}

template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
inline void CodeGeneratorARMVIXL::EmitPcRelativeLinkerPatches(
    const ArenaDeque<PcRelativePatchInfo>& infos,
    ArenaVector<linker::LinkerPatch>* linker_patches) {
  for (const PcRelativePatchInfo& info : infos) {
    const DexFile* dex_file = info.target_dex_file;
    size_t offset_or_index = info.offset_or_index;
    DCHECK(info.add_pc_label.IsBound());
    uint32_t add_pc_offset = dchecked_integral_cast<uint32_t>(info.add_pc_label.GetLocation());
    // Add MOVW patch.
    DCHECK(info.movw_label.IsBound());
    uint32_t movw_offset = dchecked_integral_cast<uint32_t>(info.movw_label.GetLocation());
    linker_patches->push_back(Factory(movw_offset, dex_file, add_pc_offset, offset_or_index));
    // Add MOVT patch.
    DCHECK(info.movt_label.IsBound());
    uint32_t movt_offset = dchecked_integral_cast<uint32_t>(info.movt_label.GetLocation());
    linker_patches->push_back(Factory(movt_offset, dex_file, add_pc_offset, offset_or_index));
  }
}

void CodeGeneratorARMVIXL::EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) {
  DCHECK(linker_patches->empty());
  size_t size =
      /* MOVW+MOVT for each entry */ 2u * boot_image_method_patches_.size() +
      /* MOVW+MOVT for each entry */ 2u * method_bss_entry_patches_.size() +
      /* MOVW+MOVT for each entry */ 2u * boot_image_type_patches_.size() +
      /* MOVW+MOVT for each entry */ 2u * type_bss_entry_patches_.size() +
      /* MOVW+MOVT for each entry */ 2u * boot_image_string_patches_.size() +
      /* MOVW+MOVT for each entry */ 2u * string_bss_entry_patches_.size() +
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

VIXLUInt32Literal* CodeGeneratorARMVIXL::DeduplicateUint32Literal(
    uint32_t value,
    Uint32ToLiteralMap* map) {
  return map->GetOrCreate(
      value,
      [this, value]() {
        return GetAssembler()->CreateLiteralDestroyedWithPool<uint32_t>(/* placeholder */ value);
      });
}

void LocationsBuilderARMVIXL::VisitMultiplyAccumulate(HMultiplyAccumulate* instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instr, LocationSummary::kNoCall);
  locations->SetInAt(HMultiplyAccumulate::kInputAccumulatorIndex,
                     Location::RequiresRegister());
  locations->SetInAt(HMultiplyAccumulate::kInputMulLeftIndex, Location::RequiresRegister());
  locations->SetInAt(HMultiplyAccumulate::kInputMulRightIndex, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void InstructionCodeGeneratorARMVIXL::VisitMultiplyAccumulate(HMultiplyAccumulate* instr) {
  vixl32::Register res = OutputRegister(instr);
  vixl32::Register accumulator =
      InputRegisterAt(instr, HMultiplyAccumulate::kInputAccumulatorIndex);
  vixl32::Register mul_left =
      InputRegisterAt(instr, HMultiplyAccumulate::kInputMulLeftIndex);
  vixl32::Register mul_right =
      InputRegisterAt(instr, HMultiplyAccumulate::kInputMulRightIndex);

  if (instr->GetOpKind() == HInstruction::kAdd) {
    __ Mla(res, mul_left, mul_right, accumulator);
  } else {
    __ Mls(res, mul_left, mul_right, accumulator);
  }
}

void LocationsBuilderARMVIXL::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

void InstructionCodeGeneratorARMVIXL::VisitBoundType(HBoundType* instruction ATTRIBUTE_UNUSED) {
  // Nothing to do, this should be removed during prepare for register allocator.
  LOG(FATAL) << "Unreachable";
}

// Simple implementation of packed switch - generate cascaded compare/jumps.
void LocationsBuilderARMVIXL::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(switch_instr, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  if (switch_instr->GetNumEntries() > kPackedSwitchCompareJumpThreshold &&
      codegen_->GetAssembler()->GetVIXLAssembler()->IsUsingT32()) {
    locations->AddTemp(Location::RequiresRegister());  // We need a temp for the table base.
    if (switch_instr->GetStartValue() != 0) {
      locations->AddTemp(Location::RequiresRegister());  // We need a temp for the bias.
    }
  }
}

// TODO(VIXL): Investigate and reach the parity with old arm codegen.
void InstructionCodeGeneratorARMVIXL::VisitPackedSwitch(HPackedSwitch* switch_instr) {
  int32_t lower_bound = switch_instr->GetStartValue();
  uint32_t num_entries = switch_instr->GetNumEntries();
  LocationSummary* locations = switch_instr->GetLocations();
  vixl32::Register value_reg = InputRegisterAt(switch_instr, 0);
  HBasicBlock* default_block = switch_instr->GetDefaultBlock();

  if (num_entries <= kPackedSwitchCompareJumpThreshold ||
      !codegen_->GetAssembler()->GetVIXLAssembler()->IsUsingT32()) {
    // Create a series of compare/jumps.
    UseScratchRegisterScope temps(GetVIXLAssembler());
    vixl32::Register temp_reg = temps.Acquire();
    // Note: It is fine for the below AddConstantSetFlags() using IP register to temporarily store
    // the immediate, because IP is used as the destination register. For the other
    // AddConstantSetFlags() and GenerateCompareWithImmediate(), the immediate values are constant,
    // and they can be encoded in the instruction without making use of IP register.
    __ Adds(temp_reg, value_reg, -lower_bound);

    const ArenaVector<HBasicBlock*>& successors = switch_instr->GetBlock()->GetSuccessors();
    // Jump to successors[0] if value == lower_bound.
    __ B(eq, codegen_->GetLabelOf(successors[0]));
    int32_t last_index = 0;
    for (; num_entries - last_index > 2; last_index += 2) {
      __ Adds(temp_reg, temp_reg, -2);
      // Jump to successors[last_index + 1] if value < case_value[last_index + 2].
      __ B(lo, codegen_->GetLabelOf(successors[last_index + 1]));
      // Jump to successors[last_index + 2] if value == case_value[last_index + 2].
      __ B(eq, codegen_->GetLabelOf(successors[last_index + 2]));
    }
    if (num_entries - last_index == 2) {
      // The last missing case_value.
      __ Cmp(temp_reg, 1);
      __ B(eq, codegen_->GetLabelOf(successors[last_index + 1]));
    }

    // And the default for any other value.
    if (!codegen_->GoesToNextBlock(switch_instr->GetBlock(), default_block)) {
      __ B(codegen_->GetLabelOf(default_block));
    }
  } else {
    // Create a table lookup.
    vixl32::Register table_base = RegisterFrom(locations->GetTemp(0));

    JumpTableARMVIXL* jump_table = codegen_->CreateJumpTable(switch_instr);

    // Remove the bias.
    vixl32::Register key_reg;
    if (lower_bound != 0) {
      key_reg = RegisterFrom(locations->GetTemp(1));
      __ Sub(key_reg, value_reg, lower_bound);
    } else {
      key_reg = value_reg;
    }

    // Check whether the value is in the table, jump to default block if not.
    __ Cmp(key_reg, num_entries - 1);
    __ B(hi, codegen_->GetLabelOf(default_block));

    UseScratchRegisterScope temps(GetVIXLAssembler());
    vixl32::Register jump_offset = temps.Acquire();

    // Load jump offset from the table.
    {
      const size_t jump_size = switch_instr->GetNumEntries() * sizeof(int32_t);
      ExactAssemblyScope aas(GetVIXLAssembler(),
                             (vixl32::kMaxInstructionSizeInBytes * 4) + jump_size,
                             CodeBufferCheckScope::kMaximumSize);
      __ adr(table_base, jump_table->GetTableStartLabel());
      __ ldr(jump_offset, MemOperand(table_base, key_reg, vixl32::LSL, 2));

      // Jump to target block by branching to table_base(pc related) + offset.
      vixl32::Register target_address = table_base;
      __ add(target_address, table_base, jump_offset);
      __ bx(target_address);

      jump_table->EmitTable(codegen_);
    }
  }
}

// Copy the result of a call into the given target.
void CodeGeneratorARMVIXL::MoveFromReturnRegister(Location trg, DataType::Type type) {
  if (!trg.IsValid()) {
    DCHECK_EQ(type, DataType::Type::kVoid);
    return;
  }

  DCHECK_NE(type, DataType::Type::kVoid);

  Location return_loc = InvokeDexCallingConventionVisitorARMVIXL().GetReturnLocation(type);
  if (return_loc.Equals(trg)) {
    return;
  }

  // TODO: Consider pairs in the parallel move resolver, then this could be nicely merged
  //       with the last branch.
  if (type == DataType::Type::kInt64) {
    TODO_VIXL32(FATAL);
  } else if (type == DataType::Type::kFloat64) {
    TODO_VIXL32(FATAL);
  } else {
    // Let the parallel move resolver take care of all of this.
    HParallelMove parallel_move(GetGraph()->GetAllocator());
    parallel_move.AddMove(return_loc, trg, type, nullptr);
    GetMoveResolver()->EmitNativeCode(&parallel_move);
  }
}

void LocationsBuilderARMVIXL::VisitClassTableGet(HClassTableGet* instruction) {
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

void InstructionCodeGeneratorARMVIXL::VisitClassTableGet(HClassTableGet* instruction) {
  if (instruction->GetTableKind() == HClassTableGet::TableKind::kVTable) {
    uint32_t method_offset = mirror::Class::EmbeddedVTableEntryOffset(
        instruction->GetIndex(), kArmPointerSize).SizeValue();
    GetAssembler()->LoadFromOffset(kLoadWord,
                                   OutputRegister(instruction),
                                   InputRegisterAt(instruction, 0),
                                   method_offset);
  } else {
    uint32_t method_offset = static_cast<uint32_t>(ImTable::OffsetOfElement(
        instruction->GetIndex(), kArmPointerSize));
    GetAssembler()->LoadFromOffset(kLoadWord,
                                   OutputRegister(instruction),
                                   InputRegisterAt(instruction, 0),
                                   mirror::Class::ImtPtrOffset(kArmPointerSize).Uint32Value());
    GetAssembler()->LoadFromOffset(kLoadWord,
                                   OutputRegister(instruction),
                                   OutputRegister(instruction),
                                   method_offset);
  }
}

static void PatchJitRootUse(uint8_t* code,
                            const uint8_t* roots_data,
                            VIXLUInt32Literal* literal,
                            uint64_t index_in_table) {
  DCHECK(literal->IsBound());
  uint32_t literal_offset = literal->GetLocation();
  uintptr_t address =
      reinterpret_cast<uintptr_t>(roots_data) + index_in_table * sizeof(GcRoot<mirror::Object>);
  uint8_t* data = code + literal_offset;
  reinterpret_cast<uint32_t*>(data)[0] = dchecked_integral_cast<uint32_t>(address);
}

void CodeGeneratorARMVIXL::EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) {
  for (const auto& entry : jit_string_patches_) {
    const StringReference& string_reference = entry.first;
    VIXLUInt32Literal* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitStringRootIndex(string_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
  for (const auto& entry : jit_class_patches_) {
    const TypeReference& type_reference = entry.first;
    VIXLUInt32Literal* table_entry_literal = entry.second;
    uint64_t index_in_table = GetJitClassRootIndex(type_reference);
    PatchJitRootUse(code, roots_data, table_entry_literal, index_in_table);
  }
}

void CodeGeneratorARMVIXL::EmitMovwMovtPlaceholder(
    CodeGeneratorARMVIXL::PcRelativePatchInfo* labels,
    vixl32::Register out) {
  ExactAssemblyScope aas(GetVIXLAssembler(),
                         3 * vixl32::kMaxInstructionSizeInBytes,
                         CodeBufferCheckScope::kMaximumSize);
  // TODO(VIXL): Think about using mov instead of movw.
  __ bind(&labels->movw_label);
  __ movw(out, /* placeholder */ 0u);
  __ bind(&labels->movt_label);
  __ movt(out, /* placeholder */ 0u);
  __ bind(&labels->add_pc_label);
  __ add(out, out, pc);
}

#undef __
#undef QUICK_ENTRY_POINT
#undef TODO_VIXL32

}  // namespace arm
}  // namespace art
