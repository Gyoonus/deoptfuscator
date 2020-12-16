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

#include "oat_writer.h"

#include <algorithm>
#include <unistd.h>
#include <zlib.h>

#include "arch/arm64/instruction_set_features_arm64.h"
#include "art_method-inl.h"
#include "base/allocator.h"
#include "base/bit_vector-inl.h"
#include "base/enums.h"
#include "base/file_magic.h"
#include "base/logging.h"  // For VLOG
#include "base/os.h"
#include "base/safe_map.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "class_table-inl.h"
#include "compiled_method-inl.h"
#include "debug/method_debug_info.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_types.h"
#include "dex/standard_dex_file.h"
#include "dex/verification_results.h"
#include "dex_container.h"
#include "dexlayout.h"
#include "driver/compiler_driver-inl.h"
#include "driver/compiler_options.h"
#include "gc/space/image_space.h"
#include "gc/space/space.h"
#include "handle_scope-inl.h"
#include "image_writer.h"
#include "jit/profile_compilation_info.h"
#include "linker/buffered_output_stream.h"
#include "linker/file_output_stream.h"
#include "linker/index_bss_mapping_encoder.h"
#include "linker/linker_patch.h"
#include "linker/multi_oat_relative_patcher.h"
#include "linker/output_stream.h"
#include "mirror/array.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "oat_quick_method_header.h"
#include "quicken_info.h"
#include "scoped_thread_state_change-inl.h"
#include "type_lookup_table.h"
#include "utils/dex_cache_arrays_layout-inl.h"
#include "vdex_file.h"
#include "verifier/verifier_deps.h"
#include "zip_archive.h"

namespace art {
namespace linker {

namespace {  // anonymous namespace

// If we write dex layout info in the oat file.
static constexpr bool kWriteDexLayoutInfo = true;

// Force the OAT method layout to be sorted-by-name instead of
// the default (class_def_idx, method_idx).
//
// Otherwise if profiles are used, that will act as
// the primary sort order.
//
// A bit easier to use for development since oatdump can easily
// show that things are being re-ordered when two methods aren't adjacent.
static constexpr bool kOatWriterForceOatCodeLayout = false;

static constexpr bool kOatWriterDebugOatCodeLayout = false;

typedef DexFile::Header __attribute__((aligned(1))) UnalignedDexFileHeader;

const UnalignedDexFileHeader* AsUnalignedDexFileHeader(const uint8_t* raw_data) {
    return reinterpret_cast<const UnalignedDexFileHeader*>(raw_data);
}

class ChecksumUpdatingOutputStream : public OutputStream {
 public:
  ChecksumUpdatingOutputStream(OutputStream* out, OatHeader* oat_header)
      : OutputStream(out->GetLocation()), out_(out), oat_header_(oat_header) { }

  bool WriteFully(const void* buffer, size_t byte_count) OVERRIDE {
    oat_header_->UpdateChecksum(buffer, byte_count);
    return out_->WriteFully(buffer, byte_count);
  }

  off_t Seek(off_t offset, Whence whence) OVERRIDE {
    return out_->Seek(offset, whence);
  }

  bool Flush() OVERRIDE {
    return out_->Flush();
  }

 private:
  OutputStream* const out_;
  OatHeader* const oat_header_;
};

inline uint32_t CodeAlignmentSize(uint32_t header_offset, const CompiledMethod& compiled_method) {
  // We want to align the code rather than the preheader.
  uint32_t unaligned_code_offset = header_offset + sizeof(OatQuickMethodHeader);
  uint32_t aligned_code_offset =  compiled_method.AlignCode(unaligned_code_offset);
  return aligned_code_offset - unaligned_code_offset;
}

}  // anonymous namespace

// Defines the location of the raw dex file to write.
class OatWriter::DexFileSource {
 public:
  enum Type {
    kNone,
    kZipEntry,
    kRawFile,
    kRawData,
  };

  explicit DexFileSource(ZipEntry* zip_entry)
      : type_(kZipEntry), source_(zip_entry) {
    DCHECK(source_ != nullptr);
  }

  explicit DexFileSource(File* raw_file)
      : type_(kRawFile), source_(raw_file) {
    DCHECK(source_ != nullptr);
  }

  explicit DexFileSource(const uint8_t* dex_file)
      : type_(kRawData), source_(dex_file) {
    DCHECK(source_ != nullptr);
  }

  Type GetType() const { return type_; }
  bool IsZipEntry() const { return type_ == kZipEntry; }
  bool IsRawFile() const { return type_ == kRawFile; }
  bool IsRawData() const { return type_ == kRawData; }

  ZipEntry* GetZipEntry() const {
    DCHECK(IsZipEntry());
    DCHECK(source_ != nullptr);
    return static_cast<ZipEntry*>(const_cast<void*>(source_));
  }

  File* GetRawFile() const {
    DCHECK(IsRawFile());
    DCHECK(source_ != nullptr);
    return static_cast<File*>(const_cast<void*>(source_));
  }

  const uint8_t* GetRawData() const {
    DCHECK(IsRawData());
    DCHECK(source_ != nullptr);
    return static_cast<const uint8_t*>(source_);
  }

  void Clear() {
    type_ = kNone;
    source_ = nullptr;
  }

 private:
  Type type_;
  const void* source_;
};

// OatClassHeader is the header only part of the oat class that is required even when compilation
// is not enabled.
class OatWriter::OatClassHeader {
 public:
  OatClassHeader(uint32_t offset,
                 uint32_t num_non_null_compiled_methods,
                 uint32_t num_methods,
                 ClassStatus status)
      : status_(enum_cast<uint16_t>(status)),
        offset_(offset) {
    // We just arbitrarily say that 0 methods means kOatClassNoneCompiled and that we won't use
    // kOatClassAllCompiled unless there is at least one compiled method. This means in an
    // interpreter only system, we can assert that all classes are kOatClassNoneCompiled.
    if (num_non_null_compiled_methods == 0) {
      type_ = kOatClassNoneCompiled;
    } else if (num_non_null_compiled_methods == num_methods) {
      type_ = kOatClassAllCompiled;
    } else {
      type_ = kOatClassSomeCompiled;
    }
  }

  bool Write(OatWriter* oat_writer, OutputStream* out, const size_t file_offset) const;

  static size_t SizeOf() {
    return sizeof(status_) + sizeof(type_);
  }

  // Data to write.
  static_assert(enum_cast<>(ClassStatus::kLast) < (1 << 16), "class status won't fit in 16bits");
  uint16_t status_;

  static_assert(OatClassType::kOatClassMax < (1 << 16), "oat_class type won't fit in 16bits");
  uint16_t type_;

  // Offset of start of OatClass from beginning of OatHeader. It is
  // used to validate file position when writing.
  uint32_t offset_;
};

// The actual oat class body contains the information about compiled methods. It is only required
// for compiler filters that have any compilation.
class OatWriter::OatClass {
 public:
  OatClass(const dchecked_vector<CompiledMethod*>& compiled_methods,
           uint32_t compiled_methods_with_code,
           uint16_t oat_class_type);
  OatClass(OatClass&& src) = default;
  size_t SizeOf() const;
  bool Write(OatWriter* oat_writer, OutputStream* out) const;

  CompiledMethod* GetCompiledMethod(size_t class_def_method_index) const {
    return compiled_methods_[class_def_method_index];
  }

  // CompiledMethods for each class_def_method_index, or null if no method is available.
  dchecked_vector<CompiledMethod*> compiled_methods_;

  // Offset from OatClass::offset_ to the OatMethodOffsets for the
  // class_def_method_index. If 0, it means the corresponding
  // CompiledMethod entry in OatClass::compiled_methods_ should be
  // null and that the OatClass::type_ should be kOatClassBitmap.
  dchecked_vector<uint32_t> oat_method_offsets_offsets_from_oat_class_;

  // Data to write.
  uint32_t method_bitmap_size_;

  // bit vector indexed by ClassDef method index. When
  // OatClassType::type_ is kOatClassBitmap, a set bit indicates the
  // method has an OatMethodOffsets in methods_offsets_, otherwise
  // the entry was ommited to save space. If OatClassType::type_ is
  // not is kOatClassBitmap, the bitmap will be null.
  std::unique_ptr<BitVector> method_bitmap_;

  // OatMethodOffsets and OatMethodHeaders for each CompiledMethod
  // present in the OatClass. Note that some may be missing if
  // OatClass::compiled_methods_ contains null values (and
  // oat_method_offsets_offsets_from_oat_class_ should contain 0
  // values in this case).
  dchecked_vector<OatMethodOffsets> method_offsets_;
  dchecked_vector<OatQuickMethodHeader> method_headers_;

 private:
  size_t GetMethodOffsetsRawSize() const {
    return method_offsets_.size() * sizeof(method_offsets_[0]);
  }

  DISALLOW_COPY_AND_ASSIGN(OatClass);
};

class OatWriter::OatDexFile {
 public:
  OatDexFile(const char* dex_file_location,
             DexFileSource source,
             CreateTypeLookupTable create_type_lookup_table,
             uint32_t dex_file_location_checksun,
             size_t dex_file_size);
  OatDexFile(OatDexFile&& src) = default;

  const char* GetLocation() const {
    return dex_file_location_data_;
  }

  size_t SizeOf() const;
  bool Write(OatWriter* oat_writer, OutputStream* out) const;
  bool WriteClassOffsets(OatWriter* oat_writer, OutputStream* out);

  size_t GetClassOffsetsRawSize() const {
    return class_offsets_.size() * sizeof(class_offsets_[0]);
  }

  // The source of the dex file.
  DexFileSource source_;

  // Whether to create the type lookup table.
  CreateTypeLookupTable create_type_lookup_table_;

  // Dex file size. Passed in the constructor, but could be
  // overwritten by LayoutAndWriteDexFile.
  size_t dex_file_size_;

  // Offset of start of OatDexFile from beginning of OatHeader. It is
  // used to validate file position when writing.
  size_t offset_;

  ///// Start of data to write to vdex/oat file.

  const uint32_t dex_file_location_size_;
  const char* const dex_file_location_data_;

  // The checksum of the dex file.
  const uint32_t dex_file_location_checksum_;

  // Offset of the dex file in the vdex file. Set when writing dex files in
  // SeekToDexFile.
  uint32_t dex_file_offset_;

  // The lookup table offset in the oat file. Set in WriteTypeLookupTables.
  uint32_t lookup_table_offset_;

  // Class and BSS offsets set in PrepareLayout.
  uint32_t class_offsets_offset_;
  uint32_t method_bss_mapping_offset_;
  uint32_t type_bss_mapping_offset_;
  uint32_t string_bss_mapping_offset_;

  // Offset of dex sections that will have different runtime madvise states.
  // Set in WriteDexLayoutSections.
  uint32_t dex_sections_layout_offset_;

  // Data to write to a separate section. We set the length
  // of the vector in OpenDexFiles.
  dchecked_vector<uint32_t> class_offsets_;

  // Dex section layout info to serialize.
  DexLayoutSections dex_sections_layout_;

