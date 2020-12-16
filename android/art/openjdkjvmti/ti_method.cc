/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_method.h"

#include <type_traits>

#include "art_jvmti.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/mutex-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_file_types.h"
#include "dex/modifiers.h"
#include "events-inl.h"
#include "gc_root-inl.h"
#include "jit/jit.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "oat_file.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_stack.h"
#include "ti_thread.h"
#include "ti_phase.h"

namespace openjdkjvmti {

struct TiMethodCallback : public art::MethodCallback {
  void RegisterNativeMethod(art::ArtMethod* method,
                            const void* cur_method,
                            /*out*/void** new_method)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (event_handler->IsEventEnabledAnywhere(ArtJvmtiEvent::kNativeMethodBind)) {
      art::Thread* thread = art::Thread::Current();
      art::JNIEnvExt* jnienv = thread->GetJniEnv();
      ScopedLocalRef<jthread> thread_jni(
          jnienv, PhaseUtil::IsLivePhase() ? jnienv->AddLocalReference<jthread>(thread->GetPeer())
                                           : nullptr);
      art::ScopedThreadSuspension sts(thread, art::ThreadState::kNative);
      event_handler->DispatchEvent<ArtJvmtiEvent::kNativeMethodBind>(
          thread,
          static_cast<JNIEnv*>(jnienv),
          thread_jni.get(),
          art::jni::EncodeArtMethod(method),
          const_cast<void*>(cur_method),
          new_method);
    }
  }

  EventHandler* event_handler = nullptr;
};

TiMethodCallback gMethodCallback;

void MethodUtil::Register(EventHandler* handler) {
  gMethodCallback.event_handler = handler;
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add method callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->AddMethodCallback(&gMethodCallback);
}

void MethodUtil::Unregister() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Remove method callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->RemoveMethodCallback(&gMethodCallback);
}

jvmtiError MethodUtil::GetBytecodes(jvmtiEnv* env,
                                    jmethodID method,
                                    jint* size_ptr,
                                    unsigned char** bytecode_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);

  if (art_method->IsNative()) {
    return ERR(NATIVE_METHOD);
  }

  if (size_ptr == nullptr || bytecode_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::CodeItemInstructionAccessor accessor(art_method->DexInstructions());
  if (!accessor.HasCodeItem()) {
    *size_ptr = 0;
    *bytecode_ptr = nullptr;
    return OK;
  }
  // 2 bytes per instruction for dex code.
  *size_ptr = accessor.InsnsSizeInCodeUnits() * 2;
  jvmtiError err = env->Allocate(*size_ptr, bytecode_ptr);
  if (err != OK) {
    return err;
  }
  memcpy(*bytecode_ptr, accessor.Insns(), *size_ptr);
  return OK;
}

jvmtiError MethodUtil::GetArgumentsSize(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        jmethodID method,
                                        jint* size_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);

  if (art_method->IsNative()) {
    return ERR(NATIVE_METHOD);
  }

  if (size_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  if (art_method->IsProxyMethod() || art_method->IsAbstract()) {
    // Use the shorty.
    art::ArtMethod* base_method = art_method->GetInterfaceMethodIfProxy(art::kRuntimePointerSize);
    size_t arg_count = art::ArtMethod::NumArgRegisters(base_method->GetShorty());
    if (!base_method->IsStatic()) {
      arg_count++;
    }
    *size_ptr = static_cast<jint>(arg_count);
    return ERR(NONE);
  }

  DCHECK_NE(art_method->GetCodeItemOffset(), 0u);
  *size_ptr = art_method->DexInstructionData().InsSize();

  return ERR(NONE);
}

