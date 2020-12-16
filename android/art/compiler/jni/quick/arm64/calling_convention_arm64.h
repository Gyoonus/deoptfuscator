/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_JNI_QUICK_ARM64_CALLING_CONVENTION_ARM64_H_
#define ART_COMPILER_JNI_QUICK_ARM64_CALLING_CONVENTION_ARM64_H_

#include "base/enums.h"
#include "jni/quick/calling_convention.h"

namespace art {
namespace arm64 {

constexpr size_t kFramePointerSize = static_cast<size_t>(PointerSize::k64);

class Arm64ManagedRuntimeCallingConvention FINAL : public ManagedRuntimeCallingConvention {
 public:
  Arm64ManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, const char* shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k64) {}
  ~Arm64ManagedRuntimeCallingConvention() OVERRIDE {}
  // Calling convention
  ManagedRegister ReturnRegister() OVERRIDE;
  ManagedRegister InterproceduralScratchRegister() OVERRIDE;
  // Managed runtime calling convention
  ManagedRegister MethodRegister() OVERRIDE;
  bool IsCurrentParamInRegister() OVERRIDE;
  bool IsCurrentParamOnStack() OVERRIDE;
  ManagedRegister CurrentParamRegister() OVERRIDE;
  FrameOffset CurrentParamStackOffset() OVERRIDE;
  const ManagedRegisterEntrySpills& EntrySpills() OVERRIDE;

 private:
  ManagedRegisterEntrySpills entry_spills_;

  DISALLOW_COPY_AND_ASSIGN(Arm64ManagedRuntimeCallingConvention);
};

class Arm64JniCallingConvention FINAL : public JniCallingConvention {
 public:
  Arm64JniCallingConvention(bool is_static,
                            bool is_synchronized,
                            bool is_critical_native,
                            const char* shorty);
  ~Arm64JniCallingConvention() OVERRIDE {}
  // Calling convention
  ManagedRegister ReturnRegister() OVERRIDE;
  ManagedRegister IntReturnRegister() OVERRIDE;
  ManagedRegister InterproceduralScratchRegister() OVERRIDE;
  // JNI calling convention
  size_t FrameSize() OVERRIDE;
  size_t OutArgSize() OVERRIDE;
  ArrayRef<const ManagedRegister> CalleeSaveRegisters() const OVERRIDE;
  ManagedRegister ReturnScratchRegister() const OVERRIDE;
  uint32_t CoreSpillMask() const OVERRIDE;
  uint32_t FpSpillMask() const OVERRIDE;
  bool IsCurrentParamInRegister() OVERRIDE;
  bool IsCurrentParamOnStack() OVERRIDE;
  ManagedRegister CurrentParamRegister() OVERRIDE;
  FrameOffset CurrentParamStackOffset() OVERRIDE;

  // aarch64 calling convention leaves upper bits undefined.
  bool RequiresSmallResultTypeExtension() const OVERRIDE {
    return true;
  }

 protected:
  size_t NumberOfOutgoingStackArgs() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(Arm64JniCallingConvention);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_ARM64_CALLING_CONVENTION_ARM64_H_
