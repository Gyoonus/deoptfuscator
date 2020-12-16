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

#include "intrinsics_x86.h"

#include <limits>

#include "arch/x86/instruction_set_features_x86.h"
#include "art_method.h"
#include "base/bit_utils.h"
#include "code_generator_x86.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "intrinsics_utils.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/reference.h"
#include "mirror/string.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "utils/x86/assembler_x86.h"
#include "utils/x86/constants_x86.h"

namespace art {

namespace x86 {

static constexpr int kDoubleNaNHigh = 0x7FF80000;
static constexpr int kDoubleNaNLow = 0x00000000;
static constexpr int64_t kDoubleNaN = INT64_C(0x7FF8000000000000);
static constexpr int32_t kFloatNaN = INT32_C(0x7FC00000);

IntrinsicLocationsBuilderX86::IntrinsicLocationsBuilderX86(CodeGeneratorX86* codegen)
  : allocator_(codegen->GetGraph()->GetAllocator()),
    codegen_(codegen) {
}


X86Assembler* IntrinsicCodeGeneratorX86::GetAssembler() {
  return down_cast<X86Assembler*>(codegen_->GetAssembler());
}

ArenaAllocator* IntrinsicCodeGeneratorX86::GetAllocator() {
  return codegen_->GetGraph()->GetAllocator();
}

bool IntrinsicLocationsBuilderX86::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  if (res == nullptr) {
    return false;
  }
  return res->Intrinsified();
}

static void MoveArguments(HInvoke* invoke, CodeGeneratorX86* codegen) {
  InvokeDexCallingConventionVisitorX86 calling_convention_visitor;
  IntrinsicVisitor::MoveArguments(invoke, codegen, &calling_convention_visitor);
}

using IntrinsicSlowPathX86 = IntrinsicSlowPath<InvokeDexCallingConventionVisitorX86>;

// NOLINT on __ macro to suppress wrong warning/fix (misc-macro-parentheses) from clang-tidy.
#define __ down_cast<X86Assembler*>(codegen->GetAssembler())->  // NOLINT

// Slow path implementing the SystemArrayCopy intrinsic copy loop with read barriers.
class ReadBarrierSystemArrayCopySlowPathX86 : public SlowPathCode {
 public:
  explicit ReadBarrierSystemArrayCopySlowPathX86(HInstruction* instruction)
      : SlowPathCode(instruction) {
    DCHECK(kEmitCompilerReadBarrier);
    DCHECK(kUseBakerReadBarrier);
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    CodeGeneratorX86* x86_codegen = down_cast<CodeGeneratorX86*>(codegen);
    LocationSummary* locations = instruction_->GetLocations();
    DCHECK(locations->CanCall());
    DCHECK(instruction_->IsInvokeStaticOrDirect())
        << "Unexpected instruction in read barrier arraycopy slow path: "
        << instruction_->DebugName();
    DCHECK(instruction_->GetLocations()->Intrinsified());
    DCHECK_EQ(instruction_->AsInvoke()->GetIntrinsic(), Intrinsics::kSystemArrayCopy);

    int32_t element_size = DataType::Size(DataType::Type::kReference);
    uint32_t offset = mirror::Array::DataOffset(element_size).Uint32Value();

    Register src = locations->InAt(0).AsRegister<Register>();
    Location src_pos = locations->InAt(1);
    Register dest = locations->InAt(2).AsRegister<Register>();
    Location dest_pos = locations->InAt(3);
    Location length = locations->InAt(4);
    Location temp1_loc = locations->GetTemp(0);
    Register temp1 = temp1_loc.AsRegister<Register>();
    Register temp2 = locations->GetTemp(1).AsRegister<Register>();
    Register temp3 = locations->GetTemp(2).AsRegister<Register>();

    __ Bind(GetEntryLabel());
    // In this code path, registers `temp1`, `temp2`, and `temp3`
    // (resp.) are not used for the base source address, the base
    // destination address, and the end source address (resp.), as in
    // other SystemArrayCopy intrinsic code paths.  Instead they are
    // (resp.) used for:
    // - the loop index (`i`);
    // - the source index (`src_index`) and the loaded (source)
    //   reference (`value`); and
    // - the destination index (`dest_index`).

    // i = 0
    __ xorl(temp1, temp1);
    NearLabel loop;
    __ Bind(&loop);
    // value = src_array[i + src_pos]
    if (src_pos.IsConstant()) {
      int32_t constant = src_pos.GetConstant()->AsIntConstant()->GetValue();
      int32_t adjusted_offset = offset + constant * element_size;
      __ movl(temp2, Address(src, temp1, ScaleFactor::TIMES_4, adjusted_offset));
    } else {
      __ leal(temp2, Address(src_pos.AsRegister<Register>(), temp1, ScaleFactor::TIMES_1, 0));
      __ movl(temp2, Address(src, temp2, ScaleFactor::TIMES_4, offset));
    }
    __ MaybeUnpoisonHeapReference(temp2);
    // TODO: Inline the mark bit check before calling the runtime?
    // value = ReadBarrier::Mark(value)
    // No need to save live registers; it's taken care of by the
    // entrypoint. Also, there is no need to update the stack mask,
    // as this runtime call will not trigger a garbage collection.
    // (See ReadBarrierMarkSlowPathX86::EmitNativeCode for more
    // explanations.)
    DCHECK_NE(temp2, ESP);
    DCHECK(0 <= temp2 && temp2 < kNumberOfCpuRegisters) << temp2;
    int32_t entry_point_offset = Thread::ReadBarrierMarkEntryPointsOffset<kX86PointerSize>(temp2);
    // This runtime call does not require a stack map.
    x86_codegen->InvokeRuntimeWithoutRecordingPcInfo(entry_point_offset, instruction_, this);
    __ MaybePoisonHeapReference(temp2);
    // dest_array[i + dest_pos] = value
    if (dest_pos.IsConstant()) {
      int32_t constant = dest_pos.GetConstant()->AsIntConstant()->GetValue();
      int32_t adjusted_offset = offset + constant * element_size;
      __ movl(Address(dest, temp1, ScaleFactor::TIMES_4, adjusted_offset), temp2);
    } else {
      __ leal(temp3, Address(dest_pos.AsRegister<Register>(), temp1, ScaleFactor::TIMES_1, 0));
      __ movl(Address(dest, temp3, ScaleFactor::TIMES_4, offset), temp2);
    }
    // ++i
    __ addl(temp1, Immediate(1));
    // if (i != length) goto loop
    x86_codegen->GenerateIntCompare(temp1_loc, length);
    __ j(kNotEqual, &loop);
    __ jmp(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "ReadBarrierSystemArrayCopySlowPathX86"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadBarrierSystemArrayCopySlowPathX86);
};

#undef __

#define __ assembler->

static void CreateFPToIntLocations(ArenaAllocator* allocator, HInvoke* invoke, bool is64bit) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
  if (is64bit) {
    locations->AddTemp(Location::RequiresFpuRegister());
  }
}

static void CreateIntToFPLocations(ArenaAllocator* allocator, HInvoke* invoke, bool is64bit) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
  if (is64bit) {
    locations->AddTemp(Location::RequiresFpuRegister());
    locations->AddTemp(Location::RequiresFpuRegister());
  }
}

static void MoveFPToInt(LocationSummary* locations, bool is64bit, X86Assembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  if (is64bit) {
    // Need to use the temporary.
    XmmRegister temp = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
    __ movsd(temp, input.AsFpuRegister<XmmRegister>());
    __ movd(output.AsRegisterPairLow<Register>(), temp);
    __ psrlq(temp, Immediate(32));
    __ movd(output.AsRegisterPairHigh<Register>(), temp);
  } else {
    __ movd(output.AsRegister<Register>(), input.AsFpuRegister<XmmRegister>());
  }
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, X86Assembler* assembler) {
  Location input = locations->InAt(0);
  Location output = locations->Out();
  if (is64bit) {
    // Need to use the temporary.
    XmmRegister temp1 = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
    XmmRegister temp2 = locations->GetTemp(1).AsFpuRegister<XmmRegister>();
    __ movd(temp1, input.AsRegisterPairLow<Register>());
    __ movd(temp2, input.AsRegisterPairHigh<Register>());
    __ punpckldq(temp1, temp2);
    __ movsd(output.AsFpuRegister<XmmRegister>(), temp1);
  } else {
    __ movd(output.AsFpuRegister<XmmRegister>(), input.AsRegister<Register>());
  }
}

void IntrinsicLocationsBuilderX86::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke, /* is64bit */ true);
}
void IntrinsicLocationsBuilderX86::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke, /* is64bit */ true);
}

void IntrinsicCodeGeneratorX86::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}
void IntrinsicCodeGeneratorX86::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke, /* is64bit */ false);
}
void IntrinsicLocationsBuilderX86::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke, /* is64bit */ false);
}

void IntrinsicCodeGeneratorX86::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}
void IntrinsicCodeGeneratorX86::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}

static void CreateIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

static void CreateLongToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void CreateLongToLongLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
}

static void GenReverseBytes(LocationSummary* locations,
                            DataType::Type size,
                            X86Assembler* assembler) {
  Register out = locations->Out().AsRegister<Register>();

  switch (size) {
    case DataType::Type::kInt16:
      // TODO: Can be done with an xchg of 8b registers. This is straight from Quick.
      __ bswapl(out);
      __ sarl(out, Immediate(16));
      break;
    case DataType::Type::kInt32:
      __ bswapl(out);
      break;
    default:
      LOG(FATAL) << "Unexpected size for reverse-bytes: " << size;
      UNREACHABLE();
  }
}

void IntrinsicLocationsBuilderX86::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitIntegerReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), DataType::Type::kInt32, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitLongReverseBytes(HInvoke* invoke) {
  CreateLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitLongReverseBytes(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  Location input = locations->InAt(0);
  Register input_lo = input.AsRegisterPairLow<Register>();
  Register input_hi = input.AsRegisterPairHigh<Register>();
  Location output = locations->Out();
  Register output_lo = output.AsRegisterPairLow<Register>();
  Register output_hi = output.AsRegisterPairHigh<Register>();

  X86Assembler* assembler = GetAssembler();
  // Assign the inputs to the outputs, mixing low/high.
  __ movl(output_lo, input_hi);
  __ movl(output_hi, input_lo);
  __ bswapl(output_lo);
  __ bswapl(output_hi);
}

void IntrinsicLocationsBuilderX86::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverseBytes(invoke->GetLocations(), DataType::Type::kInt16, GetAssembler());
}


// TODO: Consider Quick's way of doing Double abs through integer operations, as the immediate we
//       need is 64b.

static void CreateFloatToFloat(ArenaAllocator* allocator, HInvoke* invoke) {
  // TODO: Enable memory operations when the assembler supports them.
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::SameAsFirstInput());
  HInvokeStaticOrDirect* static_or_direct = invoke->AsInvokeStaticOrDirect();
  DCHECK(static_or_direct != nullptr);
  if (static_or_direct->HasSpecialInput() &&
      invoke->InputAt(static_or_direct->GetSpecialInputIndex())->IsX86ComputeBaseMethodAddress()) {
    // We need addressibility for the constant area.
    locations->SetInAt(1, Location::RequiresRegister());
    // We need a temporary to hold the constant.
    locations->AddTemp(Location::RequiresFpuRegister());
  }
}

static void MathAbsFP(HInvoke* invoke,
                      bool is64bit,
                      X86Assembler* assembler,
                      CodeGeneratorX86* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  Location output = locations->Out();

  DCHECK(output.IsFpuRegister());
  if (locations->GetInputCount() == 2 && locations->InAt(1).IsValid()) {
    HX86ComputeBaseMethodAddress* method_address =
        invoke->InputAt(1)->AsX86ComputeBaseMethodAddress();
    DCHECK(locations->InAt(1).IsRegister());
    // We also have a constant area pointer.
    Register constant_area = locations->InAt(1).AsRegister<Register>();
    XmmRegister temp = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
    if (is64bit) {
      __ movsd(temp, codegen->LiteralInt64Address(
          INT64_C(0x7FFFFFFFFFFFFFFF), method_address, constant_area));
      __ andpd(output.AsFpuRegister<XmmRegister>(), temp);
    } else {
      __ movss(temp, codegen->LiteralInt32Address(
          INT32_C(0x7FFFFFFF), method_address, constant_area));
      __ andps(output.AsFpuRegister<XmmRegister>(), temp);
    }
  } else {
    // Create the right constant on an aligned stack.
    if (is64bit) {
      __ subl(ESP, Immediate(8));
      __ pushl(Immediate(0x7FFFFFFF));
      __ pushl(Immediate(0xFFFFFFFF));
      __ andpd(output.AsFpuRegister<XmmRegister>(), Address(ESP, 0));
    } else {
      __ subl(ESP, Immediate(12));
      __ pushl(Immediate(0x7FFFFFFF));
      __ andps(output.AsFpuRegister<XmmRegister>(), Address(ESP, 0));
    }
    __ addl(ESP, Immediate(16));
  }
}

void IntrinsicLocationsBuilderX86::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFloatToFloat(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke, /* is64bit */ true, GetAssembler(), codegen_);
}

void IntrinsicLocationsBuilderX86::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFloatToFloat(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke, /* is64bit */ false, GetAssembler(), codegen_);
}

static void CreateAbsIntLocation(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RegisterLocation(EAX));
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RegisterLocation(EDX));
}

static void GenAbsInteger(LocationSummary* locations, X86Assembler* assembler) {
  Location output = locations->Out();
  Register out = output.AsRegister<Register>();
  DCHECK_EQ(out, EAX);
  Register temp = locations->GetTemp(0).AsRegister<Register>();
  DCHECK_EQ(temp, EDX);

  // Sign extend EAX into EDX.
  __ cdq();

  // XOR EAX with sign.
  __ xorl(EAX, EDX);

  // Subtract out sign to correct.
  __ subl(EAX, EDX);

  // The result is in EAX.
}

static void CreateAbsLongLocation(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
  locations->AddTemp(Location::RequiresRegister());
}

