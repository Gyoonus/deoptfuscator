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

#include "jni_macro_assembler.h"

#include <algorithm>
#include <vector>

#ifdef ART_ENABLE_CODEGEN_arm
#include "arm/jni_macro_assembler_arm_vixl.h"
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
#include "arm64/jni_macro_assembler_arm64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_mips
#include "mips/assembler_mips.h"
#endif
#ifdef ART_ENABLE_CODEGEN_mips64
#include "mips64/assembler_mips64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86
#include "x86/jni_macro_assembler_x86.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
#include "x86_64/jni_macro_assembler_x86_64.h"
#endif
#include "base/casts.h"
#include "globals.h"
#include "memory_region.h"

namespace art {

using MacroAsm32UniquePtr = std::unique_ptr<JNIMacroAssembler<PointerSize::k32>>;

template <>
MacroAsm32UniquePtr JNIMacroAssembler<PointerSize::k32>::Create(
    ArenaAllocator* allocator,
    InstructionSet instruction_set,
    const InstructionSetFeatures* instruction_set_features) {
#ifndef ART_ENABLE_CODEGEN_mips
  UNUSED(instruction_set_features);
#endif

  switch (instruction_set) {
#ifdef ART_ENABLE_CODEGEN_arm
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return MacroAsm32UniquePtr(new (allocator) arm::ArmVIXLJNIMacroAssembler(allocator));
#endif
#ifdef ART_ENABLE_CODEGEN_mips
    case InstructionSet::kMips:
      return MacroAsm32UniquePtr(new (allocator) mips::MipsAssembler(
          allocator,
          instruction_set_features != nullptr
              ? instruction_set_features->AsMipsInstructionSetFeatures()
              : nullptr));
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case InstructionSet::kX86:
      return MacroAsm32UniquePtr(new (allocator) x86::X86JNIMacroAssembler(allocator));
#endif
    default:
      LOG(FATAL) << "Unknown/unsupported 4B InstructionSet: " << instruction_set;
      UNREACHABLE();
  }
}

using MacroAsm64UniquePtr = std::unique_ptr<JNIMacroAssembler<PointerSize::k64>>;

template <>
MacroAsm64UniquePtr JNIMacroAssembler<PointerSize::k64>::Create(
    ArenaAllocator* allocator,
    InstructionSet instruction_set,
    const InstructionSetFeatures* instruction_set_features) {
#ifndef ART_ENABLE_CODEGEN_mips64
  UNUSED(instruction_set_features);
#endif

  switch (instruction_set) {
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64:
      return MacroAsm64UniquePtr(new (allocator) arm64::Arm64JNIMacroAssembler(allocator));
#endif
#ifdef ART_ENABLE_CODEGEN_mips64
    case InstructionSet::kMips64:
      return MacroAsm64UniquePtr(new (allocator) mips64::Mips64Assembler(
          allocator,
          instruction_set_features != nullptr
              ? instruction_set_features->AsMips64InstructionSetFeatures()
              : nullptr));
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
    case InstructionSet::kX86_64:
      return MacroAsm64UniquePtr(new (allocator) x86_64::X86_64JNIMacroAssembler(allocator));
#endif
    default:
      UNUSED(allocator);
      LOG(FATAL) << "Unknown/unsupported 8B InstructionSet: " << instruction_set;
      UNREACHABLE();
  }
}

}  // namespace art
