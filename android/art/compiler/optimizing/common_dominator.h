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

#ifndef ART_COMPILER_OPTIMIZING_COMMON_DOMINATOR_H_
#define ART_COMPILER_OPTIMIZING_COMMON_DOMINATOR_H_

#include "nodes.h"

namespace art {

// Helper class for finding common dominators of two or more blocks in a graph.
// The domination information of a graph must not be modified while there is
// a CommonDominator object as it's internal state could become invalid.
class CommonDominator {
 public:
  // Convenience function to find the common dominator of 2 blocks.
  static HBasicBlock* ForPair(HBasicBlock* block1, HBasicBlock* block2) {
    CommonDominator finder(block1);
    finder.Update(block2);
    return finder.Get();
  }

  // Create a finder starting with a given block.
  explicit CommonDominator(HBasicBlock* block)
      : dominator_(block), chain_length_(ChainLength(block)) {
  }

  // Update the common dominator with another block.
  void Update(HBasicBlock* block) {
    DCHECK(block != nullptr);
    if (dominator_ == nullptr) {
      dominator_ = block;
      chain_length_ = ChainLength(block);
      return;
    }
    HBasicBlock* block2 = dominator_;
    DCHECK(block2 != nullptr);
    if (block == block2) {
      return;
    }
    size_t chain_length = ChainLength(block);
    size_t chain_length2 = chain_length_;
    // Equalize the chain lengths
    for ( ; chain_length > chain_length2; --chain_length) {
      block = block->GetDominator();
      DCHECK(block != nullptr);
    }
    for ( ; chain_length2 > chain_length; --chain_length2) {
      block2 = block2->GetDominator();
      DCHECK(block2 != nullptr);
    }
    // Now run up the chain until we hit the common dominator.
    while (block != block2) {
      --chain_length;
      block = block->GetDominator();
      DCHECK(block != nullptr);
      block2 = block2->GetDominator();
      DCHECK(block2 != nullptr);
    }
    dominator_ = block;
    chain_length_ = chain_length;
  }

  HBasicBlock* Get() const {
    return dominator_;
  }

 private:
  static size_t ChainLength(HBasicBlock* block) {
    size_t result = 0;
    while (block != nullptr) {
      ++result;
      block = block->GetDominator();
    }
    return result;
  }

  HBasicBlock* dominator_;
  size_t chain_length_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_COMMON_DOMINATOR_H_
