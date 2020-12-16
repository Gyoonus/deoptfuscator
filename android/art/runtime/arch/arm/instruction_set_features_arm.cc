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

#include "instruction_set_features_arm.h"

#if defined(ART_TARGET_ANDROID) && defined(__arm__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

#include "signal.h"

#include <fstream>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#if defined(__arm__)
extern "C" bool artCheckForArmSdivInstruction();
extern "C" bool artCheckForArmv8AInstructions();
#endif

namespace art {

using android::base::StringPrintf;

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromVariant(
    const std::string& variant, std::string* error_msg) {
  static const char* arm_variants_with_armv8a[] = {
      "cortex-a32",
      "cortex-a35",
      "cortex-a53",
      "cortex-a53.a57",
      "cortex-a53.a72",
      "cortex-a55",
      "cortex-a57",
      "cortex-a72",
      "cortex-a73",
      "cortex-a75",
      "exynos-m1",
      "denver",
      "kryo"
  };
  bool has_armv8a = FindVariantInArray(arm_variants_with_armv8a,
                                       arraysize(arm_variants_with_armv8a),
                                       variant);

  // Look for variants that have divide support.
  static const char* arm_variants_with_div[] = {
      "cortex-a7",
      "cortex-a12",
      "cortex-a15",
      "cortex-a17",
      "krait",
  };
  bool has_div = has_armv8a || FindVariantInArray(arm_variants_with_div,
                                                  arraysize(arm_variants_with_div),
                                                  variant);

  // Look for variants that have LPAE support.
  static const char* arm_variants_with_lpae[] = {
      "cortex-a7",
      "cortex-a12",
      "cortex-a15",
      "cortex-a17",
      "krait",
  };
  bool has_atomic_ldrd_strd = has_armv8a || FindVariantInArray(arm_variants_with_lpae,
                                                               arraysize(arm_variants_with_lpae),
                                                               variant);

  if (has_armv8a == false && has_div == false && has_atomic_ldrd_strd == false) {
    static const char* arm_variants_with_default_features[] = {
        "cortex-a5",
        "cortex-a8",
        "cortex-a9",
        "cortex-a9-mp",
        "default",
        "generic"
    };
    if (!FindVariantInArray(arm_variants_with_default_features,
                            arraysize(arm_variants_with_default_features),
                            variant)) {
      *error_msg = StringPrintf("Attempt to use unsupported ARM variant: %s", variant.c_str());
      return nullptr;
    } else {
      // Warn if we use the default features.
      LOG(WARNING) << "Using default instruction set features for ARM CPU variant (" << variant
          << ") using conservative defaults";
    }
  }
  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromBitmap(uint32_t bitmap) {
  bool has_div = (bitmap & kDivBitfield) != 0;
  bool has_atomic_ldrd_strd = (bitmap & kAtomicLdrdStrdBitfield) != 0;
  bool has_armv8a = (bitmap & kARMv8A) != 0;
  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromCppDefines() {
// Note: This will not work for now since we still build the 32-bit as __ARCH_ARM_7A__.
#if defined(__ARM_ARCH_8A__)
  const bool has_armv8a = true;
#else
  const bool has_armv8a = false;
#endif
#if defined (__ARM_ARCH_8A__) || defined(__ARM_ARCH_EXT_IDIV__)
  const bool has_div = true;
#else
  const bool has_div = false;
#endif
#if defined (__ARM_ARCH_8A__) || defined(__ARM_FEATURE_LPAE)
  const bool has_atomic_ldrd_strd = true;
#else
  const bool has_atomic_ldrd_strd = false;
#endif
  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromCpuInfo() {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool has_atomic_ldrd_strd = false;
  bool has_div = false;
  bool has_armv8a = false;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("Features") != std::string::npos) {
          LOG(INFO) << "found features";
          if (line.find("idivt") != std::string::npos) {
            // We always expect both ARM and Thumb divide instructions to be available or not
            // available.
            CHECK_NE(line.find("idiva"), std::string::npos);
            has_div = true;
          }
          if (line.find("lpae") != std::string::npos) {
            has_atomic_ldrd_strd = true;
          }
        }
        if (line.find("architecture") != std::string::npos
            && line.find(": 8") != std::string::npos) {
          LOG(INFO) << "found architecture ARMv8";
          // Android is only run on A cores, so ARMv8 implies ARMv8-A.
          has_armv8a = true;
          // ARMv8 CPUs have LPAE and div support.
          has_div = true;
          has_atomic_ldrd_strd = true;
        }
      }
    }
    in.close();
  } else {
    LOG(ERROR) << "Failed to open /proc/cpuinfo";
  }
  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromHwcap() {
  bool has_div = false;
  bool has_atomic_ldrd_strd = false;
  bool has_armv8a = false;

#if defined(ART_TARGET_ANDROID) && defined(__arm__)
  uint64_t hwcaps = getauxval(AT_HWCAP);
  LOG(INFO) << "hwcaps=" << hwcaps;
  if ((hwcaps & HWCAP_IDIVT) != 0) {
    // We always expect both ARM and Thumb divide instructions to be available or not
    // available.
    CHECK_NE(hwcaps & HWCAP_IDIVA, 0U);
    has_div = true;
  }
  if ((hwcaps & HWCAP_LPAE) != 0) {
    has_atomic_ldrd_strd = true;
  }
  // TODO: Fix this once FPMISC makes it upstream.
  // For now we detect if we run on an ARMv8 CPU by looking for CRC32 and SHA1
  // (only available on ARMv8 CPUs).
  if ((hwcaps & HWCAP2_CRC32) != 0 && (hwcaps & HWCAP2_SHA1) != 0) {
    has_armv8a = true;
  }
#endif

  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

// A signal handler called by a fault for an illegal instruction.  We record the fact in r0
// and then increment the PC in the signal context to return to the next instruction.  We know the
// instruction is 4 bytes long.
static void bad_instr_handle(int signo ATTRIBUTE_UNUSED,
                            siginfo_t* si ATTRIBUTE_UNUSED,
                            void* data) {
#if defined(__arm__)
  struct ucontext *uc = (struct ucontext *)data;
  struct sigcontext *sc = &uc->uc_mcontext;
  sc->arm_r0 = 0;     // Set R0 to #0 to signal error.
  sc->arm_pc += 4;    // Skip offending instruction.
#else
  UNUSED(data);
#endif
}

ArmFeaturesUniquePtr ArmInstructionSetFeatures::FromAssembly() {
  // See if have a sdiv instruction.  Register a signal handler and try to execute an sdiv
  // instruction.  If we get a SIGILL then it's not supported.
  struct sigaction sa, osa;
  sa.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;
  sa.sa_sigaction = bad_instr_handle;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGILL, &sa, &osa);

  bool has_div = false;
  bool has_armv8a = false;
#if defined(__arm__)
  if (artCheckForArmSdivInstruction()) {
    has_div = true;
  }
  if (artCheckForArmv8AInstructions()) {
    has_armv8a = true;
  }
#endif

  // Restore the signal handler.
  sigaction(SIGILL, &osa, nullptr);

  // Use compile time features to "detect" LPAE support.
  // TODO: write an assembly LPAE support test.
#if defined(__ARM_FEATURE_LPAE)
  const bool has_atomic_ldrd_strd = true;
#else
  const bool has_atomic_ldrd_strd = false;
#endif
  return ArmFeaturesUniquePtr(new ArmInstructionSetFeatures(has_div,
                                                            has_atomic_ldrd_strd,
                                                            has_armv8a));
}

bool ArmInstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm != other->GetInstructionSet()) {
    return false;
  }
  const ArmInstructionSetFeatures* other_as_arm = other->AsArmInstructionSetFeatures();
  return has_div_ == other_as_arm->has_div_
      && has_atomic_ldrd_strd_ == other_as_arm->has_atomic_ldrd_strd_
      && has_armv8a_ == other_as_arm->has_armv8a_;
}

