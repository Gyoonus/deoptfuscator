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

#ifndef ART_DEX2OAT_LINKER_OAT_WRITER_H_
#define ART_DEX2OAT_LINKER_OAT_WRITER_H_

#include <stdint.h>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/array_ref.h"
#include "base/dchecked_vector.h"
#include "base/os.h"
#include "base/safe_map.h"
#include "compiler.h"
#include "debug/debug_info.h"
#include "dex/compact_dex_level.h"
#include "dex/method_reference.h"
#include "dex/string_reference.h"
#include "dex/type_reference.h"
#include "linker/relative_patcher.h"  // For RelativePatcherTargetProvider.
#include "mem_map.h"
#include "mirror/class.h"
#include "oat.h"

namespace art {

class BitVector;
class CompiledMethod;
class CompilerDriver;
class DexContainer;
class ProfileCompilationInfo;
class TimingLogger;
class TypeLookupTable;
class VdexFile;
class ZipEntry;

namespace debug {
struct MethodDebugInfo;
}  // namespace debug

namespace verifier {
class VerifierDeps;
}  // namespace verifier

namespace linker {

class ImageWriter;
class MultiOatRelativePatcher;
class OutputStream;

// OatHeader         variable length with count of D OatDexFiles
//
// TypeLookupTable[0] one descriptor to class def index hash table for each OatDexFile.
// TypeLookupTable[1]
// ...
// TypeLookupTable[D]
//
// ClassOffsets[0]   one table of OatClass offsets for each class def for each OatDexFile.
// ClassOffsets[1]
// ...
// ClassOffsets[D]
//
// OatClass[0]       one variable sized OatClass for each of C DexFile::ClassDefs
// OatClass[1]       contains OatClass entries with class status, offsets to code, etc.
// ...
// OatClass[C]
//
// MethodBssMapping  one variable sized MethodBssMapping for each dex file, optional.
// MethodBssMapping
// ...
// MethodBssMapping
//
// VmapTable         one variable sized VmapTable blob (CodeInfo or QuickeningInfo).
// VmapTable         VmapTables are deduplicated.
// ...
// VmapTable
//
// MethodInfo        one variable sized blob with MethodInfo.
// MethodInfo        MethodInfos are deduplicated.
// ...
// MethodInfo
//
// OatDexFile[0]     one variable sized OatDexFile with offsets to Dex and OatClasses
// OatDexFile[1]
// ...
// OatDexFile[D]
//
// padding           if necessary so that the following code will be page aligned
//
// OatMethodHeader   fixed size header for a CompiledMethod including the size of the MethodCode.
// MethodCode        one variable sized blob with the code of a CompiledMethod.
// OatMethodHeader   (OatMethodHeader, MethodCode) pairs are deduplicated.
// MethodCode
// ...
// OatMethodHeader
// MethodCode
//
class OatWriter {
 public:
  enum class CreateTypeLookupTable {
    kCreate,
    kDontCreate,
    kDefault = kCreate
  };

  OatWriter(bool compiling_boot_image,
            TimingLogger* timings,
            ProfileCompilationInfo* info,
            CompactDexLevel compact_dex_level);

  // To produce a valid oat file, the user must first add sources with any combination of
  //   - AddDexFileSource(),
  //   - AddZippedDexFilesSource(),
  //   - AddRawDexFileSource(),
  //   - AddVdexDexFilesSource().
  // Then the user must call in order
  //   - WriteAndOpenDexFiles()
  //   - Initialize()
  //   - WriteVerifierDeps()
  //   - WriteQuickeningInfo()
  //   - WriteChecksumsAndVdexHeader()
  //   - PrepareLayout(),
  //   - WriteRodata(),
  //   - WriteCode(),
  //   - WriteHeader().

