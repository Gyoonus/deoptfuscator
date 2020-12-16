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

#include "agent.h"

#include "android-base/stringprintf.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativeloader/native_loader.h"

#include "base/strlcpy.h"
#include "java_vm_ext.h"
#include "runtime.h"
#include "thread-current-inl.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace ti {

using android::base::StringPrintf;

const char* AGENT_ON_LOAD_FUNCTION_NAME = "Agent_OnLoad";
const char* AGENT_ON_ATTACH_FUNCTION_NAME = "Agent_OnAttach";
const char* AGENT_ON_UNLOAD_FUNCTION_NAME = "Agent_OnUnload";

AgentSpec::AgentSpec(const std::string& arg) {
  size_t eq = arg.find_first_of('=');
  if (eq == std::string::npos) {
    name_ = arg;
  } else {
    name_ = arg.substr(0, eq);
    args_ = arg.substr(eq + 1, arg.length());
  }
}

std::unique_ptr<Agent> AgentSpec::Load(/*out*/jint* call_res,
                                       /*out*/LoadError* error,
                                       /*out*/std::string* error_msg) {
  VLOG(agents) << "Loading agent: " << name_ << " " << args_;
  return DoLoadHelper(nullptr, false, nullptr, call_res, error, error_msg);
}

// Tries to attach the agent using its OnAttach method. Returns true on success.
std::unique_ptr<Agent> AgentSpec::Attach(JNIEnv* env,
                                         jobject class_loader,
                                         /*out*/jint* call_res,
                                         /*out*/LoadError* error,
                                         /*out*/std::string* error_msg) {
  VLOG(agents) << "Attaching agent: " << name_ << " " << args_;
  return DoLoadHelper(env, true, class_loader, call_res, error, error_msg);
}


// TODO We need to acquire some locks probably.
std::unique_ptr<Agent> AgentSpec::DoLoadHelper(JNIEnv* env,
                                               bool attaching,
                                               jobject class_loader,
                                               /*out*/jint* call_res,
                                               /*out*/LoadError* error,
                                               /*out*/std::string* error_msg) {
  ScopedThreadStateChange stsc(Thread::Current(), ThreadState::kNative);
  DCHECK(call_res != nullptr);
  DCHECK(error_msg != nullptr);

  std::unique_ptr<Agent> agent = DoDlOpen(env, class_loader, error, error_msg);
  if (agent == nullptr) {
    VLOG(agents) << "err: " << *error_msg;
    return nullptr;
  }
  AgentOnLoadFunction callback = attaching ? agent->onattach_ : agent->onload_;
  if (callback == nullptr) {
    *error_msg = StringPrintf("Unable to start agent %s: No %s callback found",
                              (attaching ? "attach" : "load"),
                              name_.c_str());
    VLOG(agents) << "err: " << *error_msg;
    *error = kLoadingError;
    return nullptr;
  }
  // Need to let the function fiddle with the array.
  std::unique_ptr<char[]> copied_args(new char[args_.size() + 1]);
  strlcpy(copied_args.get(), args_.c_str(), args_.size() + 1);
  // TODO Need to do some checks that we are at a good spot etc.
  *call_res = callback(Runtime::Current()->GetJavaVM(),
                       copied_args.get(),
                       nullptr);
  if (*call_res != 0) {
    *error_msg = StringPrintf("Initialization of %s returned non-zero value of %d",
                              name_.c_str(), *call_res);
    VLOG(agents) << "err: " << *error_msg;
    *error = kInitializationError;
    return nullptr;
  }
  return agent;
}

