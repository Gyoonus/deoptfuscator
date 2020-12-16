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

#include "dex_to_dex_decompiler.h"

#include "class_linker.h"
#include "common_compiler_test.h"
#include "compiled_method-inl.h"
#include "compiler_callbacks.h"
#include "dex/dex_file.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "handle_scope-inl.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "verifier/method_verifier-inl.h"
#include "verifier/verifier_deps.h"

namespace art {

class DexToDexDecompilerTest : public CommonCompilerTest {
 public:
  void CompileAll(jobject class_loader) REQUIRES(!Locks::mutator_lock_) {
    TimingLogger timings("CompilerDriverTest::CompileAll", false, false);
    TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
    compiler_options_->boot_image_ = false;
    compiler_options_->SetCompilerFilter(CompilerFilter::kQuicken);
    // Create the main VerifierDeps, here instead of in the compiler since we want to aggregate
    // the results for all the dex files, not just the results for the current dex file.
    Runtime::Current()->GetCompilerCallbacks()->SetVerifierDeps(
        new verifier::VerifierDeps(GetDexFiles(class_loader)));
    compiler_driver_->SetDexFilesForOatFile(GetDexFiles(class_loader));
    compiler_driver_->CompileAll(class_loader, GetDexFiles(class_loader), &timings);
  }

  void RunTest(const char* dex_name) {
    Thread* self = Thread::Current();
    // First load the original dex file.
    jobject original_class_loader;
    {
      ScopedObjectAccess soa(self);
      original_class_loader = LoadDex(dex_name);
    }
    const DexFile* original_dex_file = GetDexFiles(original_class_loader)[0];

    // Load the dex file again and make it writable to quicken them.
    jobject class_loader;
    const DexFile* updated_dex_file = nullptr;
    {
      ScopedObjectAccess soa(self);
      class_loader = LoadDex(dex_name);
      updated_dex_file = GetDexFiles(class_loader)[0];
      Runtime::Current()->GetClassLinker()->RegisterDexFile(
          *updated_dex_file, soa.Decode<mirror::ClassLoader>(class_loader).Ptr());
    }
    // The dex files should be identical.
    int cmp = memcmp(original_dex_file->Begin(),
                     updated_dex_file->Begin(),
                     updated_dex_file->Size());
    ASSERT_EQ(0, cmp);

    updated_dex_file->EnableWrite();
    CompileAll(class_loader);
    // The dex files should be different after quickening.
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_NE(0, cmp);

    // Unquicken the dex file.
    for (uint32_t i = 0; i < updated_dex_file->NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = updated_dex_file->GetClassDef(i);
      const uint8_t* class_data = updated_dex_file->GetClassData(class_def);
      if (class_data == nullptr) {
        continue;
      }
      ClassDataItemIterator it(*updated_dex_file, class_data);
      it.SkipAllFields();

      // Unquicken each method.
      while (it.HasNextMethod()) {
        uint32_t method_idx = it.GetMemberIndex();
        CompiledMethod* compiled_method =
            compiler_driver_->GetCompiledMethod(MethodReference(updated_dex_file, method_idx));
        ArrayRef<const uint8_t> table;
        if (compiled_method != nullptr) {
          table = compiled_method->GetVmapTable();
        }
        optimizer::ArtDecompileDEX(*updated_dex_file,
                                   *it.GetMethodCodeItem(),
                                   table,
                                   /* decompile_return_instruction */ true);
        it.Next();
      }
      DCHECK(!it.HasNext());
    }

    // Make sure after unquickening we go back to the same contents as the original dex file.
    cmp = memcmp(original_dex_file->Begin(), updated_dex_file->Begin(), updated_dex_file->Size());
    ASSERT_EQ(0, cmp);
  }
};

TEST_F(DexToDexDecompilerTest, VerifierDeps) {
  RunTest("VerifierDeps");
}

TEST_F(DexToDexDecompilerTest, DexToDexDecompiler) {
  RunTest("DexToDexDecompiler");
}

}  // namespace art
