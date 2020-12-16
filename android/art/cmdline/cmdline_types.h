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
#ifndef ART_CMDLINE_CMDLINE_TYPES_H_
#define ART_CMDLINE_CMDLINE_TYPES_H_

#define CMDLINE_NDEBUG 1  // Do not output any debugging information for parsing.

#include <list>

#include "cmdline_type_parser.h"
#include "detail/cmdline_debug_detail.h"
#include "memory_representation.h"

#include "android-base/strings.h"

// Includes for the types that are being specialized
#include <string>
#include "base/logging.h"
#include "base/time_utils.h"
#include "experimental_flags.h"
#include "gc/collector_type.h"
#include "gc/space/large_object_space.h"
#include "jdwp/jdwp.h"
#include "jdwp_provider.h"
#include "jit/profile_saver_options.h"
#include "plugin.h"
#include "read_barrier_config.h"
#include "ti/agent.h"
#include "unit.h"

namespace art {

// The default specialization will always fail parsing the type from a string.
// Provide your own specialization that inherits from CmdlineTypeParser<T>
// and implements either Parse or ParseAndAppend
// (only if the argument was defined with ::AppendValues()) but not both.
template <typename T>
struct CmdlineType : CmdlineTypeParser<T> {
};

// Specializations for CmdlineType<T> follow:

// Parse argument definitions for Unit-typed arguments.
template <>
struct CmdlineType<Unit> : CmdlineTypeParser<Unit> {
  Result Parse(const std::string& args) {
    if (args == "") {
      return Result::Success(Unit{});
    }
    return Result::Failure("Unexpected extra characters " + args);
  }
};

template <>
struct CmdlineType<JdwpProvider> : CmdlineTypeParser<JdwpProvider> {
  /*
   * Handle a single JDWP provider name. Must be either 'internal', 'default', or the file name of
   * an agent. A plugin will make use of this and the jdwpOptions to set up jdwp when appropriate.
   */
  Result Parse(const std::string& option) {
    if (option == "help") {
      return Result::Usage(
          "Example: -XjdwpProvider:none to disable JDWP\n"
          "Example: -XjdwpProvider:internal for internal jdwp implementation\n"
          "Example: -XjdwpProvider:adbconnection for adb connection mediated jdwp implementation\n"
          "Example: -XjdwpProvider:default for the default jdwp implementation\n");
    } else if (option == "default") {
      return Result::Success(JdwpProvider::kDefaultJdwpProvider);
    } else if (option == "internal") {
      return Result::Success(JdwpProvider::kInternal);
    } else if (option == "adbconnection") {
      return Result::Success(JdwpProvider::kAdbConnection);
    } else if (option == "none") {
      return Result::Success(JdwpProvider::kNone);
    } else {
      return Result::Failure(std::string("not a valid jdwp provider: ") + option);
    }
  }
  static const char* Name() { return "JdwpProvider"; }
};

template <size_t Divisor>
struct CmdlineType<Memory<Divisor>> : CmdlineTypeParser<Memory<Divisor>> {
  using typename CmdlineTypeParser<Memory<Divisor>>::Result;

  Result Parse(const std::string& arg) {
    CMDLINE_DEBUG_LOG << "Parsing memory: " << arg << std::endl;
    size_t val = ParseMemoryOption(arg.c_str(), Divisor);
    CMDLINE_DEBUG_LOG << "Memory parsed to size_t value: " << val << std::endl;

    if (val == 0) {
      return Result::Failure(std::string("not a valid memory value, or not divisible by ")
                             + std::to_string(Divisor));
    }

    return Result::Success(Memory<Divisor>(val));
  }

