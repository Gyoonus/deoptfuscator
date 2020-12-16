/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ART_TEST_TI_AGENT_SCOPED_LOCAL_REF_H_
#define ART_TEST_TI_AGENT_SCOPED_LOCAL_REF_H_

#include "jni.h"

#include <stddef.h>

#include "android-base/macros.h"

namespace art {

template<typename T>
class ScopedLocalRef {
 public:
  ScopedLocalRef(JNIEnv* env, T localRef) : mEnv(env), mLocalRef(localRef) {
  }

  ~ScopedLocalRef() {
    reset();
  }

  void reset(T ptr = nullptr) {
    if (ptr != mLocalRef) {
      if (mLocalRef != nullptr) {
        mEnv->DeleteLocalRef(mLocalRef);
      }
      mLocalRef = ptr;
    }
  }

  T release() WARN_UNUSED {
    T localRef = mLocalRef;
    mLocalRef = nullptr;
    return localRef;
  }

  T get() const {
    return mLocalRef;
  }

 private:
  JNIEnv* const mEnv;
  T mLocalRef;

  DISALLOW_COPY_AND_ASSIGN(ScopedLocalRef);
};

}  // namespace art

#endif  // ART_TEST_TI_AGENT_SCOPED_LOCAL_REF_H_
