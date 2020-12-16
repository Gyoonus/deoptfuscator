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

#include <fstream>
#include <sstream>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "base/stl_util.h"

namespace art {

using android::base::StringPrintf;

// An enum for the Mips revision.
enum class MipsLevel {
  kBase,
  kR2,
  kR5,
  kR6
};

#if defined(_MIPS_ARCH_MIPS32R6)
static constexpr MipsLevel kRuntimeMipsLevel = MipsLevel::kR6;
#elif defined(_MIPS_ARCH_MIPS32R5)
static constexpr MipsLevel kRuntimeMipsLevel = MipsLevel::kR5;
#elif defined(_MIPS_ARCH_MIPS32R2)
static constexpr MipsLevel kRuntimeMipsLevel = MipsLevel::kR2;
#else
static constexpr MipsLevel kRuntimeMipsLevel = MipsLevel::kBase;
#endif

static void GetFlagsFromCppDefined(bool* mips_isa_gte2, bool* r6, bool* fpu_32bit, bool* msa) {
  // Override defaults based on compiler flags.
  if (kRuntimeMipsLevel >= MipsLevel::kR2) {
    *mips_isa_gte2 = true;
  } else {
    *mips_isa_gte2 = false;
  }

  if (kRuntimeMipsLevel >= MipsLevel::kR5) {
    *fpu_32bit = false;
    *msa = true;
  } else {
    *fpu_32bit = true;
    *msa = false;
  }

  if (kRuntimeMipsLevel >= MipsLevel::kR6) {
    *r6 = true;
  } else {
    *r6 = false;
  }
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg ATTRIBUTE_UNUSED) {

  // Override defaults based on compiler flags.
  // This is needed when running ART test where the variant is not defined.
  bool fpu_32bit;
  bool mips_isa_gte2;
  bool r6;
  bool msa;
  GetFlagsFromCppDefined(&mips_isa_gte2, &r6, &fpu_32bit, &msa);

  // Override defaults based on variant string.
  // Only care if it is R1, R2, R5 or R6 and we assume all CPUs will have a FP unit.
  constexpr const char* kMips32Prefix = "mips32r";
  const size_t kPrefixLength = strlen(kMips32Prefix);
  if (variant.compare(0, kPrefixLength, kMips32Prefix, kPrefixLength) == 0 &&
      variant.size() > kPrefixLength) {
    r6 = (variant[kPrefixLength] >= '6');
    fpu_32bit = (variant[kPrefixLength] < '5');
    mips_isa_gte2 = (variant[kPrefixLength] >= '2');
    msa = (variant[kPrefixLength] >= '5');
  } else if (variant == "default") {
    // Default variant has FPU, is gte2. This is the traditional setting.
    //
    // Note, we get FPU bitness and R6-ness from the build (using cpp defines, see above)
    // and don't override them because many things depend on the "default" variant being
    // sufficient for most purposes. That is, "default" should work for both R2 and R6.
    // Use "mips32r#" to get a specific configuration, possibly not matching the runtime
    // ISA (e.g. for ISA-specific testing of dex2oat internals).
    mips_isa_gte2 = true;
  } else {
    LOG(WARNING) << "Unexpected CPU variant for Mips32 using defaults: " << variant;
  }

  return MipsFeaturesUniquePtr(new MipsInstructionSetFeatures(fpu_32bit, mips_isa_gte2, r6, msa));
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool fpu_32bit = (bitmap & kFpu32Bitfield) != 0;
  bool mips_isa_gte2 = (bitmap & kIsaRevGte2Bitfield) != 0;
  bool r6 = (bitmap & kR6) != 0;
  bool msa = (bitmap & kMsaBitfield) != 0;
  return MipsFeaturesUniquePtr(new MipsInstructionSetFeatures(fpu_32bit, mips_isa_gte2, r6, msa));
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromCppDefines() {
  bool fpu_32bit;
  bool mips_isa_gte2;
  bool r6;
  bool msa;
  GetFlagsFromCppDefined(&mips_isa_gte2, &r6, &fpu_32bit, &msa);

  return MipsFeaturesUniquePtr(new MipsInstructionSetFeatures(fpu_32bit, mips_isa_gte2, r6, msa));
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromCpuInfo() {
  bool fpu_32bit;
  bool mips_isa_gte2;
  bool r6;
  bool msa;
  GetFlagsFromCppDefined(&mips_isa_gte2, &r6, &fpu_32bit, &msa);

  msa = false;

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

  return MipsFeaturesUniquePtr(new MipsInstructionSetFeatures(fpu_32bit, mips_isa_gte2, r6, msa));
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromHwcap() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

MipsFeaturesUniquePtr MipsInstructionSetFeatures::FromAssembly() {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines();
}

bool MipsInstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kMips != other->GetInstructionSet()) {
    return false;
  }
  const MipsInstructionSetFeatures* other_as_mips = other->AsMipsInstructionSetFeatures();
  return (fpu_32bit_ == other_as_mips->fpu_32bit_) &&
      (mips_isa_gte2_ == other_as_mips->mips_isa_gte2_) &&
      (r6_ == other_as_mips->r6_) &&
      (msa_ == other_as_mips->msa_);
}

uint32_t MipsInstructionSetFeatures::AsBitmap() const {
  return (fpu_32bit_ ? kFpu32Bitfield : 0) |
      (mips_isa_gte2_ ? kIsaRevGte2Bitfield : 0) |
      (r6_ ? kR6 : 0) |
      (msa_ ? kMsaBitfield : 0);
}

std::string MipsInstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (fpu_32bit_) {
    result += "fpu32";
  } else {
    result += "-fpu32";
  }
  if (mips_isa_gte2_) {
    result += ",mips2";
  } else {
    result += ",-mips2";
  }
  if (r6_) {
    result += ",r6";
  }  // Suppress non-r6.
  if (msa_) {
    result += ",msa";
  } else {
    result += ",-msa";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
MipsInstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  bool fpu_32bit = fpu_32bit_;
  bool mips_isa_gte2 = mips_isa_gte2_;
  bool r6 = r6_;
  bool msa = msa_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "fpu32") {
      fpu_32bit = true;
    } else if (feature == "-fpu32") {
      fpu_32bit = false;
    } else if (feature == "mips2") {
      mips_isa_gte2 = true;
    } else if (feature == "-mips2") {
      mips_isa_gte2 = false;
    } else if (feature == "r6") {
      r6 = true;
    } else if (feature == "-r6") {
      r6 = false;
    } else if (feature == "msa") {
      msa = true;
    } else if (feature == "-msa") {
      msa = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return std::unique_ptr<const InstructionSetFeatures>(
      new MipsInstructionSetFeatures(fpu_32bit, mips_isa_gte2, r6, msa));
}

}  // namespace art