  ///// End of data to write to vdex/oat file.
 private:
  DISALLOW_COPY_AND_ASSIGN(OatDexFile);
};

#define DCHECK_OFFSET() \
  DCHECK_EQ(static_cast<off_t>(file_offset + relative_offset), out->Seek(0, kSeekCurrent)) \
    << "file_offset=" << file_offset << " relative_offset=" << relative_offset

#define DCHECK_OFFSET_() \
  DCHECK_EQ(static_cast<off_t>(file_offset + offset_), out->Seek(0, kSeekCurrent)) \
    << "file_offset=" << file_offset << " offset_=" << offset_

OatWriter::OatWriter(bool compiling_boot_image,
                     TimingLogger* timings,
                     ProfileCompilationInfo* info,
                     CompactDexLevel compact_dex_level)
  : write_state_(WriteState::kAddingDexFileSources),
    timings_(timings),
    raw_dex_files_(),
    zip_archives_(),
    zipped_dex_files_(),
    zipped_dex_file_locations_(),
    compiler_driver_(nullptr),
    image_writer_(nullptr),
    compiling_boot_image_(compiling_boot_image),
    extract_dex_files_into_vdex_(true),
    dex_files_(nullptr),
    vdex_size_(0u),
    vdex_dex_files_offset_(0u),
    vdex_dex_shared_data_offset_(0u),
    vdex_verifier_deps_offset_(0u),
    vdex_quickening_info_offset_(0u),
    oat_size_(0u),
    bss_start_(0u),
    bss_size_(0u),
    bss_methods_offset_(0u),
    bss_roots_offset_(0u),
    bss_method_entry_references_(),
    bss_method_entries_(),
    bss_type_entries_(),
    bss_string_entries_(),
    map_boot_image_tables_to_bss_(false),
    oat_data_offset_(0u),
    oat_header_(nullptr),
    size_vdex_header_(0),
    size_vdex_checksums_(0),
    size_dex_file_alignment_(0),
    size_executable_offset_alignment_(0),
    size_oat_header_(0),
    size_oat_header_key_value_store_(0),
    size_dex_file_(0),
    size_verifier_deps_(0),
    size_verifier_deps_alignment_(0),
    size_quickening_info_(0),
    size_quickening_info_alignment_(0),
    size_interpreter_to_interpreter_bridge_(0),
    size_interpreter_to_compiled_code_bridge_(0),
    size_jni_dlsym_lookup_(0),
    size_quick_generic_jni_trampoline_(0),
    size_quick_imt_conflict_trampoline_(0),
    size_quick_resolution_trampoline_(0),
    size_quick_to_interpreter_bridge_(0),
    size_trampoline_alignment_(0),
    size_method_header_(0),
    size_code_(0),
    size_code_alignment_(0),
    size_relative_call_thunks_(0),
    size_misc_thunks_(0),
    size_vmap_table_(0),
    size_method_info_(0),
    size_oat_dex_file_location_size_(0),
    size_oat_dex_file_location_data_(0),
    size_oat_dex_file_location_checksum_(0),
    size_oat_dex_file_offset_(0),
    size_oat_dex_file_class_offsets_offset_(0),
    size_oat_dex_file_lookup_table_offset_(0),
    size_oat_dex_file_dex_layout_sections_offset_(0),
    size_oat_dex_file_dex_layout_sections_(0),
    size_oat_dex_file_dex_layout_sections_alignment_(0),
    size_oat_dex_file_method_bss_mapping_offset_(0),
    size_oat_dex_file_type_bss_mapping_offset_(0),
    size_oat_dex_file_string_bss_mapping_offset_(0),
    size_oat_lookup_table_alignment_(0),
    size_oat_lookup_table_(0),
    size_oat_class_offsets_alignment_(0),
    size_oat_class_offsets_(0),
    size_oat_class_type_(0),
    size_oat_class_status_(0),
    size_oat_class_method_bitmaps_(0),
    size_oat_class_method_offsets_(0),
    size_method_bss_mappings_(0u),
    size_type_bss_mappings_(0u),
    size_string_bss_mappings_(0u),
    relative_patcher_(nullptr),
    absolute_patch_locations_(),
    profile_compilation_info_(info),
    compact_dex_level_(compact_dex_level) {
  // If we have a profile, always use at least the default compact dex level. The reason behind
  // this is that CompactDex conversion is not more expensive than normal dexlayout.
  if (info != nullptr && compact_dex_level_ == CompactDexLevel::kCompactDexLevelNone) {
    compact_dex_level_ = kDefaultCompactDexLevel;
  }
}

static bool ValidateDexFileHeader(const uint8_t* raw_header, const char* location) {
  const bool valid_standard_dex_magic = DexFileLoader::IsMagicValid(raw_header);
  if (!valid_standard_dex_magic) {
    LOG(ERROR) << "Invalid magic number in dex file header. " << " File: " << location;
    return false;
  }
  if (!DexFileLoader::IsVersionAndMagicValid(raw_header)) {
    LOG(ERROR) << "Invalid version number in dex file header. " << " File: " << location;
    return false;
  }
  const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(raw_header);
  if (header->file_size_ < sizeof(DexFile::Header)) {
    LOG(ERROR) << "Dex file header specifies file size insufficient to contain the header."
               << " File: " << location;
    return false;
  }
  return true;
}

static const UnalignedDexFileHeader* GetDexFileHeader(File* file,
                                                      uint8_t* raw_header,
                                                      const char* location) {
  // Read the dex file header and perform minimal verification.
  if (!file->ReadFully(raw_header, sizeof(DexFile::Header))) {
    PLOG(ERROR) << "Failed to read dex file header. Actual: "
                << " File: " << location << " Output: " << file->GetPath();
    return nullptr;
  }
  if (!ValidateDexFileHeader(raw_header, location)) {
    return nullptr;
  }

  return AsUnalignedDexFileHeader(raw_header);
}

bool OatWriter::AddDexFileSource(const char* filename,
                                 const char* location,
                                 CreateTypeLookupTable create_type_lookup_table) {
  DCHECK(write_state_ == WriteState::kAddingDexFileSources);
  uint32_t magic;
  std::string error_msg;
  File fd = OpenAndReadMagic(filename, &magic, &error_msg);
  if (fd.Fd() == -1) {
    PLOG(ERROR) << "Failed to read magic number from dex file: '" << filename << "'";
    return false;
  } else if (DexFileLoader::IsMagicValid(magic)) {
    uint8_t raw_header[sizeof(DexFile::Header)];
    const UnalignedDexFileHeader* header = GetDexFileHeader(&fd, raw_header, location);
    if (header == nullptr) {
      return false;
    }
    // The file is open for reading, not writing, so it's OK to let the File destructor
    // close it without checking for explicit Close(), so pass checkUsage = false.
    raw_dex_files_.emplace_back(new File(fd.Release(), location, /* checkUsage */ false));
    oat_dex_files_.emplace_back(/* OatDexFile */
        location,
        DexFileSource(raw_dex_files_.back().get()),
        create_type_lookup_table,
        header->checksum_,
        header->file_size_);
  } else if (IsZipMagic(magic)) {
    if (!AddZippedDexFilesSource(std::move(fd), location, create_type_lookup_table)) {
      return false;
    }
  } else {
    LOG(ERROR) << "Expected valid zip or dex file: '" << filename << "'";
    return false;
  }
  return true;
}

// Add dex file source(s) from a zip file specified by a file handle.
bool OatWriter::AddZippedDexFilesSource(File&& zip_fd,
                                        const char* location,
                                        CreateTypeLookupTable create_type_lookup_table) {
  DCHECK(write_state_ == WriteState::kAddingDexFileSources);
  std::string error_msg;
  zip_archives_.emplace_back(ZipArchive::OpenFromFd(zip_fd.Release(), location, &error_msg));
  ZipArchive* zip_archive = zip_archives_.back().get();
  if (zip_archive == nullptr) {
    LOG(ERROR) << "Failed to open zip from file descriptor for '" << location << "': "
        << error_msg;
    return false;
  }
  for (size_t i = 0; ; ++i) {
    std::string entry_name = DexFileLoader::GetMultiDexClassesDexName(i);
    std::unique_ptr<ZipEntry> entry(zip_archive->Find(entry_name.c_str(), &error_msg));
    if (entry == nullptr) {
      break;
    }
    zipped_dex_files_.push_back(std::move(entry));
    zipped_dex_file_locations_.push_back(DexFileLoader::GetMultiDexLocation(i, location));
    const char* full_location = zipped_dex_file_locations_.back().c_str();
    // We override the checksum from header with the CRC from ZIP entry.
    oat_dex_files_.emplace_back(/* OatDexFile */
        full_location,
        DexFileSource(zipped_dex_files_.back().get()),
        create_type_lookup_table,
        zipped_dex_files_.back()->GetCrc32(),
        zipped_dex_files_.back()->GetUncompressedLength());
  }
  if (zipped_dex_file_locations_.empty()) {
    LOG(ERROR) << "No dex files in zip file '" << location << "': " << error_msg;
    return false;
  }
  return true;
}

// Add dex file source(s) from a vdex file specified by a file handle.
bool OatWriter::AddVdexDexFilesSource(const VdexFile& vdex_file,
                                      const char* location,
                                      CreateTypeLookupTable create_type_lookup_table) {
  DCHECK(write_state_ == WriteState::kAddingDexFileSources);
  DCHECK(vdex_file.HasDexSection());
  const uint8_t* current_dex_data = nullptr;
  for (size_t i = 0; i < vdex_file.GetVerifierDepsHeader().GetNumberOfDexFiles(); ++i) {
    current_dex_data = vdex_file.GetNextDexFileData(current_dex_data);
    if (current_dex_data == nullptr) {
      LOG(ERROR) << "Unexpected number of dex files in vdex " << location;
      return false;
    }

    if (!DexFileLoader::IsMagicValid(current_dex_data)) {
      LOG(ERROR) << "Invalid magic in vdex file created from " << location;
      return false;
    }
    // We used `zipped_dex_file_locations_` to keep the strings in memory.
    zipped_dex_file_locations_.push_back(DexFileLoader::GetMultiDexLocation(i, location));
    const char* full_location = zipped_dex_file_locations_.back().c_str();
    const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(current_dex_data);
    oat_dex_files_.emplace_back(/* OatDexFile */
        full_location,
        DexFileSource(current_dex_data),
        create_type_lookup_table,
        vdex_file.GetLocationChecksum(i),
        header->file_size_);
  }

  if (vdex_file.GetNextDexFileData(current_dex_data) != nullptr) {
    LOG(ERROR) << "Unexpected number of dex files in vdex " << location;
    return false;
  }

  if (oat_dex_files_.empty()) {
    LOG(ERROR) << "No dex files in vdex file created from " << location;
    return false;
  }
  return true;
}

// Add dex file source from raw memory.
bool OatWriter::AddRawDexFileSource(const ArrayRef<const uint8_t>& data,
                                    const char* location,
                                    uint32_t location_checksum,
                                    CreateTypeLookupTable create_type_lookup_table) {
  DCHECK(write_state_ == WriteState::kAddingDexFileSources);
  if (data.size() < sizeof(DexFile::Header)) {
    LOG(ERROR) << "Provided data is shorter than dex file header. size: "
               << data.size() << " File: " << location;
    return false;
  }
  if (!ValidateDexFileHeader(data.data(), location)) {
    return false;
  }
  const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(data.data());
  if (data.size() < header->file_size_) {
    LOG(ERROR) << "Truncated dex file data. Data size: " << data.size()
               << " file size from header: " << header->file_size_ << " File: " << location;
    return false;
  }

  oat_dex_files_.emplace_back(/* OatDexFile */
      location,
      DexFileSource(data.data()),
      create_type_lookup_table,
      location_checksum,
      header->file_size_);
  return true;
}

dchecked_vector<std::string> OatWriter::GetSourceLocations() const {
  dchecked_vector<std::string> locations;
  locations.reserve(oat_dex_files_.size());
  for (const OatDexFile& oat_dex_file : oat_dex_files_) {
    locations.push_back(oat_dex_file.GetLocation());
  }
  return locations;
}

bool OatWriter::MayHaveCompiledMethods() const {
  return CompilerFilter::IsAnyCompilationEnabled(
      GetCompilerDriver()->GetCompilerOptions().GetCompilerFilter());
}

bool OatWriter::WriteAndOpenDexFiles(
    File* vdex_file,
    OutputStream* oat_rodata,
    InstructionSet instruction_set,
    const InstructionSetFeatures* instruction_set_features,
    SafeMap<std::string, std::string>* key_value_store,
    bool verify,
    bool update_input_vdex,
    CopyOption copy_dex_files,
    /*out*/ std::vector<std::unique_ptr<MemMap>>* opened_dex_files_map,
    /*out*/ std::vector<std::unique_ptr<const DexFile>>* opened_dex_files) {
  CHECK(write_state_ == WriteState::kAddingDexFileSources);

  // Record the ELF rodata section offset, i.e. the beginning of the OAT data.
  if (!RecordOatDataOffset(oat_rodata)) {
     return false;
  }

  std::vector<std::unique_ptr<MemMap>> dex_files_map;
  std::vector<std::unique_ptr<const DexFile>> dex_files;

  // Initialize VDEX and OAT headers.

  // Reserve space for Vdex header and checksums.
  vdex_size_ = sizeof(VdexFile::VerifierDepsHeader) +
      oat_dex_files_.size() * sizeof(VdexFile::VdexChecksum);
  oat_size_ = InitOatHeader(instruction_set,
                            instruction_set_features,
                            dchecked_integral_cast<uint32_t>(oat_dex_files_.size()),
                            key_value_store);

  ChecksumUpdatingOutputStream checksum_updating_rodata(oat_rodata, oat_header_.get());

  std::unique_ptr<BufferedOutputStream> vdex_out =
      std::make_unique<BufferedOutputStream>(std::make_unique<FileOutputStream>(vdex_file));
  // Write DEX files into VDEX, mmap and open them.
  if (!WriteDexFiles(vdex_out.get(), vdex_file, update_input_vdex, copy_dex_files) ||
      !OpenDexFiles(vdex_file, verify, &dex_files_map, &dex_files)) {
    return false;
  }

  // Write type lookup tables into the oat file.
  if (!WriteTypeLookupTables(&checksum_updating_rodata, dex_files)) {
    return false;
  }

  // Write dex layout sections into the oat file.
  if (!WriteDexLayoutSections(&checksum_updating_rodata, dex_files)) {
    return false;
  }

  *opened_dex_files_map = std::move(dex_files_map);
  *opened_dex_files = std::move(dex_files);
  write_state_ = WriteState::kPrepareLayout;
  return true;
}

void OatWriter::PrepareLayout(MultiOatRelativePatcher* relative_patcher) {
  CHECK(write_state_ == WriteState::kPrepareLayout);

  relative_patcher_ = relative_patcher;
  SetMultiOatRelativePatcherAdjustment();

  if (compiling_boot_image_) {
    CHECK(image_writer_ != nullptr);
  }
  InstructionSet instruction_set = compiler_driver_->GetInstructionSet();
  CHECK_EQ(instruction_set, oat_header_->GetInstructionSet());

  {
    TimingLogger::ScopedTiming split("InitBssLayout", timings_);
    InitBssLayout(instruction_set);
  }

  uint32_t offset = oat_size_;
  {
    TimingLogger::ScopedTiming split("InitClassOffsets", timings_);
    offset = InitClassOffsets(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatClasses", timings_);
    offset = InitOatClasses(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitIndexBssMappings", timings_);
    offset = InitIndexBssMappings(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatMaps", timings_);
    offset = InitOatMaps(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatDexFiles", timings_);
    oat_header_->SetOatDexFilesOffset(offset);
    offset = InitOatDexFiles(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatCode", timings_);
    offset = InitOatCode(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatCodeDexFiles", timings_);
    offset = InitOatCodeDexFiles(offset);
  }
  oat_size_ = offset;
  bss_start_ = (bss_size_ != 0u) ? RoundUp(oat_size_, kPageSize) : 0u;

  CHECK_EQ(dex_files_->size(), oat_dex_files_.size());
  if (compiling_boot_image_) {
    CHECK_EQ(image_writer_ != nullptr,
             oat_header_->GetStoreValueByKey(OatHeader::kImageLocationKey) == nullptr);
  }

  write_state_ = WriteState::kWriteRoData;
}

OatWriter::~OatWriter() {
}

class OatWriter::DexMethodVisitor {
 public:
  DexMethodVisitor(OatWriter* writer, size_t offset)
      : writer_(writer),
        offset_(offset),
        dex_file_(nullptr),
        class_def_index_(dex::kDexNoIndex) {}

  virtual bool StartClass(const DexFile* dex_file, size_t class_def_index) {
    DCHECK(dex_file_ == nullptr);
    DCHECK_EQ(class_def_index_, dex::kDexNoIndex);
    dex_file_ = dex_file;
    class_def_index_ = class_def_index;
    return true;
  }

  virtual bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) = 0;

  virtual bool EndClass() {
    if (kIsDebugBuild) {
      dex_file_ = nullptr;
      class_def_index_ = dex::kDexNoIndex;
    }
    return true;
  }

  size_t GetOffset() const {
    return offset_;
  }

 protected:
  virtual ~DexMethodVisitor() { }

  OatWriter* const writer_;

  // The offset is usually advanced for each visited method by the derived class.
  size_t offset_;

  // The dex file and class def index are set in StartClass().
  const DexFile* dex_file_;
  size_t class_def_index_;
};

class OatWriter::OatDexMethodVisitor : public DexMethodVisitor {
 public:
  OatDexMethodVisitor(OatWriter* writer, size_t offset)
      : DexMethodVisitor(writer, offset),
        oat_class_index_(0u),
        method_offsets_index_(0u) {}

  bool StartClass(const DexFile* dex_file, size_t class_def_index) OVERRIDE {
    DexMethodVisitor::StartClass(dex_file, class_def_index);
    if (kIsDebugBuild && writer_->MayHaveCompiledMethods()) {
      // There are no oat classes if there aren't any compiled methods.
      CHECK_LT(oat_class_index_, writer_->oat_classes_.size());
    }
    method_offsets_index_ = 0u;
    return true;
  }

  bool EndClass() OVERRIDE {
    ++oat_class_index_;
    return DexMethodVisitor::EndClass();
  }

 protected:
  size_t oat_class_index_;
  size_t method_offsets_index_;
};

static bool HasCompiledCode(const CompiledMethod* method) {
  return method != nullptr && !method->GetQuickCode().empty();
}

static bool HasQuickeningInfo(const CompiledMethod* method) {
  // The dextodexcompiler puts the quickening info table into the CompiledMethod
  // for simplicity.
  return method != nullptr && method->GetQuickCode().empty() && !method->GetVmapTable().empty();
}

class OatWriter::InitBssLayoutMethodVisitor : public DexMethodVisitor {
 public:
  explicit InitBssLayoutMethodVisitor(OatWriter* writer)
      : DexMethodVisitor(writer, /* offset */ 0u) {}

  bool VisitMethod(size_t class_def_method_index ATTRIBUTE_UNUSED,
                   const ClassDataItemIterator& it) OVERRIDE {
    // Look for patches with .bss references and prepare maps with placeholders for their offsets.
    CompiledMethod* compiled_method = writer_->compiler_driver_->GetCompiledMethod(
        MethodReference(dex_file_, it.GetMemberIndex()));
    if (HasCompiledCode(compiled_method)) {
      for (const LinkerPatch& patch : compiled_method->GetPatches()) {
        if (patch.GetType() == LinkerPatch::Type::kMethodBssEntry) {
          MethodReference target_method = patch.TargetMethod();
          AddBssReference(target_method,
                          target_method.dex_file->NumMethodIds(),
                          &writer_->bss_method_entry_references_);
          writer_->bss_method_entries_.Overwrite(target_method, /* placeholder */ 0u);
        } else if (patch.GetType() == LinkerPatch::Type::kTypeBssEntry) {
          TypeReference target_type(patch.TargetTypeDexFile(), patch.TargetTypeIndex());
          AddBssReference(target_type,
                          target_type.dex_file->NumTypeIds(),
                          &writer_->bss_type_entry_references_);
          writer_->bss_type_entries_.Overwrite(target_type, /* placeholder */ 0u);
        } else if (patch.GetType() == LinkerPatch::Type::kStringBssEntry) {
          StringReference target_string(patch.TargetStringDexFile(), patch.TargetStringIndex());
          AddBssReference(target_string,
                          target_string.dex_file->NumStringIds(),
                          &writer_->bss_string_entry_references_);
          writer_->bss_string_entries_.Overwrite(target_string, /* placeholder */ 0u);
        } else if (patch.GetType() == LinkerPatch::Type::kStringInternTable ||
                   patch.GetType() == LinkerPatch::Type::kTypeClassTable) {
          writer_->map_boot_image_tables_to_bss_ = true;
        }
      }
    } else {
      DCHECK(compiled_method == nullptr || compiled_method->GetPatches().empty());
    }
    return true;
  }

 private:
  void AddBssReference(const DexFileReference& ref,
                       size_t number_of_indexes,
                       /*inout*/ SafeMap<const DexFile*, BitVector>* references) {
    // We currently support inlining of throwing instructions only when they originate in the
    // same dex file as the outer method. All .bss references are used by throwing instructions.
    DCHECK_EQ(dex_file_, ref.dex_file);

    auto refs_it = references->find(ref.dex_file);
    if (refs_it == references->end()) {
      refs_it = references->Put(
          ref.dex_file,
          BitVector(number_of_indexes, /* expandable */ false, Allocator::GetMallocAllocator()));
      refs_it->second.ClearAllBits();
    }
    refs_it->second.SetBit(ref.index);
  }
};

class OatWriter::InitOatClassesMethodVisitor : public DexMethodVisitor {
 public:
  InitOatClassesMethodVisitor(OatWriter* writer, size_t offset)
      : DexMethodVisitor(writer, offset),
        compiled_methods_(),
        compiled_methods_with_code_(0u) {
    size_t num_classes = 0u;
    for (const OatDexFile& oat_dex_file : writer_->oat_dex_files_) {
      num_classes += oat_dex_file.class_offsets_.size();
    }
    // If we aren't compiling only reserve headers.
    writer_->oat_class_headers_.reserve(num_classes);
    if (writer->MayHaveCompiledMethods()) {
      writer->oat_classes_.reserve(num_classes);
    }
    compiled_methods_.reserve(256u);
    // If there are any classes, the class offsets allocation aligns the offset.
    DCHECK(num_classes == 0u || IsAligned<4u>(offset));
  }

  bool StartClass(const DexFile* dex_file, size_t class_def_index) OVERRIDE {
    DexMethodVisitor::StartClass(dex_file, class_def_index);
    compiled_methods_.clear();
    compiled_methods_with_code_ = 0u;
    return true;
  }

  bool VisitMethod(size_t class_def_method_index ATTRIBUTE_UNUSED,
                   const ClassDataItemIterator& it) OVERRIDE {
    // Fill in the compiled_methods_ array for methods that have a
    // CompiledMethod. We track the number of non-null entries in
    // compiled_methods_with_code_ since we only want to allocate
    // OatMethodOffsets for the compiled methods.
    uint32_t method_idx = it.GetMemberIndex();
    CompiledMethod* compiled_method =
        writer_->compiler_driver_->GetCompiledMethod(MethodReference(dex_file_, method_idx));
    compiled_methods_.push_back(compiled_method);
    if (HasCompiledCode(compiled_method)) {
      ++compiled_methods_with_code_;
    }
    return true;
  }

  bool EndClass() OVERRIDE {
    ClassReference class_ref(dex_file_, class_def_index_);
    ClassStatus status;
    bool found = writer_->compiler_driver_->GetCompiledClass(class_ref, &status);
    if (!found) {
      VerificationResults* results = writer_->compiler_driver_->GetVerificationResults();
      if (results != nullptr && results->IsClassRejected(class_ref)) {
        // The oat class status is used only for verification of resolved classes,
        // so use ClassStatus::kErrorResolved whether the class was resolved or unresolved
        // during compile-time verification.
        status = ClassStatus::kErrorResolved;
      } else {
        status = ClassStatus::kNotReady;
      }
    }

    writer_->oat_class_headers_.emplace_back(offset_,
                                             compiled_methods_with_code_,
                                             compiled_methods_.size(),
                                             status);
    OatClassHeader& header = writer_->oat_class_headers_.back();
    offset_ += header.SizeOf();
    if (writer_->MayHaveCompiledMethods()) {
      writer_->oat_classes_.emplace_back(compiled_methods_,
                                         compiled_methods_with_code_,
                                         header.type_);
      offset_ += writer_->oat_classes_.back().SizeOf();
    }
    return DexMethodVisitor::EndClass();
  }

 private:
  dchecked_vector<CompiledMethod*> compiled_methods_;
  size_t compiled_methods_with_code_;
};

// CompiledMethod + metadata required to do ordered method layout.
//
// See also OrderedMethodVisitor.
struct OatWriter::OrderedMethodData {
  ProfileCompilationInfo::MethodHotness method_hotness;
  OatClass* oat_class;
  CompiledMethod* compiled_method;
  MethodReference method_reference;
  size_t method_offsets_index;

  size_t class_def_index;
  uint32_t access_flags;
  const DexFile::CodeItem* code_item;

  // A value of -1 denotes missing debug info
  static constexpr size_t kDebugInfoIdxInvalid = static_cast<size_t>(-1);
  // Index into writer_->method_info_
  size_t debug_info_idx;

  bool HasDebugInfo() const {
    return debug_info_idx != kDebugInfoIdxInvalid;
  }

  // Bin each method according to the profile flags.
  //
  // Groups by e.g.
  //  -- not hot at all
  //  -- hot
  //  -- hot and startup
  //  -- hot and post-startup
  //  -- hot and startup and poststartup
  //  -- startup
  //  -- startup and post-startup
  //  -- post-startup
  //
  // (See MethodHotness enum definition for up-to-date binning order.)
  bool operator<(const OrderedMethodData& other) const {
    if (kOatWriterForceOatCodeLayout) {
      // Development flag: Override default behavior by sorting by name.

      std::string name = method_reference.PrettyMethod();
      std::string other_name = other.method_reference.PrettyMethod();
      return name < other_name;
    }

    // Use the profile's method hotness to determine sort order.
    if (GetMethodHotnessOrder() < other.GetMethodHotnessOrder()) {
      return true;
    }

    // Default: retain the original order.
    return false;
  }

 private:
  // Used to determine relative order for OAT code layout when determining
  // binning.
  size_t GetMethodHotnessOrder() const {
    bool hotness[] = {
      method_hotness.IsHot(),
      method_hotness.IsStartup(),
      method_hotness.IsPostStartup()
    };


    // Note: Bin-to-bin order does not matter. If the kernel does or does not read-ahead
    // any memory, it only goes into the buffer cache and does not grow the PSS until the first
    // time that memory is referenced in the process.

    size_t hotness_bits = 0;
    for (size_t i = 0; i < arraysize(hotness); ++i) {
      if (hotness[i]) {
        hotness_bits |= (1 << i);
      }
    }

    if (kIsDebugBuild) {
      // Check for bins that are always-empty given a real profile.
      if (method_hotness.IsHot() &&
              !method_hotness.IsStartup() && !method_hotness.IsPostStartup()) {
        std::string name = method_reference.PrettyMethod();
        LOG(FATAL) << "Method " << name << " had a Hot method that wasn't marked "
                   << "either start-up or post-startup. Possible corrupted profile?";
        // This is not fatal, so only warn.
      }
    }

    return hotness_bits;
  }
};

// Given a queue of CompiledMethod in some total order,
// visit each one in that order.
class OatWriter::OrderedMethodVisitor {
 public:
  explicit OrderedMethodVisitor(OrderedMethodList ordered_methods)
      : ordered_methods_(std::move(ordered_methods)) {
  }

  virtual ~OrderedMethodVisitor() {}

  // Invoke VisitMethod in the order of `ordered_methods`, then invoke VisitComplete.
  bool Visit() REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!VisitStart()) {
      return false;
    }

    for (const OrderedMethodData& method_data : ordered_methods_)  {
      if (!VisitMethod(method_data)) {
        return false;
      }
    }

    return VisitComplete();
  }

  // Invoked once at the beginning, prior to visiting anything else.
  //
  // Return false to abort further visiting.
  virtual bool VisitStart() { return true; }

  // Invoked repeatedly in the order specified by `ordered_methods`.
  //
  // Return false to short-circuit and to stop visiting further methods.
  virtual bool VisitMethod(const OrderedMethodData& method_data)
      REQUIRES_SHARED(Locks::mutator_lock_)  = 0;

  // Invoked once at the end, after every other method has been successfully visited.
  //
  // Return false to indicate the overall `Visit` has failed.
  virtual bool VisitComplete() = 0;

  OrderedMethodList ReleaseOrderedMethods() {
    return std::move(ordered_methods_);
  }

 private:
  // List of compiled methods, sorted by the order defined in OrderedMethodData.
  // Methods can be inserted more than once in case of duplicated methods.
  OrderedMethodList ordered_methods_;
};

// Visit every compiled method in order to determine its order within the OAT file.
// Methods from the same class do not need to be adjacent in the OAT code.
class OatWriter::LayoutCodeMethodVisitor : public OatDexMethodVisitor {
 public:
  LayoutCodeMethodVisitor(OatWriter* writer, size_t offset)
      : OatDexMethodVisitor(writer, offset) {
  }

  bool EndClass() OVERRIDE {
    OatDexMethodVisitor::EndClass();
    return true;
  }

  bool VisitMethod(size_t class_def_method_index,
                   const ClassDataItemIterator& it)
      OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_)  {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());

    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (HasCompiledCode(compiled_method)) {
      size_t debug_info_idx = OrderedMethodData::kDebugInfoIdxInvalid;

      {
        const CompilerOptions& compiler_options = writer_->compiler_driver_->GetCompilerOptions();
        ArrayRef<const uint8_t> quick_code = compiled_method->GetQuickCode();
        uint32_t code_size = quick_code.size() * sizeof(uint8_t);

        // Debug method info must be pushed in the original order
        // (i.e. all methods from the same class must be adjacent in the debug info sections)
        // ElfCompilationUnitWriter::Write requires this.
        if (compiler_options.GenerateAnyDebugInfo() && code_size != 0) {
          debug::MethodDebugInfo info = debug::MethodDebugInfo();
          writer_->method_info_.push_back(info);

          // The debug info is filled in LayoutReserveOffsetCodeMethodVisitor
          // once we know the offsets.
          //
          // Store the index into writer_->method_info_ since future push-backs
          // could reallocate and change the underlying data address.
          debug_info_idx = writer_->method_info_.size() - 1;
        }
      }

      MethodReference method_ref(dex_file_, it.GetMemberIndex());

      // Lookup method hotness from profile, if available.
      // Otherwise assume a default of none-hotness.
      ProfileCompilationInfo::MethodHotness method_hotness =
          writer_->profile_compilation_info_ != nullptr
              ? writer_->profile_compilation_info_->GetMethodHotness(method_ref)
              : ProfileCompilationInfo::MethodHotness();

      // Handle duplicate methods by pushing them repeatedly.
      OrderedMethodData method_data = {
          method_hotness,
          oat_class,
          compiled_method,
          method_ref,
          method_offsets_index_,
          class_def_index_,
          it.GetMethodAccessFlags(),
          it.GetMethodCodeItem(),
          debug_info_idx
      };
      ordered_methods_.push_back(method_data);

      method_offsets_index_++;
    }

    return true;
  }

  OrderedMethodList ReleaseOrderedMethods() {
    if (kOatWriterForceOatCodeLayout || writer_->profile_compilation_info_ != nullptr) {
      // Sort by the method ordering criteria (in OrderedMethodData).
      // Since most methods will have the same ordering criteria,
      // we preserve the original insertion order within the same sort order.
      std::stable_sort(ordered_methods_.begin(), ordered_methods_.end());
    } else {
      // The profile-less behavior is as if every method had 0 hotness
      // associated with it.
      //
      // Since sorting all methods with hotness=0 should give back the same
      // order as before, don't do anything.
      DCHECK(std::is_sorted(ordered_methods_.begin(), ordered_methods_.end()));
    }

    return std::move(ordered_methods_);
  }

 private:
  // List of compiled methods, later to be sorted by order defined in OrderedMethodData.
  // Methods can be inserted more than once in case of duplicated methods.
  OrderedMethodList ordered_methods_;
};

// Given a method order, reserve the offsets for each CompiledMethod in the OAT file.
class OatWriter::LayoutReserveOffsetCodeMethodVisitor : public OrderedMethodVisitor {
 public:
  LayoutReserveOffsetCodeMethodVisitor(OatWriter* writer,
                                       size_t offset,
                                       OrderedMethodList ordered_methods)
      : LayoutReserveOffsetCodeMethodVisitor(writer,
                                             offset,
                                             writer->GetCompilerDriver()->GetCompilerOptions(),
                                             std::move(ordered_methods)) {
  }

  virtual bool VisitComplete() OVERRIDE {
    offset_ = writer_->relative_patcher_->ReserveSpaceEnd(offset_);
    if (generate_debug_info_) {
      std::vector<debug::MethodDebugInfo> thunk_infos =
          relative_patcher_->GenerateThunkDebugInfo(executable_offset_);
      writer_->method_info_.insert(writer_->method_info_.end(),
                                   std::make_move_iterator(thunk_infos.begin()),
                                   std::make_move_iterator(thunk_infos.end()));
    }
    return true;
  }

  virtual bool VisitMethod(const OrderedMethodData& method_data)
      OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    OatClass* oat_class = method_data.oat_class;
    CompiledMethod* compiled_method = method_data.compiled_method;
    const MethodReference& method_ref = method_data.method_reference;
    uint16_t method_offsets_index_ = method_data.method_offsets_index;
    size_t class_def_index = method_data.class_def_index;
    uint32_t access_flags = method_data.access_flags;
    bool has_debug_info = method_data.HasDebugInfo();
    size_t debug_info_idx = method_data.debug_info_idx;

    DCHECK(HasCompiledCode(compiled_method)) << method_ref.PrettyMethod();

    // Derived from CompiledMethod.
    uint32_t quick_code_offset = 0;

    ArrayRef<const uint8_t> quick_code = compiled_method->GetQuickCode();
    uint32_t code_size = quick_code.size() * sizeof(uint8_t);
    uint32_t thumb_offset = compiled_method->CodeDelta();

    // Deduplicate code arrays if we are not producing debuggable code.
    bool deduped = true;
    if (debuggable_) {
      quick_code_offset = relative_patcher_->GetOffset(method_ref);
      if (quick_code_offset != 0u) {
        // Duplicate methods, we want the same code for both of them so that the oat writer puts
        // the same code in both ArtMethods so that we do not get different oat code at runtime.
      } else {
        quick_code_offset = NewQuickCodeOffset(compiled_method, method_ref, thumb_offset);
        deduped = false;
      }
    } else {
      quick_code_offset = dedupe_map_.GetOrCreate(
          compiled_method,
          [this, &deduped, compiled_method, &method_ref, thumb_offset]() {
            deduped = false;
            return NewQuickCodeOffset(compiled_method, method_ref, thumb_offset);
          });
    }

    if (code_size != 0) {
      if (relative_patcher_->GetOffset(method_ref) != 0u) {
        // TODO: Should this be a hard failure?
        LOG(WARNING) << "Multiple definitions of "
            << method_ref.dex_file->PrettyMethod(method_ref.index)
            << " offsets " << relative_patcher_->GetOffset(method_ref)
            << " " << quick_code_offset;
      } else {
        relative_patcher_->SetOffset(method_ref, quick_code_offset);
      }
    }

    // Update quick method header.
    DCHECK_LT(method_offsets_index_, oat_class->method_headers_.size());
    OatQuickMethodHeader* method_header = &oat_class->method_headers_[method_offsets_index_];
    uint32_t vmap_table_offset = method_header->GetVmapTableOffset();
    uint32_t method_info_offset = method_header->GetMethodInfoOffset();
    // The code offset was 0 when the mapping/vmap table offset was set, so it's set
    // to 0-offset and we need to adjust it by code_offset.
    uint32_t code_offset = quick_code_offset - thumb_offset;
    CHECK(!compiled_method->GetQuickCode().empty());
    // If the code is compiled, we write the offset of the stack map relative
    // to the code.
    if (vmap_table_offset != 0u) {
      vmap_table_offset += code_offset;
      DCHECK_LT(vmap_table_offset, code_offset);
    }
    if (method_info_offset != 0u) {
      method_info_offset += code_offset;
      DCHECK_LT(method_info_offset, code_offset);
    }
    uint32_t frame_size_in_bytes = compiled_method->GetFrameSizeInBytes();
    uint32_t core_spill_mask = compiled_method->GetCoreSpillMask();
    uint32_t fp_spill_mask = compiled_method->GetFpSpillMask();
    *method_header = OatQuickMethodHeader(vmap_table_offset,
                                          method_info_offset,
                                          frame_size_in_bytes,
                                          core_spill_mask,
                                          fp_spill_mask,
                                          code_size);

    if (!deduped) {
      // Update offsets. (Checksum is updated when writing.)
      offset_ += sizeof(*method_header);  // Method header is prepended before code.
      offset_ += code_size;
      // Record absolute patch locations.
      if (!compiled_method->GetPatches().empty()) {
        uintptr_t base_loc = offset_ - code_size - writer_->oat_header_->GetExecutableOffset();
        for (const LinkerPatch& patch : compiled_method->GetPatches()) {
          if (!patch.IsPcRelative()) {
            writer_->absolute_patch_locations_.push_back(base_loc + patch.LiteralOffset());
          }
        }
      }
    }

    // Exclude quickened dex methods (code_size == 0) since they have no native code.
    if (generate_debug_info_ && code_size != 0) {
      DCHECK(has_debug_info);

      bool has_code_info = method_header->IsOptimized();
      // Record debug information for this function if we are doing that.
      debug::MethodDebugInfo& info = writer_->method_info_[debug_info_idx];
      DCHECK(info.custom_name.empty());
      info.dex_file = method_ref.dex_file;
      info.class_def_index = class_def_index;
      info.dex_method_index = method_ref.index;
      info.access_flags = access_flags;
      // For intrinsics emitted by codegen, the code has no relation to the original code item.
      info.code_item = compiled_method->IsIntrinsic() ? nullptr : method_data.code_item;
      info.isa = compiled_method->GetInstructionSet();
      info.deduped = deduped;
      info.is_native_debuggable = native_debuggable_;
      info.is_optimized = method_header->IsOptimized();
      info.is_code_address_text_relative = true;
      info.code_address = code_offset - executable_offset_;
      info.code_size = code_size;
      info.frame_size_in_bytes = compiled_method->GetFrameSizeInBytes();
      info.code_info = has_code_info ? compiled_method->GetVmapTable().data() : nullptr;
      info.cfi = compiled_method->GetCFIInfo();
    } else {
      DCHECK(!has_debug_info);
    }

    DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
    OatMethodOffsets* offsets = &oat_class->method_offsets_[method_offsets_index_];
    offsets->code_offset_ = quick_code_offset;

    return true;
  }

  size_t GetOffset() const {
    return offset_;
  }

 private:
  LayoutReserveOffsetCodeMethodVisitor(OatWriter* writer,
                                       size_t offset,
                                       const CompilerOptions& compiler_options,
                                       OrderedMethodList ordered_methods)
      : OrderedMethodVisitor(std::move(ordered_methods)),
        writer_(writer),
        offset_(offset),
        relative_patcher_(writer->relative_patcher_),
        executable_offset_(writer->oat_header_->GetExecutableOffset()),
        debuggable_(compiler_options.GetDebuggable()),
        native_debuggable_(compiler_options.GetNativeDebuggable()),
        generate_debug_info_(compiler_options.GenerateAnyDebugInfo()) {
    writer->absolute_patch_locations_.reserve(
        writer->GetCompilerDriver()->GetNonRelativeLinkerPatchCount());
  }

  struct CodeOffsetsKeyComparator {
    bool operator()(const CompiledMethod* lhs, const CompiledMethod* rhs) const {
      // Code is deduplicated by CompilerDriver, compare only data pointers.
      if (lhs->GetQuickCode().data() != rhs->GetQuickCode().data()) {
        return lhs->GetQuickCode().data() < rhs->GetQuickCode().data();
      }
      // If the code is the same, all other fields are likely to be the same as well.
      if (UNLIKELY(lhs->GetVmapTable().data() != rhs->GetVmapTable().data())) {
        return lhs->GetVmapTable().data() < rhs->GetVmapTable().data();
      }
      if (UNLIKELY(lhs->GetMethodInfo().data() != rhs->GetMethodInfo().data())) {
        return lhs->GetMethodInfo().data() < rhs->GetMethodInfo().data();
      }
      if (UNLIKELY(lhs->GetPatches().data() != rhs->GetPatches().data())) {
        return lhs->GetPatches().data() < rhs->GetPatches().data();
      }
      if (UNLIKELY(lhs->IsIntrinsic() != rhs->IsIntrinsic())) {
        return rhs->IsIntrinsic();
      }
      return false;
    }
  };

  uint32_t NewQuickCodeOffset(CompiledMethod* compiled_method,
                              const MethodReference& method_ref,
                              uint32_t thumb_offset) {
    offset_ = relative_patcher_->ReserveSpace(offset_, compiled_method, method_ref);
    offset_ += CodeAlignmentSize(offset_, *compiled_method);
    DCHECK_ALIGNED_PARAM(offset_ + sizeof(OatQuickMethodHeader),
                         GetInstructionSetAlignment(compiled_method->GetInstructionSet()));
    return offset_ + sizeof(OatQuickMethodHeader) + thumb_offset;
  }

  OatWriter* writer_;

  // Offset of the code of the compiled methods.
  size_t offset_;

  // Deduplication is already done on a pointer basis by the compiler driver,
  // so we can simply compare the pointers to find out if things are duplicated.
  SafeMap<const CompiledMethod*, uint32_t, CodeOffsetsKeyComparator> dedupe_map_;

  // Cache writer_'s members and compiler options.
  MultiOatRelativePatcher* relative_patcher_;
  uint32_t executable_offset_;
  const bool debuggable_;
  const bool native_debuggable_;
  const bool generate_debug_info_;
};

class OatWriter::InitMapMethodVisitor : public OatDexMethodVisitor {
 public:
  InitMapMethodVisitor(OatWriter* writer, size_t offset)
      : OatDexMethodVisitor(writer, offset) {}

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (HasCompiledCode(compiled_method)) {
      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      DCHECK_EQ(oat_class->method_headers_[method_offsets_index_].GetVmapTableOffset(), 0u);

      ArrayRef<const uint8_t> map = compiled_method->GetVmapTable();
      uint32_t map_size = map.size() * sizeof(map[0]);
      if (map_size != 0u) {
        size_t offset = dedupe_map_.GetOrCreate(
            map.data(),
            [this, map_size]() {
              uint32_t new_offset = offset_;
              offset_ += map_size;
              return new_offset;
            });
        // Code offset is not initialized yet, so set the map offset to 0u-offset.
        DCHECK_EQ(oat_class->method_offsets_[method_offsets_index_].code_offset_, 0u);
        oat_class->method_headers_[method_offsets_index_].SetVmapTableOffset(0u - offset);
      }
      ++method_offsets_index_;
    }

