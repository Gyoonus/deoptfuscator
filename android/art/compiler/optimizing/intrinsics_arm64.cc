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

#include "intrinsics_arm64.h"

#include "arch/arm64/instruction_set_features_arm64.h"
#include "art_method.h"
#include "code_generator_arm64.h"
#include "common_arm64.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/reference.h"
#include "mirror/string-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "utils/arm64/assembler_arm64.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

// TODO(VIXL): Make VIXL compile with -Wshadow.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "aarch64/disasm-aarch64.h"
#include "aarch64/macro-assembler-aarch64.h"
#pragma GCC diagnostic pop

namespace art {

namespace arm64 {

using helpers::DRegisterFrom;
using helpers::FPRegisterFrom;
using helpers::HeapOperand;
using helpers::LocationFrom;
using helpers::OperandFrom;
using helpers::RegisterFrom;
using helpers::SRegisterFrom;
using helpers::WRegisterFrom;
using helpers::XRegisterFrom;
using helpers::InputRegisterAt;
using helpers::OutputRegister;

namespace {

ALWAYS_INLINE inline MemOperand AbsoluteHeapOperandFrom(Location location, size_t offset = 0) {
  return MemOperand(XRegisterFrom(location), offset);
}

}  // namespace

MacroAssembler* IntrinsicCodeGeneratorARM64::GetVIXLAssembler() {
  return codegen_->GetVIXLAssembler();
}

ArenaAllocator* IntrinsicCodeGeneratorARM64::GetAllocator() {
  return codegen_->GetGraph()->GetAllocator();
}

#define __ codegen->GetVIXLAssembler()->

static void MoveFromReturnRegister(Location trg,
                                   DataType::Type type,
                                   CodeGeneratorARM64* codegen) {
  if (!trg.IsValid()) {
    DCHECK(type == DataType::Type::kVoid);
    return;
  }

  DCHECK_NE(type, DataType::Type::kVoid);

  if (DataType::IsIntegralType(type) || type == DataType::Type::kReference) {
    Register trg_reg = RegisterFrom(trg, type);
    Register res_reg = RegisterFrom(ARM64ReturnLocation(type), type);
    __ Mov(trg_reg, res_reg, kDiscardForSameWReg);
  } else {
    FPRegister trg_reg = FPRegisterFrom(trg, type);
    FPRegister res_reg = FPRegisterFrom(ARM64ReturnLocation(type), type);
    __ Fmov(trg_reg, res_reg);
  }
}

static void MoveArguments(HInvoke* invoke, CodeGeneratorARM64* codegen) {
  InvokeDexCallingConventionVisitorARM64 calling_convention_visitor;
  IntrinsicVisitor::MoveArguments(invoke, codegen, &calling_convention_visitor);
}

// Slow-path for fallback (calling the managed code to handle the intrinsic) in an intrinsified
// call. This will copy the arguments into the positions for a regular call.
//
// Note: The actual parameters are required to be in the locations given by the invoke's location
//       summary. If an intrinsic modifies those locations before a slowpath call, they must be
//       restored!
class IntrinsicSlowPathARM64 : public SlowPathCodeARM64 {
 public:
  explicit IntrinsicSlowPathARM64(HInvoke* invoke)
      : SlowPathCodeARM64(invoke), invoke_(invoke) { }

  void EmitNativeCode(CodeGenerator* codegen_in) OVERRIDE {
    CodeGeneratorARM64* codegen = down_cast<CodeGeneratorARM64*>(codegen_in);
    __ Bind(GetEntryLabel());

    SaveLiveRegisters(codegen, invoke_->GetLocations());

    MoveArguments(invoke_, codegen);

    {
      // Ensure that between the BLR (emitted by Generate*Call) and RecordPcInfo there
      // are no pools emitted.
      vixl::EmissionCheckScope guard(codegen->GetVIXLAssembler(), kInvokeCodeMarginSizeInBytes);
      if (invoke_->IsInvokeStaticOrDirect()) {
        codegen->GenerateStaticOrDirectCall(
            invoke_->AsInvokeStaticOrDirect(), LocationFrom(kArtMethodRegister), this);
      } else {
        codegen->GenerateVirtualCall(
            invoke_->AsInvokeVirtual(), LocationFrom(kArtMethodRegister), this);
      }
    }

    // Copy the result back to the expected output.
    Location out = invoke_->GetLocations()->Out();
    if (out.IsValid()) {
      DCHECK(out.IsRegister());  // TODO: Replace this when we support output in memory.
      DCHECK(!invoke_->GetLocations()->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      MoveFromReturnRegister(out, invoke_->GetType(), codegen);
    }

    RestoreLiveRegisters(codegen, invoke_->GetLocations());
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "IntrinsicSlowPathARM64"; }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathARM64);
};

// Slow path implementing the SystemArrayCopy intrinsic copy loop with read barriers.
class ReadBarrierSystemArrayCopySlowPathARM64 : public SlowPathCodeARM64 {
 public:
  ReadBarrierSystemArrayCopySlowPathARM64(HInstruction* instruction, Location tmp)
      : SlowPathCodeARM64(instruction), tmp_(tmp) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen_in) OVERRIDE {
    CodeGeneratorARM64* codegen = down_cast<CodeGeneratorARM64*>(codegen_in);
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(locations->CanCall());
    DCHECK(instruction_->IsInvokeStaticOrDirect())
        << "Unexpected instruction in read barrier arraycopy slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kSystemArrayCopy);

    const int32_t element_size = DataType::Size(DataType::Type::kReference);

    Register src_curr_addr = XRegisterFrom(locations->GetTemp(0));
    Register dst_curr_addr = XRegisterFrom(locations->GetTemp(1));
    Register src_stop_addr = XRegisterFrom(locations->GetTemp(2));
    Register tmp_reg = WRegisterFrom(tmp_);

    __ Bind(GetEntryLabel());
    vixl::aarch64::Label slow_copy_loop;
    __ Bind(&slow_copy_loop);
    __ Ldr(tmp_reg, MemOperand(src_curr_addr, element_size, PostIndex));
    codegen->GetAssembler()->MaybeUnpoisonHeapReference(tmp_reg);
    // TODO: Inline the mark bit check before calling the runtime?
    // tmp_reg = ReadBarrier::Mark(tmp_reg);
    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    // (See ReadBarrierMarkSlowPathARM64::EmitNativeCode for more
    // explanations.)
    DCHECK_NE(tmp_.reg(), LR);
    DCHECK_NE(tmp_.reg(), WSP);
    DCHECK_NE(tmp_.reg(), WZR);
    // IP0 is used internally by the ReadBarrierMarkRegX entry point
    // as a temporary (and not preserved).  It thus cannot be used by
    // any live register in this slow path.
    DCHECK_NE(LocationFrom(src_curr_addr).reg(), IP0);
    DCHECK_NE(LocationFrom(dst_curr_addr).reg(), IP0);
    DCHECK_NE(LocationFrom(src_stop_addr).reg(), IP0);
    DCHECK_NE(tmp_.reg(), IP0);
    DCHECK(0 <= tmp_.reg() && tmp_.reg() < kNumberOfWRegisters) << tmp_.reg();
    // TODO: Load the entrypoint once before the loop, instead of
    // loading it at every iteration.
    int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kArm64PointerSize>(tmp_.reg());
    // This runtime call does not require a stack map.
    codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset, instruction_, this);
    codegen->GetAssembler()->MaybePoisonHeapReference(tmp_reg);
    __ Str(tmp_reg, MemOperand(dst_curr_addr, element_size, PostIndex));
    __ Cmp(src_curr_addr, src_stop_addr);
    __ B(&slow_copy_loop, ne);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierSystemArrayCopySlowPathARM64"; }

 private:
  Location tmp_;

  DISALLOW_COPY_AND_ASSIGN(ReadBarrierSystemArrayCopySlowPathARM64);
};
#undef __

bool IntrinsicLocationsBuilderARM64::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  if (res == nullptr) {
    return false;
  }
  return res->Intrinsified();
}

#define __ masm->

static void CreateFPToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void CreateIntToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

static void MoveFPToInt(LocationSummary* locations, bool is64bit, MacroAssembler* masm) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ Fmov(is64bit ? XRegisterFrom(output) : WRegisterFrom(output),
          is64bit ? DRegisterFrom(input) : SRegisterFrom(input));
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, MacroAssembler* masm) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  __ Fmov(is64bit ? DRegisterFrom(output) : SRegisterFrom(output),
          is64bit ? XRegisterFrom(input) : WRegisterFrom(input));
}

void IntrinsicLocationsBuilderARM64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ true, GetVIXLAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ false, GetVIXLAssembler());
}
void IntrinsicCodeGeneratorARM64::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ false, GetVIXLAssembler());
}

static void CreateIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void GenReverseBytes(LocationSummary* locations,
                            DataType::Type type,
                            MacroAssembler* masm) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  switch (type) {
    case DataType::Type::kInt16:
      __ Rev16(WRegisterFrom(out), WRegisterFrom(in));
      __ Sxth(WRegisterFrom(out), WRegisterFrom(out));
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kInt64:
      __ Rev(RegisterFrom(out, type), RegisterFrom(in, type));
      break;
    default:
      LOG(FATAL) << "Unexpected size for reverse-bytes: " << type;
      UNREACHABLE();
  }
}

void IntrinsicLocationsBuilderARM64::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), DataType::Type::kInt64, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), DataType::Type::kInt16, GetVIXLAssembler());
}

static void CreateIntIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void GenNumberOfLeadingZeros(LocationSummary* locations,
                                    DataType::Type type,
                                    MacroAssembler* masm) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  Location in = locations->InAt(0);
  Location out = locations->Out();

  __ Clz(RegisterFrom(out, type), RegisterFrom(in, type));
}

void IntrinsicLocationsBuilderARM64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeros(invoke->GetLocations(), DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeros(invoke->GetLocations(), DataType::Type::kInt64, GetVIXLAssembler());
}

static void GenNumberOfTrailingZeros(LocationSummary* locations,
                                     DataType::Type type,
                                     MacroAssembler* masm) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  Location in = locations->InAt(0);
  Location out = locations->Out();

  __ Rbit(RegisterFrom(out, type), RegisterFrom(in, type));
  __ Clz(RegisterFrom(out, type), RegisterFrom(out, type));
}

void IntrinsicLocationsBuilderARM64::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeros(invoke->GetLocations(), DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeros(invoke->GetLocations(), DataType::Type::kInt64, GetVIXLAssembler());
}

static void GenReverse(LocationSummary* locations,
                       DataType::Type type,
                       MacroAssembler* masm) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  Location in = locations->InAt(0);
  Location out = locations->Out();

  __ Rbit(RegisterFrom(out, type), RegisterFrom(in, type));
}

void IntrinsicLocationsBuilderARM64::VisitIntegerReverse(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongReverse(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(), DataType::Type::kInt64, GetVIXLAssembler());
}

static void GenBitCount(HInvoke* instr, DataType::Type type, MacroAssembler* masm) {
  DCHECK(DataType::IsIntOrLongType(type)) << type;
  DCHECK_EQ(instr->GetType(), DataType::Type::kInt32);
  DCHECK_EQ(DataType::Kind(instr->InputAt(0)->GetType()), type);

  UseScratchRegisterScope temps(masm);

  Register src = InputRegisterAt(instr, 0);
  Register dst = RegisterFrom(instr->GetLocations()->Out(), type);
  FPRegister fpr = (type == DataType::Type::kInt64) ? temps.AcquireD() : temps.AcquireS();

  __ Fmov(fpr, src);
  __ Cnt(fpr.V8B(), fpr.V8B());
  __ Addv(fpr.B(), fpr.V8B());
  __ Fmov(dst, fpr);
}

