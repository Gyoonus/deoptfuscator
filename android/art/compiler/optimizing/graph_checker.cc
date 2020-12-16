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

#include "graph_checker.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "android-base/stringprintf.h"

#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"

namespace art {

using android::base::StringPrintf;

static bool IsAllowedToJumpToExitBlock(HInstruction* instruction) {
  // Anything that returns is allowed to jump into the exit block.
  if (instruction->IsReturn() || instruction->IsReturnVoid()) {
    return true;
  }
  // Anything that always throws is allowed to jump into the exit block.
  if (instruction->IsGoto() && instruction->GetPrevious() != nullptr) {
    instruction = instruction->GetPrevious();
  }
  return instruction->AlwaysThrows();
}

static bool IsExitTryBoundaryIntoExitBlock(HBasicBlock* block) {
  if (!block->IsSingleTryBoundary()) {
    return false;
  }

  HTryBoundary* boundary = block->GetLastInstruction()->AsTryBoundary();
  return block->GetPredecessors().size() == 1u &&
         boundary->GetNormalFlowSuccessor()->IsExitBlock() &&
         !boundary->IsEntry();
}

void GraphChecker::VisitBasicBlock(HBasicBlock* block) {
  current_block_ = block;

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator(GetGraph()->GetArenaStack());

  // Check consistency with respect to predecessors of `block`.
  // Note: Counting duplicates with a sorted vector uses up to 6x less memory
  // than ArenaSafeMap<HBasicBlock*, size_t> and also allows storage reuse.
  ScopedArenaVector<HBasicBlock*> sorted_predecessors(allocator.Adapter(kArenaAllocGraphChecker));
  sorted_predecessors.assign(block->GetPredecessors().begin(), block->GetPredecessors().end());
  std::sort(sorted_predecessors.begin(), sorted_predecessors.end());
  for (auto it = sorted_predecessors.begin(), end = sorted_predecessors.end(); it != end; ) {
    HBasicBlock* p = *it++;
    size_t p_count_in_block_predecessors = 1u;
    for (; it != end && *it == p; ++it) {
      ++p_count_in_block_predecessors;
    }
    size_t block_count_in_p_successors =
        std::count(p->GetSuccessors().begin(), p->GetSuccessors().end(), block);
    if (p_count_in_block_predecessors != block_count_in_p_successors) {
      AddError(StringPrintf(
          "Block %d lists %zu occurrences of block %d in its predecessors, whereas "
          "block %d lists %zu occurrences of block %d in its successors.",
          block->GetBlockId(), p_count_in_block_predecessors, p->GetBlockId(),
          p->GetBlockId(), block_count_in_p_successors, block->GetBlockId()));
    }
  }

  // Check consistency with respect to successors of `block`.
  // Note: Counting duplicates with a sorted vector uses up to 6x less memory
  // than ArenaSafeMap<HBasicBlock*, size_t> and also allows storage reuse.
  ScopedArenaVector<HBasicBlock*> sorted_successors(allocator.Adapter(kArenaAllocGraphChecker));
  sorted_successors.assign(block->GetSuccessors().begin(), block->GetSuccessors().end());
  std::sort(sorted_successors.begin(), sorted_successors.end());
  for (auto it = sorted_successors.begin(), end = sorted_successors.end(); it != end; ) {
    HBasicBlock* s = *it++;
    size_t s_count_in_block_successors = 1u;
    for (; it != end && *it == s; ++it) {
      ++s_count_in_block_successors;
    }
    size_t block_count_in_s_predecessors =
        std::count(s->GetPredecessors().begin(), s->GetPredecessors().end(), block);
    if (s_count_in_block_successors != block_count_in_s_predecessors) {
      AddError(StringPrintf(
          "Block %d lists %zu occurrences of block %d in its successors, whereas "
          "block %d lists %zu occurrences of block %d in its predecessors.",
          block->GetBlockId(), s_count_in_block_successors, s->GetBlockId(),
          s->GetBlockId(), block_count_in_s_predecessors, block->GetBlockId()));
    }
  }

  // Ensure `block` ends with a branch instruction.
  // This invariant is not enforced on non-SSA graphs. Graph built from DEX with
  // dead code that falls out of the method will not end with a control-flow
  // instruction. Such code is removed during the SSA-building DCE phase.
  if (GetGraph()->IsInSsaForm() && !block->EndsWithControlFlowInstruction()) {
    AddError(StringPrintf("Block %d does not end with a branch instruction.",
                          block->GetBlockId()));
  }

  // Ensure that only Return(Void) and Throw jump to Exit. An exiting TryBoundary
  // may be between the instructions if the Throw/Return(Void) is in a try block.
  if (block->IsExitBlock()) {
    for (HBasicBlock* predecessor : block->GetPredecessors()) {
      HInstruction* last_instruction = IsExitTryBoundaryIntoExitBlock(predecessor) ?
        predecessor->GetSinglePredecessor()->GetLastInstruction() :
        predecessor->GetLastInstruction();
      if (!IsAllowedToJumpToExitBlock(last_instruction)) {
        AddError(StringPrintf("Unexpected instruction %s:%d jumps into the exit block.",
                              last_instruction->DebugName(),
                              last_instruction->GetId()));
      }
    }
  }

  // Visit this block's list of phis.
  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* current = it.Current();
    // Ensure this block's list of phis contains only phis.
    if (!current->IsPhi()) {
      AddError(StringPrintf("Block %d has a non-phi in its phi list.",
                            current_block_->GetBlockId()));
    }
    if (current->GetNext() == nullptr && current != block->GetLastPhi()) {
      AddError(StringPrintf("The recorded last phi of block %d does not match "
                            "the actual last phi %d.",
                            current_block_->GetBlockId(),
                            current->GetId()));
    }
    current->Accept(this);
  }

