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

#include "oat_file.h"

#include <dlfcn.h>
#ifndef __APPLE__
#include <link.h>  // for dl_iterate_phdr.
#endif
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <type_traits>
#include <sys/stat.h>

// dlopen_ext support from bionic.
#ifdef ART_TARGET_ANDROID
#include "android/dlext.h"
#endif

#include "android-base/stringprintf.h"

#include "art_method.h"
#include "base/bit_vector.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/logging.h"  // For VLOG_IS_ON.
#include "base/os.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_types.h"
#include "dex/standard_dex_file.h"
#include "dex/utf-inl.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "gc_root.h"
#include "gc/space/image_space.h"
#include "mem_map.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "oat.h"
#include "oat_file-inl.h"
#include "oat_file_manager.h"
#include "runtime.h"
#include "type_lookup_table.h"
#include "vdex_file.h"

namespace art {

using android::base::StringPrintf;

// Whether OatFile::Open will try dlopen. Fallback is our own ELF loader.
static constexpr bool kUseDlopen = true;

// Whether OatFile::Open will try dlopen on the host. On the host we're not linking against
// bionic, so cannot take advantage of the support for changed semantics (loading the same soname
// multiple times). However, if/when we switch the above, we likely want to switch this, too,
// to get test coverage of the code paths.
static constexpr bool kUseDlopenOnHost = true;

// For debugging, Open will print DlOpen error message if set to true.
static constexpr bool kPrintDlOpenErrorMessage = false;

// Note for OatFileBase and descendents:
//
// These are used in OatFile::Open to try all our loaders.
//
// The process is simple:
//
// 1) Allocate an instance through the standard constructor (location, executable)
// 2) Load() to try to open the file.
// 3) ComputeFields() to populate the OatFile fields like begin_, using FindDynamicSymbolAddress.
// 4) PreSetup() for any steps that should be done before the final setup.
// 5) Setup() to complete the procedure.

class OatFileBase : public OatFile {
 public:
  virtual ~OatFileBase() {}

  template <typename kOatFileBaseSubType>
  static OatFileBase* OpenOatFile(int zip_fd,
                                  const std::string& vdex_filename,
                                  const std::string& elf_filename,
                                  const std::string& location,
                                  uint8_t* requested_base,
                                  uint8_t* oat_file_begin,
                                  bool writable,
                                  bool executable,
                                  bool low_4gb,
                                  const char* abs_dex_location,
                                  std::string* error_msg);

  template <typename kOatFileBaseSubType>
  static OatFileBase* OpenOatFile(int zip_fd,
                                  int vdex_fd,
                                  int oat_fd,
                                  const std::string& vdex_filename,
                                  const std::string& oat_filename,
                                  uint8_t* requested_base,
                                  uint8_t* oat_file_begin,
                                  bool writable,
                                  bool executable,
                                  bool low_4gb,
                                  const char* abs_dex_location,
                                  std::string* error_msg);

 protected:
  OatFileBase(const std::string& filename, bool executable) : OatFile(filename, executable) {}

  virtual const uint8_t* FindDynamicSymbolAddress(const std::string& symbol_name,
                                                  std::string* error_msg) const = 0;

  virtual void PreLoad() = 0;

  bool LoadVdex(const std::string& vdex_filename,
                bool writable,
                bool low_4gb,
                std::string* error_msg);

  bool LoadVdex(int vdex_fd,
                const std::string& vdex_filename,
                bool writable,
                bool low_4gb,
                std::string* error_msg);

  virtual bool Load(const std::string& elf_filename,
                    uint8_t* oat_file_begin,
                    bool writable,
                    bool executable,
                    bool low_4gb,
                    std::string* error_msg) = 0;

  virtual bool Load(int oat_fd,
                    uint8_t* oat_file_begin,
                    bool writable,
                    bool executable,
                    bool low_4gb,
                    std::string* error_msg) = 0;

  bool ComputeFields(uint8_t* requested_base,
                     const std::string& file_path,
                     std::string* error_msg);

  virtual void PreSetup(const std::string& elf_filename) = 0;

  bool Setup(int zip_fd, const char* abs_dex_location, std::string* error_msg);

  // Setters exposed for ElfOatFile.

  void SetBegin(const uint8_t* begin) {
    begin_ = begin;
  }

  void SetEnd(const uint8_t* end) {
    end_ = end;
  }