static void GenAbsLong(LocationSummary* locations, X86Assembler* assembler) {
  Location input = locations->InAt(0);
  Register input_lo = input.AsRegisterPairLow<Register>();
  Register input_hi = input.AsRegisterPairHigh<Register>();
  Location output = locations->Out();
  Register output_lo = output.AsRegisterPairLow<Register>();
  Register output_hi = output.AsRegisterPairHigh<Register>();
  Register temp = locations->GetTemp(0).AsRegister<Register>();

  // Compute the sign into the temporary.
  __ movl(temp, input_hi);
  __ sarl(temp, Immediate(31));

  // Store the sign into the output.
  __ movl(output_lo, temp);
  __ movl(output_hi, temp);

  // XOR the input to the output.
  __ xorl(output_lo, input_lo);
  __ xorl(output_hi, input_hi);

  // Subtract the sign.
  __ subl(output_lo, temp);
  __ sbbl(output_hi, temp);
}

void IntrinsicLocationsBuilderX86::VisitMathAbsInt(HInvoke* invoke) {
  CreateAbsIntLocation(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMathAbsLong(HInvoke* invoke) {
  CreateAbsLongLocation(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsLong(invoke->GetLocations(), GetAssembler());
}

static void GenMinMaxFP(HInvoke* invoke,
                        bool is_min,
                        bool is_double,
                        X86Assembler* assembler,
                        CodeGeneratorX86* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  Location op1_loc = locations->InAt(0);
  Location op2_loc = locations->InAt(1);
  Location out_loc = locations->Out();
  XmmRegister out = out_loc.AsFpuRegister<XmmRegister>();

  // Shortcut for same input locations.
  if (op1_loc.Equals(op2_loc)) {
    DCHECK(out_loc.Equals(op1_loc));
    return;
  }

  //  (out := op1)
  //  out <=? op2
  //  if Nan jmp Nan_label
  //  if out is min jmp done
  //  if op2 is min jmp op2_label
  //  handle -0/+0
  //  jmp done
  // Nan_label:
  //  out := NaN
  // op2_label:
  //  out := op2
  // done:
  //
  // This removes one jmp, but needs to copy one input (op1) to out.
  //
  // TODO: This is straight from Quick (except literal pool). Make NaN an out-of-line slowpath?

  XmmRegister op2 = op2_loc.AsFpuRegister<XmmRegister>();

  NearLabel nan, done, op2_label;
  if (is_double) {
    __ ucomisd(out, op2);
  } else {
    __ ucomiss(out, op2);
  }

  __ j(Condition::kParityEven, &nan);

  __ j(is_min ? Condition::kAbove : Condition::kBelow, &op2_label);
  __ j(is_min ? Condition::kBelow : Condition::kAbove, &done);

  // Handle 0.0/-0.0.
  if (is_min) {
    if (is_double) {
      __ orpd(out, op2);
    } else {
      __ orps(out, op2);
    }
  } else {
    if (is_double) {
      __ andpd(out, op2);
    } else {
      __ andps(out, op2);
    }
  }
  __ jmp(&done);

  // NaN handling.
  __ Bind(&nan);
  // Do we have a constant area pointer?
  if (locations->GetInputCount() == 3 && locations->InAt(2).IsValid()) {
    HX86ComputeBaseMethodAddress* method_address =
        invoke->InputAt(2)->AsX86ComputeBaseMethodAddress();
    DCHECK(locations->InAt(2).IsRegister());
    Register constant_area = locations->InAt(2).AsRegister<Register>();
    if (is_double) {
      __ movsd(out, codegen->LiteralInt64Address(kDoubleNaN, method_address, constant_area));
    } else {
      __ movss(out, codegen->LiteralInt32Address(kFloatNaN, method_address, constant_area));
    }
  } else {
    if (is_double) {
      __ pushl(Immediate(kDoubleNaNHigh));
      __ pushl(Immediate(kDoubleNaNLow));
      __ movsd(out, Address(ESP, 0));
      __ addl(ESP, Immediate(8));
    } else {
      __ pushl(Immediate(kFloatNaN));
      __ movss(out, Address(ESP, 0));
      __ addl(ESP, Immediate(4));
    }
  }
  __ jmp(&done);

  // out := op2;
  __ Bind(&op2_label);
  if (is_double) {
    __ movsd(out, op2);
  } else {
    __ movss(out, op2);
  }

  // Done.
  __ Bind(&done);
}

static void CreateFPFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  // The following is sub-optimal, but all we can do for now. It would be fine to also accept
  // the second input to be the output (we can simply swap inputs).
  locations->SetOut(Location::SameAsFirstInput());
  HInvokeStaticOrDirect* static_or_direct = invoke->AsInvokeStaticOrDirect();
  DCHECK(static_or_direct != nullptr);
  if (static_or_direct->HasSpecialInput() &&
      invoke->InputAt(static_or_direct->GetSpecialInputIndex())->IsX86ComputeBaseMethodAddress()) {
    locations->SetInAt(2, Location::RequiresRegister());
  }
}

void IntrinsicLocationsBuilderX86::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke,
              /* is_min */ true,
              /* is_double */ true,
              GetAssembler(),
              codegen_);
}

void IntrinsicLocationsBuilderX86::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke,
              /* is_min */ true,
              /* is_double */ false,
              GetAssembler(),
              codegen_);
}

void IntrinsicLocationsBuilderX86::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke,
              /* is_min */ false,
              /* is_double */ true,
              GetAssembler(),
              codegen_);
}

void IntrinsicLocationsBuilderX86::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke,
              /* is_min */ false,
              /* is_double */ false,
              GetAssembler(),
              codegen_);
}

static void GenMinMax(LocationSummary* locations, bool is_min, bool is_long,
                      X86Assembler* assembler) {
  Location op1_loc = locations->InAt(0);
  Location op2_loc = locations->InAt(1);

  // Shortcut for same input locations.
  if (op1_loc.Equals(op2_loc)) {
    // Can return immediately, as op1_loc == out_loc.
    // Note: if we ever support separate registers, e.g., output into memory, we need to check for
    //       a copy here.
    DCHECK(locations->Out().Equals(op1_loc));
    return;
  }

  if (is_long) {
    // Need to perform a subtract to get the sign right.
    // op1 is already in the same location as the output.
    Location output = locations->Out();
    Register output_lo = output.AsRegisterPairLow<Register>();
    Register output_hi = output.AsRegisterPairHigh<Register>();

    Register op2_lo = op2_loc.AsRegisterPairLow<Register>();
    Register op2_hi = op2_loc.AsRegisterPairHigh<Register>();

    // Spare register to compute the subtraction to set condition code.
    Register temp = locations->GetTemp(0).AsRegister<Register>();

    // Subtract off op2_low.
    __ movl(temp, output_lo);
    __ subl(temp, op2_lo);

    // Now use the same tempo and the borrow to finish the subtraction of op2_hi.
    __ movl(temp, output_hi);
    __ sbbl(temp, op2_hi);

    // Now the condition code is correct.
    Condition cond = is_min ? Condition::kGreaterEqual : Condition::kLess;
    __ cmovl(cond, output_lo, op2_lo);
    __ cmovl(cond, output_hi, op2_hi);
  } else {
    Register out = locations->Out().AsRegister<Register>();
    Register op2 = op2_loc.AsRegister<Register>();

    //  (out := op1)
    //  out <=? op2
    //  if out is min jmp done
    //  out := op2
    // done:

    __ cmpl(out, op2);
    Condition cond = is_min ? Condition::kGreater : Condition::kLess;
    __ cmovl(cond, out, op2);
  }
}

static void CreateIntIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
}

static void CreateLongLongToLongLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  // Register to use to perform a long subtract to set cc.
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicLocationsBuilderX86::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ true, /* is_long */ false, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMathMinLongLong(HInvoke* invoke) {
  CreateLongLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ true, /* is_long */ true, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ false, /* is_long */ false, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateLongLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(), /* is_min */ false, /* is_long */ true, GetAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

void IntrinsicLocationsBuilderX86::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  XmmRegister in = locations->InAt(0).AsFpuRegister<XmmRegister>();
  XmmRegister out = locations->Out().AsFpuRegister<XmmRegister>();

  GetAssembler()->sqrtsd(out, in);
}

static void InvokeOutOfLineIntrinsic(CodeGeneratorX86* codegen, HInvoke* invoke) {
  MoveArguments(invoke, codegen);

  DCHECK(invoke->IsInvokeStaticOrDirect());
  codegen->GenerateStaticOrDirectCall(invoke->AsInvokeStaticOrDirect(),
                                      Location::RegisterLocation(EAX));

  // Copy the result back to the expected output.
  Location out = invoke->GetLocations()->Out();
  if (out.IsValid()) {
    DCHECK(out.IsRegister());
    codegen->MoveFromReturnRegister(out, invoke->GetType());
  }
}

static void CreateSSE41FPToFPLocations(ArenaAllocator* allocator,
                                       HInvoke* invoke,
                                       CodeGeneratorX86* codegen) {
  // Do we have instruction support?
  if (codegen->GetInstructionSetFeatures().HasSSE4_1()) {
    CreateFPToFPLocations(allocator, invoke);
    return;
  }

  // We have to fall back to a call to the intrinsic.
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetOut(Location::FpuRegisterLocation(XMM0));
  // Needs to be EAX for the invoke.
  locations->AddTemp(Location::RegisterLocation(EAX));
}

static void GenSSE41FPToFPIntrinsic(CodeGeneratorX86* codegen,
                                   HInvoke* invoke,
                                   X86Assembler* assembler,
                                   int round_mode) {
  LocationSummary* locations = invoke->GetLocations();
  if (locations->WillCall()) {
    InvokeOutOfLineIntrinsic(codegen, invoke);
  } else {
    XmmRegister in = locations->InAt(0).AsFpuRegister<XmmRegister>();
    XmmRegister out = locations->Out().AsFpuRegister<XmmRegister>();
    __ roundsd(out, in, Immediate(round_mode));
  }
}

void IntrinsicLocationsBuilderX86::VisitMathCeil(HInvoke* invoke) {
  CreateSSE41FPToFPLocations(allocator_, invoke, codegen_);
}

void IntrinsicCodeGeneratorX86::VisitMathCeil(HInvoke* invoke) {
  GenSSE41FPToFPIntrinsic(codegen_, invoke, GetAssembler(), 2);
}

void IntrinsicLocationsBuilderX86::VisitMathFloor(HInvoke* invoke) {
  CreateSSE41FPToFPLocations(allocator_, invoke, codegen_);
}

void IntrinsicCodeGeneratorX86::VisitMathFloor(HInvoke* invoke) {
  GenSSE41FPToFPIntrinsic(codegen_, invoke, GetAssembler(), 1);
}

void IntrinsicLocationsBuilderX86::VisitMathRint(HInvoke* invoke) {
  CreateSSE41FPToFPLocations(allocator_, invoke, codegen_);
}

void IntrinsicCodeGeneratorX86::VisitMathRint(HInvoke* invoke) {
  GenSSE41FPToFPIntrinsic(codegen_, invoke, GetAssembler(), 0);
}

void IntrinsicLocationsBuilderX86::VisitMathRoundFloat(HInvoke* invoke) {
  // Do we have instruction support?
  if (codegen_->GetInstructionSetFeatures().HasSSE4_1()) {
    HInvokeStaticOrDirect* static_or_direct = invoke->AsInvokeStaticOrDirect();
    DCHECK(static_or_direct != nullptr);
    LocationSummary* locations =
        new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
    locations->SetInAt(0, Location::RequiresFpuRegister());
    if (static_or_direct->HasSpecialInput() &&
        invoke->InputAt(
            static_or_direct->GetSpecialInputIndex())->IsX86ComputeBaseMethodAddress()) {
      locations->SetInAt(1, Location::RequiresRegister());
    }
    locations->SetOut(Location::RequiresRegister());
    locations->AddTemp(Location::RequiresFpuRegister());
    locations->AddTemp(Location::RequiresFpuRegister());
    return;
  }

  // We have to fall back to a call to the intrinsic.
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnMainOnly);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetOut(Location::RegisterLocation(EAX));
  // Needs to be EAX for the invoke.
  locations->AddTemp(Location::RegisterLocation(EAX));
}

void IntrinsicCodeGeneratorX86::VisitMathRoundFloat(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  if (locations->WillCall()) {  // TODO: can we reach this?
    InvokeOutOfLineIntrinsic(codegen_, invoke);
    return;
  }

  XmmRegister in = locations->InAt(0).AsFpuRegister<XmmRegister>();
  XmmRegister t1 = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
  XmmRegister t2 = locations->GetTemp(1).AsFpuRegister<XmmRegister>();
  Register out = locations->Out().AsRegister<Register>();
  NearLabel skip_incr, done;
  X86Assembler* assembler = GetAssembler();

  // Since no direct x86 rounding instruction matches the required semantics,
  // this intrinsic is implemented as follows:
  //  result = floor(in);
  //  if (in - result >= 0.5f)
  //    result = result + 1.0f;
  __ movss(t2, in);
  __ roundss(t1, in, Immediate(1));
  __ subss(t2, t1);
  if (locations->GetInputCount() == 2 && locations->InAt(1).IsValid()) {
    // Direct constant area available.
    HX86ComputeBaseMethodAddress* method_address =
        invoke->InputAt(1)->AsX86ComputeBaseMethodAddress();
    Register constant_area = locations->InAt(1).AsRegister<Register>();
    __ comiss(t2, codegen_->LiteralInt32Address(bit_cast<int32_t, float>(0.5f),
                                                method_address,
                                                constant_area));
    __ j(kBelow, &skip_incr);
    __ addss(t1, codegen_->LiteralInt32Address(bit_cast<int32_t, float>(1.0f),
                                               method_address,
                                               constant_area));
    __ Bind(&skip_incr);
  } else {
    // No constant area: go through stack.
    __ pushl(Immediate(bit_cast<int32_t, float>(0.5f)));
    __ pushl(Immediate(bit_cast<int32_t, float>(1.0f)));
    __ comiss(t2, Address(ESP, 4));
    __ j(kBelow, &skip_incr);
    __ addss(t1, Address(ESP, 0));
    __ Bind(&skip_incr);
    __ addl(ESP, Immediate(8));
  }

  // Final conversion to an integer. Unfortunately this also does not have a
  // direct x86 instruction, since NaN should map to 0 and large positive
  // values need to be clipped to the extreme value.
  __ movl(out, Immediate(kPrimIntMax));
  __ cvtsi2ss(t2, out);
  __ comiss(t1, t2);
  __ j(kAboveEqual, &done);  // clipped to max (already in out), does not jump on unordered
  __ movl(out, Immediate(0));  // does not change flags
  __ j(kUnordered, &done);  // NaN mapped to 0 (just moved in out)
  __ cvttss2si(out, t1);
  __ Bind(&done);
}

