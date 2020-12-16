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

#include "instruction_simplifier.h"

#include "art_method-inl.h"
#include "class_linker-inl.h"
#include "data_type-inl.h"
#include "escape.h"
#include "intrinsics.h"
#include "mirror/class-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "sharpening.h"

namespace art {

// Whether to run an exhaustive test of individual HInstructions cloning when each instruction
// is replaced with its copy if it is clonable.
static constexpr bool kTestInstructionClonerExhaustively = false;

class InstructionSimplifierVisitor : public HGraphDelegateVisitor {
 public:
  InstructionSimplifierVisitor(HGraph* graph,
                               CodeGenerator* codegen,
                               CompilerDriver* compiler_driver,
                               OptimizingCompilerStats* stats)
      : HGraphDelegateVisitor(graph),
        codegen_(codegen),
        compiler_driver_(compiler_driver),
        stats_(stats) {}

  void Run();

 private:
  void RecordSimplification() {
    simplification_occurred_ = true;
    simplifications_at_current_position_++;
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplifications);
  }

  bool ReplaceRotateWithRor(HBinaryOperation* op, HUShr* ushr, HShl* shl);
  bool TryReplaceWithRotate(HBinaryOperation* instruction);
  bool TryReplaceWithRotateConstantPattern(HBinaryOperation* op, HUShr* ushr, HShl* shl);
  bool TryReplaceWithRotateRegisterNegPattern(HBinaryOperation* op, HUShr* ushr, HShl* shl);
  bool TryReplaceWithRotateRegisterSubPattern(HBinaryOperation* op, HUShr* ushr, HShl* shl);

  bool TryMoveNegOnInputsAfterBinop(HBinaryOperation* binop);
  // `op` should be either HOr or HAnd.
  // De Morgan's laws:
  // ~a & ~b = ~(a | b)  and  ~a | ~b = ~(a & b)
  bool TryDeMorganNegationFactoring(HBinaryOperation* op);
  bool TryHandleAssociativeAndCommutativeOperation(HBinaryOperation* instruction);
  bool TrySubtractionChainSimplification(HBinaryOperation* instruction);
  bool TryCombineVecMultiplyAccumulate(HVecMul* mul);

  void VisitShift(HBinaryOperation* shift);

  void VisitEqual(HEqual* equal) OVERRIDE;
  void VisitNotEqual(HNotEqual* equal) OVERRIDE;
  void VisitBooleanNot(HBooleanNot* bool_not) OVERRIDE;
  void VisitInstanceFieldSet(HInstanceFieldSet* equal) OVERRIDE;
  void VisitStaticFieldSet(HStaticFieldSet* equal) OVERRIDE;
  void VisitArraySet(HArraySet* equal) OVERRIDE;
  void VisitTypeConversion(HTypeConversion* instruction) OVERRIDE;
  void VisitNullCheck(HNullCheck* instruction) OVERRIDE;
  void VisitArrayLength(HArrayLength* instruction) OVERRIDE;
  void VisitCheckCast(HCheckCast* instruction) OVERRIDE;
  void VisitAdd(HAdd* instruction) OVERRIDE;
  void VisitAnd(HAnd* instruction) OVERRIDE;
  void VisitCondition(HCondition* instruction) OVERRIDE;
  void VisitGreaterThan(HGreaterThan* condition) OVERRIDE;
  void VisitGreaterThanOrEqual(HGreaterThanOrEqual* condition) OVERRIDE;
  void VisitLessThan(HLessThan* condition) OVERRIDE;
  void VisitLessThanOrEqual(HLessThanOrEqual* condition) OVERRIDE;
  void VisitBelow(HBelow* condition) OVERRIDE;
  void VisitBelowOrEqual(HBelowOrEqual* condition) OVERRIDE;
  void VisitAbove(HAbove* condition) OVERRIDE;
  void VisitAboveOrEqual(HAboveOrEqual* condition) OVERRIDE;
  void VisitDiv(HDiv* instruction) OVERRIDE;
  void VisitMul(HMul* instruction) OVERRIDE;
  void VisitNeg(HNeg* instruction) OVERRIDE;
  void VisitNot(HNot* instruction) OVERRIDE;
  void VisitOr(HOr* instruction) OVERRIDE;
  void VisitShl(HShl* instruction) OVERRIDE;
  void VisitShr(HShr* instruction) OVERRIDE;
  void VisitSub(HSub* instruction) OVERRIDE;
  void VisitUShr(HUShr* instruction) OVERRIDE;
  void VisitXor(HXor* instruction) OVERRIDE;
  void VisitSelect(HSelect* select) OVERRIDE;
  void VisitIf(HIf* instruction) OVERRIDE;
  void VisitInstanceOf(HInstanceOf* instruction) OVERRIDE;
  void VisitInvoke(HInvoke* invoke) OVERRIDE;
  void VisitDeoptimize(HDeoptimize* deoptimize) OVERRIDE;
  void VisitVecMul(HVecMul* instruction) OVERRIDE;

  bool CanEnsureNotNullAt(HInstruction* instr, HInstruction* at) const;

  void SimplifyRotate(HInvoke* invoke, bool is_left, DataType::Type type);
  void SimplifySystemArrayCopy(HInvoke* invoke);
  void SimplifyStringEquals(HInvoke* invoke);
  void SimplifyCompare(HInvoke* invoke, bool is_signum, DataType::Type type);
  void SimplifyIsNaN(HInvoke* invoke);
  void SimplifyFP2Int(HInvoke* invoke);
  void SimplifyStringCharAt(HInvoke* invoke);
  void SimplifyStringIsEmptyOrLength(HInvoke* invoke);
  void SimplifyNPEOnArgN(HInvoke* invoke, size_t);
  void SimplifyReturnThis(HInvoke* invoke);
  void SimplifyAllocationIntrinsic(HInvoke* invoke);
  void SimplifyMemBarrier(HInvoke* invoke, MemBarrierKind barrier_kind);

  CodeGenerator* codegen_;
  CompilerDriver* compiler_driver_;
  OptimizingCompilerStats* stats_;
  bool simplification_occurred_ = false;
  int simplifications_at_current_position_ = 0;
  // We ensure we do not loop infinitely. The value should not be too high, since that
  // would allow looping around the same basic block too many times. The value should
  // not be too low either, however, since we want to allow revisiting a basic block
  // with many statements and simplifications at least once.
  static constexpr int kMaxSamePositionSimplifications = 50;
};

void InstructionSimplifier::Run() {
  if (kTestInstructionClonerExhaustively) {
    CloneAndReplaceInstructionVisitor visitor(graph_);
    visitor.VisitReversePostOrder();
  }

  InstructionSimplifierVisitor visitor(graph_, codegen_, compiler_driver_, stats_);
  visitor.Run();
}

void InstructionSimplifierVisitor::Run() {
  // Iterate in reverse post order to open up more simplifications to users
  // of instructions that got simplified.
  for (HBasicBlock* block : GetGraph()->GetReversePostOrder()) {
    // The simplification of an instruction to another instruction may yield
    // possibilities for other simplifications. So although we perform a reverse
    // post order visit, we sometimes need to revisit an instruction index.
    do {
      simplification_occurred_ = false;
      VisitBasicBlock(block);
    } while (simplification_occurred_ &&
             (simplifications_at_current_position_ < kMaxSamePositionSimplifications));
    simplifications_at_current_position_ = 0;
  }
}

namespace {

bool AreAllBitsSet(HConstant* constant) {
  return Int64FromConstant(constant) == -1;
}

}  // namespace

// Returns true if the code was simplified to use only one negation operation
// after the binary operation instead of one on each of the inputs.
bool InstructionSimplifierVisitor::TryMoveNegOnInputsAfterBinop(HBinaryOperation* binop) {
  DCHECK(binop->IsAdd() || binop->IsSub());
  DCHECK(binop->GetLeft()->IsNeg() && binop->GetRight()->IsNeg());
  HNeg* left_neg = binop->GetLeft()->AsNeg();
  HNeg* right_neg = binop->GetRight()->AsNeg();
  if (!left_neg->HasOnlyOneNonEnvironmentUse() ||
      !right_neg->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }
  // Replace code looking like
  //    NEG tmp1, a
  //    NEG tmp2, b
  //    ADD dst, tmp1, tmp2
  // with
  //    ADD tmp, a, b
  //    NEG dst, tmp
  // Note that we cannot optimize `(-a) + (-b)` to `-(a + b)` for floating-point.
  // When `a` is `-0.0` and `b` is `0.0`, the former expression yields `0.0`,
  // while the later yields `-0.0`.
  if (!DataType::IsIntegralType(binop->GetType())) {
    return false;
  }
  binop->ReplaceInput(left_neg->GetInput(), 0);
  binop->ReplaceInput(right_neg->GetInput(), 1);
  left_neg->GetBlock()->RemoveInstruction(left_neg);
  right_neg->GetBlock()->RemoveInstruction(right_neg);
  HNeg* neg = new (GetGraph()->GetAllocator()) HNeg(binop->GetType(), binop);
  binop->GetBlock()->InsertInstructionBefore(neg, binop->GetNext());
  binop->ReplaceWithExceptInReplacementAtIndex(neg, 0);
  RecordSimplification();
  return true;
}

bool InstructionSimplifierVisitor::TryDeMorganNegationFactoring(HBinaryOperation* op) {
  DCHECK(op->IsAnd() || op->IsOr()) << op->DebugName();
  DataType::Type type = op->GetType();
  HInstruction* left = op->GetLeft();
  HInstruction* right = op->GetRight();

  // We can apply De Morgan's laws if both inputs are Not's and are only used
  // by `op`.
  if (((left->IsNot() && right->IsNot()) ||
       (left->IsBooleanNot() && right->IsBooleanNot())) &&
      left->HasOnlyOneNonEnvironmentUse() &&
      right->HasOnlyOneNonEnvironmentUse()) {
    // Replace code looking like
    //    NOT nota, a
    //    NOT notb, b
    //    AND dst, nota, notb (respectively OR)
    // with
    //    OR or, a, b         (respectively AND)
    //    NOT dest, or
    HInstruction* src_left = left->InputAt(0);
    HInstruction* src_right = right->InputAt(0);
    uint32_t dex_pc = op->GetDexPc();

    // Remove the negations on the inputs.
    left->ReplaceWith(src_left);
    right->ReplaceWith(src_right);
    left->GetBlock()->RemoveInstruction(left);
    right->GetBlock()->RemoveInstruction(right);

    // Replace the `HAnd` or `HOr`.
    HBinaryOperation* hbin;
    if (op->IsAnd()) {
      hbin = new (GetGraph()->GetAllocator()) HOr(type, src_left, src_right, dex_pc);
    } else {
      hbin = new (GetGraph()->GetAllocator()) HAnd(type, src_left, src_right, dex_pc);
    }
    HInstruction* hnot;
    if (left->IsBooleanNot()) {
      hnot = new (GetGraph()->GetAllocator()) HBooleanNot(hbin, dex_pc);
    } else {
      hnot = new (GetGraph()->GetAllocator()) HNot(type, hbin, dex_pc);
    }

    op->GetBlock()->InsertInstructionBefore(hbin, op);
    op->GetBlock()->ReplaceAndRemoveInstructionWith(op, hnot);

    RecordSimplification();
    return true;
  }

  return false;
}

bool InstructionSimplifierVisitor::TryCombineVecMultiplyAccumulate(HVecMul* mul) {
  DataType::Type type = mul->GetPackedType();
  InstructionSet isa = codegen_->GetInstructionSet();
  switch (isa) {
    case InstructionSet::kArm64:
      if (!(type == DataType::Type::kUint8 ||
            type == DataType::Type::kInt8 ||
            type == DataType::Type::kUint16 ||
            type == DataType::Type::kInt16 ||
            type == DataType::Type::kInt32)) {
        return false;
      }
      break;
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
      if (!(type == DataType::Type::kUint8 ||
            type == DataType::Type::kInt8 ||
            type == DataType::Type::kUint16 ||
            type == DataType::Type::kInt16 ||
            type == DataType::Type::kInt32 ||
            type == DataType::Type::kInt64)) {
        return false;
      }
      break;
    default:
      return false;
  }

  ArenaAllocator* allocator = mul->GetBlock()->GetGraph()->GetAllocator();

  if (mul->HasOnlyOneNonEnvironmentUse()) {
    HInstruction* use = mul->GetUses().front().GetUser();
    if (use->IsVecAdd() || use->IsVecSub()) {
      // Replace code looking like
      //    VECMUL tmp, x, y
      //    VECADD/SUB dst, acc, tmp
      // with
      //    VECMULACC dst, acc, x, y
      // Note that we do not want to (unconditionally) perform the merge when the
      // multiplication has multiple uses and it can be merged in all of them.
      // Multiple uses could happen on the same control-flow path, and we would
      // then increase the amount of work. In the future we could try to evaluate
      // whether all uses are on different control-flow paths (using dominance and
      // reverse-dominance information) and only perform the merge when they are.
      HInstruction* accumulator = nullptr;
      HVecBinaryOperation* binop = use->AsVecBinaryOperation();
      HInstruction* binop_left = binop->GetLeft();
      HInstruction* binop_right = binop->GetRight();
      // This is always true since the `HVecMul` has only one use (which is checked above).
      DCHECK_NE(binop_left, binop_right);
      if (binop_right == mul) {
        accumulator = binop_left;
      } else if (use->IsVecAdd()) {
        DCHECK_EQ(binop_left, mul);
        accumulator = binop_right;
      }

      HInstruction::InstructionKind kind =
          use->IsVecAdd() ? HInstruction::kAdd : HInstruction::kSub;
      if (accumulator != nullptr) {
        HVecMultiplyAccumulate* mulacc =
            new (allocator) HVecMultiplyAccumulate(allocator,
                                                   kind,
                                                   accumulator,
                                                   mul->GetLeft(),
                                                   mul->GetRight(),
                                                   binop->GetPackedType(),
                                                   binop->GetVectorLength(),
                                                   binop->GetDexPc());

        binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, mulacc);
        DCHECK(!mul->HasUses());
        mul->GetBlock()->RemoveInstruction(mul);
        return true;
      }
    }
  }

  return false;
}