  void SetVdex(VdexFile* vdex) {
    vdex_.reset(vdex);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OatFileBase);
};

template <typename kOatFileBaseSubType>
OatFileBase* OatFileBase::OpenOatFile(int zip_fd,
                                      const std::string& vdex_filename,
                                      const std::string& elf_filename,
                                      const std::string& location,
                                      uint8_t* requested_base,
                                      uint8_t* oat_file_begin,
                                      bool writable,
                                      bool executable,
                                      bool low_4gb,
                                      const char* abs_dex_location,
                                      std::string* error_msg) {
  std::unique_ptr<OatFileBase> ret(new kOatFileBaseSubType(location, executable));

  ret->PreLoad();

  if (!ret->Load(elf_filename,
                 oat_file_begin,
                 writable,
                 executable,
                 low_4gb,
                 error_msg)) {
    return nullptr;
  }

  if (!ret->ComputeFields(requested_base, elf_filename, error_msg)) {
    return nullptr;
  }

  if (!ret->LoadVdex(vdex_filename, writable, low_4gb, error_msg)) {
    return nullptr;
  }

  ret->PreSetup(elf_filename);

  if (!ret->Setup(zip_fd, abs_dex_location, error_msg)) {
    return nullptr;
  }

  return ret.release();
}

template <typename kOatFileBaseSubType>
OatFileBase* OatFileBase::OpenOatFile(int zip_fd,
                                      int vdex_fd,
                                      int oat_fd,
                                      const std::string& vdex_location,
                                      const std::string& oat_location,
                                      uint8_t* requested_base,
                                      uint8_t* oat_file_begin,
                                      bool writable,
                                      bool executable,
                                      bool low_4gb,
                                      const char* abs_dex_location,
                                      std::string* error_msg) {
  std::unique_ptr<OatFileBase> ret(new kOatFileBaseSubType(oat_location, executable));

  if (!ret->Load(oat_fd,
                 oat_file_begin,
                 writable,
                 executable,
                 low_4gb,
                 error_msg)) {
    return nullptr;
  }

  if (!ret->ComputeFields(requested_base, oat_location, error_msg)) {
    return nullptr;
  }

  if (!ret->LoadVdex(vdex_fd, vdex_location, writable, low_4gb, error_msg)) {
    return nullptr;
  }

  ret->PreSetup(oat_location);

  if (!ret->Setup(zip_fd, abs_dex_location, error_msg)) {
    return nullptr;
  }

  return ret.release();
}

bool OatFileBase::LoadVdex(const std::string& vdex_filename,
                           bool writable,
                           bool low_4gb,
                           std::string* error_msg) {
  vdex_ = VdexFile::OpenAtAddress(vdex_begin_,
                                  vdex_end_ - vdex_begin_,
                                  vdex_begin_ != nullptr /* mmap_reuse */,
                                  vdex_filename,
                                  writable,
                                  low_4gb,
                                  /* unquicken*/ false,
                                  error_msg);
  if (vdex_.get() == nullptr) {
    *error_msg = StringPrintf("Failed to load vdex file '%s' %s",
                              vdex_filename.c_str(),
                              error_msg->c_str());
    return false;
  }
  return true;
}

bool OatFileBase::LoadVdex(int vdex_fd,
                           const std::string& vdex_filename,
                           bool writable,
                           bool low_4gb,
                           std::string* error_msg) {
  if (vdex_fd != -1) {
    struct stat s;
    int rc = TEMP_FAILURE_RETRY(fstat(vdex_fd, &s));
    if (rc == -1) {
      PLOG(WARNING) << "Failed getting length of vdex file";
    } else {
      vdex_ = VdexFile::OpenAtAddress(vdex_begin_,
                                      vdex_end_ - vdex_begin_,
                                      vdex_begin_ != nullptr /* mmap_reuse */,
                                      vdex_fd,
                                      s.st_size,
                                      vdex_filename,
                                      writable,
                                      low_4gb,
                                      false /* unquicken */,
                                      error_msg);
      if (vdex_.get() == nullptr) {
        *error_msg = "Failed opening vdex file.";
        return false;
      }
    }
  }
  return true;
}

bool OatFileBase::ComputeFields(uint8_t* requested_base,
                                const std::string& file_path,
                                std::string* error_msg) {
  std::string symbol_error_msg;
  begin_ = FindDynamicSymbolAddress("oatdata", &symbol_error_msg);
  if (begin_ == nullptr) {
    *error_msg = StringPrintf("Failed to find oatdata symbol in '%s' %s",
                              file_path.c_str(),
                              symbol_error_msg.c_str());
    return false;
  }
  if (requested_base != nullptr && begin_ != requested_base) {
    // Host can fail this check. Do not dump there to avoid polluting the output.
    if (kIsTargetBuild && (kIsDebugBuild || VLOG_IS_ON(oat))) {
      PrintFileToLog("/proc/self/maps", android::base::LogSeverity::WARNING);
    }
    *error_msg = StringPrintf("Failed to find oatdata symbol at expected address: "
        "oatdata=%p != expected=%p. See process maps in the log.",
        begin_, requested_base);
    return false;
  }
  end_ = FindDynamicSymbolAddress("oatlastword", &symbol_error_msg);
  if (end_ == nullptr) {
    *error_msg = StringPrintf("Failed to find oatlastword symbol in '%s' %s",
                              file_path.c_str(),
                              symbol_error_msg.c_str());
    return false;
  }
  // Readjust to be non-inclusive upper bound.
  end_ += sizeof(uint32_t);

  bss_begin_ = const_cast<uint8_t*>(FindDynamicSymbolAddress("oatbss", &symbol_error_msg));
  if (bss_begin_ == nullptr) {
    // No .bss section.
    bss_end_ = nullptr;
  } else {
    bss_end_ = const_cast<uint8_t*>(FindDynamicSymbolAddress("oatbsslastword", &symbol_error_msg));
    if (bss_end_ == nullptr) {
      *error_msg = StringPrintf("Failed to find oatbsslastword symbol in '%s'", file_path.c_str());
      return false;
    }
    // Readjust to be non-inclusive upper bound.
    bss_end_ += sizeof(uint32_t);
    // Find bss methods if present.
    bss_methods_ =
        const_cast<uint8_t*>(FindDynamicSymbolAddress("oatbssmethods", &symbol_error_msg));
    // Find bss roots if present.
    bss_roots_ = const_cast<uint8_t*>(FindDynamicSymbolAddress("oatbssroots", &symbol_error_msg));
  }

  vdex_begin_ = const_cast<uint8_t*>(FindDynamicSymbolAddress("oatdex", &symbol_error_msg));
  if (vdex_begin_ == nullptr) {
    // No .vdex section.
    vdex_end_ = nullptr;
  } else {
    vdex_end_ = const_cast<uint8_t*>(FindDynamicSymbolAddress("oatdexlastword", &symbol_error_msg));
    if (vdex_end_ == nullptr) {
      *error_msg = StringPrintf("Failed to find oatdexlastword symbol in '%s'", file_path.c_str());
      return false;
    }
    // Readjust to be non-inclusive upper bound.
    vdex_end_ += sizeof(uint32_t);
  }

  return true;
}

// Read an unaligned entry from the OatDexFile data in OatFile and advance the read
// position by the number of bytes read, i.e. sizeof(T).
// Return true on success, false if the read would go beyond the end of the OatFile.
template <typename T>
inline static bool ReadOatDexFileData(const OatFile& oat_file,
                                      /*inout*/const uint8_t** oat,
                                      /*out*/T* value) {
  DCHECK(oat != nullptr);
  DCHECK(value != nullptr);
  DCHECK_LE(*oat, oat_file.End());
  if (UNLIKELY(static_cast<size_t>(oat_file.End() - *oat) < sizeof(T))) {
    return false;
  }
  static_assert(std::is_trivial<T>::value, "T must be a trivial type");
  typedef __attribute__((__aligned__(1))) T unaligned_type;
  *value = *reinterpret_cast<const unaligned_type*>(*oat);
  *oat += sizeof(T);
  return true;
}

static inline bool MapConstantTables(const gc::space::ImageSpace* space,
                                     uint8_t* address) {
  // If MREMAP_DUP is ever merged to Linux kernel, use it to avoid the unnecessary open()/close().
  // Note: The current approach relies on the filename still referencing the same inode.

  File file(space->GetImageFilename(), O_RDONLY, /* checkUsage */ false);
  if (!file.IsOpened()) {
    LOG(ERROR) << "Failed to open boot image file " << space->GetImageFilename();
    return false;
  }

  uint32_t offset = space->GetImageHeader().GetBootImageConstantTablesOffset();
  uint32_t size = space->GetImageHeader().GetBootImageConstantTablesSize();
  std::string error_msg;
  std::unique_ptr<MemMap> mem_map(MemMap::MapFileAtAddress(address,
                                                           size,
                                                           PROT_READ,
                                                           MAP_PRIVATE,
                                                           file.Fd(),
                                                           offset,
                                                           /* low_4gb */ false,
                                                           /* reuse */ true,
                                                           file.GetPath().c_str(),
                                                           &error_msg));
  if (mem_map == nullptr) {
    LOG(ERROR) << "Failed to mmap boot image tables from file " << space->GetImageFilename();
    return false;
  }
  return true;
}

static bool ReadIndexBssMapping(OatFile* oat_file,
                                /*inout*/const uint8_t** oat,
                                size_t dex_file_index,
                                const std::string& dex_file_location,
                                const char* tag,
                                /*out*/const IndexBssMapping** mapping,
                                std::string* error_msg) {
  uint32_t index_bss_mapping_offset;
  if (UNLIKELY(!ReadOatDexFileData(*oat_file, oat, &index_bss_mapping_offset))) {
    *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zd for '%s' truncated "
                                  "after %s bss mapping offset",
                              oat_file->GetLocation().c_str(),
                              dex_file_index,
                              dex_file_location.c_str(),
                              tag);
    return false;
  }
  const bool readable_index_bss_mapping_size =
      index_bss_mapping_offset != 0u &&
      index_bss_mapping_offset <= oat_file->Size() &&
      IsAligned<alignof(IndexBssMapping)>(index_bss_mapping_offset) &&
      oat_file->Size() - index_bss_mapping_offset >= IndexBssMapping::ComputeSize(0);
  const IndexBssMapping* index_bss_mapping = readable_index_bss_mapping_size
      ? reinterpret_cast<const IndexBssMapping*>(oat_file->Begin() + index_bss_mapping_offset)
      : nullptr;
  if (index_bss_mapping_offset != 0u &&
      (UNLIKELY(index_bss_mapping == nullptr) ||
          UNLIKELY(index_bss_mapping->size() == 0u) ||
          UNLIKELY(oat_file->Size() - index_bss_mapping_offset <
                   IndexBssMapping::ComputeSize(index_bss_mapping->size())))) {
    *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with unaligned or "
                                  " truncated %s bss mapping, offset %u of %zu, length %zu",
                              oat_file->GetLocation().c_str(),
                              dex_file_index,
                              dex_file_location.c_str(),
                              tag,
                              index_bss_mapping_offset,
                              oat_file->Size(),
                              index_bss_mapping != nullptr ? index_bss_mapping->size() : 0u);
    return false;
  }

  *mapping = index_bss_mapping;
  return true;
}

static void DCheckIndexToBssMapping(OatFile* oat_file,
                                    uint32_t number_of_indexes,
                                    size_t slot_size,
                                    const IndexBssMapping* index_bss_mapping) {
  if (kIsDebugBuild && index_bss_mapping != nullptr) {
    size_t index_bits = IndexBssMappingEntry::IndexBits(number_of_indexes);
    const IndexBssMappingEntry* prev_entry = nullptr;
    for (const IndexBssMappingEntry& entry : *index_bss_mapping) {
      CHECK_ALIGNED_PARAM(entry.bss_offset, slot_size);
      // When loading a non-executable ElfOatFile, .bss symbols are not even
      // looked up, so we cannot verify the offset against BssSize().
      if (oat_file->IsExecutable()) {
        CHECK_LT(entry.bss_offset, oat_file->BssSize());
      }
      uint32_t mask = entry.GetMask(index_bits);
      CHECK_LE(POPCOUNT(mask) * slot_size, entry.bss_offset);
      size_t index_mask_span = (mask != 0u) ? 32u - index_bits - CTZ(mask) : 0u;
      CHECK_LE(index_mask_span, entry.GetIndex(index_bits));
      if (prev_entry != nullptr) {
        CHECK_LT(prev_entry->GetIndex(index_bits), entry.GetIndex(index_bits) - index_mask_span);
      }
      prev_entry = &entry;
    }
    CHECK_LT(prev_entry->GetIndex(index_bits), number_of_indexes);
  }
}

