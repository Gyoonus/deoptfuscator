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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_UTILS_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_UTILS_H_

#include "base/macros.h"
#include "code_generator.h"
#include "locations.h"
#include "nodes.h"
#include "utils/assembler.h"
#include "utils/label.h"

namespace art {

// Default slow-path for fallback (calling the managed code to handle the intrinsic) in an
// intrinsified call. This will copy the arguments into the positions for a regular call.
//
// Note: The actual parameters are required to be in the locations given by the invoke's location
//       summary. If an intrinsic modifies those locations before a slowpath call, they must be
//       restored!
//
// Note: If an invoke wasn't sharpened, we will put down an invoke-virtual here. That's potentially
//       sub-optimal (compared to a direct pointer call), but this is a slow-path.

template <typename TDexCallingConvention>
class IntrinsicSlowPath : public SlowPathCode {
 public:
  explicit IntrinsicSlowPath(HInvoke* invoke) : SlowPathCode(invoke), invoke_(invoke) { }

  Location MoveArguments(CodeGenerator* codegen) {
    TDexCallingConvention calling_convention_visitor;
    IntrinsicVisitor::MoveArguments(invoke_, codegen, &calling_convention_visitor);
    return calling_convention_visitor.GetMethodLocation();
  }

  void EmitNativeCode(CodeGenerator* codegen) OVERRIDE {
    Assembler* assembler = codegen->GetAssembler();
    assembler->Bind(GetEntryLabel());

    SaveLiveRegisters(codegen, invoke_->GetLocations());

    Location method_loc = MoveArguments(codegen);

    if (invoke_->IsInvokeStaticOrDirect()) {
      codegen->GenerateStaticOrDirectCall(invoke_->AsInvokeStaticOrDirect(), method_loc, this);
    } else {
      codegen->GenerateVirtualCall(invoke_->AsInvokeVirtual(), method_loc, this);
    }

    // Copy the result back to the expected output.
    Location out = invoke_->GetLocations()->Out();
    if (out.IsValid()) {
      DCHECK(out.IsRegister());  // TODO: Replace this when we support output in memory.
      DCHECK(!invoke_->GetLocations()->GetLiveRegisters()->ContainsCoreRegister(out.reg()));
      codegen->MoveFromReturnRegister(out, invoke_->GetType());
    }

    RestoreLiveRegisters(codegen, invoke_->GetLocations());
    assembler->Jump(GetExitLabel());
  }

  const char* GetDescription() const OVERRIDE { return "IntrinsicSlowPath"; }

 private:
  // The instruction where this slow path is happening.
  HInvoke* const invoke_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicSlowPath);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_UTILS_H_
