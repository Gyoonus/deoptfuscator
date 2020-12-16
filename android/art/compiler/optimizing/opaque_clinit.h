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

#ifndef ART_COMPILER_OPTIMIZING_OPAQUE_CLINIT_H_
#define ART_COMPILER_OPTIMIZING_OPAQUE_CLINIT_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

typedef std::vector<HInstruction *> HInstructionVector;

/**
 * Optimization pass performing a simple constant-expression
 * evaluation on the SSA form.
 *
 * Note that graph simplifications producing a constant should be
 * implemented in art::HOpaqueClinit, while graph simplifications
 * not producing constants should be implemented in
 * art::InstructionSimplifier.  (This convention is a choice that was
 * made during the development of these parts of the compiler and is
 * not bound by any technical requirement.)
 *
 * This class is named art::HOpaqueClinit to avoid name
 * clashes with the art::ConstantPropagation class defined in
 * compiler/dex/post_opt_passes.h.
 */
class HOpaqueClinit : public HOptimization {
 public:
  HOpaqueClinit(HGraph* graph, const char* name) : HOptimization(graph, name){}
  void Run(uint32_t ref_1, uint32_t ref_2, uint32_t code_off);
  void Run() OVERRIDE;

  static constexpr const char* kOpaqueClintPassName = "opaque_clinit";

 private:
  // reference index
  uint32_t ref_1_;
  uint32_t ref_2_;
  uint32_t code_off_;
  DISALLOW_COPY_AND_ASSIGN(HOpaqueClinit);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPAQUE_IDENTIFICATION_H_
