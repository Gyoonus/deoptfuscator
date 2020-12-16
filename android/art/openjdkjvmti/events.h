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

#ifndef ART_OPENJDKJVMTI_EVENTS_H_
#define ART_OPENJDKJVMTI_EVENTS_H_

#include <bitset>
#include <vector>

#include <android-base/logging.h>

#include "base/macros.h"
#include "base/mutex.h"
#include "jvmti.h"
#include "thread.h"

namespace openjdkjvmti {

struct ArtJvmTiEnv;
class JvmtiAllocationListener;
class JvmtiDdmChunkListener;
class JvmtiGcPauseListener;
class JvmtiMethodTraceListener;
class JvmtiMonitorListener;

// an enum for ArtEvents. This differs from the JVMTI events only in that we distinguish between
// retransformation capable and incapable loading
enum class ArtJvmtiEvent : jint {
    kMinEventTypeVal = JVMTI_MIN_EVENT_TYPE_VAL,
    kVmInit = JVMTI_EVENT_VM_INIT,
    kVmDeath = JVMTI_EVENT_VM_DEATH,
    kThreadStart = JVMTI_EVENT_THREAD_START,
    kThreadEnd = JVMTI_EVENT_THREAD_END,
    kClassFileLoadHookNonRetransformable = JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
    kClassLoad = JVMTI_EVENT_CLASS_LOAD,
    kClassPrepare = JVMTI_EVENT_CLASS_PREPARE,
    kVmStart = JVMTI_EVENT_VM_START,
    kException = JVMTI_EVENT_EXCEPTION,
    kExceptionCatch = JVMTI_EVENT_EXCEPTION_CATCH,
    kSingleStep = JVMTI_EVENT_SINGLE_STEP,
    kFramePop = JVMTI_EVENT_FRAME_POP,
    kBreakpoint = JVMTI_EVENT_BREAKPOINT,
    kFieldAccess = JVMTI_EVENT_FIELD_ACCESS,
    kFieldModification = JVMTI_EVENT_FIELD_MODIFICATION,
    kMethodEntry = JVMTI_EVENT_METHOD_ENTRY,
    kMethodExit = JVMTI_EVENT_METHOD_EXIT,
    kNativeMethodBind = JVMTI_EVENT_NATIVE_METHOD_BIND,
    kCompiledMethodLoad = JVMTI_EVENT_COMPILED_METHOD_LOAD,
    kCompiledMethodUnload = JVMTI_EVENT_COMPILED_METHOD_UNLOAD,
    kDynamicCodeGenerated = JVMTI_EVENT_DYNAMIC_CODE_GENERATED,
    kDataDumpRequest = JVMTI_EVENT_DATA_DUMP_REQUEST,
    kMonitorWait = JVMTI_EVENT_MONITOR_WAIT,
    kMonitorWaited = JVMTI_EVENT_MONITOR_WAITED,
    kMonitorContendedEnter = JVMTI_EVENT_MONITOR_CONTENDED_ENTER,
    kMonitorContendedEntered = JVMTI_EVENT_MONITOR_CONTENDED_ENTERED,
    kResourceExhausted = JVMTI_EVENT_RESOURCE_EXHAUSTED,
    kGarbageCollectionStart = JVMTI_EVENT_GARBAGE_COLLECTION_START,
    kGarbageCollectionFinish = JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
    kObjectFree = JVMTI_EVENT_OBJECT_FREE,
    kVmObjectAlloc = JVMTI_EVENT_VM_OBJECT_ALLOC,
    kClassFileLoadHookRetransformable = JVMTI_MAX_EVENT_TYPE_VAL + 1,
    kDdmPublishChunk = JVMTI_MAX_EVENT_TYPE_VAL + 2,
    kMaxEventTypeVal = kDdmPublishChunk,
};

using ArtJvmtiEventDdmPublishChunk = void (*)(jvmtiEnv *jvmti_env,
                                              JNIEnv* jni_env,
                                              jint data_type,
                                              jint data_len,
                                              const jbyte* data);

struct ArtJvmtiEventCallbacks : jvmtiEventCallbacks {
  ArtJvmtiEventCallbacks() : DdmPublishChunk(nullptr) {
    memset(this, 0, sizeof(jvmtiEventCallbacks));
  }

  // Copies extension functions from other callback struct if it exists. There must not have been
  // any modifications to this struct when it is called.
  void CopyExtensionsFrom(const ArtJvmtiEventCallbacks* cb);

  jvmtiError Set(jint index, jvmtiExtensionEvent cb);

