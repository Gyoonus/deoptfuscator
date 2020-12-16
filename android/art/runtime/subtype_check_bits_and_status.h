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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_
#define ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_

#include "base/bit_struct.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "class_status.h"
#include "subtype_check_bits.h"

namespace art {

/*
 * Enables a highly efficient O(1) subtype comparison by storing extra data
 * in the unused padding bytes of ClassStatus.
 */

// TODO: Update BitSizeOf with this version.
template <typename T>
static constexpr size_t NonNumericBitSizeOf() {
    return kBitsPerByte * sizeof(T);
}

/**
 * MSB (most significant bit)                                          LSB
 *  +---------------+---------------------------------------------------+
 *  |               |                                                   |
 *  |  ClassStatus  |                 SubtypeCheckBits                  |
 *  |               |                                                   |
 *  +---------------+---------------------------------------------------+
 *   <-- 4 bits -->             <-----     28 bits     ----->
 *
 * Invariants:
 *
 *  AddressOf(ClassStatus) == AddressOf(SubtypeCheckBitsAndStatus)
 *  BitSizeOf(SubtypeCheckBitsAndStatus) == 32
 *
 * Note that with this representation the "Path To Root" in the MSB of this 32-bit word:
 * This enables a highly efficient path comparison between any two labels:
 *
 * src <: target :=
 *   (src & mask) == (target & mask)  where  mask := (1u << len(path-to-root(target)) - 1u
 *
 * In the above example, the `len()` (and thus `mask`) is a function of the depth.
 * Since the target is known at compile time, it becomes
 *   (src & #imm_mask) == #imm
 * or
 *   ((src - #imm) << #imm_shift_to_remove_high_bits) == 0
 * or a similar expression chosen for the best performance or code size.
 *
 * (This requires that path-to-root in `target` is not truncated, i.e. it is in the Assigned state).
 */
static constexpr size_t kClassStatusBitSize = MinimumBitsToStore(enum_cast<>(ClassStatus::kLast));
static_assert(kClassStatusBitSize == 4u, "ClassStatus should need 4 bits.");
BITSTRUCT_DEFINE_START(SubtypeCheckBitsAndStatus, BitSizeOf<BitString::StorageType>())
  BitStructField<SubtypeCheckBits, /*lsb*/ 0> subtype_check_info_;
  BitStructField<ClassStatus,
                 /*lsb*/ SubtypeCheckBits::BitStructSizeOf(),
                 /*width*/ kClassStatusBitSize> status_;
  BitStructInt</*lsb*/ 0, /*width*/ BitSizeOf<BitString::StorageType>()> int32_alias_;
BITSTRUCT_DEFINE_END(SubtypeCheckBitsAndStatus);

// Use the spare alignment from "ClassStatus" to store all the new SubtypeCheckInfo data.
static_assert(sizeof(SubtypeCheckBitsAndStatus) == sizeof(uint32_t),
              "All of SubtypeCheckInfo+ClassStatus should fit into 4 bytes");
}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_