void IntrinsicLocationsBuilderARM64::VisitLongBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongBitCount(HInvoke* invoke) {
  GenBitCount(invoke, DataType::Type::kInt64, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitIntegerBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerBitCount(HInvoke* invoke) {
  GenBitCount(invoke, DataType::Type::kInt32, GetVIXLAssembler());
}

static void GenHighestOneBit(HInvoke* invoke, DataType::Type type, MacroAssembler* masm) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  UseScratchRegisterScope temps(masm);

  Register src = InputRegisterAt(invoke, 0);
  Register dst = RegisterFrom(invoke->GetLocations()->Out(), type);
  Register temp = (type == DataType::Type::kInt64) ? temps.AcquireX() : temps.AcquireW();
  size_t high_bit = (type == DataType::Type::kInt64) ? 63u : 31u;
  size_t clz_high_bit = (type == DataType::Type::kInt64) ? 6u : 5u;

  __ Clz(temp, src);
  __ Mov(dst, UINT64_C(1) << high_bit);  // MOV (bitmask immediate)
  __ Bic(dst, dst, Operand(temp, LSL, high_bit - clz_high_bit));  // Clear dst if src was 0.
  __ Lsr(dst, dst, temp);
}

void IntrinsicLocationsBuilderARM64::VisitIntegerHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke, DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke, DataType::Type::kInt64, GetVIXLAssembler());
}

static void GenLowestOneBit(HInvoke* invoke, DataType::Type type, MacroAssembler* masm) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  UseScratchRegisterScope temps(masm);

  Register src = InputRegisterAt(invoke, 0);
  Register dst = RegisterFrom(invoke->GetLocations()->Out(), type);
  Register temp = (type == DataType::Type::kInt64) ? temps.AcquireX() : temps.AcquireW();

  __ Neg(temp, src);
  __ And(dst, temp, src);
}

void IntrinsicLocationsBuilderARM64::VisitIntegerLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitIntegerLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke, DataType::Type::kInt32, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitLongLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitLongLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke, DataType::Type::kInt64, GetVIXLAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

static void MathAbsFP(LocationSummary* locations, bool is64bit, MacroAssembler* masm) {
  Location in = locations->InAt(0);
  Location out = locations->Out();

  FPRegister in_reg = is64bit ? DRegisterFrom(in) : SRegisterFrom(in);
  FPRegister out_reg = is64bit ? DRegisterFrom(out) : SRegisterFrom(out);

  __ Fabs(out_reg, in_reg);
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), /* is64bit */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), /* is64bit */ false, GetVIXLAssembler());
}

static void GenAbsInteger(LocationSummary* locations,
                          bool is64bit,
                          MacroAssembler* masm) {
  Location in = locations->InAt(0);
  Location output = locations->Out();

  Register in_reg = is64bit ? XRegisterFrom(in) : WRegisterFrom(in);
  Register out_reg = is64bit ? XRegisterFrom(output) : WRegisterFrom(output);

  __ Cmp(in_reg, Operand(0));
  __ Cneg(out_reg, in_reg, lt);
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ true, GetVIXLAssembler());
}

static void GenMinMaxFP(LocationSummary* locations,
                        bool is_min,
                        bool is_double,
                        MacroAssembler* masm) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  FPRegister op1_reg = is_double ? DRegisterFrom(op1) : SRegisterFrom(op1);
  FPRegister op2_reg = is_double ? DRegisterFrom(op2) : SRegisterFrom(op2);
  FPRegister out_reg = is_double ? DRegisterFrom(out) : SRegisterFrom(out);
  if (is_min) {
    __ Fmin(out_reg, op1_reg, op2_reg);
  } else {
    __ Fmax(out_reg, op1_reg, op2_reg);
  }
}

static void CreateFPFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

void IntrinsicLocationsBuilderARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), /* is_min */ true, /* is_double */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), /* is_min */ true, /* is_double */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(), /* is_min */ false, /* is_double */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(
      invoke->GetLocations(), /* is_min */ false, /* is_double */ false, GetVIXLAssembler());
}

static void GenMinMax(LocationSummary* locations,
                      bool is_min,
                      bool is_long,
                      MacroAssembler* masm) {
  Location op1 = locations->InAt(0);
  Location op2 = locations->InAt(1);
  Location out = locations->Out();

  Register op1_reg = is_long ? XRegisterFrom(op1) : WRegisterFrom(op1);
  Register op2_reg = is_long ? XRegisterFrom(op2) : WRegisterFrom(op2);
  Register out_reg = is_long ? XRegisterFrom(out) : WRegisterFrom(out);

  __ Cmp(op1_reg, op2_reg);
  __ Csel(out_reg, op1_reg, op2_reg, is_min ? lt : gt);
}

