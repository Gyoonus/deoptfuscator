/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "disassembler_mips.h"

#include <ostream>
#include <sstream>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "base/bit_utils.h"

using android::base::StringPrintf;

namespace art {
namespace mips {

struct MipsInstruction {
  uint32_t mask;
  uint32_t value;
  const char* name;
  const char* args_fmt;

  bool Matches(uint32_t instruction) const {
    return (instruction & mask) == value;
  }
};

static const char* gO32AbiRegNames[]  = {
  "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"
};

static const char* gN64AbiRegNames[]  = {
  "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
  "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"
};

static const uint32_t kOpcodeShift = 26;

static const uint32_t kCop1 = (17 << kOpcodeShift);
static const uint32_t kMsa = (30 << kOpcodeShift);  // MSA major opcode.

static const uint32_t kITypeMask = (0x3f << kOpcodeShift);
static const uint32_t kJTypeMask = (0x3f << kOpcodeShift);
static const uint32_t kRTypeMask = ((0x3f << kOpcodeShift) | (0x3f));
static const uint32_t kSpecial0Mask = (0x3f << kOpcodeShift);
static const uint32_t kSpecial2Mask = (0x3f << kOpcodeShift);
static const uint32_t kSpecial3Mask = (0x3f << kOpcodeShift);
static const uint32_t kFpMask = kRTypeMask;
static const uint32_t kMsaMask = kRTypeMask;
static const uint32_t kMsaSpecialMask = (0x3f << kOpcodeShift);

static const MipsInstruction gMipsInstructions[] = {
  // "sll r0, r0, 0" is the canonical "nop", used in delay slots.
  { 0xffffffff, 0, "nop", "" },

  // R-type instructions.
  { kRTypeMask, 0, "sll", "DTA", },
  // 0, 1, movci
  { kRTypeMask | (0x1f << 21), 2, "srl", "DTA", },
  { kRTypeMask, 3, "sra", "DTA", },
  { kRTypeMask | (0x1f << 6), 4, "sllv", "DTS", },
  { kRTypeMask | (0x1f << 6), 6, "srlv", "DTS", },
  { kRTypeMask | (0x1f << 6), (1 << 6) | 6, "rotrv", "DTS", },
  { kRTypeMask | (0x1f << 6), 7, "srav", "DTS", },
  { kRTypeMask, 8, "jr", "S", },
  { kRTypeMask | (0x1f << 11), 9 | (31 << 11), "jalr", "S", },  // rd = 31 is implicit.
  { kRTypeMask | (0x1f << 11), 9, "jr", "S", },  // rd = 0 is implicit.
  { kRTypeMask, 9, "jalr", "DS", },  // General case.
  { kRTypeMask | (0x1f << 6), 10, "movz", "DST", },
  { kRTypeMask | (0x1f << 6), 11, "movn", "DST", },
  { kRTypeMask, 12, "syscall", "", },  // TODO: code
  { kRTypeMask, 13, "break", "", },  // TODO: code
  { kRTypeMask, 15, "sync", "", },  // TODO: type
  { kRTypeMask, 16, "mfhi", "D", },
  { kRTypeMask, 17, "mthi", "S", },
  { kRTypeMask, 18, "mflo", "D", },
  { kRTypeMask, 19, "mtlo", "S", },
  { kRTypeMask | (0x1f << 6), 20, "dsllv", "DTS", },
  { kRTypeMask | (0x1f << 6), 22, "dsrlv", "DTS", },
  { kRTypeMask | (0x1f << 6), (1 << 6) | 22, "drotrv", "DTS", },
  { kRTypeMask | (0x1f << 6), 23, "dsrav", "DTS", },
  { kRTypeMask | (0x1f << 6), 24, "mult", "ST", },
  { kRTypeMask | (0x1f << 6), 25, "multu", "ST", },
  { kRTypeMask | (0x1f << 6), 26, "div", "ST", },
  { kRTypeMask | (0x1f << 6), 27, "divu", "ST", },
  { kRTypeMask | (0x1f << 6), 24 + (2 << 6), "mul", "DST", },
  { kRTypeMask | (0x1f << 6), 24 + (3 << 6), "muh", "DST", },
  { kRTypeMask | (0x1f << 6), 26 + (2 << 6), "div", "DST", },
  { kRTypeMask | (0x1f << 6), 26 + (3 << 6), "mod", "DST", },
  { kRTypeMask, 32, "add", "DST", },
  { kRTypeMask, 33, "addu", "DST", },
  { kRTypeMask, 34, "sub", "DST", },
  { kRTypeMask, 35, "subu", "DST", },
  { kRTypeMask, 36, "and", "DST", },
  { kRTypeMask | (0x1f << 16), 37 | (0 << 16), "move", "DS" },
  { kRTypeMask | (0x1f << 21), 37 | (0 << 21), "move", "DT" },
  { kRTypeMask, 37, "or", "DST", },
  { kRTypeMask, 38, "xor", "DST", },
  { kRTypeMask, 39, "nor", "DST", },
  { kRTypeMask, 42, "slt", "DST", },
  { kRTypeMask, 43, "sltu", "DST", },
  { kRTypeMask, 45, "daddu", "DST", },
  { kRTypeMask, 46, "dsub", "DST", },
  { kRTypeMask, 47, "dsubu", "DST", },
  // TODO: tge[u], tlt[u], teg, tne
  { kRTypeMask | (0x1f << 21), 56, "dsll", "DTA", },
  { kRTypeMask | (0x1f << 21), 58, "dsrl", "DTA", },
  { kRTypeMask | (0x1f << 21), (1 << 21) | 58, "drotr", "DTA", },
  { kRTypeMask | (0x1f << 21), 59, "dsra", "DTA", },
  { kRTypeMask | (0x1f << 21), 60, "dsll32", "DTA", },
  { kRTypeMask | (0x1f << 21), 62, "dsrl32", "DTA", },
  { kRTypeMask | (0x1f << 21), (1 << 21) | 62, "drotr32", "DTA", },
  { kRTypeMask | (0x1f << 21), 63, "dsra32", "DTA", },

  // SPECIAL0
  { kSpecial0Mask | 0x307ff, 1, "movf", "DSc" },
  { kSpecial0Mask | 0x307ff, 0x10001, "movt", "DSc" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 24, "mul", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 24, "muh", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 25, "mulu", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 25, "muhu", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 26, "div", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 26, "mod", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 27, "divu", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 27, "modu", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 28, "dmul", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 28, "dmuh", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 29, "dmulu", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 29, "dmuhu", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 30, "ddiv", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 30, "dmod", "DST" },
  { kSpecial0Mask | 0x7ff, (2 << 6) | 31, "ddivu", "DST" },
  { kSpecial0Mask | 0x7ff, (3 << 6) | 31, "dmodu", "DST" },
  { kSpecial0Mask | 0x7ff, (0 << 6) | 53, "seleqz", "DST" },
  { kSpecial0Mask | 0x7ff, (0 << 6) | 55, "selnez", "DST" },
  { kSpecial0Mask | (0x1f << 21) | 0x3f, (1 << 21) | 2, "rotr", "DTA", },
  { kSpecial0Mask | (0x1f << 16) | 0x7ff, (0x01 << 6) | 0x10, "clz", "DS" },
  { kSpecial0Mask | (0x1f << 16) | 0x7ff, (0x01 << 6) | 0x11, "clo", "DS" },
  { kSpecial0Mask | (0x1f << 16) | 0x7ff, (0x01 << 6) | 0x12, "dclz", "DS" },
  { kSpecial0Mask | (0x1f << 16) | 0x7ff, (0x01 << 6) | 0x13, "dclo", "DS" },
  { kSpecial0Mask | 0x73f, 0x05, "lsa", "DSTj" },
  { kSpecial0Mask | 0x73f, 0x15, "dlsa", "DSTj" },
  // TODO: sdbbp

  // SPECIAL2
  { kSpecial2Mask | 0x7ff, (28 << kOpcodeShift) | 2, "mul", "DST" },
  { kSpecial2Mask | 0x7ff, (28 << kOpcodeShift) | 32, "clz", "DS" },
  { kSpecial2Mask | 0x7ff, (28 << kOpcodeShift) | 33, "clo", "DS" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 0, "madd", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 1, "maddu", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 2, "mul", "DST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 4, "msub", "ST" },
  { kSpecial2Mask | 0xffff, (28 << kOpcodeShift) | 5, "msubu", "ST" },
  { kSpecial2Mask | 0x3f, (28 << kOpcodeShift) | 0x3f, "sdbbp", "" },  // TODO: code

  // SPECIAL3
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift), "ext", "TSAZ", },
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift) | 3, "dext", "TSAZ", },
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift) | 4, "ins", "TSAz", },
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift) | 5, "dinsm", "TSAJ", },
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift) | 6, "dinsu", "TSFz", },
  { kSpecial3Mask | 0x3f, (31 << kOpcodeShift) | 7, "dins", "TSAz", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | (16 << 6) | 32,
    "seb",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | (24 << 6) | 32,
    "seh",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | 32,
    "bitswap",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | 36,
    "dbitswap",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | (2 << 6) | 36,
    "dsbh",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | (5 << 6) | 36,
    "dshd",
    "DT", },
  { kSpecial3Mask | (0x1f << 21) | (0x1f << 6) | 0x3f,
    (31 << kOpcodeShift) | (2 << 6) | 32,
    "wsbh",
    "DT", },
  { kSpecial3Mask | 0x7f, (31 << kOpcodeShift) | 0x26, "sc", "Tl", },
  { kSpecial3Mask | 0x7f, (31 << kOpcodeShift) | 0x27, "scd", "Tl", },
  { kSpecial3Mask | 0x7f, (31 << kOpcodeShift) | 0x36, "ll", "Tl", },
  { kSpecial3Mask | 0x7f, (31 << kOpcodeShift) | 0x37, "lld", "Tl", },

  // J-type instructions.
  { kJTypeMask, 2 << kOpcodeShift, "j", "L" },
  { kJTypeMask, 3 << kOpcodeShift, "jal", "L" },

  // I-type instructions.
  { kITypeMask | (0x3ff << 16), 4 << kOpcodeShift, "b", "B" },
  { kITypeMask | (0x1f << 16), 4 << kOpcodeShift | (0 << 16), "beqz", "SB" },
  { kITypeMask | (0x1f << 21), 4 << kOpcodeShift | (0 << 21), "beqz", "TB" },
  { kITypeMask, 4 << kOpcodeShift, "beq", "STB" },
  { kITypeMask | (0x1f << 16), 5 << kOpcodeShift | (0 << 16), "bnez", "SB" },
  { kITypeMask | (0x1f << 21), 5 << kOpcodeShift | (0 << 21), "bnez", "TB" },
  { kITypeMask, 5 << kOpcodeShift, "bne", "STB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (1 << 16), "bgez", "SB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (0 << 16), "bltz", "SB" },
  { kITypeMask | (0x3ff << 16), 1 << kOpcodeShift | (16 << 16), "nal", "" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (16 << 16), "bltzal", "SB" },
  { kITypeMask | (0x3ff << 16), 1 << kOpcodeShift | (17 << 16), "bal", "B" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (17 << 16), "bgezal", "SB" },
  { kITypeMask | (0x1f << 16), 6 << kOpcodeShift | (0 << 16), "blez", "SB" },
  { kITypeMask, 6 << kOpcodeShift, "bgeuc", "STB" },
  { kITypeMask | (0x1f << 16), 7 << kOpcodeShift | (0 << 16), "bgtz", "SB" },
  { kITypeMask, 7 << kOpcodeShift, "bltuc", "STB" },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (6 << 16), "dahi", "Si", },
  { kITypeMask | (0x1f << 16), 1 << kOpcodeShift | (30 << 16), "dati", "Si", },

  { kITypeMask, 8 << kOpcodeShift, "beqc", "STB" },

  { kITypeMask | (0x1f << 21), 9 << kOpcodeShift | (0 << 21), "li", "Ti" },
  { kITypeMask, 9 << kOpcodeShift, "addiu", "TSi", },
  { kITypeMask, 10 << kOpcodeShift, "slti", "TSi", },
  { kITypeMask, 11 << kOpcodeShift, "sltiu", "TSi", },
  { kITypeMask, 12 << kOpcodeShift, "andi", "TSI", },
  { kITypeMask | (0x1f << 21), 13 << kOpcodeShift | (0 << 21), "li", "TI" },
  { kITypeMask, 13 << kOpcodeShift, "ori", "TSI", },
  { kITypeMask, 14 << kOpcodeShift, "xori", "TSI", },
  { kITypeMask | (0x1f << 21), 15 << kOpcodeShift, "lui", "Ti", },
  { kITypeMask, 15 << kOpcodeShift, "aui", "TSi", },

  { kITypeMask | (0x3e3 << 16), (17 << kOpcodeShift) | (8 << 21), "bc1f", "cB" },
  { kITypeMask | (0x3e3 << 16), (17 << kOpcodeShift) | (8 << 21) | (1 << 16), "bc1t", "cB" },
  { kITypeMask | (0x1f << 21), (17 << kOpcodeShift) | (9 << 21), "bc1eqz", "tB" },
  { kITypeMask | (0x1f << 21), (17 << kOpcodeShift) | (13 << 21), "bc1nez", "tB" },

  { kITypeMask | (0x1f << 21), 22 << kOpcodeShift, "blezc", "TB" },

  // TODO: de-dup
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (1  << 21) | (1  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (2  << 21) | (2  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (3  << 21) | (3  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (4  << 21) | (4  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (5  << 21) | (5  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (6  << 21) | (6  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (7  << 21) | (7  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (8  << 21) | (8  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (9  << 21) | (9  << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (10 << 21) | (10 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (11 << 21) | (11 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (12 << 21) | (12 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (13 << 21) | (13 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (14 << 21) | (14 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (15 << 21) | (15 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (16 << 21) | (16 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (17 << 21) | (17 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (18 << 21) | (18 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (19 << 21) | (19 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (20 << 21) | (20 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (21 << 21) | (21 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (22 << 21) | (22 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (23 << 21) | (23 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (24 << 21) | (24 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (25 << 21) | (25 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (26 << 21) | (26 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (27 << 21) | (27 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (28 << 21) | (28 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (29 << 21) | (29 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (30 << 21) | (30 << 16), "bgezc", "TB" },
  { kITypeMask | (0x3ff << 16), (22 << kOpcodeShift) | (31 << 21) | (31 << 16), "bgezc", "TB" },

  { kITypeMask, 22 << kOpcodeShift, "bgec", "STB" },

  { kITypeMask | (0x1f << 21), 23 << kOpcodeShift, "bgtzc", "TB" },

  // TODO: de-dup
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (1  << 21) | (1  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (2  << 21) | (2  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (3  << 21) | (3  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (4  << 21) | (4  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (5  << 21) | (5  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (6  << 21) | (6  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (7  << 21) | (7  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (8  << 21) | (8  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (9  << 21) | (9  << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (10 << 21) | (10 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (11 << 21) | (11 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (12 << 21) | (12 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (13 << 21) | (13 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (14 << 21) | (14 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (15 << 21) | (15 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (16 << 21) | (16 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (17 << 21) | (17 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (18 << 21) | (18 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (19 << 21) | (19 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (20 << 21) | (20 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (21 << 21) | (21 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (22 << 21) | (22 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (23 << 21) | (23 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (24 << 21) | (24 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (25 << 21) | (25 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (26 << 21) | (26 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (27 << 21) | (27 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (28 << 21) | (28 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (29 << 21) | (29 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (30 << 21) | (30 << 16), "bltzc", "TB" },
  { kITypeMask | (0x3ff << 16), (23 << kOpcodeShift) | (31 << 21) | (31 << 16), "bltzc", "TB" },

  { kITypeMask, 23 << kOpcodeShift, "bltc", "STB" },

  { kITypeMask, 24 << kOpcodeShift, "bnec", "STB" },

  { kITypeMask | (0x1f << 21), 25 << kOpcodeShift | (0 << 21), "dli", "Ti" },
  { kITypeMask, 25 << kOpcodeShift, "daddiu", "TSi", },
  { kITypeMask, 29 << kOpcodeShift, "daui", "TSi", },

  { kITypeMask, 32u << kOpcodeShift, "lb", "TO", },
  { kITypeMask, 33u << kOpcodeShift, "lh", "TO", },
  { kITypeMask, 34u << kOpcodeShift, "lwl", "TO", },
  { kITypeMask, 35u << kOpcodeShift, "lw", "TO", },
  { kITypeMask, 36u << kOpcodeShift, "lbu", "TO", },
  { kITypeMask, 37u << kOpcodeShift, "lhu", "TO", },
  { kITypeMask, 38u << kOpcodeShift, "lwr", "TO", },
  { kITypeMask, 39u << kOpcodeShift, "lwu", "TO", },
  { kITypeMask, 40u << kOpcodeShift, "sb", "TO", },
  { kITypeMask, 41u << kOpcodeShift, "sh", "TO", },
  { kITypeMask, 42u << kOpcodeShift, "swl", "TO", },
  { kITypeMask, 43u << kOpcodeShift, "sw", "TO", },
  { kITypeMask, 46u << kOpcodeShift, "swr", "TO", },
  { kITypeMask, 48u << kOpcodeShift, "ll", "TO", },
  { kITypeMask, 49u << kOpcodeShift, "lwc1", "tO", },
  { kJTypeMask, 50u << kOpcodeShift, "bc", "P" },
  { kITypeMask, 53u << kOpcodeShift, "ldc1", "tO", },
  { kITypeMask | (0x1f << 21), 54u << kOpcodeShift, "jic", "Ti" },
  { kITypeMask | (1 << 21), (54u << kOpcodeShift) | (1 << 21), "beqzc", "Sb" },  // TODO: de-dup?
  { kITypeMask | (1 << 22), (54u << kOpcodeShift) | (1 << 22), "beqzc", "Sb" },
  { kITypeMask | (1 << 23), (54u << kOpcodeShift) | (1 << 23), "beqzc", "Sb" },
  { kITypeMask | (1 << 24), (54u << kOpcodeShift) | (1 << 24), "beqzc", "Sb" },
  { kITypeMask | (1 << 25), (54u << kOpcodeShift) | (1 << 25), "beqzc", "Sb" },
  { kITypeMask, 55u << kOpcodeShift, "ld", "TO", },
  { kITypeMask, 56u << kOpcodeShift, "sc", "TO", },
  { kITypeMask, 57u << kOpcodeShift, "swc1", "tO", },
  { kJTypeMask, 58u << kOpcodeShift, "balc", "P" },
  { kITypeMask | (0x1f << 16), (59u << kOpcodeShift) | (30 << 16), "auipc", "Si" },
  { kITypeMask | (0x3 << 19), (59u << kOpcodeShift) | (0 << 19), "addiupc", "Sp" },
  { kITypeMask | (0x3 << 19), (59u << kOpcodeShift) | (1 << 19), "lwpc", "So" },
  { kITypeMask | (0x3 << 19), (59u << kOpcodeShift) | (2 << 19), "lwupc", "So" },
  { kITypeMask | (0x7 << 18), (59u << kOpcodeShift) | (6 << 18), "ldpc", "S0" },
  { kITypeMask, 61u << kOpcodeShift, "sdc1", "tO", },
  { kITypeMask | (0x1f << 21), 62u << kOpcodeShift, "jialc", "Ti" },
  { kITypeMask | (1 << 21), (62u << kOpcodeShift) | (1 << 21), "bnezc", "Sb" },  // TODO: de-dup?
  { kITypeMask | (1 << 22), (62u << kOpcodeShift) | (1 << 22), "bnezc", "Sb" },
  { kITypeMask | (1 << 23), (62u << kOpcodeShift) | (1 << 23), "bnezc", "Sb" },
  { kITypeMask | (1 << 24), (62u << kOpcodeShift) | (1 << 24), "bnezc", "Sb" },
  { kITypeMask | (1 << 25), (62u << kOpcodeShift) | (1 << 25), "bnezc", "Sb" },
  { kITypeMask, 63u << kOpcodeShift, "sd", "TO", },

  // Floating point.
  { kFpMask | (0x1f << 21), kCop1 | (0x00 << 21), "mfc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x01 << 21), "dmfc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x03 << 21), "mfhc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x04 << 21), "mtc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x05 << 21), "dmtc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x07 << 21), "mthc1", "Td" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 1, "cmp.un.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 2, "cmp.eq.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 3, "cmp.ueq.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 4, "cmp.lt.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 5, "cmp.ult.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 6, "cmp.le.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 7, "cmp.ule.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 17, "cmp.or.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 18, "cmp.une.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x14 << 21) | 19, "cmp.ne.s", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 1, "cmp.un.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 2, "cmp.eq.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 3, "cmp.ueq.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 4, "cmp.lt.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 5, "cmp.ult.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 6, "cmp.le.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 7, "cmp.ule.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 17, "cmp.or.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 18, "cmp.une.d", "adt" },
  { kFpMask | (0x1f << 21), kCop1 | (0x15 << 21) | 19, "cmp.ne.d", "adt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 0, "add", "fadt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 1, "sub", "fadt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 2, "mul", "fadt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 3, "div", "fadt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 4, "sqrt", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 5, "abs", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 6, "mov", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 7, "neg", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 8, "round.l", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 9, "trunc.l", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 10, "ceil.l", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 11, "floor.l", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 12, "round.w", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 13, "trunc.w", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 14, "ceil.w", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 15, "floor.w", "fad" },
  { kFpMask | (0x201 << 16), kCop1 | (0x200 << 16) | 17, "movf", "fadc" },
  { kFpMask | (0x201 << 16), kCop1 | (0x201 << 16) | 17, "movt", "fadc" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 18, "movz", "fadT" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 19, "movn", "fadT" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 20, "seleqz", "fadt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 23, "selnez", "fadt" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 26, "rint", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 27, "class", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 32, "cvt.s", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 33, "cvt.d", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 36, "cvt.w", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 37, "cvt.l", "fad" },
  { kFpMask | (0x21f << 16), kCop1 | (0x200 << 16) | 38, "cvt.ps", "fad" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 49, "c.un", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 50, "c.eq", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 51, "c.ueq", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 52, "c.olt", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 53, "c.ult", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 54, "c.ole", "fCdt" },
  { kFpMask | (0x10 << 21), kCop1 | (0x10 << 21) | 55, "c.ule", "fCdt" },
  { kFpMask, kCop1 | 0x10, "sel", "fadt" },
  { kFpMask, kCop1 | 0x1e, "max", "fadt" },
  { kFpMask, kCop1 | 0x1c, "min", "fadt" },

  // MSA instructions.
  { kMsaMask | (0x1f << 21), kMsa | (0x0 << 21) | 0x1e, "and.v", "kmn" },
  { kMsaMask | (0x1f << 21), kMsa | (0x1 << 21) | 0x1e, "or.v", "kmn" },
  { kMsaMask | (0x1f << 21), kMsa | (0x2 << 21) | 0x1e, "nor.v", "kmn" },
  { kMsaMask | (0x1f << 21), kMsa | (0x3 << 21) | 0x1e, "xor.v", "kmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x0 << 23) | 0xe, "addv", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x1 << 23) | 0xe, "subv", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0x11, "asub_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0x11, "asub_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x0 << 23) | 0x12, "mulv", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0x12, "div_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0x12, "div_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x6 << 23) | 0x12, "mod_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x7 << 23) | 0x12, "mod_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x0 << 23) | 0x10, "add_a", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0x10, "ave_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0x10, "ave_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x6 << 23) | 0x10, "aver_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x7 << 23) | 0x10, "aver_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x2 << 23) | 0xe, "max_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x3 << 23) | 0xe, "max_u", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0xe, "min_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0xe, "min_u", "Vkmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x0 << 22) | 0x1b, "fadd", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x1 << 22) | 0x1b, "fsub", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x2 << 22) | 0x1b, "fmul", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x3 << 22) | 0x1b, "fdiv", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0xe << 22) | 0x1b, "fmax", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0xc << 22) | 0x1b, "fmin", "Ukmn" },
  { kMsaMask | (0x1ff << 17), kMsa | (0x19e << 17) | 0x1e, "ffint_s", "ukm" },
  { kMsaMask | (0x1ff << 17), kMsa | (0x19c << 17) | 0x1e, "ftint_s", "ukm" },
  { kMsaMask | (0x7 << 23), kMsa | (0x0 << 23) | 0xd, "sll", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x1 << 23) | 0xd, "sra", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x2 << 23) | 0xd, "srl", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x0 << 23) | 0x9, "slli", "kmW" },
  { kMsaMask | (0x7 << 23), kMsa | (0x1 << 23) | 0x9, "srai", "kmW" },
  { kMsaMask | (0x7 << 23), kMsa | (0x2 << 23) | 0x9, "srli", "kmW" },
  { kMsaMask | (0x3ff << 16), kMsa | (0xbe << 16) | 0x19, "move.v", "km" },
  { kMsaMask | (0xf << 22), kMsa | (0x1 << 22) | 0x19, "splati", "kX" },
  { kMsaMask | (0xf << 22), kMsa | (0x2 << 22) | 0x19, "copy_s", "yX" },
  { kMsaMask | (0xf << 22), kMsa | (0x3 << 22) | 0x19, "copy_u", "yX" },
  { kMsaMask | (0xf << 22), kMsa | (0x4 << 22) | 0x19, "insert", "YD" },
  { kMsaMask | (0xff << 18), kMsa | (0xc0 << 18) | 0x1e, "fill", "vkD" },
  { kMsaMask | (0x7 << 23), kMsa | (0x6 << 23) | 0x7, "ldi", "kx" },
  { kMsaSpecialMask | (0xf << 2), kMsa | (0x8 << 2), "ld", "kw" },
  { kMsaSpecialMask | (0xf << 2), kMsa | (0x9 << 2), "st", "kw" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0x14, "ilvl", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0x14, "ilvr", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x6 << 23) | 0x14, "ilvev", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x7 << 23) | 0x14, "ilvod", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x1 << 23) | 0x12, "maddv", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x2 << 23) | 0x12, "msubv", "Vkmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x4 << 22) | 0x1b, "fmadd", "Ukmn" },
  { kMsaMask | (0xf << 22), kMsa | (0x5 << 22) | 0x1b, "fmsub", "Ukmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x4 << 23) | 0x15, "hadd_s", "Vkmn" },
  { kMsaMask | (0x7 << 23), kMsa | (0x5 << 23) | 0x15, "hadd_u", "Vkmn" },
};

static uint32_t ReadU32(const uint8_t* ptr) {
  // We only support little-endian MIPS.
  return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

const char* DisassemblerMips::RegName(uint32_t reg) {
  if (is_o32_abi_) {
    return gO32AbiRegNames[reg];
  } else {
    return gN64AbiRegNames[reg];
  }
}

size_t DisassemblerMips::Dump(std::ostream& os, const uint8_t* instr_ptr) {
  uint32_t instruction = ReadU32(instr_ptr);

  uint32_t rs = (instruction >> 21) & 0x1f;  // I-type, R-type.
  uint32_t rt = (instruction >> 16) & 0x1f;  // I-type, R-type.
  uint32_t rd = (instruction >> 11) & 0x1f;  // R-type.
  uint32_t sa = (instruction >>  6) & 0x1f;  // R-type.

  std::string opcode;
  std::ostringstream args;

  // TODO: remove this!
  uint32_t op = (instruction >> 26) & 0x3f;
  uint32_t function = (instruction & 0x3f);  // R-type.
  opcode = StringPrintf("op=%d fn=%d", op, function);

  for (size_t i = 0; i < arraysize(gMipsInstructions); ++i) {
    if (gMipsInstructions[i].Matches(instruction)) {
      opcode = gMipsInstructions[i].name;
      for (const char* args_fmt = gMipsInstructions[i].args_fmt; *args_fmt; ++args_fmt) {
        switch (*args_fmt) {
          case 'A':  // sa (shift amount or [d]ins/[d]ext position).
            args << sa;
            break;
          case 'B':  // Branch offset.
            {
              int32_t offset = static_cast<int16_t>(instruction & 0xffff);
              offset <<= 2;
              offset += 4;  // Delay slot.
              args << FormatInstructionPointer(instr_ptr + offset)
                   << StringPrintf("  ; %+d", offset);
            }
            break;
          case 'b':  // 21-bit branch offset.
            {
              int32_t offset = (instruction & 0x1fffff) - ((instruction & 0x100000) << 1);
              offset <<= 2;
              offset += 4;  // Delay slot.
              args << FormatInstructionPointer(instr_ptr + offset)
                   << StringPrintf("  ; %+d", offset);
            }
            break;
          case 'C':  // Floating-point condition code flag in c.<cond>.fmt.
            args << "cc" << (sa >> 2);
            break;
          case 'c':  // Floating-point condition code flag in bc1f/bc1t and movf/movt.
            args << "cc" << (rt >> 2);
            break;
          case 'D': args << RegName(rd); break;
          case 'd': args << 'f' << rd; break;
          case 'a': args << 'f' << sa; break;
          case 'F': args << (sa + 32); break;  // dinsu position.
          case 'f':  // Floating point "fmt".
            {
              size_t fmt = (instruction >> 21) & 0x7;  // TODO: other fmts?
              switch (fmt) {
                case 0: opcode += ".s"; break;
                case 1: opcode += ".d"; break;
                case 4: opcode += ".w"; break;
                case 5: opcode += ".l"; break;
                case 6: opcode += ".ps"; break;
                default: opcode += ".?"; break;
              }
              continue;  // No ", ".
            }
          case 'I':  // Unsigned lower 16-bit immediate.
            args << (instruction & 0xffff);
            break;
          case 'i':  // Sign-extended lower 16-bit immediate.
            args << static_cast<int16_t>(instruction & 0xffff);
            break;
          case 'J':  // sz (dinsm size).
            args << (rd - sa + 33);
            break;
          case 'j':  // sa value for lsa/dlsa.
            args << (sa + 1);
            break;
          case 'L':  // Jump label.
            {
              // TODO: is this right?
              uint32_t instr_index = (instruction & 0x1ffffff);
              uint32_t target = (instr_index << 2);
              target |= (reinterpret_cast<uintptr_t>(instr_ptr + 4) & 0xf0000000);
              args << reinterpret_cast<void*>(target);
            }
            break;
          case 'l':  // 9-bit signed offset
            {
              int32_t offset = static_cast<int16_t>(instruction) >> 7;
              args << StringPrintf("%+d(%s)", offset, RegName(rs));
            }
            break;
          case 'O':  // +x(rs)
            {
              int32_t offset = static_cast<int16_t>(instruction & 0xffff);
              args << StringPrintf("%+d(%s)", offset, RegName(rs));
              if (rs == 17) {
                args << "  ; ";
                GetDisassemblerOptions()->thread_offset_name_function_(args, offset);
              }
            }
            break;
          case 'o':  // 19-bit offset in lwpc and lwupc.
            {
              int32_t offset = (instruction & 0x7ffff) - ((instruction & 0x40000) << 1);
              offset <<= 2;
              args << FormatInstructionPointer(instr_ptr + offset);
              args << StringPrintf("  ; %+d", offset);
            }
            break;
          case '0':  // 18-bit offset in ldpc.
            {
              int32_t offset = (instruction & 0x3ffff) - ((instruction & 0x20000) << 1);
              offset <<= 3;
              uintptr_t ptr = RoundDown(reinterpret_cast<uintptr_t>(instr_ptr), 8);
              args << FormatInstructionPointer(reinterpret_cast<const uint8_t*>(ptr + offset));
              args << StringPrintf("  ; %+d", offset);
            }
            break;
          case 'P':  // 26-bit offset in bc and balc.
            {
              int32_t offset = (instruction & 0x3ffffff) - ((instruction & 0x2000000) << 1);
              offset <<= 2;
              offset += 4;
              args << FormatInstructionPointer(instr_ptr + offset);
              args << StringPrintf("  ; %+d", offset);
            }
            break;
          case 'p':  // 19-bit offset in addiupc.
            {
              int32_t offset = (instruction & 0x7ffff) - ((instruction & 0x40000) << 1);
              args << offset << "  ; move " << RegName(rs) << ", ";
              args << FormatInstructionPointer(instr_ptr + (offset << 2));
            }
            break;
          case 'S': args << RegName(rs); break;
          case 's': args << 'f' << rs; break;
          case 'T': args << RegName(rt); break;
          case 't': args << 'f' << rt; break;
          case 'Z': args << (rd + 1); break;  // sz ([d]ext size).
          case 'z': args << (rd - sa + 1); break;  // sz ([d]ins, dinsu size).
          case 'k': args << 'w' << sa; break;
          case 'm': args << 'w' << rd; break;
          case 'n': args << 'w' << rt; break;
          case 'U':  // MSA 1-bit df (word/doubleword), position 21.
            {
              int32_t df = (instruction >> 21) & 0x1;
              switch (df) {
                case 0: opcode += ".w"; break;
                case 1: opcode += ".d"; break;
              }
              continue;  // No ", ".
            }
          case 'u':  // MSA 1-bit df (word/doubleword), position 16.
            {
              int32_t df = (instruction >> 16) & 0x1;
              switch (df) {
                case 0: opcode += ".w"; break;
                case 1: opcode += ".d"; break;
              }
              continue;  // No ", ".
            }
          case 'V':  // MSA 2-bit df, position 21.
            {
              int32_t df = (instruction >> 21) & 0x3;
              switch (df) {
                case 0: opcode += ".b"; break;
                case 1: opcode += ".h"; break;
                case 2: opcode += ".w"; break;
                case 3: opcode += ".d"; break;
              }
              continue;  // No ", ".
            }
          case 'v':  // MSA 2-bit df, position 16.
            {
              int32_t df = (instruction >> 16) & 0x3;
              switch (df) {
                case 0: opcode += ".b"; break;
                case 1: opcode += ".h"; break;
                case 2: opcode += ".w"; break;
                case 3: opcode += ".d"; break;
              }
              continue;  // No ", ".
            }
          case 'W':  // MSA df/m.
            {
              int32_t df_m = (instruction >> 16) & 0x7f;
              if ((df_m & (0x1 << 6)) == 0) {
                opcode += ".d";
                args << (df_m & 0x3f);
                break;
              }
              if ((df_m & (0x1 << 5)) == 0) {
                opcode += ".w";
                args << (df_m & 0x1f);
                break;
              }
              if ((df_m & (0x1 << 4)) == 0) {
                opcode += ".h";
                args << (df_m & 0xf);
                break;
              }
              if ((df_m & (0x1 << 3)) == 0) {
                opcode += ".b";
                args << (df_m & 0x7);
              }
              break;
            }
          case 'w':  // MSA +x(rs).
            {
              int32_t df = instruction & 0x3;
              int32_t s10 = (instruction >> 16) & 0x3ff;
              s10 -= (s10 & 0x200) << 1;  // Sign-extend s10.
              switch (df) {
                case 0: opcode += ".b"; break;
                case 1: opcode += ".h"; break;
                case 2: opcode += ".w"; break;
                case 3: opcode += ".d"; break;
              }
              args << StringPrintf("%+d(%s)", s10 << df, RegName(rd));
              break;
            }
          case 'X':  // MSA df/n - ws[x].
            {
              int32_t df_n = (instruction >> 16) & 0x3f;
              if ((df_n & (0x3 << 4)) == 0) {
                opcode += ".b";
                args << 'w' << rd << '[' << (df_n & 0xf) << ']';
                break;
              }
              if ((df_n & (0x3 << 3)) == 0) {
                opcode += ".h";
                args << 'w' << rd << '[' << (df_n & 0x7) << ']';
                break;
              }
              if ((df_n & (0x3 << 2)) == 0) {
                opcode += ".w";
                args << 'w' << rd << '[' << (df_n & 0x3) << ']';
                break;
              }
              if ((df_n & (0x3 << 1)) == 0) {
                opcode += ".d";
                args << 'w' << rd << '[' << (df_n & 0x1) << ']';
              }
              break;
            }
          case 'x':  // MSA i10.
            {
              int32_t df = (instruction >> 21) & 0x3;
              int32_t i10 = (instruction >> 11) & 0x3ff;
              i10 -= (i10 & 0x200) << 1;  // Sign-extend i10.
              switch (df) {
                case 0: opcode += ".b"; break;
                case 1: opcode += ".h"; break;
                case 2: opcode += ".w"; break;
                case 3: opcode += ".d"; break;
              }
              args << i10;
              break;
            }
          case 'Y':  // MSA df/n - wd[x].
            {
              int32_t df_n = (instruction >> 16) & 0x3f;
              if ((df_n & (0x3 << 4)) == 0) {
                opcode += ".b";
                args << 'w' << sa << '[' << (df_n & 0xf) << ']';
                break;
              }
              if ((df_n & (0x3 << 3)) == 0) {
                opcode += ".h";
                args << 'w' << sa << '[' << (df_n & 0x7) << ']';
                break;
              }
              if ((df_n & (0x3 << 2)) == 0) {
                opcode += ".w";
                args << 'w' << sa << '[' << (df_n & 0x3) << ']';
                break;
              }
              if ((df_n & (0x3 << 1)) == 0) {
                opcode += ".d";
                args << 'w' << sa << '[' << (df_n & 0x1) << ']';
              }
              break;
            }
          case 'y': args << RegName(sa); break;
        }
        if (*(args_fmt + 1)) {
          args << ", ";
        }
      }
      break;
    }
  }

  // Special cases for sequences of:
  //   pc-relative +/- 2GB branch:
  //     auipc  reg, imm
  //     jic    reg, imm
  //   pc-relative +/- 2GB branch and link:
  //     auipc  reg, imm
  //     jialc  reg, imm
  if (((op == 0x36 || op == 0x3E) && rs == 0 && rt != 0) &&  // ji[al]c
      last_ptr_ && (intptr_t)instr_ptr - (intptr_t)last_ptr_ == 4 &&
      (last_instr_ & 0xFC1F0000) == 0xEC1E0000 &&  // auipc
      ((last_instr_ >> 21) & 0x1F) == rt) {
    uint32_t offset = (last_instr_ << 16) | (instruction & 0xFFFF);
    offset -= (offset & 0x8000) << 1;
    offset -= 4;
    if (op == 0x36) {
      args << "  ; bc ";
    } else {
      args << "  ; balc ";
    }
    args << FormatInstructionPointer(instr_ptr + (int32_t)offset);
    args << StringPrintf("  ; %+d", (int32_t)offset);
  }

  os << FormatInstructionPointer(instr_ptr)
     << StringPrintf(": %08x\t%-7s ", instruction, opcode.c_str())
     << args.str() << '\n';
  last_ptr_ = instr_ptr;
  last_instr_ = instruction;
  return 4;
}

void DisassemblerMips::Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) {
  for (const uint8_t* cur = begin; cur < end; cur += 4) {
    Dump(os, cur);
  }
}

}  // namespace mips
}  // namespace art
