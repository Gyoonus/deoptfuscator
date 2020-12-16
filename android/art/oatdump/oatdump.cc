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

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "arch/instruction_set_features.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/bit_utils_iterator.h"
#include "base/os.h"
#include "base/safe_map.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker-inl.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "debug/debug_info.h"
#include "debug/elf_debug_writer.h"
#include "debug/method_debug_info.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/string_reference.h"
#include "disassembler.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/space/image_space.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "image-inl.h"
#include "imtable-inl.h"
#include "indenter.h"
#include "subtype_check.h"
#include "index_bss_mapping.h"
#include "interpreter/unstarted_runtime.h"
#include "linker/buffered_output_stream.h"
#include "linker/elf_builder.h"
#include "linker/file_output_stream.h"
#include "mirror/array-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "oat.h"
#include "oat_file-inl.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "stack_map.h"
#include "thread_list.h"
#include "type_lookup_table.h"
#include "vdex_file.h"
#include "verifier/method_verifier.h"
#include "verifier/verifier_deps.h"
#include "well_known_classes.h"

#include <sys/stat.h>
#include "cmdline.h"

namespace art {

using android::base::StringPrintf;

const char* image_methods_descriptions_[] = {
  "kResolutionMethod",
  "kImtConflictMethod",
  "kImtUnimplementedMethod",
  "kSaveAllCalleeSavesMethod",
  "kSaveRefsOnlyMethod",
  "kSaveRefsAndArgsMethod",
  "kSaveEverythingMethod",
  "kSaveEverythingMethodForClinit",
  "kSaveEverythingMethodForSuspendCheck",
};

const char* image_roots_descriptions_[] = {
  "kDexCaches",
  "kClassRoots",
  "kClassLoader",
};

// Map is so that we don't allocate multiple dex files for the same OatDexFile.
static std::map<const OatFile::OatDexFile*,
                std::unique_ptr<const DexFile>> opened_dex_files;

const DexFile* OpenDexFile(const OatFile::OatDexFile* oat_dex_file, std::string* error_msg) {
  DCHECK(oat_dex_file != nullptr);
  auto it = opened_dex_files.find(oat_dex_file);
  if (it != opened_dex_files.end()) {
    return it->second.get();
  }
  const DexFile* ret = oat_dex_file->OpenDexFile(error_msg).release();
  opened_dex_files.emplace(oat_dex_file, std::unique_ptr<const DexFile>(ret));
  return ret;
}

template <typename ElfTypes>
class OatSymbolizer FINAL {
 public:
  OatSymbolizer(const OatFile* oat_file, const std::string& output_name, bool no_bits) :
      oat_file_(oat_file),
      builder_(nullptr),
      output_name_(output_name.empty() ? "symbolized.oat" : output_name),
      no_bits_(no_bits) {
  }

  bool Symbolize() {
    const InstructionSet isa = oat_file_->GetOatHeader().GetInstructionSet();
    std::unique_ptr<const InstructionSetFeatures> features = InstructionSetFeatures::FromBitmap(
        isa, oat_file_->GetOatHeader().GetInstructionSetFeaturesBitmap());

    std::unique_ptr<File> elf_file(OS::CreateEmptyFile(output_name_.c_str()));
    if (elf_file == nullptr) {
      return false;
    }
    std::unique_ptr<linker::BufferedOutputStream> output_stream =
        std::make_unique<linker::BufferedOutputStream>(
            std::make_unique<linker::FileOutputStream>(elf_file.get()));
    builder_.reset(new linker::ElfBuilder<ElfTypes>(isa, features.get(), output_stream.get()));

    builder_->Start();

    auto* rodata = builder_->GetRoData();
    auto* text = builder_->GetText();

    const uint8_t* rodata_begin = oat_file_->Begin();
    const size_t rodata_size = oat_file_->GetOatHeader().GetExecutableOffset();
    if (!no_bits_) {
      rodata->Start();
      rodata->WriteFully(rodata_begin, rodata_size);
      rodata->End();
    }

    const uint8_t* text_begin = oat_file_->Begin() + rodata_size;
    const size_t text_size = oat_file_->End() - text_begin;
    if (!no_bits_) {
      text->Start();
      text->WriteFully(text_begin, text_size);
      text->End();
    }

    if (isa == InstructionSet::kMips || isa == InstructionSet::kMips64) {
      builder_->WriteMIPSabiflagsSection();
    }
    builder_->PrepareDynamicSection(elf_file->GetPath(),
                                    rodata_size,
                                    text_size,
                                    oat_file_->BssSize(),
                                    oat_file_->BssMethodsOffset(),
                                    oat_file_->BssRootsOffset(),
                                    oat_file_->VdexSize());
    builder_->WriteDynamicSection();

    const OatHeader& oat_header = oat_file_->GetOatHeader();
    #define DO_TRAMPOLINE(fn_name)                                                \
      if (oat_header.Get ## fn_name ## Offset() != 0) {                           \
        debug::MethodDebugInfo info = {};                                         \
        info.custom_name = #fn_name;                                              \
        info.isa = oat_header.GetInstructionSet();                                \
        info.is_code_address_text_relative = true;                                \
        size_t code_offset = oat_header.Get ## fn_name ## Offset();               \
        code_offset -= CompiledCode::CodeDelta(oat_header.GetInstructionSet());   \
        info.code_address = code_offset - oat_header.GetExecutableOffset();       \
        info.code_size = 0;  /* The symbol lasts until the next symbol. */        \
        method_debug_infos_.push_back(std::move(info));                           \
      }
    DO_TRAMPOLINE(InterpreterToInterpreterBridge)
    DO_TRAMPOLINE(InterpreterToCompiledCodeBridge)
    DO_TRAMPOLINE(JniDlsymLookup);
    DO_TRAMPOLINE(QuickGenericJniTrampoline);
    DO_TRAMPOLINE(QuickImtConflictTrampoline);
    DO_TRAMPOLINE(QuickResolutionTrampoline);
    DO_TRAMPOLINE(QuickToInterpreterBridge);
    #undef DO_TRAMPOLINE

    Walk();

    // TODO: Try to symbolize link-time thunks?
    // This would require disassembling all methods to find branches outside the method code.

    // TODO: Add symbols for dex bytecode in the .dex section.

    debug::DebugInfo debug_info{};
    debug_info.compiled_methods = ArrayRef<const debug::MethodDebugInfo>(method_debug_infos_);

    debug::WriteDebugInfo(builder_.get(),
                          debug_info,
                          dwarf::DW_DEBUG_FRAME_FORMAT,
                          true /* write_oat_patches */);

    builder_->End();

    bool ret_value = builder_->Good();

    builder_.reset();
    output_stream.reset();

    if (elf_file->FlushCloseOrErase() != 0) {
      return false;
    }
    elf_file.reset();

    return ret_value;
  }

  void Walk() {
    std::vector<const OatFile::OatDexFile*> oat_dex_files = oat_file_->GetOatDexFiles();
    for (size_t i = 0; i < oat_dex_files.size(); i++) {
      const OatFile::OatDexFile* oat_dex_file = oat_dex_files[i];
      CHECK(oat_dex_file != nullptr);
      WalkOatDexFile(oat_dex_file);
    }
  }

  void WalkOatDexFile(const OatFile::OatDexFile* oat_dex_file) {
    std::string error_msg;
    const DexFile* const dex_file = OpenDexFile(oat_dex_file, &error_msg);
    if (dex_file == nullptr) {
      return;
    }
    for (size_t class_def_index = 0;
        class_def_index < dex_file->NumClassDefs();
        class_def_index++) {
      const OatFile::OatClass oat_class = oat_dex_file->GetOatClass(class_def_index);
      OatClassType type = oat_class.GetType();
      switch (type) {
        case kOatClassAllCompiled:
        case kOatClassSomeCompiled:
          WalkOatClass(oat_class, *dex_file, class_def_index);
          break;

        case kOatClassNoneCompiled:
        case kOatClassMax:
          // Ignore.
          break;
      }
    }
  }

  void WalkOatClass(const OatFile::OatClass& oat_class,
                    const DexFile& dex_file,
                    uint32_t class_def_index) {
    const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    const uint8_t* class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {  // empty class such as a marker interface?
      return;
    }
    // Note: even if this is an interface or a native class, we still have to walk it, as there
    //       might be a static initializer.
    ClassDataItemIterator it(dex_file, class_data);
    uint32_t class_method_idx = 0;
    it.SkipAllFields();
    for (; it.HasNextMethod(); it.Next()) {
      WalkOatMethod(oat_class.GetOatMethod(class_method_idx++),
                    dex_file,
                    class_def_index,
                    it.GetMemberIndex(),
                    it.GetMethodCodeItem(),
                    it.GetMethodAccessFlags());
    }
    DCHECK(!it.HasNext());
  }

  void WalkOatMethod(const OatFile::OatMethod& oat_method,
                     const DexFile& dex_file,
                     uint32_t class_def_index,
                     uint32_t dex_method_index,
                     const DexFile::CodeItem* code_item,
                     uint32_t method_access_flags) {
    if ((method_access_flags & kAccAbstract) != 0) {
      // Abstract method, no code.
      return;
    }
    const OatHeader& oat_header = oat_file_->GetOatHeader();
    const OatQuickMethodHeader* method_header = oat_method.GetOatQuickMethodHeader();
    if (method_header == nullptr || method_header->GetCodeSize() == 0) {
      // No code.
      return;
    }

    uint32_t entry_point = oat_method.GetCodeOffset() - oat_header.GetExecutableOffset();
    // Clear Thumb2 bit.
    const void* code_address = EntryPointToCodePointer(reinterpret_cast<void*>(entry_point));

    debug::MethodDebugInfo info = {};
    DCHECK(info.custom_name.empty());
    info.dex_file = &dex_file;
    info.class_def_index = class_def_index;
    info.dex_method_index = dex_method_index;
    info.access_flags = method_access_flags;
    info.code_item = code_item;
    info.isa = oat_header.GetInstructionSet();
    info.deduped = !seen_offsets_.insert(oat_method.GetCodeOffset()).second;
    info.is_native_debuggable = oat_header.IsNativeDebuggable();
    info.is_optimized = method_header->IsOptimized();
    info.is_code_address_text_relative = true;
    info.code_address = reinterpret_cast<uintptr_t>(code_address);
    info.code_size = method_header->GetCodeSize();
    info.frame_size_in_bytes = method_header->GetFrameSizeInBytes();
    info.code_info = info.is_optimized ? method_header->GetOptimizedCodeInfoPtr() : nullptr;
    info.cfi = ArrayRef<uint8_t>();
    method_debug_infos_.push_back(info);
  }

 private:
  const OatFile* oat_file_;
  std::unique_ptr<linker::ElfBuilder<ElfTypes>> builder_;
  std::vector<debug::MethodDebugInfo> method_debug_infos_;
  std::unordered_set<uint32_t> seen_offsets_;
  const std::string output_name_;
  bool no_bits_;
};

class OatDumperOptions {
 public:
  OatDumperOptions(bool dump_vmap,
                   bool dump_code_info_stack_maps,
                   bool disassemble_code,
                   bool absolute_addresses,
                   const char* class_filter,
                   const char* method_filter,
                   bool list_classes,
                   bool list_methods,
                   bool dump_header_only,
                   const char* export_dex_location,
                   const char* app_image,
                   const char* app_oat,
                   uint32_t addr2instr)
    : dump_vmap_(dump_vmap),
      dump_code_info_stack_maps_(dump_code_info_stack_maps),
      disassemble_code_(disassemble_code),
      absolute_addresses_(absolute_addresses),
      class_filter_(class_filter),
      method_filter_(method_filter),
      list_classes_(list_classes),
      list_methods_(list_methods),
      dump_header_only_(dump_header_only),
      export_dex_location_(export_dex_location),
      app_image_(app_image),
      app_oat_(app_oat),
      addr2instr_(addr2instr),
      class_loader_(nullptr) {}

  const bool dump_vmap_;
  const bool dump_code_info_stack_maps_;
  const bool disassemble_code_;
  const bool absolute_addresses_;
  const char* const class_filter_;
  const char* const method_filter_;
  const bool list_classes_;
  const bool list_methods_;
  const bool dump_header_only_;
  const char* const export_dex_location_;
  const char* const app_image_;
  const char* const app_oat_;
  uint32_t addr2instr_;
  Handle<mirror::ClassLoader>* class_loader_;
};

class OatDumper {
 public:
  OatDumper(const OatFile& oat_file, const OatDumperOptions& options)
    : oat_file_(oat_file),
      oat_dex_files_(oat_file.GetOatDexFiles()),
      options_(options),
      resolved_addr2instr_(0),
      instruction_set_(oat_file_.GetOatHeader().GetInstructionSet()),
      disassembler_(Disassembler::Create(instruction_set_,
                                         new DisassemblerOptions(
                                             options_.absolute_addresses_,
                                             oat_file.Begin(),
                                             oat_file.End(),
                                             true /* can_read_literals_ */,
                                             Is64BitInstructionSet(instruction_set_)
                                                 ? &Thread::DumpThreadOffset<PointerSize::k64>
                                                 : &Thread::DumpThreadOffset<PointerSize::k32>))) {
    CHECK(options_.class_loader_ != nullptr);
    CHECK(options_.class_filter_ != nullptr);
    CHECK(options_.method_filter_ != nullptr);
    AddAllOffsets();
  }

  ~OatDumper() {
    delete disassembler_;
  }

  InstructionSet GetInstructionSet() {
    return instruction_set_;
  }

  typedef std::vector<std::unique_ptr<const DexFile>> DexFileUniqV;

  bool Dump(std::ostream& os) {
    bool success = true;
    const OatHeader& oat_header = oat_file_.GetOatHeader();

    os << "MAGIC:\n";
    os << oat_header.GetMagic() << "\n\n";

    os << "LOCATION:\n";
    os << oat_file_.GetLocation() << "\n\n";

    os << "CHECKSUM:\n";
    os << StringPrintf("0x%08x\n\n", oat_header.GetChecksum());

    os << "INSTRUCTION SET:\n";
    os << oat_header.GetInstructionSet() << "\n\n";

    {
      std::unique_ptr<const InstructionSetFeatures> features(
          InstructionSetFeatures::FromBitmap(oat_header.GetInstructionSet(),
                                             oat_header.GetInstructionSetFeaturesBitmap()));
      os << "INSTRUCTION SET FEATURES:\n";
      os << features->GetFeatureString() << "\n\n";
    }

    os << "DEX FILE COUNT:\n";
    os << oat_header.GetDexFileCount() << "\n\n";

#define DUMP_OAT_HEADER_OFFSET(label, offset) \
    os << label " OFFSET:\n"; \
    os << StringPrintf("0x%08x", oat_header.offset()); \
    if (oat_header.offset() != 0 && options_.absolute_addresses_) { \
      os << StringPrintf(" (%p)", oat_file_.Begin() + oat_header.offset()); \
    } \
    os << StringPrintf("\n\n");

    DUMP_OAT_HEADER_OFFSET("EXECUTABLE", GetExecutableOffset);
    DUMP_OAT_HEADER_OFFSET("INTERPRETER TO INTERPRETER BRIDGE",
                           GetInterpreterToInterpreterBridgeOffset);
    DUMP_OAT_HEADER_OFFSET("INTERPRETER TO COMPILED CODE BRIDGE",
                           GetInterpreterToCompiledCodeBridgeOffset);
    DUMP_OAT_HEADER_OFFSET("JNI DLSYM LOOKUP",
                           GetJniDlsymLookupOffset);
    DUMP_OAT_HEADER_OFFSET("QUICK GENERIC JNI TRAMPOLINE",
                           GetQuickGenericJniTrampolineOffset);
    DUMP_OAT_HEADER_OFFSET("QUICK IMT CONFLICT TRAMPOLINE",
                           GetQuickImtConflictTrampolineOffset);
    DUMP_OAT_HEADER_OFFSET("QUICK RESOLUTION TRAMPOLINE",
                           GetQuickResolutionTrampolineOffset);
    DUMP_OAT_HEADER_OFFSET("QUICK TO INTERPRETER BRIDGE",
                           GetQuickToInterpreterBridgeOffset);
#undef DUMP_OAT_HEADER_OFFSET

    os << "IMAGE PATCH DELTA:\n";
    os << StringPrintf("%d (0x%08x)\n\n",
                       oat_header.GetImagePatchDelta(),
                       oat_header.GetImagePatchDelta());

    os << "IMAGE FILE LOCATION OAT CHECKSUM:\n";
    os << StringPrintf("0x%08x\n\n", oat_header.GetImageFileLocationOatChecksum());

    os << "IMAGE FILE LOCATION OAT BEGIN:\n";
    os << StringPrintf("0x%08x\n\n", oat_header.GetImageFileLocationOatDataBegin());

    // Print the key-value store.
    {
      os << "KEY VALUE STORE:\n";
      size_t index = 0;
      const char* key;
      const char* value;
      while (oat_header.GetStoreKeyValuePairByIndex(index, &key, &value)) {
        os << key << " = " << value << "\n";
        index++;
      }
      os << "\n";
    }

    if (options_.absolute_addresses_) {
      os << "BEGIN:\n";
      os << reinterpret_cast<const void*>(oat_file_.Begin()) << "\n\n";

      os << "END:\n";
      os << reinterpret_cast<const void*>(oat_file_.End()) << "\n\n";
    }

    os << "SIZE:\n";
    os << oat_file_.Size() << "\n\n";

    os << std::flush;

    // If set, adjust relative address to be searched
    if (options_.addr2instr_ != 0) {
      resolved_addr2instr_ = options_.addr2instr_ + oat_header.GetExecutableOffset();
      os << "SEARCH ADDRESS (executable offset + input):\n";
      os << StringPrintf("0x%08x\n\n", resolved_addr2instr_);
    }

    // Dumping the dex file overview is compact enough to do even if header only.
    DexFileData cumulative;
    for (size_t i = 0; i < oat_dex_files_.size(); i++) {
      const OatFile::OatDexFile* oat_dex_file = oat_dex_files_[i];
      CHECK(oat_dex_file != nullptr);
      std::string error_msg;
      const DexFile* const dex_file = OpenDexFile(oat_dex_file, &error_msg);
      if (dex_file == nullptr) {
        os << "Failed to open dex file '" << oat_dex_file->GetDexFileLocation() << "': "
           << error_msg;
        continue;
      }
      DexFileData data(*dex_file);
      os << "Dex file data for " << dex_file->GetLocation() << "\n";
      data.Dump(os);
      os << "\n";
      const DexLayoutSections* const layout_sections = oat_dex_file->GetDexLayoutSections();
      if (layout_sections != nullptr) {
        os << "Layout data\n";
        os << *layout_sections;
        os << "\n";
      }

      cumulative.Add(data);

      // Dump .bss entries.
      DumpBssEntries(
          os,
          "ArtMethod",
          oat_dex_file->GetMethodBssMapping(),
          dex_file->NumMethodIds(),
          static_cast<size_t>(GetInstructionSetPointerSize(instruction_set_)),
          [=](uint32_t index) { return dex_file->PrettyMethod(index); });
      DumpBssEntries(
          os,
          "Class",
          oat_dex_file->GetTypeBssMapping(),
          dex_file->NumTypeIds(),
          sizeof(GcRoot<mirror::Class>),
          [=](uint32_t index) { return dex_file->PrettyType(dex::TypeIndex(index)); });
      DumpBssEntries(
          os,
          "String",
          oat_dex_file->GetStringBssMapping(),
          dex_file->NumStringIds(),
          sizeof(GcRoot<mirror::Class>),
          [=](uint32_t index) { return dex_file->StringDataByIdx(dex::StringIndex(index)); });
    }
    os << "Cumulative dex file data\n";
    cumulative.Dump(os);
    os << "\n";

    if (!options_.dump_header_only_) {
      VariableIndentationOutputStream vios(&os);
      VdexFile::VerifierDepsHeader vdex_header = oat_file_.GetVdexFile()->GetVerifierDepsHeader();
      if (vdex_header.IsValid()) {
        std::string error_msg;
        std::vector<const DexFile*> dex_files;
        for (size_t i = 0; i < oat_dex_files_.size(); i++) {
          const DexFile* dex_file = OpenDexFile(oat_dex_files_[i], &error_msg);
          if (dex_file == nullptr) {
            os << "Error opening dex file: " << error_msg << std::endl;
            return false;
          }
          dex_files.push_back(dex_file);
        }
        verifier::VerifierDeps deps(dex_files, oat_file_.GetVdexFile()->GetVerifierDepsData());
        deps.Dump(&vios);
      } else {
        os << "UNRECOGNIZED vdex file, magic "
           << vdex_header.GetMagic()
           << ", verifier deps version "
           << vdex_header.GetVerifierDepsVersion()
           << ", dex section version "
           << vdex_header.GetDexSectionVersion()
           << "\n";
      }
      for (size_t i = 0; i < oat_dex_files_.size(); i++) {
        const OatFile::OatDexFile* oat_dex_file = oat_dex_files_[i];
        CHECK(oat_dex_file != nullptr);
        if (!DumpOatDexFile(os, *oat_dex_file)) {
          success = false;
        }
      }
    }

    if (options_.export_dex_location_) {
      std::string error_msg;
      std::string vdex_filename = GetVdexFilename(oat_file_.GetLocation());
      if (!OS::FileExists(vdex_filename.c_str())) {
        os << "File " << vdex_filename.c_str() << " does not exist\n";
        return false;
      }

      DexFileUniqV vdex_dex_files;
      std::unique_ptr<const VdexFile> vdex_file = OpenVdexUnquicken(vdex_filename,
                                                                    &vdex_dex_files,
                                                                    &error_msg);
      if (vdex_file.get() == nullptr) {
        os << "Failed to open vdex file: " << error_msg << "\n";
        return false;
      }
      if (oat_dex_files_.size() != vdex_dex_files.size()) {
        os << "Dex files number in Vdex file does not match Dex files number in Oat file: "
           << vdex_dex_files.size() << " vs " << oat_dex_files_.size() << '\n';
        return false;
      }

      size_t i = 0;
      for (const auto& vdex_dex_file : vdex_dex_files) {
        const OatFile::OatDexFile* oat_dex_file = oat_dex_files_[i];
        CHECK(oat_dex_file != nullptr);
        CHECK(vdex_dex_file != nullptr);
        if (!ExportDexFile(os, *oat_dex_file, vdex_dex_file.get())) {
          success = false;
        }
        i++;
      }
    }

    {
      os << "OAT FILE STATS:\n";
      VariableIndentationOutputStream vios(&os);
      stats_.Dump(vios);
    }

    os << std::flush;
    return success;
  }

