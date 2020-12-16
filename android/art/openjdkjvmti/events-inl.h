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

#ifndef ART_OPENJDKJVMTI_EVENTS_INL_H_
#define ART_OPENJDKJVMTI_EVENTS_INL_H_

#include <array>
#include <type_traits>
#include <tuple>

#include "base/mutex-inl.h"
#include "events.h"
#include "jni_internal.h"
#include "nativehelper/scoped_local_ref.h"
#include "scoped_thread_state_change-inl.h"
#include "ti_breakpoint.h"

#include "art_jvmti.h"

namespace openjdkjvmti {

static inline ArtJvmtiEvent GetArtJvmtiEvent(ArtJvmTiEnv* env, jvmtiEvent e) {
  if (UNLIKELY(e == JVMTI_EVENT_CLASS_FILE_LOAD_HOOK)) {
    if (env->capabilities.can_retransform_classes) {
      return ArtJvmtiEvent::kClassFileLoadHookRetransformable;
    } else {
      return ArtJvmtiEvent::kClassFileLoadHookNonRetransformable;
    }
  } else {
    return static_cast<ArtJvmtiEvent>(e);
  }
}

namespace impl {

// Helper for ensuring that the dispatch environment is sane. Events with JNIEnvs need to stash
// pending exceptions since they can cause new ones to be thrown. In accordance with the JVMTI
// specification we allow exceptions originating from events to overwrite the current exception,
// including exceptions originating from earlier events.
class ScopedEventDispatchEnvironment FINAL : public art::ValueObject {
 public:
  ScopedEventDispatchEnvironment() : env_(nullptr), throw_(nullptr, nullptr) {
    DCHECK_EQ(art::Thread::Current()->GetState(), art::ThreadState::kNative);
  }

  explicit ScopedEventDispatchEnvironment(JNIEnv* env)
      : env_(env),
        throw_(env_, env_->ExceptionOccurred()) {
    DCHECK_EQ(art::Thread::Current()->GetState(), art::ThreadState::kNative);
    // The spec doesn't say how much local data should be there, so we just give 128 which seems
    // likely to be enough for most cases.
    env_->PushLocalFrame(128);
    env_->ExceptionClear();
  }

  ~ScopedEventDispatchEnvironment() {
    if (env_ != nullptr) {
      if (throw_.get() != nullptr && !env_->ExceptionCheck()) {
        // TODO It would be nice to add the overwritten exceptions to the suppressed exceptions list
        // of the newest exception.
        env_->Throw(throw_.get());
      }
      env_->PopLocalFrame(nullptr);
    }
    DCHECK_EQ(art::Thread::Current()->GetState(), art::ThreadState::kNative);
  }

