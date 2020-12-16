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

#ifndef ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_H_
#define ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_H_

#include "linker/x86/relative_patcher_x86_base.h"

namespace art {
namespace linker {

class X86RelativePatcher FINAL : public X86BaseRelativePatcher {
 public:
  X86RelativePatcher() { }

  void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                const LinkerPatch& patch,
                                uint32_t patch_offset,
                                uint32_t target_offset) OVERRIDE;
  void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                   const LinkerPatch& patch,
                                   uint32_t patch_offset) OVERRIDE;
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_X86_RELATIVE_PATCHER_X86_H_
