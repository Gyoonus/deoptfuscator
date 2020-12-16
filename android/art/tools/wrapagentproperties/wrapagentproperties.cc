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
#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <jni.h>
#include <jvmti.h>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace wrapagentproperties {

using PropMap = std::unordered_map<std::string, std::string>;
static constexpr const char* kOnLoad = "Agent_OnLoad";
static constexpr const char* kOnAttach = "Agent_OnAttach";
static constexpr const char* kOnUnload = "Agent_OnUnload";
struct ProxyJavaVM;
using AgentLoadFunction = jint (*)(ProxyJavaVM*, const char*, void*);
using AgentUnloadFunction = jint (*)(JavaVM*);

// Global namespace. Shared by every usage of this wrapper unfortunately.
// We need to keep track of them to call Agent_OnUnload.
static std::mutex unload_mutex;

struct Unloader {
  AgentUnloadFunction unload;
};
static std::vector<Unloader> unload_functions;

static jint CreateJvmtiEnv(ProxyJavaVM* vm, void** out_env, jint version);

struct ProxyJavaVM {
  const struct JNIInvokeInterface* functions;
  JavaVM* real_vm;
  PropMap* map;
  void* dlopen_handle;
  AgentLoadFunction load;
  AgentLoadFunction attach;

  ProxyJavaVM(JavaVM* vm, const std::string& agent_lib, PropMap* map)
      : functions(CreateInvokeInterface()),
        real_vm(vm),
        map(map),
        dlopen_handle(dlopen(agent_lib.c_str(), RTLD_LAZY)),
        load(nullptr),
        attach(nullptr) {
    CHECK(dlopen_handle != nullptr) << "unable to open " << agent_lib;
    {
      std::lock_guard<std::mutex> lk(unload_mutex);
      unload_functions.push_back({
        reinterpret_cast<AgentUnloadFunction>(dlsym(dlopen_handle, kOnUnload)),
      });
    }
    attach = reinterpret_cast<AgentLoadFunction>(dlsym(dlopen_handle, kOnAttach));
    load = reinterpret_cast<AgentLoadFunction>(dlsym(dlopen_handle, kOnLoad));
  }

  // TODO Use this to cleanup
  static jint WrapDestroyJavaVM(ProxyJavaVM* vm) {
    return vm->real_vm->DestroyJavaVM();
  }
  static jint WrapAttachCurrentThread(ProxyJavaVM* vm, JNIEnv** env, void* res) {
    return vm->real_vm->AttachCurrentThread(env, res);
  }
  static jint WrapDetachCurrentThread(ProxyJavaVM* vm) {
    return vm->real_vm->DetachCurrentThread();
  }
  static jint WrapAttachCurrentThreadAsDaemon(ProxyJavaVM* vm, JNIEnv** env, void* res) {
    return vm->real_vm->AttachCurrentThreadAsDaemon(env, res);
  }

  static jint WrapGetEnv(ProxyJavaVM* vm, void** out_env, jint version) {
    switch (version) {
      case JVMTI_VERSION:
      case JVMTI_VERSION_1:
      case JVMTI_VERSION_1_1:
      case JVMTI_VERSION_1_2:
        return CreateJvmtiEnv(vm, out_env, version);
      default:
        if ((version & 0x30000000) == 0x30000000) {
          LOG(ERROR) << "Version number 0x" << std::hex << version << " looks like a JVMTI "
                     << "version but it is not one that is recognized. The wrapper might not "
                     << "function correctly! Continuing anyway.";
        }
        return vm->real_vm->GetEnv(out_env, version);
    }
  }

  static JNIInvokeInterface* CreateInvokeInterface() {
    JNIInvokeInterface* out = new JNIInvokeInterface;
    memset(out, 0, sizeof(JNIInvokeInterface));
    out->DestroyJavaVM = reinterpret_cast<jint (*)(JavaVM*)>(WrapDestroyJavaVM);
    out->AttachCurrentThread =
        reinterpret_cast<jint(*)(JavaVM*, JNIEnv**, void*)>(WrapAttachCurrentThread);
    out->DetachCurrentThread = reinterpret_cast<jint(*)(JavaVM*)>(WrapDetachCurrentThread);
    out->GetEnv = reinterpret_cast<jint(*)(JavaVM*, void**, jint)>(WrapGetEnv);
    out->AttachCurrentThreadAsDaemon =
        reinterpret_cast<jint(*)(JavaVM*, JNIEnv**, void*)>(WrapAttachCurrentThreadAsDaemon);
    return out;
  }
};


struct ExtraJvmtiInterface : public jvmtiInterface_1_ {
  ProxyJavaVM* proxy_vm;
  jvmtiInterface_1_ const* original_interface;

