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

#include <string>

#include "scheduler.h"

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "data_type-inl.h"
#include "prepare_for_register_allocation.h"

#ifdef ART_ENABLE_CODEGEN_arm64
#include "scheduler_arm64.h"
#endif

#ifdef ART_ENABLE_CODEGEN_arm
#include "scheduler_arm.h"
#endif

namespace art {

void SchedulingGraph::AddDependency(SchedulingNode* node,
                                    SchedulingNode* dependency,
                                    bool is_data_dependency) {
  if (node == nullptr || dependency == nullptr) {
    // A `nullptr` node indicates an instruction out of scheduling range (eg. in
    // an other block), so we do not need to add a dependency edge to the graph.
    return;
  }

  if (is_data_dependency) {
    if (!HasImmediateDataDependency(node, dependency)) {
      node->AddDataPredecessor(dependency);
    }
  } else if (!HasImmediateOtherDependency(node, dependency)) {
    node->AddOtherPredecessor(dependency);
  }
}

static bool MayHaveReorderingDependency(SideEffects node, SideEffects other) {
  // Read after write.
  if (node.MayDependOn(other)) {
    return true;
  }

  // Write after read.
  if (other.MayDependOn(node)) {
    return true;
  }

  // Memory write after write.
  if (node.DoesAnyWrite() && other.DoesAnyWrite()) {
    return true;
  }

  return false;
}

size_t SchedulingGraph::ArrayAccessHeapLocation(HInstruction* array, HInstruction* index) const {
  DCHECK(heap_location_collector_ != nullptr);
  size_t heap_loc = heap_location_collector_->GetArrayHeapLocation(array, index);
  // This array access should be analyzed and added to HeapLocationCollector before.
  DCHECK(heap_loc != HeapLocationCollector::kHeapLocationNotFound);
  return heap_loc;
}

bool SchedulingGraph::ArrayAccessMayAlias(const HInstruction* node,
                                          const HInstruction* other) const {
  DCHECK(heap_location_collector_ != nullptr);
  size_t node_heap_loc = ArrayAccessHeapLocation(node->InputAt(0), node->InputAt(1));
  size_t other_heap_loc = ArrayAccessHeapLocation(other->InputAt(0), other->InputAt(1));

  // For example: arr[0] and arr[0]
  if (node_heap_loc == other_heap_loc) {
    return true;
  }

  // For example: arr[0] and arr[i]
  if (heap_location_collector_->MayAlias(node_heap_loc, other_heap_loc)) {
    return true;
  }

  return false;
}

static bool IsArrayAccess(const HInstruction* instruction) {
  return instruction->IsArrayGet() || instruction->IsArraySet();
}

static bool IsInstanceFieldAccess(const HInstruction* instruction) {
  return instruction->IsInstanceFieldGet() ||
         instruction->IsInstanceFieldSet() ||
         instruction->IsUnresolvedInstanceFieldGet() ||
         instruction->IsUnresolvedInstanceFieldSet();
}

static bool IsStaticFieldAccess(const HInstruction* instruction) {
  return instruction->IsStaticFieldGet() ||
         instruction->IsStaticFieldSet() ||
         instruction->IsUnresolvedStaticFieldGet() ||
         instruction->IsUnresolvedStaticFieldSet();
}

static bool IsResolvedFieldAccess(const HInstruction* instruction) {
  return instruction->IsInstanceFieldGet() ||
         instruction->IsInstanceFieldSet() ||
         instruction->IsStaticFieldGet() ||
         instruction->IsStaticFieldSet();
}

static bool IsUnresolvedFieldAccess(const HInstruction* instruction) {
  return instruction->IsUnresolvedInstanceFieldGet() ||
         instruction->IsUnresolvedInstanceFieldSet() ||
         instruction->IsUnresolvedStaticFieldGet() ||
         instruction->IsUnresolvedStaticFieldSet();
}

static bool IsFieldAccess(const HInstruction* instruction) {
  return IsResolvedFieldAccess(instruction) || IsUnresolvedFieldAccess(instruction);
}

static const FieldInfo* GetFieldInfo(const HInstruction* instruction) {
  if (instruction->IsInstanceFieldGet()) {
    return &instruction->AsInstanceFieldGet()->GetFieldInfo();
  } else if (instruction->IsInstanceFieldSet()) {
    return &instruction->AsInstanceFieldSet()->GetFieldInfo();
  } else if (instruction->IsStaticFieldGet()) {
    return &instruction->AsStaticFieldGet()->GetFieldInfo();
  } else if (instruction->IsStaticFieldSet()) {
    return &instruction->AsStaticFieldSet()->GetFieldInfo();
  } else {
    LOG(FATAL) << "Unexpected field access type";
    UNREACHABLE();
  }
}

size_t SchedulingGraph::FieldAccessHeapLocation(HInstruction* obj, const FieldInfo* field) const {
  DCHECK(obj != nullptr);
  DCHECK(field != nullptr);
  DCHECK(heap_location_collector_ != nullptr);

  size_t heap_loc = heap_location_collector_->GetFieldHeapLocation(obj, field);
  // This field access should be analyzed and added to HeapLocationCollector before.
  DCHECK(heap_loc != HeapLocationCollector::kHeapLocationNotFound);

  return heap_loc;
}

bool SchedulingGraph::FieldAccessMayAlias(const HInstruction* node,
                                          const HInstruction* other) const {
  DCHECK(heap_location_collector_ != nullptr);

  // Static and instance field accesses should not alias.
  if ((IsInstanceFieldAccess(node) && IsStaticFieldAccess(other)) ||
      (IsStaticFieldAccess(node) && IsInstanceFieldAccess(other))) {
    return false;
  }

  // If either of the field accesses is unresolved.
  if (IsUnresolvedFieldAccess(node) || IsUnresolvedFieldAccess(other)) {
    // Conservatively treat these two accesses may alias.
    return true;
  }

  // If both fields accesses are resolved.
  const FieldInfo* node_field = GetFieldInfo(node);
  const FieldInfo* other_field = GetFieldInfo(other);

  size_t node_loc = FieldAccessHeapLocation(node->InputAt(0), node_field);
  size_t other_loc = FieldAccessHeapLocation(other->InputAt(0), other_field);

  if (node_loc == other_loc) {
    return true;
  }

  if (!heap_location_collector_->MayAlias(node_loc, other_loc)) {
    return false;
  }

  return true;
}

bool SchedulingGraph::HasMemoryDependency(const HInstruction* node,
                                          const HInstruction* other) const {
  if (!MayHaveReorderingDependency(node->GetSideEffects(), other->GetSideEffects())) {
    return false;
  }

  if (heap_location_collector_ == nullptr ||
      heap_location_collector_->GetNumberOfHeapLocations() == 0) {
    // Without HeapLocation information from load store analysis,
    // we cannot do further disambiguation analysis on these two instructions.
    // Just simply say that those two instructions have memory dependency.
    return true;
  }

  if (IsArrayAccess(node) && IsArrayAccess(other)) {
    return ArrayAccessMayAlias(node, other);
  }
  if (IsFieldAccess(node) && IsFieldAccess(other)) {
    return FieldAccessMayAlias(node, other);
  }

  // TODO(xueliang): LSA to support alias analysis among HVecLoad, HVecStore and ArrayAccess
  if (node->IsVecMemoryOperation() && other->IsVecMemoryOperation()) {
    return true;
  }
  if (node->IsVecMemoryOperation() && IsArrayAccess(other)) {
    return true;
  }
  if (IsArrayAccess(node) && other->IsVecMemoryOperation()) {
    return true;
  }

  // Heap accesses of different kinds should not alias.
  if (IsArrayAccess(node) && IsFieldAccess(other)) {
    return false;
  }
  if (IsFieldAccess(node) && IsArrayAccess(other)) {
    return false;
  }
  if (node->IsVecMemoryOperation() && IsFieldAccess(other)) {
    return false;
  }
  if (IsFieldAccess(node) && other->IsVecMemoryOperation()) {
    return false;
  }

  // We conservatively treat all other cases having dependency,
  // for example, Invoke and ArrayGet.
  return true;
}

bool SchedulingGraph::HasExceptionDependency(const HInstruction* node,
                                             const HInstruction* other) const {
  if (other->CanThrow() && node->GetSideEffects().DoesAnyWrite()) {
    return true;
  }
  if (other->GetSideEffects().DoesAnyWrite() && node->CanThrow()) {
    return true;
  }
  if (other->CanThrow() && node->CanThrow()) {
    return true;
  }

  // Above checks should cover all cases where we cannot reorder two
  // instructions which may throw exception.
  return false;
}

// Check whether `node` depends on `other`, taking into account `SideEffect`
// information and `CanThrow` information.
bool SchedulingGraph::HasSideEffectDependency(const HInstruction* node,
                                              const HInstruction* other) const {
  if (HasMemoryDependency(node, other)) {
    return true;
  }

  // Even if above memory dependency check has passed, it is still necessary to
  // check dependencies between instructions that can throw and instructions
  // that write to memory.
  if (HasExceptionDependency(node, other)) {
    return true;
  }

  return false;
}

void SchedulingGraph::AddDependencies(HInstruction* instruction, bool is_scheduling_barrier) {
  SchedulingNode* instruction_node = GetNode(instruction);

  // Define-use dependencies.
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    AddDataDependency(GetNode(use.GetUser()), instruction_node);
  }

