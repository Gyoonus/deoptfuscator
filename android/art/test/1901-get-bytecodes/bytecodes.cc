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

#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "android-base/logging.h"
#include "jni.h"
#include "jvmti.h"

#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1901Bytecodes {

extern "C" JNIEXPORT jbyteArray JNICALL Java_art_Test1901_getBytecodes(JNIEnv* env,
                                                                       jclass,
                                                                       jobject jmethod) {
  jmethodID method = env->FromReflectedMethod(jmethod);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  unsigned char* bytecodes = nullptr;
  jint bytecodes_size = 0;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetBytecodes(method, &bytecodes_size, &bytecodes))) {
    return nullptr;
  }
  jbyteArray out = env->NewByteArray(bytecodes_size);
  if (env->ExceptionCheck()) {
    return nullptr;
  } else if (bytecodes_size == 0) {
    return out;
  }
  jbyte* bytes = env->GetByteArrayElements(out, /* is_copy */ nullptr);
  memcpy(bytes, bytecodes, bytecodes_size);
  env->ReleaseByteArrayElements(out, bytes, 0);
  return out;
}

}  // namespace Test1901Bytecodes
}  // namespace art
