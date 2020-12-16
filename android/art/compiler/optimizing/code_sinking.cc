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

#include "code_sinking.h"

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "common_dominator.h"
#include "nodes.h"

namespace art {

void CodeSinking::Run() {
  HBasicBlock* exit = graph_->GetExitBlock();
  if (exit == nullptr) {
    // Infinite loop, just bail.
    return;
  }
  // TODO(ngeoffray): we do not profile branches yet, so use throw instructions
  // as an indicator of an uncommon branch.
  for (HBasicBlock* exit_predecessor : exit->GetPredecessors()) {
    HInstruction* last = exit_predecessor->GetLastInstruction();
    // Any predecessor of the exit that does not return, throws an exception.
    if (!last->IsReturn() && !last->IsReturnVoid()) {
      SinkCodeToUncommonBranch(exit_predecessor);
    }
  }
}

static bool IsInterestingInstruction(HInstruction* instruction) {
  // Instructions from the entry graph (for example constants) are never interesting to move.
  if (instruction->GetBlock() == instruction->GetBlock()->GetGraph()->GetEntryBlock()) {
    return false;
  }
  // We want to move moveable instructions that cannot throw, as well as
  // heap stores and allocations.

  // Volatile stores cannot be moved.
  if (instruction->IsInstanceFieldSet()) {
    if (instruction->AsInstanceFieldSet()->IsVolatile()) {
      return false;
    }
  }

  // Check allocations first, as they can throw, but it is safe to move them.
  if (instruction->IsNewInstance() || instruction->IsNewArray()) {
    return true;
  }

  // Check it is safe to move ConstructorFence.
  // (Safe to move ConstructorFence for only protecting the new-instance but not for finals.)
  if (instruction->IsConstructorFence()) {
    HConstructorFence* ctor_fence = instruction->AsConstructorFence();

    // A fence with "0" inputs is dead and should've been removed in a prior pass.
    DCHECK_NE(0u, ctor_fence->InputCount());

    // TODO: this should be simplified to 'return true' since it's
    // potentially pessimizing any code sinking for inlined constructors with final fields.
    // TODO: double check that if the final field assignments are not moved,
    // then the fence is not moved either.

    return ctor_fence->GetAssociatedAllocation() != nullptr;
  }

  // All other instructions that can throw cannot be moved.
  if (instruction->CanThrow()) {
    return false;
  }

  // We can only store on local allocations. Other heap references can
  // be escaping. Note that allocations can escape too, but we only move
  // allocations if their users can move to, or are in the list of
  // post dominated blocks.
  if (instruction->IsInstanceFieldSet()) {
    if (!instruction->InputAt(0)->IsNewInstance()) {
      return false;
    }
  }

  if (instruction->IsArraySet()) {
    if (!instruction->InputAt(0)->IsNewArray()) {
      return false;
    }
  }

  // Heap accesses cannot go pass instructions that have memory side effects, which
  // we are not tracking here. Note that the load/store elimination optimization
  // runs before this optimization, and should have removed interesting ones.
  // In theory, we could handle loads of local allocations, but this is currently
  // hard to test, as LSE removes them.
  if (instruction->IsStaticFieldGet() ||
      instruction->IsInstanceFieldGet() ||
      instruction->IsArrayGet()) {
    return false;
  }

  if (instruction->IsInstanceFieldSet() ||
      instruction->IsArraySet() ||
      instruction->CanBeMoved()) {
    return true;
  }
  return false;
}

static void AddInstruction(HInstruction* instruction,
                           const ArenaBitVector& processed_instructions,
                           const ArenaBitVector& discard_blocks,
                           ScopedArenaVector<HInstruction*>* worklist) {
  // Add to the work list if the instruction is not in the list of blocks
  // to discard, hasn't been already processed and is of interest.
  if (!discard_blocks.IsBitSet(instruction->GetBlock()->GetBlockId()) &&
      !processed_instructions.IsBitSet(instruction->GetId()) &&
      IsInterestingInstruction(instruction)) {
    worklist->push_back(instruction);
  }
}

static void AddInputs(HInstruction* instruction,
                      const ArenaBitVector& processed_instructions,
                      const ArenaBitVector& discard_blocks,
                      ScopedArenaVector<HInstruction*>* worklist) {
  for (HInstruction* input : instruction->GetInputs()) {
    AddInstruction(input, processed_instructions, discard_blocks, worklist);
  }
}

static void AddInputs(HBasicBlock* block,
                      const ArenaBitVector& processed_instructions,
                      const ArenaBitVector& discard_blocks,
                      ScopedArenaVector<HInstruction*>* worklist) {
  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    AddInputs(it.Current(), processed_instructions, discard_blocks, worklist);
  }
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    AddInputs(it.Current(), processed_instructions, discard_blocks, worklist);
  }
}

