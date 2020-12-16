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

#include "superblock_cloner.h"

#include "common_dominator.h"
#include "graph_checker.h"

#include <iostream>

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;
using HBasicBlockSet = SuperblockCloner::HBasicBlockSet;
using HEdgeSet = SuperblockCloner::HEdgeSet;

void HEdge::Dump(std::ostream& stream) const {
  stream << "(" << from_ << "->" << to_ << ")";
}

//
// Static helper methods.
//

// Returns whether instruction has any uses (regular or environmental) outside the region,
// defined by basic block set.
static bool IsUsedOutsideRegion(const HInstruction* instr, const HBasicBlockSet& bb_set) {
  auto& uses = instr->GetUses();
  for (auto use_node = uses.begin(), e = uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  auto& env_uses = instr->GetEnvUses();
  for (auto use_node = env_uses.begin(), e = env_uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser()->GetHolder();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  return false;
}

// Returns whether the phi's inputs are the same HInstruction.
static bool ArePhiInputsTheSame(const HPhi* phi) {
  HInstruction* first_input = phi->InputAt(0);
  for (size_t i = 1, e = phi->InputCount(); i < e; i++) {
    if (phi->InputAt(i) != first_input) {
      return false;
    }
  }

  return true;
}

// Returns a common predecessor of loop1 and loop2 in the loop tree or nullptr if it is the whole
// graph.
static HLoopInformation* FindCommonLoop(HLoopInformation* loop1, HLoopInformation* loop2) {
  if (loop1 != nullptr || loop2 != nullptr) {
    return nullptr;
  }

  if (loop1->IsIn(*loop2)) {
    return loop2;
  } else if (loop2->IsIn(*loop1)) {
    return loop1;
  }
  HBasicBlock* block = CommonDominator::ForPair(loop1->GetHeader(), loop2->GetHeader());
  return block->GetLoopInformation();
}

// Calls HGraph::OrderLoopHeaderPredecessors for each loop in the graph.
static void OrderLoopsHeadersPredecessors(HGraph* graph) {
  for (HBasicBlock* block : graph->GetPostOrder()) {
    if (block->IsLoopHeader()) {
      graph->OrderLoopHeaderPredecessors(block);
    }
  }
}

//
// Helpers for CloneBasicBlock.
//

void SuperblockCloner::ReplaceInputsWithCopies(HInstruction* copy_instr) {
  DCHECK(!copy_instr->IsPhi());
  for (size_t i = 0, e = copy_instr->InputCount(); i < e; i++) {
    // Copy instruction holds the same input as the original instruction holds.
    HInstruction* orig_input = copy_instr->InputAt(i);
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      // Defined outside the subgraph.
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    // copy_instr will be registered as a user of copy_inputs after returning from this function:
    // 'copy_block->AddInstruction(copy_instr)'.
    copy_instr->SetRawInputAt(i, copy_input);
  }
}

void SuperblockCloner::DeepCloneEnvironmentWithRemapping(HInstruction* copy_instr,
                                                         const HEnvironment* orig_env) {
  if (orig_env->GetParent() != nullptr) {
    DeepCloneEnvironmentWithRemapping(copy_instr, orig_env->GetParent());
  }
  HEnvironment* copy_env = new (arena_) HEnvironment(arena_, *orig_env, copy_instr);

  for (size_t i = 0; i < orig_env->Size(); i++) {
    HInstruction* env_input = orig_env->GetInstructionAt(i);
    if (env_input != nullptr && IsInOrigBBSet(env_input->GetBlock())) {
      env_input = GetInstrCopy(env_input);
      DCHECK(env_input != nullptr && env_input->GetBlock() != nullptr);
    }
    copy_env->SetRawEnvAt(i, env_input);
    if (env_input != nullptr) {
      env_input->AddEnvUseAt(copy_env, i);
    }
  }
  // InsertRawEnvironment assumes that instruction already has an environment that's why we use
  // SetRawEnvironment in the 'else' case.
  // As this function calls itself recursively with the same copy_instr - this copy_instr may
  // have partially copied chain of HEnvironments.
  if (copy_instr->HasEnvironment()) {
    copy_instr->InsertRawEnvironment(copy_env);
  } else {
    copy_instr->SetRawEnvironment(copy_env);
  }
}

//
// Helpers for RemapEdgesSuccessors.
//

void SuperblockCloner::RemapOrigInternalOrIncomingEdge(HBasicBlock* orig_block,
                                                       HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);

  size_t this_index = orig_succ->GetPredecessorIndexOf(orig_block);
  size_t phi_input_count = 0;
  // This flag reflects whether the original successor has at least one phi and this phi
  // has been already processed in the loop. Used for validation purposes in DCHECK to check that
  // in the end all of the phis in the copy successor have the same number of inputs - the number
  // of copy successor's predecessors.
  bool first_phi_met = false;
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(this_index);
    // Remove corresponding input for original phi.
    orig_phi->RemoveInputAt(this_index);
    // Copy phi doesn't yet have either orig_block as predecessor or the input that corresponds
    // to orig_block, so add the input at the end of the list.
    copy_phi->AddInput(orig_phi_input);
    if (!first_phi_met) {
      phi_input_count = copy_phi->InputCount();
      first_phi_met = true;
    } else {
      DCHECK_EQ(phi_input_count, copy_phi->InputCount());
    }
  }
  // orig_block will be put at the end of the copy_succ's predecessors list; that corresponds
  // to the previously added phi inputs position.
  orig_block->ReplaceSuccessor(orig_succ, copy_succ);
  DCHECK(!first_phi_met || copy_succ->GetPredecessors().size() == phi_input_count);
}