jvmtiError MethodUtil::GetLocalVariableTable(jvmtiEnv* env,
                                             jmethodID method,
                                             jint* entry_count_ptr,
                                             jvmtiLocalVariableEntry** table_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);

  if (art_method->IsNative()) {
    return ERR(NATIVE_METHOD);
  }

  if (entry_count_ptr == nullptr || table_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());

  const art::DexFile* const dex_file = art_method->GetDexFile();
  if (dex_file == nullptr) {
    return ERR(ABSENT_INFORMATION);
  }

  // TODO HasCodeItem == false means that the method is abstract (or native, but we check that
  // earlier). We should check what is returned by the RI in this situation since it's not clear
  // what the appropriate return value is from the spec.
  art::CodeItemDebugInfoAccessor accessor(art_method->DexInstructionDebugInfo());
  if (!accessor.HasCodeItem()) {
    return ERR(ABSENT_INFORMATION);
  }

  struct LocalVariableContext {
    explicit LocalVariableContext(jvmtiEnv* jenv) : env_(jenv), variables_(), err_(OK) {}

    static void Callback(void* raw_ctx, const art::DexFile::LocalInfo& entry) {
      reinterpret_cast<LocalVariableContext*>(raw_ctx)->Insert(entry);
    }

    void Insert(const art::DexFile::LocalInfo& entry) {
      if (err_ != OK) {
        return;
      }
      JvmtiUniquePtr<char[]> name_str = CopyString(env_, entry.name_, &err_);
      if (err_ != OK) {
        return;
      }
      JvmtiUniquePtr<char[]> sig_str = CopyString(env_, entry.descriptor_, &err_);
      if (err_ != OK) {
        return;
      }
      JvmtiUniquePtr<char[]> generic_sig_str = CopyString(env_, entry.signature_, &err_);
      if (err_ != OK) {
        return;
      }
      variables_.push_back({
        .start_location = static_cast<jlocation>(entry.start_address_),
        .length = static_cast<jint>(entry.end_address_ - entry.start_address_),
        .name = name_str.release(),
        .signature = sig_str.release(),
        .generic_signature = generic_sig_str.release(),
        .slot = entry.reg_,
      });
    }

    jvmtiError Release(jint* out_entry_count_ptr, jvmtiLocalVariableEntry** out_table_ptr) {
      jlong table_size = sizeof(jvmtiLocalVariableEntry) * variables_.size();
      if (err_ != OK ||
          (err_ = env_->Allocate(table_size,
                                 reinterpret_cast<unsigned char**>(out_table_ptr))) != OK) {
        Cleanup();
        return err_;
      } else {
        *out_entry_count_ptr = variables_.size();
        memcpy(*out_table_ptr, variables_.data(), table_size);
        return OK;
      }
    }

    void Cleanup() {
      for (jvmtiLocalVariableEntry& e : variables_) {
        env_->Deallocate(reinterpret_cast<unsigned char*>(e.name));
        env_->Deallocate(reinterpret_cast<unsigned char*>(e.signature));
        env_->Deallocate(reinterpret_cast<unsigned char*>(e.generic_signature));
      }
    }

    jvmtiEnv* env_;
    std::vector<jvmtiLocalVariableEntry> variables_;
    jvmtiError err_;
  };

  LocalVariableContext context(env);
  if (!accessor.DecodeDebugLocalInfo(art_method->IsStatic(),
                                     art_method->GetDexMethodIndex(),
                                     LocalVariableContext::Callback,
                                     &context)) {
    // Something went wrong with decoding the debug information. It might as well not be there.
    return ERR(ABSENT_INFORMATION);
  } else {
    return context.Release(entry_count_ptr, table_ptr);
  }
}

jvmtiError MethodUtil::GetMaxLocals(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                    jmethodID method,
                                    jint* max_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);

  if (art_method->IsNative()) {
    return ERR(NATIVE_METHOD);
  }

  if (max_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  if (art_method->IsProxyMethod() || art_method->IsAbstract()) {
    // This isn't specified as an error case, so return 0.
    *max_ptr = 0;
    return ERR(NONE);
  }

  DCHECK_NE(art_method->GetCodeItemOffset(), 0u);
  *max_ptr = art_method->DexInstructionData().RegistersSize();

  return ERR(NONE);
}

