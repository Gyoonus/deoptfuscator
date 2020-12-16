// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <android-base/logging.h>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <jni.h>
#include <jvmti.h>
#include <memory>
#include <string>
#include <vector>

namespace breakpoint_logger {

struct SingleBreakpointTarget {
  std::string class_name;
  std::string method_name;
  std::string method_sig;
  jlocation location;
};

struct BreakpointTargets {
  std::vector<SingleBreakpointTarget> bps;
};

static void VMInitCB(jvmtiEnv* jvmti, JNIEnv* env, jthread thr ATTRIBUTE_UNUSED) {
  BreakpointTargets* all_targets = nullptr;
  jvmtiError err = jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&all_targets));
  if (err != JVMTI_ERROR_NONE || all_targets == nullptr) {
    env->FatalError("unable to get breakpoint targets");
  }
  for (const SingleBreakpointTarget& target : all_targets->bps) {
    jclass k = env->FindClass(target.class_name.c_str());
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->FatalError("Could not find class!");
      return;
    }
    jmethodID m = env->GetMethodID(k, target.method_name.c_str(), target.method_sig.c_str());
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m = env->GetStaticMethodID(k, target.method_name.c_str(), target.method_sig.c_str());
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->FatalError("Could not find method!");
        return;
      }
    }
    err = jvmti->SetBreakpoint(m, target.location);
    if (err != JVMTI_ERROR_NONE) {
      env->FatalError("unable to set breakpoint");
      return;
    }
    env->DeleteLocalRef(k);
  }
}

class ScopedThreadInfo {
 public:
  ScopedThreadInfo(jvmtiEnv* jvmti_env, JNIEnv* env, jthread thread)
      : jvmti_env_(jvmti_env), env_(env), free_name_(false) {
    memset(&info_, 0, sizeof(info_));
    if (thread == nullptr) {
      info_.name = const_cast<char*>("<NULLPTR>");
    } else if (jvmti_env->GetThreadInfo(thread, &info_) != JVMTI_ERROR_NONE) {
      info_.name = const_cast<char*>("<UNKNOWN THREAD>");
    } else {
      free_name_ = true;
    }
  }

  ~ScopedThreadInfo() {
    if (free_name_) {
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(info_.name));
    }
    env_->DeleteLocalRef(info_.thread_group);
    env_->DeleteLocalRef(info_.context_class_loader);
  }

  const char* GetName() const {
    return info_.name;
  }

 private:
  jvmtiEnv* jvmti_env_;
  JNIEnv* env_;
  bool free_name_;
  jvmtiThreadInfo info_;
};

class ScopedClassInfo {
 public:
  ScopedClassInfo(jvmtiEnv* jvmti_env, jclass c)
      : jvmti_env_(jvmti_env),
        class_(c),
        name_(nullptr),
        generic_(nullptr),
        file_(nullptr),
        debug_ext_(nullptr) {}

  ~ScopedClassInfo() {
    if (class_ != nullptr) {
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(name_));
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(file_));
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(debug_ext_));
    }
  }

  bool Init() {
    if (class_ == nullptr) {
      name_ = const_cast<char*>("<NONE>");
      generic_ = const_cast<char*>("<NONE>");
      return true;
    } else {
      jvmtiError ret1 = jvmti_env_->GetSourceFileName(class_, &file_);
      jvmtiError ret2 = jvmti_env_->GetSourceDebugExtension(class_, &debug_ext_);
      return jvmti_env_->GetClassSignature(class_, &name_, &generic_) == JVMTI_ERROR_NONE &&
          ret1 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret1 != JVMTI_ERROR_INVALID_CLASS &&
          ret2 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret2 != JVMTI_ERROR_INVALID_CLASS;
    }
  }

  jclass GetClass() const {
    return class_;
  }
  const char* GetName() const {
    return name_;
  }
  // Generic type parameters, whatever is in the <> for a class
  const char* GetGeneric() const {
    return generic_;
  }
  const char* GetSourceDebugExtension() const {
    if (debug_ext_ == nullptr) {
      return "<UNKNOWN_SOURCE_DEBUG_EXTENSION>";
    } else {
      return debug_ext_;
    }
  }
  const char* GetSourceFileName() const {
    if (file_ == nullptr) {
      return "<UNKNOWN_FILE>";
    } else {
      return file_;
    }
  }

 private:
  jvmtiEnv* jvmti_env_;
  jclass class_;
  char* name_;
  char* generic_;
  char* file_;
  char* debug_ext_;
};