  // Scheduling barrier dependencies.
  DCHECK(!is_scheduling_barrier || contains_scheduling_barrier_);
  if (contains_scheduling_barrier_) {
    // A barrier depends on instructions after it. And instructions before the
    // barrier depend on it.
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      CHECK(other_node != nullptr)
          << other->DebugName()
          << " is in block " << other->GetBlock()->GetBlockId()
          << ", and expected in block " << instruction->GetBlock()->GetBlockId();
      bool other_is_barrier = other_node->IsSchedulingBarrier();
      if (is_scheduling_barrier || other_is_barrier) {
        AddOtherDependency(other_node, instruction_node);
      }
      if (other_is_barrier) {
        // This other scheduling barrier guarantees ordering of instructions after
        // it, so avoid creating additional useless dependencies in the graph.
        // For example if we have
        //     instr_1
        //     barrier_2
        //     instr_3
        //     barrier_4
        //     instr_5
        // we only create the following non-data dependencies
        //     1 -> 2
        //     2 -> 3
        //     2 -> 4
        //     3 -> 4
        //     4 -> 5
        // and do not create
        //     1 -> 4
        //     2 -> 5
        // Note that in this example we could also avoid creating the dependency
        // `2 -> 4`.  But if we remove `instr_3` that dependency is required to
        // order the barriers. So we generate it to avoid a special case.
        break;
      }
    }
  }

  // Side effect dependencies.
  if (!instruction->GetSideEffects().DoesNothing() || instruction->CanThrow()) {
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      if (other_node->IsSchedulingBarrier()) {
        // We have reached a scheduling barrier so we can stop further
        // processing.
        DCHECK(HasImmediateOtherDependency(other_node, instruction_node));
        break;
      }
      if (HasSideEffectDependency(other, instruction)) {
        AddOtherDependency(other_node, instruction_node);
      }
    }
  }

  // Environment dependencies.
  // We do not need to process those if the instruction is a scheduling barrier,
  // since the barrier already has non-data dependencies on all following
  // instructions.
  if (!is_scheduling_barrier) {
    for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
      // Note that here we could stop processing if the environment holder is
      // across a scheduling barrier. But checking this would likely require
      // more work than simply iterating through environment uses.
      AddOtherDependency(GetNode(use.GetUser()->GetHolder()), instruction_node);
    }
  }
}

