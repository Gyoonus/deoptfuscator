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

#ifndef ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_
#define ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_

#include "base/arena_bit_vector.h"
#include "base/arena_containers.h"
#include "base/bit_vector-inl.h"
#include "nodes.h"

namespace art {

static const bool kSuperblockClonerLogging = false;
static const bool kSuperblockClonerVerify = false;

// Represents an edge between two HBasicBlocks.
//
// Note: objects of this class are small - pass them by value.
class HEdge : public ArenaObject<kArenaAllocSuperblockCloner> {
 public:
  HEdge(HBasicBlock* from, HBasicBlock* to) : from_(from->GetBlockId()), to_(to->GetBlockId()) {
    DCHECK_NE(to_, kInvalidBlockId);
    DCHECK_NE(from_, kInvalidBlockId);
  }
  HEdge(uint32_t from, uint32_t to) : from_(from), to_(to) {
    DCHECK_NE(to_, kInvalidBlockId);
    DCHECK_NE(from_, kInvalidBlockId);
  }
  HEdge() : from_(kInvalidBlockId), to_(kInvalidBlockId) {}

  uint32_t GetFrom() const { return from_; }
  uint32_t GetTo() const { return to_; }

  bool operator==(const HEdge& other) const {
    return this->from_ == other.from_ && this->to_ == other.to_;
  }

  bool operator!=(const HEdge& other) const { return !operator==(other); }
  void Dump(std::ostream& stream) const;

  // Returns whether an edge represents a valid edge in CF graph: whether the from_ block
  // has to_ block as a successor.
  bool IsValid() const { return from_ != kInvalidBlockId && to_ != kInvalidBlockId; }

 private:
  // Predecessor block id.
  uint32_t from_;
  // Successor block id.
  uint32_t to_;
};

// Returns whether a HEdge edge corresponds to an existing edge in the graph.
inline bool IsEdgeValid(HEdge edge, HGraph* graph) {
  if (!edge.IsValid()) {
    return false;
  }
  uint32_t from = edge.GetFrom();
  uint32_t to = edge.GetTo();
  if (from >= graph->GetBlocks().size() || to >= graph->GetBlocks().size()) {
    return false;
  }

  HBasicBlock* block_from = graph->GetBlocks()[from];
  HBasicBlock* block_to = graph->GetBlocks()[to];
  if (block_from == nullptr || block_to == nullptr) {
    return false;
  }

  return block_from->HasSuccessor(block_to, 0);
}

// SuperblockCloner provides a feature of cloning subgraphs in a smart, high level way without
// fine grain manipulation with IR; data flow and graph properties are resolved/adjusted
// automatically. The clone transformation is defined by specifying a set of basic blocks to copy
// and a set of rules how to treat edges, remap their successors. By using this approach such
// optimizations as Branch Target Expansion, Loop Peeling, Loop Unrolling can be implemented.
//
// The idea of the transformation is based on "Superblock cloning" technique described in the book
// "Engineering a Compiler. Second Edition", Keith D. Cooper, Linda Torczon, Rice University
// Houston, Texas. 2nd edition, Morgan Kaufmann. The original paper is "The Superblock: An Efective
// Technique for VLIW and Superscalar Compilation" by Hwu, W.M.W., Mahlke, S.A., Chen, W.Y. et al.
// J Supercomput (1993) 7: 229. doi:10.1007/BF01205185.
//
// There are two states of the IR graph: original graph (before the transformation) and
// copy graph (after).
//
// Before the transformation:
// Defining a set of basic block to copy (orig_bb_set) partitions all of the edges in the original
// graph into 4 categories/sets (use the following notation for edges: "(pred, succ)",
// where pred, succ - basic blocks):
//  - internal - pred, succ are members of ‘orig_bb_set’.
//  - outside  - pred, succ are not members of ‘orig_bb_set’.
//  - incoming - pred is not a member of ‘orig_bb_set’, succ is.
//  - outgoing - pred is a member of ‘orig_bb_set’, succ is not.
//
// Transformation:
//
// 1. Initial cloning:
//   1.1. For each ‘orig_block’ in orig_bb_set create a copy ‘copy_block’; these new blocks
//        form ‘copy_bb_set’.
//   1.2. For each edge (X, Y) from internal set create an edge (X_1, Y_1) where X_1, Y_1 are the
//        copies of X, Y basic blocks correspondingly; these new edges form ‘copy_internal’ edge
//        set.
//   1.3. For each edge (X, Y) from outgoing set create an edge (X_1, Y_1) where X_1, Y_1 are the
//        copies of X, Y basic blocks correspondingly; these new edges form ‘copy_outgoing’ edge
//        set.
// 2. Successors remapping.
//   2.1. 'remap_orig_internal’ - set of edges (X, Y) from ‘orig_bb_set’ whose successors should
//        be remapped to copy nodes: ((X, Y) will be transformed into (X, Y_1)).
//   2.2. ‘remap_copy_internal’ - set of edges (X_1, Y_1) from ‘copy_bb_set’ whose successors
//        should be remapped to copy nodes: (X_1, Y_1) will be transformed into (X_1, Y)).
//   2.3. 'remap_incoming’ - set of edges (X, Y) from the ‘incoming’ edge set in the original graph
//        whose successors should be remapped to copies nodes: ((X, Y) will be transformed into
//        (X, Y_1)).
// 3. Adjust control flow structures and relations (dominance, reverse post order, loops, etc).
// 4. Fix/resolve data flow.
// 5. Do cleanups (DCE, critical edges splitting, etc).
//
class SuperblockCloner : public ValueObject {
 public:
  // TODO: Investigate optimal types for the containers.
  using HBasicBlockMap = ArenaSafeMap<HBasicBlock*, HBasicBlock*>;
  using HInstructionMap = ArenaSafeMap<HInstruction*, HInstruction*>;
  using HBasicBlockSet = ArenaBitVector;
  using HEdgeSet = ArenaHashSet<HEdge>;

