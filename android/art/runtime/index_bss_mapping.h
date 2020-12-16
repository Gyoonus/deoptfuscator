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

#ifndef ART_RUNTIME_INDEX_BSS_MAPPING_H_
#define ART_RUNTIME_INDEX_BSS_MAPPING_H_

#include <android-base/logging.h>

#include "base/bit_utils.h"

namespace art {

template<typename T> class LengthPrefixedArray;

// IndexBssMappingEntry describes a mapping of one or more indexes to their offsets in the .bss.
// A sorted array of IndexBssMappingEntry is used to describe the mapping of method indexes,
// type indexes or string indexes to offsets of their assigned slots in the .bss.
//
// The highest index and a mask are stored in a single `uint32_t index_and_mask` and the split
// between the index and the mask is provided externally. The "mask" bits specify whether some
// of the previous indexes are mapped to immediately preceding slots. This is permissible only
// if the slots are consecutive and in the same order as indexes.
//
// The .bss offset of the slot associated with the highest index is stored in plain form as
// `bss_offset`. If the mask specifies any smaller indexes being mapped to immediately
// preceding slots, their offsets are calculated using an externally supplied size of the slot.
struct IndexBssMappingEntry {
  static size_t IndexBits(uint32_t number_of_indexes) {
    DCHECK_NE(number_of_indexes, 0u);
    return MinimumBitsToStore(number_of_indexes - 1u);
  }

  static uint32_t IndexMask(size_t index_bits) {
    DCHECK_LE(index_bits, 32u);
    constexpr uint32_t kAllOnes = static_cast<uint32_t>(-1);
    // Handle `index_bits == 32u` explicitly; shifting uint32_t left by 32 is undefined behavior.
    return (index_bits == 32u) ? kAllOnes : ~(kAllOnes << index_bits);
  }

  uint32_t GetIndex(size_t index_bits) const {
    return index_and_mask & IndexMask(index_bits);
  }

  uint32_t GetMask(size_t index_bits) const {
    DCHECK_LT(index_bits, 32u);  // GetMask() is valid only if there is at least 1 mask bit.
    return index_and_mask >> index_bits;
  }

  size_t GetBssOffset(size_t index_bits, uint32_t index, size_t slot_size) const;

  uint32_t index_and_mask;
  uint32_t bss_offset;
};

using IndexBssMapping = LengthPrefixedArray<IndexBssMappingEntry>;

class IndexBssMappingLookup {
 public:
  static constexpr size_t npos = static_cast<size_t>(-1);

  static size_t GetBssOffset(const IndexBssMapping* mapping,
                             uint32_t index,
                             uint32_t number_of_indexes,
                             size_t slot_size);
};

}  // namespace art

#endif  // ART_RUNTIME_INDEX_BSS_MAPPING_H_
