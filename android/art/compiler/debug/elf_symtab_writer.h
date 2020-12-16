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
 */

#ifndef ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_
#define ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_

#include <map>
#include <unordered_set>

#include "base/utils.h"
#include "debug/debug_info.h"
#include "debug/method_debug_info.h"
#include "dex/dex_file-inl.h"
#include "dex/code_item_accessors.h"
#include "linker/elf_builder.h"

namespace art {
namespace debug {

// The ARM specification defines three special mapping symbols
// $a, $t and $d which mark ARM, Thumb and data ranges respectively.
// These symbols can be used by tools, for example, to pretty
// print instructions correctly.  Objdump will use them if they
// exist, but it will still work well without them.
// However, these extra symbols take space, so let's just generate
// one symbol which marks the whole .text section as code.
// Note that ARM's Streamline requires it to match function symbol.
constexpr bool kGenerateArmMappingSymbol = true;

// Magic name for .symtab symbols which enumerate dex files used
// by this ELF file (currently mmapped inside the .dex section).
constexpr const char* kDexFileSymbolName = "$dexfile";

template <typename ElfTypes>
static void WriteDebugSymbols(linker::ElfBuilder<ElfTypes>* builder,
                              bool mini_debug_info,
                              const DebugInfo& debug_info) {
  uint64_t mapping_symbol_address = std::numeric_limits<uint64_t>::max();
  const auto* text = builder->GetText();
  auto* strtab = builder->GetStrTab();
  auto* symtab = builder->GetSymTab();

  if (debug_info.Empty()) {
    return;
  }

  // Find all addresses which contain deduped methods.
  // The first instance of method is not marked deduped_, but the rest is.
  std::unordered_set<uint64_t> deduped_addresses;
  for (const MethodDebugInfo& info : debug_info.compiled_methods) {
    if (info.deduped) {
      deduped_addresses.insert(info.code_address);
    }
    if (kGenerateArmMappingSymbol && info.isa == InstructionSet::kThumb2) {
      uint64_t address = info.code_address;
      address += info.is_code_address_text_relative ? text->GetAddress() : 0;
      mapping_symbol_address = std::min(mapping_symbol_address, address);
    }
  }

  strtab->Start();
  strtab->Write("");  // strtab should start with empty string.
  // Generate ARM mapping symbols. ELF local symbols must be added first.
  if (mapping_symbol_address != std::numeric_limits<uint64_t>::max()) {
    symtab->Add(strtab->Write("$t"), text, mapping_symbol_address, 0, STB_LOCAL, STT_NOTYPE);
  }
  // Add symbols for compiled methods.
  for (const MethodDebugInfo& info : debug_info.compiled_methods) {
    if (info.deduped) {
      continue;  // Add symbol only for the first instance.
    }
    size_t name_offset;
    if (!info.custom_name.empty()) {
      name_offset = strtab->Write(info.custom_name);
    } else {
      DCHECK(info.dex_file != nullptr);
      std::string name = info.dex_file->PrettyMethod(info.dex_method_index, !mini_debug_info);
      if (deduped_addresses.find(info.code_address) != deduped_addresses.end()) {
        name += " [DEDUPED]";
      }
      name_offset = strtab->Write(name);
    }

    uint64_t address = info.code_address;
    address += info.is_code_address_text_relative ? text->GetAddress() : 0;
    // Add in code delta, e.g., thumb bit 0 for Thumb2 code.
    address += CompiledMethod::CodeDelta(info.isa);
    symtab->Add(name_offset, text, address, info.code_size, STB_GLOBAL, STT_FUNC);
  }
  // Add symbols for dex files.
  if (!debug_info.dex_files.empty() && builder->GetDex()->Exists()) {
    auto dex = builder->GetDex();
    for (auto it : debug_info.dex_files) {
      uint64_t dex_address = dex->GetAddress() + it.first /* offset within the section */;
      const DexFile* dex_file = it.second;
      typename ElfTypes::Word dex_name = strtab->Write(kDexFileSymbolName);
      symtab->Add(dex_name, dex, dex_address, dex_file->Size(), STB_GLOBAL, STT_FUNC);
    }
  }
  strtab->End();

  // Symbols are buffered and written after names (because they are smaller).
  symtab->WriteCachedSection();
}

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_SYMTAB_WRITER_H_

