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

#include "ssa_phi_elimination.h"

#include "base/arena_bit_vector.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/bit_vector-inl.h"

namespace art {

void SsaDeadPhiElimination::Run() {
  MarkDeadPhis();
  EliminateDeadPhis();
}

void SsaDeadPhiElimination::MarkDeadPhis() {
  // Use local allocator for allocating memory used by this optimization.
  ScopedArenaAllocator allocator(graph_->GetArenaStack());

  static constexpr size_t kDefaultWorklistSize = 8;
  ScopedArenaVector<HPhi*> worklist(allocator.Adapter(kArenaAllocSsaPhiElimination));
  worklist.reserve(kDefaultWorklistSize);

  // Phis are constructed live and should not be revived if previously marked
  // dead. This algorithm temporarily breaks that invariant but we DCHECK that
  // only phis which were initially live are revived.
  ScopedArenaSet<HPhi*> initially_live(allocator.Adapter(kArenaAllocSsaPhiElimination));

  // Add to the worklist phis referenced by non-phi instructions.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (phi->IsDead()) {
        continue;
      }

      bool keep_alive = (graph_->IsDebuggable() && phi->HasEnvironmentUses());
      if (!keep_alive) {
        for (const HUseListNode<HInstruction*>& use : phi->GetUses()) {
          if (!use.GetUser()->IsPhi()) {
            keep_alive = true;
            break;
          }
        }
      }

      if (keep_alive) {
        worklist.push_back(phi);
      } else {
        phi->SetDead();
        if (kIsDebugBuild) {
          initially_live.insert(phi);
        }
      }
    }
  }

  // Process the worklist by propagating liveness to phi inputs.
  while (!worklist.empty()) {
    HPhi* phi = worklist.back();
    worklist.pop_back();
    for (HInstruction* raw_input : phi->GetInputs()) {
      HPhi* input = raw_input->AsPhi();
      if (input != nullptr && input->IsDead()) {
        // Input is a dead phi. Revive it and add to the worklist. We make sure
        // that the phi was not dead initially (see definition of `initially_live`).
        DCHECK(ContainsElement(initially_live, input));
        input->SetLive();
        worklist.push_back(input);
      }
    }
  }
}

void SsaDeadPhiElimination::EliminateDeadPhis() {
  // Remove phis that are not live. Visit in post order so that phis
  // that are not inputs of loop phis can be removed when they have
  // no users left (dead phis might use dead phis).
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    HInstruction* current = block->GetFirstPhi();
    HInstruction* next = nullptr;
    HPhi* phi;
    while (current != nullptr) {
      phi = current->AsPhi();
      next = current->GetNext();
      if (phi->IsDead()) {
        // Make sure the phi is only used by other dead phis.
        if (kIsDebugBuild) {
          for (const HUseListNode<HInstruction*>& use : phi->GetUses()) {
            HInstruction* user = use.GetUser();
            DCHECK(user->IsLoopHeaderPhi());
            DCHECK(user->AsPhi()->IsDead());
          }
        }
        // Remove the phi from use lists of its inputs.
        phi->RemoveAsUserOfAllInputs();
        // Remove the phi from environments that use it.
        for (const HUseListNode<HEnvironment*>& use : phi->GetEnvUses()) {
          HEnvironment* user = use.GetUser();
          user->SetRawEnvAt(use.GetIndex(), nullptr);
        }
        // Delete it from the instruction list.
        block->RemovePhi(phi, /*ensure_safety=*/ false);
      }
      current = next;
    }
  }
}

