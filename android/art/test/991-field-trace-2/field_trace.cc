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

#include <stdio.h>

#include "android-base/macros.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test991FieldTrace {

extern "C" JNIEXPORT void JNICALL Java_art_Test991_doNativeReadWrite(
    JNIEnv* env, jclass klass, jobject testclass) {
  CHECK(testclass != nullptr);
  ScopedLocalRef<jclass> testclass_klass(env, env->GetObjectClass(testclass));
  jmethodID notifyMethod = env->GetStaticMethodID(klass, "doPrintNativeNotification", "(I)V");
  if (env->ExceptionCheck()) {
    return;
  }
  jfieldID xyz_field = env->GetFieldID(testclass_klass.get(), "xyz", "I");
  if (env->ExceptionCheck()) {
    return;
  }
  jint val = env->GetIntField(testclass, xyz_field);
  if (env->ExceptionCheck()) {
    return;
  }
  env->CallStaticVoidMethod(klass, notifyMethod, val);
  if (env->ExceptionCheck()) {
    return;
  }
  val += 1;
  env->SetIntField(testclass, xyz_field, val);
}

}  // namespace Test991FieldTrace
}  // namespace art

