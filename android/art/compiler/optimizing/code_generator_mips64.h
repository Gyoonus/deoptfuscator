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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS64_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS64_H_

#include "code_generator.h"
#include "dex/type_reference.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "parallel_move_resolver.h"
#include "utils/mips64/assembler_mips64.h"

namespace art {
namespace mips64 {

// InvokeDexCallingConvention registers

static constexpr GpuRegister kParameterCoreRegisters[] =
    { A1, A2, A3, A4, A5, A6, A7 };
static constexpr size_t kParameterCoreRegistersLength = arraysize(kParameterCoreRegisters);

static constexpr FpuRegister kParameterFpuRegisters[] =
    { F13, F14, F15, F16, F17, F18, F19 };
static constexpr size_t kParameterFpuRegistersLength = arraysize(kParameterFpuRegisters);


// InvokeRuntimeCallingConvention registers

static constexpr GpuRegister kRuntimeParameterCoreRegisters[] =
    { A0, A1, A2, A3, A4, A5, A6, A7 };
static constexpr size_t kRuntimeParameterCoreRegistersLength =
    arraysize(kRuntimeParameterCoreRegisters);

static constexpr FpuRegister kRuntimeParameterFpuRegisters[] =
    { F12, F13, F14, F15, F16, F17, F18, F19 };
static constexpr size_t kRuntimeParameterFpuRegistersLength =
    arraysize(kRuntimeParameterFpuRegisters);


static constexpr GpuRegister kCoreCalleeSaves[] =
    { S0, S1, S2, S3, S4, S5, S6, S7, GP, S8, RA };
static constexpr FpuRegister kFpuCalleeSaves[] =
    { F24, F25, F26, F27, F28, F29, F30, F31 };


class CodeGeneratorMIPS64;

VectorRegister VectorRegisterFrom(Location location);

class InvokeDexCallingConvention : public CallingConvention<GpuRegister, FpuRegister> {
 public:
  InvokeDexCallingConvention()
      : CallingConvention(kParameterCoreRegisters,
                          kParameterCoreRegistersLength,
                          kParameterFpuRegisters,
                          kParameterFpuRegistersLength,
                          kMips64PointerSize) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConvention);
};

class InvokeDexCallingConventionVisitorMIPS64 : public InvokeDexCallingConventionVisitor {
 public:
  InvokeDexCallingConventionVisitorMIPS64() {}
  virtual ~InvokeDexCallingConventionVisitorMIPS64() {}

  Location GetNextLocation(DataType::Type type) OVERRIDE;
  Location GetReturnLocation(DataType::Type type) const OVERRIDE;
  Location GetMethodLocation() const OVERRIDE;

 private:
  InvokeDexCallingConvention calling_convention;

  DISALLOW_COPY_AND_ASSIGN(InvokeDexCallingConventionVisitorMIPS64);
};

class InvokeRuntimeCallingConvention : public CallingConvention<GpuRegister, FpuRegister> {
 public:
  InvokeRuntimeCallingConvention()
      : CallingConvention(kRuntimeParameterCoreRegisters,
                          kRuntimeParameterCoreRegistersLength,
                          kRuntimeParameterFpuRegisters,
                          kRuntimeParameterFpuRegistersLength,
                          kMips64PointerSize) {}

  Location GetReturnLocation(DataType::Type return_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(InvokeRuntimeCallingConvention);
};

class FieldAccessCallingConventionMIPS64 : public FieldAccessCallingConvention {
 public:
  FieldAccessCallingConventionMIPS64() {}

  Location GetObjectLocation() const OVERRIDE {
    return Location::RegisterLocation(A1);
  }
  Location GetFieldIndexLocation() const OVERRIDE {
    return Location::RegisterLocation(A0);
  }
  Location GetReturnLocation(DataType::Type type ATTRIBUTE_UNUSED) const OVERRIDE {
    return Location::RegisterLocation(V0);
  }
  Location GetSetValueLocation(DataType::Type type ATTRIBUTE_UNUSED,
                               bool is_instance) const OVERRIDE {
    return is_instance
        ? Location::RegisterLocation(A2)
        : Location::RegisterLocation(A1);
  }
  Location GetFpuLocation(DataType::Type type ATTRIBUTE_UNUSED) const OVERRIDE {
    return Location::FpuRegisterLocation(F0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAccessCallingConventionMIPS64);
};

class ParallelMoveResolverMIPS64 : public ParallelMoveResolverWithSwap {
 public:
  ParallelMoveResolverMIPS64(ArenaAllocator* allocator, CodeGeneratorMIPS64* codegen)
      : ParallelMoveResolverWithSwap(allocator), codegen_(codegen) {}

  void EmitMove(size_t index) OVERRIDE;
  void EmitSwap(size_t index) OVERRIDE;
  void SpillScratch(int reg) OVERRIDE;
  void RestoreScratch(int reg) OVERRIDE;

