/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "dalvik_system_VMRuntime.h"

#ifdef ART_TARGET_ANDROID
#include <sys/resource.h>
#include <sys/time.h>
extern "C" void android_set_application_target_sdk_version(uint32_t version);
#endif
#include <limits.h>
#include "nativehelper/scoped_utf_chars.h"

#include "android-base/stringprintf.h"

#include "arch/instruction_set.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "debugger.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/allocator/dlmalloc.h"
#include "gc/heap.h"
#include "gc/space/dlmalloc_space.h"
#include "gc/space/image_space.h"
#include "gc/task_processor.h"
#include "intern_table.h"
#include "java_vm_ext.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_local_ref.h"
#include "runtime.h"
#include "scoped_fast_native_object_access-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "thread_list.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

static jfloat VMRuntime_getTargetHeapUtilization(JNIEnv*, jobject) {
  return Runtime::Current()->GetHeap()->GetTargetHeapUtilization();
}

static void VMRuntime_nativeSetTargetHeapUtilization(JNIEnv*, jobject, jfloat target) {
  Runtime::Current()->GetHeap()->SetTargetHeapUtilization(target);
}

static void VMRuntime_startJitCompilation(JNIEnv*, jobject) {
}

static void VMRuntime_disableJitCompilation(JNIEnv*, jobject) {
}

static jboolean VMRuntime_hasUsedHiddenApi(JNIEnv*, jobject) {
  return Runtime::Current()->HasPendingHiddenApiWarning() ? JNI_TRUE : JNI_FALSE;
}

static void VMRuntime_setHiddenApiExemptions(JNIEnv* env,
                                            jclass,
                                            jobjectArray exemptions) {
  std::vector<std::string> exemptions_vec;
  int exemptions_length = env->GetArrayLength(exemptions);
  for (int i = 0; i < exemptions_length; i++) {
    jstring exemption = reinterpret_cast<jstring>(env->GetObjectArrayElement(exemptions, i));
    const char* raw_exemption = env->GetStringUTFChars(exemption, nullptr);
    exemptions_vec.push_back(raw_exemption);
    env->ReleaseStringUTFChars(exemption, raw_exemption);
  }

  Runtime::Current()->SetHiddenApiExemptions(exemptions_vec);
}

static void VMRuntime_setHiddenApiAccessLogSamplingRate(JNIEnv*, jclass, jint rate) {
  Runtime::Current()->SetHiddenApiEventLogSampleRate(rate);
}

static jobject VMRuntime_newNonMovableArray(JNIEnv* env, jobject, jclass javaElementClass,
                                            jint length) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(length < 0)) {
    ThrowNegativeArraySizeException(length);
    return nullptr;
  }
  ObjPtr<mirror::Class> element_class = soa.Decode<mirror::Class>(javaElementClass);
  if (UNLIKELY(element_class == nullptr)) {
    ThrowNullPointerException("element class == null");
    return nullptr;
  }
  Runtime* runtime = Runtime::Current();
  ObjPtr<mirror::Class> array_class =
      runtime->GetClassLinker()->FindArrayClass(soa.Self(), &element_class);
  if (UNLIKELY(array_class == nullptr)) {
    return nullptr;
  }
  gc::AllocatorType allocator = runtime->GetHeap()->GetCurrentNonMovingAllocator();
  ObjPtr<mirror::Array> result = mirror::Array::Alloc<true>(soa.Self(),
                                                            array_class,
                                                            length,
                                                            array_class->GetComponentSizeShift(),
                                                            allocator);
  return soa.AddLocalReference<jobject>(result);
}