jvmtiError MethodUtil::GetMethodName(jvmtiEnv* env,
                                     jmethodID method,
                                     char** name_ptr,
                                     char** signature_ptr,
                                     char** generic_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);
  art_method = art_method->GetInterfaceMethodIfProxy(art::kRuntimePointerSize);

  JvmtiUniquePtr<char[]> name_copy;
  if (name_ptr != nullptr) {
    const char* method_name = art_method->GetName();
    if (method_name == nullptr) {
      method_name = "<error>";
    }
    jvmtiError ret;
    name_copy = CopyString(env, method_name, &ret);
    if (name_copy == nullptr) {
      return ret;
    }
    *name_ptr = name_copy.get();
  }

  JvmtiUniquePtr<char[]> signature_copy;
  if (signature_ptr != nullptr) {
    const art::Signature sig = art_method->GetSignature();
    std::string str = sig.ToString();
    jvmtiError ret;
    signature_copy = CopyString(env, str.c_str(), &ret);
    if (signature_copy == nullptr) {
      return ret;
    }
    *signature_ptr = signature_copy.get();
  }

  if (generic_ptr != nullptr) {
    *generic_ptr = nullptr;
    if (!art_method->GetDeclaringClass()->IsProxyClass()) {
      art::mirror::ObjectArray<art::mirror::String>* str_array =
          art::annotations::GetSignatureAnnotationForMethod(art_method);
      if (str_array != nullptr) {
        std::ostringstream oss;
        for (int32_t i = 0; i != str_array->GetLength(); ++i) {
          oss << str_array->Get(i)->ToModifiedUtf8();
        }
        std::string output_string = oss.str();
        jvmtiError ret;
        JvmtiUniquePtr<char[]> generic_copy = CopyString(env, output_string.c_str(), &ret);
        if (generic_copy == nullptr) {
          return ret;
        }
        *generic_ptr = generic_copy.release();
      } else if (soa.Self()->IsExceptionPending()) {
        // TODO: Should we report an error here?
        soa.Self()->ClearException();
      }
    }
  }

  // Everything is fine, release the buffers.
  name_copy.release();
  signature_copy.release();

  return ERR(NONE);
}

jvmtiError MethodUtil::GetMethodDeclaringClass(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                               jmethodID method,
                                               jclass* declaring_class_ptr) {
  if (declaring_class_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);
  // Note: No GetInterfaceMethodIfProxy, we want to actual class.

  art::ScopedObjectAccess soa(art::Thread::Current());
  art::mirror::Class* klass = art_method->GetDeclaringClass();
  *declaring_class_ptr = soa.AddLocalReference<jclass>(klass);

  return ERR(NONE);
}

jvmtiError MethodUtil::GetMethodLocation(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                         jmethodID method,
                                         jlocation* start_location_ptr,
                                         jlocation* end_location_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);

  if (art_method->IsNative()) {
    return ERR(NATIVE_METHOD);
  }

  if (start_location_ptr == nullptr || end_location_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  if (art_method->IsProxyMethod() || art_method->IsAbstract()) {
    // This isn't specified as an error case, so return -1/-1 as the RI does.
    *start_location_ptr = -1;
    *end_location_ptr = -1;
    return ERR(NONE);
  }

  DCHECK_NE(art_method->GetCodeItemOffset(), 0u);
  *start_location_ptr = 0;
  *end_location_ptr = art_method->DexInstructions().InsnsSizeInCodeUnits() - 1;

  return ERR(NONE);
}

jvmtiError MethodUtil::GetMethodModifiers(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                          jmethodID method,
                                          jint* modifiers_ptr) {
  if (modifiers_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);
  uint32_t modifiers = art_method->GetAccessFlags();

  // Note: Keep this code in sync with Executable.fixMethodFlags.
  if ((modifiers & art::kAccAbstract) != 0) {
    modifiers &= ~art::kAccNative;
  }
  modifiers &= ~art::kAccSynchronized;
  if ((modifiers & art::kAccDeclaredSynchronized) != 0) {
    modifiers |= art::kAccSynchronized;
  }
  modifiers &= art::kAccJavaFlagsMask;

  *modifiers_ptr = modifiers;
  return ERR(NONE);
}

using LineNumberContext = std::vector<jvmtiLineNumberEntry>;

static bool CollectLineNumbers(void* void_context, const art::DexFile::PositionInfo& entry) {
  LineNumberContext* context = reinterpret_cast<LineNumberContext*>(void_context);
  jvmtiLineNumberEntry jvmti_entry = { static_cast<jlocation>(entry.address_),
                                       static_cast<jint>(entry.line_) };
  context->push_back(jvmti_entry);
  return false;  // Collect all, no early exit.
}

