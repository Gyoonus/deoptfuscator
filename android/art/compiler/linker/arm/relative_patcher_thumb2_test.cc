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

#include "linker/arm/relative_patcher_thumb2.h"

#include "base/casts.h"
#include "linker/relative_patcher_test.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object.h"
#include "oat_quick_method_header.h"

namespace art {
namespace linker {

class Thumb2RelativePatcherTest : public RelativePatcherTest {
 public:
  Thumb2RelativePatcherTest() : RelativePatcherTest(InstructionSet::kThumb2, "default") { }

 protected:
  static const uint8_t kCallRawCode[];
  static const ArrayRef<const uint8_t> kCallCode;
  static const uint8_t kNopRawCode[];
  static const ArrayRef<const uint8_t> kNopCode;
  static const uint8_t kUnpatchedPcRelativeRawCode[];
  static const ArrayRef<const uint8_t> kUnpatchedPcRelativeCode;
  static const uint32_t kPcInsnOffset;

  // The PC in Thumb mode is 4 bytes after the instruction location.
  static constexpr uint32_t kPcAdjustment = 4u;

  // Branches within range [-256, 256) can be created from these by adding the low 8 bits.
  static constexpr uint32_t kBlPlus0 = 0xf000f800u;
  static constexpr uint32_t kBlMinus256 = 0xf7ffff00u;

  // Special BL values.
  static constexpr uint32_t kBlPlusMax = 0xf3ffd7ffu;
  static constexpr uint32_t kBlMinusMax = 0xf400d000u;

  // BNE +0, 32-bit, encoding T3. Bits 0-10, 11, 13, 16-21, 26 are placeholder for target offset.
  static constexpr uint32_t kBneWPlus0 = 0xf0408000u;

  // LDR immediate, 16-bit, encoding T1. Bits 6-10 are imm5, 0-2 are Rt, 3-5 are Rn.
  static constexpr uint32_t kLdrInsn = 0x6800u;

  // LDR immediate, 32-bit, encoding T3. Bits 0-11 are offset, 12-15 are Rt, 16-20 are Rn.
  static constexpr uint32_t kLdrWInsn = 0xf8d00000u;

  // LDR immediate, negative offset, encoding T4. Bits 0-7 are the offset to subtract.
  static constexpr uint32_t kLdrNegativeOffset = 0xf8500c00u;

  // LDR register, lsl #2. Bits 4-5 are the imm2, i.e. the lsl shift.
  static constexpr uint32_t kLdrRegLsl2 = 0xf8500020u;

  // NOP instructions.
  static constexpr uint32_t kNopInsn = 0xbf00u;
  static constexpr uint32_t kNopWInsn = 0xf3af8000u;

  void InsertInsn(std::vector<uint8_t>* code, size_t pos, uint32_t insn) {
    CHECK_LE(pos, code->size());
    if (IsUint<16>(insn)) {
      const uint8_t insn_code[] = {
          static_cast<uint8_t>(insn),
          static_cast<uint8_t>(insn >> 8),
      };
      static_assert(sizeof(insn_code) == 2u, "Invalid sizeof(insn_code).");
      code->insert(code->begin() + pos, insn_code, insn_code + sizeof(insn_code));
    } else {
      const uint8_t insn_code[] = {
          static_cast<uint8_t>(insn >> 16),
          static_cast<uint8_t>(insn >> 24),
          static_cast<uint8_t>(insn),
          static_cast<uint8_t>(insn >> 8),
      };
      static_assert(sizeof(insn_code) == 4u, "Invalid sizeof(insn_code).");
      code->insert(code->begin() + pos, insn_code, insn_code + sizeof(insn_code));
    }
  }

  void PushBackInsn(std::vector<uint8_t>* code, uint32_t insn) {
    InsertInsn(code, code->size(), insn);
  }

  std::vector<uint8_t> GenNops(size_t num_nops) {
    std::vector<uint8_t> result;
    result.reserve(num_nops * 2u);
    for (size_t i = 0; i != num_nops; ++i) {
      PushBackInsn(&result, kNopInsn);
    }
    return result;
  }

  std::vector<uint8_t> RawCode(std::initializer_list<uint32_t> insns) {
    std::vector<uint8_t> raw_code;
    size_t number_of_16_bit_insns =
        std::count_if(insns.begin(), insns.end(), [](uint32_t x) { return IsUint<16>(x); });
    raw_code.reserve(insns.size() * 4u - number_of_16_bit_insns * 2u);
    for (uint32_t insn : insns) {
      PushBackInsn(&raw_code, insn);
    }
    return raw_code;
  }

  uint32_t BneWWithOffset(uint32_t bne_offset, uint32_t target_offset) {
    if (!IsAligned<2u>(bne_offset)) {
      LOG(ERROR) << "Unaligned bne_offset: " << bne_offset;
      return 0xffffffffu;  // Fails code diff later.
    }
    if (!IsAligned<2u>(target_offset)) {
      LOG(ERROR) << "Unaligned target_offset: " << target_offset;
      return 0xffffffffu;  // Fails code diff later.
    }
    uint32_t diff = target_offset - bne_offset - kPcAdjustment;
    DCHECK_ALIGNED(diff, 2u);
    if ((diff >> 20) != 0 && (diff >> 20) != 0xfffu) {
      LOG(ERROR) << "Target out of range: " << diff;
      return 0xffffffffu;  // Fails code diff later.
    }
    return kBneWPlus0 | ((diff >> 1) & 0x7ffu)          // imm11
                      | (((diff >> 12) & 0x3fu) << 16)  // imm6
                      | (((diff >> 18) & 1) << 13)      // J1
                      | (((diff >> 19) & 1) << 11)      // J2
                      | (((diff >> 20) & 1) << 26);     // S
  }

  bool Create2MethodsWithGap(const ArrayRef<const uint8_t>& method1_code,
                             const ArrayRef<const LinkerPatch>& method1_patches,
                             const ArrayRef<const uint8_t>& method3_code,
                             const ArrayRef<const LinkerPatch>& method3_patches,
                             uint32_t distance_without_thunks) {
    CHECK_EQ(distance_without_thunks % kArmAlignment, 0u);
    uint32_t method1_offset =
        kTrampolineSize + CodeAlignmentSize(kTrampolineSize) + sizeof(OatQuickMethodHeader);
    AddCompiledMethod(MethodRef(1u), method1_code, method1_patches);

    // We want to put the method3 at a very precise offset.
    const uint32_t method3_offset = method1_offset + distance_without_thunks;
    CHECK_ALIGNED(method3_offset, kArmAlignment);

    // Calculate size of method2 so that we put method3 at the correct place.
    const uint32_t method1_end = method1_offset + method1_code.size();
    const uint32_t method2_offset =
        method1_end + CodeAlignmentSize(method1_end) + sizeof(OatQuickMethodHeader);
    const uint32_t method2_size = (method3_offset - sizeof(OatQuickMethodHeader) - method2_offset);
    std::vector<uint8_t> method2_raw_code(method2_size);
    ArrayRef<const uint8_t> method2_code(method2_raw_code);
    AddCompiledMethod(MethodRef(2u), method2_code);

    AddCompiledMethod(MethodRef(3u), method3_code, method3_patches);

    Link();

    // Check assumptions.
    CHECK_EQ(GetMethodOffset(1), method1_offset);
    CHECK_EQ(GetMethodOffset(2), method2_offset);
    auto result3 = method_offset_map_.FindMethodOffset(MethodRef(3));
    CHECK(result3.first);
    // There may be a thunk before method2.
    if (result3.second == method3_offset + 1 /* thumb mode */) {
      return false;  // No thunk.
    } else {
      uint32_t thunk_end =
          CompiledCode::AlignCode(method3_offset - sizeof(OatQuickMethodHeader),
                                  InstructionSet::kThumb2) +
          MethodCallThunkSize();
      uint32_t header_offset = thunk_end + CodeAlignmentSize(thunk_end);
      CHECK_EQ(result3.second, header_offset + sizeof(OatQuickMethodHeader) + 1 /* thumb mode */);
      return true;   // Thunk present.
    }
  }

  uint32_t GetMethodOffset(uint32_t method_idx) {
    auto result = method_offset_map_.FindMethodOffset(MethodRef(method_idx));
    CHECK(result.first);
    CHECK_NE(result.second & 1u, 0u);
    return result.second - 1 /* thumb mode */;
  }

  std::vector<uint8_t> CompileMethodCallThunk() {
    ArmBaseRelativePatcher::ThunkKey key = ArmBaseRelativePatcher::GetMethodCallKey();
    return static_cast<Thumb2RelativePatcher*>(patcher_.get())->CompileThunk(key);
  }