static jobject VMRuntime_newUnpaddedArray(JNIEnv* env, jobject, jclass javaElementClass,
                                          jint length) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(length < 0)) {
    ThrowNegativeArraySizeException(length);
    return nullptr;
  }
  ObjPtr<mirror::Class> element_class = soa.Decode<mirror::Class>(javaElementClass);
  if (UNLIKELY(element_class == nullptr)) {
    ThrowNullPointerException("element class == null");
    return nullptr;
  }
  Runtime* runtime = Runtime::Current();
  ObjPtr<mirror::Class> array_class = runtime->GetClassLinker()->FindArrayClass(soa.Self(),
                                                                                &element_class);
  if (UNLIKELY(array_class == nullptr)) {
    return nullptr;
  }
  gc::AllocatorType allocator = runtime->GetHeap()->GetCurrentAllocator();
  ObjPtr<mirror::Array> result = mirror::Array::Alloc<true, true>(
      soa.Self(),
      array_class,
      length,
      array_class->GetComponentSizeShift(),
      allocator);
  return soa.AddLocalReference<jobject>(result);
}

static jlong VMRuntime_addressOf(JNIEnv* env, jobject, jobject javaArray) {
  if (javaArray == nullptr) {  // Most likely allocation failed
    return 0;
  }
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::Array> array = soa.Decode<mirror::Array>(javaArray);
  if (!array->IsArrayInstance()) {
    ThrowIllegalArgumentException("not an array");
    return 0;
  }
  if (Runtime::Current()->GetHeap()->IsMovableObject(array)) {
    ThrowRuntimeException("Trying to get address of movable array object");
    return 0;
  }
  return reinterpret_cast<uintptr_t>(array->GetRawData(array->GetClass()->GetComponentSize(), 0));
}

static void VMRuntime_clearGrowthLimit(JNIEnv*, jobject) {
  Runtime::Current()->GetHeap()->ClearGrowthLimit();
}

static void VMRuntime_clampGrowthLimit(JNIEnv*, jobject) {
  Runtime::Current()->GetHeap()->ClampGrowthLimit();
}

static jboolean VMRuntime_isDebuggerActive(JNIEnv*, jobject) {
  return Dbg::IsDebuggerActive();
}

static jboolean VMRuntime_isNativeDebuggable(JNIEnv*, jobject) {
  return Runtime::Current()->IsNativeDebuggable();
}

static jboolean VMRuntime_isJavaDebuggable(JNIEnv*, jobject) {
  return Runtime::Current()->IsJavaDebuggable();
}

static jobjectArray VMRuntime_properties(JNIEnv* env, jobject) {
  DCHECK(WellKnownClasses::java_lang_String != nullptr);

  const std::vector<std::string>& properties = Runtime::Current()->GetProperties();
  ScopedLocalRef<jobjectArray> ret(env,
                                   env->NewObjectArray(static_cast<jsize>(properties.size()),
                                                       WellKnownClasses::java_lang_String,
                                                       nullptr /* initial element */));
  if (ret == nullptr) {
    DCHECK(env->ExceptionCheck());
    return nullptr;
  }
  for (size_t i = 0; i != properties.size(); ++i) {
    ScopedLocalRef<jstring> str(env, env->NewStringUTF(properties[i].c_str()));
    if (str == nullptr) {
      DCHECK(env->ExceptionCheck());
      return nullptr;
    }
    env->SetObjectArrayElement(ret.get(), static_cast<jsize>(i), str.get());
    DCHECK(!env->ExceptionCheck());
  }
  return ret.release();
}

// This is for backward compatibility with dalvik which returned the
// meaningless "." when no boot classpath or classpath was
// specified. Unfortunately, some tests were using java.class.path to
// lookup relative file locations, so they are counting on this to be
// ".", presumably some applications or libraries could have as well.
static const char* DefaultToDot(const std::string& class_path) {
  return class_path.empty() ? "." : class_path.c_str();
}

static jstring VMRuntime_bootClassPath(JNIEnv* env, jobject) {
  return env->NewStringUTF(DefaultToDot(Runtime::Current()->GetBootClassPathString()));
}

static jstring VMRuntime_classPath(JNIEnv* env, jobject) {
  return env->NewStringUTF(DefaultToDot(Runtime::Current()->GetClassPathString()));
}

static jstring VMRuntime_vmVersion(JNIEnv* env, jobject) {
  return env->NewStringUTF(Runtime::GetVersion());
}