  // Visit this block's list of instructions.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* current = it.Current();
    // Ensure this block's list of instructions does not contains phis.
    if (current->IsPhi()) {
      AddError(StringPrintf("Block %d has a phi in its non-phi list.",
                            current_block_->GetBlockId()));
    }
    if (current->GetNext() == nullptr && current != block->GetLastInstruction()) {
      AddError(StringPrintf("The recorded last instruction of block %d does not match "
                            "the actual last instruction %d.",
                            current_block_->GetBlockId(),
                            current->GetId()));
    }
    current->Accept(this);
  }

  // Ensure that catch blocks are not normal successors, and normal blocks are
  // never exceptional successors.
  for (HBasicBlock* successor : block->GetNormalSuccessors()) {
    if (successor->IsCatchBlock()) {
      AddError(StringPrintf("Catch block %d is a normal successor of block %d.",
                            successor->GetBlockId(),
                            block->GetBlockId()));
    }
  }
  for (HBasicBlock* successor : block->GetExceptionalSuccessors()) {
    if (!successor->IsCatchBlock()) {
      AddError(StringPrintf("Normal block %d is an exceptional successor of block %d.",
                            successor->GetBlockId(),
                            block->GetBlockId()));
    }
  }

  // Ensure dominated blocks have `block` as the dominator.
  for (HBasicBlock* dominated : block->GetDominatedBlocks()) {
    if (dominated->GetDominator() != block) {
      AddError(StringPrintf("Block %d should be the dominator of %d.",
                            block->GetBlockId(),
                            dominated->GetBlockId()));
    }
  }

  // Ensure there is no critical edge (i.e., an edge connecting a
  // block with multiple successors to a block with multiple
  // predecessors). Exceptional edges are synthesized and hence
  // not accounted for.
  if (block->GetSuccessors().size() > 1) {
    if (IsExitTryBoundaryIntoExitBlock(block)) {
      // Allowed critical edge (Throw/Return/ReturnVoid)->TryBoundary->Exit.
    } else {
      for (HBasicBlock* successor : block->GetNormalSuccessors()) {
        if (successor->GetPredecessors().size() > 1) {
          AddError(StringPrintf("Critical edge between blocks %d and %d.",
                                block->GetBlockId(),
                                successor->GetBlockId()));
        }
      }
    }
  }

  // Ensure try membership information is consistent.
  if (block->IsCatchBlock()) {
    if (block->IsTryBlock()) {
      const HTryBoundary& try_entry = block->GetTryCatchInformation()->GetTryEntry();
      AddError(StringPrintf("Catch blocks should not be try blocks but catch block %d "
                            "has try entry %s:%d.",
                            block->GetBlockId(),
                            try_entry.DebugName(),
                            try_entry.GetId()));
    }

    if (block->IsLoopHeader()) {
      AddError(StringPrintf("Catch blocks should not be loop headers but catch block %d is.",
                            block->GetBlockId()));
    }
  } else {
    for (HBasicBlock* predecessor : block->GetPredecessors()) {
      const HTryBoundary* incoming_try_entry = predecessor->ComputeTryEntryOfSuccessors();
      if (block->IsTryBlock()) {
        const HTryBoundary& stored_try_entry = block->GetTryCatchInformation()->GetTryEntry();
        if (incoming_try_entry == nullptr) {
          AddError(StringPrintf("Block %d has try entry %s:%d but no try entry follows "
                                "from predecessor %d.",
                                block->GetBlockId(),
                                stored_try_entry.DebugName(),
                                stored_try_entry.GetId(),
                                predecessor->GetBlockId()));
        } else if (!incoming_try_entry->HasSameExceptionHandlersAs(stored_try_entry)) {
          AddError(StringPrintf("Block %d has try entry %s:%d which is not consistent "
                                "with %s:%d that follows from predecessor %d.",
                                block->GetBlockId(),
                                stored_try_entry.DebugName(),
                                stored_try_entry.GetId(),
                                incoming_try_entry->DebugName(),
                                incoming_try_entry->GetId(),
                                predecessor->GetBlockId()));
        }
      } else if (incoming_try_entry != nullptr) {
        AddError(StringPrintf("Block %d is not a try block but try entry %s:%d follows "
                              "from predecessor %d.",
                              block->GetBlockId(),
                              incoming_try_entry->DebugName(),
                              incoming_try_entry->GetId(),
                              predecessor->GetBlockId()));
      }
    }
  }

  if (block->IsLoopHeader()) {
    HandleLoop(block);
  }
}

void GraphChecker::VisitBoundsCheck(HBoundsCheck* check) {
  if (!GetGraph()->HasBoundsChecks()) {
    AddError(StringPrintf("Instruction %s:%d is a HBoundsCheck, "
                          "but HasBoundsChecks() returns false",
                          check->DebugName(),
                          check->GetId()));
  }

  // Perform the instruction base checks too.
  VisitInstruction(check);
}

