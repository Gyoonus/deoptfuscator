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

#include "base/time_utils.h"
#include "jni.h"
#include "runtime.h"
#include "thread_list.h"

namespace art {

extern "C" JNIEXPORT void JNICALL Java_Main_suspendAndResume(JNIEnv*, jclass) {
  static constexpr size_t kInitialSleepUS = 100 * 1000;  // 100ms.
  usleep(kInitialSleepUS);  // Leave some time for threads to get in here before we start suspending.
  enum Operation {
    kOPSuspendAll,
    kOPDumpStack,
    kOPSuspendAllDumpStack,
    // Total number of operations.
    kOPNumber,
  };
  const uint64_t start_time = NanoTime();
  size_t iterations = 0;
  // Run for a fixed period of 10 seconds.
  while (NanoTime() - start_time < MsToNs(10 * 1000)) {
    switch (static_cast<Operation>(iterations % kOPNumber)) {
      case kOPSuspendAll: {
        ScopedSuspendAll ssa(__FUNCTION__);
        usleep(500);
        break;
      }
      case kOPDumpStack: {
        Runtime::Current()->GetThreadList()->Dump(LOG_STREAM(INFO));
        usleep(500);
        break;
      }
      case kOPSuspendAllDumpStack: {
        // Not yet supported.
        if ((false)) {
          ScopedSuspendAll ssa(__FUNCTION__);
          Runtime::Current()->GetThreadList()->Dump(LOG_STREAM(INFO));
        }
        break;
      }
      case kOPNumber:
        break;
    }
    ++iterations;
  }
  LOG(INFO) << "Did " << iterations << " iterations";
}

}  // namespace art
