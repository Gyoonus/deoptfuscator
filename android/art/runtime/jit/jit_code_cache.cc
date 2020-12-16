/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit_code_cache.h"

#include <sstream>

#include "arch/context.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/logging.h"  // For VLOG.
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "cha.h"
#include "debugger_interface.h"
#include "dex/dex_file_loader.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc/accounting/bitmap-inl.h"
#include "gc/scoped_gc_critical_section.h"
#include "handle.h"
#include "intern_table.h"
#include "jit/jit.h"
#include "jit/profiling_info.h"
#include "linear_alloc.h"
#include "mem_map.h"
#include "oat_file-inl.h"
#include "oat_quick_method_header.h"
#include "object_callbacks.h"
#include "profile_compilation_info.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"
#include "thread_list.h"

namespace art {
namespace jit {

static constexpr int kProtData = PROT_READ | PROT_WRITE;
static constexpr int kProtCode = PROT_READ | PROT_EXEC;

static constexpr size_t kCodeSizeLogThreshold = 50 * KB;
static constexpr size_t kStackMapSizeLogThreshold = 50 * KB;

class JitCodeCache::JniStubKey {
 public:
  explicit JniStubKey(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_)
      : shorty_(method->GetShorty()),
        is_static_(method->IsStatic()),
        is_fast_native_(method->IsFastNative()),
        is_critical_native_(method->IsCriticalNative()),
        is_synchronized_(method->IsSynchronized()) {
    DCHECK(!(is_fast_native_ && is_critical_native_));
  }

  bool operator<(const JniStubKey& rhs) const {
    if (is_static_ != rhs.is_static_) {
      return rhs.is_static_;
    }
    if (is_synchronized_ != rhs.is_synchronized_) {
      return rhs.is_synchronized_;
    }
    if (is_fast_native_ != rhs.is_fast_native_) {
      return rhs.is_fast_native_;
    }
    if (is_critical_native_ != rhs.is_critical_native_) {
      return rhs.is_critical_native_;
    }
    return strcmp(shorty_, rhs.shorty_) < 0;
  }

  // Update the shorty to point to another method's shorty. Call this function when removing
  // the method that references the old shorty from JniCodeData and not removing the entire
  // JniCodeData; the old shorty may become a dangling pointer when that method is unloaded.
  void UpdateShorty(ArtMethod* method) const REQUIRES_SHARED(Locks::mutator_lock_) {
    const char* shorty = method->GetShorty();
    DCHECK_STREQ(shorty_, shorty);
    shorty_ = shorty;
  }

 private:
  // The shorty points to a DexFile data and may need to change
  // to point to the same shorty in a different DexFile.
  mutable const char* shorty_;

  const bool is_static_;
  const bool is_fast_native_;
  const bool is_critical_native_;
  const bool is_synchronized_;
};

class JitCodeCache::JniStubData {
 public:
  JniStubData() : code_(nullptr), methods_() {}

  void SetCode(const void* code) {
    DCHECK(code != nullptr);
    code_ = code;
  }

  const void* GetCode() const {
    return code_;
  }

  bool IsCompiled() const {
    return GetCode() != nullptr;
  }

  void AddMethod(ArtMethod* method) {
    if (!ContainsElement(methods_, method)) {
      methods_.push_back(method);
    }
  }

  const std::vector<ArtMethod*>& GetMethods() const {
    return methods_;
  }

  void RemoveMethodsIn(const LinearAlloc& alloc) {
    auto kept_end = std::remove_if(
        methods_.begin(),
        methods_.end(),
        [&alloc](ArtMethod* method) { return alloc.ContainsUnsafe(method); });
    methods_.erase(kept_end, methods_.end());
  }

  bool RemoveMethod(ArtMethod* method) {
    auto it = std::find(methods_.begin(), methods_.end(), method);
    if (it != methods_.end()) {
      methods_.erase(it);
      return true;
    } else {
      return false;
    }
  }

  void MoveObsoleteMethod(ArtMethod* old_method, ArtMethod* new_method) {
    std::replace(methods_.begin(), methods_.end(), old_method, new_method);
  }

 private:
  const void* code_;
  std::vector<ArtMethod*> methods_;
};

JitCodeCache* JitCodeCache::Create(size_t initial_capacity,
                                   size_t max_capacity,
                                   bool generate_debug_info,
                                   bool used_only_for_profile_data,
                                   std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  CHECK_GE(max_capacity, initial_capacity);

  // Generating debug information is for using the Linux perf tool on
  // host which does not work with ashmem.
  // Also, target linux does not support ashmem.
  bool use_ashmem = !generate_debug_info && !kIsTargetLinux;

  // With 'perf', we want a 1-1 mapping between an address and a method.
  bool garbage_collect_code = !generate_debug_info;

  // We need to have 32 bit offsets from method headers in code cache which point to things
  // in the data cache. If the maps are more than 4G apart, having multiple maps wouldn't work.
  // Ensure we're below 1 GB to be safe.
  if (max_capacity > 1 * GB) {
    std::ostringstream oss;
    oss << "Maxium code cache capacity is limited to 1 GB, "
        << PrettySize(max_capacity) << " is too big";
    *error_msg = oss.str();
    return nullptr;
  }

  // Decide how we should map the code and data sections.
  // If we use the code cache just for profiling we do not need to map the code section as
  // executable.
  // NOTE 1: this is yet another workaround to bypass strict SElinux policies in order to be able
  //         to profile system server.
  // NOTE 2: We could just not create the code section at all but we will need to
  //         special case too many cases.
  int memmap_flags_prot_code = used_only_for_profile_data ? (kProtCode & ~PROT_EXEC) : kProtCode;

  std::string error_str;
  // Map name specific for android_os_Debug.cpp accounting.
  // Map in low 4gb to simplify accessing root tables for x86_64.
  // We could do PC-relative addressing to avoid this problem, but that
  // would require reserving code and data area before submitting, which
  // means more windows for the code memory to be RWX.
  std::unique_ptr<MemMap> data_map(MemMap::MapAnonymous(
      "data-code-cache", nullptr,
      max_capacity,
      kProtData,
      /* low_4gb */ true,
      /* reuse */ false,
      &error_str,
      use_ashmem));
  if (data_map == nullptr) {
    std::ostringstream oss;
    oss << "Failed to create read write cache: " << error_str << " size=" << max_capacity;
    *error_msg = oss.str();
    return nullptr;
  }

  // Align both capacities to page size, as that's the unit mspaces use.
  initial_capacity = RoundDown(initial_capacity, 2 * kPageSize);
  max_capacity = RoundDown(max_capacity, 2 * kPageSize);

  // Data cache is 1 / 2 of the map.
  // TODO: Make this variable?
  size_t data_size = max_capacity / 2;
  size_t code_size = max_capacity - data_size;
  DCHECK_EQ(code_size + data_size, max_capacity);
  uint8_t* divider = data_map->Begin() + data_size;

  MemMap* code_map = data_map->RemapAtEnd(
      divider,
      "jit-code-cache",
      memmap_flags_prot_code | PROT_WRITE,
      &error_str, use_ashmem);
  if (code_map == nullptr) {
    std::ostringstream oss;
    oss << "Failed to create read write execute cache: " << error_str << " size=" << max_capacity;
    *error_msg = oss.str();
    return nullptr;
  }
  DCHECK_EQ(code_map->Begin(), divider);
  data_size = initial_capacity / 2;
  code_size = initial_capacity - data_size;
  DCHECK_EQ(code_size + data_size, initial_capacity);
  return new JitCodeCache(
      code_map,
      data_map.release(),
      code_size,
      data_size,
      max_capacity,
      garbage_collect_code,
      memmap_flags_prot_code);
}

JitCodeCache::JitCodeCache(MemMap* code_map,
                           MemMap* data_map,
                           size_t initial_code_capacity,
                           size_t initial_data_capacity,
                           size_t max_capacity,
                           bool garbage_collect_code,
                           int memmap_flags_prot_code)
    : lock_("Jit code cache", kJitCodeCacheLock),
      lock_cond_("Jit code cache condition variable", lock_),
      collection_in_progress_(false),
      code_map_(code_map),
      data_map_(data_map),
      max_capacity_(max_capacity),
      current_capacity_(initial_code_capacity + initial_data_capacity),
      code_end_(initial_code_capacity),
      data_end_(initial_data_capacity),
      last_collection_increased_code_cache_(false),
      last_update_time_ns_(0),
      garbage_collect_code_(garbage_collect_code),
      used_memory_for_data_(0),
      used_memory_for_code_(0),
      number_of_compilations_(0),
      number_of_osr_compilations_(0),
      number_of_collections_(0),
      histogram_stack_map_memory_use_("Memory used for stack maps", 16),
      histogram_code_memory_use_("Memory used for compiled code", 16),
      histogram_profiling_info_memory_use_("Memory used for profiling info", 16),
      is_weak_access_enabled_(true),
      inline_cache_cond_("Jit inline cache condition variable", lock_),
      memmap_flags_prot_code_(memmap_flags_prot_code) {

  DCHECK_GE(max_capacity, initial_code_capacity + initial_data_capacity);
  code_mspace_ = create_mspace_with_base(code_map_->Begin(), code_end_, false /*locked*/);
  data_mspace_ = create_mspace_with_base(data_map_->Begin(), data_end_, false /*locked*/);

  if (code_mspace_ == nullptr || data_mspace_ == nullptr) {
    PLOG(FATAL) << "create_mspace_with_base failed";
  }

  SetFootprintLimit(current_capacity_);

  CheckedCall(mprotect,
              "mprotect jit code cache",
              code_map_->Begin(),
              code_map_->Size(),
              memmap_flags_prot_code_);
  CheckedCall(mprotect,
              "mprotect jit data cache",
              data_map_->Begin(),
              data_map_->Size(),
              kProtData);

  VLOG(jit) << "Created jit code cache: initial data size="
            << PrettySize(initial_data_capacity)
            << ", initial code size="
            << PrettySize(initial_code_capacity);
}

JitCodeCache::~JitCodeCache() {}

bool JitCodeCache::ContainsPc(const void* ptr) const {
  return code_map_->Begin() <= ptr && ptr < code_map_->End();
}

bool JitCodeCache::ContainsMethod(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it != jni_stubs_map_.end() &&
        it->second.IsCompiled() &&
        ContainsElement(it->second.GetMethods(), method)) {
      return true;
    }
  } else {
    for (const auto& it : method_code_map_) {
      if (it.second == method) {
        return true;
      }
    }
  }
  return false;
}

