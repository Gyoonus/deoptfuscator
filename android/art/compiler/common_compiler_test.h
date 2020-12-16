/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_COMMON_COMPILER_TEST_H_
#define ART_COMPILER_COMMON_COMPILER_TEST_H_

#include <list>
#include <unordered_set>
#include <vector>

#include "common_runtime_test.h"
#include "compiler.h"
#include "oat_file.h"

namespace art {
namespace mirror {
class ClassLoader;
}  // namespace mirror

class CompilerDriver;
class CompilerOptions;
class CumulativeLogger;
class ProfileCompilationInfo;
class VerificationResults;

template<class T> class Handle;

class CommonCompilerTest : public CommonRuntimeTest {
 public:
  CommonCompilerTest();
  ~CommonCompilerTest();

  // Create an OatMethod based on pointers (for unit tests).
  OatFile::OatMethod CreateOatMethod(const void* code);

  void MakeExecutable(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  static void MakeExecutable(const void* code_start, size_t code_length);

  void MakeExecutable(ObjPtr<mirror::ClassLoader> class_loader, const char* class_name)
      REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  virtual void SetUp();

  virtual void SetUpRuntimeOptions(RuntimeOptions* options);

  Compiler::Kind GetCompilerKind() const;
  void SetCompilerKind(Compiler::Kind compiler_kind);

  InstructionSet GetInstructionSet() const;

  // Get the set of image classes given to the compiler-driver in SetUp. Note: the compiler
  // driver assumes ownership of the set, so the test should properly release the set.
  virtual std::unordered_set<std::string>* GetImageClasses();

  // Get the set of compiled classes given to the compiler-driver in SetUp. Note: the compiler
  // driver assumes ownership of the set, so the test should properly release the set.
  virtual std::unordered_set<std::string>* GetCompiledClasses();

  // Get the set of compiled methods given to the compiler-driver in SetUp. Note: the compiler
  // driver assumes ownership of the set, so the test should properly release the set.
  virtual std::unordered_set<std::string>* GetCompiledMethods();

  virtual ProfileCompilationInfo* GetProfileCompilationInfo();

  virtual CompilerFilter::Filter GetCompilerFilter() const {
    return CompilerFilter::kDefaultCompilerFilter;
  }

  virtual void TearDown();

  void CompileClass(mirror::ClassLoader* class_loader, const char* class_name)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CompileMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  void CompileDirectMethod(Handle<mirror::ClassLoader> class_loader, const char* class_name,
                           const char* method_name, const char* signature)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CompileVirtualMethod(Handle<mirror::ClassLoader> class_loader, const char* class_name,
                            const char* method_name, const char* signature)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CreateCompilerDriver(Compiler::Kind kind, InstructionSet isa, size_t number_of_threads = 2U);

  void ReserveImageSpace();

  void UnreserveImageSpace();

  Compiler::Kind compiler_kind_ = Compiler::kOptimizing;
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<VerificationResults> verification_results_;
  std::unique_ptr<CompilerDriver> compiler_driver_;
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;


 private:
  std::unique_ptr<MemMap> image_reservation_;

  // Chunks must not move their storage after being created - use the node-based std::list.
  std::list<std::vector<uint8_t>> header_code_and_maps_chunks_;
};

}  // namespace art

#endif  // ART_COMPILER_COMMON_COMPILER_TEST_H_
