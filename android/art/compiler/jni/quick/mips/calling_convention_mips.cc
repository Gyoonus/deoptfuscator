/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "calling_convention_mips.h"

#include <android-base/logging.h>

#include "handle_scope-inl.h"
#include "utils/mips/managed_register_mips.h"

namespace art {
namespace mips {

//
// JNI calling convention constants.
//

// Up to how many float-like (float, double) args can be enregistered in floating-point registers.
// The rest of the args must go in integer registers or on the stack.
constexpr size_t kMaxFloatOrDoubleRegisterArguments = 2u;
// Up to how many integer-like (pointers, objects, longs, int, short, bool, etc) args can be
// enregistered. The rest of the args must go on the stack.
constexpr size_t kMaxIntLikeRegisterArguments = 4u;

static const Register kJniCoreArgumentRegisters[] = { A0, A1, A2, A3 };
static const FRegister kJniFArgumentRegisters[] = { F12, F14 };
static const DRegister kJniDArgumentRegisters[] = { D6, D7 };

//
// Managed calling convention constants.
//

static const Register kManagedCoreArgumentRegisters[] = { A0, A1, A2, A3, T0, T1 };
static const FRegister kManagedFArgumentRegisters[] = { F8, F10, F12, F14, F16, F18 };
static const DRegister kManagedDArgumentRegisters[] = { D4, D5, D6, D7, D8, D9 };

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    MipsManagedRegister::FromCoreRegister(S2),
    MipsManagedRegister::FromCoreRegister(S3),
    MipsManagedRegister::FromCoreRegister(S4),
    MipsManagedRegister::FromCoreRegister(S5),
    MipsManagedRegister::FromCoreRegister(S6),
    MipsManagedRegister::FromCoreRegister(S7),
    MipsManagedRegister::FromCoreRegister(FP),
    // No hard float callee saves.
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // RA is a special callee save which is not reported by CalleeSaveRegisters().
  uint32_t result = 1 << RA;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsMips().IsCoreRegister()) {
      result |= (1 << r.AsMips().AsCoreRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = 0u;

// Calling convention
ManagedRegister MipsManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return MipsManagedRegister::FromCoreRegister(T9);
}

ManagedRegister MipsJniCallingConvention::InterproceduralScratchRegister() {
  return MipsManagedRegister::FromCoreRegister(T9);
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty) {
  if (shorty[0] == 'F') {
    return MipsManagedRegister::FromFRegister(F0);
  } else if (shorty[0] == 'D') {
    return MipsManagedRegister::FromDRegister(D0);
  } else if (shorty[0] == 'J') {
    return MipsManagedRegister::FromRegisterPair(V0_V1);
  } else if (shorty[0] == 'V') {
    return MipsManagedRegister::NoRegister();
  } else {
    return MipsManagedRegister::FromCoreRegister(V0);
  }
}

ManagedRegister MipsManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister MipsJniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister MipsJniCallingConvention::IntReturnRegister() {
  return MipsManagedRegister::FromCoreRegister(V0);
}

// Managed runtime calling convention

ManagedRegister MipsManagedRuntimeCallingConvention::MethodRegister() {
  return MipsManagedRegister::FromCoreRegister(A0);
}

bool MipsManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything moved to stack on entry.
}

bool MipsManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;
}

ManagedRegister MipsManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset MipsManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  FrameOffset result =
      FrameOffset(displacement_.Int32Value() +        // displacement
                  kFramePointerSize +                 // Method*
                  (itr_slots_ * kFramePointerSize));  // offset into in args
  return result;
}

