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

#ifndef ART_COMPILER_UTILS_INTRUSIVE_FORWARD_LIST_H_
#define ART_COMPILER_UTILS_INTRUSIVE_FORWARD_LIST_H_

#include <stdint.h>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>

#include <android-base/logging.h>

#include "base/casts.h"
#include "base/macros.h"

namespace art {

struct IntrusiveForwardListHook {
  IntrusiveForwardListHook() : next_hook(nullptr) { }
  explicit IntrusiveForwardListHook(const IntrusiveForwardListHook* hook) : next_hook(hook) { }

  // Allow copyable values but do not copy the hook, it is not part of the value.
  IntrusiveForwardListHook(const IntrusiveForwardListHook& other ATTRIBUTE_UNUSED)
      : next_hook(nullptr) { }
  IntrusiveForwardListHook& operator=(const IntrusiveForwardListHook& src ATTRIBUTE_UNUSED) {
    return *this;
  }

  mutable const IntrusiveForwardListHook* next_hook;
};

template <typename Derived, typename Tag = void>
struct IntrusiveForwardListNode : public IntrusiveForwardListHook {
};

template <typename T, IntrusiveForwardListHook T::* NextPtr = &T::hook>
class IntrusiveForwardListMemberHookTraits;

template <typename T, typename Tag = void>
class IntrusiveForwardListBaseHookTraits;

template <typename T,
          typename HookTraits =
              IntrusiveForwardListBaseHookTraits<typename std::remove_const<T>::type>>
class IntrusiveForwardList;

template <typename T, typename HookTraits>
class IntrusiveForwardListIterator : public std::iterator<std::forward_iterator_tag, T> {
 public:
  // Construct/copy/destroy (except the private constructor used by IntrusiveForwardList<>).
  IntrusiveForwardListIterator() : hook_(nullptr) { }
  IntrusiveForwardListIterator(const IntrusiveForwardListIterator& src) = default;
  IntrusiveForwardListIterator& operator=(const IntrusiveForwardListIterator& src) = default;

  // Conversion from iterator to const_iterator.
  template <typename OtherT,
            typename = typename std::enable_if<std::is_same<T, const OtherT>::value>::type>
  IntrusiveForwardListIterator(const IntrusiveForwardListIterator<OtherT, HookTraits>& src)  // NOLINT, implicit
      : hook_(src.hook_) { }

  // Iteration.
  IntrusiveForwardListIterator& operator++() {
    DCHECK(hook_ != nullptr);
    hook_ = hook_->next_hook;
    return *this;
  }
  IntrusiveForwardListIterator operator++(int) {
    IntrusiveForwardListIterator tmp(*this);
    ++*this;
    return tmp;
  }

  // Dereference
  T& operator*() const {
    DCHECK(hook_ != nullptr);
    return *HookTraits::GetValue(hook_);
  }
  T* operator->() const {
    return &**this;
  }

 private:
  explicit IntrusiveForwardListIterator(const IntrusiveForwardListHook* hook) : hook_(hook) { }

  const IntrusiveForwardListHook* hook_;

  template <typename OtherT, typename OtherTraits>
  friend class IntrusiveForwardListIterator;

  template <typename OtherT, typename OtherTraits>
  friend class IntrusiveForwardList;

