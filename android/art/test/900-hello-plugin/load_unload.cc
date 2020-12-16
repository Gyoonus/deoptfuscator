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

#include <jni.h>
#include <stdio.h>

#include <android-base/logging.h>
#include <android-base/macros.h>

#include "art_method-inl.h"
#include "java_vm_ext.h"
#include "runtime.h"

namespace art {

constexpr jint TEST_900_ENV_VERSION_NUMBER = 0x900FFFFF;
constexpr uintptr_t ENV_VALUE = 900;

// Allow this library to be used as a plugin too so we can test the stack.
static jint GetEnvHandler(JavaVMExt* vm ATTRIBUTE_UNUSED, void** new_env, jint version) {
  printf("%s called in test 900\n", __func__);
  if (version != TEST_900_ENV_VERSION_NUMBER) {
    return JNI_EVERSION;
  }
  printf("GetEnvHandler called with version 0x%x\n", version);
  *new_env = reinterpret_cast<void*>(ENV_VALUE);
  return JNI_OK;
}

extern "C" bool ArtPlugin_Initialize() {
  printf("%s called in test 900\n", __func__);
  Runtime::Current()->GetJavaVM()->AddEnvironmentHook(GetEnvHandler);
  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  printf("%s called in test 900\n", __func__);
  return true;
}

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm,
                                               char* options,
                                               void* reserved ATTRIBUTE_UNUSED) {
  printf("Agent_OnLoad called with options \"%s\"\n", options);
  if (strcmp("test_900_round_2", options) == 0) {
    return 0;
  }
  uintptr_t env = 0;
  jint res = vm->GetEnv(reinterpret_cast<void**>(&env), TEST_900_ENV_VERSION_NUMBER);
  if (res != JNI_OK) {
    printf("GetEnv(TEST_900_ENV_VERSION_NUMBER) returned non-zero\n");
  }
  printf("GetEnv returned '%" PRIdPTR "' environment!\n", env);
  return 0;
}

extern "C" JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* vm ATTRIBUTE_UNUSED) {
  printf("Agent_OnUnload called\n");
}

}  // namespace art