void InstructionSimplifierVisitor::VisitShift(HBinaryOperation* instruction) {
  DCHECK(instruction->IsShl() || instruction->IsShr() || instruction->IsUShr());
  HInstruction* shift_amount = instruction->GetRight();
  HInstruction* value = instruction->GetLeft();

  int64_t implicit_mask = (value->GetType() == DataType::Type::kInt64)
      ? kMaxLongShiftDistance
      : kMaxIntShiftDistance;

  if (shift_amount->IsConstant()) {
    int64_t cst = Int64FromConstant(shift_amount->AsConstant());
    int64_t masked_cst = cst & implicit_mask;
    if (masked_cst == 0) {
      // Replace code looking like
      //    SHL dst, value, 0
      // with
      //    value
      instruction->ReplaceWith(value);
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    } else if (masked_cst != cst) {
      // Replace code looking like
      //    SHL dst, value, cst
      // where cst exceeds maximum distance with the equivalent
      //    SHL dst, value, cst & implicit_mask
      // (as defined by shift semantics). This ensures other
      // optimizations do not need to special case for such situations.
      DCHECK_EQ(shift_amount->GetType(), DataType::Type::kInt32);
      instruction->ReplaceInput(GetGraph()->GetIntConstant(masked_cst), /* index */ 1);
      RecordSimplification();
      return;
    }
  }

  // Shift operations implicitly mask the shift amount according to the type width. Get rid of
  // unnecessary And/Or/Xor/Add/Sub/TypeConversion operations on the shift amount that do not
  // affect the relevant bits.
  // Replace code looking like
  //    AND adjusted_shift, shift, <superset of implicit mask>
  //    [OR/XOR/ADD/SUB adjusted_shift, shift, <value not overlapping with implicit mask>]
  //    [<conversion-from-integral-non-64-bit-type> adjusted_shift, shift]
  //    SHL dst, value, adjusted_shift
  // with
  //    SHL dst, value, shift
  if (shift_amount->IsAnd() ||
      shift_amount->IsOr() ||
      shift_amount->IsXor() ||
      shift_amount->IsAdd() ||
      shift_amount->IsSub()) {
    int64_t required_result = shift_amount->IsAnd() ? implicit_mask : 0;
    HBinaryOperation* bin_op = shift_amount->AsBinaryOperation();
    HConstant* mask = bin_op->GetConstantRight();
    if (mask != nullptr && (Int64FromConstant(mask) & implicit_mask) == required_result) {
      instruction->ReplaceInput(bin_op->GetLeastConstantLeft(), 1);
      RecordSimplification();
      return;
    }
  } else if (shift_amount->IsTypeConversion()) {
    DCHECK_NE(shift_amount->GetType(), DataType::Type::kBool);  // We never convert to bool.
    DataType::Type source_type = shift_amount->InputAt(0)->GetType();
    // Non-integral and 64-bit source types require an explicit type conversion.
    if (DataType::IsIntegralType(source_type) && !DataType::Is64BitType(source_type)) {
      instruction->ReplaceInput(shift_amount->AsTypeConversion()->GetInput(), 1);
      RecordSimplification();
      return;
    }
  }
}

static bool IsSubRegBitsMinusOther(HSub* sub, size_t reg_bits, HInstruction* other) {
  return (sub->GetRight() == other &&
          sub->GetLeft()->IsConstant() &&
          (Int64FromConstant(sub->GetLeft()->AsConstant()) & (reg_bits - 1)) == 0);
}

bool InstructionSimplifierVisitor::ReplaceRotateWithRor(HBinaryOperation* op,
                                                        HUShr* ushr,
                                                        HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr()) << op->DebugName();
  HRor* ror =
      new (GetGraph()->GetAllocator()) HRor(ushr->GetType(), ushr->GetLeft(), ushr->GetRight());
  op->GetBlock()->ReplaceAndRemoveInstructionWith(op, ror);
  if (!ushr->HasUses()) {
    ushr->GetBlock()->RemoveInstruction(ushr);
  }
  if (!ushr->GetRight()->HasUses()) {
    ushr->GetRight()->GetBlock()->RemoveInstruction(ushr->GetRight());
  }
  if (!shl->HasUses()) {
    shl->GetBlock()->RemoveInstruction(shl);
  }
  if (!shl->GetRight()->HasUses()) {
    shl->GetRight()->GetBlock()->RemoveInstruction(shl->GetRight());
  }
  RecordSimplification();
  return true;
}

// Try to replace a binary operation flanked by one UShr and one Shl with a bitfield rotation.
bool InstructionSimplifierVisitor::TryReplaceWithRotate(HBinaryOperation* op) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  HInstruction* left = op->GetLeft();
  HInstruction* right = op->GetRight();
  // If we have an UShr and a Shl (in either order).
  if ((left->IsUShr() && right->IsShl()) || (left->IsShl() && right->IsUShr())) {
    HUShr* ushr = left->IsUShr() ? left->AsUShr() : right->AsUShr();
    HShl* shl = left->IsShl() ? left->AsShl() : right->AsShl();
    DCHECK(DataType::IsIntOrLongType(ushr->GetType()));
    if (ushr->GetType() == shl->GetType() &&
        ushr->GetLeft() == shl->GetLeft()) {
      if (ushr->GetRight()->IsConstant() && shl->GetRight()->IsConstant()) {
        // Shift distances are both constant, try replacing with Ror if they
        // add up to the register size.
        return TryReplaceWithRotateConstantPattern(op, ushr, shl);
      } else if (ushr->GetRight()->IsSub() || shl->GetRight()->IsSub()) {
        // Shift distances are potentially of the form x and (reg_size - x).
        return TryReplaceWithRotateRegisterSubPattern(op, ushr, shl);
      } else if (ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg()) {
        // Shift distances are potentially of the form d and -d.
        return TryReplaceWithRotateRegisterNegPattern(op, ushr, shl);
      }
    }
  }
  return false;
}

// Try replacing code looking like (x >>> #rdist OP x << #ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #ldist
//    OP   dst, dst, tmp
// or like (x >>> #rdist OP x << #-ldist):
//    UShr dst, x,   #rdist
//    Shl  tmp, x,   #-ldist
//    OP   dst, dst, tmp
// with
//    Ror  dst, x,   #rdist
bool InstructionSimplifierVisitor::TryReplaceWithRotateConstantPattern(HBinaryOperation* op,
                                                                       HUShr* ushr,
                                                                       HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  size_t reg_bits = DataType::Size(ushr->GetType()) * kBitsPerByte;
  size_t rdist = Int64FromConstant(ushr->GetRight()->AsConstant());
  size_t ldist = Int64FromConstant(shl->GetRight()->AsConstant());
  if (((ldist + rdist) & (reg_bits - 1)) == 0) {
    ReplaceRotateWithRor(op, ushr, shl);
    return true;
  }
  return false;
}

// Replace code looking like (x >>> -d OP x << d):
//    Neg  neg, d
//    UShr dst, x,   neg
//    Shl  tmp, x,   d
//    OP   dst, dst, tmp
// with
//    Neg  neg, d
//    Ror  dst, x,   neg
// *** OR ***
// Replace code looking like (x >>> d OP x << -d):
//    UShr dst, x,   d
//    Neg  neg, d
//    Shl  tmp, x,   neg
//    OP   dst, dst, tmp
// with
//    Ror  dst, x,   d
bool InstructionSimplifierVisitor::TryReplaceWithRotateRegisterNegPattern(HBinaryOperation* op,
                                                                          HUShr* ushr,
                                                                          HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  DCHECK(ushr->GetRight()->IsNeg() || shl->GetRight()->IsNeg());
  bool neg_is_left = shl->GetRight()->IsNeg();
  HNeg* neg = neg_is_left ? shl->GetRight()->AsNeg() : ushr->GetRight()->AsNeg();
  // And the shift distance being negated is the distance being shifted the other way.
  if (neg->InputAt(0) == (neg_is_left ? ushr->GetRight() : shl->GetRight())) {
    ReplaceRotateWithRor(op, ushr, shl);
  }
  return false;
}

// Try replacing code looking like (x >>> d OP x << (#bits - d)):
//    UShr dst, x,     d
//    Sub  ld,  #bits, d
//    Shl  tmp, x,     ld
//    OP   dst, dst,   tmp
// with
//    Ror  dst, x,     d
// *** OR ***
// Replace code looking like (x >>> (#bits - d) OP x << d):
//    Sub  rd,  #bits, d
//    UShr dst, x,     rd
//    Shl  tmp, x,     d
//    OP   dst, dst,   tmp
// with
//    Neg  neg, d
//    Ror  dst, x,     neg
bool InstructionSimplifierVisitor::TryReplaceWithRotateRegisterSubPattern(HBinaryOperation* op,
                                                                          HUShr* ushr,
                                                                          HShl* shl) {
  DCHECK(op->IsAdd() || op->IsXor() || op->IsOr());
  DCHECK(ushr->GetRight()->IsSub() || shl->GetRight()->IsSub());
  size_t reg_bits = DataType::Size(ushr->GetType()) * kBitsPerByte;
  HInstruction* shl_shift = shl->GetRight();
  HInstruction* ushr_shift = ushr->GetRight();
  if ((shl_shift->IsSub() && IsSubRegBitsMinusOther(shl_shift->AsSub(), reg_bits, ushr_shift)) ||
      (ushr_shift->IsSub() && IsSubRegBitsMinusOther(ushr_shift->AsSub(), reg_bits, shl_shift))) {
    return ReplaceRotateWithRor(op, ushr, shl);
  }
  return false;
}

void InstructionSimplifierVisitor::VisitNullCheck(HNullCheck* null_check) {
  HInstruction* obj = null_check->InputAt(0);
  if (!obj->CanBeNull()) {
    null_check->ReplaceWith(obj);
    null_check->GetBlock()->RemoveInstruction(null_check);
    if (stats_ != nullptr) {
      stats_->RecordStat(MethodCompilationStat::kRemovedNullCheck);
    }
  }
}

bool InstructionSimplifierVisitor::CanEnsureNotNullAt(HInstruction* input, HInstruction* at) const {
  if (!input->CanBeNull()) {
    return true;
  }

  for (const HUseListNode<HInstruction*>& use : input->GetUses()) {
    HInstruction* user = use.GetUser();
    if (user->IsNullCheck() && user->StrictlyDominates(at)) {
      return true;
    }
  }

  return false;
}

// Returns whether doing a type test between the class of `object` against `klass` has
// a statically known outcome. The result of the test is stored in `outcome`.
static bool TypeCheckHasKnownOutcome(HLoadClass* klass, HInstruction* object, bool* outcome) {
  DCHECK(!object->IsNullConstant()) << "Null constants should be special cased";
  ReferenceTypeInfo obj_rti = object->GetReferenceTypeInfo();
  ScopedObjectAccess soa(Thread::Current());
  if (!obj_rti.IsValid()) {
    // We run the simplifier before the reference type propagation so type info might not be
    // available.
    return false;
  }

  ReferenceTypeInfo class_rti = klass->GetLoadedClassRTI();
  if (!class_rti.IsValid()) {
    // Happens when the loaded class is unresolved.
    return false;
  }
  DCHECK(class_rti.IsExact());
  if (class_rti.IsSupertypeOf(obj_rti)) {
    *outcome = true;
    return true;
  } else if (obj_rti.IsExact()) {
    // The test failed at compile time so will also fail at runtime.
    *outcome = false;
    return true;
  } else if (!class_rti.IsInterface()
             && !obj_rti.IsInterface()
             && !obj_rti.IsSupertypeOf(class_rti)) {
    // Different type hierarchy. The test will fail.
    *outcome = false;
    return true;
  }
  return false;
}

void InstructionSimplifierVisitor::VisitCheckCast(HCheckCast* check_cast) {
  HInstruction* object = check_cast->InputAt(0);
  HLoadClass* load_class = check_cast->InputAt(1)->AsLoadClass();
  if (load_class->NeedsAccessCheck()) {
    // If we need to perform an access check we cannot remove the instruction.
    return;
  }

  if (CanEnsureNotNullAt(object, check_cast)) {
    check_cast->ClearMustDoNullCheck();
  }

  if (object->IsNullConstant()) {
    check_cast->GetBlock()->RemoveInstruction(check_cast);
    MaybeRecordStat(stats_, MethodCompilationStat::kRemovedCheckedCast);
    return;
  }

  // Note: The `outcome` is initialized to please valgrind - the compiler can reorder
  // the return value check with the `outcome` check, b/27651442 .
  bool outcome = false;
  if (TypeCheckHasKnownOutcome(load_class, object, &outcome)) {
    if (outcome) {
      check_cast->GetBlock()->RemoveInstruction(check_cast);
      MaybeRecordStat(stats_, MethodCompilationStat::kRemovedCheckedCast);
      if (!load_class->HasUses()) {
        // We cannot rely on DCE to remove the class because the `HLoadClass` thinks it can throw.
        // However, here we know that it cannot because the checkcast was successfull, hence
        // the class was already loaded.
        load_class->GetBlock()->RemoveInstruction(load_class);
      }
    } else {
      // Don't do anything for exceptional cases for now. Ideally we should remove
      // all instructions and blocks this instruction dominates.
    }
  }
}

