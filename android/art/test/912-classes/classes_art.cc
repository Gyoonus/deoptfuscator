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

#include <stdio.h>

#include <mutex>
#include <vector>

#include "android-base/macros.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"
#include "test_env.h"

namespace art {
namespace Test912ArtClasses {

static void EnableEvents(JNIEnv* env,
                         jboolean enable,
                         decltype(jvmtiEventCallbacks().ClassLoad) class_load,
                         decltype(jvmtiEventCallbacks().ClassPrepare) class_prepare) {
  if (enable == JNI_FALSE) {
    jvmtiError ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                         JVMTI_EVENT_CLASS_LOAD,
                                                         nullptr);
    if (JvmtiErrorToException(env, jvmti_env, ret)) {
      return;
    }
    ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                              JVMTI_EVENT_CLASS_PREPARE,
                                              nullptr);
    JvmtiErrorToException(env, jvmti_env, ret);
    return;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.ClassLoad = class_load;
  callbacks.ClassPrepare = class_prepare;
  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }

  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_CLASS_LOAD,
                                            nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_CLASS_PREPARE,
                                            nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
}

struct ClassLoadSeen {
  static void JNICALL ClassLoadSeenCallback(jvmtiEnv* jenv ATTRIBUTE_UNUSED,
                                            JNIEnv* jni_env ATTRIBUTE_UNUSED,
                                            jthread thread ATTRIBUTE_UNUSED,
                                            jclass klass ATTRIBUTE_UNUSED) {
    saw_event = true;
  }

  static bool saw_event;
};
bool ClassLoadSeen::saw_event = false;

extern "C" JNIEXPORT void JNICALL Java_art_Test912Art_enableClassLoadSeenEvents(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jboolean b) {
  EnableEvents(env, b, ClassLoadSeen::ClassLoadSeenCallback, nullptr);
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912Art_hadLoadEvent(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass Main_klass ATTRIBUTE_UNUSED) {
  return ClassLoadSeen::saw_event ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912Art_isLoadedClass(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jstring class_name) {
  ScopedUtfChars name(env, class_name);

  jint class_count;
  jclass* classes;
  jvmtiError res = jvmti_env->GetLoadedClasses(&class_count, &classes);
  if (JvmtiErrorToException(env, jvmti_env, res)) {
    return JNI_FALSE;
  }

  bool found = false;
  for (jint i = 0; !found && i < class_count; ++i) {
    char* sig;
    jvmtiError res2 = jvmti_env->GetClassSignature(classes[i], &sig, nullptr);
    if (JvmtiErrorToException(env, jvmti_env, res2)) {
      return JNI_FALSE;
    }

    found = strcmp(name.c_str(), sig) == 0;

    CheckJvmtiError(jvmti_env, jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(sig)));
  }

  CheckJvmtiError(jvmti_env, jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(classes)));

  return found;
}

// We use the implementations from runtime_state.cc.

extern "C" JNIEXPORT void JNICALL Java_Main_ensureJitCompiled(JNIEnv* env,
                                                             jclass,
                                                             jclass cls,
                                                             jstring method_name);
extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasJit(JNIEnv*, jclass);

extern "C" JNIEXPORT void JNICALL Java_art_Test912Art_ensureJitCompiled(
    JNIEnv* env, jclass klass, jclass test_class, jstring name) {
  Java_Main_ensureJitCompiled(env, klass, test_class, name);
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test912Art_hasJit(JNIEnv* env, jclass klass) {
  return Java_Main_hasJit(env, klass);
}

}  // namespace Test912ArtClasses
}  // namespace art