class ScopedMethodInfo {
 public:
  ScopedMethodInfo(jvmtiEnv* jvmti_env, JNIEnv* env, jmethodID method)
      : jvmti_env_(jvmti_env),
        env_(env),
        method_(method),
        declaring_class_(nullptr),
        class_info_(nullptr),
        name_(nullptr),
        signature_(nullptr),
        generic_(nullptr),
        first_line_(-1) {}

  ~ScopedMethodInfo() {
    env_->DeleteLocalRef(declaring_class_);
    jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(name_));
    jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(signature_));
    jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
  }

  bool Init() {
    if (jvmti_env_->GetMethodDeclaringClass(method_, &declaring_class_) != JVMTI_ERROR_NONE) {
      return false;
    }
    class_info_.reset(new ScopedClassInfo(jvmti_env_, declaring_class_));
    jint nlines;
    jvmtiLineNumberEntry* lines;
    jvmtiError err = jvmti_env_->GetLineNumberTable(method_, &nlines, &lines);
    if (err == JVMTI_ERROR_NONE) {
      if (nlines > 0) {
        first_line_ = lines[0].line_number;
      }
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(lines));
    } else if (err != JVMTI_ERROR_ABSENT_INFORMATION &&
               err != JVMTI_ERROR_NATIVE_METHOD) {
      return false;
    }
    return class_info_->Init() &&
        (jvmti_env_->GetMethodName(method_, &name_, &signature_, &generic_) == JVMTI_ERROR_NONE);
  }

  const ScopedClassInfo& GetDeclaringClassInfo() const {
    return *class_info_;
  }

  jclass GetDeclaringClass() const {
    return declaring_class_;
  }

  const char* GetName() const {
    return name_;
  }

  const char* GetSignature() const {
    return signature_;
  }

  const char* GetGeneric() const {
    return generic_;
  }

  jint GetFirstLine() const {
    return first_line_;
  }

 private:
  jvmtiEnv* jvmti_env_;
  JNIEnv* env_;
  jmethodID method_;
  jclass declaring_class_;
  std::unique_ptr<ScopedClassInfo> class_info_;
  char* name_;
  char* signature_;
  char* generic_;
  jint first_line_;

  friend std::ostream& operator<<(std::ostream& os, ScopedMethodInfo const& method);
};

std::ostream& operator<<(std::ostream& os, const ScopedMethodInfo* method) {
  return os << *method;
}

std::ostream& operator<<(std::ostream& os, ScopedMethodInfo const& method) {
  return os << method.GetDeclaringClassInfo().GetName() << "->" << method.GetName()
            << method.GetSignature() << " (source: "
            << method.GetDeclaringClassInfo().GetSourceFileName() << ":" << method.GetFirstLine()
            << ")";
}

static void BreakpointCB(jvmtiEnv* jvmti_env,
                         JNIEnv* env,
                         jthread thread,
                         jmethodID method,
                         jlocation location) {
  ScopedThreadInfo info(jvmti_env, env, thread);
  ScopedMethodInfo method_info(jvmti_env, env, method);
  if (!method_info.Init()) {
    LOG(ERROR) << "Unable to get method info!";
    return;
  }
  LOG(WARNING) << "Breakpoint at location: 0x" << std::setw(8) << std::setfill('0') << std::hex
            << location << " in method " << method_info << " thread: " << info.GetName();
}

static std::string SubstrOf(const std::string& s, size_t start, size_t end) {
  if (end == std::string::npos) {
    end = s.size();
  }
  if (end == start) {
    return "";
  }
  CHECK_GT(end, start) << "cannot get substr of " << s;
  return s.substr(start, end - start);
}

