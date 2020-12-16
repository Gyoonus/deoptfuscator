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

#include "entrypoints/quick/quick_alloc_entrypoints.h"

#include "art_method-inl.h"
#include "base/enums.h"
#include "base/quasi_atomic.h"
#include "callee_save_frame.h"
#include "dex/dex_file_types.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"

namespace art {

static constexpr bool kUseTlabFastPath = true;

template <bool kInitialized,
          bool kFinalize,
          bool kInstrumented,
          gc::AllocatorType allocator_type>
static ALWAYS_INLINE inline mirror::Object* artAllocObjectFromCode(
    mirror::Class* klass,
    Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  DCHECK(klass != nullptr);
  if (kUseTlabFastPath && !kInstrumented && allocator_type == gc::kAllocatorTypeTLAB) {
    if (kInitialized || klass->IsInitialized()) {
      if (!kFinalize || !klass->IsFinalizable()) {
        size_t byte_count = klass->GetObjectSize();
        byte_count = RoundUp(byte_count, gc::space::BumpPointerSpace::kAlignment);
        mirror::Object* obj;
        if (LIKELY(byte_count < self->TlabSize())) {
          obj = self->AllocTlab(byte_count);
          DCHECK(obj != nullptr) << "AllocTlab can't fail";
          obj->SetClass(klass);
          if (kUseBakerReadBarrier) {
            obj->AssertReadBarrierState();
          }
          QuasiAtomic::ThreadFenceForConstructor();
          return obj;
        }
      }
    }
  }
  if (kInitialized) {
    return AllocObjectFromCodeInitialized<kInstrumented>(klass, self, allocator_type);
  } else if (!kFinalize) {
    return AllocObjectFromCodeResolved<kInstrumented>(klass, self, allocator_type);
  } else {
    return AllocObjectFromCode<kInstrumented>(klass, self, allocator_type);
  }
}

#define GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, suffix2, instrumented_bool, allocator_type) \
extern "C" mirror::Object* artAllocObjectFromCodeWithChecks##suffix##suffix2( \
    mirror::Class* klass, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  return artAllocObjectFromCode<false, true, instrumented_bool, allocator_type>(klass, self); \
} \
extern "C" mirror::Object* artAllocObjectFromCodeResolved##suffix##suffix2( \
    mirror::Class* klass, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  return artAllocObjectFromCode<false, false, instrumented_bool, allocator_type>(klass, self); \
} \
extern "C" mirror::Object* artAllocObjectFromCodeInitialized##suffix##suffix2( \
    mirror::Class* klass, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  return artAllocObjectFromCode<true, false, instrumented_bool, allocator_type>(klass, self); \
} \
extern "C" mirror::Array* artAllocArrayFromCodeResolved##suffix##suffix2( \
    mirror::Class* klass, int32_t component_count, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  ScopedQuickEntrypointChecks sqec(self); \
  return AllocArrayFromCodeResolved<instrumented_bool>(klass, component_count, self, \
                                                       allocator_type); \
} \
extern "C" mirror::String* artAllocStringFromBytesFromCode##suffix##suffix2( \
    mirror::ByteArray* byte_array, int32_t high, int32_t offset, int32_t byte_count, \
    Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  ScopedQuickEntrypointChecks sqec(self); \
  StackHandleScope<1> hs(self); \
  Handle<mirror::ByteArray> handle_array(hs.NewHandle(byte_array)); \
  return mirror::String::AllocFromByteArray<instrumented_bool>(self, byte_count, handle_array, \
                                                               offset, high, allocator_type); \
} \
extern "C" mirror::String* artAllocStringFromCharsFromCode##suffix##suffix2( \
    int32_t offset, int32_t char_count, mirror::CharArray* char_array, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  StackHandleScope<1> hs(self); \
  Handle<mirror::CharArray> handle_array(hs.NewHandle(char_array)); \
  return mirror::String::AllocFromCharArray<instrumented_bool>(self, char_count, handle_array, \
                                                               offset, allocator_type); \
} \
extern "C" mirror::String* artAllocStringFromStringFromCode##suffix##suffix2( /* NOLINT */ \
    mirror::String* string, Thread* self) \
    REQUIRES_SHARED(Locks::mutator_lock_) { \
  StackHandleScope<1> hs(self); \
  Handle<mirror::String> handle_string(hs.NewHandle(string)); \
  return mirror::String::AllocFromString<instrumented_bool>(self, handle_string->GetLength(), \
                                                            handle_string, 0, allocator_type); \
}

