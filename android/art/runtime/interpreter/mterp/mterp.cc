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

/*
 * Mterp entry point and support functions.
 */
#include "mterp.h"

#include "base/quasi_atomic.h"
#include "debugger.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "interpreter/interpreter_common.h"
#include "interpreter/interpreter_intrinsics.h"

namespace art {
namespace interpreter {
/*
 * Verify some constants used by the mterp interpreter.
 */
void CheckMterpAsmConstants() {
  /*
   * If we're using computed goto instruction transitions, make sure
   * none of the handlers overflows the 128-byte limit.  This won't tell
   * which one did, but if any one is too big the total size will
   * overflow.
   */
  const int width = 128;
  int interp_size = (uintptr_t) artMterpAsmInstructionEnd -
                    (uintptr_t) artMterpAsmInstructionStart;
  if ((interp_size == 0) || (interp_size != (art::kNumPackedOpcodes * width))) {
      LOG(FATAL) << "ERROR: unexpected asm interp size " << interp_size
                 << "(did an instruction handler exceed " << width << " bytes?)";
  }
}

void InitMterpTls(Thread* self) {
  self->SetMterpDefaultIBase(artMterpAsmInstructionStart);
  self->SetMterpAltIBase(artMterpAsmAltInstructionStart);
  self->SetMterpCurrentIBase((kTraceExecutionEnabled || kTestExportPC) ?
                             artMterpAsmAltInstructionStart :
                             artMterpAsmInstructionStart);
}

/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the sparse-switch
 * instruction).
 */
extern "C" ssize_t MterpDoSparseSwitch(const uint16_t* switchData, int32_t testVal) {
  const int kInstrLen = 3;
  uint16_t size;
  const int32_t* keys;
  const int32_t* entries;

  /*
   * Sparse switch data format:
   *  ushort ident = 0x0200   magic value
   *  ushort size             number of entries in the table; > 0
   *  int keys[size]          keys, sorted low-to-high; 32-bit aligned
   *  int targets[size]       branch targets, relative to switch opcode
   *
   * Total size is (2+size*4) 16-bit code units.
   */

  uint16_t signature = *switchData++;
  DCHECK_EQ(signature, static_cast<uint16_t>(art::Instruction::kSparseSwitchSignature));

  size = *switchData++;

  /* The keys are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  keys = reinterpret_cast<const int32_t*>(switchData);

  /* The entries are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  entries = keys + size;

  /*
   * Binary-search through the array of keys, which are guaranteed to
   * be sorted low-to-high.
   */
  int lo = 0;
  int hi = size - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;

    int32_t foundVal = keys[mid];
    if (testVal < foundVal) {
      hi = mid - 1;
    } else if (testVal > foundVal) {
      lo = mid + 1;
    } else {
      return entries[mid];
    }
  }
  return kInstrLen;
}

extern "C" ssize_t MterpDoPackedSwitch(const uint16_t* switchData, int32_t testVal) {
  const int kInstrLen = 3;

  /*
   * Packed switch data format:
   *  ushort ident = 0x0100   magic value
   *  ushort size             number of entries in the table
   *  int first_key           first (and lowest) switch case value
   *  int targets[size]       branch targets, relative to switch opcode
   *
   * Total size is (4+size*2) 16-bit code units.
   */
  uint16_t signature = *switchData++;
  DCHECK_EQ(signature, static_cast<uint16_t>(art::Instruction::kPackedSwitchSignature));

  uint16_t size = *switchData++;

  int32_t firstKey = *switchData++;
  firstKey |= (*switchData++) << 16;

  int index = testVal - firstKey;
  if (index < 0 || index >= size) {
    return kInstrLen;
  }

  /*
   * The entries are guaranteed to be aligned on a 32-bit boundary;
   * we can treat them as a native int array.
   */
  const int32_t* entries = reinterpret_cast<const int32_t*>(switchData);
  return entries[index];
}

