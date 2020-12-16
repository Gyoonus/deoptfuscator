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

#include "load_store_elimination.h"

#include "base/array_ref.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "escape.h"
#include "load_store_analysis.h"
#include "side_effects_analysis.h"

#include <iostream>

/**
 * The general algorithm of load-store elimination (LSE).
 * Load-store analysis in the previous pass collects a list of heap locations
 * and does alias analysis of those heap locations.
 * LSE keeps track of a list of heap values corresponding to the heap
 * locations. It visits basic blocks in reverse post order and for
 * each basic block, visits instructions sequentially, and processes
 * instructions as follows:
 * - If the instruction is a load, and the heap location for that load has a
 *   valid heap value, the load can be eliminated. In order to maintain the
 *   validity of all heap locations during the optimization phase, the real
 *   elimination is delayed till the end of LSE.
 * - If the instruction is a store, it updates the heap value for the heap
 *   location of the store with the store instruction. The real heap value
 *   can be fetched from the store instruction. Heap values are invalidated
 *   for heap locations that may alias with the store instruction's heap
 *   location. The store instruction can be eliminated unless the value stored
 *   is later needed e.g. by a load from the same/aliased heap location or
 *   the heap location persists at method return/deoptimization.
 *   The store instruction is also needed if it's not used to track the heap
 *   value anymore, e.g. when it fails to merge with the heap values from other
 *   predecessors.
 * - A store that stores the same value as the heap value is eliminated.
 * - The list of heap values are merged at basic block entry from the basic
 *   block's predecessors. The algorithm is single-pass, so loop side-effects is
 *   used as best effort to decide if a heap location is stored inside the loop.
 * - A special type of objects called singletons are instantiated in the method
 *   and have a single name, i.e. no aliases. Singletons have exclusive heap
 *   locations since they have no aliases. Singletons are helpful in narrowing
 *   down the life span of a heap location such that they do not always
 *   need to participate in merging heap values. Allocation of a singleton
 *   can be eliminated if that singleton is not used and does not persist
 *   at method return/deoptimization.
 * - For newly instantiated instances, their heap values are initialized to
 *   language defined default values.
 * - Some instructions such as invokes are treated as loading and invalidating
 *   all the heap values, depending on the instruction's side effects.
 * - Finalizable objects are considered as persisting at method
 *   return/deoptimization.
 * - Currently this LSE algorithm doesn't handle SIMD graph, e.g. with VecLoad
 *   and VecStore instructions.
 * - Currently this LSE algorithm doesn't handle graph with try-catch, due to
 *   the special block merging structure.
 */

namespace art {

// An unknown heap value. Loads with such a value in the heap location cannot be eliminated.
// A heap location can be set to kUnknownHeapValue when:
// - initially set a value.
// - killed due to aliasing, merging, invocation, or loop side effects.
static HInstruction* const kUnknownHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-1));

// Default heap value after an allocation.
// A heap location can be set to that value right after an allocation.
static HInstruction* const kDefaultHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-2));

