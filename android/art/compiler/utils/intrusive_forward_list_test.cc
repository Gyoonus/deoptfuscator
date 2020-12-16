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
#include <vector>

#include "gtest/gtest.h"
#include "intrusive_forward_list.h"

namespace art {

struct IFLTestValue : public IntrusiveForwardListNode<IFLTestValue> {
  // Deliberately not explicit.
  IFLTestValue(int v) : value(v) { }  // NOLINT(runtime/explicit)

  int value;
};
using IFLTestValueList = IntrusiveForwardList<IFLTestValue>;
using ConstIFLTestValueList = IntrusiveForwardList<const IFLTestValue>;

bool operator==(const IFLTestValue& lhs, const IFLTestValue& rhs) {
  return lhs.value == rhs.value;
}

bool operator<(const IFLTestValue& lhs, const IFLTestValue& rhs) {
  return lhs.value < rhs.value;
}

struct IFLTestValue2 {
  // Deliberately not explicit.
  IFLTestValue2(int v) : hook(), value(v) { }  // NOLINT(runtime/explicit)

  IntrusiveForwardListHook hook;
  int value;
};
using IFLTestValue2List =
    IntrusiveForwardList<IFLTestValue2, IntrusiveForwardListMemberHookTraits<IFLTestValue2>>;

bool operator==(const IFLTestValue2& lhs, const IFLTestValue2& rhs) {
  return lhs.value == rhs.value;
}

bool operator<(const IFLTestValue2& lhs, const IFLTestValue2& rhs) {
  return lhs.value < rhs.value;
}

#define ASSERT_LISTS_EQUAL(expected, value)                                         \
  do {                                                                              \
    ASSERT_EQ((expected).empty(), (value).empty());                                 \
    ASSERT_EQ(std::distance((expected).begin(), (expected).end()),                  \
              std::distance((value).begin(), (value).end()));                       \
    ASSERT_TRUE(std::equal((expected).begin(), (expected).end(), (value).begin())); \
  } while (false)

class IntrusiveForwardListTest : public testing::Test {
 public:
  template <typename ListType>
  void IteratorToConstIterator();

  template <typename ListType>
  void IteratorOperators();

  template <typename ListType>
  void ConstructRange();

  template <typename ListType>
  void Assign();

  template <typename ListType>
  void PushPop();

  template <typename ListType>
  void InsertAfter1();

  template <typename ListType>
  void InsertAfter2();

  template <typename ListType>
  void EraseAfter1();

  template <typename ListType>
  void EraseAfter2();

  template <typename ListType>
  void SwapClear();

  template <typename ListType>
  void SpliceAfter();

  template <typename ListType>
  void Remove();

  template <typename ListType>
  void Unique();

  template <typename ListType>
  void Merge();

  template <typename ListType>
  void Sort1();

  template <typename ListType>
  void Sort2();

  template <typename ListType>
  void Reverse();