void IntrinsicLocationsBuilderARM64::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ true, /* is_long */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMinLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ true, /* is_long */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ false, /* is_long */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ false, /* is_long */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = GetVIXLAssembler();
  __ Fsqrt(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathCeil(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCeil(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = GetVIXLAssembler();
  __ Frintp(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathFloor(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathFloor(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = GetVIXLAssembler();
  __ Frintm(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

void IntrinsicLocationsBuilderARM64::VisitMathRint(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRint(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = GetVIXLAssembler();
  __ Frintn(DRegisterFrom(locations->Out()), DRegisterFrom(locations->InAt(0)));
}

static void CreateFPToIntPlusFPTempLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresFpuRegister());
}

static void GenMathRound(HInvoke* invoke, bool is_double, vixl::aarch64::MacroAssembler* masm) {
  // Java 8 API definition for Math.round():
  // Return the closest long or int to the argument, with ties rounding to positive infinity.
  //
  // There is no single instruction in ARMv8 that can support the above definition.
  // We choose to use FCVTAS here, because it has closest semantic.
  // FCVTAS performs rounding to nearest integer, ties away from zero.
  // For most inputs (positive values, zero or NaN), this instruction is enough.
  // We only need a few handling code after FCVTAS if the input is negative half value.
  //
  // The reason why we didn't choose FCVTPS instruction here is that
  // although it performs rounding toward positive infinity, it doesn't perform rounding to nearest.
  // For example, FCVTPS(-1.9) = -1 and FCVTPS(1.1) = 2.
  // If we were using this instruction, for most inputs, more handling code would be needed.
  LocationSummary* l = invoke->GetLocations();
  FPRegister in_reg = is_double ? DRegisterFrom(l->InAt(0)) : SRegisterFrom(l->InAt(0));
  FPRegister tmp_fp = is_double ? DRegisterFrom(l->GetTemp(0)) : SRegisterFrom(l->GetTemp(0));
  Register out_reg = is_double ? XRegisterFrom(l->Out()) : WRegisterFrom(l->Out());
  vixl::aarch64::Label done;

  // Round to nearest integer, ties away from zero.
  __ Fcvtas(out_reg, in_reg);

  // For positive values, zero or NaN inputs, rounding is done.
  __ Tbz(out_reg, out_reg.GetSizeInBits() - 1, &done);

  // Handle input < 0 cases.
  // If input is negative but not a tie, previous result (round to nearest) is valid.
  // If input is a negative tie, out_reg += 1.
  __ Frinta(tmp_fp, in_reg);
  __ Fsub(tmp_fp, in_reg, tmp_fp);
  __ Fcmp(tmp_fp, 0.5);
  __ Cinc(out_reg, out_reg, eq);

  __ Bind(&done);
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundDouble(HInvoke* invoke) {
  CreateFPToIntPlusFPTempLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundDouble(HInvoke* invoke) {
  GenMathRound(invoke, /* is_double */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMathRoundFloat(HInvoke* invoke) {
  CreateFPToIntPlusFPTempLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathRoundFloat(HInvoke* invoke) {
  GenMathRound(invoke, /* is_double */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekByte(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Ldrsb(WRegisterFrom(invoke->GetLocations()->Out()),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekIntNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Ldr(WRegisterFrom(invoke->GetLocations()->Out()),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekLongNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Ldr(XRegisterFrom(invoke->GetLocations()->Out()),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPeekShortNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Ldrsh(WRegisterFrom(invoke->GetLocations()->Out()),
           AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

static void CreateIntIntToVoidLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeByte(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Strb(WRegisterFrom(invoke->GetLocations()->InAt(1)),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeIntNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Str(WRegisterFrom(invoke->GetLocations()->InAt(1)),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeLongNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Str(XRegisterFrom(invoke->GetLocations()->InAt(1)),
         AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMemoryPokeShortNative(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  __ Strh(WRegisterFrom(invoke->GetLocations()->InAt(1)),
          AbsoluteHeapOperandFrom(invoke->GetLocations()->InAt(0), 0));
}

void IntrinsicLocationsBuilderARM64::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitThreadCurrentThread(HInvoke* invoke) {
  codegen_->Load(DataType::Type::kReference, WRegisterFrom(invoke->GetLocations()->Out()),
                 MemOperand(tr, Thread::PeerOffset<kArm64PointerSize>().Int32Value()));
}

static void GenUnsafeGet(HInvoke* invoke,
                         DataType::Type type,
                         bool is_volatile,
                         CodeGeneratorARM64* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  DCHECK((type == DataType::Type::kInt32) ||
         (type == DataType::Type::kInt64) ||
         (type == DataType::Type::kReference));
  Location base_loc = locations->InAt(1);
  Register base = WRegisterFrom(base_loc);      // Object pointer.
  Location offset_loc = locations->InAt(2);
  Register offset = XRegisterFrom(offset_loc);  // Long offset.
  Location trg_loc = locations->Out();
  Register trg = RegisterFrom(trg_loc, type);

  if (type == DataType::Type::kReference && kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // UnsafeGetObject/UnsafeGetObjectVolatile with Baker's read barrier case.
    Register temp = WRegisterFrom(locations->GetTemp(0));
    codegen->GenerateReferenceLoadWithBakerReadBarrier(invoke,
                                                       trg_loc,
                                                       base,
                                                       /* offset */ 0u,
                                                       /* index */ offset_loc,
                                                       /* scale_factor */ 0u,
                                                       temp,
                                                       /* needs_null_check */ false,
                                                       is_volatile);
  } else {
    // Other cases.
    MemOperand mem_op(base.X(), offset);
    if (is_volatile) {
      codegen->LoadAcquire(invoke, trg, mem_op, /* needs_null_check */ true);
    } else {
      codegen->Load(type, trg, mem_op);
    }

    if (type == DataType::Type::kReference) {
      DCHECK(trg.IsW());
      codegen->MaybeGenerateReadBarrierSlow(invoke, trg_loc, trg_loc, base_loc, 0u, offset_loc);
    }
  }
}

static void CreateIntIntIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  bool can_call = kEmitCompilerReadBarrier &&
      (invoke->GetIntrinsic() == Intrinsics::kUnsafeGetObject ||
       invoke->GetIntrinsic() == Intrinsics::kUnsafeGetObjectVolatile);
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke,
                                      can_call
                                          ? LocationSummary::kCallOnSlowPath
                                          : LocationSummary::kNoCall,
                                      kIntrinsified);
  if (can_call && kUseBakerReadBarrier) {
    locations->SetCustomSlowPathCallerSaves(RegisterSet::Empty());  // No caller-save registers.
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARM64::GenerateReferenceLoadWithBakerReadBarrier.
    locations->AddTemp(Location::RequiresRegister());
  }
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(),
                    (can_call ? Location::kOutputOverlap : Location::kNoOutputOverlap));
}

void IntrinsicLocationsBuilderARM64::VisitUnsafeGet(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetLong(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetObject(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetObject(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ true, codegen_);
}

static void CreateIntIntIntIntToVoid(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARM64::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(allocator_, invoke);
}

static void GenUnsafePut(HInvoke* invoke,
                         DataType::Type type,
                         bool is_volatile,
                         bool is_ordered,
                         CodeGeneratorARM64* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = codegen->GetVIXLAssembler();

  Register base = WRegisterFrom(locations->InAt(1));    // Object pointer.
  Register offset = XRegisterFrom(locations->InAt(2));  // Long offset.
  Register value = RegisterFrom(locations->InAt(3), type);
  Register source = value;
  MemOperand mem_op(base.X(), offset);

  {
    // We use a block to end the scratch scope before the write barrier, thus
    // freeing the temporary registers so they can be used in `MarkGCCard`.
    UseScratchRegisterScope temps(masm);

    if (kPoisonHeapReferences && type == DataType::Type::kReference) {
      DCHECK(value.IsW());
      Register temp = temps.AcquireW();
      __ Mov(temp.W(), value.W());
      codegen->GetAssembler()->PoisonHeapReference(temp.W());
      source = temp;
    }

    if (is_volatile || is_ordered) {
      codegen->StoreRelease(invoke, type, source, mem_op, /* needs_null_check */ false);
    } else {
      codegen->Store(type, source, mem_op);
    }
  }

  if (type == DataType::Type::kReference) {
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(base, value, value_can_be_null);
  }
}

void IntrinsicCodeGeneratorARM64::VisitUnsafePut(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt32,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObject(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kReference,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLong(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke,
               DataType::Type::kInt64,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}

static void CreateIntIntIntIntIntToInt(ArenaAllocator* allocator,
                                       HInvoke* invoke,
                                       DataType::Type type) {
  bool can_call = kEmitCompilerReadBarrier &&
      kUseBakerReadBarrier &&
      (invoke->GetIntrinsic() == Intrinsics::kUnsafeCASObject);
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke,
                                      can_call
                                          ? LocationSummary::kCallOnSlowPath
                                          : LocationSummary::kNoCall,
                                      kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  locations->SetInAt(4, Location::RequiresRegister());

  // If heap poisoning is enabled, we don't want the unpoisoning
  // operations to potentially clobber the output. Likewise when
  // emitting a (Baker) read barrier, which may call.
  Location::OutputOverlap overlaps =
      ((kPoisonHeapReferences && type == DataType::Type::kReference) || can_call)
      ? Location::kOutputOverlap
      : Location::kNoOutputOverlap;
  locations->SetOut(Location::RequiresRegister(), overlaps);
  if (type == DataType::Type::kReference && kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // Temporary register for (Baker) read barrier.
    locations->AddTemp(Location::RequiresRegister());
  }
}

static void GenCas(HInvoke* invoke, DataType::Type type, CodeGeneratorARM64* codegen) {
  MacroAssembler* masm = codegen->GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Location out_loc = locations->Out();
  Register out = WRegisterFrom(out_loc);                           // Boolean result.

  Register base = WRegisterFrom(locations->InAt(1));               // Object pointer.
  Location offset_loc = locations->InAt(2);
  Register offset = XRegisterFrom(offset_loc);                     // Long offset.
  Register expected = RegisterFrom(locations->InAt(3), type);      // Expected.
  Register value = RegisterFrom(locations->InAt(4), type);         // Value.

  // This needs to be before the temp registers, as MarkGCCard also uses VIXL temps.
  if (type == DataType::Type::kReference) {
    // Mark card for object assuming new value is stored.
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(base, value, value_can_be_null);

    // The only read barrier implementation supporting the
    // UnsafeCASObject intrinsic is the Baker-style read barriers.
    DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      Register temp = WRegisterFrom(locations->GetTemp(0));
      // Need to make sure the reference stored in the field is a to-space
      // one before attempting the CAS or the CAS could fail incorrectly.
      codegen->UpdateReferenceFieldWithBakerReadBarrier(
          invoke,
          out_loc,  // Unused, used only as a "temporary" within the read barrier.
          base,
          /* field_offset */ offset_loc,
          temp,
          /* needs_null_check */ false,
          /* use_load_acquire */ false);
    }
  }

  UseScratchRegisterScope temps(masm);
  Register tmp_ptr = temps.AcquireX();                             // Pointer to actual memory.
  Register tmp_value = temps.AcquireSameSizeAs(value);             // Value in memory.

  Register tmp_32 = tmp_value.W();

  __ Add(tmp_ptr, base.X(), Operand(offset));

  if (kPoisonHeapReferences && type == DataType::Type::kReference) {
    codegen->GetAssembler()->PoisonHeapReference(expected);
    if (value.Is(expected)) {
      // Do not poison `value`, as it is the same register as
      // `expected`, which has just been poisoned.
    } else {
      codegen->GetAssembler()->PoisonHeapReference(value);
    }
  }

  // do {
  //   tmp_value = [tmp_ptr] - expected;
  // } while (tmp_value == 0 && failure([tmp_ptr] <- r_new_value));
  // result = tmp_value != 0;

  vixl::aarch64::Label loop_head, exit_loop;
  __ Bind(&loop_head);
  __ Ldaxr(tmp_value, MemOperand(tmp_ptr));
  __ Cmp(tmp_value, expected);
  __ B(&exit_loop, ne);
  __ Stlxr(tmp_32, value, MemOperand(tmp_ptr));
  __ Cbnz(tmp_32, &loop_head);
  __ Bind(&exit_loop);
  __ Cset(out, eq);

  if (kPoisonHeapReferences && type == DataType::Type::kReference) {
    codegen->GetAssembler()->UnpoisonHeapReference(expected);
    if (value.Is(expected)) {
      // Do not unpoison `value`, as it is the same register as
      // `expected`, which has just been unpoisoned.
    } else {
      codegen->GetAssembler()->UnpoisonHeapReference(value);
    }
  }
}

void IntrinsicLocationsBuilderARM64::VisitUnsafeCASInt(HInvoke* invoke) {
  CreateIntIntIntIntIntToInt(allocator_, invoke, DataType::Type::kInt32);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeCASLong(HInvoke* invoke) {
  CreateIntIntIntIntIntToInt(allocator_, invoke, DataType::Type::kInt64);
}
void IntrinsicLocationsBuilderARM64::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CreateIntIntIntIntIntToInt(allocator_, invoke, DataType::Type::kReference);
}

void IntrinsicCodeGeneratorARM64::VisitUnsafeCASInt(HInvoke* invoke) {
  GenCas(invoke, DataType::Type::kInt32, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeCASLong(HInvoke* invoke) {
  GenCas(invoke, DataType::Type::kInt64, codegen_);
}
void IntrinsicCodeGeneratorARM64::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  GenCas(invoke, DataType::Type::kReference, codegen_);
}

void IntrinsicLocationsBuilderARM64::VisitStringCompareTo(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke,
                                       invoke->InputAt(1)->CanBeNull()
                                           ? LocationSummary::kCallOnSlowPath
                                           : LocationSummary::kNoCall,
                                       kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  // Need temporary registers for String compression's feature.
  if (mirror::kUseStringCompression) {
    locations->AddTemp(Location::RequiresRegister());
  }
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorARM64::VisitStringCompareTo(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register str = InputRegisterAt(invoke, 0);
  Register arg = InputRegisterAt(invoke, 1);
  DCHECK(str.IsW());
  DCHECK(arg.IsW());
  Register out = OutputRegister(invoke);

  Register temp0 = WRegisterFrom(locations->GetTemp(0));
  Register temp1 = WRegisterFrom(locations->GetTemp(1));
  Register temp2 = WRegisterFrom(locations->GetTemp(2));
  Register temp3;
  if (mirror::kUseStringCompression) {
    temp3 = WRegisterFrom(locations->GetTemp(3));
  }

  vixl::aarch64::Label loop;
  vixl::aarch64::Label find_char_diff;
  vixl::aarch64::Label end;
  vixl::aarch64::Label different_compression;

  // Get offsets of count and value fields within a string object.
  const int32_t count_offset = mirror::String::CountOffset().Int32Value();
  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // Take slow path and throw if input can be and is null.
  SlowPathCodeARM64* slow_path = nullptr;
  const bool can_slow_path = invoke->InputAt(1)->CanBeNull();
  if (can_slow_path) {
    slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
    codegen_->AddSlowPath(slow_path);
    __ Cbz(arg, slow_path->GetEntryLabel());
  }

  // Reference equality check, return 0 if same reference.
  __ Subs(out, str, arg);
  __ B(&end, eq);

  if (mirror::kUseStringCompression) {
    // Load `count` fields of this and argument strings.
    __ Ldr(temp3, HeapOperand(str, count_offset));
    __ Ldr(temp2, HeapOperand(arg, count_offset));
    // Clean out compression flag from lengths.
    __ Lsr(temp0, temp3, 1u);
    __ Lsr(temp1, temp2, 1u);
  } else {
    // Load lengths of this and argument strings.
    __ Ldr(temp0, HeapOperand(str, count_offset));
    __ Ldr(temp1, HeapOperand(arg, count_offset));
  }
  // out = length diff.
  __ Subs(out, temp0, temp1);
  // temp0 = min(len(str), len(arg)).
  __ Csel(temp0, temp1, temp0, ge);
  // Shorter string is empty?
  __ Cbz(temp0, &end);

  if (mirror::kUseStringCompression) {
    // Check if both strings using same compression style to use this comparison loop.
    __ Eor(temp2, temp2, Operand(temp3));
    // Interleave with compression flag extraction which is needed for both paths
    // and also set flags which is needed only for the different compressions path.
    __ Ands(temp3.W(), temp3.W(), Operand(1));
    __ Tbnz(temp2, 0, &different_compression);  // Does not use flags.
  }
  // Store offset of string value in preparation for comparison loop.
  __ Mov(temp1, value_offset);
  if (mirror::kUseStringCompression) {
    // For string compression, calculate the number of bytes to compare (not chars).
    // This could in theory exceed INT32_MAX, so treat temp0 as unsigned.
    __ Lsl(temp0, temp0, temp3);
  }

  UseScratchRegisterScope scratch_scope(masm);
  Register temp4 = scratch_scope.AcquireX();

  // Assertions that must hold in order to compare strings 8 bytes at a time.
  DCHECK_ALIGNED(value_offset, 8);
  static_assert(IsAligned<8>(kObjectAlignment), "String of odd length is not zero padded");

  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  // Promote temp2 to an X reg, ready for LDR.
  temp2 = temp2.X();

  // Loop to compare 4x16-bit characters at a time (ok because of string data alignment).
  __ Bind(&loop);
  __ Ldr(temp4, MemOperand(str.X(), temp1.X()));
  __ Ldr(temp2, MemOperand(arg.X(), temp1.X()));
  __ Cmp(temp4, temp2);
  __ B(ne, &find_char_diff);
  __ Add(temp1, temp1, char_size * 4);
  // With string compression, we have compared 8 bytes, otherwise 4 chars.
  __ Subs(temp0, temp0, (mirror::kUseStringCompression) ? 8 : 4);
  __ B(&loop, hi);
  __ B(&end);

  // Promote temp1 to an X reg, ready for EOR.
  temp1 = temp1.X();

  // Find the single character difference.
  __ Bind(&find_char_diff);
  // Get the bit position of the first character that differs.
  __ Eor(temp1, temp2, temp4);
  __ Rbit(temp1, temp1);
  __ Clz(temp1, temp1);

  // If the number of chars remaining <= the index where the difference occurs (0-3), then
  // the difference occurs outside the remaining string data, so just return length diff (out).
  // Unlike ARM, we're doing the comparison in one go here, without the subtraction at the
  // find_char_diff_2nd_cmp path, so it doesn't matter whether the comparison is signed or
  // unsigned when string compression is disabled.
  // When it's enabled, the comparison must be unsigned.
  __ Cmp(temp0, Operand(temp1.W(), LSR, (mirror::kUseStringCompression) ? 3 : 4));
  __ B(ls, &end);

  // Extract the characters and calculate the difference.
  if (mirror:: kUseStringCompression) {
    __ Bic(temp1, temp1, 0x7);
    __ Bic(temp1, temp1, Operand(temp3.X(), LSL, 3u));
  } else {
    __ Bic(temp1, temp1, 0xf);
  }
  __ Lsr(temp2, temp2, temp1);
  __ Lsr(temp4, temp4, temp1);
  if (mirror::kUseStringCompression) {
    // Prioritize the case of compressed strings and calculate such result first.
    __ Uxtb(temp1, temp4);
    __ Sub(out, temp1.W(), Operand(temp2.W(), UXTB));
    __ Tbz(temp3, 0u, &end);  // If actually compressed, we're done.
  }
  __ Uxth(temp4, temp4);
  __ Sub(out, temp4.W(), Operand(temp2.W(), UXTH));

  if (mirror::kUseStringCompression) {
    __ B(&end);
    __ Bind(&different_compression);

    // Comparison for different compression style.
    const size_t c_char_size = DataType::Size(DataType::Type::kInt8);
    DCHECK_EQ(c_char_size, 1u);
    temp1 = temp1.W();
    temp2 = temp2.W();
    temp4 = temp4.W();

    // `temp1` will hold the compressed data pointer, `temp2` the uncompressed data pointer.
    // Note that flags have been set by the `str` compression flag extraction to `temp3`
    // before branching to the `different_compression` label.
    __ Csel(temp1, str, arg, eq);   // Pointer to the compressed string.
    __ Csel(temp2, str, arg, ne);   // Pointer to the uncompressed string.

    // We want to free up the temp3, currently holding `str` compression flag, for comparison.
    // So, we move it to the bottom bit of the iteration count `temp0` which we then need to treat
    // as unsigned. Start by freeing the bit with a LSL and continue further down by a SUB which
    // will allow `subs temp0, #2; bhi different_compression_loop` to serve as the loop condition.
    __ Lsl(temp0, temp0, 1u);

    // Adjust temp1 and temp2 from string pointers to data pointers.
    __ Add(temp1, temp1, Operand(value_offset));
    __ Add(temp2, temp2, Operand(value_offset));

    // Complete the move of the compression flag.
    __ Sub(temp0, temp0, Operand(temp3));

    vixl::aarch64::Label different_compression_loop;
    vixl::aarch64::Label different_compression_diff;

    __ Bind(&different_compression_loop);
    __ Ldrb(temp4, MemOperand(temp1.X(), c_char_size, PostIndex));
    __ Ldrh(temp3, MemOperand(temp2.X(), char_size, PostIndex));
    __ Subs(temp4, temp4, Operand(temp3));
    __ B(&different_compression_diff, ne);
    __ Subs(temp0, temp0, 2);
    __ B(&different_compression_loop, hi);
    __ B(&end);

    // Calculate the difference.
    __ Bind(&different_compression_diff);
    __ Tst(temp0, Operand(1));
    static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                  "Expecting 0=compressed, 1=uncompressed");
    __ Cneg(out, temp4, ne);
  }

  __ Bind(&end);

  if (can_slow_path) {
    __ Bind(slow_path->GetExitLabel());
  }
}

// The cut off for unrolling the loop in String.equals() intrinsic for const strings.
// The normal loop plus the pre-header is 9 instructions without string compression and 12
// instructions with string compression. We can compare up to 8 bytes in 4 instructions
// (LDR+LDR+CMP+BNE) and up to 16 bytes in 5 instructions (LDP+LDP+CMP+CCMP+BNE). Allow up
// to 10 instructions for the unrolled loop.
constexpr size_t kShortConstStringEqualsCutoffInBytes = 32;

static const char* GetConstString(HInstruction* candidate, uint32_t* utf16_length) {
  if (candidate->IsLoadString()) {
    HLoadString* load_string = candidate->AsLoadString();
    const DexFile& dex_file = load_string->GetDexFile();
    return dex_file.StringDataAndUtf16LengthByIdx(load_string->GetStringIndex(), utf16_length);
  }
  return nullptr;
}

void IntrinsicLocationsBuilderARM64::VisitStringEquals(HInvoke* invoke) {
  if (kEmitCompilerReadBarrier &&
      !StringEqualsOptimizations(invoke).GetArgumentIsString() &&
      !StringEqualsOptimizations(invoke).GetNoReadBarrierForStringClass()) {
    // No support for this odd case (String class is moveable, not in the boot image).
    return;
  }

  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());

  // For the generic implementation and for long const strings we need a temporary.
  // We do not need it for short const strings, up to 8 bytes, see code generation below.
  uint32_t const_string_length = 0u;
  const char* const_string = GetConstString(invoke->InputAt(0), &const_string_length);
  if (const_string == nullptr) {
    const_string = GetConstString(invoke->InputAt(1), &const_string_length);
  }
  bool is_compressed =
      mirror::kUseStringCompression &&
      const_string != nullptr &&
      mirror::String::DexFileStringAllASCII(const_string, const_string_length);
  if (const_string == nullptr || const_string_length > (is_compressed ? 8u : 4u)) {
    locations->AddTemp(Location::RequiresRegister());
  }

  // TODO: If the String.equals() is used only for an immediately following HIf, we can
  // mark it as emitted-at-use-site and emit branches directly to the appropriate blocks.
  // Then we shall need an extra temporary register instead of the output register.
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorARM64::VisitStringEquals(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register str = WRegisterFrom(locations->InAt(0));
  Register arg = WRegisterFrom(locations->InAt(1));
  Register out = XRegisterFrom(locations->Out());

  UseScratchRegisterScope scratch_scope(masm);
  Register temp = scratch_scope.AcquireW();
  Register temp1 = scratch_scope.AcquireW();

  vixl::aarch64::Label loop;
  vixl::aarch64::Label end;
  vixl::aarch64::Label return_true;
  vixl::aarch64::Label return_false;

  // Get offsets of count, value, and class fields within a string object.
  const int32_t count_offset = mirror::String::CountOffset().Int32Value();
  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();
  const int32_t class_offset = mirror::Object::ClassOffset().Int32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  StringEqualsOptimizations optimizations(invoke);
  if (!optimizations.GetArgumentNotNull()) {
    // Check if input is null, return false if it is.
    __ Cbz(arg, &return_false);
  }

  // Reference equality check, return true if same reference.
  __ Cmp(str, arg);
  __ B(&return_true, eq);

  if (!optimizations.GetArgumentIsString()) {
    // Instanceof check for the argument by comparing class fields.
    // All string objects must have the same type since String cannot be subclassed.
    // Receiver must be a string object, so its class field is equal to all strings' class fields.
    // If the argument is a string object, its class field must be equal to receiver's class field.
    __ Ldr(temp, MemOperand(str.X(), class_offset));
    __ Ldr(temp1, MemOperand(arg.X(), class_offset));
    __ Cmp(temp, temp1);
    __ B(&return_false, ne);
  }

  // Check if one of the inputs is a const string. Do not special-case both strings
  // being const, such cases should be handled by constant folding if needed.
  uint32_t const_string_length = 0u;
  const char* const_string = GetConstString(invoke->InputAt(0), &const_string_length);
  if (const_string == nullptr) {
    const_string = GetConstString(invoke->InputAt(1), &const_string_length);
    if (const_string != nullptr) {
      std::swap(str, arg);  // Make sure the const string is in `str`.
    }
  }
  bool is_compressed =
      mirror::kUseStringCompression &&
      const_string != nullptr &&
      mirror::String::DexFileStringAllASCII(const_string, const_string_length);

  if (const_string != nullptr) {
    // Load `count` field of the argument string and check if it matches the const string.
    // Also compares the compression style, if differs return false.
    __ Ldr(temp, MemOperand(arg.X(), count_offset));
    // Temporarily release temp1 as we may not be able to embed the flagged count in CMP immediate.
    scratch_scope.Release(temp1);
    __ Cmp(temp, Operand(mirror::String::GetFlaggedCount(const_string_length, is_compressed)));
    temp1 = scratch_scope.AcquireW();
    __ B(&return_false, ne);
  } else {
    // Load `count` fields of this and argument strings.
    __ Ldr(temp, MemOperand(str.X(), count_offset));
    __ Ldr(temp1, MemOperand(arg.X(), count_offset));
    // Check if `count` fields are equal, return false if they're not.
    // Also compares the compression style, if differs return false.
    __ Cmp(temp, temp1);
    __ B(&return_false, ne);
  }

  // Assertions that must hold in order to compare strings 8 bytes at a time.
  // Ok to do this because strings are zero-padded to kObjectAlignment.
  DCHECK_ALIGNED(value_offset, 8);
  static_assert(IsAligned<8>(kObjectAlignment), "String of odd length is not zero padded");

  if (const_string != nullptr &&
      const_string_length <= (is_compressed ? kShortConstStringEqualsCutoffInBytes
                                            : kShortConstStringEqualsCutoffInBytes / 2u)) {
    // Load and compare the contents. Though we know the contents of the short const string
    // at compile time, materializing constants may be more code than loading from memory.
    int32_t offset = value_offset;
    size_t remaining_bytes =
        RoundUp(is_compressed ? const_string_length : const_string_length * 2u, 8u);
    temp = temp.X();
    temp1 = temp1.X();
    while (remaining_bytes > sizeof(uint64_t)) {
      Register temp2 = XRegisterFrom(locations->GetTemp(0));
      __ Ldp(temp, temp1, MemOperand(str.X(), offset));
      __ Ldp(temp2, out, MemOperand(arg.X(), offset));
      __ Cmp(temp, temp2);
      __ Ccmp(temp1, out, NoFlag, eq);
      __ B(&return_false, ne);
      offset += 2u * sizeof(uint64_t);
      remaining_bytes -= 2u * sizeof(uint64_t);
    }
    if (remaining_bytes != 0u) {
      __ Ldr(temp, MemOperand(str.X(), offset));
      __ Ldr(temp1, MemOperand(arg.X(), offset));
      __ Cmp(temp, temp1);
      __ B(&return_false, ne);
    }
  } else {
    // Return true if both strings are empty. Even with string compression `count == 0` means empty.
    static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                  "Expecting 0=compressed, 1=uncompressed");
    __ Cbz(temp, &return_true);

    if (mirror::kUseStringCompression) {
      // For string compression, calculate the number of bytes to compare (not chars).
      // This could in theory exceed INT32_MAX, so treat temp as unsigned.
      __ And(temp1, temp, Operand(1));    // Extract compression flag.
      __ Lsr(temp, temp, 1u);             // Extract length.
      __ Lsl(temp, temp, temp1);          // Calculate number of bytes to compare.
    }

    // Store offset of string value in preparation for comparison loop
    __ Mov(temp1, value_offset);

    temp1 = temp1.X();
    Register temp2 = XRegisterFrom(locations->GetTemp(0));
    // Loop to compare strings 8 bytes at a time starting at the front of the string.
    __ Bind(&loop);
    __ Ldr(out, MemOperand(str.X(), temp1));
    __ Ldr(temp2, MemOperand(arg.X(), temp1));
    __ Add(temp1, temp1, Operand(sizeof(uint64_t)));
    __ Cmp(out, temp2);
    __ B(&return_false, ne);
    // With string compression, we have compared 8 bytes, otherwise 4 chars.
    __ Sub(temp, temp, Operand(mirror::kUseStringCompression ? 8 : 4), SetFlags);
    __ B(&loop, hi);
  }

  // Return true and exit the function.
  // If loop does not result in returning false, we return true.
  __ Bind(&return_true);
  __ Mov(out, 1);
  __ B(&end);

  // Return false and exit the function.
  __ Bind(&return_false);
  __ Mov(out, 0);
  __ Bind(&end);
}

static void GenerateVisitStringIndexOf(HInvoke* invoke,
                                       MacroAssembler* masm,
                                       CodeGeneratorARM64* codegen,
                                       bool start_at_zero) {
  LocationSummary* locations = invoke->GetLocations();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // Check for code points > 0xFFFF. Either a slow-path check when we don't know statically,
  // or directly dispatch for a large constant, or omit slow-path for a small constant or a char.
  SlowPathCodeARM64* slow_path = nullptr;
  HInstruction* code_point = invoke->InputAt(1);
  if (code_point->IsIntConstant()) {
    if (static_cast<uint32_t>(code_point->AsIntConstant()->GetValue()) > 0xFFFFU) {
      // Always needs the slow-path. We could directly dispatch to it, but this case should be
      // rare, so for simplicity just put the full slow-path down and branch unconditionally.
      slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
      codegen->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
  } else if (code_point->GetType() != DataType::Type::kUint16) {
    Register char_reg = WRegisterFrom(locations->InAt(1));
    __ Tst(char_reg, 0xFFFF0000);
    slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
    codegen->AddSlowPath(slow_path);
    __ B(ne, slow_path->GetEntryLabel());
  }

  if (start_at_zero) {
    // Start-index = 0.
    Register tmp_reg = WRegisterFrom(locations->GetTemp(0));
    __ Mov(tmp_reg, 0);
  }

  codegen->InvokeRuntime(kQuickIndexOf, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickIndexOf, int32_t, void*, uint32_t, uint32_t>();

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void IntrinsicLocationsBuilderARM64::VisitStringIndexOf(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime calling convention. So it's
  // best to align the inputs accordingly.
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kInt32));

  // Need to send start_index=0.
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(2)));
}

void IntrinsicCodeGeneratorARM64::VisitStringIndexOf(HInvoke* invoke) {
  GenerateVisitStringIndexOf(invoke, GetVIXLAssembler(), codegen_, /* start_at_zero */ true);
}

void IntrinsicLocationsBuilderARM64::VisitStringIndexOfAfter(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime calling convention. So it's
  // best to align the inputs accordingly.
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kInt32));
}

void IntrinsicCodeGeneratorARM64::VisitStringIndexOfAfter(HInvoke* invoke) {
  GenerateVisitStringIndexOf(invoke, GetVIXLAssembler(), codegen_, /* start_at_zero */ false);
}

void IntrinsicLocationsBuilderARM64::VisitStringNewStringFromBytes(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetInAt(3, LocationFrom(calling_convention.GetRegisterAt(3)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
}

void IntrinsicCodeGeneratorARM64::VisitStringNewStringFromBytes(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register byte_array = WRegisterFrom(locations->InAt(0));
  __ Cmp(byte_array, 0);
  SlowPathCodeARM64* slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(slow_path);
  __ B(eq, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromBytes, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickAllocStringFromBytes, void*, void*, int32_t, int32_t, int32_t>();
  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderARM64::VisitStringNewStringFromChars(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
}

void IntrinsicCodeGeneratorARM64::VisitStringNewStringFromChars(HInvoke* invoke) {
  // No need to emit code checking whether `locations->InAt(2)` is a null
  // pointer, as callers of the native method
  //
  //   java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
  //
  // all include a null check on `data` before calling that method.
  codegen_->InvokeRuntime(kQuickAllocStringFromChars, invoke, invoke->GetDexPc());
  CheckEntrypointTypes<kQuickAllocStringFromChars, void*, int32_t, int32_t, void*>();
}

void IntrinsicLocationsBuilderARM64::VisitStringNewStringFromString(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kReference));
}

void IntrinsicCodeGeneratorARM64::VisitStringNewStringFromString(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register string_to_copy = WRegisterFrom(locations->InAt(0));
  __ Cmp(string_to_copy, 0);
  SlowPathCodeARM64* slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(slow_path);
  __ B(eq, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromString, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickAllocStringFromString, void*, void*>();
  __ Bind(slow_path->GetExitLabel());
}

static void CreateFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  DCHECK_EQ(invoke->GetNumberOfArguments(), 1U);
  DCHECK(DataType::IsFloatingPointType(invoke->InputAt(0)->GetType()));
  DCHECK(DataType::IsFloatingPointType(invoke->GetType()));

  LocationSummary* const locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;

  locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0)));
  locations->SetOut(calling_convention.GetReturnLocation(invoke->GetType()));
}

static void CreateFPFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  DCHECK_EQ(invoke->GetNumberOfArguments(), 2U);
  DCHECK(DataType::IsFloatingPointType(invoke->InputAt(0)->GetType()));
  DCHECK(DataType::IsFloatingPointType(invoke->InputAt(1)->GetType()));
  DCHECK(DataType::IsFloatingPointType(invoke->GetType()));

  LocationSummary* const locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;

  locations->SetInAt(0, LocationFrom(calling_convention.GetFpuRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetFpuRegisterAt(1)));
  locations->SetOut(calling_convention.GetReturnLocation(invoke->GetType()));
}

static void GenFPToFPCall(HInvoke* invoke,
                          CodeGeneratorARM64* codegen,
                          QuickEntrypointEnum entry) {
  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());
}

void IntrinsicLocationsBuilderARM64::VisitMathCos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCos);
}

void IntrinsicLocationsBuilderARM64::VisitMathSin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathSin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSin);
}

void IntrinsicLocationsBuilderARM64::VisitMathAcos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAcos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAcos);
}

void IntrinsicLocationsBuilderARM64::VisitMathAsin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAsin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAsin);
}

void IntrinsicLocationsBuilderARM64::VisitMathAtan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAtan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAtan);
}

void IntrinsicLocationsBuilderARM64::VisitMathCbrt(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCbrt(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCbrt);
}

void IntrinsicLocationsBuilderARM64::VisitMathCosh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathCosh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCosh);
}

void IntrinsicLocationsBuilderARM64::VisitMathExp(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathExp(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExp);
}

void IntrinsicLocationsBuilderARM64::VisitMathExpm1(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathExpm1(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExpm1);
}

void IntrinsicLocationsBuilderARM64::VisitMathLog(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathLog(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog);
}

void IntrinsicLocationsBuilderARM64::VisitMathLog10(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathLog10(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog10);
}

void IntrinsicLocationsBuilderARM64::VisitMathSinh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathSinh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSinh);
}

void IntrinsicLocationsBuilderARM64::VisitMathTan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathTan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTan);
}

