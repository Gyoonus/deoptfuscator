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
 */

#include <string>
#include <vector>

#include <backtrace/BacktraceMap.h>
#include <gtest/gtest.h>

#include "base/file_utils.h"
#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "dex2oat_environment_test.h"
#include "dexopt_test.h"
#include "gc/space/image_space.h"
#include "mem_map.h"

namespace art {
void DexoptTest::SetUp() {
  ReserveImageSpace();
  Dex2oatEnvironmentTest::SetUp();
}

void DexoptTest::PreRuntimeCreate() {
  std::string error_msg;
  ASSERT_TRUE(PreRelocateImage(GetImageLocation(), &error_msg)) << error_msg;
  ASSERT_TRUE(PreRelocateImage(GetImageLocation2(), &error_msg)) << error_msg;
  UnreserveImageSpace();
}

void DexoptTest::PostRuntimeCreate() {
  ReserveImageSpace();
}

void DexoptTest::GenerateOatForTest(const std::string& dex_location,
                                    const std::string& oat_location_in,
                                    CompilerFilter::Filter filter,
                                    bool relocate,
                                    bool pic,
                                    bool with_alternate_image,
                                    const char* compilation_reason) {
  std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(kRuntimeISA));
  std::string dalvik_cache_tmp = dalvik_cache + ".redirected";
  std::string oat_location = oat_location_in;
  if (!relocate) {
    // Temporarily redirect the dalvik cache so dex2oat doesn't find the
    // relocated image file.
    ASSERT_EQ(0, rename(dalvik_cache.c_str(), dalvik_cache_tmp.c_str())) << strerror(errno);
    // If the oat location is in dalvik cache, replace the cache path with the temporary one.
    size_t pos = oat_location.find(dalvik_cache);
    if (pos != std::string::npos) {
        oat_location = oat_location.replace(pos, dalvik_cache.length(), dalvik_cache_tmp);
    }
  }

  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + oat_location);
  args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
  args.push_back("--runtime-arg");

  // Use -Xnorelocate regardless of the relocate argument.
  // We control relocation by redirecting the dalvik cache when needed
  // rather than use this flag.
  args.push_back("-Xnorelocate");

  ScratchFile profile_file;
  if (CompilerFilter::DependsOnProfile(filter)) {
    args.push_back("--profile-file=" + profile_file.GetFilename());
  }

  if (pic) {
    args.push_back("--compile-pic");
  }

  std::string image_location = GetImageLocation();
  if (with_alternate_image) {
    args.push_back("--boot-image=" + GetImageLocation2());
  }

  if (compilation_reason != nullptr) {
    args.push_back("--compilation-reason=" + std::string(compilation_reason));
  }

  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::Dex2Oat(args, &error_msg)) << error_msg;

  if (!relocate) {
    // Restore the dalvik cache if needed.
    ASSERT_EQ(0, rename(dalvik_cache_tmp.c_str(), dalvik_cache.c_str())) << strerror(errno);
    oat_location = oat_location_in;
  }

  // Verify the odex file was generated as expected.
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   oat_location.c_str(),
                                                   oat_location.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   dex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;
  EXPECT_EQ(pic, odex_file->IsPic());
  EXPECT_EQ(filter, odex_file->GetCompilerFilter());

  std::unique_ptr<ImageHeader> image_header(
          gc::space::ImageSpace::ReadImageHeader(image_location.c_str(),
                                                 kRuntimeISA,
                                                 &error_msg));
  ASSERT_TRUE(image_header != nullptr) << error_msg;
  const OatHeader& oat_header = odex_file->GetOatHeader();
  uint32_t combined_checksum = image_header->GetOatChecksum();

  if (CompilerFilter::DependsOnImageChecksum(filter)) {
    if (with_alternate_image) {
      EXPECT_NE(combined_checksum, oat_header.GetImageFileLocationOatChecksum());
    } else {
      EXPECT_EQ(combined_checksum, oat_header.GetImageFileLocationOatChecksum());
    }
  }

  if (!with_alternate_image) {
    if (CompilerFilter::IsAotCompilationEnabled(filter)) {
      if (relocate) {
        EXPECT_EQ(reinterpret_cast<uintptr_t>(image_header->GetOatDataBegin()),
            oat_header.GetImageFileLocationOatDataBegin());
        EXPECT_EQ(image_header->GetPatchDelta(), oat_header.GetImagePatchDelta());
      } else {
        EXPECT_NE(reinterpret_cast<uintptr_t>(image_header->GetOatDataBegin()),
            oat_header.GetImageFileLocationOatDataBegin());
        EXPECT_NE(image_header->GetPatchDelta(), oat_header.GetImagePatchDelta());
      }
    }
  }
}