void InstructionSimplifierVisitor::VisitInstanceOf(HInstanceOf* instruction) {
  HInstruction* object = instruction->InputAt(0);
  HLoadClass* load_class = instruction->InputAt(1)->AsLoadClass();
  if (load_class->NeedsAccessCheck()) {
    // If we need to perform an access check we cannot remove the instruction.
    return;
  }

  bool can_be_null = true;
  if (CanEnsureNotNullAt(object, instruction)) {
    can_be_null = false;
    instruction->ClearMustDoNullCheck();
  }

  HGraph* graph = GetGraph();
  if (object->IsNullConstant()) {
    MaybeRecordStat(stats_, MethodCompilationStat::kRemovedInstanceOf);
    instruction->ReplaceWith(graph->GetIntConstant(0));
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  // Note: The `outcome` is initialized to please valgrind - the compiler can reorder
  // the return value check with the `outcome` check, b/27651442 .
  bool outcome = false;
  if (TypeCheckHasKnownOutcome(load_class, object, &outcome)) {
    MaybeRecordStat(stats_, MethodCompilationStat::kRemovedInstanceOf);
    if (outcome && can_be_null) {
      // Type test will succeed, we just need a null test.
      HNotEqual* test = new (graph->GetAllocator()) HNotEqual(graph->GetNullConstant(), object);
      instruction->GetBlock()->InsertInstructionBefore(test, instruction);
      instruction->ReplaceWith(test);
    } else {
      // We've statically determined the result of the instanceof.
      instruction->ReplaceWith(graph->GetIntConstant(outcome));
    }
    RecordSimplification();
    instruction->GetBlock()->RemoveInstruction(instruction);
    if (outcome && !load_class->HasUses()) {
      // We cannot rely on DCE to remove the class because the `HLoadClass` thinks it can throw.
      // However, here we know that it cannot because the instanceof check was successfull, hence
      // the class was already loaded.
      load_class->GetBlock()->RemoveInstruction(load_class);
    }
  }
}

void InstructionSimplifierVisitor::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  if ((instruction->GetValue()->GetType() == DataType::Type::kReference)
      && CanEnsureNotNullAt(instruction->GetValue(), instruction)) {
    instruction->ClearValueCanBeNull();
  }
}

void InstructionSimplifierVisitor::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  if ((instruction->GetValue()->GetType() == DataType::Type::kReference)
      && CanEnsureNotNullAt(instruction->GetValue(), instruction)) {
    instruction->ClearValueCanBeNull();
  }
}

static HCondition* GetOppositeConditionSwapOps(ArenaAllocator* allocator, HInstruction* cond) {
  HInstruction *lhs = cond->InputAt(0);
  HInstruction *rhs = cond->InputAt(1);
  switch (cond->GetKind()) {
    case HInstruction::kEqual:
      return new (allocator) HEqual(rhs, lhs);
    case HInstruction::kNotEqual:
      return new (allocator) HNotEqual(rhs, lhs);
    case HInstruction::kLessThan:
      return new (allocator) HGreaterThan(rhs, lhs);
    case HInstruction::kLessThanOrEqual:
      return new (allocator) HGreaterThanOrEqual(rhs, lhs);
    case HInstruction::kGreaterThan:
      return new (allocator) HLessThan(rhs, lhs);
    case HInstruction::kGreaterThanOrEqual:
      return new (allocator) HLessThanOrEqual(rhs, lhs);
    case HInstruction::kBelow:
      return new (allocator) HAbove(rhs, lhs);
    case HInstruction::kBelowOrEqual:
      return new (allocator) HAboveOrEqual(rhs, lhs);
    case HInstruction::kAbove:
      return new (allocator) HBelow(rhs, lhs);
    case HInstruction::kAboveOrEqual:
      return new (allocator) HBelowOrEqual(rhs, lhs);
    default:
      LOG(FATAL) << "Unknown ConditionType " << cond->GetKind();
  }
  return nullptr;
}

static bool CmpHasBoolType(HInstruction* input, HInstruction* cmp) {
  if (input->GetType() == DataType::Type::kBool) {
    return true;  // input has direct boolean type
  } else if (cmp->GetUses().HasExactlyOneElement()) {
    // Comparison also has boolean type if both its input and the instruction
    // itself feed into the same phi node.
    HInstruction* user = cmp->GetUses().front().GetUser();
    return user->IsPhi() && user->HasInput(input) && user->HasInput(cmp);
  }
  return false;
}

void InstructionSimplifierVisitor::VisitEqual(HEqual* equal) {
  HInstruction* input_const = equal->GetConstantRight();
  if (input_const != nullptr) {
    HInstruction* input_value = equal->GetLeastConstantLeft();
    if (CmpHasBoolType(input_value, equal) && input_const->IsIntConstant()) {
      HBasicBlock* block = equal->GetBlock();
      // We are comparing the boolean to a constant which is of type int and can
      // be any constant.
      if (input_const->AsIntConstant()->IsTrue()) {
        // Replace (bool_value == true) with bool_value
        equal->ReplaceWith(input_value);
        block->RemoveInstruction(equal);
        RecordSimplification();
      } else if (input_const->AsIntConstant()->IsFalse()) {
        // Replace (bool_value == false) with !bool_value
        equal->ReplaceWith(GetGraph()->InsertOppositeCondition(input_value, equal));
        block->RemoveInstruction(equal);
        RecordSimplification();
      } else {
        // Replace (bool_value == integer_not_zero_nor_one_constant) with false
        equal->ReplaceWith(GetGraph()->GetIntConstant(0));
        block->RemoveInstruction(equal);
        RecordSimplification();
      }
    } else {
      VisitCondition(equal);
    }
  } else {
    VisitCondition(equal);
  }
}

void InstructionSimplifierVisitor::VisitNotEqual(HNotEqual* not_equal) {
  HInstruction* input_const = not_equal->GetConstantRight();
  if (input_const != nullptr) {
    HInstruction* input_value = not_equal->GetLeastConstantLeft();
    if (CmpHasBoolType(input_value, not_equal) && input_const->IsIntConstant()) {
      HBasicBlock* block = not_equal->GetBlock();
      // We are comparing the boolean to a constant which is of type int and can
      // be any constant.
      if (input_const->AsIntConstant()->IsTrue()) {
        // Replace (bool_value != true) with !bool_value
        not_equal->ReplaceWith(GetGraph()->InsertOppositeCondition(input_value, not_equal));
        block->RemoveInstruction(not_equal);
        RecordSimplification();
      } else if (input_const->AsIntConstant()->IsFalse()) {
        // Replace (bool_value != false) with bool_value
        not_equal->ReplaceWith(input_value);
        block->RemoveInstruction(not_equal);
        RecordSimplification();
      } else {
        // Replace (bool_value != integer_not_zero_nor_one_constant) with true
        not_equal->ReplaceWith(GetGraph()->GetIntConstant(1));
        block->RemoveInstruction(not_equal);
        RecordSimplification();
      }
    } else {
      VisitCondition(not_equal);
    }
  } else {
    VisitCondition(not_equal);
  }
}

void InstructionSimplifierVisitor::VisitBooleanNot(HBooleanNot* bool_not) {
  HInstruction* input = bool_not->InputAt(0);
  HInstruction* replace_with = nullptr;

  if (input->IsIntConstant()) {
    // Replace !(true/false) with false/true.
    if (input->AsIntConstant()->IsTrue()) {
      replace_with = GetGraph()->GetIntConstant(0);
    } else {
      DCHECK(input->AsIntConstant()->IsFalse()) << input->AsIntConstant()->GetValue();
      replace_with = GetGraph()->GetIntConstant(1);
    }
  } else if (input->IsBooleanNot()) {
    // Replace (!(!bool_value)) with bool_value.
    replace_with = input->InputAt(0);
  } else if (input->IsCondition() &&
             // Don't change FP compares. The definition of compares involving
             // NaNs forces the compares to be done as written by the user.
             !DataType::IsFloatingPointType(input->InputAt(0)->GetType())) {
    // Replace condition with its opposite.
    replace_with = GetGraph()->InsertOppositeCondition(input->AsCondition(), bool_not);
  }

  if (replace_with != nullptr) {
    bool_not->ReplaceWith(replace_with);
    bool_not->GetBlock()->RemoveInstruction(bool_not);
    RecordSimplification();
  }
}

// Constructs a new ABS(x) node in the HIR.
static HInstruction* NewIntegralAbs(ArenaAllocator* allocator,
                                    HInstruction* x,
                                    HInstruction* cursor) {
  DataType::Type type = x->GetType();
  DCHECK(type == DataType::Type::kInt32 || type ==  DataType::Type::kInt64);
  // Construct a fake intrinsic with as much context as is needed to allocate one.
  // The intrinsic will always be lowered into code later anyway.
  // TODO: b/65164101 : moving towards a real HAbs node makes more sense.
  HInvokeStaticOrDirect::DispatchInfo dispatch_info = {
    HInvokeStaticOrDirect::MethodLoadKind::kDirectAddress,
    HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod,
    0u
  };
  HInvokeStaticOrDirect* invoke = new (allocator) HInvokeStaticOrDirect(
      allocator,
      1,
      type,
      x->GetDexPc(),
      /*method_idx*/ -1,
      /*resolved_method*/ nullptr,
      dispatch_info,
      kStatic,
      MethodReference(nullptr, dex::kDexNoIndex),
      HInvokeStaticOrDirect::ClinitCheckRequirement::kNone);
  invoke->SetArgumentAt(0, x);
  invoke->SetIntrinsic(type == DataType::Type::kInt32 ? Intrinsics::kMathAbsInt
                                                      : Intrinsics::kMathAbsLong,
                       kNoEnvironmentOrCache,
                       kNoSideEffects,
                       kNoThrow);
  cursor->GetBlock()->InsertInstructionBefore(invoke, cursor);
  return invoke;
}

// Returns true if operands a and b consists of widening type conversions
// (either explicit or implicit) to the given to_type.
static bool AreLowerPrecisionArgs(DataType::Type to_type, HInstruction* a, HInstruction* b) {
  if (a->IsTypeConversion() && a->GetType() == to_type) {
    a = a->InputAt(0);
  }
  if (b->IsTypeConversion() && b->GetType() == to_type) {
    b = b->InputAt(0);
  }
  DataType::Type type1 = a->GetType();
  DataType::Type type2 = b->GetType();
  return (type1 == DataType::Type::kUint8  && type2 == DataType::Type::kUint8) ||
         (type1 == DataType::Type::kInt8   && type2 == DataType::Type::kInt8) ||
         (type1 == DataType::Type::kInt16  && type2 == DataType::Type::kInt16) ||
         (type1 == DataType::Type::kUint16 && type2 == DataType::Type::kUint16) ||
         (type1 == DataType::Type::kInt32  && type2 == DataType::Type::kInt32 &&
          to_type == DataType::Type::kInt64);
}

