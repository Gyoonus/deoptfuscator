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

#include "intrinsics_mips.h"

#include "arch/mips/instruction_set_features_mips.h"
#include "art_method.h"
#include "code_generator_mips.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "heap_poisoning.h"
#include "intrinsics.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "utils/mips/assembler_mips.h"
#include "utils/mips/constants_mips.h"

namespace art {

namespace mips {

IntrinsicLocationsBuilderMIPS::IntrinsicLocationsBuilderMIPS(CodeGeneratorMIPS* codegen)
  : codegen_(codegen), allocator_(codegen->GetGraph()->GetAllocator()) {
}

MipsAssembler* IntrinsicCodeGeneratorMIPS::GetAssembler() {
  return reinterpret_cast<MipsAssembler*>(codegen_->GetAssembler());
}

ArenaAllocator* IntrinsicCodeGeneratorMIPS::GetAllocator() {
  return codegen_->GetGraph()->GetAllocator();
}

inline bool IntrinsicCodeGeneratorMIPS::IsR2OrNewer() const {
  return codegen_->GetInstructionSetFeatures().IsMipsIsaRevGreaterThanEqual2();
}

inline bool IntrinsicCodeGeneratorMIPS::IsR6() const {
  return codegen_->GetInstructionSetFeatures().IsR6();
}

inline bool IntrinsicCodeGeneratorMIPS::Is32BitFPU() const {
  return codegen_->GetInstructionSetFeatures().Is32BitFloatingPoint();
}

#define __ codegen->GetAssembler()->

static void MoveFromReturnRegister(Location trg,
                                   DataType::Type type,
                                   CodeGeneratorMIPS* codegen) {
  if (!trg.IsValid()) {
    DCHECK_EQ(type, DataType::Type::kVoid);
    return;
  }

  DCHECK_NE(type, DataType::Type::kVoid);

  if (DataType::IsIntegralType(type) || type == DataType::Type::kReference) {
    Register trg_reg = trg.AsRegister<Register>();
    if (trg_reg != V0) {
      __ Move(V0, trg_reg);
    }
  } else {
    FRegister trg_reg = trg.AsFpuRegister<FRegister>();
    if (trg_reg != F0) {
      if (type == DataType::Type::kFloat32) {
        __ MovS(F0, trg_reg);
      } else {
        __ MovD(F0, trg_reg);
      }
    }
  }
}

static void MoveArguments(HInvoke* invoke, CodeGeneratorMIPS* codegen) {
  InvokeDexCallingConventionVisitorMIPS calling_convention_visitor;
  IntrinsicVisitor::MoveArguments(invoke, codegen, &calling_convention_visitor);
}

// Slow-path for fallback (calling the managed code to handle the
// intrinsic) in an intrinsified call. This will copy the arguments
// into the positions for a regular call.
//
// Note: The actual parameters are required to be in the locations
//       given by the invoke's location summary. If an intrinsic
//       modifies those locations before a slowpath call, they must be
//       restored!
class IntrinsicSlowPathMIPS : public SlowPathCodeMIPS {
 public:
  explicit IntrinsicSlowPathMIPS(HInvoke* invoke) : SlowPathCodeMIPS(invoke), invoke_(invoke) { }

  void EmitNativeCode(CodeGenerator* codegen_in) OVERRIDE {
    CodeGeneratorMIPS* codegen = down_cast<CodeGeneratorMIPS*>(codegen_in);

    __ Bind(GetEntryLabel());

    SaveLiveRegisters(codegen, invoke_->GetLocations());

    MoveArguments(invoke_, codegen);

    if (invoke_->IsInvokeStaticOrDirect()) {
      codegen->GenerateStaticOrDirectCall(
          invoke_->AsInvokeStaticOrDirect(), Location::RegisterLocation(A0), this);
    } else {
      codegen->GenerateVirtualCall(
          invoke_->AsInvokeVirtual(), Location::RegisterLocation(A0), this);
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

  const char* GetDescription() const OVERRIDE { return "IntrinsicSlowPathMIPS"; }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPathMIPS);
};

#undef __

bool IntrinsicLocationsBuilderMIPS::TryDispatch(HInvoke* invoke) {
  Dispatch(invoke);
  LocationSummary* res = invoke->GetLocations();
  return res != nullptr && res->Intrinsified();
}

#define __ assembler->

static void CreateFPToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

static void MoveFPToInt(LocationSummary* locations, bool is64bit, MipsAssembler* assembler) {
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();

  if (is64bit) {
    Register out_lo = locations->Out().AsRegisterPairLow<Register>();
    Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

    __ Mfc1(out_lo, in);
    __ MoveFromFpuHigh(out_hi, in);
  } else {
    Register out = locations->Out().AsRegister<Register>();

    __ Mfc1(out, in);
  }
}

// long java.lang.Double.doubleToRawLongBits(double)
void IntrinsicLocationsBuilderMIPS::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitDoubleDoubleToRawLongBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

// int java.lang.Float.floatToRawIntBits(float)
void IntrinsicLocationsBuilderMIPS::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitFloatFloatToRawIntBits(HInvoke* invoke) {
  MoveFPToInt(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}

static void CreateIntToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresFpuRegister());
}

static void MoveIntToFP(LocationSummary* locations, bool is64bit, MipsAssembler* assembler) {
  FRegister out = locations->Out().AsFpuRegister<FRegister>();

  if (is64bit) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();

    __ Mtc1(in_lo, out);
    __ MoveToFpuHigh(in_hi, out);
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();

    __ Mtc1(in, out);
  }
}

// double java.lang.Double.longBitsToDouble(long)
void IntrinsicLocationsBuilderMIPS::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitDoubleLongBitsToDouble(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

// float java.lang.Float.intBitsToFloat(int)
void IntrinsicLocationsBuilderMIPS::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  CreateIntToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitFloatIntBitsToFloat(HInvoke* invoke) {
  MoveIntToFP(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}

static void CreateIntToIntLocations(ArenaAllocator* allocator,
                                    HInvoke* invoke,
                                    Location::OutputOverlap overlaps = Location::kNoOutputOverlap) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), overlaps);
}

static void GenReverse(LocationSummary* locations,
                       DataType::Type type,
                       bool isR2OrNewer,
                       bool isR6,
                       bool reverseBits,
                       MipsAssembler* assembler) {
  DCHECK(type == DataType::Type::kInt16 ||
         type == DataType::Type::kInt32 ||
         type == DataType::Type::kInt64);
  DCHECK(type != DataType::Type::kInt16 || !reverseBits);

  if (type == DataType::Type::kInt16) {
    Register in = locations->InAt(0).AsRegister<Register>();
    Register out = locations->Out().AsRegister<Register>();

    if (isR2OrNewer) {
      __ Wsbh(out, in);
      __ Seh(out, out);
    } else {
      __ Sll(TMP, in, 24);
      __ Sra(TMP, TMP, 16);
      __ Sll(out, in, 16);
      __ Srl(out, out, 24);
      __ Or(out, out, TMP);
    }
  } else if (type == DataType::Type::kInt32) {
    Register in = locations->InAt(0).AsRegister<Register>();
    Register out = locations->Out().AsRegister<Register>();

    if (isR2OrNewer) {
      __ Rotr(out, in, 16);
      __ Wsbh(out, out);
    } else {
      // MIPS32r1
      // __ Rotr(out, in, 16);
      __ Sll(TMP, in, 16);
      __ Srl(out, in, 16);
      __ Or(out, out, TMP);
      // __ Wsbh(out, out);
      __ LoadConst32(AT, 0x00FF00FF);
      __ And(TMP, out, AT);
      __ Sll(TMP, TMP, 8);
      __ Srl(out, out, 8);
      __ And(out, out, AT);
      __ Or(out, out, TMP);
    }
    if (reverseBits) {
      if (isR6) {
        __ Bitswap(out, out);
      } else {
        __ LoadConst32(AT, 0x0F0F0F0F);
        __ And(TMP, out, AT);
        __ Sll(TMP, TMP, 4);
        __ Srl(out, out, 4);
        __ And(out, out, AT);
        __ Or(out, TMP, out);
        __ LoadConst32(AT, 0x33333333);
        __ And(TMP, out, AT);
        __ Sll(TMP, TMP, 2);
        __ Srl(out, out, 2);
        __ And(out, out, AT);
        __ Or(out, TMP, out);
        __ LoadConst32(AT, 0x55555555);
        __ And(TMP, out, AT);
        __ Sll(TMP, TMP, 1);
        __ Srl(out, out, 1);
        __ And(out, out, AT);
        __ Or(out, TMP, out);
      }
    }
  } else if (type == DataType::Type::kInt64) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register out_lo = locations->Out().AsRegisterPairLow<Register>();
    Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

    if (isR2OrNewer) {
      __ Rotr(AT, in_hi, 16);
      __ Rotr(TMP, in_lo, 16);
      __ Wsbh(out_lo, AT);
      __ Wsbh(out_hi, TMP);
    } else {
      // When calling CreateIntToIntLocations() we promised that the
      // use of the out_lo/out_hi wouldn't overlap with the use of
      // in_lo/in_hi. Be very careful not to write to out_lo/out_hi
      // until we're completely done reading from in_lo/in_hi.
      // __ Rotr(TMP, in_lo, 16);
      __ Sll(TMP, in_lo, 16);
      __ Srl(AT, in_lo, 16);
      __ Or(TMP, TMP, AT);             // Hold in TMP until it's safe
                                       // to write to out_hi.
      // __ Rotr(out_lo, in_hi, 16);
      __ Sll(AT, in_hi, 16);
      __ Srl(out_lo, in_hi, 16);        // Here we are finally done reading
                                        // from in_lo/in_hi so it's okay to
                                        // write to out_lo/out_hi.
      __ Or(out_lo, out_lo, AT);
      // __ Wsbh(out_hi, out_hi);
      __ LoadConst32(AT, 0x00FF00FF);
      __ And(out_hi, TMP, AT);
      __ Sll(out_hi, out_hi, 8);
      __ Srl(TMP, TMP, 8);
      __ And(TMP, TMP, AT);
      __ Or(out_hi, out_hi, TMP);
      // __ Wsbh(out_lo, out_lo);
      __ And(TMP, out_lo, AT);  // AT already holds the correct mask value
      __ Sll(TMP, TMP, 8);
      __ Srl(out_lo, out_lo, 8);
      __ And(out_lo, out_lo, AT);
      __ Or(out_lo, out_lo, TMP);
    }
    if (reverseBits) {
      if (isR6) {
        __ Bitswap(out_hi, out_hi);
        __ Bitswap(out_lo, out_lo);
      } else {
        __ LoadConst32(AT, 0x0F0F0F0F);
        __ And(TMP, out_hi, AT);
        __ Sll(TMP, TMP, 4);
        __ Srl(out_hi, out_hi, 4);
        __ And(out_hi, out_hi, AT);
        __ Or(out_hi, TMP, out_hi);
        __ And(TMP, out_lo, AT);
        __ Sll(TMP, TMP, 4);
        __ Srl(out_lo, out_lo, 4);
        __ And(out_lo, out_lo, AT);
        __ Or(out_lo, TMP, out_lo);
        __ LoadConst32(AT, 0x33333333);
        __ And(TMP, out_hi, AT);
        __ Sll(TMP, TMP, 2);
        __ Srl(out_hi, out_hi, 2);
        __ And(out_hi, out_hi, AT);
        __ Or(out_hi, TMP, out_hi);
        __ And(TMP, out_lo, AT);
        __ Sll(TMP, TMP, 2);
        __ Srl(out_lo, out_lo, 2);
        __ And(out_lo, out_lo, AT);
        __ Or(out_lo, TMP, out_lo);
        __ LoadConst32(AT, 0x55555555);
        __ And(TMP, out_hi, AT);
        __ Sll(TMP, TMP, 1);
        __ Srl(out_hi, out_hi, 1);
        __ And(out_hi, out_hi, AT);
        __ Or(out_hi, TMP, out_hi);
        __ And(TMP, out_lo, AT);
        __ Sll(TMP, TMP, 1);
        __ Srl(out_lo, out_lo, 1);
        __ And(out_lo, out_lo, AT);
        __ Or(out_lo, TMP, out_lo);
      }
    }
  }
}

