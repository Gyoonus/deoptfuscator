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

#ifndef ART_COMPILER_OPTIMIZING_CHA_GUARD_OPTIMIZATION_H_
#define ART_COMPILER_OPTIMIZING_CHA_GUARD_OPTIMIZATION_H_

#include "optimization.h"

namespace art {

/**
 * Optimize CHA guards by removing/moving them.
 */
class CHAGuardOptimization : public HOptimization {
 public:
  explicit CHAGuardOptimization(HGraph* graph,
                                const char* name = kCHAGuardOptimizationPassName)
      : HOptimization(graph, name) {}

  void Run() OVERRIDE;

  static constexpr const char* kCHAGuardOptimizationPassName = "cha_guard_optimization";

 private:
  DISALLOW_COPY_AND_ASSIGN(CHAGuardOptimization);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_CHA_GUARD_OPTIMIZATION_H_