static bool ShouldFilterUse(HInstruction* instruction,
                            HInstruction* user,
                            const ArenaBitVector& post_dominated) {
  if (instruction->IsNewInstance()) {
    return (user->IsInstanceFieldSet() || user->IsConstructorFence()) &&
        (user->InputAt(0) == instruction) &&
        !post_dominated.IsBitSet(user->GetBlock()->GetBlockId());
  } else if (instruction->IsNewArray()) {
    return (user->IsArraySet() || user->IsConstructorFence()) &&
        (user->InputAt(0) == instruction) &&
        !post_dominated.IsBitSet(user->GetBlock()->GetBlockId());
  }
  return false;
}


// Find the ideal position for moving `instruction`. If `filter` is true,
// we filter out store instructions to that instruction, which are processed
// first in the step (3) of the sinking algorithm.
// This method is tailored to the sinking algorithm, unlike
// the generic HInstruction::MoveBeforeFirstUserAndOutOfLoops.
static HInstruction* FindIdealPosition(HInstruction* instruction,
                                       const ArenaBitVector& post_dominated,
                                       bool filter = false) {
  DCHECK(!instruction->IsPhi());  // Makes no sense for Phi.

  // Find the target block.
  CommonDominator finder(/* start_block */ nullptr);
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    HInstruction* user = use.GetUser();
    if (!(filter && ShouldFilterUse(instruction, user, post_dominated))) {
      HBasicBlock* block = user->GetBlock();
      if (user->IsPhi()) {
        // Special case phis by taking the incoming block for regular ones,
        // or the dominator for catch phis.
        block = user->AsPhi()->IsCatchPhi()
            ? block->GetDominator()
            : block->GetPredecessors()[use.GetIndex()];
      }
      finder.Update(block);
    }
  }
  for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
    DCHECK(!use.GetUser()->GetHolder()->IsPhi());
    DCHECK(!filter || !ShouldFilterUse(instruction, use.GetUser()->GetHolder(), post_dominated));
    finder.Update(use.GetUser()->GetHolder()->GetBlock());
  }
  HBasicBlock* target_block = finder.Get();
  if (target_block == nullptr) {
    // No user we can go next to? Likely a LSE or DCE limitation.
    return nullptr;
  }

  // Move to the first dominator not in a loop, if we can.
  while (target_block->IsInLoop()) {
    if (!post_dominated.IsBitSet(target_block->GetDominator()->GetBlockId())) {
      break;
    }
    target_block = target_block->GetDominator();
    DCHECK(target_block != nullptr);
  }

  // Bail if the instruction can throw and we are about to move into a catch block.
  if (instruction->CanThrow() && target_block->GetTryCatchInformation() != nullptr) {
    return nullptr;
  }

  // Find insertion position. No need to filter anymore, as we have found a
  // target block.
  HInstruction* insert_pos = nullptr;
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    if (use.GetUser()->GetBlock() == target_block &&
        (insert_pos == nullptr || use.GetUser()->StrictlyDominates(insert_pos))) {
      insert_pos = use.GetUser();
    }
  }
  for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
    HInstruction* user = use.GetUser()->GetHolder();
    if (user->GetBlock() == target_block &&
        (insert_pos == nullptr || user->StrictlyDominates(insert_pos))) {
      insert_pos = user;
    }
  }
  if (insert_pos == nullptr) {
    // No user in `target_block`, insert before the control flow instruction.
    insert_pos = target_block->GetLastInstruction();
    DCHECK(insert_pos->IsControlFlow());
    // Avoid splitting HCondition from HIf to prevent unnecessary materialization.
    if (insert_pos->IsIf()) {
      HInstruction* if_input = insert_pos->AsIf()->InputAt(0);
      if (if_input == insert_pos->GetPrevious()) {
        insert_pos = if_input;
      }
    }
  }
  DCHECK(!insert_pos->IsPhi());
  return insert_pos;
}


