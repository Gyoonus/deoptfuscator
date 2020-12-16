/* Copyright (C) 2017 The Android Open Source Project
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

#include "ti_thread.h"

#include <android-base/logging.h>
#include <android-base/strings.h>

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "base/mutex.h"
#include "events-inl.h"
#include "gc/system_weak.h"
#include "gc/collector_type.h"
#include "gc/gc_cause.h"
#include "gc/scoped_gc_critical_section.h"
#include "gc_root-inl.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"
#include "obj_ptr.h"
#include "runtime.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_phase.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

art::ArtField* ThreadUtil::context_class_loader_ = nullptr;

struct ThreadCallback : public art::ThreadLifecycleCallback {
  jthread GetThreadObject(art::Thread* self) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (self->GetPeer() == nullptr) {
      return nullptr;
    }
    return self->GetJniEnv()->AddLocalReference<jthread>(self->GetPeer());
  }

  template <ArtJvmtiEvent kEvent>
  void Post(art::Thread* self) REQUIRES_SHARED(art::Locks::mutator_lock_) {
    DCHECK_EQ(self, art::Thread::Current());
    ScopedLocalRef<jthread> thread(self->GetJniEnv(), GetThreadObject(self));
    art::ScopedThreadSuspension sts(self, art::ThreadState::kNative);
    event_handler->DispatchEvent<kEvent>(self,
                                         reinterpret_cast<JNIEnv*>(self->GetJniEnv()),
                                         thread.get());
  }

  void ThreadStart(art::Thread* self) OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (!started) {
      // Runtime isn't started. We only expect at most the signal handler or JIT threads to be
      // started here.
      if (art::kIsDebugBuild) {
        std::string name;
        self->GetThreadName(name);
        if (name != "JDWP" &&
            name != "Signal Catcher" &&
            !android::base::StartsWith(name, "Jit thread pool")) {
          LOG(FATAL) << "Unexpected thread before start: " << name << " id: "
                     << self->GetThreadId();
        }
      }
      return;
    }
    Post<ArtJvmtiEvent::kThreadStart>(self);
  }

  void ThreadDeath(art::Thread* self) OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    Post<ArtJvmtiEvent::kThreadEnd>(self);
  }

  EventHandler* event_handler = nullptr;
  bool started = false;
};

ThreadCallback gThreadCallback;

void ThreadUtil::Register(EventHandler* handler) {
  art::Runtime* runtime = art::Runtime::Current();

  gThreadCallback.started = runtime->IsStarted();
  gThreadCallback.event_handler = handler;

  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add thread callback");
  runtime->GetRuntimeCallbacks()->AddThreadLifecycleCallback(&gThreadCallback);
}

void ThreadUtil::VMInitEventSent() {
  // We should have already started.
  DCHECK(gThreadCallback.started);
  // We moved to VMInit. Report the main thread as started (it was attached early, and must not be
  // reported until Init.
  gThreadCallback.Post<ArtJvmtiEvent::kThreadStart>(art::Thread::Current());
}

void ThreadUtil::CacheData() {
  // We must have started since it is now safe to cache our data;
  gThreadCallback.started = true;
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::ObjPtr<art::mirror::Class> thread_class =
      soa.Decode<art::mirror::Class>(art::WellKnownClasses::java_lang_Thread);
  CHECK(thread_class != nullptr);
  context_class_loader_ = thread_class->FindDeclaredInstanceField("contextClassLoader",
                                                                  "Ljava/lang/ClassLoader;");
  CHECK(context_class_loader_ != nullptr);
}

void ThreadUtil::Unregister() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Remove thread callback");
  art::Runtime* runtime = art::Runtime::Current();
  runtime->GetRuntimeCallbacks()->RemoveThreadLifecycleCallback(&gThreadCallback);
}

jvmtiError ThreadUtil::GetCurrentThread(jvmtiEnv* env ATTRIBUTE_UNUSED, jthread* thread_ptr) {
  art::Thread* self = art::Thread::Current();

  art::ScopedObjectAccess soa(self);

  jthread thread_peer;
  if (self->IsStillStarting()) {
    thread_peer = nullptr;
  } else {
    thread_peer = soa.AddLocalReference<jthread>(self->GetPeer());
  }

  *thread_ptr = thread_peer;
  return ERR(NONE);
}

// Get the native thread. The spec says a null object denotes the current thread.
bool ThreadUtil::GetNativeThread(jthread thread,
                                 const art::ScopedObjectAccessAlreadyRunnable& soa,
                                 /*out*/ art::Thread** thr,
                                 /*out*/ jvmtiError* err) {
  if (thread == nullptr) {
    *thr = art::Thread::Current();
    return true;
  } else if (!soa.Env()->IsInstanceOf(thread, art::WellKnownClasses::java_lang_Thread)) {
    *err = ERR(INVALID_THREAD);
    return false;
  } else {
    *thr = art::Thread::FromManagedThread(soa, thread);
    return true;
  }
}

