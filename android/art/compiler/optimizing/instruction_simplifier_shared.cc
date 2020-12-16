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

#include "instruction_simplifier_shared.h"

#include "mirror/array-inl.h"

namespace art {

namespace {

bool TrySimpleMultiplyAccumulatePatterns(HMul* mul,
                                         HBinaryOperation* input_binop,
                                         HInstruction* input_other) {
  DCHECK(DataType::IsIntOrLongType(mul->GetType()));
  DCHECK(input_binop->IsAdd() || input_binop->IsSub());
  DCHECK_NE(input_binop, input_other);
  if (!input_binop->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }

  // Try to interpret patterns like
  //    a * (b <+/-> 1)
  // as
  //    (a * b) <+/-> a
  HInstruction* input_a = input_other;
  HInstruction* input_b = nullptr;  // Set to a non-null value if we found a pattern to optimize.
  HInstruction::InstructionKind op_kind;

  if (input_binop->IsAdd()) {
    if ((input_binop->GetConstantRight() != nullptr) && input_binop->GetConstantRight()->IsOne()) {
      // Interpret
      //    a * (b + 1)
      // as
      //    (a * b) + a
      input_b = input_binop->GetLeastConstantLeft();
      op_kind = HInstruction::kAdd;
    }
  } else {
    DCHECK(input_binop->IsSub());
    if (input_binop->GetRight()->IsConstant() &&
        input_binop->GetRight()->AsConstant()->IsMinusOne()) {
      // Interpret
      //    a * (b - (-1))
      // as
      //    a + (a * b)
      input_b = input_binop->GetLeft();
      op_kind = HInstruction::kAdd;
    } else if (input_binop->GetLeft()->IsConstant() &&
               input_binop->GetLeft()->AsConstant()->IsOne()) {
      // Interpret
      //    a * (1 - b)
      // as
      //    a - (a * b)
      input_b = input_binop->GetRight();
      op_kind = HInstruction::kSub;
    }
  }

  if (input_b == nullptr) {
    // We did not find a pattern we can optimize.
    return false;
  }

  ArenaAllocator* allocator = mul->GetBlock()->GetGraph()->GetAllocator();
  HMultiplyAccumulate* mulacc = new (allocator) HMultiplyAccumulate(
      mul->GetType(), op_kind, input_a, input_a, input_b, mul->GetDexPc());

  mul->GetBlock()->ReplaceAndRemoveInstructionWith(mul, mulacc);
  input_binop->GetBlock()->RemoveInstruction(input_binop);

  return true;
}

}  // namespace

bool TryCombineMultiplyAccumulate(HMul* mul, InstructionSet isa) {
  DataType::Type type = mul->GetType();
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      if (type != DataType::Type::kInt32) {
        return false;
      }
      break;
    case InstructionSet::kArm64:
      if (!DataType::IsIntOrLongType(type)) {
        return false;
      }
      break;
    default:
      return false;
  }

  ArenaAllocator* allocator = mul->GetBlock()->GetGraph()->GetAllocator();