  // Parse a string of the form /[0-9]+[kKmMgG]?/, which is used to specify
  // memory sizes.  [kK] indicates kilobytes, [mM] megabytes, and
  // [gG] gigabytes.
  //
  // "s" should point just past the "-Xm?" part of the string.
  // "div" specifies a divisor, e.g. 1024 if the value must be a multiple
  // of 1024.
  //
  // The spec says the -Xmx and -Xms options must be multiples of 1024.  It
  // doesn't say anything about -Xss.
  //
  // Returns 0 (a useless size) if "s" is malformed or specifies a low or
  // non-evenly-divisible value.
  //
  static size_t ParseMemoryOption(const char* s, size_t div) {
    // strtoul accepts a leading [+-], which we don't want,
    // so make sure our string starts with a decimal digit.
    if (isdigit(*s)) {
      char* s2;
      size_t val = strtoul(s, &s2, 10);
      if (s2 != s) {
        // s2 should be pointing just after the number.
        // If this is the end of the string, the user
        // has specified a number of bytes.  Otherwise,
        // there should be exactly one more character
        // that specifies a multiplier.
        if (*s2 != '\0') {
          // The remainder of the string is either a single multiplier
          // character, or nothing to indicate that the value is in
          // bytes.
          char c = *s2++;
          if (*s2 == '\0') {
            size_t mul;
            if (c == '\0') {
              mul = 1;
            } else if (c == 'k' || c == 'K') {
              mul = KB;
            } else if (c == 'm' || c == 'M') {
              mul = MB;
            } else if (c == 'g' || c == 'G') {
              mul = GB;
            } else {
              // Unknown multiplier character.
              return 0;
            }

            if (val <= std::numeric_limits<size_t>::max() / mul) {
              val *= mul;
            } else {
              // Clamp to a multiple of 1024.
              val = std::numeric_limits<size_t>::max() & ~(1024-1);
            }
          } else {
            // There's more than one character after the numeric part.
            return 0;
          }
        }
        // The man page says that a -Xm value must be a multiple of 1024.
        if (val % div == 0) {
          return val;
        }
      }
    }
    return 0;
  }

  static const char* Name() { return Memory<Divisor>::Name(); }
};

template <>
struct CmdlineType<double> : CmdlineTypeParser<double> {
  Result Parse(const std::string& str) {
    char* end = nullptr;
    errno = 0;
    double value = strtod(str.c_str(), &end);

    if (*end != '\0') {
      return Result::Failure("Failed to parse double from " + str);
    }
    if (errno == ERANGE) {
      return Result::OutOfRange(
          "Failed to parse double from " + str + "; overflow/underflow occurred");
    }

    return Result::Success(value);
  }

  static const char* Name() { return "double"; }
};

template <typename T>
static inline CmdlineParseResult<T> ParseNumeric(const std::string& str) {
  static_assert(sizeof(T) < sizeof(long long int),  // NOLINT [runtime/int] [4]
                "Current support is restricted.");

  const char* begin = str.c_str();
  char* end;

  // Parse into a larger type (long long) because we can't use strtoul
  // since it silently converts negative values into unsigned long and doesn't set errno.
  errno = 0;
  long long int result = strtoll(begin, &end, 10);  // NOLINT [runtime/int] [4]
  if (begin == end || *end != '\0' || errno == EINVAL) {
    return CmdlineParseResult<T>::Failure("Failed to parse integer from " + str);
  } else if ((errno == ERANGE) ||  // NOLINT [runtime/int] [4]
      result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max()) {
    return CmdlineParseResult<T>::OutOfRange(
        "Failed to parse integer from " + str + "; out of range");
  }

  return CmdlineParseResult<T>::Success(static_cast<T>(result));
}

template <>
struct CmdlineType<unsigned int> : CmdlineTypeParser<unsigned int> {
  Result Parse(const std::string& str) {
    return ParseNumeric<unsigned int>(str);
  }

  static const char* Name() { return "unsigned integer"; }
};

template <>
struct CmdlineType<int> : CmdlineTypeParser<int> {
  Result Parse(const std::string& str) {
    return ParseNumeric<int>(str);
  }

  static const char* Name() { return "unsigned integer"; }
};

// Lightweight nanosecond value type. Allows parser to convert user-input from milliseconds
// to nanoseconds automatically after parsing.
//
// All implicit conversion from uint64_t uses nanoseconds.
struct MillisecondsToNanoseconds {
  // Create from nanoseconds.
  MillisecondsToNanoseconds(uint64_t nanoseconds) : nanoseconds_(nanoseconds) {  // NOLINT [runtime/explicit] [5]
  }

