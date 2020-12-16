/*
 * Copyright (C) 2017 The Android Open Source Project
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
 *
 * Header file of an in-memory representation of DEX files.
 */

#ifndef ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_
#define ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_

#include <memory>  // For unique_ptr
#include <unordered_map>

#include "base/utils.h"
#include "dex_writer.h"

namespace art {

// Compact dex writer for a single dex.
class CompactDexWriter : public DexWriter {
 public:
  explicit CompactDexWriter(DexLayout* dex_layout);

 protected:
  class Deduper {
   public:
    static const uint32_t kDidNotDedupe = 0;

    // if not enabled, Dedupe will always return kDidNotDedupe.
    explicit Deduper(bool enabled, DexContainer::Section* section);

    // Deduplicate a blob of data that has been written to mem_map.
    // Returns the offset of the deduplicated data or kDidNotDedupe did deduplication did not occur.
    uint32_t Dedupe(uint32_t data_start, uint32_t data_end, uint32_t item_offset);

    // Clear dedupe state to prevent deduplication against existing items in the future.
    void Clear() {
      dedupe_map_.clear();
    }

   private:
    class HashedMemoryRange {
     public:
      uint32_t offset_;
      uint32_t length_;

      class HashEqual {
       public:
        explicit HashEqual(DexContainer::Section* section) : section_(section) {}

        // Equal function.
        bool operator()(const HashedMemoryRange& a, const HashedMemoryRange& b) const {
          if (a.length_ != b.length_) {
            return false;
          }
          const uint8_t* data = Data();
          DCHECK_LE(a.offset_ + a.length_, section_->Size());
          DCHECK_LE(b.offset_ + b.length_, section_->Size());
          return std::equal(data + a.offset_, data + a.offset_ + a.length_, data + b.offset_);
        }

        // Hash function.
        size_t operator()(const HashedMemoryRange& range) const {
          DCHECK_LE(range.offset_ + range.length_, section_->Size());
          return HashBytes(Data() + range.offset_, range.length_);
        }

        ALWAYS_INLINE uint8_t* Data() const {
          return section_->Begin();
        }

       private:
        DexContainer::Section* const section_;
      };
    };

    const bool enabled_;

    // Dedupe map.
    std::unordered_map<HashedMemoryRange,
                       uint32_t,
                       HashedMemoryRange::HashEqual,
                       HashedMemoryRange::HashEqual> dedupe_map_;
  };

  // Handles alignment and deduping of a data section item.
  class ScopedDataSectionItem {
   public:
    ScopedDataSectionItem(Stream* stream, dex_ir::Item* item, size_t alignment, Deduper* deduper);
    ~ScopedDataSectionItem();
    size_t Written() const;

   private:
    Stream* const stream_;
    dex_ir::Item* const item_;
    const size_t alignment_;
    Deduper* deduper_;
    const uint32_t start_offset_;
  };

 public:
  class Container : public DexContainer {
   public:
    Section* GetMainSection() OVERRIDE {
      return &main_section_;
    }

    Section* GetDataSection() OVERRIDE {
      return &data_section_;
    }

    bool IsCompactDexContainer() const OVERRIDE {
      return true;
    }

   private:
    explicit Container(bool dedupe_code_items);

    VectorSection main_section_;
    VectorSection data_section_;
    Deduper code_item_dedupe_;
    Deduper data_item_dedupe_;

    friend class CompactDexWriter;
  };

 protected:
  // Return true if we can generate compact dex for the IR.
  bool CanGenerateCompactDex(std::string* error_msg);

  bool Write(DexContainer* output, std::string* error_msg) OVERRIDE;

  std::unique_ptr<DexContainer> CreateDexContainer() const OVERRIDE;

  void WriteHeader(Stream* stream) OVERRIDE;

  size_t GetHeaderSize() const OVERRIDE;

  uint32_t WriteDebugInfoOffsetTable(Stream* stream);

  void WriteCodeItem(Stream* stream, dex_ir::CodeItem* code_item, bool reserve_only) OVERRIDE;

  void WriteStringData(Stream* stream, dex_ir::StringData* string_data) OVERRIDE;

  void WriteDebugInfoItem(Stream* stream, dex_ir::DebugInfoItem* debug_info) OVERRIDE;

  void SortDebugInfosByMethodIndex();

  CompactDexLevel GetCompactDexLevel() const;

 private:
  // Position in the compact dex file for the debug info table data starts.
  uint32_t debug_info_offsets_pos_ = 0u;

  // Offset into the debug info table data where the lookup table is.
  uint32_t debug_info_offsets_table_offset_ = 0u;

  // Base offset of where debug info starts in the dex file.
  uint32_t debug_info_base_ = 0u;

  // Part of the shared data section owned by this file.
  uint32_t owned_data_begin_ = 0u;
  uint32_t owned_data_end_ = 0u;

  // State for where we are deduping.
  Deduper* code_item_dedupe_ = nullptr;
  Deduper* data_item_dedupe_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CompactDexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_
