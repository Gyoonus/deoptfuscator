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

#include <gtest/gtest.h>

#include "android-base/stringprintf.h"

#include "dexopt_test.h"
#include "noop_compiler_callbacks.h"

namespace art {
namespace gc {
namespace space {

TEST_F(DexoptTest, ValidateOatFile) {
  std::string dex1 = GetScratchDir() + "/Dex1.jar";
  std::string multidex1 = GetScratchDir() + "/MultiDex1.jar";
  std::string dex2 = GetScratchDir() + "/Dex2.jar";
  std::string oat_location = GetScratchDir() + "/Oat.oat";

  Copy(GetDexSrc1(), dex1);
  Copy(GetMultiDexSrc1(), multidex1);
  Copy(GetDexSrc2(), dex2);

  std::string error_msg;
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex1);
  args.push_back("--dex-file=" + multidex1);
  args.push_back("--dex-file=" + dex2);
  args.push_back("--oat-file=" + oat_location);
  ASSERT_TRUE(OatFileAssistant::Dex2Oat(args, &error_msg)) << error_msg;

  std::unique_ptr<OatFile> oat(OatFile::Open(/* zip_fd */ -1,
                                             oat_location.c_str(),
                                             oat_location.c_str(),
                                             nullptr,
                                             nullptr,
                                             false,
                                             /*low_4gb*/false,
                                             nullptr,
                                             &error_msg));
  ASSERT_TRUE(oat != nullptr) << error_msg;

  // Originally all the dex checksums should be up to date.
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Invalidate the dex1 checksum.
  Copy(GetDexSrc2(), dex1);
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // Restore the dex1 checksum.
  Copy(GetDexSrc1(), dex1);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Invalidate the non-main multidex checksum.
  Copy(GetMultiDexSrc2(), multidex1);
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // Restore the multidex checksum.
  Copy(GetMultiDexSrc1(), multidex1);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Invalidate the dex2 checksum.
  Copy(GetDexSrc1(), dex2);
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // restore the dex2 checksum.
  Copy(GetDexSrc2(), dex2);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Replace the multidex file with a non-multidex file.
  Copy(GetDexSrc1(), multidex1);
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // Restore the multidex file
  Copy(GetMultiDexSrc1(), multidex1);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Replace dex1 with a multidex file.
  Copy(GetMultiDexSrc1(), dex1);
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // Restore the dex1 file.
  Copy(GetDexSrc1(), dex1);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Remove the dex2 file.
  EXPECT_EQ(0, unlink(dex2.c_str()));
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));

  // Restore the dex2 file.
  Copy(GetDexSrc2(), dex2);
  EXPECT_TRUE(ImageSpace::ValidateOatFile(*oat, &error_msg)) << error_msg;

  // Remove the multidex file.
  EXPECT_EQ(0, unlink(multidex1.c_str()));
  EXPECT_FALSE(ImageSpace::ValidateOatFile(*oat, &error_msg));
}

template <bool kImage, bool kRelocate, bool kPatchoat, bool kImageDex2oat>
class ImageSpaceLoadingTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions* options) OVERRIDE {
    if (kImage) {
      options->emplace_back(android::base::StringPrintf("-Ximage:%s", GetCoreArtLocation().c_str()),
                            nullptr);
    }
    options->emplace_back(kRelocate ? "-Xrelocate" : "-Xnorelocate", nullptr);
    if (!kPatchoat) {
      options->emplace_back("-Xpatchoat:false", nullptr);
    }
    options->emplace_back(kImageDex2oat ? "-Ximage-dex2oat" : "-Xnoimage-dex2oat", nullptr);

    // We want to test the relocation behavior of ImageSpace. As such, don't pretend we're a
    // compiler.
    callbacks_.reset();
  }
};

using ImageSpacePatchoatTest = ImageSpaceLoadingTest<true, true, true, true>;
TEST_F(ImageSpacePatchoatTest, Test) {
  EXPECT_FALSE(Runtime::Current()->GetHeap()->GetBootImageSpaces().empty());
}

using ImageSpaceDex2oatTest = ImageSpaceLoadingTest<false, true, false, true>;
TEST_F(ImageSpaceDex2oatTest, Test) {
  EXPECT_FALSE(Runtime::Current()->GetHeap()->GetBootImageSpaces().empty());
}

using ImageSpaceNoDex2oatNoPatchoatTest = ImageSpaceLoadingTest<true, true, false, false>;
TEST_F(ImageSpaceNoDex2oatNoPatchoatTest, Test) {
  EXPECT_TRUE(Runtime::Current()->GetHeap()->GetBootImageSpaces().empty());
}

using ImageSpaceNoRelocateNoDex2oatNoPatchoatTest = ImageSpaceLoadingTest<true, false, false, false>;
TEST_F(ImageSpaceNoRelocateNoDex2oatNoPatchoatTest, Test) {
  EXPECT_FALSE(Runtime::Current()->GetHeap()->GetBootImageSpaces().empty());
}

}  // namespace space
}  // namespace gc
}  // namespace art