void GraphChecker::VisitDeoptimize(HDeoptimize* deopt) {
  if (GetGraph()->IsCompilingOsr()) {
    AddError(StringPrintf("A graph compiled OSR cannot have a HDeoptimize instruction"));
  }

  // Perform the instruction base checks too.
  VisitInstruction(deopt);
}

void GraphChecker::VisitTryBoundary(HTryBoundary* try_boundary) {
  ArrayRef<HBasicBlock* const> handlers = try_boundary->GetExceptionHandlers();

  // Ensure that all exception handlers are catch blocks.
  // Note that a normal-flow successor may be a catch block before CFG
  // simplification. We only test normal-flow successors in GraphChecker.
  for (HBasicBlock* handler : handlers) {
    if (!handler->IsCatchBlock()) {
      AddError(StringPrintf("Block %d with %s:%d has exceptional successor %d which "
                            "is not a catch block.",
                            current_block_->GetBlockId(),
                            try_boundary->DebugName(),
                            try_boundary->GetId(),
                            handler->GetBlockId()));
    }
  }

  // Ensure that handlers are not listed multiple times.
  for (size_t i = 0, e = handlers.size(); i < e; ++i) {
    if (ContainsElement(handlers, handlers[i], i + 1)) {
        AddError(StringPrintf("Exception handler block %d of %s:%d is listed multiple times.",
                            handlers[i]->GetBlockId(),
                            try_boundary->DebugName(),
                            try_boundary->GetId()));
    }
  }

  VisitInstruction(try_boundary);
}

void GraphChecker::VisitLoadException(HLoadException* load) {
  // Ensure that LoadException is the first instruction in a catch block.
  if (!load->GetBlock()->IsCatchBlock()) {
    AddError(StringPrintf("%s:%d is in a non-catch block %d.",
                          load->DebugName(),
                          load->GetId(),
                          load->GetBlock()->GetBlockId()));
  } else if (load->GetBlock()->GetFirstInstruction() != load) {
    AddError(StringPrintf("%s:%d is not the first instruction in catch block %d.",
                          load->DebugName(),
                          load->GetId(),
                          load->GetBlock()->GetBlockId()));
  }
}

