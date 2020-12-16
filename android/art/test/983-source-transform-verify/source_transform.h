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

#ifndef ART_TEST_983_SOURCE_TRANSFORM_VERIFY_SOURCE_TRANSFORM_H_
#define ART_TEST_983_SOURCE_TRANSFORM_VERIFY_SOURCE_TRANSFORM_H_

#include <jni.h>

namespace art {
namespace Test983SourceTransformVerify {

void VerifyClassData(jint class_data_len, const unsigned char* class_data);

}  // namespace Test983SourceTransformVerify
}  // namespace art

#endif  // ART_TEST_983_SOURCE_TRANSFORM_VERIFY_SOURCE_TRANSFORM_H_