std::unique_ptr<Agent> AgentSpec::DoDlOpen(JNIEnv* env,
                                           jobject class_loader,
                                           /*out*/LoadError* error,
                                           /*out*/std::string* error_msg) {
  DCHECK(error_msg != nullptr);

  ScopedLocalRef<jstring> library_path(env,
                                       class_loader == nullptr
                                           ? nullptr
                                           : JavaVMExt::GetLibrarySearchPath(env, class_loader));

  bool needs_native_bridge = false;
  std::string nativeloader_error_msg;
  void* dlopen_handle = android::OpenNativeLibrary(env,
                                                   Runtime::Current()->GetTargetSdkVersion(),
                                                   name_.c_str(),
                                                   class_loader,
                                                   library_path.get(),
                                                   &needs_native_bridge,
                                                   &nativeloader_error_msg);
  if (dlopen_handle == nullptr) {
    *error_msg = StringPrintf("Unable to dlopen %s: %s",
                              name_.c_str(),
                              nativeloader_error_msg.c_str());
    *error = kLoadingError;
    return nullptr;
  }
  if (needs_native_bridge) {
    // TODO: Consider support?
    android::CloseNativeLibrary(dlopen_handle, needs_native_bridge);
    *error_msg = StringPrintf("Native-bridge agents unsupported: %s", name_.c_str());
    *error = kLoadingError;
    return nullptr;
  }

  std::unique_ptr<Agent> agent(new Agent(name_, dlopen_handle));
  agent->PopulateFunctions();
  *error = kNoError;
  return agent;
}

std::ostream& operator<<(std::ostream &os, AgentSpec const& m) {
  return os << "AgentSpec { name=\"" << m.name_ << "\", args=\"" << m.args_ << "\" }";
}


void* Agent::FindSymbol(const std::string& name) const {
  CHECK(dlopen_handle_ != nullptr) << "Cannot find symbols in an unloaded agent library " << this;
  return dlsym(dlopen_handle_, name.c_str());
}

// TODO Lock some stuff probably.
void Agent::Unload() {
  if (dlopen_handle_ != nullptr) {
    if (onunload_ != nullptr) {
      onunload_(Runtime::Current()->GetJavaVM());
    }
    // Don't actually android::CloseNativeLibrary since some agents assume they will never get
    // unloaded. Since this only happens when the runtime is shutting down anyway this isn't a big
    // deal.
    dlopen_handle_ = nullptr;
    onload_ = nullptr;
    onattach_ = nullptr;
    onunload_ = nullptr;
  } else {
    VLOG(agents) << this << " is not currently loaded!";
  }
}

Agent::Agent(Agent&& other)
    : dlopen_handle_(nullptr),
      onload_(nullptr),
      onattach_(nullptr),
      onunload_(nullptr) {
  *this = std::move(other);
}

Agent& Agent::operator=(Agent&& other) {
  if (this != &other) {
    if (dlopen_handle_ != nullptr) {
      Unload();
    }
    name_ = std::move(other.name_);
    dlopen_handle_ = other.dlopen_handle_;
    onload_ = other.onload_;
    onattach_ = other.onattach_;
    onunload_ = other.onunload_;
    other.dlopen_handle_ = nullptr;
    other.onload_ = nullptr;
    other.onattach_ = nullptr;
    other.onunload_ = nullptr;
  }
  return *this;
}

void Agent::PopulateFunctions() {
  onload_ = reinterpret_cast<AgentOnLoadFunction>(FindSymbol(AGENT_ON_LOAD_FUNCTION_NAME));
  if (onload_ == nullptr) {
    VLOG(agents) << "Unable to find 'Agent_OnLoad' symbol in " << this;
  }
  onattach_ = reinterpret_cast<AgentOnLoadFunction>(FindSymbol(AGENT_ON_ATTACH_FUNCTION_NAME));
  if (onattach_ == nullptr) {
    VLOG(agents) << "Unable to find 'Agent_OnAttach' symbol in " << this;
  }
  onunload_ = reinterpret_cast<AgentOnUnloadFunction>(FindSymbol(AGENT_ON_UNLOAD_FUNCTION_NAME));
  if (onunload_ == nullptr) {
    VLOG(agents) << "Unable to find 'Agent_OnUnload' symbol in " << this;
  }
}

Agent::~Agent() {
  if (dlopen_handle_ != nullptr) {
    Unload();
  }
}

std::ostream& operator<<(std::ostream &os, const Agent* m) {
  return os << *m;
}

std::ostream& operator<<(std::ostream &os, Agent const& m) {
  return os << "Agent { name=\"" << m.name_ << "\", handle=" << m.dlopen_handle_ << " }";
}

}  // namespace ti
}  // namespace art
