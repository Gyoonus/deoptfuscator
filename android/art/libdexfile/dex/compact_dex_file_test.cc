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


#include "compact_dex_file.h"
#include "dex_file_loader.h"
#include "gtest/gtest.h"

namespace art {

TEST(CompactDexFileTest, MagicAndVersion) {
  // Test permutations of valid/invalid headers.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      static const size_t len = CompactDexFile::kDexVersionLen + CompactDexFile::kDexMagicSize;
      uint8_t header[len] = {};
      std::fill_n(header, len, 0x99);
      const bool valid_magic = (i & 1) == 0;
      const bool valid_version = (j & 1) == 0;
      if (valid_magic) {
        CompactDexFile::WriteMagic(header);
      }
      if (valid_version) {
        CompactDexFile::WriteCurrentVersion(header);
      }
      EXPECT_EQ(valid_magic, CompactDexFile::IsMagicValid(header));
      EXPECT_EQ(valid_version, CompactDexFile::IsVersionValid(header));
      EXPECT_EQ(valid_magic, DexFileLoader::IsMagicValid(header));
      EXPECT_EQ(valid_magic && valid_version, DexFileLoader::IsVersionAndMagicValid(header));
    }
  }
}

TEST(CompactDexFileTest, CodeItemFields) {
  auto test_and_write = [&] (uint16_t registers_size,
                             uint16_t ins_size,
                             uint16_t outs_size,
                             uint16_t tries_size,
                             uint32_t insns_size_in_code_units) {
    ASSERT_GE(registers_size, ins_size);
    uint16_t buffer[sizeof(CompactDexFile::CodeItem) +
                        CompactDexFile::CodeItem::kMaxPreHeaderSize] = {};
    CompactDexFile::CodeItem* code_item = reinterpret_cast<CompactDexFile::CodeItem*>(
        &buffer[CompactDexFile::CodeItem::kMaxPreHeaderSize]);
    const uint16_t* preheader_ptr = code_item->Create(registers_size,
                                                      ins_size,
                                                      outs_size,
                                                      tries_size,
                                                      insns_size_in_code_units,
                                                      code_item->GetPreHeader());
    ASSERT_GT(preheader_ptr, buffer);

    uint16_t out_registers_size;
    uint16_t out_ins_size;
    uint16_t out_outs_size;
    uint16_t out_tries_size;
    uint32_t out_insns_size_in_code_units;
    code_item->DecodeFields</*kDecodeOnlyInstructionCount*/false>(&out_insns_size_in_code_units,
                                                                  &out_registers_size,
                                                                  &out_ins_size,
                                                                  &out_outs_size,
                                                                  &out_tries_size);
    ASSERT_EQ(registers_size, out_registers_size);
    ASSERT_EQ(ins_size, out_ins_size);
    ASSERT_EQ(outs_size, out_outs_size);
    ASSERT_EQ(tries_size, out_tries_size);
    ASSERT_EQ(insns_size_in_code_units, out_insns_size_in_code_units);

    ++out_insns_size_in_code_units;  // Force value to change.
    code_item->DecodeFields</*kDecodeOnlyInstructionCount*/true>(&out_insns_size_in_code_units,
                                                                 /*registers_size*/ nullptr,
                                                                 /*ins_size*/ nullptr,
                                                                 /*outs_size*/ nullptr,
                                                                 /*tries_size*/ nullptr);
    ASSERT_EQ(insns_size_in_code_units, out_insns_size_in_code_units);
  };
  static constexpr uint32_t kMax32 = std::numeric_limits<uint32_t>::max();
  static constexpr uint16_t kMax16 = std::numeric_limits<uint16_t>::max();
  test_and_write(0, 0, 0, 0, 0);
  test_and_write(kMax16, kMax16, kMax16, kMax16, kMax32);
  test_and_write(kMax16 - 1, kMax16 - 2, kMax16 - 3, kMax16 - 4, kMax32 - 5);
  test_and_write(kMax16 - 4, kMax16 - 5, kMax16 - 3, kMax16 - 2, kMax32 - 1);
  test_and_write(5, 4, 3, 2, 1);
  test_and_write(5, 0, 3, 2, 1);
  test_and_write(kMax16, 0, kMax16 / 2, 1234, kMax32 / 4);
}

}  // namespace art