static jstring VMRuntime_vmLibrary(JNIEnv* env, jobject) {
  return env->NewStringUTF(kIsDebugBuild ? "libartd.so" : "libart.so");
}

static jstring VMRuntime_vmInstructionSet(JNIEnv* env, jobject) {
  InstructionSet isa = Runtime::Current()->GetInstructionSet();
  const char* isa_string = GetInstructionSetString(isa);
  return env->NewStringUTF(isa_string);
}

static jboolean VMRuntime_is64Bit(JNIEnv*, jobject) {
  bool is64BitMode = (sizeof(void*) == sizeof(uint64_t));
  return is64BitMode ? JNI_TRUE : JNI_FALSE;
}

static jboolean VMRuntime_isCheckJniEnabled(JNIEnv* env, jobject) {
  return down_cast<JNIEnvExt*>(env)->GetVm()->IsCheckJniEnabled() ? JNI_TRUE : JNI_FALSE;
}

static void VMRuntime_setTargetSdkVersionNative(JNIEnv*, jobject, jint target_sdk_version) {
  // This is the target SDK version of the app we're about to run. It is intended that this a place
  // where workarounds can be enabled.
  // Note that targetSdkVersion may be CUR_DEVELOPMENT (10000).
  // Note that targetSdkVersion may be 0, meaning "current".
  Runtime::Current()->SetTargetSdkVersion(target_sdk_version);

#ifdef ART_TARGET_ANDROID
  // This part is letting libc/dynamic linker know about current app's
  // target sdk version to enable compatibility workarounds.
  android_set_application_target_sdk_version(static_cast<uint32_t>(target_sdk_version));
#endif
}

static void VMRuntime_registerNativeAllocation(JNIEnv* env, jobject, jint bytes) {
  if (UNLIKELY(bytes < 0)) {
    ScopedObjectAccess soa(env);
    ThrowRuntimeException("allocation size negative %d", bytes);
    return;
  }
  Runtime::Current()->GetHeap()->RegisterNativeAllocation(env, static_cast<size_t>(bytes));
}

static void VMRuntime_registerSensitiveThread(JNIEnv*, jobject) {
  Runtime::Current()->RegisterSensitiveThread();
}

static void VMRuntime_registerNativeFree(JNIEnv* env, jobject, jint bytes) {
  if (UNLIKELY(bytes < 0)) {
    ScopedObjectAccess soa(env);
    ThrowRuntimeException("allocation size negative %d", bytes);
    return;
  }
  Runtime::Current()->GetHeap()->RegisterNativeFree(env, static_cast<size_t>(bytes));
}

static void VMRuntime_updateProcessState(JNIEnv*, jobject, jint process_state) {
  Runtime* runtime = Runtime::Current();
  runtime->UpdateProcessState(static_cast<ProcessState>(process_state));
}

static void VMRuntime_trimHeap(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->Trim(ThreadForEnv(env));
}

static void VMRuntime_concurrentGC(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->ConcurrentGC(ThreadForEnv(env), gc::kGcCauseBackground, true);
}

static void VMRuntime_requestHeapTrim(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->RequestTrim(ThreadForEnv(env));
}

static void VMRuntime_requestConcurrentGC(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->RequestConcurrentGC(ThreadForEnv(env),
                                                     gc::kGcCauseBackground,
                                                     true);
}

static void VMRuntime_startHeapTaskProcessor(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->Start(ThreadForEnv(env));
}

static void VMRuntime_stopHeapTaskProcessor(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->Stop(ThreadForEnv(env));
}

static void VMRuntime_runHeapTasks(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->RunAllTasks(ThreadForEnv(env));
}

typedef std::map<std::string, ObjPtr<mirror::String>> StringTable;

class PreloadDexCachesStringsVisitor : public SingleRootVisitor {
 public:
  explicit PreloadDexCachesStringsVisitor(StringTable* table) : table_(table) { }

  void VisitRoot(mirror::Object* root, const RootInfo& info ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::String> string = root->AsString();
    table_->operator[](string->ToModifiedUtf8()) = string;
  }

