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

#include "elf_file.h"

#include "base/file_utils.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "common_compiler_test.h"
#include "elf_file.h"
#include "elf_file_impl.h"
#include "elf_writer_quick.h"
#include "linker/elf_builder.h"
#include "oat.h"

namespace art {
namespace linker {

class ElfWriterTest : public CommonCompilerTest {
 protected:
  virtual void SetUp() {
    ReserveImageSpace();
    CommonCompilerTest::SetUp();
  }
};

#define EXPECT_ELF_FILE_ADDRESS(ef, expected_value, symbol_name, build_map) \
  do { \
    void* addr = reinterpret_cast<void*>((ef)->FindSymbolAddress(SHT_DYNSYM, \
                                                                 symbol_name, \
                                                                 build_map)); \
    EXPECT_NE(nullptr, addr); \
    if ((expected_value) == nullptr) { \
      (expected_value) = addr; \
    }                        \
    EXPECT_EQ(expected_value, addr); \
    EXPECT_EQ(expected_value, (ef)->FindDynamicSymbolAddress(symbol_name)); \
  } while (false)

TEST_F(ElfWriterTest, dlsym) {
  std::string elf_location = GetCoreOatLocation();
  std::string elf_filename = GetSystemImageFilename(elf_location.c_str(), kRuntimeISA);
  LOG(INFO) << "elf_filename=" << elf_filename;

  UnreserveImageSpace();
  void* dl_oatdata = nullptr;
  void* dl_oatexec = nullptr;
  void* dl_oatlastword = nullptr;

  std::unique_ptr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  ASSERT_TRUE(file.get() != nullptr) << elf_filename;
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              false,
                                              false,
                                              /*low_4gb*/false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", false);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", false);
  }
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              false,
                                              false,
                                              /*low_4gb*/false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatdata, "oatdata", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatexec, "oatexec", true);
    EXPECT_ELF_FILE_ADDRESS(ef, dl_oatlastword, "oatlastword", true);
  }
  {
    uint8_t* base = reinterpret_cast<uint8_t*>(ART_BASE_ADDRESS);
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              false,
                                              true,
                                              /*low_4gb*/false,
                                              &error_msg,
                                              base));
    CHECK(ef.get() != nullptr) << error_msg;
    CHECK(ef->Load(file.get(), false, /*low_4gb*/false, &error_msg)) << error_msg;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatdata) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatdata")));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatexec) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatexec")));
    EXPECT_EQ(reinterpret_cast<uintptr_t>(dl_oatlastword) + reinterpret_cast<uintptr_t>(base),
        reinterpret_cast<uintptr_t>(ef->FindDynamicSymbolAddress("oatlastword")));
  }
}

TEST_F(ElfWriterTest, CheckBuildIdPresent) {
  std::string elf_location = GetCoreOatLocation();
  std::string elf_filename = GetSystemImageFilename(elf_location.c_str(), kRuntimeISA);
  LOG(INFO) << "elf_filename=" << elf_filename;

  std::unique_ptr<File> file(OS::OpenFileForReading(elf_filename.c_str()));
  ASSERT_TRUE(file.get() != nullptr);
  {
    std::string error_msg;
    std::unique_ptr<ElfFile> ef(ElfFile::Open(file.get(),
                                              false,
                                              false,
                                              /*low_4gb*/false,
                                              &error_msg));
    CHECK(ef.get() != nullptr) << error_msg;
    EXPECT_TRUE(ef->HasSection(".note.gnu.build-id"));
  }
}

TEST_F(ElfWriterTest, EncodeDecodeOatPatches) {
  const std::vector<std::vector<uintptr_t>> test_data {
      { 0, 4, 8, 15, 128, 200 },
      { 8, 8 + 127 },
      { 8, 8 + 128 },
      { },
  };
  for (const auto& patch_locations : test_data) {
    constexpr int32_t delta = 0x11235813;

    // Encode patch locations.
    std::vector<uint8_t> oat_patches;
    ElfBuilder<ElfTypes32>::EncodeOatPatches(ArrayRef<const uintptr_t>(patch_locations),
                                             &oat_patches);

    // Create buffer to be patched.
    std::vector<uint8_t> initial_data(256);
    for (size_t i = 0; i < initial_data.size(); i++) {
      initial_data[i] = i;
    }

    // Patch manually.
    std::vector<uint8_t> expected = initial_data;
    for (uintptr_t location : patch_locations) {
      typedef __attribute__((__aligned__(1))) uint32_t UnalignedAddress;
      *reinterpret_cast<UnalignedAddress*>(expected.data() + location) += delta;
    }

    // Decode and apply patch locations.
    std::vector<uint8_t> actual = initial_data;
    ElfFileImpl32::ApplyOatPatches(
        oat_patches.data(), oat_patches.data() + oat_patches.size(), delta,
        actual.data(), actual.data() + actual.size());

    EXPECT_EQ(expected, actual);
  }
}

}  // namespace linker
}  // namespace art