#define GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(suffix, allocator_type) \
    GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, Instrumented, true, allocator_type) \
    GENERATE_ENTRYPOINTS_FOR_ALLOCATOR_INST(suffix, , false, allocator_type)

GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(DlMalloc, gc::kAllocatorTypeDlMalloc)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(RosAlloc, gc::kAllocatorTypeRosAlloc)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(BumpPointer, gc::kAllocatorTypeBumpPointer)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(TLAB, gc::kAllocatorTypeTLAB)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(Region, gc::kAllocatorTypeRegion)
GENERATE_ENTRYPOINTS_FOR_ALLOCATOR(RegionTLAB, gc::kAllocatorTypeRegionTLAB)

#define GENERATE_ENTRYPOINTS(suffix) \
extern "C" void* art_quick_alloc_array_resolved##suffix(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved8##suffix(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved16##suffix(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved32##suffix(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved64##suffix(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_object_resolved##suffix(mirror::Class* klass); \
extern "C" void* art_quick_alloc_object_initialized##suffix(mirror::Class* klass); \
extern "C" void* art_quick_alloc_object_with_checks##suffix(mirror::Class* klass); \
extern "C" void* art_quick_alloc_string_from_bytes##suffix(void*, int32_t, int32_t, int32_t); \
extern "C" void* art_quick_alloc_string_from_chars##suffix(int32_t, int32_t, void*); \
extern "C" void* art_quick_alloc_string_from_string##suffix(void*); \
extern "C" void* art_quick_alloc_array_resolved##suffix##_instrumented(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved8##suffix##_instrumented(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved16##suffix##_instrumented(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved32##suffix##_instrumented(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_array_resolved64##suffix##_instrumented(mirror::Class* klass, int32_t); \
extern "C" void* art_quick_alloc_object_resolved##suffix##_instrumented(mirror::Class* klass); \
extern "C" void* art_quick_alloc_object_initialized##suffix##_instrumented(mirror::Class* klass); \
extern "C" void* art_quick_alloc_object_with_checks##suffix##_instrumented(mirror::Class* klass); \
extern "C" void* art_quick_alloc_string_from_bytes##suffix##_instrumented(void*, int32_t, int32_t, int32_t); \
extern "C" void* art_quick_alloc_string_from_chars##suffix##_instrumented(int32_t, int32_t, void*); \
extern "C" void* art_quick_alloc_string_from_string##suffix##_instrumented(void*); \
void SetQuickAllocEntryPoints##suffix(QuickEntryPoints* qpoints, bool instrumented) { \
  if (instrumented) { \
    qpoints->pAllocArrayResolved = art_quick_alloc_array_resolved##suffix##_instrumented; \
    qpoints->pAllocArrayResolved8 = art_quick_alloc_array_resolved8##suffix##_instrumented; \
    qpoints->pAllocArrayResolved16 = art_quick_alloc_array_resolved16##suffix##_instrumented; \
    qpoints->pAllocArrayResolved32 = art_quick_alloc_array_resolved32##suffix##_instrumented; \
    qpoints->pAllocArrayResolved64 = art_quick_alloc_array_resolved64##suffix##_instrumented; \
    qpoints->pAllocObjectResolved = art_quick_alloc_object_resolved##suffix##_instrumented; \
    qpoints->pAllocObjectInitialized = art_quick_alloc_object_initialized##suffix##_instrumented; \
    qpoints->pAllocObjectWithChecks = art_quick_alloc_object_with_checks##suffix##_instrumented; \
    qpoints->pAllocStringFromBytes = art_quick_alloc_string_from_bytes##suffix##_instrumented; \
    qpoints->pAllocStringFromChars = art_quick_alloc_string_from_chars##suffix##_instrumented; \
    qpoints->pAllocStringFromString = art_quick_alloc_string_from_string##suffix##_instrumented; \
  } else { \
    qpoints->pAllocArrayResolved = art_quick_alloc_array_resolved##suffix; \
    qpoints->pAllocArrayResolved8 = art_quick_alloc_array_resolved8##suffix; \
    qpoints->pAllocArrayResolved16 = art_quick_alloc_array_resolved16##suffix; \
    qpoints->pAllocArrayResolved32 = art_quick_alloc_array_resolved32##suffix; \
    qpoints->pAllocArrayResolved64 = art_quick_alloc_array_resolved64##suffix; \
    qpoints->pAllocObjectResolved = art_quick_alloc_object_resolved##suffix; \
    qpoints->pAllocObjectInitialized = art_quick_alloc_object_initialized##suffix; \
    qpoints->pAllocObjectWithChecks = art_quick_alloc_object_with_checks##suffix; \
    qpoints->pAllocStringFromBytes = art_quick_alloc_string_from_bytes##suffix; \
    qpoints->pAllocStringFromChars = art_quick_alloc_string_from_chars##suffix; \
    qpoints->pAllocStringFromString = art_quick_alloc_string_from_string##suffix; \
  } \
}

