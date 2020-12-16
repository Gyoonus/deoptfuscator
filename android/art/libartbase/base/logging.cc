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

#include "logging.h"

#include <iostream>
#include <limits>
#include <sstream>

#include "aborting.h"

// Headers for LogMessage::LogLine.
#ifdef ART_TARGET_ANDROID
#include <log/log.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace art {

LogVerbosity gLogVerbosity;

std::atomic<unsigned int> gAborting(0);

static std::unique_ptr<std::string> gCmdLine;
static std::unique_ptr<std::string> gProgramInvocationName;
static std::unique_ptr<std::string> gProgramInvocationShortName;

const char* GetCmdLine() {
  return (gCmdLine.get() != nullptr) ? gCmdLine->c_str() : nullptr;
}

const char* ProgramInvocationName() {
  return (gProgramInvocationName.get() != nullptr) ? gProgramInvocationName->c_str() : "art";
}

const char* ProgramInvocationShortName() {
  return (gProgramInvocationShortName.get() != nullptr) ? gProgramInvocationShortName->c_str()
                                                        : "art";
}

void InitLogging(char* argv[], AbortFunction& abort_function) {
  if (gCmdLine.get() != nullptr) {
    return;
  }

  // Stash the command line for later use. We can use /proc/self/cmdline on Linux to recover this,
  // but we don't have that luxury on the Mac, and there are a couple of argv[0] variants that are
  // commonly used.
  if (argv != nullptr) {
    gCmdLine.reset(new std::string(argv[0]));
    for (size_t i = 1; argv[i] != nullptr; ++i) {
      gCmdLine->append(" ");
      gCmdLine->append(argv[i]);
    }
    gProgramInvocationName.reset(new std::string(argv[0]));
    const char* last_slash = strrchr(argv[0], '/');
    gProgramInvocationShortName.reset(new std::string((last_slash != nullptr) ? last_slash + 1
                                                                           : argv[0]));
  } else {
    // TODO: fall back to /proc/self/cmdline when argv is null on Linux.
    gCmdLine.reset(new std::string("<unset>"));
  }

#ifdef ART_TARGET_ANDROID
#define INIT_LOGGING_DEFAULT_LOGGER android::base::LogdLogger()
#else
#define INIT_LOGGING_DEFAULT_LOGGER android::base::StderrLogger
#endif
  android::base::InitLogging(argv, INIT_LOGGING_DEFAULT_LOGGER,
                             std::move<AbortFunction>(abort_function));
#undef INIT_LOGGING_DEFAULT_LOGGER
}

#ifdef ART_TARGET_ANDROID
static const android_LogPriority kLogSeverityToAndroidLogPriority[] = {
  ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_FATAL
};
static_assert(arraysize(kLogSeverityToAndroidLogPriority) == ::android::base::FATAL + 1,
              "Mismatch in size of kLogSeverityToAndroidLogPriority and values in LogSeverity");
#endif

void LogHelper::LogLineLowStack(const char* file,
                                unsigned int line,
                                LogSeverity log_severity,
                                const char* message) {
#ifdef ART_TARGET_ANDROID
  // Use android_writeLog() to avoid stack-based buffers used by android_printLog().
  const char* tag = ProgramInvocationShortName();
  int priority = kLogSeverityToAndroidLogPriority[static_cast<size_t>(log_severity)];
  char* buf = nullptr;
  size_t buf_size = 0u;
  if (priority == ANDROID_LOG_FATAL) {
    // Allocate buffer for snprintf(buf, buf_size, "%s:%u] %s", file, line, message) below.
    // If allocation fails, fall back to printing only the message.
    buf_size = strlen(file) + 1 /* ':' */ + std::numeric_limits<decltype(line)>::max_digits10 +
        2 /* "] " */ + strlen(message) + 1 /* terminating 0 */;
    buf = reinterpret_cast<char*>(malloc(buf_size));
  }
  if (buf != nullptr) {
    snprintf(buf, buf_size, "%s:%u] %s", file, line, message);
    android_writeLog(priority, tag, buf);
    free(buf);
  } else {
    android_writeLog(priority, tag, message);
  }
#else
  static constexpr char kLogCharacters[] = { 'V', 'D', 'I', 'W', 'E', 'F', 'F' };
  static_assert(
      arraysize(kLogCharacters) == static_cast<size_t>(::android::base::FATAL) + 1,
      "Wrong character array size");

  const char* program_name = ProgramInvocationShortName();
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, program_name, strlen(program_name)));
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, " ", 1));
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, &kLogCharacters[static_cast<size_t>(log_severity)], 1));
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, " ", 1));
  // TODO: pid and tid.
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, file, strlen(file)));
  // TODO: line.
  UNUSED(line);
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, "] ", 2));
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, message, strlen(message)));
  TEMP_FAILURE_RETRY(write(STDERR_FILENO, "\n", 1));
#endif  // ART_TARGET_ANDROID
}

}  // namespace art
