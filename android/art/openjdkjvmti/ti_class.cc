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

#include "ti_class.h"

#include "android-base/stringprintf.h"

#include <mutex>
#include <unordered_set>

#include "art_jvmti.h"
#include "base/array_ref.h"
#include "base/macros.h"
#include "base/utils.h"
#include "class_linker.h"
#include "class_loader_utils.h"
#include "class_table-inl.h"
#include "common_throws.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_file_loader.h"
#include "dex/primitive.h"
#include "events-inl.h"
#include "fixed_up_dex_file.h"
#include "gc/heap-visit-objects-inl.h"
#include "gc/heap.h"
#include "gc_root.h"
#include "handle.h"
#include "jni_env_ext-inl.h"
#include "jni_internal.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object_reference.h"
#include "mirror/reference.h"
#include "nativehelper/scoped_local_ref.h"
#include "reflection.h"
#include "runtime.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_class_loader-inl.h"
#include "ti_phase.h"
#include "ti_redefine.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

using android::base::StringPrintf;

static std::unique_ptr<const art::DexFile> MakeSingleDexFile(art::Thread* self,
                                                             const char* descriptor,
                                                             const std::string& orig_location,
                                                             jint final_len,
                                                             const unsigned char* final_dex_data)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
  // Make the mmap
  std::string error_msg;
  art::ArrayRef<const unsigned char> final_data(final_dex_data, final_len);
  std::unique_ptr<art::MemMap> map(Redefiner::MoveDataToMemMap(orig_location,
                                                               final_data,
                                                               &error_msg));
  if (map.get() == nullptr) {
    LOG(WARNING) << "Unable to allocate mmap for redefined dex file! Error was: " << error_msg;
    self->ThrowOutOfMemoryError(StringPrintf(
        "Unable to allocate dex file for transformation of %s", descriptor).c_str());
    return nullptr;
  }

  // Make a dex-file
  if (map->Size() < sizeof(art::DexFile::Header)) {
    LOG(WARNING) << "Could not read dex file header because dex_data was too short";
    art::ThrowClassFormatError(nullptr,
                               "Unable to read transformed dex file of %s",
                               descriptor);
    return nullptr;
  }
  uint32_t checksum = reinterpret_cast<const art::DexFile::Header*>(map->Begin())->checksum_;
  std::string map_name = map->GetName();
  const art::ArtDexFileLoader dex_file_loader;
  std::unique_ptr<const art::DexFile> dex_file(dex_file_loader.Open(map_name,
                                                                    checksum,
                                                                    std::move(map),
                                                                    /*verify*/true,
                                                                    /*verify_checksum*/true,
                                                                    &error_msg));
  if (dex_file.get() == nullptr) {
    LOG(WARNING) << "Unable to load modified dex file for " << descriptor << ": " << error_msg;
    art::ThrowClassFormatError(nullptr,
                               "Unable to read transformed dex file of %s because %s",
                               descriptor,
                               error_msg.c_str());
    return nullptr;
  }
  if (dex_file->NumClassDefs() != 1) {
    LOG(WARNING) << "Dex file contains more than 1 class_def. Ignoring.";
    // TODO Throw some other sort of error here maybe?
    art::ThrowClassFormatError(
        nullptr,
        "Unable to use transformed dex file of %s because it contained too many classes",
        descriptor);
    return nullptr;
  }
  return dex_file;
}

// A deleter that acts like the jvmtiEnv->Deallocate so that asan does not get tripped up.
// TODO We should everything use the actual jvmtiEnv->Allocate/Deallocate functions once we can
// figure out which env to use.
template <typename T>
class FakeJvmtiDeleter {
 public:
  FakeJvmtiDeleter() {}

  FakeJvmtiDeleter(FakeJvmtiDeleter&) = default;
  FakeJvmtiDeleter(FakeJvmtiDeleter&&) = default;
  FakeJvmtiDeleter& operator=(const FakeJvmtiDeleter&) = default;

  template <typename U> void operator()(const U* ptr) const {
    if (ptr != nullptr) {
      free(const_cast<U*>(ptr));
    }
  }
};

struct ClassCallback : public art::ClassLoadCallback {
  void ClassPreDefine(const char* descriptor,
                      art::Handle<art::mirror::Class> klass,
                      art::Handle<art::mirror::ClassLoader> class_loader,
                      const art::DexFile& initial_dex_file,
                      const art::DexFile::ClassDef& initial_class_def ATTRIBUTE_UNUSED,
                      /*out*/art::DexFile const** final_dex_file,
                      /*out*/art::DexFile::ClassDef const** final_class_def)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    bool is_enabled =
        event_handler->IsEventEnabledAnywhere(ArtJvmtiEvent::kClassFileLoadHookRetransformable) ||
        event_handler->IsEventEnabledAnywhere(ArtJvmtiEvent::kClassFileLoadHookNonRetransformable);
    if (!is_enabled) {
      return;
    }
    if (descriptor[0] != 'L') {
      // It is a primitive or array. Just return
      return;
    }
    jvmtiPhase phase = PhaseUtil::GetPhaseUnchecked();
    if (UNLIKELY(phase != JVMTI_PHASE_START && phase != JVMTI_PHASE_LIVE)) {
      // We want to wait until we are at least in the START phase so that all WellKnownClasses and
      // mirror classes have been initialized and loaded. The runtime relies on these classes having
      // specific fields and methods present. Since PreDefine hooks don't need to abide by this
      // restriction we will simply not send the event for these classes.
      LOG(WARNING) << "Ignoring load of class <" << descriptor << "> as it is being loaded during "
                   << "runtime initialization.";
      return;
    }