bool ThreadUtil::GetAliveNativeThread(jthread thread,
                                      const art::ScopedObjectAccessAlreadyRunnable& soa,
                                      /*out*/ art::Thread** thr,
                                      /*out*/ jvmtiError* err) {
  if (!GetNativeThread(thread, soa, thr, err)) {
    return false;
  } else if (*thr == nullptr || (*thr)->GetState() == art::ThreadState::kTerminated) {
    *err = ERR(THREAD_NOT_ALIVE);
    return false;
  } else {
    return true;
  }
}

jvmtiError ThreadUtil::GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr) {
  if (info_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  if (!PhaseUtil::IsLivePhase()) {
    return JVMTI_ERROR_WRONG_PHASE;
  }

  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::MutexLock mu(self, *art::Locks::thread_list_lock_);

  art::Thread* target;
  jvmtiError err = ERR(INTERNAL);
  if (!GetNativeThread(thread, soa, &target, &err)) {
    return err;
  }

  JvmtiUniquePtr<char[]> name_uptr;
  if (target != nullptr) {
    // Have a native thread object, this thread is alive.
    std::string name;
    target->GetThreadName(name);
    jvmtiError name_result;
    name_uptr = CopyString(env, name.c_str(), &name_result);
    if (name_uptr == nullptr) {
      return name_result;
    }
    info_ptr->name = name_uptr.get();

    info_ptr->priority = target->GetNativePriority();

    info_ptr->is_daemon = target->IsDaemon();

    art::ObjPtr<art::mirror::Object> peer = target->GetPeerFromOtherThread();

    // ThreadGroup.
    if (peer != nullptr) {
      art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_group);
      CHECK(f != nullptr);
      art::ObjPtr<art::mirror::Object> group = f->GetObject(peer);
      info_ptr->thread_group = group == nullptr
                                   ? nullptr
                                   : soa.AddLocalReference<jthreadGroup>(group);
    } else {
      info_ptr->thread_group = nullptr;
    }

    // Context classloader.
    DCHECK(context_class_loader_ != nullptr);
    art::ObjPtr<art::mirror::Object> ccl = peer != nullptr
        ? context_class_loader_->GetObject(peer)
        : nullptr;
    info_ptr->context_class_loader = ccl == nullptr
                                         ? nullptr
                                         : soa.AddLocalReference<jobject>(ccl);
  } else {
    // Only the peer. This thread has either not been started, or is dead. Read things from
    // the Java side.
    art::ObjPtr<art::mirror::Object> peer = soa.Decode<art::mirror::Object>(thread);

    // Name.
    {
      art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_name);
      CHECK(f != nullptr);
      art::ObjPtr<art::mirror::Object> name = f->GetObject(peer);
      std::string name_cpp;
      const char* name_cstr;
      if (name != nullptr) {
        name_cpp = name->AsString()->ToModifiedUtf8();
        name_cstr = name_cpp.c_str();
      } else {
        name_cstr = "";
      }
      jvmtiError name_result;
      name_uptr = CopyString(env, name_cstr, &name_result);
      if (name_uptr == nullptr) {
        return name_result;
      }
      info_ptr->name = name_uptr.get();
    }

    // Priority.
    {
      art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_priority);
      CHECK(f != nullptr);
      info_ptr->priority = static_cast<jint>(f->GetInt(peer));
    }

    // Daemon.
    {
      art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_daemon);
      CHECK(f != nullptr);
      info_ptr->is_daemon = f->GetBoolean(peer) == 0 ? JNI_FALSE : JNI_TRUE;
    }

    // ThreadGroup.
    {
      art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_group);
      CHECK(f != nullptr);
      art::ObjPtr<art::mirror::Object> group = f->GetObject(peer);
      info_ptr->thread_group = group == nullptr
                                   ? nullptr
                                   : soa.AddLocalReference<jthreadGroup>(group);
    }

    // Context classloader.
    DCHECK(context_class_loader_ != nullptr);
    art::ObjPtr<art::mirror::Object> ccl = peer != nullptr
        ? context_class_loader_->GetObject(peer)
        : nullptr;
    info_ptr->context_class_loader = ccl == nullptr
                                         ? nullptr
                                         : soa.AddLocalReference<jobject>(ccl);
  }

  name_uptr.release();

  return ERR(NONE);
}

struct InternalThreadState {
  art::Thread* native_thread;
  art::ThreadState art_state;
  int thread_user_code_suspend_count;
};

// Return the thread's (or current thread, if null) thread state.
static InternalThreadState GetNativeThreadState(art::Thread* target)
    REQUIRES_SHARED(art::Locks::mutator_lock_)
    REQUIRES(art::Locks::thread_list_lock_, art::Locks::user_code_suspension_lock_) {
  InternalThreadState thread_state = {};
  art::MutexLock tscl_mu(art::Thread::Current(), *art::Locks::thread_suspend_count_lock_);
  thread_state.native_thread = target;
  if (target == nullptr || target->IsStillStarting()) {
    thread_state.art_state = art::ThreadState::kStarting;
    thread_state.thread_user_code_suspend_count = 0;
  } else {
    thread_state.art_state = target->GetState();
    thread_state.thread_user_code_suspend_count = target->GetUserCodeSuspendCount();
  }
  return thread_state;
}

