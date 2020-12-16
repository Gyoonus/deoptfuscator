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

#ifndef ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
#define ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_

#include "base/bit_vector-inl.h"
#include "base/hash_map.h"
#include "base/scoped_arena_containers.h"
#include "base/value_object.h"
#include "memory_region.h"
#include "method_info.h"
#include "nodes.h"
#include "stack_map.h"

namespace art {

// Helper to build art::StackMapStream::LocationCatalogEntriesIndices.
class LocationCatalogEntriesIndicesEmptyFn {
 public:
  void MakeEmpty(std::pair<DexRegisterLocation, size_t>& item) const {
    item.first = DexRegisterLocation::None();
  }
  bool IsEmpty(const std::pair<DexRegisterLocation, size_t>& item) const {
    return item.first == DexRegisterLocation::None();
  }
};

// Hash function for art::StackMapStream::LocationCatalogEntriesIndices.
// This hash function does not create collisions.
class DexRegisterLocationHashFn {
 public:
  size_t operator()(DexRegisterLocation key) const {
    // Concatenate `key`s fields to create a 64-bit value to be hashed.
    int64_t kind_and_value =
        (static_cast<int64_t>(key.kind_) << 32) | static_cast<int64_t>(key.value_);
    return inner_hash_fn_(kind_and_value);
  }
 private:
  std::hash<int64_t> inner_hash_fn_;
};


/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
 public:
  explicit StackMapStream(ScopedArenaAllocator* allocator, InstructionSet instruction_set)
      : allocator_(allocator),
        instruction_set_(instruction_set),
        stack_maps_(allocator->Adapter(kArenaAllocStackMapStream)),
        location_catalog_entries_(allocator->Adapter(kArenaAllocStackMapStream)),
        location_catalog_entries_indices_(allocator->Adapter(kArenaAllocStackMapStream)),
        dex_register_locations_(allocator->Adapter(kArenaAllocStackMapStream)),
        inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)),
        stack_masks_(allocator->Adapter(kArenaAllocStackMapStream)),
        register_masks_(allocator->Adapter(kArenaAllocStackMapStream)),
        method_indices_(allocator->Adapter(kArenaAllocStackMapStream)),
        dex_register_entries_(allocator->Adapter(kArenaAllocStackMapStream)),
        stack_mask_max_(-1),
        dex_pc_max_(kNoDexPc),
        register_mask_max_(0),
        number_of_stack_maps_with_inline_info_(0),
        dex_map_hash_to_stack_map_indices_(std::less<uint32_t>(),
                                           allocator->Adapter(kArenaAllocStackMapStream)),
        current_entry_(),
        current_inline_info_(),
        code_info_encoding_(allocator->Adapter(kArenaAllocStackMapStream)),
        needed_size_(0),
        current_dex_register_(0),
        in_inline_frame_(false) {
    stack_maps_.reserve(10);
    location_catalog_entries_.reserve(4);
    dex_register_locations_.reserve(10 * 4);
    inline_infos_.reserve(2);
    code_info_encoding_.reserve(16);
  }

  // A dex register map entry for a single stack map entry, contains what registers are live as
  // well as indices into the location catalog.
  class DexRegisterMapEntry {
   public:
    static const size_t kOffsetUnassigned = -1;

    BitVector* live_dex_registers_mask;
    uint32_t num_dex_registers;
    size_t locations_start_index;
    // Computed fields
    size_t hash = 0;
    size_t offset = kOffsetUnassigned;

    size_t ComputeSize(size_t catalog_size) const;
  };

  // See runtime/stack_map.h to know what these fields contain.
  struct StackMapEntry {
    uint32_t dex_pc;
    CodeOffset native_pc_code_offset;
    uint32_t register_mask;
    BitVector* sp_mask;
    uint8_t inlining_depth;
    size_t inline_infos_start_index;
    uint32_t stack_mask_index;
    uint32_t register_mask_index;
    DexRegisterMapEntry dex_register_entry;
    size_t dex_register_map_index;
    InvokeType invoke_type;
    uint32_t dex_method_index;
    uint32_t dex_method_index_idx;  // Index into dex method index table.
  };

  struct InlineInfoEntry {
    uint32_t dex_pc;  // dex::kDexNoIndex for intrinsified native methods.
    ArtMethod* method;
    uint32_t method_index;
    DexRegisterMapEntry dex_register_entry;
    size_t dex_register_map_index;
    uint32_t dex_method_index_idx;  // Index into the dex method index table.
  };

  void BeginStackMapEntry(uint32_t dex_pc,
                          uint32_t native_pc_offset,
                          uint32_t register_mask,
                          BitVector* sp_mask,
                          uint32_t num_dex_registers,
                          uint8_t inlining_depth);
  void EndStackMapEntry();

  void AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value);

  void AddInvoke(InvokeType type, uint32_t dex_method_index);

  void BeginInlineInfoEntry(ArtMethod* method,
                            uint32_t dex_pc,
                            uint32_t num_dex_registers,
                            const DexFile* outer_dex_file = nullptr);
  void EndInlineInfoEntry();

  size_t GetNumberOfStackMaps() const {
    return stack_maps_.size();
  }

  const StackMapEntry& GetStackMap(size_t i) const {
    return stack_maps_[i];
  }

  void SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset) {
    stack_maps_[i].native_pc_code_offset =
        CodeOffset::FromOffset(native_pc_offset, instruction_set_);
  }

  // Prepares the stream to fill in a memory region. Must be called before FillIn.
  // Returns the size (in bytes) needed to store this stream.
  size_t PrepareForFillIn();
  void FillInCodeInfo(MemoryRegion region);
  void FillInMethodInfo(MemoryRegion region);

  size_t ComputeMethodInfoSize() const;

 private:
  size_t ComputeDexRegisterLocationCatalogSize() const;
  size_t ComputeDexRegisterMapsSize() const;
  void ComputeInlineInfoEncoding(InlineInfoEncoding* encoding,
                                 size_t dex_register_maps_bytes);

  CodeOffset ComputeMaxNativePcCodeOffset() const;

  // Returns the number of unique stack masks.
  size_t PrepareStackMasks(size_t entry_size_in_bits);

  // Returns the number of unique register masks.
  size_t PrepareRegisterMasks();

  // Prepare and deduplicate method indices.
  void PrepareMethodIndices();

  // Deduplicate entry if possible and return the corresponding index into dex_register_entries_
  // array. If entry is not a duplicate, a new entry is added to dex_register_entries_.
  size_t AddDexRegisterMapEntry(const DexRegisterMapEntry& entry);

  // Return true if the two dex register map entries are equal.
  bool DexRegisterMapEntryEquals(const DexRegisterMapEntry& a, const DexRegisterMapEntry& b) const;

  // Fill in the corresponding entries of a register map.
  void ComputeInvokeInfoEncoding(CodeInfoEncoding* encoding);

  // Returns the index of an entry with the same dex register map as the current_entry,
  // or kNoSameDexMapFound if no such entry exists.
  size_t FindEntryWithTheSameDexMap();
  bool HaveTheSameDexMaps(const StackMapEntry& a, const StackMapEntry& b) const;

  // Fill in the corresponding entries of a register map.
  void FillInDexRegisterMap(DexRegisterMap dex_register_map,
                            uint32_t num_dex_registers,
                            const BitVector& live_dex_registers_mask,
                            uint32_t start_index_in_dex_register_locations) const;

  // Returns the offset for the dex register inside of the dex register location region. See FillIn.
  // Only copies the dex register map if the offset for the entry is not already assigned.
  size_t MaybeCopyDexRegisterMap(DexRegisterMapEntry& entry,
                                 size_t* current_offset,
                                 MemoryRegion dex_register_locations_region);
  void CheckDexRegisterMap(const CodeInfo& code_info,
                           const DexRegisterMap& dex_register_map,
                           size_t num_dex_registers,
                           BitVector* live_dex_registers_mask,
                           size_t dex_register_locations_index) const;
  void CheckCodeInfo(MemoryRegion region) const;

  ScopedArenaAllocator* const allocator_;
  const InstructionSet instruction_set_;
  ScopedArenaVector<StackMapEntry> stack_maps_;

  // A catalog of unique [location_kind, register_value] pairs (per method).
  ScopedArenaVector<DexRegisterLocation> location_catalog_entries_;
  // Map from Dex register location catalog entries to their indices in the
  // location catalog.
  using LocationCatalogEntriesIndices = ScopedArenaHashMap<DexRegisterLocation,
                                                           size_t,
                                                           LocationCatalogEntriesIndicesEmptyFn,
                                                           DexRegisterLocationHashFn>;
  LocationCatalogEntriesIndices location_catalog_entries_indices_;

  // A set of concatenated maps of Dex register locations indices to `location_catalog_entries_`.
  ScopedArenaVector<size_t> dex_register_locations_;
  ScopedArenaVector<InlineInfoEntry> inline_infos_;
  ScopedArenaVector<uint8_t> stack_masks_;
  ScopedArenaVector<uint32_t> register_masks_;
  ScopedArenaVector<uint32_t> method_indices_;
  ScopedArenaVector<DexRegisterMapEntry> dex_register_entries_;
  int stack_mask_max_;
  uint32_t dex_pc_max_;
  uint32_t register_mask_max_;
  size_t number_of_stack_maps_with_inline_info_;

  ScopedArenaSafeMap<uint32_t, ScopedArenaVector<uint32_t>> dex_map_hash_to_stack_map_indices_;

  StackMapEntry current_entry_;
  InlineInfoEntry current_inline_info_;
  ScopedArenaVector<uint8_t> code_info_encoding_;
  size_t needed_size_;
  uint32_t current_dex_register_;
  bool in_inline_frame_;

  static constexpr uint32_t kNoSameDexMapFound = -1;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