 private:
  StringTable* const table_;
};

// Based on ClassLinker::ResolveString.
static void PreloadDexCachesResolveString(
    ObjPtr<mirror::DexCache> dex_cache, dex::StringIndex string_idx, StringTable& strings)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t slot_idx = dex_cache->StringSlotIndex(string_idx);
  auto pair = dex_cache->GetStrings()[slot_idx].load(std::memory_order_relaxed);
  if (!pair.object.IsNull()) {
    return;  // The entry already contains some String.
  }
  const DexFile* dex_file = dex_cache->GetDexFile();
  const char* utf8 = dex_file->StringDataByIdx(string_idx);
  ObjPtr<mirror::String> string = strings[utf8];
  if (string == nullptr) {
    return;
  }
  // LOG(INFO) << "VMRuntime.preloadDexCaches resolved string=" << utf8;
  dex_cache->SetResolvedString(string_idx, string);
}

// Based on ClassLinker::ResolveType.
static void PreloadDexCachesResolveType(Thread* self,
                                        ObjPtr<mirror::DexCache> dex_cache,
                                        dex::TypeIndex type_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t slot_idx = dex_cache->TypeSlotIndex(type_idx);
  auto pair = dex_cache->GetResolvedTypes()[slot_idx].load(std::memory_order_relaxed);
  if (!pair.object.IsNull()) {
    return;  // The entry already contains some Class.
  }
  const DexFile* dex_file = dex_cache->GetDexFile();
  const char* class_name = dex_file->StringByTypeIdx(type_idx);
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::Class> klass = (class_name[1] == '\0')
      ? linker->FindPrimitiveClass(class_name[0])
      : linker->LookupClass(self, class_name, nullptr);
  if (klass == nullptr) {
    return;
  }
  // LOG(INFO) << "VMRuntime.preloadDexCaches resolved klass=" << class_name;
  dex_cache->SetResolvedType(type_idx, klass);
  // Skip uninitialized classes because filled static storage entry implies it is initialized.
  if (!klass->IsInitialized()) {
    // LOG(INFO) << "VMRuntime.preloadDexCaches uninitialized klass=" << class_name;
    return;
  }
  // LOG(INFO) << "VMRuntime.preloadDexCaches static storage klass=" << class_name;
}

// Based on ClassLinker::ResolveField.
static void PreloadDexCachesResolveField(ObjPtr<mirror::DexCache> dex_cache,
                                         uint32_t field_idx,
                                         bool is_static)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t slot_idx = dex_cache->FieldSlotIndex(field_idx);
  auto pair = mirror::DexCache::GetNativePairPtrSize(dex_cache->GetResolvedFields(),
                                                     slot_idx,
                                                     kRuntimePointerSize);
  if (pair.object != nullptr) {
    return;  // The entry already contains some ArtField.
  }
  const DexFile* dex_file = dex_cache->GetDexFile();
  const DexFile::FieldId& field_id = dex_file->GetFieldId(field_idx);
  ObjPtr<mirror::Class> klass = Runtime::Current()->GetClassLinker()->LookupResolvedType(
      field_id.class_idx_, dex_cache, /* class_loader */ nullptr);
  if (klass == nullptr) {
    return;
  }
  ArtField* field = is_static
      ? mirror::Class::FindStaticField(Thread::Current(), klass, dex_cache, field_idx)
      : klass->FindInstanceField(dex_cache, field_idx);
  if (field == nullptr) {
    return;
  }
  dex_cache->SetResolvedField(field_idx, field, kRuntimePointerSize);
}

