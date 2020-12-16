/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "jni.h"

#include <android-base/logging.h>
#include <android-base/macros.h>

#include "art_method-inl.h"
#include "base/enums.h"
#include "dex/dex_file-inl.h"
#include "instrumentation.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/profile_compilation_info.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "nativehelper/ScopedUtfChars.h"
#include "oat_file.h"
#include "oat_quick_method_header.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

// public static native boolean hasJit();

static jit::Jit* GetJitIfEnabled() {
  Runtime* runtime = Runtime::Current();
  bool can_jit =
      runtime != nullptr
      && runtime->GetJit() != nullptr
      && runtime->GetInstrumentation()->GetCurrentInstrumentationLevel() !=
            instrumentation::Instrumentation::InstrumentationLevel::kInstrumentWithInterpreter;
  return can_jit ? runtime->GetJit() : nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasJit(JNIEnv*, jclass) {
  return GetJitIfEnabled() != nullptr;
}

// public static native boolean hasOatFile();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasOatFile(JNIEnv* env, jclass cls) {
  ScopedObjectAccess soa(env);

  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(cls);
  const DexFile& dex_file = klass->GetDexFile();
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  return (oat_dex_file != nullptr) ? JNI_TRUE : JNI_FALSE;
}

// public static native boolean runtimeIsSoftFail();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_runtimeIsSoftFail(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                  jclass cls ATTRIBUTE_UNUSED) {
  return Runtime::Current()->IsVerificationSoftFail() ? JNI_TRUE : JNI_FALSE;
}

// public static native boolean isDex2OatEnabled();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isDex2OatEnabled(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                 jclass cls ATTRIBUTE_UNUSED) {
  return Runtime::Current()->IsDex2OatEnabled();
}

// public static native boolean hasImage();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasImage(JNIEnv* env ATTRIBUTE_UNUSED,
                                                         jclass cls ATTRIBUTE_UNUSED) {
  return Runtime::Current()->GetHeap()->HasBootImageSpace();
}

// public static native boolean isImageDex2OatEnabled();

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isImageDex2OatEnabled(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                      jclass cls ATTRIBUTE_UNUSED) {
  return Runtime::Current()->IsImageDex2OatEnabled();
}

// public static native boolean compiledWithOptimizing();
// Did we use the optimizing compiler to compile this?

