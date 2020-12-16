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

#include "stack_map_stream.h"

#include "art_method-inl.h"
#include "base/stl_util.h"
#include "dex/dex_file_types.h"
#include "optimizing/optimizing_compiler.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

void StackMapStream::BeginStackMapEntry(uint32_t dex_pc,
                                        uint32_t native_pc_offset,
                                        uint32_t register_mask,
                                        BitVector* sp_mask,
                                        uint32_t num_dex_registers,
                                        uint8_t inlining_depth) {
  DCHECK_EQ(0u, current_entry_.dex_pc) << "EndStackMapEntry not called after BeginStackMapEntry";
  current_entry_.dex_pc = dex_pc;
  current_entry_.native_pc_code_offset = CodeOffset::FromOffset(native_pc_offset, instruction_set_);
  current_entry_.register_mask = register_mask;
  current_entry_.sp_mask = sp_mask;
  current_entry_.inlining_depth = inlining_depth;
  current_entry_.inline_infos_start_index = inline_infos_.size();
  current_entry_.stack_mask_index = 0;
  current_entry_.dex_method_index = dex::kDexNoIndex;
  current_entry_.dex_register_entry.num_dex_registers = num_dex_registers;
  current_entry_.dex_register_entry.locations_start_index = dex_register_locations_.size();
  current_entry_.dex_register_entry.live_dex_registers_mask = nullptr;
  if (num_dex_registers != 0u) {
    current_entry_.dex_register_entry.live_dex_registers_mask =
        ArenaBitVector::Create(allocator_, num_dex_registers, true, kArenaAllocStackMapStream);
    current_entry_.dex_register_entry.live_dex_registers_mask->ClearAllBits();
  }
  if (sp_mask != nullptr) {
    stack_mask_max_ = std::max(stack_mask_max_, sp_mask->GetHighestBitSet());
  }
  if (inlining_depth > 0) {
    number_of_stack_maps_with_inline_info_++;
  }

  // Note: dex_pc can be kNoDexPc for native method intrinsics.
  if (dex_pc != dex::kDexNoIndex && (dex_pc_max_ == dex::kDexNoIndex || dex_pc_max_ < dex_pc)) {
    dex_pc_max_ = dex_pc;
  }
  register_mask_max_ = std::max(register_mask_max_, register_mask);
  current_dex_register_ = 0;
}

void StackMapStream::EndStackMapEntry() {
  current_entry_.dex_register_map_index = AddDexRegisterMapEntry(current_entry_.dex_register_entry);
  stack_maps_.push_back(current_entry_);
  current_entry_ = StackMapEntry();
}

void StackMapStream::AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
  if (kind != DexRegisterLocation::Kind::kNone) {
    // Ensure we only use non-compressed location kind at this stage.
    DCHECK(DexRegisterLocation::IsShortLocationKind(kind)) << kind;
    DexRegisterLocation location(kind, value);

    // Look for Dex register `location` in the location catalog (using the
    // companion hash map of locations to indices).  Use its index if it
    // is already in the location catalog.  If not, insert it (in the
    // location catalog and the hash map) and use the newly created index.
    auto it = location_catalog_entries_indices_.Find(location);
    if (it != location_catalog_entries_indices_.end()) {
      // Retrieve the index from the hash map.
      dex_register_locations_.push_back(it->second);
    } else {
      // Create a new entry in the location catalog and the hash map.
      size_t index = location_catalog_entries_.size();
      location_catalog_entries_.push_back(location);
      dex_register_locations_.push_back(index);
      location_catalog_entries_indices_.Insert(std::make_pair(location, index));
    }
    DexRegisterMapEntry* const entry = in_inline_frame_
        ? &current_inline_info_.dex_register_entry
        : &current_entry_.dex_register_entry;
    DCHECK_LT(current_dex_register_, entry->num_dex_registers);
    entry->live_dex_registers_mask->SetBit(current_dex_register_);
    entry->hash += (1 <<
        (current_dex_register_ % (sizeof(DexRegisterMapEntry::hash) * kBitsPerByte)));
    entry->hash += static_cast<uint32_t>(value);
    entry->hash += static_cast<uint32_t>(kind);
  }
  current_dex_register_++;
}

void StackMapStream::AddInvoke(InvokeType invoke_type, uint32_t dex_method_index) {
  current_entry_.invoke_type = invoke_type;
  current_entry_.dex_method_index = dex_method_index;
}