  if (mul->HasOnlyOneNonEnvironmentUse()) {
    HInstruction* use = mul->GetUses().front().GetUser();
    if (use->IsAdd() || use->IsSub()) {
      // Replace code looking like
      //    MUL tmp, x, y
      //    SUB dst, acc, tmp
      // with
      //    MULSUB dst, acc, x, y
      // Note that we do not want to (unconditionally) perform the merge when the
      // multiplication has multiple uses and it can be merged in all of them.
      // Multiple uses could happen on the same control-flow path, and we would
      // then increase the amount of work. In the future we could try to evaluate
      // whether all uses are on different control-flow paths (using dominance and
      // reverse-dominance information) and only perform the merge when they are.
      HInstruction* accumulator = nullptr;
      HBinaryOperation* binop = use->AsBinaryOperation();
      HInstruction* binop_left = binop->GetLeft();
      HInstruction* binop_right = binop->GetRight();
      // Be careful after GVN. This should not happen since the `HMul` has only
      // one use.
      DCHECK_NE(binop_left, binop_right);
      if (binop_right == mul) {
        accumulator = binop_left;
      } else if (use->IsAdd()) {
        DCHECK_EQ(binop_left, mul);
        accumulator = binop_right;
      }

      if (accumulator != nullptr) {
        HMultiplyAccumulate* mulacc =
            new (allocator) HMultiplyAccumulate(type,
                                                binop->GetKind(),
                                                accumulator,
                                                mul->GetLeft(),
                                                mul->GetRight());

        binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, mulacc);
        DCHECK(!mul->HasUses());
        mul->GetBlock()->RemoveInstruction(mul);
        return true;
      }
    } else if (use->IsNeg() && isa != InstructionSet::kArm) {
      HMultiplyAccumulate* mulacc =
          new (allocator) HMultiplyAccumulate(type,
                                              HInstruction::kSub,
                                              mul->GetBlock()->GetGraph()->GetConstant(type, 0),
                                              mul->GetLeft(),
                                              mul->GetRight());

      use->GetBlock()->ReplaceAndRemoveInstructionWith(use, mulacc);
      DCHECK(!mul->HasUses());
      mul->GetBlock()->RemoveInstruction(mul);
      return true;
    }
  }

  // Use multiply accumulate instruction for a few simple patterns.
  // We prefer not applying the following transformations if the left and
  // right inputs perform the same operation.
  // We rely on GVN having squashed the inputs if appropriate. However the
  // results are still correct even if that did not happen.
  if (mul->GetLeft() == mul->GetRight()) {
    return false;
  }

  HInstruction* left = mul->GetLeft();
  HInstruction* right = mul->GetRight();
  if ((right->IsAdd() || right->IsSub()) &&
      TrySimpleMultiplyAccumulatePatterns(mul, right->AsBinaryOperation(), left)) {
    return true;
  }
  if ((left->IsAdd() || left->IsSub()) &&
      TrySimpleMultiplyAccumulatePatterns(mul, left->AsBinaryOperation(), right)) {
    return true;
  }
  return false;
}


bool TryMergeNegatedInput(HBinaryOperation* op) {
  DCHECK(op->IsAnd() || op->IsOr() || op->IsXor()) << op->DebugName();
  HInstruction* left = op->GetLeft();
  HInstruction* right = op->GetRight();

  // Only consider the case where there is exactly one Not, with 2 Not's De
  // Morgan's laws should be applied instead.
  if (left->IsNot() ^ right->IsNot()) {
    HInstruction* hnot = (left->IsNot() ? left : right);
    HInstruction* hother = (left->IsNot() ? right : left);

    // Only do the simplification if the Not has only one use and can thus be
    // safely removed. Even though ARM64 negated bitwise operations do not have
    // an immediate variant (only register), we still do the simplification when
    // `hother` is a constant, because it removes an instruction if the constant
    // cannot be encoded as an immediate:
    //   mov r0, #large_constant
    //   neg r2, r1
    //   and r0, r0, r2
    // becomes:
    //   mov r0, #large_constant
    //   bic r0, r0, r1
    if (hnot->HasOnlyOneNonEnvironmentUse()) {
      // Replace code looking like
      //    NOT tmp, mask
      //    AND dst, src, tmp   (respectively ORR, EOR)
      // with
      //    BIC dst, src, mask  (respectively ORN, EON)
      HInstruction* src = hnot->AsNot()->GetInput();

      HBitwiseNegatedRight* neg_op = new (hnot->GetBlock()->GetGraph()->GetAllocator())
          HBitwiseNegatedRight(op->GetType(), op->GetKind(), hother, src, op->GetDexPc());

      op->GetBlock()->ReplaceAndRemoveInstructionWith(op, neg_op);
      hnot->GetBlock()->RemoveInstruction(hnot);
      return true;
    }
  }

  return false;
}


