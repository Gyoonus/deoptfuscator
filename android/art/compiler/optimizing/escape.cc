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

#include "escape.h"

#include "nodes.h"

namespace art {

void CalculateEscape(HInstruction* reference,
                     bool (*no_escape)(HInstruction*, HInstruction*),
                     /*out*/ bool* is_singleton,
                     /*out*/ bool* is_singleton_and_not_returned,
                     /*out*/ bool* is_singleton_and_not_deopt_visible) {
  // For references not allocated in the method, don't assume anything.
  if (!reference->IsNewInstance() && !reference->IsNewArray()) {
    *is_singleton = false;
    *is_singleton_and_not_returned = false;
    *is_singleton_and_not_deopt_visible = false;
    return;
  }
  // Assume the best until proven otherwise.
  *is_singleton = true;
  *is_singleton_and_not_returned = true;
  *is_singleton_and_not_deopt_visible = true;

  if (reference->IsNewInstance() && reference->AsNewInstance()->IsFinalizable()) {
    // Finalizable reference is treated as being returned in the end.
    *is_singleton_and_not_returned = false;
  }

  // Visit all uses to determine if this reference can escape into the heap,
  // a method call, an alias, etc.
  for (const HUseListNode<HInstruction*>& use : reference->GetUses()) {
    HInstruction* user = use.GetUser();
    if (no_escape != nullptr && (*no_escape)(reference, user)) {
      // Client supplied analysis says there is no escape.
      continue;
    } else if (user->IsBoundType() || user->IsNullCheck()) {
      // BoundType shouldn't normally be necessary for an allocation. Just be conservative
      // for the uncommon cases. Similarly, null checks are eventually eliminated for explicit
      // allocations, but if we see one before it is simplified, assume an alias.
      *is_singleton = false;
      *is_singleton_and_not_returned = false;
      *is_singleton_and_not_deopt_visible = false;
      return;
    } else if (user->IsPhi() ||
               user->IsSelect() ||
               (user->IsInvoke() && user->GetSideEffects().DoesAnyWrite()) ||
               (user->IsInstanceFieldSet() && (reference == user->InputAt(1))) ||
               (user->IsUnresolvedInstanceFieldSet() && (reference == user->InputAt(1))) ||
               (user->IsStaticFieldSet() && (reference == user->InputAt(1))) ||
               (user->IsUnresolvedStaticFieldSet() && (reference == user->InputAt(0))) ||
               (user->IsArraySet() && (reference == user->InputAt(2)))) {
      // The reference is merged to HPhi/HSelect, passed to a callee, or stored to heap.
      // Hence, the reference is no longer the only name that can refer to its value.
      *is_singleton = false;
      *is_singleton_and_not_returned = false;
      *is_singleton_and_not_deopt_visible = false;
      return;
    } else if ((user->IsUnresolvedInstanceFieldGet() && (reference == user->InputAt(0))) ||
               (user->IsUnresolvedInstanceFieldSet() && (reference == user->InputAt(0)))) {
      // The field is accessed in an unresolved way. We mark the object as a non-singleton.
      // Note that we could optimize this case and still perform some optimizations until
      // we hit the unresolved access, but the conservative assumption is the simplest.
      *is_singleton = false;
      *is_singleton_and_not_returned = false;
      *is_singleton_and_not_deopt_visible = false;
      return;
    } else if (user->IsReturn()) {
      *is_singleton_and_not_returned = false;
    }
  }

  // Look at the environment uses if it's for HDeoptimize. Other environment uses are fine,
  // as long as client optimizations that rely on this information are disabled for debuggable.
  for (const HUseListNode<HEnvironment*>& use : reference->GetEnvUses()) {
    HEnvironment* user = use.GetUser();
    if (user->GetHolder()->IsDeoptimize()) {
      *is_singleton_and_not_deopt_visible = false;
      break;
    }
  }
}

bool DoesNotEscape(HInstruction* reference, bool (*no_escape)(HInstruction*, HInstruction*)) {
  bool is_singleton = false;
  bool is_singleton_and_not_returned = false;
  bool is_singleton_and_not_deopt_visible = false;  // not relevant for escape
  CalculateEscape(reference,
                  no_escape,
                  &is_singleton,
                  &is_singleton_and_not_returned,
                  &is_singleton_and_not_deopt_visible);
  return is_singleton_and_not_returned;
}

}  // namespace art