  void Exchange(int index1, int index2, bool double_slot);
  void ExchangeQuadSlots(int index1, int index2);

  Mips64Assembler* GetAssembler() const;

 private:
  CodeGeneratorMIPS64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(ParallelMoveResolverMIPS64);
};

class SlowPathCodeMIPS64 : public SlowPathCode {
 public:
  explicit SlowPathCodeMIPS64(HInstruction* instruction)
      : SlowPathCode(instruction), entry_label_(), exit_label_() {}

  Mips64Label* GetEntryLabel() { return &entry_label_; }
  Mips64Label* GetExitLabel() { return &exit_label_; }

 private:
  Mips64Label entry_label_;
  Mips64Label exit_label_;

  DISALLOW_COPY_AND_ASSIGN(SlowPathCodeMIPS64);
};

class LocationsBuilderMIPS64 : public HGraphVisitor {
 public:
  LocationsBuilderMIPS64(HGraph* graph, CodeGeneratorMIPS64* codegen)
      : HGraphVisitor(graph), codegen_(codegen) {}

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS64(DECLARE_VISIT_INSTRUCTION)

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

  InvokeDexCallingConventionVisitorMIPS64 parameter_visitor_;

  CodeGeneratorMIPS64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(LocationsBuilderMIPS64);
};

class InstructionCodeGeneratorMIPS64 : public InstructionCodeGenerator {
 public:
  InstructionCodeGeneratorMIPS64(HGraph* graph, CodeGeneratorMIPS64* codegen);

#define DECLARE_VISIT_INSTRUCTION(name, super)     \
  void Visit##name(H##name* instr) OVERRIDE;

  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS64(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Unreachable instruction " << instruction->DebugName()
               << " (id " << instruction->GetId() << ")";
  }

  Mips64Assembler* GetAssembler() const { return assembler_; }

  // Compare-and-jump packed switch generates approx. 3 + 2.5 * N 32-bit
  // instructions for N cases.
  // Table-based packed switch generates approx. 11 32-bit instructions
  // and N 32-bit data words for N cases.
  // At N = 6 they come out as 18 and 17 32-bit words respectively.
  // We switch to the table-based method starting with 7 cases.
  static constexpr uint32_t kPackedSwitchJumpTableThreshold = 6;

  void GenerateMemoryBarrier(MemBarrierKind kind);

 private:
  void GenerateClassInitializationCheck(SlowPathCodeMIPS64* slow_path, GpuRegister class_reg);
  void GenerateSuspendCheck(HSuspendCheck* check, HBasicBlock* successor);
  void HandleBinaryOp(HBinaryOperation* operation);
  void HandleCondition(HCondition* instruction);
  void HandleShift(HBinaryOperation* operation);
  void HandleFieldSet(HInstruction* instruction,
                      const FieldInfo& field_info,
                      bool value_can_be_null);
  void HandleFieldGet(HInstruction* instruction, const FieldInfo& field_info);

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
                               GpuRegister obj,
                               uint32_t offset,
                               ReadBarrierOption read_barrier_option,
                               Mips64Label* label_low = nullptr);

  void GenerateTestAndBranch(HInstruction* instruction,
                             size_t condition_input_index,
                             Mips64Label* true_target,
                             Mips64Label* false_target);
  void DivRemOneOrMinusOne(HBinaryOperation* instruction);
  void DivRemByPowerOfTwo(HBinaryOperation* instruction);
  void GenerateDivRemWithAnyConstant(HBinaryOperation* instruction);
  void GenerateDivRemIntegral(HBinaryOperation* instruction);
  void GenerateIntLongCompare(IfCondition cond, bool is64bit, LocationSummary* locations);
  // When the function returns `false` it means that the condition holds if `dst` is non-zero
  // and doesn't hold if `dst` is zero. If it returns `true`, the roles of zero and non-zero
  // `dst` are exchanged.
  bool MaterializeIntLongCompare(IfCondition cond,
                                 bool is64bit,
                                 LocationSummary* input_locations,
                                 GpuRegister dst);
  void GenerateIntLongCompareAndBranch(IfCondition cond,
                                       bool is64bit,
                                       LocationSummary* locations,
                                       Mips64Label* label);
  void GenerateFpCompare(IfCondition cond,
                         bool gt_bias,
                         DataType::Type type,
                         LocationSummary* locations);
  // When the function returns `false` it means that the condition holds if `dst` is non-zero
  // and doesn't hold if `dst` is zero. If it returns `true`, the roles of zero and non-zero
  // `dst` are exchanged.
  bool MaterializeFpCompare(IfCondition cond,
                            bool gt_bias,
                            DataType::Type type,
                            LocationSummary* input_locations,
                            FpuRegister dst);
  void GenerateFpCompareAndBranch(IfCondition cond,
                                  bool gt_bias,
                                  DataType::Type type,
                                  LocationSummary* locations,
                                  Mips64Label* label);
  void HandleGoto(HInstruction* got, HBasicBlock* successor);
  void GenPackedSwitchWithCompares(GpuRegister value_reg,
                                   int32_t lower_bound,
                                   uint32_t num_entries,
                                   HBasicBlock* switch_block,
                                   HBasicBlock* default_block);
  void GenTableBasedPackedSwitch(GpuRegister value_reg,
                                 int32_t lower_bound,
                                 uint32_t num_entries,
                                 HBasicBlock* switch_block,
                                 HBasicBlock* default_block);
  int32_t VecAddress(LocationSummary* locations,
                     size_t size,
                     /* out */ GpuRegister* adjusted_base);
  void GenConditionalMove(HSelect* select);