bool SchedulingGraph::HasImmediateDataDependency(const SchedulingNode* node,
                                                 const SchedulingNode* other) const {
  return ContainsElement(node->GetDataPredecessors(), other);
}

bool SchedulingGraph::HasImmediateDataDependency(const HInstruction* instruction,
                                                 const HInstruction* other_instruction) const {
  const SchedulingNode* node = GetNode(instruction);
  const SchedulingNode* other = GetNode(other_instruction);
  if (node == nullptr || other == nullptr) {
    // Both instructions must be in current basic block, i.e. the SchedulingGraph can see their
    // corresponding SchedulingNode in the graph, and tell whether there is a dependency.
    // Otherwise there is no dependency from SchedulingGraph's perspective, for example,
    // instruction and other_instruction are in different basic blocks.
    return false;
  }
  return HasImmediateDataDependency(node, other);
}

bool SchedulingGraph::HasImmediateOtherDependency(const SchedulingNode* node,
                                                  const SchedulingNode* other) const {
  return ContainsElement(node->GetOtherPredecessors(), other);
}

bool SchedulingGraph::HasImmediateOtherDependency(const HInstruction* instruction,
                                                  const HInstruction* other_instruction) const {
  const SchedulingNode* node = GetNode(instruction);
  const SchedulingNode* other = GetNode(other_instruction);
  if (node == nullptr || other == nullptr) {
    // Both instructions must be in current basic block, i.e. the SchedulingGraph can see their
    // corresponding SchedulingNode in the graph, and tell whether there is a dependency.
    // Otherwise there is no dependency from SchedulingGraph's perspective, for example,
    // instruction and other_instruction are in different basic blocks.
    return false;
  }
  return HasImmediateOtherDependency(node, other);
}