void IntrinsicLocationsBuilderARM64::VisitMathTanh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathTanh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTanh);
}

void IntrinsicLocationsBuilderARM64::VisitMathAtan2(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathAtan2(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAtan2);
}

void IntrinsicLocationsBuilderARM64::VisitMathPow(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathPow(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickPow);
}

void IntrinsicLocationsBuilderARM64::VisitMathHypot(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathHypot(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickHypot);
}

void IntrinsicLocationsBuilderARM64::VisitMathNextAfter(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitMathNextAfter(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickNextAfter);
}

void IntrinsicLocationsBuilderARM64::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  locations->SetInAt(4, Location::RequiresRegister());

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  // Check assumption that sizeof(Char) is 2 (used in scaling below).
  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  // Location of data in char array buffer.
  const uint32_t data_offset = mirror::Array::DataOffset(char_size).Uint32Value();

  // Location of char array data in string.
  const uint32_t value_offset = mirror::String::ValueOffset().Uint32Value();

  // void getCharsNoCheck(int srcBegin, int srcEnd, char[] dst, int dstBegin);
  // Since getChars() calls getCharsNoCheck() - we use registers rather than constants.
  Register srcObj = XRegisterFrom(locations->InAt(0));
  Register srcBegin = XRegisterFrom(locations->InAt(1));
  Register srcEnd = XRegisterFrom(locations->InAt(2));
  Register dstObj = XRegisterFrom(locations->InAt(3));
  Register dstBegin = XRegisterFrom(locations->InAt(4));

  Register src_ptr = XRegisterFrom(locations->GetTemp(0));
  Register num_chr = XRegisterFrom(locations->GetTemp(1));
  Register tmp1 = XRegisterFrom(locations->GetTemp(2));

  UseScratchRegisterScope temps(masm);
  Register dst_ptr = temps.AcquireX();
  Register tmp2 = temps.AcquireX();

  vixl::aarch64::Label done;
  vixl::aarch64::Label compressed_string_loop;
  __ Sub(num_chr, srcEnd, srcBegin);
  // Early out for valid zero-length retrievals.
  __ Cbz(num_chr, &done);

  // dst address start to copy to.
  __ Add(dst_ptr, dstObj, Operand(data_offset));
  __ Add(dst_ptr, dst_ptr, Operand(dstBegin, LSL, 1));

  // src address to copy from.
  __ Add(src_ptr, srcObj, Operand(value_offset));
  vixl::aarch64::Label compressed_string_preloop;
  if (mirror::kUseStringCompression) {
    // Location of count in string.
    const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
    // String's length.
    __ Ldr(tmp2, MemOperand(srcObj, count_offset));
    __ Tbz(tmp2, 0, &compressed_string_preloop);
  }
  __ Add(src_ptr, src_ptr, Operand(srcBegin, LSL, 1));

  // Do the copy.
  vixl::aarch64::Label loop;
  vixl::aarch64::Label remainder;

  // Save repairing the value of num_chr on the < 8 character path.
  __ Subs(tmp1, num_chr, 8);
  __ B(lt, &remainder);

  // Keep the result of the earlier subs, we are going to fetch at least 8 characters.
  __ Mov(num_chr, tmp1);

  // Main loop used for longer fetches loads and stores 8x16-bit characters at a time.
  // (Unaligned addresses are acceptable here and not worth inlining extra code to rectify.)
  __ Bind(&loop);
  __ Ldp(tmp1, tmp2, MemOperand(src_ptr, char_size * 8, PostIndex));
  __ Subs(num_chr, num_chr, 8);
  __ Stp(tmp1, tmp2, MemOperand(dst_ptr, char_size * 8, PostIndex));
  __ B(ge, &loop);

  __ Adds(num_chr, num_chr, 8);
  __ B(eq, &done);

  // Main loop for < 8 character case and remainder handling. Loads and stores one
  // 16-bit Java character at a time.
  __ Bind(&remainder);
  __ Ldrh(tmp1, MemOperand(src_ptr, char_size, PostIndex));
  __ Subs(num_chr, num_chr, 1);
  __ Strh(tmp1, MemOperand(dst_ptr, char_size, PostIndex));
  __ B(gt, &remainder);
  __ B(&done);

  if (mirror::kUseStringCompression) {
    const size_t c_char_size = DataType::Size(DataType::Type::kInt8);
    DCHECK_EQ(c_char_size, 1u);
    __ Bind(&compressed_string_preloop);
    __ Add(src_ptr, src_ptr, Operand(srcBegin));
    // Copy loop for compressed src, copying 1 character (8-bit) to (16-bit) at a time.
    __ Bind(&compressed_string_loop);
    __ Ldrb(tmp1, MemOperand(src_ptr, c_char_size, PostIndex));
    __ Strh(tmp1, MemOperand(dst_ptr, char_size, PostIndex));
    __ Subs(num_chr, num_chr, Operand(1));
    __ B(gt, &compressed_string_loop);
  }

  __ Bind(&done);
}

