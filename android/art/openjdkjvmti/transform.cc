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

#include <stddef.h>
#include <sys/types.h>

#include <unordered_map>
#include <unordered_set>

#include "transform.h"

#include "art_method.h"
#include "base/array_ref.h"
#include "class_linker.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "dex/utf.h"
#include "events-inl.h"
#include "fault_handler.h"
#include "gc_root-inl.h"
#include "globals.h"
#include "jni_env_ext-inl.h"
#include "jvalue.h"
#include "jvmti.h"
#include "linear_alloc.h"
#include "mem_map.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/class_loader-inl.h"
#include "mirror/string-inl.h"
#include "oat_file.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread_list.h"
#include "ti_redefine.h"
#include "transform.h"
#include "utils/dex_cache_arrays_layout-inl.h"

namespace openjdkjvmti {

// A FaultHandler that will deal with initializing ClassDefinitions when they are actually needed.
class TransformationFaultHandler FINAL : public art::FaultHandler {
 public:
  explicit TransformationFaultHandler(art::FaultManager* manager)
      : art::FaultHandler(manager),
        uninitialized_class_definitions_lock_("JVMTI Initialized class definitions lock",
                                              art::LockLevel::kSignalHandlingLock),
        class_definition_initialized_cond_("JVMTI Initialized class definitions condition",
                                           uninitialized_class_definitions_lock_) {
    manager->AddHandler(this, /* generated_code */ false);
  }

  ~TransformationFaultHandler() {
    art::MutexLock mu(art::Thread::Current(), uninitialized_class_definitions_lock_);
    uninitialized_class_definitions_.clear();
  }

  bool Action(int sig, siginfo_t* siginfo, void* context ATTRIBUTE_UNUSED) OVERRIDE {
    DCHECK_EQ(sig, SIGSEGV);
    art::Thread* self = art::Thread::Current();
    if (UNLIKELY(uninitialized_class_definitions_lock_.IsExclusiveHeld(self))) {
      if (self != nullptr) {
        LOG(FATAL) << "Recursive call into Transformation fault handler!";
        UNREACHABLE();
      } else {
        LOG(ERROR) << "Possible deadlock due to recursive signal delivery of segv.";
      }
    }
    uintptr_t ptr = reinterpret_cast<uintptr_t>(siginfo->si_addr);
    ArtClassDefinition* res = nullptr;

    {
      // NB Technically using a mutex and condition variables here is non-posix compliant but
      // everything should be fine since both glibc and bionic implementations of mutexs and
      // condition variables work fine so long as the thread was not interrupted during a
      // lock/unlock (which it wasn't) on all architectures we care about.
      art::MutexLock mu(self, uninitialized_class_definitions_lock_);
      auto it = std::find_if(uninitialized_class_definitions_.begin(),
                             uninitialized_class_definitions_.end(),
                             [&](const auto op) { return op->ContainsAddress(ptr); });
      if (it != uninitialized_class_definitions_.end()) {
        res = *it;
        // Remove the class definition.
        uninitialized_class_definitions_.erase(it);
        // Put it in the initializing list
        initializing_class_definitions_.push_back(res);
      } else {
        // Wait for the ptr to be initialized (if it is currently initializing).
        while (DefinitionIsInitializing(ptr)) {
          WaitForClassInitializationToFinish();
        }
        // Return true (continue with user code) if we find that the definition has been
        // initialized. Return false (continue on to next signal handler) if the definition is not
        // initialized or found.
        return std::find_if(initialized_class_definitions_.begin(),
                            initialized_class_definitions_.end(),
                            [&](const auto op) { return op->ContainsAddress(ptr); }) !=
            initialized_class_definitions_.end();
      }
    }

    if (LIKELY(self != nullptr)) {
      CHECK_EQ(self->GetState(), art::ThreadState::kNative)
          << "Transformation fault handler occurred outside of native mode";
    }

    VLOG(signals) << "Lazy initialization of dex file for transformation of " << res->GetName()
                  << " during SEGV";
    res->InitializeMemory();

    {
      art::MutexLock mu(self, uninitialized_class_definitions_lock_);
      // Move to initialized state and notify waiters.
      initializing_class_definitions_.erase(std::find(initializing_class_definitions_.begin(),
                                                      initializing_class_definitions_.end(),
                                                      res));
      initialized_class_definitions_.push_back(res);
      class_definition_initialized_cond_.Broadcast(self);
    }

    return true;
  }