  template <typename ListType>
  void ModifyValue();
};

template <typename ListType>
void IntrusiveForwardListTest::IteratorToConstIterator() {
  ListType ifl;
  typename ListType::iterator begin = ifl.begin();
  typename ListType::const_iterator cbegin = ifl.cbegin();
  typename ListType::const_iterator converted_begin = begin;
  ASSERT_TRUE(converted_begin == cbegin);
}

TEST_F(IntrusiveForwardListTest, IteratorToConstIterator) {
  IteratorToConstIterator<IFLTestValueList>();
  IteratorToConstIterator<ConstIFLTestValueList>();
  IteratorToConstIterator<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::IteratorOperators() {
  using ValueType = typename ListType::value_type;
  ListType ifl;
  ASSERT_TRUE(ifl.begin() == ifl.cbegin());
  ASSERT_FALSE(ifl.begin() != ifl.cbegin());
  ASSERT_TRUE(ifl.end() == ifl.cend());
  ASSERT_FALSE(ifl.end() != ifl.cend());

  ASSERT_TRUE(ifl.begin() == ifl.end());  // Empty.
  ASSERT_FALSE(ifl.begin() != ifl.end());  // Empty.

  ValueType value(1);
  ifl.insert_after(ifl.cbefore_begin(), value);

  ASSERT_FALSE(ifl.begin() == ifl.end());  // Not empty.
  ASSERT_TRUE(ifl.begin() != ifl.end());  // Not empty.
}

TEST_F(IntrusiveForwardListTest, IteratorOperators) {
  IteratorOperators<IFLTestValueList>();
  IteratorOperators<ConstIFLTestValueList>();
  IteratorOperators<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::ConstructRange() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 1, 2, 7 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
}

TEST_F(IntrusiveForwardListTest, ConstructRange) {
  ConstructRange<IFLTestValueList>();
  ConstructRange<ConstIFLTestValueList>();
  ConstructRange<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Assign() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref1({ 2, 8, 5 });
  std::vector<ValueType> storage1(ref1.begin(), ref1.end());
  ListType ifl;
  ifl.assign(storage1.begin(), storage1.end());
  ASSERT_LISTS_EQUAL(ref1, ifl);
  std::forward_list<int> ref2({ 7, 1, 3 });
  std::vector<ValueType> storage2(ref2.begin(), ref2.end());
  ifl.assign(storage2.begin(), storage2.end());
  ASSERT_LISTS_EQUAL(ref2, ifl);
}

TEST_F(IntrusiveForwardListTest, Assign) {
  Assign<IFLTestValueList>();
  Assign<ConstIFLTestValueList>();
  Assign<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::PushPop() {
  using ValueType = typename ListType::value_type;
  ValueType value3(3);
  ValueType value7(7);
  std::forward_list<int> ref;
  ListType ifl;
  ASSERT_LISTS_EQUAL(ref, ifl);
  ref.push_front(3);
  ifl.push_front(value3);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(3, ifl.front());
  ref.push_front(7);
  ifl.push_front(value7);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(7, ifl.front());
  ref.pop_front();
  ifl.pop_front();
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(3, ifl.front());
  ref.pop_front();
  ifl.pop_front();
  ASSERT_LISTS_EQUAL(ref, ifl);
}

TEST_F(IntrusiveForwardListTest, PushPop) {
  PushPop<IFLTestValueList>();
  PushPop<ConstIFLTestValueList>();
  PushPop<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::InsertAfter1() {
  using ValueType = typename ListType::value_type;
  ValueType value4(4);
  ValueType value8(8);
  ValueType value5(5);
  ValueType value3(3);
  std::forward_list<int> ref;
  ListType ifl;

  auto ref_it = ref.insert_after(ref.before_begin(), 4);
  auto ifl_it = ifl.insert_after(ifl.before_begin(), value4);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);
  CHECK(ref_it == ref.begin());
  ASSERT_TRUE(ifl_it == ifl.begin());

  ref_it = ref.insert_after(ref.begin(), 8);
  ifl_it = ifl.insert_after(ifl.begin(), value8);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);
  CHECK(ref_it != ref.end());
  ASSERT_TRUE(ifl_it != ifl.end());
  CHECK(++ref_it == ref.end());
  ASSERT_TRUE(++ifl_it == ifl.end());

  ref_it = ref.insert_after(ref.begin(), 5);
  ifl_it = ifl.insert_after(ifl.begin(), value5);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);

  ref_it = ref.insert_after(ref_it, 3);
  ifl_it = ifl.insert_after(ifl_it, value3);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);
}

