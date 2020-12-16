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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_H_

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "base/enums.h"
#include "dex/string_reference.h"
#include "dex/type_reference.h"
#include "globals.h"
#include "graph_visualizer.h"
#include "locations.h"
#include "memory_region.h"
#include "nodes.h"
#include "optimizing_compiler_stats.h"
#include "read_barrier_option.h"
#include "stack.h"
#include "stack_map.h"
#include "utils/label.h"

namespace art {

// Binary encoding of 2^32 for type double.
static int64_t constexpr k2Pow32EncodingForDouble = INT64_C(0x41F0000000000000);
// Binary encoding of 2^31 for type double.
static int64_t constexpr k2Pow31EncodingForDouble = INT64_C(0x41E0000000000000);

// Minimum value for a primitive integer.
static int32_t constexpr kPrimIntMin = 0x80000000;
// Minimum value for a primitive long.
static int64_t constexpr kPrimLongMin = INT64_C(0x8000000000000000);

// Maximum value for a primitive integer.
static int32_t constexpr kPrimIntMax = 0x7fffffff;
// Maximum value for a primitive long.
static int64_t constexpr kPrimLongMax = INT64_C(0x7fffffffffffffff);

static constexpr ReadBarrierOption kCompilerReadBarrierOption =
    kEmitCompilerReadBarrier ? kWithReadBarrier : kWithoutReadBarrier;

class Assembler;
class CodeGenerator;
class CompilerDriver;
class CompilerOptions;
class StackMapStream;
class ParallelMoveResolver;

namespace linker {
class LinkerPatch;
}  // namespace linker

class CodeAllocator {
 public:
  CodeAllocator() {}
  virtual ~CodeAllocator() {}

  virtual uint8_t* Allocate(size_t size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeAllocator);
};

class SlowPathCode : public DeletableArenaObject<kArenaAllocSlowPaths> {
 public:
  explicit SlowPathCode(HInstruction* instruction) : instruction_(instruction) {
    for (size_t i = 0; i < kMaximumNumberOfExpectedRegisters; ++i) {
      saved_core_stack_offsets_[i] = kRegisterNotSaved;
      saved_fpu_stack_offsets_[i] = kRegisterNotSaved;
    }
  }

  virtual ~SlowPathCode() {}

  virtual void EmitNativeCode(CodeGenerator* codegen) = 0;

  // Save live core and floating-point caller-save registers and
  // update the stack mask in `locations` for registers holding object
  // references.
  virtual void SaveLiveRegisters(CodeGenerator* codegen, LocationSummary* locations);
  // Restore live core and floating-point caller-save registers.
  virtual void RestoreLiveRegisters(CodeGenerator* codegen, LocationSummary* locations);

  bool IsCoreRegisterSaved(int reg) const {
    return saved_core_stack_offsets_[reg] != kRegisterNotSaved;
  }

  bool IsFpuRegisterSaved(int reg) const {
    return saved_fpu_stack_offsets_[reg] != kRegisterNotSaved;
  }

  uint32_t GetStackOffsetOfCoreRegister(int reg) const {
    return saved_core_stack_offsets_[reg];
  }

  uint32_t GetStackOffsetOfFpuRegister(int reg) const {
    return saved_fpu_stack_offsets_[reg];
  }

  virtual bool IsFatal() const { return false; }

  virtual const char* GetDescription() const = 0;

  Label* GetEntryLabel() { return &entry_label_; }
  Label* GetExitLabel() { return &exit_label_; }

  HInstruction* GetInstruction() const {
    return instruction_;
  }

  uint32_t GetDexPc() const {
    return instruction_ != nullptr ? instruction_->GetDexPc() : kNoDexPc;
  }

 protected:
  static constexpr size_t kMaximumNumberOfExpectedRegisters = 32;
  static constexpr uint32_t kRegisterNotSaved = -1;
  // The instruction where this slow path is happening.
  HInstruction* instruction_;
  uint32_t saved_core_stack_offsets_[kMaximumNumberOfExpectedRegisters];
  uint32_t saved_fpu_stack_offsets_[kMaximumNumberOfExpectedRegisters];

 private:
  Label entry_label_;
  Label exit_label_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathCode);
};

class InvokeDexCallingConventionVisitor {
 public:
  virtual Location GetNextLocation(DataType::Type type) = 0;
  virtual Location GetReturnLocation(DataType::Type type) const = 0;
  virtual Location GetMethodLocation() const = 0;

 protected:
  InvokeDexCallingConventionVisitor() {}
  virtual ~InvokeDexCallingConventionVisitor() {}

  // The current index for core registers.
  uint32_t gp_index_ = 0u;
  // The current index for floating-point registers.
  uint32_t float_index_ = 0u;
  // The current stack index.
  uint32_t stack_index_ = 0u;

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionVisitor);
};