void StackMapStream::BeginInlineInfoEntry(ArtMethod* method,
                                          uint32_t dex_pc,
                                          uint32_t num_dex_registers,
                                          const DexFile* outer_dex_file) {
  DCHECK(!in_inline_frame_);
  in_inline_frame_ = true;
  if (EncodeArtMethodInInlineInfo(method)) {
    current_inline_info_.method = method;
  } else {
    if (dex_pc != static_cast<uint32_t>(-1) && kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      DCHECK(IsSameDexFile(*outer_dex_file, *method->GetDexFile()));
    }
    current_inline_info_.method_index = method->GetDexMethodIndexUnchecked();
  }
  current_inline_info_.dex_pc = dex_pc;
  current_inline_info_.dex_register_entry.num_dex_registers = num_dex_registers;
  current_inline_info_.dex_register_entry.locations_start_index = dex_register_locations_.size();
  current_inline_info_.dex_register_entry.live_dex_registers_mask = nullptr;
  if (num_dex_registers != 0) {
    current_inline_info_.dex_register_entry.live_dex_registers_mask =
        ArenaBitVector::Create(allocator_, num_dex_registers, true, kArenaAllocStackMapStream);
    current_inline_info_.dex_register_entry.live_dex_registers_mask->ClearAllBits();
  }
  current_dex_register_ = 0;
}

void StackMapStream::EndInlineInfoEntry() {
  current_inline_info_.dex_register_map_index =
      AddDexRegisterMapEntry(current_inline_info_.dex_register_entry);
  DCHECK(in_inline_frame_);
  DCHECK_EQ(current_dex_register_, current_inline_info_.dex_register_entry.num_dex_registers)
      << "Inline information contains less registers than expected";
  in_inline_frame_ = false;
  inline_infos_.push_back(current_inline_info_);
  current_inline_info_ = InlineInfoEntry();
}

CodeOffset StackMapStream::ComputeMaxNativePcCodeOffset() const {
  CodeOffset max_native_pc_offset;
  for (const StackMapEntry& entry : stack_maps_) {
    max_native_pc_offset = std::max(max_native_pc_offset, entry.native_pc_code_offset);
  }
  return max_native_pc_offset;
}

size_t StackMapStream::PrepareForFillIn() {
  CodeInfoEncoding encoding;
  encoding.dex_register_map.num_entries = 0;  // TODO: Remove this field.
  encoding.dex_register_map.num_bytes = ComputeDexRegisterMapsSize();
  encoding.location_catalog.num_entries = location_catalog_entries_.size();
  encoding.location_catalog.num_bytes = ComputeDexRegisterLocationCatalogSize();
  encoding.inline_info.num_entries = inline_infos_.size();
  // Must be done before calling ComputeInlineInfoEncoding since ComputeInlineInfoEncoding requires
  // dex_method_index_idx to be filled in.
  PrepareMethodIndices();
  ComputeInlineInfoEncoding(&encoding.inline_info.encoding,
                            encoding.dex_register_map.num_bytes);
  CodeOffset max_native_pc_offset = ComputeMaxNativePcCodeOffset();
  // Prepare the CodeInfo variable-sized encoding.
  encoding.stack_mask.encoding.num_bits = stack_mask_max_ + 1;  // Need room for max element too.
  encoding.stack_mask.num_entries = PrepareStackMasks(encoding.stack_mask.encoding.num_bits);
  encoding.register_mask.encoding.num_bits = MinimumBitsToStore(register_mask_max_);
  encoding.register_mask.num_entries = PrepareRegisterMasks();
  encoding.stack_map.num_entries = stack_maps_.size();
  encoding.stack_map.encoding.SetFromSizes(
      // The stack map contains compressed native PC offsets.
      max_native_pc_offset.CompressedValue(),
      dex_pc_max_,
      encoding.dex_register_map.num_bytes,
      encoding.inline_info.num_entries,
      encoding.register_mask.num_entries,
      encoding.stack_mask.num_entries);
  ComputeInvokeInfoEncoding(&encoding);
  DCHECK_EQ(code_info_encoding_.size(), 0u);
  encoding.Compress(&code_info_encoding_);
  encoding.ComputeTableOffsets();
  // Compute table offsets so we can get the non header size.
  DCHECK_EQ(encoding.HeaderSize(), code_info_encoding_.size());
  needed_size_ = code_info_encoding_.size() + encoding.NonHeaderSize();
  return needed_size_;
}

