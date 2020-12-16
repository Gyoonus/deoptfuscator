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

#include "logging.h"

#include <type_traits>

#include "android-base/logging.h"
#include "base/bit_utils.h"
#include "base/macros.h"
#include "common_runtime_test.h"
#include "runtime_debug.h"

namespace art {

static void SimpleAborter(const char* msg) {
  LOG(FATAL_WITHOUT_ABORT) << msg;
  _exit(1);
}

class LoggingTest : public CommonRuntimeTest {
 protected:
  void PostRuntimeCreate() OVERRIDE {
    // In our abort tests we really don't want the runtime to create a real dump.
    android::base::SetAborter(SimpleAborter);
  }
};

#ifdef NDEBUG
#error Unexpected NDEBUG
#endif

class TestClass {
 public:
  DECLARE_RUNTIME_DEBUG_FLAG(kFlag);
};
DEFINE_RUNTIME_DEBUG_FLAG(TestClass, kFlag);

TEST_F(LoggingTest, DECL_DEF) {
  SetRuntimeDebugFlagsEnabled(true);
  EXPECT_TRUE(TestClass::kFlag);

  SetRuntimeDebugFlagsEnabled(false);
  EXPECT_FALSE(TestClass::kFlag);
}

}  // namespace art