    art::Thread* self = art::Thread::Current();
    ArtClassDefinition def;
    def.InitFirstLoad(descriptor, class_loader, initial_dex_file);

    // Call all non-retransformable agents.
    Transformer::TransformSingleClassDirect<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
        event_handler, self, &def);

    std::vector<unsigned char> post_non_retransform;
    if (def.IsModified()) {
      // Copy the dex data after the non-retransformable events.
      post_non_retransform.resize(def.GetDexData().size());
      memcpy(post_non_retransform.data(), def.GetDexData().data(), post_non_retransform.size());
    }

    // Call all retransformable agents.
    Transformer::TransformSingleClassDirect<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
        event_handler, self, &def);

    if (def.IsModified()) {
      LOG(WARNING) << "Changing class " << descriptor;
      art::StackHandleScope<2> hs(self);
      // Save the results of all the non-retransformable agents.
      // First allocate the ClassExt
      art::Handle<art::mirror::ClassExt> ext(hs.NewHandle(klass->EnsureExtDataPresent(self)));
      // Make sure we have a ClassExt. This is fine even though we are a temporary since it will
      // get copied.
      if (ext.IsNull()) {
        // We will just return failure if we fail to allocate
        LOG(WARNING) << "Could not allocate ext-data for class '" << descriptor << "'. "
                     << "Aborting transformation since we will be unable to store it.";
        self->AssertPendingOOMException();
        return;
      }

      // Allocate the byte array to store the dex file bytes in.
      art::MutableHandle<art::mirror::Object> arr(hs.NewHandle<art::mirror::Object>(nullptr));
      if (post_non_retransform.empty() && strcmp(descriptor, "Ljava/lang/Long;") != 0) {
        // we didn't have any non-retransformable agents. We can just cache a pointer to the
        // initial_dex_file. It will be kept live by the class_loader.
        jlong dex_ptr = reinterpret_cast<uintptr_t>(&initial_dex_file);
        art::JValue val;
        val.SetJ(dex_ptr);
        arr.Assign(art::BoxPrimitive(art::Primitive::kPrimLong, val));
      } else {
        arr.Assign(art::mirror::ByteArray::AllocateAndFill(
            self,
            reinterpret_cast<const signed char*>(post_non_retransform.data()),
            post_non_retransform.size()));
      }
      if (arr.IsNull()) {
        LOG(WARNING) << "Unable to allocate memory for initial dex-file. Aborting transformation";
        self->AssertPendingOOMException();
        return;
      }

      std::unique_ptr<const art::DexFile> dex_file(MakeSingleDexFile(self,
                                                                     descriptor,
                                                                     initial_dex_file.GetLocation(),
                                                                     def.GetDexData().size(),
                                                                     def.GetDexData().data()));
      if (dex_file.get() == nullptr) {
        return;
      }

      // TODO Check Redefined dex file for all invariants.
      LOG(WARNING) << "Dex file created by class-definition time transformation of "
                   << descriptor << " is not checked for all retransformation invariants.";

      if (!ClassLoaderHelper::AddToClassLoader(self, class_loader, dex_file.get())) {
        LOG(ERROR) << "Unable to add " << descriptor << " to class loader!";
        return;
      }

      // Actually set the ClassExt's original bytes once we have actually succeeded.
      ext->SetOriginalDexFile(arr.Get());
      // Set the return values
      *final_class_def = &dex_file->GetClassDef(0);
      *final_dex_file = dex_file.release();
    }
  }

  void ClassLoad(art::Handle<art::mirror::Class> klass) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (event_handler->IsEventEnabledAnywhere(ArtJvmtiEvent::kClassLoad)) {
      art::Thread* thread = art::Thread::Current();
      ScopedLocalRef<jclass> jklass(thread->GetJniEnv(),
                                    thread->GetJniEnv()->AddLocalReference<jclass>(klass.Get()));
      art::ObjPtr<art::mirror::Object> peer(thread->GetPeer());
      ScopedLocalRef<jthread> thread_jni(
          thread->GetJniEnv(),
          peer.IsNull() ? nullptr : thread->GetJniEnv()->AddLocalReference<jthread>(peer));
      event_handler->DispatchEvent<ArtJvmtiEvent::kClassLoad>(
          thread,
          static_cast<JNIEnv*>(thread->GetJniEnv()),
          thread_jni.get(),
          jklass.get());
      if (klass->IsTemp()) {
        AddTempClass(thread, jklass.get());
      }
    }
  }

  void ClassPrepare(art::Handle<art::mirror::Class> temp_klass,
                    art::Handle<art::mirror::Class> klass)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (event_handler->IsEventEnabledAnywhere(ArtJvmtiEvent::kClassPrepare)) {
      art::Thread* thread = art::Thread::Current();
      if (temp_klass.Get() != klass.Get()) {
        DCHECK(temp_klass->IsTemp());
        DCHECK(temp_klass->IsRetired());
        HandleTempClass(thread, temp_klass, klass);
      }
      ScopedLocalRef<jclass> jklass(thread->GetJniEnv(),
                                    thread->GetJniEnv()->AddLocalReference<jclass>(klass.Get()));
      art::ObjPtr<art::mirror::Object> peer(thread->GetPeer());
      ScopedLocalRef<jthread> thread_jni(
          thread->GetJniEnv(),
          peer.IsNull() ? nullptr : thread->GetJniEnv()->AddLocalReference<jthread>(peer));
      event_handler->DispatchEvent<ArtJvmtiEvent::kClassPrepare>(
          thread,
          static_cast<JNIEnv*>(thread->GetJniEnv()),
          thread_jni.get(),
          jklass.get());
    }
  }

  // To support parallel class-loading, we need to perform some locking dances here. Namely,
  // the fixup stage must not be holding the temp_classes lock when it fixes up the system
  // (as that requires suspending all mutators).

  void AddTempClass(art::Thread* self, jclass klass) {
    std::unique_lock<std::mutex> mu(temp_classes_lock);
    jclass global_klass = reinterpret_cast<jclass>(self->GetJniEnv()->NewGlobalRef(klass));
    temp_classes.push_back(global_klass);
  }

  void HandleTempClass(art::Thread* self,
                       art::Handle<art::mirror::Class> temp_klass,
                       art::Handle<art::mirror::Class> klass)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    bool requires_fixup = false;
    {
      std::unique_lock<std::mutex> mu(temp_classes_lock);
      if (temp_classes.empty()) {
        return;
      }

      for (auto it = temp_classes.begin(); it != temp_classes.end(); ++it) {
        if (temp_klass.Get() == art::ObjPtr<art::mirror::Class>::DownCast(self->DecodeJObject(*it))) {
          self->GetJniEnv()->DeleteGlobalRef(*it);
          temp_classes.erase(it);
          requires_fixup = true;
          break;
        }
      }
    }
    if (requires_fixup) {
      FixupTempClass(self, temp_klass, klass);
    }
  }

  void FixupTempClass(art::Thread* self,
                      art::Handle<art::mirror::Class> temp_klass,
                      art::Handle<art::mirror::Class> klass)
     REQUIRES_SHARED(art::Locks::mutator_lock_) {
    // Suspend everything.
    art::gc::Heap* heap = art::Runtime::Current()->GetHeap();
    if (heap->IsGcConcurrentAndMoving()) {
      // Need to take a heap dump while GC isn't running. See the
      // comment in Heap::VisitObjects().
      heap->IncrementDisableMovingGC(self);
    }
    {
      art::ScopedThreadSuspension sts(self, art::kWaitingForVisitObjects);
      art::ScopedSuspendAll ssa("FixupTempClass");

      art::mirror::Class* input = temp_klass.Get();
      art::mirror::Class* output = klass.Get();

      FixupGlobalReferenceTables(input, output);
      FixupLocalReferenceTables(self, input, output);
      FixupHeap(input, output);
    }
    if (heap->IsGcConcurrentAndMoving()) {
      heap->DecrementDisableMovingGC(self);
    }
  }

  class RootUpdater : public art::RootVisitor {
   public:
    RootUpdater(const art::mirror::Class* input, art::mirror::Class* output)
        : input_(input), output_(output) {}

    void VisitRoots(art::mirror::Object*** roots,
                    size_t count,
                    const art::RootInfo& info ATTRIBUTE_UNUSED)
        OVERRIDE {
      for (size_t i = 0; i != count; ++i) {
        if (*roots[i] == input_) {
          *roots[i] = output_;
        }
      }
    }

    void VisitRoots(art::mirror::CompressedReference<art::mirror::Object>** roots,
                    size_t count,
                    const art::RootInfo& info ATTRIBUTE_UNUSED)
        OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
      for (size_t i = 0; i != count; ++i) {
        if (roots[i]->AsMirrorPtr() == input_) {
          roots[i]->Assign(output_);
        }
      }
    }

   private:
    const art::mirror::Class* input_;
    art::mirror::Class* output_;
  };

  void FixupGlobalReferenceTables(art::mirror::Class* input, art::mirror::Class* output)
      REQUIRES(art::Locks::mutator_lock_) {
    art::JavaVMExt* java_vm = art::Runtime::Current()->GetJavaVM();

    // Fix up the global table with a root visitor.
    RootUpdater global_update(input, output);
    java_vm->VisitRoots(&global_update);

    class WeakGlobalUpdate : public art::IsMarkedVisitor {
     public:
      WeakGlobalUpdate(art::mirror::Class* root_input, art::mirror::Class* root_output)
          : input_(root_input), output_(root_output) {}

      art::mirror::Object* IsMarked(art::mirror::Object* obj) OVERRIDE {
        if (obj == input_) {
          return output_;
        }
        return obj;
      }

     private:
      const art::mirror::Class* input_;
      art::mirror::Class* output_;
    };
    WeakGlobalUpdate weak_global_update(input, output);
    java_vm->SweepJniWeakGlobals(&weak_global_update);
  }

  void FixupLocalReferenceTables(art::Thread* self,
                                 art::mirror::Class* input,
                                 art::mirror::Class* output)
      REQUIRES(art::Locks::mutator_lock_) {
    class LocalUpdate {
     public:
      LocalUpdate(const art::mirror::Class* root_input, art::mirror::Class* root_output)
          : input_(root_input), output_(root_output) {}

      static void Callback(art::Thread* t, void* arg) REQUIRES(art::Locks::mutator_lock_) {
        LocalUpdate* local = reinterpret_cast<LocalUpdate*>(arg);

        // Fix up the local table with a root visitor.
        RootUpdater local_update(local->input_, local->output_);
        t->GetJniEnv()->VisitJniLocalRoots(
            &local_update, art::RootInfo(art::kRootJNILocal, t->GetThreadId()));
      }

     private:
      const art::mirror::Class* input_;
      art::mirror::Class* output_;
    };
    LocalUpdate local_upd(input, output);
    art::MutexLock mu(self, *art::Locks::thread_list_lock_);
    art::Runtime::Current()->GetThreadList()->ForEach(LocalUpdate::Callback, &local_upd);
  }

  void FixupHeap(art::mirror::Class* input, art::mirror::Class* output)
        REQUIRES(art::Locks::mutator_lock_) {
    class HeapFixupVisitor {
     public:
      HeapFixupVisitor(const art::mirror::Class* root_input, art::mirror::Class* root_output)
                : input_(root_input), output_(root_output) {}

      void operator()(art::mirror::Object* src,
                      art::MemberOffset field_offset,
                      bool is_static ATTRIBUTE_UNUSED) const
          REQUIRES_SHARED(art::Locks::mutator_lock_) {
        art::mirror::HeapReference<art::mirror::Object>* trg =
          src->GetFieldObjectReferenceAddr(field_offset);
        if (trg->AsMirrorPtr() == input_) {
          DCHECK_NE(field_offset.Uint32Value(), 0u);  // This shouldn't be the class field of
                                                      // an object.
          trg->Assign(output_);
        }
      }

      void operator()(art::ObjPtr<art::mirror::Class> klass ATTRIBUTE_UNUSED,
                      art::ObjPtr<art::mirror::Reference> reference) const
          REQUIRES_SHARED(art::Locks::mutator_lock_) {
        art::mirror::Object* val = reference->GetReferent();
        if (val == input_) {
          reference->SetReferent<false>(output_);
        }
      }

      void VisitRoot(art::mirror::CompressedReference<art::mirror::Object>* root ATTRIBUTE_UNUSED)
          const {
        LOG(FATAL) << "Unreachable";
      }

      void VisitRootIfNonNull(
          art::mirror::CompressedReference<art::mirror::Object>* root ATTRIBUTE_UNUSED) const {
        LOG(FATAL) << "Unreachable";
      }

     private:
      const art::mirror::Class* input_;
      art::mirror::Class* output_;
    };
    HeapFixupVisitor hfv(input, output);
    auto object_visitor = [&](art::mirror::Object* obj) {
      obj->VisitReferences<false>(hfv, hfv);  // Visit references, not native roots.
    };
    art::Runtime::Current()->GetHeap()->VisitObjectsPaused(object_visitor);
  }

  // A set of all the temp classes we have handed out. We have to fix up references to these.
  // For simplicity, we store the temp classes as JNI global references in a vector. Normally a
  // Prepare event will closely follow, so the vector should be small.
  std::mutex temp_classes_lock;
  std::vector<jclass> temp_classes;

  EventHandler* event_handler = nullptr;
};

