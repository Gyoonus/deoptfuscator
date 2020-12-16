/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <ctime>

#include "object.h"

#include "array-inl.h"
#include "art_field-inl.h"
#include "art_field.h"
#include "class-inl.h"
#include "class.h"
#include "class_linker-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "handle_scope-inl.h"
#include "iftable-inl.h"
#include "monitor.h"
#include "object-inl.h"
#include "object-refvisitor-inl.h"
#include "object_array-inl.h"
#include "runtime.h"
#include "throwable.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

Atomic<uint32_t> Object::hash_code_seed(987654321U + std::time(nullptr));

class CopyReferenceFieldsWithReadBarrierVisitor {
 public:
  explicit CopyReferenceFieldsWithReadBarrierVisitor(ObjPtr<Object> dest_obj)
      : dest_obj_(dest_obj) {}

  void operator()(ObjPtr<Object> obj, MemberOffset offset, bool /* is_static */) const
      ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    // GetFieldObject() contains a RB.
    ObjPtr<Object> ref = obj->GetFieldObject<Object>(offset);
    // No WB here as a large object space does not have a card table
    // coverage. Instead, cards will be marked separately.
    dest_obj_->SetFieldObjectWithoutWriteBarrier<false, false>(offset, ref);
  }

  void operator()(ObjPtr<mirror::Class> klass, mirror::Reference* ref) const
      ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    // Copy java.lang.ref.Reference.referent which isn't visited in
    // Object::VisitReferences().
    DCHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  // Unused since we don't copy class native roots.
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
      const {}
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

 private:
  ObjPtr<Object> const dest_obj_;
};

Object* Object::CopyObject(ObjPtr<mirror::Object> dest,
                           ObjPtr<mirror::Object> src,
                           size_t num_bytes) {
  // Copy instance data.  Don't assume memcpy copies by words (b/32012820).
  {
    const size_t offset = sizeof(Object);
    uint8_t* src_bytes = reinterpret_cast<uint8_t*>(src.Ptr()) + offset;
    uint8_t* dst_bytes = reinterpret_cast<uint8_t*>(dest.Ptr()) + offset;
    num_bytes -= offset;
    DCHECK_ALIGNED(src_bytes, sizeof(uintptr_t));
    DCHECK_ALIGNED(dst_bytes, sizeof(uintptr_t));
    // Use word sized copies to begin.
    while (num_bytes >= sizeof(uintptr_t)) {
      reinterpret_cast<Atomic<uintptr_t>*>(dst_bytes)->StoreRelaxed(
          reinterpret_cast<Atomic<uintptr_t>*>(src_bytes)->LoadRelaxed());
      src_bytes += sizeof(uintptr_t);
      dst_bytes += sizeof(uintptr_t);
      num_bytes -= sizeof(uintptr_t);
    }
    // Copy possible 32 bit word.
    if (sizeof(uintptr_t) != sizeof(uint32_t) && num_bytes >= sizeof(uint32_t)) {
      reinterpret_cast<Atomic<uint32_t>*>(dst_bytes)->StoreRelaxed(
          reinterpret_cast<Atomic<uint32_t>*>(src_bytes)->LoadRelaxed());
      src_bytes += sizeof(uint32_t);
      dst_bytes += sizeof(uint32_t);
      num_bytes -= sizeof(uint32_t);
    }
    // Copy remaining bytes, avoid going past the end of num_bytes since there may be a redzone
    // there.
    while (num_bytes > 0) {
      reinterpret_cast<Atomic<uint8_t>*>(dst_bytes)->StoreRelaxed(
          reinterpret_cast<Atomic<uint8_t>*>(src_bytes)->LoadRelaxed());
      src_bytes += sizeof(uint8_t);
      dst_bytes += sizeof(uint8_t);
      num_bytes -= sizeof(uint8_t);
    }
  }

  if (kUseReadBarrier) {
    // We need a RB here. After copying the whole object above, copy references fields one by one
    // again with a RB to make sure there are no from space refs. TODO: Optimize this later?
    CopyReferenceFieldsWithReadBarrierVisitor visitor(dest);
    src->VisitReferences(visitor, visitor);
  }
  gc::Heap* heap = Runtime::Current()->GetHeap();
  // Perform write barriers on copied object references.
  ObjPtr<Class> c = src->GetClass();
  if (c->IsArrayClass()) {
    if (!c->GetComponentType()->IsPrimitive()) {
      ObjectArray<Object>* array = dest->AsObjectArray<Object>();
      heap->WriteBarrierArray(dest, 0, array->GetLength());
    }
  } else {
    heap->WriteBarrierEveryFieldOf(dest);
  }
  return dest.Ptr();
}

