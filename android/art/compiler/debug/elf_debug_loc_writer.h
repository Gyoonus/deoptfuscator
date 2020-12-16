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

#ifndef ART_COMPILER_DEBUG_ELF_DEBUG_LOC_WRITER_H_
#define ART_COMPILER_DEBUG_ELF_DEBUG_LOC_WRITER_H_

#include <cstring>
#include <map>

#include "arch/instruction_set.h"
#include "compiled_method.h"
#include "debug/dwarf/debug_info_entry_writer.h"
#include "debug/dwarf/register.h"
#include "debug/method_debug_info.h"
#include "stack_map.h"

namespace art {
namespace debug {
using Reg = dwarf::Reg;

static Reg GetDwarfCoreReg(InstructionSet isa, int machine_reg) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return Reg::ArmCore(machine_reg);
    case InstructionSet::kArm64:
      return Reg::Arm64Core(machine_reg);
    case InstructionSet::kX86:
      return Reg::X86Core(machine_reg);
    case InstructionSet::kX86_64:
      return Reg::X86_64Core(machine_reg);
    case InstructionSet::kMips:
      return Reg::MipsCore(machine_reg);
    case InstructionSet::kMips64:
      return Reg::Mips64Core(machine_reg);
    case InstructionSet::kNone:
      LOG(FATAL) << "No instruction set";
  }
  UNREACHABLE();
}

static Reg GetDwarfFpReg(InstructionSet isa, int machine_reg) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return Reg::ArmFp(machine_reg);
    case InstructionSet::kArm64:
      return Reg::Arm64Fp(machine_reg);
    case InstructionSet::kX86:
      return Reg::X86Fp(machine_reg);
    case InstructionSet::kX86_64:
      return Reg::X86_64Fp(machine_reg);
    case InstructionSet::kMips:
      return Reg::MipsFp(machine_reg);
    case InstructionSet::kMips64:
      return Reg::Mips64Fp(machine_reg);
    case InstructionSet::kNone:
      LOG(FATAL) << "No instruction set";
  }
  UNREACHABLE();
}

struct VariableLocation {
  uint32_t low_pc;  // Relative to compilation unit.
  uint32_t high_pc;  // Relative to compilation unit.
  DexRegisterLocation reg_lo;  // May be None if the location is unknown.
  DexRegisterLocation reg_hi;  // Most significant bits of 64-bit value.
};

// Get the location of given dex register (e.g. stack or machine register).
// Note that the location might be different based on the current pc.
// The result will cover all ranges where the variable is in scope.
// PCs corresponding to stackmap with dex register map are accurate,
// all other PCs are best-effort only.
static std::vector<VariableLocation> GetVariableLocations(
    const MethodDebugInfo* method_info,
    const std::vector<DexRegisterMap>& dex_register_maps,
    uint16_t vreg,
    bool is64bitValue,
    uint64_t compilation_unit_code_address,
    uint32_t dex_pc_low,
    uint32_t dex_pc_high,
    InstructionSet isa) {
  std::vector<VariableLocation> variable_locations;

  // Get stack maps sorted by pc (they might not be sorted internally).
  // TODO(dsrbecky) Remove this once stackmaps get sorted by pc.
  const CodeInfo code_info(method_info->code_info);
  const CodeInfoEncoding encoding = code_info.ExtractEncoding();
  std::map<uint32_t, uint32_t> stack_maps;  // low_pc -> stack_map_index.
  for (uint32_t s = 0; s < code_info.GetNumberOfStackMaps(encoding); s++) {
    StackMap stack_map = code_info.GetStackMapAt(s, encoding);
    DCHECK(stack_map.IsValid());
    if (!stack_map.HasDexRegisterMap(encoding.stack_map.encoding)) {
      // The compiler creates stackmaps without register maps at the start of
      // basic blocks in order to keep instruction-accurate line number mapping.
      // However, we never stop at those (breakpoint locations always have map).
      // Therefore, for the purpose of local variables, we ignore them.
      // The main reason for this is to save space by avoiding undefined gaps.
      continue;
    }
    const uint32_t pc_offset = stack_map.GetNativePcOffset(encoding.stack_map.encoding, isa);
    DCHECK_LE(pc_offset, method_info->code_size);
    DCHECK_LE(compilation_unit_code_address, method_info->code_address);
    const uint32_t low_pc = dchecked_integral_cast<uint32_t>(
        method_info->code_address + pc_offset - compilation_unit_code_address);
    stack_maps.emplace(low_pc, s);
  }

  // Create entries for the requested register based on stack map data.
  for (auto it = stack_maps.begin(); it != stack_maps.end(); it++) {
    const uint32_t low_pc = it->first;
    const uint32_t stack_map_index = it->second;
    const StackMap& stack_map = code_info.GetStackMapAt(stack_map_index, encoding);
    auto next_it = it;
    next_it++;
    const uint32_t high_pc = next_it != stack_maps.end()
      ? next_it->first
      : method_info->code_address + method_info->code_size - compilation_unit_code_address;
    DCHECK_LE(low_pc, high_pc);
    if (low_pc == high_pc) {
      continue;  // Ignore if the address range is empty.
    }

    // Check that the stack map is in the requested range.
    uint32_t dex_pc = stack_map.GetDexPc(encoding.stack_map.encoding);
    if (!(dex_pc_low <= dex_pc && dex_pc < dex_pc_high)) {
      // The variable is not in scope at this PC. Therefore omit the entry.
      // Note that this is different to None() entry which means in scope, but unknown location.
      continue;
    }

    // Find the location of the dex register.
    DexRegisterLocation reg_lo = DexRegisterLocation::None();
    DexRegisterLocation reg_hi = DexRegisterLocation::None();
    DCHECK_LT(stack_map_index, dex_register_maps.size());
    DexRegisterMap dex_register_map = dex_register_maps[stack_map_index];
    DCHECK(dex_register_map.IsValid());
    CodeItemDataAccessor accessor(*method_info->dex_file, method_info->code_item);
    reg_lo = dex_register_map.GetDexRegisterLocation(
        vreg, accessor.RegistersSize(), code_info, encoding);
    if (is64bitValue) {
      reg_hi = dex_register_map.GetDexRegisterLocation(
          vreg + 1, accessor.RegistersSize(), code_info, encoding);
    }

    // Add location entry for this address range.
    if (!variable_locations.empty() &&
        variable_locations.back().reg_lo == reg_lo &&
        variable_locations.back().reg_hi == reg_hi &&
        variable_locations.back().high_pc == low_pc) {
      // Merge with the previous entry (extend its range).
      variable_locations.back().high_pc = high_pc;
    } else {
      variable_locations.push_back({low_pc, high_pc, reg_lo, reg_hi});
    }
  }

  return variable_locations;
}

