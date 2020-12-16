/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "mem_map.h"

#include <sys/mman.h>

#include <memory>
#include <random>

#include "base/memory_tool.h"
#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"

namespace art {

class MemMapTest : public CommonRuntimeTest {
 public:
  static uint8_t* BaseBegin(MemMap* mem_map) {
    return reinterpret_cast<uint8_t*>(mem_map->base_begin_);
  }

  static size_t BaseSize(MemMap* mem_map) {
    return mem_map->base_size_;
  }

  static bool IsAddressMapped(void* addr) {
    bool res = msync(addr, 1, MS_SYNC) == 0;
    if (!res && errno != ENOMEM) {
      PLOG(FATAL) << "Unexpected error occurred on msync";
    }
    return res;
  }

  static std::vector<uint8_t> RandomData(size_t size) {
    std::random_device rd;
    std::uniform_int_distribution<uint8_t> dist;
    std::vector<uint8_t> res;
    res.resize(size);
    for (size_t i = 0; i < size; i++) {
      res[i] = dist(rd);
    }
    return res;
  }

  static uint8_t* GetValidMapAddress(size_t size, bool low_4gb) {
    // Find a valid map address and unmap it before returning.
    std::string error_msg;
    std::unique_ptr<MemMap> map(MemMap::MapAnonymous("temp",
                                                     nullptr,
                                                     size,
                                                     PROT_READ,
                                                     low_4gb,
                                                     false,
                                                     &error_msg));
    CHECK(map != nullptr);
    return map->Begin();
  }

  static void RemapAtEndTest(bool low_4gb) {
    std::string error_msg;
    // Cast the page size to size_t.
    const size_t page_size = static_cast<size_t>(kPageSize);
    // Map a two-page memory region.
    MemMap* m0 = MemMap::MapAnonymous("MemMapTest_RemapAtEndTest_map0",
                                      nullptr,
                                      2 * page_size,
                                      PROT_READ | PROT_WRITE,
                                      low_4gb,
                                      false,
                                      &error_msg);
    // Check its state and write to it.
    uint8_t* base0 = m0->Begin();
    ASSERT_TRUE(base0 != nullptr) << error_msg;
    size_t size0 = m0->Size();
    EXPECT_EQ(m0->Size(), 2 * page_size);
    EXPECT_EQ(BaseBegin(m0), base0);
    EXPECT_EQ(BaseSize(m0), size0);
    memset(base0, 42, 2 * page_size);
    // Remap the latter half into a second MemMap.
    MemMap* m1 = m0->RemapAtEnd(base0 + page_size,
                                "MemMapTest_RemapAtEndTest_map1",
                                PROT_READ | PROT_WRITE,
                                &error_msg);
    // Check the states of the two maps.
    EXPECT_EQ(m0->Begin(), base0) << error_msg;
    EXPECT_EQ(m0->Size(), page_size);
    EXPECT_EQ(BaseBegin(m0), base0);
    EXPECT_EQ(BaseSize(m0), page_size);
    uint8_t* base1 = m1->Begin();
    size_t size1 = m1->Size();
    EXPECT_EQ(base1, base0 + page_size);
    EXPECT_EQ(size1, page_size);
    EXPECT_EQ(BaseBegin(m1), base1);
    EXPECT_EQ(BaseSize(m1), size1);
    // Write to the second region.
    memset(base1, 43, page_size);
    // Check the contents of the two regions.
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base0[i], 42);
    }
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base1[i], 43);
    }
    // Unmap the first region.
    delete m0;
    // Make sure the second region is still accessible after the first
    // region is unmapped.
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base1[i], 43);
    }
    delete m1;
  }

  void CommonInit() {
    MemMap::Init();
  }

#if defined(__LP64__) && !defined(__x86_64__)
  static uintptr_t GetLinearScanPos() {
    return MemMap::next_mem_pos_;
  }
#endif
};

#if defined(__LP64__) && !defined(__x86_64__)

#ifdef __BIONIC__
extern uintptr_t CreateStartPos(uint64_t input);
#endif

