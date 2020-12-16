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

#ifndef ART_COMPILER_OPTIMIZING_CONSTRUCTOR_FENCE_REDUNDANCY_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_CONSTRUCTOR_FENCE_REDUNDANCY_ELIMINATION_H_

#include "optimization.h"

namespace art {

/*
 * Constructor Fence Redundancy Elimination (CFRE).
 *
 * A local optimization pass that merges redundant constructor fences
 * together within the same basic block.
 *
 * Abbreviations:
 * - CF: Constructor Fence
 * - CFS: Constructor Fence Set
 * - CFTargets: The unique set of the inputs of all the instructions in CFS.
 *
 * Given any CFS = { CF(x), CF(y), CF(z), ... }, define CFTargets = { x, y, z, ... }.
 * - Publish(R) must not exist for any R in CFTargets if this Publish(R) is between any CF in CFS.
 * - This type of Publish(R) is called an "interesting publish".
 *
 * A Publish(R) is considered any instruction at which the reference to "R"
 * may escape (e.g. invoke, store, return, etc) to another thread.
 *
 * Starting at the beginning of the block:
 * - Find the largest contiguous CFS.
 * - If we see an interesting publish, merge all instructions in CFS into a single CF(CFTargets).
 * - Repeat until the block is fully visited.
 * - At the end of the block, merge all instructions in CFS into a single CF(CFTargets).
 */
class ConstructorFenceRedundancyElimination : public HOptimization {
 public:
  ConstructorFenceRedundancyElimination(HGraph* graph,
                                        OptimizingCompilerStats* stats,
                                        const char* name = kCFREPassName)
      : HOptimization(graph, name, stats) {}

  void Run() OVERRIDE;

  static constexpr const char* kCFREPassName = "constructor_fence_redundancy_elimination";

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstructorFenceRedundancyElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CONSTRUCTOR_FENCE_REDUNDANCY_ELIMINATION_H_
