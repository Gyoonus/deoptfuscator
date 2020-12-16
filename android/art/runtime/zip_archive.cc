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

#include "zip_archive.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "android-base/stringprintf.h"
#include "ziparchive/zip_archive.h"

#include "base/bit_utils.h"
#include "base/unix_file/fd_file.h"
#include "dex/dex_file.h"

namespace art {

// Log file contents and mmap info when mapping entries directly.
static constexpr const bool kDebugZipMapDirectly = false;

using android::base::StringPrintf;

uint32_t ZipEntry::GetUncompressedLength() {
  return zip_entry_->uncompressed_length;
}

uint32_t ZipEntry::GetCrc32() {
  return zip_entry_->crc32;
}

bool ZipEntry::IsUncompressed() {
  return zip_entry_->method == kCompressStored;
}

bool ZipEntry::IsAlignedTo(size_t alignment) const {
  DCHECK(IsPowerOfTwo(alignment)) << alignment;
  return IsAlignedParam(zip_entry_->offset, static_cast<int>(alignment));
}

bool ZipEntry::IsAlignedToDexHeader() const {
  return IsAlignedTo(alignof(DexFile::Header));
}

ZipEntry::~ZipEntry() {
  delete zip_entry_;
}

bool ZipEntry::ExtractToFile(File& file, std::string* error_msg) {
  const int32_t error = ExtractEntryToFile(handle_, zip_entry_, file.Fd());
  if (error) {
    *error_msg = std::string(ErrorCodeString(error));
    return false;
  }

  return true;
}

MemMap* ZipEntry::ExtractToMemMap(const char* zip_filename, const char* entry_filename,
                                  std::string* error_msg) {
  std::string name(entry_filename);
  name += " extracted in memory from ";
  name += zip_filename;
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous(name.c_str(),
                                                   nullptr, GetUncompressedLength(),
                                                   PROT_READ | PROT_WRITE, false, false,
                                                   error_msg));
  if (map.get() == nullptr) {
    DCHECK(!error_msg->empty());
    return nullptr;
  }

  const int32_t error = ExtractToMemory(handle_, zip_entry_,
                                        map->Begin(), map->Size());
  if (error) {
    *error_msg = std::string(ErrorCodeString(error));
    return nullptr;
  }

  return map.release();
}

MemMap* ZipEntry::MapDirectlyFromFile(const char* zip_filename, std::string* error_msg) {
  const int zip_fd = GetFileDescriptor(handle_);
  const char* entry_filename = entry_name_.c_str();

  // Should not happen since we don't have a memory ZipArchive constructor.
  // However the underlying ZipArchive isn't required to have an FD,
  // so check to be sure.
  CHECK_GE(zip_fd, 0) <<
      StringPrintf("Cannot map '%s' (in zip '%s') directly because the zip archive "
                   "is not file backed.",
                   entry_filename,
                   zip_filename);

  if (!IsUncompressed()) {
    *error_msg = StringPrintf("Cannot map '%s' (in zip '%s') directly because it is compressed.",
                              entry_filename,
                              zip_filename);
    return nullptr;
  } else if (zip_entry_->uncompressed_length != zip_entry_->compressed_length) {
    *error_msg = StringPrintf("Cannot map '%s' (in zip '%s') directly because "
                              "entry has bad size (%u != %u).",
                              entry_filename,
                              zip_filename,
                              zip_entry_->uncompressed_length,
                              zip_entry_->compressed_length);
    return nullptr;
  }

  std::string name(entry_filename);
  name += " mapped directly in memory from ";
  name += zip_filename;

  const off_t offset = zip_entry_->offset;

  if (kDebugZipMapDirectly) {
    LOG(INFO) << "zip_archive: " << "make mmap of " << name << " @ offset = " << offset;
  }

  std::unique_ptr<MemMap> map(
      MemMap::MapFileAtAddress(nullptr,  // Expected pointer address
                               GetUncompressedLength(),  // Byte count
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE,
                               zip_fd,
                               offset,
                               false,  // Don't restrict allocation to lower4GB
                               false,  // Doesn't overlap existing map (reuse=false)
                               name.c_str(),
                               /*out*/error_msg));

  if (map == nullptr) {
    DCHECK(!error_msg->empty());
  }

  if (kDebugZipMapDirectly) {
    // Dump contents of file, same format as using this shell command:
    // $> od -j <offset> -t x1 <zip_filename>
    static constexpr const int kMaxDumpChars = 15;
    lseek(zip_fd, 0, SEEK_SET);

    int count = offset + kMaxDumpChars;

    std::string tmp;
    char buf;

    // Dump file contents.
    int i = 0;
    while (read(zip_fd, &buf, 1) > 0 && i < count) {
      tmp += StringPrintf("%3d ", (unsigned int)buf);
      ++i;
    }

    LOG(INFO) << "map_fd raw bytes starting at 0";
    LOG(INFO) << "" << tmp;
    LOG(INFO) << "---------------------------";

    // Dump map contents.
    if (map != nullptr) {
      tmp = "";

      count = kMaxDumpChars;

      uint8_t* begin = map->Begin();
      for (i = 0; i < count; ++i) {
        tmp += StringPrintf("%3d ", (unsigned int)begin[i]);
      }

      LOG(INFO) << "map address " << StringPrintf("%p", begin);
      LOG(INFO) << "map first " << kMaxDumpChars << " chars:";
      LOG(INFO) << tmp;
    }
  }

  return map.release();
}