  static jvmtiError WrapDisposeEnvironment(jvmtiEnv* env) {
    ExtraJvmtiInterface* funcs = reinterpret_cast<ExtraJvmtiInterface*>(
        const_cast<jvmtiInterface_1_*>(env->functions));
    jvmtiInterface_1_** out_iface = const_cast<jvmtiInterface_1_**>(&env->functions);
    *out_iface = const_cast<jvmtiInterface_1_*>(funcs->original_interface);
    funcs->original_interface->Deallocate(env, reinterpret_cast<unsigned char*>(funcs));
    jvmtiError res = (*out_iface)->DisposeEnvironment(env);
    return res;
  }

  static jvmtiError WrapGetSystemProperty(jvmtiEnv* env, const char* prop, char** out) {
    ExtraJvmtiInterface* funcs = reinterpret_cast<ExtraJvmtiInterface*>(
        const_cast<jvmtiInterface_1_*>(env->functions));
    if (funcs->proxy_vm->map->find(prop) != funcs->proxy_vm->map->end()) {
      std::string str_prop(prop);
      const std::string& val = funcs->proxy_vm->map->at(str_prop);
      jvmtiError res = env->Allocate(val.size() + 1, reinterpret_cast<unsigned char**>(out));
      if (res != JVMTI_ERROR_NONE) {
        return res;
      }
      strcpy(*out, val.c_str());
      return JVMTI_ERROR_NONE;
    } else {
      return funcs->original_interface->GetSystemProperty(env, prop, out);
    }
  }

  static jvmtiError WrapGetSystemProperties(jvmtiEnv* env, jint* cnt, char*** prop_ptr) {
    ExtraJvmtiInterface* funcs = reinterpret_cast<ExtraJvmtiInterface*>(
        const_cast<jvmtiInterface_1_*>(env->functions));
    jint init_cnt;
    char** init_prop_ptr;
    jvmtiError res = funcs->original_interface->GetSystemProperties(env, &init_cnt, &init_prop_ptr);
    if (res != JVMTI_ERROR_NONE) {
      return res;
    }
    std::unordered_set<std::string> all_props;
    for (const auto& p : *funcs->proxy_vm->map) {
      all_props.insert(p.first);
    }
    for (jint i = 0; i < init_cnt; i++) {
      all_props.insert(init_prop_ptr[i]);
      env->Deallocate(reinterpret_cast<unsigned char*>(init_prop_ptr[i]));
    }
    env->Deallocate(reinterpret_cast<unsigned char*>(init_prop_ptr));
    *cnt = all_props.size();
    res = env->Allocate(all_props.size() * sizeof(char*),
                        reinterpret_cast<unsigned char**>(prop_ptr));
    if (res != JVMTI_ERROR_NONE) {
      return res;
    }
    char** out_prop_ptr = *prop_ptr;
    jint i = 0;
    for (const std::string& p : all_props) {
      res = env->Allocate(p.size() + 1, reinterpret_cast<unsigned char**>(&out_prop_ptr[i]));
      if (res != JVMTI_ERROR_NONE) {
        return res;
      }
      strcpy(out_prop_ptr[i], p.c_str());
      i++;
    }
    CHECK_EQ(i, *cnt);
    return JVMTI_ERROR_NONE;
  }

  static jvmtiError WrapSetSystemProperty(jvmtiEnv* env, const char* prop, const char* val) {
    ExtraJvmtiInterface* funcs = reinterpret_cast<ExtraJvmtiInterface*>(
        const_cast<jvmtiInterface_1_*>(env->functions));
    jvmtiError res = funcs->original_interface->SetSystemProperty(env, prop, val);
    if (res != JVMTI_ERROR_NONE) {
      return res;
    }
    if (funcs->proxy_vm->map->find(prop) != funcs->proxy_vm->map->end()) {
      funcs->proxy_vm->map->at(prop) = val;
    }
    return JVMTI_ERROR_NONE;
  }

  // TODO It would be way better to actually set up a full proxy like we did for JavaVM but the
  // number of functions makes it not worth it.
  static jint SetupProxyJvmtiEnv(ProxyJavaVM* vm, jvmtiEnv* real_env) {
    ExtraJvmtiInterface* new_iface = nullptr;
    if (JVMTI_ERROR_NONE != real_env->Allocate(sizeof(ExtraJvmtiInterface),
                                              reinterpret_cast<unsigned char**>(&new_iface))) {
      LOG(ERROR) << "Could not allocate extra space for new jvmti interface struct";
      return JNI_ERR;
    }
    memcpy(new_iface, real_env->functions, sizeof(jvmtiInterface_1_));
    new_iface->proxy_vm = vm;
    new_iface->original_interface = real_env->functions;

    // Replace these functions with the new ones.
    new_iface->DisposeEnvironment = WrapDisposeEnvironment;
    new_iface->GetSystemProperty = WrapGetSystemProperty;
    new_iface->GetSystemProperties = WrapGetSystemProperties;
    new_iface->SetSystemProperty = WrapSetSystemProperty;

    // Replace the functions table with our new one with replaced functions.
    jvmtiInterface_1_** out_iface = const_cast<jvmtiInterface_1_**>(&real_env->functions);
    *out_iface = new_iface;
    return JNI_OK;
  }
};

