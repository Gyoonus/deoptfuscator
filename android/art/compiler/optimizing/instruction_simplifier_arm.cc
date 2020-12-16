/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "instruction_simplifier_arm.h"

#include "code_generator.h"
#include "common_arm.h"
#include "instruction_simplifier_shared.h"
#include "mirror/array-inl.h"
#include "mirror/string.h"
#include "nodes.h"

namespace art {

using helpers::CanFitInShifterOperand;
using helpers::HasShifterOperand;

namespace arm {

class InstructionSimplifierArmVisitor : public HGraphVisitor {
 public:
  InstructionSimplifierArmVisitor(HGraph* graph, OptimizingCompilerStats* stats)
      : HGraphVisitor(graph), stats_(stats) {}

 private:
  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool TryMergeIntoUsersShifterOperand(HInstruction* instruction);
  bool TryMergeIntoShifterOperand(HInstruction* use, HInstruction* bitfield_op, bool do_merge);
  bool CanMergeIntoShifterOperand(HInstruction* use, HInstruction* bitfield_op) {
    return TryMergeIntoShifterOperand(use, bitfield_op, /* do_merge */ false);
  }
  bool MergeIntoShifterOperand(HInstruction* use, HInstruction* bitfield_op) {
    DCHECK(CanMergeIntoShifterOperand(use, bitfield_op));
    return TryMergeIntoShifterOperand(use, bitfield_op, /* do_merge */ true);
  }

  /**
   * This simplifier uses a special-purpose BB visitor.
   * (1) No need to visit Phi nodes.
   * (2) Since statements can be removed in a "forward" fashion,
   *     the visitor should test if each statement is still there.
   */
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // TODO: fragile iteration, provide more robust iterators?
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitArrayGet(HArrayGet* instruction) OVERRIDE;
  void VisitArraySet(HArraySet* instruction) OVERRIDE;
  void VisitMul(HMul* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitShl(HShl* instruction) OVERRIDE;
  void VisitShr(HShr* instruction) OVERRIDE;
  void VisitTypeConversion(HTypeConversion* instruction) OVERRIDE;
  void VisitUShr(HUShr* instruction) OVERRIDE;

  OptimizingCompilerStats* stats_;
};

bool InstructionSimplifierArmVisitor::TryMergeIntoShifterOperand(HInstruction* use,
                                                                 HInstruction* bitfield_op,
                                                                 bool do_merge) {
  DCHECK(HasShifterOperand(use, InstructionSet::kArm));
  DCHECK(use->IsBinaryOperation());
  DCHECK(CanFitInShifterOperand(bitfield_op));
  DCHECK(!bitfield_op->HasEnvironmentUses());

  DataType::Type type = use->GetType();
  if (type != DataType::Type::kInt32 && type != DataType::Type::kInt64) {
    return false;
  }

  HInstruction* left = use->InputAt(0);
  HInstruction* right = use->InputAt(1);
  DCHECK(left == bitfield_op || right == bitfield_op);

  if (left == right) {
    // TODO: Handle special transformations in this situation?
    // For example should we transform `(x << 1) + (x << 1)` into `(x << 2)`?
    // Or should this be part of a separate transformation logic?
    return false;
  }

  bool is_commutative = use->AsBinaryOperation()->IsCommutative();
  HInstruction* other_input;
  if (bitfield_op == right) {
    other_input = left;
  } else {
    if (is_commutative) {
      other_input = right;
    } else {
      return false;
    }
  }

  HDataProcWithShifterOp::OpKind op_kind;
  int shift_amount = 0;

  HDataProcWithShifterOp::GetOpInfoFromInstruction(bitfield_op, &op_kind, &shift_amount);
  shift_amount &= use->GetType() == DataType::Type::kInt32
      ? kMaxIntShiftDistance
      : kMaxLongShiftDistance;

  if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
    if (!use->IsAdd() && (!use->IsSub() || use->GetType() != DataType::Type::kInt64)) {
      return false;
    }
  // Shift by 1 is a special case that results in the same number and type of instructions
  // as this simplification, but potentially shorter code.
  } else if (type == DataType::Type::kInt64 && shift_amount == 1) {
    return false;
  }

  if (do_merge) {
    HDataProcWithShifterOp* alu_with_op =
        new (GetGraph()->GetAllocator()) HDataProcWithShifterOp(use,
                                                                other_input,
                                                                bitfield_op->InputAt(0),
                                                                op_kind,
                                                                shift_amount,
                                                                use->GetDexPc());
    use->GetBlock()->ReplaceAndRemoveInstructionWith(use, alu_with_op);
    if (bitfield_op->GetUses().empty()) {
      bitfield_op->GetBlock()->RemoveInstruction(bitfield_op);
    }
    RecordSimplification();
  }

