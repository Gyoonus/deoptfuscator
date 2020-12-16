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

#ifndef ART_COMPILER_DEBUG_ELF_COMPILATION_UNIT_H_
#define ART_COMPILER_DEBUG_ELF_COMPILATION_UNIT_H_

#include <vector>

#include "debug/method_debug_info.h"

namespace art {
namespace debug {

struct ElfCompilationUnit {
  std::vector<const MethodDebugInfo*> methods;
  size_t debug_line_offset = 0;
  bool is_code_address_text_relative;  // Is the address offset from start of .text section?
  uint64_t code_address = std::numeric_limits<uint64_t>::max();
  uint64_t code_end = 0;
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_COMPILATION_UNIT_H_

