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
#include <forward_list>
#include <list>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

#include "base/transform_iterator.h"

namespace art {

namespace {  // anonymous namespace

struct ValueHolder {
  // Deliberately not explicit.
  ValueHolder(int v) : value(v) { }  // NOLINT
  int value;
};

bool operator==(const ValueHolder& lhs, const ValueHolder& rhs) {
  return lhs.value == rhs.value;
}

}  // anonymous namespace

TEST(TransformIterator, VectorAdd1) {
  auto add1 = [](const ValueHolder& h) { return h.value + 1; };
  std::vector<ValueHolder> input({ 1, 7, 3, 8 });
  std::vector<int> output;

  using vector_titer = decltype(MakeTransformIterator(input.begin(), add1));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_titer::value_type>::value, "value_type");
  static_assert(std::is_same<vector_titer, vector_titer::pointer>::value, "pointer");
  static_assert(std::is_same<int, vector_titer::reference>::value, "reference");

  using vector_ctiter = decltype(MakeTransformIterator(input.cbegin(), add1));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_ctiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_ctiter::value_type>::value, "value_type");
  static_assert(std::is_same<vector_ctiter, vector_ctiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, vector_ctiter::reference>::value, "reference");

  using vector_rtiter = decltype(MakeTransformIterator(input.rbegin(), add1));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_rtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_rtiter::value_type>::value, "value_type");
  static_assert(std::is_same<vector_rtiter, vector_rtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, vector_rtiter::reference>::value, "reference");

  using vector_crtiter = decltype(MakeTransformIterator(input.crbegin(), add1));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_crtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_crtiter::value_type>::value, "value_type");
  static_assert(std::is_same<vector_crtiter, vector_crtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, vector_crtiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), add1),
            MakeTransformIterator(input.end(), add1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 2, 8, 4, 9 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.cbegin(), add1),
            MakeTransformIterator(input.cend(), add1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 2, 8, 4, 9 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.rbegin(), add1),
            MakeTransformIterator(input.rend(), add1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 9, 4, 8, 2 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.crbegin(), add1),
            MakeTransformIterator(input.crend(), add1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 9, 4, 8, 2 }), output);
  output.clear();

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.begin(), add1)[i]);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.cbegin(), add1)[i]);
    ptrdiff_t index_from_rbegin = static_cast<ptrdiff_t>(input.size() - i - 1u);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.rbegin(), add1)[index_from_rbegin]);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.crbegin(), add1)[index_from_rbegin]);
    ptrdiff_t index_from_end = -static_cast<ptrdiff_t>(input.size() - i);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.end(), add1)[index_from_end]);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.cend(), add1)[index_from_end]);
    ptrdiff_t index_from_rend = -1 - static_cast<ptrdiff_t>(i);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.rend(), add1)[index_from_rend]);
    ASSERT_EQ(input[i].value + 1, MakeTransformIterator(input.crend(), add1)[index_from_rend]);

    ASSERT_EQ(MakeTransformIterator(input.begin(), add1) + i,
              MakeTransformIterator(input.begin() + i, add1));
    ASSERT_EQ(MakeTransformIterator(input.cbegin(), add1) + i,
              MakeTransformIterator(input.cbegin() + i, add1));
    ASSERT_EQ(MakeTransformIterator(input.rbegin(), add1) + i,
              MakeTransformIterator(input.rbegin() + i, add1));
    ASSERT_EQ(MakeTransformIterator(input.crbegin(), add1) + i,
              MakeTransformIterator(input.crbegin() + i, add1));
    ASSERT_EQ(MakeTransformIterator(input.end(), add1) - i,
              MakeTransformIterator(input.end() - i, add1));
    ASSERT_EQ(MakeTransformIterator(input.cend(), add1) - i,
              MakeTransformIterator(input.cend() - i, add1));
    ASSERT_EQ(MakeTransformIterator(input.rend(), add1) - i,
              MakeTransformIterator(input.rend() - i, add1));
    ASSERT_EQ(MakeTransformIterator(input.crend(), add1) - i,
              MakeTransformIterator(input.crend() - i, add1));
  }
  ASSERT_EQ(input.end(),
            (MakeTransformIterator(input.begin(), add1) + input.size()).base());
  ASSERT_EQ(MakeTransformIterator(input.end(), add1) - MakeTransformIterator(input.begin(), add1),
            static_cast<ptrdiff_t>(input.size()));

  // Test iterator->const_iterator conversion and comparison.
  auto it = MakeTransformIterator(input.begin(), add1);
  decltype(MakeTransformIterator(input.cbegin(), add1)) cit = it;
  static_assert(!std::is_same<decltype(it), decltype(cit)>::value, "Types must be different");
  ASSERT_EQ(it, cit);
  auto rit = MakeTransformIterator(input.rbegin(), add1);
  decltype(MakeTransformIterator(input.crbegin(), add1)) crit(rit);
  static_assert(!std::is_same<decltype(rit), decltype(crit)>::value, "Types must be different");
  ASSERT_EQ(rit, crit);
}