    return true;
  }

 private:
  // Deduplication is already done on a pointer basis by the compiler driver,
  // so we can simply compare the pointers to find out if things are duplicated.
  SafeMap<const uint8_t*, uint32_t> dedupe_map_;
};

class OatWriter::InitMethodInfoVisitor : public OatDexMethodVisitor {
 public:
  InitMethodInfoVisitor(OatWriter* writer, size_t offset) : OatDexMethodVisitor(writer, offset) {}

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (HasCompiledCode(compiled_method)) {
      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      DCHECK_EQ(oat_class->method_headers_[method_offsets_index_].GetMethodInfoOffset(), 0u);
      ArrayRef<const uint8_t> map = compiled_method->GetMethodInfo();
      const uint32_t map_size = map.size() * sizeof(map[0]);
      if (map_size != 0u) {
        size_t offset = dedupe_map_.GetOrCreate(
            map.data(),
            [this, map_size]() {
              uint32_t new_offset = offset_;
              offset_ += map_size;
              return new_offset;
            });
        // Code offset is not initialized yet, so set the map offset to 0u-offset.
        DCHECK_EQ(oat_class->method_offsets_[method_offsets_index_].code_offset_, 0u);
        oat_class->method_headers_[method_offsets_index_].SetMethodInfoOffset(0u - offset);
      }
      ++method_offsets_index_;
    }

    return true;
  }

 private:
  // Deduplication is already done on a pointer basis by the compiler driver,
  // so we can simply compare the pointers to find out if things are duplicated.
  SafeMap<const uint8_t*, uint32_t> dedupe_map_;
};

class OatWriter::InitImageMethodVisitor : public OatDexMethodVisitor {
 public:
  InitImageMethodVisitor(OatWriter* writer,
                         size_t offset,
                         const std::vector<const DexFile*>* dex_files)
      : OatDexMethodVisitor(writer, offset),
        pointer_size_(GetInstructionSetPointerSize(writer_->compiler_driver_->GetInstructionSet())),
        class_loader_(writer->HasImage() ? writer->image_writer_->GetClassLoader() : nullptr),
        dex_files_(dex_files),
        class_linker_(Runtime::Current()->GetClassLinker()) {}

  // Handle copied methods here. Copy pointer to quick code from
  // an origin method to a copied method only if they are
  // in the same oat file. If the origin and the copied methods are
  // in different oat files don't touch the copied method.
  // References to other oat files are not supported yet.
  bool StartClass(const DexFile* dex_file, size_t class_def_index) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    OatDexMethodVisitor::StartClass(dex_file, class_def_index);
    // Skip classes that are not in the image.
    if (!IsImageClass()) {
      return true;
    }
    ObjPtr<mirror::DexCache> dex_cache = class_linker_->FindDexCache(Thread::Current(), *dex_file);
    const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
    mirror::Class* klass = dex_cache->GetResolvedType(class_def.class_idx_);
    if (klass != nullptr) {
      for (ArtMethod& method : klass->GetCopiedMethods(pointer_size_)) {
        // Find origin method. Declaring class and dex_method_idx
        // in the copied method should be the same as in the origin
        // method.
        mirror::Class* declaring_class = method.GetDeclaringClass();
        ArtMethod* origin = declaring_class->FindClassMethod(
            declaring_class->GetDexCache(),
            method.GetDexMethodIndex(),
            pointer_size_);
        CHECK(origin != nullptr);
        CHECK(!origin->IsDirect());
        CHECK(origin->GetDeclaringClass() == declaring_class);
        if (IsInOatFile(&declaring_class->GetDexFile())) {
          const void* code_ptr =
              origin->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size_);
          if (code_ptr == nullptr) {
            methods_to_process_.push_back(std::make_pair(&method, origin));
          } else {
            method.SetEntryPointFromQuickCompiledCodePtrSize(
                code_ptr, pointer_size_);
          }
        }
      }
    }
    return true;
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Skip methods that are not in the image.
    if (!IsImageClass()) {
      return true;
    }

    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    OatMethodOffsets offsets(0u);
    if (HasCompiledCode(compiled_method)) {
      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      offsets = oat_class->method_offsets_[method_offsets_index_];
      ++method_offsets_index_;
    }

    Thread* self = Thread::Current();
    ObjPtr<mirror::DexCache> dex_cache = class_linker_->FindDexCache(self, *dex_file_);
    ArtMethod* method;
    if (writer_->HasBootImage()) {
      const InvokeType invoke_type = it.GetMethodInvokeType(
          dex_file_->GetClassDef(class_def_index_));
      // Unchecked as we hold mutator_lock_ on entry.
      ScopedObjectAccessUnchecked soa(self);
      StackHandleScope<1> hs(self);
      method = class_linker_->ResolveMethod<ClassLinker::ResolveMode::kNoChecks>(
          it.GetMemberIndex(),
          hs.NewHandle(dex_cache),
          ScopedNullHandle<mirror::ClassLoader>(),
          /* referrer */ nullptr,
          invoke_type);
      if (method == nullptr) {
        LOG(FATAL_WITHOUT_ABORT) << "Unexpected failure to resolve a method: "
            << dex_file_->PrettyMethod(it.GetMemberIndex(), true);
        self->AssertPendingException();
        mirror::Throwable* exc = self->GetException();
        std::string dump = exc->Dump();
        LOG(FATAL) << dump;
        UNREACHABLE();
      }
    } else {
      // Should already have been resolved by the compiler.
      // It may not be resolved if the class failed to verify, in this case, don't set the
      // entrypoint. This is not fatal since we shall use a resolution method.
      method = class_linker_->LookupResolvedMethod(it.GetMemberIndex(), dex_cache, class_loader_);
    }
    if (method != nullptr &&
        compiled_method != nullptr &&
        compiled_method->GetQuickCode().size() != 0) {
      method->SetEntryPointFromQuickCompiledCodePtrSize(
          reinterpret_cast<void*>(offsets.code_offset_), pointer_size_);
    }

    return true;
  }

  // Check whether current class is image class
  bool IsImageClass() {
    const DexFile::TypeId& type_id =
        dex_file_->GetTypeId(dex_file_->GetClassDef(class_def_index_).class_idx_);
    const char* class_descriptor = dex_file_->GetTypeDescriptor(type_id);
    return writer_->GetCompilerDriver()->IsImageClass(class_descriptor);
  }

  // Check whether specified dex file is in the compiled oat file.
  bool IsInOatFile(const DexFile* dex_file) {
    return ContainsElement(*dex_files_, dex_file);
  }

  // Assign a pointer to quick code for copied methods
  // not handled in the method StartClass
  void Postprocess() {
    for (std::pair<ArtMethod*, ArtMethod*>& p : methods_to_process_) {
      ArtMethod* method = p.first;
      ArtMethod* origin = p.second;
      const void* code_ptr =
          origin->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size_);
      if (code_ptr != nullptr) {
        method->SetEntryPointFromQuickCompiledCodePtrSize(code_ptr, pointer_size_);
      }
    }
  }

 private:
  const PointerSize pointer_size_;
  ObjPtr<mirror::ClassLoader> class_loader_;
  const std::vector<const DexFile*>* dex_files_;
  ClassLinker* const class_linker_;
  std::vector<std::pair<ArtMethod*, ArtMethod*>> methods_to_process_;
};

class OatWriter::WriteCodeMethodVisitor : public OrderedMethodVisitor {
 public:
  WriteCodeMethodVisitor(OatWriter* writer,
                         OutputStream* out,
                         const size_t file_offset,
                         size_t relative_offset,
                         OrderedMethodList ordered_methods)
      : OrderedMethodVisitor(std::move(ordered_methods)),
        writer_(writer),
        offset_(relative_offset),
        dex_file_(nullptr),
        pointer_size_(GetInstructionSetPointerSize(writer_->compiler_driver_->GetInstructionSet())),
        class_loader_(writer->HasImage() ? writer->image_writer_->GetClassLoader() : nullptr),
        out_(out),
        file_offset_(file_offset),
        class_linker_(Runtime::Current()->GetClassLinker()),
        dex_cache_(nullptr),
        no_thread_suspension_("OatWriter patching") {
    patched_code_.reserve(16 * KB);
    if (writer_->HasBootImage()) {
      // If we're creating the image, the address space must be ready so that we can apply patches.
      CHECK(writer_->image_writer_->IsImageAddressSpaceReady());
    }
  }

  virtual bool VisitStart() OVERRIDE {
    return true;
  }

  void UpdateDexFileAndDexCache(const DexFile* dex_file)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    dex_file_ = dex_file;

    // Ordered method visiting is only for compiled methods.
    DCHECK(writer_->MayHaveCompiledMethods());