TEST_F(MemMapTest, Start) {
  CommonInit();
  uintptr_t start = GetLinearScanPos();
  EXPECT_LE(64 * KB, start);
  EXPECT_LT(start, static_cast<uintptr_t>(ART_BASE_ADDRESS));
#ifdef __BIONIC__
  // Test a couple of values. Make sure they are different.
  uintptr_t last = 0;
  for (size_t i = 0; i < 100; ++i) {
    uintptr_t random_start = CreateStartPos(i * kPageSize);
    EXPECT_NE(last, random_start);
    last = random_start;
  }

  // Even on max, should be below ART_BASE_ADDRESS.
  EXPECT_LT(CreateStartPos(~0), static_cast<uintptr_t>(ART_BASE_ADDRESS));
#endif
  // End of test.
}
#endif

// We need mremap to be able to test ReplaceMapping at all
#if HAVE_MREMAP_SYSCALL
TEST_F(MemMapTest, ReplaceMapping_SameSize) {
  std::string error_msg;
  std::unique_ptr<MemMap> dest(MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                                    nullptr,
                                                    kPageSize,
                                                    PROT_READ,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(dest != nullptr);
  MemMap* source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                        nullptr,
                                        kPageSize,
                                        PROT_WRITE | PROT_READ,
                                        false,
                                        false,
                                        &error_msg);
  ASSERT_TRUE(source != nullptr);
  void* source_addr = source->Begin();
  void* dest_addr = dest->Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));

  std::vector<uint8_t> data = RandomData(kPageSize);
  memcpy(source->Begin(), data.data(), data.size());

  ASSERT_TRUE(dest->ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_TRUE(source == nullptr);

  ASSERT_EQ(dest->Size(), static_cast<size_t>(kPageSize));

  ASSERT_EQ(memcmp(dest->Begin(), data.data(), dest->Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_MakeLarger) {
  std::string error_msg;
  std::unique_ptr<MemMap> dest(MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                                    nullptr,
                                                    5 * kPageSize,  // Need to make it larger
                                                                    // initially so we know
                                                                    // there won't be mappings
                                                                    // in the way we we move
                                                                    // source.
                                                    PROT_READ,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(dest != nullptr);
  MemMap* source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                        nullptr,
                                        3 * kPageSize,
                                        PROT_WRITE | PROT_READ,
                                        false,
                                        false,
                                        &error_msg);
  ASSERT_TRUE(source != nullptr);
  uint8_t* source_addr = source->Begin();
  uint8_t* dest_addr = dest->Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));

  // Fill the source with random data.
  std::vector<uint8_t> data = RandomData(3 * kPageSize);
  memcpy(source->Begin(), data.data(), data.size());

  // Make the dest smaller so that we know we'll have space.
  dest->SetSize(kPageSize);

  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_FALSE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_EQ(dest->Size(), static_cast<size_t>(kPageSize));

  ASSERT_TRUE(dest->ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_EQ(dest->Size(), static_cast<size_t>(3 * kPageSize));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_TRUE(source == nullptr);

  ASSERT_EQ(memcmp(dest->Begin(), data.data(), dest->Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_MakeSmaller) {
  std::string error_msg;
  std::unique_ptr<MemMap> dest(MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                                    nullptr,
                                                    3 * kPageSize,
                                                    PROT_READ,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(dest != nullptr);
  MemMap* source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                        nullptr,
                                        kPageSize,
                                        PROT_WRITE | PROT_READ,
                                        false,
                                        false,
                                        &error_msg);
  ASSERT_TRUE(source != nullptr);
  uint8_t* source_addr = source->Begin();
  uint8_t* dest_addr = dest->Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_EQ(dest->Size(), static_cast<size_t>(3 * kPageSize));

  std::vector<uint8_t> data = RandomData(kPageSize);
  memcpy(source->Begin(), data.data(), kPageSize);

  ASSERT_TRUE(dest->ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_EQ(dest->Size(), static_cast<size_t>(kPageSize));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_FALSE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_TRUE(source == nullptr);

  ASSERT_EQ(memcmp(dest->Begin(), data.data(), dest->Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_FailureOverlap) {
  std::string error_msg;
  std::unique_ptr<MemMap> dest(
      MemMap::MapAnonymous(
          "MapAnonymousEmpty-atomic-replace-dest",
          nullptr,
          3 * kPageSize,  // Need to make it larger initially so we know there won't be mappings in
                          // the way we we move source.
          PROT_READ | PROT_WRITE,
          false,
          false,
          &error_msg));
  ASSERT_TRUE(dest != nullptr);
  // Resize down to 1 page so we can remap the rest.
  dest->SetSize(kPageSize);
  // Create source from the last 2 pages
  MemMap* source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                        dest->Begin() + kPageSize,
                                        2 * kPageSize,
                                        PROT_WRITE | PROT_READ,
                                        false,
                                        false,
                                        &error_msg);
  ASSERT_TRUE(source != nullptr);
  MemMap* orig_source = source;
  ASSERT_EQ(dest->Begin() + kPageSize, source->Begin());
  uint8_t* source_addr = source->Begin();
  uint8_t* dest_addr = dest->Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));

  // Fill the source and dest with random data.
  std::vector<uint8_t> data = RandomData(2 * kPageSize);
  memcpy(source->Begin(), data.data(), data.size());
  std::vector<uint8_t> dest_data = RandomData(kPageSize);
  memcpy(dest->Begin(), dest_data.data(), dest_data.size());

  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_EQ(dest->Size(), static_cast<size_t>(kPageSize));

  ASSERT_FALSE(dest->ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_TRUE(source == orig_source);
  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_EQ(source->Size(), data.size());
  ASSERT_EQ(dest->Size(), dest_data.size());

  ASSERT_EQ(memcmp(source->Begin(), data.data(), data.size()), 0);
  ASSERT_EQ(memcmp(dest->Begin(), dest_data.data(), dest_data.size()), 0);

  delete source;
}
#endif  // HAVE_MREMAP_SYSCALL

TEST_F(MemMapTest, MapAnonymousEmpty) {
  CommonInit();
  std::string error_msg;
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousEmpty",
                                                   nullptr,
                                                   0,
                                                   PROT_READ,
                                                   false,
                                                   false,
                                                   &error_msg));
  ASSERT_TRUE(map.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  map.reset(MemMap::MapAnonymous("MapAnonymousEmpty",
                                 nullptr,
                                 kPageSize,
                                 PROT_READ | PROT_WRITE,
                                 false,
                                 false,
                                 &error_msg));
  ASSERT_TRUE(map.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
}

TEST_F(MemMapTest, MapAnonymousFailNullError) {
  CommonInit();
  // Test that we don't crash with a null error_str when mapping at an invalid location.
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousInvalid",
                                                    reinterpret_cast<uint8_t*>(kPageSize),
                                                    0x20000,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    nullptr));
  ASSERT_EQ(nullptr, map.get());
}

#ifdef __LP64__
TEST_F(MemMapTest, MapAnonymousEmpty32bit) {
  CommonInit();
  std::string error_msg;
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousEmpty",
                                                   nullptr,
                                                   kPageSize,
                                                   PROT_READ | PROT_WRITE,
                                                   true,
                                                   false,
                                                   &error_msg));
  ASSERT_TRUE(map.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_LT(reinterpret_cast<uintptr_t>(BaseBegin(map.get())), 1ULL << 32);
}
TEST_F(MemMapTest, MapFile32Bit) {
  CommonInit();
  std::string error_msg;
  ScratchFile scratch_file;
  constexpr size_t kMapSize = kPageSize;
  std::unique_ptr<uint8_t[]> data(new uint8_t[kMapSize]());
  ASSERT_TRUE(scratch_file.GetFile()->WriteFully(&data[0], kMapSize));
  std::unique_ptr<MemMap> map(MemMap::MapFile(/*byte_count*/kMapSize,
                                              PROT_READ,
                                              MAP_PRIVATE,
                                              scratch_file.GetFd(),
                                              /*start*/0,
                                              /*low_4gb*/true,
                                              scratch_file.GetFilename().c_str(),
                                              &error_msg));
  ASSERT_TRUE(map != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map->Size(), kMapSize);
  ASSERT_LT(reinterpret_cast<uintptr_t>(BaseBegin(map.get())), 1ULL << 32);
}
#endif

TEST_F(MemMapTest, MapAnonymousExactAddr) {
  CommonInit();
  std::string error_msg;
  // Find a valid address.
  uint8_t* valid_address = GetValidMapAddress(kPageSize, /*low_4gb*/false);
  // Map at an address that should work, which should succeed.
  std::unique_ptr<MemMap> map0(MemMap::MapAnonymous("MapAnonymous0",
                                                    valid_address,
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map0.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_TRUE(map0->BaseBegin() == valid_address);
  // Map at an unspecified address, which should succeed.
  std::unique_ptr<MemMap> map1(MemMap::MapAnonymous("MapAnonymous1",
                                                    nullptr,
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map1.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_TRUE(map1->BaseBegin() != nullptr);
  // Attempt to map at the same address, which should fail.
  std::unique_ptr<MemMap> map2(MemMap::MapAnonymous("MapAnonymous2",
                                                    reinterpret_cast<uint8_t*>(map1->BaseBegin()),
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map2.get() == nullptr) << error_msg;
  ASSERT_TRUE(!error_msg.empty());
}

TEST_F(MemMapTest, RemapAtEnd) {
  RemapAtEndTest(false);
}

#ifdef __LP64__
TEST_F(MemMapTest, RemapAtEnd32bit) {
  RemapAtEndTest(true);
}
#endif

TEST_F(MemMapTest, MapAnonymousExactAddr32bitHighAddr) {
  // Some MIPS32 hardware (namely the Creator Ci20 development board)
  // cannot allocate in the 2GB-4GB region.
  TEST_DISABLED_FOR_MIPS();

  CommonInit();
  // This test may not work under valgrind.
  if (RUNNING_ON_MEMORY_TOOL == 0) {
    constexpr size_t size = 0x100000;
    // Try all addresses starting from 2GB to 4GB.
    size_t start_addr = 2 * GB;
    std::string error_msg;
    std::unique_ptr<MemMap> map;
    for (; start_addr <= std::numeric_limits<uint32_t>::max() - size; start_addr += size) {
      map.reset(MemMap::MapAnonymous("MapAnonymousExactAddr32bitHighAddr",
                                     reinterpret_cast<uint8_t*>(start_addr),
                                     size,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb*/true,
                                     false,
                                     &error_msg));
      if (map != nullptr) {
        break;
      }
    }
    ASSERT_TRUE(map.get() != nullptr) << error_msg;
    ASSERT_GE(reinterpret_cast<uintptr_t>(map->End()), 2u * GB);
    ASSERT_TRUE(error_msg.empty());
    ASSERT_EQ(BaseBegin(map.get()), reinterpret_cast<void*>(start_addr));
  }
}

TEST_F(MemMapTest, MapAnonymousOverflow) {
  CommonInit();
  std::string error_msg;
  uintptr_t ptr = 0;
  ptr -= kPageSize;  // Now it's close to the top.
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousOverflow",
                                                   reinterpret_cast<uint8_t*>(ptr),
                                                   2 * kPageSize,  // brings it over the top.
                                                   PROT_READ | PROT_WRITE,
                                                   false,
                                                   false,
                                                   &error_msg));
  ASSERT_EQ(nullptr, map.get());
  ASSERT_FALSE(error_msg.empty());
}

#ifdef __LP64__
TEST_F(MemMapTest, MapAnonymousLow4GBExpectedTooHigh) {
  CommonInit();
  std::string error_msg;
  std::unique_ptr<MemMap> map(
      MemMap::MapAnonymous("MapAnonymousLow4GBExpectedTooHigh",
                           reinterpret_cast<uint8_t*>(UINT64_C(0x100000000)),
                           kPageSize,
                           PROT_READ | PROT_WRITE,
                           true,
                           false,
                           &error_msg));
  ASSERT_EQ(nullptr, map.get());
  ASSERT_FALSE(error_msg.empty());
}

TEST_F(MemMapTest, MapAnonymousLow4GBRangeTooHigh) {
  CommonInit();
  std::string error_msg;
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousLow4GBRangeTooHigh",
                                                   reinterpret_cast<uint8_t*>(0xF0000000),
                                                   0x20000000,
                                                   PROT_READ | PROT_WRITE,
                                                   true,
                                                   false,
                                                   &error_msg));
  ASSERT_EQ(nullptr, map.get());
  ASSERT_FALSE(error_msg.empty());
}
#endif

TEST_F(MemMapTest, MapAnonymousReuse) {
  CommonInit();
  std::string error_msg;
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymousReserve",
                                                   nullptr,
                                                   0x20000,
                                                   PROT_READ | PROT_WRITE,
                                                   false,
                                                   false,
                                                   &error_msg));
  ASSERT_NE(nullptr, map.get());
  ASSERT_TRUE(error_msg.empty());
  std::unique_ptr<MemMap> map2(MemMap::MapAnonymous("MapAnonymousReused",
                                                    reinterpret_cast<uint8_t*>(map->BaseBegin()),
                                                    0x10000,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    true,
                                                    &error_msg));
  ASSERT_NE(nullptr, map2.get());
  ASSERT_TRUE(error_msg.empty());
}

