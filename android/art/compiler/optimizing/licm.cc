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

#include "licm.h"

#include "side_effects_analysis.h"

namespace art {

static bool IsPhiOf(HInstruction* instruction, HBasicBlock* block) {
  return instruction->IsPhi() && instruction->GetBlock() == block;
}

/**
 * Returns whether `instruction` has all its inputs and environment defined
 * before the loop it is in.
 */
static bool InputsAreDefinedBeforeLoop(HInstruction* instruction) {
  DCHECK(instruction->IsInLoop());
  HLoopInformation* info = instruction->GetBlock()->GetLoopInformation();
  for (const HInstruction* input : instruction->GetInputs()) {
    HLoopInformation* input_loop = input->GetBlock()->GetLoopInformation();
    // We only need to check whether the input is defined in the loop. If it is not
    // it is defined before the loop.
    if (input_loop != nullptr && input_loop->IsIn(*info)) {
      return false;
    }
  }

  for (HEnvironment* environment = instruction->GetEnvironment();
       environment != nullptr;
       environment = environment->GetParent()) {
    for (size_t i = 0, e = environment->Size(); i < e; ++i) {
      HInstruction* input = environment->GetInstructionAt(i);
      if (input != nullptr) {
        HLoopInformation* input_loop = input->GetBlock()->GetLoopInformation();
        if (input_loop != nullptr && input_loop->IsIn(*info)) {
          // We can move an instruction that takes a loop header phi in the environment:
          // we will just replace that phi with its first input later in `UpdateLoopPhisIn`.
          bool is_loop_header_phi = IsPhiOf(input, info->GetHeader());
          if (!is_loop_header_phi) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

/**
 * If `environment` has a loop header phi, we replace it with its first input.
 */
static void UpdateLoopPhisIn(HEnvironment* environment, HLoopInformation* info) {
  for (; environment != nullptr; environment = environment->GetParent()) {
    for (size_t i = 0, e = environment->Size(); i < e; ++i) {
      HInstruction* input = environment->GetInstructionAt(i);
      if (input != nullptr && IsPhiOf(input, info->GetHeader())) {
        environment->RemoveAsUserOfInput(i);
        HInstruction* incoming = input->InputAt(0);
        environment->SetRawEnvAt(i, incoming);
        incoming->AddEnvUseAt(environment, i);
      }
    }
  }
}

void LICM::Run() {
  DCHECK(side_effects_.HasRun());

  // Only used during debug.
  ArenaBitVector* visited = nullptr;
  if (kIsDebugBuild) {
    visited = new (graph_->GetAllocator()) ArenaBitVector(graph_->GetAllocator(),
                                                          graph_->GetBlocks().size(),
                                                          false,
                                                          kArenaAllocLICM);
  }

  // Post order visit to visit inner loops before outer loops.
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!block->IsLoopHeader()) {
      // Only visit the loop when we reach the header.
      continue;
    }

    HLoopInformation* loop_info = block->GetLoopInformation();
    SideEffects loop_effects = side_effects_.GetLoopEffects(block);
    HBasicBlock* pre_header = loop_info->GetPreHeader();

    for (HBlocksInLoopIterator it_loop(*loop_info); !it_loop.Done(); it_loop.Advance()) {
      HBasicBlock* inner = it_loop.Current();
      DCHECK(inner->IsInLoop());
      if (inner->GetLoopInformation() != loop_info) {
        // Thanks to post order visit, inner loops were already visited.
        DCHECK(visited->IsBitSet(inner->GetBlockId()));
        continue;
      }
      if (kIsDebugBuild) {
        visited->SetBit(inner->GetBlockId());
      }

      if (loop_info->ContainsIrreducibleLoop()) {
        // We cannot licm in an irreducible loop, or in a natural loop containing an
        // irreducible loop.
        continue;
      }
      DCHECK(!loop_info->IsIrreducible());

      // We can move an instruction that can throw only as long as it is the first visible
      // instruction (throw or write) in the loop. Note that the first potentially visible
      // instruction that is not hoisted stops this optimization. Non-throwing instructions,
      // on the other hand, can still be hoisted.
      bool found_first_non_hoisted_visible_instruction_in_loop = !inner->IsLoopHeader();
      for (HInstructionIterator inst_it(inner->GetInstructions());
           !inst_it.Done();
           inst_it.Advance()) {
        HInstruction* instruction = inst_it.Current();
        bool can_move = false;
        if (instruction->CanBeMoved() && InputsAreDefinedBeforeLoop(instruction)) {
          if (instruction->CanThrow()) {
            if (!found_first_non_hoisted_visible_instruction_in_loop) {
              DCHECK(instruction->GetBlock()->IsLoopHeader());
              if (instruction->IsClinitCheck()) {
                // clinit is only done once, and since all visible instructions
                // in the loop header so far have been hoisted out, we can hoist
                // the clinit check out also.
                can_move = true;
              } else if (!instruction->GetSideEffects().MayDependOn(loop_effects)) {
                can_move = true;
              }
            }
          } else if (!instruction->GetSideEffects().MayDependOn(loop_effects)) {
            can_move = true;
          }
        }
        if (can_move) {
          // We need to update the environment if the instruction has a loop header
          // phi in it.
          if (instruction->NeedsEnvironment()) {
            UpdateLoopPhisIn(instruction->GetEnvironment(), loop_info);
          } else {
            DCHECK(!instruction->HasEnvironment());
          }
          instruction->MoveBefore(pre_header->GetLastInstruction());
          MaybeRecordStat(stats_, MethodCompilationStat::kLoopInvariantMoved);
        }

        if (!can_move && (instruction->CanThrow() || instruction->DoesAnyWrite())) {
          // If `instruction` can do something visible (throw or write),
          // we cannot move further instructions that can throw.
          found_first_non_hoisted_visible_instruction_in_loop = true;
        }
      }
    }
  }
}

}  // namespace art