static bool ParseSingleBreakpoint(const std::string& bp, /*out*/SingleBreakpointTarget* target) {
  std::string option = bp;
  if (option.empty() || option[0] != 'L' || option.find(';') == std::string::npos) {
    LOG(ERROR) << option << " doesn't look like it has a class name";
    return false;
  }
  target->class_name = SubstrOf(option, 1, option.find(';'));

  option = SubstrOf(option, option.find(';') + 1, std::string::npos);
  if (option.size() < 2 || option[0] != '-' || option[1] != '>') {
    LOG(ERROR) << bp << " doesn't seem to indicate a method, expected ->";
    return false;
  }
  option = SubstrOf(option, 2, std::string::npos);
  size_t sig_start = option.find('(');
  size_t loc_start = option.find('@');
  if (option.empty() || sig_start == std::string::npos) {
    LOG(ERROR) << bp << " doesn't seem to have a method sig!";
    return false;
  } else if (loc_start == std::string::npos ||
             loc_start < sig_start ||
             loc_start + 1 >= option.size()) {
    LOG(ERROR) << bp << " doesn't seem to have a valid location!";
    return false;
  }
  target->method_name = SubstrOf(option, 0, sig_start);
  target->method_sig = SubstrOf(option, sig_start, loc_start);
  target->location = std::stol(SubstrOf(option, loc_start + 1, std::string::npos));
  return true;
}

static std::string RemoveLastOption(const std::string& op) {
  if (op.find(',') == std::string::npos) {
    return "";
  } else {
    return SubstrOf(op, op.find(',') + 1, std::string::npos);
  }
}

// Fills targets with the breakpoints to add.
// Lname/of/Klass;->methodName(Lsig/of/Method)Lreturn/Type;@location,<...>
static bool ParseArgs(const std::string& start_options,
                      /*out*/BreakpointTargets* targets) {
  for (std::string options = start_options;
       !options.empty();
       options = RemoveLastOption(options)) {
    SingleBreakpointTarget target;
    std::string next = SubstrOf(options, 0, options.find(','));
    if (!ParseSingleBreakpoint(next, /*out*/ &target)) {
      LOG(ERROR) << "Unable to parse breakpoint from " << next;
      return false;
    }
    targets->bps.push_back(target);
  }
  return true;
}

enum class StartType {
  OnAttach, OnLoad,
};

static jint AgentStart(StartType start,
                       JavaVM* vm,
                       char* options,
                       void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* jvmti = nullptr;
  jvmtiError error = JVMTI_ERROR_NONE;
  {
    jint res = 0;
    res = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_1);

    if (res != JNI_OK || jvmti == nullptr) {
      LOG(ERROR) << "Unable to access JVMTI, error code " << res;
      return JNI_ERR;
    }
  }

  void* bp_target_mem = nullptr;
  error = jvmti->Allocate(sizeof(BreakpointTargets),
                          reinterpret_cast<unsigned char**>(&bp_target_mem));
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to alloc memory for breakpoint target data";
    return JNI_ERR;
  }

  BreakpointTargets* data = new(bp_target_mem) BreakpointTargets;
  error = jvmti->SetEnvironmentLocalStorage(data);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set local storage";
    return JNI_ERR;
  }

  if (!ParseArgs(options, /*out*/data)) {
    LOG(ERROR) << "failed to parse breakpoint list!";
    return JNI_ERR;
  }

  jvmtiCapabilities caps{};
  caps.can_generate_breakpoint_events = JNI_TRUE;
  caps.can_get_line_numbers           = JNI_TRUE;
  caps.can_get_source_file_name       = JNI_TRUE;
  caps.can_get_source_debug_extension = JNI_TRUE;
  error = jvmti->AddCapabilities(&caps);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set caps";
    return JNI_ERR;
  }

  jvmtiEventCallbacks callbacks{};
  callbacks.Breakpoint = &BreakpointCB;
  callbacks.VMInit = &VMInitCB;

  error = jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));

  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set event callbacks.";
    return JNI_ERR;
  }

  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                          JVMTI_EVENT_BREAKPOINT,
                                          nullptr /* all threads */);
  if (error != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to enable breakpoint event";
    return JNI_ERR;
  }
  if (start == StartType::OnAttach) {
    JNIEnv* env = nullptr;
    jint res = 0;
    res = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
    if (res != JNI_OK || env == nullptr) {
      LOG(ERROR) << "Unable to get jnienv";
      return JNI_ERR;
    }
    VMInitCB(jvmti, env, nullptr);
  } else {
    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_VM_INIT,
                                            nullptr /* all threads */);
    if (error != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to set event vminit";
      return JNI_ERR;
    }
  }
  return JNI_OK;
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char* options, void* reserved) {
  return AgentStart(StartType::OnAttach, vm, options, reserved);
}

// Early attachment
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart(StartType::OnLoad, jvm, options, reserved);
}

}  // namespace breakpoint_logger