  size_t ComputeSize(const void* oat_data) {
    if (reinterpret_cast<const uint8_t*>(oat_data) < oat_file_.Begin() ||
        reinterpret_cast<const uint8_t*>(oat_data) > oat_file_.End()) {
      return 0;  // Address not in oat file
    }
    uintptr_t begin_offset = reinterpret_cast<uintptr_t>(oat_data) -
                             reinterpret_cast<uintptr_t>(oat_file_.Begin());
    auto it = offsets_.upper_bound(begin_offset);
    CHECK(it != offsets_.end());
    uintptr_t end_offset = *it;
    return end_offset - begin_offset;
  }

  InstructionSet GetOatInstructionSet() {
    return oat_file_.GetOatHeader().GetInstructionSet();
  }

  const void* GetQuickOatCode(ArtMethod* m) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < oat_dex_files_.size(); i++) {
      const OatFile::OatDexFile* oat_dex_file = oat_dex_files_[i];
      CHECK(oat_dex_file != nullptr);
      std::string error_msg;
      const DexFile* const dex_file = OpenDexFile(oat_dex_file, &error_msg);
      if (dex_file == nullptr) {
        LOG(WARNING) << "Failed to open dex file '" << oat_dex_file->GetDexFileLocation()
            << "': " << error_msg;
      } else {
        const char* descriptor = m->GetDeclaringClassDescriptor();
        const DexFile::ClassDef* class_def =
            OatDexFile::FindClassDef(*dex_file, descriptor, ComputeModifiedUtf8Hash(descriptor));
        if (class_def != nullptr) {
          uint16_t class_def_index = dex_file->GetIndexForClassDef(*class_def);
          const OatFile::OatClass oat_class = oat_dex_file->GetOatClass(class_def_index);
          size_t method_index = m->GetMethodIndex();
          return oat_class.GetOatMethod(method_index).GetQuickCode();
        }
      }
    }
    return nullptr;
  }

  // Returns nullptr and updates error_msg if the Vdex file cannot be opened, otherwise all Dex
  // files are fully unquickened and stored in dex_files
  std::unique_ptr<const VdexFile> OpenVdexUnquicken(const std::string& vdex_filename,
                                                    /* out */ DexFileUniqV* dex_files,
                                                    /* out */ std::string* error_msg) {
    std::unique_ptr<const File> file(OS::OpenFileForReading(vdex_filename.c_str()));
    if (file == nullptr) {
      *error_msg = "Could not open file " + vdex_filename + " for reading.";
      return nullptr;
    }

    int64_t vdex_length = file->GetLength();
    if (vdex_length == -1) {
      *error_msg = "Could not read the length of file " + vdex_filename;
      return nullptr;
    }

    std::unique_ptr<MemMap> mmap(MemMap::MapFile(
        file->GetLength(),
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE,
        file->Fd(),
        /* start offset */ 0,
        /* low_4gb */ false,
        vdex_filename.c_str(),
        error_msg));
    if (mmap == nullptr) {
      *error_msg = "Failed to mmap file " + vdex_filename + ": " + *error_msg;
      return nullptr;
    }

    std::unique_ptr<VdexFile> vdex_file(new VdexFile(mmap.release()));
    if (!vdex_file->IsValid()) {
      *error_msg = "Vdex file is not valid";
      return nullptr;
    }

    DexFileUniqV tmp_dex_files;
    if (!vdex_file->OpenAllDexFiles(&tmp_dex_files, error_msg)) {
      *error_msg = "Failed to open Dex files from Vdex: " + *error_msg;
      return nullptr;
    }

    vdex_file->Unquicken(MakeNonOwningPointerVector(tmp_dex_files),
                         /* decompile_return_instruction */ true);

    *dex_files = std::move(tmp_dex_files);
    return vdex_file;
  }

  struct Stats {
    enum ByteKind {
      kByteKindCode,
      kByteKindQuickMethodHeader,
      kByteKindCodeInfoLocationCatalog,
      kByteKindCodeInfoDexRegisterMap,
      kByteKindCodeInfoEncoding,
      kByteKindCodeInfoInvokeInfo,
      kByteKindCodeInfoStackMasks,
      kByteKindCodeInfoRegisterMasks,
      kByteKindStackMapNativePc,
      kByteKindStackMapDexPc,
      kByteKindStackMapDexRegisterMap,
      kByteKindStackMapInlineInfoIndex,
      kByteKindStackMapRegisterMaskIndex,
      kByteKindStackMapStackMaskIndex,
      kByteKindInlineInfoMethodIndexIdx,
      kByteKindInlineInfoDexPc,
      kByteKindInlineInfoExtraData,
      kByteKindInlineInfoDexRegisterMap,
      kByteKindInlineInfoIsLast,
      kByteKindCount,
      // Special ranges for std::accumulate convenience.
      kByteKindStackMapFirst = kByteKindStackMapNativePc,
      kByteKindStackMapLast = kByteKindStackMapStackMaskIndex,
      kByteKindInlineInfoFirst = kByteKindInlineInfoMethodIndexIdx,
      kByteKindInlineInfoLast = kByteKindInlineInfoIsLast,
    };
    int64_t bits[kByteKindCount] = {};
    // Since code has deduplication, seen tracks already seen pointers to avoid double counting
    // deduplicated code and tables.
    std::unordered_set<const void*> seen;

    // Returns true if it was newly added.
    bool AddBitsIfUnique(ByteKind kind, int64_t count, const void* address) {
      if (seen.insert(address).second == true) {
        // True means the address was not already in the set.
        AddBits(kind, count);
        return true;
      }
      return false;
    }

    void AddBits(ByteKind kind, int64_t count) {
      bits[kind] += count;
    }

    void Dump(VariableIndentationOutputStream& os) {
      const int64_t sum = std::accumulate(bits, bits + kByteKindCount, 0u);
      os.Stream() << "Dumping cumulative use of " << sum / kBitsPerByte << " accounted bytes\n";
      if (sum > 0) {
        Dump(os, "Code                            ", bits[kByteKindCode], sum);
        Dump(os, "QuickMethodHeader               ", bits[kByteKindQuickMethodHeader], sum);
        Dump(os, "CodeInfoEncoding                ", bits[kByteKindCodeInfoEncoding], sum);
        Dump(os, "CodeInfoLocationCatalog         ", bits[kByteKindCodeInfoLocationCatalog], sum);
        Dump(os, "CodeInfoDexRegisterMap          ", bits[kByteKindCodeInfoDexRegisterMap], sum);
        Dump(os, "CodeInfoStackMasks              ", bits[kByteKindCodeInfoStackMasks], sum);
        Dump(os, "CodeInfoRegisterMasks           ", bits[kByteKindCodeInfoRegisterMasks], sum);
        Dump(os, "CodeInfoInvokeInfo              ", bits[kByteKindCodeInfoInvokeInfo], sum);
        // Stack map section.
        const int64_t stack_map_bits = std::accumulate(bits + kByteKindStackMapFirst,
                                                       bits + kByteKindStackMapLast + 1,
                                                       0u);
        Dump(os, "CodeInfoStackMap                ", stack_map_bits, sum);
        {
          ScopedIndentation indent1(&os);
          Dump(os,
               "StackMapNativePc              ",
               bits[kByteKindStackMapNativePc],
               stack_map_bits,
               "stack map");
          Dump(os,
               "StackMapDexPcEncoding         ",
               bits[kByteKindStackMapDexPc],
               stack_map_bits,
               "stack map");
          Dump(os,
               "StackMapDexRegisterMap        ",
               bits[kByteKindStackMapDexRegisterMap],
               stack_map_bits,
               "stack map");
          Dump(os,
               "StackMapInlineInfoIndex       ",
               bits[kByteKindStackMapInlineInfoIndex],
               stack_map_bits,
               "stack map");
          Dump(os,
               "StackMapRegisterMaskIndex     ",
               bits[kByteKindStackMapRegisterMaskIndex],
               stack_map_bits,
               "stack map");
          Dump(os,
               "StackMapStackMaskIndex        ",
               bits[kByteKindStackMapStackMaskIndex],
               stack_map_bits,
               "stack map");
        }
        // Inline info section.
        const int64_t inline_info_bits = std::accumulate(bits + kByteKindInlineInfoFirst,
                                                         bits + kByteKindInlineInfoLast + 1,
                                                         0u);
        Dump(os, "CodeInfoInlineInfo              ", inline_info_bits, sum);
        {
          ScopedIndentation indent1(&os);
          Dump(os,
               "InlineInfoMethodIndexIdx      ",
               bits[kByteKindInlineInfoMethodIndexIdx],
               inline_info_bits,
               "inline info");
          Dump(os,
               "InlineInfoDexPc               ",
               bits[kByteKindStackMapDexPc],
               inline_info_bits,
               "inline info");
          Dump(os,
               "InlineInfoExtraData           ",
               bits[kByteKindInlineInfoExtraData],
               inline_info_bits,
               "inline info");
          Dump(os,
               "InlineInfoDexRegisterMap      ",
               bits[kByteKindInlineInfoDexRegisterMap],
               inline_info_bits,
               "inline info");
          Dump(os,
               "InlineInfoIsLast              ",
               bits[kByteKindInlineInfoIsLast],
               inline_info_bits,
               "inline info");
        }
      }
      os.Stream() << "\n" << std::flush;
    }

   private:
    void Dump(VariableIndentationOutputStream& os,
              const char* name,
              int64_t size,
              int64_t total,
              const char* sum_of = "total") {
      const double percent = (static_cast<double>(size) / static_cast<double>(total)) * 100;
      os.Stream() << StringPrintf("%s = %8" PRId64 " (%2.0f%% of %s)\n",
                                  name,
                                  size / kBitsPerByte,
                                  percent,
                                  sum_of);
    }
  };

 private:
  void AddAllOffsets() {
    // We don't know the length of the code for each method, but we need to know where to stop
    // when disassembling. What we do know is that a region of code will be followed by some other
    // region, so if we keep a sorted sequence of the start of each region, we can infer the length
    // of a piece of code by using upper_bound to find the start of the next region.
    for (size_t i = 0; i < oat_dex_files_.size(); i++) {
      const OatFile::OatDexFile* oat_dex_file = oat_dex_files_[i];
      CHECK(oat_dex_file != nullptr);
      std::string error_msg;
      const DexFile* const dex_file = OpenDexFile(oat_dex_file, &error_msg);
      if (dex_file == nullptr) {
        LOG(WARNING) << "Failed to open dex file '" << oat_dex_file->GetDexFileLocation()
            << "': " << error_msg;
        continue;
      }
      offsets_.insert(reinterpret_cast<uintptr_t>(&dex_file->GetHeader()));
      for (size_t class_def_index = 0;
           class_def_index < dex_file->NumClassDefs();
           class_def_index++) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
        const OatFile::OatClass oat_class = oat_dex_file->GetOatClass(class_def_index);
        const uint8_t* class_data = dex_file->GetClassData(class_def);
        if (class_data != nullptr) {
          ClassDataItemIterator it(*dex_file, class_data);
          it.SkipAllFields();
          uint32_t class_method_index = 0;
          while (it.HasNextMethod()) {
            AddOffsets(oat_class.GetOatMethod(class_method_index++));
            it.Next();
          }
        }
      }
    }

    // If the last thing in the file is code for a method, there won't be an offset for the "next"
    // thing. Instead of having a special case in the upper_bound code, let's just add an entry
    // for the end of the file.
    offsets_.insert(oat_file_.Size());
  }

  static uint32_t AlignCodeOffset(uint32_t maybe_thumb_offset) {
    return maybe_thumb_offset & ~0x1;  // TODO: Make this Thumb2 specific.
  }

  void AddOffsets(const OatFile::OatMethod& oat_method) {
    uint32_t code_offset = oat_method.GetCodeOffset();
    if (oat_file_.GetOatHeader().GetInstructionSet() == InstructionSet::kThumb2) {
      code_offset &= ~0x1;
    }
    offsets_.insert(code_offset);
    offsets_.insert(oat_method.GetVmapTableOffset());
  }

  // Dex file data, may be for multiple different dex files.
  class DexFileData {
   public:
    DexFileData() {}

    explicit DexFileData(const DexFile& dex_file)
        : num_string_ids_(dex_file.NumStringIds()),
          num_method_ids_(dex_file.NumMethodIds()),
          num_field_ids_(dex_file.NumFieldIds()),
          num_type_ids_(dex_file.NumTypeIds()),
          num_class_defs_(dex_file.NumClassDefs()) {
      for (size_t class_def_index = 0; class_def_index < num_class_defs_; ++class_def_index) {
        const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
        WalkClass(dex_file, class_def);
      }
    }

    void Add(const DexFileData& other) {
      AddAll(unique_string_ids_from_code_, other.unique_string_ids_from_code_);
      num_string_ids_from_code_ += other.num_string_ids_from_code_;
      AddAll(dex_code_item_ptrs_, other.dex_code_item_ptrs_);
      dex_code_bytes_ += other.dex_code_bytes_;
      num_string_ids_ += other.num_string_ids_;
      num_method_ids_ += other.num_method_ids_;
      num_field_ids_ += other.num_field_ids_;
      num_type_ids_ += other.num_type_ids_;
      num_class_defs_ += other.num_class_defs_;
    }

    void Dump(std::ostream& os) {
      os << "Num string ids: " << num_string_ids_ << "\n";
      os << "Num method ids: " << num_method_ids_ << "\n";
      os << "Num field ids: " << num_field_ids_ << "\n";
      os << "Num type ids: " << num_type_ids_ << "\n";
      os << "Num class defs: " << num_class_defs_ << "\n";
      os << "Unique strings loaded from dex code: " << unique_string_ids_from_code_.size() << "\n";
      os << "Total strings loaded from dex code: " << num_string_ids_from_code_ << "\n";
      os << "Number of unique dex code items: " << dex_code_item_ptrs_.size() << "\n";
      os << "Total number of dex code bytes: " << dex_code_bytes_ << "\n";
    }

   private:
    // All of the elements from one container to another.
    template <typename Dest, typename Src>
    static void AddAll(Dest& dest, const Src& src) {
      dest.insert(src.begin(), src.end());
    }

    void WalkClass(const DexFile& dex_file, const DexFile::ClassDef& class_def) {
      const uint8_t* class_data = dex_file.GetClassData(class_def);
      if (class_data == nullptr) {  // empty class such as a marker interface?
        return;
      }
      ClassDataItemIterator it(dex_file, class_data);
      it.SkipAllFields();
      while (it.HasNextMethod()) {
        WalkCodeItem(dex_file, it.GetMethodCodeItem());
        it.Next();
      }
      DCHECK(!it.HasNext());
    }

    void WalkCodeItem(const DexFile& dex_file, const DexFile::CodeItem* code_item) {
      if (code_item == nullptr) {
        return;
      }
      CodeItemInstructionAccessor instructions(dex_file, code_item);

      // If we inserted a new dex code item pointer, add to total code bytes.
      const uint16_t* code_ptr = instructions.Insns();
      if (dex_code_item_ptrs_.insert(code_ptr).second) {
        dex_code_bytes_ += instructions.InsnsSizeInCodeUnits() * sizeof(code_ptr[0]);
      }

      for (const DexInstructionPcPair& inst : instructions) {
        switch (inst->Opcode()) {
          case Instruction::CONST_STRING: {
            const dex::StringIndex string_index(inst->VRegB_21c());
            unique_string_ids_from_code_.insert(StringReference(&dex_file, string_index));
            ++num_string_ids_from_code_;
            break;
          }
          case Instruction::CONST_STRING_JUMBO: {
            const dex::StringIndex string_index(inst->VRegB_31c());
            unique_string_ids_from_code_.insert(StringReference(&dex_file, string_index));
            ++num_string_ids_from_code_;
            break;
          }
          default:
            break;
        }
      }
    }

    // Unique string ids loaded from dex code.
    std::set<StringReference> unique_string_ids_from_code_;

    // Total string ids loaded from dex code.
    size_t num_string_ids_from_code_ = 0;

    // Unique code pointers.
    std::set<const void*> dex_code_item_ptrs_;

    // Total "unique" dex code bytes.
    size_t dex_code_bytes_ = 0;

    // Other dex ids.
    size_t num_string_ids_ = 0;
    size_t num_method_ids_ = 0;
    size_t num_field_ids_ = 0;
    size_t num_type_ids_ = 0;
    size_t num_class_defs_ = 0;
  };

  bool DumpOatDexFile(std::ostream& os, const OatFile::OatDexFile& oat_dex_file) {
    bool success = true;
    bool stop_analysis = false;
    os << "OatDexFile:\n";
    os << StringPrintf("location: %s\n", oat_dex_file.GetDexFileLocation().c_str());
    os << StringPrintf("checksum: 0x%08x\n", oat_dex_file.GetDexFileLocationChecksum());

    const uint8_t* const oat_file_begin = oat_dex_file.GetOatFile()->Begin();
    if (oat_dex_file.GetOatFile()->ContainsDexCode()) {
      const uint8_t* const vdex_file_begin = oat_dex_file.GetOatFile()->DexBegin();

      // Print data range of the dex file embedded inside the corresponding vdex file.
      const uint8_t* const dex_file_pointer = oat_dex_file.GetDexFilePointer();
      uint32_t dex_offset = dchecked_integral_cast<uint32_t>(dex_file_pointer - vdex_file_begin);
      os << StringPrintf(
          "dex-file: 0x%08x..0x%08x\n",
          dex_offset,
          dchecked_integral_cast<uint32_t>(dex_offset + oat_dex_file.FileSize() - 1));
    } else {
      os << StringPrintf("dex-file not in VDEX file\n");
    }

    // Create the dex file early. A lot of print-out things depend on it.
    std::string error_msg;
    const DexFile* const dex_file = OpenDexFile(&oat_dex_file, &error_msg);
    if (dex_file == nullptr) {
      os << "NOT FOUND: " << error_msg << "\n\n";
      os << std::flush;
      return false;
    }

    // Print lookup table, if it exists.
    if (oat_dex_file.GetLookupTableData() != nullptr) {
      uint32_t table_offset = dchecked_integral_cast<uint32_t>(
          oat_dex_file.GetLookupTableData() - oat_file_begin);
      uint32_t table_size = TypeLookupTable::RawDataLength(dex_file->NumClassDefs());
      os << StringPrintf("type-table: 0x%08x..0x%08x\n",
                         table_offset,
                         table_offset + table_size - 1);
    }

    VariableIndentationOutputStream vios(&os);
    ScopedIndentation indent1(&vios);
    for (size_t class_def_index = 0;
         class_def_index < dex_file->NumClassDefs();
         class_def_index++) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
      const char* descriptor = dex_file->GetClassDescriptor(class_def);

      // TODO: Support regex
      if (DescriptorToDot(descriptor).find(options_.class_filter_) == std::string::npos) {
        continue;
      }

      uint32_t oat_class_offset = oat_dex_file.GetOatClassOffset(class_def_index);
      const OatFile::OatClass oat_class = oat_dex_file.GetOatClass(class_def_index);
      os << StringPrintf("%zd: %s (offset=0x%08x) (type_idx=%d)",
                         class_def_index, descriptor, oat_class_offset, class_def.class_idx_.index_)
         << " (" << oat_class.GetStatus() << ")"
         << " (" << oat_class.GetType() << ")\n";
      // TODO: include bitmap here if type is kOatClassSomeCompiled?
      if (options_.list_classes_) {
        continue;
      }
      if (!DumpOatClass(&vios, oat_class, *dex_file, class_def, &stop_analysis)) {
        success = false;
      }
      if (stop_analysis) {
        os << std::flush;
        return success;
      }
    }
    os << "\n";
    os << std::flush;
    return success;
  }

  // Backwards compatible Dex file export. If dex_file is nullptr (valid Vdex file not present) the
  // Dex resource is extracted from the oat_dex_file and its checksum is repaired since it's not
  // unquickened. Otherwise the dex_file has been fully unquickened and is expected to verify the
  // original checksum.
  bool ExportDexFile(std::ostream& os,
                     const OatFile::OatDexFile& oat_dex_file,
                     const DexFile* dex_file) {
    std::string error_msg;
    std::string dex_file_location = oat_dex_file.GetDexFileLocation();
    size_t fsize = oat_dex_file.FileSize();

    // Some quick checks just in case
    if (fsize == 0 || fsize < sizeof(DexFile::Header)) {
      os << "Invalid dex file\n";
      return false;
    }

    if (dex_file == nullptr) {
      // Exported bytecode is quickened (dex-to-dex transformations present)
      dex_file = OpenDexFile(&oat_dex_file, &error_msg);
      if (dex_file == nullptr) {
        os << "Failed to open dex file '" << dex_file_location << "': " << error_msg;
        return false;
      }

      // Recompute checksum
      reinterpret_cast<DexFile::Header*>(const_cast<uint8_t*>(dex_file->Begin()))->checksum_ =
          dex_file->CalculateChecksum();
    } else {
      // Vdex unquicken output should match original input bytecode
      uint32_t orig_checksum =
          reinterpret_cast<DexFile::Header*>(const_cast<uint8_t*>(dex_file->Begin()))->checksum_;
      CHECK_EQ(orig_checksum, dex_file->CalculateChecksum());
      if (orig_checksum != dex_file->CalculateChecksum()) {
        os << "Unexpected checksum from unquicken dex file '" << dex_file_location << "'\n";
        return false;
      }
    }

    // Update header for shared section.
    uint32_t shared_section_offset = 0u;
    uint32_t shared_section_size = 0u;
    if (dex_file->IsCompactDexFile()) {
      CompactDexFile::Header* const header =
          reinterpret_cast<CompactDexFile::Header*>(const_cast<uint8_t*>(dex_file->Begin()));
      shared_section_offset = header->data_off_;
      shared_section_size = header->data_size_;
      // The shared section will be serialized right after the dex file.
      header->data_off_ = header->file_size_;
    }
    // Verify output directory exists
    if (!OS::DirectoryExists(options_.export_dex_location_)) {
      // TODO: Extend OS::DirectoryExists if symlink support is required
      os << options_.export_dex_location_ << " output directory not found or symlink\n";
      return false;
    }

    // Beautify path names
    if (dex_file_location.size() > PATH_MAX || dex_file_location.size() <= 0) {
      return false;
    }

    std::string dex_orig_name;
    size_t dex_orig_pos = dex_file_location.rfind('/');
    if (dex_orig_pos == std::string::npos)
      dex_orig_name = dex_file_location;
    else
      dex_orig_name = dex_file_location.substr(dex_orig_pos + 1);

    // A more elegant approach to efficiently name user installed apps is welcome
    if (dex_orig_name.size() == 8 &&
        dex_orig_name.compare("base.apk") == 0 &&
        dex_orig_pos != std::string::npos) {
      dex_file_location.erase(dex_orig_pos, strlen("base.apk") + 1);
      size_t apk_orig_pos = dex_file_location.rfind('/');
      if (apk_orig_pos != std::string::npos) {
        dex_orig_name = dex_file_location.substr(++apk_orig_pos);
      }
    }

    std::string out_dex_path(options_.export_dex_location_);
    if (out_dex_path.back() != '/') {
      out_dex_path.append("/");
    }
    out_dex_path.append(dex_orig_name);
    out_dex_path.append("_export.dex");
    if (out_dex_path.length() > PATH_MAX) {
      return false;
    }

    std::unique_ptr<File> file(OS::CreateEmptyFile(out_dex_path.c_str()));
    if (file.get() == nullptr) {
      os << "Failed to open output dex file " << out_dex_path;
      return false;
    }

    bool success = file->WriteFully(dex_file->Begin(), fsize);
    if (!success) {
      os << "Failed to write dex file";
      file->Erase();
      return false;
    }

    if (shared_section_size != 0) {
      success = file->WriteFully(dex_file->Begin() + shared_section_offset, shared_section_size);
      if (!success) {
        os << "Failed to write shared data section";
        file->Erase();
        return false;
      }
    }

    if (file->FlushCloseOrErase() != 0) {
      os << "Flush and close failed";
      return false;
    }

    os << StringPrintf("Dex file exported at %s (%zd bytes)\n", out_dex_path.c_str(), fsize);
    os << std::flush;

    return true;
  }

  bool DumpOatClass(VariableIndentationOutputStream* vios,
                    const OatFile::OatClass& oat_class, const DexFile& dex_file,
                    const DexFile::ClassDef& class_def, bool* stop_analysis) {
    bool success = true;
    bool addr_found = false;
    const uint8_t* class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {  // empty class such as a marker interface?
      vios->Stream() << std::flush;
      return success;
    }
    ClassDataItemIterator it(dex_file, class_data);
    it.SkipAllFields();
    uint32_t class_method_index = 0;
    while (it.HasNextMethod()) {
      if (!DumpOatMethod(vios, class_def, class_method_index, oat_class, dex_file,
                         it.GetMemberIndex(), it.GetMethodCodeItem(),
                         it.GetRawMemberAccessFlags(), &addr_found)) {
        success = false;
      }
      if (addr_found) {
        *stop_analysis = true;
        return success;
      }
      class_method_index++;
      it.Next();
    }
    DCHECK(!it.HasNext());
    vios->Stream() << std::flush;
    return success;
  }

  static constexpr uint32_t kPrologueBytes = 16;

  // When this was picked, the largest arm method was 55,256 bytes and arm64 was 50,412 bytes.
  static constexpr uint32_t kMaxCodeSize = 100 * 1000;

  bool DumpOatMethod(VariableIndentationOutputStream* vios,
                     const DexFile::ClassDef& class_def,
                     uint32_t class_method_index,
                     const OatFile::OatClass& oat_class,
                     const DexFile& dex_file,
                     uint32_t dex_method_idx,
                     const DexFile::CodeItem* code_item,
                     uint32_t method_access_flags,
                     bool* addr_found) {
    bool success = true;

    CodeItemDataAccessor code_item_accessor(dex_file, code_item);

    // TODO: Support regex
    std::string method_name = dex_file.GetMethodName(dex_file.GetMethodId(dex_method_idx));
    if (method_name.find(options_.method_filter_) == std::string::npos) {
      return success;
    }

    std::string pretty_method = dex_file.PrettyMethod(dex_method_idx, true);
    vios->Stream() << StringPrintf("%d: %s (dex_method_idx=%d)\n",
                                   class_method_index, pretty_method.c_str(),
                                   dex_method_idx);
    if (options_.list_methods_) {
      return success;
    }

    uint32_t oat_method_offsets_offset = oat_class.GetOatMethodOffsetsOffset(class_method_index);
    const OatMethodOffsets* oat_method_offsets = oat_class.GetOatMethodOffsets(class_method_index);
    const OatFile::OatMethod oat_method = oat_class.GetOatMethod(class_method_index);
    uint32_t code_offset = oat_method.GetCodeOffset();
    uint32_t code_size = oat_method.GetQuickCodeSize();
    if (resolved_addr2instr_ != 0) {
      if (resolved_addr2instr_ > code_offset + code_size) {
        return success;
      } else {
        *addr_found = true;  // stop analyzing file at next iteration
      }
    }

    // Everything below is indented at least once.
    ScopedIndentation indent1(vios);

    {
      vios->Stream() << "DEX CODE:\n";
      ScopedIndentation indent2(vios);
      if (code_item_accessor.HasCodeItem()) {
        for (const DexInstructionPcPair& inst : code_item_accessor) {
          vios->Stream() << StringPrintf("0x%04x: ", inst.DexPc()) << inst->DumpHexLE(5)
                         << StringPrintf("\t| %s\n", inst->DumpString(&dex_file).c_str());
        }
      }
    }

    std::unique_ptr<StackHandleScope<1>> hs;
    std::unique_ptr<verifier::MethodVerifier> verifier;
    if (Runtime::Current() != nullptr) {
      // We need to have the handle scope stay live until after the verifier since the verifier has
      // a handle to the dex cache from hs.
      hs.reset(new StackHandleScope<1>(Thread::Current()));
      vios->Stream() << "VERIFIER TYPE ANALYSIS:\n";
      ScopedIndentation indent2(vios);
      verifier.reset(DumpVerifier(vios, hs.get(),
                                  dex_method_idx, &dex_file, class_def, code_item,
                                  method_access_flags));
    }
    {
      vios->Stream() << "OatMethodOffsets ";
      if (options_.absolute_addresses_) {
        vios->Stream() << StringPrintf("%p ", oat_method_offsets);
      }
      vios->Stream() << StringPrintf("(offset=0x%08x)\n", oat_method_offsets_offset);
      if (oat_method_offsets_offset > oat_file_.Size()) {
        vios->Stream() << StringPrintf(
            "WARNING: oat method offsets offset 0x%08x is past end of file 0x%08zx.\n",
            oat_method_offsets_offset, oat_file_.Size());
        // If we can't read OatMethodOffsets, the rest of the data is dangerous to read.
        vios->Stream() << std::flush;
        return false;
      }

      ScopedIndentation indent2(vios);
      vios->Stream() << StringPrintf("code_offset: 0x%08x ", code_offset);
      uint32_t aligned_code_begin = AlignCodeOffset(oat_method.GetCodeOffset());
      if (aligned_code_begin > oat_file_.Size()) {
        vios->Stream() << StringPrintf("WARNING: "
                                       "code offset 0x%08x is past end of file 0x%08zx.\n",
                                       aligned_code_begin, oat_file_.Size());
        success = false;
      }
      vios->Stream() << "\n";
    }
    {
      vios->Stream() << "OatQuickMethodHeader ";
      uint32_t method_header_offset = oat_method.GetOatQuickMethodHeaderOffset();
      const OatQuickMethodHeader* method_header = oat_method.GetOatQuickMethodHeader();
      stats_.AddBitsIfUnique(Stats::kByteKindQuickMethodHeader,
                             sizeof(*method_header) * kBitsPerByte,
                             method_header);
      if (options_.absolute_addresses_) {
        vios->Stream() << StringPrintf("%p ", method_header);
      }
      vios->Stream() << StringPrintf("(offset=0x%08x)\n", method_header_offset);
      if (method_header_offset > oat_file_.Size()) {
        vios->Stream() << StringPrintf(
            "WARNING: oat quick method header offset 0x%08x is past end of file 0x%08zx.\n",
            method_header_offset, oat_file_.Size());
        // If we can't read the OatQuickMethodHeader, the rest of the data is dangerous to read.
        vios->Stream() << std::flush;
        return false;
      }

      ScopedIndentation indent2(vios);
      vios->Stream() << "vmap_table: ";
      if (options_.absolute_addresses_) {
        vios->Stream() << StringPrintf("%p ", oat_method.GetVmapTable());
      }
      uint32_t vmap_table_offset = method_header ==
          nullptr ? 0 : method_header->GetVmapTableOffset();
      vios->Stream() << StringPrintf("(offset=0x%08x)\n", vmap_table_offset);

      size_t vmap_table_offset_limit =
          IsMethodGeneratedByDexToDexCompiler(oat_method, code_item_accessor)
              ? oat_file_.GetVdexFile()->Size()
              : method_header->GetCode() - oat_file_.Begin();
      if (vmap_table_offset >= vmap_table_offset_limit) {
        vios->Stream() << StringPrintf("WARNING: "
                                       "vmap table offset 0x%08x is past end of file 0x%08zx. "
                                       "vmap table offset was loaded from offset 0x%08x.\n",
                                       vmap_table_offset,
                                       vmap_table_offset_limit,
                                       oat_method.GetVmapTableOffsetOffset());
        success = false;
      } else if (options_.dump_vmap_) {
        DumpVmapData(vios, oat_method, code_item_accessor);
      }
    }
    {
      vios->Stream() << "QuickMethodFrameInfo\n";

      ScopedIndentation indent2(vios);
      vios->Stream()
          << StringPrintf("frame_size_in_bytes: %zd\n", oat_method.GetFrameSizeInBytes());
      vios->Stream() << StringPrintf("core_spill_mask: 0x%08x ", oat_method.GetCoreSpillMask());
      DumpSpillMask(vios->Stream(), oat_method.GetCoreSpillMask(), false);
      vios->Stream() << "\n";
      vios->Stream() << StringPrintf("fp_spill_mask: 0x%08x ", oat_method.GetFpSpillMask());
      DumpSpillMask(vios->Stream(), oat_method.GetFpSpillMask(), true);
      vios->Stream() << "\n";
    }
    {
      // Based on spill masks from QuickMethodFrameInfo so placed
      // after it is dumped, but useful for understanding quick
      // code, so dumped here.
      ScopedIndentation indent2(vios);
      DumpVregLocations(vios->Stream(), oat_method, code_item_accessor);
    }
    {
      vios->Stream() << "CODE: ";
      uint32_t code_size_offset = oat_method.GetQuickCodeSizeOffset();
      if (code_size_offset > oat_file_.Size()) {
        ScopedIndentation indent2(vios);
        vios->Stream() << StringPrintf("WARNING: "
                                       "code size offset 0x%08x is past end of file 0x%08zx.",
                                       code_size_offset, oat_file_.Size());
        success = false;
      } else {
        const void* code = oat_method.GetQuickCode();
        uint32_t aligned_code_begin = AlignCodeOffset(code_offset);
        uint64_t aligned_code_end = aligned_code_begin + code_size;
        stats_.AddBitsIfUnique(Stats::kByteKindCode, code_size * kBitsPerByte, code);

        if (options_.absolute_addresses_) {
          vios->Stream() << StringPrintf("%p ", code);
        }
        vios->Stream() << StringPrintf("(code_offset=0x%08x size_offset=0x%08x size=%u)%s\n",
                                       code_offset,
                                       code_size_offset,
                                       code_size,
                                       code != nullptr ? "..." : "");

        ScopedIndentation indent2(vios);
        if (aligned_code_begin > oat_file_.Size()) {
          vios->Stream() << StringPrintf("WARNING: "
                                         "start of code at 0x%08x is past end of file 0x%08zx.",
                                         aligned_code_begin, oat_file_.Size());
          success = false;
        } else if (aligned_code_end > oat_file_.Size()) {
          vios->Stream() << StringPrintf(
              "WARNING: "
              "end of code at 0x%08" PRIx64 " is past end of file 0x%08zx. "
              "code size is 0x%08x loaded from offset 0x%08x.\n",
              aligned_code_end, oat_file_.Size(),
              code_size, code_size_offset);
          success = false;
          if (options_.disassemble_code_) {
            if (code_size_offset + kPrologueBytes <= oat_file_.Size()) {
              DumpCode(vios, oat_method, code_item_accessor, true, kPrologueBytes);
            }
          }
        } else if (code_size > kMaxCodeSize) {
          vios->Stream() << StringPrintf(
              "WARNING: "
              "code size %d is bigger than max expected threshold of %d. "
              "code size is 0x%08x loaded from offset 0x%08x.\n",
              code_size, kMaxCodeSize,
              code_size, code_size_offset);
          success = false;
          if (options_.disassemble_code_) {
            if (code_size_offset + kPrologueBytes <= oat_file_.Size()) {
              DumpCode(vios, oat_method, code_item_accessor, true, kPrologueBytes);
            }
          }
        } else if (options_.disassemble_code_) {
          DumpCode(vios, oat_method, code_item_accessor, !success, 0);
        }
      }
    }
    vios->Stream() << std::flush;
    return success;
  }

  void DumpSpillMask(std::ostream& os, uint32_t spill_mask, bool is_float) {
    if (spill_mask == 0) {
      return;
    }
    os << "(";
    for (size_t i = 0; i < 32; i++) {
      if ((spill_mask & (1 << i)) != 0) {
        if (is_float) {
          os << "fr" << i;
        } else {
          os << "r" << i;
        }
        spill_mask ^= 1 << i;  // clear bit
        if (spill_mask != 0) {
          os << ", ";
        } else {
          break;
        }
      }
    }
    os << ")";
  }

  // Display data stored at the the vmap offset of an oat method.
  void DumpVmapData(VariableIndentationOutputStream* vios,
                    const OatFile::OatMethod& oat_method,
                    const CodeItemDataAccessor& code_item_accessor) {
    if (IsMethodGeneratedByOptimizingCompiler(oat_method, code_item_accessor)) {
      // The optimizing compiler outputs its CodeInfo data in the vmap table.
      const void* raw_code_info = oat_method.GetVmapTable();
      if (raw_code_info != nullptr) {
        CodeInfo code_info(raw_code_info);
        DCHECK(code_item_accessor.HasCodeItem());
        ScopedIndentation indent1(vios);
        MethodInfo method_info = oat_method.GetOatQuickMethodHeader()->GetOptimizedMethodInfo();
        DumpCodeInfo(vios, code_info, oat_method, code_item_accessor, method_info);
      }
    } else if (IsMethodGeneratedByDexToDexCompiler(oat_method, code_item_accessor)) {
      // We don't encode the size in the table, so just emit that we have quickened
      // information.
      ScopedIndentation indent(vios);
      vios->Stream() << "quickened data\n";
    } else {
      // Otherwise, there is nothing to display.
    }
  }

  // Display a CodeInfo object emitted by the optimizing compiler.
  void DumpCodeInfo(VariableIndentationOutputStream* vios,
                    const CodeInfo& code_info,
                    const OatFile::OatMethod& oat_method,
                    const CodeItemDataAccessor& code_item_accessor,
                    const MethodInfo& method_info) {
    code_info.Dump(vios,
                   oat_method.GetCodeOffset(),
                   code_item_accessor.RegistersSize(),
                   options_.dump_code_info_stack_maps_,
                   instruction_set_,
                   method_info);
  }

  static int GetOutVROffset(uint16_t out_num, InstructionSet isa) {
    // According to stack model, the first out is above the Method referernce.
    return static_cast<size_t>(InstructionSetPointerSize(isa)) + out_num * sizeof(uint32_t);
  }

  static uint32_t GetVRegOffsetFromQuickCode(const CodeItemDataAccessor& code_item_accessor,
                                             uint32_t core_spills,
                                             uint32_t fp_spills,
                                             size_t frame_size,
                                             int reg,
                                             InstructionSet isa) {
    PointerSize pointer_size = InstructionSetPointerSize(isa);
    if (kIsDebugBuild) {
      auto* runtime = Runtime::Current();
      if (runtime != nullptr) {
        CHECK_EQ(runtime->GetClassLinker()->GetImagePointerSize(), pointer_size);
      }
    }
    DCHECK_ALIGNED(frame_size, kStackAlignment);
    DCHECK_NE(reg, -1);
    int spill_size = POPCOUNT(core_spills) * GetBytesPerGprSpillLocation(isa)
        + POPCOUNT(fp_spills) * GetBytesPerFprSpillLocation(isa)
        + sizeof(uint32_t);  // Filler.
    int num_regs = code_item_accessor.RegistersSize() - code_item_accessor.InsSize();
    int temp_threshold = code_item_accessor.RegistersSize();
    const int max_num_special_temps = 1;
    if (reg == temp_threshold) {
      // The current method pointer corresponds to special location on stack.
      return 0;
    } else if (reg >= temp_threshold + max_num_special_temps) {
      /*
       * Special temporaries may have custom locations and the logic above deals with that.
       * However, non-special temporaries are placed relative to the outs.
       */
      int temps_start = code_item_accessor.OutsSize() * sizeof(uint32_t)
          + static_cast<size_t>(pointer_size) /* art method */;
      int relative_offset = (reg - (temp_threshold + max_num_special_temps)) * sizeof(uint32_t);
      return temps_start + relative_offset;
    } else if (reg < num_regs) {
      int locals_start = frame_size - spill_size - num_regs * sizeof(uint32_t);
      return locals_start + (reg * sizeof(uint32_t));
    } else {
      // Handle ins.
      return frame_size + ((reg - num_regs) * sizeof(uint32_t))
          + static_cast<size_t>(pointer_size) /* art method */;
    }
  }

  void DumpVregLocations(std::ostream& os, const OatFile::OatMethod& oat_method,
                         const CodeItemDataAccessor& code_item_accessor) {
    if (code_item_accessor.HasCodeItem()) {
      size_t num_locals_ins = code_item_accessor.RegistersSize();
      size_t num_ins = code_item_accessor.InsSize();
      size_t num_locals = num_locals_ins - num_ins;
      size_t num_outs = code_item_accessor.OutsSize();

      os << "vr_stack_locations:";
      for (size_t reg = 0; reg <= num_locals_ins; reg++) {
        // For readability, delimit the different kinds of VRs.
        if (reg == num_locals_ins) {
          os << "\n\tmethod*:";
        } else if (reg == num_locals && num_ins > 0) {
          os << "\n\tins:";
        } else if (reg == 0 && num_locals > 0) {
          os << "\n\tlocals:";
        }

        uint32_t offset = GetVRegOffsetFromQuickCode(code_item_accessor,
                                                     oat_method.GetCoreSpillMask(),
                                                     oat_method.GetFpSpillMask(),
                                                     oat_method.GetFrameSizeInBytes(),
                                                     reg,
                                                     GetInstructionSet());
        os << " v" << reg << "[sp + #" << offset << "]";
      }

      for (size_t out_reg = 0; out_reg < num_outs; out_reg++) {
        if (out_reg == 0) {
          os << "\n\touts:";
        }

        uint32_t offset = GetOutVROffset(out_reg, GetInstructionSet());
        os << " v" << out_reg << "[sp + #" << offset << "]";
      }

      os << "\n";
    }
  }

  // Has `oat_method` -- corresponding to the Dex `code_item` -- been compiled by
  // the optimizing compiler?
  static bool IsMethodGeneratedByOptimizingCompiler(
      const OatFile::OatMethod& oat_method,
      const CodeItemDataAccessor& code_item_accessor) {
    // If the native GC map is null and the Dex `code_item` is not
    // null, then this method has been compiled with the optimizing
    // compiler.
    return oat_method.GetQuickCode() != nullptr &&
           oat_method.GetVmapTable() != nullptr &&
           code_item_accessor.HasCodeItem();
  }

  // Has `oat_method` -- corresponding to the Dex `code_item` -- been compiled by
  // the dextodex compiler?
  static bool IsMethodGeneratedByDexToDexCompiler(
      const OatFile::OatMethod& oat_method,
      const CodeItemDataAccessor& code_item_accessor) {
    // If the quick code is null, the Dex `code_item` is not
    // null, and the vmap table is not null, then this method has been compiled
    // with the dextodex compiler.
    return oat_method.GetQuickCode() == nullptr &&
           oat_method.GetVmapTable() != nullptr &&
           code_item_accessor.HasCodeItem();
  }

  verifier::MethodVerifier* DumpVerifier(VariableIndentationOutputStream* vios,
                                         StackHandleScope<1>* hs,
                                         uint32_t dex_method_idx,
                                         const DexFile* dex_file,
                                         const DexFile::ClassDef& class_def,
                                         const DexFile::CodeItem* code_item,
                                         uint32_t method_access_flags) {
    if ((method_access_flags & kAccNative) == 0) {
      ScopedObjectAccess soa(Thread::Current());
      Runtime* const runtime = Runtime::Current();
      DCHECK(options_.class_loader_ != nullptr);
      Handle<mirror::DexCache> dex_cache = hs->NewHandle(
          runtime->GetClassLinker()->RegisterDexFile(*dex_file, options_.class_loader_->Get()));
      CHECK(dex_cache != nullptr);
      ArtMethod* method = runtime->GetClassLinker()->ResolveMethodWithoutInvokeType(
          dex_method_idx, dex_cache, *options_.class_loader_);
      CHECK(method != nullptr);
      return verifier::MethodVerifier::VerifyMethodAndDump(
          soa.Self(), vios, dex_method_idx, dex_file, dex_cache, *options_.class_loader_,
          class_def, code_item, method, method_access_flags);
    }

    return nullptr;
  }

  // The StackMapsHelper provides the stack maps in the native PC order.
  // For identical native PCs, the order from the CodeInfo is preserved.
  class StackMapsHelper {
   public:
    explicit StackMapsHelper(const uint8_t* raw_code_info, InstructionSet instruction_set)
        : code_info_(raw_code_info),
          encoding_(code_info_.ExtractEncoding()),
          number_of_stack_maps_(code_info_.GetNumberOfStackMaps(encoding_)),
          indexes_(),
          offset_(static_cast<uint32_t>(-1)),
          stack_map_index_(0u),
          instruction_set_(instruction_set) {
      if (number_of_stack_maps_ != 0u) {
        // Check if native PCs are ordered.
        bool ordered = true;
        StackMap last = code_info_.GetStackMapAt(0u, encoding_);
        for (size_t i = 1; i != number_of_stack_maps_; ++i) {
          StackMap current = code_info_.GetStackMapAt(i, encoding_);
          if (last.GetNativePcOffset(encoding_.stack_map.encoding, instruction_set) >
              current.GetNativePcOffset(encoding_.stack_map.encoding, instruction_set)) {
            ordered = false;
            break;
          }
          last = current;
        }
        if (!ordered) {
          // Create indirection indexes for access in native PC order. We do not optimize
          // for the fact that there can currently be only two separately ordered ranges,
          // namely normal stack maps and catch-point stack maps.
          indexes_.resize(number_of_stack_maps_);
          std::iota(indexes_.begin(), indexes_.end(), 0u);
          std::sort(indexes_.begin(),
                    indexes_.end(),
                    [this](size_t lhs, size_t rhs) {
                      StackMap left = code_info_.GetStackMapAt(lhs, encoding_);
                      uint32_t left_pc = left.GetNativePcOffset(encoding_.stack_map.encoding,
                                                                instruction_set_);
                      StackMap right = code_info_.GetStackMapAt(rhs, encoding_);
                      uint32_t right_pc = right.GetNativePcOffset(encoding_.stack_map.encoding,
                                                                  instruction_set_);
                      // If the PCs are the same, compare indexes to preserve the original order.
                      return (left_pc < right_pc) || (left_pc == right_pc && lhs < rhs);
                    });
        }
        offset_ = GetStackMapAt(0).GetNativePcOffset(encoding_.stack_map.encoding,
                                                     instruction_set_);
      }
    }

    const CodeInfo& GetCodeInfo() const {
      return code_info_;
    }

    const CodeInfoEncoding& GetEncoding() const {
      return encoding_;
    }

    uint32_t GetOffset() const {
      return offset_;
    }

    StackMap GetStackMap() const {
      return GetStackMapAt(stack_map_index_);
    }

    void Next() {
      ++stack_map_index_;
      offset_ = (stack_map_index_ == number_of_stack_maps_)
          ? static_cast<uint32_t>(-1)
          : GetStackMapAt(stack_map_index_).GetNativePcOffset(encoding_.stack_map.encoding,
                                                              instruction_set_);
    }

   private:
    StackMap GetStackMapAt(size_t i) const {
      if (!indexes_.empty()) {
        i = indexes_[i];
      }
      DCHECK_LT(i, number_of_stack_maps_);
      return code_info_.GetStackMapAt(i, encoding_);
    }

    const CodeInfo code_info_;
    const CodeInfoEncoding encoding_;
    const size_t number_of_stack_maps_;
    dchecked_vector<size_t> indexes_;  // Used if stack map native PCs are not ordered.
    uint32_t offset_;
    size_t stack_map_index_;
    const InstructionSet instruction_set_;
  };

  void DumpCode(VariableIndentationOutputStream* vios,
                const OatFile::OatMethod& oat_method,
                const CodeItemDataAccessor& code_item_accessor,
                bool bad_input, size_t code_size) {
    const void* quick_code = oat_method.GetQuickCode();

    if (code_size == 0) {
      code_size = oat_method.GetQuickCodeSize();
    }
    if (code_size == 0 || quick_code == nullptr) {
      vios->Stream() << "NO CODE!\n";
      return;
    } else if (!bad_input && IsMethodGeneratedByOptimizingCompiler(oat_method,
                                                                   code_item_accessor)) {
      // The optimizing compiler outputs its CodeInfo data in the vmap table.
      StackMapsHelper helper(oat_method.GetVmapTable(), instruction_set_);
      MethodInfo method_info(oat_method.GetOatQuickMethodHeader()->GetOptimizedMethodInfo());
      {
        CodeInfoEncoding encoding(helper.GetEncoding());
        StackMapEncoding stack_map_encoding(encoding.stack_map.encoding);
        const size_t num_stack_maps = encoding.stack_map.num_entries;
        if (stats_.AddBitsIfUnique(Stats::kByteKindCodeInfoEncoding,
                                   encoding.HeaderSize() * kBitsPerByte,
                                   oat_method.GetVmapTable())) {
          // Stack maps
          stats_.AddBits(
              Stats::kByteKindStackMapNativePc,
              stack_map_encoding.GetNativePcEncoding().BitSize() * num_stack_maps);
          stats_.AddBits(
              Stats::kByteKindStackMapDexPc,
              stack_map_encoding.GetDexPcEncoding().BitSize() * num_stack_maps);
          stats_.AddBits(
              Stats::kByteKindStackMapDexRegisterMap,
              stack_map_encoding.GetDexRegisterMapEncoding().BitSize() * num_stack_maps);
          stats_.AddBits(
              Stats::kByteKindStackMapInlineInfoIndex,
              stack_map_encoding.GetInlineInfoEncoding().BitSize() * num_stack_maps);
          stats_.AddBits(
              Stats::kByteKindStackMapRegisterMaskIndex,
              stack_map_encoding.GetRegisterMaskIndexEncoding().BitSize() * num_stack_maps);
          stats_.AddBits(
              Stats::kByteKindStackMapStackMaskIndex,
              stack_map_encoding.GetStackMaskIndexEncoding().BitSize() * num_stack_maps);

          // Stack masks
          stats_.AddBits(
              Stats::kByteKindCodeInfoStackMasks,
              encoding.stack_mask.encoding.BitSize() * encoding.stack_mask.num_entries);

          // Register masks
          stats_.AddBits(
              Stats::kByteKindCodeInfoRegisterMasks,
              encoding.register_mask.encoding.BitSize() * encoding.register_mask.num_entries);

          // Invoke infos
          if (encoding.invoke_info.num_entries > 0u) {
            stats_.AddBits(
                Stats::kByteKindCodeInfoInvokeInfo,
                encoding.invoke_info.encoding.BitSize() * encoding.invoke_info.num_entries);
          }

          // Location catalog
          const size_t location_catalog_bytes =
              helper.GetCodeInfo().GetDexRegisterLocationCatalogSize(encoding);
          stats_.AddBits(Stats::kByteKindCodeInfoLocationCatalog,
                         kBitsPerByte * location_catalog_bytes);
          // Dex register bytes.
          const size_t dex_register_bytes =
              helper.GetCodeInfo().GetDexRegisterMapsSize(encoding,
                                                          code_item_accessor.RegistersSize());
          stats_.AddBits(
              Stats::kByteKindCodeInfoDexRegisterMap,
              kBitsPerByte * dex_register_bytes);

          // Inline infos.
          const size_t num_inline_infos = encoding.inline_info.num_entries;
          if (num_inline_infos > 0u) {
            stats_.AddBits(
                Stats::kByteKindInlineInfoMethodIndexIdx,
                encoding.inline_info.encoding.GetMethodIndexIdxEncoding().BitSize() *
                    num_inline_infos);
            stats_.AddBits(
                Stats::kByteKindInlineInfoDexPc,
                encoding.inline_info.encoding.GetDexPcEncoding().BitSize() * num_inline_infos);
            stats_.AddBits(
                Stats::kByteKindInlineInfoExtraData,
                encoding.inline_info.encoding.GetExtraDataEncoding().BitSize() * num_inline_infos);
            stats_.AddBits(
                Stats::kByteKindInlineInfoDexRegisterMap,
                encoding.inline_info.encoding.GetDexRegisterMapEncoding().BitSize() *
                    num_inline_infos);
            stats_.AddBits(Stats::kByteKindInlineInfoIsLast, num_inline_infos);
          }
        }
      }
      const uint8_t* quick_native_pc = reinterpret_cast<const uint8_t*>(quick_code);
      size_t offset = 0;
      while (offset < code_size) {
        offset += disassembler_->Dump(vios->Stream(), quick_native_pc + offset);
        if (offset == helper.GetOffset()) {
          ScopedIndentation indent1(vios);
          StackMap stack_map = helper.GetStackMap();
          DCHECK(stack_map.IsValid());
          stack_map.Dump(vios,
                         helper.GetCodeInfo(),
                         helper.GetEncoding(),
                         method_info,
                         oat_method.GetCodeOffset(),
                         code_item_accessor.RegistersSize(),
                         instruction_set_);
          do {
            helper.Next();
            // There may be multiple stack maps at a given PC. We display only the first one.
          } while (offset == helper.GetOffset());
        }
        DCHECK_LT(offset, helper.GetOffset());
      }
    } else {
      const uint8_t* quick_native_pc = reinterpret_cast<const uint8_t*>(quick_code);
      size_t offset = 0;
      while (offset < code_size) {
        offset += disassembler_->Dump(vios->Stream(), quick_native_pc + offset);
      }
    }
  }

  template <typename NameGetter>
  void DumpBssEntries(std::ostream& os,
                      const char* slot_type,
                      const IndexBssMapping* mapping,
                      uint32_t number_of_indexes,
                      size_t slot_size,
                      NameGetter name) {
    os << ".bss mapping for " << slot_type << ": ";
    if (mapping == nullptr) {
      os << "empty.\n";
      return;
    }
    size_t index_bits = IndexBssMappingEntry::IndexBits(number_of_indexes);
    size_t num_valid_indexes = 0u;
    for (const IndexBssMappingEntry& entry : *mapping) {
      num_valid_indexes += 1u + POPCOUNT(entry.GetMask(index_bits));
    }
    os << mapping->size() << " entries for " << num_valid_indexes << " valid indexes.\n";
    os << std::hex;
    for (const IndexBssMappingEntry& entry : *mapping) {
      uint32_t index = entry.GetIndex(index_bits);
      uint32_t mask = entry.GetMask(index_bits);
      size_t bss_offset = entry.bss_offset - POPCOUNT(mask) * slot_size;
      for (uint32_t n : LowToHighBits(mask)) {
        size_t current_index = index - (32u - index_bits) + n;
        os << "  0x" << bss_offset << ": " << slot_type << ": " << name(current_index) << "\n";
        bss_offset += slot_size;
      }
      DCHECK_EQ(bss_offset, entry.bss_offset);
      os << "  0x" << bss_offset << ": " << slot_type << ": " << name(index) << "\n";
    }
    os << std::dec;
  }

  const OatFile& oat_file_;
  const std::vector<const OatFile::OatDexFile*> oat_dex_files_;
  const OatDumperOptions& options_;
  uint32_t resolved_addr2instr_;
  const InstructionSet instruction_set_;
  std::set<uintptr_t> offsets_;
  Disassembler* disassembler_;
  Stats stats_;
};

