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

#include "monitor.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <log/log.h>
#include <log/log_event_list.h>

#include "art_method.h"
#include "thread.h"

#define EVENT_LOG_TAG_dvm_lock_sample 20003

namespace art {

void Monitor::LogContentionEvent(Thread* self,
                                 uint32_t wait_ms,
                                 uint32_t sample_percent,
                                 ArtMethod* owner_method,
                                 uint32_t owner_dex_pc) {
  android_log_event_list ctx(EVENT_LOG_TAG_dvm_lock_sample);

  const char* owner_filename;
  int32_t owner_line_number;
  TranslateLocation(owner_method, owner_dex_pc, &owner_filename, &owner_line_number);

  // Emit the process name, <= 37 bytes.
  {
    int fd = open("/proc/self/cmdline", O_RDONLY);
    char procName[33];
    memset(procName, 0, sizeof(procName));
    read(fd, procName, sizeof(procName) - 1);
    close(fd);
    ctx << procName;
  }

  // Emit the sensitive thread ("main thread") status. We follow tradition that this corresponds
  // to a C++ bool's value, but be explicit.
  constexpr uint32_t kIsSensitive = 1u;
  constexpr uint32_t kIsNotSensitive = 0u;
  ctx << (Thread::IsSensitiveThread() ? kIsSensitive : kIsNotSensitive);

  // Emit self thread name string.
  {
    std::string thread_name;
    self->GetThreadName(thread_name);
    ctx << thread_name;
  }

  // Emit the wait time.
  ctx << wait_ms;

  const char* filename = nullptr;
  {
    uint32_t pc;
    ArtMethod* m = self->GetCurrentMethod(&pc);
    int32_t line_number;
    TranslateLocation(m, pc, &filename, &line_number);

    // Emit the source code file name.
    ctx << filename;

    // Emit the source code line number.
    ctx << line_number;

    // Emit the method name.
    ctx << ArtMethod::PrettyMethod(m);
  }

  // Emit the lock owner source code file name.
  if (owner_filename == nullptr) {
    owner_filename = "";
  } else if (strcmp(filename, owner_filename) == 0) {
    // Common case, so save on log space.
    owner_filename = "-";
  }
  ctx << owner_filename;

  // Emit the source code line number.
  ctx << owner_line_number;

  // Emit the owner method name.
  ctx << ArtMethod::PrettyMethod(owner_method);

  // Emit the sample percentage.
  ctx << sample_percent;

  ctx << LOG_ID_EVENTS;
}

}  // namespace art