static void CreateFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetOut(Location::FpuRegisterLocation(XMM0));
}

static void GenFPToFPCall(HInvoke* invoke, CodeGeneratorX86* codegen, QuickEntrypointEnum entry) {
  LocationSummary* locations = invoke->GetLocations();
  DCHECK(locations->WillCall());
  DCHECK(invoke->IsInvokeStaticOrDirect());
  X86Assembler* assembler = codegen->GetAssembler();

  // We need some place to pass the parameters.
  __ subl(ESP, Immediate(16));
  __ cfi().AdjustCFAOffset(16);

  // Pass the parameters at the bottom of the stack.
  __ movsd(Address(ESP, 0), XMM0);

  // If we have a second parameter, pass it next.
  if (invoke->GetNumberOfArguments() == 2) {
    __ movsd(Address(ESP, 8), XMM1);
  }

  // Now do the actual call.
  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());

  // Extract the return value from the FP stack.
  __ fstpl(Address(ESP, 0));
  __ movsd(XMM0, Address(ESP, 0));

  // And clean up the stack.
  __ addl(ESP, Immediate(16));
  __ cfi().AdjustCFAOffset(-16);
}

void IntrinsicLocationsBuilderX86::VisitMathCos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathCos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCos);
}

void IntrinsicLocationsBuilderX86::VisitMathSin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathSin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSin);
}

void IntrinsicLocationsBuilderX86::VisitMathAcos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAcos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAcos);
}

void IntrinsicLocationsBuilderX86::VisitMathAsin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAsin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAsin);
}

void IntrinsicLocationsBuilderX86::VisitMathAtan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAtan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAtan);
}

void IntrinsicLocationsBuilderX86::VisitMathCbrt(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathCbrt(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCbrt);
}

void IntrinsicLocationsBuilderX86::VisitMathCosh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathCosh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCosh);
}

void IntrinsicLocationsBuilderX86::VisitMathExp(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathExp(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExp);
}

void IntrinsicLocationsBuilderX86::VisitMathExpm1(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathExpm1(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExpm1);
}

void IntrinsicLocationsBuilderX86::VisitMathLog(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathLog(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog);
}

void IntrinsicLocationsBuilderX86::VisitMathLog10(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathLog10(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog10);
}

void IntrinsicLocationsBuilderX86::VisitMathSinh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathSinh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSinh);
}

void IntrinsicLocationsBuilderX86::VisitMathTan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathTan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTan);
}

void IntrinsicLocationsBuilderX86::VisitMathTanh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathTanh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTanh);
}

static void CreateFPFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetInAt(1, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(1)));
  locations->SetOut(Location::FpuRegisterLocation(XMM0));
}

void IntrinsicLocationsBuilderX86::VisitMathAtan2(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathAtan2(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAtan2);
}

void IntrinsicLocationsBuilderX86::VisitMathPow(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathPow(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickPow);
}

void IntrinsicLocationsBuilderX86::VisitMathHypot(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathHypot(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickHypot);
}

void IntrinsicLocationsBuilderX86::VisitMathNextAfter(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMathNextAfter(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickNextAfter);
}

void IntrinsicLocationsBuilderX86::VisitSystemArrayCopyChar(HInvoke* invoke) {
  // We need at least two of the positions or length to be an integer constant,
  // or else we won't have enough free registers.
  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dest_pos = invoke->InputAt(3)->AsIntConstant();
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();

  int num_constants =
      ((src_pos != nullptr) ? 1 : 0)
      + ((dest_pos != nullptr) ? 1 : 0)
      + ((length != nullptr) ? 1 : 0);

  if (num_constants < 2) {
    // Not enough free registers.
    return;
  }

  // As long as we are checking, we might as well check to see if the src and dest
  // positions are >= 0.
  if ((src_pos != nullptr && src_pos->GetValue() < 0) ||
      (dest_pos != nullptr && dest_pos->GetValue() < 0)) {
    // We will have to fail anyways.
    return;
  }

  // And since we are already checking, check the length too.
  if (length != nullptr) {
    int32_t len = length->GetValue();
    if (len < 0) {
      // Just call as normal.
      return;
    }
  }

  // Okay, it is safe to generate inline code.
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnSlowPath, kIntrinsified);
  // arraycopy(Object src, int srcPos, Object dest, int destPos, int length).
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(invoke->InputAt(1)));
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RegisterOrConstant(invoke->InputAt(3)));
  locations->SetInAt(4, Location::RegisterOrConstant(invoke->InputAt(4)));

  // And we need some temporaries.  We will use REP MOVSW, so we need fixed registers.
  locations->AddTemp(Location::RegisterLocation(ESI));
  locations->AddTemp(Location::RegisterLocation(EDI));
  locations->AddTemp(Location::RegisterLocation(ECX));
}

static void CheckPosition(X86Assembler* assembler,
                          Location pos,
                          Register input,
                          Location length,
                          SlowPathCode* slow_path,
                          Register temp,
                          bool length_is_input_length = false) {
  // Where is the length in the Array?
  const uint32_t length_offset = mirror::Array::LengthOffset().Uint32Value();

  if (pos.IsConstant()) {
    int32_t pos_const = pos.GetConstant()->AsIntConstant()->GetValue();
    if (pos_const == 0) {
      if (!length_is_input_length) {
        // Check that length(input) >= length.
        if (length.IsConstant()) {
          __ cmpl(Address(input, length_offset),
                  Immediate(length.GetConstant()->AsIntConstant()->GetValue()));
        } else {
          __ cmpl(Address(input, length_offset), length.AsRegister<Register>());
        }
        __ j(kLess, slow_path->GetEntryLabel());
      }
    } else {
      // Check that length(input) >= pos.
      __ movl(temp, Address(input, length_offset));
      __ subl(temp, Immediate(pos_const));
      __ j(kLess, slow_path->GetEntryLabel());

      // Check that (length(input) - pos) >= length.
      if (length.IsConstant()) {
        __ cmpl(temp, Immediate(length.GetConstant()->AsIntConstant()->GetValue()));
      } else {
        __ cmpl(temp, length.AsRegister<Register>());
      }
      __ j(kLess, slow_path->GetEntryLabel());
    }
  } else if (length_is_input_length) {
    // The only way the copy can succeed is if pos is zero.
    Register pos_reg = pos.AsRegister<Register>();
    __ testl(pos_reg, pos_reg);
    __ j(kNotEqual, slow_path->GetEntryLabel());
  } else {
    // Check that pos >= 0.
    Register pos_reg = pos.AsRegister<Register>();
    __ testl(pos_reg, pos_reg);
    __ j(kLess, slow_path->GetEntryLabel());

    // Check that pos <= length(input).
    __ cmpl(Address(input, length_offset), pos_reg);
    __ j(kLess, slow_path->GetEntryLabel());

    // Check that (length(input) - pos) >= length.
    __ movl(temp, Address(input, length_offset));
    __ subl(temp, pos_reg);
    if (length.IsConstant()) {
      __ cmpl(temp, Immediate(length.GetConstant()->AsIntConstant()->GetValue()));
    } else {
      __ cmpl(temp, length.AsRegister<Register>());
    }
    __ j(kLess, slow_path->GetEntryLabel());
  }
}