void SuperblockCloner::AddCopyInternalEdge(HBasicBlock* orig_block,
                                           HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);
  copy_block->AddSuccessor(copy_succ);

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    copy_phi->AddInput(orig_phi_input);
  }
}

void SuperblockCloner::RemapCopyInternalEdge(HBasicBlock* orig_block,
                                             HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  copy_block->AddSuccessor(orig_succ);
  DCHECK(copy_block->HasSuccessor(orig_succ));

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    orig_phi->AddInput(orig_phi_input);
  }
}

//
// Local versions of CF calculation/adjustment routines.
//

// TODO: merge with the original version in nodes.cc. The concern is that we don't want to affect
// the performance of the base version by checking the local set.
// TODO: this version works when updating the back edges info for natural loop-based local_set.
// Check which exactly types of subgraphs can be analysed or rename it to
// FindBackEdgesInTheNaturalLoop.
void SuperblockCloner::FindBackEdgesLocal(HBasicBlock* entry_block, ArenaBitVector* local_set) {
  ArenaBitVector visited(arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  // "visited" must be empty on entry, it's an output argument for all visited (i.e. live) blocks.
  DCHECK_EQ(visited.GetHighestBitSet(), -1);

  // Nodes that we're currently visiting, indexed by block id.
  ArenaBitVector visiting(arena_, graph_->GetBlocks().size(), false, kArenaAllocGraphBuilder);
  // Number of successors visited from a given node, indexed by block id.
  ArenaVector<size_t> successors_visited(graph_->GetBlocks().size(),
                                         0u,
                                         arena_->Adapter(kArenaAllocGraphBuilder));
  // Stack of nodes that we're currently visiting (same as marked in "visiting" above).
  ArenaVector<HBasicBlock*> worklist(arena_->Adapter(kArenaAllocGraphBuilder));
  constexpr size_t kDefaultWorklistSize = 8;
  worklist.reserve(kDefaultWorklistSize);

  visited.SetBit(entry_block->GetBlockId());
  visiting.SetBit(entry_block->GetBlockId());
  worklist.push_back(entry_block);

  while (!worklist.empty()) {
    HBasicBlock* current = worklist.back();
    uint32_t current_id = current->GetBlockId();
    if (successors_visited[current_id] == current->GetSuccessors().size()) {
      visiting.ClearBit(current_id);
      worklist.pop_back();
    } else {
      HBasicBlock* successor = current->GetSuccessors()[successors_visited[current_id]++];
      uint32_t successor_id = successor->GetBlockId();
      if (!local_set->IsBitSet(successor_id)) {
        continue;
      }

      if (visiting.IsBitSet(successor_id)) {
        DCHECK(ContainsElement(worklist, successor));
        successor->AddBackEdgeWhileUpdating(current);
      } else if (!visited.IsBitSet(successor_id)) {
        visited.SetBit(successor_id);
        visiting.SetBit(successor_id);
        worklist.push_back(successor);
      }
    }
  }
}

void SuperblockCloner::RecalculateBackEdgesInfo(ArenaBitVector* outer_loop_bb_set) {
  // TODO: DCHECK that after the transformation the graph is connected.
  HBasicBlock* block_entry = nullptr;

  if (outer_loop_ == nullptr) {
    for (auto block : graph_->GetBlocks()) {
      if (block != nullptr) {
        outer_loop_bb_set->SetBit(block->GetBlockId());
        HLoopInformation* info = block->GetLoopInformation();
        if (info != nullptr) {
          info->ResetBasicBlockData();
        }
      }
    }
    block_entry = graph_->GetEntryBlock();
  } else {
    outer_loop_bb_set->Copy(&outer_loop_bb_set_);
    block_entry = outer_loop_->GetHeader();

    // Add newly created copy blocks.
    for (auto entry : *bb_map_) {
      outer_loop_bb_set->SetBit(entry.second->GetBlockId());
    }

    // Clear loop_info for the whole outer loop.
    for (uint32_t idx : outer_loop_bb_set->Indexes()) {
      HBasicBlock* block = GetBlockById(idx);
      HLoopInformation* info = block->GetLoopInformation();
      if (info != nullptr) {
        info->ResetBasicBlockData();
      }
    }
  }

  FindBackEdgesLocal(block_entry, outer_loop_bb_set);

  for (uint32_t idx : outer_loop_bb_set->Indexes()) {
    HBasicBlock* block = GetBlockById(idx);
    HLoopInformation* info = block->GetLoopInformation();
    // Reset LoopInformation for regular blocks and old headers which are no longer loop headers.
    if (info != nullptr &&
        (info->GetHeader() != block || info->NumberOfBackEdges() == 0)) {
      block->SetLoopInformation(nullptr);
    }
  }
}

// This is a modified version of HGraph::AnalyzeLoops.
GraphAnalysisResult SuperblockCloner::AnalyzeLoopsLocally(ArenaBitVector* outer_loop_bb_set) {
  // We iterate post order to ensure we visit inner loops before outer loops.
  // `PopulateRecursive` needs this guarantee to know whether a natural loop
  // contains an irreducible loop.
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      if (block->IsCatchBlock()) {
        // TODO: Dealing with exceptional back edges could be tricky because
        //       they only approximate the real control flow. Bail out for now.
        return kAnalysisFailThrowCatchLoop;
      }
      block->GetLoopInformation()->Populate();
    }
  }

  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      HLoopInformation* cur_loop = block->GetLoopInformation();
      HLoopInformation* outer_loop = cur_loop->GetPreHeader()->GetLoopInformation();
      if (outer_loop != nullptr) {
        outer_loop->PopulateInnerLoopUpwards(cur_loop);
      }
    }
  }

  return kAnalysisSuccess;
}

