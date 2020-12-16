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

#include "code_generator_arm64.h"

#include "mirror/array-inl.h"
#include "mirror/string.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

namespace art {
namespace arm64 {

using helpers::ARM64EncodableConstantOrRegister;
using helpers::Arm64CanEncodeConstantAsImmediate;
using helpers::DRegisterFrom;
using helpers::HeapOperand;
using helpers::InputRegisterAt;
using helpers::Int64ConstantFrom;
using helpers::OutputRegister;
using helpers::VRegisterFrom;
using helpers::WRegisterFrom;
using helpers::XRegisterFrom;

#define __ GetVIXLAssembler()->

void LocationsBuilderARM64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  HInstruction* input = instruction->InputAt(0);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, ARM64EncodableConstantOrRegister(input, instruction));
      locations->SetOut(Location::RequiresFpuRegister());
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      if (input->IsConstant() &&
          Arm64CanEncodeConstantAsImmediate(input->AsConstant(), instruction)) {
        locations->SetInAt(0, Location::ConstantLocation(input->AsConstant()));
        locations->SetOut(Location::RequiresFpuRegister());
      } else {
        locations->SetInAt(0, Location::RequiresFpuRegister());
        locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void InstructionCodeGeneratorARM64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  Location src_loc = locations->InAt(0);
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Movi(dst.V16B(), Int64ConstantFrom(src_loc));
      } else {
        __ Dup(dst.V16B(), InputRegisterAt(instruction, 0));
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Movi(dst.V8H(), Int64ConstantFrom(src_loc));
      } else {
        __ Dup(dst.V8H(), InputRegisterAt(instruction, 0));
      }
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Movi(dst.V4S(), Int64ConstantFrom(src_loc));
      } else {
        __ Dup(dst.V4S(), InputRegisterAt(instruction, 0));
      }
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Movi(dst.V2D(), Int64ConstantFrom(src_loc));
      } else {
        __ Dup(dst.V2D(), XRegisterFrom(src_loc));
      }
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Fmov(dst.V4S(), src_loc.GetConstant()->AsFloatConstant()->GetValue());
      } else {
        __ Dup(dst.V4S(), VRegisterFrom(src_loc).V4S(), 0);
      }
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      if (src_loc.IsConstant()) {
        __ Fmov(dst.V2D(), src_loc.GetConstant()->AsDoubleConstant()->GetValue());
      } else {
        __ Dup(dst.V2D(), VRegisterFrom(src_loc).V2D(), 0);
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresRegister());
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::SameAsFirstInput());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void InstructionCodeGeneratorARM64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Umov(OutputRegister(instruction), src.V4S(), 0);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Umov(OutputRegister(instruction), src.V2D(), 0);
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 4u);
      DCHECK(locations->InAt(0).Equals(locations->Out()));  // no code required
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

