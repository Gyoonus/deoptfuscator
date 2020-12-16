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

#ifndef ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_
#define ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_

#include <string>

#include "arch/instruction_set.h"
#include "base/array_ref.h"
#include "dex/dex_file.h"

namespace art {
namespace debug {

struct MethodDebugInfo {
  std::string custom_name;
  const DexFile* dex_file;  // Native methods (trampolines) do not reference dex file.
  size_t class_def_index;
  uint32_t dex_method_index;
  uint32_t access_flags;
  const DexFile::CodeItem* code_item;
  InstructionSet isa;
  bool deduped;
  bool is_native_debuggable;
  bool is_optimized;
  bool is_code_address_text_relative;  // Is the address offset from start of .text section?
  uint64_t code_address;
  uint32_t code_size;
  uint32_t frame_size_in_bytes;
  const void* code_info;
  ArrayRef<const uint8_t> cfi;
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_