size_t StackMapStream::ComputeDexRegisterLocationCatalogSize() const {
  size_t size = DexRegisterLocationCatalog::kFixedSize;
  for (const DexRegisterLocation& dex_register_location : location_catalog_entries_) {
    size += DexRegisterLocationCatalog::EntrySize(dex_register_location);
  }
  return size;
}

size_t StackMapStream::DexRegisterMapEntry::ComputeSize(size_t catalog_size) const {
  // For num_dex_registers == 0u live_dex_registers_mask may be null.
  if (num_dex_registers == 0u) {
    return 0u;  // No register map will be emitted.
  }
  DCHECK(live_dex_registers_mask != nullptr);

  // Size of the map in bytes.
  size_t size = DexRegisterMap::kFixedSize;
  // Add the live bit mask for the Dex register liveness.
  size += DexRegisterMap::GetLiveBitMaskSize(num_dex_registers);
  // Compute the size of the set of live Dex register entries.
  size_t number_of_live_dex_registers = live_dex_registers_mask->NumSetBits();
  size_t map_entries_size_in_bits =
      DexRegisterMap::SingleEntrySizeInBits(catalog_size) * number_of_live_dex_registers;
  size_t map_entries_size_in_bytes =
      RoundUp(map_entries_size_in_bits, kBitsPerByte) / kBitsPerByte;
  size += map_entries_size_in_bytes;
  return size;
}

size_t StackMapStream::ComputeDexRegisterMapsSize() const {
  size_t size = 0;
  for (const DexRegisterMapEntry& entry : dex_register_entries_) {
    size += entry.ComputeSize(location_catalog_entries_.size());
  }
  return size;
}

void StackMapStream::ComputeInvokeInfoEncoding(CodeInfoEncoding* encoding) {
  DCHECK(encoding != nullptr);
  uint32_t native_pc_max = 0;
  uint16_t method_index_max = 0;
  size_t invoke_infos_count = 0;
  size_t invoke_type_max = 0;
  for (const StackMapEntry& entry : stack_maps_) {
    if (entry.dex_method_index != dex::kDexNoIndex) {
      native_pc_max = std::max(native_pc_max, entry.native_pc_code_offset.CompressedValue());
      method_index_max = std::max(method_index_max, static_cast<uint16_t>(entry.dex_method_index));
      invoke_type_max = std::max(invoke_type_max, static_cast<size_t>(entry.invoke_type));
      ++invoke_infos_count;
    }
  }
  encoding->invoke_info.num_entries = invoke_infos_count;
  encoding->invoke_info.encoding.SetFromSizes(native_pc_max, invoke_type_max, method_index_max);
}

void StackMapStream::ComputeInlineInfoEncoding(InlineInfoEncoding* encoding,
                                               size_t dex_register_maps_bytes) {
  uint32_t method_index_max = 0;
  uint32_t dex_pc_max = dex::kDexNoIndex;
  uint32_t extra_data_max = 0;

  uint32_t inline_info_index = 0;
  for (const StackMapEntry& entry : stack_maps_) {
    for (size_t j = 0; j < entry.inlining_depth; ++j) {
      InlineInfoEntry inline_entry = inline_infos_[inline_info_index++];
      if (inline_entry.method == nullptr) {
        method_index_max = std::max(method_index_max, inline_entry.dex_method_index_idx);
        extra_data_max = std::max(extra_data_max, 1u);
      } else {
        method_index_max = std::max(
            method_index_max, High32Bits(reinterpret_cast<uintptr_t>(inline_entry.method)));
        extra_data_max = std::max(
            extra_data_max, Low32Bits(reinterpret_cast<uintptr_t>(inline_entry.method)));
      }
      if (inline_entry.dex_pc != dex::kDexNoIndex &&
          (dex_pc_max == dex::kDexNoIndex || dex_pc_max < inline_entry.dex_pc)) {
        dex_pc_max = inline_entry.dex_pc;
      }
    }
  }
  DCHECK_EQ(inline_info_index, inline_infos_.size());

  encoding->SetFromSizes(method_index_max, dex_pc_max, extra_data_max, dex_register_maps_bytes);
}

