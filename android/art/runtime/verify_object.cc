/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "verify_object-inl.h"

#include "base/bit_utils.h"
#include "gc/heap.h"
#include "globals.h"
#include "mirror/object-inl.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {

void VerifyObjectImpl(ObjPtr<mirror::Object> obj) {
  if (kVerifyObjectSupport > kVerifyObjectModeFast) {
    // Slow object verification, try the heap right away.
    Runtime::Current()->GetHeap()->VerifyObjectBody(obj);
  } else {
    // Fast object verification, only call the heap if our quick sanity tests fail. The heap will
    // print the diagnostic message.
    bool failed = !IsAligned<kObjectAlignment>(obj.Ptr());
    if (!failed) {
      mirror::Class* c = obj->GetClass<kVerifyNone>();
      failed = failed || !IsAligned<kObjectAlignment>(c);
      failed = failed || !VerifyClassClass(c);
    }
    if (UNLIKELY(failed)) {
      Runtime::Current()->GetHeap()->VerifyObjectBody(obj);
    }
  }
}

}  // namespace art
