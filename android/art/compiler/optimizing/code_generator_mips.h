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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_

#include "code_generator.h"
#include "dex/dex_file_types.h"
#include "dex/string_reference.h"
#include "dex/type_reference.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "parallel_move_resolver.h"
#include "utils/mips/assembler_mips.h"

namespace art {
namespace mips {

// InvokeDexCallingConvention registers

static constexpr Register kParameterCoreRegisters[] =
    { A1, A2, A3, T0, T1 };
static constexpr size_t kParameterCoreRegistersLength = arraysize(kParameterCoreRegisters);

static constexpr FRegister kParameterFpuRegisters[] =
    { F8, F10, F12, F14, F16, F18 };
static constexpr size_t kParameterFpuRegistersLength = arraysize(kParameterFpuRegisters);


// InvokeRuntimeCallingConvention registers

static constexpr Register kRuntimeParameterCoreRegisters[] =
    { A0, A1, A2, A3 };
static constexpr size_t kRuntimeParameterCoreRegistersLength =
    arraysize(kRuntimeParameterCoreRegisters);

static constexpr FRegister kRuntimeParameterFpuRegisters[] =
    { F12, F14 };
static constexpr size_t kRuntimeParameterFpuRegistersLength =
    arraysize(kRuntimeParameterFpuRegisters);


static constexpr Register kCoreCalleeSaves[] =
    { S0, S1, S2, S3, S4, S5, S6, S7, FP, RA };
static constexpr FRegister kFpuCalleeSaves[] =
    { F20, F22, F24, F26, F28, F30 };


class CodeGeneratorMIPS;

VectorRegister VectorRegisterFrom(Location location);

class InvokeDexCallingConvention : public CallingConvention<Register, FRegister> {
 public:
  InvokeDexCallingConvention()
      : CallingConvention(kParameterCoreRegisters,
                          kParameterCoreRegistersLength,
                          kParameterFpuRegisters,
                          kParameterFpuRegistersLength,
                          kMipsPointerSize) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConvention);
};

class InvokeDexCallingConventionVisitorMIPS : public InvokeDexCallingConventionVisitor {
 public:
  InvokeDexCallingConventionVisitorMIPS() {}
  virtual ~InvokeDexCallingConventionVisitorMIPS() {}

  Location GetNextLocation(DataType::Type type) OVERRIDE;
  Location GetReturnLocation(DataType::Type type) const OVERRIDE;
  Location GetMethodLocation() const OVERRIDE;

 private:
  InvokeDexCallingConvention calling_convention;

  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionVisitorMIPS);
};

class InvokeRuntimeCallingConvention : public CallingConvention<Register, FRegister> {
 public:
  InvokeRuntimeCallingConvention()
      : CallingConvention(kRuntimeParameterCoreRegisters,
                          kRuntimeParameterCoreRegistersLength,
                          kRuntimeParameterFpuRegisters,
                          kRuntimeParameterFpuRegistersLength,
                          kMipsPointerSize) {}

  Location GetReturnLocation(DataType::Type return_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeRuntimeCallingConvention);
};

class FieldAccessCallingConventionMIPS : public FieldAccessCallingConvention {
 public:
  FieldAccessCallingConventionMIPS() {}

  Location GetObjectLocation() const OVERRIDE {
    return Location::RegisterLocation(A1);
  }
  Location GetFieldIndexLocation() const OVERRIDE {
    return Location::RegisterLocation(A0);
  }
  Location GetReturnLocation(DataType::Type type) const OVERRIDE {
    return DataType::Is64BitType(type)
        ? Location::RegisterPairLocation(V0, V1)
        : Location::RegisterLocation(V0);
  }
  Location GetSetValueLocation(DataType::Type type, bool is_instance) const OVERRIDE {
    return DataType::Is64BitType(type)
        ? Location::RegisterPairLocation(A2, A3)
        : (is_instance ? Location::RegisterLocation(A2) : Location::RegisterLocation(A1));
  }
  Location GetFpuLocation(DataType::Type type ATTRIBUTE_UNUSED) const OVERRIDE {
    return Location::FpuRegisterLocation(F0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAccessCallingConventionMIPS);
};

class ParallelMoveResolverMIPS : public ParallelMoveResolverWithSwap {
 public:
  ParallelMoveResolverMIPS(ArenaAllocator* allocator, CodeGeneratorMIPS* codegen)
      : ParallelMoveResolverWithSwap(allocator), codegen_(codegen) {}

