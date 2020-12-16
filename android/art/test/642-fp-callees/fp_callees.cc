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

#include <android-base/logging.h>

#include "base/casts.h"
#include "jni.h"

namespace art {

// Make the array volatile, which is apparently making the C compiler
// use FP registers in the method below.
volatile double array[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0 };

extern "C" JNIEXPORT void JNICALL Java_Main_holdFpTemporaries(JNIEnv* env, jclass cls) {
  jmethodID mid = env->GetStaticMethodID(cls, "caller", "(IIJ)V");
  CHECK(mid != nullptr);
  // Load values from the arrays, which will be loaded in callee-save FP registers.
  double a = array[0];
  double b = array[1];
  double c = array[2];
  double d = array[3];
  double e = array[4];
  double f = array[5];
  double g = array[6];
  double h = array[7];
  double i = array[8];
  double j = array[9];
  double k = array[10];
  double l = array[11];
  env->CallStaticVoidMethod(cls, mid, 1, 1, 1L);
  // Load it in a temporary to please C compiler with bit_cast.
  double temp = array[0];
  CHECK_EQ(bit_cast<int64_t>(a), bit_cast<int64_t>(temp));
  temp = array[1];
  CHECK_EQ(bit_cast<int64_t>(b), bit_cast<int64_t>(temp));
  temp = array[2];
  CHECK_EQ(bit_cast<int64_t>(c), bit_cast<int64_t>(temp));
  temp = array[3];
  CHECK_EQ(bit_cast<int64_t>(d), bit_cast<int64_t>(temp));
  temp = array[4];
  CHECK_EQ(bit_cast<int64_t>(e), bit_cast<int64_t>(temp));
  temp = array[5];
  CHECK_EQ(bit_cast<int64_t>(f), bit_cast<int64_t>(temp));
  temp = array[6];
  CHECK_EQ(bit_cast<int64_t>(g), bit_cast<int64_t>(temp));
  temp = array[7];
  CHECK_EQ(bit_cast<int64_t>(h), bit_cast<int64_t>(temp));
  temp = array[8];
  CHECK_EQ(bit_cast<int64_t>(i), bit_cast<int64_t>(temp));
  temp = array[9];
  CHECK_EQ(bit_cast<int64_t>(j), bit_cast<int64_t>(temp));
  temp = array[10];
  CHECK_EQ(bit_cast<int64_t>(k), bit_cast<int64_t>(temp));
  temp = array[11];
  CHECK_EQ(bit_cast<int64_t>(l), bit_cast<int64_t>(temp));
}

}  // namespace art
