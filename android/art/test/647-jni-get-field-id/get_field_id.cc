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

#include "jni.h"

#include "nativehelper/ScopedUtfChars.h"

namespace art {

extern "C" JNIEXPORT jboolean JNICALL Java_Main_getFieldId(JNIEnv* env,
                                                           jclass,
                                                           jclass cls,
                                                           jstring name,
                                                           jstring signature) {
  ScopedUtfChars name_chars(env, name);
  if (name_chars.c_str() == nullptr) {
    return false;
  }
  ScopedUtfChars signature_chars(env, signature);
  if (signature_chars.c_str() == nullptr) {
    return false;
  }
  jfieldID field_id = env->GetFieldID(cls, name_chars.c_str(), signature_chars.c_str());
  if (field_id == nullptr) {
    return false;
  }
  return true;
}

}  // namespace art
