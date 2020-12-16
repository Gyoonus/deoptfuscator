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

#ifndef ART_COMPILER_DEX_VERIFIED_METHOD_H_
#define ART_COMPILER_DEX_VERIFIED_METHOD_H_

#include <vector>

#include "base/mutex.h"
#include "base/safe_map.h"
#include "dex/dex_file.h"
#include "dex/method_reference.h"

namespace art {

namespace verifier {
class MethodVerifier;
}  // namespace verifier

class VerifiedMethod {
 public:
  VerifiedMethod(uint32_t encountered_error_types, bool has_runtime_throw);

  // Cast elision set type.
  // Since we're adding the dex PCs to the set in increasing order, a sorted vector
  // is better for performance (not just memory usage), especially for large sets.
  typedef std::vector<uint32_t> SafeCastSet;

  static const VerifiedMethod* Create(verifier::MethodVerifier* method_verifier)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ~VerifiedMethod() = default;

  const SafeCastSet* GetSafeCastSet() const {
    return safe_cast_set_.get();
  }

  // Returns true if the cast can statically be verified to be redundant
  // by using the check-cast elision peephole optimization in the verifier.
  bool IsSafeCast(uint32_t pc) const;

  // Returns true if there were any errors during verification.
  bool HasVerificationFailures() const {
    return encountered_error_types_ != 0;
  }

  uint32_t GetEncounteredVerificationFailures() const {
    return encountered_error_types_;
  }

  bool HasRuntimeThrow() const {
    return has_runtime_throw_;
  }

 private:
  // Generate safe case set into safe_cast_set_.
  void GenerateSafeCastSet(verifier::MethodVerifier* method_verifier)
      REQUIRES_SHARED(Locks::mutator_lock_);

  std::unique_ptr<SafeCastSet> safe_cast_set_;

  const uint32_t encountered_error_types_;
  const bool has_runtime_throw_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_VERIFIED_METHOD_H_
