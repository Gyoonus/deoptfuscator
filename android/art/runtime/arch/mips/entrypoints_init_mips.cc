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

#include <string.h>

#include "arch/mips/asm_support_mips.h"
#include "base/atomic.h"
#include "base/logging.h"
#include "base/quasi_atomic.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/jni/jni_entrypoints.h"
#include "entrypoints/math_entrypoints.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/quick/quick_default_externs.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "entrypoints_direct_mips.h"
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
extern "C" mirror::Object* art_quick_read_barrier_mark_reg14(mirror::Object*);
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
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg00),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg01 = is_active ? art_quick_read_barrier_mark_reg01 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg01),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg02 = is_active ? art_quick_read_barrier_mark_reg02 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg02),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg03 = is_active ? art_quick_read_barrier_mark_reg03 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg03),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg04 = is_active ? art_quick_read_barrier_mark_reg04 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg04),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg05 = is_active ? art_quick_read_barrier_mark_reg05 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg05),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg06 = is_active ? art_quick_read_barrier_mark_reg06 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg06),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg07 = is_active ? art_quick_read_barrier_mark_reg07 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg07),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg08 = is_active ? art_quick_read_barrier_mark_reg08 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg08),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg09 = is_active ? art_quick_read_barrier_mark_reg09 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg09),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg10 = is_active ? art_quick_read_barrier_mark_reg10 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg10),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg11 = is_active ? art_quick_read_barrier_mark_reg11 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg11),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg12 = is_active ? art_quick_read_barrier_mark_reg12 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg12),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg13 = is_active ? art_quick_read_barrier_mark_reg13 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg13),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg14 = is_active ? art_quick_read_barrier_mark_reg14 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg14),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg17 = is_active ? art_quick_read_barrier_mark_reg17 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg17),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg18 = is_active ? art_quick_read_barrier_mark_reg18 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg18),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg19 = is_active ? art_quick_read_barrier_mark_reg19 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg19),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg20 = is_active ? art_quick_read_barrier_mark_reg20 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg20),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg21 = is_active ? art_quick_read_barrier_mark_reg21 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg21),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg22 = is_active ? art_quick_read_barrier_mark_reg22 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg22),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg29 = is_active ? art_quick_read_barrier_mark_reg29 : nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg29),
                "Non-direct C stub marked direct.");
}

