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

#include "stack_map.h"

#include "art_method.h"
#include "base/arena_bit_vector.h"
#include "stack_map_stream.h"

#include "gtest/gtest.h"

namespace art {

// Check that the stack mask of given stack map is identical
// to the given bit vector. Returns true if they are same.
static bool CheckStackMask(
    const CodeInfo& code_info,
    const CodeInfoEncoding& encoding,
    const StackMap& stack_map,
    const BitVector& bit_vector) {
  BitMemoryRegion stack_mask = code_info.GetStackMaskOf(encoding, stack_map);
  if (bit_vector.GetNumberOfBits() > encoding.stack_mask.encoding.BitSize()) {
    return false;
  }
  for (size_t i = 0; i < encoding.stack_mask.encoding.BitSize(); ++i) {
    if (stack_mask.LoadBit(i) != bit_vector.IsBitSet(i)) {
      return false;
    }
  }
  return true;
}

using Kind = DexRegisterLocation::Kind;

TEST(StackMapTest, Test1) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, false);
  size_t number_of_dex_registers = 2;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Short location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps(encoding));

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(2u, number_of_catalog_entries);
  DexRegisterLocationCatalog location_catalog = code_info.GetDexRegisterLocationCatalog(encoding);
  // The Dex register location catalog contains:
  // - one 1-byte short Dex register location, and
  // - one 5-byte large Dex register location.
  size_t expected_location_catalog_size = 1u + 5u;
  ASSERT_EQ(expected_location_catalog_size, location_catalog.Size());

  StackMap stack_map = code_info.GetStackMapAt(0, encoding);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0, encoding)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64, encoding)));
  ASSERT_EQ(0u, stack_map.GetDexPc(encoding.stack_map.encoding));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(encoding, stack_map));

  ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask, and
  // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
  size_t expected_dex_register_map_size = 1u + 1u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

  ASSERT_EQ(Kind::kInStack, dex_register_map.GetLocationKind(
                0, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kConstant, dex_register_map.GetLocationKind(
                1, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kInStack, dex_register_map.GetLocationInternalKind(
                0, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kConstantLargeValue, dex_register_map.GetLocationInternalKind(
                1, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(0, dex_register_map.GetStackOffsetInBytes(
                0, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info, encoding));

  size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
      0, number_of_dex_registers, number_of_catalog_entries);
  size_t index1 = dex_register_map.GetLocationCatalogEntryIndex(
      1, number_of_dex_registers, number_of_catalog_entries);
  ASSERT_EQ(0u, index0);
  ASSERT_EQ(1u, index1);
  DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
  DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
  ASSERT_EQ(Kind::kInStack, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
}

TEST(StackMapTest, Test2) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  size_t number_of_dex_registers = 2;
  size_t number_of_dex_registers_in_inline_info = 0;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask1, number_of_dex_registers, 2);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.BeginInlineInfoEntry(&art_method, 3, number_of_dex_registers_in_inline_info);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 2, number_of_dex_registers_in_inline_info);
  stream.EndInlineInfoEntry();
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask2(&allocator, 0, true);
  sp_mask2.SetBit(3);
  sp_mask2.SetBit(8);
  stream.BeginStackMapEntry(1, 128, 0xFF, &sp_mask2, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 18);     // Short location.
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask3(&allocator, 0, true);
  sp_mask3.SetBit(1);
  sp_mask3.SetBit(5);
  stream.BeginStackMapEntry(2, 192, 0xAB, &sp_mask3, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 6);       // Short location.
  stream.AddDexRegisterEntry(Kind::kInRegisterHigh, 8);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask4(&allocator, 0, true);
  sp_mask4.SetBit(6);
  sp_mask4.SetBit(7);
  stream.BeginStackMapEntry(3, 256, 0xCD, &sp_mask4, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);      // Short location, same in stack map 2.
  stream.AddDexRegisterEntry(Kind::kInFpuRegisterHigh, 1);  // Short location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(4u, code_info.GetNumberOfStackMaps(encoding));

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(7u, number_of_catalog_entries);
  DexRegisterLocationCatalog location_catalog = code_info.GetDexRegisterLocationCatalog(encoding);
  // The Dex register location catalog contains:
  // - six 1-byte short Dex register locations, and
  // - one 5-byte large Dex register location.
  size_t expected_location_catalog_size = 6u * 1u + 5u;
  ASSERT_EQ(expected_location_catalog_size, location_catalog.Size());

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0, encoding);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0, encoding)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64, encoding)));
    ASSERT_EQ(0u, stack_map.GetDexPc(encoding.stack_map.encoding));
    ASSERT_EQ(64u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
    ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(encoding, stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInStack, dex_register_map.GetLocationKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kConstant, dex_register_map.GetLocationKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInStack, dex_register_map.GetLocationInternalKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kConstantLargeValue, dex_register_map.GetLocationInternalKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(0, dex_register_map.GetStackOffsetInBytes(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info, encoding));

    size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
        0, number_of_dex_registers, number_of_catalog_entries);
    size_t index1 = dex_register_map.GetLocationCatalogEntryIndex(
        1, number_of_dex_registers, number_of_catalog_entries);
    ASSERT_EQ(0u, index0);
    ASSERT_EQ(1u, index1);
    DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
    ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());

    ASSERT_TRUE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
    InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
    ASSERT_EQ(2u, inline_info.GetDepth(encoding.inline_info.encoding));
    ASSERT_EQ(3u, inline_info.GetDexPcAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_EQ(2u, inline_info.GetDexPcAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_TRUE(inline_info.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_TRUE(inline_info.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 1));
  }

  // Second stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(1, encoding);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1u, encoding)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(128u, encoding)));
    ASSERT_EQ(1u, stack_map.GetDexPc(encoding.stack_map.encoding));
    ASSERT_EQ(128u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
    ASSERT_EQ(0xFFu, code_info.GetRegisterMaskOf(encoding, stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask2));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInRegister, dex_register_map.GetLocationKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map.GetLocationKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInRegister, dex_register_map.GetLocationInternalKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map.GetLocationInternalKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(18, dex_register_map.GetMachineRegister(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(3, dex_register_map.GetMachineRegister(
                  1, number_of_dex_registers, code_info, encoding));

    size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
        0, number_of_dex_registers, number_of_catalog_entries);
    size_t index1 = dex_register_map.GetLocationCatalogEntryIndex(
        1, number_of_dex_registers, number_of_catalog_entries);
    ASSERT_EQ(2u, index0);
    ASSERT_EQ(3u, index1);
    DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetKind());
    ASSERT_EQ(Kind::kInRegister, location0.GetInternalKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetInternalKind());
    ASSERT_EQ(18, location0.GetValue());
    ASSERT_EQ(3, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
  }

  // Third stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(2, encoding);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(2u, encoding)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(192u, encoding)));
    ASSERT_EQ(2u, stack_map.GetDexPc(encoding.stack_map.encoding));
    ASSERT_EQ(192u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
    ASSERT_EQ(0xABu, code_info.GetRegisterMaskOf(encoding, stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask3));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInRegister, dex_register_map.GetLocationKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInRegisterHigh, dex_register_map.GetLocationKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInRegister, dex_register_map.GetLocationInternalKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInRegisterHigh, dex_register_map.GetLocationInternalKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(6, dex_register_map.GetMachineRegister(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(8, dex_register_map.GetMachineRegister(
                  1, number_of_dex_registers, code_info, encoding));

    size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
        0, number_of_dex_registers, number_of_catalog_entries);
    size_t index1 = dex_register_map.GetLocationCatalogEntryIndex(
        1, number_of_dex_registers, number_of_catalog_entries);
    ASSERT_EQ(4u, index0);
    ASSERT_EQ(5u, index1);
    DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInRegisterHigh, location1.GetKind());
    ASSERT_EQ(Kind::kInRegister, location0.GetInternalKind());
    ASSERT_EQ(Kind::kInRegisterHigh, location1.GetInternalKind());
    ASSERT_EQ(6, location0.GetValue());
    ASSERT_EQ(8, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
  }

  // Fourth stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(3, encoding);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(3u, encoding)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(256u, encoding)));
    ASSERT_EQ(3u, stack_map.GetDexPc(encoding.stack_map.encoding));
    ASSERT_EQ(256u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
    ASSERT_EQ(0xCDu, code_info.GetRegisterMaskOf(encoding, stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask4));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(0));
    ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_dex_register_map_size = 1u + 1u;
    ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map.GetLocationKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInFpuRegisterHigh, dex_register_map.GetLocationKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map.GetLocationInternalKind(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInFpuRegisterHigh, dex_register_map.GetLocationInternalKind(
                  1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(3, dex_register_map.GetMachineRegister(
                  0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(1, dex_register_map.GetMachineRegister(
                  1, number_of_dex_registers, code_info, encoding));

    size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
        0, number_of_dex_registers, number_of_catalog_entries);
    size_t index1 = dex_register_map.GetLocationCatalogEntryIndex(
        1, number_of_dex_registers, number_of_catalog_entries);
    ASSERT_EQ(3u, index0);  // Shared with second stack map.
    ASSERT_EQ(6u, index1);
    DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInFpuRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegisterHigh, location1.GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, location0.GetInternalKind());
    ASSERT_EQ(Kind::kInFpuRegisterHigh, location1.GetInternalKind());
    ASSERT_EQ(3, location0.GetValue());
    ASSERT_EQ(1, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
  }
}

TEST(StackMapTest, TestDeduplicateInlineInfoDexRegisterMap) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  const size_t number_of_dex_registers = 2;
  const size_t number_of_dex_registers_in_inline_info = 2;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask1, number_of_dex_registers, 1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.BeginInlineInfoEntry(&art_method, 3, number_of_dex_registers_in_inline_info);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.EndInlineInfoEntry();
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps(encoding));

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(2u, number_of_catalog_entries);
  DexRegisterLocationCatalog location_catalog = code_info.GetDexRegisterLocationCatalog(encoding);
  // The Dex register location catalog contains:
  // - one 1-byte short Dex register locations, and
  // - one 5-byte large Dex register location.
  const size_t expected_location_catalog_size = 1u + 5u;
  ASSERT_EQ(expected_location_catalog_size, location_catalog.Size());

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0, encoding);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0, encoding)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64, encoding)));
    ASSERT_EQ(0u, stack_map.GetDexPc(encoding.stack_map.encoding));
    ASSERT_EQ(64u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
    ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(encoding, stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, encoding, stack_map, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
    DexRegisterMap map(code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers));
    ASSERT_TRUE(map.IsDexRegisterLive(0));
    ASSERT_TRUE(map.IsDexRegisterLive(1));
    ASSERT_EQ(2u, map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
    // The Dex register map contains:
    // - one 1-byte live bit mask, and
    // - one 1-byte set of location catalog entry indices composed of two 2-bit values.
    size_t expected_map_size = 1u + 1u;
    ASSERT_EQ(expected_map_size, map.Size());

    ASSERT_EQ(Kind::kInStack, map.GetLocationKind(0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kConstant,
              map.GetLocationKind(1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kInStack,
              map.GetLocationInternalKind(0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(Kind::kConstantLargeValue,
              map.GetLocationInternalKind(1, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(0, map.GetStackOffsetInBytes(0, number_of_dex_registers, code_info, encoding));
    ASSERT_EQ(-2, map.GetConstant(1, number_of_dex_registers, code_info, encoding));

    const size_t index0 =
        map.GetLocationCatalogEntryIndex(0, number_of_dex_registers, number_of_catalog_entries);
    const size_t index1 =
        map.GetLocationCatalogEntryIndex(1, number_of_dex_registers, number_of_catalog_entries);
    ASSERT_EQ(0u, index0);
    ASSERT_EQ(1u, index1);
    DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
    DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(Kind::kInStack, location0.GetInternalKind());
    ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());

    // Test that the inline info dex register map deduplicated to the same offset as the stack map
    // one.
    ASSERT_TRUE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
    InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
    EXPECT_EQ(inline_info.GetDexRegisterMapOffsetAtDepth(encoding.inline_info.encoding, 0),
              stack_map.GetDexRegisterMapOffset(encoding.stack_map.encoding));
  }
}

TEST(StackMapTest, TestNonLiveDexRegisters) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 2;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kNone, 0);            // No location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps(encoding));

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(1u, number_of_catalog_entries);
  DexRegisterLocationCatalog location_catalog = code_info.GetDexRegisterLocationCatalog(encoding);
  // The Dex register location catalog contains:
  // - one 5-byte large Dex register location.
  size_t expected_location_catalog_size = 5u;
  ASSERT_EQ(expected_location_catalog_size, location_catalog.Size());

  StackMap stack_map = code_info.GetStackMapAt(0, encoding);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0, encoding)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64, encoding)));
  ASSERT_EQ(0u, stack_map.GetDexPc(encoding.stack_map.encoding));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(encoding, stack_map));

  ASSERT_TRUE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
  DexRegisterMap dex_register_map =
      code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);
  ASSERT_FALSE(dex_register_map.IsDexRegisterLive(0));
  ASSERT_TRUE(dex_register_map.IsDexRegisterLive(1));
  ASSERT_EQ(1u, dex_register_map.GetNumberOfLiveDexRegisters(number_of_dex_registers));
  // The Dex register map contains:
  // - one 1-byte live bit mask.
  // No space is allocated for the sole location catalog entry index, as it is useless.
  size_t expected_dex_register_map_size = 1u + 0u;
  ASSERT_EQ(expected_dex_register_map_size, dex_register_map.Size());

  ASSERT_EQ(Kind::kNone, dex_register_map.GetLocationKind(
                0, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kConstant, dex_register_map.GetLocationKind(
                1, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kNone, dex_register_map.GetLocationInternalKind(
                0, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(Kind::kConstantLargeValue, dex_register_map.GetLocationInternalKind(
                1, number_of_dex_registers, code_info, encoding));
  ASSERT_EQ(-2, dex_register_map.GetConstant(1, number_of_dex_registers, code_info, encoding));

  size_t index0 = dex_register_map.GetLocationCatalogEntryIndex(
      0, number_of_dex_registers, number_of_catalog_entries);
  size_t index1 =  dex_register_map.GetLocationCatalogEntryIndex(
      1, number_of_dex_registers, number_of_catalog_entries);
  ASSERT_EQ(DexRegisterLocationCatalog::kNoLocationEntryIndex, index0);
  ASSERT_EQ(0u, index1);
  DexRegisterLocation location0 = location_catalog.GetDexRegisterLocation(index0);
  DexRegisterLocation location1 = location_catalog.GetDexRegisterLocation(index1);
  ASSERT_EQ(Kind::kNone, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(Kind::kNone, location0.GetInternalKind());
  ASSERT_EQ(Kind::kConstantLargeValue, location1.GetInternalKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
}

// Generate a stack map whose dex register offset is
// StackMap::kNoDexRegisterMapSmallEncoding, and ensure we do
// not treat it as kNoDexRegisterMap.
TEST(StackMapTest, DexRegisterMapOffsetOverflow) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 1024;
  // Create the first stack map (and its Dex register map).
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  uint32_t number_of_dex_live_registers_in_dex_register_map_0 = number_of_dex_registers - 8;
  for (uint32_t i = 0; i < number_of_dex_live_registers_in_dex_register_map_0; ++i) {
    // Use two different Dex register locations to populate this map,
    // as using a single value (in the whole CodeInfo object) would
    // make this Dex register mapping data empty (see
    // art::DexRegisterMap::SingleEntrySizeInBits).
    stream.AddDexRegisterEntry(Kind::kConstant, i % 2);  // Short location.
  }
  stream.EndStackMapEntry();
  // Create the second stack map (and its Dex register map).
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  for (uint32_t i = 0; i < number_of_dex_registers; ++i) {
    stream.AddDexRegisterEntry(Kind::kConstant, 0);  // Short location.
  }
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  // The location catalog contains two entries (DexRegisterLocation(kConstant, 0)
  // and DexRegisterLocation(kConstant, 1)), therefore the location catalog index
  // has a size of 1 bit.
  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(2u, number_of_catalog_entries);
  ASSERT_EQ(1u, DexRegisterMap::SingleEntrySizeInBits(number_of_catalog_entries));

  // The first Dex register map contains:
  // - a live register bit mask for 1024 registers (that is, 128 bytes of
  //   data); and
  // - Dex register mapping information for 1016 1-bit Dex (live) register
  //   locations (that is, 127 bytes of data).
  // Hence it has a size of 255 bytes, and therefore...
  ASSERT_EQ(128u, DexRegisterMap::GetLiveBitMaskSize(number_of_dex_registers));
  StackMap stack_map0 = code_info.GetStackMapAt(0, encoding);
  DexRegisterMap dex_register_map0 =
      code_info.GetDexRegisterMapOf(stack_map0, encoding, number_of_dex_registers);
  ASSERT_EQ(127u, dex_register_map0.GetLocationMappingDataSize(number_of_dex_registers,
                                                               number_of_catalog_entries));
  ASSERT_EQ(255u, dex_register_map0.Size());

  StackMap stack_map1 = code_info.GetStackMapAt(1, encoding);
  ASSERT_TRUE(stack_map1.HasDexRegisterMap(encoding.stack_map.encoding));
  // ...the offset of the second Dex register map (relative to the
  // beginning of the Dex register maps region) is 255 (i.e.,
  // kNoDexRegisterMapSmallEncoding).
  ASSERT_NE(stack_map1.GetDexRegisterMapOffset(encoding.stack_map.encoding),
            StackMap::kNoDexRegisterMap);
  ASSERT_EQ(stack_map1.GetDexRegisterMapOffset(encoding.stack_map.encoding), 0xFFu);
}

TEST(StackMapTest, TestShareDexRegisterMap) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 2;
  // First stack map.
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();
  // Second stack map, which should share the same dex register map.
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();
  // Third stack map (doesn't share the dex register map).
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 2);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo ci(region);
  CodeInfoEncoding encoding = ci.ExtractEncoding();

  // Verify first stack map.
  StackMap sm0 = ci.GetStackMapAt(0, encoding);
  DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0, encoding, number_of_dex_registers);
  ASSERT_EQ(0, dex_registers0.GetMachineRegister(0, number_of_dex_registers, ci, encoding));
  ASSERT_EQ(-2, dex_registers0.GetConstant(1, number_of_dex_registers, ci, encoding));

  // Verify second stack map.
  StackMap sm1 = ci.GetStackMapAt(1, encoding);
  DexRegisterMap dex_registers1 = ci.GetDexRegisterMapOf(sm1, encoding, number_of_dex_registers);
  ASSERT_EQ(0, dex_registers1.GetMachineRegister(0, number_of_dex_registers, ci, encoding));
  ASSERT_EQ(-2, dex_registers1.GetConstant(1, number_of_dex_registers, ci, encoding));

  // Verify third stack map.
  StackMap sm2 = ci.GetStackMapAt(2, encoding);
  DexRegisterMap dex_registers2 = ci.GetDexRegisterMapOf(sm2, encoding, number_of_dex_registers);
  ASSERT_EQ(2, dex_registers2.GetMachineRegister(0, number_of_dex_registers, ci, encoding));
  ASSERT_EQ(-2, dex_registers2.GetConstant(1, number_of_dex_registers, ci, encoding));

  // Verify dex register map offsets.
  ASSERT_EQ(sm0.GetDexRegisterMapOffset(encoding.stack_map.encoding),
            sm1.GetDexRegisterMapOffset(encoding.stack_map.encoding));
  ASSERT_NE(sm0.GetDexRegisterMapOffset(encoding.stack_map.encoding),
            sm2.GetDexRegisterMapOffset(encoding.stack_map.encoding));
  ASSERT_NE(sm1.GetDexRegisterMapOffset(encoding.stack_map.encoding),
            sm2.GetDexRegisterMapOffset(encoding.stack_map.encoding));
}

TEST(StackMapTest, TestNoDexRegisterMap) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 0;
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask, number_of_dex_registers, 0);
  stream.EndStackMapEntry();

  number_of_dex_registers = 1;
  stream.BeginStackMapEntry(1, 68, 0x4, &sp_mask, number_of_dex_registers, 0);
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps(encoding));

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries(encoding);
  ASSERT_EQ(0u, number_of_catalog_entries);
  DexRegisterLocationCatalog location_catalog = code_info.GetDexRegisterLocationCatalog(encoding);
  ASSERT_EQ(0u, location_catalog.Size());

  StackMap stack_map = code_info.GetStackMapAt(0, encoding);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0, encoding)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64, encoding)));
  ASSERT_EQ(0u, stack_map.GetDexPc(encoding.stack_map.encoding));
  ASSERT_EQ(64u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(encoding, stack_map));

  ASSERT_FALSE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
  ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));

  stack_map = code_info.GetStackMapAt(1, encoding);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1, encoding)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(68, encoding)));
  ASSERT_EQ(1u, stack_map.GetDexPc(encoding.stack_map.encoding));
  ASSERT_EQ(68u, stack_map.GetNativePcOffset(encoding.stack_map.encoding, kRuntimeISA));
  ASSERT_EQ(0x4u, code_info.GetRegisterMaskOf(encoding, stack_map));

  ASSERT_FALSE(stack_map.HasDexRegisterMap(encoding.stack_map.encoding));
  ASSERT_FALSE(stack_map.HasInlineInfo(encoding.stack_map.encoding));
}