class FieldAccessCallingConvention {
 public:
  virtual Location GetObjectLocation() const = 0;
  virtual Location GetFieldIndexLocation() const = 0;
  virtual Location GetReturnLocation(DataType::Type type) const = 0;
  virtual Location GetSetValueLocation(DataType::Type type, bool is_instance) const = 0;
  virtual Location GetFpuLocation(DataType::Type type) const = 0;
  virtual ~FieldAccessCallingConvention() {}

 protected:
  FieldAccessCallingConvention() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAccessCallingConvention);
};

class CodeGenerator : public DeletableArenaObject<kArenaAllocCodeGenerator> {
 public:
  // Compiles the graph to executable instructions.
  void Compile(CodeAllocator* allocator);
  static std::unique_ptr<CodeGenerator> Create(HGraph* graph,
                                               InstructionSet instruction_set,
                                               const InstructionSetFeatures& isa_features,
                                               const CompilerOptions& compiler_options,
                                               OptimizingCompilerStats* stats = nullptr);
  virtual ~CodeGenerator();

  // Get the graph. This is the outermost graph, never the graph of a method being inlined.
  HGraph* GetGraph() const { return graph_; }

  HBasicBlock* GetNextBlockToEmit() const;
  HBasicBlock* FirstNonEmptyBlock(HBasicBlock* block) const;
  bool GoesToNextBlock(HBasicBlock* current, HBasicBlock* next) const;

  size_t GetStackSlotOfParameter(HParameterValue* parameter) const {
    // Note that this follows the current calling convention.
    return GetFrameSize()
        + static_cast<size_t>(InstructionSetPointerSize(GetInstructionSet()))  // Art method
        + parameter->GetIndex() * kVRegSize;
  }