TEST(TransformIterator, ListSub1) {
  auto sub1 = [](const ValueHolder& h) { return h.value - 1; };
  std::list<ValueHolder> input({ 2, 3, 5, 7, 11 });
  std::vector<int> output;

  using list_titer = decltype(MakeTransformIterator(input.begin(), sub1));
  static_assert(std::is_same<std::bidirectional_iterator_tag,
                             list_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, list_titer::value_type>::value, "value_type");
  static_assert(std::is_same<list_titer, list_titer::pointer>::value, "pointer");
  static_assert(std::is_same<int, list_titer::reference>::value, "reference");

  using list_ctiter = decltype(MakeTransformIterator(input.cbegin(), sub1));
  static_assert(std::is_same<std::bidirectional_iterator_tag,
                             list_ctiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, list_ctiter::value_type>::value, "value_type");
  static_assert(std::is_same<list_ctiter, list_ctiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, list_ctiter::reference>::value, "reference");

  using list_rtiter = decltype(MakeTransformIterator(input.rbegin(), sub1));
  static_assert(std::is_same<std::bidirectional_iterator_tag,
                             list_rtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, list_rtiter::value_type>::value, "value_type");
  static_assert(std::is_same<list_rtiter, list_rtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, list_rtiter::reference>::value, "reference");

  using list_crtiter = decltype(MakeTransformIterator(input.crbegin(), sub1));
  static_assert(std::is_same<std::bidirectional_iterator_tag,
                             list_crtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, list_crtiter::value_type>::value, "value_type");
  static_assert(std::is_same<list_crtiter, list_crtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, list_crtiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), sub1),
            MakeTransformIterator(input.end(), sub1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 2, 4, 6, 10 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.cbegin(), sub1),
            MakeTransformIterator(input.cend(), sub1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 1, 2, 4, 6, 10 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.rbegin(), sub1),
            MakeTransformIterator(input.rend(), sub1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 10, 6, 4, 2, 1 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.crbegin(), sub1),
            MakeTransformIterator(input.crend(), sub1),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 10, 6, 4, 2, 1  }), output);
  output.clear();

  // Test iterator->const_iterator conversion and comparison.
  auto it = MakeTransformIterator(input.begin(), sub1);
  decltype(MakeTransformIterator(input.cbegin(), sub1)) cit = it;
  static_assert(!std::is_same<decltype(it), decltype(cit)>::value, "Types must be different");
  ASSERT_EQ(it, cit);
}

