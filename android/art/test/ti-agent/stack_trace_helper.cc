
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

#include "common_helper.h"

#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace common_stack_trace {

extern "C" JNIEXPORT jint JNICALL Java_art_StackTrace_GetStackDepth(
    JNIEnv* env, jclass, jthread thr) {
  jint ret;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetFrameCount(thr, &ret));
  return ret;
}

extern "C" JNIEXPORT jobjectArray Java_art_StackTrace_nativeGetStackTrace(JNIEnv* env,
                                                                          jclass,
                                                                          jthread thr) {
  jint depth;
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/StackTrace$StackFrameData"));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jmethodID constructor = env->GetMethodID(
      klass.get(), "<init>", "(Ljava/lang/Thread;Ljava/lang/reflect/Executable;JI)V");
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetFrameCount(thr, &depth))) {
    return nullptr;
  }
  // Just give some extra space.
  depth += 10;
  jvmtiFrameInfo* frames;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->Allocate(depth * sizeof(jvmtiFrameInfo),
                                          reinterpret_cast<unsigned char**>(&frames)))) {
    return nullptr;
  }
  jint nframes = 0;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetStackTrace(thr, 0, depth, frames, &nframes))) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
    return nullptr;
  }
  jobjectArray frames_array = env->NewObjectArray(nframes, klass.get(), nullptr);
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
    return nullptr;
  }
  for (jint i = 0; i < nframes; i++) {
    jobject jmethod = GetJavaMethod(jvmti_env, env, frames[i].method);
    if (env->ExceptionCheck()) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
      return nullptr;
    }
    jobject frame_obj = env->NewObject(klass.get(),
                                       constructor,
                                       thr,
                                       jmethod,
                                       frames[i].location,
                                       i);
    if (env->ExceptionCheck()) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
      return nullptr;
    }
    env->SetObjectArrayElement(frames_array, i, frame_obj);
    if (env->ExceptionCheck()) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
      return nullptr;
    }
  }
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(frames));
  return frames_array;
}

}  // namespace common_stack_trace
}  // namespace art
