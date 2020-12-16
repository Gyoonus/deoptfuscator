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

#include "linker/mips/relative_patcher_mips.h"

#include "compiled_method.h"
#include "debug/method_debug_info.h"
#include "linker/linker_patch.h"

namespace art {
namespace linker {

uint32_t MipsRelativePatcher::ReserveSpace(
    uint32_t offset,
    const CompiledMethod* compiled_method ATTRIBUTE_UNUSED,
    MethodReference method_ref ATTRIBUTE_UNUSED) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t MipsRelativePatcher::ReserveSpaceEnd(uint32_t offset) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t MipsRelativePatcher::WriteThunks(OutputStream* out ATTRIBUTE_UNUSED, uint32_t offset) {
  return offset;  // No thunks added; no limit on relative call distance.
}

void MipsRelativePatcher::PatchCall(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                    uint32_t literal_offset ATTRIBUTE_UNUSED,
                                    uint32_t patch_offset ATTRIBUTE_UNUSED,
                                    uint32_t target_offset ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "PatchCall unimplemented on MIPS";
}

void MipsRelativePatcher::PatchPcRelativeReference(std::vector<uint8_t>* code,
                                                   const LinkerPatch& patch,
                                                   uint32_t patch_offset,
                                                   uint32_t target_offset) {
  uint32_t anchor_literal_offset = patch.PcInsnOffset();
  uint32_t literal_offset = patch.LiteralOffset();
  bool high_patch = ((*code)[literal_offset + 0] == 0x34) && ((*code)[literal_offset + 1] == 0x12);

  // Perform basic sanity checks.
  if (high_patch) {
    if (is_r6) {
      // auipc reg, offset_high
      DCHECK_EQ(((*code)[literal_offset + 2] & 0x1F), 0x1E);
      DCHECK_EQ(((*code)[literal_offset + 3] & 0xFC), 0xEC);
    } else {
      // lui reg, offset_high
      DCHECK_EQ(((*code)[literal_offset + 2] & 0xE0), 0x00);
      DCHECK_EQ((*code)[literal_offset + 3], 0x3C);
    }
  } else {
    // instr reg(s), offset_low
    CHECK_EQ((*code)[literal_offset + 0], 0x78);
    CHECK_EQ((*code)[literal_offset + 1], 0x56);
  }

  // Apply patch.
  uint32_t anchor_offset = patch_offset - literal_offset + anchor_literal_offset;
  uint32_t diff = target_offset - anchor_offset;
  diff += (diff & 0x8000) << 1;  // Account for sign extension in "instr reg(s), offset_low".

  if (high_patch) {
    // lui reg, offset_high / auipc reg, offset_high
    (*code)[literal_offset + 0] = static_cast<uint8_t>(diff >> 16);
    (*code)[literal_offset + 1] = static_cast<uint8_t>(diff >> 24);
  } else {
    // instr reg(s), offset_low
    (*code)[literal_offset + 0] = static_cast<uint8_t>(diff >> 0);
    (*code)[literal_offset + 1] = static_cast<uint8_t>(diff >> 8);
  }
}

void MipsRelativePatcher::PatchBakerReadBarrierBranch(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                                      const LinkerPatch& patch ATTRIBUTE_UNUSED,
                                                      uint32_t patch_offset ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "UNIMPLEMENTED";
}

std::vector<debug::MethodDebugInfo> MipsRelativePatcher::GenerateThunkDebugInfo(
    uint32_t executable_offset ATTRIBUTE_UNUSED) {
  return std::vector<debug::MethodDebugInfo>();  // No thunks added.
}

}  // namespace linker
}  // namespace art