  // Add dex file source(s) from a file, either a plain dex file or
  // a zip file with one or more dex files.
  bool AddDexFileSource(
      const char* filename,
      const char* location,
      CreateTypeLookupTable create_type_lookup_table = CreateTypeLookupTable::kDefault);
  // Add dex file source(s) from a zip file specified by a file handle.
  bool AddZippedDexFilesSource(
      File&& zip_fd,
      const char* location,
      CreateTypeLookupTable create_type_lookup_table = CreateTypeLookupTable::kDefault);
  // Add dex file source from raw memory.
  bool AddRawDexFileSource(
      const ArrayRef<const uint8_t>& data,
      const char* location,
      uint32_t location_checksum,
      CreateTypeLookupTable create_type_lookup_table = CreateTypeLookupTable::kDefault);
  // Add dex file source(s) from a vdex file.
  bool AddVdexDexFilesSource(
      const VdexFile& vdex_file,
      const char* location,
      CreateTypeLookupTable create_type_lookup_table = CreateTypeLookupTable::kDefault);
  dchecked_vector<std::string> GetSourceLocations() const;

  // Write raw dex files to the vdex file, mmap the file and open the dex files from it.
  // Supporting data structures are written into the .rodata section of the oat file.
  // The `verify` setting dictates whether the dex file verifier should check the dex files.
  // This is generally the case, and should only be false for tests.
  // If `update_input_vdex` is true, then this method won't actually write the dex files,
  // and the compiler will just re-use the existing vdex file.
  bool WriteAndOpenDexFiles(File* vdex_file,
                            OutputStream* oat_rodata,
                            InstructionSet instruction_set,
                            const InstructionSetFeatures* instruction_set_features,
                            SafeMap<std::string, std::string>* key_value_store,
                            bool verify,
                            bool update_input_vdex,
                            CopyOption copy_dex_files,
                            /*out*/ std::vector<std::unique_ptr<MemMap>>* opened_dex_files_map,
                            /*out*/ std::vector<std::unique_ptr<const DexFile>>* opened_dex_files);
  bool WriteQuickeningInfo(OutputStream* vdex_out);
  bool WriteVerifierDeps(OutputStream* vdex_out, verifier::VerifierDeps* verifier_deps);
  bool WriteChecksumsAndVdexHeader(OutputStream* vdex_out);
  // Initialize the writer with the given parameters.
  void Initialize(const CompilerDriver* compiler,
                  ImageWriter* image_writer,
                  const std::vector<const DexFile*>& dex_files) {
    compiler_driver_ = compiler;
    image_writer_ = image_writer;
    dex_files_ = &dex_files;
  }

  // Prepare layout of remaining data.
  void PrepareLayout(MultiOatRelativePatcher* relative_patcher);
  // Write the rest of .rodata section (ClassOffsets[], OatClass[], maps).
  bool WriteRodata(OutputStream* out);
  // Write the code to the .text section.
  bool WriteCode(OutputStream* out);
  // Write the oat header. This finalizes the oat file.
  bool WriteHeader(OutputStream* out,
                   uint32_t image_file_location_oat_checksum,
                   uintptr_t image_file_location_oat_begin,
                   int32_t image_patch_delta);

  // Returns whether the oat file has an associated image.
  bool HasImage() const {
    // Since the image is being created at the same time as the oat file,
    // check if there's an image writer.
    return image_writer_ != nullptr;
  }

  bool HasBootImage() const {
    return compiling_boot_image_;
  }

  const OatHeader& GetOatHeader() const {
    return *oat_header_;
  }

  size_t GetOatSize() const {
    return oat_size_;
  }

  size_t GetBssSize() const {
    return bss_size_;
  }

  size_t GetBssMethodsOffset() const {
    return bss_methods_offset_;
  }

  size_t GetBssRootsOffset() const {
    return bss_roots_offset_;
  }

  size_t GetVdexSize() const {
    return vdex_size_;
  }

  size_t GetOatDataOffset() const {
    return oat_data_offset_;
  }

  ~OatWriter();

  debug::DebugInfo GetDebugInfo() const;

  const CompilerDriver* GetCompilerDriver() const {
    return compiler_driver_;
  }

 private:
  class DexFileSource;
  class OatClassHeader;
  class OatClass;
  class OatDexFile;