TEST_F(IntrusiveForwardListTest, InsertAfter1) {
  InsertAfter1<IFLTestValueList>();
  InsertAfter1<ConstIFLTestValueList>();
  InsertAfter1<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::InsertAfter2() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref;
  ListType ifl;

  auto ref_it = ref.insert_after(ref.before_begin(), { 2, 8, 5 });
  std::vector<ValueType> storage1({ { 2 }, { 8 }, { 5 } });
  auto ifl_it = ifl.insert_after(ifl.before_begin(), storage1.begin(), storage1.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);

  std::vector<ValueType> storage2({ { 7 }, { 2 } });
  ref_it = ref.insert_after(ref.begin(), { 7, 2 });
  ifl_it = ifl.insert_after(ifl.begin(), storage2.begin(), storage2.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(*ref_it, *ifl_it);

  std::vector<ValueType> storage3({ { 1 }, { 3 }, { 4 }, { 9 } });
  ref_it = ref.begin();
  ifl_it = ifl.begin();
  std::advance(ref_it, std::distance(ref.begin(), ref.end()) - 1);
  std::advance(ifl_it, std::distance(ifl.begin(), ifl.end()) - 1);
  ref_it = ref.insert_after(ref_it, { 1, 3, 4, 9 });
  ifl_it = ifl.insert_after(ifl_it, storage3.begin(), storage3.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
}

TEST_F(IntrusiveForwardListTest, InsertAfter2) {
  InsertAfter2<IFLTestValueList>();
  InsertAfter2<ConstIFLTestValueList>();
  InsertAfter2<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::EraseAfter1() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 1, 2, 7, 4, 5 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 5);

  auto ref_it = ref.begin();
  auto ifl_it = ifl.begin();
  std::advance(ref_it, 2);
  std::advance(ifl_it, 2);
  ref_it = ref.erase_after(ref_it);
  ifl_it = ifl.erase_after(ifl_it);
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 4);
  CHECK(ref_it != ref.end());
  ASSERT_TRUE(ifl_it != ifl.end());
  CHECK(++ref_it == ref.end());
  ASSERT_TRUE(++ifl_it == ifl.end());

  ref_it = ref.begin();
  ifl_it = ifl.begin();
  std::advance(ref_it, 2);
  std::advance(ifl_it, 2);
  ref_it = ref.erase_after(ref_it);
  ifl_it = ifl.erase_after(ifl_it);
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 3);
  CHECK(ref_it == ref.end());
  ASSERT_TRUE(ifl_it == ifl.end());

  ref_it = ref.erase_after(ref.begin());
  ifl_it = ifl.erase_after(ifl.begin());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 2);
  CHECK(ref_it != ref.end());
  ASSERT_TRUE(ifl_it != ifl.end());
  CHECK(++ref_it == ref.end());
  ASSERT_TRUE(++ifl_it == ifl.end());

  ref_it = ref.erase_after(ref.before_begin());
  ifl_it = ifl.erase_after(ifl.before_begin());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 1);
  CHECK(ref_it == ref.begin());
  ASSERT_TRUE(ifl_it == ifl.begin());

  ref_it = ref.erase_after(ref.before_begin());
  ifl_it = ifl.erase_after(ifl.before_begin());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 0);
  CHECK(ref_it == ref.begin());
  ASSERT_TRUE(ifl_it == ifl.begin());
}

TEST_F(IntrusiveForwardListTest, EraseAfter1) {
  EraseAfter1<IFLTestValueList>();
  EraseAfter1<ConstIFLTestValueList>();
  EraseAfter1<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::EraseAfter2() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 1, 2, 7, 4, 5, 3, 2, 8, 9 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 9);

  auto ref_it = ref.begin();
  auto ifl_it = ifl.begin();
  std::advance(ref_it, 3);
  std::advance(ifl_it, 3);
  ref_it = ref.erase_after(ref.begin(), ref_it);
  ifl_it = ifl.erase_after(ifl.begin(), ifl_it);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ASSERT_EQ(std::distance(ref.begin(), ref_it), std::distance(ifl.begin(), ifl_it));
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 7);

  ref_it = ref.erase_after(ref_it, ref.end());
  ifl_it = ifl.erase_after(ifl_it, ifl.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK(ref_it == ref.end());
  ASSERT_TRUE(ifl_it == ifl.end());
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 2);

  ref_it = ref.erase_after(ref.before_begin(), ref.end());
  ifl_it = ifl.erase_after(ifl.before_begin(), ifl.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK(ref_it == ref.end());
  ASSERT_TRUE(ifl_it == ifl.end());
  CHECK_EQ(std::distance(ref.begin(), ref.end()), 0);
}

TEST_F(IntrusiveForwardListTest, EraseAfter2) {
  EraseAfter2<IFLTestValueList>();
  EraseAfter2<ConstIFLTestValueList>();
  EraseAfter2<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::SwapClear() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref1({ 1, 2, 7 });
  std::vector<ValueType> storage1(ref1.begin(), ref1.end());
  ListType ifl1(storage1.begin(), storage1.end());
  std::forward_list<int> ref2({ 3, 8, 6 });
  std::vector<ValueType> storage2(ref2.begin(), ref2.end());
  ListType ifl2(storage2.begin(), storage2.end());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  ref1.swap(ref2);
  ifl1.swap(ifl2);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  ref1.clear();
  ifl1.clear();
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  swap(ref1, ref2);
  swap(ifl1, ifl2);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  ref1.clear();
  ifl1.clear();
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
}

