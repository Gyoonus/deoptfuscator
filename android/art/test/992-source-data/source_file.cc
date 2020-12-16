/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <inttypes.h>

#include <cstdio>
#include <memory>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_binder.h"
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test992SourceFile {

extern "C" JNIEXPORT
jstring JNICALL Java_art_Test992_getSourceFileName(JNIEnv* env,
                                                   jclass klass ATTRIBUTE_UNUSED,
                                                   jclass target) {
  char* file = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetSourceFileName(target, &file))) {
    return nullptr;
  }
  jstring ret = env->NewStringUTF(file);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(file));
  return ret;
}

extern "C" JNIEXPORT
jstring JNICALL Java_art_Test992_getSourceDebugExtension(JNIEnv* env,
                                                         jclass klass ATTRIBUTE_UNUSED,
                                                         jclass target) {
  char* ext = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetSourceDebugExtension(target, &ext))) {
    return nullptr;
  }
  jstring ret = env->NewStringUTF(ext);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(ext));
  return ret;
}

}  // namespace Test992SourceFile
}  // namespace art

