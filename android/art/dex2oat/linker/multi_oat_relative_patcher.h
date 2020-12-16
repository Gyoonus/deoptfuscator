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

#ifndef ART_DEX2OAT_LINKER_MULTI_OAT_RELATIVE_PATCHER_H_
#define ART_DEX2OAT_LINKER_MULTI_OAT_RELATIVE_PATCHER_H_

#include "arch/instruction_set.h"
#include "base/safe_map.h"
#include "debug/method_debug_info.h"
#include "dex/method_reference.h"
#include "linker/relative_patcher.h"

namespace art {

class CompiledMethod;
class InstructionSetFeatures;

namespace linker {

// MultiOatRelativePatcher is a helper class for handling patching across
// any number of oat files. It provides storage for method code offsets
// and wraps RelativePatcher calls, adjusting relative offsets according
// to the value set by SetAdjustment().
class MultiOatRelativePatcher FINAL {
 public:
  using const_iterator = SafeMap<MethodReference, uint32_t>::const_iterator;

  MultiOatRelativePatcher(InstructionSet instruction_set, const InstructionSetFeatures* features);

  // Mark the start of a new oat file (for statistics retrieval) and set the
  // adjustment for a new oat file to apply to all relative offsets that are
  // passed to the MultiOatRelativePatcher.
  //
  // The adjustment should be the global offset of the base from which relative
  // offsets are calculated, such as the start of .rodata for the current oat file.
  // It must must never point directly to a method's code to avoid relative offsets
  // with value 0 because this value is used as a missing offset indication in
  // GetOffset() and an error indication in WriteThunks(). Additionally, it must be
  // page-aligned, so that it does not skew alignment calculations, say arm64 ADRP.
  void StartOatFile(uint32_t adjustment);

  // Get relative offset. Returns 0 when the offset has not been set yet.
  uint32_t GetOffset(MethodReference method_ref) {
    auto it = method_offset_map_.map.find(method_ref);
    return (it != method_offset_map_.map.end()) ? it->second - adjustment_ : 0u;
  }

  // Set the offset.
  void SetOffset(MethodReference method_ref, uint32_t offset) {
    method_offset_map_.map.Put(method_ref, offset + adjustment_);
  }

  // Wrapper around RelativePatcher::ReserveSpace(), doing offset adjustment.
  uint32_t ReserveSpace(uint32_t offset,
                        const CompiledMethod* compiled_method,
                        MethodReference method_ref) {
    offset += adjustment_;
    offset = relative_patcher_->ReserveSpace(offset, compiled_method, method_ref);
    offset -= adjustment_;
    return offset;
  }

  // Wrapper around RelativePatcher::ReserveSpaceEnd(), doing offset adjustment.
  uint32_t ReserveSpaceEnd(uint32_t offset) {
    offset += adjustment_;
    offset = relative_patcher_->ReserveSpaceEnd(offset);
    offset -= adjustment_;
    return offset;
  }

  // Wrapper around RelativePatcher::WriteThunks(), doing offset adjustment.
  uint32_t WriteThunks(OutputStream* out, uint32_t offset) {
    offset += adjustment_;
    offset = relative_patcher_->WriteThunks(out, offset);
    if (offset != 0u) {  // 0u indicates write error.
      offset -= adjustment_;
    }
    return offset;
  }

  // Wrapper around RelativePatcher::PatchCall(), doing offset adjustment.
  void PatchCall(std::vector<uint8_t>* code,
                 uint32_t literal_offset,
                 uint32_t patch_offset,
                 uint32_t target_offset) {
    patch_offset += adjustment_;
    target_offset += adjustment_;
    relative_patcher_->PatchCall(code, literal_offset, patch_offset, target_offset);
  }

  // Wrapper around RelativePatcher::PatchPcRelativeReference(), doing offset adjustment.
  void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                const LinkerPatch& patch,
                                uint32_t patch_offset,
                                uint32_t target_offset) {
    patch_offset += adjustment_;
    target_offset += adjustment_;
    relative_patcher_->PatchPcRelativeReference(code, patch, patch_offset, target_offset);
  }

  void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                   const LinkerPatch& patch,
                                   uint32_t patch_offset) {
    patch_offset += adjustment_;
    relative_patcher_->PatchBakerReadBarrierBranch(code, patch, patch_offset);
  }

  std::vector<debug::MethodDebugInfo> GenerateThunkDebugInfo(size_t executable_offset) {
    executable_offset += adjustment_;
    return relative_patcher_->GenerateThunkDebugInfo(executable_offset);
  }

  // Wrappers around RelativePatcher for statistics retrieval.
  uint32_t CodeAlignmentSize() const;
  uint32_t RelativeCallThunksSize() const;
  uint32_t MiscThunksSize() const;

 private:
  // Map method reference to assigned offset.
  // Wrap the map in a class implementing RelativePatcherTargetProvider.
  class MethodOffsetMap : public RelativePatcherTargetProvider {
   public:
    std::pair<bool, uint32_t> FindMethodOffset(MethodReference ref) OVERRIDE;
    SafeMap<MethodReference, uint32_t> map;
  };

  MethodOffsetMap method_offset_map_;
  std::unique_ptr<RelativePatcher> relative_patcher_;
  uint32_t adjustment_;
  InstructionSet instruction_set_;

  uint32_t start_size_code_alignment_;
  uint32_t start_size_relative_call_thunks_;
  uint32_t start_size_misc_thunks_;

  friend class MultiOatRelativePatcherTest;

  DISALLOW_COPY_AND_ASSIGN(MultiOatRelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_MULTI_OAT_RELATIVE_PATCHER_H_