bool ArmInstructionSetFeatures::HasAtLeast(const InstructionSetFeatures* other) const {
  if (InstructionSet::kArm != other->GetInstructionSet()) {
    return false;
  }
  const ArmInstructionSetFeatures* other_as_arm = other->AsArmInstructionSetFeatures();
  return (has_div_ || !other_as_arm->has_div_)
      && (has_atomic_ldrd_strd_ || !other_as_arm->has_atomic_ldrd_strd_)
      && (has_armv8a_ || !other_as_arm->has_armv8a_);
}

uint32_t ArmInstructionSetFeatures::AsBitmap() const {
  return (has_div_ ? kDivBitfield : 0)
      | (has_atomic_ldrd_strd_ ? kAtomicLdrdStrdBitfield : 0)
      | (has_armv8a_ ? kARMv8A : 0);
}

std::string ArmInstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (has_div_) {
    result += "div";
  } else {
    result += "-div";
  }
  if (has_atomic_ldrd_strd_) {
    result += ",atomic_ldrd_strd";
  } else {
    result += ",-atomic_ldrd_strd";
  }
  if (has_armv8a_) {
    result += ",armv8a";
  } else {
    result += ",-armv8a";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures>
ArmInstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features, std::string* error_msg) const {
  bool has_atomic_ldrd_strd = has_atomic_ldrd_strd_;
  bool has_div = has_div_;
  bool has_armv8a = has_armv8a_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "div") {
      has_div = true;
    } else if (feature == "-div") {
      has_div = false;
    } else if (feature == "atomic_ldrd_strd") {
      has_atomic_ldrd_strd = true;
    } else if (feature == "-atomic_ldrd_strd") {
      has_atomic_ldrd_strd = false;
    } else if (feature == "armv8a") {
      has_armv8a = true;
    } else if (feature == "-armv8a") {
      has_armv8a = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return std::unique_ptr<const InstructionSetFeatures>(
      new ArmInstructionSetFeatures(has_div, has_atomic_ldrd_strd, has_armv8a));
}

}  // namespace art
