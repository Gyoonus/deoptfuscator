/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "art_method.h"
#include "class_linker.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/executable.h"
#include "mirror/object-inl.h"
#include "obj_ptr.h"
#include "oat_file.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace {

extern "C" JNIEXPORT jlong JNICALL Java_Main_getOatMethodQuickCode(JNIEnv* env,
                                                                   jclass,
                                                                   jobject method) {
  CHECK(method != nullptr);
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Executable> exec = soa.Decode<mirror::Executable>(method);
  ArtMethod* art_method = exec->GetArtMethod();

  const void* quick_code =
    art_method->GetOatMethodQuickCode(Runtime::Current()->GetClassLinker()->GetImagePointerSize());

  return static_cast<jlong>(reinterpret_cast<uintptr_t>(quick_code));
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasOatCompiledCode(JNIEnv* env,
                                                                   jclass,
                                                                   jclass kls) {
  CHECK(kls != nullptr);
  ScopedObjectAccess soa(env);
  Thread* self = Thread::Current();

  ObjPtr<mirror::Class> klass_ptr = self->DecodeJObject(kls)->AsClass();

  bool found = false;
  OatFile::OatClass oat_class = OatFile::FindOatClass(*klass_ptr->GetDexCache()->GetDexFile(),
                                                      klass_ptr->GetDexClassDefIndex(),
                                                      /* out */ &found);

  if (!found) {
    return false;
  }

  OatClassType type = oat_class.GetType();
  switch (type) {
    case kOatClassAllCompiled:
    case kOatClassSomeCompiled:
      return true;

    case kOatClassNoneCompiled:
    case kOatClassMax:
      return false;
  }

  LOG(FATAL) << "unhandled switch statement";
  UNREACHABLE();
}

}  // namespace
}  // namespace art
