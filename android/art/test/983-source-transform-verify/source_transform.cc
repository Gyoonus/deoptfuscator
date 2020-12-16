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

#include "source_transform.h"

#include "jni.h"

#include "android-base/stringprintf.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test983SourceTransformVerify {

constexpr bool kSkipInitialLoad = true;

static void Println(JNIEnv* env, const char* msg) {
  ScopedLocalRef<jclass> test_klass(env, env->FindClass("art/Test983"));
  jmethodID println_method = env->GetStaticMethodID(test_klass.get(),
                                                    "doPrintln",
                                                    "(Ljava/lang/String;)V");
  ScopedLocalRef<jstring> data(env, env->NewStringUTF(msg));
  env->CallStaticVoidMethod(test_klass.get(), println_method, data.get());
}

// The hook we are using.
void JNICALL CheckDexFileHook(jvmtiEnv* jvmti_env ATTRIBUTE_UNUSED,
                              JNIEnv* env,
                              jclass class_being_redefined,
                              jobject loader ATTRIBUTE_UNUSED,
                              const char* name,
                              jobject protection_domain ATTRIBUTE_UNUSED,
                              jint class_data_len,
                              const unsigned char* class_data,
                              jint* new_class_data_len ATTRIBUTE_UNUSED,
                              unsigned char** new_class_data ATTRIBUTE_UNUSED) {
  if (kSkipInitialLoad && class_being_redefined == nullptr) {
    // Something got loaded concurrently. Just ignore it for now. To make sure the test is
    // repeatable we only care about things that come from RetransformClasses.
    return;
  }
  Println(env, android::base::StringPrintf("Dex file hook for %s", name).c_str());
  if (IsJVM()) {
    return;
  }

  VerifyClassData(class_data_len, class_data);
}

// Get all capabilities except those related to retransformation.
extern "C" JNIEXPORT void JNICALL Java_art_Test983_setupLoadHook(JNIEnv* env, jclass) {
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.ClassFileLoadHook = CheckDexFileHook;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)));
}

}  // namespace Test983SourceTransformVerify
}  // namespace art
