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

#include "block_builder.h"

#include "base/logging.h"  // FOR VLOG.
#include "dex/bytecode_utils.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file_exception_helpers.h"
#include "quicken_info.h"

namespace art {

HBasicBlockBuilder::HBasicBlockBuilder(HGraph* graph,
                                       const DexFile* const dex_file,
                                       const CodeItemDebugInfoAccessor& accessor,
                                       ScopedArenaAllocator* local_allocator)
    : allocator_(graph->GetAllocator()),
      graph_(graph),
      dex_file_(dex_file),
      code_item_accessor_(accessor),
      local_allocator_(local_allocator),
      branch_targets_(code_item_accessor_.HasCodeItem()
                          ? code_item_accessor_.InsnsSizeInCodeUnits()
                          : /* fake dex_pc=0 for intrinsic graph */ 1u,
                      nullptr,
                      local_allocator->Adapter(kArenaAllocGraphBuilder)),
      throwing_blocks_(kDefaultNumberOfThrowingBlocks,
                       local_allocator->Adapter(kArenaAllocGraphBuilder)),
      number_of_branches_(0u),
      quicken_index_for_dex_pc_(std::less<uint32_t>(),
                                local_allocator->Adapter(kArenaAllocGraphBuilder)) {}

HBasicBlock* HBasicBlockBuilder::MaybeCreateBlockAt(uint32_t dex_pc) {
  return MaybeCreateBlockAt(dex_pc, dex_pc);
}

HBasicBlock* HBasicBlockBuilder::MaybeCreateBlockAt(uint32_t semantic_dex_pc,
                                                    uint32_t store_dex_pc) {
  HBasicBlock* block = branch_targets_[store_dex_pc];
  if (block == nullptr) {
    block = new (allocator_) HBasicBlock(graph_, semantic_dex_pc);
    branch_targets_[store_dex_pc] = block;
  }
  DCHECK_EQ(block->GetDexPc(), semantic_dex_pc);
  return block;
}

bool HBasicBlockBuilder::CreateBranchTargets() {
  // Create the first block for the dex instructions, single successor of the entry block.
  MaybeCreateBlockAt(0u);

  if (code_item_accessor_.TriesSize() != 0) {
    // Create branch targets at the start/end of the TryItem range. These are
    // places where the program might fall through into/out of the a block and
    // where TryBoundary instructions will be inserted later. Other edges which
    // enter/exit the try blocks are a result of branches/switches.
    for (const DexFile::TryItem& try_item : code_item_accessor_.TryItems()) {
      uint32_t dex_pc_start = try_item.start_addr_;
      uint32_t dex_pc_end = dex_pc_start + try_item.insn_count_;
      MaybeCreateBlockAt(dex_pc_start);
      if (dex_pc_end < code_item_accessor_.InsnsSizeInCodeUnits()) {
        // TODO: Do not create block if the last instruction cannot fall through.
        MaybeCreateBlockAt(dex_pc_end);
      } else if (dex_pc_end == code_item_accessor_.InsnsSizeInCodeUnits()) {
        // The TryItem spans until the very end of the CodeItem and therefore
        // cannot have any code afterwards.
      } else {
        // The TryItem spans beyond the end of the CodeItem. This is invalid code.
        VLOG(compiler) << "Not compiled: TryItem spans beyond the end of the CodeItem";
        return false;
      }
    }

    // Create branch targets for exception handlers.
    const uint8_t* handlers_ptr = code_item_accessor_.GetCatchHandlerData();
    uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_ptr);
    for (uint32_t idx = 0; idx < handlers_size; ++idx) {
      CatchHandlerIterator iterator(handlers_ptr);
      for (; iterator.HasNext(); iterator.Next()) {
        MaybeCreateBlockAt(iterator.GetHandlerAddress());
      }
      handlers_ptr = iterator.EndDataPointer();
    }
  }

