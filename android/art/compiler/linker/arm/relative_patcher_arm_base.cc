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

#include "linker/arm/relative_patcher_arm_base.h"

#include "base/stl_util.h"
#include "compiled_method-inl.h"
#include "debug/method_debug_info.h"
#include "dex/dex_file_types.h"
#include "linker/linker_patch.h"
#include "linker/output_stream.h"
#include "oat.h"
#include "oat_quick_method_header.h"

namespace art {
namespace linker {

class ArmBaseRelativePatcher::ThunkData {
 public:
  ThunkData(std::vector<uint8_t> code, uint32_t max_next_offset)
      : code_(std::move(code)),
        offsets_(),
        max_next_offset_(max_next_offset),
        pending_offset_(0u) {
    DCHECK(NeedsNextThunk());  // The data is constructed only when we expect to need the thunk.
  }

  ThunkData(ThunkData&& src) = default;

  size_t CodeSize() const {
    return code_.size();
  }

  ArrayRef<const uint8_t> GetCode() const {
    return ArrayRef<const uint8_t>(code_);
  }

  bool NeedsNextThunk() const {
    return max_next_offset_ != 0u;
  }

  uint32_t MaxNextOffset() const {
    DCHECK(NeedsNextThunk());
    return max_next_offset_;
  }

  void ClearMaxNextOffset() {
    DCHECK(NeedsNextThunk());
    max_next_offset_ = 0u;
  }

  void SetMaxNextOffset(uint32_t max_next_offset) {
    DCHECK(!NeedsNextThunk());
    max_next_offset_ = max_next_offset;
  }

  // Adjust the MaxNextOffset() down if needed to fit the code before the next thunk.
  // Returns true if it was adjusted, false if the old value was kept.
  bool MakeSpaceBefore(const ThunkData& next_thunk, size_t alignment) {
    DCHECK(NeedsNextThunk());
    DCHECK(next_thunk.NeedsNextThunk());
    DCHECK_ALIGNED_PARAM(MaxNextOffset(), alignment);
    DCHECK_ALIGNED_PARAM(next_thunk.MaxNextOffset(), alignment);
    if (next_thunk.MaxNextOffset() - CodeSize() < MaxNextOffset()) {
      max_next_offset_ = RoundDown(next_thunk.MaxNextOffset() - CodeSize(), alignment);
      return true;
    } else {
      return false;
    }
  }

  uint32_t ReserveOffset(size_t offset) {
    DCHECK(NeedsNextThunk());
    DCHECK_LE(offset, max_next_offset_);
    max_next_offset_ = 0u;  // The reserved offset should satisfy all pending references.
    offsets_.push_back(offset);
    return offset + CodeSize();
  }

  bool HasReservedOffset() const {
    return !offsets_.empty();
  }

  uint32_t LastReservedOffset() const {
    DCHECK(HasReservedOffset());
    return offsets_.back();
  }

  bool HasPendingOffset() const {
    return pending_offset_ != offsets_.size();
  }

  uint32_t GetPendingOffset() const {
    DCHECK(HasPendingOffset());
    return offsets_[pending_offset_];
  }

  void MarkPendingOffsetAsWritten() {
    DCHECK(HasPendingOffset());
    ++pending_offset_;
  }

  bool HasWrittenOffset() const {
    return pending_offset_ != 0u;
  }

  uint32_t LastWrittenOffset() const {
    DCHECK(HasWrittenOffset());
    return offsets_[pending_offset_ - 1u];
  }

  size_t IndexOfFirstThunkAtOrAfter(uint32_t offset) const {
    size_t number_of_thunks = NumberOfThunks();
    for (size_t i = 0; i != number_of_thunks; ++i) {
      if (GetThunkOffset(i) >= offset) {
        return i;
      }
    }
    return number_of_thunks;
  }

  size_t NumberOfThunks() const {
    return offsets_.size();
  }

  uint32_t GetThunkOffset(size_t index) const {
    DCHECK_LT(index, NumberOfThunks());
    return offsets_[index];
  }