// int java.lang.Integer.reverseBytes(int)
void IntrinsicLocationsBuilderMIPS::VisitIntegerReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerReverseBytes(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(),
             DataType::Type::kInt32,
             IsR2OrNewer(),
             IsR6(),
             /* reverseBits */ false,
             GetAssembler());
}

// long java.lang.Long.reverseBytes(long)
void IntrinsicLocationsBuilderMIPS::VisitLongReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitLongReverseBytes(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(),
             DataType::Type::kInt64,
             IsR2OrNewer(),
             IsR6(),
             /* reverseBits */ false,
             GetAssembler());
}

// short java.lang.Short.reverseBytes(short)
void IntrinsicLocationsBuilderMIPS::VisitShortReverseBytes(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitShortReverseBytes(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(),
             DataType::Type::kInt16,
             IsR2OrNewer(),
             IsR6(),
             /* reverseBits */ false,
             GetAssembler());
}

static void GenNumberOfLeadingZeroes(LocationSummary* locations,
                                     bool is64bit,
                                     bool isR6,
                                     MipsAssembler* assembler) {
  Register out = locations->Out().AsRegister<Register>();
  if (is64bit) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();

    if (isR6) {
      __ ClzR6(AT, in_hi);
      __ ClzR6(TMP, in_lo);
      __ Seleqz(TMP, TMP, in_hi);
    } else {
      __ ClzR2(AT, in_hi);
      __ ClzR2(TMP, in_lo);
      __ Movn(TMP, ZERO, in_hi);
    }
    __ Addu(out, AT, TMP);
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();

    if (isR6) {
      __ ClzR6(out, in);
    } else {
      __ ClzR2(out, in);
    }
  }
}

// int java.lang.Integer.numberOfLeadingZeros(int i)
void IntrinsicLocationsBuilderMIPS::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeroes(invoke->GetLocations(), /* is64bit */ false, IsR6(), GetAssembler());
}

// int java.lang.Long.numberOfLeadingZeros(long i)
void IntrinsicLocationsBuilderMIPS::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitLongNumberOfLeadingZeros(HInvoke* invoke) {
  GenNumberOfLeadingZeroes(invoke->GetLocations(), /* is64bit */ true, IsR6(), GetAssembler());
}

static void GenNumberOfTrailingZeroes(LocationSummary* locations,
                                      bool is64bit,
                                      bool isR6,
                                      MipsAssembler* assembler) {
  Register out = locations->Out().AsRegister<Register>();
  Register in_lo;
  Register in;

  if (is64bit) {
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();

    in_lo = locations->InAt(0).AsRegisterPairLow<Register>();

    // If in_lo is zero then count the number of trailing zeroes in in_hi;
    // otherwise count the number of trailing zeroes in in_lo.
    // out = in_lo ? in_lo : in_hi;
    if (isR6) {
      __ Seleqz(out, in_hi, in_lo);
      __ Selnez(TMP, in_lo, in_lo);
      __ Or(out, out, TMP);
    } else {
      __ Movz(out, in_hi, in_lo);
      __ Movn(out, in_lo, in_lo);
    }

    in = out;
  } else {
    in = locations->InAt(0).AsRegister<Register>();
    // Give in_lo a dummy value to keep the compiler from complaining.
    // Since we only get here in the 32-bit case, this value will never
    // be used.
    in_lo = in;
  }

  if (isR6) {
    // We don't have an instruction to count the number of trailing zeroes.
    // Start by flipping the bits end-for-end so we can count the number of
    // leading zeroes instead.
    __ Rotr(out, in, 16);
    __ Wsbh(out, out);
    __ Bitswap(out, out);
    __ ClzR6(out, out);
  } else {
    // Convert trailing zeroes to trailing ones, and bits to their left
    // to zeroes.
    __ Addiu(TMP, in, -1);
    __ Xor(out, TMP, in);
    __ And(out, out, TMP);
    // Count number of leading zeroes.
    __ ClzR2(out, out);
    // Subtract number of leading zeroes from 32 to get number of trailing ones.
    // Remember that the trailing ones were formerly trailing zeroes.
    __ LoadConst32(TMP, 32);
    __ Subu(out, TMP, out);
  }

  if (is64bit) {
    // If in_lo is zero, then we counted the number of trailing zeroes in in_hi so we must add the
    // number of trailing zeroes in in_lo (32) to get the correct final count
    __ LoadConst32(TMP, 32);
    if (isR6) {
      __ Seleqz(TMP, TMP, in_lo);
    } else {
      __ Movn(TMP, ZERO, in_lo);
    }
    __ Addu(out, out, TMP);
  }
}

// int java.lang.Integer.numberOfTrailingZeros(int i)
void IntrinsicLocationsBuilderMIPS::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke, Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeroes(invoke->GetLocations(), /* is64bit */ false, IsR6(), GetAssembler());
}

// int java.lang.Long.numberOfTrailingZeros(long i)
void IntrinsicLocationsBuilderMIPS::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke, Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorMIPS::VisitLongNumberOfTrailingZeros(HInvoke* invoke) {
  GenNumberOfTrailingZeroes(invoke->GetLocations(), /* is64bit */ true, IsR6(), GetAssembler());
}

// int java.lang.Integer.reverse(int)
void IntrinsicLocationsBuilderMIPS::VisitIntegerReverse(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(),
             DataType::Type::kInt32,
             IsR2OrNewer(),
             IsR6(),
             /* reverseBits */ true,
             GetAssembler());
}

// long java.lang.Long.reverse(long)
void IntrinsicLocationsBuilderMIPS::VisitLongReverse(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitLongReverse(HInvoke* invoke) {
  GenReverse(invoke->GetLocations(),
             DataType::Type::kInt64,
             IsR2OrNewer(),
             IsR6(),
             /* reverseBits */ true,
             GetAssembler());
}

static void CreateFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kNoOutputOverlap);
}

static void GenBitCount(LocationSummary* locations,
                        DataType::Type type,
                        bool isR6,
                        MipsAssembler* assembler) {
  Register out = locations->Out().AsRegister<Register>();

  // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  //
  // A generalization of the best bit counting method to integers of
  // bit-widths up to 128 (parameterized by type T) is this:
  //
  // v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
  // v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
  // v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
  // c = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * BITS_PER_BYTE; // count
  //
  // For comparison, for 32-bit quantities, this algorithm can be executed
  // using 20 MIPS instructions (the calls to LoadConst32() generate two
  // machine instructions each for the values being used in this algorithm).
  // A(n unrolled) loop-based algorithm required 25 instructions.
  //
  // For 64-bit quantities, this algorithm gets executed twice, (once
  // for in_lo, and again for in_hi), but saves a few instructions
  // because the mask values only have to be loaded once.  Using this
  // algorithm the count for a 64-bit operand can be performed in 29
  // instructions compared to a loop-based algorithm which required 47
  // instructions.

  if (type == DataType::Type::kInt32) {
    Register in = locations->InAt(0).AsRegister<Register>();

    __ Srl(TMP, in, 1);
    __ LoadConst32(AT, 0x55555555);
    __ And(TMP, TMP, AT);
    __ Subu(TMP, in, TMP);
    __ LoadConst32(AT, 0x33333333);
    __ And(out, TMP, AT);
    __ Srl(TMP, TMP, 2);
    __ And(TMP, TMP, AT);
    __ Addu(TMP, out, TMP);
    __ Srl(out, TMP, 4);
    __ Addu(out, out, TMP);
    __ LoadConst32(AT, 0x0F0F0F0F);
    __ And(out, out, AT);
    __ LoadConst32(TMP, 0x01010101);
    if (isR6) {
      __ MulR6(out, out, TMP);
    } else {
      __ MulR2(out, out, TMP);
    }
    __ Srl(out, out, 24);
  } else {
    DCHECK_EQ(type, DataType::Type::kInt64);
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register tmp_hi = locations->GetTemp(0).AsRegister<Register>();
    Register out_hi = locations->GetTemp(1).AsRegister<Register>();
    Register tmp_lo = TMP;
    Register out_lo = out;

    __ Srl(tmp_lo, in_lo, 1);
    __ Srl(tmp_hi, in_hi, 1);

    __ LoadConst32(AT, 0x55555555);

    __ And(tmp_lo, tmp_lo, AT);
    __ Subu(tmp_lo, in_lo, tmp_lo);

    __ And(tmp_hi, tmp_hi, AT);
    __ Subu(tmp_hi, in_hi, tmp_hi);

    __ LoadConst32(AT, 0x33333333);

    __ And(out_lo, tmp_lo, AT);
    __ Srl(tmp_lo, tmp_lo, 2);
    __ And(tmp_lo, tmp_lo, AT);
    __ Addu(tmp_lo, out_lo, tmp_lo);

    __ And(out_hi, tmp_hi, AT);
    __ Srl(tmp_hi, tmp_hi, 2);
    __ And(tmp_hi, tmp_hi, AT);
    __ Addu(tmp_hi, out_hi, tmp_hi);

    // Here we deviate from the original algorithm a bit. We've reached
    // the stage where the bitfields holding the subtotals are large
    // enough to hold the combined subtotals for both the low word, and
    // the high word. This means that we can add the subtotals for the
    // the high, and low words into a single word, and compute the final
    // result for both the high, and low words using fewer instructions.
    __ LoadConst32(AT, 0x0F0F0F0F);

    __ Addu(TMP, tmp_hi, tmp_lo);

    __ Srl(out, TMP, 4);
    __ And(out, out, AT);
    __ And(TMP, TMP, AT);
    __ Addu(out, out, TMP);

    __ LoadConst32(AT, 0x01010101);

    if (isR6) {
      __ MulR6(out, out, AT);
    } else {
      __ MulR2(out, out, AT);
    }

    __ Srl(out, out, 24);
  }
}

