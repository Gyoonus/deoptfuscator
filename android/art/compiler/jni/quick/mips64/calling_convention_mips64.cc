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

#include "calling_convention_mips64.h"

#include <android-base/logging.h>

#include "handle_scope-inl.h"
#include "utils/mips64/managed_register_mips64.h"

namespace art {
namespace mips64 {

// Up to kow many args can be enregistered. The rest of the args must go on the stack.
constexpr size_t kMaxRegisterArguments = 8u;

static const GpuRegister kGpuArgumentRegisters[] = {
  A0, A1, A2, A3, A4, A5, A6, A7
};

static const FpuRegister kFpuArgumentRegisters[] = {
  F12, F13, F14, F15, F16, F17, F18, F19
};

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    Mips64ManagedRegister::FromGpuRegister(S2),
    Mips64ManagedRegister::FromGpuRegister(S3),
    Mips64ManagedRegister::FromGpuRegister(S4),
    Mips64ManagedRegister::FromGpuRegister(S5),
    Mips64ManagedRegister::FromGpuRegister(S6),
    Mips64ManagedRegister::FromGpuRegister(S7),
    Mips64ManagedRegister::FromGpuRegister(GP),
    Mips64ManagedRegister::FromGpuRegister(S8),
    // No hard float callee saves.
};

static constexpr uint32_t CalculateCoreCalleeSpillMask() {
  // RA is a special callee save which is not reported by CalleeSaveRegisters().
  uint32_t result = 1 << RA;
  for (auto&& r : kCalleeSaveRegisters) {
    if (r.AsMips64().IsGpuRegister()) {
      result |= (1 << r.AsMips64().AsGpuRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask();
static constexpr uint32_t kFpCalleeSpillMask = 0u;

// Calling convention
ManagedRegister Mips64ManagedRuntimeCallingConvention::InterproceduralScratchRegister() {
  return Mips64ManagedRegister::FromGpuRegister(T9);
}

ManagedRegister Mips64JniCallingConvention::InterproceduralScratchRegister() {
  return Mips64ManagedRegister::FromGpuRegister(T9);
}

static ManagedRegister ReturnRegisterForShorty(const char* shorty) {
  if (shorty[0] == 'F' || shorty[0] == 'D') {
    return Mips64ManagedRegister::FromFpuRegister(F0);
  } else if (shorty[0] == 'V') {
    return Mips64ManagedRegister::NoRegister();
  } else {
    return Mips64ManagedRegister::FromGpuRegister(V0);
  }
}

ManagedRegister Mips64ManagedRuntimeCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister Mips64JniCallingConvention::ReturnRegister() {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister Mips64JniCallingConvention::IntReturnRegister() {
  return Mips64ManagedRegister::FromGpuRegister(V0);
}

// Managed runtime calling convention

ManagedRegister Mips64ManagedRuntimeCallingConvention::MethodRegister() {
  return Mips64ManagedRegister::FromGpuRegister(A0);
}

bool Mips64ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  return false;  // Everything moved to stack on entry.
}

bool Mips64ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return true;
}

ManagedRegister Mips64ManagedRuntimeCallingConvention::CurrentParamRegister() {
  LOG(FATAL) << "Should not reach here";
  return ManagedRegister::NoRegister();
}

FrameOffset Mips64ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  FrameOffset result =
      FrameOffset(displacement_.Int32Value() +  // displacement
                  kFramePointerSize +  // Method ref
                  (itr_slots_ * sizeof(uint32_t)));  // offset into in args
  return result;
}

const ManagedRegisterEntrySpills& Mips64ManagedRuntimeCallingConvention::EntrySpills() {
  // We spill the argument registers on MIPS64 to free them up for scratch use,
  // we then assume all arguments are on the stack.
  if ((entry_spills_.size() == 0) && (NumArgs() > 0)) {
    int reg_index = 1;   // we start from A1, A0 holds ArtMethod*.

    // We need to choose the correct register size since the managed
    // stack uses 32bit stack slots.
    ResetIterator(FrameOffset(0));
    while (HasNext()) {
      if (reg_index < 8) {
        if (IsCurrentParamAFloatOrDouble()) {  // FP regs.
          FpuRegister arg = kFpuArgumentRegisters[reg_index];
          Mips64ManagedRegister reg = Mips64ManagedRegister::FromFpuRegister(arg);
          entry_spills_.push_back(reg, IsCurrentParamADouble() ? 8 : 4);
        } else {  // GP regs.
          GpuRegister arg = kGpuArgumentRegisters[reg_index];
          Mips64ManagedRegister reg = Mips64ManagedRegister::FromGpuRegister(arg);
          entry_spills_.push_back(reg,
                                  (IsCurrentParamALong() && (!IsCurrentParamAReference())) ? 8 : 4);
        }
        // e.g. A1, A2, F3, A4, F5, F6, A7
        reg_index++;
      }

      Next();
    }
  }
  return entry_spills_;
}

// JNI calling convention

Mips64JniCallingConvention::Mips64JniCallingConvention(bool is_static,
                                                       bool is_synchronized,
                                                       bool is_critical_native,
                                                       const char* shorty)
    : JniCallingConvention(is_static,
                           is_synchronized,
                           is_critical_native,
                           shorty,
                           kMips64PointerSize) {
}

uint32_t Mips64JniCallingConvention::CoreSpillMask() const {
  return kCoreCalleeSpillMask;
}

uint32_t Mips64JniCallingConvention::FpSpillMask() const {
  return kFpCalleeSpillMask;
}

ManagedRegister Mips64JniCallingConvention::ReturnScratchRegister() const {
  return Mips64ManagedRegister::FromGpuRegister(AT);
}

size_t Mips64JniCallingConvention::FrameSize() {
  // ArtMethod*, RA and callee save area size, local reference segment state.
  size_t method_ptr_size = static_cast<size_t>(kFramePointerSize);
  size_t ra_and_callee_save_area_size = (CalleeSaveRegisters().size() + 1) * kFramePointerSize;

  size_t frame_data_size = method_ptr_size + ra_and_callee_save_area_size;
  if (LIKELY(HasLocalReferenceSegmentState())) {                     // Local ref. segment state.
    // Local reference segment state is sometimes excluded.
    frame_data_size += sizeof(uint32_t);
  }
  // References plus 2 words for HandleScope header.
  size_t handle_scope_size = HandleScope::SizeOf(kMips64PointerSize, ReferenceCount());

  size_t total_size = frame_data_size;
  if (LIKELY(HasHandleScope())) {
    // HandleScope is sometimes excluded.
    total_size += handle_scope_size;                                 // Handle scope size.
  }

  // Plus return value spill area size.
  total_size += SizeOfReturnValue();

  return RoundUp(total_size, kStackAlignment);
}

size_t Mips64JniCallingConvention::OutArgSize() {
  return RoundUp(NumberOfOutgoingStackArgs() * kFramePointerSize, kStackAlignment);
}

ArrayRef<const ManagedRegister> Mips64JniCallingConvention::CalleeSaveRegisters() const {
  return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
}

bool Mips64JniCallingConvention::IsCurrentParamInRegister() {
  return itr_args_ < kMaxRegisterArguments;
}

bool Mips64JniCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

ManagedRegister Mips64JniCallingConvention::CurrentParamRegister() {
  CHECK(IsCurrentParamInRegister());
  if (IsCurrentParamAFloatOrDouble()) {
    return Mips64ManagedRegister::FromFpuRegister(kFpuArgumentRegisters[itr_args_]);
  } else {
    return Mips64ManagedRegister::FromGpuRegister(kGpuArgumentRegisters[itr_args_]);
  }
}

FrameOffset Mips64JniCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  size_t args_on_stack = itr_args_ - kMaxRegisterArguments;
  size_t offset = displacement_.Int32Value() - OutArgSize() + (args_on_stack * kFramePointerSize);
  CHECK_LT(offset, OutArgSize());
  return FrameOffset(offset);
}

size_t Mips64JniCallingConvention::NumberOfOutgoingStackArgs() {
  // all arguments including JNI args
  size_t all_args = NumArgs() + NumberOfExtraArgumentsForJni();

  // Nothing on the stack unless there are more than 8 arguments
  return (all_args > kMaxRegisterArguments) ? all_args - kMaxRegisterArguments : 0;
}
}  // namespace mips64
}  // namespace art