  uint32_t MethodCallThunkSize() {
    return CompileMethodCallThunk().size();
  }

  bool CheckThunk(uint32_t thunk_offset) {
    const std::vector<uint8_t> expected_code = CompileMethodCallThunk();
    if (output_.size() < thunk_offset + expected_code.size()) {
      LOG(ERROR) << "output_.size() == " << output_.size() << " < "
          << "thunk_offset + expected_code.size() == " << (thunk_offset + expected_code.size());
      return false;
    }
    ArrayRef<const uint8_t> linked_code(&output_[thunk_offset], expected_code.size());
    if (linked_code == ArrayRef<const uint8_t>(expected_code)) {
      return true;
    }
    // Log failure info.
    DumpDiff(ArrayRef<const uint8_t>(expected_code), linked_code);
    return false;
  }

  std::vector<uint8_t> GenNopsAndBl(size_t num_nops, uint32_t bl) {
    std::vector<uint8_t> result;
    result.reserve(num_nops * 2u + 4u);
    for (size_t i = 0; i != num_nops; ++i) {
      PushBackInsn(&result, kNopInsn);
    }
    PushBackInsn(&result, bl);
    return result;
  }

  void TestStringBssEntry(uint32_t bss_begin, uint32_t string_entry_offset);
  void TestStringReference(uint32_t string_offset);
  void CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches, uint32_t target_offset);

  std::vector<uint8_t> CompileBakerOffsetThunk(uint32_t base_reg,
                                               uint32_t holder_reg,
                                               bool narrow) {
    const LinkerPatch patch = LinkerPatch::BakerReadBarrierBranchPatch(
        0u, Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(base_reg, holder_reg, narrow));
    ArmBaseRelativePatcher::ThunkKey key = ArmBaseRelativePatcher::GetBakerThunkKey(patch);
    return down_cast<Thumb2RelativePatcher*>(patcher_.get())->CompileThunk(key);
  }

  std::vector<uint8_t> CompileBakerArrayThunk(uint32_t base_reg) {
    LinkerPatch patch = LinkerPatch::BakerReadBarrierBranchPatch(
        0u, Thumb2RelativePatcher::EncodeBakerReadBarrierArrayData(base_reg));
    ArmBaseRelativePatcher::ThunkKey key = ArmBaseRelativePatcher::GetBakerThunkKey(patch);
    return down_cast<Thumb2RelativePatcher*>(patcher_.get())->CompileThunk(key);
  }

  std::vector<uint8_t> CompileBakerGcRootThunk(uint32_t root_reg, bool narrow) {
    LinkerPatch patch = LinkerPatch::BakerReadBarrierBranchPatch(
        0u, Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(root_reg, narrow));
    ArmBaseRelativePatcher::ThunkKey key = ArmBaseRelativePatcher::GetBakerThunkKey(patch);
    return down_cast<Thumb2RelativePatcher*>(patcher_.get())->CompileThunk(key);
  }

  uint32_t GetOutputInsn32(uint32_t offset) {
    CHECK_LE(offset, output_.size());
    CHECK_GE(output_.size() - offset, 4u);
    return (static_cast<uint32_t>(output_[offset]) << 16) |
           (static_cast<uint32_t>(output_[offset + 1]) << 24) |
           (static_cast<uint32_t>(output_[offset + 2]) << 0) |
           (static_cast<uint32_t>(output_[offset + 3]) << 8);
  }

  uint16_t GetOutputInsn16(uint32_t offset) {
    CHECK_LE(offset, output_.size());
    CHECK_GE(output_.size() - offset, 2u);
    return (static_cast<uint32_t>(output_[offset]) << 0) |
           (static_cast<uint32_t>(output_[offset + 1]) << 8);
  }

