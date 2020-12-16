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

#include "arch/arm64/asm_support_arm64.h"
#include "base/bit_utils.h"
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
// art_quick_read_barrier_mark_regX uses an non-standard calling
// convention: it expects its input in register X and returns its
// result in that same register, and saves and restores all
// caller-save registers.
extern "C" mirror::Object* art_quick_read_barrier_mark_reg00(mirror::Object*);
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
extern "C" mirror::Object* art_quick_read_barrier_mark_reg12(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg13(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg14(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg15(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg16(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg17(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg18(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg19(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg20(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg21(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg22(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg22(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg23(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg24(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg25(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg26(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg27(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg28(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_reg29(mirror::Object*);

extern "C" mirror::Object* art_quick_read_barrier_mark_introspection(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_introspection_arrays(mirror::Object*);
extern "C" mirror::Object* art_quick_read_barrier_mark_introspection_gc_roots(mirror::Object*);

void UpdateReadBarrierEntrypoints(QuickEntryPoints* qpoints, bool is_active) {
  // ARM64 is the architecture with the largest number of core
  // registers (32) that supports the read barrier configuration.
  // Because registers 30 (LR) and 31 (SP/XZR) cannot be used to pass
  // arguments, only define ReadBarrierMarkRegX entrypoints for the
  // first 30 registers.  This limitation is not a problem on other
  // supported architectures (ARM, x86 and x86-64) either, as they
  // have less core registers (resp. 16, 8 and 16).  (We may have to
  // revise that design choice if read barrier support is added for
  // MIPS and/or MIPS64.)
  qpoints->pReadBarrierMarkReg00 = is_active ? art_quick_read_barrier_mark_reg00 : nullptr;
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
  qpoints->pReadBarrierMarkReg14 = is_active ? art_quick_read_barrier_mark_reg14 : nullptr;
  qpoints->pReadBarrierMarkReg15 = is_active ? art_quick_read_barrier_mark_reg15 : nullptr;
  qpoints->pReadBarrierMarkReg17 = is_active ? art_quick_read_barrier_mark_reg17 : nullptr;
  qpoints->pReadBarrierMarkReg18 = is_active ? art_quick_read_barrier_mark_reg18 : nullptr;
  qpoints->pReadBarrierMarkReg19 = is_active ? art_quick_read_barrier_mark_reg19 : nullptr;
  qpoints->pReadBarrierMarkReg20 = is_active ? art_quick_read_barrier_mark_reg20 : nullptr;
  qpoints->pReadBarrierMarkReg21 = is_active ? art_quick_read_barrier_mark_reg21 : nullptr;
  qpoints->pReadBarrierMarkReg22 = is_active ? art_quick_read_barrier_mark_reg22 : nullptr;
  qpoints->pReadBarrierMarkReg23 = is_active ? art_quick_read_barrier_mark_reg23 : nullptr;
  qpoints->pReadBarrierMarkReg24 = is_active ? art_quick_read_barrier_mark_reg24 : nullptr;
  qpoints->pReadBarrierMarkReg25 = is_active ? art_quick_read_barrier_mark_reg25 : nullptr;
  qpoints->pReadBarrierMarkReg26 = is_active ? art_quick_read_barrier_mark_reg26 : nullptr;
  qpoints->pReadBarrierMarkReg27 = is_active ? art_quick_read_barrier_mark_reg27 : nullptr;
  qpoints->pReadBarrierMarkReg28 = is_active ? art_quick_read_barrier_mark_reg28 : nullptr;
  qpoints->pReadBarrierMarkReg29 = is_active ? art_quick_read_barrier_mark_reg29 : nullptr;

  // Check that array switch cases are at appropriate offsets from the introspection entrypoint.
  DCHECK_ALIGNED(art_quick_read_barrier_mark_introspection, 512u);
  intptr_t array_diff =
      reinterpret_cast<intptr_t>(art_quick_read_barrier_mark_introspection_arrays) -
      reinterpret_cast<intptr_t>(art_quick_read_barrier_mark_introspection);
  DCHECK_EQ(BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET, array_diff);
  // Check that the GC root entrypoint is at appropriate offset from the introspection entrypoint.
  intptr_t gc_roots_diff =
      reinterpret_cast<intptr_t>(art_quick_read_barrier_mark_introspection_gc_roots) -
      reinterpret_cast<intptr_t>(art_quick_read_barrier_mark_introspection);
  DCHECK_EQ(BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET, gc_roots_diff);
  // The register 16, i.e. IP0, is reserved, so there is no art_quick_read_barrier_mark_reg16.
  // We're using the entry to hold a pointer to the introspection entrypoint instead.
  qpoints->pReadBarrierMarkReg16 = is_active ? art_quick_read_barrier_mark_introspection : nullptr;
}

void InitEntryPoints(JniEntryPoints* jpoints, QuickEntryPoints* qpoints) {
  DefaultInitEntryPoints(jpoints, qpoints);

  // Cast
  qpoints->pInstanceofNonTrivial = artInstanceOfFromCode;
  qpoints->pCheckInstanceOf = art_quick_check_instance_of;

  // Math
  // TODO null entrypoints not needed for ARM64 - generate inline.
  qpoints->pCmpgDouble = nullptr;
  qpoints->pCmpgFloat = nullptr;
  qpoints->pCmplDouble = nullptr;
  qpoints->pCmplFloat = nullptr;
  qpoints->pFmod = fmod;
  qpoints->pL2d = nullptr;
  qpoints->pFmodf = fmodf;
  qpoints->pL2f = nullptr;
  qpoints->pD2iz = nullptr;
  qpoints->pF2iz = nullptr;
  qpoints->pIdivmod = nullptr;
  qpoints->pD2l = nullptr;
  qpoints->pF2l = nullptr;
  qpoints->pLdiv = nullptr;
  qpoints->pLmod = nullptr;
  qpoints->pLmul = nullptr;
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
  // The ARM64 StringCompareTo intrinsic does not call the runtime.
  qpoints->pStringCompareTo = nullptr;
  qpoints->pMemcpy = memcpy;

  // Read barrier.
  qpoints->pReadBarrierJni = ReadBarrierJni;
  qpoints->pReadBarrierMarkReg16 = nullptr;  // IP0 is used as a temp by the asm stub.
  UpdateReadBarrierEntrypoints(qpoints, /*is_active*/ false);
  qpoints->pReadBarrierSlow = artReadBarrierSlow;
  qpoints->pReadBarrierForRootSlow = artReadBarrierForRootSlow;
}

}  // namespace art
