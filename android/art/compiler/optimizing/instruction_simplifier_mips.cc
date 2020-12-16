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

#include "instruction_simplifier_mips.h"

#include "arch/mips/instruction_set_features_mips.h"
#include "mirror/array-inl.h"

namespace art {
namespace mips {

class InstructionSimplifierMipsVisitor : public HGraphVisitor {
 public:
  InstructionSimplifierMipsVisitor(HGraph* graph,
                                   CodeGenerator* codegen,
                                   OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        stats_(stats),
        codegen_(down_cast<CodeGeneratorMIPS*>(codegen)) {}

 private:
  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool TryExtractArrayAccessIndex(HInstruction* access,
                                  HInstruction* index,
                                  DataType::Type packed_type);
  void VisitArrayGet(HArrayGet* instruction) OVERRIDE;
  void VisitArraySet(HArraySet* instruction) OVERRIDE;

  OptimizingCompilerStats* stats_;
  CodeGeneratorMIPS* codegen_;
};

bool InstructionSimplifierMipsVisitor::TryExtractArrayAccessIndex(HInstruction* access,
                                                                  HInstruction* index,
                                                                  DataType::Type packed_type) {
  if (codegen_->GetInstructionSetFeatures().IsR6() ||
      codegen_->GetInstructionSetFeatures().HasMsa()) {
    return false;
  }
  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // If index is constant the whole address calculation often can be done by load/store
    // instructions themselves.
    // TODO: Treat the case with non-embeddable constants.
    return false;
  }

  if (packed_type != DataType::Type::kInt16 && packed_type != DataType::Type::kUint16 &&
      packed_type != DataType::Type::kInt32 && packed_type != DataType::Type::kInt64 &&
      packed_type != DataType::Type::kFloat32 && packed_type != DataType::Type::kFloat64) {
    return false;
  }

  if (access->IsArrayGet() && access->AsArrayGet()->IsStringCharAt()) {
    return false;
  }

  HGraph* graph = access->GetBlock()->GetGraph();
  ArenaAllocator* allocator = graph->GetAllocator();
  size_t component_shift = DataType::SizeShift(packed_type);

  bool is_extracting_beneficial = false;
  // It is beneficial to extract index intermediate address only if there are at least 2 users.
  for (const HUseListNode<HInstruction*>& use : index->GetUses()) {
    HInstruction* user = use.GetUser();
    if (user->IsArrayGet() && user != access && !user->AsArrayGet()->IsStringCharAt()) {
      HArrayGet* another_access = user->AsArrayGet();
      DataType::Type another_packed_type = another_access->GetType();
      size_t another_component_shift = DataType::SizeShift(another_packed_type);
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    } else if (user->IsArraySet() && user != access) {
      HArraySet* another_access = user->AsArraySet();
      DataType::Type another_packed_type = another_access->GetType();
      size_t another_component_shift = DataType::SizeShift(another_packed_type);
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    } else if (user->IsIntermediateArrayAddressIndex()) {
      HIntermediateArrayAddressIndex* another_access = user->AsIntermediateArrayAddressIndex();
      size_t another_component_shift = another_access->GetShift()->AsIntConstant()->GetValue();
      if (another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    }
  }

  if (!is_extracting_beneficial) {
    return false;
  }

  HIntConstant* shift = graph->GetIntConstant(component_shift);
  HIntermediateArrayAddressIndex* address =
      new (allocator) HIntermediateArrayAddressIndex(index, shift, kNoDexPc);
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 1);
  return true;
}

void InstructionSimplifierMipsVisitor::VisitArrayGet(HArrayGet* instruction) {
  DataType::Type packed_type = instruction->GetType();
  if (TryExtractArrayAccessIndex(instruction, instruction->GetIndex(), packed_type)) {
    RecordSimplification();
  }
}

void InstructionSimplifierMipsVisitor::VisitArraySet(HArraySet* instruction) {
  DataType::Type packed_type = instruction->GetComponentType();
  if (TryExtractArrayAccessIndex(instruction, instruction->GetIndex(), packed_type)) {
    RecordSimplification();
  }
}

void InstructionSimplifierMips::Run() {
  InstructionSimplifierMipsVisitor visitor(graph_, codegen_, stats_);
  visitor.VisitReversePostOrder();
}

}  // namespace mips
}  // namespace art
