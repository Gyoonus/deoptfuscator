/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_JNI_QUICK_JNI_COMPILER_H_
#define ART_COMPILER_JNI_QUICK_JNI_COMPILER_H_

#include <vector>

#include "arch/instruction_set.h"
#include "base/array_ref.h"

namespace art {

class ArtMethod;
class CompilerDriver;
class DexFile;

class JniCompiledMethod {
 public:
  JniCompiledMethod(InstructionSet instruction_set,
                    std::vector<uint8_t>&& code,
                    uint32_t frame_size,
                    uint32_t core_spill_mask,
                    uint32_t fp_spill_mask,
                    ArrayRef<const uint8_t> cfi)
      : instruction_set_(instruction_set),
        code_(std::move(code)),
        frame_size_(frame_size),
        core_spill_mask_(core_spill_mask),
        fp_spill_mask_(fp_spill_mask),
        cfi_(cfi.begin(), cfi.end()) {}

  JniCompiledMethod(JniCompiledMethod&& other) = default;
  ~JniCompiledMethod() = default;

  InstructionSet GetInstructionSet() const { return instruction_set_; }
  ArrayRef<const uint8_t> GetCode() const { return ArrayRef<const uint8_t>(code_); }
  uint32_t GetFrameSize() const { return frame_size_; }
  uint32_t GetCoreSpillMask() const { return core_spill_mask_; }
  uint32_t GetFpSpillMask() const { return fp_spill_mask_; }
  ArrayRef<const uint8_t> GetCfi() const { return ArrayRef<const uint8_t>(cfi_); }

 private:
  InstructionSet instruction_set_;
  std::vector<uint8_t> code_;
  uint32_t frame_size_;
  uint32_t core_spill_mask_;
  uint32_t fp_spill_mask_;
  std::vector<uint8_t> cfi_;
};

JniCompiledMethod ArtQuickJniCompileMethod(CompilerDriver* compiler,
                                           uint32_t access_flags,
                                           uint32_t method_idx,
                                           const DexFile& dex_file);

}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_JNI_COMPILER_H_
