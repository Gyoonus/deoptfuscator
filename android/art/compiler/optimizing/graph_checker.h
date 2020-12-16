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

#ifndef ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
#define ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_

#include <ostream>

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "nodes.h"

namespace art {

// A control-flow graph visitor performing various checks.
class GraphChecker : public HGraphDelegateVisitor {
 public:
  explicit GraphChecker(HGraph* graph, const char* dump_prefix = "art::GraphChecker: ")
    : HGraphDelegateVisitor(graph),
      errors_(graph->GetAllocator()->Adapter(kArenaAllocGraphChecker)),
      dump_prefix_(dump_prefix),
      allocator_(graph->GetArenaStack()),
      seen_ids_(&allocator_, graph->GetCurrentInstructionId(), false, kArenaAllocGraphChecker) {
    seen_ids_.ClearAllBits();
  }

  // Check the whole graph (in reverse post-order).
  void Run() {
    // VisitReversePostOrder is used instead of VisitInsertionOrder,
    // as the latter might visit dead blocks removed by the dominator
    // computation.
    VisitReversePostOrder();
  }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;

  void VisitInstruction(HInstruction* instruction) OVERRIDE;
  void VisitPhi(HPhi* phi) OVERRIDE;

  void VisitBinaryOperation(HBinaryOperation* op) OVERRIDE;
  void VisitBooleanNot(HBooleanNot* instruction) OVERRIDE;
  void VisitBoundType(HBoundType* instruction) OVERRIDE;
  void VisitBoundsCheck(HBoundsCheck* check) OVERRIDE;
  void VisitCheckCast(HCheckCast* check) OVERRIDE;
  void VisitCondition(HCondition* op) OVERRIDE;
  void VisitConstant(HConstant* instruction) OVERRIDE;
  void VisitDeoptimize(HDeoptimize* instruction) OVERRIDE;
  void VisitIf(HIf* instruction) OVERRIDE;
  void VisitInstanceOf(HInstanceOf* check) OVERRIDE;
  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) OVERRIDE;
  void VisitLoadException(HLoadException* load) OVERRIDE;
  void VisitNeg(HNeg* instruction) OVERRIDE;
  void VisitPackedSwitch(HPackedSwitch* instruction) OVERRIDE;
  void VisitReturn(HReturn* ret) OVERRIDE;
  void VisitReturnVoid(HReturnVoid* ret) OVERRIDE;
  void VisitSelect(HSelect* instruction) OVERRIDE;
  void VisitTryBoundary(HTryBoundary* try_boundary) OVERRIDE;
  void VisitTypeConversion(HTypeConversion* instruction) OVERRIDE;

  void HandleLoop(HBasicBlock* loop_header);
  void HandleBooleanInput(HInstruction* instruction, size_t input_index);

  // Was the last visit of the graph valid?
  bool IsValid() const {
    return errors_.empty();
  }

  // Get the list of detected errors.
  const ArenaVector<std::string>& GetErrors() const {
    return errors_;
  }

  // Print detected errors on output stream `os`.
  void Dump(std::ostream& os) const {
    for (size_t i = 0, e = errors_.size(); i < e; ++i) {
      os << dump_prefix_ << errors_[i] << std::endl;
    }
  }

 protected:
  // Report a new error.
  void AddError(const std::string& error) {
    errors_.push_back(error);
  }

  // The block currently visited.
  HBasicBlock* current_block_ = nullptr;
  // Errors encountered while checking the graph.
  ArenaVector<std::string> errors_;

 private:
  // String displayed before dumped errors.
  const char* const dump_prefix_;
  ScopedArenaAllocator allocator_;
  ArenaBitVector seen_ids_;

  DISALLOW_COPY_AND_ASSIGN(GraphChecker);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