// An allocation pre-fence visitor that copies the object.
class CopyObjectVisitor {
 public:
  CopyObjectVisitor(Handle<Object>* orig, size_t num_bytes)
      : orig_(orig), num_bytes_(num_bytes) {}

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Object::CopyObject(obj, orig_->Get(), num_bytes_);
  }

 private:
  Handle<Object>* const orig_;
  const size_t num_bytes_;
  DISALLOW_COPY_AND_ASSIGN(CopyObjectVisitor);
};

Object* Object::Clone(Thread* self) {
  CHECK(!IsClass()) << "Can't clone classes.";
  // Object::SizeOf gets the right size even if we're an array. Using c->AllocObject() here would
  // be wrong.
  gc::Heap* heap = Runtime::Current()->GetHeap();
  size_t num_bytes = SizeOf();
  StackHandleScope<1> hs(self);
  Handle<Object> this_object(hs.NewHandle(this));
  ObjPtr<Object> copy;
  CopyObjectVisitor visitor(&this_object, num_bytes);
  if (heap->IsMovableObject(this)) {
    copy = heap->AllocObject<true>(self, GetClass(), num_bytes, visitor);
  } else {
    copy = heap->AllocNonMovableObject<true>(self, GetClass(), num_bytes, visitor);
  }
  if (this_object->GetClass()->IsFinalizable()) {
    heap->AddFinalizerReference(self, &copy);
  }
  return copy.Ptr();
}

uint32_t Object::GenerateIdentityHashCode() {
  uint32_t expected_value, new_value;
  do {
    expected_value = hash_code_seed.LoadRelaxed();
    new_value = expected_value * 1103515245 + 12345;
  } while (!hash_code_seed.CompareAndSetWeakRelaxed(expected_value, new_value) ||
      (expected_value & LockWord::kHashMask) == 0);
  return expected_value & LockWord::kHashMask;
}

void Object::SetHashCodeSeed(uint32_t new_seed) {
  hash_code_seed.StoreRelaxed(new_seed);
}

int32_t Object::IdentityHashCode() {
  ObjPtr<Object> current_this = this;  // The this pointer may get invalidated by thread suspension.
  while (true) {
    LockWord lw = current_this->GetLockWord(false);
    switch (lw.GetState()) {
      case LockWord::kUnlocked: {
        // Try to compare and swap in a new hash, if we succeed we will return the hash on the next
        // loop iteration.
        LockWord hash_word = LockWord::FromHashCode(GenerateIdentityHashCode(), lw.GCState());
        DCHECK_EQ(hash_word.GetState(), LockWord::kHashCode);
        if (current_this->CasLockWordWeakRelaxed(lw, hash_word)) {
          return hash_word.GetHashCode();
        }
        break;
      }
      case LockWord::kThinLocked: {
        // Inflate the thin lock to a monitor and stick the hash code inside of the monitor. May
        // fail spuriously.
        Thread* self = Thread::Current();
        StackHandleScope<1> hs(self);
        Handle<mirror::Object> h_this(hs.NewHandle(current_this));
        Monitor::InflateThinLocked(self, h_this, lw, GenerateIdentityHashCode());
        // A GC may have occurred when we switched to kBlocked.
        current_this = h_this.Get();
        break;
      }
      case LockWord::kFatLocked: {
        // Already inflated, return the hash stored in the monitor.
        Monitor* monitor = lw.FatLockMonitor();
        DCHECK(monitor != nullptr);
        return monitor->GetHashCode();
      }
      case LockWord::kHashCode: {
        return lw.GetHashCode();
      }
      default: {
        LOG(FATAL) << "Invalid state during hashcode " << lw.GetState();
        break;
      }
    }
  }
  UNREACHABLE();
}

