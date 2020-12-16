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

#ifndef ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_
#define ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "dex/code_item_accessors.h"
#include "dex/dex_file.h"
#include "nodes.h"

namespace art {

class HBasicBlockBuilder : public ValueObject {
 public:
  HBasicBlockBuilder(HGraph* graph,
                     const DexFile* const dex_file,
                     const CodeItemDebugInfoAccessor& accessor,
                     ScopedArenaAllocator* local_allocator);

  // Creates basic blocks in `graph_` at branch target dex_pc positions of the
  // `code_item_`. Blocks are connected but left unpopulated with instructions.
  // TryBoundary blocks are inserted at positions where control-flow enters/
  // exits a try block.
  bool Build();

  // Creates basic blocks in `graph_` for compiling an intrinsic.
  void BuildIntrinsic();

  size_t GetNumberOfBranches() const { return number_of_branches_; }
  HBasicBlock* GetBlockAt(uint32_t dex_pc) const { return branch_targets_[dex_pc]; }

  size_t GetQuickenIndex(uint32_t dex_pc) const;

 private:
  // Creates a basic block starting at given `dex_pc`.
  HBasicBlock* MaybeCreateBlockAt(uint32_t dex_pc);

  // Creates a basic block for bytecode instructions at `semantic_dex_pc` and
  // stores it under the `store_dex_pc` key. This is used when multiple blocks
  // share the same semantic dex_pc, e.g. when building switch decision trees.
  HBasicBlock* MaybeCreateBlockAt(uint32_t semantic_dex_pc, uint32_t store_dex_pc);

  bool CreateBranchTargets();
  void ConnectBasicBlocks();
  void InsertTryBoundaryBlocks();

  // Helper method which decides whether `catch_block` may have live normal
  // predecessors and thus whether a synthetic catch block needs to be created
  // to avoid mixing normal and exceptional predecessors.
  // Should only be called during InsertTryBoundaryBlocks on blocks at catch
  // handler dex_pcs.
  bool MightHaveLiveNormalPredecessors(HBasicBlock* catch_block);

  ArenaAllocator* const allocator_;
  HGraph* const graph_;

  const DexFile* const dex_file_;
  CodeItemDataAccessor code_item_accessor_;  // null code item for intrinsic graph.

  ScopedArenaAllocator* const local_allocator_;
  ScopedArenaVector<HBasicBlock*> branch_targets_;
  ScopedArenaVector<HBasicBlock*> throwing_blocks_;
  size_t number_of_branches_;

  // A table to quickly find the quicken index for the first instruction of a basic block.
  ScopedArenaSafeMap<uint32_t, uint32_t> quicken_index_for_dex_pc_;

  static constexpr size_t kDefaultNumberOfThrowingBlocks = 2u;

  DISALLOW_COPY_AND_ASSIGN(HBasicBlockBuilder);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_BLOCK_BUILDER_H_
