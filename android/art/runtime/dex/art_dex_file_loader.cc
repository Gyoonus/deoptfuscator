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

#include "art_dex_file_loader.h"

#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include <sys/stat.h>

#include "android-base/stringprintf.h"

#include "base/file_magic.h"
#include "base/file_utils.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "dex/compact_dex_file.h"
#include "dex/dex_file.h"
#include "dex/dex_file_verifier.h"
#include "dex/standard_dex_file.h"
#include "zip_archive.h"

namespace art {

namespace {

class MemMapContainer : public DexFileContainer {
 public:
  explicit MemMapContainer(std::unique_ptr<MemMap>&& mem_map) : mem_map_(std::move(mem_map)) { }
  virtual ~MemMapContainer() OVERRIDE { }

  int GetPermissions() OVERRIDE {
    if (mem_map_.get() == nullptr) {
      return 0;
    } else {
      return mem_map_->GetProtect();
    }
  }

  bool IsReadOnly() OVERRIDE {
    return GetPermissions() == PROT_READ;
  }

  bool EnableWrite() OVERRIDE {
    CHECK(IsReadOnly());
    if (mem_map_.get() == nullptr) {
      return false;
    } else {
      return mem_map_->Protect(PROT_READ | PROT_WRITE);
    }
  }

  bool DisableWrite() OVERRIDE {
    CHECK(!IsReadOnly());
    if (mem_map_.get() == nullptr) {
      return false;
    } else {
      return mem_map_->Protect(PROT_READ);
    }
  }

 private:
  std::unique_ptr<MemMap> mem_map_;
  DISALLOW_COPY_AND_ASSIGN(MemMapContainer);
};

}  // namespace

using android::base::StringPrintf;

static constexpr OatDexFile* kNoOatDexFile = nullptr;


bool ArtDexFileLoader::GetMultiDexChecksums(const char* filename,
                                            std::vector<uint32_t>* checksums,
                                            std::string* error_msg,
                                            int zip_fd,
                                            bool* zip_file_only_contains_uncompressed_dex) const {
  CHECK(checksums != nullptr);
  uint32_t magic;

  File fd;
  if (zip_fd != -1) {
     if (ReadMagicAndReset(zip_fd, &magic, error_msg)) {
       fd = File(zip_fd, false /* check_usage */);
     }
  } else {
    fd = OpenAndReadMagic(filename, &magic, error_msg);
  }
  if (fd.Fd() == -1) {
    DCHECK(!error_msg->empty());
    return false;
  }
  if (IsZipMagic(magic)) {
    std::unique_ptr<ZipArchive> zip_archive(
        ZipArchive::OpenFromFd(fd.Release(), filename, error_msg));
    if (zip_archive.get() == nullptr) {
      *error_msg = StringPrintf("Failed to open zip archive '%s' (error msg: %s)", filename,
                                error_msg->c_str());
      return false;
    }

    uint32_t i = 0;
    std::string zip_entry_name = GetMultiDexClassesDexName(i++);
    std::unique_ptr<ZipEntry> zip_entry(zip_archive->Find(zip_entry_name.c_str(), error_msg));
    if (zip_entry.get() == nullptr) {
      *error_msg = StringPrintf("Zip archive '%s' doesn't contain %s (error msg: %s)", filename,
          zip_entry_name.c_str(), error_msg->c_str());
      return false;
    }

    if (zip_file_only_contains_uncompressed_dex != nullptr) {
      // Start by assuming everything is uncompressed.
      *zip_file_only_contains_uncompressed_dex = true;
    }

    do {
      if (zip_file_only_contains_uncompressed_dex != nullptr) {
        if (!(zip_entry->IsUncompressed() && zip_entry->IsAlignedToDexHeader())) {
          *zip_file_only_contains_uncompressed_dex = false;
        }
      }
      checksums->push_back(zip_entry->GetCrc32());
      zip_entry_name = GetMultiDexClassesDexName(i++);
      zip_entry.reset(zip_archive->Find(zip_entry_name.c_str(), error_msg));
    } while (zip_entry.get() != nullptr);
    return true;
  }
  if (IsMagicValid(magic)) {
    std::unique_ptr<const DexFile> dex_file(OpenFile(fd.Release(),
                                                     filename,
                                                     /* verify */ false,
                                                     /* verify_checksum */ false,
                                                     /* mmap_shared */ false,
                                                     error_msg));
    if (dex_file == nullptr) {
      return false;
    }
    checksums->push_back(dex_file->GetHeader().checksum_);
    return true;
  }
  *error_msg = StringPrintf("Expected valid zip or dex file: '%s'", filename);
  return false;
}

std::unique_ptr<const DexFile> ArtDexFileLoader::Open(const uint8_t* base,
                                                      size_t size,
                                                      const std::string& location,
                                                      uint32_t location_checksum,
                                                      const OatDexFile* oat_dex_file,
                                                      bool verify,
                                                      bool verify_checksum,
                                                      std::string* error_msg) const {
  ScopedTrace trace(std::string("Open dex file from RAM ") + location);
  return OpenCommon(base,
                    size,
                    /*data_base*/ nullptr,
                    /*data_size*/ 0u,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    /*container*/ nullptr,
                    /*verify_result*/ nullptr);
}

std::unique_ptr<const DexFile> ArtDexFileLoader::Open(const std::string& location,
                                                      uint32_t location_checksum,
                                                      std::unique_ptr<MemMap> map,
                                                      bool verify,
                                                      bool verify_checksum,
                                                      std::string* error_msg) const {
  ScopedTrace trace(std::string("Open dex file from mapped-memory ") + location);
  CHECK(map.get() != nullptr);

  if (map->Size() < sizeof(DexFile::Header)) {
    *error_msg = StringPrintf(
        "DexFile: failed to open dex file '%s' that is too short to have a header",
        location.c_str());
    return nullptr;
  }

  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 /*data_base*/ nullptr,
                                                 /*data_size*/ 0u,
                                                 location,
                                                 location_checksum,
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 std::make_unique<MemMapContainer>(std::move(map)),
                                                 /*verify_result*/ nullptr);
  // Opening CompactDex is only supported from vdex files.
  if (dex_file != nullptr && dex_file->IsCompactDexFile()) {
    *error_msg = StringPrintf("Opening CompactDex file '%s' is only supported from vdex files",
                              location.c_str());
    return nullptr;
  }
  return dex_file;
}