 private:
  JNIEnv* env_;
  ScopedLocalRef<jthrowable> throw_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEventDispatchEnvironment);
};

// Infrastructure to achieve type safety for event dispatch.

#define FORALL_EVENT_TYPES(fn)                                                       \
  fn(VMInit,                  ArtJvmtiEvent::kVmInit)                                \
  fn(VMDeath,                 ArtJvmtiEvent::kVmDeath)                               \
  fn(ThreadStart,             ArtJvmtiEvent::kThreadStart)                           \
  fn(ThreadEnd,               ArtJvmtiEvent::kThreadEnd)                             \
  fn(ClassFileLoadHook,       ArtJvmtiEvent::kClassFileLoadHookRetransformable)      \
  fn(ClassFileLoadHook,       ArtJvmtiEvent::kClassFileLoadHookNonRetransformable)   \
  fn(ClassLoad,               ArtJvmtiEvent::kClassLoad)                             \
  fn(ClassPrepare,            ArtJvmtiEvent::kClassPrepare)                          \
  fn(VMStart,                 ArtJvmtiEvent::kVmStart)                               \
  fn(Exception,               ArtJvmtiEvent::kException)                             \
  fn(ExceptionCatch,          ArtJvmtiEvent::kExceptionCatch)                        \
  fn(SingleStep,              ArtJvmtiEvent::kSingleStep)                            \
  fn(FramePop,                ArtJvmtiEvent::kFramePop)                              \
  fn(Breakpoint,              ArtJvmtiEvent::kBreakpoint)                            \
  fn(FieldAccess,             ArtJvmtiEvent::kFieldAccess)                           \
  fn(FieldModification,       ArtJvmtiEvent::kFieldModification)                     \
  fn(MethodEntry,             ArtJvmtiEvent::kMethodEntry)                           \
  fn(MethodExit,              ArtJvmtiEvent::kMethodExit)                            \
  fn(NativeMethodBind,        ArtJvmtiEvent::kNativeMethodBind)                      \
  fn(CompiledMethodLoad,      ArtJvmtiEvent::kCompiledMethodLoad)                    \
  fn(CompiledMethodUnload,    ArtJvmtiEvent::kCompiledMethodUnload)                  \
  fn(DynamicCodeGenerated,    ArtJvmtiEvent::kDynamicCodeGenerated)                  \
  fn(DataDumpRequest,         ArtJvmtiEvent::kDataDumpRequest)                       \
  fn(MonitorWait,             ArtJvmtiEvent::kMonitorWait)                           \
  fn(MonitorWaited,           ArtJvmtiEvent::kMonitorWaited)                         \
  fn(MonitorContendedEnter,   ArtJvmtiEvent::kMonitorContendedEnter)                 \
  fn(MonitorContendedEntered, ArtJvmtiEvent::kMonitorContendedEntered)               \
  fn(ResourceExhausted,       ArtJvmtiEvent::kResourceExhausted)                     \
  fn(GarbageCollectionStart,  ArtJvmtiEvent::kGarbageCollectionStart)                \
  fn(GarbageCollectionFinish, ArtJvmtiEvent::kGarbageCollectionFinish)               \
  fn(ObjectFree,              ArtJvmtiEvent::kObjectFree)                            \
  fn(VMObjectAlloc,           ArtJvmtiEvent::kVmObjectAlloc)                         \
  fn(DdmPublishChunk,         ArtJvmtiEvent::kDdmPublishChunk)

template <ArtJvmtiEvent kEvent>
struct EventFnType {
};

#define EVENT_FN_TYPE(name, enum_name)                    \
template <>                                               \
struct EventFnType<enum_name> {                           \
  using type = decltype(ArtJvmtiEventCallbacks().name);   \
};

FORALL_EVENT_TYPES(EVENT_FN_TYPE)

#undef EVENT_FN_TYPE

#define MAKE_EVENT_HANDLER_FUNC(name, enum_name)                                          \
template<>                                                                                \
struct EventHandlerFunc<enum_name> {                                                      \
  using EventFnType = typename impl::EventFnType<enum_name>::type;                        \
  explicit EventHandlerFunc(ArtJvmTiEnv* env)                                             \
      : env_(env),                                                                        \
        fn_(env_->event_callbacks == nullptr ? nullptr : env_->event_callbacks->name) { } \
                                                                                          \
  template <typename ...Args>                                                             \
  ALWAYS_INLINE                                                                           \
  void ExecuteCallback(JNIEnv* jnienv, Args... args) const {                              \
    if (fn_ != nullptr) {                                                                 \
      ScopedEventDispatchEnvironment sede(jnienv);                                        \
      DoExecute(jnienv, args...);                                                         \
    }                                                                                     \
  }                                                                                       \
                                                                                          \
  template <typename ...Args>                                                             \
  ALWAYS_INLINE                                                                           \
  void ExecuteCallback(Args... args) const {                                              \
    if (fn_ != nullptr) {                                                                 \
      ScopedEventDispatchEnvironment sede;                                                \
      DoExecute(args...);                                                                 \
    }                                                                                     \
  }                                                                                       \
                                                                                          \
 private:                                                                                 \
  template <typename ...Args>                                                             \
  ALWAYS_INLINE                                                                           \
  inline void DoExecute(Args... args) const {                                             \
    static_assert(std::is_same<EventFnType, void(*)(jvmtiEnv*, Args...)>::value,          \
          "Unexpected different type of ExecuteCallback");                                \
    fn_(env_, args...);                                                                   \
  }                                                                                       \
                                                                                          \
 public:                                                                                  \
  ArtJvmTiEnv* env_;                                                                      \
  EventFnType fn_;                                                                        \
};

FORALL_EVENT_TYPES(MAKE_EVENT_HANDLER_FUNC)

#undef MAKE_EVENT_HANDLER_FUNC

#undef FORALL_EVENT_TYPES

}  // namespace impl