class ImageDumper {
 public:
  ImageDumper(std::ostream* os,
              gc::space::ImageSpace& image_space,
              const ImageHeader& image_header,
              OatDumperOptions* oat_dumper_options)
      : os_(os),
        vios_(os),
        indent1_(&vios_),
        image_space_(image_space),
        image_header_(image_header),
        oat_dumper_options_(oat_dumper_options) {}

  bool Dump() REQUIRES_SHARED(Locks::mutator_lock_) {
    std::ostream& os = *os_;
    std::ostream& indent_os = vios_.Stream();

    os << "MAGIC: " << image_header_.GetMagic() << "\n\n";

    os << "IMAGE LOCATION: " << image_space_.GetImageLocation() << "\n\n";

    os << "IMAGE BEGIN: " << reinterpret_cast<void*>(image_header_.GetImageBegin()) << "\n\n";

    os << "IMAGE SIZE: " << image_header_.GetImageSize() << "\n\n";

    for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
      auto section = static_cast<ImageHeader::ImageSections>(i);
      os << "IMAGE SECTION " << section << ": " << image_header_.GetImageSection(section) << "\n\n";
    }

    os << "OAT CHECKSUM: " << StringPrintf("0x%08x\n\n", image_header_.GetOatChecksum());

    os << "OAT FILE BEGIN:" << reinterpret_cast<void*>(image_header_.GetOatFileBegin()) << "\n\n";

