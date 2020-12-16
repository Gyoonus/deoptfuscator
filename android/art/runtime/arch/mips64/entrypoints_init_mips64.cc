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

#include <math.h>
#include <string.h>

#include "arch/mips64/asm_support_mips64.h"
#include "base/atomic.h"
#include "base/quasi_atomic.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/jni/jni_entrypoints.h"
#include "entrypoints/math_entrypoints.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/quick/quick_default_externs.h"
#include "entrypoints/quick/quick_default_init_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "interpreter/interpreter.h"

namespace art {

// Cast entrypoints.
extern "C" size_t artInstanceOfFromCode(mirror::Object* obj, mirror::Class* ref_class);

// Read barrier entrypoints.
// art_quick_read_barrier_mark_regXX uses a non-standard calling
// convention: it expects its input in register XX+1 and returns its
// result in that same register, and saves and restores all
// caller-save registers.
extern "C" mirror::Object* art_quick_read_barrier_mark_reg01(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg02(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg03(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg04(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg05(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg06(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg07(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg08(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg09(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg10(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg11(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg12(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg13(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg17(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg18(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg19(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg20(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg21(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg22(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg29(mirror::Object*);

extern "C" mirror::Object* art_quick_read_barrier_mark_introspection(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_introspection_gc_roots(mirror::Object*);
extern "C" void art_quick_read_barrier_mark_introspection_end_of_entries(void);

// Math entrypoints.
extern int32_t CmpgDouble(double a, double b);
extern int32_t CmplDouble(double a, double b);
extern int32_t CmpgFloat(float a, float b);
extern int32_t CmplFloat(float a, float b);
extern "C" int64_t artLmul(int64_t a, int64_t b);
extern "C" int64_t artLdiv(int64_t a, int64_t b);
extern "C" int64_t artLmod(int64_t a, int64_t b);

// Math conversions.
extern "C" int32_t __fixsfsi(float op1);      // FLOAT_TO_INT
extern "C" int32_t __fixdfsi(double op1);     // DOUBLE_TO_INT
extern "C" float __floatdisf(int64_t op1);    // LONG_TO_FLOAT
extern "C" double __floatdidf(int64_t op1);   // LONG_TO_DOUBLE
extern "C" int64_t __fixsfdi(float op1);      // FLOAT_TO_LONG
extern "C" int64_t __fixdfdi(double op1);     // DOUBLE_TO_LONG

// Single-precision FP arithmetics.
extern "C" float fmodf(float a, float b);      // REM_FLOAT[_2ADDR]

// Double-precision FP arithmetics.
extern "C" double fmod(double a, double b);     // REM_DOUBLE[_2ADDR]

// Long long arithmetics - REM_LONG[_2ADDR] and DIV_LONG[_2ADDR]
extern "C" int64_t __divdi3(int64_t, int64_t);
extern "C" int64_t __moddi3(int64_t, int64_t);

// No read barrier entrypoints for marking registers.
void UpdateReadBarrierEntrypoints(QuickEntryPoints* qpoints, bool is_active) {
  intptr_t introspection_field_array_entries_size =
      reinterpret_cast<intptr_t>(&art_quick_read_barrier_mark_introspection_gc_roots) -
      reinterpret_cast<intptr_t>(&art_quick_read_barrier_mark_introspection);
  static_assert(
      BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET == 2 *
          BAKER_MARK_INTROSPECTION_REGISTER_COUNT * BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE,
      "Expecting equal");
  DCHECK_EQ(introspection_field_array_entries_size,
            BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET);
  intptr_t introspection_gc_root_entries_size =
      reinterpret_cast<intptr_t>(&art_quick_read_barrier_mark_introspection_end_of_entries) -
      reinterpret_cast<intptr_t>(&art_quick_read_barrier_mark_introspection_gc_roots);
  DCHECK_EQ(introspection_gc_root_entries_size,
            BAKER_MARK_INTROSPECTION_REGISTER_COUNT * BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE);
  qpoints->pReadBarrierMarkReg00 = is_active ? art_quick_read_barrier_mark_introspection : nullptr;
  qpoints->pReadBarrierMarkReg01 = is_active ? art_quick_read_barrier_mark_reg01 : nullptr;
  qpoints->pReadBarrierMarkReg02 = is_active ? art_quick_read_barrier_mark_reg02 : nullptr;
  qpoints->pReadBarrierMarkReg03 = is_active ? art_quick_read_barrier_mark_reg03 : nullptr;
  qpoints->pReadBarrierMarkReg04 = is_active ? art_quick_read_barrier_mark_reg04 : nullptr;
  qpoints->pReadBarrierMarkReg05 = is_active ? art_quick_read_barrier_mark_reg05 : nullptr;
  qpoints->pReadBarrierMarkReg06 = is_active ? art_quick_read_barrier_mark_reg06 : nullptr;
  qpoints->pReadBarrierMarkReg07 = is_active ? art_quick_read_barrier_mark_reg07 : nullptr;
  qpoints->pReadBarrierMarkReg08 = is_active ? art_quick_read_barrier_mark_reg08 : nullptr;
  qpoints->pReadBarrierMarkReg09 = is_active ? art_quick_read_barrier_mark_reg09 : nullptr;
  qpoints->pReadBarrierMarkReg10 = is_active ? art_quick_read_barrier_mark_reg10 : nullptr;
  qpoints->pReadBarrierMarkReg11 = is_active ? art_quick_read_barrier_mark_reg11 : nullptr;
  qpoints->pReadBarrierMarkReg12 = is_active ? art_quick_read_barrier_mark_reg12 : nullptr;
  qpoints->pReadBarrierMarkReg13 = is_active ? art_quick_read_barrier_mark_reg13 : nullptr;
  qpoints->pReadBarrierMarkReg17 = is_active ? art_quick_read_barrier_mark_reg17 : nullptr;
  qpoints->pReadBarrierMarkReg18 = is_active ? art_quick_read_barrier_mark_reg18 : nullptr;
  qpoints->pReadBarrierMarkReg19 = is_active ? art_quick_read_barrier_mark_reg19 : nullptr;
  qpoints->pReadBarrierMarkReg20 = is_active ? art_quick_read_barrier_mark_reg20 : nullptr;
  qpoints->pReadBarrierMarkReg21 = is_active ? art_quick_read_barrier_mark_reg21 : nullptr;
  qpoints->pReadBarrierMarkReg22 = is_active ? art_quick_read_barrier_mark_reg22 : nullptr;
  qpoints->pReadBarrierMarkReg29 = is_active ? art_quick_read_barrier_mark_reg29 : nullptr;
}

void InitEntryPoints(JniEntryPoints* jpoints, QuickEntryPoints* qpoints) {
  DefaultInitEntryPoints(jpoints, qpoints);

  // Cast
  qpoints->pInstanceofNonTrivial = artInstanceOfFromCode;
  qpoints->pCheckInstanceOf = art_quick_check_instance_of;

  // Math
  qpoints->pCmpgDouble = CmpgDouble;
  qpoints->pCmpgFloat = CmpgFloat;
  qpoints->pCmplDouble = CmplDouble;
  qpoints->pCmplFloat = CmplFloat;
  qpoints->pFmod = fmod;
  qpoints->pL2d = art_l2d;
  qpoints->pFmodf = fmodf;
  qpoints->pL2f = art_l2f;
  qpoints->pD2iz = art_d2i;
  qpoints->pF2iz = art_f2i;
  qpoints->pIdivmod = nullptr;
  qpoints->pD2l = art_d2l;
  qpoints->pF2l = art_f2l;
  qpoints->pLdiv = artLdiv;
  qpoints->pLmod = artLmod;
  qpoints->pLmul = artLmul;
  qpoints->pShlLong = nullptr;
  qpoints->pShrLong = nullptr;
  qpoints->pUshrLong = nullptr;

  // More math.
  qpoints->pCos = cos;
  qpoints->pSin = sin;
  qpoints->pAcos = acos;
  qpoints->pAsin = asin;
  qpoints->pAtan = atan;
  qpoints->pAtan2 = atan2;
  qpoints->pPow = pow;
  qpoints->pCbrt = cbrt;
  qpoints->pCosh = cosh;
  qpoints->pExp = exp;
  qpoints->pExpm1 = expm1;
  qpoints->pHypot = hypot;
  qpoints->pLog = log;
  qpoints->pLog10 = log10;
  qpoints->pNextAfter = nextafter;
  qpoints->pSinh = sinh;
  qpoints->pTan = tan;
  qpoints->pTanh = tanh;

  // Intrinsics
  qpoints->pIndexOf = art_quick_indexof;
  qpoints->pStringCompareTo = art_quick_string_compareto;
  qpoints->pMemcpy = memcpy;

  // TODO - use lld/scd instructions for Mips64
  // Atomic 64-bit load/store
  qpoints->pA64Load = QuasiAtomic::Read64;
  qpoints->pA64Store = QuasiAtomic::Write64;

  // Read barrier.
  qpoints->pReadBarrierJni = ReadBarrierJni;
  UpdateReadBarrierEntrypoints(qpoints, /*is_active*/ false);
  // Cannot use the following registers to pass arguments:
  // 0(ZERO), 1(AT), 15(T3), 16(S0), 17(S1), 24(T8), 25(T9), 26(K0), 27(K1), 28(GP), 29(SP), 31(RA).
  // Note that there are 30 entry points only: 00 for register 1(AT), ..., 29 for register 30(S8).
  qpoints->pReadBarrierMarkReg14 = nullptr;
  qpoints->pReadBarrierMarkReg15 = nullptr;
  qpoints->pReadBarrierMarkReg16 = nullptr;
  qpoints->pReadBarrierMarkReg23 = nullptr;
  qpoints->pReadBarrierMarkReg24 = nullptr;
  qpoints->pReadBarrierMarkReg25 = nullptr;
  qpoints->pReadBarrierMarkReg26 = nullptr;
  qpoints->pReadBarrierMarkReg27 = nullptr;
  qpoints->pReadBarrierMarkReg28 = nullptr;
  qpoints->pReadBarrierSlow = artReadBarrierSlow;
  qpoints->pReadBarrierForRootSlow = artReadBarrierForRootSlow;
}

}  // namespace art