  virtual void Initialize() = 0;
  virtual void Finalize(CodeAllocator* allocator);
  virtual void EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches);
  virtual void GenerateFrameEntry() = 0;
  virtual void GenerateFrameExit() = 0;
  virtual void Bind(HBasicBlock* block) = 0;
  virtual void MoveConstant(Location destination, int32_t value) = 0;
  virtual void MoveLocation(Location dst, Location src, DataType::Type dst_type) = 0;
  virtual void AddLocationAsTemp(Location location, LocationSummary* locations) = 0;

  virtual Assembler* GetAssembler() = 0;
  virtual const Assembler& GetAssembler() const = 0;
  virtual size_t GetWordSize() const = 0;
  virtual size_t GetFloatingPointSpillSlotSize() const = 0;
  virtual uintptr_t GetAddressOf(HBasicBlock* block) = 0;
  void InitializeCodeGeneration(size_t number_of_spill_slots,
                                size_t maximum_safepoint_spill_size,
                                size_t number_of_out_slots,
                                const ArenaVector<HBasicBlock*>& block_order);
  // Backends can override this as necessary. For most, no special alignment is required.
  virtual uint32_t GetPreferredSlotsAlignment() const { return 1; }

  uint32_t GetFrameSize() const { return frame_size_; }
  void SetFrameSize(uint32_t size) { frame_size_ = size; }
  uint32_t GetCoreSpillMask() const { return core_spill_mask_; }
  uint32_t GetFpuSpillMask() const { return fpu_spill_mask_; }

  size_t GetNumberOfCoreRegisters() const { return number_of_core_registers_; }
  size_t GetNumberOfFloatingPointRegisters() const { return number_of_fpu_registers_; }
  virtual void SetupBlockedRegisters() const = 0;

  virtual void ComputeSpillMask() {
    core_spill_mask_ = allocated_registers_.GetCoreRegisters() & core_callee_save_mask_;
    DCHECK_NE(core_spill_mask_, 0u) << "At least the return address register must be saved";
    fpu_spill_mask_ = allocated_registers_.GetFloatingPointRegisters() & fpu_callee_save_mask_;
  }

  static uint32_t ComputeRegisterMask(const int* registers, size_t length) {
    uint32_t mask = 0;
    for (size_t i = 0, e = length; i < e; ++i) {
      mask |= (1 << registers[i]);
    }
    return mask;
  }

  virtual void DumpCoreRegister(std::ostream& stream, int reg) const = 0;
  virtual void DumpFloatingPointRegister(std::ostream& stream, int reg) const = 0;
  virtual InstructionSet GetInstructionSet() const = 0;

  const CompilerOptions& GetCompilerOptions() const { return compiler_options_; }

  // Saves the register in the stack. Returns the size taken on stack.
  virtual size_t SaveCoreRegister(size_t stack_index, uint32_t reg_id) = 0;
  // Restores the register from the stack. Returns the size taken on stack.
  virtual size_t RestoreCoreRegister(size_t stack_index, uint32_t reg_id) = 0;

  virtual size_t SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) = 0;
  virtual size_t RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) = 0;

  virtual bool NeedsTwoRegisters(DataType::Type type) const = 0;
  // Returns whether we should split long moves in parallel moves.
  virtual bool ShouldSplitLongMoves() const { return false; }

  size_t GetNumberOfCoreCalleeSaveRegisters() const {
    return POPCOUNT(core_callee_save_mask_);
  }

  size_t GetNumberOfCoreCallerSaveRegisters() const {
    DCHECK_GE(GetNumberOfCoreRegisters(), GetNumberOfCoreCalleeSaveRegisters());
    return GetNumberOfCoreRegisters() - GetNumberOfCoreCalleeSaveRegisters();
  }

  bool IsCoreCalleeSaveRegister(int reg) const {
    return (core_callee_save_mask_ & (1 << reg)) != 0;
  }

  bool IsFloatingPointCalleeSaveRegister(int reg) const {
    return (fpu_callee_save_mask_ & (1 << reg)) != 0;
  }

  uint32_t GetSlowPathSpills(LocationSummary* locations, bool core_registers) const {
    DCHECK(locations->OnlyCallsOnSlowPath() ||
           (locations->Intrinsified() && locations->CallsOnMainAndSlowPath() &&
               !locations->HasCustomSlowPathCallingConvention()));
    uint32_t live_registers = core_registers
        ? locations->GetLiveRegisters()->GetCoreRegisters()
        : locations->GetLiveRegisters()->GetFloatingPointRegisters();
    if (locations->HasCustomSlowPathCallingConvention()) {
      // Save only the live registers that the custom calling convention wants us to save.
      uint32_t caller_saves = core_registers
          ? locations->GetCustomSlowPathCallerSaves().GetCoreRegisters()
          : locations->GetCustomSlowPathCallerSaves().GetFloatingPointRegisters();
      return live_registers & caller_saves;
    } else {
      // Default ABI, we need to spill non-callee-save live registers.
      uint32_t callee_saves = core_registers ? core_callee_save_mask_ : fpu_callee_save_mask_;
      return live_registers & ~callee_saves;
    }
  }

  size_t GetNumberOfSlowPathSpills(LocationSummary* locations, bool core_registers) const {
    return POPCOUNT(GetSlowPathSpills(locations, core_registers));
  }

  size_t GetStackOffsetOfShouldDeoptimizeFlag() const {
    DCHECK(GetGraph()->HasShouldDeoptimizeFlag());
    DCHECK_GE(GetFrameSize(), FrameEntrySpillSize() + kShouldDeoptimizeFlagSize);
    return GetFrameSize() - FrameEntrySpillSize() - kShouldDeoptimizeFlagSize;
  }

  // Record native to dex mapping for a suspend point.  Required by runtime.
  void RecordPcInfo(HInstruction* instruction, uint32_t dex_pc, SlowPathCode* slow_path = nullptr);
  // Check whether we have already recorded mapping at this PC.
  bool HasStackMapAtCurrentPc();
  // Record extra stack maps if we support native debugging.
  void MaybeRecordNativeDebugInfo(HInstruction* instruction,
                                  uint32_t dex_pc,
                                  SlowPathCode* slow_path = nullptr);

  bool CanMoveNullCheckToUser(HNullCheck* null_check);
  void MaybeRecordImplicitNullCheck(HInstruction* instruction);
  LocationSummary* CreateThrowingSlowPathLocations(
      HInstruction* instruction, RegisterSet caller_saves = RegisterSet::Empty());
  void GenerateNullCheck(HNullCheck* null_check);
  virtual void GenerateImplicitNullCheck(HNullCheck* null_check) = 0;
  virtual void GenerateExplicitNullCheck(HNullCheck* null_check) = 0;

  // Records a stack map which the runtime might use to set catch phi values
  // during exception delivery.
  // TODO: Replace with a catch-entering instruction that records the environment.
  void RecordCatchBlockInfo();

  // Get the ScopedArenaAllocator used for codegen memory allocation.
  ScopedArenaAllocator* GetScopedAllocator();

  void AddSlowPath(SlowPathCode* slow_path);

  void BuildStackMaps(MemoryRegion stack_map_region,
                      MemoryRegion method_info_region,
                      const DexFile::CodeItem* code_item_for_osr_check);
  void ComputeStackMapAndMethodInfoSize(size_t* stack_map_size, size_t* method_info_size);
  size_t GetNumberOfJitRoots() const;

  // Fills the `literals` array with literals collected during code generation.
  // Also emits literal patches.
  void EmitJitRoots(uint8_t* code,
                    Handle<mirror::ObjectArray<mirror::Object>> roots,
                    const uint8_t* roots_data)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsLeafMethod() const {
    return is_leaf_;
  }

  void MarkNotLeaf() {
    is_leaf_ = false;
    requires_current_method_ = true;
  }

  void SetRequiresCurrentMethod() {
    requires_current_method_ = true;
  }

  bool RequiresCurrentMethod() const {
    return requires_current_method_;
  }

  // Clears the spill slots taken by loop phis in the `LocationSummary` of the
  // suspend check. This is called when the code generator generates code
  // for the suspend check at the back edge (instead of where the suspend check
  // is, which is the loop entry). At this point, the spill slots for the phis
  // have not been written to.
  void ClearSpillSlotsFromLoopPhisInStackMap(HSuspendCheck* suspend_check,
                                             HParallelMove* spills) const;

  bool* GetBlockedCoreRegisters() const { return blocked_core_registers_; }
  bool* GetBlockedFloatingPointRegisters() const { return blocked_fpu_registers_; }

  bool IsBlockedCoreRegister(size_t i) { return blocked_core_registers_[i]; }
  bool IsBlockedFloatingPointRegister(size_t i) { return blocked_fpu_registers_[i]; }

  // Helper that returns the offset of the array's length field.
  // Note: Besides the normal arrays, we also use the HArrayLength for
  // accessing the String's `count` field in String intrinsics.
  static uint32_t GetArrayLengthOffset(HArrayLength* array_length);

  // Helper that returns the offset of the array's data.
  // Note: Besides the normal arrays, we also use the HArrayGet for
  // accessing the String's `value` field in String intrinsics.
  static uint32_t GetArrayDataOffset(HArrayGet* array_get);

  void EmitParallelMoves(Location from1,
                         Location to1,
                         DataType::Type type1,
                         Location from2,
                         Location to2,
                         DataType::Type type2);

  static bool InstanceOfNeedsReadBarrier(HInstanceOf* instance_of) {
    // Used only for kExactCheck, kAbstractClassCheck, kClassHierarchyCheck and kArrayObjectCheck.
    DCHECK(instance_of->GetTypeCheckKind() == TypeCheckKind::kExactCheck ||
           instance_of->GetTypeCheckKind() == TypeCheckKind::kAbstractClassCheck ||
           instance_of->GetTypeCheckKind() == TypeCheckKind::kClassHierarchyCheck ||
           instance_of->GetTypeCheckKind() == TypeCheckKind::kArrayObjectCheck)
        << instance_of->GetTypeCheckKind();
    // If the target class is in the boot image, it's non-moveable and it doesn't matter
    // if we compare it with a from-space or to-space reference, the result is the same.
    // It's OK to traverse a class hierarchy jumping between from-space and to-space.
    return kEmitCompilerReadBarrier && !instance_of->GetTargetClass()->IsInBootImage();
  }

  static ReadBarrierOption ReadBarrierOptionForInstanceOf(HInstanceOf* instance_of) {
    return InstanceOfNeedsReadBarrier(instance_of) ? kWithReadBarrier : kWithoutReadBarrier;
  }

  static bool IsTypeCheckSlowPathFatal(HCheckCast* check_cast) {
    switch (check_cast->GetTypeCheckKind()) {
      case TypeCheckKind::kExactCheck:
      case TypeCheckKind::kAbstractClassCheck:
      case TypeCheckKind::kClassHierarchyCheck:
      case TypeCheckKind::kArrayObjectCheck:
      case TypeCheckKind::kInterfaceCheck: {
        bool needs_read_barrier =
            kEmitCompilerReadBarrier && !check_cast->GetTargetClass()->IsInBootImage();
        // We do not emit read barriers for HCheckCast, so we can get false negatives
        // and the slow path shall re-check and simply return if the cast is actually OK.
        return !needs_read_barrier;
      }
      case TypeCheckKind::kArrayCheck:
      case TypeCheckKind::kUnresolvedCheck:
        return false;
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

  static LocationSummary::CallKind GetCheckCastCallKind(HCheckCast* check_cast) {
    return (IsTypeCheckSlowPathFatal(check_cast) && !check_cast->CanThrowIntoCatchBlock())
        ? LocationSummary::kNoCall  // In fact, call on a fatal (non-returning) slow path.
        : LocationSummary::kCallOnSlowPath;
  }

  static bool StoreNeedsWriteBarrier(DataType::Type type, HInstruction* value) {
    // Check that null value is not represented as an integer constant.
    DCHECK(type != DataType::Type::kReference || !value->IsIntConstant());
    return type == DataType::Type::kReference && !value->IsNullConstant();
  }


  // Performs checks pertaining to an InvokeRuntime call.
  void ValidateInvokeRuntime(QuickEntrypointEnum entrypoint,
                             HInstruction* instruction,
                             SlowPathCode* slow_path);

  // Performs checks pertaining to an InvokeRuntimeWithoutRecordingPcInfo call.
  static void ValidateInvokeRuntimeWithoutRecordingPcInfo(HInstruction* instruction,
                                                          SlowPathCode* slow_path);

  void AddAllocatedRegister(Location location) {
    allocated_registers_.Add(location);
  }

  bool HasAllocatedRegister(bool is_core, int reg) const {
    return is_core
        ? allocated_registers_.ContainsCoreRegister(reg)
        : allocated_registers_.ContainsFloatingPointRegister(reg);
  }

  void AllocateLocations(HInstruction* instruction);

  // Tells whether the stack frame of the compiled method is
  // considered "empty", that is either actually having a size of zero,
  // or just containing the saved return address register.
  bool HasEmptyFrame() const {
    return GetFrameSize() == (CallPushesPC() ? GetWordSize() : 0);
  }

  static int8_t GetInt8ValueOf(HConstant* constant) {
    DCHECK(constant->IsIntConstant());
    return constant->AsIntConstant()->GetValue();
  }

  static int16_t GetInt16ValueOf(HConstant* constant) {
    DCHECK(constant->IsIntConstant());
    return constant->AsIntConstant()->GetValue();
  }

  static int32_t GetInt32ValueOf(HConstant* constant) {
    if (constant->IsIntConstant()) {
      return constant->AsIntConstant()->GetValue();
    } else if (constant->IsNullConstant()) {
      return 0;
    } else {
      DCHECK(constant->IsFloatConstant());
      return bit_cast<int32_t, float>(constant->AsFloatConstant()->GetValue());
    }
  }

  static int64_t GetInt64ValueOf(HConstant* constant) {
    if (constant->IsIntConstant()) {
      return constant->AsIntConstant()->GetValue();
    } else if (constant->IsNullConstant()) {
      return 0;
    } else if (constant->IsFloatConstant()) {
      return bit_cast<int32_t, float>(constant->AsFloatConstant()->GetValue());
    } else if (constant->IsLongConstant()) {
      return constant->AsLongConstant()->GetValue();
    } else {
      DCHECK(constant->IsDoubleConstant());
      return bit_cast<int64_t, double>(constant->AsDoubleConstant()->GetValue());
    }
  }

  size_t GetFirstRegisterSlotInSlowPath() const {
    return first_register_slot_in_slow_path_;
  }

  uint32_t FrameEntrySpillSize() const {
    return GetFpuSpillSize() + GetCoreSpillSize();
  }

  virtual ParallelMoveResolver* GetMoveResolver() = 0;

  static void CreateCommonInvokeLocationSummary(
      HInvoke* invoke, InvokeDexCallingConventionVisitor* visitor);

  void GenerateInvokeStaticOrDirectRuntimeCall(
      HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path);
  void GenerateInvokeUnresolvedRuntimeCall(HInvokeUnresolved* invoke);

  void GenerateInvokePolymorphicCall(HInvokePolymorphic* invoke);

  void CreateUnresolvedFieldLocationSummary(
      HInstruction* field_access,
      DataType::Type field_type,
      const FieldAccessCallingConvention& calling_convention);

  void GenerateUnresolvedFieldAccess(
      HInstruction* field_access,
      DataType::Type field_type,
      uint32_t field_index,
      uint32_t dex_pc,
      const FieldAccessCallingConvention& calling_convention);

  static void CreateLoadClassRuntimeCallLocationSummary(HLoadClass* cls,
                                                        Location runtime_type_index_location,
                                                        Location runtime_return_location);
  void GenerateLoadClassRuntimeCall(HLoadClass* cls);

  static void CreateSystemArrayCopyLocationSummary(HInvoke* invoke);

  void SetDisassemblyInformation(DisassemblyInformation* info) { disasm_info_ = info; }
  DisassemblyInformation* GetDisassemblyInformation() const { return disasm_info_; }

  virtual void InvokeRuntime(QuickEntrypointEnum entrypoint,
                             HInstruction* instruction,
                             uint32_t dex_pc,
                             SlowPathCode* slow_path = nullptr) = 0;

  // Check if the desired_string_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  virtual HLoadString::LoadKind GetSupportedLoadStringKind(
      HLoadString::LoadKind desired_string_load_kind) = 0;

  // Check if the desired_class_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  virtual HLoadClass::LoadKind GetSupportedLoadClassKind(
      HLoadClass::LoadKind desired_class_load_kind) = 0;

  static LocationSummary::CallKind GetLoadStringCallKind(HLoadString* load) {
    switch (load->GetLoadKind()) {
      case HLoadString::LoadKind::kBssEntry:
        DCHECK(load->NeedsEnvironment());
        return LocationSummary::kCallOnSlowPath;
      case HLoadString::LoadKind::kRuntimeCall:
        DCHECK(load->NeedsEnvironment());
        return LocationSummary::kCallOnMainOnly;
      case HLoadString::LoadKind::kJitTableAddress:
        DCHECK(!load->NeedsEnvironment());
        return kEmitCompilerReadBarrier
            ? LocationSummary::kCallOnSlowPath
            : LocationSummary::kNoCall;
        break;
      default:
        DCHECK(!load->NeedsEnvironment());
        return LocationSummary::kNoCall;
    }
  }

  // Check if the desired_dispatch_info is supported. If it is, return it,
  // otherwise return a fall-back info that should be used instead.
  virtual HInvokeStaticOrDirect::DispatchInfo GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke) = 0;

  // Generate a call to a static or direct method.
  virtual void GenerateStaticOrDirectCall(
      HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path = nullptr) = 0;
  // Generate a call to a virtual method.
  virtual void GenerateVirtualCall(
      HInvokeVirtual* invoke, Location temp, SlowPathCode* slow_path = nullptr) = 0;

  // Copy the result of a call into the given target.
  virtual void MoveFromReturnRegister(Location trg, DataType::Type type) = 0;

  virtual void GenerateNop() = 0;

  static QuickEntrypointEnum GetArrayAllocationEntrypoint(Handle<mirror::Class> array_klass);

 protected:
  // Patch info used for recording locations of required linker patches and their targets,
  // i.e. target method, string, type or code identified by their dex file and index,
  // or .data.bimg.rel.ro entries identified by the boot image offset.
  template <typename LabelType>
  struct PatchInfo {
    PatchInfo(const DexFile* dex_file, uint32_t off_or_idx)
        : target_dex_file(dex_file), offset_or_index(off_or_idx), label() { }

    // Target dex file or null for .data.bmig.rel.ro patches.
    const DexFile* target_dex_file;
    // Either the boot image offset (to write to .data.bmig.rel.ro) or string/type/method index.
    uint32_t offset_or_index;
    // Label for the instruction to patch.
    LabelType label;
  };

  CodeGenerator(HGraph* graph,
                size_t number_of_core_registers,
                size_t number_of_fpu_registers,
                size_t number_of_register_pairs,
                uint32_t core_callee_save_mask,
                uint32_t fpu_callee_save_mask,
                const CompilerOptions& compiler_options,
                OptimizingCompilerStats* stats);

  virtual HGraphVisitor* GetLocationBuilder() = 0;
  virtual HGraphVisitor* GetInstructionVisitor() = 0;

  // Returns the location of the first spilled entry for floating point registers,
  // relative to the stack pointer.
  uint32_t GetFpuSpillStart() const {
    return GetFrameSize() - FrameEntrySpillSize();
  }

  uint32_t GetFpuSpillSize() const {
    return POPCOUNT(fpu_spill_mask_) * GetFloatingPointSpillSlotSize();
  }

  uint32_t GetCoreSpillSize() const {
    return POPCOUNT(core_spill_mask_) * GetWordSize();
  }

  virtual bool HasAllocatedCalleeSaveRegisters() const {
    // We check the core registers against 1 because it always comprises the return PC.
    return (POPCOUNT(allocated_registers_.GetCoreRegisters() & core_callee_save_mask_) != 1)
      || (POPCOUNT(allocated_registers_.GetFloatingPointRegisters() & fpu_callee_save_mask_) != 0);
  }

  bool CallPushesPC() const {
    InstructionSet instruction_set = GetInstructionSet();
    return instruction_set == InstructionSet::kX86 || instruction_set == InstructionSet::kX86_64;
  }

  // Arm64 has its own type for a label, so we need to templatize these methods
  // to share the logic.

  template <typename LabelType>
  LabelType* CommonInitializeLabels() {
    // We use raw array allocations instead of ArenaVector<> because Labels are
    // non-constructible and non-movable and as such cannot be held in a vector.
    size_t size = GetGraph()->GetBlocks().size();
    LabelType* labels =
        GetGraph()->GetAllocator()->AllocArray<LabelType>(size, kArenaAllocCodeGenerator);
    for (size_t i = 0; i != size; ++i) {
      new(labels + i) LabelType();
    }
    return labels;
  }

  template <typename LabelType>
  LabelType* CommonGetLabelOf(LabelType* raw_pointer_to_labels_array, HBasicBlock* block) const {
    block = FirstNonEmptyBlock(block);
    return raw_pointer_to_labels_array + block->GetBlockId();
  }

  SlowPathCode* GetCurrentSlowPath() {
    return current_slow_path_;
  }

  StackMapStream* GetStackMapStream();

  void ReserveJitStringRoot(StringReference string_reference, Handle<mirror::String> string);
  uint64_t GetJitStringRootIndex(StringReference string_reference);
  void ReserveJitClassRoot(TypeReference type_reference, Handle<mirror::Class> klass);
  uint64_t GetJitClassRootIndex(TypeReference type_reference);

  // Emit the patches assocatied with JIT roots. Only applies to JIT compiled code.
  virtual void EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data);

  // Frame size required for this method.
  uint32_t frame_size_;
  uint32_t core_spill_mask_;
  uint32_t fpu_spill_mask_;
  uint32_t first_register_slot_in_slow_path_;

  // Registers that were allocated during linear scan.
  RegisterSet allocated_registers_;

  // Arrays used when doing register allocation to know which
  // registers we can allocate. `SetupBlockedRegisters` updates the
  // arrays.
  bool* const blocked_core_registers_;
  bool* const blocked_fpu_registers_;
  size_t number_of_core_registers_;
  size_t number_of_fpu_registers_;
  size_t number_of_register_pairs_;
  const uint32_t core_callee_save_mask_;
  const uint32_t fpu_callee_save_mask_;

  // The order to use for code generation.
  const ArenaVector<HBasicBlock*>* block_order_;

  DisassemblyInformation* disasm_info_;

 private:
  class CodeGenerationData;

  void InitializeCodeGenerationData();
  size_t GetStackOffsetOfSavedRegister(size_t index);
  void GenerateSlowPaths();
  void BlockIfInRegister(Location location, bool is_out = false) const;
  void EmitEnvironment(HEnvironment* environment, SlowPathCode* slow_path);

  OptimizingCompilerStats* stats_;

  HGraph* const graph_;
  const CompilerOptions& compiler_options_;

  // The current slow-path that we're generating code for.
  SlowPathCode* current_slow_path_;

  // The current block index in `block_order_` of the block
  // we are generating code for.
  size_t current_block_index_;

  // Whether the method is a leaf method.
  bool is_leaf_;

  // Whether an instruction in the graph accesses the current method.
  // TODO: Rename: this actually indicates that some instruction in the method
  // needs the environment including a valid stack frame.
  bool requires_current_method_;

  // The CodeGenerationData contains a ScopedArenaAllocator intended for reusing the
  // ArenaStack memory allocated in previous passes instead of adding to the memory
  // held by the ArenaAllocator. This ScopedArenaAllocator is created in
  // CodeGenerator::Compile() and remains alive until the CodeGenerator is destroyed.
  std::unique_ptr<CodeGenerationData> code_generation_data_;

  friend class OptimizingCFITest;

  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};