// Generate the entrypoint functions.
#if !defined(__APPLE__) || !defined(__LP64__)
GENERATE_ENTRYPOINTS(_dlmalloc)
GENERATE_ENTRYPOINTS(_rosalloc)
GENERATE_ENTRYPOINTS(_bump_pointer)
GENERATE_ENTRYPOINTS(_tlab)
GENERATE_ENTRYPOINTS(_region)
GENERATE_ENTRYPOINTS(_region_tlab)
#endif

static bool entry_points_instrumented = false;
static gc::AllocatorType entry_points_allocator = gc::kAllocatorTypeDlMalloc;

void SetQuickAllocEntryPointsAllocator(gc::AllocatorType allocator) {
  entry_points_allocator = allocator;
}

void SetQuickAllocEntryPointsInstrumented(bool instrumented) {
  entry_points_instrumented = instrumented;
}

void ResetQuickAllocEntryPoints(QuickEntryPoints* qpoints, bool is_marking) {
#if !defined(__APPLE__) || !defined(__LP64__)
  switch (entry_points_allocator) {
    case gc::kAllocatorTypeDlMalloc: {
      SetQuickAllocEntryPoints_dlmalloc(qpoints, entry_points_instrumented);
      return;
    }
    case gc::kAllocatorTypeRosAlloc: {
      SetQuickAllocEntryPoints_rosalloc(qpoints, entry_points_instrumented);
      return;
    }
    case gc::kAllocatorTypeBumpPointer: {
      CHECK(kMovingCollector);
      SetQuickAllocEntryPoints_bump_pointer(qpoints, entry_points_instrumented);
      return;
    }
    case gc::kAllocatorTypeTLAB: {
      CHECK(kMovingCollector);
      SetQuickAllocEntryPoints_tlab(qpoints, entry_points_instrumented);
      return;
    }
    case gc::kAllocatorTypeRegion: {
      CHECK(kMovingCollector);
      SetQuickAllocEntryPoints_region(qpoints, entry_points_instrumented);
      return;
    }
    case gc::kAllocatorTypeRegionTLAB: {
      CHECK(kMovingCollector);
      if (is_marking) {
        SetQuickAllocEntryPoints_region_tlab(qpoints, entry_points_instrumented);
      } else {
        // Not marking means we need no read barriers and can just use the normal TLAB case.
        SetQuickAllocEntryPoints_tlab(qpoints, entry_points_instrumented);
      }
      return;
    }
    default:
      break;
  }
#else
  UNUSED(qpoints);
  UNUSED(is_marking);
#endif
  UNIMPLEMENTED(FATAL);
  UNREACHABLE();
}

}  // namespace art
