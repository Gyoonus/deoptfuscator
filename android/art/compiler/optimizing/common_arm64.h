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

#ifndef ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_
#define ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_

#include "code_generator.h"
#include "instruction_simplifier_shared.h"
#include "locations.h"
#include "nodes.h"
#include "utils/arm64/assembler_arm64.h"

// TODO(VIXL): Make VIXL compile with -Wshadow.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "aarch64/disasm-aarch64.h"
#include "aarch64/macro-assembler-aarch64.h"
#include "aarch64/simulator-aarch64.h"
#pragma GCC diagnostic pop

namespace art {

using helpers::CanFitInShifterOperand;
using helpers::HasShifterOperand;

namespace arm64 {
namespace helpers {

// Convenience helpers to ease conversion to and from VIXL operands.
static_assert((SP == 31) && (WSP == 31) && (XZR == 32) && (WZR == 32),
              "Unexpected values for register codes.");

inline int VIXLRegCodeFromART(int code) {
  if (code == SP) {
    return vixl::aarch64::kSPRegInternalCode;
  }
  if (code == XZR) {
    return vixl::aarch64::kZeroRegCode;
  }
  return code;
}

inline int ARTRegCodeFromVIXL(int code) {
  if (code == vixl::aarch64::kSPRegInternalCode) {
    return SP;
  }
  if (code == vixl::aarch64::kZeroRegCode) {
    return XZR;
  }
  return code;
}

inline vixl::aarch64::Register XRegisterFrom(Location location) {
  DCHECK(location.IsRegister()) << location;
  return vixl::aarch64::Register::GetXRegFromCode(VIXLRegCodeFromART(location.reg()));
}

inline vixl::aarch64::Register WRegisterFrom(Location location) {
  DCHECK(location.IsRegister()) << location;
  return vixl::aarch64::Register::GetWRegFromCode(VIXLRegCodeFromART(location.reg()));
}

inline vixl::aarch64::Register RegisterFrom(Location location, DataType::Type type) {
  DCHECK(type != DataType::Type::kVoid && !DataType::IsFloatingPointType(type)) << type;
  return type == DataType::Type::kInt64 ? XRegisterFrom(location) : WRegisterFrom(location);
}

inline vixl::aarch64::Register OutputRegister(HInstruction* instr) {
  return RegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

inline vixl::aarch64::Register InputRegisterAt(HInstruction* instr, int input_index) {
  return RegisterFrom(instr->GetLocations()->InAt(input_index),
                      instr->InputAt(input_index)->GetType());
}

inline vixl::aarch64::FPRegister DRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return vixl::aarch64::FPRegister::GetDRegFromCode(location.reg());
}

inline vixl::aarch64::FPRegister QRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return vixl::aarch64::FPRegister::GetQRegFromCode(location.reg());
}

inline vixl::aarch64::FPRegister VRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return vixl::aarch64::FPRegister::GetVRegFromCode(location.reg());
}

inline vixl::aarch64::FPRegister SRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister()) << location;
  return vixl::aarch64::FPRegister::GetSRegFromCode(location.reg());
}

inline vixl::aarch64::FPRegister FPRegisterFrom(Location location, DataType::Type type) {
  DCHECK(DataType::IsFloatingPointType(type)) << type;
  return type == DataType::Type::kFloat64 ? DRegisterFrom(location) : SRegisterFrom(location);
}

inline vixl::aarch64::FPRegister OutputFPRegister(HInstruction* instr) {
  return FPRegisterFrom(instr->GetLocations()->Out(), instr->GetType());
}

inline vixl::aarch64::FPRegister InputFPRegisterAt(HInstruction* instr, int input_index) {
  return FPRegisterFrom(instr->GetLocations()->InAt(input_index),
                        instr->InputAt(input_index)->GetType());
}

inline vixl::aarch64::CPURegister CPURegisterFrom(Location location, DataType::Type type) {
  return DataType::IsFloatingPointType(type)
      ? vixl::aarch64::CPURegister(FPRegisterFrom(location, type))
      : vixl::aarch64::CPURegister(RegisterFrom(location, type));
}

inline vixl::aarch64::CPURegister OutputCPURegister(HInstruction* instr) {
  return DataType::IsFloatingPointType(instr->GetType())
      ? static_cast<vixl::aarch64::CPURegister>(OutputFPRegister(instr))
      : static_cast<vixl::aarch64::CPURegister>(OutputRegister(instr));
}

inline vixl::aarch64::CPURegister InputCPURegisterAt(HInstruction* instr, int index) {
  return DataType::IsFloatingPointType(instr->InputAt(index)->GetType())
      ? static_cast<vixl::aarch64::CPURegister>(InputFPRegisterAt(instr, index))
      : static_cast<vixl::aarch64::CPURegister>(InputRegisterAt(instr, index));
}