void IntrinsicCodeGeneratorX86::VisitSystemArrayCopyChar(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register src = locations->InAt(0).AsRegister<Register>();
  Location srcPos = locations->InAt(1);
  Register dest = locations->InAt(2).AsRegister<Register>();
  Location destPos = locations->InAt(3);
  Location length = locations->InAt(4);

  // Temporaries that we need for MOVSW.
  Register src_base = locations->GetTemp(0).AsRegister<Register>();
  DCHECK_EQ(src_base, ESI);
  Register dest_base = locations->GetTemp(1).AsRegister<Register>();
  DCHECK_EQ(dest_base, EDI);
  Register count = locations->GetTemp(2).AsRegister<Register>();
  DCHECK_EQ(count, ECX);

  SlowPathCode* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
  codegen_->AddSlowPath(slow_path);

  // Bail out if the source and destination are the same (to handle overlap).
  __ cmpl(src, dest);
  __ j(kEqual, slow_path->GetEntryLabel());

  // Bail out if the source is null.
  __ testl(src, src);
  __ j(kEqual, slow_path->GetEntryLabel());

  // Bail out if the destination is null.
  __ testl(dest, dest);
  __ j(kEqual, slow_path->GetEntryLabel());

  // If the length is negative, bail out.
  // We have already checked in the LocationsBuilder for the constant case.
  if (!length.IsConstant()) {
    __ cmpl(length.AsRegister<Register>(), length.AsRegister<Register>());
    __ j(kLess, slow_path->GetEntryLabel());
  }

  // We need the count in ECX.
  if (length.IsConstant()) {
    __ movl(count, Immediate(length.GetConstant()->AsIntConstant()->GetValue()));
  } else {
    __ movl(count, length.AsRegister<Register>());
  }

  // Validity checks: source. Use src_base as a temporary register.
  CheckPosition(assembler, srcPos, src, Location::RegisterLocation(count), slow_path, src_base);

  // Validity checks: dest. Use src_base as a temporary register.
  CheckPosition(assembler, destPos, dest, Location::RegisterLocation(count), slow_path, src_base);

  // Okay, everything checks out.  Finally time to do the copy.
  // Check assumption that sizeof(Char) is 2 (used in scaling below).
  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  const uint32_t data_offset = mirror::Array::DataOffset(char_size).Uint32Value();

  if (srcPos.IsConstant()) {
    int32_t srcPos_const = srcPos.GetConstant()->AsIntConstant()->GetValue();
    __ leal(src_base, Address(src, char_size * srcPos_const + data_offset));
  } else {
    __ leal(src_base, Address(src, srcPos.AsRegister<Register>(),
                              ScaleFactor::TIMES_2, data_offset));
  }
  if (destPos.IsConstant()) {
    int32_t destPos_const = destPos.GetConstant()->AsIntConstant()->GetValue();

    __ leal(dest_base, Address(dest, char_size * destPos_const + data_offset));
  } else {
    __ leal(dest_base, Address(dest, destPos.AsRegister<Register>(),
                               ScaleFactor::TIMES_2, data_offset));
  }

  // Do the move.
  __ rep_movsw();

  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderX86::VisitStringCompareTo(HInvoke* invoke) {
  // The inputs plus one temp.
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetOut(Location::RegisterLocation(EAX));
}

void IntrinsicCodeGeneratorX86::VisitStringCompareTo(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  Register argument = locations->InAt(1).AsRegister<Register>();
  __ testl(argument, argument);
  SlowPathCode* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
  codegen_->AddSlowPath(slow_path);
  __ j(kEqual, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickStringCompareTo, invoke, invoke->GetDexPc(), slow_path);
  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderX86::VisitStringEquals(HInvoke* invoke) {
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

  // Request temporary registers, ECX and EDI needed for repe_cmpsl instruction.
  locations->AddTemp(Location::RegisterLocation(ECX));
  locations->AddTemp(Location::RegisterLocation(EDI));

  // Set output, ESI needed for repe_cmpsl instruction anyways.
  locations->SetOut(Location::RegisterLocation(ESI), Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorX86::VisitStringEquals(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register str = locations->InAt(0).AsRegister<Register>();
  Register arg = locations->InAt(1).AsRegister<Register>();
  Register ecx = locations->GetTemp(0).AsRegister<Register>();
  Register edi = locations->GetTemp(1).AsRegister<Register>();
  Register esi = locations->Out().AsRegister<Register>();

  NearLabel end, return_true, return_false;

  // Get offsets of count, value, and class fields within a string object.
  const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
  const uint32_t value_offset = mirror::String::ValueOffset().Uint32Value();
  const uint32_t class_offset = mirror::Object::ClassOffset().Uint32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  StringEqualsOptimizations optimizations(invoke);
  if (!optimizations.GetArgumentNotNull()) {
    // Check if input is null, return false if it is.
    __ testl(arg, arg);
    __ j(kEqual, &return_false);
  }

  if (!optimizations.GetArgumentIsString()) {
    // Instanceof check for the argument by comparing class fields.
    // All string objects must have the same type since String cannot be subclassed.
    // Receiver must be a string object, so its class field is equal to all strings' class fields.
    // If the argument is a string object, its class field must be equal to receiver's class field.
    __ movl(ecx, Address(str, class_offset));
    __ cmpl(ecx, Address(arg, class_offset));
    __ j(kNotEqual, &return_false);
  }

  // Reference equality check, return true if same reference.
  __ cmpl(str, arg);
  __ j(kEqual, &return_true);

  // Load length and compression flag of receiver string.
  __ movl(ecx, Address(str, count_offset));
  // Check if lengths and compression flags are equal, return false if they're not.
  // Two identical strings will always have same compression style since
  // compression style is decided on alloc.
  __ cmpl(ecx, Address(arg, count_offset));
  __ j(kNotEqual, &return_false);
  // Return true if strings are empty. Even with string compression `count == 0` means empty.
  static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                "Expecting 0=compressed, 1=uncompressed");
  __ jecxz(&return_true);

  if (mirror::kUseStringCompression) {
    NearLabel string_uncompressed;
    // Extract length and differentiate between both compressed or both uncompressed.
    // Different compression style is cut above.
    __ shrl(ecx, Immediate(1));
    __ j(kCarrySet, &string_uncompressed);
    // Divide string length by 2, rounding up, and continue as if uncompressed.
    __ addl(ecx, Immediate(1));
    __ shrl(ecx, Immediate(1));
    __ Bind(&string_uncompressed);
  }
  // Load starting addresses of string values into ESI/EDI as required for repe_cmpsl instruction.
  __ leal(esi, Address(str, value_offset));
  __ leal(edi, Address(arg, value_offset));

  // Divide string length by 2 to compare characters 2 at a time and adjust for lengths not
  // divisible by 2.
  __ addl(ecx, Immediate(1));
  __ shrl(ecx, Immediate(1));

  // Assertions that must hold in order to compare strings 2 characters (uncompressed)
  // or 4 characters (compressed) at a time.
  DCHECK_ALIGNED(value_offset, 4);
  static_assert(IsAligned<4>(kObjectAlignment), "String of odd length is not zero padded");

  // Loop to compare strings two characters at a time starting at the beginning of the string.
  __ repe_cmpsl();
  // If strings are not equal, zero flag will be cleared.
  __ j(kNotEqual, &return_false);

  // Return true and exit the function.
  // If loop does not result in returning false, we return true.
  __ Bind(&return_true);
  __ movl(esi, Immediate(1));
  __ jmp(&end);

  // Return false and exit the function.
  __ Bind(&return_false);
  __ xorl(esi, esi);
  __ Bind(&end);
}

static void CreateStringIndexOfLocations(HInvoke* invoke,
                                         ArenaAllocator* allocator,
                                         bool start_at_zero) {
  LocationSummary* locations = new (allocator) LocationSummary(invoke,
                                                               LocationSummary::kCallOnSlowPath,
                                                               kIntrinsified);
  // The data needs to be in EDI for scasw. So request that the string is there, anyways.
  locations->SetInAt(0, Location::RegisterLocation(EDI));
  // If we look for a constant char, we'll still have to copy it into EAX. So just request the
  // allocator to do that, anyways. We can still do the constant check by checking the parameter
  // of the instruction explicitly.
  // Note: This works as we don't clobber EAX anywhere.
  locations->SetInAt(1, Location::RegisterLocation(EAX));
  if (!start_at_zero) {
    locations->SetInAt(2, Location::RequiresRegister());          // The starting index.
  }
  // As we clobber EDI during execution anyways, also use it as the output.
  locations->SetOut(Location::SameAsFirstInput());

  // repne scasw uses ECX as the counter.
  locations->AddTemp(Location::RegisterLocation(ECX));
  // Need another temporary to be able to compute the result.
  locations->AddTemp(Location::RequiresRegister());
  if (mirror::kUseStringCompression) {
    // Need another temporary to be able to save unflagged string length.
    locations->AddTemp(Location::RequiresRegister());
  }
}

static void GenerateStringIndexOf(HInvoke* invoke,
                                  X86Assembler* assembler,
                                  CodeGeneratorX86* codegen,
                                  bool start_at_zero) {
  LocationSummary* locations = invoke->GetLocations();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  Register string_obj = locations->InAt(0).AsRegister<Register>();
  Register search_value = locations->InAt(1).AsRegister<Register>();
  Register counter = locations->GetTemp(0).AsRegister<Register>();
  Register string_length = locations->GetTemp(1).AsRegister<Register>();
  Register out = locations->Out().AsRegister<Register>();
  // Only used when string compression feature is on.
  Register string_length_flagged;

  // Check our assumptions for registers.
  DCHECK_EQ(string_obj, EDI);
  DCHECK_EQ(search_value, EAX);
  DCHECK_EQ(counter, ECX);
  DCHECK_EQ(out, EDI);

  // Check for code points > 0xFFFF. Either a slow-path check when we don't know statically,
  // or directly dispatch for a large constant, or omit slow-path for a small constant or a char.
  SlowPathCode* slow_path = nullptr;
  HInstruction* code_point = invoke->InputAt(1);
  if (code_point->IsIntConstant()) {
    if (static_cast<uint32_t>(code_point->AsIntConstant()->GetValue()) >
    std::numeric_limits<uint16_t>::max()) {
      // Always needs the slow-path. We could directly dispatch to it, but this case should be
      // rare, so for simplicity just put the full slow-path down and branch unconditionally.
      slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
      codegen->AddSlowPath(slow_path);
      __ jmp(slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
  } else if (code_point->GetType() != DataType::Type::kUint16) {
    __ cmpl(search_value, Immediate(std::numeric_limits<uint16_t>::max()));
    slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
    codegen->AddSlowPath(slow_path);
    __ j(kAbove, slow_path->GetEntryLabel());
  }

  // From here down, we know that we are looking for a char that fits in 16 bits.
  // Location of reference to data array within the String object.
  int32_t value_offset = mirror::String::ValueOffset().Int32Value();
  // Location of count within the String object.
  int32_t count_offset = mirror::String::CountOffset().Int32Value();

  // Load the count field of the string containing the length and compression flag.
  __ movl(string_length, Address(string_obj, count_offset));

  // Do a zero-length check. Even with string compression `count == 0` means empty.
  static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                "Expecting 0=compressed, 1=uncompressed");
  // TODO: Support jecxz.
  NearLabel not_found_label;
  __ testl(string_length, string_length);
  __ j(kEqual, &not_found_label);

  if (mirror::kUseStringCompression) {
    string_length_flagged = locations->GetTemp(2).AsRegister<Register>();
    __ movl(string_length_flagged, string_length);
    // Extract the length and shift out the least significant bit used as compression flag.
    __ shrl(string_length, Immediate(1));
  }

  if (start_at_zero) {
    // Number of chars to scan is the same as the string length.
    __ movl(counter, string_length);

    // Move to the start of the string.
    __ addl(string_obj, Immediate(value_offset));
  } else {
    Register start_index = locations->InAt(2).AsRegister<Register>();

    // Do a start_index check.
    __ cmpl(start_index, string_length);
    __ j(kGreaterEqual, &not_found_label);

    // Ensure we have a start index >= 0;
    __ xorl(counter, counter);
    __ cmpl(start_index, Immediate(0));
    __ cmovl(kGreater, counter, start_index);

    if (mirror::kUseStringCompression) {
      NearLabel modify_counter, offset_uncompressed_label;
      __ testl(string_length_flagged, Immediate(1));
      __ j(kNotZero, &offset_uncompressed_label);
      // Move to the start of the string: string_obj + value_offset + start_index.
      __ leal(string_obj, Address(string_obj, counter, ScaleFactor::TIMES_1, value_offset));
      __ jmp(&modify_counter);

      // Move to the start of the string: string_obj + value_offset + 2 * start_index.
      __ Bind(&offset_uncompressed_label);
      __ leal(string_obj, Address(string_obj, counter, ScaleFactor::TIMES_2, value_offset));

      // Now update ecx (the repne scasw work counter). We have string.length - start_index left to
      // compare.
      __ Bind(&modify_counter);
    } else {
      __ leal(string_obj, Address(string_obj, counter, ScaleFactor::TIMES_2, value_offset));
    }
    __ negl(counter);
    __ leal(counter, Address(string_length, counter, ScaleFactor::TIMES_1, 0));
  }

  if (mirror::kUseStringCompression) {
    NearLabel uncompressed_string_comparison;
    NearLabel comparison_done;
    __ testl(string_length_flagged, Immediate(1));
    __ j(kNotZero, &uncompressed_string_comparison);

    // Check if EAX (search_value) is ASCII.
    __ cmpl(search_value, Immediate(127));
    __ j(kGreater, &not_found_label);
    // Comparing byte-per-byte.
    __ repne_scasb();
    __ jmp(&comparison_done);

    // Everything is set up for repne scasw:
    //   * Comparison address in EDI.
    //   * Counter in ECX.
    __ Bind(&uncompressed_string_comparison);
    __ repne_scasw();
    __ Bind(&comparison_done);
  } else {
    __ repne_scasw();
  }
  // Did we find a match?
  __ j(kNotEqual, &not_found_label);

  // Yes, we matched.  Compute the index of the result.
  __ subl(string_length, counter);
  __ leal(out, Address(string_length, -1));

  NearLabel done;
  __ jmp(&done);

  // Failed to match; return -1.
  __ Bind(&not_found_label);
  __ movl(out, Immediate(-1));

  // And join up at the end.
  __ Bind(&done);
  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

void IntrinsicLocationsBuilderX86::VisitStringIndexOf(HInvoke* invoke) {
  CreateStringIndexOfLocations(invoke, allocator_, /* start_at_zero */ true);
}

void IntrinsicCodeGeneratorX86::VisitStringIndexOf(HInvoke* invoke) {
  GenerateStringIndexOf(invoke, GetAssembler(), codegen_, /* start_at_zero */ true);
}

void IntrinsicLocationsBuilderX86::VisitStringIndexOfAfter(HInvoke* invoke) {
  CreateStringIndexOfLocations(invoke, allocator_, /* start_at_zero */ false);
}

void IntrinsicCodeGeneratorX86::VisitStringIndexOfAfter(HInvoke* invoke) {
  GenerateStringIndexOf(invoke, GetAssembler(), codegen_, /* start_at_zero */ false);
}

void IntrinsicLocationsBuilderX86::VisitStringNewStringFromBytes(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  locations->SetInAt(3, Location::RegisterLocation(calling_convention.GetRegisterAt(3)));
  locations->SetOut(Location::RegisterLocation(EAX));
}

void IntrinsicCodeGeneratorX86::VisitStringNewStringFromBytes(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register byte_array = locations->InAt(0).AsRegister<Register>();
  __ testl(byte_array, byte_array);
  SlowPathCode* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
  codegen_->AddSlowPath(slow_path);
  __ j(kEqual, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromBytes, invoke, invoke->GetDexPc());
  CheckEntrypointTypes<kQuickAllocStringFromBytes, void*, void*, int32_t, int32_t, int32_t>();
  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderX86::VisitStringNewStringFromChars(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  locations->SetOut(Location::RegisterLocation(EAX));
}

void IntrinsicCodeGeneratorX86::VisitStringNewStringFromChars(HInvoke* invoke) {
  // No need to emit code checking whether `locations->InAt(2)` is a null
  // pointer, as callers of the native method
  //
  //   java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
  //
  // all include a null check on `data` before calling that method.
  codegen_->InvokeRuntime(kQuickAllocStringFromChars, invoke, invoke->GetDexPc());
  CheckEntrypointTypes<kQuickAllocStringFromChars, void*, int32_t, int32_t, void*>();
}

void IntrinsicLocationsBuilderX86::VisitStringNewStringFromString(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetOut(Location::RegisterLocation(EAX));
}

void IntrinsicCodeGeneratorX86::VisitStringNewStringFromString(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register string_to_copy = locations->InAt(0).AsRegister<Register>();
  __ testl(string_to_copy, string_to_copy);
  SlowPathCode* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
  codegen_->AddSlowPath(slow_path);
  __ j(kEqual, slow_path->GetEntryLabel());

  codegen_->InvokeRuntime(kQuickAllocStringFromString, invoke, invoke->GetDexPc());
  CheckEntrypointTypes<kQuickAllocStringFromString, void*, void*>();
  __ Bind(slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderX86::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  // public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin);
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(invoke->InputAt(1)));
  // Place srcEnd in ECX to save a move below.
  locations->SetInAt(2, Location::RegisterLocation(ECX));
  locations->SetInAt(3, Location::RequiresRegister());
  locations->SetInAt(4, Location::RequiresRegister());

  // And we need some temporaries.  We will use REP MOVSW, so we need fixed registers.
  // We don't have enough registers to also grab ECX, so handle below.
  locations->AddTemp(Location::RegisterLocation(ESI));
  locations->AddTemp(Location::RegisterLocation(EDI));
}

void IntrinsicCodeGeneratorX86::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  size_t char_component_size = DataType::Size(DataType::Type::kUint16);
  // Location of data in char array buffer.
  const uint32_t data_offset = mirror::Array::DataOffset(char_component_size).Uint32Value();
  // Location of char array data in string.
  const uint32_t value_offset = mirror::String::ValueOffset().Uint32Value();

  // public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin);
  Register obj = locations->InAt(0).AsRegister<Register>();
  Location srcBegin = locations->InAt(1);
  int srcBegin_value =
    srcBegin.IsConstant() ? srcBegin.GetConstant()->AsIntConstant()->GetValue() : 0;
  Register srcEnd = locations->InAt(2).AsRegister<Register>();
  Register dst = locations->InAt(3).AsRegister<Register>();
  Register dstBegin = locations->InAt(4).AsRegister<Register>();

  // Check assumption that sizeof(Char) is 2 (used in scaling below).
  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  // Compute the number of chars (words) to move.
  // Save ECX, since we don't know if it will be used later.
  __ pushl(ECX);
  int stack_adjust = kX86WordSize;
  __ cfi().AdjustCFAOffset(stack_adjust);
  DCHECK_EQ(srcEnd, ECX);
  if (srcBegin.IsConstant()) {
    __ subl(ECX, Immediate(srcBegin_value));
  } else {
    DCHECK(srcBegin.IsRegister());
    __ subl(ECX, srcBegin.AsRegister<Register>());
  }

  NearLabel done;
  if (mirror::kUseStringCompression) {
    // Location of count in string
    const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
    const size_t c_char_size = DataType::Size(DataType::Type::kInt8);
    DCHECK_EQ(c_char_size, 1u);
    __ pushl(EAX);
    __ cfi().AdjustCFAOffset(stack_adjust);

    NearLabel copy_loop, copy_uncompressed;
    __ testl(Address(obj, count_offset), Immediate(1));
    static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                  "Expecting 0=compressed, 1=uncompressed");
    __ j(kNotZero, &copy_uncompressed);
    // Compute the address of the source string by adding the number of chars from
    // the source beginning to the value offset of a string.
    __ leal(ESI, CodeGeneratorX86::ArrayAddress(obj, srcBegin, TIMES_1, value_offset));

    // Start the loop to copy String's value to Array of Char.
    __ leal(EDI, Address(dst, dstBegin, ScaleFactor::TIMES_2, data_offset));
    __ Bind(&copy_loop);
    __ jecxz(&done);
    // Use EAX temporary (convert byte from ESI to word).
    // TODO: Use LODSB/STOSW (not supported by X86Assembler) with AH initialized to 0.
    __ movzxb(EAX, Address(ESI, 0));
    __ movw(Address(EDI, 0), EAX);
    __ leal(EDI, Address(EDI, char_size));
    __ leal(ESI, Address(ESI, c_char_size));
    // TODO: Add support for LOOP to X86Assembler.
    __ subl(ECX, Immediate(1));
    __ jmp(&copy_loop);
    __ Bind(&copy_uncompressed);
  }

  // Do the copy for uncompressed string.
  // Compute the address of the destination buffer.
  __ leal(EDI, Address(dst, dstBegin, ScaleFactor::TIMES_2, data_offset));
  __ leal(ESI, CodeGeneratorX86::ArrayAddress(obj, srcBegin, TIMES_2, value_offset));
  __ rep_movsw();

  __ Bind(&done);
  if (mirror::kUseStringCompression) {
    // Restore EAX.
    __ popl(EAX);
    __ cfi().AdjustCFAOffset(-stack_adjust);
  }
  // Restore ECX.
  __ popl(ECX);
  __ cfi().AdjustCFAOffset(-stack_adjust);
}

static void GenPeek(LocationSummary* locations, DataType::Type size, X86Assembler* assembler) {
  Register address = locations->InAt(0).AsRegisterPairLow<Register>();
  Location out_loc = locations->Out();
  // x86 allows unaligned access. We do not have to check the input or use specific instructions
  // to avoid a SIGBUS.
  switch (size) {
    case DataType::Type::kInt8:
      __ movsxb(out_loc.AsRegister<Register>(), Address(address, 0));
      break;
    case DataType::Type::kInt16:
      __ movsxw(out_loc.AsRegister<Register>(), Address(address, 0));
      break;
    case DataType::Type::kInt32:
      __ movl(out_loc.AsRegister<Register>(), Address(address, 0));
      break;
    case DataType::Type::kInt64:
      __ movl(out_loc.AsRegisterPairLow<Register>(), Address(address, 0));
      __ movl(out_loc.AsRegisterPairHigh<Register>(), Address(address, 4));
      break;
    default:
      LOG(FATAL) << "Type not recognized for peek: " << size;
      UNREACHABLE();
  }
}

void IntrinsicLocationsBuilderX86::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateLongToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPeekByte(HInvoke* invoke) {
  GenPeek(invoke->GetLocations(), DataType::Type::kInt8, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateLongToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPeekIntNative(HInvoke* invoke) {
  GenPeek(invoke->GetLocations(), DataType::Type::kInt32, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateLongToLongLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPeekLongNative(HInvoke* invoke) {
  GenPeek(invoke->GetLocations(), DataType::Type::kInt64, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateLongToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPeekShortNative(HInvoke* invoke) {
  GenPeek(invoke->GetLocations(), DataType::Type::kInt16, GetAssembler());
}

static void CreateLongIntToVoidLocations(ArenaAllocator* allocator,
                                         DataType::Type size,
                                         HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  HInstruction* value = invoke->InputAt(1);
  if (size == DataType::Type::kInt8) {
    locations->SetInAt(1, Location::ByteRegisterOrConstant(EDX, value));
  } else {
    locations->SetInAt(1, Location::RegisterOrConstant(value));
  }
}

static void GenPoke(LocationSummary* locations, DataType::Type size, X86Assembler* assembler) {
  Register address = locations->InAt(0).AsRegisterPairLow<Register>();
  Location value_loc = locations->InAt(1);
  // x86 allows unaligned access. We do not have to check the input or use specific instructions
  // to avoid a SIGBUS.
  switch (size) {
    case DataType::Type::kInt8:
      if (value_loc.IsConstant()) {
        __ movb(Address(address, 0),
                Immediate(value_loc.GetConstant()->AsIntConstant()->GetValue()));
      } else {
        __ movb(Address(address, 0), value_loc.AsRegister<ByteRegister>());
      }
      break;
    case DataType::Type::kInt16:
      if (value_loc.IsConstant()) {
        __ movw(Address(address, 0),
                Immediate(value_loc.GetConstant()->AsIntConstant()->GetValue()));
      } else {
        __ movw(Address(address, 0), value_loc.AsRegister<Register>());
      }
      break;
    case DataType::Type::kInt32:
      if (value_loc.IsConstant()) {
        __ movl(Address(address, 0),
                Immediate(value_loc.GetConstant()->AsIntConstant()->GetValue()));
      } else {
        __ movl(Address(address, 0), value_loc.AsRegister<Register>());
      }
      break;
    case DataType::Type::kInt64:
      if (value_loc.IsConstant()) {
        int64_t value = value_loc.GetConstant()->AsLongConstant()->GetValue();
        __ movl(Address(address, 0), Immediate(Low32Bits(value)));
        __ movl(Address(address, 4), Immediate(High32Bits(value)));
      } else {
        __ movl(Address(address, 0), value_loc.AsRegisterPairLow<Register>());
        __ movl(Address(address, 4), value_loc.AsRegisterPairHigh<Register>());
      }
      break;
    default:
      LOG(FATAL) << "Type not recognized for poke: " << size;
      UNREACHABLE();
  }
}

void IntrinsicLocationsBuilderX86::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateLongIntToVoidLocations(allocator_, DataType::Type::kInt8, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPokeByte(HInvoke* invoke) {
  GenPoke(invoke->GetLocations(), DataType::Type::kInt8, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateLongIntToVoidLocations(allocator_, DataType::Type::kInt32, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPokeIntNative(HInvoke* invoke) {
  GenPoke(invoke->GetLocations(), DataType::Type::kInt32, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateLongIntToVoidLocations(allocator_, DataType::Type::kInt64, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPokeLongNative(HInvoke* invoke) {
  GenPoke(invoke->GetLocations(), DataType::Type::kInt64, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateLongIntToVoidLocations(allocator_, DataType::Type::kInt16, invoke);
}

void IntrinsicCodeGeneratorX86::VisitMemoryPokeShortNative(HInvoke* invoke) {
  GenPoke(invoke->GetLocations(), DataType::Type::kInt16, GetAssembler());
}

void IntrinsicLocationsBuilderX86::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorX86::VisitThreadCurrentThread(HInvoke* invoke) {
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();
  GetAssembler()->fs()->movl(out, Address::Absolute(Thread::PeerOffset<kX86PointerSize>()));
}

static void GenUnsafeGet(HInvoke* invoke,
                         DataType::Type type,
                         bool is_volatile,
                         CodeGeneratorX86* codegen) {
  X86Assembler* assembler = down_cast<X86Assembler*>(codegen->GetAssembler());
  LocationSummary* locations = invoke->GetLocations();
  Location base_loc = locations->InAt(1);
  Register base = base_loc.AsRegister<Register>();
  Location offset_loc = locations->InAt(2);
  Register offset = offset_loc.AsRegisterPairLow<Register>();
  Location output_loc = locations->Out();

  switch (type) {
    case DataType::Type::kInt32: {
      Register output = output_loc.AsRegister<Register>();
      __ movl(output, Address(base, offset, ScaleFactor::TIMES_1, 0));
      break;
    }

    case DataType::Type::kReference: {
      Register output = output_loc.AsRegister<Register>();
      if (kEmitCompilerReadBarrier) {
        if (kUseBakerReadBarrier) {
          Address src(base, offset, ScaleFactor::TIMES_1, 0);
          codegen->GenerateReferenceLoadWithBakerReadBarrier(
              invoke, output_loc, base, src, /* needs_null_check */ false);
        } else {
          __ movl(output, Address(base, offset, ScaleFactor::TIMES_1, 0));
          codegen->GenerateReadBarrierSlow(
              invoke, output_loc, output_loc, base_loc, 0U, offset_loc);
        }
      } else {
        __ movl(output, Address(base, offset, ScaleFactor::TIMES_1, 0));
        __ MaybeUnpoisonHeapReference(output);
      }
      break;
    }

    case DataType::Type::kInt64: {
        Register output_lo = output_loc.AsRegisterPairLow<Register>();
        Register output_hi = output_loc.AsRegisterPairHigh<Register>();
        if (is_volatile) {
          // Need to use a XMM to read atomically.
          XmmRegister temp = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
          __ movsd(temp, Address(base, offset, ScaleFactor::TIMES_1, 0));
          __ movd(output_lo, temp);
          __ psrlq(temp, Immediate(32));
          __ movd(output_hi, temp);
        } else {
          __ movl(output_lo, Address(base, offset, ScaleFactor::TIMES_1, 0));
          __ movl(output_hi, Address(base, offset, ScaleFactor::TIMES_1, 4));
        }
      }
      break;

    default:
      LOG(FATAL) << "Unsupported op size " << type;
      UNREACHABLE();
  }
}

static void CreateIntIntIntToIntLocations(ArenaAllocator* allocator,
                                          HInvoke* invoke,
                                          DataType::Type type,
                                          bool is_volatile) {
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
  if (type == DataType::Type::kInt64) {
    if (is_volatile) {
      // Need to use XMM to read volatile.
      locations->AddTemp(Location::RequiresFpuRegister());
      locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
    } else {
      locations->SetOut(Location::RequiresRegister(), Location::kOutputOverlap);
    }
  } else {
    locations->SetOut(Location::RequiresRegister(),
                      (can_call ? Location::kOutputOverlap : Location::kNoOutputOverlap));
  }
}

void IntrinsicLocationsBuilderX86::VisitUnsafeGet(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(
      allocator_, invoke, DataType::Type::kInt32, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafeGetVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt32, /* is_volatile */ true);
}
void IntrinsicLocationsBuilderX86::VisitUnsafeGetLong(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(
      allocator_, invoke, DataType::Type::kInt64, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt64, /* is_volatile */ true);
}
void IntrinsicLocationsBuilderX86::VisitUnsafeGetObject(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(
      allocator_, invoke, DataType::Type::kReference, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(
      allocator_, invoke, DataType::Type::kReference, /* is_volatile */ true);
}


void IntrinsicCodeGeneratorX86::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafeGetLongVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafeGetObject(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ true, codegen_);
}


static void CreateIntIntIntIntToVoidPlusTempsLocations(ArenaAllocator* allocator,
                                                       DataType::Type type,
                                                       HInvoke* invoke,
                                                       bool is_volatile) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
  if (type == DataType::Type::kReference) {
    // Need temp registers for card-marking.
    locations->AddTemp(Location::RequiresRegister());  // Possibly used for reference poisoning too.
    // Ensure the value is in a byte register.
    locations->AddTemp(Location::RegisterLocation(ECX));
  } else if (type == DataType::Type::kInt64 && is_volatile) {
    locations->AddTemp(Location::RequiresFpuRegister());
    locations->AddTemp(Location::RequiresFpuRegister());
  }
}

void IntrinsicLocationsBuilderX86::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt32, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt32, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt32, invoke, /* is_volatile */ true);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kReference, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kReference, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kReference, invoke, /* is_volatile */ true);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt64, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt64, invoke, /* is_volatile */ false);
}
void IntrinsicLocationsBuilderX86::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidPlusTempsLocations(
      allocator_, DataType::Type::kInt64, invoke, /* is_volatile */ true);
}

// We don't care for ordered: it requires an AnyStore barrier, which is already given by the x86
// memory model.
static void GenUnsafePut(LocationSummary* locations,
                         DataType::Type type,
                         bool is_volatile,
                         CodeGeneratorX86* codegen) {
  X86Assembler* assembler = down_cast<X86Assembler*>(codegen->GetAssembler());
  Register base = locations->InAt(1).AsRegister<Register>();
  Register offset = locations->InAt(2).AsRegisterPairLow<Register>();
  Location value_loc = locations->InAt(3);

  if (type == DataType::Type::kInt64) {
    Register value_lo = value_loc.AsRegisterPairLow<Register>();
    Register value_hi = value_loc.AsRegisterPairHigh<Register>();
    if (is_volatile) {
      XmmRegister temp1 = locations->GetTemp(0).AsFpuRegister<XmmRegister>();
      XmmRegister temp2 = locations->GetTemp(1).AsFpuRegister<XmmRegister>();
      __ movd(temp1, value_lo);
      __ movd(temp2, value_hi);
      __ punpckldq(temp1, temp2);
      __ movsd(Address(base, offset, ScaleFactor::TIMES_1, 0), temp1);
    } else {
      __ movl(Address(base, offset, ScaleFactor::TIMES_1, 0), value_lo);
      __ movl(Address(base, offset, ScaleFactor::TIMES_1, 4), value_hi);
    }
  } else if (kPoisonHeapReferences && type == DataType::Type::kReference) {
    Register temp = locations->GetTemp(0).AsRegister<Register>();
    __ movl(temp, value_loc.AsRegister<Register>());
    __ PoisonHeapReference(temp);
    __ movl(Address(base, offset, ScaleFactor::TIMES_1, 0), temp);
  } else {
    __ movl(Address(base, offset, ScaleFactor::TIMES_1, 0), value_loc.AsRegister<Register>());
  }

  if (is_volatile) {
    codegen->MemoryFence();
  }

  if (type == DataType::Type::kReference) {
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(locations->GetTemp(0).AsRegister<Register>(),
                        locations->GetTemp(1).AsRegister<Register>(),
                        base,
                        value_loc.AsRegister<Register>(),
                        value_can_be_null);
  }
}

void IntrinsicCodeGeneratorX86::VisitUnsafePut(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt32, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt32, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt32, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutObject(HInvoke* invoke) {
  GenUnsafePut(
      invoke->GetLocations(), DataType::Type::kReference, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  GenUnsafePut(
      invoke->GetLocations(), DataType::Type::kReference, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  GenUnsafePut(
      invoke->GetLocations(), DataType::Type::kReference, /* is_volatile */ true, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutLong(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt64, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt64, /* is_volatile */ false, codegen_);
}
void IntrinsicCodeGeneratorX86::VisitUnsafePutLongVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(), DataType::Type::kInt64, /* is_volatile */ true, codegen_);
}

static void CreateIntIntIntIntIntToInt(ArenaAllocator* allocator,
                                       DataType::Type type,
                                       HInvoke* invoke) {
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
  // Offset is a long, but in 32 bit mode, we only need the low word.
  // Can we update the invoke here to remove a TypeConvert to Long?
  locations->SetInAt(2, Location::RequiresRegister());
  // Expected value must be in EAX or EDX:EAX.
  // For long, new value must be in ECX:EBX.
  if (type == DataType::Type::kInt64) {
    locations->SetInAt(3, Location::RegisterPairLocation(EAX, EDX));
    locations->SetInAt(4, Location::RegisterPairLocation(EBX, ECX));
  } else {
    locations->SetInAt(3, Location::RegisterLocation(EAX));
    locations->SetInAt(4, Location::RequiresRegister());
  }

  // Force a byte register for the output.
  locations->SetOut(Location::RegisterLocation(EAX));
  if (type == DataType::Type::kReference) {
    // Need temporary registers for card-marking, and possibly for
    // (Baker) read barrier.
    locations->AddTemp(Location::RequiresRegister());  // Possibly used for reference poisoning too.
    // Need a byte register for marking.
    locations->AddTemp(Location::RegisterLocation(ECX));
  }
}

void IntrinsicLocationsBuilderX86::VisitUnsafeCASInt(HInvoke* invoke) {
  CreateIntIntIntIntIntToInt(allocator_, DataType::Type::kInt32, invoke);
}

void IntrinsicLocationsBuilderX86::VisitUnsafeCASLong(HInvoke* invoke) {
  CreateIntIntIntIntIntToInt(allocator_, DataType::Type::kInt64, invoke);
}

void IntrinsicLocationsBuilderX86::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CreateIntIntIntIntIntToInt(allocator_, DataType::Type::kReference, invoke);
}

static void GenCAS(DataType::Type type, HInvoke* invoke, CodeGeneratorX86* codegen) {
  X86Assembler* assembler = down_cast<X86Assembler*>(codegen->GetAssembler());
  LocationSummary* locations = invoke->GetLocations();

  Register base = locations->InAt(1).AsRegister<Register>();
  Register offset = locations->InAt(2).AsRegisterPairLow<Register>();
  Location out = locations->Out();
  DCHECK_EQ(out.AsRegister<Register>(), EAX);

  // The address of the field within the holding object.
  Address field_addr(base, offset, ScaleFactor::TIMES_1, 0);

  if (type == DataType::Type::kReference) {
    // The only read barrier implementation supporting the
    // UnsafeCASObject intrinsic is the Baker-style read barriers.
    DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

    Location temp1_loc = locations->GetTemp(0);
    Register temp1 = temp1_loc.AsRegister<Register>();
    Register temp2 = locations->GetTemp(1).AsRegister<Register>();

    Register expected = locations->InAt(3).AsRegister<Register>();
    // Ensure `expected` is in EAX (required by the CMPXCHG instruction).
    DCHECK_EQ(expected, EAX);
    Register value = locations->InAt(4).AsRegister<Register>();

    // Mark card for object assuming new value is stored.
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(temp1, temp2, base, value, value_can_be_null);

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      // Need to make sure the reference stored in the field is a to-space
      // one before attempting the CAS or the CAS could fail incorrectly.
      codegen->GenerateReferenceLoadWithBakerReadBarrier(
          invoke,
          temp1_loc,  // Unused, used only as a "temporary" within the read barrier.
          base,
          field_addr,
          /* needs_null_check */ false,
          /* always_update_field */ true,
          &temp2);
    }

    bool base_equals_value = (base == value);
    if (kPoisonHeapReferences) {
      if (base_equals_value) {
        // If `base` and `value` are the same register location, move
        // `value` to a temporary register.  This way, poisoning
        // `value` won't invalidate `base`.
        value = temp1;
        __ movl(value, base);
      }

      // Check that the register allocator did not assign the location
      // of `expected` (EAX) to `value` nor to `base`, so that heap
      // poisoning (when enabled) works as intended below.
      // - If `value` were equal to `expected`, both references would
      //   be poisoned twice, meaning they would not be poisoned at
      //   all, as heap poisoning uses address negation.
      // - If `base` were equal to `expected`, poisoning `expected`
      //   would invalidate `base`.
      DCHECK_NE(value, expected);
      DCHECK_NE(base, expected);

      __ PoisonHeapReference(expected);
      __ PoisonHeapReference(value);
    }

    __ LockCmpxchgl(field_addr, value);

    // LOCK CMPXCHG has full barrier semantics, and we don't need
    // scheduling barriers at this time.

    // Convert ZF into the Boolean result.
    __ setb(kZero, out.AsRegister<Register>());
    __ movzxb(out.AsRegister<Register>(), out.AsRegister<ByteRegister>());

    // If heap poisoning is enabled, we need to unpoison the values
    // that were poisoned earlier.
    if (kPoisonHeapReferences) {
      if (base_equals_value) {
        // `value` has been moved to a temporary register, no need to
        // unpoison it.
      } else {
        // Ensure `value` is different from `out`, so that unpoisoning
        // the former does not invalidate the latter.
        DCHECK_NE(value, out.AsRegister<Register>());
        __ UnpoisonHeapReference(value);
      }
      // Do not unpoison the reference contained in register
      // `expected`, as it is the same as register `out` (EAX).
    }
  } else {
    if (type == DataType::Type::kInt32) {
      // Ensure the expected value is in EAX (required by the CMPXCHG
      // instruction).
      DCHECK_EQ(locations->InAt(3).AsRegister<Register>(), EAX);
      __ LockCmpxchgl(field_addr, locations->InAt(4).AsRegister<Register>());
    } else if (type == DataType::Type::kInt64) {
      // Ensure the expected value is in EAX:EDX and that the new
      // value is in EBX:ECX (required by the CMPXCHG8B instruction).
      DCHECK_EQ(locations->InAt(3).AsRegisterPairLow<Register>(), EAX);
      DCHECK_EQ(locations->InAt(3).AsRegisterPairHigh<Register>(), EDX);
      DCHECK_EQ(locations->InAt(4).AsRegisterPairLow<Register>(), EBX);
      DCHECK_EQ(locations->InAt(4).AsRegisterPairHigh<Register>(), ECX);
      __ LockCmpxchg8b(field_addr);
    } else {
      LOG(FATAL) << "Unexpected CAS type " << type;
    }

    // LOCK CMPXCHG/LOCK CMPXCHG8B have full barrier semantics, and we
    // don't need scheduling barriers at this time.

    // Convert ZF into the Boolean result.
    __ setb(kZero, out.AsRegister<Register>());
    __ movzxb(out.AsRegister<Register>(), out.AsRegister<ByteRegister>());
  }
}

void IntrinsicCodeGeneratorX86::VisitUnsafeCASInt(HInvoke* invoke) {
  GenCAS(DataType::Type::kInt32, invoke, codegen_);
}

void IntrinsicCodeGeneratorX86::VisitUnsafeCASLong(HInvoke* invoke) {
  GenCAS(DataType::Type::kInt64, invoke, codegen_);
}

void IntrinsicCodeGeneratorX86::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  GenCAS(DataType::Type::kReference, invoke, codegen_);
}

void IntrinsicLocationsBuilderX86::VisitIntegerReverse(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
}

static void SwapBits(Register reg, Register temp, int32_t shift, int32_t mask,
                     X86Assembler* assembler) {
  Immediate imm_shift(shift);
  Immediate imm_mask(mask);
  __ movl(temp, reg);
  __ shrl(reg, imm_shift);
  __ andl(temp, imm_mask);
  __ andl(reg, imm_mask);
  __ shll(temp, imm_shift);
  __ orl(reg, temp);
}

void IntrinsicCodeGeneratorX86::VisitIntegerReverse(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register reg = locations->InAt(0).AsRegister<Register>();
  Register temp = locations->GetTemp(0).AsRegister<Register>();

  /*
   * Use one bswap instruction to reverse byte order first and then use 3 rounds of
   * swapping bits to reverse bits in a number x. Using bswap to save instructions
   * compared to generic luni implementation which has 5 rounds of swapping bits.
   * x = bswap x
   * x = (x & 0x55555555) << 1 | (x >> 1) & 0x55555555;
   * x = (x & 0x33333333) << 2 | (x >> 2) & 0x33333333;
   * x = (x & 0x0F0F0F0F) << 4 | (x >> 4) & 0x0F0F0F0F;
   */
  __ bswapl(reg);
  SwapBits(reg, temp, 1, 0x55555555, assembler);
  SwapBits(reg, temp, 2, 0x33333333, assembler);
  SwapBits(reg, temp, 4, 0x0f0f0f0f, assembler);
}

void IntrinsicLocationsBuilderX86::VisitLongReverse(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::SameAsFirstInput());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorX86::VisitLongReverse(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register reg_low = locations->InAt(0).AsRegisterPairLow<Register>();
  Register reg_high = locations->InAt(0).AsRegisterPairHigh<Register>();
  Register temp = locations->GetTemp(0).AsRegister<Register>();

  // We want to swap high/low, then bswap each one, and then do the same
  // as a 32 bit reverse.
  // Exchange high and low.
  __ movl(temp, reg_low);
  __ movl(reg_low, reg_high);
  __ movl(reg_high, temp);

  // bit-reverse low
  __ bswapl(reg_low);
  SwapBits(reg_low, temp, 1, 0x55555555, assembler);
  SwapBits(reg_low, temp, 2, 0x33333333, assembler);
  SwapBits(reg_low, temp, 4, 0x0f0f0f0f, assembler);

  // bit-reverse high
  __ bswapl(reg_high);
  SwapBits(reg_high, temp, 1, 0x55555555, assembler);
  SwapBits(reg_high, temp, 2, 0x33333333, assembler);
  SwapBits(reg_high, temp, 4, 0x0f0f0f0f, assembler);
}

static void CreateBitCountLocations(
    ArenaAllocator* allocator, CodeGeneratorX86* codegen, HInvoke* invoke, bool is_long) {
  if (!codegen->GetInstructionSetFeatures().HasPopCnt()) {
    // Do nothing if there is no popcnt support. This results in generating
    // a call for the intrinsic rather than direct code.
    return;
  }
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  if (is_long) {
    locations->AddTemp(Location::RequiresRegister());
  }
  locations->SetInAt(0, Location::Any());
  locations->SetOut(Location::RequiresRegister());
}

static void GenBitCount(X86Assembler* assembler,
                        CodeGeneratorX86* codegen,
                        HInvoke* invoke, bool is_long) {
  LocationSummary* locations = invoke->GetLocations();
  Location src = locations->InAt(0);
  Register out = locations->Out().AsRegister<Register>();

  if (invoke->InputAt(0)->IsConstant()) {
    // Evaluate this at compile time.
    int64_t value = Int64FromConstant(invoke->InputAt(0)->AsConstant());
    int32_t result = is_long
        ? POPCOUNT(static_cast<uint64_t>(value))
        : POPCOUNT(static_cast<uint32_t>(value));
    codegen->Load32BitValue(out, result);
    return;
  }

  // Handle the non-constant cases.
  if (!is_long) {
    if (src.IsRegister()) {
      __ popcntl(out, src.AsRegister<Register>());
    } else {
      DCHECK(src.IsStackSlot());
      __ popcntl(out, Address(ESP, src.GetStackIndex()));
    }
  } else {
    // The 64-bit case needs to worry about two parts.
    Register temp = locations->GetTemp(0).AsRegister<Register>();
    if (src.IsRegisterPair()) {
      __ popcntl(temp, src.AsRegisterPairLow<Register>());
      __ popcntl(out, src.AsRegisterPairHigh<Register>());
    } else {
      DCHECK(src.IsDoubleStackSlot());
      __ popcntl(temp, Address(ESP, src.GetStackIndex()));
      __ popcntl(out, Address(ESP, src.GetHighStackIndex(kX86WordSize)));
    }
    __ addl(out, temp);
  }
}

void IntrinsicLocationsBuilderX86::VisitIntegerBitCount(HInvoke* invoke) {
  CreateBitCountLocations(allocator_, codegen_, invoke, /* is_long */ false);
}

void IntrinsicCodeGeneratorX86::VisitIntegerBitCount(HInvoke* invoke) {
  GenBitCount(GetAssembler(), codegen_, invoke, /* is_long */ false);
}

void IntrinsicLocationsBuilderX86::VisitLongBitCount(HInvoke* invoke) {
  CreateBitCountLocations(allocator_, codegen_, invoke, /* is_long */ true);
}

void IntrinsicCodeGeneratorX86::VisitLongBitCount(HInvoke* invoke) {
  GenBitCount(GetAssembler(), codegen_, invoke, /* is_long */ true);
}

static void CreateLeadingZeroLocations(ArenaAllocator* allocator, HInvoke* invoke, bool is_long) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  if (is_long) {
    locations->SetInAt(0, Location::RequiresRegister());
  } else {
    locations->SetInAt(0, Location::Any());
  }
  locations->SetOut(Location::RequiresRegister());
}

static void GenLeadingZeros(X86Assembler* assembler,
                            CodeGeneratorX86* codegen,
                            HInvoke* invoke, bool is_long) {
  LocationSummary* locations = invoke->GetLocations();
  Location src = locations->InAt(0);
  Register out = locations->Out().AsRegister<Register>();

  if (invoke->InputAt(0)->IsConstant()) {
    // Evaluate this at compile time.
    int64_t value = Int64FromConstant(invoke->InputAt(0)->AsConstant());
    if (value == 0) {
      value = is_long ? 64 : 32;
    } else {
      value = is_long ? CLZ(static_cast<uint64_t>(value)) : CLZ(static_cast<uint32_t>(value));
    }
    codegen->Load32BitValue(out, value);
    return;
  }

  // Handle the non-constant cases.
  if (!is_long) {
    if (src.IsRegister()) {
      __ bsrl(out, src.AsRegister<Register>());
    } else {
      DCHECK(src.IsStackSlot());
      __ bsrl(out, Address(ESP, src.GetStackIndex()));
    }

    // BSR sets ZF if the input was zero, and the output is undefined.
    NearLabel all_zeroes, done;
    __ j(kEqual, &all_zeroes);

    // Correct the result from BSR to get the final CLZ result.
    __ xorl(out, Immediate(31));
    __ jmp(&done);

    // Fix the zero case with the expected result.
    __ Bind(&all_zeroes);
    __ movl(out, Immediate(32));

    __ Bind(&done);
    return;
  }

  // 64 bit case needs to worry about both parts of the register.
  DCHECK(src.IsRegisterPair());
  Register src_lo = src.AsRegisterPairLow<Register>();
  Register src_hi = src.AsRegisterPairHigh<Register>();
  NearLabel handle_low, done, all_zeroes;

  // Is the high word zero?
  __ testl(src_hi, src_hi);
  __ j(kEqual, &handle_low);

  // High word is not zero. We know that the BSR result is defined in this case.
  __ bsrl(out, src_hi);

  // Correct the result from BSR to get the final CLZ result.
  __ xorl(out, Immediate(31));
  __ jmp(&done);

  // High word was zero.  We have to compute the low word count and add 32.
  __ Bind(&handle_low);
  __ bsrl(out, src_lo);
  __ j(kEqual, &all_zeroes);

  // We had a valid result.  Use an XOR to both correct the result and add 32.
  __ xorl(out, Immediate(63));
  __ jmp(&done);

  // All zero case.
  __ Bind(&all_zeroes);
  __ movl(out, Immediate(64));

  __ Bind(&done);
}

void IntrinsicLocationsBuilderX86::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateLeadingZeroLocations(allocator_, invoke, /* is_long */ false);
}

void IntrinsicCodeGeneratorX86::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  GenLeadingZeros(GetAssembler(), codegen_, invoke, /* is_long */ false);
}

void IntrinsicLocationsBuilderX86::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateLeadingZeroLocations(allocator_, invoke, /* is_long */ true);
}

void IntrinsicCodeGeneratorX86::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  GenLeadingZeros(GetAssembler(), codegen_, invoke, /* is_long */ true);
}

static void CreateTrailingZeroLocations(ArenaAllocator* allocator, HInvoke* invoke, bool is_long) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  if (is_long) {
    locations->SetInAt(0, Location::RequiresRegister());
  } else {
    locations->SetInAt(0, Location::Any());
  }
  locations->SetOut(Location::RequiresRegister());
}

static void GenTrailingZeros(X86Assembler* assembler,
                             CodeGeneratorX86* codegen,
                             HInvoke* invoke, bool is_long) {
  LocationSummary* locations = invoke->GetLocations();
  Location src = locations->InAt(0);
  Register out = locations->Out().AsRegister<Register>();

  if (invoke->InputAt(0)->IsConstant()) {
    // Evaluate this at compile time.
    int64_t value = Int64FromConstant(invoke->InputAt(0)->AsConstant());
    if (value == 0) {
      value = is_long ? 64 : 32;
    } else {
      value = is_long ? CTZ(static_cast<uint64_t>(value)) : CTZ(static_cast<uint32_t>(value));
    }
    codegen->Load32BitValue(out, value);
    return;
  }

  // Handle the non-constant cases.
  if (!is_long) {
    if (src.IsRegister()) {
      __ bsfl(out, src.AsRegister<Register>());
    } else {
      DCHECK(src.IsStackSlot());
      __ bsfl(out, Address(ESP, src.GetStackIndex()));
    }

    // BSF sets ZF if the input was zero, and the output is undefined.
    NearLabel done;
    __ j(kNotEqual, &done);

    // Fix the zero case with the expected result.
    __ movl(out, Immediate(32));

    __ Bind(&done);
    return;
  }

  // 64 bit case needs to worry about both parts of the register.
  DCHECK(src.IsRegisterPair());
  Register src_lo = src.AsRegisterPairLow<Register>();
  Register src_hi = src.AsRegisterPairHigh<Register>();
  NearLabel done, all_zeroes;

  // If the low word is zero, then ZF will be set.  If not, we have the answer.
  __ bsfl(out, src_lo);
  __ j(kNotEqual, &done);

  // Low word was zero.  We have to compute the high word count and add 32.
  __ bsfl(out, src_hi);
  __ j(kEqual, &all_zeroes);

  // We had a valid result.  Add 32 to account for the low word being zero.
  __ addl(out, Immediate(32));
  __ jmp(&done);

  // All zero case.
  __ Bind(&all_zeroes);
  __ movl(out, Immediate(64));

  __ Bind(&done);
}

void IntrinsicLocationsBuilderX86::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  CreateTrailingZeroLocations(allocator_, invoke, /* is_long */ false);
}

void IntrinsicCodeGeneratorX86::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  GenTrailingZeros(GetAssembler(), codegen_, invoke, /* is_long */ false);
}

void IntrinsicLocationsBuilderX86::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  CreateTrailingZeroLocations(allocator_, invoke, /* is_long */ true);
}

void IntrinsicCodeGeneratorX86::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  GenTrailingZeros(GetAssembler(), codegen_, invoke, /* is_long */ true);
}

static bool IsSameInput(HInstruction* instruction, size_t input0, size_t input1) {
  return instruction->InputAt(input0) == instruction->InputAt(input1);
}

// Compute base address for the System.arraycopy intrinsic in `base`.
static void GenSystemArrayCopyBaseAddress(X86Assembler* assembler,
                                          DataType::Type type,
                                          const Register& array,
                                          const Location& pos,
                                          const Register& base) {
  // This routine is only used by the SystemArrayCopy intrinsic at the
  // moment. We can allow DataType::Type::kReference as `type` to implement
  // the SystemArrayCopyChar intrinsic.
  DCHECK_EQ(type, DataType::Type::kReference);
  const int32_t element_size = DataType::Size(type);
  const ScaleFactor scale_factor = static_cast<ScaleFactor>(DataType::SizeShift(type));
  const uint32_t data_offset = mirror::Array::DataOffset(element_size).Uint32Value();

  if (pos.IsConstant()) {
    int32_t constant = pos.GetConstant()->AsIntConstant()->GetValue();
    __ leal(base, Address(array, element_size * constant + data_offset));
  } else {
    __ leal(base, Address(array, pos.AsRegister<Register>(), scale_factor, data_offset));
  }
}

// Compute end source address for the System.arraycopy intrinsic in `end`.
static void GenSystemArrayCopyEndAddress(X86Assembler* assembler,
                                         DataType::Type type,
                                         const Location& copy_length,
                                         const Register& base,
                                         const Register& end) {
  // This routine is only used by the SystemArrayCopy intrinsic at the
  // moment. We can allow DataType::Type::kReference as `type` to implement
  // the SystemArrayCopyChar intrinsic.
  DCHECK_EQ(type, DataType::Type::kReference);
  const int32_t element_size = DataType::Size(type);
  const ScaleFactor scale_factor = static_cast<ScaleFactor>(DataType::SizeShift(type));

  if (copy_length.IsConstant()) {
    int32_t constant = copy_length.GetConstant()->AsIntConstant()->GetValue();
    __ leal(end, Address(base, element_size * constant));
  } else {
    __ leal(end, Address(base, copy_length.AsRegister<Register>(), scale_factor, 0));
  }
}

void IntrinsicLocationsBuilderX86::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CodeGenerator::CreateSystemArrayCopyLocationSummary(invoke);
  if (invoke->GetLocations() != nullptr) {
    // Need a byte register for marking.
    invoke->GetLocations()->SetTempAt(1, Location::RegisterLocation(ECX));

    static constexpr size_t kSrc = 0;
    static constexpr size_t kSrcPos = 1;
    static constexpr size_t kDest = 2;
    static constexpr size_t kDestPos = 3;
    static constexpr size_t kLength = 4;

    if (!invoke->InputAt(kSrcPos)->IsIntConstant() &&
        !invoke->InputAt(kDestPos)->IsIntConstant() &&
        !invoke->InputAt(kLength)->IsIntConstant()) {
      if (!IsSameInput(invoke, kSrcPos, kDestPos) &&
          !IsSameInput(invoke, kSrcPos, kLength) &&
          !IsSameInput(invoke, kDestPos, kLength) &&
          !IsSameInput(invoke, kSrc, kDest)) {
        // Not enough registers, make the length also take a stack slot.
        invoke->GetLocations()->SetInAt(kLength, Location::Any());
      }
    }
  }
}