// Helper to set up locations for vector unary operations.
static void CreateVecUnOpLocations(ArenaAllocator* allocator, HVecUnaryOperation* instruction) {
  LocationSummary* locations = new (allocator) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(),
                        instruction->IsVecNot() ? Location::kOutputOverlap
                                                : Location::kNoOutputOverlap);
      break;
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecReduce(HVecReduce* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecReduce(HVecReduce* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  VRegister dst = DRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      switch (instruction->GetKind()) {
        case HVecReduce::kSum:
          __ Addv(dst.S(), src.V4S());
          break;
        case HVecReduce::kMin:
          __ Sminv(dst.S(), src.V4S());
          break;
        case HVecReduce::kMax:
          __ Smaxv(dst.S(), src.V4S());
          break;
      }
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      switch (instruction->GetKind()) {
        case HVecReduce::kSum:
          __ Addp(dst.D(), src.V2D());
          break;
        default:
          LOG(FATAL) << "Unsupported SIMD min/max";
          UNREACHABLE();
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecCnv(HVecCnv* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecCnv(HVecCnv* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  DataType::Type from = instruction->GetInputType();
  DataType::Type to = instruction->GetResultType();
  if (from == DataType::Type::kInt32 && to == DataType::Type::kFloat32) {
    DCHECK_EQ(4u, instruction->GetVectorLength());
    __ Scvtf(dst.V4S(), src.V4S());
  } else {
    LOG(FATAL) << "Unsupported SIMD type";
  }
}

void LocationsBuilderARM64::VisitVecNeg(HVecNeg* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecNeg(HVecNeg* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Neg(dst.V16B(), src.V16B());
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Neg(dst.V8H(), src.V8H());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Neg(dst.V4S(), src.V4S());
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Neg(dst.V2D(), src.V2D());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fneg(dst.V4S(), src.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fneg(dst.V2D(), src.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecAbs(HVecAbs* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecAbs(HVecAbs* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Abs(dst.V16B(), src.V16B());
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Abs(dst.V8H(), src.V8H());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Abs(dst.V4S(), src.V4S());
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Abs(dst.V2D(), src.V2D());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fabs(dst.V4S(), src.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fabs(dst.V2D(), src.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecNot(HVecNot* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecNot(HVecNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister src = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:  // special case boolean-not
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Movi(dst.V16B(), 1);
      __ Eor(dst.V16B(), dst.V16B(), src.V16B());
      break;
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      __ Not(dst.V16B(), src.V16B());  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

// Helper to set up locations for vector binary operations.
static void CreateVecBinOpLocations(ArenaAllocator* allocator, HVecBinaryOperation* instruction) {
  LocationSummary* locations = new (allocator) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecAdd(HVecAdd* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecAdd(HVecAdd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Add(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Add(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Add(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Add(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fadd(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fadd(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Urhadd(dst.V16B(), lhs.V16B(), rhs.V16B())
          : __ Uhadd(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Srhadd(dst.V16B(), lhs.V16B(), rhs.V16B())
          : __ Shadd(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Urhadd(dst.V8H(), lhs.V8H(), rhs.V8H())
          : __ Uhadd(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Srhadd(dst.V8H(), lhs.V8H(), rhs.V8H())
          : __ Shadd(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecSub(HVecSub* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecSub(HVecSub* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Sub(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Sub(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Sub(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Sub(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fsub(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fsub(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecMul(HVecMul* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecMul(HVecMul* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Mul(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Mul(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Mul(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fmul(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fmul(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecDiv(HVecDiv* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecDiv(HVecDiv* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fdiv(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fdiv(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecMin(HVecMin* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecMin(HVecMin* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Umin(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Smin(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Umin(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Smin(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kUint32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Umin(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Smin(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fmin(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fmin(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecMax(HVecMax* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecMax(HVecMax* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Umax(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Smax(dst.V16B(), lhs.V16B(), rhs.V16B());
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Umax(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Smax(dst.V8H(), lhs.V8H(), rhs.V8H());
      break;
    case DataType::Type::kUint32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Umax(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Smax(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Fmax(dst.V4S(), lhs.V4S(), rhs.V4S());
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Fmax(dst.V2D(), lhs.V2D(), rhs.V2D());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecAnd(HVecAnd* instruction) {
  // TODO: Allow constants supported by BIC (vector, immediate).
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecAnd(HVecAnd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ And(dst.V16B(), lhs.V16B(), rhs.V16B());  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecAndNot(HVecAndNot* instruction) {
  LOG(FATAL) << "Unsupported SIMD instruction " << instruction->GetId();
}

void InstructionCodeGeneratorARM64::VisitVecAndNot(HVecAndNot* instruction) {
  // TODO: Use BIC (vector, register).
  LOG(FATAL) << "Unsupported SIMD instruction " << instruction->GetId();
}

void LocationsBuilderARM64::VisitVecOr(HVecOr* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecOr(HVecOr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Orr(dst.V16B(), lhs.V16B(), rhs.V16B());  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecXor(HVecXor* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecXor(HVecXor* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister rhs = VRegisterFrom(locations->InAt(1));
  VRegister dst = VRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      __ Eor(dst.V16B(), lhs.V16B(), rhs.V16B());  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

// Helper to set up locations for vector shift operations.
static void CreateVecShiftLocations(ArenaAllocator* allocator, HVecBinaryOperation* instruction) {
  LocationSummary* locations = new (allocator) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::ConstantLocation(instruction->InputAt(1)->AsConstant()));
      locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecShl(HVecShl* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecShl(HVecShl* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Shl(dst.V16B(), lhs.V16B(), value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Shl(dst.V8H(), lhs.V8H(), value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Shl(dst.V4S(), lhs.V4S(), value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Shl(dst.V2D(), lhs.V2D(), value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecShr(HVecShr* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecShr(HVecShr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Sshr(dst.V16B(), lhs.V16B(), value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Sshr(dst.V8H(), lhs.V8H(), value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Sshr(dst.V4S(), lhs.V4S(), value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Sshr(dst.V2D(), lhs.V2D(), value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecUShr(HVecUShr* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorARM64::VisitVecUShr(HVecUShr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister lhs = VRegisterFrom(locations->InAt(0));
  VRegister dst = VRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Ushr(dst.V16B(), lhs.V16B(), value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Ushr(dst.V8H(), lhs.V8H(), value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Ushr(dst.V4S(), lhs.V4S(), value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Ushr(dst.V2D(), lhs.V2D(), value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecSetScalars(HVecSetScalars* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);

  DCHECK_EQ(1u, instruction->InputCount());  // only one input currently implemented

  HInstruction* input = instruction->InputAt(0);
  bool is_zero = IsZeroBitPattern(input);

  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, is_zero ? Location::ConstantLocation(input->AsConstant())
                                    : Location::RequiresRegister());
      locations->SetOut(Location::RequiresFpuRegister());
      break;
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, is_zero ? Location::ConstantLocation(input->AsConstant())
                                    : Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresFpuRegister());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void InstructionCodeGeneratorARM64::VisitVecSetScalars(HVecSetScalars* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister dst = VRegisterFrom(locations->Out());

  DCHECK_EQ(1u, instruction->InputCount());  // only one input currently implemented

  // Zero out all other elements first.
  __ Movi(dst.V16B(), 0);

  // Shorthand for any type of zero.
  if (IsZeroBitPattern(instruction->InputAt(0))) {
    return;
  }

  // Set required elements.
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Mov(dst.V16B(), 0, InputRegisterAt(instruction, 0));
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Mov(dst.V8H(), 0, InputRegisterAt(instruction, 0));
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Mov(dst.V4S(), 0, InputRegisterAt(instruction, 0));
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Mov(dst.V2D(), 0, InputRegisterAt(instruction, 0));
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

// Helper to set up locations for vector accumulations.
static void CreateVecAccumLocations(ArenaAllocator* allocator, HVecOperation* instruction) {
  LocationSummary* locations = new (allocator) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresFpuRegister());
      locations->SetInAt(1, Location::RequiresFpuRegister());
      locations->SetInAt(2, Location::RequiresFpuRegister());
      locations->SetOut(Location::SameAsFirstInput());
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecMultiplyAccumulate(HVecMultiplyAccumulate* instruction) {
  CreateVecAccumLocations(GetGraph()->GetAllocator(), instruction);
}

// Some early revisions of the Cortex-A53 have an erratum (835769) whereby it is possible for a
// 64-bit scalar multiply-accumulate instruction in AArch64 state to generate an incorrect result.
// However vector MultiplyAccumulate instruction is not affected.
void InstructionCodeGeneratorARM64::VisitVecMultiplyAccumulate(HVecMultiplyAccumulate* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister acc = VRegisterFrom(locations->InAt(0));
  VRegister left = VRegisterFrom(locations->InAt(1));
  VRegister right = VRegisterFrom(locations->InAt(2));

  DCHECK(locations->InAt(0).Equals(locations->Out()));

  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ Mla(acc.V16B(), left.V16B(), right.V16B());
      } else {
        __ Mls(acc.V16B(), left.V16B(), right.V16B());
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ Mla(acc.V8H(), left.V8H(), right.V8H());
      } else {
        __ Mls(acc.V8H(), left.V8H(), right.V8H());
      }
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ Mla(acc.V4S(), left.V4S(), right.V4S());
      } else {
        __ Mls(acc.V4S(), left.V4S(), right.V4S());
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  CreateVecAccumLocations(GetGraph()->GetAllocator(), instruction);
  // Some conversions require temporary registers.
  LocationSummary* locations = instruction->GetLocations();
  HVecOperation* a = instruction->InputAt(1)->AsVecOperation();
  HVecOperation* b = instruction->InputAt(2)->AsVecOperation();
  DCHECK_EQ(HVecOperation::ToSignedType(a->GetPackedType()),
            HVecOperation::ToSignedType(b->GetPackedType()));
  switch (a->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt64:
          locations->AddTemp(Location::RequiresFpuRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          FALLTHROUGH_INTENDED;
        case DataType::Type::kInt32:
          locations->AddTemp(Location::RequiresFpuRegister());
          locations->AddTemp(Location::RequiresFpuRegister());
          break;
        default:
          break;
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      if (instruction->GetPackedType() == DataType::Type::kInt64) {
        locations->AddTemp(Location::RequiresFpuRegister());
        locations->AddTemp(Location::RequiresFpuRegister());
      }
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      if (instruction->GetPackedType() == a->GetPackedType()) {
        locations->AddTemp(Location::RequiresFpuRegister());
      }
      break;
    default:
      break;
  }
}

void InstructionCodeGeneratorARM64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VRegister acc = VRegisterFrom(locations->InAt(0));
  VRegister left = VRegisterFrom(locations->InAt(1));
  VRegister right = VRegisterFrom(locations->InAt(2));

  DCHECK(locations->InAt(0).Equals(locations->Out()));

  // Handle all feasible acc_T += sad(a_S, b_S) type combinations (T x S).
  HVecOperation* a = instruction->InputAt(1)->AsVecOperation();
  HVecOperation* b = instruction->InputAt(2)->AsVecOperation();
  DCHECK_EQ(HVecOperation::ToSignedType(a->GetPackedType()),
            HVecOperation::ToSignedType(b->GetPackedType()));
  switch (a->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, a->GetVectorLength());
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt16:
          DCHECK_EQ(8u, instruction->GetVectorLength());
          __ Sabal(acc.V8H(), left.V8B(), right.V8B());
          __ Sabal2(acc.V8H(), left.V16B(), right.V16B());
          break;
        case DataType::Type::kInt32: {
          DCHECK_EQ(4u, instruction->GetVectorLength());
          VRegister tmp1 = VRegisterFrom(locations->GetTemp(0));
          VRegister tmp2 = VRegisterFrom(locations->GetTemp(1));
          __ Sxtl(tmp1.V8H(), left.V8B());
          __ Sxtl(tmp2.V8H(), right.V8B());
          __ Sabal(acc.V4S(), tmp1.V4H(), tmp2.V4H());
          __ Sabal2(acc.V4S(), tmp1.V8H(), tmp2.V8H());
          __ Sxtl2(tmp1.V8H(), left.V16B());
          __ Sxtl2(tmp2.V8H(), right.V16B());
          __ Sabal(acc.V4S(), tmp1.V4H(), tmp2.V4H());
          __ Sabal2(acc.V4S(), tmp1.V8H(), tmp2.V8H());
          break;
        }
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VRegister tmp1 = VRegisterFrom(locations->GetTemp(0));
          VRegister tmp2 = VRegisterFrom(locations->GetTemp(1));
          VRegister tmp3 = VRegisterFrom(locations->GetTemp(2));
          VRegister tmp4 = VRegisterFrom(locations->GetTemp(3));
          __ Sxtl(tmp1.V8H(), left.V8B());
          __ Sxtl(tmp2.V8H(), right.V8B());
          __ Sxtl(tmp3.V4S(), tmp1.V4H());
          __ Sxtl(tmp4.V4S(), tmp2.V4H());
          __ Sabal(acc.V2D(), tmp3.V2S(), tmp4.V2S());
          __ Sabal2(acc.V2D(), tmp3.V4S(), tmp4.V4S());
          __ Sxtl2(tmp3.V4S(), tmp1.V8H());
          __ Sxtl2(tmp4.V4S(), tmp2.V8H());
          __ Sabal(acc.V2D(), tmp3.V2S(), tmp4.V2S());
          __ Sabal2(acc.V2D(), tmp3.V4S(), tmp4.V4S());
          __ Sxtl2(tmp1.V8H(), left.V16B());
          __ Sxtl2(tmp2.V8H(), right.V16B());
          __ Sxtl(tmp3.V4S(), tmp1.V4H());
          __ Sxtl(tmp4.V4S(), tmp2.V4H());
          __ Sabal(acc.V2D(), tmp3.V2S(), tmp4.V2S());
          __ Sabal2(acc.V2D(), tmp3.V4S(), tmp4.V4S());
          __ Sxtl2(tmp3.V4S(), tmp1.V8H());
          __ Sxtl2(tmp4.V4S(), tmp2.V8H());
          __ Sabal(acc.V2D(), tmp3.V2S(), tmp4.V2S());
          __ Sabal2(acc.V2D(), tmp3.V4S(), tmp4.V4S());
          break;
        }
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, a->GetVectorLength());
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt32:
          DCHECK_EQ(4u, instruction->GetVectorLength());
          __ Sabal(acc.V4S(), left.V4H(), right.V4H());
          __ Sabal2(acc.V4S(), left.V8H(), right.V8H());
          break;
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VRegister tmp1 = VRegisterFrom(locations->GetTemp(0));
          VRegister tmp2 = VRegisterFrom(locations->GetTemp(1));
          __ Sxtl(tmp1.V4S(), left.V4H());
          __ Sxtl(tmp2.V4S(), right.V4H());
          __ Sabal(acc.V2D(), tmp1.V2S(), tmp2.V2S());
          __ Sabal2(acc.V2D(), tmp1.V4S(), tmp2.V4S());
          __ Sxtl2(tmp1.V4S(), left.V8H());
          __ Sxtl2(tmp2.V4S(), right.V8H());
          __ Sabal(acc.V2D(), tmp1.V2S(), tmp2.V2S());
          __ Sabal2(acc.V2D(), tmp1.V4S(), tmp2.V4S());
          break;
        }
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, a->GetVectorLength());
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt32: {
          DCHECK_EQ(4u, instruction->GetVectorLength());
          VRegister tmp = VRegisterFrom(locations->GetTemp(0));
          __ Sub(tmp.V4S(), left.V4S(), right.V4S());
          __ Abs(tmp.V4S(), tmp.V4S());
          __ Add(acc.V4S(), acc.V4S(), tmp.V4S());
          break;
        }
        case DataType::Type::kInt64:
          DCHECK_EQ(2u, instruction->GetVectorLength());
          __ Sabal(acc.V2D(), left.V2S(), right.V2S());
          __ Sabal2(acc.V2D(), left.V4S(), right.V4S());
          break;
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, a->GetVectorLength());
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VRegister tmp = VRegisterFrom(locations->GetTemp(0));
          __ Sub(tmp.V2D(), left.V2D(), right.V2D());
          __ Abs(tmp.V2D(), tmp.V2D());
          __ Add(acc.V2D(), acc.V2D(), tmp.V2D());
          break;
        }
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
  }
}

// Helper to set up locations for vector memory operations.
static void CreateVecMemLocations(ArenaAllocator* allocator,
                                  HVecMemoryOperation* instruction,
                                  bool is_load) {
  LocationSummary* locations = new (allocator) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetInAt(1, Location::RegisterOrConstant(instruction->InputAt(1)));
      if (is_load) {
        locations->SetOut(Location::RequiresFpuRegister());
      } else {
        locations->SetInAt(2, Location::RequiresFpuRegister());
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

// Helper to set up locations for vector memory operations. Returns the memory operand and,
// if used, sets the output parameter scratch to a temporary register used in this operand,
// so that the client can release it right after the memory operand use.
MemOperand InstructionCodeGeneratorARM64::VecAddress(
    HVecMemoryOperation* instruction,
    UseScratchRegisterScope* temps_scope,
    size_t size,
    bool is_string_char_at,
    /*out*/ Register* scratch) {
  LocationSummary* locations = instruction->GetLocations();
  Register base = InputRegisterAt(instruction, 0);

  if (instruction->InputAt(1)->IsIntermediateAddressIndex()) {
    DCHECK(!is_string_char_at);
    return MemOperand(base.X(), InputRegisterAt(instruction, 1).X());
  }

  Location index = locations->InAt(1);
  uint32_t offset = is_string_char_at
      ? mirror::String::ValueOffset().Uint32Value()
      : mirror::Array::DataOffset(size).Uint32Value();
  size_t shift = ComponentSizeShiftWidth(size);

  // HIntermediateAddress optimization is only applied for scalar ArrayGet and ArraySet.
  DCHECK(!instruction->InputAt(0)->IsIntermediateAddress());

  if (index.IsConstant()) {
    offset += Int64ConstantFrom(index) << shift;
    return HeapOperand(base, offset);
  } else {
    *scratch = temps_scope->AcquireSameSizeAs(base);
    __ Add(*scratch, base, Operand(WRegisterFrom(index), LSL, shift));
    return HeapOperand(*scratch, offset);
  }
}

void LocationsBuilderARM64::VisitVecLoad(HVecLoad* instruction) {
  CreateVecMemLocations(GetGraph()->GetAllocator(), instruction, /*is_load*/ true);
}

void InstructionCodeGeneratorARM64::VisitVecLoad(HVecLoad* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t size = DataType::Size(instruction->GetPackedType());
  VRegister reg = VRegisterFrom(locations->Out());
  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register scratch;

  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt16:  // (short) s.charAt(.) can yield HVecLoad/Int16/StringCharAt.
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      // Special handling of compressed/uncompressed string load.
      if (mirror::kUseStringCompression && instruction->IsStringCharAt()) {
        vixl::aarch64::Label uncompressed_load, done;
        // Test compression bit.
        static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                      "Expecting 0=compressed, 1=uncompressed");
        uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
        Register length = temps.AcquireW();
        __ Ldr(length, HeapOperand(InputRegisterAt(instruction, 0), count_offset));
        __ Tbnz(length.W(), 0, &uncompressed_load);
        temps.Release(length);  // no longer needed
        // Zero extend 8 compressed bytes into 8 chars.
        __ Ldr(DRegisterFrom(locations->Out()).V8B(),
               VecAddress(instruction, &temps, 1, /*is_string_char_at*/ true, &scratch));
        __ Uxtl(reg.V8H(), reg.V8B());
        __ B(&done);
        if (scratch.IsValid()) {
          temps.Release(scratch);  // if used, no longer needed
        }
        // Load 8 direct uncompressed chars.
        __ Bind(&uncompressed_load);
        __ Ldr(reg, VecAddress(instruction, &temps, size, /*is_string_char_at*/ true, &scratch));
        __ Bind(&done);
        return;
      }
      FALLTHROUGH_INTENDED;
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ Ldr(reg, VecAddress(instruction, &temps, size, instruction->IsStringCharAt(), &scratch));
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderARM64::VisitVecStore(HVecStore* instruction) {
  CreateVecMemLocations(GetGraph()->GetAllocator(), instruction, /*is_load*/ false);
}

void InstructionCodeGeneratorARM64::VisitVecStore(HVecStore* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t size = DataType::Size(instruction->GetPackedType());
  VRegister reg = VRegisterFrom(locations->InAt(2));
  UseScratchRegisterScope temps(GetVIXLAssembler());
  Register scratch;

  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ Str(reg, VecAddress(instruction, &temps, size, /*is_string_char_at*/ false, &scratch));
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

#undef __

}  // namespace arm64
}  // namespace art