// Based on ClassLinker::ResolveMethod.
static void PreloadDexCachesResolveMethod(ObjPtr<mirror::DexCache> dex_cache, uint32_t method_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  uint32_t slot_idx = dex_cache->MethodSlotIndex(method_idx);
  auto pair = mirror::DexCache::GetNativePairPtrSize(dex_cache->GetResolvedMethods(),
                                                     slot_idx,
                                                     kRuntimePointerSize);
  if (pair.object != nullptr) {
    return;  // The entry already contains some ArtMethod.
  }
  const DexFile* dex_file = dex_cache->GetDexFile();
  const DexFile::MethodId& method_id = dex_file->GetMethodId(method_idx);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();

  ObjPtr<mirror::Class> klass = class_linker->LookupResolvedType(
      method_id.class_idx_, dex_cache, /* class_loader */ nullptr);
  if (klass == nullptr) {
    return;
  }
  // Call FindResolvedMethod to populate the dex cache.
  class_linker->FindResolvedMethod(klass, dex_cache, /* class_loader */ nullptr, method_idx);
}

struct DexCacheStats {
    uint32_t num_strings;
    uint32_t num_types;
    uint32_t num_fields;
    uint32_t num_methods;
    DexCacheStats() : num_strings(0),
                      num_types(0),
                      num_fields(0),
                      num_methods(0) {}
};

static const bool kPreloadDexCachesEnabled = true;

// Disabled because it takes a long time (extra half second) but
// gives almost no benefit in terms of saving private dirty pages.
static const bool kPreloadDexCachesStrings = false;

static const bool kPreloadDexCachesTypes = true;
static const bool kPreloadDexCachesFieldsAndMethods = true;

static const bool kPreloadDexCachesCollectStats = true;

static void PreloadDexCachesStatsTotal(DexCacheStats* total) {
  if (!kPreloadDexCachesCollectStats) {
    return;
  }

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  const std::vector<const DexFile*>& boot_class_path = linker->GetBootClassPath();
  for (size_t i = 0; i< boot_class_path.size(); i++) {
    const DexFile* dex_file = boot_class_path[i];
    CHECK(dex_file != nullptr);
    total->num_strings += dex_file->NumStringIds();
    total->num_fields += dex_file->NumFieldIds();
    total->num_methods += dex_file->NumMethodIds();
    total->num_types += dex_file->NumTypeIds();
  }
}

static void PreloadDexCachesStatsFilled(DexCacheStats* filled)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!kPreloadDexCachesCollectStats) {
    return;
  }
  // TODO: Update for hash-based DexCache arrays.
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  for (const DexFile* dex_file : class_linker->GetBootClassPath()) {
    CHECK(dex_file != nullptr);
    // In fallback mode, not all boot classpath components might be registered, yet.
    if (!class_linker->IsDexFileRegistered(self, *dex_file)) {
      continue;
    }
    ObjPtr<mirror::DexCache> const dex_cache = class_linker->FindDexCache(self, *dex_file);
    DCHECK(dex_cache != nullptr);  // Boot class path dex caches are never unloaded.
    for (size_t j = 0, num_strings = dex_cache->NumStrings(); j < num_strings; ++j) {
      auto pair = dex_cache->GetStrings()[j].load(std::memory_order_relaxed);
      if (!pair.object.IsNull()) {
        filled->num_strings++;
      }
    }
    for (size_t j = 0, num_types = dex_cache->NumResolvedTypes(); j < num_types; ++j) {
      auto pair = dex_cache->GetResolvedTypes()[j].load(std::memory_order_relaxed);
      if (!pair.object.IsNull()) {
        filled->num_types++;
      }
    }
    for (size_t j = 0, num_fields = dex_cache->NumResolvedFields(); j < num_fields; ++j) {
      auto pair = mirror::DexCache::GetNativePairPtrSize(dex_cache->GetResolvedFields(),
                                                         j,
                                                         kRuntimePointerSize);
      if (pair.object != nullptr) {
        filled->num_fields++;
      }
    }
    for (size_t j = 0, num_methods = dex_cache->NumResolvedMethods(); j < num_methods; ++j) {
      auto pair = mirror::DexCache::GetNativePairPtrSize(dex_cache->GetResolvedMethods(),
                                                         j,
                                                         kRuntimePointerSize);
      if (pair.object != nullptr) {
        filled->num_methods++;
      }
    }
  }
}

