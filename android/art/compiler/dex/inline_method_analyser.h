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

#ifndef ART_COMPILER_DEX_INLINE_METHOD_ANALYSER_H_
#define ART_COMPILER_DEX_INLINE_METHOD_ANALYSER_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "dex/dex_file.h"
#include "dex/dex_instruction.h"
#include "dex/method_reference.h"

/*
 * NOTE: This code is part of the quick compiler. It lives in the runtime
 * only to allow the debugger to check whether a method has been inlined.
 */

namespace art {

class CodeItemDataAccessor;

namespace verifier {
class MethodVerifier;
}  // namespace verifier
class ArtMethod;

enum InlineMethodOpcode : uint16_t {
  kInlineOpNop,
  kInlineOpReturnArg,
  kInlineOpNonWideConst,
  kInlineOpIGet,
  kInlineOpIPut,
  kInlineOpConstructor,
};

struct InlineIGetIPutData {
  // The op_variant below is DexMemAccessType but the runtime doesn't know that enumeration.
  uint16_t op_variant : 3;
  uint16_t method_is_static : 1;
  uint16_t object_arg : 4;
  uint16_t src_arg : 4;  // iput only
  uint16_t return_arg_plus1 : 4;  // iput only, method argument to return + 1, 0 = return void.
  uint16_t field_idx;
  uint32_t is_volatile : 1;
  uint32_t field_offset : 31;
};
static_assert(sizeof(InlineIGetIPutData) == sizeof(uint64_t), "Invalid size of InlineIGetIPutData");

struct InlineReturnArgData {
  uint16_t arg;
  uint16_t is_wide : 1;
  uint16_t is_object : 1;
  uint16_t reserved : 14;
  uint32_t reserved2;
};
static_assert(sizeof(InlineReturnArgData) == sizeof(uint64_t),
              "Invalid size of InlineReturnArgData");

struct InlineConstructorData {
  // There can be up to 3 IPUTs, unused fields are marked with kNoDexIndex16.
  uint16_t iput0_field_index;
  uint16_t iput1_field_index;
  uint16_t iput2_field_index;
  uint16_t iput0_arg : 4;
  uint16_t iput1_arg : 4;
  uint16_t iput2_arg : 4;
  uint16_t reserved : 4;
};
static_assert(sizeof(InlineConstructorData) == sizeof(uint64_t),
              "Invalid size of InlineConstructorData");

struct InlineMethod {
  InlineMethodOpcode opcode;
  union {
    uint64_t data;
    InlineIGetIPutData ifield_data;
    InlineReturnArgData return_data;
    InlineConstructorData constructor_data;
  } d;
};

class InlineMethodAnalyser {
 public:
  /**
   * Analyse method code to determine if the method is a candidate for inlining.
   * If it is, record the inlining data.
   *
   * @return true if the method is a candidate for inlining, false otherwise.
   */
  static bool AnalyseMethodCode(ArtMethod* method, InlineMethod* result)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr bool IsInstructionIGet(Instruction::Code opcode) {
    return Instruction::IGET <= opcode && opcode <= Instruction::IGET_SHORT;
  }

  static constexpr bool IsInstructionIPut(Instruction::Code opcode) {
    return Instruction::IPUT <= opcode && opcode <= Instruction::IPUT_SHORT;
  }

  static constexpr uint16_t IGetVariant(Instruction::Code opcode) {
    return opcode - Instruction::IGET;
  }

  static constexpr uint16_t IPutVariant(Instruction::Code opcode) {
    return opcode - Instruction::IPUT;
  }

  // Determines whether the method is a synthetic accessor (method name starts with "access$").
  static bool IsSyntheticAccessor(MethodReference ref);

 private:
  static bool AnalyseMethodCode(const CodeItemDataAccessor* code_item,
                                const MethodReference& method_ref,
                                bool is_static,
                                ArtMethod* method,
                                InlineMethod* result)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static bool AnalyseReturnMethod(const CodeItemDataAccessor* code_item, InlineMethod* result);
  static bool AnalyseConstMethod(const CodeItemDataAccessor* code_item, InlineMethod* result);
  static bool AnalyseIGetMethod(const CodeItemDataAccessor* code_item,
                                const MethodReference& method_ref,
                                bool is_static,
                                ArtMethod* method,
                                InlineMethod* result)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static bool AnalyseIPutMethod(const CodeItemDataAccessor* code_item,
                                const MethodReference& method_ref,
                                bool is_static,
                                ArtMethod* method,
                                InlineMethod* result)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can we fast path instance field access in a verified accessor?
  // If yes, computes field's offset and volatility and whether the method is static or not.
  static bool ComputeSpecialAccessorInfo(ArtMethod* method,
                                         uint32_t field_idx,
                                         bool is_put,
                                         InlineIGetIPutData* result)
      REQUIRES_SHARED(Locks::mutator_lock_);
};

}  // namespace art

#endif  // ART_COMPILER_DEX_INLINE_METHOD_ANALYSER_H_
