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

#include "compiled_method.h"
#include "debug/method_debug_info.h"
#include "gtest/gtest.h"
#include "linker/linker_patch.h"
#include "linker/vector_output_stream.h"

namespace art {
namespace linker {

static const MethodReference kNullMethodRef = MethodReference(nullptr, 0u);

class MultiOatRelativePatcherTest : public testing::Test {
 protected:
  class MockPatcher : public RelativePatcher {
   public:
    MockPatcher() { }

    uint32_t ReserveSpace(uint32_t offset,
                          const CompiledMethod* compiled_method ATTRIBUTE_UNUSED,
                          MethodReference method_ref) OVERRIDE {
      last_reserve_offset_ = offset;
      last_reserve_method_ = method_ref;
      offset += next_reserve_adjustment_;
      next_reserve_adjustment_ = 0u;
      return offset;
    }

    uint32_t ReserveSpaceEnd(uint32_t offset) OVERRIDE {
      last_reserve_offset_ = offset;
      last_reserve_method_ = kNullMethodRef;
      offset += next_reserve_adjustment_;
      next_reserve_adjustment_ = 0u;
      return offset;
    }

    uint32_t WriteThunks(OutputStream* out, uint32_t offset) OVERRIDE {
      last_write_offset_ = offset;
      if (next_write_alignment_ != 0u) {
        offset += next_write_alignment_;
        bool success = WriteCodeAlignment(out, next_write_alignment_);
        CHECK(success);
        next_write_alignment_ = 0u;
      }
      if (next_write_call_thunk_ != 0u) {
        offset += next_write_call_thunk_;
        std::vector<uint8_t> thunk(next_write_call_thunk_, 'c');
        bool success = WriteThunk(out, ArrayRef<const uint8_t>(thunk));
        CHECK(success);
        next_write_call_thunk_ = 0u;
      }
      if (next_write_misc_thunk_ != 0u) {
        offset += next_write_misc_thunk_;
        std::vector<uint8_t> thunk(next_write_misc_thunk_, 'm');
        bool success = WriteMiscThunk(out, ArrayRef<const uint8_t>(thunk));
        CHECK(success);
        next_write_misc_thunk_ = 0u;
      }
      return offset;
    }

    void PatchCall(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                   uint32_t literal_offset,
                   uint32_t patch_offset,
                   uint32_t target_offset) OVERRIDE {
      last_literal_offset_ = literal_offset;
      last_patch_offset_ = patch_offset;
      last_target_offset_ = target_offset;
    }

    void PatchPcRelativeReference(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                  const LinkerPatch& patch,
                                  uint32_t patch_offset,
                                  uint32_t target_offset) OVERRIDE {
      last_literal_offset_ = patch.LiteralOffset();
      last_patch_offset_ = patch_offset;
      last_target_offset_ = target_offset;
    }

    void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                     const LinkerPatch& patch ATTRIBUTE_UNUSED,
                                     uint32_t patch_offset ATTRIBUTE_UNUSED) {
      LOG(FATAL) << "UNIMPLEMENTED";
    }

    std::vector<debug::MethodDebugInfo> GenerateThunkDebugInfo(
        uint32_t executable_offset ATTRIBUTE_UNUSED) {
      LOG(FATAL) << "UNIMPLEMENTED";
      UNREACHABLE();
    }

    uint32_t last_reserve_offset_ = 0u;
    MethodReference last_reserve_method_ = kNullMethodRef;
    uint32_t next_reserve_adjustment_ = 0u;

    uint32_t last_write_offset_ = 0u;
    uint32_t next_write_alignment_ = 0u;
    uint32_t next_write_call_thunk_ = 0u;
    uint32_t next_write_misc_thunk_ = 0u;

    uint32_t last_literal_offset_ = 0u;
    uint32_t last_patch_offset_ = 0u;
    uint32_t last_target_offset_ = 0u;
  };