static jint CreateJvmtiEnv(ProxyJavaVM* vm, void** out_env, jint version) {
  jint res = vm->real_vm->GetEnv(out_env, version);
  if (res != JNI_OK) {
    LOG(WARNING) << "Could not create jvmtiEnv to proxy!";
    return res;
  }
  return ExtraJvmtiInterface::SetupProxyJvmtiEnv(vm, reinterpret_cast<jvmtiEnv*>(*out_env));
}

enum class StartType {
  OnAttach, OnLoad,
};

static jint CallNextAgent(StartType start,
                          ProxyJavaVM* vm,
                          std::string options,
                          void* reserved) {
  // TODO It might be good to set it up so that the library is unloaded even if no jvmtiEnv's are
  // created but this isn't expected to be common so we will just not bother.
  return ((start == StartType::OnLoad) ? vm->load : vm->attach)(vm, options.c_str(), reserved);
}

static std::string substrOf(const std::string& s, size_t start, size_t end) {
  if (end == start) {
    return "";
  } else if (end == std::string::npos) {
    end = s.size();
  }
  return s.substr(start, end - start);
}

static PropMap* ReadPropMap(const std::string& file) {
  std::unique_ptr<PropMap> map(new PropMap);
  std::ifstream prop_file(file, std::ios::in);
  std::string line;
  while (std::getline(prop_file, line)) {
    if (line.size() == 0 || line[0] == '#') {
      continue;
    }
    if (line.find('=') == std::string::npos) {
      LOG(INFO) << "line: " << line << " didn't have a '='";
      return nullptr;
    }
    std::string prop = substrOf(line, 0, line.find('='));
    std::string val = substrOf(line, line.find('=') + 1, std::string::npos);
    LOG(INFO) << "Overriding property " << std::quoted(prop) << " new value is "
              << std::quoted(val);
    map->insert({prop, val});
  }
  return map.release();
}

static bool ParseArgs(const std::string& options,
                      /*out*/std::string* prop_file,
                      /*out*/std::string* agent_lib,
                      /*out*/std::string* agent_options) {
  if (options.find(',') == std::string::npos) {
    LOG(ERROR) << "No agent lib in " << options;
    return false;
  }
  *prop_file = substrOf(options, 0, options.find(','));
  *agent_lib = substrOf(options, options.find(',') + 1, options.find('='));
  if (options.find('=') != std::string::npos) {
    *agent_options = substrOf(options, options.find('=') + 1, std::string::npos);
  } else {
    *agent_options = "";
  }
  return true;
}

static jint AgentStart(StartType start, JavaVM* vm, char* options, void* reserved) {
  std::string agent_lib;
  std::string agent_options;
  std::string prop_file;
  if (!ParseArgs(options, /*out*/ &prop_file, /*out*/ &agent_lib, /*out*/ &agent_options)) {
    return JNI_ERR;
  }
  // It would be good to not leak these but since they will live for almost the whole program run
  // anyway it isn't a huge deal.
  PropMap* map = ReadPropMap(prop_file);
  if (map == nullptr) {
    LOG(ERROR) << "unable to read property file at " << std::quoted(prop_file) << "!";
    return JNI_ERR;
  }
  ProxyJavaVM* proxy = new ProxyJavaVM(vm, agent_lib, map);
  LOG(INFO) << "Chaining to next agent[" << std::quoted(agent_lib) << "] options=["
            << std::quoted(agent_options) << "]";
  return CallNextAgent(start, proxy, agent_options, reserved);
}

// Late attachment (e.g. 'am attach-agent').
extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char* options, void* reserved) {
  return AgentStart(StartType::OnAttach, vm, options, reserved);
}

// Early attachment
// (e.g. 'java -agentpath:/path/to/libwrapagentproperties.so=/path/to/propfile,/path/to/wrapped.so=[ops]').
extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* reserved) {
  return AgentStart(StartType::OnLoad, jvm, options, reserved);
}

extern "C" JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* jvm) {
  std::lock_guard<std::mutex> lk(unload_mutex);
  for (const Unloader& u : unload_functions) {
    u.unload(jvm);
    // Don't dlclose since some agents expect to still have code loaded after this.
  }
  unload_functions.clear();
}

}  // namespace wrapagentproperties

