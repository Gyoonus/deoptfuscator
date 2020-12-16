/*
 * Copyright 2016 The Android Open Source Project
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

#include <jni.h>
#include <stdio.h>

#include <android-base/logging.h>

#include "base/macros.h"

#include "jni_binder.h"
#include "jvmti_helper.h"
#include "test_env.h"

#include "901-hello-ti-agent/basics.h"
#include "909-attach-agent/attach.h"
#include "936-search-onload/search_onload.h"
#include "1919-vminit-thread-start-timing/vminit.h"

namespace art {

namespace common_redefine {
jint OnLoad(JavaVM* vm, char* options, void* reserved);
}  // namespace common_redefine

namespace common_retransform {
jint OnLoad(JavaVM* vm, char* options, void* reserved);
}  // namespace common_retransform

namespace common_transform {
jint OnLoad(JavaVM* vm, char* options, void* reserved);
}  // namespace common_transform

namespace {

using OnLoad   = jint (*)(JavaVM* vm, char* options, void* reserved);
using OnAttach = jint (*)(JavaVM* vm, char* options, void* reserved);

struct AgentLib {
  const char* name;
  OnLoad load;
  OnAttach attach;
};

// A trivial OnLoad implementation that only initializes the global jvmti_env.
static jint MinimalOnLoad(JavaVM* vm,
                          char* options ATTRIBUTE_UNUSED,
                          void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0) != 0) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  SetStandardCapabilities(jvmti_env);
  return 0;
}

// A list of all non-standard the agents we have for testing. All other agents will use
// MinimalOnLoad.
static AgentLib agents[] = {
  { "901-hello-ti-agent", Test901HelloTi::OnLoad, nullptr },
  { "909-attach-agent", nullptr, Test909AttachAgent::OnAttach },
  { "916-obsolete-jit", common_redefine::OnLoad, nullptr },
  { "921-hello-failure", common_retransform::OnLoad, nullptr },
  { "934-load-transform", common_retransform::OnLoad, nullptr },
  { "935-non-retransformable", common_transform::OnLoad, nullptr },
  { "936-search-onload", Test936SearchOnload::OnLoad, nullptr },
  { "937-hello-retransform-package", common_retransform::OnLoad, nullptr },
  { "938-load-transform-bcp", common_retransform::OnLoad, nullptr },
  { "939-hello-transformation-bcp", common_redefine::OnLoad, nullptr },
  { "941-recursive-obsolete-jit", common_redefine::OnLoad, nullptr },
  { "943-private-recursive-jit", common_redefine::OnLoad, nullptr },
  { "1919-vminit-thread-start-timing", Test1919VMInitThreadStart::OnLoad, nullptr },
};

static AgentLib* FindAgent(char* name) {
  for (AgentLib& l : agents) {
    if (strncmp(l.name, name, strlen(l.name)) == 0) {
      return &l;
    }
  }
  return nullptr;
}

static bool FindAgentNameAndOptions(char* options,
                                    /*out*/char** name,
                                    /*out*/char** other_options) {
  // Name is the first element.
  *name = options;
  char* rest = options;
  // name is the first thing in the options
  while (*rest != '\0' && *rest != ',') {
    rest++;
  }
  if (*rest == ',') {
    *rest = '\0';
    rest++;
  }
  *other_options = rest;
  return true;
}

static void SetIsJVM(const char* options) {
  SetJVM(strncmp(options, "jvm", 3) == 0);
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
  char* remaining_options = nullptr;
  char* name_option = nullptr;
  if (!FindAgentNameAndOptions(options, &name_option, &remaining_options)) {
    printf("Unable to find agent name in options: %s\n", options);
    return -1;
  }

  SetIsJVM(remaining_options);

  AgentLib* lib = FindAgent(name_option);
  OnLoad fn = nullptr;
  if (lib == nullptr) {
    fn = &MinimalOnLoad;
  } else {
    if (lib->load == nullptr) {
      printf("agent: %s does not include an OnLoad method.\n", name_option);
      return -3;
    }
    fn = lib->load;
  }
  return fn(vm, remaining_options, reserved);
}

extern "C" JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
  char* remaining_options = nullptr;
  char* name_option = nullptr;
  if (!FindAgentNameAndOptions(options, &name_option, &remaining_options)) {
    printf("Unable to find agent name in options: %s\n", options);
    return -1;
  }

  AgentLib* lib = FindAgent(name_option);
  if (lib == nullptr) {
    printf("Unable to find agent named: %s, add it to the list in test/ti-agent/common_load.cc\n",
           name_option);
    return -2;
  }
  if (lib->attach == nullptr) {
    printf("agent: %s does not include an OnAttach method.\n", name_option);
    return -3;
  }
  SetIsJVM(remaining_options);
  return lib->attach(vm, remaining_options, reserved);
}

}  // namespace art
