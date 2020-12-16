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

#include "code_generator_utils.h"

#include <android-base/logging.h>

#include "nodes.h"

namespace art {

void CalculateMagicAndShiftForDivRem(int64_t divisor, bool is_long,
                                     int64_t* magic, int* shift) {
  // It does not make sense to calculate magic and shift for zero divisor.
  DCHECK_NE(divisor, 0);

  /* Implementation according to H.S.Warren's "Hacker's Delight" (Addison Wesley, 2002)
   * Chapter 10 and T.Grablund, P.L.Montogomery's "Division by Invariant Integers Using
   * Multiplication" (PLDI 1994).
   * The magic number M and shift S can be calculated in the following way:
   * Let nc be the most positive value of numerator(n) such that nc = kd - 1,
   * where divisor(d) >= 2.
   * Let nc be the most negative value of numerator(n) such that nc = kd + 1,
   * where divisor(d) <= -2.
   * Thus nc can be calculated like:
   * nc = exp + exp % d - 1, where d >= 2 and exp = 2^31 for int or 2^63 for long
   * nc = -exp + (exp + 1) % d, where d >= 2 and exp = 2^31 for int or 2^63 for long
   *
   * So the shift p is the smallest p satisfying
   * 2^p > nc * (d - 2^p % d), where d >= 2
   * 2^p > nc * (d + 2^p % d), where d <= -2.
   *
   * The magic number M is calculated by
   * M = (2^p + d - 2^p % d) / d, where d >= 2
   * M = (2^p - d - 2^p % d) / d, where d <= -2.
   *
   * Notice that p is always bigger than or equal to 32 (resp. 64), so we just return 32 - p
   * (resp. 64 - p) as the shift number S.
   */

  int64_t p = is_long ? 63 : 31;
  const uint64_t exp = is_long ? (UINT64_C(1) << 63) : (UINT32_C(1) << 31);

  // Initialize the computations.
  uint64_t abs_d = (divisor >= 0) ? divisor : -divisor;
  uint64_t sign_bit = is_long ? static_cast<uint64_t>(divisor) >> 63 :
                                static_cast<uint32_t>(divisor) >> 31;
  uint64_t tmp = exp + sign_bit;
  uint64_t abs_nc = tmp - 1 - (tmp % abs_d);
  uint64_t quotient1 = exp / abs_nc;
  uint64_t remainder1 = exp % abs_nc;
  uint64_t quotient2 = exp / abs_d;
  uint64_t remainder2 = exp % abs_d;

  /*
   * To avoid handling both positive and negative divisor, "Hacker's Delight"
   * introduces a method to handle these 2 cases together to avoid duplication.
   */
  uint64_t delta;
  do {
    p++;
    quotient1 = 2 * quotient1;
    remainder1 = 2 * remainder1;
    if (remainder1 >= abs_nc) {
      quotient1++;
      remainder1 = remainder1 - abs_nc;
    }
    quotient2 = 2 * quotient2;
    remainder2 = 2 * remainder2;
    if (remainder2 >= abs_d) {
      quotient2++;
      remainder2 = remainder2 - abs_d;
    }
    delta = abs_d - remainder2;
  } while (quotient1 < delta || (quotient1 == delta && remainder1 == 0));

  *magic = (divisor > 0) ? (quotient2 + 1) : (-quotient2 - 1);

  if (!is_long) {
    *magic = static_cast<int>(*magic);
  }

  *shift = is_long ? p - 64 : p - 32;
}

bool IsBooleanValueOrMaterializedCondition(HInstruction* cond_input) {
  return !cond_input->IsCondition() || !cond_input->IsEmittedAtUseSite();
}

}  // namespace art
