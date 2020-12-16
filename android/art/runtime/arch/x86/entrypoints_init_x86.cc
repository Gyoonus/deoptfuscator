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

#include <math.h>

#include "entrypoints/jni/jni_entrypoints.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/quick/quick_default_externs.h"
#include "entrypoints/quick/quick_default_init_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "interpreter/interpreter.h"

namespace art {

// Cast entrypoints.
extern "C" size_t art_quick_instance_of(mirror::Object* obj, mirror::Class* ref_class);

// Read barrier entrypoints.
// art_quick_read_barrier_mark_regX uses an non-standard calling
// convention: it expects its input in register X and returns its
// result in that same register, and saves and restores all
// caller-save registers.
extern "C" mirror::Object* art_quick_read_barrier_mark_reg00(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg01(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg02(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg03(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg05(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg06(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg07(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_slow(mirror::Object*, mirror::Object*, uint32_t);
extern "C" mirror::Object* art_quick_read_barrier_for_root_slow(GcRoot<mirror::Object>*);

void UpdateReadBarrierEntrypoints(QuickEntryPoints* qpoints, bool is_active) {
  qpoints->pReadBarrierMarkReg00 = is_active ? art_quick_read_barrier_mark_reg00 : nullptr;
  qpoints->pReadBarrierMarkReg01 = is_active ? art_quick_read_barrier_mark_reg01 : nullptr;
  qpoints->pReadBarrierMarkReg02 = is_active ? art_quick_read_barrier_mark_reg02 : nullptr;
  qpoints->pReadBarrierMarkReg03 = is_active ? art_quick_read_barrier_mark_reg03 : nullptr;
  qpoints->pReadBarrierMarkReg05 = is_active ? art_quick_read_barrier_mark_reg05 : nullptr;
  qpoints->pReadBarrierMarkReg06 = is_active ? art_quick_read_barrier_mark_reg06 : nullptr;
  qpoints->pReadBarrierMarkReg07 = is_active ? art_quick_read_barrier_mark_reg07 : nullptr;
}

void InitEntryPoints(JniEntryPoints* jpoints, QuickEntryPoints* qpoints) {
  DefaultInitEntryPoints(jpoints, qpoints);

  // Cast
  qpoints->pInstanceofNonTrivial = art_quick_instance_of;
  qpoints->pCheckInstanceOf = art_quick_check_instance_of;

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

  // Math
  qpoints->pD2l = art_quick_d2l;
  qpoints->pF2l = art_quick_f2l;
  qpoints->pLdiv = art_quick_ldiv;
  qpoints->pLmod = art_quick_lmod;
  qpoints->pLmul = art_quick_lmul;
  qpoints->pShlLong = art_quick_lshl;
  qpoints->pShrLong = art_quick_lshr;
  qpoints->pUshrLong = art_quick_lushr;

  // Intrinsics
  // qpoints->pIndexOf = nullptr;  // Not needed on x86
  qpoints->pStringCompareTo = art_quick_string_compareto;
  qpoints->pMemcpy = art_quick_memcpy;

  // Read barrier.
  qpoints->pReadBarrierJni = ReadBarrierJni;
  UpdateReadBarrierEntrypoints(qpoints, /*is_active*/ false);
  qpoints->pReadBarrierMarkReg04 = nullptr;  // Cannot use register 4 (ESP) to pass arguments.
  // x86 has only 8 core registers.
  qpoints->pReadBarrierMarkReg08 = nullptr;
  qpoints->pReadBarrierMarkReg09 = nullptr;
  qpoints->pReadBarrierMarkReg10 = nullptr;
  qpoints->pReadBarrierMarkReg11 = nullptr;
  qpoints->pReadBarrierMarkReg12 = nullptr;
  qpoints->pReadBarrierMarkReg13 = nullptr;
  qpoints->pReadBarrierMarkReg14 = nullptr;
  qpoints->pReadBarrierMarkReg15 = nullptr;
  qpoints->pReadBarrierMarkReg16 = nullptr;
  qpoints->pReadBarrierMarkReg17 = nullptr;
  qpoints->pReadBarrierMarkReg18 = nullptr;
  qpoints->pReadBarrierMarkReg19 = nullptr;
  qpoints->pReadBarrierMarkReg20 = nullptr;
  qpoints->pReadBarrierMarkReg21 = nullptr;
  qpoints->pReadBarrierMarkReg22 = nullptr;
  qpoints->pReadBarrierMarkReg23 = nullptr;
  qpoints->pReadBarrierMarkReg24 = nullptr;
  qpoints->pReadBarrierMarkReg25 = nullptr;
  qpoints->pReadBarrierMarkReg26 = nullptr;
  qpoints->pReadBarrierMarkReg27 = nullptr;
  qpoints->pReadBarrierMarkReg28 = nullptr;
  qpoints->pReadBarrierMarkReg29 = nullptr;
  qpoints->pReadBarrierSlow = art_quick_read_barrier_slow;
  qpoints->pReadBarrierForRootSlow = art_quick_read_barrier_for_root_slow;
}

}  // namespace art
