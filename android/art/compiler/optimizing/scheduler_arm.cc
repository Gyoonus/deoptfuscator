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

#include "scheduler_arm.h"

#include "arch/arm/instruction_set_features_arm.h"
#include "code_generator_utils.h"
#include "common_arm.h"
#include "heap_poisoning.h"
#include "mirror/array-inl.h"
#include "mirror/string.h"

namespace art {
namespace arm {

using helpers::Int32ConstantFrom;
using helpers::Uint64ConstantFrom;

void SchedulingLatencyVisitorARM::HandleBinaryOperationLantencies(HBinaryOperation* instr) {
  switch (instr->GetResultType()) {
    case DataType::Type::kInt64:
      // HAdd and HSub long operations translate to ADDS+ADC or SUBS+SBC pairs,
      // so a bubble (kArmNopLatency) is added to represent the internal carry flag
      // dependency inside these pairs.
      last_visited_internal_latency_ = kArmIntegerOpLatency + kArmNopLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      last_visited_latency_ = kArmFloatingPointOpLatency;
      break;
    default:
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitAdd(HAdd* instr) {
  HandleBinaryOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitSub(HSub* instr) {
  HandleBinaryOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitMul(HMul* instr) {
  switch (instr->GetResultType()) {
    case DataType::Type::kInt64:
      last_visited_internal_latency_ = 3 * kArmMulIntegerLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      last_visited_latency_ = kArmMulFloatingPointLatency;
      break;
    default:
      last_visited_latency_ = kArmMulIntegerLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::HandleBitwiseOperationLantencies(HBinaryOperation* instr) {
  switch (instr->GetResultType()) {
    case DataType::Type::kInt64:
      last_visited_internal_latency_ = kArmIntegerOpLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      last_visited_latency_ = kArmFloatingPointOpLatency;
      break;
    default:
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitAnd(HAnd* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitOr(HOr* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitXor(HXor* instr) {
  HandleBitwiseOperationLantencies(instr);
}

void SchedulingLatencyVisitorARM::VisitRor(HRor* instr) {
  switch (instr->GetResultType()) {
    case DataType::Type::kInt32:
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case DataType::Type::kInt64: {
      // HandleLongRotate
      HInstruction* rhs = instr->GetRight();
      if (rhs->IsConstant()) {
        uint64_t rot = Uint64ConstantFrom(rhs->AsConstant()) & kMaxLongShiftDistance;
        if (rot != 0u) {
          last_visited_internal_latency_ = 3 * kArmIntegerOpLatency;
          last_visited_latency_ = kArmIntegerOpLatency;
        } else {
          last_visited_internal_latency_ = kArmIntegerOpLatency;
          last_visited_latency_ = kArmIntegerOpLatency;
        }
      } else {
        last_visited_internal_latency_ = 9 * kArmIntegerOpLatency + kArmBranchLatency;
        last_visited_latency_ = kArmBranchLatency;
      }
      break;
    }
    default:
      LOG(FATAL) << "Unexpected operation type " << instr->GetResultType();
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::HandleShiftLatencies(HBinaryOperation* instr) {
  DataType::Type type = instr->GetResultType();
  HInstruction* rhs = instr->GetRight();
  switch (type) {
    case DataType::Type::kInt32:
      if (!rhs->IsConstant()) {
        last_visited_internal_latency_ = kArmIntegerOpLatency;
      }
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case DataType::Type::kInt64:
      if (!rhs->IsConstant()) {
        last_visited_internal_latency_ = 8 * kArmIntegerOpLatency;
      } else {
        uint32_t shift_value = Int32ConstantFrom(rhs->AsConstant()) & kMaxLongShiftDistance;
        if (shift_value == 1 || shift_value >= 32) {
          last_visited_internal_latency_ = kArmIntegerOpLatency;
        } else {
          last_visited_internal_latency_ = 2 * kArmIntegerOpLatency;
        }
      }
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    default:
      LOG(FATAL) << "Unexpected operation type " << type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitShl(HShl* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::VisitShr(HShr* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::VisitUShr(HUShr* instr) {
  HandleShiftLatencies(instr);
}

void SchedulingLatencyVisitorARM::HandleGenerateConditionWithZero(IfCondition condition) {
  switch (condition) {
    case kCondEQ:
    case kCondBE:
    case kCondNE:
    case kCondA:
      last_visited_internal_latency_ += kArmIntegerOpLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case kCondGE:
      // Mvn
      last_visited_internal_latency_ += kArmIntegerOpLatency;
      FALLTHROUGH_INTENDED;
    case kCondLT:
      // Lsr
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case kCondAE:
      // Trivially true.
      // Mov
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    case kCondB:
      // Trivially false.
      // Mov
      last_visited_latency_ = kArmIntegerOpLatency;
      break;
    default:
      LOG(FATAL) << "Unexpected condition " << condition;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateLongTestConstant(HCondition* condition) {
  DCHECK_EQ(condition->GetLeft()->GetType(), DataType::Type::kInt64);

  IfCondition cond = condition->GetCondition();

  HInstruction* right = condition->InputAt(1);

  int64_t value = Uint64ConstantFrom(right);

  // Comparisons against 0 are common enough, so codegen has special handling for them.
  if (value == 0) {
    switch (cond) {
      case kCondNE:
      case kCondA:
      case kCondEQ:
      case kCondBE:
        // Orrs
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        return;
      case kCondLT:
      case kCondGE:
        // Cmp
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        return;
      case kCondB:
      case kCondAE:
        // Cmp
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        return;
      default:
        break;
    }
  }

  switch (cond) {
    case kCondEQ:
    case kCondNE:
    case kCondB:
    case kCondBE:
    case kCondA:
    case kCondAE: {
      // Cmp, IT, Cmp
      last_visited_internal_latency_ += 3 * kArmIntegerOpLatency;
      break;
    }
    case kCondLE:
    case kCondGT:
      // Trivially true or false.
      if (value == std::numeric_limits<int64_t>::max()) {
        // Cmp
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        break;
      }
      FALLTHROUGH_INTENDED;
    case kCondGE:
    case kCondLT: {
      // Cmp, Sbcs
      last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;
      break;
    }
    default:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateLongTest(HCondition* condition) {
  DCHECK_EQ(condition->GetLeft()->GetType(), DataType::Type::kInt64);

  IfCondition cond = condition->GetCondition();

  switch (cond) {
    case kCondEQ:
    case kCondNE:
    case kCondB:
    case kCondBE:
    case kCondA:
    case kCondAE: {
      // Cmp, IT, Cmp
      last_visited_internal_latency_ += 3 * kArmIntegerOpLatency;
      break;
    }
    case kCondLE:
    case kCondGT:
    case kCondGE:
    case kCondLT: {
      // Cmp, Sbcs
      last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;
      break;
    }
    default:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
  }
}

// The GenerateTest series of function all counted as internal latency.
void SchedulingLatencyVisitorARM::HandleGenerateTest(HCondition* condition) {
  const DataType::Type type = condition->GetLeft()->GetType();

  if (type == DataType::Type::kInt64) {
    condition->InputAt(1)->IsConstant()
        ? HandleGenerateLongTestConstant(condition)
        : HandleGenerateLongTest(condition);
  } else if (DataType::IsFloatingPointType(type)) {
    // GenerateVcmp + Vmrs
    last_visited_internal_latency_ += 2 * kArmFloatingPointOpLatency;
  } else {
    // Cmp
    last_visited_internal_latency_ += kArmIntegerOpLatency;
  }
}

bool SchedulingLatencyVisitorARM::CanGenerateTest(HCondition* condition) {
  if (condition->GetLeft()->GetType() == DataType::Type::kInt64) {
    HInstruction* right = condition->InputAt(1);

    if (right->IsConstant()) {
      IfCondition c = condition->GetCondition();
      const uint64_t value = Uint64ConstantFrom(right);

      if (c < kCondLT || c > kCondGE) {
        if (value != 0) {
          return false;
        }
      } else if (c == kCondLE || c == kCondGT) {
        if (value < std::numeric_limits<int64_t>::max() &&
            !codegen_->GetAssembler()->ShifterOperandCanHold(
                SBC, High32Bits(value + 1), vixl32::FlagsUpdate::SetFlags)) {
          return false;
        }
      } else if (!codegen_->GetAssembler()->ShifterOperandCanHold(
                      SBC, High32Bits(value), vixl32::FlagsUpdate::SetFlags)) {
        return false;
      }
    }
  }

  return true;
}

void SchedulingLatencyVisitorARM::HandleGenerateConditionGeneric(HCondition* cond) {
  HandleGenerateTest(cond);

  // Unlike codegen pass, we cannot check 'out' register IsLow() here,
  // because scheduling is before liveness(location builder) and register allocator,
  // so we can only choose to follow one path of codegen by assuming otu.IsLow() is true.
  last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;
  last_visited_latency_ = kArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::HandleGenerateEqualLong(HCondition* cond) {
  DCHECK_EQ(cond->GetLeft()->GetType(), DataType::Type::kInt64);

  IfCondition condition = cond->GetCondition();

  last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;

  if (condition == kCondNE) {
    // Orrs, IT, Mov
    last_visited_internal_latency_ += 3 * kArmIntegerOpLatency;
  } else {
    last_visited_internal_latency_ += kArmIntegerOpLatency;
    HandleGenerateConditionWithZero(condition);
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateLongComparesAndJumps() {
  last_visited_internal_latency_ += 4 * kArmIntegerOpLatency;
  last_visited_internal_latency_ += kArmBranchLatency;
}

void SchedulingLatencyVisitorARM::HandleGenerateConditionLong(HCondition* cond) {
  DCHECK_EQ(cond->GetLeft()->GetType(), DataType::Type::kInt64);

  IfCondition condition = cond->GetCondition();
  HInstruction* right = cond->InputAt(1);

  if (right->IsConstant()) {
    // Comparisons against 0 are common enough, so codegen has special handling for them.
    if (Uint64ConstantFrom(right) == 0) {
      switch (condition) {
        case kCondNE:
        case kCondA:
        case kCondEQ:
        case kCondBE:
          // Orr
          last_visited_internal_latency_ += kArmIntegerOpLatency;
          HandleGenerateConditionWithZero(condition);
          return;
        case kCondLT:
        case kCondGE:
          FALLTHROUGH_INTENDED;
        case kCondAE:
        case kCondB:
          HandleGenerateConditionWithZero(condition);
          return;
        case kCondLE:
        case kCondGT:
        default:
          break;
      }
    }
  }

  if ((condition == kCondEQ || condition == kCondNE) &&
      !CanGenerateTest(cond)) {
    HandleGenerateEqualLong(cond);
    return;
  }

  if (CanGenerateTest(cond)) {
    HandleGenerateConditionGeneric(cond);
    return;
  }

  HandleGenerateLongComparesAndJumps();

  last_visited_internal_latency_ += kArmIntegerOpLatency;
  last_visited_latency_ = kArmBranchLatency;;
}

void SchedulingLatencyVisitorARM::HandleGenerateConditionIntegralOrNonPrimitive(HCondition* cond) {
  const DataType::Type type = cond->GetLeft()->GetType();

  DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;

  if (type == DataType::Type::kInt64) {
    HandleGenerateConditionLong(cond);
    return;
  }

  IfCondition condition = cond->GetCondition();
  HInstruction* right = cond->InputAt(1);
  int64_t value;

  if (right->IsConstant()) {
    value = Uint64ConstantFrom(right);

    // Comparisons against 0 are common enough, so codegen has special handling for them.
    if (value == 0) {
      switch (condition) {
        case kCondNE:
        case kCondA:
        case kCondEQ:
        case kCondBE:
        case kCondLT:
        case kCondGE:
        case kCondAE:
        case kCondB:
          HandleGenerateConditionWithZero(condition);
          return;
        case kCondLE:
        case kCondGT:
        default:
          break;
      }
    }
  }

  if (condition == kCondEQ || condition == kCondNE) {
    if (condition == kCondNE) {
      // CMP, IT, MOV.ne
      last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;
      last_visited_latency_ = kArmIntegerOpLatency;
    } else {
      last_visited_internal_latency_ += kArmIntegerOpLatency;
      HandleGenerateConditionWithZero(condition);
    }
    return;
  }

  HandleGenerateConditionGeneric(cond);
}

void SchedulingLatencyVisitorARM::HandleCondition(HCondition* cond) {
  if (cond->IsEmittedAtUseSite()) {
    last_visited_latency_ = 0;
    return;
  }

  const DataType::Type type = cond->GetLeft()->GetType();

  if (DataType::IsFloatingPointType(type)) {
    HandleGenerateConditionGeneric(cond);
    return;
  }

  DCHECK(DataType::IsIntegralType(type) || type == DataType::Type::kReference) << type;

  const IfCondition condition = cond->GetCondition();

  if (type == DataType::Type::kBool &&
      cond->GetRight()->GetType() == DataType::Type::kBool &&
      (condition == kCondEQ || condition == kCondNE)) {
    if (condition == kCondEQ) {
      last_visited_internal_latency_ = kArmIntegerOpLatency;
    }
    last_visited_latency_ = kArmIntegerOpLatency;
    return;
  }

  HandleGenerateConditionIntegralOrNonPrimitive(cond);
}

void SchedulingLatencyVisitorARM::VisitCondition(HCondition* instr) {
  HandleCondition(instr);
}

void SchedulingLatencyVisitorARM::VisitCompare(HCompare* instr) {
  DataType::Type type = instr->InputAt(0)->GetType();
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
      last_visited_internal_latency_ = 2 * kArmIntegerOpLatency;
      break;
    case DataType::Type::kInt64:
      last_visited_internal_latency_ = 2 * kArmIntegerOpLatency + 3 * kArmBranchLatency;
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      last_visited_internal_latency_ = kArmIntegerOpLatency + 2 * kArmFloatingPointOpLatency;
      break;
    default:
      last_visited_internal_latency_ = 2 * kArmIntegerOpLatency;
      break;
  }
  last_visited_latency_ = kArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitBitwiseNegatedRight(HBitwiseNegatedRight* instruction) {
  if (instruction->GetResultType() == DataType::Type::kInt32) {
    last_visited_latency_ = kArmIntegerOpLatency;
  } else {
    last_visited_internal_latency_ = kArmIntegerOpLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateDataProcInstruction(bool internal_latency) {
  if (internal_latency) {
    last_visited_internal_latency_ += kArmIntegerOpLatency;
  } else {
    last_visited_latency_ = kArmDataProcWithShifterOpLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateDataProc(HDataProcWithShifterOp* instruction) {
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();
  if (kind == HInstruction::kAdd) {
    last_visited_internal_latency_ = kArmIntegerOpLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  } else if (kind == HInstruction::kSub) {
    last_visited_internal_latency_ = kArmIntegerOpLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  } else {
    HandleGenerateDataProcInstruction(/* internal_latency */ true);
    HandleGenerateDataProcInstruction();
  }
}

void SchedulingLatencyVisitorARM::HandleGenerateLongDataProc(HDataProcWithShifterOp* instruction) {
  DCHECK_EQ(instruction->GetType(), DataType::Type::kInt64);
  DCHECK(HDataProcWithShifterOp::IsShiftOp(instruction->GetOpKind()));

  const uint32_t shift_value = instruction->GetShiftAmount();
  const HInstruction::InstructionKind kind = instruction->GetInstrKind();

  if (shift_value >= 32) {
    // Different shift types actually generate similar code here,
    // no need to differentiate shift types like the codegen pass does,
    // which also avoids handling shift types from different ARM backends.
    HandleGenerateDataProc(instruction);
  } else {
    DCHECK_GT(shift_value, 1U);
    DCHECK_LT(shift_value, 32U);

    if (kind == HInstruction::kOr || kind == HInstruction::kXor) {
      HandleGenerateDataProcInstruction(/* internal_latency */ true);
      HandleGenerateDataProcInstruction(/* internal_latency */ true);
      HandleGenerateDataProcInstruction();
    } else {
      last_visited_internal_latency_ += 2 * kArmIntegerOpLatency;
      HandleGenerateDataProc(instruction);
    }
  }
}

void SchedulingLatencyVisitorARM::VisitDataProcWithShifterOp(HDataProcWithShifterOp* instruction) {
  const HDataProcWithShifterOp::OpKind op_kind = instruction->GetOpKind();

  if (instruction->GetType() == DataType::Type::kInt32) {
    HandleGenerateDataProcInstruction();
  } else {
    DCHECK_EQ(instruction->GetType(), DataType::Type::kInt64);
    if (HDataProcWithShifterOp::IsExtensionOp(op_kind)) {
      HandleGenerateDataProc(instruction);
    } else {
      HandleGenerateLongDataProc(instruction);
    }
  }
}

void SchedulingLatencyVisitorARM::VisitIntermediateAddress(HIntermediateAddress* ATTRIBUTE_UNUSED) {
  // Although the code generated is a simple `add` instruction, we found through empirical results
  // that spacing it from its use in memory accesses was beneficial.
  last_visited_internal_latency_ = kArmNopLatency;
  last_visited_latency_ = kArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitIntermediateAddressIndex(
    HIntermediateAddressIndex* ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "IntermediateAddressIndex is not implemented for ARM";
}

void SchedulingLatencyVisitorARM::VisitMultiplyAccumulate(HMultiplyAccumulate* ATTRIBUTE_UNUSED) {
  last_visited_latency_ = kArmMulIntegerLatency;
}

void SchedulingLatencyVisitorARM::VisitArrayGet(HArrayGet* instruction) {
  DataType::Type type = instruction->GetType();
  const bool maybe_compressed_char_at =
      mirror::kUseStringCompression && instruction->IsStringCharAt();
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();
  HInstruction* index = instruction->InputAt(1);

  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      if (maybe_compressed_char_at) {
        last_visited_internal_latency_ += kArmMemoryLoadLatency;
      }
      if (index->IsConstant()) {
        if (maybe_compressed_char_at) {
          last_visited_internal_latency_ +=
              kArmIntegerOpLatency + kArmBranchLatency + kArmMemoryLoadLatency;
          last_visited_latency_ = kArmBranchLatency;
        } else {
          last_visited_latency_ += kArmMemoryLoadLatency;
        }
      } else {
        if (has_intermediate_address) {
        } else {
          last_visited_internal_latency_ += kArmIntegerOpLatency;
        }
        if (maybe_compressed_char_at) {
          last_visited_internal_latency_ +=
              kArmIntegerOpLatency + kArmBranchLatency + kArmMemoryLoadLatency;
          last_visited_latency_ = kArmBranchLatency;
        } else {
          last_visited_latency_ += kArmMemoryLoadLatency;
        }
      }
      break;
    }

    case DataType::Type::kReference: {
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        last_visited_latency_ = kArmLoadWithBakerReadBarrierLatency;
      } else {
        if (index->IsConstant()) {
          last_visited_latency_ = kArmMemoryLoadLatency;
        } else {
          if (has_intermediate_address) {
          } else {
            last_visited_internal_latency_ += kArmIntegerOpLatency;
          }
          last_visited_internal_latency_ = kArmMemoryLoadLatency;
        }
      }
      break;
    }

    case DataType::Type::kInt64: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    case DataType::Type::kFloat32: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    case DataType::Type::kFloat64: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ += kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    default:
      LOG(FATAL) << "Unreachable type " << type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitArrayLength(HArrayLength* instruction) {
  last_visited_latency_ = kArmMemoryLoadLatency;
  if (mirror::kUseStringCompression && instruction->IsStringLength()) {
    last_visited_internal_latency_ = kArmMemoryLoadLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::VisitArraySet(HArraySet* instruction) {
  HInstruction* index = instruction->InputAt(1);
  DataType::Type value_type = instruction->GetComponentType();
  HInstruction* array_instr = instruction->GetArray();
  bool has_intermediate_address = array_instr->IsIntermediateAddress();

  switch (value_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryStoreLatency;
      } else {
        if (has_intermediate_address) {
        } else {
          last_visited_internal_latency_ = kArmIntegerOpLatency;
        }
        last_visited_latency_ = kArmMemoryStoreLatency;
      }
      break;
    }

    case DataType::Type::kReference: {
      if (instruction->InputAt(2)->IsNullConstant()) {
        if (index->IsConstant()) {
          last_visited_latency_ = kArmMemoryStoreLatency;
        } else {
          last_visited_internal_latency_ = kArmIntegerOpLatency;
          last_visited_latency_ = kArmMemoryStoreLatency;
        }
      } else {
        // Following the exact instructions of runtime type checks is too complicated,
        // just giving it a simple slow latency.
        last_visited_latency_ = kArmRuntimeTypeCheckLatency;
      }
      break;
    }

    case DataType::Type::kInt64: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    case DataType::Type::kFloat32: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    case DataType::Type::kFloat64: {
      if (index->IsConstant()) {
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_internal_latency_ = kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;
    }

    default:
      LOG(FATAL) << "Unreachable type " << value_type;
      UNREACHABLE();
  }
}

void SchedulingLatencyVisitorARM::VisitBoundsCheck(HBoundsCheck* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = kArmIntegerOpLatency;
  // Users do not use any data results.
  last_visited_latency_ = 0;
}

void SchedulingLatencyVisitorARM::HandleDivRemConstantIntegralLatencies(int32_t imm) {
  if (imm == 0) {
    last_visited_internal_latency_ = 0;
    last_visited_latency_ = 0;
  } else if (imm == 1 || imm == -1) {
    last_visited_latency_ = kArmIntegerOpLatency;
  } else if (IsPowerOfTwo(AbsOrMin(imm))) {
    last_visited_internal_latency_ = 3 * kArmIntegerOpLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  } else {
    last_visited_internal_latency_ = kArmMulIntegerLatency + 2 * kArmIntegerOpLatency;
    last_visited_latency_ = kArmIntegerOpLatency;
  }
}

void SchedulingLatencyVisitorARM::VisitDiv(HDiv* instruction) {
  DataType::Type type = instruction->GetResultType();
  switch (type) {
    case DataType::Type::kInt32: {
      HInstruction* rhs = instruction->GetRight();
      if (rhs->IsConstant()) {
        int32_t imm = Int32ConstantFrom(rhs->AsConstant());
        HandleDivRemConstantIntegralLatencies(imm);
      } else {
        last_visited_latency_ = kArmDivIntegerLatency;
      }
      break;
    }
    case DataType::Type::kFloat32:
      last_visited_latency_ = kArmDivFloatLatency;
      break;
    case DataType::Type::kFloat64:
      last_visited_latency_ = kArmDivDoubleLatency;
      break;
    default:
      last_visited_internal_latency_ = kArmCallInternalLatency;
      last_visited_latency_ = kArmCallLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitInstanceFieldGet(HInstanceFieldGet* instruction) {
  HandleFieldGetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitInstanceFieldSet(HInstanceFieldSet* instruction) {
  HandleFieldSetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitInstanceOf(HInstanceOf* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = kArmCallInternalLatency;
  last_visited_latency_ = kArmIntegerOpLatency;
}

void SchedulingLatencyVisitorARM::VisitInvoke(HInvoke* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = kArmCallInternalLatency;
  last_visited_latency_ = kArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitLoadString(HLoadString* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = kArmLoadStringInternalLatency;
  last_visited_latency_ = kArmMemoryLoadLatency;
}

void SchedulingLatencyVisitorARM::VisitNewArray(HNewArray* ATTRIBUTE_UNUSED) {
  last_visited_internal_latency_ = kArmIntegerOpLatency + kArmCallInternalLatency;
  last_visited_latency_ = kArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitNewInstance(HNewInstance* instruction) {
  if (instruction->IsStringAlloc()) {
    last_visited_internal_latency_ = 2 * kArmMemoryLoadLatency + kArmCallInternalLatency;
  } else {
    last_visited_internal_latency_ = kArmCallInternalLatency;
  }
  last_visited_latency_ = kArmCallLatency;
}

void SchedulingLatencyVisitorARM::VisitRem(HRem* instruction) {
  DataType::Type type = instruction->GetResultType();
  switch (type) {
    case DataType::Type::kInt32: {
      HInstruction* rhs = instruction->GetRight();
      if (rhs->IsConstant()) {
        int32_t imm = Int32ConstantFrom(rhs->AsConstant());
        HandleDivRemConstantIntegralLatencies(imm);
      } else {
        last_visited_internal_latency_ = kArmDivIntegerLatency;
        last_visited_latency_ = kArmMulIntegerLatency;
      }
      break;
    }
    default:
      last_visited_internal_latency_ = kArmCallInternalLatency;
      last_visited_latency_ = kArmCallLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::HandleFieldGetLatencies(HInstruction* instruction,
                                                          const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());
  DCHECK(codegen_ != nullptr);
  bool is_volatile = field_info.IsVolatile();
  DataType::Type field_type = field_info.GetFieldType();
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();

  switch (field_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
      last_visited_latency_ = kArmMemoryLoadLatency;
      break;

    case DataType::Type::kReference:
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        last_visited_internal_latency_ = kArmMemoryLoadLatency + kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;

    case DataType::Type::kInt64:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ = kArmMemoryLoadLatency + kArmIntegerOpLatency;
        last_visited_latency_ = kArmMemoryLoadLatency;
      } else {
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;

    case DataType::Type::kFloat32:
      last_visited_latency_ = kArmMemoryLoadLatency;
      break;

    case DataType::Type::kFloat64:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ =
            kArmMemoryLoadLatency + kArmIntegerOpLatency + kArmMemoryLoadLatency;
        last_visited_latency_ = kArmIntegerOpLatency;
      } else {
        last_visited_latency_ = kArmMemoryLoadLatency;
      }
      break;

    default:
      last_visited_latency_ = kArmMemoryLoadLatency;
      break;
  }

  if (is_volatile) {
    last_visited_internal_latency_ += kArmMemoryBarrierLatency;
  }
}

void SchedulingLatencyVisitorARM::HandleFieldSetLatencies(HInstruction* instruction,
                                                          const FieldInfo& field_info) {
  DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());
  DCHECK(codegen_ != nullptr);
  bool is_volatile = field_info.IsVolatile();
  DataType::Type field_type = field_info.GetFieldType();
  bool needs_write_barrier =
      CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1));
  bool atomic_ldrd_strd = codegen_->GetInstructionSetFeatures().HasAtomicLdrdAndStrd();

  switch (field_type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      if (is_volatile) {
        last_visited_internal_latency_ = kArmMemoryBarrierLatency + kArmMemoryStoreLatency;
        last_visited_latency_ = kArmMemoryBarrierLatency;
      } else {
        last_visited_latency_ = kArmMemoryStoreLatency;
      }
      break;

    case DataType::Type::kInt32:
    case DataType::Type::kReference:
      if (kPoisonHeapReferences && needs_write_barrier) {
        last_visited_internal_latency_ += kArmIntegerOpLatency * 2;
      }
      last_visited_latency_ = kArmMemoryStoreLatency;
      break;

    case DataType::Type::kInt64:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ =
            kArmIntegerOpLatency + kArmMemoryLoadLatency + kArmMemoryStoreLatency;
        last_visited_latency_ = kArmIntegerOpLatency;
      } else {
        last_visited_latency_ = kArmMemoryStoreLatency;
      }
      break;

    case DataType::Type::kFloat32:
      last_visited_latency_ = kArmMemoryStoreLatency;
      break;

    case DataType::Type::kFloat64:
      if (is_volatile && !atomic_ldrd_strd) {
        last_visited_internal_latency_ = kArmIntegerOpLatency +
            kArmIntegerOpLatency + kArmMemoryLoadLatency + kArmMemoryStoreLatency;
        last_visited_latency_ = kArmIntegerOpLatency;
      } else {
        last_visited_latency_ = kArmMemoryStoreLatency;
      }
      break;

    default:
      last_visited_latency_ = kArmMemoryStoreLatency;
      break;
  }
}

void SchedulingLatencyVisitorARM::VisitStaticFieldGet(HStaticFieldGet* instruction) {
  HandleFieldGetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitStaticFieldSet(HStaticFieldSet* instruction) {
  HandleFieldSetLatencies(instruction, instruction->GetFieldInfo());
}

void SchedulingLatencyVisitorARM::VisitSuspendCheck(HSuspendCheck* instruction) {
  HBasicBlock* block = instruction->GetBlock();
  DCHECK((block->GetLoopInformation() != nullptr) ||
         (block->IsEntryBlock() && instruction->GetNext()->IsGoto()));
  // Users do not use any data results.
  last_visited_latency_ = 0;
}

void SchedulingLatencyVisitorARM::VisitTypeConversion(HTypeConversion* instr) {
  DataType::Type result_type = instr->GetResultType();
  DataType::Type input_type = instr->GetInputType();

  switch (result_type) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      last_visited_latency_ = kArmIntegerOpLatency;  // SBFX or UBFX
      break;

    case DataType::Type::kInt32:
      switch (input_type) {
        case DataType::Type::kInt64:
          last_visited_latency_ = kArmIntegerOpLatency;  // MOV
          break;
        case DataType::Type::kFloat32:
        case DataType::Type::kFloat64:
          last_visited_internal_latency_ = kArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = kArmIntegerOpLatency;
          break;
      }
      break;

    case DataType::Type::kInt64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          // MOV and extension
          last_visited_internal_latency_ = kArmIntegerOpLatency;
          last_visited_latency_ = kArmIntegerOpLatency;
          break;
        case DataType::Type::kFloat32:
        case DataType::Type::kFloat64:
          // invokes runtime
          last_visited_internal_latency_ = kArmCallInternalLatency;
          break;
        default:
          last_visited_internal_latency_ = kArmIntegerOpLatency;
          last_visited_latency_ = kArmIntegerOpLatency;
          break;
      }
      break;

    case DataType::Type::kFloat32:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          last_visited_internal_latency_ = kArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        case DataType::Type::kInt64:
          // invokes runtime
          last_visited_internal_latency_ = kArmCallInternalLatency;
          break;
        case DataType::Type::kFloat64:
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
      }
      break;

    case DataType::Type::kFloat64:
      switch (input_type) {
        case DataType::Type::kBool:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kUint16:
        case DataType::Type::kInt16:
        case DataType::Type::kInt32:
          last_visited_internal_latency_ = kArmTypeConversionFloatingPointIntegerLatency;
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        case DataType::Type::kInt64:
          last_visited_internal_latency_ = 5 * kArmFloatingPointOpLatency;
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        case DataType::Type::kFloat32:
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
        default:
          last_visited_latency_ = kArmFloatingPointOpLatency;
          break;
      }
      break;

    default:
      last_visited_latency_ = kArmTypeConversionFloatingPointIntegerLatency;
      break;
  }
}

}  // namespace arm
}  // namespace art
