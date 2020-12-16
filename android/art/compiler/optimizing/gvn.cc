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

#include "gvn.h"

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/utils.h"
#include "side_effects_analysis.h"

namespace art {

/**
 * A ValueSet holds instructions that can replace other instructions. It is updated
 * through the `Add` method, and the `Kill` method. The `Kill` method removes
 * instructions that are affected by the given side effect.
 *
 * The `Lookup` method returns an equivalent instruction to the given instruction
 * if there is one in the set. In GVN, we would say those instructions have the
 * same "number".
 */
class ValueSet : public ArenaObject<kArenaAllocGvn> {
 public:
  // Constructs an empty ValueSet which owns all its buckets.
  explicit ValueSet(ScopedArenaAllocator* allocator)
      : allocator_(allocator),
        num_buckets_(kMinimumNumberOfBuckets),
        buckets_(allocator->AllocArray<Node*>(num_buckets_, kArenaAllocGvn)),
        buckets_owned_(allocator, num_buckets_, false, kArenaAllocGvn),
        num_entries_(0u) {
    // ArenaAllocator returns zeroed memory, so no need to set buckets to null.
    DCHECK(IsPowerOfTwo(num_buckets_));
    std::fill_n(buckets_, num_buckets_, nullptr);
    buckets_owned_.SetInitialBits(num_buckets_);
  }

  // Copy constructor. Depending on the load factor, it will either make a deep
  // copy (all buckets owned) or a shallow one (buckets pointing to the parent).
  ValueSet(ScopedArenaAllocator* allocator, const ValueSet& other)
      : allocator_(allocator),
        num_buckets_(other.IdealBucketCount()),
        buckets_(allocator->AllocArray<Node*>(num_buckets_, kArenaAllocGvn)),
        buckets_owned_(allocator, num_buckets_, false, kArenaAllocGvn),
        num_entries_(0u) {
    // ArenaAllocator returns zeroed memory, so entries of buckets_ and
    // buckets_owned_ are initialized to null and false, respectively.
    DCHECK(IsPowerOfTwo(num_buckets_));
    PopulateFromInternal(other);
  }

  // Erases all values in this set and populates it with values from `other`.
  void PopulateFrom(const ValueSet& other) {
    if (this == &other) {
      return;
    }
    PopulateFromInternal(other);
  }

  // Returns true if `this` has enough buckets so that if `other` is copied into
  // it, the load factor will not cross the upper threshold.
  // If `exact_match` is set, true is returned only if `this` has the ideal
  // number of buckets. Larger number of buckets is allowed otherwise.
  bool CanHoldCopyOf(const ValueSet& other, bool exact_match) {
    if (exact_match) {
      return other.IdealBucketCount() == num_buckets_;
    } else {
      return other.IdealBucketCount() <= num_buckets_;
    }
  }

  // Adds an instruction in the set.
  void Add(HInstruction* instruction) {
    DCHECK(Lookup(instruction) == nullptr);
    size_t hash_code = HashCode(instruction);
    size_t index = BucketIndex(hash_code);

    if (!buckets_owned_.IsBitSet(index)) {
      CloneBucket(index);
    }
    buckets_[index] = new (allocator_) Node(instruction, hash_code, buckets_[index]);
    ++num_entries_;
  }

  // If in the set, returns an equivalent instruction to the given instruction.
  // Returns null otherwise.
  HInstruction* Lookup(HInstruction* instruction) const {
    size_t hash_code = HashCode(instruction);
    size_t index = BucketIndex(hash_code);

    for (Node* node = buckets_[index]; node != nullptr; node = node->GetNext()) {
      if (node->GetHashCode() == hash_code) {
        HInstruction* existing = node->GetInstruction();
        if (existing->Equals(instruction)) {
          return existing;
        }
      }
    }
    return nullptr;
  }

  // Returns whether instruction is in the set.
  bool Contains(HInstruction* instruction) const {
    size_t hash_code = HashCode(instruction);
    size_t index = BucketIndex(hash_code);

    for (Node* node = buckets_[index]; node != nullptr; node = node->GetNext()) {
      if (node->GetInstruction() == instruction) {
        return true;
      }
    }
    return false;
  }

