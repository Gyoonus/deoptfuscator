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

#ifndef ART_CMDLINE_DETAIL_CMDLINE_PARSER_DETAIL_H_
#define ART_CMDLINE_DETAIL_CMDLINE_PARSER_DETAIL_H_

#include <sstream>
#include <string>
#include <vector>

namespace art {
// Implementation details for some template querying. Don't look inside if you hate templates.
namespace detail {
template <typename T>
typename std::remove_reference<T>::type& FakeReference();

// SupportsInsertionOperator<T, TStream>::value will evaluate to a boolean,
// whose value is true if the TStream class supports the << operator against T,
// and false otherwise.
template <typename T2, typename TStream2 = std::ostream>
struct SupportsInsertionOperator {
 private:
  template <typename TStream, typename T>
  static std::true_type InsertionOperatorTest(TStream& os, const T& value,
                                              std::remove_reference<decltype(os << value)>* = 0);  // NOLINT [whitespace/operators] [3]

  template <typename TStream, typename ... T>
  static std::false_type InsertionOperatorTest(TStream& os, const T& ... args);

 public:
  static constexpr bool value =
      decltype(InsertionOperatorTest(FakeReference<TStream2>(), std::declval<T2>()))::value;
};

template <typename TLeft, typename TRight = TLeft, bool IsFloatingPoint = false>
struct SupportsEqualityOperatorImpl;

template <typename TLeft, typename TRight>
struct SupportsEqualityOperatorImpl<TLeft, TRight, false> {
 private:
  template <typename TL, typename TR>
  static std::true_type EqualityOperatorTest(const TL& left, const TR& right,
                                             std::remove_reference<decltype(left == right)>* = 0);  // NOLINT [whitespace/operators] [3]

  template <typename TL, typename ... T>
  static std::false_type EqualityOperatorTest(const TL& left, const T& ... args);

 public:
  static constexpr bool value =
      decltype(EqualityOperatorTest(std::declval<TLeft>(), std::declval<TRight>()))::value;
};

// Partial specialization when TLeft/TRight are both floating points.
// This is a work-around because decltype(floatvar1 == floatvar2)
// will not compile with clang:
// error: comparing floating point with == or != is unsafe [-Werror,-Wfloat-equal]
template <typename TLeft, typename TRight>
struct SupportsEqualityOperatorImpl<TLeft, TRight, true> {
  static constexpr bool value = true;
};

// SupportsEqualityOperatorImpl<T1, T2>::value will evaluate to a boolean,
// whose value is true if T1 can be compared against T2 with ==,
// and false otherwise.
template <typename TLeft, typename TRight = TLeft>
struct SupportsEqualityOperator :  // NOLINT [whitespace/labels] [4]
    SupportsEqualityOperatorImpl<TLeft, TRight,
                                 std::is_floating_point<TLeft>::value
                                 && std::is_floating_point<TRight>::value> {
};

// Convert any kind of type to an std::string, even if there's no
// serialization support for it. Unknown types get converted to an
// an arbitrary value.
//
// Meant for printing user-visible errors or unit test failures only.
template <typename T>
std::string ToStringAny(const T& value,
                        typename std::enable_if<
                            SupportsInsertionOperator<T>::value>::type* = 0) {
  std::stringstream stream;
  stream << value;
  return stream.str();
}

template <typename T>
std::string ToStringAny(const std::vector<T> value,
                        typename std::enable_if<
                            SupportsInsertionOperator<T>::value>::type* = 0) {
  std::stringstream stream;
  stream << "vector{";

  for (size_t i = 0; i < value.size(); ++i) {
    stream << ToStringAny(value[i]);

    if (i != value.size() - 1) {
      stream << ',';
    }
  }

  stream << "}";
  return stream.str();
}

template <typename T>
std::string ToStringAny(const T&,
                        typename std::enable_if<
                            !SupportsInsertionOperator<T>::value>::type* = 0
) {
  return std::string("(unknown type [no operator<< implemented] for )");
}
}  // namespace detail  // NOLINT [readability/namespace] [5]
}  // namespace art

#endif  // ART_CMDLINE_DETAIL_CMDLINE_PARSER_DETAIL_H_