MemMap* ZipEntry::MapDirectlyOrExtract(const char* zip_filename,
                                       const char* entry_filename,
                                       std::string* error_msg) {
  if (IsUncompressed() && GetFileDescriptor(handle_) >= 0) {
    MemMap* ret = MapDirectlyFromFile(zip_filename, error_msg);
    if (ret != nullptr) {
      return ret;
    }
  }
  // Fall back to extraction for the failure case.
  return ExtractToMemMap(zip_filename, entry_filename, error_msg);
}

static void SetCloseOnExec(int fd) {
  // This dance is more portable than Linux's O_CLOEXEC open(2) flag.
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    PLOG(WARNING) << "fcntl(" << fd << ", F_GETFD) failed";
    return;
  }
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  if (rc == -1) {
    PLOG(WARNING) << "fcntl(" << fd << ", F_SETFD, " << flags << ") failed";
    return;
  }
}

ZipArchive* ZipArchive::Open(const char* filename, std::string* error_msg) {
  DCHECK(filename != nullptr);

  ZipArchiveHandle handle;
  const int32_t error = OpenArchive(filename, &handle);
  if (error) {
    *error_msg = std::string(ErrorCodeString(error));
    CloseArchive(handle);
    return nullptr;
  }

  SetCloseOnExec(GetFileDescriptor(handle));
  return new ZipArchive(handle);
}

ZipArchive* ZipArchive::OpenFromFd(int fd, const char* filename, std::string* error_msg) {
  DCHECK(filename != nullptr);
  DCHECK_GT(fd, 0);

  ZipArchiveHandle handle;
  const int32_t error = OpenArchiveFd(fd, filename, &handle);
  if (error) {
    *error_msg = std::string(ErrorCodeString(error));
    CloseArchive(handle);
    return nullptr;
  }

  SetCloseOnExec(GetFileDescriptor(handle));
  return new ZipArchive(handle);
}

ZipEntry* ZipArchive::Find(const char* name, std::string* error_msg) const {
  DCHECK(name != nullptr);

  // Resist the urge to delete the space. <: is a bigraph sequence.
  std::unique_ptr< ::ZipEntry> zip_entry(new ::ZipEntry);
  const int32_t error = FindEntry(handle_, ZipString(name), zip_entry.get());
  if (error) {
    *error_msg = std::string(ErrorCodeString(error));
    return nullptr;
  }

  return new ZipEntry(handle_, zip_entry.release(), name);
}

ZipArchive::~ZipArchive() {
  CloseArchive(handle_);
}

}  // namespace art