TEST(StackMapTest, InlineTest) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);

  // First stack map.
  stream.BeginStackMapEntry(0, 64, 0x3, &sp_mask1, 2, 2);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);

  stream.BeginInlineInfoEntry(&art_method, 2, 1);
  stream.AddDexRegisterEntry(Kind::kInStack, 8);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 3, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 16);
  stream.AddDexRegisterEntry(Kind::kConstant, 20);
  stream.AddDexRegisterEntry(Kind::kInRegister, 15);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Second stack map.
  stream.BeginStackMapEntry(2, 22, 0x3, &sp_mask1, 2, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(&art_method, 2, 1);
  stream.AddDexRegisterEntry(Kind::kInStack, 12);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 3, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 80);
  stream.AddDexRegisterEntry(Kind::kConstant, 10);
  stream.AddDexRegisterEntry(Kind::kInRegister, 5);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 5, 0);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Third stack map.
  stream.BeginStackMapEntry(4, 56, 0x3, &sp_mask1, 2, 0);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);
  stream.EndStackMapEntry();

  // Fourth stack map.
  stream.BeginStackMapEntry(6, 78, 0x3, &sp_mask1, 2, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(&art_method, 2, 0);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 5, 1);
  stream.AddDexRegisterEntry(Kind::kInRegister, 2);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 10, 2);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 3);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo ci(region);
  CodeInfoEncoding encoding = ci.ExtractEncoding();

  {
    // Verify first stack map.
    StackMap sm0 = ci.GetStackMapAt(0, encoding);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0, encoding, 2);
    ASSERT_EQ(0, dex_registers0.GetStackOffsetInBytes(0, 2, ci, encoding));
    ASSERT_EQ(4, dex_registers0.GetConstant(1, 2, ci, encoding));

    InlineInfo if0 = ci.GetInlineInfoOf(sm0, encoding);
    ASSERT_EQ(2u, if0.GetDepth(encoding.inline_info.encoding));
    ASSERT_EQ(2u, if0.GetDexPcAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_TRUE(if0.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_EQ(3u, if0.GetDexPcAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_TRUE(if0.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 1));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(0, if0, encoding, 1);
    ASSERT_EQ(8, dex_registers1.GetStackOffsetInBytes(0, 1, ci, encoding));

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(1, if0, encoding, 3);
    ASSERT_EQ(16, dex_registers2.GetStackOffsetInBytes(0, 3, ci, encoding));
    ASSERT_EQ(20, dex_registers2.GetConstant(1, 3, ci, encoding));
    ASSERT_EQ(15, dex_registers2.GetMachineRegister(2, 3, ci, encoding));
  }

  {
    // Verify second stack map.
    StackMap sm1 = ci.GetStackMapAt(1, encoding);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm1, encoding, 2);
    ASSERT_EQ(56, dex_registers0.GetStackOffsetInBytes(0, 2, ci, encoding));
    ASSERT_EQ(0, dex_registers0.GetConstant(1, 2, ci, encoding));

    InlineInfo if1 = ci.GetInlineInfoOf(sm1, encoding);
    ASSERT_EQ(3u, if1.GetDepth(encoding.inline_info.encoding));
    ASSERT_EQ(2u, if1.GetDexPcAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_TRUE(if1.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_EQ(3u, if1.GetDexPcAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_TRUE(if1.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_EQ(5u, if1.GetDexPcAtDepth(encoding.inline_info.encoding, 2));
    ASSERT_TRUE(if1.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 2));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(0, if1, encoding, 1);
    ASSERT_EQ(12, dex_registers1.GetStackOffsetInBytes(0, 1, ci, encoding));

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(1, if1, encoding, 3);
    ASSERT_EQ(80, dex_registers2.GetStackOffsetInBytes(0, 3, ci, encoding));
    ASSERT_EQ(10, dex_registers2.GetConstant(1, 3, ci, encoding));
    ASSERT_EQ(5, dex_registers2.GetMachineRegister(2, 3, ci, encoding));

    ASSERT_FALSE(if1.HasDexRegisterMapAtDepth(encoding.inline_info.encoding, 2));
  }

  {
    // Verify third stack map.
    StackMap sm2 = ci.GetStackMapAt(2, encoding);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm2, encoding, 2);
    ASSERT_FALSE(dex_registers0.IsDexRegisterLive(0));
    ASSERT_EQ(4, dex_registers0.GetConstant(1, 2, ci, encoding));
    ASSERT_FALSE(sm2.HasInlineInfo(encoding.stack_map.encoding));
  }

  {
    // Verify fourth stack map.
    StackMap sm3 = ci.GetStackMapAt(3, encoding);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm3, encoding, 2);
    ASSERT_EQ(56, dex_registers0.GetStackOffsetInBytes(0, 2, ci, encoding));
    ASSERT_EQ(0, dex_registers0.GetConstant(1, 2, ci, encoding));

    InlineInfo if2 = ci.GetInlineInfoOf(sm3, encoding);
    ASSERT_EQ(3u, if2.GetDepth(encoding.inline_info.encoding));
    ASSERT_EQ(2u, if2.GetDexPcAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_TRUE(if2.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 0));
    ASSERT_EQ(5u, if2.GetDexPcAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_TRUE(if2.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 1));
    ASSERT_EQ(10u, if2.GetDexPcAtDepth(encoding.inline_info.encoding, 2));
    ASSERT_TRUE(if2.EncodesArtMethodAtDepth(encoding.inline_info.encoding, 2));

    ASSERT_FALSE(if2.HasDexRegisterMapAtDepth(encoding.inline_info.encoding, 0));

    DexRegisterMap dex_registers1 = ci.GetDexRegisterMapAtDepth(1, if2, encoding, 1);
    ASSERT_EQ(2, dex_registers1.GetMachineRegister(0, 1, ci, encoding));

    DexRegisterMap dex_registers2 = ci.GetDexRegisterMapAtDepth(2, if2, encoding, 2);
    ASSERT_FALSE(dex_registers2.IsDexRegisterLive(0));
    ASSERT_EQ(3, dex_registers2.GetMachineRegister(1, 2, ci, encoding));
  }
}