void SsaRedundantPhiElimination::Run() {
  // Use local allocator for allocating memory used by this optimization.
  ScopedArenaAllocator allocator(graph_->GetArenaStack());

  static constexpr size_t kDefaultWorklistSize = 8;
  ScopedArenaVector<HPhi*> worklist(allocator.Adapter(kArenaAllocSsaPhiElimination));
  worklist.reserve(kDefaultWorklistSize);

  // Add all phis in the worklist. Order does not matter for correctness, and
  // neither will necessarily converge faster.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      worklist.push_back(inst_it.Current()->AsPhi());
    }
  }

  ArenaBitVector visited_phis_in_cycle(&allocator,
                                       graph_->GetCurrentInstructionId(),
                                       /* expandable */ false,
                                       kArenaAllocSsaPhiElimination);
  visited_phis_in_cycle.ClearAllBits();
  ScopedArenaVector<HPhi*> cycle_worklist(allocator.Adapter(kArenaAllocSsaPhiElimination));

  while (!worklist.empty()) {
    HPhi* phi = worklist.back();
    worklist.pop_back();

    // If the phi has already been processed, continue.
    if (!phi->IsInBlock()) {
      continue;
    }

    // If the phi is dead, we know we won't revive it and it will be removed,
    // so don't process it.
    if (phi->IsDead()) {
      continue;
    }

    HInstruction* candidate = nullptr;
    visited_phis_in_cycle.ClearAllBits();
    cycle_worklist.clear();

    cycle_worklist.push_back(phi);
    visited_phis_in_cycle.SetBit(phi->GetId());
    bool catch_phi_in_cycle = phi->IsCatchPhi();
    bool irreducible_loop_phi_in_cycle = phi->IsIrreducibleLoopHeaderPhi();

    // First do a simple loop over inputs and check if they are all the same.
    for (HInstruction* input : phi->GetInputs()) {
      if (input == phi) {
        continue;
      } else if (candidate == nullptr) {
        candidate = input;
      } else if (candidate != input) {
        candidate = nullptr;
        break;
      }
    }

    // If we haven't found a candidate, check for a phi cycle. Note that we need to detect
    // such cycles to avoid having reference and non-reference equivalents. We check this
    // invariant in the graph checker.
    if (candidate == nullptr) {
      // We iterate over the array as long as it grows.
      for (size_t i = 0; i < cycle_worklist.size(); ++i) {
        HPhi* current = cycle_worklist[i];
        DCHECK(!current->IsLoopHeaderPhi() ||
               current->GetBlock()->IsLoopPreHeaderFirstPredecessor());

        for (HInstruction* input : current->GetInputs()) {
          if (input == current) {
            continue;
          } else if (input->IsPhi()) {
            if (!visited_phis_in_cycle.IsBitSet(input->GetId())) {
              cycle_worklist.push_back(input->AsPhi());
              visited_phis_in_cycle.SetBit(input->GetId());
              catch_phi_in_cycle |= input->AsPhi()->IsCatchPhi();
              irreducible_loop_phi_in_cycle |= input->IsIrreducibleLoopHeaderPhi();
            } else {
              // Already visited, nothing to do.
            }
          } else if (candidate == nullptr) {
            candidate = input;
          } else if (candidate != input) {
            candidate = nullptr;
            // Clear the cycle worklist to break out of the outer loop.
            cycle_worklist.clear();
            break;
          }
        }
      }
    }

    if (candidate == nullptr) {
      continue;
    }

    if (irreducible_loop_phi_in_cycle && !candidate->IsConstant()) {
      // For irreducible loops, we need to keep the phis to satisfy our linear scan
      // algorithm.
      // There is one exception for constants, as the type propagation requires redundant
      // cyclic phis of a constant to be removed. This is ok for the linear scan as it
      // has to deal with constants anyway, and they can trivially be rematerialized.
      continue;
    }

    for (HPhi* current : cycle_worklist) {
      // The candidate may not dominate a phi in a catch block: there may be non-throwing
      // instructions at the beginning of a try range, that may be the first input of
      // catch phis.
      // TODO(dbrazdil): Remove this situation by moving those non-throwing instructions
      // before the try entry.
      if (catch_phi_in_cycle) {
        if (!candidate->StrictlyDominates(current)) {
          continue;
        }
      } else {
        DCHECK(candidate->StrictlyDominates(current));
      }

      // Because we're updating the users of this phi, we may have new candidates
      // for elimination. Add phis that use this phi to the worklist.
      for (const HUseListNode<HInstruction*>& use : current->GetUses()) {
        HInstruction* user = use.GetUser();
        if (user->IsPhi() && !visited_phis_in_cycle.IsBitSet(user->GetId())) {
          worklist.push_back(user->AsPhi());
        }
      }
      DCHECK(candidate->StrictlyDominates(current));
      current->ReplaceWith(candidate);
      current->GetBlock()->RemovePhi(current);
    }
  }
}

}  // namespace art
