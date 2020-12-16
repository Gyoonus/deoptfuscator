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

#include "linker/x86/relative_patcher_x86.h"

#include "compiled_method.h"
#include "linker/linker_patch.h"

namespace art {
namespace linker {

void X86RelativePatcher::PatchPcRelativeReference(std::vector<uint8_t>* code,
                                                  const LinkerPatch& patch,
                                                  uint32_t patch_offset,
                                                  uint32_t target_offset) {
  uint32_t anchor_literal_offset = patch.PcInsnOffset();
  uint32_t literal_offset = patch.LiteralOffset();

  // Check that the anchor points to pop in a "call +0; pop <reg>" sequence.
  DCHECK_GE(anchor_literal_offset, 5u);
  DCHECK_LT(anchor_literal_offset, code->size());
  DCHECK_EQ((*code)[anchor_literal_offset - 5u], 0xe8u);
  DCHECK_EQ((*code)[anchor_literal_offset - 4u], 0x00u);
  DCHECK_EQ((*code)[anchor_literal_offset - 3u], 0x00u);
  DCHECK_EQ((*code)[anchor_literal_offset - 2u], 0x00u);
  DCHECK_EQ((*code)[anchor_literal_offset - 1u], 0x00u);
  DCHECK_EQ((*code)[anchor_literal_offset] & 0xf8u, 0x58u);

  // Check that the patched data contains kDummy32BitOffset.
  // Must match X86Mir2Lir::kDummy32BitOffset and CodeGeneratorX86_64::kDummy32BitOffset.
  constexpr int kDummy32BitOffset = 256;
  DCHECK_LE(literal_offset, code->size());
  DCHECK_EQ((*code)[literal_offset + 0u], static_cast<uint8_t>(kDummy32BitOffset >> 0));
  DCHECK_EQ((*code)[literal_offset + 1u], static_cast<uint8_t>(kDummy32BitOffset >> 8));
  DCHECK_EQ((*code)[literal_offset + 2u], static_cast<uint8_t>(kDummy32BitOffset >> 16));
  DCHECK_EQ((*code)[literal_offset + 3u], static_cast<uint8_t>(kDummy32BitOffset >> 24));

  // Apply patch.
  uint32_t anchor_offset = patch_offset - literal_offset + anchor_literal_offset;
  uint32_t diff = target_offset - anchor_offset;
  (*code)[literal_offset + 0u] = static_cast<uint8_t>(diff >> 0);
  (*code)[literal_offset + 1u] = static_cast<uint8_t>(diff >> 8);
  (*code)[literal_offset + 2u] = static_cast<uint8_t>(diff >> 16);
  (*code)[literal_offset + 3u] = static_cast<uint8_t>(diff >> 24);
}

void X86RelativePatcher::PatchBakerReadBarrierBranch(std::vector<uint8_t>* code ATTRIBUTE_UNUSED,
                                                     const LinkerPatch& patch ATTRIBUTE_UNUSED,
                                                     uint32_t patch_offset ATTRIBUTE_UNUSED) {
  LOG(FATAL) << "UNIMPLEMENTED";
}

}  // namespace linker
}  // namespace art