  // Create from milliseconds.
  static MillisecondsToNanoseconds FromMilliseconds(unsigned int milliseconds) {
    return MillisecondsToNanoseconds(MsToNs(milliseconds));
  }

  // Get the underlying nanoseconds value.
  uint64_t GetNanoseconds() const {
    return nanoseconds_;
  }

  // Get the milliseconds value [via a conversion]. Loss of precision will occur.
  uint64_t GetMilliseconds() const {
    return NsToMs(nanoseconds_);
  }

  // Get the underlying nanoseconds value.
  operator uint64_t() const {
    return GetNanoseconds();
  }

  // Default constructors/copy-constructors.
  MillisecondsToNanoseconds() : nanoseconds_(0ul) {}
  MillisecondsToNanoseconds(const MillisecondsToNanoseconds&) = default;
  MillisecondsToNanoseconds(MillisecondsToNanoseconds&&) = default;

 private:
  uint64_t nanoseconds_;
};

template <>
struct CmdlineType<MillisecondsToNanoseconds> : CmdlineTypeParser<MillisecondsToNanoseconds> {
  Result Parse(const std::string& str) {
    CmdlineType<unsigned int> uint_parser;
    CmdlineParseResult<unsigned int> res = uint_parser.Parse(str);

    if (res.IsSuccess()) {
      return Result::Success(MillisecondsToNanoseconds::FromMilliseconds(res.GetValue()));
    } else {
      return Result::CastError(res);
    }
  }

  static const char* Name() { return "MillisecondsToNanoseconds"; }
};

template <>
struct CmdlineType<std::string> : CmdlineTypeParser<std::string> {
  Result Parse(const std::string& args) {
    return Result::Success(args);
  }

  Result ParseAndAppend(const std::string& args,
                        std::string& existing_value) {
    if (existing_value.empty()) {
      existing_value = args;
    } else {
      existing_value += ' ';
      existing_value += args;
    }
    return Result::SuccessNoValue();
  }
};

template <>
struct CmdlineType<std::vector<Plugin>> : CmdlineTypeParser<std::vector<Plugin>> {
  Result Parse(const std::string& args) {
    assert(false && "Use AppendValues() for a Plugin vector type");
    return Result::Failure("Unconditional failure: Plugin vector must be appended: " + args);
  }

  Result ParseAndAppend(const std::string& args,
                        std::vector<Plugin>& existing_value) {
    existing_value.push_back(Plugin::Create(args));
    return Result::SuccessNoValue();
  }

  static const char* Name() { return "std::vector<Plugin>"; }
};

template <>
struct CmdlineType<std::list<ti::AgentSpec>> : CmdlineTypeParser<std::list<ti::AgentSpec>> {
  Result Parse(const std::string& args) {
    assert(false && "Use AppendValues() for an Agent list type");
    return Result::Failure("Unconditional failure: Agent list must be appended: " + args);
  }

  Result ParseAndAppend(const std::string& args,
                        std::list<ti::AgentSpec>& existing_value) {
    existing_value.emplace_back(args);
    return Result::SuccessNoValue();
  }

  static const char* Name() { return "std::list<ti::AgentSpec>"; }
};

template <>
struct CmdlineType<std::vector<std::string>> : CmdlineTypeParser<std::vector<std::string>> {
  Result Parse(const std::string& args) {
    assert(false && "Use AppendValues() for a string vector type");
    return Result::Failure("Unconditional failure: string vector must be appended: " + args);
  }

  Result ParseAndAppend(const std::string& args,
                        std::vector<std::string>& existing_value) {
    existing_value.push_back(args);
    return Result::SuccessNoValue();
  }

  static const char* Name() { return "std::vector<std::string>"; }
};

template <char Separator>
struct ParseStringList {
  explicit ParseStringList(std::vector<std::string>&& list) : list_(list) {}

  operator std::vector<std::string>() const {
    return list_;
  }

  operator std::vector<std::string>&&() && {
    return std::move(list_);
  }

  size_t Size() const {
    return list_.size();
  }