  void EmitMove(size_t index) OVERRIDE;
  void EmitSwap(size_t index) OVERRIDE;
  void SpillScratch(int reg) OVERRIDE;
  void RestoreScratch(int reg) OVERRIDE;

  void Exchange(int index1, int index2, bool double_slot);
  void ExchangeQuadSlots(int index1, int index2);

  MipsAssembler* GetAssembler() const;

 private:
  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverMIPS);
};

class SlowPathCodeMIPS : public SlowPathCode {
 public:
  explicit SlowPathCodeMIPS(HInstruction* instruction)
      : SlowPathCode(instruction), entry_label_(), exit_label_() {}

  MipsLabel* GetEntryLabel() { return &entry_label_; }
  MipsLabel* GetExitLabel() { return &exit_label_; }

 private:
  MipsLabel entry_label_;
  MipsLabel exit_label_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathCodeMIPS);
};

class LocationsBuilderMIPS : public HGraphVisitor {
 public:
  LocationsBuilderMIPS(HGraph* graph, CodeGeneratorMIPS* codegen)
      : HGraphVisitor(graph), codegen_(codegen) {}

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

 private:
  void HandleInvoke(HInvoke* invoke);
  void HandleBinaryOp(HBinaryOperation* operation);
  void HandleCondition(HCondition* instruction);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction, const FieldInfo& field_info);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);
  Location RegisterOrZeroConstant(HInstruction* instruction);
  Location FpuRegisterOrConstantForStore(HInstruction* instruction);

  InvokeDexCallingConventionVisitorMIPS parameter_visitor_;

  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(LocationsBuilderMIPS);
};

class InstructionCodeGeneratorMIPS : public InstructionCodeGenerator {
 public:
  InstructionCodeGeneratorMIPS(HGraph* graph, CodeGeneratorMIPS* codegen);

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

  MipsAssembler* GetAssembler() const { return assembler_; }

  // Compare-and-jump packed switch generates approx. 3 + 2.5 * N 32-bit
  // instructions for N cases.
  // Table-based packed switch generates approx. 11 32-bit instructions
  // and N 32-bit data words for N cases.
  // At N = 6 they come out as 18 and 17 32-bit words respectively.
  // We switch to the table-based method starting with 7 cases.
  static constexpr uint32_t kPackedSwitchJumpTableThreshold = 6;

  void GenerateMemoryBarrier(MemBarrierKind kind);