// Mirrors ARRAYCOPY_SHORT_CHAR_ARRAY_THRESHOLD in libcore, so we can choose to use the native
// implementation there for longer copy lengths.
static constexpr int32_t kSystemArrayCopyCharThreshold = 32;

static void SetSystemArrayCopyLocationRequires(LocationSummary* locations,
                                               uint32_t at,
                                               HInstruction* input) {
  HIntConstant* const_input = input->AsIntConstant();
  if (const_input != nullptr && !vixl::aarch64::Assembler::IsImmAddSub(const_input->GetValue())) {
    locations->SetInAt(at, Location::RequiresRegister());
  } else {
    locations->SetInAt(at, Location::RegisterOrConstant(input));
  }
}

void IntrinsicLocationsBuilderARM64::VisitSystemArrayCopyChar(HInvoke* invoke) {
  // Check to see if we have known failures that will cause us to have to bail out
  // to the runtime, and just generate the runtime call directly.
  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dst_pos = invoke->InputAt(3)->AsIntConstant();

  // The positions must be non-negative.
  if ((src_pos != nullptr && src_pos->GetValue() < 0) ||
      (dst_pos != nullptr && dst_pos->GetValue() < 0)) {
    // We will have to fail anyways.
    return;
  }

  // The length must be >= 0 and not so long that we would (currently) prefer libcore's
  // native implementation.
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();
  if (length != nullptr) {
    int32_t len = length->GetValue();
    if (len < 0 || len > kSystemArrayCopyCharThreshold) {
      // Just call as normal.
      return;
    }
  }

  ArenaAllocator* allocator = invoke->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnSlowPath, kIntrinsified);
  // arraycopy(char[] src, int src_pos, char[] dst, int dst_pos, int length).
  locations->SetInAt(0, Location::RequiresRegister());
  SetSystemArrayCopyLocationRequires(locations, 1, invoke->InputAt(1));
  locations->SetInAt(2, Location::RequiresRegister());
  SetSystemArrayCopyLocationRequires(locations, 3, invoke->InputAt(3));
  SetSystemArrayCopyLocationRequires(locations, 4, invoke->InputAt(4));

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