TEST_F(MemMapTest, CheckNoGaps) {
  CommonInit();
  std::string error_msg;
  constexpr size_t kNumPages = 3;
  // Map a 3-page mem map.
  std::unique_ptr<MemMap> map(MemMap::MapAnonymous("MapAnonymous0",
                                                   nullptr,
                                                   kPageSize * kNumPages,
                                                   PROT_READ | PROT_WRITE,
                                                   false,
                                                   false,
                                                   &error_msg));
  ASSERT_TRUE(map.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  // Record the base address.
  uint8_t* map_base = reinterpret_cast<uint8_t*>(map->BaseBegin());
  // Unmap it.
  map.reset();

  // Map at the same address, but in page-sized separate mem maps,
  // assuming the space at the address is still available.
  std::unique_ptr<MemMap> map0(MemMap::MapAnonymous("MapAnonymous0",
                                                    map_base,
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map0.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  std::unique_ptr<MemMap> map1(MemMap::MapAnonymous("MapAnonymous1",
                                                    map_base + kPageSize,
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map1.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  std::unique_ptr<MemMap> map2(MemMap::MapAnonymous("MapAnonymous2",
                                                    map_base + kPageSize * 2,
                                                    kPageSize,
                                                    PROT_READ | PROT_WRITE,
                                                    false,
                                                    false,
                                                    &error_msg));
  ASSERT_TRUE(map2.get() != nullptr) << error_msg;
  ASSERT_TRUE(error_msg.empty());

  // One-map cases.
  ASSERT_TRUE(MemMap::CheckNoGaps(map0.get(), map0.get()));
  ASSERT_TRUE(MemMap::CheckNoGaps(map1.get(), map1.get()));
  ASSERT_TRUE(MemMap::CheckNoGaps(map2.get(), map2.get()));

  // Two or three-map cases.
  ASSERT_TRUE(MemMap::CheckNoGaps(map0.get(), map1.get()));
  ASSERT_TRUE(MemMap::CheckNoGaps(map1.get(), map2.get()));
  ASSERT_TRUE(MemMap::CheckNoGaps(map0.get(), map2.get()));

  // Unmap the middle one.
  map1.reset();

  // Should return false now that there's a gap in the middle.
  ASSERT_FALSE(MemMap::CheckNoGaps(map0.get(), map2.get()));
}

TEST_F(MemMapTest, AlignBy) {
  CommonInit();
  std::string error_msg;
  // Cast the page size to size_t.
  const size_t page_size = static_cast<size_t>(kPageSize);
  // Map a region.
  std::unique_ptr<MemMap> m0(MemMap::MapAnonymous("MemMapTest_AlignByTest_map0",
                                                  nullptr,
                                                  14 * page_size,
                                                  PROT_READ | PROT_WRITE,
                                                  false,
                                                  false,
                                                  &error_msg));
  uint8_t* base0 = m0->Begin();
  ASSERT_TRUE(base0 != nullptr) << error_msg;
  ASSERT_EQ(m0->Size(), 14 * page_size);
  ASSERT_EQ(BaseBegin(m0.get()), base0);
  ASSERT_EQ(BaseSize(m0.get()), m0->Size());

  // Break it into several regions by using RemapAtEnd.
  std::unique_ptr<MemMap> m1(m0->RemapAtEnd(base0 + 3 * page_size,
                                            "MemMapTest_AlignByTest_map1",
                                            PROT_READ | PROT_WRITE,
                                            &error_msg));
  uint8_t* base1 = m1->Begin();
  ASSERT_TRUE(base1 != nullptr) << error_msg;
  ASSERT_EQ(base1, base0 + 3 * page_size);
  ASSERT_EQ(m0->Size(), 3 * page_size);

  std::unique_ptr<MemMap> m2(m1->RemapAtEnd(base1 + 4 * page_size,
                                            "MemMapTest_AlignByTest_map2",
                                            PROT_READ | PROT_WRITE,
                                            &error_msg));
  uint8_t* base2 = m2->Begin();
  ASSERT_TRUE(base2 != nullptr) << error_msg;
  ASSERT_EQ(base2, base1 + 4 * page_size);
  ASSERT_EQ(m1->Size(), 4 * page_size);

  std::unique_ptr<MemMap> m3(m2->RemapAtEnd(base2 + 3 * page_size,
                                            "MemMapTest_AlignByTest_map1",
                                            PROT_READ | PROT_WRITE,
                                            &error_msg));
  uint8_t* base3 = m3->Begin();
  ASSERT_TRUE(base3 != nullptr) << error_msg;
  ASSERT_EQ(base3, base2 + 3 * page_size);
  ASSERT_EQ(m2->Size(), 3 * page_size);
  ASSERT_EQ(m3->Size(), 4 * page_size);

  uint8_t* end0 = base0 + m0->Size();
  uint8_t* end1 = base1 + m1->Size();
  uint8_t* end2 = base2 + m2->Size();
  uint8_t* end3 = base3 + m3->Size();

  ASSERT_EQ(static_cast<size_t>(end3 - base0), 14 * page_size);

  if (IsAlignedParam(base0, 2 * page_size)) {
    ASSERT_FALSE(IsAlignedParam(base1, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(base2, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(base3, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(end3, 2 * page_size));
  } else {
    ASSERT_TRUE(IsAlignedParam(base1, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(base2, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(base3, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(end3, 2 * page_size));
  }

  // Align by 2 * page_size;
  m0->AlignBy(2 * page_size);
  m1->AlignBy(2 * page_size);
  m2->AlignBy(2 * page_size);
  m3->AlignBy(2 * page_size);

  EXPECT_TRUE(IsAlignedParam(m0->Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m1->Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m2->Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m3->Begin(), 2 * page_size));

  EXPECT_TRUE(IsAlignedParam(m0->Begin() + m0->Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m1->Begin() + m1->Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m2->Begin() + m2->Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m3->Begin() + m3->Size(), 2 * page_size));

  if (IsAlignedParam(base0, 2 * page_size)) {
    EXPECT_EQ(m0->Begin(), base0);
    EXPECT_EQ(m0->Begin() + m0->Size(), end0 - page_size);
    EXPECT_EQ(m1->Begin(), base1 + page_size);
    EXPECT_EQ(m1->Begin() + m1->Size(), end1 - page_size);
    EXPECT_EQ(m2->Begin(), base2 + page_size);
    EXPECT_EQ(m2->Begin() + m2->Size(), end2);
    EXPECT_EQ(m3->Begin(), base3);
    EXPECT_EQ(m3->Begin() + m3->Size(), end3);
  } else {
    EXPECT_EQ(m0->Begin(), base0 + page_size);
    EXPECT_EQ(m0->Begin() + m0->Size(), end0);
    EXPECT_EQ(m1->Begin(), base1);
    EXPECT_EQ(m1->Begin() + m1->Size(), end1);
    EXPECT_EQ(m2->Begin(), base2);
    EXPECT_EQ(m2->Begin() + m2->Size(), end2 - page_size);
    EXPECT_EQ(m3->Begin(), base3 + page_size);
    EXPECT_EQ(m3->Begin() + m3->Size(), end3 - page_size);
  }
}

}  // namespace art