    if (writer_->GetCompilerDriver()->GetCompilerOptions().IsAotCompilationEnabled()) {
      // Only need to set the dex cache if we have compilation. Other modes might have unloaded it.
      if (dex_cache_ == nullptr || dex_cache_->GetDexFile() != dex_file) {
        dex_cache_ = class_linker_->FindDexCache(Thread::Current(), *dex_file);
        DCHECK(dex_cache_ != nullptr);
      }
    }
  }

  virtual bool VisitComplete() {
    offset_ = writer_->relative_patcher_->WriteThunks(out_, offset_);
    if (UNLIKELY(offset_ == 0u)) {
      PLOG(ERROR) << "Failed to write final relative call thunks";
      return false;
    }
    return true;
  }

  virtual bool VisitMethod(const OrderedMethodData& method_data) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const MethodReference& method_ref = method_data.method_reference;
    UpdateDexFileAndDexCache(method_ref.dex_file);

    OatClass* oat_class = method_data.oat_class;
    CompiledMethod* compiled_method = method_data.compiled_method;
    uint16_t method_offsets_index = method_data.method_offsets_index;

    // No thread suspension since dex_cache_ that may get invalidated if that occurs.
    ScopedAssertNoThreadSuspension tsc(__FUNCTION__);
    DCHECK(HasCompiledCode(compiled_method)) << method_ref.PrettyMethod();

    // TODO: cleanup DCHECK_OFFSET_ to accept file_offset as parameter.
    size_t file_offset = file_offset_;  // Used by DCHECK_OFFSET_ macro.
    OutputStream* out = out_;

    ArrayRef<const uint8_t> quick_code = compiled_method->GetQuickCode();
    uint32_t code_size = quick_code.size() * sizeof(uint8_t);

    // Deduplicate code arrays.
    const OatMethodOffsets& method_offsets = oat_class->method_offsets_[method_offsets_index];
    if (method_offsets.code_offset_ > offset_) {
      offset_ = writer_->relative_patcher_->WriteThunks(out, offset_);
      if (offset_ == 0u) {
        ReportWriteFailure("relative call thunk", method_ref);
        return false;
      }
      uint32_t alignment_size = CodeAlignmentSize(offset_, *compiled_method);
      if (alignment_size != 0) {
        if (!writer_->WriteCodeAlignment(out, alignment_size)) {
          ReportWriteFailure("code alignment padding", method_ref);
          return false;
        }
        offset_ += alignment_size;
        DCHECK_OFFSET_();
      }
      DCHECK_ALIGNED_PARAM(offset_ + sizeof(OatQuickMethodHeader),
                           GetInstructionSetAlignment(compiled_method->GetInstructionSet()));
      DCHECK_EQ(method_offsets.code_offset_,
                offset_ + sizeof(OatQuickMethodHeader) + compiled_method->CodeDelta())
          << dex_file_->PrettyMethod(method_ref.index);
      const OatQuickMethodHeader& method_header =
          oat_class->method_headers_[method_offsets_index];
      if (!out->WriteFully(&method_header, sizeof(method_header))) {
        ReportWriteFailure("method header", method_ref);
        return false;
      }
      writer_->size_method_header_ += sizeof(method_header);
      offset_ += sizeof(method_header);
      DCHECK_OFFSET_();

      if (!compiled_method->GetPatches().empty()) {
        patched_code_.assign(quick_code.begin(), quick_code.end());
        quick_code = ArrayRef<const uint8_t>(patched_code_);
        for (const LinkerPatch& patch : compiled_method->GetPatches()) {
          uint32_t literal_offset = patch.LiteralOffset();
          switch (patch.GetType()) {
            case LinkerPatch::Type::kMethodBssEntry: {
              uint32_t target_offset =
                  writer_->bss_start_ + writer_->bss_method_entries_.Get(patch.TargetMethod());
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kCallRelative: {
              // NOTE: Relative calls across oat files are not supported.
              uint32_t target_offset = GetTargetOffset(patch);
              writer_->relative_patcher_->PatchCall(&patched_code_,
                                                    literal_offset,
                                                    offset_ + literal_offset,
                                                    target_offset);
              break;
            }
            case LinkerPatch::Type::kStringRelative: {
              uint32_t target_offset = GetTargetObjectOffset(GetTargetString(patch));
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kStringInternTable: {
              uint32_t target_offset = GetInternTableEntryOffset(patch);
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kStringBssEntry: {
              StringReference ref(patch.TargetStringDexFile(), patch.TargetStringIndex());
              uint32_t target_offset =
                  writer_->bss_start_ + writer_->bss_string_entries_.Get(ref);
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kTypeRelative: {
              uint32_t target_offset = GetTargetObjectOffset(GetTargetType(patch));
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kTypeClassTable: {
              uint32_t target_offset = GetClassTableEntryOffset(patch);
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kTypeBssEntry: {
              TypeReference ref(patch.TargetTypeDexFile(), patch.TargetTypeIndex());
              uint32_t target_offset = writer_->bss_start_ + writer_->bss_type_entries_.Get(ref);
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kCall: {
              uint32_t target_offset = GetTargetOffset(patch);
              PatchCodeAddress(&patched_code_, literal_offset, target_offset);
              break;
            }
            case LinkerPatch::Type::kMethodRelative: {
              uint32_t target_offset = GetTargetMethodOffset(GetTargetMethod(patch));
              writer_->relative_patcher_->PatchPcRelativeReference(&patched_code_,
                                                                   patch,
                                                                   offset_ + literal_offset,
                                                                   target_offset);
              break;
            }
            case LinkerPatch::Type::kBakerReadBarrierBranch: {
              writer_->relative_patcher_->PatchBakerReadBarrierBranch(&patched_code_,
                                                                      patch,
                                                                      offset_ + literal_offset);
              break;
            }
            default: {
              DCHECK(false) << "Unexpected linker patch type: " << patch.GetType();
              break;
            }
          }
        }
      }

      if (!out->WriteFully(quick_code.data(), code_size)) {
        ReportWriteFailure("method code", method_ref);
        return false;
      }
      writer_->size_code_ += code_size;
      offset_ += code_size;
    }
    DCHECK_OFFSET_();

    return true;
  }

  size_t GetOffset() const {
    return offset_;
  }

 private:
  OatWriter* const writer_;

  // Updated in VisitMethod as methods are written out.
  size_t offset_;

  // Potentially varies with every different VisitMethod.
  // Used to determine which DexCache to use when finding ArtMethods.
  const DexFile* dex_file_;

  // Pointer size we are compiling to.
  const PointerSize pointer_size_;
  // The image writer's classloader, if there is one, else null.
  ObjPtr<mirror::ClassLoader> class_loader_;
  // Stream to output file, where the OAT code will be written to.
  OutputStream* const out_;
  const size_t file_offset_;
  ClassLinker* const class_linker_;
  ObjPtr<mirror::DexCache> dex_cache_;
  std::vector<uint8_t> patched_code_;
  const ScopedAssertNoThreadSuspension no_thread_suspension_;

  void ReportWriteFailure(const char* what, const MethodReference& method_ref) {
    PLOG(ERROR) << "Failed to write " << what << " for "
        << method_ref.PrettyMethod() << " to " << out_->GetLocation();
  }

  ArtMethod* GetTargetMethod(const LinkerPatch& patch)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    MethodReference ref = patch.TargetMethod();
    ObjPtr<mirror::DexCache> dex_cache =
        (dex_file_ == ref.dex_file) ? dex_cache_ : class_linker_->FindDexCache(
            Thread::Current(), *ref.dex_file);
    ArtMethod* method =
        class_linker_->LookupResolvedMethod(ref.index, dex_cache, class_loader_);
    CHECK(method != nullptr);
    return method;
  }

  uint32_t GetTargetOffset(const LinkerPatch& patch) REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t target_offset = writer_->relative_patcher_->GetOffset(patch.TargetMethod());
    // If there's no new compiled code, either we're compiling an app and the target method
    // is in the boot image, or we need to point to the correct trampoline.
    if (UNLIKELY(target_offset == 0)) {
      ArtMethod* target = GetTargetMethod(patch);
      DCHECK(target != nullptr);
      const void* oat_code_offset =
          target->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size_);
      if (oat_code_offset != 0) {
        DCHECK(!writer_->HasBootImage());
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickResolutionStub(oat_code_offset));
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickToInterpreterBridge(oat_code_offset));
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickGenericJniStub(oat_code_offset));
        target_offset = PointerToLowMemUInt32(oat_code_offset);
      } else {
        target_offset = target->IsNative()
            ? writer_->oat_header_->GetQuickGenericJniTrampolineOffset()
            : writer_->oat_header_->GetQuickToInterpreterBridgeOffset();
      }
    }
    return target_offset;
  }

  ObjPtr<mirror::DexCache> GetDexCache(const DexFile* target_dex_file)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return (target_dex_file == dex_file_)
        ? dex_cache_
        : class_linker_->FindDexCache(Thread::Current(), *target_dex_file);
  }

  ObjPtr<mirror::Class> GetTargetType(const LinkerPatch& patch)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(writer_->HasImage());
    ObjPtr<mirror::DexCache> dex_cache = GetDexCache(patch.TargetTypeDexFile());
    ObjPtr<mirror::Class> type =
        class_linker_->LookupResolvedType(patch.TargetTypeIndex(), dex_cache, class_loader_);
    CHECK(type != nullptr);
    return type;
  }

  ObjPtr<mirror::String> GetTargetString(const LinkerPatch& patch)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ClassLinker* linker = Runtime::Current()->GetClassLinker();
    ObjPtr<mirror::String> string =
        linker->LookupString(patch.TargetStringIndex(), GetDexCache(patch.TargetStringDexFile()));
    DCHECK(string != nullptr);
    DCHECK(writer_->HasBootImage() ||
           Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(string));
    return string;
  }

  uint32_t GetTargetMethodOffset(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(writer_->HasBootImage());
    method = writer_->image_writer_->GetImageMethodAddress(method);
    size_t oat_index = writer_->image_writer_->GetOatIndexForDexFile(dex_file_);
    uintptr_t oat_data_begin = writer_->image_writer_->GetOatDataBegin(oat_index);
    // TODO: Clean up offset types. The target offset must be treated as signed.
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(method) - oat_data_begin);
  }

  uint32_t GetTargetObjectOffset(ObjPtr<mirror::Object> object)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(writer_->HasBootImage());
    object = writer_->image_writer_->GetImageAddress(object.Ptr());
    size_t oat_index = writer_->image_writer_->GetOatIndexForDexFile(dex_file_);
    uintptr_t oat_data_begin = writer_->image_writer_->GetOatDataBegin(oat_index);
    // TODO: Clean up offset types. The target offset must be treated as signed.
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object.Ptr()) - oat_data_begin);
  }

  void PatchObjectAddress(std::vector<uint8_t>* code, uint32_t offset, mirror::Object* object)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (writer_->HasBootImage()) {
      object = writer_->image_writer_->GetImageAddress(object);
    } else {
      // NOTE: We're using linker patches for app->boot references when the image can
      // be relocated and therefore we need to emit .oat_patches. We're not using this
      // for app->app references, so check that the object is in the image space.
      DCHECK(Runtime::Current()->GetHeap()->FindSpaceFromObject(object, false)->IsImageSpace());
    }
    // Note: We only patch targeting Objects in image which is in the low 4gb.
    uint32_t address = PointerToLowMemUInt32(object);
    DCHECK_LE(offset + 4, code->size());
    uint8_t* data = &(*code)[offset];
    data[0] = address & 0xffu;
    data[1] = (address >> 8) & 0xffu;
    data[2] = (address >> 16) & 0xffu;
    data[3] = (address >> 24) & 0xffu;
  }

  void PatchCodeAddress(std::vector<uint8_t>* code, uint32_t offset, uint32_t target_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t address = target_offset;
    if (writer_->HasBootImage()) {
      size_t oat_index = writer_->image_writer_->GetOatIndexForDexCache(dex_cache_);
      // TODO: Clean up offset types.
      // The target_offset must be treated as signed for cross-oat patching.
      const void* target = reinterpret_cast<const void*>(
          writer_->image_writer_->GetOatDataBegin(oat_index) +
          static_cast<int32_t>(target_offset));
      address = PointerToLowMemUInt32(target);
    }
    DCHECK_LE(offset + 4, code->size());
    uint8_t* data = &(*code)[offset];
    data[0] = address & 0xffu;
    data[1] = (address >> 8) & 0xffu;
    data[2] = (address >> 16) & 0xffu;
    data[3] = (address >> 24) & 0xffu;
  }

  // Calculate the offset of the InternTable slot (GcRoot<String>) when mmapped to the .bss.
  uint32_t GetInternTableEntryOffset(const LinkerPatch& patch)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!writer_->HasBootImage());
    const uint8_t* string_root = writer_->LookupBootImageInternTableSlot(
        *patch.TargetStringDexFile(), patch.TargetStringIndex());
    DCHECK(string_root != nullptr);
    return GetBootImageTableEntryOffset(string_root);
  }

  // Calculate the offset of the ClassTable::TableSlot when mmapped to the .bss.
  uint32_t GetClassTableEntryOffset(const LinkerPatch& patch)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!writer_->HasBootImage());
    const uint8_t* table_slot =
        writer_->LookupBootImageClassTableSlot(*patch.TargetTypeDexFile(), patch.TargetTypeIndex());
    DCHECK(table_slot != nullptr);
    return GetBootImageTableEntryOffset(table_slot);
  }

  uint32_t GetBootImageTableEntryOffset(const uint8_t* raw_root) {
    uint32_t base_offset = writer_->bss_start_;
    for (gc::space::ImageSpace* space : Runtime::Current()->GetHeap()->GetBootImageSpaces()) {
      const uint8_t* const_tables_begin =
          space->Begin() + space->GetImageHeader().GetBootImageConstantTablesOffset();
      size_t offset = static_cast<size_t>(raw_root - const_tables_begin);
      if (offset < space->GetImageHeader().GetBootImageConstantTablesSize()) {
        DCHECK_LE(base_offset + offset, writer_->bss_start_ + writer_->bss_methods_offset_);
        return base_offset + offset;
      }
      base_offset += space->GetImageHeader().GetBootImageConstantTablesSize();
    }
    LOG(FATAL) << "Didn't find boot image string in boot image intern tables!";
    UNREACHABLE();
  }
};

class OatWriter::WriteMapMethodVisitor : public OatDexMethodVisitor {
 public:
  WriteMapMethodVisitor(OatWriter* writer,
                        OutputStream* out,
                        const size_t file_offset,
                        size_t relative_offset)
      : OatDexMethodVisitor(writer, relative_offset),
        out_(out),
        file_offset_(file_offset) {}

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) OVERRIDE {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    const CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (HasCompiledCode(compiled_method)) {
      size_t file_offset = file_offset_;
      OutputStream* out = out_;

      uint32_t map_offset = oat_class->method_headers_[method_offsets_index_].GetVmapTableOffset();
      uint32_t code_offset = oat_class->method_offsets_[method_offsets_index_].code_offset_;
      ++method_offsets_index_;

      DCHECK((compiled_method->GetVmapTable().size() == 0u && map_offset == 0u) ||
             (compiled_method->GetVmapTable().size() != 0u && map_offset != 0u))
          << compiled_method->GetVmapTable().size() << " " << map_offset << " "
          << dex_file_->PrettyMethod(it.GetMemberIndex());

      // If vdex is enabled, only emit the map for compiled code. The quickening info
      // is emitted in the vdex already.
      if (map_offset != 0u) {
        // Transform map_offset to actual oat data offset.
        map_offset = (code_offset - compiled_method->CodeDelta()) - map_offset;
        DCHECK_NE(map_offset, 0u);
        DCHECK_LE(map_offset, offset_) << dex_file_->PrettyMethod(it.GetMemberIndex());

        ArrayRef<const uint8_t> map = compiled_method->GetVmapTable();
        size_t map_size = map.size() * sizeof(map[0]);
        if (map_offset == offset_) {
          // Write deduplicated map (code info for Optimizing or transformation info for dex2dex).
          if (UNLIKELY(!out->WriteFully(map.data(), map_size))) {
            ReportWriteFailure(it);
            return false;
          }
          offset_ += map_size;
        }
      }
      DCHECK_OFFSET_();
    }

    return true;
  }

 private:
  OutputStream* const out_;
  size_t const file_offset_;

  void ReportWriteFailure(const ClassDataItemIterator& it) {
    PLOG(ERROR) << "Failed to write map for "
        << dex_file_->PrettyMethod(it.GetMemberIndex()) << " to " << out_->GetLocation();
  }
};

class OatWriter::WriteMethodInfoVisitor : public OatDexMethodVisitor {
 public:
  WriteMethodInfoVisitor(OatWriter* writer,
                         OutputStream* out,
                         const size_t file_offset,
                         size_t relative_offset)
      : OatDexMethodVisitor(writer, relative_offset),
        out_(out),
        file_offset_(file_offset) {}

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) OVERRIDE {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    const CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (HasCompiledCode(compiled_method)) {
      size_t file_offset = file_offset_;
      OutputStream* out = out_;
      uint32_t map_offset = oat_class->method_headers_[method_offsets_index_].GetMethodInfoOffset();
      uint32_t code_offset = oat_class->method_offsets_[method_offsets_index_].code_offset_;
      ++method_offsets_index_;
      DCHECK((compiled_method->GetMethodInfo().size() == 0u && map_offset == 0u) ||
             (compiled_method->GetMethodInfo().size() != 0u && map_offset != 0u))
          << compiled_method->GetMethodInfo().size() << " " << map_offset << " "
          << dex_file_->PrettyMethod(it.GetMemberIndex());
      if (map_offset != 0u) {
        // Transform map_offset to actual oat data offset.
        map_offset = (code_offset - compiled_method->CodeDelta()) - map_offset;
        DCHECK_NE(map_offset, 0u);
        DCHECK_LE(map_offset, offset_) << dex_file_->PrettyMethod(it.GetMemberIndex());

        ArrayRef<const uint8_t> map = compiled_method->GetMethodInfo();
        size_t map_size = map.size() * sizeof(map[0]);
        if (map_offset == offset_) {
          // Write deduplicated map (code info for Optimizing or transformation info for dex2dex).
          if (UNLIKELY(!out->WriteFully(map.data(), map_size))) {
            ReportWriteFailure(it);
            return false;
          }
          offset_ += map_size;
        }
      }
      DCHECK_OFFSET_();
    }

    return true;
  }

 private:
  OutputStream* const out_;
  size_t const file_offset_;

  void ReportWriteFailure(const ClassDataItemIterator& it) {
    PLOG(ERROR) << "Failed to write map for "
        << dex_file_->PrettyMethod(it.GetMemberIndex()) << " to " << out_->GetLocation();
  }
};

// Visit all methods from all classes in all dex files with the specified visitor.
bool OatWriter::VisitDexMethods(DexMethodVisitor* visitor) {
  for (const DexFile* dex_file : *dex_files_) {
    const size_t class_def_count = dex_file->NumClassDefs();
    for (size_t class_def_index = 0; class_def_index != class_def_count; ++class_def_index) {
      if (UNLIKELY(!visitor->StartClass(dex_file, class_def_index))) {
        return false;
      }
      if (MayHaveCompiledMethods()) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
        const uint8_t* class_data = dex_file->GetClassData(class_def);
        if (class_data != nullptr) {  // ie not an empty class, such as a marker interface
          ClassDataItemIterator it(*dex_file, class_data);
          it.SkipAllFields();
          size_t class_def_method_index = 0u;
          while (it.HasNextMethod()) {
            if (!visitor->VisitMethod(class_def_method_index, it)) {
              return false;
            }
            ++class_def_method_index;
            it.Next();
          }
          DCHECK(!it.HasNext());
        }
      }
      if (UNLIKELY(!visitor->EndClass())) {
        return false;
      }
    }
  }
  return true;
}

size_t OatWriter::InitOatHeader(InstructionSet instruction_set,
                                const InstructionSetFeatures* instruction_set_features,
                                uint32_t num_dex_files,
                                SafeMap<std::string, std::string>* key_value_store) {
  TimingLogger::ScopedTiming split("InitOatHeader", timings_);
  oat_header_.reset(OatHeader::Create(instruction_set,
                                      instruction_set_features,
                                      num_dex_files,
                                      key_value_store));
  size_oat_header_ += sizeof(OatHeader);
  size_oat_header_key_value_store_ += oat_header_->GetHeaderSize() - sizeof(OatHeader);
  return oat_header_->GetHeaderSize();
}

size_t OatWriter::InitClassOffsets(size_t offset) {
  // Reserve space for class offsets in OAT and update class_offsets_offset_.
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    DCHECK_EQ(oat_dex_file.class_offsets_offset_, 0u);
    if (!oat_dex_file.class_offsets_.empty()) {
      // Class offsets are required to be 4 byte aligned.
      offset = RoundUp(offset, 4u);
      oat_dex_file.class_offsets_offset_ = offset;
      offset += oat_dex_file.GetClassOffsetsRawSize();
      DCHECK_ALIGNED(offset, 4u);
    }
  }
  return offset;
}

size_t OatWriter::InitOatClasses(size_t offset) {
  // calculate the offsets within OatDexFiles to OatClasses
  InitOatClassesMethodVisitor visitor(this, offset);
  bool success = VisitDexMethods(&visitor);
  CHECK(success);
  offset = visitor.GetOffset();

  // Update oat_dex_files_.
  auto oat_class_it = oat_class_headers_.begin();
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    for (uint32_t& class_offset : oat_dex_file.class_offsets_) {
      DCHECK(oat_class_it != oat_class_headers_.end());
      class_offset = oat_class_it->offset_;
      ++oat_class_it;
    }
  }
  CHECK(oat_class_it == oat_class_headers_.end());

  return offset;
}

