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

#include "instruction_set_features_mips64.h"

#include <fstream>
#include <sstream>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/logging.h"

namespace art {

using android::base::StringPrintf;

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg ATTRIBUTE_UNUSED) {
  bool msa = true;
  if (variant != "default" && variant != "mips64r6") {
    LOG(WARNING) << "Unexpected CPU variant for Mips64 using defaults: " << variant;
  }
  return Mips64FeaturesUniquePtr(new Mips64InstructionSetFeatures(msa));
}

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool msa = (bitmap & kMsaBitfield) != 0;
  return Mips64FeaturesUniquePtr(new Mips64InstructionSetFeatures(msa));
}

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromCppDefines() {
#if defined(_MIPS_ARCH_MIPS64R6)
  const bool msa = true;
#else
  const bool msa = false;
#endif
  return Mips64FeaturesUniquePtr(new Mips64InstructionSetFeatures(msa));
}

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool msa = false;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("ASEs") != std::string::npos) {
          LOG(INFO) << "found Application Specific Extensions";
          if (line.find("msa") != std::string::npos) {
            msa = true;
          }
        }
      }
    }
    in.close();
  } else {
    LOG(ERROR) << "Failed to open /proc/cpuinfo";
  }
  return Mips64FeaturesUniquePtr(new Mips64InstructionSetFeatures(msa));
}

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromHwcap() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

Mips64FeaturesUniquePtr Mips64InstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

bool Mips64InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kMips64 != other->GetInstructionSet()) {
    return false;
  }
  const Mips64InstructionSetFeatures* other_as_mips64 = other->AsMips64InstructionSetFeatures();
  return msa_ == other_as_mips64->msa_;
}

uint32_t Mips64InstructionSetFeatures::AsBitmap() const {
  return (msa_ ? kMsaBitfield : 0);
}

std::string Mips64InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (msa_) {
    result += "msa";
  } else {
    result += "-msa";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
Mips64InstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  bool msa = msa_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "msa") {
      msa = true;
    } else if (feature == "-msa") {
      msa = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return std::unique_ptr<const InstructionSetFeatures>(new Mips64InstructionSetFeatures(msa));
}

}  // namespace art