TEST_F(IntrusiveForwardListTest, SwapClear) {
  SwapClear<IFLTestValueList>();
  SwapClear<ConstIFLTestValueList>();
  SwapClear<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::SpliceAfter() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref1({ 3, 1, 2, 7, 4, 5, 4, 8, 7 });
  std::forward_list<int> ref2;
  std::vector<ValueType> storage(ref1.begin(), ref1.end());
  ListType ifl1(storage.begin(), storage.end());
  ListType ifl2;
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move everything to ref2/ifl2.
  ref2.splice_after(ref2.before_begin(), ref1);
  ifl2.splice_after(ifl2.before_begin(), ifl1);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move first element (3) to ref1/ifl1.
  ref1.splice_after(ref1.before_begin(), ref2, ref2.before_begin());
  ifl1.splice_after(ifl1.before_begin(), ifl2, ifl2.before_begin());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move second element (2) to ref1/ifl1 after the first element (3).
  ref1.splice_after(ref1.begin(), ref2, ref2.begin());
  ifl1.splice_after(ifl1.begin(), ifl2, ifl2.begin());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move everything from ref2/ifl2 between the 2 elements now in ref1/ifl1.
  ref1.splice_after(ref1.begin(), ref2);
  ifl1.splice_after(ifl1.begin(), ifl2);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  std::forward_list<int> check({ 3, 1, 7, 4, 5, 4, 8, 7, 2 });
  ASSERT_LISTS_EQUAL(check, ifl1);
  ASSERT_TRUE(ifl2.empty());

  // Empty splice_after().
  ref2.splice_after(
      ref2.before_begin(), ref1, ref1.before_begin(), ref1.begin());
  ifl2.splice_after(ifl2.before_begin(), ifl1, ifl1.before_begin(), ifl1.begin());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move { 1, 7 } to ref2/ifl2.
  auto ref_it = ref1.begin();
  auto ifl_it = ifl1.begin();
  std::advance(ref_it, 3);
  std::advance(ifl_it, 3);
  ref2.splice_after(ref2.before_begin(), ref1, ref1.begin(), ref_it);
  ifl2.splice_after(ifl2.before_begin(), ifl1, ifl1.begin(), ifl_it);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  // Move { 8, 7, 2 } to the beginning of ref1/ifl1.
  ref_it = ref1.begin();
  ifl_it = ifl1.begin();
  std::advance(ref_it, 3);
  std::advance(ifl_it, 3);
  ref1.splice_after(ref1.before_begin(), ref1, ref_it, ref1.end());
  ifl1.splice_after(ifl1.before_begin(), ifl1, ifl_it, ifl1.end());
  ASSERT_LISTS_EQUAL(ref1, ifl1);

  check.assign({ 8, 7, 2, 3, 4, 5, 4 });
  ASSERT_LISTS_EQUAL(check, ifl1);
  check.assign({ 1, 7 });
  ASSERT_LISTS_EQUAL(check, ifl2);

  // Move all but the first element to ref2/ifl2.
  ref_it = ref2.begin();
  ifl_it = ifl2.begin();
  std::advance(ref_it, 1);
  std::advance(ifl_it, 1);
  ref2.splice_after(ref_it, ref1, ref1.begin(), ref1.end());
  ifl2.splice_after(ifl_it, ifl1, ifl1.begin(), ifl1.end());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);

  check.assign({8});
  ASSERT_LISTS_EQUAL(check, ifl1);

  // Move the first element of ref1/ifl1 to the beginning of ref1/ifl1 (do nothing).
  ref1.splice_after(ref1.before_begin(), ref1, ref1.before_begin());
  ifl1.splice_after(ifl1.before_begin(), ifl1, ifl1.before_begin());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(check, ifl1);

  // Move the first element of ref2/ifl2 after itself (do nothing).
  ref1.splice_after(ref1.begin(), ref1, ref1.before_begin());
  ifl1.splice_after(ifl1.begin(), ifl1, ifl1.before_begin());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(check, ifl1);

  check.assign({ 1, 7, 7, 2, 3, 4, 5, 4 });
  ASSERT_LISTS_EQUAL(check, ifl2);

  // Move the first element of ref2/ifl2 to the beginning of ref2/ifl2 (do nothing).
  ref2.splice_after(ref2.before_begin(), ref2, ref2.before_begin());
  ifl2.splice_after(ifl2.before_begin(), ifl2, ifl2.before_begin());
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  ASSERT_LISTS_EQUAL(check, ifl2);

  // Move the first element of ref2/ifl2 after itself (do nothing).
  ref2.splice_after(ref2.begin(), ref2, ref2.before_begin());
  ifl2.splice_after(ifl2.begin(), ifl2, ifl2.before_begin());
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  ASSERT_LISTS_EQUAL(check, ifl2);
}

