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

#ifndef ART_CMDLINE_CMDLINE_RESULT_H_
#define ART_CMDLINE_CMDLINE_RESULT_H_

#include <assert.h>
#include "base/utils.h"

namespace art {
// Result of an attempt to process the command line arguments. If fails, specifies
// the specific error code and an error message.
// Use the value-carrying CmdlineParseResult<T> to get an additional value out in a success case.
struct CmdlineResult {
  enum Status {
    kSuccess,
    // Error codes:
    kUsage,
    kFailure,
    kOutOfRange,
    kUnknown,
  };

  // Short-hand for checking if the result was successful.
  operator bool() const {
    return IsSuccess();
  }

  // Check if the operation has succeeded.
  bool IsSuccess() const { return status_ == kSuccess; }
  // Check if the operation was not a success.
  bool IsError() const { return status_ != kSuccess; }
  // Get the specific status, regardless of whether it's failure or success.
  Status GetStatus() const { return status_; }

  // Get the error message, *must* only be called for error status results.
  const std::string& GetMessage() const { assert(IsError()); return message_; }

  // Constructor any status. No message.
  explicit CmdlineResult(Status status) : status_(status) {}

  // Constructor with an error status, copying the message.
  CmdlineResult(Status status, const std::string& message)
    : status_(status), message_(message) {
    assert(status != kSuccess);
  }

  // Constructor with an error status, taking over the message.
  CmdlineResult(Status status, std::string&& message)
    : status_(status), message_(message) {
    assert(status != kSuccess);
  }

  // Make sure copying exists
  CmdlineResult(const CmdlineResult&) = default;
  // Make sure moving is cheap
  CmdlineResult(CmdlineResult&&) = default;

 private:
  const Status status_;
  const std::string message_;
};

// TODO: code-generate this
static inline std::ostream& operator<<(std::ostream& stream, CmdlineResult::Status status) {
  switch (status) {
    case CmdlineResult::kSuccess:
      stream << "kSuccess";
      break;
    case CmdlineResult::kUsage:
      stream << "kUsage";
      break;
    case CmdlineResult::kFailure:
      stream << "kFailure";
      break;
    case CmdlineResult::kOutOfRange:
      stream << "kOutOfRange";
      break;
    case CmdlineResult::kUnknown:
      stream << "kUnknown";
      break;
    default:
      UNREACHABLE();
  }
  return stream;
}
}  // namespace art

#endif  // ART_CMDLINE_CMDLINE_RESULT_H_