bool OatFileBase::Setup(int zip_fd, const char* abs_dex_location, std::string* error_msg) {
  if (!GetOatHeader().IsValid()) {
    std::string cause = GetOatHeader().GetValidationErrorMessage();
    *error_msg = StringPrintf("Invalid oat header for '%s': %s",
                              GetLocation().c_str(),
                              cause.c_str());
    return false;
  }
  PointerSize pointer_size = GetInstructionSetPointerSize(GetOatHeader().GetInstructionSet());
  size_t key_value_store_size =
      (Size() >= sizeof(OatHeader)) ? GetOatHeader().GetKeyValueStoreSize() : 0u;
  if (Size() < sizeof(OatHeader) + key_value_store_size) {
    *error_msg = StringPrintf("In oat file '%s' found truncated OatHeader, "
                                  "size = %zu < %zu + %zu",
                              GetLocation().c_str(),
                              Size(),
                              sizeof(OatHeader),
                              key_value_store_size);
    return false;
  }

  size_t oat_dex_files_offset = GetOatHeader().GetOatDexFilesOffset();
  if (oat_dex_files_offset < GetOatHeader().GetHeaderSize() || oat_dex_files_offset > Size()) {
    *error_msg = StringPrintf("In oat file '%s' found invalid oat dex files offset: "
                                  "%zu is not in [%zu, %zu]",
                              GetLocation().c_str(),
                              oat_dex_files_offset,
                              GetOatHeader().GetHeaderSize(),
                              Size());
    return false;
  }
  const uint8_t* oat = Begin() + oat_dex_files_offset;  // Jump to the OatDexFile records.

  DCHECK_GE(static_cast<size_t>(pointer_size), alignof(GcRoot<mirror::Object>));
  if (!IsAligned<kPageSize>(bss_begin_) ||
      !IsAlignedParam(bss_methods_, static_cast<size_t>(pointer_size)) ||
      !IsAlignedParam(bss_roots_, static_cast<size_t>(pointer_size)) ||
      !IsAligned<alignof(GcRoot<mirror::Object>)>(bss_end_)) {
    *error_msg = StringPrintf("In oat file '%s' found unaligned bss symbol(s): "
                                  "begin = %p, methods_ = %p, roots = %p, end = %p",
                              GetLocation().c_str(),
                              bss_begin_,
                              bss_methods_,
                              bss_roots_,
                              bss_end_);
    return false;
  }

  if ((bss_methods_ != nullptr && (bss_methods_ < bss_begin_ || bss_methods_ > bss_end_)) ||
      (bss_roots_ != nullptr && (bss_roots_ < bss_begin_ || bss_roots_ > bss_end_)) ||
      (bss_methods_ != nullptr && bss_roots_ != nullptr && bss_methods_ > bss_roots_)) {
    *error_msg = StringPrintf("In oat file '%s' found bss symbol(s) outside .bss or unordered: "
                                  "begin = %p, methods = %p, roots = %p, end = %p",
                              GetLocation().c_str(),
                              bss_begin_,
                              bss_methods_,
                              bss_roots_,
                              bss_end_);
    return false;
  }

  uint8_t* after_tables =
      (bss_methods_ != nullptr) ? bss_methods_ : bss_roots_;  // May be null.
  uint8_t* boot_image_tables = (bss_begin_ == after_tables) ? nullptr : bss_begin_;
  uint8_t* boot_image_tables_end =
      (bss_begin_ == after_tables) ? nullptr : (after_tables != nullptr) ? after_tables : bss_end_;
  DCHECK_EQ(boot_image_tables != nullptr, boot_image_tables_end != nullptr);
  uint32_t dex_file_count = GetOatHeader().GetDexFileCount();
  oat_dex_files_storage_.reserve(dex_file_count);
  for (size_t i = 0; i < dex_file_count; i++) {
    uint32_t dex_file_location_size;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &dex_file_location_size))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu truncated after dex file "
                                    "location size",
                                GetLocation().c_str(),
                                i);
      return false;
    }
    if (UNLIKELY(dex_file_location_size == 0U)) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu with empty location name",
                                GetLocation().c_str(),
                                i);
      return false;
    }
    if (UNLIKELY(static_cast<size_t>(End() - oat) < dex_file_location_size)) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu with truncated dex file "
                                    "location",
                                GetLocation().c_str(),
                                i);
      return false;
    }
    const char* dex_file_location_data = reinterpret_cast<const char*>(oat);
    oat += dex_file_location_size;

    std::string dex_file_location = ResolveRelativeEncodedDexLocation(
        abs_dex_location,
        std::string(dex_file_location_data, dex_file_location_size));

    uint32_t dex_file_checksum;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &dex_file_checksum))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' truncated after "
                                    "dex file checksum",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str());
      return false;
    }

    uint32_t dex_file_offset;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &dex_file_offset))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' truncated "
                                    "after dex file offsets",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str());
      return false;
    }
    if (UNLIKELY(dex_file_offset > DexSize())) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with dex file "
                                    "offset %u > %zu",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                dex_file_offset,
                                DexSize());
      return false;
    }
    const uint8_t* dex_file_pointer = nullptr;
    if (UNLIKELY(dex_file_offset == 0U)) {
      if (uncompressed_dex_files_ == nullptr) {
        // Do not support mixed-mode oat files.
        if (i > 0) {
          *error_msg = StringPrintf("In oat file '%s', unsupported uncompressed-dex-file for dex "
                                        "file %zu (%s)",
                                    GetLocation().c_str(),
                                    i,
                                    dex_file_location.c_str());
          return false;
        }
        uncompressed_dex_files_.reset(new std::vector<std::unique_ptr<const DexFile>>());
        // No dex files, load it from location.
        const ArtDexFileLoader dex_file_loader;
        bool loaded = false;
        if (zip_fd != -1) {
          loaded = dex_file_loader.OpenZip(zip_fd,
                                           dex_file_location,
                                           /* verify */ false,
                                           /* verify_checksum */ false,
                                           error_msg,
                                           uncompressed_dex_files_.get());
        } else {
          loaded = dex_file_loader.Open(dex_file_location.c_str(),
                                        dex_file_location,
                                        /* verify */ false,
                                        /* verify_checksum */ false,
                                        error_msg,
                                        uncompressed_dex_files_.get());
        }
        if (!loaded) {
          if (Runtime::Current() == nullptr) {
            // If there's no runtime, we're running oatdump, so return
            // a half constructed oat file that oatdump knows how to deal with.
            LOG(WARNING) << "Could not find associated dex files of oat file. "
                         << "Oatdump will only dump the header.";
            return true;
          } else {
            return false;
          }
        }
        // The oat file may be out of date wrt/ the dex-file location. We need to be defensive
        // here and ensure that at least the number of dex files still matches.
        // Note: actual checksum comparisons are the duty of the OatFileAssistant and will be
        //       done after loading the OatFile.
        if (uncompressed_dex_files_->size() != dex_file_count) {
          *error_msg = StringPrintf("In oat file '%s', expected %u uncompressed dex files, but "
                                        "found %zu in '%s'",
                                    GetLocation().c_str(),
                                    dex_file_count,
                                    uncompressed_dex_files_->size(),
                                    dex_file_location.c_str());
          return false;
        }
      }
      dex_file_pointer = uncompressed_dex_files_.get()->at(i)->Begin();
    } else {
      // Do not support mixed-mode oat files.
      if (uncompressed_dex_files_ != nullptr) {
        *error_msg = StringPrintf("In oat file '%s', unsupported embedded dex-file for dex file "
                                      "%zu (%s)",
                                  GetLocation().c_str(),
                                  i,
                                  dex_file_location.c_str());
        return false;
      }
      if (UNLIKELY(DexSize() - dex_file_offset < sizeof(DexFile::Header))) {
        *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with dex file "
                                      "offset %u of %zu but the size of dex file header is %zu",
                                  GetLocation().c_str(),
                                  i,
                                  dex_file_location.c_str(),
                                  dex_file_offset,
                                  DexSize(),
                                  sizeof(DexFile::Header));
        return false;
      }
      dex_file_pointer = DexBegin() + dex_file_offset;
    }

    const bool valid_magic = DexFileLoader::IsMagicValid(dex_file_pointer);
    if (UNLIKELY(!valid_magic)) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with invalid "
                                    "dex file magic '%s'",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                dex_file_pointer);
      return false;
    }
    if (UNLIKELY(!DexFileLoader::IsVersionAndMagicValid(dex_file_pointer))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with invalid "
                                    "dex file version '%s'",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                dex_file_pointer);
      return false;
    }
    const DexFile::Header* header = reinterpret_cast<const DexFile::Header*>(dex_file_pointer);
    if (dex_file_offset != 0 && (DexSize() - dex_file_offset < header->file_size_)) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with dex file "
                                    "offset %u and size %u truncated at %zu",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                dex_file_offset,
                                header->file_size_,
                                DexSize());
      return false;
    }

    uint32_t class_offsets_offset;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &class_offsets_offset))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' truncated "
                                    "after class offsets offset",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str());
      return false;
    }
    if (UNLIKELY(class_offsets_offset > Size()) ||
        UNLIKELY((Size() - class_offsets_offset) / sizeof(uint32_t) < header->class_defs_size_)) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with truncated "
                                    "class offsets, offset %u of %zu, class defs %u",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                class_offsets_offset,
                                Size(),
                                header->class_defs_size_);
      return false;
    }
    if (UNLIKELY(!IsAligned<alignof(uint32_t)>(class_offsets_offset))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with unaligned "
                                    "class offsets, offset %u",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                class_offsets_offset);
      return false;
    }
    const uint32_t* class_offsets_pointer =
        reinterpret_cast<const uint32_t*>(Begin() + class_offsets_offset);

    uint32_t lookup_table_offset;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &lookup_table_offset))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zd for '%s' truncated "
                                    "after lookup table offset",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str());
      return false;
    }
    const uint8_t* lookup_table_data = lookup_table_offset != 0u
        ? Begin() + lookup_table_offset
        : nullptr;
    if (lookup_table_offset != 0u &&
        (UNLIKELY(lookup_table_offset > Size()) ||
            UNLIKELY(Size() - lookup_table_offset <
                     TypeLookupTable::RawDataLength(header->class_defs_size_)))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zu for '%s' with truncated "
                                    "type lookup table, offset %u of %zu, class defs %u",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str(),
                                lookup_table_offset,
                                Size(),
                                header->class_defs_size_);
      return false;
    }

    uint32_t dex_layout_sections_offset;
    if (UNLIKELY(!ReadOatDexFileData(*this, &oat, &dex_layout_sections_offset))) {
      *error_msg = StringPrintf("In oat file '%s' found OatDexFile #%zd for '%s' truncated "
                                    "after dex layout sections offset",
                                GetLocation().c_str(),
                                i,
                                dex_file_location.c_str());
      return false;
    }
    const DexLayoutSections* const dex_layout_sections = dex_layout_sections_offset != 0
        ? reinterpret_cast<const DexLayoutSections*>(Begin() + dex_layout_sections_offset)
        : nullptr;

    const IndexBssMapping* method_bss_mapping;
    const IndexBssMapping* type_bss_mapping;
    const IndexBssMapping* string_bss_mapping;
    if (!ReadIndexBssMapping(
            this, &oat, i, dex_file_location, "method", &method_bss_mapping, error_msg) ||
        !ReadIndexBssMapping(
            this, &oat, i, dex_file_location, "type", &type_bss_mapping, error_msg) ||
        !ReadIndexBssMapping(
            this, &oat, i, dex_file_location, "string", &string_bss_mapping, error_msg)) {
      return false;
    }
    DCheckIndexToBssMapping(
        this, header->method_ids_size_, static_cast<size_t>(pointer_size), method_bss_mapping);
    DCheckIndexToBssMapping(
        this, header->type_ids_size_, sizeof(GcRoot<mirror::Class>), type_bss_mapping);
    DCheckIndexToBssMapping(
        this, header->string_ids_size_, sizeof(GcRoot<mirror::String>), string_bss_mapping);

    std::string canonical_location =
        DexFileLoader::GetDexCanonicalLocation(dex_file_location.c_str());

    // Create the OatDexFile and add it to the owning container.
    OatDexFile* oat_dex_file = new OatDexFile(this,
                                              dex_file_location,
                                              canonical_location,
                                              dex_file_checksum,
                                              dex_file_pointer,
                                              lookup_table_data,
                                              method_bss_mapping,
                                              type_bss_mapping,
                                              string_bss_mapping,
                                              class_offsets_pointer,
                                              dex_layout_sections);
    oat_dex_files_storage_.push_back(oat_dex_file);

    // Add the location and canonical location (if different) to the oat_dex_files_ table.
    StringPiece key(oat_dex_file->GetDexFileLocation());
    oat_dex_files_.Put(key, oat_dex_file);
    if (canonical_location != dex_file_location) {
      StringPiece canonical_key(oat_dex_file->GetCanonicalDexFileLocation());
      oat_dex_files_.Put(canonical_key, oat_dex_file);
    }
  }

  if (boot_image_tables != nullptr) {
    Runtime* runtime = Runtime::Current();
    if (UNLIKELY(runtime == nullptr)) {
      // This must be oatdump without boot image. Make sure the .bss is inaccessible.
      CheckedCall(mprotect, "protect bss", const_cast<uint8_t*>(BssBegin()), BssSize(), PROT_NONE);
    } else if (!IsExecutable()) {
      // Do not try to mmap boot image tables into .bss if the oat file is not executable.
    } else {
      // Map boot image tables into the .bss. The reserved size must match size of the tables.
      size_t reserved_size = static_cast<size_t>(boot_image_tables_end - boot_image_tables);
      size_t tables_size = 0u;
      for (gc::space::ImageSpace* space : runtime->GetHeap()->GetBootImageSpaces()) {
        tables_size += space->GetImageHeader().GetBootImageConstantTablesSize();
        DCHECK_ALIGNED(tables_size, kPageSize);
      }
      if (tables_size != reserved_size) {
        *error_msg = StringPrintf("In oat file '%s' found unexpected boot image table sizes, "
                                      " %zu bytes, should be %zu.",
                                  GetLocation().c_str(),
                                  reserved_size,
                                  tables_size);
        return false;
      }
      for (gc::space::ImageSpace* space : Runtime::Current()->GetHeap()->GetBootImageSpaces()) {
        uint32_t current_tables_size = space->GetImageHeader().GetBootImageConstantTablesSize();
        if (current_tables_size != 0u && !MapConstantTables(space, boot_image_tables)) {
          return false;
        }
        boot_image_tables += current_tables_size;
      }
      DCHECK(boot_image_tables == boot_image_tables_end);
    }
  }
  return true;
}