void IntrinsicCodeGeneratorX86::VisitSystemArrayCopy(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // SystemArrayCopy intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  X86Assembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  uint32_t class_offset = mirror::Object::ClassOffset().Int32Value();
  uint32_t super_offset = mirror::Class::SuperClassOffset().Int32Value();
  uint32_t component_offset = mirror::Class::ComponentTypeOffset().Int32Value();
  uint32_t primitive_offset = mirror::Class::PrimitiveTypeOffset().Int32Value();
  uint32_t monitor_offset = mirror::Object::MonitorOffset().Int32Value();

  Register src = locations->InAt(0).AsRegister<Register>();
  Location src_pos = locations->InAt(1);
  Register dest = locations->InAt(2).AsRegister<Register>();
  Location dest_pos = locations->InAt(3);
  Location length_arg = locations->InAt(4);
  Location length = length_arg;
  Location temp1_loc = locations->GetTemp(0);
  Register temp1 = temp1_loc.AsRegister<Register>();
  Location temp2_loc = locations->GetTemp(1);
  Register temp2 = temp2_loc.AsRegister<Register>();

  SlowPathCode* intrinsic_slow_path =
      new (codegen_->GetScopedAllocator()) IntrinsicSlowPathX86(invoke);
  codegen_->AddSlowPath(intrinsic_slow_path);

  NearLabel conditions_on_positions_validated;
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
        __ cmpl(src, dest);
        __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
      }
    } else {
      if (!optimizations.GetDestinationIsSource()) {
        __ cmpl(src, dest);
        __ j(kNotEqual, &conditions_on_positions_validated);
      }
      __ cmpl(dest_pos.AsRegister<Register>(), Immediate(src_pos_constant));
      __ j(kGreater, intrinsic_slow_path->GetEntryLabel());
    }
  } else {
    if (!optimizations.GetDestinationIsSource()) {
      __ cmpl(src, dest);
      __ j(kNotEqual, &conditions_on_positions_validated);
    }
    if (dest_pos.IsConstant()) {
      int32_t dest_pos_constant = dest_pos.GetConstant()->AsIntConstant()->GetValue();
      __ cmpl(src_pos.AsRegister<Register>(), Immediate(dest_pos_constant));
      __ j(kLess, intrinsic_slow_path->GetEntryLabel());
    } else {
      __ cmpl(src_pos.AsRegister<Register>(), dest_pos.AsRegister<Register>());
      __ j(kLess, intrinsic_slow_path->GetEntryLabel());
    }
  }

  __ Bind(&conditions_on_positions_validated);

  if (!optimizations.GetSourceIsNotNull()) {
    // Bail out if the source is null.
    __ testl(src, src);
    __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
  }

  if (!optimizations.GetDestinationIsNotNull() && !optimizations.GetDestinationIsSource()) {
    // Bail out if the destination is null.
    __ testl(dest, dest);
    __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
  }

  Location temp3_loc = locations->GetTemp(2);
  Register temp3 = temp3_loc.AsRegister<Register>();
  if (length.IsStackSlot()) {
    __ movl(temp3, Address(ESP, length.GetStackIndex()));
    length = Location::RegisterLocation(temp3);
  }

  // If the length is negative, bail out.
  // We have already checked in the LocationsBuilder for the constant case.
  if (!length.IsConstant() &&
      !optimizations.GetCountIsSourceLength() &&
      !optimizations.GetCountIsDestinationLength()) {
    __ testl(length.AsRegister<Register>(), length.AsRegister<Register>());
    __ j(kLess, intrinsic_slow_path->GetEntryLabel());
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

    if (!optimizations.GetSourceIsNonPrimitiveArray()) {
      if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
        // /* HeapReference<Class> */ temp1 = src->klass_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, src, class_offset, /* needs_null_check */ false);
        // Bail out if the source is not a non primitive array.
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, temp1, component_offset, /* needs_null_check */ false);
        __ testl(temp1, temp1);
        __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
        // If heap poisoning is enabled, `temp1` has been unpoisoned
        // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
      } else {
        // /* HeapReference<Class> */ temp1 = src->klass_
        __ movl(temp1, Address(src, class_offset));
        __ MaybeUnpoisonHeapReference(temp1);
        // Bail out if the source is not a non primitive array.
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        __ movl(temp1, Address(temp1, component_offset));
        __ testl(temp1, temp1);
        __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
        __ MaybeUnpoisonHeapReference(temp1);
      }
      __ cmpw(Address(temp1, primitive_offset), Immediate(Primitive::kPrimNot));
      __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
    }

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      if (length.Equals(Location::RegisterLocation(temp3))) {
        // When Baker read barriers are enabled, register `temp3`,
        // which in the present case contains the `length` parameter,
        // will be overwritten below.  Make the `length` location
        // reference the original stack location; it will be moved
        // back to `temp3` later if necessary.
        DCHECK(length_arg.IsStackSlot());
        length = length_arg;
      }

      // /* HeapReference<Class> */ temp1 = dest->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp1_loc, dest, class_offset, /* needs_null_check */ false);

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
            invoke, temp2_loc, temp1, component_offset, /* needs_null_check */ false);
        __ testl(temp2, temp2);
        __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
        // If heap poisoning is enabled, `temp2` has been unpoisoned
        // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
        __ cmpw(Address(temp2, primitive_offset), Immediate(Primitive::kPrimNot));
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
      }

      // For the same reason given earlier, `temp1` is not trashed by the
      // read barrier emitted by GenerateFieldLoadWithBakerReadBarrier below.
      // /* HeapReference<Class> */ temp2 = src->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp2_loc, src, class_offset, /* needs_null_check */ false);
      // Note: if heap poisoning is on, we are comparing two unpoisoned references here.
      __ cmpl(temp1, temp2);

      if (optimizations.GetDestinationIsTypedObjectArray()) {
        NearLabel do_copy;
        __ j(kEqual, &do_copy);
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        codegen_->GenerateFieldLoadWithBakerReadBarrier(
            invoke, temp1_loc, temp1, component_offset, /* needs_null_check */ false);
        // We do not need to emit a read barrier for the following
        // heap reference load, as `temp1` is only used in a
        // comparison with null below, and this reference is not
        // kept afterwards.
        __ cmpl(Address(temp1, super_offset), Immediate(0));
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
        __ Bind(&do_copy);
      } else {
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
      }
    } else {
      // Non read barrier code.

      // /* HeapReference<Class> */ temp1 = dest->klass_
      __ movl(temp1, Address(dest, class_offset));
      if (!optimizations.GetDestinationIsNonPrimitiveArray()) {
        __ MaybeUnpoisonHeapReference(temp1);
        // Bail out if the destination is not a non primitive array.
        // /* HeapReference<Class> */ temp2 = temp1->component_type_
        __ movl(temp2, Address(temp1, component_offset));
        __ testl(temp2, temp2);
        __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
        __ MaybeUnpoisonHeapReference(temp2);
        __ cmpw(Address(temp2, primitive_offset), Immediate(Primitive::kPrimNot));
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
        // Re-poison the heap reference to make the compare instruction below
        // compare two poisoned references.
        __ PoisonHeapReference(temp1);
      }

      // Note: if heap poisoning is on, we are comparing two poisoned references here.
      __ cmpl(temp1, Address(src, class_offset));

      if (optimizations.GetDestinationIsTypedObjectArray()) {
        NearLabel do_copy;
        __ j(kEqual, &do_copy);
        __ MaybeUnpoisonHeapReference(temp1);
        // /* HeapReference<Class> */ temp1 = temp1->component_type_
        __ movl(temp1, Address(temp1, component_offset));
        __ MaybeUnpoisonHeapReference(temp1);
        __ cmpl(Address(temp1, super_offset), Immediate(0));
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
        __ Bind(&do_copy);
      } else {
        __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
      }
    }
  } else if (!optimizations.GetSourceIsNonPrimitiveArray()) {
    DCHECK(optimizations.GetDestinationIsNonPrimitiveArray());
    // Bail out if the source is not a non primitive array.
    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      // /* HeapReference<Class> */ temp1 = src->klass_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp1_loc, src, class_offset, /* needs_null_check */ false);
      // /* HeapReference<Class> */ temp1 = temp1->component_type_
      codegen_->GenerateFieldLoadWithBakerReadBarrier(
          invoke, temp1_loc, temp1, component_offset, /* needs_null_check */ false);
      __ testl(temp1, temp1);
      __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
      // If heap poisoning is enabled, `temp1` has been unpoisoned
      // by the the previous call to GenerateFieldLoadWithBakerReadBarrier.
    } else {
      // /* HeapReference<Class> */ temp1 = src->klass_
      __ movl(temp1, Address(src, class_offset));
      __ MaybeUnpoisonHeapReference(temp1);
      // /* HeapReference<Class> */ temp1 = temp1->component_type_
      __ movl(temp1, Address(temp1, component_offset));
      __ testl(temp1, temp1);
      __ j(kEqual, intrinsic_slow_path->GetEntryLabel());
      __ MaybeUnpoisonHeapReference(temp1);
    }
    __ cmpw(Address(temp1, primitive_offset), Immediate(Primitive::kPrimNot));
    __ j(kNotEqual, intrinsic_slow_path->GetEntryLabel());
  }

  const DataType::Type type = DataType::Type::kReference;
  const int32_t element_size = DataType::Size(type);

  // Compute the base source address in `temp1`.
  GenSystemArrayCopyBaseAddress(GetAssembler(), type, src, src_pos, temp1);

  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    // If it is needed (in the case of the fast-path loop), the base
    // destination address is computed later, as `temp2` is used for
    // intermediate computations.

    // Compute the end source address in `temp3`.
    if (length.IsStackSlot()) {
      // Location `length` is again pointing at a stack slot, as
      // register `temp3` (which was containing the length parameter
      // earlier) has been overwritten; restore it now
      DCHECK(length.Equals(length_arg));
      __ movl(temp3, Address(ESP, length.GetStackIndex()));
      length = Location::RegisterLocation(temp3);
    }
    GenSystemArrayCopyEndAddress(GetAssembler(), type, length, temp1, temp3);

    // SystemArrayCopy implementation for Baker read barriers (see
    // also CodeGeneratorX86::GenerateReferenceLoadWithBakerReadBarrier):
    //
    //   if (src_ptr != end_ptr) {
    //     uint32_t rb_state = Lockword(src->monitor_).ReadBarrierState();
    //     lfence;  // Load fence or artificial data dependency to prevent load-load reordering
    //     bool is_gray = (rb_state == ReadBarrier::GrayState());
    //     if (is_gray) {
    //       // Slow-path copy.
    //       for (size_t i = 0; i != length; ++i) {
    //         dest_array[dest_pos + i] =
    //             MaybePoison(ReadBarrier::Mark(MaybeUnpoison(src_array[src_pos + i])));
    //       }
    //     } else {
    //       // Fast-path copy.
    //       do {
    //         *dest_ptr++ = *src_ptr++;
    //       } while (src_ptr != end_ptr)
    //     }
    //   }

    NearLabel loop, done;

    // Don't enter copy loop if `length == 0`.
    __ cmpl(temp1, temp3);
    __ j(kEqual, &done);

    // Given the numeric representation, it's enough to check the low bit of the rb_state.
    static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
    static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
    constexpr uint32_t gray_byte_position = LockWord::kReadBarrierStateShift / kBitsPerByte;
    constexpr uint32_t gray_bit_position = LockWord::kReadBarrierStateShift % kBitsPerByte;
    constexpr int32_t test_value = static_cast<int8_t>(1 << gray_bit_position);

    // if (rb_state == ReadBarrier::GrayState())
    //   goto slow_path;
    // At this point, just do the "if" and make sure that flags are preserved until the branch.
    __ testb(Address(src, monitor_offset + gray_byte_position), Immediate(test_value));

    // Load fence to prevent load-load reordering.
    // Note that this is a no-op, thanks to the x86 memory model.
    codegen_->GenerateMemoryBarrier(MemBarrierKind::kLoadAny);

    // Slow path used to copy array when `src` is gray.
    SlowPathCode* read_barrier_slow_path =
        new (codegen_->GetScopedAllocator()) ReadBarrierSystemArrayCopySlowPathX86(invoke);
    codegen_->AddSlowPath(read_barrier_slow_path);

    // We have done the "if" of the gray bit check above, now branch based on the flags.
    __ j(kNotZero, read_barrier_slow_path->GetEntryLabel());

    // Fast-path copy.
    // Compute the base destination address in `temp2`.
    GenSystemArrayCopyBaseAddress(GetAssembler(), type, dest, dest_pos, temp2);
    // Iterate over the arrays and do a raw copy of the objects. We don't need to
    // poison/unpoison.
    __ Bind(&loop);
    __ pushl(Address(temp1, 0));
    __ cfi().AdjustCFAOffset(4);
    __ popl(Address(temp2, 0));
    __ cfi().AdjustCFAOffset(-4);
    __ addl(temp1, Immediate(element_size));
    __ addl(temp2, Immediate(element_size));
    __ cmpl(temp1, temp3);
    __ j(kNotEqual, &loop);

    __ Bind(read_barrier_slow_path->GetExitLabel());
    __ Bind(&done);
  } else {
    // Non read barrier code.
    // Compute the base destination address in `temp2`.
    GenSystemArrayCopyBaseAddress(GetAssembler(), type, dest, dest_pos, temp2);
    // Compute the end source address in `temp3`.
    GenSystemArrayCopyEndAddress(GetAssembler(), type, length, temp1, temp3);
    // Iterate over the arrays and do a raw copy of the objects. We don't need to
    // poison/unpoison.
    NearLabel loop, done;
    __ cmpl(temp1, temp3);
    __ j(kEqual, &done);
    __ Bind(&loop);
    __ pushl(Address(temp1, 0));
    __ cfi().AdjustCFAOffset(4);
    __ popl(Address(temp2, 0));
    __ cfi().AdjustCFAOffset(-4);
    __ addl(temp1, Immediate(element_size));
    __ addl(temp2, Immediate(element_size));
    __ cmpl(temp1, temp3);
    __ j(kNotEqual, &loop);
    __ Bind(&done);
  }

  // We only need one card marking on the destination array.
  codegen_->MarkGCCard(temp1, temp2, dest, Register(kNoRegister), /* value_can_be_null */ false);

  __ Bind(intrinsic_slow_path->GetExitLabel());
}

