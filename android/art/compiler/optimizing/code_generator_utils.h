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

#ifndef ART_COMPILER_OPTIMIZING_CODE_GENERATOR_UTILS_H_
#define ART_COMPILER_OPTIMIZING_CODE_GENERATOR_UTILS_H_

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace art {

class HInstruction;

// Computes the magic number and the shift needed in the div/rem by constant algorithm, as out
// arguments `magic` and `shift`
void CalculateMagicAndShiftForDivRem(int64_t divisor, bool is_long, int64_t* magic, int* shift);

// Returns true if `cond_input` is expected to have a location. Assumes that
// `cond_input` is a conditional input of the currently emitted instruction and
// that it has been previously visited by the InstructionCodeGenerator.
bool IsBooleanValueOrMaterializedCondition(HInstruction* cond_input);

template <typename T> T AbsOrMin(T value) {
  return (value == std::numeric_limits<T>::min())
      ? value
      : std::abs(value);
}

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CODE_GENERATOR_UTILS_H_
