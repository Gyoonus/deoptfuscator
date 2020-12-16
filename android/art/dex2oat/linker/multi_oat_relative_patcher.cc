/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "multi_oat_relative_patcher.h"

#include <android-base/logging.h>

#include "base/bit_utils.h"
#include "globals.h"

namespace art {
namespace linker {

MultiOatRelativePatcher::MultiOatRelativePatcher(InstructionSet instruction_set,
                                                 const InstructionSetFeatures* features)
    : method_offset_map_(),
      relative_patcher_(RelativePatcher::Create(instruction_set, features, &method_offset_map_)),
      adjustment_(0u),
      instruction_set_(instruction_set),
      start_size_code_alignment_(0u),
      start_size_relative_call_thunks_(0u),
      start_size_misc_thunks_(0u) {
}

void MultiOatRelativePatcher::StartOatFile(uint32_t adjustment) {
  DCHECK_ALIGNED(adjustment, kPageSize);
  adjustment_ = adjustment;

  start_size_code_alignment_ = relative_patcher_->CodeAlignmentSize();
  start_size_relative_call_thunks_ = relative_patcher_->RelativeCallThunksSize();
  start_size_misc_thunks_ = relative_patcher_->MiscThunksSize();
}

uint32_t MultiOatRelativePatcher::CodeAlignmentSize() const {
  DCHECK_GE(relative_patcher_->CodeAlignmentSize(), start_size_code_alignment_);
  return relative_patcher_->CodeAlignmentSize() - start_size_code_alignment_;
}

uint32_t MultiOatRelativePatcher::RelativeCallThunksSize() const {
  DCHECK_GE(relative_patcher_->RelativeCallThunksSize(), start_size_relative_call_thunks_);
  return relative_patcher_->RelativeCallThunksSize() - start_size_relative_call_thunks_;
}

uint32_t MultiOatRelativePatcher::MiscThunksSize() const {
  DCHECK_GE(relative_patcher_->MiscThunksSize(), start_size_misc_thunks_);
  return relative_patcher_->MiscThunksSize() - start_size_misc_thunks_;
}

std::pair<bool, uint32_t> MultiOatRelativePatcher::MethodOffsetMap::FindMethodOffset(
    MethodReference ref) {
  auto it = map.find(ref);
  if (it == map.end()) {
    return std::pair<bool, uint32_t>(false, 0u);
  } else {
    return std::pair<bool, uint32_t>(true, it->second);
  }
}
}  // namespace linker
}  // namespace art