template <ArtJvmtiEvent kEvent, typename ...Args>
inline std::vector<impl::EventHandlerFunc<kEvent>> EventHandler::CollectEvents(art::Thread* thread,
                                                                               Args... args) const {
  art::ReaderMutexLock mu(thread, envs_lock_);
  std::vector<impl::EventHandlerFunc<kEvent>> handlers;
  for (ArtJvmTiEnv* env : envs) {
    if (ShouldDispatch<kEvent>(env, thread, args...)) {
      impl::EventHandlerFunc<kEvent> h(env);
      handlers.push_back(h);
    }
  }
  return handlers;
}

// C++ does not allow partial template function specialization. The dispatch for our separated
// ClassFileLoadHook event types is the same, so use this helper for code deduplication.
template <ArtJvmtiEvent kEvent>
inline void EventHandler::DispatchClassFileLoadHookEvent(art::Thread* thread,
                                                         JNIEnv* jnienv,
                                                         jclass class_being_redefined,
                                                         jobject loader,
                                                         const char* name,
                                                         jobject protection_domain,
                                                         jint class_data_len,
                                                         const unsigned char* class_data,
                                                         jint* new_class_data_len,
                                                         unsigned char** new_class_data) const {
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  static_assert(kEvent == ArtJvmtiEvent::kClassFileLoadHookRetransformable ||
                kEvent == ArtJvmtiEvent::kClassFileLoadHookNonRetransformable, "Unsupported event");
  DCHECK(*new_class_data == nullptr);
  jint current_len = class_data_len;
  unsigned char* current_class_data = const_cast<unsigned char*>(class_data);
  std::vector<impl::EventHandlerFunc<kEvent>> handlers =
      CollectEvents<kEvent>(thread,
                            jnienv,
                            class_being_redefined,
                            loader,
                            name,
                            protection_domain,
                            class_data_len,
                            class_data,
                            new_class_data_len,
                            new_class_data);
  ArtJvmTiEnv* last_env = nullptr;
  for (const impl::EventHandlerFunc<kEvent>& event : handlers) {
    jint new_len = 0;
    unsigned char* new_data = nullptr;
    ExecuteCallback<kEvent>(event,
                            jnienv,
                            class_being_redefined,
                            loader,
                            name,
                            protection_domain,
                            current_len,
                            static_cast<const unsigned char*>(current_class_data),
                            &new_len,
                            &new_data);
    if (new_data != nullptr && new_data != current_class_data) {
      // Destroy the data the last transformer made. We skip this if the previous state was the
      // initial one since we don't know here which jvmtiEnv allocated it.
      // NB Currently this doesn't matter since all allocations just go to malloc but in the
      // future we might have jvmtiEnv's keep track of their allocations for leak-checking.
      if (last_env != nullptr) {
        last_env->Deallocate(current_class_data);
      }
      last_env = event.env_;
      current_class_data = new_data;
      current_len = new_len;
    }
  }
  if (last_env != nullptr) {
    *new_class_data_len = current_len;
    *new_class_data = current_class_data;
  }
}

