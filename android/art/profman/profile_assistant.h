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

#ifndef ART_PROFMAN_PROFILE_ASSISTANT_H_
#define ART_PROFMAN_PROFILE_ASSISTANT_H_

#include <string>
#include <vector>

#include "base/scoped_flock.h"
#include "jit/profile_compilation_info.h"

namespace art {

class ProfileAssistant {
 public:
  // These also serve as return codes of profman and are processed by installd
  // (frameworks/native/cmds/installd/commands.cpp)
  enum ProcessingResult {
    kCompile = 0,
    kSkipCompilation = 1,
    kErrorBadProfiles = 2,
    kErrorIO = 3,
    kErrorCannotLock = 4
  };

  // Process the profile information present in the given files. Returns one of
  // ProcessingResult values depending on profile information and whether or not
  // the analysis ended up successfully (i.e. no errors during reading,
  // merging or writing of profile files).
  //
  // When the returned value is kCompile there is a significant difference
  // between profile_files and reference_profile_files. In this case
  // reference_profile will be updated with the profiling info obtain after
  // merging all profiles.
  //
  // When the returned value is kSkipCompilation, the difference between the
  // merge of the current profiles and the reference one is insignificant. In
  // this case no file will be updated.
  //
  static ProcessingResult ProcessProfiles(
      const std::vector<std::string>& profile_files,
      const std::string& reference_profile_file,
      const ProfileCompilationInfo::ProfileLoadFilterFn& filter_fn
          = ProfileCompilationInfo::ProfileFilterFnAcceptAll);

  static ProcessingResult ProcessProfiles(
      const std::vector<int>& profile_files_fd_,
      int reference_profile_file_fd,
      const ProfileCompilationInfo::ProfileLoadFilterFn& filter_fn
          = ProfileCompilationInfo::ProfileFilterFnAcceptAll);

 private:
  static ProcessingResult ProcessProfilesInternal(
      const std::vector<ScopedFlock>& profile_files,
      const ScopedFlock& reference_profile_file,
      const ProfileCompilationInfo::ProfileLoadFilterFn& filter_fn);

  DISALLOW_COPY_AND_ASSIGN(ProfileAssistant);
};

}  // namespace art

#endif  // ART_PROFMAN_PROFILE_ASSISTANT_H_