  // Removes all instructions in the set affected by the given side effects.
  void Kill(SideEffects side_effects) {
    DeleteAllImpureWhich([side_effects](Node* node) {
      return node->GetInstruction()->GetSideEffects().MayDependOn(side_effects);
    });
  }

  void Clear() {
    num_entries_ = 0;
    for (size_t i = 0; i < num_buckets_; ++i) {
      buckets_[i] = nullptr;
    }
    buckets_owned_.SetInitialBits(num_buckets_);
  }

  // Updates this set by intersecting with instructions in a predecessor's set.
  void IntersectWith(ValueSet* predecessor) {
    if (IsEmpty()) {
      return;
    } else if (predecessor->IsEmpty()) {
      Clear();
    } else {
      // Pure instructions do not need to be tested because only impure
      // instructions can be killed.
      DeleteAllImpureWhich([predecessor](Node* node) {
        return !predecessor->Contains(node->GetInstruction());
      });
    }
  }

  bool IsEmpty() const { return num_entries_ == 0; }
  size_t GetNumberOfEntries() const { return num_entries_; }

 private:
  // Copies all entries from `other` to `this`.
  void PopulateFromInternal(const ValueSet& other) {
    DCHECK_NE(this, &other);
    DCHECK_GE(num_buckets_, other.IdealBucketCount());

    if (num_buckets_ == other.num_buckets_) {
      // Hash table remains the same size. We copy the bucket pointers and leave
      // all buckets_owned_ bits false.
      buckets_owned_.ClearAllBits();
      memcpy(buckets_, other.buckets_, num_buckets_ * sizeof(Node*));
    } else {
      // Hash table size changes. We copy and rehash all entries, and set all
      // buckets_owned_ bits to true.
      std::fill_n(buckets_, num_buckets_, nullptr);
      for (size_t i = 0; i < other.num_buckets_; ++i) {
        for (Node* node = other.buckets_[i]; node != nullptr; node = node->GetNext()) {
          size_t new_index = BucketIndex(node->GetHashCode());
          buckets_[new_index] = node->Dup(allocator_, buckets_[new_index]);
        }
      }
      buckets_owned_.SetInitialBits(num_buckets_);
    }

    num_entries_ = other.num_entries_;
  }

  class Node : public ArenaObject<kArenaAllocGvn> {
   public:
    Node(HInstruction* instruction, size_t hash_code, Node* next)
        : instruction_(instruction), hash_code_(hash_code), next_(next) {}

    size_t GetHashCode() const { return hash_code_; }
    HInstruction* GetInstruction() const { return instruction_; }
    Node* GetNext() const { return next_; }
    void SetNext(Node* node) { next_ = node; }

    Node* Dup(ScopedArenaAllocator* allocator, Node* new_next = nullptr) {
      return new (allocator) Node(instruction_, hash_code_, new_next);
    }

   private:
    HInstruction* const instruction_;
    const size_t hash_code_;
    Node* next_;

    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  // Creates our own copy of a bucket that is currently pointing to a parent.
  // This algorithm can be called while iterating over the bucket because it
  // preserves the order of entries in the bucket and will return the clone of
  // the given 'iterator'.
  Node* CloneBucket(size_t index, Node* iterator = nullptr) {
    DCHECK(!buckets_owned_.IsBitSet(index));
    Node* clone_current = nullptr;
    Node* clone_previous = nullptr;
    Node* clone_iterator = nullptr;
    for (Node* node = buckets_[index]; node != nullptr; node = node->GetNext()) {
      clone_current = node->Dup(allocator_, nullptr);
      if (node == iterator) {
        clone_iterator = clone_current;
      }
      if (clone_previous == nullptr) {
        buckets_[index] = clone_current;
      } else {
        clone_previous->SetNext(clone_current);
      }
      clone_previous = clone_current;
    }
    buckets_owned_.SetBit(index);
    return clone_iterator;
  }