size_t OatWriter::InitOatMaps(size_t offset) {
  if (!MayHaveCompiledMethods()) {
    return offset;
  }
  {
    InitMapMethodVisitor visitor(this, offset);
    bool success = VisitDexMethods(&visitor);
    DCHECK(success);
    offset = visitor.GetOffset();
  }
  {
    InitMethodInfoVisitor visitor(this, offset);
    bool success = VisitDexMethods(&visitor);
    DCHECK(success);
    offset = visitor.GetOffset();
  }
  return offset;
}

template <typename GetBssOffset>
static size_t CalculateNumberOfIndexBssMappingEntries(size_t number_of_indexes,
                                                      size_t slot_size,
                                                      const BitVector& indexes,
                                                      GetBssOffset get_bss_offset) {
  IndexBssMappingEncoder encoder(number_of_indexes, slot_size);
  size_t number_of_entries = 0u;
  bool first_index = true;
  for (uint32_t index : indexes.Indexes()) {
    uint32_t bss_offset = get_bss_offset(index);
    if (first_index || !encoder.TryMerge(index, bss_offset)) {
      encoder.Reset(index, bss_offset);
      ++number_of_entries;
      first_index = false;
    }
  }
  DCHECK_NE(number_of_entries, 0u);
  return number_of_entries;
}

template <typename GetBssOffset>
static size_t CalculateIndexBssMappingSize(size_t number_of_indexes,
                                           size_t slot_size,
                                           const BitVector& indexes,
                                           GetBssOffset get_bss_offset) {
  size_t number_of_entries = CalculateNumberOfIndexBssMappingEntries(number_of_indexes,
                                                                     slot_size,
                                                                     indexes,
                                                                     get_bss_offset);
  return IndexBssMapping::ComputeSize(number_of_entries);
}

size_t OatWriter::InitIndexBssMappings(size_t offset) {
  if (bss_method_entry_references_.empty() &&
      bss_type_entry_references_.empty() &&
      bss_string_entry_references_.empty()) {
    return offset;
  }
  // If there are any classes, the class offsets allocation aligns the offset
  // and we cannot have any index bss mappings without class offsets.
  static_assert(alignof(IndexBssMapping) == 4u, "IndexBssMapping alignment check.");
  DCHECK_ALIGNED(offset, 4u);

  size_t number_of_method_dex_files = 0u;
  size_t number_of_type_dex_files = 0u;
  size_t number_of_string_dex_files = 0u;
  PointerSize pointer_size = GetInstructionSetPointerSize(oat_header_->GetInstructionSet());
  for (size_t i = 0, size = dex_files_->size(); i != size; ++i) {
    const DexFile* dex_file = (*dex_files_)[i];
    auto method_it = bss_method_entry_references_.find(dex_file);
    if (method_it != bss_method_entry_references_.end()) {
      const BitVector& method_indexes = method_it->second;
      ++number_of_method_dex_files;
      oat_dex_files_[i].method_bss_mapping_offset_ = offset;
      offset += CalculateIndexBssMappingSize(
          dex_file->NumMethodIds(),
          static_cast<size_t>(pointer_size),
          method_indexes,
          [=](uint32_t index) {
            return bss_method_entries_.Get({dex_file, index});
          });
    }

    auto type_it = bss_type_entry_references_.find(dex_file);
    if (type_it != bss_type_entry_references_.end()) {
      const BitVector& type_indexes = type_it->second;
      ++number_of_type_dex_files;
      oat_dex_files_[i].type_bss_mapping_offset_ = offset;
      offset += CalculateIndexBssMappingSize(
          dex_file->NumTypeIds(),
          sizeof(GcRoot<mirror::Class>),
          type_indexes,
          [=](uint32_t index) {
            return bss_type_entries_.Get({dex_file, dex::TypeIndex(index)});
          });
    }

    auto string_it = bss_string_entry_references_.find(dex_file);
    if (string_it != bss_string_entry_references_.end()) {
      const BitVector& string_indexes = string_it->second;
      ++number_of_string_dex_files;
      oat_dex_files_[i].string_bss_mapping_offset_ = offset;
      offset += CalculateIndexBssMappingSize(
          dex_file->NumStringIds(),
          sizeof(GcRoot<mirror::String>),
          string_indexes,
          [=](uint32_t index) {
            return bss_string_entries_.Get({dex_file, dex::StringIndex(index)});
          });
    }
  }
  // Check that all dex files targeted by bss entries are in `*dex_files_`.
  CHECK_EQ(number_of_method_dex_files, bss_method_entry_references_.size());
  CHECK_EQ(number_of_type_dex_files, bss_type_entry_references_.size());
  CHECK_EQ(number_of_string_dex_files, bss_string_entry_references_.size());
  return offset;
}

size_t OatWriter::InitOatDexFiles(size_t offset) {
  // Initialize offsets of oat dex files.
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    oat_dex_file.offset_ = offset;
    offset += oat_dex_file.SizeOf();
  }
  return offset;
}

size_t OatWriter::InitOatCode(size_t offset) {
  // calculate the offsets within OatHeader to executable code
  size_t old_offset = offset;
  // required to be on a new page boundary
  offset = RoundUp(offset, kPageSize);
  oat_header_->SetExecutableOffset(offset);
  size_executable_offset_alignment_ = offset - old_offset;
  // TODO: Remove unused trampoline offsets from the OatHeader (requires oat version change).
  oat_header_->SetInterpreterToInterpreterBridgeOffset(0);
  oat_header_->SetInterpreterToCompiledCodeBridgeOffset(0);
  if (compiler_driver_->GetCompilerOptions().IsBootImage()) {
    InstructionSet instruction_set = compiler_driver_->GetInstructionSet();
    const bool generate_debug_info = compiler_driver_->GetCompilerOptions().GenerateAnyDebugInfo();
    size_t adjusted_offset = offset;

    #define DO_TRAMPOLINE(field, fn_name)                                   \
      offset = CompiledCode::AlignCode(offset, instruction_set);            \
      adjusted_offset = offset + CompiledCode::CodeDelta(instruction_set);  \
      oat_header_->Set ## fn_name ## Offset(adjusted_offset);               \
      (field) = compiler_driver_->Create ## fn_name();                      \
      if (generate_debug_info) {                                            \
        debug::MethodDebugInfo info = {};                                   \
        info.custom_name = #fn_name;                                        \
        info.isa = instruction_set;                                         \
        info.is_code_address_text_relative = true;                          \
        /* Use the code offset rather than the `adjusted_offset`. */        \
        info.code_address = offset - oat_header_->GetExecutableOffset();    \
        info.code_size = (field)->size();                                   \
        method_info_.push_back(std::move(info));                            \
      }                                                                     \
      offset += (field)->size();

    DO_TRAMPOLINE(jni_dlsym_lookup_, JniDlsymLookup);
    DO_TRAMPOLINE(quick_generic_jni_trampoline_, QuickGenericJniTrampoline);
    DO_TRAMPOLINE(quick_imt_conflict_trampoline_, QuickImtConflictTrampoline);
    DO_TRAMPOLINE(quick_resolution_trampoline_, QuickResolutionTrampoline);
    DO_TRAMPOLINE(quick_to_interpreter_bridge_, QuickToInterpreterBridge);

    #undef DO_TRAMPOLINE
  } else {
    oat_header_->SetJniDlsymLookupOffset(0);
    oat_header_->SetQuickGenericJniTrampolineOffset(0);
    oat_header_->SetQuickImtConflictTrampolineOffset(0);
    oat_header_->SetQuickResolutionTrampolineOffset(0);
    oat_header_->SetQuickToInterpreterBridgeOffset(0);
  }
  return offset;
}

size_t OatWriter::InitOatCodeDexFiles(size_t offset) {
  if (!compiler_driver_->GetCompilerOptions().IsAnyCompilationEnabled()) {
    if (kOatWriterDebugOatCodeLayout) {
      LOG(INFO) << "InitOatCodeDexFiles: OatWriter("
                << this << "), "
                << "compilation is disabled";
    }

    return offset;
  }
  bool success = false;

  {
    ScopedObjectAccess soa(Thread::Current());

    LayoutCodeMethodVisitor layout_code_visitor(this, offset);
    success = VisitDexMethods(&layout_code_visitor);
    DCHECK(success);

    LayoutReserveOffsetCodeMethodVisitor layout_reserve_code_visitor(
        this,
        offset,
        layout_code_visitor.ReleaseOrderedMethods());
    success = layout_reserve_code_visitor.Visit();
    DCHECK(success);
    offset = layout_reserve_code_visitor.GetOffset();

    // Save the method order because the WriteCodeMethodVisitor will need this
    // order again.
    DCHECK(ordered_methods_ == nullptr);
    ordered_methods_.reset(
        new OrderedMethodList(
            layout_reserve_code_visitor.ReleaseOrderedMethods()));

    if (kOatWriterDebugOatCodeLayout) {
      LOG(INFO) << "IniatOatCodeDexFiles: method order: ";
      for (const OrderedMethodData& ordered_method : *ordered_methods_) {
        std::string pretty_name = ordered_method.method_reference.PrettyMethod();
        LOG(INFO) << pretty_name
                  << "@ offset "
                  << relative_patcher_->GetOffset(ordered_method.method_reference)
                  << " X hotness "
                  << reinterpret_cast<void*>(ordered_method.method_hotness.GetFlags());
      }
    }
  }

  if (HasImage()) {
    InitImageMethodVisitor image_visitor(this, offset, dex_files_);
    success = VisitDexMethods(&image_visitor);
    image_visitor.Postprocess();
    DCHECK(success);
    offset = image_visitor.GetOffset();
  }

  return offset;
}

void OatWriter::InitBssLayout(InstructionSet instruction_set) {
  {
    InitBssLayoutMethodVisitor visitor(this);
    bool success = VisitDexMethods(&visitor);
    DCHECK(success);
  }

  DCHECK_EQ(bss_size_, 0u);
  if (HasBootImage()) {
    DCHECK(!map_boot_image_tables_to_bss_);
    DCHECK(bss_string_entries_.empty());
  }
  if (!map_boot_image_tables_to_bss_ &&
      bss_method_entries_.empty() &&
      bss_type_entries_.empty() &&
      bss_string_entries_.empty()) {
    // Nothing to put to the .bss section.
    return;
  }

  // Allocate space for boot image tables in the .bss section.
  PointerSize pointer_size = GetInstructionSetPointerSize(instruction_set);
  if (map_boot_image_tables_to_bss_) {
    for (gc::space::ImageSpace* space : Runtime::Current()->GetHeap()->GetBootImageSpaces()) {
      bss_size_ += space->GetImageHeader().GetBootImageConstantTablesSize();
    }
  }

  bss_methods_offset_ = bss_size_;

  // Prepare offsets for .bss ArtMethod entries.
  for (auto& entry : bss_method_entries_) {
    DCHECK_EQ(entry.second, 0u);
    entry.second = bss_size_;
    bss_size_ += static_cast<size_t>(pointer_size);
  }

  bss_roots_offset_ = bss_size_;

  // Prepare offsets for .bss Class entries.
  for (auto& entry : bss_type_entries_) {
    DCHECK_EQ(entry.second, 0u);
    entry.second = bss_size_;
    bss_size_ += sizeof(GcRoot<mirror::Class>);
  }
  // Prepare offsets for .bss String entries.
  for (auto& entry : bss_string_entries_) {
    DCHECK_EQ(entry.second, 0u);
    entry.second = bss_size_;
    bss_size_ += sizeof(GcRoot<mirror::String>);
  }
}

bool OatWriter::WriteRodata(OutputStream* out) {
  CHECK(write_state_ == WriteState::kWriteRoData);

  size_t file_offset = oat_data_offset_;
  off_t current_offset = out->Seek(0, kSeekCurrent);
  if (current_offset == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to retrieve current position in " << out->GetLocation();
  }
  DCHECK_GE(static_cast<size_t>(current_offset), file_offset + oat_header_->GetHeaderSize());
  size_t relative_offset = current_offset - file_offset;

  // Wrap out to update checksum with each write.
  ChecksumUpdatingOutputStream checksum_updating_out(out, oat_header_.get());
  out = &checksum_updating_out;

  relative_offset = WriteClassOffsets(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    PLOG(ERROR) << "Failed to write class offsets to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteClasses(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    PLOG(ERROR) << "Failed to write classes to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteIndexBssMappings(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    PLOG(ERROR) << "Failed to write method bss mappings to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteMaps(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    PLOG(ERROR) << "Failed to write oat code to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteOatDexFiles(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    PLOG(ERROR) << "Failed to write oat dex information to " << out->GetLocation();
    return false;
  }

  // Write padding.
  off_t new_offset = out->Seek(size_executable_offset_alignment_, kSeekCurrent);
  relative_offset += size_executable_offset_alignment_;
  DCHECK_EQ(relative_offset, oat_header_->GetExecutableOffset());
  size_t expected_file_offset = file_offset + relative_offset;
  if (static_cast<uint32_t>(new_offset) != expected_file_offset) {
    PLOG(ERROR) << "Failed to seek to oat code section. Actual: " << new_offset
                << " Expected: " << expected_file_offset << " File: " << out->GetLocation();
    return 0;
  }
  DCHECK_OFFSET();

  write_state_ = WriteState::kWriteText;
  return true;
}

class OatWriter::WriteQuickeningInfoMethodVisitor {
 public:
  WriteQuickeningInfoMethodVisitor(OatWriter* writer, OutputStream* out)
      : writer_(writer),
        out_(out) {}

  bool VisitDexMethods(const std::vector<const DexFile*>& dex_files) {
    // Map of offsets for quicken info related to method indices.
    SafeMap<const uint8_t*, uint32_t> offset_map;
    // Use method index order to minimize the encoded size of the offset table.
    for (const DexFile* dex_file : dex_files) {
      std::vector<uint32_t>* const offsets =
          &quicken_info_offset_indices_.Put(dex_file, std::vector<uint32_t>())->second;
      for (uint32_t method_idx = 0; method_idx < dex_file->NumMethodIds(); ++method_idx) {
        uint32_t offset = 0u;
        MethodReference method_ref(dex_file, method_idx);
        CompiledMethod* compiled_method = writer_->compiler_driver_->GetCompiledMethod(method_ref);
        if (compiled_method != nullptr && HasQuickeningInfo(compiled_method)) {
          ArrayRef<const uint8_t> map = compiled_method->GetVmapTable();

          // Record each index if required. written_bytes_ is the offset from the start of the
          // quicken info data.
          // May be already inserted for deduplicate items.
          // Add offset of one to make sure 0 represents unused.
          auto pair = offset_map.emplace(map.data(), written_bytes_ + 1);
          offset = pair.first->second;
          // Write out the map if it's not already written.
          if (pair.second) {
            const uint32_t length = map.size() * sizeof(map.front());
            if (!out_->WriteFully(map.data(), length)) {
              PLOG(ERROR) << "Failed to write quickening info for " << method_ref.PrettyMethod()
                          << " to " << out_->GetLocation();
              return false;
            }
            written_bytes_ += length;
          }
        }
        offsets->push_back(offset);
      }
    }
    return true;
  }

  size_t GetNumberOfWrittenBytes() const {
    return written_bytes_;
  }

  SafeMap<const DexFile*, std::vector<uint32_t>>& GetQuickenInfoOffsetIndicies() {
    return quicken_info_offset_indices_;
  }

 private:
  OatWriter* const writer_;
  OutputStream* const out_;
  size_t written_bytes_ = 0u;
  SafeMap<const DexFile*, std::vector<uint32_t>> quicken_info_offset_indices_;
};

class OatWriter::WriteQuickeningInfoOffsetsMethodVisitor {
 public:
  WriteQuickeningInfoOffsetsMethodVisitor(
      OutputStream* out,
      uint32_t start_offset,
      SafeMap<const DexFile*, std::vector<uint32_t>>* quicken_info_offset_indices,
      std::vector<uint32_t>* out_table_offsets)
      : out_(out),
        start_offset_(start_offset),
        quicken_info_offset_indices_(quicken_info_offset_indices),
        out_table_offsets_(out_table_offsets) {}

  bool VisitDexMethods(const std::vector<const DexFile*>& dex_files) {
    for (const DexFile* dex_file : dex_files) {
      auto it = quicken_info_offset_indices_->find(dex_file);
      DCHECK(it != quicken_info_offset_indices_->end()) << "Failed to find dex file "
                                                        << dex_file->GetLocation();
      const std::vector<uint32_t>* const offsets = &it->second;

      const uint32_t current_offset = start_offset_ + written_bytes_;
      CHECK_ALIGNED_PARAM(current_offset, CompactOffsetTable::kAlignment);

      // Generate and write the data.
      std::vector<uint8_t> table_data;
      CompactOffsetTable::Build(*offsets, &table_data);

      // Store the offset since we need to put those after the dex file. Table offsets are relative
      // to the start of the quicken info section.
      out_table_offsets_->push_back(current_offset);

      const uint32_t length = table_data.size() * sizeof(table_data.front());
      if (!out_->WriteFully(table_data.data(), length)) {
        PLOG(ERROR) << "Failed to write quickening offset table for " << dex_file->GetLocation()
                    << " to " << out_->GetLocation();
        return false;
      }
      written_bytes_ += length;
    }
    return true;
  }

  size_t GetNumberOfWrittenBytes() const {
    return written_bytes_;
  }

 private:
  OutputStream* const out_;
  const uint32_t start_offset_;
  size_t written_bytes_ = 0u;
  // Maps containing the offsets for the tables.
  SafeMap<const DexFile*, std::vector<uint32_t>>* const quicken_info_offset_indices_;
  std::vector<uint32_t>* const out_table_offsets_;
};

bool OatWriter::WriteQuickeningInfo(OutputStream* vdex_out) {
  if (!extract_dex_files_into_vdex_) {
    // Nothing to write. Leave `vdex_size_` untouched and unaligned.
    vdex_quickening_info_offset_ = vdex_size_;
    size_quickening_info_alignment_ = 0;
    return true;
  }
  size_t initial_offset = vdex_size_;
  // Make sure the table is properly aligned.
  size_t start_offset = RoundUp(initial_offset, 4u);

  off_t actual_offset = vdex_out->Seek(start_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek to quickening info section. Actual: " << actual_offset
                << " Expected: " << start_offset
                << " Output: " << vdex_out->GetLocation();
    return false;
  }

  size_t current_offset = start_offset;
  if (compiler_driver_->GetCompilerOptions().IsQuickeningCompilationEnabled()) {
    std::vector<uint32_t> dex_files_indices;
    WriteQuickeningInfoMethodVisitor write_quicken_info_visitor(this, vdex_out);
    if (!write_quicken_info_visitor.VisitDexMethods(*dex_files_)) {
      PLOG(ERROR) << "Failed to write the vdex quickening info. File: " << vdex_out->GetLocation();
      return false;
    }

    uint32_t quicken_info_offset = write_quicken_info_visitor.GetNumberOfWrittenBytes();
    current_offset = current_offset + quicken_info_offset;
    uint32_t before_offset = current_offset;
    current_offset = RoundUp(current_offset, CompactOffsetTable::kAlignment);
    const size_t extra_bytes = current_offset - before_offset;
    quicken_info_offset += extra_bytes;
    actual_offset = vdex_out->Seek(current_offset, kSeekSet);
    if (actual_offset != static_cast<off_t>(current_offset)) {
      PLOG(ERROR) << "Failed to seek to quickening offset table section. Actual: " << actual_offset
                  << " Expected: " << current_offset
                  << " Output: " << vdex_out->GetLocation();
      return false;
    }

    std::vector<uint32_t> table_offsets;
    WriteQuickeningInfoOffsetsMethodVisitor table_visitor(
        vdex_out,
        quicken_info_offset,
        &write_quicken_info_visitor.GetQuickenInfoOffsetIndicies(),
        /*out*/ &table_offsets);
    if (!table_visitor.VisitDexMethods(*dex_files_)) {
      PLOG(ERROR) << "Failed to write the vdex quickening info. File: "
                  << vdex_out->GetLocation();
      return false;
    }

    CHECK_EQ(table_offsets.size(), dex_files_->size());

    current_offset += table_visitor.GetNumberOfWrittenBytes();

    // Store the offset table offset as a preheader for each dex.
    size_t index = 0;
    for (const OatDexFile& oat_dex_file : oat_dex_files_) {
      const off_t desired_offset = oat_dex_file.dex_file_offset_ -
          sizeof(VdexFile::QuickeningTableOffsetType);
      actual_offset = vdex_out->Seek(desired_offset, kSeekSet);
      if (actual_offset != desired_offset) {
        PLOG(ERROR) << "Failed to seek to before dex file for writing offset table offset: "
                    << actual_offset << " Expected: " << desired_offset
                    << " Output: " << vdex_out->GetLocation();
        return false;
      }
      uint32_t offset = table_offsets[index];
      if (!vdex_out->WriteFully(reinterpret_cast<const uint8_t*>(&offset), sizeof(offset))) {
        PLOG(ERROR) << "Failed to write verifier deps."
                    << " File: " << vdex_out->GetLocation();
        return false;
      }
      ++index;
    }
    if (!vdex_out->Flush()) {
      PLOG(ERROR) << "Failed to flush stream after writing quickening info."
                  << " File: " << vdex_out->GetLocation();
      return false;
    }
    size_quickening_info_ = current_offset - start_offset;
  } else {
    // We know we did not quicken.
    size_quickening_info_ = 0;
  }

  if (size_quickening_info_ == 0) {
    // Nothing was written. Leave `vdex_size_` untouched and unaligned.
    vdex_quickening_info_offset_ = initial_offset;
    size_quickening_info_alignment_ = 0;
  } else {
    vdex_size_ = start_offset + size_quickening_info_;
    vdex_quickening_info_offset_ = start_offset;
    size_quickening_info_alignment_ = start_offset - initial_offset;
  }

  return true;
}

bool OatWriter::WriteVerifierDeps(OutputStream* vdex_out, verifier::VerifierDeps* verifier_deps) {
  if (verifier_deps == nullptr) {
    // Nothing to write. Record the offset, but no need
    // for alignment.
    vdex_verifier_deps_offset_ = vdex_size_;
    return true;
  }

  size_t initial_offset = vdex_size_;
  size_t start_offset = RoundUp(initial_offset, 4u);

  vdex_size_ = start_offset;
  vdex_verifier_deps_offset_ = vdex_size_;
  size_verifier_deps_alignment_ = start_offset - initial_offset;

  off_t actual_offset = vdex_out->Seek(start_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek to verifier deps section. Actual: " << actual_offset
                << " Expected: " << start_offset
                << " Output: " << vdex_out->GetLocation();
    return false;
  }

  std::vector<uint8_t> buffer;
  verifier_deps->Encode(*dex_files_, &buffer);

  if (!vdex_out->WriteFully(buffer.data(), buffer.size())) {
    PLOG(ERROR) << "Failed to write verifier deps."
                << " File: " << vdex_out->GetLocation();
    return false;
  }
  if (!vdex_out->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after writing verifier deps."
                << " File: " << vdex_out->GetLocation();
    return false;
  }

  size_verifier_deps_ = buffer.size();
  vdex_size_ += size_verifier_deps_;
  return true;
}

bool OatWriter::WriteCode(OutputStream* out) {
  CHECK(write_state_ == WriteState::kWriteText);

  // Wrap out to update checksum with each write.
  ChecksumUpdatingOutputStream checksum_updating_out(out, oat_header_.get());
  out = &checksum_updating_out;

  SetMultiOatRelativePatcherAdjustment();

  const size_t file_offset = oat_data_offset_;
  size_t relative_offset = oat_header_->GetExecutableOffset();
  DCHECK_OFFSET();

  relative_offset = WriteCode(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    LOG(ERROR) << "Failed to write oat code to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteCodeDexFiles(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    LOG(ERROR) << "Failed to write oat code for dex files to " << out->GetLocation();
    return false;
  }

  const off_t oat_end_file_offset = out->Seek(0, kSeekCurrent);
  if (oat_end_file_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to get oat end file offset in " << out->GetLocation();
    return false;
  }

  if (kIsDebugBuild) {
    uint32_t size_total = 0;
    #define DO_STAT(x) \
      VLOG(compiler) << #x "=" << PrettySize(x) << " (" << (x) << "B)"; \
      size_total += (x);

    DO_STAT(size_vdex_header_);
    DO_STAT(size_vdex_checksums_);
    DO_STAT(size_dex_file_alignment_);
    DO_STAT(size_executable_offset_alignment_);
    DO_STAT(size_oat_header_);
    DO_STAT(size_oat_header_key_value_store_);
    DO_STAT(size_dex_file_);
    DO_STAT(size_verifier_deps_);
    DO_STAT(size_verifier_deps_alignment_);
    DO_STAT(size_quickening_info_);
    DO_STAT(size_quickening_info_alignment_);
    DO_STAT(size_interpreter_to_interpreter_bridge_);
    DO_STAT(size_interpreter_to_compiled_code_bridge_);
    DO_STAT(size_jni_dlsym_lookup_);
    DO_STAT(size_quick_generic_jni_trampoline_);
    DO_STAT(size_quick_imt_conflict_trampoline_);
    DO_STAT(size_quick_resolution_trampoline_);
    DO_STAT(size_quick_to_interpreter_bridge_);
    DO_STAT(size_trampoline_alignment_);
    DO_STAT(size_method_header_);
    DO_STAT(size_code_);
    DO_STAT(size_code_alignment_);
    DO_STAT(size_relative_call_thunks_);
    DO_STAT(size_misc_thunks_);
    DO_STAT(size_vmap_table_);
    DO_STAT(size_method_info_);
    DO_STAT(size_oat_dex_file_location_size_);
    DO_STAT(size_oat_dex_file_location_data_);
    DO_STAT(size_oat_dex_file_location_checksum_);
    DO_STAT(size_oat_dex_file_offset_);
    DO_STAT(size_oat_dex_file_class_offsets_offset_);
    DO_STAT(size_oat_dex_file_lookup_table_offset_);
    DO_STAT(size_oat_dex_file_dex_layout_sections_offset_);
    DO_STAT(size_oat_dex_file_dex_layout_sections_);
    DO_STAT(size_oat_dex_file_dex_layout_sections_alignment_);
    DO_STAT(size_oat_dex_file_method_bss_mapping_offset_);
    DO_STAT(size_oat_dex_file_type_bss_mapping_offset_);
    DO_STAT(size_oat_dex_file_string_bss_mapping_offset_);
    DO_STAT(size_oat_lookup_table_alignment_);
    DO_STAT(size_oat_lookup_table_);
    DO_STAT(size_oat_class_offsets_alignment_);
    DO_STAT(size_oat_class_offsets_);
    DO_STAT(size_oat_class_type_);
    DO_STAT(size_oat_class_status_);
    DO_STAT(size_oat_class_method_bitmaps_);
    DO_STAT(size_oat_class_method_offsets_);
    DO_STAT(size_method_bss_mappings_);
    DO_STAT(size_type_bss_mappings_);
    DO_STAT(size_string_bss_mappings_);
    #undef DO_STAT

    VLOG(compiler) << "size_total=" << PrettySize(size_total) << " (" << size_total << "B)";

    CHECK_EQ(vdex_size_ + oat_size_, size_total);
    CHECK_EQ(file_offset + size_total - vdex_size_, static_cast<size_t>(oat_end_file_offset));
  }

  CHECK_EQ(file_offset + oat_size_, static_cast<size_t>(oat_end_file_offset));
  CHECK_EQ(oat_size_, relative_offset);

  write_state_ = WriteState::kWriteHeader;
  return true;
}

bool OatWriter::WriteHeader(OutputStream* out,
                            uint32_t image_file_location_oat_checksum,
                            uintptr_t image_file_location_oat_begin,
                            int32_t image_patch_delta) {
  CHECK(write_state_ == WriteState::kWriteHeader);

  oat_header_->SetImageFileLocationOatChecksum(image_file_location_oat_checksum);
  oat_header_->SetImageFileLocationOatDataBegin(image_file_location_oat_begin);
  if (compiler_driver_->GetCompilerOptions().IsBootImage()) {
    CHECK_EQ(image_patch_delta, 0);
    CHECK_EQ(oat_header_->GetImagePatchDelta(), 0);
  } else {
    CHECK_ALIGNED(image_patch_delta, kPageSize);
    oat_header_->SetImagePatchDelta(image_patch_delta);
  }
  oat_header_->UpdateChecksumWithHeaderData();

  const size_t file_offset = oat_data_offset_;

  off_t current_offset = out->Seek(0, kSeekCurrent);
  if (current_offset == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to get current offset from " << out->GetLocation();
    return false;
  }
  if (out->Seek(file_offset, kSeekSet) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to seek to oat header position in " << out->GetLocation();
    return false;
  }
  DCHECK_EQ(file_offset, static_cast<size_t>(out->Seek(0, kSeekCurrent)));

  // Flush all other data before writing the header.
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush before writing oat header to " << out->GetLocation();
    return false;
  }
  // Write the header.
  size_t header_size = oat_header_->GetHeaderSize();
  if (!out->WriteFully(oat_header_.get(), header_size)) {
    PLOG(ERROR) << "Failed to write oat header to " << out->GetLocation();
    return false;
  }
  // Flush the header data.
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush after writing oat header to " << out->GetLocation();
    return false;
  }

  if (out->Seek(current_offset, kSeekSet) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to seek back after writing oat header to " << out->GetLocation();
    return false;
  }
  DCHECK_EQ(current_offset, out->Seek(0, kSeekCurrent));

  write_state_ = WriteState::kDone;
  return true;
}

