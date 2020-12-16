/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_RUNTIME_CALLBACKS_H_
#define ART_RUNTIME_RUNTIME_CALLBACKS_H_

#include <vector>

#include "base/array_ref.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "dex/dex_file.h"
#include "handle.h"

namespace art {

namespace mirror {
class Class;
class ClassLoader;
class Object;
}  // namespace mirror

class ArtMethod;
class ClassLoadCallback;
class Thread;
class MethodCallback;
class Monitor;
class ThreadLifecycleCallback;

// Note: RuntimeCallbacks uses the mutator lock to synchronize the callback lists. A thread must
//       hold the exclusive lock to add or remove a listener. A thread must hold the shared lock
//       to dispatch an event. This setup is chosen as some clients may want to suspend the
//       dispatching thread or all threads.
//
//       To make this safe, the following restrictions apply:
//       * Only the owner of a listener may ever add or remove said listener.
//       * A listener must never add or remove itself or any other listener while running.
//       * It is the responsibility of the owner to not remove the listener while it is running
//         (and suspended).
//
//       The simplest way to satisfy these restrictions is to never remove a listener, and to do
//       any state checking (is the listener enabled) in the listener itself. For an example, see
//       Dbg.

class DdmCallback {
 public:
  virtual ~DdmCallback() {}
  virtual void DdmPublishChunk(uint32_t type, const ArrayRef<const uint8_t>& data)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class DebuggerControlCallback {
 public:
  virtual ~DebuggerControlCallback() {}

  // Begin running the debugger.
  virtual void StartDebugger() = 0;
  // The debugger should begin shutting down since the runtime is ending. This is just advisory
  virtual void StopDebugger() = 0;

  // This allows the debugger to tell the runtime if it is configured.
  virtual bool IsDebuggerConfigured() = 0;
};

class RuntimeSigQuitCallback {
 public:
  virtual ~RuntimeSigQuitCallback() {}