  void TestBakerFieldWide(uint32_t offset, uint32_t ref_reg);
  void TestBakerFieldNarrow(uint32_t offset, uint32_t ref_reg);
};

const uint8_t Thumb2RelativePatcherTest::kCallRawCode[] = {
    0x00, 0xf0, 0x00, 0xf8
};

const ArrayRef<const uint8_t> Thumb2RelativePatcherTest::kCallCode(kCallRawCode);

const uint8_t Thumb2RelativePatcherTest::kNopRawCode[] = {
    0x00, 0xbf
};

const ArrayRef<const uint8_t> Thumb2RelativePatcherTest::kNopCode(kNopRawCode);

const uint8_t Thumb2RelativePatcherTest::kUnpatchedPcRelativeRawCode[] = {
    0x40, 0xf2, 0x00, 0x00,   // MOVW r0, #0 (placeholder)
    0xc0, 0xf2, 0x00, 0x00,   // MOVT r0, #0 (placeholder)
    0x78, 0x44,               // ADD r0, pc
};
const ArrayRef<const uint8_t> Thumb2RelativePatcherTest::kUnpatchedPcRelativeCode(
    kUnpatchedPcRelativeRawCode);
const uint32_t Thumb2RelativePatcherTest::kPcInsnOffset = 8u;

void Thumb2RelativePatcherTest::TestStringBssEntry(uint32_t bss_begin,
                                                   uint32_t string_entry_offset) {
  constexpr uint32_t kStringIndex = 1u;
  string_index_to_offset_map_.Put(kStringIndex, string_entry_offset);
  bss_begin_ = bss_begin;
  const LinkerPatch patches[] = {
      LinkerPatch::StringBssEntryPatch(0u, nullptr, kPcInsnOffset, kStringIndex),
      LinkerPatch::StringBssEntryPatch(4u, nullptr, kPcInsnOffset, kStringIndex),
  };
  CheckPcRelativePatch(ArrayRef<const LinkerPatch>(patches), bss_begin_ + string_entry_offset);
}

void Thumb2RelativePatcherTest::TestStringReference(uint32_t string_offset) {
  constexpr uint32_t kStringIndex = 1u;
  string_index_to_offset_map_.Put(kStringIndex, string_offset);
  const LinkerPatch patches[] = {
      LinkerPatch::RelativeStringPatch(0u, nullptr, kPcInsnOffset, kStringIndex),
      LinkerPatch::RelativeStringPatch(4u, nullptr, kPcInsnOffset, kStringIndex),
  };
  CheckPcRelativePatch(ArrayRef<const LinkerPatch>(patches), string_offset);
}

void Thumb2RelativePatcherTest::CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches,
                                                     uint32_t target_offset) {
  AddCompiledMethod(MethodRef(1u), kUnpatchedPcRelativeCode, ArrayRef<const LinkerPatch>(patches));
  Link();

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t pc_base_offset = method1_offset + kPcInsnOffset + 4u /* PC adjustment */;
  uint32_t diff = target_offset - pc_base_offset;
  // Distribute the bits of the diff between the MOVW and MOVT:
  uint32_t diffw = diff & 0xffffu;
  uint32_t difft = diff >> 16;
  uint32_t movw = 0xf2400000u |           // MOVW r0, #0 (placeholder),
      ((diffw & 0xf000u) << (16 - 12)) |  // move imm4 from bits 12-15 to bits 16-19,
      ((diffw & 0x0800u) << (26 - 11)) |  // move imm from bit 11 to bit 26,
      ((diffw & 0x0700u) << (12 - 8)) |   // move imm3 from bits 8-10 to bits 12-14,
      ((diffw & 0x00ffu));                // keep imm8 at bits 0-7.
  uint32_t movt = 0xf2c00000u |           // MOVT r0, #0 (placeholder),
      ((difft & 0xf000u) << (16 - 12)) |  // move imm4 from bits 12-15 to bits 16-19,
      ((difft & 0x0800u) << (26 - 11)) |  // move imm from bit 11 to bit 26,
      ((difft & 0x0700u) << (12 - 8)) |   // move imm3 from bits 8-10 to bits 12-14,
      ((difft & 0x00ffu));                // keep imm8 at bits 0-7.
  const uint8_t expected_code[] = {
      static_cast<uint8_t>(movw >> 16), static_cast<uint8_t>(movw >> 24),
      static_cast<uint8_t>(movw >> 0), static_cast<uint8_t>(movw >> 8),
      static_cast<uint8_t>(movt >> 16), static_cast<uint8_t>(movt >> 24),
      static_cast<uint8_t>(movt >> 0), static_cast<uint8_t>(movt >> 8),
      0x78, 0x44,
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallSelf) {
  const LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<const LinkerPatch>(patches));
  Link();

  static const uint8_t expected_code[] = {
      0xff, 0xf7, 0xfe, 0xff
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOther) {
  const LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 2u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<const LinkerPatch>(method1_patches));
  const LinkerPatch method2_patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 1u),
  };
  AddCompiledMethod(MethodRef(2u), kCallCode, ArrayRef<const LinkerPatch>(method2_patches));
  Link();

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t method2_offset = GetMethodOffset(2u);
  uint32_t diff_after = method2_offset - (method1_offset + 4u /* PC adjustment */);
  ASSERT_EQ(diff_after & 1u, 0u);
  ASSERT_LT(diff_after >> 1, 1u << 8);  // Simple encoding, (diff_after >> 1) fits into 8 bits.
  static const uint8_t method1_expected_code[] = {
      0x00, 0xf0, static_cast<uint8_t>(diff_after >> 1), 0xf8
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(method1_expected_code)));
  uint32_t diff_before = method1_offset - (method2_offset + 4u /* PC adjustment */);
  ASSERT_EQ(diff_before & 1u, 0u);
  ASSERT_GE(diff_before, -1u << 9);  // Simple encoding, -256 <= (diff >> 1) < 0.
  auto method2_expected_code = GenNopsAndBl(0u, kBlMinus256 | ((diff_before >> 1) & 0xffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(2u), ArrayRef<const uint8_t>(method2_expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallTrampoline) {
  const LinkerPatch patches[] = {
      LinkerPatch::RelativeCodePatch(0u, nullptr, 2u),
  };
  AddCompiledMethod(MethodRef(1u), kCallCode, ArrayRef<const LinkerPatch>(patches));
  Link();

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t diff = kTrampolineOffset - (method1_offset + 4u);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_GE(diff, -1u << 9);  // Simple encoding, -256 <= (diff >> 1) < 0 (checked as unsigned).
  auto expected_code = GenNopsAndBl(0u, kBlMinus256 | ((diff >> 1) & 0xffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallTrampolineTooFar) {
  constexpr uint32_t missing_method_index = 1024u;
  auto method3_raw_code = GenNopsAndBl(3u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method3 = 3u * 2u;  // After NOPs.
  ArrayRef<const uint8_t> method3_code(method3_raw_code);
  ASSERT_EQ(bl_offset_in_method3 + 4u, method3_code.size());
  const LinkerPatch method3_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method3, nullptr, missing_method_index),
  };

  constexpr uint32_t just_over_max_negative_disp = 16 * MB + 2 - 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(kNopCode,
                                            ArrayRef<const LinkerPatch>(),
                                            method3_code,
                                            ArrayRef<const LinkerPatch>(method3_patches),
                                            just_over_max_negative_disp - bl_offset_in_method3);
  ASSERT_FALSE(thunk_in_gap);  // There should be a thunk but it should be after the method2.
  ASSERT_FALSE(method_offset_map_.FindMethodOffset(MethodRef(missing_method_index)).first);

  // Check linked code.
  uint32_t method3_offset = GetMethodOffset(3u);
  uint32_t thunk_offset = CompiledCode::AlignCode(method3_offset + method3_code.size(),
                                                  InstructionSet::kThumb2);
  uint32_t diff = thunk_offset - (method3_offset + bl_offset_in_method3 + 4u /* PC adjustment */);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_LT(diff >> 1, 1u << 8);  // Simple encoding, (diff >> 1) fits into 8 bits.
  auto expected_code = GenNopsAndBl(3u, kBlPlus0 | ((diff >> 1) & 0xffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(3u), ArrayRef<const uint8_t>(expected_code)));
  EXPECT_TRUE(CheckThunk(thunk_offset));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherAlmostTooFarAfter) {
  auto method1_raw_code = GenNopsAndBl(3u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method1 = 3u * 2u;  // After NOPs.
  ArrayRef<const uint8_t> method1_code(method1_raw_code);
  ASSERT_EQ(bl_offset_in_method1 + 4u, method1_code.size());
  const LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method1, nullptr, 3u),
  };

  constexpr uint32_t max_positive_disp = 16 * MB - 2u + 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method1_code,
                                            ArrayRef<const LinkerPatch>(method1_patches),
                                            kNopCode,
                                            ArrayRef<const LinkerPatch>(),
                                            bl_offset_in_method1 + max_positive_disp);
  ASSERT_FALSE(thunk_in_gap);  // There should be no thunk.

  // Check linked code.
  auto expected_code = GenNopsAndBl(3u, kBlPlusMax);
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherAlmostTooFarBefore) {
  auto method3_raw_code = GenNopsAndBl(2u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method3 = 2u * 2u;  // After NOPs.
  ArrayRef<const uint8_t> method3_code(method3_raw_code);
  ASSERT_EQ(bl_offset_in_method3 + 4u, method3_code.size());
  const LinkerPatch method3_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method3, nullptr, 1u),
  };

  constexpr uint32_t just_over_max_negative_disp = 16 * MB - 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(kNopCode,
                                            ArrayRef<const LinkerPatch>(),
                                            method3_code,
                                            ArrayRef<const LinkerPatch>(method3_patches),
                                            just_over_max_negative_disp - bl_offset_in_method3);
  ASSERT_FALSE(thunk_in_gap);  // There should be no thunk.

  // Check linked code.
  auto expected_code = GenNopsAndBl(2u, kBlMinusMax);
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(3u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, CallOtherJustTooFarAfter) {
  auto method1_raw_code = GenNopsAndBl(2u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method1 = 2u * 2u;  // After NOPs.
  ArrayRef<const uint8_t> method1_code(method1_raw_code);
  ASSERT_EQ(bl_offset_in_method1 + 4u, method1_code.size());
  const LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method1, nullptr, 3u),
  };

  constexpr uint32_t just_over_max_positive_disp = 16 * MB + 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(method1_code,
                                            ArrayRef<const LinkerPatch>(method1_patches),
                                            kNopCode,
                                            ArrayRef<const LinkerPatch>(),
                                            bl_offset_in_method1 + just_over_max_positive_disp);
  ASSERT_TRUE(thunk_in_gap);

  uint32_t method1_offset = GetMethodOffset(1u);
  uint32_t method3_offset = GetMethodOffset(3u);
  ASSERT_TRUE(IsAligned<kArmAlignment>(method3_offset));
  uint32_t method3_header_offset = method3_offset - sizeof(OatQuickMethodHeader);
  uint32_t thunk_size = MethodCallThunkSize();
  uint32_t thunk_offset = RoundDown(method3_header_offset - thunk_size, kArmAlignment);
  DCHECK_EQ(thunk_offset + thunk_size + CodeAlignmentSize(thunk_offset + thunk_size),
            method3_header_offset);
  ASSERT_TRUE(IsAligned<kArmAlignment>(thunk_offset));
  uint32_t diff = thunk_offset - (method1_offset + bl_offset_in_method1 + 4u /* PC adjustment */);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_GE(diff, 16 * MB - (1u << 9));  // Simple encoding, unknown bits fit into the low 8 bits.
  auto expected_code = GenNopsAndBl(2u, 0xf3ffd700 | ((diff >> 1) & 0xffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
  CheckThunk(thunk_offset);
}

TEST_F(Thumb2RelativePatcherTest, CallOtherJustTooFarBefore) {
  auto method3_raw_code = GenNopsAndBl(3u, kBlPlus0);
  constexpr uint32_t bl_offset_in_method3 = 3u * 2u;  // After NOPs.
  ArrayRef<const uint8_t> method3_code(method3_raw_code);
  ASSERT_EQ(bl_offset_in_method3 + 4u, method3_code.size());
  const LinkerPatch method3_patches[] = {
      LinkerPatch::RelativeCodePatch(bl_offset_in_method3, nullptr, 1u),
  };

  constexpr uint32_t just_over_max_negative_disp = 16 * MB + 2 - 4u /* PC adjustment */;
  bool thunk_in_gap = Create2MethodsWithGap(kNopCode,
                                            ArrayRef<const LinkerPatch>(),
                                            method3_code,
                                            ArrayRef<const LinkerPatch>(method3_patches),
                                            just_over_max_negative_disp - bl_offset_in_method3);
  ASSERT_FALSE(thunk_in_gap);  // There should be a thunk but it should be after the method2.

  // Check linked code.
  uint32_t method3_offset = GetMethodOffset(3u);
  uint32_t thunk_offset = CompiledCode::AlignCode(method3_offset + method3_code.size(),
                                                  InstructionSet::kThumb2);
  uint32_t diff = thunk_offset - (method3_offset + bl_offset_in_method3 + 4u /* PC adjustment */);
  ASSERT_EQ(diff & 1u, 0u);
  ASSERT_LT(diff >> 1, 1u << 8);  // Simple encoding, (diff >> 1) fits into 8 bits.
  auto expected_code = GenNopsAndBl(3u, kBlPlus0 | ((diff >> 1) & 0xffu));
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(3u), ArrayRef<const uint8_t>(expected_code)));
  EXPECT_TRUE(CheckThunk(thunk_offset));
}