void Object::CheckFieldAssignmentImpl(MemberOffset field_offset, ObjPtr<Object> new_value) {
  ObjPtr<Class> c = GetClass();
  Runtime* runtime = Runtime::Current();
  if (runtime->GetClassLinker() == nullptr || !runtime->IsStarted() ||
      !runtime->GetHeap()->IsObjectValidationEnabled() || !c->IsResolved()) {
    return;
  }
  for (ObjPtr<Class> cur = c; cur != nullptr; cur = cur->GetSuperClass()) {
    for (ArtField& field : cur->GetIFields()) {
      if (field.GetOffset().Int32Value() == field_offset.Int32Value()) {
        CHECK_NE(field.GetTypeAsPrimitiveType(), Primitive::kPrimNot);
        // TODO: resolve the field type for moving GC.
        ObjPtr<mirror::Class> field_type =
            kMovingCollector ? field.LookupResolvedType() : field.ResolveType();
        if (field_type != nullptr) {
          CHECK(field_type->IsAssignableFrom(new_value->GetClass()));
        }
        return;
      }
    }
  }
  if (c->IsArrayClass()) {
    // Bounds and assign-ability done in the array setter.
    return;
  }
  if (IsClass()) {
    for (ArtField& field : AsClass()->GetSFields()) {
      if (field.GetOffset().Int32Value() == field_offset.Int32Value()) {
        CHECK_NE(field.GetTypeAsPrimitiveType(), Primitive::kPrimNot);
        // TODO: resolve the field type for moving GC.
        ObjPtr<mirror::Class> field_type =
            kMovingCollector ? field.LookupResolvedType() : field.ResolveType();
        if (field_type != nullptr) {
          CHECK(field_type->IsAssignableFrom(new_value->GetClass()));
        }
        return;
      }
    }
  }
  LOG(FATAL) << "Failed to find field for assignment to " << reinterpret_cast<void*>(this)
      << " of type " << c->PrettyDescriptor() << " at offset " << field_offset;
  UNREACHABLE();
}

ArtField* Object::FindFieldByOffset(MemberOffset offset) {
  return IsClass() ? ArtField::FindStaticFieldWithOffset(AsClass(), offset.Uint32Value())
      : ArtField::FindInstanceFieldWithOffset(GetClass(), offset.Uint32Value());
}

std::string Object::PrettyTypeOf(ObjPtr<mirror::Object> obj) {
  if (obj == nullptr) {
    return "null";
  }
  return obj->PrettyTypeOf();
}

std::string Object::PrettyTypeOf() {
  // From-space version is the same as the to-space version since the dex file never changes.
  // Avoiding the read barrier here is important to prevent recursive AssertToSpaceInvariant
  // issues.
  ObjPtr<mirror::Class> klass = GetClass<kDefaultVerifyFlags, kWithoutReadBarrier>();
  if (klass == nullptr) {
    return "(raw)";
  }
  std::string temp;
  std::string result(PrettyDescriptor(klass->GetDescriptor(&temp)));
  if (klass->IsClassClass()) {
    result += "<" + PrettyDescriptor(AsClass()->GetDescriptor(&temp)) + ">";
  }
  return result;
}

}  // namespace mirror
}  // namespace art
