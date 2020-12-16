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

#ifndef ART_LIBARTBASE_BASE_SYSTRACE_H_
#define ART_LIBARTBASE_BASE_SYSTRACE_H_

#define ATRACE_TAG ATRACE_TAG_DALVIK
#include <cutils/trace.h>

#include <sstream>
#include <string>

#include "android-base/stringprintf.h"

namespace art {

class ScopedTrace {
 public:
  explicit ScopedTrace(const char* name) {
    ATRACE_BEGIN(name);
  }
  template <typename Fn>
  explicit ScopedTrace(Fn fn) {
    if (ATRACE_ENABLED()) {
      ATRACE_BEGIN(fn().c_str());
    }
  }

  explicit ScopedTrace(const std::string& name) : ScopedTrace(name.c_str()) {}

  ~ScopedTrace() {
    ATRACE_END();
  }
};

// Helper for the SCOPED_TRACE macro. Do not use directly.
class ScopedTraceNoStart {
 public:
  ScopedTraceNoStart() {
  }

  ~ScopedTraceNoStart() {
    ATRACE_END();
  }

  // Message helper for the macro. Do not use directly.
  class ScopedTraceMessageHelper {
   public:
    ScopedTraceMessageHelper() {
    }
    ~ScopedTraceMessageHelper() {
      ATRACE_BEGIN(buffer_.str().c_str());
    }

    std::ostream& stream() {
      return buffer_;
    }

   private:
    std::ostringstream buffer_;
  };
};

#define SCOPED_TRACE \
  ::art::ScopedTraceNoStart trace ## __LINE__; \
  (ATRACE_ENABLED()) && ::art::ScopedTraceNoStart::ScopedTraceMessageHelper().stream()

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_SYSTRACE_H_
