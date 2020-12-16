/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "memory_region.h"

#include <stdint.h>
#include <string.h>

namespace art {

void MemoryRegion::CopyFrom(size_t offset, const MemoryRegion& from) const {
  CHECK(from.pointer() != nullptr);
  CHECK_GT(from.size(), 0U);
  CHECK_GE(this->size(), from.size());
  CHECK_LE(offset, this->size() - from.size());
  memmove(reinterpret_cast<void*>(begin() + offset), from.pointer(), from.size());
}

void MemoryRegion::StoreBits(uintptr_t bit_offset, uint32_t value, size_t length) {
  DCHECK_LE(value, MaxInt<uint32_t>(length));
  DCHECK_LE(length, BitSizeOf<uint32_t>());
  DCHECK_LE(bit_offset + length, size_in_bits());
  if (length == 0) {
    return;
  }
  // Bits are stored in this order {7 6 5 4 3 2 1 0}.
  // How many remaining bits in current byte is (bit_offset % kBitsPerByte) + 1.
  uint8_t* out = ComputeInternalPointer<uint8_t>(bit_offset >> kBitsPerByteLog2);
  size_t orig_len = length;
  uint32_t orig_value = value;
  uintptr_t bit_remainder = bit_offset % kBitsPerByte;
  while (true) {
    const uintptr_t remaining_bits = kBitsPerByte - bit_remainder;
    if (length <= remaining_bits) {
      // Length is smaller than all of remainder bits.
      size_t mask = ((1 << length) - 1) << bit_remainder;
      *out = (*out & ~mask) | (value << bit_remainder);
      break;
    }
    // Copy remaining bits in current byte.
    size_t value_mask = (1 << remaining_bits) - 1;
    *out = (*out & ~(value_mask << bit_remainder)) | ((value & value_mask) << bit_remainder);
    value >>= remaining_bits;
    bit_remainder = 0;
    length -= remaining_bits;
    ++out;
  }
  DCHECK_EQ(LoadBits(bit_offset, orig_len), orig_value) << bit_offset << " " << orig_len;
}

}  // namespace art
