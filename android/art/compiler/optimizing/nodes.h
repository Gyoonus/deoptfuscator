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

#ifndef ART_COMPILER_OPTIMIZING_NODES_H_
#define ART_COMPILER_OPTIMIZING_NODES_H_

#include <algorithm>
#include <array>
#include <type_traits>

#include "base/arena_bit_vector.h"
#include "base/arena_containers.h"
#include "base/arena_object.h"
#include "base/array_ref.h"
#include "base/iteration_range.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/transform_array_ref.h"
#include "data_type.h"
#include "deoptimization_kind.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "dex/invoke_type.h"
#include "dex/method_reference.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "handle.h"
#include "handle_scope.h"
#include "intrinsics_enum.h"
#include "locations.h"
#include "mirror/class.h"
#include "offsets.h"
#include "utils/intrusive_forward_list.h"


namespace art {

class ArenaStack;
class GraphChecker;
class HBasicBlock;
class HConstructorFence;
class HCurrentMethod;
class HDoubleConstant;
class HEnvironment;
class HFloatConstant;
class HGraphBuilder;
class HGraphVisitor;
class HInstruction;
class HIntConstant;
class HInvoke;
class HLongConstant;
class HNullConstant;
class HParameterValue;
class HPhi;
class HSuspendCheck;
class HTryBoundary;
class LiveInterval;
class LocationSummary;
class SlowPathCode;
class SsaBuilder;

namespace mirror {
class DexCache;
}  // namespace mirror

static const int kDefaultNumberOfBlocks = 8;
static const int kDefaultNumberOfSuccessors = 2;
static const int kDefaultNumberOfPredecessors = 2;
static const int kDefaultNumberOfExceptionalPredecessors = 0;
static const int kDefaultNumberOfDominatedBlocks = 1;
static const int kDefaultNumberOfBackEdges = 1;

// The maximum (meaningful) distance (31) that can be used in an integer shift/rotate operation.
static constexpr int32_t kMaxIntShiftDistance = 0x1f;
// The maximum (meaningful) distance (63) that can be used in a long shift/rotate operation.
static constexpr int32_t kMaxLongShiftDistance = 0x3f;

static constexpr uint32_t kUnknownFieldIndex = static_cast<uint32_t>(-1);
static constexpr uint16_t kUnknownClassDefIndex = static_cast<uint16_t>(-1);

static constexpr InvokeType kInvalidInvokeType = static_cast<InvokeType>(-1);

static constexpr uint32_t kNoDexPc = -1;

inline bool IsSameDexFile(const DexFile& lhs, const DexFile& rhs) {
  // For the purposes of the compiler, the dex files must actually be the same object
  // if we want to safely treat them as the same. This is especially important for JIT
  // as custom class loaders can open the same underlying file (or memory) multiple
  // times and provide different class resolution but no two class loaders should ever
  // use the same DexFile object - doing so is an unsupported hack that can lead to
  // all sorts of weird failures.
  return &lhs == &rhs;
}

enum IfCondition {
  // All types.
  kCondEQ,  // ==
  kCondNE,  // !=
  // Signed integers and floating-point numbers.
  kCondLT,  // <
  kCondLE,  // <=
  kCondGT,  // >
  kCondGE,  // >=
  // Unsigned integers.
  kCondB,   // <
  kCondBE,  // <=
  kCondA,   // >
  kCondAE,  // >=
  // First and last aliases.
  kCondFirst = kCondEQ,
  kCondLast = kCondAE,
};

enum GraphAnalysisResult {
  kAnalysisSkipped,
  kAnalysisInvalidBytecode,
  kAnalysisFailThrowCatchLoop,
  kAnalysisFailAmbiguousArrayOp,
  kAnalysisSuccess,
};

template <typename T>
static inline typename std::make_unsigned<T>::type MakeUnsigned(T x) {
  return static_cast<typename std::make_unsigned<T>::type>(x);
}

class HInstructionList : public ValueObject {
 public:
  HInstructionList() : first_instruction_(nullptr), last_instruction_(nullptr) {}

  void AddInstruction(HInstruction* instruction);
  void RemoveInstruction(HInstruction* instruction);

  // Insert `instruction` before/after an existing instruction `cursor`.
  void InsertInstructionBefore(HInstruction* instruction, HInstruction* cursor);
  void InsertInstructionAfter(HInstruction* instruction, HInstruction* cursor);

  // Return true if this list contains `instruction`.
  bool Contains(HInstruction* instruction) const;

  // Return true if `instruction1` is found before `instruction2` in
  // this instruction list and false otherwise.  Abort if none
  // of these instructions is found.
  bool FoundBefore(const HInstruction* instruction1,
                   const HInstruction* instruction2) const;

  bool IsEmpty() const { return first_instruction_ == nullptr; }
  void Clear() { first_instruction_ = last_instruction_ = nullptr; }

  // Update the block of all instructions to be `block`.
  void SetBlockOfInstructions(HBasicBlock* block) const;

  void AddAfter(HInstruction* cursor, const HInstructionList& instruction_list);
  void AddBefore(HInstruction* cursor, const HInstructionList& instruction_list);
  void Add(const HInstructionList& instruction_list);

  // Return the number of instructions in the list. This is an expensive operation.
  size_t CountSize() const;

 private:
  HInstruction* first_instruction_;
  HInstruction* last_instruction_;

  friend class HBasicBlock;
  friend class HGraph;
  friend class HInstruction;
  friend class HInstructionIterator;
  friend class HInstructionIteratorHandleChanges;
  friend class HBackwardInstructionIterator;
  DISALLOW_COPY_AND_ASSIGN(HInstructionList);
};

class ReferenceTypeInfo : ValueObject {
 public:
  typedef Handle<mirror::Class> TypeHandle;

  static ReferenceTypeInfo Create(TypeHandle type_handle, bool is_exact);

  static ReferenceTypeInfo Create(TypeHandle type_handle) REQUIRES_SHARED(Locks::mutator_lock_) {
    return Create(type_handle, type_handle->CannotBeAssignedFromOtherTypes());
  }

  static ReferenceTypeInfo CreateUnchecked(TypeHandle type_handle, bool is_exact) {
    return ReferenceTypeInfo(type_handle, is_exact);
  }

  static ReferenceTypeInfo CreateInvalid() { return ReferenceTypeInfo(); }

  static bool IsValidHandle(TypeHandle handle) {
    return handle.GetReference() != nullptr;
  }

  bool IsValid() const {
    return IsValidHandle(type_handle_);
  }

  bool IsExact() const { return is_exact_; }

  bool IsObjectClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsObjectClass();
  }

  bool IsStringClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsStringClass();
  }

  bool IsObjectArray() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return IsArrayClass() && GetTypeHandle()->GetComponentType()->IsObjectClass();
  }

  bool IsInterface() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsInterface();
  }

  bool IsArrayClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsArrayClass();
  }

  bool IsPrimitiveArrayClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsPrimitiveArray();
  }

  bool IsNonPrimitiveArrayClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    return GetTypeHandle()->IsArrayClass() && !GetTypeHandle()->IsPrimitiveArray();
  }

  bool CanArrayHold(ReferenceTypeInfo rti)  const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    if (!IsExact()) return false;
    if (!IsArrayClass()) return false;
    return GetTypeHandle()->GetComponentType()->IsAssignableFrom(rti.GetTypeHandle().Get());
  }

  bool CanArrayHoldValuesOf(ReferenceTypeInfo rti)  const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    if (!IsExact()) return false;
    if (!IsArrayClass()) return false;
    if (!rti.IsArrayClass()) return false;
    return GetTypeHandle()->GetComponentType()->IsAssignableFrom(
        rti.GetTypeHandle()->GetComponentType());
  }

  Handle<mirror::Class> GetTypeHandle() const { return type_handle_; }

  bool IsSupertypeOf(ReferenceTypeInfo rti) const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    DCHECK(rti.IsValid());
    return GetTypeHandle()->IsAssignableFrom(rti.GetTypeHandle().Get());
  }

  bool IsStrictSupertypeOf(ReferenceTypeInfo rti) const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsValid());
    DCHECK(rti.IsValid());
    return GetTypeHandle().Get() != rti.GetTypeHandle().Get() &&
        GetTypeHandle()->IsAssignableFrom(rti.GetTypeHandle().Get());
  }

  // Returns true if the type information provide the same amount of details.
  // Note that it does not mean that the instructions have the same actual type
  // (because the type can be the result of a merge).
  bool IsEqual(ReferenceTypeInfo rti) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!IsValid() && !rti.IsValid()) {
      // Invalid types are equal.
      return true;
    }
    if (!IsValid() || !rti.IsValid()) {
      // One is valid, the other not.
      return false;
    }
    return IsExact() == rti.IsExact()
        && GetTypeHandle().Get() == rti.GetTypeHandle().Get();
  }

 private:
  ReferenceTypeInfo() : type_handle_(TypeHandle()), is_exact_(false) {}
  ReferenceTypeInfo(TypeHandle type_handle, bool is_exact)
      : type_handle_(type_handle), is_exact_(is_exact) { }

  // The class of the object.
  TypeHandle type_handle_;
  // Whether or not the type is exact or a superclass of the actual type.
  // Whether or not we have any information about this type.
  bool is_exact_;
};

std::ostream& operator<<(std::ostream& os, const ReferenceTypeInfo& rhs);

// Control-flow graph of a method. Contains a list of basic blocks.
class HGraph : public ArenaObject<kArenaAllocGraph> {
 public:
  HGraph(ArenaAllocator* allocator,
         ArenaStack* arena_stack,
         const DexFile& dex_file,
         uint32_t method_idx,
         InstructionSet instruction_set,
         InvokeType invoke_type = kInvalidInvokeType,
         bool debuggable = false,
         bool osr = false,
         int start_instruction_id = 0)
      : allocator_(allocator),
        arena_stack_(arena_stack),
        blocks_(allocator->Adapter(kArenaAllocBlockList)),
        reverse_post_order_(allocator->Adapter(kArenaAllocReversePostOrder)),
        linear_order_(allocator->Adapter(kArenaAllocLinearOrder)),
        entry_block_(nullptr),
        exit_block_(nullptr),
        maximum_number_of_out_vregs_(0),
        number_of_vregs_(0),
        number_of_in_vregs_(0),
        temporaries_vreg_slots_(0),
        has_bounds_checks_(false),
        has_try_catch_(false),
        has_simd_(false),
        has_loops_(false),
        has_irreducible_loops_(false),
        debuggable_(debuggable),
        current_instruction_id_(start_instruction_id),
        dex_file_(dex_file),
        method_idx_(method_idx),
        invoke_type_(invoke_type),
        in_ssa_form_(false),
        number_of_cha_guards_(0),
        instruction_set_(instruction_set),
        cached_null_constant_(nullptr),
        cached_int_constants_(std::less<int32_t>(), allocator->Adapter(kArenaAllocConstantsMap)),
        cached_float_constants_(std::less<int32_t>(), allocator->Adapter(kArenaAllocConstantsMap)),
        cached_long_constants_(std::less<int64_t>(), allocator->Adapter(kArenaAllocConstantsMap)),
        cached_double_constants_(std::less<int64_t>(), allocator->Adapter(kArenaAllocConstantsMap)),
        cached_current_method_(nullptr),
        art_method_(nullptr),
        inexact_object_rti_(ReferenceTypeInfo::CreateInvalid()),
        osr_(osr),
        cha_single_implementation_list_(allocator->Adapter(kArenaAllocCHA)) {
    blocks_.reserve(kDefaultNumberOfBlocks);
  }

  // Acquires and stores RTI of inexact Object to be used when creating HNullConstant.
  void InitializeInexactObjectRTI(VariableSizedHandleScope* handles);

  ArenaAllocator* GetAllocator() const { return allocator_; }
  ArenaStack* GetArenaStack() const { return arena_stack_; }
  const ArenaVector<HBasicBlock*>& GetBlocks() const { return blocks_; }

  bool IsInSsaForm() const { return in_ssa_form_; }
  void SetInSsaForm() { in_ssa_form_ = true; }

  HBasicBlock* GetEntryBlock() const { return entry_block_; }
  HBasicBlock* GetExitBlock() const { return exit_block_; }
  bool HasExitBlock() const { return exit_block_ != nullptr; }

  void SetEntryBlock(HBasicBlock* block) { entry_block_ = block; }
  void SetExitBlock(HBasicBlock* block) { exit_block_ = block; }

  void AddBlock(HBasicBlock* block);

  void ComputeDominanceInformation();
  void ClearDominanceInformation();
  void ClearLoopInformation();
  void FindBackEdges(ArenaBitVector* visited);
  GraphAnalysisResult BuildDominatorTree();
  void SimplifyCFG();
  void SimplifyCatchBlocks();

  // Analyze all natural loops in this graph. Returns a code specifying that it
  // was successful or the reason for failure. The method will fail if a loop
  // is a throw-catch loop, i.e. the header is a catch block.
  GraphAnalysisResult AnalyzeLoops() const;

  // Iterate over blocks to compute try block membership. Needs reverse post
  // order and loop information.
  void ComputeTryBlockInformation();

  // Inline this graph in `outer_graph`, replacing the given `invoke` instruction.
  // Returns the instruction to replace the invoke expression or null if the
  // invoke is for a void method. Note that the caller is responsible for replacing
  // and removing the invoke instruction.
  HInstruction* InlineInto(HGraph* outer_graph, HInvoke* invoke);

  // Update the loop and try membership of `block`, which was spawned from `reference`.
  // In case `reference` is a back edge, `replace_if_back_edge` notifies whether `block`
  // should be the new back edge.
  void UpdateLoopAndTryInformationOfNewBlock(HBasicBlock* block,
                                             HBasicBlock* reference,
                                             bool replace_if_back_edge);

  // Need to add a couple of blocks to test if the loop body is entered and
  // put deoptimization instructions, etc.
  void TransformLoopHeaderForBCE(HBasicBlock* header);

  // Adds a new loop directly after the loop with the given header and exit.
  // Returns the new preheader.
  HBasicBlock* TransformLoopForVectorization(HBasicBlock* header,
                                             HBasicBlock* body,
                                             HBasicBlock* exit);

  // Removes `block` from the graph. Assumes `block` has been disconnected from
  // other blocks and has no instructions or phis.
  void DeleteDeadEmptyBlock(HBasicBlock* block);

  // Splits the edge between `block` and `successor` while preserving the
  // indices in the predecessor/successor lists. If there are multiple edges
  // between the blocks, the lowest indices are used.
  // Returns the new block which is empty and has the same dex pc as `successor`.
  HBasicBlock* SplitEdge(HBasicBlock* block, HBasicBlock* successor);

  void SplitCriticalEdge(HBasicBlock* block, HBasicBlock* successor);
  void OrderLoopHeaderPredecessors(HBasicBlock* header);

  // Transform a loop into a format with a single preheader.
  //
  // Each phi in the header should be split: original one in the header should only hold
  // inputs reachable from the back edges and a single input from the preheader. The newly created
  // phi in the preheader should collate the inputs from the original multiple incoming blocks.
  //
  // Loops in the graph typically have a single preheader, so this method is used to "repair" loops
  // that no longer have this property.
  void TransformLoopToSinglePreheaderFormat(HBasicBlock* header);

  void SimplifyLoop(HBasicBlock* header);

  int32_t GetNextInstructionId() {
    CHECK_NE(current_instruction_id_, INT32_MAX);
    return current_instruction_id_++;
  }

  int32_t GetCurrentInstructionId() const {
    return current_instruction_id_;
  }

  void SetCurrentInstructionId(int32_t id) {
    CHECK_GE(id, current_instruction_id_);
    current_instruction_id_ = id;
  }

  uint16_t GetMaximumNumberOfOutVRegs() const {
    return maximum_number_of_out_vregs_;
  }

  void SetMaximumNumberOfOutVRegs(uint16_t new_value) {
    maximum_number_of_out_vregs_ = new_value;
  }

  void UpdateMaximumNumberOfOutVRegs(uint16_t other_value) {
    maximum_number_of_out_vregs_ = std::max(maximum_number_of_out_vregs_, other_value);
  }

  void UpdateTemporariesVRegSlots(size_t slots) {
    temporaries_vreg_slots_ = std::max(slots, temporaries_vreg_slots_);
  }

  size_t GetTemporariesVRegSlots() const {
    DCHECK(!in_ssa_form_);
    return temporaries_vreg_slots_;
  }

  void SetNumberOfVRegs(uint16_t number_of_vregs) {
    number_of_vregs_ = number_of_vregs;
  }

  uint16_t GetNumberOfVRegs() const {
    return number_of_vregs_;
  }

  void SetNumberOfInVRegs(uint16_t value) {
    number_of_in_vregs_ = value;
  }

  uint16_t GetNumberOfInVRegs() const {
    return number_of_in_vregs_;
  }

  uint16_t GetNumberOfLocalVRegs() const {
    DCHECK(!in_ssa_form_);
    return number_of_vregs_ - number_of_in_vregs_;
  }

  const ArenaVector<HBasicBlock*>& GetReversePostOrder() const {
    return reverse_post_order_;
  }

  ArrayRef<HBasicBlock* const> GetReversePostOrderSkipEntryBlock() {
    DCHECK(GetReversePostOrder()[0] == entry_block_);
    return ArrayRef<HBasicBlock* const>(GetReversePostOrder()).SubArray(1);
  }

  IterationRange<ArenaVector<HBasicBlock*>::const_reverse_iterator> GetPostOrder() const {
    return ReverseRange(GetReversePostOrder());
  }

  const ArenaVector<HBasicBlock*>& GetLinearOrder() const {
    return linear_order_;
  }

  IterationRange<ArenaVector<HBasicBlock*>::const_reverse_iterator> GetLinearPostOrder() const {
    return ReverseRange(GetLinearOrder());
  }

  bool HasBoundsChecks() const {
    return has_bounds_checks_;
  }

  void SetHasBoundsChecks(bool value) {
    has_bounds_checks_ = value;
  }

  bool IsDebuggable() const { return debuggable_; }

  // Returns a constant of the given type and value. If it does not exist
  // already, it is created and inserted into the graph. This method is only for
  // integral types.
  HConstant* GetConstant(DataType::Type type, int64_t value, uint32_t dex_pc = kNoDexPc);

  // TODO: This is problematic for the consistency of reference type propagation
  // because it can be created anytime after the pass and thus it will be left
  // with an invalid type.
  HNullConstant* GetNullConstant(uint32_t dex_pc = kNoDexPc);

  HIntConstant* GetIntConstant(int32_t value, uint32_t dex_pc = kNoDexPc) {
    return CreateConstant(value, &cached_int_constants_, dex_pc);
  }
  HLongConstant* GetLongConstant(int64_t value, uint32_t dex_pc = kNoDexPc) {
    return CreateConstant(value, &cached_long_constants_, dex_pc);
  }
  HFloatConstant* GetFloatConstant(float value, uint32_t dex_pc = kNoDexPc) {
    return CreateConstant(bit_cast<int32_t, float>(value), &cached_float_constants_, dex_pc);
  }
  HDoubleConstant* GetDoubleConstant(double value, uint32_t dex_pc = kNoDexPc) {
    return CreateConstant(bit_cast<int64_t, double>(value), &cached_double_constants_, dex_pc);
  }

  HCurrentMethod* GetCurrentMethod();

  const DexFile& GetDexFile() const {
    return dex_file_;
  }

  uint32_t GetMethodIdx() const {
    return method_idx_;
  }

  // Get the method name (without the signature), e.g. "<init>"
  const char* GetMethodName() const;

  // Get the pretty method name (class + name + optionally signature).
  std::string PrettyMethod(bool with_signature = true) const;

  InvokeType GetInvokeType() const {
    return invoke_type_;
  }

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  bool IsCompilingOsr() const { return osr_; }

  ArenaSet<ArtMethod*>& GetCHASingleImplementationList() {
    return cha_single_implementation_list_;
  }

  void AddCHASingleImplementationDependency(ArtMethod* method) {
    cha_single_implementation_list_.insert(method);
  }

  bool HasShouldDeoptimizeFlag() const {
    return number_of_cha_guards_ != 0;
  }

  bool HasTryCatch() const { return has_try_catch_; }
  void SetHasTryCatch(bool value) { has_try_catch_ = value; }

  bool HasSIMD() const { return has_simd_; }
  void SetHasSIMD(bool value) { has_simd_ = value; }

  bool HasLoops() const { return has_loops_; }
  void SetHasLoops(bool value) { has_loops_ = value; }

  bool HasIrreducibleLoops() const { return has_irreducible_loops_; }
  void SetHasIrreducibleLoops(bool value) { has_irreducible_loops_ = value; }

  ArtMethod* GetArtMethod() const { return art_method_; }
  void SetArtMethod(ArtMethod* method) { art_method_ = method; }

  // Returns an instruction with the opposite Boolean value from 'cond'.
  // The instruction has been inserted into the graph, either as a constant, or
  // before cursor.
  HInstruction* InsertOppositeCondition(HInstruction* cond, HInstruction* cursor);

  ReferenceTypeInfo GetInexactObjectRti() const { return inexact_object_rti_; }

  uint32_t GetNumberOfCHAGuards() { return number_of_cha_guards_; }
  void SetNumberOfCHAGuards(uint32_t num) { number_of_cha_guards_ = num; }
  void IncrementNumberOfCHAGuards() { number_of_cha_guards_++; }

 private:
  void RemoveInstructionsAsUsersFromDeadBlocks(const ArenaBitVector& visited) const;
  void RemoveDeadBlocks(const ArenaBitVector& visited);

  template <class InstructionType, typename ValueType>
  InstructionType* CreateConstant(ValueType value,
                                  ArenaSafeMap<ValueType, InstructionType*>* cache,
                                  uint32_t dex_pc = kNoDexPc) {
    // Try to find an existing constant of the given value.
    InstructionType* constant = nullptr;
    auto cached_constant = cache->find(value);
    if (cached_constant != cache->end()) {
      constant = cached_constant->second;
    }

    // If not found or previously deleted, create and cache a new instruction.
    // Don't bother reviving a previously deleted instruction, for simplicity.
    if (constant == nullptr || constant->GetBlock() == nullptr) {
      constant = new (allocator_) InstructionType(value, dex_pc);
      cache->Overwrite(value, constant);
      InsertConstant(constant);
    }
    return constant;
  }

  void InsertConstant(HConstant* instruction);

  // Cache a float constant into the graph. This method should only be
  // called by the SsaBuilder when creating "equivalent" instructions.
  void CacheFloatConstant(HFloatConstant* constant);

  // See CacheFloatConstant comment.
  void CacheDoubleConstant(HDoubleConstant* constant);

  ArenaAllocator* const allocator_;
  ArenaStack* const arena_stack_;

  // List of blocks in insertion order.
  ArenaVector<HBasicBlock*> blocks_;

  // List of blocks to perform a reverse post order tree traversal.
  ArenaVector<HBasicBlock*> reverse_post_order_;

  // List of blocks to perform a linear order tree traversal. Unlike the reverse
  // post order, this order is not incrementally kept up-to-date.
  ArenaVector<HBasicBlock*> linear_order_;

  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;

  // The maximum number of virtual registers arguments passed to a HInvoke in this graph.
  uint16_t maximum_number_of_out_vregs_;

  // The number of virtual registers in this method. Contains the parameters.
  uint16_t number_of_vregs_;

  // The number of virtual registers used by parameters of this method.
  uint16_t number_of_in_vregs_;

  // Number of vreg size slots that the temporaries use (used in baseline compiler).
  size_t temporaries_vreg_slots_;

  // Flag whether there are bounds checks in the graph. We can skip
  // BCE if it's false. It's only best effort to keep it up to date in
  // the presence of code elimination so there might be false positives.
  bool has_bounds_checks_;

  // Flag whether there are try/catch blocks in the graph. We will skip
  // try/catch-related passes if it's false. It's only best effort to keep
  // it up to date in the presence of code elimination so there might be
  // false positives.
  bool has_try_catch_;

  // Flag whether SIMD instructions appear in the graph. If true, the
  // code generators may have to be more careful spilling the wider
  // contents of SIMD registers.
  bool has_simd_;

  // Flag whether there are any loops in the graph. We can skip loop
  // optimization if it's false. It's only best effort to keep it up
  // to date in the presence of code elimination so there might be false
  // positives.
  bool has_loops_;

  // Flag whether there are any irreducible loops in the graph. It's only
  // best effort to keep it up to date in the presence of code elimination
  // so there might be false positives.
  bool has_irreducible_loops_;

  // Indicates whether the graph should be compiled in a way that
  // ensures full debuggability. If false, we can apply more
  // aggressive optimizations that may limit the level of debugging.
  const bool debuggable_;

  // The current id to assign to a newly added instruction. See HInstruction.id_.
  int32_t current_instruction_id_;

  // The dex file from which the method is from.
  const DexFile& dex_file_;

  // The method index in the dex file.
  const uint32_t method_idx_;

  // If inlined, this encodes how the callee is being invoked.
  const InvokeType invoke_type_;

  // Whether the graph has been transformed to SSA form. Only used
  // in debug mode to ensure we are not using properties only valid
  // for non-SSA form (like the number of temporaries).
  bool in_ssa_form_;

  // Number of CHA guards in the graph. Used to short-circuit the
  // CHA guard optimization pass when there is no CHA guard left.
  uint32_t number_of_cha_guards_;

  const InstructionSet instruction_set_;

  // Cached constants.
  HNullConstant* cached_null_constant_;
  ArenaSafeMap<int32_t, HIntConstant*> cached_int_constants_;
  ArenaSafeMap<int32_t, HFloatConstant*> cached_float_constants_;
  ArenaSafeMap<int64_t, HLongConstant*> cached_long_constants_;
  ArenaSafeMap<int64_t, HDoubleConstant*> cached_double_constants_;

  HCurrentMethod* cached_current_method_;

  // The ArtMethod this graph is for. Note that for AOT, it may be null,
  // for example for methods whose declaring class could not be resolved
  // (such as when the superclass could not be found).
  ArtMethod* art_method_;

  // Keep the RTI of inexact Object to avoid having to pass stack handle
  // collection pointer to passes which may create NullConstant.
  ReferenceTypeInfo inexact_object_rti_;

  // Whether we are compiling this graph for on stack replacement: this will
  // make all loops seen as irreducible and emit special stack maps to mark
  // compiled code entries which the interpreter can directly jump to.
  const bool osr_;

  // List of methods that are assumed to have single implementation.
  ArenaSet<ArtMethod*> cha_single_implementation_list_;

  friend class SsaBuilder;           // For caching constants.
  friend class SsaLivenessAnalysis;  // For the linear order.
  friend class HInliner;             // For the reverse post order.
  ART_FRIEND_TEST(GraphTest, IfSuccessorSimpleJoinBlock1);
  DISALLOW_COPY_AND_ASSIGN(HGraph);
};

class HLoopInformation : public ArenaObject<kArenaAllocLoopInfo> {
 public:
  HLoopInformation(HBasicBlock* header, HGraph* graph)
      : header_(header),
        suspend_check_(nullptr),
        irreducible_(false),
        contains_irreducible_loop_(false),
        back_edges_(graph->GetAllocator()->Adapter(kArenaAllocLoopInfoBackEdges)),
        // Make bit vector growable, as the number of blocks may change.
        blocks_(graph->GetAllocator(),
                graph->GetBlocks().size(),
                true,
                kArenaAllocLoopInfoBackEdges) {
    back_edges_.reserve(kDefaultNumberOfBackEdges);
  }

  bool IsIrreducible() const { return irreducible_; }
  bool ContainsIrreducibleLoop() const { return contains_irreducible_loop_; }

  void Dump(std::ostream& os);

  HBasicBlock* GetHeader() const {
    return header_;
  }

  void SetHeader(HBasicBlock* block) {
    header_ = block;
  }

  HSuspendCheck* GetSuspendCheck() const { return suspend_check_; }
  void SetSuspendCheck(HSuspendCheck* check) { suspend_check_ = check; }
  bool HasSuspendCheck() const { return suspend_check_ != nullptr; }

  void AddBackEdge(HBasicBlock* back_edge) {
    back_edges_.push_back(back_edge);
  }

  void RemoveBackEdge(HBasicBlock* back_edge) {
    RemoveElement(back_edges_, back_edge);
  }

  bool IsBackEdge(const HBasicBlock& block) const {
    return ContainsElement(back_edges_, &block);
  }

  size_t NumberOfBackEdges() const {
    return back_edges_.size();
  }

  HBasicBlock* GetPreHeader() const;

  const ArenaVector<HBasicBlock*>& GetBackEdges() const {
    return back_edges_;
  }

  // Returns the lifetime position of the back edge that has the
  // greatest lifetime position.
  size_t GetLifetimeEnd() const;

  void ReplaceBackEdge(HBasicBlock* existing, HBasicBlock* new_back_edge) {
    ReplaceElement(back_edges_, existing, new_back_edge);
  }

  // Finds blocks that are part of this loop.
  void Populate();

  // Updates blocks population of the loop and all of its outer' ones recursively after the
  // population of the inner loop is updated.
  void PopulateInnerLoopUpwards(HLoopInformation* inner_loop);

  // Returns whether this loop information contains `block`.
  // Note that this loop information *must* be populated before entering this function.
  bool Contains(const HBasicBlock& block) const;

  // Returns whether this loop information is an inner loop of `other`.
  // Note that `other` *must* be populated before entering this function.
  bool IsIn(const HLoopInformation& other) const;

  // Returns true if instruction is not defined within this loop.
  bool IsDefinedOutOfTheLoop(HInstruction* instruction) const;

  const ArenaBitVector& GetBlocks() const { return blocks_; }

  void Add(HBasicBlock* block);
  void Remove(HBasicBlock* block);

  void ClearAllBlocks() {
    blocks_.ClearAllBits();
  }

  bool HasBackEdgeNotDominatedByHeader() const;

  bool IsPopulated() const {
    return blocks_.GetHighestBitSet() != -1;
  }

  bool DominatesAllBackEdges(HBasicBlock* block);

  bool HasExitEdge() const;

  // Resets back edge and blocks-in-loop data.
  void ResetBasicBlockData() {
    back_edges_.clear();
    ClearAllBlocks();
  }

 private:
  // Internal recursive implementation of `Populate`.
  void PopulateRecursive(HBasicBlock* block);
  void PopulateIrreducibleRecursive(HBasicBlock* block, ArenaBitVector* finalized);

  HBasicBlock* header_;
  HSuspendCheck* suspend_check_;
  bool irreducible_;
  bool contains_irreducible_loop_;
  ArenaVector<HBasicBlock*> back_edges_;
  ArenaBitVector blocks_;

  DISALLOW_COPY_AND_ASSIGN(HLoopInformation);
};

// Stores try/catch information for basic blocks.
// Note that HGraph is constructed so that catch blocks cannot simultaneously
// be try blocks.
class TryCatchInformation : public ArenaObject<kArenaAllocTryCatchInfo> {
 public:
  // Try block information constructor.
  explicit TryCatchInformation(const HTryBoundary& try_entry)
      : try_entry_(&try_entry),
        catch_dex_file_(nullptr),
        catch_type_index_(DexFile::kDexNoIndex16) {
    DCHECK(try_entry_ != nullptr);
  }

  // Catch block information constructor.
  TryCatchInformation(dex::TypeIndex catch_type_index, const DexFile& dex_file)
      : try_entry_(nullptr),
        catch_dex_file_(&dex_file),
        catch_type_index_(catch_type_index) {}

