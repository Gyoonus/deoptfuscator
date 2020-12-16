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

#include "non_debuggable_classes.h"

#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "obj_ptr-inl.h"
#include "thread-current-inl.h"

namespace art {

std::vector<jclass>  NonDebuggableClasses::non_debuggable_classes;

void NonDebuggableClasses::AddNonDebuggableClass(jclass klass) {
  Thread* self = Thread::Current();
  JNIEnvExt* env = self->GetJniEnv();
  ObjPtr<mirror::Class> mirror_klass(self->DecodeJObject(klass)->AsClass());
  for (jclass c : non_debuggable_classes) {
    if (self->DecodeJObject(c)->AsClass() == mirror_klass.Ptr()) {
      return;
    }
  }
  non_debuggable_classes.push_back(reinterpret_cast<jclass>(env->NewGlobalRef(klass)));
}

}  // namespace art
