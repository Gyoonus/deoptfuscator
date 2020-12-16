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

#include <stdint.h>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "callee_save_frame.h"
#include "dex/dex_file-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "gc_root-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object_reference.h"

namespace art {

inline constexpr bool FindFieldTypeIsRead(FindFieldType type) {
  return type == InstanceObjectRead ||
         type == InstancePrimitiveRead ||
         type == StaticObjectRead ||
         type == StaticPrimitiveRead;
}

// Helper function to do a null check after trying to resolve the field. Not for statics since obj
// does not exist there. There is a suspend check, object is a double pointer to update the value
// in the caller in case it moves.
template<FindFieldType type, bool kAccessCheck>
ALWAYS_INLINE static inline ArtField* FindInstanceField(uint32_t field_idx,
                                                        ArtMethod* referrer,
                                                        Thread* self,
                                                        size_t size,
                                                        mirror::Object** obj)
    REQUIRES(!Roles::uninterruptible_)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  HandleWrapper<mirror::Object> h(hs.NewHandleWrapper(obj));
  ArtField* field = FindFieldFromCode<type, kAccessCheck>(field_idx, referrer, self, size);
  if (LIKELY(field != nullptr) && UNLIKELY(h == nullptr)) {
    ThrowNullPointerExceptionForFieldAccess(field, /*is_read*/FindFieldTypeIsRead(type));
    return nullptr;
  }
  return field;
}

static ArtMethod* GetReferrer(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (kIsDebugBuild) {
    // stub_test doesn't call this code with a proper frame, so get the outer, and if
    // it does not have compiled code return it.
    ArtMethod* outer = GetCalleeSaveOuterMethod(self, CalleeSaveType::kSaveRefsOnly);
    if (outer->GetEntryPointFromQuickCompiledCode() == nullptr) {
      return outer;
    }
  }
  return GetCalleeSaveMethodCallerAndOuterMethod(self, CalleeSaveType::kSaveRefsOnly).caller;
}

