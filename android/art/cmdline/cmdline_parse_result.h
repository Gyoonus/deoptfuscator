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

#ifndef ART_CMDLINE_CMDLINE_PARSE_RESULT_H_
#define ART_CMDLINE_CMDLINE_PARSE_RESULT_H_

#include "cmdline_result.h"
#include "detail/cmdline_parser_detail.h"

namespace art {
// Result of a type-parsing attempt. If successful holds the strongly-typed value,
// otherwise it holds either a usage or a failure string message that should be displayed back
// to the user.
//
// CmdlineType::Parse/CmdlineType::ParseAndAppend must return this type.
template <typename T>
struct CmdlineParseResult : CmdlineResult {
  using CmdlineResult::CmdlineResult;

  // Create an error result with the usage error code and the specified message.
  static CmdlineParseResult Usage(const std::string& message) {
    return CmdlineParseResult(kUsage, message);
  }

  // Create an error result with the failure error code and no message.
  static CmdlineParseResult<T> Failure()  {
    return CmdlineParseResult(kFailure);
  }

  // Create an error result with the failure error code and no message.
  static CmdlineParseResult<T> Failure(const std::string& message) {
    return CmdlineParseResult(kFailure, message);
  }

  // Create a successful result which holds the specified value.
  static CmdlineParseResult<T> Success(const T& value) {
    return CmdlineParseResult(value);
  }

  // Create a successful result, taking over the value.
  static CmdlineParseResult<T> Success(T&& value) {
    return CmdlineParseResult(std::forward<T>(value));
  }

  // Create succesful result, without any values. Used when a value was successfully appended
  // into an existing object.
  static CmdlineParseResult<T> SuccessNoValue() {
    return CmdlineParseResult(T {});
  }

  // Create an error result with the OutOfRange error and the specified message.
  static CmdlineParseResult<T> OutOfRange(const std::string& message) {
    return CmdlineParseResult(kOutOfRange, message);
  }

  // Create an error result with the OutOfRange code and a custom message
  // which is printed from the actual/min/max values.
  // Values are converted to string using the ostream<< operator.
  static CmdlineParseResult<T> OutOfRange(const T& value,
                                          const T& min,
                                          const T& max) {
    return CmdlineParseResult(kOutOfRange,
                              "actual: " + art::detail::ToStringAny(value) +
                              ", min: " + art::detail::ToStringAny(min) +
                              ", max: " + art::detail::ToStringAny(max));
  }

  // Get a read-only reference to the underlying value.
  // The result must have been successful and must have a value.
  const T& GetValue() const {
    assert(IsSuccess());
    assert(has_value_);
    return value_;
  }

  // Get a mutable reference to the underlying value.
  // The result must have been successful and must have a value.
  T& GetValue() {
    assert(IsSuccess());
    assert(has_value_);
    return value_;
  }

  // Take over the value.
  // The result must have been successful and must have a value.
  T&& ReleaseValue() {
    assert(IsSuccess());
    assert(has_value_);
    return std::move(value_);
  }

  // Whether or not the result has a value (e.g. created with Result::Success).
  // Error results never have values, success results commonly, but not always, have values.
  bool HasValue() const {
    return has_value_;
  }

  // Cast an error-result from type T2 to T1.
  // Safe since error-results don't store a typed value.
  template <typename T2>
  static CmdlineParseResult<T> CastError(const CmdlineParseResult<T2>& other) {
    assert(other.IsError());
    return CmdlineParseResult<T>(other.GetStatus());
  }

  // Make sure copying is allowed
  CmdlineParseResult(const CmdlineParseResult&) = default;
  // Make sure moving is cheap
  CmdlineParseResult(CmdlineParseResult&&) = default;

 private:
  explicit CmdlineParseResult(const T& value)
    : CmdlineResult(kSuccess), value_(value), has_value_(true) {}
  explicit CmdlineParseResult(T&& value)
    : CmdlineResult(kSuccess), value_(std::forward<T>(value)), has_value_(true) {}
  CmdlineParseResult()
    : CmdlineResult(kSuccess), value_(), has_value_(false) {}

  T value_;
  bool has_value_ = false;
};

}  // namespace art

#endif  // ART_CMDLINE_CMDLINE_PARSE_RESULT_H_
