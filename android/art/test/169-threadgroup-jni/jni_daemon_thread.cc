/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <nativehelper/scoped_local_ref.h>
#include <pthread.h>

#include <android-base/logging.h>

namespace art {

static JavaVM* vm = nullptr;

static void* Runner(void* arg) {
  CHECK(vm != nullptr);

  jobject thread_group = reinterpret_cast<jobject>(arg);
  JNIEnv* env = nullptr;
  JavaVMAttachArgs args = { JNI_VERSION_1_6, __FUNCTION__, thread_group };
  int attach_result = vm->AttachCurrentThread(&env, &args);
  CHECK_EQ(attach_result, 0);

  {
    ScopedLocalRef<jclass> klass(env, env->FindClass("Main"));
    CHECK(klass != nullptr);

    jmethodID id = env->GetStaticMethodID(klass.get(), "runFromNative", "()V");
    CHECK(id != nullptr);

    env->CallStaticVoidMethod(klass.get(), id);
  }

  int detach_result = vm->DetachCurrentThread();
  CHECK_EQ(detach_result, 0);
  return nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_Main_testNativeThread(
    JNIEnv* env, jclass, jobject thread_group) {
  CHECK_EQ(env->GetJavaVM(&vm), 0);
  jobject global_thread_group = env->NewGlobalRef(thread_group);

  pthread_t pthread;
  int pthread_create_result = pthread_create(&pthread, nullptr, Runner, global_thread_group);
  CHECK_EQ(pthread_create_result, 0);
  int pthread_join_result = pthread_join(pthread, nullptr);
  CHECK_EQ(pthread_join_result, 0);

  env->DeleteGlobalRef(global_thread_group);
}

}  // namespace art
