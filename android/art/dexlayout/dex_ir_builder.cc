/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <stdint.h>
#include <vector>

#include "dex_ir_builder.h"
#include "dexlayout.h"

namespace art {
namespace dex_ir {

static void CheckAndSetRemainingOffsets(const DexFile& dex_file,
                                        Collections* collections,
                                        const Options& options);

Header* DexIrBuilder(const DexFile& dex_file,
                     bool eagerly_assign_offsets,
                     const Options& options) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  Header* header = new Header(disk_header.magic_,
                              disk_header.checksum_,
                              disk_header.signature_,
                              disk_header.endian_tag_,
                              disk_header.file_size_,
                              disk_header.header_size_,
                              disk_header.link_size_,
                              disk_header.link_off_,
                              disk_header.data_size_,
                              disk_header.data_off_,
                              dex_file.SupportsDefaultMethods());
  Collections& collections = header->GetCollections();
  collections.SetEagerlyAssignOffsets(eagerly_assign_offsets);
  // Walk the rest of the header fields.
  // StringId table.
  collections.SetStringIdsOffset(disk_header.string_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumStringIds(); ++i) {
    collections.CreateStringId(dex_file, i);
  }
  // TypeId table.
  collections.SetTypeIdsOffset(disk_header.type_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumTypeIds(); ++i) {
    collections.CreateTypeId(dex_file, i);
  }
  // ProtoId table.
  collections.SetProtoIdsOffset(disk_header.proto_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumProtoIds(); ++i) {
    collections.CreateProtoId(dex_file, i);
  }
  // FieldId table.
  collections.SetFieldIdsOffset(disk_header.field_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumFieldIds(); ++i) {
    collections.CreateFieldId(dex_file, i);
  }
  // MethodId table.
  collections.SetMethodIdsOffset(disk_header.method_ids_off_);
  for (uint32_t i = 0; i < dex_file.NumMethodIds(); ++i) {
    collections.CreateMethodId(dex_file, i);
  }
  // ClassDef table.
  collections.SetClassDefsOffset(disk_header.class_defs_off_);
  for (uint32_t i = 0; i < dex_file.NumClassDefs(); ++i) {
    if (!options.class_filter_.empty()) {
      // If the filter is enabled (not empty), filter out classes that don't have a matching
      // descriptor.
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(i);
      const char* descriptor = dex_file.GetClassDescriptor(class_def);
      if (options.class_filter_.find(descriptor) == options.class_filter_.end()) {
        continue;
      }
    }
    collections.CreateClassDef(dex_file, i);
  }
  // MapItem.
  collections.SetMapListOffset(disk_header.map_off_);
  // CallSiteIds and MethodHandleItems.
  collections.CreateCallSitesAndMethodHandles(dex_file);
  CheckAndSetRemainingOffsets(dex_file, &collections, options);

  // Sort the vectors by the map order (same order as the file).
  collections.SortVectorsByMapOrder();

  // Load the link data if it exists.
  collections.SetLinkData(std::vector<uint8_t>(
      dex_file.DataBegin() + dex_file.GetHeader().link_off_,
      dex_file.DataBegin() + dex_file.GetHeader().link_off_ + dex_file.GetHeader().link_size_));

  return header;
}

static void CheckAndSetRemainingOffsets(const DexFile& dex_file,
                                        Collections* collections,
                                        const Options& options) {
  const DexFile::Header& disk_header = dex_file.GetHeader();
  // Read MapItems and validate/set remaining offsets.
  const DexFile::MapList* map = dex_file.GetMapList();
  const uint32_t count = map->size_;
  for (uint32_t i = 0; i < count; ++i) {
    const DexFile::MapItem* item = map->list_ + i;
    switch (item->type_) {
      case DexFile::kDexTypeHeaderItem:
        CHECK_EQ(item->size_, 1u);
        CHECK_EQ(item->offset_, 0u);
        break;
      case DexFile::kDexTypeStringIdItem:
        CHECK_EQ(item->size_, collections->StringIdsSize());
        CHECK_EQ(item->offset_, collections->StringIdsOffset());
        break;
      case DexFile::kDexTypeTypeIdItem:
        CHECK_EQ(item->size_, collections->TypeIdsSize());
        CHECK_EQ(item->offset_, collections->TypeIdsOffset());
        break;
      case DexFile::kDexTypeProtoIdItem:
        CHECK_EQ(item->size_, collections->ProtoIdsSize());
        CHECK_EQ(item->offset_, collections->ProtoIdsOffset());
        break;
      case DexFile::kDexTypeFieldIdItem:
        CHECK_EQ(item->size_, collections->FieldIdsSize());
        CHECK_EQ(item->offset_, collections->FieldIdsOffset());
        break;
      case DexFile::kDexTypeMethodIdItem:
        CHECK_EQ(item->size_, collections->MethodIdsSize());
        CHECK_EQ(item->offset_, collections->MethodIdsOffset());
        break;
      case DexFile::kDexTypeClassDefItem:
        if (options.class_filter_.empty()) {
          // The filter may have removed some classes, this will get fixed up during writing.
          CHECK_EQ(item->size_, collections->ClassDefsSize());
        }
        CHECK_EQ(item->offset_, collections->ClassDefsOffset());
        break;
      case DexFile::kDexTypeCallSiteIdItem:
        CHECK_EQ(item->size_, collections->CallSiteIdsSize());
        CHECK_EQ(item->offset_, collections->CallSiteIdsOffset());
        break;
      case DexFile::kDexTypeMethodHandleItem:
        CHECK_EQ(item->size_, collections->MethodHandleItemsSize());
        CHECK_EQ(item->offset_, collections->MethodHandleItemsOffset());
        break;
      case DexFile::kDexTypeMapList:
        CHECK_EQ(item->size_, 1u);
        CHECK_EQ(item->offset_, disk_header.map_off_);
        break;
      case DexFile::kDexTypeTypeList:
        collections->SetTypeListsOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationSetRefList:
        collections->SetAnnotationSetRefListsOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationSetItem:
        collections->SetAnnotationSetItemsOffset(item->offset_);
        break;
      case DexFile::kDexTypeClassDataItem:
        collections->SetClassDatasOffset(item->offset_);
        break;
      case DexFile::kDexTypeCodeItem:
        collections->SetCodeItemsOffset(item->offset_);
        break;
      case DexFile::kDexTypeStringDataItem:
        collections->SetStringDatasOffset(item->offset_);
        break;
      case DexFile::kDexTypeDebugInfoItem:
        collections->SetDebugInfoItemsOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationItem:
        collections->SetAnnotationItemsOffset(item->offset_);
        collections->AddAnnotationsFromMapListSection(dex_file, item->offset_, item->size_);
        break;
      case DexFile::kDexTypeEncodedArrayItem:
        collections->SetEncodedArrayItemsOffset(item->offset_);
        break;
      case DexFile::kDexTypeAnnotationsDirectoryItem:
        collections->SetAnnotationsDirectoryItemsOffset(item->offset_);
        break;
      default:
        LOG(ERROR) << "Unknown map list item type.";
    }
  }
}

}  // namespace dex_ir
}  // namespace art