TEST_F(Thumb2RelativePatcherTest, StringBssEntry1) {
  TestStringBssEntry(0x00ff0000u, 0x00fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringBssEntry2) {
  TestStringBssEntry(0x02ff0000u, 0x05fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringBssEntry3) {
  TestStringBssEntry(0x08ff0000u, 0x08fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringBssEntry4) {
  TestStringBssEntry(0xd0ff0000u, 0x60fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringReference1) {
  TestStringReference(0x00ff00fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringReference2) {
  TestStringReference(0x02ff05fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringReference3) {
  TestStringReference(0x08ff08fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

TEST_F(Thumb2RelativePatcherTest, StringReference4) {
  TestStringReference(0xd0ff60fcu);
  ASSERT_LT(GetMethodOffset(1u), 0xfcu);
}

void Thumb2RelativePatcherTest::TestBakerFieldWide(uint32_t offset, uint32_t ref_reg) {
  uint32_t valid_regs[] = {
      0,  1,  2,  3,      5,  6,  7,  // R4 is reserved for entrypoint address.
      8,  9, 10, 11,                  // IP, SP, LR and PC are reserved.
  };
  DCHECK_ALIGNED(offset, 4u);
  DCHECK_LT(offset, 4 * KB);
  constexpr size_t kMethodCodeSize = 8u;
  constexpr size_t kLiteralOffset = 0u;
  uint32_t method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    for (uint32_t holder_reg : valid_regs) {
      uint32_t ldr = kLdrWInsn | offset | (base_reg << 16) | (ref_reg << 12);
      const std::vector<uint8_t> raw_code = RawCode({kBneWPlus0, ldr});
      ASSERT_EQ(kMethodCodeSize, raw_code.size());
      ArrayRef<const uint8_t> code(raw_code);
      uint32_t encoded_data = Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
          base_reg, holder_reg, /* narrow */ false);
      const LinkerPatch patches[] = {
          LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset, encoded_data),
      };
      ++method_idx;
      AddCompiledMethod(MethodRef(method_idx), code, ArrayRef<const LinkerPatch>(patches));
    }
  }
  Link();

  // All thunks are at the end.
  uint32_t thunk_offset = GetMethodOffset(method_idx) + RoundUp(kMethodCodeSize, kArmAlignment);
  method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    for (uint32_t holder_reg : valid_regs) {
      ++method_idx;
      uint32_t bne = BneWWithOffset(GetMethodOffset(method_idx) + kLiteralOffset, thunk_offset);
      uint32_t ldr = kLdrWInsn | offset | (base_reg << 16) | (ref_reg << 12);
      const std::vector<uint8_t> expected_code = RawCode({bne, ldr});
      ASSERT_EQ(kMethodCodeSize, expected_code.size()) << "bne=0x" << std::hex << bne;
      ASSERT_TRUE(
          CheckLinkedMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(expected_code)));

      std::vector<uint8_t> expected_thunk =
          CompileBakerOffsetThunk(base_reg, holder_reg, /* narrow */ false);
      ASSERT_GT(output_.size(), thunk_offset);
      ASSERT_GE(output_.size() - thunk_offset, expected_thunk.size());
      ArrayRef<const uint8_t> compiled_thunk(output_.data() + thunk_offset,
                                             expected_thunk.size());
      if (ArrayRef<const uint8_t>(expected_thunk) != compiled_thunk) {
        DumpDiff(ArrayRef<const uint8_t>(expected_thunk), compiled_thunk);
        ASSERT_TRUE(false);
      }

      size_t gray_check_offset = thunk_offset;
      if (holder_reg == base_reg) {
        // Verify that the null-check uses the correct register, i.e. holder_reg.
        if (holder_reg < 8) {
          ASSERT_GE(output_.size() - gray_check_offset, 2u);
          ASSERT_EQ(0xb100 | holder_reg, GetOutputInsn16(thunk_offset) & 0xfd07u);
          gray_check_offset +=2u;
        } else {
          ASSERT_GE(output_.size() - gray_check_offset, 6u);
          ASSERT_EQ(0xf1b00f00u | (holder_reg << 16), GetOutputInsn32(thunk_offset) & 0xfbff8f00u);
          ASSERT_EQ(0xd000u, GetOutputInsn16(thunk_offset + 4u) & 0xff00u);  // BEQ
          gray_check_offset += 6u;
        }
      }
      // Verify that the lock word for gray bit check is loaded from the holder address.
      ASSERT_GE(output_.size() - gray_check_offset,
                4u * /* 32-bit instructions */ 4u + 2u * /* 16-bit instructions */ 2u);
      const uint32_t load_lock_word =
          kLdrWInsn |
          (holder_reg << 16) |
          (/* IP */ 12 << 12) |
          mirror::Object::MonitorOffset().Uint32Value();
      ASSERT_EQ(load_lock_word, GetOutputInsn32(gray_check_offset));
      // Verify the gray bit check.
      DCHECK_GE(LockWord::kReadBarrierStateShift, 8u);  // ROR modified immediate.
      uint32_t ror_shift = 7 + (32 - LockWord::kReadBarrierStateShift);
      const uint32_t tst_gray_bit_without_offset =
          0xf0100f00 | (/* IP */ 12 << 16)
                     | (((ror_shift >> 4) & 1) << 26)   // i
                     | (((ror_shift >> 1) & 7) << 12)   // imm3
                     | ((ror_shift & 1) << 7);          // imm8, ROR('1':imm8<7:0>, ror_shift).
      EXPECT_EQ(tst_gray_bit_without_offset, GetOutputInsn32(gray_check_offset + 4u));
      EXPECT_EQ(0xd100u, GetOutputInsn16(gray_check_offset + 8u) & 0xff00u);  // BNE
      // Verify the fake dependency (skip "ADD LR, LR, #ldr_offset").
      const uint32_t fake_dependency =
          0xeb000010 |              // ADD Rd, Rn, Rm, LSR 32 (type=01, imm3=000, imm2=00)
          (/* IP */ 12) |           // Rm = IP
          (base_reg << 16) |        // Rn = base_reg
          (base_reg << 8);          // Rd = base_reg
      EXPECT_EQ(fake_dependency, GetOutputInsn32(gray_check_offset + 14u));
      // Do not check the rest of the implementation.

      // The next thunk follows on the next aligned offset.
      thunk_offset += RoundUp(expected_thunk.size(), kArmAlignment);
    }
  }
}

void Thumb2RelativePatcherTest::TestBakerFieldNarrow(uint32_t offset, uint32_t ref_reg) {
  uint32_t valid_regs[] = {
      0,  1,  2,  3,      5,  6,  7,  // R4 is reserved for entrypoint address.
      8,  9, 10, 11,                  // IP, SP, LR and PC are reserved.
  };
  DCHECK_ALIGNED(offset, 4u);
  DCHECK_LT(offset, 32u);
  constexpr size_t kMethodCodeSize = 6u;
  constexpr size_t kLiteralOffset = 0u;
  uint32_t method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    if (base_reg >= 8u) {
      continue;
    }
    for (uint32_t holder_reg : valid_regs) {
      uint32_t ldr = kLdrInsn | (offset << (6 - 2)) | (base_reg << 3) | ref_reg;
      const std::vector<uint8_t> raw_code = RawCode({kBneWPlus0, ldr});
      ASSERT_EQ(kMethodCodeSize, raw_code.size());
      ArrayRef<const uint8_t> code(raw_code);
      uint32_t encoded_data = Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
          base_reg, holder_reg, /* narrow */ true);
      const LinkerPatch patches[] = {
          LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset, encoded_data),
      };
      ++method_idx;
      AddCompiledMethod(MethodRef(method_idx), code, ArrayRef<const LinkerPatch>(patches));
    }
  }
  Link();

  // All thunks are at the end.
  uint32_t thunk_offset = GetMethodOffset(method_idx) + RoundUp(kMethodCodeSize, kArmAlignment);
  method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    if (base_reg >= 8u) {
      continue;
    }
    for (uint32_t holder_reg : valid_regs) {
      ++method_idx;
      uint32_t bne = BneWWithOffset(GetMethodOffset(method_idx) + kLiteralOffset, thunk_offset);
      uint32_t ldr = kLdrInsn | (offset << (6 - 2)) | (base_reg << 3) | ref_reg;
      const std::vector<uint8_t> expected_code = RawCode({bne, ldr});
      ASSERT_EQ(kMethodCodeSize, expected_code.size()) << "bne=0x" << std::hex << bne;
      ASSERT_TRUE(
          CheckLinkedMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(expected_code)));

      std::vector<uint8_t> expected_thunk =
          CompileBakerOffsetThunk(base_reg, holder_reg, /* narrow */ true);
      ASSERT_GT(output_.size(), thunk_offset);
      ASSERT_GE(output_.size() - thunk_offset, expected_thunk.size());
      ArrayRef<const uint8_t> compiled_thunk(output_.data() + thunk_offset,
                                             expected_thunk.size());
      if (ArrayRef<const uint8_t>(expected_thunk) != compiled_thunk) {
        DumpDiff(ArrayRef<const uint8_t>(expected_thunk), compiled_thunk);
        ASSERT_TRUE(false);
      }

      size_t gray_check_offset = thunk_offset;
      if (holder_reg == base_reg) {
        // Verify that the null-check uses the correct register, i.e. holder_reg.
        if (holder_reg < 8) {
          ASSERT_GE(output_.size() - gray_check_offset, 2u);
          ASSERT_EQ(0xb100 | holder_reg, GetOutputInsn16(thunk_offset) & 0xfd07u);
          gray_check_offset +=2u;
        } else {
          ASSERT_GE(output_.size() - gray_check_offset, 6u);
          ASSERT_EQ(0xf1b00f00u | (holder_reg << 16), GetOutputInsn32(thunk_offset) & 0xfbff8f00u);
          ASSERT_EQ(0xd000u, GetOutputInsn16(thunk_offset + 4u) & 0xff00u);  // BEQ
          gray_check_offset += 6u;
        }
      }
      // Verify that the lock word for gray bit check is loaded from the holder address.
      ASSERT_GE(output_.size() - gray_check_offset,
                4u * /* 32-bit instructions */ 4u + 2u * /* 16-bit instructions */ 2u);
      const uint32_t load_lock_word =
          kLdrWInsn |
          (holder_reg << 16) |
          (/* IP */ 12 << 12) |
          mirror::Object::MonitorOffset().Uint32Value();
      ASSERT_EQ(load_lock_word, GetOutputInsn32(gray_check_offset));
      // Verify the gray bit check.
      DCHECK_GE(LockWord::kReadBarrierStateShift, 8u);  // ROR modified immediate.
      uint32_t ror_shift = 7 + (32 - LockWord::kReadBarrierStateShift);
      const uint32_t tst_gray_bit_without_offset =
          0xf0100f00 | (/* IP */ 12 << 16)
                     | (((ror_shift >> 4) & 1) << 26)   // i
                     | (((ror_shift >> 1) & 7) << 12)   // imm3
                     | ((ror_shift & 1) << 7);          // imm8, ROR('1':imm8<7:0>, ror_shift).
      EXPECT_EQ(tst_gray_bit_without_offset, GetOutputInsn32(gray_check_offset + 4u));
      EXPECT_EQ(0xd100u, GetOutputInsn16(gray_check_offset + 8u) & 0xff00u);  // BNE
      // Verify the fake dependency (skip "ADD LR, LR, #ldr_offset").
      const uint32_t fake_dependency =
          0xeb000010 |              // ADD Rd, Rn, Rm, LSR 32 (type=01, imm3=000, imm2=00)
          (/* IP */ 12) |           // Rm = IP
          (base_reg << 16) |        // Rn = base_reg
          (base_reg << 8);          // Rd = base_reg
      EXPECT_EQ(fake_dependency, GetOutputInsn32(gray_check_offset + 14u));
      // Do not check the rest of the implementation.

      // The next thunk follows on the next aligned offset.
      thunk_offset += RoundUp(expected_thunk.size(), kArmAlignment);
    }
  }
}

