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

#include "dalvik_system_VMDebug.h"

#include <string.h>
#include <unistd.h>

#include <sstream>

#include "nativehelper/jni_macros.h"

#include "base/histogram-inl.h"
#include "base/time_utils.h"
#include "class_linker.h"
#include "common_throws.h"
#include "debugger.h"
#include "gc/space/bump_pointer_space.h"
#include "gc/space/dlmalloc_space.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space-inl.h"
#include "gc/space/zygote_space.h"
#include "handle_scope-inl.h"
#include "hprof/hprof.h"
#include "java_vm_ext.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "mirror/object_array-inl.h"
#include "native_util.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"
#include "scoped_fast_native_object_access-inl.h"
#include "trace.h"
#include "well_known_classes.h"

namespace art {

static jobjectArray VMDebug_getVmFeatureList(JNIEnv* env, jclass) {
  static const char* features[] = {
    "method-trace-profiling",
    "method-trace-profiling-streaming",
    "method-sample-profiling",
    "hprof-heap-dump",
    "hprof-heap-dump-streaming",
  };
  jobjectArray result = env->NewObjectArray(arraysize(features),
                                            WellKnownClasses::java_lang_String,
                                            nullptr);
  if (result != nullptr) {
    for (size_t i = 0; i < arraysize(features); ++i) {
      ScopedLocalRef<jstring> jfeature(env, env->NewStringUTF(features[i]));
      if (jfeature.get() == nullptr) {
        return nullptr;
      }
      env->SetObjectArrayElement(result, i, jfeature.get());
    }
  }
  return result;
}

static void VMDebug_startAllocCounting(JNIEnv*, jclass) {
  Runtime::Current()->SetStatsEnabled(true);
}

static void VMDebug_stopAllocCounting(JNIEnv*, jclass) {
  Runtime::Current()->SetStatsEnabled(false);
}

static jint VMDebug_getAllocCount(JNIEnv*, jclass, jint kind) {
  return Runtime::Current()->GetStat(kind);
}

static void VMDebug_resetAllocCount(JNIEnv*, jclass, jint kinds) {
  Runtime::Current()->ResetStats(kinds);
}

static void VMDebug_startMethodTracingDdmsImpl(JNIEnv*, jclass, jint bufferSize, jint flags,
                                               jboolean samplingEnabled, jint intervalUs) {
  Trace::Start("[DDMS]", -1, bufferSize, flags, Trace::TraceOutputMode::kDDMS,
               samplingEnabled ? Trace::TraceMode::kSampling : Trace::TraceMode::kMethodTracing,
               intervalUs);
}

static void VMDebug_startMethodTracingFd(JNIEnv* env, jclass, jstring javaTraceFilename,
                                         jint javaFd, jint bufferSize, jint flags,
                                         jboolean samplingEnabled, jint intervalUs,
                                         jboolean streamingOutput) {
  int originalFd = javaFd;
  if (originalFd < 0) {
    return;
  }

  int fd = dup(originalFd);
  if (fd < 0) {
    ScopedObjectAccess soa(env);
    soa.Self()->ThrowNewExceptionF("Ljava/lang/RuntimeException;",
                                   "dup(%d) failed: %s", originalFd, strerror(errno));
    return;
  }

  ScopedUtfChars traceFilename(env, javaTraceFilename);
  if (traceFilename.c_str() == nullptr) {
    return;
  }
  Trace::TraceOutputMode outputMode = streamingOutput
                                          ? Trace::TraceOutputMode::kStreaming
                                          : Trace::TraceOutputMode::kFile;
  Trace::Start(traceFilename.c_str(), fd, bufferSize, flags, outputMode,
               samplingEnabled ? Trace::TraceMode::kSampling : Trace::TraceMode::kMethodTracing,
               intervalUs);
}

static void VMDebug_startMethodTracingFilename(JNIEnv* env, jclass, jstring javaTraceFilename,
                                               jint bufferSize, jint flags,
                                               jboolean samplingEnabled, jint intervalUs) {
  ScopedUtfChars traceFilename(env, javaTraceFilename);
  if (traceFilename.c_str() == nullptr) {
    return;
  }
  Trace::Start(traceFilename.c_str(), -1, bufferSize, flags, Trace::TraceOutputMode::kFile,
               samplingEnabled ? Trace::TraceMode::kSampling : Trace::TraceMode::kMethodTracing,
               intervalUs);
}

static jint VMDebug_getMethodTracingMode(JNIEnv*, jclass) {
  return Trace::GetMethodTracingMode();
}

static void VMDebug_stopMethodTracing(JNIEnv*, jclass) {
  Trace::Stop();
}

static void VMDebug_startEmulatorTracing(JNIEnv*, jclass) {
  UNIMPLEMENTED(WARNING);
  // dvmEmulatorTraceStart();
}

static void VMDebug_stopEmulatorTracing(JNIEnv*, jclass) {
  UNIMPLEMENTED(WARNING);
  // dvmEmulatorTraceStop();
}

static jboolean VMDebug_isDebuggerConnected(JNIEnv*, jclass) {
  return Dbg::IsDebuggerActive();
}

static jboolean VMDebug_isDebuggingEnabled(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  return Runtime::Current()->GetRuntimeCallbacks()->IsDebuggerConfigured();
}

static jlong VMDebug_lastDebuggerActivity(JNIEnv*, jclass) {
  return Dbg::LastDebuggerActivity();
}

static void ThrowUnsupportedOperationException(JNIEnv* env) {
  ScopedObjectAccess soa(env);
  soa.Self()->ThrowNewException("Ljava/lang/UnsupportedOperationException;", nullptr);
}

static void VMDebug_startInstructionCounting(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_stopInstructionCounting(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_getInstructionCount(JNIEnv* env, jclass, jintArray /*javaCounts*/) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_resetInstructionCount(JNIEnv* env, jclass) {
  ThrowUnsupportedOperationException(env);
}

static void VMDebug_printLoadedClasses(JNIEnv* env, jclass, jint flags) {
  class DumpClassVisitor : public ClassVisitor {
   public:
    explicit DumpClassVisitor(int dump_flags) : flags_(dump_flags) {}

    bool operator()(ObjPtr<mirror::Class> klass) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
      klass->DumpClass(LOG_STREAM(ERROR), flags_);
      return true;
    }

   private:
    const int flags_;
  };
  DumpClassVisitor visitor(flags);

  ScopedFastNativeObjectAccess soa(env);
  return Runtime::Current()->GetClassLinker()->VisitClasses(&visitor);
}

static jint VMDebug_getLoadedClassCount(JNIEnv* env, jclass) {
  ScopedFastNativeObjectAccess soa(env);
  return Runtime::Current()->GetClassLinker()->NumLoadedClasses();
}

/*
 * Returns the thread-specific CPU-time clock value for the current thread,
 * or -1 if the feature isn't supported.
 */
static jlong VMDebug_threadCpuTimeNanos(JNIEnv*, jclass) {
  return ThreadCpuNanoTime();
}

/*
 * static void dumpHprofData(String fileName, FileDescriptor fd)
 *
 * Cause "hprof" data to be dumped.  We can throw an IOException if an
 * error occurs during file handling.
 */
static void VMDebug_dumpHprofData(JNIEnv* env, jclass, jstring javaFilename, jint javaFd) {
  // Only one of these may be null.
  if (javaFilename == nullptr && javaFd < 0) {
    ScopedObjectAccess soa(env);
    ThrowNullPointerException("fileName == null && fd == null");
    return;
  }

  std::string filename;
  if (javaFilename != nullptr) {
    ScopedUtfChars chars(env, javaFilename);
    if (env->ExceptionCheck()) {
      return;
    }
    filename = chars.c_str();
  } else {
    filename = "[fd]";
  }

  int fd = javaFd;

  hprof::DumpHeap(filename.c_str(), fd, false);
}

static void VMDebug_dumpHprofDataDdms(JNIEnv*, jclass) {
  hprof::DumpHeap("[DDMS]", -1, true);
}

static void VMDebug_dumpReferenceTables(JNIEnv* env, jclass) {
  ScopedObjectAccess soa(env);
  LOG(INFO) << "--- reference table dump ---";

  soa.Env()->DumpReferenceTables(LOG_STREAM(INFO));
  soa.Vm()->DumpReferenceTables(LOG_STREAM(INFO));

  LOG(INFO) << "---";
}

static void VMDebug_crash(JNIEnv*, jclass) {
  LOG(FATAL) << "Crashing runtime on request";
}

static void VMDebug_infopoint(JNIEnv*, jclass, jint id) {
  LOG(INFO) << "VMDebug infopoint " << id << " hit";
}

static jlong VMDebug_countInstancesOfClass(JNIEnv* env,
                                           jclass,
                                           jclass javaClass,
                                           jboolean countAssignable) {
  ScopedObjectAccess soa(env);
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  // Caller's responsibility to do GC if desired.
  ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(javaClass);
  if (c == nullptr) {
    return 0;
  }
  VariableSizedHandleScope hs(soa.Self());
  std::vector<Handle<mirror::Class>> classes {hs.NewHandle(c)};
  uint64_t count = 0;
  heap->CountInstances(classes, countAssignable, &count);
  return count;
}

static jlongArray VMDebug_countInstancesOfClasses(JNIEnv* env,
                                                  jclass,
                                                  jobjectArray javaClasses,
                                                  jboolean countAssignable) {
  ScopedObjectAccess soa(env);
  gc::Heap* const heap = Runtime::Current()->GetHeap();
  // Caller's responsibility to do GC if desired.
  ObjPtr<mirror::ObjectArray<mirror::Class>> decoded_classes =
      soa.Decode<mirror::ObjectArray<mirror::Class>>(javaClasses);
  if (decoded_classes == nullptr) {
    return nullptr;
  }
  VariableSizedHandleScope hs(soa.Self());
  std::vector<Handle<mirror::Class>> classes;
  for (size_t i = 0, count = decoded_classes->GetLength(); i < count; ++i) {
    classes.push_back(hs.NewHandle(decoded_classes->Get(i)));
  }
  std::vector<uint64_t> counts(classes.size(), 0u);
  // Heap::CountInstances can handle null and will put 0 for these classes.
  heap->CountInstances(classes, countAssignable, &counts[0]);
  ObjPtr<mirror::LongArray> long_counts = mirror::LongArray::Alloc(soa.Self(), counts.size());
  if (long_counts == nullptr) {
    soa.Self()->AssertPendingOOMException();
    return nullptr;
  }
  for (size_t i = 0; i < counts.size(); ++i) {
    long_counts->Set(i, counts[i]);
  }
  return soa.AddLocalReference<jlongArray>(long_counts);
}

static jobjectArray VMDebug_getInstancesOfClasses(JNIEnv* env,
                                                  jclass,
                                                  jobjectArray javaClasses,
                                                  jboolean includeAssignable) {
  ScopedObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ObjectArray<mirror::Class>> classes = hs.NewHandle(
      soa.Decode<mirror::ObjectArray<mirror::Class>>(javaClasses));
  if (classes == nullptr) {
    return nullptr;
  }

  jclass object_array_class = env->FindClass("[Ljava/lang/Object;");
  if (env->ExceptionCheck() == JNI_TRUE) {
    return nullptr;
  }
  CHECK(object_array_class != nullptr);

  size_t num_classes = classes->GetLength();
  jobjectArray result = env->NewObjectArray(num_classes, object_array_class, nullptr);
  if (env->ExceptionCheck() == JNI_TRUE) {
    return nullptr;
  }

  gc::Heap* const heap = Runtime::Current()->GetHeap();
  MutableHandle<mirror::Class> h_class(hs.NewHandle<mirror::Class>(nullptr));
  for (size_t i = 0; i < num_classes; ++i) {
    h_class.Assign(classes->Get(i));

    VariableSizedHandleScope hs2(soa.Self());
    std::vector<Handle<mirror::Object>> raw_instances;
    heap->GetInstances(hs2, h_class, includeAssignable, /* max_count */ 0, raw_instances);
    jobjectArray array = env->NewObjectArray(raw_instances.size(),
                                             WellKnownClasses::java_lang_Object,
                                             nullptr);
    if (env->ExceptionCheck() == JNI_TRUE) {
      return nullptr;
    }

    for (size_t j = 0; j < raw_instances.size(); ++j) {
      env->SetObjectArrayElement(array, j, raw_instances[j].ToJObject());
    }
    env->SetObjectArrayElement(result, i, array);
  }
  return result;
}

// We export the VM internal per-heap-space size/alloc/free metrics
// for the zygote space, alloc space (application heap), and the large
// object space for dumpsys meminfo. The other memory region data such
// as PSS, private/shared dirty/shared data are available via
// /proc/<pid>/smaps.
static void VMDebug_getHeapSpaceStats(JNIEnv* env, jclass, jlongArray data) {
  jlong* arr = reinterpret_cast<jlong*>(env->GetPrimitiveArrayCritical(data, 0));
  if (arr == nullptr || env->GetArrayLength(data) < 9) {
    return;
  }

  size_t allocSize = 0;
  size_t allocUsed = 0;
  size_t zygoteSize = 0;
  size_t zygoteUsed = 0;
  size_t largeObjectsSize = 0;
  size_t largeObjectsUsed = 0;
  gc::Heap* heap = Runtime::Current()->GetHeap();
  {
    ScopedObjectAccess soa(env);
    for (gc::space::ContinuousSpace* space : heap->GetContinuousSpaces()) {
      if (space->IsImageSpace()) {
        // Currently don't include the image space.
      } else if (space->IsZygoteSpace()) {
        gc::space::ZygoteSpace* zygote_space = space->AsZygoteSpace();
        zygoteSize += zygote_space->Size();
        zygoteUsed += zygote_space->GetBytesAllocated();
      } else if (space->IsMallocSpace()) {
        // This is a malloc space.
        gc::space::MallocSpace* malloc_space = space->AsMallocSpace();
        allocSize += malloc_space->GetFootprint();
        allocUsed += malloc_space->GetBytesAllocated();
      } else if (space->IsBumpPointerSpace()) {
        gc::space::BumpPointerSpace* bump_pointer_space = space->AsBumpPointerSpace();
        allocSize += bump_pointer_space->Size();
        allocUsed += bump_pointer_space->GetBytesAllocated();
      }
    }
    for (gc::space::DiscontinuousSpace* space : heap->GetDiscontinuousSpaces()) {
      if (space->IsLargeObjectSpace()) {
        largeObjectsSize += space->AsLargeObjectSpace()->GetBytesAllocated();
        largeObjectsUsed += largeObjectsSize;
      }
    }
  }
  size_t allocFree = allocSize - allocUsed;
  size_t zygoteFree = zygoteSize - zygoteUsed;
  size_t largeObjectsFree = largeObjectsSize - largeObjectsUsed;

  int j = 0;
  arr[j++] = allocSize;
  arr[j++] = allocUsed;
  arr[j++] = allocFree;
  arr[j++] = zygoteSize;
  arr[j++] = zygoteUsed;
  arr[j++] = zygoteFree;
  arr[j++] = largeObjectsSize;
  arr[j++] = largeObjectsUsed;
  arr[j++] = largeObjectsFree;
  env->ReleasePrimitiveArrayCritical(data, arr, 0);
}

// The runtime stat names for VMDebug.getRuntimeStat().
enum class VMDebugRuntimeStatId {
  kArtGcGcCount = 0,
  kArtGcGcTime,
  kArtGcBytesAllocated,
  kArtGcBytesFreed,
  kArtGcBlockingGcCount,
  kArtGcBlockingGcTime,
  kArtGcGcCountRateHistogram,
  kArtGcBlockingGcCountRateHistogram,
  kNumRuntimeStats,
};

static jstring VMDebug_getRuntimeStatInternal(JNIEnv* env, jclass, jint statId) {
  gc::Heap* heap = Runtime::Current()->GetHeap();
  switch (static_cast<VMDebugRuntimeStatId>(statId)) {
    case VMDebugRuntimeStatId::kArtGcGcCount: {
      std::string output = std::to_string(heap->GetGcCount());
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcGcTime: {
      std::string output = std::to_string(NsToMs(heap->GetGcTime()));
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcBytesAllocated: {
      std::string output = std::to_string(heap->GetBytesAllocatedEver());
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcBytesFreed: {
      std::string output = std::to_string(heap->GetBytesFreedEver());
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcBlockingGcCount: {
      std::string output = std::to_string(heap->GetBlockingGcCount());
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcBlockingGcTime: {
      std::string output = std::to_string(NsToMs(heap->GetBlockingGcTime()));
      return env->NewStringUTF(output.c_str());
    }
    case VMDebugRuntimeStatId::kArtGcGcCountRateHistogram: {
      std::ostringstream output;
      heap->DumpGcCountRateHistogram(output);
      return env->NewStringUTF(output.str().c_str());
    }
    case VMDebugRuntimeStatId::kArtGcBlockingGcCountRateHistogram: {
      std::ostringstream output;
      heap->DumpBlockingGcCountRateHistogram(output);
      return env->NewStringUTF(output.str().c_str());
    }
    default:
      return nullptr;
  }
}

static bool SetRuntimeStatValue(JNIEnv* env,
                                jobjectArray result,
                                VMDebugRuntimeStatId id,
                                const std::string& value) {
  ScopedLocalRef<jstring> jvalue(env, env->NewStringUTF(value.c_str()));
  if (jvalue.get() == nullptr) {
    return false;
  }
  env->SetObjectArrayElement(result, static_cast<jint>(id), jvalue.get());
  return true;
}

static jobjectArray VMDebug_getRuntimeStatsInternal(JNIEnv* env, jclass) {
  jobjectArray result = env->NewObjectArray(
      static_cast<jint>(VMDebugRuntimeStatId::kNumRuntimeStats),
      WellKnownClasses::java_lang_String,
      nullptr);
  if (result == nullptr) {
    return nullptr;
  }
  gc::Heap* heap = Runtime::Current()->GetHeap();
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcGcCount,
                           std::to_string(heap->GetGcCount()))) {
    return nullptr;
  }
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcGcTime,
                           std::to_string(NsToMs(heap->GetGcTime())))) {
    return nullptr;
  }
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcBytesAllocated,
                           std::to_string(heap->GetBytesAllocatedEver()))) {
    return nullptr;
  }
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcBytesFreed,
                           std::to_string(heap->GetBytesFreedEver()))) {
    return nullptr;
  }
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcBlockingGcCount,
                           std::to_string(heap->GetBlockingGcCount()))) {
    return nullptr;
  }
  if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcBlockingGcTime,
                           std::to_string(NsToMs(heap->GetBlockingGcTime())))) {
    return nullptr;
  }
  {
    std::ostringstream output;
    heap->DumpGcCountRateHistogram(output);
    if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcGcCountRateHistogram,
                             output.str())) {
      return nullptr;
    }
  }
  {
    std::ostringstream output;
    heap->DumpBlockingGcCountRateHistogram(output);
    if (!SetRuntimeStatValue(env, result, VMDebugRuntimeStatId::kArtGcBlockingGcCountRateHistogram,
                             output.str())) {
      return nullptr;
    }
  }
  return result;
}