    os << "OAT DATA BEGIN:" << reinterpret_cast<void*>(image_header_.GetOatDataBegin()) << "\n\n";

    os << "OAT DATA END:" << reinterpret_cast<void*>(image_header_.GetOatDataEnd()) << "\n\n";

    os << "OAT FILE END:" << reinterpret_cast<void*>(image_header_.GetOatFileEnd()) << "\n\n";

    os << "PATCH DELTA:" << image_header_.GetPatchDelta() << "\n\n";

    os << "COMPILE PIC: " << (image_header_.CompilePic() ? "yes" : "no") << "\n\n";

    {
      os << "ROOTS: " << reinterpret_cast<void*>(image_header_.GetImageRoots()) << "\n";
      static_assert(arraysize(image_roots_descriptions_) ==
          static_cast<size_t>(ImageHeader::kImageRootsMax), "sizes must match");
      DCHECK_LE(image_header_.GetImageRoots()->GetLength(), ImageHeader::kImageRootsMax);
      for (int32_t i = 0, size = image_header_.GetImageRoots()->GetLength(); i != size; ++i) {
        ImageHeader::ImageRoot image_root = static_cast<ImageHeader::ImageRoot>(i);
        const char* image_root_description = image_roots_descriptions_[i];
        mirror::Object* image_root_object = image_header_.GetImageRoot(image_root);
        indent_os << StringPrintf("%s: %p\n", image_root_description, image_root_object);
        if (image_root_object != nullptr && image_root_object->IsObjectArray()) {
          mirror::ObjectArray<mirror::Object>* image_root_object_array
              = image_root_object->AsObjectArray<mirror::Object>();
          ScopedIndentation indent2(&vios_);
          for (int j = 0; j < image_root_object_array->GetLength(); j++) {
            mirror::Object* value = image_root_object_array->Get(j);
            size_t run = 0;
            for (int32_t k = j + 1; k < image_root_object_array->GetLength(); k++) {
              if (value == image_root_object_array->Get(k)) {
                run++;
              } else {
                break;
              }
            }
            if (run == 0) {
              indent_os << StringPrintf("%d: ", j);
            } else {
              indent_os << StringPrintf("%d to %zd: ", j, j + run);
              j = j + run;
            }
            if (value != nullptr) {
              PrettyObjectValue(indent_os, value->GetClass(), value);
            } else {
              indent_os << j << ": null\n";
            }
          }
        }
      }
    }