size_t OatWriter::WriteClassOffsets(OutputStream* out, size_t file_offset, size_t relative_offset) {
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    if (oat_dex_file.class_offsets_offset_ != 0u) {
      // Class offsets are required to be 4 byte aligned.
      if (UNLIKELY(!IsAligned<4u>(relative_offset))) {
        size_t padding_size =  RoundUp(relative_offset, 4u) - relative_offset;
        if (!WriteUpTo16BytesAlignment(out, padding_size, &size_oat_class_offsets_alignment_)) {
          return 0u;
        }
        relative_offset += padding_size;
      }
      DCHECK_OFFSET();
      if (!oat_dex_file.WriteClassOffsets(this, out)) {
        return 0u;
      }
      relative_offset += oat_dex_file.GetClassOffsetsRawSize();
    }
  }
  return relative_offset;
}

size_t OatWriter::WriteClasses(OutputStream* out, size_t file_offset, size_t relative_offset) {
  const bool may_have_compiled = MayHaveCompiledMethods();
  if (may_have_compiled) {
    CHECK_EQ(oat_class_headers_.size(), oat_classes_.size());
  }
  for (size_t i = 0; i < oat_class_headers_.size(); ++i) {
    // If there are any classes, the class offsets allocation aligns the offset.
    DCHECK_ALIGNED(relative_offset, 4u);
    DCHECK_OFFSET();
    if (!oat_class_headers_[i].Write(this, out, oat_data_offset_)) {
      return 0u;
    }
    relative_offset += oat_class_headers_[i].SizeOf();
    if (may_have_compiled) {
      if (!oat_classes_[i].Write(this, out)) {
        return 0u;
      }
      relative_offset += oat_classes_[i].SizeOf();
    }
  }
  return relative_offset;
}

size_t OatWriter::WriteMaps(OutputStream* out, size_t file_offset, size_t relative_offset) {
  {
    size_t vmap_tables_offset = relative_offset;
    WriteMapMethodVisitor visitor(this, out, file_offset, relative_offset);
    if (UNLIKELY(!VisitDexMethods(&visitor))) {
      return 0;
    }
    relative_offset = visitor.GetOffset();
    size_vmap_table_ = relative_offset - vmap_tables_offset;
  }
  {
    size_t method_infos_offset = relative_offset;
    WriteMethodInfoVisitor visitor(this, out, file_offset, relative_offset);
    if (UNLIKELY(!VisitDexMethods(&visitor))) {
      return 0;
    }
    relative_offset = visitor.GetOffset();
    size_method_info_ = relative_offset - method_infos_offset;
  }

  return relative_offset;
}


template <typename GetBssOffset>
size_t WriteIndexBssMapping(OutputStream* out,
                            size_t number_of_indexes,
                            size_t slot_size,
                            const BitVector& indexes,
                            GetBssOffset get_bss_offset) {
  // Allocate the IndexBssMapping.
  size_t number_of_entries = CalculateNumberOfIndexBssMappingEntries(
      number_of_indexes, slot_size, indexes, get_bss_offset);
  size_t mappings_size = IndexBssMapping::ComputeSize(number_of_entries);
  DCHECK_ALIGNED(mappings_size, sizeof(uint32_t));
  std::unique_ptr<uint32_t[]> storage(new uint32_t[mappings_size / sizeof(uint32_t)]);
  IndexBssMapping* mappings = new(storage.get()) IndexBssMapping(number_of_entries);
  mappings->ClearPadding();
  // Encode the IndexBssMapping.
  IndexBssMappingEncoder encoder(number_of_indexes, slot_size);
  auto init_it = mappings->begin();
  bool first_index = true;
  for (uint32_t index : indexes.Indexes()) {
    size_t bss_offset = get_bss_offset(index);
    if (first_index) {
      first_index = false;
      encoder.Reset(index, bss_offset);
    } else if (!encoder.TryMerge(index, bss_offset)) {
      *init_it = encoder.GetEntry();
      ++init_it;
      encoder.Reset(index, bss_offset);
    }
  }
  // Store the last entry.
  *init_it = encoder.GetEntry();
  ++init_it;
  DCHECK(init_it == mappings->end());

  if (!out->WriteFully(storage.get(), mappings_size)) {
    return 0u;
  }
  return mappings_size;
}

size_t OatWriter::WriteIndexBssMappings(OutputStream* out,
                                        size_t file_offset,
                                        size_t relative_offset) {
  TimingLogger::ScopedTiming split("WriteMethodBssMappings", timings_);
  if (bss_method_entry_references_.empty() &&
      bss_type_entry_references_.empty() &&
      bss_string_entry_references_.empty()) {
    return relative_offset;
  }
  // If there are any classes, the class offsets allocation aligns the offset
  // and we cannot have method bss mappings without class offsets.
  static_assert(alignof(IndexBssMapping) == sizeof(uint32_t),
                "IndexBssMapping alignment check.");
  DCHECK_ALIGNED(relative_offset, sizeof(uint32_t));

  PointerSize pointer_size = GetInstructionSetPointerSize(oat_header_->GetInstructionSet());
  for (size_t i = 0, size = dex_files_->size(); i != size; ++i) {
    const DexFile* dex_file = (*dex_files_)[i];
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    auto method_it = bss_method_entry_references_.find(dex_file);
    if (method_it != bss_method_entry_references_.end()) {
      const BitVector& method_indexes = method_it->second;
      DCHECK_EQ(relative_offset, oat_dex_file->method_bss_mapping_offset_);
      DCHECK_OFFSET();
      size_t method_mappings_size = WriteIndexBssMapping(
          out,
          dex_file->NumMethodIds(),
          static_cast<size_t>(pointer_size),
          method_indexes,
          [=](uint32_t index) {
            return bss_method_entries_.Get({dex_file, index});
          });
      if (method_mappings_size == 0u) {
        return 0u;
      }
      size_method_bss_mappings_ += method_mappings_size;
      relative_offset += method_mappings_size;
    } else {
      DCHECK_EQ(0u, oat_dex_file->method_bss_mapping_offset_);
    }

    auto type_it = bss_type_entry_references_.find(dex_file);
    if (type_it != bss_type_entry_references_.end()) {
      const BitVector& type_indexes = type_it->second;
      DCHECK_EQ(relative_offset, oat_dex_file->type_bss_mapping_offset_);
      DCHECK_OFFSET();
      size_t type_mappings_size = WriteIndexBssMapping(
          out,
          dex_file->NumTypeIds(),
          sizeof(GcRoot<mirror::Class>),
          type_indexes,
          [=](uint32_t index) {
            return bss_type_entries_.Get({dex_file, dex::TypeIndex(index)});
          });
      if (type_mappings_size == 0u) {
        return 0u;
      }
      size_type_bss_mappings_ += type_mappings_size;
      relative_offset += type_mappings_size;
    } else {
      DCHECK_EQ(0u, oat_dex_file->type_bss_mapping_offset_);
    }

    auto string_it = bss_string_entry_references_.find(dex_file);
    if (string_it != bss_string_entry_references_.end()) {
      const BitVector& string_indexes = string_it->second;
      DCHECK_EQ(relative_offset, oat_dex_file->string_bss_mapping_offset_);
      DCHECK_OFFSET();
      size_t string_mappings_size = WriteIndexBssMapping(
          out,
          dex_file->NumStringIds(),
          sizeof(GcRoot<mirror::String>),
          string_indexes,
          [=](uint32_t index) {
            return bss_string_entries_.Get({dex_file, dex::StringIndex(index)});
          });
      if (string_mappings_size == 0u) {
        return 0u;
      }
      size_string_bss_mappings_ += string_mappings_size;
      relative_offset += string_mappings_size;
    } else {
      DCHECK_EQ(0u, oat_dex_file->string_bss_mapping_offset_);
    }
  }
  return relative_offset;
}

size_t OatWriter::WriteOatDexFiles(OutputStream* out, size_t file_offset, size_t relative_offset) {
  TimingLogger::ScopedTiming split("WriteOatDexFiles", timings_);

  for (size_t i = 0, size = oat_dex_files_.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    DCHECK_EQ(relative_offset, oat_dex_file->offset_);
    DCHECK_OFFSET();

    // Write OatDexFile.
    if (!oat_dex_file->Write(this, out)) {
      return 0u;
    }
    relative_offset += oat_dex_file->SizeOf();
  }

  return relative_offset;
}

size_t OatWriter::WriteCode(OutputStream* out, size_t file_offset, size_t relative_offset) {
  if (compiler_driver_->GetCompilerOptions().IsBootImage()) {
    InstructionSet instruction_set = compiler_driver_->GetInstructionSet();

    #define DO_TRAMPOLINE(field) \
      do { \
        uint32_t aligned_offset = CompiledCode::AlignCode(relative_offset, instruction_set); \
        uint32_t alignment_padding = aligned_offset - relative_offset; \
        out->Seek(alignment_padding, kSeekCurrent); \
        size_trampoline_alignment_ += alignment_padding; \
        if (!out->WriteFully((field)->data(), (field)->size())) { \
          PLOG(ERROR) << "Failed to write " # field " to " << out->GetLocation(); \
          return false; \
        } \
        size_ ## field += (field)->size(); \
        relative_offset += alignment_padding + (field)->size(); \
        DCHECK_OFFSET(); \
      } while (false)

    DO_TRAMPOLINE(jni_dlsym_lookup_);
    DO_TRAMPOLINE(quick_generic_jni_trampoline_);
    DO_TRAMPOLINE(quick_imt_conflict_trampoline_);
    DO_TRAMPOLINE(quick_resolution_trampoline_);
    DO_TRAMPOLINE(quick_to_interpreter_bridge_);
    #undef DO_TRAMPOLINE
  }
  return relative_offset;
}

size_t OatWriter::WriteCodeDexFiles(OutputStream* out,
                                    size_t file_offset,
                                    size_t relative_offset) {
  if (!compiler_driver_->GetCompilerOptions().IsAnyCompilationEnabled()) {
    // As with InitOatCodeDexFiles, also skip the writer if
    // compilation was disabled.
    if (kOatWriterDebugOatCodeLayout) {
      LOG(INFO) << "WriteCodeDexFiles: OatWriter("
                << this << "), "
                << "compilation is disabled";
    }

    return relative_offset;
  }
  ScopedObjectAccess soa(Thread::Current());
  DCHECK(ordered_methods_ != nullptr);
  std::unique_ptr<OrderedMethodList> ordered_methods_ptr =
      std::move(ordered_methods_);
  WriteCodeMethodVisitor visitor(this,
                                 out,
                                 file_offset,
                                 relative_offset,
                                 std::move(*ordered_methods_ptr));
  if (UNLIKELY(!visitor.Visit())) {
    return 0;
  }
  relative_offset = visitor.GetOffset();

  size_code_alignment_ += relative_patcher_->CodeAlignmentSize();
  size_relative_call_thunks_ += relative_patcher_->RelativeCallThunksSize();
  size_misc_thunks_ += relative_patcher_->MiscThunksSize();

  return relative_offset;
}

bool OatWriter::RecordOatDataOffset(OutputStream* out) {
  // Get the elf file offset of the oat file.
  const off_t raw_file_offset = out->Seek(0, kSeekCurrent);
  if (raw_file_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to get file offset in " << out->GetLocation();
    return false;
  }
  oat_data_offset_ = static_cast<size_t>(raw_file_offset);
  return true;
}