static void CheckSystemArrayCopyPosition(MacroAssembler* masm,
                                         const Location& pos,
                                         const Register& input,
                                         const Location& length,
                                         SlowPathCodeARM64* slow_path,
                                         const Register& temp,
                                         bool length_is_input_length = false) {
  const int32_t length_offset = mirror::Array::LengthOffset().Int32Value();
  if (pos.IsConstant()) {
    int32_t pos_const = pos.GetConstant()->AsIntConstant()->GetValue();
    if (pos_const == 0) {
      if (!length_is_input_length) {
        // Check that length(input) >= length.
        __ Ldr(temp, MemOperand(input, length_offset));
        __ Cmp(temp, OperandFrom(length, DataType::Type::kInt32));
        __ B(slow_path->GetEntryLabel(), lt);
      }
    } else {
      // Check that length(input) >= pos.
      __ Ldr(temp, MemOperand(input, length_offset));
      __ Subs(temp, temp, pos_const);
      __ B(slow_path->GetEntryLabel(), lt);

      // Check that (length(input) - pos) >= length.
      __ Cmp(temp, OperandFrom(length, DataType::Type::kInt32));
      __ B(slow_path->GetEntryLabel(), lt);
    }
  } else if (length_is_input_length) {
    // The only way the copy can succeed is if pos is zero.
    __ Cbnz(WRegisterFrom(pos), slow_path->GetEntryLabel());
  } else {
    // Check that pos >= 0.
    Register pos_reg = WRegisterFrom(pos);
    __ Tbnz(pos_reg, pos_reg.GetSizeInBits() - 1, slow_path->GetEntryLabel());

    // Check that pos <= length(input) && (length(input) - pos) >= length.
    __ Ldr(temp, MemOperand(input, length_offset));
    __ Subs(temp, temp, pos_reg);
    // Ccmp if length(input) >= pos, else definitely bail to slow path (N!=V == lt).
    __ Ccmp(temp, OperandFrom(length, DataType::Type::kInt32), NFlag, ge);
    __ B(slow_path->GetEntryLabel(), lt);
  }
}

// Compute base source address, base destination address, and end
// source address for System.arraycopy* intrinsics in `src_base`,
// `dst_base` and `src_end` respectively.
static void GenSystemArrayCopyAddresses(MacroAssembler* masm,
                                        DataType::Type type,
                                        const Register& src,
                                        const Location& src_pos,
                                        const Register& dst,
                                        const Location& dst_pos,
                                        const Location& copy_length,
                                        const Register& src_base,
                                        const Register& dst_base,
                                        const Register& src_end) {
  // This routine is used by the SystemArrayCopy and the SystemArrayCopyChar intrinsics.
  DCHECK(type == DataType::Type::kReference || type == DataType::Type::kUint16)
      << "Unexpected element type: " << type;
  const int32_t element_size = DataType::Size(type);
  const int32_t element_size_shift = DataType::SizeShift(type);
  const uint32_t data_offset = mirror::Array::DataOffset(element_size).Uint32Value();

  if (src_pos.IsConstant()) {
    int32_t constant = src_pos.GetConstant()->AsIntConstant()->GetValue();
    __ Add(src_base, src, element_size * constant + data_offset);
  } else {
    __ Add(src_base, src, data_offset);
    __ Add(src_base, src_base, Operand(XRegisterFrom(src_pos), LSL, element_size_shift));
  }

  if (dst_pos.IsConstant()) {
    int32_t constant = dst_pos.GetConstant()->AsIntConstant()->GetValue();
    __ Add(dst_base, dst, element_size * constant + data_offset);
  } else {
    __ Add(dst_base, dst, data_offset);
    __ Add(dst_base, dst_base, Operand(XRegisterFrom(dst_pos), LSL, element_size_shift));
  }

  if (copy_length.IsConstant()) {
    int32_t constant = copy_length.GetConstant()->AsIntConstant()->GetValue();
    __ Add(src_end, src_base, element_size * constant);
  } else {
    __ Add(src_end, src_base, Operand(XRegisterFrom(copy_length), LSL, element_size_shift));
  }
}

void IntrinsicCodeGeneratorARM64::VisitSystemArrayCopyChar(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();
  Register src = XRegisterFrom(locations->InAt(0));
  Location src_pos = locations->InAt(1);
  Register dst = XRegisterFrom(locations->InAt(2));
  Location dst_pos = locations->InAt(3);
  Location length = locations->InAt(4);

  SlowPathCodeARM64* slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(slow_path);

  // If source and destination are the same, take the slow path. Overlapping copy regions must be
  // copied in reverse and we can't know in all cases if it's needed.
  __ Cmp(src, dst);
  __ B(slow_path->GetEntryLabel(), eq);

  // Bail out if the source is null.
  __ Cbz(src, slow_path->GetEntryLabel());

  // Bail out if the destination is null.
  __ Cbz(dst, slow_path->GetEntryLabel());

  if (!length.IsConstant()) {
    // Merge the following two comparisons into one:
    //   If the length is negative, bail out (delegate to libcore's native implementation).
    //   If the length > 32 then (currently) prefer libcore's native implementation.
    __ Cmp(WRegisterFrom(length), kSystemArrayCopyCharThreshold);
    __ B(slow_path->GetEntryLabel(), hi);
  } else {
    // We have already checked in the LocationsBuilder for the constant case.
    DCHECK_GE(length.GetConstant()->AsIntConstant()->GetValue(), 0);
    DCHECK_LE(length.GetConstant()->AsIntConstant()->GetValue(), 32);
  }

  Register src_curr_addr = WRegisterFrom(locations->GetTemp(0));
  Register dst_curr_addr = WRegisterFrom(locations->GetTemp(1));
  Register src_stop_addr = WRegisterFrom(locations->GetTemp(2));

  CheckSystemArrayCopyPosition(masm,
                               src_pos,
                               src,
                               length,
                               slow_path,
                               src_curr_addr,
                               false);

  CheckSystemArrayCopyPosition(masm,
                               dst_pos,
                               dst,
                               length,
                               slow_path,
                               src_curr_addr,
                               false);

  src_curr_addr = src_curr_addr.X();
  dst_curr_addr = dst_curr_addr.X();
  src_stop_addr = src_stop_addr.X();

  GenSystemArrayCopyAddresses(masm,
                              DataType::Type::kUint16,
                              src,
                              src_pos,
                              dst,
                              dst_pos,
                              length,
                              src_curr_addr,
                              dst_curr_addr,
                              src_stop_addr);

  // Iterate over the arrays and do a raw copy of the chars.
  const int32_t char_size = DataType::Size(DataType::Type::kUint16);
  UseScratchRegisterScope temps(masm);
  Register tmp = temps.AcquireW();
  vixl::aarch64::Label loop, done;
  __ Bind(&loop);
  __ Cmp(src_curr_addr, src_stop_addr);
  __ B(&done, eq);
  __ Ldrh(tmp, MemOperand(src_curr_addr, char_size, PostIndex));
  __ Strh(tmp, MemOperand(dst_curr_addr, char_size, PostIndex));
  __ B(&loop);
  __ Bind(&done);

  __ Bind(slow_path->GetExitLabel());
}

// We can choose to use the native implementation there for longer copy lengths.
static constexpr int32_t kSystemArrayCopyThreshold = 128;

// CodeGenerator::CreateSystemArrayCopyLocationSummary use three temporary registers.
// We want to use two temporary registers in order to reduce the register pressure in arm64.
// So we don't use the CodeGenerator::CreateSystemArrayCopyLocationSummary.
void IntrinsicLocationsBuilderARM64::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  // Check to see if we have known failures that will cause us to have to bail out
  // to the runtime, and just generate the runtime call directly.
  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dest_pos = invoke->InputAt(3)->AsIntConstant();

  // The positions must be non-negative.
  if ((src_pos != nullptr && src_pos->GetValue() < 0) ||
      (dest_pos != nullptr && dest_pos->GetValue() < 0)) {
    // We will have to fail anyways.
    return;
  }

  // The length must be >= 0.
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();
  if (length != nullptr) {
    int32_t len = length->GetValue();
    if (len < 0 || len >= kSystemArrayCopyThreshold) {
      // Just call as normal.
      return;
    }
  }

  SystemArrayCopyOptimizations optimizations(invoke);

  if (optimizations.GetDestinationIsSource()) {
    if (src_pos != nullptr && dest_pos != nullptr && src_pos->GetValue() < dest_pos->GetValue()) {
      // We only support backward copying if source and destination are the same.
      return;
    }
  }

  if (optimizations.GetDestinationIsPrimitiveArray() || optimizations.GetSourceIsPrimitiveArray()) {
    // We currently don't intrinsify primitive copying.
    return;
  }

  ArenaAllocator* allocator = invoke->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnSlowPath, kIntrinsified);
  // arraycopy(Object src, int src_pos, Object dest, int dest_pos, int length).
  locations->SetInAt(0, Location::RequiresRegister());
  SetSystemArrayCopyLocationRequires(locations, 1, invoke->InputAt(1));
  locations->SetInAt(2, Location::RequiresRegister());
  SetSystemArrayCopyLocationRequires(locations, 3, invoke->InputAt(3));
  SetSystemArrayCopyLocationRequires(locations, 4, invoke->InputAt(4));

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // Temporary register IP0, obtained from the VIXL scratch register
    // pool, cannot be used in ReadBarrierSystemArrayCopySlowPathARM64
    // (because that register is clobbered by ReadBarrierMarkRegX
    // entry points). It cannot be used in calls to
    // CodeGeneratorARM64::GenerateFieldLoadWithBakerReadBarrier
    // either. For these reasons, get a third extra temporary register
    // from the register allocator.
    locations->AddTemp(Location::RequiresRegister());
  } else {
    // Cases other than Baker read barriers: the third temporary will
    // be acquired from the VIXL scratch register pool.
  }
}

