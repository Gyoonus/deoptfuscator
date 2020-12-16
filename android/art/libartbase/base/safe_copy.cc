/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "safe_copy.h"

#include <sys/uio.h>
#include <sys/user.h>
#include <unistd.h>

#include <algorithm>

#include <android-base/macros.h>

#include "base/bit_utils.h"

namespace art {

ssize_t SafeCopy(void *dst, const void *src, size_t len) {
#if defined(__linux__)
  struct iovec dst_iov = {
    .iov_base = dst,
    .iov_len = len,
  };

  // Split up the remote read across page boundaries.
  // From the manpage:
  //   A partial read/write may result if one of the remote_iov elements points to an invalid
  //   memory region in the remote process.
  //
  //   Partial transfers apply at the granularity of iovec elements.  These system calls won't
  //   perform a partial transfer that splits a single iovec element.
  constexpr size_t kMaxIovecs = 64;
  struct iovec src_iovs[kMaxIovecs];
  size_t iovecs_used = 0;

  const char* cur = static_cast<const char*>(src);
  while (len > 0) {
    if (iovecs_used == kMaxIovecs) {
      errno = EINVAL;
      return -1;
    }

    src_iovs[iovecs_used].iov_base = const_cast<char*>(cur);
    if (!IsAlignedParam(cur, PAGE_SIZE)) {
      src_iovs[iovecs_used].iov_len = AlignUp(cur, PAGE_SIZE) - cur;
    } else {
      src_iovs[iovecs_used].iov_len = PAGE_SIZE;
    }

    src_iovs[iovecs_used].iov_len = std::min(src_iovs[iovecs_used].iov_len, len);

    len -= src_iovs[iovecs_used].iov_len;
    cur += src_iovs[iovecs_used].iov_len;
    ++iovecs_used;
  }

  ssize_t rc = process_vm_readv(getpid(), &dst_iov, 1, src_iovs, iovecs_used, 0);
  if (rc == -1) {
    return 0;
  }
  return rc;
#else
  UNUSED(dst, src, len);
  return -1;
#endif
}

}  // namespace art
