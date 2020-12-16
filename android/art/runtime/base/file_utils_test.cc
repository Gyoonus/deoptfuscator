/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "base/file_utils.h"

#include <libgen.h>
#include <stdlib.h>

#include "base/stl_util.h"
#include "common_runtime_test.h"

namespace art {

class FileUtilsTest : public CommonRuntimeTest {};

TEST_F(FileUtilsTest, GetDalvikCacheFilename) {
  std::string name;
  std::string error;

  EXPECT_TRUE(GetDalvikCacheFilename("/system/app/Foo.apk", "/foo", &name, &error)) << error;
  EXPECT_EQ("/foo/system@app@Foo.apk@classes.dex", name);

  EXPECT_TRUE(GetDalvikCacheFilename("/data/app/foo-1.apk", "/foo", &name, &error)) << error;
  EXPECT_EQ("/foo/data@app@foo-1.apk@classes.dex", name);

  EXPECT_TRUE(GetDalvikCacheFilename("/system/framework/core.jar", "/foo", &name, &error)) << error;
  EXPECT_EQ("/foo/system@framework@core.jar@classes.dex", name);

  EXPECT_TRUE(GetDalvikCacheFilename("/system/framework/boot.art", "/foo", &name, &error)) << error;
  EXPECT_EQ("/foo/system@framework@boot.art", name);

  EXPECT_TRUE(GetDalvikCacheFilename("/system/framework/boot.oat", "/foo", &name, &error)) << error;
  EXPECT_EQ("/foo/system@framework@boot.oat", name);
}

TEST_F(FileUtilsTest, GetDalvikCache) {
  EXPECT_STREQ("", GetDalvikCache("should-not-exist123").c_str());

  EXPECT_STREQ((android_data_ + "/dalvik-cache/.").c_str(), GetDalvikCache(".").c_str());
}


TEST_F(FileUtilsTest, GetSystemImageFilename) {
  EXPECT_STREQ("/system/framework/arm/boot.art",
               GetSystemImageFilename("/system/framework/boot.art", InstructionSet::kArm).c_str());
}

TEST_F(FileUtilsTest, GetAndroidRootSafe) {
  std::string error_msg;

  // We don't expect null returns for most cases, so don't check and let std::string crash.

  // CommonRuntimeTest sets ANDROID_ROOT, so expect this to be the same.
  std::string android_root = GetAndroidRootSafe(&error_msg);
  std::string android_root_env = getenv("ANDROID_ROOT");
  EXPECT_EQ(android_root, android_root_env);

  // Set ANDROID_ROOT to something else (but the directory must exist). So use dirname.
  char* root_dup = strdup(android_root_env.c_str());
  char* dir = dirname(root_dup);
  ASSERT_EQ(0, setenv("ANDROID_ROOT", dir, 1 /* overwrite */));
  std::string android_root2 = GetAndroidRootSafe(&error_msg);
  EXPECT_STREQ(dir, android_root2.c_str());
  free(root_dup);

  // Set a bogus value for ANDROID_ROOT. This should be an error.
  ASSERT_EQ(0, setenv("ANDROID_ROOT", "/this/is/obviously/bogus", 1 /* overwrite */));
  EXPECT_TRUE(GetAndroidRootSafe(&error_msg) == nullptr);

  // Unset ANDROID_ROOT and see that it still returns something (as libart code is running).
  ASSERT_EQ(0, unsetenv("ANDROID_ROOT"));
  std::string android_root3 = GetAndroidRootSafe(&error_msg);
  // This should be the same as the other root (modulo realpath), otherwise the test setup is
  // broken.
  UniqueCPtr<char> real_root(realpath(android_root.c_str(), nullptr));
  UniqueCPtr<char> real_root3(realpath(android_root3.c_str(), nullptr));
  EXPECT_STREQ(real_root.get(), real_root3.get());


  // Reset ANDROID_ROOT, as other things may depend on it.
  ASSERT_EQ(0, setenv("ANDROID_ROOT", android_root_env.c_str(), 1 /* overwrite */));
}

}  // namespace art