static const std::string InstructionTypeId(const HInstruction* instruction) {
  return DataType::TypeId(instruction->GetType()) + std::to_string(instruction->GetId());
}

// Ideally we would reuse the graph visualizer code, but it is not available
// from here and it is not worth moving all that code only for our use.
static void DumpAsDotNode(std::ostream& output, const SchedulingNode* node) {
  const HInstruction* instruction = node->GetInstruction();
  // Use the instruction typed id as the node identifier.
  std::string instruction_id = InstructionTypeId(instruction);
  output << instruction_id << "[shape=record, label=\""
      << instruction_id << ' ' << instruction->DebugName() << " [";
  // List the instruction's inputs in its description. When visualizing the
  // graph this helps differentiating data inputs from other dependencies.
  const char* seperator = "";
  for (const HInstruction* input : instruction->GetInputs()) {
    output << seperator << InstructionTypeId(input);
    seperator = ",";
  }
  output << "]";
  // Other properties of the node.
  output << "\\ninternal_latency: " << node->GetInternalLatency();
  output << "\\ncritical_path: " << node->GetCriticalPath();
  if (node->IsSchedulingBarrier()) {
    output << "\\n(barrier)";
  }
  output << "\"];\n";
  // We want program order to go from top to bottom in the graph output, so we
  // reverse the edges and specify `dir=back`.
  for (const SchedulingNode* predecessor : node->GetDataPredecessors()) {
    const HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[label=\"" << predecessor->GetLatency() << "\",dir=back]\n";
  }
  for (const SchedulingNode* predecessor : node->GetOtherPredecessors()) {
    const HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[dir=back,color=blue]\n";
  }
}

void SchedulingGraph::DumpAsDotGraph(const std::string& description,
                                     const ScopedArenaVector<SchedulingNode*>& initial_candidates) {
  // TODO(xueliang): ideally we should move scheduling information into HInstruction, after that
  // we should move this dotty graph dump feature to visualizer, and have a compiler option for it.
  std::ofstream output("scheduling_graphs.dot", std::ofstream::out | std::ofstream::app);
  // Description of this graph, as a comment.
  output << "// " << description << "\n";
  // Start the dot graph. Use an increasing index for easier differentiation.
  output << "digraph G {\n";
  for (const auto& entry : nodes_map_) {
    SchedulingNode* node = entry.second.get();
    DumpAsDotNode(output, node);
  }
  // Create a fake 'end_of_scheduling' node to help visualization of critical_paths.
  for (SchedulingNode* node : initial_candidates) {
    const HInstruction* instruction = node->GetInstruction();
    output << InstructionTypeId(instruction) << ":s -> end_of_scheduling:n "
      << "[label=\"" << node->GetLatency() << "\",dir=back]\n";
  }
  // End of the dot graph.
  output << "}\n";
  output.close();
}

SchedulingNode* CriticalPathSchedulingNodeSelector::SelectMaterializedCondition(
    ScopedArenaVector<SchedulingNode*>* nodes, const SchedulingGraph& graph) const {
  // Schedule condition inputs that can be materialized immediately before their use.
  // In following example, after we've scheduled HSelect, we want LessThan to be scheduled
  // immediately, because it is a materialized condition, and will be emitted right before HSelect
  // in codegen phase.
  //
  // i20 HLessThan [...]                  HLessThan    HAdd      HAdd
  // i21 HAdd [...]                ===>      |          |         |
  // i22 HAdd [...]                          +----------+---------+
  // i23 HSelect [i21, i22, i20]                     HSelect

  if (prev_select_ == nullptr) {
    return nullptr;
  }

  const HInstruction* instruction = prev_select_->GetInstruction();
  const HCondition* condition = nullptr;
  DCHECK(instruction != nullptr);

  if (instruction->IsIf()) {
    condition = instruction->AsIf()->InputAt(0)->AsCondition();
  } else if (instruction->IsSelect()) {
    condition = instruction->AsSelect()->GetCondition()->AsCondition();
  }

  SchedulingNode* condition_node = (condition != nullptr) ? graph.GetNode(condition) : nullptr;

  if ((condition_node != nullptr) &&
      condition->HasOnlyOneNonEnvironmentUse() &&
      ContainsElement(*nodes, condition_node)) {
    DCHECK(!condition_node->HasUnscheduledSuccessors());
    // Remove the condition from the list of candidates and schedule it.
    RemoveElement(*nodes, condition_node);
    return condition_node;
  }

  return nullptr;
}

SchedulingNode* CriticalPathSchedulingNodeSelector::PopHighestPriorityNode(
    ScopedArenaVector<SchedulingNode*>* nodes, const SchedulingGraph& graph) {
  DCHECK(!nodes->empty());
  SchedulingNode* select_node = nullptr;

  // Optimize for materialized condition and its emit before use scenario.
  select_node = SelectMaterializedCondition(nodes, graph);

  if (select_node == nullptr) {
    // Get highest priority node based on critical path information.
    select_node = (*nodes)[0];
    size_t select = 0;
    for (size_t i = 1, e = nodes->size(); i < e; i++) {
      SchedulingNode* check = (*nodes)[i];
      SchedulingNode* candidate = (*nodes)[select];
      select_node = GetHigherPrioritySchedulingNode(candidate, check);
      if (select_node == check) {
        select = i;
      }
    }
    DeleteNodeAtIndex(nodes, select);
  }

  prev_select_ = select_node;
  return select_node;
}

SchedulingNode* CriticalPathSchedulingNodeSelector::GetHigherPrioritySchedulingNode(
    SchedulingNode* candidate, SchedulingNode* check) const {
  uint32_t candidate_path = candidate->GetCriticalPath();
  uint32_t check_path = check->GetCriticalPath();
  // First look at the critical_path.
  if (check_path != candidate_path) {
    return check_path < candidate_path ? check : candidate;
  }
  // If both critical paths are equal, schedule instructions with a higher latency
  // first in program order.
  return check->GetLatency() < candidate->GetLatency() ? check : candidate;
}

void HScheduler::Schedule(HGraph* graph) {
  // We run lsa here instead of in a separate pass to better control whether we
  // should run the analysis or not.
  LoadStoreAnalysis lsa(graph);
  if (!only_optimize_loop_blocks_ || graph->HasLoops()) {
    lsa.Run();
    scheduling_graph_.SetHeapLocationCollector(lsa.GetHeapLocationCollector());
  }

  for (HBasicBlock* block : graph->GetReversePostOrder()) {
    if (IsSchedulable(block)) {
      Schedule(block);
    }
  }
}

void HScheduler::Schedule(HBasicBlock* block) {
  ScopedArenaVector<SchedulingNode*> scheduling_nodes(allocator_->Adapter(kArenaAllocScheduler));

  // Build the scheduling graph.
  scheduling_graph_.Clear();
  for (HBackwardInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instruction = it.Current();
    CHECK_EQ(instruction->GetBlock(), block)
        << instruction->DebugName()
        << " is in block " << instruction->GetBlock()->GetBlockId()
        << ", and expected in block " << block->GetBlockId();
    SchedulingNode* node = scheduling_graph_.AddNode(instruction, IsSchedulingBarrier(instruction));
    CalculateLatency(node);
    scheduling_nodes.push_back(node);
  }

  if (scheduling_graph_.Size() <= 1) {
    scheduling_graph_.Clear();
    return;
  }

  cursor_ = block->GetLastInstruction();

  // Find the initial candidates for scheduling.
  candidates_.clear();
  for (SchedulingNode* node : scheduling_nodes) {
    if (!node->HasUnscheduledSuccessors()) {
      node->MaybeUpdateCriticalPath(node->GetLatency());
      candidates_.push_back(node);
    }
  }

  ScopedArenaVector<SchedulingNode*> initial_candidates(allocator_->Adapter(kArenaAllocScheduler));
  if (kDumpDotSchedulingGraphs) {
    // Remember the list of initial candidates for debug output purposes.
    initial_candidates.assign(candidates_.begin(), candidates_.end());
  }

  // Schedule all nodes.
  while (!candidates_.empty()) {
    Schedule(selector_->PopHighestPriorityNode(&candidates_, scheduling_graph_));
  }

  if (kDumpDotSchedulingGraphs) {
    // Dump the graph in `dot` format.
    HGraph* graph = block->GetGraph();
    std::stringstream description;
    description << graph->GetDexFile().PrettyMethod(graph->GetMethodIdx())
        << " B" << block->GetBlockId();
    scheduling_graph_.DumpAsDotGraph(description.str(), initial_candidates);
  }
}

void HScheduler::Schedule(SchedulingNode* scheduling_node) {
  // Check whether any of the node's predecessors will be valid candidates after
  // this node is scheduled.
  uint32_t path_to_node = scheduling_node->GetCriticalPath();
  for (SchedulingNode* predecessor : scheduling_node->GetDataPredecessors()) {
    predecessor->MaybeUpdateCriticalPath(
        path_to_node + predecessor->GetInternalLatency() + predecessor->GetLatency());
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates_.push_back(predecessor);
    }
  }
  for (SchedulingNode* predecessor : scheduling_node->GetOtherPredecessors()) {
    // Do not update the critical path.
    // The 'other' (so 'non-data') dependencies (usually) do not represent a
    // 'material' dependency of nodes on others. They exist for program
    // correctness. So we do not use them to compute the critical path.
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates_.push_back(predecessor);
    }
  }

  Schedule(scheduling_node->GetInstruction());
}

