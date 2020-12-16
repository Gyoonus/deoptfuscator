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

#include "scoped_flock.h"

#include <sys/file.h>
#include <sys/stat.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "base/unix_file/fd_file.h"

namespace art {

using android::base::StringPrintf;

/* static */ ScopedFlock LockedFile::Open(const char* filename, std::string* error_msg) {
  return Open(filename, O_CREAT | O_RDWR, true, error_msg);
}

/* static */ ScopedFlock LockedFile::Open(const char* filename, int flags, bool block,
                                          std::string* error_msg) {
  while (true) {
    // NOTE: We don't check usage here because the ScopedFlock should *never* be
    // responsible for flushing its underlying FD. Its only purpose should be
    // to acquire a lock, and the unlock / close in the corresponding
    // destructor. Callers should explicitly flush files they're writing to if
    // that is the desired behaviour.
    std::unique_ptr<File> file(OS::OpenFileWithFlags(filename, flags, false /* check_usage */));
    if (file.get() == nullptr) {
      *error_msg = StringPrintf("Failed to open file '%s': %s", filename, strerror(errno));
      return nullptr;
    }

    int operation = block ? LOCK_EX : (LOCK_EX | LOCK_NB);
    int flock_result = TEMP_FAILURE_RETRY(flock(file->Fd(), operation));
    if (flock_result == EWOULDBLOCK) {
      // File is locked by someone else and we are required not to block;
      return nullptr;
    }
    if (flock_result != 0) {
      *error_msg = StringPrintf("Failed to lock file '%s': %s", filename, strerror(errno));
      return nullptr;
    }
    struct stat fstat_stat;
    int fstat_result = TEMP_FAILURE_RETRY(fstat(file->Fd(), &fstat_stat));
    if (fstat_result != 0) {
      *error_msg = StringPrintf("Failed to fstat file '%s': %s", filename, strerror(errno));
      return nullptr;
    }
    struct stat stat_stat;
    int stat_result = TEMP_FAILURE_RETRY(stat(filename, &stat_stat));
    if (stat_result != 0) {
      PLOG(WARNING) << "Failed to stat, will retry: " << filename;
      // ENOENT can happen if someone racing with us unlinks the file we created so just retry.
      if (block) {
        continue;
      } else {
        // Note that in theory we could race with someone here for a long time and end up retrying
        // over and over again. This potential behavior does not fit well in the non-blocking
        // semantics. Thus, if we are not require to block return failure when racing.
        return nullptr;
      }
    }
    if (fstat_stat.st_dev != stat_stat.st_dev || fstat_stat.st_ino != stat_stat.st_ino) {
      LOG(WARNING) << "File changed while locking, will retry: " << filename;
      if (block) {
        continue;
      } else {
        // See comment above.
        return nullptr;
      }
    }

    return ScopedFlock(new LockedFile(std::move((*file.get()))));
  }
}

ScopedFlock LockedFile::DupOf(const int fd, const std::string& path,
                              const bool read_only_mode, std::string* error_msg) {
  // NOTE: We don't check usage here because the ScopedFlock should *never* be
  // responsible for flushing its underlying FD. Its only purpose should be
  // to acquire a lock, and the unlock / close in the corresponding
  // destructor. Callers should explicitly flush files they're writing to if
  // that is the desired behaviour.
  ScopedFlock locked_file(
      new LockedFile(dup(fd), path, false /* check_usage */, read_only_mode));
  if (locked_file->Fd() == -1) {
    *error_msg = StringPrintf("Failed to duplicate open file '%s': %s",
                              locked_file->GetPath().c_str(), strerror(errno));
    return nullptr;
  }
  if (0 != TEMP_FAILURE_RETRY(flock(locked_file->Fd(), LOCK_EX))) {
    *error_msg = StringPrintf(
        "Failed to lock file '%s': %s", locked_file->GetPath().c_str(), strerror(errno));
    return nullptr;
  }

  return locked_file;
}

void LockedFile::ReleaseLock() {
  if (this->Fd() != -1) {
    int flock_result = TEMP_FAILURE_RETRY(flock(this->Fd(), LOCK_UN));
    if (flock_result != 0) {
      // Only printing a warning is okay since this is only used with either:
      // 1) a non-blocking Init call, or
      // 2) as a part of a seperate binary (eg dex2oat) which has it's own timeout logic to prevent
      //    deadlocks.
      // This means we can be sure that the warning won't cause a deadlock.
      PLOG(WARNING) << "Unable to unlock file " << this->GetPath();
    }
  }
}

}  // namespace art