const ManagedRegisterEntrySpills& MipsManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on MIPS to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if ((entry_spills_.size() == 0) && (NumArgs() > 0)) {
    uint32_t gpr_index = 1;  // Skip A0, it is used for ArtMethod*.
    uint32_t fpr_index = 0;

    for (ResetIterator(FrameOffset(0)); HasNext(); Next()) {
      if (IsCurrentParamAFloatOrDouble()) {
        if (IsCurrentParamADouble()) {
          if (fpr_index < arraysize(kManagedDArgumentRegisters)) {
            entry_spills_.push_back(
                MipsManagedRegister::FromDRegister(kManagedDArgumentRegisters[fpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 8);
          }
        } else {
          if (fpr_index < arraysize(kManagedFArgumentRegisters)) {
            entry_spills_.push_back(
                MipsManagedRegister::FromFRegister(kManagedFArgumentRegisters[fpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }
      } else {
        if (IsCurrentParamALong() && !IsCurrentParamAReference()) {
          if (gpr_index == 1 || gpr_index == 3) {
            // Don't use A1-A2(A3-T0) as a register pair, move to A2-A3(T0-T1) instead.
            gpr_index++;
          }
          if (gpr_index < arraysize(kManagedCoreArgumentRegisters) - 1) {
            entry_spills_.push_back(
                MipsManagedRegister::FromCoreRegister(kManagedCoreArgumentRegisters[gpr_index++]));
          } else if (gpr_index == arraysize(kManagedCoreArgumentRegisters) - 1) {
            gpr_index++;
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }

        if (gpr_index < arraysize(kManagedCoreArgumentRegisters)) {
          entry_spills_.push_back(
              MipsManagedRegister::FromCoreRegister(kManagedCoreArgumentRegisters[gpr_index++]));
        } else {
          entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
        }
      }
    }
  }
  return entry_spills_;
}

// JNI calling convention

MipsJniCallingConvention::MipsJniCallingConvention(bool is_static,
                                                   bool is_synchronized,
                                                   bool is_critical_native,
                                                   const char* shorty)
    : JniCallingConvention(is_static,
                           is_synchronized,
                           is_critical_native,
                           shorty,
                           kMipsPointerSize) {
  // SYSTEM V - Application Binary Interface (MIPS RISC Processor):
  // Data Representation - Fundamental Types (3-4) specifies fundamental alignments for each type.
  //   "Each member is assigned to the lowest available offset with the appropriate alignment. This
  // may require internal padding, depending on the previous member."
  //
  // All of our stack arguments are usually 4-byte aligned, however longs and doubles must be 8
  // bytes aligned. Add padding to maintain 8-byte alignment invariant.
  //
  // Compute padding to ensure longs and doubles are not split in o32.
  size_t padding = 0;
  size_t cur_arg, cur_reg;
  if (LIKELY(HasExtraArgumentsForJni())) {
    // Ignore the 'this' jobject or jclass for static methods and the JNIEnv.
    // We start at the aligned register A2.
    //
    // Ignore the first 2 parameters because they are guaranteed to be aligned.
    cur_arg = NumImplicitArgs();  // Skip the "this" argument.
    cur_reg = 2;  // Skip {A0=JNIEnv, A1=jobject} / {A0=JNIEnv, A1=jclass} parameters (start at A2).
  } else {
    // Check every parameter.
    cur_arg = 0;
    cur_reg = 0;
  }

  // Shift across a logical register mapping that looks like:
  //
  //   | A0 | A1 | A2 | A3 | SP+16 | SP+20 | SP+24 | ... | SP+n | SP+n+4 |
  //
  //   or some of variants with floating-point registers (F12 and F14), for example
  //
  //   | F12     | F14 | A3 | SP+16 | SP+20 | SP+24 | ... | SP+n | SP+n+4 |
  //
  //   (where SP is the stack pointer at the start of called function).
  //
  // Any time there would normally be a long/double in an odd logical register,
  // we have to push out the rest of the mappings by 4 bytes to maintain an 8-byte alignment.
  //
  // This works for both physical register pairs {A0, A1}, {A2, A3},
  // floating-point registers F12, F14 and for when the value is on the stack.
  //
  // For example:
  // (a) long would normally go into A1, but we shift it into A2
  //  | INT | (PAD) | LONG    |
  //  | A0  |  A1   | A2 | A3 |
  //
  // (b) long would normally go into A3, but we shift it into SP
  //  | INT | INT | INT | (PAD) | LONG        |
  //  | A0  | A1  | A2  |  A3   | SP+16 SP+20 |
  //
  // where INT is any <=4 byte arg, and LONG is any 8-byte arg.
  for (; cur_arg < NumArgs(); cur_arg++) {
    if (IsParamALongOrDouble(cur_arg)) {
      if ((cur_reg & 1) != 0) {
        padding += 4;
        cur_reg++;   // Additional bump to ensure alignment.
      }
      cur_reg += 2;  // Bump the iterator twice for every long argument.
    } else {
      cur_reg++;     // Bump the iterator for every argument.
    }
  }
  if (cur_reg < kMaxIntLikeRegisterArguments) {
    // As a special case when, as a result of shifting (or not) there are no arguments on the stack,
    // we actually have 0 stack padding.
    //
    // For example with @CriticalNative and:
    // (int, long) -> shifts the long but doesn't need to pad the stack
    //
    //          shift
    //           \/
    //  | INT | (PAD) | LONG      | (EMPTY) ...
    //  | r0  |  r1   |  r2  | r3 |   SP    ...
    //                                /\
    //                          no stack padding
    padding_ = 0;
  } else {
    padding_ = padding;
  }

  // Argument Passing (3-17):
  //   "When the first argument is integral, the remaining arguments are passed in the integer
  // registers."
  //
  //   "The rules that determine which arguments go into registers and which ones must be passed on
  // the stack are most easily explained by considering the list of arguments as a structure,
  // aligned according to normal structure rules. Mapping of this structure into the combination of
  // stack and registers is as follows: up to two leading floating-point arguments can be passed in
  // $f12 and $f14; everything else with a structure offset greater than or equal to 16 is passed on
  // the stack. The remainder of the arguments are passed in $4..$7 based on their structure offset.
  // Holes left in the structure for alignment are unused, whether in registers or in the stack."
  //
  // For example with @CriticalNative and:
  // (a) first argument is not floating-point, so all go into integer registers
  //  | INT | FLOAT | DOUBLE  |
  //  | A0  |  A1   | A2 | A3 |
  // (b) first argument is floating-point, but 2nd is integer
  //  | FLOAT | INT | DOUBLE  |
  //  |  F12  | A1  | A2 | A3 |
  // (c) first two arguments are floating-point (float, double)
  //  | FLOAT | (PAD) | DOUBLE |  INT  |
  //  |  F12  |       |  F14   | SP+16 |
  // (d) first two arguments are floating-point (double, float)
  //  | DOUBLE | FLOAT | INT |
  //  |  F12   |  F14  | A3  |
  // (e) first three arguments are floating-point, but just first two will go into fp registers
  //  | DOUBLE | FLOAT | FLOAT |
  //  |  F12   |  F14  |  A3   |
  //
  // Find out if the first argument is a floating-point. In that case, floating-point registers will
  // be used for up to two leading floating-point arguments. Otherwise, all arguments will be passed
  // using integer registers.
  use_fp_arg_registers_ = false;
  if (is_critical_native) {
    if (NumArgs() > 0) {
      if (IsParamAFloatOrDouble(0)) {
        use_fp_arg_registers_ = true;
      }
    }
  }
}

uint32_t MipsJniCallingConvention::CoreSpillMask() const {
  return kCoreCalleeSpillMask;
}

uint32_t MipsJniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

ManagedRegister MipsJniCallingConvention::ReturnScratchRegister() const {
  return MipsManagedRegister::FromCoreRegister(AT);
}

size_t MipsJniCallingConvention::FrameSize() {
  // ArtMethod*, RA and callee save area size, local reference segment state.
  const size_t method_ptr_size = static_cast<size_t>(kMipsPointerSize);
  const size_t ra_return_addr_size = kFramePointerSize;
  const size_t callee_save_area_size = CalleeSaveRegisters().size() * kFramePointerSize;

  size_t frame_data_size = method_ptr_size + ra_return_addr_size + callee_save_area_size;

  if (LIKELY(HasLocalReferenceSegmentState())) {
    // Local reference segment state.
    frame_data_size += kFramePointerSize;
  }

  // References plus 2 words for HandleScope header.
  const size_t handle_scope_size = HandleScope::SizeOf(kMipsPointerSize, ReferenceCount());

  size_t total_size = frame_data_size;
  if (LIKELY(HasHandleScope())) {
    // HandleScope is sometimes excluded.
    total_size += handle_scope_size;    // Handle scope size.
  }

  // Plus return value spill area size.
  total_size += SizeOfReturnValue();

  return RoundUp(total_size, kStackAlignment);
}

size_t MipsJniCallingConvention::OutArgSize() {
  // Argument Passing (3-17):
  //   "Despite the fact that some or all of the arguments to a function are passed in registers,
  // always allocate space on the stack for all arguments. This stack space should be a structure
  // large enough to contain all the arguments, aligned according to normal structure rules (after
  // promotion and structure return pointer insertion). The locations within the stack frame used
  // for arguments are called the home locations."
  //
  // Allocate 16 bytes for home locations + space needed for stack arguments.
  return RoundUp(
      (kMaxIntLikeRegisterArguments + NumberOfOutgoingStackArgs()) * kFramePointerSize + padding_,
      kStackAlignment);
}

ArrayRef<const ManagedRegister> MipsJniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

// JniCallingConvention ABI follows o32 where longs and doubles must occur
// in even register numbers and stack slots.
void MipsJniCallingConvention::Next() {
  JniCallingConvention::Next();

  if (LIKELY(HasNext())) {  // Avoid CHECK failure for IsCurrentParam
    // Ensure slot is 8-byte aligned for longs/doubles (o32).
    if (IsCurrentParamALongOrDouble() && ((itr_slots_ & 0x1u) != 0)) {
      // itr_slots_ needs to be an even number, according to o32.
      itr_slots_++;
    }
  }
}

bool MipsJniCallingConvention::IsCurrentParamInRegister() {
  // Argument Passing (3-17):
  //   "The rules that determine which arguments go into registers and which ones must be passed on
  // the stack are most easily explained by considering the list of arguments as a structure,
  // aligned according to normal structure rules. Mapping of this structure into the combination of
  // stack and registers is as follows: up to two leading floating-point arguments can be passed in
  // $f12 and $f14; everything else with a structure offset greater than or equal to 16 is passed on
  // the stack. The remainder of the arguments are passed in $4..$7 based on their structure offset.
  // Holes left in the structure for alignment are unused, whether in registers or in the stack."
  //
  // Even when floating-point registers are used, there can be up to 4 arguments passed in
  // registers.
  return itr_slots_ < kMaxIntLikeRegisterArguments;
}

bool MipsJniCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

ManagedRegister MipsJniCallingConvention::CurrentParamRegister() {
  CHECK_LT(itr_slots_, kMaxIntLikeRegisterArguments);
  // Up to two leading floating-point arguments can be passed in floating-point registers.
  if (use_fp_arg_registers_ && (itr_args_ < kMaxFloatOrDoubleRegisterArguments)) {
    if (IsCurrentParamAFloatOrDouble()) {
      if (IsCurrentParamADouble()) {
        return MipsManagedRegister::FromDRegister(kJniDArgumentRegisters[itr_args_]);
      } else {
        return MipsManagedRegister::FromFRegister(kJniFArgumentRegisters[itr_args_]);
      }
    }
  }
  // All other arguments (including other floating-point arguments) will be passed in integer
  // registers.
  if (IsCurrentParamALongOrDouble()) {
    if (itr_slots_ == 0u) {
      return MipsManagedRegister::FromRegisterPair(A0_A1);
    } else {
      CHECK_EQ(itr_slots_, 2u);
      return MipsManagedRegister::FromRegisterPair(A2_A3);
    }
  } else {
    return MipsManagedRegister::FromCoreRegister(kJniCoreArgumentRegisters[itr_slots_]);
  }
}

FrameOffset MipsJniCallingConvention::CurrentParamStackOffset() {
  CHECK_GE(itr_slots_, kMaxIntLikeRegisterArguments);
  size_t offset = displacement_.Int32Value() - OutArgSize() + (itr_slots_ * kFramePointerSize);
  CHECK_LT(offset, OutArgSize());
  return FrameOffset(offset);
}

size_t MipsJniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = HasSelfClass() ? 1 : 0;            // Count jclass.
  // Regular argument parameters and this.
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();  // Twice count 8-byte args.
  // Count JNIEnv* less arguments in registers.
  size_t internal_args = (HasJniEnv() ? 1 : 0);
  size_t total_args = static_args + param_args + internal_args;

  return total_args - std::min(kMaxIntLikeRegisterArguments, static_cast<size_t>(total_args));
}

}  // namespace mips
}  // namespace art