  void RemoveDefinition(ArtClassDefinition* def) REQUIRES(!uninitialized_class_definitions_lock_) {
    art::MutexLock mu(art::Thread::Current(), uninitialized_class_definitions_lock_);
    auto it = std::find(uninitialized_class_definitions_.begin(),
                        uninitialized_class_definitions_.end(),
                        def);
    if (it != uninitialized_class_definitions_.end()) {
      uninitialized_class_definitions_.erase(it);
      return;
    }
    while (std::find(initializing_class_definitions_.begin(),
                     initializing_class_definitions_.end(),
                     def) != initializing_class_definitions_.end()) {
      WaitForClassInitializationToFinish();
    }
    it = std::find(initialized_class_definitions_.begin(),
                   initialized_class_definitions_.end(),
                   def);
    CHECK(it != initialized_class_definitions_.end()) << "Could not find class definition for "
                                                      << def->GetName();
    initialized_class_definitions_.erase(it);
  }

  void AddArtDefinition(ArtClassDefinition* def) REQUIRES(!uninitialized_class_definitions_lock_) {
    DCHECK(def->IsLazyDefinition());
    art::MutexLock mu(art::Thread::Current(), uninitialized_class_definitions_lock_);
    uninitialized_class_definitions_.push_back(def);
  }

 private:
  bool DefinitionIsInitializing(uintptr_t ptr) REQUIRES(uninitialized_class_definitions_lock_) {
    return std::find_if(initializing_class_definitions_.begin(),
                        initializing_class_definitions_.end(),
                        [&](const auto op) { return op->ContainsAddress(ptr); }) !=
        initializing_class_definitions_.end();
  }

  void WaitForClassInitializationToFinish() REQUIRES(uninitialized_class_definitions_lock_) {
    class_definition_initialized_cond_.Wait(art::Thread::Current());
  }

  art::Mutex uninitialized_class_definitions_lock_ ACQUIRED_BEFORE(art::Locks::abort_lock_);
  art::ConditionVariable class_definition_initialized_cond_
      GUARDED_BY(uninitialized_class_definitions_lock_);

  // A list of the class definitions that have a non-readable map.
  std::vector<ArtClassDefinition*> uninitialized_class_definitions_
      GUARDED_BY(uninitialized_class_definitions_lock_);

  // A list of class definitions that are currently undergoing unquickening. Threads should wait
  // until the definition is no longer in this before returning.
  std::vector<ArtClassDefinition*> initializing_class_definitions_
      GUARDED_BY(uninitialized_class_definitions_lock_);

  // A list of class definitions that are already unquickened. Threads should immediately return if
  // it is here.
  std::vector<ArtClassDefinition*> initialized_class_definitions_
      GUARDED_BY(uninitialized_class_definitions_lock_);
};

static TransformationFaultHandler* gTransformFaultHandler = nullptr;

void Transformer::Setup() {
  // Although we create this the fault handler is actually owned by the 'art::fault_manager' which
  // will take care of destroying it.
  if (art::MemMap::kCanReplaceMapping && ArtClassDefinition::kEnableOnDemandDexDequicken) {
    gTransformFaultHandler = new TransformationFaultHandler(&art::fault_manager);
  }
}

// Simple helper to add and remove the class definition from the fault handler.
class ScopedDefinitionHandler {
 public:
  explicit ScopedDefinitionHandler(ArtClassDefinition* def)
      : def_(def), is_lazy_(def_->IsLazyDefinition()) {
    if (is_lazy_) {
      gTransformFaultHandler->AddArtDefinition(def_);
    }
  }

  ~ScopedDefinitionHandler() {
    if (is_lazy_) {
      gTransformFaultHandler->RemoveDefinition(def_);
    }
  }

 private:
  ArtClassDefinition* def_;
  bool is_lazy_;
};

// Initialize templates.
template
void Transformer::TransformSingleClassDirect<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
    EventHandler* event_handler, art::Thread* self, /*in-out*/ArtClassDefinition* def);
