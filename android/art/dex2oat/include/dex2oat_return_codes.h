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

#ifndef ART_DEX2OAT_INCLUDE_DEX2OAT_RETURN_CODES_H_
#define ART_DEX2OAT_INCLUDE_DEX2OAT_RETURN_CODES_H_

namespace art {
namespace dex2oat {

enum class ReturnCode : int {
  kNoFailure = 0,          // No failure, execution completed successfully.
  kOther = 1,              // Some other not closer specified error occurred.
  kCreateRuntime = 2,      // Dex2oat failed creating a runtime. This may be indicative
                           // of a missing or out of date boot image, for example.
};

}  // namespace dex2oat
}  // namespace art

#endif  // ART_DEX2OAT_INCLUDE_DEX2OAT_RETURN_CODES_H_
