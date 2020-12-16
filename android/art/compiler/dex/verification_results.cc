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

#include "verification_results.h"

#include <android-base/logging.h>

#include "base/mutex-inl.h"
#include "base/stl_util.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "runtime.h"
#include "thread-current-inl.h"
#include "thread.h"
#include "utils/atomic_dex_ref_map-inl.h"
#include "verified_method.h"
#include "verifier/method_verifier-inl.h"

namespace art {

VerificationResults::VerificationResults(const CompilerOptions* compiler_options)
    : compiler_options_(compiler_options),
      verified_methods_lock_("compiler verified methods lock"),
      rejected_classes_lock_("compiler rejected classes lock") {}

VerificationResults::~VerificationResults() {
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  STLDeleteValues(&verified_methods_);
  atomic_verified_methods_.Visit([](const DexFileReference& ref ATTRIBUTE_UNUSED,
                                    const VerifiedMethod* method) {
    delete method;
  });
}

void VerificationResults::ProcessVerifiedMethod(verifier::MethodVerifier* method_verifier) {
  DCHECK(method_verifier != nullptr);
  MethodReference ref = method_verifier->GetMethodReference();
  std::unique_ptr<const VerifiedMethod> verified_method(VerifiedMethod::Create(method_verifier));
  if (verified_method == nullptr) {
    // We'll punt this later.
    return;
  }
  AtomicMap::InsertResult result = atomic_verified_methods_.Insert(ref,
                                                                   /*expected*/ nullptr,
                                                                   verified_method.get());
  const VerifiedMethod* existing = nullptr;
  bool inserted;
  if (result != AtomicMap::kInsertResultInvalidDexFile) {
    inserted = (result == AtomicMap::kInsertResultSuccess);
    if (!inserted) {
      // Rare case.
      CHECK(atomic_verified_methods_.Get(ref, &existing));
      CHECK_NE(verified_method.get(), existing);
    }
  } else {
    WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
    auto it = verified_methods_.find(ref);
    inserted = it == verified_methods_.end();
    if (inserted) {
      verified_methods_.Put(ref, verified_method.get());
      DCHECK(verified_methods_.find(ref) != verified_methods_.end());
    } else {
      existing = it->second;
    }
  }
  if (inserted) {
    // Successfully added, release the unique_ptr since we no longer have ownership.
    DCHECK_EQ(GetVerifiedMethod(ref), verified_method.get());
    verified_method.release();
  } else {
    // TODO: Investigate why are we doing the work again for this method and try to avoid it.
    LOG(WARNING) << "Method processed more than once: " << ref.PrettyMethod();
    if (!Runtime::Current()->UseJitCompilation()) {
      if (kIsDebugBuild) {
        auto ex_set = existing->GetSafeCastSet();
        auto ve_set = verified_method->GetSafeCastSet();
        CHECK_EQ(ex_set == nullptr, ve_set == nullptr);
        CHECK((ex_set == nullptr) || (ex_set->size() == ve_set->size()));
      }
    }
    // Let the unique_ptr delete the new verified method since there was already an existing one
    // registered. It is unsafe to replace the existing one since the JIT may be using it to
    // generate a native GC map.
  }
}

const VerifiedMethod* VerificationResults::GetVerifiedMethod(MethodReference ref) {
  const VerifiedMethod* ret = nullptr;
  if (atomic_verified_methods_.Get(ref, &ret)) {
    return ret;
  }
  ReaderMutexLock mu(Thread::Current(), verified_methods_lock_);
  auto it = verified_methods_.find(ref);
  return (it != verified_methods_.end()) ? it->second : nullptr;
}

void VerificationResults::CreateVerifiedMethodFor(MethodReference ref) {
  // This method should only be called for classes verified at compile time,
  // which have no verifier error, nor has methods that we know will throw
  // at runtime.
  std::unique_ptr<VerifiedMethod> verified_method = std::make_unique<VerifiedMethod>(
      /* encountered_error_types */ 0, /* has_runtime_throw */ false);
  if (atomic_verified_methods_.Insert(ref,
                                      /*expected*/ nullptr,
                                      verified_method.get()) ==
          AtomicMap::InsertResult::kInsertResultSuccess) {
    verified_method.release();
  }
}

void VerificationResults::AddRejectedClass(ClassReference ref) {
  {
    WriterMutexLock mu(Thread::Current(), rejected_classes_lock_);
    rejected_classes_.insert(ref);
  }
  DCHECK(IsClassRejected(ref));
}

bool VerificationResults::IsClassRejected(ClassReference ref) {
  ReaderMutexLock mu(Thread::Current(), rejected_classes_lock_);
  return (rejected_classes_.find(ref) != rejected_classes_.end());
}

bool VerificationResults::IsCandidateForCompilation(MethodReference&,
                                                    const uint32_t access_flags) {
  if (!compiler_options_->IsAotCompilationEnabled()) {
    return false;
  }
  // Don't compile class initializers unless kEverything.
  if ((compiler_options_->GetCompilerFilter() != CompilerFilter::kEverything) &&
     ((access_flags & kAccConstructor) != 0) && ((access_flags & kAccStatic) != 0)) {
    return false;
  }
  return true;
}

void VerificationResults::AddDexFile(const DexFile* dex_file) {
  atomic_verified_methods_.AddDexFile(dex_file);
  WriterMutexLock mu(Thread::Current(), verified_methods_lock_);
  // There can be some verified methods that are already registered for the dex_file since we set
  // up well known classes earlier. Remove these and put them in the array so that we don't
  // accidentally miss seeing them.
  for (auto it = verified_methods_.begin(); it != verified_methods_.end(); ) {
    MethodReference ref = it->first;
    if (ref.dex_file == dex_file) {
      CHECK(atomic_verified_methods_.Insert(ref, nullptr, it->second) ==
          AtomicMap::kInsertResultSuccess);
      it = verified_methods_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace art