jvmtiError MethodUtil::GetLineNumberTable(jvmtiEnv* env,
                                          jmethodID method,
                                          jint* entry_count_ptr,
                                          jvmtiLineNumberEntry** table_ptr) {
  if (method == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);
  DCHECK(!art_method->IsRuntimeMethod());

  art::CodeItemDebugInfoAccessor accessor;
  const art::DexFile* dex_file;
  {
    art::ScopedObjectAccess soa(art::Thread::Current());

    if (art_method->IsProxyMethod()) {
      return ERR(ABSENT_INFORMATION);
    }
    if (art_method->IsNative()) {
      return ERR(NATIVE_METHOD);
    }
    if (entry_count_ptr == nullptr || table_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }

    accessor = art::CodeItemDebugInfoAccessor(art_method->DexInstructionDebugInfo());
    dex_file = art_method->GetDexFile();
    DCHECK(accessor.HasCodeItem()) << art_method->PrettyMethod() << " " << dex_file->GetLocation();
  }

  LineNumberContext context;
  bool success = dex_file->DecodeDebugPositionInfo(
      accessor.DebugInfoOffset(), CollectLineNumbers, &context);
  if (!success) {
    return ERR(ABSENT_INFORMATION);
  }

  unsigned char* data;
  jlong mem_size = context.size() * sizeof(jvmtiLineNumberEntry);
  jvmtiError alloc_error = env->Allocate(mem_size, &data);
  if (alloc_error != ERR(NONE)) {
    return alloc_error;
  }
  *table_ptr = reinterpret_cast<jvmtiLineNumberEntry*>(data);
  memcpy(*table_ptr, context.data(), mem_size);
  *entry_count_ptr = static_cast<jint>(context.size());

  return ERR(NONE);
}