void GraphChecker::VisitInstruction(HInstruction* instruction) {
  if (seen_ids_.IsBitSet(instruction->GetId())) {
    AddError(StringPrintf("Instruction id %d is duplicate in graph.",
                          instruction->GetId()));
  } else {
    seen_ids_.SetBit(instruction->GetId());
  }

  // Ensure `instruction` is associated with `current_block_`.
  if (instruction->GetBlock() == nullptr) {
    AddError(StringPrintf("%s %d in block %d not associated with any block.",
                          instruction->IsPhi() ? "Phi" : "Instruction",
                          instruction->GetId(),
                          current_block_->GetBlockId()));
  } else if (instruction->GetBlock() != current_block_) {
    AddError(StringPrintf("%s %d in block %d associated with block %d.",
                          instruction->IsPhi() ? "Phi" : "Instruction",
                          instruction->GetId(),
                          current_block_->GetBlockId(),
                          instruction->GetBlock()->GetBlockId()));
  }

  // Ensure the inputs of `instruction` are defined in a block of the graph.
  for (HInstruction* input : instruction->GetInputs()) {
    if (input->GetBlock() == nullptr) {
      AddError(StringPrintf("Input %d of instruction %d is not in any "
                            "basic block of the control-flow graph.",
                            input->GetId(),
                            instruction->GetId()));
    } else {
      const HInstructionList& list = input->IsPhi()
          ? input->GetBlock()->GetPhis()
          : input->GetBlock()->GetInstructions();
      if (!list.Contains(input)) {
        AddError(StringPrintf("Input %d of instruction %d is not defined "
                              "in a basic block of the control-flow graph.",
                              input->GetId(),
                              instruction->GetId()));
      }
    }
  }

  // Ensure the uses of `instruction` are defined in a block of the graph,
  // and the entry in the use list is consistent.
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    HInstruction* user = use.GetUser();
    const HInstructionList& list = user->IsPhi()
        ? user->GetBlock()->GetPhis()
        : user->GetBlock()->GetInstructions();
    if (!list.Contains(user)) {
      AddError(StringPrintf("User %s:%d of instruction %d is not defined "
                            "in a basic block of the control-flow graph.",
                            user->DebugName(),
                            user->GetId(),
                            instruction->GetId()));
    }
    size_t use_index = use.GetIndex();
    HConstInputsRef user_inputs = user->GetInputs();
    if ((use_index >= user_inputs.size()) || (user_inputs[use_index] != instruction)) {
      AddError(StringPrintf("User %s:%d of instruction %s:%d has a wrong "
                            "UseListNode index.",
                            user->DebugName(),
                            user->GetId(),
                            instruction->DebugName(),
                            instruction->GetId()));
    }
  }

  // Ensure the environment uses entries are consistent.
  for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
    HEnvironment* user = use.GetUser();
    size_t use_index = use.GetIndex();
    if ((use_index >= user->Size()) || (user->GetInstructionAt(use_index) != instruction)) {
      AddError(StringPrintf("Environment user of %s:%d has a wrong "
                            "UseListNode index.",
                            instruction->DebugName(),
                            instruction->GetId()));
    }
  }

  // Ensure 'instruction' has pointers to its inputs' use entries.
  auto&& input_records = instruction->GetInputRecords();
  for (size_t i = 0; i < input_records.size(); ++i) {
    const HUserRecord<HInstruction*>& input_record = input_records[i];
    HInstruction* input = input_record.GetInstruction();
    if ((input_record.GetBeforeUseNode() == input->GetUses().end()) ||
        (input_record.GetUseNode() == input->GetUses().end()) ||
        !input->GetUses().ContainsNode(*input_record.GetUseNode()) ||
        (input_record.GetUseNode()->GetIndex() != i)) {
      AddError(StringPrintf("Instruction %s:%d has an invalid iterator before use entry "
                            "at input %u (%s:%d).",
                            instruction->DebugName(),
                            instruction->GetId(),
                            static_cast<unsigned>(i),
                            input->DebugName(),
                            input->GetId()));
    }
  }

  // Ensure an instruction dominates all its uses.
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    HInstruction* user = use.GetUser();
    if (!user->IsPhi() && !instruction->StrictlyDominates(user)) {
      AddError(StringPrintf("Instruction %s:%d in block %d does not dominate "
                            "use %s:%d in block %d.",
                            instruction->DebugName(),
                            instruction->GetId(),
                            current_block_->GetBlockId(),
                            user->DebugName(),
                            user->GetId(),
                            user->GetBlock()->GetBlockId()));
    }
  }

  if (instruction->NeedsEnvironment() && !instruction->HasEnvironment()) {
    AddError(StringPrintf("Instruction %s:%d in block %d requires an environment "
                          "but does not have one.",
                          instruction->DebugName(),
                          instruction->GetId(),
                          current_block_->GetBlockId()));
  }

  // Ensure an instruction having an environment is dominated by the
  // instructions contained in the environment.
  for (HEnvironment* environment = instruction->GetEnvironment();
       environment != nullptr;
       environment = environment->GetParent()) {
    for (size_t i = 0, e = environment->Size(); i < e; ++i) {
      HInstruction* env_instruction = environment->GetInstructionAt(i);
      if (env_instruction != nullptr
          && !env_instruction->StrictlyDominates(instruction)) {
        AddError(StringPrintf("Instruction %d in environment of instruction %d "
                              "from block %d does not dominate instruction %d.",
                              env_instruction->GetId(),
                              instruction->GetId(),
                              current_block_->GetBlockId(),
                              instruction->GetId()));
      }
    }
  }

  // Ensure that reference type instructions have reference type info.
  if (instruction->GetType() == DataType::Type::kReference) {
    if (!instruction->GetReferenceTypeInfo().IsValid()) {
      AddError(StringPrintf("Reference type instruction %s:%d does not have "
                            "valid reference type information.",
                            instruction->DebugName(),
                            instruction->GetId()));
    }
  }

  if (instruction->CanThrowIntoCatchBlock()) {
    // Find the top-level environment. This corresponds to the environment of
    // the catch block since we do not inline methods with try/catch.
    HEnvironment* environment = instruction->GetEnvironment();
    while (environment->GetParent() != nullptr) {
      environment = environment->GetParent();
    }

    // Find all catch blocks and test that `instruction` has an environment
    // value for each one.
    const HTryBoundary& entry = instruction->GetBlock()->GetTryCatchInformation()->GetTryEntry();
    for (HBasicBlock* catch_block : entry.GetExceptionHandlers()) {
      for (HInstructionIterator phi_it(catch_block->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
        HPhi* catch_phi = phi_it.Current()->AsPhi();
        if (environment->GetInstructionAt(catch_phi->GetRegNumber()) == nullptr) {
          AddError(StringPrintf("Instruction %s:%d throws into catch block %d "
                                "with catch phi %d for vreg %d but its "
                                "corresponding environment slot is empty.",
                                instruction->DebugName(),
                                instruction->GetId(),
                                catch_block->GetBlockId(),
                                catch_phi->GetId(),
                                catch_phi->GetRegNumber()));
        }
      }
    }
  }
}

void GraphChecker::VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) {
  VisitInstruction(invoke);

  if (invoke->IsStaticWithExplicitClinitCheck()) {
    const HInstruction* last_input = invoke->GetInputs().back();
    if (last_input == nullptr) {
      AddError(StringPrintf("Static invoke %s:%d marked as having an explicit clinit check "
                            "has a null pointer as last input.",
                            invoke->DebugName(),
                            invoke->GetId()));
    } else if (!last_input->IsClinitCheck() && !last_input->IsLoadClass()) {
      AddError(StringPrintf("Static invoke %s:%d marked as having an explicit clinit check "
                            "has a last instruction (%s:%d) which is neither a clinit check "
                            "nor a load class instruction.",
                            invoke->DebugName(),
                            invoke->GetId(),
                            last_input->DebugName(),
                            last_input->GetId()));
    }
  }
}

void GraphChecker::VisitReturn(HReturn* ret) {
  VisitInstruction(ret);
  HBasicBlock* successor = ret->GetBlock()->GetSingleSuccessor();
  if (!successor->IsExitBlock() && !IsExitTryBoundaryIntoExitBlock(successor)) {
    AddError(StringPrintf("%s:%d does not jump to the exit block.",
                          ret->DebugName(),
                          ret->GetId()));
  }
}