  SuperblockCloner(HGraph* graph,
                   const HBasicBlockSet* orig_bb_set,
                   HBasicBlockMap* bb_map,
                   HInstructionMap* hir_map);

  // Sets edge successor remapping info specified by corresponding edge sets.
  void SetSuccessorRemappingInfo(const HEdgeSet* remap_orig_internal,
                                 const HEdgeSet* remap_copy_internal,
                                 const HEdgeSet* remap_incoming);

  // Returns whether the specified subgraph is copyable.
  // TODO: Start from small range of graph patterns then extend it.
  bool IsSubgraphClonable() const;

  // Runs the copy algorithm according to the description.
  void Run();

  // Cleans up the graph after transformation: splits critical edges, recalculates control flow
  // information (back-edges, dominators, loop info, etc), eliminates redundant phis.
  void CleanUp();

  // Returns a clone of a basic block (orig_block).
  //
  //  - The copy block will have no successors/predecessors; they should be set up manually.
  //  - For each instruction in the orig_block a copy is created and inserted into the copy block;
  //    this correspondence is recorded in the map (old instruction, new instruction).
  //  - Graph HIR is not valid after this transformation: all of the HIRs have their inputs the
  //    same, as in the original block, PHIs do not reflect a correct correspondence between the
  //    value and predecessors (as the copy block has no predecessors by now), etc.
  HBasicBlock* CloneBasicBlock(const HBasicBlock* orig_block);

  // Creates a clone for each basic blocks in orig_bb_set adding corresponding entries into bb_map_
  // and hir_map_.
  void CloneBasicBlocks();

  HInstruction* GetInstrCopy(HInstruction* orig_instr) const {
    auto copy_input_iter = hir_map_->find(orig_instr);
    DCHECK(copy_input_iter != hir_map_->end());
    return copy_input_iter->second;
  }

  HBasicBlock* GetBlockCopy(HBasicBlock* orig_block) const {
    HBasicBlock* block = bb_map_->Get(orig_block);
    DCHECK(block != nullptr);
    return block;
  }

  HInstruction* GetInstrOrig(HInstruction* copy_instr) const {
    for (auto it : *hir_map_) {
      if (it.second == copy_instr) {
        return it.first;
      }
    }
    return nullptr;
  }

  bool IsInOrigBBSet(uint32_t block_id) const {
    return orig_bb_set_.IsBitSet(block_id);
  }

  bool IsInOrigBBSet(const HBasicBlock* block) const {
    return IsInOrigBBSet(block->GetBlockId());
  }

 private:
  // Fills the 'exits' vector with the subgraph exits.
  void SearchForSubgraphExits(ArenaVector<HBasicBlock*>* exits);

  // Finds and records information about the area in the graph for which control-flow (back edges,
  // loops, dominators) needs to be adjusted.
  void FindAndSetLocalAreaForAdjustments();

