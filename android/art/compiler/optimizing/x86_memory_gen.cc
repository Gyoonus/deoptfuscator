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

#include "x86_memory_gen.h"
#include "code_generator.h"
#include "driver/compiler_options.h"

namespace art {
namespace x86 {

/**
 * Replace instructions with memory operand forms.
 */
class MemoryOperandVisitor : public HGraphVisitor {
 public:
  MemoryOperandVisitor(HGraph* graph, bool do_implicit_null_checks)
      : HGraphVisitor(graph),
        do_implicit_null_checks_(do_implicit_null_checks) {}

 private:
  void VisitBoundsCheck(HBoundsCheck* check) OVERRIDE {
    // Replace the length by the array itself, so that we can do compares to memory.
    HArrayLength* array_len = check->InputAt(1)->AsArrayLength();

    // We only want to replace an ArrayLength.
    if (array_len == nullptr) {
      return;
    }

    HInstruction* array = array_len->InputAt(0);
    DCHECK_EQ(array->GetType(), DataType::Type::kReference);

    // Don't apply this optimization when the array is nullptr.
    if (array->IsConstant() || (array->IsNullCheck() && array->InputAt(0)->IsConstant())) {
      return;
    }

    // Is there a null check that could be an implicit check?
    if (array->IsNullCheck() && do_implicit_null_checks_) {
      // The ArrayLen may generate the implicit null check.  Can the
      // bounds check do so as well?
      if (array_len->GetNextDisregardingMoves() != check) {
        // No, it won't.  Leave as is.
        return;
      }
    }

    // Can we suppress the ArrayLength and generate at BoundCheck?
    if (array_len->HasOnlyOneNonEnvironmentUse()) {
      array_len->MarkEmittedAtUseSite();
      // We need the ArrayLength just before the BoundsCheck.
      array_len->MoveBefore(check);
    }
  }

  bool do_implicit_null_checks_;
};

X86MemoryOperandGeneration::X86MemoryOperandGeneration(HGraph* graph,
                                                       CodeGenerator* codegen,
                                                       OptimizingCompilerStats* stats)
    : HOptimization(graph, kX86MemoryOperandGenerationPassName, stats),
      do_implicit_null_checks_(codegen->GetCompilerOptions().GetImplicitNullChecks()) {
}

void X86MemoryOperandGeneration::Run() {
  MemoryOperandVisitor visitor(graph_, do_implicit_null_checks_);
  visitor.VisitInsertionOrder();
}

}  // namespace x86
}  // namespace art