#define TEST_BAKER_FIELD_WIDE(offset, ref_reg)    \
  TEST_F(Thumb2RelativePatcherTest,               \
    BakerOffsetWide##offset##_##ref_reg) {        \
    TestBakerFieldWide(offset, ref_reg);          \
  }

TEST_BAKER_FIELD_WIDE(/* offset */ 0, /* ref_reg */ 0)
TEST_BAKER_FIELD_WIDE(/* offset */ 8, /* ref_reg */ 3)
TEST_BAKER_FIELD_WIDE(/* offset */ 28, /* ref_reg */ 7)
TEST_BAKER_FIELD_WIDE(/* offset */ 0xffc, /* ref_reg */ 11)

#define TEST_BAKER_FIELD_NARROW(offset, ref_reg)  \
  TEST_F(Thumb2RelativePatcherTest,               \
    BakerOffsetNarrow##offset##_##ref_reg) {      \
    TestBakerFieldNarrow(offset, ref_reg);        \
  }

TEST_BAKER_FIELD_NARROW(/* offset */ 0, /* ref_reg */ 0)
TEST_BAKER_FIELD_NARROW(/* offset */ 8, /* ref_reg */ 3)
TEST_BAKER_FIELD_NARROW(/* offset */ 28, /* ref_reg */ 7)

TEST_F(Thumb2RelativePatcherTest, BakerOffsetThunkInTheMiddle) {
  // One thunk in the middle with maximum distance branches to it from both sides.
  // Use offset = 0, base_reg = 0, ref_reg = 0, the LDR is simply `kLdrWInsn`.
  constexpr uint32_t kLiteralOffset1 = 6u;
  const std::vector<uint8_t> raw_code1 = RawCode({kNopWInsn, kNopInsn, kBneWPlus0, kLdrWInsn});
  ArrayRef<const uint8_t> code1(raw_code1);
  uint32_t encoded_data = Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
      /* base_reg */ 0, /* holder_reg */ 0, /* narrow */ false);
  const LinkerPatch patches1[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset1, encoded_data),
  };
  AddCompiledMethod(MethodRef(1u), code1, ArrayRef<const LinkerPatch>(patches1));

  constexpr uint32_t expected_thunk_offset =
      kLiteralOffset1 + kPcAdjustment + /* kMaxBcondPositiveDisplacement */ ((1 << 20) - 2u);
  static_assert(IsAligned<kArmAlignment>(expected_thunk_offset), "Target offset must be aligned.");
  size_t filler1_size = expected_thunk_offset -
                        RoundUp(raw_code1.size() + sizeof(OatQuickMethodHeader), kArmAlignment);
  std::vector<uint8_t> raw_filler1_code = GenNops(filler1_size / 2u);
  ArrayRef<const uint8_t> filler1_code(raw_filler1_code);
  AddCompiledMethod(MethodRef(2u), filler1_code);

  // Enforce thunk reservation with a tiny method.
  AddCompiledMethod(MethodRef(3u), kNopCode);

  constexpr uint32_t kLiteralOffset2 = 4;
  static_assert(IsAligned<kArmAlignment>(kLiteralOffset2 + kPcAdjustment),
                "PC for BNE must be aligned.");

  // Allow reaching the thunk from the very beginning of a method almost 1MiB away. Backward branch
  // reaches the full 1MiB but we need to take PC adjustment into account. Things to subtract:
  //   - thunk size and method 3 pre-header, rounded up (padding in between if needed)
  //   - method 3 code and method 4 pre-header, rounded up (padding in between if needed)
  //   - method 4 header (let there be no padding between method 4 code and method 5 pre-header).
  size_t thunk_size =
      CompileBakerOffsetThunk(/* base_reg */ 0, /* holder_reg */ 0, /* narrow */ false).size();
  size_t filler2_size =
      1 * MB - (kLiteralOffset2 + kPcAdjustment)
             - RoundUp(thunk_size + sizeof(OatQuickMethodHeader), kArmAlignment)
             - RoundUp(kNopCode.size() + sizeof(OatQuickMethodHeader), kArmAlignment)
             - sizeof(OatQuickMethodHeader);
  std::vector<uint8_t> raw_filler2_code = GenNops(filler2_size / 2u);
  ArrayRef<const uint8_t> filler2_code(raw_filler2_code);
  AddCompiledMethod(MethodRef(4u), filler2_code);

  const std::vector<uint8_t> raw_code2 = RawCode({kNopWInsn, kBneWPlus0, kLdrWInsn});
  ArrayRef<const uint8_t> code2(raw_code2);
  const LinkerPatch patches2[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset2, encoded_data),
  };
  AddCompiledMethod(MethodRef(5u), code2, ArrayRef<const LinkerPatch>(patches2));

  Link();

  uint32_t first_method_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(5u);
  EXPECT_EQ(2 * MB, last_method_offset - first_method_offset);

  const uint32_t bne_max_forward = kBneWPlus0 | 0x003f2fff;
  const uint32_t bne_max_backward = kBneWPlus0 | 0x04000000;
  const std::vector<uint8_t> expected_code1 =
      RawCode({kNopWInsn, kNopInsn, bne_max_forward, kLdrWInsn});
  const std::vector<uint8_t> expected_code2 = RawCode({kNopWInsn, bne_max_backward, kLdrWInsn});
  ASSERT_TRUE(CheckLinkedMethod(MethodRef(1), ArrayRef<const uint8_t>(expected_code1)));
  ASSERT_TRUE(CheckLinkedMethod(MethodRef(5), ArrayRef<const uint8_t>(expected_code2)));
}