// int java.lang.Integer.bitCount(int)
void IntrinsicLocationsBuilderMIPS::VisitIntegerBitCount(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerBitCount(HInvoke* invoke) {
  GenBitCount(invoke->GetLocations(), DataType::Type::kInt32, IsR6(), GetAssembler());
}

// int java.lang.Long.bitCount(int)
void IntrinsicLocationsBuilderMIPS::VisitLongBitCount(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitLongBitCount(HInvoke* invoke) {
  GenBitCount(invoke->GetLocations(), DataType::Type::kInt64, IsR6(), GetAssembler());
}

static void MathAbsFP(LocationSummary* locations,
                      bool is64bit,
                      bool isR2OrNewer,
                      bool isR6,
                      MipsAssembler* assembler) {
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister out = locations->Out().AsFpuRegister<FRegister>();

  // Note, as a "quality of implementation", rather than pure "spec compliance", we require that
  // Math.abs() clears the sign bit (but changes nothing else) for all numbers, including NaN
  // (signaling NaN may become quiet though).
  //
  // The ABS.fmt instructions (abs.s and abs.d) do exactly that when NAN2008=1 (R6). For this case,
  // both regular floating point numbers and NAN values are treated alike, only the sign bit is
  // affected by this instruction.
  // But when NAN2008=0 (R2 and before), the ABS.fmt instructions can't be used. For this case, any
  // NaN operand signals invalid operation. This means that other bits (not just sign bit) might be
  // changed when doing abs(NaN). Because of that, we clear sign bit in a different way.
  if (isR6) {
    if (is64bit) {
      __ AbsD(out, in);
    } else {
      __ AbsS(out, in);
    }
  } else {
    if (is64bit) {
      if (in != out) {
        __ MovD(out, in);
      }
      __ MoveFromFpuHigh(TMP, in);
      // ins instruction is not available for R1.
      if (isR2OrNewer) {
        __ Ins(TMP, ZERO, 31, 1);
      } else {
        __ Sll(TMP, TMP, 1);
        __ Srl(TMP, TMP, 1);
      }
      __ MoveToFpuHigh(TMP, out);
    } else {
      __ Mfc1(TMP, in);
      // ins instruction is not available for R1.
      if (isR2OrNewer) {
        __ Ins(TMP, ZERO, 31, 1);
      } else {
        __ Sll(TMP, TMP, 1);
        __ Srl(TMP, TMP, 1);
      }
      __ Mtc1(TMP, out);
    }
  }
}

// double java.lang.Math.abs(double)
void IntrinsicLocationsBuilderMIPS::VisitMathAbsDouble(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAbsDouble(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), /* is64bit */ true, IsR2OrNewer(), IsR6(), GetAssembler());
}

// float java.lang.Math.abs(float)
void IntrinsicLocationsBuilderMIPS::VisitMathAbsFloat(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAbsFloat(HInvoke* invoke) {
  MathAbsFP(invoke->GetLocations(), /* is64bit */ false, IsR2OrNewer(), IsR6(), GetAssembler());
}

static void GenAbsInteger(LocationSummary* locations, bool is64bit, MipsAssembler* assembler) {
  if (is64bit) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register out_lo = locations->Out().AsRegisterPairLow<Register>();
    Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

    // The comments in this section show the analogous operations which would
    // be performed if we had 64-bit registers "in", and "out".
    // __ Dsra32(AT, in, 31);
    __ Sra(AT, in_hi, 31);
    // __ Xor(out, in, AT);
    __ Xor(TMP, in_lo, AT);
    __ Xor(out_hi, in_hi, AT);
    // __ Dsubu(out, out, AT);
    __ Subu(out_lo, TMP, AT);
    __ Sltu(TMP, out_lo, TMP);
    __ Addu(out_hi, out_hi, TMP);
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();
    Register out = locations->Out().AsRegister<Register>();

    __ Sra(AT, in, 31);
    __ Xor(out, in, AT);
    __ Subu(out, out, AT);
  }
}

// int java.lang.Math.abs(int)
void IntrinsicLocationsBuilderMIPS::VisitMathAbsInt(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAbsInt(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ false, GetAssembler());
}

// long java.lang.Math.abs(long)
void IntrinsicLocationsBuilderMIPS::VisitMathAbsLong(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAbsLong(HInvoke* invoke) {
  GenAbsInteger(invoke->GetLocations(), /* is64bit */ true, GetAssembler());
}

static void GenMinMaxFP(LocationSummary* locations,
                        bool is_min,
                        DataType::Type type,
                        bool is_R6,
                        MipsAssembler* assembler) {
  FRegister out = locations->Out().AsFpuRegister<FRegister>();
  FRegister a = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister b = locations->InAt(1).AsFpuRegister<FRegister>();

  if (is_R6) {
    MipsLabel noNaNs;
    MipsLabel done;
    FRegister ftmp = ((out != a) && (out != b)) ? out : FTMP;

    // When Java computes min/max it prefers a NaN to a number; the
    // behavior of MIPSR6 is to prefer numbers to NaNs, i.e., if one of
    // the inputs is a NaN and the other is a valid number, the MIPS
    // instruction will return the number; Java wants the NaN value
    // returned. This is why there is extra logic preceding the use of
    // the MIPS min.fmt/max.fmt instructions. If either a, or b holds a
    // NaN, return the NaN, otherwise return the min/max.
    if (type == DataType::Type::kFloat64) {
      __ CmpUnD(FTMP, a, b);
      __ Bc1eqz(FTMP, &noNaNs);

      // One of the inputs is a NaN
      __ CmpEqD(ftmp, a, a);
      // If a == a then b is the NaN, otherwise a is the NaN.
      __ SelD(ftmp, a, b);

      if (ftmp != out) {
        __ MovD(out, ftmp);
      }

      __ B(&done);

      __ Bind(&noNaNs);

      if (is_min) {
        __ MinD(out, a, b);
      } else {
        __ MaxD(out, a, b);
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kFloat32);
      __ CmpUnS(FTMP, a, b);
      __ Bc1eqz(FTMP, &noNaNs);

      // One of the inputs is a NaN
      __ CmpEqS(ftmp, a, a);
      // If a == a then b is the NaN, otherwise a is the NaN.
      __ SelS(ftmp, a, b);

      if (ftmp != out) {
        __ MovS(out, ftmp);
      }

      __ B(&done);

      __ Bind(&noNaNs);

      if (is_min) {
        __ MinS(out, a, b);
      } else {
        __ MaxS(out, a, b);
      }
    }

    __ Bind(&done);
  } else {
    MipsLabel ordered;
    MipsLabel compare;
    MipsLabel select;
    MipsLabel done;

    if (type == DataType::Type::kFloat64) {
      __ CunD(a, b);
    } else {
      DCHECK_EQ(type, DataType::Type::kFloat32);
      __ CunS(a, b);
    }
    __ Bc1f(&ordered);

    // a or b (or both) is a NaN. Return one, which is a NaN.
    if (type == DataType::Type::kFloat64) {
      __ CeqD(b, b);
    } else {
      __ CeqS(b, b);
    }
    __ B(&select);

    __ Bind(&ordered);

    // Neither is a NaN.
    // a == b? (-0.0 compares equal with +0.0)
    // If equal, handle zeroes, else compare further.
    if (type == DataType::Type::kFloat64) {
      __ CeqD(a, b);
    } else {
      __ CeqS(a, b);
    }
    __ Bc1f(&compare);

    // a == b either bit for bit or one is -0.0 and the other is +0.0.
    if (type == DataType::Type::kFloat64) {
      __ MoveFromFpuHigh(TMP, a);
      __ MoveFromFpuHigh(AT, b);
    } else {
      __ Mfc1(TMP, a);
      __ Mfc1(AT, b);
    }

    if (is_min) {
      // -0.0 prevails over +0.0.
      __ Or(TMP, TMP, AT);
    } else {
      // +0.0 prevails over -0.0.
      __ And(TMP, TMP, AT);
    }

    if (type == DataType::Type::kFloat64) {
      __ Mfc1(AT, a);
      __ Mtc1(AT, out);
      __ MoveToFpuHigh(TMP, out);
    } else {
      __ Mtc1(TMP, out);
    }
    __ B(&done);

    __ Bind(&compare);

    if (type == DataType::Type::kFloat64) {
      if (is_min) {
        // return (a <= b) ? a : b;
        __ ColeD(a, b);
      } else {
        // return (a >= b) ? a : b;
        __ ColeD(b, a);  // b <= a
      }
    } else {
      if (is_min) {
        // return (a <= b) ? a : b;
        __ ColeS(a, b);
      } else {
        // return (a >= b) ? a : b;
        __ ColeS(b, a);  // b <= a
      }
    }

    __ Bind(&select);

    if (type == DataType::Type::kFloat64) {
      __ MovtD(out, a);
      __ MovfD(out, b);
    } else {
      __ MovtS(out, a);
      __ MovfS(out, b);
    }

    __ Bind(&done);
  }
}

static void CreateFPFPToFPLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->SetInAt(1, Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresFpuRegister(), Location::kOutputOverlap);
}

// double java.lang.Math.min(double, double)
void IntrinsicLocationsBuilderMIPS::VisitMathMinDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMinDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(),
              /* is_min */ true,
              DataType::Type::kFloat64,
              IsR6(),
              GetAssembler());
}

// float java.lang.Math.min(float, float)
void IntrinsicLocationsBuilderMIPS::VisitMathMinFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMinFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(),
              /* is_min */ true,
              DataType::Type::kFloat32,
              IsR6(),
              GetAssembler());
}

// double java.lang.Math.max(double, double)
void IntrinsicLocationsBuilderMIPS::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMaxDoubleDouble(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(),
              /* is_min */ false,
              DataType::Type::kFloat64,
              IsR6(),
              GetAssembler());
}

// float java.lang.Math.max(float, float)
void IntrinsicLocationsBuilderMIPS::VisitMathMaxFloatFloat(HInvoke* invoke) {
  CreateFPFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMaxFloatFloat(HInvoke* invoke) {
  GenMinMaxFP(invoke->GetLocations(),
              /* is_min */ false,
              DataType::Type::kFloat32,
              IsR6(),
              GetAssembler());
}

static void CreateIntIntToIntLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetOut(Location::RequiresRegister(), Location::kNoOutputOverlap);
}

static void GenMinMax(LocationSummary* locations,
                      bool is_min,
                      DataType::Type type,
                      bool is_R6,
                      MipsAssembler* assembler) {
  if (is_R6) {
    // Some architectures, such as ARM and MIPS (prior to r6), have a
    // conditional move instruction which only changes the target
    // (output) register if the condition is true (MIPS prior to r6 had
    // MOVF, MOVT, MOVN, and MOVZ). The SELEQZ and SELNEZ instructions
    // always change the target (output) register.  If the condition is
    // true the output register gets the contents of the "rs" register;
    // otherwise, the output register is set to zero. One consequence
    // of this is that to implement something like "rd = c==0 ? rs : rt"
    // MIPS64r6 needs to use a pair of SELEQZ/SELNEZ instructions.
    // After executing this pair of instructions one of the output
    // registers from the pair will necessarily contain zero. Then the
    // code ORs the output registers from the SELEQZ/SELNEZ instructions
    // to get the final result.
    //
    // The initial test to see if the output register is same as the
    // first input register is needed to make sure that value in the
    // first input register isn't clobbered before we've finished
    // computing the output value. The logic in the corresponding else
    // clause performs the same task but makes sure the second input
    // register isn't clobbered in the event that it's the same register
    // as the output register; the else clause also handles the case
    // where the output register is distinct from both the first, and the
    // second input registers.
    if (type == DataType::Type::kInt64) {
      Register a_lo = locations->InAt(0).AsRegisterPairLow<Register>();
      Register a_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register b_lo = locations->InAt(1).AsRegisterPairLow<Register>();
      Register b_hi = locations->InAt(1).AsRegisterPairHigh<Register>();
      Register out_lo = locations->Out().AsRegisterPairLow<Register>();
      Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

      MipsLabel compare_done;

      if (a_lo == b_lo) {
        if (out_lo != a_lo) {
          __ Move(out_lo, a_lo);
          __ Move(out_hi, a_hi);
        }
      } else {
        __ Slt(TMP, b_hi, a_hi);
        __ Bne(b_hi, a_hi, &compare_done);

        __ Sltu(TMP, b_lo, a_lo);

        __ Bind(&compare_done);

        if (is_min) {
          __ Seleqz(AT, a_lo, TMP);
          __ Selnez(out_lo, b_lo, TMP);  // Safe even if out_lo == a_lo/b_lo
                                         // because at this point we're
                                         // done using a_lo/b_lo.
        } else {
          __ Selnez(AT, a_lo, TMP);
          __ Seleqz(out_lo, b_lo, TMP);  // ditto
        }
        __ Or(out_lo, out_lo, AT);
        if (is_min) {
          __ Seleqz(AT, a_hi, TMP);
          __ Selnez(out_hi, b_hi, TMP);  // ditto but for out_hi & a_hi/b_hi
        } else {
          __ Selnez(AT, a_hi, TMP);
          __ Seleqz(out_hi, b_hi, TMP);  // ditto but for out_hi & a_hi/b_hi
        }
        __ Or(out_hi, out_hi, AT);
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kInt32);
      Register a = locations->InAt(0).AsRegister<Register>();
      Register b = locations->InAt(1).AsRegister<Register>();
      Register out = locations->Out().AsRegister<Register>();

      if (a == b) {
        if (out != a) {
          __ Move(out, a);
        }
      } else {
        __ Slt(AT, b, a);
        if (is_min) {
          __ Seleqz(TMP, a, AT);
          __ Selnez(AT, b, AT);
        } else {
          __ Selnez(TMP, a, AT);
          __ Seleqz(AT, b, AT);
        }
        __ Or(out, TMP, AT);
      }
    }
  } else {
    if (type == DataType::Type::kInt64) {
      Register a_lo = locations->InAt(0).AsRegisterPairLow<Register>();
      Register a_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
      Register b_lo = locations->InAt(1).AsRegisterPairLow<Register>();
      Register b_hi = locations->InAt(1).AsRegisterPairHigh<Register>();
      Register out_lo = locations->Out().AsRegisterPairLow<Register>();
      Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

      MipsLabel compare_done;

      if (a_lo == b_lo) {
        if (out_lo != a_lo) {
          __ Move(out_lo, a_lo);
          __ Move(out_hi, a_hi);
        }
      } else {
        __ Slt(TMP, a_hi, b_hi);
        __ Bne(a_hi, b_hi, &compare_done);

        __ Sltu(TMP, a_lo, b_lo);

        __ Bind(&compare_done);

        if (is_min) {
          if (out_lo != a_lo) {
            __ Movn(out_hi, a_hi, TMP);
            __ Movn(out_lo, a_lo, TMP);
          }
          if (out_lo != b_lo) {
            __ Movz(out_hi, b_hi, TMP);
            __ Movz(out_lo, b_lo, TMP);
          }
        } else {
          if (out_lo != a_lo) {
            __ Movz(out_hi, a_hi, TMP);
            __ Movz(out_lo, a_lo, TMP);
          }
          if (out_lo != b_lo) {
            __ Movn(out_hi, b_hi, TMP);
            __ Movn(out_lo, b_lo, TMP);
          }
        }
      }
    } else {
      DCHECK_EQ(type, DataType::Type::kInt32);
      Register a = locations->InAt(0).AsRegister<Register>();
      Register b = locations->InAt(1).AsRegister<Register>();
      Register out = locations->Out().AsRegister<Register>();

      if (a == b) {
        if (out != a) {
          __ Move(out, a);
        }
      } else {
        __ Slt(AT, a, b);
        if (is_min) {
          if (out != a) {
            __ Movn(out, a, AT);
          }
          if (out != b) {
            __ Movz(out, b, AT);
          }
        } else {
          if (out != a) {
            __ Movz(out, a, AT);
          }
          if (out != b) {
            __ Movn(out, b, AT);
          }
        }
      }
    }
  }
}