inline vixl::aarch64::CPURegister InputCPURegisterOrZeroRegAt(HInstruction* instr,
                                                                     int index) {
  HInstruction* input = instr->InputAt(index);
  DataType::Type input_type = input->GetType();
  if (input->IsConstant() && input->AsConstant()->IsZeroBitPattern()) {
    return (DataType::Size(input_type) >= vixl::aarch64::kXRegSizeInBytes)
        ? vixl::aarch64::Register(vixl::aarch64::xzr)
        : vixl::aarch64::Register(vixl::aarch64::wzr);
  }
  return InputCPURegisterAt(instr, index);
}

inline int64_t Int64ConstantFrom(Location location) {
  HConstant* instr = location.GetConstant();
  if (instr->IsIntConstant()) {
    return instr->AsIntConstant()->GetValue();
  } else if (instr->IsNullConstant()) {
    return 0;
  } else {
    DCHECK(instr->IsLongConstant()) << instr->DebugName();
    return instr->AsLongConstant()->GetValue();
  }
}

inline vixl::aarch64::Operand OperandFrom(Location location, DataType::Type type) {
  if (location.IsRegister()) {
    return vixl::aarch64::Operand(RegisterFrom(location, type));
  } else {
    return vixl::aarch64::Operand(Int64ConstantFrom(location));
  }
}

inline vixl::aarch64::Operand InputOperandAt(HInstruction* instr, int input_index) {
  return OperandFrom(instr->GetLocations()->InAt(input_index),
                     instr->InputAt(input_index)->GetType());
}

inline vixl::aarch64::MemOperand StackOperandFrom(Location location) {
  return vixl::aarch64::MemOperand(vixl::aarch64::sp, location.GetStackIndex());
}

inline vixl::aarch64::MemOperand HeapOperand(const vixl::aarch64::Register& base,
                                                    size_t offset = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return vixl::aarch64::MemOperand(base.X(), offset);
}

inline vixl::aarch64::MemOperand HeapOperand(const vixl::aarch64::Register& base,
                                                    const vixl::aarch64::Register& regoffset,
                                                    vixl::aarch64::Shift shift = vixl::aarch64::LSL,
                                                    unsigned shift_amount = 0) {
  // A heap reference must be 32bit, so fit in a W register.
  DCHECK(base.IsW());
  return vixl::aarch64::MemOperand(base.X(), regoffset, shift, shift_amount);
}

inline vixl::aarch64::MemOperand HeapOperand(const vixl::aarch64::Register& base,
                                                    Offset offset) {
  return HeapOperand(base, offset.SizeValue());
}

inline vixl::aarch64::MemOperand HeapOperandFrom(Location location, Offset offset) {
  return HeapOperand(RegisterFrom(location, DataType::Type::kReference), offset);
}

inline Location LocationFrom(const vixl::aarch64::Register& reg) {
  return Location::RegisterLocation(ARTRegCodeFromVIXL(reg.GetCode()));
}

inline Location LocationFrom(const vixl::aarch64::FPRegister& fpreg) {
  return Location::FpuRegisterLocation(fpreg.GetCode());
}

inline vixl::aarch64::Operand OperandFromMemOperand(
    const vixl::aarch64::MemOperand& mem_op) {
  if (mem_op.IsImmediateOffset()) {
    return vixl::aarch64::Operand(mem_op.GetOffset());
  } else {
    DCHECK(mem_op.IsRegisterOffset());
    if (mem_op.GetExtend() != vixl::aarch64::NO_EXTEND) {
      return vixl::aarch64::Operand(mem_op.GetRegisterOffset(),
                                    mem_op.GetExtend(),
                                    mem_op.GetShiftAmount());
    } else if (mem_op.GetShift() != vixl::aarch64::NO_SHIFT) {
      return vixl::aarch64::Operand(mem_op.GetRegisterOffset(),
                                    mem_op.GetShift(),
                                    mem_op.GetShiftAmount());
    } else {
      LOG(FATAL) << "Should not reach here";
      UNREACHABLE();
    }
  }
}