    {
      os << "METHOD ROOTS\n";
      static_assert(arraysize(image_methods_descriptions_) ==
          static_cast<size_t>(ImageHeader::kImageMethodsCount), "sizes must match");
      for (int i = 0; i < ImageHeader::kImageMethodsCount; i++) {
        auto image_root = static_cast<ImageHeader::ImageMethod>(i);
        const char* description = image_methods_descriptions_[i];
        auto* image_method = image_header_.GetImageMethod(image_root);
        indent_os << StringPrintf("%s: %p\n", description, image_method);
      }
    }
    os << "\n";

    Runtime* const runtime = Runtime::Current();
    ClassLinker* class_linker = runtime->GetClassLinker();
    std::string image_filename = image_space_.GetImageFilename();
    std::string oat_location = ImageHeader::GetOatLocationFromImageLocation(image_filename);
    os << "OAT LOCATION: " << oat_location;
    os << "\n";
    std::string error_msg;
    const OatFile* oat_file = image_space_.GetOatFile();
    if (oat_file == nullptr) {
      oat_file = runtime->GetOatFileManager().FindOpenedOatFileFromOatLocation(oat_location);
    }
    if (oat_file == nullptr) {
      oat_file = OatFile::Open(/* zip_fd */ -1,
                               oat_location,
                               oat_location,
                               nullptr,
                               nullptr,
                               false,
                               /*low_4gb*/false,
                               nullptr,
                               &error_msg);
    }
    if (oat_file == nullptr) {
      os << "OAT FILE NOT FOUND: " << error_msg << "\n";
      return EXIT_FAILURE;
    }
    os << "\n";

    stats_.oat_file_bytes = oat_file->Size();

    oat_dumper_.reset(new OatDumper(*oat_file, *oat_dumper_options_));

    for (const OatFile::OatDexFile* oat_dex_file : oat_file->GetOatDexFiles()) {
      CHECK(oat_dex_file != nullptr);
      stats_.oat_dex_file_sizes.push_back(std::make_pair(oat_dex_file->GetDexFileLocation(),
                                                         oat_dex_file->FileSize()));
    }

    os << "OBJECTS:\n" << std::flush;