////////////////////////
// OatFile via dlopen //
////////////////////////

class DlOpenOatFile FINAL : public OatFileBase {
 public:
  DlOpenOatFile(const std::string& filename, bool executable)
      : OatFileBase(filename, executable),
        dlopen_handle_(nullptr),
        shared_objects_before_(0) {
  }

  ~DlOpenOatFile() {
    if (dlopen_handle_ != nullptr) {
      if (!kIsTargetBuild) {
        MutexLock mu(Thread::Current(), *Locks::host_dlopen_handles_lock_);
        host_dlopen_handles_.erase(dlopen_handle_);
        dlclose(dlopen_handle_);
      } else {
        dlclose(dlopen_handle_);
      }
    }
  }

 protected:
  const uint8_t* FindDynamicSymbolAddress(const std::string& symbol_name,
                                          std::string* error_msg) const OVERRIDE {
    const uint8_t* ptr =
        reinterpret_cast<const uint8_t*>(dlsym(dlopen_handle_, symbol_name.c_str()));
    if (ptr == nullptr) {
      *error_msg = dlerror();
    }
    return ptr;
  }

  void PreLoad() OVERRIDE;

  bool Load(const std::string& elf_filename,
            uint8_t* oat_file_begin,
            bool writable,
            bool executable,
            bool low_4gb,
            std::string* error_msg) OVERRIDE;

  bool Load(int, uint8_t*, bool, bool, bool, std::string*) {
    return false;
  }

  // Ask the linker where it mmaped the file and notify our mmap wrapper of the regions.
  void PreSetup(const std::string& elf_filename) OVERRIDE;

 private:
  bool Dlopen(const std::string& elf_filename,
              uint8_t* oat_file_begin,
              std::string* error_msg);

  // On the host, if the same library is loaded again with dlopen the same
  // file handle is returned. This differs from the behavior of dlopen on the
  // target, where dlopen reloads the library at a different address every
  // time you load it. The runtime relies on the target behavior to ensure
  // each instance of the loaded library has a unique dex cache. To avoid
  // problems, we fall back to our own linker in the case when the same
  // library is opened multiple times on host. dlopen_handles_ is used to
  // detect that case.
  // Guarded by host_dlopen_handles_lock_;
  static std::unordered_set<void*> host_dlopen_handles_;

  // dlopen handle during runtime.
  void* dlopen_handle_;  // TODO: Unique_ptr with custom deleter.

  // Dummy memory map objects corresponding to the regions mapped by dlopen.
  std::vector<std::unique_ptr<MemMap>> dlopen_mmaps_;

  // The number of shared objects the linker told us about before loading. Used to
  // (optimistically) optimize the PreSetup stage (see comment there).
  size_t shared_objects_before_;

  DISALLOW_COPY_AND_ASSIGN(DlOpenOatFile);
};

std::unordered_set<void*> DlOpenOatFile::host_dlopen_handles_;

void DlOpenOatFile::PreLoad() {
#ifdef __APPLE__
  UNUSED(shared_objects_before_);
  LOG(FATAL) << "Should not reach here.";
  UNREACHABLE();
#else
  // Count the entries in dl_iterate_phdr we get at this point in time.
  struct dl_iterate_context {
    static int callback(struct dl_phdr_info *info ATTRIBUTE_UNUSED,
                        size_t size ATTRIBUTE_UNUSED,
                        void *data) {
      reinterpret_cast<dl_iterate_context*>(data)->count++;
      return 0;  // Continue iteration.
    }
    size_t count = 0;
  } context;

  dl_iterate_phdr(dl_iterate_context::callback, &context);
  shared_objects_before_ = context.count;
#endif
}

bool DlOpenOatFile::Load(const std::string& elf_filename,
                         uint8_t* oat_file_begin,
                         bool writable,
                         bool executable,
                         bool low_4gb,
                         std::string* error_msg) {
  // Use dlopen only when flagged to do so, and when it's OK to load things executable.
  // TODO: Also try when not executable? The issue here could be re-mapping as writable (as
  //       !executable is a sign that we may want to patch), which may not be allowed for
  //       various reasons.
  if (!kUseDlopen) {
    *error_msg = "DlOpen is disabled.";
    return false;
  }
  if (low_4gb) {
    *error_msg = "DlOpen does not support low 4gb loading.";
    return false;
  }
  if (writable) {
    *error_msg = "DlOpen does not support writable loading.";
    return false;
  }
  if (!executable) {
    *error_msg = "DlOpen does not support non-executable loading.";
    return false;
  }

  // dlopen always returns the same library if it is already opened on the host. For this reason
  // we only use dlopen if we are the target or we do not already have the dex file opened. Having
  // the same library loaded multiple times at different addresses is required for class unloading
  // and for having dex caches arrays in the .bss section.
  if (!kIsTargetBuild) {
    if (!kUseDlopenOnHost) {
      *error_msg = "DlOpen disabled for host.";
      return false;
    }
  }

  bool success = Dlopen(elf_filename, oat_file_begin, error_msg);
  DCHECK(dlopen_handle_ != nullptr || !success);

  return success;
}