// Macro used to define this set of functions:
//
//   art{Get,Set}<Kind>{Static,Instance}FromCode
//   art{Get,Set}<Kind>{Static,Instance}FromCompiledCode
//
#define ART_GET_FIELD_FROM_CODE(Kind, PrimitiveType, RetType, SetType,         \
                                PrimitiveOrObject, IsObject, Ptr)              \
  extern "C" RetType artGet ## Kind ## StaticFromCode(uint32_t field_idx,      \
                                                      ArtMethod* referrer,     \
                                                      Thread* self)            \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    ScopedQuickEntrypointChecks sqec(self);                                    \
    ArtField* field = FindFieldFast(                                           \
        field_idx, referrer, Static ## PrimitiveOrObject ## Read,              \
        sizeof(PrimitiveType));                                                \
    if (LIKELY(field != nullptr)) {                                            \
      return field->Get ## Kind (field->GetDeclaringClass())Ptr;  /* NOLINT */ \
    }                                                                          \
    field = FindFieldFromCode<Static ## PrimitiveOrObject ## Read, true>(      \
        field_idx, referrer, self, sizeof(PrimitiveType));                     \
    if (LIKELY(field != nullptr)) {                                            \
      return field->Get ## Kind (field->GetDeclaringClass())Ptr;  /* NOLINT */ \
    }                                                                          \
    /* Will throw exception by checking with Thread::Current. */               \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  extern "C" RetType artGet ## Kind ## InstanceFromCode(uint32_t field_idx,    \
                                                        mirror::Object* obj,   \
                                                        ArtMethod* referrer,   \
                                                        Thread* self)          \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    ScopedQuickEntrypointChecks sqec(self);                                    \
    ArtField* field = FindFieldFast(                                           \
        field_idx, referrer, Instance ## PrimitiveOrObject ## Read,            \
        sizeof(PrimitiveType));                                                \
    if (LIKELY(field != nullptr) && obj != nullptr) {                          \
      return field->Get ## Kind (obj)Ptr;  /* NOLINT */                        \
    }                                                                          \
    field = FindInstanceField<Instance ## PrimitiveOrObject ## Read, true>(    \
        field_idx, referrer, self, sizeof(PrimitiveType), &obj);               \
    if (LIKELY(field != nullptr)) {                                            \
      return field->Get ## Kind (obj)Ptr;  /* NOLINT */                        \
    }                                                                          \
    /* Will throw exception by checking with Thread::Current. */               \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  extern "C" int artSet ## Kind ## StaticFromCode(uint32_t field_idx,          \
                                                  SetType new_value,           \
                                                  ArtMethod* referrer,         \
                                                  Thread* self)                \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    ScopedQuickEntrypointChecks sqec(self);                                    \
    ArtField* field = FindFieldFast(                                           \
        field_idx, referrer, Static ## PrimitiveOrObject ## Write,             \
        sizeof(PrimitiveType));                                                \
    if (LIKELY(field != nullptr)) {                                            \
      field->Set ## Kind <false>(field->GetDeclaringClass(), new_value);       \
      return 0;                                                                \
    }                                                                          \
    if (IsObject) {                                                            \
      StackHandleScope<1> hs(self);                                            \
      HandleWrapper<mirror::Object> h_obj(hs.NewHandleWrapper(                 \
          reinterpret_cast<mirror::Object**>(&new_value)));                    \
      field = FindFieldFromCode<Static ## PrimitiveOrObject ## Write, true>(   \
          field_idx, referrer, self, sizeof(PrimitiveType));                   \
    } else {                                                                   \
      field = FindFieldFromCode<Static ## PrimitiveOrObject ## Write, true>(   \
          field_idx, referrer, self, sizeof(PrimitiveType));                   \
    }                                                                          \
    if (LIKELY(field != nullptr)) {                                            \
      field->Set ## Kind <false>(field->GetDeclaringClass(), new_value);       \
      return 0;                                                                \
    }                                                                          \
    return -1;                                                                 \
  }                                                                            \
                                                                               \
  extern "C" int artSet ## Kind ## InstanceFromCode(uint32_t field_idx,        \
                                                    mirror::Object* obj,       \
                                                    SetType new_value,         \
                                                    ArtMethod* referrer,       \
                                                    Thread* self)              \
    REQUIRES_SHARED(Locks::mutator_lock_) {                                    \
    ScopedQuickEntrypointChecks sqec(self);                                    \
    ArtField* field = FindFieldFast(                                           \
        field_idx, referrer, Instance ## PrimitiveOrObject ## Write,           \
        sizeof(PrimitiveType));                                                \
    if (LIKELY(field != nullptr && obj != nullptr)) {                          \
      field->Set ## Kind <false>(obj, new_value);                              \
      return 0;                                                                \
    }                                                                          \
    if (IsObject) {                                                            \
      StackHandleScope<1> hs(self);                                            \
      HandleWrapper<mirror::Object> h_obj(hs.NewHandleWrapper(                 \
          reinterpret_cast<mirror::Object**>(&new_value)));                    \
      field = FindInstanceField<Instance ## PrimitiveOrObject ## Write, true>( \
          field_idx,                                                           \
          referrer,                                                            \
          self,                                                                \
          sizeof(PrimitiveType),                                               \
          &obj);                                                               \
    } else {                                                                   \
      field = FindInstanceField<Instance ## PrimitiveOrObject ## Write, true>( \
          field_idx,                                                           \
          referrer,                                                            \
          self,                                                                \
          sizeof(PrimitiveType),                                               \
          &obj);                                                               \
    }                                                                          \
    if (LIKELY(field != nullptr)) {                                            \
      field->Set ## Kind<false>(obj, new_value);                               \
      return 0;                                                                \
    }                                                                          \
    return -1;                                                                 \
  }                                                                            \
                                                                               \
  extern "C" RetType artGet ## Kind ## StaticFromCompiledCode(                 \
      uint32_t field_idx,                                                      \
      Thread* self)                                                            \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    return artGet ## Kind ## StaticFromCode(                                   \
        field_idx, GetReferrer(self), self);                                   \
  }                                                                            \
                                                                               \
  extern "C" RetType artGet ## Kind ## InstanceFromCompiledCode(               \
      uint32_t field_idx,                                                      \
      mirror::Object* obj,                                                     \
      Thread* self)                                                            \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    return artGet ## Kind ## InstanceFromCode(                                 \
        field_idx, obj, GetReferrer(self), self);                              \
  }                                                                            \
                                                                               \
  extern "C" int artSet ## Kind ## StaticFromCompiledCode(                     \
      uint32_t field_idx,                                                      \
      SetType new_value,                                                       \
      Thread* self)                                                            \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    return artSet ## Kind ## StaticFromCode(                                   \
        field_idx, new_value, GetReferrer(self), self);                        \
  }                                                                            \
                                                                               \
  extern "C" int artSet ## Kind ## InstanceFromCompiledCode(                   \
      uint32_t field_idx,                                                      \
      mirror::Object* obj,                                                     \
      SetType new_value,                                                       \
      Thread* self)                                                            \
      REQUIRES_SHARED(Locks::mutator_lock_) {                                  \
    return artSet ## Kind ## InstanceFromCode(                                 \
        field_idx, obj, new_value, GetReferrer(self), self);                   \
  }

