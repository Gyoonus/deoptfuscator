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

#ifndef ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_H_
#define ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_H_

#include "arch/instruction_set.h"
#include "base/array_ref.h"
#include "base/arena_object.h"
#include "base/macros.h"

namespace art {

class CodeGenerator;
class HBasicBlock;
class HGraph;
class HInstruction;
class HParallelMove;
class LiveInterval;
class Location;
class SsaLivenessAnalysis;

/**
 * Base class for any register allocator.
 */
class RegisterAllocator : public DeletableArenaObject<kArenaAllocRegisterAllocator> {
 public:
  enum Strategy {
    kRegisterAllocatorLinearScan,
    kRegisterAllocatorGraphColor
  };

  static constexpr Strategy kRegisterAllocatorDefault = kRegisterAllocatorLinearScan;

  static std::unique_ptr<RegisterAllocator> Create(ScopedArenaAllocator* allocator,
                                                   CodeGenerator* codegen,
                                                   const SsaLivenessAnalysis& analysis,
                                                   Strategy strategy = kRegisterAllocatorDefault);

  virtual ~RegisterAllocator();

  // Main entry point for the register allocator. Given the liveness analysis,
  // allocates registers to live intervals.
  virtual void AllocateRegisters() = 0;

  // Validate that the register allocator did not allocate the same register to
  // intervals that intersect each other. Returns false if it failed.
  virtual bool Validate(bool log_fatal_on_failure) = 0;

  static bool CanAllocateRegistersFor(const HGraph& graph,
                                      InstructionSet instruction_set);

  // Verifies that live intervals do not conflict. Used by unit testing.
  static bool ValidateIntervals(ArrayRef<LiveInterval* const> intervals,
                                size_t number_of_spill_slots,
                                size_t number_of_out_slots,
                                const CodeGenerator& codegen,
                                bool processing_core_registers,
                                bool log_fatal_on_failure);

  static constexpr const char* kRegisterAllocatorPassName = "register";

 protected:
  RegisterAllocator(ScopedArenaAllocator* allocator,
                    CodeGenerator* codegen,
                    const SsaLivenessAnalysis& analysis);

  // Split `interval` at the position `position`. The new interval starts at `position`.
  // If `position` is at the start of `interval`, returns `interval` with its
  // register location(s) cleared.
  static LiveInterval* Split(LiveInterval* interval, size_t position);

  // Split `interval` at a position between `from` and `to`. The method will try
  // to find an optimal split position.
  LiveInterval* SplitBetween(LiveInterval* interval, size_t from, size_t to);

  ScopedArenaAllocator* const allocator_;
  CodeGenerator* const codegen_;
  const SsaLivenessAnalysis& liveness_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_REGISTER_ALLOCATOR_H_