bool DlOpenOatFile::Dlopen(const std::string& elf_filename,
                           uint8_t* oat_file_begin,
                           std::string* error_msg) {
#ifdef __APPLE__
  // The dl_iterate_phdr syscall is missing.  There is similar API on OSX,
  // but let's fallback to the custom loading code for the time being.
  UNUSED(elf_filename, oat_file_begin);
  *error_msg = "Dlopen unsupported on Mac.";
  return false;
#else
  {
    UniqueCPtr<char> absolute_path(realpath(elf_filename.c_str(), nullptr));
    if (absolute_path == nullptr) {
      *error_msg = StringPrintf("Failed to find absolute path for '%s'", elf_filename.c_str());
      return false;
    }
#ifdef ART_TARGET_ANDROID
    android_dlextinfo extinfo = {};
    extinfo.flags = ANDROID_DLEXT_FORCE_LOAD |                  // Force-load, don't reuse handle
                                                                //   (open oat files multiple
                                                                //    times).
                    ANDROID_DLEXT_FORCE_FIXED_VADDR;            // Take a non-zero vaddr as absolute
                                                                //   (non-pic boot image).
    if (oat_file_begin != nullptr) {                            //
      extinfo.flags |= ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS;     // Use the requested addr if
      extinfo.reserved_addr = oat_file_begin;                   // vaddr = 0.
    }                                                           //   (pic boot image).
    dlopen_handle_ = android_dlopen_ext(absolute_path.get(), RTLD_NOW, &extinfo);
#else
    UNUSED(oat_file_begin);
    static_assert(!kIsTargetBuild || kIsTargetLinux, "host_dlopen_handles_ will leak handles");
    MutexLock mu(Thread::Current(), *Locks::host_dlopen_handles_lock_);
    dlopen_handle_ = dlopen(absolute_path.get(), RTLD_NOW);
    if (dlopen_handle_ != nullptr) {
      if (!host_dlopen_handles_.insert(dlopen_handle_).second) {
        dlclose(dlopen_handle_);
        dlopen_handle_ = nullptr;
        *error_msg = StringPrintf("host dlopen re-opened '%s'", elf_filename.c_str());
        return false;
      }
    }
#endif  // ART_TARGET_ANDROID
  }
  if (dlopen_handle_ == nullptr) {
    *error_msg = StringPrintf("Failed to dlopen '%s': %s", elf_filename.c_str(), dlerror());
    return false;
  }
  return true;
#endif
}

void DlOpenOatFile::PreSetup(const std::string& elf_filename) {
#ifdef __APPLE__
  UNUSED(elf_filename);
  LOG(FATAL) << "Should not reach here.";
  UNREACHABLE();
#else
  struct dl_iterate_context {
    static int callback(struct dl_phdr_info *info, size_t /* size */, void *data) {
      auto* context = reinterpret_cast<dl_iterate_context*>(data);
      context->shared_objects_seen++;
      if (context->shared_objects_seen < context->shared_objects_before) {
        // We haven't been called yet for anything we haven't seen before. Just continue.
        // Note: this is aggressively optimistic. If another thread was unloading a library,
        //       we may miss out here. However, this does not happen often in practice.
        return 0;
      }

      // See whether this callback corresponds to the file which we have just loaded.
      bool contains_begin = false;
      for (int i = 0; i < info->dlpi_phnum; i++) {
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
          uint8_t* vaddr = reinterpret_cast<uint8_t*>(info->dlpi_addr +
              info->dlpi_phdr[i].p_vaddr);
          size_t memsz = info->dlpi_phdr[i].p_memsz;
          if (vaddr <= context->begin_ && context->begin_ < vaddr + memsz) {
            contains_begin = true;
            break;
          }
        }
      }
      // Add dummy mmaps for this file.
      if (contains_begin) {
        for (int i = 0; i < info->dlpi_phnum; i++) {
          if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            uint8_t* vaddr = reinterpret_cast<uint8_t*>(info->dlpi_addr +
                info->dlpi_phdr[i].p_vaddr);
            size_t memsz = info->dlpi_phdr[i].p_memsz;
            MemMap* mmap = MemMap::MapDummy(info->dlpi_name, vaddr, memsz);
            context->dlopen_mmaps_->push_back(std::unique_ptr<MemMap>(mmap));
          }
        }
        return 1;  // Stop iteration and return 1 from dl_iterate_phdr.
      }
      return 0;  // Continue iteration and return 0 from dl_iterate_phdr when finished.
    }
    const uint8_t* const begin_;
    std::vector<std::unique_ptr<MemMap>>* const dlopen_mmaps_;
    const size_t shared_objects_before;
    size_t shared_objects_seen;
  };
  dl_iterate_context context = { Begin(), &dlopen_mmaps_, shared_objects_before_, 0};

  if (dl_iterate_phdr(dl_iterate_context::callback, &context) == 0) {
    // Hm. Maybe our optimization went wrong. Try another time with shared_objects_before == 0
    // before giving up. This should be unusual.
    VLOG(oat) << "Need a second run in PreSetup, didn't find with shared_objects_before="
              << shared_objects_before_;
    dl_iterate_context context0 = { Begin(), &dlopen_mmaps_, 0, 0};
    if (dl_iterate_phdr(dl_iterate_context::callback, &context0) == 0) {
      // OK, give up and print an error.
      PrintFileToLog("/proc/self/maps", android::base::LogSeverity::WARNING);
      LOG(ERROR) << "File " << elf_filename << " loaded with dlopen but cannot find its mmaps.";
    }
  }
#endif
}

////////////////////////////////////////////////
// OatFile via our own ElfFile implementation //
////////////////////////////////////////////////

class ElfOatFile FINAL : public OatFileBase {
 public:
  ElfOatFile(const std::string& filename, bool executable) : OatFileBase(filename, executable) {}

  static ElfOatFile* OpenElfFile(int zip_fd,
                                 File* file,
                                 const std::string& location,
                                 uint8_t* requested_base,
                                 uint8_t* oat_file_begin,  // Override base if not null
                                 bool writable,
                                 bool executable,
                                 bool low_4gb,
                                 const char* abs_dex_location,
                                 std::string* error_msg);

  bool InitializeFromElfFile(int zip_fd,
                             ElfFile* elf_file,
                             VdexFile* vdex_file,
                             const char* abs_dex_location,
                             std::string* error_msg);

 protected:
  const uint8_t* FindDynamicSymbolAddress(const std::string& symbol_name,
                                          std::string* error_msg) const OVERRIDE {
    const uint8_t* ptr = elf_file_->FindDynamicSymbolAddress(symbol_name);
    if (ptr == nullptr) {
      *error_msg = "(Internal implementation could not find symbol)";
    }
    return ptr;
  }

  void PreLoad() OVERRIDE {
  }

  bool Load(const std::string& elf_filename,
            uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
            bool writable,
            bool executable,
            bool low_4gb,
            std::string* error_msg) OVERRIDE;

  bool Load(int oat_fd,
            uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
            bool writable,
            bool executable,
            bool low_4gb,
            std::string* error_msg) OVERRIDE;

  void PreSetup(const std::string& elf_filename ATTRIBUTE_UNUSED) OVERRIDE {
  }

 private:
  bool ElfFileOpen(File* file,
                   uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
                   bool writable,
                   bool executable,
                   bool low_4gb,
                   std::string* error_msg);

 private:
  // Backing memory map for oat file during cross compilation.
  std::unique_ptr<ElfFile> elf_file_;

  DISALLOW_COPY_AND_ASSIGN(ElfOatFile);
};

ElfOatFile* ElfOatFile::OpenElfFile(int zip_fd,
                                    File* file,
                                    const std::string& location,
                                    uint8_t* requested_base,
                                    uint8_t* oat_file_begin,  // Override base if not null
                                    bool writable,
                                    bool executable,
                                    bool low_4gb,
                                    const char* abs_dex_location,
                                    std::string* error_msg) {
  ScopedTrace trace("Open elf file " + location);
  std::unique_ptr<ElfOatFile> oat_file(new ElfOatFile(location, executable));
  bool success = oat_file->ElfFileOpen(file,
                                       oat_file_begin,
                                       writable,
                                       low_4gb,
                                       executable,
                                       error_msg);
  if (!success) {
    CHECK(!error_msg->empty());
    return nullptr;
  }

  // Complete the setup.
  if (!oat_file->ComputeFields(requested_base, file->GetPath(), error_msg)) {
    return nullptr;
  }

  if (!oat_file->Setup(zip_fd, abs_dex_location, error_msg)) {
    return nullptr;
  }

  return oat_file.release();
}