inline bool Arm64CanEncodeConstantAsImmediate(HConstant* constant, HInstruction* instr) {
  int64_t value = CodeGenerator::GetInt64ValueOf(constant);

  // TODO: Improve this when IsSIMDConstantEncodable method is implemented in VIXL.
  if (instr->IsVecReplicateScalar()) {
    if (constant->IsLongConstant()) {
      return false;
    } else if (constant->IsFloatConstant()) {
      return vixl::aarch64::Assembler::IsImmFP32(constant->AsFloatConstant()->GetValue());
    } else if (constant->IsDoubleConstant()) {
      return vixl::aarch64::Assembler::IsImmFP64(constant->AsDoubleConstant()->GetValue());
    }
    return IsUint<8>(value);
  }

  // For single uses we let VIXL handle the constant generation since it will
  // use registers that are not managed by the register allocator (wip0, wip1).
  if (constant->GetUses().HasExactlyOneElement()) {
    return true;
  }

  // Our code generator ensures shift distances are within an encodable range.
  if (instr->IsRor()) {
    return true;
  }

  if (instr->IsAnd() || instr->IsOr() || instr->IsXor()) {
    // Uses logical operations.
    return vixl::aarch64::Assembler::IsImmLogical(value, vixl::aarch64::kXRegSize);
  } else if (instr->IsNeg()) {
    // Uses mov -immediate.
    return vixl::aarch64::Assembler::IsImmMovn(value, vixl::aarch64::kXRegSize);
  } else {
    DCHECK(instr->IsAdd() ||
           instr->IsIntermediateAddress() ||
           instr->IsBoundsCheck() ||
           instr->IsCompare() ||
           instr->IsCondition() ||
           instr->IsSub())
        << instr->DebugName();
    // Uses aliases of ADD/SUB instructions.
    // If `value` does not fit but `-value` does, VIXL will automatically use
    // the 'opposite' instruction.
    return vixl::aarch64::Assembler::IsImmAddSub(value)
        || vixl::aarch64::Assembler::IsImmAddSub(-value);
  }
}

inline Location ARM64EncodableConstantOrRegister(HInstruction* constant,
                                                        HInstruction* instr) {
  if (constant->IsConstant()
      && Arm64CanEncodeConstantAsImmediate(constant->AsConstant(), instr)) {
    return Location::ConstantLocation(constant->AsConstant());
  }

  return Location::RequiresRegister();
}

// Check if registers in art register set have the same register code in vixl. If the register
// codes are same, we can initialize vixl register list simply by the register masks. Currently,
// only SP/WSP and ZXR/WZR codes are different between art and vixl.
// Note: This function is only used for debug checks.
inline bool ArtVixlRegCodeCoherentForRegSet(uint32_t art_core_registers,
                                            size_t num_core,
                                            uint32_t art_fpu_registers,
                                            size_t num_fpu) {
  // The register masks won't work if the number of register is larger than 32.
  DCHECK_GE(sizeof(art_core_registers) * 8, num_core);
  DCHECK_GE(sizeof(art_fpu_registers) * 8, num_fpu);
  for (size_t art_reg_code = 0;  art_reg_code < num_core; ++art_reg_code) {
    if (RegisterSet::Contains(art_core_registers, art_reg_code)) {
      if (art_reg_code != static_cast<size_t>(VIXLRegCodeFromART(art_reg_code))) {
        return false;
      }
    }
  }
  // There is no register code translation for float registers.
  return true;
}

inline vixl::aarch64::Shift ShiftFromOpKind(HDataProcWithShifterOp::OpKind op_kind) {
  switch (op_kind) {
    case HDataProcWithShifterOp::kASR: return vixl::aarch64::ASR;
    case HDataProcWithShifterOp::kLSL: return vixl::aarch64::LSL;
    case HDataProcWithShifterOp::kLSR: return vixl::aarch64::LSR;
    default:
      LOG(FATAL) << "Unexpected op kind " << op_kind;
      UNREACHABLE();
      return vixl::aarch64::NO_SHIFT;
  }
}

inline vixl::aarch64::Extend ExtendFromOpKind(HDataProcWithShifterOp::OpKind op_kind) {
  switch (op_kind) {
    case HDataProcWithShifterOp::kUXTB: return vixl::aarch64::UXTB;
    case HDataProcWithShifterOp::kUXTH: return vixl::aarch64::UXTH;
    case HDataProcWithShifterOp::kUXTW: return vixl::aarch64::UXTW;
    case HDataProcWithShifterOp::kSXTB: return vixl::aarch64::SXTB;
    case HDataProcWithShifterOp::kSXTH: return vixl::aarch64::SXTH;
    case HDataProcWithShifterOp::kSXTW: return vixl::aarch64::SXTW;
    default:
      LOG(FATAL) << "Unexpected op kind " << op_kind;
      UNREACHABLE();
      return vixl::aarch64::NO_EXTEND;
  }
}

inline bool ShifterOperandSupportsExtension(HInstruction* instruction) {
  DCHECK(HasShifterOperand(instruction, InstructionSet::kArm64));
  // Although the `neg` instruction is an alias of the `sub` instruction, `HNeg`
  // does *not* support extension. This is because the `extended register` form
  // of the `sub` instruction interprets the left register with code 31 as the
  // stack pointer and not the zero register. (So does the `immediate` form.) In
  // the other form `shifted register, the register with code 31 is interpreted
  // as the zero register.
  return instruction->IsAdd() || instruction->IsSub();
}

inline bool IsConstantZeroBitPattern(const HInstruction* instruction) {
  return instruction->IsConstant() && instruction->AsConstant()->IsZeroBitPattern();
}

}  // namespace helpers
}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_COMMON_ARM64_H_