void InstructionSimplifierVisitor::VisitSelect(HSelect* select) {
  HInstruction* replace_with = nullptr;
  HInstruction* condition = select->GetCondition();
  HInstruction* true_value = select->GetTrueValue();
  HInstruction* false_value = select->GetFalseValue();

  if (condition->IsBooleanNot()) {
    // Change ((!cond) ? x : y) to (cond ? y : x).
    condition = condition->InputAt(0);
    std::swap(true_value, false_value);
    select->ReplaceInput(false_value, 0);
    select->ReplaceInput(true_value, 1);
    select->ReplaceInput(condition, 2);
    RecordSimplification();
  }

  if (true_value == false_value) {
    // Replace (cond ? x : x) with (x).
    replace_with = true_value;
  } else if (condition->IsIntConstant()) {
    if (condition->AsIntConstant()->IsTrue()) {
      // Replace (true ? x : y) with (x).
      replace_with = true_value;
    } else {
      // Replace (false ? x : y) with (y).
      DCHECK(condition->AsIntConstant()->IsFalse()) << condition->AsIntConstant()->GetValue();
      replace_with = false_value;
    }
  } else if (true_value->IsIntConstant() && false_value->IsIntConstant()) {
    if (true_value->AsIntConstant()->IsTrue() && false_value->AsIntConstant()->IsFalse()) {
      // Replace (cond ? true : false) with (cond).
      replace_with = condition;
    } else if (true_value->AsIntConstant()->IsFalse() && false_value->AsIntConstant()->IsTrue()) {
      // Replace (cond ? false : true) with (!cond).
      replace_with = GetGraph()->InsertOppositeCondition(condition, select);
    }
  } else if (condition->IsCondition()) {
    IfCondition cmp = condition->AsCondition()->GetCondition();
    HInstruction* a = condition->InputAt(0);
    HInstruction* b = condition->InputAt(1);
    DataType::Type t_type = true_value->GetType();
    DataType::Type f_type = false_value->GetType();
    // Here we have a <cmp> b ? true_value : false_value.
    // Test if both values are same-typed int or long.
    if (t_type == f_type &&
        (t_type == DataType::Type::kInt32 || t_type == DataType::Type::kInt64)) {
      // Try to replace typical integral ABS constructs.
      if (true_value->IsNeg()) {
        HInstruction* negated = true_value->InputAt(0);
        if ((cmp == kCondLT || cmp == kCondLE) &&
            (a == negated && a == false_value && IsInt64Value(b, 0))) {
          // Found a < 0 ? -a : a which can be replaced by ABS(a).
          replace_with = NewIntegralAbs(GetGraph()->GetAllocator(), false_value, select);
        }
      } else if (false_value->IsNeg()) {
        HInstruction* negated = false_value->InputAt(0);
        if ((cmp == kCondGT || cmp == kCondGE) &&
            (a == true_value && a == negated && IsInt64Value(b, 0))) {
          // Found a > 0 ? a : -a which can be replaced by ABS(a).
          replace_with = NewIntegralAbs(GetGraph()->GetAllocator(), true_value, select);
        }
      } else if (true_value->IsSub() && false_value->IsSub()) {
        HInstruction* true_sub1 = true_value->InputAt(0);
        HInstruction* true_sub2 = true_value->InputAt(1);
        HInstruction* false_sub1 = false_value->InputAt(0);
        HInstruction* false_sub2 = false_value->InputAt(1);
        if ((((cmp == kCondGT || cmp == kCondGE) &&
              (a == true_sub1 && b == true_sub2 && a == false_sub2 && b == false_sub1)) ||
             ((cmp == kCondLT || cmp == kCondLE) &&
              (a == true_sub2 && b == true_sub1 && a == false_sub1 && b == false_sub2))) &&
            AreLowerPrecisionArgs(t_type, a, b)) {
          // Found a > b ? a - b  : b - a   or
          //       a < b ? b - a  : a - b
          // which can be replaced by ABS(a - b) for lower precision operands a, b.
          replace_with = NewIntegralAbs(GetGraph()->GetAllocator(), true_value, select);
        }
      }
    }
  }

  if (replace_with != nullptr) {
    select->ReplaceWith(replace_with);
    select->GetBlock()->RemoveInstruction(select);
    RecordSimplification();
  }
}

void InstructionSimplifierVisitor::VisitIf(HIf* instruction) {
  HInstruction* condition = instruction->InputAt(0);
  if (condition->IsBooleanNot()) {
    // Swap successors if input is negated.
    instruction->ReplaceInput(condition->InputAt(0), 0);
    instruction->GetBlock()->SwapSuccessors();
    RecordSimplification();
  }
}

void InstructionSimplifierVisitor::VisitArrayLength(HArrayLength* instruction) {
  HInstruction* input = instruction->InputAt(0);
  // If the array is a NewArray with constant size, replace the array length
  // with the constant instruction. This helps the bounds check elimination phase.
  if (input->IsNewArray()) {
    input = input->AsNewArray()->GetLength();
    if (input->IsIntConstant()) {
      instruction->ReplaceWith(input);
    }
  }
}

void InstructionSimplifierVisitor::VisitArraySet(HArraySet* instruction) {
  HInstruction* value = instruction->GetValue();
  if (value->GetType() != DataType::Type::kReference) {
    return;
  }

  if (CanEnsureNotNullAt(value, instruction)) {
    instruction->ClearValueCanBeNull();
  }

  if (value->IsArrayGet()) {
    if (value->AsArrayGet()->GetArray() == instruction->GetArray()) {
      // If the code is just swapping elements in the array, no need for a type check.
      instruction->ClearNeedsTypeCheck();
      return;
    }
  }

  if (value->IsNullConstant()) {
    instruction->ClearNeedsTypeCheck();
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  ReferenceTypeInfo array_rti = instruction->GetArray()->GetReferenceTypeInfo();
  ReferenceTypeInfo value_rti = value->GetReferenceTypeInfo();
  if (!array_rti.IsValid()) {
    return;
  }

  if (value_rti.IsValid() && array_rti.CanArrayHold(value_rti)) {
    instruction->ClearNeedsTypeCheck();
    return;
  }

  if (array_rti.IsObjectArray()) {
    if (array_rti.IsExact()) {
      instruction->ClearNeedsTypeCheck();
      return;
    }
    instruction->SetStaticTypeOfArrayIsObjectArray();
  }
}

static bool IsTypeConversionLossless(DataType::Type input_type, DataType::Type result_type) {
  // Make sure all implicit conversions have been simplified and no new ones have been introduced.
  DCHECK(!DataType::IsTypeConversionImplicit(input_type, result_type))
      << input_type << "," << result_type;
  // The conversion to a larger type is loss-less with the exception of two cases,
  //   - conversion to the unsigned type Uint16, where we may lose some bits, and
  //   - conversion from float to long, the only FP to integral conversion with smaller FP type.
  // For integral to FP conversions this holds because the FP mantissa is large enough.
  // Note: The size check excludes Uint8 as the result type.
  return DataType::Size(result_type) > DataType::Size(input_type) &&
      result_type != DataType::Type::kUint16 &&
      !(result_type == DataType::Type::kInt64 && input_type == DataType::Type::kFloat32);
}

static inline bool TryReplaceFieldOrArrayGetType(HInstruction* maybe_get, DataType::Type new_type) {
  if (maybe_get->IsInstanceFieldGet()) {
    maybe_get->AsInstanceFieldGet()->SetType(new_type);
    return true;
  } else if (maybe_get->IsStaticFieldGet()) {
    maybe_get->AsStaticFieldGet()->SetType(new_type);
    return true;
  } else if (maybe_get->IsArrayGet() && !maybe_get->AsArrayGet()->IsStringCharAt()) {
    maybe_get->AsArrayGet()->SetType(new_type);
    return true;
  } else {
    return false;
  }
}

// The type conversion is only used for storing into a field/element of the
// same/narrower size.
static bool IsTypeConversionForStoringIntoNoWiderFieldOnly(HTypeConversion* type_conversion) {
  if (type_conversion->HasEnvironmentUses()) {
    return false;
  }
  DataType::Type input_type = type_conversion->GetInputType();
  DataType::Type result_type = type_conversion->GetResultType();
  if (!DataType::IsIntegralType(input_type) ||
      !DataType::IsIntegralType(result_type) ||
      input_type == DataType::Type::kInt64 ||
      result_type == DataType::Type::kInt64) {
    // Type conversion is needed if non-integer types are involved, or 64-bit
    // types are involved, which may use different number of registers.
    return false;
  }
  if (DataType::Size(input_type) >= DataType::Size(result_type)) {
    // Type conversion is not necessary when storing to a field/element of the
    // same/smaller size.
  } else {
    // We do not handle this case here.
    return false;
  }

  // Check if the converted value is only used for storing into heap.
  for (const HUseListNode<HInstruction*>& use : type_conversion->GetUses()) {
    HInstruction* instruction = use.GetUser();
    if (instruction->IsInstanceFieldSet() &&
        instruction->AsInstanceFieldSet()->GetFieldType() == result_type) {
      DCHECK_EQ(instruction->AsInstanceFieldSet()->GetValue(), type_conversion);
      continue;
    }
    if (instruction->IsStaticFieldSet() &&
        instruction->AsStaticFieldSet()->GetFieldType() == result_type) {
      DCHECK_EQ(instruction->AsStaticFieldSet()->GetValue(), type_conversion);
      continue;
    }
    if (instruction->IsArraySet() &&
        instruction->AsArraySet()->GetComponentType() == result_type &&
        // not index use.
        instruction->AsArraySet()->GetIndex() != type_conversion) {
      DCHECK_EQ(instruction->AsArraySet()->GetValue(), type_conversion);
      continue;
    }
    // The use is not as a store value, or the field/element type is not the
    // same as the result_type, keep the type conversion.
    return false;
  }
  // Codegen automatically handles the type conversion during the store.
  return true;
}

void InstructionSimplifierVisitor::VisitTypeConversion(HTypeConversion* instruction) {
  HInstruction* input = instruction->GetInput();
  DataType::Type input_type = input->GetType();
  DataType::Type result_type = instruction->GetResultType();
  if (DataType::IsTypeConversionImplicit(input_type, result_type)) {
    // Remove the implicit conversion; this includes conversion to the same type.
    instruction->ReplaceWith(input);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if (input->IsTypeConversion()) {
    HTypeConversion* input_conversion = input->AsTypeConversion();
    HInstruction* original_input = input_conversion->GetInput();
    DataType::Type original_type = original_input->GetType();

    // When the first conversion is lossless, a direct conversion from the original type
    // to the final type yields the same result, even for a lossy second conversion, for
    // example float->double->int or int->double->float.
    bool is_first_conversion_lossless = IsTypeConversionLossless(original_type, input_type);

    // For integral conversions, see if the first conversion loses only bits that the second
    // doesn't need, i.e. the final type is no wider than the intermediate. If so, direct
    // conversion yields the same result, for example long->int->short or int->char->short.
    bool integral_conversions_with_non_widening_second =
        DataType::IsIntegralType(input_type) &&
        DataType::IsIntegralType(original_type) &&
        DataType::IsIntegralType(result_type) &&
        DataType::Size(result_type) <= DataType::Size(input_type);

    if (is_first_conversion_lossless || integral_conversions_with_non_widening_second) {
      // If the merged conversion is implicit, do the simplification unconditionally.
      if (DataType::IsTypeConversionImplicit(original_type, result_type)) {
        instruction->ReplaceWith(original_input);
        instruction->GetBlock()->RemoveInstruction(instruction);
        if (!input_conversion->HasUses()) {
          // Don't wait for DCE.
          input_conversion->GetBlock()->RemoveInstruction(input_conversion);
        }
        RecordSimplification();
        return;
      }
      // Otherwise simplify only if the first conversion has no other use.
      if (input_conversion->HasOnlyOneNonEnvironmentUse()) {
        input_conversion->ReplaceWith(original_input);
        input_conversion->GetBlock()->RemoveInstruction(input_conversion);
        RecordSimplification();
        return;
      }
    }
  } else if (input->IsAnd() && DataType::IsIntegralType(result_type)) {
    DCHECK(DataType::IsIntegralType(input_type));
    HAnd* input_and = input->AsAnd();
    HConstant* constant = input_and->GetConstantRight();
    if (constant != nullptr) {
      int64_t value = Int64FromConstant(constant);
      DCHECK_NE(value, -1);  // "& -1" would have been optimized away in VisitAnd().
      size_t trailing_ones = CTZ(~static_cast<uint64_t>(value));
      if (trailing_ones >= kBitsPerByte * DataType::Size(result_type)) {
        // The `HAnd` is useless, for example in `(byte) (x & 0xff)`, get rid of it.
        HInstruction* original_input = input_and->GetLeastConstantLeft();
        if (DataType::IsTypeConversionImplicit(original_input->GetType(), result_type)) {
          instruction->ReplaceWith(original_input);
          instruction->GetBlock()->RemoveInstruction(instruction);
          RecordSimplification();
          return;
        } else if (input->HasOnlyOneNonEnvironmentUse()) {
          input_and->ReplaceWith(original_input);
          input_and->GetBlock()->RemoveInstruction(input_and);
          RecordSimplification();
          return;
        }
      }
    }
  } else if (input->HasOnlyOneNonEnvironmentUse() &&
             ((input_type == DataType::Type::kInt8 && result_type == DataType::Type::kUint8) ||
              (input_type == DataType::Type::kUint8 && result_type == DataType::Type::kInt8) ||
              (input_type == DataType::Type::kInt16 && result_type == DataType::Type::kUint16) ||
              (input_type == DataType::Type::kUint16 && result_type == DataType::Type::kInt16))) {
    // Try to modify the type of the load to `result_type` and remove the explicit type conversion.
    if (TryReplaceFieldOrArrayGetType(input, result_type)) {
      instruction->ReplaceWith(input);
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    }
  }

  if (IsTypeConversionForStoringIntoNoWiderFieldOnly(instruction)) {
    instruction->ReplaceWith(input);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }
}

void InstructionSimplifierVisitor::VisitAdd(HAdd* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  bool integral_type = DataType::IsIntegralType(instruction->GetType());
  if ((input_cst != nullptr) && input_cst->IsArithmeticZero()) {
    // Replace code looking like
    //    ADD dst, src, 0
    // with
    //    src
    // Note that we cannot optimize `x + 0.0` to `x` for floating-point. When
    // `x` is `-0.0`, the former expression yields `0.0`, while the later
    // yields `-0.0`.
    if (integral_type) {
      instruction->ReplaceWith(input_other);
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    }
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  bool left_is_neg = left->IsNeg();
  bool right_is_neg = right->IsNeg();

  if (left_is_neg && right_is_neg) {
    if (TryMoveNegOnInputsAfterBinop(instruction)) {
      return;
    }
  }

  HNeg* neg = left_is_neg ? left->AsNeg() : right->AsNeg();
  if ((left_is_neg ^ right_is_neg) && neg->HasOnlyOneNonEnvironmentUse()) {
    // Replace code looking like
    //    NEG tmp, b
    //    ADD dst, a, tmp
    // with
    //    SUB dst, a, b
    // We do not perform the optimization if the input negation has environment
    // uses or multiple non-environment uses as it could lead to worse code. In
    // particular, we do not want the live range of `b` to be extended if we are
    // not sure the initial 'NEG' instruction can be removed.
    HInstruction* other = left_is_neg ? right : left;
    HSub* sub =
        new(GetGraph()->GetAllocator()) HSub(instruction->GetType(), other, neg->GetInput());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, sub);
    RecordSimplification();
    neg->GetBlock()->RemoveInstruction(neg);
    return;
  }

  if (TryReplaceWithRotate(instruction)) {
    return;
  }

  // TryHandleAssociativeAndCommutativeOperation() does not remove its input,
  // so no need to return.
  TryHandleAssociativeAndCommutativeOperation(instruction);

  if ((left->IsSub() || right->IsSub()) &&
      TrySubtractionChainSimplification(instruction)) {
    return;
  }

  if (integral_type) {
    // Replace code patterns looking like
    //    SUB dst1, x, y        SUB dst1, x, y
    //    ADD dst2, dst1, y     ADD dst2, y, dst1
    // with
    //    SUB dst1, x, y
    // ADD instruction is not needed in this case, we may use
    // one of inputs of SUB instead.
    if (left->IsSub() && left->InputAt(1) == right) {
      instruction->ReplaceWith(left->InputAt(0));
      RecordSimplification();
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    } else if (right->IsSub() && right->InputAt(1) == left) {
      instruction->ReplaceWith(right->InputAt(0));
      RecordSimplification();
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    }
  }
}

void InstructionSimplifierVisitor::VisitAnd(HAnd* instruction) {
  DCHECK(DataType::IsIntegralType(instruction->GetType()));
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if (input_cst != nullptr) {
    int64_t value = Int64FromConstant(input_cst);
    if (value == -1 ||
        // Similar cases under zero extension.
        (DataType::IsUnsignedType(input_other->GetType()) &&
         ((DataType::MaxValueOfIntegralType(input_other->GetType()) & ~value) == 0))) {
      // Replace code looking like
      //    AND dst, src, 0xFFF...FF
      // with
      //    src
      instruction->ReplaceWith(input_other);
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    }
    if (input_other->IsTypeConversion() &&
        input_other->GetType() == DataType::Type::kInt64 &&
        DataType::IsIntegralType(input_other->InputAt(0)->GetType()) &&
        IsInt<32>(value) &&
        input_other->HasOnlyOneNonEnvironmentUse()) {
      // The AND can be reordered before the TypeConversion. Replace
      //   LongConstant cst, <32-bit-constant-sign-extended-to-64-bits>
      //   TypeConversion<Int64> tmp, src
      //   AND dst, tmp, cst
      // with
      //   IntConstant cst, <32-bit-constant>
      //   AND tmp, src, cst
      //   TypeConversion<Int64> dst, tmp
      // This helps 32-bit targets and does not hurt 64-bit targets.
      // This also simplifies detection of other patterns, such as Uint8 loads.
      HInstruction* new_and_input = input_other->InputAt(0);
      // Implicit conversion Int64->Int64 would have been removed previously.
      DCHECK_NE(new_and_input->GetType(), DataType::Type::kInt64);
      HConstant* new_const = GetGraph()->GetConstant(DataType::Type::kInt32, value);
      HAnd* new_and =
          new (GetGraph()->GetAllocator()) HAnd(DataType::Type::kInt32, new_and_input, new_const);
      instruction->GetBlock()->InsertInstructionBefore(new_and, instruction);
      HTypeConversion* new_conversion =
          new (GetGraph()->GetAllocator()) HTypeConversion(DataType::Type::kInt64, new_and);
      instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, new_conversion);
      input_other->GetBlock()->RemoveInstruction(input_other);
      RecordSimplification();
      // Try to process the new And now, do not wait for the next round of simplifications.
      instruction = new_and;
      input_other = new_and_input;
    }
    // Eliminate And from UShr+And if the And-mask contains all the bits that
    // can be non-zero after UShr. Transform Shr+And to UShr if the And-mask
    // precisely clears the shifted-in sign bits.
    if ((input_other->IsUShr() || input_other->IsShr()) && input_other->InputAt(1)->IsConstant()) {
      size_t reg_bits = (instruction->GetResultType() == DataType::Type::kInt64) ? 64 : 32;
      size_t shift = Int64FromConstant(input_other->InputAt(1)->AsConstant()) & (reg_bits - 1);
      size_t num_tail_bits_set = CTZ(value + 1);
      if ((num_tail_bits_set >= reg_bits - shift) && input_other->IsUShr()) {
        // This AND clears only bits known to be clear, for example "(x >>> 24) & 0xff".
        instruction->ReplaceWith(input_other);
        instruction->GetBlock()->RemoveInstruction(instruction);
        RecordSimplification();
        return;
      }  else if ((num_tail_bits_set == reg_bits - shift) && IsPowerOfTwo(value + 1) &&
          input_other->HasOnlyOneNonEnvironmentUse()) {
        DCHECK(input_other->IsShr());  // For UShr, we would have taken the branch above.
        // Replace SHR+AND with USHR, for example "(x >> 24) & 0xff" -> "x >>> 24".
        HUShr* ushr = new (GetGraph()->GetAllocator()) HUShr(instruction->GetType(),
                                                             input_other->InputAt(0),
                                                             input_other->InputAt(1),
                                                             input_other->GetDexPc());
        instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, ushr);
        input_other->GetBlock()->RemoveInstruction(input_other);
        RecordSimplification();
        return;
      }
    }
    if ((value == 0xff || value == 0xffff) && instruction->GetType() != DataType::Type::kInt64) {
      // Transform AND to a type conversion to Uint8/Uint16. If `input_other` is a field
      // or array Get with only a single use, short-circuit the subsequent simplification
      // of the Get+TypeConversion and change the Get's type to `new_type` instead.
      DataType::Type new_type = (value == 0xff) ? DataType::Type::kUint8 : DataType::Type::kUint16;
      DataType::Type find_type = (value == 0xff) ? DataType::Type::kInt8 : DataType::Type::kInt16;
      if (input_other->GetType() == find_type &&
          input_other->HasOnlyOneNonEnvironmentUse() &&
          TryReplaceFieldOrArrayGetType(input_other, new_type)) {
        instruction->ReplaceWith(input_other);
        instruction->GetBlock()->RemoveInstruction(instruction);
      } else if (DataType::IsTypeConversionImplicit(input_other->GetType(), new_type)) {
        instruction->ReplaceWith(input_other);
        instruction->GetBlock()->RemoveInstruction(instruction);
      } else {
        HTypeConversion* type_conversion = new (GetGraph()->GetAllocator()) HTypeConversion(
            new_type, input_other, instruction->GetDexPc());
        instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, type_conversion);
      }
      RecordSimplification();
      return;
    }
  }

  // We assume that GVN has run before, so we only perform a pointer comparison.
  // If for some reason the values are equal but the pointers are different, we
  // are still correct and only miss an optimization opportunity.
  if (instruction->GetLeft() == instruction->GetRight()) {
    // Replace code looking like
    //    AND dst, src, src
    // with
    //    src
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if (TryDeMorganNegationFactoring(instruction)) {
    return;
  }

  // TryHandleAssociativeAndCommutativeOperation() does not remove its input,
  // so no need to return.
  TryHandleAssociativeAndCommutativeOperation(instruction);
}