  // The function VisitDexMethods() below iterates through all the methods in all
  // the compiled dex files in order of their definitions. The method visitor
  // classes provide individual bits of processing for each of the passes we need to
  // first collect the data we want to write to the oat file and then, in later passes,
  // to actually write it.
  class DexMethodVisitor;
  class OatDexMethodVisitor;
  class InitBssLayoutMethodVisitor;
  class InitOatClassesMethodVisitor;
  class LayoutCodeMethodVisitor;
  class LayoutReserveOffsetCodeMethodVisitor;
  struct OrderedMethodData;
  class OrderedMethodVisitor;
  class InitCodeMethodVisitor;
  class InitMapMethodVisitor;
  class InitMethodInfoVisitor;
  class InitImageMethodVisitor;
  class WriteCodeMethodVisitor;
  class WriteMapMethodVisitor;
  class WriteMethodInfoVisitor;
  class WriteQuickeningInfoMethodVisitor;
  class WriteQuickeningInfoOffsetsMethodVisitor;

  // Visit all the methods in all the compiled dex files in their definition order
  // with a given DexMethodVisitor.
  bool VisitDexMethods(DexMethodVisitor* visitor);

  // If `update_input_vdex` is true, then this method won't actually write the dex files,
  // and the compiler will just re-use the existing vdex file.
  bool WriteDexFiles(OutputStream* out,
                     File* file,
                     bool update_input_vdex,
                     CopyOption copy_dex_files);
  bool WriteDexFile(OutputStream* out,
                    File* file,
                    OatDexFile* oat_dex_file,
                    bool update_input_vdex);
  bool SeekToDexFile(OutputStream* out, File* file, OatDexFile* oat_dex_file);
  bool LayoutAndWriteDexFile(OutputStream* out, OatDexFile* oat_dex_file);
  bool WriteDexFile(OutputStream* out,
                    File* file,
                    OatDexFile* oat_dex_file,
                    ZipEntry* dex_file);
  bool WriteDexFile(OutputStream* out,
                    File* file,
                    OatDexFile* oat_dex_file,
                    File* dex_file);
  bool WriteDexFile(OutputStream* out,
                    OatDexFile* oat_dex_file,
                    const uint8_t* dex_file,
                    bool update_input_vdex);
  bool OpenDexFiles(File* file,
                    bool verify,
                    /*out*/ std::vector<std::unique_ptr<MemMap>>* opened_dex_files_map,
                    /*out*/ std::vector<std::unique_ptr<const DexFile>>* opened_dex_files);

  size_t InitOatHeader(InstructionSet instruction_set,
                       const InstructionSetFeatures* instruction_set_features,
                       uint32_t num_dex_files,
                       SafeMap<std::string, std::string>* key_value_store);
  size_t InitClassOffsets(size_t offset);
  size_t InitOatClasses(size_t offset);
  size_t InitOatMaps(size_t offset);
  size_t InitIndexBssMappings(size_t offset);
  size_t InitOatDexFiles(size_t offset);
  size_t InitOatCode(size_t offset);
  size_t InitOatCodeDexFiles(size_t offset);
  void InitBssLayout(InstructionSet instruction_set);