bool OatWriter::WriteDexFiles(OutputStream* out,
                              File* file,
                              bool update_input_vdex,
                              CopyOption copy_dex_files) {
  TimingLogger::ScopedTiming split("Write Dex files", timings_);

  // If extraction is enabled, only do it if not all the dex files are aligned and uncompressed.
  if (copy_dex_files == CopyOption::kOnlyIfCompressed) {
    extract_dex_files_into_vdex_ = false;
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
      if (!oat_dex_file.source_.IsZipEntry()) {
        extract_dex_files_into_vdex_ = true;
        break;
      }
      ZipEntry* entry = oat_dex_file.source_.GetZipEntry();
      if (!entry->IsUncompressed() || !entry->IsAlignedToDexHeader()) {
        extract_dex_files_into_vdex_ = true;
        break;
      }
    }
  } else if (copy_dex_files == CopyOption::kAlways) {
    extract_dex_files_into_vdex_ = true;
  } else {
    DCHECK(copy_dex_files == CopyOption::kNever);
    extract_dex_files_into_vdex_ = false;
  }

  if (extract_dex_files_into_vdex_) {
    // Add the dex section header.
    vdex_size_ += sizeof(VdexFile::DexSectionHeader);
    vdex_dex_files_offset_ = vdex_size_;
    // Write dex files.
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
      if (!WriteDexFile(out, file, &oat_dex_file, update_input_vdex)) {
        return false;
      }
    }

    // Write shared dex file data section and fix up the dex file headers.
    vdex_dex_shared_data_offset_ = vdex_size_;
    uint32_t shared_data_size = 0u;

    if (dex_container_ != nullptr) {
      CHECK(!update_input_vdex) << "Update input vdex should have empty dex container";
      DexContainer::Section* const section = dex_container_->GetDataSection();
      if (section->Size() > 0) {
        CHECK(compact_dex_level_ != CompactDexLevel::kCompactDexLevelNone);
        const off_t existing_offset = out->Seek(0, kSeekCurrent);
        if (static_cast<uint32_t>(existing_offset) != vdex_dex_shared_data_offset_) {
          PLOG(ERROR) << "Expected offset " << vdex_dex_shared_data_offset_ << " but got "
                      << existing_offset;
          return false;
        }
        shared_data_size = section->Size();
        if (!out->WriteFully(section->Begin(), shared_data_size)) {
          PLOG(ERROR) << "Failed to write shared data!";
          return false;
        }
        if (!out->Flush()) {
          PLOG(ERROR) << "Failed to flush after writing shared dex section.";
          return false;
        }
        // Fix up the dex headers to have correct offsets to the data section.
        for (OatDexFile& oat_dex_file : oat_dex_files_) {
          // Overwrite the header by reading it, updating the offset, and writing it back out.
          DexFile::Header header;
          if (!file->PreadFully(&header, sizeof(header), oat_dex_file.dex_file_offset_)) {
            PLOG(ERROR) << "Failed to read dex header for updating";
            return false;
          }
          if (!CompactDexFile::IsMagicValid(header.magic_)) {
            // Non-compact dex file, probably failed to convert due to duplicate methods.
            continue;
          }
          CHECK_GT(vdex_dex_shared_data_offset_, oat_dex_file.dex_file_offset_);
          // Offset is from the dex file base.
          header.data_off_ = vdex_dex_shared_data_offset_ - oat_dex_file.dex_file_offset_;
          // The size should already be what part of the data buffer may be used by the dex.
          CHECK_LE(header.data_size_, shared_data_size);
          if (!file->PwriteFully(&header, sizeof(header), oat_dex_file.dex_file_offset_)) {
            PLOG(ERROR) << "Failed to write dex header for updating";
            return false;
          }
        }
        section->Clear();
      }
      dex_container_.reset();
    } else {
      const uint8_t* data_begin = nullptr;
      for (OatDexFile& oat_dex_file : oat_dex_files_) {
        DexFile::Header header;
        if (!file->PreadFully(&header, sizeof(header), oat_dex_file.dex_file_offset_)) {
          PLOG(ERROR) << "Failed to read dex header";
          return false;
        }
        if (!CompactDexFile::IsMagicValid(header.magic_)) {
          // Non compact dex does not have shared data section.
          continue;
        }
        const uint32_t expected_data_off = vdex_dex_shared_data_offset_ -
            oat_dex_file.dex_file_offset_;
        if (header.data_off_ != expected_data_off) {
          PLOG(ERROR) << "Shared data section offset " << header.data_off_
                      << " does not match expected value " << expected_data_off;
          return false;
        }
        if (oat_dex_file.source_.IsRawData()) {
          // Figure out the start of the shared data section so we can copy it below.
          const uint8_t* cur_data_begin = oat_dex_file.source_.GetRawData() + header.data_off_;
          if (data_begin != nullptr) {
            CHECK_EQ(data_begin, cur_data_begin);
          }
          data_begin = cur_data_begin;
        }
        // The different dex files currently can have different data sizes since
        // the dex writer writes them one at a time into the shared section.:w
        shared_data_size = std::max(shared_data_size, header.data_size_);
      }
      // If we are not updating the input vdex, write out the shared data section.
      if (!update_input_vdex) {
        const off_t existing_offset = out->Seek(0, kSeekCurrent);
        if (static_cast<uint32_t>(existing_offset) != vdex_dex_shared_data_offset_) {
          PLOG(ERROR) << "Expected offset " << vdex_dex_shared_data_offset_ << " but got "
                      << existing_offset;
          return false;
        }
        if (!out->WriteFully(data_begin, shared_data_size)) {
          PLOG(ERROR) << "Failed to write shared data!";
          return false;
        }
        if (!out->Flush()) {
          PLOG(ERROR) << "Failed to flush after writing shared dex section.";
          return false;
        }
      }
    }
    vdex_size_ += shared_data_size;
    size_dex_file_ += shared_data_size;
  } else {
    vdex_dex_shared_data_offset_ = vdex_size_;
  }

  return true;
}

void OatWriter::CloseSources() {
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    oat_dex_file.source_.Clear();  // Get rid of the reference, it's about to be invalidated.
  }
  zipped_dex_files_.clear();
  zip_archives_.clear();
  raw_dex_files_.clear();
}

bool OatWriter::WriteDexFile(OutputStream* out,
                             File* file,
                             OatDexFile* oat_dex_file,
                             bool update_input_vdex) {
  if (!SeekToDexFile(out, file, oat_dex_file)) {
    return false;
  }
  // update_input_vdex disables compact dex and layout.
  if (profile_compilation_info_ != nullptr ||
      compact_dex_level_ != CompactDexLevel::kCompactDexLevelNone) {
    CHECK(!update_input_vdex)
        << "We should never update the input vdex when doing dexlayout or compact dex";
    if (!LayoutAndWriteDexFile(out, oat_dex_file)) {
      return false;
    }
  } else if (oat_dex_file->source_.IsZipEntry()) {
    DCHECK(!update_input_vdex);
    if (!WriteDexFile(out, file, oat_dex_file, oat_dex_file->source_.GetZipEntry())) {
      return false;
    }
  } else if (oat_dex_file->source_.IsRawFile()) {
    DCHECK(!update_input_vdex);
    if (!WriteDexFile(out, file, oat_dex_file, oat_dex_file->source_.GetRawFile())) {
      return false;
    }
  } else {
    DCHECK(oat_dex_file->source_.IsRawData());
    if (!WriteDexFile(out, oat_dex_file, oat_dex_file->source_.GetRawData(), update_input_vdex)) {
      return false;
    }
  }

  // Update current size and account for the written data.
  DCHECK_EQ(vdex_size_, oat_dex_file->dex_file_offset_);
  vdex_size_ += oat_dex_file->dex_file_size_;
  size_dex_file_ += oat_dex_file->dex_file_size_;
  return true;
}