// TODO: http://b/11309598 This code was ported over based on the
// Dalvik version. However, ART has similar code in other places such
// as the CompilerDriver. This code could probably be refactored to
// serve both uses.
static void VMRuntime_preloadDexCaches(JNIEnv* env, jobject) {
  if (!kPreloadDexCachesEnabled) {
    return;
  }

  ScopedObjectAccess soa(env);

  DexCacheStats total;
  DexCacheStats before;
  if (kPreloadDexCachesCollectStats) {
    LOG(INFO) << "VMRuntime.preloadDexCaches starting";
    PreloadDexCachesStatsTotal(&total);
    PreloadDexCachesStatsFilled(&before);
  }

  Runtime* runtime = Runtime::Current();
  ClassLinker* linker = runtime->GetClassLinker();

  // We use a std::map to avoid heap allocating StringObjects to lookup in gDvm.literalStrings
  StringTable strings;
  if (kPreloadDexCachesStrings) {
    PreloadDexCachesStringsVisitor visitor(&strings);
    runtime->GetInternTable()->VisitRoots(&visitor, kVisitRootFlagAllRoots);
  }

  const std::vector<const DexFile*>& boot_class_path = linker->GetBootClassPath();
  for (size_t i = 0; i < boot_class_path.size(); i++) {
    const DexFile* dex_file = boot_class_path[i];
    CHECK(dex_file != nullptr);
    ObjPtr<mirror::DexCache> dex_cache = linker->RegisterDexFile(*dex_file, nullptr);
    CHECK(dex_cache != nullptr);  // Boot class path dex caches are never unloaded.
    if (kPreloadDexCachesStrings) {
      for (size_t j = 0; j < dex_cache->NumStrings(); j++) {
        PreloadDexCachesResolveString(dex_cache, dex::StringIndex(j), strings);
      }
    }

    if (kPreloadDexCachesTypes) {
      for (size_t j = 0; j < dex_cache->NumResolvedTypes(); j++) {
        PreloadDexCachesResolveType(soa.Self(), dex_cache, dex::TypeIndex(j));
      }
    }

    if (kPreloadDexCachesFieldsAndMethods) {
      for (size_t class_def_index = 0;
           class_def_index < dex_file->NumClassDefs();
           class_def_index++) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
        const uint8_t* class_data = dex_file->GetClassData(class_def);
        if (class_data == nullptr) {
          continue;
        }
        ClassDataItemIterator it(*dex_file, class_data);
        for (; it.HasNextStaticField(); it.Next()) {
          uint32_t field_idx = it.GetMemberIndex();
          PreloadDexCachesResolveField(dex_cache, field_idx, true);
        }
        for (; it.HasNextInstanceField(); it.Next()) {
          uint32_t field_idx = it.GetMemberIndex();
          PreloadDexCachesResolveField(dex_cache, field_idx, false);
        }
        for (; it.HasNextDirectMethod(); it.Next()) {
          uint32_t method_idx = it.GetMemberIndex();
          PreloadDexCachesResolveMethod(dex_cache, method_idx);
        }
        for (; it.HasNextVirtualMethod(); it.Next()) {
          uint32_t method_idx = it.GetMemberIndex();
          PreloadDexCachesResolveMethod(dex_cache, method_idx);
        }
      }
    }
  }

  if (kPreloadDexCachesCollectStats) {
    DexCacheStats after;
    PreloadDexCachesStatsFilled(&after);
    LOG(INFO) << StringPrintf("VMRuntime.preloadDexCaches strings total=%d before=%d after=%d",
                              total.num_strings, before.num_strings, after.num_strings);
    LOG(INFO) << StringPrintf("VMRuntime.preloadDexCaches types total=%d before=%d after=%d",
                              total.num_types, before.num_types, after.num_types);
    LOG(INFO) << StringPrintf("VMRuntime.preloadDexCaches fields total=%d before=%d after=%d",
                              total.num_fields, before.num_fields, after.num_fields);
    LOG(INFO) << StringPrintf("VMRuntime.preloadDexCaches methods total=%d before=%d after=%d",
                              total.num_methods, before.num_methods, after.num_methods);
    LOG(INFO) << StringPrintf("VMRuntime.preloadDexCaches finished");
  }
}


