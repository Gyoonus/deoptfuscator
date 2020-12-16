/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_UTILS_H_
#define ART_LIBARTBASE_BASE_UTILS_H_

#include <pthread.h>
#include <stdlib.h>

#include <random>
#include <string>

#include <android-base/logging.h>

#include "base/casts.h"
#include "base/enums.h"
#include "base/globals.h"
#include "base/macros.h"
#include "base/stringpiece.h"

namespace art {

template <typename T>
bool ParseUint(const char *in, T* out) {
  char* end;
  unsigned long long int result = strtoull(in, &end, 0);  // NOLINT(runtime/int)
  if (in == end || *end != '\0') {
    return false;
  }
  if (std::numeric_limits<T>::max() < result) {
    return false;
  }
  *out = static_cast<T>(result);
  return true;
}

template <typename T>
bool ParseInt(const char* in, T* out) {
  char* end;
  long long int result = strtoll(in, &end, 0);  // NOLINT(runtime/int)
  if (in == end || *end != '\0') {
    return false;
  }
  if (result < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < result) {
    return false;
  }
  *out = static_cast<T>(result);
  return true;
}

static inline uint32_t PointerToLowMemUInt32(const void* p) {
  uintptr_t intp = reinterpret_cast<uintptr_t>(p);
  DCHECK_LE(intp, 0xFFFFFFFFU);
  return intp & 0xFFFFFFFFU;
}

// Returns a human-readable size string such as "1MB".
std::string PrettySize(int64_t size_in_bytes);

// Splits a string using the given separator character into a vector of
// strings. Empty strings will be omitted.
void Split(const std::string& s, char separator, std::vector<std::string>* result);

// Returns the calling thread's tid. (The C libraries don't expose this.)
pid_t GetTid();

// Returns the given thread's name.
std::string GetThreadName(pid_t tid);

// Sets the name of the current thread. The name may be truncated to an
// implementation-defined limit.
void SetThreadName(const char* thread_name);

// Reads data from "/proc/self/task/${tid}/stat".
void GetTaskStats(pid_t tid, char* state, int* utime, int* stime, int* task_cpu);

class VoidFunctor {
 public:
  template <typename A>
  inline void operator() (A a ATTRIBUTE_UNUSED) const {
  }

  template <typename A, typename B>
  inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED) const {
  }

  template <typename A, typename B, typename C>
  inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED, C c ATTRIBUTE_UNUSED) const {
  }
};

inline bool TestBitmap(size_t idx, const uint8_t* bitmap) {
  return ((bitmap[idx / kBitsPerByte] >> (idx % kBitsPerByte)) & 0x01) != 0;
}

static inline constexpr bool ValidPointerSize(size_t pointer_size) {
  return pointer_size == 4 || pointer_size == 8;
}

static inline const void* EntryPointToCodePointer(const void* entry_point) {
  uintptr_t code = reinterpret_cast<uintptr_t>(entry_point);
  // TODO: Make this Thumb2 specific. It is benign on other architectures as code is always at
  //       least 2 byte aligned.
  code &= ~0x1;
  return reinterpret_cast<const void*>(code);
}

using UsageFn = void (*)(const char*, ...);

template <typename T>
static void ParseIntOption(const StringPiece& option,
                            const std::string& option_name,
                            T* out,
                            UsageFn usage,
                            bool is_long_option = true) {
  std::string option_prefix = option_name + (is_long_option ? "=" : "");
  DCHECK(option.starts_with(option_prefix)) << option << " " << option_prefix;
  const char* value_string = option.substr(option_prefix.size()).data();
  int64_t parsed_integer_value = 0;
  if (!ParseInt(value_string, &parsed_integer_value)) {
    usage("Failed to parse %s '%s' as an integer", option_name.c_str(), value_string);
  }
  *out = dchecked_integral_cast<T>(parsed_integer_value);
}

template <typename T>
static void ParseUintOption(const StringPiece& option,
                            const std::string& option_name,
                            T* out,
                            UsageFn usage,
                            bool is_long_option = true) {
  ParseIntOption(option, option_name, out, usage, is_long_option);
  if (*out < 0) {
    usage("%s passed a negative value %d", option_name.c_str(), *out);
    *out = 0;
  }
}

void ParseDouble(const std::string& option,
                 char after_char,
                 double min,
                 double max,
                 double* parsed_value,
                 UsageFn Usage);

#if defined(__BIONIC__)
struct Arc4RandomGenerator {
  typedef uint32_t result_type;
  static constexpr uint32_t min() { return std::numeric_limits<uint32_t>::min(); }
  static constexpr uint32_t max() { return std::numeric_limits<uint32_t>::max(); }
  uint32_t operator() () { return arc4random(); }
};
using RNG = Arc4RandomGenerator;
#else
using RNG = std::random_device;
#endif

template <typename T>
static T GetRandomNumber(T min, T max) {
  CHECK_LT(min, max);
  std::uniform_int_distribution<T> dist(min, max);
  RNG rng;
  return dist(rng);
}

// Sleep forever and never come back.
NO_RETURN void SleepForever();

inline void FlushInstructionCache(char* begin, char* end) {
  __builtin___clear_cache(begin, end);
}

inline void FlushDataCache(char* begin, char* end) {
  // Same as FlushInstructionCache for lack of other builtin. __builtin___clear_cache
  // flushes both caches.
  __builtin___clear_cache(begin, end);
}

template <typename T>
constexpr PointerSize ConvertToPointerSize(T any) {
  if (any == 4 || any == 8) {
    return static_cast<PointerSize>(any);
  } else {
    LOG(FATAL);
    UNREACHABLE();
  }
}

// Returns a type cast pointer if object pointed to is within the provided bounds.
// Otherwise returns nullptr.
template <typename T>
inline static T BoundsCheckedCast(const void* pointer,
                                  const void* lower,
                                  const void* upper) {
  const uint8_t* bound_begin = static_cast<const uint8_t*>(lower);
  const uint8_t* bound_end = static_cast<const uint8_t*>(upper);
  DCHECK(bound_begin <= bound_end);

  T result = reinterpret_cast<T>(pointer);
  const uint8_t* begin = static_cast<const uint8_t*>(pointer);
  const uint8_t* end = begin + sizeof(*result);
  if (begin < bound_begin || end > bound_end || begin > end) {
    return nullptr;
  }
  return result;
}

template <typename T, size_t size>
constexpr size_t ArrayCount(const T (&)[size]) {
  return size;
}

// Return -1 if <, 0 if ==, 1 if >.
template <typename T>
inline static int32_t Compare(T lhs, T rhs) {
  return (lhs < rhs) ? -1 : ((lhs == rhs) ? 0 : 1);
}

// Return -1 if < 0, 0 if == 0, 1 if > 0.
template <typename T>
inline static int32_t Signum(T opnd) {
  return (opnd < 0) ? -1 : ((opnd == 0) ? 0 : 1);
}

template <typename Func, typename... Args>
static inline void CheckedCall(const Func& function, const char* what, Args... args) {
  int rc = function(args...);
  if (UNLIKELY(rc != 0)) {
    errno = rc;
    PLOG(FATAL) << "Checked call failed for " << what;
  }
}

// Hash bytes using a relatively fast hash.
static inline size_t HashBytes(const uint8_t* data, size_t len) {
  size_t hash = 0x811c9dc5;
  for (uint32_t i = 0; i < len; ++i) {
    hash = (hash * 16777619) ^ data[i];
  }
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
  return hash;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_UTILS_H_