template <typename T>
static jvmtiError IsMethodT(jvmtiEnv* env ATTRIBUTE_UNUSED,
                            jmethodID method,
                            T test,
                            jboolean* is_t_ptr) {
  if (method == nullptr) {
    return ERR(INVALID_METHODID);
  }
  if (is_t_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::ArtMethod* art_method = art::jni::DecodeArtMethod(method);
  *is_t_ptr = test(art_method) ? JNI_TRUE : JNI_FALSE;

  return ERR(NONE);
}

jvmtiError MethodUtil::IsMethodNative(jvmtiEnv* env, jmethodID m, jboolean* is_native_ptr) {
  auto test = [](art::ArtMethod* method) {
    return method->IsNative();
  };
  return IsMethodT(env, m, test, is_native_ptr);
}

jvmtiError MethodUtil::IsMethodObsolete(jvmtiEnv* env, jmethodID m, jboolean* is_obsolete_ptr) {
  auto test = [](art::ArtMethod* method) {
    return method->IsObsolete();
  };
  return IsMethodT(env, m, test, is_obsolete_ptr);
}

jvmtiError MethodUtil::IsMethodSynthetic(jvmtiEnv* env, jmethodID m, jboolean* is_synthetic_ptr) {
  auto test = [](art::ArtMethod* method) {
    return method->IsSynthetic();
  };
  return IsMethodT(env, m, test, is_synthetic_ptr);
}

class CommonLocalVariableClosure : public art::Closure {
 public:
  CommonLocalVariableClosure(jint depth, jint slot)
      : result_(ERR(INTERNAL)), depth_(depth), slot_(slot) {}

  void Run(art::Thread* self) OVERRIDE REQUIRES(art::Locks::mutator_lock_) {
    art::Locks::mutator_lock_->AssertSharedHeld(art::Thread::Current());
    art::ScopedAssertNoThreadSuspension sants("CommonLocalVariableClosure::Run");
    std::unique_ptr<art::Context> context(art::Context::Create());
    FindFrameAtDepthVisitor visitor(self, context.get(), depth_);
    visitor.WalkStack();
    if (!visitor.FoundFrame()) {
      // Must have been a bad depth.
      result_ = ERR(NO_MORE_FRAMES);
      return;
    }
    art::ArtMethod* method = visitor.GetMethod();
    // Native and 'art' proxy methods don't have registers.
    if (method->IsNative() || method->IsProxyMethod()) {
      // TODO It might be useful to fake up support for get at least on proxy frames.
      result_ = ERR(OPAQUE_FRAME);
      return;
    } else if (method->DexInstructionData().RegistersSize() <= slot_) {
      result_ = ERR(INVALID_SLOT);
      return;
    }
    bool needs_instrument = !visitor.IsShadowFrame();
    uint32_t pc = visitor.GetDexPc(/*abort_on_failure*/ false);
    if (pc == art::dex::kDexNoIndex) {
      // Cannot figure out current PC.
      result_ = ERR(OPAQUE_FRAME);
      return;
    }
    std::string descriptor;
    art::Primitive::Type slot_type = art::Primitive::kPrimVoid;
    jvmtiError err = GetSlotType(method, pc, &descriptor, &slot_type);
    if (err != OK) {
      result_ = err;
      return;
    }

    err = GetTypeError(method, slot_type, descriptor);
    if (err != OK) {
      result_ = err;
      return;
    }
    result_ = Execute(method, visitor);
    if (needs_instrument) {
      art::Runtime::Current()->GetInstrumentation()->InstrumentThreadStack(self);
    }
  }

  virtual jvmtiError GetResult() {
    return result_;
  }

 protected:
  virtual jvmtiError Execute(art::ArtMethod* method, art::StackVisitor& visitor)
      REQUIRES_SHARED(art::Locks::mutator_lock_) = 0;
  virtual jvmtiError GetTypeError(art::ArtMethod* method,
                                  art::Primitive::Type type,
                                  const std::string& descriptor)
      REQUIRES_SHARED(art::Locks::mutator_lock_)  = 0;

  jvmtiError GetSlotType(art::ArtMethod* method,
                         uint32_t dex_pc,
                         /*out*/std::string* descriptor,
                         /*out*/art::Primitive::Type* type)
      REQUIRES(art::Locks::mutator_lock_) {
    const art::DexFile* dex_file = method->GetDexFile();
    if (dex_file == nullptr) {
      return ERR(OPAQUE_FRAME);
    }
    art::CodeItemDebugInfoAccessor accessor(method->DexInstructionDebugInfo());
    if (!accessor.HasCodeItem()) {
      return ERR(OPAQUE_FRAME);
    }

    struct GetLocalVariableInfoContext {
      explicit GetLocalVariableInfoContext(jint slot,
                                          uint32_t pc,
                                          std::string* out_descriptor,
                                          art::Primitive::Type* out_type)
          : found_(false), jslot_(slot), pc_(pc), descriptor_(out_descriptor), type_(out_type) {
        *descriptor_ = "";
        *type_ = art::Primitive::kPrimVoid;
      }

      static void Callback(void* raw_ctx, const art::DexFile::LocalInfo& entry) {
        reinterpret_cast<GetLocalVariableInfoContext*>(raw_ctx)->Handle(entry);
      }

      void Handle(const art::DexFile::LocalInfo& entry) {
        if (found_) {
          return;
        } else if (entry.start_address_ <= pc_ &&
                   entry.end_address_ > pc_ &&
                   entry.reg_ == jslot_) {
          found_ = true;
          *type_ = art::Primitive::GetType(entry.descriptor_[0]);
          *descriptor_ = entry.descriptor_;
        }
        return;
      }

      bool found_;
      jint jslot_;
      uint32_t pc_;
      std::string* descriptor_;
      art::Primitive::Type* type_;
    };

    GetLocalVariableInfoContext context(slot_, dex_pc, descriptor, type);
    if (!dex_file->DecodeDebugLocalInfo(accessor.RegistersSize(),
                                        accessor.InsSize(),
                                        accessor.InsnsSizeInCodeUnits(),
                                        accessor.DebugInfoOffset(),
                                        method->IsStatic(),
                                        method->GetDexMethodIndex(),
                                        GetLocalVariableInfoContext::Callback,
                                        &context) || !context.found_) {
      // Something went wrong with decoding the debug information. It might as well not be there.
      return ERR(INVALID_SLOT);
    } else {
      return OK;
    }
  }

  jvmtiError result_;
  jint depth_;
  jint slot_;
};

class GetLocalVariableClosure : public CommonLocalVariableClosure {
 public:
  GetLocalVariableClosure(jint depth,
                          jint slot,
                          art::Primitive::Type type,
                          jvalue* val)
      : CommonLocalVariableClosure(depth, slot),
        type_(type),
        val_(val),
        obj_val_(nullptr) {}

  virtual jvmtiError GetResult() REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (result_ == OK && type_ == art::Primitive::kPrimNot) {
      val_->l = obj_val_.IsNull()
          ? nullptr
          : art::Thread::Current()->GetJniEnv()->AddLocalReference<jobject>(obj_val_.Read());
    }
    return CommonLocalVariableClosure::GetResult();
  }

 protected:
  jvmtiError GetTypeError(art::ArtMethod* method ATTRIBUTE_UNUSED,
                          art::Primitive::Type slot_type,
                          const std::string& descriptor ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    switch (slot_type) {
      case art::Primitive::kPrimByte:
      case art::Primitive::kPrimChar:
      case art::Primitive::kPrimInt:
      case art::Primitive::kPrimShort:
      case art::Primitive::kPrimBoolean:
        return type_ == art::Primitive::kPrimInt ? OK : ERR(TYPE_MISMATCH);
      case art::Primitive::kPrimLong:
      case art::Primitive::kPrimFloat:
      case art::Primitive::kPrimDouble:
      case art::Primitive::kPrimNot:
        return type_ == slot_type ? OK : ERR(TYPE_MISMATCH);
      case art::Primitive::kPrimVoid:
        LOG(FATAL) << "Unexpected primitive type " << slot_type;
        UNREACHABLE();
    }
  }

  jvmtiError Execute(art::ArtMethod* method, art::StackVisitor& visitor)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    switch (type_) {
      case art::Primitive::kPrimNot: {
        uint32_t ptr_val;
        if (!visitor.GetVReg(method,
                             static_cast<uint16_t>(slot_),
                             art::kReferenceVReg,
                             &ptr_val)) {
          return ERR(OPAQUE_FRAME);
        }
        obj_val_ = art::GcRoot<art::mirror::Object>(
            reinterpret_cast<art::mirror::Object*>(ptr_val));
        break;
      }
      case art::Primitive::kPrimInt:
      case art::Primitive::kPrimFloat: {
        if (!visitor.GetVReg(method,
                             static_cast<uint16_t>(slot_),
                             type_ == art::Primitive::kPrimFloat ? art::kFloatVReg : art::kIntVReg,
                             reinterpret_cast<uint32_t*>(&val_->i))) {
          return ERR(OPAQUE_FRAME);
        }
        break;
      }
      case art::Primitive::kPrimDouble:
      case art::Primitive::kPrimLong: {
        auto lo_type = type_ == art::Primitive::kPrimLong ? art::kLongLoVReg : art::kDoubleLoVReg;
        auto high_type = type_ == art::Primitive::kPrimLong ? art::kLongHiVReg : art::kDoubleHiVReg;
        if (!visitor.GetVRegPair(method,
                                 static_cast<uint16_t>(slot_),
                                 lo_type,
                                 high_type,
                                 reinterpret_cast<uint64_t*>(&val_->j))) {
          return ERR(OPAQUE_FRAME);
        }
        break;
      }
      default: {
        LOG(FATAL) << "unexpected register type " << type_;
        UNREACHABLE();
      }
    }
    return OK;
  }

 private:
  art::Primitive::Type type_;
  jvalue* val_;
  art::GcRoot<art::mirror::Object> obj_val_;
};