void InstructionSimplifierVisitor::VisitGreaterThan(HGreaterThan* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitGreaterThanOrEqual(HGreaterThanOrEqual* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitLessThan(HLessThan* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitLessThanOrEqual(HLessThanOrEqual* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitBelow(HBelow* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitBelowOrEqual(HBelowOrEqual* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitAbove(HAbove* condition) {
  VisitCondition(condition);
}

void InstructionSimplifierVisitor::VisitAboveOrEqual(HAboveOrEqual* condition) {
  VisitCondition(condition);
}

// Recognize the following pattern:
// obj.getClass() ==/!= Foo.class
// And replace it with a constant value if the type of `obj` is statically known.
static bool RecognizeAndSimplifyClassCheck(HCondition* condition) {
  HInstruction* input_one = condition->InputAt(0);
  HInstruction* input_two = condition->InputAt(1);
  HLoadClass* load_class = input_one->IsLoadClass()
      ? input_one->AsLoadClass()
      : input_two->AsLoadClass();
  if (load_class == nullptr) {
    return false;
  }

  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  if (!class_rti.IsValid()) {
    // Unresolved class.
    return false;
  }

  HInstanceFieldGet* field_get = (load_class == input_one)
      ? input_two->AsInstanceFieldGet()
      : input_one->AsInstanceFieldGet();
  if (field_get == nullptr) {
    return false;
  }

  HInstruction* receiver = field_get->InputAt(0);
  ReferenceTypeInfo receiver_type = receiver->GetReferenceTypeInfo();
  if (!receiver_type.IsExact()) {
    return false;
  }

  {
    ScopedObjectAccess soa(Thread::Current());
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    ArtField* field = class_linker->GetClassRoot(ClassLinker::kJavaLangObject)->GetInstanceField(0);
    DCHECK_EQ(std::string(field->GetName()), "shadow$_klass_");
    if (field_get->GetFieldInfo().GetField() != field) {
      return false;
    }

    // We can replace the compare.
    int value = 0;
    if (receiver_type.IsEqual(class_rti)) {
      value = condition->IsEqual() ? 1 : 0;
    } else {
      value = condition->IsNotEqual() ? 1 : 0;
    }
    condition->ReplaceWith(condition->GetBlock()->GetGraph()->GetIntConstant(value));
    return true;
  }
}

void InstructionSimplifierVisitor::VisitCondition(HCondition* condition) {
  if (condition->IsEqual() || condition->IsNotEqual()) {
    if (RecognizeAndSimplifyClassCheck(condition)) {
      return;
    }
  }

  // Reverse condition if left is constant. Our code generators prefer constant
  // on the right hand side.
  if (condition->GetLeft()->IsConstant() && !condition->GetRight()->IsConstant()) {
    HBasicBlock* block = condition->GetBlock();
    HCondition* replacement =
        GetOppositeConditionSwapOps(block->GetGraph()->GetAllocator(), condition);
    // If it is a fp we must set the opposite bias.
    if (replacement != nullptr) {
      if (condition->IsLtBias()) {
        replacement->SetBias(ComparisonBias::kGtBias);
      } else if (condition->IsGtBias()) {
        replacement->SetBias(ComparisonBias::kLtBias);
      }
      block->ReplaceAndRemoveInstructionWith(condition, replacement);
      RecordSimplification();

      condition = replacement;
    }
  }

  HInstruction* left = condition->GetLeft();
  HInstruction* right = condition->GetRight();

  // Try to fold an HCompare into this HCondition.

  // We can only replace an HCondition which compares a Compare to 0.
  // Both 'dx' and 'jack' generate a compare to 0 when compiling a
  // condition with a long, float or double comparison as input.
  if (!left->IsCompare() || !right->IsConstant() || right->AsIntConstant()->GetValue() != 0) {
    // Conversion is not possible.
    return;
  }

  // Is the Compare only used for this purpose?
  if (!left->GetUses().HasExactlyOneElement()) {
    // Someone else also wants the result of the compare.
    return;
  }

  if (!left->GetEnvUses().empty()) {
    // There is a reference to the compare result in an environment. Do we really need it?
    if (GetGraph()->IsDebuggable()) {
      return;
    }

    // We have to ensure that there are no deopt points in the sequence.
    if (left->HasAnyEnvironmentUseBefore(condition)) {
      return;
    }
  }

  // Clean up any environment uses from the HCompare, if any.
  left->RemoveEnvironmentUsers();

  // We have decided to fold the HCompare into the HCondition. Transfer the information.
  condition->SetBias(left->AsCompare()->GetBias());

  // Replace the operands of the HCondition.
  condition->ReplaceInput(left->InputAt(0), 0);
  condition->ReplaceInput(left->InputAt(1), 1);

  // Remove the HCompare.
  left->GetBlock()->RemoveInstruction(left);

  RecordSimplification();
}

// Return whether x / divisor == x * (1.0f / divisor), for every float x.
static constexpr bool CanDivideByReciprocalMultiplyFloat(int32_t divisor) {
  // True, if the most significant bits of divisor are 0.
  return ((divisor & 0x7fffff) == 0);
}

// Return whether x / divisor == x * (1.0 / divisor), for every double x.
static constexpr bool CanDivideByReciprocalMultiplyDouble(int64_t divisor) {
  // True, if the most significant bits of divisor are 0.
  return ((divisor & ((UINT64_C(1) << 52) - 1)) == 0);
}

void InstructionSimplifierVisitor::VisitDiv(HDiv* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  DataType::Type type = instruction->GetType();

  if ((input_cst != nullptr) && input_cst->IsOne()) {
    // Replace code looking like
    //    DIV dst, src, 1
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if ((input_cst != nullptr) && input_cst->IsMinusOne()) {
    // Replace code looking like
    //    DIV dst, src, -1
    // with
    //    NEG dst, src
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(
        instruction, new (GetGraph()->GetAllocator()) HNeg(type, input_other));
    RecordSimplification();
    return;
  }

  if ((input_cst != nullptr) && DataType::IsFloatingPointType(type)) {
    // Try replacing code looking like
    //    DIV dst, src, constant
    // with
    //    MUL dst, src, 1 / constant
    HConstant* reciprocal = nullptr;
    if (type == DataType::Type::kFloat64) {
      double value = input_cst->AsDoubleConstant()->GetValue();
      if (CanDivideByReciprocalMultiplyDouble(bit_cast<int64_t, double>(value))) {
        reciprocal = GetGraph()->GetDoubleConstant(1.0 / value);
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kFloat32);
      float value = input_cst->AsFloatConstant()->GetValue();
      if (CanDivideByReciprocalMultiplyFloat(bit_cast<int32_t, float>(value))) {
        reciprocal = GetGraph()->GetFloatConstant(1.0f / value);
      }
    }

    if (reciprocal != nullptr) {
      instruction->GetBlock()->ReplaceAndRemoveInstructionWith(
          instruction, new (GetGraph()->GetAllocator()) HMul(type, input_other, reciprocal));
      RecordSimplification();
      return;
    }
  }
}

void InstructionSimplifierVisitor::VisitMul(HMul* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();
  DataType::Type type = instruction->GetType();
  HBasicBlock* block = instruction->GetBlock();
  ArenaAllocator* allocator = GetGraph()->GetAllocator();

  if (input_cst == nullptr) {
    return;
  }

  if (input_cst->IsOne()) {
    // Replace code looking like
    //    MUL dst, src, 1
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if (input_cst->IsMinusOne() &&
      (DataType::IsFloatingPointType(type) || DataType::IsIntOrLongType(type))) {
    // Replace code looking like
    //    MUL dst, src, -1
    // with
    //    NEG dst, src
    HNeg* neg = new (allocator) HNeg(type, input_other);
    block->ReplaceAndRemoveInstructionWith(instruction, neg);
    RecordSimplification();
    return;
  }

  if (DataType::IsFloatingPointType(type) &&
      ((input_cst->IsFloatConstant() && input_cst->AsFloatConstant()->GetValue() == 2.0f) ||
       (input_cst->IsDoubleConstant() && input_cst->AsDoubleConstant()->GetValue() == 2.0))) {
    // Replace code looking like
    //    FP_MUL dst, src, 2.0
    // with
    //    FP_ADD dst, src, src
    // The 'int' and 'long' cases are handled below.
    block->ReplaceAndRemoveInstructionWith(instruction,
                                           new (allocator) HAdd(type, input_other, input_other));
    RecordSimplification();
    return;
  }

  if (DataType::IsIntOrLongType(type)) {
    int64_t factor = Int64FromConstant(input_cst);
    // Even though constant propagation also takes care of the zero case, other
    // optimizations can lead to having a zero multiplication.
    if (factor == 0) {
      // Replace code looking like
      //    MUL dst, src, 0
      // with
      //    0
      instruction->ReplaceWith(input_cst);
      instruction->GetBlock()->RemoveInstruction(instruction);
      RecordSimplification();
      return;
    } else if (IsPowerOfTwo(factor)) {
      // Replace code looking like
      //    MUL dst, src, pow_of_2
      // with
      //    SHL dst, src, log2(pow_of_2)
      HIntConstant* shift = GetGraph()->GetIntConstant(WhichPowerOf2(factor));
      HShl* shl = new (allocator) HShl(type, input_other, shift);
      block->ReplaceAndRemoveInstructionWith(instruction, shl);
      RecordSimplification();
      return;
    } else if (IsPowerOfTwo(factor - 1)) {
      // Transform code looking like
      //    MUL dst, src, (2^n + 1)
      // into
      //    SHL tmp, src, n
      //    ADD dst, src, tmp
      HShl* shl = new (allocator) HShl(type,
                                       input_other,
                                       GetGraph()->GetIntConstant(WhichPowerOf2(factor - 1)));
      HAdd* add = new (allocator) HAdd(type, input_other, shl);

      block->InsertInstructionBefore(shl, instruction);
      block->ReplaceAndRemoveInstructionWith(instruction, add);
      RecordSimplification();
      return;
    } else if (IsPowerOfTwo(factor + 1)) {
      // Transform code looking like
      //    MUL dst, src, (2^n - 1)
      // into
      //    SHL tmp, src, n
      //    SUB dst, tmp, src
      HShl* shl = new (allocator) HShl(type,
                                       input_other,
                                       GetGraph()->GetIntConstant(WhichPowerOf2(factor + 1)));
      HSub* sub = new (allocator) HSub(type, shl, input_other);

      block->InsertInstructionBefore(shl, instruction);
      block->ReplaceAndRemoveInstructionWith(instruction, sub);
      RecordSimplification();
      return;
    }
  }

  // TryHandleAssociativeAndCommutativeOperation() does not remove its input,
  // so no need to return.
  TryHandleAssociativeAndCommutativeOperation(instruction);
}

void InstructionSimplifierVisitor::VisitNeg(HNeg* instruction) {
  HInstruction* input = instruction->GetInput();
  if (input->IsNeg()) {
    // Replace code looking like
    //    NEG tmp, src
    //    NEG dst, tmp
    // with
    //    src
    HNeg* previous_neg = input->AsNeg();
    instruction->ReplaceWith(previous_neg->GetInput());
    instruction->GetBlock()->RemoveInstruction(instruction);
    // We perform the optimization even if the input negation has environment
    // uses since it allows removing the current instruction. But we only delete
    // the input negation only if it is does not have any uses left.
    if (!previous_neg->HasUses()) {
      previous_neg->GetBlock()->RemoveInstruction(previous_neg);
    }
    RecordSimplification();
    return;
  }

  if (input->IsSub() && input->HasOnlyOneNonEnvironmentUse() &&
      !DataType::IsFloatingPointType(input->GetType())) {
    // Replace code looking like
    //    SUB tmp, a, b
    //    NEG dst, tmp
    // with
    //    SUB dst, b, a
    // We do not perform the optimization if the input subtraction has
    // environment uses or multiple non-environment uses as it could lead to
    // worse code. In particular, we do not want the live ranges of `a` and `b`
    // to be extended if we are not sure the initial 'SUB' instruction can be
    // removed.
    // We do not perform optimization for fp because we could lose the sign of zero.
    HSub* sub = input->AsSub();
    HSub* new_sub = new (GetGraph()->GetAllocator()) HSub(
        instruction->GetType(), sub->GetRight(), sub->GetLeft());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, new_sub);
    if (!sub->HasUses()) {
      sub->GetBlock()->RemoveInstruction(sub);
    }
    RecordSimplification();
  }
}

void InstructionSimplifierVisitor::VisitNot(HNot* instruction) {
  HInstruction* input = instruction->GetInput();
  if (input->IsNot()) {
    // Replace code looking like
    //    NOT tmp, src
    //    NOT dst, tmp
    // with
    //    src
    // We perform the optimization even if the input negation has environment
    // uses since it allows removing the current instruction. But we only delete
    // the input negation only if it is does not have any uses left.
    HNot* previous_not = input->AsNot();
    instruction->ReplaceWith(previous_not->GetInput());
    instruction->GetBlock()->RemoveInstruction(instruction);
    if (!previous_not->HasUses()) {
      previous_not->GetBlock()->RemoveInstruction(previous_not);
    }
    RecordSimplification();
  }
}

void InstructionSimplifierVisitor::VisitOr(HOr* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZeroBitPattern()) {
    // Replace code looking like
    //    OR dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  // We assume that GVN has run before, so we only perform a pointer comparison.
  // If for some reason the values are equal but the pointers are different, we
  // are still correct and only miss an optimization opportunity.
  if (instruction->GetLeft() == instruction->GetRight()) {
    // Replace code looking like
    //    OR dst, src, src
    // with
    //    src
    instruction->ReplaceWith(instruction->GetLeft());
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if (TryDeMorganNegationFactoring(instruction)) return;

  if (TryReplaceWithRotate(instruction)) {
    return;
  }

  // TryHandleAssociativeAndCommutativeOperation() does not remove its input,
  // so no need to return.
  TryHandleAssociativeAndCommutativeOperation(instruction);
}

void InstructionSimplifierVisitor::VisitShl(HShl* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitShr(HShr* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitSub(HSub* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  DataType::Type type = instruction->GetType();
  if (DataType::IsFloatingPointType(type)) {
    return;
  }

  if ((input_cst != nullptr) && input_cst->IsArithmeticZero()) {
    // Replace code looking like
    //    SUB dst, src, 0
    // with
    //    src
    // Note that we cannot optimize `x - 0.0` to `x` for floating-point. When
    // `x` is `-0.0`, the former expression yields `0.0`, while the later
    // yields `-0.0`.
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  HBasicBlock* block = instruction->GetBlock();
  ArenaAllocator* allocator = GetGraph()->GetAllocator();

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  if (left->IsConstant()) {
    if (Int64FromConstant(left->AsConstant()) == 0) {
      // Replace code looking like
      //    SUB dst, 0, src
      // with
      //    NEG dst, src
      // Note that we cannot optimize `0.0 - x` to `-x` for floating-point. When
      // `x` is `0.0`, the former expression yields `0.0`, while the later
      // yields `-0.0`.
      HNeg* neg = new (allocator) HNeg(type, right);
      block->ReplaceAndRemoveInstructionWith(instruction, neg);
      RecordSimplification();
      return;
    }
  }

  if (left->IsNeg() && right->IsNeg()) {
    if (TryMoveNegOnInputsAfterBinop(instruction)) {
      return;
    }
  }

  if (right->IsNeg() && right->HasOnlyOneNonEnvironmentUse()) {
    // Replace code looking like
    //    NEG tmp, b
    //    SUB dst, a, tmp
    // with
    //    ADD dst, a, b
    HAdd* add = new(GetGraph()->GetAllocator()) HAdd(type, left, right->AsNeg()->GetInput());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, add);
    RecordSimplification();
    right->GetBlock()->RemoveInstruction(right);
    return;
  }

  if (left->IsNeg() && left->HasOnlyOneNonEnvironmentUse()) {
    // Replace code looking like
    //    NEG tmp, a
    //    SUB dst, tmp, b
    // with
    //    ADD tmp, a, b
    //    NEG dst, tmp
    // The second version is not intrinsically better, but enables more
    // transformations.
    HAdd* add = new(GetGraph()->GetAllocator()) HAdd(type, left->AsNeg()->GetInput(), right);
    instruction->GetBlock()->InsertInstructionBefore(add, instruction);
    HNeg* neg = new (GetGraph()->GetAllocator()) HNeg(instruction->GetType(), add);
    instruction->GetBlock()->InsertInstructionBefore(neg, instruction);
    instruction->ReplaceWith(neg);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    left->GetBlock()->RemoveInstruction(left);
    return;
  }

  if (TrySubtractionChainSimplification(instruction)) {
    return;
  }

  if (left->IsAdd()) {
    // Replace code patterns looking like
    //    ADD dst1, x, y        ADD dst1, x, y
    //    SUB dst2, dst1, y     SUB dst2, dst1, x
    // with
    //    ADD dst1, x, y
    // SUB instruction is not needed in this case, we may use
    // one of inputs of ADD instead.
    // It is applicable to integral types only.
    DCHECK(DataType::IsIntegralType(type));
    if (left->InputAt(1) == right) {
      instruction->ReplaceWith(left->InputAt(0));
      RecordSimplification();
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    } else if (left->InputAt(0) == right) {
      instruction->ReplaceWith(left->InputAt(1));
      RecordSimplification();
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    }
  }
}

void InstructionSimplifierVisitor::VisitUShr(HUShr* instruction) {
  VisitShift(instruction);
}

void InstructionSimplifierVisitor::VisitXor(HXor* instruction) {
  HConstant* input_cst = instruction->GetConstantRight();
  HInstruction* input_other = instruction->GetLeastConstantLeft();

  if ((input_cst != nullptr) && input_cst->IsZeroBitPattern()) {
    // Replace code looking like
    //    XOR dst, src, 0
    // with
    //    src
    instruction->ReplaceWith(input_other);
    instruction->GetBlock()->RemoveInstruction(instruction);
    RecordSimplification();
    return;
  }

  if ((input_cst != nullptr) && input_cst->IsOne()
      && input_other->GetType() == DataType::Type::kBool) {
    // Replace code looking like
    //    XOR dst, src, 1
    // with
    //    BOOLEAN_NOT dst, src
    HBooleanNot* boolean_not = new (GetGraph()->GetAllocator()) HBooleanNot(input_other);
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, boolean_not);
    RecordSimplification();
    return;
  }

  if ((input_cst != nullptr) && AreAllBitsSet(input_cst)) {
    // Replace code looking like
    //    XOR dst, src, 0xFFF...FF
    // with
    //    NOT dst, src
    HNot* bitwise_not = new (GetGraph()->GetAllocator()) HNot(instruction->GetType(), input_other);
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, bitwise_not);
    RecordSimplification();
    return;
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  if (((left->IsNot() && right->IsNot()) ||
       (left->IsBooleanNot() && right->IsBooleanNot())) &&
      left->HasOnlyOneNonEnvironmentUse() &&
      right->HasOnlyOneNonEnvironmentUse()) {
    // Replace code looking like
    //    NOT nota, a
    //    NOT notb, b
    //    XOR dst, nota, notb
    // with
    //    XOR dst, a, b
    instruction->ReplaceInput(left->InputAt(0), 0);
    instruction->ReplaceInput(right->InputAt(0), 1);
    left->GetBlock()->RemoveInstruction(left);
    right->GetBlock()->RemoveInstruction(right);
    RecordSimplification();
    return;
  }

  if (TryReplaceWithRotate(instruction)) {
    return;
  }

  // TryHandleAssociativeAndCommutativeOperation() does not remove its input,
  // so no need to return.
  TryHandleAssociativeAndCommutativeOperation(instruction);
}

void InstructionSimplifierVisitor::SimplifyStringEquals(HInvoke* instruction) {
  HInstruction* argument = instruction->InputAt(1);
  HInstruction* receiver = instruction->InputAt(0);
  if (receiver == argument) {
    // Because String.equals is an instance call, the receiver is
    // a null check if we don't know it's null. The argument however, will
    // be the actual object. So we cannot end up in a situation where both
    // are equal but could be null.
    DCHECK(CanEnsureNotNullAt(argument, instruction));
    instruction->ReplaceWith(GetGraph()->GetIntConstant(1));
    instruction->GetBlock()->RemoveInstruction(instruction);
  } else {
    StringEqualsOptimizations optimizations(instruction);
    if (CanEnsureNotNullAt(argument, instruction)) {
      optimizations.SetArgumentNotNull();
    }
    ScopedObjectAccess soa(Thread::Current());
    ReferenceTypeInfo argument_rti = argument->GetReferenceTypeInfo();
    if (argument_rti.IsValid() && argument_rti.IsStringClass()) {
      optimizations.SetArgumentIsString();
    } else if (kUseReadBarrier) {
      DCHECK(instruction->GetResolvedMethod() != nullptr);
      DCHECK(instruction->GetResolvedMethod()->GetDeclaringClass()->IsStringClass() ||
             // Object.equals() can be devirtualized to String.equals().
             instruction->GetResolvedMethod()->GetDeclaringClass()->IsObjectClass());
      Runtime* runtime = Runtime::Current();
      // For AOT, we always assume that the boot image shall contain the String.class and
      // we do not need a read barrier for boot image classes as they are non-moveable.
      // For JIT, check if we actually have a boot image; if we do, the String.class
      // should also be non-moveable.
      if (runtime->IsAotCompiler() || runtime->GetHeap()->HasBootImageSpace()) {
        DCHECK(runtime->IsAotCompiler() ||
               !runtime->GetHeap()->IsMovableObject(
                   instruction->GetResolvedMethod()->GetDeclaringClass()));
        optimizations.SetNoReadBarrierForStringClass();
      }
    }
  }
}

void InstructionSimplifierVisitor::SimplifyRotate(HInvoke* invoke,
                                                  bool is_left,
                                                  DataType::Type type) {
  DCHECK(invoke->IsInvokeStaticOrDirect());
  DCHECK_EQ(invoke->GetInvokeType(), InvokeType::kStatic);
  HInstruction* value = invoke->InputAt(0);
  HInstruction* distance = invoke->InputAt(1);
  // Replace the invoke with an HRor.
  if (is_left) {
    // Unconditionally set the type of the negated distance to `int`,
    // as shift and rotate operations expect a 32-bit (or narrower)
    // value for their distance input.
    distance = new (GetGraph()->GetAllocator()) HNeg(DataType::Type::kInt32, distance);
    invoke->GetBlock()->InsertInstructionBefore(distance, invoke);
  }
  HRor* ror = new (GetGraph()->GetAllocator()) HRor(type, value, distance);
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, ror);
  // Remove ClinitCheck and LoadClass, if possible.
  HInstruction* clinit = invoke->GetInputs().back();
  if (clinit->IsClinitCheck() && !clinit->HasUses()) {
    clinit->GetBlock()->RemoveInstruction(clinit);
    HInstruction* ldclass = clinit->InputAt(0);
    if (ldclass->IsLoadClass() && !ldclass->HasUses()) {
      ldclass->GetBlock()->RemoveInstruction(ldclass);
    }
  }
}

static bool IsArrayLengthOf(HInstruction* potential_length, HInstruction* potential_array) {
  if (potential_length->IsArrayLength()) {
    return potential_length->InputAt(0) == potential_array;
  }

  if (potential_array->IsNewArray()) {
    return potential_array->AsNewArray()->GetLength() == potential_length;
  }

  return false;
}

void InstructionSimplifierVisitor::SimplifySystemArrayCopy(HInvoke* instruction) {
  HInstruction* source = instruction->InputAt(0);
  HInstruction* destination = instruction->InputAt(2);
  HInstruction* count = instruction->InputAt(4);
  SystemArrayCopyOptimizations optimizations(instruction);
  if (CanEnsureNotNullAt(source, instruction)) {
    optimizations.SetSourceIsNotNull();
  }
  if (CanEnsureNotNullAt(destination, instruction)) {
    optimizations.SetDestinationIsNotNull();
  }
  if (destination == source) {
    optimizations.SetDestinationIsSource();
  }

  if (IsArrayLengthOf(count, source)) {
    optimizations.SetCountIsSourceLength();
  }

  if (IsArrayLengthOf(count, destination)) {
    optimizations.SetCountIsDestinationLength();
  }

  {
    ScopedObjectAccess soa(Thread::Current());
    DataType::Type source_component_type = DataType::Type::kVoid;
    DataType::Type destination_component_type = DataType::Type::kVoid;
    ReferenceTypeInfo destination_rti = destination->GetReferenceTypeInfo();
    if (destination_rti.IsValid()) {
      if (destination_rti.IsObjectArray()) {
        if (destination_rti.IsExact()) {
          optimizations.SetDoesNotNeedTypeCheck();
        }
        optimizations.SetDestinationIsTypedObjectArray();
      }
      if (destination_rti.IsPrimitiveArrayClass()) {
        destination_component_type = DataTypeFromPrimitive(
            destination_rti.GetTypeHandle()->GetComponentType()->GetPrimitiveType());
        optimizations.SetDestinationIsPrimitiveArray();
      } else if (destination_rti.IsNonPrimitiveArrayClass()) {
        optimizations.SetDestinationIsNonPrimitiveArray();
      }
    }
    ReferenceTypeInfo source_rti = source->GetReferenceTypeInfo();
    if (source_rti.IsValid()) {
      if (destination_rti.IsValid() && destination_rti.CanArrayHoldValuesOf(source_rti)) {
        optimizations.SetDoesNotNeedTypeCheck();
      }
      if (source_rti.IsPrimitiveArrayClass()) {
        optimizations.SetSourceIsPrimitiveArray();
        source_component_type = DataTypeFromPrimitive(
            source_rti.GetTypeHandle()->GetComponentType()->GetPrimitiveType());
      } else if (source_rti.IsNonPrimitiveArrayClass()) {
        optimizations.SetSourceIsNonPrimitiveArray();
      }
    }
    // For primitive arrays, use their optimized ArtMethod implementations.
    if ((source_component_type != DataType::Type::kVoid) &&
        (source_component_type == destination_component_type)) {
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      PointerSize image_size = class_linker->GetImagePointerSize();
      HInvokeStaticOrDirect* invoke = instruction->AsInvokeStaticOrDirect();
      mirror::Class* system = invoke->GetResolvedMethod()->GetDeclaringClass();
      ArtMethod* method = nullptr;
      switch (source_component_type) {
        case DataType::Type::kBool:
          method = system->FindClassMethod("arraycopy", "([ZI[ZII)V", image_size);
          break;
        case DataType::Type::kInt8:
          method = system->FindClassMethod("arraycopy", "([BI[BII)V", image_size);
          break;
        case DataType::Type::kUint16:
          method = system->FindClassMethod("arraycopy", "([CI[CII)V", image_size);
          break;
        case DataType::Type::kInt16:
          method = system->FindClassMethod("arraycopy", "([SI[SII)V", image_size);
          break;
        case DataType::Type::kInt32:
          method = system->FindClassMethod("arraycopy", "([II[III)V", image_size);
          break;
        case DataType::Type::kFloat32:
          method = system->FindClassMethod("arraycopy", "([FI[FII)V", image_size);
          break;
        case DataType::Type::kInt64:
          method = system->FindClassMethod("arraycopy", "([JI[JII)V", image_size);
          break;
        case DataType::Type::kFloat64:
          method = system->FindClassMethod("arraycopy", "([DI[DII)V", image_size);
          break;
        default:
          LOG(FATAL) << "Unreachable";
      }
      DCHECK(method != nullptr);
      DCHECK(method->IsStatic());
      DCHECK(method->GetDeclaringClass() == system);
      invoke->SetResolvedMethod(method);
      // Sharpen the new invoke. Note that we do not update the dex method index of
      // the invoke, as we would need to look it up in the current dex file, and it
      // is unlikely that it exists. The most usual situation for such typed
      // arraycopy methods is a direct pointer to the boot image.
      HSharpening::SharpenInvokeStaticOrDirect(invoke, codegen_, compiler_driver_);
    }
  }
}

void InstructionSimplifierVisitor::SimplifyCompare(HInvoke* invoke,
                                                   bool is_signum,
                                                   DataType::Type type) {
  DCHECK(invoke->IsInvokeStaticOrDirect());
  uint32_t dex_pc = invoke->GetDexPc();
  HInstruction* left = invoke->InputAt(0);
  HInstruction* right;
  if (!is_signum) {
    right = invoke->InputAt(1);
  } else if (type == DataType::Type::kInt64) {
    right = GetGraph()->GetLongConstant(0);
  } else {
    right = GetGraph()->GetIntConstant(0);
  }
  HCompare* compare = new (GetGraph()->GetAllocator())
      HCompare(type, left, right, ComparisonBias::kNoBias, dex_pc);
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, compare);
}

void InstructionSimplifierVisitor::SimplifyIsNaN(HInvoke* invoke) {
  DCHECK(invoke->IsInvokeStaticOrDirect());
  uint32_t dex_pc = invoke->GetDexPc();
  // IsNaN(x) is the same as x != x.
  HInstruction* x = invoke->InputAt(0);
  HCondition* condition = new (GetGraph()->GetAllocator()) HNotEqual(x, x, dex_pc);
  condition->SetBias(ComparisonBias::kLtBias);
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, condition);
}

void InstructionSimplifierVisitor::SimplifyFP2Int(HInvoke* invoke) {
  DCHECK(invoke->IsInvokeStaticOrDirect());
  uint32_t dex_pc = invoke->GetDexPc();
  HInstruction* x = invoke->InputAt(0);
  DataType::Type type = x->GetType();
  // Set proper bit pattern for NaN and replace intrinsic with raw version.
  HInstruction* nan;
  if (type == DataType::Type::kFloat64) {
    nan = GetGraph()->GetLongConstant(0x7ff8000000000000L);
    invoke->SetIntrinsic(Intrinsics::kDoubleDoubleToRawLongBits,
                         kNeedsEnvironmentOrCache,
                         kNoSideEffects,
                         kNoThrow);
  } else {
    DCHECK_EQ(type, DataType::Type::kFloat32);
    nan = GetGraph()->GetIntConstant(0x7fc00000);
    invoke->SetIntrinsic(Intrinsics::kFloatFloatToRawIntBits,
                         kNeedsEnvironmentOrCache,
                         kNoSideEffects,
                         kNoThrow);
  }
  // Test IsNaN(x), which is the same as x != x.
  HCondition* condition = new (GetGraph()->GetAllocator()) HNotEqual(x, x, dex_pc);
  condition->SetBias(ComparisonBias::kLtBias);
  invoke->GetBlock()->InsertInstructionBefore(condition, invoke->GetNext());
  // Select between the two.
  HInstruction* select = new (GetGraph()->GetAllocator()) HSelect(condition, nan, invoke, dex_pc);
  invoke->GetBlock()->InsertInstructionBefore(select, condition->GetNext());
  invoke->ReplaceWithExceptInReplacementAtIndex(select, 0);  // false at index 0
}

void InstructionSimplifierVisitor::SimplifyStringCharAt(HInvoke* invoke) {
  HInstruction* str = invoke->InputAt(0);
  HInstruction* index = invoke->InputAt(1);
  uint32_t dex_pc = invoke->GetDexPc();
  ArenaAllocator* allocator = GetGraph()->GetAllocator();
  // We treat String as an array to allow DCE and BCE to seamlessly work on strings,
  // so create the HArrayLength, HBoundsCheck and HArrayGet.
  HArrayLength* length = new (allocator) HArrayLength(str, dex_pc, /* is_string_length */ true);
  invoke->GetBlock()->InsertInstructionBefore(length, invoke);
  HBoundsCheck* bounds_check = new (allocator) HBoundsCheck(
      index, length, dex_pc, /* is_string_char_at */ true);
  invoke->GetBlock()->InsertInstructionBefore(bounds_check, invoke);
  HArrayGet* array_get = new (allocator) HArrayGet(str,
                                                   bounds_check,
                                                   DataType::Type::kUint16,
                                                   SideEffects::None(),  // Strings are immutable.
                                                   dex_pc,
                                                   /* is_string_char_at */ true);
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, array_get);
  bounds_check->CopyEnvironmentFrom(invoke->GetEnvironment());
  GetGraph()->SetHasBoundsChecks(true);
}

void InstructionSimplifierVisitor::SimplifyStringIsEmptyOrLength(HInvoke* invoke) {
  HInstruction* str = invoke->InputAt(0);
  uint32_t dex_pc = invoke->GetDexPc();
  // We treat String as an array to allow DCE and BCE to seamlessly work on strings,
  // so create the HArrayLength.
  HArrayLength* length =
      new (GetGraph()->GetAllocator()) HArrayLength(str, dex_pc, /* is_string_length */ true);
  HInstruction* replacement;
  if (invoke->GetIntrinsic() == Intrinsics::kStringIsEmpty) {
    // For String.isEmpty(), create the `HEqual` representing the `length == 0`.
    invoke->GetBlock()->InsertInstructionBefore(length, invoke);
    HIntConstant* zero = GetGraph()->GetIntConstant(0);
    HEqual* equal = new (GetGraph()->GetAllocator()) HEqual(length, zero, dex_pc);
    replacement = equal;
  } else {
    DCHECK_EQ(invoke->GetIntrinsic(), Intrinsics::kStringLength);
    replacement = length;
  }
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, replacement);
}

// This method should only be used on intrinsics whose sole way of throwing an
// exception is raising a NPE when the nth argument is null. If that argument
// is provably non-null, we can clear the flag.
void InstructionSimplifierVisitor::SimplifyNPEOnArgN(HInvoke* invoke, size_t n) {
  HInstruction* arg = invoke->InputAt(n);
  if (invoke->CanThrow() && !arg->CanBeNull()) {
    invoke->SetCanThrow(false);
  }
}

// Methods that return "this" can replace the returned value with the receiver.
void InstructionSimplifierVisitor::SimplifyReturnThis(HInvoke* invoke) {
  if (invoke->HasUses()) {
    HInstruction* receiver = invoke->InputAt(0);
    invoke->ReplaceWith(receiver);
    RecordSimplification();
  }
}

// Helper method for StringBuffer escape analysis.
static bool NoEscapeForStringBufferReference(HInstruction* reference, HInstruction* user) {
  if (user->IsInvokeStaticOrDirect()) {
    // Any constructor on StringBuffer is okay.
    return user->AsInvokeStaticOrDirect()->GetResolvedMethod() != nullptr &&
           user->AsInvokeStaticOrDirect()->GetResolvedMethod()->IsConstructor() &&
           user->InputAt(0) == reference;
  } else if (user->IsInvokeVirtual()) {
    switch (user->AsInvokeVirtual()->GetIntrinsic()) {
      case Intrinsics::kStringBufferLength:
      case Intrinsics::kStringBufferToString:
        DCHECK_EQ(user->InputAt(0), reference);
        return true;
      case Intrinsics::kStringBufferAppend:
        // Returns "this", so only okay if no further uses.
        DCHECK_EQ(user->InputAt(0), reference);
        DCHECK_NE(user->InputAt(1), reference);
        return !user->HasUses();
      default:
        break;
    }
  }
  return false;
}

// Certain allocation intrinsics are not removed by dead code elimination
// because of potentially throwing an OOM exception or other side effects.
// This method removes such intrinsics when special circumstances allow.
void InstructionSimplifierVisitor::SimplifyAllocationIntrinsic(HInvoke* invoke) {
  if (!invoke->HasUses()) {
    // Instruction has no uses. If unsynchronized, we can remove right away, safely ignoring
    // the potential OOM of course. Otherwise, we must ensure the receiver object of this
    // call does not escape since only thread-local synchronization may be removed.
    bool is_synchronized = invoke->GetIntrinsic() == Intrinsics::kStringBufferToString;
    HInstruction* receiver = invoke->InputAt(0);
    if (!is_synchronized || DoesNotEscape(receiver, NoEscapeForStringBufferReference)) {
      invoke->GetBlock()->RemoveInstruction(invoke);
      RecordSimplification();
    }
  }
}

void InstructionSimplifierVisitor::SimplifyMemBarrier(HInvoke* invoke,
                                                      MemBarrierKind barrier_kind) {
  uint32_t dex_pc = invoke->GetDexPc();
  HMemoryBarrier* mem_barrier =
      new (GetGraph()->GetAllocator()) HMemoryBarrier(barrier_kind, dex_pc);
  invoke->GetBlock()->ReplaceAndRemoveInstructionWith(invoke, mem_barrier);
}

void InstructionSimplifierVisitor::VisitInvoke(HInvoke* instruction) {
  switch (instruction->GetIntrinsic()) {
    case Intrinsics::kStringEquals:
      SimplifyStringEquals(instruction);
      break;
    case Intrinsics::kSystemArrayCopy:
      SimplifySystemArrayCopy(instruction);
      break;
    case Intrinsics::kIntegerRotateRight:
      SimplifyRotate(instruction, /* is_left */ false, DataType::Type::kInt32);
      break;
    case Intrinsics::kLongRotateRight:
      SimplifyRotate(instruction, /* is_left */ false, DataType::Type::kInt64);
      break;
    case Intrinsics::kIntegerRotateLeft:
      SimplifyRotate(instruction, /* is_left */ true, DataType::Type::kInt32);
      break;
    case Intrinsics::kLongRotateLeft:
      SimplifyRotate(instruction, /* is_left */ true, DataType::Type::kInt64);
      break;
    case Intrinsics::kIntegerCompare:
      SimplifyCompare(instruction, /* is_signum */ false, DataType::Type::kInt32);
      break;
    case Intrinsics::kLongCompare:
      SimplifyCompare(instruction, /* is_signum */ false, DataType::Type::kInt64);
      break;
    case Intrinsics::kIntegerSignum:
      SimplifyCompare(instruction, /* is_signum */ true, DataType::Type::kInt32);
      break;
    case Intrinsics::kLongSignum:
      SimplifyCompare(instruction, /* is_signum */ true, DataType::Type::kInt64);
      break;
    case Intrinsics::kFloatIsNaN:
    case Intrinsics::kDoubleIsNaN:
      SimplifyIsNaN(instruction);
      break;
    case Intrinsics::kFloatFloatToIntBits:
    case Intrinsics::kDoubleDoubleToLongBits:
      SimplifyFP2Int(instruction);
      break;
    case Intrinsics::kStringCharAt:
      SimplifyStringCharAt(instruction);
      break;
    case Intrinsics::kStringIsEmpty:
    case Intrinsics::kStringLength:
      SimplifyStringIsEmptyOrLength(instruction);
      break;
    case Intrinsics::kStringStringIndexOf:
    case Intrinsics::kStringStringIndexOfAfter:
      SimplifyNPEOnArgN(instruction, 1);  // 0th has own NullCheck
      break;
    case Intrinsics::kStringBufferAppend:
    case Intrinsics::kStringBuilderAppend:
      SimplifyReturnThis(instruction);
      break;
    case Intrinsics::kStringBufferToString:
    case Intrinsics::kStringBuilderToString:
      SimplifyAllocationIntrinsic(instruction);
      break;
    case Intrinsics::kUnsafeLoadFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kLoadAny);
      break;
    case Intrinsics::kUnsafeStoreFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kAnyStore);
      break;
    case Intrinsics::kUnsafeFullFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kAnyAny);
      break;
    case Intrinsics::kVarHandleFullFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kAnyAny);
      break;
    case Intrinsics::kVarHandleAcquireFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kLoadAny);
      break;
    case Intrinsics::kVarHandleReleaseFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kAnyStore);
      break;
    case Intrinsics::kVarHandleLoadLoadFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kLoadAny);
      break;
    case Intrinsics::kVarHandleStoreStoreFence:
      SimplifyMemBarrier(instruction, MemBarrierKind::kStoreStore);
      break;
    default:
      break;
  }
}