  ArtJvmtiEventDdmPublishChunk DdmPublishChunk;
};

bool IsExtensionEvent(jint e);
bool IsExtensionEvent(ArtJvmtiEvent e);

// Convert a jvmtiEvent into a ArtJvmtiEvent
ALWAYS_INLINE static inline ArtJvmtiEvent GetArtJvmtiEvent(ArtJvmTiEnv* env, jvmtiEvent e);

static inline jvmtiEvent GetJvmtiEvent(ArtJvmtiEvent e) {
  if (UNLIKELY(e == ArtJvmtiEvent::kClassFileLoadHookRetransformable)) {
    return JVMTI_EVENT_CLASS_FILE_LOAD_HOOK;
  } else {
    return static_cast<jvmtiEvent>(e);
  }
}

struct EventMask {
  static constexpr size_t kEventsSize =
      static_cast<size_t>(ArtJvmtiEvent::kMaxEventTypeVal) -
      static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal) + 1;
  std::bitset<kEventsSize> bit_set;

  static bool EventIsInRange(ArtJvmtiEvent event) {
    return event >= ArtJvmtiEvent::kMinEventTypeVal && event <= ArtJvmtiEvent::kMaxEventTypeVal;
  }

  void Set(ArtJvmtiEvent event, bool value = true) {
    DCHECK(EventIsInRange(event));
    bit_set.set(static_cast<size_t>(event) - static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal),
                value);
  }

  bool Test(ArtJvmtiEvent event) const {
    DCHECK(EventIsInRange(event));
    return bit_set.test(
        static_cast<size_t>(event) - static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal));
  }
};

struct EventMasks {
  // The globally enabled events.
  EventMask global_event_mask;

  // The per-thread enabled events.

  // It is not enough to store a Thread pointer, as these may be reused. Use the pointer and the
  // thread id.
  // Note: We could just use the tid like tracing does.
  using UniqueThread = std::pair<art::Thread*, uint32_t>;
  // TODO: Native thread objects are immovable, so we can use them as keys in an (unordered) map,
  //       if necessary.
  std::vector<std::pair<UniqueThread, EventMask>> thread_event_masks;

  // A union of the per-thread events, for fast-pathing.
  EventMask unioned_thread_event_mask;

  EventMask& GetEventMask(art::Thread* thread);
  EventMask* GetEventMaskOrNull(art::Thread* thread);
  // Circular dependencies mean we cannot see the definition of ArtJvmTiEnv so the mutex is simply
  // asserted in the function.
  // Note that the 'env' passed in must be the same env this EventMasks is associated with.
  void EnableEvent(ArtJvmTiEnv* env, art::Thread* thread, ArtJvmtiEvent event);
      // REQUIRES(env->event_info_mutex_);
  // Circular dependencies mean we cannot see the definition of ArtJvmTiEnv so the mutex is simply
  // asserted in the function.
  // Note that the 'env' passed in must be the same env this EventMasks is associated with.
  void DisableEvent(ArtJvmTiEnv* env, art::Thread* thread, ArtJvmtiEvent event);
      // REQUIRES(env->event_info_mutex_);
  bool IsEnabledAnywhere(ArtJvmtiEvent event);
  // Make any changes to event masks needed for the given capability changes. If caps_added is true
  // then caps is all the newly set capabilities of the jvmtiEnv. If it is false then caps is the
  // set of all capabilities that were removed from the jvmtiEnv.
  void HandleChangedCapabilities(const jvmtiCapabilities& caps, bool caps_added);
};

namespace impl {
template <ArtJvmtiEvent kEvent> struct EventHandlerFunc { };
}  // namespace impl

// Helper class for event handling.
class EventHandler {
 public:
  EventHandler();
  ~EventHandler();

  // do cleanup for the event handler.
  void Shutdown();

  // Register an env. It is assumed that this happens on env creation, that is, no events are
  // enabled, yet.
  void RegisterArtJvmTiEnv(ArtJvmTiEnv* env) REQUIRES(!envs_lock_);

  // Remove an env.
  void RemoveArtJvmTiEnv(ArtJvmTiEnv* env) REQUIRES(!envs_lock_);

  bool IsEventEnabledAnywhere(ArtJvmtiEvent event) const {
    if (!EventMask::EventIsInRange(event)) {
      return false;
    }
    return global_mask.Test(event);
  }

  jvmtiError SetEvent(ArtJvmTiEnv* env,
                      art::Thread* thread,
                      ArtJvmtiEvent event,
                      jvmtiEventMode mode)
      REQUIRES(!envs_lock_);

  // Dispatch event to all registered environments. Since this one doesn't have a JNIEnv* it doesn't
  // matter if it has the mutator_lock.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEvent(art::Thread* thread, Args... args) const
      REQUIRES(!envs_lock_);

