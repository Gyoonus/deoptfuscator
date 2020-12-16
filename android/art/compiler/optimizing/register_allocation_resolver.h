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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATION_RESOLVER_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATION_RESOLVER_H_

#include "base/array_ref.h"
#include "base/value_object.h"
#include "data_type.h"

namespace art {

class ArenaAllocator;
class CodeGenerator;
class HBasicBlock;
class HInstruction;
class HParallelMove;
class LiveInterval;
class Location;
class SsaLivenessAnalysis;

/**
 * Reconciles the locations assigned to live intervals with the location
 * summary of each instruction, and inserts moves to resolve split intervals,
 * nonlinear control flow, and phi inputs.
 */
class RegisterAllocationResolver : ValueObject {
 public:
  RegisterAllocationResolver(CodeGenerator* codegen, const SsaLivenessAnalysis& liveness);

  void Resolve(ArrayRef<HInstruction* const> safepoints,
               size_t reserved_out_slots,  // Includes slot(s) for the art method.
               size_t int_spill_slots,
               size_t long_spill_slots,
               size_t float_spill_slots,
               size_t double_spill_slots,
               size_t catch_phi_spill_slots,
               ArrayRef<LiveInterval* const> temp_intervals);

 private:
  // Update live registers of safepoint location summary.
  void UpdateSafepointLiveRegisters();

  // Calculate the maximum size of the spill area for safepoints.
  size_t CalculateMaximumSafepointSpillSize(ArrayRef<HInstruction* const> safepoints);

  // Connect adjacent siblings within blocks, and resolve inputs along the way.
  void ConnectSiblings(LiveInterval* interval);

  // Connect siblings between block entries and exits.
  void ConnectSplitSiblings(LiveInterval* interval, HBasicBlock* from, HBasicBlock* to) const;

  // Helper methods for inserting parallel moves in the graph.
  void InsertParallelMoveAtExitOf(HBasicBlock* block,
                                  HInstruction* instruction,
                                  Location source,
                                  Location destination) const;
  void InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                   HInstruction* instruction,
                                   Location source,
                                   Location destination) const;
  void InsertMoveAfter(HInstruction* instruction, Location source, Location destination) const;
  void AddInputMoveFor(HInstruction* input,
                       HInstruction* user,
                       Location source,
                       Location destination) const;
  void InsertParallelMoveAt(size_t position,
                            HInstruction* instruction,
                            Location source,
                            Location destination) const;
  void AddMove(HParallelMove* move,
               Location source,
               Location destination,
               HInstruction* instruction,
               DataType::Type type) const;

  ArenaAllocator* const allocator_;
  CodeGenerator* const codegen_;
  const SsaLivenessAnalysis& liveness_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAllocationResolver);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATION_RESOLVER_H_