size_t StackMapStream::MaybeCopyDexRegisterMap(DexRegisterMapEntry& entry,
                                               size_t* current_offset,
                                               MemoryRegion dex_register_locations_region) {
  DCHECK(current_offset != nullptr);
  if ((entry.num_dex_registers == 0) || (entry.live_dex_registers_mask->NumSetBits() == 0)) {
    // No dex register map needed.
    return StackMap::kNoDexRegisterMap;
  }
  if (entry.offset == DexRegisterMapEntry::kOffsetUnassigned) {
    // Not already copied, need to copy and and assign an offset.
    entry.offset = *current_offset;
    const size_t entry_size = entry.ComputeSize(location_catalog_entries_.size());
    DexRegisterMap dex_register_map(
        dex_register_locations_region.Subregion(entry.offset, entry_size));
    *current_offset += entry_size;
    // Fill in the map since it was just added.
    FillInDexRegisterMap(dex_register_map,
                         entry.num_dex_registers,
                         *entry.live_dex_registers_mask,
                         entry.locations_start_index);
  }
  return entry.offset;
}

void StackMapStream::FillInMethodInfo(MemoryRegion region) {
  {
    MethodInfo info(region.begin(), method_indices_.size());
    for (size_t i = 0; i < method_indices_.size(); ++i) {
      info.SetMethodIndex(i, method_indices_[i]);
    }
  }
  if (kIsDebugBuild) {
    // Check the data matches.
    MethodInfo info(region.begin());
    const size_t count = info.NumMethodIndices();
    DCHECK_EQ(count, method_indices_.size());
    for (size_t i = 0; i < count; ++i) {
      DCHECK_EQ(info.GetMethodIndex(i), method_indices_[i]);
    }
  }
}

