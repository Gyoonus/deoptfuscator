/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "instruction_set_features_mips.h"

#include <gtest/gtest.h>

namespace art {

TEST(MipsInstructionSetFeaturesTest, MipsFeaturesFromDefaultVariant) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "default", &error_msg));
  ASSERT_TRUE(mips_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips_features->GetInstructionSet(), InstructionSet::kMips);
  EXPECT_TRUE(mips_features->Equals(mips_features.get()));
  EXPECT_STREQ("fpu32,mips2,-msa", mips_features->GetFeatureString().c_str());
  EXPECT_EQ(mips_features->AsBitmap(), 3U);
}

TEST(MipsInstructionSetFeaturesTest, MipsFeaturesFromR1Variant) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips32r1_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r1", &error_msg));
  ASSERT_TRUE(mips32r1_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips32r1_features->GetInstructionSet(), InstructionSet::kMips);
  EXPECT_TRUE(mips32r1_features->Equals(mips32r1_features.get()));
  EXPECT_STREQ("fpu32,-mips2,-msa", mips32r1_features->GetFeatureString().c_str());
  EXPECT_EQ(mips32r1_features->AsBitmap(), 1U);

  std::unique_ptr<const InstructionSetFeatures> mips_default_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "default", &error_msg));
  ASSERT_TRUE(mips_default_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r1_features->Equals(mips_default_features.get()));
}

TEST(MipsInstructionSetFeaturesTest, MipsFeaturesFromR2Variant) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips32r2_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r2", &error_msg));
  ASSERT_TRUE(mips32r2_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips32r2_features->GetInstructionSet(), InstructionSet::kMips);
  EXPECT_TRUE(mips32r2_features->Equals(mips32r2_features.get()));
  EXPECT_STREQ("fpu32,mips2,-msa", mips32r2_features->GetFeatureString().c_str());
  EXPECT_EQ(mips32r2_features->AsBitmap(), 3U);

  std::unique_ptr<const InstructionSetFeatures> mips_default_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "default", &error_msg));
  ASSERT_TRUE(mips_default_features.get() != nullptr) << error_msg;
  EXPECT_TRUE(mips32r2_features->Equals(mips_default_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r1_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r1", &error_msg));
  ASSERT_TRUE(mips32r1_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r2_features->Equals(mips32r1_features.get()));
}

TEST(MipsInstructionSetFeaturesTest, MipsFeaturesFromR5Variant) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips32r5_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r5", &error_msg));
  ASSERT_TRUE(mips32r5_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips32r5_features->GetInstructionSet(), InstructionSet::kMips);
  EXPECT_TRUE(mips32r5_features->Equals(mips32r5_features.get()));
  EXPECT_STREQ("-fpu32,mips2,msa", mips32r5_features->GetFeatureString().c_str());
  EXPECT_EQ(mips32r5_features->AsBitmap(), 10U);

  std::unique_ptr<const InstructionSetFeatures> mips_default_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "default", &error_msg));
  ASSERT_TRUE(mips_default_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r5_features->Equals(mips_default_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r1_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r1", &error_msg));
  ASSERT_TRUE(mips32r1_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r5_features->Equals(mips32r1_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r2_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r2", &error_msg));
  ASSERT_TRUE(mips32r2_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r5_features->Equals(mips32r2_features.get()));
}

TEST(MipsInstructionSetFeaturesTest, MipsFeaturesFromR6Variant) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> mips32r6_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r6", &error_msg));
  ASSERT_TRUE(mips32r6_features.get() != nullptr) << error_msg;
  EXPECT_EQ(mips32r6_features->GetInstructionSet(), InstructionSet::kMips);
  EXPECT_TRUE(mips32r6_features->Equals(mips32r6_features.get()));
  EXPECT_STREQ("-fpu32,mips2,r6,msa", mips32r6_features->GetFeatureString().c_str());
  EXPECT_EQ(mips32r6_features->AsBitmap(), 14U);

  std::unique_ptr<const InstructionSetFeatures> mips_default_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "default", &error_msg));
  ASSERT_TRUE(mips_default_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r6_features->Equals(mips_default_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r1_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r1", &error_msg));
  ASSERT_TRUE(mips32r1_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r6_features->Equals(mips32r1_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r2_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r2", &error_msg));
  ASSERT_TRUE(mips32r2_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r6_features->Equals(mips32r2_features.get()));

  std::unique_ptr<const InstructionSetFeatures> mips32r5_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kMips, "mips32r5", &error_msg));
  ASSERT_TRUE(mips32r5_features.get() != nullptr) << error_msg;
  EXPECT_FALSE(mips32r6_features->Equals(mips32r5_features.get()));
}

}  // namespace art