const void* JitCodeCache::GetJniStubCode(ArtMethod* method) {
  DCHECK(method->IsNative());
  MutexLock mu(Thread::Current(), lock_);
  auto it = jni_stubs_map_.find(JniStubKey(method));
  if (it != jni_stubs_map_.end()) {
    JniStubData& data = it->second;
    if (data.IsCompiled() && ContainsElement(data.GetMethods(), method)) {
      return data.GetCode();
    }
  }
  return nullptr;
}

class ScopedCodeCacheWrite : ScopedTrace {
 public:
  explicit ScopedCodeCacheWrite(const JitCodeCache* const code_cache,
                                bool only_for_tlb_shootdown = false)
      : ScopedTrace("ScopedCodeCacheWrite"),
        code_cache_(code_cache),
        only_for_tlb_shootdown_(only_for_tlb_shootdown) {
    ScopedTrace trace("mprotect all");
    CheckedCall(
        mprotect,
        "make code writable",
        code_cache_->code_map_->Begin(),
        only_for_tlb_shootdown_ ? kPageSize : code_cache_->code_map_->Size(),
        code_cache_->memmap_flags_prot_code_ | PROT_WRITE);
  }

  ~ScopedCodeCacheWrite() {
    ScopedTrace trace("mprotect code");
    CheckedCall(
        mprotect,
        "make code protected",
        code_cache_->code_map_->Begin(),
        only_for_tlb_shootdown_ ? kPageSize : code_cache_->code_map_->Size(),
        code_cache_->memmap_flags_prot_code_);
  }

 private:
  const JitCodeCache* const code_cache_;

  // If we're using ScopedCacheWrite only for TLB shootdown, we limit the scope of mprotect to
  // one page.
  const bool only_for_tlb_shootdown_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCodeCacheWrite);
};

uint8_t* JitCodeCache::CommitCode(Thread* self,
                                  ArtMethod* method,
                                  uint8_t* stack_map,
                                  uint8_t* method_info,
                                  uint8_t* roots_data,
                                  size_t frame_size_in_bytes,
                                  size_t core_spill_mask,
                                  size_t fp_spill_mask,
                                  const uint8_t* code,
                                  size_t code_size,
                                  size_t data_size,
                                  bool osr,
                                  Handle<mirror::ObjectArray<mirror::Object>> roots,
                                  bool has_should_deoptimize_flag,
                                  const ArenaSet<ArtMethod*>& cha_single_implementation_list) {
  uint8_t* result = CommitCodeInternal(self,
                                       method,
                                       stack_map,
                                       method_info,
                                       roots_data,
                                       frame_size_in_bytes,
                                       core_spill_mask,
                                       fp_spill_mask,
                                       code,
                                       code_size,
                                       data_size,
                                       osr,
                                       roots,
                                       has_should_deoptimize_flag,
                                       cha_single_implementation_list);
  if (result == nullptr) {
    // Retry.
    GarbageCollectCache(self);
    result = CommitCodeInternal(self,
                                method,
                                stack_map,
                                method_info,
                                roots_data,
                                frame_size_in_bytes,
                                core_spill_mask,
                                fp_spill_mask,
                                code,
                                code_size,
                                data_size,
                                osr,
                                roots,
                                has_should_deoptimize_flag,
                                cha_single_implementation_list);
  }
  return result;
}

bool JitCodeCache::WaitForPotentialCollectionToComplete(Thread* self) {
  bool in_collection = false;
  while (collection_in_progress_) {
    in_collection = true;
    lock_cond_.Wait(self);
  }
  return in_collection;
}

static uintptr_t FromCodeToAllocation(const void* code) {
  size_t alignment = GetInstructionSetAlignment(kRuntimeISA);
  return reinterpret_cast<uintptr_t>(code) - RoundUp(sizeof(OatQuickMethodHeader), alignment);
}

static uint32_t ComputeRootTableSize(uint32_t number_of_roots) {
  return sizeof(uint32_t) + number_of_roots * sizeof(GcRoot<mirror::Object>);
}

static uint32_t GetNumberOfRoots(const uint8_t* stack_map) {
  // The length of the table is stored just before the stack map (and therefore at the end of
  // the table itself), in order to be able to fetch it from a `stack_map` pointer.
  return reinterpret_cast<const uint32_t*>(stack_map)[-1];
}

static void FillRootTableLength(uint8_t* roots_data, uint32_t length) {
  // Store the length of the table at the end. This will allow fetching it from a `stack_map`
  // pointer.
  reinterpret_cast<uint32_t*>(roots_data)[length] = length;
}

static const uint8_t* FromStackMapToRoots(const uint8_t* stack_map_data) {
  return stack_map_data - ComputeRootTableSize(GetNumberOfRoots(stack_map_data));
}

static void FillRootTable(uint8_t* roots_data, Handle<mirror::ObjectArray<mirror::Object>> roots)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  GcRoot<mirror::Object>* gc_roots = reinterpret_cast<GcRoot<mirror::Object>*>(roots_data);
  const uint32_t length = roots->GetLength();
  // Put all roots in `roots_data`.
  for (uint32_t i = 0; i < length; ++i) {
    ObjPtr<mirror::Object> object = roots->Get(i);
    if (kIsDebugBuild) {
      // Ensure the string is strongly interned. b/32995596
      if (object->IsString()) {
        ObjPtr<mirror::String> str = reinterpret_cast<mirror::String*>(object.Ptr());
        ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
        CHECK(class_linker->GetInternTable()->LookupStrong(Thread::Current(), str) != nullptr);
      }
    }
    gc_roots[i] = GcRoot<mirror::Object>(object);
  }
}

static uint8_t* GetRootTable(const void* code_ptr, uint32_t* number_of_roots = nullptr) {
  OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
  uint8_t* data = method_header->GetOptimizedCodeInfoPtr();
  uint32_t roots = GetNumberOfRoots(data);
  if (number_of_roots != nullptr) {
    *number_of_roots = roots;
  }
  return data - ComputeRootTableSize(roots);
}

// Use a sentinel for marking entries in the JIT table that have been cleared.
// This helps diagnosing in case the compiled code tries to wrongly access such
// entries.
static mirror::Class* const weak_sentinel =
    reinterpret_cast<mirror::Class*>(Context::kBadGprBase + 0xff);

// Helper for the GC to process a weak class in a JIT root table.
static inline void ProcessWeakClass(GcRoot<mirror::Class>* root_ptr,
                                    IsMarkedVisitor* visitor,
                                    mirror::Class* update)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This does not need a read barrier because this is called by GC.
  mirror::Class* cls = root_ptr->Read<kWithoutReadBarrier>();
  if (cls != nullptr && cls != weak_sentinel) {
    DCHECK((cls->IsClass<kDefaultVerifyFlags, kWithoutReadBarrier>()));
    // Look at the classloader of the class to know if it has been unloaded.
    // This does not need a read barrier because this is called by GC.
    mirror::Object* class_loader =
        cls->GetClassLoader<kDefaultVerifyFlags, kWithoutReadBarrier>();
    if (class_loader == nullptr || visitor->IsMarked(class_loader) != nullptr) {
      // The class loader is live, update the entry if the class has moved.
      mirror::Class* new_cls = down_cast<mirror::Class*>(visitor->IsMarked(cls));
      // Note that new_object can be null for CMS and newly allocated objects.
      if (new_cls != nullptr && new_cls != cls) {
        *root_ptr = GcRoot<mirror::Class>(new_cls);
      }
    } else {
      // The class loader is not live, clear the entry.
      *root_ptr = GcRoot<mirror::Class>(update);
    }
  }
}