void IntrinsicLocationsBuilderX86::VisitIntegerValueOf(HInvoke* invoke) {
  InvokeRuntimeCallingConvention calling_convention;
  IntrinsicVisitor::ComputeIntegerValueOfLocations(
      invoke,
      codegen_,
      Location::RegisterLocation(EAX),
      Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void IntrinsicCodeGeneratorX86::VisitIntegerValueOf(HInvoke* invoke) {
  IntrinsicVisitor::IntegerValueOfInfo info = IntrinsicVisitor::ComputeIntegerValueOfInfo();
  LocationSummary* locations = invoke->GetLocations();
  X86Assembler* assembler = GetAssembler();

  Register out = locations->Out().AsRegister<Register>();
  InvokeRuntimeCallingConvention calling_convention;
  if (invoke->InputAt(0)->IsConstant()) {
    int32_t value = invoke->InputAt(0)->AsIntConstant()->GetValue();
    if (value >= info.low && value <= info.high) {
      // Just embed the j.l.Integer in the code.
      ScopedObjectAccess soa(Thread::Current());
      mirror::Object* boxed = info.cache->Get(value + (-info.low));
      DCHECK(boxed != nullptr && Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boxed));
      uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(boxed));
      __ movl(out, Immediate(address));
    } else {
      // Allocate and initialize a new j.l.Integer.
      // TODO: If we JIT, we could allocate the j.l.Integer now, and store it in the
      // JIT object table.
      uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
      __ movl(calling_convention.GetRegisterAt(0), Immediate(address));
      codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
      CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
      __ movl(Address(out, info.value_offset), Immediate(value));
    }
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();
    // Check bounds of our cache.
    __ leal(out, Address(in, -info.low));
    __ cmpl(out, Immediate(info.high - info.low + 1));
    NearLabel allocate, done;
    __ j(kAboveEqual, &allocate);
    // If the value is within the bounds, load the j.l.Integer directly from the array.
    uint32_t data_offset = mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();
    uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.cache));
    __ movl(out, Address(out, TIMES_4, data_offset + address));
    __ MaybeUnpoisonHeapReference(out);
    __ jmp(&done);
    __ Bind(&allocate);
    // Otherwise allocate and initialize a new j.l.Integer.
    address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
    __ movl(calling_convention.GetRegisterAt(0), Immediate(address));
    codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
    __ movl(Address(out, info.value_offset), in);
    __ Bind(&done);
  }
}