  // Iterate over all instructions and find branching instructions. Create blocks for
  // the locations these instructions branch to.
  for (const DexInstructionPcPair& pair : code_item_accessor_) {
    const uint32_t dex_pc = pair.DexPc();
    const Instruction& instruction = pair.Inst();

    if (instruction.IsBranch()) {
      number_of_branches_++;
      MaybeCreateBlockAt(dex_pc + instruction.GetTargetOffset());
    } else if (instruction.IsSwitch()) {
      number_of_branches_++;  // count as at least one branch (b/77652521)
      DexSwitchTable table(instruction, dex_pc);
      for (DexSwitchTableIterator s_it(table); !s_it.Done(); s_it.Advance()) {
        MaybeCreateBlockAt(dex_pc + s_it.CurrentTargetOffset());

        // Create N-1 blocks where we will insert comparisons of the input value
        // against the Switch's case keys.
        if (table.ShouldBuildDecisionTree() && !s_it.IsLast()) {
          // Store the block under dex_pc of the current key at the switch data
          // instruction for uniqueness but give it the dex_pc of the SWITCH
          // instruction which it semantically belongs to.
          MaybeCreateBlockAt(dex_pc, s_it.GetDexPcForCurrentIndex());
        }
      }
    } else if (instruction.Opcode() == Instruction::MOVE_EXCEPTION) {
      // End the basic block after MOVE_EXCEPTION. This simplifies the later
      // stage of TryBoundary-block insertion.
    } else {
      continue;
    }

    if (instruction.CanFlowThrough()) {
      DexInstructionIterator next(std::next(DexInstructionIterator(pair)));
      if (next == code_item_accessor_.end()) {
        // In the normal case we should never hit this but someone can artificially forge a dex
        // file to fall-through out the method code. In this case we bail out compilation.
        VLOG(compiler) << "Not compiled: Fall-through beyond the CodeItem";
        return false;
      }
      MaybeCreateBlockAt(next.DexPc());
    }
  }

  return true;
}

void HBasicBlockBuilder::ConnectBasicBlocks() {
  HBasicBlock* block = graph_->GetEntryBlock();
  graph_->AddBlock(block);

  size_t quicken_index = 0;
  bool is_throwing_block = false;
  // Calculate the qucikening index here instead of CreateBranchTargets since it's easier to
  // calculate in dex_pc order.
  for (const DexInstructionPcPair& pair : code_item_accessor_) {
    const uint32_t dex_pc = pair.DexPc();
    const Instruction& instruction = pair.Inst();

    // Check if this dex_pc address starts a new basic block.
    HBasicBlock* next_block = GetBlockAt(dex_pc);
    if (next_block != nullptr) {
      // We only need quicken index entries for basic block boundaries.
      quicken_index_for_dex_pc_.Put(dex_pc, quicken_index);
      if (block != nullptr) {
        // Last instruction did not end its basic block but a new one starts here.
        // It must have been a block falling through into the next one.
        block->AddSuccessor(next_block);
      }
      block = next_block;
      is_throwing_block = false;
      graph_->AddBlock(block);
    }
    // Make sure to increment this before the continues.
    if (QuickenInfoTable::NeedsIndexForInstruction(&instruction)) {
      ++quicken_index;
    }

    if (block == nullptr) {
      // Ignore dead code.
      continue;
    }

    if (!is_throwing_block && IsThrowingDexInstruction(instruction)) {
      DCHECK(!ContainsElement(throwing_blocks_, block));
      is_throwing_block = true;
      throwing_blocks_.push_back(block);
    }

    if (instruction.IsBranch()) {
      uint32_t target_dex_pc = dex_pc + instruction.GetTargetOffset();
      block->AddSuccessor(GetBlockAt(target_dex_pc));
    } else if (instruction.IsReturn() || (instruction.Opcode() == Instruction::THROW)) {
      block->AddSuccessor(graph_->GetExitBlock());
    } else if (instruction.IsSwitch()) {
      DexSwitchTable table(instruction, dex_pc);
      for (DexSwitchTableIterator s_it(table); !s_it.Done(); s_it.Advance()) {
        uint32_t target_dex_pc = dex_pc + s_it.CurrentTargetOffset();
        block->AddSuccessor(GetBlockAt(target_dex_pc));

        if (table.ShouldBuildDecisionTree() && !s_it.IsLast()) {
          uint32_t next_case_dex_pc = s_it.GetDexPcForCurrentIndex();
          HBasicBlock* next_case_block = GetBlockAt(next_case_dex_pc);
          block->AddSuccessor(next_case_block);
          block = next_case_block;
          graph_->AddBlock(block);
        }
      }
    } else {
      // Remaining code only applies to instructions which end their basic block.
      continue;
    }

    // Go to the next instruction in case we read dex PC below.
    if (instruction.CanFlowThrough()) {
      block->AddSuccessor(GetBlockAt(std::next(DexInstructionIterator(pair)).DexPc()));
    }

    // The basic block ends here. Do not add any more instructions.
    block = nullptr;
  }

  graph_->AddBlock(graph_->GetExitBlock());
}

// Returns the TryItem stored for `block` or nullptr if there is no info for it.
static const DexFile::TryItem* GetTryItem(
    HBasicBlock* block,
    const ScopedArenaSafeMap<uint32_t, const DexFile::TryItem*>& try_block_info) {
  auto iterator = try_block_info.find(block->GetBlockId());
  return (iterator == try_block_info.end()) ? nullptr : iterator->second;
}

