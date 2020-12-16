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

#ifndef ART_COMPILER_UTILS_STACK_CHECKS_H_
#define ART_COMPILER_UTILS_STACK_CHECKS_H_

#include "arch/instruction_set.h"

namespace art {

// Size of a frame that we definitely consider large. Anything larger than this should
// definitely get a stack overflow check.
static constexpr size_t kLargeFrameSize = 2 * KB;

// Size of a frame that should be small. Anything leaf method smaller than this should run
// without a stack overflow check.
// The constant is from experience with frameworks code.
static constexpr size_t kSmallFrameSize = 1 * KB;

// Determine whether a frame is small or large, used in the decision on whether to elide a
// stack overflow check on method entry.
//
// A frame is considered large when it's above kLargeFrameSize.
static inline bool FrameNeedsStackCheck(size_t size, InstructionSet isa ATTRIBUTE_UNUSED) {
  return size >= kLargeFrameSize;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_STACK_CHECKS_H_