TEST(TransformIterator, ForwardListSub1) {
  auto mul3 = [](const ValueHolder& h) { return h.value * 3; };
  std::forward_list<ValueHolder> input({ 1, 1, 2, 3, 5, 8 });
  std::vector<int> output;

  using flist_titer = decltype(MakeTransformIterator(input.begin(), mul3));
  static_assert(std::is_same<std::forward_iterator_tag,
                             flist_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, flist_titer::value_type>::value, "value_type");
  static_assert(std::is_same<flist_titer, flist_titer::pointer>::value, "pointer");
  static_assert(std::is_same<int, flist_titer::reference>::value, "reference");

  using flist_ctiter = decltype(MakeTransformIterator(input.cbegin(), mul3));
  static_assert(std::is_same<std::forward_iterator_tag,
                             flist_ctiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, flist_ctiter::value_type>::value, "value_type");
  static_assert(std::is_same<flist_ctiter, flist_ctiter::pointer>::value, "pointer");
  static_assert(std::is_same<int, flist_ctiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), mul3),
            MakeTransformIterator(input.end(), mul3),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 3, 3, 6, 9, 15, 24 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.cbegin(), mul3),
            MakeTransformIterator(input.cend(), mul3),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 3, 3, 6, 9, 15, 24 }), output);
  output.clear();

  // Test iterator->const_iterator conversion and comparison.
  auto it = MakeTransformIterator(input.begin(), mul3);
  decltype(MakeTransformIterator(input.cbegin(), mul3)) cit = it;
  static_assert(!std::is_same<decltype(it), decltype(cit)>::value, "Types must be different");
  ASSERT_EQ(it, cit);
}

TEST(TransformIterator, VectorConstReference) {
  auto ref = [](const ValueHolder& h) -> const int& { return h.value; };
  std::vector<ValueHolder> input({ 7, 3, 1, 2, 4, 8 });
  std::vector<int> output;

  using vector_titer = decltype(MakeTransformIterator(input.begin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_titer::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_titer::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_titer::reference>::value, "reference");

  using vector_ctiter = decltype(MakeTransformIterator(input.cbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_ctiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_ctiter::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_ctiter::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_ctiter::reference>::value, "reference");

  using vector_rtiter = decltype(MakeTransformIterator(input.rbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_rtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_rtiter::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_rtiter::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_rtiter::reference>::value, "reference");

  using vector_crtiter = decltype(MakeTransformIterator(input.crbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_crtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_crtiter::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_crtiter::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_crtiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), ref),
            MakeTransformIterator(input.end(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 7, 3, 1, 2, 4, 8 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.cbegin(), ref),
            MakeTransformIterator(input.cend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 7, 3, 1, 2, 4, 8 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.rbegin(), ref),
            MakeTransformIterator(input.rend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 4, 2, 1, 3, 7 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.crbegin(), ref),
            MakeTransformIterator(input.crend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 4, 2, 1, 3, 7 }), output);
  output.clear();

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.begin(), ref)[i]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.cbegin(), ref)[i]);
    ptrdiff_t index_from_rbegin = static_cast<ptrdiff_t>(input.size() - i - 1u);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rbegin(), ref)[index_from_rbegin]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.crbegin(), ref)[index_from_rbegin]);
    ptrdiff_t index_from_end = -static_cast<ptrdiff_t>(input.size() - i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.end(), ref)[index_from_end]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.cend(), ref)[index_from_end]);
    ptrdiff_t index_from_rend = -1 - static_cast<ptrdiff_t>(i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rend(), ref)[index_from_rend]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.crend(), ref)[index_from_rend]);

    ASSERT_EQ(MakeTransformIterator(input.begin(), ref) + i,
              MakeTransformIterator(input.begin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.cbegin(), ref) + i,
              MakeTransformIterator(input.cbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rbegin(), ref) + i,
              MakeTransformIterator(input.rbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.crbegin(), ref) + i,
              MakeTransformIterator(input.crbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.end(), ref) - i,
              MakeTransformIterator(input.end() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.cend(), ref) - i,
              MakeTransformIterator(input.cend() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rend(), ref) - i,
              MakeTransformIterator(input.rend() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.crend(), ref) - i,
              MakeTransformIterator(input.crend() - i, ref));
  }
  ASSERT_EQ(input.end(),
            (MakeTransformIterator(input.begin(), ref) + input.size()).base());
  ASSERT_EQ(MakeTransformIterator(input.end(), ref) - MakeTransformIterator(input.begin(), ref),
            static_cast<ptrdiff_t>(input.size()));
}