extern "C" size_t MterpShouldSwitchInterpreters()
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Runtime* const runtime = Runtime::Current();
  const instrumentation::Instrumentation* const instrumentation = runtime->GetInstrumentation();
  return instrumentation->NonJitProfilingActive() ||
      Dbg::IsDebuggerActive() ||
      // An async exception has been thrown. We need to go to the switch interpreter. MTerp doesn't
      // know how to deal with these so we could end up never dealing with it if we are in an
      // infinite loop. Since this can be called in a tight loop and getting the current thread
      // requires a TLS read we instead first check a short-circuit runtime flag that will only be
      // set if something tries to set an async exception. This will make this function faster in
      // the common case where no async exception has ever been sent. We don't need to worry about
      // synchronization on the runtime flag since it is only set in a checkpoint which will either
      // take place on the current thread or act as a synchronization point.
      (UNLIKELY(runtime->AreAsyncExceptionsThrown()) &&
       Thread::Current()->IsAsyncExceptionPending());
}


extern "C" size_t MterpInvokeVirtual(Thread* self,
                                     ShadowFrame* shadow_frame,
                                     uint16_t* dex_pc_ptr,
                                     uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFastInvoke<kVirtual>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeSuper(Thread* self,
                                   ShadowFrame* shadow_frame,
                                   uint16_t* dex_pc_ptr,
                                   uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kSuper, false, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeInterface(Thread* self,
                                       ShadowFrame* shadow_frame,
                                       uint16_t* dex_pc_ptr,
                                       uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kInterface, false, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeDirect(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFastInvoke<kDirect>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeStatic(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFastInvoke<kStatic>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeCustom(Thread* self,
                                    ShadowFrame* shadow_frame,
                                    uint16_t* dex_pc_ptr,
                                    uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokeCustom<false /* is_range */>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokePolymorphic(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokePolymorphic<false /* is_range */>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualRange(Thread* self,
                                          ShadowFrame* shadow_frame,
                                          uint16_t* dex_pc_ptr,
                                          uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kVirtual, true, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeSuperRange(Thread* self,
                                        ShadowFrame* shadow_frame,
                                        uint16_t* dex_pc_ptr,
                                        uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kSuper, true, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeInterfaceRange(Thread* self,
                                            ShadowFrame* shadow_frame,
                                            uint16_t* dex_pc_ptr,
                                            uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kInterface, true, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeDirectRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kDirect, true, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeStaticRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvoke<kStatic, true, false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeCustomRange(Thread* self,
                                         ShadowFrame* shadow_frame,
                                         uint16_t* dex_pc_ptr,
                                         uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokeCustom<true /* is_range */>(self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokePolymorphicRange(Thread* self,
                                              ShadowFrame* shadow_frame,
                                              uint16_t* dex_pc_ptr,
                                              uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokePolymorphic<true /* is_range */>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualQuick(Thread* self,
                                          ShadowFrame* shadow_frame,
                                          uint16_t* dex_pc_ptr,
                                          uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  const uint32_t vregC = inst->VRegC_35c();
  const uint32_t vtable_idx = inst->VRegB_35c();
  ObjPtr<mirror::Object> const receiver = shadow_frame->GetVRegReference(vregC);
  if (receiver != nullptr) {
    ArtMethod* const called_method = receiver->GetClass()->GetEmbeddedVTableEntry(
        vtable_idx, kRuntimePointerSize);
    if ((called_method != nullptr) && called_method->IsIntrinsic()) {
      if (MterpHandleIntrinsic(shadow_frame, called_method, inst, inst_data, result_register)) {
        jit::Jit* jit = Runtime::Current()->GetJit();
        if (jit != nullptr) {
          jit->InvokeVirtualOrInterface(
              receiver, shadow_frame->GetMethod(), shadow_frame->GetDexPC(), called_method);
        }
        return !self->IsExceptionPending();
      }
    }
  }
  return DoInvokeVirtualQuick<false>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" size_t MterpInvokeVirtualQuickRange(Thread* self,
                                               ShadowFrame* shadow_frame,
                                               uint16_t* dex_pc_ptr,
                                               uint16_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  JValue* result_register = shadow_frame->GetResultRegister();
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoInvokeVirtualQuick<true>(
      self, *shadow_frame, inst, inst_data, result_register);
}

extern "C" void MterpThreadFenceForConstructor() {
  QuasiAtomic::ThreadFenceForConstructor();
}

extern "C" size_t MterpConstString(uint32_t index,
                                   uint32_t tgt_vreg,
                                   ShadowFrame* shadow_frame,
                                   Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::String> s = ResolveString(self, *shadow_frame, dex::StringIndex(index));
  if (UNLIKELY(s == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, s.Ptr());
  return false;
}

extern "C" size_t MterpConstClass(uint32_t index,
                                  uint32_t tgt_vreg,
                                  ShadowFrame* shadow_frame,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   shadow_frame->GetMethod(),
                                                   self,
                                                   /* can_run_clinit */ false,
                                                   /* verify_access */ false);
  if (UNLIKELY(c == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, c.Ptr());
  return false;
}

extern "C" size_t MterpConstMethodHandle(uint32_t index,
                                         uint32_t tgt_vreg,
                                         ShadowFrame* shadow_frame,
                                         Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::MethodHandle> mh = ResolveMethodHandle(self, index, shadow_frame->GetMethod());
  if (UNLIKELY(mh == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, mh.Ptr());
  return false;
}

extern "C" size_t MterpConstMethodType(uint32_t index,
                                       uint32_t tgt_vreg,
                                       ShadowFrame* shadow_frame,
                                       Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::MethodType> mt = ResolveMethodType(self, index, shadow_frame->GetMethod());
  if (UNLIKELY(mt == nullptr)) {
    return true;
  }
  shadow_frame->SetVRegReference(tgt_vreg, mt.Ptr());
  return false;
}

extern "C" size_t MterpCheckCast(uint32_t index,
                                 StackReference<mirror::Object>* vreg_addr,
                                 art::ArtMethod* method,
                                 Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   method,
                                                   self,
                                                   false,
                                                   false);
  if (UNLIKELY(c == nullptr)) {
    return true;
  }
  // Must load obj from vreg following ResolveVerifyAndClinit due to moving gc.
  mirror::Object* obj = vreg_addr->AsMirrorPtr();
  if (UNLIKELY(obj != nullptr && !obj->InstanceOf(c))) {
    ThrowClassCastException(c, obj->GetClass());
    return true;
  }
  return false;
}

extern "C" size_t MterpInstanceOf(uint32_t index,
                                  StackReference<mirror::Object>* vreg_addr,
                                  art::ArtMethod* method,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(index),
                                                   method,
                                                   self,
                                                   false,
                                                   false);
  if (UNLIKELY(c == nullptr)) {
    return false;  // Caller will check for pending exception.  Return value unimportant.
  }
  // Must load obj from vreg following ResolveVerifyAndClinit due to moving gc.
  mirror::Object* obj = vreg_addr->AsMirrorPtr();
  return (obj != nullptr) && obj->InstanceOf(c);
}

extern "C" size_t MterpFillArrayData(mirror::Object* obj, const Instruction::ArrayDataPayload* payload)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return FillArrayData(obj, payload);
}

extern "C" size_t MterpNewInstance(ShadowFrame* shadow_frame, Thread* self, uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  mirror::Object* obj = nullptr;
  ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegB_21c()),
                                                   shadow_frame->GetMethod(),
                                                   self,
                                                   /* can_run_clinit */ false,
                                                   /* verify_access */ false);
  if (LIKELY(c != nullptr)) {
    if (UNLIKELY(c->IsStringClass())) {
      gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
      obj = mirror::String::AllocEmptyString<true>(self, allocator_type);
    } else {
      obj = AllocObjectFromCode<true>(c.Ptr(),
                                      self,
                                      Runtime::Current()->GetHeap()->GetCurrentAllocator());
    }
  }
  if (UNLIKELY(obj == nullptr)) {
    return false;
  }
  obj->GetClass()->AssertInitializedOrInitializingInThread(self);
  shadow_frame->SetVRegReference(inst->VRegA_21c(inst_data), obj);
  return true;
}

extern "C" size_t MterpSputObject(ShadowFrame* shadow_frame, uint16_t* dex_pc_ptr,
                                uint32_t inst_data, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFieldPut<StaticObjectWrite, Primitive::kPrimNot, false, false>
      (self, *shadow_frame, inst, inst_data);
}

extern "C" size_t MterpIputObject(ShadowFrame* shadow_frame,
                                  uint16_t* dex_pc_ptr,
                                  uint32_t inst_data,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFieldPut<InstanceObjectWrite, Primitive::kPrimNot, false, false>
      (self, *shadow_frame, inst, inst_data);
}

extern "C" size_t MterpIputObjectQuick(ShadowFrame* shadow_frame,
                                       uint16_t* dex_pc_ptr,
                                       uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoIPutQuick<Primitive::kPrimNot, false>(*shadow_frame, inst, inst_data);
}

extern "C" size_t MterpAputObject(ShadowFrame* shadow_frame,
                                  uint16_t* dex_pc_ptr,
                                  uint32_t inst_data)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  mirror::Object* a = shadow_frame->GetVRegReference(inst->VRegB_23x());
  if (UNLIKELY(a == nullptr)) {
    return false;
  }
  int32_t index = shadow_frame->GetVReg(inst->VRegC_23x());
  mirror::Object* val = shadow_frame->GetVRegReference(inst->VRegA_23x(inst_data));
  mirror::ObjectArray<mirror::Object>* array = a->AsObjectArray<mirror::Object>();
  if (array->CheckIsValidIndex(index) && array->CheckAssignable(val)) {
    array->SetWithoutChecks<false>(index, val);
    return true;
  }
  return false;
}

extern "C" size_t MterpFilledNewArray(ShadowFrame* shadow_frame,
                                      uint16_t* dex_pc_ptr,
                                      Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFilledNewArray<false, false, false>(inst, *shadow_frame, self,
                                               shadow_frame->GetResultRegister());
}

extern "C" size_t MterpFilledNewArrayRange(ShadowFrame* shadow_frame,
                                           uint16_t* dex_pc_ptr,
                                           Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  return DoFilledNewArray<true, false, false>(inst, *shadow_frame, self,
                                              shadow_frame->GetResultRegister());
}

extern "C" size_t MterpNewArray(ShadowFrame* shadow_frame,
                                uint16_t* dex_pc_ptr,
                                uint32_t inst_data, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  int32_t length = shadow_frame->GetVReg(inst->VRegB_22c(inst_data));
  mirror::Object* obj = AllocArrayFromCode<false, true>(
      dex::TypeIndex(inst->VRegC_22c()), length, shadow_frame->GetMethod(), self,
      Runtime::Current()->GetHeap()->GetCurrentAllocator());
  if (UNLIKELY(obj == nullptr)) {
      return false;
  }
  shadow_frame->SetVRegReference(inst->VRegA_22c(inst_data), obj);
  return true;
}

extern "C" size_t MterpHandleException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(self->IsExceptionPending());
  const instrumentation::Instrumentation* const instrumentation =
      Runtime::Current()->GetInstrumentation();
  return MoveToExceptionHandler(self, *shadow_frame, instrumentation);
}

extern "C" void MterpCheckBefore(Thread* self, ShadowFrame* shadow_frame, uint16_t* dex_pc_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const Instruction* inst = Instruction::At(dex_pc_ptr);
  uint16_t inst_data = inst->Fetch16(0);
  if (inst->Opcode(inst_data) == Instruction::MOVE_EXCEPTION) {
    self->AssertPendingException();
  } else {
    self->AssertNoPendingException();
  }
  if (kTraceExecutionEnabled) {
    uint32_t dex_pc = dex_pc_ptr - shadow_frame->GetDexInstructions();
    TraceExecution(*shadow_frame, inst, dex_pc);
  }
  if (kTestExportPC) {
    // Save invalid dex pc to force segfault if improperly used.
    shadow_frame->SetDexPCPtr(reinterpret_cast<uint16_t*>(kExportPCPoison));
  }
}

extern "C" void MterpLogDivideByZeroException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "DivideByZero: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogArrayIndexException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "ArrayIndex: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNegativeArraySizeException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NegativeArraySize: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNoSuchMethodException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NoSuchMethod: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogExceptionThrownException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "ExceptionThrown: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogNullObjectException(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "NullObject: " << inst->Opcode(inst_data);
}

extern "C" void MterpLogFallback(Thread* self, ShadowFrame* shadow_frame)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "Fallback: " << inst->Opcode(inst_data) << ", Suspend Pending?: "
            << self->IsExceptionPending();
}

extern "C" void MterpLogOSR(Thread* self, ShadowFrame* shadow_frame, int32_t offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  LOG(INFO) << "OSR: " << inst->Opcode(inst_data) << ", offset = " << offset;
}

extern "C" void MterpLogSuspendFallback(Thread* self, ShadowFrame* shadow_frame, uint32_t flags)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  UNUSED(self);
  const Instruction* inst = Instruction::At(shadow_frame->GetDexPCPtr());
  uint16_t inst_data = inst->Fetch16(0);
  if (flags & kCheckpointRequest) {
    LOG(INFO) << "Checkpoint fallback: " << inst->Opcode(inst_data);
  } else if (flags & kSuspendRequest) {
    LOG(INFO) << "Suspend fallback: " << inst->Opcode(inst_data);
  } else if (flags & kEmptyCheckpointRequest) {
    LOG(INFO) << "Empty checkpoint fallback: " << inst->Opcode(inst_data);
  }
}

extern "C" size_t MterpSuspendCheck(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  self->AllowThreadSuspension();
  return MterpShouldSwitchInterpreters();
}

extern "C" ssize_t artSet8InstanceFromMterp(uint32_t field_idx,
                                            mirror::Object* obj,
                                            uint8_t new_value,
                                            ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite, sizeof(int8_t));
  if (LIKELY(field != nullptr && obj != nullptr)) {
    Primitive::Type type = field->GetTypeAsPrimitiveType();
    if (type == Primitive::kPrimBoolean) {
      field->SetBoolean<false>(obj, new_value);
    } else {
      DCHECK_EQ(Primitive::kPrimByte, type);
      field->SetByte<false>(obj, new_value);
    }
    return 0;  // success
  }
  return -1;  // failure
}

extern "C" ssize_t artSet16InstanceFromMterp(uint32_t field_idx,
                                             mirror::Object* obj,
                                             uint16_t new_value,
                                             ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite,
                                          sizeof(int16_t));
  if (LIKELY(field != nullptr && obj != nullptr)) {
    Primitive::Type type = field->GetTypeAsPrimitiveType();
    if (type == Primitive::kPrimChar) {
      field->SetChar<false>(obj, new_value);
    } else {
      DCHECK_EQ(Primitive::kPrimShort, type);
      field->SetShort<false>(obj, new_value);
    }
    return 0;  // success
  }
  return -1;  // failure
}

extern "C" ssize_t artSet32InstanceFromMterp(uint32_t field_idx,
                                             mirror::Object* obj,
                                             uint32_t new_value,
                                             ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite,
                                          sizeof(int32_t));
  if (LIKELY(field != nullptr && obj != nullptr)) {
    field->Set32<false>(obj, new_value);
    return 0;  // success
  }
  return -1;  // failure
}

extern "C" ssize_t artSet64InstanceFromMterp(uint32_t field_idx,
                                             mirror::Object* obj,
                                             uint64_t* new_value,
                                             ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = FindFieldFast(field_idx, referrer, InstancePrimitiveWrite,
                                          sizeof(int64_t));
  if (LIKELY(field != nullptr  && obj != nullptr)) {
    field->Set64<false>(obj, *new_value);
    return 0;  // success
  }
  return -1;  // failure
}

extern "C" ssize_t artSetObjInstanceFromMterp(uint32_t field_idx,
                                              mirror::Object* obj,
                                              mirror::Object* new_value,
                                              ArtMethod* referrer)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* field = FindFieldFast(field_idx, referrer, InstanceObjectWrite,
                                          sizeof(mirror::HeapReference<mirror::Object>));
  if (LIKELY(field != nullptr && obj != nullptr)) {
    field->SetObj<false>(obj, new_value);
    return 0;  // success
  }
  return -1;  // failure
}

template <typename return_type, Primitive::Type primitive_type>
ALWAYS_INLINE return_type MterpGetStatic(uint32_t field_idx,
                                         ArtMethod* referrer,
                                         Thread* self,
                                         return_type (ArtField::*func)(ObjPtr<mirror::Object>))
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return_type res = 0;  // On exception, the result will be ignored.
  ArtField* f =
      FindFieldFromCode<StaticPrimitiveRead, false>(field_idx,
                                                    referrer,
                                                    self,
                                                    primitive_type);
  if (LIKELY(f != nullptr)) {
    ObjPtr<mirror::Object> obj = f->GetDeclaringClass();
    res = (f->*func)(obj);
  }
  return res;
}