  bool IsTryBlock() const { return try_entry_ != nullptr; }

  const HTryBoundary& GetTryEntry() const {
    DCHECK(IsTryBlock());
    return *try_entry_;
  }

  bool IsCatchBlock() const { return catch_dex_file_ != nullptr; }

  bool IsCatchAllTypeIndex() const {
    DCHECK(IsCatchBlock());
    return !catch_type_index_.IsValid();
  }

  dex::TypeIndex GetCatchTypeIndex() const {
    DCHECK(IsCatchBlock());
    return catch_type_index_;
  }

  const DexFile& GetCatchDexFile() const {
    DCHECK(IsCatchBlock());
    return *catch_dex_file_;
  }

 private:
  // One of possibly several TryBoundary instructions entering the block's try.
  // Only set for try blocks.
  const HTryBoundary* try_entry_;

  // Exception type information. Only set for catch blocks.
  const DexFile* catch_dex_file_;
  const dex::TypeIndex catch_type_index_;
};

static constexpr size_t kNoLifetime = -1;
static constexpr uint32_t kInvalidBlockId = static_cast<uint32_t>(-1);

// A block in a method. Contains the list of instructions represented
// as a double linked list. Each block knows its predecessors and
// successors.

class HBasicBlock : public ArenaObject<kArenaAllocBasicBlock> {
 public:
  explicit HBasicBlock(HGraph* graph, uint32_t dex_pc = kNoDexPc)
      : graph_(graph),
        predecessors_(graph->GetAllocator()->Adapter(kArenaAllocPredecessors)),
        successors_(graph->GetAllocator()->Adapter(kArenaAllocSuccessors)),
        loop_information_(nullptr),
        dominator_(nullptr),
        dominated_blocks_(graph->GetAllocator()->Adapter(kArenaAllocDominated)),
        block_id_(kInvalidBlockId),
        dex_pc_(dex_pc),
        lifetime_start_(kNoLifetime),
        lifetime_end_(kNoLifetime),
        try_catch_information_(nullptr) {
    predecessors_.reserve(kDefaultNumberOfPredecessors);
    successors_.reserve(kDefaultNumberOfSuccessors);
    dominated_blocks_.reserve(kDefaultNumberOfDominatedBlocks);
  }

  const ArenaVector<HBasicBlock*>& GetPredecessors() const {
    return predecessors_;
  }

  const ArenaVector<HBasicBlock*>& GetSuccessors() const {
    return successors_;
  }

  ArrayRef<HBasicBlock* const> GetNormalSuccessors() const;
  ArrayRef<HBasicBlock* const> GetExceptionalSuccessors() const;

  bool HasSuccessor(const HBasicBlock* block, size_t start_from = 0u) {
    return ContainsElement(successors_, block, start_from);
  }

  const ArenaVector<HBasicBlock*>& GetDominatedBlocks() const {
    return dominated_blocks_;
  }

  bool IsEntryBlock() const {
    return graph_->GetEntryBlock() == this;
  }

  bool IsExitBlock() const {
    return graph_->GetExitBlock() == this;
  }

  bool IsSingleGoto() const;
  bool IsSingleReturn() const;
  bool IsSingleReturnOrReturnVoidAllowingPhis() const;
  bool IsSingleTryBoundary() const;

  // Returns true if this block emits nothing but a jump.
  bool IsSingleJump() const {
    HLoopInformation* loop_info = GetLoopInformation();
    return (IsSingleGoto() || IsSingleTryBoundary())
           // Back edges generate a suspend check.
           && (loop_info == nullptr || !loop_info->IsBackEdge(*this));
  }

  void AddBackEdge(HBasicBlock* back_edge) {
    if (loop_information_ == nullptr) {
      loop_information_ = new (graph_->GetAllocator()) HLoopInformation(this, graph_);
    }
    DCHECK_EQ(loop_information_->GetHeader(), this);
    loop_information_->AddBackEdge(back_edge);
  }

  // Registers a back edge; if the block was not a loop header before the call associates a newly
  // created loop info with it.
  //
  // Used in SuperblockCloner to preserve LoopInformation object instead of reseting loop
  // info for all blocks during back edges recalculation.
  void AddBackEdgeWhileUpdating(HBasicBlock* back_edge) {
    if (loop_information_ == nullptr || loop_information_->GetHeader() != this) {
      loop_information_ = new (graph_->GetAllocator()) HLoopInformation(this, graph_);
    }
    loop_information_->AddBackEdge(back_edge);
  }

  HGraph* GetGraph() const { return graph_; }
  void SetGraph(HGraph* graph) { graph_ = graph; }

  uint32_t GetBlockId() const { return block_id_; }
  void SetBlockId(int id) { block_id_ = id; }
  uint32_t GetDexPc() const { return dex_pc_; }

  HBasicBlock* GetDominator() const { return dominator_; }
  void SetDominator(HBasicBlock* dominator) { dominator_ = dominator; }
  void AddDominatedBlock(HBasicBlock* block) { dominated_blocks_.push_back(block); }

  void RemoveDominatedBlock(HBasicBlock* block) {
    RemoveElement(dominated_blocks_, block);
  }

  void ReplaceDominatedBlock(HBasicBlock* existing, HBasicBlock* new_block) {
    ReplaceElement(dominated_blocks_, existing, new_block);
  }

  void ClearDominanceInformation();

  int NumberOfBackEdges() const {
    return IsLoopHeader() ? loop_information_->NumberOfBackEdges() : 0;
  }

  HInstruction* GetFirstInstruction() const { return instructions_.first_instruction_; }
  HInstruction* GetLastInstruction() const { return instructions_.last_instruction_; }
  const HInstructionList& GetInstructions() const { return instructions_; }
  HInstruction* GetFirstPhi() const { return phis_.first_instruction_; }
  HInstruction* GetLastPhi() const { return phis_.last_instruction_; }
  const HInstructionList& GetPhis() const { return phis_; }

  HInstruction* GetFirstInstructionDisregardMoves() const;

  void AddSuccessor(HBasicBlock* block) {
    successors_.push_back(block);
    block->predecessors_.push_back(this);
  }

  void ReplaceSuccessor(HBasicBlock* existing, HBasicBlock* new_block) {
    size_t successor_index = GetSuccessorIndexOf(existing);
    existing->RemovePredecessor(this);
    new_block->predecessors_.push_back(this);
    successors_[successor_index] = new_block;
  }

  void ReplacePredecessor(HBasicBlock* existing, HBasicBlock* new_block) {
    size_t predecessor_index = GetPredecessorIndexOf(existing);
    existing->RemoveSuccessor(this);
    new_block->successors_.push_back(this);
    predecessors_[predecessor_index] = new_block;
  }

  // Insert `this` between `predecessor` and `successor. This method
  // preserves the indicies, and will update the first edge found between
  // `predecessor` and `successor`.
  void InsertBetween(HBasicBlock* predecessor, HBasicBlock* successor) {
    size_t predecessor_index = successor->GetPredecessorIndexOf(predecessor);
    size_t successor_index = predecessor->GetSuccessorIndexOf(successor);
    successor->predecessors_[predecessor_index] = this;
    predecessor->successors_[successor_index] = this;
    successors_.push_back(successor);
    predecessors_.push_back(predecessor);
  }

  void RemovePredecessor(HBasicBlock* block) {
    predecessors_.erase(predecessors_.begin() + GetPredecessorIndexOf(block));
  }

  void RemoveSuccessor(HBasicBlock* block) {
    successors_.erase(successors_.begin() + GetSuccessorIndexOf(block));
  }

  void ClearAllPredecessors() {
    predecessors_.clear();
  }

  void AddPredecessor(HBasicBlock* block) {
    predecessors_.push_back(block);
    block->successors_.push_back(this);
  }

  void SwapPredecessors() {
    DCHECK_EQ(predecessors_.size(), 2u);
    std::swap(predecessors_[0], predecessors_[1]);
  }

  void SwapSuccessors() {
    DCHECK_EQ(successors_.size(), 2u);
    std::swap(successors_[0], successors_[1]);
  }

  size_t GetPredecessorIndexOf(HBasicBlock* predecessor) const {
    return IndexOfElement(predecessors_, predecessor);
  }

  size_t GetSuccessorIndexOf(HBasicBlock* successor) const {
    return IndexOfElement(successors_, successor);
  }

  HBasicBlock* GetSinglePredecessor() const {
    DCHECK_EQ(GetPredecessors().size(), 1u);
    return GetPredecessors()[0];
  }

  HBasicBlock* GetSingleSuccessor() const {
    DCHECK_EQ(GetSuccessors().size(), 1u);
    return GetSuccessors()[0];
  }

  // Returns whether the first occurrence of `predecessor` in the list of
  // predecessors is at index `idx`.
  bool IsFirstIndexOfPredecessor(HBasicBlock* predecessor, size_t idx) const {
    DCHECK_EQ(GetPredecessors()[idx], predecessor);
    return GetPredecessorIndexOf(predecessor) == idx;
  }

  // Create a new block between this block and its predecessors. The new block
  // is added to the graph, all predecessor edges are relinked to it and an edge
  // is created to `this`. Returns the new empty block. Reverse post order or
  // loop and try/catch information are not updated.
  HBasicBlock* CreateImmediateDominator();

  // Split the block into two blocks just before `cursor`. Returns the newly
  // created, latter block. Note that this method will add the block to the
  // graph, create a Goto at the end of the former block and will create an edge
  // between the blocks. It will not, however, update the reverse post order or
  // loop and try/catch information.
  HBasicBlock* SplitBefore(HInstruction* cursor);

  // Split the block into two blocks just before `cursor`. Returns the newly
  // created block. Note that this method just updates raw block information,
  // like predecessors, successors, dominators, and instruction list. It does not
  // update the graph, reverse post order, loop information, nor make sure the
  // blocks are consistent (for example ending with a control flow instruction).
  HBasicBlock* SplitBeforeForInlining(HInstruction* cursor);

  // Similar to `SplitBeforeForInlining` but does it after `cursor`.
  HBasicBlock* SplitAfterForInlining(HInstruction* cursor);

  // Merge `other` at the end of `this`. Successors and dominated blocks of
  // `other` are changed to be successors and dominated blocks of `this`. Note
  // that this method does not update the graph, reverse post order, loop
  // information, nor make sure the blocks are consistent (for example ending
  // with a control flow instruction).
  void MergeWithInlined(HBasicBlock* other);

  // Replace `this` with `other`. Predecessors, successors, and dominated blocks
  // of `this` are moved to `other`.
  // Note that this method does not update the graph, reverse post order, loop
  // information, nor make sure the blocks are consistent (for example ending
  // with a control flow instruction).
  void ReplaceWith(HBasicBlock* other);

  // Merges the instructions of `other` at the end of `this`.
  void MergeInstructionsWith(HBasicBlock* other);

  // Merge `other` at the end of `this`. This method updates loops, reverse post
  // order, links to predecessors, successors, dominators and deletes the block
  // from the graph. The two blocks must be successive, i.e. `this` the only
  // predecessor of `other` and vice versa.
  void MergeWith(HBasicBlock* other);

  // Disconnects `this` from all its predecessors, successors and dominator,
  // removes it from all loops it is included in and eventually from the graph.
  // The block must not dominate any other block. Predecessors and successors
  // are safely updated.
  void DisconnectAndDelete();

  void AddInstruction(HInstruction* instruction);
  // Insert `instruction` before/after an existing instruction `cursor`.
  void InsertInstructionBefore(HInstruction* instruction, HInstruction* cursor);
  void InsertInstructionAfter(HInstruction* instruction, HInstruction* cursor);
  // Replace phi `initial` with `replacement` within this block.
  void ReplaceAndRemovePhiWith(HPhi* initial, HPhi* replacement);
  // Replace instruction `initial` with `replacement` within this block.
  void ReplaceAndRemoveInstructionWith(HInstruction* initial,
                                       HInstruction* replacement);
  void AddPhi(HPhi* phi);
  void InsertPhiAfter(HPhi* instruction, HPhi* cursor);
  // RemoveInstruction and RemovePhi delete a given instruction from the respective
  // instruction list. With 'ensure_safety' set to true, it verifies that the
  // instruction is not in use and removes it from the use lists of its inputs.
  void RemoveInstruction(HInstruction* instruction, bool ensure_safety = true);
  void RemovePhi(HPhi* phi, bool ensure_safety = true);
  void RemoveInstructionOrPhi(HInstruction* instruction, bool ensure_safety = true);

  bool IsLoopHeader() const {
    return IsInLoop() && (loop_information_->GetHeader() == this);
  }

  bool IsLoopPreHeaderFirstPredecessor() const {
    DCHECK(IsLoopHeader());
    return GetPredecessors()[0] == GetLoopInformation()->GetPreHeader();
  }

  bool IsFirstPredecessorBackEdge() const {
    DCHECK(IsLoopHeader());
    return GetLoopInformation()->IsBackEdge(*GetPredecessors()[0]);
  }

  HLoopInformation* GetLoopInformation() const {
    return loop_information_;
  }

  // Set the loop_information_ on this block. Overrides the current
  // loop_information if it is an outer loop of the passed loop information.
  // Note that this method is called while creating the loop information.
  void SetInLoop(HLoopInformation* info) {
    if (IsLoopHeader()) {
      // Nothing to do. This just means `info` is an outer loop.
    } else if (!IsInLoop()) {
      loop_information_ = info;
    } else if (loop_information_->Contains(*info->GetHeader())) {
      // Block is currently part of an outer loop. Make it part of this inner loop.
      // Note that a non loop header having a loop information means this loop information
      // has already been populated
      loop_information_ = info;
    } else {
      // Block is part of an inner loop. Do not update the loop information.
      // Note that we cannot do the check `info->Contains(loop_information_)->GetHeader()`
      // at this point, because this method is being called while populating `info`.
    }
  }

  // Raw update of the loop information.
  void SetLoopInformation(HLoopInformation* info) {
    loop_information_ = info;
  }

  bool IsInLoop() const { return loop_information_ != nullptr; }

  TryCatchInformation* GetTryCatchInformation() const { return try_catch_information_; }

  void SetTryCatchInformation(TryCatchInformation* try_catch_information) {
    try_catch_information_ = try_catch_information;
  }

  bool IsTryBlock() const {
    return try_catch_information_ != nullptr && try_catch_information_->IsTryBlock();
  }

  bool IsCatchBlock() const {
    return try_catch_information_ != nullptr && try_catch_information_->IsCatchBlock();
  }

  // Returns the try entry that this block's successors should have. They will
  // be in the same try, unless the block ends in a try boundary. In that case,
  // the appropriate try entry will be returned.
  const HTryBoundary* ComputeTryEntryOfSuccessors() const;

  bool HasThrowingInstructions() const;

  // Returns whether this block dominates the blocked passed as parameter.
  bool Dominates(HBasicBlock* block) const;

  size_t GetLifetimeStart() const { return lifetime_start_; }
  size_t GetLifetimeEnd() const { return lifetime_end_; }

  void SetLifetimeStart(size_t start) { lifetime_start_ = start; }
  void SetLifetimeEnd(size_t end) { lifetime_end_ = end; }

  bool EndsWithControlFlowInstruction() const;
  bool EndsWithIf() const;
  bool EndsWithTryBoundary() const;
  bool HasSinglePhi() const;

 private:
  HGraph* graph_;
  ArenaVector<HBasicBlock*> predecessors_;
  ArenaVector<HBasicBlock*> successors_;
  HInstructionList instructions_;
  HInstructionList phis_;
  HLoopInformation* loop_information_;
  HBasicBlock* dominator_;
  ArenaVector<HBasicBlock*> dominated_blocks_;
  uint32_t block_id_;
  // The dex program counter of the first instruction of this block.
  const uint32_t dex_pc_;
  size_t lifetime_start_;
  size_t lifetime_end_;
  TryCatchInformation* try_catch_information_;

  friend class HGraph;
  friend class HInstruction;

  DISALLOW_COPY_AND_ASSIGN(HBasicBlock);
};

// Iterates over the LoopInformation of all loops which contain 'block'
// from the innermost to the outermost.
class HLoopInformationOutwardIterator : public ValueObject {
 public:
  explicit HLoopInformationOutwardIterator(const HBasicBlock& block)
      : current_(block.GetLoopInformation()) {}

  bool Done() const { return current_ == nullptr; }

  void Advance() {
    DCHECK(!Done());
    current_ = current_->GetPreHeader()->GetLoopInformation();
  }

  HLoopInformation* Current() const {
    DCHECK(!Done());
    return current_;
  }

 private:
  HLoopInformation* current_;

  DISALLOW_COPY_AND_ASSIGN(HLoopInformationOutwardIterator);
};

#define FOR_EACH_CONCRETE_INSTRUCTION_COMMON(M)                         \
  M(Above, Condition)                                                   \
  M(AboveOrEqual, Condition)                                            \
  M(Add, BinaryOperation)                                               \
  M(And, BinaryOperation)                                               \
  M(ArrayGet, Instruction)                                              \
  M(ArrayLength, Instruction)                                           \
  M(ArraySet, Instruction)                                              \
  M(Below, Condition)                                                   \
  M(BelowOrEqual, Condition)                                            \
  M(BooleanNot, UnaryOperation)                                         \
  M(BoundsCheck, Instruction)                                           \
  M(BoundType, Instruction)                                             \
  M(CheckCast, Instruction)                                             \
  M(ClassTableGet, Instruction)                                         \
  M(ClearException, Instruction)                                        \
  M(ClinitCheck, Instruction)                                           \
  M(Compare, BinaryOperation)                                           \
  M(ConstructorFence, Instruction)                                      \
  M(CurrentMethod, Instruction)                                         \
  M(ShouldDeoptimizeFlag, Instruction)                                  \
  M(Deoptimize, Instruction)                                            \
  M(Div, BinaryOperation)                                               \
  M(DivZeroCheck, Instruction)                                          \
  M(DoubleConstant, Constant)                                           \
  M(Equal, Condition)                                                   \
  M(Exit, Instruction)                                                  \
  M(FloatConstant, Constant)                                            \
  M(Goto, Instruction)                                                  \
  M(GreaterThan, Condition)                                             \
  M(GreaterThanOrEqual, Condition)                                      \
  M(If, Instruction)                                                    \
  M(InstanceFieldGet, Instruction)                                      \
  M(InstanceFieldSet, Instruction)                                      \
  M(InstanceOf, Instruction)                                            \
  M(IntConstant, Constant)                                              \
  M(IntermediateAddress, Instruction)                                   \
  M(InvokeUnresolved, Invoke)                                           \
  M(InvokeInterface, Invoke)                                            \
  M(InvokeStaticOrDirect, Invoke)                                       \
  M(InvokeVirtual, Invoke)                                              \
  M(InvokePolymorphic, Invoke)                                          \
  M(LessThan, Condition)                                                \
  M(LessThanOrEqual, Condition)                                         \
  M(LoadClass, Instruction)                                             \
  M(LoadException, Instruction)                                         \
  M(LoadString, Instruction)                                            \
  M(LongConstant, Constant)                                             \
  M(MemoryBarrier, Instruction)                                         \
  M(MonitorOperation, Instruction)                                      \
  M(Mul, BinaryOperation)                                               \
  M(NativeDebugInfo, Instruction)                                       \
  M(Neg, UnaryOperation)                                                \
  M(NewArray, Instruction)                                              \
  M(NewInstance, Instruction)                                           \
  M(Not, UnaryOperation)                                                \
  M(NotEqual, Condition)                                                \
  M(NullConstant, Instruction)                                          \
  M(NullCheck, Instruction)                                             \
  M(Or, BinaryOperation)                                                \
  M(PackedSwitch, Instruction)                                          \
  M(ParallelMove, Instruction)                                          \
  M(ParameterValue, Instruction)                                        \
  M(Phi, Instruction)                                                   \
  M(Rem, BinaryOperation)                                               \
  M(Return, Instruction)                                                \
  M(ReturnVoid, Instruction)                                            \
  M(Ror, BinaryOperation)                                               \
  M(Shl, BinaryOperation)                                               \
  M(Shr, BinaryOperation)                                               \
  M(StaticFieldGet, Instruction)                                        \
  M(StaticFieldSet, Instruction)                                        \
  M(UnresolvedInstanceFieldGet, Instruction)                            \
  M(UnresolvedInstanceFieldSet, Instruction)                            \
  M(UnresolvedStaticFieldGet, Instruction)                              \
  M(UnresolvedStaticFieldSet, Instruction)                              \
  M(Select, Instruction)                                                \
  M(Sub, BinaryOperation)                                               \
  M(SuspendCheck, Instruction)                                          \
  M(Throw, Instruction)                                                 \
  M(TryBoundary, Instruction)                                           \
  M(TypeConversion, Instruction)                                        \
  M(UShr, BinaryOperation)                                              \
  M(Xor, BinaryOperation)                                               \
  M(VecReplicateScalar, VecUnaryOperation)                              \
  M(VecExtractScalar, VecUnaryOperation)                                \
  M(VecReduce, VecUnaryOperation)                                       \
  M(VecCnv, VecUnaryOperation)                                          \
  M(VecNeg, VecUnaryOperation)                                          \
  M(VecAbs, VecUnaryOperation)                                          \
  M(VecNot, VecUnaryOperation)                                          \
  M(VecAdd, VecBinaryOperation)                                         \
  M(VecHalvingAdd, VecBinaryOperation)                                  \
  M(VecSub, VecBinaryOperation)                                         \
  M(VecMul, VecBinaryOperation)                                         \
  M(VecDiv, VecBinaryOperation)                                         \
  M(VecMin, VecBinaryOperation)                                         \
  M(VecMax, VecBinaryOperation)                                         \
  M(VecAnd, VecBinaryOperation)                                         \
  M(VecAndNot, VecBinaryOperation)                                      \
  M(VecOr, VecBinaryOperation)                                          \
  M(VecXor, VecBinaryOperation)                                         \
  M(VecShl, VecBinaryOperation)                                         \
  M(VecShr, VecBinaryOperation)                                         \
  M(VecUShr, VecBinaryOperation)                                        \
  M(VecSetScalars, VecOperation)                                        \
  M(VecMultiplyAccumulate, VecOperation)                                \
  M(VecSADAccumulate, VecOperation)                                     \
  M(VecLoad, VecMemoryOperation)                                        \
  M(VecStore, VecMemoryOperation)                                       \

/*
 * Instructions, shared across several (not all) architectures.
 */
#if !defined(ART_ENABLE_CODEGEN_arm) && !defined(ART_ENABLE_CODEGEN_arm64)
#define FOR_EACH_CONCRETE_INSTRUCTION_SHARED(M)
#else
#define FOR_EACH_CONCRETE_INSTRUCTION_SHARED(M)                         \
  M(BitwiseNegatedRight, Instruction)                                   \
  M(DataProcWithShifterOp, Instruction)                                 \
  M(MultiplyAccumulate, Instruction)                                    \
  M(IntermediateAddressIndex, Instruction)
#endif

#define FOR_EACH_CONCRETE_INSTRUCTION_ARM(M)

#define FOR_EACH_CONCRETE_INSTRUCTION_ARM64(M)

#ifndef ART_ENABLE_CODEGEN_mips
#define FOR_EACH_CONCRETE_INSTRUCTION_MIPS(M)
#else
#define FOR_EACH_CONCRETE_INSTRUCTION_MIPS(M)                           \
  M(MipsComputeBaseMethodAddress, Instruction)                          \
  M(MipsPackedSwitch, Instruction)                                      \
  M(IntermediateArrayAddressIndex, Instruction)
#endif

#define FOR_EACH_CONCRETE_INSTRUCTION_MIPS64(M)

#ifndef ART_ENABLE_CODEGEN_x86
#define FOR_EACH_CONCRETE_INSTRUCTION_X86(M)
#else
#define FOR_EACH_CONCRETE_INSTRUCTION_X86(M)                            \
  M(X86ComputeBaseMethodAddress, Instruction)                           \
  M(X86LoadFromConstantTable, Instruction)                              \
  M(X86FPNeg, Instruction)                                              \
  M(X86PackedSwitch, Instruction)
#endif

#define FOR_EACH_CONCRETE_INSTRUCTION_X86_64(M)

#define FOR_EACH_CONCRETE_INSTRUCTION(M)                                \
  FOR_EACH_CONCRETE_INSTRUCTION_COMMON(M)                               \
  FOR_EACH_CONCRETE_INSTRUCTION_SHARED(M)                               \
  FOR_EACH_CONCRETE_INSTRUCTION_ARM(M)                                  \
  FOR_EACH_CONCRETE_INSTRUCTION_ARM64(M)                                \
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS(M)                                 \
  FOR_EACH_CONCRETE_INSTRUCTION_MIPS64(M)                               \
  FOR_EACH_CONCRETE_INSTRUCTION_X86(M)                                  \
  FOR_EACH_CONCRETE_INSTRUCTION_X86_64(M)

#define FOR_EACH_ABSTRACT_INSTRUCTION(M)                                \
  M(Condition, BinaryOperation)                                         \
  M(Constant, Instruction)                                              \
  M(UnaryOperation, Instruction)                                        \
  M(BinaryOperation, Instruction)                                       \
  M(Invoke, Instruction)                                                \
  M(VecOperation, Instruction)                                          \
  M(VecUnaryOperation, VecOperation)                                    \
  M(VecBinaryOperation, VecOperation)                                   \
  M(VecMemoryOperation, VecOperation)

#define FOR_EACH_INSTRUCTION(M)                                         \
  FOR_EACH_CONCRETE_INSTRUCTION(M)                                      \
  FOR_EACH_ABSTRACT_INSTRUCTION(M)

#define FORWARD_DECLARATION(type, super) class H##type;
FOR_EACH_INSTRUCTION(FORWARD_DECLARATION)
#undef FORWARD_DECLARATION

