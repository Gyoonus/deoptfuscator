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

#ifndef ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_ARM_BASE_H_
#define ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_ARM_BASE_H_

#include <deque>
#include <vector>

#include "base/safe_map.h"
#include "dex/method_reference.h"
#include "linker/relative_patcher.h"

namespace art {
namespace linker {

class ArmBaseRelativePatcher : public RelativePatcher {
 public:
  uint32_t ReserveSpace(uint32_t offset,
                        const CompiledMethod* compiled_method,
                        MethodReference method_ref) OVERRIDE;
  uint32_t ReserveSpaceEnd(uint32_t offset) OVERRIDE;
  uint32_t WriteThunks(OutputStream* out, uint32_t offset) OVERRIDE;
  std::vector<debug::MethodDebugInfo> GenerateThunkDebugInfo(uint32_t executable_offset) OVERRIDE;

 protected:
  ArmBaseRelativePatcher(RelativePatcherTargetProvider* provider,
                         InstructionSet instruction_set);
  ~ArmBaseRelativePatcher();

  enum class ThunkType {
    kMethodCall,              // Method call thunk.
    kBakerReadBarrier,        // Baker read barrier.
  };

  class ThunkKey {
   public:
    explicit ThunkKey(ThunkType type, uint32_t custom_value1 = 0u, uint32_t custom_value2 = 0u)
        : type_(type), custom_value1_(custom_value1), custom_value2_(custom_value2) { }

    ThunkType GetType() const {
      return type_;
    }

    uint32_t GetCustomValue1() const {
      return custom_value1_;
    }

    uint32_t GetCustomValue2() const {
      return custom_value2_;
    }

   private:
    ThunkType type_;
    uint32_t custom_value1_;
    uint32_t custom_value2_;
  };

  class ThunkKeyCompare {
   public:
    bool operator()(const ThunkKey& lhs, const ThunkKey& rhs) const {
      if (lhs.GetType() != rhs.GetType()) {
        return lhs.GetType() < rhs.GetType();
      }
      if (lhs.GetCustomValue1() != rhs.GetCustomValue1()) {
        return lhs.GetCustomValue1() < rhs.GetCustomValue1();
      }
      return lhs.GetCustomValue2() < rhs.GetCustomValue2();
    }
  };

  static ThunkKey GetMethodCallKey();
  static ThunkKey GetBakerThunkKey(const LinkerPatch& patch);

  uint32_t ReserveSpaceInternal(uint32_t offset,
                                const CompiledMethod* compiled_method,
                                MethodReference method_ref,
                                uint32_t max_extra_space);
  uint32_t GetThunkTargetOffset(const ThunkKey& key, uint32_t patch_offset);

  uint32_t CalculateMethodCallDisplacement(uint32_t patch_offset,
                                           uint32_t target_offset);

  virtual std::vector<uint8_t> CompileThunk(const ThunkKey& key) = 0;
  virtual std::string GetThunkDebugName(const ThunkKey& key) = 0;
  virtual uint32_t MaxPositiveDisplacement(const ThunkKey& key) = 0;
  virtual uint32_t MaxNegativeDisplacement(const ThunkKey& key) = 0;

 private:
  class ThunkData;

  void ProcessPatches(const CompiledMethod* compiled_method, uint32_t code_offset);
  void AddUnreservedThunk(ThunkData* data);

  void ResolveMethodCalls(uint32_t quick_code_offset, MethodReference method_ref);

  uint32_t CalculateMaxNextOffset(uint32_t patch_offset, const ThunkKey& key);

  RelativePatcherTargetProvider* const provider_;
  const InstructionSet instruction_set_;

  // The data for all thunks.
  // SafeMap<> nodes don't move after being inserted, so we can use direct pointers to the data.
  using ThunkMap = SafeMap<ThunkKey, ThunkData, ThunkKeyCompare>;
  ThunkMap thunks_;

  // ReserveSpace() tracks unprocessed method call patches. These may be resolved later.
  class UnprocessedMethodCallPatch {
   public:
    UnprocessedMethodCallPatch(uint32_t patch_offset, MethodReference target_method)
        : patch_offset_(patch_offset), target_method_(target_method) { }

    uint32_t GetPatchOffset() const {
      return patch_offset_;
    }

    MethodReference GetTargetMethod() const {
      return target_method_;
    }

   private:
    uint32_t patch_offset_;
    MethodReference target_method_;
  };
  std::deque<UnprocessedMethodCallPatch> unprocessed_method_call_patches_;
  // Once we have compiled a method call thunk, cache pointer to the data.
  ThunkData* method_call_thunk_;

  // Thunks
  std::deque<ThunkData*> unreserved_thunks_;

  class PendingThunkComparator;
  std::vector<ThunkData*> pending_thunks_;  // Heap with the PendingThunkComparator.

  friend class Arm64RelativePatcherTest;
  friend class Thumb2RelativePatcherTest;

  DISALLOW_COPY_AND_ASSIGN(ArmBaseRelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_ARM_BASE_H_