TEST(TransformIterator, VectorNonConstReference) {
  auto ref = [](ValueHolder& h) -> int& { return h.value; };
  std::vector<ValueHolder> input({ 7, 3, 1, 2, 4, 8 });
  std::vector<int> output;

  using vector_titer = decltype(MakeTransformIterator(input.begin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_titer::value_type>::value, "value_type");
  static_assert(std::is_same<int*, vector_titer::pointer>::value, "pointer");
  static_assert(std::is_same<int&, vector_titer::reference>::value, "reference");

  using vector_rtiter = decltype(MakeTransformIterator(input.rbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_rtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_rtiter::value_type>::value, "value_type");
  static_assert(std::is_same<int*, vector_rtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int&, vector_rtiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), ref),
            MakeTransformIterator(input.end(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 7, 3, 1, 2, 4, 8 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.rbegin(), ref),
            MakeTransformIterator(input.rend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 4, 2, 1, 3, 7 }), output);
  output.clear();

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.begin(), ref)[i]);
    ptrdiff_t index_from_rbegin = static_cast<ptrdiff_t>(input.size() - i - 1u);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rbegin(), ref)[index_from_rbegin]);
    ptrdiff_t index_from_end = -static_cast<ptrdiff_t>(input.size() - i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.end(), ref)[index_from_end]);
    ptrdiff_t index_from_rend = -1 - static_cast<ptrdiff_t>(i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rend(), ref)[index_from_rend]);

    ASSERT_EQ(MakeTransformIterator(input.begin(), ref) + i,
              MakeTransformIterator(input.begin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rbegin(), ref) + i,
              MakeTransformIterator(input.rbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.end(), ref) - i,
              MakeTransformIterator(input.end() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rend(), ref) - i,
              MakeTransformIterator(input.rend() - i, ref));
  }
  ASSERT_EQ(input.end(),
            (MakeTransformIterator(input.begin(), ref) + input.size()).base());
  ASSERT_EQ(MakeTransformIterator(input.end(), ref) - MakeTransformIterator(input.begin(), ref),
            static_cast<ptrdiff_t>(input.size()));

  // Test writing through the transform iterator.
  std::list<int> transform_input({ 1, -1, 2, -2, 3, -3 });
  std::vector<ValueHolder> transformed(transform_input.size(), 0);
  std::transform(transform_input.begin(),
                 transform_input.end(),
                 MakeTransformIterator(transformed.begin(), ref),
                 [](int v) { return -2 * v; });
  ASSERT_EQ(std::vector<ValueHolder>({ -2, 2, -4, 4, -6, 6 }), transformed);
}

