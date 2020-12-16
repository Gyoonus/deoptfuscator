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

#ifndef ART_COMPILER_UTILS_JNI_MACRO_ASSEMBLER_H_
#define ART_COMPILER_UTILS_JNI_MACRO_ASSEMBLER_H_

#include <vector>

#include <android-base/logging.h>

#include "arch/instruction_set.h"
#include "base/arena_allocator.h"
#include "base/arena_object.h"
#include "base/array_ref.h"
#include "base/enums.h"
#include "base/macros.h"
#include "managed_register.h"
#include "offsets.h"

namespace art {

class ArenaAllocator;
class DebugFrameOpCodeWriterForAssembler;
class InstructionSetFeatures;
class MemoryRegion;
class JNIMacroLabel;

enum class JNIMacroUnaryCondition {
  kZero,
  kNotZero
};

template <PointerSize kPointerSize>
class JNIMacroAssembler : public DeletableArenaObject<kArenaAllocAssembler> {
 public:
  static std::unique_ptr<JNIMacroAssembler<kPointerSize>> Create(
      ArenaAllocator* allocator,
      InstructionSet instruction_set,
      const InstructionSetFeatures* instruction_set_features = nullptr);

  // Finalize the code; emit slow paths, fixup branches, add literal pool, etc.
  virtual void FinalizeCode() = 0;

  // Size of generated code
  virtual size_t CodeSize() const = 0;

  // Copy instructions out of assembly buffer into the given region of memory
  virtual void FinalizeInstructions(const MemoryRegion& region) = 0;

  // Emit code that will create an activation on the stack
  virtual void BuildFrame(size_t frame_size,
                          ManagedRegister method_reg,
                          ArrayRef<const ManagedRegister> callee_save_regs,
                          const ManagedRegisterEntrySpills& entry_spills) = 0;

  // Emit code that will remove an activation from the stack
  //
  // Argument `may_suspend` must be `true` if the compiled method may be
  // suspended during its execution (otherwise `false`, if it is impossible
  // to suspend during its execution).
  virtual void RemoveFrame(size_t frame_size,
                           ArrayRef<const ManagedRegister> callee_save_regs,
                           bool may_suspend) = 0;

  virtual void IncreaseFrameSize(size_t adjust) = 0;
  virtual void DecreaseFrameSize(size_t adjust) = 0;

  // Store routines
  virtual void Store(FrameOffset offs, ManagedRegister src, size_t size) = 0;
  virtual void StoreRef(FrameOffset dest, ManagedRegister src) = 0;
  virtual void StoreRawPtr(FrameOffset dest, ManagedRegister src) = 0;

  virtual void StoreImmediateToFrame(FrameOffset dest, uint32_t imm, ManagedRegister scratch) = 0;

  virtual void StoreStackOffsetToThread(ThreadOffset<kPointerSize> thr_offs,
                                        FrameOffset fr_offs,
                                        ManagedRegister scratch) = 0;

  virtual void StoreStackPointerToThread(ThreadOffset<kPointerSize> thr_offs) = 0;

  virtual void StoreSpanning(FrameOffset dest,
                             ManagedRegister src,
                             FrameOffset in_off,
                             ManagedRegister scratch) = 0;

  // Load routines
  virtual void Load(ManagedRegister dest, FrameOffset src, size_t size) = 0;

  virtual void LoadFromThread(ManagedRegister dest,
                              ThreadOffset<kPointerSize> src,
                              size_t size) = 0;

  virtual void LoadRef(ManagedRegister dest, FrameOffset src) = 0;
  // If unpoison_reference is true and kPoisonReference is true, then we negate the read reference.
  virtual void LoadRef(ManagedRegister dest,
                       ManagedRegister base,
                       MemberOffset offs,
                       bool unpoison_reference) = 0;

  virtual void LoadRawPtr(ManagedRegister dest, ManagedRegister base, Offset offs) = 0;

  virtual void LoadRawPtrFromThread(ManagedRegister dest, ThreadOffset<kPointerSize> offs) = 0;

  // Copying routines
  virtual void Move(ManagedRegister dest, ManagedRegister src, size_t size) = 0;

  virtual void CopyRawPtrFromThread(FrameOffset fr_offs,
                                    ThreadOffset<kPointerSize> thr_offs,
                                    ManagedRegister scratch) = 0;

  virtual void CopyRawPtrToThread(ThreadOffset<kPointerSize> thr_offs,
                                  FrameOffset fr_offs,
                                  ManagedRegister scratch) = 0;

  virtual void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister scratch) = 0;