TEST(StackMapTest, CodeOffsetTest) {
  // Test minimum alignments, encoding, and decoding.
  CodeOffset offset_thumb2 =
      CodeOffset::FromOffset(kThumb2InstructionAlignment, InstructionSet::kThumb2);
  CodeOffset offset_arm64 =
      CodeOffset::FromOffset(kArm64InstructionAlignment, InstructionSet::kArm64);
  CodeOffset offset_x86 =
      CodeOffset::FromOffset(kX86InstructionAlignment, InstructionSet::kX86);
  CodeOffset offset_x86_64 =
      CodeOffset::FromOffset(kX86_64InstructionAlignment, InstructionSet::kX86_64);
  CodeOffset offset_mips =
      CodeOffset::FromOffset(kMipsInstructionAlignment, InstructionSet::kMips);
  CodeOffset offset_mips64 =
      CodeOffset::FromOffset(kMips64InstructionAlignment, InstructionSet::kMips64);
  EXPECT_EQ(offset_thumb2.Uint32Value(InstructionSet::kThumb2), kThumb2InstructionAlignment);
  EXPECT_EQ(offset_arm64.Uint32Value(InstructionSet::kArm64), kArm64InstructionAlignment);
  EXPECT_EQ(offset_x86.Uint32Value(InstructionSet::kX86), kX86InstructionAlignment);
  EXPECT_EQ(offset_x86_64.Uint32Value(InstructionSet::kX86_64), kX86_64InstructionAlignment);
  EXPECT_EQ(offset_mips.Uint32Value(InstructionSet::kMips), kMipsInstructionAlignment);
  EXPECT_EQ(offset_mips64.Uint32Value(InstructionSet::kMips64), kMips64InstructionAlignment);
}

