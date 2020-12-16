/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_MIPS_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_MIPS_H_

#include "nodes.h"
#include "optimization.h"
#include "code_generator_mips.h"

namespace art {

class CodeGenerator;

namespace mips {

class InstructionSimplifierMips : public HOptimization {
 public:
  InstructionSimplifierMips(HGraph* graph, CodeGenerator* codegen, OptimizingCompilerStats* stats)
      : HOptimization(graph, kInstructionSimplifierMipsPassName, stats),
        codegen_(down_cast<CodeGeneratorMIPS*>(codegen)) {}

  static constexpr const char* kInstructionSimplifierMipsPassName = "instruction_simplifier_mips";

  void Run() OVERRIDE;

 private:
  CodeGeneratorMIPS* codegen_;
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_MIPS_H_