 private:
  std::vector<uint8_t> code_;       // The code of the thunk.
  std::vector<uint32_t> offsets_;   // Offsets at which the thunk needs to be written.
  uint32_t max_next_offset_;        // The maximum offset at which the next thunk can be placed.
  uint32_t pending_offset_;         // The index of the next offset to write.
};

class ArmBaseRelativePatcher::PendingThunkComparator {
 public:
  bool operator()(const ThunkData* lhs, const ThunkData* rhs) const {
    DCHECK(lhs->HasPendingOffset());
    DCHECK(rhs->HasPendingOffset());
    // The top of the heap is defined to contain the highest element and we want to pick
    // the thunk with the smallest pending offset, so use the reverse ordering, i.e. ">".
    return lhs->GetPendingOffset() > rhs->GetPendingOffset();
  }
};

uint32_t ArmBaseRelativePatcher::ReserveSpace(uint32_t offset,
                                              const CompiledMethod* compiled_method,
                                              MethodReference method_ref) {
  return ReserveSpaceInternal(offset, compiled_method, method_ref, 0u);
}

uint32_t ArmBaseRelativePatcher::ReserveSpaceEnd(uint32_t offset) {
  // For multi-oat compilations (boot image), ReserveSpaceEnd() is called for each oat file.
  // Since we do not know here whether this is the last file or whether the next opportunity
  // to place thunk will be soon enough, we need to reserve all needed thunks now. Code for
  // subsequent oat files can still call back to them.
  if (!unprocessed_method_call_patches_.empty()) {
    ResolveMethodCalls(offset, MethodReference(nullptr, dex::kDexNoIndex));
  }
  for (ThunkData* data : unreserved_thunks_) {
    uint32_t thunk_offset = CompiledCode::AlignCode(offset, instruction_set_);
    offset = data->ReserveOffset(thunk_offset);
  }
  unreserved_thunks_.clear();
  // We also need to delay initiating the pending_thunks_ until the call to WriteThunks().
  // Check that the `pending_thunks_.capacity()` indicates that no WriteThunks() has taken place.
  DCHECK_EQ(pending_thunks_.capacity(), 0u);
  return offset;
}

uint32_t ArmBaseRelativePatcher::WriteThunks(OutputStream* out, uint32_t offset) {
  if (pending_thunks_.capacity() == 0u) {
    if (thunks_.empty()) {
      return offset;
    }
    // First call to WriteThunks(), prepare the thunks for writing.
    pending_thunks_.reserve(thunks_.size());
    for (auto& entry : thunks_) {
      ThunkData* data = &entry.second;
      if (data->HasPendingOffset()) {
        pending_thunks_.push_back(data);
      }
    }
    std::make_heap(pending_thunks_.begin(), pending_thunks_.end(), PendingThunkComparator());
  }
  uint32_t aligned_offset = CompiledMethod::AlignCode(offset, instruction_set_);
  while (!pending_thunks_.empty() &&
         pending_thunks_.front()->GetPendingOffset() == aligned_offset) {
    // Write alignment bytes and code.
    uint32_t aligned_code_delta = aligned_offset - offset;
    if (aligned_code_delta != 0u && UNLIKELY(!WriteCodeAlignment(out, aligned_code_delta))) {
      return 0u;
    }
    if (UNLIKELY(!WriteThunk(out, pending_thunks_.front()->GetCode()))) {
      return 0u;
    }
    offset = aligned_offset + pending_thunks_.front()->CodeSize();
    // Mark the thunk as written at the pending offset and update the `pending_thunks_` heap.
    std::pop_heap(pending_thunks_.begin(), pending_thunks_.end(), PendingThunkComparator());
    pending_thunks_.back()->MarkPendingOffsetAsWritten();
    if (pending_thunks_.back()->HasPendingOffset()) {
      std::push_heap(pending_thunks_.begin(), pending_thunks_.end(), PendingThunkComparator());
    } else {
      pending_thunks_.pop_back();
    }
    aligned_offset = CompiledMethod::AlignCode(offset, instruction_set_);
  }
  DCHECK(pending_thunks_.empty() || pending_thunks_.front()->GetPendingOffset() > aligned_offset);
  return offset;
}

std::vector<debug::MethodDebugInfo> ArmBaseRelativePatcher::GenerateThunkDebugInfo(
    uint32_t executable_offset) {
  // For multi-oat compilation (boot image), `thunks_` records thunks for all oat files.
  // To return debug info for the current oat file, we must ignore thunks before the
  // `executable_offset` as they are in the previous oat files and this function must be
  // called before reserving thunk positions for subsequent oat files.
  size_t number_of_thunks = 0u;
  for (auto&& entry : thunks_) {
    const ThunkData& data = entry.second;
    number_of_thunks += data.NumberOfThunks() - data.IndexOfFirstThunkAtOrAfter(executable_offset);
  }
  std::vector<debug::MethodDebugInfo> result;
  result.reserve(number_of_thunks);
  for (auto&& entry : thunks_) {
    const ThunkKey& key = entry.first;
    const ThunkData& data = entry.second;
    size_t start = data.IndexOfFirstThunkAtOrAfter(executable_offset);
    if (start == data.NumberOfThunks()) {
      continue;
    }
    // Get the base name to use for the first occurrence of the thunk.
    std::string base_name = GetThunkDebugName(key);
    for (size_t i = start, num = data.NumberOfThunks(); i != num; ++i) {
      debug::MethodDebugInfo info = {};
      if (i == 0u) {
        info.custom_name = base_name;
      } else {
        // Add a disambiguating tag for subsequent identical thunks. Since the `thunks_`
        // keeps records also for thunks in previous oat files, names based on the thunk
        // index shall be unique across the whole multi-oat output.
        info.custom_name = base_name + "_" + std::to_string(i);
      }
      info.isa = instruction_set_;
      info.is_code_address_text_relative = true;
      info.code_address = data.GetThunkOffset(i) - executable_offset;
      info.code_size = data.CodeSize();
      result.push_back(std::move(info));
    }
  }
  return result;
}

ArmBaseRelativePatcher::ArmBaseRelativePatcher(RelativePatcherTargetProvider* provider,
                                               InstructionSet instruction_set)
    : provider_(provider),
      instruction_set_(instruction_set),
      thunks_(),
      unprocessed_method_call_patches_(),
      method_call_thunk_(nullptr),
      pending_thunks_() {
}

ArmBaseRelativePatcher::~ArmBaseRelativePatcher() {
  // All work done by member destructors.
}

uint32_t ArmBaseRelativePatcher::ReserveSpaceInternal(uint32_t offset,
                                                      const CompiledMethod* compiled_method,
                                                      MethodReference method_ref,
                                                      uint32_t max_extra_space) {
  // Adjust code size for extra space required by the subclass.
  uint32_t max_code_size = compiled_method->GetQuickCode().size() + max_extra_space;
  uint32_t code_offset;
  uint32_t next_aligned_offset;
  while (true) {
    code_offset = compiled_method->AlignCode(offset + sizeof(OatQuickMethodHeader));
    next_aligned_offset = compiled_method->AlignCode(code_offset + max_code_size);
    if (unreserved_thunks_.empty() ||
        unreserved_thunks_.front()->MaxNextOffset() >= next_aligned_offset) {
      break;
    }
    ThunkData* thunk = unreserved_thunks_.front();
    if (thunk == method_call_thunk_) {
      ResolveMethodCalls(code_offset, method_ref);
      // This may have changed `method_call_thunk_` data, so re-check if we need to reserve.
      if (unreserved_thunks_.empty() ||
          unreserved_thunks_.front()->MaxNextOffset() >= next_aligned_offset) {
        break;
      }
      // We need to process the new `front()` whether it's still the `method_call_thunk_` or not.
      thunk = unreserved_thunks_.front();
    }
    unreserved_thunks_.pop_front();
    uint32_t thunk_offset = CompiledCode::AlignCode(offset, instruction_set_);
    offset = thunk->ReserveOffset(thunk_offset);
    if (thunk == method_call_thunk_) {
      // All remaining method call patches will be handled by this thunk.
      DCHECK(!unprocessed_method_call_patches_.empty());
      DCHECK_LE(thunk_offset - unprocessed_method_call_patches_.front().GetPatchOffset(),
                MaxPositiveDisplacement(GetMethodCallKey()));
      unprocessed_method_call_patches_.clear();
    }
  }

  // Process patches and check that adding thunks for the current method did not push any
  // thunks (previously existing or newly added) before `next_aligned_offset`. This is
  // essentially a check that we never compile a method that's too big. The calls or branches
  // from the method should be able to reach beyond the end of the method and over any pending
  // thunks. (The number of different thunks should be relatively low and their code short.)
  ProcessPatches(compiled_method, code_offset);
  CHECK(unreserved_thunks_.empty() ||
        unreserved_thunks_.front()->MaxNextOffset() >= next_aligned_offset);

  return offset;
}

uint32_t ArmBaseRelativePatcher::CalculateMethodCallDisplacement(uint32_t patch_offset,
                                                                 uint32_t target_offset) {
  DCHECK(method_call_thunk_ != nullptr);
  // Unsigned arithmetic with its well-defined overflow behavior is just fine here.
  uint32_t displacement = target_offset - patch_offset;
  uint32_t max_positive_displacement = MaxPositiveDisplacement(GetMethodCallKey());
  uint32_t max_negative_displacement = MaxNegativeDisplacement(GetMethodCallKey());
  // NOTE: With unsigned arithmetic we do mean to use && rather than || below.
  if (displacement > max_positive_displacement && displacement < -max_negative_displacement) {
    // Unwritten thunks have higher offsets, check if it's within range.
    DCHECK(!method_call_thunk_->HasPendingOffset() ||
           method_call_thunk_->GetPendingOffset() > patch_offset);
    if (method_call_thunk_->HasPendingOffset() &&
        method_call_thunk_->GetPendingOffset() - patch_offset <= max_positive_displacement) {
      displacement = method_call_thunk_->GetPendingOffset() - patch_offset;
    } else {
      // We must have a previous thunk then.
      DCHECK(method_call_thunk_->HasWrittenOffset());
      DCHECK_LT(method_call_thunk_->LastWrittenOffset(), patch_offset);
      displacement = method_call_thunk_->LastWrittenOffset() - patch_offset;
      DCHECK_GE(displacement, -max_negative_displacement);
    }
  }
  return displacement;
}

uint32_t ArmBaseRelativePatcher::GetThunkTargetOffset(const ThunkKey& key, uint32_t patch_offset) {
  auto it = thunks_.find(key);
  CHECK(it != thunks_.end());
  const ThunkData& data = it->second;
  if (data.HasWrittenOffset()) {
    uint32_t offset = data.LastWrittenOffset();
    DCHECK_LT(offset, patch_offset);
    if (patch_offset - offset <= MaxNegativeDisplacement(key)) {
      return offset;
    }
  }
  DCHECK(data.HasPendingOffset());
  uint32_t offset = data.GetPendingOffset();
  DCHECK_GT(offset, patch_offset);
  DCHECK_LE(offset - patch_offset, MaxPositiveDisplacement(key));
  return offset;
}

ArmBaseRelativePatcher::ThunkKey ArmBaseRelativePatcher::GetMethodCallKey() {
  return ThunkKey(ThunkType::kMethodCall);
}

ArmBaseRelativePatcher::ThunkKey ArmBaseRelativePatcher::GetBakerThunkKey(
    const LinkerPatch& patch) {
  DCHECK_EQ(patch.GetType(), LinkerPatch::Type::kBakerReadBarrierBranch);
  return ThunkKey(ThunkType::kBakerReadBarrier,
                  patch.GetBakerCustomValue1(),
                  patch.GetBakerCustomValue2());
}

void ArmBaseRelativePatcher::ProcessPatches(const CompiledMethod* compiled_method,
                                            uint32_t code_offset) {
  for (const LinkerPatch& patch : compiled_method->GetPatches()) {
    uint32_t patch_offset = code_offset + patch.LiteralOffset();
    ThunkKey key(static_cast<ThunkType>(-1));
    ThunkData* old_data = nullptr;
    if (patch.GetType() == LinkerPatch::Type::kCallRelative) {
      key = GetMethodCallKey();
      unprocessed_method_call_patches_.emplace_back(patch_offset, patch.TargetMethod());
      if (method_call_thunk_ == nullptr) {
        uint32_t max_next_offset = CalculateMaxNextOffset(patch_offset, key);
        auto it = thunks_.Put(key, ThunkData(CompileThunk(key), max_next_offset));
        method_call_thunk_ = &it->second;
        AddUnreservedThunk(method_call_thunk_);
      } else {
        old_data = method_call_thunk_;
      }
    } else if (patch.GetType() == LinkerPatch::Type::kBakerReadBarrierBranch) {
      key = GetBakerThunkKey(patch);
      auto lb = thunks_.lower_bound(key);
      if (lb == thunks_.end() || thunks_.key_comp()(key, lb->first)) {
        uint32_t max_next_offset = CalculateMaxNextOffset(patch_offset, key);
        auto it = thunks_.PutBefore(lb, key, ThunkData(CompileThunk(key), max_next_offset));
        AddUnreservedThunk(&it->second);
      } else {
        old_data = &lb->second;
      }
    }
    if (old_data != nullptr) {
      // Shared path where an old thunk may need an update.
      DCHECK(key.GetType() != static_cast<ThunkType>(-1));
      DCHECK(!old_data->HasReservedOffset() || old_data->LastReservedOffset() < patch_offset);
      if (old_data->NeedsNextThunk()) {
        // Patches for a method are ordered by literal offset, so if we still need to place
        // this thunk for a previous patch, that thunk shall be in range for this patch.
        DCHECK_LE(old_data->MaxNextOffset(), CalculateMaxNextOffset(patch_offset, key));
      } else {
        if (!old_data->HasReservedOffset() ||
            patch_offset - old_data->LastReservedOffset() > MaxNegativeDisplacement(key)) {
          old_data->SetMaxNextOffset(CalculateMaxNextOffset(patch_offset, key));
          AddUnreservedThunk(old_data);
        }
      }
    }
  }
}

void ArmBaseRelativePatcher::AddUnreservedThunk(ThunkData* data) {
  DCHECK(data->NeedsNextThunk());
  size_t index = unreserved_thunks_.size();
  while (index != 0u && data->MaxNextOffset() < unreserved_thunks_[index - 1u]->MaxNextOffset()) {
    --index;
  }
  unreserved_thunks_.insert(unreserved_thunks_.begin() + index, data);
  // We may need to update the max next offset(s) if the thunk code would not fit.
  size_t alignment = GetInstructionSetAlignment(instruction_set_);
  if (index + 1u != unreserved_thunks_.size()) {
    // Note: Ignore the return value as we need to process previous thunks regardless.
    data->MakeSpaceBefore(*unreserved_thunks_[index + 1u], alignment);
  }
  // Make space for previous thunks. Once we find a pending thunk that does
  // not need an adjustment, we can stop.
  while (index != 0u && unreserved_thunks_[index - 1u]->MakeSpaceBefore(*data, alignment)) {
    --index;
    data = unreserved_thunks_[index];
  }
}

void ArmBaseRelativePatcher::ResolveMethodCalls(uint32_t quick_code_offset,
                                                MethodReference method_ref) {
  DCHECK(!unreserved_thunks_.empty());
  DCHECK(!unprocessed_method_call_patches_.empty());
  DCHECK(method_call_thunk_ != nullptr);
  uint32_t max_positive_displacement = MaxPositiveDisplacement(GetMethodCallKey());
  uint32_t max_negative_displacement = MaxNegativeDisplacement(GetMethodCallKey());
  // Process as many patches as possible, stop only on unresolved targets or calls too far back.
  while (!unprocessed_method_call_patches_.empty()) {
    MethodReference target_method = unprocessed_method_call_patches_.front().GetTargetMethod();
    uint32_t patch_offset = unprocessed_method_call_patches_.front().GetPatchOffset();
    DCHECK(!method_call_thunk_->HasReservedOffset() ||
           method_call_thunk_->LastReservedOffset() <= patch_offset);
    if (!method_call_thunk_->HasReservedOffset() ||
        patch_offset - method_call_thunk_->LastReservedOffset() > max_negative_displacement) {
      // No previous thunk in range, check if we can reach the target directly.
      if (target_method == method_ref) {
        DCHECK_GT(quick_code_offset, patch_offset);
        if (quick_code_offset - patch_offset > max_positive_displacement) {
          break;
        }
      } else {
        auto result = provider_->FindMethodOffset(target_method);
        if (!result.first) {
          break;
        }
        uint32_t target_offset = result.second - CompiledCode::CodeDelta(instruction_set_);
        if (target_offset >= patch_offset) {
          DCHECK_LE(target_offset - patch_offset, max_positive_displacement);
        } else if (patch_offset - target_offset > max_negative_displacement) {
          break;
        }
      }
    }
    unprocessed_method_call_patches_.pop_front();
  }
  if (!unprocessed_method_call_patches_.empty()) {
    // Try to adjust the max next offset in `method_call_thunk_`. Do this conservatively only if
    // the thunk shall be at the end of the `unreserved_thunks_` to avoid dealing with overlaps.
    uint32_t new_max_next_offset =
        unprocessed_method_call_patches_.front().GetPatchOffset() + max_positive_displacement;
    if (new_max_next_offset >
        unreserved_thunks_.back()->MaxNextOffset() + unreserved_thunks_.back()->CodeSize()) {
      method_call_thunk_->ClearMaxNextOffset();
      method_call_thunk_->SetMaxNextOffset(new_max_next_offset);
      if (method_call_thunk_ != unreserved_thunks_.back()) {
        RemoveElement(unreserved_thunks_, method_call_thunk_);
        unreserved_thunks_.push_back(method_call_thunk_);
      }
    }
  } else {
    // We have resolved all method calls, we do not need a new thunk anymore.
    method_call_thunk_->ClearMaxNextOffset();
    RemoveElement(unreserved_thunks_, method_call_thunk_);
  }
}

inline uint32_t ArmBaseRelativePatcher::CalculateMaxNextOffset(uint32_t patch_offset,
                                                               const ThunkKey& key) {
  return RoundDown(patch_offset + MaxPositiveDisplacement(key),
                   GetInstructionSetAlignment(instruction_set_));
}

}  // namespace linker
}  // namespace art