TEST(StackMapTest, TestDeduplicateStackMask) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, true);
  sp_mask.SetBit(1);
  sp_mask.SetBit(4);
  stream.BeginStackMapEntry(0, 4, 0x3, &sp_mask, 0, 0);
  stream.EndStackMapEntry();
  stream.BeginStackMapEntry(0, 8, 0x3, &sp_mask, 0, 0);
  stream.EndStackMapEntry();

  size_t size = stream.PrepareForFillIn();
  void* memory = allocator.Alloc(size, kArenaAllocMisc);
  MemoryRegion region(memory, size);
  stream.FillInCodeInfo(region);

  CodeInfo code_info(region);
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps(encoding));

  StackMap stack_map1 = code_info.GetStackMapForNativePcOffset(4, encoding);
  StackMap stack_map2 = code_info.GetStackMapForNativePcOffset(8, encoding);
  EXPECT_EQ(stack_map1.GetStackMaskIndex(encoding.stack_map.encoding),
            stack_map2.GetStackMaskIndex(encoding.stack_map.encoding));
}

TEST(StackMapTest, TestInvokeInfo) {
  ArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);

  ArenaBitVector sp_mask(&allocator, 0, true);
  sp_mask.SetBit(1);
  stream.BeginStackMapEntry(0, 4, 0x3, &sp_mask, 0, 0);
  stream.AddInvoke(kSuper, 1);
  stream.EndStackMapEntry();
  stream.BeginStackMapEntry(0, 8, 0x3, &sp_mask, 0, 0);
  stream.AddInvoke(kStatic, 3);
  stream.EndStackMapEntry();
  stream.BeginStackMapEntry(0, 16, 0x3, &sp_mask, 0, 0);
  stream.AddInvoke(kDirect, 65535);
  stream.EndStackMapEntry();

  const size_t code_info_size = stream.PrepareForFillIn();
  MemoryRegion code_info_region(allocator.Alloc(code_info_size, kArenaAllocMisc), code_info_size);
  stream.FillInCodeInfo(code_info_region);

  const size_t method_info_size = stream.ComputeMethodInfoSize();
  MemoryRegion method_info_region(allocator.Alloc(method_info_size, kArenaAllocMisc),
                                  method_info_size);
  stream.FillInMethodInfo(method_info_region);

  CodeInfo code_info(code_info_region);
  MethodInfo method_info(method_info_region.begin());
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  ASSERT_EQ(3u, code_info.GetNumberOfStackMaps(encoding));

  InvokeInfo invoke1(code_info.GetInvokeInfoForNativePcOffset(4, encoding));
  InvokeInfo invoke2(code_info.GetInvokeInfoForNativePcOffset(8, encoding));
  InvokeInfo invoke3(code_info.GetInvokeInfoForNativePcOffset(16, encoding));
  InvokeInfo invoke_invalid(code_info.GetInvokeInfoForNativePcOffset(12, encoding));
  EXPECT_FALSE(invoke_invalid.IsValid());  // No entry for that index.
  EXPECT_TRUE(invoke1.IsValid());
  EXPECT_TRUE(invoke2.IsValid());
  EXPECT_TRUE(invoke3.IsValid());
  EXPECT_EQ(invoke1.GetInvokeType(encoding.invoke_info.encoding), kSuper);
  EXPECT_EQ(invoke1.GetMethodIndex(encoding.invoke_info.encoding, method_info), 1u);
  EXPECT_EQ(invoke1.GetNativePcOffset(encoding.invoke_info.encoding, kRuntimeISA), 4u);
  EXPECT_EQ(invoke2.GetInvokeType(encoding.invoke_info.encoding), kStatic);
  EXPECT_EQ(invoke2.GetMethodIndex(encoding.invoke_info.encoding, method_info), 3u);
  EXPECT_EQ(invoke2.GetNativePcOffset(encoding.invoke_info.encoding, kRuntimeISA), 8u);
  EXPECT_EQ(invoke3.GetInvokeType(encoding.invoke_info.encoding), kDirect);
  EXPECT_EQ(invoke3.GetMethodIndex(encoding.invoke_info.encoding, method_info), 65535u);
  EXPECT_EQ(invoke3.GetNativePcOffset(encoding.invoke_info.encoding, kRuntimeISA), 16u);
}

}  // namespace art
