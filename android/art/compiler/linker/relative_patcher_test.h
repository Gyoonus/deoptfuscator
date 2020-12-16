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

#ifndef ART_COMPILER_LINKER_RELATIVE_PATCHER_TEST_H_
#define ART_COMPILER_LINKER_RELATIVE_PATCHER_TEST_H_

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/array_ref.h"
#include "base/macros.h"
#include "compiled_method-inl.h"
#include "dex/verification_results.h"
#include "dex/method_reference.h"
#include "dex/string_reference.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "globals.h"
#include "gtest/gtest.h"
#include "linker/relative_patcher.h"
#include "oat.h"
#include "oat_quick_method_header.h"
#include "vector_output_stream.h"

namespace art {
namespace linker {

// Base class providing infrastructure for architecture-specific tests.
class RelativePatcherTest : public testing::Test {
 protected:
  RelativePatcherTest(InstructionSet instruction_set, const std::string& variant)
      : compiler_options_(),
        verification_results_(&compiler_options_),
        driver_(&compiler_options_,
                &verification_results_,
                Compiler::kQuick,
                instruction_set,
                /* instruction_set_features*/ nullptr,
                /* image_classes */ nullptr,
                /* compiled_classes */ nullptr,
                /* compiled_methods */ nullptr,
                /* thread_count */ 1u,
                /* swap_fd */ -1,
                /* profile_compilation_info */ nullptr),
        error_msg_(),
        instruction_set_(instruction_set),
        features_(InstructionSetFeatures::FromVariant(instruction_set, variant, &error_msg_)),
        method_offset_map_(),
        patcher_(RelativePatcher::Create(instruction_set, features_.get(), &method_offset_map_)),
        bss_begin_(0u),
        compiled_method_refs_(),
        compiled_methods_(),
        patched_code_(),
        output_(),
        out_("test output stream", &output_) {
    CHECK(error_msg_.empty()) << instruction_set << "/" << variant;
    patched_code_.reserve(16 * KB);
  }

  MethodReference MethodRef(uint32_t method_idx) {
    CHECK_NE(method_idx, 0u);
    return MethodReference(nullptr, method_idx);
  }

  void AddCompiledMethod(
      MethodReference method_ref,
      const ArrayRef<const uint8_t>& code,
      const ArrayRef<const LinkerPatch>& patches = ArrayRef<const LinkerPatch>()) {
    compiled_method_refs_.push_back(method_ref);
    compiled_methods_.emplace_back(new CompiledMethod(
        &driver_,
        instruction_set_,
        code,
        /* frame_size_in_bytes */ 0u,
        /* core_spill_mask */ 0u,
        /* fp_spill_mask */ 0u,
        /* method_info */ ArrayRef<const uint8_t>(),
        /* vmap_table */ ArrayRef<const uint8_t>(),
        /* cfi_info */ ArrayRef<const uint8_t>(),
        patches));
  }

  uint32_t CodeAlignmentSize(uint32_t header_offset_to_align) {
    // We want to align the code rather than the preheader.
    uint32_t unaligned_code_offset = header_offset_to_align + sizeof(OatQuickMethodHeader);
    uint32_t aligned_code_offset =
        CompiledMethod::AlignCode(unaligned_code_offset, instruction_set_);
    return aligned_code_offset - unaligned_code_offset;
  }