void SuperblockCloner::CleanUpControlFlow() {
  // TODO: full control flow clean up for now, optimize it.
  graph_->ClearDominanceInformation();

  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set);

  // TODO: do it locally.
  graph_->SimplifyCFG();
  graph_->ComputeDominanceInformation();

  // AnalyzeLoopsLocally requires a correct post-ordering information which was calculated just
  // before in ComputeDominanceInformation.
  GraphAnalysisResult result = AnalyzeLoopsLocally(&outer_loop_bb_set);
  DCHECK_EQ(result, kAnalysisSuccess);

  // TODO: do it locally
  OrderLoopsHeadersPredecessors(graph_);

  graph_->ComputeTryBlockInformation();
}

//
// Helpers for ResolveDataFlow
//

void SuperblockCloner::ResolvePhi(HPhi* phi) {
  HBasicBlock* phi_block = phi->GetBlock();
  for (size_t i = 0, e = phi->InputCount(); i < e; i++) {
    HInstruction* input = phi->InputAt(i);
    HBasicBlock* input_block = input->GetBlock();

    // Originally defined outside the region.
    if (!IsInOrigBBSet(input_block)) {
      continue;
    }
    HBasicBlock* corresponding_block = phi_block->GetPredecessors()[i];
    if (!IsInOrigBBSet(corresponding_block)) {
      phi->ReplaceInput(GetInstrCopy(input), i);
    }
  }
}

//
// Main algorithm methods.
//

void SuperblockCloner::SearchForSubgraphExits(ArenaVector<HBasicBlock*>* exits) {
  DCHECK(exits->empty());
  for (uint32_t block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(block_id);
    for (HBasicBlock* succ : block->GetSuccessors()) {
      if (!IsInOrigBBSet(succ)) {
        exits->push_back(succ);
      }
    }
  }
}

