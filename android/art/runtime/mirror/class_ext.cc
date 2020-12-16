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

#include "class_ext-inl.h"

#include "art_method-inl.h"
#include "base/casts.h"
#include "base/enums.h"
#include "base/utils.h"
#include "class-inl.h"
#include "dex/dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "object-inl.h"
#include "object_array.h"
#include "stack_trace_element.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

GcRoot<Class> ClassExt::dalvik_system_ClassExt_;

uint32_t ClassExt::ClassSize(PointerSize pointer_size) {
  uint32_t vtable_entries = Object::kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

void ClassExt::SetObsoleteArrays(ObjPtr<PointerArray> methods,
                                 ObjPtr<ObjectArray<DexCache>> dex_caches) {
  CHECK_EQ(methods.IsNull(), dex_caches.IsNull());
  auto obsolete_dex_cache_off = OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_dex_caches_);
  auto obsolete_methods_off = OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_methods_);
  DCHECK(!Runtime::Current()->IsActiveTransaction());
  SetFieldObject<false>(obsolete_dex_cache_off, dex_caches.Ptr());
  SetFieldObject<false>(obsolete_methods_off, methods.Ptr());
}

// We really need to be careful how we update this. If we ever in the future make it so that
// these arrays are written into without all threads being suspended we have a race condition! This
// race could cause obsolete methods to be missed.
bool ClassExt::ExtendObsoleteArrays(Thread* self, uint32_t increase) {
  // TODO It would be good to check that we have locked the class associated with this ClassExt.
  StackHandleScope<5> hs(self);
  Handle<ClassExt> h_this(hs.NewHandle(this));
  Handle<PointerArray> old_methods(hs.NewHandle(h_this->GetObsoleteMethods()));
  Handle<ObjectArray<DexCache>> old_dex_caches(hs.NewHandle(h_this->GetObsoleteDexCaches()));
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  size_t new_len;
  if (old_methods == nullptr) {
    CHECK(old_dex_caches == nullptr);
    new_len = increase;
  } else {
    CHECK_EQ(old_methods->GetLength(), old_dex_caches->GetLength());
    new_len = increase + old_methods->GetLength();
  }
  Handle<PointerArray> new_methods(hs.NewHandle<PointerArray>(
      cl->AllocPointerArray(self, new_len)));
  if (new_methods.IsNull()) {
    // Fail.
    self->AssertPendingOOMException();
    return false;
  }
  Handle<ObjectArray<DexCache>> new_dex_caches(hs.NewHandle<ObjectArray<DexCache>>(
      ObjectArray<DexCache>::Alloc(self,
                                   cl->FindClass(self,
                                                 "[Ljava/lang/DexCache;",
                                                 ScopedNullHandle<ClassLoader>()),
                                   new_len)));
  if (new_dex_caches.IsNull()) {
    // Fail.
    self->AssertPendingOOMException();
    return false;
  }

  if (!old_methods.IsNull()) {
    // Copy the old contents.
    new_methods->Memcpy(0,
                        old_methods.Get(),
                        0,
                        old_methods->GetLength(),
                        cl->GetImagePointerSize());
    new_dex_caches->AsObjectArray<Object>()->AssignableCheckingMemcpy<false>(
        0, old_dex_caches->AsObjectArray<Object>(), 0, old_dex_caches->GetLength(), false);
  }
  // Set the fields.
  h_this->SetObsoleteArrays(new_methods.Get(), new_dex_caches.Get());

  return true;
}

ClassExt* ClassExt::Alloc(Thread* self) {
  DCHECK(dalvik_system_ClassExt_.Read() != nullptr);
  return down_cast<ClassExt*>(dalvik_system_ClassExt_.Read()->AllocObject(self).Ptr());
}

void ClassExt::SetVerifyError(ObjPtr<Object> err) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetFieldObject<true>(OFFSET_OF_OBJECT_MEMBER(ClassExt, verify_error_), err);
  } else {
    SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(ClassExt, verify_error_), err);
  }
}

void ClassExt::SetOriginalDexFile(ObjPtr<Object> bytes) {
  DCHECK(!Runtime::Current()->IsActiveTransaction());
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(ClassExt, original_dex_file_), bytes);
}

void ClassExt::SetClass(ObjPtr<Class> dalvik_system_ClassExt) {
  CHECK(dalvik_system_ClassExt != nullptr);
  dalvik_system_ClassExt_ = GcRoot<Class>(dalvik_system_ClassExt);
}

void ClassExt::ResetClass() {
  CHECK(!dalvik_system_ClassExt_.IsNull());
  dalvik_system_ClassExt_ = GcRoot<Class>(nullptr);
}

void ClassExt::VisitRoots(RootVisitor* visitor) {
  dalvik_system_ClassExt_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

}  // namespace mirror
}  // namespace art