jvmtiError MethodUtil::GetLocalVariableGeneric(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                               jthread thread,
                                               jint depth,
                                               jint slot,
                                               art::Primitive::Type type,
                                               jvalue* val) {
  if (depth < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  art::Thread* self = art::Thread::Current();
  // Suspend JIT since it can get confused if we deoptimize methods getting jitted.
  art::jit::ScopedJitSuspend suspend_jit;
  art::ScopedObjectAccess soa(self);
  art::Locks::thread_list_lock_->ExclusiveLock(self);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(thread, soa, &target, &err)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return err;
  }
  art::ScopedAssertNoThreadSuspension sants("Performing GetLocalVariable");
  GetLocalVariableClosure c(depth, slot, type, val);
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.  We
  // need to avoid suspending as we wait for the checkpoint to occur since we are (potentially)
  // transfering a GcRoot across threads.
  if (!target->RequestSynchronousCheckpoint(&c, art::ThreadState::kRunnable)) {
    return ERR(THREAD_NOT_ALIVE);
  } else {
    return c.GetResult();
  }
}

class SetLocalVariableClosure : public CommonLocalVariableClosure {
 public:
  SetLocalVariableClosure(art::Thread* caller,
                          jint depth,
                          jint slot,
                          art::Primitive::Type type,
                          jvalue val)
      : CommonLocalVariableClosure(depth, slot), caller_(caller), type_(type), val_(val) {}

