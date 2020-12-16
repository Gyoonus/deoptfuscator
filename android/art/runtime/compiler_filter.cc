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

#include "compiler_filter.h"

#include "base/utils.h"

namespace art {

bool CompilerFilter::IsAotCompilationEnabled(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify:
    case CompilerFilter::kQuicken: return false;

    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverythingProfile:
    case CompilerFilter::kEverything: return true;
  }
  UNREACHABLE();
}

bool CompilerFilter::IsJniCompilationEnabled(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify: return false;

    case CompilerFilter::kQuicken:
    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverythingProfile:
    case CompilerFilter::kEverything: return true;
  }
  UNREACHABLE();
}

bool CompilerFilter::IsQuickeningCompilationEnabled(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify: return false;

    case CompilerFilter::kQuicken:
    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverythingProfile:
    case CompilerFilter::kEverything: return true;
  }
  UNREACHABLE();
}

bool CompilerFilter::IsAnyCompilationEnabled(Filter filter) {
  return IsJniCompilationEnabled(filter) ||
      IsQuickeningCompilationEnabled(filter) ||
      IsAotCompilationEnabled(filter);
}

bool CompilerFilter::IsVerificationEnabled(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract: return false;

    case CompilerFilter::kVerify:
    case CompilerFilter::kQuicken:
    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverythingProfile:
    case CompilerFilter::kEverything: return true;
  }
  UNREACHABLE();
}

bool CompilerFilter::DependsOnImageChecksum(Filter filter) {
  // We run dex2dex with verification, so the oat file will depend on the
  // image checksum if verification is enabled.
  return IsVerificationEnabled(filter);
}

bool CompilerFilter::DependsOnProfile(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify:
    case CompilerFilter::kQuicken:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverything: return false;

    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kEverythingProfile: return true;
  }
  UNREACHABLE();
}

CompilerFilter::Filter CompilerFilter::GetNonProfileDependentFilterFrom(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify:
    case CompilerFilter::kQuicken:
    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverything:
      return filter;

    case CompilerFilter::kSpaceProfile:
      return CompilerFilter::kSpace;

    case CompilerFilter::kSpeedProfile:
      return CompilerFilter::kSpeed;

    case CompilerFilter::kEverythingProfile:
      return CompilerFilter::kEverything;
  }
  UNREACHABLE();
}

CompilerFilter::Filter CompilerFilter::GetSafeModeFilterFrom(Filter filter) {
  // For safe mode, we should not return a filter that generates AOT compiled
  // code.
  switch (filter) {
    case CompilerFilter::kAssumeVerified:
    case CompilerFilter::kExtract:
    case CompilerFilter::kVerify:
    case CompilerFilter::kQuicken:
      return filter;

    case CompilerFilter::kSpace:
    case CompilerFilter::kSpeed:
    case CompilerFilter::kEverything:
    case CompilerFilter::kSpaceProfile:
    case CompilerFilter::kSpeedProfile:
    case CompilerFilter::kEverythingProfile:
      return CompilerFilter::kQuicken;
  }
  UNREACHABLE();
}

bool CompilerFilter::IsAsGoodAs(Filter current, Filter target) {
  return current >= target;
}

bool CompilerFilter::IsBetter(Filter current, Filter target) {
  return current > target;
}

std::string CompilerFilter::NameOfFilter(Filter filter) {
  switch (filter) {
    case CompilerFilter::kAssumeVerified: return "assume-verified";
    case CompilerFilter::kExtract: return "extract";
    case CompilerFilter::kVerify: return "verify";
    case CompilerFilter::kQuicken: return "quicken";
    case CompilerFilter::kSpaceProfile: return "space-profile";
    case CompilerFilter::kSpace: return "space";
    case CompilerFilter::kSpeedProfile: return "speed-profile";
    case CompilerFilter::kSpeed: return "speed";
    case CompilerFilter::kEverythingProfile: return "everything-profile";
    case CompilerFilter::kEverything: return "everything";
  }
  UNREACHABLE();
}

bool CompilerFilter::ParseCompilerFilter(const char* option, Filter* filter) {
  CHECK(filter != nullptr);

  if (strcmp(option, "verify-none") == 0) {
    LOG(WARNING) << "'verify-none' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'assume-verified' instead.";
    *filter = kAssumeVerified;
  } else if (strcmp(option, "interpret-only") == 0) {
    LOG(WARNING) << "'interpret-only' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'quicken' instead.";
    *filter = kQuicken;
  } else if (strcmp(option, "verify-profile") == 0) {
    LOG(WARNING) << "'verify-profile' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'verify' instead.";
    *filter = kVerify;
  } else if (strcmp(option, "verify-at-runtime") == 0) {
    LOG(WARNING) << "'verify-at-runtime' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'extract' instead.";
    *filter = kExtract;
  } else if (strcmp(option, "balanced") == 0) {
    LOG(WARNING) << "'balanced' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'speed' instead.";
    *filter = kSpeed;
  } else if (strcmp(option, "time") == 0) {
    LOG(WARNING) << "'time' is an obsolete compiler filter name that will be "
                 << "removed in future releases, please use 'space' instead.";
    *filter = kSpace;
  } else if (strcmp(option, "assume-verified") == 0) {
    *filter = kAssumeVerified;
  } else if (strcmp(option, "extract") == 0) {
    *filter = kExtract;
  } else if (strcmp(option, "verify") == 0) {
    *filter = kVerify;
  } else if (strcmp(option, "quicken") == 0) {
    *filter = kQuicken;
  } else if (strcmp(option, "space") == 0) {
    *filter = kSpace;
  } else if (strcmp(option, "space-profile") == 0) {
    *filter = kSpaceProfile;
  } else if (strcmp(option, "speed") == 0) {
    *filter = kSpeed;
  } else if (strcmp(option, "speed-profile") == 0) {
    *filter = kSpeedProfile;
  } else if (strcmp(option, "everything") == 0) {
    *filter = kEverything;
  } else if (strcmp(option, "everything-profile") == 0) {
    *filter = kEverythingProfile;
  } else {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const CompilerFilter::Filter& rhs) {
  return os << CompilerFilter::NameOfFilter(rhs);
}

}  // namespace art