// Move an instruction after cursor instruction inside one basic block.
static void MoveAfterInBlock(HInstruction* instruction, HInstruction* cursor) {
  DCHECK_EQ(instruction->GetBlock(), cursor->GetBlock());
  DCHECK_NE(cursor, cursor->GetBlock()->GetLastInstruction());
  DCHECK(!instruction->IsControlFlow());
  DCHECK(!cursor->IsControlFlow());
  instruction->MoveBefore(cursor->GetNext(), /* do_checks */ false);
}

void HScheduler::Schedule(HInstruction* instruction) {
  if (instruction == cursor_) {
    cursor_ = cursor_->GetPrevious();
  } else {
    MoveAfterInBlock(instruction, cursor_);
  }
}

bool HScheduler::IsSchedulable(const HInstruction* instruction) const {
  // We want to avoid exhaustively listing all instructions, so we first check
  // for instruction categories that we know are safe.
  if (instruction->IsControlFlow() ||
      instruction->IsConstant()) {
    return true;
  }
  // Currently all unary and binary operations are safe to schedule, so avoid
  // checking for each of them individually.
  // Since nothing prevents a new scheduling-unsafe HInstruction to subclass
  // HUnaryOperation (or HBinaryOperation), check in debug mode that we have
  // the exhaustive lists here.
  if (instruction->IsUnaryOperation()) {
    DCHECK(instruction->IsBooleanNot() ||
           instruction->IsNot() ||
           instruction->IsNeg()) << "unexpected instruction " << instruction->DebugName();
    return true;
  }
  if (instruction->IsBinaryOperation()) {
    DCHECK(instruction->IsAdd() ||
           instruction->IsAnd() ||
           instruction->IsCompare() ||
           instruction->IsCondition() ||
           instruction->IsDiv() ||
           instruction->IsMul() ||
           instruction->IsOr() ||
           instruction->IsRem() ||
           instruction->IsRor() ||
           instruction->IsShl() ||
           instruction->IsShr() ||
           instruction->IsSub() ||
           instruction->IsUShr() ||
           instruction->IsXor()) << "unexpected instruction " << instruction->DebugName();
    return true;
  }
  // The scheduler should not see any of these.
  DCHECK(!instruction->IsParallelMove()) << "unexpected instruction " << instruction->DebugName();
  // List of instructions explicitly excluded:
  //    HClearException
  //    HClinitCheck
  //    HDeoptimize
  //    HLoadClass
  //    HLoadException
  //    HMemoryBarrier
  //    HMonitorOperation
  //    HNativeDebugInfo
  //    HThrow
  //    HTryBoundary
  // TODO: Some of the instructions above may be safe to schedule (maybe as
  // scheduling barriers).
  return instruction->IsArrayGet() ||
      instruction->IsArraySet() ||
      instruction->IsArrayLength() ||
      instruction->IsBoundType() ||
      instruction->IsBoundsCheck() ||
      instruction->IsCheckCast() ||
      instruction->IsClassTableGet() ||
      instruction->IsCurrentMethod() ||
      instruction->IsDivZeroCheck() ||
      (instruction->IsInstanceFieldGet() && !instruction->AsInstanceFieldGet()->IsVolatile()) ||
      (instruction->IsInstanceFieldSet() && !instruction->AsInstanceFieldSet()->IsVolatile()) ||
      instruction->IsInstanceOf() ||
      instruction->IsInvokeInterface() ||
      instruction->IsInvokeStaticOrDirect() ||
      instruction->IsInvokeUnresolved() ||
      instruction->IsInvokeVirtual() ||
      instruction->IsLoadString() ||
      instruction->IsNewArray() ||
      instruction->IsNewInstance() ||
      instruction->IsNullCheck() ||
      instruction->IsPackedSwitch() ||
      instruction->IsParameterValue() ||
      instruction->IsPhi() ||
      instruction->IsReturn() ||
      instruction->IsReturnVoid() ||
      instruction->IsSelect() ||
      (instruction->IsStaticFieldGet() && !instruction->AsStaticFieldGet()->IsVolatile()) ||
      (instruction->IsStaticFieldSet() && !instruction->AsStaticFieldSet()->IsVolatile()) ||
      instruction->IsSuspendCheck() ||
      instruction->IsTypeConversion();
}