bool ElfOatFile::InitializeFromElfFile(int zip_fd,
                                       ElfFile* elf_file,
                                       VdexFile* vdex_file,
                                       const char* abs_dex_location,
                                       std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  if (IsExecutable()) {
    *error_msg = "Cannot initialize from elf file in executable mode.";
    return false;
  }
  elf_file_.reset(elf_file);
  SetVdex(vdex_file);
  uint64_t offset, size;
  bool has_section = elf_file->GetSectionOffsetAndSize(".rodata", &offset, &size);
  CHECK(has_section);
  SetBegin(elf_file->Begin() + offset);
  SetEnd(elf_file->Begin() + size + offset);
  // Ignore the optional .bss section when opening non-executable.
  return Setup(zip_fd, abs_dex_location, error_msg);
}

bool ElfOatFile::Load(const std::string& elf_filename,
                      uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
                      bool writable,
                      bool executable,
                      bool low_4gb,
                      std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  std::unique_ptr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  if (file == nullptr) {
    *error_msg = StringPrintf("Failed to open oat filename for reading: %s", strerror(errno));
    return false;
  }
  return ElfOatFile::ElfFileOpen(file.get(),
                                 oat_file_begin,
                                 writable,
                                 executable,
                                 low_4gb,
                                 error_msg);
}

bool ElfOatFile::Load(int oat_fd,
                      uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
                      bool writable,
                      bool executable,
                      bool low_4gb,
                      std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  if (oat_fd != -1) {
    std::unique_ptr<File> file = std::make_unique<File>(oat_fd, false);
    file->DisableAutoClose();
    if (file == nullptr) {
      *error_msg = StringPrintf("Failed to open oat filename for reading: %s",
                                strerror(errno));
      return false;
    }
    return ElfOatFile::ElfFileOpen(file.get(),
                                   oat_file_begin,
                                   writable,
                                   executable,
                                   low_4gb,
                                   error_msg);
  }
  return false;
}

bool ElfOatFile::ElfFileOpen(File* file,
                             uint8_t* oat_file_begin,
                             bool writable,
                             bool executable,
                             bool low_4gb,
                             std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  // TODO: rename requested_base to oat_data_begin
  elf_file_.reset(ElfFile::Open(file,
                                writable,
                                /*program_header_only*/true,
                                low_4gb,
                                error_msg,
                                oat_file_begin));
  if (elf_file_ == nullptr) {
    DCHECK(!error_msg->empty());
    return false;
  }
  bool loaded = elf_file_->Load(file, executable, low_4gb, error_msg);
  DCHECK(loaded || !error_msg->empty());
  return loaded;
}

//////////////////////////
// General OatFile code //
//////////////////////////

std::string OatFile::ResolveRelativeEncodedDexLocation(
      const char* abs_dex_location, const std::string& rel_dex_location) {
  // For host, we still do resolution as the rel_dex_location might be absolute
  // for a target dex (for example /system/foo/foo.apk).
  if (abs_dex_location != nullptr && (rel_dex_location[0] != '/' || !kIsTargetBuild)) {
    // Strip :classes<N>.dex used for secondary multidex files.
    std::string base = DexFileLoader::GetBaseLocation(rel_dex_location);
    std::string multidex_suffix = DexFileLoader::GetMultiDexSuffix(rel_dex_location);

    // Check if the base is a suffix of the provided abs_dex_location.
    std::string target_suffix = ((rel_dex_location[0] != '/') ? "/" : "") + base;
    std::string abs_location(abs_dex_location);
    if (abs_location.size() > target_suffix.size()) {
      size_t pos = abs_location.size() - target_suffix.size();
      if (abs_location.compare(pos, std::string::npos, target_suffix) == 0) {
        return abs_location + multidex_suffix;
      }
    }
  }
  return rel_dex_location;
}

static void CheckLocation(const std::string& location) {
  CHECK(!location.empty());
}

OatFile* OatFile::OpenWithElfFile(int zip_fd,
                                  ElfFile* elf_file,
                                  VdexFile* vdex_file,
                                  const std::string& location,
                                  const char* abs_dex_location,
                                  std::string* error_msg) {
  std::unique_ptr<ElfOatFile> oat_file(new ElfOatFile(location, false /* executable */));
  return oat_file->InitializeFromElfFile(zip_fd, elf_file, vdex_file, abs_dex_location, error_msg)
      ? oat_file.release()
      : nullptr;
}

OatFile* OatFile::Open(int zip_fd,
                       const std::string& oat_filename,
                       const std::string& oat_location,
                       uint8_t* requested_base,
                       uint8_t* oat_file_begin,
                       bool executable,
                       bool low_4gb,
                       const char* abs_dex_location,
                       std::string* error_msg) {
  ScopedTrace trace("Open oat file " + oat_location);
  CHECK(!oat_filename.empty()) << oat_location;
  CheckLocation(oat_location);

  std::string vdex_filename = GetVdexFilename(oat_filename);

  // Check that the files even exist, fast-fail.
  if (!OS::FileExists(vdex_filename.c_str())) {
    *error_msg = StringPrintf("File %s does not exist.", vdex_filename.c_str());
    return nullptr;
  } else if (!OS::FileExists(oat_filename.c_str())) {
    *error_msg = StringPrintf("File %s does not exist.", oat_filename.c_str());
    return nullptr;
  }

  // Try dlopen first, as it is required for native debuggability. This will fail fast if dlopen is
  // disabled.
  OatFile* with_dlopen = OatFileBase::OpenOatFile<DlOpenOatFile>(zip_fd,
                                                                 vdex_filename,
                                                                 oat_filename,
                                                                 oat_location,
                                                                 requested_base,
                                                                 oat_file_begin,
                                                                 false /* writable */,
                                                                 executable,
                                                                 low_4gb,
                                                                 abs_dex_location,
                                                                 error_msg);
  if (with_dlopen != nullptr) {
    return with_dlopen;
  }
  if (kPrintDlOpenErrorMessage) {
    LOG(ERROR) << "Failed to dlopen: " << oat_filename << " with error " << *error_msg;
  }
  // If we aren't trying to execute, we just use our own ElfFile loader for a couple reasons:
  //
  // On target, dlopen may fail when compiling due to selinux restrictions on installd.
  //
  // We use our own ELF loader for Quick to deal with legacy apps that
  // open a generated dex file by name, remove the file, then open
  // another generated dex file with the same name. http://b/10614658
  //
  // On host, dlopen is expected to fail when cross compiling, so fall back to OpenElfFile.
  //
  //
  // Another independent reason is the absolute placement of boot.oat. dlopen on the host usually
  // does honor the virtual address encoded in the ELF file only for ET_EXEC files, not ET_DYN.
  OatFile* with_internal = OatFileBase::OpenOatFile<ElfOatFile>(zip_fd,
                                                                vdex_filename,
                                                                oat_filename,
                                                                oat_location,
                                                                requested_base,
                                                                oat_file_begin,
                                                                false /* writable */,
                                                                executable,
                                                                low_4gb,
                                                                abs_dex_location,
                                                                error_msg);
  return with_internal;
}

OatFile* OatFile::Open(int zip_fd,
                       int vdex_fd,
                       int oat_fd,
                       const std::string& oat_location,
                       uint8_t* requested_base,
                       uint8_t* oat_file_begin,
                       bool executable,
                       bool low_4gb,
                       const char* abs_dex_location,
                       std::string* error_msg) {
  CHECK(!oat_location.empty()) << oat_location;

  std::string vdex_location = GetVdexFilename(oat_location);

  OatFile* with_internal = OatFileBase::OpenOatFile<ElfOatFile>(zip_fd,
                                                                vdex_fd,
                                                                oat_fd,
                                                                vdex_location,
                                                                oat_location,
                                                                requested_base,
                                                                oat_file_begin,
                                                                false /* writable */,
                                                                executable,
                                                                low_4gb,
                                                                abs_dex_location,
                                                                error_msg);
  return with_internal;
}

OatFile* OatFile::OpenWritable(int zip_fd,
                               File* file,
                               const std::string& location,
                               const char* abs_dex_location,
                               std::string* error_msg) {
  CheckLocation(location);
  return ElfOatFile::OpenElfFile(zip_fd,
                                 file,
                                 location,
                                 nullptr,
                                 nullptr,
                                 true,
                                 false,
                                 /*low_4gb*/false,
                                 abs_dex_location,
                                 error_msg);
}

OatFile* OatFile::OpenReadable(int zip_fd,
                               File* file,
                               const std::string& location,
                               const char* abs_dex_location,
                               std::string* error_msg) {
  CheckLocation(location);
  return ElfOatFile::OpenElfFile(zip_fd,
                                 file,
                                 location,
                                 nullptr,
                                 nullptr,
                                 false,
                                 false,
                                 /*low_4gb*/false,
                                 abs_dex_location,
                                 error_msg);
}

OatFile::OatFile(const std::string& location, bool is_executable)
    : location_(location),
      vdex_(nullptr),
      begin_(nullptr),
      end_(nullptr),
      bss_begin_(nullptr),
      bss_end_(nullptr),
      bss_methods_(nullptr),
      bss_roots_(nullptr),
      is_executable_(is_executable),
      vdex_begin_(nullptr),
      vdex_end_(nullptr),
      secondary_lookup_lock_("OatFile secondary lookup lock", kOatFileSecondaryLookupLock) {
  CHECK(!location_.empty());
}

