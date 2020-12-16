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

#ifndef ART_COMPILER_UTILS_ARM_JNI_MACRO_ASSEMBLER_ARM_VIXL_H_
#define ART_COMPILER_UTILS_ARM_JNI_MACRO_ASSEMBLER_ARM_VIXL_H_

#include <android-base/logging.h>

#include "base/arena_containers.h"
#include "base/macros.h"
#include "constants_arm.h"
#include "offsets.h"
#include "utils/arm/assembler_arm_shared.h"
#include "utils/arm/assembler_arm_vixl.h"
#include "utils/arm/managed_register_arm.h"
#include "utils/assembler.h"
#include "utils/jni_macro_assembler.h"

namespace art {
namespace arm {

class ArmVIXLJNIMacroAssembler FINAL
    : public JNIMacroAssemblerFwd<ArmVIXLAssembler, PointerSize::k32> {
 private:
  class ArmException;
 public:
  explicit ArmVIXLJNIMacroAssembler(ArenaAllocator* allocator)
      : JNIMacroAssemblerFwd(allocator),
        exception_blocks_(allocator->Adapter(kArenaAllocAssembler)) {}

  virtual ~ArmVIXLJNIMacroAssembler() {}
  void FinalizeCode() OVERRIDE;

  //
  // Overridden common assembler high-level functionality
  //

  // Emit code that will create an activation on the stack.
  void BuildFrame(size_t frame_size,
                  ManagedRegister method_reg,
                  ArrayRef<const ManagedRegister> callee_save_regs,
                  const ManagedRegisterEntrySpills& entry_spills) OVERRIDE;

  // Emit code that will remove an activation from the stack.
  void RemoveFrame(size_t frame_size,
                   ArrayRef<const ManagedRegister> callee_save_regs,
                   bool may_suspend) OVERRIDE;

  void IncreaseFrameSize(size_t adjust) OVERRIDE;
  void DecreaseFrameSize(size_t adjust) OVERRIDE;

  // Store routines.
  void Store(FrameOffset offs, ManagedRegister src, size_t size) OVERRIDE;
  void StoreRef(FrameOffset dest, ManagedRegister src) OVERRIDE;
  void StoreRawPtr(FrameOffset dest, ManagedRegister src) OVERRIDE;

  void StoreImmediateToFrame(FrameOffset dest, uint32_t imm, ManagedRegister scratch) OVERRIDE;

  void StoreStackOffsetToThread(ThreadOffset32 thr_offs,
                                FrameOffset fr_offs,
                                ManagedRegister scratch) OVERRIDE;

  void StoreStackPointerToThread(ThreadOffset32 thr_offs) OVERRIDE;

  void StoreSpanning(FrameOffset dest,
                     ManagedRegister src,
                     FrameOffset in_off,
                     ManagedRegister scratch) OVERRIDE;

  // Load routines.
  void Load(ManagedRegister dest, FrameOffset src, size_t size) OVERRIDE;

  void LoadFromThread(ManagedRegister dest,
                      ThreadOffset32 src,
                      size_t size) OVERRIDE;

  void LoadRef(ManagedRegister dest, FrameOffset src) OVERRIDE;

  void LoadRef(ManagedRegister dest,
               ManagedRegister base,
               MemberOffset offs,
               bool unpoison_reference) OVERRIDE;

  void LoadRawPtr(ManagedRegister dest, ManagedRegister base, Offset offs) OVERRIDE;

  void LoadRawPtrFromThread(ManagedRegister dest, ThreadOffset32 offs) OVERRIDE;

  // Copying routines.
  void Move(ManagedRegister dest, ManagedRegister src, size_t size) OVERRIDE;

  void CopyRawPtrFromThread(FrameOffset fr_offs,
                            ThreadOffset32 thr_offs,
                            ManagedRegister scratch) OVERRIDE;

  void CopyRawPtrToThread(ThreadOffset32 thr_offs,
                          FrameOffset fr_offs,
                          ManagedRegister scratch) OVERRIDE;

  void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister scratch) OVERRIDE;