static jint GetJvmtiThreadStateFromInternal(const InternalThreadState& state) {
  art::ThreadState internal_thread_state = state.art_state;
  jint jvmti_state = JVMTI_THREAD_STATE_ALIVE;

  if (state.thread_user_code_suspend_count != 0) {
    // Suspended can be set with any thread state so check it here. Even if the thread isn't in
    // kSuspended state it will move to that once it hits a checkpoint so we can still set this.
    jvmti_state |= JVMTI_THREAD_STATE_SUSPENDED;
    // Note: We do not have data about the previous state. Otherwise we should load the previous
    //       state here.
  }

  if (state.native_thread->IsInterrupted()) {
    // Interrupted can be set with any thread state so check it here.
    jvmti_state |= JVMTI_THREAD_STATE_INTERRUPTED;
  }

  // Enumerate all the thread states and fill in the other bits. This contains the results of
  // following the decision tree in the JVMTI spec GetThreadState documentation.
  switch (internal_thread_state) {
    case art::ThreadState::kRunnable:
    case art::ThreadState::kWaitingWeakGcRootRead:
    case art::ThreadState::kSuspended:
      // These are all simply runnable.
      // kRunnable is self-explanatory.
      // kWaitingWeakGcRootRead is set during some operations with strings due to the intern-table
      // so we want to keep it marked as runnable.
      // kSuspended we don't mark since if we don't have a user_code_suspend_count then it is done
      // by the GC and not a JVMTI suspension, which means it cannot be removed by ResumeThread.
      jvmti_state |= JVMTI_THREAD_STATE_RUNNABLE;
      break;
    case art::ThreadState::kNative:
      // kNative means native and runnable. Technically THREAD_STATE_IN_NATIVE can be set with any
      // state but we don't have the information to know if it should be present for any but the
      // kNative state.
      jvmti_state |= (JVMTI_THREAD_STATE_IN_NATIVE |
                      JVMTI_THREAD_STATE_RUNNABLE);
      break;
    case art::ThreadState::kBlocked:
      // Blocked is one of the top level states so it sits alone.
      jvmti_state |= JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER;
      break;
    case art::ThreadState::kWaiting:
      // Object.wait() so waiting, indefinitely, in object.wait.
      jvmti_state |= (JVMTI_THREAD_STATE_WAITING |
                      JVMTI_THREAD_STATE_WAITING_INDEFINITELY |
                      JVMTI_THREAD_STATE_IN_OBJECT_WAIT);
      break;
    case art::ThreadState::kTimedWaiting:
      // Object.wait(long) so waiting, with timeout, in object.wait.
      jvmti_state |= (JVMTI_THREAD_STATE_WAITING |
                      JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT |
                      JVMTI_THREAD_STATE_IN_OBJECT_WAIT);
      break;
    case art::ThreadState::kSleeping:
      // In object.sleep. This is a timed wait caused by sleep.
      jvmti_state |= (JVMTI_THREAD_STATE_WAITING |
                      JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT |
                      JVMTI_THREAD_STATE_SLEEPING);
      break;
    // TODO We might want to print warnings if we have the debugger running while JVMTI agents are
    // attached.
    case art::ThreadState::kWaitingForDebuggerSend:
    case art::ThreadState::kWaitingForDebuggerToAttach:
    case art::ThreadState::kWaitingInMainDebuggerLoop:
    case art::ThreadState::kWaitingForDebuggerSuspension:
    case art::ThreadState::kWaitingForLockInflation:
    case art::ThreadState::kWaitingForTaskProcessor:
    case art::ThreadState::kWaitingForGcToComplete:
    case art::ThreadState::kWaitingForCheckPointsToRun:
    case art::ThreadState::kWaitingPerformingGc:
    case art::ThreadState::kWaitingForJniOnLoad:
    case art::ThreadState::kWaitingInMainSignalCatcherLoop:
    case art::ThreadState::kWaitingForSignalCatcherOutput:
    case art::ThreadState::kWaitingForDeoptimization:
    case art::ThreadState::kWaitingForMethodTracingStart:
    case art::ThreadState::kWaitingForVisitObjects:
    case art::ThreadState::kWaitingForGetObjectsAllocated:
    case art::ThreadState::kWaitingForGcThreadFlip:
      // All of these are causing the thread to wait for an indeterminate amount of time but isn't
      // caused by sleep, park, or object#wait.
      jvmti_state |= (JVMTI_THREAD_STATE_WAITING |
                      JVMTI_THREAD_STATE_WAITING_INDEFINITELY);
      break;
    case art::ThreadState::kStarting:
    case art::ThreadState::kTerminated:
      // We only call this if we are alive so we shouldn't see either of these states.
      LOG(FATAL) << "Should not be in state " << internal_thread_state;
      UNREACHABLE();
  }
  // TODO: PARKED. We'll have to inspect the stack.

  return jvmti_state;
}

