/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_ATOMIC_H_
#define ART_LIBARTBASE_BASE_ATOMIC_H_

#include <stdint.h>
#include <atomic>
#include <limits>
#include <vector>

#include <android-base/logging.h>

#include "base/macros.h"

namespace art {

template<typename T>
class PACKED(sizeof(T)) Atomic : public std::atomic<T> {
 public:
  Atomic<T>() : std::atomic<T>(T()) { }

  explicit Atomic<T>(T value) : std::atomic<T>(value) { }

  // Load from memory without ordering or synchronization constraints.
  T LoadRelaxed() const {
    return this->load(std::memory_order_relaxed);
  }

  // Load from memory with acquire ordering.
  T LoadAcquire() const {
    return this->load(std::memory_order_acquire);
  }

  // Word tearing allowed, but may race.
  // TODO: Optimize?
  // There has been some discussion of eventually disallowing word
  // tearing for Java data loads.
  T LoadJavaData() const {
    return this->load(std::memory_order_relaxed);
  }

  // Load from memory with a total ordering.
  // Corresponds exactly to a Java volatile load.
  T LoadSequentiallyConsistent() const {
    return this->load(std::memory_order_seq_cst);
  }

  // Store to memory without ordering or synchronization constraints.
  void StoreRelaxed(T desired_value) {
    this->store(desired_value, std::memory_order_relaxed);
  }

  // Word tearing allowed, but may race.
  void StoreJavaData(T desired_value) {
    this->store(desired_value, std::memory_order_relaxed);
  }

  // Store to memory with release ordering.
  void StoreRelease(T desired_value) {
    this->store(desired_value, std::memory_order_release);
  }

  // Store to memory with a total ordering.
  void StoreSequentiallyConsistent(T desired_value) {
    this->store(desired_value, std::memory_order_seq_cst);
  }

