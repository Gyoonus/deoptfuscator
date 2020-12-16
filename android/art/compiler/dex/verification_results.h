/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_VERIFICATION_RESULTS_H_
#define ART_COMPILER_DEX_VERIFICATION_RESULTS_H_

#include <stdint.h>
#include <set>

#include "base/dchecked_vector.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/safe_map.h"
#include "dex/class_reference.h"
#include "dex/method_reference.h"
#include "utils/atomic_dex_ref_map.h"

namespace art {

namespace verifier {
class MethodVerifier;
class VerifierDepsTest;
}  // namespace verifier

class CompilerOptions;
class VerifiedMethod;

// Used by CompilerCallbacks to track verification information from the Runtime.
class VerificationResults {
 public:
  explicit VerificationResults(const CompilerOptions* compiler_options);
  ~VerificationResults();

  void ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!verified_methods_lock_);

  void CreateVerifiedMethodFor(MethodReference ref)
      REQUIRES(!verified_methods_lock_);

  const VerifiedMethod* GetVerifiedMethod(MethodReference ref)
      REQUIRES(!verified_methods_lock_);

  void AddRejectedClass(ClassReference ref) REQUIRES(!rejected_classes_lock_);
  bool IsClassRejected(ClassReference ref) REQUIRES(!rejected_classes_lock_);

  bool IsCandidateForCompilation(MethodReference& method_ref, const uint32_t access_flags);

  // Add a dex file to enable using the atomic map.
  void AddDexFile(const DexFile* dex_file) REQUIRES(!verified_methods_lock_);

 private:
  // Verified methods. The method array is fixed to avoid needing a lock to extend it.
  using AtomicMap = AtomicDexRefMap<MethodReference, const VerifiedMethod*>;
  using VerifiedMethodMap = SafeMap<MethodReference, const VerifiedMethod*>;

  VerifiedMethodMap verified_methods_ GUARDED_BY(verified_methods_lock_);
  const CompilerOptions* const compiler_options_;

  // Dex2oat can add dex files to atomic_verified_methods_ to avoid locking when calling
  // GetVerifiedMethod.
  AtomicMap atomic_verified_methods_;

  ReaderWriterMutex verified_methods_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // Rejected classes.
  ReaderWriterMutex rejected_classes_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::set<ClassReference> rejected_classes_ GUARDED_BY(rejected_classes_lock_);

  friend class verifier::VerifierDepsTest;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_VERIFICATION_RESULTS_H_