void InitEntryPoints(JniEntryPoints* jpoints, QuickEntryPoints* qpoints) {
  // Note: MIPS has asserts checking for the type of entrypoint. Don't move it
  //       to InitDefaultEntryPoints().

  // JNI
  jpoints->pDlsymLookup = art_jni_dlsym_lookup_stub;

  // Alloc
  ResetQuickAllocEntryPoints(qpoints, /*is_active*/ false);

  // Cast
  qpoints->pInstanceofNonTrivial = artInstanceOfFromCode;
  static_assert(IsDirectEntrypoint(kQuickInstanceofNonTrivial), "Direct C stub not marked direct.");
  qpoints->pCheckInstanceOf = art_quick_check_instance_of;
  static_assert(!IsDirectEntrypoint(kQuickCheckInstanceOf), "Non-direct C stub marked direct.");

  // DexCache
  qpoints->pInitializeStaticStorage = art_quick_initialize_static_storage;
  static_assert(!IsDirectEntrypoint(kQuickInitializeStaticStorage),
                "Non-direct C stub marked direct.");
  qpoints->pInitializeTypeAndVerifyAccess = art_quick_initialize_type_and_verify_access;
  static_assert(!IsDirectEntrypoint(kQuickInitializeTypeAndVerifyAccess),
                "Non-direct C stub marked direct.");
  qpoints->pInitializeType = art_quick_initialize_type;
  static_assert(!IsDirectEntrypoint(kQuickInitializeType), "Non-direct C stub marked direct.");
  qpoints->pResolveString = art_quick_resolve_string;
  static_assert(!IsDirectEntrypoint(kQuickResolveString), "Non-direct C stub marked direct.");

  // Field
  qpoints->pSet8Instance = art_quick_set8_instance;
  static_assert(!IsDirectEntrypoint(kQuickSet8Instance), "Non-direct C stub marked direct.");
  qpoints->pSet8Static = art_quick_set8_static;
  static_assert(!IsDirectEntrypoint(kQuickSet8Static), "Non-direct C stub marked direct.");
  qpoints->pSet16Instance = art_quick_set16_instance;
  static_assert(!IsDirectEntrypoint(kQuickSet16Instance), "Non-direct C stub marked direct.");
  qpoints->pSet16Static = art_quick_set16_static;
  static_assert(!IsDirectEntrypoint(kQuickSet16Static), "Non-direct C stub marked direct.");
  qpoints->pSet32Instance = art_quick_set32_instance;
  static_assert(!IsDirectEntrypoint(kQuickSet32Instance), "Non-direct C stub marked direct.");
  qpoints->pSet32Static = art_quick_set32_static;
  static_assert(!IsDirectEntrypoint(kQuickSet32Static), "Non-direct C stub marked direct.");
  qpoints->pSet64Instance = art_quick_set64_instance;
  static_assert(!IsDirectEntrypoint(kQuickSet64Instance), "Non-direct C stub marked direct.");
  qpoints->pSet64Static = art_quick_set64_static;
  static_assert(!IsDirectEntrypoint(kQuickSet64Static), "Non-direct C stub marked direct.");
  qpoints->pSetObjInstance = art_quick_set_obj_instance;
  static_assert(!IsDirectEntrypoint(kQuickSetObjInstance), "Non-direct C stub marked direct.");
  qpoints->pSetObjStatic = art_quick_set_obj_static;
  static_assert(!IsDirectEntrypoint(kQuickSetObjStatic), "Non-direct C stub marked direct.");
  qpoints->pGetBooleanInstance = art_quick_get_boolean_instance;
  static_assert(!IsDirectEntrypoint(kQuickGetBooleanInstance), "Non-direct C stub marked direct.");
  qpoints->pGetByteInstance = art_quick_get_byte_instance;
  static_assert(!IsDirectEntrypoint(kQuickGetByteInstance), "Non-direct C stub marked direct.");
  qpoints->pGetCharInstance = art_quick_get_char_instance;
  static_assert(!IsDirectEntrypoint(kQuickGetCharInstance), "Non-direct C stub marked direct.");
  qpoints->pGetShortInstance = art_quick_get_short_instance;
  static_assert(!IsDirectEntrypoint(kQuickGetShortInstance), "Non-direct C stub marked direct.");
  qpoints->pGet32Instance = art_quick_get32_instance;
  static_assert(!IsDirectEntrypoint(kQuickGet32Instance), "Non-direct C stub marked direct.");
  qpoints->pGet64Instance = art_quick_get64_instance;
  static_assert(!IsDirectEntrypoint(kQuickGet64Instance), "Non-direct C stub marked direct.");
  qpoints->pGetObjInstance = art_quick_get_obj_instance;
  static_assert(!IsDirectEntrypoint(kQuickGetObjInstance), "Non-direct C stub marked direct.");
  qpoints->pGetBooleanStatic = art_quick_get_boolean_static;
  static_assert(!IsDirectEntrypoint(kQuickGetBooleanStatic), "Non-direct C stub marked direct.");
  qpoints->pGetByteStatic = art_quick_get_byte_static;
  static_assert(!IsDirectEntrypoint(kQuickGetByteStatic), "Non-direct C stub marked direct.");
  qpoints->pGetCharStatic = art_quick_get_char_static;
  static_assert(!IsDirectEntrypoint(kQuickGetCharStatic), "Non-direct C stub marked direct.");
  qpoints->pGetShortStatic = art_quick_get_short_static;
  static_assert(!IsDirectEntrypoint(kQuickGetShortStatic), "Non-direct C stub marked direct.");
  qpoints->pGet32Static = art_quick_get32_static;
  static_assert(!IsDirectEntrypoint(kQuickGet32Static), "Non-direct C stub marked direct.");
  qpoints->pGet64Static = art_quick_get64_static;
  static_assert(!IsDirectEntrypoint(kQuickGet64Static), "Non-direct C stub marked direct.");
  qpoints->pGetObjStatic = art_quick_get_obj_static;
  static_assert(!IsDirectEntrypoint(kQuickGetObjStatic), "Non-direct C stub marked direct.");

  // Array
  qpoints->pAputObject = art_quick_aput_obj;
  static_assert(!IsDirectEntrypoint(kQuickAputObject), "Non-direct C stub marked direct.");

  // JNI
  qpoints->pJniMethodStart = JniMethodStart;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodStart), "Non-direct C stub marked direct.");
  qpoints->pJniMethodFastStart = JniMethodFastStart;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodFastStart), "Non-direct C stub marked direct.");
  qpoints->pJniMethodStartSynchronized = JniMethodStartSynchronized;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodStartSynchronized),
                "Non-direct C stub marked direct.");
  qpoints->pJniMethodEnd = JniMethodEnd;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodEnd), "Non-direct C stub marked direct.");
  qpoints->pJniMethodFastEnd = JniMethodFastEnd;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodFastEnd), "Non-direct C stub marked direct.");
  qpoints->pJniMethodEndSynchronized = JniMethodEndSynchronized;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodEndSynchronized),
                "Non-direct C stub marked direct.");
  qpoints->pJniMethodEndWithReference = JniMethodEndWithReference;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodEndWithReference),
                "Non-direct C stub marked direct.");
  qpoints->pJniMethodFastEndWithReference = JniMethodFastEndWithReference;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodFastEndWithReference),
                "Non-direct C stub marked direct.");
  qpoints->pJniMethodEndWithReferenceSynchronized = JniMethodEndWithReferenceSynchronized;
  static_assert(!IsDirectEntrypoint(kQuickJniMethodEndWithReferenceSynchronized),
                "Non-direct C stub marked direct.");
  qpoints->pQuickGenericJniTrampoline = art_quick_generic_jni_trampoline;
  static_assert(!IsDirectEntrypoint(kQuickQuickGenericJniTrampoline),
                "Non-direct C stub marked direct.");

  // Locks
  if (UNLIKELY(VLOG_IS_ON(systrace_lock_logging))) {
    qpoints->pLockObject = art_quick_lock_object_no_inline;
    qpoints->pUnlockObject = art_quick_unlock_object_no_inline;
  } else {
    qpoints->pLockObject = art_quick_lock_object;
    qpoints->pUnlockObject = art_quick_unlock_object;
  }
  static_assert(!IsDirectEntrypoint(kQuickLockObject), "Non-direct C stub marked direct.");
  static_assert(!IsDirectEntrypoint(kQuickUnlockObject), "Non-direct C stub marked direct.");

  // Math
  qpoints->pCmpgDouble = CmpgDouble;
  static_assert(IsDirectEntrypoint(kQuickCmpgDouble), "Direct C stub not marked direct.");
  qpoints->pCmpgFloat = CmpgFloat;
  static_assert(IsDirectEntrypoint(kQuickCmpgFloat), "Direct C stub not marked direct.");
  qpoints->pCmplDouble = CmplDouble;
  static_assert(IsDirectEntrypoint(kQuickCmplDouble), "Direct C stub not marked direct.");
  qpoints->pCmplFloat = CmplFloat;
  static_assert(IsDirectEntrypoint(kQuickCmplFloat), "Direct C stub not marked direct.");
  qpoints->pFmod = fmod;
  static_assert(IsDirectEntrypoint(kQuickFmod), "Direct C stub not marked direct.");
  qpoints->pL2d = art_l2d;
  static_assert(IsDirectEntrypoint(kQuickL2d), "Direct C stub not marked direct.");
  qpoints->pFmodf = fmodf;
  static_assert(IsDirectEntrypoint(kQuickFmodf), "Direct C stub not marked direct.");
  qpoints->pL2f = art_l2f;
  static_assert(IsDirectEntrypoint(kQuickL2f), "Direct C stub not marked direct.");
  qpoints->pD2iz = art_d2i;
  static_assert(IsDirectEntrypoint(kQuickD2iz), "Direct C stub not marked direct.");
  qpoints->pF2iz = art_f2i;
  static_assert(IsDirectEntrypoint(kQuickF2iz), "Direct C stub not marked direct.");
  qpoints->pIdivmod = nullptr;
  qpoints->pD2l = art_d2l;
  static_assert(IsDirectEntrypoint(kQuickD2l), "Direct C stub not marked direct.");
  qpoints->pF2l = art_f2l;
  static_assert(IsDirectEntrypoint(kQuickF2l), "Direct C stub not marked direct.");
  qpoints->pLdiv = artLdiv;
  static_assert(IsDirectEntrypoint(kQuickLdiv), "Direct C stub not marked direct.");
  qpoints->pLmod = artLmod;
  static_assert(IsDirectEntrypoint(kQuickLmod), "Direct C stub not marked direct.");
  qpoints->pLmul = artLmul;
  static_assert(IsDirectEntrypoint(kQuickLmul), "Direct C stub not marked direct.");
  qpoints->pShlLong = art_quick_shl_long;
  static_assert(!IsDirectEntrypoint(kQuickShlLong), "Non-direct C stub marked direct.");
  qpoints->pShrLong = art_quick_shr_long;
  static_assert(!IsDirectEntrypoint(kQuickShrLong), "Non-direct C stub marked direct.");
  qpoints->pUshrLong = art_quick_ushr_long;
  static_assert(!IsDirectEntrypoint(kQuickUshrLong), "Non-direct C stub marked direct.");

  // More math.
  qpoints->pCos = cos;
  static_assert(IsDirectEntrypoint(kQuickCos), "Direct C stub marked non-direct.");
  qpoints->pSin = sin;
  static_assert(IsDirectEntrypoint(kQuickSin), "Direct C stub marked non-direct.");
  qpoints->pAcos = acos;
  static_assert(IsDirectEntrypoint(kQuickAcos), "Direct C stub marked non-direct.");
  qpoints->pAsin = asin;
  static_assert(IsDirectEntrypoint(kQuickAsin), "Direct C stub marked non-direct.");
  qpoints->pAtan = atan;
  static_assert(IsDirectEntrypoint(kQuickAtan), "Direct C stub marked non-direct.");
  qpoints->pAtan2 = atan2;
  static_assert(IsDirectEntrypoint(kQuickAtan2), "Direct C stub marked non-direct.");
  qpoints->pPow = pow;
  static_assert(IsDirectEntrypoint(kQuickPow), "Direct C stub marked non-direct.");
  qpoints->pCbrt = cbrt;
  static_assert(IsDirectEntrypoint(kQuickCbrt), "Direct C stub marked non-direct.");
  qpoints->pCosh = cosh;
  static_assert(IsDirectEntrypoint(kQuickCosh), "Direct C stub marked non-direct.");
  qpoints->pExp = exp;
  static_assert(IsDirectEntrypoint(kQuickExp), "Direct C stub marked non-direct.");
  qpoints->pExpm1 = expm1;
  static_assert(IsDirectEntrypoint(kQuickExpm1), "Direct C stub marked non-direct.");
  qpoints->pHypot = hypot;
  static_assert(IsDirectEntrypoint(kQuickHypot), "Direct C stub marked non-direct.");
  qpoints->pLog = log;
  static_assert(IsDirectEntrypoint(kQuickLog), "Direct C stub marked non-direct.");
  qpoints->pLog10 = log10;
  static_assert(IsDirectEntrypoint(kQuickLog10), "Direct C stub marked non-direct.");
  qpoints->pNextAfter = nextafter;
  static_assert(IsDirectEntrypoint(kQuickNextAfter), "Direct C stub marked non-direct.");
  qpoints->pSinh = sinh;
  static_assert(IsDirectEntrypoint(kQuickSinh), "Direct C stub marked non-direct.");
  qpoints->pTan = tan;
  static_assert(IsDirectEntrypoint(kQuickTan), "Direct C stub marked non-direct.");
  qpoints->pTanh = tanh;
  static_assert(IsDirectEntrypoint(kQuickTanh), "Direct C stub marked non-direct.");

  // Intrinsics
  qpoints->pIndexOf = art_quick_indexof;
  static_assert(!IsDirectEntrypoint(kQuickIndexOf), "Non-direct C stub marked direct.");
  qpoints->pStringCompareTo = art_quick_string_compareto;
  static_assert(!IsDirectEntrypoint(kQuickStringCompareTo), "Non-direct C stub marked direct.");
  qpoints->pMemcpy = memcpy;

  // Invocation
  qpoints->pQuickImtConflictTrampoline = art_quick_imt_conflict_trampoline;
  qpoints->pQuickResolutionTrampoline = art_quick_resolution_trampoline;
  qpoints->pQuickToInterpreterBridge = art_quick_to_interpreter_bridge;
  qpoints->pInvokeDirectTrampolineWithAccessCheck =
      art_quick_invoke_direct_trampoline_with_access_check;
  static_assert(!IsDirectEntrypoint(kQuickInvokeDirectTrampolineWithAccessCheck),
                "Non-direct C stub marked direct.");
  qpoints->pInvokeInterfaceTrampolineWithAccessCheck =
      art_quick_invoke_interface_trampoline_with_access_check;
  static_assert(!IsDirectEntrypoint(kQuickInvokeInterfaceTrampolineWithAccessCheck),
                "Non-direct C stub marked direct.");
  qpoints->pInvokeStaticTrampolineWithAccessCheck =
      art_quick_invoke_static_trampoline_with_access_check;
  static_assert(!IsDirectEntrypoint(kQuickInvokeStaticTrampolineWithAccessCheck),
                "Non-direct C stub marked direct.");
  qpoints->pInvokeSuperTrampolineWithAccessCheck =
      art_quick_invoke_super_trampoline_with_access_check;
  static_assert(!IsDirectEntrypoint(kQuickInvokeSuperTrampolineWithAccessCheck),
                "Non-direct C stub marked direct.");
  qpoints->pInvokeVirtualTrampolineWithAccessCheck =
      art_quick_invoke_virtual_trampoline_with_access_check;
  static_assert(!IsDirectEntrypoint(kQuickInvokeVirtualTrampolineWithAccessCheck),
                "Non-direct C stub marked direct.");
  qpoints->pInvokePolymorphic = art_quick_invoke_polymorphic;

  // Thread
  qpoints->pTestSuspend = art_quick_test_suspend;
  static_assert(!IsDirectEntrypoint(kQuickTestSuspend), "Non-direct C stub marked direct.");

  // Throws
  qpoints->pDeliverException = art_quick_deliver_exception;
  static_assert(!IsDirectEntrypoint(kQuickDeliverException), "Non-direct C stub marked direct.");
  qpoints->pThrowArrayBounds = art_quick_throw_array_bounds;
  static_assert(!IsDirectEntrypoint(kQuickThrowArrayBounds), "Non-direct C stub marked direct.");
  qpoints->pThrowDivZero = art_quick_throw_div_zero;
  static_assert(!IsDirectEntrypoint(kQuickThrowDivZero), "Non-direct C stub marked direct.");
  qpoints->pThrowNullPointer = art_quick_throw_null_pointer_exception;
  static_assert(!IsDirectEntrypoint(kQuickThrowNullPointer), "Non-direct C stub marked direct.");
  qpoints->pThrowStackOverflow = art_quick_throw_stack_overflow;
  static_assert(!IsDirectEntrypoint(kQuickThrowStackOverflow), "Non-direct C stub marked direct.");
  qpoints->pThrowStringBounds = art_quick_throw_string_bounds;
  static_assert(!IsDirectEntrypoint(kQuickThrowStringBounds), "Non-direct C stub marked direct.");

  // Deoptimization from compiled code.
  qpoints->pDeoptimize = art_quick_deoptimize_from_compiled_code;
  static_assert(!IsDirectEntrypoint(kQuickDeoptimize), "Non-direct C stub marked direct.");

  // Atomic 64-bit load/store
  qpoints->pA64Load = QuasiAtomic::Read64;
  static_assert(IsDirectEntrypoint(kQuickA64Load), "Non-direct C stub marked direct.");
  qpoints->pA64Store = QuasiAtomic::Write64;
  static_assert(IsDirectEntrypoint(kQuickA64Store), "Non-direct C stub marked direct.");

  // Read barrier.
  qpoints->pReadBarrierJni = ReadBarrierJni;
  static_assert(IsDirectEntrypoint(kQuickReadBarrierJni), "Direct C stub not marked direct.");
  UpdateReadBarrierEntrypoints(qpoints, /*is_active*/ false);
  // Cannot use the following registers to pass arguments:
  // 0(ZERO), 1(AT), 16(S0), 17(S1), 24(T8), 25(T9), 26(K0), 27(K1), 28(GP), 29(SP), 31(RA).
  // Note that there are 30 entry points only: 00 for register 1(AT), ..., 29 for register 30(S8).
  qpoints->pReadBarrierMarkReg15 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg15),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg16 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg16),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg23 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg23),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg24 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg24),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg25 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg25),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg26 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg26),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg27 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg27),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierMarkReg28 = nullptr;
  static_assert(!IsDirectEntrypoint(kQuickReadBarrierMarkReg28),
                "Non-direct C stub marked direct.");
  qpoints->pReadBarrierSlow = artReadBarrierSlow;
  static_assert(IsDirectEntrypoint(kQuickReadBarrierSlow), "Direct C stub not marked direct.");
  qpoints->pReadBarrierForRootSlow = artReadBarrierForRootSlow;
  static_assert(IsDirectEntrypoint(kQuickReadBarrierForRootSlow),
                "Direct C stub not marked direct.");
}

}  // namespace art