  // Iterates over buckets with impure instructions (even indices) and deletes
  // the ones on which 'cond' returns true.
  template<typename Functor>
  void DeleteAllImpureWhich(Functor cond) {
    for (size_t i = 0; i < num_buckets_; i += 2) {
      Node* node = buckets_[i];
      Node* previous = nullptr;

      if (node == nullptr) {
        continue;
      }

      if (!buckets_owned_.IsBitSet(i)) {
        // Bucket is not owned but maybe we won't need to change it at all.
        // Iterate as long as the entries don't satisfy 'cond'.
        while (node != nullptr) {
          if (cond(node)) {
            // We do need to delete an entry but we do not own the bucket.
            // Clone the bucket, make sure 'previous' and 'node' point to
            // the cloned entries and break.
            previous = CloneBucket(i, previous);
            node = (previous == nullptr) ? buckets_[i] : previous->GetNext();
            break;
          }
          previous = node;
          node = node->GetNext();
        }
      }

      // By this point we either own the bucket and can start deleting entries,
      // or we do not own it but no entries matched 'cond'.
      DCHECK(buckets_owned_.IsBitSet(i) || node == nullptr);

      // We iterate over the remainder of entries and delete those that match
      // the given condition.
      while (node != nullptr) {
        Node* next = node->GetNext();
        if (cond(node)) {
          if (previous == nullptr) {
            buckets_[i] = next;
          } else {
            previous->SetNext(next);
          }
        } else {
          previous = node;
        }
        node = next;
      }
    }
  }

  // Computes a bucket count such that the load factor is reasonable.
  // This is estimated as (num_entries_ * 1.5) and rounded up to nearest pow2.
  size_t IdealBucketCount() const {
    size_t bucket_count = RoundUpToPowerOfTwo(num_entries_ + (num_entries_ >> 1));
    if (bucket_count > kMinimumNumberOfBuckets) {
      return bucket_count;
    } else {
      return kMinimumNumberOfBuckets;
    }
  }

  // Generates a hash code for an instruction.
  size_t HashCode(HInstruction* instruction) const {
    size_t hash_code = instruction->ComputeHashCode();
    // Pure instructions are put into odd buckets to speed up deletion. Note that in the
    // case of irreducible loops, we don't put pure instructions in odd buckets, as we
    // need to delete them when entering the loop.
    // ClinitCheck is treated as a pure instruction since it's only executed
    // once.
    bool pure = !instruction->GetSideEffects().HasDependencies() ||
                instruction->IsClinitCheck();
    if (!pure || instruction->GetBlock()->GetGraph()->HasIrreducibleLoops()) {
      return (hash_code << 1) | 0;
    } else {
      return (hash_code << 1) | 1;
    }
  }

  // Converts a hash code to a bucket index.
  size_t BucketIndex(size_t hash_code) const {
    return hash_code & (num_buckets_ - 1);
  }

  ScopedArenaAllocator* const allocator_;

  // The internal bucket implementation of the set.
  size_t const num_buckets_;
  Node** const buckets_;

  // Flags specifying which buckets were copied into the set from its parent.
  // If a flag is not set, the corresponding bucket points to entries in the
  // parent and must be cloned prior to making changes.
  ArenaBitVector buckets_owned_;

  // The number of entries in the set.
  size_t num_entries_;

  static constexpr size_t kMinimumNumberOfBuckets = 8;

  DISALLOW_COPY_AND_ASSIGN(ValueSet);
};

/**
 * Optimization phase that removes redundant instruction.
 */
class GlobalValueNumberer : public ValueObject {
 public:
  GlobalValueNumberer(HGraph* graph,
                      const SideEffectsAnalysis& side_effects)
      : graph_(graph),
        allocator_(graph->GetArenaStack()),
        side_effects_(side_effects),
        sets_(graph->GetBlocks().size(), nullptr, allocator_.Adapter(kArenaAllocGvn)),
        visited_blocks_(
            &allocator_, graph->GetBlocks().size(), /* expandable */ false, kArenaAllocGvn) {
    visited_blocks_.ClearAllBits();
  }

  void Run();

 private:
  // Per-block GVN. Will also update the ValueSet of the dominated and
  // successor blocks.
  void VisitBasicBlock(HBasicBlock* block);

