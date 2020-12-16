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

#include "linker/x86/relative_patcher_x86_base.h"

#include "debug/method_debug_info.h"

namespace art {
namespace linker {

uint32_t X86BaseRelativePatcher::ReserveSpace(
    uint32_t offset,
    const CompiledMethod* compiled_method ATTRIBUTE_UNUSED,
    MethodReference method_ref ATTRIBUTE_UNUSED) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t X86BaseRelativePatcher::ReserveSpaceEnd(uint32_t offset) {
  return offset;  // No space reserved; no limit on relative call distance.
}

uint32_t X86BaseRelativePatcher::WriteThunks(OutputStream* out ATTRIBUTE_UNUSED, uint32_t offset) {
  return offset;  // No thunks added; no limit on relative call distance.
}

std::vector<debug::MethodDebugInfo> X86BaseRelativePatcher::GenerateThunkDebugInfo(
    uint32_t executable_offset ATTRIBUTE_UNUSED) {
  return std::vector<debug::MethodDebugInfo>();  // No thunks added.
}

void X86BaseRelativePatcher::PatchCall(std::vector<uint8_t>* code,
                                       uint32_t literal_offset,
                                       uint32_t patch_offset,
                                       uint32_t target_offset) {
  DCHECK_LE(literal_offset + 4u, code->size());
  // Unsigned arithmetic with its well-defined overflow behavior is just fine here.
  uint32_t displacement = target_offset - patch_offset;
  displacement -= kPcDisplacement;  // The base PC is at the end of the 4-byte patch.

  typedef __attribute__((__aligned__(1))) int32_t unaligned_int32_t;
  reinterpret_cast<unaligned_int32_t*>(&(*code)[literal_offset])[0] = displacement;
}

}  // namespace linker
}  // namespace art
