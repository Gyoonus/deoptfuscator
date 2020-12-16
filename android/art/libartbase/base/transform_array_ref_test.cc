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

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "base/transform_array_ref.h"

namespace art {

namespace {  // anonymous namespace

struct ValueHolder {
  // Deliberately not explicit.
  ValueHolder(int v) : value(v) { }  // NOLINT
  int value;
};

ATTRIBUTE_UNUSED bool operator==(const ValueHolder& lhs, const ValueHolder& rhs) {
  return lhs.value == rhs.value;
}

}  // anonymous namespace

TEST(TransformArrayRef, ConstRefAdd1) {
  auto add1 = [](const ValueHolder& h) { return h.value + 1; };
  std::vector<ValueHolder> input({ 7, 6, 4, 0 });
  std::vector<int> output;

  auto taref = MakeTransformArrayRef(input, add1);
  using TarefIter = decltype(taref)::iterator;
  using ConstTarefIter = decltype(taref)::const_iterator;
  static_assert(std::is_same<int, decltype(taref)::value_type>::value, "value_type");
  static_assert(std::is_same<TarefIter, decltype(taref)::pointer>::value, "pointer");
  static_assert(std::is_same<int, decltype(taref)::reference>::value, "reference");
  static_assert(std::is_same<ConstTarefIter, decltype(taref)::const_pointer>::value,
                "const_pointer");
  static_assert(std::is_same<int, decltype(taref)::const_reference>::value, "const_reference");

  std::copy(taref.begin(), taref.end(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 7, 5, 1 }), output);
  output.clear();

  std::copy(taref.cbegin(), taref.cend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 7, 5, 1 }), output);
  output.clear();

  std::copy(taref.rbegin(), taref.rend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 5, 7, 8 }), output);
  output.clear();

  std::copy(taref.crbegin(), taref.crend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 5, 7, 8 }), output);
  output.clear();

  ASSERT_EQ(input.size(), taref.size());
  ASSERT_EQ(input.empty(), taref.empty());
  ASSERT_EQ(input.front().value + 1, taref.front());
  ASSERT_EQ(input.back().value + 1, taref.back());

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value + 1, taref[i]);
  }
}

TEST(TransformArrayRef, NonConstRefSub1) {
  auto sub1 = [](ValueHolder& h) { return h.value - 1; };
  std::vector<ValueHolder> input({ 4, 4, 5, 7, 10 });
  std::vector<int> output;

  auto taref = MakeTransformArrayRef(input, sub1);
  using TarefIter = decltype(taref)::iterator;
  static_assert(std::is_same<void, decltype(taref)::const_iterator>::value, "const_iterator");
  static_assert(std::is_same<int, decltype(taref)::value_type>::value, "value_type");
  static_assert(std::is_same<TarefIter, decltype(taref)::pointer>::value, "pointer");
  static_assert(std::is_same<int, decltype(taref)::reference>::value, "reference");
  static_assert(std::is_same<void, decltype(taref)::const_pointer>::value, "const_pointer");
  static_assert(std::is_same<void, decltype(taref)::const_reference>::value, "const_reference");

  std::copy(taref.begin(), taref.end(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 3, 3, 4, 6, 9 }), output);
  output.clear();

  std::copy(taref.rbegin(), taref.rend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 9, 6, 4, 3, 3 }), output);
  output.clear();

  ASSERT_EQ(input.size(), taref.size());
  ASSERT_EQ(input.empty(), taref.empty());
  ASSERT_EQ(input.front().value - 1, taref.front());
  ASSERT_EQ(input.back().value - 1, taref.back());

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value - 1, taref[i]);
  }
}

TEST(TransformArrayRef, ConstAndNonConstRef) {
  struct Ref {
    int& operator()(ValueHolder& h) const { return h.value; }
    const int& operator()(const ValueHolder& h) const { return h.value; }
  };
  Ref ref;
  std::vector<ValueHolder> input({ 1, 0, 1, 0, 3, 1 });
  std::vector<int> output;

  auto taref = MakeTransformArrayRef(input, ref);
  static_assert(std::is_same<int, decltype(taref)::value_type>::value, "value_type");
  static_assert(std::is_same<int*, decltype(taref)::pointer>::value, "pointer");
  static_assert(std::is_same<int&, decltype(taref)::reference>::value, "reference");
  static_assert(std::is_same<const int*, decltype(taref)::const_pointer>::value, "const_pointer");
  static_assert(std::is_same<const int&, decltype(taref)::const_reference>::value,
                "const_reference");

  std::copy(taref.begin(), taref.end(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 0, 1, 0, 3, 1 }), output);
  output.clear();

  std::copy(taref.cbegin(), taref.cend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 0, 1, 0, 3, 1 }), output);
  output.clear();

  std::copy(taref.rbegin(), taref.rend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 3, 0, 1, 0, 1 }), output);
  output.clear();

  std::copy(taref.crbegin(), taref.crend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 3, 0, 1, 0, 1 }), output);
  output.clear();

  ASSERT_EQ(input.size(), taref.size());
  ASSERT_EQ(input.empty(), taref.empty());
  ASSERT_EQ(input.front().value, taref.front());
  ASSERT_EQ(input.back().value, taref.back());

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value, taref[i]);
  }

  // Test writing through the transform iterator.
  std::vector<int> transform_input({ 24, 37, 11, 71 });
  std::vector<ValueHolder> transformed(transform_input.size(), 0);
  taref = MakeTransformArrayRef(transformed, ref);
  for (size_t i = 0; i != transform_input.size(); ++i) {
    taref[i] = transform_input[i];
  }
  ASSERT_EQ(std::vector<ValueHolder>({ 24, 37, 11, 71 }), transformed);

  const std::vector<ValueHolder>& cinput = input;

  auto ctaref = MakeTransformArrayRef(cinput, ref);
  static_assert(std::is_same<int, decltype(ctaref)::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, decltype(ctaref)::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, decltype(ctaref)::reference>::value, "reference");
  static_assert(std::is_same<const int*, decltype(ctaref)::const_pointer>::value, "const_pointer");
  static_assert(std::is_same<const int&, decltype(ctaref)::const_reference>::value,
                "const_reference");

  std::copy(ctaref.begin(), ctaref.end(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 0, 1, 0, 3, 1 }), output);
  output.clear();

  std::copy(ctaref.cbegin(), ctaref.cend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 0, 1, 0, 3, 1 }), output);
  output.clear();

  std::copy(ctaref.rbegin(), ctaref.rend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 3, 0, 1, 0, 1 }), output);
  output.clear();

  std::copy(ctaref.crbegin(), ctaref.crend(), std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 3, 0, 1, 0, 1 }), output);
  output.clear();

  ASSERT_EQ(cinput.size(), ctaref.size());
  ASSERT_EQ(cinput.empty(), ctaref.empty());
  ASSERT_EQ(cinput.front().value, ctaref.front());
  ASSERT_EQ(cinput.back().value, ctaref.back());

  for (size_t i = 0; i != cinput.size(); ++i) {
    ASSERT_EQ(cinput[i].value, ctaref[i]);
  }

  // Test conversion adding const.
  decltype(ctaref) ctaref2 = taref;
  ASSERT_EQ(taref.size(), ctaref2.size());
  for (size_t i = 0; i != taref.size(); ++i) {
    ASSERT_EQ(taref[i], ctaref2[i]);
  }
}

}  // namespace art
