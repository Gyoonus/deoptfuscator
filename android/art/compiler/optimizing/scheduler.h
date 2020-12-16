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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_H_

#include <fstream>

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/time_utils.h"
#include "code_generator.h"
#include "driver/compiler_driver.h"
#include "load_store_analysis.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

// General description of instruction scheduling.
//
// This pass tries to improve the quality of the generated code by reordering
// instructions in the graph to avoid execution delays caused by execution
// dependencies.
// Currently, scheduling is performed at the block level, so no `HInstruction`
// ever leaves its block in this pass.
//
// The scheduling process iterates through blocks in the graph. For blocks that
// we can and want to schedule:
// 1) Build a dependency graph for instructions.
//    It includes data dependencies (inputs/uses), but also environment
//    dependencies and side-effect dependencies.
// 2) Schedule the dependency graph.
//    This is a topological sort of the dependency graph, using heuristics to
//    decide what node to scheduler first when there are multiple candidates.
//
// A few factors impacting the quality of the scheduling are:
// - The heuristics used to decide what node to schedule in the topological sort
//   when there are multiple valid candidates. There is a wide range of
//   complexity possible here, going from a simple model only considering
//   latencies, to a super detailed CPU pipeline model.
// - Fewer dependencies in the dependency graph give more freedom for the
//   scheduling heuristics. For example de-aliasing can allow possibilities for
//   reordering of memory accesses.
// - The level of abstraction of the IR. It is easier to evaluate scheduling for
//   IRs that translate to a single assembly instruction than for IRs
//   that generate multiple assembly instructions or generate different code
//   depending on properties of the IR.
// - Scheduling is performed before register allocation, it is not aware of the
//   impact of moving instructions on register allocation.
//
//
// The scheduling code uses the terms predecessors, successors, and dependencies.
// This can be confusing at times, so here are clarifications.
// These terms are used from the point of view of the program dependency graph. So
// the inputs of an instruction are part of its dependencies, and hence part its
// predecessors. So the uses of an instruction are (part of) its successors.
// (Side-effect dependencies can yield predecessors or successors that are not
// inputs or uses.)
//
// Here is a trivial example. For the Java code:
//
//    int a = 1 + 2;
//
// we would have the instructions
//
//    i1 HIntConstant 1
//    i2 HIntConstant 2
//    i3 HAdd [i1,i2]
//
// `i1` and `i2` are predecessors of `i3`.
// `i3` is a successor of `i1` and a successor of `i2`.
// In a scheduling graph for this code we would have three nodes `n1`, `n2`,
// and `n3` (respectively for instructions `i1`, `i1`, and `i3`).
// Conceptually the program dependency graph for this would contain two edges
//
//    n1 -> n3
//    n2 -> n3
//
// Since we schedule backwards (starting from the last instruction in each basic
// block), the implementation of nodes keeps a list of pointers their
// predecessors. So `n3` would keep pointers to its predecessors `n1` and `n2`.
//
// Node dependencies are also referred to from the program dependency graph
// point of view: we say that node `B` immediately depends on `A` if there is an
// edge from `A` to `B` in the program dependency graph. `A` is a predecessor of
// `B`, `B` is a successor of `A`. In the example above `n3` depends on `n1` and
// `n2`.
// Since nodes in the scheduling graph keep a list of their predecessors, node
// `B` will have a pointer to its predecessor `A`.
// As we schedule backwards, `B` will be selected for scheduling before `A` is.
//
// So the scheduling for the example above could happen as follow
//
//    |---------------------------+------------------------|
//    | candidates for scheduling | instructions scheduled |
//    | --------------------------+------------------------|
//
// The only node without successors is `n3`, so it is the only initial
// candidate.
//
//    | n3                        | (none)                 |
//
// We schedule `n3` as the last (and only) instruction. All its predecessors
// that do not have any unscheduled successors become candidate. That is, `n1`
// and `n2` become candidates.
//
//    | n1, n2                    | n3                     |
//
// One of the candidates is selected. In practice this is where scheduling
// heuristics kick in, to decide which of the candidates should be selected.
// In this example, let it be `n1`. It is scheduled before previously scheduled
// nodes (in program order). There are no other nodes to add to the list of
// candidates.
//
//    | n2                        | n1                     |
//    |                           | n3                     |
//
// The only candidate available for scheduling is `n2`. Schedule it before
// (in program order) the previously scheduled nodes.
//
//    | (none)                    | n2                     |
//    |                           | n1                     |
//    |                           | n3                     |
//    |---------------------------+------------------------|
//
// So finally the instructions will be executed in the order `i2`, `i1`, and `i3`.
// In this trivial example, it does not matter which of `i1` and `i2` is
// scheduled first since they are constants. However the same process would
// apply if `i1` and `i2` were actual operations (for example `HMul` and `HDiv`).