void JitCodeCache::SweepRootTables(IsMarkedVisitor* visitor) {
  MutexLock mu(Thread::Current(), lock_);
  for (const auto& entry : method_code_map_) {
    uint32_t number_of_roots = 0;
    uint8_t* roots_data = GetRootTable(entry.first, &number_of_roots);
    GcRoot<mirror::Object>* roots = reinterpret_cast<GcRoot<mirror::Object>*>(roots_data);
    for (uint32_t i = 0; i < number_of_roots; ++i) {
      // This does not need a read barrier because this is called by GC.
      mirror::Object* object = roots[i].Read<kWithoutReadBarrier>();
      if (object == nullptr || object == weak_sentinel) {
        // entry got deleted in a previous sweep.
      } else if (object->IsString<kDefaultVerifyFlags, kWithoutReadBarrier>()) {
        mirror::Object* new_object = visitor->IsMarked(object);
        // We know the string is marked because it's a strongly-interned string that
        // is always alive. The IsMarked implementation of the CMS collector returns
        // null for newly allocated objects, but we know those haven't moved. Therefore,
        // only update the entry if we get a different non-null string.
        // TODO: Do not use IsMarked for j.l.Class, and adjust once we move this method
        // out of the weak access/creation pause. b/32167580
        if (new_object != nullptr && new_object != object) {
          DCHECK(new_object->IsString());
          roots[i] = GcRoot<mirror::Object>(new_object);
        }
      } else {
        ProcessWeakClass(
            reinterpret_cast<GcRoot<mirror::Class>*>(&roots[i]), visitor, weak_sentinel);
      }
    }
  }
  // Walk over inline caches to clear entries containing unloaded classes.
  for (ProfilingInfo* info : profiling_infos_) {
    for (size_t i = 0; i < info->number_of_inline_caches_; ++i) {
      InlineCache* cache = &info->cache_[i];
      for (size_t j = 0; j < InlineCache::kIndividualCacheSize; ++j) {
        ProcessWeakClass(&cache->classes_[j], visitor, nullptr);
      }
    }
  }
}

void JitCodeCache::FreeCode(const void* code_ptr) {
  uintptr_t allocation = FromCodeToAllocation(code_ptr);
  // Notify native debugger that we are about to remove the code.
  // It does nothing if we are not using native debugger.
  MutexLock mu(Thread::Current(), *Locks::native_debug_interface_lock_);
  RemoveNativeDebugInfoForJit(code_ptr);
  if (OatQuickMethodHeader::FromCodePointer(code_ptr)->IsOptimized()) {
    FreeData(GetRootTable(code_ptr));
  }  // else this is a JNI stub without any data.
  FreeCode(reinterpret_cast<uint8_t*>(allocation));
}

void JitCodeCache::FreeAllMethodHeaders(
    const std::unordered_set<OatQuickMethodHeader*>& method_headers) {
  {
    MutexLock mu(Thread::Current(), *Locks::cha_lock_);
    Runtime::Current()->GetClassLinker()->GetClassHierarchyAnalysis()
        ->RemoveDependentsWithMethodHeaders(method_headers);
  }

  // We need to remove entries in method_headers from CHA dependencies
  // first since once we do FreeCode() below, the memory can be reused
  // so it's possible for the same method_header to start representing
  // different compile code.
  MutexLock mu(Thread::Current(), lock_);
  ScopedCodeCacheWrite scc(this);
  for (const OatQuickMethodHeader* method_header : method_headers) {
    FreeCode(method_header->GetCode());
  }
}

void JitCodeCache::RemoveMethodsIn(Thread* self, const LinearAlloc& alloc) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  // We use a set to first collect all method_headers whose code need to be
  // removed. We need to free the underlying code after we remove CHA dependencies
  // for entries in this set. And it's more efficient to iterate through
  // the CHA dependency map just once with an unordered_set.
  std::unordered_set<OatQuickMethodHeader*> method_headers;
  {
    MutexLock mu(self, lock_);
    // We do not check if a code cache GC is in progress, as this method comes
    // with the classlinker_classes_lock_ held, and suspending ourselves could
    // lead to a deadlock.
    {
      ScopedCodeCacheWrite scc(this);
      for (auto it = jni_stubs_map_.begin(); it != jni_stubs_map_.end();) {
        it->second.RemoveMethodsIn(alloc);
        if (it->second.GetMethods().empty()) {
          method_headers.insert(OatQuickMethodHeader::FromCodePointer(it->second.GetCode()));
          it = jni_stubs_map_.erase(it);
        } else {
          it->first.UpdateShorty(it->second.GetMethods().front());
          ++it;
        }
      }
      for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
        if (alloc.ContainsUnsafe(it->second)) {
          method_headers.insert(OatQuickMethodHeader::FromCodePointer(it->first));
          it = method_code_map_.erase(it);
        } else {
          ++it;
        }
      }
    }
    for (auto it = osr_code_map_.begin(); it != osr_code_map_.end();) {
      if (alloc.ContainsUnsafe(it->first)) {
        // Note that the code has already been pushed to method_headers in the loop
        // above and is going to be removed in FreeCode() below.
        it = osr_code_map_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = profiling_infos_.begin(); it != profiling_infos_.end();) {
      ProfilingInfo* info = *it;
      if (alloc.ContainsUnsafe(info->GetMethod())) {
        info->GetMethod()->SetProfilingInfo(nullptr);
        FreeData(reinterpret_cast<uint8_t*>(info));
        it = profiling_infos_.erase(it);
      } else {
        ++it;
      }
    }
  }
  FreeAllMethodHeaders(method_headers);
}

bool JitCodeCache::IsWeakAccessEnabled(Thread* self) const {
  return kUseReadBarrier
      ? self->GetWeakRefAccessEnabled()
      : is_weak_access_enabled_.LoadSequentiallyConsistent();
}

void JitCodeCache::WaitUntilInlineCacheAccessible(Thread* self) {
  if (IsWeakAccessEnabled(self)) {
    return;
  }
  ScopedThreadSuspension sts(self, kWaitingWeakGcRootRead);
  MutexLock mu(self, lock_);
  while (!IsWeakAccessEnabled(self)) {
    inline_cache_cond_.Wait(self);
  }
}

void JitCodeCache::BroadcastForInlineCacheAccess() {
  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  inline_cache_cond_.Broadcast(self);
}

void JitCodeCache::AllowInlineCacheAccess() {
  DCHECK(!kUseReadBarrier);
  is_weak_access_enabled_.StoreSequentiallyConsistent(true);
  BroadcastForInlineCacheAccess();
}

void JitCodeCache::DisallowInlineCacheAccess() {
  DCHECK(!kUseReadBarrier);
  is_weak_access_enabled_.StoreSequentiallyConsistent(false);
}

void JitCodeCache::CopyInlineCacheInto(const InlineCache& ic,
                                       Handle<mirror::ObjectArray<mirror::Class>> array) {
  WaitUntilInlineCacheAccessible(Thread::Current());
  // Note that we don't need to lock `lock_` here, the compiler calling
  // this method has already ensured the inline cache will not be deleted.
  for (size_t in_cache = 0, in_array = 0;
       in_cache < InlineCache::kIndividualCacheSize;
       ++in_cache) {
    mirror::Class* object = ic.classes_[in_cache].Read();
    if (object != nullptr) {
      array->Set(in_array++, object);
    }
  }
}

static void ClearMethodCounter(ArtMethod* method, bool was_warm) {
  if (was_warm) {
    // Don't do any read barrier, as the declaring class of `method` may
    // be in the process of being GC'ed (reading the declaring class is done
    // when DCHECKing the declaring class is resolved, which we know it is
    // at this point).
    method->SetPreviouslyWarm<kWithoutReadBarrier>();
  }
  // We reset the counter to 1 so that the profile knows that the method was executed at least once.
  // This is required for layout purposes.
  // We also need to make sure we'll pass the warmup threshold again, so we set to 0 if
  // the warmup threshold is 1.
  uint16_t jit_warmup_threshold = Runtime::Current()->GetJITOptions()->GetWarmupThreshold();
  method->SetCounter(std::min(jit_warmup_threshold - 1, 1));
}

