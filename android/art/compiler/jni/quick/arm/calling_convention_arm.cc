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

#include "calling_convention_arm.h"

#include <android-base/logging.h>

#include "base/macros.h"
#include "handle_scope-inl.h"
#include "utils/arm/managed_register_arm.h"

namespace art {
namespace arm {

static_assert(kArmPointerSize == PointerSize::k32, "Unexpected ARM pointer size");

//
// JNI calling convention constants.
//

// List of parameters passed via registers for JNI.
// JNI uses soft-float, so there is only a GPR list.
static const Register kJniArgumentRegisters[] = {
  R0, R1, R2, R3
};

static const size_t kJniArgumentRegisterCount = arraysize(kJniArgumentRegisters);

//
// Managed calling convention constants.
//

// Used by hard float. (General purpose registers.)
static const Register kHFCoreArgumentRegisters[] = {
  R0, R1, R2, R3
};

// (VFP single-precision registers.)
static const SRegister kHFSArgumentRegisters[] = {
  S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15
};

// (VFP double-precision registers.)
static const DRegister kHFDArgumentRegisters[] = {
  D0, D1, D2, D3, D4, D5, D6, D7
};

static_assert(arraysize(kHFDArgumentRegisters) * 2 == arraysize(kHFSArgumentRegisters),
    "ks d argument registers mismatch");

//
// Shared managed+JNI calling convention constants.
//

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    ArmManagedRegister::FromCoreRegister(R5),
    ArmManagedRegister::FromCoreRegister(R6),
    ArmManagedRegister::FromCoreRegister(R7),
    ArmManagedRegister::FromCoreRegister(R8),
    ArmManagedRegister::FromCoreRegister(R10),
    ArmManagedRegister::FromCoreRegister(R11),
    // Hard float registers.
    ArmManagedRegister::FromSRegister(S16),
    ArmManagedRegister::FromSRegister(S17),
    ArmManagedRegister::FromSRegister(S18),
    ArmManagedRegister::FromSRegister(S19),
    ArmManagedRegister::FromSRegister(S20),
    ArmManagedRegister::FromSRegister(S21),
    ArmManagedRegister::FromSRegister(S22),
    ArmManagedRegister::FromSRegister(S23),
    ArmManagedRegister::FromSRegister(S24),
    ArmManagedRegister::FromSRegister(S25),
    ArmManagedRegister::FromSRegister(S26),
    ArmManagedRegister::FromSRegister(S27),
    ArmManagedRegister::FromSRegister(S28),
    ArmManagedRegister::FromSRegister(S29),
    ArmManagedRegister::FromSRegister(S30),
    ArmManagedRegister::FromSRegister(S31)
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // LR is a special callee save which is not reported by CalleeSaveRegisters().
  uint32_t result = 1 << LR;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsArm().IsCoreRegister()) {
      result |= (1 << r.AsArm().AsCoreRegister());
    }
  }
  return result;
}

static constexpr uint32_t CalculateFpCalleeSpillMask() {
  uint32_t result = 0;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsArm().IsSRegister()) {
      result |= (1 << r.AsArm().AsSRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = CalculateFpCalleeSpillMask();

// Calling convention

ManagedRegister ArmManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return ArmManagedRegister::FromCoreRegister(IP);  // R12
}

ManagedRegister ArmJniCallingConvention::InterproceduralScratchRegister() {
  return ArmManagedRegister::FromCoreRegister(IP);  // R12
}

ManagedRegister ArmManagedRuntimeCallingConvention::ReturnRegister() {
  switch (GetShorty()[0]) {
    case 'V':
      return ArmManagedRegister::NoRegister();
    case 'D':
      return ArmManagedRegister::FromDRegister(D0);
    case 'F':
      return ArmManagedRegister::FromSRegister(S0);
    case 'J':
      return ArmManagedRegister::FromRegisterPair(R0_R1);
    default:
      return ArmManagedRegister::FromCoreRegister(R0);
  }
}

ManagedRegister ArmJniCallingConvention::ReturnRegister() {
  switch (GetShorty()[0]) {
  case 'V':
    return ArmManagedRegister::NoRegister();
  case 'D':
  case 'J':
    return ArmManagedRegister::FromRegisterPair(R0_R1);
  default:
    return ArmManagedRegister::FromCoreRegister(R0);
  }
}

ManagedRegister ArmJniCallingConvention::IntReturnRegister() {
  return ArmManagedRegister::FromCoreRegister(R0);
}

// Managed runtime calling convention

ManagedRegister ArmManagedRuntimeCallingConvention::MethodRegister() {
  return ArmManagedRegister::FromCoreRegister(R0);
}

bool ArmManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything moved to stack on entry.
}