  // Dispatch event to all registered environments stashing exceptions as needed. This works since
  // JNIEnv* is always the second argument if it is passed to an event. Needed since C++ does not
  // allow partial template function specialization.
  //
  // We need both of these since we want to make sure to push a stack frame when it is possible for
  // the event to allocate local references.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEvent(art::Thread* thread, JNIEnv* jnienv, Args... args) const
      REQUIRES(!envs_lock_);

  // Tell the event handler capabilities were added/lost so it can adjust the sent events.If
  // caps_added is true then caps is all the newly set capabilities of the jvmtiEnv. If it is false
  // then caps is the set of all capabilities that were removed from the jvmtiEnv.
  ALWAYS_INLINE
  inline void HandleChangedCapabilities(ArtJvmTiEnv* env,
                                        const jvmtiCapabilities& caps,
                                        bool added)
      REQUIRES(!envs_lock_);

  // Dispatch event to the given environment, only.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEventOnEnv(ArtJvmTiEnv* env,
                                 art::Thread* thread,
                                 JNIEnv* jnienv,
                                 Args... args) const
      REQUIRES(!envs_lock_);

  // Dispatch event to the given environment, only.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEventOnEnv(ArtJvmTiEnv* env, art::Thread* thread, Args... args) const
      REQUIRES(!envs_lock_);

 private:
  void SetupTraceListener(JvmtiMethodTraceListener* listener, ArtJvmtiEvent event, bool enable);

  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline std::vector<impl::EventHandlerFunc<kEvent>> CollectEvents(art::Thread* thread,
                                                                   Args... args) const
      REQUIRES(!envs_lock_);

  template <ArtJvmtiEvent kEvent>
  ALWAYS_INLINE
  inline bool ShouldDispatchOnThread(ArtJvmTiEnv* env, art::Thread* thread) const;

  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  static inline void ExecuteCallback(impl::EventHandlerFunc<kEvent> handler,
                                     JNIEnv* env,
                                     Args... args)
      REQUIRES(!envs_lock_);

  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  static inline void ExecuteCallback(impl::EventHandlerFunc<kEvent> handler, Args... args)
      REQUIRES(!envs_lock_);

  // Public for use to collect dispatches
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline bool ShouldDispatch(ArtJvmTiEnv* env, art::Thread* thread, Args... args) const;

  ALWAYS_INLINE
  inline bool NeedsEventUpdate(ArtJvmTiEnv* env,
                               const jvmtiCapabilities& caps,
                               bool added);

  // Recalculates the event mask for the given event.
  ALWAYS_INLINE
  inline void RecalculateGlobalEventMask(ArtJvmtiEvent event) REQUIRES(!envs_lock_);
  ALWAYS_INLINE
  inline void RecalculateGlobalEventMaskLocked(ArtJvmtiEvent event) REQUIRES_SHARED(envs_lock_);

  template <ArtJvmtiEvent kEvent>
  ALWAYS_INLINE inline void DispatchClassFileLoadHookEvent(art::Thread* thread,
                                                           JNIEnv* jnienv,
                                                           jclass class_being_redefined,
                                                           jobject loader,
                                                           const char* name,
                                                           jobject protection_domain,
                                                           jint class_data_len,
                                                           const unsigned char* class_data,
                                                           jint* new_class_data_len,
                                                           unsigned char** new_class_data) const
      REQUIRES(!envs_lock_);

  void HandleEventType(ArtJvmtiEvent event, bool enable);
  void HandleLocalAccessCapabilityAdded();
  void HandleBreakpointEventsChanged(bool enable);

  bool OtherMonitorEventsEnabledAnywhere(ArtJvmtiEvent event);

  // List of all JvmTiEnv objects that have been created, in their creation order. It is a std::list
  // since we mostly access it by iterating over the entire thing, only ever append to the end, and
  // need to be able to remove arbitrary elements from it.
  std::list<ArtJvmTiEnv*> envs GUARDED_BY(envs_lock_);

  // Top level lock. Nothing at all should be held when we lock this.
  mutable art::ReaderWriterMutex envs_lock_
      ACQUIRED_BEFORE(art::Locks::instrument_entrypoints_lock_);

  // A union of all enabled events, anywhere.
  EventMask global_mask;

  std::unique_ptr<JvmtiAllocationListener> alloc_listener_;
  std::unique_ptr<JvmtiDdmChunkListener> ddm_listener_;
  std::unique_ptr<JvmtiGcPauseListener> gc_pause_listener_;
  std::unique_ptr<JvmtiMethodTraceListener> method_trace_listener_;
  std::unique_ptr<JvmtiMonitorListener> monitor_listener_;

  // True if frame pop has ever been enabled. Since we store pointers to stack frames we need to
  // continue to listen to this event even if it has been disabled.
  // TODO We could remove the listeners once all jvmtiEnvs have drained their shadow-frame vectors.
  bool frame_pop_enabled;
};

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_EVENTS_H_
