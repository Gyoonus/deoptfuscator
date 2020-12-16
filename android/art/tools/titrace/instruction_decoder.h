// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#ifndef ART_TOOLS_TITRACE_INSTRUCTION_DECODER_H_
#define ART_TOOLS_TITRACE_INSTRUCTION_DECODER_H_

#include <stddef.h>

namespace titrace {

enum class InstructionFileFormat {
  kClass,
  kDex
};

class InstructionDecoder {
 public:
  virtual size_t GetMaximumOpcode() = 0;
  virtual const char* GetName(size_t opcode) = 0;
  virtual size_t LocationToOffset(size_t j_location) = 0;

  virtual ~InstructionDecoder() {}

  static InstructionDecoder* NewInstance(InstructionFileFormat file_format);
};

}  // namespace titrace

#endif  // ART_TOOLS_TITRACE_INSTRUCTION_DECODER_H_
