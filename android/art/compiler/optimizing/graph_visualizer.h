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

#ifndef ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
#define ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_

#include <ostream>

#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/value_object.h"

namespace art {

class CodeGenerator;
class DexCompilationUnit;
class HGraph;
class HInstruction;
class SlowPathCode;

/**
 * This class outputs the HGraph in the C1visualizer format.
 * Note: Currently only works if the compiler is single threaded.
 */
struct GeneratedCodeInterval {
  size_t start;
  size_t end;
};

struct SlowPathCodeInfo {
  const SlowPathCode* slow_path;
  GeneratedCodeInterval code_interval;
};

// This information is filled by the code generator. It will be used by the
// graph visualizer to associate disassembly of the generated code with the
// instructions and slow paths. We assume that the generated code follows the
// following structure:
//   - frame entry
//   - instructions
//   - slow paths
class DisassemblyInformation {
 public:
  explicit DisassemblyInformation(ArenaAllocator* allocator)
      : frame_entry_interval_({0, 0}),
        instruction_intervals_(std::less<const HInstruction*>(), allocator->Adapter()),
        slow_path_intervals_(allocator->Adapter()) {}

  void SetFrameEntryInterval(size_t start, size_t end) {
    frame_entry_interval_ = {start, end};
  }

  void AddInstructionInterval(HInstruction* instr, size_t start, size_t end) {
    instruction_intervals_.Put(instr, {start, end});
  }

  void AddSlowPathInterval(SlowPathCode* slow_path, size_t start, size_t end) {
    slow_path_intervals_.push_back({slow_path, {start, end}});
  }

  GeneratedCodeInterval GetFrameEntryInterval() const {
    return frame_entry_interval_;
  }

  GeneratedCodeInterval* GetFrameEntryInterval() {
    return &frame_entry_interval_;
  }

  const ArenaSafeMap<const HInstruction*, GeneratedCodeInterval>& GetInstructionIntervals() const {
    return instruction_intervals_;
  }

  ArenaSafeMap<const HInstruction*, GeneratedCodeInterval>* GetInstructionIntervals() {
    return &instruction_intervals_;
  }

  const ArenaVector<SlowPathCodeInfo>& GetSlowPathIntervals() const { return slow_path_intervals_; }

  ArenaVector<SlowPathCodeInfo>* GetSlowPathIntervals() { return &slow_path_intervals_; }

 private:
  GeneratedCodeInterval frame_entry_interval_;
  ArenaSafeMap<const HInstruction*, GeneratedCodeInterval> instruction_intervals_;
  ArenaVector<SlowPathCodeInfo> slow_path_intervals_;
};

class HGraphVisualizer : public ValueObject {
 public:
  HGraphVisualizer(std::ostream* output,
                   HGraph* graph,
                   const CodeGenerator& codegen);

  void PrintHeader(const char* method_name) const;
  void DumpGraph(const char* pass_name, bool is_after_pass, bool graph_in_bad_state) const;
  void DumpGraphWithDisassembly() const;

 private:
  std::ostream* const output_;
  HGraph* const graph_;
  const CodeGenerator& codegen_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisualizer);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