extern "C" int32_t MterpGetBooleanStatic(uint32_t field_idx,
                                         ArtMethod* referrer,
                                         Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<uint8_t, Primitive::kPrimBoolean>(field_idx,
                                                          referrer,
                                                          self,
                                                          &ArtField::GetBoolean);
}

extern "C" int32_t MterpGetByteStatic(uint32_t field_idx,
                                      ArtMethod* referrer,
                                      Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<int8_t, Primitive::kPrimByte>(field_idx,
                                                      referrer,
                                                      self,
                                                      &ArtField::GetByte);
}

extern "C" uint32_t MterpGetCharStatic(uint32_t field_idx,
                                       ArtMethod* referrer,
                                       Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<uint16_t, Primitive::kPrimChar>(field_idx,
                                                        referrer,
                                                        self,
                                                        &ArtField::GetChar);
}

extern "C" int32_t MterpGetShortStatic(uint32_t field_idx,
                                       ArtMethod* referrer,
                                       Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<int16_t, Primitive::kPrimShort>(field_idx,
                                                        referrer,
                                                        self,
                                                        &ArtField::GetShort);
}

extern "C" mirror::Object* MterpGetObjStatic(uint32_t field_idx,
                                             ArtMethod* referrer,
                                             Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<ObjPtr<mirror::Object>, Primitive::kPrimNot>(field_idx,
                                                                     referrer,
                                                                     self,
                                                                     &ArtField::GetObject).Ptr();
}