void IntrinsicCodeGeneratorARM64::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  MacroAssembler* masm = GetVIXLAssembler();
  LocationSummary* locations = invoke->GetLocations();

  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();

  Register src = XRegisterFrom(locations->InAt(0));
  Location src_pos = locations->InAt(1);
  Register dest = XRegisterFrom(locations->InAt(2));
  Location dest_pos = locations->InAt(3);
  Location length = locations->InAt(4);
  Register temp1 = WRegisterFrom(locations->GetTemp(0));
  Location temp1_loc = LocationFrom(temp1);
  Register temp2 = WRegisterFrom(locations->GetTemp(1));
  Location temp2_loc = LocationFrom(temp2);

  SlowPathCodeARM64* intrinsic_slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARM64(invoke);
  codegen_->AddSlowPath(intrinsic_slow_path);

  vixl::aarch64::Label conditions_on_positions_validated;
  SystemArrayCopyOptimizations optimizations(invoke);

  // If source and destination are the same, we go to slow path if we need to do
  // forward copying.
  if (src_pos.IsConstant()) {
    int32_t src_pos_constant = src_pos.GetConstant()->AsIntConstant()->GetValue();
    if (dest_pos.IsConstant()) {
      int32_t dest_pos_constant = dest_pos.GetConstant()->AsIntConstant()->GetValue();
      if (optimizations.GetDestinationIsSource()) {
        // Checked when building locations.
        DCHECK_GE(src_pos_constant, dest_pos_constant);
      } else if (src_pos_constant < dest_pos_constant) {
        __ Cmp(src, dest);
        __ B(intrinsic_slow_path->GetEntryLabel(), eq);
      }
      // Checked when building locations.
      DCHECK(!optimizations.GetDestinationIsSource()
             || (src_pos_constant >= dest_pos.GetConstant()->AsIntConstant()->GetValue()));
    } else {
      if (!optimizations.GetDestinationIsSource()) {
        __ Cmp(src, dest);
        __ B(&conditions_on_positions_validated, ne);
      }
      __ Cmp(WRegisterFrom(dest_pos), src_pos_constant);
      __ B(intrinsic_slow_path->GetEntryLabel(), gt);
    }
  } else {
    if (!optimizations.GetDestinationIsSource()) {
      __ Cmp(src, dest);
      __ B(&conditions_on_positions_validated, ne);
    }
    __ Cmp(RegisterFrom(src_pos, invoke->InputAt(1)->GetType()),
           OperandFrom(dest_pos, invoke->InputAt(3)->GetType()));
    __ B(intrinsic_slow_path->GetEntryLabel(), lt);
  }

  __ Bind(&conditions_on_positions_validated);

  if (!optimizations.GetSourceIsNotNull()) {
    // Bail out if the source is null.
    __ Cbz(src, intrinsic_slow_path->GetEntryLabel());
  }

  if (!optimizations.GetDestinationIsNotNull() && !optimizations.GetDestinationIsSource()) {
    // Bail out if the destination is null.
    __ Cbz(dest, intrinsic_slow_path->GetEntryLabel());
  }

  // We have already checked in the LocationsBuilder for the constant case.
  if (!length.IsConstant() &&
      !optimizations.GetCountIsSourceLength() &&
      !optimizations.GetCountIsDestinationLength()) {
    // Merge the following two comparisons into one:
    //   If the length is negative, bail out (delegate to libcore's native implementation).
    //   If the length >= 128 then (currently) prefer native implementation.
    __ Cmp(WRegisterFrom(length), kSystemArrayCopyThreshold);
    __ B(intrinsic_slow_path->GetEntryLabel(), hs);
  }
  // Validity checks: source.
  CheckSystemArrayCopyPosition(masm,
                               src_pos,
                               src,
                               length,
                               intrinsic_slow_path,
                               temp1,
                               optimizations.GetCountIsSourceLength());

  // Validity checks: dest.
  CheckSystemArrayCopyPosition(masm,
                               dest_pos,
                               dest,
                               length,
                               intrinsic_slow_path,
                               temp1,
                               optimizations.GetCountIsDestinationLength());
  {
    // We use a block to end the scratch scope before the write barrier, thus
    // freeing the temporary registers so they can be used in `MarkGCCard`.
    UseScratchRegisterScope temps(masm);
    Location temp3_loc;  // Used only for Baker read barrier.
    Register temp3;
    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      temp3_loc = locations->GetTemp(2);
      temp3 = WRegisterFrom(temp3_loc);
    } else {
      temp3 = temps.AcquireW();
    }

    if (!optimizations.GetDoesNotNeedTypeCheck()) {
      // Check whether all elements of the source array are assignable to the component
      // type of the destination array. We do two checks: the classes are the same,
      // or the destination is Object[]. If none of these checks succeed, we go to the
      // slow path.

      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        if (!optimizations.GetSourceIsNonPrimitiveArray()) {
          // /* HeapReference<Class> */ temp1 = src->klass_
          codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                          temp1_loc,
                                                          src.W(),
                                                          class_offset,
                                                          temp3_loc,
                                                          /* needs_null_check */ false,
                                                          /* use_load_acquire */ false);
          // Bail out if the source is not a non primitive array.
          // /* HeapReference<Class> */ temp1 = temp1->component_type_
          codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                          temp1_loc,
                                                          temp1,
                                                          component_offset,
                                                          temp3_loc,
                                                          /* needs_null_check */ false,
                                                          /* use_load_acquire */ false);
          __ Cbz(temp1, intrinsic_slow_path->GetEntryLabel());
          // If heap poisoning is enabled, `temp1` has been unpoisoned
          // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
          // /* uint16_t */ temp1 = static_cast<uint16>(temp1->primitive_type_);
          __ Ldrh(temp1, HeapOperand(temp1, primitive_offset));
          static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
          __ Cbnz(temp1, intrinsic_slow_path->GetEntryLabel());
        }

        // /* HeapReference<Class> */ temp1 = dest->klass_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                        temp1_loc,
                                                        dest.W(),
                                                        class_offset,
                                                        temp3_loc,
                                                        /* needs_null_check */ false,
                                                        /* use_load_acquire */ false);

        if (!optimizations.GetDestinationIsNonPrimitiveArray()) {
          // Bail out if the destination is not a non primitive array.
          //
          // Register `temp1` is not trashed by the read barrier emitted
          // by GenerateFieldLoadWithBakerReadBarrier below, as that
          // method produces a call to a ReadBarrierMarkRegX entry point,
          // which saves all potentially live registers, including
          // temporaries such a `temp1`.
          // /* HeapReference<Class> */ temp2 = temp1->component_type_
          codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                          temp2_loc,
                                                          temp1,
                                                          component_offset,
                                                          temp3_loc,
                                                          /* needs_null_check */ false,
                                                          /* use_load_acquire */ false);
          __ Cbz(temp2, intrinsic_slow_path->GetEntryLabel());
          // If heap poisoning is enabled, `temp2` has been unpoisoned
          // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
          // /* uint16_t */ temp2 = static_cast<uint16>(temp2->primitive_type_);
          __ Ldrh(temp2, HeapOperand(temp2, primitive_offset));
          static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
          __ Cbnz(temp2, intrinsic_slow_path->GetEntryLabel());
        }

        // For the same reason given earlier, `temp1` is not trashed by the
        // read barrier emitted by GenerateFieldLoadWithBakerReadBarrier below.
        // /* HeapReference<Class> */ temp2 = src->klass_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                        temp2_loc,
                                                        src.W(),
                                                        class_offset,
                                                        temp3_loc,
                                                        /* needs_null_check */ false,
                                                        /* use_load_acquire */ false);
        // Note: if heap poisoning is on, we are comparing two unpoisoned references here.
        __ Cmp(temp1, temp2);

        if (optimizations.GetDestinationIsTypedObjectArray()) {
          vixl::aarch64::Label do_copy;
          __ B(&do_copy, eq);
          // /* HeapReference<Class> */ temp1 = temp1->component_type_
          codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                          temp1_loc,
                                                          temp1,
                                                          component_offset,
                                                          temp3_loc,
                                                          /* needs_null_check */ false,
                                                          /* use_load_acquire */ false);
          // /* HeapReference<Class> */ temp1 = temp1->super_class_
          // We do not need to emit a read barrier for the following
          // heap reference load, as `temp1` is only used in a
          // comparison with null below, and this reference is not
          // kept afterwards.
          __ Ldr(temp1, HeapOperand(temp1, super_offset));
          __ Cbnz(temp1, intrinsic_slow_path->GetEntryLabel());
          __ Bind(&do_copy);
        } else {
          __ B(intrinsic_slow_path->GetEntryLabel(), ne);
        }
      } else {
        // Non read barrier code.

        // /* HeapReference<Class> */ temp1 = dest->klass_
        __ Ldr(temp1, MemOperand(dest, class_offset));
        // /* HeapReference<Class> */ temp2 = src->klass_
        __ Ldr(temp2, MemOperand(src, class_offset));
        bool did_unpoison = false;
        if (!optimizations.GetDestinationIsNonPrimitiveArray() ||
            !optimizations.GetSourceIsNonPrimitiveArray()) {
          // One or two of the references need to be unpoisoned. Unpoison them
          // both to make the identity check valid.
          codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp1);
          codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp2);
          did_unpoison = true;
        }

        if (!optimizations.GetDestinationIsNonPrimitiveArray()) {
          // Bail out if the destination is not a non primitive array.
          // /* HeapReference<Class> */ temp3 = temp1->component_type_
          __ Ldr(temp3, HeapOperand(temp1, component_offset));
          __ Cbz(temp3, intrinsic_slow_path->GetEntryLabel());
          codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp3);
          // /* uint16_t */ temp3 = static_cast<uint16>(temp3->primitive_type_);
          __ Ldrh(temp3, HeapOperand(temp3, primitive_offset));
          static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
          __ Cbnz(temp3, intrinsic_slow_path->GetEntryLabel());
        }

        if (!optimizations.GetSourceIsNonPrimitiveArray()) {
          // Bail out if the source is not a non primitive array.
          // /* HeapReference<Class> */ temp3 = temp2->component_type_
          __ Ldr(temp3, HeapOperand(temp2, component_offset));
          __ Cbz(temp3, intrinsic_slow_path->GetEntryLabel());
          codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp3);
          // /* uint16_t */ temp3 = static_cast<uint16>(temp3->primitive_type_);
          __ Ldrh(temp3, HeapOperand(temp3, primitive_offset));
          static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
          __ Cbnz(temp3, intrinsic_slow_path->GetEntryLabel());
        }

        __ Cmp(temp1, temp2);

        if (optimizations.GetDestinationIsTypedObjectArray()) {
          vixl::aarch64::Label do_copy;
          __ B(&do_copy, eq);
          if (!did_unpoison) {
            codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp1);
          }
          // /* HeapReference<Class> */ temp1 = temp1->component_type_
          __ Ldr(temp1, HeapOperand(temp1, component_offset));
          codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp1);
          // /* HeapReference<Class> */ temp1 = temp1->super_class_
          __ Ldr(temp1, HeapOperand(temp1, super_offset));
          // No need to unpoison the result, we're comparing against null.
          __ Cbnz(temp1, intrinsic_slow_path->GetEntryLabel());
          __ Bind(&do_copy);
        } else {
          __ B(intrinsic_slow_path->GetEntryLabel(), ne);
        }
      }
    } else if (!optimizations.GetSourceIsNonPrimitiveArray()) {
      DCHECK(optimizations.GetDestinationIsNonPrimitiveArray());
      // Bail out if the source is not a non primitive array.
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        // /* HeapReference<Class> */ temp1 = src->klass_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                        temp1_loc,
                                                        src.W(),
                                                        class_offset,
                                                        temp3_loc,
                                                        /* needs_null_check */ false,
                                                        /* use_load_acquire */ false);
        // /* HeapReference<Class> */ temp2 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(invoke,
                                                        temp2_loc,
                                                        temp1,
                                                        component_offset,
                                                        temp3_loc,
                                                        /* needs_null_check */ false,
                                                        /* use_load_acquire */ false);
        __ Cbz(temp2, intrinsic_slow_path->GetEntryLabel());
        // If heap poisoning is enabled, `temp2` has been unpoisoned
        // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
      } else {
        // /* HeapReference<Class> */ temp1 = src->klass_
        __ Ldr(temp1, HeapOperand(src.W(), class_offset));
        codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp1);
        // /* HeapReference<Class> */ temp2 = temp1->component_type_
        __ Ldr(temp2, HeapOperand(temp1, component_offset));
        __ Cbz(temp2, intrinsic_slow_path->GetEntryLabel());
        codegen_->GetAssembler()->MaybeUnpoisonHeapReference(temp2);
      }
      // /* uint16_t */ temp2 = static_cast<uint16>(temp2->primitive_type_);
      __ Ldrh(temp2, HeapOperand(temp2, primitive_offset));
      static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
      __ Cbnz(temp2, intrinsic_slow_path->GetEntryLabel());
    }

    if (length.IsConstant() && length.GetConstant()->AsIntConstant()->GetValue() == 0) {
      // Null constant length: not need to emit the loop code at all.
    } else {
      Register src_curr_addr = temp1.X();
      Register dst_curr_addr = temp2.X();
      Register src_stop_addr = temp3.X();
      vixl::aarch64::Label done;
      const DataType::Type type = DataType::Type::kReference;
      const int32_t element_size = DataType::Size(type);

      if (length.IsRegister()) {
        // Don't enter the copy loop if the length is null.
        __ Cbz(WRegisterFrom(length), &done);
      }

      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        // TODO: Also convert this intrinsic to the IsGcMarking strategy?

        // SystemArrayCopy implementation for Baker read barriers (see
        // also CodeGeneratorARM64::GenerateReferenceLoadWithBakerReadBarrier):
        //
        //   uint32_t rb_state = Lockword(src->monitor_).ReadBarrierState();
        //   lfence;  // Load fence or artificial data dependency to prevent load-load reordering
        //   bool is_gray = (rb_state == ReadBarrier::GrayState());
        //   if (is_gray) {
        //     // Slow-path copy.
        //     do {
        //       *dest_ptr++ = MaybePoison(ReadBarrier::Mark(MaybeUnpoison(*src_ptr++)));
        //     } while (src_ptr != end_ptr)
        //   } else {
        //     // Fast-path copy.
        //     do {
        //       *dest_ptr++ = *src_ptr++;
        //     } while (src_ptr != end_ptr)
        //   }

        // Make sure `tmp` is not IP0, as it is clobbered by
        // ReadBarrierMarkRegX entry points in
        // ReadBarrierSystemArrayCopySlowPathARM64.
        DCHECK(temps.IsAvailable(ip0));
        temps.Exclude(ip0);
        Register tmp = temps.AcquireW();
        DCHECK_NE(LocationFrom(tmp).reg(), IP0);
        // Put IP0 back in the pool so that VIXL has at least one
        // scratch register available to emit macro-instructions (note
        // that IP1 is already used for `tmp`). Indeed some
        // macro-instructions used in GenSystemArrayCopyAddresses
        // (invoked hereunder) may require a scratch register (for
        // instance to emit a load with a large constant offset).
        temps.Include(ip0);

        // /* int32_t */ monitor = src->monitor_
        __ Ldr(tmp, HeapOperand(src.W(), monitor_offset));
        // /* LockWord */ lock_word = LockWord(monitor)
        static_assert(sizeof(LockWord) == sizeof(int32_t),
                      "art::LockWord and int32_t have different sizes.");

        // Introduce a dependency on the lock_word including rb_state,
        // to prevent load-load reordering, and without using
        // a memory barrier (which would be more expensive).
        // `src` is unchanged by this operation, but its value now depends
        // on `tmp`.
        __ Add(src.X(), src.X(), Operand(tmp.X(), LSR, 32));

        // Compute base source address, base destination address, and end
        // source address for System.arraycopy* intrinsics in `src_base`,
        // `dst_base` and `src_end` respectively.
        // Note that `src_curr_addr` is computed from from `src` (and
        // `src_pos`) here, and thus honors the artificial dependency
        // of `src` on `tmp`.
        GenSystemArrayCopyAddresses(masm,
                                    type,
                                    src,
                                    src_pos,
                                    dest,
                                    dest_pos,
                                    length,
                                    src_curr_addr,
                                    dst_curr_addr,
                                    src_stop_addr);

        // Slow path used to copy array when `src` is gray.
        SlowPathCodeARM64* read_barrier_slow_path =
            new (codegen_->GetScopedAllocator()) ReadBarrierSystemArrayCopySlowPathARM64(
                invoke, LocationFrom(tmp));
        codegen_->AddSlowPath(read_barrier_slow_path);

        // Given the numeric representation, it's enough to check the low bit of the rb_state.
        static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
        static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
        __ Tbnz(tmp, LockWord::kReadBarrierStateShift, read_barrier_slow_path->GetEntryLabel());

        // Fast-path copy.
        // Iterate over the arrays and do a raw copy of the objects. We don't need to
        // poison/unpoison.
        vixl::aarch64::Label loop;
        __ Bind(&loop);
        __ Ldr(tmp, MemOperand(src_curr_addr, element_size, PostIndex));
        __ Str(tmp, MemOperand(dst_curr_addr, element_size, PostIndex));
        __ Cmp(src_curr_addr, src_stop_addr);
        __ B(&loop, ne);

        __ Bind(read_barrier_slow_path->GetExitLabel());
      } else {
        // Non read barrier code.
        // Compute base source address, base destination address, and end
        // source address for System.arraycopy* intrinsics in `src_base`,
        // `dst_base` and `src_end` respectively.
        GenSystemArrayCopyAddresses(masm,
                                    type,
                                    src,
                                    src_pos,
                                    dest,
                                    dest_pos,
                                    length,
                                    src_curr_addr,
                                    dst_curr_addr,
                                    src_stop_addr);
        // Iterate over the arrays and do a raw copy of the objects. We don't need to
        // poison/unpoison.
        vixl::aarch64::Label loop;
        __ Bind(&loop);
        {
          Register tmp = temps.AcquireW();
          __ Ldr(tmp, MemOperand(src_curr_addr, element_size, PostIndex));
          __ Str(tmp, MemOperand(dst_curr_addr, element_size, PostIndex));
        }
        __ Cmp(src_curr_addr, src_stop_addr);
        __ B(&loop, ne);
      }
      __ Bind(&done);
    }
  }

  // We only need one card marking on the destination array.
  codegen_->MarkGCCard(dest.W(), Register(), /* value_can_be_null */ false);

  __ Bind(intrinsic_slow_path->GetExitLabel());
}

