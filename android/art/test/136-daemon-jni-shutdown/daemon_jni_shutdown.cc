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

#include <dlfcn.h>
#include <iostream>

#include "base/casts.h"
#include "base/macros.h"
#include "java_vm_ext.h"
#include "jni_env_ext.h"
#include "thread-current-inl.h"

namespace art {
namespace {

static volatile std::atomic<bool> vm_was_shutdown(false);
static const int kThreadCount = 4;

static std::atomic<int> barrier_count(kThreadCount + 1);

static void JniThreadBarrierWait() {
  barrier_count--;
  while (barrier_count.load() != 0) {
    usleep(1000);
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_waitAndCallIntoJniEnv(JNIEnv* env, jclass) {
  // Wait for all threads to enter JNI together.
  JniThreadBarrierWait();
  // Wait until the runtime is shutdown.
  while (!vm_was_shutdown.load()) {
    usleep(1000);
  }
  std::cout << "About to call exception check\n";
  env->ExceptionCheck();
  LOG(ERROR) << "Should not be reached!";
}

// NO_RETURN does not work with extern "C" for target builds.
extern "C" JNIEXPORT void JNICALL Java_Main_destroyJavaVMAndExit(JNIEnv* env, jclass) {
  // Wait for all threads to enter JNI together.
  JniThreadBarrierWait();
  // Fake up the managed stack so we can detach.
  Thread* const self = Thread::Current();
  self->SetTopOfStack(nullptr);
  self->SetTopOfShadowStack(nullptr);
  JavaVM* vm = down_cast<JNIEnvExt*>(env)->GetVm();
  vm->DetachCurrentThread();
  // Open ourself again to make sure the native library does not get unloaded from
  // underneath us due to DestroyJavaVM. b/28406866
  void* handle = dlopen(kIsDebugBuild ? "libarttestd.so" : "libarttest.so", RTLD_NOW);
  CHECK(handle != nullptr);
  vm->DestroyJavaVM();
  vm_was_shutdown.store(true);
  // Give threads some time to get stuck in ExceptionCheck.
  usleep(1000000);
  if (env != nullptr) {
    // Use env != nullptr to trick noreturn.
    exit(0);
  }
}

}  // namespace
}  // namespace art
