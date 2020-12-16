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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_

#include "base/array_ref.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "data_type.h"
#include "dex/code_item_accessors.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "handle.h"
#include "nodes.h"
#include "quicken_info.h"

namespace art {

class ArenaBitVector;
class ArtField;
class ArtMethod;
class CodeGenerator;
class CompilerDriver;
class DexCompilationUnit;
class HBasicBlockBuilder;
class Instruction;
class OptimizingCompilerStats;
class SsaBuilder;
class VariableSizedHandleScope;

namespace mirror {
class Class;
}  // namespace mirror

class HInstructionBuilder : public ValueObject {
 public:
  HInstructionBuilder(HGraph* graph,
                      HBasicBlockBuilder* block_builder,
                      SsaBuilder* ssa_builder,
                      const DexFile* dex_file,
                      const CodeItemDebugInfoAccessor& accessor,
                      DataType::Type return_type,
                      const DexCompilationUnit* dex_compilation_unit,
                      const DexCompilationUnit* outer_compilation_unit,
                      CompilerDriver* compiler_driver,
                      CodeGenerator* code_generator,
                      ArrayRef<const uint8_t> interpreter_metadata,
                      OptimizingCompilerStats* compiler_stats,
                      VariableSizedHandleScope* handles,
                      ScopedArenaAllocator* local_allocator);

  bool Build();
  void BuildIntrinsic(ArtMethod* method);

 private:
  void InitializeBlockLocals();
  void PropagateLocalsToCatchBlocks();
  void SetLoopHeaderPhiInputs();

  bool ProcessDexInstruction(const Instruction& instruction, uint32_t dex_pc, size_t quicken_index);
  ArenaBitVector* FindNativeDebugInfoLocations();

  bool CanDecodeQuickenedInfo() const;
  uint16_t LookupQuickenedInfo(uint32_t quicken_index);

  HBasicBlock* FindBlockStartingAt(uint32_t dex_pc) const;

  ScopedArenaVector<HInstruction*>* GetLocalsFor(HBasicBlock* block);
  // Out of line version of GetLocalsFor(), which has a fast path that is
  // beneficial to get inlined by callers.
  ScopedArenaVector<HInstruction*>* GetLocalsForWithAllocation(
      HBasicBlock* block, ScopedArenaVector<HInstruction*>* locals, const size_t vregs);
  HInstruction* ValueOfLocalAt(HBasicBlock* block, size_t local);
  HInstruction* LoadLocal(uint32_t register_index, DataType::Type type) const;
  HInstruction* LoadNullCheckedLocal(uint32_t register_index, uint32_t dex_pc);
  void UpdateLocal(uint32_t register_index, HInstruction* instruction);

  void AppendInstruction(HInstruction* instruction);
  void InsertInstructionAtTop(HInstruction* instruction);
  void InitializeInstruction(HInstruction* instruction);

  void InitializeParameters();

