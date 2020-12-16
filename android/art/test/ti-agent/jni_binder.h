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

#ifndef ART_TEST_TI_AGENT_JNI_BINDER_H_
#define ART_TEST_TI_AGENT_JNI_BINDER_H_

#include "jni.h"
#include "jvmti.h"

namespace art {

// Find the given classname. First try the implied classloader, then the system classloader,
// then use JVMTI to find all classloaders.
jclass GetClass(jvmtiEnv* jvmti_env, JNIEnv* env, const char* class_name, jobject class_loader);

// Load the class through JNI. Inspect it, find all native methods. Construct the corresponding
// mangled name, run dlsym and bind the method.
//
// This will abort on failure.
void BindFunctions(jvmtiEnv* jvmti_env,
                   JNIEnv* env,
                   const char* class_name,
                   jobject class_loader = nullptr);

void BindFunctionsOnClass(jvmtiEnv* jvmti_env, JNIEnv* env, jclass klass);

}  // namespace art

#endif  // ART_TEST_TI_AGENT_JNI_BINDER_H_