  template <typename OtherT1, typename OtherT2, typename OtherTraits>
  friend typename std::enable_if<std::is_same<const OtherT1, const OtherT2>::value, bool>::type
  operator==(const IntrusiveForwardListIterator<OtherT1, OtherTraits>& lhs,
             const IntrusiveForwardListIterator<OtherT2, OtherTraits>& rhs);
};

template <typename T, typename OtherT, typename HookTraits>
typename std::enable_if<std::is_same<const T, const OtherT>::value, bool>::type operator==(
    const IntrusiveForwardListIterator<T, HookTraits>& lhs,
    const IntrusiveForwardListIterator<OtherT, HookTraits>& rhs) {
  return lhs.hook_ == rhs.hook_;
}

template <typename T, typename OtherT, typename HookTraits>
typename std::enable_if<std::is_same<const T, const OtherT>::value, bool>::type operator!=(
    const IntrusiveForwardListIterator<T, HookTraits>& lhs,
    const IntrusiveForwardListIterator<OtherT, HookTraits>& rhs) {
  return !(lhs == rhs);
}

// Intrusive version of std::forward_list<>. See also slist<> in Boost.Intrusive.
//
// This class template provides the same interface as std::forward_list<> as long
// as the functions are meaningful for an intrusive container; this excludes emplace
// functions and functions taking an std::initializer_list<> as the container does
// not construct elements.
template <typename T, typename HookTraits>
class IntrusiveForwardList {
 public:
  typedef HookTraits hook_traits;
  typedef       T  value_type;
  typedef       T& reference;
  typedef const T& const_reference;
  typedef       T* pointer;
  typedef const T* const_pointer;
  typedef IntrusiveForwardListIterator<      T, hook_traits> iterator;
  typedef IntrusiveForwardListIterator<const T, hook_traits> const_iterator;

  // Construct/copy/destroy.
  IntrusiveForwardList() = default;
  template <typename InputIterator>
  IntrusiveForwardList(InputIterator first, InputIterator last) : IntrusiveForwardList() {
    insert_after(before_begin(), first, last);
  }
  IntrusiveForwardList(IntrusiveForwardList&& src) : first_(src.first_.next_hook) {
    src.first_.next_hook = nullptr;
  }
  IntrusiveForwardList& operator=(const IntrusiveForwardList& src) = delete;
  IntrusiveForwardList& operator=(IntrusiveForwardList&& src) {
    IntrusiveForwardList tmp(std::move(src));
    tmp.swap(*this);
    return *this;
  }
  ~IntrusiveForwardList() = default;

  // Iterators.
  iterator before_begin() { return iterator(&first_); }
  const_iterator before_begin() const { return const_iterator(&first_); }
  iterator begin() { return iterator(first_.next_hook); }
  const_iterator begin() const { return const_iterator(first_.next_hook); }
  iterator end() { return iterator(nullptr); }
  const_iterator end() const { return const_iterator(nullptr); }
  const_iterator cbefore_begin() const { return const_iterator(&first_); }
  const_iterator cbegin() const { return const_iterator(first_.next_hook); }
  const_iterator cend() const { return const_iterator(nullptr); }

  // Capacity.
  bool empty() const { return begin() == end(); }
  size_t max_size() { return static_cast<size_t>(-1); }

  // Element access.
  reference front() { return *begin(); }
  const_reference front() const { return *begin(); }

  // Modifiers.
  template <typename InputIterator>
  void assign(InputIterator first, InputIterator last) {
    IntrusiveForwardList tmp(first, last);
    tmp.swap(*this);
  }
  void push_front(value_type& value) {
    insert_after(before_begin(), value);
  }
  void pop_front() {
    DCHECK(!empty());
    erase_after(before_begin());
  }
  iterator insert_after(const_iterator position, value_type& value) {
    const IntrusiveForwardListHook* new_hook = hook_traits::GetHook(&value);
    new_hook->next_hook = position.hook_->next_hook;
    position.hook_->next_hook = new_hook;
    return iterator(new_hook);
  }
  template <typename InputIterator>
  iterator insert_after(const_iterator position, InputIterator first, InputIterator last) {
    while (first != last) {
      position = insert_after(position, *first++);
    }
    return iterator(position.hook_);
  }
  iterator erase_after(const_iterator position) {
    const_iterator last = position;
    std::advance(last, 2);
    return erase_after(position, last);
  }
  iterator erase_after(const_iterator position, const_iterator last) {
    DCHECK(position != last);
    position.hook_->next_hook = last.hook_;
    return iterator(last.hook_);
  }
  void swap(IntrusiveForwardList& other) {
    std::swap(first_.next_hook, other.first_.next_hook);
  }
  void clear() {
    first_.next_hook = nullptr;
  }