// Set to true to have instruction scheduling dump scheduling graphs to the file
// `scheduling_graphs.dot`. See `SchedulingGraph::DumpAsDotGraph()`.
static constexpr bool kDumpDotSchedulingGraphs = false;

// Typically used as a default instruction latency.
static constexpr uint32_t kGenericInstructionLatency = 1;

class HScheduler;

/**
 * A node representing an `HInstruction` in the `SchedulingGraph`.
 */
class SchedulingNode : public DeletableArenaObject<kArenaAllocScheduler> {
 public:
  SchedulingNode(HInstruction* instr, ScopedArenaAllocator* allocator, bool is_scheduling_barrier)
      : latency_(0),
        internal_latency_(0),
        critical_path_(0),
        instruction_(instr),
        is_scheduling_barrier_(is_scheduling_barrier),
        data_predecessors_(allocator->Adapter(kArenaAllocScheduler)),
        other_predecessors_(allocator->Adapter(kArenaAllocScheduler)),
        num_unscheduled_successors_(0) {
    data_predecessors_.reserve(kPreallocatedPredecessors);
  }

  void AddDataPredecessor(SchedulingNode* predecessor) {
    data_predecessors_.push_back(predecessor);
    predecessor->num_unscheduled_successors_++;
  }

  const ScopedArenaVector<SchedulingNode*>& GetDataPredecessors() const {
    return data_predecessors_;
  }

  void AddOtherPredecessor(SchedulingNode* predecessor) {
    other_predecessors_.push_back(predecessor);
    predecessor->num_unscheduled_successors_++;
  }

  const ScopedArenaVector<SchedulingNode*>& GetOtherPredecessors() const {
    return other_predecessors_;
  }

  void DecrementNumberOfUnscheduledSuccessors() {
    num_unscheduled_successors_--;
  }

  void MaybeUpdateCriticalPath(uint32_t other_critical_path) {
    critical_path_ = std::max(critical_path_, other_critical_path);
  }

  bool HasUnscheduledSuccessors() const {
    return num_unscheduled_successors_ != 0;
  }

  HInstruction* GetInstruction() const { return instruction_; }
  uint32_t GetLatency() const { return latency_; }
  void SetLatency(uint32_t latency) { latency_ = latency; }
  uint32_t GetInternalLatency() const { return internal_latency_; }
  void SetInternalLatency(uint32_t internal_latency) { internal_latency_ = internal_latency; }
  uint32_t GetCriticalPath() const { return critical_path_; }
  bool IsSchedulingBarrier() const { return is_scheduling_barrier_; }

 private:
  // The latency of this node. It represents the latency between the moment the
  // last instruction for this node has executed to the moment the result
  // produced by this node is available to users.
  uint32_t latency_;
  // This represents the time spent *within* the generated code for this node.
  // It should be zero for nodes that only generate a single instruction.
  uint32_t internal_latency_;

  // The critical path from this instruction to the end of scheduling. It is
  // used by the scheduling heuristics to measure the priority of this instruction.
  // It is defined as
  //     critical_path_ = latency_ + max((use.internal_latency_ + use.critical_path_) for all uses)
  // (Note that here 'uses' is equivalent to 'data successors'. Also see comments in
  // `HScheduler::Schedule(SchedulingNode* scheduling_node)`).
  uint32_t critical_path_;