  size_t WriteClassOffsets(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteClasses(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteMaps(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteIndexBssMappings(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteOatDexFiles(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteCode(OutputStream* out, size_t file_offset, size_t relative_offset);
  size_t WriteCodeDexFiles(OutputStream* out, size_t file_offset, size_t relative_offset);

  bool RecordOatDataOffset(OutputStream* out);
  bool WriteTypeLookupTables(OutputStream* oat_rodata,
                             const std::vector<std::unique_ptr<const DexFile>>& opened_dex_files);
  bool WriteDexLayoutSections(OutputStream* oat_rodata,
                              const std::vector<std::unique_ptr<const DexFile>>& opened_dex_files);
  bool WriteCodeAlignment(OutputStream* out, uint32_t aligned_code_delta);
  bool WriteUpTo16BytesAlignment(OutputStream* out, uint32_t size, uint32_t* stat);
  void SetMultiOatRelativePatcherAdjustment();
  void CloseSources();

  bool MayHaveCompiledMethods() const;

  bool VdexWillContainDexFiles() const {
    return dex_files_ != nullptr && extract_dex_files_into_vdex_;
  }

  // Find the address of the GcRoot<String> in the InternTable for a boot image string.
  const uint8_t* LookupBootImageInternTableSlot(const DexFile& dex_file,
                                                dex::StringIndex string_idx);
  // Find the address of the ClassTable::TableSlot for a boot image class.
  const uint8_t* LookupBootImageClassTableSlot(const DexFile& dex_file, dex::TypeIndex type_idx);

  enum class WriteState {
    kAddingDexFileSources,
    kPrepareLayout,
    kWriteRoData,
    kWriteText,
    kWriteHeader,
    kDone
  };

  WriteState write_state_;
  TimingLogger* timings_;

  std::vector<std::unique_ptr<File>> raw_dex_files_;
  std::vector<std::unique_ptr<ZipArchive>> zip_archives_;
  std::vector<std::unique_ptr<ZipEntry>> zipped_dex_files_;

  // Using std::list<> which doesn't move elements around on push/emplace_back().
  // We need this because we keep plain pointers to the strings' c_str().
  std::list<std::string> zipped_dex_file_locations_;

  dchecked_vector<debug::MethodDebugInfo> method_info_;

  const CompilerDriver* compiler_driver_;
  ImageWriter* image_writer_;
  const bool compiling_boot_image_;
  // Whether the dex files being compiled are going to be extracted to the vdex.
  bool extract_dex_files_into_vdex_;

  // note OatFile does not take ownership of the DexFiles
  const std::vector<const DexFile*>* dex_files_;

  // Size required for Vdex data structures.
  size_t vdex_size_;

  // Offset of section holding Dex files inside Vdex.
  size_t vdex_dex_files_offset_;

  // Offset of section holding shared dex data section in the Vdex.
  size_t vdex_dex_shared_data_offset_;

  // Offset of section holding VerifierDeps inside Vdex.
  size_t vdex_verifier_deps_offset_;

  // Offset of section holding quickening info inside Vdex.
  size_t vdex_quickening_info_offset_;

  // Size required for Oat data structures.
  size_t oat_size_;

  // The start of the required .bss section.
  size_t bss_start_;

  // The size of the required .bss section holding the DexCache data and GC roots.
  size_t bss_size_;

  // The offset of the methods in .bss section.
  size_t bss_methods_offset_;

  // The offset of the GC roots in .bss section.
  size_t bss_roots_offset_;

  // Map for recording references to ArtMethod entries in .bss.
  SafeMap<const DexFile*, BitVector> bss_method_entry_references_;

  // Map for recording references to GcRoot<mirror::Class> entries in .bss.
  SafeMap<const DexFile*, BitVector> bss_type_entry_references_;

  // Map for recording references to GcRoot<mirror::String> entries in .bss.
  SafeMap<const DexFile*, BitVector> bss_string_entry_references_;

  // Map for allocating ArtMethod entries in .bss. Indexed by MethodReference for the target
  // method in the dex file with the "method reference value comparator" for deduplication.
  // The value is the target offset for patching, starting at `bss_start_ + bss_methods_offset_`.
  SafeMap<MethodReference, size_t, MethodReferenceValueComparator> bss_method_entries_;

  // Map for allocating Class entries in .bss. Indexed by TypeReference for the source
  // type in the dex file with the "type value comparator" for deduplication. The value
  // is the target offset for patching, starting at `bss_start_ + bss_roots_offset_`.
  SafeMap<TypeReference, size_t, TypeReferenceValueComparator> bss_type_entries_;

  // Map for allocating String entries in .bss. Indexed by StringReference for the source
  // string in the dex file with the "string value comparator" for deduplication. The value
  // is the target offset for patching, starting at `bss_start_ + bss_roots_offset_`.
  SafeMap<StringReference, size_t, StringReferenceValueComparator> bss_string_entries_;

  // Whether boot image tables should be mapped to the .bss. This is needed for compiled
  // code that reads from these tables with PC-relative instructions.
  bool map_boot_image_tables_to_bss_;

  // Offset of the oat data from the start of the mmapped region of the elf file.
  size_t oat_data_offset_;

  // Fake OatDexFiles to hold type lookup tables for the compiler.
  std::vector<std::unique_ptr<art::OatDexFile>> type_lookup_table_oat_dex_files_;

  // data to write
  std::unique_ptr<OatHeader> oat_header_;
  dchecked_vector<OatDexFile> oat_dex_files_;
  dchecked_vector<OatClassHeader> oat_class_headers_;
  dchecked_vector<OatClass> oat_classes_;
  std::unique_ptr<const std::vector<uint8_t>> jni_dlsym_lookup_;
  std::unique_ptr<const std::vector<uint8_t>> quick_generic_jni_trampoline_;
  std::unique_ptr<const std::vector<uint8_t>> quick_imt_conflict_trampoline_;
  std::unique_ptr<const std::vector<uint8_t>> quick_resolution_trampoline_;
  std::unique_ptr<const std::vector<uint8_t>> quick_to_interpreter_bridge_;

  // output stats
  uint32_t size_vdex_header_;
  uint32_t size_vdex_checksums_;
  uint32_t size_dex_file_alignment_;
  uint32_t size_executable_offset_alignment_;
  uint32_t size_oat_header_;
  uint32_t size_oat_header_key_value_store_;
  uint32_t size_dex_file_;
  uint32_t size_verifier_deps_;
  uint32_t size_verifier_deps_alignment_;
  uint32_t size_quickening_info_;
  uint32_t size_quickening_info_alignment_;
  uint32_t size_interpreter_to_interpreter_bridge_;
  uint32_t size_interpreter_to_compiled_code_bridge_;
  uint32_t size_jni_dlsym_lookup_;
  uint32_t size_quick_generic_jni_trampoline_;
  uint32_t size_quick_imt_conflict_trampoline_;
  uint32_t size_quick_resolution_trampoline_;
  uint32_t size_quick_to_interpreter_bridge_;
  uint32_t size_trampoline_alignment_;
  uint32_t size_method_header_;
  uint32_t size_code_;
  uint32_t size_code_alignment_;
  uint32_t size_relative_call_thunks_;
  uint32_t size_misc_thunks_;
  uint32_t size_vmap_table_;
  uint32_t size_method_info_;
  uint32_t size_oat_dex_file_location_size_;
  uint32_t size_oat_dex_file_location_data_;
  uint32_t size_oat_dex_file_location_checksum_;
  uint32_t size_oat_dex_file_offset_;
  uint32_t size_oat_dex_file_class_offsets_offset_;
  uint32_t size_oat_dex_file_lookup_table_offset_;
  uint32_t size_oat_dex_file_dex_layout_sections_offset_;
  uint32_t size_oat_dex_file_dex_layout_sections_;
  uint32_t size_oat_dex_file_dex_layout_sections_alignment_;
  uint32_t size_oat_dex_file_method_bss_mapping_offset_;
  uint32_t size_oat_dex_file_type_bss_mapping_offset_;
  uint32_t size_oat_dex_file_string_bss_mapping_offset_;
  uint32_t size_oat_lookup_table_alignment_;
  uint32_t size_oat_lookup_table_;
  uint32_t size_oat_class_offsets_alignment_;
  uint32_t size_oat_class_offsets_;
  uint32_t size_oat_class_type_;
  uint32_t size_oat_class_status_;
  uint32_t size_oat_class_method_bitmaps_;
  uint32_t size_oat_class_method_offsets_;
  uint32_t size_method_bss_mappings_;
  uint32_t size_type_bss_mappings_;
  uint32_t size_string_bss_mappings_;

  // The helper for processing relative patches is external so that we can patch across oat files.
  MultiOatRelativePatcher* relative_patcher_;

  // The locations of absolute patches relative to the start of the executable section.
  dchecked_vector<uintptr_t> absolute_patch_locations_;

  // Profile info used to generate new layout of files.
  ProfileCompilationInfo* profile_compilation_info_;

  // Compact dex level that is generated.
  CompactDexLevel compact_dex_level_;

  using OrderedMethodList = std::vector<OrderedMethodData>;

  // List of compiled methods, sorted by the order defined in OrderedMethodData.
  // Methods can be inserted more than once in case of duplicated methods.
  // This pointer is only non-null after InitOatCodeDexFiles succeeds.
  std::unique_ptr<OrderedMethodList> ordered_methods_;

  // Container of shared dex data.
  std::unique_ptr<DexContainer> dex_container_;

  DISALLOW_COPY_AND_ASSIGN(OatWriter);
};

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_OAT_WRITER_H_