bool HScheduler::IsSchedulable(const HBasicBlock* block) const {
  // We may be only interested in loop blocks.
  if (only_optimize_loop_blocks_ && !block->IsInLoop()) {
    return false;
  }
  if (block->GetTryCatchInformation() != nullptr) {
    // Do not schedule blocks that are part of try-catch.
    // Because scheduler cannot see if catch block has assumptions on the instruction order in
    // the try block. In following example, if we enable scheduler for the try block,
    // MulitiplyAccumulate may be scheduled before DivZeroCheck,
    // which can result in an incorrect value in the catch block.
    //   try {
    //     a = a/b;    // DivZeroCheck
    //                 // Div
    //     c = c*d+e;  // MulitiplyAccumulate
    //   } catch {System.out.print(c); }
    return false;
  }
  // Check whether all instructions in this block are schedulable.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (!IsSchedulable(it.Current())) {
      return false;
    }
  }
  return true;
}

bool HScheduler::IsSchedulingBarrier(const HInstruction* instr) const {
  return instr->IsControlFlow() ||
      // Don't break calling convention.
      instr->IsParameterValue() ||
      // Code generation of goto relies on SuspendCheck's position.
      instr->IsSuspendCheck();
}

void HInstructionScheduling::Run(bool only_optimize_loop_blocks,
                                 bool schedule_randomly) {
#if defined(ART_ENABLE_CODEGEN_arm64) || defined(ART_ENABLE_CODEGEN_arm)
  // Phase-local allocator that allocates scheduler internal data structures like
  // scheduling nodes, internel nodes map, dependencies, etc.
  ScopedArenaAllocator allocator(graph_->GetArenaStack());
  CriticalPathSchedulingNodeSelector critical_path_selector;
  RandomSchedulingNodeSelector random_selector;
  SchedulingNodeSelector* selector = schedule_randomly
      ? static_cast<SchedulingNodeSelector*>(&random_selector)
      : static_cast<SchedulingNodeSelector*>(&critical_path_selector);
#else
  // Avoid compilation error when compiling for unsupported instruction set.
  UNUSED(only_optimize_loop_blocks);
  UNUSED(schedule_randomly);
  UNUSED(codegen_);
#endif

  switch (instruction_set_) {
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64: {
      arm64::HSchedulerARM64 scheduler(&allocator, selector);
      scheduler.SetOnlyOptimizeLoopBlocks(only_optimize_loop_blocks);
      scheduler.Schedule(graph_);
      break;
    }
#endif
#if defined(ART_ENABLE_CODEGEN_arm)
    case InstructionSet::kThumb2:
    case InstructionSet::kArm: {
      arm::SchedulingLatencyVisitorARM arm_latency_visitor(codegen_);
      arm::HSchedulerARM scheduler(&allocator, selector, &arm_latency_visitor);
      scheduler.SetOnlyOptimizeLoopBlocks(only_optimize_loop_blocks);
      scheduler.Schedule(graph_);
      break;
    }
#endif
    default:
      break;
  }
}

}  // namespace art