uint8_t* JitCodeCache::CommitCodeInternal(Thread* self,
                                          ArtMethod* method,
                                          uint8_t* stack_map,
                                          uint8_t* method_info,
                                          uint8_t* roots_data,
                                          size_t frame_size_in_bytes,
                                          size_t core_spill_mask,
                                          size_t fp_spill_mask,
                                          const uint8_t* code,
                                          size_t code_size,
                                          size_t data_size,
                                          bool osr,
                                          Handle<mirror::ObjectArray<mirror::Object>> roots,
                                          bool has_should_deoptimize_flag,
                                          const ArenaSet<ArtMethod*>&
                                              cha_single_implementation_list) {
  DCHECK_NE(stack_map != nullptr, method->IsNative());
  DCHECK(!method->IsNative() || !osr);
  size_t alignment = GetInstructionSetAlignment(kRuntimeISA);
  // Ensure the header ends up at expected instruction alignment.
  size_t header_size = RoundUp(sizeof(OatQuickMethodHeader), alignment);
  size_t total_size = header_size + code_size;

  OatQuickMethodHeader* method_header = nullptr;
  uint8_t* code_ptr = nullptr;
  uint8_t* memory = nullptr;
  {
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    WaitForPotentialCollectionToComplete(self);
    {
      ScopedCodeCacheWrite scc(this);
      memory = AllocateCode(total_size);
      if (memory == nullptr) {
        return nullptr;
      }
      code_ptr = memory + header_size;

      std::copy(code, code + code_size, code_ptr);
      method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      new (method_header) OatQuickMethodHeader(
          (stack_map != nullptr) ? code_ptr - stack_map : 0u,
          (method_info != nullptr) ? code_ptr - method_info : 0u,
          frame_size_in_bytes,
          core_spill_mask,
          fp_spill_mask,
          code_size);
      // Flush caches before we remove write permission because some ARMv8 Qualcomm kernels may
      // trigger a segfault if a page fault occurs when requesting a cache maintenance operation.
      // This is a kernel bug that we need to work around until affected devices (e.g. Nexus 5X and
      // 6P) stop being supported or their kernels are fixed.
      //
      // For reference, this behavior is caused by this commit:
      // https://android.googlesource.com/kernel/msm/+/3fbe6bc28a6b9939d0650f2f17eb5216c719950c
      FlushInstructionCache(reinterpret_cast<char*>(code_ptr),
                            reinterpret_cast<char*>(code_ptr + code_size));
      DCHECK(!Runtime::Current()->IsAotCompiler());
      if (has_should_deoptimize_flag) {
        method_header->SetHasShouldDeoptimizeFlag();
      }
    }

    number_of_compilations_++;
  }
  // We need to update the entry point in the runnable state for the instrumentation.
  {
    // Need cha_lock_ for checking all single-implementation flags and register
    // dependencies.
    MutexLock cha_mu(self, *Locks::cha_lock_);
    bool single_impl_still_valid = true;
    for (ArtMethod* single_impl : cha_single_implementation_list) {
      if (!single_impl->HasSingleImplementation()) {
        // Simply discard the compiled code. Clear the counter so that it may be recompiled later.
        // Hopefully the class hierarchy will be more stable when compilation is retried.
        single_impl_still_valid = false;
        ClearMethodCounter(method, /*was_warm*/ false);
        break;
      }
    }

    // Discard the code if any single-implementation assumptions are now invalid.
    if (!single_impl_still_valid) {
      VLOG(jit) << "JIT discarded jitted code due to invalid single-implementation assumptions.";
      return nullptr;
    }
    DCHECK(cha_single_implementation_list.empty() || !Runtime::Current()->IsJavaDebuggable())
        << "Should not be using cha on debuggable apps/runs!";

    for (ArtMethod* single_impl : cha_single_implementation_list) {
      Runtime::Current()->GetClassLinker()->GetClassHierarchyAnalysis()->AddDependency(
          single_impl, method, method_header);
    }

    // The following needs to be guarded by cha_lock_ also. Otherwise it's
    // possible that the compiled code is considered invalidated by some class linking,
    // but below we still make the compiled code valid for the method.
    MutexLock mu(self, lock_);
    if (UNLIKELY(method->IsNative())) {
      DCHECK(stack_map == nullptr);
      DCHECK(roots_data == nullptr);
      auto it = jni_stubs_map_.find(JniStubKey(method));
      DCHECK(it != jni_stubs_map_.end())
          << "Entry inserted in NotifyCompilationOf() should be alive.";
      JniStubData* data = &it->second;
      DCHECK(ContainsElement(data->GetMethods(), method))
          << "Entry inserted in NotifyCompilationOf() should contain this method.";
      data->SetCode(code_ptr);
      instrumentation::Instrumentation* instrum = Runtime::Current()->GetInstrumentation();
      for (ArtMethod* m : data->GetMethods()) {
        instrum->UpdateMethodsCode(m, method_header->GetEntryPoint());
      }
    } else {
      // Fill the root table before updating the entry point.
      DCHECK_EQ(FromStackMapToRoots(stack_map), roots_data);
      DCHECK_LE(roots_data, stack_map);
      FillRootTable(roots_data, roots);
      {
        // Flush data cache, as compiled code references literals in it.
        // We also need a TLB shootdown to act as memory barrier across cores.
        ScopedCodeCacheWrite ccw(this, /* only_for_tlb_shootdown */ true);
        FlushDataCache(reinterpret_cast<char*>(roots_data),
                       reinterpret_cast<char*>(roots_data + data_size));
      }
      method_code_map_.Put(code_ptr, method);
      if (osr) {
        number_of_osr_compilations_++;
        osr_code_map_.Put(method, code_ptr);
      } else {
        Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
            method, method_header->GetEntryPoint());
      }
    }
    if (collection_in_progress_) {
      // We need to update the live bitmap if there is a GC to ensure it sees this new
      // code.
      GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(code_ptr));
    }
    last_update_time_ns_.StoreRelease(NanoTime());
    VLOG(jit)
        << "JIT added (osr=" << std::boolalpha << osr << std::noboolalpha << ") "
        << ArtMethod::PrettyMethod(method) << "@" << method
        << " ccache_size=" << PrettySize(CodeCacheSizeLocked()) << ": "
        << " dcache_size=" << PrettySize(DataCacheSizeLocked()) << ": "
        << reinterpret_cast<const void*>(method_header->GetEntryPoint()) << ","
        << reinterpret_cast<const void*>(method_header->GetEntryPoint() +
                                         method_header->GetCodeSize());
    histogram_code_memory_use_.AddValue(code_size);
    if (code_size > kCodeSizeLogThreshold) {
      LOG(INFO) << "JIT allocated "
                << PrettySize(code_size)
                << " for compiled code of "
                << ArtMethod::PrettyMethod(method);
    }
  }

  return reinterpret_cast<uint8_t*>(method_header);
}

size_t JitCodeCache::CodeCacheSize() {
  MutexLock mu(Thread::Current(), lock_);
  return CodeCacheSizeLocked();
}

bool JitCodeCache::RemoveMethod(ArtMethod* method, bool release_memory) {
  // This function is used only for testing and only with non-native methods.
  CHECK(!method->IsNative());

  MutexLock mu(Thread::Current(), lock_);

  bool osr = osr_code_map_.find(method) != osr_code_map_.end();
  bool in_cache = RemoveMethodLocked(method, release_memory);

  if (!in_cache) {
    return false;
  }

  method->ClearCounter();
  Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
      method, GetQuickToInterpreterBridge());
  VLOG(jit)
      << "JIT removed (osr=" << std::boolalpha << osr << std::noboolalpha << ") "
      << ArtMethod::PrettyMethod(method) << "@" << method
      << " ccache_size=" << PrettySize(CodeCacheSizeLocked()) << ": "
      << " dcache_size=" << PrettySize(DataCacheSizeLocked());
  return true;
}

bool JitCodeCache::RemoveMethodLocked(ArtMethod* method, bool release_memory) {
  if (LIKELY(!method->IsNative())) {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    if (info != nullptr) {
      RemoveElement(profiling_infos_, info);
    }
    method->SetProfilingInfo(nullptr);
  }

  bool in_cache = false;
  ScopedCodeCacheWrite ccw(this);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it != jni_stubs_map_.end() && it->second.RemoveMethod(method)) {
      in_cache = true;
      if (it->second.GetMethods().empty()) {
        if (release_memory) {
          FreeCode(it->second.GetCode());
        }
        jni_stubs_map_.erase(it);
      } else {
        it->first.UpdateShorty(it->second.GetMethods().front());
      }
    }
  } else {
    for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
      if (it->second == method) {
        in_cache = true;
        if (release_memory) {
          FreeCode(it->first);
        }
        it = method_code_map_.erase(it);
      } else {
        ++it;
      }
    }

    auto osr_it = osr_code_map_.find(method);
    if (osr_it != osr_code_map_.end()) {
      osr_code_map_.erase(osr_it);
    }
  }

  return in_cache;
}