 private:
  void GenerateClassInitializationCheck(SlowPathCodeMIPS* slow_path, Register class_reg);
  void GenerateSuspendCheck(HSuspendCheck* check, HBasicBlock* successor);
  void HandleBinaryOp(HBinaryOperation* operation);
  void HandleCondition(HCondition* instruction);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction,
                      const FieldInfo& field_info,
                      uint32_t dex_pc,
                      bool value_can_be_null);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info, uint32_t dex_pc);

  // Generate a heap reference load using one register `out`:
  //
  //   out <- *(out + offset)
  //
  // while honoring heap poisoning and/or read barriers (if any).
  //
  // Location `maybe_temp` is used when generating a read barrier and
  // shall be a register in that case; it may be an invalid location
  // otherwise.
  void GenerateReferenceLoadOneRegister(HInstruction* instruction,
                                        Location out,
                                        uint32_t offset,
                                        Location maybe_temp,
                                        ReadBarrierOption read_barrier_option);
  // Generate a heap reference load using two different registers
  // `out` and `obj`:
  //
  //   out <- *(obj + offset)
  //
  // while honoring heap poisoning and/or read barriers (if any).
  //
  // Location `maybe_temp` is used when generating a Baker's (fast
  // path) read barrier and shall be a register in that case; it may
  // be an invalid location otherwise.
  void GenerateReferenceLoadTwoRegisters(HInstruction* instruction,
                                         Location out,
                                         Location obj,
                                         uint32_t offset,
                                         Location maybe_temp,
                                         ReadBarrierOption read_barrier_option);

  // Generate a GC root reference load:
  //
  //   root <- *(obj + offset)
  //
  // while honoring read barriers (if any).
  void GenerateGcRootFieldLoad(HInstruction* instruction,
                               Location root,
                               Register obj,
                               uint32_t offset,
                               ReadBarrierOption read_barrier_option,
                               MipsLabel* label_low = nullptr);

  void GenerateIntCompare(IfCondition cond, LocationSummary* locations);
  // When the function returns `false` it means that the condition holds if `dst` is non-zero
  // and doesn't hold if `dst` is zero. If it returns `true`, the roles of zero and non-zero
  // `dst` are exchanged.
  bool MaterializeIntCompare(IfCondition cond,
                             LocationSummary* input_locations,
                             Register dst);
  void GenerateIntCompareAndBranch(IfCondition cond,
                                   LocationSummary* locations,
                                   MipsLabel* label);
  void GenerateLongCompare(IfCondition cond, LocationSummary* locations);
  void GenerateLongCompareAndBranch(IfCondition cond,
                                    LocationSummary* locations,
                                    MipsLabel* label);
  void GenerateFpCompare(IfCondition cond,
                         bool gt_bias,
                         DataType::Type type,
                         LocationSummary* locations);
  // When the function returns `false` it means that the condition holds if the condition
  // code flag `cc` is non-zero and doesn't hold if `cc` is zero. If it returns `true`,
  // the roles of zero and non-zero values of the `cc` flag are exchanged.
  bool MaterializeFpCompareR2(IfCondition cond,
                              bool gt_bias,
                              DataType::Type type,
                              LocationSummary* input_locations,
                              int cc);
  // When the function returns `false` it means that the condition holds if `dst` is non-zero
  // and doesn't hold if `dst` is zero. If it returns `true`, the roles of zero and non-zero
  // `dst` are exchanged.
  bool MaterializeFpCompareR6(IfCondition cond,
                              bool gt_bias,
                              DataType::Type type,
                              LocationSummary* input_locations,
                              FRegister dst);
  void GenerateFpCompareAndBranch(IfCondition cond,
                                  bool gt_bias,
                                  DataType::Type type,
                                  LocationSummary* locations,
                                  MipsLabel* label);
  void GenerateTestAndBranch(HInstruction* instruction,
                             size_t condition_input_index,
                             MipsLabel* true_target,
                             MipsLabel* false_target);
  void DivRemOneOrMinusOne(HBinaryOperation* instruction);
  void DivRemByPowerOfTwo(HBinaryOperation* instruction);
  void GenerateDivRemWithAnyConstant(HBinaryOperation* instruction);
  void GenerateDivRemIntegral(HBinaryOperation* instruction);
  void HandleGoto(HInstruction* got, HBasicBlock* successor);
  void GenPackedSwitchWithCompares(Register value_reg,
                                   int32_t lower_bound,
                                   uint32_t num_entries,
                                   HBasicBlock* switch_block,
                                   HBasicBlock* default_block);
  void GenTableBasedPackedSwitch(Register value_reg,
                                 Register constant_area,
                                 int32_t lower_bound,
                                 uint32_t num_entries,
                                 HBasicBlock* switch_block,
                                 HBasicBlock* default_block);

  int32_t VecAddress(LocationSummary* locations,
                     size_t size,
                     /* out */ Register* adjusted_base);
  void GenConditionalMoveR2(HSelect* select);
  void GenConditionalMoveR6(HSelect* select);

  MipsAssembler* const assembler_;
  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(InstructionCodeGeneratorMIPS);
};

class CodeGeneratorMIPS : public CodeGenerator {
 public:
  CodeGeneratorMIPS(HGraph* graph,
                    const MipsInstructionSetFeatures& isa_features,
                    const CompilerOptions& compiler_options,
                    OptimizingCompilerStats* stats = nullptr);
  virtual ~CodeGeneratorMIPS() {}

  void ComputeSpillMask() OVERRIDE;
  bool HasAllocatedCalleeSaveRegisters() const OVERRIDE;
  void GenerateFrameEntry() OVERRIDE;
  void GenerateFrameExit() OVERRIDE;

  void Bind(HBasicBlock* block) OVERRIDE;

  void MoveConstant(Location location, HConstant* c);

  size_t GetWordSize() const OVERRIDE { return kMipsWordSize; }

  size_t GetFloatingPointSpillSlotSize() const OVERRIDE {
    return GetGraph()->HasSIMD()
        ? 2 * kMipsDoublewordSize   // 16 bytes for each spill.
        : 1 * kMipsDoublewordSize;  //  8 bytes for each spill.
  }

  uintptr_t GetAddressOf(HBasicBlock* block) OVERRIDE {
    return assembler_.GetLabelLocation(GetLabelOf(block));
  }

