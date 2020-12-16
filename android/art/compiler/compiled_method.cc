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

#include "compiled_method.h"

#include "driver/compiled_method_storage.h"
#include "driver/compiler_driver.h"
#include "utils/swap_space.h"

namespace art {

CompiledCode::CompiledCode(CompilerDriver* compiler_driver,
                           InstructionSet instruction_set,
                           const ArrayRef<const uint8_t>& quick_code)
    : compiler_driver_(compiler_driver),
      quick_code_(compiler_driver_->GetCompiledMethodStorage()->DeduplicateCode(quick_code)),
      packed_fields_(InstructionSetField::Encode(instruction_set)) {
}

CompiledCode::~CompiledCode() {
  compiler_driver_->GetCompiledMethodStorage()->ReleaseCode(quick_code_);
}

bool CompiledCode::operator==(const CompiledCode& rhs) const {
  if (quick_code_ != nullptr) {
    if (rhs.quick_code_ == nullptr) {
      return false;
    } else if (quick_code_->size() != rhs.quick_code_->size()) {
      return false;
    } else {
      return std::equal(quick_code_->begin(), quick_code_->end(), rhs.quick_code_->begin());
    }
  }
  return (rhs.quick_code_ == nullptr);
}

size_t CompiledCode::AlignCode(size_t offset) const {
  return AlignCode(offset, GetInstructionSet());
}

size_t CompiledCode::AlignCode(size_t offset, InstructionSet instruction_set) {
  return RoundUp(offset, GetInstructionSetAlignment(instruction_set));
}

size_t CompiledCode::CodeDelta() const {
  return CodeDelta(GetInstructionSet());
}

size_t CompiledCode::CodeDelta(InstructionSet instruction_set) {
  switch (instruction_set) {
    case InstructionSet::kArm:
    case InstructionSet::kArm64:
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
    case InstructionSet::kX86:
    case InstructionSet::kX86_64:
      return 0;
    case InstructionSet::kThumb2: {
      // +1 to set the low-order bit so a BLX will switch to Thumb mode
      return 1;
    }
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return 0;
  }
}

const void* CompiledCode::CodePointer(const void* code_pointer, InstructionSet instruction_set) {
  switch (instruction_set) {
    case InstructionSet::kArm:
    case InstructionSet::kArm64:
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
    case InstructionSet::kX86:
    case InstructionSet::kX86_64:
      return code_pointer;
    case InstructionSet::kThumb2: {
      uintptr_t address = reinterpret_cast<uintptr_t>(code_pointer);
      // Set the low-order bit so a BLX will switch to Thumb mode
      address |= 0x1;
      return reinterpret_cast<const void*>(address);
    }
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return nullptr;
  }
}

CompiledMethod::CompiledMethod(CompilerDriver* driver,
                               InstructionSet instruction_set,
                               const ArrayRef<const uint8_t>& quick_code,
                               const size_t frame_size_in_bytes,
                               const uint32_t core_spill_mask,
                               const uint32_t fp_spill_mask,
                               const ArrayRef<const uint8_t>& method_info,
                               const ArrayRef<const uint8_t>& vmap_table,
                               const ArrayRef<const uint8_t>& cfi_info,
                               const ArrayRef<const linker::LinkerPatch>& patches)
    : CompiledCode(driver, instruction_set, quick_code),
      frame_size_in_bytes_(frame_size_in_bytes),
      core_spill_mask_(core_spill_mask),
      fp_spill_mask_(fp_spill_mask),
      method_info_(driver->GetCompiledMethodStorage()->DeduplicateMethodInfo(method_info)),
      vmap_table_(driver->GetCompiledMethodStorage()->DeduplicateVMapTable(vmap_table)),
      cfi_info_(driver->GetCompiledMethodStorage()->DeduplicateCFIInfo(cfi_info)),
      patches_(driver->GetCompiledMethodStorage()->DeduplicateLinkerPatches(patches)) {
}

CompiledMethod* CompiledMethod::SwapAllocCompiledMethod(
    CompilerDriver* driver,
    InstructionSet instruction_set,
    const ArrayRef<const uint8_t>& quick_code,
    const size_t frame_size_in_bytes,
    const uint32_t core_spill_mask,
    const uint32_t fp_spill_mask,
    const ArrayRef<const uint8_t>& method_info,
    const ArrayRef<const uint8_t>& vmap_table,
    const ArrayRef<const uint8_t>& cfi_info,
    const ArrayRef<const linker::LinkerPatch>& patches) {
  SwapAllocator<CompiledMethod> alloc(driver->GetCompiledMethodStorage()->GetSwapSpaceAllocator());
  CompiledMethod* ret = alloc.allocate(1);
  alloc.construct(ret,
                  driver,
                  instruction_set,
                  quick_code,
                  frame_size_in_bytes,
                  core_spill_mask,
                  fp_spill_mask,
                  method_info,
                  vmap_table,
                  cfi_info, patches);
  return ret;
}

void CompiledMethod::ReleaseSwapAllocatedCompiledMethod(CompilerDriver* driver, CompiledMethod* m) {
  SwapAllocator<CompiledMethod> alloc(driver->GetCompiledMethodStorage()->GetSwapSpaceAllocator());
  alloc.destroy(m);
  alloc.deallocate(m, 1);
}

CompiledMethod::~CompiledMethod() {
  CompiledMethodStorage* storage = GetCompilerDriver()->GetCompiledMethodStorage();
  storage->ReleaseLinkerPatches(patches_);
  storage->ReleaseCFIInfo(cfi_info_);
  storage->ReleaseVMapTable(vmap_table_);
  storage->ReleaseMethodInfo(method_info_);
}

}  // namespace art
