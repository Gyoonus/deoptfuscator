/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "jdwp.h"

#include "gtest/gtest.h"

namespace art {
namespace JDWP {

TEST(JdwpOptionsTest, Options) {
  {
    /*
     * "Example: -Xrunjdwp:transport=dt_socket,address=8000,server=y\n"
     */
    JDWP::JdwpOptions opt = JDWP::JdwpOptions();
    const char *opt_args = "transport=dt_socket,address=8000,server=y";

    EXPECT_TRUE(ParseJdwpOptions(opt_args, &opt));
    EXPECT_EQ(opt.transport, JdwpTransportType::kJdwpTransportSocket);
    EXPECT_EQ(opt.port, 8000u);
    EXPECT_EQ(opt.server, true);
    EXPECT_EQ(opt.suspend, false);
  }

  {
    /*
     * Example: transport=dt_socket,address=localhost:6500,server=n
     */
    JDWP::JdwpOptions opt = JDWP::JdwpOptions();
    const char *opt_args = "transport=dt_socket,address=localhost:6500,server=y";

    EXPECT_TRUE(ParseJdwpOptions(opt_args, &opt));
    EXPECT_EQ(opt.transport, JdwpTransportType::kJdwpTransportSocket);
    EXPECT_EQ(opt.port, 6500u);
    EXPECT_EQ(opt.host, "localhost");
    EXPECT_EQ(opt.server, true);
    EXPECT_EQ(opt.suspend, false);
  }

  {
    /*
     * Example: transport=dt_android_adb,server=n,suspend=y;
     */
    JDWP::JdwpOptions opt = JDWP::JdwpOptions();
    const char *opt_args = "transport=dt_android_adb,server=y";

    EXPECT_TRUE(ParseJdwpOptions(opt_args, &opt));
    EXPECT_EQ(opt.transport, JdwpTransportType::kJdwpTransportAndroidAdb);
    EXPECT_EQ(opt.port, 0xFFFF);
    EXPECT_EQ(opt.host, "");
    EXPECT_EQ(opt.server, true);
    EXPECT_EQ(opt.suspend, false);
  }

  /*
   * Test failures
  */
  JDWP::JdwpOptions opt = JDWP::JdwpOptions();
  EXPECT_FALSE(ParseJdwpOptions("help", &opt));
  EXPECT_FALSE(ParseJdwpOptions("blabla", &opt));
  EXPECT_FALSE(ParseJdwpOptions("transport=dt_android_adb,server=n", &opt));
}

}  // namespace JDWP
}  // namespace art