bool TryExtractArrayAccessAddress(HInstruction* access,
                                  HInstruction* array,
                                  HInstruction* index,
                                  size_t data_offset) {
  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // When the index is a constant all the addressing can be fitted in the
    // memory access instruction, so do not split the access.
    return false;
  }
  if (access->IsArraySet() &&
      access->AsArraySet()->GetValue()->GetType() == DataType::Type::kReference) {
    // The access may require a runtime call or the original array pointer.
    return false;
  }
  if (kEmitCompilerReadBarrier &&
      access->IsArrayGet() &&
      access->GetType() == DataType::Type::kReference) {
    // For object arrays, the read barrier instrumentation requires
    // the original array pointer.
    // TODO: This can be relaxed for Baker CC.
    return false;
  }

  // Proceed to extract the base address computation.
  HGraph* graph = access->GetBlock()->GetGraph();
  ArenaAllocator* allocator = graph->GetAllocator();

  HIntConstant* offset = graph->GetIntConstant(data_offset);
  HIntermediateAddress* address = new (allocator) HIntermediateAddress(array, offset, kNoDexPc);
  // TODO: Is it ok to not have this on the intermediate address?
  // address->SetReferenceTypeInfo(array->GetReferenceTypeInfo());
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 0);
  // Both instructions must depend on GC to prevent any instruction that can
  // trigger GC to be inserted between the two.
  access->AddSideEffects(SideEffects::DependsOnGC());
  DCHECK(address->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  DCHECK(access->GetSideEffects().Includes(SideEffects::DependsOnGC()));
  // TODO: Code generation for HArrayGet and HArraySet will check whether the input address
  // is an HIntermediateAddress and generate appropriate code.
  // We would like to replace the `HArrayGet` and `HArraySet` with custom instructions (maybe
  // `HArm64Load` and `HArm64Store`,`HArmLoad` and `HArmStore`). We defer these changes
  // because these new instructions would not bring any advantages yet.
  // Also see the comments in
  // `InstructionCodeGeneratorARMVIXL::VisitArrayGet()`
  // `InstructionCodeGeneratorARMVIXL::VisitArraySet()`
  // `InstructionCodeGeneratorARM64::VisitArrayGet()`
  // `InstructionCodeGeneratorARM64::VisitArraySet()`.
  return true;
}

bool TryExtractVecArrayAccessAddress(HVecMemoryOperation* access, HInstruction* index) {
  if (index->IsConstant()) {
    // If index is constant the whole address calculation often can be done by LDR/STR themselves.
    // TODO: Treat the case with not-embedable constant.
    return false;
  }

  HGraph* graph = access->GetBlock()->GetGraph();
  ArenaAllocator* allocator = graph->GetAllocator();
  DataType::Type packed_type = access->GetPackedType();
  uint32_t data_offset = mirror::Array::DataOffset(
      DataType::Size(packed_type)).Uint32Value();
  size_t component_shift = DataType::SizeShift(packed_type);

  bool is_extracting_beneficial = false;
  // It is beneficial to extract index intermediate address only if there are at least 2 users.
  for (const HUseListNode<HInstruction*>& use : index->GetUses()) {
    HInstruction* user = use.GetUser();
    if (user->IsVecMemoryOperation() && user != access) {
      HVecMemoryOperation* another_access = user->AsVecMemoryOperation();
      DataType::Type another_packed_type = another_access->GetPackedType();
      uint32_t another_data_offset = mirror::Array::DataOffset(
          DataType::Size(another_packed_type)).Uint32Value();
      size_t another_component_shift = DataType::SizeShift(another_packed_type);
      if (another_data_offset == data_offset && another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    } else if (user->IsIntermediateAddressIndex()) {
      HIntermediateAddressIndex* another_access = user->AsIntermediateAddressIndex();
      uint32_t another_data_offset = another_access->GetOffset()->AsIntConstant()->GetValue();
      size_t another_component_shift = another_access->GetShift()->AsIntConstant()->GetValue();
      if (another_data_offset == data_offset && another_component_shift == component_shift) {
        is_extracting_beneficial = true;
        break;
      }
    }
  }

  if (!is_extracting_beneficial) {
    return false;
  }

  // Proceed to extract the index + data_offset address computation.
  HIntConstant* offset = graph->GetIntConstant(data_offset);
  HIntConstant* shift = graph->GetIntConstant(component_shift);
  HIntermediateAddressIndex* address =
      new (allocator) HIntermediateAddressIndex(index, offset, shift, kNoDexPc);

  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 1);

  return true;
}

}  // namespace art
