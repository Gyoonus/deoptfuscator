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

#include "art_method-inl.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/profiling_info.h"
#include "nativehelper/ScopedUtfChars.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "stack_map.h"

namespace art {

class OsrVisitor : public StackVisitor {
 public:
  explicit OsrVisitor(Thread* thread, const char* method_name)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        method_name_(method_name),
        in_osr_method_(false),
        in_interpreter_(false) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare(method_name_) == 0) {
      const OatQuickMethodHeader* header =
          Runtime::Current()->GetJit()->GetCodeCache()->LookupOsrMethodHeader(m);
      if (header != nullptr && header == GetCurrentOatQuickMethodHeader()) {
        in_osr_method_ = true;
      } else if (IsShadowFrame()) {
        in_interpreter_ = true;
      }
      return false;
    }
    return true;
  }

  const char* const method_name_;
  bool in_osr_method_;
  bool in_interpreter_;
};

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInOsrCode(JNIEnv* env,
                                                            jclass,
                                                            jstring method_name) {
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    // Just return true for non-jit configurations to stop the infinite loop.
    return JNI_TRUE;
  }
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ScopedObjectAccess soa(Thread::Current());
  OsrVisitor visitor(soa.Self(), chars.c_str());
  visitor.WalkStack();
  return visitor.in_osr_method_;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInInterpreter(JNIEnv* env,
                                                                jclass,
                                                                jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    // The return value is irrelevant if we're not using JIT.
    return false;
  }
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ScopedObjectAccess soa(Thread::Current());
  OsrVisitor visitor(soa.Self(), chars.c_str());
  visitor.WalkStack();
  return visitor.in_interpreter_;
}

class ProfilingInfoVisitor : public StackVisitor {
 public:
  explicit ProfilingInfoVisitor(Thread* thread, const char* method_name)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        method_name_(method_name) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare(method_name_) == 0) {
      ProfilingInfo::Create(Thread::Current(), m, /* retry_allocation */ true);
      return false;
    }
    return true;
  }

  const char* const method_name_;
};

extern "C" JNIEXPORT void JNICALL Java_Main_ensureHasProfilingInfo(JNIEnv* env,
                                                                   jclass,
                                                                   jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    return;
  }
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ScopedObjectAccess soa(Thread::Current());
  ProfilingInfoVisitor visitor(soa.Self(), chars.c_str());
  visitor.WalkStack();
}

class OsrCheckVisitor : public StackVisitor {
 public:
  OsrCheckVisitor(Thread* thread, const char* method_name)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        method_name_(method_name) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    jit::Jit* jit = Runtime::Current()->GetJit();
    if (m_name.compare(method_name_) == 0) {
      while (jit->GetCodeCache()->LookupOsrMethodHeader(m) == nullptr) {
        // Sleep to yield to the compiler thread.
        usleep(1000);
        // Will either ensure it's compiled or do the compilation itself.
        jit->CompileMethod(m, Thread::Current(), /* osr */ true);
      }
      return false;
    }
    return true;
  }

  const char* const method_name_;
};

extern "C" JNIEXPORT void JNICALL Java_Main_ensureHasOsrCode(JNIEnv* env,
                                                             jclass,
                                                             jstring method_name) {
  if (!Runtime::Current()->UseJitCompilation()) {
    return;
  }
  ScopedUtfChars chars(env, method_name);
  CHECK(chars.c_str() != nullptr);
  ScopedObjectAccess soa(Thread::Current());
  OsrCheckVisitor visitor(soa.Self(), chars.c_str());
  visitor.WalkStack();
}

}  // namespace art