// This notifies the code cache that the given method has been redefined and that it should remove
// any cached information it has on the method. All threads must be suspended before calling this
// method. The compiled code for the method (if there is any) must not be in any threads call stack.
void JitCodeCache::NotifyMethodRedefined(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  RemoveMethodLocked(method, /* release_memory */ true);
}

// This invalidates old_method. Once this function returns one can no longer use old_method to
// execute code unless it is fixed up. This fixup will happen later in the process of installing a
// class redefinition.
// TODO We should add some info to ArtMethod to note that 'old_method' has been invalidated and
// shouldn't be used since it is no longer logically in the jit code cache.
// TODO We should add DCHECKS that validate that the JIT is paused when this method is entered.
void JitCodeCache::MoveObsoleteMethod(ArtMethod* old_method, ArtMethod* new_method) {
  MutexLock mu(Thread::Current(), lock_);
  if (old_method->IsNative()) {
    // Update methods in jni_stubs_map_.
    for (auto& entry : jni_stubs_map_) {
      JniStubData& data = entry.second;
      data.MoveObsoleteMethod(old_method, new_method);
    }
    return;
  }
  // Update ProfilingInfo to the new one and remove it from the old_method.
  if (old_method->GetProfilingInfo(kRuntimePointerSize) != nullptr) {
    DCHECK_EQ(old_method->GetProfilingInfo(kRuntimePointerSize)->GetMethod(), old_method);
    ProfilingInfo* info = old_method->GetProfilingInfo(kRuntimePointerSize);
    old_method->SetProfilingInfo(nullptr);
    // Since the JIT should be paused and all threads suspended by the time this is called these
    // checks should always pass.
    DCHECK(!info->IsInUseByCompiler());
    new_method->SetProfilingInfo(info);
    info->method_ = new_method;
  }
  // Update method_code_map_ to point to the new method.
  for (auto& it : method_code_map_) {
    if (it.second == old_method) {
      it.second = new_method;
    }
  }
  // Update osr_code_map_ to point to the new method.
  auto code_map = osr_code_map_.find(old_method);
  if (code_map != osr_code_map_.end()) {
    osr_code_map_.Put(new_method, code_map->second);
    osr_code_map_.erase(old_method);
  }
}

size_t JitCodeCache::CodeCacheSizeLocked() {
  return used_memory_for_code_;
}

size_t JitCodeCache::DataCacheSize() {
  MutexLock mu(Thread::Current(), lock_);
  return DataCacheSizeLocked();
}

size_t JitCodeCache::DataCacheSizeLocked() {
  return used_memory_for_data_;
}

void JitCodeCache::ClearData(Thread* self,
                             uint8_t* stack_map_data,
                             uint8_t* roots_data) {
  DCHECK_EQ(FromStackMapToRoots(stack_map_data), roots_data);
  MutexLock mu(self, lock_);
  FreeData(reinterpret_cast<uint8_t*>(roots_data));
}

size_t JitCodeCache::ReserveData(Thread* self,
                                 size_t stack_map_size,
                                 size_t method_info_size,
                                 size_t number_of_roots,
                                 ArtMethod* method,
                                 uint8_t** stack_map_data,
                                 uint8_t** method_info_data,
                                 uint8_t** roots_data) {
  size_t table_size = ComputeRootTableSize(number_of_roots);
  size_t size = RoundUp(stack_map_size + method_info_size + table_size, sizeof(void*));
  uint8_t* result = nullptr;

  {
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    WaitForPotentialCollectionToComplete(self);
    result = AllocateData(size);
  }

  if (result == nullptr) {
    // Retry.
    GarbageCollectCache(self);
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    WaitForPotentialCollectionToComplete(self);
    result = AllocateData(size);
  }

  MutexLock mu(self, lock_);
  histogram_stack_map_memory_use_.AddValue(size);
  if (size > kStackMapSizeLogThreshold) {
    LOG(INFO) << "JIT allocated "
              << PrettySize(size)
              << " for stack maps of "
              << ArtMethod::PrettyMethod(method);
  }
  if (result != nullptr) {
    *roots_data = result;
    *stack_map_data = result + table_size;
    *method_info_data = *stack_map_data + stack_map_size;
    FillRootTableLength(*roots_data, number_of_roots);
    return size;
  } else {
    *roots_data = nullptr;
    *stack_map_data = nullptr;
    *method_info_data = nullptr;
    return 0;
  }
}

class MarkCodeVisitor FINAL : public StackVisitor {
 public:
  MarkCodeVisitor(Thread* thread_in, JitCodeCache* code_cache_in)
      : StackVisitor(thread_in, nullptr, StackVisitor::StackWalkKind::kSkipInlinedFrames),
        code_cache_(code_cache_in),
        bitmap_(code_cache_->GetLiveBitmap()) {}

  bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
    if (method_header == nullptr) {
      return true;
    }
    const void* code = method_header->GetCode();
    if (code_cache_->ContainsPc(code)) {
      // Use the atomic set version, as multiple threads are executing this code.
      bitmap_->AtomicTestAndSet(FromCodeToAllocation(code));
    }
    return true;
  }

 private:
  JitCodeCache* const code_cache_;
  CodeCacheBitmap* const bitmap_;
};

class MarkCodeClosure FINAL : public Closure {
 public:
  MarkCodeClosure(JitCodeCache* code_cache, Barrier* barrier)
      : code_cache_(code_cache), barrier_(barrier) {}

  void Run(Thread* thread) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ScopedTrace trace(__PRETTY_FUNCTION__);
    DCHECK(thread == Thread::Current() || thread->IsSuspended());
    MarkCodeVisitor visitor(thread, code_cache_);
    visitor.WalkStack();
    if (kIsDebugBuild) {
      // The stack walking code queries the side instrumentation stack if it
      // sees an instrumentation exit pc, so the JIT code of methods in that stack
      // must have been seen. We sanity check this below.
      for (const instrumentation::InstrumentationStackFrame& frame
              : *thread->GetInstrumentationStack()) {
        // The 'method_' in InstrumentationStackFrame is the one that has return_pc_ in
        // its stack frame, it is not the method owning return_pc_. We just pass null to
        // LookupMethodHeader: the method is only checked against in debug builds.
        OatQuickMethodHeader* method_header =
            code_cache_->LookupMethodHeader(frame.return_pc_, /* method */ nullptr);
        if (method_header != nullptr) {
          const void* code = method_header->GetCode();
          CHECK(code_cache_->GetLiveBitmap()->Test(FromCodeToAllocation(code)));
        }
      }
    }
    barrier_->Pass(Thread::Current());
  }

 private:
  JitCodeCache* const code_cache_;
  Barrier* const barrier_;
};

void JitCodeCache::NotifyCollectionDone(Thread* self) {
  collection_in_progress_ = false;
  lock_cond_.Broadcast(self);
}

void JitCodeCache::SetFootprintLimit(size_t new_footprint) {
  size_t per_space_footprint = new_footprint / 2;
  DCHECK(IsAlignedParam(per_space_footprint, kPageSize));
  DCHECK_EQ(per_space_footprint * 2, new_footprint);
  mspace_set_footprint_limit(data_mspace_, per_space_footprint);
  {
    ScopedCodeCacheWrite scc(this);
    mspace_set_footprint_limit(code_mspace_, per_space_footprint);
  }
}

bool JitCodeCache::IncreaseCodeCacheCapacity() {
  if (current_capacity_ == max_capacity_) {
    return false;
  }

  // Double the capacity if we're below 1MB, or increase it by 1MB if
  // we're above.
  if (current_capacity_ < 1 * MB) {
    current_capacity_ *= 2;
  } else {
    current_capacity_ += 1 * MB;
  }
  if (current_capacity_ > max_capacity_) {
    current_capacity_ = max_capacity_;
  }

  VLOG(jit) << "Increasing code cache capacity to " << PrettySize(current_capacity_);

  SetFootprintLimit(current_capacity_);

  return true;
}

void JitCodeCache::MarkCompiledCodeOnThreadStacks(Thread* self) {
  Barrier barrier(0);
  size_t threads_running_checkpoint = 0;
  MarkCodeClosure closure(this, &barrier);
  threads_running_checkpoint = Runtime::Current()->GetThreadList()->RunCheckpoint(&closure);
  // Now that we have run our checkpoint, move to a suspended state and wait
  // for other threads to run the checkpoint.
  ScopedThreadSuspension sts(self, kSuspended);
  if (threads_running_checkpoint != 0) {
    barrier.Increment(self, threads_running_checkpoint);
  }
}

bool JitCodeCache::ShouldDoFullCollection() {
  if (current_capacity_ == max_capacity_) {
    // Always do a full collection when the code cache is full.
    return true;
  } else if (current_capacity_ < kReservedCapacity) {
    // Always do partial collection when the code cache size is below the reserved
    // capacity.
    return false;
  } else if (last_collection_increased_code_cache_) {
    // This time do a full collection.
    return true;
  } else {
    // This time do a partial collection.
    return false;
  }
}

