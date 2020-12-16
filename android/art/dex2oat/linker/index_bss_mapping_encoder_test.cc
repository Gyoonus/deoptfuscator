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

#include "index_bss_mapping_encoder.h"

#include "base/enums.h"
#include "gtest/gtest.h"

namespace art {
namespace linker {

TEST(IndexBssMappingEncoder, TryMerge16BitIndex) {
  for (PointerSize pointer_size : {PointerSize::k32, PointerSize::k64}) {
    size_t raw_pointer_size = static_cast<size_t>(pointer_size);
    IndexBssMappingEncoder encoder(/* number_of_indexes */ 0x10000, raw_pointer_size);
    encoder.Reset(1u, 0u);
    ASSERT_FALSE(encoder.TryMerge(5u, raw_pointer_size + 1));       // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(18u, raw_pointer_size));          // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(5u, raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(IndexBssMappingLookup::npos,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 17u, raw_pointer_size));
    ASSERT_FALSE(encoder.TryMerge(17u, 2 * raw_pointer_size + 1));  // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(18u, 2 * raw_pointer_size));      // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(17u, 2 * raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(2 * raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 17u, raw_pointer_size));
    ASSERT_EQ(0x00110000u | 17u, encoder.GetEntry().index_and_mask);
    ASSERT_FALSE(encoder.TryMerge(18u, 3 * raw_pointer_size));      // Index out of range.
  }
}

TEST(IndexBssMappingEncoder, TryMerge8BitIndex) {
  for (PointerSize pointer_size : {PointerSize::k32, PointerSize::k64}) {
    size_t raw_pointer_size = static_cast<size_t>(pointer_size);
    IndexBssMappingEncoder encoder(/* number_of_indexes */ 0x100, raw_pointer_size);
    encoder.Reset(1u, 0u);
    ASSERT_FALSE(encoder.TryMerge(5u, raw_pointer_size + 1));       // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(26u, raw_pointer_size));          // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(5u, raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(IndexBssMappingLookup::npos,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 17u, raw_pointer_size));
    ASSERT_FALSE(encoder.TryMerge(25u, 2 * raw_pointer_size + 1));  // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(26u, 2 * raw_pointer_size));      // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(25u, 2 * raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(2 * raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 25u, raw_pointer_size));
    ASSERT_EQ(0x00001100u | 25u, encoder.GetEntry().index_and_mask);
    ASSERT_FALSE(encoder.TryMerge(26u, 3 * raw_pointer_size));      // Index out of range.
  }
}

TEST(IndexBssMappingEncoder, TryMerge20BitIndex) {
  for (PointerSize pointer_size : {PointerSize::k32, PointerSize::k64}) {
    size_t raw_pointer_size = static_cast<size_t>(pointer_size);
    IndexBssMappingEncoder encoder(/* number_of_indexes */ 0x100000, raw_pointer_size);
    encoder.Reset(1u, 0u);
    ASSERT_FALSE(encoder.TryMerge(5u, raw_pointer_size + 1));       // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(14u, raw_pointer_size));          // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(5u, raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(IndexBssMappingLookup::npos,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 17u, raw_pointer_size));
    ASSERT_FALSE(encoder.TryMerge(13u, 2 * raw_pointer_size + 1));  // Wrong bss_offset difference.
    ASSERT_FALSE(encoder.TryMerge(14u, 2 * raw_pointer_size));      // Index out of range.
    ASSERT_TRUE(encoder.TryMerge(13u, 2 * raw_pointer_size));
    ASSERT_EQ(0u, encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 1u, raw_pointer_size));
    ASSERT_EQ(raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 5u, raw_pointer_size));
    ASSERT_EQ(2 * raw_pointer_size,
              encoder.GetEntry().GetBssOffset(encoder.GetIndexBits(), 13u, raw_pointer_size));
    ASSERT_EQ(0x01100000u | 13u, encoder.GetEntry().index_and_mask);
    ASSERT_FALSE(encoder.TryMerge(14u, 3 * raw_pointer_size));      // Index out of range.
  }
}

}  // namespace linker
}  // namespace art