  std::string Join() const {
    return android::base::Join(list_, Separator);
  }

  static ParseStringList<Separator> Split(const std::string& str) {
    std::vector<std::string> list;
    art::Split(str, Separator, &list);
    return ParseStringList<Separator>(std::move(list));
  }

  ParseStringList() = default;
  ParseStringList(const ParseStringList&) = default;
  ParseStringList(ParseStringList&&) = default;

 private:
  std::vector<std::string> list_;
};

template <char Separator>
struct CmdlineType<ParseStringList<Separator>> : CmdlineTypeParser<ParseStringList<Separator>> {
  using Result = CmdlineParseResult<ParseStringList<Separator>>;

  Result Parse(const std::string& args) {
    return Result::Success(ParseStringList<Separator>::Split(args));
  }

  static const char* Name() { return "ParseStringList<Separator>"; }
};

static gc::CollectorType ParseCollectorType(const std::string& option) {
  if (option == "MS" || option == "nonconcurrent") {
    return gc::kCollectorTypeMS;
  } else if (option == "CMS" || option == "concurrent") {
    return gc::kCollectorTypeCMS;
  } else if (option == "SS") {
    return gc::kCollectorTypeSS;
  } else if (option == "GSS") {
    return gc::kCollectorTypeGSS;
  } else if (option == "CC") {
    return gc::kCollectorTypeCC;
  } else if (option == "MC") {
    return gc::kCollectorTypeMC;
  } else {
    return gc::kCollectorTypeNone;
  }
}

struct XGcOption {
  // These defaults are used when the command line arguments for -Xgc:
  // are either omitted completely or partially.
  gc::CollectorType collector_type_ = gc::kCollectorTypeDefault;
  bool verify_pre_gc_heap_ = false;
  bool verify_pre_sweeping_heap_ = kIsDebugBuild;
  bool verify_post_gc_heap_ = false;
  bool verify_pre_gc_rosalloc_ = kIsDebugBuild;
  bool verify_pre_sweeping_rosalloc_ = false;
  bool verify_post_gc_rosalloc_ = false;
  // Do no measurements for kUseTableLookupReadBarrier to avoid test timeouts. b/31679493
  bool measure_ = kIsDebugBuild && !kUseTableLookupReadBarrier;
  bool gcstress_ = false;
};

template <>
struct CmdlineType<XGcOption> : CmdlineTypeParser<XGcOption> {
  Result Parse(const std::string& option) {  // -Xgc: already stripped
    XGcOption xgc{};

    std::vector<std::string> gc_options;
    Split(option, ',', &gc_options);
    for (const std::string& gc_option : gc_options) {
      gc::CollectorType collector_type = ParseCollectorType(gc_option);
      if (collector_type != gc::kCollectorTypeNone) {
        xgc.collector_type_ = collector_type;
      } else if (gc_option == "preverify") {
        xgc.verify_pre_gc_heap_ = true;
      } else if (gc_option == "nopreverify") {
        xgc.verify_pre_gc_heap_ = false;
      }  else if (gc_option == "presweepingverify") {
        xgc.verify_pre_sweeping_heap_ = true;
      } else if (gc_option == "nopresweepingverify") {
        xgc.verify_pre_sweeping_heap_ = false;
      } else if (gc_option == "postverify") {
        xgc.verify_post_gc_heap_ = true;
      } else if (gc_option == "nopostverify") {
        xgc.verify_post_gc_heap_ = false;
      } else if (gc_option == "preverify_rosalloc") {
        xgc.verify_pre_gc_rosalloc_ = true;
      } else if (gc_option == "nopreverify_rosalloc") {
        xgc.verify_pre_gc_rosalloc_ = false;
      } else if (gc_option == "presweepingverify_rosalloc") {
        xgc.verify_pre_sweeping_rosalloc_ = true;
      } else if (gc_option == "nopresweepingverify_rosalloc") {
        xgc.verify_pre_sweeping_rosalloc_ = false;
      } else if (gc_option == "postverify_rosalloc") {
        xgc.verify_post_gc_rosalloc_ = true;
      } else if (gc_option == "nopostverify_rosalloc") {
        xgc.verify_post_gc_rosalloc_ = false;
      } else if (gc_option == "gcstress") {
        xgc.gcstress_ = true;
      } else if (gc_option == "nogcstress") {
        xgc.gcstress_ = false;
      } else if (gc_option == "measure") {
        xgc.measure_ = true;
      } else if ((gc_option == "precise") ||
                 (gc_option == "noprecise") ||
                 (gc_option == "verifycardtable") ||
                 (gc_option == "noverifycardtable")) {
        // Ignored for backwards compatibility.
      } else {
        return Result::Usage(std::string("Unknown -Xgc option ") + gc_option);
      }
    }

    return Result::Success(std::move(xgc));
  }