 protected:
  jvmtiError GetTypeError(art::ArtMethod* method,
                          art::Primitive::Type slot_type,
                          const std::string& descriptor)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    switch (slot_type) {
      case art::Primitive::kPrimNot: {
        if (type_ != art::Primitive::kPrimNot) {
          return ERR(TYPE_MISMATCH);
        } else if (val_.l == nullptr) {
          return OK;
        } else {
          art::ClassLinker* cl = art::Runtime::Current()->GetClassLinker();
          art::ObjPtr<art::mirror::Class> set_class =
              caller_->DecodeJObject(val_.l)->GetClass();
          art::ObjPtr<art::mirror::ClassLoader> loader =
              method->GetDeclaringClass()->GetClassLoader();
          art::ObjPtr<art::mirror::Class> slot_class =
              cl->LookupClass(caller_, descriptor.c_str(), loader);
          DCHECK(!slot_class.IsNull());
          return slot_class->IsAssignableFrom(set_class) ? OK : ERR(TYPE_MISMATCH);
        }
      }
      case art::Primitive::kPrimByte:
      case art::Primitive::kPrimChar:
      case art::Primitive::kPrimInt:
      case art::Primitive::kPrimShort:
      case art::Primitive::kPrimBoolean:
        return type_ == art::Primitive::kPrimInt ? OK : ERR(TYPE_MISMATCH);
      case art::Primitive::kPrimLong:
      case art::Primitive::kPrimFloat:
      case art::Primitive::kPrimDouble:
        return type_ == slot_type ? OK : ERR(TYPE_MISMATCH);
      case art::Primitive::kPrimVoid:
        LOG(FATAL) << "Unexpected primitive type " << slot_type;
        UNREACHABLE();
    }
  }

  jvmtiError Execute(art::ArtMethod* method, art::StackVisitor& visitor)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    switch (type_) {
      case art::Primitive::kPrimNot: {
        uint32_t ptr_val;
        art::ObjPtr<art::mirror::Object> obj(caller_->DecodeJObject(val_.l));
        ptr_val = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(obj.Ptr()));
        if (!visitor.SetVReg(method,
                             static_cast<uint16_t>(slot_),
                             ptr_val,
                             art::kReferenceVReg)) {
          return ERR(OPAQUE_FRAME);
        }
        break;
      }
      case art::Primitive::kPrimInt:
      case art::Primitive::kPrimFloat: {
        if (!visitor.SetVReg(method,
                             static_cast<uint16_t>(slot_),
                             static_cast<uint32_t>(val_.i),
                             type_ == art::Primitive::kPrimFloat ? art::kFloatVReg
                                                                 : art::kIntVReg)) {
          return ERR(OPAQUE_FRAME);
        }
        break;
      }
      case art::Primitive::kPrimDouble:
      case art::Primitive::kPrimLong: {
        auto lo_type = type_ == art::Primitive::kPrimLong ? art::kLongLoVReg : art::kDoubleLoVReg;
        auto high_type = type_ == art::Primitive::kPrimLong ? art::kLongHiVReg : art::kDoubleHiVReg;
        if (!visitor.SetVRegPair(method,
                                 static_cast<uint16_t>(slot_),
                                 static_cast<uint64_t>(val_.j),
                                 lo_type,
                                 high_type)) {
          return ERR(OPAQUE_FRAME);
        }
        break;
      }
      default: {
        LOG(FATAL) << "unexpected register type " << type_;
        UNREACHABLE();
      }
    }
    return OK;
  }

 private:
  art::Thread* caller_;
  art::Primitive::Type type_;
  jvalue val_;
};

jvmtiError MethodUtil::SetLocalVariableGeneric(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                               jthread thread,
                                               jint depth,
                                               jint slot,
                                               art::Primitive::Type type,
                                               jvalue val) {
  if (depth < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  // Make sure that we know not to do any OSR anymore.
  // TODO We should really keep track of this at the Frame granularity.
  DeoptManager::Get()->SetLocalsUpdated();
  art::Thread* self = art::Thread::Current();
  // Suspend JIT since it can get confused if we deoptimize methods getting jitted.
  art::jit::ScopedJitSuspend suspend_jit;
  art::ScopedObjectAccess soa(self);
  art::Locks::thread_list_lock_->ExclusiveLock(self);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(thread, soa, &target, &err)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return err;
  }
  SetLocalVariableClosure c(self, depth, slot, type, val);
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
  if (!target->RequestSynchronousCheckpoint(&c)) {
    return ERR(THREAD_NOT_ALIVE);
  } else {
    return c.GetResult();
  }
}

class GetLocalInstanceClosure : public art::Closure {
 public:
  explicit GetLocalInstanceClosure(jint depth)
      : result_(ERR(INTERNAL)),
        depth_(depth),
        val_(nullptr) {}

  void Run(art::Thread* self) OVERRIDE REQUIRES(art::Locks::mutator_lock_) {
    art::ScopedAssertNoThreadSuspension sants("GetLocalInstanceClosure::Run");
    art::Locks::mutator_lock_->AssertSharedHeld(art::Thread::Current());
    std::unique_ptr<art::Context> context(art::Context::Create());
    FindFrameAtDepthVisitor visitor(self, context.get(), depth_);
    visitor.WalkStack();
    if (!visitor.FoundFrame()) {
      // Must have been a bad depth.
      result_ = ERR(NO_MORE_FRAMES);
      return;
    }
    result_ = OK;
    val_ = art::GcRoot<art::mirror::Object>(visitor.GetThisObject());
  }

