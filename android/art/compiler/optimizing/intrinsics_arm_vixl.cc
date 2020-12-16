/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "intrinsics_arm_vixl.h"

#include "arch/arm/instruction_set_features_arm.h"
#include "art_method.h"
#include "code_generator_arm_vixl.h"
#include "common_arm.h"
#include "heap_poisoning.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/reference.h"
#include "mirror/string.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

#include "aarch32/constants-aarch32.h"

namespace art {
namespace arm {

#define __ assembler->GetVIXLAssembler()->

using helpers::DRegisterFrom;
using helpers::HighRegisterFrom;
using helpers::InputDRegisterAt;
using helpers::InputRegisterAt;
using helpers::InputSRegisterAt;
using helpers::InputVRegisterAt;
using helpers::Int32ConstantFrom;
using helpers::LocationFrom;
using helpers::LowRegisterFrom;
using helpers::LowSRegisterFrom;
using helpers::HighSRegisterFrom;
using helpers::OutputDRegister;
using helpers::OutputSRegister;
using helpers::OutputRegister;
using helpers::OutputVRegister;
using helpers::RegisterFrom;
using helpers::SRegisterFrom;
using helpers::DRegisterFromS;

using namespace vixl::aarch32;  // NOLINT(build/namespaces)

using vixl::ExactAssemblyScope;
using vixl::CodeBufferCheckScope;

ArmVIXLAssembler* IntrinsicCodeGeneratorARMVIXL::GetAssembler() {
  return codegen_->GetAssembler();
}

ArenaAllocator* IntrinsicCodeGeneratorARMVIXL::GetAllocator() {
  return codegen_->GetGraph()->GetAllocator();
}

// Default slow-path for fallback (calling the managed code to handle the intrinsic) in an
// intrinsified call. This will copy the arguments into the positions for a regular call.
//
// Note: The actual parameters are required to be in the locations given by the invoke's location
//       summary. If an intrinsic modifies those locations before a slowpath call, they must be
//       restored!
//
// Note: If an invoke wasn't sharpened, we will put down an invoke-virtual here. That's potentially
//       sub-optimal (compared to a direct pointer call), but this is a slow-path.

class IntrinsicSlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit IntrinsicSlowPathARMVIXL(HInvoke* invoke)
      : SlowPathCodeARMVIXL(invoke), invoke_(invoke) {}

  Location MoveArguments(CodeGenerator* codegen) {
    InvokeDexCallingConventionVisitorARMVIXL calling_convention_visitor;
    IntrinsicVisitor::MoveArguments(invoke_, codegen, &calling_convention_visitor);
    return calling_convention_visitor.GetMethodLocation();
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    ArmVIXLAssembler* assembler = down_cast<ArmVIXLAssembler*>(codegen->GetAssembler());
    __ Bind(GetEntryLabel());

    SaveLiveRegisters(codegen, invoke_->GetLocations());

    Location method_loc = MoveArguments(codegen);

    if (invoke_->IsInvokeStaticOrDirect()) {
      codegen->GenerateStaticOrDirectCall(invoke_->AsInvokeStaticOrDirect(), method_loc, this);
    } else {
      codegen->GenerateVirtualCall(invoke_->AsInvokeVirtual(), method_loc, this);
    }

    // Copy the result back to the expected output.
    Location out = invoke_->GetLocations()->Out();
    if (out.IsValid()) {
      DCHECK(out.IsRegister());  // TODO: Replace this when we support output in memory.
      DCHECK(!invoke_->GetLocations()->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      codegen->MoveFromReturnRegister(out, invoke_->GetType());
    }

    RestoreLiveRegisters(codegen, invoke_->GetLocations());
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "IntrinsicSlowPath"; }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathARMVIXL);
};

// Compute base address for the System.arraycopy intrinsic in `base`.
static void GenSystemArrayCopyBaseAddress(ArmVIXLAssembler* assembler,
                                          DataType::Type type,
                                          const vixl32::Register& array,
                                          const Location& pos,
                                          const vixl32::Register& base) {
  // This routine is only used by the SystemArrayCopy intrinsic at the
  // moment. We can allow DataType::Type::kReference as `type` to implement
  // the SystemArrayCopyChar intrinsic.
  DCHECK_EQ(type, DataType::Type::kReference);
  const int32_t element_size = DataType::Size(type);
  const uint32_t element_size_shift = DataType::SizeShift(type);
  const uint32_t data_offset = mirror::Array::DataOffset(element_size).Uint32Value();

  if (pos.IsConstant()) {
    int32_t constant = Int32ConstantFrom(pos);
    __ Add(base, array, element_size * constant + data_offset);
  } else {
    __ Add(base, array, Operand(RegisterFrom(pos), vixl32::LSL, element_size_shift));
    __ Add(base, base, data_offset);
  }
}

// Compute end address for the System.arraycopy intrinsic in `end`.
static void GenSystemArrayCopyEndAddress(ArmVIXLAssembler* assembler,
                                         DataType::Type type,
                                         const Location& copy_length,
                                         const vixl32::Register& base,
                                         const vixl32::Register& end) {
  // This routine is only used by the SystemArrayCopy intrinsic at the
  // moment. We can allow DataType::Type::kReference as `type` to implement
  // the SystemArrayCopyChar intrinsic.
  DCHECK_EQ(type, DataType::Type::kReference);
  const int32_t element_size = DataType::Size(type);
  const uint32_t element_size_shift = DataType::SizeShift(type);

  if (copy_length.IsConstant()) {
    int32_t constant = Int32ConstantFrom(copy_length);
    __ Add(end, base, element_size * constant);
  } else {
    __ Add(end, base, Operand(RegisterFrom(copy_length), vixl32::LSL, element_size_shift));
  }
}

// Slow path implementing the SystemArrayCopy intrinsic copy loop with read barriers.
class ReadBarrierSystemArrayCopySlowPathARMVIXL : public SlowPathCodeARMVIXL {
 public:
  explicit ReadBarrierSystemArrayCopySlowPathARMVIXL(HInstruction* instruction)
      : SlowPathCodeARMVIXL(instruction) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen);
    ArmVIXLAssembler* assembler = arm_codegen->GetAssembler();
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(locations->CanCall());
    DCHECK(instruction_->IsInvokeStaticOrDirect())
        << "Unexpected instruction in read barrier arraycopy slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kSystemArrayCopy);

    DataType::Type type = DataType::Type::kReference;
    const int32_t element_size = DataType::Size(type);

    vixl32::Register dest = InputRegisterAt(instruction_, 2);
    Location dest_pos = locations->InAt(3);
    vixl32::Register src_curr_addr = RegisterFrom(locations->GetTemp(0));
    vixl32::Register dst_curr_addr = RegisterFrom(locations->GetTemp(1));
    vixl32::Register src_stop_addr = RegisterFrom(locations->GetTemp(2));
    vixl32::Register tmp = RegisterFrom(locations->GetTemp(3));

    __ Bind(GetEntryLabel());
    // Compute the base destination address in `dst_curr_addr`.
    GenSystemArrayCopyBaseAddress(assembler, type, dest, dest_pos, dst_curr_addr);

    vixl32::Label loop;
    __ Bind(&loop);
    __ Ldr(tmp, MemOperand(src_curr_addr, element_size, PostIndex));
    assembler->MaybeUnpoisonHeapReference(tmp);
    // TODO: Inline the mark bit check before calling the runtime?
    // tmp = ReadBarrier::Mark(tmp);
    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    // (See ReadBarrierMarkSlowPathARM::EmitNativeCode for more
    // explanations.)
    DCHECK(!tmp.IsSP());
    DCHECK(!tmp.IsLR());
    DCHECK(!tmp.IsPC());
    // IP is used internally by the ReadBarrierMarkRegX entry point
    // as a temporary (and not preserved).  It thus cannot be used by
    // any live register in this slow path.
    DCHECK(!src_curr_addr.Is(ip));
    DCHECK(!dst_curr_addr.Is(ip));
    DCHECK(!src_stop_addr.Is(ip));
    DCHECK(!tmp.Is(ip));
    DCHECK(tmp.IsRegister()) << tmp;
    // TODO: Load the entrypoint once before the loop, instead of
    // loading it at every iteration.
    int32_t entry_point_offset =
        Thread::ReadBarrierMarkEntryPointsOffset<kArmPointerSize>(tmp.GetCode());
    // This runtime call does not require a stack map.
    arm_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset, instruction_, this);
    assembler->MaybePoisonHeapReference(tmp);
    __ Str(tmp, MemOperand(dst_curr_addr, element_size, PostIndex));
    __ Cmp(src_curr_addr, src_stop_addr);
    __ B(ne, &loop, /* far_target */ false);
    __ B(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE {
    return "ReadBarrierSystemArrayCopySlowPathARMVIXL";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierSystemArrayCopySlowPathARMVIXL);
};

IntrinsicLocationsBuilderARMVIXL::IntrinsicLocationsBuilderARMVIXL(CodeGeneratorARMVIXL* codegen)
    : allocator_(codegen->GetGraph()->GetAllocator()),
      codegen_(codegen),
      assembler_(codegen->GetAssembler()),
      features_(codegen->GetInstructionSetFeatures()) {}

bool IntrinsicLocationsBuilderARMVIXL::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  if (res == nullptr) {
    return false;
  }
  return res->Intrinsified();
}

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

static void MoveFPToInt(LocationSummary* locations, bool is64bit, ArmVIXLAssembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  if (is64bit) {
    __ Vmov(LowRegisterFrom(output), HighRegisterFrom(output), DRegisterFrom(input));
  } else {
    __ Vmov(RegisterFrom(output), SRegisterFrom(input));
  }
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, ArmVIXLAssembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  if (is64bit) {
    __ Vmov(DRegisterFrom(output), LowRegisterFrom(input), HighRegisterFrom(input));
  } else {
    __ Vmov(SRegisterFrom(output), RegisterFrom(input));
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}
void IntrinsicCodeGeneratorARMVIXL::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}
void IntrinsicCodeGeneratorARMVIXL::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}

static void CreateIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void CreateLongToLongLocationsWithOverlap(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
}

static void CreateFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