// Our goal for DispatchEvent: Do not allow implicit type conversion. Types of ...args must match
// exactly the argument types of the corresponding Jvmti kEvent function pointer.

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEvent(art::Thread* thread, Args... args) const {
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  static_assert(!std::is_same<JNIEnv*,
                              typename std::decay_t<
                                  std::tuple_element_t<0, std::tuple<Args..., nullptr_t>>>>::value,
                "Should be calling DispatchEvent with explicit JNIEnv* argument!");
  DCHECK(thread == nullptr || !thread->IsExceptionPending());
  std::vector<impl::EventHandlerFunc<kEvent>> events = CollectEvents<kEvent>(thread, args...);
  for (auto event : events) {
    ExecuteCallback<kEvent>(event, args...);
  }
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEvent(art::Thread* thread, JNIEnv* jnienv, Args... args) const {
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  std::vector<impl::EventHandlerFunc<kEvent>> events = CollectEvents<kEvent>(thread,
                                                                             jnienv,
                                                                             args...);
  for (auto event : events) {
    ExecuteCallback<kEvent>(event, jnienv, args...);
  }
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEventOnEnv(
    ArtJvmTiEnv* env, art::Thread* thread, JNIEnv* jnienv, Args... args) const {
  DCHECK(env != nullptr);
  if (ShouldDispatch<kEvent, JNIEnv*, Args...>(env, thread, jnienv, args...)) {
    art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
    impl::EventHandlerFunc<kEvent> func(env);
    ExecuteCallback<kEvent>(func, jnienv, args...);
  }
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEventOnEnv(
    ArtJvmTiEnv* env, art::Thread* thread, Args... args) const {
  static_assert(!std::is_same<JNIEnv*,
                              typename std::decay_t<
                                  std::tuple_element_t<0, std::tuple<Args..., nullptr_t>>>>::value,
                "Should be calling DispatchEventOnEnv with explicit JNIEnv* argument!");
  DCHECK(env != nullptr);
  if (ShouldDispatch<kEvent, Args...>(env, thread, args...)) {
    art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
    impl::EventHandlerFunc<kEvent> func(env);
    ExecuteCallback<kEvent>(func, args...);
  }
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::ExecuteCallback(impl::EventHandlerFunc<kEvent> handler, Args... args) {
  handler.ExecuteCallback(args...);
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::ExecuteCallback(impl::EventHandlerFunc<kEvent> handler,
                                          JNIEnv* jnienv,
                                          Args... args) {
  handler.ExecuteCallback(jnienv, args...);
}

// Events that need custom logic for if we send the event but are otherwise normal. This includes
// the kBreakpoint, kFramePop, kFieldAccess, and kFieldModification events.

// Need to give custom specializations for Breakpoint since it needs to filter out which particular
// methods/dex_pcs agents get notified on.
template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kBreakpoint>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID jmethod,
    jlocation location) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  art::ArtMethod* method = art::jni::DecodeArtMethod(jmethod);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kBreakpoint>(env, thread) &&
      env->breakpoints.find({method, location}) != env->breakpoints.end();
}

template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFramePop>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID jmethod ATTRIBUTE_UNUSED,
    jboolean is_exception ATTRIBUTE_UNUSED,
    const art::ShadowFrame* frame) const {
  // Search for the frame. Do this before checking if we need to send the event so that we don't
  // have to deal with use-after-free or the frames being reallocated later.
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return env->notify_frames.erase(frame) != 0 &&
      ShouldDispatchOnThread<ArtJvmtiEvent::kFramePop>(env, thread);
}

// Need to give custom specializations for FieldAccess and FieldModification since they need to
// filter out which particular fields agents want to get notified on.
// TODO The spec allows us to do shortcuts like only allow one agent to ever set these watches. This
// could make the system more performant.
template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFieldModification>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID method ATTRIBUTE_UNUSED,
    jlocation location ATTRIBUTE_UNUSED,
    jclass field_klass ATTRIBUTE_UNUSED,
    jobject object ATTRIBUTE_UNUSED,
    jfieldID field,
    char type_char ATTRIBUTE_UNUSED,
    jvalue val ATTRIBUTE_UNUSED) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kFieldModification>(env, thread) &&
      env->modify_watched_fields.find(
          art::jni::DecodeArtField(field)) != env->modify_watched_fields.end();
}

template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFieldAccess>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID method ATTRIBUTE_UNUSED,
    jlocation location ATTRIBUTE_UNUSED,
    jclass field_klass ATTRIBUTE_UNUSED,
    jobject object ATTRIBUTE_UNUSED,
    jfieldID field) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kFieldAccess>(env, thread) &&
      env->access_watched_fields.find(
          art::jni::DecodeArtField(field)) != env->access_watched_fields.end();
}

// Need to give custom specializations for FramePop since it needs to filter out which particular
// agents get the event. This specialization gets an extra argument so we can determine which (if
// any) environments have the frame pop.
// TODO It might be useful to use more template magic to have this only define ShouldDispatch or
// something.
template <>
inline void EventHandler::ExecuteCallback<ArtJvmtiEvent::kFramePop>(
    impl::EventHandlerFunc<ArtJvmtiEvent::kFramePop> event,
    JNIEnv* jnienv,
    jthread jni_thread,
    jmethodID jmethod,
    jboolean is_exception,
    const art::ShadowFrame* frame ATTRIBUTE_UNUSED) {
  ExecuteCallback<ArtJvmtiEvent::kFramePop>(event, jnienv, jni_thread, jmethod, is_exception);
}