// Write table into .debug_loc which describes location of dex register.
// The dex register might be valid only at some points and it might
// move between machine registers and stack.
static void WriteDebugLocEntry(const MethodDebugInfo* method_info,
                               const std::vector<DexRegisterMap>& dex_register_maps,
                               uint16_t vreg,
                               bool is64bitValue,
                               uint64_t compilation_unit_code_address,
                               uint32_t dex_pc_low,
                               uint32_t dex_pc_high,
                               InstructionSet isa,
                               dwarf::DebugInfoEntryWriter<>* debug_info,
                               std::vector<uint8_t>* debug_loc_buffer,
                               std::vector<uint8_t>* debug_ranges_buffer) {
  using Kind = DexRegisterLocation::Kind;
  if (method_info->code_info == nullptr || dex_register_maps.empty()) {
    return;
  }

  std::vector<VariableLocation> variable_locations = GetVariableLocations(
      method_info,
      dex_register_maps,
      vreg,
      is64bitValue,
      compilation_unit_code_address,
      dex_pc_low,
      dex_pc_high,
      isa);

  // Write .debug_loc entries.
  dwarf::Writer<> debug_loc(debug_loc_buffer);
  const size_t debug_loc_offset = debug_loc.size();
  const bool is64bit = Is64BitInstructionSet(isa);
  std::vector<uint8_t> expr_buffer;
  for (const VariableLocation& variable_location : variable_locations) {
    // Translate dex register location to DWARF expression.
    // Note that 64-bit value might be split to two distinct locations.
    // (for example, two 32-bit machine registers, or even stack and register)
    dwarf::Expression expr(&expr_buffer);
    DexRegisterLocation reg_lo = variable_location.reg_lo;
    DexRegisterLocation reg_hi = variable_location.reg_hi;
    for (int piece = 0; piece < (is64bitValue ? 2 : 1); piece++) {
      DexRegisterLocation reg_loc = (piece == 0 ? reg_lo : reg_hi);
      const Kind kind = reg_loc.GetKind();
      const int32_t value = reg_loc.GetValue();
      if (kind == Kind::kInStack) {
        // The stack offset is relative to SP. Make it relative to CFA.
        expr.WriteOpFbreg(value - method_info->frame_size_in_bytes);
        if (piece == 0 && reg_hi.GetKind() == Kind::kInStack &&
            reg_hi.GetValue() == value + 4) {
          break;  // the high word is correctly implied by the low word.
        }
      } else if (kind == Kind::kInRegister) {
        expr.WriteOpReg(GetDwarfCoreReg(isa, value).num());
        if (piece == 0 && reg_hi.GetKind() == Kind::kInRegisterHigh &&
            reg_hi.GetValue() == value) {
          break;  // the high word is correctly implied by the low word.
        }
      } else if (kind == Kind::kInFpuRegister) {
        if ((isa == InstructionSet::kArm || isa == InstructionSet::kThumb2) &&
            piece == 0 && reg_hi.GetKind() == Kind::kInFpuRegister &&
            reg_hi.GetValue() == value + 1 && value % 2 == 0) {
          // Translate S register pair to D register (e.g. S4+S5 to D2).
          expr.WriteOpReg(Reg::ArmDp(value / 2).num());
          break;
        }
        expr.WriteOpReg(GetDwarfFpReg(isa, value).num());
        if (piece == 0 && reg_hi.GetKind() == Kind::kInFpuRegisterHigh &&
            reg_hi.GetValue() == reg_lo.GetValue()) {
          break;  // the high word is correctly implied by the low word.
        }
      } else if (kind == Kind::kConstant) {
        expr.WriteOpConsts(value);
        expr.WriteOpStackValue();
      } else if (kind == Kind::kNone) {
        break;
      } else {
        // kInStackLargeOffset and kConstantLargeValue are hidden by GetKind().
        // kInRegisterHigh and kInFpuRegisterHigh should be handled by
        // the special cases above and they should not occur alone.
        LOG(WARNING) << "Unexpected register location: " << kind
                     << " (This can indicate either a bug in the dexer when generating"
                     << " local variable information, or a bug in ART compiler."
                     << " Please file a bug at go/art-bug)";
        break;
      }
      if (is64bitValue) {
        // Write the marker which is needed by split 64-bit values.
        // This code is skipped by the special cases.
        expr.WriteOpPiece(4);
      }
    }

    if (expr.size() > 0) {
      if (is64bit) {
        debug_loc.PushUint64(variable_location.low_pc);
        debug_loc.PushUint64(variable_location.high_pc);
      } else {
        debug_loc.PushUint32(variable_location.low_pc);
        debug_loc.PushUint32(variable_location.high_pc);
      }
      // Write the expression.
      debug_loc.PushUint16(expr.size());
      debug_loc.PushData(expr.data());
    } else {
      // Do not generate .debug_loc if the location is not known.
    }
  }
  // Write end-of-list entry.
  if (is64bit) {
    debug_loc.PushUint64(0);
    debug_loc.PushUint64(0);
  } else {
    debug_loc.PushUint32(0);
    debug_loc.PushUint32(0);
  }

  // Write .debug_ranges entries.
  // This includes ranges where the variable is in scope but the location is not known.
  dwarf::Writer<> debug_ranges(debug_ranges_buffer);
  size_t debug_ranges_offset = debug_ranges.size();
  for (size_t i = 0; i < variable_locations.size(); i++) {
    uint32_t low_pc = variable_locations[i].low_pc;
    uint32_t high_pc = variable_locations[i].high_pc;
    while (i + 1 < variable_locations.size() && variable_locations[i+1].low_pc == high_pc) {
      // Merge address range with the next entry.
      high_pc = variable_locations[++i].high_pc;
    }
    if (is64bit) {
      debug_ranges.PushUint64(low_pc);
      debug_ranges.PushUint64(high_pc);
    } else {
      debug_ranges.PushUint32(low_pc);
      debug_ranges.PushUint32(high_pc);
    }
  }
  // Write end-of-list entry.
  if (is64bit) {
    debug_ranges.PushUint64(0);
    debug_ranges.PushUint64(0);
  } else {
    debug_ranges.PushUint32(0);
    debug_ranges.PushUint32(0);
  }

  // Simple de-duplication - check whether this entry is same as the last one (or tail of it).
  size_t debug_ranges_entry_size = debug_ranges.size() - debug_ranges_offset;
  if (debug_ranges_offset >= debug_ranges_entry_size) {
    size_t previous_offset = debug_ranges_offset - debug_ranges_entry_size;
    if (memcmp(debug_ranges_buffer->data() + previous_offset,
               debug_ranges_buffer->data() + debug_ranges_offset,
               debug_ranges_entry_size) == 0) {
      // Remove what we have just written and use the last entry instead.
      debug_ranges_buffer->resize(debug_ranges_offset);
      debug_ranges_offset = previous_offset;
    }
  }

  // Write attributes to .debug_info.
  debug_info->WriteSecOffset(dwarf::DW_AT_location, debug_loc_offset);
  debug_info->WriteSecOffset(dwarf::DW_AT_start_scope, debug_ranges_offset);
}

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_ELF_DEBUG_LOC_WRITER_H_