  // Operations.
  void splice_after(const_iterator position, IntrusiveForwardList& src) {
    DCHECK(position != end());
    splice_after(position, src, src.before_begin(), src.end());
  }
  void splice_after(const_iterator position, IntrusiveForwardList&& src) {
    splice_after(position, src);  // Use l-value overload.
  }
  // Splice the element after `i`.
  void splice_after(const_iterator position, IntrusiveForwardList& src, const_iterator i) {
    // The standard specifies that this version does nothing if `position == i`
    // or `position == ++i`. We must handle the latter here because the overload
    // `splice_after(position, src, first, last)` does not allow `position` inside
    // the range `(first, last)`.
    if (++const_iterator(i) == position) {
      return;
    }
    const_iterator last = i;
    std::advance(last, 2);
    splice_after(position, src, i, last);
  }
  // Splice the element after `i`.
  void splice_after(const_iterator position, IntrusiveForwardList&& src, const_iterator i) {
    splice_after(position, src, i);  // Use l-value overload.
  }
  // Splice elements between `first` and `last`, i.e. open range `(first, last)`.
  void splice_after(const_iterator position,
                    IntrusiveForwardList& src,
                    const_iterator first,
                    const_iterator last) {
    DCHECK(position != end());
    DCHECK(first != last);
    if (++const_iterator(first) == last) {
      // Nothing to do.
      return;
    }
    // If position is just before end() and last is src.end(), we can finish this quickly.
    if (++const_iterator(position) == end() && last == src.end()) {
      position.hook_->next_hook = first.hook_->next_hook;
      first.hook_->next_hook = nullptr;
      return;
    }
    // Otherwise we need to find the position before last to fix up the hook.
    const_iterator before_last = first;
    while (++const_iterator(before_last) != last) {
      ++before_last;
    }
    // Detach (first, last).
    const IntrusiveForwardListHook* first_taken = first.hook_->next_hook;
    first.hook_->next_hook = last.hook_;
    // Attach the sequence to the new position.
    before_last.hook_->next_hook = position.hook_->next_hook;
    position.hook_->next_hook = first_taken;
  }
  // Splice elements between `first` and `last`, i.e. open range `(first, last)`.
  void splice_after(const_iterator position,
                    IntrusiveForwardList&& src,
                    const_iterator first,
                    const_iterator last) {
    splice_after(position, src, first, last);  // Use l-value overload.
  }
  void remove(const value_type& value) {
    remove_if([value](const value_type& v) { return value == v; });
  }
  template <typename Predicate>
  void remove_if(Predicate pred) {
    iterator prev = before_begin();
    for (iterator current = begin(); current != end(); ++current) {
      if (pred(*current)) {
        erase_after(prev);
        current = prev;
      } else {
        prev = current;
      }
    }
  }
  void unique() {
    unique(std::equal_to<value_type>());
  }
  template <typename BinaryPredicate>
  void unique(BinaryPredicate pred) {
    if (!empty()) {
      iterator prev = begin();
      iterator current = prev;
      ++current;
      for (; current != end(); ++current) {
        if (pred(*prev, *current)) {
          erase_after(prev);
          current = prev;
        } else {
          prev = current;
        }
      }
    }
  }
  void merge(IntrusiveForwardList& other) {
    merge(other, std::less<value_type>());
  }
  void merge(IntrusiveForwardList&& other) {
    merge(other);  // Use l-value overload.
  }
  template <typename Compare>
  void merge(IntrusiveForwardList& other, Compare cmp) {
    iterator prev = before_begin();
    iterator current = begin();
    iterator other_prev = other.before_begin();
    iterator other_current = other.begin();
    while (current != end() && other_current != other.end()) {
      if (cmp(*other_current, *current)) {
        ++other_current;
        splice_after(prev, other, other_prev);
        ++prev;
      } else {
        prev = current;
        ++current;
      }
      DCHECK(++const_iterator(prev) == current);
      DCHECK(++const_iterator(other_prev) == other_current);
    }
    splice_after(prev, other);
  }
  template <typename Compare>
  void merge(IntrusiveForwardList&& other, Compare cmp) {
    merge(other, cmp);  // Use l-value overload.
  }
  void sort() {
    sort(std::less<value_type>());
  }
  template <typename Compare>
  void sort(Compare cmp) {
    size_t n = std::distance(begin(), end());
    if (n >= 2u) {
      const_iterator middle = before_begin();
      std::advance(middle, n / 2u);
      IntrusiveForwardList second_half;
      second_half.splice_after(second_half.before_begin(), *this, middle, end());
      sort(cmp);
      second_half.sort(cmp);
      merge(second_half, cmp);
    }
  }
  void reverse() {
    IntrusiveForwardList reversed;
    while (!empty()) {
      value_type& value = front();
      erase_after(before_begin());
      reversed.insert_after(reversed.before_begin(), value);
    }
    reversed.swap(*this);
  }

