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

// Note: this include order may seem strange and is against the regular style. However it is the
//       required order as nodes_shared does not have the right dependency chain and compilation
//       will fail (as AsType on HInstruction will be defined before the full Instruction).
#include "nodes.h"

#include "nodes_shared.h"

#include "common_arm64.h"

namespace art {

using helpers::CanFitInShifterOperand;

void HDataProcWithShifterOp::GetOpInfoFromInstruction(HInstruction* instruction,
                                                      /*out*/OpKind* op_kind,
                                                      /*out*/int* shift_amount) {
  DCHECK(CanFitInShifterOperand(instruction));
  if (instruction->IsShl()) {
    *op_kind = kLSL;
    *shift_amount = instruction->AsShl()->GetRight()->AsIntConstant()->GetValue();
  } else if (instruction->IsShr()) {
    *op_kind = kASR;
    *shift_amount = instruction->AsShr()->GetRight()->AsIntConstant()->GetValue();
  } else if (instruction->IsUShr()) {
    *op_kind = kLSR;
    *shift_amount = instruction->AsUShr()->GetRight()->AsIntConstant()->GetValue();
  } else {
    DCHECK(instruction->IsTypeConversion());
    DataType::Type result_type = instruction->AsTypeConversion()->GetResultType();
    DataType::Type input_type = instruction->AsTypeConversion()->GetInputType();
    int result_size = DataType::Size(result_type);
    int input_size = DataType::Size(input_type);
    int min_size = std::min(result_size, input_size);
    if (result_type == DataType::Type::kInt32 && input_type == DataType::Type::kInt64) {
      // There is actually nothing to do. On ARM the high register from the
      // pair will be ignored. On ARM64 the register will be used as a W
      // register, discarding the top bits. This is represented by the
      // default encoding 'LSL 0'.
      *op_kind = kLSL;
      *shift_amount = 0;
    } else if (result_type == DataType::Type::kUint8 ||
               (input_type == DataType::Type::kUint8 && input_size < result_size)) {
      *op_kind = kUXTB;
    } else if (result_type == DataType::Type::kUint16 ||
               (input_type == DataType::Type::kUint16 && input_size < result_size)) {
      *op_kind = kUXTH;
    } else {
      switch (min_size) {
        case 1: *op_kind = kSXTB; break;
        case 2: *op_kind = kSXTH; break;
        case 4: *op_kind = kSXTW; break;
        default:
          LOG(FATAL) << "Unexpected min size " << min_size;
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const HDataProcWithShifterOp::OpKind op) {
  switch (op) {
    case HDataProcWithShifterOp::kLSL:  return os << "LSL";
    case HDataProcWithShifterOp::kLSR:  return os << "LSR";
    case HDataProcWithShifterOp::kASR:  return os << "ASR";
    case HDataProcWithShifterOp::kUXTB: return os << "UXTB";
    case HDataProcWithShifterOp::kUXTH: return os << "UXTH";
    case HDataProcWithShifterOp::kUXTW: return os << "UXTW";
    case HDataProcWithShifterOp::kSXTB: return os << "SXTB";
    case HDataProcWithShifterOp::kSXTH: return os << "SXTH";
    case HDataProcWithShifterOp::kSXTW: return os << "SXTW";
    default:
      LOG(FATAL) << "Invalid OpKind " << static_cast<int>(op);
      UNREACHABLE();
  }
}

}  // namespace art
