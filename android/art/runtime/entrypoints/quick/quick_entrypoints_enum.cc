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

#include "quick_entrypoints_enum.h"

namespace art {

bool EntrypointRequiresStackMap(QuickEntrypointEnum trampoline) {
  // Entrypoints that do not require a stackmap. In general leaf methods
  // outside of the VM that are not safepoints.
  switch (trampoline) {
    // Listed in the same order as in quick_entrypoints_list.h.
    case kQuickCmpgDouble:
    case kQuickCmpgFloat:
    case kQuickCmplDouble:
    case kQuickCmplFloat:
    case kQuickCos:
    case kQuickSin:
    case kQuickAcos:
    case kQuickAsin:
    case kQuickAtan:
    case kQuickAtan2:
    case kQuickCbrt:
    case kQuickCosh:
    case kQuickExp:
    case kQuickExpm1:
    case kQuickHypot:
    case kQuickLog:
    case kQuickLog10:
    case kQuickNextAfter:
    case kQuickSinh:
    case kQuickTan:
    case kQuickTanh:
    case kQuickFmod:
    case kQuickL2d:
    case kQuickFmodf:
    case kQuickL2f:
    case kQuickD2iz:
    case kQuickF2iz:
    case kQuickIdivmod:
    case kQuickD2l:
    case kQuickF2l:
    case kQuickLdiv:
    case kQuickLmod:
    case kQuickLmul:
    case kQuickShlLong:
    case kQuickShrLong:
    case kQuickUshrLong:
      return false;

    /* Used by mips for 64bit volatile load/stores. */
    case kQuickA64Load:
    case kQuickA64Store:
      return false;

    default:
      return true;
  }
}

bool EntrypointCanTriggerGC(QuickEntrypointEnum entrypoint) {
  switch (entrypoint) {
    // Listed in the same order as in quick_entrypoints_list.h.
    case kQuickCmpgDouble:
    case kQuickCmpgFloat:
    case kQuickCmplDouble:
    case kQuickCmplFloat:
    case kQuickCos:
    case kQuickSin:
    case kQuickAcos:
    case kQuickAsin:
    case kQuickAtan:
    case kQuickAtan2:
    case kQuickCbrt:
    case kQuickCosh:
    case kQuickExp:
    case kQuickExpm1:
    case kQuickHypot:
    case kQuickLog:
    case kQuickLog10:
    case kQuickNextAfter:
    case kQuickSinh:
    case kQuickTan:
    case kQuickTanh:
    case kQuickFmod:
    case kQuickL2d:
    case kQuickFmodf:
    case kQuickL2f:
    case kQuickD2iz:
    case kQuickF2iz:
    case kQuickIdivmod:
    case kQuickD2l:
    case kQuickF2l:
    case kQuickLdiv:
    case kQuickLmod:
    case kQuickLmul:
    case kQuickShlLong:
    case kQuickShrLong:
    case kQuickUshrLong:
      return false;

    /* Used by mips for 64bit volatile load/stores. */
    case kQuickA64Load:
    case kQuickA64Store:
      return false;

    default:
      return true;
  }
}

}   // namespace art
