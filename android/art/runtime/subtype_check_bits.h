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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_BITS_H_
#define ART_RUNTIME_SUBTYPE_CHECK_BITS_H_

#include "base/bit_string.h"
#include "base/bit_struct.h"
#include "base/bit_utils.h"

namespace art {

/**
 * The SubtypeCheckBits memory layout (in bits):
 *
 *   1 bit       Variable
 *     |             |
 *     v             v     <---- up to 23 bits ---->
 *
 *  +----+-----------+--------+-------------------------+
 *  |    |                  Bitstring                   |
 *  +    +-----------+--------+-------------------------+
 *  | OF | (unused)  |  Next  |      Path To Root       |
 *  +    |           |--------+----+----------+----+----+
 *  |    | (0....0)  |        |    |   ...    |    |    |
 *  +----+-----------+--------+----+----------+----+----+
 * MSB (most significant bit)                          LSB
 *
 * The bitstring takes up to 23 bits; anything exceeding that is truncated:
 * - Path To Root is a list of chars, encoded as a BitString:
 *     starting at the root (in LSB), each character is a sibling index unique to the parent,
 *   Paths longer than BitString::kCapacity are truncated to fit within the BitString.
 * - Next is a single BitStringChar (immediatelly following Path To Root)
 *     When new children are assigned paths, they get allocated the parent's Next value.
 *     The next value is subsequently incremented.
 *
 * The exact bit position of (unused) is variable-length:
 * In the cases that the "Path To Root" + "Next" does not fill up the entire
 * BitString capacity, the remaining bits are (unused) and left as 0s.
 *
 * There is also an additional "OF" (overflow) field to indicate that the
 * PathToRoot has been truncated.
 *
 * See subtype_check.h and subtype_check_info.h for more details.
 */
BITSTRUCT_DEFINE_START(SubtypeCheckBits, /*size*/ BitString::BitStructSizeOf() + 1u)
  BitStructField<BitString, /*lsb*/ 0> bitstring_;
  BitStructUint</*lsb*/ BitString::BitStructSizeOf(), /*width*/ 1> overflow_;
BITSTRUCT_DEFINE_END(SubtypeCheckBits);

}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_BITS_H_