// Iterates over the exception handlers of `try_item`, finds the corresponding
// catch blocks and makes them successors of `try_boundary`. The order of
// successors matches the order in which runtime exception delivery searches
// for a handler.
static void LinkToCatchBlocks(HTryBoundary* try_boundary,
                              const CodeItemDataAccessor& accessor,
                              const DexFile::TryItem* try_item,
                              const ScopedArenaSafeMap<uint32_t, HBasicBlock*>& catch_blocks) {
  for (CatchHandlerIterator it(accessor.GetCatchHandlerData(try_item->handler_off_));
      it.HasNext();
      it.Next()) {
    try_boundary->AddExceptionHandler(catch_blocks.Get(it.GetHandlerAddress()));
  }
}

bool HBasicBlockBuilder::MightHaveLiveNormalPredecessors(HBasicBlock* catch_block) {
  if (kIsDebugBuild) {
    DCHECK_NE(catch_block->GetDexPc(), kNoDexPc) << "Should not be called on synthetic blocks";
    DCHECK(!graph_->GetEntryBlock()->GetSuccessors().empty())
        << "Basic blocks must have been created and connected";
    for (HBasicBlock* predecessor : catch_block->GetPredecessors()) {
      DCHECK(!predecessor->IsSingleTryBoundary())
          << "TryBoundary blocks must not have not been created yet";
    }
  }

  const Instruction& first = code_item_accessor_.InstructionAt(catch_block->GetDexPc());
  if (first.Opcode() == Instruction::MOVE_EXCEPTION) {
    // Verifier guarantees that if a catch block begins with MOVE_EXCEPTION then
    // it has no live normal predecessors.
    return false;
  } else if (catch_block->GetPredecessors().empty()) {
    // Normal control-flow edges have already been created. Since block's list of
    // predecessors is empty, it cannot have any live or dead normal predecessors.
    return false;
  }

  // The catch block has normal predecessors but we do not know which are live
  // and which will be removed during the initial DCE. Return `true` to signal
  // that it may have live normal predecessors.
  return true;
}

