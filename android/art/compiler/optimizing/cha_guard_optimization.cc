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

#include "cha_guard_optimization.h"

namespace art {

// Note we can only do CHA guard elimination/motion in a single pass, since
// if a guard is not removed, another guard might be removed due to
// the existence of the first guard. The first guard should not be further
// removed in another pass. For example, due to further optimizations,
// a receiver of a guard might turn out to be a parameter value, or defined at
// a different site, which makes the guard removable as a result. However
// it's not safe to remove the guard in another pass since another guard might
// have been removed due to the existence of this guard.
//
// As a consequence, we decided not to rely on other passes to remove them
// (such as GVN or instruction simplifier).

class CHAGuardVisitor : HGraphVisitor {
 public:
  explicit CHAGuardVisitor(HGraph* graph)
      : HGraphVisitor(graph),
        block_has_cha_guard_(GetGraph()->GetBlocks().size(),
                             0,
                             graph->GetAllocator()->Adapter(kArenaAllocCHA)),
        instruction_iterator_(nullptr) {
    number_of_guards_to_visit_ = GetGraph()->GetNumberOfCHAGuards();
    DCHECK_NE(number_of_guards_to_visit_, 0u);
    // Will recount number of guards during guard optimization.
    GetGraph()->SetNumberOfCHAGuards(0);
  }

  void VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) OVERRIDE;

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;

 private:
  void RemoveGuard(HShouldDeoptimizeFlag* flag);
  // Return true if `flag` is removed.
  bool OptimizeForParameter(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is removed.
  bool OptimizeWithDominatingGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);
  // Return true if `flag` is hoisted.
  bool HoistGuard(HShouldDeoptimizeFlag* flag, HInstruction* receiver);

  // Record if each block has any CHA guard. It's updated during the
  // reverse post order visit. Use int instead of bool since ArenaVector
  // does not support bool.
  ArenaVector<int> block_has_cha_guard_;

  // The iterator that's being used for this visitor. Need it to manually
  // advance the iterator due to removing/moving more than one instruction.
  HInstructionIterator* instruction_iterator_;

  // Used to short-circuit the pass when there is no more guards left to visit.
  uint32_t number_of_guards_to_visit_;

  DISALLOW_COPY_AND_ASSIGN(CHAGuardVisitor);
};

void CHAGuardVisitor::VisitBasicBlock(HBasicBlock* block) {
  if (number_of_guards_to_visit_ == 0) {
    return;
  }
  // Skip phis, just iterate through instructions.
  HInstructionIterator it(block->GetInstructions());
  instruction_iterator_ = &it;
  for (; !it.Done(); it.Advance()) {
    DCHECK(it.Current()->IsInBlock());
    it.Current()->Accept(this);
  }
}

void CHAGuardVisitor::RemoveGuard(HShouldDeoptimizeFlag* flag) {
  HBasicBlock* block = flag->GetBlock();
  HInstruction* compare = flag->GetNext();
  DCHECK(compare->IsNotEqual());
  HInstruction* deopt = compare->GetNext();
  DCHECK(deopt->IsDeoptimize());

  // Advance instruction iterator first before we remove the guard.
  // We need to do it twice since we remove three instructions and the
  // visitor is responsible for advancing it once.
  instruction_iterator_->Advance();
  instruction_iterator_->Advance();
  block->RemoveInstruction(deopt);
  block->RemoveInstruction(compare);
  block->RemoveInstruction(flag);
}

bool CHAGuardVisitor::OptimizeForParameter(HShouldDeoptimizeFlag* flag,
                                           HInstruction* receiver) {
  // If some compiled code is invalidated by CHA due to class loading, the
  // compiled code will not be entered anymore. So the very fact that the
  // compiled code is invoked guarantees that a parameter receiver conforms
  // to all the CHA devirtualization assumptions made by the compiled code,
  // since all parameter receivers pre-exist any (potential) invalidation of
  // the compiled code.
  //
  // TODO: allow more cases such as a phi whose inputs are all parameters.
  if (receiver->IsParameterValue()) {
    RemoveGuard(flag);
    return true;
  }
  return false;
}

