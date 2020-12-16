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

#include "search_onload.h"

#include <inttypes.h>

#include <android-base/macros.h>
#include <android-base/stringprintf.h>

#include "base/macros.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test936SearchOnload {

jint OnLoad(JavaVM* vm,
            char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  SetStandardCapabilities(jvmti_env);

  char* dex_loc = getenv("DEX_LOCATION");
  std::string dex1 = android::base::StringPrintf("%s/936-search-onload.jar", dex_loc);
  std::string dex2 = android::base::StringPrintf("%s/936-search-onload-ex.jar", dex_loc);

  jvmtiError result = jvmti_env->AddToBootstrapClassLoaderSearch(dex1.c_str());
  if (result != JVMTI_ERROR_NONE) {
    printf("Could not add to bootstrap classloader.\n");
    return 1;
  }

  result = jvmti_env->AddToSystemClassLoaderSearch(dex2.c_str());
  if (result != JVMTI_ERROR_NONE) {
    printf("Could not add to system classloader.\n");
    return 1;
  }

  return JNI_OK;
}

}  // namespace Test936SearchOnload
}  // namespace art
