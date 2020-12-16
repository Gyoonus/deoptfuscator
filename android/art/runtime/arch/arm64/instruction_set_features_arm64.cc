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

#include "instruction_set_features_arm64.h"

#include <fstream>
#include <sstream>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "base/stl_util.h"

namespace art {

using android::base::StringPrintf;

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg) {
  // Look for variants that need a fix for a53 erratum 835769.
  static const char* arm64_variants_with_a53_835769_bug[] = {
      // Pessimistically assume all generic CPUs are cortex-a53.
      "default",
      "generic",
      "cortex-a53",
      "cortex-a53.a57",
      "cortex-a53.a72",
      // Pessimistically assume all "big" cortex CPUs are paired with a cortex-a53.
      "cortex-a57",
      "cortex-a72",
      "cortex-a73",
  };
  bool needs_a53_835769_fix = FindVariantInArray(arm64_variants_with_a53_835769_bug,
                                                 arraysize(arm64_variants_with_a53_835769_bug),
                                                 variant);

  if (!needs_a53_835769_fix) {
    // Check to see if this is an expected variant.
    static const char* arm64_known_variants[] = {
        "cortex-a35",
        "cortex-a55",
        "cortex-a75",
        "exynos-m1",
        "exynos-m2",
        "exynos-m3",
        "denver64",
        "kryo"
    };
    if (!FindVariantInArray(arm64_known_variants, arraysize(arm64_known_variants), variant)) {
      std::ostringstream os;
      os << "Unexpected CPU variant for Arm64: " << variant;
      *error_msg = os.str();
      return nullptr;
    }
  }

  // The variants that need a fix for 843419 are the same that need a fix for 835769.
  bool needs_a53_843419_fix = needs_a53_835769_fix;

  return Arm64FeaturesUniquePtr(
      new Arm64InstructionSetFeatures(needs_a53_835769_fix, needs_a53_843419_fix));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool is_a53 = (bitmap & kA53Bitfield) != 0;
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53, is_a53));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCppDefines() {
  const bool is_a53 = true;  // Pessimistically assume all ARM64s are A53s.
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53, is_a53));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromCpuInfo() {
  const bool is_a53 = true;  // Conservative default.
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53, is_a53));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromHwcap() {
  const bool is_a53 = true;  // Pessimistically assume all ARM64s are A53s.
  return Arm64FeaturesUniquePtr(new Arm64InstructionSetFeatures(is_a53, is_a53));
}

Arm64FeaturesUniquePtr Arm64InstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

bool Arm64InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm64 != other->GetInstructionSet()) {
    return false;
  }
  const Arm64InstructionSetFeatures* other_as_arm64 = other->AsArm64InstructionSetFeatures();
  return fix_cortex_a53_835769_ == other_as_arm64->fix_cortex_a53_835769_ &&
      fix_cortex_a53_843419_ == other_as_arm64->fix_cortex_a53_843419_;
}

uint32_t Arm64InstructionSetFeatures::AsBitmap() const {
  return (fix_cortex_a53_835769_ ? kA53Bitfield : 0);
}

std::string Arm64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (fix_cortex_a53_835769_) {
    result += "a53";
  } else {
    result += "-a53";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
Arm64InstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  bool is_a53 = fix_cortex_a53_835769_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "a53") {
      is_a53 = true;
    } else if (feature == "-a53") {
      is_a53 = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return std::unique_ptr<const InstructionSetFeatures>(
      new Arm64InstructionSetFeatures(is_a53, is_a53));
}

}  // namespace art