  Mips64Assembler* const assembler_;
  CodeGeneratorMIPS64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(InstructionCodeGeneratorMIPS64);
};

class CodeGeneratorMIPS64 : public CodeGenerator {
 public:
  CodeGeneratorMIPS64(HGraph* graph,
                      const Mips64InstructionSetFeatures& isa_features,
                      const CompilerOptions& compiler_options,
                      OptimizingCompilerStats* stats = nullptr);
  virtual ~CodeGeneratorMIPS64() {}

  void GenerateFrameEntry() OVERRIDE;
  void GenerateFrameExit() OVERRIDE;

  void Bind(HBasicBlock* block) OVERRIDE;

  size_t GetWordSize() const OVERRIDE { return kMips64DoublewordSize; }

  size_t GetFloatingPointSpillSlotSize() const OVERRIDE {
    return GetGraph()->HasSIMD()
        ? 2 * kMips64DoublewordSize   // 16 bytes for each spill.
        : 1 * kMips64DoublewordSize;  //  8 bytes for each spill.
  }

  uintptr_t GetAddressOf(HBasicBlock* block) OVERRIDE {
    return assembler_.GetLabelLocation(GetLabelOf(block));
  }

  HGraphVisitor* GetLocationBuilder() OVERRIDE { return &location_builder_; }
  HGraphVisitor* GetInstructionVisitor() OVERRIDE { return &instruction_visitor_; }
  Mips64Assembler* GetAssembler() OVERRIDE { return &assembler_; }
  const Mips64Assembler& GetAssembler() const OVERRIDE { return assembler_; }

