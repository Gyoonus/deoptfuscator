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

#include "call_site.h"

#include "class-inl.h"
#include "gc_root-inl.h"

namespace art {
namespace mirror {

GcRoot<mirror::Class> CallSite::static_class_;

mirror::CallSite* CallSite::Create(Thread* const self, Handle<MethodHandle> target) {
  StackHandleScope<1> hs(self);
  Handle<mirror::CallSite> cs(
      hs.NewHandle(ObjPtr<CallSite>::DownCast(StaticClass()->AllocObject(self))));
  CHECK(!Runtime::Current()->IsActiveTransaction());
  cs->SetFieldObject<false>(TargetOffset(), target.Get());
  return cs.Get();
}

void CallSite::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void CallSite::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void CallSite::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

}  // namespace mirror
}  // namespace art
