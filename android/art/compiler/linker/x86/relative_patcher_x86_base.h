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

#ifndef ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_BASE_H_
#define ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_BASE_H_

#include "linker/relative_patcher.h"

namespace art {
namespace linker {

class X86BaseRelativePatcher : public RelativePatcher {
 public:
  uint32_t ReserveSpace(uint32_t offset,
                        const CompiledMethod* compiled_method,
                        MethodReference method_ref) OVERRIDE;
  uint32_t ReserveSpaceEnd(uint32_t offset) OVERRIDE;
  uint32_t WriteThunks(OutputStream* out, uint32_t offset) OVERRIDE;
  void PatchCall(std::vector<uint8_t>* code,
                 uint32_t literal_offset,
                 uint32_t patch_offset,
                 uint32_t target_offset) OVERRIDE;
  std::vector<debug::MethodDebugInfo> GenerateThunkDebugInfo(uint32_t executable_offset) OVERRIDE;

 protected:
  X86BaseRelativePatcher() { }

  // PC displacement from patch location; the base address of x86/x86-64 relative
  // calls and x86-64 RIP-relative addressing is the PC of the next instruction and
  // the patch location is 4 bytes earlier.
  static constexpr int32_t kPcDisplacement = 4;

 private:
  DISALLOW_COPY_AND_ASSIGN(X86BaseRelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_BASE_H_
