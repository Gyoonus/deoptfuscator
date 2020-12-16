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

#ifndef ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
#define ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Optimization phase that removes dead phis from the graph. Dead phis are unused
 * phis, or phis only used by other phis.
 */
class SsaDeadPhiElimination : public HOptimization {
 public:
  explicit SsaDeadPhiElimination(HGraph* graph)
      : HOptimization(graph, kSsaDeadPhiEliminationPassName) {}

  void Run() OVERRIDE;

  void MarkDeadPhis();
  void EliminateDeadPhis();

  static constexpr const char* kSsaDeadPhiEliminationPassName = "dead_phi_elimination";

 private:
  DISALLOW_COPY_AND_ASSIGN(SsaDeadPhiElimination);
};

/**
 * Removes redundant phis that may have been introduced when doing SSA conversion.
 * For example, when entering a loop, we create phis for all live registers. These
 * registers might be updated with the same value, or not updated at all. We can just
 * replace the phi with the value when entering the loop.
 */
class SsaRedundantPhiElimination : public HOptimization {
 public:
  explicit SsaRedundantPhiElimination(HGraph* graph)
      : HOptimization(graph, kSsaRedundantPhiEliminationPassName) {}

  void Run() OVERRIDE;

  static constexpr const char* kSsaRedundantPhiEliminationPassName = "redundant_phi_elimination";

 private:
  DISALLOW_COPY_AND_ASSIGN(SsaRedundantPhiElimination);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_PHI_ELIMINATION_H_