  static const char* Name() { return "XgcOption"; }
};

struct BackgroundGcOption {
  // If background_collector_type_ is kCollectorTypeNone, it defaults to the
  // XGcOption::collector_type_ after parsing options. If you set this to
  // kCollectorTypeHSpaceCompact then we will do an hspace compaction when
  // we transition to background instead of a normal collector transition.
  gc::CollectorType background_collector_type_;

  BackgroundGcOption(gc::CollectorType background_collector_type)  // NOLINT [runtime/explicit] [5]
    : background_collector_type_(background_collector_type) {}
  BackgroundGcOption()
    : background_collector_type_(gc::kCollectorTypeNone) {
  }

  operator gc::CollectorType() const { return background_collector_type_; }
};

template<>
struct CmdlineType<BackgroundGcOption>
  : CmdlineTypeParser<BackgroundGcOption>, private BackgroundGcOption {
  Result Parse(const std::string& substring) {
    // Special handling for HSpaceCompact since this is only valid as a background GC type.
    if (substring == "HSpaceCompact") {
      background_collector_type_ = gc::kCollectorTypeHomogeneousSpaceCompact;
    } else {
      gc::CollectorType collector_type = ParseCollectorType(substring);
      if (collector_type != gc::kCollectorTypeNone) {
        background_collector_type_ = collector_type;
      } else {
        return Result::Failure();
      }
    }

    BackgroundGcOption res = *this;
    return Result::Success(res);
  }

  static const char* Name() { return "BackgroundGcOption"; }
};

template <>
struct CmdlineType<LogVerbosity> : CmdlineTypeParser<LogVerbosity> {
  Result Parse(const std::string& options) {
    LogVerbosity log_verbosity = LogVerbosity();

    std::vector<std::string> verbose_options;
    Split(options, ',', &verbose_options);
    for (size_t j = 0; j < verbose_options.size(); ++j) {
      if (verbose_options[j] == "class") {
        log_verbosity.class_linker = true;
      } else if (verbose_options[j] == "collector") {
        log_verbosity.collector = true;
      } else if (verbose_options[j] == "compiler") {
        log_verbosity.compiler = true;
      } else if (verbose_options[j] == "deopt") {
        log_verbosity.deopt = true;
      } else if (verbose_options[j] == "gc") {
        log_verbosity.gc = true;
      } else if (verbose_options[j] == "heap") {
        log_verbosity.heap = true;
      } else if (verbose_options[j] == "jdwp") {
        log_verbosity.jdwp = true;
      } else if (verbose_options[j] == "jit") {
        log_verbosity.jit = true;
      } else if (verbose_options[j] == "jni") {
        log_verbosity.jni = true;
      } else if (verbose_options[j] == "monitor") {
        log_verbosity.monitor = true;
      } else if (verbose_options[j] == "oat") {
        log_verbosity.oat = true;
      } else if (verbose_options[j] == "profiler") {
        log_verbosity.profiler = true;
      } else if (verbose_options[j] == "signals") {
        log_verbosity.signals = true;
      } else if (verbose_options[j] == "simulator") {
        log_verbosity.simulator = true;
      } else if (verbose_options[j] == "startup") {
        log_verbosity.startup = true;
      } else if (verbose_options[j] == "third-party-jni") {
        log_verbosity.third_party_jni = true;
      } else if (verbose_options[j] == "threads") {
        log_verbosity.threads = true;
      } else if (verbose_options[j] == "verifier") {
        log_verbosity.verifier = true;
      } else if (verbose_options[j] == "verifier-debug") {
        log_verbosity.verifier_debug = true;
      } else if (verbose_options[j] == "image") {
        log_verbosity.image = true;
      } else if (verbose_options[j] == "systrace-locks") {
        log_verbosity.systrace_lock_logging = true;
      } else if (verbose_options[j] == "agents") {
        log_verbosity.agents = true;
      } else if (verbose_options[j] == "dex") {
        log_verbosity.dex = true;
      } else {
        return Result::Usage(std::string("Unknown -verbose option ") + verbose_options[j]);
      }
    }

    return Result::Success(log_verbosity);
  }