  // Emit linker patches.
  void EmitLinkerPatches(ArenaVector<linker::LinkerPatch>* linker_patches) OVERRIDE;
  void EmitJitRootPatches(uint8_t* code, const uint8_t* roots_data) OVERRIDE;

  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference field load when Baker's read barriers are used.
  void GenerateFieldLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             GpuRegister obj,
                                             uint32_t offset,
                                             Location temp,
                                             bool needs_null_check);
  // Fast path implementation of ReadBarrier::Barrier for a heap
  // reference array load when Baker's read barriers are used.
  void GenerateArrayLoadWithBakerReadBarrier(HInstruction* instruction,
                                             Location ref,
                                             GpuRegister obj,
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
                                                 GpuRegister obj,
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

  void MarkGCCard(GpuRegister object, GpuRegister value, bool value_can_be_null);

  // Register allocation.

  void SetupBlockedRegisters() const OVERRIDE;

  size_t SaveCoreRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t RestoreCoreRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t SaveFloatingPointRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;
  size_t RestoreFloatingPointRegister(size_t stack_index, uint32_t reg_id) OVERRIDE;

  void DumpCoreRegister(std::ostream& stream, int reg) const OVERRIDE;
  void DumpFloatingPointRegister(std::ostream& stream, int reg) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE { return InstructionSet::kMips64; }

  const Mips64InstructionSetFeatures& GetInstructionSetFeatures() const {
    return isa_features_;
  }

  Mips64Label* GetLabelOf(HBasicBlock* block) const {
    return CommonGetLabelOf<Mips64Label>(block_labels_, block);
  }

  void Initialize() OVERRIDE {
    block_labels_ = CommonInitializeLabels<Mips64Label>();
  }

  // We prefer aligned loads and stores (less code), so spill and restore registers in slow paths
  // at aligned locations.
  uint32_t GetPreferredSlotsAlignment() const OVERRIDE { return kMips64DoublewordSize; }

  void Finalize(CodeAllocator* allocator) OVERRIDE;

  // Code generation helpers.
  void MoveLocation(Location dst, Location src, DataType::Type dst_type) OVERRIDE;

  void MoveConstant(Location destination, int32_t value) OVERRIDE;

  void AddLocationAsTemp(Location location, LocationSummary* locations) OVERRIDE;


  void SwapLocations(Location loc1, Location loc2, DataType::Type type);

  // Generate code to invoke a runtime entry point.
  void InvokeRuntime(QuickEntrypointEnum entrypoint,
                     HInstruction* instruction,
                     uint32_t dex_pc,
                     SlowPathCode* slow_path = nullptr) OVERRIDE;

  // Generate code to invoke a runtime entry point, but do not record
  // PC-related information in a stack map.
  void InvokeRuntimeWithoutRecordingPcInfo(int32_t entry_point_offset,
                                           HInstruction* instruction,
                                           SlowPathCode* slow_path);

  void GenerateInvokeRuntime(int32_t entry_point_offset);

  ParallelMoveResolver* GetMoveResolver() OVERRIDE { return &move_resolver_; }

  bool NeedsTwoRegisters(DataType::Type type ATTRIBUTE_UNUSED) const OVERRIDE { return false; }

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
    UNIMPLEMENTED(FATAL) << "Not implemented on MIPS64";
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
  //     auipc r1, high       // patch
  //     lwu   r2, low(r1)    // patch
  //     beqzc r2, slow_path
  //   back:
  //     ...
  //   slow_path:
  //     ...
  //     sw    r2, low(r1)    // patch
  //     bc    back
  struct PcRelativePatchInfo : PatchInfo<Mips64Label> {
    PcRelativePatchInfo(const DexFile* dex_file,
                        uint32_t off_or_idx,
                        const PcRelativePatchInfo* info_high)
        : PatchInfo<Mips64Label>(dex_file, off_or_idx),
          patch_info_high(info_high) { }

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
  Literal* DeduplicateBootImageAddressLiteral(uint64_t address);

  void EmitPcRelativeAddressPlaceholderHigh(PcRelativePatchInfo* info_high,
                                            GpuRegister out,
                                            PcRelativePatchInfo* info_low = nullptr);

  void PatchJitRootUse(uint8_t* code,
                       const uint8_t* roots_data,
                       const Literal* literal,
                       uint64_t index_in_table) const;
  Literal* DeduplicateJitStringLiteral(const DexFile& dex_file,
                                       dex::StringIndex string_index,
                                       Handle<mirror::String> handle);
  Literal* DeduplicateJitClassLiteral(const DexFile& dex_file,
                                      dex::TypeIndex type_index,
                                      Handle<mirror::Class> handle);

 private:
  using Uint32ToLiteralMap = ArenaSafeMap<uint32_t, Literal*>;
  using Uint64ToLiteralMap = ArenaSafeMap<uint64_t, Literal*>;
  using StringToLiteralMap = ArenaSafeMap<StringReference,
                                          Literal*,
                                          StringReferenceValueComparator>;
  using TypeToLiteralMap = ArenaSafeMap<TypeReference,
                                        Literal*,
                                        TypeReferenceValueComparator>;

  Literal* DeduplicateUint32Literal(uint32_t value, Uint32ToLiteralMap* map);
  Literal* DeduplicateUint64Literal(uint64_t value);

  PcRelativePatchInfo* NewPcRelativePatch(const DexFile* dex_file,
                                          uint32_t offset_or_index,
                                          const PcRelativePatchInfo* info_high,
                                          ArenaDeque<PcRelativePatchInfo>* patches);

  template <linker::LinkerPatch (*Factory)(size_t, const DexFile*, uint32_t, uint32_t)>
  void EmitPcRelativeLinkerPatches(const ArenaDeque<PcRelativePatchInfo>& infos,
                                   ArenaVector<linker::LinkerPatch>* linker_patches);

  // Labels for each block that will be compiled.
  Mips64Label* block_labels_;  // Indexed by block id.
  Mips64Label frame_entry_label_;
  LocationsBuilderMIPS64 location_builder_;
  InstructionCodeGeneratorMIPS64 instruction_visitor_;
  ParallelMoveResolverMIPS64 move_resolver_;
  Mips64Assembler assembler_;
  const Mips64InstructionSetFeatures& isa_features_;

  // Deduplication map for 32-bit literals, used for non-patchable boot image addresses.
  Uint32ToLiteralMap uint32_literals_;
  // Deduplication map for 64-bit literals, used for non-patchable method address or method code
  // address.
  Uint64ToLiteralMap uint64_literals_;
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
  // PC-relative type patch info for kBssEntry.
  ArenaDeque<PcRelativePatchInfo> string_bss_entry_patches_;

  // Patches for string root accesses in JIT compiled code.
  StringToLiteralMap jit_string_patches_;
  // Patches for class root accesses in JIT compiled code.
  TypeToLiteralMap jit_class_patches_;

  DISALLOW_COPY_AND_ASSIGN(CodeGeneratorMIPS64);
};

}  // namespace mips64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_MIPS64_H_