void StackMapStream::FillInCodeInfo(MemoryRegion region) {
  DCHECK_EQ(0u, current_entry_.dex_pc) << "EndStackMapEntry not called after BeginStackMapEntry";
  DCHECK_NE(0u, needed_size_) << "PrepareForFillIn not called before FillIn";

  DCHECK_EQ(region.size(), needed_size_);

  // Note that the memory region does not have to be zeroed when we JIT code
  // because we do not use the arena allocator there.

  // Write the CodeInfo header.
  region.CopyFrom(0, MemoryRegion(code_info_encoding_.data(), code_info_encoding_.size()));

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  DCHECK_EQ(encoding.stack_map.num_entries, stack_maps_.size());

  MemoryRegion dex_register_locations_region = region.Subregion(
      encoding.dex_register_map.byte_offset,
      encoding.dex_register_map.num_bytes);

  // Set the Dex register location catalog.
  MemoryRegion dex_register_location_catalog_region = region.Subregion(
      encoding.location_catalog.byte_offset,
      encoding.location_catalog.num_bytes);
  DexRegisterLocationCatalog dex_register_location_catalog(dex_register_location_catalog_region);
  // Offset in `dex_register_location_catalog` where to store the next
  // register location.
  size_t location_catalog_offset = DexRegisterLocationCatalog::kFixedSize;
  for (DexRegisterLocation dex_register_location : location_catalog_entries_) {
    dex_register_location_catalog.SetRegisterInfo(location_catalog_offset, dex_register_location);
    location_catalog_offset += DexRegisterLocationCatalog::EntrySize(dex_register_location);
  }
  // Ensure we reached the end of the Dex registers location_catalog.
  DCHECK_EQ(location_catalog_offset, dex_register_location_catalog_region.size());

  ArenaBitVector empty_bitmask(allocator_, 0, /* expandable */ false, kArenaAllocStackMapStream);
  uintptr_t next_dex_register_map_offset = 0;
  uintptr_t next_inline_info_index = 0;
  size_t invoke_info_idx = 0;
  for (size_t i = 0, e = stack_maps_.size(); i < e; ++i) {
    StackMap stack_map = code_info.GetStackMapAt(i, encoding);
    StackMapEntry entry = stack_maps_[i];

    stack_map.SetDexPc(encoding.stack_map.encoding, entry.dex_pc);
    stack_map.SetNativePcCodeOffset(encoding.stack_map.encoding, entry.native_pc_code_offset);
    stack_map.SetRegisterMaskIndex(encoding.stack_map.encoding, entry.register_mask_index);
    stack_map.SetStackMaskIndex(encoding.stack_map.encoding, entry.stack_mask_index);

    size_t offset = MaybeCopyDexRegisterMap(dex_register_entries_[entry.dex_register_map_index],
                                            &next_dex_register_map_offset,
                                            dex_register_locations_region);
    stack_map.SetDexRegisterMapOffset(encoding.stack_map.encoding, offset);

    if (entry.dex_method_index != dex::kDexNoIndex) {
      InvokeInfo invoke_info(code_info.GetInvokeInfo(encoding, invoke_info_idx));
      invoke_info.SetNativePcCodeOffset(encoding.invoke_info.encoding, entry.native_pc_code_offset);
      invoke_info.SetInvokeType(encoding.invoke_info.encoding, entry.invoke_type);
      invoke_info.SetMethodIndexIdx(encoding.invoke_info.encoding, entry.dex_method_index_idx);
      ++invoke_info_idx;
    }

    // Set the inlining info.
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfo(next_inline_info_index, encoding);

      // Fill in the index.
      stack_map.SetInlineInfoIndex(encoding.stack_map.encoding, next_inline_info_index);
      DCHECK_EQ(next_inline_info_index, entry.inline_infos_start_index);
      next_inline_info_index += entry.inlining_depth;

      inline_info.SetDepth(encoding.inline_info.encoding, entry.inlining_depth);
      DCHECK_LE(entry.inline_infos_start_index + entry.inlining_depth, inline_infos_.size());

      for (size_t depth = 0; depth < entry.inlining_depth; ++depth) {
        InlineInfoEntry inline_entry = inline_infos_[depth + entry.inline_infos_start_index];
        if (inline_entry.method != nullptr) {
          inline_info.SetMethodIndexIdxAtDepth(
              encoding.inline_info.encoding,
              depth,
              High32Bits(reinterpret_cast<uintptr_t>(inline_entry.method)));
          inline_info.SetExtraDataAtDepth(
              encoding.inline_info.encoding,
              depth,
              Low32Bits(reinterpret_cast<uintptr_t>(inline_entry.method)));
        } else {
          inline_info.SetMethodIndexIdxAtDepth(encoding.inline_info.encoding,
                                               depth,
                                               inline_entry.dex_method_index_idx);
          inline_info.SetExtraDataAtDepth(encoding.inline_info.encoding, depth, 1);
        }
        inline_info.SetDexPcAtDepth(encoding.inline_info.encoding, depth, inline_entry.dex_pc);
        size_t dex_register_map_offset = MaybeCopyDexRegisterMap(
            dex_register_entries_[inline_entry.dex_register_map_index],
            &next_dex_register_map_offset,
            dex_register_locations_region);
        inline_info.SetDexRegisterMapOffsetAtDepth(encoding.inline_info.encoding,
                                                   depth,
                                                   dex_register_map_offset);
      }
    } else if (encoding.stack_map.encoding.GetInlineInfoEncoding().BitSize() > 0) {
      stack_map.SetInlineInfoIndex(encoding.stack_map.encoding, StackMap::kNoInlineInfo);
    }
  }

  // Write stack masks table.
  const size_t stack_mask_bits = encoding.stack_mask.encoding.BitSize();
  if (stack_mask_bits > 0) {
    size_t stack_mask_bytes = RoundUp(stack_mask_bits, kBitsPerByte) / kBitsPerByte;
    for (size_t i = 0; i < encoding.stack_mask.num_entries; ++i) {
      MemoryRegion source(&stack_masks_[i * stack_mask_bytes], stack_mask_bytes);
      BitMemoryRegion stack_mask = code_info.GetStackMask(i, encoding);
      for (size_t bit_index = 0; bit_index < stack_mask_bits; ++bit_index) {
        stack_mask.StoreBit(bit_index, source.LoadBit(bit_index));
      }
    }
  }

  // Write register masks table.
  for (size_t i = 0; i < encoding.register_mask.num_entries; ++i) {
    BitMemoryRegion register_mask = code_info.GetRegisterMask(i, encoding);
    register_mask.StoreBits(0, register_masks_[i], encoding.register_mask.encoding.BitSize());
  }

  // Verify all written data in debug build.
  if (kIsDebugBuild) {
    CheckCodeInfo(region);
  }
}