// int java.lang.Math.min(int, int)
void IntrinsicLocationsBuilderMIPS::VisitMathMinIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMinIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(),
            /* is_min */ true,
            DataType::Type::kInt32,
            IsR6(),
            GetAssembler());
}

// long java.lang.Math.min(long, long)
void IntrinsicLocationsBuilderMIPS::VisitMathMinLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMinLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(),
            /* is_min */ true,
            DataType::Type::kInt64,
            IsR6(),
            GetAssembler());
}

// int java.lang.Math.max(int, int)
void IntrinsicLocationsBuilderMIPS::VisitMathMaxIntInt(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMaxIntInt(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(),
            /* is_min */ false,
            DataType::Type::kInt32,
            IsR6(),
            GetAssembler());
}

// long java.lang.Math.max(long, long)
void IntrinsicLocationsBuilderMIPS::VisitMathMaxLongLong(HInvoke* invoke) {
  CreateIntIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathMaxLongLong(HInvoke* invoke) {
  GenMinMax(invoke->GetLocations(),
            /* is_min */ false,
            DataType::Type::kInt64,
            IsR6(),
            GetAssembler());
}

// double java.lang.Math.sqrt(double)
void IntrinsicLocationsBuilderMIPS::VisitMathSqrt(HInvoke* invoke) {
  CreateFPToFPLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathSqrt(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MipsAssembler* assembler = GetAssembler();
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister out = locations->Out().AsFpuRegister<FRegister>();

  __ SqrtD(out, in);
}

// byte libcore.io.Memory.peekByte(long address)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPeekByte(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPeekByte(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();

  __ Lb(out, adr, 0);
}

// short libcore.io.Memory.peekShort(long address)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPeekShortNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPeekShortNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();

  if (IsR6()) {
    __ Lh(out, adr, 0);
  } else if (IsR2OrNewer()) {
    // Unlike for words, there are no lhl/lhr instructions to load
    // unaligned halfwords so the code loads individual bytes, in case
    // the address isn't halfword-aligned, and assembles them into a
    // signed halfword.
    __ Lb(AT, adr, 1);   // This byte must be sign-extended.
    __ Lb(out, adr, 0);  // This byte can be either sign-extended, or
                         // zero-extended because the following
                         // instruction overwrites the sign bits.
    __ Ins(out, AT, 8, 24);
  } else {
    __ Lbu(AT, adr, 0);  // This byte must be zero-extended.  If it's not
                         // the "or" instruction below will destroy the upper
                         // 24 bits of the final result.
    __ Lb(out, adr, 1);  // This byte must be sign-extended.
    __ Sll(out, out, 8);
    __ Or(out, out, AT);
  }
}

// int libcore.io.Memory.peekInt(long address)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPeekIntNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke, Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPeekIntNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();

  if (IsR6()) {
    __ Lw(out, adr, 0);
  } else {
    __ Lwr(out, adr, 0);
    __ Lwl(out, adr, 3);
  }
}

// long libcore.io.Memory.peekLong(long address)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPeekLongNative(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke, Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPeekLongNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register out_lo = invoke->GetLocations()->Out().AsRegisterPairLow<Register>();
  Register out_hi = invoke->GetLocations()->Out().AsRegisterPairHigh<Register>();

  if (IsR6()) {
    __ Lw(out_lo, adr, 0);
    __ Lw(out_hi, adr, 4);
  } else {
    __ Lwr(out_lo, adr, 0);
    __ Lwl(out_lo, adr, 3);
    __ Lwr(out_hi, adr, 4);
    __ Lwl(out_hi, adr, 7);
  }
}

static void CreateIntIntToVoidLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RequiresRegister());
}

// void libcore.io.Memory.pokeByte(long address, byte value)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPokeByte(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPokeByte(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register val = invoke->GetLocations()->InAt(1).AsRegister<Register>();

  __ Sb(val, adr, 0);
}

// void libcore.io.Memory.pokeShort(long address, short value)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPokeShortNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPokeShortNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register val = invoke->GetLocations()->InAt(1).AsRegister<Register>();

  if (IsR6()) {
    __ Sh(val, adr, 0);
  } else {
    // Unlike for words, there are no shl/shr instructions to store
    // unaligned halfwords so the code stores individual bytes, in case
    // the address isn't halfword-aligned.
    __ Sb(val, adr, 0);
    __ Srl(AT, val, 8);
    __ Sb(AT, adr, 1);
  }
}

// void libcore.io.Memory.pokeInt(long address, int value)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPokeIntNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPokeIntNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register val = invoke->GetLocations()->InAt(1).AsRegister<Register>();

  if (IsR6()) {
    __ Sw(val, adr, 0);
  } else {
    __ Swr(val, adr, 0);
    __ Swl(val, adr, 3);
  }
}

// void libcore.io.Memory.pokeLong(long address, long value)
void IntrinsicLocationsBuilderMIPS::VisitMemoryPokeLongNative(HInvoke* invoke) {
  CreateIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMemoryPokeLongNative(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register adr = invoke->GetLocations()->InAt(0).AsRegisterPairLow<Register>();
  Register val_lo = invoke->GetLocations()->InAt(1).AsRegisterPairLow<Register>();
  Register val_hi = invoke->GetLocations()->InAt(1).AsRegisterPairHigh<Register>();

  if (IsR6()) {
    __ Sw(val_lo, adr, 0);
    __ Sw(val_hi, adr, 4);
  } else {
    __ Swr(val_lo, adr, 0);
    __ Swl(val_lo, adr, 3);
    __ Swr(val_hi, adr, 4);
    __ Swl(val_hi, adr, 7);
  }
}

// Thread java.lang.Thread.currentThread()
void IntrinsicLocationsBuilderMIPS::VisitThreadCurrentThread(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitThreadCurrentThread(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();

  __ LoadFromOffset(kLoadWord,
                    out,
                    TR,
                    Thread::PeerOffset<kMipsPointerSize>().Int32Value());
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
    // path in InstructionCodeGeneratorMIPS::GenerateReferenceLoadWithBakerReadBarrier.
    locations->AddTemp(Location::RequiresRegister());
  }
}

// Note that the caller must supply a properly aligned memory address.
// If they do not, the behavior is undefined (atomicity not guaranteed, exception may occur).
static void GenUnsafeGet(HInvoke* invoke,
                         DataType::Type type,
                         bool is_volatile,
                         bool is_R6,
                         CodeGeneratorMIPS* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  DCHECK((type == DataType::Type::kInt32) ||
         (type == DataType::Type::kInt64) ||
         (type == DataType::Type::kReference)) << type;
  MipsAssembler* assembler = codegen->GetAssembler();
  // Target register.
  Location trg_loc = locations->Out();
  // Object pointer.
  Location base_loc = locations->InAt(1);
  Register base = base_loc.AsRegister<Register>();
  // The "offset" argument is passed as a "long". Since this code is for
  // a 32-bit processor, we can only use 32-bit addresses, so we only
  // need the low 32-bits of offset.
  Location offset_loc = locations->InAt(2);
  Register offset_lo = offset_loc.AsRegisterPairLow<Register>();

  if (!(kEmitCompilerReadBarrier && kUseBakerReadBarrier && (type == DataType::Type::kReference))) {
    __ Addu(TMP, base, offset_lo);
  }

  switch (type) {
    case DataType::Type::kInt64: {
      Register trg_lo = trg_loc.AsRegisterPairLow<Register>();
      Register trg_hi = trg_loc.AsRegisterPairHigh<Register>();
      CHECK(!is_volatile);  // TODO: support atomic 8-byte volatile loads.
      if (is_R6) {
        __ Lw(trg_lo, TMP, 0);
        __ Lw(trg_hi, TMP, 4);
      } else {
        __ Lwr(trg_lo, TMP, 0);
        __ Lwl(trg_lo, TMP, 3);
        __ Lwr(trg_hi, TMP, 4);
        __ Lwl(trg_hi, TMP, 7);
      }
      break;
    }

    case DataType::Type::kInt32: {
      Register trg = trg_loc.AsRegister<Register>();
      if (is_R6) {
        __ Lw(trg, TMP, 0);
      } else {
        __ Lwr(trg, TMP, 0);
        __ Lwl(trg, TMP, 3);
      }
      if (is_volatile) {
        __ Sync(0);
      }
      break;
    }

    case DataType::Type::kReference: {
      Register trg = trg_loc.AsRegister<Register>();
      if (kEmitCompilerReadBarrier) {
        if (kUseBakerReadBarrier) {
          Location temp = locations->GetTemp(0);
          codegen->GenerateReferenceLoadWithBakerReadBarrier(invoke,
                                                             trg_loc,
                                                             base,
                                                             /* offset */ 0U,
                                                             /* index */ offset_loc,
                                                             TIMES_1,
                                                             temp,
                                                             /* needs_null_check */ false);
          if (is_volatile) {
            __ Sync(0);
          }
        } else {
          if (is_R6) {
            __ Lw(trg, TMP, 0);
          } else {
            __ Lwr(trg, TMP, 0);
            __ Lwl(trg, TMP, 3);
          }
          if (is_volatile) {
            __ Sync(0);
          }
          codegen->GenerateReadBarrierSlow(invoke,
                                           trg_loc,
                                           trg_loc,
                                           base_loc,
                                           /* offset */ 0U,
                                           /* index */ offset_loc);
        }
      } else {
        if (is_R6) {
          __ Lw(trg, TMP, 0);
        } else {
          __ Lwr(trg, TMP, 0);
          __ Lwl(trg, TMP, 3);
        }
        if (is_volatile) {
          __ Sync(0);
        }
        __ MaybeUnpoisonHeapReference(trg);
      }
      break;
    }

    default:
      LOG(FATAL) << "Unexpected type " << type;
      UNREACHABLE();
  }
}

// int sun.misc.Unsafe.getInt(Object o, long offset)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeGet(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt32);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeGet(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ false, IsR6(), codegen_);
}

// int sun.misc.Unsafe.getIntVolatile(Object o, long offset)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeGetVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt32);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeGetVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt32, /* is_volatile */ true, IsR6(), codegen_);
}

// long sun.misc.Unsafe.getLong(Object o, long offset)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeGetLong(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kInt64);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeGetLong(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kInt64, /* is_volatile */ false, IsR6(), codegen_);
}

// Object sun.misc.Unsafe.getObject(Object o, long offset)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeGetObject(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kReference);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeGetObject(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ false, IsR6(), codegen_);
}

// Object sun.misc.Unsafe.getObjectVolatile(Object o, long offset)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntToIntLocations(allocator_, invoke, DataType::Type::kReference);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeGetObjectVolatile(HInvoke* invoke) {
  GenUnsafeGet(invoke, DataType::Type::kReference, /* is_volatile */ true, IsR6(), codegen_);
}

static void CreateIntIntIntIntToVoidLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::NoLocation());        // Unused receiver.
  locations->SetInAt(1, Location::RequiresRegister());
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RequiresRegister());
}

