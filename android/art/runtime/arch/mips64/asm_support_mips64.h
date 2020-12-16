/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_

#include "asm_support.h"

// 64 ($f24-$f31) + 64 ($s0-$s7) + 8 ($gp) + 8 ($s8) + 8 ($ra) + 1x8 bytes padding
#define FRAME_SIZE_SAVE_ALL_CALLEE_SAVES 160
// 48 ($s2-$s7) + 8 ($gp) + 8 ($s8) + 8 ($ra) + 1x8 bytes padding
#define FRAME_SIZE_SAVE_REFS_ONLY 80
// $f12-$f19, $a1-$a7, $s2-$s7 + $gp + $s8 + $ra, 16 total + 1x8 bytes padding + method*
#define FRAME_SIZE_SAVE_REFS_AND_ARGS 208
// $f0-$f31, $at, $v0-$v1, $a0-$a7, $t0-$t3, $s0-$s7, $t8-$t9, $gp, $s8, $ra + padding + method*
#define FRAME_SIZE_SAVE_EVERYTHING 496
#define FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT FRAME_SIZE_SAVE_EVERYTHING
#define FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK FRAME_SIZE_SAVE_EVERYTHING

// &art_quick_read_barrier_mark_introspection is the first of many entry points:
//   20 entry points for long field offsets, large array indices and variable array indices
//     (see macro BRB_FIELD_LONG_OFFSET_ENTRY)
//   20 entry points for short field offsets and small array indices
//     (see macro BRB_FIELD_SHORT_OFFSET_ENTRY)
//   20 entry points for GC roots
//     (see macro BRB_GC_ROOT_ENTRY)

// There are as many entry points of each kind as there are registers that
// can hold a reference: V0-V1, A0-A7, T0-T2, S2-S8.
#define BAKER_MARK_INTROSPECTION_REGISTER_COUNT 20

#define BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE (8 * 4)  // 8 instructions in
                                                                 // BRB_FIELD_*_OFFSET_ENTRY.

#define BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET \
    (2 * BAKER_MARK_INTROSPECTION_REGISTER_COUNT * BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE)

#define BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE (4 * 4)  // 4 instructions in BRB_GC_ROOT_ENTRY.

#endif  // ART_RUNTIME_ARCH_MIPS64_ASM_SUPPORT_MIPS64_H_