void JitCodeCache::GarbageCollectCache(Thread* self) {
  ScopedTrace trace(__FUNCTION__);
  if (!garbage_collect_code_) {
    MutexLock mu(self, lock_);
    IncreaseCodeCacheCapacity();
    return;
  }

  // Wait for an existing collection, or let everyone know we are starting one.
  {
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    if (WaitForPotentialCollectionToComplete(self)) {
      return;
    } else {
      number_of_collections_++;
      live_bitmap_.reset(CodeCacheBitmap::Create(
          "code-cache-bitmap",
          reinterpret_cast<uintptr_t>(code_map_->Begin()),
          reinterpret_cast<uintptr_t>(code_map_->Begin() + current_capacity_ / 2)));
      collection_in_progress_ = true;
    }
  }

  TimingLogger logger("JIT code cache timing logger", true, VLOG_IS_ON(jit));
  {
    TimingLogger::ScopedTiming st("Code cache collection", &logger);

    bool do_full_collection = false;
    {
      MutexLock mu(self, lock_);
      do_full_collection = ShouldDoFullCollection();
    }

    VLOG(jit) << "Do "
              << (do_full_collection ? "full" : "partial")
              << " code cache collection, code="
              << PrettySize(CodeCacheSize())
              << ", data=" << PrettySize(DataCacheSize());

    DoCollection(self, /* collect_profiling_info */ do_full_collection);

    VLOG(jit) << "After code cache collection, code="
              << PrettySize(CodeCacheSize())
              << ", data=" << PrettySize(DataCacheSize());

    {
      MutexLock mu(self, lock_);

      // Increase the code cache only when we do partial collections.
      // TODO: base this strategy on how full the code cache is?
      if (do_full_collection) {
        last_collection_increased_code_cache_ = false;
      } else {
        last_collection_increased_code_cache_ = true;
        IncreaseCodeCacheCapacity();
      }

      bool next_collection_will_be_full = ShouldDoFullCollection();

      // Start polling the liveness of compiled code to prepare for the next full collection.
      if (next_collection_will_be_full) {
        // Save the entry point of methods we have compiled, and update the entry
        // point of those methods to the interpreter. If the method is invoked, the
        // interpreter will update its entry point to the compiled code and call it.
        for (ProfilingInfo* info : profiling_infos_) {
          const void* entry_point = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
          if (ContainsPc(entry_point)) {
            info->SetSavedEntryPoint(entry_point);
            // Don't call Instrumentation::UpdateMethodsCode(), as it can check the declaring
            // class of the method. We may be concurrently running a GC which makes accessing
            // the class unsafe. We know it is OK to bypass the instrumentation as we've just
            // checked that the current entry point is JIT compiled code.
            info->GetMethod()->SetEntryPointFromQuickCompiledCode(GetQuickToInterpreterBridge());
          }
        }

        DCHECK(CheckLiveCompiledCodeHasProfilingInfo());

        // Change entry points of native methods back to the GenericJNI entrypoint.
        for (const auto& entry : jni_stubs_map_) {
          const JniStubData& data = entry.second;
          if (!data.IsCompiled()) {
            continue;
          }
          // Make sure a single invocation of the GenericJNI trampoline tries to recompile.
          uint16_t new_counter = Runtime::Current()->GetJit()->HotMethodThreshold() - 1u;
          const OatQuickMethodHeader* method_header =
              OatQuickMethodHeader::FromCodePointer(data.GetCode());
          for (ArtMethod* method : data.GetMethods()) {
            if (method->GetEntryPointFromQuickCompiledCode() == method_header->GetEntryPoint()) {
              // Don't call Instrumentation::UpdateMethodsCode(), same as for normal methods above.
              method->SetCounter(new_counter);
              method->SetEntryPointFromQuickCompiledCode(GetQuickGenericJniStub());
            }
          }
        }
      }
      live_bitmap_.reset(nullptr);
      NotifyCollectionDone(self);
    }
  }
  Runtime::Current()->GetJit()->AddTimingLogger(logger);
}

void JitCodeCache::RemoveUnmarkedCode(Thread* self) {
  ScopedTrace trace(__FUNCTION__);
  std::unordered_set<OatQuickMethodHeader*> method_headers;
  {
    MutexLock mu(self, lock_);
    ScopedCodeCacheWrite scc(this);
    // Iterate over all compiled code and remove entries that are not marked.
    for (auto it = jni_stubs_map_.begin(); it != jni_stubs_map_.end();) {
      JniStubData* data = &it->second;
      if (!data->IsCompiled() || GetLiveBitmap()->Test(FromCodeToAllocation(data->GetCode()))) {
        ++it;
      } else {
        method_headers.insert(OatQuickMethodHeader::FromCodePointer(data->GetCode()));
        it = jni_stubs_map_.erase(it);
      }
    }
    for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
      const void* code_ptr = it->first;
      uintptr_t allocation = FromCodeToAllocation(code_ptr);
      if (GetLiveBitmap()->Test(allocation)) {
        ++it;
      } else {
        method_headers.insert(OatQuickMethodHeader::FromCodePointer(code_ptr));
        it = method_code_map_.erase(it);
      }
    }
  }
  FreeAllMethodHeaders(method_headers);
}

void JitCodeCache::DoCollection(Thread* self, bool collect_profiling_info) {
  ScopedTrace trace(__FUNCTION__);
  {
    MutexLock mu(self, lock_);
    if (collect_profiling_info) {
      // Clear the profiling info of methods that do not have compiled code as entrypoint.
      // Also remove the saved entry point from the ProfilingInfo objects.
      for (ProfilingInfo* info : profiling_infos_) {
        const void* ptr = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
        if (!ContainsPc(ptr) && !info->IsInUseByCompiler()) {
          info->GetMethod()->SetProfilingInfo(nullptr);
        }

        if (info->GetSavedEntryPoint() != nullptr) {
          info->SetSavedEntryPoint(nullptr);
          // We are going to move this method back to interpreter. Clear the counter now to
          // give it a chance to be hot again.
          ClearMethodCounter(info->GetMethod(), /*was_warm*/ true);
        }
      }
    } else if (kIsDebugBuild) {
      // Sanity check that the profiling infos do not have a dangling entry point.
      for (ProfilingInfo* info : profiling_infos_) {
        DCHECK(info->GetSavedEntryPoint() == nullptr);
      }
    }

    // Mark compiled code that are entrypoints of ArtMethods. Compiled code that is not
    // an entry point is either:
    // - an osr compiled code, that will be removed if not in a thread call stack.
    // - discarded compiled code, that will be removed if not in a thread call stack.
    for (const auto& entry : jni_stubs_map_) {
      const JniStubData& data = entry.second;
      const void* code_ptr = data.GetCode();
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      for (ArtMethod* method : data.GetMethods()) {
        if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
          GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(code_ptr));
          break;
        }
      }
    }
    for (const auto& it : method_code_map_) {
      ArtMethod* method = it.second;
      const void* code_ptr = it.first;
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
        GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(code_ptr));
      }
    }

    // Empty osr method map, as osr compiled code will be deleted (except the ones
    // on thread stacks).
    osr_code_map_.clear();
  }

  // Run a checkpoint on all threads to mark the JIT compiled code they are running.
  MarkCompiledCodeOnThreadStacks(self);

  // At this point, mutator threads are still running, and entrypoints of methods can
  // change. We do know they cannot change to a code cache entry that is not marked,
  // therefore we can safely remove those entries.
  RemoveUnmarkedCode(self);

  if (collect_profiling_info) {
    MutexLock mu(self, lock_);
    // Free all profiling infos of methods not compiled nor being compiled.
    auto profiling_kept_end = std::remove_if(profiling_infos_.begin(), profiling_infos_.end(),
      [this] (ProfilingInfo* info) NO_THREAD_SAFETY_ANALYSIS {
        const void* ptr = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
        // We have previously cleared the ProfilingInfo pointer in the ArtMethod in the hope
        // that the compiled code would not get revived. As mutator threads run concurrently,
        // they may have revived the compiled code, and now we are in the situation where
        // a method has compiled code but no ProfilingInfo.
        // We make sure compiled methods have a ProfilingInfo object. It is needed for
        // code cache collection.
        if (ContainsPc(ptr) &&
            info->GetMethod()->GetProfilingInfo(kRuntimePointerSize) == nullptr) {
          info->GetMethod()->SetProfilingInfo(info);
        } else if (info->GetMethod()->GetProfilingInfo(kRuntimePointerSize) != info) {
          // No need for this ProfilingInfo object anymore.
          FreeData(reinterpret_cast<uint8_t*>(info));
          return true;
        }
        return false;
      });
    profiling_infos_.erase(profiling_kept_end, profiling_infos_.end());
    DCHECK(CheckLiveCompiledCodeHasProfilingInfo());
  }
}