TEST_F(IntrusiveForwardListTest, SpliceAfter) {
  SpliceAfter<IFLTestValueList>();
  SpliceAfter<ConstIFLTestValueList>();
  SpliceAfter<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Remove() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 3, 1, 2, 7, 4, 5, 4, 8, 7 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  ref.remove(1);
  ifl.remove(1);
  ASSERT_LISTS_EQUAL(ref, ifl);
  ref.remove(4);
  ifl.remove(4);
  ASSERT_LISTS_EQUAL(ref, ifl);
  auto odd = [](ValueType value) { return (value.value & 1) != 0; };
  ref.remove_if(odd);
  ifl.remove_if(odd);
  ASSERT_LISTS_EQUAL(ref, ifl);
  auto all = [](ValueType value ATTRIBUTE_UNUSED) { return true; };
  ref.remove_if(all);
  ifl.remove_if(all);
  ASSERT_LISTS_EQUAL(ref, ifl);
}

TEST_F(IntrusiveForwardListTest, Remove) {
  Remove<IFLTestValueList>();
  Remove<ConstIFLTestValueList>();
  Remove<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Unique() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 3, 1, 1, 2, 3, 3, 7, 7, 4, 4, 5, 7 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  ref.unique();
  ifl.unique();
  ASSERT_LISTS_EQUAL(ref, ifl);
  std::forward_list<int> check({ 3, 1, 2, 3, 7, 4, 5, 7 });
  ASSERT_LISTS_EQUAL(check, ifl);

  auto bin_pred = [](const ValueType& lhs, const ValueType& rhs) {
    return (lhs.value & ~1) == (rhs.value & ~1);
  };
  ref.unique(bin_pred);
  ifl.unique(bin_pred);
  ASSERT_LISTS_EQUAL(ref, ifl);
  check.assign({ 3, 1, 2, 7, 4, 7 });
  ASSERT_LISTS_EQUAL(check, ifl);
}

TEST_F(IntrusiveForwardListTest, Unique) {
  Unique<IFLTestValueList>();
  Unique<ConstIFLTestValueList>();
  Unique<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Merge() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref1({ 1, 4, 8, 8, 12 });
  std::vector<ValueType> storage1(ref1.begin(), ref1.end());
  ListType ifl1(storage1.begin(), storage1.end());
  std::forward_list<int> ref2({ 3, 5, 6, 7, 9 });
  std::vector<ValueType> storage2(ref2.begin(), ref2.end());
  ListType ifl2(storage2.begin(), storage2.end());
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  CHECK(std::is_sorted(ref1.begin(), ref1.end()));
  CHECK(std::is_sorted(ref2.begin(), ref2.end()));
  ref1.merge(ref2);
  ifl1.merge(ifl2);
  ASSERT_LISTS_EQUAL(ref1, ifl1);
  ASSERT_LISTS_EQUAL(ref2, ifl2);
  CHECK(ref2.empty());
  std::forward_list<int> check({ 1, 3, 4, 5, 6, 7, 8, 8, 9, 12 });
  ASSERT_LISTS_EQUAL(check, ifl1);
}

TEST_F(IntrusiveForwardListTest, Merge) {
  Merge<IFLTestValueList>();
  Merge<ConstIFLTestValueList>();
  Merge<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Sort1() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 2, 9, 8, 3, 7, 4, 1, 5, 3, 0 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK(!std::is_sorted(ref.begin(), ref.end()));
  ref.sort();
  ifl.sort();
  ASSERT_LISTS_EQUAL(ref, ifl);
  std::forward_list<int> check({ 0, 1, 2, 3, 3, 4, 5, 7, 8, 9 });
  ASSERT_LISTS_EQUAL(check, ifl);
}