bool ArtDexFileLoader::Open(const char* filename,
                            const std::string& location,
                            bool verify,
                            bool verify_checksum,
                            std::string* error_msg,
                            std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::Open: out-param is nullptr";
  uint32_t magic;
  File fd = OpenAndReadMagic(filename, &magic, error_msg);
  if (fd.Fd() == -1) {
    DCHECK(!error_msg->empty());
    return false;
  }
  if (IsZipMagic(magic)) {
    return OpenZip(fd.Release(), location, verify, verify_checksum, error_msg, dex_files);
  }
  if (IsMagicValid(magic)) {
    std::unique_ptr<const DexFile> dex_file(OpenFile(fd.Release(),
                                                     location,
                                                     verify,
                                                     verify_checksum,
                                                     /* mmap_shared */ false,
                                                     error_msg));
    if (dex_file.get() != nullptr) {
      dex_files->push_back(std::move(dex_file));
      return true;
    } else {
      return false;
    }
  }
  *error_msg = StringPrintf("Expected valid zip or dex file: '%s'", filename);
  return false;
}

std::unique_ptr<const DexFile> ArtDexFileLoader::OpenDex(int fd,
                                                         const std::string& location,
                                                         bool verify,
                                                         bool verify_checksum,
                                                         bool mmap_shared,
                                                         std::string* error_msg) const {
  ScopedTrace trace("Open dex file " + std::string(location));
  return OpenFile(fd, location, verify, verify_checksum, mmap_shared, error_msg);
}