// Need to give a custom specialization for NativeMethodBind since it has to deal with an out
// variable.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kNativeMethodBind>(art::Thread* thread,
                                                                          JNIEnv* jnienv,
                                                                          jthread jni_thread,
                                                                          jmethodID method,
                                                                          void* cur_method,
                                                                          void** new_method) const {
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  std::vector<impl::EventHandlerFunc<ArtJvmtiEvent::kNativeMethodBind>> events =
      CollectEvents<ArtJvmtiEvent::kNativeMethodBind>(thread,
                                                      jnienv,
                                                      jni_thread,
                                                      method,
                                                      cur_method,
                                                      new_method);
  *new_method = cur_method;
  for (auto event : events) {
    *new_method = cur_method;
    ExecuteCallback<ArtJvmtiEvent::kNativeMethodBind>(event,
                                                      jnienv,
                                                      jni_thread,
                                                      method,
                                                      cur_method,
                                                      new_method);
    if (*new_method != nullptr) {
      cur_method = *new_method;
    }
  }
  *new_method = cur_method;
}

// C++ does not allow partial template function specialization. The dispatch for our separated
// ClassFileLoadHook event types is the same, and in the DispatchClassFileLoadHookEvent helper.
// The following two DispatchEvent specializations dispatch to it.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <ArtJvmtiEvent kEvent>
inline bool EventHandler::ShouldDispatchOnThread(ArtJvmTiEnv* env, art::Thread* thread) const {
  bool dispatch = env->event_masks.global_event_mask.Test(kEvent);

  if (!dispatch && thread != nullptr && env->event_masks.unioned_thread_event_mask.Test(kEvent)) {
    EventMask* mask = env->event_masks.GetEventMaskOrNull(thread);
    dispatch = mask != nullptr && mask->Test(kEvent);
  }
  return dispatch;
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline bool EventHandler::ShouldDispatch(ArtJvmTiEnv* env,
                                         art::Thread* thread,
                                         Args... args ATTRIBUTE_UNUSED) const {
  static_assert(std::is_same<typename impl::EventFnType<kEvent>::type,
                             void(*)(jvmtiEnv*, Args...)>::value,
                "Unexpected different type of shouldDispatch");

  return ShouldDispatchOnThread<kEvent>(env, thread);
}

inline void EventHandler::RecalculateGlobalEventMask(ArtJvmtiEvent event) {
  art::WriterMutexLock mu(art::Thread::Current(), envs_lock_);
  RecalculateGlobalEventMaskLocked(event);
}

inline void EventHandler::RecalculateGlobalEventMaskLocked(ArtJvmtiEvent event) {
  bool union_value = false;
  for (const ArtJvmTiEnv* stored_env : envs) {
    if (stored_env == nullptr) {
      continue;
    }
    union_value |= stored_env->event_masks.global_event_mask.Test(event);
    union_value |= stored_env->event_masks.unioned_thread_event_mask.Test(event);
    if (union_value) {
      break;
    }
  }
  global_mask.Set(event, union_value);
}

inline bool EventHandler::NeedsEventUpdate(ArtJvmTiEnv* env,
                                           const jvmtiCapabilities& caps,
                                           bool added) {
  ArtJvmtiEvent event = added ? ArtJvmtiEvent::kClassFileLoadHookNonRetransformable
                              : ArtJvmtiEvent::kClassFileLoadHookRetransformable;
  return (added && caps.can_access_local_variables == 1) ||
      caps.can_generate_breakpoint_events == 1 ||
      (caps.can_retransform_classes == 1 &&
       IsEventEnabledAnywhere(event) &&
       env->event_masks.IsEnabledAnywhere(event));
}

inline void EventHandler::HandleChangedCapabilities(ArtJvmTiEnv* env,
                                                    const jvmtiCapabilities& caps,
                                                    bool added) {
  if (UNLIKELY(NeedsEventUpdate(env, caps, added))) {
    env->event_masks.HandleChangedCapabilities(caps, added);
    if (caps.can_retransform_classes == 1) {
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookRetransformable);
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookNonRetransformable);
    }
    if (added && caps.can_access_local_variables == 1) {
      HandleLocalAccessCapabilityAdded();
    }
    if (caps.can_generate_breakpoint_events == 1) {
      HandleBreakpointEventsChanged(added);
    }
  }
}

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_EVENTS_INL_H_