TEST(TransformIterator, VectorConstAndNonConstReference) {
  struct Ref {
    int& operator()(ValueHolder& h) const { return h.value; }
    const int& operator()(const ValueHolder& h) const { return h.value; }
  };
  Ref ref;
  std::vector<ValueHolder> input({ 7, 3, 1, 2, 4, 8 });
  std::vector<int> output;

  using vector_titer = decltype(MakeTransformIterator(input.begin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_titer::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_titer::value_type>::value, "value_type");
  static_assert(std::is_same<int*, vector_titer::pointer>::value, "pointer");
  static_assert(std::is_same<int&, vector_titer::reference>::value, "reference");

  using vector_ctiter = decltype(MakeTransformIterator(input.cbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_ctiter::iterator_category>::value, "category");
  // static_assert(std::is_same<int, vector_ctiter::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_ctiter::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_ctiter::reference>::value, "reference");

  using vector_rtiter = decltype(MakeTransformIterator(input.rbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_rtiter::iterator_category>::value, "category");
  static_assert(std::is_same<int, vector_rtiter::value_type>::value, "value_type");
  static_assert(std::is_same<int*, vector_rtiter::pointer>::value, "pointer");
  static_assert(std::is_same<int&, vector_rtiter::reference>::value, "reference");

  using vector_crtiter = decltype(MakeTransformIterator(input.crbegin(), ref));
  static_assert(std::is_same<std::random_access_iterator_tag,
                             vector_crtiter::iterator_category>::value, "category");
  // static_assert(std::is_same<int, vector_crtiter::value_type>::value, "value_type");
  static_assert(std::is_same<const int*, vector_crtiter::pointer>::value, "pointer");
  static_assert(std::is_same<const int&, vector_crtiter::reference>::value, "reference");

  std::copy(MakeTransformIterator(input.begin(), ref),
            MakeTransformIterator(input.end(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 7, 3, 1, 2, 4, 8 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.cbegin(), ref),
            MakeTransformIterator(input.cend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 7, 3, 1, 2, 4, 8 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.rbegin(), ref),
            MakeTransformIterator(input.rend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 4, 2, 1, 3, 7 }), output);
  output.clear();

  std::copy(MakeTransformIterator(input.crbegin(), ref),
            MakeTransformIterator(input.crend(), ref),
            std::back_inserter(output));
  ASSERT_EQ(std::vector<int>({ 8, 4, 2, 1, 3, 7 }), output);
  output.clear();

  for (size_t i = 0; i != input.size(); ++i) {
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.begin(), ref)[i]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.cbegin(), ref)[i]);
    ptrdiff_t index_from_rbegin = static_cast<ptrdiff_t>(input.size() - i - 1u);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rbegin(), ref)[index_from_rbegin]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.crbegin(), ref)[index_from_rbegin]);
    ptrdiff_t index_from_end = -static_cast<ptrdiff_t>(input.size() - i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.end(), ref)[index_from_end]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.cend(), ref)[index_from_end]);
    ptrdiff_t index_from_rend = -1 - static_cast<ptrdiff_t>(i);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.rend(), ref)[index_from_rend]);
    ASSERT_EQ(input[i].value, MakeTransformIterator(input.crend(), ref)[index_from_rend]);

    ASSERT_EQ(MakeTransformIterator(input.begin(), ref) + i,
              MakeTransformIterator(input.begin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.cbegin(), ref) + i,
              MakeTransformIterator(input.cbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rbegin(), ref) + i,
              MakeTransformIterator(input.rbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.crbegin(), ref) + i,
              MakeTransformIterator(input.crbegin() + i, ref));
    ASSERT_EQ(MakeTransformIterator(input.end(), ref) - i,
              MakeTransformIterator(input.end() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.cend(), ref) - i,
              MakeTransformIterator(input.cend() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.rend(), ref) - i,
              MakeTransformIterator(input.rend() - i, ref));
    ASSERT_EQ(MakeTransformIterator(input.crend(), ref) - i,
              MakeTransformIterator(input.crend() - i, ref));
  }
  ASSERT_EQ(input.end(),
            (MakeTransformIterator(input.begin(), ref) + input.size()).base());
  ASSERT_EQ(MakeTransformIterator(input.end(), ref) - MakeTransformIterator(input.begin(), ref),
            static_cast<ptrdiff_t>(input.size()));

  // Test iterator->const_iterator conversion and comparison.
  auto it = MakeTransformIterator(input.begin(), ref);
  decltype(MakeTransformIterator(input.cbegin(), ref)) cit = it;
  static_assert(!std::is_same<decltype(it), decltype(cit)>::value, "Types must be different");
  ASSERT_EQ(it, cit);
  auto rit = MakeTransformIterator(input.rbegin(), ref);
  decltype(MakeTransformIterator(input.crbegin(), ref)) crit(rit);
  static_assert(!std::is_same<decltype(rit), decltype(crit)>::value, "Types must be different");
  ASSERT_EQ(rit, crit);

  // Test writing through the transform iterator.
  std::list<int> transform_input({ 42, 73, 11, 17 });
  std::vector<ValueHolder> transformed(transform_input.size(), 0);
  std::transform(transform_input.begin(),
                 transform_input.end(),
                 MakeTransformIterator(transformed.begin(), ref),
                 [](int v) { return -v; });
  ASSERT_EQ(std::vector<ValueHolder>({ -42, -73, -11, -17 }), transformed);
}

TEST(TransformIterator, TransformRange) {
  auto ref = [](ValueHolder& h) -> int& { return h.value; };
  std::vector<ValueHolder> data({ 1, 0, 1, 3, 1, 0 });

  for (int& v : MakeTransformRange(data, ref)) {
    v += 11;
  }
  ASSERT_EQ(std::vector<ValueHolder>({ 12, 11, 12, 14, 12, 11 }), data);
}

}  // namespace art
