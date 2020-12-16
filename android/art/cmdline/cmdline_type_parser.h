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

#ifndef ART_CMDLINE_CMDLINE_TYPE_PARSER_H_
#define ART_CMDLINE_CMDLINE_TYPE_PARSER_H_

#include "cmdline_parse_result.h"

namespace art {

// Base class for user-defined CmdlineType<T> specializations.
//
// Not strictly necessary, but if the specializations fail to Define all of these functions
// the compilation will fail.
template <typename T>
struct CmdlineTypeParser {
  // Return value of parsing attempts. Represents a Success(T value) or an Error(int code)
  using Result = CmdlineParseResult<T>;

  // Parse a single value for an argument definition out of the wildcard component.
  //
  // e.g. if the argument definition was "foo:_", and the user-provided input was "foo:bar",
  // then args is "bar".
  Result Parse(const std::string& args ATTRIBUTE_UNUSED) {
    assert(false);
    return Result::Failure("Missing type specialization and/or value map");
  }

  // Parse a value and append it into the existing value so far, for argument
  // definitions which are marked with AppendValues().
  //
  // The value is parsed out of the wildcard component as in Parse.
  //
  // If the initial value does not exist yet, a default value is created by
  // value-initializing with 'T()'.
  Result ParseAndAppend(const std::string& args ATTRIBUTE_UNUSED,
                        T& existing_value ATTRIBUTE_UNUSED) {
    assert(false);
    return Result::Failure("Missing type specialization and/or value map");
  }

  // Runtime type name of T, so that we can print more useful error messages.
  static const char* Name() { assert(false); return "UnspecializedType"; }

  // Whether or not your type can parse argument definitions defined without a "_"
  // e.g. -Xenable-profiler just mutates the existing profiler struct in-place
  // so it doesn't need to do any parsing other than token recognition.
  //
  // If this is false, then either the argument definition has a _, from which the parsing
  // happens, or the tokens get mapped to a value list/map from which a 1:1 matching occurs.
  //
  // This should almost *always* be false!
  static constexpr bool kCanParseBlankless = false;

 protected:
  // Don't accidentally initialize instances of this directly; they will assert at runtime.
  CmdlineTypeParser() = default;
};


}  // namespace art

#endif  // ART_CMDLINE_CMDLINE_TYPE_PARSER_H_