bool JitCodeCache::CheckLiveCompiledCodeHasProfilingInfo() {
  ScopedTrace trace(__FUNCTION__);
  // Check that methods we have compiled do have a ProfilingInfo object. We would
  // have memory leaks of compiled code otherwise.
  for (const auto& it : method_code_map_) {
    ArtMethod* method = it.second;
    if (method->GetProfilingInfo(kRuntimePointerSize) == nullptr) {
      const void* code_ptr = it.first;
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
        // If the code is not dead, then we have a problem. Note that this can even
        // happen just after a collection, as mutator threads are running in parallel
        // and could deoptimize an existing compiled code.
        return false;
      }
    }
  }
  return true;
}

OatQuickMethodHeader* JitCodeCache::LookupMethodHeader(uintptr_t pc, ArtMethod* method) {
  static_assert(kRuntimeISA != InstructionSet::kThumb2, "kThumb2 cannot be a runtime ISA");
  if (kRuntimeISA == InstructionSet::kArm) {
    // On Thumb-2, the pc is offset by one.
    --pc;
  }
  if (!ContainsPc(reinterpret_cast<const void*>(pc))) {
    return nullptr;
  }

  if (!kIsDebugBuild) {
    // Called with null `method` only from MarkCodeClosure::Run() in debug build.
    CHECK(method != nullptr);
  }

  MutexLock mu(Thread::Current(), lock_);
  OatQuickMethodHeader* method_header = nullptr;
  ArtMethod* found_method = nullptr;  // Only for DCHECK(), not for JNI stubs.
  if (method != nullptr && UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it == jni_stubs_map_.end() || !ContainsElement(it->second.GetMethods(), method)) {
      return nullptr;
    }
    const void* code_ptr = it->second.GetCode();
    method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
    if (!method_header->Contains(pc)) {
      return nullptr;
    }
  } else {
    auto it = method_code_map_.lower_bound(reinterpret_cast<const void*>(pc));
    if (it != method_code_map_.begin()) {
      --it;
      const void* code_ptr = it->first;
      if (OatQuickMethodHeader::FromCodePointer(code_ptr)->Contains(pc)) {
        method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
        found_method = it->second;
      }
    }
    if (method_header == nullptr && method == nullptr) {
      // Scan all compiled JNI stubs as well. This slow search is used only
      // for checks in debug build, for release builds the `method` is not null.
      for (auto&& entry : jni_stubs_map_) {
        const JniStubData& data = entry.second;
        if (data.IsCompiled() &&
            OatQuickMethodHeader::FromCodePointer(data.GetCode())->Contains(pc)) {
          method_header = OatQuickMethodHeader::FromCodePointer(data.GetCode());
        }
      }
    }
    if (method_header == nullptr) {
      return nullptr;
    }
  }

  if (kIsDebugBuild && method != nullptr && !method->IsNative()) {
    // When we are walking the stack to redefine classes and creating obsolete methods it is
    // possible that we might have updated the method_code_map by making this method obsolete in a
    // previous frame. Therefore we should just check that the non-obsolete version of this method
    // is the one we expect. We change to the non-obsolete versions in the error message since the
    // obsolete version of the method might not be fully initialized yet. This situation can only
    // occur when we are in the process of allocating and setting up obsolete methods. Otherwise
    // method and it->second should be identical. (See openjdkjvmti/ti_redefine.cc for more
    // information.)
    DCHECK_EQ(found_method->GetNonObsoleteMethod(), method->GetNonObsoleteMethod())
        << ArtMethod::PrettyMethod(method->GetNonObsoleteMethod()) << " "
        << ArtMethod::PrettyMethod(found_method->GetNonObsoleteMethod()) << " "
        << std::hex << pc;
  }
  return method_header;
}

OatQuickMethodHeader* JitCodeCache::LookupOsrMethodHeader(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  auto it = osr_code_map_.find(method);
  if (it == osr_code_map_.end()) {
    return nullptr;
  }
  return OatQuickMethodHeader::FromCodePointer(it->second);
}

ProfilingInfo* JitCodeCache::AddProfilingInfo(Thread* self,
                                              ArtMethod* method,
                                              const std::vector<uint32_t>& entries,
                                              bool retry_allocation)
    // No thread safety analysis as we are using TryLock/Unlock explicitly.
    NO_THREAD_SAFETY_ANALYSIS {
  ProfilingInfo* info = nullptr;
  if (!retry_allocation) {
    // If we are allocating for the interpreter, just try to lock, to avoid
    // lock contention with the JIT.
    if (lock_.ExclusiveTryLock(self)) {
      info = AddProfilingInfoInternal(self, method, entries);
      lock_.ExclusiveUnlock(self);
    }
  } else {
    {
      MutexLock mu(self, lock_);
      info = AddProfilingInfoInternal(self, method, entries);
    }

    if (info == nullptr) {
      GarbageCollectCache(self);
      MutexLock mu(self, lock_);
      info = AddProfilingInfoInternal(self, method, entries);
    }
  }
  return info;
}

ProfilingInfo* JitCodeCache::AddProfilingInfoInternal(Thread* self ATTRIBUTE_UNUSED,
                                                      ArtMethod* method,
                                                      const std::vector<uint32_t>& entries) {
  size_t profile_info_size = RoundUp(
      sizeof(ProfilingInfo) + sizeof(InlineCache) * entries.size(),
      sizeof(void*));

  // Check whether some other thread has concurrently created it.
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  if (info != nullptr) {
    return info;
  }

  uint8_t* data = AllocateData(profile_info_size);
  if (data == nullptr) {
    return nullptr;
  }
  info = new (data) ProfilingInfo(method, entries);

  // Make sure other threads see the data in the profiling info object before the
  // store in the ArtMethod's ProfilingInfo pointer.
  QuasiAtomic::ThreadFenceRelease();

  method->SetProfilingInfo(info);
  profiling_infos_.push_back(info);
  histogram_profiling_info_memory_use_.AddValue(profile_info_size);
  return info;
}

// NO_THREAD_SAFETY_ANALYSIS as this is called from mspace code, at which point the lock
// is already held.
void* JitCodeCache::MoreCore(const void* mspace, intptr_t increment) NO_THREAD_SAFETY_ANALYSIS {
  if (code_mspace_ == mspace) {
    size_t result = code_end_;
    code_end_ += increment;
    return reinterpret_cast<void*>(result + code_map_->Begin());
  } else {
    DCHECK_EQ(data_mspace_, mspace);
    size_t result = data_end_;
    data_end_ += increment;
    return reinterpret_cast<void*>(result + data_map_->Begin());
  }
}

void JitCodeCache::GetProfiledMethods(const std::set<std::string>& dex_base_locations,
                                      std::vector<ProfileMethodInfo>& methods) {
  ScopedTrace trace(__FUNCTION__);
  MutexLock mu(Thread::Current(), lock_);
  uint16_t jit_compile_threshold = Runtime::Current()->GetJITOptions()->GetCompileThreshold();
  for (const ProfilingInfo* info : profiling_infos_) {
    ArtMethod* method = info->GetMethod();
    const DexFile* dex_file = method->GetDexFile();
    const std::string base_location = DexFileLoader::GetBaseLocation(dex_file->GetLocation());
    if (!ContainsElement(dex_base_locations, base_location)) {
      // Skip dex files which are not profiled.
      continue;
    }
    std::vector<ProfileMethodInfo::ProfileInlineCache> inline_caches;

    // If the method didn't reach the compilation threshold don't save the inline caches.
    // They might be incomplete and cause unnecessary deoptimizations.
    // If the inline cache is empty the compiler will generate a regular invoke virtual/interface.
    if (method->GetCounter() < jit_compile_threshold) {
      methods.emplace_back(/*ProfileMethodInfo*/
          MethodReference(dex_file, method->GetDexMethodIndex()), inline_caches);
      continue;
    }

    for (size_t i = 0; i < info->number_of_inline_caches_; ++i) {
      std::vector<TypeReference> profile_classes;
      const InlineCache& cache = info->cache_[i];
      ArtMethod* caller = info->GetMethod();
      bool is_missing_types = false;
      for (size_t k = 0; k < InlineCache::kIndividualCacheSize; k++) {
        mirror::Class* cls = cache.classes_[k].Read();
        if (cls == nullptr) {
          break;
        }

        // Check if the receiver is in the boot class path or if it's in the
        // same class loader as the caller. If not, skip it, as there is not
        // much we can do during AOT.
        if (!cls->IsBootStrapClassLoaded() &&
            caller->GetClassLoader() != cls->GetClassLoader()) {
          is_missing_types = true;
          continue;
        }

        const DexFile* class_dex_file = nullptr;
        dex::TypeIndex type_index;

        if (cls->GetDexCache() == nullptr) {
          DCHECK(cls->IsArrayClass()) << cls->PrettyClass();
          // Make a best effort to find the type index in the method's dex file.
          // We could search all open dex files but that might turn expensive
          // and probably not worth it.
          class_dex_file = dex_file;
          type_index = cls->FindTypeIndexInOtherDexFile(*dex_file);
        } else {
          class_dex_file = &(cls->GetDexFile());
          type_index = cls->GetDexTypeIndex();
        }
        if (!type_index.IsValid()) {
          // Could be a proxy class or an array for which we couldn't find the type index.
          is_missing_types = true;
          continue;
        }
        if (ContainsElement(dex_base_locations,
                            DexFileLoader::GetBaseLocation(class_dex_file->GetLocation()))) {
          // Only consider classes from the same apk (including multidex).
          profile_classes.emplace_back(/*ProfileMethodInfo::ProfileClassReference*/
              class_dex_file, type_index);
        } else {
          is_missing_types = true;
        }
      }
      if (!profile_classes.empty()) {
        inline_caches.emplace_back(/*ProfileMethodInfo::ProfileInlineCache*/
            cache.dex_pc_, is_missing_types, profile_classes);
      }
    }
    methods.emplace_back(/*ProfileMethodInfo*/
        MethodReference(dex_file, method->GetDexMethodIndex()), inline_caches);
  }
}