  // Returns whether the current method needs access check for the type.
  // Output parameter finalizable is set to whether the type is finalizable.
  bool NeedsAccessCheck(dex::TypeIndex type_index, /*out*/bool* finalizable) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<typename T>
  void Unop_12x(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  template<typename T>
  void Binop_23x(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  template<typename T>
  void Binop_23x_shift(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  void Binop_23x_cmp(const Instruction& instruction,
                     DataType::Type type,
                     ComparisonBias bias,
                     uint32_t dex_pc);

  template<typename T>
  void Binop_12x(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  template<typename T>
  void Binop_12x_shift(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  template<typename T>
  void Binop_22b(const Instruction& instruction, bool reverse, uint32_t dex_pc);

  template<typename T>
  void Binop_22s(const Instruction& instruction, bool reverse, uint32_t dex_pc);

  template<typename T> void If_21t(const Instruction& instruction, uint32_t dex_pc);
  template<typename T> void If_22t(const Instruction& instruction, uint32_t dex_pc);

  void Conversion_12x(const Instruction& instruction,
                      DataType::Type input_type,
                      DataType::Type result_type,
                      uint32_t dex_pc);

  void BuildCheckedDivRem(uint16_t out_reg,
                          uint16_t first_reg,
                          int64_t second_reg_or_constant,
                          uint32_t dex_pc,
                          DataType::Type type,
                          bool second_is_lit,
                          bool is_div);

  void BuildReturn(const Instruction& instruction, DataType::Type type, uint32_t dex_pc);

  // Builds an instance field access node and returns whether the instruction is supported.
  bool BuildInstanceFieldAccess(const Instruction& instruction,
                                uint32_t dex_pc,
                                bool is_put,
                                size_t quicken_index);

  void BuildUnresolvedStaticFieldAccess(const Instruction& instruction,
                                        uint32_t dex_pc,
                                        bool is_put,
                                        DataType::Type field_type);
  // Builds a static field access node.
  void BuildStaticFieldAccess(const Instruction& instruction, uint32_t dex_pc, bool is_put);

  void BuildArrayAccess(const Instruction& instruction,
                        uint32_t dex_pc,
                        bool is_get,
                        DataType::Type anticipated_type);

  // Builds an invocation node and returns whether the instruction is supported.
  bool BuildInvoke(const Instruction& instruction,
                   uint32_t dex_pc,
                   uint32_t method_idx,
                   uint32_t number_of_vreg_arguments,
                   bool is_range,
                   uint32_t* args,
                   uint32_t register_index);

  // Builds an invocation node for invoke-polymorphic and returns whether the
  // instruction is supported.
  bool BuildInvokePolymorphic(const Instruction& instruction,
                              uint32_t dex_pc,
                              uint32_t method_idx,
                              uint32_t proto_idx,
                              uint32_t number_of_vreg_arguments,
                              bool is_range,
                              uint32_t* args,
                              uint32_t register_index);

  // Builds a new array node and the instructions that fill it.
  HNewArray* BuildFilledNewArray(uint32_t dex_pc,
                                 dex::TypeIndex type_index,
                                 uint32_t number_of_vreg_arguments,
                                 bool is_range,
                                 uint32_t* args,
                                 uint32_t register_index);

  void BuildFillArrayData(const Instruction& instruction, uint32_t dex_pc);

  // Fills the given object with data as specified in the fill-array-data
  // instruction. Currently only used for non-reference and non-floating point
  // arrays.
  template <typename T>
  void BuildFillArrayData(HInstruction* object,
                          const T* data,
                          uint32_t element_count,
                          DataType::Type anticipated_type,
                          uint32_t dex_pc);

  // Fills the given object with data as specified in the fill-array-data
  // instruction. The data must be for long and double arrays.
  void BuildFillWideArrayData(HInstruction* object,
                              const int64_t* data,
                              uint32_t element_count,
                              uint32_t dex_pc);

  // Builds a `HInstanceOf`, or a `HCheckCast` instruction.
  void BuildTypeCheck(const Instruction& instruction,
                      uint8_t destination,
                      uint8_t reference,
                      dex::TypeIndex type_index,
                      uint32_t dex_pc);

  // Builds an instruction sequence for a switch statement.
  void BuildSwitch(const Instruction& instruction, uint32_t dex_pc);

  // Builds a `HLoadString` loading the given `string_index`.
  void BuildLoadString(dex::StringIndex string_index, uint32_t dex_pc);

  // Builds a `HLoadClass` loading the given `type_index`.
  HLoadClass* BuildLoadClass(dex::TypeIndex type_index, uint32_t dex_pc);

  HLoadClass* BuildLoadClass(dex::TypeIndex type_index,
                             const DexFile& dex_file,
                             Handle<mirror::Class> klass,
                             uint32_t dex_pc,
                             bool needs_access_check)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the outer-most compiling method's class.
  ObjPtr<mirror::Class> GetOutermostCompilingClass() const;

  // Returns the class whose method is being compiled.
  ObjPtr<mirror::Class> GetCompilingClass() const;

  // Returns whether `type_index` points to the outer-most compiling method's class.
  bool IsOutermostCompilingClass(dex::TypeIndex type_index) const;

  void PotentiallySimplifyFakeString(uint16_t original_dex_register,
                                     uint32_t dex_pc,
                                     HInvoke* invoke);

  bool SetupInvokeArguments(HInvoke* invoke,
                            uint32_t number_of_vreg_arguments,
                            uint32_t* args,
                            uint32_t register_index,
                            bool is_range,
                            const char* descriptor,
                            size_t start_index,
                            size_t* argument_index);

  bool HandleInvoke(HInvoke* invoke,
                    uint32_t number_of_vreg_arguments,
                    uint32_t* args,
                    uint32_t register_index,
                    bool is_range,
                    const char* descriptor,
                    HClinitCheck* clinit_check,
                    bool is_unresolved);

  bool HandleStringInit(HInvoke* invoke,
                        uint32_t number_of_vreg_arguments,
                        uint32_t* args,
                        uint32_t register_index,
                        bool is_range,
                        const char* descriptor);
  void HandleStringInitResult(HInvokeStaticOrDirect* invoke);

  HClinitCheck* ProcessClinitCheckForInvoke(
      uint32_t dex_pc,
      ArtMethod* method,
      HInvokeStaticOrDirect::ClinitCheckRequirement* clinit_check_requirement)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Build a HNewInstance instruction.
  HNewInstance* BuildNewInstance(dex::TypeIndex type_index, uint32_t dex_pc);

  // Build a HConstructorFence for HNewInstance and HNewArray instructions. This ensures the
  // happens-before ordering for default-initialization of the object referred to by new_instance.
  void BuildConstructorFenceForAllocation(HInstruction* allocation);

  // Return whether the compiler can assume `cls` is initialized.
  bool IsInitialized(Handle<mirror::Class> cls) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Try to resolve a method using the class linker. Return null if a method could
  // not be resolved.
  ArtMethod* ResolveMethod(uint16_t method_idx, InvokeType invoke_type);

  // Try to resolve a field using the class linker. Return null if it could not
  // be found.
  ArtField* ResolveField(uint16_t field_idx, bool is_static, bool is_put);

  ObjPtr<mirror::Class> LookupResolvedType(dex::TypeIndex type_index,
                                           const DexCompilationUnit& compilation_unit) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<mirror::Class> LookupReferrerClass() const REQUIRES_SHARED(Locks::mutator_lock_);

  ArenaAllocator* const allocator_;
  HGraph* const graph_;
  VariableSizedHandleScope* const handles_;

  // The dex file where the method being compiled is, and the bytecode data.
  const DexFile* const dex_file_;
  const CodeItemDebugInfoAccessor code_item_accessor_;  // null for intrinsic graph.

  // The return type of the method being compiled.
  const DataType::Type return_type_;

  HBasicBlockBuilder* const block_builder_;
  SsaBuilder* const ssa_builder_;

  CompilerDriver* const compiler_driver_;

  CodeGenerator* const code_generator_;

  // The compilation unit of the current method being compiled. Note that
  // it can be an inlined method.
  const DexCompilationUnit* const dex_compilation_unit_;

  // The compilation unit of the outermost method being compiled. That is the
  // method being compiled (and not inlined), and potentially inlining other
  // methods.
  const DexCompilationUnit* const outer_compilation_unit_;

  // Original values kept after instruction quickening.
  QuickenInfoTable quicken_info_;

  OptimizingCompilerStats* const compilation_stats_;

  ScopedArenaAllocator* const local_allocator_;
  ScopedArenaVector<ScopedArenaVector<HInstruction*>> locals_for_;
  HBasicBlock* current_block_;
  ScopedArenaVector<HInstruction*>* current_locals_;
  HInstruction* latest_result_;
  // Current "this" parameter.
  // Valid only after InitializeParameters() finishes.
  // * Null for static methods.
  // * Non-null for instance methods.
  HParameterValue* current_this_parameter_;

  ScopedArenaVector<HBasicBlock*> loop_headers_;

  static constexpr int kDefaultNumberOfLoops = 2;

  DISALLOW_COPY_AND_ASSIGN(HInstructionBuilder);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_BUILDER_H_