// Note that the caller must supply a properly aligned memory address.
// If they do not, the behavior is undefined (atomicity not guaranteed, exception may occur).
static void GenUnsafePut(LocationSummary* locations,
                         DataType::Type type,
                         bool is_volatile,
                         bool is_ordered,
                         bool is_R6,
                         CodeGeneratorMIPS* codegen) {
  DCHECK((type == DataType::Type::kInt32) ||
         (type == DataType::Type::kInt64) ||
         (type == DataType::Type::kReference)) << type;
  MipsAssembler* assembler = codegen->GetAssembler();
  // Object pointer.
  Register base = locations->InAt(1).AsRegister<Register>();
  // The "offset" argument is passed as a "long", i.e., it's 64-bits in
  // size. Since this code is for a 32-bit processor, we can only use
  // 32-bit addresses, so we only need the low 32-bits of offset.
  Register offset_lo = locations->InAt(2).AsRegisterPairLow<Register>();

  __ Addu(TMP, base, offset_lo);
  if (is_volatile || is_ordered) {
    __ Sync(0);
  }
  if ((type == DataType::Type::kInt32) || (type == DataType::Type::kReference)) {
    Register value = locations->InAt(3).AsRegister<Register>();

    if (kPoisonHeapReferences && type == DataType::Type::kReference) {
      __ PoisonHeapReference(AT, value);
      value = AT;
    }

    if (is_R6) {
      __ Sw(value, TMP, 0);
    } else {
      __ Swr(value, TMP, 0);
      __ Swl(value, TMP, 3);
    }
  } else {
    Register value_lo = locations->InAt(3).AsRegisterPairLow<Register>();
    Register value_hi = locations->InAt(3).AsRegisterPairHigh<Register>();
    CHECK(!is_volatile);  // TODO: support atomic 8-byte volatile stores.
    if (is_R6) {
      __ Sw(value_lo, TMP, 0);
      __ Sw(value_hi, TMP, 4);
    } else {
      __ Swr(value_lo, TMP, 0);
      __ Swl(value_lo, TMP, 3);
      __ Swr(value_hi, TMP, 4);
      __ Swl(value_hi, TMP, 7);
    }
  }

  if (is_volatile) {
    __ Sync(0);
  }

  if (type == DataType::Type::kReference) {
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(base, locations->InAt(3).AsRegister<Register>(), value_can_be_null);
  }
}

