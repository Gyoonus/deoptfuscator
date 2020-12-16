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

#include <algorithm>

#include "index_bss_mapping.h"

#include "base/bit_utils.h"
#include "base/length_prefixed_array.h"

namespace art {

size_t IndexBssMappingEntry::GetBssOffset(size_t index_bits,
                                          uint32_t index,
                                          size_t slot_size) const {
  uint32_t diff = GetIndex(index_bits) - index;
  if (diff == 0u) {
    return bss_offset;
  }
  size_t mask_bits = 32u - index_bits;
  if (diff > mask_bits) {
    return IndexBssMappingLookup::npos;
  }
  // Shift out the index bits and bits for lower indexes.
  // Note that `index_bits + (mask_bits - diff) == 32 - diff`.
  uint32_t mask_from_index = index_and_mask >> (32u - diff);
  if ((mask_from_index & 1u) != 0u) {
    return bss_offset - POPCOUNT(mask_from_index) * slot_size;
  } else {
    return IndexBssMappingLookup::npos;
  }
}

constexpr size_t IndexBssMappingLookup::npos;

size_t IndexBssMappingLookup::GetBssOffset(const IndexBssMapping* mapping,
                                           uint32_t index,
                                           uint32_t number_of_indexes,
                                           size_t slot_size) {
  DCHECK_LT(index, number_of_indexes);
  if (mapping == nullptr) {
    return npos;
  }
  size_t index_bits = IndexBssMappingEntry::IndexBits(number_of_indexes);
  uint32_t index_mask = IndexBssMappingEntry::IndexMask(index_bits);
  auto it = std::partition_point(
      mapping->begin(),
      mapping->end(),
      [=](const struct IndexBssMappingEntry& entry) {
        return (entry.index_and_mask & index_mask) < index;
      });
  if (it == mapping->end()) {
    return npos;
  }
  const IndexBssMappingEntry& entry = *it;
  return entry.GetBssOffset(index_bits, index, slot_size);
}

}  // namespace art