// Define these functions:
//
//   artGetByteStaticFromCode
//   artGetByteInstanceFromCode
//   artSetByteStaticFromCode
//   artSetByteInstanceFromCode
//   artGetByteStaticFromCompiledCode
//   artGetByteInstanceFromCompiledCode
//   artSetByteStaticFromCompiledCode
//   artSetByteInstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(Byte, int8_t, ssize_t, uint32_t, Primitive, false, )

// Define these functions:
//
//   artGetBooleanStaticFromCode
//   artGetBooleanInstanceFromCode
//   artSetBooleanStaticFromCode
//   artSetBooleanInstanceFromCode
//   artGetBooleanStaticFromCompiledCode
//   artGetBooleanInstanceFromCompiledCode
//   artSetBooleanStaticFromCompiledCode
//   artSetBooleanInstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(Boolean, int8_t, size_t, uint32_t, Primitive, false, )

// Define these functions:
//
//   artGetShortStaticFromCode
//   artGetShortInstanceFromCode
//   artSetShortStaticFromCode
//   artSetShortInstanceFromCode
//   artGetShortStaticFromCompiledCode
//   artGetShortInstanceFromCompiledCode
//   artSetShortStaticFromCompiledCode
//   artSetShortInstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(Short, int16_t, ssize_t, uint16_t, Primitive, false, )

// Define these functions:
//
//   artGetCharStaticFromCode
//   artGetCharInstanceFromCode
//   artSetCharStaticFromCode
//   artSetCharInstanceFromCode
//   artGetCharStaticFromCompiledCode
//   artGetCharInstanceFromCompiledCode
//   artSetCharStaticFromCompiledCode
//   artSetCharInstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(Char, int16_t, size_t, uint16_t, Primitive, false, )

// Define these functions:
//
//   artGet32StaticFromCode
//   artGet32InstanceFromCode
//   artSet32StaticFromCode
//   artSet32InstanceFromCode
//   artGet32StaticFromCompiledCode
//   artGet32InstanceFromCompiledCode
//   artSet32StaticFromCompiledCode
//   artSet32InstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(32, int32_t, size_t, uint32_t, Primitive, false, )

// Define these functions:
//
//   artGet64StaticFromCode
//   artGet64InstanceFromCode
//   artSet64StaticFromCode
//   artSet64InstanceFromCode
//   artGet64StaticFromCompiledCode
//   artGet64InstanceFromCompiledCode
//   artSet64StaticFromCompiledCode
//   artSet64InstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(64, int64_t, uint64_t, uint64_t, Primitive, false, )