uint64_t JitCodeCache::GetLastUpdateTimeNs() const {
  return last_update_time_ns_.LoadAcquire();
}

bool JitCodeCache::IsOsrCompiled(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  return osr_code_map_.find(method) != osr_code_map_.end();
}

bool JitCodeCache::NotifyCompilationOf(ArtMethod* method, Thread* self, bool osr) {
  if (!osr && ContainsPc(method->GetEntryPointFromQuickCompiledCode())) {
    return false;
  }

  MutexLock mu(self, lock_);
  if (osr && (osr_code_map_.find(method) != osr_code_map_.end())) {
    return false;
  }

  if (UNLIKELY(method->IsNative())) {
    JniStubKey key(method);
    auto it = jni_stubs_map_.find(key);
    bool new_compilation = false;
    if (it == jni_stubs_map_.end()) {
      // Create a new entry to mark the stub as being compiled.
      it = jni_stubs_map_.Put(key, JniStubData{});
      new_compilation = true;
    }
    JniStubData* data = &it->second;
    data->AddMethod(method);
    if (data->IsCompiled()) {
      OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(data->GetCode());
      const void* entrypoint = method_header->GetEntryPoint();
      // Update also entrypoints of other methods held by the JniStubData.
      // We could simply update the entrypoint of `method` but if the last JIT GC has
      // changed these entrypoints to GenericJNI in preparation for a full GC, we may
      // as well change them back as this stub shall not be collected anyway and this
      // can avoid a few expensive GenericJNI calls.
      instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
      for (ArtMethod* m : data->GetMethods()) {
        // Call the dedicated method instead of the more generic UpdateMethodsCode, because
        // `m` might be in the process of being deleted.
        instrumentation->UpdateNativeMethodsCodeToJitCode(m, entrypoint);
      }
      if (collection_in_progress_) {
        GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(data->GetCode()));
      }
    }
    return new_compilation;
  } else {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    if (info == nullptr) {
      VLOG(jit) << method->PrettyMethod() << " needs a ProfilingInfo to be compiled";
      // Because the counter is not atomic, there are some rare cases where we may not hit the
      // threshold for creating the ProfilingInfo. Reset the counter now to "correct" this.
      ClearMethodCounter(method, /*was_warm*/ false);
      return false;
    }

    if (info->IsMethodBeingCompiled(osr)) {
      return false;
    }

    info->SetIsMethodBeingCompiled(true, osr);
    return true;
  }
}

ProfilingInfo* JitCodeCache::NotifyCompilerUse(ArtMethod* method, Thread* self) {
  MutexLock mu(self, lock_);
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  if (info != nullptr) {
    if (!info->IncrementInlineUse()) {
      // Overflow of inlining uses, just bail.
      return nullptr;
    }
  }
  return info;
}

void JitCodeCache::DoneCompilerUse(ArtMethod* method, Thread* self) {
  MutexLock mu(self, lock_);
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  DCHECK(info != nullptr);
  info->DecrementInlineUse();
}

void JitCodeCache::DoneCompiling(ArtMethod* method, Thread* self, bool osr) {
  DCHECK_EQ(Thread::Current(), self);
  MutexLock mu(self, lock_);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    DCHECK(it != jni_stubs_map_.end());
    JniStubData* data = &it->second;
    DCHECK(ContainsElement(data->GetMethods(), method));
    if (UNLIKELY(!data->IsCompiled())) {
      // Failed to compile; the JNI compiler never fails, but the cache may be full.
      jni_stubs_map_.erase(it);  // Remove the entry added in NotifyCompilationOf().
    }  // else CommitCodeInternal() updated entrypoints of all methods in the JniStubData.
  } else {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    DCHECK(info->IsMethodBeingCompiled(osr));
    info->SetIsMethodBeingCompiled(false, osr);
  }
}

size_t JitCodeCache::GetMemorySizeOfCodePointer(const void* ptr) {
  MutexLock mu(Thread::Current(), lock_);
  return mspace_usable_size(reinterpret_cast<const void*>(FromCodeToAllocation(ptr)));
}

void JitCodeCache::InvalidateCompiledCodeFor(ArtMethod* method,
                                             const OatQuickMethodHeader* header) {
  DCHECK(!method->IsNative());
  ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
  if ((profiling_info != nullptr) &&
      (profiling_info->GetSavedEntryPoint() == header->GetEntryPoint())) {
    // Prevent future uses of the compiled code.
    profiling_info->SetSavedEntryPoint(nullptr);
  }

  if (method->GetEntryPointFromQuickCompiledCode() == header->GetEntryPoint()) {
    // The entrypoint is the one to invalidate, so we just update it to the interpreter entry point
    // and clear the counter to get the method Jitted again.
    Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
        method, GetQuickToInterpreterBridge());
    ClearMethodCounter(method, /*was_warm*/ profiling_info != nullptr);
  } else {
    MutexLock mu(Thread::Current(), lock_);
    auto it = osr_code_map_.find(method);
    if (it != osr_code_map_.end() && OatQuickMethodHeader::FromCodePointer(it->second) == header) {
      // Remove the OSR method, to avoid using it again.
      osr_code_map_.erase(it);
    }
  }
}

uint8_t* JitCodeCache::AllocateCode(size_t code_size) {
  size_t alignment = GetInstructionSetAlignment(kRuntimeISA);
  uint8_t* result = reinterpret_cast<uint8_t*>(
      mspace_memalign(code_mspace_, alignment, code_size));
  size_t header_size = RoundUp(sizeof(OatQuickMethodHeader), alignment);
  // Ensure the header ends up at expected instruction alignment.
  DCHECK_ALIGNED_PARAM(reinterpret_cast<uintptr_t>(result + header_size), alignment);
  used_memory_for_code_ += mspace_usable_size(result);
  return result;
}

void JitCodeCache::FreeCode(uint8_t* code) {
  used_memory_for_code_ -= mspace_usable_size(code);
  mspace_free(code_mspace_, code);
}

uint8_t* JitCodeCache::AllocateData(size_t data_size) {
  void* result = mspace_malloc(data_mspace_, data_size);
  used_memory_for_data_ += mspace_usable_size(result);
  return reinterpret_cast<uint8_t*>(result);
}

void JitCodeCache::FreeData(uint8_t* data) {
  used_memory_for_data_ -= mspace_usable_size(data);
  mspace_free(data_mspace_, data);
}

void JitCodeCache::Dump(std::ostream& os) {
  MutexLock mu(Thread::Current(), lock_);
  MutexLock mu2(Thread::Current(), *Locks::native_debug_interface_lock_);
  os << "Current JIT code cache size: " << PrettySize(used_memory_for_code_) << "\n"
     << "Current JIT data cache size: " << PrettySize(used_memory_for_data_) << "\n"
     << "Current JIT mini-debug-info size: " << PrettySize(GetJitNativeDebugInfoMemUsage()) << "\n"
     << "Current JIT capacity: " << PrettySize(current_capacity_) << "\n"
     << "Current number of JIT JNI stub entries: " << jni_stubs_map_.size() << "\n"
     << "Current number of JIT code cache entries: " << method_code_map_.size() << "\n"
     << "Total number of JIT compilations: " << number_of_compilations_ << "\n"
     << "Total number of JIT compilations for on stack replacement: "
        << number_of_osr_compilations_ << "\n"
     << "Total number of JIT code cache collections: " << number_of_collections_ << std::endl;
  histogram_stack_map_memory_use_.PrintMemoryUse(os);
  histogram_code_memory_use_.PrintMemoryUse(os);
  histogram_profiling_info_memory_use_.PrintMemoryUse(os);
}

}  // namespace jit
}  // namespace art