// Use HGraphDelegateVisitor for which all VisitInvokeXXX() delegate to VisitInvoke().
class LSEVisitor : public HGraphDelegateVisitor {
 public:
  LSEVisitor(HGraph* graph,
             const HeapLocationCollector& heap_locations_collector,
             const SideEffectsAnalysis& side_effects,
             OptimizingCompilerStats* stats)
      : HGraphDelegateVisitor(graph, stats),
        heap_location_collector_(heap_locations_collector),
        side_effects_(side_effects),
        allocator_(graph->GetArenaStack()),
        heap_values_for_(graph->GetBlocks().size(),
                         ScopedArenaVector<HInstruction*>(heap_locations_collector.
                                                          GetNumberOfHeapLocations(),
                                                          kUnknownHeapValue,
                                                          allocator_.Adapter(kArenaAllocLSE)),
                         allocator_.Adapter(kArenaAllocLSE)),
        removed_loads_(allocator_.Adapter(kArenaAllocLSE)),
        substitute_instructions_for_loads_(allocator_.Adapter(kArenaAllocLSE)),
        possibly_removed_stores_(allocator_.Adapter(kArenaAllocLSE)),
        singleton_new_instances_(allocator_.Adapter(kArenaAllocLSE)) {
  }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // Populate the heap_values array for this block.
    // TODO: try to reuse the heap_values array from one predecessor if possible.
    if (block->IsLoopHeader()) {
      HandleLoopSideEffects(block);
    } else {
      MergePredecessorValues(block);
    }
    HGraphVisitor::VisitBasicBlock(block);
  }

  HTypeConversion* AddTypeConversionIfNecessary(HInstruction* instruction,
                                                HInstruction* value,
                                                DataType::Type expected_type) {
    HTypeConversion* type_conversion = nullptr;
    // Should never add type conversion into boolean value.
    if (expected_type != DataType::Type::kBool &&
        !DataType::IsTypeConversionImplicit(value->GetType(), expected_type)) {
      type_conversion = new (GetGraph()->GetAllocator()) HTypeConversion(
          expected_type, value, instruction->GetDexPc());
      instruction->GetBlock()->InsertInstructionBefore(type_conversion, instruction);
    }
    return type_conversion;
  }

  // Find an instruction's substitute if it's a removed load.
  // Return the same instruction if it should not be removed.
  HInstruction* FindSubstitute(HInstruction* instruction) {
    if (!IsLoad(instruction)) {
      return instruction;
    }
    size_t size = removed_loads_.size();
    for (size_t i = 0; i < size; i++) {
      if (removed_loads_[i] == instruction) {
        HInstruction* substitute = substitute_instructions_for_loads_[i];
        // The substitute list is a flat hierarchy.
        DCHECK_EQ(FindSubstitute(substitute), substitute);
        return substitute;
      }
    }
    return instruction;
  }

  void AddRemovedLoad(HInstruction* load, HInstruction* heap_value) {
    DCHECK(IsLoad(load));
    DCHECK_EQ(FindSubstitute(heap_value), heap_value) <<
        "Unexpected heap_value that has a substitute " << heap_value->DebugName();
    removed_loads_.push_back(load);
    substitute_instructions_for_loads_.push_back(heap_value);
  }

  // Scan the list of removed loads to see if we can reuse `type_conversion`, if
  // the other removed load has the same substitute and type and is dominated
  // by `type_conversioni`.
  void TryToReuseTypeConversion(HInstruction* type_conversion, size_t index) {
    size_t size = removed_loads_.size();
    HInstruction* load = removed_loads_[index];
    HInstruction* substitute = substitute_instructions_for_loads_[index];
    for (size_t j = index + 1; j < size; j++) {
      HInstruction* load2 = removed_loads_[j];
      HInstruction* substitute2 = substitute_instructions_for_loads_[j];
      if (load2 == nullptr) {
        DCHECK(substitute2->IsTypeConversion());
        continue;
      }
      DCHECK(load2->IsInstanceFieldGet() ||
             load2->IsStaticFieldGet() ||
             load2->IsArrayGet());
      DCHECK(substitute2 != nullptr);
      if (substitute2 == substitute &&
          load2->GetType() == load->GetType() &&
          type_conversion->GetBlock()->Dominates(load2->GetBlock()) &&
          // Don't share across irreducible loop headers.
          // TODO: can be more fine-grained than this by testing each dominator.
          (load2->GetBlock() == type_conversion->GetBlock() ||
           !GetGraph()->HasIrreducibleLoops())) {
        // The removed_loads_ are added in reverse post order.
        DCHECK(type_conversion->StrictlyDominates(load2));
        load2->ReplaceWith(type_conversion);
        load2->GetBlock()->RemoveInstruction(load2);
        removed_loads_[j] = nullptr;
        substitute_instructions_for_loads_[j] = type_conversion;
      }
    }
  }

  // Remove recorded instructions that should be eliminated.
  void RemoveInstructions() {
    size_t size = removed_loads_.size();
    DCHECK_EQ(size, substitute_instructions_for_loads_.size());
    for (size_t i = 0; i < size; i++) {
      HInstruction* load = removed_loads_[i];
      if (load == nullptr) {
        // The load has been handled in the scan for type conversion below.
        DCHECK(substitute_instructions_for_loads_[i]->IsTypeConversion());
        continue;
      }
      DCHECK(load->IsInstanceFieldGet() ||
             load->IsStaticFieldGet() ||
             load->IsArrayGet());
      HInstruction* substitute = substitute_instructions_for_loads_[i];
      DCHECK(substitute != nullptr);
      // We proactively retrieve the substitute for a removed load, so
      // a load that has a substitute should not be observed as a heap
      // location value.
      DCHECK_EQ(FindSubstitute(substitute), substitute);

      // The load expects to load the heap value as type load->GetType().
      // However the tracked heap value may not be of that type. An explicit
      // type conversion may be needed.
      // There are actually three types involved here:
      // (1) tracked heap value's type (type A)
      // (2) heap location (field or element)'s type (type B)
      // (3) load's type (type C)
      // We guarantee that type A stored as type B and then fetched out as
      // type C is the same as casting from type A to type C directly, since
      // type B and type C will have the same size which is guarenteed in
      // HInstanceFieldGet/HStaticFieldGet/HArrayGet's SetType().
      // So we only need one type conversion from type A to type C.
      HTypeConversion* type_conversion = AddTypeConversionIfNecessary(
          load, substitute, load->GetType());
      if (type_conversion != nullptr) {
        TryToReuseTypeConversion(type_conversion, i);
        load->ReplaceWith(type_conversion);
        substitute_instructions_for_loads_[i] = type_conversion;
      } else {
        load->ReplaceWith(substitute);
      }
      load->GetBlock()->RemoveInstruction(load);
    }

    // At this point, stores in possibly_removed_stores_ can be safely removed.
    for (HInstruction* store : possibly_removed_stores_) {
      DCHECK(store->IsInstanceFieldSet() || store->IsStaticFieldSet() || store->IsArraySet());
      store->GetBlock()->RemoveInstruction(store);
    }

    // Eliminate singleton-classified instructions:
    //   * - Constructor fences (they never escape this thread).
    //   * - Allocations (if they are unused).
    for (HInstruction* new_instance : singleton_new_instances_) {
      size_t removed = HConstructorFence::RemoveConstructorFences(new_instance);
      MaybeRecordStat(stats_,
                      MethodCompilationStat::kConstructorFenceRemovedLSE,
                      removed);

      if (!new_instance->HasNonEnvironmentUses()) {
        new_instance->RemoveEnvironmentUsers();
        new_instance->GetBlock()->RemoveInstruction(new_instance);
      }
    }
  }

 private:
  static bool IsLoad(HInstruction* instruction) {
    if (instruction == kUnknownHeapValue || instruction == kDefaultHeapValue) {
      return false;
    }
    // Unresolved load is not treated as a load.
    return instruction->IsInstanceFieldGet() ||
        instruction->IsStaticFieldGet() ||
        instruction->IsArrayGet();
  }

  static bool IsStore(HInstruction* instruction) {
    if (instruction == kUnknownHeapValue || instruction == kDefaultHeapValue) {
      return false;
    }
    // Unresolved store is not treated as a store.
    return instruction->IsInstanceFieldSet() ||
        instruction->IsArraySet() ||
        instruction->IsStaticFieldSet();
  }

  // Returns the real heap value by finding its substitute or by "peeling"
  // a store instruction.
  HInstruction* GetRealHeapValue(HInstruction* heap_value) {
    if (IsLoad(heap_value)) {
      return FindSubstitute(heap_value);
    }
    if (!IsStore(heap_value)) {
      return heap_value;
    }

    // We keep track of store instructions as the heap values which might be
    // eliminated if the stores are later found not necessary. The real stored
    // value needs to be fetched from the store instruction.
    if (heap_value->IsInstanceFieldSet()) {
      heap_value = heap_value->AsInstanceFieldSet()->GetValue();
    } else if (heap_value->IsStaticFieldSet()) {
      heap_value = heap_value->AsStaticFieldSet()->GetValue();
    } else {
      DCHECK(heap_value->IsArraySet());
      heap_value = heap_value->AsArraySet()->GetValue();
    }
    // heap_value may already be a removed load.
    return FindSubstitute(heap_value);
  }

  // If heap_value is a store, need to keep the store.
  // This is necessary if a heap value is killed or replaced by another value,
  // so that the store is no longer used to track heap value.
  void KeepIfIsStore(HInstruction* heap_value) {
    if (!IsStore(heap_value)) {
      return;
    }
    auto idx = std::find(possibly_removed_stores_.begin(),
        possibly_removed_stores_.end(), heap_value);
    if (idx != possibly_removed_stores_.end()) {
      // Make sure the store is kept.
      possibly_removed_stores_.erase(idx);
    }
  }

  // If a heap location X may alias with heap location at `loc_index`
  // and heap_values of that heap location X holds a store, keep that store.
  // It's needed for a dependent load that's not eliminated since any store
  // that may put value into the load's heap location needs to be kept.
  void KeepStoresIfAliasedToLocation(ScopedArenaVector<HInstruction*>& heap_values,
                                     size_t loc_index) {
    for (size_t i = 0; i < heap_values.size(); i++) {
      if ((i == loc_index) || heap_location_collector_.MayAlias(i, loc_index)) {
        KeepIfIsStore(heap_values[i]);
      }
    }
  }

  void HandleLoopSideEffects(HBasicBlock* block) {
    DCHECK(block->IsLoopHeader());
    int block_id = block->GetBlockId();
    ScopedArenaVector<HInstruction*>& heap_values = heap_values_for_[block_id];
    HBasicBlock* pre_header = block->GetLoopInformation()->GetPreHeader();
    ScopedArenaVector<HInstruction*>& pre_header_heap_values =
        heap_values_for_[pre_header->GetBlockId()];

    // Don't eliminate loads in irreducible loops.
    // Also keep the stores before the loop.
    if (block->GetLoopInformation()->IsIrreducible()) {
      if (kIsDebugBuild) {
        for (size_t i = 0; i < heap_values.size(); i++) {
          DCHECK_EQ(heap_values[i], kUnknownHeapValue);
        }
      }
      for (size_t i = 0; i < heap_values.size(); i++) {
        KeepIfIsStore(pre_header_heap_values[i]);
      }
      return;
    }

    // Inherit the values from pre-header.
    for (size_t i = 0; i < heap_values.size(); i++) {
      heap_values[i] = pre_header_heap_values[i];
    }

    // We do a single pass in reverse post order. For loops, use the side effects as a hint
    // to see if the heap values should be killed.
    if (side_effects_.GetLoopEffects(block).DoesAnyWrite()) {
      for (size_t i = 0; i < heap_values.size(); i++) {
        HeapLocation* location = heap_location_collector_.GetHeapLocation(i);
        ReferenceInfo* ref_info = location->GetReferenceInfo();
        if (ref_info->IsSingleton() && !location->IsValueKilledByLoopSideEffects()) {
          // A singleton's field that's not stored into inside a loop is
          // invariant throughout the loop. Nothing to do.
        } else {
          // heap value is killed by loop side effects.
          KeepIfIsStore(pre_header_heap_values[i]);
          heap_values[i] = kUnknownHeapValue;
        }
      }
    } else {
      // The loop doesn't kill any value.
    }
  }

  void MergePredecessorValues(HBasicBlock* block) {
    ArrayRef<HBasicBlock* const> predecessors(block->GetPredecessors());
    if (predecessors.size() == 0) {
      return;
    }
    if (block->IsExitBlock()) {
      // Exit block doesn't really merge values since the control flow ends in
      // its predecessors. Each predecessor needs to make sure stores are kept
      // if necessary.
      return;
    }

    ScopedArenaVector<HInstruction*>& heap_values = heap_values_for_[block->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* merged_value = nullptr;
      // If we can merge the store itself from the predecessors, we keep
      // the store as the heap value as long as possible. In case we cannot
      // merge the store, we try to merge the values of the stores.
      HInstruction* merged_store_value = nullptr;
      // Whether merged_value is a result that's merged from all predecessors.
      bool from_all_predecessors = true;
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      HInstruction* ref = ref_info->GetReference();
      HInstruction* singleton_ref = nullptr;
      if (ref_info->IsSingleton()) {
        // We do more analysis based on singleton's liveness when merging
        // heap values for such cases.
        singleton_ref = ref;
      }

      for (HBasicBlock* predecessor : predecessors) {
        HInstruction* pred_value = heap_values_for_[predecessor->GetBlockId()][i];
        if (!IsStore(pred_value)) {
          pred_value = FindSubstitute(pred_value);
        }
        DCHECK(pred_value != nullptr);
        HInstruction* pred_store_value = GetRealHeapValue(pred_value);
        if ((singleton_ref != nullptr) &&
            !singleton_ref->GetBlock()->Dominates(predecessor)) {
          // singleton_ref is not live in this predecessor. No need to merge
          // since singleton_ref is not live at the beginning of this block.
          DCHECK_EQ(pred_value, kUnknownHeapValue);
          from_all_predecessors = false;
          break;
        }
        if (merged_value == nullptr) {
          // First seen heap value.
          DCHECK(pred_value != nullptr);
          merged_value = pred_value;
        } else if (pred_value != merged_value) {
          // There are conflicting values.
          merged_value = kUnknownHeapValue;
          // We may still be able to merge store values.
        }

        // Conflicting stores may be storing the same value. We do another merge
        // of real stored values.
        if (merged_store_value == nullptr) {
          // First seen store value.
          DCHECK(pred_store_value != nullptr);
          merged_store_value = pred_store_value;
        } else if (pred_store_value != merged_store_value) {
          // There are conflicting store values.
          merged_store_value = kUnknownHeapValue;
          // There must be conflicting stores also.
          DCHECK_EQ(merged_value, kUnknownHeapValue);
          // No need to merge anymore.
          break;
        }
      }

      if (merged_value == nullptr) {
        DCHECK(!from_all_predecessors);
        DCHECK(singleton_ref != nullptr);
      }
      if (from_all_predecessors) {
        if (ref_info->IsSingletonAndRemovable() &&
            block->IsSingleReturnOrReturnVoidAllowingPhis()) {
          // Values in the singleton are not needed anymore.
        } else if (!IsStore(merged_value)) {
          // We don't track merged value as a store anymore. We have to
          // hold the stores in predecessors live here.
          for (HBasicBlock* predecessor : predecessors) {
            ScopedArenaVector<HInstruction*>& pred_values =
                heap_values_for_[predecessor->GetBlockId()];
            KeepIfIsStore(pred_values[i]);
          }
        }
      } else {
        DCHECK(singleton_ref != nullptr);
        // singleton_ref is non-existing at the beginning of the block. There is
        // no need to keep the stores.
      }

      if (!from_all_predecessors) {
        DCHECK(singleton_ref != nullptr);
        DCHECK((singleton_ref->GetBlock() == block) ||
               !singleton_ref->GetBlock()->Dominates(block))
            << "method: " << GetGraph()->GetMethodName();
        // singleton_ref is not defined before block or defined only in some of its
        // predecessors, so block doesn't really have the location at its entry.
        heap_values[i] = kUnknownHeapValue;
      } else if (predecessors.size() == 1) {
        // Inherit heap value from the single predecessor.
        DCHECK_EQ(heap_values_for_[predecessors[0]->GetBlockId()][i], merged_value);
        heap_values[i] = merged_value;
      } else {
        DCHECK(merged_value == kUnknownHeapValue ||
               merged_value == kDefaultHeapValue ||
               merged_value->GetBlock()->Dominates(block));
        if (merged_value != kUnknownHeapValue) {
          heap_values[i] = merged_value;
        } else {
          // Stores in different predecessors may be storing the same value.
          heap_values[i] = merged_store_value;
        }
      }
    }
  }

  // `instruction` is being removed. Try to see if the null check on it
  // can be removed. This can happen if the same value is set in two branches
  // but not in dominators. Such as:
  //   int[] a = foo();
  //   if () {
  //     a[0] = 2;
  //   } else {
  //     a[0] = 2;
  //   }
  //   // a[0] can now be replaced with constant 2, and the null check on it can be removed.
  void TryRemovingNullCheck(HInstruction* instruction) {
    HInstruction* prev = instruction->GetPrevious();
    if ((prev != nullptr) && prev->IsNullCheck() && (prev == instruction->InputAt(0))) {
      // Previous instruction is a null check for this instruction. Remove the null check.
      prev->ReplaceWith(prev->InputAt(0));
      prev->GetBlock()->RemoveInstruction(prev);
    }
  }

  HInstruction* GetDefaultValue(DataType::Type type) {
    switch (type) {
      case DataType::Type::kReference:
        return GetGraph()->GetNullConstant();
      case DataType::Type::kBool:
      case DataType::Type::kUint8:
      case DataType::Type::kInt8:
      case DataType::Type::kUint16:
      case DataType::Type::kInt16:
      case DataType::Type::kInt32:
        return GetGraph()->GetIntConstant(0);
      case DataType::Type::kInt64:
        return GetGraph()->GetLongConstant(0);
      case DataType::Type::kFloat32:
        return GetGraph()->GetFloatConstant(0);
      case DataType::Type::kFloat64:
        return GetGraph()->GetDoubleConstant(0);
      default:
        UNREACHABLE();
    }
  }

  void VisitGetLocation(HInstruction* instruction,
                        HInstruction* ref,
                        size_t offset,
                        HInstruction* index,
                        size_t vector_length,
                        int16_t declaring_class_def_index) {
    HInstruction* original_ref = heap_location_collector_.HuntForOriginalReference(ref);
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(original_ref);
    size_t idx = heap_location_collector_.FindHeapLocationIndex(
        ref_info, offset, index, vector_length, declaring_class_def_index);
    DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    HInstruction* heap_value = heap_values[idx];
    if (heap_value == kDefaultHeapValue) {
      HInstruction* constant = GetDefaultValue(instruction->GetType());
      AddRemovedLoad(instruction, constant);
      heap_values[idx] = constant;
      return;
    }
    heap_value = GetRealHeapValue(heap_value);
    if (heap_value == kUnknownHeapValue) {
      // Load isn't eliminated. Put the load as the value into the HeapLocation.
      // This acts like GVN but with better aliasing analysis.
      heap_values[idx] = instruction;
      KeepStoresIfAliasedToLocation(heap_values, idx);
    } else {
      if (DataType::Kind(heap_value->GetType()) != DataType::Kind(instruction->GetType())) {
        // The only situation where the same heap location has different type is when
        // we do an array get on an instruction that originates from the null constant
        // (the null could be behind a field access, an array access, a null check or
        // a bound type).
        // In order to stay properly typed on primitive types, we do not eliminate
        // the array gets.
        if (kIsDebugBuild) {
          DCHECK(heap_value->IsArrayGet()) << heap_value->DebugName();
          DCHECK(instruction->IsArrayGet()) << instruction->DebugName();
        }
        // Load isn't eliminated. Put the load as the value into the HeapLocation.
        // This acts like GVN but with better aliasing analysis.
        heap_values[idx] = instruction;
        KeepStoresIfAliasedToLocation(heap_values, idx);
        return;
      }
      AddRemovedLoad(instruction, heap_value);
      TryRemovingNullCheck(instruction);
    }
  }

  bool Equal(HInstruction* heap_value, HInstruction* value) {
    DCHECK(!IsStore(value)) << value->DebugName();
    if (heap_value == kUnknownHeapValue) {
      // Don't compare kUnknownHeapValue with other values.
      return false;
    }
    if (heap_value == value) {
      return true;
    }
    if (heap_value == kDefaultHeapValue && GetDefaultValue(value->GetType()) == value) {
      return true;
    }
    HInstruction* real_heap_value = GetRealHeapValue(heap_value);
    if (real_heap_value != heap_value) {
      return Equal(real_heap_value, value);
    }
    return false;
  }

  void VisitSetLocation(HInstruction* instruction,
                        HInstruction* ref,
                        size_t offset,
                        HInstruction* index,
                        size_t vector_length,
                        int16_t declaring_class_def_index,
                        HInstruction* value) {
    DCHECK(!IsStore(value)) << value->DebugName();
    // value may already have a substitute.
    value = FindSubstitute(value);
    HInstruction* original_ref = heap_location_collector_.HuntForOriginalReference(ref);
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(original_ref);
    size_t idx = heap_location_collector_.FindHeapLocationIndex(
        ref_info, offset, index, vector_length, declaring_class_def_index);
    DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    HInstruction* heap_value = heap_values[idx];
    bool possibly_redundant = false;

    if (Equal(heap_value, value)) {
      // Store into the heap location with the same value.
      // This store can be eliminated right away.
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    } else {
      HLoopInformation* loop_info = instruction->GetBlock()->GetLoopInformation();
      if (loop_info == nullptr) {
        // Store is not in a loop. We try to precisely track the heap value by
        // the store.
        possibly_redundant = true;
      } else if (!loop_info->IsIrreducible()) {
        // instruction is a store in the loop so the loop must do write.
        DCHECK(side_effects_.GetLoopEffects(loop_info->GetHeader()).DoesAnyWrite());
        if (ref_info->IsSingleton() && !loop_info->IsDefinedOutOfTheLoop(original_ref)) {
          // original_ref is created inside the loop. Value stored to it isn't needed at
          // the loop header. This is true for outer loops also.
          possibly_redundant = true;
        } else {
          // Keep the store since its value may be needed at the loop header.
        }
      } else {
        // Keep the store inside irreducible loops.
      }
    }
    if (possibly_redundant) {
      possibly_removed_stores_.push_back(instruction);
    }

    // Put the store as the heap value. If the value is loaded or needed after
    // return/deoptimization later, this store isn't really redundant.
    heap_values[idx] = instruction;

    // This store may kill values in other heap locations due to aliasing.
    for (size_t i = 0; i < heap_values.size(); i++) {
      if (i == idx) {
        continue;
      }
      if (Equal(heap_values[i], value)) {
        // Same value should be kept even if aliasing happens.
        continue;
      }
      if (heap_values[i] == kUnknownHeapValue) {
        // Value is already unknown, no need for aliasing check.
        continue;
      }
      if (heap_location_collector_.MayAlias(i, idx)) {
        // Kill heap locations that may alias and as a result if the heap value
        // is a store, the store needs to be kept.
        KeepIfIsStore(heap_values[i]);
        heap_values[i] = kUnknownHeapValue;
      }
    }
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instruction) OVERRIDE {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    VisitGetLocation(instruction,
                     obj,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) OVERRIDE {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction,
                     obj,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index,
                     value);
  }

  void VisitStaticFieldGet(HStaticFieldGet* instruction) OVERRIDE {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    VisitGetLocation(instruction,
                     cls,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) OVERRIDE {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction,
                     cls,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index,
                     value);
  }

  void VisitArrayGet(HArrayGet* instruction) OVERRIDE {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    VisitGetLocation(instruction,
                     array,
                     HeapLocation::kInvalidFieldOffset,
                     index,
                     HeapLocation::kScalar,
                     HeapLocation::kDeclaringClassDefIndexForArrays);
  }

  void VisitArraySet(HArraySet* instruction) OVERRIDE {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    HInstruction* value = instruction->InputAt(2);
    VisitSetLocation(instruction,
                     array,
                     HeapLocation::kInvalidFieldOffset,
                     index,
                     HeapLocation::kScalar,
                     HeapLocation::kDeclaringClassDefIndexForArrays,
                     value);
  }

  void VisitDeoptimize(HDeoptimize* instruction) {
    const ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (HInstruction* heap_value : heap_values) {
      // A store is kept as the heap value for possibly removed stores.
      // That value stored is generally observeable after deoptimization, except
      // for singletons that don't escape after deoptimization.
      if (IsStore(heap_value)) {
        if (heap_value->IsStaticFieldSet()) {
          KeepIfIsStore(heap_value);
          continue;
        }
        HInstruction* reference = heap_value->InputAt(0);
        if (heap_location_collector_.FindReferenceInfoOf(reference)->IsSingleton()) {
          if (reference->IsNewInstance() && reference->AsNewInstance()->IsFinalizable()) {
            // Finalizable objects alway escape.
            KeepIfIsStore(heap_value);
            continue;
          }
          // Check whether the reference for a store is used by an environment local of
          // HDeoptimize. If not, the singleton is not observed after
          // deoptimizion.
          for (const HUseListNode<HEnvironment*>& use : reference->GetEnvUses()) {
            HEnvironment* user = use.GetUser();
            if (user->GetHolder() == instruction) {
              // The singleton for the store is visible at this deoptimization
              // point. Need to keep the store so that the heap value is
              // seen by the interpreter.
              KeepIfIsStore(heap_value);
            }
          }
        } else {
          KeepIfIsStore(heap_value);
        }
      }
    }
  }

  // Keep necessary stores before exiting a method via return/throw.
  void HandleExit(HBasicBlock* block) {
    const ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[block->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* heap_value = heap_values[i];
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (!ref_info->IsSingletonAndRemovable()) {
        KeepIfIsStore(heap_value);
      }
    }
  }

  void VisitReturn(HReturn* instruction) OVERRIDE {
    HandleExit(instruction->GetBlock());
  }

  void VisitReturnVoid(HReturnVoid* return_void) OVERRIDE {
    HandleExit(return_void->GetBlock());
  }

  void VisitThrow(HThrow* throw_instruction) OVERRIDE {
    HandleExit(throw_instruction->GetBlock());
  }

  void HandleInvoke(HInstruction* instruction) {
    SideEffects side_effects = instruction->GetSideEffects();
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (ref_info->IsSingleton()) {
        // Singleton references cannot be seen by the callee.
      } else {
        if (side_effects.DoesAnyRead()) {
          // Invocation may read the heap value.
          KeepIfIsStore(heap_values[i]);
        }
        if (side_effects.DoesAnyWrite()) {
          // Keep the store since it's not used to track the heap value anymore.
          KeepIfIsStore(heap_values[i]);
          heap_values[i] = kUnknownHeapValue;
        }
      }
    }
  }

  void VisitInvoke(HInvoke* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitClinitCheck(HClinitCheck* clinit) OVERRIDE {
    HandleInvoke(clinit);
  }

  void VisitUnresolvedInstanceFieldGet(HUnresolvedInstanceFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedInstanceFieldSet(HUnresolvedInstanceFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldGet(HUnresolvedStaticFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldSet(HUnresolvedStaticFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitNewInstance(HNewInstance* new_instance) OVERRIDE {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_instance);
    if (ref_info == nullptr) {
      // new_instance isn't used for field accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable() && !new_instance->NeedsChecks()) {
      DCHECK(!new_instance->IsFinalizable());
      // new_instance can potentially be eliminated.
      singleton_new_instances_.push_back(new_instance);
    }
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[new_instance->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* ref =
          heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo()->GetReference();
      size_t offset = heap_location_collector_.GetHeapLocation(i)->GetOffset();
      if (ref == new_instance && offset >= mirror::kObjectHeaderSize) {
        // Instance fields except the header fields are set to default heap values.
        heap_values[i] = kDefaultHeapValue;
      }
    }
  }

  void VisitNewArray(HNewArray* new_array) OVERRIDE {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_array);
    if (ref_info == nullptr) {
      // new_array isn't used for array accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable()) {
      if (new_array->GetLength()->IsIntConstant() &&
          new_array->GetLength()->AsIntConstant()->GetValue() >= 0) {
        // new_array can potentially be eliminated.
        singleton_new_instances_.push_back(new_array);
      } else {
        // new_array may throw NegativeArraySizeException. Keep it.
      }
    }
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[new_array->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HeapLocation* location = heap_location_collector_.GetHeapLocation(i);
      HInstruction* ref = location->GetReferenceInfo()->GetReference();
      if (ref == new_array && location->GetIndex() != nullptr) {
        // Array elements are set to default heap values.
        heap_values[i] = kDefaultHeapValue;
      }
    }
  }

  const HeapLocationCollector& heap_location_collector_;
  const SideEffectsAnalysis& side_effects_;

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator_;

  // One array of heap values for each block.
  ScopedArenaVector<ScopedArenaVector<HInstruction*>> heap_values_for_;

  // We record the instructions that should be eliminated but may be
  // used by heap locations. They'll be removed in the end.
  ScopedArenaVector<HInstruction*> removed_loads_;
  ScopedArenaVector<HInstruction*> substitute_instructions_for_loads_;

  // Stores in this list may be removed from the list later when it's
  // found that the store cannot be eliminated.
  ScopedArenaVector<HInstruction*> possibly_removed_stores_;

  ScopedArenaVector<HInstruction*> singleton_new_instances_;

  DISALLOW_COPY_AND_ASSIGN(LSEVisitor);
};

void LoadStoreElimination::Run() {
  if (graph_->IsDebuggable() || graph_->HasTryCatch()) {
    // Debugger may set heap values or trigger deoptimization of callers.
    // Try/catch support not implemented yet.
    // Skip this optimization.
    return;
  }
  const HeapLocationCollector& heap_location_collector = lsa_.GetHeapLocationCollector();
  if (heap_location_collector.GetNumberOfHeapLocations() == 0) {
    // No HeapLocation information from LSA, skip this optimization.
    return;
  }

  // TODO: analyze VecLoad/VecStore better.
  if (graph_->HasSIMD()) {
    return;
  }

  LSEVisitor lse_visitor(graph_, heap_location_collector, side_effects_, stats_);
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    lse_visitor.VisitBasicBlock(block);
  }
  lse_visitor.RemoveInstructions();
}

}  // namespace art