void StackMapStream::FillInDexRegisterMap(DexRegisterMap dex_register_map,
                                          uint32_t num_dex_registers,
                                          const BitVector& live_dex_registers_mask,
                                          uint32_t start_index_in_dex_register_locations) const {
  dex_register_map.SetLiveBitMask(num_dex_registers, live_dex_registers_mask);
  // Set the dex register location mapping data.
  size_t number_of_live_dex_registers = live_dex_registers_mask.NumSetBits();
  DCHECK_LE(number_of_live_dex_registers, dex_register_locations_.size());
  DCHECK_LE(start_index_in_dex_register_locations,
            dex_register_locations_.size() - number_of_live_dex_registers);
  for (size_t index_in_dex_register_locations = 0;
      index_in_dex_register_locations != number_of_live_dex_registers;
       ++index_in_dex_register_locations) {
    size_t location_catalog_entry_index = dex_register_locations_[
        start_index_in_dex_register_locations + index_in_dex_register_locations];
    dex_register_map.SetLocationCatalogEntryIndex(
        index_in_dex_register_locations,
        location_catalog_entry_index,
        num_dex_registers,
        location_catalog_entries_.size());
  }
}

size_t StackMapStream::AddDexRegisterMapEntry(const DexRegisterMapEntry& entry) {
  const size_t current_entry_index = dex_register_entries_.size();
  auto entries_it = dex_map_hash_to_stack_map_indices_.find(entry.hash);
  if (entries_it == dex_map_hash_to_stack_map_indices_.end()) {
    // We don't have a perfect hash functions so we need a list to collect all stack maps
    // which might have the same dex register map.
    ScopedArenaVector<uint32_t> stack_map_indices(allocator_->Adapter(kArenaAllocStackMapStream));
    stack_map_indices.push_back(current_entry_index);
    dex_map_hash_to_stack_map_indices_.Put(entry.hash, std::move(stack_map_indices));
  } else {
    // We might have collisions, so we need to check whether or not we really have a match.
    for (uint32_t test_entry_index : entries_it->second) {
      if (DexRegisterMapEntryEquals(dex_register_entries_[test_entry_index], entry)) {
        return test_entry_index;
      }
    }
    entries_it->second.push_back(current_entry_index);
  }
  dex_register_entries_.push_back(entry);
  return current_entry_index;
}

bool StackMapStream::DexRegisterMapEntryEquals(const DexRegisterMapEntry& a,
                                               const DexRegisterMapEntry& b) const {
  if ((a.live_dex_registers_mask == nullptr) != (b.live_dex_registers_mask == nullptr)) {
    return false;
  }
  if (a.num_dex_registers != b.num_dex_registers) {
    return false;
  }
  if (a.num_dex_registers != 0u) {
    DCHECK(a.live_dex_registers_mask != nullptr);
    DCHECK(b.live_dex_registers_mask != nullptr);
    if (!a.live_dex_registers_mask->Equal(b.live_dex_registers_mask)) {
      return false;
    }
    size_t number_of_live_dex_registers = a.live_dex_registers_mask->NumSetBits();
    DCHECK_LE(number_of_live_dex_registers, dex_register_locations_.size());
    DCHECK_LE(a.locations_start_index,
              dex_register_locations_.size() - number_of_live_dex_registers);
    DCHECK_LE(b.locations_start_index,
              dex_register_locations_.size() - number_of_live_dex_registers);
    auto a_begin = dex_register_locations_.begin() + a.locations_start_index;
    auto b_begin = dex_register_locations_.begin() + b.locations_start_index;
    if (!std::equal(a_begin, a_begin + number_of_live_dex_registers, b_begin)) {
      return false;
    }
  }
  return true;
}

// Helper for CheckCodeInfo - check that register map has the expected content.
void StackMapStream::CheckDexRegisterMap(const CodeInfo& code_info,
                                         const DexRegisterMap& dex_register_map,
                                         size_t num_dex_registers,
                                         BitVector* live_dex_registers_mask,
                                         size_t dex_register_locations_index) const {
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  for (size_t reg = 0; reg < num_dex_registers; reg++) {
    // Find the location we tried to encode.
    DexRegisterLocation expected = DexRegisterLocation::None();
    if (live_dex_registers_mask->IsBitSet(reg)) {
      size_t catalog_index = dex_register_locations_[dex_register_locations_index++];
      expected = location_catalog_entries_[catalog_index];
    }
    // Compare to the seen location.
    if (expected.GetKind() == DexRegisterLocation::Kind::kNone) {
      DCHECK(!dex_register_map.IsValid() || !dex_register_map.IsDexRegisterLive(reg))
          << dex_register_map.IsValid() << " " << dex_register_map.IsDexRegisterLive(reg);
    } else {
      DCHECK(dex_register_map.IsDexRegisterLive(reg));
      DexRegisterLocation seen = dex_register_map.GetDexRegisterLocation(
          reg, num_dex_registers, code_info, encoding);
      DCHECK_EQ(expected.GetKind(), seen.GetKind());
      DCHECK_EQ(expected.GetValue(), seen.GetValue());
    }
  }
  if (num_dex_registers == 0) {
    DCHECK(!dex_register_map.IsValid());
  }
}