    // Loop through the image space and dump its objects.
    gc::Heap* heap = runtime->GetHeap();
    Thread* self = Thread::Current();
    {
      {
        WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
        heap->FlushAllocStack();
      }
      // Since FlushAllocStack() above resets the (active) allocation
      // stack. Need to revoke the thread-local allocation stacks that
      // point into it.
      ScopedThreadSuspension sts(self, kNative);
      ScopedSuspendAll ssa(__FUNCTION__);
      heap->RevokeAllThreadLocalAllocationStacks(self);
    }
    {
      // Mark dex caches.
      dex_caches_.clear();
      {
        ReaderMutexLock mu(self, *Locks::dex_lock_);
        for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
          ObjPtr<mirror::DexCache> dex_cache =
              ObjPtr<mirror::DexCache>::DownCast(self->DecodeJObject(data.weak_root));
          if (dex_cache != nullptr) {
            dex_caches_.insert(dex_cache.Ptr());
          }
        }
      }
      auto dump_visitor = [&](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
        DumpObject(obj);
      };
      ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
      // Dump the normal objects before ArtMethods.
      image_space_.GetLiveBitmap()->Walk(dump_visitor);
      indent_os << "\n";
      // TODO: Dump fields.
      // Dump methods after.
      DumpArtMethodVisitor visitor(this);
      image_header_.VisitPackedArtMethods(&visitor,
                                          image_space_.Begin(),
                                          image_header_.GetPointerSize());
      // Dump the large objects separately.
      heap->GetLargeObjectsSpace()->GetLiveBitmap()->Walk(dump_visitor);
      indent_os << "\n";
    }
    os << "STATS:\n" << std::flush;
    std::unique_ptr<File> file(OS::OpenFileForReading(image_filename.c_str()));
    size_t data_size = image_header_.GetDataSize();  // stored size in file.
    if (file == nullptr) {
      LOG(WARNING) << "Failed to find image in " << image_filename;
    } else {
      stats_.file_bytes = file->GetLength();
      // If the image is compressed, adjust to decompressed size.
      size_t uncompressed_size = image_header_.GetImageSize() - sizeof(ImageHeader);
      if (image_header_.GetStorageMode() == ImageHeader::kStorageModeUncompressed) {
        DCHECK_EQ(uncompressed_size, data_size) << "Sizes should match for uncompressed image";
      }
      stats_.file_bytes += uncompressed_size - data_size;
    }
    size_t header_bytes = sizeof(ImageHeader);
    const auto& object_section = image_header_.GetObjectsSection();
    const auto& field_section = image_header_.GetFieldsSection();
    const auto& method_section = image_header_.GetMethodsSection();
    const auto& dex_cache_arrays_section = image_header_.GetDexCacheArraysSection();
    const auto& intern_section = image_header_.GetInternedStringsSection();
    const auto& class_table_section = image_header_.GetClassTableSection();
    const auto& bitmap_section = image_header_.GetImageBitmapSection();

    stats_.header_bytes = header_bytes;

    // Objects are kObjectAlignment-aligned.
    // CHECK_EQ(RoundUp(header_bytes, kObjectAlignment), object_section.Offset());
    if (object_section.Offset() > header_bytes) {
      stats_.alignment_bytes += object_section.Offset() - header_bytes;
    }

    // Field section is 4-byte aligned.
    constexpr size_t kFieldSectionAlignment = 4U;
    uint32_t end_objects = object_section.Offset() + object_section.Size();
    CHECK_EQ(RoundUp(end_objects, kFieldSectionAlignment), field_section.Offset());
    stats_.alignment_bytes += field_section.Offset() - end_objects;

    // Method section is 4/8 byte aligned depending on target. Just check for 4-byte alignment.
    uint32_t end_fields = field_section.Offset() + field_section.Size();
    CHECK_ALIGNED(method_section.Offset(), 4);
    stats_.alignment_bytes += method_section.Offset() - end_fields;

    // Dex cache arrays section is aligned depending on the target. Just check for 4-byte alignment.
    uint32_t end_methods = method_section.Offset() + method_section.Size();
    CHECK_ALIGNED(dex_cache_arrays_section.Offset(), 4);
    stats_.alignment_bytes += dex_cache_arrays_section.Offset() - end_methods;

    // Intern table is 8-byte aligned.
    uint32_t end_caches = dex_cache_arrays_section.Offset() + dex_cache_arrays_section.Size();
    CHECK_ALIGNED(intern_section.Offset(), sizeof(uint64_t));
    stats_.alignment_bytes += intern_section.Offset() - end_caches;

    // Add space between intern table and class table.
    uint32_t end_intern = intern_section.Offset() + intern_section.Size();
    stats_.alignment_bytes += class_table_section.Offset() - end_intern;

    // Add space between end of image data and bitmap. Expect the bitmap to be page-aligned.
    const size_t bitmap_offset = sizeof(ImageHeader) + data_size;
    CHECK_ALIGNED(bitmap_section.Offset(), kPageSize);
    stats_.alignment_bytes += RoundUp(bitmap_offset, kPageSize) - bitmap_offset;

    stats_.bitmap_bytes += bitmap_section.Size();
    stats_.art_field_bytes += field_section.Size();
    stats_.art_method_bytes += method_section.Size();
    stats_.dex_cache_arrays_bytes += dex_cache_arrays_section.Size();
    stats_.interned_strings_bytes += intern_section.Size();
    stats_.class_table_bytes += class_table_section.Size();
    stats_.Dump(os, indent_os);
    os << "\n";

    os << std::flush;

    return oat_dumper_->Dump(os);
  }

 private:
  class DumpArtMethodVisitor : public ArtMethodVisitor {
   public:
    explicit DumpArtMethodVisitor(ImageDumper* image_dumper) : image_dumper_(image_dumper) {}

    virtual void Visit(ArtMethod* method) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
      std::ostream& indent_os = image_dumper_->vios_.Stream();
      indent_os << method << " " << " ArtMethod: " << ArtMethod::PrettyMethod(method) << "\n";
      image_dumper_->DumpMethod(method, indent_os);
      indent_os << "\n";
    }

   private:
    ImageDumper* const image_dumper_;
  };

  static void PrettyObjectValue(std::ostream& os,
                                ObjPtr<mirror::Class> type,
                                ObjPtr<mirror::Object> value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(type != nullptr);
    if (value == nullptr) {
      os << StringPrintf("null   %s\n", type->PrettyDescriptor().c_str());
    } else if (type->IsStringClass()) {
      mirror::String* string = value->AsString();
      os << StringPrintf("%p   String: %s\n", string,
                         PrintableString(string->ToModifiedUtf8().c_str()).c_str());
    } else if (type->IsClassClass()) {
      mirror::Class* klass = value->AsClass();
      os << StringPrintf("%p   Class: %s\n", klass, mirror::Class::PrettyDescriptor(klass).c_str());
    } else {
      os << StringPrintf("%p   %s\n", value.Ptr(), type->PrettyDescriptor().c_str());
    }
  }

  static void PrintField(std::ostream& os, ArtField* field, ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    os << StringPrintf("%s: ", field->GetName());
    switch (field->GetTypeAsPrimitiveType()) {
      case Primitive::kPrimLong:
        os << StringPrintf("%" PRId64 " (0x%" PRIx64 ")\n", field->Get64(obj), field->Get64(obj));
        break;
      case Primitive::kPrimDouble:
        os << StringPrintf("%f (%a)\n", field->GetDouble(obj), field->GetDouble(obj));
        break;
      case Primitive::kPrimFloat:
        os << StringPrintf("%f (%a)\n", field->GetFloat(obj), field->GetFloat(obj));
        break;
      case Primitive::kPrimInt:
        os << StringPrintf("%d (0x%x)\n", field->Get32(obj), field->Get32(obj));
        break;
      case Primitive::kPrimChar:
        os << StringPrintf("%u (0x%x)\n", field->GetChar(obj), field->GetChar(obj));
        break;
      case Primitive::kPrimShort:
        os << StringPrintf("%d (0x%x)\n", field->GetShort(obj), field->GetShort(obj));
        break;
      case Primitive::kPrimBoolean:
        os << StringPrintf("%s (0x%x)\n", field->GetBoolean(obj) ? "true" : "false",
            field->GetBoolean(obj));
        break;
      case Primitive::kPrimByte:
        os << StringPrintf("%d (0x%x)\n", field->GetByte(obj), field->GetByte(obj));
        break;
      case Primitive::kPrimNot: {
        // Get the value, don't compute the type unless it is non-null as we don't want
        // to cause class loading.
        ObjPtr<mirror::Object> value = field->GetObj(obj);
        if (value == nullptr) {
          os << StringPrintf("null   %s\n", PrettyDescriptor(field->GetTypeDescriptor()).c_str());
        } else {
          // Grab the field type without causing resolution.
          ObjPtr<mirror::Class> field_type = field->LookupResolvedType();
          if (field_type != nullptr) {
            PrettyObjectValue(os, field_type, value);
          } else {
            os << StringPrintf("%p   %s\n",
                               value.Ptr(),
                               PrettyDescriptor(field->GetTypeDescriptor()).c_str());
          }
        }
        break;
      }
      default:
        os << "unexpected field type: " << field->GetTypeDescriptor() << "\n";
        break;
    }
  }

  static void DumpFields(std::ostream& os, mirror::Object* obj, mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Class* super = klass->GetSuperClass();
    if (super != nullptr) {
      DumpFields(os, obj, super);
    }
    for (ArtField& field : klass->GetIFields()) {
      PrintField(os, &field, obj);
    }
  }

  bool InDumpSpace(const mirror::Object* object) {
    return image_space_.Contains(object);
  }

  const void* GetQuickOatCodeBegin(ArtMethod* m) REQUIRES_SHARED(Locks::mutator_lock_) {
    const void* quick_code = m->GetEntryPointFromQuickCompiledCodePtrSize(
        image_header_.GetPointerSize());
    if (Runtime::Current()->GetClassLinker()->IsQuickResolutionStub(quick_code)) {
      quick_code = oat_dumper_->GetQuickOatCode(m);
    }
    if (oat_dumper_->GetInstructionSet() == InstructionSet::kThumb2) {
      quick_code = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(quick_code) & ~0x1);
    }
    return quick_code;
  }

  uint32_t GetQuickOatCodeSize(ArtMethod* m)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const uint32_t* oat_code_begin = reinterpret_cast<const uint32_t*>(GetQuickOatCodeBegin(m));
    if (oat_code_begin == nullptr) {
      return 0;
    }
    return oat_code_begin[-1];
  }

  const void* GetQuickOatCodeEnd(ArtMethod* m)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const uint8_t* oat_code_begin = reinterpret_cast<const uint8_t*>(GetQuickOatCodeBegin(m));
    if (oat_code_begin == nullptr) {
      return nullptr;
    }
    return oat_code_begin + GetQuickOatCodeSize(m);
  }

  void DumpObject(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(obj != nullptr);
    if (!InDumpSpace(obj)) {
      return;
    }

    size_t object_bytes = obj->SizeOf();
    size_t alignment_bytes = RoundUp(object_bytes, kObjectAlignment) - object_bytes;
    stats_.object_bytes += object_bytes;
    stats_.alignment_bytes += alignment_bytes;

    std::ostream& os = vios_.Stream();

    mirror::Class* obj_class = obj->GetClass();
    if (obj_class->IsArrayClass()) {
      os << StringPrintf("%p: %s length:%d\n", obj, obj_class->PrettyDescriptor().c_str(),
                         obj->AsArray()->GetLength());
    } else if (obj->IsClass()) {
      mirror::Class* klass = obj->AsClass();
      os << StringPrintf("%p: java.lang.Class \"%s\" (", obj,
                         mirror::Class::PrettyDescriptor(klass).c_str())
         << klass->GetStatus() << ")\n";
    } else if (obj_class->IsStringClass()) {
      os << StringPrintf("%p: java.lang.String %s\n", obj,
                         PrintableString(obj->AsString()->ToModifiedUtf8().c_str()).c_str());
    } else {
      os << StringPrintf("%p: %s\n", obj, obj_class->PrettyDescriptor().c_str());
    }
    ScopedIndentation indent1(&vios_);
    DumpFields(os, obj, obj_class);
    const PointerSize image_pointer_size = image_header_.GetPointerSize();
    if (obj->IsObjectArray()) {
      auto* obj_array = obj->AsObjectArray<mirror::Object>();
      for (int32_t i = 0, length = obj_array->GetLength(); i < length; i++) {
        mirror::Object* value = obj_array->Get(i);
        size_t run = 0;
        for (int32_t j = i + 1; j < length; j++) {
          if (value == obj_array->Get(j)) {
            run++;
          } else {
            break;
          }
        }
        if (run == 0) {
          os << StringPrintf("%d: ", i);
        } else {
          os << StringPrintf("%d to %zd: ", i, i + run);
          i = i + run;
        }
        mirror::Class* value_class =
            (value == nullptr) ? obj_class->GetComponentType() : value->GetClass();
        PrettyObjectValue(os, value_class, value);
      }
    } else if (obj->IsClass()) {
      ObjPtr<mirror::Class> klass = obj->AsClass();

      if (kBitstringSubtypeCheckEnabled) {
        os << "SUBTYPE_CHECK_BITS: ";
        SubtypeCheck<ObjPtr<mirror::Class>>::Dump(klass, os);
        os << "\n";
      }

      if (klass->NumStaticFields() != 0) {
        os << "STATICS:\n";
        ScopedIndentation indent2(&vios_);
        for (ArtField& field : klass->GetSFields()) {
          PrintField(os, &field, field.GetDeclaringClass());
        }
      }
    } else {
      auto it = dex_caches_.find(obj);
      if (it != dex_caches_.end()) {
        auto* dex_cache = down_cast<mirror::DexCache*>(obj);
        const auto& field_section = image_header_.GetFieldsSection();
        const auto& method_section = image_header_.GetMethodsSection();
        size_t num_methods = dex_cache->NumResolvedMethods();
        if (num_methods != 0u) {
          os << "Methods (size=" << num_methods << "):\n";
          ScopedIndentation indent2(&vios_);
          mirror::MethodDexCacheType* resolved_methods = dex_cache->GetResolvedMethods();
          for (size_t i = 0, length = dex_cache->NumResolvedMethods(); i < length; ++i) {
            ArtMethod* elem = mirror::DexCache::GetNativePairPtrSize(
                resolved_methods, i, image_pointer_size).object;
            size_t run = 0;
            for (size_t j = i + 1;
                 j != length &&
                 elem == mirror::DexCache::GetNativePairPtrSize(
                     resolved_methods, j, image_pointer_size).object;
                 ++j) {
              ++run;
            }
            if (run == 0) {
              os << StringPrintf("%zd: ", i);
            } else {
              os << StringPrintf("%zd to %zd: ", i, i + run);
              i = i + run;
            }
            std::string msg;
            if (elem == nullptr) {
              msg = "null";
            } else if (method_section.Contains(
                reinterpret_cast<uint8_t*>(elem) - image_space_.Begin())) {
              msg = reinterpret_cast<ArtMethod*>(elem)->PrettyMethod();
            } else {
              msg = "<not in method section>";
            }
            os << StringPrintf("%p   %s\n", elem, msg.c_str());
          }
        }
        size_t num_fields = dex_cache->NumResolvedFields();
        if (num_fields != 0u) {
          os << "Fields (size=" << num_fields << "):\n";
          ScopedIndentation indent2(&vios_);
          auto* resolved_fields = dex_cache->GetResolvedFields();
          for (size_t i = 0, length = dex_cache->NumResolvedFields(); i < length; ++i) {
            ArtField* elem = mirror::DexCache::GetNativePairPtrSize(
                resolved_fields, i, image_pointer_size).object;
            size_t run = 0;
            for (size_t j = i + 1;
                 j != length &&
                 elem == mirror::DexCache::GetNativePairPtrSize(
                     resolved_fields, j, image_pointer_size).object;
                 ++j) {
              ++run;
            }
            if (run == 0) {
              os << StringPrintf("%zd: ", i);
            } else {
              os << StringPrintf("%zd to %zd: ", i, i + run);
              i = i + run;
            }
            std::string msg;
            if (elem == nullptr) {
              msg = "null";
            } else if (field_section.Contains(
                reinterpret_cast<uint8_t*>(elem) - image_space_.Begin())) {
              msg = reinterpret_cast<ArtField*>(elem)->PrettyField();
            } else {
              msg = "<not in field section>";
            }
            os << StringPrintf("%p   %s\n", elem, msg.c_str());
          }
        }
        size_t num_types = dex_cache->NumResolvedTypes();
        if (num_types != 0u) {
          os << "Types (size=" << num_types << "):\n";
          ScopedIndentation indent2(&vios_);
          auto* resolved_types = dex_cache->GetResolvedTypes();
          for (size_t i = 0; i < num_types; ++i) {
            auto pair = resolved_types[i].load(std::memory_order_relaxed);
            size_t run = 0;
            for (size_t j = i + 1; j != num_types; ++j) {
              auto other_pair = resolved_types[j].load(std::memory_order_relaxed);
              if (pair.index != other_pair.index ||
                  pair.object.Read() != other_pair.object.Read()) {
                break;
              }
              ++run;
            }
            if (run == 0) {
              os << StringPrintf("%zd: ", i);
            } else {
              os << StringPrintf("%zd to %zd: ", i, i + run);
              i = i + run;
            }
            std::string msg;
            auto* elem = pair.object.Read();
            if (elem == nullptr) {
              msg = "null";
            } else {
              msg = elem->PrettyClass();
            }
            os << StringPrintf("%p   %u %s\n", elem, pair.index, msg.c_str());
          }
        }
      }
    }
    std::string temp;
    stats_.Update(obj_class->GetDescriptor(&temp), object_bytes);
  }

  void DumpMethod(ArtMethod* method, std::ostream& indent_os)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(method != nullptr);
    const void* quick_oat_code_begin = GetQuickOatCodeBegin(method);
    const void* quick_oat_code_end = GetQuickOatCodeEnd(method);
    const PointerSize pointer_size = image_header_.GetPointerSize();
    OatQuickMethodHeader* method_header = reinterpret_cast<OatQuickMethodHeader*>(
        reinterpret_cast<uintptr_t>(quick_oat_code_begin) - sizeof(OatQuickMethodHeader));
    if (method->IsNative()) {
      bool first_occurrence;
      uint32_t quick_oat_code_size = GetQuickOatCodeSize(method);
      ComputeOatSize(quick_oat_code_begin, &first_occurrence);
      if (first_occurrence) {
        stats_.native_to_managed_code_bytes += quick_oat_code_size;
      }
      if (quick_oat_code_begin != method->GetEntryPointFromQuickCompiledCodePtrSize(
          image_header_.GetPointerSize())) {
        indent_os << StringPrintf("OAT CODE: %p\n", quick_oat_code_begin);
      }
    } else if (method->IsAbstract() || method->IsClassInitializer()) {
      // Don't print information for these.
    } else if (method->IsRuntimeMethod()) {
      ImtConflictTable* table = method->GetImtConflictTable(image_header_.GetPointerSize());
      if (table != nullptr) {
        indent_os << "IMT conflict table " << table << " method: ";
        for (size_t i = 0, count = table->NumEntries(pointer_size); i < count; ++i) {
          indent_os << ArtMethod::PrettyMethod(table->GetImplementationMethod(i, pointer_size))
                    << " ";
        }
      }
    } else {
      CodeItemDataAccessor code_item_accessor(method->DexInstructionData());
      size_t dex_instruction_bytes = code_item_accessor.InsnsSizeInCodeUnits() * 2;
      stats_.dex_instruction_bytes += dex_instruction_bytes;

      bool first_occurrence;
      size_t vmap_table_bytes = 0u;
      if (!method_header->IsOptimized()) {
        // Method compiled with the optimizing compiler have no vmap table.
        vmap_table_bytes = ComputeOatSize(method_header->GetVmapTable(), &first_occurrence);
        if (first_occurrence) {
          stats_.vmap_table_bytes += vmap_table_bytes;
        }
      }

      uint32_t quick_oat_code_size = GetQuickOatCodeSize(method);
      ComputeOatSize(quick_oat_code_begin, &first_occurrence);
      if (first_occurrence) {
        stats_.managed_code_bytes += quick_oat_code_size;
        if (method->IsConstructor()) {
          if (method->IsStatic()) {
            stats_.class_initializer_code_bytes += quick_oat_code_size;
          } else if (dex_instruction_bytes > kLargeConstructorDexBytes) {
            stats_.large_initializer_code_bytes += quick_oat_code_size;
          }
        } else if (dex_instruction_bytes > kLargeMethodDexBytes) {
          stats_.large_method_code_bytes += quick_oat_code_size;
        }
      }
      stats_.managed_code_bytes_ignoring_deduplication += quick_oat_code_size;

      uint32_t method_access_flags = method->GetAccessFlags();

      indent_os << StringPrintf("OAT CODE: %p-%p\n", quick_oat_code_begin, quick_oat_code_end);
      indent_os << StringPrintf("SIZE: Dex Instructions=%zd StackMaps=%zd AccessFlags=0x%x\n",
                                dex_instruction_bytes,
                                vmap_table_bytes,
                                method_access_flags);

      size_t total_size = dex_instruction_bytes +
          vmap_table_bytes + quick_oat_code_size + ArtMethod::Size(image_header_.GetPointerSize());

      double expansion =
      static_cast<double>(quick_oat_code_size) / static_cast<double>(dex_instruction_bytes);
      stats_.ComputeOutliers(total_size, expansion, method);
    }
  }

  std::set<const void*> already_seen_;
  // Compute the size of the given data within the oat file and whether this is the first time
  // this data has been requested
  size_t ComputeOatSize(const void* oat_data, bool* first_occurrence) {
    if (already_seen_.count(oat_data) == 0) {
      *first_occurrence = true;
      already_seen_.insert(oat_data);
    } else {
      *first_occurrence = false;
    }
    return oat_dumper_->ComputeSize(oat_data);
  }

 public:
  struct Stats {
    size_t oat_file_bytes;
    size_t file_bytes;

    size_t header_bytes;
    size_t object_bytes;
    size_t art_field_bytes;
    size_t art_method_bytes;
    size_t dex_cache_arrays_bytes;
    size_t interned_strings_bytes;
    size_t class_table_bytes;
    size_t bitmap_bytes;
    size_t alignment_bytes;

    size_t managed_code_bytes;
    size_t managed_code_bytes_ignoring_deduplication;
    size_t native_to_managed_code_bytes;
    size_t class_initializer_code_bytes;
    size_t large_initializer_code_bytes;
    size_t large_method_code_bytes;

    size_t vmap_table_bytes;

    size_t dex_instruction_bytes;

    std::vector<ArtMethod*> method_outlier;
    std::vector<size_t> method_outlier_size;
    std::vector<double> method_outlier_expansion;
    std::vector<std::pair<std::string, size_t>> oat_dex_file_sizes;

    Stats()
        : oat_file_bytes(0),
          file_bytes(0),
          header_bytes(0),
          object_bytes(0),
          art_field_bytes(0),
          art_method_bytes(0),
          dex_cache_arrays_bytes(0),
          interned_strings_bytes(0),
          class_table_bytes(0),
          bitmap_bytes(0),
          alignment_bytes(0),
          managed_code_bytes(0),
          managed_code_bytes_ignoring_deduplication(0),
          native_to_managed_code_bytes(0),
          class_initializer_code_bytes(0),
          large_initializer_code_bytes(0),
          large_method_code_bytes(0),
          vmap_table_bytes(0),
          dex_instruction_bytes(0) {}

    struct SizeAndCount {
      SizeAndCount(size_t bytes_in, size_t count_in) : bytes(bytes_in), count(count_in) {}
      size_t bytes;
      size_t count;
    };
    typedef SafeMap<std::string, SizeAndCount> SizeAndCountTable;
    SizeAndCountTable sizes_and_counts;

    void Update(const char* descriptor, size_t object_bytes_in) {
      SizeAndCountTable::iterator it = sizes_and_counts.find(descriptor);
      if (it != sizes_and_counts.end()) {
        it->second.bytes += object_bytes_in;
        it->second.count += 1;
      } else {
        sizes_and_counts.Put(descriptor, SizeAndCount(object_bytes_in, 1));
      }
    }

    double PercentOfOatBytes(size_t size) {
      return (static_cast<double>(size) / static_cast<double>(oat_file_bytes)) * 100;
    }

    double PercentOfFileBytes(size_t size) {
      return (static_cast<double>(size) / static_cast<double>(file_bytes)) * 100;
    }

    double PercentOfObjectBytes(size_t size) {
      return (static_cast<double>(size) / static_cast<double>(object_bytes)) * 100;
    }

    void ComputeOutliers(size_t total_size, double expansion, ArtMethod* method) {
      method_outlier_size.push_back(total_size);
      method_outlier_expansion.push_back(expansion);
      method_outlier.push_back(method);
    }

    void DumpOutliers(std::ostream& os)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      size_t sum_of_sizes = 0;
      size_t sum_of_sizes_squared = 0;
      size_t sum_of_expansion = 0;
      size_t sum_of_expansion_squared = 0;
      size_t n = method_outlier_size.size();
      if (n <= 1) {
        return;
      }
      for (size_t i = 0; i < n; i++) {
        size_t cur_size = method_outlier_size[i];
        sum_of_sizes += cur_size;
        sum_of_sizes_squared += cur_size * cur_size;
        double cur_expansion = method_outlier_expansion[i];
        sum_of_expansion += cur_expansion;
        sum_of_expansion_squared += cur_expansion * cur_expansion;
      }
      size_t size_mean = sum_of_sizes / n;
      size_t size_variance = (sum_of_sizes_squared - sum_of_sizes * size_mean) / (n - 1);
      double expansion_mean = sum_of_expansion / n;
      double expansion_variance =
          (sum_of_expansion_squared - sum_of_expansion * expansion_mean) / (n - 1);

      // Dump methods whose size is a certain number of standard deviations from the mean
      size_t dumped_values = 0;
      size_t skipped_values = 0;
      for (size_t i = 100; i > 0; i--) {  // i is the current number of standard deviations
        size_t cur_size_variance = i * i * size_variance;
        bool first = true;
        for (size_t j = 0; j < n; j++) {
          size_t cur_size = method_outlier_size[j];
          if (cur_size > size_mean) {
            size_t cur_var = cur_size - size_mean;
            cur_var = cur_var * cur_var;
            if (cur_var > cur_size_variance) {
              if (dumped_values > 20) {
                if (i == 1) {
                  skipped_values++;
                } else {
                  i = 2;  // jump to counting for 1 standard deviation
                  break;
                }
              } else {
                if (first) {
                  os << "\nBig methods (size > " << i << " standard deviations the norm):\n";
                  first = false;
                }
                os << ArtMethod::PrettyMethod(method_outlier[j]) << " requires storage of "
                    << PrettySize(cur_size) << "\n";
                method_outlier_size[j] = 0;  // don't consider this method again
                dumped_values++;
              }
            }
          }
        }
      }
      if (skipped_values > 0) {
        os << "... skipped " << skipped_values
           << " methods with size > 1 standard deviation from the norm\n";
      }
      os << std::flush;

      // Dump methods whose expansion is a certain number of standard deviations from the mean
      dumped_values = 0;
      skipped_values = 0;
      for (size_t i = 10; i > 0; i--) {  // i is the current number of standard deviations
        double cur_expansion_variance = i * i * expansion_variance;
        bool first = true;
        for (size_t j = 0; j < n; j++) {
          double cur_expansion = method_outlier_expansion[j];
          if (cur_expansion > expansion_mean) {
            size_t cur_var = cur_expansion - expansion_mean;
            cur_var = cur_var * cur_var;
            if (cur_var > cur_expansion_variance) {
              if (dumped_values > 20) {
                if (i == 1) {
                  skipped_values++;
                } else {
                  i = 2;  // jump to counting for 1 standard deviation
                  break;
                }
              } else {
                if (first) {
                  os << "\nLarge expansion methods (size > " << i
                      << " standard deviations the norm):\n";
                  first = false;
                }
                os << ArtMethod::PrettyMethod(method_outlier[j]) << " expanded code by "
                   << cur_expansion << "\n";
                method_outlier_expansion[j] = 0.0;  // don't consider this method again
                dumped_values++;
              }
            }
          }
        }
      }
      if (skipped_values > 0) {
        os << "... skipped " << skipped_values
           << " methods with expansion > 1 standard deviation from the norm\n";
      }
      os << "\n" << std::flush;
    }

    void Dump(std::ostream& os, std::ostream& indent_os)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      {
        os << "art_file_bytes = " << PrettySize(file_bytes) << "\n\n"
           << "art_file_bytes = header_bytes + object_bytes + alignment_bytes\n";
        indent_os << StringPrintf("header_bytes           =  %8zd (%2.0f%% of art file bytes)\n"
                                  "object_bytes           =  %8zd (%2.0f%% of art file bytes)\n"
                                  "art_field_bytes        =  %8zd (%2.0f%% of art file bytes)\n"
                                  "art_method_bytes       =  %8zd (%2.0f%% of art file bytes)\n"
                                  "dex_cache_arrays_bytes =  %8zd (%2.0f%% of art file bytes)\n"
                                  "interned_string_bytes  =  %8zd (%2.0f%% of art file bytes)\n"
                                  "class_table_bytes      =  %8zd (%2.0f%% of art file bytes)\n"
                                  "bitmap_bytes           =  %8zd (%2.0f%% of art file bytes)\n"
                                  "alignment_bytes        =  %8zd (%2.0f%% of art file bytes)\n\n",
                                  header_bytes, PercentOfFileBytes(header_bytes),
                                  object_bytes, PercentOfFileBytes(object_bytes),
                                  art_field_bytes, PercentOfFileBytes(art_field_bytes),
                                  art_method_bytes, PercentOfFileBytes(art_method_bytes),
                                  dex_cache_arrays_bytes,
                                  PercentOfFileBytes(dex_cache_arrays_bytes),
                                  interned_strings_bytes,
                                  PercentOfFileBytes(interned_strings_bytes),
                                  class_table_bytes, PercentOfFileBytes(class_table_bytes),
                                  bitmap_bytes, PercentOfFileBytes(bitmap_bytes),
                                  alignment_bytes, PercentOfFileBytes(alignment_bytes))
            << std::flush;
        CHECK_EQ(file_bytes,
                 header_bytes + object_bytes + art_field_bytes + art_method_bytes +
                 dex_cache_arrays_bytes + interned_strings_bytes + class_table_bytes +
                 bitmap_bytes + alignment_bytes);
      }

      os << "object_bytes breakdown:\n";
      size_t object_bytes_total = 0;
      for (const auto& sizes_and_count : sizes_and_counts) {
        const std::string& descriptor(sizes_and_count.first);
        double average = static_cast<double>(sizes_and_count.second.bytes) /
            static_cast<double>(sizes_and_count.second.count);
        double percent = PercentOfObjectBytes(sizes_and_count.second.bytes);
        os << StringPrintf("%32s %8zd bytes %6zd instances "
                           "(%4.0f bytes/instance) %2.0f%% of object_bytes\n",
                           descriptor.c_str(), sizes_and_count.second.bytes,
                           sizes_and_count.second.count, average, percent);
        object_bytes_total += sizes_and_count.second.bytes;
      }
      os << "\n" << std::flush;
      CHECK_EQ(object_bytes, object_bytes_total);

      os << StringPrintf("oat_file_bytes               = %8zd\n"
                         "managed_code_bytes           = %8zd (%2.0f%% of oat file bytes)\n"
                         "native_to_managed_code_bytes = %8zd (%2.0f%% of oat file bytes)\n\n"
                         "class_initializer_code_bytes = %8zd (%2.0f%% of oat file bytes)\n"
                         "large_initializer_code_bytes = %8zd (%2.0f%% of oat file bytes)\n"
                         "large_method_code_bytes      = %8zd (%2.0f%% of oat file bytes)\n\n",
                         oat_file_bytes,
                         managed_code_bytes,
                         PercentOfOatBytes(managed_code_bytes),
                         native_to_managed_code_bytes,
                         PercentOfOatBytes(native_to_managed_code_bytes),
                         class_initializer_code_bytes,
                         PercentOfOatBytes(class_initializer_code_bytes),
                         large_initializer_code_bytes,
                         PercentOfOatBytes(large_initializer_code_bytes),
                         large_method_code_bytes,
                         PercentOfOatBytes(large_method_code_bytes))
            << "DexFile sizes:\n";
      for (const std::pair<std::string, size_t>& oat_dex_file_size : oat_dex_file_sizes) {
        os << StringPrintf("%s = %zd (%2.0f%% of oat file bytes)\n",
                           oat_dex_file_size.first.c_str(), oat_dex_file_size.second,
                           PercentOfOatBytes(oat_dex_file_size.second));
      }

      os << "\n" << StringPrintf("vmap_table_bytes       = %7zd (%2.0f%% of oat file bytes)\n\n",
                                 vmap_table_bytes, PercentOfOatBytes(vmap_table_bytes))
         << std::flush;

      os << StringPrintf("dex_instruction_bytes = %zd\n", dex_instruction_bytes)
         << StringPrintf("managed_code_bytes expansion = %.2f (ignoring deduplication %.2f)\n\n",
                         static_cast<double>(managed_code_bytes) /
                             static_cast<double>(dex_instruction_bytes),
                         static_cast<double>(managed_code_bytes_ignoring_deduplication) /
                             static_cast<double>(dex_instruction_bytes))
         << std::flush;

      DumpOutliers(os);
    }
  } stats_;

 private:
  enum {
    // Number of bytes for a constructor to be considered large. Based on the 1000 basic block
    // threshold, we assume 2 bytes per instruction and 2 instructions per block.
    kLargeConstructorDexBytes = 4000,
    // Number of bytes for a method to be considered large. Based on the 4000 basic block
    // threshold, we assume 2 bytes per instruction and 2 instructions per block.
    kLargeMethodDexBytes = 16000
  };

  // For performance, use the *os_ directly for anything that doesn't need indentation
  // and prepare an indentation stream with default indentation 1.
  std::ostream* os_;
  VariableIndentationOutputStream vios_;
  ScopedIndentation indent1_;

  gc::space::ImageSpace& image_space_;
  const ImageHeader& image_header_;
  std::unique_ptr<OatDumper> oat_dumper_;
  OatDumperOptions* oat_dumper_options_;
  std::set<mirror::Object*> dex_caches_;

  DISALLOW_COPY_AND_ASSIGN(ImageDumper);
};

