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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_MIPS_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_MIPS_H_

#include "intrinsics.h"

namespace art {

class ArenaAllocator;
class HInvokeStaticOrDirect;
class HInvokeVirtual;

namespace mips {

class CodeGeneratorMIPS;
class MipsAssembler;

class IntrinsicLocationsBuilderMIPS FINAL : public IntrinsicVisitor {
 public:
  explicit IntrinsicLocationsBuilderMIPS(CodeGeneratorMIPS* codegen);

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
  CodeGeneratorMIPS* const codegen_;
  ArenaAllocator* const allocator_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicLocationsBuilderMIPS);
};

class IntrinsicCodeGeneratorMIPS FINAL : public IntrinsicVisitor {
 public:
  explicit IntrinsicCodeGeneratorMIPS(CodeGeneratorMIPS* codegen) : codegen_(codegen) {}

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  void Visit ## Name(HInvoke* invoke) OVERRIDE;
#include "intrinsics_list.h"
  INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS

  bool IsR2OrNewer() const;
  bool IsR6() const;
  bool Is32BitFPU() const;

 private:
  MipsAssembler* GetAssembler();

  ArenaAllocator* GetAllocator();

  CodeGeneratorMIPS* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicCodeGeneratorMIPS);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_MIPS_H_