/*
 * This is called by the framework when it knows the application directory and
 * process name.
 */
static void VMRuntime_registerAppInfo(JNIEnv* env,
                                      jclass clazz ATTRIBUTE_UNUSED,
                                      jstring profile_file,
                                      jobjectArray code_paths) {
  std::vector<std::string> code_paths_vec;
  int code_paths_length = env->GetArrayLength(code_paths);
  for (int i = 0; i < code_paths_length; i++) {
    jstring code_path = reinterpret_cast<jstring>(env->GetObjectArrayElement(code_paths, i));
    const char* raw_code_path = env->GetStringUTFChars(code_path, nullptr);
    code_paths_vec.push_back(raw_code_path);
    env->ReleaseStringUTFChars(code_path, raw_code_path);
  }

  const char* raw_profile_file = env->GetStringUTFChars(profile_file, nullptr);
  std::string profile_file_str(raw_profile_file);
  env->ReleaseStringUTFChars(profile_file, raw_profile_file);

  Runtime::Current()->RegisterAppInfo(code_paths_vec, profile_file_str);
}

static jboolean VMRuntime_isBootClassPathOnDisk(JNIEnv* env, jclass, jstring java_instruction_set) {
  ScopedUtfChars instruction_set(env, java_instruction_set);
  if (instruction_set.c_str() == nullptr) {
    return JNI_FALSE;
  }
  InstructionSet isa = GetInstructionSetFromString(instruction_set.c_str());
  if (isa == InstructionSet::kNone) {
    ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
    std::string message(StringPrintf("Instruction set %s is invalid.", instruction_set.c_str()));
    env->ThrowNew(iae.get(), message.c_str());
    return JNI_FALSE;
  }
  std::string error_msg;
  std::unique_ptr<ImageHeader> image_header(gc::space::ImageSpace::ReadImageHeader(
      Runtime::Current()->GetImageLocation().c_str(), isa, &error_msg));
  return image_header.get() != nullptr;
}

static jstring VMRuntime_getCurrentInstructionSet(JNIEnv* env, jclass) {
  return env->NewStringUTF(GetInstructionSetString(kRuntimeISA));
}

static jboolean VMRuntime_didPruneDalvikCache(JNIEnv* env ATTRIBUTE_UNUSED,
                                              jclass klass ATTRIBUTE_UNUSED) {
  return Runtime::Current()->GetPrunedDalvikCache() ? JNI_TRUE : JNI_FALSE;
}

static void VMRuntime_setSystemDaemonThreadPriority(JNIEnv* env ATTRIBUTE_UNUSED,
                                                    jclass klass ATTRIBUTE_UNUSED) {
#ifdef ART_TARGET_ANDROID
  Thread* self = Thread::Current();
  DCHECK(self != nullptr);
  pid_t tid = self->GetTid();
  // We use a priority lower than the default for the system daemon threads (eg HeapTaskDaemon) to
  // avoid jank due to CPU contentions between GC and other UI-related threads. b/36631902.
  // We may use a native priority that doesn't have a corresponding java.lang.Thread-level priority.
  static constexpr int kSystemDaemonNiceValue = 4;  // priority 124
  if (setpriority(PRIO_PROCESS, tid, kSystemDaemonNiceValue) != 0) {
    PLOG(INFO) << *self << " setpriority(PRIO_PROCESS, " << tid << ", "
               << kSystemDaemonNiceValue << ") failed";
  }
#endif
}

static void VMRuntime_setDedupeHiddenApiWarnings(JNIEnv* env ATTRIBUTE_UNUSED,
                                                 jclass klass ATTRIBUTE_UNUSED,
                                                 jboolean dedupe) {
  Runtime::Current()->SetDedupeHiddenApiWarnings(dedupe);
}

