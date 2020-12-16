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

#ifndef ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_
#define ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_

#include <set>
#include <unordered_map>
#include <unordered_set>

#include "base/bit_vector.h"
#include "dex/dex_file.h"
#include "dex/invoke_type.h"
#include "dex/method_reference.h"
#include "handle.h"
#include "quicken_info.h"

namespace art {

class CompiledMethod;
class CompilerDriver;
class DexCompilationUnit;

namespace mirror {
class ClassLoader;
}  // namespace mirror

namespace optimizer {

class DexToDexCompiler {
 public:
  enum class CompilationLevel {
    kDontDexToDexCompile,   // Only meaning wrt image time interpretation.
    kOptimize               // Perform peep-hole optimizations.
  };

  explicit DexToDexCompiler(CompilerDriver* driver);

  CompiledMethod* CompileMethod(const DexFile::CodeItem* code_item,
                                uint32_t access_flags,
                                InvokeType invoke_type,
                                uint16_t class_def_idx,
                                uint32_t method_idx,
                                Handle<mirror::ClassLoader> class_loader,
                                const DexFile& dex_file,
                                const CompilationLevel compilation_level) WARN_UNUSED;

  void MarkForCompilation(Thread* self,
                          const MethodReference& method_ref);

  void ClearState();

  // Unquicken all methods that have conflicting quicken info. This is not done during the
  // quickening process to avoid race conditions.
  void UnquickenConflictingMethods();

  CompilerDriver* GetDriver() {
    return driver_;
  }

  bool ShouldCompileMethod(const MethodReference& ref);

  // Return the number of code items to quicken.
  size_t NumCodeItemsToQuicken(Thread* self) const;

  void SetDexFiles(const std::vector<const DexFile*>& dex_files);

 private:
  // Holds the state for compiling a single method.
  struct CompilationState;

  // Quicken state for a code item, may be referenced by multiple methods.
  struct QuickenState {
    std::vector<MethodReference> methods_;
    std::vector<uint8_t> quicken_data_;
    bool optimized_return_void_ = false;
    bool conflict_ = false;
  };

  BitVector* GetOrAddBitVectorForDex(const DexFile* dex_file) REQUIRES(lock_);

  CompilerDriver* const driver_;

  // State for adding methods (should this be in its own class?).
  const DexFile* active_dex_file_ = nullptr;
  BitVector* active_bit_vector_ = nullptr;

  // Lock that guards duplicate code items and the bitmap.
  mutable Mutex lock_;
  // Record what method references are going to get quickened.
  std::unordered_map<const DexFile*, BitVector> should_quicken_;
  // Guarded by lock_ during writing, accessed without a lock during quickening.
  // This is safe because no thread is adding to the shared code items during the quickening phase.
  std::unordered_set<const DexFile::CodeItem*> shared_code_items_;
  // Blacklisted code items are unquickened in UnquickenConflictingMethods.
  std::unordered_map<const DexFile::CodeItem*, QuickenState> shared_code_item_quicken_info_
      GUARDED_BY(lock_);
  // Number of added code items.
  size_t num_code_items_ GUARDED_BY(lock_) = 0u;
};

std::ostream& operator<<(std::ostream& os, const DexToDexCompiler::CompilationLevel& rhs);

}  // namespace optimizer

}  // namespace art

#endif  // ART_COMPILER_DEX_DEX_TO_DEX_COMPILER_H_