TEST_F(IntrusiveForwardListTest, Sort1) {
  Sort1<IFLTestValueList>();
  Sort1<ConstIFLTestValueList>();
  Sort1<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Sort2() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 2, 9, 8, 3, 7, 4, 1, 5, 3, 0 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  auto cmp = [](const ValueType& lhs, const ValueType& rhs) {
    return (lhs.value & ~1) < (rhs.value & ~1);
  };
  CHECK(!std::is_sorted(ref.begin(), ref.end(), cmp));
  ref.sort(cmp);
  ifl.sort(cmp);
  ASSERT_LISTS_EQUAL(ref, ifl);
  std::forward_list<int> check({ 1, 0, 2, 3, 3, 4, 5, 7, 9, 8 });
  ASSERT_LISTS_EQUAL(check, ifl);
}

TEST_F(IntrusiveForwardListTest, Sort2) {
  Sort2<IFLTestValueList>();
  Sort2<ConstIFLTestValueList>();
  Sort2<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::Reverse() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 8, 3, 5, 4, 1, 3 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);
  CHECK(!std::is_sorted(ref.begin(), ref.end()));
  ref.reverse();
  ifl.reverse();
  ASSERT_LISTS_EQUAL(ref, ifl);
  std::forward_list<int> check({ 3, 1, 4, 5, 3, 8 });
  ASSERT_LISTS_EQUAL(check, ifl);
}

TEST_F(IntrusiveForwardListTest, Reverse) {
  Reverse<IFLTestValueList>();
  Reverse<ConstIFLTestValueList>();
  Reverse<IFLTestValue2List>();
}

template <typename ListType>
void IntrusiveForwardListTest::ModifyValue() {
  using ValueType = typename ListType::value_type;
  std::forward_list<int> ref({ 3, 7, 42 });
  std::vector<ValueType> storage(ref.begin(), ref.end());
  ListType ifl(storage.begin(), storage.end());
  ASSERT_LISTS_EQUAL(ref, ifl);

  auto add1 = [](const ValueType& value) { return value.value + 1; };
  std::transform(ref.begin(), ref.end(), ref.begin(), add1);
  std::transform(ifl.begin(), ifl.end(), ifl.begin(), add1);
  ASSERT_LISTS_EQUAL(ref, ifl);
}

TEST_F(IntrusiveForwardListTest, ModifyValue) {
  ModifyValue<IFLTestValueList>();
  // Does not compile with ConstIFLTestValueList because LHS of the assignment is const.
  // ModifyValue<ConstIFLTestValueList>();
  static_assert(std::is_const<ConstIFLTestValueList::iterator::value_type>::value, "Const check.");
  ModifyValue<IFLTestValue2List>();
}

struct Tag1;
struct Tag2;
struct TwoListsValue : public IntrusiveForwardListNode<TwoListsValue, Tag1>,
                       public IntrusiveForwardListNode<TwoListsValue, Tag2> {
  // Deliberately not explicit.
  TwoListsValue(int v) : value(v) { }  // NOLINT(runtime/explicit)

  int value;
};
using FirstList =
    IntrusiveForwardList<TwoListsValue, IntrusiveForwardListBaseHookTraits<TwoListsValue, Tag1>>;
using SecondList =
    IntrusiveForwardList<TwoListsValue, IntrusiveForwardListBaseHookTraits<TwoListsValue, Tag2>>;

bool operator==(const TwoListsValue& lhs, const TwoListsValue& rhs) {
  return lhs.value == rhs.value;
}

TEST_F(IntrusiveForwardListTest, TwoLists) {
  // Test that a value can be in two lists at the same time and the hooks do not interfere.
  std::vector<TwoListsValue> storage({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });  // storage[i] = i

  std::vector<int> order1({ 3, 1, 7, 2, 8, 9, 4, 0, 6, 5 });
  FirstList list1;
  auto pos1 = list1.before_begin();
  for (size_t idx : order1) {
    pos1 = list1.insert_after(pos1, storage[idx]);
  }

  std::vector<int> order2({ 8, 5, 1, 6, 7, 2, 9, 3, 0, 4 });
  SecondList list2;
  auto pos2 = list2.before_begin();
  for (size_t idx : order2) {
    pos2 = list2.insert_after(pos2, storage[idx]);
  }

  // Using `storage[i] = i`, we can easily compare that nodes of each list are in the right order.
  ASSERT_LISTS_EQUAL(order1, list1);
  ASSERT_LISTS_EQUAL(order2, list2);
}

}  // namespace art