static jint GetJavaStateFromInternal(const InternalThreadState& state) {
  switch (state.art_state) {
    case art::ThreadState::kTerminated:
      return JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED;

    case art::ThreadState::kRunnable:
    case art::ThreadState::kNative:
    case art::ThreadState::kWaitingWeakGcRootRead:
    case art::ThreadState::kSuspended:
      return JVMTI_JAVA_LANG_THREAD_STATE_RUNNABLE;

    case art::ThreadState::kTimedWaiting:
    case art::ThreadState::kSleeping:
      return JVMTI_JAVA_LANG_THREAD_STATE_TIMED_WAITING;

    case art::ThreadState::kBlocked:
      return JVMTI_JAVA_LANG_THREAD_STATE_BLOCKED;

    case art::ThreadState::kStarting:
      return JVMTI_JAVA_LANG_THREAD_STATE_NEW;

    case art::ThreadState::kWaiting:
    case art::ThreadState::kWaitingForTaskProcessor:
    case art::ThreadState::kWaitingForLockInflation:
    case art::ThreadState::kWaitingForGcToComplete:
    case art::ThreadState::kWaitingPerformingGc:
    case art::ThreadState::kWaitingForCheckPointsToRun:
    case art::ThreadState::kWaitingForDebuggerSend:
    case art::ThreadState::kWaitingForDebuggerToAttach:
    case art::ThreadState::kWaitingInMainDebuggerLoop:
    case art::ThreadState::kWaitingForDebuggerSuspension:
    case art::ThreadState::kWaitingForDeoptimization:
    case art::ThreadState::kWaitingForGetObjectsAllocated:
    case art::ThreadState::kWaitingForJniOnLoad:
    case art::ThreadState::kWaitingForSignalCatcherOutput:
    case art::ThreadState::kWaitingInMainSignalCatcherLoop:
    case art::ThreadState::kWaitingForMethodTracingStart:
    case art::ThreadState::kWaitingForVisitObjects:
    case art::ThreadState::kWaitingForGcThreadFlip:
      return JVMTI_JAVA_LANG_THREAD_STATE_WAITING;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

// Suspends the current thread if it has any suspend requests on it.
void ThreadUtil::SuspendCheck(art::Thread* self) {
  art::ScopedObjectAccess soa(self);
  // Really this is only needed if we are in FastJNI and actually have the mutator_lock_ already.
  self->FullSuspendCheck();
}

bool ThreadUtil::WouldSuspendForUserCodeLocked(art::Thread* self) {
  DCHECK(self == art::Thread::Current());
  art::MutexLock tscl_mu(self, *art::Locks::thread_suspend_count_lock_);
  return self->GetUserCodeSuspendCount() != 0;
}

bool ThreadUtil::WouldSuspendForUserCode(art::Thread* self) {
  DCHECK(self == art::Thread::Current());
  art::MutexLock ucsl_mu(self, *art::Locks::user_code_suspension_lock_);
  return WouldSuspendForUserCodeLocked(self);
}

jvmtiError ThreadUtil::GetThreadState(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                      jthread thread,
                                      jint* thread_state_ptr) {
  if (thread_state_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::Thread* self = art::Thread::Current();
  InternalThreadState state = {};
  // Loop since we need to bail out and try again if we would end up getting suspended while holding
  // the user_code_suspension_lock_ due to a SuspendReason::kForUserCode. In this situation we
  // release the lock, wait to get resumed and try again.
  do {
    SuspendCheck(self);
    art::MutexLock ucsl_mu(self, *art::Locks::user_code_suspension_lock_);
    if (WouldSuspendForUserCodeLocked(self)) {
      // Make sure we won't be suspended in the middle of holding the thread_suspend_count_lock_ by
      // a user-code suspension. We retry and do another SuspendCheck to clear this.
      continue;
    }
    art::ScopedObjectAccess soa(self);
    art::MutexLock tll_mu(self, *art::Locks::thread_list_lock_);
    jvmtiError err = ERR(INTERNAL);
    art::Thread* target = nullptr;
    if (!GetNativeThread(thread, soa, &target, &err)) {
      return err;
    }
    state = GetNativeThreadState(target);
    if (state.art_state == art::ThreadState::kStarting) {
      break;
    }
    DCHECK(state.native_thread != nullptr);

    // Translate internal thread state to JVMTI and Java state.
    jint jvmti_state = GetJvmtiThreadStateFromInternal(state);

    // Java state is derived from nativeGetState.
    // TODO: Our implementation assigns "runnable" to suspended. As such, we will have slightly
    //       different mask if a thread got suspended due to user-code. However, this is for
    //       consistency with the Java view.
    jint java_state = GetJavaStateFromInternal(state);

    *thread_state_ptr = jvmti_state | java_state;

    return ERR(NONE);
  } while (true);

  DCHECK_EQ(state.art_state, art::ThreadState::kStarting);

  if (thread == nullptr) {
    // No native thread, and no Java thread? We must be starting up. Report as wrong phase.
    return ERR(WRONG_PHASE);
  }

  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);

  // Need to read the Java "started" field to know whether this is starting or terminated.
  art::Handle<art::mirror::Object> peer(hs.NewHandle(soa.Decode<art::mirror::Object>(thread)));
  art::ObjPtr<art::mirror::Class> thread_klass =
      soa.Decode<art::mirror::Class>(art::WellKnownClasses::java_lang_Thread);
  if (!thread_klass->IsAssignableFrom(peer->GetClass())) {
    return ERR(INVALID_THREAD);
  }
  art::ArtField* started_field = thread_klass->FindDeclaredInstanceField("started", "Z");
  CHECK(started_field != nullptr);
  bool started = started_field->GetBoolean(peer.Get()) != 0;
  constexpr jint kStartedState = JVMTI_JAVA_LANG_THREAD_STATE_NEW;
  constexpr jint kTerminatedState = JVMTI_THREAD_STATE_TERMINATED |
                                    JVMTI_JAVA_LANG_THREAD_STATE_TERMINATED;
  *thread_state_ptr = started ? kTerminatedState : kStartedState;
  return ERR(NONE);
}

jvmtiError ThreadUtil::GetAllThreads(jvmtiEnv* env,
                                     jint* threads_count_ptr,
                                     jthread** threads_ptr) {
  if (threads_count_ptr == nullptr || threads_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::Thread* current = art::Thread::Current();

  art::ScopedObjectAccess soa(current);

  art::MutexLock mu(current, *art::Locks::thread_list_lock_);
  std::list<art::Thread*> thread_list = art::Runtime::Current()->GetThreadList()->GetList();

  std::vector<art::ObjPtr<art::mirror::Object>> peers;

  for (art::Thread* thread : thread_list) {
    // Skip threads that are still starting.
    if (thread->IsStillStarting()) {
      continue;
    }

    art::ObjPtr<art::mirror::Object> peer = thread->GetPeerFromOtherThread();
    if (peer != nullptr) {
      peers.push_back(peer);
    }
  }

  if (peers.empty()) {
    *threads_count_ptr = 0;
    *threads_ptr = nullptr;
  } else {
    unsigned char* data;
    jvmtiError data_result = env->Allocate(peers.size() * sizeof(jthread), &data);
    if (data_result != ERR(NONE)) {
      return data_result;
    }
    jthread* threads = reinterpret_cast<jthread*>(data);
    for (size_t i = 0; i != peers.size(); ++i) {
      threads[i] = soa.AddLocalReference<jthread>(peers[i]);
    }

    *threads_count_ptr = static_cast<jint>(peers.size());
    *threads_ptr = threads;
  }
  return ERR(NONE);
}

// The struct that we store in the art::Thread::custom_tls_ that maps the jvmtiEnvs to the data
// stored with that thread. This is needed since different jvmtiEnvs are not supposed to share TLS
// data but we only have a single slot in Thread objects to store data.
struct JvmtiGlobalTLSData {
  std::unordered_map<jvmtiEnv*, const void*> data GUARDED_BY(art::Locks::thread_list_lock_);
};

static void RemoveTLSData(art::Thread* target, void* ctx) REQUIRES(art::Locks::thread_list_lock_) {
  jvmtiEnv* env = reinterpret_cast<jvmtiEnv*>(ctx);
  art::Locks::thread_list_lock_->AssertHeld(art::Thread::Current());
  JvmtiGlobalTLSData* global_tls = reinterpret_cast<JvmtiGlobalTLSData*>(target->GetCustomTLS());
  if (global_tls != nullptr) {
    global_tls->data.erase(env);
  }
}

void ThreadUtil::RemoveEnvironment(jvmtiEnv* env) {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, *art::Locks::thread_list_lock_);
  art::ThreadList* list = art::Runtime::Current()->GetThreadList();
  list->ForEach(RemoveTLSData, env);
}

jvmtiError ThreadUtil::SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::MutexLock mu(self, *art::Locks::thread_list_lock_);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!GetAliveNativeThread(thread, soa, &target, &err)) {
    return err;
  }

  JvmtiGlobalTLSData* global_tls = reinterpret_cast<JvmtiGlobalTLSData*>(target->GetCustomTLS());
  if (global_tls == nullptr) {
    target->SetCustomTLS(new JvmtiGlobalTLSData);
    global_tls = reinterpret_cast<JvmtiGlobalTLSData*>(target->GetCustomTLS());
  }

  global_tls->data[env] = data;

  return ERR(NONE);
}