// Define these functions:
//
//   artGetObjStaticFromCode
//   artGetObjInstanceFromCode
//   artSetObjStaticFromCode
//   artSetObjInstanceFromCode
//   artGetObjStaticFromCompiledCode
//   artGetObjInstanceFromCompiledCode
//   artSetObjStaticFromCompiledCode
//   artSetObjInstanceFromCompiledCode
//
ART_GET_FIELD_FROM_CODE(Obj, mirror::HeapReference<mirror::Object>, mirror::Object*,
                        mirror::Object*, Object, true, .Ptr())

#undef ART_GET_FIELD_FROM_CODE


// To cut on the number of entrypoints, we have shared entries for
// byte/boolean and char/short for setting an instance or static field. We just
// forward those to the unsigned variant.
extern "C" int artSet8StaticFromCompiledCode(uint32_t field_idx,
                                             uint32_t new_value,
                                             Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetBooleanStaticFromCode(field_idx, new_value, GetReferrer(self), self);
}

extern "C" int artSet16StaticFromCompiledCode(uint32_t field_idx,
                                              uint16_t new_value,
                                              Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetCharStaticFromCode(field_idx, new_value, GetReferrer(self), self);
}

extern "C" int artSet8InstanceFromCompiledCode(uint32_t field_idx,
                                               mirror::Object* obj,
                                               uint8_t new_value,
                                               Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetBooleanInstanceFromCode(field_idx, obj, new_value, GetReferrer(self), self);
}

extern "C" int artSet16InstanceFromCompiledCode(uint32_t field_idx,
                                                mirror::Object* obj,
                                                uint16_t new_value,
                                                Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetCharInstanceFromCode(field_idx, obj, new_value, GetReferrer(self), self);
}

extern "C" int artSet8StaticFromCode(uint32_t field_idx,
                                     uint32_t new_value,
                                     ArtMethod* referrer,
                                     Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetBooleanStaticFromCode(field_idx, new_value, referrer, self);
}

extern "C" int artSet16StaticFromCode(uint32_t field_idx,
                                      uint16_t new_value,
                                      ArtMethod* referrer,
                                      Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetCharStaticFromCode(field_idx, new_value, referrer, self);
}

extern "C" int artSet8InstanceFromCode(uint32_t field_idx,
                                       mirror::Object* obj,
                                       uint8_t new_value,
                                       ArtMethod* referrer,
                                       Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetBooleanInstanceFromCode(field_idx, obj, new_value, referrer, self);
}

extern "C" int artSet16InstanceFromCode(uint32_t field_idx,
                                        mirror::Object* obj,
                                        uint16_t new_value,
                                        ArtMethod* referrer,
                                        Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return artSetCharInstanceFromCode(field_idx, obj, new_value, referrer, self);
}

extern "C" mirror::Object* artReadBarrierMark(mirror::Object* obj) {
  DCHECK(kEmitCompilerReadBarrier);
  return ReadBarrier::Mark(obj);
}

extern "C" mirror::Object* artReadBarrierSlow(mirror::Object* ref ATTRIBUTE_UNUSED,
                                              mirror::Object* obj,
                                              uint32_t offset) {
  // Used only in connection with non-volatile loads.
  DCHECK(kEmitCompilerReadBarrier);
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(obj) + offset;
  mirror::HeapReference<mirror::Object>* ref_addr =
     reinterpret_cast<mirror::HeapReference<mirror::Object>*>(raw_addr);
  constexpr ReadBarrierOption kReadBarrierOption =
      kUseReadBarrier ? kWithReadBarrier : kWithoutReadBarrier;
  mirror::Object* result =
      ReadBarrier::Barrier<mirror::Object, /* kIsVolatile */ false, kReadBarrierOption>(
        obj,
        MemberOffset(offset),
        ref_addr);
  return result;
}

extern "C" mirror::Object* artReadBarrierForRootSlow(GcRoot<mirror::Object>* root) {
  DCHECK(kEmitCompilerReadBarrier);
  return root->Read();
}

}  // namespace art