// void sun.misc.Unsafe.putInt(Object o, long offset, int x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePut(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePut(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ false,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putOrderedInt(Object o, long offset, int x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ false,
               /* is_ordered */ true,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putIntVolatile(Object o, long offset, int x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt32,
               /* is_volatile */ true,
               /* is_ordered */ false,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putObject(Object o, long offset, Object x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutObject(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutObject(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ false,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putOrderedObject(Object o, long offset, Object x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutObjectOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ false,
               /* is_ordered */ true,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putObjectVolatile(Object o, long offset, Object x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutObjectVolatile(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kReference,
               /* is_volatile */ true,
               /* is_ordered */ false,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putLong(Object o, long offset, long x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutLong(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutLong(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ false,
               IsR6(),
               codegen_);
}

// void sun.misc.Unsafe.putOrderedLong(Object o, long offset, long x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  CreateIntIntIntIntToVoidLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafePutLongOrdered(HInvoke* invoke) {
  GenUnsafePut(invoke->GetLocations(),
               DataType::Type::kInt64,
               /* is_volatile */ false,
               /* is_ordered */ true,
               IsR6(),
               codegen_);
}

static void CreateIntIntIntIntIntToIntPlusTemps(ArenaAllocator* allocator, HInvoke* invoke) {
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
  locations->SetOut(Location::RequiresRegister());

  // Temporary register used in CAS by (Baker) read barrier.
  if (can_call) {
    locations->AddTemp(Location::RequiresRegister());
  }
}

// Note that the caller must supply a properly aligned memory address.
// If they do not, the behavior is undefined (atomicity not guaranteed, exception may occur).
static void GenCas(HInvoke* invoke, DataType::Type type, CodeGeneratorMIPS* codegen) {
  MipsAssembler* assembler = codegen->GetAssembler();
  LocationSummary* locations = invoke->GetLocations();
  bool isR6 = codegen->GetInstructionSetFeatures().IsR6();
  Register base = locations->InAt(1).AsRegister<Register>();
  Location offset_loc = locations->InAt(2);
  Register offset_lo = offset_loc.AsRegisterPairLow<Register>();
  Register expected = locations->InAt(3).AsRegister<Register>();
  Register value = locations->InAt(4).AsRegister<Register>();
  Location out_loc = locations->Out();
  Register out = out_loc.AsRegister<Register>();

  DCHECK_NE(base, out);
  DCHECK_NE(offset_lo, out);
  DCHECK_NE(expected, out);

  if (type == DataType::Type::kReference) {
    // The only read barrier implementation supporting the
    // UnsafeCASObject intrinsic is the Baker-style read barriers.
    DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

    // Mark card for object assuming new value is stored. Worst case we will mark an unchanged
    // object and scan the receiver at the next GC for nothing.
    bool value_can_be_null = true;  // TODO: Worth finding out this information?
    codegen->MarkGCCard(base, value, value_can_be_null);

    if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
      Location temp = locations->GetTemp(0);
      // Need to make sure the reference stored in the field is a to-space
      // one before attempting the CAS or the CAS could fail incorrectly.
      codegen->GenerateReferenceLoadWithBakerReadBarrier(
          invoke,
          out_loc,  // Unused, used only as a "temporary" within the read barrier.
          base,
          /* offset */ 0u,
          /* index */ offset_loc,
          ScaleFactor::TIMES_1,
          temp,
          /* needs_null_check */ false,
          /* always_update_field */ true);
    }
  }

  MipsLabel loop_head, exit_loop;
  __ Addu(TMP, base, offset_lo);

  if (kPoisonHeapReferences && type == DataType::Type::kReference) {
    __ PoisonHeapReference(expected);
    // Do not poison `value`, if it is the same register as
    // `expected`, which has just been poisoned.
    if (value != expected) {
      __ PoisonHeapReference(value);
    }
  }

  // do {
  //   tmp_value = [tmp_ptr] - expected;
  // } while (tmp_value == 0 && failure([tmp_ptr] <- r_new_value));
  // result = tmp_value != 0;

  __ Sync(0);
  __ Bind(&loop_head);
  if ((type == DataType::Type::kInt32) || (type == DataType::Type::kReference)) {
    if (isR6) {
      __ LlR6(out, TMP);
    } else {
      __ LlR2(out, TMP);
    }
  } else {
    LOG(FATAL) << "Unsupported op size " << type;
    UNREACHABLE();
  }
  __ Subu(out, out, expected);          // If we didn't get the 'expected'
  __ Sltiu(out, out, 1);                // value, set 'out' to false, and
  __ Beqz(out, &exit_loop);             // return.
  __ Move(out, value);  // Use 'out' for the 'store conditional' instruction.
                        // If we use 'value' directly, we would lose 'value'
                        // in the case that the store fails.  Whether the
                        // store succeeds, or fails, it will load the
                        // correct Boolean value into the 'out' register.
  // This test isn't really necessary. We only support DataType::Type::kInt,
  // DataType::Type::kReference, and we already verified that we're working on one
  // of those two types. It's left here in case the code needs to support
  // other types in the future.
  if ((type == DataType::Type::kInt32) || (type == DataType::Type::kReference)) {
    if (isR6) {
      __ ScR6(out, TMP);
    } else {
      __ ScR2(out, TMP);
    }
  }
  __ Beqz(out, &loop_head);     // If we couldn't do the read-modify-write
                                // cycle atomically then retry.
  __ Bind(&exit_loop);
  __ Sync(0);

  if (kPoisonHeapReferences && type == DataType::Type::kReference) {
    __ UnpoisonHeapReference(expected);
    // Do not unpoison `value`, if it is the same register as
    // `expected`, which has just been unpoisoned.
    if (value != expected) {
      __ UnpoisonHeapReference(value);
    }
  }
}

// boolean sun.misc.Unsafe.compareAndSwapInt(Object o, long offset, int expected, int x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeCASInt(HInvoke* invoke) {
  CreateIntIntIntIntIntToIntPlusTemps(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeCASInt(HInvoke* invoke) {
  GenCas(invoke, DataType::Type::kInt32, codegen_);
}

// boolean sun.misc.Unsafe.compareAndSwapObject(Object o, long offset, Object expected, Object x)
void IntrinsicLocationsBuilderMIPS::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  if (kEmitCompilerReadBarrier && !kUseBakerReadBarrier) {
    return;
  }

  CreateIntIntIntIntIntToIntPlusTemps(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitUnsafeCASObject(HInvoke* invoke) {
  // The only read barrier implementation supporting the
  // UnsafeCASObject intrinsic is the Baker-style read barriers.
  DCHECK(!kEmitCompilerReadBarrier || kUseBakerReadBarrier);

  GenCas(invoke, DataType::Type::kReference, codegen_);
}

// int java.lang.String.compareTo(String anotherString)
void IntrinsicLocationsBuilderMIPS::VisitStringCompareTo(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));
}

void IntrinsicCodeGeneratorMIPS::VisitStringCompareTo(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  Register argument = locations->InAt(1).AsRegister<Register>();
  SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
  codegen_->AddSlowPath(slow_path);
  __ Beqz(argument, slow_path->GetEntryLabel());
  codegen_->InvokeRuntime(kQuickStringCompareTo, invoke, invoke->GetDexPc(), slow_path);
  __ Bind(slow_path->GetExitLabel());
}

// boolean java.lang.String.equals(Object anObject)
void IntrinsicLocationsBuilderMIPS::VisitStringEquals(HInvoke* invoke) {
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
  locations->SetOut(Location::RequiresRegister());

  // Temporary registers to store lengths of strings and for calculations.
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitStringEquals(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register str = locations->InAt(0).AsRegister<Register>();
  Register arg = locations->InAt(1).AsRegister<Register>();
  Register out = locations->Out().AsRegister<Register>();

  Register temp1 = locations->GetTemp(0).AsRegister<Register>();
  Register temp2 = locations->GetTemp(1).AsRegister<Register>();
  Register temp3 = locations->GetTemp(2).AsRegister<Register>();

  MipsLabel loop;
  MipsLabel end;
  MipsLabel return_true;
  MipsLabel return_false;

  // Get offsets of count, value, and class fields within a string object.
  const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
  const uint32_t value_offset = mirror::String::ValueOffset().Uint32Value();
  const uint32_t class_offset = mirror::Object::ClassOffset().Uint32Value();

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // If the register containing the pointer to "this", and the register
  // containing the pointer to "anObject" are the same register then
  // "this", and "anObject" are the same object and we can
  // short-circuit the logic to a true result.
  if (str == arg) {
    __ LoadConst32(out, 1);
    return;
  }
  StringEqualsOptimizations optimizations(invoke);
  if (!optimizations.GetArgumentNotNull()) {
    // Check if input is null, return false if it is.
    __ Beqz(arg, &return_false);
  }

  // Reference equality check, return true if same reference.
  __ Beq(str, arg, &return_true);

  if (!optimizations.GetArgumentIsString()) {
    // Instanceof check for the argument by comparing class fields.
    // All string objects must have the same type since String cannot be subclassed.
    // Receiver must be a string object, so its class field is equal to all strings' class fields.
    // If the argument is a string object, its class field must be equal to receiver's class field.
    __ Lw(temp1, str, class_offset);
    __ Lw(temp2, arg, class_offset);
    __ Bne(temp1, temp2, &return_false);
  }

  // Load `count` fields of this and argument strings.
  __ Lw(temp1, str, count_offset);
  __ Lw(temp2, arg, count_offset);
  // Check if `count` fields are equal, return false if they're not.
  // Also compares the compression style, if differs return false.
  __ Bne(temp1, temp2, &return_false);
  // Return true if both strings are empty. Even with string compression `count == 0` means empty.
  static_assert(static_cast<uint32_t>(mirror::StringCompressionFlag::kCompressed) == 0u,
                "Expecting 0=compressed, 1=uncompressed");
  __ Beqz(temp1, &return_true);

  // Don't overwrite input registers
  __ Move(TMP, str);
  __ Move(temp3, arg);

  // Assertions that must hold in order to compare strings 4 bytes at a time.
  DCHECK_ALIGNED(value_offset, 4);
  static_assert(IsAligned<4>(kObjectAlignment), "String of odd length is not zero padded");

  // For string compression, calculate the number of bytes to compare (not chars).
  if (mirror::kUseStringCompression) {
    // Extract compression flag.
    if (IsR2OrNewer()) {
      __ Ext(temp2, temp1, 0, 1);
    } else {
      __ Sll(temp2, temp1, 31);
      __ Srl(temp2, temp2, 31);
    }
    __ Srl(temp1, temp1, 1);             // Extract length.
    __ Sllv(temp1, temp1, temp2);        // Double the byte count if uncompressed.
  }

  // Loop to compare strings 4 bytes at a time starting at the beginning of the string.
  // Ok to do this because strings are zero-padded to kObjectAlignment.
  __ Bind(&loop);
  __ Lw(out, TMP, value_offset);
  __ Lw(temp2, temp3, value_offset);
  __ Bne(out, temp2, &return_false);
  __ Addiu(TMP, TMP, 4);
  __ Addiu(temp3, temp3, 4);
  // With string compression, we have compared 4 bytes, otherwise 2 chars.
  __ Addiu(temp1, temp1, mirror::kUseStringCompression ? -4 : -2);
  __ Bgtz(temp1, &loop);

  // Return true and exit the function.
  // If loop does not result in returning false, we return true.
  __ Bind(&return_true);
  __ LoadConst32(out, 1);
  __ B(&end);

  // Return false and exit the function.
  __ Bind(&return_false);
  __ LoadConst32(out, 0);
  __ Bind(&end);
}

static void GenerateStringIndexOf(HInvoke* invoke,
                                  bool start_at_zero,
                                  MipsAssembler* assembler,
                                  CodeGeneratorMIPS* codegen) {
  LocationSummary* locations = invoke->GetLocations();
  Register tmp_reg = start_at_zero ? locations->GetTemp(0).AsRegister<Register>() : TMP;

  // Note that the null check must have been done earlier.
  DCHECK(!invoke->CanDoImplicitNullCheckOn(invoke->InputAt(0)));

  // Check for code points > 0xFFFF. Either a slow-path check when we don't know statically,
  // or directly dispatch for a large constant, or omit slow-path for a small constant or a char.
  SlowPathCodeMIPS* slow_path = nullptr;
  HInstruction* code_point = invoke->InputAt(1);
  if (code_point->IsIntConstant()) {
    if (!IsUint<16>(code_point->AsIntConstant()->GetValue())) {
      // Always needs the slow-path. We could directly dispatch to it,
      // but this case should be rare, so for simplicity just put the
      // full slow-path down and branch unconditionally.
      slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
      codegen->AddSlowPath(slow_path);
      __ B(slow_path->GetEntryLabel());
      __ Bind(slow_path->GetExitLabel());
      return;
    }
  } else if (code_point->GetType() != DataType::Type::kUint16) {
    Register char_reg = locations->InAt(1).AsRegister<Register>();
    // The "bltu" conditional branch tests to see if the character value
    // fits in a valid 16-bit (MIPS halfword) value. If it doesn't then
    // the character being searched for, if it exists in the string, is
    // encoded using UTF-16 and stored in the string as two (16-bit)
    // halfwords. Currently the assembly code used to implement this
    // intrinsic doesn't support searching for a character stored as
    // two halfwords so we fallback to using the generic implementation
    // of indexOf().
    __ LoadConst32(tmp_reg, std::numeric_limits<uint16_t>::max());
    slow_path = new (codegen->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
    codegen->AddSlowPath(slow_path);
    __ Bltu(tmp_reg, char_reg, slow_path->GetEntryLabel());
  }

  if (start_at_zero) {
    DCHECK_EQ(tmp_reg, A2);
    // Start-index = 0.
    __ Clear(tmp_reg);
  }

  codegen->InvokeRuntime(kQuickIndexOf, invoke, invoke->GetDexPc(), slow_path);
  if (slow_path != nullptr) {
    __ Bind(slow_path->GetExitLabel());
  }
}

// int java.lang.String.indexOf(int ch)
void IntrinsicLocationsBuilderMIPS::VisitStringIndexOf(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime
  // calling convention. So it's best to align the inputs accordingly.
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));

  // Need a temp for slow-path codepoint compare, and need to send start-index=0.
  locations->AddTemp(Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
}

void IntrinsicCodeGeneratorMIPS::VisitStringIndexOf(HInvoke* invoke) {
  GenerateStringIndexOf(invoke, /* start_at_zero */ true, GetAssembler(), codegen_);
}

// int java.lang.String.indexOf(int ch, int fromIndex)
void IntrinsicLocationsBuilderMIPS::VisitStringIndexOfAfter(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  // We have a hand-crafted assembly stub that follows the runtime
  // calling convention. So it's best to align the inputs accordingly.
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));

  // Need a temp for slow-path codepoint compare.
  locations->AddTemp(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitStringIndexOfAfter(HInvoke* invoke) {
  GenerateStringIndexOf(invoke, /* start_at_zero */ false, GetAssembler(), codegen_);
}

// java.lang.StringFactory.newStringFromBytes(byte[] data, int high, int offset, int byteCount)
void IntrinsicLocationsBuilderMIPS::VisitStringNewStringFromBytes(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  locations->SetInAt(3, Location::RegisterLocation(calling_convention.GetRegisterAt(3)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));
}

void IntrinsicCodeGeneratorMIPS::VisitStringNewStringFromBytes(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register byte_array = locations->InAt(0).AsRegister<Register>();
  SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
  codegen_->AddSlowPath(slow_path);
  __ Beqz(byte_array, slow_path->GetEntryLabel());
  codegen_->InvokeRuntime(kQuickAllocStringFromBytes, invoke, invoke->GetDexPc(), slow_path);
  __ Bind(slow_path->GetExitLabel());
}

// java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
void IntrinsicLocationsBuilderMIPS::VisitStringNewStringFromChars(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  locations->SetInAt(1, Location::RegisterLocation(calling_convention.GetRegisterAt(1)));
  locations->SetInAt(2, Location::RegisterLocation(calling_convention.GetRegisterAt(2)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));
}

void IntrinsicCodeGeneratorMIPS::VisitStringNewStringFromChars(HInvoke* invoke) {
  // No need to emit code checking whether `locations->InAt(2)` is a null
  // pointer, as callers of the native method
  //
  //   java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
  //
  // all include a null check on `data` before calling that method.
  codegen_->InvokeRuntime(kQuickAllocStringFromChars, invoke, invoke->GetDexPc());
}

// java.lang.StringFactory.newStringFromString(String toCopy)
void IntrinsicLocationsBuilderMIPS::VisitStringNewStringFromString(HInvoke* invoke) {
  LocationSummary* locations = new (allocator_) LocationSummary(
      invoke, LocationSummary::kCallOnMainAndSlowPath, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;
  locations->SetInAt(0, Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
  Location outLocation = calling_convention.GetReturnLocation(DataType::Type::kInt32);
  locations->SetOut(Location::RegisterLocation(outLocation.AsRegister<Register>()));
}

void IntrinsicCodeGeneratorMIPS::VisitStringNewStringFromString(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register string_to_copy = locations->InAt(0).AsRegister<Register>();
  SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
  codegen_->AddSlowPath(slow_path);
  __ Beqz(string_to_copy, slow_path->GetEntryLabel());
  codegen_->InvokeRuntime(kQuickAllocStringFromString, invoke, invoke->GetDexPc());
  __ Bind(slow_path->GetExitLabel());
}

static void GenIsInfinite(LocationSummary* locations,
                          const DataType::Type type,
                          const bool isR6,
                          MipsAssembler* assembler) {
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
  Register out = locations->Out().AsRegister<Register>();

  DCHECK(type == DataType::Type::kFloat32 || type == DataType::Type::kFloat64);

  if (isR6) {
    if (type == DataType::Type::kFloat64) {
        __ ClassD(FTMP, in);
    } else {
        __ ClassS(FTMP, in);
    }
    __ Mfc1(out, FTMP);
    __ Andi(out, out, kPositiveInfinity | kNegativeInfinity);
    __ Sltu(out, ZERO, out);
  } else {
    // If one, or more, of the exponent bits is zero, then the number can't be infinite.
    if (type == DataType::Type::kFloat64) {
      __ MoveFromFpuHigh(TMP, in);
      __ LoadConst32(AT, High32Bits(kPositiveInfinityDouble));
    } else {
      __ Mfc1(TMP, in);
      __ LoadConst32(AT, kPositiveInfinityFloat);
    }
    __ Xor(TMP, TMP, AT);

    __ Sll(TMP, TMP, 1);

    if (type == DataType::Type::kFloat64) {
      __ Mfc1(AT, in);
      __ Or(TMP, TMP, AT);
    }
    // If any of the significand bits are one, then the number is not infinite.
    __ Sltiu(out, TMP, 1);
  }
}

// boolean java.lang.Float.isInfinite(float)
void IntrinsicLocationsBuilderMIPS::VisitFloatIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitFloatIsInfinite(HInvoke* invoke) {
  GenIsInfinite(invoke->GetLocations(), DataType::Type::kFloat32, IsR6(), GetAssembler());
}

// boolean java.lang.Double.isInfinite(double)
void IntrinsicLocationsBuilderMIPS::VisitDoubleIsInfinite(HInvoke* invoke) {
  CreateFPToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitDoubleIsInfinite(HInvoke* invoke) {
  GenIsInfinite(invoke->GetLocations(), DataType::Type::kFloat64, IsR6(), GetAssembler());
}

static void GenHighestOneBit(LocationSummary* locations,
                             const DataType::Type type,
                             bool isR6,
                             MipsAssembler* assembler) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  if (type == DataType::Type::kInt64) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register out_lo = locations->Out().AsRegisterPairLow<Register>();
    Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

    if (isR6) {
      __ ClzR6(TMP, in_hi);
    } else {
      __ ClzR2(TMP, in_hi);
    }
    __ LoadConst32(AT, 0x80000000);
    __ Srlv(out_hi, AT, TMP);
    __ And(out_hi, out_hi, in_hi);
    if (isR6) {
      __ ClzR6(TMP, in_lo);
    } else {
      __ ClzR2(TMP, in_lo);
    }
    __ Srlv(out_lo, AT, TMP);
    __ And(out_lo, out_lo, in_lo);
    if (isR6) {
      __ Seleqz(out_lo, out_lo, out_hi);
    } else {
      __ Movn(out_lo, ZERO, out_hi);
    }
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();
    Register out = locations->Out().AsRegister<Register>();

    if (isR6) {
      __ ClzR6(TMP, in);
    } else {
      __ ClzR2(TMP, in);
    }
    __ LoadConst32(AT, 0x80000000);
    __ Srlv(AT, AT, TMP);  // Srlv shifts in the range of [0;31] bits (lower 5 bits of arg).
    __ And(out, AT, in);   // So this is required for 0 (=shift by 32).
  }
}

// int java.lang.Integer.highestOneBit(int)
void IntrinsicLocationsBuilderMIPS::VisitIntegerHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke->GetLocations(), DataType::Type::kInt32, IsR6(), GetAssembler());
}

// long java.lang.Long.highestOneBit(long)
void IntrinsicLocationsBuilderMIPS::VisitLongHighestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke, Location::kOutputOverlap);
}

void IntrinsicCodeGeneratorMIPS::VisitLongHighestOneBit(HInvoke* invoke) {
  GenHighestOneBit(invoke->GetLocations(), DataType::Type::kInt64, IsR6(), GetAssembler());
}

static void GenLowestOneBit(LocationSummary* locations,
                            const DataType::Type type,
                            bool isR6,
                            MipsAssembler* assembler) {
  DCHECK(type == DataType::Type::kInt32 || type == DataType::Type::kInt64);

  if (type == DataType::Type::kInt64) {
    Register in_lo = locations->InAt(0).AsRegisterPairLow<Register>();
    Register in_hi = locations->InAt(0).AsRegisterPairHigh<Register>();
    Register out_lo = locations->Out().AsRegisterPairLow<Register>();
    Register out_hi = locations->Out().AsRegisterPairHigh<Register>();

    __ Subu(TMP, ZERO, in_lo);
    __ And(out_lo, TMP, in_lo);
    __ Subu(TMP, ZERO, in_hi);
    __ And(out_hi, TMP, in_hi);
    if (isR6) {
      __ Seleqz(out_hi, out_hi, out_lo);
    } else {
      __ Movn(out_hi, ZERO, out_lo);
    }
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();
    Register out = locations->Out().AsRegister<Register>();

    __ Subu(TMP, ZERO, in);
    __ And(out, TMP, in);
  }
}

// int java.lang.Integer.lowestOneBit(int)
void IntrinsicLocationsBuilderMIPS::VisitIntegerLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke->GetLocations(), DataType::Type::kInt32, IsR6(), GetAssembler());
}

// long java.lang.Long.lowestOneBit(long)
void IntrinsicLocationsBuilderMIPS::VisitLongLowestOneBit(HInvoke* invoke) {
  CreateIntToIntLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitLongLowestOneBit(HInvoke* invoke) {
  GenLowestOneBit(invoke->GetLocations(), DataType::Type::kInt64, IsR6(), GetAssembler());
}

// int java.lang.Math.round(float)
void IntrinsicLocationsBuilderMIPS::VisitMathRoundFloat(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::RequiresFpuRegister());
  locations->AddTemp(Location::RequiresFpuRegister());
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitMathRoundFloat(HInvoke* invoke) {
  LocationSummary* locations = invoke->GetLocations();
  MipsAssembler* assembler = GetAssembler();
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
  FRegister half = locations->GetTemp(0).AsFpuRegister<FRegister>();
  Register out = locations->Out().AsRegister<Register>();

  MipsLabel done;

  if (IsR6()) {
    // out = floor(in);
    //
    // if (out != MAX_VALUE && out != MIN_VALUE) {
    //     TMP = ((in - out) >= 0.5) ? 1 : 0;
    //     return out += TMP;
    // }
    // return out;

    // out = floor(in);
    __ FloorWS(FTMP, in);
    __ Mfc1(out, FTMP);

    // if (out != MAX_VALUE && out != MIN_VALUE)
    __ Addiu(TMP, out, 1);
    __ Aui(TMP, TMP, 0x8000);  // TMP = out + 0x8000 0001
                               // or    out - 0x7FFF FFFF.
                               // IOW, TMP = 1 if out = Int.MIN_VALUE
                               // or   TMP = 0 if out = Int.MAX_VALUE.
    __ Srl(TMP, TMP, 1);       // TMP = 0 if out = Int.MIN_VALUE
                               //         or out = Int.MAX_VALUE.
    __ Beqz(TMP, &done);

    // TMP = (0.5f <= (in - out)) ? -1 : 0;
    __ Cvtsw(FTMP, FTMP);      // Convert output of floor.w.s back to "float".
    __ LoadConst32(AT, bit_cast<int32_t, float>(0.5f));
    __ SubS(FTMP, in, FTMP);
    __ Mtc1(AT, half);

    __ CmpLeS(FTMP, half, FTMP);
    __ Mfc1(TMP, FTMP);

    // Return out -= TMP.
    __ Subu(out, out, TMP);
  } else {
    // if (in.isNaN) {
    //   return 0;
    // }
    //
    // out = floor.w.s(in);
    //
    // /*
    //  * This "if" statement is only needed for the pre-R6 version of floor.w.s
    //  * which outputs Integer.MAX_VALUE for negative numbers with magnitudes
    //  * too large to fit in a 32-bit integer.
    //  */
    // if (out == Integer.MAX_VALUE) {
    //   TMP = (in < 0.0f) ? 1 : 0;
    //   /*
    //    * If TMP is 1, then adding it to out will wrap its value from
    //    * Integer.MAX_VALUE to Integer.MIN_VALUE.
    //    */
    //   return out += TMP;
    // }
    //
    // /*
    //  * For negative values not handled by the previous "if" statement the
    //  * test here will correctly set the value of TMP.
    //  */
    // TMP = ((in - out) >= 0.5f) ? 1 : 0;
    // return out += TMP;

    MipsLabel finite;
    MipsLabel add;

    // Test for NaN.
    __ CunS(in, in);

    // Return zero for NaN.
    __ Move(out, ZERO);
    __ Bc1t(&done);

    // out = floor(in);
    __ FloorWS(FTMP, in);
    __ Mfc1(out, FTMP);

    __ LoadConst32(TMP, -1);

    // TMP = (out = java.lang.Integer.MAX_VALUE) ? -1 : 0;
    __ LoadConst32(AT, std::numeric_limits<int32_t>::max());
    __ Bne(AT, out, &finite);

    __ Mtc1(ZERO, FTMP);
    __ ColtS(in, FTMP);

    __ B(&add);

    __ Bind(&finite);

    // TMP = (0.5f <= (in - out)) ? -1 : 0;
    __ Cvtsw(FTMP, FTMP);  // Convert output of floor.w.s back to "float".
    __ LoadConst32(AT, bit_cast<int32_t, float>(0.5f));
    __ SubS(FTMP, in, FTMP);
    __ Mtc1(AT, half);
    __ ColeS(half, FTMP);

    __ Bind(&add);

    __ Movf(TMP, ZERO);

    // Return out -= TMP.
    __ Subu(out, out, TMP);
  }
  __ Bind(&done);
}

// void java.lang.String.getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin)
void IntrinsicLocationsBuilderMIPS::VisitStringGetCharsNoCheck(HInvoke* invoke) {
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

void IntrinsicCodeGeneratorMIPS::VisitStringGetCharsNoCheck(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  // Check assumption that sizeof(Char) is 2 (used in scaling below).
  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);
  const size_t char_shift = DataType::SizeShift(DataType::Type::kUint16);

  Register srcObj = locations->InAt(0).AsRegister<Register>();
  Register srcBegin = locations->InAt(1).AsRegister<Register>();
  Register srcEnd = locations->InAt(2).AsRegister<Register>();
  Register dstObj = locations->InAt(3).AsRegister<Register>();
  Register dstBegin = locations->InAt(4).AsRegister<Register>();

  Register dstPtr = locations->GetTemp(0).AsRegister<Register>();
  Register srcPtr = locations->GetTemp(1).AsRegister<Register>();
  Register numChrs = locations->GetTemp(2).AsRegister<Register>();

  MipsLabel done;
  MipsLabel loop;

  // Location of data in char array buffer.
  const uint32_t data_offset = mirror::Array::DataOffset(char_size).Uint32Value();

  // Get offset of value field within a string object.
  const int32_t value_offset = mirror::String::ValueOffset().Int32Value();

  __ Beq(srcEnd, srcBegin, &done);  // No characters to move.

  // Calculate number of characters to be copied.
  __ Subu(numChrs, srcEnd, srcBegin);

  // Calculate destination address.
  __ Addiu(dstPtr, dstObj, data_offset);
  __ ShiftAndAdd(dstPtr, dstBegin, dstPtr, char_shift);

  if (mirror::kUseStringCompression) {
    MipsLabel uncompressed_copy, compressed_loop;
    const uint32_t count_offset = mirror::String::CountOffset().Uint32Value();
    // Load count field and extract compression flag.
    __ LoadFromOffset(kLoadWord, TMP, srcObj, count_offset);
    __ Sll(TMP, TMP, 31);

    // If string is uncompressed, use uncompressed path.
    __ Bnez(TMP, &uncompressed_copy);

    // Copy loop for compressed src, copying 1 character (8-bit) to (16-bit) at a time.
    __ Addu(srcPtr, srcObj, srcBegin);
    __ Bind(&compressed_loop);
    __ LoadFromOffset(kLoadUnsignedByte, TMP, srcPtr, value_offset);
    __ StoreToOffset(kStoreHalfword, TMP, dstPtr, 0);
    __ Addiu(numChrs, numChrs, -1);
    __ Addiu(srcPtr, srcPtr, 1);
    __ Addiu(dstPtr, dstPtr, 2);
    __ Bnez(numChrs, &compressed_loop);

    __ B(&done);
    __ Bind(&uncompressed_copy);
  }

  // Calculate source address.
  __ Addiu(srcPtr, srcObj, value_offset);
  __ ShiftAndAdd(srcPtr, srcBegin, srcPtr, char_shift);

  __ Bind(&loop);
  __ Lh(AT, srcPtr, 0);
  __ Addiu(numChrs, numChrs, -1);
  __ Addiu(srcPtr, srcPtr, char_size);
  __ Sh(AT, dstPtr, 0);
  __ Addiu(dstPtr, dstPtr, char_size);
  __ Bnez(numChrs, &loop);

  __ Bind(&done);
}

static void CreateFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;

  locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kFloat64));
}

