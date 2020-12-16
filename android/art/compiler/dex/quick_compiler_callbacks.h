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

#ifndef ART_COMPILER_DEX_QUICK_COMPILER_CALLBACKS_H_
#define ART_COMPILER_DEX_QUICK_COMPILER_CALLBACKS_H_

#include "compiler_callbacks.h"
#include "verifier/verifier_deps.h"

namespace art {

class CompilerDriver;
class DexFile;
class VerificationResults;

class QuickCompilerCallbacks FINAL : public CompilerCallbacks {
 public:
  explicit QuickCompilerCallbacks(CompilerCallbacks::CallbackMode mode)
      : CompilerCallbacks(mode), dex_files_(nullptr) {}

  ~QuickCompilerCallbacks() { }

  void MethodVerified(verifier::MethodVerifier* verifier)
      REQUIRES_SHARED(Locks::mutator_lock_) OVERRIDE;

  void ClassRejected(ClassReference ref) OVERRIDE;

  // We are running in an environment where we can call patchoat safely so we should.
  bool IsRelocationPossible() OVERRIDE {
    return true;
  }

  verifier::VerifierDeps* GetVerifierDeps() const OVERRIDE {
    return verifier_deps_.get();
  }

  void SetVerifierDeps(verifier::VerifierDeps* deps) OVERRIDE {
    verifier_deps_.reset(deps);
  }

  void SetVerificationResults(VerificationResults* verification_results) {
    verification_results_ = verification_results;
  }

  ClassStatus GetPreviousClassState(ClassReference ref) OVERRIDE;

  void SetDoesClassUnloading(bool does_class_unloading, CompilerDriver* compiler_driver)
      OVERRIDE {
    does_class_unloading_ = does_class_unloading;
    compiler_driver_ = compiler_driver;
    DCHECK(!does_class_unloading || compiler_driver_ != nullptr);
  }

  void UpdateClassState(ClassReference ref, ClassStatus state) OVERRIDE;

  bool CanUseOatStatusForVerification(mirror::Class* klass) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetDexFiles(const std::vector<const DexFile*>* dex_files) {
    dex_files_ = dex_files;
  }

 private:
  VerificationResults* verification_results_ = nullptr;
  bool does_class_unloading_ = false;
  CompilerDriver* compiler_driver_ = nullptr;
  std::unique_ptr<verifier::VerifierDeps> verifier_deps_;
  const std::vector<const DexFile*>* dex_files_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_COMPILER_CALLBACKS_H_
