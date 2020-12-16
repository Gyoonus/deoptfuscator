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

#include "verified_method.h"

#include <algorithm>
#include <memory>

#include <android-base/logging.h>

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_instruction-inl.h"
#include "runtime.h"
#include "verifier/method_verifier-inl.h"
#include "verifier/reg_type-inl.h"
#include "verifier/register_line-inl.h"
#include "verifier/verifier_deps.h"

namespace art {

VerifiedMethod::VerifiedMethod(uint32_t encountered_error_types, bool has_runtime_throw)
    : encountered_error_types_(encountered_error_types),
      has_runtime_throw_(has_runtime_throw) {
}

const VerifiedMethod* VerifiedMethod::Create(verifier::MethodVerifier* method_verifier) {
  DCHECK(Runtime::Current()->IsAotCompiler());
  std::unique_ptr<VerifiedMethod> verified_method(
      new VerifiedMethod(method_verifier->GetEncounteredFailureTypes(),
                         method_verifier->HasInstructionThatWillThrow()));

  if (method_verifier->HasCheckCasts()) {
    verified_method->GenerateSafeCastSet(method_verifier);
  }

  return verified_method.release();
}

bool VerifiedMethod::IsSafeCast(uint32_t pc) const {
  if (safe_cast_set_ == nullptr) {
    return false;
  }
  return std::binary_search(safe_cast_set_->begin(), safe_cast_set_->end(), pc);
}

void VerifiedMethod::GenerateSafeCastSet(verifier::MethodVerifier* method_verifier) {
  /*
   * Walks over the method code and adds any cast instructions in which
   * the type cast is implicit to a set, which is used in the code generation
   * to elide these casts.
   */
  if (method_verifier->HasFailures()) {
    return;
  }
  for (const DexInstructionPcPair& pair : method_verifier->CodeItem()) {
    const Instruction& inst = pair.Inst();
    const Instruction::Code code = inst.Opcode();
    if (code == Instruction::CHECK_CAST) {
      const uint32_t dex_pc = pair.DexPc();
      if (!method_verifier->GetInstructionFlags(dex_pc).IsVisited()) {
        // Do not attempt to quicken this instruction, it's unreachable anyway.
        continue;
      }
      const verifier::RegisterLine* line = method_verifier->GetRegLine(dex_pc);
      const verifier::RegType& reg_type(line->GetRegisterType(method_verifier,
                                                              inst.VRegA_21c()));
      const verifier::RegType& cast_type =
          method_verifier->ResolveCheckedClass(dex::TypeIndex(inst.VRegB_21c()));
      // Pass null for the method verifier to not record the VerifierDeps dependency
      // if the types are not assignable.
      if (cast_type.IsStrictlyAssignableFrom(reg_type, /* method_verifier */ nullptr)) {
        // The types are assignable, we record that dependency in the VerifierDeps so
        // that if this changes after OTA, we will re-verify again.
        // We check if reg_type has a class, as the verifier may have inferred it's
        // 'null'.
        if (reg_type.HasClass()) {
          DCHECK(cast_type.HasClass());
          verifier::VerifierDeps::MaybeRecordAssignability(method_verifier->GetDexFile(),
                                                           cast_type.GetClass(),
                                                           reg_type.GetClass(),
                                                           /* strict */ true,
                                                           /* assignable */ true);
        }
        if (safe_cast_set_ == nullptr) {
          safe_cast_set_.reset(new SafeCastSet());
        }
        // Verify ordering for push_back() to the sorted vector.
        DCHECK(safe_cast_set_->empty() || safe_cast_set_->back() < dex_pc);
        safe_cast_set_->push_back(dex_pc);
      }
    }
  }
  DCHECK(safe_cast_set_ == nullptr || !safe_cast_set_->empty());
}

}  // namespace art