  return true;
}

// Merge a bitfield move instruction into its uses if it can be merged in all of them.
bool InstructionSimplifierArmVisitor::TryMergeIntoUsersShifterOperand(HInstruction* bitfield_op) {
  DCHECK(CanFitInShifterOperand(bitfield_op));

  if (bitfield_op->HasEnvironmentUses()) {
    return false;
  }

  const HUseList<HInstruction*>& uses = bitfield_op->GetUses();

  // Check whether we can merge the instruction in all its users' shifter operand.
  for (const HUseListNode<HInstruction*>& use : uses) {
    HInstruction* user = use.GetUser();
    if (!HasShifterOperand(user, InstructionSet::kArm)) {
      return false;
    }
    if (!CanMergeIntoShifterOperand(user, bitfield_op)) {
      return false;
    }
  }

  // Merge the instruction into its uses.
  for (auto it = uses.begin(), end = uses.end(); it != end; /* ++it below */) {
    HInstruction* user = it->GetUser();
    // Increment `it` now because `*it` will disappear thanks to MergeIntoShifterOperand().
    ++it;
    bool merged = MergeIntoShifterOperand(user, bitfield_op);
    DCHECK(merged);
  }

  return true;
}

void InstructionSimplifierArmVisitor::VisitAnd(HAnd* instruction) {
  if (TryMergeNegatedInput(instruction)) {
    RecordSimplification();
  }
}

void InstructionSimplifierArmVisitor::VisitArrayGet(HArrayGet* instruction) {
  size_t data_offset = CodeGenerator::GetArrayDataOffset(instruction);
  DataType::Type type = instruction->GetType();

  // TODO: Implement reading (length + compression) for String compression feature from
  // negative offset (count_offset - data_offset). Thumb2Assembler (now removed) did
  // not support T4 encoding of "LDR (immediate)", but ArmVIXLMacroAssembler might.
  // Don't move array pointer if it is charAt because we need to take the count first.
  if (mirror::kUseStringCompression && instruction->IsStringCharAt()) {
    return;
  }

  if (type == DataType::Type::kInt64
      || type == DataType::Type::kFloat32
      || type == DataType::Type::kFloat64) {
    // T32 doesn't support ShiftedRegOffset mem address mode for these types
    // to enable optimization.
    return;
  }

  if (TryExtractArrayAccessAddress(instruction,
                                   instruction->GetArray(),
                                   instruction->GetIndex(),
                                   data_offset)) {
    RecordSimplification();
  }
}

void InstructionSimplifierArmVisitor::VisitArraySet(HArraySet* instruction) {
  size_t access_size = DataType::Size(instruction->GetComponentType());
  size_t data_offset = mirror::Array::DataOffset(access_size).Uint32Value();
  DataType::Type type = instruction->GetComponentType();

  if (type == DataType::Type::kInt64
      || type == DataType::Type::kFloat32
      || type == DataType::Type::kFloat64) {
    // T32 doesn't support ShiftedRegOffset mem address mode for these types
    // to enable optimization.
    return;
  }

  if (TryExtractArrayAccessAddress(instruction,
                                   instruction->GetArray(),
                                   instruction->GetIndex(),
                                   data_offset)) {
    RecordSimplification();
  }
}

void InstructionSimplifierArmVisitor::VisitMul(HMul* instruction) {
  if (TryCombineMultiplyAccumulate(instruction, InstructionSet::kArm)) {
    RecordSimplification();
  }
}

void InstructionSimplifierArmVisitor::VisitOr(HOr* instruction) {
  if (TryMergeNegatedInput(instruction)) {
    RecordSimplification();
  }
}

void InstructionSimplifierArmVisitor::VisitShl(HShl* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArmVisitor::VisitShr(HShr* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArmVisitor::VisitTypeConversion(HTypeConversion* instruction) {
  DataType::Type result_type = instruction->GetResultType();
  DataType::Type input_type = instruction->GetInputType();

  if (input_type == result_type) {
    // We let the arch-independent code handle this.
    return;
  }

  if (DataType::IsIntegralType(result_type) && DataType::IsIntegralType(input_type)) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArmVisitor::VisitUShr(HUShr* instruction) {
  if (instruction->InputAt(1)->IsConstant()) {
    TryMergeIntoUsersShifterOperand(instruction);
  }
}

void InstructionSimplifierArm::Run() {
  InstructionSimplifierArmVisitor visitor(graph_, stats_);
  visitor.VisitReversePostOrder();
}

}  // namespace arm
}  // namespace art