jvmtiError ThreadUtil::GetThreadLocalStorage(jvmtiEnv* env,
                                             jthread thread,
                                             void** data_ptr) {
  if (data_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::MutexLock mu(self, *art::Locks::thread_list_lock_);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!GetAliveNativeThread(thread, soa, &target, &err)) {
    return err;
  }

  JvmtiGlobalTLSData* global_tls = reinterpret_cast<JvmtiGlobalTLSData*>(target->GetCustomTLS());
  if (global_tls == nullptr) {
    *data_ptr = nullptr;
    return OK;
  }
  auto it = global_tls->data.find(env);
  if (it != global_tls->data.end()) {
    *data_ptr = const_cast<void*>(it->second);
  } else {
    *data_ptr = nullptr;
  }

  return ERR(NONE);
}

struct AgentData {
  const void* arg;
  jvmtiStartFunction proc;
  jthread thread;
  JavaVM* java_vm;
  jvmtiEnv* jvmti_env;
  jint priority;
  std::string name;
};

static void* AgentCallback(void* arg) {
  std::unique_ptr<AgentData> data(reinterpret_cast<AgentData*>(arg));
  CHECK(data->thread != nullptr);

  // We already have a peer. So call our special Attach function.
  art::Thread* self = art::Thread::Attach(data->name.c_str(), true, data->thread);
  CHECK(self != nullptr) << "threads_being_born_ should have ensured thread could be attached.";
  // The name in Attach() is only for logging. Set the thread name. This is important so
  // that the thread is no longer seen as starting up.
  {
    art::ScopedObjectAccess soa(self);
    self->SetThreadName(data->name.c_str());
  }

  // Release the peer.
  JNIEnv* env = self->GetJniEnv();
  env->DeleteGlobalRef(data->thread);
  data->thread = nullptr;

  {
    // The StartThreadBirth was called in the parent thread. We let the runtime know we are up
    // before going into the provided code.
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    art::Runtime::Current()->EndThreadBirth();
  }

  // Run the agent code.
  data->proc(data->jvmti_env, env, const_cast<void*>(data->arg));

  // Detach the thread.
  int detach_result = data->java_vm->DetachCurrentThread();
  CHECK_EQ(detach_result, 0);

  return nullptr;
}