  virtual void Copy(FrameOffset dest, FrameOffset src, ManagedRegister scratch, size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    ManagedRegister src_base,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(ManagedRegister dest_base,
                    Offset dest_offset,
                    FrameOffset src,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    FrameOffset src_base,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(ManagedRegister dest,
                    Offset dest_offset,
                    ManagedRegister src,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void Copy(FrameOffset dest,
                    Offset dest_offset,
                    FrameOffset src,
                    Offset src_offset,
                    ManagedRegister scratch,
                    size_t size) = 0;

  virtual void MemoryBarrier(ManagedRegister scratch) = 0;

  // Sign extension
  virtual void SignExtend(ManagedRegister mreg, size_t size) = 0;

  // Zero extension
  virtual void ZeroExtend(ManagedRegister mreg, size_t size) = 0;

  // Exploit fast access in managed code to Thread::Current()
  virtual void GetCurrentThread(ManagedRegister tr) = 0;
  virtual void GetCurrentThread(FrameOffset dest_offset, ManagedRegister scratch) = 0;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  virtual void CreateHandleScopeEntry(ManagedRegister out_reg,
                                      FrameOffset handlescope_offset,
                                      ManagedRegister in_reg,
                                      bool null_allowed) = 0;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  virtual void CreateHandleScopeEntry(FrameOffset out_off,
                                      FrameOffset handlescope_offset,
                                      ManagedRegister scratch,
                                      bool null_allowed) = 0;

  // src holds a handle scope entry (Object**) load this into dst
  virtual void LoadReferenceFromHandleScope(ManagedRegister dst, ManagedRegister src) = 0;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  virtual void VerifyObject(ManagedRegister src, bool could_be_null) = 0;
  virtual void VerifyObject(FrameOffset src, bool could_be_null) = 0;

  // Call to address held at [base+offset]
  virtual void Call(ManagedRegister base, Offset offset, ManagedRegister scratch) = 0;
  virtual void Call(FrameOffset base, Offset offset, ManagedRegister scratch) = 0;
  virtual void CallFromThread(ThreadOffset<kPointerSize> offset, ManagedRegister scratch) = 0;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  virtual void ExceptionPoll(ManagedRegister scratch, size_t stack_adjust) = 0;

  // Create a new label that can be used with Jump/Bind calls.
  virtual std::unique_ptr<JNIMacroLabel> CreateLabel() = 0;
  // Emit an unconditional jump to the label.
  virtual void Jump(JNIMacroLabel* label) = 0;
  // Emit a conditional jump to the label by applying a unary condition test to the register.
  virtual void Jump(JNIMacroLabel* label, JNIMacroUnaryCondition cond, ManagedRegister test) = 0;
  // Code at this offset will serve as the target for the Jump call.
  virtual void Bind(JNIMacroLabel* label) = 0;

  virtual ~JNIMacroAssembler() {}

  /**
   * @brief Buffer of DWARF's Call Frame Information opcodes.
   * @details It is used by debuggers and other tools to unwind the call stack.
   */
  virtual DebugFrameOpCodeWriterForAssembler& cfi() = 0;

  void SetEmitRunTimeChecksInDebugMode(bool value) {
    emit_run_time_checks_in_debug_mode_ = value;
  }

 protected:
  JNIMacroAssembler() {}

  // Should run-time checks be emitted in debug mode?
  bool emit_run_time_checks_in_debug_mode_ = false;
};

// A "Label" class used with the JNIMacroAssembler
// allowing one to use branches (jumping from one place to another).
//
// This is just an interface, so every platform must provide
// its own implementation of it.
//
// It is only safe to use a label created
// via JNIMacroAssembler::CreateLabel with that same macro assembler.
class JNIMacroLabel {
 public:
  virtual ~JNIMacroLabel() = 0;

  const InstructionSet isa_;
 protected:
  explicit JNIMacroLabel(InstructionSet isa) : isa_(isa) {}
};

inline JNIMacroLabel::~JNIMacroLabel() {
  // Compulsory definition for a pure virtual destructor
  // to avoid linking errors.
}

template <typename T, PointerSize kPointerSize>
class JNIMacroAssemblerFwd : public JNIMacroAssembler<kPointerSize> {
 public:
  void FinalizeCode() OVERRIDE {
    asm_.FinalizeCode();
  }

  size_t CodeSize() const OVERRIDE {
    return asm_.CodeSize();
  }

  void FinalizeInstructions(const MemoryRegion& region) OVERRIDE {
    asm_.FinalizeInstructions(region);
  }

  DebugFrameOpCodeWriterForAssembler& cfi() OVERRIDE {
    return asm_.cfi();
  }

 protected:
  explicit JNIMacroAssemblerFwd(ArenaAllocator* allocator) : asm_(allocator) {}

  T asm_;
};

template <typename Self, typename PlatformLabel, InstructionSet kIsa>
class JNIMacroLabelCommon : public JNIMacroLabel {
 public:
  static Self* Cast(JNIMacroLabel* label) {
    CHECK(label != nullptr);
    CHECK_EQ(kIsa, label->isa_);

    return reinterpret_cast<Self*>(label);
  }

 protected:
  PlatformLabel* AsPlatformLabel() {
    return &label_;
  }

  JNIMacroLabelCommon() : JNIMacroLabel(kIsa) {
  }

  virtual ~JNIMacroLabelCommon() OVERRIDE {}

 private:
  PlatformLabel label_;
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_JNI_MACRO_ASSEMBLER_H_
