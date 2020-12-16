/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "dwarf_test.h"

#include "debug/dwarf/debug_frame_opcode_writer.h"
#include "debug/dwarf/debug_info_entry_writer.h"
#include "debug/dwarf/debug_line_opcode_writer.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/headers.h"
#include "gtest/gtest.h"

namespace art {
namespace dwarf {

// Run the tests only on host since we need objdump.
#ifndef ART_TARGET_ANDROID

constexpr CFIFormat kCFIFormat = DW_DEBUG_FRAME_FORMAT;

TEST_F(DwarfTest, DebugFrame) {
  const bool is64bit = false;

  // Pick offset value which would catch Uleb vs Sleb errors.
  const int offset = 40000;
  ASSERT_EQ(UnsignedLeb128Size(offset / 4), 2u);
  ASSERT_EQ(SignedLeb128Size(offset / 4), 3u);
  DW_CHECK("Data alignment factor: -4");
  const Reg reg(6);

  // Test the opcodes in the order mentioned in the spec.
  // There are usually several encoding variations of each opcode.
  DebugFrameOpCodeWriter<> opcodes;
  DW_CHECK("FDE");
  int pc = 0;
  for (int i : {0, 1, 0x3F, 0x40, 0xFF, 0x100, 0xFFFF, 0x10000}) {
    pc += i;
    opcodes.AdvancePC(pc);
  }
  DW_CHECK_NEXT("DW_CFA_advance_loc: 1 to 01000001");
  DW_CHECK_NEXT("DW_CFA_advance_loc: 63 to 01000040");
  DW_CHECK_NEXT("DW_CFA_advance_loc1: 64 to 01000080");
  DW_CHECK_NEXT("DW_CFA_advance_loc1: 255 to 0100017f");
  DW_CHECK_NEXT("DW_CFA_advance_loc2: 256 to 0100027f");
  DW_CHECK_NEXT("DW_CFA_advance_loc2: 65535 to 0101027e");
  DW_CHECK_NEXT("DW_CFA_advance_loc4: 65536 to 0102027e");
  opcodes.DefCFA(reg, offset);
  DW_CHECK_NEXT("DW_CFA_def_cfa: r6 (esi) ofs 40000");
  opcodes.DefCFA(reg, -offset);
  DW_CHECK_NEXT("DW_CFA_def_cfa_sf: r6 (esi) ofs -40000");
  opcodes.DefCFARegister(reg);
  DW_CHECK_NEXT("DW_CFA_def_cfa_register: r6 (esi)");
  opcodes.DefCFAOffset(offset);
  DW_CHECK_NEXT("DW_CFA_def_cfa_offset: 40000");
  opcodes.DefCFAOffset(-offset);
  DW_CHECK_NEXT("DW_CFA_def_cfa_offset_sf: -40000");
  uint8_t expr[] = { 0 };
  opcodes.DefCFAExpression(expr, arraysize(expr));
  DW_CHECK_NEXT("DW_CFA_def_cfa_expression");
  opcodes.Undefined(reg);
  DW_CHECK_NEXT("DW_CFA_undefined: r6 (esi)");
  opcodes.SameValue(reg);
  DW_CHECK_NEXT("DW_CFA_same_value: r6 (esi)");
  opcodes.Offset(Reg(0x3F), -offset);
  // Bad register likely means that it does not exist on x86,
  // but we want to test high register numbers anyway.
  DW_CHECK_NEXT("DW_CFA_offset: bad register: r63 at cfa-40000");
  opcodes.Offset(Reg(0x40), -offset);
  DW_CHECK_NEXT("DW_CFA_offset_extended: bad register: r64 at cfa-40000");
  opcodes.Offset(Reg(0x40), offset);
  DW_CHECK_NEXT("DW_CFA_offset_extended_sf: bad register: r64 at cfa+40000");
  opcodes.ValOffset(reg, -offset);
  DW_CHECK_NEXT("DW_CFA_val_offset: r6 (esi) at cfa-40000");
  opcodes.ValOffset(reg, offset);
  DW_CHECK_NEXT("DW_CFA_val_offset_sf: r6 (esi) at cfa+40000");
  opcodes.Register(reg, Reg(1));
  DW_CHECK_NEXT("DW_CFA_register: r6 (esi) in r1 (ecx)");
  opcodes.Expression(reg, expr, arraysize(expr));
  DW_CHECK_NEXT("DW_CFA_expression: r6 (esi)");
  opcodes.ValExpression(reg, expr, arraysize(expr));
  DW_CHECK_NEXT("DW_CFA_val_expression: r6 (esi)");
  opcodes.Restore(Reg(0x3F));
  DW_CHECK_NEXT("DW_CFA_restore: bad register: r63");
  opcodes.Restore(Reg(0x40));
  DW_CHECK_NEXT("DW_CFA_restore_extended: bad register: r64");
  opcodes.Restore(reg);
  DW_CHECK_NEXT("DW_CFA_restore: r6 (esi)");
  opcodes.RememberState();
  DW_CHECK_NEXT("DW_CFA_remember_state");
  opcodes.RestoreState();
  DW_CHECK_NEXT("DW_CFA_restore_state");
  opcodes.Nop();
  DW_CHECK_NEXT("DW_CFA_nop");

  // Also test helpers.
  opcodes.DefCFA(Reg(4), 100);  // ESP
  DW_CHECK_NEXT("DW_CFA_def_cfa: r4 (esp) ofs 100");
  opcodes.AdjustCFAOffset(8);
  DW_CHECK_NEXT("DW_CFA_def_cfa_offset: 108");
  opcodes.RelOffset(Reg(0), 0);  // push R0
  DW_CHECK_NEXT("DW_CFA_offset: r0 (eax) at cfa-108");
  opcodes.RelOffset(Reg(1), 4);  // push R1
  DW_CHECK_NEXT("DW_CFA_offset: r1 (ecx) at cfa-104");
  opcodes.RelOffsetForMany(Reg(2), 8, 1 | (1 << 3), 4);  // push R2 and R5
  DW_CHECK_NEXT("DW_CFA_offset: r2 (edx) at cfa-100");
  DW_CHECK_NEXT("DW_CFA_offset: r5 (ebp) at cfa-96");
  opcodes.RestoreMany(Reg(2), 1 | (1 << 3));  // pop R2 and R5
  DW_CHECK_NEXT("DW_CFA_restore: r2 (edx)");
  DW_CHECK_NEXT("DW_CFA_restore: r5 (ebp)");

  DebugFrameOpCodeWriter<> initial_opcodes;
  WriteCIE(is64bit, Reg(is64bit ? 16 : 8),
           initial_opcodes, kCFIFormat, &debug_frame_data_);
  std::vector<uintptr_t> debug_frame_patches;
  std::vector<uintptr_t> expected_patches = { 28 };
  WriteFDE(is64bit, 0, 0, 0x01000000, 0x01000000, ArrayRef<const uint8_t>(*opcodes.data()),
           kCFIFormat, 0, &debug_frame_data_, &debug_frame_patches);

  EXPECT_EQ(expected_patches, debug_frame_patches);
  CheckObjdumpOutput(is64bit, "-W");
}

TEST_F(DwarfTest, DebugFrame64) {
  constexpr bool is64bit = true;
  DebugFrameOpCodeWriter<> initial_opcodes;
  WriteCIE(is64bit, Reg(16),
           initial_opcodes, kCFIFormat, &debug_frame_data_);
  DebugFrameOpCodeWriter<> opcodes;
  std::vector<uintptr_t> debug_frame_patches;
  std::vector<uintptr_t> expected_patches = { 32 };
  WriteFDE(is64bit, 0, 0, 0x0100000000000000, 0x0200000000000000,
           ArrayRef<const uint8_t>(*opcodes.data()),
                     kCFIFormat, 0, &debug_frame_data_, &debug_frame_patches);
  DW_CHECK("FDE cie=00000000 pc=100000000000000..300000000000000");

  EXPECT_EQ(expected_patches, debug_frame_patches);
  CheckObjdumpOutput(is64bit, "-W");
}

// Test x86_64 register mapping. It is the only non-trivial architecture.
// ARM, X86, and Mips have: dwarf_reg = art_reg + constant.
TEST_F(DwarfTest, x86_64_RegisterMapping) {
  constexpr bool is64bit = true;
  DebugFrameOpCodeWriter<> opcodes;
  for (int i = 0; i < 16; i++) {
    opcodes.RelOffset(Reg::X86_64Core(i), 0);
  }
  DW_CHECK("FDE");
  DW_CHECK_NEXT("DW_CFA_offset: r0 (rax)");
  DW_CHECK_NEXT("DW_CFA_offset: r2 (rcx)");
  DW_CHECK_NEXT("DW_CFA_offset: r1 (rdx)");
  DW_CHECK_NEXT("DW_CFA_offset: r3 (rbx)");
  DW_CHECK_NEXT("DW_CFA_offset: r7 (rsp)");
  DW_CHECK_NEXT("DW_CFA_offset: r6 (rbp)");
  DW_CHECK_NEXT("DW_CFA_offset: r4 (rsi)");
  DW_CHECK_NEXT("DW_CFA_offset: r5 (rdi)");
  DW_CHECK_NEXT("DW_CFA_offset: r8 (r8)");
  DW_CHECK_NEXT("DW_CFA_offset: r9 (r9)");
  DW_CHECK_NEXT("DW_CFA_offset: r10 (r10)");
  DW_CHECK_NEXT("DW_CFA_offset: r11 (r11)");
  DW_CHECK_NEXT("DW_CFA_offset: r12 (r12)");
  DW_CHECK_NEXT("DW_CFA_offset: r13 (r13)");
  DW_CHECK_NEXT("DW_CFA_offset: r14 (r14)");
  DW_CHECK_NEXT("DW_CFA_offset: r15 (r15)");
  DebugFrameOpCodeWriter<> initial_opcodes;
  WriteCIE(is64bit, Reg(16),
           initial_opcodes, kCFIFormat, &debug_frame_data_);
  std::vector<uintptr_t> debug_frame_patches;
  WriteFDE(is64bit, 0, 0, 0x0100000000000000, 0x0200000000000000,
           ArrayRef<const uint8_t>(*opcodes.data()),
                     kCFIFormat, 0, &debug_frame_data_, &debug_frame_patches);

  CheckObjdumpOutput(is64bit, "-W");
}

TEST_F(DwarfTest, DebugLine) {
  const bool is64bit = false;
  const int code_factor_bits = 1;
  DebugLineOpCodeWriter<> opcodes(is64bit, code_factor_bits);

  std::vector<std::string> include_directories;
  include_directories.push_back("/path/to/source");
  DW_CHECK("/path/to/source");

  std::vector<FileEntry> files {
    { "file0.c", 0, 1000, 2000 },
    { "file1.c", 1, 1000, 2000 },
    { "file2.c", 1, 1000, 2000 },
  };
  DW_CHECK("1\t0\t1000\t2000\tfile0.c");
  DW_CHECK_NEXT("2\t1\t1000\t2000\tfile1.c");
  DW_CHECK_NEXT("3\t1\t1000\t2000\tfile2.c");

  DW_CHECK("Line Number Statements");
  opcodes.SetAddress(0x01000000);
  DW_CHECK_NEXT("Extended opcode 2: set Address to 0x1000000");
  opcodes.AddRow();
  DW_CHECK_NEXT("Copy");
  opcodes.AdvancePC(0x01000100);
  DW_CHECK_NEXT("Advance PC by 256 to 0x1000100");
  opcodes.SetFile(2);
  DW_CHECK_NEXT("Set File Name to entry 2 in the File Name Table");
  opcodes.AdvanceLine(3);
  DW_CHECK_NEXT("Advance Line by 2 to 3");
  opcodes.SetColumn(4);
  DW_CHECK_NEXT("Set column to 4");
  opcodes.SetIsStmt(true);
  DW_CHECK_NEXT("Set is_stmt to 1");
  opcodes.SetIsStmt(false);
  DW_CHECK_NEXT("Set is_stmt to 0");
  opcodes.SetBasicBlock();
  DW_CHECK_NEXT("Set basic block");
  opcodes.SetPrologueEnd();
  DW_CHECK_NEXT("Set prologue_end to true");
  opcodes.SetEpilogueBegin();
  DW_CHECK_NEXT("Set epilogue_begin to true");
  opcodes.SetISA(5);
  DW_CHECK_NEXT("Set ISA to 5");
  opcodes.EndSequence();
  DW_CHECK_NEXT("Extended opcode 1: End of Sequence");
  opcodes.DefineFile("file.c", 0, 1000, 2000);
  DW_CHECK_NEXT("Extended opcode 3: define new File Table entry");
  DW_CHECK_NEXT("Entry\tDir\tTime\tSize\tName");
  DW_CHECK_NEXT("1\t0\t1000\t2000\tfile.c");

  std::vector<uintptr_t> debug_line_patches;
  std::vector<uintptr_t> expected_patches = { 87 };
  WriteDebugLineTable(include_directories, files, opcodes,
                      0, &debug_line_data_, &debug_line_patches);

  EXPECT_EQ(expected_patches, debug_line_patches);
  CheckObjdumpOutput(is64bit, "-W");
}

// DWARF has special one byte codes which advance PC and line at the same time.
TEST_F(DwarfTest, DebugLineSpecialOpcodes) {
  const bool is64bit = false;
  const int code_factor_bits = 1;
  uint32_t pc = 0x01000000;
  int line = 1;
  DebugLineOpCodeWriter<> opcodes(is64bit, code_factor_bits);
  opcodes.SetAddress(pc);
  size_t num_rows = 0;
  DW_CHECK("Line Number Statements:");
  DW_CHECK("Special opcode");
  DW_CHECK("Advance PC by constant");
  DW_CHECK("Decoded dump of debug contents of section .debug_line:");
  DW_CHECK("Line number    Starting address");
  for (int addr_delta = 0; addr_delta < 80; addr_delta += 2) {
    for (int line_delta = 16; line_delta >= -16; --line_delta) {
      pc += addr_delta;
      line += line_delta;
      opcodes.AddRow(pc, line);
      num_rows++;
      ASSERT_EQ(opcodes.CurrentAddress(), pc);
      ASSERT_EQ(opcodes.CurrentLine(), line);
      char expected[1024];
      sprintf(expected, "%i           0x%x", line, pc);
      DW_CHECK_NEXT(expected);
    }
  }
  EXPECT_LT(opcodes.data()->size(), num_rows * 3);

  std::vector<std::string> directories;
  std::vector<FileEntry> files = { { "file.c", 0, 1000, 2000 } };
  std::vector<uintptr_t> debug_line_patches;
  WriteDebugLineTable(directories, files, opcodes,
                      0, &debug_line_data_, &debug_line_patches);

  CheckObjdumpOutput(is64bit, "-W -WL");
}

TEST_F(DwarfTest, DebugInfo) {
  constexpr bool is64bit = false;
  DebugAbbrevWriter<> debug_abbrev(&debug_abbrev_data_);
  DebugInfoEntryWriter<> info(is64bit, &debug_abbrev);
  DW_CHECK("Contents of the .debug_info section:");
  info.StartTag(dwarf::DW_TAG_compile_unit);
  DW_CHECK("Abbrev Number: 1 (DW_TAG_compile_unit)");
  info.WriteStrp(dwarf::DW_AT_producer, "Compiler name", &debug_str_data_);
  DW_CHECK_NEXT("DW_AT_producer    : (indirect string, offset: 0x0): Compiler name");
  info.WriteAddr(dwarf::DW_AT_low_pc, 0x01000000);
  DW_CHECK_NEXT("DW_AT_low_pc      : 0x1000000");
  info.WriteAddr(dwarf::DW_AT_high_pc, 0x02000000);
  DW_CHECK_NEXT("DW_AT_high_pc     : 0x2000000");
  info.StartTag(dwarf::DW_TAG_subprogram);
  DW_CHECK("Abbrev Number: 2 (DW_TAG_subprogram)");
  info.WriteStrp(dwarf::DW_AT_name, "Foo", &debug_str_data_);
  DW_CHECK_NEXT("DW_AT_name        : (indirect string, offset: 0xe): Foo");
  info.WriteAddr(dwarf::DW_AT_low_pc, 0x01010000);
  DW_CHECK_NEXT("DW_AT_low_pc      : 0x1010000");
  info.WriteAddr(dwarf::DW_AT_high_pc, 0x01020000);
  DW_CHECK_NEXT("DW_AT_high_pc     : 0x1020000");
  info.EndTag();  // DW_TAG_subprogram
  info.StartTag(dwarf::DW_TAG_subprogram);
  DW_CHECK("Abbrev Number: 2 (DW_TAG_subprogram)");
  info.WriteStrp(dwarf::DW_AT_name, "Bar", &debug_str_data_);
  DW_CHECK_NEXT("DW_AT_name        : (indirect string, offset: 0x12): Bar");
  info.WriteAddr(dwarf::DW_AT_low_pc, 0x01020000);
  DW_CHECK_NEXT("DW_AT_low_pc      : 0x1020000");
  info.WriteAddr(dwarf::DW_AT_high_pc, 0x01030000);
  DW_CHECK_NEXT("DW_AT_high_pc     : 0x1030000");
  info.EndTag();  // DW_TAG_subprogram
  info.EndTag();  // DW_TAG_compile_unit
  // Test that previous list was properly terminated and empty children.
  info.StartTag(dwarf::DW_TAG_compile_unit);
  info.EndTag();  // DW_TAG_compile_unit

  // The abbrev table is just side product, but check it as well.
  DW_CHECK("Abbrev Number: 3 (DW_TAG_compile_unit)");
  DW_CHECK("Contents of the .debug_abbrev section:");
  DW_CHECK("1      DW_TAG_compile_unit    [has children]");
  DW_CHECK_NEXT("DW_AT_producer     DW_FORM_strp");
  DW_CHECK_NEXT("DW_AT_low_pc       DW_FORM_addr");
  DW_CHECK_NEXT("DW_AT_high_pc      DW_FORM_addr");
  DW_CHECK("2      DW_TAG_subprogram    [no children]");
  DW_CHECK_NEXT("DW_AT_name         DW_FORM_strp");
  DW_CHECK_NEXT("DW_AT_low_pc       DW_FORM_addr");
  DW_CHECK_NEXT("DW_AT_high_pc      DW_FORM_addr");
  DW_CHECK("3      DW_TAG_compile_unit    [no children]");

  std::vector<uintptr_t> debug_info_patches;
  std::vector<uintptr_t> expected_patches = { 16, 20, 29, 33, 42, 46 };
  dwarf::WriteDebugInfoCU(0 /* debug_abbrev_offset */, info,
                          0, &debug_info_data_, &debug_info_patches);

  EXPECT_EQ(expected_patches, debug_info_patches);
  CheckObjdumpOutput(is64bit, "-W");
}

#endif  // ART_TARGET_ANDROID

}  // namespace dwarf
}  // namespace art