extern "C" int32_t MterpGet32Static(uint32_t field_idx,
                                    ArtMethod* referrer,
                                    Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<int32_t, Primitive::kPrimInt>(field_idx,
                                                      referrer,
                                                      self,
                                                      &ArtField::GetInt);
}

extern "C" int64_t MterpGet64Static(uint32_t field_idx, ArtMethod* referrer, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpGetStatic<int64_t, Primitive::kPrimLong>(field_idx,
                                                       referrer,
                                                       self,
                                                       &ArtField::GetLong);
}


template <typename field_type, Primitive::Type primitive_type>
int MterpSetStatic(uint32_t field_idx,
                   field_type new_value,
                   ArtMethod* referrer,
                   Thread* self,
                   void (ArtField::*func)(ObjPtr<mirror::Object>, field_type val))
    REQUIRES_SHARED(Locks::mutator_lock_) {
  int res = 0;  // Assume success (following quick_field_entrypoints conventions)
  ArtField* f =
      FindFieldFromCode<StaticPrimitiveWrite, false>(field_idx, referrer, self, primitive_type);
  if (LIKELY(f != nullptr)) {
    ObjPtr<mirror::Object> obj = f->GetDeclaringClass();
    (f->*func)(obj, new_value);
  } else {
    res = -1;  // Failure
  }
  return res;
}