  virtual void SigQuit() REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class RuntimePhaseCallback {
 public:
  enum RuntimePhase {
    kInitialAgents,   // Initial agent loading is done.
    kStart,           // The runtime is started.
    kInit,            // The runtime is initialized (and will run user code soon).
    kDeath,           // The runtime just died.
  };

  virtual ~RuntimePhaseCallback() {}

  virtual void NextRuntimePhase(RuntimePhase phase) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class MonitorCallback {
 public:
  // Called just before the thread goes to sleep to wait for the monitor to become unlocked.
  virtual void MonitorContendedLocking(Monitor* mon) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  // Called just after the monitor has been successfully acquired when it was already locked.
  virtual void MonitorContendedLocked(Monitor* mon) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  // Called on entry to the Object#wait method regardless of whether or not the call is valid.
  virtual void ObjectWaitStart(Handle<mirror::Object> obj, int64_t millis_timeout)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  // Called just after the monitor has woken up from going to sleep for a wait(). At this point the
  // thread does not possess a lock on the monitor. This will only be called for threads wait calls
  // where the thread did (or at least could have) gone to sleep.
  virtual void MonitorWaitFinished(Monitor* m, bool timed_out)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  virtual ~MonitorCallback() {}
};

// A callback to let parts of the runtime note that they are currently relying on a particular
// method remaining in it's current state. Users should not rely on always being called. If multiple
// callbacks are added the runtime will short-circuit when the first one returns 'true'.
class MethodInspectionCallback {
 public:
  virtual ~MethodInspectionCallback() {}

  // Returns true if the method is being inspected currently and the runtime should not modify it in
  // potentially dangerous ways (i.e. replace with compiled version, JIT it, etc).
  virtual bool IsMethodBeingInspected(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  // Returns true if the method is safe to Jit, false otherwise.
  // Note that '!IsMethodSafeToJit(m) implies IsMethodBeingInspected(m)'. That is that if this
  // method returns false IsMethodBeingInspected must return true.
  virtual bool IsMethodSafeToJit(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  // Returns true if we expect the method to be debuggable but are not doing anything unusual with
  // it currently.
  virtual bool MethodNeedsDebugVersion(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

class RuntimeCallbacks {
 public:
  void AddThreadLifecycleCallback(ThreadLifecycleCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveThreadLifecycleCallback(ThreadLifecycleCallback* cb) REQUIRES(Locks::mutator_lock_);

  void ThreadStart(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void ThreadDeath(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);

  void AddClassLoadCallback(ClassLoadCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveClassLoadCallback(ClassLoadCallback* cb) REQUIRES(Locks::mutator_lock_);

  void ClassLoad(Handle<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  void ClassPrepare(Handle<mirror::Class> temp_klass, Handle<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb)
      REQUIRES(Locks::mutator_lock_);
  void RemoveRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb)
      REQUIRES(Locks::mutator_lock_);

  void SigQuit() REQUIRES_SHARED(Locks::mutator_lock_);

  void AddRuntimePhaseCallback(RuntimePhaseCallback* cb)
      REQUIRES(Locks::mutator_lock_);
  void RemoveRuntimePhaseCallback(RuntimePhaseCallback* cb)
      REQUIRES(Locks::mutator_lock_);

  void NextRuntimePhase(RuntimePhaseCallback::RuntimePhase phase)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void ClassPreDefine(const char* descriptor,
                      Handle<mirror::Class> temp_class,
                      Handle<mirror::ClassLoader> loader,
                      const DexFile& initial_dex_file,
                      const DexFile::ClassDef& initial_class_def,
                      /*out*/DexFile const** final_dex_file,
                      /*out*/DexFile::ClassDef const** final_class_def)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddMethodCallback(MethodCallback* cb) REQUIRES(Locks::mutator_lock_);
  void RemoveMethodCallback(MethodCallback* cb) REQUIRES(Locks::mutator_lock_);

  void RegisterNativeMethod(ArtMethod* method,
                            const void* original_implementation,
                            /*out*/void** new_implementation)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void MonitorContendedLocking(Monitor* m) REQUIRES_SHARED(Locks::mutator_lock_);
  void MonitorContendedLocked(Monitor* m) REQUIRES_SHARED(Locks::mutator_lock_);
  void ObjectWaitStart(Handle<mirror::Object> m, int64_t timeout)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void MonitorWaitFinished(Monitor* m, bool timed_out)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddMonitorCallback(MonitorCallback* cb) REQUIRES_SHARED(Locks::mutator_lock_);
  void RemoveMonitorCallback(MonitorCallback* cb) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if some MethodInspectionCallback indicates the method is being inspected/depended
  // on by some code.
  bool IsMethodBeingInspected(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns false if some MethodInspectionCallback indicates the method cannot be safetly jitted
  // (which implies that it is being Inspected). Returns true otherwise. If it returns false the
  // entrypoint should not be changed to JITed code.
  bool IsMethodSafeToJit(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if some MethodInspectionCallback indicates the method needs to use a debug
  // version. This allows later code to set breakpoints or perform other actions that could be
  // broken by some optimizations.
  bool MethodNeedsDebugVersion(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  void AddMethodInspectionCallback(MethodInspectionCallback* cb)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RemoveMethodInspectionCallback(MethodInspectionCallback* cb)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // DDMS callbacks
  void DdmPublishChunk(uint32_t type, const ArrayRef<const uint8_t>& data)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AddDdmCallback(DdmCallback* cb) REQUIRES_SHARED(Locks::mutator_lock_);
  void RemoveDdmCallback(DdmCallback* cb) REQUIRES_SHARED(Locks::mutator_lock_);

  void StartDebugger() REQUIRES_SHARED(Locks::mutator_lock_);
  // NO_THREAD_SAFETY_ANALYSIS since this is only called when we are in the middle of shutting down
  // and the mutator_lock_ is no longer acquirable.
  void StopDebugger() NO_THREAD_SAFETY_ANALYSIS;
  bool IsDebuggerConfigured() REQUIRES_SHARED(Locks::mutator_lock_);

  void AddDebuggerControlCallback(DebuggerControlCallback* cb)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void RemoveDebuggerControlCallback(DebuggerControlCallback* cb)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  std::vector<ThreadLifecycleCallback*> thread_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<ClassLoadCallback*> class_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<RuntimeSigQuitCallback*> sigquit_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<RuntimePhaseCallback*> phase_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<MethodCallback*> method_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<MonitorCallback*> monitor_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<MethodInspectionCallback*> method_inspection_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<DdmCallback*> ddm_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
  std::vector<DebuggerControlCallback*> debugger_control_callbacks_
      GUARDED_BY(Locks::mutator_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_CALLBACKS_H_