  // Extensions.
  bool HasExactlyOneElement() const {
    return !empty() && ++begin() == end();
  }
  size_t SizeSlow() const {
    return std::distance(begin(), end());
  }
  bool ContainsNode(const_reference node) const {
    for (auto&& n : *this) {
      if (std::addressof(n) == std::addressof(node)) {
        return true;
      }
    }
    return false;
  }

 private:
  static IntrusiveForwardListHook* ModifiableHook(const IntrusiveForwardListHook* hook) {
    return const_cast<IntrusiveForwardListHook*>(hook);
  }

  IntrusiveForwardListHook first_;
};

template <typename T, typename HookTraits>
void swap(IntrusiveForwardList<T, HookTraits>& lhs, IntrusiveForwardList<T, HookTraits>& rhs) {
  lhs.swap(rhs);
}

template <typename T, typename HookTraits>
bool operator==(const IntrusiveForwardList<T, HookTraits>& lhs,
                const IntrusiveForwardList<T, HookTraits>& rhs) {
  auto lit = lhs.begin();
  auto rit = rhs.begin();
  for (; lit != lhs.end() && rit != rhs.end(); ++lit, ++rit) {
    if (*lit != *rit) {
      return false;
    }
  }
  return lit == lhs.end() && rit == rhs.end();
}

template <typename T, typename HookTraits>
bool operator!=(const IntrusiveForwardList<T, HookTraits>& lhs,
                const IntrusiveForwardList<T, HookTraits>& rhs) {
  return !(lhs == rhs);
}

template <typename T, typename HookTraits>
bool operator<(const IntrusiveForwardList<T, HookTraits>& lhs,
               const IntrusiveForwardList<T, HookTraits>& rhs) {
  return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T, typename HookTraits>
bool operator>(const IntrusiveForwardList<T, HookTraits>& lhs,
               const IntrusiveForwardList<T, HookTraits>& rhs) {
  return rhs < lhs;
}

template <typename T, typename HookTraits>
bool operator<=(const IntrusiveForwardList<T, HookTraits>& lhs,
                const IntrusiveForwardList<T, HookTraits>& rhs) {
  return !(rhs < lhs);
}

template <typename T, typename HookTraits>
bool operator>=(const IntrusiveForwardList<T, HookTraits>& lhs,
                const IntrusiveForwardList<T, HookTraits>& rhs) {
  return !(lhs < rhs);
}

template <typename T, IntrusiveForwardListHook T::* NextPtr>
class IntrusiveForwardListMemberHookTraits {
 public:
  static const IntrusiveForwardListHook* GetHook(const T* value) {
    return &(value->*NextPtr);
  }

  static T* GetValue(const IntrusiveForwardListHook* hook) {
    return reinterpret_cast<T*>(
        reinterpret_cast<uintptr_t>(hook) - OFFSETOF_MEMBERPTR(T, NextPtr));
  }
};

template <typename T, typename Tag>
class IntrusiveForwardListBaseHookTraits {
 public:
  static const IntrusiveForwardListHook* GetHook(const T* value) {
    // Explicit conversion to the "node" followed by implicit conversion to the "hook".
    return static_cast<const IntrusiveForwardListNode<T, Tag>*>(value);
  }

  static T* GetValue(const IntrusiveForwardListHook* hook) {
    return down_cast<T*>(down_cast<IntrusiveForwardListNode<T, Tag>*>(
        const_cast<IntrusiveForwardListHook*>(hook)));
  }
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_INTRUSIVE_FORWARD_LIST_H_