ClassCallback gClassCallback;

void ClassUtil::Register(EventHandler* handler) {
  gClassCallback.event_handler = handler;
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add load callback");
  art::Runtime::Current()->GetRuntimeCallbacks()->AddClassLoadCallback(&gClassCallback);
}

void ClassUtil::Unregister() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Remove thread callback");
  art::Runtime* runtime = art::Runtime::Current();
  runtime->GetRuntimeCallbacks()->RemoveClassLoadCallback(&gClassCallback);
}

jvmtiError ClassUtil::GetClassFields(jvmtiEnv* env,
                                     jclass jklass,
                                     jint* field_count_ptr,
                                     jfieldID** fields_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  // Check if this class is a temporary class object used for loading. Since we are seeing it the
  // class must not have been prepared yet since otherwise the fixup would have gotten the jobject
  // to point to the final class object.
  if (klass->IsTemp() || klass->IsRetired()) {
    return ERR(CLASS_NOT_PREPARED);
  }

  if (field_count_ptr == nullptr || fields_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::IterationRange<art::StrideIterator<art::ArtField>> ifields = klass->GetIFields();
  art::IterationRange<art::StrideIterator<art::ArtField>> sfields = klass->GetSFields();
  size_t array_size = klass->NumInstanceFields() + klass->NumStaticFields();

  unsigned char* out_ptr;
  jvmtiError allocError = env->Allocate(array_size * sizeof(jfieldID), &out_ptr);
  if (allocError != ERR(NONE)) {
    return allocError;
  }
  jfieldID* field_array = reinterpret_cast<jfieldID*>(out_ptr);

  size_t array_idx = 0;
  for (art::ArtField& field : sfields) {
    field_array[array_idx] = art::jni::EncodeArtField(&field);
    ++array_idx;
  }
  for (art::ArtField& field : ifields) {
    field_array[array_idx] = art::jni::EncodeArtField(&field);
    ++array_idx;
  }

  *field_count_ptr = static_cast<jint>(array_size);
  *fields_ptr = field_array;

  return ERR(NONE);
}

jvmtiError ClassUtil::GetClassMethods(jvmtiEnv* env,
                                      jclass jklass,
                                      jint* method_count_ptr,
                                      jmethodID** methods_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  // Check if this class is a temporary class object used for loading. Since we are seeing it the
  // class must not have been prepared yet since otherwise the fixup would have gotten the jobject
  // to point to the final class object.
  if (klass->IsTemp() || klass->IsRetired()) {
    return ERR(CLASS_NOT_PREPARED);
  }

  if (method_count_ptr == nullptr || methods_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  size_t array_size = klass->NumDeclaredVirtualMethods() + klass->NumDirectMethods();
  unsigned char* out_ptr;
  jvmtiError allocError = env->Allocate(array_size * sizeof(jmethodID), &out_ptr);
  if (allocError != ERR(NONE)) {
    return allocError;
  }
  jmethodID* method_array = reinterpret_cast<jmethodID*>(out_ptr);

  if (art::kIsDebugBuild) {
    size_t count = 0;
    for (auto& m ATTRIBUTE_UNUSED : klass->GetDeclaredMethods(art::kRuntimePointerSize)) {
      count++;
    }
    CHECK_EQ(count, klass->NumDirectMethods() + klass->NumDeclaredVirtualMethods());
  }

  size_t array_idx = 0;
  for (auto& m : klass->GetDeclaredMethods(art::kRuntimePointerSize)) {
    method_array[array_idx] = art::jni::EncodeArtMethod(&m);
    ++array_idx;
  }

  *method_count_ptr = static_cast<jint>(array_size);
  *methods_ptr = method_array;

  return ERR(NONE);
}

jvmtiError ClassUtil::GetImplementedInterfaces(jvmtiEnv* env,
                                               jclass jklass,
                                               jint* interface_count_ptr,
                                               jclass** interfaces_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  if (interface_count_ptr == nullptr || interfaces_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  // Need to handle array specifically. Arrays implement Serializable and Cloneable, but the
  // spec says these should not be reported.
  if (klass->IsArrayClass()) {
    *interface_count_ptr = 0;
    *interfaces_ptr = nullptr;  // TODO: Should we allocate a dummy here?
    return ERR(NONE);
  }

  size_t array_size = klass->NumDirectInterfaces();
  unsigned char* out_ptr;
  jvmtiError allocError = env->Allocate(array_size * sizeof(jclass), &out_ptr);
  if (allocError != ERR(NONE)) {
    return allocError;
  }
  jclass* interface_array = reinterpret_cast<jclass*>(out_ptr);

  art::StackHandleScope<1> hs(soa.Self());
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(klass));

  for (uint32_t idx = 0; idx != array_size; ++idx) {
    art::ObjPtr<art::mirror::Class> inf_klass =
        art::mirror::Class::ResolveDirectInterface(soa.Self(), h_klass, idx);
    if (inf_klass == nullptr) {
      soa.Self()->ClearException();
      env->Deallocate(out_ptr);
      // TODO: What is the right error code here?
      return ERR(INTERNAL);
    }
    interface_array[idx] = soa.AddLocalReference<jclass>(inf_klass);
  }

  *interface_count_ptr = static_cast<jint>(array_size);
  *interfaces_ptr = interface_array;

  return ERR(NONE);
}

jvmtiError ClassUtil::GetClassSignature(jvmtiEnv* env,
                                         jclass jklass,
                                         char** signature_ptr,
                                         char** generic_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  JvmtiUniquePtr<char[]> sig_copy;
  if (signature_ptr != nullptr) {
    std::string storage;
    const char* descriptor = klass->GetDescriptor(&storage);

    jvmtiError ret;
    sig_copy = CopyString(env, descriptor, &ret);
    if (sig_copy == nullptr) {
      return ret;
    }
    *signature_ptr = sig_copy.get();
  }

  if (generic_ptr != nullptr) {
    *generic_ptr = nullptr;
    if (!klass->IsProxyClass() && klass->GetDexCache() != nullptr) {
      art::StackHandleScope<1> hs(soa.Self());
      art::Handle<art::mirror::Class> h_klass = hs.NewHandle(klass);
      art::mirror::ObjectArray<art::mirror::String>* str_array =
          art::annotations::GetSignatureAnnotationForClass(h_klass);
      if (str_array != nullptr) {
        std::ostringstream oss;
        for (int32_t i = 0; i != str_array->GetLength(); ++i) {
          oss << str_array->Get(i)->ToModifiedUtf8();
        }
        std::string output_string = oss.str();
        jvmtiError ret;
        JvmtiUniquePtr<char[]> copy = CopyString(env, output_string.c_str(), &ret);
        if (copy == nullptr) {
          return ret;
        }
        *generic_ptr = copy.release();
      } else if (soa.Self()->IsExceptionPending()) {
        // TODO: Should we report an error here?
        soa.Self()->ClearException();
      }
    }
  }

  // Everything is fine, release the buffers.
  sig_copy.release();

  return ERR(NONE);
}

jvmtiError ClassUtil::GetClassStatus(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                     jclass jklass,
                                     jint* status_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  if (status_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  if (klass->IsArrayClass()) {
    *status_ptr = JVMTI_CLASS_STATUS_ARRAY;
  } else if (klass->IsPrimitive()) {
    *status_ptr = JVMTI_CLASS_STATUS_PRIMITIVE;
  } else {
    *status_ptr = JVMTI_CLASS_STATUS_VERIFIED;  // All loaded classes are structurally verified.
    // This is finicky. If there's an error, we'll say it wasn't prepared.
    if (klass->IsResolved()) {
      *status_ptr |= JVMTI_CLASS_STATUS_PREPARED;
    }
    if (klass->IsInitialized()) {
      *status_ptr |= JVMTI_CLASS_STATUS_INITIALIZED;
    }
    // Technically the class may be erroneous for other reasons, but we do not have enough info.
    if (klass->IsErroneous()) {
      *status_ptr |= JVMTI_CLASS_STATUS_ERROR;
    }
  }

  return ERR(NONE);
}

template <typename T>
static jvmtiError ClassIsT(jclass jklass, T test, jboolean* is_t_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  if (is_t_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  *is_t_ptr = test(klass) ? JNI_TRUE : JNI_FALSE;
  return ERR(NONE);
}

jvmtiError ClassUtil::IsInterface(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                  jclass jklass,
                                  jboolean* is_interface_ptr) {
  auto test = [](art::ObjPtr<art::mirror::Class> klass) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return klass->IsInterface();
  };
  return ClassIsT(jklass, test, is_interface_ptr);
}

jvmtiError ClassUtil::IsArrayClass(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                   jclass jklass,
                                   jboolean* is_array_class_ptr) {
  auto test = [](art::ObjPtr<art::mirror::Class> klass) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    return klass->IsArrayClass();
  };
  return ClassIsT(jklass, test, is_array_class_ptr);
}

// Keep this in sync with Class.getModifiers().
static uint32_t ClassGetModifiers(art::Thread* self, art::ObjPtr<art::mirror::Class> klass)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  if (klass->IsArrayClass()) {
    uint32_t component_modifiers = ClassGetModifiers(self, klass->GetComponentType());
    if ((component_modifiers & art::kAccInterface) != 0) {
      component_modifiers &= ~(art::kAccInterface | art::kAccStatic);
    }
    return art::kAccAbstract | art::kAccFinal | component_modifiers;
  }

  uint32_t modifiers = klass->GetAccessFlags() & art::kAccJavaFlagsMask;

  art::StackHandleScope<1> hs(self);
  art::Handle<art::mirror::Class> h_klass(hs.NewHandle(klass));
  return art::mirror::Class::GetInnerClassFlags(h_klass, modifiers);
}

jvmtiError ClassUtil::GetClassModifiers(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                        jclass jklass,
                                        jint* modifiers_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  if (modifiers_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  *modifiers_ptr = ClassGetModifiers(soa.Self(), klass);

  return ERR(NONE);
}

jvmtiError ClassUtil::GetClassLoader(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                     jclass jklass,
                                     jobject* classloader_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> klass = soa.Decode<art::mirror::Class>(jklass);
  if (klass == nullptr) {
    return ERR(INVALID_CLASS);
  }

  if (classloader_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  *classloader_ptr = soa.AddLocalReference<jobject>(klass->GetClassLoader());

  return ERR(NONE);
}

// Copies unique class descriptors into the classes list from dex_files.
static jvmtiError CopyClassDescriptors(jvmtiEnv* env,
                                       const std::vector<const art::DexFile*>& dex_files,
                                       /*out*/jint* count_ptr,
                                       /*out*/char*** classes) {
  jvmtiError res = OK;
  std::set<art::StringPiece> unique_descriptors;
  std::vector<const char*> descriptors;
  auto add_descriptor = [&](const char* desc) {
    // Don't add duplicates.
    if (res == OK && unique_descriptors.find(desc) == unique_descriptors.end()) {
      // The desc will remain valid since we hold a ref to the class_loader.
      unique_descriptors.insert(desc);
      descriptors.push_back(CopyString(env, desc, &res).release());
    }
  };
  for (const art::DexFile* dex_file : dex_files) {
    uint32_t num_defs = dex_file->NumClassDefs();
    for (uint32_t i = 0; i < num_defs; i++) {
      add_descriptor(dex_file->GetClassDescriptor(dex_file->GetClassDef(i)));
    }
  }
  char** out_data = nullptr;
  if (res == OK) {
    res = env->Allocate(sizeof(char*) * descriptors.size(),
                        reinterpret_cast<unsigned char**>(&out_data));
  }
  if (res != OK) {
    env->Deallocate(reinterpret_cast<unsigned char*>(out_data));
    // Failed to allocate. Cleanup everything.
    for (const char* data : descriptors) {
      env->Deallocate(reinterpret_cast<unsigned char*>(const_cast<char*>(data)));
    }
    descriptors.clear();
    return res;
  }
  // Everything is good.
  memcpy(out_data, descriptors.data(), sizeof(char*) * descriptors.size());
  *count_ptr = static_cast<jint>(descriptors.size());
  *classes = out_data;
  return OK;
}

jvmtiError ClassUtil::GetClassLoaderClassDescriptors(jvmtiEnv* env,
                                                     jobject loader,
                                                     /*out*/jint* count_ptr,
                                                     /*out*/char*** classes) {
  art::Thread* self = art::Thread::Current();
  if (env == nullptr) {
    return ERR(INVALID_ENVIRONMENT);
  } else if (self == nullptr) {
    return ERR(UNATTACHED_THREAD);
  } else if (count_ptr == nullptr || classes == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::JNIEnvExt* jnienv = self->GetJniEnv();
  if (loader == nullptr ||
      jnienv->IsInstanceOf(loader, art::WellKnownClasses::java_lang_BootClassLoader)) {
    // We can just get the dex files directly for the boot class path.
    return CopyClassDescriptors(env,
                                art::Runtime::Current()->GetClassLinker()->GetBootClassPath(),
                                count_ptr,
                                classes);
  }
  if (!jnienv->IsInstanceOf(loader, art::WellKnownClasses::java_lang_ClassLoader)) {
    return ERR(ILLEGAL_ARGUMENT);
  } else if (!jnienv->IsInstanceOf(loader,
                                   art::WellKnownClasses::dalvik_system_BaseDexClassLoader)) {
    LOG(ERROR) << "GetClassLoaderClassDescriptors is only implemented for BootClassPath and "
               << "dalvik.system.BaseDexClassLoader class loaders";
    // TODO Possibly return OK With no classes would  be better since these ones cannot have any
    // real classes associated with them.
    return ERR(NOT_IMPLEMENTED);
  }

  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);
  art::Handle<art::mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<art::mirror::ClassLoader>(loader)));
  std::vector<const art::DexFile*> dex_files;
  art::VisitClassLoaderDexFiles(
      soa,
      class_loader,
      [&](const art::DexFile* dex_file) {
        dex_files.push_back(dex_file);
        return true;  // Continue with other dex files.
      });
  // We hold the loader so the dex files won't go away until after this call at worst.
  return CopyClassDescriptors(env, dex_files, count_ptr, classes);
}

jvmtiError ClassUtil::GetClassLoaderClasses(jvmtiEnv* env,
                                            jobject initiating_loader,
                                            jint* class_count_ptr,
                                            jclass** classes_ptr) {
  UNUSED(env, initiating_loader, class_count_ptr, classes_ptr);

  if (class_count_ptr == nullptr || classes_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::Thread* self = art::Thread::Current();
  if (!self->GetJniEnv()->IsInstanceOf(initiating_loader,
                                       art::WellKnownClasses::java_lang_ClassLoader)) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  if (self->GetJniEnv()->IsInstanceOf(initiating_loader,
                                      art::WellKnownClasses::java_lang_BootClassLoader)) {
    // Need to use null for the BootClassLoader.
    initiating_loader = nullptr;
  }

  art::ScopedObjectAccess soa(self);
  art::ObjPtr<art::mirror::ClassLoader> class_loader =
      soa.Decode<art::mirror::ClassLoader>(initiating_loader);

  art::ClassLinker* class_linker = art::Runtime::Current()->GetClassLinker();

  art::ReaderMutexLock mu(self, *art::Locks::classlinker_classes_lock_);

  art::ClassTable* class_table = class_linker->ClassTableForClassLoader(class_loader);
  if (class_table == nullptr) {
    // Nothing loaded.
    *class_count_ptr = 0;
    *classes_ptr = nullptr;
    return ERR(NONE);
  }

  struct ClassTableCount {
    bool operator()(art::ObjPtr<art::mirror::Class> klass) {
      DCHECK(klass != nullptr);
      ++count;
      return true;
    }

    size_t count = 0;
  };
  ClassTableCount ctc;
  class_table->Visit(ctc);

  if (ctc.count == 0) {
    // Nothing loaded.
    *class_count_ptr = 0;
    *classes_ptr = nullptr;
    return ERR(NONE);
  }

  unsigned char* data;
  jvmtiError data_result = env->Allocate(ctc.count * sizeof(jclass), &data);
  if (data_result != ERR(NONE)) {
    return data_result;
  }
  jclass* class_array = reinterpret_cast<jclass*>(data);

  struct ClassTableFill {
    bool operator()(art::ObjPtr<art::mirror::Class> klass)
        REQUIRES_SHARED(art::Locks::mutator_lock_) {
      DCHECK(klass != nullptr);
      DCHECK_LT(count, ctc_ref.count);
      local_class_array[count++] = soa_ptr->AddLocalReference<jclass>(klass);
      return true;
    }

    jclass* local_class_array;
    const ClassTableCount& ctc_ref;
    art::ScopedObjectAccess* soa_ptr;
    size_t count;
  };
  ClassTableFill ctf = { class_array, ctc, &soa, 0 };
  class_table->Visit(ctf);
  DCHECK_EQ(ctc.count, ctf.count);

  *class_count_ptr = ctc.count;
  *classes_ptr = class_array;

  return ERR(NONE);
}

jvmtiError ClassUtil::GetClassVersionNumbers(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                             jclass jklass,
                                             jint* minor_version_ptr,
                                             jint* major_version_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  if (jklass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  art::ObjPtr<art::mirror::Object> jklass_obj = soa.Decode<art::mirror::Object>(jklass);
  if (!jklass_obj->IsClass()) {
    return ERR(INVALID_CLASS);
  }
  art::ObjPtr<art::mirror::Class> klass = jklass_obj->AsClass();
  if (klass->IsPrimitive() || klass->IsArrayClass()) {
    return ERR(INVALID_CLASS);
  }

  if (minor_version_ptr == nullptr || major_version_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  // Note: proxies will show the dex file version of java.lang.reflect.Proxy, as that is
  //       what their dex cache copies from.
  uint32_t version = klass->GetDexFile().GetHeader().GetVersion();

  *major_version_ptr = static_cast<jint>(version);
  *minor_version_ptr = 0;

  return ERR(NONE);
}

jvmtiError ClassUtil::GetSourceFileName(jvmtiEnv* env, jclass jklass, char** source_name_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  if (jklass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  art::ObjPtr<art::mirror::Object> jklass_obj = soa.Decode<art::mirror::Object>(jklass);
  if (!jklass_obj->IsClass()) {
    return ERR(INVALID_CLASS);
  }
  art::ObjPtr<art::mirror::Class> klass = jklass_obj->AsClass();
  if (klass->IsPrimitive() || klass->IsArrayClass()) {
    return ERR(ABSENT_INFORMATION);
  }
  JvmtiUniquePtr<char[]> source_copy;
  const char* file_name = klass->GetSourceFile();
  if (file_name == nullptr) {
    return ERR(ABSENT_INFORMATION);
  }
  jvmtiError ret;
  source_copy = CopyString(env, file_name, &ret);
  if (source_copy == nullptr) {
    return ret;
  }
  *source_name_ptr = source_copy.release();
  return OK;
}

jvmtiError ClassUtil::GetSourceDebugExtension(jvmtiEnv* env,
                                              jclass jklass,
                                              char** source_debug_extension_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  if (jklass == nullptr) {
    return ERR(INVALID_CLASS);
  }
  art::ObjPtr<art::mirror::Object> jklass_obj = soa.Decode<art::mirror::Object>(jklass);
  if (!jklass_obj->IsClass()) {
    return ERR(INVALID_CLASS);
  }
  art::StackHandleScope<1> hs(art::Thread::Current());
  art::Handle<art::mirror::Class> klass(hs.NewHandle(jklass_obj->AsClass()));
  if (klass->IsPrimitive() || klass->IsArrayClass()) {
    return ERR(ABSENT_INFORMATION);
  }
  JvmtiUniquePtr<char[]> ext_copy;
  const char* data = art::annotations::GetSourceDebugExtension(klass);
  if (data == nullptr) {
    return ERR(ABSENT_INFORMATION);
  }
  jvmtiError ret;
  ext_copy = CopyString(env, data, &ret);
  if (ext_copy == nullptr) {
    return ret;
  }
  *source_debug_extension_ptr = ext_copy.release();
  return OK;
}

}  // namespace openjdkjvmti