static void CreateFPFPToFPCallLocations(ArenaAllocator* allocator, HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator) LocationSummary(invoke, LocationSummary::kCallOnMainOnly, kIntrinsified);
  InvokeRuntimeCallingConvention calling_convention;

  locations->SetInAt(0, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(0)));
  locations->SetInAt(1, Location::FpuRegisterLocation(calling_convention.GetFpuRegisterAt(1)));
  locations->SetOut(calling_convention.GetReturnLocation(DataType::Type::kFloat64));
}

static void GenFPToFPCall(HInvoke* invoke, CodeGeneratorMIPS* codegen, QuickEntrypointEnum entry) {
  LocationSummary* locations = invoke->GetLocations();
  FRegister in = locations->InAt(0).AsFpuRegister<FRegister>();
  DCHECK_EQ(in, F12);
  FRegister out = locations->Out().AsFpuRegister<FRegister>();
  DCHECK_EQ(out, F0);

  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());
}

static void GenFPFPToFPCall(HInvoke* invoke,
                            CodeGeneratorMIPS* codegen,
                            QuickEntrypointEnum entry) {
  LocationSummary* locations = invoke->GetLocations();
  FRegister in0 = locations->InAt(0).AsFpuRegister<FRegister>();
  DCHECK_EQ(in0, F12);
  FRegister in1 = locations->InAt(1).AsFpuRegister<FRegister>();
  DCHECK_EQ(in1, F14);
  FRegister out = locations->Out().AsFpuRegister<FRegister>();
  DCHECK_EQ(out, F0);

  codegen->InvokeRuntime(entry, invoke, invoke->GetDexPc());
}

// static double java.lang.Math.cos(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathCos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathCos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCos);
}

// static double java.lang.Math.sin(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathSin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathSin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSin);
}

// static double java.lang.Math.acos(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathAcos(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAcos(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAcos);
}

// static double java.lang.Math.asin(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathAsin(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAsin(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAsin);
}

// static double java.lang.Math.atan(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathAtan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAtan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickAtan);
}

// static double java.lang.Math.atan2(double y, double x)
void IntrinsicLocationsBuilderMIPS::VisitMathAtan2(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathAtan2(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, codegen_, kQuickAtan2);
}

// static double java.lang.Math.pow(double y, double x)
void IntrinsicLocationsBuilderMIPS::VisitMathPow(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathPow(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, codegen_, kQuickPow);
}

// static double java.lang.Math.cbrt(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathCbrt(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathCbrt(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCbrt);
}

// static double java.lang.Math.cosh(double x)
void IntrinsicLocationsBuilderMIPS::VisitMathCosh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathCosh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickCosh);
}

// static double java.lang.Math.exp(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathExp(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathExp(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExp);
}

// static double java.lang.Math.expm1(double x)
void IntrinsicLocationsBuilderMIPS::VisitMathExpm1(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathExpm1(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickExpm1);
}

// static double java.lang.Math.hypot(double x, double y)
void IntrinsicLocationsBuilderMIPS::VisitMathHypot(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathHypot(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, codegen_, kQuickHypot);
}

// static double java.lang.Math.log(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathLog(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathLog(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog);
}

// static double java.lang.Math.log10(double x)
void IntrinsicLocationsBuilderMIPS::VisitMathLog10(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathLog10(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickLog10);
}

// static double java.lang.Math.nextAfter(double start, double direction)
void IntrinsicLocationsBuilderMIPS::VisitMathNextAfter(HInvoke* invoke) {
  CreateFPFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathNextAfter(HInvoke* invoke) {
  GenFPFPToFPCall(invoke, codegen_, kQuickNextAfter);
}

// static double java.lang.Math.sinh(double x)
void IntrinsicLocationsBuilderMIPS::VisitMathSinh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathSinh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickSinh);
}

// static double java.lang.Math.tan(double a)
void IntrinsicLocationsBuilderMIPS::VisitMathTan(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathTan(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTan);
}

// static double java.lang.Math.tanh(double x)
void IntrinsicLocationsBuilderMIPS::VisitMathTanh(HInvoke* invoke) {
  CreateFPToFPCallLocations(allocator_, invoke);
}

void IntrinsicCodeGeneratorMIPS::VisitMathTanh(HInvoke* invoke) {
  GenFPToFPCall(invoke, codegen_, kQuickTanh);
}