void HBasicBlockBuilder::InsertTryBoundaryBlocks() {
  if (code_item_accessor_.TriesSize() == 0) {
    return;
  }

  // Keep a map of all try blocks and their respective TryItems. We do not use
  // the block's pointer but rather its id to ensure deterministic iteration.
  ScopedArenaSafeMap<uint32_t, const DexFile::TryItem*> try_block_info(
      std::less<uint32_t>(), local_allocator_->Adapter(kArenaAllocGraphBuilder));

  // Obtain TryItem information for blocks with throwing instructions, and split
  // blocks which are both try & catch to simplify the graph.
  for (HBasicBlock* block : graph_->GetBlocks()) {
    if (block->GetDexPc() == kNoDexPc) {
      continue;
    }

    // Do not bother creating exceptional edges for try blocks which have no
    // throwing instructions. In that case we simply assume that the block is
    // not covered by a TryItem. This prevents us from creating a throw-catch
    // loop for synchronized blocks.
    if (ContainsElement(throwing_blocks_, block)) {
      // Try to find a TryItem covering the block.
      const DexFile::TryItem* try_item = code_item_accessor_.FindTryItem(block->GetDexPc());
      if (try_item != nullptr) {
        // Block throwing and in a TryItem. Store the try block information.
        try_block_info.Put(block->GetBlockId(), try_item);
      }
    }
  }

  // Map from a handler dex_pc to the corresponding catch block.
  ScopedArenaSafeMap<uint32_t, HBasicBlock*> catch_blocks(
      std::less<uint32_t>(), local_allocator_->Adapter(kArenaAllocGraphBuilder));

  // Iterate over catch blocks, create artifical landing pads if necessary to
  // simplify the CFG, and set metadata.
  const uint8_t* handlers_ptr = code_item_accessor_.GetCatchHandlerData();
  uint32_t handlers_size = DecodeUnsignedLeb128(&handlers_ptr);
  for (uint32_t idx = 0; idx < handlers_size; ++idx) {
    CatchHandlerIterator iterator(handlers_ptr);
    for (; iterator.HasNext(); iterator.Next()) {
      uint32_t address = iterator.GetHandlerAddress();
      if (catch_blocks.find(address) != catch_blocks.end()) {
        // Catch block already processed.
        continue;
      }

      // Check if we should create an artifical landing pad for the catch block.
      // We create one if the catch block is also a try block because we do not
      // have a strategy for inserting TryBoundaries on exceptional edges.
      // We also create one if the block might have normal predecessors so as to
      // simplify register allocation.
      HBasicBlock* catch_block = GetBlockAt(address);
      bool is_try_block = (try_block_info.find(catch_block->GetBlockId()) != try_block_info.end());
      if (is_try_block || MightHaveLiveNormalPredecessors(catch_block)) {
        HBasicBlock* new_catch_block = new (allocator_) HBasicBlock(graph_, address);
        new_catch_block->AddInstruction(new (allocator_) HGoto(address));
        new_catch_block->AddSuccessor(catch_block);
        graph_->AddBlock(new_catch_block);
        catch_block = new_catch_block;
      }

      catch_blocks.Put(address, catch_block);
      catch_block->SetTryCatchInformation(
        new (allocator_) TryCatchInformation(iterator.GetHandlerTypeIndex(), *dex_file_));
    }
    handlers_ptr = iterator.EndDataPointer();
  }

  // Do a pass over the try blocks and insert entering TryBoundaries where at
  // least one predecessor is not covered by the same TryItem as the try block.
  // We do not split each edge separately, but rather create one boundary block
  // that all predecessors are relinked to. This preserves loop headers (b/23895756).
  for (const auto& entry : try_block_info) {
    uint32_t block_id = entry.first;
    const DexFile::TryItem* try_item = entry.second;
    HBasicBlock* try_block = graph_->GetBlocks()[block_id];
    for (HBasicBlock* predecessor : try_block->GetPredecessors()) {
      if (GetTryItem(predecessor, try_block_info) != try_item) {
        // Found a predecessor not covered by the same TryItem. Insert entering
        // boundary block.
        HTryBoundary* try_entry = new (allocator_) HTryBoundary(
            HTryBoundary::BoundaryKind::kEntry, try_block->GetDexPc());
        try_block->CreateImmediateDominator()->AddInstruction(try_entry);
        LinkToCatchBlocks(try_entry, code_item_accessor_, try_item, catch_blocks);
        break;
      }
    }
  }

  // Do a second pass over the try blocks and insert exit TryBoundaries where
  // the successor is not in the same TryItem.
  for (const auto& entry : try_block_info) {
    uint32_t block_id = entry.first;
    const DexFile::TryItem* try_item = entry.second;
    HBasicBlock* try_block = graph_->GetBlocks()[block_id];
    // NOTE: Do not use iterators because SplitEdge would invalidate them.
    for (size_t i = 0, e = try_block->GetSuccessors().size(); i < e; ++i) {
      HBasicBlock* successor = try_block->GetSuccessors()[i];

      // If the successor is a try block, all of its predecessors must be
      // covered by the same TryItem. Otherwise the previous pass would have
      // created a non-throwing boundary block.
      if (GetTryItem(successor, try_block_info) != nullptr) {
        DCHECK_EQ(try_item, GetTryItem(successor, try_block_info));
        continue;
      }

      // Insert TryBoundary and link to catch blocks.
      HTryBoundary* try_exit =
          new (allocator_) HTryBoundary(HTryBoundary::BoundaryKind::kExit, successor->GetDexPc());
      graph_->SplitEdge(try_block, successor)->AddInstruction(try_exit);
      LinkToCatchBlocks(try_exit, code_item_accessor_, try_item, catch_blocks);
    }
  }
}

bool HBasicBlockBuilder::Build() {
  DCHECK(code_item_accessor_.HasCodeItem());
  DCHECK(graph_->GetBlocks().empty());

  graph_->SetEntryBlock(new (allocator_) HBasicBlock(graph_, kNoDexPc));
  graph_->SetExitBlock(new (allocator_) HBasicBlock(graph_, kNoDexPc));

  // TODO(dbrazdil): Do CreateBranchTargets and ConnectBasicBlocks in one pass.
  if (!CreateBranchTargets()) {
    return false;
  }

  ConnectBasicBlocks();
  InsertTryBoundaryBlocks();

  return true;
}

void HBasicBlockBuilder::BuildIntrinsic() {
  DCHECK(!code_item_accessor_.HasCodeItem());
  DCHECK(graph_->GetBlocks().empty());

  // Create blocks.
  HBasicBlock* entry_block = new (allocator_) HBasicBlock(graph_, kNoDexPc);
  HBasicBlock* exit_block = new (allocator_) HBasicBlock(graph_, kNoDexPc);
  HBasicBlock* body = MaybeCreateBlockAt(/* semantic_dex_pc */ kNoDexPc, /* store_dex_pc */ 0u);

  // Add blocks to the graph.
  graph_->AddBlock(entry_block);
  graph_->AddBlock(body);
  graph_->AddBlock(exit_block);
  graph_->SetEntryBlock(entry_block);
  graph_->SetExitBlock(exit_block);

  // Connect blocks.
  entry_block->AddSuccessor(body);
  body->AddSuccessor(exit_block);
}

size_t HBasicBlockBuilder::GetQuickenIndex(uint32_t dex_pc) const {
  return quicken_index_for_dex_pc_.Get(dex_pc);
}

}  // namespace art
