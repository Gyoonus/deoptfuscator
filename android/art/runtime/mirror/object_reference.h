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

#ifndef ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_
#define ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_

#include "base/atomic.h"
#include "base/mutex.h"  // For Locks::mutator_lock_.
#include "globals.h"
#include "heap_poisoning.h"
#include "obj_ptr.h"

namespace art {
namespace mirror {

class Object;

// Classes shared with the managed side of the world need to be packed so that they don't have
// extra platform specific padding.
#define MANAGED PACKED(4)

template<bool kPoisonReferences, class MirrorType>
class PtrCompression {
 public:
  // Compress reference to its bit representation.
  static uint32_t Compress(MirrorType* mirror_ptr) {
    uintptr_t as_bits = reinterpret_cast<uintptr_t>(mirror_ptr);
    return static_cast<uint32_t>(kPoisonReferences ? -as_bits : as_bits);
  }

  // Uncompress an encoded reference from its bit representation.
  static MirrorType* Decompress(uint32_t ref) {
    uintptr_t as_bits = kPoisonReferences ? -ref : ref;
    return reinterpret_cast<MirrorType*>(as_bits);
  }

  // Convert an ObjPtr to a compressed reference.
  static uint32_t Compress(ObjPtr<MirrorType> ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    return Compress(ptr.Ptr());
  }
};

// Value type representing a reference to a mirror::Object of type MirrorType.
template<bool kPoisonReferences, class MirrorType>
class MANAGED ObjectReference {
 private:
  using Compression = PtrCompression<kPoisonReferences, MirrorType>;

 public:
  MirrorType* AsMirrorPtr() const {
    return Compression::Decompress(reference_);
  }

  void Assign(MirrorType* other) {
    reference_ = Compression::Compress(other);
  }

  void Assign(ObjPtr<MirrorType> ptr) REQUIRES_SHARED(Locks::mutator_lock_);

  void Clear() {
    reference_ = 0;
    DCHECK(IsNull());
  }

  bool IsNull() const {
    return reference_ == 0;
  }

  uint32_t AsVRegValue() const {
    return reference_;
  }

  static ObjectReference<kPoisonReferences, MirrorType> FromMirrorPtr(MirrorType* mirror_ptr)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return ObjectReference<kPoisonReferences, MirrorType>(mirror_ptr);
  }

 protected:
  explicit ObjectReference(MirrorType* mirror_ptr) REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Compression::Compress(mirror_ptr)) {
  }

  // The encoded reference to a mirror::Object.
  uint32_t reference_;
};

// References between objects within the managed heap.
// Similar API to ObjectReference, but not a value type. Supports atomic access.
template<class MirrorType>
class MANAGED HeapReference {
 private:
  using Compression = PtrCompression<kPoisonHeapReferences, MirrorType>;

 public:
  HeapReference() REQUIRES_SHARED(Locks::mutator_lock_) : HeapReference(nullptr) {}

  template <bool kIsVolatile = false>
  MirrorType* AsMirrorPtr() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Compression::Decompress(
        kIsVolatile ? reference_.LoadSequentiallyConsistent() : reference_.LoadJavaData());
  }

  template <bool kIsVolatile = false>
  void Assign(MirrorType* other) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kIsVolatile) {
      reference_.StoreSequentiallyConsistent(Compression::Compress(other));
    } else {
      reference_.StoreJavaData(Compression::Compress(other));
    }
  }

  template <bool kIsVolatile = false>
  void Assign(ObjPtr<MirrorType> ptr) REQUIRES_SHARED(Locks::mutator_lock_);

  void Clear() {
    reference_.StoreJavaData(0);
    DCHECK(IsNull());
  }

  bool IsNull() const {
    return reference_.LoadJavaData() == 0;
  }

  static HeapReference<MirrorType> FromMirrorPtr(MirrorType* mirror_ptr)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return HeapReference<MirrorType>(mirror_ptr);
  }

  bool CasWeakRelaxed(MirrorType* old_ptr, MirrorType* new_ptr)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  explicit HeapReference(MirrorType* mirror_ptr) REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Compression::Compress(mirror_ptr)) {}

  // The encoded reference to a mirror::Object. Atomically updateable.
  Atomic<uint32_t> reference_;
};

static_assert(sizeof(mirror::HeapReference<mirror::Object>) == kHeapReferenceSize,
              "heap reference size does not match");

// Standard compressed reference used in the runtime. Used for StackReference and GC roots.
template<class MirrorType>
class MANAGED CompressedReference : public mirror::ObjectReference<false, MirrorType> {
 public:
  CompressedReference<MirrorType>() REQUIRES_SHARED(Locks::mutator_lock_)
      : mirror::ObjectReference<false, MirrorType>(nullptr) {}

  static CompressedReference<MirrorType> FromMirrorPtr(MirrorType* p)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return CompressedReference<MirrorType>(p);
  }

 private:
  explicit CompressedReference(MirrorType* p) REQUIRES_SHARED(Locks::mutator_lock_)
      : mirror::ObjectReference<false, MirrorType>(p) {}
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_REFERENCE_H_
