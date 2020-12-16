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

#include "code_generator_mips64.h"
#include "mirror/array-inl.h"

namespace art {
namespace mips64 {

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<Mips64Assembler*>(GetAssembler())->  // NOLINT

VectorRegister VectorRegisterFrom(Location location) {
  DCHECK(location.IsFpuRegister());
  return static_cast<VectorRegister>(location.AsFpuRegister<FpuRegister>());
}

void LocationsBuilderMIPS64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  LocationSummary* locations = new (GetGraph()->GetAllocator()) LocationSummary(instruction);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      locations->SetInAt(0, Location::RequiresRegister());
      locations->SetOut(Location::RequiresFpuRegister());
      break;
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

void InstructionCodeGeneratorMIPS64::VisitVecReplicateScalar(HVecReplicateScalar* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ FillB(dst, locations->InAt(0).AsRegister<GpuRegister>());
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ FillH(dst, locations->InAt(0).AsRegister<GpuRegister>());
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FillW(dst, locations->InAt(0).AsRegister<GpuRegister>());
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FillD(dst, locations->InAt(0).AsRegister<GpuRegister>());
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ ReplicateFPToVectorRegister(dst,
                                     locations->InAt(0).AsFpuRegister<FpuRegister>(),
                                     /* is_double */ false);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ ReplicateFPToVectorRegister(dst,
                                     locations->InAt(0).AsFpuRegister<FpuRegister>(),
                                     /* is_double */ true);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
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

void InstructionCodeGeneratorMIPS64::VisitVecExtractScalar(HVecExtractScalar* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Copy_sW(locations->Out().AsRegister<GpuRegister>(), src, 0);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Copy_sD(locations->Out().AsRegister<GpuRegister>(), src, 0);
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
  DataType::Type type = instruction->GetPackedType();
  switch (type) {
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
      locations->SetOut(Location::RequiresFpuRegister(),
                        (instruction->IsVecNeg() || instruction->IsVecAbs() ||
                            (instruction->IsVecReduce() && type == DataType::Type::kInt64))
                            ? Location::kOutputOverlap
                            : Location::kNoOutputOverlap);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecReduce(HVecReduce* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecReduce(HVecReduce* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  VectorRegister tmp = static_cast<VectorRegister>(FTMP);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      switch (instruction->GetKind()) {
        case HVecReduce::kSum:
          __ Hadd_sD(tmp, src, src);
          __ IlvlD(dst, tmp, tmp);
          __ AddvW(dst, dst, tmp);
          break;
        case HVecReduce::kMin:
          __ IlvodW(tmp, src, src);
          __ Min_sW(tmp, src, tmp);
          __ IlvlW(dst, tmp, tmp);
          __ Min_sW(dst, dst, tmp);
          break;
        case HVecReduce::kMax:
          __ IlvodW(tmp, src, src);
          __ Max_sW(tmp, src, tmp);
          __ IlvlW(dst, tmp, tmp);
          __ Max_sW(dst, dst, tmp);
          break;
      }
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      switch (instruction->GetKind()) {
        case HVecReduce::kSum:
          __ IlvlD(dst, src, src);
          __ AddvD(dst, dst, src);
          break;
        case HVecReduce::kMin:
          __ IlvlD(dst, src, src);
          __ Min_sD(dst, dst, src);
          break;
        case HVecReduce::kMax:
          __ IlvlD(dst, src, src);
          __ Max_sD(dst, dst, src);
          break;
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecCnv(HVecCnv* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecCnv(HVecCnv* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  DataType::Type from = instruction->GetInputType();
  DataType::Type to = instruction->GetResultType();
  if (from == DataType::Type::kInt32 && to == DataType::Type::kFloat32) {
    DCHECK_EQ(4u, instruction->GetVectorLength());
    __ Ffint_sW(dst, src);
  } else {
    LOG(FATAL) << "Unsupported SIMD type";
    UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecNeg(HVecNeg* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecNeg(HVecNeg* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ FillB(dst, ZERO);
      __ SubvB(dst, dst, src);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ FillH(dst, ZERO);
      __ SubvH(dst, dst, src);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FillW(dst, ZERO);
      __ SubvW(dst, dst, src);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FillD(dst, ZERO);
      __ SubvD(dst, dst, src);
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FillW(dst, ZERO);
      __ FsubW(dst, dst, src);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FillD(dst, ZERO);
      __ FsubD(dst, dst, src);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecAbs(HVecAbs* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecAbs(HVecAbs* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ FillB(dst, ZERO);       // all zeroes
      __ Add_aB(dst, dst, src);  // dst = abs(0) + abs(src)
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ FillH(dst, ZERO);       // all zeroes
      __ Add_aH(dst, dst, src);  // dst = abs(0) + abs(src)
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FillW(dst, ZERO);       // all zeroes
      __ Add_aW(dst, dst, src);  // dst = abs(0) + abs(src)
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FillD(dst, ZERO);       // all zeroes
      __ Add_aD(dst, dst, src);  // dst = abs(0) + abs(src)
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ LdiW(dst, -1);          // all ones
      __ SrliW(dst, dst, 1);
      __ AndV(dst, dst, src);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ LdiD(dst, -1);          // all ones
      __ SrliD(dst, dst, 1);
      __ AndV(dst, dst, src);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecNot(HVecNot* instruction) {
  CreateVecUnOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecNot(HVecNot* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister src = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:  // special case boolean-not
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ LdiB(dst, 1);
      __ XorV(dst, dst, src);
      break;
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
    case DataType::Type::kFloat32:
    case DataType::Type::kFloat64:
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ NorV(dst, src, src);  // lanes do not matter
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

void LocationsBuilderMIPS64::VisitVecAdd(HVecAdd* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecAdd(HVecAdd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ AddvB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ AddvH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ AddvW(dst, lhs, rhs);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ AddvD(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FaddW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FaddD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecHalvingAdd(HVecHalvingAdd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Aver_uB(dst, lhs, rhs)
          : __ Ave_uB(dst, lhs, rhs);
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Aver_sB(dst, lhs, rhs)
          : __ Ave_sB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Aver_uH(dst, lhs, rhs)
          : __ Ave_uH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      instruction->IsRounded()
          ? __ Aver_sH(dst, lhs, rhs)
          : __ Ave_sH(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecSub(HVecSub* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecSub(HVecSub* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ SubvB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ SubvH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ SubvW(dst, lhs, rhs);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ SubvD(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FsubW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FsubD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecMul(HVecMul* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecMul(HVecMul* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ MulvB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ MulvH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ MulvW(dst, lhs, rhs);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ MulvD(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FmulW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FmulD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecDiv(HVecDiv* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecDiv(HVecDiv* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FdivW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FdivD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecMin(HVecMin* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecMin(HVecMin* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Min_uB(dst, lhs, rhs);
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Min_sB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Min_uH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Min_sH(dst, lhs, rhs);
      break;
    case DataType::Type::kUint32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Min_uW(dst, lhs, rhs);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Min_sW(dst, lhs, rhs);
      break;
    case DataType::Type::kUint64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Min_uD(dst, lhs, rhs);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Min_sD(dst, lhs, rhs);
      break;
    // When one of arguments is NaN, fmin.df returns other argument, but Java expects a NaN value.
    // TODO: Fix min(x, NaN) cases for float and double.
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FminW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FminD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecMax(HVecMax* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecMax(HVecMax* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Max_uB(dst, lhs, rhs);
      break;
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ Max_sB(dst, lhs, rhs);
      break;
    case DataType::Type::kUint16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Max_uH(dst, lhs, rhs);
      break;
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ Max_sH(dst, lhs, rhs);
      break;
    case DataType::Type::kUint32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Max_uW(dst, lhs, rhs);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ Max_sW(dst, lhs, rhs);
      break;
    case DataType::Type::kUint64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Max_uD(dst, lhs, rhs);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ Max_sD(dst, lhs, rhs);
      break;
    // When one of arguments is NaN, fmax.df returns other argument, but Java expects a NaN value.
    // TODO: Fix max(x, NaN) cases for float and double.
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ FmaxW(dst, lhs, rhs);
      break;
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ FmaxD(dst, lhs, rhs);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecAnd(HVecAnd* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecAnd(HVecAnd* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
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
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ AndV(dst, lhs, rhs);  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecAndNot(HVecAndNot* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecAndNot(HVecAndNot* instruction) {
  LOG(FATAL) << "No SIMD for " << instruction->GetId();
}

void LocationsBuilderMIPS64::VisitVecOr(HVecOr* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecOr(HVecOr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
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
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ OrV(dst, lhs, rhs);  // lanes do not matter
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecXor(HVecXor* instruction) {
  CreateVecBinOpLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecXor(HVecXor* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister rhs = VectorRegisterFrom(locations->InAt(1));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
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
      DCHECK_LE(2u, instruction->GetVectorLength());
      DCHECK_LE(instruction->GetVectorLength(), 16u);
      __ XorV(dst, lhs, rhs);  // lanes do not matter
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

void LocationsBuilderMIPS64::VisitVecShl(HVecShl* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecShl(HVecShl* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ SlliB(dst, lhs, value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ SlliH(dst, lhs, value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ SlliW(dst, lhs, value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ SlliD(dst, lhs, value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecShr(HVecShr* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecShr(HVecShr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ SraiB(dst, lhs, value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ SraiH(dst, lhs, value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ SraiW(dst, lhs, value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ SraiD(dst, lhs, value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecUShr(HVecUShr* instruction) {
  CreateVecShiftLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecUShr(HVecUShr* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister lhs = VectorRegisterFrom(locations->InAt(0));
  VectorRegister dst = VectorRegisterFrom(locations->Out());
  int32_t value = locations->InAt(1).GetConstant()->AsIntConstant()->GetValue();
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ SrliB(dst, lhs, value);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ SrliH(dst, lhs, value);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ SrliW(dst, lhs, value);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ SrliD(dst, lhs, value);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecSetScalars(HVecSetScalars* instruction) {
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

void InstructionCodeGeneratorMIPS64::VisitVecSetScalars(HVecSetScalars* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister dst = VectorRegisterFrom(locations->Out());

  DCHECK_EQ(1u, instruction->InputCount());  // only one input currently implemented

  // Zero out all other elements first.
  __ FillW(dst, ZERO);

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
      __ InsertB(dst, locations->InAt(0).AsRegister<GpuRegister>(), 0);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ InsertH(dst, locations->InAt(0).AsRegister<GpuRegister>(), 0);
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ InsertW(dst, locations->InAt(0).AsRegister<GpuRegister>(), 0);
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ InsertD(dst, locations->InAt(0).AsRegister<GpuRegister>(), 0);
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

void LocationsBuilderMIPS64::VisitVecMultiplyAccumulate(HVecMultiplyAccumulate* instruction) {
  CreateVecAccumLocations(GetGraph()->GetAllocator(), instruction);
}

void InstructionCodeGeneratorMIPS64::VisitVecMultiplyAccumulate(HVecMultiplyAccumulate* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister acc = VectorRegisterFrom(locations->InAt(0));
  VectorRegister left = VectorRegisterFrom(locations->InAt(1));
  VectorRegister right = VectorRegisterFrom(locations->InAt(2));
  switch (instruction->GetPackedType()) {
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ MaddvB(acc, left, right);
      } else {
        __ MsubvB(acc, left, right);
      }
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ MaddvH(acc, left, right);
      } else {
        __ MsubvH(acc, left, right);
      }
      break;
    case DataType::Type::kInt32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ MaddvW(acc, left, right);
      } else {
        __ MsubvW(acc, left, right);
      }
      break;
    case DataType::Type::kInt64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      if (instruction->GetOpKind() == HInstruction::kAdd) {
        __ MaddvD(acc, left, right);
      } else {
        __ MsubvD(acc, left, right);
      }
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  CreateVecAccumLocations(GetGraph()->GetAllocator(), instruction);
  LocationSummary* locations = instruction->GetLocations();
  // All conversions require at least one temporary register.
  locations->AddTemp(Location::RequiresFpuRegister());
  // Some conversions require a second temporary register.
  HVecOperation* a = instruction->InputAt(1)->AsVecOperation();
  HVecOperation* b = instruction->InputAt(2)->AsVecOperation();
  DCHECK_EQ(HVecOperation::ToSignedType(a->GetPackedType()),
            HVecOperation::ToSignedType(b->GetPackedType()));
  switch (a->GetPackedType()) {
    case DataType::Type::kInt32:
      if (instruction->GetPackedType() == DataType::Type::kInt32) {
        break;
      }
      FALLTHROUGH_INTENDED;
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      locations->AddTemp(Location::RequiresFpuRegister());
      break;
    default:
      break;
  }
}

void InstructionCodeGeneratorMIPS64::VisitVecSADAccumulate(HVecSADAccumulate* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  VectorRegister acc = VectorRegisterFrom(locations->InAt(0));
  VectorRegister left = VectorRegisterFrom(locations->InAt(1));
  VectorRegister right = VectorRegisterFrom(locations->InAt(2));
  VectorRegister tmp = static_cast<VectorRegister>(FTMP);
  VectorRegister tmp1 = VectorRegisterFrom(locations->GetTemp(0));

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
        case DataType::Type::kUint16:
        case DataType::Type::kInt16: {
          DCHECK_EQ(8u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillB(tmp, ZERO);
          __ Hadd_sH(tmp1, left, tmp);
          __ Hadd_sH(tmp2, right, tmp);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ AddvH(acc, acc, tmp1);
          __ Hadd_sH(tmp1, tmp, left);
          __ Hadd_sH(tmp2, tmp, right);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ AddvH(acc, acc, tmp1);
          break;
        }
        case DataType::Type::kInt32: {
          DCHECK_EQ(4u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillB(tmp, ZERO);
          __ Hadd_sH(tmp1, left, tmp);
          __ Hadd_sH(tmp2, right, tmp);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ Hadd_sW(tmp1, tmp1, tmp1);
          __ AddvW(acc, acc, tmp1);
          __ Hadd_sH(tmp1, tmp, left);
          __ Hadd_sH(tmp2, tmp, right);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ Hadd_sW(tmp1, tmp1, tmp1);
          __ AddvW(acc, acc, tmp1);
          break;
        }
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillB(tmp, ZERO);
          __ Hadd_sH(tmp1, left, tmp);
          __ Hadd_sH(tmp2, right, tmp);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ Hadd_sW(tmp1, tmp1, tmp1);
          __ Hadd_sD(tmp1, tmp1, tmp1);
          __ AddvD(acc, acc, tmp1);
          __ Hadd_sH(tmp1, tmp, left);
          __ Hadd_sH(tmp2, tmp, right);
          __ Asub_sH(tmp1, tmp1, tmp2);
          __ Hadd_sW(tmp1, tmp1, tmp1);
          __ Hadd_sD(tmp1, tmp1, tmp1);
          __ AddvD(acc, acc, tmp1);
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
        case DataType::Type::kInt32: {
          DCHECK_EQ(4u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillH(tmp, ZERO);
          __ Hadd_sW(tmp1, left, tmp);
          __ Hadd_sW(tmp2, right, tmp);
          __ Asub_sW(tmp1, tmp1, tmp2);
          __ AddvW(acc, acc, tmp1);
          __ Hadd_sW(tmp1, tmp, left);
          __ Hadd_sW(tmp2, tmp, right);
          __ Asub_sW(tmp1, tmp1, tmp2);
          __ AddvW(acc, acc, tmp1);
          break;
        }
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillH(tmp, ZERO);
          __ Hadd_sW(tmp1, left, tmp);
          __ Hadd_sW(tmp2, right, tmp);
          __ Asub_sW(tmp1, tmp1, tmp2);
          __ Hadd_sD(tmp1, tmp1, tmp1);
          __ AddvD(acc, acc, tmp1);
          __ Hadd_sW(tmp1, tmp, left);
          __ Hadd_sW(tmp2, tmp, right);
          __ Asub_sW(tmp1, tmp1, tmp2);
          __ Hadd_sD(tmp1, tmp1, tmp1);
          __ AddvD(acc, acc, tmp1);
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
          __ FillW(tmp, ZERO);
          __ SubvW(tmp1, left, right);
          __ Add_aW(tmp1, tmp1, tmp);
          __ AddvW(acc, acc, tmp1);
          break;
        }
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          VectorRegister tmp2 = VectorRegisterFrom(locations->GetTemp(1));
          __ FillW(tmp, ZERO);
          __ Hadd_sD(tmp1, left, tmp);
          __ Hadd_sD(tmp2, right, tmp);
          __ Asub_sD(tmp1, tmp1, tmp2);
          __ AddvD(acc, acc, tmp1);
          __ Hadd_sD(tmp1, tmp, left);
          __ Hadd_sD(tmp2, tmp, right);
          __ Asub_sD(tmp1, tmp1, tmp2);
          __ AddvD(acc, acc, tmp1);
          break;
        }
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    case DataType::Type::kInt64: {
      DCHECK_EQ(2u, a->GetVectorLength());
      switch (instruction->GetPackedType()) {
        case DataType::Type::kInt64: {
          DCHECK_EQ(2u, instruction->GetVectorLength());
          __ FillD(tmp, ZERO);
          __ SubvD(tmp1, left, right);
          __ Add_aD(tmp1, tmp1, tmp);
          __ AddvD(acc, acc, tmp1);
          break;
        }
        default:
          LOG(FATAL) << "Unsupported SIMD type";
          UNREACHABLE();
      }
      break;
    }
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
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

// Helper to prepare register and offset for vector memory operations. Returns the offset and sets
// the output parameter adjusted_base to the original base or to a reserved temporary register (AT).
int32_t InstructionCodeGeneratorMIPS64::VecAddress(LocationSummary* locations,
                                                   size_t size,
                                                   /* out */ GpuRegister* adjusted_base) {
  GpuRegister base = locations->InAt(0).AsRegister<GpuRegister>();
  Location index = locations->InAt(1);
  int scale = TIMES_1;
  switch (size) {
    case 2: scale = TIMES_2; break;
    case 4: scale = TIMES_4; break;
    case 8: scale = TIMES_8; break;
    default: break;
  }
  int32_t offset = mirror::Array::DataOffset(size).Int32Value();

  if (index.IsConstant()) {
    offset += index.GetConstant()->AsIntConstant()->GetValue() << scale;
    __ AdjustBaseOffsetAndElementSizeShift(base, offset, scale);
    *adjusted_base = base;
  } else {
    GpuRegister index_reg = index.AsRegister<GpuRegister>();
    if (scale != TIMES_1) {
      __ Dlsa(AT, index_reg, base, scale);
    } else {
      __ Daddu(AT, base, index_reg);
    }
    *adjusted_base = AT;
  }
  return offset;
}

void LocationsBuilderMIPS64::VisitVecLoad(HVecLoad* instruction) {
  CreateVecMemLocations(GetGraph()->GetAllocator(), instruction, /* is_load */ true);
}

void InstructionCodeGeneratorMIPS64::VisitVecLoad(HVecLoad* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t size = DataType::Size(instruction->GetPackedType());
  VectorRegister reg = VectorRegisterFrom(locations->Out());
  GpuRegister base;
  int32_t offset = VecAddress(locations, size, &base);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ LdB(reg, base, offset);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      // Loading 8-bytes (needed if dealing with compressed strings in StringCharAt) from unaligned
      // memory address may cause a trap to the kernel if the CPU doesn't directly support unaligned
      // loads and stores.
      // TODO: Implement support for StringCharAt.
      DCHECK(!instruction->IsStringCharAt());
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ LdH(reg, base, offset);
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ LdW(reg, base, offset);
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ LdD(reg, base, offset);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

void LocationsBuilderMIPS64::VisitVecStore(HVecStore* instruction) {
  CreateVecMemLocations(GetGraph()->GetAllocator(), instruction, /* is_load */ false);
}

void InstructionCodeGeneratorMIPS64::VisitVecStore(HVecStore* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t size = DataType::Size(instruction->GetPackedType());
  VectorRegister reg = VectorRegisterFrom(locations->InAt(2));
  GpuRegister base;
  int32_t offset = VecAddress(locations, size, &base);
  switch (instruction->GetPackedType()) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
      DCHECK_EQ(16u, instruction->GetVectorLength());
      __ StB(reg, base, offset);
      break;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      DCHECK_EQ(8u, instruction->GetVectorLength());
      __ StH(reg, base, offset);
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
      DCHECK_EQ(4u, instruction->GetVectorLength());
      __ StW(reg, base, offset);
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      DCHECK_EQ(2u, instruction->GetVectorLength());
      __ StD(reg, base, offset);
      break;
    default:
      LOG(FATAL) << "Unsupported SIMD type";
      UNREACHABLE();
  }
}

#undef __

}  // namespace mips64
}  // namespace art