extern "C" int MterpSetBooleanStatic(uint32_t field_idx,
                                     uint8_t new_value,
                                     ArtMethod* referrer,
                                     Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<uint8_t, Primitive::kPrimBoolean>(field_idx,
                                                          new_value,
                                                          referrer,
                                                          self,
                                                          &ArtField::SetBoolean<false>);
}

extern "C" int MterpSetByteStatic(uint32_t field_idx,
                                  int8_t new_value,
                                  ArtMethod* referrer,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<int8_t, Primitive::kPrimByte>(field_idx,
                                                      new_value,
                                                      referrer,
                                                      self,
                                                      &ArtField::SetByte<false>);
}

extern "C" int MterpSetCharStatic(uint32_t field_idx,
                                  uint16_t new_value,
                                  ArtMethod* referrer,
                                  Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<uint16_t, Primitive::kPrimChar>(field_idx,
                                                        new_value,
                                                        referrer,
                                                        self,
                                                        &ArtField::SetChar<false>);
}

extern "C" int MterpSetShortStatic(uint32_t field_idx,
                                   int16_t new_value,
                                   ArtMethod* referrer,
                                   Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<int16_t, Primitive::kPrimShort>(field_idx,
                                                        new_value,
                                                        referrer,
                                                        self,
                                                        &ArtField::SetShort<false>);
}