bool ArtDexFileLoader::OpenZip(int fd,
                               const std::string& location,
                               bool verify,
                               bool verify_checksum,
                               std::string* error_msg,
                               std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
  ScopedTrace trace("Dex file open Zip " + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::OpenZip: out-param is nullptr";
  std::unique_ptr<ZipArchive> zip_archive(ZipArchive::OpenFromFd(fd, location.c_str(), error_msg));
  if (zip_archive.get() == nullptr) {
    DCHECK(!error_msg->empty());
    return false;
  }
  return OpenAllDexFilesFromZip(
      *zip_archive, location, verify, verify_checksum, error_msg, dex_files);
}

std::unique_ptr<const DexFile> ArtDexFileLoader::OpenFile(int fd,
                                                          const std::string& location,
                                                          bool verify,
                                                          bool verify_checksum,
                                                          bool mmap_shared,
                                                          std::string* error_msg) const {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  CHECK(!location.empty());
  std::unique_ptr<MemMap> map;
  {
    File delayed_close(fd, /* check_usage */ false);
    struct stat sbuf;
    memset(&sbuf, 0, sizeof(sbuf));
    if (fstat(fd, &sbuf) == -1) {
      *error_msg = StringPrintf("DexFile: fstat '%s' failed: %s", location.c_str(),
                                strerror(errno));
      return nullptr;
    }
    if (S_ISDIR(sbuf.st_mode)) {
      *error_msg = StringPrintf("Attempt to mmap directory '%s'", location.c_str());
      return nullptr;
    }
    size_t length = sbuf.st_size;
    map.reset(MemMap::MapFile(length,
                              PROT_READ,
                              mmap_shared ? MAP_SHARED : MAP_PRIVATE,
                              fd,
                              0,
                              /*low_4gb*/false,
                              location.c_str(),
                              error_msg));
    if (map == nullptr) {
      DCHECK(!error_msg->empty());
      return nullptr;
    }
  }

  if (map->Size() < sizeof(DexFile::Header)) {
    *error_msg = StringPrintf(
        "DexFile: failed to open dex file '%s' that is too short to have a header",
        location.c_str());
    return nullptr;
  }

  const DexFile::Header* dex_header = reinterpret_cast<const DexFile::Header*>(map->Begin());

  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 /*data_base*/ nullptr,
                                                 /*data_size*/ 0u,
                                                 location,
                                                 dex_header->checksum_,
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 std::make_unique<MemMapContainer>(std::move(map)),
                                                 /*verify_result*/ nullptr);

  // Opening CompactDex is only supported from vdex files.
  if (dex_file != nullptr && dex_file->IsCompactDexFile()) {
    *error_msg = StringPrintf("Opening CompactDex file '%s' is only supported from vdex files",
                              location.c_str());
    return nullptr;
  }
  return dex_file;
}

std::unique_ptr<const DexFile> ArtDexFileLoader::OpenOneDexFileFromZip(
    const ZipArchive& zip_archive,
    const char* entry_name,
    const std::string& location,
    bool verify,
    bool verify_checksum,
    std::string* error_msg,
    ZipOpenErrorCode* error_code) const {
  ScopedTrace trace("Dex file open from Zip Archive " + std::string(location));
  CHECK(!location.empty());
  std::unique_ptr<ZipEntry> zip_entry(zip_archive.Find(entry_name, error_msg));
  if (zip_entry == nullptr) {
    *error_code = ZipOpenErrorCode::kEntryNotFound;
    return nullptr;
  }
  if (zip_entry->GetUncompressedLength() == 0) {
    *error_msg = StringPrintf("Dex file '%s' has zero length", location.c_str());
    *error_code = ZipOpenErrorCode::kDexFileError;
    return nullptr;
  }

  std::unique_ptr<MemMap> map;
  if (zip_entry->IsUncompressed()) {
    if (!zip_entry->IsAlignedTo(alignof(DexFile::Header))) {
      // Do not mmap unaligned ZIP entries because
      // doing so would fail dex verification which requires 4 byte alignment.
      LOG(WARNING) << "Can't mmap dex file " << location << "!" << entry_name << " directly; "
                   << "please zipalign to " << alignof(DexFile::Header) << " bytes. "
                   << "Falling back to extracting file.";
    } else {
      // Map uncompressed files within zip as file-backed to avoid a dirty copy.
      map.reset(zip_entry->MapDirectlyFromFile(location.c_str(), /*out*/error_msg));
      if (map == nullptr) {
        LOG(WARNING) << "Can't mmap dex file " << location << "!" << entry_name << " directly; "
                     << "is your ZIP file corrupted? Falling back to extraction.";
        // Try again with Extraction which still has a chance of recovery.
      }
    }
  }

  if (map == nullptr) {
    // Default path for compressed ZIP entries,
    // and fallback for stored ZIP entries.
    map.reset(zip_entry->ExtractToMemMap(location.c_str(), entry_name, error_msg));
  }

  if (map == nullptr) {
    *error_msg = StringPrintf("Failed to extract '%s' from '%s': %s", entry_name, location.c_str(),
                              error_msg->c_str());
    *error_code = ZipOpenErrorCode::kExtractToMemoryError;
    return nullptr;
  }
  VerifyResult verify_result;
  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 /*data_base*/ nullptr,
                                                 /*data_size*/ 0u,
                                                 location,
                                                 zip_entry->GetCrc32(),
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 std::make_unique<MemMapContainer>(std::move(map)),
                                                 &verify_result);
  if (dex_file != nullptr && dex_file->IsCompactDexFile()) {
    *error_msg = StringPrintf("Opening CompactDex file '%s' is only supported from vdex files",
                              location.c_str());
    return nullptr;
  }
  if (dex_file == nullptr) {
    if (verify_result == VerifyResult::kVerifyNotAttempted) {
      *error_code = ZipOpenErrorCode::kDexFileError;
    } else {
      *error_code = ZipOpenErrorCode::kVerifyError;
    }
    return nullptr;
  }
  if (!dex_file->DisableWrite()) {
    *error_msg = StringPrintf("Failed to make dex file '%s' read only", location.c_str());
    *error_code = ZipOpenErrorCode::kMakeReadOnlyError;
    return nullptr;
  }
  CHECK(dex_file->IsReadOnly()) << location;
  if (verify_result != VerifyResult::kVerifySucceeded) {
    *error_code = ZipOpenErrorCode::kVerifyError;
    return nullptr;
  }
  *error_code = ZipOpenErrorCode::kNoError;
  return dex_file;
}