bool OatWriter::SeekToDexFile(OutputStream* out, File* file, OatDexFile* oat_dex_file) {
  // Dex files are required to be 4 byte aligned.
  size_t initial_offset = vdex_size_;
  size_t start_offset = RoundUp(initial_offset, 4);
  size_dex_file_alignment_ += start_offset - initial_offset;

  // Leave extra room for the quicken offset table offset.
  start_offset += sizeof(VdexFile::QuickeningTableOffsetType);
  // TODO: Not count the offset as part of alignment.
  size_dex_file_alignment_ += sizeof(VdexFile::QuickeningTableOffsetType);

  size_t file_offset = start_offset;

  // Seek to the start of the dex file and flush any pending operations in the stream.
  // Verify that, after flushing the stream, the file is at the same offset as the stream.
  off_t actual_offset = out->Seek(file_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(file_offset)) {
    PLOG(ERROR) << "Failed to seek to dex file section. Actual: " << actual_offset
                << " Expected: " << file_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush before writing dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = lseek(file->Fd(), 0, SEEK_CUR);
  if (actual_offset != static_cast<off_t>(file_offset)) {
    PLOG(ERROR) << "Stream/file position mismatch! Actual: " << actual_offset
                << " Expected: " << file_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  vdex_size_ = start_offset;
  oat_dex_file->dex_file_offset_ = start_offset;
  return true;
}

bool OatWriter::LayoutAndWriteDexFile(OutputStream* out, OatDexFile* oat_dex_file) {
  // Open dex files and write them into `out`.
  // Note that we only verify dex files which do not belong to the boot class path.
  // This is because those have been processed by `hiddenapi` and would not pass
  // some of the checks. No guarantees are lost, however, as `hiddenapi` verifies
  // the dex files prior to processing.
  TimingLogger::ScopedTiming split("Dex Layout", timings_);
  std::string error_msg;
  std::string location(oat_dex_file->GetLocation());
  std::unique_ptr<const DexFile> dex_file;
  const ArtDexFileLoader dex_file_loader;
  if (oat_dex_file->source_.IsZipEntry()) {
    ZipEntry* zip_entry = oat_dex_file->source_.GetZipEntry();
    std::unique_ptr<MemMap> mem_map(
        zip_entry->ExtractToMemMap(location.c_str(), "classes.dex", &error_msg));
    if (mem_map == nullptr) {
      LOG(ERROR) << "Failed to extract dex file to mem map for layout: " << error_msg;
      return false;
    }
    dex_file = dex_file_loader.Open(location,
                               zip_entry->GetCrc32(),
                               std::move(mem_map),
                               /* verify */ !compiling_boot_image_,
                               /* verify_checksum */ true,
                               &error_msg);
  } else if (oat_dex_file->source_.IsRawFile()) {
    File* raw_file = oat_dex_file->source_.GetRawFile();
    int dup_fd = dup(raw_file->Fd());
    if (dup_fd < 0) {
      PLOG(ERROR) << "Failed to dup dex file descriptor (" << raw_file->Fd() << ") at " << location;
      return false;
    }
    dex_file = dex_file_loader.OpenDex(dup_fd, location,
                                       /* verify */ !compiling_boot_image_,
                                       /* verify_checksum */ true,
                                       /* mmap_shared */ false,
                                       &error_msg);
  } else {
    // The source data is a vdex file.
    CHECK(oat_dex_file->source_.IsRawData())
        << static_cast<size_t>(oat_dex_file->source_.GetType());
    const uint8_t* raw_dex_file = oat_dex_file->source_.GetRawData();
    // Note: The raw data has already been checked to contain the header
    // and all the data that the header specifies as the file size.
    DCHECK(raw_dex_file != nullptr);
    DCHECK(ValidateDexFileHeader(raw_dex_file, oat_dex_file->GetLocation()));
    const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(raw_dex_file);
    // Since the source may have had its layout changed, or may be quickened, don't verify it.
    dex_file = dex_file_loader.Open(raw_dex_file,
                                    header->file_size_,
                                    location,
                                    oat_dex_file->dex_file_location_checksum_,
                                    nullptr,
                                    /* verify */ false,
                                    /* verify_checksum */ false,
                                    &error_msg);
  }
  if (dex_file == nullptr) {
    LOG(ERROR) << "Failed to open dex file for layout: " << error_msg;
    return false;
  }
  Options options;
  options.compact_dex_level_ = compact_dex_level_;
  options.update_checksum_ = true;
  DexLayout dex_layout(options, profile_compilation_info_, /*file*/ nullptr, /*header*/ nullptr);
  const uint8_t* dex_src = nullptr;
  if (dex_layout.ProcessDexFile(location.c_str(), dex_file.get(), 0, &dex_container_, &error_msg)) {
    oat_dex_file->dex_sections_layout_ = dex_layout.GetSections();
    // Dex layout can affect the size of the dex file, so we update here what we have set
    // when adding the dex file as a source.
    const UnalignedDexFileHeader* header =
        AsUnalignedDexFileHeader(dex_container_->GetMainSection()->Begin());
    oat_dex_file->dex_file_size_ = header->file_size_;
    dex_src = dex_container_->GetMainSection()->Begin();
  } else {
    LOG(WARNING) << "Failed to run dex layout, reason:" << error_msg;
    // Since we failed to convert the dex, just copy the input dex.
    dex_src = dex_file->Begin();
  }
  if (!WriteDexFile(out, oat_dex_file, dex_src, /* update_input_vdex */ false)) {
    return false;
  }
  if (dex_container_ != nullptr) {
    // Clear the main section in case we write more data into the container.
    dex_container_->GetMainSection()->Clear();
  }
  CHECK_EQ(oat_dex_file->dex_file_location_checksum_, dex_file->GetLocationChecksum());
  return true;
}

bool OatWriter::WriteDexFile(OutputStream* out,
                             File* file,
                             OatDexFile* oat_dex_file,
                             ZipEntry* dex_file) {
  size_t start_offset = vdex_size_;
  DCHECK_EQ(static_cast<off_t>(start_offset), out->Seek(0, kSeekCurrent));

  // Extract the dex file and get the extracted size.
  std::string error_msg;
  if (!dex_file->ExtractToFile(*file, &error_msg)) {
    LOG(ERROR) << "Failed to extract dex file from ZIP entry: " << error_msg
               << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (file->Flush() != 0) {
    PLOG(ERROR) << "Failed to flush dex file from ZIP entry."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  off_t extracted_end = lseek(file->Fd(), 0, SEEK_CUR);
  if (extracted_end == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed get end offset after writing dex file from ZIP entry."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (extracted_end < static_cast<off_t>(start_offset)) {
    LOG(ERROR) << "Dex file end position is before start position! End: " << extracted_end
               << " Start: " << start_offset
               << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  uint64_t extracted_size = static_cast<uint64_t>(extracted_end - start_offset);
  if (extracted_size < sizeof(DexFile::Header)) {
    LOG(ERROR) << "Extracted dex file is shorter than dex file header. size: "
               << extracted_size << " File: " << oat_dex_file->GetLocation();
    return false;
  }

  // Read the dex file header and extract required data to OatDexFile.
  off_t actual_offset = lseek(file->Fd(), start_offset, SEEK_SET);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek back to dex file header. Actual: " << actual_offset
                << " Expected: " << start_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (extracted_size < oat_dex_file->dex_file_size_) {
    LOG(ERROR) << "Extracted truncated dex file. Extracted size: " << extracted_size
               << " file size from header: " << oat_dex_file->dex_file_size_
               << " File: " << oat_dex_file->GetLocation();
    return false;
  }

  // Seek both file and stream to the end offset.
  size_t end_offset = start_offset + oat_dex_file->dex_file_size_;
  actual_offset = lseek(file->Fd(), end_offset, SEEK_SET);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = out->Seek(end_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek stream to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after seeking over dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // If we extracted more than the size specified in the header, truncate the file.
  if (extracted_size > oat_dex_file->dex_file_size_) {
    if (file->SetLength(end_offset) != 0) {
      PLOG(ERROR) << "Failed to truncate excessive dex file length."
                  << " File: " << oat_dex_file->GetLocation()
                  << " Output: " << file->GetPath();
      return false;
    }
  }

  return true;
}

bool OatWriter::WriteDexFile(OutputStream* out,
                             File* file,
                             OatDexFile* oat_dex_file,
                             File* dex_file) {
  size_t start_offset = vdex_size_;
  DCHECK_EQ(static_cast<off_t>(start_offset), out->Seek(0, kSeekCurrent));

  off_t input_offset = lseek(dex_file->Fd(), 0, SEEK_SET);
  if (input_offset != static_cast<off_t>(0)) {
    PLOG(ERROR) << "Failed to seek to dex file header. Actual: " << input_offset
                << " Expected: 0"
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // Copy the input dex file using sendfile().
  if (!file->Copy(dex_file, 0, oat_dex_file->dex_file_size_)) {
    PLOG(ERROR) << "Failed to copy dex file to oat file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (file->Flush() != 0) {
    PLOG(ERROR) << "Failed to flush dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // Check file position and seek the stream to the end offset.
  size_t end_offset = start_offset + oat_dex_file->dex_file_size_;
  off_t actual_offset = lseek(file->Fd(), 0, SEEK_CUR);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Unexpected file position after copying dex file. Actual: " << actual_offset
                << " Expected: " << end_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = out->Seek(end_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek stream to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after seeking over dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  return true;
}

bool OatWriter::WriteDexFile(OutputStream* out,
                             OatDexFile* oat_dex_file,
                             const uint8_t* dex_file,
                             bool update_input_vdex) {
  // Note: The raw data has already been checked to contain the header
  // and all the data that the header specifies as the file size.
  DCHECK(dex_file != nullptr);
  DCHECK(ValidateDexFileHeader(dex_file, oat_dex_file->GetLocation()));
  const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(dex_file);

  if (update_input_vdex) {
    // The vdex already contains the dex code, no need to write it again.
  } else {
    if (!out->WriteFully(dex_file, header->file_size_)) {
      PLOG(ERROR) << "Failed to write dex file " << oat_dex_file->GetLocation()
                  << " to " << out->GetLocation();
      return false;
    }
    if (!out->Flush()) {
      PLOG(ERROR) << "Failed to flush stream after writing dex file."
                  << " File: " << oat_dex_file->GetLocation();
      return false;
    }
  }
  return true;
}

bool OatWriter::OpenDexFiles(
    File* file,
    bool verify,
    /*out*/ std::vector<std::unique_ptr<MemMap>>* opened_dex_files_map,
    /*out*/ std::vector<std::unique_ptr<const DexFile>>* opened_dex_files) {
  TimingLogger::ScopedTiming split("OpenDexFiles", timings_);

  if (oat_dex_files_.empty()) {
    // Nothing to do.
    return true;
  }

  if (!extract_dex_files_into_vdex_) {
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    std::vector<std::unique_ptr<MemMap>> maps;
    for (OatDexFile& oat_dex_file : oat_dex_files_) {
      std::string error_msg;
      MemMap* map = oat_dex_file.source_.GetZipEntry()->MapDirectlyOrExtract(
          oat_dex_file.dex_file_location_data_, "zipped dex", &error_msg);
      if (map == nullptr) {
        LOG(ERROR) << error_msg;
        return false;
      }
      maps.emplace_back(map);
      // Now, open the dex file.
      const ArtDexFileLoader dex_file_loader;
      dex_files.emplace_back(dex_file_loader.Open(map->Begin(),
                                                  map->Size(),
                                                  oat_dex_file.GetLocation(),
                                                  oat_dex_file.dex_file_location_checksum_,
                                                  /* oat_dex_file */ nullptr,
                                                  verify,
                                                  verify,
                                                  &error_msg));
      if (dex_files.back() == nullptr) {
        LOG(ERROR) << "Failed to open dex file from oat file. File: " << oat_dex_file.GetLocation()
                   << " Error: " << error_msg;
        return false;
      }
      oat_dex_file.class_offsets_.resize(dex_files.back()->GetHeader().class_defs_size_);
    }
    *opened_dex_files_map = std::move(maps);
    *opened_dex_files = std::move(dex_files);
    CloseSources();
    return true;
  }
  // We could have closed the sources at the point of writing the dex files, but to
  // make it consistent with the case we're not writing the dex files, we close them now.
  CloseSources();

  size_t map_offset = oat_dex_files_[0].dex_file_offset_;
  size_t length = vdex_size_ - map_offset;

  std::string error_msg;
  std::unique_ptr<MemMap> dex_files_map(MemMap::MapFile(
      length,
      PROT_READ | PROT_WRITE,
      MAP_SHARED,
      file->Fd(),
      map_offset,
      /* low_4gb */ false,
      file->GetPath().c_str(),
      &error_msg));
  if (dex_files_map == nullptr) {
    LOG(ERROR) << "Failed to mmap() dex files from oat file. File: " << file->GetPath()
               << " error: " << error_msg;
    return false;
  }
  const ArtDexFileLoader dex_file_loader;
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    const uint8_t* raw_dex_file =
        dex_files_map->Begin() + oat_dex_file.dex_file_offset_ - map_offset;

    if (kIsDebugBuild) {
      // Sanity check our input files.
      // Note that ValidateDexFileHeader() logs error messages.
      CHECK(ValidateDexFileHeader(raw_dex_file, oat_dex_file.GetLocation()))
          << "Failed to verify written dex file header!"
          << " Output: " << file->GetPath() << " ~ " << std::hex << map_offset
          << " ~ " << static_cast<const void*>(raw_dex_file);

      const UnalignedDexFileHeader* header = AsUnalignedDexFileHeader(raw_dex_file);
      CHECK_EQ(header->file_size_, oat_dex_file.dex_file_size_)
          << "File size mismatch in written dex file header! Expected: "
          << oat_dex_file.dex_file_size_ << " Actual: " << header->file_size_
          << " Output: " << file->GetPath();
    }

    // Now, open the dex file.
    dex_files.emplace_back(dex_file_loader.Open(raw_dex_file,
                                                oat_dex_file.dex_file_size_,
                                                oat_dex_file.GetLocation(),
                                                oat_dex_file.dex_file_location_checksum_,
                                                /* oat_dex_file */ nullptr,
                                                verify,
                                                verify,
                                                &error_msg));
    if (dex_files.back() == nullptr) {
      LOG(ERROR) << "Failed to open dex file from oat file. File: " << oat_dex_file.GetLocation()
                 << " Error: " << error_msg;
      return false;
    }

    // Set the class_offsets size now that we have easy access to the DexFile and
    // it has been verified in dex_file_loader.Open.
    oat_dex_file.class_offsets_.resize(dex_files.back()->GetHeader().class_defs_size_);
  }

  opened_dex_files_map->push_back(std::move(dex_files_map));
  *opened_dex_files = std::move(dex_files);
  return true;
}

bool OatWriter::WriteTypeLookupTables(
    OutputStream* oat_rodata,
    const std::vector<std::unique_ptr<const DexFile>>& opened_dex_files) {
  TimingLogger::ScopedTiming split("WriteTypeLookupTables", timings_);

  uint32_t expected_offset = oat_data_offset_ + oat_size_;
  off_t actual_offset = oat_rodata->Seek(expected_offset, kSeekSet);
  if (static_cast<uint32_t>(actual_offset) != expected_offset) {
    PLOG(ERROR) << "Failed to seek to TypeLookupTable section. Actual: " << actual_offset
                << " Expected: " << expected_offset << " File: " << oat_rodata->GetLocation();
    return false;
  }

  DCHECK_EQ(opened_dex_files.size(), oat_dex_files_.size());
  for (size_t i = 0, size = opened_dex_files.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    DCHECK_EQ(oat_dex_file->lookup_table_offset_, 0u);

    if (oat_dex_file->create_type_lookup_table_ != CreateTypeLookupTable::kCreate ||
        oat_dex_file->class_offsets_.empty()) {
      continue;
    }

    size_t table_size = TypeLookupTable::RawDataLength(oat_dex_file->class_offsets_.size());
    if (table_size == 0u) {
      continue;
    }

    // Create the lookup table. When `nullptr` is given as the storage buffer,
    // TypeLookupTable allocates its own and OatDexFile takes ownership.
    const DexFile& dex_file = *opened_dex_files[i];
    {
      std::unique_ptr<TypeLookupTable> type_lookup_table =
          TypeLookupTable::Create(dex_file, /* storage */ nullptr);
      type_lookup_table_oat_dex_files_.push_back(
          std::make_unique<art::OatDexFile>(std::move(type_lookup_table)));
      dex_file.SetOatDexFile(type_lookup_table_oat_dex_files_.back().get());
    }
    TypeLookupTable* const table = type_lookup_table_oat_dex_files_.back()->GetTypeLookupTable();

    // Type tables are required to be 4 byte aligned.
    size_t initial_offset = oat_size_;
    size_t rodata_offset = RoundUp(initial_offset, 4);
    size_t padding_size = rodata_offset - initial_offset;

    if (padding_size != 0u) {
      std::vector<uint8_t> buffer(padding_size, 0u);
      if (!oat_rodata->WriteFully(buffer.data(), padding_size)) {
        PLOG(ERROR) << "Failed to write lookup table alignment padding."
                    << " File: " << oat_dex_file->GetLocation()
                    << " Output: " << oat_rodata->GetLocation();
        return false;
      }
    }

    DCHECK_EQ(oat_data_offset_ + rodata_offset,
              static_cast<size_t>(oat_rodata->Seek(0u, kSeekCurrent)));
    DCHECK_EQ(table_size, table->RawDataLength());

    if (!oat_rodata->WriteFully(table->RawData(), table_size)) {
      PLOG(ERROR) << "Failed to write lookup table."
                  << " File: " << oat_dex_file->GetLocation()
                  << " Output: " << oat_rodata->GetLocation();
      return false;
    }

    oat_dex_file->lookup_table_offset_ = rodata_offset;

    oat_size_ += padding_size + table_size;
    size_oat_lookup_table_ += table_size;
    size_oat_lookup_table_alignment_ += padding_size;
  }

  if (!oat_rodata->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after writing type lookup tables."
                << " File: " << oat_rodata->GetLocation();
    return false;
  }

  return true;
}

bool OatWriter::WriteDexLayoutSections(
    OutputStream* oat_rodata,
    const std::vector<std::unique_ptr<const DexFile>>& opened_dex_files) {
  TimingLogger::ScopedTiming split(__FUNCTION__, timings_);

  if (!kWriteDexLayoutInfo) {
    return true;;
  }

  uint32_t expected_offset = oat_data_offset_ + oat_size_;
  off_t actual_offset = oat_rodata->Seek(expected_offset, kSeekSet);
  if (static_cast<uint32_t>(actual_offset) != expected_offset) {
    PLOG(ERROR) << "Failed to seek to dex layout section offset section. Actual: " << actual_offset
                << " Expected: " << expected_offset << " File: " << oat_rodata->GetLocation();
    return false;
  }

  DCHECK_EQ(opened_dex_files.size(), oat_dex_files_.size());
  size_t rodata_offset = oat_size_;
  for (size_t i = 0, size = opened_dex_files.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    DCHECK_EQ(oat_dex_file->dex_sections_layout_offset_, 0u);

    // Write dex layout section alignment bytes.
    const size_t padding_size =
        RoundUp(rodata_offset, alignof(DexLayoutSections)) - rodata_offset;
    if (padding_size != 0u) {
      std::vector<uint8_t> buffer(padding_size, 0u);
      if (!oat_rodata->WriteFully(buffer.data(), padding_size)) {
        PLOG(ERROR) << "Failed to write lookup table alignment padding."
                    << " File: " << oat_dex_file->GetLocation()
                    << " Output: " << oat_rodata->GetLocation();
        return false;
      }
      size_oat_dex_file_dex_layout_sections_alignment_ += padding_size;
      rodata_offset += padding_size;
    }

    DCHECK_ALIGNED(rodata_offset, alignof(DexLayoutSections));
    DCHECK_EQ(oat_data_offset_ + rodata_offset,
              static_cast<size_t>(oat_rodata->Seek(0u, kSeekCurrent)));
    DCHECK(oat_dex_file != nullptr);
    if (!oat_rodata->WriteFully(&oat_dex_file->dex_sections_layout_,
                                sizeof(oat_dex_file->dex_sections_layout_))) {
      PLOG(ERROR) << "Failed to write dex layout sections."
                  << " File: " << oat_dex_file->GetLocation()
                  << " Output: " << oat_rodata->GetLocation();
      return false;
    }
    oat_dex_file->dex_sections_layout_offset_ = rodata_offset;
    size_oat_dex_file_dex_layout_sections_ += sizeof(oat_dex_file->dex_sections_layout_);
    rodata_offset += sizeof(oat_dex_file->dex_sections_layout_);
  }
  oat_size_ = rodata_offset;

  if (!oat_rodata->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after writing type dex layout sections."
                << " File: " << oat_rodata->GetLocation();
    return false;
  }

  return true;
}

bool OatWriter::WriteChecksumsAndVdexHeader(OutputStream* vdex_out) {
  // Write checksums
  off_t checksums_offset = sizeof(VdexFile::VerifierDepsHeader);
  off_t actual_offset = vdex_out->Seek(checksums_offset, kSeekSet);
  if (actual_offset != checksums_offset) {
    PLOG(ERROR) << "Failed to seek to the checksum location of vdex file. Actual: " << actual_offset
                << " File: " << vdex_out->GetLocation();
    return false;
  }

  for (size_t i = 0, size = oat_dex_files_.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    if (!vdex_out->WriteFully(
            &oat_dex_file->dex_file_location_checksum_, sizeof(VdexFile::VdexChecksum))) {
      PLOG(ERROR) << "Failed to write dex file location checksum. File: "
                  << vdex_out->GetLocation();
      return false;
    }
    size_vdex_checksums_ += sizeof(VdexFile::VdexChecksum);
  }

  // Maybe write dex section header.
  DCHECK_NE(vdex_verifier_deps_offset_, 0u);
  DCHECK_NE(vdex_quickening_info_offset_, 0u);

  bool has_dex_section = extract_dex_files_into_vdex_;
  if (has_dex_section) {
    DCHECK_NE(vdex_dex_files_offset_, 0u);
    size_t dex_section_size = vdex_dex_shared_data_offset_ - vdex_dex_files_offset_;
    size_t dex_shared_data_size = vdex_verifier_deps_offset_ - vdex_dex_shared_data_offset_;
    size_t quickening_info_section_size = vdex_size_ - vdex_quickening_info_offset_;

    VdexFile::DexSectionHeader dex_section_header(dex_section_size,
                                                  dex_shared_data_size,
                                                  quickening_info_section_size);
    if (!vdex_out->WriteFully(&dex_section_header, sizeof(VdexFile::DexSectionHeader))) {
      PLOG(ERROR) << "Failed to write vdex header. File: " << vdex_out->GetLocation();
      return false;
    }
    size_vdex_header_ += sizeof(VdexFile::DexSectionHeader);
  }

  // Write header.
  actual_offset = vdex_out->Seek(0, kSeekSet);
  if (actual_offset != 0) {
    PLOG(ERROR) << "Failed to seek to the beginning of vdex file. Actual: " << actual_offset
                << " File: " << vdex_out->GetLocation();
    return false;
  }

  size_t verifier_deps_section_size = vdex_quickening_info_offset_ - vdex_verifier_deps_offset_;

  VdexFile::VerifierDepsHeader deps_header(
      oat_dex_files_.size(), verifier_deps_section_size, has_dex_section);
  if (!vdex_out->WriteFully(&deps_header, sizeof(VdexFile::VerifierDepsHeader))) {
    PLOG(ERROR) << "Failed to write vdex header. File: " << vdex_out->GetLocation();
    return false;
  }
  size_vdex_header_ += sizeof(VdexFile::VerifierDepsHeader);

  if (!vdex_out->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after writing to vdex file."
                << " File: " << vdex_out->GetLocation();
    return false;
  }

  return true;
}

bool OatWriter::WriteCodeAlignment(OutputStream* out, uint32_t aligned_code_delta) {
  return WriteUpTo16BytesAlignment(out, aligned_code_delta, &size_code_alignment_);
}

bool OatWriter::WriteUpTo16BytesAlignment(OutputStream* out, uint32_t size, uint32_t* stat) {
  static const uint8_t kPadding[] = {
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
  };
  DCHECK_LE(size, sizeof(kPadding));
  if (UNLIKELY(!out->WriteFully(kPadding, size))) {
    return false;
  }
  *stat += size;
  return true;
}

void OatWriter::SetMultiOatRelativePatcherAdjustment() {
  DCHECK(dex_files_ != nullptr);
  DCHECK(relative_patcher_ != nullptr);
  DCHECK_NE(oat_data_offset_, 0u);
  if (image_writer_ != nullptr && !dex_files_->empty()) {
    // The oat data begin may not be initialized yet but the oat file offset is ready.
    size_t oat_index = image_writer_->GetOatIndexForDexFile(dex_files_->front());
    size_t elf_file_offset = image_writer_->GetOatFileOffset(oat_index);
    relative_patcher_->StartOatFile(elf_file_offset + oat_data_offset_);
  }
}

OatWriter::OatDexFile::OatDexFile(const char* dex_file_location,
                                  DexFileSource source,
                                  CreateTypeLookupTable create_type_lookup_table,
                                  uint32_t dex_file_location_checksum,
                                  size_t dex_file_size)
    : source_(source),
      create_type_lookup_table_(create_type_lookup_table),
      dex_file_size_(dex_file_size),
      offset_(0),
      dex_file_location_size_(strlen(dex_file_location)),
      dex_file_location_data_(dex_file_location),
      dex_file_location_checksum_(dex_file_location_checksum),
      dex_file_offset_(0u),
      lookup_table_offset_(0u),
      class_offsets_offset_(0u),
      method_bss_mapping_offset_(0u),
      type_bss_mapping_offset_(0u),
      string_bss_mapping_offset_(0u),
      dex_sections_layout_offset_(0u),
      class_offsets_() {
}

size_t OatWriter::OatDexFile::SizeOf() const {
  return sizeof(dex_file_location_size_)
          + dex_file_location_size_
          + sizeof(dex_file_location_checksum_)
          + sizeof(dex_file_offset_)
          + sizeof(class_offsets_offset_)
          + sizeof(lookup_table_offset_)
          + sizeof(method_bss_mapping_offset_)
          + sizeof(type_bss_mapping_offset_)
          + sizeof(string_bss_mapping_offset_)
          + sizeof(dex_sections_layout_offset_);
}

bool OatWriter::OatDexFile::Write(OatWriter* oat_writer, OutputStream* out) const {
  const size_t file_offset = oat_writer->oat_data_offset_;
  DCHECK_OFFSET_();

  if (!out->WriteFully(&dex_file_location_size_, sizeof(dex_file_location_size_))) {
    PLOG(ERROR) << "Failed to write dex file location length to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_size_ += sizeof(dex_file_location_size_);

  if (!out->WriteFully(dex_file_location_data_, dex_file_location_size_)) {
    PLOG(ERROR) << "Failed to write dex file location data to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_data_ += dex_file_location_size_;

  if (!out->WriteFully(&dex_file_location_checksum_, sizeof(dex_file_location_checksum_))) {
    PLOG(ERROR) << "Failed to write dex file location checksum to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_checksum_ += sizeof(dex_file_location_checksum_);

  if (!out->WriteFully(&dex_file_offset_, sizeof(dex_file_offset_))) {
    PLOG(ERROR) << "Failed to write dex file offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_offset_ += sizeof(dex_file_offset_);

  if (!out->WriteFully(&class_offsets_offset_, sizeof(class_offsets_offset_))) {
    PLOG(ERROR) << "Failed to write class offsets offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_class_offsets_offset_ += sizeof(class_offsets_offset_);

  if (!out->WriteFully(&lookup_table_offset_, sizeof(lookup_table_offset_))) {
    PLOG(ERROR) << "Failed to write lookup table offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_lookup_table_offset_ += sizeof(lookup_table_offset_);

  if (!out->WriteFully(&dex_sections_layout_offset_, sizeof(dex_sections_layout_offset_))) {
    PLOG(ERROR) << "Failed to write dex section layout info to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_dex_layout_sections_offset_ += sizeof(dex_sections_layout_offset_);

  if (!out->WriteFully(&method_bss_mapping_offset_, sizeof(method_bss_mapping_offset_))) {
    PLOG(ERROR) << "Failed to write method bss mapping offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_method_bss_mapping_offset_ += sizeof(method_bss_mapping_offset_);

  if (!out->WriteFully(&type_bss_mapping_offset_, sizeof(type_bss_mapping_offset_))) {
    PLOG(ERROR) << "Failed to write type bss mapping offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_type_bss_mapping_offset_ += sizeof(type_bss_mapping_offset_);

  if (!out->WriteFully(&string_bss_mapping_offset_, sizeof(string_bss_mapping_offset_))) {
    PLOG(ERROR) << "Failed to write string bss mapping offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_string_bss_mapping_offset_ += sizeof(string_bss_mapping_offset_);

  return true;
}

bool OatWriter::OatDexFile::WriteClassOffsets(OatWriter* oat_writer, OutputStream* out) {
  if (!out->WriteFully(class_offsets_.data(), GetClassOffsetsRawSize())) {
    PLOG(ERROR) << "Failed to write oat class offsets for " << GetLocation()
                << " to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_offsets_ += GetClassOffsetsRawSize();
  return true;
}

OatWriter::OatClass::OatClass(const dchecked_vector<CompiledMethod*>& compiled_methods,
                              uint32_t compiled_methods_with_code,
                              uint16_t oat_class_type)
    : compiled_methods_(compiled_methods) {
  const uint32_t num_methods = compiled_methods.size();
  CHECK_LE(compiled_methods_with_code, num_methods);

  oat_method_offsets_offsets_from_oat_class_.resize(num_methods);

  method_offsets_.resize(compiled_methods_with_code);
  method_headers_.resize(compiled_methods_with_code);

  uint32_t oat_method_offsets_offset_from_oat_class = OatClassHeader::SizeOf();
  // We only create this instance if there are at least some compiled.
  if (oat_class_type == kOatClassSomeCompiled) {
    method_bitmap_.reset(new BitVector(num_methods, false, Allocator::GetMallocAllocator()));
    method_bitmap_size_ = method_bitmap_->GetSizeOf();
    oat_method_offsets_offset_from_oat_class += sizeof(method_bitmap_size_);
    oat_method_offsets_offset_from_oat_class += method_bitmap_size_;
  } else {
    method_bitmap_ = nullptr;
    method_bitmap_size_ = 0;
  }

  for (size_t i = 0; i < num_methods; i++) {
    CompiledMethod* compiled_method = compiled_methods_[i];
    if (HasCompiledCode(compiled_method)) {
      oat_method_offsets_offsets_from_oat_class_[i] = oat_method_offsets_offset_from_oat_class;
      oat_method_offsets_offset_from_oat_class += sizeof(OatMethodOffsets);
      if (oat_class_type == kOatClassSomeCompiled) {
        method_bitmap_->SetBit(i);
      }
    } else {
      oat_method_offsets_offsets_from_oat_class_[i] = 0;
    }
  }
}

size_t OatWriter::OatClass::SizeOf() const {
  return ((method_bitmap_size_ == 0) ? 0 : sizeof(method_bitmap_size_))
          + method_bitmap_size_
          + (sizeof(method_offsets_[0]) * method_offsets_.size());
}

bool OatWriter::OatClassHeader::Write(OatWriter* oat_writer,
                                      OutputStream* out,
                                      const size_t file_offset) const {
  DCHECK_OFFSET_();
  if (!out->WriteFully(&status_, sizeof(status_))) {
    PLOG(ERROR) << "Failed to write class status to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_status_ += sizeof(status_);

  if (!out->WriteFully(&type_, sizeof(type_))) {
    PLOG(ERROR) << "Failed to write oat class type to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_type_ += sizeof(type_);
  return true;
}

bool OatWriter::OatClass::Write(OatWriter* oat_writer, OutputStream* out) const {
  if (method_bitmap_size_ != 0) {
    if (!out->WriteFully(&method_bitmap_size_, sizeof(method_bitmap_size_))) {
      PLOG(ERROR) << "Failed to write method bitmap size to " << out->GetLocation();
      return false;
    }
    oat_writer->size_oat_class_method_bitmaps_ += sizeof(method_bitmap_size_);

    if (!out->WriteFully(method_bitmap_->GetRawStorage(), method_bitmap_size_)) {
      PLOG(ERROR) << "Failed to write method bitmap to " << out->GetLocation();
      return false;
    }
    oat_writer->size_oat_class_method_bitmaps_ += method_bitmap_size_;
  }

  if (!out->WriteFully(method_offsets_.data(), GetMethodOffsetsRawSize())) {
    PLOG(ERROR) << "Failed to write method offsets to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_method_offsets_ += GetMethodOffsetsRawSize();
  return true;
}

const uint8_t* OatWriter::LookupBootImageInternTableSlot(const DexFile& dex_file,
                                                         dex::StringIndex string_idx)
    NO_THREAD_SAFETY_ANALYSIS {  // Single-threaded OatWriter can avoid locking.
  uint32_t utf16_length;
  const char* utf8_data = dex_file.StringDataAndUtf16LengthByIdx(string_idx, &utf16_length);
  DCHECK_EQ(utf16_length, CountModifiedUtf8Chars(utf8_data));
  InternTable::Utf8String string(utf16_length,
                                 utf8_data,
                                 ComputeUtf16HashFromModifiedUtf8(utf8_data, utf16_length));
  const InternTable* intern_table = Runtime::Current()->GetClassLinker()->intern_table_;
  for (const InternTable::Table::UnorderedSet& table : intern_table->strong_interns_.tables_) {
    auto it = table.Find(string);
    if (it != table.end()) {
      return reinterpret_cast<const uint8_t*>(std::addressof(*it));
    }
  }
  LOG(FATAL) << "Did not find boot image string " << utf8_data;
  UNREACHABLE();
}

const uint8_t* OatWriter::LookupBootImageClassTableSlot(const DexFile& dex_file,
                                                        dex::TypeIndex type_idx)
    NO_THREAD_SAFETY_ANALYSIS {  // Single-threaded OatWriter can avoid locking.
  const char* descriptor = dex_file.StringByTypeIdx(type_idx);
  ClassTable::DescriptorHashPair pair(descriptor, ComputeModifiedUtf8Hash(descriptor));
  ClassTable* table = Runtime::Current()->GetClassLinker()->boot_class_table_.get();
  for (const ClassTable::ClassSet& class_set : table->classes_) {
    auto it = class_set.Find(pair);
    if (it != class_set.end()) {
      return reinterpret_cast<const uint8_t*>(std::addressof(*it));
    }
  }
  LOG(FATAL) << "Did not find boot image class " << descriptor;
  UNREACHABLE();
}

debug::DebugInfo OatWriter::GetDebugInfo() const {
  debug::DebugInfo debug_info{};
  debug_info.compiled_methods = ArrayRef<const debug::MethodDebugInfo>(method_info_);
  if (VdexWillContainDexFiles()) {
    DCHECK_EQ(dex_files_->size(), oat_dex_files_.size());
    for (size_t i = 0, size = dex_files_->size(); i != size; ++i) {
      const DexFile* dex_file = (*dex_files_)[i];
      const OatDexFile& oat_dex_file = oat_dex_files_[i];
      uint32_t dex_file_offset = oat_dex_file.dex_file_offset_;
      if (dex_file_offset != 0) {
        debug_info.dex_files.emplace(dex_file_offset, dex_file);
      }
    }
  }
  return debug_info;
}

}  // namespace linker
}  // namespace art
