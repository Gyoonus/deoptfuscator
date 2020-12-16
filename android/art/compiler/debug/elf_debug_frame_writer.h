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

#ifndef ART_COMPILER_DEBUG_ELF_DEBUG_FRAME_WRITER_H_
#define ART_COMPILER_DEBUG_ELF_DEBUG_FRAME_WRITER_H_

#include <vector>

#include "arch/instruction_set.h"
#include "debug/dwarf/debug_frame_opcode_writer.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/headers.h"
#include "debug/method_debug_info.h"
#include "linker/elf_builder.h"

namespace art {
namespace debug {

static void WriteCIE(InstructionSet isa,
                     dwarf::CFIFormat format,
                     std::vector<uint8_t>* buffer) {
  using Reg = dwarf::Reg;
  // Scratch registers should be marked as undefined.  This tells the
  // debugger that its value in the previous frame is not recoverable.
  bool is64bit = Is64BitInstructionSet(isa);
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::ArmCore(13), 0);  // R13(SP).
      // core registers.
      for (int reg = 0; reg < 13; reg++) {
        if (reg < 4 || reg == 12) {
          opcodes.Undefined(Reg::ArmCore(reg));
        } else {
          opcodes.SameValue(Reg::ArmCore(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 16) {
          opcodes.Undefined(Reg::ArmFp(reg));
        } else {
          opcodes.SameValue(Reg::ArmFp(reg));
        }
      }
      auto return_reg = Reg::ArmCore(14);  // R14(LR).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case InstructionSet::kArm64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::Arm64Core(31), 0);  // R31(SP).
      // core registers.
      for (int reg = 0; reg < 30; reg++) {
        if (reg < 8 || reg == 16 || reg == 17) {
          opcodes.Undefined(Reg::Arm64Core(reg));
        } else {
          opcodes.SameValue(Reg::Arm64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 8 || reg >= 16) {
          opcodes.Undefined(Reg::Arm64Fp(reg));
        } else {
          opcodes.SameValue(Reg::Arm64Fp(reg));
        }
      }
      auto return_reg = Reg::Arm64Core(30);  // R30(LR).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case InstructionSet::kMips:
    case InstructionSet::kMips64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::MipsCore(29), 0);  // R29(SP).
      // core registers.
      for (int reg = 1; reg < 26; reg++) {
        if (reg < 16 || reg == 24 || reg == 25) {  // AT, V*, A*, T*.
          opcodes.Undefined(Reg::MipsCore(reg));
        } else {
          opcodes.SameValue(Reg::MipsCore(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 24) {
          opcodes.Undefined(Reg::Mips64Fp(reg));
        } else {
          opcodes.SameValue(Reg::Mips64Fp(reg));
        }
      }
      auto return_reg = Reg::MipsCore(31);  // R31(RA).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case InstructionSet::kX86: {
      // FIXME: Add fp registers once libunwind adds support for them. Bug: 20491296
      constexpr bool generate_opcodes_for_x86_fp = false;
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::X86Core(4), 4);   // R4(ESP).
      opcodes.Offset(Reg::X86Core(8), -4);  // R8(EIP).
      // core registers.
      for (int reg = 0; reg < 8; reg++) {
        if (reg <= 3) {
          opcodes.Undefined(Reg::X86Core(reg));
        } else if (reg == 4) {
          // Stack pointer.
        } else {
          opcodes.SameValue(Reg::X86Core(reg));
        }
      }
      // fp registers.
      if (generate_opcodes_for_x86_fp) {
        for (int reg = 0; reg < 8; reg++) {
          opcodes.Undefined(Reg::X86Fp(reg));
        }
      }
      auto return_reg = Reg::X86Core(8);  // R8(EIP).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case InstructionSet::kX86_64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(Reg::X86_64Core(4), 8);  // R4(RSP).
      opcodes.Offset(Reg::X86_64Core(16), -8);  // R16(RIP).
      // core registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg == 4) {
          // Stack pointer.
        } else if (reg < 12 && reg != 3 && reg != 5) {  // except EBX and EBP.
          opcodes.Undefined(Reg::X86_64Core(reg));
        } else {
          opcodes.SameValue(Reg::X86_64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg < 12) {
          opcodes.Undefined(Reg::X86_64Fp(reg));
        } else {
          opcodes.SameValue(Reg::X86_64Fp(reg));
        }
      }
      auto return_reg = Reg::X86_64Core(16);  // R16(RIP).
      WriteCIE(is64bit, return_reg, opcodes, format, buffer);
      return;
    }
    case InstructionSet::kNone:
      break;
  }
  LOG(FATAL) << "Cannot write CIE frame for ISA " << isa;
  UNREACHABLE();
}

template<typename ElfTypes>
void WriteCFISection(linker::ElfBuilder<ElfTypes>* builder,
                     const ArrayRef<const MethodDebugInfo>& method_infos,
                     dwarf::CFIFormat format,
                     bool write_oat_patches) {
  CHECK(format == dwarf::DW_DEBUG_FRAME_FORMAT || format == dwarf::DW_EH_FRAME_FORMAT);
  typedef typename ElfTypes::Addr Elf_Addr;

  // The methods can be written in any order.
  // Let's therefore sort them in the lexicographical order of the opcodes.
  // This has no effect on its own. However, if the final .debug_frame section is
  // compressed it reduces the size since similar opcodes sequences are grouped.
  std::vector<const MethodDebugInfo*> sorted_method_infos;
  sorted_method_infos.reserve(method_infos.size());
  for (size_t i = 0; i < method_infos.size(); i++) {
    if (!method_infos[i].cfi.empty() && !method_infos[i].deduped) {
      sorted_method_infos.push_back(&method_infos[i]);
    }
  }
  if (sorted_method_infos.empty()) {
    return;
  }
  std::stable_sort(
      sorted_method_infos.begin(),
      sorted_method_infos.end(),
      [](const MethodDebugInfo* lhs, const MethodDebugInfo* rhs) {
        ArrayRef<const uint8_t> l = lhs->cfi;
        ArrayRef<const uint8_t> r = rhs->cfi;
        return std::lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
      });

  std::vector<uint32_t> binary_search_table;
  std::vector<uintptr_t> patch_locations;
  if (format == dwarf::DW_EH_FRAME_FORMAT) {
    binary_search_table.reserve(2 * sorted_method_infos.size());
  } else {
    patch_locations.reserve(sorted_method_infos.size());
  }

  // Write .eh_frame/.debug_frame section.
  const bool is_debug_frame = format == dwarf::DW_DEBUG_FRAME_FORMAT;
  auto* cfi_section = (is_debug_frame ? builder->GetDebugFrame() : builder->GetEhFrame());
  {
    cfi_section->Start();
    const bool is64bit = Is64BitInstructionSet(builder->GetIsa());
    const Elf_Addr cfi_address = (is_debug_frame ? 0 : cfi_section->GetAddress());
    const Elf_Addr cie_address = cfi_address;
    Elf_Addr buffer_address = cfi_address;
    std::vector<uint8_t> buffer;  // Small temporary buffer.
    WriteCIE(builder->GetIsa(), format, &buffer);
    cfi_section->WriteFully(buffer.data(), buffer.size());
    buffer_address += buffer.size();
    buffer.clear();
    for (const MethodDebugInfo* mi : sorted_method_infos) {
      DCHECK(!mi->deduped);
      DCHECK(!mi->cfi.empty());
      const Elf_Addr code_address = mi->code_address +
          (mi->is_code_address_text_relative ? builder->GetText()->GetAddress() : 0);
      if (format == dwarf::DW_EH_FRAME_FORMAT) {
        binary_search_table.push_back(dchecked_integral_cast<uint32_t>(code_address));
        binary_search_table.push_back(dchecked_integral_cast<uint32_t>(buffer_address));
      }
      WriteFDE(is64bit, cfi_address, cie_address,
               code_address, mi->code_size,
               mi->cfi, format, buffer_address, &buffer,
               &patch_locations);
      cfi_section->WriteFully(buffer.data(), buffer.size());
      buffer_address += buffer.size();
      buffer.clear();
    }
    cfi_section->End();
  }

  if (format == dwarf::DW_EH_FRAME_FORMAT) {
    auto* header_section = builder->GetEhFrameHdr();
    header_section->Start();
    uint32_t header_address = dchecked_integral_cast<int32_t>(header_section->GetAddress());
    // Write .eh_frame_hdr section.
    std::vector<uint8_t> buffer;
    dwarf::Writer<> header(&buffer);
    header.PushUint8(1);  // Version.
    // Encoding of .eh_frame pointer - libunwind does not honor datarel here,
    // so we have to use pcrel which means relative to the pointer's location.
    header.PushUint8(dwarf::DW_EH_PE_pcrel | dwarf::DW_EH_PE_sdata4);
    // Encoding of binary search table size.
    header.PushUint8(dwarf::DW_EH_PE_udata4);
    // Encoding of binary search table addresses - libunwind supports only this
    // specific combination, which means relative to the start of .eh_frame_hdr.
    header.PushUint8(dwarf::DW_EH_PE_datarel | dwarf::DW_EH_PE_sdata4);
    // .eh_frame pointer
    header.PushInt32(cfi_section->GetAddress() - (header_address + 4u));
    // Binary search table size (number of entries).
    header.PushUint32(dchecked_integral_cast<uint32_t>(binary_search_table.size()/2));
    header_section->WriteFully(buffer.data(), buffer.size());
    // Binary search table.
    for (size_t i = 0; i < binary_search_table.size(); i++) {
      // Make addresses section-relative since we know the header address now.
      binary_search_table[i] -= header_address;
    }
    header_section->WriteFully(binary_search_table.data(), binary_search_table.size());
    header_section->End();
  } else {
    if (write_oat_patches) {
      builder->WritePatches(".debug_frame.oat_patches",
                            ArrayRef<const uintptr_t>(patch_locations));
    }
  }
}

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_DEBUG_FRAME_WRITER_H_