  // The instruction that this node represents.
  HInstruction* const instruction_;

  // If a node is scheduling barrier, other nodes cannot be scheduled before it.
  const bool is_scheduling_barrier_;

  // The lists of predecessors. They cannot be scheduled before this node. Once
  // this node is scheduled, we check whether any of its predecessors has become a
  // valid candidate for scheduling.
  // Predecessors in `data_predecessors_` are data dependencies. Those in
  // `other_predecessors_` contain side-effect dependencies, environment
  // dependencies, and scheduling barrier dependencies.
  ScopedArenaVector<SchedulingNode*> data_predecessors_;
  ScopedArenaVector<SchedulingNode*> other_predecessors_;

  // The number of unscheduled successors for this node. This number is
  // decremented as successors are scheduled. When it reaches zero this node
  // becomes a valid candidate to schedule.
  uint32_t num_unscheduled_successors_;

  static constexpr size_t kPreallocatedPredecessors = 4;
};

/*
 * Directed acyclic graph for scheduling.
 */
class SchedulingGraph : public ValueObject {
 public:
  SchedulingGraph(const HScheduler* scheduler, ScopedArenaAllocator* allocator)
      : scheduler_(scheduler),
        allocator_(allocator),
        contains_scheduling_barrier_(false),
        nodes_map_(allocator_->Adapter(kArenaAllocScheduler)),
        heap_location_collector_(nullptr) {}

  SchedulingNode* AddNode(HInstruction* instr, bool is_scheduling_barrier = false) {
    std::unique_ptr<SchedulingNode> node(
        new (allocator_) SchedulingNode(instr, allocator_, is_scheduling_barrier));
    SchedulingNode* result = node.get();
    nodes_map_.Insert(std::make_pair(instr, std::move(node)));
    contains_scheduling_barrier_ |= is_scheduling_barrier;
    AddDependencies(instr, is_scheduling_barrier);
    return result;
  }

  void Clear() {
    nodes_map_.Clear();
    contains_scheduling_barrier_ = false;
  }

  void SetHeapLocationCollector(const HeapLocationCollector& heap_location_collector) {
    heap_location_collector_ = &heap_location_collector;
  }

  SchedulingNode* GetNode(const HInstruction* instr) const {
    auto it = nodes_map_.Find(instr);
    if (it == nodes_map_.end()) {
      return nullptr;
    } else {
      return it->second.get();
    }
  }

  bool IsSchedulingBarrier(const HInstruction* instruction) const;

  bool HasImmediateDataDependency(const SchedulingNode* node, const SchedulingNode* other) const;
  bool HasImmediateDataDependency(const HInstruction* node, const HInstruction* other) const;
  bool HasImmediateOtherDependency(const SchedulingNode* node, const SchedulingNode* other) const;
  bool HasImmediateOtherDependency(const HInstruction* node, const HInstruction* other) const;

  size_t Size() const {
    return nodes_map_.Size();
  }

  // Dump the scheduling graph, in dot file format, appending it to the file
  // `scheduling_graphs.dot`.
  void DumpAsDotGraph(const std::string& description,
                      const ScopedArenaVector<SchedulingNode*>& initial_candidates);

 protected:
  void AddDependency(SchedulingNode* node, SchedulingNode* dependency, bool is_data_dependency);
  void AddDataDependency(SchedulingNode* node, SchedulingNode* dependency) {
    AddDependency(node, dependency, /*is_data_dependency*/true);
  }
  void AddOtherDependency(SchedulingNode* node, SchedulingNode* dependency) {
    AddDependency(node, dependency, /*is_data_dependency*/false);
  }
  bool HasMemoryDependency(const HInstruction* node, const HInstruction* other) const;
  bool HasExceptionDependency(const HInstruction* node, const HInstruction* other) const;
  bool HasSideEffectDependency(const HInstruction* node, const HInstruction* other) const;
  bool ArrayAccessMayAlias(const HInstruction* node, const HInstruction* other) const;
  bool FieldAccessMayAlias(const HInstruction* node, const HInstruction* other) const;
  size_t ArrayAccessHeapLocation(HInstruction* array, HInstruction* index) const;
  size_t FieldAccessHeapLocation(HInstruction* obj, const FieldInfo* field) const;