// Technically we do not have a limitation with respect to the number of dex files that can be in a
// multidex APK. However, it's bad practice, as each dex file requires its own tables for symbols
// (types, classes, methods, ...) and dex caches. So warn the user that we open a zip with what
// seems an excessive number.
static constexpr size_t kWarnOnManyDexFilesThreshold = 100;

bool ArtDexFileLoader::OpenAllDexFilesFromZip(
    const ZipArchive& zip_archive,
    const std::string& location,
    bool verify,
    bool verify_checksum,
    std::string* error_msg,
    std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
  ScopedTrace trace("Dex file open from Zip " + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::OpenFromZip: out-param is nullptr";
  ZipOpenErrorCode error_code;
  std::unique_ptr<const DexFile> dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                kClassesDex,
                                                                location,
                                                                verify,
                                                                verify_checksum,
                                                                error_msg,
                                                                &error_code));
  if (dex_file.get() == nullptr) {
    return false;
  } else {
    // Had at least classes.dex.
    dex_files->push_back(std::move(dex_file));

    // Now try some more.

    // We could try to avoid std::string allocations by working on a char array directly. As we
    // do not expect a lot of iterations, this seems too involved and brittle.

    for (size_t i = 1; ; ++i) {
      std::string name = GetMultiDexClassesDexName(i);
      std::string fake_location = GetMultiDexLocation(i, location.c_str());
      std::unique_ptr<const DexFile> next_dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                         name.c_str(),
                                                                         fake_location,
                                                                         verify,
                                                                         verify_checksum,
                                                                         error_msg,
                                                                         &error_code));
      if (next_dex_file.get() == nullptr) {
        if (error_code != ZipOpenErrorCode::kEntryNotFound) {
          LOG(WARNING) << "Zip open failed: " << *error_msg;
        }
        break;
      } else {
        dex_files->push_back(std::move(next_dex_file));
      }

      if (i == kWarnOnManyDexFilesThreshold) {
        LOG(WARNING) << location << " has in excess of " << kWarnOnManyDexFilesThreshold
                     << " dex files. Please consider coalescing and shrinking the number to "
                        " avoid runtime overhead.";
      }

      if (i == std::numeric_limits<size_t>::max()) {
        LOG(ERROR) << "Overflow in number of dex files!";
        break;
      }
    }

    return true;
  }
}

std::unique_ptr<DexFile> ArtDexFileLoader::OpenCommon(const uint8_t* base,
                                                      size_t size,
                                                      const uint8_t* data_base,
                                                      size_t data_size,
                                                      const std::string& location,
                                                      uint32_t location_checksum,
                                                      const OatDexFile* oat_dex_file,
                                                      bool verify,
                                                      bool verify_checksum,
                                                      std::string* error_msg,
                                                      std::unique_ptr<DexFileContainer> container,
                                                      VerifyResult* verify_result) {
  std::unique_ptr<DexFile> dex_file = DexFileLoader::OpenCommon(base,
                                                                size,
                                                                data_base,
                                                                data_size,
                                                                location,
                                                                location_checksum,
                                                                oat_dex_file,
                                                                verify,
                                                                verify_checksum,
                                                                error_msg,
                                                                std::move(container),
                                                                verify_result);

  // Check if this dex file is located in the framework directory.
  // If it is, set a flag on the dex file. This is used by hidden API
  // policy decision logic.
  // Location can contain multidex suffix, so fetch its canonical version. Note
  // that this will call `realpath`.
  std::string path = DexFileLoader::GetDexCanonicalLocation(location.c_str());
  if (dex_file != nullptr && LocationIsOnSystemFramework(path.c_str())) {
    dex_file->SetIsPlatformDexFile();
  }

  return dex_file;
}

}  // namespace art