  HGraph* graph_;
  ScopedArenaAllocator allocator_;
  const SideEffectsAnalysis& side_effects_;

  ValueSet* FindSetFor(HBasicBlock* block) const {
    ValueSet* result = sets_[block->GetBlockId()];
    DCHECK(result != nullptr) << "Could not find set for block B" << block->GetBlockId();
    return result;
  }

  void AbandonSetFor(HBasicBlock* block) {
    DCHECK(sets_[block->GetBlockId()] != nullptr)
        << "Block B" << block->GetBlockId() << " expected to have a set";
    sets_[block->GetBlockId()] = nullptr;
  }

  // Returns false if the GlobalValueNumberer has already visited all blocks
  // which may reference `block`.
  bool WillBeReferencedAgain(HBasicBlock* block) const;

  // Iterates over visited blocks and finds one which has a ValueSet such that:
  // (a) it will not be referenced in the future, and
  // (b) it can hold a copy of `reference_set` with a reasonable load factor.
  HBasicBlock* FindVisitedBlockWithRecyclableSet(HBasicBlock* block,
                                                 const ValueSet& reference_set) const;

  // ValueSet for blocks. Initially null, but for an individual block they
  // are allocated and populated by the dominator, and updated by all blocks
  // in the path from the dominator to the block.
  ScopedArenaVector<ValueSet*> sets_;

  // BitVector which serves as a fast-access map from block id to
  // visited/unvisited Boolean.
  ArenaBitVector visited_blocks_;