OatFile::~OatFile() {
  STLDeleteElements(&oat_dex_files_storage_);
}

const OatHeader& OatFile::GetOatHeader() const {
  return *reinterpret_cast<const OatHeader*>(Begin());
}

const uint8_t* OatFile::Begin() const {
  CHECK(begin_ != nullptr);
  return begin_;
}

const uint8_t* OatFile::End() const {
  CHECK(end_ != nullptr);
  return end_;
}

const uint8_t* OatFile::BssBegin() const {
  return bss_begin_;
}

const uint8_t* OatFile::BssEnd() const {
  return bss_end_;
}

const uint8_t* OatFile::VdexBegin() const {
  return vdex_begin_;
}

const uint8_t* OatFile::VdexEnd() const {
  return vdex_end_;
}

const uint8_t* OatFile::DexBegin() const {
  return vdex_->Begin();
}

const uint8_t* OatFile::DexEnd() const {
  return vdex_->End();
}

ArrayRef<ArtMethod*> OatFile::GetBssMethods() const {
  if (bss_methods_ != nullptr) {
    ArtMethod** methods = reinterpret_cast<ArtMethod**>(bss_methods_);
    ArtMethod** methods_end =
        reinterpret_cast<ArtMethod**>(bss_roots_ != nullptr ? bss_roots_ : bss_end_);
    return ArrayRef<ArtMethod*>(methods, methods_end - methods);
  } else {
    return ArrayRef<ArtMethod*>();
  }
}

ArrayRef<GcRoot<mirror::Object>> OatFile::GetBssGcRoots() const {
  if (bss_roots_ != nullptr) {
    auto* roots = reinterpret_cast<GcRoot<mirror::Object>*>(bss_roots_);
    auto* roots_end = reinterpret_cast<GcRoot<mirror::Object>*>(bss_end_);
    return ArrayRef<GcRoot<mirror::Object>>(roots, roots_end - roots);
  } else {
    return ArrayRef<GcRoot<mirror::Object>>();
  }
}

const OatFile::OatDexFile* OatFile::GetOatDexFile(const char* dex_location,
                                                  const uint32_t* dex_location_checksum,
                                                  std::string* error_msg) const {
  // NOTE: We assume here that the canonical location for a given dex_location never
  // changes. If it does (i.e. some symlink used by the filename changes) we may return
  // an incorrect OatDexFile. As long as we have a checksum to check, we shall return
  // an identical file or fail; otherwise we may see some unpredictable failures.

  // TODO: Additional analysis of usage patterns to see if this can be simplified
  // without any performance loss, for example by not doing the first lock-free lookup.

  const OatFile::OatDexFile* oat_dex_file = nullptr;
  StringPiece key(dex_location);
  // Try to find the key cheaply in the oat_dex_files_ map which holds dex locations
  // directly mentioned in the oat file and doesn't require locking.
  auto primary_it = oat_dex_files_.find(key);
  if (primary_it != oat_dex_files_.end()) {
    oat_dex_file = primary_it->second;
    DCHECK(oat_dex_file != nullptr);
  } else {
    // This dex_location is not one of the dex locations directly mentioned in the
    // oat file. The correct lookup is via the canonical location but first see in
    // the secondary_oat_dex_files_ whether we've looked up this location before.
    MutexLock mu(Thread::Current(), secondary_lookup_lock_);
    auto secondary_lb = secondary_oat_dex_files_.lower_bound(key);
    if (secondary_lb != secondary_oat_dex_files_.end() && key == secondary_lb->first) {
      oat_dex_file = secondary_lb->second;  // May be null.
    } else {
      // We haven't seen this dex_location before, we must check the canonical location.
      std::string dex_canonical_location = DexFileLoader::GetDexCanonicalLocation(dex_location);
      if (dex_canonical_location != dex_location) {
        StringPiece canonical_key(dex_canonical_location);
        auto canonical_it = oat_dex_files_.find(canonical_key);
        if (canonical_it != oat_dex_files_.end()) {
          oat_dex_file = canonical_it->second;
        }  // else keep null.
      }  // else keep null.

      // Copy the key to the string_cache_ and store the result in secondary map.
      string_cache_.emplace_back(key.data(), key.length());
      StringPiece key_copy(string_cache_.back());
      secondary_oat_dex_files_.PutBefore(secondary_lb, key_copy, oat_dex_file);
    }
  }

  if (oat_dex_file == nullptr) {
    if (error_msg != nullptr) {
      std::string dex_canonical_location = DexFileLoader::GetDexCanonicalLocation(dex_location);
      *error_msg = "Failed to find OatDexFile for DexFile " + std::string(dex_location)
          + " (canonical path " + dex_canonical_location + ") in OatFile " + GetLocation();
    }
    return nullptr;
  }

  if (dex_location_checksum != nullptr &&
      oat_dex_file->GetDexFileLocationChecksum() != *dex_location_checksum) {
    if (error_msg != nullptr) {
      std::string dex_canonical_location = DexFileLoader::GetDexCanonicalLocation(dex_location);
      std::string checksum = StringPrintf("0x%08x", oat_dex_file->GetDexFileLocationChecksum());
      std::string required_checksum = StringPrintf("0x%08x", *dex_location_checksum);
      *error_msg = "OatDexFile for DexFile " + std::string(dex_location)
          + " (canonical path " + dex_canonical_location + ") in OatFile " + GetLocation()
          + " has checksum " + checksum + " but " + required_checksum + " was required";
    }
    return nullptr;
  }
  return oat_dex_file;
}

OatFile::OatDexFile::OatDexFile(const OatFile* oat_file,
                                const std::string& dex_file_location,
                                const std::string& canonical_dex_file_location,
                                uint32_t dex_file_location_checksum,
                                const uint8_t* dex_file_pointer,
                                const uint8_t* lookup_table_data,
                                const IndexBssMapping* method_bss_mapping_data,
                                const IndexBssMapping* type_bss_mapping_data,
                                const IndexBssMapping* string_bss_mapping_data,
                                const uint32_t* oat_class_offsets_pointer,
                                const DexLayoutSections* dex_layout_sections)
    : oat_file_(oat_file),
      dex_file_location_(dex_file_location),
      canonical_dex_file_location_(canonical_dex_file_location),
      dex_file_location_checksum_(dex_file_location_checksum),
      dex_file_pointer_(dex_file_pointer),
      lookup_table_data_(lookup_table_data),
      method_bss_mapping_(method_bss_mapping_data),
      type_bss_mapping_(type_bss_mapping_data),
      string_bss_mapping_(string_bss_mapping_data),
      oat_class_offsets_pointer_(oat_class_offsets_pointer),
      dex_layout_sections_(dex_layout_sections) {
  // Initialize TypeLookupTable.
  if (lookup_table_data_ != nullptr) {
    // Peek the number of classes from the DexFile.
    const DexFile::Header* dex_header = reinterpret_cast<const DexFile::Header*>(dex_file_pointer_);
    const uint32_t num_class_defs = dex_header->class_defs_size_;
    if (lookup_table_data_ + TypeLookupTable::RawDataLength(num_class_defs) > GetOatFile()->End()) {
      LOG(WARNING) << "found truncated lookup table in " << dex_file_location_;
    } else {
      const uint8_t* dex_data = dex_file_pointer_;
      // TODO: Clean this up to create the type lookup table after the dex file has been created?
      if (CompactDexFile::IsMagicValid(dex_header->magic_)) {
        dex_data += dex_header->data_off_;
      }
      lookup_table_ = TypeLookupTable::Open(dex_data, lookup_table_data_, num_class_defs);
    }
  }
}

OatFile::OatDexFile::OatDexFile(std::unique_ptr<TypeLookupTable>&& lookup_table)
    : lookup_table_(std::move(lookup_table)) {}

OatFile::OatDexFile::~OatDexFile() {}

size_t OatFile::OatDexFile::FileSize() const {
  return reinterpret_cast<const DexFile::Header*>(dex_file_pointer_)->file_size_;
}

std::unique_ptr<const DexFile> OatFile::OatDexFile::OpenDexFile(std::string* error_msg) const {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  static constexpr bool kVerify = false;
  static constexpr bool kVerifyChecksum = false;
  const ArtDexFileLoader dex_file_loader;
  return dex_file_loader.Open(dex_file_pointer_,
                              FileSize(),
                              dex_file_location_,
                              dex_file_location_checksum_,
                              this,
                              kVerify,
                              kVerifyChecksum,
                              error_msg);
}

uint32_t OatFile::OatDexFile::GetOatClassOffset(uint16_t class_def_index) const {
  return oat_class_offsets_pointer_[class_def_index];
}