  void Link() {
    // Reserve space.
    static_assert(kTrampolineOffset == 0u, "Unexpected trampoline offset.");
    uint32_t offset = kTrampolineSize;
    size_t idx = 0u;
    for (auto& compiled_method : compiled_methods_) {
      offset = patcher_->ReserveSpace(offset, compiled_method.get(), compiled_method_refs_[idx]);

      uint32_t alignment_size = CodeAlignmentSize(offset);
      offset += alignment_size;

      offset += sizeof(OatQuickMethodHeader);
      uint32_t quick_code_offset = offset + compiled_method->CodeDelta();
      const auto code = compiled_method->GetQuickCode();
      offset += code.size();

      method_offset_map_.map.Put(compiled_method_refs_[idx], quick_code_offset);
      ++idx;
    }
    offset = patcher_->ReserveSpaceEnd(offset);
    uint32_t output_size = offset;
    output_.reserve(output_size);

    // Write data.
    DCHECK(output_.empty());
    uint8_t dummy_trampoline[kTrampolineSize];
    memset(dummy_trampoline, 0, sizeof(dummy_trampoline));
    out_.WriteFully(dummy_trampoline, kTrampolineSize);
    offset = kTrampolineSize;
    static const uint8_t kPadding[] = {
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
    };
    uint8_t dummy_header[sizeof(OatQuickMethodHeader)];
    memset(dummy_header, 0, sizeof(dummy_header));
    for (auto& compiled_method : compiled_methods_) {
      offset = patcher_->WriteThunks(&out_, offset);

      uint32_t alignment_size = CodeAlignmentSize(offset);
      CHECK_LE(alignment_size, sizeof(kPadding));
      out_.WriteFully(kPadding, alignment_size);
      offset += alignment_size;

      out_.WriteFully(dummy_header, sizeof(OatQuickMethodHeader));
      offset += sizeof(OatQuickMethodHeader);
      ArrayRef<const uint8_t> code = compiled_method->GetQuickCode();
      if (!compiled_method->GetPatches().empty()) {
        patched_code_.assign(code.begin(), code.end());
        code = ArrayRef<const uint8_t>(patched_code_);
        for (const LinkerPatch& patch : compiled_method->GetPatches()) {
          if (patch.GetType() == LinkerPatch::Type::kCallRelative) {
            auto result = method_offset_map_.FindMethodOffset(patch.TargetMethod());
            uint32_t target_offset =
                result.first ? result.second : kTrampolineOffset + compiled_method->CodeDelta();
            patcher_->PatchCall(&patched_code_, patch.LiteralOffset(),
                                offset + patch.LiteralOffset(), target_offset);
          } else if (patch.GetType() == LinkerPatch::Type::kStringBssEntry) {
            uint32_t target_offset =
                bss_begin_ + string_index_to_offset_map_.Get(patch.TargetStringIndex().index_);
            patcher_->PatchPcRelativeReference(&patched_code_,
                                               patch,
                                               offset + patch.LiteralOffset(),
                                               target_offset);
          } else if (patch.GetType() == LinkerPatch::Type::kStringRelative) {
            uint32_t target_offset =
                string_index_to_offset_map_.Get(patch.TargetStringIndex().index_);
            patcher_->PatchPcRelativeReference(&patched_code_,
                                               patch,
                                               offset + patch.LiteralOffset(),
                                               target_offset);
          } else if (patch.GetType() == LinkerPatch::Type::kBakerReadBarrierBranch) {
            patcher_->PatchBakerReadBarrierBranch(&patched_code_,
                                                  patch,
                                                  offset + patch.LiteralOffset());
          } else {
            LOG(FATAL) << "Bad patch type. " << patch.GetType();
            UNREACHABLE();
          }
        }
      }
      out_.WriteFully(&code[0], code.size());
      offset += code.size();
    }
    offset = patcher_->WriteThunks(&out_, offset);
    CHECK_EQ(offset, output_size);
    CHECK_EQ(output_.size(), output_size);
  }