TEST_F(Thumb2RelativePatcherTest, BakerOffsetThunkBeforeFiller) {
  // Based on the first part of BakerOffsetThunkInTheMiddle but the BNE is one instruction
  // earlier, so the thunk is emitted before the filler.
  // Use offset = 0, base_reg = 0, ref_reg = 0, the LDR is simply `kLdrWInsn`.
  constexpr uint32_t kLiteralOffset1 = 4u;
  const std::vector<uint8_t> raw_code1 = RawCode({kNopWInsn, kBneWPlus0, kLdrWInsn, kNopInsn});
  ArrayRef<const uint8_t> code1(raw_code1);
  uint32_t encoded_data = Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
      /* base_reg */ 0, /* holder_reg */ 0, /* narrow */ false);
  const LinkerPatch patches1[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset1, encoded_data),
  };
  AddCompiledMethod(MethodRef(1u), code1, ArrayRef<const LinkerPatch>(patches1));

  constexpr uint32_t expected_thunk_offset =
      kLiteralOffset1 + kPcAdjustment + /* kMaxBcondPositiveDisplacement + 2 */ (1u << 20);
  static_assert(IsAligned<kArmAlignment>(expected_thunk_offset), "Target offset must be aligned.");
  size_t filler1_size = expected_thunk_offset -
                        RoundUp(raw_code1.size() + sizeof(OatQuickMethodHeader), kArmAlignment);
  std::vector<uint8_t> raw_filler1_code = GenNops(filler1_size / 2u);
  ArrayRef<const uint8_t> filler1_code(raw_filler1_code);
  AddCompiledMethod(MethodRef(2u), filler1_code);

  Link();

  const uint32_t bne = BneWWithOffset(kLiteralOffset1, RoundUp(raw_code1.size(), kArmAlignment));
  const std::vector<uint8_t> expected_code1 = RawCode({kNopWInsn, bne, kLdrWInsn, kNopInsn});
  ASSERT_TRUE(CheckLinkedMethod(MethodRef(1), ArrayRef<const uint8_t>(expected_code1)));
}

TEST_F(Thumb2RelativePatcherTest, BakerOffsetThunkInTheMiddleUnreachableFromLast) {
  // Based on the BakerOffsetThunkInTheMiddle but the BNE in the last method is preceded
  // by NOP and cannot reach the thunk in the middle, so we emit an extra thunk at the end.
  // Use offset = 0, base_reg = 0, ref_reg = 0, the LDR is simply `kLdrWInsn`.
  constexpr uint32_t kLiteralOffset1 = 6u;
  const std::vector<uint8_t> raw_code1 = RawCode({kNopWInsn, kNopInsn, kBneWPlus0, kLdrWInsn});
  ArrayRef<const uint8_t> code1(raw_code1);
  uint32_t encoded_data = Thumb2RelativePatcher::EncodeBakerReadBarrierFieldData(
      /* base_reg */ 0, /* holder_reg */ 0, /* narrow */ false);
  const LinkerPatch patches1[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset1, encoded_data),
  };
  AddCompiledMethod(MethodRef(1u), code1, ArrayRef<const LinkerPatch>(patches1));

  constexpr uint32_t expected_thunk_offset =
      kLiteralOffset1 + kPcAdjustment + /* kMaxBcondPositiveDisplacement */ ((1 << 20) - 2u);
  static_assert(IsAligned<kArmAlignment>(expected_thunk_offset), "Target offset must be aligned.");
  size_t filler1_size = expected_thunk_offset -
                        RoundUp(raw_code1.size() + sizeof(OatQuickMethodHeader), kArmAlignment);
  std::vector<uint8_t> raw_filler1_code = GenNops(filler1_size / 2u);
  ArrayRef<const uint8_t> filler1_code(raw_filler1_code);
  AddCompiledMethod(MethodRef(2u), filler1_code);

  // Enforce thunk reservation with a tiny method.
  AddCompiledMethod(MethodRef(3u), kNopCode);

  constexpr uint32_t kReachableFromOffset2 = 4;
  constexpr uint32_t kLiteralOffset2 = kReachableFromOffset2 + 2;
  static_assert(IsAligned<kArmAlignment>(kReachableFromOffset2 + kPcAdjustment),
                "PC for BNE must be aligned.");

  // If not for the extra NOP, this would allow reaching the thunk from the BNE
  // of a method 1MiB away. Backward branch reaches the full 1MiB  but we need to take
  // PC adjustment into account. Things to subtract:
  //   - thunk size and method 3 pre-header, rounded up (padding in between if needed)
  //   - method 3 code and method 4 pre-header, rounded up (padding in between if needed)
  //   - method 4 header (let there be no padding between method 4 code and method 5 pre-header).
  size_t thunk_size =
      CompileBakerOffsetThunk(/* base_reg */ 0, /* holder_reg */ 0, /* narrow */ false).size();
  size_t filler2_size =
      1 * MB - (kReachableFromOffset2 + kPcAdjustment)
             - RoundUp(thunk_size + sizeof(OatQuickMethodHeader), kArmAlignment)
             - RoundUp(kNopCode.size() + sizeof(OatQuickMethodHeader), kArmAlignment)
             - sizeof(OatQuickMethodHeader);
  std::vector<uint8_t> raw_filler2_code = GenNops(filler2_size / 2u);
  ArrayRef<const uint8_t> filler2_code(raw_filler2_code);
  AddCompiledMethod(MethodRef(4u), filler2_code);

  // Extra 16-bit NOP compared to BakerOffsetThunkInTheMiddle.
  const std::vector<uint8_t> raw_code2 = RawCode({kNopWInsn, kNopInsn, kBneWPlus0, kLdrWInsn});
  ArrayRef<const uint8_t> code2(raw_code2);
  const LinkerPatch patches2[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kLiteralOffset2, encoded_data),
  };
  AddCompiledMethod(MethodRef(5u), code2, ArrayRef<const LinkerPatch>(patches2));

  Link();

  uint32_t first_method_offset = GetMethodOffset(1u);
  uint32_t last_method_offset = GetMethodOffset(5u);
  EXPECT_EQ(2 * MB, last_method_offset - first_method_offset);

  const uint32_t bne_max_forward = kBneWPlus0 | 0x003f2fff;
  const uint32_t bne_last =
      BneWWithOffset(kLiteralOffset2, RoundUp(raw_code2.size(), kArmAlignment));
  const std::vector<uint8_t> expected_code1 =
      RawCode({kNopWInsn, kNopInsn, bne_max_forward, kLdrWInsn});
  const std::vector<uint8_t> expected_code2 =
      RawCode({kNopWInsn, kNopInsn, bne_last, kLdrWInsn});
  ASSERT_TRUE(CheckLinkedMethod(MethodRef(1), ArrayRef<const uint8_t>(expected_code1)));
  ASSERT_TRUE(CheckLinkedMethod(MethodRef(5), ArrayRef<const uint8_t>(expected_code2)));
}