template <typename C, typename F>
class CallingConvention {
 public:
  CallingConvention(const C* registers,
                    size_t number_of_registers,
                    const F* fpu_registers,
                    size_t number_of_fpu_registers,
                    PointerSize pointer_size)
      : registers_(registers),
        number_of_registers_(number_of_registers),
        fpu_registers_(fpu_registers),
        number_of_fpu_registers_(number_of_fpu_registers),
        pointer_size_(pointer_size) {}

  size_t GetNumberOfRegisters() const { return number_of_registers_; }
  size_t GetNumberOfFpuRegisters() const { return number_of_fpu_registers_; }

  C GetRegisterAt(size_t index) const {
    DCHECK_LT(index, number_of_registers_);
    return registers_[index];
  }

  F GetFpuRegisterAt(size_t index) const {
    DCHECK_LT(index, number_of_fpu_registers_);
    return fpu_registers_[index];
  }

  size_t GetStackOffsetOf(size_t index) const {
    // We still reserve the space for parameters passed by registers.
    // Add space for the method pointer.
    return static_cast<size_t>(pointer_size_) + index * kVRegSize;
  }

 private:
  const C* registers_;
  const size_t number_of_registers_;
  const F* fpu_registers_;
  const size_t number_of_fpu_registers_;
  const PointerSize pointer_size_;