void SuperblockCloner::FindAndSetLocalAreaForAdjustments() {
  DCHECK(outer_loop_ == nullptr);
  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);

  // For a reducible graph we need to update back-edges and dominance information only for
  // the outermost loop which is affected by the transformation - it can be found by picking
  // the common most outer loop of loops to which the subgraph exits blocks belong.
  // Note: it can a loop or the whole graph (outer_loop_ will be nullptr in this case).
  for (HBasicBlock* exit : exits) {
    HLoopInformation* loop_exit_loop_info = exit->GetLoopInformation();
    if (loop_exit_loop_info == nullptr) {
      outer_loop_ = nullptr;
      break;
    }
    outer_loop_ = FindCommonLoop(outer_loop_, loop_exit_loop_info);
  }

  if (outer_loop_ != nullptr) {
    // Save the loop population info as it will be changed later.
    outer_loop_bb_set_.Copy(&outer_loop_->GetBlocks());
  }
}

void SuperblockCloner::RemapEdgesSuccessors() {
  // Redirect incoming edges.
  for (HEdge e : *remap_incoming_) {
    HBasicBlock* orig_block = GetBlockById(e.GetFrom());
    HBasicBlock* orig_succ = GetBlockById(e.GetTo());
    RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
  }

  // Redirect internal edges.
  for (uint32_t orig_block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* orig_block = GetBlockById(orig_block_id);

    for (HBasicBlock* orig_succ : orig_block->GetSuccessors()) {
      uint32_t orig_succ_id = orig_succ->GetBlockId();

      // Check for outgoing edge.
      if (!IsInOrigBBSet(orig_succ)) {
        HBasicBlock* copy_block = GetBlockCopy(orig_block);
        copy_block->AddSuccessor(orig_succ);
        continue;
      }

      auto orig_redir = remap_orig_internal_->Find(HEdge(orig_block_id, orig_succ_id));
      auto copy_redir = remap_copy_internal_->Find(HEdge(orig_block_id, orig_succ_id));

      // Due to construction all successors of copied block were set to original.
      if (copy_redir != remap_copy_internal_->end()) {
        RemapCopyInternalEdge(orig_block, orig_succ);
      } else {
        AddCopyInternalEdge(orig_block, orig_succ);
      }

      if (orig_redir != remap_orig_internal_->end()) {
        RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
      }
    }
  }
}

void SuperblockCloner::AdjustControlFlowInfo() {
  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set);

  graph_->ClearDominanceInformation();
  // TODO: Do it locally.
  graph_->ComputeDominanceInformation();
}

// TODO: Current FastCase restriction guarantees that instructions' inputs are already mapped to
// the valid values; only phis' inputs must be adjusted.
void SuperblockCloner::ResolveDataFlow() {
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;

    for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
      HPhi* orig_phi = it.Current()->AsPhi();
      HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
      ResolvePhi(orig_phi);
      ResolvePhi(copy_phi);
    }
    if (kIsDebugBuild) {
      // Inputs of instruction copies must be already mapped to correspondent inputs copies.
      for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
        CheckInstructionInputsRemapping(it.Current());
      }
    }
  }
}

//
// Debug and logging methods.
//

void SuperblockCloner::CheckInstructionInputsRemapping(HInstruction* orig_instr) {
  DCHECK(!orig_instr->IsPhi());
  HInstruction* copy_instr = GetInstrCopy(orig_instr);
  for (size_t i = 0, e = orig_instr->InputCount(); i < e; i++) {
    HInstruction* orig_input = orig_instr->InputAt(i);
    DCHECK(orig_input->GetBlock()->Dominates(orig_instr->GetBlock()));

    // If original input is defined outside the region then it will remain for both original
    // instruction and the copy after the transformation.
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
  }

  // Resolve environment.
  if (orig_instr->HasEnvironment()) {
    HEnvironment* orig_env = orig_instr->GetEnvironment();

    for (size_t i = 0, e = orig_env->Size(); i < e; ++i) {
      HInstruction* orig_input = orig_env->GetInstructionAt(i);

      // If original input is defined outside the region then it will remain for both original
      // instruction and the copy after the transformation.
      if (orig_input == nullptr || !IsInOrigBBSet(orig_input->GetBlock())) {
        continue;
      }

      HInstruction* copy_input = GetInstrCopy(orig_input);
      DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
    }
  }
}

//
// Public methods.
//

SuperblockCloner::SuperblockCloner(HGraph* graph,
                                   const HBasicBlockSet* orig_bb_set,
                                   HBasicBlockMap* bb_map,
                                   HInstructionMap* hir_map)
  : graph_(graph),
    arena_(graph->GetAllocator()),
    orig_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner),
    remap_orig_internal_(nullptr),
    remap_copy_internal_(nullptr),
    remap_incoming_(nullptr),
    bb_map_(bb_map),
    hir_map_(hir_map),
    outer_loop_(nullptr),
    outer_loop_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner) {
  orig_bb_set_.Copy(orig_bb_set);
}

