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

#ifndef ART_COMPILER_LINKER_RELATIVE_PATCHER_H_
#define ART_COMPILER_LINKER_RELATIVE_PATCHER_H_

#include <vector>

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/array_ref.h"
#include "base/macros.h"
#include "dex/method_reference.h"

namespace art {

class CompiledMethod;

namespace debug {
struct MethodDebugInfo;
}  // namespace debug

namespace linker {

class LinkerPatch;
class OutputStream;

/**
 * @class RelativePatcherTargetProvider
 * @brief Interface for providing method offsets for relative call targets.
 */
class RelativePatcherTargetProvider {
 public:
  /**
   * Find the offset of the target method of a relative call if known.
   *
   * The process of assigning target method offsets includes calls to the relative patcher's
   * ReserveSpace() which in turn can use FindMethodOffset() to determine if a method already
   * has an offset assigned and, if so, what's that offset. If the offset has not yet been
   * assigned or if it's too far for the particular architecture's relative call,
   * ReserveSpace() may need to allocate space for a special dispatch thunk.
   *
   * @param ref the target method of the relative call.
   * @return true in the first element of the pair if the method was found, false otherwise;
   *         if found, the second element specifies the offset.
   */
  virtual std::pair<bool, uint32_t> FindMethodOffset(MethodReference ref) = 0;

 protected:
  virtual ~RelativePatcherTargetProvider() { }
};

/**
 * @class RelativePatcher
 * @brief Interface for architecture-specific link-time patching of PC-relative references.
 */
class RelativePatcher {
 public:
  static std::unique_ptr<RelativePatcher> Create(
      InstructionSet instruction_set, const InstructionSetFeatures* features,
      RelativePatcherTargetProvider* provider);

  virtual ~RelativePatcher() { }

  uint32_t CodeAlignmentSize() const {
    return size_code_alignment_;
  }

  uint32_t RelativeCallThunksSize() const {
    return size_relative_call_thunks_;
  }

  uint32_t MiscThunksSize() const {
    return size_misc_thunks_;
  }

  // Reserve space for thunks if needed before a method, return adjusted offset.
  virtual uint32_t ReserveSpace(uint32_t offset,
                                const CompiledMethod* compiled_method,
                                MethodReference method_ref) = 0;

  // Reserve space for thunks if needed after the last method, return adjusted offset.
  // The caller may use this method to preemptively force thunk space reservation and
  // then resume reservation for more methods. This is useful when there is a gap in
  // the .text segment, for example when going to the next oat file for multi-image.
  virtual uint32_t ReserveSpaceEnd(uint32_t offset) = 0;

  // Write relative call thunks if needed, return adjusted offset. Returns 0 on write failure.
  virtual uint32_t WriteThunks(OutputStream* out, uint32_t offset) = 0;

  // Patch method code. The input displacement is relative to the patched location,
  // the patcher may need to adjust it if the correct base is different.
  virtual void PatchCall(std::vector<uint8_t>* code,
                         uint32_t literal_offset,
                         uint32_t patch_offset,
                         uint32_t target_offset) = 0;

  // Patch a reference to a dex cache location.
  virtual void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                        const LinkerPatch& patch,
                                        uint32_t patch_offset,
                                        uint32_t target_offset) = 0;

  // Patch a branch to a Baker read barrier thunk.
  virtual void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                           const LinkerPatch& patch,
                                           uint32_t patch_offset) = 0;

  virtual std::vector<debug::MethodDebugInfo> GenerateThunkDebugInfo(
      uint32_t executable_offset) = 0;

 protected:
  RelativePatcher()
      : size_code_alignment_(0u),
        size_relative_call_thunks_(0u),
        size_misc_thunks_(0u) {
  }

  bool WriteCodeAlignment(OutputStream* out, uint32_t aligned_code_delta);
  bool WriteThunk(OutputStream* out, const ArrayRef<const uint8_t>& thunk);
  bool WriteMiscThunk(OutputStream* out, const ArrayRef<const uint8_t>& thunk);

 private:
  uint32_t size_code_alignment_;
  uint32_t size_relative_call_thunks_;
  uint32_t size_misc_thunks_;

  DISALLOW_COPY_AND_ASSIGN(RelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_RELATIVE_PATCHER_H_