  // Add dependencies nodes for the given `HInstruction`: inputs, environments, and side-effects.
  void AddDependencies(HInstruction* instruction, bool is_scheduling_barrier = false);

  const HScheduler* const scheduler_;

  ScopedArenaAllocator* const allocator_;

  bool contains_scheduling_barrier_;

  ScopedArenaHashMap<const HInstruction*, std::unique_ptr<SchedulingNode>> nodes_map_;

  const HeapLocationCollector* heap_location_collector_;
};

/*
 * The visitors derived from this base class are used by schedulers to evaluate
 * the latencies of `HInstruction`s.
 */
class SchedulingLatencyVisitor : public HGraphDelegateVisitor {
 public:
  // This class and its sub-classes will never be used to drive a visit of an
  // `HGraph` but only to visit `HInstructions` one at a time, so we do not need
  // to pass a valid graph to `HGraphDelegateVisitor()`.
  SchedulingLatencyVisitor()
      : HGraphDelegateVisitor(nullptr),
        last_visited_latency_(0),
        last_visited_internal_latency_(0) {}

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    LOG(FATAL) << "Error visiting " << instruction->DebugName() << ". "
        "Architecture-specific scheduling latency visitors must handle all instructions"
        " (potentially by overriding the generic `VisitInstruction()`.";
    UNREACHABLE();
  }

  void Visit(HInstruction* instruction) {
    instruction->Accept(this);
  }

  void CalculateLatency(SchedulingNode* node) {
    // By default nodes have no internal latency.
    last_visited_internal_latency_ = 0;
    Visit(node->GetInstruction());
  }

  uint32_t GetLastVisitedLatency() const { return last_visited_latency_; }
  uint32_t GetLastVisitedInternalLatency() const { return last_visited_internal_latency_; }

 protected:
  // The latency of the most recent visited SchedulingNode.
  // This is for reporting the latency value to the user of this visitor.
  uint32_t last_visited_latency_;
  // This represents the time spent *within* the generated code for the most recent visited
  // SchedulingNode. This is for reporting the internal latency value to the user of this visitor.
  uint32_t last_visited_internal_latency_;
};

class SchedulingNodeSelector : public ArenaObject<kArenaAllocScheduler> {
 public:
  virtual SchedulingNode* PopHighestPriorityNode(ScopedArenaVector<SchedulingNode*>* nodes,
                                                 const SchedulingGraph& graph) = 0;
  virtual ~SchedulingNodeSelector() {}
 protected:
  static void DeleteNodeAtIndex(ScopedArenaVector<SchedulingNode*>* nodes, size_t index) {
    (*nodes)[index] = nodes->back();
    nodes->pop_back();
  }
};

/*
 * Select a `SchedulingNode` at random within the candidates.
 */
class RandomSchedulingNodeSelector : public SchedulingNodeSelector {
 public:
  RandomSchedulingNodeSelector() : seed_(0) {
    seed_  = static_cast<uint32_t>(NanoTime());
    srand(seed_);
  }

  SchedulingNode* PopHighestPriorityNode(ScopedArenaVector<SchedulingNode*>* nodes,
                                         const SchedulingGraph& graph) OVERRIDE {
    UNUSED(graph);
    DCHECK(!nodes->empty());
    size_t select = rand_r(&seed_) % nodes->size();
    SchedulingNode* select_node = (*nodes)[select];
    DeleteNodeAtIndex(nodes, select);
    return select_node;
  }

  uint32_t seed_;
};

/*
 * Select a `SchedulingNode` according to critical path information,
 * with heuristics to favor certain instruction patterns like materialized condition.
 */
class CriticalPathSchedulingNodeSelector : public SchedulingNodeSelector {
 public:
  CriticalPathSchedulingNodeSelector() : prev_select_(nullptr) {}

