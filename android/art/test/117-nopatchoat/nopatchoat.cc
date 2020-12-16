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

#include "class_linker.h"
#include "dex/dex_file-inl.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "mirror/class-inl.h"
#include "oat_file.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

class NoPatchoatTest {
 public:
  static const OatFile::OatDexFile* getOatDexFile(jclass cls) {
    ScopedObjectAccess soa(Thread::Current());
    ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(cls);
    const DexFile& dex_file = klass->GetDexFile();
    return dex_file.GetOatDexFile();
  }

  static bool isRelocationDeltaZero() {
    std::vector<gc::space::ImageSpace*> spaces =
        Runtime::Current()->GetHeap()->GetBootImageSpaces();
    return !spaces.empty() && spaces[0]->GetImageHeader().GetPatchDelta() == 0;
  }

  static bool hasExecutableOat(jclass cls) {
    const OatFile::OatDexFile* oat_dex_file = getOatDexFile(cls);

    return oat_dex_file != nullptr && oat_dex_file->GetOatFile()->IsExecutable();
  }

  static bool needsRelocation(jclass cls) {
    const OatFile::OatDexFile* oat_dex_file = getOatDexFile(cls);

    if (oat_dex_file == nullptr) {
      return false;
    }

    const OatFile* oat_file = oat_dex_file->GetOatFile();
    return !oat_file->IsPic()
        && CompilerFilter::IsAotCompilationEnabled(oat_file->GetCompilerFilter());
  }
};

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isRelocationDeltaZero(JNIEnv*, jclass) {
  return NoPatchoatTest::isRelocationDeltaZero();
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_hasExecutableOat(JNIEnv*, jclass cls) {
  return NoPatchoatTest::hasExecutableOat(cls);
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_needsRelocation(JNIEnv*, jclass cls) {
  return NoPatchoatTest::needsRelocation(cls);
}

}  // namespace art