void GraphChecker::VisitReturnVoid(HReturnVoid* ret) {
  VisitInstruction(ret);
  HBasicBlock* successor = ret->GetBlock()->GetSingleSuccessor();
  if (!successor->IsExitBlock() && !IsExitTryBoundaryIntoExitBlock(successor)) {
    AddError(StringPrintf("%s:%d does not jump to the exit block.",
                          ret->DebugName(),
                          ret->GetId()));
  }
}

void GraphChecker::VisitCheckCast(HCheckCast* check) {
  VisitInstruction(check);
  HInstruction* input = check->InputAt(1);
  if (!input->IsLoadClass()) {
    AddError(StringPrintf("%s:%d expects a HLoadClass as second input, not %s:%d.",
                          check->DebugName(),
                          check->GetId(),
                          input->DebugName(),
                          input->GetId()));
  }
}

void GraphChecker::VisitInstanceOf(HInstanceOf* instruction) {
  VisitInstruction(instruction);
  HInstruction* input = instruction->InputAt(1);
  if (!input->IsLoadClass()) {
    AddError(StringPrintf("%s:%d expects a HLoadClass as second input, not %s:%d.",
                          instruction->DebugName(),
                          instruction->GetId(),
                          input->DebugName(),
                          input->GetId()));
  }
}

void GraphChecker::HandleLoop(HBasicBlock* loop_header) {
  int id = loop_header->GetBlockId();
  HLoopInformation* loop_information = loop_header->GetLoopInformation();

  if (loop_information->GetPreHeader()->GetSuccessors().size() != 1) {
    AddError(StringPrintf(
        "Loop pre-header %d of loop defined by header %d has %zu successors.",
        loop_information->GetPreHeader()->GetBlockId(),
        id,
        loop_information->GetPreHeader()->GetSuccessors().size()));
  }

  if (loop_information->GetSuspendCheck() == nullptr) {
    AddError(StringPrintf(
        "Loop with header %d does not have a suspend check.",
        loop_header->GetBlockId()));
  }

  if (loop_information->GetSuspendCheck() != loop_header->GetFirstInstructionDisregardMoves()) {
    AddError(StringPrintf(
        "Loop header %d does not have the loop suspend check as the first instruction.",
        loop_header->GetBlockId()));
  }

  // Ensure the loop header has only one incoming branch and the remaining
  // predecessors are back edges.
  size_t num_preds = loop_header->GetPredecessors().size();
  if (num_preds < 2) {
    AddError(StringPrintf(
        "Loop header %d has less than two predecessors: %zu.",
        id,
        num_preds));
  } else {
    HBasicBlock* first_predecessor = loop_header->GetPredecessors()[0];
    if (loop_information->IsBackEdge(*first_predecessor)) {
      AddError(StringPrintf(
          "First predecessor of loop header %d is a back edge.",
          id));
    }
    for (size_t i = 1, e = loop_header->GetPredecessors().size(); i < e; ++i) {
      HBasicBlock* predecessor = loop_header->GetPredecessors()[i];
      if (!loop_information->IsBackEdge(*predecessor)) {
        AddError(StringPrintf(
            "Loop header %d has multiple incoming (non back edge) blocks: %d.",
            id,
            predecessor->GetBlockId()));
      }
    }
  }

  const ArenaBitVector& loop_blocks = loop_information->GetBlocks();

  // Ensure back edges belong to the loop.
  if (loop_information->NumberOfBackEdges() == 0) {
    AddError(StringPrintf(
        "Loop defined by header %d has no back edge.",
        id));
  } else {
    for (HBasicBlock* back_edge : loop_information->GetBackEdges()) {
      int back_edge_id = back_edge->GetBlockId();
      if (!loop_blocks.IsBitSet(back_edge_id)) {
        AddError(StringPrintf(
            "Loop defined by header %d has an invalid back edge %d.",
            id,
            back_edge_id));
      } else if (back_edge->GetLoopInformation() != loop_information) {
        AddError(StringPrintf(
            "Back edge %d of loop defined by header %d belongs to nested loop "
            "with header %d.",
            back_edge_id,
            id,
            back_edge->GetLoopInformation()->GetHeader()->GetBlockId()));
      }
    }
  }

  // If this is a nested loop, ensure the outer loops contain a superset of the blocks.
  for (HLoopInformationOutwardIterator it(*loop_header); !it.Done(); it.Advance()) {
    HLoopInformation* outer_info = it.Current();
    if (!loop_blocks.IsSubsetOf(&outer_info->GetBlocks())) {
      AddError(StringPrintf("Blocks of loop defined by header %d are not a subset of blocks of "
                            "an outer loop defined by header %d.",
                            id,
                            outer_info->GetHeader()->GetBlockId()));
    }
  }

  // Ensure the pre-header block is first in the list of predecessors of a loop
  // header and that the header block is its only successor.
  if (!loop_header->IsLoopPreHeaderFirstPredecessor()) {
    AddError(StringPrintf(
        "Loop pre-header is not the first predecessor of the loop header %d.",
        id));
  }

  // Ensure all blocks in the loop are live and dominated by the loop header in
  // the case of natural loops.
  for (uint32_t i : loop_blocks.Indexes()) {
    HBasicBlock* loop_block = GetGraph()->GetBlocks()[i];
    if (loop_block == nullptr) {
      AddError(StringPrintf("Loop defined by header %d contains a previously removed block %d.",
                            id,
                            i));
    } else if (!loop_information->IsIrreducible() && !loop_header->Dominates(loop_block)) {
      AddError(StringPrintf("Loop block %d not dominated by loop header %d.",
                            i,
                            id));
    }
  }
}