OatFile::OatClass OatFile::OatDexFile::GetOatClass(uint16_t class_def_index) const {
  uint32_t oat_class_offset = GetOatClassOffset(class_def_index);

  const uint8_t* oat_class_pointer = oat_file_->Begin() + oat_class_offset;
  CHECK_LT(oat_class_pointer, oat_file_->End()) << oat_file_->GetLocation();

  const uint8_t* status_pointer = oat_class_pointer;
  CHECK_LT(status_pointer, oat_file_->End()) << oat_file_->GetLocation();
  ClassStatus status = enum_cast<ClassStatus>(*reinterpret_cast<const int16_t*>(status_pointer));
  CHECK_LE(status, ClassStatus::kLast);

  const uint8_t* type_pointer = status_pointer + sizeof(uint16_t);
  CHECK_LT(type_pointer, oat_file_->End()) << oat_file_->GetLocation();
  OatClassType type = static_cast<OatClassType>(*reinterpret_cast<const uint16_t*>(type_pointer));
  CHECK_LT(type, kOatClassMax);

  const uint8_t* after_type_pointer = type_pointer + sizeof(int16_t);
  CHECK_LE(after_type_pointer, oat_file_->End()) << oat_file_->GetLocation();

  uint32_t bitmap_size = 0;
  const uint8_t* bitmap_pointer = nullptr;
  const uint8_t* methods_pointer = nullptr;
  if (type != kOatClassNoneCompiled) {
    if (type == kOatClassSomeCompiled) {
      bitmap_size = static_cast<uint32_t>(*reinterpret_cast<const uint32_t*>(after_type_pointer));
      bitmap_pointer = after_type_pointer + sizeof(bitmap_size);
      CHECK_LE(bitmap_pointer, oat_file_->End()) << oat_file_->GetLocation();
      methods_pointer = bitmap_pointer + bitmap_size;
    } else {
      methods_pointer = after_type_pointer;
    }
    CHECK_LE(methods_pointer, oat_file_->End()) << oat_file_->GetLocation();
  }

  return OatFile::OatClass(oat_file_,
                           status,
                           type,
                           bitmap_size,
                           reinterpret_cast<const uint32_t*>(bitmap_pointer),
                           reinterpret_cast<const OatMethodOffsets*>(methods_pointer));
}

const DexFile::ClassDef* OatFile::OatDexFile::FindClassDef(const DexFile& dex_file,
                                                           const char* descriptor,
                                                           size_t hash) {
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  DCHECK_EQ(ComputeModifiedUtf8Hash(descriptor), hash);
  bool used_lookup_table = false;
  const DexFile::ClassDef* lookup_table_classdef = nullptr;
  if (LIKELY((oat_dex_file != nullptr) && (oat_dex_file->GetTypeLookupTable() != nullptr))) {
    used_lookup_table = true;
    const uint32_t class_def_idx = oat_dex_file->GetTypeLookupTable()->Lookup(descriptor, hash);
    lookup_table_classdef = (class_def_idx != dex::kDexNoIndex)
        ? &dex_file.GetClassDef(class_def_idx)
        : nullptr;
    if (!kIsDebugBuild) {
      return lookup_table_classdef;
    }
  }
  // Fast path for rare no class defs case.
  const uint32_t num_class_defs = dex_file.NumClassDefs();
  if (num_class_defs == 0) {
    return nullptr;
  }
  const DexFile::TypeId* type_id = dex_file.FindTypeId(descriptor);
  if (type_id != nullptr) {
    dex::TypeIndex type_idx = dex_file.GetIndexForTypeId(*type_id);
    const DexFile::ClassDef* found_class_def = dex_file.FindClassDef(type_idx);
    if (kIsDebugBuild && used_lookup_table) {
      DCHECK_EQ(found_class_def, lookup_table_classdef);
    }
    return found_class_def;
  }
  return nullptr;
}

// Madvise the dex file based on the state we are moving to.
void OatDexFile::MadviseDexFile(const DexFile& dex_file, MadviseState state) {
  Runtime* const runtime = Runtime::Current();
  const bool low_ram = runtime->GetHeap()->IsLowMemoryMode();
  // TODO: Also do madvise hints for non low ram devices.
  if (!low_ram) {
    return;
  }
  if (state == MadviseState::kMadviseStateAtLoad && runtime->MAdviseRandomAccess()) {
    // Default every dex file to MADV_RANDOM when its loaded by default for low ram devices.
    // Other devices have enough page cache to get performance benefits from loading more pages
    // into the page cache.
    DexLayoutSection::MadviseLargestPageAlignedRegion(dex_file.Begin(),
                                                      dex_file.Begin() + dex_file.Size(),
                                                      MADV_RANDOM);
  }
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  if (oat_dex_file != nullptr) {
    // Should always be there.
    const DexLayoutSections* const sections = oat_dex_file->GetDexLayoutSections();
    CHECK(sections != nullptr);
    sections->Madvise(&dex_file, state);
  }
}

OatFile::OatClass::OatClass(const OatFile* oat_file,
                            ClassStatus status,
                            OatClassType type,
                            uint32_t bitmap_size,
                            const uint32_t* bitmap_pointer,
                            const OatMethodOffsets* methods_pointer)
    : oat_file_(oat_file), status_(status), type_(type),
      bitmap_(bitmap_pointer), methods_pointer_(methods_pointer) {
    switch (type_) {
      case kOatClassAllCompiled: {
        CHECK_EQ(0U, bitmap_size);
        CHECK(bitmap_pointer == nullptr);
        CHECK(methods_pointer != nullptr);
        break;
      }
      case kOatClassSomeCompiled: {
        CHECK_NE(0U, bitmap_size);
        CHECK(bitmap_pointer != nullptr);
        CHECK(methods_pointer != nullptr);
        break;
      }
      case kOatClassNoneCompiled: {
        CHECK_EQ(0U, bitmap_size);
        CHECK(bitmap_pointer == nullptr);
        CHECK(methods_pointer_ == nullptr);
        break;
      }
      case kOatClassMax: {
        LOG(FATAL) << "Invalid OatClassType " << type_;
        break;
      }
    }
}

uint32_t OatFile::OatClass::GetOatMethodOffsetsOffset(uint32_t method_index) const {
  const OatMethodOffsets* oat_method_offsets = GetOatMethodOffsets(method_index);
  if (oat_method_offsets == nullptr) {
    return 0u;
  }
  return reinterpret_cast<const uint8_t*>(oat_method_offsets) - oat_file_->Begin();
}

const OatMethodOffsets* OatFile::OatClass::GetOatMethodOffsets(uint32_t method_index) const {
  // NOTE: We don't keep the number of methods and cannot do a bounds check for method_index.
  if (methods_pointer_ == nullptr) {
    CHECK_EQ(kOatClassNoneCompiled, type_);
    return nullptr;
  }
  size_t methods_pointer_index;
  if (bitmap_ == nullptr) {
    CHECK_EQ(kOatClassAllCompiled, type_);
    methods_pointer_index = method_index;
  } else {
    CHECK_EQ(kOatClassSomeCompiled, type_);
    if (!BitVector::IsBitSet(bitmap_, method_index)) {
      return nullptr;
    }
    size_t num_set_bits = BitVector::NumSetBits(bitmap_, method_index);
    methods_pointer_index = num_set_bits;
  }
  const OatMethodOffsets& oat_method_offsets = methods_pointer_[methods_pointer_index];
  return &oat_method_offsets;
}

const OatFile::OatMethod OatFile::OatClass::GetOatMethod(uint32_t method_index) const {
  const OatMethodOffsets* oat_method_offsets = GetOatMethodOffsets(method_index);
  if (oat_method_offsets == nullptr) {
    return OatMethod(nullptr, 0);
  }
  if (oat_file_->IsExecutable() ||
      Runtime::Current() == nullptr ||        // This case applies for oatdump.
      Runtime::Current()->IsAotCompiler()) {
    return OatMethod(oat_file_->Begin(), oat_method_offsets->code_offset_);
  }
  // We aren't allowed to use the compiled code. We just force it down the interpreted / jit
  // version.
  return OatMethod(oat_file_->Begin(), 0);
}

void OatFile::OatMethod::LinkMethod(ArtMethod* method) const {
  CHECK(method != nullptr);
  method->SetEntryPointFromQuickCompiledCode(GetQuickCode());
}

bool OatFile::IsPic() const {
  return GetOatHeader().IsPic();
  // TODO: Check against oat_patches. b/18144996
}

bool OatFile::IsDebuggable() const {
  return GetOatHeader().IsDebuggable();
}

CompilerFilter::Filter OatFile::GetCompilerFilter() const {
  return GetOatHeader().GetCompilerFilter();
}

std::string OatFile::GetClassLoaderContext() const {
  return GetOatHeader().GetStoreValueByKey(OatHeader::kClassPathKey);
}

const char* OatFile::GetCompilationReason() const {
  return GetOatHeader().GetStoreValueByKey(OatHeader::kCompilationReasonKey);
}

OatFile::OatClass OatFile::FindOatClass(const DexFile& dex_file,
                                        uint16_t class_def_idx,
                                        bool* found) {
  DCHECK_NE(class_def_idx, DexFile::kDexNoIndex16);
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  if (oat_dex_file == nullptr || oat_dex_file->GetOatFile() == nullptr) {
    *found = false;
    return OatFile::OatClass::Invalid();
  }
  *found = true;
  return oat_dex_file->GetOatClass(class_def_idx);
}

void OatFile::OatDexFile::AssertAotCompiler() {
  CHECK(Runtime::Current()->IsAotCompiler());
}

}  // namespace art
