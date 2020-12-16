/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_DEX2OAT_LINKER_ELF_WRITER_QUICK_H_
#define ART_DEX2OAT_LINKER_ELF_WRITER_QUICK_H_

#include <memory>

#include "arch/instruction_set.h"
#include "base/os.h"
#include "elf_writer.h"

namespace art {

class CompilerOptions;
class InstructionSetFeatures;

namespace linker {

std::unique_ptr<ElfWriter> CreateElfWriterQuick(InstructionSet instruction_set,
                                                const InstructionSetFeatures* features,
                                                const CompilerOptions* compiler_options,
                                                File* elf_file);

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_ELF_WRITER_QUICK_H_