  HGraphVisitor* GetLocationBuilder() OVERRIDE { return &location_builder_; }
  HGraphVisitor* GetInstructionVisitor() OVERRIDE { return &instruction_visitor_; }
  MipsAssembler* GetAssembler() OVERRIDE { return &assembler_; }
  const MipsAssembler& GetAssembler() const OVERRIDE { return assembler_; }

  // Emit linker patches.
  void EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) OVERRIDE;
  void EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) OVERRIDE;

  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference field load when Baker's read barriers are used.
  void GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             Register obj,
                                             uint32_t offset,
                                             Location temp,
                                             bool needs_null_check);
  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference array load when Baker's read barriers are used.
  void GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             Register obj,
                                             uint32_t data_offset,
                                             Location index,
                                             Location temp,
                                             bool needs_null_check);

  // Factored implementation, used by GenerateFieldLoadWithBakerReadBarrier,
  // GenerateArrayLoadWithBakerReadBarrier and some intrinsics.
  //
  // Load the object reference located at the address
  // `obj + offset + (index << scale_factor)`, held by object `obj`, into
  // `ref`, and mark it if needed.
  //
  // If `always_update_field` is true, the value of the reference is
  // atomically updated in the holder (`obj`).
  void GenerateReferenceLoadWithBakerReadBarrier(HInstruction* instruction,
                                                 Location ref,
                                                 Register obj,
                                                 uint32_t offset,
                                                 Location index,
                                                 ScaleFactor scale_factor,
                                                 Location temp,
                                                 bool needs_null_check,
                                                 bool always_update_field = false);

  // Generate a read barrier for a heap reference within `instruction`
  // using a slow path.
  //
  // A read barrier for an object reference read from the heap is
  // implemented as a call to the artReadBarrierSlow runtime entry
  // point, which is passed the values in locations `ref`, `obj`, and
  // `offset`:
  //
  //   mirror::Object* artReadBarrierSlow(mirror::Object* ref,
  //                                      mirror::Object* obj,
  //                                      uint32_t offset);
  //
  // The `out` location contains the value returned by
  // artReadBarrierSlow.
  //
  // When `index` is provided (i.e. for array accesses), the offset
  // value passed to artReadBarrierSlow is adjusted to take `index`
  // into account.
  void GenerateReadBarrierSlow(HInstruction* instruction,
                               Location out,
                               Location ref,
                               Location obj,
                               uint32_t offset,
                               Location index = Location::NoLocation());

  // If read barriers are enabled, generate a read barrier for a heap
  // reference using a slow path. If heap poisoning is enabled, also
  // unpoison the reference in `out`.
  void MaybeGenerateReadBarrierSlow(HInstruction* instruction,
                                    Location out,
                                    Location ref,
                                    Location obj,
                                    uint32_t offset,
                                    Location index = Location::NoLocation());

  // Generate a read barrier for a GC root within `instruction` using
  // a slow path.
  //
  // A read barrier for an object reference GC root is implemented as
  // a call to the artReadBarrierForRootSlow runtime entry point,
  // which is passed the value in location `root`:
  //
  //   mirror::Object* artReadBarrierForRootSlow(GcRoot<mirror::Object>* root);
  //
  // The `out` location contains the value returned by
  // artReadBarrierForRootSlow.
  void GenerateReadBarrierForRootSlow(HInstruction* instruction, Location out, Location root);

  void MarkGCCard(Register object, Register value, bool value_can_be_null);

  // Register allocation.

  void SetupBlockedRegisters() const OVERRIDE;

  size_t SaveCoreRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t RestoreCoreRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  void ClobberRA() {
    clobbered_ra_ = true;
  }

  void DumpCoreRegister(std::ostream& stream, int reg) const OVERRIDE;
  void DumpFloatingPointRegister(std::ostream& stream, int reg) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE { return InstructionSet::kMips; }

  const MipsInstructionSetFeatures& GetInstructionSetFeatures() const {
    return isa_features_;
  }

  MipsLabel* GetLabelOf(HBasicBlock* block) const {
    return CommonGetLabelOf<MipsLabel>(block_labels_, block);
  }

  void Initialize() OVERRIDE {
    block_labels_ = CommonInitializeLabels<MipsLabel>();
  }

  void Finalize(CodeAllocator* allocator) OVERRIDE;

  // Code generation helpers.

  void MoveLocation(Location dst, Location src, DataType::Type dst_type) OVERRIDE;

  void MoveConstant(Location destination, int32_t value) OVERRIDE;

  void AddLocationAsTemp(Location location, LocationSummary* locations) OVERRIDE;

  // Generate code to invoke a runtime entry point.
  void InvokeRuntime(QuickEntrypointEnum entrypoint,
                     HInstruction* instruction,
                     uint32_t dex_pc,
                     SlowPathCode* slow_path = nullptr) OVERRIDE;

  // Generate code to invoke a runtime entry point, but do not record
  // PC-related information in a stack map.
  void InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                           HInstruction* instruction,
                                           SlowPathCode* slow_path,
                                           bool direct);

  void GenerateInvokeRuntime(int32_t entry_point_offset, bool direct);

  ParallelMoveResolver* GetMoveResolver() OVERRIDE { return &move_resolver_; }

  bool NeedsTwoRegisters(DataType::Type type) const OVERRIDE {
    return type == DataType::Type::kInt64;
  }

  // Check if the desired_string_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  HLoadString::LoadKind GetSupportedLoadStringKind(
      HLoadString::LoadKind desired_string_load_kind) OVERRIDE;

  // Check if the desired_class_load_kind is supported. If it is, return it,
  // otherwise return a fall-back kind that should be used instead.
  HLoadClass::LoadKind GetSupportedLoadClassKind(
      HLoadClass::LoadKind desired_class_load_kind) OVERRIDE;

  // Check if the desired_dispatch_info is supported. If it is, return it,
  // otherwise return a fall-back info that should be used instead.
  HInvokeStaticOrDirect::DispatchInfo GetSupportedInvokeStaticOrDirectDispatch(
      const HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
      HInvokeStaticOrDirect* invoke) OVERRIDE;

  void GenerateStaticOrDirectCall(
      HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path = nullptr) OVERRIDE;
  void GenerateVirtualCall(
      HInvokeVirtual* invoke, Location temp, SlowPathCode* slow_path = nullptr) OVERRIDE;

  void MoveFromReturnRegister(Location trg ATTRIBUTE_UNUSED,
                              DataType::Type type ATTRIBUTE_UNUSED) OVERRIDE {
    UNIMPLEMENTED(FATAL) << "Not implemented on MIPS";
  }

  void GenerateNop() OVERRIDE;
  void GenerateImplicitNullCheck(HNullCheck* instruction) OVERRIDE;
  void GenerateExplicitNullCheck(HNullCheck* instruction) OVERRIDE;

  // The PcRelativePatchInfo is used for PC-relative addressing of methods/strings/types,
  // whether through .data.bimg.rel.ro, .bss, or directly in the boot image.
  //
  // The 16-bit halves of the 32-bit PC-relative offset are patched separately, necessitating
  // two patches/infos. There can be more than two patches/infos if the instruction supplying
  // the high half is shared with e.g. a slow path, while the low half is supplied by separate
  // instructions, e.g.:
  //     lui   r1, high       // patch
  //     addu  r1, r1, rbase
  //     lw    r2, low(r1)    // patch
  //     beqz  r2, slow_path
  //   back:
  //     ...
  //   slow_path:
  //     ...
  //     sw    r2, low(r1)    // patch
  //     b     back
  struct PcRelativePatchInfo : PatchInfo<MipsLabel> {
    PcRelativePatchInfo(const DexFile* dex_file,
                        uint32_t off_or_idx,
                        const PcRelativePatchInfo* info_high)
        : PatchInfo<MipsLabel>(dex_file, off_or_idx),
          pc_rel_label(),
          patch_info_high(info_high) { }

    // Label for the instruction corresponding to PC+0. Not bound or used in low half patches.
    // Not bound in high half patches on R2 when using HMipsComputeBaseMethodAddress.
    // Bound in high half patches on R2 when using the NAL instruction instead of
    // HMipsComputeBaseMethodAddress.
    // Bound in high half patches on R6.
    MipsLabel pc_rel_label;
    // Pointer to the info for the high half patch or nullptr if this is the high half patch info.
    const PcRelativePatchInfo* patch_info_high;

   private:
    PcRelativePatchInfo(PcRelativePatchInfo&& other) = delete;
    DISALLOW_COPY_AND_ASSIGN(PcRelativePatchInfo);
  };

  PcRelativePatchInfo* NewBootImageMethodPatch(MethodReference target_method,
                                               const PcRelativePatchInfo* info_high = nullptr);
  PcRelativePatchInfo* NewMethodBssEntryPatch(MethodReference target_method,
                                              const PcRelativePatchInfo* info_high = nullptr);
  PcRelativePatchInfo* NewBootImageTypePatch(const DexFile& dex_file,
                                             dex::TypeIndex type_index,
                                             const PcRelativePatchInfo* info_high = nullptr);
  PcRelativePatchInfo* NewTypeBssEntryPatch(const DexFile& dex_file,
                                            dex::TypeIndex type_index,
                                            const PcRelativePatchInfo* info_high = nullptr);
  PcRelativePatchInfo* NewBootImageStringPatch(const DexFile& dex_file,
                                               dex::StringIndex string_index,
                                               const PcRelativePatchInfo* info_high = nullptr);
  PcRelativePatchInfo* NewStringBssEntryPatch(const DexFile& dex_file,
                                              dex::StringIndex string_index,
                                              const PcRelativePatchInfo* info_high = nullptr);
  Literal* DeduplicateBootImageAddressLiteral(uint32_t address);

  void EmitPcRelativeAddressPlaceholderHigh(PcRelativePatchInfo* info_high,
                                            Register out,
                                            Register base);

  // The JitPatchInfo is used for JIT string and class loads.
  struct JitPatchInfo {
    JitPatchInfo(const DexFile& dex_file, uint64_t idx)
        : target_dex_file(dex_file), index(idx) { }
    JitPatchInfo(JitPatchInfo&& other) = default;

    const DexFile& target_dex_file;
    // String/type index.
    uint64_t index;
    // Label for the instruction loading the most significant half of the address.
    MipsLabel high_label;
    // Label for the instruction supplying the least significant half of the address.
    MipsLabel low_label;
  };

  void PatchJitRootUse(uint8_t* code,
                       const uint8_t* roots_data,
                       const JitPatchInfo& info,
                       uint64_t index_in_table) const;
  JitPatchInfo* NewJitRootStringPatch(const DexFile& dex_file,
                                      dex::StringIndex string_index,
                                      Handle<mirror::String> handle);
  JitPatchInfo* NewJitRootClassPatch(const DexFile& dex_file,
                                     dex::TypeIndex type_index,
                                     Handle<mirror::Class> handle);

 private:
  Register GetInvokeStaticOrDirectExtraParameter(HInvokeStaticOrDirect* invoke, Register temp);

  using Uint32ToLiteralMap = ArenaSafeMap<uint32_t, Literal*>;

  Literal* DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map);
  PcRelativePatchInfo* NewPcRelativePatch(const DexFile* dex_file,
                                          uint32_t offset_or_index,
                                          const PcRelativePatchInfo* info_high,
                                          ArenaDeque<PcRelativePatchInfo>* patches);

  template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
  void EmitPcRelativeLinkerPatches(const ArenaDeque<PcRelativePatchInfo>& infos,
                                   ArenaVector<linker::LinkerPatch>* linker_patches);

  // Labels for each block that will be compiled.
  MipsLabel* block_labels_;
  MipsLabel frame_entry_label_;
  LocationsBuilderMIPS location_builder_;
  InstructionCodeGeneratorMIPS instruction_visitor_;
  ParallelMoveResolverMIPS move_resolver_;
  MipsAssembler assembler_;
  const MipsInstructionSetFeatures& isa_features_;

  // Deduplication map for 32-bit literals, used for non-patchable boot image addresses.
  Uint32ToLiteralMap uint32_literals_;
  // PC-relative method patch info for kBootImageLinkTimePcRelative.
  ArenaDeque<PcRelativePatchInfo> boot_image_method_patches_;
  // PC-relative method patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> method_bss_entry_patches_;
  // PC-relative type patch info for kBootImageLinkTimePcRelative.
  ArenaDeque<PcRelativePatchInfo> boot_image_type_patches_;
  // PC-relative type patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> type_bss_entry_patches_;
  // PC-relative String patch info; type depends on configuration (intern table or boot image PIC).
  ArenaDeque<PcRelativePatchInfo> boot_image_string_patches_;
  // PC-relative String patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> string_bss_entry_patches_;

  // Patches for string root accesses in JIT compiled code.
  ArenaDeque<JitPatchInfo> jit_string_patches_;
  // Patches for class root accesses in JIT compiled code.
  ArenaDeque<JitPatchInfo> jit_class_patches_;

  // PC-relative loads on R2 clobber RA, which may need to be preserved explicitly in leaf methods.
  // This is a flag set by pc_relative_fixups_mips and dex_cache_array_fixups_mips optimizations.
  bool clobbered_ra_;

  DISALLOW_COPY_AND_ASSIGN(CodeGeneratorMIPS);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS_H_