  DISALLOW_COPY_AND_ASSIGN(CallingConvention);
};

/**
 * A templated class SlowPathGenerator with a templated method NewSlowPath()
 * that can be used by any code generator to share equivalent slow-paths with
 * the objective of reducing generated code size.
 *
 * InstructionType:  instruction that requires SlowPathCodeType
 * SlowPathCodeType: subclass of SlowPathCode, with constructor SlowPathCodeType(InstructionType *)
 */
template <typename InstructionType>
class SlowPathGenerator {
  static_assert(std::is_base_of<HInstruction, InstructionType>::value,
                "InstructionType is not a subclass of art::HInstruction");

 public:
  SlowPathGenerator(HGraph* graph, CodeGenerator* codegen)
      : graph_(graph),
        codegen_(codegen),
        slow_path_map_(std::less<uint32_t>(),
                       graph->GetAllocator()->Adapter(kArenaAllocSlowPaths)) {}

  // Creates and adds a new slow-path, if needed, or returns existing one otherwise.
  // Templating the method (rather than the whole class) on the slow-path type enables
  // keeping this code at a generic, non architecture-specific place.
  //
  // NOTE: This approach assumes each InstructionType only generates one SlowPathCodeType.
  //       To relax this requirement, we would need some RTTI on the stored slow-paths,
  //       or template the class as a whole on SlowPathType.
  template <typename SlowPathCodeType>
  SlowPathCodeType* NewSlowPath(InstructionType* instruction) {
    static_assert(std::is_base_of<SlowPathCode, SlowPathCodeType>::value,
                  "SlowPathCodeType is not a subclass of art::SlowPathCode");
    static_assert(std::is_constructible<SlowPathCodeType, InstructionType*>::value,
                  "SlowPathCodeType is not constructible from InstructionType*");
    // Iterate over potential candidates for sharing. Currently, only same-typed
    // slow-paths with exactly the same dex-pc are viable candidates.
    // TODO: pass dex-pc/slow-path-type to run-time to allow even more sharing?
    const uint32_t dex_pc = instruction->GetDexPc();
    auto iter = slow_path_map_.find(dex_pc);
    if (iter != slow_path_map_.end()) {
      const ArenaVector<std::pair<InstructionType*, SlowPathCode*>>& candidates = iter->second;
      for (const auto& it : candidates) {
        InstructionType* other_instruction = it.first;
        SlowPathCodeType* other_slow_path = down_cast<SlowPathCodeType*>(it.second);
        // Determine if the instructions allow for slow-path sharing.
        if (HaveSameLiveRegisters(instruction, other_instruction) &&
            HaveSameStackMap(instruction, other_instruction)) {
          // Can share: reuse existing one.
          return other_slow_path;
        }
      }
    } else {
      // First time this dex-pc is seen.
      iter = slow_path_map_.Put(dex_pc,
                                {{}, {graph_->GetAllocator()->Adapter(kArenaAllocSlowPaths)}});
    }
    // Cannot share: create and add new slow-path for this particular dex-pc.
    SlowPathCodeType* slow_path =
        new (codegen_->GetScopedAllocator()) SlowPathCodeType(instruction);
    iter->second.emplace_back(std::make_pair(instruction, slow_path));
    codegen_->AddSlowPath(slow_path);
    return slow_path;
  }

