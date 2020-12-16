/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "constructor_fence_redundancy_elimination.h"

#include "base/arena_allocator.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"

namespace art {

static constexpr bool kCfreLogFenceInputCount = false;

// TODO: refactor this code by reusing escape analysis.
class CFREVisitor : public HGraphVisitor {
 public:
  CFREVisitor(HGraph* graph, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        scoped_allocator_(graph->GetArenaStack()),
        candidate_fences_(scoped_allocator_.Adapter(kArenaAllocCFRE)),
        candidate_fence_targets_(scoped_allocator_.Adapter(kArenaAllocCFRE)),
        stats_(stats) {}

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // Visit all instructions in block.
    HGraphVisitor::VisitBasicBlock(block);

    // If there were any unmerged fences left, merge them together,
    // the objects are considered 'published' at the end of the block.
    MergeCandidateFences();
  }

  void VisitConstructorFence(HConstructorFence* constructor_fence) OVERRIDE {
    candidate_fences_.push_back(constructor_fence);

    for (size_t input_idx = 0; input_idx < constructor_fence->InputCount(); ++input_idx) {
      candidate_fence_targets_.Insert(constructor_fence->InputAt(input_idx));
    }
  }

  void VisitBoundType(HBoundType* bound_type) OVERRIDE {
    VisitAlias(bound_type);
  }

  void VisitNullCheck(HNullCheck* null_check) OVERRIDE {
    VisitAlias(null_check);
  }

  void VisitSelect(HSelect* select) OVERRIDE {
    VisitAlias(select);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, value);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction, value);
  }

  void VisitArraySet(HArraySet* instruction) OVERRIDE {
    HInstruction* value = instruction->InputAt(2);
    VisitSetLocation(instruction, value);
  }

  void VisitDeoptimize(HDeoptimize* instruction ATTRIBUTE_UNUSED) {
    // Pessimize: Merge all fences.
    MergeCandidateFences();
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeVirtual(HInvokeVirtual* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeInterface(HInvokeInterface* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeUnresolved(HInvokeUnresolved* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokePolymorphic(HInvokePolymorphic* invoke) OVERRIDE {
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

 private:
  void HandleInvoke(HInstruction* invoke) {
    // An object is considered "published" if it escapes into an invoke as any of the parameters.
    if (HasInterestingPublishTargetAsInput(invoke)) {
        MergeCandidateFences();
    }
  }

  // Called by any instruction visitor that may create an alias.
  // These instructions may create an alias:
  // - BoundType
  // - NullCheck
  // - Select
  //
  // These also create an alias, but are not handled by this function:
  // - Phi: propagates values across blocks, but we always merge at the end of a block.
  // - Invoke: this is handled by HandleInvoke.
  void VisitAlias(HInstruction* aliasing_inst) {
    // An object is considered "published" if it becomes aliased by other instructions.
    if (HasInterestingPublishTargetAsInput(aliasing_inst))  {
      // Note that constructing a "NullCheck" for new-instance, new-array,
      // or a 'this' (receiver) reference is impossible.
      //
      // If by some reason we actually encounter such a NullCheck(FenceTarget),
      // we LOG(WARNING).
      if (UNLIKELY(aliasing_inst->IsNullCheck())) {
        LOG(kIsDebugBuild ? FATAL : WARNING)
            << "Unexpected instruction: NullCheck; should not be legal in graph";
        // We then do a best-effort to handle this case.
      }
      MergeCandidateFences();
    }
  }

  void VisitSetLocation(HInstruction* inst ATTRIBUTE_UNUSED, HInstruction* store_input) {
    // An object is considered "published" if it's stored onto the heap.
    // Sidenote: A later "LSE" pass can still remove the fence if it proves the
    // object doesn't actually escape.
    if (IsInterestingPublishTarget(store_input)) {
      // Merge all constructor fences that we've seen since
      // the last interesting store (or since the beginning).
      MergeCandidateFences();
    }
  }

  bool HasInterestingPublishTargetAsInput(HInstruction* inst) {
    for (size_t input_count = 0; input_count < inst->InputCount(); ++input_count) {
      if (IsInterestingPublishTarget(inst->InputAt(input_count))) {
        return true;
      }
    }

    return false;
  }

  // Merges all the existing fences we've seen so far into the last-most fence.
  //
  // This resets the list of candidate fences and their targets back to {}.
  void MergeCandidateFences() {
    if (candidate_fences_.empty()) {
      // Nothing to do, need 1+ fences to merge.
      return;
    }

    // The merge target is always the "last" candidate fence.
    HConstructorFence* merge_target = candidate_fences_[candidate_fences_.size() - 1];

    for (HConstructorFence* fence : candidate_fences_) {
      MaybeMerge(merge_target, fence);
    }

    if (kCfreLogFenceInputCount) {
      LOG(INFO) << "CFRE-MergeCandidateFences: Post-merge fence input count "
                << merge_target->InputCount();
    }

    // Each merge acts as a cut-off point. The optimization is reset completely.
    // In theory, we could push the fence as far as its publish, but in practice
    // there is no benefit to this extra complexity unless we also reordered
    // the stores to come later.
    candidate_fences_.clear();
    candidate_fence_targets_.Clear();
  }

  // A publishing 'store' is only interesting if the value being stored
  // is one of the fence `targets` in `candidate_fences`.
  bool IsInterestingPublishTarget(HInstruction* store_input) const {
    return candidate_fence_targets_.Find(store_input) != candidate_fence_targets_.end();
  }

  void MaybeMerge(HConstructorFence* target, HConstructorFence* src) {
    if (target == src) {
      return;  // Don't merge a fence into itself.
      // This is mostly for stats-purposes, we don't want to count merge(x,x)
      // as removing a fence because it's a no-op.
    }

    target->Merge(src);

    MaybeRecordStat(stats_, MethodCompilationStat::kConstructorFenceRemovedCFRE);
  }

  // Phase-local heap memory allocator for CFRE optimizer.
  ScopedArenaAllocator scoped_allocator_;

  // Set of constructor fences that we've seen in the current block.
  // Each constructor fences acts as a guard for one or more `targets`.
  // There exist no stores to any `targets` between any of these fences.
  //
  // Fences are in succession order (e.g. fence[i] succeeds fence[i-1]
  // within the same basic block).
  ScopedArenaVector<HConstructorFence*> candidate_fences_;

  // Stores a set of the fence targets, to allow faster lookup of whether
  // a detected publish is a target of one of the candidate fences.
  ScopedArenaHashSet<HInstruction*> candidate_fence_targets_;

  // Used to record stats about the optimization.
  OptimizingCompilerStats* const stats_;

  DISALLOW_COPY_AND_ASSIGN(CFREVisitor);
};

void ConstructorFenceRedundancyElimination::Run() {
  CFREVisitor cfre_visitor(graph_, stats_);

  // Arbitrarily visit in reverse-post order.
  // The exact block visit order does not matter, as the algorithm
  // only operates on a single block at a time.
  cfre_visitor.VisitReversePostOrder();
}

}  // namespace art