void InstructionSimplifierVisitor::VisitDeoptimize(HDeoptimize* deoptimize) {
  HInstruction* cond = deoptimize->InputAt(0);
  if (cond->IsConstant()) {
    if (cond->AsIntConstant()->IsFalse()) {
      // Never deopt: instruction can be removed.
      if (deoptimize->GuardsAnInput()) {
        deoptimize->ReplaceWith(deoptimize->GuardedInput());
      }
      deoptimize->GetBlock()->RemoveInstruction(deoptimize);
    } else {
      // Always deopt.
    }
  }
}

// Replace code looking like
//    OP y, x, const1
//    OP z, y, const2
// with
//    OP z, x, const3
// where OP is both an associative and a commutative operation.
bool InstructionSimplifierVisitor::TryHandleAssociativeAndCommutativeOperation(
    HBinaryOperation* instruction) {
  DCHECK(instruction->IsCommutative());

  if (!DataType::IsIntegralType(instruction->GetType())) {
    return false;
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  // Variable names as described above.
  HConstant* const2;
  HBinaryOperation* y;

  if (instruction->InstructionTypeEquals(left) && right->IsConstant()) {
    const2 = right->AsConstant();
    y = left->AsBinaryOperation();
  } else if (left->IsConstant() && instruction->InstructionTypeEquals(right)) {
    const2 = left->AsConstant();
    y = right->AsBinaryOperation();
  } else {
    // The node does not match the pattern.
    return false;
  }

  // If `y` has more than one use, we do not perform the optimization
  // because it might increase code size (e.g. if the new constant is
  // no longer encodable as an immediate operand in the target ISA).
  if (!y->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }

  // GetConstantRight() can return both left and right constants
  // for commutative operations.
  HConstant* const1 = y->GetConstantRight();
  if (const1 == nullptr) {
    return false;
  }

  instruction->ReplaceInput(const1, 0);
  instruction->ReplaceInput(const2, 1);
  HConstant* const3 = instruction->TryStaticEvaluation();
  DCHECK(const3 != nullptr);
  instruction->ReplaceInput(y->GetLeastConstantLeft(), 0);
  instruction->ReplaceInput(const3, 1);
  RecordSimplification();
  return true;
}

static HBinaryOperation* AsAddOrSub(HInstruction* binop) {
  return (binop->IsAdd() || binop->IsSub()) ? binop->AsBinaryOperation() : nullptr;
}

// Helper function that performs addition statically, considering the result type.
static int64_t ComputeAddition(DataType::Type type, int64_t x, int64_t y) {
  // Use the Compute() method for consistency with TryStaticEvaluation().
  if (type == DataType::Type::kInt32) {
    return HAdd::Compute<int32_t>(x, y);
  } else {
    DCHECK_EQ(type, DataType::Type::kInt64);
    return HAdd::Compute<int64_t>(x, y);
  }
}

// Helper function that handles the child classes of HConstant
// and returns an integer with the appropriate sign.
static int64_t GetValue(HConstant* constant, bool is_negated) {
  int64_t ret = Int64FromConstant(constant);
  return is_negated ? -ret : ret;
}

// Replace code looking like
//    OP1 y, x, const1
//    OP2 z, y, const2
// with
//    OP3 z, x, const3
// where OPx is either ADD or SUB, and at least one of OP{1,2} is SUB.
bool InstructionSimplifierVisitor::TrySubtractionChainSimplification(
    HBinaryOperation* instruction) {
  DCHECK(instruction->IsAdd() || instruction->IsSub()) << instruction->DebugName();

  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntegralType(type)) {
    return false;
  }

  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  // Variable names as described above.
  HConstant* const2 = right->IsConstant() ? right->AsConstant() : left->AsConstant();
  if (const2 == nullptr) {
    return false;
  }

  HBinaryOperation* y = (AsAddOrSub(left) != nullptr)
      ? left->AsBinaryOperation()
      : AsAddOrSub(right);
  // If y has more than one use, we do not perform the optimization because
  // it might increase code size (e.g. if the new constant is no longer
  // encodable as an immediate operand in the target ISA).
  if ((y == nullptr) || !y->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }

  left = y->GetLeft();
  HConstant* const1 = left->IsConstant() ? left->AsConstant() : y->GetRight()->AsConstant();
  if (const1 == nullptr) {
    return false;
  }

  HInstruction* x = (const1 == left) ? y->GetRight() : left;
  // If both inputs are constants, let the constant folding pass deal with it.
  if (x->IsConstant()) {
    return false;
  }

  bool is_const2_negated = (const2 == right) && instruction->IsSub();
  int64_t const2_val = GetValue(const2, is_const2_negated);
  bool is_y_negated = (y == right) && instruction->IsSub();
  right = y->GetRight();
  bool is_const1_negated = is_y_negated ^ ((const1 == right) && y->IsSub());
  int64_t const1_val = GetValue(const1, is_const1_negated);
  bool is_x_negated = is_y_negated ^ ((x == right) && y->IsSub());
  int64_t const3_val = ComputeAddition(type, const1_val, const2_val);
  HBasicBlock* block = instruction->GetBlock();
  HConstant* const3 = block->GetGraph()->GetConstant(type, const3_val);
  ArenaAllocator* allocator = instruction->GetAllocator();
  HInstruction* z;

  if (is_x_negated) {
    z = new (allocator) HSub(type, const3, x, instruction->GetDexPc());
  } else {
    z = new (allocator) HAdd(type, x, const3, instruction->GetDexPc());
  }

  block->ReplaceAndRemoveInstructionWith(instruction, z);
  RecordSimplification();
  return true;
}

void InstructionSimplifierVisitor::VisitVecMul(HVecMul* instruction) {
  if (TryCombineVecMultiplyAccumulate(instruction)) {
    RecordSimplification();
  }
}

}  // namespace art