TEST_F(Thumb2RelativePatcherTest, BakerArray) {
  uint32_t valid_regs[] = {
      0,  1,  2,  3,      5,  6,  7,  // R4 is reserved for entrypoint address.
      8,  9, 10, 11,                  // IP, SP, LR and PC are reserved.
  };
  auto ldr = [](uint32_t base_reg) {
    uint32_t index_reg = (base_reg == 0u) ? 1u : 0u;
    uint32_t ref_reg = (base_reg == 2) ? 3u : 2u;
    return kLdrRegLsl2 | index_reg | (base_reg << 16) | (ref_reg << 12);
  };
  constexpr size_t kMethodCodeSize = 8u;
  constexpr size_t kLiteralOffset = 0u;
  uint32_t method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    ++method_idx;
    const std::vector<uint8_t> raw_code = RawCode({kBneWPlus0, ldr(base_reg)});
    ASSERT_EQ(kMethodCodeSize, raw_code.size());
    ArrayRef<const uint8_t> code(raw_code);
    const LinkerPatch patches[] = {
        LinkerPatch::BakerReadBarrierBranchPatch(
            kLiteralOffset, Thumb2RelativePatcher::EncodeBakerReadBarrierArrayData(base_reg)),
    };
    AddCompiledMethod(MethodRef(method_idx), code, ArrayRef<const LinkerPatch>(patches));
  }
  Link();

  // All thunks are at the end.
  uint32_t thunk_offset = GetMethodOffset(method_idx) + RoundUp(kMethodCodeSize, kArmAlignment);
  method_idx = 0u;
  for (uint32_t base_reg : valid_regs) {
    ++method_idx;
    uint32_t bne = BneWWithOffset(GetMethodOffset(method_idx) + kLiteralOffset, thunk_offset);
    const std::vector<uint8_t> expected_code = RawCode({bne, ldr(base_reg)});
    ASSERT_EQ(kMethodCodeSize, expected_code.size());
    EXPECT_TRUE(CheckLinkedMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(expected_code)));

    std::vector<uint8_t> expected_thunk = CompileBakerArrayThunk(base_reg);
    ASSERT_GT(output_.size(), thunk_offset);
    ASSERT_GE(output_.size() - thunk_offset, expected_thunk.size());
    ArrayRef<const uint8_t> compiled_thunk(output_.data() + thunk_offset,
                                           expected_thunk.size());
    if (ArrayRef<const uint8_t>(expected_thunk) != compiled_thunk) {
      DumpDiff(ArrayRef<const uint8_t>(expected_thunk), compiled_thunk);
      ASSERT_TRUE(false);
    }

    // Verify that the lock word for gray bit check is loaded from the correct address
    // before the base_reg which points to the array data.
    ASSERT_GE(output_.size() - thunk_offset,
              4u * /* 32-bit instructions */ 4u + 2u * /* 16-bit instructions */ 2u);
    int32_t data_offset =
        mirror::Array::DataOffset(Primitive::ComponentSize(Primitive::kPrimNot)).Int32Value();
    int32_t offset = mirror::Object::MonitorOffset().Int32Value() - data_offset;
    ASSERT_LT(offset, 0);
    ASSERT_GT(offset, -256);
    const uint32_t load_lock_word =
        kLdrNegativeOffset |
        (-offset & 0xffu) |
        (base_reg << 16) |
        (/* IP */ 12 << 12);
    EXPECT_EQ(load_lock_word, GetOutputInsn32(thunk_offset));
    // Verify the gray bit check.
    DCHECK_GE(LockWord::kReadBarrierStateShift, 8u);  // ROR modified immediate.
    uint32_t ror_shift = 7 + (32 - LockWord::kReadBarrierStateShift);
    const uint32_t tst_gray_bit_without_offset =
        0xf0100f00 | (/* IP */ 12 << 16)
                   | (((ror_shift >> 4) & 1) << 26)   // i
                   | (((ror_shift >> 1) & 7) << 12)   // imm3
                   | ((ror_shift & 1) << 7);          // imm8, ROR('1':imm8<7:0>, ror_shift).
    EXPECT_EQ(tst_gray_bit_without_offset, GetOutputInsn32(thunk_offset + 4u));
    EXPECT_EQ(0xd100u, GetOutputInsn16(thunk_offset + 8u) & 0xff00u);  // BNE
    // Verify the fake dependency.
    const uint32_t fake_dependency =
        0xeb000010 |              // ADD Rd, Rn, Rm, LSR 32 (type=01, imm3=000, imm2=00)
        (/* IP */ 12) |           // Rm = IP
        (base_reg << 16) |        // Rn = base_reg
        (base_reg << 8);          // Rd = base_reg
    EXPECT_EQ(fake_dependency, GetOutputInsn32(thunk_offset + 14u));
    // Do not check the rest of the implementation.

    // The next thunk follows on the next aligned offset.
    thunk_offset += RoundUp(expected_thunk.size(), kArmAlignment);
  }
}

TEST_F(Thumb2RelativePatcherTest, BakerGcRootWide) {
  uint32_t valid_regs[] = {
      0,  1,  2,  3,      5,  6,  7,  // R4 is reserved for entrypoint address.
      8,  9, 10, 11,                  // IP, SP, LR and PC are reserved.
  };
  constexpr size_t kMethodCodeSize = 8u;
  constexpr size_t kLiteralOffset = 4u;
  uint32_t method_idx = 0u;
  for (uint32_t root_reg : valid_regs) {
    ++method_idx;
    uint32_t ldr = kLdrWInsn | (/* offset */ 8) | (/* base_reg */ 0 << 16) | (root_reg << 12);
    const std::vector<uint8_t> raw_code = RawCode({ldr, kBneWPlus0});
    ASSERT_EQ(kMethodCodeSize, raw_code.size());
    ArrayRef<const uint8_t> code(raw_code);
    const LinkerPatch patches[] = {
        LinkerPatch::BakerReadBarrierBranchPatch(
            kLiteralOffset,
            Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(root_reg, /* narrow */ false)),
    };
    AddCompiledMethod(MethodRef(method_idx), code, ArrayRef<const LinkerPatch>(patches));
  }
  Link();

  // All thunks are at the end.
  uint32_t thunk_offset = GetMethodOffset(method_idx) + RoundUp(kMethodCodeSize, kArmAlignment);
  method_idx = 0u;
  for (uint32_t root_reg : valid_regs) {
    ++method_idx;
    uint32_t bne = BneWWithOffset(GetMethodOffset(method_idx) + kLiteralOffset, thunk_offset);
    uint32_t ldr = kLdrWInsn | (/* offset */ 8) | (/* base_reg */ 0 << 16) | (root_reg << 12);
    const std::vector<uint8_t> expected_code = RawCode({ldr, bne});
    ASSERT_EQ(kMethodCodeSize, expected_code.size());
    EXPECT_TRUE(CheckLinkedMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(expected_code)));

    std::vector<uint8_t> expected_thunk = CompileBakerGcRootThunk(root_reg, /* narrow */ false);
    ASSERT_GT(output_.size(), thunk_offset);
    ASSERT_GE(output_.size() - thunk_offset, expected_thunk.size());
    ArrayRef<const uint8_t> compiled_thunk(output_.data() + thunk_offset,
                                           expected_thunk.size());
    if (ArrayRef<const uint8_t>(expected_thunk) != compiled_thunk) {
      DumpDiff(ArrayRef<const uint8_t>(expected_thunk), compiled_thunk);
      ASSERT_TRUE(false);
    }

    // Verify that the fast-path null-check uses the correct register, i.e. root_reg.
    if (root_reg < 8) {
      ASSERT_GE(output_.size() - thunk_offset, 2u);
      ASSERT_EQ(0xb100 | root_reg, GetOutputInsn16(thunk_offset) & 0xfd07u);
    } else {
      ASSERT_GE(output_.size() - thunk_offset, 6u);
      ASSERT_EQ(0xf1b00f00u | (root_reg << 16), GetOutputInsn32(thunk_offset) & 0xfbff8f00u);
      ASSERT_EQ(0xd000u, GetOutputInsn16(thunk_offset + 4u) & 0xff00u);  // BEQ
    }
    // Do not check the rest of the implementation.

    // The next thunk follows on the next aligned offset.
    thunk_offset += RoundUp(expected_thunk.size(), kArmAlignment);
  }
}

