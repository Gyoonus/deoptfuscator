/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * Header file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#ifndef ART_DEXLAYOUT_DEXLAYOUT_H_
#define ART_DEXLAYOUT_DEXLAYOUT_H_

#include <stdint.h>
#include <stdio.h>
#include <unordered_map>

#include "dex/compact_dex_level.h"
#include "dex_container.h"
#include "dex/dex_file_layout.h"
#include "dex_ir.h"

namespace art {

class DexFile;
class Instruction;
class ProfileCompilationInfo;

/* Supported output formats. */
enum OutputFormat {
  kOutputPlain = 0,  // default
  kOutputXml,        // XML-style
};

/* Command-line options. */
class Options {
 public:
  Options() = default;

  bool dump_ = false;
  bool build_dex_ir_ = false;
  bool checksum_only_ = false;
  bool disassemble_ = false;
  bool exports_only_ = false;
  bool ignore_bad_checksum_ = false;
  bool output_to_container_ = false;
  bool show_annotations_ = false;
  bool show_file_headers_ = false;
  bool show_section_headers_ = false;
  bool show_section_statistics_ = false;
  bool verbose_ = false;
  bool verify_output_ = kIsDebugBuild;
  bool visualize_pattern_ = false;
  bool update_checksum_ = false;
  CompactDexLevel compact_dex_level_ = CompactDexLevel::kCompactDexLevelNone;
  bool dedupe_code_items_ = true;
  OutputFormat output_format_ = kOutputPlain;
  const char* output_dex_directory_ = nullptr;
  const char* output_file_name_ = nullptr;
  const char* profile_file_name_ = nullptr;
  // Filter that removes classes that don't have a matching descriptor (during IR creation).
  // This speeds up cases when the output only requires a single class.
  std::set<std::string> class_filter_;
};

// Hotness info
class DexLayoutHotnessInfo {
 public:
  // Store layout information so that the offset calculation can specify the section sizes.
  std::unordered_map<dex_ir::CodeItem*, LayoutType> code_item_layout_;
};

class DexLayout {
 public:
  class VectorOutputContainer {
   public:
    // Begin is not necessarily aligned (for now).
    uint8_t* Begin() {
      return &data_[0];
    }

   private:
    std::vector<uint8_t> data_;
  };


  // Setting this to false disables class def layout entirely, which is stronger than strictly
  // necessary to ensure the partial order w.r.t. class derivation. TODO: Re-enable (b/68317550).
  static constexpr bool kChangeClassDefOrder = false;

  DexLayout(Options& options,
            ProfileCompilationInfo* info,
            FILE* out_file,
            dex_ir::Header* header)
      : options_(options),
        info_(info),
        out_file_(out_file),
        header_(header) { }

  int ProcessFile(const char* file_name);
  bool ProcessDexFile(const char* file_name,
                      const DexFile* dex_file,
                      size_t dex_file_index,
                      std::unique_ptr<DexContainer>* dex_container,
                      std::string* error_msg);

  dex_ir::Header* GetHeader() const { return header_; }
  void SetHeader(dex_ir::Header* header) { header_ = header; }

  DexLayoutSections& GetSections() {
    return dex_sections_;
  }

  const DexLayoutHotnessInfo& LayoutHotnessInfo() const {
    return layout_hotness_info_;
  }

  const Options& GetOptions() const {
    return options_;
  }

 private:
  void DumpAnnotationSetItem(dex_ir::AnnotationSetItem* set_item);
  void DumpBytecodes(uint32_t idx, const dex_ir::CodeItem* code, uint32_t code_offset);
  void DumpCatches(const dex_ir::CodeItem* code);
  void DumpClass(int idx, char** last_package);
  void DumpClassAnnotations(int idx);
  void DumpClassDef(int idx);
  void DumpCode(uint32_t idx,
                const dex_ir::CodeItem* code,
                uint32_t code_offset,
                const char* declaring_class_descriptor,
                const char* method_name,
                bool is_static,
                const dex_ir::ProtoId* proto);
  void DumpEncodedAnnotation(dex_ir::EncodedAnnotation* annotation);
  void DumpEncodedValue(const dex_ir::EncodedValue* data);
  void DumpFileHeader();
  void DumpIField(uint32_t idx, uint32_t flags, int i);
  void DumpInstruction(const dex_ir::CodeItem* code,
                       uint32_t code_offset,
                       uint32_t insn_idx,
                       uint32_t insn_width,
                       const Instruction* dec_insn);
  void DumpInterface(const dex_ir::TypeId* type_item, int i);
  void DumpLocalInfo(const dex_ir::CodeItem* code);
  void DumpMethod(uint32_t idx, uint32_t flags, const dex_ir::CodeItem* code, int i);
  void DumpPositionInfo(const dex_ir::CodeItem* code);
  void DumpSField(uint32_t idx, uint32_t flags, int i, dex_ir::EncodedValue* init);
  void DumpDexFile();

  void LayoutClassDefsAndClassData(const DexFile* dex_file);
  void LayoutCodeItems(const DexFile* dex_file);
  void LayoutStringData(const DexFile* dex_file);

  // Creates a new layout for the dex file based on profile info.
  // Currently reorders ClassDefs, ClassDataItems, and CodeItems.
  void LayoutOutputFile(const DexFile* dex_file);
  bool OutputDexFile(const DexFile* input_dex_file,
                     bool compute_offsets,
                     std::unique_ptr<DexContainer>* dex_container,
                     std::string* error_msg);

  void DumpCFG(const DexFile* dex_file, int idx);
  void DumpCFG(const DexFile* dex_file, uint32_t dex_method_idx, const DexFile::CodeItem* code);

  Options& options_;
  ProfileCompilationInfo* info_;
  FILE* out_file_;
  dex_ir::Header* header_;
  DexLayoutSections dex_sections_;
  // Layout hotness information is only calculated when dexlayout is enabled.
  DexLayoutHotnessInfo layout_hotness_info_;

  DISALLOW_COPY_AND_ASSIGN(DexLayout);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEXLAYOUT_H_
