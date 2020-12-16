/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_CMDLINE_DETAIL_CMDLINE_DEBUG_DETAIL_H_
#define ART_CMDLINE_DETAIL_CMDLINE_DEBUG_DETAIL_H_

#include <iostream>
#ifndef CMDLINE_NDEBUG
#define CMDLINE_DEBUG_LOG std::cerr
#else
#define CMDLINE_DEBUG_LOG ::art::detail::debug_log_ignore()
#endif

namespace art {
// Implementation details for some template querying. Don't look inside if you hate templates.
namespace detail {
struct debug_log_ignore {
  // Ignore most of the normal operator<< usage.
  template <typename T>
  debug_log_ignore& operator<<(const T&) { return *this; }
  // Ignore std::endl and the like.
  debug_log_ignore& operator<<(std::ostream& (*)(std::ostream&) ) { return *this; }
};
}  // namespace detail  // NOLINT [readability/namespace] [5]
}  // namespace art

#endif  // ART_CMDLINE_DETAIL_CMDLINE_DEBUG_DETAIL_H_
