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

#include "method_handles_lookup.h"

#include "class-inl.h"
#include "dex/modifiers.h"
#include "gc_root-inl.h"
#include "handle_scope.h"
#include "jni_internal.h"
#include "mirror/method_handle_impl.h"
#include "object-inl.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

GcRoot<mirror::Class> MethodHandlesLookup::static_class_;

void MethodHandlesLookup::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void MethodHandlesLookup::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void MethodHandlesLookup::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

MethodHandlesLookup* MethodHandlesLookup::Create(Thread* const self, Handle<Class> lookup_class)
  REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
  static constexpr uint32_t kAllModes = kAccPublic | kAccPrivate | kAccProtected | kAccStatic;

  StackHandleScope<1> hs(self);
  Handle<MethodHandlesLookup> mhl(
      hs.NewHandle(ObjPtr<MethodHandlesLookup>::DownCast(StaticClass()->AllocObject(self))));
  mhl->SetFieldObject<false>(LookupClassOffset(), lookup_class.Get());
  mhl->SetField32<false>(AllowedModesOffset(), kAllModes);
  return mhl.Get();
}

MethodHandlesLookup* MethodHandlesLookup::GetDefault(Thread* const self) {
  ArtMethod* lookup = jni::DecodeArtMethod(WellKnownClasses::java_lang_invoke_MethodHandles_lookup);
  JValue result;
  lookup->Invoke(self, nullptr, 0, &result, "L");
  return down_cast<MethodHandlesLookup*>(result.GetL());
}

MethodHandle* MethodHandlesLookup::FindConstructor(Thread* const self,
                                                           Handle<Class> klass,
                                                           Handle<MethodType> method_type) {
  ArtMethod* findConstructor =
      jni::DecodeArtMethod(WellKnownClasses::java_lang_invoke_MethodHandles_Lookup_findConstructor);
  uint32_t args[] = {
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(klass.Get())),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(method_type.Get()))
  };
  JValue result;
  findConstructor->Invoke(self, args, sizeof(args), &result, "LLL");
  return down_cast<MethodHandle*>(result.GetL());
}

}  // namespace mirror
}  // namespace art