#define DECLARE_INSTRUCTION(type)                                         \
  private:                                                                \
  H##type& operator=(const H##type&) = delete;                            \
  public:                                                                 \
  const char* DebugName() const OVERRIDE { return #type; }                \
  bool InstructionTypeEquals(const HInstruction* other) const OVERRIDE {  \
    return other->Is##type();                                             \
  }                                                                       \
  HInstruction* Clone(ArenaAllocator* arena) const OVERRIDE {             \
    DCHECK(IsClonable());                                                 \
    return new (arena) H##type(*this->As##type());                        \
  }                                                                       \
  void Accept(HGraphVisitor* visitor) OVERRIDE

#define DECLARE_ABSTRACT_INSTRUCTION(type)                              \
  private:                                                              \
  H##type& operator=(const H##type&) = delete;                          \
  public:                                                               \
  bool Is##type() const { return As##type() != nullptr; }               \
  const H##type* As##type() const { return this; }                      \
  H##type* As##type() { return this; }

#define DEFAULT_COPY_CONSTRUCTOR(type)                                  \
  explicit H##type(const H##type& other) = default;

template <typename T>
class HUseListNode : public ArenaObject<kArenaAllocUseListNode>,
                     public IntrusiveForwardListNode<HUseListNode<T>> {
 public:
  // Get the instruction which has this use as one of the inputs.
  T GetUser() const { return user_; }
  // Get the position of the input record that this use corresponds to.
  size_t GetIndex() const { return index_; }
  // Set the position of the input record that this use corresponds to.
  void SetIndex(size_t index) { index_ = index; }

 private:
  HUseListNode(T user, size_t index)
      : user_(user), index_(index) {}

  T const user_;
  size_t index_;

  friend class HInstruction;

  DISALLOW_COPY_AND_ASSIGN(HUseListNode);
};

template <typename T>
using HUseList = IntrusiveForwardList<HUseListNode<T>>;

// This class is used by HEnvironment and HInstruction classes to record the
// instructions they use and pointers to the corresponding HUseListNodes kept
// by the used instructions.
template <typename T>
class HUserRecord : public ValueObject {
 public:
  HUserRecord() : instruction_(nullptr), before_use_node_() {}
  explicit HUserRecord(HInstruction* instruction) : instruction_(instruction), before_use_node_() {}

  HUserRecord(const HUserRecord<T>& old_record, typename HUseList<T>::iterator before_use_node)
      : HUserRecord(old_record.instruction_, before_use_node) {}
  HUserRecord(HInstruction* instruction, typename HUseList<T>::iterator before_use_node)
      : instruction_(instruction), before_use_node_(before_use_node) {
    DCHECK(instruction_ != nullptr);
  }

  HInstruction* GetInstruction() const { return instruction_; }
  typename HUseList<T>::iterator GetBeforeUseNode() const { return before_use_node_; }
  typename HUseList<T>::iterator GetUseNode() const { return ++GetBeforeUseNode(); }

 private:
  // Instruction used by the user.
  HInstruction* instruction_;

  // Iterator before the corresponding entry in the use list kept by 'instruction_'.
  typename HUseList<T>::iterator before_use_node_;
};

// Helper class that extracts the input instruction from HUserRecord<HInstruction*>.
// This is used for HInstruction::GetInputs() to return a container wrapper providing
// HInstruction* values even though the underlying container has HUserRecord<>s.
struct HInputExtractor {
  HInstruction* operator()(HUserRecord<HInstruction*>& record) const {
    return record.GetInstruction();
  }
  const HInstruction* operator()(const HUserRecord<HInstruction*>& record) const {
    return record.GetInstruction();
  }
};

using HInputsRef = TransformArrayRef<HUserRecord<HInstruction*>, HInputExtractor>;
using HConstInputsRef = TransformArrayRef<const HUserRecord<HInstruction*>, HInputExtractor>;

/**
 * Side-effects representation.
 *
 * For write/read dependences on fields/arrays, the dependence analysis uses
 * type disambiguation (e.g. a float field write cannot modify the value of an
 * integer field read) and the access type (e.g.  a reference array write cannot
 * modify the value of a reference field read [although it may modify the
 * reference fetch prior to reading the field, which is represented by its own
 * write/read dependence]). The analysis makes conservative points-to
 * assumptions on reference types (e.g. two same typed arrays are assumed to be
 * the same, and any reference read depends on any reference read without
 * further regard of its type).
 *
 * The internal representation uses 38-bit and is described in the table below.
 * The first line indicates the side effect, and for field/array accesses the
 * second line indicates the type of the access (in the order of the
 * DataType::Type enum).
 * The two numbered lines below indicate the bit position in the bitfield (read
 * vertically).
 *
 *   |Depends on GC|ARRAY-R  |FIELD-R  |Can trigger GC|ARRAY-W  |FIELD-W  |
 *   +-------------+---------+---------+--------------+---------+---------+
 *   |             |DFJISCBZL|DFJISCBZL|              |DFJISCBZL|DFJISCBZL|
 *   |      3      |333333322|222222221|       1      |111111110|000000000|
 *   |      7      |654321098|765432109|       8      |765432109|876543210|
 *
 * Note that, to ease the implementation, 'changes' bits are least significant
 * bits, while 'dependency' bits are most significant bits.
 */
class SideEffects : public ValueObject {
 public:
  SideEffects() : flags_(0) {}

  static SideEffects None() {
    return SideEffects(0);
  }

  static SideEffects All() {
    return SideEffects(kAllChangeBits | kAllDependOnBits);
  }

  static SideEffects AllChanges() {
    return SideEffects(kAllChangeBits);
  }

  static SideEffects AllDependencies() {
    return SideEffects(kAllDependOnBits);
  }

  static SideEffects AllExceptGCDependency() {
    return AllWritesAndReads().Union(SideEffects::CanTriggerGC());
  }

  static SideEffects AllWritesAndReads() {
    return SideEffects(kAllWrites | kAllReads);
  }

  static SideEffects AllWrites() {
    return SideEffects(kAllWrites);
  }

  static SideEffects AllReads() {
    return SideEffects(kAllReads);
  }

  static SideEffects FieldWriteOfType(DataType::Type type, bool is_volatile) {
    return is_volatile
        ? AllWritesAndReads()
        : SideEffects(TypeFlag(type, kFieldWriteOffset));
  }

  static SideEffects ArrayWriteOfType(DataType::Type type) {
    return SideEffects(TypeFlag(type, kArrayWriteOffset));
  }

  static SideEffects FieldReadOfType(DataType::Type type, bool is_volatile) {
    return is_volatile
        ? AllWritesAndReads()
        : SideEffects(TypeFlag(type, kFieldReadOffset));
  }

  static SideEffects ArrayReadOfType(DataType::Type type) {
    return SideEffects(TypeFlag(type, kArrayReadOffset));
  }

  static SideEffects CanTriggerGC() {
    return SideEffects(1ULL << kCanTriggerGCBit);
  }

  static SideEffects DependsOnGC() {
    return SideEffects(1ULL << kDependsOnGCBit);
  }

  // Combines the side-effects of this and the other.
  SideEffects Union(SideEffects other) const {
    return SideEffects(flags_ | other.flags_);
  }

  SideEffects Exclusion(SideEffects other) const {
    return SideEffects(flags_ & ~other.flags_);
  }

  void Add(SideEffects other) {
    flags_ |= other.flags_;
  }

  bool Includes(SideEffects other) const {
    return (other.flags_ & flags_) == other.flags_;
  }

  bool HasSideEffects() const {
    return (flags_ & kAllChangeBits);
  }

  bool HasDependencies() const {
    return (flags_ & kAllDependOnBits);
  }

  // Returns true if there are no side effects or dependencies.
  bool DoesNothing() const {
    return flags_ == 0;
  }

  // Returns true if something is written.
  bool DoesAnyWrite() const {
    return (flags_ & kAllWrites);
  }

  // Returns true if something is read.
  bool DoesAnyRead() const {
    return (flags_ & kAllReads);
  }

  // Returns true if potentially everything is written and read
  // (every type and every kind of access).
  bool DoesAllReadWrite() const {
    return (flags_ & (kAllWrites | kAllReads)) == (kAllWrites | kAllReads);
  }

  bool DoesAll() const {
    return flags_ == (kAllChangeBits | kAllDependOnBits);
  }

  // Returns true if `this` may read something written by `other`.
  bool MayDependOn(SideEffects other) const {
    const uint64_t depends_on_flags = (flags_ & kAllDependOnBits) >> kChangeBits;
    return (other.flags_ & depends_on_flags);
  }

  // Returns string representation of flags (for debugging only).
  // Format: |x|DFJISCBZL|DFJISCBZL|y|DFJISCBZL|DFJISCBZL|
  std::string ToString() const {
    std::string flags = "|";
    for (int s = kLastBit; s >= 0; s--) {
      bool current_bit_is_set = ((flags_ >> s) & 1) != 0;
      if ((s == kDependsOnGCBit) || (s == kCanTriggerGCBit)) {
        // This is a bit for the GC side effect.
        if (current_bit_is_set) {
          flags += "GC";
        }
        flags += "|";
      } else {
        // This is a bit for the array/field analysis.
        // The underscore character stands for the 'can trigger GC' bit.
        static const char *kDebug = "LZBCSIJFDLZBCSIJFD_LZBCSIJFDLZBCSIJFD";
        if (current_bit_is_set) {
          flags += kDebug[s];
        }
        if ((s == kFieldWriteOffset) || (s == kArrayWriteOffset) ||
            (s == kFieldReadOffset) || (s == kArrayReadOffset)) {
          flags += "|";
        }
      }
    }
    return flags;
  }

  bool Equals(const SideEffects& other) const { return flags_ == other.flags_; }

 private:
  static constexpr int kFieldArrayAnalysisBits = 9;

  static constexpr int kFieldWriteOffset = 0;
  static constexpr int kArrayWriteOffset = kFieldWriteOffset + kFieldArrayAnalysisBits;
  static constexpr int kLastBitForWrites = kArrayWriteOffset + kFieldArrayAnalysisBits - 1;
  static constexpr int kCanTriggerGCBit = kLastBitForWrites + 1;

  static constexpr int kChangeBits = kCanTriggerGCBit + 1;

  static constexpr int kFieldReadOffset = kCanTriggerGCBit + 1;
  static constexpr int kArrayReadOffset = kFieldReadOffset + kFieldArrayAnalysisBits;
  static constexpr int kLastBitForReads = kArrayReadOffset + kFieldArrayAnalysisBits - 1;
  static constexpr int kDependsOnGCBit = kLastBitForReads + 1;

  static constexpr int kLastBit = kDependsOnGCBit;
  static constexpr int kDependOnBits = kLastBit + 1 - kChangeBits;

  // Aliases.

  static_assert(kChangeBits == kDependOnBits,
                "the 'change' bits should match the 'depend on' bits.");

  static constexpr uint64_t kAllChangeBits = ((1ULL << kChangeBits) - 1);
  static constexpr uint64_t kAllDependOnBits = ((1ULL << kDependOnBits) - 1) << kChangeBits;
  static constexpr uint64_t kAllWrites =
      ((1ULL << (kLastBitForWrites + 1 - kFieldWriteOffset)) - 1) << kFieldWriteOffset;
  static constexpr uint64_t kAllReads =
      ((1ULL << (kLastBitForReads + 1 - kFieldReadOffset)) - 1) << kFieldReadOffset;

  // Translates type to bit flag. The type must correspond to a Java type.
  static uint64_t TypeFlag(DataType::Type type, int offset) {
    int shift;
    switch (type) {
      case DataType::Type::kReference: shift = 0; break;
      case DataType::Type::kBool:      shift = 1; break;
      case DataType::Type::kInt8:      shift = 2; break;
      case DataType::Type::kUint16:    shift = 3; break;
      case DataType::Type::kInt16:     shift = 4; break;
      case DataType::Type::kInt32:     shift = 5; break;
      case DataType::Type::kInt64:     shift = 6; break;
      case DataType::Type::kFloat32:   shift = 7; break;
      case DataType::Type::kFloat64:   shift = 8; break;
      default:
        LOG(FATAL) << "Unexpected data type " << type;
        UNREACHABLE();
    }
    DCHECK_LE(kFieldWriteOffset, shift);
    DCHECK_LT(shift, kArrayWriteOffset);
    return UINT64_C(1) << (shift + offset);
  }

  // Private constructor on direct flags value.
  explicit SideEffects(uint64_t flags) : flags_(flags) {}

  uint64_t flags_;
};

// A HEnvironment object contains the values of virtual registers at a given location.
class HEnvironment : public ArenaObject<kArenaAllocEnvironment> {
 public:
  ALWAYS_INLINE HEnvironment(ArenaAllocator* allocator,
                             size_t number_of_vregs,
                             ArtMethod* method,
                             uint32_t dex_pc,
                             HInstruction* holder)
     : vregs_(number_of_vregs, allocator->Adapter(kArenaAllocEnvironmentVRegs)),
       locations_(allocator->Adapter(kArenaAllocEnvironmentLocations)),
       parent_(nullptr),
       method_(method),
       dex_pc_(dex_pc),
       holder_(holder) {
  }

  ALWAYS_INLINE HEnvironment(ArenaAllocator* allocator,
                             const HEnvironment& to_copy,
                             HInstruction* holder)
      : HEnvironment(allocator,
                     to_copy.Size(),
                     to_copy.GetMethod(),
                     to_copy.GetDexPc(),
                     holder) {}

  void AllocateLocations() {
    DCHECK(locations_.empty());
    locations_.resize(vregs_.size());
  }

  void SetAndCopyParentChain(ArenaAllocator* allocator, HEnvironment* parent) {
    if (parent_ != nullptr) {
      parent_->SetAndCopyParentChain(allocator, parent);
    } else {
      parent_ = new (allocator) HEnvironment(allocator, *parent, holder_);
      parent_->CopyFrom(parent);
      if (parent->GetParent() != nullptr) {
        parent_->SetAndCopyParentChain(allocator, parent->GetParent());
      }
    }
  }

  void CopyFrom(ArrayRef<HInstruction* const> locals);
  void CopyFrom(HEnvironment* environment);

  // Copy from `env`. If it's a loop phi for `loop_header`, copy the first
  // input to the loop phi instead. This is for inserting instructions that
  // require an environment (like HDeoptimization) in the loop pre-header.
  void CopyFromWithLoopPhiAdjustment(HEnvironment* env, HBasicBlock* loop_header);

  void SetRawEnvAt(size_t index, HInstruction* instruction) {
    vregs_[index] = HUserRecord<HEnvironment*>(instruction);
  }

  HInstruction* GetInstructionAt(size_t index) const {
    return vregs_[index].GetInstruction();
  }

  void RemoveAsUserOfInput(size_t index) const;

  size_t Size() const { return vregs_.size(); }

  HEnvironment* GetParent() const { return parent_; }

  void SetLocationAt(size_t index, Location location) {
    locations_[index] = location;
  }

  Location GetLocationAt(size_t index) const {
    return locations_[index];
  }

  uint32_t GetDexPc() const {
    return dex_pc_;
  }

  ArtMethod* GetMethod() const {
    return method_;
  }

  HInstruction* GetHolder() const {
    return holder_;
  }


  bool IsFromInlinedInvoke() const {
    return GetParent() != nullptr;
  }

 private:
  ArenaVector<HUserRecord<HEnvironment*>> vregs_;
  ArenaVector<Location> locations_;
  HEnvironment* parent_;
  ArtMethod* method_;
  const uint32_t dex_pc_;

  // The instruction that holds this environment.
  HInstruction* const holder_;

  friend class HInstruction;

  DISALLOW_COPY_AND_ASSIGN(HEnvironment);
};

class HInstruction : public ArenaObject<kArenaAllocInstruction> {
 public:
#define DECLARE_KIND(type, super) k##type,
  enum InstructionKind {
    FOR_EACH_INSTRUCTION(DECLARE_KIND)
    kLastInstructionKind
  };
#undef DECLARE_KIND

  HInstruction(InstructionKind kind, SideEffects side_effects, uint32_t dex_pc)
      : previous_(nullptr),
        next_(nullptr),
        block_(nullptr),
        dex_pc_(dex_pc),
        id_(-1),
        ssa_index_(-1),
        packed_fields_(0u),
        environment_(nullptr),
        locations_(nullptr),
        live_interval_(nullptr),
        lifetime_position_(kNoLifetime),
        side_effects_(side_effects),
        reference_type_handle_(ReferenceTypeInfo::CreateInvalid().GetTypeHandle()) {
    SetPackedField<InstructionKindField>(kind);
    SetPackedFlag<kFlagReferenceTypeIsExact>(ReferenceTypeInfo::CreateInvalid().IsExact());
  }

  virtual ~HInstruction() {}


  HInstruction* GetNext() const { return next_; }
  HInstruction* GetPrevious() const { return previous_; }

  HInstruction* GetNextDisregardingMoves() const;
  HInstruction* GetPreviousDisregardingMoves() const;

  HBasicBlock* GetBlock() const { return block_; }
  ArenaAllocator* GetAllocator() const { return block_->GetGraph()->GetAllocator(); }
  void SetBlock(HBasicBlock* block) { block_ = block; }
  bool IsInBlock() const { return block_ != nullptr; }
  bool IsInLoop() const { return block_->IsInLoop(); }
  bool IsLoopHeaderPhi() const { return IsPhi() && block_->IsLoopHeader(); }
  bool IsIrreducibleLoopHeaderPhi() const {
    return IsLoopHeaderPhi() && GetBlock()->GetLoopInformation()->IsIrreducible();
  }

  virtual ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() = 0;

  ArrayRef<const HUserRecord<HInstruction*>> GetInputRecords() const {
    // One virtual method is enough, just const_cast<> and then re-add the const.
    return ArrayRef<const HUserRecord<HInstruction*>>(
        const_cast<HInstruction*>(this)->GetInputRecords());
  }

  HInputsRef GetInputs() {
    return MakeTransformArrayRef(GetInputRecords(), HInputExtractor());
  }

  HConstInputsRef GetInputs() const {
    return MakeTransformArrayRef(GetInputRecords(), HInputExtractor());
  }

  size_t InputCount() const { return GetInputRecords().size(); }
  HInstruction* InputAt(size_t i) const { return InputRecordAt(i).GetInstruction(); }

  bool HasInput(HInstruction* input) const {
    for (const HInstruction* i : GetInputs()) {
      if (i == input) {
        return true;
      }
    }
    return false;
  }

  void SetRawInputAt(size_t index, HInstruction* input) {
    SetRawInputRecordAt(index, HUserRecord<HInstruction*>(input));
  }

  virtual void Accept(HGraphVisitor* visitor) = 0;
  virtual const char* DebugName() const = 0;

  virtual DataType::Type GetType() const { return DataType::Type::kVoid; }

  virtual bool NeedsEnvironment() const { return false; }

  uint32_t GetDexPc() const { return dex_pc_; }

  virtual bool IsControlFlow() const { return false; }

  // Can the instruction throw?
  // TODO: We should rename to CanVisiblyThrow, as some instructions (like HNewInstance),
  // could throw OOME, but it is still OK to remove them if they are unused.
  virtual bool CanThrow() const { return false; }

  // Does the instruction always throw an exception unconditionally?
  virtual bool AlwaysThrows() const { return false; }

  bool CanThrowIntoCatchBlock() const { return CanThrow() && block_->IsTryBlock(); }

  bool HasSideEffects() const { return side_effects_.HasSideEffects(); }
  bool DoesAnyWrite() const { return side_effects_.DoesAnyWrite(); }

  // Does not apply for all instructions, but having this at top level greatly
  // simplifies the null check elimination.
  // TODO: Consider merging can_be_null into ReferenceTypeInfo.
  virtual bool CanBeNull() const {
    DCHECK_EQ(GetType(), DataType::Type::kReference) << "CanBeNull only applies to reference types";
    return true;
  }

  virtual bool CanDoImplicitNullCheckOn(HInstruction* obj ATTRIBUTE_UNUSED) const {
    return false;
  }

  virtual bool IsActualObject() const {
    return GetType() == DataType::Type::kReference;
  }

  void SetReferenceTypeInfo(ReferenceTypeInfo rti);

  ReferenceTypeInfo GetReferenceTypeInfo() const {
    DCHECK_EQ(GetType(), DataType::Type::kReference);
    return ReferenceTypeInfo::CreateUnchecked(reference_type_handle_,
                                              GetPackedFlag<kFlagReferenceTypeIsExact>());
  }

  void AddUseAt(HInstruction* user, size_t index) {
    DCHECK(user != nullptr);
    // Note: fixup_end remains valid across push_front().
    auto fixup_end = uses_.empty() ? uses_.begin() : ++uses_.begin();
    HUseListNode<HInstruction*>* new_node =
        new (GetBlock()->GetGraph()->GetAllocator()) HUseListNode<HInstruction*>(user, index);
    uses_.push_front(*new_node);
    FixUpUserRecordsAfterUseInsertion(fixup_end);
  }

  void AddEnvUseAt(HEnvironment* user, size_t index) {
    DCHECK(user != nullptr);
    // Note: env_fixup_end remains valid across push_front().
    auto env_fixup_end = env_uses_.empty() ? env_uses_.begin() : ++env_uses_.begin();
    HUseListNode<HEnvironment*>* new_node =
        new (GetBlock()->GetGraph()->GetAllocator()) HUseListNode<HEnvironment*>(user, index);
    env_uses_.push_front(*new_node);
    FixUpUserRecordsAfterEnvUseInsertion(env_fixup_end);
  }

  void RemoveAsUserOfInput(size_t input) {
    HUserRecord<HInstruction*> input_use = InputRecordAt(input);
    HUseList<HInstruction*>::iterator before_use_node = input_use.GetBeforeUseNode();
    input_use.GetInstruction()->uses_.erase_after(before_use_node);
    input_use.GetInstruction()->FixUpUserRecordsAfterUseRemoval(before_use_node);
  }

  void RemoveAsUserOfAllInputs() {
    for (const HUserRecord<HInstruction*>& input_use : GetInputRecords()) {
      HUseList<HInstruction*>::iterator before_use_node = input_use.GetBeforeUseNode();
      input_use.GetInstruction()->uses_.erase_after(before_use_node);
      input_use.GetInstruction()->FixUpUserRecordsAfterUseRemoval(before_use_node);
    }
  }

  const HUseList<HInstruction*>& GetUses() const { return uses_; }
  const HUseList<HEnvironment*>& GetEnvUses() const { return env_uses_; }

  bool HasUses() const { return !uses_.empty() || !env_uses_.empty(); }
  bool HasEnvironmentUses() const { return !env_uses_.empty(); }
  bool HasNonEnvironmentUses() const { return !uses_.empty(); }
  bool HasOnlyOneNonEnvironmentUse() const {
    return !HasEnvironmentUses() && GetUses().HasExactlyOneElement();
  }

  bool IsRemovable() const {
    return
        !DoesAnyWrite() &&
        !CanThrow() &&
        !IsSuspendCheck() &&
        !IsControlFlow() &&
        !IsNativeDebugInfo() &&
        !IsParameterValue() &&
        // If we added an explicit barrier then we should keep it.
        !IsMemoryBarrier() &&
        !IsConstructorFence();
  }

  bool IsDeadAndRemovable() const {
    return IsRemovable() && !HasUses();
  }

  // Does this instruction strictly dominate `other_instruction`?
  // Returns false if this instruction and `other_instruction` are the same.
  // Aborts if this instruction and `other_instruction` are both phis.
  bool StrictlyDominates(HInstruction* other_instruction) const;

  int GetId() const { return id_; }
  void SetId(int id) { id_ = id; }

  int GetSsaIndex() const { return ssa_index_; }
  void SetSsaIndex(int ssa_index) { ssa_index_ = ssa_index; }
  bool HasSsaIndex() const { return ssa_index_ != -1; }

  bool HasEnvironment() const { return environment_ != nullptr; }
  HEnvironment* GetEnvironment() const { return environment_; }
  // Set the `environment_` field. Raw because this method does not
  // update the uses lists.
  void SetRawEnvironment(HEnvironment* environment) {
    DCHECK(environment_ == nullptr);
    DCHECK_EQ(environment->GetHolder(), this);
    environment_ = environment;
  }

  void InsertRawEnvironment(HEnvironment* environment) {
    DCHECK(environment_ != nullptr);
    DCHECK_EQ(environment->GetHolder(), this);
    DCHECK(environment->GetParent() == nullptr);
    environment->parent_ = environment_;
    environment_ = environment;
  }

  void RemoveEnvironment();

  // Set the environment of this instruction, copying it from `environment`. While
  // copying, the uses lists are being updated.
  void CopyEnvironmentFrom(HEnvironment* environment) {
    DCHECK(environment_ == nullptr);
    ArenaAllocator* allocator = GetBlock()->GetGraph()->GetAllocator();
    environment_ = new (allocator) HEnvironment(allocator, *environment, this);
    environment_->CopyFrom(environment);
    if (environment->GetParent() != nullptr) {
      environment_->SetAndCopyParentChain(allocator, environment->GetParent());
    }
  }

  void CopyEnvironmentFromWithLoopPhiAdjustment(HEnvironment* environment,
                                                HBasicBlock* block) {
    DCHECK(environment_ == nullptr);
    ArenaAllocator* allocator = GetBlock()->GetGraph()->GetAllocator();
    environment_ = new (allocator) HEnvironment(allocator, *environment, this);
    environment_->CopyFromWithLoopPhiAdjustment(environment, block);
    if (environment->GetParent() != nullptr) {
      environment_->SetAndCopyParentChain(allocator, environment->GetParent());
    }
  }

  // Returns the number of entries in the environment. Typically, that is the
  // number of dex registers in a method. It could be more in case of inlining.
  size_t EnvironmentSize() const;

  LocationSummary* GetLocations() const { return locations_; }
  void SetLocations(LocationSummary* locations) { locations_ = locations; }

  void ReplaceWith(HInstruction* instruction);
  void ReplaceUsesDominatedBy(HInstruction* dominator, HInstruction* replacement);
  void ReplaceInput(HInstruction* replacement, size_t index);

  // This is almost the same as doing `ReplaceWith()`. But in this helper, the
  // uses of this instruction by `other` are *not* updated.
  void ReplaceWithExceptInReplacementAtIndex(HInstruction* other, size_t use_index) {
    ReplaceWith(other);
    other->ReplaceInput(this, use_index);
  }

  // Move `this` instruction before `cursor`
  void MoveBefore(HInstruction* cursor, bool do_checks = true);

  // Move `this` before its first user and out of any loops. If there is no
  // out-of-loop user that dominates all other users, move the instruction
  // to the end of the out-of-loop common dominator of the user's blocks.
  //
  // This can be used only on non-throwing instructions with no side effects that
  // have at least one use but no environment uses.
  void MoveBeforeFirstUserAndOutOfLoops();

#define INSTRUCTION_TYPE_CHECK(type, super)                                    \
  bool Is##type() const;                                                       \
  const H##type* As##type() const;                                             \
  H##type* As##type();

  FOR_EACH_CONCRETE_INSTRUCTION(INSTRUCTION_TYPE_CHECK)
#undef INSTRUCTION_TYPE_CHECK

#define INSTRUCTION_TYPE_CHECK(type, super)                                    \
  bool Is##type() const { return (As##type() != nullptr); }                    \
  virtual const H##type* As##type() const { return nullptr; }                  \
  virtual H##type* As##type() { return nullptr; }
  FOR_EACH_ABSTRACT_INSTRUCTION(INSTRUCTION_TYPE_CHECK)
#undef INSTRUCTION_TYPE_CHECK

  // Return a clone of the instruction if it is clonable (shallow copy by default, custom copy
  // if a custom copy-constructor is provided for a particular type). If IsClonable() is false for
  // the instruction then the behaviour of this function is undefined.
  //
  // Note: It is semantically valid to create a clone of the instruction only until
  // prepare_for_register_allocator phase as lifetime, intervals and codegen info are not
  // copied.
  //
  // Note: HEnvironment and some other fields are not copied and are set to default values, see
  // 'explicit HInstruction(const HInstruction& other)' for details.
  virtual HInstruction* Clone(ArenaAllocator* arena ATTRIBUTE_UNUSED) const {
    LOG(FATAL) << "Cloning is not implemented for the instruction " <<
                  DebugName() << " " << GetId();
    UNREACHABLE();
  }

  // Return whether instruction can be cloned (copied).
  virtual bool IsClonable() const { return false; }

  // Returns whether the instruction can be moved within the graph.
  // TODO: this method is used by LICM and GVN with possibly different
  //       meanings? split and rename?
  virtual bool CanBeMoved() const { return false; }

  // Returns whether the two instructions are of the same kind.
  virtual bool InstructionTypeEquals(const HInstruction* other ATTRIBUTE_UNUSED) const {
    return false;
  }

  // Returns whether any data encoded in the two instructions is equal.
  // This method does not look at the inputs. Both instructions must be
  // of the same type, otherwise the method has undefined behavior.
  virtual bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const {
    return false;
  }

  // Returns whether two instructions are equal, that is:
  // 1) They have the same type and contain the same data (InstructionDataEquals).
  // 2) Their inputs are identical.
  bool Equals(const HInstruction* other) const;

  // TODO: Remove this indirection when the [[pure]] attribute proposal (n3744)
  // is adopted and implemented by our C++ compiler(s). Fow now, we need to hide
  // the virtual function because the __attribute__((__pure__)) doesn't really
  // apply the strong requirement for virtual functions, preventing optimizations.
  InstructionKind GetKind() const { return GetPackedField<InstructionKindField>(); }

  virtual size_t ComputeHashCode() const {
    size_t result = GetKind();
    for (const HInstruction* input : GetInputs()) {
      result = (result * 31) + input->GetId();
    }
    return result;
  }

  SideEffects GetSideEffects() const { return side_effects_; }
  void SetSideEffects(SideEffects other) { side_effects_ = other; }
  void AddSideEffects(SideEffects other) { side_effects_.Add(other); }

  size_t GetLifetimePosition() const { return lifetime_position_; }
  void SetLifetimePosition(size_t position) { lifetime_position_ = position; }
  LiveInterval* GetLiveInterval() const { return live_interval_; }
  void SetLiveInterval(LiveInterval* interval) { live_interval_ = interval; }
  bool HasLiveInterval() const { return live_interval_ != nullptr; }

  bool IsSuspendCheckEntry() const { return IsSuspendCheck() && GetBlock()->IsEntryBlock(); }

  // Returns whether the code generation of the instruction will require to have access
  // to the current method. Such instructions are:
  // (1): Instructions that require an environment, as calling the runtime requires
  //      to walk the stack and have the current method stored at a specific stack address.
  // (2): HCurrentMethod, potentially used by HInvokeStaticOrDirect, HLoadString, or HLoadClass
  //      to access the dex cache.
  bool NeedsCurrentMethod() const {
    return NeedsEnvironment() || IsCurrentMethod();
  }

  // Returns whether the code generation of the instruction will require to have access
  // to the dex cache of the current method's declaring class via the current method.
  virtual bool NeedsDexCacheOfDeclaringClass() const { return false; }

  // Does this instruction have any use in an environment before
  // control flow hits 'other'?
  bool HasAnyEnvironmentUseBefore(HInstruction* other);

  // Remove all references to environment uses of this instruction.
  // The caller must ensure that this is safe to do.
  void RemoveEnvironmentUsers();

  bool IsEmittedAtUseSite() const { return GetPackedFlag<kFlagEmittedAtUseSite>(); }
  void MarkEmittedAtUseSite() { SetPackedFlag<kFlagEmittedAtUseSite>(true); }

 protected:
  // If set, the machine code for this instruction is assumed to be generated by
  // its users. Used by liveness analysis to compute use positions accordingly.
  static constexpr size_t kFlagEmittedAtUseSite = 0u;
  static constexpr size_t kFlagReferenceTypeIsExact = kFlagEmittedAtUseSite + 1;
  static constexpr size_t kFieldInstructionKind = kFlagReferenceTypeIsExact + 1;
  static constexpr size_t kFieldInstructionKindSize =
      MinimumBitsToStore(static_cast<size_t>(InstructionKind::kLastInstructionKind - 1));
  static constexpr size_t kNumberOfGenericPackedBits =
      kFieldInstructionKind + kFieldInstructionKindSize;
  static constexpr size_t kMaxNumberOfPackedBits = sizeof(uint32_t) * kBitsPerByte;

  static_assert(kNumberOfGenericPackedBits <= kMaxNumberOfPackedBits,
                "Too many generic packed fields");

  const HUserRecord<HInstruction*> InputRecordAt(size_t i) const {
    return GetInputRecords()[i];
  }

  void SetRawInputRecordAt(size_t index, const HUserRecord<HInstruction*>& input) {
    ArrayRef<HUserRecord<HInstruction*>> input_records = GetInputRecords();
    input_records[index] = input;
  }

  uint32_t GetPackedFields() const {
    return packed_fields_;
  }

  template <size_t flag>
  bool GetPackedFlag() const {
    return (packed_fields_ & (1u << flag)) != 0u;
  }

  template <size_t flag>
  void SetPackedFlag(bool value = true) {
    packed_fields_ = (packed_fields_ & ~(1u << flag)) | ((value ? 1u : 0u) << flag);
  }

  template <typename BitFieldType>
  typename BitFieldType::value_type GetPackedField() const {
    return BitFieldType::Decode(packed_fields_);
  }

  template <typename BitFieldType>
  void SetPackedField(typename BitFieldType::value_type value) {
    DCHECK(IsUint<BitFieldType::size>(static_cast<uintptr_t>(value)));
    packed_fields_ = BitFieldType::Update(value, packed_fields_);
  }

  // Copy construction for the instruction (used for Clone function).
  //
  // Fields (e.g. lifetime, intervals and codegen info) associated with phases starting from
  // prepare_for_register_allocator are not copied (set to default values).
  //
  // Copy constructors must be provided for every HInstruction type; default copy constructor is
  // fine for most of them. However for some of the instructions a custom copy constructor must be
  // specified (when instruction has non-trivially copyable fields and must have a special behaviour
  // for copying them).
  explicit HInstruction(const HInstruction& other)
      : previous_(nullptr),
        next_(nullptr),
        block_(nullptr),
        dex_pc_(other.dex_pc_),
        id_(-1),
        ssa_index_(-1),
        packed_fields_(other.packed_fields_),
        environment_(nullptr),
        locations_(nullptr),
        live_interval_(nullptr),
        lifetime_position_(kNoLifetime),
        side_effects_(other.side_effects_),
        reference_type_handle_(other.reference_type_handle_) {
  }

 private:
  using InstructionKindField =
     BitField<InstructionKind, kFieldInstructionKind, kFieldInstructionKindSize>;

  void FixUpUserRecordsAfterUseInsertion(HUseList<HInstruction*>::iterator fixup_end) {
    auto before_use_node = uses_.before_begin();
    for (auto use_node = uses_.begin(); use_node != fixup_end; ++use_node) {
      HInstruction* user = use_node->GetUser();
      size_t input_index = use_node->GetIndex();
      user->SetRawInputRecordAt(input_index, HUserRecord<HInstruction*>(this, before_use_node));
      before_use_node = use_node;
    }
  }

  void FixUpUserRecordsAfterUseRemoval(HUseList<HInstruction*>::iterator before_use_node) {
    auto next = ++HUseList<HInstruction*>::iterator(before_use_node);
    if (next != uses_.end()) {
      HInstruction* next_user = next->GetUser();
      size_t next_index = next->GetIndex();
      DCHECK(next_user->InputRecordAt(next_index).GetInstruction() == this);
      next_user->SetRawInputRecordAt(next_index, HUserRecord<HInstruction*>(this, before_use_node));
    }
  }

  void FixUpUserRecordsAfterEnvUseInsertion(HUseList<HEnvironment*>::iterator env_fixup_end) {
    auto before_env_use_node = env_uses_.before_begin();
    for (auto env_use_node = env_uses_.begin(); env_use_node != env_fixup_end; ++env_use_node) {
      HEnvironment* user = env_use_node->GetUser();
      size_t input_index = env_use_node->GetIndex();
      user->vregs_[input_index] = HUserRecord<HEnvironment*>(this, before_env_use_node);
      before_env_use_node = env_use_node;
    }
  }

  void FixUpUserRecordsAfterEnvUseRemoval(HUseList<HEnvironment*>::iterator before_env_use_node) {
    auto next = ++HUseList<HEnvironment*>::iterator(before_env_use_node);
    if (next != env_uses_.end()) {
      HEnvironment* next_user = next->GetUser();
      size_t next_index = next->GetIndex();
      DCHECK(next_user->vregs_[next_index].GetInstruction() == this);
      next_user->vregs_[next_index] = HUserRecord<HEnvironment*>(this, before_env_use_node);
    }
  }

  HInstruction* previous_;
  HInstruction* next_;
  HBasicBlock* block_;
  const uint32_t dex_pc_;

  // An instruction gets an id when it is added to the graph.
  // It reflects creation order. A negative id means the instruction
  // has not been added to the graph.
  int id_;

  // When doing liveness analysis, instructions that have uses get an SSA index.
  int ssa_index_;

  // Packed fields.
  uint32_t packed_fields_;

  // List of instructions that have this instruction as input.
  HUseList<HInstruction*> uses_;

  // List of environments that contain this instruction.
  HUseList<HEnvironment*> env_uses_;

  // The environment associated with this instruction. Not null if the instruction
  // might jump out of the method.
  HEnvironment* environment_;

  // Set by the code generator.
  LocationSummary* locations_;

  // Set by the liveness analysis.
  LiveInterval* live_interval_;

  // Set by the liveness analysis, this is the position in a linear
  // order of blocks where this instruction's live interval start.
  size_t lifetime_position_;

  SideEffects side_effects_;

  // The reference handle part of the reference type info.
  // The IsExact() flag is stored in packed fields.
  // TODO: for primitive types this should be marked as invalid.
  ReferenceTypeInfo::TypeHandle reference_type_handle_;

  friend class GraphChecker;
  friend class HBasicBlock;
  friend class HEnvironment;
  friend class HGraph;
  friend class HInstructionList;
};
std::ostream& operator<<(std::ostream& os, const HInstruction::InstructionKind& rhs);

// Iterates over the instructions, while preserving the next instruction
// in case the current instruction gets removed from the list by the user
// of this iterator.
class HInstructionIterator : public ValueObject {
 public:
  explicit HInstructionIterator(const HInstructionList& instructions)
      : instruction_(instructions.first_instruction_) {
    next_ = Done() ? nullptr : instruction_->GetNext();
  }

  bool Done() const { return instruction_ == nullptr; }
  HInstruction* Current() const { return instruction_; }
  void Advance() {
    instruction_ = next_;
    next_ = Done() ? nullptr : instruction_->GetNext();
  }

 private:
  HInstruction* instruction_;
  HInstruction* next_;

  DISALLOW_COPY_AND_ASSIGN(HInstructionIterator);
};

// Iterates over the instructions without saving the next instruction,
// therefore handling changes in the graph potentially made by the user
// of this iterator.
class HInstructionIteratorHandleChanges : public ValueObject {
 public:
  explicit HInstructionIteratorHandleChanges(const HInstructionList& instructions)
      : instruction_(instructions.first_instruction_) {
  }

  bool Done() const { return instruction_ == nullptr; }
  HInstruction* Current() const { return instruction_; }
  void Advance() {
    instruction_ = instruction_->GetNext();
  }

 private:
  HInstruction* instruction_;

  DISALLOW_COPY_AND_ASSIGN(HInstructionIteratorHandleChanges);
};


class HBackwardInstructionIterator : public ValueObject {
 public:
  explicit HBackwardInstructionIterator(const HInstructionList& instructions)
      : instruction_(instructions.last_instruction_) {
    next_ = Done() ? nullptr : instruction_->GetPrevious();
  }

  bool Done() const { return instruction_ == nullptr; }
  HInstruction* Current() const { return instruction_; }
  void Advance() {
    instruction_ = next_;
    next_ = Done() ? nullptr : instruction_->GetPrevious();
  }

 private:
  HInstruction* instruction_;
  HInstruction* next_;

  DISALLOW_COPY_AND_ASSIGN(HBackwardInstructionIterator);
};

class HVariableInputSizeInstruction : public HInstruction {
 public:
  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE {
    return ArrayRef<HUserRecord<HInstruction*>>(inputs_);
  }

  void AddInput(HInstruction* input);
  void InsertInputAt(size_t index, HInstruction* input);
  void RemoveInputAt(size_t index);

  // Removes all the inputs.
  // Also removes this instructions from each input's use list
  // (for non-environment uses only).
  void RemoveAllInputs();

 protected:
  HVariableInputSizeInstruction(InstructionKind inst_kind,
                                SideEffects side_effects,
                                uint32_t dex_pc,
                                ArenaAllocator* allocator,
                                size_t number_of_inputs,
                                ArenaAllocKind kind)
      : HInstruction(inst_kind, side_effects, dex_pc),
        inputs_(number_of_inputs, allocator->Adapter(kind)) {}

  DEFAULT_COPY_CONSTRUCTOR(VariableInputSizeInstruction);

  ArenaVector<HUserRecord<HInstruction*>> inputs_;
};

template<size_t N>
class HTemplateInstruction: public HInstruction {
 public:
  HTemplateInstruction<N>(InstructionKind kind, SideEffects side_effects, uint32_t dex_pc)
      : HInstruction(kind, side_effects, dex_pc), inputs_() {}
  virtual ~HTemplateInstruction() {}

  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE FINAL {
    return ArrayRef<HUserRecord<HInstruction*>>(inputs_);
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(TemplateInstruction<N>);

 private:
  std::array<HUserRecord<HInstruction*>, N> inputs_;

  friend class SsaBuilder;
};

// HTemplateInstruction specialization for N=0.
template<>
class HTemplateInstruction<0>: public HInstruction {
 public:
  explicit HTemplateInstruction<0>(InstructionKind kind, SideEffects side_effects, uint32_t dex_pc)
      : HInstruction(kind, side_effects, dex_pc) {}

  virtual ~HTemplateInstruction() {}

  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE FINAL {
    return ArrayRef<HUserRecord<HInstruction*>>();
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(TemplateInstruction<0>);

 private:
  friend class SsaBuilder;
};

template<intptr_t N>
class HExpression : public HTemplateInstruction<N> {
 public:
  using HInstruction::InstructionKind;
  HExpression<N>(InstructionKind kind,
                 DataType::Type type,
                 SideEffects side_effects,
                 uint32_t dex_pc)
      : HTemplateInstruction<N>(kind, side_effects, dex_pc) {
    this->template SetPackedField<TypeField>(type);
  }
  virtual ~HExpression() {}

  DataType::Type GetType() const OVERRIDE {
    return TypeField::Decode(this->GetPackedFields());
  }

 protected:
  static constexpr size_t kFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kNumberOfExpressionPackedBits = kFieldType + kFieldTypeSize;
  static_assert(kNumberOfExpressionPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using TypeField = BitField<DataType::Type, kFieldType, kFieldTypeSize>;
  DEFAULT_COPY_CONSTRUCTOR(Expression<N>);
};

// Represents dex's RETURN_VOID opcode. A HReturnVoid is a control flow
// instruction that branches to the exit block.
class HReturnVoid FINAL : public HTemplateInstruction<0> {
 public:
  explicit HReturnVoid(uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kReturnVoid, SideEffects::None(), dex_pc) {
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(ReturnVoid);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ReturnVoid);
};

// Represents dex's RETURN opcodes. A HReturn is a control flow
// instruction that branches to the exit block.
class HReturn FINAL : public HTemplateInstruction<1> {
 public:
  explicit HReturn(HInstruction* value, uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kReturn, SideEffects::None(), dex_pc) {
    SetRawInputAt(0, value);
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(Return);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Return);
};

class HPhi FINAL : public HVariableInputSizeInstruction {
 public:
  HPhi(ArenaAllocator* allocator,
       uint32_t reg_number,
       size_t number_of_inputs,
       DataType::Type type,
       uint32_t dex_pc = kNoDexPc)
      : HVariableInputSizeInstruction(
            kPhi,
            SideEffects::None(),
            dex_pc,
            allocator,
            number_of_inputs,
            kArenaAllocPhiInputs),
        reg_number_(reg_number) {
    SetPackedField<TypeField>(ToPhiType(type));
    DCHECK_NE(GetType(), DataType::Type::kVoid);
    // Phis are constructed live and marked dead if conflicting or unused.
    // Individual steps of SsaBuilder should assume that if a phi has been
    // marked dead, it can be ignored and will be removed by SsaPhiElimination.
    SetPackedFlag<kFlagIsLive>(true);
    SetPackedFlag<kFlagCanBeNull>(true);
  }

  bool IsClonable() const OVERRIDE { return true; }

  // Returns a type equivalent to the given `type`, but that a `HPhi` can hold.
  static DataType::Type ToPhiType(DataType::Type type) {
    return DataType::Kind(type);
  }

  bool IsCatchPhi() const { return GetBlock()->IsCatchBlock(); }

  DataType::Type GetType() const OVERRIDE { return GetPackedField<TypeField>(); }
  void SetType(DataType::Type new_type) {
    // Make sure that only valid type changes occur. The following are allowed:
    //  (1) int  -> float/ref (primitive type propagation),
    //  (2) long -> double (primitive type propagation).
    DCHECK(GetType() == new_type ||
           (GetType() == DataType::Type::kInt32 && new_type == DataType::Type::kFloat32) ||
           (GetType() == DataType::Type::kInt32 && new_type == DataType::Type::kReference) ||
           (GetType() == DataType::Type::kInt64 && new_type == DataType::Type::kFloat64));
    SetPackedField<TypeField>(new_type);
  }

  bool CanBeNull() const OVERRIDE { return GetPackedFlag<kFlagCanBeNull>(); }
  void SetCanBeNull(bool can_be_null) { SetPackedFlag<kFlagCanBeNull>(can_be_null); }

  uint32_t GetRegNumber() const { return reg_number_; }

  void SetDead() { SetPackedFlag<kFlagIsLive>(false); }
  void SetLive() { SetPackedFlag<kFlagIsLive>(true); }
  bool IsDead() const { return !IsLive(); }
  bool IsLive() const { return GetPackedFlag<kFlagIsLive>(); }

  bool IsVRegEquivalentOf(const HInstruction* other) const {
    return other != nullptr
        && other->IsPhi()
        && other->AsPhi()->GetBlock() == GetBlock()
        && other->AsPhi()->GetRegNumber() == GetRegNumber();
  }

  bool HasEquivalentPhi() const {
    if (GetPrevious() != nullptr && GetPrevious()->AsPhi()->GetRegNumber() == GetRegNumber()) {
      return true;
    }
    if (GetNext() != nullptr && GetNext()->AsPhi()->GetRegNumber() == GetRegNumber()) {
      return true;
    }
    return false;
  }

  // Returns the next equivalent phi (starting from the current one) or null if there is none.
  // An equivalent phi is a phi having the same dex register and type.
  // It assumes that phis with the same dex register are adjacent.
  HPhi* GetNextEquivalentPhiWithSameType() {
    HInstruction* next = GetNext();
    while (next != nullptr && next->AsPhi()->GetRegNumber() == reg_number_) {
      if (next->GetType() == GetType()) {
        return next->AsPhi();
      }
      next = next->GetNext();
    }
    return nullptr;
  }

  DECLARE_INSTRUCTION(Phi);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Phi);

 private:
  static constexpr size_t kFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kFlagIsLive = kFieldType + kFieldTypeSize;
  static constexpr size_t kFlagCanBeNull = kFlagIsLive + 1;
  static constexpr size_t kNumberOfPhiPackedBits = kFlagCanBeNull + 1;
  static_assert(kNumberOfPhiPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using TypeField = BitField<DataType::Type, kFieldType, kFieldTypeSize>;

  const uint32_t reg_number_;
};

// The exit instruction is the only instruction of the exit block.
// Instructions aborting the method (HThrow and HReturn) must branch to the
// exit block.
class HExit FINAL : public HTemplateInstruction<0> {
 public:
  explicit HExit(uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kExit, SideEffects::None(), dex_pc) {
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(Exit);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Exit);
};

// Jumps from one block to another.
class HGoto FINAL : public HTemplateInstruction<0> {
 public:
  explicit HGoto(uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kGoto, SideEffects::None(), dex_pc) {
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool IsControlFlow() const OVERRIDE { return true; }

  HBasicBlock* GetSuccessor() const {
    return GetBlock()->GetSingleSuccessor();
  }

  DECLARE_INSTRUCTION(Goto);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Goto);
};

class HConstant : public HExpression<0> {
 public:
  explicit HConstant(InstructionKind kind, DataType::Type type, uint32_t dex_pc = kNoDexPc)
      : HExpression(kind, type, SideEffects::None(), dex_pc) {
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  // Is this constant -1 in the arithmetic sense?
  virtual bool IsMinusOne() const { return false; }
  // Is this constant 0 in the arithmetic sense?
  virtual bool IsArithmeticZero() const { return false; }
  // Is this constant a 0-bit pattern?
  virtual bool IsZeroBitPattern() const { return false; }
  // Is this constant 1 in the arithmetic sense?
  virtual bool IsOne() const { return false; }

  virtual uint64_t GetValueAsUint64() const = 0;

  DECLARE_ABSTRACT_INSTRUCTION(Constant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Constant);
};

class HNullConstant FINAL : public HConstant {
 public:
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  uint64_t GetValueAsUint64() const OVERRIDE { return 0; }

  size_t ComputeHashCode() const OVERRIDE { return 0; }

  // The null constant representation is a 0-bit pattern.
  virtual bool IsZeroBitPattern() const { return true; }

  DECLARE_INSTRUCTION(NullConstant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NullConstant);

 private:
  explicit HNullConstant(uint32_t dex_pc = kNoDexPc)
      : HConstant(kNullConstant, DataType::Type::kReference, dex_pc) {
  }

  friend class HGraph;
};

// Constants of the type int. Those can be from Dex instructions, or
// synthesized (for example with the if-eqz instruction).
class HIntConstant FINAL : public HConstant {
 public:
  int32_t GetValue() const { return value_; }

  uint64_t GetValueAsUint64() const OVERRIDE {
    return static_cast<uint64_t>(static_cast<uint32_t>(value_));
  }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsIntConstant()) << other->DebugName();
    return other->AsIntConstant()->value_ == value_;
  }

  size_t ComputeHashCode() const OVERRIDE { return GetValue(); }

  bool IsMinusOne() const OVERRIDE { return GetValue() == -1; }
  bool IsArithmeticZero() const OVERRIDE { return GetValue() == 0; }
  bool IsZeroBitPattern() const OVERRIDE { return GetValue() == 0; }
  bool IsOne() const OVERRIDE { return GetValue() == 1; }

  // Integer constants are used to encode Boolean values as well,
  // where 1 means true and 0 means false.
  bool IsTrue() const { return GetValue() == 1; }
  bool IsFalse() const { return GetValue() == 0; }

  DECLARE_INSTRUCTION(IntConstant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(IntConstant);

 private:
  explicit HIntConstant(int32_t value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kIntConstant, DataType::Type::kInt32, dex_pc), value_(value) {
  }
  explicit HIntConstant(bool value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kIntConstant, DataType::Type::kInt32, dex_pc),
        value_(value ? 1 : 0) {
  }

  const int32_t value_;

  friend class HGraph;
  ART_FRIEND_TEST(GraphTest, InsertInstructionBefore);
  ART_FRIEND_TYPED_TEST(ParallelMoveTest, ConstantLast);
};

class HLongConstant FINAL : public HConstant {
 public:
  int64_t GetValue() const { return value_; }

  uint64_t GetValueAsUint64() const OVERRIDE { return value_; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsLongConstant()) << other->DebugName();
    return other->AsLongConstant()->value_ == value_;
  }

  size_t ComputeHashCode() const OVERRIDE { return static_cast<size_t>(GetValue()); }

  bool IsMinusOne() const OVERRIDE { return GetValue() == -1; }
  bool IsArithmeticZero() const OVERRIDE { return GetValue() == 0; }
  bool IsZeroBitPattern() const OVERRIDE { return GetValue() == 0; }
  bool IsOne() const OVERRIDE { return GetValue() == 1; }

  DECLARE_INSTRUCTION(LongConstant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LongConstant);

 private:
  explicit HLongConstant(int64_t value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kLongConstant, DataType::Type::kInt64, dex_pc),
        value_(value) {
  }

  const int64_t value_;

  friend class HGraph;
};

class HFloatConstant FINAL : public HConstant {
 public:
  float GetValue() const { return value_; }

  uint64_t GetValueAsUint64() const OVERRIDE {
    return static_cast<uint64_t>(bit_cast<uint32_t, float>(value_));
  }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsFloatConstant()) << other->DebugName();
    return other->AsFloatConstant()->GetValueAsUint64() == GetValueAsUint64();
  }

  size_t ComputeHashCode() const OVERRIDE { return static_cast<size_t>(GetValue()); }

  bool IsMinusOne() const OVERRIDE {
    return bit_cast<uint32_t, float>(value_) == bit_cast<uint32_t, float>((-1.0f));
  }
  bool IsArithmeticZero() const OVERRIDE {
    return std::fpclassify(value_) == FP_ZERO;
  }
  bool IsArithmeticPositiveZero() const {
    return IsArithmeticZero() && !std::signbit(value_);
  }
  bool IsArithmeticNegativeZero() const {
    return IsArithmeticZero() && std::signbit(value_);
  }
  bool IsZeroBitPattern() const OVERRIDE {
    return bit_cast<uint32_t, float>(value_) == bit_cast<uint32_t, float>(0.0f);
  }
  bool IsOne() const OVERRIDE {
    return bit_cast<uint32_t, float>(value_) == bit_cast<uint32_t, float>(1.0f);
  }
  bool IsNaN() const {
    return std::isnan(value_);
  }

  DECLARE_INSTRUCTION(FloatConstant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(FloatConstant);

 private:
  explicit HFloatConstant(float value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kFloatConstant, DataType::Type::kFloat32, dex_pc),
        value_(value) {
  }
  explicit HFloatConstant(int32_t value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kFloatConstant, DataType::Type::kFloat32, dex_pc),
        value_(bit_cast<float, int32_t>(value)) {
  }

  const float value_;

  // Only the SsaBuilder and HGraph can create floating-point constants.
  friend class SsaBuilder;
  friend class HGraph;
};

class HDoubleConstant FINAL : public HConstant {
 public:
  double GetValue() const { return value_; }

  uint64_t GetValueAsUint64() const OVERRIDE { return bit_cast<uint64_t, double>(value_); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    DCHECK(other->IsDoubleConstant()) << other->DebugName();
    return other->AsDoubleConstant()->GetValueAsUint64() == GetValueAsUint64();
  }

  size_t ComputeHashCode() const OVERRIDE { return static_cast<size_t>(GetValue()); }

  bool IsMinusOne() const OVERRIDE {
    return bit_cast<uint64_t, double>(value_) == bit_cast<uint64_t, double>((-1.0));
  }
  bool IsArithmeticZero() const OVERRIDE {
    return std::fpclassify(value_) == FP_ZERO;
  }
  bool IsArithmeticPositiveZero() const {
    return IsArithmeticZero() && !std::signbit(value_);
  }
  bool IsArithmeticNegativeZero() const {
    return IsArithmeticZero() && std::signbit(value_);
  }
  bool IsZeroBitPattern() const OVERRIDE {
    return bit_cast<uint64_t, double>(value_) == bit_cast<uint64_t, double>((0.0));
  }
  bool IsOne() const OVERRIDE {
    return bit_cast<uint64_t, double>(value_) == bit_cast<uint64_t, double>(1.0);
  }
  bool IsNaN() const {
    return std::isnan(value_);
  }

  DECLARE_INSTRUCTION(DoubleConstant);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(DoubleConstant);

 private:
  explicit HDoubleConstant(double value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kDoubleConstant, DataType::Type::kFloat64, dex_pc),
        value_(value) {
  }
  explicit HDoubleConstant(int64_t value, uint32_t dex_pc = kNoDexPc)
      : HConstant(kDoubleConstant, DataType::Type::kFloat64, dex_pc),
        value_(bit_cast<double, int64_t>(value)) {
  }

  const double value_;

  // Only the SsaBuilder and HGraph can create floating-point constants.
  friend class SsaBuilder;
  friend class HGraph;
};

// Conditional branch. A block ending with an HIf instruction must have
// two successors.
class HIf FINAL : public HTemplateInstruction<1> {
 public:
  explicit HIf(HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kIf, SideEffects::None(), dex_pc) {
    SetRawInputAt(0, input);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool IsControlFlow() const OVERRIDE { return true; }

  HBasicBlock* IfTrueSuccessor() const {
    return GetBlock()->GetSuccessors()[0];
  }

  HBasicBlock* IfFalseSuccessor() const {
    return GetBlock()->GetSuccessors()[1];
  }

  DECLARE_INSTRUCTION(If);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(If);
};


// Abstract instruction which marks the beginning and/or end of a try block and
// links it to the respective exception handlers. Behaves the same as a Goto in
// non-exceptional control flow.
// Normal-flow successor is stored at index zero, exception handlers under
// higher indices in no particular order.
class HTryBoundary FINAL : public HTemplateInstruction<0> {
 public:
  enum class BoundaryKind {
    kEntry,
    kExit,
    kLast = kExit
  };

  explicit HTryBoundary(BoundaryKind kind, uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kTryBoundary, SideEffects::None(), dex_pc) {
    SetPackedField<BoundaryKindField>(kind);
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  // Returns the block's non-exceptional successor (index zero).
  HBasicBlock* GetNormalFlowSuccessor() const { return GetBlock()->GetSuccessors()[0]; }

  ArrayRef<HBasicBlock* const> GetExceptionHandlers() const {
    return ArrayRef<HBasicBlock* const>(GetBlock()->GetSuccessors()).SubArray(1u);
  }

  // Returns whether `handler` is among its exception handlers (non-zero index
  // successors).
  bool HasExceptionHandler(const HBasicBlock& handler) const {
    DCHECK(handler.IsCatchBlock());
    return GetBlock()->HasSuccessor(&handler, 1u /* Skip first successor. */);
  }

  // If not present already, adds `handler` to its block's list of exception
  // handlers.
  void AddExceptionHandler(HBasicBlock* handler) {
    if (!HasExceptionHandler(*handler)) {
      GetBlock()->AddSuccessor(handler);
    }
  }

  BoundaryKind GetBoundaryKind() const { return GetPackedField<BoundaryKindField>(); }
  bool IsEntry() const { return GetBoundaryKind() == BoundaryKind::kEntry; }

  bool HasSameExceptionHandlersAs(const HTryBoundary& other) const;

  DECLARE_INSTRUCTION(TryBoundary);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(TryBoundary);

 private:
  static constexpr size_t kFieldBoundaryKind = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldBoundaryKindSize =
      MinimumBitsToStore(static_cast<size_t>(BoundaryKind::kLast));
  static constexpr size_t kNumberOfTryBoundaryPackedBits =
      kFieldBoundaryKind + kFieldBoundaryKindSize;
  static_assert(kNumberOfTryBoundaryPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using BoundaryKindField = BitField<BoundaryKind, kFieldBoundaryKind, kFieldBoundaryKindSize>;
};

// Deoptimize to interpreter, upon checking a condition.
class HDeoptimize FINAL : public HVariableInputSizeInstruction {
 public:
  // Use this constructor when the `HDeoptimize` acts as a barrier, where no code can move
  // across.
  HDeoptimize(ArenaAllocator* allocator,
              HInstruction* cond,
              DeoptimizationKind kind,
              uint32_t dex_pc)
      : HVariableInputSizeInstruction(
            kDeoptimize,
            SideEffects::All(),
            dex_pc,
            allocator,
            /* number_of_inputs */ 1,
            kArenaAllocMisc) {
    SetPackedFlag<kFieldCanBeMoved>(false);
    SetPackedField<DeoptimizeKindField>(kind);
    SetRawInputAt(0, cond);
  }

  bool IsClonable() const OVERRIDE { return true; }

  // Use this constructor when the `HDeoptimize` guards an instruction, and any user
  // that relies on the deoptimization to pass should have its input be the `HDeoptimize`
  // instead of `guard`.
  // We set CanTriggerGC to prevent any intermediate address to be live
  // at the point of the `HDeoptimize`.
  HDeoptimize(ArenaAllocator* allocator,
              HInstruction* cond,
              HInstruction* guard,
              DeoptimizationKind kind,
              uint32_t dex_pc)
      : HVariableInputSizeInstruction(
            kDeoptimize,
            SideEffects::CanTriggerGC(),
            dex_pc,
            allocator,
            /* number_of_inputs */ 2,
            kArenaAllocMisc) {
    SetPackedFlag<kFieldCanBeMoved>(true);
    SetPackedField<DeoptimizeKindField>(kind);
    SetRawInputAt(0, cond);
    SetRawInputAt(1, guard);
  }

  bool CanBeMoved() const OVERRIDE { return GetPackedFlag<kFieldCanBeMoved>(); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return (other->CanBeMoved() == CanBeMoved()) && (other->AsDeoptimize()->GetKind() == GetKind());
  }

  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE { return true; }

  DeoptimizationKind GetDeoptimizationKind() const { return GetPackedField<DeoptimizeKindField>(); }

  DataType::Type GetType() const OVERRIDE {
    return GuardsAnInput() ? GuardedInput()->GetType() : DataType::Type::kVoid;
  }

  bool GuardsAnInput() const {
    return InputCount() == 2;
  }

  HInstruction* GuardedInput() const {
    DCHECK(GuardsAnInput());
    return InputAt(1);
  }

  void RemoveGuard() {
    RemoveInputAt(1);
  }

  DECLARE_INSTRUCTION(Deoptimize);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Deoptimize);

 private:
  static constexpr size_t kFieldCanBeMoved = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldDeoptimizeKind = kNumberOfGenericPackedBits + 1;
  static constexpr size_t kFieldDeoptimizeKindSize =
      MinimumBitsToStore(static_cast<size_t>(DeoptimizationKind::kLast));
  static constexpr size_t kNumberOfDeoptimizePackedBits =
      kFieldDeoptimizeKind + kFieldDeoptimizeKindSize;
  static_assert(kNumberOfDeoptimizePackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using DeoptimizeKindField =
      BitField<DeoptimizationKind, kFieldDeoptimizeKind, kFieldDeoptimizeKindSize>;
};

// Represents a should_deoptimize flag. Currently used for CHA-based devirtualization.
// The compiled code checks this flag value in a guard before devirtualized call and
// if it's true, starts to do deoptimization.
// It has a 4-byte slot on stack.
// TODO: allocate a register for this flag.
class HShouldDeoptimizeFlag FINAL : public HVariableInputSizeInstruction {
 public:
  // CHA guards are only optimized in a separate pass and it has no side effects
  // with regard to other passes.
  HShouldDeoptimizeFlag(ArenaAllocator* allocator, uint32_t dex_pc)
      : HVariableInputSizeInstruction(kShouldDeoptimizeFlag,
                                      SideEffects::None(),
                                      dex_pc,
                                      allocator,
                                      0,
                                      kArenaAllocCHA) {
  }

  DataType::Type GetType() const OVERRIDE { return DataType::Type::kInt32; }

  // We do all CHA guard elimination/motion in a single pass, after which there is no
  // further guard elimination/motion since a guard might have been used for justification
  // of the elimination of another guard. Therefore, we pretend this guard cannot be moved
  // to avoid other optimizations trying to move it.
  bool CanBeMoved() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(ShouldDeoptimizeFlag);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ShouldDeoptimizeFlag);
};

// Represents the ArtMethod that was passed as a first argument to
// the method. It is used by instructions that depend on it, like
// instructions that work with the dex cache.
class HCurrentMethod FINAL : public HExpression<0> {
 public:
  explicit HCurrentMethod(DataType::Type type, uint32_t dex_pc = kNoDexPc)
      : HExpression(kCurrentMethod, type, SideEffects::None(), dex_pc) {
  }

  DECLARE_INSTRUCTION(CurrentMethod);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(CurrentMethod);
};

// Fetches an ArtMethod from the virtual table or the interface method table
// of a class.
class HClassTableGet FINAL : public HExpression<1> {
 public:
  enum class TableKind {
    kVTable,
    kIMTable,
    kLast = kIMTable
  };
  HClassTableGet(HInstruction* cls,
                 DataType::Type type,
                 TableKind kind,
                 size_t index,
                 uint32_t dex_pc)
      : HExpression(kClassTableGet, type, SideEffects::None(), dex_pc),
        index_(index) {
    SetPackedField<TableKindField>(kind);
    SetRawInputAt(0, cls);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return other->AsClassTableGet()->GetIndex() == index_ &&
        other->AsClassTableGet()->GetPackedFields() == GetPackedFields();
  }

  TableKind GetTableKind() const { return GetPackedField<TableKindField>(); }
  size_t GetIndex() const { return index_; }

  DECLARE_INSTRUCTION(ClassTableGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ClassTableGet);

 private:
  static constexpr size_t kFieldTableKind = kNumberOfExpressionPackedBits;
  static constexpr size_t kFieldTableKindSize =
      MinimumBitsToStore(static_cast<size_t>(TableKind::kLast));
  static constexpr size_t kNumberOfClassTableGetPackedBits = kFieldTableKind + kFieldTableKindSize;
  static_assert(kNumberOfClassTableGetPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using TableKindField = BitField<TableKind, kFieldTableKind, kFieldTableKind>;

  // The index of the ArtMethod in the table.
  const size_t index_;
};

// PackedSwitch (jump table). A block ending with a PackedSwitch instruction will
// have one successor for each entry in the switch table, and the final successor
// will be the block containing the next Dex opcode.
class HPackedSwitch FINAL : public HTemplateInstruction<1> {
 public:
  HPackedSwitch(int32_t start_value,
                uint32_t num_entries,
                HInstruction* input,
                uint32_t dex_pc = kNoDexPc)
    : HTemplateInstruction(kPackedSwitch, SideEffects::None(), dex_pc),
      start_value_(start_value),
      num_entries_(num_entries) {
    SetRawInputAt(0, input);
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool IsControlFlow() const OVERRIDE { return true; }

  int32_t GetStartValue() const { return start_value_; }

  uint32_t GetNumEntries() const { return num_entries_; }

  HBasicBlock* GetDefaultBlock() const {
    // Last entry is the default block.
    return GetBlock()->GetSuccessors()[num_entries_];
  }
  DECLARE_INSTRUCTION(PackedSwitch);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(PackedSwitch);

 private:
  const int32_t start_value_;
  const uint32_t num_entries_;
};

class HUnaryOperation : public HExpression<1> {
 public:
  HUnaryOperation(InstructionKind kind,
                  DataType::Type result_type,
                  HInstruction* input,
                  uint32_t dex_pc = kNoDexPc)
      : HExpression(kind, result_type, SideEffects::None(), dex_pc) {
    SetRawInputAt(0, input);
  }

  // All of the UnaryOperation instructions are clonable.
  bool IsClonable() const OVERRIDE { return true; }

  HInstruction* GetInput() const { return InputAt(0); }
  DataType::Type GetResultType() const { return GetType(); }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  // Try to statically evaluate `this` and return a HConstant
  // containing the result of this evaluation.  If `this` cannot
  // be evaluated as a constant, return null.
  HConstant* TryStaticEvaluation() const;

  // Apply this operation to `x`.
  virtual HConstant* Evaluate(HIntConstant* x) const = 0;
  virtual HConstant* Evaluate(HLongConstant* x) const = 0;
  virtual HConstant* Evaluate(HFloatConstant* x) const = 0;
  virtual HConstant* Evaluate(HDoubleConstant* x) const = 0;

  DECLARE_ABSTRACT_INSTRUCTION(UnaryOperation);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UnaryOperation);
};

class HBinaryOperation : public HExpression<2> {
 public:
  HBinaryOperation(InstructionKind kind,
                   DataType::Type result_type,
                   HInstruction* left,
                   HInstruction* right,
                   SideEffects side_effects = SideEffects::None(),
                   uint32_t dex_pc = kNoDexPc)
      : HExpression(kind, result_type, side_effects, dex_pc) {
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }

  // All of the BinaryOperation instructions are clonable.
  bool IsClonable() const OVERRIDE { return true; }

  HInstruction* GetLeft() const { return InputAt(0); }
  HInstruction* GetRight() const { return InputAt(1); }
  DataType::Type GetResultType() const { return GetType(); }

  virtual bool IsCommutative() const { return false; }

  // Put constant on the right.
  // Returns whether order is changed.
  bool OrderInputsWithConstantOnTheRight() {
    HInstruction* left = InputAt(0);
    HInstruction* right = InputAt(1);
    if (left->IsConstant() && !right->IsConstant()) {
      ReplaceInput(right, 0);
      ReplaceInput(left, 1);
      return true;
    }
    return false;
  }

  // Order inputs by instruction id, but favor constant on the right side.
  // This helps GVN for commutative ops.
  void OrderInputs() {
    DCHECK(IsCommutative());
    HInstruction* left = InputAt(0);
    HInstruction* right = InputAt(1);
    if (left == right || (!left->IsConstant() && right->IsConstant())) {
      return;
    }
    if (OrderInputsWithConstantOnTheRight()) {
      return;
    }
    // Order according to instruction id.
    if (left->GetId() > right->GetId()) {
      ReplaceInput(right, 0);
      ReplaceInput(left, 1);
    }
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  // Try to statically evaluate `this` and return a HConstant
  // containing the result of this evaluation.  If `this` cannot
  // be evaluated as a constant, return null.
  HConstant* TryStaticEvaluation() const;

  // Apply this operation to `x` and `y`.
  virtual HConstant* Evaluate(HNullConstant* x ATTRIBUTE_UNUSED,
                              HNullConstant* y ATTRIBUTE_UNUSED) const {
    LOG(FATAL) << DebugName() << " is not defined for the (null, null) case.";
    UNREACHABLE();
  }
  virtual HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const = 0;
  virtual HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const = 0;
  virtual HConstant* Evaluate(HLongConstant* x ATTRIBUTE_UNUSED,
                              HIntConstant* y ATTRIBUTE_UNUSED) const {
    LOG(FATAL) << DebugName() << " is not defined for the (long, int) case.";
    UNREACHABLE();
  }
  virtual HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const = 0;
  virtual HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const = 0;

  // Returns an input that can legally be used as the right input and is
  // constant, or null.
  HConstant* GetConstantRight() const;

  // If `GetConstantRight()` returns one of the input, this returns the other
  // one. Otherwise it returns null.
  HInstruction* GetLeastConstantLeft() const;

  DECLARE_ABSTRACT_INSTRUCTION(BinaryOperation);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BinaryOperation);
};

// The comparison bias applies for floating point operations and indicates how NaN
// comparisons are treated:
enum class ComparisonBias {
  kNoBias,  // bias is not applicable (i.e. for long operation)
  kGtBias,  // return 1 for NaN comparisons
  kLtBias,  // return -1 for NaN comparisons
  kLast = kLtBias
};

std::ostream& operator<<(std::ostream& os, const ComparisonBias& rhs);

class HCondition : public HBinaryOperation {
 public:
  HCondition(InstructionKind kind,
             HInstruction* first,
             HInstruction* second,
             uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kind,
                         DataType::Type::kBool,
                         first,
                         second,
                         SideEffects::None(),
                         dex_pc) {
    SetPackedField<ComparisonBiasField>(ComparisonBias::kNoBias);
  }

  // For code generation purposes, returns whether this instruction is just before
  // `instruction`, and disregard moves in between.
  bool IsBeforeWhenDisregardMoves(HInstruction* instruction) const;

  DECLARE_ABSTRACT_INSTRUCTION(Condition);

  virtual IfCondition GetCondition() const = 0;

  virtual IfCondition GetOppositeCondition() const = 0;

  bool IsGtBias() const { return GetBias() == ComparisonBias::kGtBias; }
  bool IsLtBias() const { return GetBias() == ComparisonBias::kLtBias; }

  ComparisonBias GetBias() const { return GetPackedField<ComparisonBiasField>(); }
  void SetBias(ComparisonBias bias) { SetPackedField<ComparisonBiasField>(bias); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return GetPackedFields() == other->AsCondition()->GetPackedFields();
  }

  bool IsFPConditionTrueIfNaN() const {
    DCHECK(DataType::IsFloatingPointType(InputAt(0)->GetType())) << InputAt(0)->GetType();
    IfCondition if_cond = GetCondition();
    if (if_cond == kCondNE) {
      return true;
    } else if (if_cond == kCondEQ) {
      return false;
    }
    return ((if_cond == kCondGT) || (if_cond == kCondGE)) && IsGtBias();
  }

  bool IsFPConditionFalseIfNaN() const {
    DCHECK(DataType::IsFloatingPointType(InputAt(0)->GetType())) << InputAt(0)->GetType();
    IfCondition if_cond = GetCondition();
    if (if_cond == kCondEQ) {
      return true;
    } else if (if_cond == kCondNE) {
      return false;
    }
    return ((if_cond == kCondLT) || (if_cond == kCondLE)) && IsGtBias();
  }

 protected:
  // Needed if we merge a HCompare into a HCondition.
  static constexpr size_t kFieldComparisonBias = kNumberOfExpressionPackedBits;
  static constexpr size_t kFieldComparisonBiasSize =
      MinimumBitsToStore(static_cast<size_t>(ComparisonBias::kLast));
  static constexpr size_t kNumberOfConditionPackedBits =
      kFieldComparisonBias + kFieldComparisonBiasSize;
  static_assert(kNumberOfConditionPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using ComparisonBiasField =
      BitField<ComparisonBias, kFieldComparisonBias, kFieldComparisonBiasSize>;

  template <typename T>
  int32_t Compare(T x, T y) const { return x > y ? 1 : (x < y ? -1 : 0); }

  template <typename T>
  int32_t CompareFP(T x, T y) const {
    DCHECK(DataType::IsFloatingPointType(InputAt(0)->GetType())) << InputAt(0)->GetType();
    DCHECK_NE(GetBias(), ComparisonBias::kNoBias);
    // Handle the bias.
    return std::isunordered(x, y) ? (IsGtBias() ? 1 : -1) : Compare(x, y);
  }

  // Return an integer constant containing the result of a condition evaluated at compile time.
  HIntConstant* MakeConstantCondition(bool value, uint32_t dex_pc) const {
    return GetBlock()->GetGraph()->GetIntConstant(value, dex_pc);
  }

  DEFAULT_COPY_CONSTRUCTOR(Condition);
};

// Instruction to check if two inputs are equal to each other.
class HEqual FINAL : public HCondition {
 public:
  HEqual(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kEqual, first, second, dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  HConstant* Evaluate(HNullConstant* x ATTRIBUTE_UNUSED,
                      HNullConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    return MakeConstantCondition(true, GetDexPc());
  }
  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HEqual instruction; evaluate it as
  // `Compare(x, y) == 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0),
                                 GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(Equal);

  IfCondition GetCondition() const OVERRIDE {
    return kCondEQ;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondNE;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Equal);

 private:
  template <typename T> static bool Compute(T x, T y) { return x == y; }
};

class HNotEqual FINAL : public HCondition {
 public:
  HNotEqual(HInstruction* first, HInstruction* second,
            uint32_t dex_pc = kNoDexPc)
      : HCondition(kNotEqual, first, second, dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  HConstant* Evaluate(HNullConstant* x ATTRIBUTE_UNUSED,
                      HNullConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    return MakeConstantCondition(false, GetDexPc());
  }
  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HNotEqual instruction; evaluate it as
  // `Compare(x, y) != 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(NotEqual);

  IfCondition GetCondition() const OVERRIDE {
    return kCondNE;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondEQ;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NotEqual);

 private:
  template <typename T> static bool Compute(T x, T y) { return x != y; }
};

class HLessThan FINAL : public HCondition {
 public:
  HLessThan(HInstruction* first, HInstruction* second,
            uint32_t dex_pc = kNoDexPc)
      : HCondition(kLessThan, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HLessThan instruction; evaluate it as
  // `Compare(x, y) < 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(LessThan);

  IfCondition GetCondition() const OVERRIDE {
    return kCondLT;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondGE;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LessThan);

 private:
  template <typename T> static bool Compute(T x, T y) { return x < y; }
};

class HLessThanOrEqual FINAL : public HCondition {
 public:
  HLessThanOrEqual(HInstruction* first, HInstruction* second,
                   uint32_t dex_pc = kNoDexPc)
      : HCondition(kLessThanOrEqual, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HLessThanOrEqual instruction; evaluate it as
  // `Compare(x, y) <= 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(LessThanOrEqual);

  IfCondition GetCondition() const OVERRIDE {
    return kCondLE;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondGT;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LessThanOrEqual);

 private:
  template <typename T> static bool Compute(T x, T y) { return x <= y; }
};

class HGreaterThan FINAL : public HCondition {
 public:
  HGreaterThan(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kGreaterThan, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HGreaterThan instruction; evaluate it as
  // `Compare(x, y) > 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(GreaterThan);

  IfCondition GetCondition() const OVERRIDE {
    return kCondGT;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondLE;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(GreaterThan);

 private:
  template <typename T> static bool Compute(T x, T y) { return x > y; }
};

class HGreaterThanOrEqual FINAL : public HCondition {
 public:
  HGreaterThanOrEqual(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kGreaterThanOrEqual, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  // In the following Evaluate methods, a HCompare instruction has
  // been merged into this HGreaterThanOrEqual instruction; evaluate it as
  // `Compare(x, y) >= 0`.
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(Compare(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(CompareFP(x->GetValue(), y->GetValue()), 0), GetDexPc());
  }

  DECLARE_INSTRUCTION(GreaterThanOrEqual);

  IfCondition GetCondition() const OVERRIDE {
    return kCondGE;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondLT;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(GreaterThanOrEqual);

 private:
  template <typename T> static bool Compute(T x, T y) { return x >= y; }
};

class HBelow FINAL : public HCondition {
 public:
  HBelow(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kBelow, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Below);

  IfCondition GetCondition() const OVERRIDE {
    return kCondB;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondAE;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Below);

 private:
  template <typename T> static bool Compute(T x, T y) {
    return MakeUnsigned(x) < MakeUnsigned(y);
  }
};

class HBelowOrEqual FINAL : public HCondition {
 public:
  HBelowOrEqual(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kBelowOrEqual, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(BelowOrEqual);

  IfCondition GetCondition() const OVERRIDE {
    return kCondBE;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondA;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BelowOrEqual);

 private:
  template <typename T> static bool Compute(T x, T y) {
    return MakeUnsigned(x) <= MakeUnsigned(y);
  }
};

class HAbove FINAL : public HCondition {
 public:
  HAbove(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kAbove, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Above);

  IfCondition GetCondition() const OVERRIDE {
    return kCondA;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondBE;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Above);

 private:
  template <typename T> static bool Compute(T x, T y) {
    return MakeUnsigned(x) > MakeUnsigned(y);
  }
};

class HAboveOrEqual FINAL : public HCondition {
 public:
  HAboveOrEqual(HInstruction* first, HInstruction* second, uint32_t dex_pc = kNoDexPc)
      : HCondition(kAboveOrEqual, first, second, dex_pc) {
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantCondition(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(AboveOrEqual);

  IfCondition GetCondition() const OVERRIDE {
    return kCondAE;
  }

  IfCondition GetOppositeCondition() const OVERRIDE {
    return kCondB;
  }

 protected:
  DEFAULT_COPY_CONSTRUCTOR(AboveOrEqual);

 private:
  template <typename T> static bool Compute(T x, T y) {
    return MakeUnsigned(x) >= MakeUnsigned(y);
  }
};

// Instruction to check how two inputs compare to each other.
// Result is 0 if input0 == input1, 1 if input0 > input1, or -1 if input0 < input1.
class HCompare FINAL : public HBinaryOperation {
 public:
  // Note that `comparison_type` is the type of comparison performed
  // between the comparison's inputs, not the type of the instantiated
  // HCompare instruction (which is always DataType::Type::kInt).
  HCompare(DataType::Type comparison_type,
           HInstruction* first,
           HInstruction* second,
           ComparisonBias bias,
           uint32_t dex_pc)
      : HBinaryOperation(kCompare,
                         DataType::Type::kInt32,
                         first,
                         second,
                         SideEffectsForArchRuntimeCalls(comparison_type),
                         dex_pc) {
    SetPackedField<ComparisonBiasField>(bias);
    DCHECK_EQ(comparison_type, DataType::Kind(first->GetType()));
    DCHECK_EQ(comparison_type, DataType::Kind(second->GetType()));
  }

  template <typename T>
  int32_t Compute(T x, T y) const { return x > y ? 1 : (x < y ? -1 : 0); }

  template <typename T>
  int32_t ComputeFP(T x, T y) const {
    DCHECK(DataType::IsFloatingPointType(InputAt(0)->GetType())) << InputAt(0)->GetType();
    DCHECK_NE(GetBias(), ComparisonBias::kNoBias);
    // Handle the bias.
    return std::isunordered(x, y) ? (IsGtBias() ? 1 : -1) : Compute(x, y);
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    // Note that there is no "cmp-int" Dex instruction so we shouldn't
    // reach this code path when processing a freshly built HIR
    // graph. However HCompare integer instructions can be synthesized
    // by the instruction simplifier to implement IntegerCompare and
    // IntegerSignum intrinsics, so we have to handle this case.
    return MakeConstantComparison(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return MakeConstantComparison(Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return MakeConstantComparison(ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return MakeConstantComparison(ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return GetPackedFields() == other->AsCompare()->GetPackedFields();
  }

  ComparisonBias GetBias() const { return GetPackedField<ComparisonBiasField>(); }

  // Does this compare instruction have a "gt bias" (vs an "lt bias")?
  // Only meaningful for floating-point comparisons.
  bool IsGtBias() const {
    DCHECK(DataType::IsFloatingPointType(InputAt(0)->GetType())) << InputAt(0)->GetType();
    return GetBias() == ComparisonBias::kGtBias;
  }

  static SideEffects SideEffectsForArchRuntimeCalls(DataType::Type type ATTRIBUTE_UNUSED) {
    // Comparisons do not require a runtime call in any back end.
    return SideEffects::None();
  }

  DECLARE_INSTRUCTION(Compare);

 protected:
  static constexpr size_t kFieldComparisonBias = kNumberOfExpressionPackedBits;
  static constexpr size_t kFieldComparisonBiasSize =
      MinimumBitsToStore(static_cast<size_t>(ComparisonBias::kLast));
  static constexpr size_t kNumberOfComparePackedBits =
      kFieldComparisonBias + kFieldComparisonBiasSize;
  static_assert(kNumberOfComparePackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using ComparisonBiasField =
      BitField<ComparisonBias, kFieldComparisonBias, kFieldComparisonBiasSize>;

  // Return an integer constant containing the result of a comparison evaluated at compile time.
  HIntConstant* MakeConstantComparison(int32_t value, uint32_t dex_pc) const {
    DCHECK(value == -1 || value == 0 || value == 1) << value;
    return GetBlock()->GetGraph()->GetIntConstant(value, dex_pc);
  }

  DEFAULT_COPY_CONSTRUCTOR(Compare);
};

class HNewInstance FINAL : public HExpression<1> {
 public:
  HNewInstance(HInstruction* cls,
               uint32_t dex_pc,
               dex::TypeIndex type_index,
               const DexFile& dex_file,
               bool finalizable,
               QuickEntrypointEnum entrypoint)
      : HExpression(kNewInstance,
                    DataType::Type::kReference,
                    SideEffects::CanTriggerGC(),
                    dex_pc),
        type_index_(type_index),
        dex_file_(dex_file),
        entrypoint_(entrypoint) {
    SetPackedFlag<kFlagFinalizable>(finalizable);
    SetRawInputAt(0, cls);
  }

  bool IsClonable() const OVERRIDE { return true; }

  dex::TypeIndex GetTypeIndex() const { return type_index_; }
  const DexFile& GetDexFile() const { return dex_file_; }

  // Calls runtime so needs an environment.
  bool NeedsEnvironment() const OVERRIDE { return true; }

  // Can throw errors when out-of-memory or if it's not instantiable/accessible.
  bool CanThrow() const OVERRIDE { return true; }

  bool NeedsChecks() const {
    return entrypoint_ == kQuickAllocObjectWithChecks;
  }

  bool IsFinalizable() const { return GetPackedFlag<kFlagFinalizable>(); }

  bool CanBeNull() const OVERRIDE { return false; }

  QuickEntrypointEnum GetEntrypoint() const { return entrypoint_; }

  void SetEntrypoint(QuickEntrypointEnum entrypoint) {
    entrypoint_ = entrypoint;
  }

  HLoadClass* GetLoadClass() const {
    HInstruction* input = InputAt(0);
    if (input->IsClinitCheck()) {
      input = input->InputAt(0);
    }
    DCHECK(input->IsLoadClass());
    return input->AsLoadClass();
  }

  bool IsStringAlloc() const;

  DECLARE_INSTRUCTION(NewInstance);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NewInstance);

 private:
  static constexpr size_t kFlagFinalizable = kNumberOfExpressionPackedBits;
  static constexpr size_t kNumberOfNewInstancePackedBits = kFlagFinalizable + 1;
  static_assert(kNumberOfNewInstancePackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");

  const dex::TypeIndex type_index_;
  const DexFile& dex_file_;
  QuickEntrypointEnum entrypoint_;
};

enum IntrinsicNeedsEnvironmentOrCache {
  kNoEnvironmentOrCache,        // Intrinsic does not require an environment or dex cache.
  kNeedsEnvironmentOrCache      // Intrinsic requires an environment or requires a dex cache.
};

enum IntrinsicSideEffects {
  kNoSideEffects,     // Intrinsic does not have any heap memory side effects.
  kReadSideEffects,   // Intrinsic may read heap memory.
  kWriteSideEffects,  // Intrinsic may write heap memory.
  kAllSideEffects     // Intrinsic may read or write heap memory, or trigger GC.
};

enum IntrinsicExceptions {
  kNoThrow,  // Intrinsic does not throw any exceptions.
  kCanThrow  // Intrinsic may throw exceptions.
};

class HInvoke : public HVariableInputSizeInstruction {
 public:
  bool NeedsEnvironment() const OVERRIDE;

  void SetArgumentAt(size_t index, HInstruction* argument) {
    SetRawInputAt(index, argument);
  }

  // Return the number of arguments.  This number can be lower than
  // the number of inputs returned by InputCount(), as some invoke
  // instructions (e.g. HInvokeStaticOrDirect) can have non-argument
  // inputs at the end of their list of inputs.
  uint32_t GetNumberOfArguments() const { return number_of_arguments_; }

  DataType::Type GetType() const OVERRIDE { return GetPackedField<ReturnTypeField>(); }

  uint32_t GetDexMethodIndex() const { return dex_method_index_; }

  InvokeType GetInvokeType() const {
    return GetPackedField<InvokeTypeField>();
  }

  Intrinsics GetIntrinsic() const {
    return intrinsic_;
  }

  void SetIntrinsic(Intrinsics intrinsic,
                    IntrinsicNeedsEnvironmentOrCache needs_env_or_cache,
                    IntrinsicSideEffects side_effects,
                    IntrinsicExceptions exceptions);

  bool IsFromInlinedInvoke() const {
    return GetEnvironment()->IsFromInlinedInvoke();
  }

  void SetCanThrow(bool can_throw) { SetPackedFlag<kFlagCanThrow>(can_throw); }

  bool CanThrow() const OVERRIDE { return GetPackedFlag<kFlagCanThrow>(); }

  void SetAlwaysThrows(bool always_throws) { SetPackedFlag<kFlagAlwaysThrows>(always_throws); }

  bool AlwaysThrows() const OVERRIDE { return GetPackedFlag<kFlagAlwaysThrows>(); }

  bool CanBeMoved() const OVERRIDE { return IsIntrinsic() && !DoesAnyWrite(); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return intrinsic_ != Intrinsics::kNone && intrinsic_ == other->AsInvoke()->intrinsic_;
  }

  uint32_t* GetIntrinsicOptimizations() {
    return &intrinsic_optimizations_;
  }

  const uint32_t* GetIntrinsicOptimizations() const {
    return &intrinsic_optimizations_;
  }

  bool IsIntrinsic() const { return intrinsic_ != Intrinsics::kNone; }

  ArtMethod* GetResolvedMethod() const { return resolved_method_; }
  void SetResolvedMethod(ArtMethod* method) { resolved_method_ = method; }

  DECLARE_ABSTRACT_INSTRUCTION(Invoke);

 protected:
  static constexpr size_t kFieldInvokeType = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldInvokeTypeSize =
      MinimumBitsToStore(static_cast<size_t>(kMaxInvokeType));
  static constexpr size_t kFieldReturnType =
      kFieldInvokeType + kFieldInvokeTypeSize;
  static constexpr size_t kFieldReturnTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kFlagCanThrow = kFieldReturnType + kFieldReturnTypeSize;
  static constexpr size_t kFlagAlwaysThrows = kFlagCanThrow + 1;
  static constexpr size_t kNumberOfInvokePackedBits = kFlagAlwaysThrows + 1;
  static_assert(kNumberOfInvokePackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using InvokeTypeField = BitField<InvokeType, kFieldInvokeType, kFieldInvokeTypeSize>;
  using ReturnTypeField = BitField<DataType::Type, kFieldReturnType, kFieldReturnTypeSize>;

  HInvoke(InstructionKind kind,
          ArenaAllocator* allocator,
          uint32_t number_of_arguments,
          uint32_t number_of_other_inputs,
          DataType::Type return_type,
          uint32_t dex_pc,
          uint32_t dex_method_index,
          ArtMethod* resolved_method,
          InvokeType invoke_type)
    : HVariableInputSizeInstruction(
          kind,
          SideEffects::AllExceptGCDependency(),  // Assume write/read on all fields/arrays.
          dex_pc,
          allocator,
          number_of_arguments + number_of_other_inputs,
          kArenaAllocInvokeInputs),
      number_of_arguments_(number_of_arguments),
      resolved_method_(resolved_method),
      dex_method_index_(dex_method_index),
      intrinsic_(Intrinsics::kNone),
      intrinsic_optimizations_(0) {
    SetPackedField<ReturnTypeField>(return_type);
    SetPackedField<InvokeTypeField>(invoke_type);
    SetPackedFlag<kFlagCanThrow>(true);
  }

  DEFAULT_COPY_CONSTRUCTOR(Invoke);

  uint32_t number_of_arguments_;
  ArtMethod* resolved_method_;
  const uint32_t dex_method_index_;
  Intrinsics intrinsic_;

  // A magic word holding optimizations for intrinsics. See intrinsics.h.
  uint32_t intrinsic_optimizations_;
};

class HInvokeUnresolved FINAL : public HInvoke {
 public:
  HInvokeUnresolved(ArenaAllocator* allocator,
                    uint32_t number_of_arguments,
                    DataType::Type return_type,
                    uint32_t dex_pc,
                    uint32_t dex_method_index,
                    InvokeType invoke_type)
      : HInvoke(kInvokeUnresolved,
                allocator,
                number_of_arguments,
                0u /* number_of_other_inputs */,
                return_type,
                dex_pc,
                dex_method_index,
                nullptr,
                invoke_type) {
  }

  bool IsClonable() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(InvokeUnresolved);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InvokeUnresolved);
};

class HInvokePolymorphic FINAL : public HInvoke {
 public:
  HInvokePolymorphic(ArenaAllocator* allocator,
                     uint32_t number_of_arguments,
                     DataType::Type return_type,
                     uint32_t dex_pc,
                     uint32_t dex_method_index)
      : HInvoke(kInvokePolymorphic,
                allocator,
                number_of_arguments,
                0u /* number_of_other_inputs */,
                return_type,
                dex_pc,
                dex_method_index,
                nullptr,
                kVirtual) {
  }

  bool IsClonable() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(InvokePolymorphic);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InvokePolymorphic);
};

class HInvokeStaticOrDirect FINAL : public HInvoke {
 public:
  // Requirements of this method call regarding the class
  // initialization (clinit) check of its declaring class.
  enum class ClinitCheckRequirement {
    kNone,      // Class already initialized.
    kExplicit,  // Static call having explicit clinit check as last input.
    kImplicit,  // Static call implicitly requiring a clinit check.
    kLast = kImplicit
  };

  // Determines how to load the target ArtMethod*.
  enum class MethodLoadKind {
    // Use a String init ArtMethod* loaded from Thread entrypoints.
    kStringInit,

    // Use the method's own ArtMethod* loaded by the register allocator.
    kRecursive,

    // Use PC-relative boot image ArtMethod* address that will be known at link time.
    // Used for boot image methods referenced by boot image code.
    kBootImageLinkTimePcRelative,

    // Use ArtMethod* at a known address, embed the direct address in the code.
    // Used for app->boot calls with non-relocatable image and for JIT-compiled calls.
    kDirectAddress,

    // Load from an entry in the .bss section using a PC-relative load.
    // Used for classes outside boot image when .bss is accessible with a PC-relative load.
    kBssEntry,

    // Make a runtime call to resolve and call the method. This is the last-resort-kind
    // used when other kinds are unimplemented on a particular architecture.
    kRuntimeCall,
  };

  // Determines the location of the code pointer.
  enum class CodePtrLocation {
    // Recursive call, use local PC-relative call instruction.
    kCallSelf,

    // Use code pointer from the ArtMethod*.
    // Used when we don't know the target code. This is also the last-resort-kind used when
    // other kinds are unimplemented or impractical (i.e. slow) on a particular architecture.
    kCallArtMethod,
  };

  struct DispatchInfo {
    MethodLoadKind method_load_kind;
    CodePtrLocation code_ptr_location;
    // The method load data holds
    //   - thread entrypoint offset for kStringInit method if this is a string init invoke.
    //     Note that there are multiple string init methods, each having its own offset.
    //   - the method address for kDirectAddress
    uint64_t method_load_data;
  };

  HInvokeStaticOrDirect(ArenaAllocator* allocator,
                        uint32_t number_of_arguments,
                        DataType::Type return_type,
                        uint32_t dex_pc,
                        uint32_t method_index,
                        ArtMethod* resolved_method,
                        DispatchInfo dispatch_info,
                        InvokeType invoke_type,
                        MethodReference target_method,
                        ClinitCheckRequirement clinit_check_requirement)
      : HInvoke(kInvokeStaticOrDirect,
                allocator,
                number_of_arguments,
                // There is potentially one extra argument for the HCurrentMethod node, and
                // potentially one other if the clinit check is explicit, and potentially
                // one other if the method is a string factory.
                (NeedsCurrentMethodInput(dispatch_info.method_load_kind) ? 1u : 0u) +
                    (clinit_check_requirement == ClinitCheckRequirement::kExplicit ? 1u : 0u),
                return_type,
                dex_pc,
                method_index,
                resolved_method,
                invoke_type),
        target_method_(target_method),
        dispatch_info_(dispatch_info) {
    SetPackedField<ClinitCheckRequirementField>(clinit_check_requirement);
  }

  bool IsClonable() const OVERRIDE { return true; }

  void SetDispatchInfo(const DispatchInfo& dispatch_info) {
    bool had_current_method_input = HasCurrentMethodInput();
    bool needs_current_method_input = NeedsCurrentMethodInput(dispatch_info.method_load_kind);

    // Using the current method is the default and once we find a better
    // method load kind, we should not go back to using the current method.
    DCHECK(had_current_method_input || !needs_current_method_input);

    if (had_current_method_input && !needs_current_method_input) {
      DCHECK_EQ(InputAt(GetSpecialInputIndex()), GetBlock()->GetGraph()->GetCurrentMethod());
      RemoveInputAt(GetSpecialInputIndex());
    }
    dispatch_info_ = dispatch_info;
  }

  DispatchInfo GetDispatchInfo() const {
    return dispatch_info_;
  }

  void AddSpecialInput(HInstruction* input) {
    // We allow only one special input.
    DCHECK(!IsStringInit() && !HasCurrentMethodInput());
    DCHECK(InputCount() == GetSpecialInputIndex() ||
           (InputCount() == GetSpecialInputIndex() + 1 && IsStaticWithExplicitClinitCheck()));
    InsertInputAt(GetSpecialInputIndex(), input);
  }

  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE {
    ArrayRef<HUserRecord<HInstruction*>> input_records = HInvoke::GetInputRecords();
    if (kIsDebugBuild && IsStaticWithExplicitClinitCheck()) {
      DCHECK(!input_records.empty());
      DCHECK_GT(input_records.size(), GetNumberOfArguments());
      HInstruction* last_input = input_records.back().GetInstruction();
      // Note: `last_input` may be null during arguments setup.
      if (last_input != nullptr) {
        // `last_input` is the last input of a static invoke marked as having
        // an explicit clinit check. It must either be:
        // - an art::HClinitCheck instruction, set by art::HGraphBuilder; or
        // - an art::HLoadClass instruction, set by art::PrepareForRegisterAllocation.
        DCHECK(last_input->IsClinitCheck() || last_input->IsLoadClass()) << last_input->DebugName();
      }
    }
    return input_records;
  }

  bool CanDoImplicitNullCheckOn(HInstruction* obj ATTRIBUTE_UNUSED) const OVERRIDE {
    // We access the method via the dex cache so we can't do an implicit null check.
    // TODO: for intrinsics we can generate implicit null checks.
    return false;
  }

  bool CanBeNull() const OVERRIDE {
    return GetPackedField<ReturnTypeField>() == DataType::Type::kReference && !IsStringInit();
  }

  // Get the index of the special input, if any.
  //
  // If the invoke HasCurrentMethodInput(), the "special input" is the current
  // method pointer; otherwise there may be one platform-specific special input,
  // such as PC-relative addressing base.
  uint32_t GetSpecialInputIndex() const { return GetNumberOfArguments(); }
  bool HasSpecialInput() const { return GetNumberOfArguments() != InputCount(); }

  MethodLoadKind GetMethodLoadKind() const { return dispatch_info_.method_load_kind; }
  CodePtrLocation GetCodePtrLocation() const { return dispatch_info_.code_ptr_location; }
  bool IsRecursive() const { return GetMethodLoadKind() == MethodLoadKind::kRecursive; }
  bool NeedsDexCacheOfDeclaringClass() const OVERRIDE;
  bool IsStringInit() const { return GetMethodLoadKind() == MethodLoadKind::kStringInit; }
  bool HasMethodAddress() const { return GetMethodLoadKind() == MethodLoadKind::kDirectAddress; }
  bool HasPcRelativeMethodLoadKind() const {
    return GetMethodLoadKind() == MethodLoadKind::kBootImageLinkTimePcRelative ||
           GetMethodLoadKind() == MethodLoadKind::kBssEntry;
  }
  bool HasCurrentMethodInput() const {
    // This function can be called only after the invoke has been fully initialized by the builder.
    if (NeedsCurrentMethodInput(GetMethodLoadKind())) {
      DCHECK(InputAt(GetSpecialInputIndex())->IsCurrentMethod());
      return true;
    } else {
      DCHECK(InputCount() == GetSpecialInputIndex() ||
             !InputAt(GetSpecialInputIndex())->IsCurrentMethod());
      return false;
    }
  }

  QuickEntrypointEnum GetStringInitEntryPoint() const {
    DCHECK(IsStringInit());
    return static_cast<QuickEntrypointEnum>(dispatch_info_.method_load_data);
  }

  uint64_t GetMethodAddress() const {
    DCHECK(HasMethodAddress());
    return dispatch_info_.method_load_data;
  }

  const DexFile& GetDexFileForPcRelativeDexCache() const;

  ClinitCheckRequirement GetClinitCheckRequirement() const {
    return GetPackedField<ClinitCheckRequirementField>();
  }

  // Is this instruction a call to a static method?
  bool IsStatic() const {
    return GetInvokeType() == kStatic;
  }

  MethodReference GetTargetMethod() const {
    return target_method_;
  }

  // Remove the HClinitCheck or the replacement HLoadClass (set as last input by
  // PrepareForRegisterAllocation::VisitClinitCheck() in lieu of the initial HClinitCheck)
  // instruction; only relevant for static calls with explicit clinit check.
  void RemoveExplicitClinitCheck(ClinitCheckRequirement new_requirement) {
    DCHECK(IsStaticWithExplicitClinitCheck());
    size_t last_input_index = inputs_.size() - 1u;
    HInstruction* last_input = inputs_.back().GetInstruction();
    DCHECK(last_input != nullptr);
    DCHECK(last_input->IsLoadClass() || last_input->IsClinitCheck()) << last_input->DebugName();
    RemoveAsUserOfInput(last_input_index);
    inputs_.pop_back();
    SetPackedField<ClinitCheckRequirementField>(new_requirement);
    DCHECK(!IsStaticWithExplicitClinitCheck());
  }

  // Is this a call to a static method whose declaring class has an
  // explicit initialization check in the graph?
  bool IsStaticWithExplicitClinitCheck() const {
    return IsStatic() && (GetClinitCheckRequirement() == ClinitCheckRequirement::kExplicit);
  }

  // Is this a call to a static method whose declaring class has an
  // implicit intialization check requirement?
  bool IsStaticWithImplicitClinitCheck() const {
    return IsStatic() && (GetClinitCheckRequirement() == ClinitCheckRequirement::kImplicit);
  }

  // Does this method load kind need the current method as an input?
  static bool NeedsCurrentMethodInput(MethodLoadKind kind) {
    return kind == MethodLoadKind::kRecursive || kind == MethodLoadKind::kRuntimeCall;
  }

  DECLARE_INSTRUCTION(InvokeStaticOrDirect);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InvokeStaticOrDirect);

 private:
  static constexpr size_t kFieldClinitCheckRequirement = kNumberOfInvokePackedBits;
  static constexpr size_t kFieldClinitCheckRequirementSize =
      MinimumBitsToStore(static_cast<size_t>(ClinitCheckRequirement::kLast));
  static constexpr size_t kNumberOfInvokeStaticOrDirectPackedBits =
      kFieldClinitCheckRequirement + kFieldClinitCheckRequirementSize;
  static_assert(kNumberOfInvokeStaticOrDirectPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using ClinitCheckRequirementField = BitField<ClinitCheckRequirement,
                                               kFieldClinitCheckRequirement,
                                               kFieldClinitCheckRequirementSize>;

  // Cached values of the resolved method, to avoid needing the mutator lock.
  const MethodReference target_method_;
  DispatchInfo dispatch_info_;
};
std::ostream& operator<<(std::ostream& os, HInvokeStaticOrDirect::MethodLoadKind rhs);
std::ostream& operator<<(std::ostream& os, HInvokeStaticOrDirect::ClinitCheckRequirement rhs);

class HInvokeVirtual FINAL : public HInvoke {
 public:
  HInvokeVirtual(ArenaAllocator* allocator,
                 uint32_t number_of_arguments,
                 DataType::Type return_type,
                 uint32_t dex_pc,
                 uint32_t dex_method_index,
                 ArtMethod* resolved_method,
                 uint32_t vtable_index)
      : HInvoke(kInvokeVirtual,
                allocator,
                number_of_arguments,
                0u,
                return_type,
                dex_pc,
                dex_method_index,
                resolved_method,
                kVirtual),
        vtable_index_(vtable_index) {
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool CanBeNull() const OVERRIDE {
    switch (GetIntrinsic()) {
      case Intrinsics::kThreadCurrentThread:
      case Intrinsics::kStringBufferAppend:
      case Intrinsics::kStringBufferToString:
      case Intrinsics::kStringBuilderAppend:
      case Intrinsics::kStringBuilderToString:
        return false;
      default:
        return HInvoke::CanBeNull();
    }
  }

  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    // TODO: Add implicit null checks in intrinsics.
    return (obj == InputAt(0)) && !GetLocations()->Intrinsified();
  }

  uint32_t GetVTableIndex() const { return vtable_index_; }

  DECLARE_INSTRUCTION(InvokeVirtual);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InvokeVirtual);

 private:
  // Cached value of the resolved method, to avoid needing the mutator lock.
  const uint32_t vtable_index_;
};

class HInvokeInterface FINAL : public HInvoke {
 public:
  HInvokeInterface(ArenaAllocator* allocator,
                   uint32_t number_of_arguments,
                   DataType::Type return_type,
                   uint32_t dex_pc,
                   uint32_t dex_method_index,
                   ArtMethod* resolved_method,
                   uint32_t imt_index)
      : HInvoke(kInvokeInterface,
                allocator,
                number_of_arguments,
                0u,
                return_type,
                dex_pc,
                dex_method_index,
                resolved_method,
                kInterface),
        imt_index_(imt_index) {
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    // TODO: Add implicit null checks in intrinsics.
    return (obj == InputAt(0)) && !GetLocations()->Intrinsified();
  }

  bool NeedsDexCacheOfDeclaringClass() const OVERRIDE {
    // The assembly stub currently needs it.
    return true;
  }

  uint32_t GetImtIndex() const { return imt_index_; }

  DECLARE_INSTRUCTION(InvokeInterface);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InvokeInterface);

 private:
  // Cached value of the resolved method, to avoid needing the mutator lock.
  const uint32_t imt_index_;
};

class HNeg FINAL : public HUnaryOperation {
 public:
  HNeg(DataType::Type result_type, HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HUnaryOperation(kNeg, result_type, input, dex_pc) {
    DCHECK_EQ(result_type, DataType::Kind(input->GetType()));
  }

  template <typename T> static T Compute(T x) { return -x; }

  HConstant* Evaluate(HIntConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(Compute(x->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Neg);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Neg);
};

class HNewArray FINAL : public HExpression<2> {
 public:
  HNewArray(HInstruction* cls, HInstruction* length, uint32_t dex_pc)
      : HExpression(kNewArray, DataType::Type::kReference, SideEffects::CanTriggerGC(), dex_pc) {
    SetRawInputAt(0, cls);
    SetRawInputAt(1, length);
  }

  bool IsClonable() const OVERRIDE { return true; }

  // Calls runtime so needs an environment.
  bool NeedsEnvironment() const OVERRIDE { return true; }

  // May throw NegativeArraySizeException, OutOfMemoryError, etc.
  bool CanThrow() const OVERRIDE { return true; }

  bool CanBeNull() const OVERRIDE { return false; }

  HLoadClass* GetLoadClass() const {
    DCHECK(InputAt(0)->IsLoadClass());
    return InputAt(0)->AsLoadClass();
  }

  HInstruction* GetLength() const {
    return InputAt(1);
  }

  DECLARE_INSTRUCTION(NewArray);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NewArray);
};

class HAdd FINAL : public HBinaryOperation {
 public:
  HAdd(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kAdd, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  template <typename T> static T Compute(T x, T y) { return x + y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Add);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Add);
};

class HSub FINAL : public HBinaryOperation {
 public:
  HSub(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kSub, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  template <typename T> static T Compute(T x, T y) { return x - y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Sub);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Sub);
};

class HMul FINAL : public HBinaryOperation {
 public:
  HMul(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kMul, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  template <typename T> static T Compute(T x, T y) { return x * y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Mul);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Mul);
};

class HDiv FINAL : public HBinaryOperation {
 public:
  HDiv(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc)
      : HBinaryOperation(kDiv, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  template <typename T>
  T ComputeIntegral(T x, T y) const {
    DCHECK(!DataType::IsFloatingPointType(GetType())) << GetType();
    // Our graph structure ensures we never have 0 for `y` during
    // constant folding.
    DCHECK_NE(y, 0);
    // Special case -1 to avoid getting a SIGFPE on x86(_64).
    return (y == -1) ? -x : x / y;
  }

  template <typename T>
  T ComputeFP(T x, T y) const {
    DCHECK(DataType::IsFloatingPointType(GetType())) << GetType();
    return x / y;
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        ComputeIntegral(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        ComputeIntegral(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(
        ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(
        ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Div);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Div);
};

class HRem FINAL : public HBinaryOperation {
 public:
  HRem(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc)
      : HBinaryOperation(kRem, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  template <typename T>
  T ComputeIntegral(T x, T y) const {
    DCHECK(!DataType::IsFloatingPointType(GetType())) << GetType();
    // Our graph structure ensures we never have 0 for `y` during
    // constant folding.
    DCHECK_NE(y, 0);
    // Special case -1 to avoid getting a SIGFPE on x86(_64).
    return (y == -1) ? 0 : x % y;
  }

  template <typename T>
  T ComputeFP(T x, T y) const {
    DCHECK(DataType::IsFloatingPointType(GetType())) << GetType();
    return std::fmod(x, y);
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        ComputeIntegral(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        ComputeIntegral(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x, HFloatConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetFloatConstant(
        ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HDoubleConstant* x, HDoubleConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetDoubleConstant(
        ComputeFP(x->GetValue(), y->GetValue()), GetDexPc());
  }

  DECLARE_INSTRUCTION(Rem);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Rem);
};

class HDivZeroCheck FINAL : public HExpression<1> {
 public:
  // `HDivZeroCheck` can trigger GC, as it may call the `ArithmeticException`
  // constructor.
  HDivZeroCheck(HInstruction* value, uint32_t dex_pc)
      : HExpression(kDivZeroCheck, value->GetType(), SideEffects::CanTriggerGC(), dex_pc) {
    SetRawInputAt(0, value);
  }

  DataType::Type GetType() const OVERRIDE { return InputAt(0)->GetType(); }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE { return true; }
  bool CanThrow() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(DivZeroCheck);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(DivZeroCheck);
};

class HShl FINAL : public HBinaryOperation {
 public:
  HShl(DataType::Type result_type,
       HInstruction* value,
       HInstruction* distance,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kShl, result_type, value, distance, SideEffects::None(), dex_pc) {
    DCHECK_EQ(result_type, DataType::Kind(value->GetType()));
    DCHECK_EQ(DataType::Type::kInt32, DataType::Kind(distance->GetType()));
  }

  template <typename T>
  static T Compute(T value, int32_t distance, int32_t max_shift_distance) {
    return value << (distance & max_shift_distance);
  }

  HConstant* Evaluate(HIntConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxIntShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxLongShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value ATTRIBUTE_UNUSED,
                      HLongConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for the (long, long) case.";
    UNREACHABLE();
  }
  HConstant* Evaluate(HFloatConstant* value ATTRIBUTE_UNUSED,
                      HFloatConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* value ATTRIBUTE_UNUSED,
                      HDoubleConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Shl);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Shl);
};

class HShr FINAL : public HBinaryOperation {
 public:
  HShr(DataType::Type result_type,
       HInstruction* value,
       HInstruction* distance,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kShr, result_type, value, distance, SideEffects::None(), dex_pc) {
    DCHECK_EQ(result_type, DataType::Kind(value->GetType()));
    DCHECK_EQ(DataType::Type::kInt32, DataType::Kind(distance->GetType()));
  }

  template <typename T>
  static T Compute(T value, int32_t distance, int32_t max_shift_distance) {
    return value >> (distance & max_shift_distance);
  }

  HConstant* Evaluate(HIntConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxIntShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxLongShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value ATTRIBUTE_UNUSED,
                      HLongConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for the (long, long) case.";
    UNREACHABLE();
  }
  HConstant* Evaluate(HFloatConstant* value ATTRIBUTE_UNUSED,
                      HFloatConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* value ATTRIBUTE_UNUSED,
                      HDoubleConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Shr);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Shr);
};

class HUShr FINAL : public HBinaryOperation {
 public:
  HUShr(DataType::Type result_type,
        HInstruction* value,
        HInstruction* distance,
        uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kUShr, result_type, value, distance, SideEffects::None(), dex_pc) {
    DCHECK_EQ(result_type, DataType::Kind(value->GetType()));
    DCHECK_EQ(DataType::Type::kInt32, DataType::Kind(distance->GetType()));
  }

  template <typename T>
  static T Compute(T value, int32_t distance, int32_t max_shift_distance) {
    typedef typename std::make_unsigned<T>::type V;
    V ux = static_cast<V>(value);
    return static_cast<T>(ux >> (distance & max_shift_distance));
  }

  HConstant* Evaluate(HIntConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxIntShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxLongShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value ATTRIBUTE_UNUSED,
                      HLongConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for the (long, long) case.";
    UNREACHABLE();
  }
  HConstant* Evaluate(HFloatConstant* value ATTRIBUTE_UNUSED,
                      HFloatConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* value ATTRIBUTE_UNUSED,
                      HDoubleConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(UShr);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UShr);
};

class HAnd FINAL : public HBinaryOperation {
 public:
  HAnd(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kAnd, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  template <typename T> static T Compute(T x, T y) { return x & y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(And);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(And);
};

class HOr FINAL : public HBinaryOperation {
 public:
  HOr(DataType::Type result_type,
      HInstruction* left,
      HInstruction* right,
      uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kOr, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  template <typename T> static T Compute(T x, T y) { return x | y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Or);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Or);
};

class HXor FINAL : public HBinaryOperation {
 public:
  HXor(DataType::Type result_type,
       HInstruction* left,
       HInstruction* right,
       uint32_t dex_pc = kNoDexPc)
      : HBinaryOperation(kXor, result_type, left, right, SideEffects::None(), dex_pc) {
  }

  bool IsCommutative() const OVERRIDE { return true; }

  template <typename T> static T Compute(T x, T y) { return x ^ y; }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Xor);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Xor);
};

class HRor FINAL : public HBinaryOperation {
 public:
  HRor(DataType::Type result_type, HInstruction* value, HInstruction* distance)
      : HBinaryOperation(kRor, result_type, value, distance) {
    DCHECK_EQ(result_type, DataType::Kind(value->GetType()));
    DCHECK_EQ(DataType::Type::kInt32, DataType::Kind(distance->GetType()));
  }

  template <typename T>
  static T Compute(T value, int32_t distance, int32_t max_shift_value) {
    typedef typename std::make_unsigned<T>::type V;
    V ux = static_cast<V>(value);
    if ((distance & max_shift_value) == 0) {
      return static_cast<T>(ux);
    } else {
      const V reg_bits = sizeof(T) * 8;
      return static_cast<T>(ux >> (distance & max_shift_value)) |
                           (value << (reg_bits - (distance & max_shift_value)));
    }
  }

  HConstant* Evaluate(HIntConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxIntShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value, HIntConstant* distance) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(value->GetValue(), distance->GetValue(), kMaxLongShiftDistance), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* value ATTRIBUTE_UNUSED,
                      HLongConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for the (long, long) case.";
    UNREACHABLE();
  }
  HConstant* Evaluate(HFloatConstant* value ATTRIBUTE_UNUSED,
                      HFloatConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* value ATTRIBUTE_UNUSED,
                      HDoubleConstant* distance ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Ror);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Ror);
};

// The value of a parameter in this method. Its location depends on
// the calling convention.
class HParameterValue FINAL : public HExpression<0> {
 public:
  HParameterValue(const DexFile& dex_file,
                  dex::TypeIndex type_index,
                  uint8_t index,
                  DataType::Type parameter_type,
                  bool is_this = false)
      : HExpression(kParameterValue, parameter_type, SideEffects::None(), kNoDexPc),
        dex_file_(dex_file),
        type_index_(type_index),
        index_(index) {
    SetPackedFlag<kFlagIsThis>(is_this);
    SetPackedFlag<kFlagCanBeNull>(!is_this);
  }

  const DexFile& GetDexFile() const { return dex_file_; }
  dex::TypeIndex GetTypeIndex() const { return type_index_; }
  uint8_t GetIndex() const { return index_; }
  bool IsThis() const { return GetPackedFlag<kFlagIsThis>(); }

  bool CanBeNull() const OVERRIDE { return GetPackedFlag<kFlagCanBeNull>(); }
  void SetCanBeNull(bool can_be_null) { SetPackedFlag<kFlagCanBeNull>(can_be_null); }

  DECLARE_INSTRUCTION(ParameterValue);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ParameterValue);

 private:
  // Whether or not the parameter value corresponds to 'this' argument.
  static constexpr size_t kFlagIsThis = kNumberOfExpressionPackedBits;
  static constexpr size_t kFlagCanBeNull = kFlagIsThis + 1;
  static constexpr size_t kNumberOfParameterValuePackedBits = kFlagCanBeNull + 1;
  static_assert(kNumberOfParameterValuePackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");

  const DexFile& dex_file_;
  const dex::TypeIndex type_index_;
  // The index of this parameter in the parameters list. Must be less
  // than HGraph::number_of_in_vregs_.
  const uint8_t index_;
};

class HNot FINAL : public HUnaryOperation {
 public:
  HNot(DataType::Type result_type, HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HUnaryOperation(kNot, result_type, input, dex_pc) {
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  template <typename T> static T Compute(T x) { return ~x; }

  HConstant* Evaluate(HIntConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(Not);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Not);
};

class HBooleanNot FINAL : public HUnaryOperation {
 public:
  explicit HBooleanNot(HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HUnaryOperation(kBooleanNot, DataType::Type::kBool, input, dex_pc) {
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  template <typename T> static bool Compute(T x) {
    DCHECK(IsUint<1>(x)) << x;
    return !x;
  }

  HConstant* Evaluate(HIntConstant* x) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(Compute(x->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for long values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  DECLARE_INSTRUCTION(BooleanNot);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BooleanNot);
};

class HTypeConversion FINAL : public HExpression<1> {
 public:
  // Instantiate a type conversion of `input` to `result_type`.
  HTypeConversion(DataType::Type result_type, HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HExpression(kTypeConversion, result_type, SideEffects::None(), dex_pc) {
    SetRawInputAt(0, input);
    // Invariant: We should never generate a conversion to a Boolean value.
    DCHECK_NE(DataType::Type::kBool, result_type);
  }

  HInstruction* GetInput() const { return InputAt(0); }
  DataType::Type GetInputType() const { return GetInput()->GetType(); }
  DataType::Type GetResultType() const { return GetType(); }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  // Try to statically evaluate the conversion and return a HConstant
  // containing the result.  If the input cannot be converted, return nullptr.
  HConstant* TryStaticEvaluation() const;

  DECLARE_INSTRUCTION(TypeConversion);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(TypeConversion);
};

static constexpr uint32_t kNoRegNumber = -1;

class HNullCheck FINAL : public HExpression<1> {
 public:
  // `HNullCheck` can trigger GC, as it may call the `NullPointerException`
  // constructor.
  HNullCheck(HInstruction* value, uint32_t dex_pc)
      : HExpression(kNullCheck, value->GetType(), SideEffects::CanTriggerGC(), dex_pc) {
    SetRawInputAt(0, value);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE { return true; }

  bool CanBeNull() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(NullCheck);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NullCheck);
};

// Embeds an ArtField and all the information required by the compiler. We cache
// that information to avoid requiring the mutator lock every time we need it.
class FieldInfo : public ValueObject {
 public:
  FieldInfo(ArtField* field,
            MemberOffset field_offset,
            DataType::Type field_type,
            bool is_volatile,
            uint32_t index,
            uint16_t declaring_class_def_index,
            const DexFile& dex_file)
      : field_(field),
        field_offset_(field_offset),
        field_type_(field_type),
        is_volatile_(is_volatile),
        index_(index),
        declaring_class_def_index_(declaring_class_def_index),
        dex_file_(dex_file) {}

  ArtField* GetField() const { return field_; }
  MemberOffset GetFieldOffset() const { return field_offset_; }
  DataType::Type GetFieldType() const { return field_type_; }
  uint32_t GetFieldIndex() const { return index_; }
  uint16_t GetDeclaringClassDefIndex() const { return declaring_class_def_index_;}
  const DexFile& GetDexFile() const { return dex_file_; }
  bool IsVolatile() const { return is_volatile_; }

 private:
  ArtField* const field_;
  const MemberOffset field_offset_;
  const DataType::Type field_type_;
  const bool is_volatile_;
  const uint32_t index_;
  const uint16_t declaring_class_def_index_;
  const DexFile& dex_file_;
};

class HInstanceFieldGet FINAL : public HExpression<1> {
 public:
  HInstanceFieldGet(HInstruction* value,
                    ArtField* field,
                    DataType::Type field_type,
                    MemberOffset field_offset,
                    bool is_volatile,
                    uint32_t field_idx,
                    uint16_t declaring_class_def_index,
                    const DexFile& dex_file,
                    uint32_t dex_pc)
      : HExpression(kInstanceFieldGet,
                    field_type,
                    SideEffects::FieldReadOfType(field_type, is_volatile),
                    dex_pc),
        field_info_(field,
                    field_offset,
                    field_type,
                    is_volatile,
                    field_idx,
                    declaring_class_def_index,
                    dex_file) {
    SetRawInputAt(0, value);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return !IsVolatile(); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    const HInstanceFieldGet* other_get = other->AsInstanceFieldGet();
    return GetFieldOffset().SizeValue() == other_get->GetFieldOffset().SizeValue();
  }

  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    return (obj == InputAt(0)) && art::CanDoImplicitNullCheckOn(GetFieldOffset().Uint32Value());
  }

  size_t ComputeHashCode() const OVERRIDE {
    return (HInstruction::ComputeHashCode() << 7) | GetFieldOffset().SizeValue();
  }

  const FieldInfo& GetFieldInfo() const { return field_info_; }
  MemberOffset GetFieldOffset() const { return field_info_.GetFieldOffset(); }
  DataType::Type GetFieldType() const { return field_info_.GetFieldType(); }
  bool IsVolatile() const { return field_info_.IsVolatile(); }

  void SetType(DataType::Type new_type) {
    DCHECK(DataType::IsIntegralType(GetType()));
    DCHECK(DataType::IsIntegralType(new_type));
    DCHECK_EQ(DataType::Size(GetType()), DataType::Size(new_type));
    SetPackedField<TypeField>(new_type);
  }

  DECLARE_INSTRUCTION(InstanceFieldGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InstanceFieldGet);

 private:
  const FieldInfo field_info_;
};

class HInstanceFieldSet FINAL : public HTemplateInstruction<2> {
 public:
  HInstanceFieldSet(HInstruction* object,
                    HInstruction* value,
                    ArtField* field,
                    DataType::Type field_type,
                    MemberOffset field_offset,
                    bool is_volatile,
                    uint32_t field_idx,
                    uint16_t declaring_class_def_index,
                    const DexFile& dex_file,
                    uint32_t dex_pc)
      : HTemplateInstruction(kInstanceFieldSet,
                             SideEffects::FieldWriteOfType(field_type, is_volatile),
                             dex_pc),
        field_info_(field,
                    field_offset,
                    field_type,
                    is_volatile,
                    field_idx,
                    declaring_class_def_index,
                    dex_file) {
    SetPackedFlag<kFlagValueCanBeNull>(true);
    SetRawInputAt(0, object);
    SetRawInputAt(1, value);
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    return (obj == InputAt(0)) && art::CanDoImplicitNullCheckOn(GetFieldOffset().Uint32Value());
  }

  const FieldInfo& GetFieldInfo() const { return field_info_; }
  MemberOffset GetFieldOffset() const { return field_info_.GetFieldOffset(); }
  DataType::Type GetFieldType() const { return field_info_.GetFieldType(); }
  bool IsVolatile() const { return field_info_.IsVolatile(); }
  HInstruction* GetValue() const { return InputAt(1); }
  bool GetValueCanBeNull() const { return GetPackedFlag<kFlagValueCanBeNull>(); }
  void ClearValueCanBeNull() { SetPackedFlag<kFlagValueCanBeNull>(false); }

  DECLARE_INSTRUCTION(InstanceFieldSet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InstanceFieldSet);

 private:
  static constexpr size_t kFlagValueCanBeNull = kNumberOfGenericPackedBits;
  static constexpr size_t kNumberOfInstanceFieldSetPackedBits = kFlagValueCanBeNull + 1;
  static_assert(kNumberOfInstanceFieldSetPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");

  const FieldInfo field_info_;
};

class HArrayGet FINAL : public HExpression<2> {
 public:
  HArrayGet(HInstruction* array,
            HInstruction* index,
            DataType::Type type,
            uint32_t dex_pc)
     : HArrayGet(array,
                 index,
                 type,
                 SideEffects::ArrayReadOfType(type),
                 dex_pc,
                 /* is_string_char_at */ false) {
  }

  HArrayGet(HInstruction* array,
            HInstruction* index,
            DataType::Type type,
            SideEffects side_effects,
            uint32_t dex_pc,
            bool is_string_char_at)
      : HExpression(kArrayGet, type, side_effects, dex_pc) {
    SetPackedFlag<kFlagIsStringCharAt>(is_string_char_at);
    SetRawInputAt(0, array);
    SetRawInputAt(1, index);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }
  bool CanDoImplicitNullCheckOn(HInstruction* obj ATTRIBUTE_UNUSED) const OVERRIDE {
    // TODO: We can be smarter here.
    // Currently, unless the array is the result of NewArray, the array access is always
    // preceded by some form of null NullCheck necessary for the bounds check, usually
    // implicit null check on the ArrayLength input to BoundsCheck or Deoptimize for
    // dynamic BCE. There are cases when these could be removed to produce better code.
    // If we ever add optimizations to do so we should allow an implicit check here
    // (as long as the address falls in the first page).
    //
    // As an example of such fancy optimization, we could eliminate BoundsCheck for
    //     a = cond ? new int[1] : null;
    //     a[0];  // The Phi does not need bounds check for either input.
    return false;
  }

  bool IsEquivalentOf(HArrayGet* other) const {
    bool result = (GetDexPc() == other->GetDexPc());
    if (kIsDebugBuild && result) {
      DCHECK_EQ(GetBlock(), other->GetBlock());
      DCHECK_EQ(GetArray(), other->GetArray());
      DCHECK_EQ(GetIndex(), other->GetIndex());
      if (DataType::IsIntOrLongType(GetType())) {
        DCHECK(DataType::IsFloatingPointType(other->GetType())) << other->GetType();
      } else {
        DCHECK(DataType::IsFloatingPointType(GetType())) << GetType();
        DCHECK(DataType::IsIntOrLongType(other->GetType())) << other->GetType();
      }
    }
    return result;
  }

  bool IsStringCharAt() const { return GetPackedFlag<kFlagIsStringCharAt>(); }

  HInstruction* GetArray() const { return InputAt(0); }
  HInstruction* GetIndex() const { return InputAt(1); }

  void SetType(DataType::Type new_type) {
    DCHECK(DataType::IsIntegralType(GetType()));
    DCHECK(DataType::IsIntegralType(new_type));
    DCHECK_EQ(DataType::Size(GetType()), DataType::Size(new_type));
    SetPackedField<TypeField>(new_type);
  }

  DECLARE_INSTRUCTION(ArrayGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ArrayGet);

 private:
  // We treat a String as an array, creating the HArrayGet from String.charAt()
  // intrinsic in the instruction simplifier. We can always determine whether
  // a particular HArrayGet is actually a String.charAt() by looking at the type
  // of the input but that requires holding the mutator lock, so we prefer to use
  // a flag, so that code generators don't need to do the locking.
  static constexpr size_t kFlagIsStringCharAt = kNumberOfExpressionPackedBits;
  static constexpr size_t kNumberOfArrayGetPackedBits = kFlagIsStringCharAt + 1;
  static_assert(kNumberOfArrayGetPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
};

class HArraySet FINAL : public HTemplateInstruction<3> {
 public:
  HArraySet(HInstruction* array,
            HInstruction* index,
            HInstruction* value,
            DataType::Type expected_component_type,
            uint32_t dex_pc)
      : HArraySet(array,
                  index,
                  value,
                  expected_component_type,
                  // Make a best guess for side effects now, may be refined during SSA building.
                  ComputeSideEffects(GetComponentType(value->GetType(), expected_component_type)),
                  dex_pc) {
  }

  HArraySet(HInstruction* array,
            HInstruction* index,
            HInstruction* value,
            DataType::Type expected_component_type,
            SideEffects side_effects,
            uint32_t dex_pc)
      : HTemplateInstruction(kArraySet, side_effects, dex_pc) {
    SetPackedField<ExpectedComponentTypeField>(expected_component_type);
    SetPackedFlag<kFlagNeedsTypeCheck>(value->GetType() == DataType::Type::kReference);
    SetPackedFlag<kFlagValueCanBeNull>(true);
    SetPackedFlag<kFlagStaticTypeOfArrayIsObjectArray>(false);
    SetRawInputAt(0, array);
    SetRawInputAt(1, index);
    SetRawInputAt(2, value);
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool NeedsEnvironment() const OVERRIDE {
    // We call a runtime method to throw ArrayStoreException.
    return NeedsTypeCheck();
  }

  // Can throw ArrayStoreException.
  bool CanThrow() const OVERRIDE { return NeedsTypeCheck(); }

  bool CanDoImplicitNullCheckOn(HInstruction* obj ATTRIBUTE_UNUSED) const OVERRIDE {
    // TODO: Same as for ArrayGet.
    return false;
  }

  void ClearNeedsTypeCheck() {
    SetPackedFlag<kFlagNeedsTypeCheck>(false);
  }

  void ClearValueCanBeNull() {
    SetPackedFlag<kFlagValueCanBeNull>(false);
  }

  void SetStaticTypeOfArrayIsObjectArray() {
    SetPackedFlag<kFlagStaticTypeOfArrayIsObjectArray>(true);
  }

  bool GetValueCanBeNull() const { return GetPackedFlag<kFlagValueCanBeNull>(); }
  bool NeedsTypeCheck() const { return GetPackedFlag<kFlagNeedsTypeCheck>(); }
  bool StaticTypeOfArrayIsObjectArray() const {
    return GetPackedFlag<kFlagStaticTypeOfArrayIsObjectArray>();
  }

  HInstruction* GetArray() const { return InputAt(0); }
  HInstruction* GetIndex() const { return InputAt(1); }
  HInstruction* GetValue() const { return InputAt(2); }

  DataType::Type GetComponentType() const {
    return GetComponentType(GetValue()->GetType(), GetRawExpectedComponentType());
  }

  static DataType::Type GetComponentType(DataType::Type value_type,
                                         DataType::Type expected_component_type) {
    // The Dex format does not type floating point index operations. Since the
    // `expected_component_type` comes from SSA building and can therefore not
    // be correct, we also check what is the value type. If it is a floating
    // point type, we must use that type.
    return ((value_type == DataType::Type::kFloat32) || (value_type == DataType::Type::kFloat64))
        ? value_type
        : expected_component_type;
  }

  DataType::Type GetRawExpectedComponentType() const {
    return GetPackedField<ExpectedComponentTypeField>();
  }

  static SideEffects ComputeSideEffects(DataType::Type type) {
    return SideEffects::ArrayWriteOfType(type).Union(SideEffectsForArchRuntimeCalls(type));
  }

  static SideEffects SideEffectsForArchRuntimeCalls(DataType::Type value_type) {
    return (value_type == DataType::Type::kReference) ? SideEffects::CanTriggerGC()
                                                      : SideEffects::None();
  }

  DECLARE_INSTRUCTION(ArraySet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ArraySet);

 private:
  static constexpr size_t kFieldExpectedComponentType = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldExpectedComponentTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kFlagNeedsTypeCheck =
      kFieldExpectedComponentType + kFieldExpectedComponentTypeSize;
  static constexpr size_t kFlagValueCanBeNull = kFlagNeedsTypeCheck + 1;
  // Cached information for the reference_type_info_ so that codegen
  // does not need to inspect the static type.
  static constexpr size_t kFlagStaticTypeOfArrayIsObjectArray = kFlagValueCanBeNull + 1;
  static constexpr size_t kNumberOfArraySetPackedBits =
      kFlagStaticTypeOfArrayIsObjectArray + 1;
  static_assert(kNumberOfArraySetPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using ExpectedComponentTypeField =
      BitField<DataType::Type, kFieldExpectedComponentType, kFieldExpectedComponentTypeSize>;
};

class HArrayLength FINAL : public HExpression<1> {
 public:
  HArrayLength(HInstruction* array, uint32_t dex_pc, bool is_string_length = false)
      : HExpression(kArrayLength, DataType::Type::kInt32, SideEffects::None(), dex_pc) {
    SetPackedFlag<kFlagIsStringLength>(is_string_length);
    // Note that arrays do not change length, so the instruction does not
    // depend on any write.
    SetRawInputAt(0, array);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }
  bool CanDoImplicitNullCheckOn(HInstruction* obj) const OVERRIDE {
    return obj == InputAt(0);
  }

  bool IsStringLength() const { return GetPackedFlag<kFlagIsStringLength>(); }

  DECLARE_INSTRUCTION(ArrayLength);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ArrayLength);

 private:
  // We treat a String as an array, creating the HArrayLength from String.length()
  // or String.isEmpty() intrinsic in the instruction simplifier. We can always
  // determine whether a particular HArrayLength is actually a String.length() by
  // looking at the type of the input but that requires holding the mutator lock, so
  // we prefer to use a flag, so that code generators don't need to do the locking.
  static constexpr size_t kFlagIsStringLength = kNumberOfExpressionPackedBits;
  static constexpr size_t kNumberOfArrayLengthPackedBits = kFlagIsStringLength + 1;
  static_assert(kNumberOfArrayLengthPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
};

class HBoundsCheck FINAL : public HExpression<2> {
 public:
  // `HBoundsCheck` can trigger GC, as it may call the `IndexOutOfBoundsException`
  // constructor.
  HBoundsCheck(HInstruction* index,
               HInstruction* length,
               uint32_t dex_pc,
               bool is_string_char_at = false)
      : HExpression(kBoundsCheck, index->GetType(), SideEffects::CanTriggerGC(), dex_pc) {
    DCHECK_EQ(DataType::Type::kInt32, DataType::Kind(index->GetType()));
    SetPackedFlag<kFlagIsStringCharAt>(is_string_char_at);
    SetRawInputAt(0, index);
    SetRawInputAt(1, length);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE { return true; }

  bool IsStringCharAt() const { return GetPackedFlag<kFlagIsStringCharAt>(); }

  HInstruction* GetIndex() const { return InputAt(0); }

  DECLARE_INSTRUCTION(BoundsCheck);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BoundsCheck);

 private:
  static constexpr size_t kFlagIsStringCharAt = kNumberOfExpressionPackedBits;
};

class HSuspendCheck FINAL : public HTemplateInstruction<0> {
 public:
  explicit HSuspendCheck(uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kSuspendCheck, SideEffects::CanTriggerGC(), dex_pc),
        slow_path_(nullptr) {
  }

  bool IsClonable() const OVERRIDE { return true; }

  bool NeedsEnvironment() const OVERRIDE {
    return true;
  }

  void SetSlowPath(SlowPathCode* slow_path) { slow_path_ = slow_path; }
  SlowPathCode* GetSlowPath() const { return slow_path_; }

  DECLARE_INSTRUCTION(SuspendCheck);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(SuspendCheck);

 private:
  // Only used for code generation, in order to share the same slow path between back edges
  // of a same loop.
  SlowPathCode* slow_path_;
};

// Pseudo-instruction which provides the native debugger with mapping information.
// It ensures that we can generate line number and local variables at this point.
class HNativeDebugInfo : public HTemplateInstruction<0> {
 public:
  explicit HNativeDebugInfo(uint32_t dex_pc)
      : HTemplateInstruction<0>(kNativeDebugInfo, SideEffects::None(), dex_pc) {
  }

  bool NeedsEnvironment() const OVERRIDE {
    return true;
  }

  DECLARE_INSTRUCTION(NativeDebugInfo);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(NativeDebugInfo);
};

/**
 * Instruction to load a Class object.
 */
class HLoadClass FINAL : public HInstruction {
 public:
  // Determines how to load the Class.
  enum class LoadKind {
    // We cannot load this class. See HSharpening::SharpenLoadClass.
    kInvalid = -1,

    // Use the Class* from the method's own ArtMethod*.
    kReferrersClass,

    // Use PC-relative boot image Class* address that will be known at link time.
    // Used for boot image classes referenced by boot image code.
    kBootImageLinkTimePcRelative,

    // Use a known boot image Class* address, embedded in the code by the codegen.
    // Used for boot image classes referenced by apps in AOT- and JIT-compiled code.
    kBootImageAddress,

    // Use a PC-relative load from a boot image ClassTable mmapped into the .bss
    // of the oat file.
    kBootImageClassTable,

    // Load from an entry in the .bss section using a PC-relative load.
    // Used for classes outside boot image when .bss is accessible with a PC-relative load.
    kBssEntry,

    // Load from the root table associated with the JIT compiled method.
    kJitTableAddress,

    // Load using a simple runtime call. This is the fall-back load kind when
    // the codegen is unable to use another appropriate kind.
    kRuntimeCall,

    kLast = kRuntimeCall
  };

  HLoadClass(HCurrentMethod* current_method,
             dex::TypeIndex type_index,
             const DexFile& dex_file,
             Handle<mirror::Class> klass,
             bool is_referrers_class,
             uint32_t dex_pc,
             bool needs_access_check)
      : HInstruction(kLoadClass, SideEffectsForArchRuntimeCalls(), dex_pc),
        special_input_(HUserRecord<HInstruction*>(current_method)),
        type_index_(type_index),
        dex_file_(dex_file),
        klass_(klass),
        loaded_class_rti_(ReferenceTypeInfo::CreateInvalid()) {
    // Referrers class should not need access check. We never inline unverified
    // methods so we can't possibly end up in this situation.
    DCHECK(!is_referrers_class || !needs_access_check);

    SetPackedField<LoadKindField>(
        is_referrers_class ? LoadKind::kReferrersClass : LoadKind::kRuntimeCall);
    SetPackedFlag<kFlagNeedsAccessCheck>(needs_access_check);
    SetPackedFlag<kFlagIsInBootImage>(false);
    SetPackedFlag<kFlagGenerateClInitCheck>(false);
  }

  bool IsClonable() const OVERRIDE { return true; }

  void SetLoadKind(LoadKind load_kind);

  LoadKind GetLoadKind() const {
    return GetPackedField<LoadKindField>();
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const;

  size_t ComputeHashCode() const OVERRIDE { return type_index_.index_; }

  bool CanBeNull() const OVERRIDE { return false; }

  bool NeedsEnvironment() const OVERRIDE {
    return CanCallRuntime();
  }

  void SetMustGenerateClinitCheck(bool generate_clinit_check) {
    // The entrypoint the code generator is going to call does not do
    // clinit of the class.
    DCHECK(!NeedsAccessCheck());
    SetPackedFlag<kFlagGenerateClInitCheck>(generate_clinit_check);
  }

  bool CanCallRuntime() const {
    return NeedsAccessCheck() ||
           MustGenerateClinitCheck() ||
           GetLoadKind() == LoadKind::kRuntimeCall ||
           GetLoadKind() == LoadKind::kBssEntry;
  }

  bool CanThrow() const OVERRIDE {
    return NeedsAccessCheck() ||
           MustGenerateClinitCheck() ||
           // If the class is in the boot image, the lookup in the runtime call cannot throw.
           // This keeps CanThrow() consistent between non-PIC (using kBootImageAddress) and
           // PIC and subsequently avoids a DCE behavior dependency on the PIC option.
           ((GetLoadKind() == LoadKind::kRuntimeCall ||
             GetLoadKind() == LoadKind::kBssEntry) &&
            !IsInBootImage());
  }

  ReferenceTypeInfo GetLoadedClassRTI() {
    return loaded_class_rti_;
  }

  void SetLoadedClassRTI(ReferenceTypeInfo rti) {
    // Make sure we only set exact types (the loaded class should never be merged).
    DCHECK(rti.IsExact());
    loaded_class_rti_ = rti;
  }

  dex::TypeIndex GetTypeIndex() const { return type_index_; }
  const DexFile& GetDexFile() const { return dex_file_; }

  bool NeedsDexCacheOfDeclaringClass() const OVERRIDE {
    return GetLoadKind() == LoadKind::kRuntimeCall;
  }

  static SideEffects SideEffectsForArchRuntimeCalls() {
    return SideEffects::CanTriggerGC();
  }

  bool IsReferrersClass() const { return GetLoadKind() == LoadKind::kReferrersClass; }
  bool NeedsAccessCheck() const { return GetPackedFlag<kFlagNeedsAccessCheck>(); }
  bool IsInBootImage() const { return GetPackedFlag<kFlagIsInBootImage>(); }
  bool MustGenerateClinitCheck() const { return GetPackedFlag<kFlagGenerateClInitCheck>(); }

  void MarkInBootImage() {
    SetPackedFlag<kFlagIsInBootImage>(true);
  }

  void AddSpecialInput(HInstruction* special_input);

  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE FINAL {
    return ArrayRef<HUserRecord<HInstruction*>>(
        &special_input_, (special_input_.GetInstruction() != nullptr) ? 1u : 0u);
  }

  DataType::Type GetType() const OVERRIDE {
    return DataType::Type::kReference;
  }

  Handle<mirror::Class> GetClass() const {
    return klass_;
  }

  DECLARE_INSTRUCTION(LoadClass);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LoadClass);

 private:
  static constexpr size_t kFlagNeedsAccessCheck    = kNumberOfGenericPackedBits;
  static constexpr size_t kFlagIsInBootImage       = kFlagNeedsAccessCheck + 1;
  // Whether this instruction must generate the initialization check.
  // Used for code generation.
  static constexpr size_t kFlagGenerateClInitCheck = kFlagIsInBootImage + 1;
  static constexpr size_t kFieldLoadKind           = kFlagGenerateClInitCheck + 1;
  static constexpr size_t kFieldLoadKindSize =
      MinimumBitsToStore(static_cast<size_t>(LoadKind::kLast));
  static constexpr size_t kNumberOfLoadClassPackedBits = kFieldLoadKind + kFieldLoadKindSize;
  static_assert(kNumberOfLoadClassPackedBits < kMaxNumberOfPackedBits, "Too many packed fields.");
  using LoadKindField = BitField<LoadKind, kFieldLoadKind, kFieldLoadKindSize>;

  static bool HasTypeReference(LoadKind load_kind) {
    return load_kind == LoadKind::kReferrersClass ||
        load_kind == LoadKind::kBootImageLinkTimePcRelative ||
        load_kind == LoadKind::kBootImageClassTable ||
        load_kind == LoadKind::kBssEntry ||
        load_kind == LoadKind::kRuntimeCall;
  }

  void SetLoadKindInternal(LoadKind load_kind);

  // The special input is the HCurrentMethod for kRuntimeCall or kReferrersClass.
  // For other load kinds it's empty or possibly some architecture-specific instruction
  // for PC-relative loads, i.e. kBssEntry or kBootImageLinkTimePcRelative.
  HUserRecord<HInstruction*> special_input_;

  // A type index and dex file where the class can be accessed. The dex file can be:
  // - The compiling method's dex file if the class is defined there too.
  // - The compiling method's dex file if the class is referenced there.
  // - The dex file where the class is defined. When the load kind can only be
  //   kBssEntry or kRuntimeCall, we cannot emit code for this `HLoadClass`.
  const dex::TypeIndex type_index_;
  const DexFile& dex_file_;

  Handle<mirror::Class> klass_;

  ReferenceTypeInfo loaded_class_rti_;
};
std::ostream& operator<<(std::ostream& os, HLoadClass::LoadKind rhs);

// Note: defined outside class to see operator<<(., HLoadClass::LoadKind).
inline void HLoadClass::SetLoadKind(LoadKind load_kind) {
  // The load kind should be determined before inserting the instruction to the graph.
  DCHECK(GetBlock() == nullptr);
  DCHECK(GetEnvironment() == nullptr);
  SetPackedField<LoadKindField>(load_kind);
  if (load_kind != LoadKind::kRuntimeCall && load_kind != LoadKind::kReferrersClass) {
    special_input_ = HUserRecord<HInstruction*>(nullptr);
  }
  if (!NeedsEnvironment()) {
    SetSideEffects(SideEffects::None());
  }
}

// Note: defined outside class to see operator<<(., HLoadClass::LoadKind).
inline void HLoadClass::AddSpecialInput(HInstruction* special_input) {
  // The special input is used for PC-relative loads on some architectures,
  // including literal pool loads, which are PC-relative too.
  DCHECK(GetLoadKind() == LoadKind::kBootImageLinkTimePcRelative ||
         GetLoadKind() == LoadKind::kBootImageAddress ||
         GetLoadKind() == LoadKind::kBootImageClassTable ||
         GetLoadKind() == LoadKind::kBssEntry) << GetLoadKind();
  DCHECK(special_input_.GetInstruction() == nullptr);
  special_input_ = HUserRecord<HInstruction*>(special_input);
  special_input->AddUseAt(this, 0);
}

class HLoadString FINAL : public HInstruction {
 public:
  // Determines how to load the String.
  enum class LoadKind {
    // Use PC-relative boot image String* address that will be known at link time.
    // Used for boot image strings referenced by boot image code.
    kBootImageLinkTimePcRelative,

    // Use a known boot image String* address, embedded in the code by the codegen.
    // Used for boot image strings referenced by apps in AOT- and JIT-compiled code.
    kBootImageAddress,

    // Use a PC-relative load from a boot image InternTable mmapped into the .bss
    // of the oat file.
    kBootImageInternTable,

    // Load from an entry in the .bss section using a PC-relative load.
    // Used for strings outside boot image when .bss is accessible with a PC-relative load.
    kBssEntry,

    // Load from the root table associated with the JIT compiled method.
    kJitTableAddress,

    // Load using a simple runtime call. This is the fall-back load kind when
    // the codegen is unable to use another appropriate kind.
    kRuntimeCall,

    kLast = kRuntimeCall,
  };

  HLoadString(HCurrentMethod* current_method,
              dex::StringIndex string_index,
              const DexFile& dex_file,
              uint32_t dex_pc)
      : HInstruction(kLoadString, SideEffectsForArchRuntimeCalls(), dex_pc),
        special_input_(HUserRecord<HInstruction*>(current_method)),
        string_index_(string_index),
        dex_file_(dex_file) {
    SetPackedField<LoadKindField>(LoadKind::kRuntimeCall);
  }

  bool IsClonable() const OVERRIDE { return true; }

  void SetLoadKind(LoadKind load_kind);

  LoadKind GetLoadKind() const {
    return GetPackedField<LoadKindField>();
  }

  const DexFile& GetDexFile() const {
    return dex_file_;
  }

  dex::StringIndex GetStringIndex() const {
    return string_index_;
  }

  Handle<mirror::String> GetString() const {
    return string_;
  }

  void SetString(Handle<mirror::String> str) {
    string_ = str;
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE;

  size_t ComputeHashCode() const OVERRIDE { return string_index_.index_; }

  // Will call the runtime if we need to load the string through
  // the dex cache and the string is not guaranteed to be there yet.
  bool NeedsEnvironment() const OVERRIDE {
    LoadKind load_kind = GetLoadKind();
    if (load_kind == LoadKind::kBootImageLinkTimePcRelative ||
        load_kind == LoadKind::kBootImageAddress ||
        load_kind == LoadKind::kBootImageInternTable ||
        load_kind == LoadKind::kJitTableAddress) {
      return false;
    }
    return true;
  }

  bool NeedsDexCacheOfDeclaringClass() const OVERRIDE {
    return GetLoadKind() == LoadKind::kRuntimeCall;
  }

  bool CanBeNull() const OVERRIDE { return false; }
  bool CanThrow() const OVERRIDE { return NeedsEnvironment(); }

  static SideEffects SideEffectsForArchRuntimeCalls() {
    return SideEffects::CanTriggerGC();
  }

  void AddSpecialInput(HInstruction* special_input);

  using HInstruction::GetInputRecords;  // Keep the const version visible.
  ArrayRef<HUserRecord<HInstruction*>> GetInputRecords() OVERRIDE FINAL {
    return ArrayRef<HUserRecord<HInstruction*>>(
        &special_input_, (special_input_.GetInstruction() != nullptr) ? 1u : 0u);
  }

  DataType::Type GetType() const OVERRIDE {
    return DataType::Type::kReference;
  }

  DECLARE_INSTRUCTION(LoadString);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LoadString);

 private:
  static constexpr size_t kFieldLoadKind = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldLoadKindSize =
      MinimumBitsToStore(static_cast<size_t>(LoadKind::kLast));
  static constexpr size_t kNumberOfLoadStringPackedBits = kFieldLoadKind + kFieldLoadKindSize;
  static_assert(kNumberOfLoadStringPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using LoadKindField = BitField<LoadKind, kFieldLoadKind, kFieldLoadKindSize>;

  void SetLoadKindInternal(LoadKind load_kind);

  // The special input is the HCurrentMethod for kRuntimeCall.
  // For other load kinds it's empty or possibly some architecture-specific instruction
  // for PC-relative loads, i.e. kBssEntry or kBootImageLinkTimePcRelative.
  HUserRecord<HInstruction*> special_input_;

  dex::StringIndex string_index_;
  const DexFile& dex_file_;

  Handle<mirror::String> string_;
};
std::ostream& operator<<(std::ostream& os, HLoadString::LoadKind rhs);

// Note: defined outside class to see operator<<(., HLoadString::LoadKind).
inline void HLoadString::SetLoadKind(LoadKind load_kind) {
  // The load kind should be determined before inserting the instruction to the graph.
  DCHECK(GetBlock() == nullptr);
  DCHECK(GetEnvironment() == nullptr);
  DCHECK_EQ(GetLoadKind(), LoadKind::kRuntimeCall);
  SetPackedField<LoadKindField>(load_kind);
  if (load_kind != LoadKind::kRuntimeCall) {
    special_input_ = HUserRecord<HInstruction*>(nullptr);
  }
  if (!NeedsEnvironment()) {
    SetSideEffects(SideEffects::None());
  }
}

// Note: defined outside class to see operator<<(., HLoadString::LoadKind).
inline void HLoadString::AddSpecialInput(HInstruction* special_input) {
  // The special input is used for PC-relative loads on some architectures,
  // including literal pool loads, which are PC-relative too.
  DCHECK(GetLoadKind() == LoadKind::kBootImageLinkTimePcRelative ||
         GetLoadKind() == LoadKind::kBootImageAddress ||
         GetLoadKind() == LoadKind::kBootImageInternTable ||
         GetLoadKind() == LoadKind::kBssEntry) << GetLoadKind();
  // HLoadString::GetInputRecords() returns an empty array at this point,
  // so use the GetInputRecords() from the base class to set the input record.
  DCHECK(special_input_.GetInstruction() == nullptr);
  special_input_ = HUserRecord<HInstruction*>(special_input);
  special_input->AddUseAt(this, 0);
}

/**
 * Performs an initialization check on its Class object input.
 */
class HClinitCheck FINAL : public HExpression<1> {
 public:
  HClinitCheck(HLoadClass* constant, uint32_t dex_pc)
      : HExpression(
            kClinitCheck,
            DataType::Type::kReference,
            SideEffects::AllExceptGCDependency(),  // Assume write/read on all fields/arrays.
            dex_pc) {
    SetRawInputAt(0, constant);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE {
    // May call runtime to initialize the class.
    return true;
  }

  bool CanThrow() const OVERRIDE { return true; }

  HLoadClass* GetLoadClass() const {
    DCHECK(InputAt(0)->IsLoadClass());
    return InputAt(0)->AsLoadClass();
  }

  DECLARE_INSTRUCTION(ClinitCheck);


 protected:
  DEFAULT_COPY_CONSTRUCTOR(ClinitCheck);
};

class HStaticFieldGet FINAL : public HExpression<1> {
 public:
  HStaticFieldGet(HInstruction* cls,
                  ArtField* field,
                  DataType::Type field_type,
                  MemberOffset field_offset,
                  bool is_volatile,
                  uint32_t field_idx,
                  uint16_t declaring_class_def_index,
                  const DexFile& dex_file,
                  uint32_t dex_pc)
      : HExpression(kStaticFieldGet,
                    field_type,
                    SideEffects::FieldReadOfType(field_type, is_volatile),
                    dex_pc),
        field_info_(field,
                    field_offset,
                    field_type,
                    is_volatile,
                    field_idx,
                    declaring_class_def_index,
                    dex_file) {
    SetRawInputAt(0, cls);
  }


  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return !IsVolatile(); }

  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    const HStaticFieldGet* other_get = other->AsStaticFieldGet();
    return GetFieldOffset().SizeValue() == other_get->GetFieldOffset().SizeValue();
  }

  size_t ComputeHashCode() const OVERRIDE {
    return (HInstruction::ComputeHashCode() << 7) | GetFieldOffset().SizeValue();
  }

  const FieldInfo& GetFieldInfo() const { return field_info_; }
  MemberOffset GetFieldOffset() const { return field_info_.GetFieldOffset(); }
  DataType::Type GetFieldType() const { return field_info_.GetFieldType(); }
  bool IsVolatile() const { return field_info_.IsVolatile(); }

  void SetType(DataType::Type new_type) {
    DCHECK(DataType::IsIntegralType(GetType()));
    DCHECK(DataType::IsIntegralType(new_type));
    DCHECK_EQ(DataType::Size(GetType()), DataType::Size(new_type));
    SetPackedField<TypeField>(new_type);
  }

  DECLARE_INSTRUCTION(StaticFieldGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(StaticFieldGet);

 private:
  const FieldInfo field_info_;
};

class HStaticFieldSet FINAL : public HTemplateInstruction<2> {
 public:
  HStaticFieldSet(HInstruction* cls,
                  HInstruction* value,
                  ArtField* field,
                  DataType::Type field_type,
                  MemberOffset field_offset,
                  bool is_volatile,
                  uint32_t field_idx,
                  uint16_t declaring_class_def_index,
                  const DexFile& dex_file,
                  uint32_t dex_pc)
      : HTemplateInstruction(kStaticFieldSet,
                             SideEffects::FieldWriteOfType(field_type, is_volatile),
                             dex_pc),
        field_info_(field,
                    field_offset,
                    field_type,
                    is_volatile,
                    field_idx,
                    declaring_class_def_index,
                    dex_file) {
    SetPackedFlag<kFlagValueCanBeNull>(true);
    SetRawInputAt(0, cls);
    SetRawInputAt(1, value);
  }

  bool IsClonable() const OVERRIDE { return true; }
  const FieldInfo& GetFieldInfo() const { return field_info_; }
  MemberOffset GetFieldOffset() const { return field_info_.GetFieldOffset(); }
  DataType::Type GetFieldType() const { return field_info_.GetFieldType(); }
  bool IsVolatile() const { return field_info_.IsVolatile(); }

  HInstruction* GetValue() const { return InputAt(1); }
  bool GetValueCanBeNull() const { return GetPackedFlag<kFlagValueCanBeNull>(); }
  void ClearValueCanBeNull() { SetPackedFlag<kFlagValueCanBeNull>(false); }

  DECLARE_INSTRUCTION(StaticFieldSet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(StaticFieldSet);

 private:
  static constexpr size_t kFlagValueCanBeNull = kNumberOfGenericPackedBits;
  static constexpr size_t kNumberOfStaticFieldSetPackedBits = kFlagValueCanBeNull + 1;
  static_assert(kNumberOfStaticFieldSetPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");

  const FieldInfo field_info_;
};

class HUnresolvedInstanceFieldGet FINAL : public HExpression<1> {
 public:
  HUnresolvedInstanceFieldGet(HInstruction* obj,
                              DataType::Type field_type,
                              uint32_t field_index,
                              uint32_t dex_pc)
      : HExpression(kUnresolvedInstanceFieldGet,
                    field_type,
                    SideEffects::AllExceptGCDependency(),
                    dex_pc),
        field_index_(field_index) {
    SetRawInputAt(0, obj);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool NeedsEnvironment() const OVERRIDE { return true; }
  bool CanThrow() const OVERRIDE { return true; }

  DataType::Type GetFieldType() const { return GetType(); }
  uint32_t GetFieldIndex() const { return field_index_; }

  DECLARE_INSTRUCTION(UnresolvedInstanceFieldGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UnresolvedInstanceFieldGet);

 private:
  const uint32_t field_index_;
};

class HUnresolvedInstanceFieldSet FINAL : public HTemplateInstruction<2> {
 public:
  HUnresolvedInstanceFieldSet(HInstruction* obj,
                              HInstruction* value,
                              DataType::Type field_type,
                              uint32_t field_index,
                              uint32_t dex_pc)
      : HTemplateInstruction(kUnresolvedInstanceFieldSet,
                             SideEffects::AllExceptGCDependency(),
                             dex_pc),
        field_index_(field_index) {
    SetPackedField<FieldTypeField>(field_type);
    DCHECK_EQ(DataType::Kind(field_type), DataType::Kind(value->GetType()));
    SetRawInputAt(0, obj);
    SetRawInputAt(1, value);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool NeedsEnvironment() const OVERRIDE { return true; }
  bool CanThrow() const OVERRIDE { return true; }

  DataType::Type GetFieldType() const { return GetPackedField<FieldTypeField>(); }
  uint32_t GetFieldIndex() const { return field_index_; }

  DECLARE_INSTRUCTION(UnresolvedInstanceFieldSet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UnresolvedInstanceFieldSet);

 private:
  static constexpr size_t kFieldFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldFieldTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kNumberOfUnresolvedStaticFieldSetPackedBits =
      kFieldFieldType + kFieldFieldTypeSize;
  static_assert(kNumberOfUnresolvedStaticFieldSetPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using FieldTypeField = BitField<DataType::Type, kFieldFieldType, kFieldFieldTypeSize>;

  const uint32_t field_index_;
};

class HUnresolvedStaticFieldGet FINAL : public HExpression<0> {
 public:
  HUnresolvedStaticFieldGet(DataType::Type field_type,
                            uint32_t field_index,
                            uint32_t dex_pc)
      : HExpression(kUnresolvedStaticFieldGet,
                    field_type,
                    SideEffects::AllExceptGCDependency(),
                    dex_pc),
        field_index_(field_index) {
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool NeedsEnvironment() const OVERRIDE { return true; }
  bool CanThrow() const OVERRIDE { return true; }

  DataType::Type GetFieldType() const { return GetType(); }
  uint32_t GetFieldIndex() const { return field_index_; }

  DECLARE_INSTRUCTION(UnresolvedStaticFieldGet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UnresolvedStaticFieldGet);

 private:
  const uint32_t field_index_;
};

class HUnresolvedStaticFieldSet FINAL : public HTemplateInstruction<1> {
 public:
  HUnresolvedStaticFieldSet(HInstruction* value,
                            DataType::Type field_type,
                            uint32_t field_index,
                            uint32_t dex_pc)
      : HTemplateInstruction(kUnresolvedStaticFieldSet,
                             SideEffects::AllExceptGCDependency(),
                             dex_pc),
        field_index_(field_index) {
    SetPackedField<FieldTypeField>(field_type);
    DCHECK_EQ(DataType::Kind(field_type), DataType::Kind(value->GetType()));
    SetRawInputAt(0, value);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool NeedsEnvironment() const OVERRIDE { return true; }
  bool CanThrow() const OVERRIDE { return true; }

  DataType::Type GetFieldType() const { return GetPackedField<FieldTypeField>(); }
  uint32_t GetFieldIndex() const { return field_index_; }

  DECLARE_INSTRUCTION(UnresolvedStaticFieldSet);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(UnresolvedStaticFieldSet);

 private:
  static constexpr size_t kFieldFieldType = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldFieldTypeSize =
      MinimumBitsToStore(static_cast<size_t>(DataType::Type::kLast));
  static constexpr size_t kNumberOfUnresolvedStaticFieldSetPackedBits =
      kFieldFieldType + kFieldFieldTypeSize;
  static_assert(kNumberOfUnresolvedStaticFieldSetPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using FieldTypeField = BitField<DataType::Type, kFieldFieldType, kFieldFieldTypeSize>;

  const uint32_t field_index_;
};

// Implement the move-exception DEX instruction.
class HLoadException FINAL : public HExpression<0> {
 public:
  explicit HLoadException(uint32_t dex_pc = kNoDexPc)
      : HExpression(kLoadException, DataType::Type::kReference, SideEffects::None(), dex_pc) {
  }

  bool CanBeNull() const OVERRIDE { return false; }

  DECLARE_INSTRUCTION(LoadException);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(LoadException);
};

// Implicit part of move-exception which clears thread-local exception storage.
// Must not be removed because the runtime expects the TLS to get cleared.
class HClearException FINAL : public HTemplateInstruction<0> {
 public:
  explicit HClearException(uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kClearException, SideEffects::AllWrites(), dex_pc) {
  }

  DECLARE_INSTRUCTION(ClearException);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ClearException);
};

class HThrow FINAL : public HTemplateInstruction<1> {
 public:
  HThrow(HInstruction* exception, uint32_t dex_pc)
      : HTemplateInstruction(kThrow, SideEffects::CanTriggerGC(), dex_pc) {
    SetRawInputAt(0, exception);
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE { return true; }

  bool AlwaysThrows() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(Throw);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Throw);
};

/**
 * Implementation strategies for the code generator of a HInstanceOf
 * or `HCheckCast`.
 */
enum class TypeCheckKind {
  kUnresolvedCheck,       // Check against an unresolved type.
  kExactCheck,            // Can do a single class compare.
  kClassHierarchyCheck,   // Can just walk the super class chain.
  kAbstractClassCheck,    // Can just walk the super class chain, starting one up.
  kInterfaceCheck,        // No optimization yet when checking against an interface.
  kArrayObjectCheck,      // Can just check if the array is not primitive.
  kArrayCheck,            // No optimization yet when checking against a generic array.
  kLast = kArrayCheck
};

std::ostream& operator<<(std::ostream& os, TypeCheckKind rhs);

class HInstanceOf FINAL : public HExpression<2> {
 public:
  HInstanceOf(HInstruction* object,
              HLoadClass* target_class,
              TypeCheckKind check_kind,
              uint32_t dex_pc)
      : HExpression(kInstanceOf,
                    DataType::Type::kBool,
                    SideEffectsForArchRuntimeCalls(check_kind),
                    dex_pc) {
    SetPackedField<TypeCheckKindField>(check_kind);
    SetPackedFlag<kFlagMustDoNullCheck>(true);
    SetRawInputAt(0, object);
    SetRawInputAt(1, target_class);
  }

  HLoadClass* GetTargetClass() const {
    HInstruction* load_class = InputAt(1);
    DCHECK(load_class->IsLoadClass());
    return load_class->AsLoadClass();
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE {
    return CanCallRuntime(GetTypeCheckKind());
  }

  // Used only in code generation.
  bool MustDoNullCheck() const { return GetPackedFlag<kFlagMustDoNullCheck>(); }
  void ClearMustDoNullCheck() { SetPackedFlag<kFlagMustDoNullCheck>(false); }
  TypeCheckKind GetTypeCheckKind() const { return GetPackedField<TypeCheckKindField>(); }
  bool IsExactCheck() const { return GetTypeCheckKind() == TypeCheckKind::kExactCheck; }

  static bool CanCallRuntime(TypeCheckKind check_kind) {
    // Mips currently does runtime calls for any other checks.
    return check_kind != TypeCheckKind::kExactCheck;
  }

  static SideEffects SideEffectsForArchRuntimeCalls(TypeCheckKind check_kind) {
    return CanCallRuntime(check_kind) ? SideEffects::CanTriggerGC() : SideEffects::None();
  }

  DECLARE_INSTRUCTION(InstanceOf);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(InstanceOf);

 private:
  static constexpr size_t kFieldTypeCheckKind = kNumberOfExpressionPackedBits;
  static constexpr size_t kFieldTypeCheckKindSize =
      MinimumBitsToStore(static_cast<size_t>(TypeCheckKind::kLast));
  static constexpr size_t kFlagMustDoNullCheck = kFieldTypeCheckKind + kFieldTypeCheckKindSize;
  static constexpr size_t kNumberOfInstanceOfPackedBits = kFlagMustDoNullCheck + 1;
  static_assert(kNumberOfInstanceOfPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using TypeCheckKindField = BitField<TypeCheckKind, kFieldTypeCheckKind, kFieldTypeCheckKindSize>;
};

class HBoundType FINAL : public HExpression<1> {
 public:
  explicit HBoundType(HInstruction* input, uint32_t dex_pc = kNoDexPc)
      : HExpression(kBoundType, DataType::Type::kReference, SideEffects::None(), dex_pc),
        upper_bound_(ReferenceTypeInfo::CreateInvalid()) {
    SetPackedFlag<kFlagUpperCanBeNull>(true);
    SetPackedFlag<kFlagCanBeNull>(true);
    DCHECK_EQ(input->GetType(), DataType::Type::kReference);
    SetRawInputAt(0, input);
  }

  bool IsClonable() const OVERRIDE { return true; }

  // {Get,Set}Upper* should only be used in reference type propagation.
  const ReferenceTypeInfo& GetUpperBound() const { return upper_bound_; }
  bool GetUpperCanBeNull() const { return GetPackedFlag<kFlagUpperCanBeNull>(); }
  void SetUpperBound(const ReferenceTypeInfo& upper_bound, bool can_be_null);

  void SetCanBeNull(bool can_be_null) {
    DCHECK(GetUpperCanBeNull() || !can_be_null);
    SetPackedFlag<kFlagCanBeNull>(can_be_null);
  }

  bool CanBeNull() const OVERRIDE { return GetPackedFlag<kFlagCanBeNull>(); }

  DECLARE_INSTRUCTION(BoundType);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BoundType);

 private:
  // Represents the top constraint that can_be_null_ cannot exceed (i.e. if this
  // is false then CanBeNull() cannot be true).
  static constexpr size_t kFlagUpperCanBeNull = kNumberOfExpressionPackedBits;
  static constexpr size_t kFlagCanBeNull = kFlagUpperCanBeNull + 1;
  static constexpr size_t kNumberOfBoundTypePackedBits = kFlagCanBeNull + 1;
  static_assert(kNumberOfBoundTypePackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");

  // Encodes the most upper class that this instruction can have. In other words
  // it is always the case that GetUpperBound().IsSupertypeOf(GetReferenceType()).
  // It is used to bound the type in cases like:
  //   if (x instanceof ClassX) {
  //     // uper_bound_ will be ClassX
  //   }
  ReferenceTypeInfo upper_bound_;
};

class HCheckCast FINAL : public HTemplateInstruction<2> {
 public:
  HCheckCast(HInstruction* object,
             HLoadClass* target_class,
             TypeCheckKind check_kind,
             uint32_t dex_pc)
      : HTemplateInstruction(kCheckCast, SideEffects::CanTriggerGC(), dex_pc) {
    SetPackedField<TypeCheckKindField>(check_kind);
    SetPackedFlag<kFlagMustDoNullCheck>(true);
    SetRawInputAt(0, object);
    SetRawInputAt(1, target_class);
  }

  HLoadClass* GetTargetClass() const {
    HInstruction* load_class = InputAt(1);
    DCHECK(load_class->IsLoadClass());
    return load_class->AsLoadClass();
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }

  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool NeedsEnvironment() const OVERRIDE {
    // Instruction may throw a CheckCastError.
    return true;
  }

  bool CanThrow() const OVERRIDE { return true; }

  bool MustDoNullCheck() const { return GetPackedFlag<kFlagMustDoNullCheck>(); }
  void ClearMustDoNullCheck() { SetPackedFlag<kFlagMustDoNullCheck>(false); }
  TypeCheckKind GetTypeCheckKind() const { return GetPackedField<TypeCheckKindField>(); }
  bool IsExactCheck() const { return GetTypeCheckKind() == TypeCheckKind::kExactCheck; }

  DECLARE_INSTRUCTION(CheckCast);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(CheckCast);

 private:
  static constexpr size_t kFieldTypeCheckKind = kNumberOfGenericPackedBits;
  static constexpr size_t kFieldTypeCheckKindSize =
      MinimumBitsToStore(static_cast<size_t>(TypeCheckKind::kLast));
  static constexpr size_t kFlagMustDoNullCheck = kFieldTypeCheckKind + kFieldTypeCheckKindSize;
  static constexpr size_t kNumberOfCheckCastPackedBits = kFlagMustDoNullCheck + 1;
  static_assert(kNumberOfCheckCastPackedBits <= kMaxNumberOfPackedBits, "Too many packed fields.");
  using TypeCheckKindField = BitField<TypeCheckKind, kFieldTypeCheckKind, kFieldTypeCheckKindSize>;
};

/**
 * @brief Memory barrier types (see "The JSR-133 Cookbook for Compiler Writers").
 * @details We define the combined barrier types that are actually required
 * by the Java Memory Model, rather than using exactly the terminology from
 * the JSR-133 cookbook.  These should, in many cases, be replaced by acquire/release
 * primitives.  Note that the JSR-133 cookbook generally does not deal with
 * store atomicity issues, and the recipes there are not always entirely sufficient.
 * The current recipe is as follows:
 * -# Use AnyStore ~= (LoadStore | StoreStore) ~= release barrier before volatile store.
 * -# Use AnyAny barrier after volatile store.  (StoreLoad is as expensive.)
 * -# Use LoadAny barrier ~= (LoadLoad | LoadStore) ~= acquire barrier after each volatile load.
 * -# Use StoreStore barrier after all stores but before return from any constructor whose
 *    class has final fields.
 * -# Use NTStoreStore to order non-temporal stores with respect to all later
 *    store-to-memory instructions.  Only generated together with non-temporal stores.
 */
enum MemBarrierKind {
  kAnyStore,
  kLoadAny,
  kStoreStore,
  kAnyAny,
  kNTStoreStore,
  kLastBarrierKind = kNTStoreStore
};
std::ostream& operator<<(std::ostream& os, const MemBarrierKind& kind);

class HMemoryBarrier FINAL : public HTemplateInstruction<0> {
 public:
  explicit HMemoryBarrier(MemBarrierKind barrier_kind, uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(
            kMemoryBarrier,
            SideEffects::AllWritesAndReads(),  // Assume write/read on all fields/arrays.
            dex_pc) {
    SetPackedField<BarrierKindField>(barrier_kind);
  }

  bool IsClonable() const OVERRIDE { return true; }

  MemBarrierKind GetBarrierKind() { return GetPackedField<BarrierKindField>(); }

  DECLARE_INSTRUCTION(MemoryBarrier);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(MemoryBarrier);

 private:
  static constexpr size_t kFieldBarrierKind = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldBarrierKindSize =
      MinimumBitsToStore(static_cast<size_t>(kLastBarrierKind));
  static constexpr size_t kNumberOfMemoryBarrierPackedBits =
      kFieldBarrierKind + kFieldBarrierKindSize;
  static_assert(kNumberOfMemoryBarrierPackedBits <= kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using BarrierKindField = BitField<MemBarrierKind, kFieldBarrierKind, kFieldBarrierKindSize>;
};

// A constructor fence orders all prior stores to fields that could be accessed via a final field of
// the specified object(s), with respect to any subsequent store that might "publish"
// (i.e. make visible) the specified object to another thread.
//
// JLS 17.5.1 "Semantics of final fields" states that a freeze action happens
// for all final fields (that were set) at the end of the invoked constructor.
//
// The constructor fence models the freeze actions for the final fields of an object
// being constructed (semantically at the end of the constructor). Constructor fences
// have a per-object affinity; two separate objects being constructed get two separate
// constructor fences.
//
// (Note: that if calling a super-constructor or forwarding to another constructor,
// the freezes would happen at the end of *that* constructor being invoked).
//
// The memory model guarantees that when the object being constructed is "published" after
// constructor completion (i.e. escapes the current thread via a store), then any final field
// writes must be observable on other threads (once they observe that publication).
//
// Further, anything written before the freeze, and read by dereferencing through the final field,
// must also be visible (so final object field could itself have an object with non-final fields;
// yet the freeze must also extend to them).
//
// Constructor example:
//
//     class HasFinal {
//        final int field;                              Optimizing IR for <init>()V:
//        HasFinal() {
//          field = 123;                                HInstanceFieldSet(this, HasFinal.field, 123)
//          // freeze(this.field);                      HConstructorFence(this)
//        }                                             HReturn
//     }
//
// HConstructorFence can serve double duty as a fence for new-instance/new-array allocations of
// already-initialized classes; in that case the allocation must act as a "default-initializer"
// of the object which effectively writes the class pointer "final field".
//
// For example, we can model default-initialiation as roughly the equivalent of the following:
//
//     class Object {
//       private final Class header;
//     }
//
//  Java code:                                           Optimizing IR:
//
//     T new_instance<T>() {
//       Object obj = allocate_memory(T.class.size);     obj = HInvoke(art_quick_alloc_object, T)
//       obj.header = T.class;                           // header write is done by above call.
//       // freeze(obj.header)                           HConstructorFence(obj)
//       return (T)obj;
//     }
//
// See also:
// * CompilerDriver::RequiresConstructorBarrier
// * QuasiAtomic::ThreadFenceForConstructor
//
class HConstructorFence FINAL : public HVariableInputSizeInstruction {
                                  // A fence has variable inputs because the inputs can be removed
                                  // after prepare_for_register_allocation phase.
                                  // (TODO: In the future a fence could freeze multiple objects
                                  //        after merging two fences together.)
 public:
  // `fence_object` is the reference that needs to be protected for correct publication.
  //
  // It makes sense in the following situations:
  // * <init> constructors, it's the "this" parameter (i.e. HParameterValue, s.t. IsThis() == true).
  // * new-instance-like instructions, it's the return value (i.e. HNewInstance).
  //
  // After construction the `fence_object` becomes the 0th input.
  // This is not an input in a real sense, but just a convenient place to stash the information
  // about the associated object.
  HConstructorFence(HInstruction* fence_object,
                    uint32_t dex_pc,
                    ArenaAllocator* allocator)
    // We strongly suspect there is not a more accurate way to describe the fine-grained reordering
    // constraints described in the class header. We claim that these SideEffects constraints
    // enforce a superset of the real constraints.
    //
    // The ordering described above is conservatively modeled with SideEffects as follows:
    //
    // * To prevent reordering of the publication stores:
    // ----> "Reads of objects" is the initial SideEffect.
    // * For every primitive final field store in the constructor:
    // ----> Union that field's type as a read (e.g. "Read of T") into the SideEffect.
    // * If there are any stores to reference final fields in the constructor:
    // ----> Use a more conservative "AllReads" SideEffect because any stores to any references
    //       that are reachable from `fence_object` also need to be prevented for reordering
    //       (and we do not want to do alias analysis to figure out what those stores are).
    //
    // In the implementation, this initially starts out as an "all reads" side effect; this is an
    // even more conservative approach than the one described above, and prevents all of the
    // above reordering without analyzing any of the instructions in the constructor.
    //
    // If in a later phase we discover that there are no writes to reference final fields,
    // we can refine the side effect to a smaller set of type reads (see above constraints).
      : HVariableInputSizeInstruction(kConstructorFence,
                                      SideEffects::AllReads(),
                                      dex_pc,
                                      allocator,
                                      /* number_of_inputs */ 1,
                                      kArenaAllocConstructorFenceInputs) {
    DCHECK(fence_object != nullptr);
    SetRawInputAt(0, fence_object);
  }

  // The object associated with this constructor fence.
  //
  // (Note: This will be null after the prepare_for_register_allocation phase,
  // as all constructor fence inputs are removed there).
  HInstruction* GetFenceObject() const {
    return InputAt(0);
  }

  // Find all the HConstructorFence uses (`fence_use`) for `this` and:
  // - Delete `fence_use` from `this`'s use list.
  // - Delete `this` from `fence_use`'s inputs list.
  // - If the `fence_use` is dead, remove it from the graph.
  //
  // A fence is considered dead once it no longer has any uses
  // and all of the inputs are dead.
  //
  // This must *not* be called during/after prepare_for_register_allocation,
  // because that removes all the inputs to the fences but the fence is actually
  // still considered live.
  //
  // Returns how many HConstructorFence instructions were removed from graph.
  static size_t RemoveConstructorFences(HInstruction* instruction);

  // Combine all inputs of `this` and `other` instruction and remove
  // `other` from the graph.
  //
  // Inputs are unique after the merge.
  //
  // Requirement: `this` must not be the same as `other.
  void Merge(HConstructorFence* other);

  // Check if this constructor fence is protecting
  // an HNewInstance or HNewArray that is also the immediate
  // predecessor of `this`.
  //
  // If `ignore_inputs` is true, then the immediate predecessor doesn't need
  // to be one of the inputs of `this`.
  //
  // Returns the associated HNewArray or HNewInstance,
  // or null otherwise.
  HInstruction* GetAssociatedAllocation(bool ignore_inputs = false);

  DECLARE_INSTRUCTION(ConstructorFence);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ConstructorFence);
};

class HMonitorOperation FINAL : public HTemplateInstruction<1> {
 public:
  enum class OperationKind {
    kEnter,
    kExit,
    kLast = kExit
  };

  HMonitorOperation(HInstruction* object, OperationKind kind, uint32_t dex_pc)
    : HTemplateInstruction(
          kMonitorOperation,
          SideEffects::AllExceptGCDependency(),  // Assume write/read on all fields/arrays.
          dex_pc) {
    SetPackedField<OperationKindField>(kind);
    SetRawInputAt(0, object);
  }

  // Instruction may go into runtime, so we need an environment.
  bool NeedsEnvironment() const OVERRIDE { return true; }

  bool CanThrow() const OVERRIDE {
    // Verifier guarantees that monitor-exit cannot throw.
    // This is important because it allows the HGraphBuilder to remove
    // a dead throw-catch loop generated for `synchronized` blocks/methods.
    return IsEnter();
  }

  OperationKind GetOperationKind() const { return GetPackedField<OperationKindField>(); }
  bool IsEnter() const { return GetOperationKind() == OperationKind::kEnter; }

  DECLARE_INSTRUCTION(MonitorOperation);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(MonitorOperation);

 private:
  static constexpr size_t kFieldOperationKind = HInstruction::kNumberOfGenericPackedBits;
  static constexpr size_t kFieldOperationKindSize =
      MinimumBitsToStore(static_cast<size_t>(OperationKind::kLast));
  static constexpr size_t kNumberOfMonitorOperationPackedBits =
      kFieldOperationKind + kFieldOperationKindSize;
  static_assert(kNumberOfMonitorOperationPackedBits <= HInstruction::kMaxNumberOfPackedBits,
                "Too many packed fields.");
  using OperationKindField = BitField<OperationKind, kFieldOperationKind, kFieldOperationKindSize>;
};

class HSelect FINAL : public HExpression<3> {
 public:
  HSelect(HInstruction* condition,
          HInstruction* true_value,
          HInstruction* false_value,
          uint32_t dex_pc)
      : HExpression(kSelect, HPhi::ToPhiType(true_value->GetType()), SideEffects::None(), dex_pc) {
    DCHECK_EQ(HPhi::ToPhiType(true_value->GetType()), HPhi::ToPhiType(false_value->GetType()));

    // First input must be `true_value` or `false_value` to allow codegens to
    // use the SameAsFirstInput allocation policy. We make it `false_value`, so
    // that architectures which implement HSelect as a conditional move also
    // will not need to invert the condition.
    SetRawInputAt(0, false_value);
    SetRawInputAt(1, true_value);
    SetRawInputAt(2, condition);
  }

  bool IsClonable() const OVERRIDE { return true; }
  HInstruction* GetFalseValue() const { return InputAt(0); }
  HInstruction* GetTrueValue() const { return InputAt(1); }
  HInstruction* GetCondition() const { return InputAt(2); }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }

  bool CanBeNull() const OVERRIDE {
    return GetTrueValue()->CanBeNull() || GetFalseValue()->CanBeNull();
  }

  DECLARE_INSTRUCTION(Select);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(Select);
};

class MoveOperands : public ArenaObject<kArenaAllocMoveOperands> {
 public:
  MoveOperands(Location source,
               Location destination,
               DataType::Type type,
               HInstruction* instruction)
      : source_(source), destination_(destination), type_(type), instruction_(instruction) {}

  Location GetSource() const { return source_; }
  Location GetDestination() const { return destination_; }

  void SetSource(Location value) { source_ = value; }
  void SetDestination(Location value) { destination_ = value; }

  // The parallel move resolver marks moves as "in-progress" by clearing the
  // destination (but not the source).
  Location MarkPending() {
    DCHECK(!IsPending());
    Location dest = destination_;
    destination_ = Location::NoLocation();
    return dest;
  }

  void ClearPending(Location dest) {
    DCHECK(IsPending());
    destination_ = dest;
  }

  bool IsPending() const {
    DCHECK(source_.IsValid() || destination_.IsInvalid());
    return destination_.IsInvalid() && source_.IsValid();
  }

  // True if this blocks a move from the given location.
  bool Blocks(Location loc) const {
    return !IsEliminated() && source_.OverlapsWith(loc);
  }

  // A move is redundant if it's been eliminated, if its source and
  // destination are the same, or if its destination is unneeded.
  bool IsRedundant() const {
    return IsEliminated() || destination_.IsInvalid() || source_.Equals(destination_);
  }

  // We clear both operands to indicate move that's been eliminated.
  void Eliminate() {
    source_ = destination_ = Location::NoLocation();
  }

  bool IsEliminated() const {
    DCHECK(!source_.IsInvalid() || destination_.IsInvalid());
    return source_.IsInvalid();
  }

  DataType::Type GetType() const { return type_; }

  bool Is64BitMove() const {
    return DataType::Is64BitType(type_);
  }

  HInstruction* GetInstruction() const { return instruction_; }

 private:
  Location source_;
  Location destination_;
  // The type this move is for.
  DataType::Type type_;
  // The instruction this move is assocatied with. Null when this move is
  // for moving an input in the expected locations of user (including a phi user).
  // This is only used in debug mode, to ensure we do not connect interval siblings
  // in the same parallel move.
  HInstruction* instruction_;
};

std::ostream& operator<<(std::ostream& os, const MoveOperands& rhs);

static constexpr size_t kDefaultNumberOfMoves = 4;

class HParallelMove FINAL : public HTemplateInstruction<0> {
 public:
  explicit HParallelMove(ArenaAllocator* allocator, uint32_t dex_pc = kNoDexPc)
      : HTemplateInstruction(kParallelMove, SideEffects::None(), dex_pc),
        moves_(allocator->Adapter(kArenaAllocMoveOperands)) {
    moves_.reserve(kDefaultNumberOfMoves);
  }

  void AddMove(Location source,
               Location destination,
               DataType::Type type,
               HInstruction* instruction) {
    DCHECK(source.IsValid());
    DCHECK(destination.IsValid());
    if (kIsDebugBuild) {
      if (instruction != nullptr) {
        for (const MoveOperands& move : moves_) {
          if (move.GetInstruction() == instruction) {
            // Special case the situation where the move is for the spill slot
            // of the instruction.
            if ((GetPrevious() == instruction)
                || ((GetPrevious() == nullptr)
                    && instruction->IsPhi()
                    && instruction->GetBlock() == GetBlock())) {
              DCHECK_NE(destination.GetKind(), move.GetDestination().GetKind())
                  << "Doing parallel moves for the same instruction.";
            } else {
              DCHECK(false) << "Doing parallel moves for the same instruction.";
            }
          }
        }
      }
      for (const MoveOperands& move : moves_) {
        DCHECK(!destination.OverlapsWith(move.GetDestination()))
            << "Overlapped destination for two moves in a parallel move: "
            << move.GetSource() << " ==> " << move.GetDestination() << " and "
            << source << " ==> " << destination;
      }
    }
    moves_.emplace_back(source, destination, type, instruction);
  }

  MoveOperands* MoveOperandsAt(size_t index) {
    return &moves_[index];
  }

  size_t NumMoves() const { return moves_.size(); }

  DECLARE_INSTRUCTION(ParallelMove);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(ParallelMove);

 private:
  ArenaVector<MoveOperands> moves_;
};

// This instruction computes an intermediate address pointing in the 'middle' of an object. The
// result pointer cannot be handled by GC, so extra care is taken to make sure that this value is
// never used across anything that can trigger GC.
// The result of this instruction is not a pointer in the sense of `DataType::Type::kreference`.
// So we represent it by the type `DataType::Type::kInt`.
class HIntermediateAddress FINAL : public HExpression<2> {
 public:
  HIntermediateAddress(HInstruction* base_address, HInstruction* offset, uint32_t dex_pc)
      : HExpression(kIntermediateAddress,
                    DataType::Type::kInt32,
                    SideEffects::DependsOnGC(),
                    dex_pc) {
        DCHECK_EQ(DataType::Size(DataType::Type::kInt32),
                  DataType::Size(DataType::Type::kReference))
            << "kPrimInt and kPrimNot have different sizes.";
    SetRawInputAt(0, base_address);
    SetRawInputAt(1, offset);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }
  bool IsActualObject() const OVERRIDE { return false; }

  HInstruction* GetBaseAddress() const { return InputAt(0); }
  HInstruction* GetOffset() const { return InputAt(1); }

  DECLARE_INSTRUCTION(IntermediateAddress);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(IntermediateAddress);
};


}  // namespace art

#include "nodes_vector.h"

#if defined(ART_ENABLE_CODEGEN_arm) || defined(ART_ENABLE_CODEGEN_arm64)
#include "nodes_shared.h"
#endif
#ifdef ART_ENABLE_CODEGEN_mips
#include "nodes_mips.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86
#include "nodes_x86.h"
#endif

namespace art {

class OptimizingCompilerStats;

class HGraphVisitor : public ValueObject {
 public:
  explicit HGraphVisitor(HGraph* graph, OptimizingCompilerStats* stats = nullptr)
      : stats_(stats),
        graph_(graph) {}
  virtual ~HGraphVisitor() {}

  virtual void VisitInstruction(HInstruction* instruction ATTRIBUTE_UNUSED) {}
  virtual void VisitBasicBlock(HBasicBlock* block);

  // Visit the graph following basic block insertion order.
  void VisitInsertionOrder();

  // Visit the graph following dominator tree reverse post-order.
  void VisitReversePostOrder();

  HGraph* GetGraph() const { return graph_; }

  // Visit functions for instruction classes.
#define DECLARE_VISIT_INSTRUCTION(name, super)                                        \
  virtual void Visit##name(H##name* instr) { VisitInstruction(instr); }

  FOR_EACH_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

 protected:
  OptimizingCompilerStats* stats_;

 private:
  HGraph* const graph_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisitor);
};

class HGraphDelegateVisitor : public HGraphVisitor {
 public:
  explicit HGraphDelegateVisitor(HGraph* graph, OptimizingCompilerStats* stats = nullptr)
      : HGraphVisitor(graph, stats) {}
  virtual ~HGraphDelegateVisitor() {}

  // Visit functions that delegate to to super class.
#define DECLARE_VISIT_INSTRUCTION(name, super)                                        \
  void Visit##name(H##name* instr) OVERRIDE { Visit##super(instr); }

  FOR_EACH_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

 private:
  DISALLOW_COPY_AND_ASSIGN(HGraphDelegateVisitor);
};

// Create a clone of the instruction, insert it into the graph; replace the old one with a new
// and remove the old instruction.
HInstruction* ReplaceInstrOrPhiByClone(HInstruction* instr);

// Create a clone for each clonable instructions/phis and replace the original with the clone.
//
// Used for testing individual instruction cloner.
class CloneAndReplaceInstructionVisitor : public HGraphDelegateVisitor {
 public:
  explicit CloneAndReplaceInstructionVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph), instr_replaced_by_clones_count_(0) {}

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    if (instruction->IsClonable()) {
      ReplaceInstrOrPhiByClone(instruction);
      instr_replaced_by_clones_count_++;
    }
  }

  size_t GetInstrReplacedByClonesCount() const { return instr_replaced_by_clones_count_; }

 private:
  size_t instr_replaced_by_clones_count_;

  DISALLOW_COPY_AND_ASSIGN(CloneAndReplaceInstructionVisitor);
};

// Iterator over the blocks that art part of the loop. Includes blocks part
// of an inner loop. The order in which the blocks are iterated is on their
// block id.
class HBlocksInLoopIterator : public ValueObject {
 public:
  explicit HBlocksInLoopIterator(const HLoopInformation& info)
      : blocks_in_loop_(info.GetBlocks()),
        blocks_(info.GetHeader()->GetGraph()->GetBlocks()),
        index_(0) {
    if (!blocks_in_loop_.IsBitSet(index_)) {
      Advance();
    }
  }

  bool Done() const { return index_ == blocks_.size(); }
  HBasicBlock* Current() const { return blocks_[index_]; }
  void Advance() {
    ++index_;
    for (size_t e = blocks_.size(); index_ < e; ++index_) {
      if (blocks_in_loop_.IsBitSet(index_)) {
        break;
      }
    }
  }

 private:
  const BitVector& blocks_in_loop_;
  const ArenaVector<HBasicBlock*>& blocks_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HBlocksInLoopIterator);
};

// Iterator over the blocks that art part of the loop. Includes blocks part
// of an inner loop. The order in which the blocks are iterated is reverse
// post order.
class HBlocksInLoopReversePostOrderIterator : public ValueObject {
 public:
  explicit HBlocksInLoopReversePostOrderIterator(const HLoopInformation& info)
      : blocks_in_loop_(info.GetBlocks()),
        blocks_(info.GetHeader()->GetGraph()->GetReversePostOrder()),
        index_(0) {
    if (!blocks_in_loop_.IsBitSet(blocks_[index_]->GetBlockId())) {
      Advance();
    }
  }

  bool Done() const { return index_ == blocks_.size(); }
  HBasicBlock* Current() const { return blocks_[index_]; }
  void Advance() {
    ++index_;
    for (size_t e = blocks_.size(); index_ < e; ++index_) {
      if (blocks_in_loop_.IsBitSet(blocks_[index_]->GetBlockId())) {
        break;
      }
    }
  }

 private:
  const BitVector& blocks_in_loop_;
  const ArenaVector<HBasicBlock*>& blocks_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HBlocksInLoopReversePostOrderIterator);
};

// Returns int64_t value of a properly typed constant.
inline int64_t Int64FromConstant(HConstant* constant) {
  if (constant->IsIntConstant()) {
    return constant->AsIntConstant()->GetValue();
  } else if (constant->IsLongConstant()) {
    return constant->AsLongConstant()->GetValue();
  } else {
    DCHECK(constant->IsNullConstant()) << constant->DebugName();
    return 0;
  }
}

// Returns true iff instruction is an integral constant (and sets value on success).
inline bool IsInt64AndGet(HInstruction* instruction, /*out*/ int64_t* value) {
  if (instruction->IsIntConstant()) {
    *value = instruction->AsIntConstant()->GetValue();
    return true;
  } else if (instruction->IsLongConstant()) {
    *value = instruction->AsLongConstant()->GetValue();
    return true;
  } else if (instruction->IsNullConstant()) {
    *value = 0;
    return true;
  }
  return false;
}

// Returns true iff instruction is the given integral constant.
inline bool IsInt64Value(HInstruction* instruction, int64_t value) {
  int64_t val = 0;
  return IsInt64AndGet(instruction, &val) && val == value;
}

// Returns true iff instruction is a zero bit pattern.
inline bool IsZeroBitPattern(HInstruction* instruction) {
  return instruction->IsConstant() && instruction->AsConstant()->IsZeroBitPattern();
}

#define INSTRUCTION_TYPE_CHECK(type, super)                                    \
  inline bool HInstruction::Is##type() const { return GetKind() == k##type; }  \
  inline const H##type* HInstruction::As##type() const {                       \
    return Is##type() ? down_cast<const H##type*>(this) : nullptr;             \
  }                                                                            \
  inline H##type* HInstruction::As##type() {                                   \
    return Is##type() ? static_cast<H##type*>(this) : nullptr;                 \
  }

  FOR_EACH_CONCRETE_INSTRUCTION(INSTRUCTION_TYPE_CHECK)
#undef INSTRUCTION_TYPE_CHECK

// Create space in `blocks` for adding `number_of_new_blocks` entries
// starting at location `at`. Blocks after `at` are moved accordingly.
inline void MakeRoomFor(ArenaVector<HBasicBlock*>* blocks,
                        size_t number_of_new_blocks,
                        size_t after) {
  DCHECK_LT(after, blocks->size());
  size_t old_size = blocks->size();
  size_t new_size = old_size + number_of_new_blocks;
  blocks->resize(new_size);
  std::copy_backward(blocks->begin() + after + 1u, blocks->begin() + old_size, blocks->end());
}

/*
 * Hunt "under the hood" of array lengths (leading to array references),
 * null checks (also leading to array references), and new arrays
 * (leading to the actual length). This makes it more likely related
 * instructions become actually comparable.
 */
inline HInstruction* HuntForDeclaration(HInstruction* instruction) {
  while (instruction->IsArrayLength() ||
         instruction->IsNullCheck() ||
         instruction->IsNewArray()) {
    instruction = instruction->IsNewArray()
        ? instruction->AsNewArray()->GetLength()
        : instruction->InputAt(0);
  }
  return instruction;
}

void RemoveEnvironmentUses(HInstruction* instruction);
bool HasEnvironmentUsedByOthers(HInstruction* instruction);
void ResetEnvironmentInputRecords(HInstruction* instruction);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_H_