void IntrinsicLocationsBuilderX86::VisitThreadInterrupted(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorX86::VisitThreadInterrupted(HInvoke* invoke) {
  X86Assembler* assembler = GetAssembler();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();
  Address address = Address::Absolute(Thread::InterruptedOffset<kX86PointerSize>().Int32Value());
  NearLabel done;
  __ fs()->movl(out, address);
  __ testl(out, out);
  __ j(kEqual, &done);
  __ fs()->movl(address, Immediate(0));
  codegen_->MemoryFence();
  __ Bind(&done);
}

void IntrinsicLocationsBuilderX86::VisitReachabilityFence(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::Any());
}

void IntrinsicCodeGeneratorX86::VisitReachabilityFence(HInvoke* invoke ATTRIBUTE_UNUSED) { }

UNIMPLEMENTED_INTRINSIC(X86, MathRoundDouble)
UNIMPLEMENTED_INTRINSIC(X86, ReferenceGetReferent)
UNIMPLEMENTED_INTRINSIC(X86, FloatIsInfinite)
UNIMPLEMENTED_INTRINSIC(X86, DoubleIsInfinite)
UNIMPLEMENTED_INTRINSIC(X86, IntegerHighestOneBit)
UNIMPLEMENTED_INTRINSIC(X86, LongHighestOneBit)
UNIMPLEMENTED_INTRINSIC(X86, IntegerLowestOneBit)
UNIMPLEMENTED_INTRINSIC(X86, LongLowestOneBit)

UNIMPLEMENTED_INTRINSIC(X86, StringStringIndexOf);
UNIMPLEMENTED_INTRINSIC(X86, StringStringIndexOfAfter);
UNIMPLEMENTED_INTRINSIC(X86, StringBufferAppend);
UNIMPLEMENTED_INTRINSIC(X86, StringBufferLength);
UNIMPLEMENTED_INTRINSIC(X86, StringBufferToString);
UNIMPLEMENTED_INTRINSIC(X86, StringBuilderAppend);
UNIMPLEMENTED_INTRINSIC(X86, StringBuilderLength);
UNIMPLEMENTED_INTRINSIC(X86, StringBuilderToString);

// 1.8.
UNIMPLEMENTED_INTRINSIC(X86, UnsafeGetAndAddInt)
UNIMPLEMENTED_INTRINSIC(X86, UnsafeGetAndAddLong)
UNIMPLEMENTED_INTRINSIC(X86, UnsafeGetAndSetInt)
UNIMPLEMENTED_INTRINSIC(X86, UnsafeGetAndSetLong)
UNIMPLEMENTED_INTRINSIC(X86, UnsafeGetAndSetObject)

UNREACHABLE_INTRINSICS(X86)

#undef __

}  // namespace x86
}  // namespace art
