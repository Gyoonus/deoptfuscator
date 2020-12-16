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

#include "runtime_callbacks.h"

#include <algorithm>

#include "art_method.h"
#include "base/macros.h"
#include "class_linker.h"
#include "monitor.h"
#include "thread.h"

namespace art {

template <typename T>
ALWAYS_INLINE
static inline void Remove(T* cb, std::vector<T*>* data) {
  auto it = std::find(data->begin(), data->end(), cb);
  if (it != data->end()) {
    data->erase(it);
  }
}

void RuntimeCallbacks::AddDdmCallback(DdmCallback* cb) {
  ddm_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveDdmCallback(DdmCallback* cb) {
  Remove(cb, &ddm_callbacks_);
}

void RuntimeCallbacks::DdmPublishChunk(uint32_t type, const ArrayRef<const uint8_t>& data) {
  for (DdmCallback* cb : ddm_callbacks_) {
    cb->DdmPublishChunk(type, data);
  }
}

void RuntimeCallbacks::AddDebuggerControlCallback(DebuggerControlCallback* cb) {
  debugger_control_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveDebuggerControlCallback(DebuggerControlCallback* cb) {
  Remove(cb, &debugger_control_callbacks_);
}

bool RuntimeCallbacks::IsDebuggerConfigured() {
  for (DebuggerControlCallback* cb : debugger_control_callbacks_) {
    if (cb->IsDebuggerConfigured()) {
      return true;
    }
  }
  return false;
}

void RuntimeCallbacks::StartDebugger() {
  for (DebuggerControlCallback* cb : debugger_control_callbacks_) {
    cb->StartDebugger();
  }
}

void RuntimeCallbacks::StopDebugger() {
  for (DebuggerControlCallback* cb : debugger_control_callbacks_) {
    cb->StopDebugger();
  }
}

void RuntimeCallbacks::AddMethodInspectionCallback(MethodInspectionCallback* cb) {
  method_inspection_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveMethodInspectionCallback(MethodInspectionCallback* cb) {
  Remove(cb, &method_inspection_callbacks_);
}

bool RuntimeCallbacks::IsMethodSafeToJit(ArtMethod* m) {
  for (MethodInspectionCallback* cb : method_inspection_callbacks_) {
    if (!cb->IsMethodSafeToJit(m)) {
      DCHECK(cb->IsMethodBeingInspected(m))
          << "Contract requires that !IsMethodSafeToJit(m) -> IsMethodBeingInspected(m)";
      return false;
    }
  }
  return true;
}

bool RuntimeCallbacks::IsMethodBeingInspected(ArtMethod* m) {
  for (MethodInspectionCallback* cb : method_inspection_callbacks_) {
    if (cb->IsMethodBeingInspected(m)) {
      return true;
    }
  }
  return false;
}

bool RuntimeCallbacks::MethodNeedsDebugVersion(ArtMethod* m) {
  for (MethodInspectionCallback* cb : method_inspection_callbacks_) {
    if (cb->MethodNeedsDebugVersion(m)) {
      return true;
    }
  }
  return false;
}

void RuntimeCallbacks::AddThreadLifecycleCallback(ThreadLifecycleCallback* cb) {
  thread_callbacks_.push_back(cb);
}

void RuntimeCallbacks::MonitorContendedLocking(Monitor* m) {
  for (MonitorCallback* cb : monitor_callbacks_) {
    cb->MonitorContendedLocking(m);
  }
}

void RuntimeCallbacks::MonitorContendedLocked(Monitor* m) {
  for (MonitorCallback* cb : monitor_callbacks_) {
    cb->MonitorContendedLocked(m);
  }
}

void RuntimeCallbacks::ObjectWaitStart(Handle<mirror::Object> m, int64_t timeout) {
  for (MonitorCallback* cb : monitor_callbacks_) {
    cb->ObjectWaitStart(m, timeout);
  }
}

void RuntimeCallbacks::MonitorWaitFinished(Monitor* m, bool timeout) {
  for (MonitorCallback* cb : monitor_callbacks_) {
    cb->MonitorWaitFinished(m, timeout);
  }
}

void RuntimeCallbacks::AddMonitorCallback(MonitorCallback* cb) {
  monitor_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveMonitorCallback(MonitorCallback* cb) {
  Remove(cb, &monitor_callbacks_);
}

void RuntimeCallbacks::RemoveThreadLifecycleCallback(ThreadLifecycleCallback* cb) {
  Remove(cb, &thread_callbacks_);
}

void RuntimeCallbacks::ThreadStart(Thread* self) {
  for (ThreadLifecycleCallback* cb : thread_callbacks_) {
    cb->ThreadStart(self);
  }
}

void RuntimeCallbacks::ThreadDeath(Thread* self) {
  for (ThreadLifecycleCallback* cb : thread_callbacks_) {
    cb->ThreadDeath(self);
  }
}

void RuntimeCallbacks::AddClassLoadCallback(ClassLoadCallback* cb) {
  class_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveClassLoadCallback(ClassLoadCallback* cb) {
  Remove(cb, &class_callbacks_);
}

void RuntimeCallbacks::ClassLoad(Handle<mirror::Class> klass) {
  for (ClassLoadCallback* cb : class_callbacks_) {
    cb->ClassLoad(klass);
  }
}

void RuntimeCallbacks::ClassPreDefine(const char* descriptor,
                                      Handle<mirror::Class> temp_class,
                                      Handle<mirror::ClassLoader> loader,
                                      const DexFile& initial_dex_file,
                                      const DexFile::ClassDef& initial_class_def,
                                      /*out*/DexFile const** final_dex_file,
                                      /*out*/DexFile::ClassDef const** final_class_def) {
  DexFile const* current_dex_file = &initial_dex_file;
  DexFile::ClassDef const* current_class_def = &initial_class_def;
  for (ClassLoadCallback* cb : class_callbacks_) {
    DexFile const* new_dex_file = nullptr;
    DexFile::ClassDef const* new_class_def = nullptr;
    cb->ClassPreDefine(descriptor,
                       temp_class,
                       loader,
                       *current_dex_file,
                       *current_class_def,
                       &new_dex_file,
                       &new_class_def);
    if ((new_dex_file != nullptr && new_dex_file != current_dex_file) ||
        (new_class_def != nullptr && new_class_def != current_class_def)) {
      DCHECK(new_dex_file != nullptr && new_class_def != nullptr);
      current_dex_file = new_dex_file;
      current_class_def = new_class_def;
    }
  }
  *final_dex_file = current_dex_file;
  *final_class_def = current_class_def;
}

void RuntimeCallbacks::ClassPrepare(Handle<mirror::Class> temp_klass, Handle<mirror::Class> klass) {
  for (ClassLoadCallback* cb : class_callbacks_) {
    cb->ClassPrepare(temp_klass, klass);
  }
}

void RuntimeCallbacks::AddRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb) {
  sigquit_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveRuntimeSigQuitCallback(RuntimeSigQuitCallback* cb) {
  Remove(cb, &sigquit_callbacks_);
}

void RuntimeCallbacks::SigQuit() {
  for (RuntimeSigQuitCallback* cb : sigquit_callbacks_) {
    cb->SigQuit();
  }
}

void RuntimeCallbacks::AddRuntimePhaseCallback(RuntimePhaseCallback* cb) {
  phase_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveRuntimePhaseCallback(RuntimePhaseCallback* cb) {
  Remove(cb, &phase_callbacks_);
}

void RuntimeCallbacks::NextRuntimePhase(RuntimePhaseCallback::RuntimePhase phase) {
  for (RuntimePhaseCallback* cb : phase_callbacks_) {
    cb->NextRuntimePhase(phase);
  }
}

void RuntimeCallbacks::AddMethodCallback(MethodCallback* cb) {
  method_callbacks_.push_back(cb);
}

void RuntimeCallbacks::RemoveMethodCallback(MethodCallback* cb) {
  Remove(cb, &method_callbacks_);
}

void RuntimeCallbacks::RegisterNativeMethod(ArtMethod* method,
                                            const void* in_cur_method,
                                            /*out*/void** new_method) {
  void* cur_method = const_cast<void*>(in_cur_method);
  *new_method = cur_method;
  for (MethodCallback* cb : method_callbacks_) {
    cb->RegisterNativeMethod(method, cur_method, new_method);
    if (*new_method != nullptr) {
      cur_method = *new_method;
    }
  }
}

}  // namespace art