  void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            ManagedRegister src_base,
            Offset src_offset,
            ManagedRegister scratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest_base,
            Offset dest_offset,
            FrameOffset src,
            ManagedRegister scratch,
            size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            FrameOffset src_base,
            Offset src_offset,
            ManagedRegister scratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest,
            Offset dest_offset,
            ManagedRegister src,
            Offset src_offset,
            ManagedRegister scratch,
            size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            Offset dest_offset,
            FrameOffset src,
            Offset src_offset,
            ManagedRegister scratch,
            size_t size) OVERRIDE;

  // Sign extension.
  void SignExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Zero extension.
  void ZeroExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Exploit fast access in managed code to Thread::Current().
  void GetCurrentThread(ManagedRegister mtr) OVERRIDE;
  void GetCurrentThread(FrameOffset dest_offset,
                        ManagedRegister scratch) OVERRIDE;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  void CreateHandleScopeEntry(ManagedRegister out_reg,
                              FrameOffset handlescope_offset,
                              ManagedRegister in_reg,
                              bool null_allowed) OVERRIDE;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  void CreateHandleScopeEntry(FrameOffset out_off,
                              FrameOffset handlescope_offset,
                              ManagedRegister scratch,
                              bool null_allowed) OVERRIDE;

  // src holds a handle scope entry (Object**) load this into dst.
  void LoadReferenceFromHandleScope(ManagedRegister dst,
                                    ManagedRegister src) OVERRIDE;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  void VerifyObject(ManagedRegister src, bool could_be_null) OVERRIDE;
  void VerifyObject(FrameOffset src, bool could_be_null) OVERRIDE;

  // Call to address held at [base+offset].
  void Call(ManagedRegister base, Offset offset, ManagedRegister scratch) OVERRIDE;
  void Call(FrameOffset base, Offset offset, ManagedRegister scratch) OVERRIDE;
  void CallFromThread(ThreadOffset32 offset, ManagedRegister scratch) OVERRIDE;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust);

  // Create a new label that can be used with Jump/Bind calls.
  std::unique_ptr<JNIMacroLabel> CreateLabel() OVERRIDE;
  // Emit an unconditional jump to the label.
  void Jump(JNIMacroLabel* label) OVERRIDE;
  // Emit a conditional jump to the label by applying a unary condition test to the register.
  void Jump(JNIMacroLabel* label, JNIMacroUnaryCondition cond, ManagedRegister test) OVERRIDE;
  // Code at this offset will serve as the target for the Jump call.
  void Bind(JNIMacroLabel* label) OVERRIDE;

  void MemoryBarrier(ManagedRegister scratch) OVERRIDE;

  void EmitExceptionPoll(ArmVIXLJNIMacroAssembler::ArmException *exception);
  void Load(ArmManagedRegister dest, vixl32::Register base, int32_t offset, size_t size);

 private:
  class ArmException {
   private:
    ArmException(ArmManagedRegister scratch, size_t stack_adjust)
        : scratch_(scratch), stack_adjust_(stack_adjust) {}

    vixl32::Label* Entry() { return &exception_entry_; }

    // Register used for passing Thread::Current()->exception_ .
    const ArmManagedRegister scratch_;

    // Stack adjust for ExceptionPool.
    const size_t stack_adjust_;

    vixl32::Label exception_entry_;

    friend class ArmVIXLJNIMacroAssembler;
    DISALLOW_COPY_AND_ASSIGN(ArmException);
  };

  // List of exception blocks to generate at the end of the code cache.
  ArenaVector<std::unique_ptr<ArmVIXLJNIMacroAssembler::ArmException>> exception_blocks_;
  // Used for testing.
  friend class ArmVIXLAssemblerTest_VixlLoadFromOffset_Test;
  friend class ArmVIXLAssemblerTest_VixlStoreToOffset_Test;
};

class ArmVIXLJNIMacroLabel FINAL
    : public JNIMacroLabelCommon<ArmVIXLJNIMacroLabel,
                                 vixl32::Label,
                                 InstructionSet::kArm> {
 public:
  vixl32::Label* AsArm() {
    return AsPlatformLabel();
  }
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_JNI_MACRO_ASSEMBLER_ARM_VIXL_H_