static int DumpImage(gc::space::ImageSpace* image_space,
                     OatDumperOptions* options,
                     std::ostream* os) REQUIRES_SHARED(Locks::mutator_lock_) {
  const ImageHeader& image_header = image_space->GetImageHeader();
  if (!image_header.IsValid()) {
    LOG(ERROR) << "Invalid image header " << image_space->GetImageLocation();
    return EXIT_FAILURE;
  }
  ImageDumper image_dumper(os, *image_space, image_header, options);
  if (!image_dumper.Dump()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static int DumpImages(Runtime* runtime, OatDumperOptions* options, std::ostream* os) {
  // Dumping the image, no explicit class loader.
  ScopedNullHandle<mirror::ClassLoader> null_class_loader;
  options->class_loader_ = &null_class_loader;

  ScopedObjectAccess soa(Thread::Current());
  if (options->app_image_ != nullptr) {
    if (options->app_oat_ == nullptr) {
      LOG(ERROR) << "Can not dump app image without app oat file";
      return EXIT_FAILURE;
    }
    // We can't know if the app image is 32 bits yet, but it contains pointers into the oat file.
    // We need to map the oat file in the low 4gb or else the fixup wont be able to fit oat file
    // pointers into 32 bit pointer sized ArtMethods.
    std::string error_msg;
    std::unique_ptr<OatFile> oat_file(OatFile::Open(/* zip_fd */ -1,
                                                    options->app_oat_,
                                                    options->app_oat_,
                                                    nullptr,
                                                    nullptr,
                                                    false,
                                                    /*low_4gb*/true,
                                                    nullptr,
                                                    &error_msg));
    if (oat_file == nullptr) {
      LOG(ERROR) << "Failed to open oat file " << options->app_oat_ << " with error " << error_msg;
      return EXIT_FAILURE;
    }
    std::unique_ptr<gc::space::ImageSpace> space(
        gc::space::ImageSpace::CreateFromAppImage(options->app_image_, oat_file.get(), &error_msg));
    if (space == nullptr) {
      LOG(ERROR) << "Failed to open app image " << options->app_image_ << " with error "
                 << error_msg;
    }
    // Open dex files for the image.
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    if (!runtime->GetClassLinker()->OpenImageDexFiles(space.get(), &dex_files, &error_msg)) {
      LOG(ERROR) << "Failed to open app image dex files " << options->app_image_ << " with error "
                 << error_msg;
    }
    // Dump the actual image.
    int result = DumpImage(space.get(), options, os);
    if (result != EXIT_SUCCESS) {
      return result;
    }
    // Fall through to dump the boot images.
  }

  gc::Heap* heap = runtime->GetHeap();
  CHECK(heap->HasBootImageSpace()) << "No image spaces";
  for (gc::space::ImageSpace* image_space : heap->GetBootImageSpaces()) {
    int result = DumpImage(image_space, options, os);
    if (result != EXIT_SUCCESS) {
      return result;
    }
  }
  return EXIT_SUCCESS;
}

static jobject InstallOatFile(Runtime* runtime,
                              std::unique_ptr<OatFile> oat_file,
                              std::vector<const DexFile*>* class_path)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  CHECK(self != nullptr);
  // Need well-known-classes.
  WellKnownClasses::Init(self->GetJniEnv());

  // Open dex files.
  OatFile* oat_file_ptr = oat_file.get();
  ClassLinker* class_linker = runtime->GetClassLinker();
  runtime->GetOatFileManager().RegisterOatFile(std::move(oat_file));
  for (const OatFile::OatDexFile* odf : oat_file_ptr->GetOatDexFiles()) {
    std::string error_msg;
    const DexFile* const dex_file = OpenDexFile(odf, &error_msg);
    CHECK(dex_file != nullptr) << error_msg;
    class_path->push_back(dex_file);
  }

  // Need a class loader. Fake that we're a compiler.
  // Note: this will run initializers through the unstarted runtime, so make sure it's
  //       initialized.
  interpreter::UnstartedRuntime::Initialize();

  jobject class_loader = class_linker->CreatePathClassLoader(self, *class_path);

  // Need to register dex files to get a working dex cache.
  for (const DexFile* dex_file : *class_path) {
    ObjPtr<mirror::DexCache> dex_cache = class_linker->RegisterDexFile(
        *dex_file, self->DecodeJObject(class_loader)->AsClassLoader());
    CHECK(dex_cache != nullptr);
  }

  return class_loader;
}

static int DumpOatWithRuntime(Runtime* runtime,
                              std::unique_ptr<OatFile> oat_file,
                              OatDumperOptions* options,
                              std::ostream* os) {
  CHECK(runtime != nullptr && oat_file != nullptr && options != nullptr);
  ScopedObjectAccess soa(Thread::Current());

  OatFile* oat_file_ptr = oat_file.get();
  std::vector<const DexFile*> class_path;
  jobject class_loader = InstallOatFile(runtime, std::move(oat_file), &class_path);

  // Use the class loader while dumping.
  StackHandleScope<1> scope(soa.Self());
  Handle<mirror::ClassLoader> loader_handle = scope.NewHandle(
      soa.Decode<mirror::ClassLoader>(class_loader));
  options->class_loader_ = &loader_handle;

  OatDumper oat_dumper(*oat_file_ptr, *options);
  bool success = oat_dumper.Dump(*os);
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int DumpOatWithoutRuntime(OatFile* oat_file, OatDumperOptions* options, std::ostream* os) {
  CHECK(oat_file != nullptr && options != nullptr);
  // No image = no class loader.
  ScopedNullHandle<mirror::ClassLoader> null_class_loader;
  options->class_loader_ = &null_class_loader;

  OatDumper oat_dumper(*oat_file, *options);
  bool success = oat_dumper.Dump(*os);
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int DumpOat(Runtime* runtime,
                   const char* oat_filename,
                   const char* dex_filename,
                   OatDumperOptions* options,
                   std::ostream* os) {
  if (dex_filename == nullptr) {
    LOG(WARNING) << "No dex filename provided, "
                 << "oatdump might fail if the oat file does not contain the dex code.";
  }
  std::string error_msg;
  std::unique_ptr<OatFile> oat_file(OatFile::Open(/* zip_fd */ -1,
                                                  oat_filename,
                                                  oat_filename,
                                                  nullptr,
                                                  nullptr,
                                                  false,
                                                  /*low_4gb*/false,
                                                  dex_filename,
                                                  &error_msg));
  if (oat_file == nullptr) {
    LOG(ERROR) << "Failed to open oat file from '" << oat_filename << "': " << error_msg;
    return EXIT_FAILURE;
  }

  if (runtime != nullptr) {
    return DumpOatWithRuntime(runtime, std::move(oat_file), options, os);
  } else {
    return DumpOatWithoutRuntime(oat_file.get(), options, os);
  }
}

static int SymbolizeOat(const char* oat_filename,
                        const char* dex_filename,
                        std::string& output_name,
                        bool no_bits) {
  std::string error_msg;
  std::unique_ptr<OatFile> oat_file(OatFile::Open(/* zip_fd */ -1,
                                                  oat_filename,
                                                  oat_filename,
                                                  nullptr,
                                                  nullptr,
                                                  false,
                                                  /*low_4gb*/false,
                                                  dex_filename,
                                                  &error_msg));
  if (oat_file == nullptr) {
    LOG(ERROR) << "Failed to open oat file from '" << oat_filename << "': " << error_msg;
    return EXIT_FAILURE;
  }

  bool result;
  // Try to produce an ELF file of the same type. This is finicky, as we have used 32-bit ELF
  // files for 64-bit code in the past.
  if (Is64BitInstructionSet(oat_file->GetOatHeader().GetInstructionSet())) {
    OatSymbolizer<ElfTypes64> oat_symbolizer(oat_file.get(), output_name, no_bits);
    result = oat_symbolizer.Symbolize();
  } else {
    OatSymbolizer<ElfTypes32> oat_symbolizer(oat_file.get(), output_name, no_bits);
    result = oat_symbolizer.Symbolize();
  }
  if (!result) {
    LOG(ERROR) << "Failed to symbolize";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

class IMTDumper {
 public:
  static bool Dump(Runtime* runtime,
                   const std::string& imt_file,
                   bool dump_imt_stats,
                   const char* oat_filename,
                   const char* dex_filename) {
    Thread* self = Thread::Current();

    ScopedObjectAccess soa(self);
    StackHandleScope<1> scope(self);
    MutableHandle<mirror::ClassLoader> class_loader = scope.NewHandle<mirror::ClassLoader>(nullptr);
    std::vector<const DexFile*> class_path;

    if (oat_filename != nullptr) {
      std::string error_msg;
      std::unique_ptr<OatFile> oat_file(OatFile::Open(/* zip_fd */ -1,
                                                      oat_filename,
                                                      oat_filename,
                                                      nullptr,
                                                      nullptr,
                                                      false,
                                                      /*low_4gb*/false,
                                                      dex_filename,
                                                      &error_msg));
      if (oat_file == nullptr) {
        LOG(ERROR) << "Failed to open oat file from '" << oat_filename << "': " << error_msg;
        return false;
      }

      class_loader.Assign(soa.Decode<mirror::ClassLoader>(
          InstallOatFile(runtime, std::move(oat_file), &class_path)));
    } else {
      class_loader.Assign(nullptr);  // Boot classloader. Just here for explicit documentation.
      class_path = runtime->GetClassLinker()->GetBootClassPath();
    }

    if (!imt_file.empty()) {
      return DumpImt(runtime, imt_file, class_loader);
    }

    if (dump_imt_stats) {
      return DumpImtStats(runtime, class_path, class_loader);
    }

    LOG(FATAL) << "Should not reach here";
    UNREACHABLE();
  }

 private:
  static bool DumpImt(Runtime* runtime,
                      const std::string& imt_file,
                      Handle<mirror::ClassLoader> h_class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    std::vector<std::string> lines = ReadCommentedInputFromFile(imt_file);
    std::unordered_set<std::string> prepared;

    for (const std::string& line : lines) {
      // A line should be either a class descriptor, in which case we will dump the complete IMT,
      // or a class descriptor and an interface method, in which case we will lookup the method,
      // determine its IMT slot, and check the class' IMT.
      size_t first_space = line.find(' ');
      if (first_space == std::string::npos) {
        DumpIMTForClass(runtime, line, h_class_loader, &prepared);
      } else {
        DumpIMTForMethod(runtime,
                         line.substr(0, first_space),
                         line.substr(first_space + 1, std::string::npos),
                         h_class_loader,
                         &prepared);
      }
      std::cerr << std::endl;
    }

    return true;
  }

  static bool DumpImtStats(Runtime* runtime,
                           const std::vector<const DexFile*>& dex_files,
                           Handle<mirror::ClassLoader> h_class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t without_imt = 0;
    size_t with_imt = 0;
    std::map<size_t, size_t> histogram;

    ClassLinker* class_linker = runtime->GetClassLinker();
    const PointerSize pointer_size = class_linker->GetImagePointerSize();
    std::unordered_set<std::string> prepared;

    Thread* self = Thread::Current();
    StackHandleScope<1> scope(self);
    MutableHandle<mirror::Class> h_klass(scope.NewHandle<mirror::Class>(nullptr));

    for (const DexFile* dex_file : dex_files) {
      for (uint32_t class_def_index = 0;
           class_def_index != dex_file->NumClassDefs();
           ++class_def_index) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
        const char* descriptor = dex_file->GetClassDescriptor(class_def);
        h_klass.Assign(class_linker->FindClass(self, descriptor, h_class_loader));
        if (h_klass == nullptr) {
          std::cerr << "Warning: could not load " << descriptor << std::endl;
          continue;
        }

        if (HasNoIMT(runtime, h_klass, pointer_size, &prepared)) {
          without_imt++;
          continue;
        }

        ImTable* im_table = PrepareAndGetImTable(runtime, h_klass, pointer_size, &prepared);
        if (im_table == nullptr) {
          // Should not happen, but accept.
          without_imt++;
          continue;
        }

        with_imt++;
        for (size_t imt_index = 0; imt_index != ImTable::kSize; ++imt_index) {
          ArtMethod* ptr = im_table->Get(imt_index, pointer_size);
          if (ptr->IsRuntimeMethod()) {
            if (ptr->IsImtUnimplementedMethod()) {
              histogram[0]++;
            } else {
              ImtConflictTable* current_table = ptr->GetImtConflictTable(pointer_size);
              histogram[current_table->NumEntries(pointer_size)]++;
            }
          } else {
            histogram[1]++;
          }
        }
      }
    }

    std::cerr << "IMT stats:"
              << std::endl << std::endl;

    std::cerr << "  " << with_imt << " classes with IMT."
              << std::endl << std::endl;
    std::cerr << "  " << without_imt << " classes without IMT (or copy from Object)."
              << std::endl << std::endl;

    double sum_one = 0;
    size_t count_one = 0;

    std::cerr << "  " << "IMT histogram" << std::endl;
    for (auto& bucket : histogram) {
      std::cerr << "    " << bucket.first << " " << bucket.second << std::endl;
      if (bucket.first > 0) {
        sum_one += bucket.second * bucket.first;
        count_one += bucket.second;
      }
    }

    double count_zero = count_one + histogram[0];
    std::cerr << "   Stats:" << std::endl;
    std::cerr << "     Average depth (including empty): " << (sum_one / count_zero) << std::endl;
    std::cerr << "     Average depth (excluding empty): " << (sum_one / count_one) << std::endl;

    return true;
  }

  // Return whether the given class has no IMT (or the one shared with java.lang.Object).
  static bool HasNoIMT(Runtime* runtime,
                       Handle<mirror::Class> klass,
                       const PointerSize pointer_size,
                       std::unordered_set<std::string>* prepared)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (klass->IsObjectClass() || !klass->ShouldHaveImt()) {
      return true;
    }

    if (klass->GetImt(pointer_size) == nullptr) {
      PrepareClass(runtime, klass, prepared);
    }

    mirror::Class* object_class = mirror::Class::GetJavaLangClass()->GetSuperClass();
    DCHECK(object_class->IsObjectClass());

    bool result = klass->GetImt(pointer_size) == object_class->GetImt(pointer_size);

    if (klass->GetIfTable()->Count() == 0) {
      DCHECK(result);
    }

    return result;
  }

  static void PrintTable(ImtConflictTable* table, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (table == nullptr) {
      std::cerr << "    <No IMT?>" << std::endl;
      return;
    }
    size_t table_index = 0;
    for (;;) {
      ArtMethod* ptr = table->GetInterfaceMethod(table_index, pointer_size);
      if (ptr == nullptr) {
        return;
      }
      table_index++;
      std::cerr << "    " << ptr->PrettyMethod(true) << std::endl;
    }
  }

  static ImTable* PrepareAndGetImTable(Runtime* runtime,
                                       Thread* self,
                                       Handle<mirror::ClassLoader> h_loader,
                                       const std::string& class_name,
                                       const PointerSize pointer_size,
                                       mirror::Class** klass_out,
                                       std::unordered_set<std::string>* prepared)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (class_name.empty()) {
      return nullptr;
    }

    std::string descriptor;
    if (class_name[0] == 'L') {
      descriptor = class_name;
    } else {
      descriptor = DotToDescriptor(class_name.c_str());
    }

    mirror::Class* klass = runtime->GetClassLinker()->FindClass(self, descriptor.c_str(), h_loader);

    if (klass == nullptr) {
      self->ClearException();
      std::cerr << "Did not find " <<  class_name << std::endl;
      *klass_out = nullptr;
      return nullptr;
    }

    StackHandleScope<1> scope(Thread::Current());
    Handle<mirror::Class> h_klass = scope.NewHandle<mirror::Class>(klass);

    ImTable* ret = PrepareAndGetImTable(runtime, h_klass, pointer_size, prepared);
    *klass_out = h_klass.Get();
    return ret;
  }

  static ImTable* PrepareAndGetImTable(Runtime* runtime,
                                       Handle<mirror::Class> h_klass,
                                       const PointerSize pointer_size,
                                       std::unordered_set<std::string>* prepared)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PrepareClass(runtime, h_klass, prepared);
    return h_klass->GetImt(pointer_size);
  }

  static void DumpIMTForClass(Runtime* runtime,
                              const std::string& class_name,
                              Handle<mirror::ClassLoader> h_loader,
                              std::unordered_set<std::string>* prepared)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const PointerSize pointer_size = runtime->GetClassLinker()->GetImagePointerSize();
    mirror::Class* klass;
    ImTable* imt = PrepareAndGetImTable(runtime,
                                        Thread::Current(),
                                        h_loader,
                                        class_name,
                                        pointer_size,
                                        &klass,
                                        prepared);
    if (imt == nullptr) {
      return;
    }

    std::cerr << class_name << std::endl << " IMT:" << std::endl;
    for (size_t index = 0; index < ImTable::kSize; ++index) {
      std::cerr << "  " << index << ":" << std::endl;
      ArtMethod* ptr = imt->Get(index, pointer_size);
      if (ptr->IsRuntimeMethod()) {
        if (ptr->IsImtUnimplementedMethod()) {
          std::cerr << "    <empty>" << std::endl;
        } else {
          ImtConflictTable* current_table = ptr->GetImtConflictTable(pointer_size);
          PrintTable(current_table, pointer_size);
        }
      } else {
        std::cerr << "    " << ptr->PrettyMethod(true) << std::endl;
      }
    }

    std::cerr << " Interfaces:" << std::endl;
    // Run through iftable, find methods that slot here, see if they fit.
    mirror::IfTable* if_table = klass->GetIfTable();
    for (size_t i = 0, num_interfaces = klass->GetIfTableCount(); i < num_interfaces; ++i) {
      mirror::Class* iface = if_table->GetInterface(i);
      std::string iface_name;
      std::cerr << "  " << iface->GetDescriptor(&iface_name) << std::endl;

      for (ArtMethod& iface_method : iface->GetVirtualMethods(pointer_size)) {
        uint32_t class_hash, name_hash, signature_hash;
        ImTable::GetImtHashComponents(&iface_method, &class_hash, &name_hash, &signature_hash);
        uint32_t imt_slot = ImTable::GetImtIndex(&iface_method);
        std::cerr << "    " << iface_method.PrettyMethod(true)
            << " slot=" << imt_slot
            << std::hex
            << " class_hash=0x" << class_hash
            << " name_hash=0x" << name_hash
            << " signature_hash=0x" << signature_hash
            << std::dec
            << std::endl;
      }
    }
  }

  static void DumpIMTForMethod(Runtime* runtime,
                               const std::string& class_name,
                               const std::string& method,
                               Handle<mirror::ClassLoader> h_loader,
                               std::unordered_set<std::string>* prepared)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const PointerSize pointer_size = runtime->GetClassLinker()->GetImagePointerSize();
    mirror::Class* klass;
    ImTable* imt = PrepareAndGetImTable(runtime,
                                        Thread::Current(),
                                        h_loader,
                                        class_name,
                                        pointer_size,
                                        &klass,
                                        prepared);
    if (imt == nullptr) {
      return;
    }

    std::cerr << class_name << " <" << method << ">" << std::endl;
    for (size_t index = 0; index < ImTable::kSize; ++index) {
      ArtMethod* ptr = imt->Get(index, pointer_size);
      if (ptr->IsRuntimeMethod()) {
        if (ptr->IsImtUnimplementedMethod()) {
          continue;
        }

        ImtConflictTable* current_table = ptr->GetImtConflictTable(pointer_size);
        if (current_table == nullptr) {
          continue;
        }

        size_t table_index = 0;
        for (;;) {
          ArtMethod* ptr2 = current_table->GetInterfaceMethod(table_index, pointer_size);
          if (ptr2 == nullptr) {
            break;
          }
          table_index++;

          std::string p_name = ptr2->PrettyMethod(true);
          if (android::base::StartsWith(p_name, method.c_str())) {
            std::cerr << "  Slot "
                      << index
                      << " ("
                      << current_table->NumEntries(pointer_size)
                      << ")"
                      << std::endl;
            PrintTable(current_table, pointer_size);
            return;
          }
        }
      } else {
        std::string p_name = ptr->PrettyMethod(true);
        if (android::base::StartsWith(p_name, method.c_str())) {
          std::cerr << "  Slot " << index << " (1)" << std::endl;
          std::cerr << "    " << p_name << std::endl;
        } else {
          // Run through iftable, find methods that slot here, see if they fit.
          mirror::IfTable* if_table = klass->GetIfTable();
          for (size_t i = 0, num_interfaces = klass->GetIfTableCount(); i < num_interfaces; ++i) {
            mirror::Class* iface = if_table->GetInterface(i);
            size_t num_methods = iface->NumDeclaredVirtualMethods();
            if (num_methods > 0) {
              for (ArtMethod& iface_method : iface->GetMethods(pointer_size)) {
                if (ImTable::GetImtIndex(&iface_method) == index) {
                  std::string i_name = iface_method.PrettyMethod(true);
                  if (android::base::StartsWith(i_name, method.c_str())) {
                    std::cerr << "  Slot " << index << " (1)" << std::endl;
                    std::cerr << "    " << p_name << " (" << i_name << ")" << std::endl;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Read lines from the given stream, dropping comments and empty lines
  static std::vector<std::string> ReadCommentedInputStream(std::istream& in_stream) {
    std::vector<std::string> output;
    while (in_stream.good()) {
      std::string dot;
      std::getline(in_stream, dot);
      if (android::base::StartsWith(dot, "#") || dot.empty()) {
        continue;
      }
      output.push_back(dot);
    }
    return output;
  }

  // Read lines from the given file, dropping comments and empty lines.
  static std::vector<std::string> ReadCommentedInputFromFile(const std::string& input_filename) {
    std::unique_ptr<std::ifstream> input_file(new std::ifstream(input_filename, std::ifstream::in));
    if (input_file.get() == nullptr) {
      LOG(ERROR) << "Failed to open input file " << input_filename;
      return std::vector<std::string>();
    }
    std::vector<std::string> result = ReadCommentedInputStream(*input_file);
    input_file->close();
    return result;
  }

  // Prepare a class, i.e., ensure it has a filled IMT. Will do so recursively for superclasses,
  // and note in the given set that the work was done.
  static void PrepareClass(Runtime* runtime,
                           Handle<mirror::Class> h_klass,
                           std::unordered_set<std::string>* done)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!h_klass->ShouldHaveImt()) {
      return;
    }

    std::string name;
    name = h_klass->GetDescriptor(&name);

    if (done->find(name) != done->end()) {
      return;
    }
    done->insert(name);

    if (h_klass->HasSuperClass()) {
      StackHandleScope<1> h(Thread::Current());
      PrepareClass(runtime, h.NewHandle<mirror::Class>(h_klass->GetSuperClass()), done);
    }

    if (!h_klass->IsTemp()) {
      runtime->GetClassLinker()->FillIMTAndConflictTables(h_klass.Get());
    }
  }
};

struct OatdumpArgs : public CmdlineArgs {
 protected:
  using Base = CmdlineArgs;

  virtual ParseStatus ParseCustom(const StringPiece& option,
                                  std::string* error_msg) OVERRIDE {
    {
      ParseStatus base_parse = Base::ParseCustom(option, error_msg);
      if (base_parse != kParseUnknownArgument) {
        return base_parse;
      }
    }

    if (option.starts_with("--oat-file=")) {
      oat_filename_ = option.substr(strlen("--oat-file=")).data();
    } else if (option.starts_with("--dex-file=")) {
      dex_filename_ = option.substr(strlen("--dex-file=")).data();
    } else if (option.starts_with("--image=")) {
      image_location_ = option.substr(strlen("--image=")).data();
    } else if (option == "--no-dump:vmap") {
      dump_vmap_ = false;
    } else if (option =="--dump:code_info_stack_maps") {
      dump_code_info_stack_maps_ = true;
    } else if (option == "--no-disassemble") {
      disassemble_code_ = false;
    } else if (option =="--header-only") {
      dump_header_only_ = true;
    } else if (option.starts_with("--symbolize=")) {
      oat_filename_ = option.substr(strlen("--symbolize=")).data();
      symbolize_ = true;
    } else if (option.starts_with("--only-keep-debug")) {
      only_keep_debug_ = true;
    } else if (option.starts_with("--class-filter=")) {
      class_filter_ = option.substr(strlen("--class-filter=")).data();
    } else if (option.starts_with("--method-filter=")) {
      method_filter_ = option.substr(strlen("--method-filter=")).data();
    } else if (option.starts_with("--list-classes")) {
      list_classes_ = true;
    } else if (option.starts_with("--list-methods")) {
      list_methods_ = true;
    } else if (option.starts_with("--export-dex-to=")) {
      export_dex_location_ = option.substr(strlen("--export-dex-to=")).data();
    } else if (option.starts_with("--addr2instr=")) {
      if (!ParseUint(option.substr(strlen("--addr2instr=")).data(), &addr2instr_)) {
        *error_msg = "Address conversion failed";
        return kParseError;
      }
    } else if (option.starts_with("--app-image=")) {
      app_image_ = option.substr(strlen("--app-image=")).data();
    } else if (option.starts_with("--app-oat=")) {
      app_oat_ = option.substr(strlen("--app-oat=")).data();
    } else if (option.starts_with("--dump-imt=")) {
      imt_dump_ = option.substr(strlen("--dump-imt=")).data();
    } else if (option == "--dump-imt-stats") {
      imt_stat_dump_ = true;
    } else {
      return kParseUnknownArgument;
    }

    return kParseOk;
  }

  virtual ParseStatus ParseChecks(std::string* error_msg) OVERRIDE {
    // Infer boot image location from the image location if possible.
    if (boot_image_location_ == nullptr) {
      boot_image_location_ = image_location_;
    }

    // Perform the parent checks.
    ParseStatus parent_checks = Base::ParseChecks(error_msg);
    if (parent_checks != kParseOk) {
      return parent_checks;
    }

    // Perform our own checks.
    if (image_location_ == nullptr && oat_filename_ == nullptr) {
      *error_msg = "Either --image or --oat-file must be specified";
      return kParseError;
    } else if (image_location_ != nullptr && oat_filename_ != nullptr) {
      *error_msg = "Either --image or --oat-file must be specified but not both";
      return kParseError;
    }

    return kParseOk;
  }

  virtual std::string GetUsage() const {
    std::string usage;

    usage +=
        "Usage: oatdump [options] ...\n"
        "    Example: oatdump --image=$ANDROID_PRODUCT_OUT/system/framework/boot.art\n"
        "    Example: adb shell oatdump --image=/system/framework/boot.art\n"
        "\n"
        // Either oat-file or image is required.
        "  --oat-file=<file.oat>: specifies an input oat filename.\n"
        "      Example: --oat-file=/system/framework/boot.oat\n"
        "\n"
        "  --image=<file.art>: specifies an input image location.\n"
        "      Example: --image=/system/framework/boot.art\n"
        "\n"
        "  --app-image=<file.art>: specifies an input app image. Must also have a specified\n"
        " boot image (with --image) and app oat file (with --app-oat).\n"
        "      Example: --app-image=app.art\n"
        "\n"
        "  --app-oat=<file.odex>: specifies an input app oat.\n"
        "      Example: --app-oat=app.odex\n"
        "\n";

    usage += Base::GetUsage();

    usage +=  // Optional.
        "  --no-dump:vmap may be used to disable vmap dumping.\n"
        "      Example: --no-dump:vmap\n"
        "\n"
        "  --dump:code_info_stack_maps enables dumping of stack maps in CodeInfo sections.\n"
        "      Example: --dump:code_info_stack_maps\n"
        "\n"
        "  --no-disassemble may be used to disable disassembly.\n"
        "      Example: --no-disassemble\n"
        "\n"
        "  --header-only may be used to print only the oat header.\n"
        "      Example: --header-only\n"
        "\n"
        "  --list-classes may be used to list target file classes (can be used with filters).\n"
        "      Example: --list-classes\n"
        "      Example: --list-classes --class-filter=com.example.foo\n"
        "\n"
        "  --list-methods may be used to list target file methods (can be used with filters).\n"
        "      Example: --list-methods\n"
        "      Example: --list-methods --class-filter=com.example --method-filter=foo\n"
        "\n"
        "  --symbolize=<file.oat>: output a copy of file.oat with elf symbols included.\n"
        "      Example: --symbolize=/system/framework/boot.oat\n"
        "\n"
        "  --only-keep-debug<file.oat>: Modifies the behaviour of --symbolize so that\n"
        "      .rodata and .text sections are omitted in the output file to save space.\n"
        "      Example: --symbolize=/system/framework/boot.oat --only-keep-debug\n"
        "\n"
        "  --class-filter=<class name>: only dumps classes that contain the filter.\n"
        "      Example: --class-filter=com.example.foo\n"
        "\n"
        "  --method-filter=<method name>: only dumps methods that contain the filter.\n"
        "      Example: --method-filter=foo\n"
        "\n"
        "  --export-dex-to=<directory>: may be used to export oat embedded dex files.\n"
        "      Example: --export-dex-to=/data/local/tmp\n"
        "\n"
        "  --addr2instr=<address>: output matching method disassembled code from relative\n"
        "                          address (e.g. PC from crash dump)\n"
        "      Example: --addr2instr=0x00001a3b\n"
        "\n"
        "  --dump-imt=<file.txt>: output IMT collisions (if any) for the given receiver\n"
        "                         types and interface methods in the given file. The file\n"
        "                         is read line-wise, where each line should either be a class\n"
        "                         name or descriptor, or a class name/descriptor and a prefix\n"
        "                         of a complete method name (separated by a whitespace).\n"
        "      Example: --dump-imt=imt.txt\n"
        "\n"
        "  --dump-imt-stats: output IMT statistics for the given boot image\n"
        "      Example: --dump-imt-stats"
        "\n";

    return usage;
  }

 public:
  const char* oat_filename_ = nullptr;
  const char* dex_filename_ = nullptr;
  const char* class_filter_ = "";
  const char* method_filter_ = "";
  const char* image_location_ = nullptr;
  std::string elf_filename_prefix_;
  std::string imt_dump_;
  bool dump_vmap_ = true;
  bool dump_code_info_stack_maps_ = false;
  bool disassemble_code_ = true;
  bool symbolize_ = false;
  bool only_keep_debug_ = false;
  bool list_classes_ = false;
  bool list_methods_ = false;
  bool dump_header_only_ = false;
  bool imt_stat_dump_ = false;
  uint32_t addr2instr_ = 0;
  const char* export_dex_location_ = nullptr;
  const char* app_image_ = nullptr;
  const char* app_oat_ = nullptr;
};

struct OatdumpMain : public CmdlineMain<OatdumpArgs> {
  virtual bool NeedsRuntime() OVERRIDE {
    CHECK(args_ != nullptr);

    // If we are only doing the oat file, disable absolute_addresses. Keep them for image dumping.
    bool absolute_addresses = (args_->oat_filename_ == nullptr);

    oat_dumper_options_.reset(new OatDumperOptions(
        args_->dump_vmap_,
        args_->dump_code_info_stack_maps_,
        args_->disassemble_code_,
        absolute_addresses,
        args_->class_filter_,
        args_->method_filter_,
        args_->list_classes_,
        args_->list_methods_,
        args_->dump_header_only_,
        args_->export_dex_location_,
        args_->app_image_,
        args_->app_oat_,
        args_->addr2instr_));

    return (args_->boot_image_location_ != nullptr ||
            args_->image_location_ != nullptr ||
            !args_->imt_dump_.empty()) &&
          !args_->symbolize_;
  }

  virtual bool ExecuteWithoutRuntime() OVERRIDE {
    CHECK(args_ != nullptr);
    CHECK(args_->oat_filename_ != nullptr);

    MemMap::Init();

    if (args_->symbolize_) {
      // ELF has special kind of section called SHT_NOBITS which allows us to create
      // sections which exist but their data is omitted from the ELF file to save space.
      // This is what "strip --only-keep-debug" does when it creates separate ELF file
      // with only debug data. We use it in similar way to exclude .rodata and .text.
      bool no_bits = args_->only_keep_debug_;
      return SymbolizeOat(args_->oat_filename_, args_->dex_filename_, args_->output_name_, no_bits)
          == EXIT_SUCCESS;
    } else {
      return DumpOat(nullptr,
                     args_->oat_filename_,
                     args_->dex_filename_,
                     oat_dumper_options_.get(),
                     args_->os_) == EXIT_SUCCESS;
    }
  }

  virtual bool ExecuteWithRuntime(Runtime* runtime) {
    CHECK(args_ != nullptr);

    if (!args_->imt_dump_.empty() || args_->imt_stat_dump_) {
      return IMTDumper::Dump(runtime,
                             args_->imt_dump_,
                             args_->imt_stat_dump_,
                             args_->oat_filename_,
                             args_->dex_filename_);
    }

    if (args_->oat_filename_ != nullptr) {
      return DumpOat(runtime,
                     args_->oat_filename_,
                     args_->dex_filename_,
                     oat_dumper_options_.get(),
                     args_->os_) == EXIT_SUCCESS;
    }

    return DumpImages(runtime, oat_dumper_options_.get(), args_->os_) == EXIT_SUCCESS;
  }

  std::unique_ptr<OatDumperOptions> oat_dumper_options_;
};

}  // namespace art

int main(int argc, char** argv) {
  // Output all logging to stderr.
  android::base::SetLogger(android::base::StderrLogger);

  art::OatdumpMain main;
  return main.Main(argc, argv);
}
