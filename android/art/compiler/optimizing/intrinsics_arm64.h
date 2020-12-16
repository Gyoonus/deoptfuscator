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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_ARM64_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_ARM64_H_

#include "intrinsics.h"

namespace vixl {
namespace aarch64 {

class MacroAssembler;

}  // namespace aarch64
}  // namespace vixl

namespace art {

class ArenaAllocator;
class HInvokeStaticOrDirect;
class HInvokeVirtual;

namespace arm64 {

class CodeGeneratorARM64;

class IntrinsicLocationsBuilderARM64 FINAL : public IntrinsicVisitor {
 public:
  explicit IntrinsicLocationsBuilderARM64(ArenaAllocator* allocator, CodeGeneratorARM64* codegen)
      : allocator_(allocator), codegen_(codegen) {}

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  void Visit ## Name(HInvoke* invoke) OVERRIDE;
#include "intrinsics_list.h"
  INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

  // Check whether an invoke is an intrinsic, and if so, create a location summary. Returns whether
  // a corresponding LocationSummary with the intrinsified_ flag set was generated and attached to
  // the invoke.
  bool TryDispatch(HInvoke* invoke);

 private:
  ArenaAllocator* const allocator_;
  CodeGeneratorARM64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicLocationsBuilderARM64);
};

class IntrinsicCodeGeneratorARM64 FINAL : public IntrinsicVisitor {
 public:
  explicit IntrinsicCodeGeneratorARM64(CodeGeneratorARM64* codegen) : codegen_(codegen) {}

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  void Visit ## Name(HInvoke* invoke) OVERRIDE;
#include "intrinsics_list.h"
  INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

 private:
  vixl::aarch64::MacroAssembler* GetVIXLAssembler();

  ArenaAllocator* GetAllocator();

  CodeGeneratorARM64* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicCodeGeneratorARM64);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_ARM64_H_