size_t StackMapStream::PrepareRegisterMasks() {
  register_masks_.resize(stack_maps_.size(), 0u);
  ScopedArenaUnorderedMap<uint32_t, size_t> dedupe(allocator_->Adapter(kArenaAllocStackMapStream));
  for (StackMapEntry& stack_map : stack_maps_) {
    const size_t index = dedupe.size();
    stack_map.register_mask_index = dedupe.emplace(stack_map.register_mask, index).first->second;
    register_masks_[index] = stack_map.register_mask;
  }
  return dedupe.size();
}

void StackMapStream::PrepareMethodIndices() {
  CHECK(method_indices_.empty());
  method_indices_.resize(stack_maps_.size() + inline_infos_.size());
  ScopedArenaUnorderedMap<uint32_t, size_t> dedupe(allocator_->Adapter(kArenaAllocStackMapStream));
  for (StackMapEntry& stack_map : stack_maps_) {
    const size_t index = dedupe.size();
    const uint32_t method_index = stack_map.dex_method_index;
    if (method_index != dex::kDexNoIndex) {
      stack_map.dex_method_index_idx = dedupe.emplace(method_index, index).first->second;
      method_indices_[index] = method_index;
    }
  }
  for (InlineInfoEntry& inline_info : inline_infos_) {
    const size_t index = dedupe.size();
    const uint32_t method_index = inline_info.method_index;
    CHECK_NE(method_index, dex::kDexNoIndex);
    inline_info.dex_method_index_idx = dedupe.emplace(method_index, index).first->second;
    method_indices_[index] = method_index;
  }
  method_indices_.resize(dedupe.size());
}


size_t StackMapStream::PrepareStackMasks(size_t entry_size_in_bits) {
  // Preallocate memory since we do not want it to move (the dedup map will point into it).
  const size_t byte_entry_size = RoundUp(entry_size_in_bits, kBitsPerByte) / kBitsPerByte;
  stack_masks_.resize(byte_entry_size * stack_maps_.size(), 0u);
  // For deduplicating we store the stack masks as byte packed for simplicity. We can bit pack later
  // when copying out from stack_masks_.
  ScopedArenaUnorderedMap<MemoryRegion,
                          size_t,
                          FNVHash<MemoryRegion>,
                          MemoryRegion::ContentEquals> dedup(
                              stack_maps_.size(), allocator_->Adapter(kArenaAllocStackMapStream));
  for (StackMapEntry& stack_map : stack_maps_) {
    size_t index = dedup.size();
    MemoryRegion stack_mask(stack_masks_.data() + index * byte_entry_size, byte_entry_size);
    for (size_t i = 0; i < entry_size_in_bits; i++) {
      stack_mask.StoreBit(i, stack_map.sp_mask != nullptr && stack_map.sp_mask->IsBitSet(i));
    }
    stack_map.stack_mask_index = dedup.emplace(stack_mask, index).first->second;
  }
  return dedup.size();
}