void SuperblockCloner::SetSuccessorRemappingInfo(const HEdgeSet* remap_orig_internal,
                                                 const HEdgeSet* remap_copy_internal,
                                                 const HEdgeSet* remap_incoming) {
  remap_orig_internal_ = remap_orig_internal;
  remap_copy_internal_ = remap_copy_internal;
  remap_incoming_ = remap_incoming;
}

bool SuperblockCloner::IsSubgraphClonable() const {
  // TODO: Support irreducible graphs and graphs with try-catch.
  if (graph_->HasIrreducibleLoops() || graph_->HasTryCatch()) {
    return false;
  }

  // Check that there are no instructions defined in the subgraph and used outside.
  // TODO: Improve this by accepting graph with such uses but only one exit.
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(idx);

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->IsClonable() ||
          IsUsedOutsideRegion(instr, orig_bb_set_)) {
        return false;
      }
    }

    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->IsClonable() ||
          IsUsedOutsideRegion(instr, orig_bb_set_)) {
        return false;
      }
    }
  }

  return true;
}

void SuperblockCloner::Run() {
  DCHECK(bb_map_ != nullptr);
  DCHECK(hir_map_ != nullptr);
  DCHECK(remap_orig_internal_ != nullptr &&
         remap_copy_internal_ != nullptr &&
         remap_incoming_ != nullptr);
  DCHECK(IsSubgraphClonable());

  // Find an area in the graph for which control flow information should be adjusted.
  FindAndSetLocalAreaForAdjustments();
  // Clone the basic blocks from the orig_bb_set_; data flow is invalid after the call and is to be
  // adjusted.
  CloneBasicBlocks();
  // Connect the blocks together/remap successors and fix phis which are directly affected my the
  // remapping.
  RemapEdgesSuccessors();
  // Recalculate dominance and backedge information which is required by the next stage.
  AdjustControlFlowInfo();
  // Fix data flow of the graph.
  ResolveDataFlow();
}

void SuperblockCloner::CleanUp() {
  CleanUpControlFlow();

  // Remove phis which have all inputs being same.
  // When a block has a single predecessor it must not have any phis. However after the
  // transformation it could happen that there is such block with a phi with a single input.
  // As this is needed to be processed we also simplify phis with multiple same inputs here.
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;
    for (HInstructionIterator inst_it(orig_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        orig_block->RemovePhi(phi);
      }
    }

    HBasicBlock* copy_block = GetBlockCopy(orig_block);
    for (HInstructionIterator inst_it(copy_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        copy_block->RemovePhi(phi);
      }
    }
  }
}

HBasicBlock* SuperblockCloner::CloneBasicBlock(const HBasicBlock* orig_block) {
  HGraph* graph = orig_block->GetGraph();
  HBasicBlock* copy_block = new (arena_) HBasicBlock(graph, orig_block->GetDexPc());
  graph->AddBlock(copy_block);

  // Clone all the phis and add them to the map.
  for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    copy_block->AddPhi(copy_instr->AsPhi());
    copy_instr->AsPhi()->RemoveAllInputs();
    DCHECK(!orig_instr->HasEnvironment());
    hir_map_->Put(orig_instr, copy_instr);
  }

  // Clone all the instructions and add them to the map.
  for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    ReplaceInputsWithCopies(copy_instr);
    copy_block->AddInstruction(copy_instr);
    if (orig_instr->HasEnvironment()) {
      DeepCloneEnvironmentWithRemapping(copy_instr, orig_instr->GetEnvironment());
    }
    hir_map_->Put(orig_instr, copy_instr);
  }

  return copy_block;
}

void SuperblockCloner::CloneBasicBlocks() {
  // By this time ReversePostOrder must be valid: in 'CloneBasicBlock' inputs of the copied
  // instructions might be replaced by copies of the original inputs (depending where those inputs
  // are defined). So the definitions of the original inputs must be visited before their original
  // uses. The property of the reducible graphs "if 'A' dom 'B' then rpo_num('A') >= rpo_num('B')"
  // guarantees that.
  for (HBasicBlock* orig_block : graph_->GetReversePostOrder()) {
    if (!IsInOrigBBSet(orig_block)) {
      continue;
    }
    HBasicBlock* copy_block = CloneBasicBlock(orig_block);
    bb_map_->Put(orig_block, copy_block);
    if (kSuperblockClonerLogging) {
      std::cout << "new block :" << copy_block->GetBlockId() << ": " << orig_block->GetBlockId() <<
                   std::endl;
    }
  }
}

}  // namespace art