  // Remaps edges' successors according to the info specified in the edges sets.
  //
  // Only edge successors/predecessors and phis' input records (to have a correspondence between
  // a phi input record (not value) and a block's predecessor) are adjusted at this stage: neither
  // phis' nor instructions' inputs values are resolved.
  void RemapEdgesSuccessors();

  // Adjusts control-flow (back edges, loops, dominators) for the local area defined by
  // FindAndSetLocalAreaForAdjustments.
  void AdjustControlFlowInfo();

  // Resolves Data Flow - adjusts phis' and instructions' inputs in order to have a valid graph in
  // the SSA form.
  void ResolveDataFlow();

  //
  // Helpers for CloneBasicBlock.
  //

  // Adjusts copy instruction's inputs: if the input of the original instruction is defined in the
  // orig_bb_set, replaces it with a corresponding copy otherwise leaves it the same as original.
  void ReplaceInputsWithCopies(HInstruction* copy_instr);

  // Recursively clones the environment for the copy instruction. If the input of the original
  // environment is defined in the orig_bb_set, replaces it with a corresponding copy otherwise
  // leaves it the same as original.
  void DeepCloneEnvironmentWithRemapping(HInstruction* copy_instr, const HEnvironment* orig_env);

  //
  // Helpers for RemapEdgesSuccessors.
  //

  // Remaps incoming or original internal edge to its copy, adjusts the phi inputs in orig_succ and
  // copy_succ.
  void RemapOrigInternalOrIncomingEdge(HBasicBlock* orig_block, HBasicBlock* orig_succ);

  // Adds copy internal edge (from copy_block to copy_succ), updates phis in the copy_succ.
  void AddCopyInternalEdge(HBasicBlock* orig_block, HBasicBlock* orig_succ);

  // Remaps copy internal edge to its origin, adjusts the phi inputs in orig_succ.
  void RemapCopyInternalEdge(HBasicBlock* orig_block, HBasicBlock* orig_succ);

  //
  // Local versions of control flow calculation/adjustment routines.
  //

  void FindBackEdgesLocal(HBasicBlock* entry_block, ArenaBitVector* local_set);
  void RecalculateBackEdgesInfo(ArenaBitVector* outer_loop_bb_set);
  GraphAnalysisResult AnalyzeLoopsLocally(ArenaBitVector* outer_loop_bb_set);
  void CleanUpControlFlow();

  //
  // Helpers for ResolveDataFlow
  //

  // Resolves the inputs of the phi.
  void ResolvePhi(HPhi* phi);

  //
  // Debug and logging methods.
  //
  void CheckInstructionInputsRemapping(HInstruction* orig_instr);

  HBasicBlock* GetBlockById(uint32_t block_id) const {
    DCHECK(block_id < graph_->GetBlocks().size());
    HBasicBlock* block = graph_->GetBlocks()[block_id];
    DCHECK(block != nullptr);
    return block;
  }

  HGraph* const graph_;
  ArenaAllocator* const arena_;

  // Set of basic block in the original graph to be copied.
  HBasicBlockSet orig_bb_set_;

  // Sets of edges which require successors remapping.
  const HEdgeSet* remap_orig_internal_;
  const HEdgeSet* remap_copy_internal_;
  const HEdgeSet* remap_incoming_;

  // Correspondence map for blocks: (original block, copy block).
  HBasicBlockMap* bb_map_;
  // Correspondence map for instructions: (original HInstruction, copy HInstruction).
  HInstructionMap* hir_map_;
  // Area in the graph for which control-flow (back edges, loops, dominators) needs to be adjusted.
  HLoopInformation* outer_loop_;
  HBasicBlockSet outer_loop_bb_set_;

  ART_FRIEND_TEST(SuperblockClonerTest, AdjustControlFlowInfo);

  DISALLOW_COPY_AND_ASSIGN(SuperblockCloner);
};

}  // namespace art

namespace std {

template <>
struct hash<art::HEdge> {
  size_t operator()(art::HEdge const& x) const noexcept  {
    // Use Cantor pairing function as the hash function.
    uint32_t a = x.GetFrom();
    uint32_t b = x.GetTo();
    return (a + b) * (a + b + 1) / 2 + b;
  }
};

}  // namespace std

#endif  // ART_COMPILER_OPTIMIZING_SUPERBLOCK_CLONER_H_
