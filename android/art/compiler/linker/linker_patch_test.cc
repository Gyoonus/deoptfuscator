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

#include <gtest/gtest.h>

#include "linker_patch.h"

namespace art {
namespace linker {

TEST(LinkerPatch, LinkerPatchOperators) {
  const DexFile* dex_file1 = reinterpret_cast<const DexFile*>(1);
  const DexFile* dex_file2 = reinterpret_cast<const DexFile*>(2);
  LinkerPatch patches[] = {
      LinkerPatch::RelativeMethodPatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file1, 3001u, 1001u),  // Index 3.
      LinkerPatch::RelativeMethodPatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeMethodPatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file1, 3001u, 1001u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::MethodBssEntryPatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::CodePatch(16u, dex_file1, 1000u),
      LinkerPatch::CodePatch(16u, dex_file1, 1001u),
      LinkerPatch::CodePatch(16u, dex_file2, 1000u),
      LinkerPatch::CodePatch(16u, dex_file2, 1001u),
      LinkerPatch::RelativeCodePatch(16u, dex_file1, 1000u),
      LinkerPatch::RelativeCodePatch(16u, dex_file1, 1001u),
      LinkerPatch::RelativeCodePatch(16u, dex_file2, 1000u),
      LinkerPatch::RelativeCodePatch(16u, dex_file2, 1001u),
      LinkerPatch::RelativeTypePatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeTypePatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeTypePatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeTypePatch(16u, dex_file1, 3001u, 1001u),
      LinkerPatch::RelativeTypePatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeTypePatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeTypePatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeTypePatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file1, 3001u, 1001u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::TypeBssEntryPatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::RelativeStringPatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeStringPatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeStringPatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeStringPatch(16u, dex_file1, 3001u, 1001u),
      LinkerPatch::RelativeStringPatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeStringPatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeStringPatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeStringPatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file1, 3000u, 1000u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file1, 3001u, 1000u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file1, 3000u, 1001u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file1, 3001u, 1001u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file2, 3000u, 1000u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file2, 3001u, 1000u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file2, 3000u, 1001u),
      LinkerPatch::StringBssEntryPatch(16u, dex_file2, 3001u, 1001u),
      LinkerPatch::BakerReadBarrierBranchPatch(16u, 0u, 0u),
      LinkerPatch::BakerReadBarrierBranchPatch(16u, 0u, 1u),
      LinkerPatch::BakerReadBarrierBranchPatch(16u, 1u, 0u),
      LinkerPatch::BakerReadBarrierBranchPatch(16u, 1u, 1u),

      LinkerPatch::RelativeMethodPatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeMethodPatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::MethodBssEntryPatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::CodePatch(32u, dex_file1, 1000u),
      LinkerPatch::CodePatch(32u, dex_file1, 1001u),
      LinkerPatch::CodePatch(32u, dex_file2, 1000u),
      LinkerPatch::CodePatch(32u, dex_file2, 1001u),
      LinkerPatch::RelativeCodePatch(32u, dex_file1, 1000u),
      LinkerPatch::RelativeCodePatch(32u, dex_file1, 1001u),
      LinkerPatch::RelativeCodePatch(32u, dex_file2, 1000u),
      LinkerPatch::RelativeCodePatch(32u, dex_file2, 1001u),
      LinkerPatch::RelativeTypePatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeTypePatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeTypePatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeTypePatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::RelativeTypePatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeTypePatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeTypePatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeTypePatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::TypeBssEntryPatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::RelativeStringPatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::RelativeStringPatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::RelativeStringPatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::RelativeStringPatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::RelativeStringPatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::RelativeStringPatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::RelativeStringPatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::RelativeStringPatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file1, 3000u, 1000u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file1, 3001u, 1000u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file1, 3000u, 1001u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file1, 3001u, 1001u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file2, 3000u, 1000u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file2, 3001u, 1000u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file2, 3000u, 1001u),
      LinkerPatch::StringBssEntryPatch(32u, dex_file2, 3001u, 1001u),
      LinkerPatch::BakerReadBarrierBranchPatch(32u, 0u, 0u),
      LinkerPatch::BakerReadBarrierBranchPatch(32u, 0u, 1u),
      LinkerPatch::BakerReadBarrierBranchPatch(32u, 1u, 0u),
      LinkerPatch::BakerReadBarrierBranchPatch(32u, 1u, 1u),

      LinkerPatch::RelativeMethodPatch(16u, dex_file1, 3001u, 1001u),  // Same as patch at index 3.
  };
  constexpr size_t last_index = arraysize(patches) - 1u;

  for (size_t i = 0; i != arraysize(patches); ++i) {
    for (size_t j = 0; j != arraysize(patches); ++j) {
      bool expected = (i != last_index ? i : 3u) == (j != last_index ? j : 3u);
      EXPECT_EQ(expected, patches[i] == patches[j]) << i << " " << j;
    }
  }

  for (size_t i = 0; i != arraysize(patches); ++i) {
    for (size_t j = 0; j != arraysize(patches); ++j) {
      bool expected = (i != last_index ? i : 3u) < (j != last_index ? j : 3u);
      EXPECT_EQ(expected, patches[i] < patches[j]) << i << " " << j;
    }
  }
}

}  // namespace linker
}  // namespace art