static bool IsSameSizeConstant(const HInstruction* insn1, const HInstruction* insn2) {
  return insn1->IsConstant()
      && insn2->IsConstant()
      && DataType::Is64BitType(insn1->GetType()) == DataType::Is64BitType(insn2->GetType());
}

static bool IsConstantEquivalent(const HInstruction* insn1,
                                 const HInstruction* insn2,
                                 BitVector* visited) {
  if (insn1->IsPhi() &&
      insn1->AsPhi()->IsVRegEquivalentOf(insn2)) {
    HConstInputsRef insn1_inputs = insn1->GetInputs();
    HConstInputsRef insn2_inputs = insn2->GetInputs();
    if (insn1_inputs.size() != insn2_inputs.size()) {
      return false;
    }

    // Testing only one of the two inputs for recursion is sufficient.
    if (visited->IsBitSet(insn1->GetId())) {
      return true;
    }
    visited->SetBit(insn1->GetId());

    for (size_t i = 0; i < insn1_inputs.size(); ++i) {
      if (!IsConstantEquivalent(insn1_inputs[i], insn2_inputs[i], visited)) {
        return false;
      }
    }
    return true;
  } else if (IsSameSizeConstant(insn1, insn2)) {
    return insn1->AsConstant()->GetValueAsUint64() == insn2->AsConstant()->GetValueAsUint64();
  } else {
    return false;
  }
}

void GraphChecker::VisitPhi(HPhi* phi) {
  VisitInstruction(phi);

  // Ensure the first input of a phi is not itself.
  ArrayRef<HUserRecord<HInstruction*>> input_records = phi->GetInputRecords();
  if (input_records[0].GetInstruction() == phi) {
    AddError(StringPrintf("Loop phi %d in block %d is its own first input.",
                          phi->GetId(),
                          phi->GetBlock()->GetBlockId()));
  }

  // Ensure that the inputs have the same primitive kind as the phi.
  for (size_t i = 0; i < input_records.size(); ++i) {
    HInstruction* input = input_records[i].GetInstruction();
    if (DataType::Kind(input->GetType()) != DataType::Kind(phi->GetType())) {
        AddError(StringPrintf(
            "Input %d at index %zu of phi %d from block %d does not have the "
            "same kind as the phi: %s versus %s",
            input->GetId(), i, phi->GetId(), phi->GetBlock()->GetBlockId(),
            DataType::PrettyDescriptor(input->GetType()),
            DataType::PrettyDescriptor(phi->GetType())));
    }
  }
  if (phi->GetType() != HPhi::ToPhiType(phi->GetType())) {
    AddError(StringPrintf("Phi %d in block %d does not have an expected phi type: %s",
                          phi->GetId(),
                          phi->GetBlock()->GetBlockId(),
                          DataType::PrettyDescriptor(phi->GetType())));
  }

  if (phi->IsCatchPhi()) {
    // The number of inputs of a catch phi should be the total number of throwing
    // instructions caught by this catch block. We do not enforce this, however,
    // because we do not remove the corresponding inputs when we prove that an
    // instruction cannot throw. Instead, we at least test that all phis have the
    // same, non-zero number of inputs (b/24054676).
    if (input_records.empty()) {
      AddError(StringPrintf("Phi %d in catch block %d has zero inputs.",
                            phi->GetId(),
                            phi->GetBlock()->GetBlockId()));
    } else {
      HInstruction* next_phi = phi->GetNext();
      if (next_phi != nullptr) {
        size_t input_count_next = next_phi->InputCount();
        if (input_records.size() != input_count_next) {
          AddError(StringPrintf("Phi %d in catch block %d has %zu inputs, "
                                "but phi %d has %zu inputs.",
                                phi->GetId(),
                                phi->GetBlock()->GetBlockId(),
                                input_records.size(),
                                next_phi->GetId(),
                                input_count_next));
        }
      }
    }
  } else {
    // Ensure the number of inputs of a non-catch phi is the same as the number
    // of its predecessors.
    const ArenaVector<HBasicBlock*>& predecessors = phi->GetBlock()->GetPredecessors();
    if (input_records.size() != predecessors.size()) {
      AddError(StringPrintf(
          "Phi %d in block %d has %zu inputs, "
          "but block %d has %zu predecessors.",
          phi->GetId(), phi->GetBlock()->GetBlockId(), input_records.size(),
          phi->GetBlock()->GetBlockId(), predecessors.size()));
    } else {
      // Ensure phi input at index I either comes from the Ith
      // predecessor or from a block that dominates this predecessor.
      for (size_t i = 0; i < input_records.size(); ++i) {
        HInstruction* input = input_records[i].GetInstruction();
        HBasicBlock* predecessor = predecessors[i];
        if (!(input->GetBlock() == predecessor
              || input->GetBlock()->Dominates(predecessor))) {
          AddError(StringPrintf(
              "Input %d at index %zu of phi %d from block %d is not defined in "
              "predecessor number %zu nor in a block dominating it.",
              input->GetId(), i, phi->GetId(), phi->GetBlock()->GetBlockId(),
              i));
        }
      }
    }
  }

  // Ensure that catch phis are sorted by their vreg number, as required by
  // the register allocator and code generator. This does not apply to normal
  // phis which can be constructed artifically.
  if (phi->IsCatchPhi()) {
    HInstruction* next_phi = phi->GetNext();
    if (next_phi != nullptr && phi->GetRegNumber() > next_phi->AsPhi()->GetRegNumber()) {
      AddError(StringPrintf("Catch phis %d and %d in block %d are not sorted by their "
                            "vreg numbers.",
                            phi->GetId(),
                            next_phi->GetId(),
                            phi->GetBlock()->GetBlockId()));
    }
  }

  // Test phi equivalents. There should not be two of the same type and they should only be
  // created for constants which were untyped in DEX. Note that this test can be skipped for
  // a synthetic phi (indicated by lack of a virtual register).
  if (phi->GetRegNumber() != kNoRegNumber) {
    for (HInstructionIterator phi_it(phi->GetBlock()->GetPhis());
         !phi_it.Done();
         phi_it.Advance()) {
      HPhi* other_phi = phi_it.Current()->AsPhi();
      if (phi != other_phi && phi->GetRegNumber() == other_phi->GetRegNumber()) {
        if (phi->GetType() == other_phi->GetType()) {
          std::stringstream type_str;
          type_str << phi->GetType();
          AddError(StringPrintf("Equivalent phi (%d) found for VReg %d with type: %s.",
                                phi->GetId(),
                                phi->GetRegNumber(),
                                type_str.str().c_str()));
        } else if (phi->GetType() == DataType::Type::kReference) {
          std::stringstream type_str;
          type_str << other_phi->GetType();
          AddError(StringPrintf(
              "Equivalent non-reference phi (%d) found for VReg %d with type: %s.",
              phi->GetId(),
              phi->GetRegNumber(),
              type_str.str().c_str()));
        } else {
          // Use local allocator for allocating memory.
          ScopedArenaAllocator allocator(GetGraph()->GetArenaStack());
          // If we get here, make sure we allocate all the necessary storage at once
          // because the BitVector reallocation strategy has very bad worst-case behavior.
          ArenaBitVector visited(&allocator,
                                 GetGraph()->GetCurrentInstructionId(),
                                 /* expandable */ false,
                                 kArenaAllocGraphChecker);
          visited.ClearAllBits();
          if (!IsConstantEquivalent(phi, other_phi, &visited)) {
            AddError(StringPrintf("Two phis (%d and %d) found for VReg %d but they "
                                  "are not equivalents of constants.",
                                  phi->GetId(),
                                  other_phi->GetId(),
                                  phi->GetRegNumber()));
          }
        }
      }
    }
  }
}