static void GenIsInfinite(LocationSummary* locations,
                          bool is64bit,
                          MacroAssembler* masm) {
  Operand infinity;
  Register out;

  if (is64bit) {
    infinity = kPositiveInfinityDouble;
    out = XRegisterFrom(locations->Out());
  } else {
    infinity = kPositiveInfinityFloat;
    out = WRegisterFrom(locations->Out());
  }

  const Register zero = vixl::aarch64::Assembler::AppropriateZeroRegFor(out);

  MoveFPToInt(locations, is64bit, masm);
  __ Eor(out, out, infinity);
  // We don't care about the sign bit, so shift left.
  __ Cmp(zero, Operand(out, LSL, 1));
  __ Cset(out, eq);
}

void IntrinsicLocationsBuilderARM64::VisitFloatIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitFloatIsInfinite(HInvoke* invoke) {
  GenIsInfinite(invoke->GetLocations(), /* is64bit */ false, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitDoubleIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARM64::VisitDoubleIsInfinite(HInvoke* invoke) {
  GenIsInfinite(invoke->GetLocations(), /* is64bit */ true, GetVIXLAssembler());
}

void IntrinsicLocationsBuilderARM64::VisitIntegerValueOf(HInvoke* invoke) {
  InvokeRuntimeCallingConvention calling_convention;
  IntrinsicVisitor::ComputeIntegerValueOfLocations(
      invoke,
      codegen_,
      calling_convention.GetReturnLocation(DataType::Type::kReference),
      Location::RegisterLocation(calling_convention.GetRegisterAt(0).GetCode()));
}

void IntrinsicCodeGeneratorARM64::VisitIntegerValueOf(HInvoke* invoke) {
  IntrinsicVisitor::IntegerValueOfInfo info = IntrinsicVisitor::ComputeIntegerValueOfInfo();
  LocationSummary* locations = invoke->GetLocations();
  MacroAssembler* masm = GetVIXLAssembler();

  Register out = RegisterFrom(locations->Out(), DataType::Type::kReference);
  UseScratchRegisterScope temps(masm);
  Register temp = temps.AcquireW();
  InvokeRuntimeCallingConvention calling_convention;
  Register argument = calling_convention.GetRegisterAt(0);
  if (invoke->InputAt(0)->IsConstant()) {
    int32_t value = invoke->InputAt(0)->AsIntConstant()->GetValue();
    if (value >= info.low && value <= info.high) {
      // Just embed the j.l.Integer in the code.
      ScopedObjectAccess soa(Thread::Current());
      mirror::Object* boxed = info.cache->Get(value + (-info.low));
      DCHECK(boxed != nullptr && Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boxed));
      uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(boxed));
      __ Ldr(out.W(), codegen_->DeduplicateBootImageAddressLiteral(address));
    } else {
      // Allocate and initialize a new j.l.Integer.
      // TODO: If we JIT, we could allocate the j.l.Integer now, and store it in the
      // JIT object table.
      uint32_t address =
          dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
      __ Ldr(argument.W(), codegen_->DeduplicateBootImageAddressLiteral(address));
      codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
      CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
      __ Mov(temp.W(), value);
      __ Str(temp.W(), HeapOperand(out.W(), info.value_offset));
      // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
      // one.
      codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    }
  } else {
    Register in = RegisterFrom(locations->InAt(0), DataType::Type::kInt32);
    // Check bounds of our cache.
    __ Add(out.W(), in.W(), -info.low);
    __ Cmp(out.W(), info.high - info.low + 1);
    vixl::aarch64::Label allocate, done;
    __ B(&allocate, hs);
    // If the value is within the bounds, load the j.l.Integer directly from the array.
    uint32_t data_offset = mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();
    uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.cache));
    __ Ldr(temp.W(), codegen_->DeduplicateBootImageAddressLiteral(data_offset + address));
    MemOperand source = HeapOperand(
        temp, out.X(), LSL, DataType::SizeShift(DataType::Type::kReference));
    codegen_->Load(DataType::Type::kReference, out, source);
    codegen_->GetAssembler()->MaybeUnpoisonHeapReference(out);
    __ B(&done);
    __ Bind(&allocate);
    // Otherwise allocate and initialize a new j.l.Integer.
    address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
    __ Ldr(argument.W(), codegen_->DeduplicateBootImageAddressLiteral(address));
    codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
    __ Str(in.W(), HeapOperand(out.W(), info.value_offset));
    // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
    // one.
    codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARM64::VisitThreadInterrupted(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARM64::VisitThreadInterrupted(HInvoke* invoke) {
  MacroAssembler* masm = GetVIXLAssembler();
  Register out = RegisterFrom(invoke->GetLocations()->Out(), DataType::Type::kInt32);
  UseScratchRegisterScope temps(masm);
  Register temp = temps.AcquireX();

  __ Add(temp, tr, Thread::InterruptedOffset<kArm64PointerSize>().Int32Value());
  __ Ldar(out.W(), MemOperand(temp));

  vixl::aarch64::Label done;
  __ Cbz(out.W(), &done);
  __ Stlr(wzr, MemOperand(temp));
  __ Bind(&done);
}

void IntrinsicLocationsBuilderARM64::VisitReachabilityFence(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::Any());
}

void IntrinsicCodeGeneratorARM64::VisitReachabilityFence(HInvoke* invoke ATTRIBUTE_UNUSED) { }

UNIMPLEMENTED_INTRINSIC(ARM64, ReferenceGetReferent)

UNIMPLEMENTED_INTRINSIC(ARM64, StringStringIndexOf);
UNIMPLEMENTED_INTRINSIC(ARM64, StringStringIndexOfAfter);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBufferAppend);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBufferLength);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBufferToString);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBuilderAppend);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBuilderLength);
UNIMPLEMENTED_INTRINSIC(ARM64, StringBuilderToString);

// 1.8.
UNIMPLEMENTED_INTRINSIC(ARM64, UnsafeGetAndAddInt)
UNIMPLEMENTED_INTRINSIC(ARM64, UnsafeGetAndAddLong)
UNIMPLEMENTED_INTRINSIC(ARM64, UnsafeGetAndSetInt)
UNIMPLEMENTED_INTRINSIC(ARM64, UnsafeGetAndSetLong)
UNIMPLEMENTED_INTRINSIC(ARM64, UnsafeGetAndSetObject)

UNREACHABLE_INTRINSICS(ARM64)

#undef __

}  // namespace arm64
}  // namespace art