 private:
  // Tests if both instructions have same set of live physical registers. This ensures
  // the slow-path has exactly the same preamble on saving these registers to stack.
  bool HaveSameLiveRegisters(const InstructionType* i1, const InstructionType* i2) const {
    const uint32_t core_spill = ~codegen_->GetCoreSpillMask();
    const uint32_t fpu_spill = ~codegen_->GetFpuSpillMask();
    RegisterSet* live1 = i1->GetLocations()->GetLiveRegisters();
    RegisterSet* live2 = i2->GetLocations()->GetLiveRegisters();
    return (((live1->GetCoreRegisters() & core_spill) ==
             (live2->GetCoreRegisters() & core_spill)) &&
            ((live1->GetFloatingPointRegisters() & fpu_spill) ==
             (live2->GetFloatingPointRegisters() & fpu_spill)));
  }

  // Tests if both instructions have the same stack map. This ensures the interpreter
  // will find exactly the same dex-registers at the same entries.
  bool HaveSameStackMap(const InstructionType* i1, const InstructionType* i2) const {
    DCHECK(i1->HasEnvironment());
    DCHECK(i2->HasEnvironment());
    // We conservatively test if the two instructions find exactly the same instructions
    // and location in each dex-register. This guarantees they will have the same stack map.
    HEnvironment* e1 = i1->GetEnvironment();
    HEnvironment* e2 = i2->GetEnvironment();
    if (e1->GetParent() != e2->GetParent() || e1->Size() != e2->Size()) {
      return false;
    }
    for (size_t i = 0, sz = e1->Size(); i < sz; ++i) {
      if (e1->GetInstructionAt(i) != e2->GetInstructionAt(i) ||
          !e1->GetLocationAt(i).Equals(e2->GetLocationAt(i))) {
        return false;
      }
    }
    return true;
  }

  HGraph* const graph_;
  CodeGenerator* const codegen_;

  // Map from dex-pc to vector of already existing instruction/slow-path pairs.
  ArenaSafeMap<uint32_t, ArenaVector<std::pair<InstructionType*, SlowPathCode*>>> slow_path_map_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathGenerator);
};

class InstructionCodeGenerator : public HGraphVisitor {
 public:
  InstructionCodeGenerator(HGraph* graph, CodeGenerator* codegen)
      : HGraphVisitor(graph),
        deopt_slow_paths_(graph, codegen) {}

 protected:
  // Add slow-path generator for each instruction/slow-path combination that desires sharing.
  // TODO: under current regime, only deopt sharing make sense; extend later.
  SlowPathGenerator<HDeoptimize> deopt_slow_paths_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_H_
