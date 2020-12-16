/*
 * Copyright 2016 The Android Open Source Project
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

#define LOG_TAG "libart_fake"

#include <android/log.h>

#define LOGIT(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
namespace art {
class Dbg {
 public:
  void SuspendVM();
  void ResumeVM();
};

class FaultManager {
 public:
  void EnsureArtActionInFrontOfSignalChain();
};

void Dbg::SuspendVM() {
  LOGIT("Linking to and calling into libart.so internal functions is not supported. "
        "This call to '%s' is being ignored.", __func__);
}
void Dbg::ResumeVM() {
  LOGIT("Linking to and calling into libart.so internal functions is not supported. "
        "This call to '%s' is being ignored.", __func__);
}
void FaultManager::EnsureArtActionInFrontOfSignalChain() {
  LOGIT("Linking to and calling into libart.so internal functions is not supported. "
        "This call to '%s' is being ignored.", __func__);
}
};  // namespace art