template
void Transformer::TransformSingleClassDirect<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
    EventHandler* event_handler, art::Thread* self, /*in-out*/ArtClassDefinition* def);

template<ArtJvmtiEvent kEvent>
void Transformer::TransformSingleClassDirect(EventHandler* event_handler,
                                             art::Thread* self,
                                             /*in-out*/ArtClassDefinition* def) {
  static_assert(kEvent == ArtJvmtiEvent::kClassFileLoadHookNonRetransformable ||
                kEvent == ArtJvmtiEvent::kClassFileLoadHookRetransformable,
                "bad event type");
  // We don't want to do transitions between calling the event and setting the new data so change to
  // native state early. This also avoids any problems that the FaultHandler might have in
  // determining if an access to the dex_data is from generated code or not.
  art::ScopedThreadStateChange stsc(self, art::ThreadState::kNative);
  ScopedDefinitionHandler handler(def);
  jint new_len = -1;
  unsigned char* new_data = nullptr;
  art::ArrayRef<const unsigned char> dex_data = def->GetDexData();
  event_handler->DispatchEvent<kEvent>(
      self,
      static_cast<JNIEnv*>(self->GetJniEnv()),
      def->GetClass(),
      def->GetLoader(),
      def->GetName().c_str(),
      def->GetProtectionDomain(),
      static_cast<jint>(dex_data.size()),
      dex_data.data(),
      /*out*/&new_len,
      /*out*/&new_data);
  def->SetNewDexData(new_len, new_data);
}

jvmtiError Transformer::RetransformClassesDirect(
      EventHandler* event_handler,
      art::Thread* self,
      /*in-out*/std::vector<ArtClassDefinition>* definitions) {
  for (ArtClassDefinition& def : *definitions) {
    TransformSingleClassDirect<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(event_handler,
                                                                                 self,
                                                                                 &def);
  }
  return OK;
}

jvmtiError Transformer::RetransformClasses(ArtJvmTiEnv* env,
                                           EventHandler* event_handler,
                                           art::Runtime* runtime,
                                           art::Thread* self,
                                           jint class_count,
                                           const jclass* classes,
                                           /*out*/std::string* error_msg) {
  if (env == nullptr) {
    *error_msg = "env was null!";
    return ERR(INVALID_ENVIRONMENT);
  } else if (class_count < 0) {
    *error_msg = "class_count was less then 0";
    return ERR(ILLEGAL_ARGUMENT);
  } else if (class_count == 0) {
    // We don't actually need to do anything. Just return OK.
    return OK;
  } else if (classes == nullptr) {
    *error_msg = "null classes!";
    return ERR(NULL_POINTER);
  }
  // A holder that will Deallocate all the class bytes buffers on destruction.
  std::vector<ArtClassDefinition> definitions;
  jvmtiError res = OK;
  for (jint i = 0; i < class_count; i++) {
    res = Redefiner::GetClassRedefinitionError(classes[i], error_msg);
    if (res != OK) {
      return res;
    }
    ArtClassDefinition def;
    res = def.Init(self, classes[i]);
    if (res != OK) {
      return res;
    }
    definitions.push_back(std::move(def));
  }
  res = RetransformClassesDirect(event_handler, self, &definitions);
  if (res != OK) {
    return res;
  }
  return Redefiner::RedefineClassesDirect(env, runtime, self, definitions, error_msg);
}

// TODO Move this somewhere else, ti_class?
jvmtiError GetClassLocation(ArtJvmTiEnv* env, jclass klass, /*out*/std::string* location) {
  JNIEnv* jni_env = nullptr;
  jint ret = env->art_vm->GetEnv(reinterpret_cast<void**>(&jni_env), JNI_VERSION_1_1);
  if (ret != JNI_OK) {
    // TODO Different error might be better?
    return ERR(INTERNAL);
  }
  art::ScopedObjectAccess soa(jni_env);
  art::StackHandleScope<1> hs(art::Thread::Current());
  art::Handle<art::mirror::Class> hs_klass(hs.NewHandle(soa.Decode<art::mirror::Class>(klass)));
  const art::DexFile& dex = hs_klass->GetDexFile();
  *location = dex.GetLocation();
  return OK;
}

}  // namespace openjdkjvmti