// Check that all StackMapStream inputs are correctly encoded by trying to read them back.
void StackMapStream::CheckCodeInfo(MemoryRegion region) const {
  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  DCHECK_EQ(code_info.GetNumberOfStackMaps(encoding), stack_maps_.size());
  size_t invoke_info_index = 0;
  for (size_t s = 0; s < stack_maps_.size(); ++s) {
    const StackMap stack_map = code_info.GetStackMapAt(s, encoding);
    const StackMapEncoding& stack_map_encoding = encoding.stack_map.encoding;
    StackMapEntry entry = stack_maps_[s];

    // Check main stack map fields.
    DCHECK_EQ(stack_map.GetNativePcOffset(stack_map_encoding, instruction_set_),
              entry.native_pc_code_offset.Uint32Value(instruction_set_));
    DCHECK_EQ(stack_map.GetDexPc(stack_map_encoding), entry.dex_pc);
    DCHECK_EQ(stack_map.GetRegisterMaskIndex(stack_map_encoding), entry.register_mask_index);
    DCHECK_EQ(code_info.GetRegisterMaskOf(encoding, stack_map), entry.register_mask);
    const size_t num_stack_mask_bits = code_info.GetNumberOfStackMaskBits(encoding);
    DCHECK_EQ(stack_map.GetStackMaskIndex(stack_map_encoding), entry.stack_mask_index);
    BitMemoryRegion stack_mask = code_info.GetStackMaskOf(encoding, stack_map);
    if (entry.sp_mask != nullptr) {
      DCHECK_GE(stack_mask.size_in_bits(), entry.sp_mask->GetNumberOfBits());
      for (size_t b = 0; b < num_stack_mask_bits; b++) {
        DCHECK_EQ(stack_mask.LoadBit(b), entry.sp_mask->IsBitSet(b));
      }
    } else {
      for (size_t b = 0; b < num_stack_mask_bits; b++) {
        DCHECK_EQ(stack_mask.LoadBit(b), 0u);
      }
    }
    if (entry.dex_method_index != dex::kDexNoIndex) {
      InvokeInfo invoke_info = code_info.GetInvokeInfo(encoding, invoke_info_index);
      DCHECK_EQ(invoke_info.GetNativePcOffset(encoding.invoke_info.encoding, instruction_set_),
                entry.native_pc_code_offset.Uint32Value(instruction_set_));
      DCHECK_EQ(invoke_info.GetInvokeType(encoding.invoke_info.encoding), entry.invoke_type);
      DCHECK_EQ(invoke_info.GetMethodIndexIdx(encoding.invoke_info.encoding),
                entry.dex_method_index_idx);
      invoke_info_index++;
    }
    CheckDexRegisterMap(code_info,
                        code_info.GetDexRegisterMapOf(
                            stack_map, encoding, entry.dex_register_entry.num_dex_registers),
                        entry.dex_register_entry.num_dex_registers,
                        entry.dex_register_entry.live_dex_registers_mask,
                        entry.dex_register_entry.locations_start_index);

    // Check inline info.
    DCHECK_EQ(stack_map.HasInlineInfo(stack_map_encoding), (entry.inlining_depth != 0));
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
      DCHECK_EQ(inline_info.GetDepth(encoding.inline_info.encoding), entry.inlining_depth);
      for (size_t d = 0; d < entry.inlining_depth; ++d) {
        size_t inline_info_index = entry.inline_infos_start_index + d;
        DCHECK_LT(inline_info_index, inline_infos_.size());
        InlineInfoEntry inline_entry = inline_infos_[inline_info_index];
        DCHECK_EQ(inline_info.GetDexPcAtDepth(encoding.inline_info.encoding, d),
                  inline_entry.dex_pc);
        if (inline_info.EncodesArtMethodAtDepth(encoding.inline_info.encoding, d)) {
          DCHECK_EQ(inline_info.GetArtMethodAtDepth(encoding.inline_info.encoding, d),
                    inline_entry.method);
        } else {
          const size_t method_index_idx =
              inline_info.GetMethodIndexIdxAtDepth(encoding.inline_info.encoding, d);
          DCHECK_EQ(method_index_idx, inline_entry.dex_method_index_idx);
          DCHECK_EQ(method_indices_[method_index_idx], inline_entry.method_index);
        }

        CheckDexRegisterMap(code_info,
                            code_info.GetDexRegisterMapAtDepth(
                                d,
                                inline_info,
                                encoding,
                                inline_entry.dex_register_entry.num_dex_registers),
                            inline_entry.dex_register_entry.num_dex_registers,
                            inline_entry.dex_register_entry.live_dex_registers_mask,
                            inline_entry.dex_register_entry.locations_start_index);
      }
    }
  }
}

size_t StackMapStream::ComputeMethodInfoSize() const {
  DCHECK_NE(0u, needed_size_) << "PrepareForFillIn not called before " << __FUNCTION__;
  return MethodInfo::ComputeSize(method_indices_.size());
}

}  // namespace art