  jvmtiError GetResult(jobject* data_out) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (result_ == OK) {
      *data_out = val_.IsNull()
          ? nullptr
          : art::Thread::Current()->GetJniEnv()->AddLocalReference<jobject>(val_.Read());
    }
    return result_;
  }

 private:
  jvmtiError result_;
  jint depth_;
  art::GcRoot<art::mirror::Object> val_;
};

jvmtiError MethodUtil::GetLocalInstance(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        jthread thread,
                                        jint depth,
                                        jobject* data) {
  if (depth < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::Locks::thread_list_lock_->ExclusiveLock(self);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!ThreadUtil::GetAliveNativeThread(thread, soa, &target, &err)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return err;
  }
  art::ScopedAssertNoThreadSuspension sants("Performing GetLocalInstance");
  GetLocalInstanceClosure c(depth);
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.  We
  // need to avoid suspending as we wait for the checkpoint to occur since we are (potentially)
  // transfering a GcRoot across threads.
  if (!target->RequestSynchronousCheckpoint(&c, art::ThreadState::kRunnable)) {
    return ERR(THREAD_NOT_ALIVE);
  } else {
    return c.GetResult(data);
  }
}

#define FOR_JVMTI_JVALUE_TYPES(fn) \
    fn(jint, art::Primitive::kPrimInt, i) \
    fn(jlong, art::Primitive::kPrimLong, j) \
    fn(jfloat, art::Primitive::kPrimFloat, f) \
    fn(jdouble, art::Primitive::kPrimDouble, d) \
    fn(jobject, art::Primitive::kPrimNot, l)

namespace impl {

template<typename T> void WriteJvalue(T, jvalue*);
template<typename T> void ReadJvalue(jvalue, T*);
template<typename T> art::Primitive::Type GetJNIType();

#define JNI_TYPE_CHAR(type, prim, id) \
template<> art::Primitive::Type GetJNIType<type>() { \
  return prim; \
}

FOR_JVMTI_JVALUE_TYPES(JNI_TYPE_CHAR);

#undef JNI_TYPE_CHAR

#define RW_JVALUE(srctype, prim, id) \
    template<> void ReadJvalue<srctype>(jvalue in, std::add_pointer<srctype>::type out) { \
      *out = in.id; \
    } \
    template<> void WriteJvalue<srctype>(srctype in, jvalue* out) { \
      out->id = in; \
    }

FOR_JVMTI_JVALUE_TYPES(RW_JVALUE);

#undef RW_JVALUE

}  // namespace impl

template<typename T>
jvmtiError MethodUtil::SetLocalVariable(jvmtiEnv* env,
                                        jthread thread,
                                        jint depth,
                                        jint slot,
                                        T data) {
  jvalue v = {.j = 0};
  art::Primitive::Type type = impl::GetJNIType<T>();
  impl::WriteJvalue(data, &v);
  return SetLocalVariableGeneric(env, thread, depth, slot, type, v);
}

template<typename T>
jvmtiError MethodUtil::GetLocalVariable(jvmtiEnv* env,
                                        jthread thread,
                                        jint depth,
                                        jint slot,
                                        T* data) {
  if (data == nullptr) {
    return ERR(NULL_POINTER);
  }
  jvalue v = {.j = 0};
  art::Primitive::Type type = impl::GetJNIType<T>();
  jvmtiError err = GetLocalVariableGeneric(env, thread, depth, slot, type, &v);
  if (err != OK) {
    return err;
  } else {
    impl::ReadJvalue(v, data);
    return OK;
  }
}

#define GET_SET_LV(srctype, prim, id) \
    template jvmtiError MethodUtil::GetLocalVariable<srctype>(jvmtiEnv*, \
                                                              jthread, \
                                                              jint, \
                                                              jint, \
                                                              std::add_pointer<srctype>::type); \
    template jvmtiError MethodUtil::SetLocalVariable<srctype>(jvmtiEnv*, \
                                                              jthread, \
                                                              jint, \
                                                              jint, \
                                                              srctype);

FOR_JVMTI_JVALUE_TYPES(GET_SET_LV);

#undef GET_SET_LV

#undef FOR_JVMTI_JVALUE_TYPES

}  // namespace openjdkjvmti