  SchedulingNode* PopHighestPriorityNode(ScopedArenaVector<SchedulingNode*>* nodes,
                                         const SchedulingGraph& graph) OVERRIDE;

 protected:
  SchedulingNode* GetHigherPrioritySchedulingNode(SchedulingNode* candidate,
                                                  SchedulingNode* check) const;

  SchedulingNode* SelectMaterializedCondition(ScopedArenaVector<SchedulingNode*>* nodes,
                                              const SchedulingGraph& graph) const;

 private:
  const SchedulingNode* prev_select_;
};

class HScheduler {
 public:
  HScheduler(ScopedArenaAllocator* allocator,
             SchedulingLatencyVisitor* latency_visitor,
             SchedulingNodeSelector* selector)
      : allocator_(allocator),
        latency_visitor_(latency_visitor),
        selector_(selector),
        only_optimize_loop_blocks_(true),
        scheduling_graph_(this, allocator),
        cursor_(nullptr),
        candidates_(allocator_->Adapter(kArenaAllocScheduler)) {}
  virtual ~HScheduler() {}

  void Schedule(HGraph* graph);

  void SetOnlyOptimizeLoopBlocks(bool loop_only) { only_optimize_loop_blocks_ = loop_only; }

  // Instructions can not be rescheduled across a scheduling barrier.
  virtual bool IsSchedulingBarrier(const HInstruction* instruction) const;

 protected:
  void Schedule(HBasicBlock* block);
  void Schedule(SchedulingNode* scheduling_node);
  void Schedule(HInstruction* instruction);

  // Any instruction returning `false` via this method will prevent its
  // containing basic block from being scheduled.
  // This method is used to restrict scheduling to instructions that we know are
  // safe to handle.
  //
  // For newly introduced instructions by default HScheduler::IsSchedulable returns false.
  // HScheduler${ARCH}::IsSchedulable can be overridden to return true for an instruction (see
  // scheduler_arm64.h for example) if it is safe to schedule it; in this case one *must* also
  // look at/update HScheduler${ARCH}::IsSchedulingBarrier for this instruction.
  virtual bool IsSchedulable(const HInstruction* instruction) const;
  bool IsSchedulable(const HBasicBlock* block) const;

  void CalculateLatency(SchedulingNode* node) {
    latency_visitor_->CalculateLatency(node);
    node->SetLatency(latency_visitor_->GetLastVisitedLatency());
    node->SetInternalLatency(latency_visitor_->GetLastVisitedInternalLatency());
  }

  ScopedArenaAllocator* const allocator_;
  SchedulingLatencyVisitor* const latency_visitor_;
  SchedulingNodeSelector* const selector_;
  bool only_optimize_loop_blocks_;

  // We instantiate the members below as part of this class to avoid
  // instantiating them locally for every chunk scheduled.
  SchedulingGraph scheduling_graph_;
  // A pointer indicating where the next instruction to be scheduled will be inserted.
  HInstruction* cursor_;
  // The list of candidates for scheduling. A node becomes a candidate when all
  // its predecessors have been scheduled.
  ScopedArenaVector<SchedulingNode*> candidates_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HScheduler);
};

inline bool SchedulingGraph::IsSchedulingBarrier(const HInstruction* instruction) const {
  return scheduler_->IsSchedulingBarrier(instruction);
}

class HInstructionScheduling : public HOptimization {
 public:
  HInstructionScheduling(HGraph* graph,
                         InstructionSet instruction_set,
                         CodeGenerator* cg = nullptr,
                         const char* name = kInstructionSchedulingPassName)
      : HOptimization(graph, name),
        codegen_(cg),
        instruction_set_(instruction_set) {}

  void Run() {
    Run(/*only_optimize_loop_blocks*/ true, /*schedule_randomly*/ false);
  }
  void Run(bool only_optimize_loop_blocks, bool schedule_randomly);

  static constexpr const char* kInstructionSchedulingPassName = "scheduler";

 private:
  CodeGenerator* const codegen_;
  const InstructionSet instruction_set_;
  DISALLOW_COPY_AND_ASSIGN(HInstructionScheduling);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_H_