static void GenNumberOfLeadingZeros(HInvoke* invoke,
                                    DataType::Type type,
                                    CodeGeneratorARMVIXL* codegen) {
  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  LocationSummary* locations = invoke->GetLocations();
  Location in = locations->InAt(0);
  vixl32::Register out = RegisterFrom(locations->Out());

  DCHECK((type == DataType::Type::kInt32) || (type == DataType::Type::kInt64));

  if (type == DataType::Type::kInt64) {
    vixl32::Register in_reg_lo = LowRegisterFrom(in);
    vixl32::Register in_reg_hi = HighRegisterFrom(in);
    vixl32::Label end;
    vixl32::Label* final_label = codegen->GetFinalLabel(invoke, &end);
    __ Clz(out, in_reg_hi);
    __ CompareAndBranchIfNonZero(in_reg_hi, final_label, /* far_target */ false);
    __ Clz(out, in_reg_lo);
    __ Add(out, out, 32);
    if (end.IsReferenced()) {
      __ Bind(&end);
    }
  } else {
    __ Clz(out, RegisterFrom(in));
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeros(invoke, DataType::Type::kInt32, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeros(invoke, DataType::Type::kInt64, codegen_);
}

static void GenNumberOfTrailingZeros(HInvoke* invoke,
                                     DataType::Type type,
                                     CodeGeneratorARMVIXL* codegen) {
  DCHECK((type == DataType::Type::kInt32) || (type == DataType::Type::kInt64));

  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  LocationSummary* locations = invoke->GetLocations();
  vixl32::Register out = RegisterFrom(locations->Out());

  if (type == DataType::Type::kInt64) {
    vixl32::Register in_reg_lo = LowRegisterFrom(locations->InAt(0));
    vixl32::Register in_reg_hi = HighRegisterFrom(locations->InAt(0));
    vixl32::Label end;
    vixl32::Label* final_label = codegen->GetFinalLabel(invoke, &end);
    __ Rbit(out, in_reg_lo);
    __ Clz(out, out);
    __ CompareAndBranchIfNonZero(in_reg_lo, final_label, /* far_target */ false);
    __ Rbit(out, in_reg_hi);
    __ Clz(out, out);
    __ Add(out, out, 32);
    if (end.IsReferenced()) {
      __ Bind(&end);
    }
  } else {
    vixl32::Register in = RegisterFrom(locations->InAt(0));
    __ Rbit(out, in);
    __ Clz(out, out);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeros(invoke, DataType::Type::kInt32, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeros(invoke, DataType::Type::kInt64, codegen_);
}

static void MathAbsFP(HInvoke* invoke, ArmVIXLAssembler* assembler) {
  __ Vabs(OutputVRegister(invoke), InputVRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke, GetAssembler());
}

static void CreateIntToIntPlusTemp(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);

  locations->AddTemp(Location::RequiresRegister());
}

static void GenAbsInteger(LocationSummary* locations,
                          bool is64bit,
                          ArmVIXLAssembler* assembler) {
  Location in = locations->InAt(0);
  Location output = locations->Out();

  vixl32::Register mask = RegisterFrom(locations->GetTemp(0));

  if (is64bit) {
    vixl32::Register in_reg_lo = LowRegisterFrom(in);
    vixl32::Register in_reg_hi = HighRegisterFrom(in);
    vixl32::Register out_reg_lo = LowRegisterFrom(output);
    vixl32::Register out_reg_hi = HighRegisterFrom(output);

    DCHECK(!out_reg_lo.Is(in_reg_hi)) << "Diagonal overlap unexpected.";

    __ Asr(mask, in_reg_hi, 31);
    __ Adds(out_reg_lo, in_reg_lo, mask);
    __ Adc(out_reg_hi, in_reg_hi, mask);
    __ Eor(out_reg_lo, mask, out_reg_lo);
    __ Eor(out_reg_hi, mask, out_reg_hi);
  } else {
    vixl32::Register in_reg = RegisterFrom(in);
    vixl32::Register out_reg = RegisterFrom(output);

    __ Asr(mask, in_reg, 31);
    __ Add(out_reg, in_reg, mask);
    __ Eor(out_reg, mask, out_reg);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToIntPlusTemp(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}


void IntrinsicLocationsBuilderARMVIXL::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToIntPlusTemp(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

static void GenMinMaxFloat(HInvoke* invoke, bool is_min, CodeGeneratorARMVIXL* codegen) {
  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  Location op1_loc = invoke->GetLocations()->InAt(0);
  Location op2_loc = invoke->GetLocations()->InAt(1);
  Location out_loc = invoke->GetLocations()->Out();

  // Optimization: don't generate any code if inputs are the same.
  if (op1_loc.Equals(op2_loc)) {
    DCHECK(out_loc.Equals(op1_loc));  // out_loc is set as SameAsFirstInput() in location builder.
    return;
  }

  vixl32::SRegister op1 = SRegisterFrom(op1_loc);
  vixl32::SRegister op2 = SRegisterFrom(op2_loc);
  vixl32::SRegister out = OutputSRegister(invoke);
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  const vixl32::Register temp1 = temps.Acquire();
  vixl32::Register temp2 = RegisterFrom(invoke->GetLocations()->GetTemp(0));
  vixl32::Label nan, done;
  vixl32::Label* final_label = codegen->GetFinalLabel(invoke, &done);

  DCHECK(op1.Is(out));

  __ Vcmp(op1, op2);
  __ Vmrs(RegisterOrAPSR_nzcv(kPcCode), FPSCR);
  __ B(vs, &nan, /* far_target */ false);  // if un-ordered, go to NaN handling.

  // op1 <> op2
  vixl32::ConditionType cond = is_min ? gt : lt;
  {
    ExactAssemblyScope it_scope(assembler->GetVIXLAssembler(),
                                2 * kMaxInstructionSizeInBytes,
                                CodeBufferCheckScope::kMaximumSize);
    __ it(cond);
    __ vmov(cond, F32, out, op2);
  }
  // for <>(not equal), we've done min/max calculation.
  __ B(ne, final_label, /* far_target */ false);

  // handle op1 == op2, max(+0.0,-0.0), min(+0.0,-0.0).
  __ Vmov(temp1, op1);
  __ Vmov(temp2, op2);
  if (is_min) {
    __ Orr(temp1, temp1, temp2);
  } else {
    __ And(temp1, temp1, temp2);
  }
  __ Vmov(out, temp1);
  __ B(final_label);

  // handle NaN input.
  __ Bind(&nan);
  __ Movt(temp1, High16Bits(kNanFloat));  // 0x7FC0xxxx is a NaN.
  __ Vmov(out, temp1);

  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

static void CreateFPFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
  invoke->GetLocations()->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFloat(invoke, /* is_min */ true, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
  invoke->GetLocations()->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFloat(invoke, /* is_min */ false, codegen_);
}

static void GenMinMaxDouble(HInvoke* invoke, bool is_min, CodeGeneratorARMVIXL* codegen) {
  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  Location op1_loc = invoke->GetLocations()->InAt(0);
  Location op2_loc = invoke->GetLocations()->InAt(1);
  Location out_loc = invoke->GetLocations()->Out();

  // Optimization: don't generate any code if inputs are the same.
  if (op1_loc.Equals(op2_loc)) {
    DCHECK(out_loc.Equals(op1_loc));  // out_loc is set as SameAsFirstInput() in.
    return;
  }

  vixl32::DRegister op1 = DRegisterFrom(op1_loc);
  vixl32::DRegister op2 = DRegisterFrom(op2_loc);
  vixl32::DRegister out = OutputDRegister(invoke);
  vixl32::Label handle_nan_eq, done;
  vixl32::Label* final_label = codegen->GetFinalLabel(invoke, &done);

  DCHECK(op1.Is(out));

  __ Vcmp(op1, op2);
  __ Vmrs(RegisterOrAPSR_nzcv(kPcCode), FPSCR);
  __ B(vs, &handle_nan_eq, /* far_target */ false);  // if un-ordered, go to NaN handling.

  // op1 <> op2
  vixl32::ConditionType cond = is_min ? gt : lt;
  {
    ExactAssemblyScope it_scope(assembler->GetVIXLAssembler(),
                                2 * kMaxInstructionSizeInBytes,
                                CodeBufferCheckScope::kMaximumSize);
    __ it(cond);
    __ vmov(cond, F64, out, op2);
  }
  // for <>(not equal), we've done min/max calculation.
  __ B(ne, final_label, /* far_target */ false);

  // handle op1 == op2, max(+0.0,-0.0).
  if (!is_min) {
    __ Vand(F64, out, op1, op2);
    __ B(final_label);
  }

  // handle op1 == op2, min(+0.0,-0.0), NaN input.
  __ Bind(&handle_nan_eq);
  __ Vorr(F64, out, op1, op2);  // assemble op1/-0.0/NaN.

  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxDouble(invoke, /* is_min */ true , codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxDouble(invoke, /* is_min */ false, codegen_);
}

static void GenMinMaxLong(HInvoke* invoke, bool is_min, ArmVIXLAssembler* assembler) {
  Location op1_loc = invoke->GetLocations()->InAt(0);
  Location op2_loc = invoke->GetLocations()->InAt(1);
  Location out_loc = invoke->GetLocations()->Out();

  // Optimization: don't generate any code if inputs are the same.
  if (op1_loc.Equals(op2_loc)) {
    DCHECK(out_loc.Equals(op1_loc));  // out_loc is set as SameAsFirstInput() in location builder.
    return;
  }

  vixl32::Register op1_lo = LowRegisterFrom(op1_loc);
  vixl32::Register op1_hi = HighRegisterFrom(op1_loc);
  vixl32::Register op2_lo = LowRegisterFrom(op2_loc);
  vixl32::Register op2_hi = HighRegisterFrom(op2_loc);
  vixl32::Register out_lo = LowRegisterFrom(out_loc);
  vixl32::Register out_hi = HighRegisterFrom(out_loc);
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  const vixl32::Register temp = temps.Acquire();

  DCHECK(op1_lo.Is(out_lo));
  DCHECK(op1_hi.Is(out_hi));

  // Compare op1 >= op2, or op1 < op2.
  __ Cmp(out_lo, op2_lo);
  __ Sbcs(temp, out_hi, op2_hi);

  // Now GE/LT condition code is correct for the long comparison.
  {
    vixl32::ConditionType cond = is_min ? ge : lt;
    ExactAssemblyScope it_scope(assembler->GetVIXLAssembler(),
                                3 * kMaxInstructionSizeInBytes,
                                CodeBufferCheckScope::kMaximumSize);
    __ itt(cond);
    __ mov(cond, out_lo, op2_lo);
    __ mov(cond, out_hi, op2_hi);
  }
}

static void CreateLongLongToLongLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMinLongLong(HInvoke* invoke) {
  CreateLongLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMaxLong(invoke, /* is_min */ true, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateLongLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMaxLong(invoke, /* is_min */ false, GetAssembler());
}

static void GenMinMax(HInvoke* invoke, bool is_min, ArmVIXLAssembler* assembler) {
  vixl32::Register op1 = InputRegisterAt(invoke, 0);
  vixl32::Register op2 = InputRegisterAt(invoke, 1);
  vixl32::Register out = OutputRegister(invoke);

  __ Cmp(op1, op2);

  {
    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           3 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);

    __ ite(is_min ? lt : gt);
    __ mov(is_min ? lt : gt, out, op1);
    __ mov(is_min ? ge : le, out, op2);
  }
}

static void CreateIntIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke, /* is_min */ true, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke, /* is_min */ false, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathSqrt(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Vsqrt(OutputDRegister(invoke), InputDRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathRint(HInvoke* invoke) {
  if (features_.HasARMv8AInstructions()) {
    CreateFPToFPLocations(allocator_, invoke);
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathRint(HInvoke* invoke) {
  DCHECK(codegen_->GetInstructionSetFeatures().HasARMv8AInstructions());
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Vrintn(F64, F64, OutputDRegister(invoke), InputDRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathRoundFloat(HInvoke* invoke) {
  if (features_.HasARMv8AInstructions()) {
    LocationSummary* locations =
        new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
    locations->SetInAt(0, Location::RequiresFpuRegister());
    locations->SetOut(Location::RequiresRegister());
    locations->AddTemp(Location::RequiresFpuRegister());
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathRoundFloat(HInvoke* invoke) {
  DCHECK(codegen_->GetInstructionSetFeatures().HasARMv8AInstructions());

  ArmVIXLAssembler* assembler = GetAssembler();
  vixl32::SRegister in_reg = InputSRegisterAt(invoke, 0);
  vixl32::Register out_reg = OutputRegister(invoke);
  vixl32::SRegister temp1 = LowSRegisterFrom(invoke->GetLocations()->GetTemp(0));
  vixl32::SRegister temp2 = HighSRegisterFrom(invoke->GetLocations()->GetTemp(0));
  vixl32::Label done;
  vixl32::Label* final_label = codegen_->GetFinalLabel(invoke, &done);

  // Round to nearest integer, ties away from zero.
  __ Vcvta(S32, F32, temp1, in_reg);
  __ Vmov(out_reg, temp1);

  // For positive, zero or NaN inputs, rounding is done.
  __ Cmp(out_reg, 0);
  __ B(ge, final_label, /* far_target */ false);

  // Handle input < 0 cases.
  // If input is negative but not a tie, previous result (round to nearest) is valid.
  // If input is a negative tie, change rounding direction to positive infinity, out_reg += 1.
  __ Vrinta(F32, F32, temp1, in_reg);
  __ Vmov(temp2, 0.5);
  __ Vsub(F32, temp1, in_reg, temp1);
  __ Vcmp(F32, temp1, temp2);
  __ Vmrs(RegisterOrAPSR_nzcv(kPcCode), FPSCR);
  {
    // Use ExactAsemblyScope here because we are using IT.
    ExactAssemblyScope it_scope(assembler->GetVIXLAssembler(),
                                2 * kMaxInstructionSizeInBytes,
                                CodeBufferCheckScope::kMaximumSize);
    __ it(eq);
    __ add(eq, out_reg, out_reg, 1);
  }

  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPeekByte(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  // Ignore upper 4B of long address.
  __ Ldrsb(OutputRegister(invoke), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPeekIntNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  // Ignore upper 4B of long address.
  __ Ldr(OutputRegister(invoke), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPeekLongNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  // Ignore upper 4B of long address.
  vixl32::Register addr = LowRegisterFrom(invoke->GetLocations()->InAt(0));
  // Worst case: Control register bit SCTLR.A = 0. Then unaligned accesses throw a processor
  // exception. So we can't use ldrd as addr may be unaligned.
  vixl32::Register lo = LowRegisterFrom(invoke->GetLocations()->Out());
  vixl32::Register hi = HighRegisterFrom(invoke->GetLocations()->Out());
  if (addr.Is(lo)) {
    __ Ldr(hi, MemOperand(addr, 4));
    __ Ldr(lo, MemOperand(addr));
  } else {
    __ Ldr(lo, MemOperand(addr));
    __ Ldr(hi, MemOperand(addr, 4));
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPeekShortNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  // Ignore upper 4B of long address.
  __ Ldrsh(OutputRegister(invoke), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

static void CreateIntIntToVoidLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPokeByte(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Strb(InputRegisterAt(invoke, 1), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPokeIntNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Str(InputRegisterAt(invoke, 1), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPokeLongNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  // Ignore upper 4B of long address.
  vixl32::Register addr = LowRegisterFrom(invoke->GetLocations()->InAt(0));
  // Worst case: Control register bit SCTLR.A = 0. Then unaligned accesses throw a processor
  // exception. So we can't use ldrd as addr may be unaligned.
  __ Str(LowRegisterFrom(invoke->GetLocations()->InAt(1)), MemOperand(addr));
  __ Str(HighRegisterFrom(invoke->GetLocations()->InAt(1)), MemOperand(addr, 4));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMemoryPokeShortNative(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Strh(InputRegisterAt(invoke, 1), MemOperand(LowRegisterFrom(invoke->GetLocations()->InAt(0))));
}

void IntrinsicLocationsBuilderARMVIXL::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitThreadCurrentThread(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Ldr(OutputRegister(invoke),
         MemOperand(tr, Thread::PeerOffset<kArmPointerSize>().Int32Value()));
}

static void GenUnsafeGet(HInvoke* invoke,
                         DataType::Type type,
                         bool is_volatile,
                         CodeGeneratorARMVIXL* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  Location base_loc = locations->InAt(1);
  vixl32::Register base = InputRegisterAt(invoke, 1);     // Object pointer.
  Location offset_loc = locations->InAt(2);
  vixl32::Register offset = LowRegisterFrom(offset_loc);  // Long offset, lo part only.
  Location trg_loc = locations->Out();

  switch (type) {
    case DataType::Type::kInt32: {
      vixl32::Register trg = RegisterFrom(trg_loc);
      __ Ldr(trg, MemOperand(base, offset));
      if (is_volatile) {
        __ Dmb(vixl32::ISH);
      }
      break;
    }

    case DataType::Type::kReference: {
      vixl32::Register trg = RegisterFrom(trg_loc);
      if (kEmitCompilerReadBarrier) {
        if (kUseBakerReadBarrier) {
          Location temp = locations->GetTemp(0);
          codegen->GenerateReferenceLoadWithBakerReadBarrier(
              invoke, trg_loc, base, 0U, offset_loc, TIMES_1, temp, /* needs_null_check */ false);
          if (is_volatile) {
            __ Dmb(vixl32::ISH);
          }
        } else {
          __ Ldr(trg, MemOperand(base, offset));
          if (is_volatile) {
            __ Dmb(vixl32::ISH);
          }
          codegen->GenerateReadBarrierSlow(invoke, trg_loc, trg_loc, base_loc, 0U, offset_loc);
        }
      } else {
        __ Ldr(trg, MemOperand(base, offset));
        if (is_volatile) {
          __ Dmb(vixl32::ISH);
        }
        assembler->MaybeUnpoisonHeapReference(trg);
      }
      break;
    }

    case DataType::Type::kInt64: {
      vixl32::Register trg_lo = LowRegisterFrom(trg_loc);
      vixl32::Register trg_hi = HighRegisterFrom(trg_loc);
      if (is_volatile && !codegen->GetInstructionSetFeatures().HasAtomicLdrdAndStrd()) {
        UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
        const vixl32::Register temp_reg = temps.Acquire();
        __ Add(temp_reg, base, offset);
        __ Ldrexd(trg_lo, trg_hi, MemOperand(temp_reg));
      } else {
        __ Ldrd(trg_lo, trg_hi, MemOperand(base, offset));
      }
      if (is_volatile) {
        __ Dmb(vixl32::ISH);
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected type " << type;
      UNREACHABLE();
  }
}

static void CreateIntIntIntToIntLocations(ArenaAllocator* allocator,
                                          HInvoke* invoke,
                                          DataType::Type type) {
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
  }
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(),
                    (can_call ? Location::kOutputOverlap : Location::kNoOutputOverlap));
  if (type == DataType::Type::kReference && kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // We need a temporary register for the read barrier marking slow
    // path in CodeGeneratorARMVIXL::GenerateReferenceLoadWithBakerReadBarrier.
    locations->AddTemp(Location::RequiresRegister());
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGet(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt32);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGetVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt32);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGetLong(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt64);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt64);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGetObject(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kReference);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kReference);
}

void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGetObject(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ true, codegen_);
}

static void CreateIntIntIntIntToVoid(ArenaAllocator* allocator,
                                     const ArmInstructionSetFeatures& features,
                                     DataType::Type type,
                                     bool is_volatile,
                                     HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());

  if (type == DataType::Type::kInt64) {
    // Potentially need temps for ldrexd-strexd loop.
    if (is_volatile && !features.HasAtomicLdrdAndStrd()) {
      locations->AddTemp(Location::RequiresRegister());  // Temp_lo.
      locations->AddTemp(Location::RequiresRegister());  // Temp_hi.
    }
  } else if (type == DataType::Type::kReference) {
    // Temps for card-marking.
    locations->AddTemp(Location::RequiresRegister());  // Temp.
    locations->AddTemp(Location::RequiresRegister());  // Card.
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt32, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt32, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt32, /* is_volatile */ true, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kReference, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kReference, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kReference, /* is_volatile */ true, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt64, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt64, /* is_volatile */ false, invoke);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoid(
      allocator_, features_, DataType::Type::kInt64, /* is_volatile */ true, invoke);
}

static void GenUnsafePut(LocationSummary* locations,
                         DataType::Type type,
                         bool is_volatile,
                         bool is_ordered,
                         CodeGeneratorARMVIXL* codegen) {
  ArmVIXLAssembler* assembler = codegen->GetAssembler();

  vixl32::Register base = RegisterFrom(locations->InAt(1));       // Object pointer.
  vixl32::Register offset = LowRegisterFrom(locations->InAt(2));  // Long offset, lo part only.
  vixl32::Register value;

  if (is_volatile || is_ordered) {
    __ Dmb(vixl32::ISH);
  }

  if (type == DataType::Type::kInt64) {
    vixl32::Register value_lo = LowRegisterFrom(locations->InAt(3));
    vixl32::Register value_hi = HighRegisterFrom(locations->InAt(3));
    value = value_lo;
    if (is_volatile && !codegen->GetInstructionSetFeatures().HasAtomicLdrdAndStrd()) {
      vixl32::Register temp_lo = RegisterFrom(locations->GetTemp(0));
      vixl32::Register temp_hi = RegisterFrom(locations->GetTemp(1));
      UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
      const vixl32::Register temp_reg = temps.Acquire();

      __ Add(temp_reg, base, offset);
      vixl32::Label loop_head;
      __ Bind(&loop_head);
      __ Ldrexd(temp_lo, temp_hi, MemOperand(temp_reg));
      __ Strexd(temp_lo, value_lo, value_hi, MemOperand(temp_reg));
      __ Cmp(temp_lo, 0);
      __ B(ne, &loop_head, /* far_target */ false);
    } else {
      __ Strd(value_lo, value_hi, MemOperand(base, offset));
    }
  } else {
    value = RegisterFrom(locations->InAt(3));
    vixl32::Register source = value;
    if (kPoisonHeapReferences && type == DataType::Type::kReference) {
      vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
      __ Mov(temp, value);
      assembler->PoisonHeapReference(temp);
      source = temp;
    }
    __ Str(source, MemOperand(base, offset));
  }

  if (is_volatile) {
    __ Dmb(vixl32::ISH);
  }

  if (type == DataType::Type::kReference) {
    vixl32::Register temp = RegisterFrom(locations->GetTemp(0));
    vixl32::Register card = RegisterFrom(locations->GetTemp(1));
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(temp, card, base, value, value_can_be_null);
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePut(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutObject(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutLong(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ false,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ true,
               codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt64,
               /* is_volatile */ true,
               /* is_ordered */ false,
               codegen_);
}

static void CreateIntIntIntIntIntToIntPlusTemps(ArenaAllocator* allocator,
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

  // Temporary registers used in CAS. In the object case
  // (UnsafeCASObject intrinsic), these are also used for
  // card-marking, and possibly for (Baker) read barrier.
  locations->AddTemp(Location::RequiresRegister());  // Pointer.
  locations->AddTemp(Location::RequiresRegister());  // Temp 1.
}

static void GenCas(HInvoke* invoke, DataType::Type type, CodeGeneratorARMVIXL* codegen) {
  DCHECK_NE(type, DataType::Type::kInt64);

  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Location out_loc = locations->Out();
  vixl32::Register out = OutputRegister(invoke);                      // Boolean result.

  vixl32::Register base = InputRegisterAt(invoke, 1);                 // Object pointer.
  Location offset_loc = locations->InAt(2);
  vixl32::Register offset = LowRegisterFrom(offset_loc);              // Offset (discard high 4B).
  vixl32::Register expected = InputRegisterAt(invoke, 3);             // Expected.
  vixl32::Register value = InputRegisterAt(invoke, 4);                // Value.

  Location tmp_ptr_loc = locations->GetTemp(0);
  vixl32::Register tmp_ptr = RegisterFrom(tmp_ptr_loc);               // Pointer to actual memory.
  vixl32::Register tmp = RegisterFrom(locations->GetTemp(1));         // Value in memory.

  if (type == DataType::Type::kReference) {
    // The only read barrier implementation supporting the
    // UnsafeCASObject intrinsic is the Baker-style read barriers.
    DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

    // Mark card for object assuming new value is stored. Worst case we will mark an unchanged
    // object and scan the receiver at the next GC for nothing.
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(tmp_ptr, tmp, base, value, value_can_be_null);

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      // Need to make sure the reference stored in the field is a to-space
      // one before attempting the CAS or the CAS could fail incorrectly.
      codegen->UpdateReferenceFieldWithBakerReadBarrier(
          invoke,
          out_loc,  // Unused, used only as a "temporary" within the read barrier.
          base,
          /* field_offset */ offset_loc,
          tmp_ptr_loc,
          /* needs_null_check */ false,
          tmp);
    }
  }

  // Prevent reordering with prior memory operations.
  // Emit a DMB ISH instruction instead of an DMB ISHST one, as the
  // latter allows a preceding load to be delayed past the STXR
  // instruction below.
  __ Dmb(vixl32::ISH);

  __ Add(tmp_ptr, base, offset);

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
  //   tmp = [r_ptr] - expected;
  // } while (tmp == 0 && failure([r_ptr] <- r_new_value));
  // result = tmp != 0;

  vixl32::Label loop_head;
  __ Bind(&loop_head);

  __ Ldrex(tmp, MemOperand(tmp_ptr));

  __ Subs(tmp, tmp, expected);

  {
    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           3 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);

    __ itt(eq);
    __ strex(eq, tmp, value, MemOperand(tmp_ptr));
    __ cmp(eq, tmp, 1);
  }

  __ B(eq, &loop_head, /* far_target */ false);

  __ Dmb(vixl32::ISH);

  __ Rsbs(out, tmp, 1);

  {
    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           2 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);

    __ it(cc);
    __ mov(cc, out, 0);
  }

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

void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeCASInt(HInvoke* invoke) {
  CreateIntIntIntIntIntToIntPlusTemps(allocator_, invoke, DataType::Type::kInt32);
}
void IntrinsicLocationsBuilderARMVIXL::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CreateIntIntIntIntIntToIntPlusTemps(allocator_, invoke, DataType::Type::kReference);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeCASInt(HInvoke* invoke) {
  GenCas(invoke, DataType::Type::kInt32, codegen_);
}
void IntrinsicCodeGeneratorARMVIXL::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  GenCas(invoke, DataType::Type::kReference, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringCompareTo(HInvoke* invoke) {
  // The inputs plus one temp.
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

// Forward declaration.
//
// ART build system imposes a size limit (deviceFrameSizeLimit) on the stack frames generated
// by the compiler for every C++ function, and if this function gets inlined in
// IntrinsicCodeGeneratorARMVIXL::VisitStringCompareTo, the limit will be exceeded, resulting in a
// build failure. That is the reason why NO_INLINE attribute is used.
static void NO_INLINE GenerateStringCompareToLoop(ArmVIXLAssembler* assembler,
                                                  HInvoke* invoke,
                                                  vixl32::Label* end,
                                                  vixl32::Label* different_compression);

void IntrinsicCodeGeneratorARMVIXL::VisitStringCompareTo(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  const vixl32::Register str = InputRegisterAt(invoke, 0);
  const vixl32::Register arg = InputRegisterAt(invoke, 1);
  const vixl32::Register out = OutputRegister(invoke);

  const vixl32::Register temp0 = RegisterFrom(locations->GetTemp(0));
  const vixl32::Register temp1 = RegisterFrom(locations->GetTemp(1));
  const vixl32::Register temp2 = RegisterFrom(locations->GetTemp(2));
  vixl32::Register temp3;
  if (mirror::kUseStringCompression) {
    temp3 = RegisterFrom(locations->GetTemp(3));
  }

  vixl32::Label end;
  vixl32::Label different_compression;

  // Get offsets of count and value fields within a string object.
  const int32_t count_offset = mirror::String::CountOffset().Int32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // Take slow path and throw if input can be and is null.
  SlowPathCodeARMVIXL* slow_path = nullptr;
  const bool can_slow_path = invoke->InputAt(1)->CanBeNull();
  if (can_slow_path) {
    slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
    codegen_->AddSlowPath(slow_path);
    __ CompareAndBranchIfZero(arg, slow_path->GetEntryLabel());
  }

  // Reference equality check, return 0 if same reference.
  __ Subs(out, str, arg);
  __ B(eq, &end);

  if (mirror::kUseStringCompression) {
    // Load `count` fields of this and argument strings.
    __ Ldr(temp3, MemOperand(str, count_offset));
    __ Ldr(temp2, MemOperand(arg, count_offset));
    // Extract lengths from the `count` fields.
    __ Lsr(temp0, temp3, 1u);
    __ Lsr(temp1, temp2, 1u);
  } else {
    // Load lengths of this and argument strings.
    __ Ldr(temp0, MemOperand(str, count_offset));
    __ Ldr(temp1, MemOperand(arg, count_offset));
  }
  // out = length diff.
  __ Subs(out, temp0, temp1);
  // temp0 = min(len(str), len(arg)).

  {
    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           2 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);

    __ it(gt);
    __ mov(gt, temp0, temp1);
  }

  // Shorter string is empty?
  // Note that mirror::kUseStringCompression==true introduces lots of instructions,
  // which makes &end label far away from this branch and makes it not 'CBZ-encodable'.
  __ CompareAndBranchIfZero(temp0, &end, mirror::kUseStringCompression);

  if (mirror::kUseStringCompression) {
    // Check if both strings using same compression style to use this comparison loop.
    __ Eors(temp2, temp2, temp3);
    __ Lsrs(temp2, temp2, 1u);
    __ B(cs, &different_compression);
    // For string compression, calculate the number of bytes to compare (not chars).
    // This could in theory exceed INT32_MAX, so treat temp0 as unsigned.
    __ Lsls(temp3, temp3, 31u);  // Extract purely the compression flag.

    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           2 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);

    __ it(ne);
    __ add(ne, temp0, temp0, temp0);
  }


  GenerateStringCompareToLoop(assembler, invoke, &end, &different_compression);

  __ Bind(&end);

  if (can_slow_path) {
    __ Bind(slow_path->GetExitLabel());
  }
}

static void GenerateStringCompareToLoop(ArmVIXLAssembler* assembler,
                                                  HInvoke* invoke,
                                                  vixl32::Label* end,
                                                  vixl32::Label* different_compression) {
  LocationSummary* locations = invoke->GetLocations();

  const vixl32::Register str = InputRegisterAt(invoke, 0);
  const vixl32::Register arg = InputRegisterAt(invoke, 1);
  const vixl32::Register out = OutputRegister(invoke);

  const vixl32::Register temp0 = RegisterFrom(locations->GetTemp(0));
  const vixl32::Register temp1 = RegisterFrom(locations->GetTemp(1));
  const vixl32::Register temp2 = RegisterFrom(locations->GetTemp(2));
  vixl32::Register temp3;
  if (mirror::kUseStringCompression) {
    temp3 = RegisterFrom(locations->GetTemp(3));
  }

  vixl32::Label loop;
  vixl32::Label find_char_diff;

  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();
  // Store offset of string value in preparation for comparison loop.
  __ Mov(temp1, value_offset);

  // Assertions that must hold in order to compare multiple characters at a time.
  CHECK_ALIGNED(value_offset, 8);
  static_assert(IsAligned<8>(kObjectAlignment),
                "String data must be 8-byte aligned for unrolled CompareTo loop.");

  const unsigned char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());

  vixl32::Label find_char_diff_2nd_cmp;
  // Unrolled loop comparing 4x16-bit chars per iteration (ok because of string data alignment).
  __ Bind(&loop);
  vixl32::Register temp_reg = temps.Acquire();
  __ Ldr(temp_reg, MemOperand(str, temp1));
  __ Ldr(temp2, MemOperand(arg, temp1));
  __ Cmp(temp_reg, temp2);
  __ B(ne, &find_char_diff, /* far_target */ false);
  __ Add(temp1, temp1, char_size * 2);

  __ Ldr(temp_reg, MemOperand(str, temp1));
  __ Ldr(temp2, MemOperand(arg, temp1));
  __ Cmp(temp_reg, temp2);
  __ B(ne, &find_char_diff_2nd_cmp, /* far_target */ false);
  __ Add(temp1, temp1, char_size * 2);
  // With string compression, we have compared 8 bytes, otherwise 4 chars.
  __ Subs(temp0, temp0, (mirror::kUseStringCompression ? 8 : 4));
  __ B(hi, &loop, /* far_target */ false);
  __ B(end);

  __ Bind(&find_char_diff_2nd_cmp);
  if (mirror::kUseStringCompression) {
    __ Subs(temp0, temp0, 4);  // 4 bytes previously compared.
    __ B(ls, end, /* far_target */ false);  // Was the second comparison fully beyond the end?
  } else {
    // Without string compression, we can start treating temp0 as signed
    // and rely on the signed comparison below.
    __ Sub(temp0, temp0, 2);
  }

  // Find the single character difference.
  __ Bind(&find_char_diff);
  // Get the bit position of the first character that differs.
  __ Eor(temp1, temp2, temp_reg);
  __ Rbit(temp1, temp1);
  __ Clz(temp1, temp1);

  // temp0 = number of characters remaining to compare.
  // (Without string compression, it could be < 1 if a difference is found by the second CMP
  // in the comparison loop, and after the end of the shorter string data).

  // Without string compression (temp1 >> 4) = character where difference occurs between the last
  // two words compared, in the interval [0,1].
  // (0 for low half-word different, 1 for high half-word different).
  // With string compression, (temp1 << 3) = byte where the difference occurs,
  // in the interval [0,3].

  // If temp0 <= (temp1 >> (kUseStringCompression ? 3 : 4)), the difference occurs outside
  // the remaining string data, so just return length diff (out).
  // The comparison is unsigned for string compression, otherwise signed.
  __ Cmp(temp0, Operand(temp1, vixl32::LSR, (mirror::kUseStringCompression ? 3 : 4)));
  __ B((mirror::kUseStringCompression ? ls : le), end, /* far_target */ false);

  // Extract the characters and calculate the difference.
  if (mirror::kUseStringCompression) {
    // For compressed strings we need to clear 0x7 from temp1, for uncompressed we need to clear
    // 0xf. We also need to prepare the character extraction mask `uncompressed ? 0xffffu : 0xffu`.
    // The compression flag is now in the highest bit of temp3, so let's play some tricks.
    __ Orr(temp3, temp3, 0xffu << 23);                  // uncompressed ? 0xff800000u : 0x7ff80000u
    __ Bic(temp1, temp1, Operand(temp3, vixl32::LSR, 31 - 3));  // &= ~(uncompressed ? 0xfu : 0x7u)
    __ Asr(temp3, temp3, 7u);                           // uncompressed ? 0xffff0000u : 0xff0000u.
    __ Lsr(temp2, temp2, temp1);                        // Extract second character.
    __ Lsr(temp3, temp3, 16u);                          // uncompressed ? 0xffffu : 0xffu
    __ Lsr(out, temp_reg, temp1);                       // Extract first character.
    __ And(temp2, temp2, temp3);
    __ And(out, out, temp3);
  } else {
    __ Bic(temp1, temp1, 0xf);
    __ Lsr(temp2, temp2, temp1);
    __ Lsr(out, temp_reg, temp1);
    __ Movt(temp2, 0);
    __ Movt(out, 0);
  }

  __ Sub(out, out, temp2);
  temps.Release(temp_reg);

  if (mirror::kUseStringCompression) {
    __ B(end);
    __ Bind(different_compression);

    // Comparison for different compression style.
    const size_t c_char_size = DataType::Size(DataType::Type::kInt8);
    DCHECK_EQ(c_char_size, 1u);

    // We want to free up the temp3, currently holding `str.count`, for comparison.
    // So, we move it to the bottom bit of the iteration count `temp0` which we tnen
    // need to treat as unsigned. Start by freeing the bit with an ADD and continue
    // further down by a LSRS+SBC which will flip the meaning of the flag but allow
    // `subs temp0, #2; bhi different_compression_loop` to serve as the loop condition.
    __ Add(temp0, temp0, temp0);              // Unlike LSL, this ADD is always 16-bit.
    // `temp1` will hold the compressed data pointer, `temp2` the uncompressed data pointer.
    __ Mov(temp1, str);
    __ Mov(temp2, arg);
    __ Lsrs(temp3, temp3, 1u);                // Continue the move of the compression flag.
    {
      ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                             3 * kMaxInstructionSizeInBytes,
                             CodeBufferCheckScope::kMaximumSize);
      __ itt(cs);                             // Interleave with selection of temp1 and temp2.
      __ mov(cs, temp1, arg);                 // Preserves flags.
      __ mov(cs, temp2, str);                 // Preserves flags.
    }
    __ Sbc(temp0, temp0, 0);                  // Complete the move of the compression flag.

    // Adjust temp1 and temp2 from string pointers to data pointers.
    __ Add(temp1, temp1, value_offset);
    __ Add(temp2, temp2, value_offset);

    vixl32::Label different_compression_loop;
    vixl32::Label different_compression_diff;

    // Main loop for different compression.
    temp_reg = temps.Acquire();
    __ Bind(&different_compression_loop);
    __ Ldrb(temp_reg, MemOperand(temp1, c_char_size, PostIndex));
    __ Ldrh(temp3, MemOperand(temp2, char_size, PostIndex));
    __ Cmp(temp_reg, temp3);
    __ B(ne, &different_compression_diff, /* far_target */ false);
    __ Subs(temp0, temp0, 2);
    __ B(hi, &different_compression_loop, /* far_target */ false);
    __ B(end);

    // Calculate the difference.
    __ Bind(&different_compression_diff);
    __ Sub(out, temp_reg, temp3);
    temps.Release(temp_reg);
    // Flip the difference if the `arg` is compressed.
    // `temp0` contains inverted `str` compression flag, i.e the same as `arg` compression flag.
    __ Lsrs(temp0, temp0, 1u);
    static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                  "Expecting 0=compressed, 1=uncompressed");

    ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                           2 * kMaxInstructionSizeInBytes,
                           CodeBufferCheckScope::kMaximumSize);
    __ it(cc);
    __ rsb(cc, out, out, 0);
  }
}

// The cut off for unrolling the loop in String.equals() intrinsic for const strings.
// The normal loop plus the pre-header is 9 instructions (18-26 bytes) without string compression
// and 12 instructions (24-32 bytes) with string compression. We can compare up to 4 bytes in 4
// instructions (LDR+LDR+CMP+BNE) and up to 8 bytes in 6 instructions (LDRD+LDRD+CMP+BNE+CMP+BNE).
// Allow up to 12 instructions (32 bytes) for the unrolled loop.
constexpr size_t kShortConstStringEqualsCutoffInBytes = 16;

static const char* GetConstString(HInstruction* candidate, uint32_t* utf16_length) {
  if (candidate->IsLoadString()) {
    HLoadString* load_string = candidate->AsLoadString();
    const DexFile& dex_file = load_string->GetDexFile();
    return dex_file.StringDataAndUtf16LengthByIdx(load_string->GetStringIndex(), utf16_length);
  }
  return nullptr;
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringEquals(HInvoke* invoke) {
  if (kEmitCompilerReadBarrier &&
      !StringEqualsOptimizations(invoke).GetArgumentIsString() &&
      !StringEqualsOptimizations(invoke).GetNoReadBarrierForStringClass()) {
    // No support for this odd case (String class is moveable, not in the boot image).
    return;
  }

  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());

  // Temporary registers to store lengths of strings and for calculations.
  // Using instruction cbz requires a low register, so explicitly set a temp to be R0.
  locations->AddTemp(LocationFrom(r0));

  // For the generic implementation and for long const strings we need an extra temporary.
  // We do not need it for short const strings, up to 4 bytes, see code generation below.
  uint32_t const_string_length = 0u;
  const char* const_string = GetConstString(invoke->InputAt(0), &const_string_length);
  if (const_string == nullptr) {
    const_string = GetConstString(invoke->InputAt(1), &const_string_length);
  }
  bool is_compressed =
      mirror::kUseStringCompression &&
      const_string != nullptr &&
      mirror::String::DexFileStringAllASCII(const_string, const_string_length);
  if (const_string == nullptr || const_string_length > (is_compressed ? 4u : 2u)) {
    locations->AddTemp(Location::RequiresRegister());
  }

  // TODO: If the String.equals() is used only for an immediately following HIf, we can
  // mark it as emitted-at-use-site and emit branches directly to the appropriate blocks.
  // Then we shall need an extra temporary register instead of the output register.
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringEquals(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  vixl32::Register str = InputRegisterAt(invoke, 0);
  vixl32::Register arg = InputRegisterAt(invoke, 1);
  vixl32::Register out = OutputRegister(invoke);

  vixl32::Register temp = RegisterFrom(locations->GetTemp(0));

  vixl32::Label loop;
  vixl32::Label end;
  vixl32::Label return_true;
  vixl32::Label return_false;
  vixl32::Label* final_label = codegen_->GetFinalLabel(invoke, &end);

  // Get offsets of count, value, and class fields within a string object.
  const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
  const uint32_t value_offset = mirror::String::ValueOffset().Uint32Value();
  const uint32_t class_offset = mirror::Object::ClassOffset().Uint32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  StringEqualsOptimizations optimizations(invoke);
  if (!optimizations.GetArgumentNotNull()) {
    // Check if input is null, return false if it is.
    __ CompareAndBranchIfZero(arg, &return_false, /* far_target */ false);
  }

  // Reference equality check, return true if same reference.
  __ Cmp(str, arg);
  __ B(eq, &return_true, /* far_target */ false);

  if (!optimizations.GetArgumentIsString()) {
    // Instanceof check for the argument by comparing class fields.
    // All string objects must have the same type since String cannot be subclassed.
    // Receiver must be a string object, so its class field is equal to all strings' class fields.
    // If the argument is a string object, its class field must be equal to receiver's class field.
    __ Ldr(temp, MemOperand(str, class_offset));
    __ Ldr(out, MemOperand(arg, class_offset));
    __ Cmp(temp, out);
    __ B(ne, &return_false, /* far_target */ false);
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
    __ Ldr(temp, MemOperand(arg, count_offset));
    __ Cmp(temp, Operand(mirror::String::GetFlaggedCount(const_string_length, is_compressed)));
    __ B(ne, &return_false, /* far_target */ false);
  } else {
    // Load `count` fields of this and argument strings.
    __ Ldr(temp, MemOperand(str, count_offset));
    __ Ldr(out, MemOperand(arg, count_offset));
    // Check if `count` fields are equal, return false if they're not.
    // Also compares the compression style, if differs return false.
    __ Cmp(temp, out);
    __ B(ne, &return_false, /* far_target */ false);
  }

  // Assertions that must hold in order to compare strings 4 bytes at a time.
  // Ok to do this because strings are zero-padded to kObjectAlignment.
  DCHECK_ALIGNED(value_offset, 4);
  static_assert(IsAligned<4>(kObjectAlignment), "String data must be aligned for fast compare.");

  if (const_string != nullptr &&
      const_string_length <= (is_compressed ? kShortConstStringEqualsCutoffInBytes
                                            : kShortConstStringEqualsCutoffInBytes / 2u)) {
    // Load and compare the contents. Though we know the contents of the short const string
    // at compile time, materializing constants may be more code than loading from memory.
    int32_t offset = value_offset;
    size_t remaining_bytes =
        RoundUp(is_compressed ? const_string_length : const_string_length * 2u, 4u);
    while (remaining_bytes > sizeof(uint32_t)) {
      vixl32::Register temp1 = RegisterFrom(locations->GetTemp(1));
      UseScratchRegisterScope scratch_scope(assembler->GetVIXLAssembler());
      vixl32::Register temp2 = scratch_scope.Acquire();
      __ Ldrd(temp, temp1, MemOperand(str, offset));
      __ Ldrd(temp2, out, MemOperand(arg, offset));
      __ Cmp(temp, temp2);
      __ B(ne, &return_false, /* far_label */ false);
      __ Cmp(temp1, out);
      __ B(ne, &return_false, /* far_label */ false);
      offset += 2u * sizeof(uint32_t);
      remaining_bytes -= 2u * sizeof(uint32_t);
    }
    if (remaining_bytes != 0u) {
      __ Ldr(temp, MemOperand(str, offset));
      __ Ldr(out, MemOperand(arg, offset));
      __ Cmp(temp, out);
      __ B(ne, &return_false, /* far_label */ false);
    }
  } else {
    // Return true if both strings are empty. Even with string compression `count == 0` means empty.
    static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                  "Expecting 0=compressed, 1=uncompressed");
    __ CompareAndBranchIfZero(temp, &return_true, /* far_target */ false);

    if (mirror::kUseStringCompression) {
      // For string compression, calculate the number of bytes to compare (not chars).
      // This could in theory exceed INT32_MAX, so treat temp as unsigned.
      __ Lsrs(temp, temp, 1u);                        // Extract length and check compression flag.
      ExactAssemblyScope aas(assembler->GetVIXLAssembler(),
                             2 * kMaxInstructionSizeInBytes,
                             CodeBufferCheckScope::kMaximumSize);
      __ it(cs);                                      // If uncompressed,
      __ add(cs, temp, temp, temp);                   //   double the byte count.
    }

    vixl32::Register temp1 = RegisterFrom(locations->GetTemp(1));
    UseScratchRegisterScope scratch_scope(assembler->GetVIXLAssembler());
    vixl32::Register temp2 = scratch_scope.Acquire();

    // Store offset of string value in preparation for comparison loop.
    __ Mov(temp1, value_offset);

    // Loop to compare strings 4 bytes at a time starting at the front of the string.
    __ Bind(&loop);
    __ Ldr(out, MemOperand(str, temp1));
    __ Ldr(temp2, MemOperand(arg, temp1));
    __ Add(temp1, temp1, Operand::From(sizeof(uint32_t)));
    __ Cmp(out, temp2);
    __ B(ne, &return_false, /* far_target */ false);
    // With string compression, we have compared 4 bytes, otherwise 2 chars.
    __ Subs(temp, temp, mirror::kUseStringCompression ? 4 : 2);
    __ B(hi, &loop, /* far_target */ false);
  }

  // Return true and exit the function.
  // If loop does not result in returning false, we return true.
  __ Bind(&return_true);
  __ Mov(out, 1);
  __ B(final_label);

  // Return false and exit the function.
  __ Bind(&return_false);
  __ Mov(out, 0);

  if (end.IsReferenced()) {
    __ Bind(&end);
  }
}

static void GenerateVisitStringIndexOf(HInvoke* invoke,
                                       ArmVIXLAssembler* assembler,
                                       CodeGeneratorARMVIXL* codegen,
                                       bool start_at_zero) {
  LocationSummary* locations = invoke->GetLocations();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // Check for code points > 0xFFFF. Either a slow-path check when we don't know statically,
  // or directly dispatch for a large constant, or omit slow-path for a small constant or a char.
  SlowPathCodeARMVIXL* slow_path = nullptr;
  HInstruction* code_point = invoke->InputAt(1);
  if (code_point->IsIntConstant()) {
    if (static_cast<uint32_t>(Int32ConstantFrom(code_point)) >
        std::numeric_limits<uint16_t>::max()) {
      // Always needs the slow-path. We could directly dispatch to it, but this case should be
      // rare, so for simplicity just put the full slow-path down and branch unconditionally.
      slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
      codegen->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
  } else if (code_point->GetType() != DataType::Type::kUint16) {
    vixl32::Register char_reg = InputRegisterAt(invoke, 1);
    // 0xffff is not modified immediate but 0x10000 is, so use `>= 0x10000` instead of `> 0xffff`.
    __ Cmp(char_reg, static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1);
    slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
    codegen->AddSlowPath(slow_path);
    __ B(hs, slow_path->GetEntryLabel());
  }

  if (start_at_zero) {
    vixl32::Register tmp_reg = RegisterFrom(locations->GetTemp(0));
    DCHECK(tmp_reg.Is(r2));
    // Start-index = 0.
    __ Mov(tmp_reg, 0);
  }

  codegen->InvokeRuntime(kQuickIndexOf, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickIndexOf, int32_t, void*, uint32_t, uint32_t>();

  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringIndexOf(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime calling convention. So it's
  // best to align the inputs accordingly.
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetOut(LocationFrom(r0));

  // Need to send start-index=0.
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(2)));
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringIndexOf(HInvoke* invoke) {
  GenerateVisitStringIndexOf(invoke, GetAssembler(), codegen_, /* start_at_zero */ true);
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringIndexOfAfter(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime calling convention. So it's
  // best to align the inputs accordingly.
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetOut(LocationFrom(r0));
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringIndexOfAfter(HInvoke* invoke) {
  GenerateVisitStringIndexOf(invoke, GetAssembler(), codegen_, /* start_at_zero */ false);
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringNewStringFromBytes(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetInAt(3, LocationFrom(calling_convention.GetRegisterAt(3)));
  locations->SetOut(LocationFrom(r0));
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringNewStringFromBytes(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  vixl32::Register byte_array = InputRegisterAt(invoke, 0);
  __ Cmp(byte_array, 0);
  SlowPathCodeARMVIXL* slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
  codegen_->AddSlowPath(slow_path);
  __ B(eq, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromBytes, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickAllocStringFromBytes, void*, void*, int32_t, int32_t, int32_t>();
  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringNewStringFromChars(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->SetOut(LocationFrom(r0));
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringNewStringFromChars(HInvoke* invoke) {
  // No need to emit code checking whether `locations->InAt(2)` is a null
  // pointer, as callers of the native method
  //
  //   java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
  //
  // all include a null check on `data` before calling that method.
  codegen_->InvokeRuntime(kQuickAllocStringFromChars, invoke, invoke->GetDexPc());
  CheckEntrypointTypes<kQuickAllocStringFromChars, void*, int32_t, int32_t, void*>();
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringNewStringFromString(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  locations->SetInAt(0, LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->SetOut(LocationFrom(r0));
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringNewStringFromString(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  vixl32::Register string_to_copy = InputRegisterAt(invoke, 0);
  __ Cmp(string_to_copy, 0);
  SlowPathCodeARMVIXL* slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
  codegen_->AddSlowPath(slow_path);
  __ B(eq, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromString, invoke, invoke->GetDexPc(), slow_path);
  CheckEntrypointTypes<kQuickAllocStringFromString, void*, void*>();

  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderARMVIXL::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CodeGenerator::CreateSystemArrayCopyLocationSummary(invoke);
  LocationSummary* locations = invoke->GetLocations();
  if (locations == nullptr) {
    return;
  }

  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dest_pos = invoke->InputAt(3)->AsIntConstant();
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();

  if (src_pos != nullptr && !assembler_->ShifterOperandCanAlwaysHold(src_pos->GetValue())) {
    locations->SetInAt(1, Location::RequiresRegister());
  }
  if (dest_pos != nullptr && !assembler_->ShifterOperandCanAlwaysHold(dest_pos->GetValue())) {
    locations->SetInAt(3, Location::RequiresRegister());
  }
  if (length != nullptr && !assembler_->ShifterOperandCanAlwaysHold(length->GetValue())) {
    locations->SetInAt(4, Location::RequiresRegister());
  }
  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // Temporary register IP cannot be used in
    // ReadBarrierSystemArrayCopySlowPathARM (because that register
    // is clobbered by ReadBarrierMarkRegX entry points). Get an extra
    // temporary register from the register allocator.
    locations->AddTemp(Location::RequiresRegister());
    CodeGeneratorARMVIXL* arm_codegen = down_cast<CodeGeneratorARMVIXL*>(codegen_);
    arm_codegen->MaybeAddBakerCcEntrypointTempForFields(locations);
  }
}

static void CheckPosition(ArmVIXLAssembler* assembler,
                          Location pos,
                          vixl32::Register input,
                          Location length,
                          SlowPathCodeARMVIXL* slow_path,
                          vixl32::Register temp,
                          bool length_is_input_length = false) {
  // Where is the length in the Array?
  const uint32_t length_offset = mirror::Array::LengthOffset().Uint32Value();

  if (pos.IsConstant()) {
    int32_t pos_const = Int32ConstantFrom(pos);
    if (pos_const == 0) {
      if (!length_is_input_length) {
        // Check that length(input) >= length.
        __ Ldr(temp, MemOperand(input, length_offset));
        if (length.IsConstant()) {
          __ Cmp(temp, Int32ConstantFrom(length));
        } else {
          __ Cmp(temp, RegisterFrom(length));
        }
        __ B(lt, slow_path->GetEntryLabel());
      }
    } else {
      // Check that length(input) >= pos.
      __ Ldr(temp, MemOperand(input, length_offset));
      __ Subs(temp, temp, pos_const);
      __ B(lt, slow_path->GetEntryLabel());

      // Check that (length(input) - pos) >= length.
      if (length.IsConstant()) {
        __ Cmp(temp, Int32ConstantFrom(length));
      } else {
        __ Cmp(temp, RegisterFrom(length));
      }
      __ B(lt, slow_path->GetEntryLabel());
    }
  } else if (length_is_input_length) {
    // The only way the copy can succeed is if pos is zero.
    vixl32::Register pos_reg = RegisterFrom(pos);
    __ CompareAndBranchIfNonZero(pos_reg, slow_path->GetEntryLabel());
  } else {
    // Check that pos >= 0.
    vixl32::Register pos_reg = RegisterFrom(pos);
    __ Cmp(pos_reg, 0);
    __ B(lt, slow_path->GetEntryLabel());

    // Check that pos <= length(input).
    __ Ldr(temp, MemOperand(input, length_offset));
    __ Subs(temp, temp, pos_reg);
    __ B(lt, slow_path->GetEntryLabel());

    // Check that (length(input) - pos) >= length.
    if (length.IsConstant()) {
      __ Cmp(temp, Int32ConstantFrom(length));
    } else {
      __ Cmp(temp, RegisterFrom(length));
    }
    __ B(lt, slow_path->GetEntryLabel());
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  ArmVIXLAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();

  vixl32::Register src = InputRegisterAt(invoke, 0);
  Location src_pos = locations->InAt(1);
  vixl32::Register dest = InputRegisterAt(invoke, 2);
  Location dest_pos = locations->InAt(3);
  Location length = locations->InAt(4);
  Location temp1_loc = locations->GetTemp(0);
  vixl32::Register temp1 = RegisterFrom(temp1_loc);
  Location temp2_loc = locations->GetTemp(1);
  vixl32::Register temp2 = RegisterFrom(temp2_loc);
  Location temp3_loc = locations->GetTemp(2);
  vixl32::Register temp3 = RegisterFrom(temp3_loc);

  SlowPathCodeARMVIXL* intrinsic_slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathARMVIXL(invoke);
  codegen_->AddSlowPath(intrinsic_slow_path);

  vixl32::Label conditions_on_positions_validated;
  SystemArrayCopyOptimizations optimizations(invoke);

  // If source and destination are the same, we go to slow path if we need to do
  // forward copying.
  if (src_pos.IsConstant()) {
    int32_t src_pos_constant = Int32ConstantFrom(src_pos);
    if (dest_pos.IsConstant()) {
      int32_t dest_pos_constant = Int32ConstantFrom(dest_pos);
      if (optimizations.GetDestinationIsSource()) {
        // Checked when building locations.
        DCHECK_GE(src_pos_constant, dest_pos_constant);
      } else if (src_pos_constant < dest_pos_constant) {
        __ Cmp(src, dest);
        __ B(eq, intrinsic_slow_path->GetEntryLabel());
      }

      // Checked when building locations.
      DCHECK(!optimizations.GetDestinationIsSource()
             || (src_pos_constant >= Int32ConstantFrom(dest_pos)));
    } else {
      if (!optimizations.GetDestinationIsSource()) {
        __ Cmp(src, dest);
        __ B(ne, &conditions_on_positions_validated, /* far_target */ false);
      }
      __ Cmp(RegisterFrom(dest_pos), src_pos_constant);
      __ B(gt, intrinsic_slow_path->GetEntryLabel());
    }
  } else {
    if (!optimizations.GetDestinationIsSource()) {
      __ Cmp(src, dest);
      __ B(ne, &conditions_on_positions_validated, /* far_target */ false);
    }
    if (dest_pos.IsConstant()) {
      int32_t dest_pos_constant = Int32ConstantFrom(dest_pos);
      __ Cmp(RegisterFrom(src_pos), dest_pos_constant);
    } else {
      __ Cmp(RegisterFrom(src_pos), RegisterFrom(dest_pos));
    }
    __ B(lt, intrinsic_slow_path->GetEntryLabel());
  }

  __ Bind(&conditions_on_positions_validated);

  if (!optimizations.GetSourceIsNotNull()) {
    // Bail out if the source is null.
    __ CompareAndBranchIfZero(src, intrinsic_slow_path->GetEntryLabel());
  }

  if (!optimizations.GetDestinationIsNotNull() && !optimizations.GetDestinationIsSource()) {
    // Bail out if the destination is null.
    __ CompareAndBranchIfZero(dest, intrinsic_slow_path->GetEntryLabel());
  }

  // If the length is negative, bail out.
  // We have already checked in the LocationsBuilder for the constant case.
  if (!length.IsConstant() &&
      !optimizations.GetCountIsSourceLength() &&
      !optimizations.GetCountIsDestinationLength()) {
    __ Cmp(RegisterFrom(length), 0);
    __ B(lt, intrinsic_slow_path->GetEntryLabel());
  }

  // Validity checks: source.
  CheckPosition(assembler,
                src_pos,
                src,
                length,
                intrinsic_slow_path,
                temp1,
                optimizations.GetCountIsSourceLength());

  // Validity checks: dest.
  CheckPosition(assembler,
                dest_pos,
                dest,
                length,
                intrinsic_slow_path,
                temp1,
                optimizations.GetCountIsDestinationLength());

  if (!optimizations.GetDoesNotNeedTypeCheck()) {
    // Check whether all elements of the source array are assignable to the component
    // type of the destination array. We do two checks: the classes are the same,
    // or the destination is Object[]. If none of these checks succeed, we go to the
    // slow path.

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      if (!optimizations.GetSourceIsNonPrimitiveArray()) {
        // /* HeapReference<Class> */ temp1 = src->klass_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, src, class_offset, temp2_loc, /* needs_null_check */ false);
        // Bail out if the source is not a non primitive array.
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, temp1, component_offset, temp2_loc, /* needs_null_check */ false);
        __ CompareAndBranchIfZero(temp1, intrinsic_slow_path->GetEntryLabel());
        // If heap poisoning is enabled, `temp1` has been unpoisoned
        // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
        // /* uint16_t */ temp1 = static_cast<uint16>(temp1->primitive_type_);
        __ Ldrh(temp1, MemOperand(temp1, primitive_offset));
        static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
        __ CompareAndBranchIfNonZero(temp1, intrinsic_slow_path->GetEntryLabel());
      }

      // /* HeapReference<Class> */ temp1 = dest->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp1_loc, dest, class_offset, temp2_loc, /* needs_null_check */ false);

      if (!optimizations.GetDestinationIsNonPrimitiveArray()) {
        // Bail out if the destination is not a non primitive array.
        //
        // Register `temp1` is not trashed by the read barrier emitted
        // by GenerateFieldLoadWithBakerReadBarrier below, as that
        // method produces a call to a ReadBarrierMarkRegX entry point,
        // which saves all potentially live registers, including
        // temporaries such a `temp1`.
        // /* HeapReference<Class> */ temp2 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp2_loc, temp1, component_offset, temp3_loc, /* needs_null_check */ false);
        __ CompareAndBranchIfZero(temp2, intrinsic_slow_path->GetEntryLabel());
        // If heap poisoning is enabled, `temp2` has been unpoisoned
        // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
        // /* uint16_t */ temp2 = static_cast<uint16>(temp2->primitive_type_);
        __ Ldrh(temp2, MemOperand(temp2, primitive_offset));
        static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
        __ CompareAndBranchIfNonZero(temp2, intrinsic_slow_path->GetEntryLabel());
      }

      // For the same reason given earlier, `temp1` is not trashed by the
      // read barrier emitted by GenerateFieldLoadWithBakerReadBarrier below.
      // /* HeapReference<Class> */ temp2 = src->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp2_loc, src, class_offset, temp3_loc, /* needs_null_check */ false);
      // Note: if heap poisoning is on, we are comparing two unpoisoned references here.
      __ Cmp(temp1, temp2);

      if (optimizations.GetDestinationIsTypedObjectArray()) {
        vixl32::Label do_copy;
        __ B(eq, &do_copy, /* far_target */ false);
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, temp1, component_offset, temp2_loc, /* needs_null_check */ false);
        // /* HeapReference<Class> */ temp1 = temp1->super_class_
        // We do not need to emit a read barrier for the following
        // heap reference load, as `temp1` is only used in a
        // comparison with null below, and this reference is not
        // kept afterwards.
        __ Ldr(temp1, MemOperand(temp1, super_offset));
        __ CompareAndBranchIfNonZero(temp1, intrinsic_slow_path->GetEntryLabel());
        __ Bind(&do_copy);
      } else {
        __ B(ne, intrinsic_slow_path->GetEntryLabel());
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
        assembler->MaybeUnpoisonHeapReference(temp1);
        assembler->MaybeUnpoisonHeapReference(temp2);
        did_unpoison = true;
      }

      if (!optimizations.GetDestinationIsNonPrimitiveArray()) {
        // Bail out if the destination is not a non primitive array.
        // /* HeapReference<Class> */ temp3 = temp1->component_type_
        __ Ldr(temp3, MemOperand(temp1, component_offset));
        __ CompareAndBranchIfZero(temp3, intrinsic_slow_path->GetEntryLabel());
        assembler->MaybeUnpoisonHeapReference(temp3);
        // /* uint16_t */ temp3 = static_cast<uint16>(temp3->primitive_type_);
        __ Ldrh(temp3, MemOperand(temp3, primitive_offset));
        static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
        __ CompareAndBranchIfNonZero(temp3, intrinsic_slow_path->GetEntryLabel());
      }

      if (!optimizations.GetSourceIsNonPrimitiveArray()) {
        // Bail out if the source is not a non primitive array.
        // /* HeapReference<Class> */ temp3 = temp2->component_type_
        __ Ldr(temp3, MemOperand(temp2, component_offset));
        __ CompareAndBranchIfZero(temp3, intrinsic_slow_path->GetEntryLabel());
        assembler->MaybeUnpoisonHeapReference(temp3);
        // /* uint16_t */ temp3 = static_cast<uint16>(temp3->primitive_type_);
        __ Ldrh(temp3, MemOperand(temp3, primitive_offset));
        static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
        __ CompareAndBranchIfNonZero(temp3, intrinsic_slow_path->GetEntryLabel());
      }

      __ Cmp(temp1, temp2);

      if (optimizations.GetDestinationIsTypedObjectArray()) {
        vixl32::Label do_copy;
        __ B(eq, &do_copy, /* far_target */ false);
        if (!did_unpoison) {
          assembler->MaybeUnpoisonHeapReference(temp1);
        }
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        __ Ldr(temp1, MemOperand(temp1, component_offset));
        assembler->MaybeUnpoisonHeapReference(temp1);
        // /* HeapReference<Class> */ temp1 = temp1->super_class_
        __ Ldr(temp1, MemOperand(temp1, super_offset));
        // No need to unpoison the result, we're comparing against null.
        __ CompareAndBranchIfNonZero(temp1, intrinsic_slow_path->GetEntryLabel());
        __ Bind(&do_copy);
      } else {
        __ B(ne, intrinsic_slow_path->GetEntryLabel());
      }
    }
  } else if (!optimizations.GetSourceIsNonPrimitiveArray()) {
    DCHECK(optimizations.GetDestinationIsNonPrimitiveArray());
    // Bail out if the source is not a non primitive array.
    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      // /* HeapReference<Class> */ temp1 = src->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp1_loc, src, class_offset, temp2_loc, /* needs_null_check */ false);
      // /* HeapReference<Class> */ temp3 = temp1->component_type_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp3_loc, temp1, component_offset, temp2_loc, /* needs_null_check */ false);
      __ CompareAndBranchIfZero(temp3, intrinsic_slow_path->GetEntryLabel());
      // If heap poisoning is enabled, `temp3` has been unpoisoned
      // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
    } else {
      // /* HeapReference<Class> */ temp1 = src->klass_
      __ Ldr(temp1, MemOperand(src, class_offset));
      assembler->MaybeUnpoisonHeapReference(temp1);
      // /* HeapReference<Class> */ temp3 = temp1->component_type_
      __ Ldr(temp3, MemOperand(temp1, component_offset));
      __ CompareAndBranchIfZero(temp3, intrinsic_slow_path->GetEntryLabel());
      assembler->MaybeUnpoisonHeapReference(temp3);
    }
    // /* uint16_t */ temp3 = static_cast<uint16>(temp3->primitive_type_);
    __ Ldrh(temp3, MemOperand(temp3, primitive_offset));
    static_assert(Primitive::kPrimNot == 0, "Expected 0 for kPrimNot");
    __ CompareAndBranchIfNonZero(temp3, intrinsic_slow_path->GetEntryLabel());
  }

  if (length.IsConstant() && Int32ConstantFrom(length) == 0) {
    // Null constant length: not need to emit the loop code at all.
  } else {
    vixl32::Label done;
    const DataType::Type type = DataType::Type::kReference;
    const int32_t element_size = DataType::Size(type);

    if (length.IsRegister()) {
      // Don't enter the copy loop if the length is null.
      __ CompareAndBranchIfZero(RegisterFrom(length), &done, /* is_far_target */ false);
    }

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      // TODO: Also convert this intrinsic to the IsGcMarking strategy?

      // SystemArrayCopy implementation for Baker read barriers (see
      // also CodeGeneratorARMVIXL::GenerateReferenceLoadWithBakerReadBarrier):
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

      // /* int32_t */ monitor = src->monitor_
      __ Ldr(temp2, MemOperand(src, monitor_offset));
      // /* LockWord */ lock_word = LockWord(monitor)
      static_assert(sizeof(LockWord) == sizeof(int32_t),
                    "art::LockWord and int32_t have different sizes.");

      // Introduce a dependency on the lock_word including the rb_state,
      // which shall prevent load-load reordering without using
      // a memory barrier (which would be more expensive).
      // `src` is unchanged by this operation, but its value now depends
      // on `temp2`.
      __ Add(src, src, Operand(temp2, vixl32::LSR, 32));

      // Compute the base source address in `temp1`.
      // Note that `temp1` (the base source address) is computed from
      // `src` (and `src_pos`) here, and thus honors the artificial
      // dependency of `src` on `temp2`.
      GenSystemArrayCopyBaseAddress(GetAssembler(), type, src, src_pos, temp1);
      // Compute the end source address in `temp3`.
      GenSystemArrayCopyEndAddress(GetAssembler(), type, length, temp1, temp3);
      // The base destination address is computed later, as `temp2` is
      // used for intermediate computations.

      // Slow path used to copy array when `src` is gray.
      // Note that the base destination address is computed in `temp2`
      // by the slow path code.
      SlowPathCodeARMVIXL* read_barrier_slow_path =
          new (codegen_->GetScopedAllocator()) ReadBarrierSystemArrayCopySlowPathARMVIXL(invoke);
      codegen_->AddSlowPath(read_barrier_slow_path);

      // Given the numeric representation, it's enough to check the low bit of the
      // rb_state. We do that by shifting the bit out of the lock word with LSRS
      // which can be a 16-bit instruction unlike the TST immediate.
      static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
      static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
      __ Lsrs(temp2, temp2, LockWord::kReadBarrierStateShift + 1);
      // Carry flag is the last bit shifted out by LSRS.
      __ B(cs, read_barrier_slow_path->GetEntryLabel());

      // Fast-path copy.
      // Compute the base destination address in `temp2`.
      GenSystemArrayCopyBaseAddress(GetAssembler(), type, dest, dest_pos, temp2);
      // Iterate over the arrays and do a raw copy of the objects. We don't need to
      // poison/unpoison.
      vixl32::Label loop;
      __ Bind(&loop);
      {
        UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
        const vixl32::Register temp_reg = temps.Acquire();
        __ Ldr(temp_reg, MemOperand(temp1, element_size, PostIndex));
        __ Str(temp_reg, MemOperand(temp2, element_size, PostIndex));
      }
      __ Cmp(temp1, temp3);
      __ B(ne, &loop, /* far_target */ false);

      __ Bind(read_barrier_slow_path->GetExitLabel());
    } else {
      // Non read barrier code.
      // Compute the base source address in `temp1`.
      GenSystemArrayCopyBaseAddress(GetAssembler(), type, src, src_pos, temp1);
      // Compute the base destination address in `temp2`.
      GenSystemArrayCopyBaseAddress(GetAssembler(), type, dest, dest_pos, temp2);
      // Compute the end source address in `temp3`.
      GenSystemArrayCopyEndAddress(GetAssembler(), type, length, temp1, temp3);
      // Iterate over the arrays and do a raw copy of the objects. We don't need to
      // poison/unpoison.
      vixl32::Label loop;
      __ Bind(&loop);
      {
        UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
        const vixl32::Register temp_reg = temps.Acquire();
        __ Ldr(temp_reg, MemOperand(temp1, element_size, PostIndex));
        __ Str(temp_reg, MemOperand(temp2, element_size, PostIndex));
      }
      __ Cmp(temp1, temp3);
      __ B(ne, &loop, /* far_target */ false);
    }
    __ Bind(&done);
  }

  // We only need one card marking on the destination array.
  codegen_->MarkGCCard(temp1, temp2, dest, NoReg, /* value_can_be_null */ false);

  __ Bind(intrinsic_slow_path->GetExitLabel());
}

static void CreateFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  // If the graph is debuggable, all callee-saved floating-point registers are blocked by
  // the code generator. Furthermore, the register allocator creates fixed live intervals
  // for all caller-saved registers because we are doing a function call. As a result, if
  // the input and output locations are unallocated, the register allocator runs out of
  // registers and fails; however, a debuggable graph is not the common case.
  if (invoke->GetBlock()->GetGraph()->IsDebuggable()) {
    return;
  }

  DCHECK_EQ(invoke->GetNumberOfArguments(), 1U);
  DCHECK_EQ(invoke->InputAt(0)->GetType(), DataType::Type::kFloat64);
  DCHECK_EQ(invoke->GetType(), DataType::Type::kFloat64);

  LocationSummary* const locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  const InvokeRuntimeCallingConventionARMVIXL calling_convention;

  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
  // Native code uses the soft float ABI.
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(1)));
}

static void CreateFPFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  // If the graph is debuggable, all callee-saved floating-point registers are blocked by
  // the code generator. Furthermore, the register allocator creates fixed live intervals
  // for all caller-saved registers because we are doing a function call. As a result, if
  // the input and output locations are unallocated, the register allocator runs out of
  // registers and fails; however, a debuggable graph is not the common case.
  if (invoke->GetBlock()->GetGraph()->IsDebuggable()) {
    return;
  }

  DCHECK_EQ(invoke->GetNumberOfArguments(), 2U);
  DCHECK_EQ(invoke->InputAt(0)->GetType(), DataType::Type::kFloat64);
  DCHECK_EQ(invoke->InputAt(1)->GetType(), DataType::Type::kFloat64);
  DCHECK_EQ(invoke->GetType(), DataType::Type::kFloat64);

  LocationSummary* const locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  const InvokeRuntimeCallingConventionARMVIXL calling_convention;

  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
  // Native code uses the soft float ABI.
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(0)));
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(1)));
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(2)));
  locations->AddTemp(LocationFrom(calling_convention.GetRegisterAt(3)));
}

static void GenFPToFPCall(HInvoke* invoke,
                          ArmVIXLAssembler* assembler,
                          CodeGeneratorARMVIXL* codegen,
                          QuickEntrypointEnum entry) {
  LocationSummary* const locations = invoke->GetLocations();

  DCHECK_EQ(invoke->GetNumberOfArguments(), 1U);
  DCHECK(locations->WillCall() && locations->Intrinsified());

  // Native code uses the soft float ABI.
  __ Vmov(RegisterFrom(locations->GetTemp(0)),
          RegisterFrom(locations->GetTemp(1)),
          InputDRegisterAt(invoke, 0));
  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());
  __ Vmov(OutputDRegister(invoke),
          RegisterFrom(locations->GetTemp(0)),
          RegisterFrom(locations->GetTemp(1)));
}

static void GenFPFPToFPCall(HInvoke* invoke,
                            ArmVIXLAssembler* assembler,
                            CodeGeneratorARMVIXL* codegen,
                            QuickEntrypointEnum entry) {
  LocationSummary* const locations = invoke->GetLocations();

  DCHECK_EQ(invoke->GetNumberOfArguments(), 2U);
  DCHECK(locations->WillCall() && locations->Intrinsified());

  // Native code uses the soft float ABI.
  __ Vmov(RegisterFrom(locations->GetTemp(0)),
          RegisterFrom(locations->GetTemp(1)),
          InputDRegisterAt(invoke, 0));
  __ Vmov(RegisterFrom(locations->GetTemp(2)),
          RegisterFrom(locations->GetTemp(3)),
          InputDRegisterAt(invoke, 1));
  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());
  __ Vmov(OutputDRegister(invoke),
          RegisterFrom(locations->GetTemp(0)),
          RegisterFrom(locations->GetTemp(1)));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathCos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathCos(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickCos);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathSin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathSin(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickSin);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAcos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAcos(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickAcos);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAsin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAsin(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickAsin);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAtan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAtan(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickAtan);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathCbrt(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathCbrt(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickCbrt);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathCosh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathCosh(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickCosh);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathExp(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathExp(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickExp);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathExpm1(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathExpm1(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickExpm1);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathLog(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathLog(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickLog);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathLog10(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathLog10(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickLog10);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathSinh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathSinh(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickSinh);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathTan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathTan(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickTan);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathTanh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathTanh(HInvoke* invoke) {
  GenFPToFPCall(invoke, GetAssembler(), codegen_, kQuickTanh);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathAtan2(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathAtan2(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, GetAssembler(), codegen_, kQuickAtan2);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathPow(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathPow(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, GetAssembler(), codegen_, kQuickPow);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathHypot(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathHypot(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, GetAssembler(), codegen_, kQuickHypot);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathNextAfter(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathNextAfter(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, GetAssembler(), codegen_, kQuickNextAfter);
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerReverse(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerReverse(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Rbit(OutputRegister(invoke), InputRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongReverse(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongReverse(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  vixl32::Register in_reg_lo  = LowRegisterFrom(locations->InAt(0));
  vixl32::Register in_reg_hi  = HighRegisterFrom(locations->InAt(0));
  vixl32::Register out_reg_lo = LowRegisterFrom(locations->Out());
  vixl32::Register out_reg_hi = HighRegisterFrom(locations->Out());

  __ Rbit(out_reg_lo, in_reg_hi);
  __ Rbit(out_reg_hi, in_reg_lo);
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerReverseBytes(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Rev(OutputRegister(invoke), InputRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongReverseBytes(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongReverseBytes(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  vixl32::Register in_reg_lo  = LowRegisterFrom(locations->InAt(0));
  vixl32::Register in_reg_hi  = HighRegisterFrom(locations->InAt(0));
  vixl32::Register out_reg_lo = LowRegisterFrom(locations->Out());
  vixl32::Register out_reg_hi = HighRegisterFrom(locations->Out());

  __ Rev(out_reg_lo, in_reg_hi);
  __ Rev(out_reg_hi, in_reg_lo);
}

void IntrinsicLocationsBuilderARMVIXL::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitShortReverseBytes(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  __ Revsh(OutputRegister(invoke), InputRegisterAt(invoke, 0));
}

static void GenBitCount(HInvoke* instr, DataType::Type type, ArmVIXLAssembler* assembler) {
  DCHECK(DataType::IsIntOrLongType(type)) << type;
  DCHECK_EQ(instr->GetType(), DataType::Type::kInt32);
  DCHECK_EQ(DataType::Kind(instr->InputAt(0)->GetType()), type);

  bool is_long = type == DataType::Type::kInt64;
  LocationSummary* locations = instr->GetLocations();
  Location in = locations->InAt(0);
  vixl32::Register src_0 = is_long ? LowRegisterFrom(in) : RegisterFrom(in);
  vixl32::Register src_1 = is_long ? HighRegisterFrom(in) : src_0;
  vixl32::SRegister tmp_s = LowSRegisterFrom(locations->GetTemp(0));
  vixl32::DRegister tmp_d = DRegisterFrom(locations->GetTemp(0));
  vixl32::Register  out_r = OutputRegister(instr);

  // Move data from core register(s) to temp D-reg for bit count calculation, then move back.
  // According to Cortex A57 and A72 optimization guides, compared to transferring to full D-reg,
  // transferring data from core reg to upper or lower half of vfp D-reg requires extra latency,
  // That's why for integer bit count, we use 'vmov d0, r0, r0' instead of 'vmov d0[0], r0'.
  __ Vmov(tmp_d, src_1, src_0);     // Temp DReg |--src_1|--src_0|
  __ Vcnt(Untyped8, tmp_d, tmp_d);  // Temp DReg |c|c|c|c|c|c|c|c|
  __ Vpaddl(U8, tmp_d, tmp_d);      // Temp DReg |--c|--c|--c|--c|
  __ Vpaddl(U16, tmp_d, tmp_d);     // Temp DReg |------c|------c|
  if (is_long) {
    __ Vpaddl(U32, tmp_d, tmp_d);   // Temp DReg |--------------c|
  }
  __ Vmov(out_r, tmp_s);
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
  invoke->GetLocations()->AddTemp(Location::RequiresFpuRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerBitCount(HInvoke* invoke) {
  GenBitCount(invoke, DataType::Type::kInt32, GetAssembler());
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongBitCount(HInvoke* invoke) {
  VisitIntegerBitCount(invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongBitCount(HInvoke* invoke) {
  GenBitCount(invoke, DataType::Type::kInt64, GetAssembler());
}

static void GenHighestOneBit(HInvoke* invoke,
                             DataType::Type type,
                             CodeGeneratorARMVIXL* codegen) {
  DCHECK(DataType::IsIntOrLongType(type));

  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  const vixl32::Register temp = temps.Acquire();

  if (type == DataType::Type::kInt64) {
    LocationSummary* locations = invoke->GetLocations();
    Location in = locations->InAt(0);
    Location out = locations->Out();

    vixl32::Register in_reg_lo = LowRegisterFrom(in);
    vixl32::Register in_reg_hi = HighRegisterFrom(in);
    vixl32::Register out_reg_lo = LowRegisterFrom(out);
    vixl32::Register out_reg_hi = HighRegisterFrom(out);

    __ Mov(temp, 0x80000000);  // Modified immediate.
    __ Clz(out_reg_lo, in_reg_lo);
    __ Clz(out_reg_hi, in_reg_hi);
    __ Lsr(out_reg_lo, temp, out_reg_lo);
    __ Lsrs(out_reg_hi, temp, out_reg_hi);

    // Discard result for lowest 32 bits if highest 32 bits are not zero.
    // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
    // we check that the output is in a low register, so that a 16-bit MOV
    // encoding can be used. If output is in a high register, then we generate
    // 4 more bytes of code to avoid a branch.
    Operand mov_src(0);
    if (!out_reg_lo.IsLow()) {
      __ Mov(LeaveFlags, temp, 0);
      mov_src = Operand(temp);
    }
    ExactAssemblyScope it_scope(codegen->GetVIXLAssembler(),
                                  2 * vixl32::k16BitT32InstructionSizeInBytes,
                                  CodeBufferCheckScope::kExactSize);
    __ it(ne);
    __ mov(ne, out_reg_lo, mov_src);
  } else {
    vixl32::Register out = OutputRegister(invoke);
    vixl32::Register in = InputRegisterAt(invoke, 0);

    __ Mov(temp, 0x80000000);  // Modified immediate.
    __ Clz(out, in);
    __ Lsr(out, temp, out);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke, DataType::Type::kInt32, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongHighestOneBit(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke, DataType::Type::kInt64, codegen_);
}

static void GenLowestOneBit(HInvoke* invoke,
                            DataType::Type type,
                            CodeGeneratorARMVIXL* codegen) {
  DCHECK(DataType::IsIntOrLongType(type));

  ArmVIXLAssembler* assembler = codegen->GetAssembler();
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  const vixl32::Register temp = temps.Acquire();

  if (type == DataType::Type::kInt64) {
    LocationSummary* locations = invoke->GetLocations();
    Location in = locations->InAt(0);
    Location out = locations->Out();

    vixl32::Register in_reg_lo = LowRegisterFrom(in);
    vixl32::Register in_reg_hi = HighRegisterFrom(in);
    vixl32::Register out_reg_lo = LowRegisterFrom(out);
    vixl32::Register out_reg_hi = HighRegisterFrom(out);

    __ Rsb(out_reg_hi, in_reg_hi, 0);
    __ Rsb(out_reg_lo, in_reg_lo, 0);
    __ And(out_reg_hi, out_reg_hi, in_reg_hi);
    // The result of this operation is 0 iff in_reg_lo is 0
    __ Ands(out_reg_lo, out_reg_lo, in_reg_lo);

    // Discard result for highest 32 bits if lowest 32 bits are not zero.
    // Since IT blocks longer than a 16-bit instruction are deprecated by ARMv8,
    // we check that the output is in a low register, so that a 16-bit MOV
    // encoding can be used. If output is in a high register, then we generate
    // 4 more bytes of code to avoid a branch.
    Operand mov_src(0);
    if (!out_reg_lo.IsLow()) {
      __ Mov(LeaveFlags, temp, 0);
      mov_src = Operand(temp);
    }
    ExactAssemblyScope it_scope(codegen->GetVIXLAssembler(),
                                  2 * vixl32::k16BitT32InstructionSizeInBytes,
                                  CodeBufferCheckScope::kExactSize);
    __ it(ne);
    __ mov(ne, out_reg_hi, mov_src);
  } else {
    vixl32::Register out = OutputRegister(invoke);
    vixl32::Register in = InputRegisterAt(invoke, 0);

    __ Rsb(temp, in, 0);
    __ And(out, temp, in);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke, DataType::Type::kInt32, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitLongLowestOneBit(HInvoke* invoke) {
  CreateLongToLongLocationsWithOverlap(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitLongLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke, DataType::Type::kInt64, codegen_);
}

void IntrinsicLocationsBuilderARMVIXL::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  locations->SetInAt(4, Location::RequiresRegister());

  // Temporary registers to store lengths of strings and for calculations.
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
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
  vixl32::Register srcObj = InputRegisterAt(invoke, 0);
  vixl32::Register srcBegin = InputRegisterAt(invoke, 1);
  vixl32::Register srcEnd = InputRegisterAt(invoke, 2);
  vixl32::Register dstObj = InputRegisterAt(invoke, 3);
  vixl32::Register dstBegin = InputRegisterAt(invoke, 4);

  vixl32::Register num_chr = RegisterFrom(locations->GetTemp(0));
  vixl32::Register src_ptr = RegisterFrom(locations->GetTemp(1));
  vixl32::Register dst_ptr = RegisterFrom(locations->GetTemp(2));

  vixl32::Label done, compressed_string_loop;
  vixl32::Label* final_label = codegen_->GetFinalLabel(invoke, &done);
  // dst to be copied.
  __ Add(dst_ptr, dstObj, data_offset);
  __ Add(dst_ptr, dst_ptr, Operand(dstBegin, vixl32::LSL, 1));

  __ Subs(num_chr, srcEnd, srcBegin);
  // Early out for valid zero-length retrievals.
  __ B(eq, final_label, /* far_target */ false);

  // src range to copy.
  __ Add(src_ptr, srcObj, value_offset);

  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  vixl32::Register temp;
  vixl32::Label compressed_string_preloop;
  if (mirror::kUseStringCompression) {
    // Location of count in string.
    const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
    temp = temps.Acquire();
    // String's length.
    __ Ldr(temp, MemOperand(srcObj, count_offset));
    __ Tst(temp, 1);
    temps.Release(temp);
    __ B(eq, &compressed_string_preloop, /* far_target */ false);
  }
  __ Add(src_ptr, src_ptr, Operand(srcBegin, vixl32::LSL, 1));

  // Do the copy.
  vixl32::Label loop, remainder;

  temp = temps.Acquire();
  // Save repairing the value of num_chr on the < 4 character path.
  __ Subs(temp, num_chr, 4);
  __ B(lt, &remainder, /* far_target */ false);

  // Keep the result of the earlier subs, we are going to fetch at least 4 characters.
  __ Mov(num_chr, temp);

  // Main loop used for longer fetches loads and stores 4x16-bit characters at a time.
  // (LDRD/STRD fault on unaligned addresses and it's not worth inlining extra code
  // to rectify these everywhere this intrinsic applies.)
  __ Bind(&loop);
  __ Ldr(temp, MemOperand(src_ptr, char_size * 2));
  __ Subs(num_chr, num_chr, 4);
  __ Str(temp, MemOperand(dst_ptr, char_size * 2));
  __ Ldr(temp, MemOperand(src_ptr, char_size * 4, PostIndex));
  __ Str(temp, MemOperand(dst_ptr, char_size * 4, PostIndex));
  temps.Release(temp);
  __ B(ge, &loop, /* far_target */ false);

  __ Adds(num_chr, num_chr, 4);
  __ B(eq, final_label, /* far_target */ false);

  // Main loop for < 4 character case and remainder handling. Loads and stores one
  // 16-bit Java character at a time.
  __ Bind(&remainder);
  temp = temps.Acquire();
  __ Ldrh(temp, MemOperand(src_ptr, char_size, PostIndex));
  __ Subs(num_chr, num_chr, 1);
  __ Strh(temp, MemOperand(dst_ptr, char_size, PostIndex));
  temps.Release(temp);
  __ B(gt, &remainder, /* far_target */ false);

  if (mirror::kUseStringCompression) {
    __ B(final_label);

    const size_t c_char_size = DataType::Size(DataType::Type::kInt8);
    DCHECK_EQ(c_char_size, 1u);
    // Copy loop for compressed src, copying 1 character (8-bit) to (16-bit) at a time.
    __ Bind(&compressed_string_preloop);
    __ Add(src_ptr, src_ptr, srcBegin);
    __ Bind(&compressed_string_loop);
    temp = temps.Acquire();
    __ Ldrb(temp, MemOperand(src_ptr, c_char_size, PostIndex));
    __ Strh(temp, MemOperand(dst_ptr, char_size, PostIndex));
    temps.Release(temp);
    __ Subs(num_chr, num_chr, 1);
    __ B(gt, &compressed_string_loop, /* far_target */ false);
  }

  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitFloatIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitFloatIsInfinite(HInvoke* invoke) {
  ArmVIXLAssembler* const assembler = GetAssembler();
  const vixl32::Register out = OutputRegister(invoke);
  // Shifting left by 1 bit makes the value encodable as an immediate operand;
  // we don't care about the sign bit anyway.
  constexpr uint32_t infinity = kPositiveInfinityFloat << 1U;

  __ Vmov(out, InputSRegisterAt(invoke, 0));
  // We don't care about the sign bit, so shift left.
  __ Lsl(out, out, 1);
  __ Eor(out, out, infinity);
  codegen_->GenerateConditionWithZero(kCondEQ, out, out);
}

void IntrinsicLocationsBuilderARMVIXL::VisitDoubleIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorARMVIXL::VisitDoubleIsInfinite(HInvoke* invoke) {
  ArmVIXLAssembler* const assembler = GetAssembler();
  const vixl32::Register out = OutputRegister(invoke);
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  const vixl32::Register temp = temps.Acquire();
  // The highest 32 bits of double precision positive infinity separated into
  // two constants encodable as immediate operands.
  constexpr uint32_t infinity_high  = 0x7f000000U;
  constexpr uint32_t infinity_high2 = 0x00f00000U;

  static_assert((infinity_high | infinity_high2) ==
                    static_cast<uint32_t>(kPositiveInfinityDouble >> 32U),
                "The constants do not add up to the high 32 bits of double "
                "precision positive infinity.");
  __ Vmov(temp, out, InputDRegisterAt(invoke, 0));
  __ Eor(out, out, infinity_high);
  __ Eor(out, out, infinity_high2);
  // We don't care about the sign bit, so shift left.
  __ Orr(out, temp, Operand(out, vixl32::LSL, 1));
  codegen_->GenerateConditionWithZero(kCondEQ, out, out);
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathCeil(HInvoke* invoke) {
  if (features_.HasARMv8AInstructions()) {
    CreateFPToFPLocations(allocator_, invoke);
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathCeil(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  DCHECK(codegen_->GetInstructionSetFeatures().HasARMv8AInstructions());
  __ Vrintp(F64, F64, OutputDRegister(invoke), InputDRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitMathFloor(HInvoke* invoke) {
  if (features_.HasARMv8AInstructions()) {
    CreateFPToFPLocations(allocator_, invoke);
  }
}

void IntrinsicCodeGeneratorARMVIXL::VisitMathFloor(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  DCHECK(codegen_->GetInstructionSetFeatures().HasARMv8AInstructions());
  __ Vrintm(F64, F64, OutputDRegister(invoke), InputDRegisterAt(invoke, 0));
}

void IntrinsicLocationsBuilderARMVIXL::VisitIntegerValueOf(HInvoke* invoke) {
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  IntrinsicVisitor::ComputeIntegerValueOfLocations(
      invoke,
      codegen_,
      LocationFrom(r0),
      LocationFrom(calling_convention.GetRegisterAt(0)));
}

void IntrinsicCodeGeneratorARMVIXL::VisitIntegerValueOf(HInvoke* invoke) {
  IntrinsicVisitor::IntegerValueOfInfo info = IntrinsicVisitor::ComputeIntegerValueOfInfo();
  LocationSummary* locations = invoke->GetLocations();
  ArmVIXLAssembler* const assembler = GetAssembler();

  vixl32::Register out = RegisterFrom(locations->Out());
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  InvokeRuntimeCallingConventionARMVIXL calling_convention;
  vixl32::Register argument = calling_convention.GetRegisterAt(0);
  if (invoke->InputAt(0)->IsConstant()) {
    int32_t value = invoke->InputAt(0)->AsIntConstant()->GetValue();
    if (value >= info.low && value <= info.high) {
      // Just embed the j.l.Integer in the code.
      ScopedObjectAccess soa(Thread::Current());
      mirror::Object* boxed = info.cache->Get(value + (-info.low));
      DCHECK(boxed != nullptr && Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boxed));
      uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(boxed));
      __ Ldr(out, codegen_->DeduplicateBootImageAddressLiteral(address));
    } else {
      // Allocate and initialize a new j.l.Integer.
      // TODO: If we JIT, we could allocate the j.l.Integer now, and store it in the
      // JIT object table.
      uint32_t address =
          dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
      __ Ldr(argument, codegen_->DeduplicateBootImageAddressLiteral(address));
      codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
      CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
      __ Mov(temp, value);
      assembler->StoreToOffset(kStoreWord, temp, out, info.value_offset);
      // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
      // one.
      codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    }
  } else {
    vixl32::Register in = RegisterFrom(locations->InAt(0));
    // Check bounds of our cache.
    __ Add(out, in, -info.low);
    __ Cmp(out, info.high - info.low + 1);
    vixl32::Label allocate, done;
    __ B(hs, &allocate, /* is_far_target */ false);
    // If the value is within the bounds, load the j.l.Integer directly from the array.
    uint32_t data_offset = mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();
    uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.cache));
    __ Ldr(temp, codegen_->DeduplicateBootImageAddressLiteral(data_offset + address));
    codegen_->LoadFromShiftedRegOffset(DataType::Type::kReference, locations->Out(), temp, out);
    assembler->MaybeUnpoisonHeapReference(out);
    __ B(&done);
    __ Bind(&allocate);
    // Otherwise allocate and initialize a new j.l.Integer.
    address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
    __ Ldr(argument, codegen_->DeduplicateBootImageAddressLiteral(address));
    codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
    assembler->StoreToOffset(kStoreWord, in, out, info.value_offset);
    // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
    // one.
    codegen_->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitThreadInterrupted(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorARMVIXL::VisitThreadInterrupted(HInvoke* invoke) {
  ArmVIXLAssembler* assembler = GetAssembler();
  vixl32::Register out = RegisterFrom(invoke->GetLocations()->Out());
  int32_t offset = Thread::InterruptedOffset<kArmPointerSize>().Int32Value();
  __ Ldr(out, MemOperand(tr, offset));
  UseScratchRegisterScope temps(assembler->GetVIXLAssembler());
  vixl32::Register temp = temps.Acquire();
  vixl32::Label done;
  vixl32::Label* const final_label = codegen_->GetFinalLabel(invoke, &done);
  __ CompareAndBranchIfZero(out, final_label, /* far_target */ false);
  __ Dmb(vixl32::ISH);
  __ Mov(temp, 0);
  assembler->StoreToOffset(kStoreWord, temp, tr, offset);
  __ Dmb(vixl32::ISH);
  if (done.IsReferenced()) {
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderARMVIXL::VisitReachabilityFence(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::Any());
}

void IntrinsicCodeGeneratorARMVIXL::VisitReachabilityFence(HInvoke* invoke ATTRIBUTE_UNUSED) { }

UNIMPLEMENTED_INTRINSIC(ARMVIXL, MathRoundDouble)   // Could be done by changing rounding mode, maybe?
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeCASLong)     // High register pressure.
UNIMPLEMENTED_INTRINSIC(ARMVIXL, SystemArrayCopyChar)
UNIMPLEMENTED_INTRINSIC(ARMVIXL, ReferenceGetReferent)

UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringStringIndexOf);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringStringIndexOfAfter);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBufferAppend);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBufferLength);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBufferToString);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBuilderAppend);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBuilderLength);
UNIMPLEMENTED_INTRINSIC(ARMVIXL, StringBuilderToString);

// 1.8.
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeGetAndAddInt)
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeGetAndAddLong)
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeGetAndSetInt)
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeGetAndSetLong)
UNIMPLEMENTED_INTRINSIC(ARMVIXL, UnsafeGetAndSetObject)

UNREACHABLE_INTRINSICS(ARMVIXL)

#undef __

}  // namespace arm
}  // namespace art