  MultiOatRelativePatcherTest()
      : instruction_set_features_(InstructionSetFeatures::FromCppDefines()),
        patcher_(kRuntimeISA, instruction_set_features_.get()) {
    std::unique_ptr<MockPatcher> mock(new MockPatcher());
    mock_ = mock.get();
    patcher_.relative_patcher_ = std::move(mock);
  }

  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;
  MultiOatRelativePatcher patcher_;
  MockPatcher* mock_;
};

TEST_F(MultiOatRelativePatcherTest, Offsets) {
  const DexFile* dex_file = reinterpret_cast<const DexFile*>(1);
  MethodReference ref1(dex_file, 1u);
  MethodReference ref2(dex_file, 2u);
  EXPECT_EQ(0u, patcher_.GetOffset(ref1));
  EXPECT_EQ(0u, patcher_.GetOffset(ref2));

  uint32_t adjustment1 = 0x1000;
  patcher_.StartOatFile(adjustment1);
  EXPECT_EQ(0u, patcher_.GetOffset(ref1));
  EXPECT_EQ(0u, patcher_.GetOffset(ref2));

  uint32_t off1 = 0x1234;
  patcher_.SetOffset(ref1, off1);
  EXPECT_EQ(off1, patcher_.GetOffset(ref1));
  EXPECT_EQ(0u, patcher_.GetOffset(ref2));

  uint32_t adjustment2 = 0x30000;
  patcher_.StartOatFile(adjustment2);
  EXPECT_EQ(off1 + adjustment1 - adjustment2, patcher_.GetOffset(ref1));
  EXPECT_EQ(0u, patcher_.GetOffset(ref2));

  uint32_t off2 = 0x4321;
  patcher_.SetOffset(ref2, off2);
  EXPECT_EQ(off1 + adjustment1 - adjustment2, patcher_.GetOffset(ref1));
  EXPECT_EQ(off2, patcher_.GetOffset(ref2));

  uint32_t adjustment3 = 0x78000;
  patcher_.StartOatFile(adjustment3);
  EXPECT_EQ(off1 + adjustment1 - adjustment3, patcher_.GetOffset(ref1));
  EXPECT_EQ(off2 + adjustment2 - adjustment3, patcher_.GetOffset(ref2));
}

TEST_F(MultiOatRelativePatcherTest, OffsetsInReserve) {
  const DexFile* dex_file = reinterpret_cast<const DexFile*>(1);
  MethodReference ref1(dex_file, 1u);
  MethodReference ref2(dex_file, 2u);
  MethodReference ref3(dex_file, 3u);
  const CompiledMethod* method = reinterpret_cast<const CompiledMethod*>(-1);

  uint32_t adjustment1 = 0x1000;
  patcher_.StartOatFile(adjustment1);

  uint32_t method1_offset = 0x100;
  uint32_t method1_offset_check = patcher_.ReserveSpace(method1_offset, method, ref1);
  ASSERT_EQ(adjustment1 + method1_offset, mock_->last_reserve_offset_);
  ASSERT_TRUE(ref1 == mock_->last_reserve_method_);
  ASSERT_EQ(method1_offset, method1_offset_check);

  uint32_t method2_offset = 0x1230;
  uint32_t method2_reserve_adjustment = 0x10;
  mock_->next_reserve_adjustment_ = method2_reserve_adjustment;
  uint32_t method2_offset_adjusted = patcher_.ReserveSpace(method2_offset, method, ref2);
  ASSERT_EQ(adjustment1 + method2_offset, mock_->last_reserve_offset_);
  ASSERT_TRUE(ref2 == mock_->last_reserve_method_);
  ASSERT_EQ(method2_offset + method2_reserve_adjustment, method2_offset_adjusted);

  uint32_t end1_offset = 0x4320;
  uint32_t end1_offset_check = patcher_.ReserveSpaceEnd(end1_offset);
  ASSERT_EQ(adjustment1 + end1_offset, mock_->last_reserve_offset_);
  ASSERT_TRUE(kNullMethodRef == mock_->last_reserve_method_);
  ASSERT_EQ(end1_offset, end1_offset_check);

  uint32_t adjustment2 = 0xd000;
  patcher_.StartOatFile(adjustment2);

  uint32_t method3_offset = 0xf00;
  uint32_t method3_offset_check = patcher_.ReserveSpace(method3_offset, method, ref3);
  ASSERT_EQ(adjustment2 + method3_offset, mock_->last_reserve_offset_);
  ASSERT_TRUE(ref3 == mock_->last_reserve_method_);
  ASSERT_EQ(method3_offset, method3_offset_check);

  uint32_t end2_offset = 0x2400;
  uint32_t end2_reserve_adjustment = 0x20;
  mock_->next_reserve_adjustment_ = end2_reserve_adjustment;
  uint32_t end2_offset_adjusted = patcher_.ReserveSpaceEnd(end2_offset);
  ASSERT_EQ(adjustment2 + end2_offset, mock_->last_reserve_offset_);
  ASSERT_TRUE(kNullMethodRef == mock_->last_reserve_method_);
  ASSERT_EQ(end2_offset + end2_reserve_adjustment, end2_offset_adjusted);
}

TEST_F(MultiOatRelativePatcherTest, Write) {
  std::vector<uint8_t> output;
  VectorOutputStream vos("output", &output);

  uint32_t adjustment1 = 0x1000;
  patcher_.StartOatFile(adjustment1);

  uint32_t method1_offset = 0x100;
  uint32_t method1_offset_check = patcher_.WriteThunks(&vos, method1_offset);
  ASSERT_EQ(adjustment1 + method1_offset, mock_->last_write_offset_);
  ASSERT_EQ(method1_offset, method1_offset_check);
  vos.WriteFully("1", 1);  // Mark method1.

  uint32_t method2_offset = 0x1230;
  uint32_t method2_alignment_size = 1;
  uint32_t method2_call_thunk_size = 2;
  mock_->next_write_alignment_ = method2_alignment_size;
  mock_->next_write_call_thunk_ = method2_call_thunk_size;
  uint32_t method2_offset_adjusted = patcher_.WriteThunks(&vos, method2_offset);
  ASSERT_EQ(adjustment1 + method2_offset, mock_->last_write_offset_);
  ASSERT_EQ(method2_offset + method2_alignment_size + method2_call_thunk_size,
            method2_offset_adjusted);
  vos.WriteFully("2", 1);  // Mark method2.

  EXPECT_EQ(method2_alignment_size, patcher_.CodeAlignmentSize());
  EXPECT_EQ(method2_call_thunk_size, patcher_.RelativeCallThunksSize());

  uint32_t adjustment2 = 0xd000;
  patcher_.StartOatFile(adjustment2);

  uint32_t method3_offset = 0xf00;
  uint32_t method3_alignment_size = 2;
  uint32_t method3_misc_thunk_size = 1;
  mock_->next_write_alignment_ = method3_alignment_size;
  mock_->next_write_misc_thunk_ = method3_misc_thunk_size;
  uint32_t method3_offset_adjusted = patcher_.WriteThunks(&vos, method3_offset);
  ASSERT_EQ(adjustment2 + method3_offset, mock_->last_write_offset_);
  ASSERT_EQ(method3_offset + method3_alignment_size + method3_misc_thunk_size,
            method3_offset_adjusted);
  vos.WriteFully("3", 1);  // Mark method3.

  EXPECT_EQ(method3_alignment_size, patcher_.CodeAlignmentSize());
  EXPECT_EQ(method3_misc_thunk_size, patcher_.MiscThunksSize());

  uint8_t expected_output[] = {
      '1',
      0, 'c', 'c', '2',
      0, 0, 'm', '3',
  };
  ASSERT_EQ(arraysize(expected_output), output.size());
  for (size_t i = 0; i != arraysize(expected_output); ++i) {
    ASSERT_EQ(expected_output[i], output[i]) << i;
  }
}

TEST_F(MultiOatRelativePatcherTest, Patch) {
  std::vector<uint8_t> code(16);

  uint32_t adjustment1 = 0x1000;
  patcher_.StartOatFile(adjustment1);

  uint32_t method1_literal_offset = 4u;
  uint32_t method1_patch_offset = 0x1234u;
  uint32_t method1_target_offset = 0x8888u;
  patcher_.PatchCall(&code, method1_literal_offset, method1_patch_offset, method1_target_offset);
  DCHECK_EQ(method1_literal_offset, mock_->last_literal_offset_);
  DCHECK_EQ(method1_patch_offset + adjustment1, mock_->last_patch_offset_);
  DCHECK_EQ(method1_target_offset + adjustment1, mock_->last_target_offset_);

  uint32_t method2_literal_offset = 12u;
  uint32_t method2_patch_offset = 0x7654u;
  uint32_t method2_target_offset = 0xccccu;
  LinkerPatch method2_patch =
      LinkerPatch::StringBssEntryPatch(method2_literal_offset, nullptr, 0u, 1u);
  patcher_.PatchPcRelativeReference(
      &code, method2_patch, method2_patch_offset, method2_target_offset);
  DCHECK_EQ(method2_literal_offset, mock_->last_literal_offset_);
  DCHECK_EQ(method2_patch_offset + adjustment1, mock_->last_patch_offset_);
  DCHECK_EQ(method2_target_offset + adjustment1, mock_->last_target_offset_);

  uint32_t adjustment2 = 0xd000;
  patcher_.StartOatFile(adjustment2);

  uint32_t method3_literal_offset = 8u;
  uint32_t method3_patch_offset = 0x108u;
  uint32_t method3_target_offset = 0x200u;
  patcher_.PatchCall(&code, method3_literal_offset, method3_patch_offset, method3_target_offset);
  DCHECK_EQ(method3_literal_offset, mock_->last_literal_offset_);
  DCHECK_EQ(method3_patch_offset + adjustment2, mock_->last_patch_offset_);
  DCHECK_EQ(method3_target_offset + adjustment2, mock_->last_target_offset_);
}

}  // namespace linker
}  // namespace art
