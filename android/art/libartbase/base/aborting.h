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

#ifndef ART_LIBARTBASE_BASE_ABORTING_H_
#define ART_LIBARTBASE_BASE_ABORTING_H_

#include <atomic>

namespace art {

// 0 if not abort, non-zero if an abort is in progress. Used on fatal exit to prevents recursive
// aborts. Global declaration allows us to disable some error checking to ensure fatal shutdown
// makes forward progress.
extern std::atomic<unsigned int> gAborting;

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_ABORTING_H_