extern "C" int MterpSet32Static(uint32_t field_idx,
                                int32_t new_value,
                                ArtMethod* referrer,
                                Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<int32_t, Primitive::kPrimInt>(field_idx,
                                                      new_value,
                                                      referrer,
                                                      self,
                                                      &ArtField::SetInt<false>);
}

extern "C" int MterpSet64Static(uint32_t field_idx,
                                int64_t* new_value,
                                ArtMethod* referrer,
                                Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return MterpSetStatic<int64_t, Primitive::kPrimLong>(field_idx,
                                                       *new_value,
                                                       referrer,
                                                       self,
                                                       &ArtField::SetLong<false>);
}

extern "C" mirror::Object* artAGetObjectFromMterp(mirror::Object* arr,
                                                  int32_t index)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(arr == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return nullptr;
  }
  mirror::ObjectArray<mirror::Object>* array = arr->AsObjectArray<mirror::Object>();
  if (LIKELY(array->CheckIsValidIndex(index))) {
    return array->GetWithoutChecks(index);
  } else {
    return nullptr;
  }
}

extern "C" mirror::Object* artIGetObjectFromMterp(mirror::Object* obj,
                                                  uint32_t field_offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerExceptionFromInterpreter();
    return nullptr;
  }
  return obj->GetFieldObject<mirror::Object>(MemberOffset(field_offset));
}