static void VMDebug_nativeAttachAgent(JNIEnv* env, jclass, jstring agent, jobject classloader) {
  if (agent == nullptr) {
    ScopedObjectAccess soa(env);
    ThrowNullPointerException("agent is null");
    return;
  }

  if (!Dbg::IsJdwpAllowed()) {
    ScopedObjectAccess soa(env);
    ThrowSecurityException("Can't attach agent, process is not debuggable.");
    return;
  }

  std::string filename;
  {
    ScopedUtfChars chars(env, agent);
    if (env->ExceptionCheck()) {
      return;
    }
    filename = chars.c_str();
  }

  Runtime::Current()->AttachAgent(env, filename, classloader);
}

static void VMDebug_allowHiddenApiReflectionFrom(JNIEnv* env, jclass, jclass j_caller) {
  Runtime* runtime = Runtime::Current();
  ScopedObjectAccess soa(env);

  if (!runtime->IsJavaDebuggable()) {
    ThrowSecurityException("Can't exempt class, process is not debuggable.");
    return;
  }

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> h_caller(hs.NewHandle(soa.Decode<mirror::Class>(j_caller)));
  if (h_caller.IsNull()) {
    ThrowNullPointerException("argument is null");
    return;
  }

  h_caller->SetSkipHiddenApiChecks();
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(VMDebug, countInstancesOfClass, "(Ljava/lang/Class;Z)J"),
  NATIVE_METHOD(VMDebug, countInstancesOfClasses, "([Ljava/lang/Class;Z)[J"),
  NATIVE_METHOD(VMDebug, crash, "()V"),
  NATIVE_METHOD(VMDebug, dumpHprofData, "(Ljava/lang/String;I)V"),
  NATIVE_METHOD(VMDebug, dumpHprofDataDdms, "()V"),
  NATIVE_METHOD(VMDebug, dumpReferenceTables, "()V"),
  NATIVE_METHOD(VMDebug, getAllocCount, "(I)I"),
  NATIVE_METHOD(VMDebug, getHeapSpaceStats, "([J)V"),
  NATIVE_METHOD(VMDebug, getInstancesOfClasses, "([Ljava/lang/Class;Z)[[Ljava/lang/Object;"),
  NATIVE_METHOD(VMDebug, getInstructionCount, "([I)V"),
  FAST_NATIVE_METHOD(VMDebug, getLoadedClassCount, "()I"),
  NATIVE_METHOD(VMDebug, getVmFeatureList, "()[Ljava/lang/String;"),
  NATIVE_METHOD(VMDebug, infopoint, "(I)V"),
  FAST_NATIVE_METHOD(VMDebug, isDebuggerConnected, "()Z"),
  FAST_NATIVE_METHOD(VMDebug, isDebuggingEnabled, "()Z"),
  NATIVE_METHOD(VMDebug, getMethodTracingMode, "()I"),
  FAST_NATIVE_METHOD(VMDebug, lastDebuggerActivity, "()J"),
  FAST_NATIVE_METHOD(VMDebug, printLoadedClasses, "(I)V"),
  NATIVE_METHOD(VMDebug, resetAllocCount, "(I)V"),
  NATIVE_METHOD(VMDebug, resetInstructionCount, "()V"),
  NATIVE_METHOD(VMDebug, startAllocCounting, "()V"),
  NATIVE_METHOD(VMDebug, startEmulatorTracing, "()V"),
  NATIVE_METHOD(VMDebug, startInstructionCounting, "()V"),
  NATIVE_METHOD(VMDebug, startMethodTracingDdmsImpl, "(IIZI)V"),
  NATIVE_METHOD(VMDebug, startMethodTracingFd, "(Ljava/lang/String;IIIZIZ)V"),
  NATIVE_METHOD(VMDebug, startMethodTracingFilename, "(Ljava/lang/String;IIZI)V"),
  NATIVE_METHOD(VMDebug, stopAllocCounting, "()V"),
  NATIVE_METHOD(VMDebug, stopEmulatorTracing, "()V"),
  NATIVE_METHOD(VMDebug, stopInstructionCounting, "()V"),
  NATIVE_METHOD(VMDebug, stopMethodTracing, "()V"),
  FAST_NATIVE_METHOD(VMDebug, threadCpuTimeNanos, "()J"),
  NATIVE_METHOD(VMDebug, getRuntimeStatInternal, "(I)Ljava/lang/String;"),
  NATIVE_METHOD(VMDebug, getRuntimeStatsInternal, "()[Ljava/lang/String;"),
  NATIVE_METHOD(VMDebug, nativeAttachAgent, "(Ljava/lang/String;Ljava/lang/ClassLoader;)V"),
  NATIVE_METHOD(VMDebug, allowHiddenApiReflectionFrom, "(Ljava/lang/Class;)V"),
};

void register_dalvik_system_VMDebug(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/VMDebug");
}

}  // namespace art
