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

#ifndef ART_DEX2OAT_LINKER_INDEX_BSS_MAPPING_ENCODER_H_
#define ART_DEX2OAT_LINKER_INDEX_BSS_MAPPING_ENCODER_H_

#include <android-base/logging.h>

#include "base/bit_utils.h"
#include "base/bit_vector-inl.h"
#include "index_bss_mapping.h"

namespace art {
namespace linker {

// Helper class for encoding compressed IndexBssMapping.
class IndexBssMappingEncoder {
 public:
  IndexBssMappingEncoder(size_t number_of_indexes, size_t slot_size)
      : index_bits_(IndexBssMappingEntry::IndexBits(number_of_indexes)),
        slot_size_(slot_size) {
    entry_.index_and_mask = static_cast<uint32_t>(-1);
    entry_.bss_offset = static_cast<uint32_t>(-1);
    DCHECK_NE(number_of_indexes, 0u);
  }

  // Try to merge the next index -> bss_offset mapping into the current entry.
  // Return true on success, false on failure.
  bool TryMerge(uint32_t index, uint32_t bss_offset) {
    DCHECK_LE(MinimumBitsToStore(index), index_bits_);
    DCHECK_NE(index, entry_.GetIndex(index_bits_));
    if (entry_.bss_offset + slot_size_ != bss_offset) {
      return false;
    }
    uint32_t diff = index - entry_.GetIndex(index_bits_);
    if (diff > 32u - index_bits_) {
      return false;
    }
    uint32_t mask = entry_.GetMask(index_bits_);
    if ((mask & ~(static_cast<uint32_t>(-1) << diff)) != 0u) {
      return false;
    }
    // Insert the bit indicating the index we've just overwritten
    // and shift bits indicating indexes before that.
    mask = ((mask << index_bits_) >> diff) | (static_cast<uint32_t>(1u) << (32 - diff));
    entry_.index_and_mask = mask | index;
    entry_.bss_offset = bss_offset;
    return true;
  }

  void Reset(uint32_t method_index, uint32_t bss_offset) {
    DCHECK_LE(MinimumBitsToStore(method_index), index_bits_);
    entry_.index_and_mask = method_index;  // Mask bits set to 0.
    entry_.bss_offset = bss_offset;
  }

  IndexBssMappingEntry GetEntry() {
    return entry_;
  }

  size_t GetIndexBits() const {
    return index_bits_;
  }

 private:
  const size_t index_bits_;
  const size_t slot_size_;
  IndexBssMappingEntry entry_;
};

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_INDEX_BSS_MAPPING_ENCODER_H_
