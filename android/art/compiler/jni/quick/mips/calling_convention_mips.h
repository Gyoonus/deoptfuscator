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

#ifndef ART_COMPILER_JNI_QUICK_MIPS_CALLING_CONVENTION_MIPS_H_
#define ART_COMPILER_JNI_QUICK_MIPS_CALLING_CONVENTION_MIPS_H_

#include "base/enums.h"
#include "jni/quick/calling_convention.h"

namespace art {
namespace mips {

constexpr size_t kFramePointerSize = 4;
static_assert(kFramePointerSize == static_cast<size_t>(PointerSize::k32),
              "Invalid frame pointer size");

class MipsManagedRuntimeCallingConvention FINAL : public ManagedRuntimeCallingConvention {
 public:
  MipsManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, const char* shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k32) {}
  ~MipsManagedRuntimeCallingConvention() OVERRIDE {}
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

  DISALLOW_COPY_AND_ASSIGN(MipsManagedRuntimeCallingConvention);
};

class MipsJniCallingConvention FINAL : public JniCallingConvention {
 public:
  MipsJniCallingConvention(bool is_static,
                           bool is_synchronized,
                           bool is_critical_native,
                           const char* shorty);
  ~MipsJniCallingConvention() OVERRIDE {}
  // Calling convention
  ManagedRegister ReturnRegister() OVERRIDE;
  ManagedRegister IntReturnRegister() OVERRIDE;
  ManagedRegister InterproceduralScratchRegister() OVERRIDE;
  // JNI calling convention
  void Next() OVERRIDE;  // Override default behavior for o32.
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

  // Mips does not need to extend small return types.
  bool RequiresSmallResultTypeExtension() const OVERRIDE {
    return false;
  }

 protected:
  size_t NumberOfOutgoingStackArgs() OVERRIDE;

 private:
  // Padding to ensure longs and doubles are not split in o32.
  size_t padding_;
  size_t use_fp_arg_registers_;

  DISALLOW_COPY_AND_ASSIGN(MipsJniCallingConvention);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_MIPS_CALLING_CONVENTION_MIPS_H_