  DISALLOW_COPY_AND_ASSIGN(GlobalValueNumberer);
};

void GlobalValueNumberer::Run() {
  DCHECK(side_effects_.HasRun());
  sets_[graph_->GetEntryBlock()->GetBlockId()] = new (&allocator_) ValueSet(&allocator_);

  // Use the reverse post order to ensure the non back-edge predecessors of a block are
  // visited before the block itself.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    VisitBasicBlock(block);
  }
}

void GlobalValueNumberer::VisitBasicBlock(HBasicBlock* block) {
  ValueSet* set = nullptr;

  const ArenaVector<HBasicBlock*>& predecessors = block->GetPredecessors();
  if (predecessors.size() == 0 || predecessors[0]->IsEntryBlock()) {
    // The entry block should only accumulate constant instructions, and
    // the builder puts constants only in the entry block.
    // Therefore, there is no need to propagate the value set to the next block.
    set = new (&allocator_) ValueSet(&allocator_);
  } else {
    HBasicBlock* dominator = block->GetDominator();
    ValueSet* dominator_set = FindSetFor(dominator);

    if (dominator->GetSuccessors().size() == 1) {
      // `block` is a direct successor of its dominator. No need to clone the
      // dominator's set, `block` can take over its ownership including its buckets.
      DCHECK_EQ(dominator->GetSingleSuccessor(), block);
      AbandonSetFor(dominator);
      set = dominator_set;
    } else {
      // Try to find a basic block which will never be referenced again and whose
      // ValueSet can therefore be recycled. We will need to copy `dominator_set`
      // into the recycled set, so we pass `dominator_set` as a reference for size.
      HBasicBlock* recyclable = FindVisitedBlockWithRecyclableSet(block, *dominator_set);
      if (recyclable == nullptr) {
        // No block with a suitable ValueSet found. Allocate a new one and
        // copy `dominator_set` into it.
        set = new (&allocator_) ValueSet(&allocator_, *dominator_set);
      } else {
        // Block with a recyclable ValueSet found. Clone `dominator_set` into it.
        set = FindSetFor(recyclable);
        AbandonSetFor(recyclable);
        set->PopulateFrom(*dominator_set);
      }
    }

    if (!set->IsEmpty()) {
      if (block->IsLoopHeader()) {
        if (block->GetLoopInformation()->ContainsIrreducibleLoop()) {
          // To satisfy our linear scan algorithm, no instruction should flow in an irreducible
          // loop header. We clear the set at entry of irreducible loops and any loop containing
          // an irreducible loop, as in both cases, GVN can extend the liveness of an instruction
          // across the irreducible loop.
          // Note that, if we're not compiling OSR, we could still do GVN and introduce
          // phis at irreducible loop headers. We decided it was not worth the complexity.
          set->Clear();
        } else {
          DCHECK(!block->GetLoopInformation()->IsIrreducible());
          DCHECK_EQ(block->GetDominator(), block->GetLoopInformation()->GetPreHeader());
          set->Kill(side_effects_.GetLoopEffects(block));
        }
      } else if (predecessors.size() > 1) {
        for (HBasicBlock* predecessor : predecessors) {
          set->IntersectWith(FindSetFor(predecessor));
          if (set->IsEmpty()) {
            break;
          }
        }
      }
    }
  }

  sets_[block->GetBlockId()] = set;

  HInstruction* current = block->GetFirstInstruction();
  while (current != nullptr) {
    // Save the next instruction in case `current` is removed from the graph.
    HInstruction* next = current->GetNext();
    // Do not kill the set with the side effects of the instruction just now: if
    // the instruction is GVN'ed, we don't need to kill.
    if (current->CanBeMoved()) {
      if (current->IsBinaryOperation() && current->AsBinaryOperation()->IsCommutative()) {
        // For commutative ops, (x op y) will be treated the same as (y op x)
        // after fixed ordering.
        current->AsBinaryOperation()->OrderInputs();
      }
      HInstruction* existing = set->Lookup(current);
      if (existing != nullptr) {
        // This replacement doesn't make more OrderInputs() necessary since
        // current is either used by an instruction that it dominates,
        // which hasn't been visited yet due to the order we visit instructions.
        // Or current is used by a phi, and we don't do OrderInputs() on a phi anyway.
        current->ReplaceWith(existing);
        current->GetBlock()->RemoveInstruction(current);
      } else {
        set->Kill(current->GetSideEffects());
        set->Add(current);
      }
    } else {
      set->Kill(current->GetSideEffects());
    }
    current = next;
  }

  visited_blocks_.SetBit(block->GetBlockId());
}

bool GlobalValueNumberer::WillBeReferencedAgain(HBasicBlock* block) const {
  DCHECK(visited_blocks_.IsBitSet(block->GetBlockId()));

  for (const HBasicBlock* dominated_block : block->GetDominatedBlocks()) {
    if (!visited_blocks_.IsBitSet(dominated_block->GetBlockId())) {
      return true;
    }
  }

  for (const HBasicBlock* successor : block->GetSuccessors()) {
    if (!visited_blocks_.IsBitSet(successor->GetBlockId())) {
      return true;
    }
  }

  return false;
}

HBasicBlock* GlobalValueNumberer::FindVisitedBlockWithRecyclableSet(
    HBasicBlock* block, const ValueSet& reference_set) const {
  HBasicBlock* secondary_match = nullptr;

  for (size_t block_id : visited_blocks_.Indexes()) {
    ValueSet* current_set = sets_[block_id];
    if (current_set == nullptr) {
      // Set was already recycled.
      continue;
    }

    HBasicBlock* current_block = block->GetGraph()->GetBlocks()[block_id];

    // We test if `current_set` has enough buckets to store a copy of
    // `reference_set` with a reasonable load factor. If we find a set whose
    // number of buckets matches perfectly, we return right away. If we find one
    // that is larger, we return it if no perfectly-matching set is found.
    // Note that we defer testing WillBeReferencedAgain until all other criteria
    // have been satisfied because it might be expensive.
    if (current_set->CanHoldCopyOf(reference_set, /* exact_match */ true)) {
      if (!WillBeReferencedAgain(current_block)) {
        return current_block;
      }
    } else if (secondary_match == nullptr &&
               current_set->CanHoldCopyOf(reference_set, /* exact_match */ false)) {
      if (!WillBeReferencedAgain(current_block)) {
        secondary_match = current_block;
      }
    }
  }

  return secondary_match;
}

void GVNOptimization::Run() {
  GlobalValueNumberer gvn(graph_, side_effects_);
  gvn.Run();
}

}  // namespace art