extern "C" JNIEXPORT jboolean JNICALL Java_Main_compiledWithOptimizing(JNIEnv* env, jclass cls) {
  ScopedObjectAccess soa(env);

  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(cls);
  const DexFile& dex_file = klass->GetDexFile();
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  if (oat_dex_file == nullptr) {
    // Could be JIT, which also uses optimizing, but conservatively say no.
    return JNI_FALSE;
  }
  const OatFile* oat_file = oat_dex_file->GetOatFile();
  CHECK(oat_file != nullptr);

  const char* cmd_line = oat_file->GetOatHeader().GetStoreValueByKey(OatHeader::kDex2OatCmdLineKey);
  CHECK(cmd_line != nullptr);  // Huh? This should not happen.

  // Check the backend.
  constexpr const char* kCompilerBackend = "--compiler-backend=";
  const char* backend = strstr(cmd_line, kCompilerBackend);
  if (backend != nullptr) {
    // If it's set, make sure it's optimizing.
    backend += strlen(kCompilerBackend);
    if (strncmp(backend, "Optimizing", strlen("Optimizing")) != 0) {
      return JNI_FALSE;
    }
  }

  // Check the filter.
  constexpr const char* kCompilerFilter = "--compiler-filter=";
  const char* filter = strstr(cmd_line, kCompilerFilter);
  if (filter != nullptr) {
    // If it's set, make sure it's not interpret-only|verify-none|verify-at-runtime.
    // Note: The space filter might have an impact on the test, but ignore that for now.
    filter += strlen(kCompilerFilter);
    constexpr const char* kInterpretOnly = "interpret-only";
    constexpr const char* kVerifyNone = "verify-none";
    constexpr const char* kVerifyAtRuntime = "verify-at-runtime";
    if (strncmp(filter, kInterpretOnly, strlen(kInterpretOnly)) == 0 ||
        strncmp(filter, kVerifyNone, strlen(kVerifyNone)) == 0 ||
        strncmp(filter, kVerifyAtRuntime, strlen(kVerifyAtRuntime)) == 0) {
      return JNI_FALSE;
    }
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isAotCompiled(JNIEnv* env,
                                                              jclass,
                                                              jclass cls,
                                                              jstring method_name) {
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ArtMethod* method = soa.Decode<mirror::Class>(cls)->FindDeclaredDirectMethodByName(
        chars.c_str(), kRuntimePointerSize);
  const void* oat_code = method->GetOatMethodQuickCode(kRuntimePointerSize);
  if (oat_code == nullptr) {
    return false;
  }
  const void* actual_code = method->GetEntryPointFromQuickCompiledCodePtrSize(kRuntimePointerSize);
  bool interpreter =
      Runtime::Current()->GetClassLinker()->ShouldUseInterpreterEntrypoint(method, actual_code);
  return !interpreter;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasJitCompiledEntrypoint(JNIEnv* env,
                                                                         jclass,
                                                                         jclass cls,
                                                                         jstring method_name) {
  jit::Jit* jit = GetJitIfEnabled();
  if (jit == nullptr) {
    return false;
  }
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ArtMethod* method = soa.Decode<mirror::Class>(cls)->FindDeclaredDirectMethodByName(
        chars.c_str(), kRuntimePointerSize);
  return jit->GetCodeCache()->ContainsPc(method->GetEntryPointFromQuickCompiledCode());
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasJitCompiledCode(JNIEnv* env,
                                                                   jclass,
                                                                   jclass cls,
                                                                   jstring method_name) {
  jit::Jit* jit = GetJitIfEnabled();
  if (jit == nullptr) {
    return false;
  }
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ArtMethod* method = soa.Decode<mirror::Class>(cls)->FindDeclaredDirectMethodByName(
        chars.c_str(), kRuntimePointerSize);
  return jit->GetCodeCache()->ContainsMethod(method);
}

extern "C" JNIEXPORT void JNICALL Java_Main_ensureJitCompiled(JNIEnv* env,
                                                             jclass,
                                                             jclass cls,
                                                             jstring method_name) {
  jit::Jit* jit = GetJitIfEnabled();
  if (jit == nullptr) {
    return;
  }

  Thread* self = Thread::Current();
  ArtMethod* method = nullptr;
  {
    ScopedObjectAccess soa(self);

    ScopedUtfChars chars(env, method_name);
    CHECK(chars.c_str() != nullptr);
    method = soa.Decode<mirror::Class>(cls)->FindDeclaredDirectMethodByName(
        chars.c_str(), kRuntimePointerSize);
    if (method == nullptr) {
      method = soa.Decode<mirror::Class>(cls)->FindDeclaredVirtualMethodByName(
          chars.c_str(), kRuntimePointerSize);
    }
    DCHECK(method != nullptr) << "Unable to find method called " << chars.c_str();
  }

  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  // Update the code cache to make sure the JIT code does not get deleted.
  // Note: this will apply to all JIT compilations.
  code_cache->SetGarbageCollectCode(false);
  while (true) {
    const void* pc = method->GetEntryPointFromQuickCompiledCode();
    if (code_cache->ContainsPc(pc)) {
      break;
    } else {
      // Sleep to yield to the compiler thread.
      usleep(1000);
      ScopedObjectAccess soa(self);
      // Make sure there is a profiling info, required by the compiler.
      ProfilingInfo::Create(self, method, /* retry_allocation */ true);
      // Will either ensure it's compiled or do the compilation itself.
      jit->CompileMethod(method, self, /* osr */ false);
    }
  }
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasSingleImplementation(JNIEnv* env,
                                                                        jclass,
                                                                        jclass cls,
                                                                        jstring method_name) {
  ArtMethod* method = nullptr;
  ScopedObjectAccess soa(Thread::Current());
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  method = soa.Decode<mirror::Class>(cls)->FindDeclaredVirtualMethodByName(
      chars.c_str(), kRuntimePointerSize);
  return method->HasSingleImplementation();
}

extern "C" JNIEXPORT int JNICALL Java_Main_getHotnessCounter(JNIEnv* env,
                                                             jclass,
                                                             jclass cls,
                                                             jstring method_name) {
  ArtMethod* method = nullptr;
  {
    ScopedObjectAccess soa(Thread::Current());

    ScopedUtfChars chars(env, method_name);
    CHECK(chars.c_str() != nullptr);
    method = soa.Decode<mirror::Class>(cls)->FindDeclaredDirectMethodByName(
        chars.c_str(), kRuntimePointerSize);
  }

  return method->GetCounter();
}

extern "C" JNIEXPORT int JNICALL Java_Main_numberOfDeoptimizations(JNIEnv*, jclass) {
  return Runtime::Current()->GetNumberOfDeoptimizations();
}

extern "C" JNIEXPORT void JNICALL Java_Main_fetchProfiles(JNIEnv*, jclass) {
  jit::Jit* jit = GetJitIfEnabled();
  if (jit == nullptr) {
    return;
  }
  jit::JitCodeCache* code_cache = jit->GetCodeCache();
  std::vector<ProfileMethodInfo> unused_vector;
  std::set<std::string> unused_locations;
  unused_locations.insert("fake_location");
  ScopedObjectAccess soa(Thread::Current());
  code_cache->GetProfiledMethods(unused_locations, unused_vector);
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isClassMoveable(JNIEnv*,
                                                                jclass,
                                                                jclass cls) {
  Runtime* runtime = Runtime::Current();
  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(cls);
  return runtime->GetHeap()->IsMovableObject(klass);
}

extern "C" JNIEXPORT void JNICALL Java_Main_waitForCompilation(JNIEnv*, jclass) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    jit->WaitForCompilationToFinish(Thread::Current());
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_stopJit(JNIEnv*, jclass) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    jit->Stop();
  }
}

extern "C" JNIEXPORT void JNICALL Java_Main_startJit(JNIEnv*, jclass) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    jit->Start();
  }
}

}  // namespace art