/*
 * Create a hotness_countdown based on the current method hotness_count and profiling
 * mode.  In short, determine how many hotness events we hit before reporting back
 * to the full instrumentation via MterpAddHotnessBatch.  Called once on entry to the method,
 * and regenerated following batch updates.
 */
extern "C" ssize_t MterpSetUpHotnessCountdown(ArtMethod* method,
                                              ShadowFrame* shadow_frame,
                                              Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint16_t hotness_count = method->GetCounter();
  int32_t countdown_value = jit::kJitHotnessDisabled;
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    int32_t warm_threshold = jit->WarmMethodThreshold();
    int32_t hot_threshold = jit->HotMethodThreshold();
    int32_t osr_threshold = jit->OSRMethodThreshold();
    if (hotness_count < warm_threshold) {
      countdown_value = warm_threshold - hotness_count;
    } else if (hotness_count < hot_threshold) {
      countdown_value = hot_threshold - hotness_count;
    } else if (hotness_count < osr_threshold) {
      countdown_value = osr_threshold - hotness_count;
    } else {
      countdown_value = jit::kJitCheckForOSR;
    }
    if (jit::Jit::ShouldUsePriorityThreadWeight(self)) {
      int32_t priority_thread_weight = jit->PriorityThreadWeight();
      countdown_value = std::min(countdown_value, countdown_value / priority_thread_weight);
    }
  }
  /*
   * The actual hotness threshold may exceed the range of our int16_t countdown value.  This is
   * not a problem, though.  We can just break it down into smaller chunks.
   */
  countdown_value = std::min(countdown_value,
                             static_cast<int32_t>(std::numeric_limits<int16_t>::max()));
  shadow_frame->SetCachedHotnessCountdown(countdown_value);
  shadow_frame->SetHotnessCountdown(countdown_value);
  return countdown_value;
}

/*
 * Report a batch of hotness events to the instrumentation and then return the new
 * countdown value to the next time we should report.
 */
extern "C" ssize_t MterpAddHotnessBatch(ArtMethod* method,
                                        ShadowFrame* shadow_frame,
                                        Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    int16_t count = shadow_frame->GetCachedHotnessCountdown() - shadow_frame->GetHotnessCountdown();
    jit->AddSamples(self, method, count, /*with_backedges*/ true);
  }
  return MterpSetUpHotnessCountdown(method, shadow_frame, self);
}

extern "C" size_t MterpMaybeDoOnStackReplacement(Thread* self,
                                                 ShadowFrame* shadow_frame,
                                                 int32_t offset)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  int16_t osr_countdown = shadow_frame->GetCachedHotnessCountdown() - 1;
  bool did_osr = false;
  /*
   * To reduce the cost of polling the compiler to determine whether the requested OSR
   * compilation has completed, only check every Nth time.  NOTE: the "osr_countdown <= 0"
   * condition is satisfied either by the decrement below or the initial setting of
   * the cached countdown field to kJitCheckForOSR, which elsewhere is asserted to be -1.
   */
  if (osr_countdown <= 0) {
    ArtMethod* method = shadow_frame->GetMethod();
    JValue* result = shadow_frame->GetResultRegister();
    uint32_t dex_pc = shadow_frame->GetDexPC();
    jit::Jit* jit = Runtime::Current()->GetJit();
    osr_countdown = jit::Jit::kJitRecheckOSRThreshold;
    if (offset <= 0) {
      // Keep updating hotness in case a compilation request was dropped.  Eventually it will retry.
      jit->AddSamples(self, method, osr_countdown, /*with_backedges*/ true);
    }
    did_osr = jit::Jit::MaybeDoOnStackReplacement(self, method, dex_pc, offset, result);
  }
  shadow_frame->SetCachedHotnessCountdown(osr_countdown);
  return did_osr;
}

}  // namespace interpreter
}  // namespace art