void GraphChecker::HandleBooleanInput(HInstruction* instruction, size_t input_index) {
  HInstruction* input = instruction->InputAt(input_index);
  if (input->IsIntConstant()) {
    int32_t value = input->AsIntConstant()->GetValue();
    if (value != 0 && value != 1) {
      AddError(StringPrintf(
          "%s instruction %d has a non-Boolean constant input %d whose value is: %d.",
          instruction->DebugName(),
          instruction->GetId(),
          static_cast<int>(input_index),
          value));
    }
  } else if (DataType::Kind(input->GetType()) != DataType::Type::kInt32) {
    // TODO: We need a data-flow analysis to determine if an input like Phi,
    //       Select or a binary operation is actually Boolean. Allow for now.
    AddError(StringPrintf(
        "%s instruction %d has a non-integer input %d whose type is: %s.",
        instruction->DebugName(),
        instruction->GetId(),
        static_cast<int>(input_index),
        DataType::PrettyDescriptor(input->GetType())));
  }
}

void GraphChecker::VisitPackedSwitch(HPackedSwitch* instruction) {
  VisitInstruction(instruction);
  // Check that the number of block successors matches the switch count plus
  // one for the default block.
  HBasicBlock* block = instruction->GetBlock();
  if (instruction->GetNumEntries() + 1u != block->GetSuccessors().size()) {
    AddError(StringPrintf(
        "%s instruction %d in block %d expects %u successors to the block, but found: %zu.",
        instruction->DebugName(),
        instruction->GetId(),
        block->GetBlockId(),
        instruction->GetNumEntries() + 1u,
        block->GetSuccessors().size()));
  }
}

void GraphChecker::VisitIf(HIf* instruction) {
  VisitInstruction(instruction);
  HandleBooleanInput(instruction, 0);
}

void GraphChecker::VisitSelect(HSelect* instruction) {
  VisitInstruction(instruction);
  HandleBooleanInput(instruction, 2);
}

void GraphChecker::VisitBooleanNot(HBooleanNot* instruction) {
  VisitInstruction(instruction);
  HandleBooleanInput(instruction, 0);
}