jvmtiError ThreadUtil::RunAgentThread(jvmtiEnv* jvmti_env,
                                      jthread thread,
                                      jvmtiStartFunction proc,
                                      const void* arg,
                                      jint priority) {
  if (!PhaseUtil::IsLivePhase()) {
    return ERR(WRONG_PHASE);
  }
  if (priority < JVMTI_THREAD_MIN_PRIORITY || priority > JVMTI_THREAD_MAX_PRIORITY) {
    return ERR(INVALID_PRIORITY);
  }
  JNIEnv* env = art::Thread::Current()->GetJniEnv();
  if (thread == nullptr || !env->IsInstanceOf(thread, art::WellKnownClasses::java_lang_Thread)) {
    return ERR(INVALID_THREAD);
  }
  if (proc == nullptr) {
    return ERR(NULL_POINTER);
  }

  {
    art::Runtime* runtime = art::Runtime::Current();
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    if (runtime->IsShuttingDownLocked()) {
      // The runtime is shutting down so we cannot create new threads.
      // TODO It's not fully clear from the spec what we should do here. We aren't yet in
      // JVMTI_PHASE_DEAD so we cannot return ERR(WRONG_PHASE) but creating new threads is now
      // impossible. Existing agents don't seem to generally do anything with this return value so
      // it doesn't matter too much. We could do something like sending a fake ThreadStart event
      // even though code is never actually run.
      return ERR(INTERNAL);
    }
    runtime->StartThreadBirth();
  }

  std::unique_ptr<AgentData> data(new AgentData);
  data->arg = arg;
  data->proc = proc;
  // We need a global ref for Java objects, as local refs will be invalid.
  data->thread = env->NewGlobalRef(thread);
  data->java_vm = art::Runtime::Current()->GetJavaVM();
  data->jvmti_env = jvmti_env;
  data->priority = priority;
  ScopedLocalRef<jstring> s(
      env,
      reinterpret_cast<jstring>(
          env->GetObjectField(thread, art::WellKnownClasses::java_lang_Thread_name)));
  if (s == nullptr) {
    data->name = "JVMTI Agent Thread";
  } else {
    ScopedUtfChars name(env, s.get());
    data->name = name.c_str();
  }

  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread,
                                            nullptr,
                                            &AgentCallback,
                                            reinterpret_cast<void*>(data.get()));
  if (pthread_create_result != 0) {
    // If the create succeeded the other thread will call EndThreadBirth.
    art::Runtime* runtime = art::Runtime::Current();
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    runtime->EndThreadBirth();
    return ERR(INTERNAL);
  }
  data.release();

  return ERR(NONE);
}

jvmtiError ThreadUtil::SuspendOther(art::Thread* self,
                                    jthread target_jthread) {
  // Loop since we need to bail out and try again if we would end up getting suspended while holding
  // the user_code_suspension_lock_ due to a SuspendReason::kForUserCode. In this situation we
  // release the lock, wait to get resumed and try again.
  do {
    // Suspend ourself if we have any outstanding suspends. This is so we won't suspend due to
    // another SuspendThread in the middle of suspending something else potentially causing a
    // deadlock. We need to do this in the loop because if we ended up back here then we had
    // outstanding SuspendReason::kForUserCode suspensions and we should wait for them to be cleared
    // before continuing.
    SuspendCheck(self);
    art::MutexLock mu(self, *art::Locks::user_code_suspension_lock_);
    if (WouldSuspendForUserCodeLocked(self)) {
      // Make sure we won't be suspended in the middle of holding the thread_suspend_count_lock_ by
      // a user-code suspension. We retry and do another SuspendCheck to clear this.
      continue;
    }
    // We are not going to be suspended by user code from now on.
    {
      art::ScopedObjectAccess soa(self);
      art::MutexLock thread_list_mu(self, *art::Locks::thread_list_lock_);
      art::Thread* target = nullptr;
      jvmtiError err = ERR(INTERNAL);
      if (!GetAliveNativeThread(target_jthread, soa, &target, &err)) {
        return err;
      }
      art::ThreadState state = target->GetState();
      if (state == art::ThreadState::kStarting || target->IsStillStarting()) {
        return ERR(THREAD_NOT_ALIVE);
      } else {
        art::MutexLock thread_suspend_count_mu(self, *art::Locks::thread_suspend_count_lock_);
        if (target->GetUserCodeSuspendCount() != 0) {
          return ERR(THREAD_SUSPENDED);
        }
      }
    }
    bool timeout = true;
    art::Thread* ret_target = art::Runtime::Current()->GetThreadList()->SuspendThreadByPeer(
        target_jthread,
        /* request_suspension */ true,
        art::SuspendReason::kForUserCode,
        &timeout);
    if (ret_target == nullptr && !timeout) {
      // TODO It would be good to get more information about why exactly the thread failed to
      // suspend.
      return ERR(INTERNAL);
    } else if (!timeout) {
      // we didn't time out and got a result.
      return OK;
    }
    // We timed out. Just go around and try again.
  } while (true);
  UNREACHABLE();
}