  bool CheckLinkedMethod(MethodReference method_ref, const ArrayRef<const uint8_t>& expected_code) {
    // Sanity check: original code size must match linked_code.size().
    size_t idx = 0u;
    for (auto ref : compiled_method_refs_) {
      if (ref == method_ref) {
        break;
      }
      ++idx;
    }
    CHECK_NE(idx, compiled_method_refs_.size());
    CHECK_EQ(compiled_methods_[idx]->GetQuickCode().size(), expected_code.size());

    auto result = method_offset_map_.FindMethodOffset(method_ref);
    CHECK(result.first);  // Must have been linked.
    size_t offset = result.second - compiled_methods_[idx]->CodeDelta();
    CHECK_LT(offset, output_.size());
    CHECK_LE(offset + expected_code.size(), output_.size());
    ArrayRef<const uint8_t> linked_code(&output_[offset], expected_code.size());
    if (linked_code == expected_code) {
      return true;
    }
    // Log failure info.
    DumpDiff(expected_code, linked_code);
    return false;
  }

  void DumpDiff(const ArrayRef<const uint8_t>& expected_code,
                const ArrayRef<const uint8_t>& linked_code) {
    std::ostringstream expected_hex;
    std::ostringstream linked_hex;
    std::ostringstream diff_indicator;
    static const char digits[] = "0123456789abcdef";
    bool found_diff = false;
    for (size_t i = 0; i != expected_code.size(); ++i) {
      expected_hex << " " << digits[expected_code[i] >> 4] << digits[expected_code[i] & 0xf];
      linked_hex << " " << digits[linked_code[i] >> 4] << digits[linked_code[i] & 0xf];
      if (!found_diff) {
        found_diff = (expected_code[i] != linked_code[i]);
        diff_indicator << (found_diff ? " ^^" : "   ");
      }
    }
    CHECK(found_diff);
    std::string expected_hex_str = expected_hex.str();
    std::string linked_hex_str = linked_hex.str();
    std::string diff_indicator_str = diff_indicator.str();
    if (diff_indicator_str.length() > 60) {
      CHECK_EQ(diff_indicator_str.length() % 3u, 0u);
      size_t remove = diff_indicator_str.length() / 3 - 5;
      std::ostringstream oss;
      oss << "[stripped " << remove << "]";
      std::string replacement = oss.str();
      expected_hex_str.replace(0u, remove * 3u, replacement);
      linked_hex_str.replace(0u, remove * 3u, replacement);
      diff_indicator_str.replace(0u, remove * 3u, replacement);
    }
    LOG(ERROR) << "diff expected_code linked_code";
    LOG(ERROR) << "<" << expected_hex_str;
    LOG(ERROR) << ">" << linked_hex_str;
    LOG(ERROR) << " " << diff_indicator_str;
  }

  // Map method reference to assinged offset.
  // Wrap the map in a class implementing RelativePatcherTargetProvider.
  class MethodOffsetMap FINAL : public RelativePatcherTargetProvider {
   public:
    std::pair<bool, uint32_t> FindMethodOffset(MethodReference ref) OVERRIDE {
      auto it = map.find(ref);
      if (it == map.end()) {
        return std::pair<bool, uint32_t>(false, 0u);
      } else {
        return std::pair<bool, uint32_t>(true, it->second);
      }
    }
    SafeMap<MethodReference, uint32_t> map;
  };

  static const uint32_t kTrampolineSize = 4u;
  static const uint32_t kTrampolineOffset = 0u;

  CompilerOptions compiler_options_;
  VerificationResults verification_results_;
  CompilerDriver driver_;  // Needed for constructing CompiledMethod.
  std::string error_msg_;
  InstructionSet instruction_set_;
  std::unique_ptr<const InstructionSetFeatures> features_;
  MethodOffsetMap method_offset_map_;
  std::unique_ptr<RelativePatcher> patcher_;
  uint32_t bss_begin_;
  SafeMap<uint32_t, uint32_t> string_index_to_offset_map_;
  std::vector<MethodReference> compiled_method_refs_;
  std::vector<std::unique_ptr<CompiledMethod>> compiled_methods_;
  std::vector<uint8_t> patched_code_;
  std::vector<uint8_t> output_;
  VectorOutputStream out_;
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_RELATIVE_PATCHER_TEST_H_