void DexoptTest::GenerateOdexForTest(const std::string& dex_location,
                         const std::string& odex_location,
                         CompilerFilter::Filter filter) {
  GenerateOatForTest(dex_location,
                     odex_location,
                     filter,
                     /*relocate*/false,
                     /*pic*/false,
                     /*with_alternate_image*/false);
}

void DexoptTest::GeneratePicOdexForTest(const std::string& dex_location,
                            const std::string& odex_location,
                            CompilerFilter::Filter filter,
                            const char* compilation_reason) {
  GenerateOatForTest(dex_location,
                     odex_location,
                     filter,
                     /*relocate*/false,
                     /*pic*/true,
                     /*with_alternate_image*/false,
                     compilation_reason);
}

void DexoptTest::GenerateOatForTest(const char* dex_location,
                        CompilerFilter::Filter filter,
                        bool relocate,
                        bool pic,
                        bool with_alternate_image) {
  std::string oat_location;
  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
        dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;
  GenerateOatForTest(dex_location,
                     oat_location,
                     filter,
                     relocate,
                     pic,
                     with_alternate_image);
}

void DexoptTest::GenerateOatForTest(const char* dex_location, CompilerFilter::Filter filter) {
  GenerateOatForTest(dex_location,
                     filter,
                     /*relocate*/true,
                     /*pic*/false,
                     /*with_alternate_image*/false);
}

bool DexoptTest::PreRelocateImage(const std::string& image_location, std::string* error_msg) {
  std::string dalvik_cache;
  bool have_android_data;
  bool dalvik_cache_exists;
  bool is_global_cache;
  GetDalvikCache(GetInstructionSetString(kRuntimeISA),
                 true,
                 &dalvik_cache,
                 &have_android_data,
                 &dalvik_cache_exists,
                 &is_global_cache);
  if (!dalvik_cache_exists) {
    *error_msg = "Failed to create dalvik cache";
    return false;
  }

  std::string patchoat = GetAndroidRoot();
  patchoat += kIsDebugBuild ? "/bin/patchoatd" : "/bin/patchoat";

  std::vector<std::string> argv;
  argv.push_back(patchoat);
  argv.push_back("--input-image-location=" + image_location);
  argv.push_back("--output-image-directory=" + dalvik_cache);
  argv.push_back("--instruction-set=" + std::string(GetInstructionSetString(kRuntimeISA)));
  argv.push_back("--base-offset-delta=0x00008000");
  return Exec(argv, error_msg);
}

void DexoptTest::ReserveImageSpace() {
  MemMap::Init();

  // Ensure a chunk of memory is reserved for the image space.
  // The reservation_end includes room for the main space that has to come
  // right after the image in case of the GSS collector.
  uint64_t reservation_start = ART_BASE_ADDRESS;
  uint64_t reservation_end = ART_BASE_ADDRESS + 384 * MB;

  std::unique_ptr<BacktraceMap> map(BacktraceMap::Create(getpid(), true));
  ASSERT_TRUE(map.get() != nullptr) << "Failed to build process map";
  for (BacktraceMap::iterator it = map->begin();
      reservation_start < reservation_end && it != map->end(); ++it) {
    const backtrace_map_t* entry = *it;
    ReserveImageSpaceChunk(reservation_start, std::min(entry->start, reservation_end));
    reservation_start = std::max(reservation_start, entry->end);
  }
  ReserveImageSpaceChunk(reservation_start, reservation_end);
}

void DexoptTest::ReserveImageSpaceChunk(uintptr_t start, uintptr_t end) {
  if (start < end) {
    std::string error_msg;
    image_reservation_.push_back(std::unique_ptr<MemMap>(
        MemMap::MapAnonymous("image reservation",
            reinterpret_cast<uint8_t*>(start), end - start,
            PROT_NONE, false, false, &error_msg)));
    ASSERT_TRUE(image_reservation_.back().get() != nullptr) << error_msg;
    LOG(INFO) << "Reserved space for image " <<
      reinterpret_cast<void*>(image_reservation_.back()->Begin()) << "-" <<
      reinterpret_cast<void*>(image_reservation_.back()->End());
  }
}

void DexoptTest::UnreserveImageSpace() {
  image_reservation_.clear();
}

}  // namespace art
