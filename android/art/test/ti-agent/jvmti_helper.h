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

#ifndef ART_TEST_TI_AGENT_JVMTI_HELPER_H_
#define ART_TEST_TI_AGENT_JVMTI_HELPER_H_

#include <memory>
#include <ostream>

#include "jni.h"
#include "jvmti.h"

#include "android-base/logging.h"

namespace art {

// Get a standard set of capabilities for use in tests.
jvmtiCapabilities GetStandardCapabilities();

// Add all the standard capabilities to the given env.
void SetStandardCapabilities(jvmtiEnv* env);

// Add all capabilities to the given env.
// TODO Remove this in the future.
void SetAllCapabilities(jvmtiEnv* env);

// Check whether the given error is NONE. If not, print out the corresponding error message
// and abort.
void CheckJvmtiError(jvmtiEnv* env, jvmtiError error);

// Convert the given error to a RuntimeException with a message derived from the error. Returns
// true on error, false if error is JVMTI_ERROR_NONE.
bool JvmtiErrorToException(JNIEnv* env, jvmtiEnv* jvmtienv, jvmtiError error);

class JvmtiDeleter {
 public:
  JvmtiDeleter() : env_(nullptr) {}
  explicit JvmtiDeleter(jvmtiEnv* env) : env_(env) {}

  JvmtiDeleter(JvmtiDeleter&) = default;
  JvmtiDeleter(JvmtiDeleter&&) = default;
  JvmtiDeleter& operator=(const JvmtiDeleter&) = default;

  void operator()(unsigned char* ptr) const {
    CHECK(env_ != nullptr);
    jvmtiError ret = env_->Deallocate(ptr);
    CheckJvmtiError(env_, ret);
  }

 private:
  mutable jvmtiEnv* env_;
};

using JvmtiUniquePtr = std::unique_ptr<unsigned char, JvmtiDeleter>;

template <typename T>
static inline JvmtiUniquePtr MakeJvmtiUniquePtr(jvmtiEnv* env, T* mem) {
  return JvmtiUniquePtr(reinterpret_cast<unsigned char*>(mem), JvmtiDeleter(env));
}

template <typename T>
static inline jvmtiError Deallocate(jvmtiEnv* env, T* mem) {
  return env->Deallocate(reinterpret_cast<unsigned char*>(mem));
}

// To print jvmtiError. Does not rely on GetErrorName, so is an approximation.
std::ostream& operator<<(std::ostream& os, const jvmtiError& rhs);

}  // namespace art

#endif  // ART_TEST_TI_AGENT_JVMTI_HELPER_H_
