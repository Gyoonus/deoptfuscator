/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_

#include "nodes.h"
#include "optimization.h"
#include "optimizing_compiler_stats.h"

namespace art {

class CodeGenerator;
class CompilerDriver;

/**
 * Implements optimizations specific to each instruction.
 *
 * Note that graph simplifications producing a constant should be
 * implemented in art::HConstantFolding, while graph simplifications
 * not producing constants should be implemented in
 * art::InstructionSimplifier.  (This convention is a choice that was
 * made during the development of these parts of the compiler and is
 * not bound by any technical requirement.)
 */
class InstructionSimplifier : public HOptimization {
 public:
  InstructionSimplifier(HGraph* graph,
                        CodeGenerator* codegen,
                        CompilerDriver* compiler_driver,
                        OptimizingCompilerStats* stats = nullptr,
                        const char* name = kInstructionSimplifierPassName)
      : HOptimization(graph, name, stats),
        codegen_(codegen),
        compiler_driver_(compiler_driver) {}

  static constexpr const char* kInstructionSimplifierPassName = "instruction_simplifier";

  void Run() OVERRIDE;

 private:
  CodeGenerator* codegen_;
  CompilerDriver* compiler_driver_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSimplifier);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