void CodeSinking::SinkCodeToUncommonBranch(HBasicBlock* end_block) {
  // Local allocator to discard data structures created below at the end of this optimization.
  ScopedArenaAllocator allocator(graph_->GetArenaStack());

  size_t number_of_instructions = graph_->GetCurrentInstructionId();
  ScopedArenaVector<HInstruction*> worklist(allocator.Adapter(kArenaAllocMisc));
  ArenaBitVector processed_instructions(&allocator, number_of_instructions, /* expandable */ false);
  processed_instructions.ClearAllBits();
  ArenaBitVector post_dominated(&allocator, graph_->GetBlocks().size(), /* expandable */ false);
  post_dominated.ClearAllBits();
  ArenaBitVector instructions_that_can_move(
      &allocator, number_of_instructions, /* expandable */ false);
  instructions_that_can_move.ClearAllBits();
  ScopedArenaVector<HInstruction*> move_in_order(allocator.Adapter(kArenaAllocMisc));

  // Step (1): Visit post order to get a subset of blocks post dominated by `end_block`.
  // TODO(ngeoffray): Getting the full set of post-dominated shoud be done by
  // computint the post dominator tree, but that could be too time consuming. Also,
  // we should start the analysis from blocks dominated by an uncommon branch, but we
  // don't profile branches yet.
  bool found_block = false;
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (block == end_block) {
      found_block = true;
      post_dominated.SetBit(block->GetBlockId());
    } else if (found_block) {
      bool is_post_dominated = true;
      if (block->GetSuccessors().empty()) {
        // We currently bail for loops.
        is_post_dominated = false;
      } else {
        for (HBasicBlock* successor : block->GetSuccessors()) {
          if (!post_dominated.IsBitSet(successor->GetBlockId())) {
            is_post_dominated = false;
            break;
          }
        }
      }
      if (is_post_dominated) {
        post_dominated.SetBit(block->GetBlockId());
      }
    }
  }

  // Now that we have found a subset of post-dominated blocks, add to the worklist all inputs
  // of instructions in these blocks that are not themselves in these blocks.
  // Also find the common dominator of the found post dominated blocks, to help filtering
  // out un-movable uses in step (2).
  CommonDominator finder(end_block);
  for (size_t i = 0, e = graph_->GetBlocks().size(); i < e; ++i) {
    if (post_dominated.IsBitSet(i)) {
      finder.Update(graph_->GetBlocks()[i]);
      AddInputs(graph_->GetBlocks()[i], processed_instructions, post_dominated, &worklist);
    }
  }
  HBasicBlock* common_dominator = finder.Get();

  // Step (2): iterate over the worklist to find sinking candidates.
  while (!worklist.empty()) {
    HInstruction* instruction = worklist.back();
    if (processed_instructions.IsBitSet(instruction->GetId())) {
      // The instruction has already been processed, continue. This happens
      // when the instruction is the input/user of multiple instructions.
      worklist.pop_back();
      continue;
    }
    bool all_users_in_post_dominated_blocks = true;
    bool can_move = true;
    // Check users of the instruction.
    for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
      HInstruction* user = use.GetUser();
      if (!post_dominated.IsBitSet(user->GetBlock()->GetBlockId()) &&
          !instructions_that_can_move.IsBitSet(user->GetId())) {
        all_users_in_post_dominated_blocks = false;
        // If we've already processed this user, or the user cannot be moved, or
        // is not dominating the post dominated blocks, bail.
        // TODO(ngeoffray): The domination check is an approximation. We should
        // instead check if the dominated blocks post dominate the user's block,
        // but we do not have post dominance information here.
        if (processed_instructions.IsBitSet(user->GetId()) ||
            !IsInterestingInstruction(user) ||
            !user->GetBlock()->Dominates(common_dominator)) {
          can_move = false;
          break;
        }
      }
    }

    // Check environment users of the instruction. Some of these users require
    // the instruction not to move.
    if (all_users_in_post_dominated_blocks) {
      for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
        HEnvironment* environment = use.GetUser();
        HInstruction* user = environment->GetHolder();
        if (!post_dominated.IsBitSet(user->GetBlock()->GetBlockId())) {
          if (graph_->IsDebuggable() ||
              user->IsDeoptimize() ||
              user->CanThrowIntoCatchBlock() ||
              (user->IsSuspendCheck() && graph_->IsCompilingOsr())) {
            can_move = false;
            break;
          }
        }
      }
    }
    if (!can_move) {
      // Instruction cannot be moved, mark it as processed and remove it from the work
      // list.
      processed_instructions.SetBit(instruction->GetId());
      worklist.pop_back();
    } else if (all_users_in_post_dominated_blocks) {
      // Instruction is a candidate for being sunk. Mark it as such, remove it from the
      // work list, and add its inputs to the work list.
      instructions_that_can_move.SetBit(instruction->GetId());
      move_in_order.push_back(instruction);
      processed_instructions.SetBit(instruction->GetId());
      worklist.pop_back();
      AddInputs(instruction, processed_instructions, post_dominated, &worklist);
      // Drop the environment use not in the list of post-dominated block. This is
      // to help step (3) of this optimization, when we start moving instructions
      // closer to their use.
      for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
        HEnvironment* environment = use.GetUser();
        HInstruction* user = environment->GetHolder();
        if (!post_dominated.IsBitSet(user->GetBlock()->GetBlockId())) {
          environment->RemoveAsUserOfInput(use.GetIndex());
          environment->SetRawEnvAt(use.GetIndex(), nullptr);
        }
      }
    } else {
      // The information we have on the users was not enough to decide whether the
      // instruction could be moved.
      // Add the users to the work list, and keep the instruction in the work list
      // to process it again once all users have been processed.
      for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
        AddInstruction(use.GetUser(), processed_instructions, post_dominated, &worklist);
      }
    }
  }

  // Make sure we process instructions in dominated order. This is required for heap
  // stores.
  std::sort(move_in_order.begin(), move_in_order.end(), [](HInstruction* a, HInstruction* b) {
    return b->StrictlyDominates(a);
  });

  // Step (3): Try to move sinking candidates.
  for (HInstruction* instruction : move_in_order) {
    HInstruction* position = nullptr;
    if (instruction->IsArraySet()
            || instruction->IsInstanceFieldSet()
            || instruction->IsConstructorFence()) {
      if (!instructions_that_can_move.IsBitSet(instruction->InputAt(0)->GetId())) {
        // A store can trivially move, but it can safely do so only if the heap
        // location it stores to can also move.
        // TODO(ngeoffray): Handle allocation/store cycles by pruning these instructions
        // from the set and all their inputs.
        continue;
      }
      // Find the position of the instruction we're storing into, filtering out this
      // store and all other stores to that instruction.
      position = FindIdealPosition(instruction->InputAt(0), post_dominated, /* filter */ true);

      // The position needs to be dominated by the store, in order for the store to move there.
      if (position == nullptr || !instruction->GetBlock()->Dominates(position->GetBlock())) {
        continue;
      }
    } else {
      // Find the ideal position within the post dominated blocks.
      position = FindIdealPosition(instruction, post_dominated);
      if (position == nullptr) {
        continue;
      }
    }
    // Bail if we could not find a position in the post dominated blocks (for example,
    // if there are multiple users whose common dominator is not in the list of
    // post dominated blocks).
    if (!post_dominated.IsBitSet(position->GetBlock()->GetBlockId())) {
      continue;
    }
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSunk);
    instruction->MoveBefore(position, /* ensure_safety */ false);
  }
}

}  // namespace art