bool CHAGuardVisitor::OptimizeWithDominatingGuard(HShouldDeoptimizeFlag* flag,
                                                  HInstruction* receiver) {
  // If there is another guard that dominates the current guard, and
  // that guard is dominated by receiver's definition, then the current
  // guard can be eliminated, since receiver must pre-exist that other
  // guard, and passing that guard guarantees that receiver conforms to
  // all the CHA devirtualization assumptions.
  HBasicBlock* dominator = flag->GetBlock();
  HBasicBlock* receiver_def_block = receiver->GetBlock();

  // Complexity of the following algorithm:
  // We potentially need to traverse the full dominator chain to receiver_def_block,
  // plus a (partial) linear search within one block for each guard.
  // So the worst case for each guard is bounded by the size of the
  // biggest block plus the depth of the dominating tree.

  while (dominator != receiver_def_block) {
    if (block_has_cha_guard_[dominator->GetBlockId()] == 1) {
      RemoveGuard(flag);
      return true;
    }
    dominator = dominator->GetDominator();
  }

  // At this point dominator is the block where receiver is defined.
  // We do a linear search within dominator to see if there is a guard after
  // receiver's definition.
  HInstruction* instruction;
  if (dominator == flag->GetBlock()) {
    // Flag and receiver are defined in the same block. Search backward from
    // the current guard.
    instruction = flag->GetPrevious();
  } else {
    // Search backward from the last instruction of that dominator.
    instruction = dominator->GetLastInstruction();
  }
  while (instruction != receiver) {
    if (instruction == nullptr) {
      // receiver must be defined in this block, we didn't find it
      // in the instruction list, so it must be a Phi.
      DCHECK(receiver->IsPhi());
      break;
    }
    if (instruction->IsShouldDeoptimizeFlag()) {
      RemoveGuard(flag);
      return true;
    }
    instruction = instruction->GetPrevious();
  }
  return false;
}

bool CHAGuardVisitor::HoistGuard(HShouldDeoptimizeFlag* flag,
                                 HInstruction* receiver) {
  // If receiver is loop invariant, we can hoist the guard out of the
  // loop since passing a guard before entering the loop guarantees that
  // receiver conforms to all the CHA devirtualization assumptions.
  // We only hoist guards out of the inner loop since that offers most of the
  // benefit and it might help remove other guards in the inner loop.
  HBasicBlock* block = flag->GetBlock();
  HLoopInformation* loop_info = block->GetLoopInformation();
  if (loop_info != nullptr &&
      !loop_info->IsIrreducible() &&
      loop_info->IsDefinedOutOfTheLoop(receiver)) {
    HInstruction* compare = flag->GetNext();
    DCHECK(compare->IsNotEqual());
    HInstruction* deopt = compare->GetNext();
    DCHECK(deopt->IsDeoptimize());

    // Advance instruction iterator first before we move the guard.
    // We need to do it twice since we move three instructions and the
    // visitor is responsible for advancing it once.
    instruction_iterator_->Advance();
    instruction_iterator_->Advance();

    HBasicBlock* pre_header = loop_info->GetPreHeader();
    flag->MoveBefore(pre_header->GetLastInstruction());
    compare->MoveBefore(pre_header->GetLastInstruction());

    block->RemoveInstruction(deopt);
    HInstruction* suspend = loop_info->GetSuspendCheck();
    // Need a new deoptimize instruction that copies the environment
    // of the suspend instruction for the loop.
    HDeoptimize* deoptimize = new (GetGraph()->GetAllocator()) HDeoptimize(
        GetGraph()->GetAllocator(), compare, DeoptimizationKind::kCHA, suspend->GetDexPc());
    pre_header->InsertInstructionBefore(deoptimize, pre_header->GetLastInstruction());
    deoptimize->CopyEnvironmentFromWithLoopPhiAdjustment(
        suspend->GetEnvironment(), loop_info->GetHeader());
    block_has_cha_guard_[pre_header->GetBlockId()] = 1;
    GetGraph()->IncrementNumberOfCHAGuards();
    return true;
  }
  return false;
}

void CHAGuardVisitor::VisitShouldDeoptimizeFlag(HShouldDeoptimizeFlag* flag) {
  number_of_guards_to_visit_--;
  HInstruction* receiver = flag->InputAt(0);
  // Don't need the receiver anymore.
  flag->RemoveInputAt(0);
  if (receiver->IsNullCheck()) {
    receiver = receiver->InputAt(0);
  }

  if (OptimizeForParameter(flag, receiver)) {
    DCHECK(!flag->IsInBlock());
    return;
  }
  if (OptimizeWithDominatingGuard(flag, receiver)) {
    DCHECK(!flag->IsInBlock());
    return;
  }
  if (HoistGuard(flag, receiver)) {
    DCHECK(flag->IsInBlock());
    return;
  }

  // Need to keep the CHA guard in place.
  block_has_cha_guard_[flag->GetBlock()->GetBlockId()] = 1;
  GetGraph()->IncrementNumberOfCHAGuards();
}

void CHAGuardOptimization::Run() {
  if (graph_->GetNumberOfCHAGuards() == 0) {
    return;
  }
  CHAGuardVisitor visitor(graph_);
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    visitor.VisitBasicBlock(block);
  }
}

}  // namespace art
