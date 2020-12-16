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

#ifndef ART_COMPILER_LINKER_ERROR_DELAYING_OUTPUT_STREAM_H_
#define ART_COMPILER_LINKER_ERROR_DELAYING_OUTPUT_STREAM_H_

#include "output_stream.h"

#include <android-base/logging.h>

#include "base/macros.h"

namespace art {
namespace linker {

// OutputStream wrapper that delays reporting an error until Flush().
class ErrorDelayingOutputStream FINAL : public OutputStream {
 public:
  explicit ErrorDelayingOutputStream(OutputStream* output)
      : OutputStream(output->GetLocation()),
        output_(output),
        output_good_(true),
        output_offset_(0) { }

  // This function always succeeds to simplify code.
  // Use Good() to check the actual status of the output stream.
  bool WriteFully(const void* buffer, size_t byte_count) OVERRIDE {
    if (output_good_) {
      if (!output_->WriteFully(buffer, byte_count)) {
        PLOG(ERROR) << "Failed to write " << byte_count
                    << " bytes to " << GetLocation() << " at offset " << output_offset_;
        output_good_ = false;
      }
    }
    output_offset_ += byte_count;
    return true;
  }

  // This function always succeeds to simplify code.
  // Use Good() to check the actual status of the output stream.
  off_t Seek(off_t offset, Whence whence) OVERRIDE {
    // We keep shadow copy of the offset so that we return
    // the expected value even if the output stream failed.
    off_t new_offset;
    switch (whence) {
      case kSeekSet:
        new_offset = offset;
        break;
      case kSeekCurrent:
        new_offset = output_offset_ + offset;
        break;
      default:
        LOG(FATAL) << "Unsupported seek type: " << whence;
        UNREACHABLE();
    }
    if (output_good_) {
      off_t actual_offset = output_->Seek(offset, whence);
      if (actual_offset == static_cast<off_t>(-1)) {
        PLOG(ERROR) << "Failed to seek in " << GetLocation() << ". Offset=" << offset
                    << " whence=" << whence << " new_offset=" << new_offset;
        output_good_ = false;
      }
      DCHECK_EQ(actual_offset, new_offset);
    }
    output_offset_ = new_offset;
    return new_offset;
  }

  // Flush the output and return whether all operations have succeeded.
  // Do nothing if we already have a pending error.
  bool Flush() OVERRIDE {
    if (output_good_) {
      output_good_ = output_->Flush();
    }
    return output_good_;
  }

  // Check (without flushing) whether all operations have succeeded so far.
  bool Good() const {
    return output_good_;
  }

 private:
  OutputStream* output_;
  bool output_good_;  // True if all writes to output succeeded.
  off_t output_offset_;  // Keep track of the current position in the stream.
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_ERROR_DELAYING_OUTPUT_STREAM_H_
