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

#ifndef ART_TEST_TI_AGENT_COMMON_HELPER_H_
#define ART_TEST_TI_AGENT_COMMON_HELPER_H_

#include "jni.h"
#include "jvmti.h"

namespace art {

// Taken from art/runtime/dex/modifiers.h
static constexpr uint32_t kAccStatic =       0x0008;  // field, method, ic

jobject GetJavaField(jvmtiEnv* jvmti, JNIEnv* env, jclass field_klass, jfieldID f);
jobject GetJavaMethod(jvmtiEnv* jvmti, JNIEnv* env, jmethodID m);
jobject GetJavaValueByType(JNIEnv* env, char type, jvalue value);
jobject GetJavaValue(jvmtiEnv* jvmtienv, JNIEnv* env, jmethodID m, jvalue value);

}  // namespace art

#endif  // ART_TEST_TI_AGENT_COMMON_HELPER_H_