void GraphChecker::VisitCondition(HCondition* op) {
  VisitInstruction(op);
  if (op->GetType() != DataType::Type::kBool) {
    AddError(StringPrintf(
        "Condition %s %d has a non-Boolean result type: %s.",
        op->DebugName(), op->GetId(),
        DataType::PrettyDescriptor(op->GetType())));
  }
  HInstruction* lhs = op->InputAt(0);
  HInstruction* rhs = op->InputAt(1);
  if (DataType::Kind(lhs->GetType()) != DataType::Kind(rhs->GetType())) {
    AddError(StringPrintf(
        "Condition %s %d has inputs of different kinds: %s, and %s.",
        op->DebugName(), op->GetId(),
        DataType::PrettyDescriptor(lhs->GetType()),
        DataType::PrettyDescriptor(rhs->GetType())));
  }
  if (!op->IsEqual() && !op->IsNotEqual()) {
    if ((lhs->GetType() == DataType::Type::kReference)) {
      AddError(StringPrintf(
          "Condition %s %d uses an object as left-hand side input.",
          op->DebugName(), op->GetId()));
    } else if (rhs->GetType() == DataType::Type::kReference) {
      AddError(StringPrintf(
          "Condition %s %d uses an object as right-hand side input.",
          op->DebugName(), op->GetId()));
    }
  }
}

void GraphChecker::VisitNeg(HNeg* instruction) {
  VisitInstruction(instruction);
  DataType::Type input_type = instruction->InputAt(0)->GetType();
  DataType::Type result_type = instruction->GetType();
  if (result_type != DataType::Kind(input_type)) {
    AddError(StringPrintf("Binary operation %s %d has a result type different "
                          "from its input kind: %s vs %s.",
                          instruction->DebugName(), instruction->GetId(),
                          DataType::PrettyDescriptor(result_type),
                          DataType::PrettyDescriptor(input_type)));
  }
}

void GraphChecker::VisitBinaryOperation(HBinaryOperation* op) {
  VisitInstruction(op);
  DataType::Type lhs_type = op->InputAt(0)->GetType();
  DataType::Type rhs_type = op->InputAt(1)->GetType();
  DataType::Type result_type = op->GetType();

  // Type consistency between inputs.
  if (op->IsUShr() || op->IsShr() || op->IsShl() || op->IsRor()) {
    if (DataType::Kind(rhs_type) != DataType::Type::kInt32) {
      AddError(StringPrintf("Shift/rotate operation %s %d has a non-int kind second input: "
                            "%s of type %s.",
                            op->DebugName(), op->GetId(),
                            op->InputAt(1)->DebugName(),
                            DataType::PrettyDescriptor(rhs_type)));
    }
  } else {
    if (DataType::Kind(lhs_type) != DataType::Kind(rhs_type)) {
      AddError(StringPrintf("Binary operation %s %d has inputs of different kinds: %s, and %s.",
                            op->DebugName(), op->GetId(),
                            DataType::PrettyDescriptor(lhs_type),
                            DataType::PrettyDescriptor(rhs_type)));
    }
  }

  // Type consistency between result and input(s).
  if (op->IsCompare()) {
    if (result_type != DataType::Type::kInt32) {
      AddError(StringPrintf("Compare operation %d has a non-int result type: %s.",
                            op->GetId(),
                            DataType::PrettyDescriptor(result_type)));
    }
  } else if (op->IsUShr() || op->IsShr() || op->IsShl() || op->IsRor()) {
    // Only check the first input (value), as the second one (distance)
    // must invariably be of kind `int`.
    if (result_type != DataType::Kind(lhs_type)) {
      AddError(StringPrintf("Shift/rotate operation %s %d has a result type different "
                            "from its left-hand side (value) input kind: %s vs %s.",
                            op->DebugName(), op->GetId(),
                            DataType::PrettyDescriptor(result_type),
                            DataType::PrettyDescriptor(lhs_type)));
    }
  } else {
    if (DataType::Kind(result_type) != DataType::Kind(lhs_type)) {
      AddError(StringPrintf("Binary operation %s %d has a result kind different "
                            "from its left-hand side input kind: %s vs %s.",
                            op->DebugName(), op->GetId(),
                            DataType::PrettyDescriptor(result_type),
                            DataType::PrettyDescriptor(lhs_type)));
    }
    if (DataType::Kind(result_type) != DataType::Kind(rhs_type)) {
      AddError(StringPrintf("Binary operation %s %d has a result kind different "
                            "from its right-hand side input kind: %s vs %s.",
                            op->DebugName(), op->GetId(),
                            DataType::PrettyDescriptor(result_type),
                            DataType::PrettyDescriptor(rhs_type)));
    }
  }
}

void GraphChecker::VisitConstant(HConstant* instruction) {
  HBasicBlock* block = instruction->GetBlock();
  if (!block->IsEntryBlock()) {
    AddError(StringPrintf(
        "%s %d should be in the entry block but is in block %d.",
        instruction->DebugName(),
        instruction->GetId(),
        block->GetBlockId()));
  }
}

void GraphChecker::VisitBoundType(HBoundType* instruction) {
  VisitInstruction(instruction);

  if (!instruction->GetUpperBound().IsValid()) {
    AddError(StringPrintf(
        "%s %d does not have a valid upper bound RTI.",
        instruction->DebugName(),
        instruction->GetId()));
  }
}

void GraphChecker::VisitTypeConversion(HTypeConversion* instruction) {
  VisitInstruction(instruction);
  DataType::Type result_type = instruction->GetResultType();
  DataType::Type input_type = instruction->GetInputType();
  // Invariant: We should never generate a conversion to a Boolean value.
  if (result_type == DataType::Type::kBool) {
    AddError(StringPrintf(
        "%s %d converts to a %s (from a %s).",
        instruction->DebugName(),
        instruction->GetId(),
        DataType::PrettyDescriptor(result_type),
        DataType::PrettyDescriptor(input_type)));
  }
}

}  // namespace art