// static void java.lang.System.arraycopy(Object src, int srcPos,
//                                        Object dest, int destPos,
//                                        int length)
void IntrinsicLocationsBuilderMIPS::VisitSystemArrayCopyChar(HInvoke* invoke) {
  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dest_pos = invoke->InputAt(3)->AsIntConstant();
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();

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

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

// Utility routine to verify that "length(input) - pos >= length"
static void EnoughItems(MipsAssembler* assembler,
                        Register length_input_minus_pos,
                        Location length,
                        SlowPathCodeMIPS* slow_path) {
  if (length.IsConstant()) {
    int32_t length_constant = length.GetConstant()->AsIntConstant()->GetValue();

    if (IsInt<16>(length_constant)) {
      __ Slti(TMP, length_input_minus_pos, length_constant);
      __ Bnez(TMP, slow_path->GetEntryLabel());
    } else {
      __ LoadConst32(TMP, length_constant);
      __ Blt(length_input_minus_pos, TMP, slow_path->GetEntryLabel());
    }
  } else {
    __ Blt(length_input_minus_pos, length.AsRegister<Register>(), slow_path->GetEntryLabel());
  }
}

static void CheckPosition(MipsAssembler* assembler,
                          Location pos,
                          Register input,
                          Location length,
                          SlowPathCodeMIPS* slow_path,
                          bool length_is_input_length = false) {
  // Where is the length in the Array?
  const uint32_t length_offset = mirror::Array::LengthOffset().Uint32Value();

  // Calculate length(input) - pos.
  if (pos.IsConstant()) {
    int32_t pos_const = pos.GetConstant()->AsIntConstant()->GetValue();
    if (pos_const == 0) {
      if (!length_is_input_length) {
        // Check that length(input) >= length.
        __ LoadFromOffset(kLoadWord, AT, input, length_offset);
        EnoughItems(assembler, AT, length, slow_path);
      }
    } else {
      // Check that (length(input) - pos) >= zero.
      __ LoadFromOffset(kLoadWord, AT, input, length_offset);
      DCHECK_GT(pos_const, 0);
      __ Addiu32(AT, AT, -pos_const, TMP);
      __ Bltz(AT, slow_path->GetEntryLabel());

      // Verify that (length(input) - pos) >= length.
      EnoughItems(assembler, AT, length, slow_path);
    }
  } else if (length_is_input_length) {
    // The only way the copy can succeed is if pos is zero.
    Register pos_reg = pos.AsRegister<Register>();
    __ Bnez(pos_reg, slow_path->GetEntryLabel());
  } else {
    // Verify that pos >= 0.
    Register pos_reg = pos.AsRegister<Register>();
    __ Bltz(pos_reg, slow_path->GetEntryLabel());

    // Check that (length(input) - pos) >= zero.
    __ LoadFromOffset(kLoadWord, AT, input, length_offset);
    __ Subu(AT, AT, pos_reg);
    __ Bltz(AT, slow_path->GetEntryLabel());

    // Verify that (length(input) - pos) >= length.
    EnoughItems(assembler, AT, length, slow_path);
  }
}

void IntrinsicCodeGeneratorMIPS::VisitSystemArrayCopyChar(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  LocationSummary* locations = invoke->GetLocations();

  Register src = locations->InAt(0).AsRegister<Register>();
  Location src_pos = locations->InAt(1);
  Register dest = locations->InAt(2).AsRegister<Register>();
  Location dest_pos = locations->InAt(3);
  Location length = locations->InAt(4);

  MipsLabel loop;

  Register dest_base = locations->GetTemp(0).AsRegister<Register>();
  Register src_base = locations->GetTemp(1).AsRegister<Register>();
  Register count = locations->GetTemp(2).AsRegister<Register>();

  SlowPathCodeMIPS* slow_path = new (codegen_->GetScopedAllocator()) IntrinsicSlowPathMIPS(invoke);
  codegen_->AddSlowPath(slow_path);

  // Bail out if the source and destination are the same (to handle overlap).
  __ Beq(src, dest, slow_path->GetEntryLabel());

  // Bail out if the source is null.
  __ Beqz(src, slow_path->GetEntryLabel());

  // Bail out if the destination is null.
  __ Beqz(dest, slow_path->GetEntryLabel());

  // Load length into register for count.
  if (length.IsConstant()) {
    __ LoadConst32(count, length.GetConstant()->AsIntConstant()->GetValue());
  } else {
    // If the length is negative, bail out.
    // We have already checked in the LocationsBuilder for the constant case.
    __ Bltz(length.AsRegister<Register>(), slow_path->GetEntryLabel());

    __ Move(count, length.AsRegister<Register>());
  }

  // Validity checks: source.
  CheckPosition(assembler, src_pos, src, Location::RegisterLocation(count), slow_path);

  // Validity checks: dest.
  CheckPosition(assembler, dest_pos, dest, Location::RegisterLocation(count), slow_path);

  // If count is zero, we're done.
  __ Beqz(count, slow_path->GetExitLabel());

  // Okay, everything checks out.  Finally time to do the copy.
  // Check assumption that sizeof(Char) is 2 (used in scaling below).
  const size_t char_size = DataType::Size(DataType::Type::kUint16);
  DCHECK_EQ(char_size, 2u);

  const size_t char_shift = DataType::SizeShift(DataType::Type::kUint16);

  const uint32_t data_offset = mirror::Array::DataOffset(char_size).Uint32Value();

  // Calculate source and destination addresses.
  if (src_pos.IsConstant()) {
    int32_t src_pos_const = src_pos.GetConstant()->AsIntConstant()->GetValue();

    __ Addiu32(src_base, src, data_offset + char_size * src_pos_const, TMP);
  } else {
    __ Addiu32(src_base, src, data_offset, TMP);
    __ ShiftAndAdd(src_base, src_pos.AsRegister<Register>(), src_base, char_shift);
  }
  if (dest_pos.IsConstant()) {
    int32_t dest_pos_const = dest_pos.GetConstant()->AsIntConstant()->GetValue();

    __ Addiu32(dest_base, dest, data_offset + char_size * dest_pos_const, TMP);
  } else {
    __ Addiu32(dest_base, dest, data_offset, TMP);
    __ ShiftAndAdd(dest_base, dest_pos.AsRegister<Register>(), dest_base, char_shift);
  }

  __ Bind(&loop);
  __ Lh(TMP, src_base, 0);
  __ Addiu(src_base, src_base, char_size);
  __ Addiu(count, count, -1);
  __ Sh(TMP, dest_base, 0);
  __ Addiu(dest_base, dest_base, char_size);
  __ Bnez(count, &loop);

  __ Bind(slow_path->GetExitLabel());
}

// long java.lang.Integer.valueOf(long)
void IntrinsicLocationsBuilderMIPS::VisitIntegerValueOf(HInvoke* invoke) {
  InvokeRuntimeCallingConvention calling_convention;
  IntrinsicVisitor::ComputeIntegerValueOfLocations(
      invoke,
      codegen_,
      calling_convention.GetReturnLocation(DataType::Type::kReference),
      Location::RegisterLocation(calling_convention.GetRegisterAt(0)));
}

void IntrinsicCodeGeneratorMIPS::VisitIntegerValueOf(HInvoke* invoke) {
  IntrinsicVisitor::IntegerValueOfInfo info = IntrinsicVisitor::ComputeIntegerValueOfInfo();
  LocationSummary* locations = invoke->GetLocations();
  MipsAssembler* assembler = GetAssembler();
  InstructionCodeGeneratorMIPS* icodegen =
      down_cast<InstructionCodeGeneratorMIPS*>(codegen_->GetInstructionVisitor());

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
      __ LoadConst32(out, address);
    } else {
      // Allocate and initialize a new j.l.Integer.
      // TODO: If we JIT, we could allocate the j.l.Integer now, and store it in the
      // JIT object table.
      uint32_t address =
          dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
      __ LoadConst32(calling_convention.GetRegisterAt(0), address);
      codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
      CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
      __ StoreConstToOffset(kStoreWord, value, out, info.value_offset, TMP);
      // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
      // one.
      icodegen->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    }
  } else {
    Register in = locations->InAt(0).AsRegister<Register>();
    MipsLabel allocate, done;
    int32_t count = static_cast<uint32_t>(info.high) - info.low + 1;

    // Is (info.low <= in) && (in <= info.high)?
    __ Addiu32(out, in, -info.low);
    // As unsigned quantities is out < (info.high - info.low + 1)?
    if (IsInt<16>(count)) {
      __ Sltiu(AT, out, count);
    } else {
      __ LoadConst32(AT, count);
      __ Sltu(AT, out, AT);
    }
    // Branch if out >= (info.high - info.low + 1).
    // This means that "in" is outside of the range [info.low, info.high].
    __ Beqz(AT, &allocate);

    // If the value is within the bounds, load the j.l.Integer directly from the array.
    uint32_t data_offset = mirror::Array::DataOffset(kHeapReferenceSize).Uint32Value();
    uint32_t address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.cache));
    __ LoadConst32(TMP, data_offset + address);
    __ ShiftAndAdd(out, out, TMP, TIMES_4);
    __ Lw(out, out, 0);
    __ MaybeUnpoisonHeapReference(out);
    __ B(&done);

    __ Bind(&allocate);
    // Otherwise allocate and initialize a new j.l.Integer.
    address = dchecked_integral_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.integer));
    __ LoadConst32(calling_convention.GetRegisterAt(0), address);
    codegen_->InvokeRuntime(kQuickAllocObjectInitialized, invoke, invoke->GetDexPc());
    CheckEntrypointTypes<kQuickAllocObjectWithChecks, void*, mirror::Class*>();
    __ StoreToOffset(kStoreWord, in, out, info.value_offset);
    // `value` is a final field :-( Ideally, we'd merge this memory barrier with the allocation
    // one.
    icodegen->GenerateMemoryBarrier(MemBarrierKind::kStoreStore);
    __ Bind(&done);
  }
}

// static boolean java.lang.Thread.interrupted()
void IntrinsicLocationsBuilderMIPS::VisitThreadInterrupted(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetOut(Location::RequiresRegister());
}

void IntrinsicCodeGeneratorMIPS::VisitThreadInterrupted(HInvoke* invoke) {
  MipsAssembler* assembler = GetAssembler();
  Register out = invoke->GetLocations()->Out().AsRegister<Register>();
  int32_t offset = Thread::InterruptedOffset<kMipsPointerSize>().Int32Value();
  __ LoadFromOffset(kLoadWord, out, TR, offset);
  MipsLabel done;
  __ Beqz(out, &done);
  __ Sync(0);
  __ StoreToOffset(kStoreWord, ZERO, TR, offset);
  __ Sync(0);
  __ Bind(&done);
}

void IntrinsicLocationsBuilderMIPS::VisitReachabilityFence(HInvoke* invoke) {
  LocationSummary* locations =
      new (allocator_) LocationSummary(invoke, LocationSummary::kNoCall, kIntrinsified);
  locations->SetInAt(0, Location::Any());
}

void IntrinsicCodeGeneratorMIPS::VisitReachabilityFence(HInvoke* invoke ATTRIBUTE_UNUSED) { }

// Unimplemented intrinsics.

UNIMPLEMENTED_INTRINSIC(MIPS, MathCeil)
UNIMPLEMENTED_INTRINSIC(MIPS, MathFloor)
UNIMPLEMENTED_INTRINSIC(MIPS, MathRint)
UNIMPLEMENTED_INTRINSIC(MIPS, MathRoundDouble)
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetLongVolatile);
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafePutLongVolatile);
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeCASLong)

UNIMPLEMENTED_INTRINSIC(MIPS, ReferenceGetReferent)
UNIMPLEMENTED_INTRINSIC(MIPS, SystemArrayCopy)

UNIMPLEMENTED_INTRINSIC(MIPS, StringStringIndexOf);
UNIMPLEMENTED_INTRINSIC(MIPS, StringStringIndexOfAfter);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBufferAppend);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBufferLength);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBufferToString);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBuilderAppend);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBuilderLength);
UNIMPLEMENTED_INTRINSIC(MIPS, StringBuilderToString);

// 1.8.
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetAndAddInt)
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetAndAddLong)
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetAndSetInt)
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetAndSetLong)
UNIMPLEMENTED_INTRINSIC(MIPS, UnsafeGetAndSetObject)

UNREACHABLE_INTRINSICS(MIPS)

#undef __

}  // namespace mips
}  // namespace art