TEST_F(Thumb2RelativePatcherTest, BakerGcRootNarrow) {
  uint32_t valid_regs[] = {
      0,  1,  2,  3,      5,  6,  7,  // R4 is reserved for entrypoint address.
                                      // Not appplicable to high registers.
  };
  constexpr size_t kMethodCodeSize = 6u;
  constexpr size_t kLiteralOffset = 2u;
  uint32_t method_idx = 0u;
  for (uint32_t root_reg : valid_regs) {
    ++method_idx;
    uint32_t ldr = kLdrInsn | (/* offset */ 8 << (6 - 2)) | (/* base_reg */ 0 << 3) | root_reg;
    const std::vector<uint8_t> raw_code = RawCode({ldr, kBneWPlus0});
    ASSERT_EQ(kMethodCodeSize, raw_code.size());
    ArrayRef<const uint8_t> code(raw_code);
    const LinkerPatch patches[] = {
        LinkerPatch::BakerReadBarrierBranchPatch(
            kLiteralOffset,
            Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(root_reg, /* narrow */ true)),
    };
    AddCompiledMethod(MethodRef(method_idx), code, ArrayRef<const LinkerPatch>(patches));
  }
  Link();

  // All thunks are at the end.
  uint32_t thunk_offset = GetMethodOffset(method_idx) + RoundUp(kMethodCodeSize, kArmAlignment);
  method_idx = 0u;
  for (uint32_t root_reg : valid_regs) {
    ++method_idx;
    uint32_t bne = BneWWithOffset(GetMethodOffset(method_idx) + kLiteralOffset, thunk_offset);
    uint32_t ldr = kLdrInsn | (/* offset */ 8 << (6 - 2)) | (/* base_reg */ 0 << 3) | root_reg;
    const std::vector<uint8_t> expected_code = RawCode({ldr, bne});
    ASSERT_EQ(kMethodCodeSize, expected_code.size());
    EXPECT_TRUE(CheckLinkedMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(expected_code)));

    std::vector<uint8_t> expected_thunk = CompileBakerGcRootThunk(root_reg, /* narrow */ true);
    ASSERT_GT(output_.size(), thunk_offset);
    ASSERT_GE(output_.size() - thunk_offset, expected_thunk.size());
    ArrayRef<const uint8_t> compiled_thunk(output_.data() + thunk_offset,
                                           expected_thunk.size());
    if (ArrayRef<const uint8_t>(expected_thunk) != compiled_thunk) {
      DumpDiff(ArrayRef<const uint8_t>(expected_thunk), compiled_thunk);
      ASSERT_TRUE(false);
    }

    // Verify that the fast-path null-check CBZ uses the correct register, i.e. root_reg.
    ASSERT_GE(output_.size() - thunk_offset, 2u);
    ASSERT_EQ(0xb100 | root_reg, GetOutputInsn16(thunk_offset) & 0xfd07u);
    // Do not check the rest of the implementation.

    // The next thunk follows on the next aligned offset.
    thunk_offset += RoundUp(expected_thunk.size(), kArmAlignment);
  }
}

TEST_F(Thumb2RelativePatcherTest, BakerGcRootOffsetBits) {
  // Test 1MiB of patches to the same thunk to stress-test different large offsets.
  // (The low bits are not that important but the location of the high bits is easy to get wrong.)
  std::vector<uint8_t> code;
  code.reserve(1 * MB);
  const size_t num_patches = 1 * MB / 8u;
  std::vector<LinkerPatch> patches;
  patches.reserve(num_patches);
  const uint32_t ldr =
      kLdrWInsn | (/* offset */ 8) | (/* base_reg */ 0 << 16) | (/* root_reg */ 0 << 12);
  uint32_t encoded_data =
      Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(/* root_reg */ 0, /* narrow */ false);
  for (size_t i = 0; i != num_patches; ++i) {
    PushBackInsn(&code, ldr);
    PushBackInsn(&code, kBneWPlus0);
    patches.push_back(LinkerPatch::BakerReadBarrierBranchPatch(8u * i + 4u, encoded_data));
  }
  ASSERT_EQ(1 * MB, code.size());
  ASSERT_EQ(num_patches, patches.size());
  AddCompiledMethod(MethodRef(1u),
                    ArrayRef<const uint8_t>(code),
                    ArrayRef<const LinkerPatch>(patches));
  Link();

  // The thunk is right after the method code.
  DCHECK_ALIGNED(1 * MB, kArmAlignment);
  std::vector<uint8_t> expected_code;
  for (size_t i = 0; i != num_patches; ++i) {
    PushBackInsn(&expected_code, ldr);
    PushBackInsn(&expected_code, BneWWithOffset(8u * i + 4u, 1 * MB));
    patches.push_back(LinkerPatch::BakerReadBarrierBranchPatch(8u * i + 4u, encoded_data));
  }
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

TEST_F(Thumb2RelativePatcherTest, BakerAndMethodCallInteraction) {
  // During development, there was a `DCHECK_LE(MaxNextOffset(), next_thunk.MaxNextOffset());`
  // in `ArmBaseRelativePatcher::ThunkData::MakeSpaceBefore()` which does not necessarily
  // hold when we're reserving thunks of different sizes. This test exposes the situation
  // by using Baker thunks and a method call thunk.

  // Add a method call patch that can reach to method 1 offset + 16MiB.
  uint32_t method_idx = 0u;
  constexpr size_t kMethodCallLiteralOffset = 2u;
  constexpr uint32_t kMissingMethodIdx = 2u;
  const std::vector<uint8_t> raw_code1 = RawCode({kNopInsn, kBlPlus0});
  const LinkerPatch method1_patches[] = {
      LinkerPatch::RelativeCodePatch(kMethodCallLiteralOffset, nullptr, 2u),
  };
  ArrayRef<const uint8_t> code1(raw_code1);
  ++method_idx;
  AddCompiledMethod(MethodRef(1u), code1, ArrayRef<const LinkerPatch>(method1_patches));

  // Skip kMissingMethodIdx.
  ++method_idx;
  ASSERT_EQ(kMissingMethodIdx, method_idx);
  // Add a method with the right size that the method code for the next one starts 1MiB
  // after code for method 1.
  size_t filler_size =
      1 * MB - RoundUp(raw_code1.size() + sizeof(OatQuickMethodHeader), kArmAlignment)
             - sizeof(OatQuickMethodHeader);
  std::vector<uint8_t> filler_code = GenNops(filler_size / 2u);
  ++method_idx;
  AddCompiledMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(filler_code));
  // Add 14 methods with 1MiB code+header, making the code for the next method start 1MiB
  // before the currently scheduled MaxNextOffset() for the method call thunk.
  for (uint32_t i = 0; i != 14; ++i) {
    filler_size = 1 * MB - sizeof(OatQuickMethodHeader);
    filler_code = GenNops(filler_size / 2u);
    ++method_idx;
    AddCompiledMethod(MethodRef(method_idx), ArrayRef<const uint8_t>(filler_code));
  }

  // Add 2 Baker GC root patches to the last method, one that would allow the thunk at
  // 1MiB + kArmAlignment, i.e. kArmAlignment after the method call thunk, and the
  // second that needs it kArmAlignment after that. Given the size of the GC root thunk
  // is more than the space required by the method call thunk plus kArmAlignment,
  // this pushes the first GC root thunk's pending MaxNextOffset() before the method call
  // thunk's pending MaxNextOffset() which needs to be adjusted.
  ASSERT_LT(RoundUp(CompileMethodCallThunk().size(), kArmAlignment) + kArmAlignment,
            CompileBakerGcRootThunk(/* root_reg */ 0, /* narrow */ false).size());
  static_assert(kArmAlignment == 8, "Code below assumes kArmAlignment == 8");
  constexpr size_t kBakerLiteralOffset1 = kArmAlignment + 2u - kPcAdjustment;
  constexpr size_t kBakerLiteralOffset2 = kBakerLiteralOffset1 + kArmAlignment;
  // Use offset = 0, base_reg = 0, the LDR is simply `kLdrWInsn | (root_reg << 12)`.
  const uint32_t ldr1 = kLdrWInsn | (/* root_reg */ 1 << 12);
  const uint32_t ldr2 = kLdrWInsn | (/* root_reg */ 2 << 12);
  const std::vector<uint8_t> last_method_raw_code = RawCode({
      kNopInsn,                                 // Padding before first GC root read barrier.
      ldr1, kBneWPlus0,                         // First GC root LDR with read barrier.
      ldr2, kBneWPlus0,                         // Second GC root LDR with read barrier.
  });
  uint32_t encoded_data1 =
      Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(/* root_reg */ 1, /* narrow */ false);
  uint32_t encoded_data2 =
      Thumb2RelativePatcher::EncodeBakerReadBarrierGcRootData(/* root_reg */ 2, /* narrow */ false);
  const LinkerPatch last_method_patches[] = {
      LinkerPatch::BakerReadBarrierBranchPatch(kBakerLiteralOffset1, encoded_data1),
      LinkerPatch::BakerReadBarrierBranchPatch(kBakerLiteralOffset2, encoded_data2),
  };
  ++method_idx;
  AddCompiledMethod(MethodRef(method_idx),
                    ArrayRef<const uint8_t>(last_method_raw_code),
                    ArrayRef<const LinkerPatch>(last_method_patches));

  // The main purpose of the test is to check that Link() does not cause a crash.
  Link();

  ASSERT_EQ(15 * MB, GetMethodOffset(method_idx) - GetMethodOffset(1u));
}

}  // namespace linker
}  // namespace art
