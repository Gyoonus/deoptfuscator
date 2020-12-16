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

#include "assembler.h"

#include <algorithm>
#include <vector>

#include "base/casts.h"
#include "globals.h"
#include "memory_region.h"

namespace art {

AssemblerBuffer::AssemblerBuffer(ArenaAllocator* allocator)
    : allocator_(allocator) {
  static const size_t kInitialBufferCapacity = 4 * KB;
  contents_ = allocator_->AllocArray<uint8_t>(kInitialBufferCapacity, kArenaAllocAssembler);
  cursor_ = contents_;
  limit_ = ComputeLimit(contents_, kInitialBufferCapacity);
  fixup_ = nullptr;
  slow_path_ = nullptr;
#ifndef NDEBUG
  has_ensured_capacity_ = false;
  fixups_processed_ = false;
#endif

  // Verify internal state.
  CHECK_EQ(Capacity(), kInitialBufferCapacity);
  CHECK_EQ(Size(), 0U);
}


AssemblerBuffer::~AssemblerBuffer() {
  if (allocator_->IsRunningOnMemoryTool()) {
    allocator_->MakeInaccessible(contents_, Capacity());
  }
}


void AssemblerBuffer::ProcessFixups(const MemoryRegion& region) {
  AssemblerFixup* fixup = fixup_;
  while (fixup != nullptr) {
    fixup->Process(region, fixup->position());
    fixup = fixup->previous();
  }
}


void AssemblerBuffer::FinalizeInstructions(const MemoryRegion& instructions) {
  // Copy the instructions from the buffer.
  MemoryRegion from(reinterpret_cast<void*>(contents()), Size());
  instructions.CopyFrom(0, from);
  // Process fixups in the instructions.
  ProcessFixups(instructions);
#ifndef NDEBUG
  fixups_processed_ = true;
#endif
}


void AssemblerBuffer::ExtendCapacity(size_t min_capacity) {
  size_t old_size = Size();
  size_t old_capacity = Capacity();
  DCHECK_GT(min_capacity, old_capacity);
  size_t new_capacity = std::min(old_capacity * 2, old_capacity + 1 * MB);
  new_capacity = std::max(new_capacity, min_capacity);

  // Allocate the new data area and copy contents of the old one to it.
  contents_ = reinterpret_cast<uint8_t*>(
      allocator_->Realloc(contents_, old_capacity, new_capacity, kArenaAllocAssembler));

  // Update the cursor and recompute the limit.
  cursor_ = contents_ + old_size;
  limit_ = ComputeLimit(contents_, new_capacity);

  // Verify internal state.
  CHECK_EQ(Capacity(), new_capacity);
  CHECK_EQ(Size(), old_size);
}

void DebugFrameOpCodeWriterForAssembler::ImplicitlyAdvancePC() {
  uint32_t pc = dchecked_integral_cast<uint32_t>(assembler_->CodeSize());
  if (delay_emitting_advance_pc_) {
    uint32_t stream_pos = dchecked_integral_cast<uint32_t>(opcodes_.size());
    delayed_advance_pcs_.push_back(DelayedAdvancePC {stream_pos, pc});
  } else {
    AdvancePC(pc);
  }
}

}  // namespace art