  // Atomically replace the value with desired_value.
  T ExchangeRelaxed(T desired_value) {
    return this->exchange(desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value.
  T ExchangeSequentiallyConsistent(T desired_value) {
    return this->exchange(desired_value, std::memory_order_seq_cst);
  }

  // Atomically replace the value with desired_value.
  T ExchangeAcquire(T desired_value) {
    return this->exchange(desired_value, std::memory_order_acquire);
  }

  // Atomically replace the value with desired_value.
  T ExchangeRelease(T desired_value) {
    return this->exchange(desired_value, std::memory_order_release);
  }

  // Atomically replace the value with desired_value if it matches the expected_value.
  // Participates in total ordering of atomic operations. Returns true on success, false otherwise.
  // If the value does not match, updates the expected_value argument with the value that was
  // atomically read for the failed comparison.
  bool CompareAndExchangeStrongSequentiallyConsistent(T* expected_value, T desired_value) {
    return this->compare_exchange_strong(*expected_value, desired_value, std::memory_order_seq_cst);
  }

  // Atomically replace the value with desired_value if it matches the expected_value.
  // Participates in total ordering of atomic operations. Returns true on success, false otherwise.
  // If the value does not match, updates the expected_value argument with the value that was
  // atomically read for the failed comparison.
  bool CompareAndExchangeStrongAcquire(T* expected_value, T desired_value) {
    return this->compare_exchange_strong(*expected_value, desired_value, std::memory_order_acquire);
  }

  // Atomically replace the value with desired_value if it matches the expected_value.
  // Participates in total ordering of atomic operations. Returns true on success, false otherwise.
  // If the value does not match, updates the expected_value argument with the value that was
  // atomically read for the failed comparison.
  bool CompareAndExchangeStrongRelease(T* expected_value, T desired_value) {
    return this->compare_exchange_strong(*expected_value, desired_value, std::memory_order_release);
  }

  // Atomically replace the value with desired_value if it matches the expected_value.
  // Participates in total ordering of atomic operations.
  bool CompareAndSetStrongSequentiallyConsistent(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_seq_cst);
  }

  // The same, except it may fail spuriously.
  bool CompareAndSetWeakSequentiallyConsistent(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_seq_cst);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Doesn't
  // imply ordering or synchronization constraints.
  bool CompareAndSetStrongRelaxed(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // to other memory locations become visible to the threads that do a consume or an acquire on the
  // same location.
  bool CompareAndSetStrongRelease(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_release);
  }

  // The same, except it may fail spuriously.
  bool CompareAndSetWeakRelaxed(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // made to other memory locations by the thread that did the release become visible in this
  // thread.
  bool CompareAndSetWeakAcquire(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_acquire);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // to other memory locations become visible to the threads that do a consume or an acquire on the
  // same location.
  bool CompareAndSetWeakRelease(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_release);
  }

  T FetchAndAddSequentiallyConsistent(const T value) {
    return this->fetch_add(value, std::memory_order_seq_cst);  // Return old_value.
  }

  T FetchAndAddRelaxed(const T value) {
    return this->fetch_add(value, std::memory_order_relaxed);  // Return old_value.
  }

  T FetchAndAddAcquire(const T value) {
    return this->fetch_add(value, std::memory_order_acquire);  // Return old_value.
  }

  T FetchAndAddRelease(const T value) {
    return this->fetch_add(value, std::memory_order_acquire);  // Return old_value.
  }

  T FetchAndSubSequentiallyConsistent(const T value) {
    return this->fetch_sub(value, std::memory_order_seq_cst);  // Return old value.
  }

  T FetchAndSubRelaxed(const T value) {
    return this->fetch_sub(value, std::memory_order_relaxed);  // Return old value.
  }

  T FetchAndBitwiseAndSequentiallyConsistent(const T value) {
    return this->fetch_and(value, std::memory_order_seq_cst);  // Return old_value.
  }

  T FetchAndBitwiseAndAcquire(const T value) {
    return this->fetch_and(value, std::memory_order_acquire);  // Return old_value.
  }

  T FetchAndBitwiseAndRelease(const T value) {
    return this->fetch_and(value, std::memory_order_release);  // Return old_value.
  }

  T FetchAndBitwiseOrSequentiallyConsistent(const T value) {
    return this->fetch_or(value, std::memory_order_seq_cst);  // Return old_value.
  }

  T FetchAndBitwiseOrAcquire(const T value) {
    return this->fetch_or(value, std::memory_order_acquire);  // Return old_value.
  }

  T FetchAndBitwiseOrRelease(const T value) {
    return this->fetch_or(value, std::memory_order_release);  // Return old_value.
  }

  T FetchAndBitwiseXorSequentiallyConsistent(const T value) {
    return this->fetch_xor(value, std::memory_order_seq_cst);  // Return old_value.
  }

  T FetchAndBitwiseXorAcquire(const T value) {
    return this->fetch_xor(value, std::memory_order_acquire);  // Return old_value.
  }

  T FetchAndBitwiseXorRelease(const T value) {
    return this->fetch_xor(value, std::memory_order_release);  // Return old_value.
  }

  volatile T* Address() {
    return reinterpret_cast<T*>(this);
  }

  static T MaxValue() {
    return std::numeric_limits<T>::max();
  }
};

typedef Atomic<int32_t> AtomicInteger;

static_assert(sizeof(AtomicInteger) == sizeof(int32_t), "Weird AtomicInteger size");
static_assert(alignof(AtomicInteger) == alignof(int32_t),
              "AtomicInteger alignment differs from that of underlyingtype");
static_assert(sizeof(Atomic<int64_t>) == sizeof(int64_t), "Weird Atomic<int64> size");

// Assert the alignment of 64-bit integers is 64-bit. This isn't true on certain 32-bit
// architectures (e.g. x86-32) but we know that 64-bit integers here are arranged to be 8-byte
// aligned.
#if defined(__LP64__)
  static_assert(alignof(Atomic<int64_t>) == alignof(int64_t),
                "Atomic<int64> alignment differs from that of underlying type");
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_ATOMIC_H_
