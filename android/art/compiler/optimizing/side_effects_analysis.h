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

#ifndef ART_COMPILER_OPTIMIZING_SIDE_EFFECTS_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_SIDE_EFFECTS_ANALYSIS_H_

#include "base/arena_containers.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

class SideEffectsAnalysis : public HOptimization {
 public:
  explicit SideEffectsAnalysis(HGraph* graph, const char* pass_name = kSideEffectsAnalysisPassName)
      : HOptimization(graph, pass_name),
        graph_(graph),
        block_effects_(graph->GetBlocks().size(),
                       graph->GetAllocator()->Adapter(kArenaAllocSideEffectsAnalysis)),
        loop_effects_(graph->GetBlocks().size(),
                      graph->GetAllocator()->Adapter(kArenaAllocSideEffectsAnalysis)) {}

  SideEffects GetLoopEffects(HBasicBlock* block) const;
  SideEffects GetBlockEffects(HBasicBlock* block) const;

  // Compute side effects of individual blocks and loops.
  void Run();

  bool HasRun() const { return has_run_; }

  static constexpr const char* kSideEffectsAnalysisPassName = "side_effects";

 private:
  void UpdateLoopEffects(HLoopInformation* info, SideEffects effects);

  HGraph* graph_;

  // Checked in debug build, to ensure the pass has been run prior to
  // running a pass that depends on it.
  bool has_run_ = false;

  // Side effects of individual blocks, that is the union of the side effects
  // of the instructions in the block.
  ArenaVector<SideEffects> block_effects_;

  // Side effects of loops, that is the union of the side effects of the
  // blocks contained in that loop.
  ArenaVector<SideEffects> loop_effects_;

  ART_FRIEND_TEST(GVNTest, LoopSideEffects);
  DISALLOW_COPY_AND_ASSIGN(SideEffectsAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SIDE_EFFECTS_ANALYSIS_H_