jvmtiError ThreadUtil::SuspendSelf(art::Thread* self) {
  CHECK(self == art::Thread::Current());
  {
    art::MutexLock mu(self, *art::Locks::user_code_suspension_lock_);
    art::MutexLock thread_list_mu(self, *art::Locks::thread_suspend_count_lock_);
    if (self->GetUserCodeSuspendCount() != 0) {
      // This can only happen if we race with another thread to suspend 'self' and we lose.
      return ERR(THREAD_SUSPENDED);
    }
    // We shouldn't be able to fail this.
    if (!self->ModifySuspendCount(self, +1, nullptr, art::SuspendReason::kForUserCode)) {
      // TODO More specific error would be nice.
      return ERR(INTERNAL);
    }
  }
  // Once we have requested the suspend we actually go to sleep. We need to do this after releasing
  // the suspend_lock to make sure we can be woken up. This call gains the mutator lock causing us
  // to go to sleep until we are resumed.
  SuspendCheck(self);
  return OK;
}

jvmtiError ThreadUtil::SuspendThread(jvmtiEnv* env ATTRIBUTE_UNUSED, jthread thread) {
  art::Thread* self = art::Thread::Current();
  bool target_is_self = false;
  {
    art::ScopedObjectAccess soa(self);
    art::MutexLock mu(self, *art::Locks::thread_list_lock_);
    art::Thread* target = nullptr;
    jvmtiError err = ERR(INTERNAL);
    if (!GetAliveNativeThread(thread, soa, &target, &err)) {
      return err;
    } else if (target == self) {
      target_is_self = true;
    }
  }
  if (target_is_self) {
    return SuspendSelf(self);
  } else {
    return SuspendOther(self, thread);
  }
}

jvmtiError ThreadUtil::ResumeThread(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                    jthread thread) {
  if (thread == nullptr) {
    return ERR(NULL_POINTER);
  }
  art::Thread* self = art::Thread::Current();
  art::Thread* target;
  // Retry until we know we won't get suspended by user code while resuming something.
  do {
    SuspendCheck(self);
    art::MutexLock ucsl_mu(self, *art::Locks::user_code_suspension_lock_);
    if (WouldSuspendForUserCodeLocked(self)) {
      // Make sure we won't be suspended in the middle of holding the thread_suspend_count_lock_ by
      // a user-code suspension. We retry and do another SuspendCheck to clear this.
      continue;
    }
    // From now on we know we cannot get suspended by user-code.
    {
      // NB This does a SuspendCheck (during thread state change) so we need to make sure we don't
      // have the 'suspend_lock' locked here.
      art::ScopedObjectAccess soa(self);
      art::MutexLock tll_mu(self, *art::Locks::thread_list_lock_);
      jvmtiError err = ERR(INTERNAL);
      if (!GetAliveNativeThread(thread, soa, &target, &err)) {
        return err;
      } else if (target == self) {
        // We would have paused until we aren't suspended anymore due to the ScopedObjectAccess so
        // we can just return THREAD_NOT_SUSPENDED. Unfortunately we cannot do any real DCHECKs
        // about current state since it's all concurrent.
        return ERR(THREAD_NOT_SUSPENDED);
      }
      // The JVMTI spec requires us to return THREAD_NOT_SUSPENDED if it is alive but we really
      // cannot tell why resume failed.
      {
        art::MutexLock thread_suspend_count_mu(self, *art::Locks::thread_suspend_count_lock_);
        if (target->GetUserCodeSuspendCount() == 0) {
          return ERR(THREAD_NOT_SUSPENDED);
        }
      }
    }
    // It is okay that we don't have a thread_list_lock here since we know that the thread cannot
    // die since it is currently held suspended by a SuspendReason::kForUserCode suspend.
    DCHECK(target != self);
    if (!art::Runtime::Current()->GetThreadList()->Resume(target,
                                                          art::SuspendReason::kForUserCode)) {
      // TODO Give a better error.
      // This is most likely THREAD_NOT_SUSPENDED but we cannot really be sure.
      return ERR(INTERNAL);
    } else {
      return OK;
    }
  } while (true);
}