  static const char* Name() { return "LogVerbosity"; }
};

template <>
struct CmdlineType<ProfileSaverOptions> : CmdlineTypeParser<ProfileSaverOptions> {
  using Result = CmdlineParseResult<ProfileSaverOptions>;

 private:
  using StringResult = CmdlineParseResult<std::string>;
  using DoubleResult = CmdlineParseResult<double>;

  template <typename T>
  static Result ParseInto(ProfileSaverOptions& options,
                          T ProfileSaverOptions::*pField,
                          CmdlineParseResult<T>&& result) {
    assert(pField != nullptr);

    if (result.IsSuccess()) {
      options.*pField = result.ReleaseValue();
      return Result::SuccessNoValue();
    }

    return Result::CastError(result);
  }

  static std::string RemovePrefix(const std::string& source) {
    size_t prefix_idx = source.find(':');

    if (prefix_idx == std::string::npos) {
      return "";
    }

    return source.substr(prefix_idx + 1);
  }

 public:
  Result ParseAndAppend(const std::string& option, ProfileSaverOptions& existing) {
    // Special case which doesn't include a wildcard argument definition.
    // We pass-it through as-is.
    if (option == "-Xjitsaveprofilinginfo") {
      existing.enabled_ = true;
      return Result::SuccessNoValue();
    }

    if (option == "profile-boot-class-path") {
      existing.profile_boot_class_path_ = true;
      return Result::SuccessNoValue();
    }

    if (option == "profile-aot-code") {
      existing.profile_aot_code_ = true;
      return Result::SuccessNoValue();
    }

    if (option == "save-without-jit-notifications") {
      existing.wait_for_jit_notifications_to_save_ = false;
      return Result::SuccessNoValue();
    }

    // The rest of these options are always the wildcard from '-Xps-*'
    std::string suffix = RemovePrefix(option);

    if (android::base::StartsWith(option, "min-save-period-ms:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::min_save_period_ms_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "save-resolved-classes-delay-ms:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::save_resolved_classes_delay_ms_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "hot-startup-method-samples:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::hot_startup_method_samples_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "min-methods-to-save:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::min_methods_to_save_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "min-classes-to-save:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::min_classes_to_save_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "min-notification-before-wake:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::min_notification_before_wake_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "max-notification-before-wake:")) {
      CmdlineType<unsigned int> type_parser;
      return ParseInto(existing,
             &ProfileSaverOptions::max_notification_before_wake_,
             type_parser.Parse(suffix));
    }
    if (android::base::StartsWith(option, "profile-path:")) {
      existing.profile_path_ = suffix;
      return Result::SuccessNoValue();
    }

    return Result::Failure(std::string("Invalid suboption '") + option + "'");
  }

  static const char* Name() { return "ProfileSaverOptions"; }
  static constexpr bool kCanParseBlankless = true;
};

template<>
struct CmdlineType<ExperimentalFlags> : CmdlineTypeParser<ExperimentalFlags> {
  Result ParseAndAppend(const std::string& option, ExperimentalFlags& existing) {
    if (option == "none") {
      existing = ExperimentalFlags::kNone;
    } else {
      return Result::Failure(std::string("Unknown option '") + option + "'");
    }
    return Result::SuccessNoValue();
  }

  static const char* Name() { return "ExperimentalFlags"; }
};
}  // namespace art
#endif  // ART_CMDLINE_CMDLINE_TYPES_H_