static void VMRuntime_setProcessPackageName(JNIEnv* env,
                                            jclass klass ATTRIBUTE_UNUSED,
                                            jstring java_package_name) {
  ScopedUtfChars package_name(env, java_package_name);
  Runtime::Current()->SetProcessPackageName(package_name.c_str());
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(VMRuntime, addressOf, "(Ljava/lang/Object;)J"),
  NATIVE_METHOD(VMRuntime, bootClassPath, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, clampGrowthLimit, "()V"),
  NATIVE_METHOD(VMRuntime, classPath, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, clearGrowthLimit, "()V"),
  NATIVE_METHOD(VMRuntime, concurrentGC, "()V"),
  NATIVE_METHOD(VMRuntime, disableJitCompilation, "()V"),
  NATIVE_METHOD(VMRuntime, hasUsedHiddenApi, "()Z"),
  NATIVE_METHOD(VMRuntime, setHiddenApiExemptions, "([Ljava/lang/String;)V"),
  NATIVE_METHOD(VMRuntime, setHiddenApiAccessLogSamplingRate, "(I)V"),
  NATIVE_METHOD(VMRuntime, getTargetHeapUtilization, "()F"),
  FAST_NATIVE_METHOD(VMRuntime, isDebuggerActive, "()Z"),
  FAST_NATIVE_METHOD(VMRuntime, isNativeDebuggable, "()Z"),
  NATIVE_METHOD(VMRuntime, isJavaDebuggable, "()Z"),
  NATIVE_METHOD(VMRuntime, nativeSetTargetHeapUtilization, "(F)V"),
  FAST_NATIVE_METHOD(VMRuntime, newNonMovableArray, "(Ljava/lang/Class;I)Ljava/lang/Object;"),
  FAST_NATIVE_METHOD(VMRuntime, newUnpaddedArray, "(Ljava/lang/Class;I)Ljava/lang/Object;"),
  NATIVE_METHOD(VMRuntime, properties, "()[Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, setTargetSdkVersionNative, "(I)V"),
  NATIVE_METHOD(VMRuntime, registerNativeAllocation, "(I)V"),
  NATIVE_METHOD(VMRuntime, registerSensitiveThread, "()V"),
  NATIVE_METHOD(VMRuntime, registerNativeFree, "(I)V"),
  NATIVE_METHOD(VMRuntime, requestConcurrentGC, "()V"),
  NATIVE_METHOD(VMRuntime, requestHeapTrim, "()V"),
  NATIVE_METHOD(VMRuntime, runHeapTasks, "()V"),
  NATIVE_METHOD(VMRuntime, updateProcessState, "(I)V"),
  NATIVE_METHOD(VMRuntime, startHeapTaskProcessor, "()V"),
  NATIVE_METHOD(VMRuntime, startJitCompilation, "()V"),
  NATIVE_METHOD(VMRuntime, stopHeapTaskProcessor, "()V"),
  NATIVE_METHOD(VMRuntime, trimHeap, "()V"),
  NATIVE_METHOD(VMRuntime, vmVersion, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, vmLibrary, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, vmInstructionSet, "()Ljava/lang/String;"),
  FAST_NATIVE_METHOD(VMRuntime, is64Bit, "()Z"),
  FAST_NATIVE_METHOD(VMRuntime, isCheckJniEnabled, "()Z"),
  NATIVE_METHOD(VMRuntime, preloadDexCaches, "()V"),
  NATIVE_METHOD(VMRuntime, registerAppInfo, "(Ljava/lang/String;[Ljava/lang/String;)V"),
  NATIVE_METHOD(VMRuntime, isBootClassPathOnDisk, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(VMRuntime, getCurrentInstructionSet, "()Ljava/lang/String;"),
  NATIVE_METHOD(VMRuntime, didPruneDalvikCache, "()Z"),
  NATIVE_METHOD(VMRuntime, setSystemDaemonThreadPriority, "()V"),
  NATIVE_METHOD(VMRuntime, setDedupeHiddenApiWarnings, "(Z)V"),
  NATIVE_METHOD(VMRuntime, setProcessPackageName, "(Ljava/lang/String;)V"),
};

void register_dalvik_system_VMRuntime(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/VMRuntime");
}

}  // namespace art