bool ArmManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;
}

ManagedRegister ArmManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset ArmManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  FrameOffset result =
      FrameOffset(displacement_.Int32Value() +        // displacement
                  kFramePointerSize +                 // Method*
                  (itr_slots_ * kFramePointerSize));  // offset into in args
  return result;
}

const ManagedRegisterEntrySpills& ArmManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on ARM to free them up for scratch use, we then assume
  // all arguments are on the stack.
  if ((entry_spills_.size() == 0) && (NumArgs() > 0)) {
    uint32_t gpr_index = 1;  // R0 ~ R3. Reserve r0 for ArtMethod*.
    uint32_t fpr_index = 0;  // S0 ~ S15.
    uint32_t fpr_double_index = 0;  // D0 ~ D7.

    ResetIterator(FrameOffset(0));
    while (HasNext()) {
      if (IsCurrentParamAFloatOrDouble()) {
        if (IsCurrentParamADouble()) {  // Double.
          // Double should not overlap with float.
          fpr_double_index = (std::max(fpr_double_index * 2, RoundUp(fpr_index, 2))) / 2;
          if (fpr_double_index < arraysize(kHFDArgumentRegisters)) {
            entry_spills_.push_back(
                ArmManagedRegister::FromDRegister(kHFDArgumentRegisters[fpr_double_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 8);
          }
        } else {  // Float.
          // Float should not overlap with double.
          if (fpr_index % 2 == 0) {
            fpr_index = std::max(fpr_double_index * 2, fpr_index);
          }
          if (fpr_index < arraysize(kHFSArgumentRegisters)) {
            entry_spills_.push_back(
                ArmManagedRegister::FromSRegister(kHFSArgumentRegisters[fpr_index++]));
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }
      } else {
        // FIXME: Pointer this returns as both reference and long.
        if (IsCurrentParamALong() && !IsCurrentParamAReference()) {  // Long.
          if (gpr_index < arraysize(kHFCoreArgumentRegisters) - 1) {
            // Skip R1, and use R2_R3 if the long is the first parameter.
            if (gpr_index == 1) {
              gpr_index++;
            }
          }

          // If it spans register and memory, we must use the value in memory.
          if (gpr_index < arraysize(kHFCoreArgumentRegisters) - 1) {
            entry_spills_.push_back(
                ArmManagedRegister::FromCoreRegister(kHFCoreArgumentRegisters[gpr_index++]));
          } else if (gpr_index == arraysize(kHFCoreArgumentRegisters) - 1) {
            gpr_index++;
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          } else {
            entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
          }
        }
        // High part of long or 32-bit argument.
        if (gpr_index < arraysize(kHFCoreArgumentRegisters)) {
          entry_spills_.push_back(
              ArmManagedRegister::FromCoreRegister(kHFCoreArgumentRegisters[gpr_index++]));
        } else {
          entry_spills_.push_back(ManagedRegister::NoRegister(), 4);
        }
      }
      Next();
    }
  }
  return entry_spills_;
}
// JNI calling convention

ArmJniCallingConvention::ArmJniCallingConvention(bool is_static,
                                                 bool is_synchronized,
                                                 bool is_critical_native,
                                                 const char* shorty)
    : JniCallingConvention(is_static,
                           is_synchronized,
                           is_critical_native,
                           shorty,
                           kArmPointerSize) {
  // AAPCS 4.1 specifies fundamental alignments for each type. All of our stack arguments are
  // usually 4-byte aligned, however longs and doubles must be 8 bytes aligned. Add padding to
  // maintain 8-byte alignment invariant.
  //
  // Compute padding to ensure longs and doubles are not split in AAPCS.
  size_t shift = 0;

  size_t cur_arg, cur_reg;
  if (LIKELY(HasExtraArgumentsForJni())) {
    // Ignore the 'this' jobject or jclass for static methods and the JNIEnv.
    // We start at the aligned register r2.
    //
    // Ignore the first 2 parameters because they are guaranteed to be aligned.
    cur_arg = NumImplicitArgs();  // skip the "this" arg.
    cur_reg = 2;  // skip {r0=JNIEnv, r1=jobject} / {r0=JNIEnv, r1=jclass} parameters (start at r2).
  } else {
    // Check every parameter.
    cur_arg = 0;
    cur_reg = 0;
  }

  // TODO: Maybe should just use IsCurrentParamALongOrDouble instead to be cleaner?
  // (this just seems like an unnecessary micro-optimization).

  // Shift across a logical register mapping that looks like:
  //
  //   | r0 | r1 | r2 | r3 | SP | SP+4| SP+8 | SP+12 | ... | SP+n | SP+n+4 |
  //
  //   (where SP is some arbitrary stack pointer that our 0th stack arg would go into).
  //
  // Any time there would normally be a long/double in an odd logical register,
  // we have to push out the rest of the mappings by 4 bytes to maintain an 8-byte alignment.
  //
  // This works for both physical register pairs {r0, r1}, {r2, r3} and for when
  // the value is on the stack.
  //
  // For example:
  // (a) long would normally go into r1, but we shift it into r2
  //  | INT | (PAD) | LONG      |
  //  | r0  |  r1   |  r2  | r3 |
  //
  // (b) long would normally go into r3, but we shift it into SP
  //  | INT | INT | INT | (PAD) | LONG     |
  //  | r0  |  r1 |  r2 |  r3   | SP+4 SP+8|
  //
  // where INT is any <=4 byte arg, and LONG is any 8-byte arg.
  for (; cur_arg < NumArgs(); cur_arg++) {
    if (IsParamALongOrDouble(cur_arg)) {
      if ((cur_reg & 1) != 0) {  // check that it's in a logical contiguous register pair
        shift += 4;
        cur_reg++;  // additional bump to ensure alignment
      }
      cur_reg += 2;  // bump the iterator twice for every long argument
    } else {
      cur_reg++;  // bump the iterator for every non-long argument
    }
  }

  if (cur_reg < kJniArgumentRegisterCount) {
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
    padding_ = shift;
  }

  // TODO: add some new JNI tests for @CriticalNative that introduced new edge cases
  // (a) Using r0,r1 pair = f(long,...)
  // (b) Shifting r1 long into r2,r3 pair = f(int, long, int, ...);
  // (c) Shifting but not introducing a stack padding = f(int, long);
}

uint32_t ArmJniCallingConvention::CoreSpillMask() const {
  // Compute spill mask to agree with callee saves initialized in the constructor
  return kCoreCalleeSpillMask;
}

uint32_t ArmJniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

ManagedRegister ArmJniCallingConvention::ReturnScratchRegister() const {
  return ArmManagedRegister::FromCoreRegister(R2);
}

size_t ArmJniCallingConvention::FrameSize() {
  // Method*, LR and callee save area size, local reference segment state
  const size_t method_ptr_size = static_cast<size_t>(kArmPointerSize);
  const size_t lr_return_addr_size = kFramePointerSize;
  const size_t callee_save_area_size = CalleeSaveRegisters().size() * kFramePointerSize;
  size_t frame_data_size = method_ptr_size + lr_return_addr_size + callee_save_area_size;

  if (LIKELY(HasLocalReferenceSegmentState())) {
    // local reference segment state
    frame_data_size += kFramePointerSize;
    // TODO: Probably better to use sizeof(IRTSegmentState) here...
  }

  // References plus link_ (pointer) and number_of_references_ (uint32_t) for HandleScope header
  const size_t handle_scope_size = HandleScope::SizeOf(kArmPointerSize, ReferenceCount());

  size_t total_size = frame_data_size;
  if (LIKELY(HasHandleScope())) {
    // HandleScope is sometimes excluded.
    total_size += handle_scope_size;                                 // handle scope size
  }

  // Plus return value spill area size
  total_size += SizeOfReturnValue();

  return RoundUp(total_size, kStackAlignment);
}

size_t ArmJniCallingConvention::OutArgSize() {
  // TODO: Identical to x86_64 except for also adding additional padding.
  return RoundUp(NumberOfOutgoingStackArgs() * kFramePointerSize + padding_,
                 kStackAlignment);
}

ArrayRef<const ManagedRegister> ArmJniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

// JniCallingConvention ABI follows AAPCS where longs and doubles must occur
// in even register numbers and stack slots
void ArmJniCallingConvention::Next() {
  // Update the iterator by usual JNI rules.
  JniCallingConvention::Next();

  if (LIKELY(HasNext())) {  // Avoid CHECK failure for IsCurrentParam
    // Ensure slot is 8-byte aligned for longs/doubles (AAPCS).
    if (IsCurrentParamALongOrDouble() && ((itr_slots_ & 0x1u) != 0)) {
      // itr_slots_ needs to be an even number, according to AAPCS.
      itr_slots_++;
    }
  }
}

bool ArmJniCallingConvention::IsCurrentParamInRegister() {
  return itr_slots_ < kJniArgumentRegisterCount;
}

bool ArmJniCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

ManagedRegister ArmJniCallingConvention::CurrentParamRegister() {
  CHECK_LT(itr_slots_, kJniArgumentRegisterCount);
  if (IsCurrentParamALongOrDouble()) {
    // AAPCS 5.1.1 requires 64-bit values to be in a consecutive register pair:
    // "A double-word sized type is passed in two consecutive registers (e.g., r0 and r1, or r2 and
    // r3). The content of the registers is as if the value had been loaded from memory
    // representation with a single LDM instruction."
    if (itr_slots_ == 0u) {
      return ArmManagedRegister::FromRegisterPair(R0_R1);
    } else if (itr_slots_ == 2u) {
      return ArmManagedRegister::FromRegisterPair(R2_R3);
    } else {
      // The register can either be R0 (+R1) or R2 (+R3). Cannot be other values.
      LOG(FATAL) << "Invalid iterator register position for a long/double " << itr_args_;
      UNREACHABLE();
    }
  } else {
    // All other types can fit into one register.
    return ArmManagedRegister::FromCoreRegister(kJniArgumentRegisters[itr_slots_]);
  }
}

FrameOffset ArmJniCallingConvention::CurrentParamStackOffset() {
  CHECK_GE(itr_slots_, kJniArgumentRegisterCount);
  size_t offset =
      displacement_.Int32Value()
          - OutArgSize()
          + ((itr_slots_ - kJniArgumentRegisterCount) * kFramePointerSize);
  CHECK_LT(offset, OutArgSize());
  return FrameOffset(offset);
}

size_t ArmJniCallingConvention::NumberOfOutgoingStackArgs() {
  size_t static_args = HasSelfClass() ? 1 : 0;  // count jclass
  // regular argument parameters and this
  size_t param_args = NumArgs() + NumLongOrDoubleArgs();  // twice count 8-byte args
  // XX: Why is the long/ordouble counted twice but not JNIEnv* ???
  // count JNIEnv* less arguments in registers
  size_t internal_args = (HasJniEnv() ? 1 : 0 /* jni env */);
  size_t total_args = static_args + param_args + internal_args;

  return total_args - std::min(kJniArgumentRegisterCount, static_cast<size_t>(total_args));

  // TODO: Very similar to x86_64 except for the return pc.
}

}  // namespace arm
}  // namespace art