static bool IsCurrentThread(jthread thr) {
  if (thr == nullptr) {
    return true;
  }
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::MutexLock mu(self, *art::Locks::thread_list_lock_);
  art::Thread* target = nullptr;
  jvmtiError err_unused = ERR(INTERNAL);
  if (ThreadUtil::GetNativeThread(thr, soa, &target, &err_unused)) {
    return target == self;
  } else {
    return false;
  }
}

// Suspends all the threads in the list at the same time. Getting this behavior is a little tricky
// since we can have threads in the list multiple times. This generally doesn't matter unless the
// current thread is present multiple times. In that case we need to suspend only once and either
// return the same error code in all the other slots if it failed or return ERR(THREAD_SUSPENDED) if
// it didn't. We also want to handle the current thread last to make the behavior of the code
// simpler to understand.
jvmtiError ThreadUtil::SuspendThreadList(jvmtiEnv* env,
                                         jint request_count,
                                         const jthread* threads,
                                         jvmtiError* results) {
  if (request_count == 0) {
    return ERR(ILLEGAL_ARGUMENT);
  } else if (results == nullptr || threads == nullptr) {
    return ERR(NULL_POINTER);
  }
  // This is the list of the indexes in 'threads' and 'results' that correspond to the currently
  // running thread. These indexes we need to handle specially since we need to only actually
  // suspend a single time.
  std::vector<jint> current_thread_indexes;
  for (jint i = 0; i < request_count; i++) {
    if (IsCurrentThread(threads[i])) {
      current_thread_indexes.push_back(i);
    } else {
      results[i] = env->SuspendThread(threads[i]);
    }
  }
  if (!current_thread_indexes.empty()) {
    jint first_current_thread_index = current_thread_indexes[0];
    // Suspend self.
    jvmtiError res = env->SuspendThread(threads[first_current_thread_index]);
    results[first_current_thread_index] = res;
    // Fill in the rest of the error values as appropriate.
    jvmtiError other_results = (res != OK) ? res : ERR(THREAD_SUSPENDED);
    for (auto it = ++current_thread_indexes.begin(); it != current_thread_indexes.end(); ++it) {
      results[*it] = other_results;
    }
  }
  return OK;
}

jvmtiError ThreadUtil::ResumeThreadList(jvmtiEnv* env,
                                        jint request_count,
                                        const jthread* threads,
                                        jvmtiError* results) {
  if (request_count == 0) {
    return ERR(ILLEGAL_ARGUMENT);
  } else if (results == nullptr || threads == nullptr) {
    return ERR(NULL_POINTER);
  }
  for (jint i = 0; i < request_count; i++) {
    results[i] = env->ResumeThread(threads[i]);
  }
  return OK;
}

jvmtiError ThreadUtil::StopThread(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                  jthread thread,
                                  jobject exception) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::StackHandleScope<1> hs(self);
  if (exception == nullptr) {
    return ERR(INVALID_OBJECT);
  }
  art::ObjPtr<art::mirror::Object> obj(soa.Decode<art::mirror::Object>(exception));
  if (!obj->GetClass()->IsThrowableClass()) {
    return ERR(INVALID_OBJECT);
  }
  art::Handle<art::mirror::Throwable> exc(hs.NewHandle(obj->AsThrowable()));
  art::Locks::thread_list_lock_->ExclusiveLock(self);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!GetAliveNativeThread(thread, soa, &target, &err)) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return err;
  } else if (target->GetState() == art::ThreadState::kStarting || target->IsStillStarting()) {
    art::Locks::thread_list_lock_->ExclusiveUnlock(self);
    return ERR(THREAD_NOT_ALIVE);
  }
  struct StopThreadClosure : public art::Closure {
   public:
    explicit StopThreadClosure(art::Handle<art::mirror::Throwable> except) : exception_(except) { }

    void Run(art::Thread* me) REQUIRES_SHARED(art::Locks::mutator_lock_) {
      // Make sure the thread is prepared to notice the exception.
      art::Runtime::Current()->GetInstrumentation()->InstrumentThreadStack(me);
      me->SetAsyncException(exception_.Get());
      // Wake up the thread if it is sleeping.
      me->Notify();
    }

   private:
    art::Handle<art::mirror::Throwable> exception_;
  };
  StopThreadClosure c(exc);
  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its execution.
  if (target->RequestSynchronousCheckpoint(&c)) {
    return OK;
  } else {
    // Something went wrong, probably the thread died.
    return ERR(THREAD_NOT_ALIVE);
  }
}

jvmtiError ThreadUtil::InterruptThread(jvmtiEnv* env ATTRIBUTE_UNUSED, jthread thread) {
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  art::MutexLock tll_mu(self, *art::Locks::thread_list_lock_);
  art::Thread* target = nullptr;
  jvmtiError err = ERR(INTERNAL);
  if (!GetAliveNativeThread(thread, soa, &target, &err)) {
    return err;
  } else if (target->GetState() == art::ThreadState::kStarting || target->IsStillStarting()) {
    return ERR(THREAD_NOT_ALIVE);
  }
  target->Interrupt(self);
  return OK;
}

}  // namespace openjdkjvmti
