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

#include "image_space.h"

#include <lz4.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include <random>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/macros.h"
#include "base/os.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file_loader.h"
#include "exec_utils.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "image-inl.h"
#include "image_space_fs.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "oat_file.h"
#include "runtime.h"
#include "space-inl.h"

namespace art {
namespace gc {
namespace space {

using android::base::StringAppendF;
using android::base::StringPrintf;

Atomic<uint32_t> ImageSpace::bitmap_index_(0);

ImageSpace::ImageSpace(const std::string& image_filename,
                       const char* image_location,
                       MemMap* mem_map,
                       accounting::ContinuousSpaceBitmap* live_bitmap,
                       uint8_t* end)
    : MemMapSpace(image_filename,
                  mem_map,
                  mem_map->Begin(),
                  end,
                  end,
                  kGcRetentionPolicyNeverCollect),
      oat_file_non_owned_(nullptr),
      image_location_(image_location) {
  DCHECK(live_bitmap != nullptr);
  live_bitmap_.reset(live_bitmap);
}

static int32_t ChooseRelocationOffsetDelta(int32_t min_delta, int32_t max_delta) {
  CHECK_ALIGNED(min_delta, kPageSize);
  CHECK_ALIGNED(max_delta, kPageSize);
  CHECK_LT(min_delta, max_delta);

  int32_t r = GetRandomNumber<int32_t>(min_delta, max_delta);
  if (r % 2 == 0) {
    r = RoundUp(r, kPageSize);
  } else {
    r = RoundDown(r, kPageSize);
  }
  CHECK_LE(min_delta, r);
  CHECK_GE(max_delta, r);
  CHECK_ALIGNED(r, kPageSize);
  return r;
}

static int32_t ChooseRelocationOffsetDelta() {
  return ChooseRelocationOffsetDelta(ART_BASE_ADDRESS_MIN_DELTA, ART_BASE_ADDRESS_MAX_DELTA);
}

static bool GenerateImage(const std::string& image_filename,
                          InstructionSet image_isa,
                          std::string* error_msg) {
  const std::string boot_class_path_string(Runtime::Current()->GetBootClassPathString());
  std::vector<std::string> boot_class_path;
  Split(boot_class_path_string, ':', &boot_class_path);
  if (boot_class_path.empty()) {
    *error_msg = "Failed to generate image because no boot class path specified";
    return false;
  }
  // We should clean up so we are more likely to have room for the image.
  if (Runtime::Current()->IsZygote()) {
    LOG(INFO) << "Pruning dalvik-cache since we are generating an image and will need to recompile";
    PruneDalvikCache(image_isa);
  }

  std::vector<std::string> arg_vector;

  std::string dex2oat(Runtime::Current()->GetCompilerExecutable());
  arg_vector.push_back(dex2oat);

  std::string image_option_string("--image=");
  image_option_string += image_filename;
  arg_vector.push_back(image_option_string);

  for (size_t i = 0; i < boot_class_path.size(); i++) {
    arg_vector.push_back(std::string("--dex-file=") + boot_class_path[i]);
  }

  std::string oat_file_option_string("--oat-file=");
  oat_file_option_string += ImageHeader::GetOatLocationFromImageLocation(image_filename);
  arg_vector.push_back(oat_file_option_string);

  // Note: we do not generate a fully debuggable boot image so we do not pass the
  // compiler flag --debuggable here.

  Runtime::Current()->AddCurrentRuntimeFeaturesAsDex2OatArguments(&arg_vector);
  CHECK_EQ(image_isa, kRuntimeISA)
      << "We should always be generating an image for the current isa.";

  int32_t base_offset = ChooseRelocationOffsetDelta();
  LOG(INFO) << "Using an offset of 0x" << std::hex << base_offset << " from default "
            << "art base address of 0x" << std::hex << ART_BASE_ADDRESS;
  arg_vector.push_back(StringPrintf("--base=0x%x", ART_BASE_ADDRESS + base_offset));

  if (!kIsTargetBuild) {
    arg_vector.push_back("--host");
  }

  const std::vector<std::string>& compiler_options = Runtime::Current()->GetImageCompilerOptions();
  for (size_t i = 0; i < compiler_options.size(); ++i) {
    arg_vector.push_back(compiler_options[i].c_str());
  }

  std::string command_line(android::base::Join(arg_vector, ' '));
  LOG(INFO) << "GenerateImage: " << command_line;
  return Exec(arg_vector, error_msg);
}

static bool FindImageFilenameImpl(const char* image_location,
                                  const InstructionSet image_isa,
                                  bool* has_system,
                                  std::string* system_filename,
                                  bool* dalvik_cache_exists,
                                  std::string* dalvik_cache,
                                  bool* is_global_cache,
                                  bool* has_cache,
                                  std::string* cache_filename) {
  DCHECK(dalvik_cache != nullptr);

  *has_system = false;
  *has_cache = false;
  // image_location = /system/framework/boot.art
  // system_image_location = /system/framework/<image_isa>/boot.art
  std::string system_image_filename(GetSystemImageFilename(image_location, image_isa));
  if (OS::FileExists(system_image_filename.c_str())) {
    *system_filename = system_image_filename;
    *has_system = true;
  }

  bool have_android_data = false;
  *dalvik_cache_exists = false;
  GetDalvikCache(GetInstructionSetString(image_isa),
                 true,
                 dalvik_cache,
                 &have_android_data,
                 dalvik_cache_exists,
                 is_global_cache);

  if (have_android_data && *dalvik_cache_exists) {
    // Always set output location even if it does not exist,
    // so that the caller knows where to create the image.
    //
    // image_location = /system/framework/boot.art
    // *image_filename = /data/dalvik-cache/<image_isa>/boot.art
    std::string error_msg;
    if (!GetDalvikCacheFilename(image_location,
                                dalvik_cache->c_str(),
                                cache_filename,
                                &error_msg)) {
      LOG(WARNING) << error_msg;
      return *has_system;
    }
    *has_cache = OS::FileExists(cache_filename->c_str());
  }
  return *has_system || *has_cache;
}

bool ImageSpace::FindImageFilename(const char* image_location,
                                   const InstructionSet image_isa,
                                   std::string* system_filename,
                                   bool* has_system,
                                   std::string* cache_filename,
                                   bool* dalvik_cache_exists,
                                   bool* has_cache,
                                   bool* is_global_cache) {
  std::string dalvik_cache_unused;
  return FindImageFilenameImpl(image_location,
                               image_isa,
                               has_system,
                               system_filename,
                               dalvik_cache_exists,
                               &dalvik_cache_unused,
                               is_global_cache,
                               has_cache,
                               cache_filename);
}

static bool ReadSpecificImageHeader(const char* filename, ImageHeader* image_header) {
    std::unique_ptr<File> image_file(OS::OpenFileForReading(filename));
    if (image_file.get() == nullptr) {
      return false;
    }
    const bool success = image_file->ReadFully(image_header, sizeof(ImageHeader));
    if (!success || !image_header->IsValid()) {
      return false;
    }
    return true;
}

// Relocate the image at image_location to dest_filename and relocate it by a random amount.
static bool RelocateImage(const char* image_location,
                          const char* dest_directory,
                          InstructionSet isa,
                          std::string* error_msg) {
  // We should clean up so we are more likely to have room for the image.
  if (Runtime::Current()->IsZygote()) {
    LOG(INFO) << "Pruning dalvik-cache since we are relocating an image and will need to recompile";
    PruneDalvikCache(isa);
  }

  std::string patchoat(Runtime::Current()->GetPatchoatExecutable());

  std::string input_image_location_arg("--input-image-location=");
  input_image_location_arg += image_location;

  std::string output_image_directory_arg("--output-image-directory=");
  output_image_directory_arg += dest_directory;

  std::string instruction_set_arg("--instruction-set=");
  instruction_set_arg += GetInstructionSetString(isa);

  std::string base_offset_arg("--base-offset-delta=");
  StringAppendF(&base_offset_arg, "%d", ChooseRelocationOffsetDelta());

  std::vector<std::string> argv;
  argv.push_back(patchoat);

  argv.push_back(input_image_location_arg);
  argv.push_back(output_image_directory_arg);

  argv.push_back(instruction_set_arg);
  argv.push_back(base_offset_arg);

  std::string command_line(android::base::Join(argv, ' '));
  LOG(INFO) << "RelocateImage: " << command_line;
  return Exec(argv, error_msg);
}

static bool VerifyImage(const char* image_location,
                        const char* dest_directory,
                        InstructionSet isa,
                        std::string* error_msg) {
  std::string patchoat(Runtime::Current()->GetPatchoatExecutable());

  std::string input_image_location_arg("--input-image-location=");
  input_image_location_arg += image_location;

  std::string output_image_directory_arg("--output-image-directory=");
  output_image_directory_arg += dest_directory;

  std::string instruction_set_arg("--instruction-set=");
  instruction_set_arg += GetInstructionSetString(isa);

  std::vector<std::string> argv;
  argv.push_back(patchoat);

  argv.push_back(input_image_location_arg);
  argv.push_back(output_image_directory_arg);

  argv.push_back(instruction_set_arg);

  argv.push_back("--verify");

  std::string command_line(android::base::Join(argv, ' '));
  LOG(INFO) << "VerifyImage: " << command_line;
  return Exec(argv, error_msg);
}

static ImageHeader* ReadSpecificImageHeader(const char* filename, std::string* error_msg) {
  std::unique_ptr<ImageHeader> hdr(new ImageHeader);
  if (!ReadSpecificImageHeader(filename, hdr.get())) {
    *error_msg = StringPrintf("Unable to read image header for %s", filename);
    return nullptr;
  }
  return hdr.release();
}

ImageHeader* ImageSpace::ReadImageHeader(const char* image_location,
                                         const InstructionSet image_isa,
                                         std::string* error_msg) {
  std::string system_filename;
  bool has_system = false;
  std::string cache_filename;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = false;
  if (FindImageFilename(image_location, image_isa, &system_filename, &has_system,
                        &cache_filename, &dalvik_cache_exists, &has_cache, &is_global_cache)) {
    if (Runtime::Current()->ShouldRelocate()) {
      if (has_system && has_cache) {
        std::unique_ptr<ImageHeader> sys_hdr(new ImageHeader);
        std::unique_ptr<ImageHeader> cache_hdr(new ImageHeader);
        if (!ReadSpecificImageHeader(system_filename.c_str(), sys_hdr.get())) {
          *error_msg = StringPrintf("Unable to read image header for %s at %s",
                                    image_location, system_filename.c_str());
          return nullptr;
        }
        if (!ReadSpecificImageHeader(cache_filename.c_str(), cache_hdr.get())) {
          *error_msg = StringPrintf("Unable to read image header for %s at %s",
                                    image_location, cache_filename.c_str());
          return nullptr;
        }
        if (sys_hdr->GetOatChecksum() != cache_hdr->GetOatChecksum()) {
          *error_msg = StringPrintf("Unable to find a relocated version of image file %s",
                                    image_location);
          return nullptr;
        }
        return cache_hdr.release();
      } else if (!has_cache) {
        *error_msg = StringPrintf("Unable to find a relocated version of image file %s",
                                  image_location);
        return nullptr;
      } else if (!has_system && has_cache) {
        // This can probably just use the cache one.
        return ReadSpecificImageHeader(cache_filename.c_str(), error_msg);
      }
    } else {
      // We don't want to relocate, Just pick the appropriate one if we have it and return.
      if (has_system && has_cache) {
        // We want the cache if the checksum matches, otherwise the system.
        std::unique_ptr<ImageHeader> system(ReadSpecificImageHeader(system_filename.c_str(),
                                                                    error_msg));
        std::unique_ptr<ImageHeader> cache(ReadSpecificImageHeader(cache_filename.c_str(),
                                                                   error_msg));
        if (system.get() == nullptr ||
            (cache.get() != nullptr && cache->GetOatChecksum() == system->GetOatChecksum())) {
          return cache.release();
        } else {
          return system.release();
        }
      } else if (has_system) {
        return ReadSpecificImageHeader(system_filename.c_str(), error_msg);
      } else if (has_cache) {
        return ReadSpecificImageHeader(cache_filename.c_str(), error_msg);
      }
    }
  }

  *error_msg = StringPrintf("Unable to find image file for %s", image_location);
  return nullptr;
}

static bool ChecksumsMatch(const char* image_a, const char* image_b, std::string* error_msg) {
  DCHECK(error_msg != nullptr);

  ImageHeader hdr_a;
  ImageHeader hdr_b;

  if (!ReadSpecificImageHeader(image_a, &hdr_a)) {
    *error_msg = StringPrintf("Cannot read header of %s", image_a);
    return false;
  }
  if (!ReadSpecificImageHeader(image_b, &hdr_b)) {
    *error_msg = StringPrintf("Cannot read header of %s", image_b);
    return false;
  }

  if (hdr_a.GetOatChecksum() != hdr_b.GetOatChecksum()) {
    *error_msg = StringPrintf("Checksum mismatch: %u(%s) vs %u(%s)",
                              hdr_a.GetOatChecksum(),
                              image_a,
                              hdr_b.GetOatChecksum(),
                              image_b);
    return false;
  }

  return true;
}

static bool CanWriteToDalvikCache(const InstructionSet isa) {
  const std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(isa));
  if (access(dalvik_cache.c_str(), O_RDWR) == 0) {
    return true;
  } else if (errno != EACCES) {
    PLOG(WARNING) << "CanWriteToDalvikCache returned error other than EACCES";
  }
  return false;
}

static bool ImageCreationAllowed(bool is_global_cache,
                                 const InstructionSet isa,
                                 std::string* error_msg) {
  // Anyone can write into a "local" cache.
  if (!is_global_cache) {
    return true;
  }

  // Only the zygote running as root is allowed to create the global boot image.
  // If the zygote is running as non-root (and cannot write to the dalvik-cache),
  // then image creation is not allowed..
  if (Runtime::Current()->IsZygote()) {
    return CanWriteToDalvikCache(isa);
  }

  *error_msg = "Only the zygote can create the global boot image.";
  return false;
}

void ImageSpace::VerifyImageAllocations() {
  uint8_t* current = Begin() + RoundUp(sizeof(ImageHeader), kObjectAlignment);
  while (current < End()) {
    CHECK_ALIGNED(current, kObjectAlignment);
    auto* obj = reinterpret_cast<mirror::Object*>(current);
    CHECK(obj->GetClass() != nullptr) << "Image object at address " << obj << " has null class";
    CHECK(live_bitmap_->Test(obj)) << obj->PrettyTypeOf();
    if (kUseBakerReadBarrier) {
      obj->AssertReadBarrierState();
    }
    current += RoundUp(obj->SizeOf(), kObjectAlignment);
  }
}

// Helper class for relocating from one range of memory to another.
class RelocationRange {
 public:
  RelocationRange() = default;
  RelocationRange(const RelocationRange&) = default;
  RelocationRange(uintptr_t source, uintptr_t dest, uintptr_t length)
      : source_(source),
        dest_(dest),
        length_(length) {}

  bool InSource(uintptr_t address) const {
    return address - source_ < length_;
  }

  bool InDest(uintptr_t address) const {
    return address - dest_ < length_;
  }

  // Translate a source address to the destination space.
  uintptr_t ToDest(uintptr_t address) const {
    DCHECK(InSource(address));
    return address + Delta();
  }

  // Returns the delta between the dest from the source.
  uintptr_t Delta() const {
    return dest_ - source_;
  }

  uintptr_t Source() const {
    return source_;
  }

  uintptr_t Dest() const {
    return dest_;
  }

  uintptr_t Length() const {
    return length_;
  }

 private:
  const uintptr_t source_;
  const uintptr_t dest_;
  const uintptr_t length_;
};

std::ostream& operator<<(std::ostream& os, const RelocationRange& reloc) {
  return os << "(" << reinterpret_cast<const void*>(reloc.Source()) << "-"
            << reinterpret_cast<const void*>(reloc.Source() + reloc.Length()) << ")->("
            << reinterpret_cast<const void*>(reloc.Dest()) << "-"
            << reinterpret_cast<const void*>(reloc.Dest() + reloc.Length()) << ")";
}

// Helper class encapsulating loading, so we can access private ImageSpace members (this is a
// friend class), but not declare functions in the header.
class ImageSpaceLoader {
 public:
  static std::unique_ptr<ImageSpace> Load(const char* image_location,
                                          const std::string& image_filename,
                                          bool is_zygote,
                                          bool is_global_cache,
                                          bool validate_oat_file,
                                          std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Should this be a RDWR lock? This is only a defensive measure, as at
    // this point the image should exist.
    // However, only the zygote can write into the global dalvik-cache, so
    // restrict to zygote processes, or any process that isn't using
    // /data/dalvik-cache (which we assume to be allowed to write there).
    const bool rw_lock = is_zygote || !is_global_cache;

    // Note that we must not use the file descriptor associated with
    // ScopedFlock::GetFile to Init the image file. We want the file
    // descriptor (and the associated exclusive lock) to be released when
    // we leave Create.
    ScopedFlock image = LockedFile::Open(image_filename.c_str(),
                                         rw_lock ? (O_CREAT | O_RDWR) : O_RDONLY /* flags */,
                                         true /* block */,
                                         error_msg);

    VLOG(startup) << "Using image file " << image_filename.c_str() << " for image location "
                  << image_location;
    // If we are in /system we can assume the image is good. We can also
    // assume this if we are using a relocated image (i.e. image checksum
    // matches) since this is only different by the offset. We need this to
    // make sure that host tests continue to work.
    // Since we are the boot image, pass null since we load the oat file from the boot image oat
    // file name.
    return Init(image_filename.c_str(),
                image_location,
                validate_oat_file,
                /* oat_file */nullptr,
                error_msg);
  }

  static std::unique_ptr<ImageSpace> Init(const char* image_filename,
                                          const char* image_location,
                                          bool validate_oat_file,
                                          const OatFile* oat_file,
                                          std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(image_filename != nullptr);
    CHECK(image_location != nullptr);

    TimingLogger logger(__PRETTY_FUNCTION__, true, VLOG_IS_ON(image));
    VLOG(image) << "ImageSpace::Init entering image_filename=" << image_filename;

    std::unique_ptr<File> file;
    {
      TimingLogger::ScopedTiming timing("OpenImageFile", &logger);
      file.reset(OS::OpenFileForReading(image_filename));
      if (file == nullptr) {
        *error_msg = StringPrintf("Failed to open '%s'", image_filename);
        return nullptr;
      }
    }
    ImageHeader temp_image_header;
    ImageHeader* image_header = &temp_image_header;
    {
      TimingLogger::ScopedTiming timing("ReadImageHeader", &logger);
      bool success = file->ReadFully(image_header, sizeof(*image_header));
      if (!success || !image_header->IsValid()) {
        *error_msg = StringPrintf("Invalid image header in '%s'", image_filename);
        return nullptr;
      }
    }
    // Check that the file is larger or equal to the header size + data size.
    const uint64_t image_file_size = static_cast<uint64_t>(file->GetLength());
    if (image_file_size < sizeof(ImageHeader) + image_header->GetDataSize()) {
      *error_msg = StringPrintf("Image file truncated: %" PRIu64 " vs. %" PRIu64 ".",
                                image_file_size,
                                sizeof(ImageHeader) + image_header->GetDataSize());
      return nullptr;
    }

    if (oat_file != nullptr) {
      // If we have an oat file, check the oat file checksum. The oat file is only non-null for the
      // app image case. Otherwise, we open the oat file after the image and check the checksum there.
      const uint32_t oat_checksum = oat_file->GetOatHeader().GetChecksum();
      const uint32_t image_oat_checksum = image_header->GetOatChecksum();
      if (oat_checksum != image_oat_checksum) {
        *error_msg = StringPrintf("Oat checksum 0x%x does not match the image one 0x%x in image %s",
                                  oat_checksum,
                                  image_oat_checksum,
                                  image_filename);
        return nullptr;
      }
    }

    if (VLOG_IS_ON(startup)) {
      LOG(INFO) << "Dumping image sections";
      for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
        const auto section_idx = static_cast<ImageHeader::ImageSections>(i);
        auto& section = image_header->GetImageSection(section_idx);
        LOG(INFO) << section_idx << " start="
            << reinterpret_cast<void*>(image_header->GetImageBegin() + section.Offset()) << " "
            << section;
      }
    }

    const auto& bitmap_section = image_header->GetImageBitmapSection();
    // The location we want to map from is the first aligned page after the end of the stored
    // (possibly compressed) data.
    const size_t image_bitmap_offset = RoundUp(sizeof(ImageHeader) + image_header->GetDataSize(),
                                               kPageSize);
    const size_t end_of_bitmap = image_bitmap_offset + bitmap_section.Size();
    if (end_of_bitmap != image_file_size) {
      *error_msg = StringPrintf(
          "Image file size does not equal end of bitmap: size=%" PRIu64 " vs. %zu.", image_file_size,
          end_of_bitmap);
      return nullptr;
    }

    std::unique_ptr<MemMap> map;

    // GetImageBegin is the preferred address to map the image. If we manage to map the
    // image at the image begin, the amount of fixup work required is minimized.
    // If it is pic we will retry with error_msg for the failure case. Pass a null error_msg to
    // avoid reading proc maps for a mapping failure and slowing everything down.
    map.reset(LoadImageFile(image_filename,
                            image_location,
                            *image_header,
                            image_header->GetImageBegin(),
                            file->Fd(),
                            logger,
                            image_header->IsPic() ? nullptr : error_msg));
    // If the header specifies PIC mode, we can also map at a random low_4gb address since we can
    // relocate in-place.
    if (map == nullptr && image_header->IsPic()) {
      map.reset(LoadImageFile(image_filename,
                              image_location,
                              *image_header,
                              /* address */ nullptr,
                              file->Fd(),
                              logger,
                              error_msg));
    }
    // Were we able to load something and continue?
    if (map == nullptr) {
      DCHECK(!error_msg->empty());
      return nullptr;
    }
    DCHECK_EQ(0, memcmp(image_header, map->Begin(), sizeof(ImageHeader)));

    std::unique_ptr<MemMap> image_bitmap_map(MemMap::MapFileAtAddress(nullptr,
                                                                      bitmap_section.Size(),
                                                                      PROT_READ, MAP_PRIVATE,
                                                                      file->Fd(),
                                                                      image_bitmap_offset,
                                                                      /*low_4gb*/false,
                                                                      /*reuse*/false,
                                                                      image_filename,
                                                                      error_msg));
    if (image_bitmap_map == nullptr) {
      *error_msg = StringPrintf("Failed to map image bitmap: %s", error_msg->c_str());
      return nullptr;
    }
    // Loaded the map, use the image header from the file now in case we patch it with
    // RelocateInPlace.
    image_header = reinterpret_cast<ImageHeader*>(map->Begin());
    const uint32_t bitmap_index = ImageSpace::bitmap_index_.FetchAndAddSequentiallyConsistent(1);
    std::string bitmap_name(StringPrintf("imagespace %s live-bitmap %u",
                                         image_filename,
                                         bitmap_index));
    // Bitmap only needs to cover until the end of the mirror objects section.
    const ImageSection& image_objects = image_header->GetObjectsSection();
    // We only want the mirror object, not the ArtFields and ArtMethods.
    uint8_t* const image_end = map->Begin() + image_objects.End();
    std::unique_ptr<accounting::ContinuousSpaceBitmap> bitmap;
    {
      TimingLogger::ScopedTiming timing("CreateImageBitmap", &logger);
      bitmap.reset(
          accounting::ContinuousSpaceBitmap::CreateFromMemMap(
              bitmap_name,
              image_bitmap_map.release(),
              reinterpret_cast<uint8_t*>(map->Begin()),
              // Make sure the bitmap is aligned to card size instead of just bitmap word size.
              RoundUp(image_objects.End(), gc::accounting::CardTable::kCardSize)));
      if (bitmap == nullptr) {
        *error_msg = StringPrintf("Could not create bitmap '%s'", bitmap_name.c_str());
        return nullptr;
      }
    }
    {
      TimingLogger::ScopedTiming timing("RelocateImage", &logger);
      if (!RelocateInPlace(*image_header,
                           map->Begin(),
                           bitmap.get(),
                           oat_file,
                           error_msg)) {
        return nullptr;
      }
    }
    // We only want the mirror object, not the ArtFields and ArtMethods.
    std::unique_ptr<ImageSpace> space(new ImageSpace(image_filename,
                                                     image_location,
                                                     map.release(),
                                                     bitmap.release(),
                                                     image_end));

    // VerifyImageAllocations() will be called later in Runtime::Init()
    // as some class roots like ArtMethod::java_lang_reflect_ArtMethod_
    // and ArtField::java_lang_reflect_ArtField_, which are used from
    // Object::SizeOf() which VerifyImageAllocations() calls, are not
    // set yet at this point.
    if (oat_file == nullptr) {
      TimingLogger::ScopedTiming timing("OpenOatFile", &logger);
      space->oat_file_ = OpenOatFile(*space, image_filename, error_msg);
      if (space->oat_file_ == nullptr) {
        DCHECK(!error_msg->empty());
        return nullptr;
      }
      space->oat_file_non_owned_ = space->oat_file_.get();
    } else {
      space->oat_file_non_owned_ = oat_file;
    }

    if (validate_oat_file) {
      TimingLogger::ScopedTiming timing("ValidateOatFile", &logger);
      CHECK(space->oat_file_ != nullptr);
      if (!ImageSpace::ValidateOatFile(*space->oat_file_, error_msg)) {
        DCHECK(!error_msg->empty());
        return nullptr;
      }
    }

    Runtime* runtime = Runtime::Current();

    // If oat_file is null, then it is the boot image space. Use oat_file_non_owned_ from the space
    // to set the runtime methods.
    CHECK_EQ(oat_file != nullptr, image_header->IsAppImage());
    if (image_header->IsAppImage()) {
      CHECK_EQ(runtime->GetResolutionMethod(),
               image_header->GetImageMethod(ImageHeader::kResolutionMethod));
      CHECK_EQ(runtime->GetImtConflictMethod(),
               image_header->GetImageMethod(ImageHeader::kImtConflictMethod));
      CHECK_EQ(runtime->GetImtUnimplementedMethod(),
               image_header->GetImageMethod(ImageHeader::kImtUnimplementedMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveAllCalleeSaves),
               image_header->GetImageMethod(ImageHeader::kSaveAllCalleeSavesMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsOnly),
               image_header->GetImageMethod(ImageHeader::kSaveRefsOnlyMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsAndArgs),
               image_header->GetImageMethod(ImageHeader::kSaveRefsAndArgsMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverything),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethod));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForClinit),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForClinit));
      CHECK_EQ(runtime->GetCalleeSaveMethod(CalleeSaveType::kSaveEverythingForSuspendCheck),
               image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForSuspendCheck));
    } else if (!runtime->HasResolutionMethod()) {
      runtime->SetInstructionSet(space->oat_file_non_owned_->GetOatHeader().GetInstructionSet());
      runtime->SetResolutionMethod(image_header->GetImageMethod(ImageHeader::kResolutionMethod));
      runtime->SetImtConflictMethod(image_header->GetImageMethod(ImageHeader::kImtConflictMethod));
      runtime->SetImtUnimplementedMethod(
          image_header->GetImageMethod(ImageHeader::kImtUnimplementedMethod));
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveAllCalleeSavesMethod),
          CalleeSaveType::kSaveAllCalleeSaves);
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveRefsOnlyMethod),
          CalleeSaveType::kSaveRefsOnly);
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveRefsAndArgsMethod),
          CalleeSaveType::kSaveRefsAndArgs);
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveEverythingMethod),
          CalleeSaveType::kSaveEverything);
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForClinit),
          CalleeSaveType::kSaveEverythingForClinit);
      runtime->SetCalleeSaveMethod(
          image_header->GetImageMethod(ImageHeader::kSaveEverythingMethodForSuspendCheck),
          CalleeSaveType::kSaveEverythingForSuspendCheck);
    }

    VLOG(image) << "ImageSpace::Init exiting " << *space.get();
    if (VLOG_IS_ON(image)) {
      logger.Dump(LOG_STREAM(INFO));
    }
    return space;
  }

 private:
  static MemMap* LoadImageFile(const char* image_filename,
                               const char* image_location,
                               const ImageHeader& image_header,
                               uint8_t* address,
                               int fd,
                               TimingLogger& logger,
                               std::string* error_msg) {
    TimingLogger::ScopedTiming timing("MapImageFile", &logger);
    const ImageHeader::StorageMode storage_mode = image_header.GetStorageMode();
    if (storage_mode == ImageHeader::kStorageModeUncompressed) {
      return MemMap::MapFileAtAddress(address,
                                      image_header.GetImageSize(),
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE,
                                      fd,
                                      0,
                                      /*low_4gb*/true,
                                      /*reuse*/false,
                                      image_filename,
                                      error_msg);
    }

    if (storage_mode != ImageHeader::kStorageModeLZ4 &&
        storage_mode != ImageHeader::kStorageModeLZ4HC) {
      if (error_msg != nullptr) {
        *error_msg = StringPrintf("Invalid storage mode in image header %d",
                                  static_cast<int>(storage_mode));
      }
      return nullptr;
    }

    // Reserve output and decompress into it.
    std::unique_ptr<MemMap> map(MemMap::MapAnonymous(image_location,
                                                     address,
                                                     image_header.GetImageSize(),
                                                     PROT_READ | PROT_WRITE,
                                                     /*low_4gb*/true,
                                                     /*reuse*/false,
                                                     error_msg));
    if (map != nullptr) {
      const size_t stored_size = image_header.GetDataSize();
      const size_t decompress_offset = sizeof(ImageHeader);  // Skip the header.
      std::unique_ptr<MemMap> temp_map(MemMap::MapFile(sizeof(ImageHeader) + stored_size,
                                                       PROT_READ,
                                                       MAP_PRIVATE,
                                                       fd,
                                                       /*offset*/0,
                                                       /*low_4gb*/false,
                                                       image_filename,
                                                       error_msg));
      if (temp_map == nullptr) {
        DCHECK(error_msg == nullptr || !error_msg->empty());
        return nullptr;
      }
      memcpy(map->Begin(), &image_header, sizeof(ImageHeader));
      const uint64_t start = NanoTime();
      // LZ4HC and LZ4 have same internal format, both use LZ4_decompress.
      TimingLogger::ScopedTiming timing2("LZ4 decompress image", &logger);
      const size_t decompressed_size = LZ4_decompress_safe(
          reinterpret_cast<char*>(temp_map->Begin()) + sizeof(ImageHeader),
          reinterpret_cast<char*>(map->Begin()) + decompress_offset,
          stored_size,
          map->Size() - decompress_offset);
      const uint64_t time = NanoTime() - start;
      // Add one 1 ns to prevent possible divide by 0.
      VLOG(image) << "Decompressing image took " << PrettyDuration(time) << " ("
                  << PrettySize(static_cast<uint64_t>(map->Size()) * MsToNs(1000) / (time + 1))
                  << "/s)";
      if (decompressed_size + sizeof(ImageHeader) != image_header.GetImageSize()) {
        if (error_msg != nullptr) {
          *error_msg = StringPrintf(
              "Decompressed size does not match expected image size %zu vs %zu",
              decompressed_size + sizeof(ImageHeader),
              image_header.GetImageSize());
        }
        return nullptr;
      }
    }

    return map.release();
  }

  class FixupVisitor : public ValueObject {
   public:
    FixupVisitor(const RelocationRange& boot_image,
                 const RelocationRange& boot_oat,
                 const RelocationRange& app_image,
                 const RelocationRange& app_oat)
        : boot_image_(boot_image),
          boot_oat_(boot_oat),
          app_image_(app_image),
          app_oat_(app_oat) {}

    // Return the relocated address of a heap object.
    template <typename T>
    ALWAYS_INLINE T* ForwardObject(T* src) const {
      const uintptr_t uint_src = reinterpret_cast<uintptr_t>(src);
      if (boot_image_.InSource(uint_src)) {
        return reinterpret_cast<T*>(boot_image_.ToDest(uint_src));
      }
      if (app_image_.InSource(uint_src)) {
        return reinterpret_cast<T*>(app_image_.ToDest(uint_src));
      }
      // Since we are fixing up the app image, there should only be pointers to the app image and
      // boot image.
      DCHECK(src == nullptr) << reinterpret_cast<const void*>(src);
      return src;
    }

    // Return the relocated address of a code pointer (contained by an oat file).
    ALWAYS_INLINE const void* ForwardCode(const void* src) const {
      const uintptr_t uint_src = reinterpret_cast<uintptr_t>(src);
      if (boot_oat_.InSource(uint_src)) {
        return reinterpret_cast<const void*>(boot_oat_.ToDest(uint_src));
      }
      if (app_oat_.InSource(uint_src)) {
        return reinterpret_cast<const void*>(app_oat_.ToDest(uint_src));
      }
      DCHECK(src == nullptr) << src;
      return src;
    }

    // Must be called on pointers that already have been relocated to the destination relocation.
    ALWAYS_INLINE bool IsInAppImage(mirror::Object* object) const {
      return app_image_.InDest(reinterpret_cast<uintptr_t>(object));
    }

   protected:
    // Source section.
    const RelocationRange boot_image_;
    const RelocationRange boot_oat_;
    const RelocationRange app_image_;
    const RelocationRange app_oat_;
  };

  // Adapt for mirror::Class::FixupNativePointers.
  class FixupObjectAdapter : public FixupVisitor {
   public:
    template<typename... Args>
    explicit FixupObjectAdapter(Args... args) : FixupVisitor(args...) {}

    template <typename T>
    T* operator()(T* obj, void** dest_addr ATTRIBUTE_UNUSED = nullptr) const {
      return ForwardObject(obj);
    }
  };

  class FixupRootVisitor : public FixupVisitor {
   public:
    template<typename... Args>
    explicit FixupRootVisitor(Args... args) : FixupVisitor(args...) {}

    ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (!root->IsNull()) {
        VisitRoot(root);
      }
    }

    ALWAYS_INLINE void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
        REQUIRES_SHARED(Locks::mutator_lock_) {
      mirror::Object* ref = root->AsMirrorPtr();
      mirror::Object* new_ref = ForwardObject(ref);
      if (ref != new_ref) {
        root->Assign(new_ref);
      }
    }
  };

  class FixupObjectVisitor : public FixupVisitor {
   public:
    template<typename... Args>
    explicit FixupObjectVisitor(gc::accounting::ContinuousSpaceBitmap* visited,
                                const PointerSize pointer_size,
                                Args... args)
        : FixupVisitor(args...),
          pointer_size_(pointer_size),
          visited_(visited) {}

    // Fix up separately since we also need to fix up method entrypoints.
    ALWAYS_INLINE void VisitRootIfNonNull(
        mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    ALWAYS_INLINE void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
        const {}

    ALWAYS_INLINE void operator()(ObjPtr<mirror::Object> obj,
                                  MemberOffset offset,
                                  bool is_static ATTRIBUTE_UNUSED) const
        NO_THREAD_SAFETY_ANALYSIS {
      // There could be overlap between ranges, we must avoid visiting the same reference twice.
      // Avoid the class field since we already fixed it up in FixupClassVisitor.
      if (offset.Uint32Value() != mirror::Object::ClassOffset().Uint32Value()) {
        // Space is not yet added to the heap, don't do a read barrier.
        mirror::Object* ref = obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(
            offset);
        // Use SetFieldObjectWithoutWriteBarrier to avoid card marking since we are writing to the
        // image.
        obj->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(offset, ForwardObject(ref));
      }
    }

    // Visit a pointer array and forward corresponding native data. Ignores pointer arrays in the
    // boot image. Uses the bitmap to ensure the same array is not visited multiple times.
    template <typename Visitor>
    void UpdatePointerArrayContents(mirror::PointerArray* array, const Visitor& visitor) const
        NO_THREAD_SAFETY_ANALYSIS {
      DCHECK(array != nullptr);
      DCHECK(visitor.IsInAppImage(array));
      // The bit for the array contents is different than the bit for the array. Since we may have
      // already visited the array as a long / int array from walking the bitmap without knowing it
      // was a pointer array.
      static_assert(kObjectAlignment == 8u, "array bit may be in another object");
      mirror::Object* const contents_bit = reinterpret_cast<mirror::Object*>(
          reinterpret_cast<uintptr_t>(array) + kObjectAlignment);
      // If the bit is not set then the contents have not yet been updated.
      if (!visited_->Test(contents_bit)) {
        array->Fixup<kVerifyNone, kWithoutReadBarrier>(array, pointer_size_, visitor);
        visited_->Set(contents_bit);
      }
    }

    // java.lang.ref.Reference visitor.
    void operator()(ObjPtr<mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<mirror::Reference> ref) const
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_) {
      mirror::Object* obj = ref->GetReferent<kWithoutReadBarrier>();
      ref->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(
          mirror::Reference::ReferentOffset(),
          ForwardObject(obj));
    }

    void operator()(mirror::Object* obj) const
        NO_THREAD_SAFETY_ANALYSIS {
      if (visited_->Test(obj)) {
        // Already visited.
        return;
      }
      visited_->Set(obj);

      // Handle class specially first since we need it to be updated to properly visit the rest of
      // the instance fields.
      {
        mirror::Class* klass = obj->GetClass<kVerifyNone, kWithoutReadBarrier>();
        DCHECK(klass != nullptr) << "Null class in image";
        // No AsClass since our fields aren't quite fixed up yet.
        mirror::Class* new_klass = down_cast<mirror::Class*>(ForwardObject(klass));
        if (klass != new_klass) {
          obj->SetClass<kVerifyNone>(new_klass);
        }
        if (new_klass != klass && IsInAppImage(new_klass)) {
          // Make sure the klass contents are fixed up since we depend on it to walk the fields.
          operator()(new_klass);
        }
      }

      if (obj->IsClass()) {
        mirror::Class* klass = obj->AsClass<kVerifyNone, kWithoutReadBarrier>();
        // Fixup super class before visiting instance fields which require
        // information from their super class to calculate offsets.
        mirror::Class* super_class = klass->GetSuperClass<kVerifyNone, kWithoutReadBarrier>();
        if (super_class != nullptr) {
          mirror::Class* new_super_class = down_cast<mirror::Class*>(ForwardObject(super_class));
          if (new_super_class != super_class && IsInAppImage(new_super_class)) {
            // Recursively fix all dependencies.
            operator()(new_super_class);
          }
        }
      }

      obj->VisitReferences</*visit native roots*/false, kVerifyNone, kWithoutReadBarrier>(
          *this,
          *this);
      // Note that this code relies on no circular dependencies.
      // We want to use our own class loader and not the one in the image.
      if (obj->IsClass<kVerifyNone, kWithoutReadBarrier>()) {
        mirror::Class* as_klass = obj->AsClass<kVerifyNone, kWithoutReadBarrier>();
        FixupObjectAdapter visitor(boot_image_, boot_oat_, app_image_, app_oat_);
        as_klass->FixupNativePointers<kVerifyNone, kWithoutReadBarrier>(as_klass,
                                                                        pointer_size_,
                                                                        visitor);
        // Deal with the pointer arrays. Use the helper function since multiple classes can reference
        // the same arrays.
        mirror::PointerArray* const vtable = as_klass->GetVTable<kVerifyNone, kWithoutReadBarrier>();
        if (vtable != nullptr && IsInAppImage(vtable)) {
          operator()(vtable);
          UpdatePointerArrayContents(vtable, visitor);
        }
        mirror::IfTable* iftable = as_klass->GetIfTable<kVerifyNone, kWithoutReadBarrier>();
        // Ensure iftable arrays are fixed up since we need GetMethodArray to return the valid
        // contents.
        if (IsInAppImage(iftable)) {
          operator()(iftable);
          for (int32_t i = 0, count = iftable->Count(); i < count; ++i) {
            if (iftable->GetMethodArrayCount<kVerifyNone, kWithoutReadBarrier>(i) > 0) {
              mirror::PointerArray* methods =
                  iftable->GetMethodArray<kVerifyNone, kWithoutReadBarrier>(i);
              if (visitor.IsInAppImage(methods)) {
                operator()(methods);
                DCHECK(methods != nullptr);
                UpdatePointerArrayContents(methods, visitor);
              }
            }
          }
        }
      }
    }

   private:
    const PointerSize pointer_size_;
    gc::accounting::ContinuousSpaceBitmap* const visited_;
  };

  class ForwardObjectAdapter {
   public:
    ALWAYS_INLINE explicit ForwardObjectAdapter(const FixupVisitor* visitor) : visitor_(visitor) {}

    template <typename T>
    ALWAYS_INLINE T* operator()(T* src) const {
      return visitor_->ForwardObject(src);
    }

   private:
    const FixupVisitor* const visitor_;
  };

  class ForwardCodeAdapter {
   public:
    ALWAYS_INLINE explicit ForwardCodeAdapter(const FixupVisitor* visitor)
        : visitor_(visitor) {}

    template <typename T>
    ALWAYS_INLINE T* operator()(T* src) const {
      return visitor_->ForwardCode(src);
    }

   private:
    const FixupVisitor* const visitor_;
  };

  class FixupArtMethodVisitor : public FixupVisitor, public ArtMethodVisitor {
   public:
    template<typename... Args>
    explicit FixupArtMethodVisitor(bool fixup_heap_objects, PointerSize pointer_size, Args... args)
        : FixupVisitor(args...),
          fixup_heap_objects_(fixup_heap_objects),
          pointer_size_(pointer_size) {}

    virtual void Visit(ArtMethod* method) NO_THREAD_SAFETY_ANALYSIS {
      // TODO: Separate visitor for runtime vs normal methods.
      if (UNLIKELY(method->IsRuntimeMethod())) {
        ImtConflictTable* table = method->GetImtConflictTable(pointer_size_);
        if (table != nullptr) {
          ImtConflictTable* new_table = ForwardObject(table);
          if (table != new_table) {
            method->SetImtConflictTable(new_table, pointer_size_);
          }
        }
        const void* old_code = method->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size_);
        const void* new_code = ForwardCode(old_code);
        if (old_code != new_code) {
          method->SetEntryPointFromQuickCompiledCodePtrSize(new_code, pointer_size_);
        }
      } else {
        if (fixup_heap_objects_) {
          method->UpdateObjectsForImageRelocation(ForwardObjectAdapter(this));
        }
        method->UpdateEntrypoints<kWithoutReadBarrier>(ForwardCodeAdapter(this), pointer_size_);
      }
    }

   private:
    const bool fixup_heap_objects_;
    const PointerSize pointer_size_;
  };

  class FixupArtFieldVisitor : public FixupVisitor, public ArtFieldVisitor {
   public:
    template<typename... Args>
    explicit FixupArtFieldVisitor(Args... args) : FixupVisitor(args...) {}

    virtual void Visit(ArtField* field) NO_THREAD_SAFETY_ANALYSIS {
      field->UpdateObjects(ForwardObjectAdapter(this));
    }
  };

  // Relocate an image space mapped at target_base which possibly used to be at a different base
  // address. Only needs a single image space, not one for both source and destination.
  // In place means modifying a single ImageSpace in place rather than relocating from one ImageSpace
  // to another.
  static bool RelocateInPlace(ImageHeader& image_header,
                              uint8_t* target_base,
                              accounting::ContinuousSpaceBitmap* bitmap,
                              const OatFile* app_oat_file,
                              std::string* error_msg) {
    DCHECK(error_msg != nullptr);
    if (!image_header.IsPic()) {
      if (image_header.GetImageBegin() == target_base) {
        return true;
      }
      *error_msg = StringPrintf("Cannot relocate non-pic image for oat file %s",
                                (app_oat_file != nullptr) ? app_oat_file->GetLocation().c_str() : "");
      return false;
    }
    // Set up sections.
    uint32_t boot_image_begin = 0;
    uint32_t boot_image_end = 0;
    uint32_t boot_oat_begin = 0;
    uint32_t boot_oat_end = 0;
    const PointerSize pointer_size = image_header.GetPointerSize();
    gc::Heap* const heap = Runtime::Current()->GetHeap();
    heap->GetBootImagesSize(&boot_image_begin, &boot_image_end, &boot_oat_begin, &boot_oat_end);
    if (boot_image_begin == boot_image_end) {
      *error_msg = "Can not relocate app image without boot image space";
      return false;
    }
    if (boot_oat_begin == boot_oat_end) {
      *error_msg = "Can not relocate app image without boot oat file";
      return false;
    }
    const uint32_t boot_image_size = boot_image_end - boot_image_begin;
    const uint32_t boot_oat_size = boot_oat_end - boot_oat_begin;
    const uint32_t image_header_boot_image_size = image_header.GetBootImageSize();
    const uint32_t image_header_boot_oat_size = image_header.GetBootOatSize();
    if (boot_image_size != image_header_boot_image_size) {
      *error_msg = StringPrintf("Boot image size %" PRIu64 " does not match expected size %"
                                    PRIu64,
                                static_cast<uint64_t>(boot_image_size),
                                static_cast<uint64_t>(image_header_boot_image_size));
      return false;
    }
    if (boot_oat_size != image_header_boot_oat_size) {
      *error_msg = StringPrintf("Boot oat size %" PRIu64 " does not match expected size %"
                                    PRIu64,
                                static_cast<uint64_t>(boot_oat_size),
                                static_cast<uint64_t>(image_header_boot_oat_size));
      return false;
    }
    TimingLogger logger(__FUNCTION__, true, false);
    RelocationRange boot_image(image_header.GetBootImageBegin(),
                               boot_image_begin,
                               boot_image_size);
    RelocationRange boot_oat(image_header.GetBootOatBegin(),
                             boot_oat_begin,
                             boot_oat_size);
    RelocationRange app_image(reinterpret_cast<uintptr_t>(image_header.GetImageBegin()),
                              reinterpret_cast<uintptr_t>(target_base),
                              image_header.GetImageSize());
    // Use the oat data section since this is where the OatFile::Begin is.
    RelocationRange app_oat(reinterpret_cast<uintptr_t>(image_header.GetOatDataBegin()),
                            // Not necessarily in low 4GB.
                            reinterpret_cast<uintptr_t>(app_oat_file->Begin()),
                            image_header.GetOatDataEnd() - image_header.GetOatDataBegin());
    VLOG(image) << "App image " << app_image;
    VLOG(image) << "App oat " << app_oat;
    VLOG(image) << "Boot image " << boot_image;
    VLOG(image) << "Boot oat " << boot_oat;
    // True if we need to fixup any heap pointers, otherwise only code pointers.
    const bool fixup_image = boot_image.Delta() != 0 || app_image.Delta() != 0;
    const bool fixup_code = boot_oat.Delta() != 0 || app_oat.Delta() != 0;
    if (!fixup_image && !fixup_code) {
      // Nothing to fix up.
      return true;
    }
    ScopedDebugDisallowReadBarriers sddrb(Thread::Current());
    // Need to update the image to be at the target base.
    const ImageSection& objects_section = image_header.GetObjectsSection();
    uintptr_t objects_begin = reinterpret_cast<uintptr_t>(target_base + objects_section.Offset());
    uintptr_t objects_end = reinterpret_cast<uintptr_t>(target_base + objects_section.End());
    FixupObjectAdapter fixup_adapter(boot_image, boot_oat, app_image, app_oat);
    if (fixup_image) {
      // Two pass approach, fix up all classes first, then fix up non class-objects.
      // The visited bitmap is used to ensure that pointer arrays are not forwarded twice.
      std::unique_ptr<gc::accounting::ContinuousSpaceBitmap> visited_bitmap(
          gc::accounting::ContinuousSpaceBitmap::Create("Relocate bitmap",
                                                        target_base,
                                                        image_header.GetImageSize()));
      FixupObjectVisitor fixup_object_visitor(visited_bitmap.get(),
                                              pointer_size,
                                              boot_image,
                                              boot_oat,
                                              app_image,
                                              app_oat);
      TimingLogger::ScopedTiming timing("Fixup classes", &logger);
      // Fixup objects may read fields in the boot image, use the mutator lock here for sanity. Though
      // its probably not required.
      ScopedObjectAccess soa(Thread::Current());
      timing.NewTiming("Fixup objects");
      bitmap->VisitMarkedRange(objects_begin, objects_end, fixup_object_visitor);
      // Fixup image roots.
      CHECK(app_image.InSource(reinterpret_cast<uintptr_t>(
          image_header.GetImageRoots<kWithoutReadBarrier>())));
      image_header.RelocateImageObjects(app_image.Delta());
      CHECK_EQ(image_header.GetImageBegin(), target_base);
      // Fix up dex cache DexFile pointers.
      auto* dex_caches = image_header.GetImageRoot<kWithoutReadBarrier>(ImageHeader::kDexCaches)->
          AsObjectArray<mirror::DexCache, kVerifyNone, kWithoutReadBarrier>();
      for (int32_t i = 0, count = dex_caches->GetLength(); i < count; ++i) {
        mirror::DexCache* dex_cache = dex_caches->Get<kVerifyNone, kWithoutReadBarrier>(i);
        // Fix up dex cache pointers.
        mirror::StringDexCacheType* strings = dex_cache->GetStrings();
        if (strings != nullptr) {
          mirror::StringDexCacheType* new_strings = fixup_adapter.ForwardObject(strings);
          if (strings != new_strings) {
            dex_cache->SetStrings(new_strings);
          }
          dex_cache->FixupStrings<kWithoutReadBarrier>(new_strings, fixup_adapter);
        }
        mirror::TypeDexCacheType* types = dex_cache->GetResolvedTypes();
        if (types != nullptr) {
          mirror::TypeDexCacheType* new_types = fixup_adapter.ForwardObject(types);
          if (types != new_types) {
            dex_cache->SetResolvedTypes(new_types);
          }
          dex_cache->FixupResolvedTypes<kWithoutReadBarrier>(new_types, fixup_adapter);
        }
        mirror::MethodDexCacheType* methods = dex_cache->GetResolvedMethods();
        if (methods != nullptr) {
          mirror::MethodDexCacheType* new_methods = fixup_adapter.ForwardObject(methods);
          if (methods != new_methods) {
            dex_cache->SetResolvedMethods(new_methods);
          }
          for (size_t j = 0, num = dex_cache->NumResolvedMethods(); j != num; ++j) {
            auto pair = mirror::DexCache::GetNativePairPtrSize(new_methods, j, pointer_size);
            ArtMethod* orig = pair.object;
            ArtMethod* copy = fixup_adapter.ForwardObject(orig);
            if (orig != copy) {
              pair.object = copy;
              mirror::DexCache::SetNativePairPtrSize(new_methods, j, pair, pointer_size);
            }
          }
        }
        mirror::FieldDexCacheType* fields = dex_cache->GetResolvedFields();
        if (fields != nullptr) {
          mirror::FieldDexCacheType* new_fields = fixup_adapter.ForwardObject(fields);
          if (fields != new_fields) {
            dex_cache->SetResolvedFields(new_fields);
          }
          for (size_t j = 0, num = dex_cache->NumResolvedFields(); j != num; ++j) {
            mirror::FieldDexCachePair orig =
                mirror::DexCache::GetNativePairPtrSize(new_fields, j, pointer_size);
            mirror::FieldDexCachePair copy(fixup_adapter.ForwardObject(orig.object), orig.index);
            if (orig.object != copy.object) {
              mirror::DexCache::SetNativePairPtrSize(new_fields, j, copy, pointer_size);
            }
          }
        }

        mirror::MethodTypeDexCacheType* method_types = dex_cache->GetResolvedMethodTypes();
        if (method_types != nullptr) {
          mirror::MethodTypeDexCacheType* new_method_types =
              fixup_adapter.ForwardObject(method_types);
          if (method_types != new_method_types) {
            dex_cache->SetResolvedMethodTypes(new_method_types);
          }
          dex_cache->FixupResolvedMethodTypes<kWithoutReadBarrier>(new_method_types, fixup_adapter);
        }
        GcRoot<mirror::CallSite>* call_sites = dex_cache->GetResolvedCallSites();
        if (call_sites != nullptr) {
          GcRoot<mirror::CallSite>* new_call_sites = fixup_adapter.ForwardObject(call_sites);
          if (call_sites != new_call_sites) {
            dex_cache->SetResolvedCallSites(new_call_sites);
          }
          dex_cache->FixupResolvedCallSites<kWithoutReadBarrier>(new_call_sites, fixup_adapter);
        }
      }
    }
    {
      // Only touches objects in the app image, no need for mutator lock.
      TimingLogger::ScopedTiming timing("Fixup methods", &logger);
      FixupArtMethodVisitor method_visitor(fixup_image,
                                           pointer_size,
                                           boot_image,
                                           boot_oat,
                                           app_image,
                                           app_oat);
      image_header.VisitPackedArtMethods(&method_visitor, target_base, pointer_size);
    }
    if (fixup_image) {
      {
        // Only touches objects in the app image, no need for mutator lock.
        TimingLogger::ScopedTiming timing("Fixup fields", &logger);
        FixupArtFieldVisitor field_visitor(boot_image, boot_oat, app_image, app_oat);
        image_header.VisitPackedArtFields(&field_visitor, target_base);
      }
      {
        TimingLogger::ScopedTiming timing("Fixup imt", &logger);
        image_header.VisitPackedImTables(fixup_adapter, target_base, pointer_size);
      }
      {
        TimingLogger::ScopedTiming timing("Fixup conflict tables", &logger);
        image_header.VisitPackedImtConflictTables(fixup_adapter, target_base, pointer_size);
      }
      // In the app image case, the image methods are actually in the boot image.
      image_header.RelocateImageMethods(boot_image.Delta());
      const auto& class_table_section = image_header.GetClassTableSection();
      if (class_table_section.Size() > 0u) {
        // Note that we require that ReadFromMemory does not make an internal copy of the elements.
        // This also relies on visit roots not doing any verification which could fail after we update
        // the roots to be the image addresses.
        ScopedObjectAccess soa(Thread::Current());
        WriterMutexLock mu(Thread::Current(), *Locks::classlinker_classes_lock_);
        ClassTable temp_table;
        temp_table.ReadFromMemory(target_base + class_table_section.Offset());
        FixupRootVisitor root_visitor(boot_image, boot_oat, app_image, app_oat);
        temp_table.VisitRoots(root_visitor);
      }
    }
    if (VLOG_IS_ON(image)) {
      logger.Dump(LOG_STREAM(INFO));
    }
    return true;
  }

  static std::unique_ptr<OatFile> OpenOatFile(const ImageSpace& image,
                                              const char* image_path,
                                              std::string* error_msg) {
    const ImageHeader& image_header = image.GetImageHeader();
    std::string oat_filename = ImageHeader::GetOatLocationFromImageLocation(image_path);

    CHECK(image_header.GetOatDataBegin() != nullptr);

    std::unique_ptr<OatFile> oat_file(OatFile::Open(/* zip_fd */ -1,
                                                    oat_filename,
                                                    oat_filename,
                                                    image_header.GetOatDataBegin(),
                                                    image_header.GetOatFileBegin(),
                                                    !Runtime::Current()->IsAotCompiler(),
                                                    /*low_4gb*/false,
                                                    nullptr,
                                                    error_msg));
    if (oat_file == nullptr) {
      *error_msg = StringPrintf("Failed to open oat file '%s' referenced from image %s: %s",
                                oat_filename.c_str(),
                                image.GetName(),
                                error_msg->c_str());
      return nullptr;
    }
    uint32_t oat_checksum = oat_file->GetOatHeader().GetChecksum();
    uint32_t image_oat_checksum = image_header.GetOatChecksum();
    if (oat_checksum != image_oat_checksum) {
      *error_msg = StringPrintf("Failed to match oat file checksum 0x%x to expected oat checksum 0x%x"
                                " in image %s",
                                oat_checksum,
                                image_oat_checksum,
                                image.GetName());
      return nullptr;
    }
    int32_t image_patch_delta = image_header.GetPatchDelta();
    int32_t oat_patch_delta = oat_file->GetOatHeader().GetImagePatchDelta();
    if (oat_patch_delta != image_patch_delta && !image_header.CompilePic()) {
      // We should have already relocated by this point. Bail out.
      *error_msg = StringPrintf("Failed to match oat file patch delta %d to expected patch delta %d "
                                "in image %s",
                                oat_patch_delta,
                                image_patch_delta,
                                image.GetName());
      return nullptr;
    }

    return oat_file;
  }
};

static constexpr uint64_t kLowSpaceValue = 50 * MB;
static constexpr uint64_t kTmpFsSentinelValue = 384 * MB;

// Read the free space of the cache partition and make a decision whether to keep the generated
// image. This is to try to mitigate situations where the system might run out of space later.
static bool CheckSpace(const std::string& cache_filename, std::string* error_msg) {
  // Using statvfs vs statvfs64 because of b/18207376, and it is enough for all practical purposes.
  struct statvfs buf;

  int res = TEMP_FAILURE_RETRY(statvfs(cache_filename.c_str(), &buf));
  if (res != 0) {
    // Could not stat. Conservatively tell the system to delete the image.
    *error_msg = "Could not stat the filesystem, assuming low-memory situation.";
    return false;
  }

  uint64_t fs_overall_size = buf.f_bsize * static_cast<uint64_t>(buf.f_blocks);
  // Zygote is privileged, but other things are not. Use bavail.
  uint64_t fs_free_size = buf.f_bsize * static_cast<uint64_t>(buf.f_bavail);

  // Take the overall size as an indicator for a tmpfs, which is being used for the decryption
  // environment. We do not want to fail quickening the boot image there, as it is beneficial
  // for time-to-UI.
  if (fs_overall_size > kTmpFsSentinelValue) {
    if (fs_free_size < kLowSpaceValue) {
      *error_msg = StringPrintf("Low-memory situation: only %4.2f megabytes available, need at "
                                "least %" PRIu64 ".",
                                static_cast<double>(fs_free_size) / MB,
                                kLowSpaceValue / MB);
      return false;
    }
  }
  return true;
}

std::unique_ptr<ImageSpace> ImageSpace::CreateBootImage(const char* image_location,
                                                        const InstructionSet image_isa,
                                                        bool secondary_image,
                                                        std::string* error_msg) {
  ScopedTrace trace(__FUNCTION__);

  // Step 0: Extra zygote work.

  // Step 0.a: If we're the zygote, mark boot.
  const bool is_zygote = Runtime::Current()->IsZygote();
  if (is_zygote && !secondary_image && CanWriteToDalvikCache(image_isa)) {
    MarkZygoteStart(image_isa, Runtime::Current()->GetZygoteMaxFailedBoots());
  }

  // Step 0.b: If we're the zygote, check for free space, and prune the cache preemptively,
  //           if necessary. While the runtime may be fine (it is pretty tolerant to
  //           out-of-disk-space situations), other parts of the platform are not.
  //
  //           The advantage of doing this proactively is that the later steps are simplified,
  //           i.e., we do not need to code retries.
  std::string system_filename;
  bool has_system = false;
  std::string cache_filename;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = true;
  std::string dalvik_cache;
  bool found_image = FindImageFilenameImpl(image_location,
                                           image_isa,
                                           &has_system,
                                           &system_filename,
                                           &dalvik_cache_exists,
                                           &dalvik_cache,
                                           &is_global_cache,
                                           &has_cache,
                                           &cache_filename);

  bool dex2oat_enabled = Runtime::Current()->IsImageDex2OatEnabled();

  if (is_zygote && dalvik_cache_exists && !secondary_image) {
    // Extra checks for the zygote. These only apply when loading the first image, explained below.
    DCHECK(!dalvik_cache.empty());
    std::string local_error_msg;
    // All secondary images are verified when the primary image is verified.
    bool verified = VerifyImage(image_location, dalvik_cache.c_str(), image_isa, &local_error_msg);
    // If we prune for space at a secondary image, we may end up in a crash loop with the _exit
    // path.
    bool check_space = CheckSpace(dalvik_cache, &local_error_msg);
    if (!verified || !check_space) {
      // Note: it is important to only prune for space on the primary image, or we will hit the
      //       restart path.
      LOG(WARNING) << local_error_msg << " Preemptively pruning the dalvik cache.";
      PruneDalvikCache(image_isa);

      // Re-evaluate the image.
      found_image = FindImageFilenameImpl(image_location,
                                          image_isa,
                                          &has_system,
                                          &system_filename,
                                          &dalvik_cache_exists,
                                          &dalvik_cache,
                                          &is_global_cache,
                                          &has_cache,
                                          &cache_filename);
    }
    if (!check_space) {
      // Disable compilation/patching - we do not want to fill up the space again.
      dex2oat_enabled = false;
    }
  }

  // Collect all the errors.
  std::vector<std::string> error_msgs;

  // Step 1: Check if we have an existing and relocated image.

  // Step 1.a: Have files in system and cache. Then they need to match.
  if (found_image && has_system && has_cache) {
    std::string local_error_msg;
    // Check that the files are matching.
    if (ChecksumsMatch(system_filename.c_str(), cache_filename.c_str(), &local_error_msg)) {
      std::unique_ptr<ImageSpace> relocated_space =
          ImageSpaceLoader::Load(image_location,
                                 cache_filename,
                                 is_zygote,
                                 is_global_cache,
                                 /* validate_oat_file */ false,
                                 &local_error_msg);
      if (relocated_space != nullptr) {
        return relocated_space;
      }
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 1.b: Only have a cache file.
  if (found_image && !has_system && has_cache) {
    std::string local_error_msg;
    std::unique_ptr<ImageSpace> cache_space =
        ImageSpaceLoader::Load(image_location,
                               cache_filename,
                               is_zygote,
                               is_global_cache,
                               /* validate_oat_file */ true,
                               &local_error_msg);
    if (cache_space != nullptr) {
      return cache_space;
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 2: We have an existing image in /system.

  // Step 2.a: We are not required to relocate it. Then we can use it directly.
  bool relocate = Runtime::Current()->ShouldRelocate();

  if (found_image && has_system && !relocate) {
    std::string local_error_msg;
    std::unique_ptr<ImageSpace> system_space =
        ImageSpaceLoader::Load(image_location,
                               system_filename,
                               is_zygote,
                               is_global_cache,
                               /* validate_oat_file */ false,
                               &local_error_msg);
    if (system_space != nullptr) {
      return system_space;
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 2.b: We require a relocated image. Then we must patch it. This step fails if this is a
  //           secondary image.
  if (found_image && has_system && relocate) {
    std::string local_error_msg;
    if (!dex2oat_enabled) {
      local_error_msg = "Patching disabled.";
    } else if (secondary_image) {
      // We really want a working image. Prune and restart.
      PruneDalvikCache(image_isa);
      _exit(1);
    } else if (ImageCreationAllowed(is_global_cache, image_isa, &local_error_msg)) {
      bool patch_success =
          RelocateImage(image_location, dalvik_cache.c_str(), image_isa, &local_error_msg);
      if (patch_success) {
        std::unique_ptr<ImageSpace> patched_space =
            ImageSpaceLoader::Load(image_location,
                                   cache_filename,
                                   is_zygote,
                                   is_global_cache,
                                   /* validate_oat_file */ false,
                                   &local_error_msg);
        if (patched_space != nullptr) {
          return patched_space;
        }
      }
    }
    error_msgs.push_back(StringPrintf("Cannot relocate image %s to %s: %s",
                                      image_location,
                                      cache_filename.c_str(),
                                      local_error_msg.c_str()));
  }

  // Step 3: We do not have an existing image in /system, so generate an image into the dalvik
  //         cache. This step fails if this is a secondary image.
  if (!has_system) {
    std::string local_error_msg;
    if (!dex2oat_enabled) {
      local_error_msg = "Image compilation disabled.";
    } else if (secondary_image) {
      local_error_msg = "Cannot compile a secondary image.";
    } else if (ImageCreationAllowed(is_global_cache, image_isa, &local_error_msg)) {
      bool compilation_success = GenerateImage(cache_filename, image_isa, &local_error_msg);
      if (compilation_success) {
        std::unique_ptr<ImageSpace> compiled_space =
            ImageSpaceLoader::Load(image_location,
                                   cache_filename,
                                   is_zygote,
                                   is_global_cache,
                                   /* validate_oat_file */ false,
                                   &local_error_msg);
        if (compiled_space != nullptr) {
          return compiled_space;
        }
      }
    }
    error_msgs.push_back(StringPrintf("Cannot compile image to %s: %s",
                                      cache_filename.c_str(),
                                      local_error_msg.c_str()));
  }

  // We failed. Prune the cache the free up space, create a compound error message and return no
  // image.
  PruneDalvikCache(image_isa);

  std::ostringstream oss;
  bool first = true;
  for (const auto& msg : error_msgs) {
    if (!first) {
      oss << "\n    ";
    }
    oss << msg;
  }
  *error_msg = oss.str();

  return nullptr;
}

bool ImageSpace::LoadBootImage(const std::string& image_file_name,
                               const InstructionSet image_instruction_set,
                               std::vector<space::ImageSpace*>* boot_image_spaces,
                               uint8_t** oat_file_end) {
  DCHECK(boot_image_spaces != nullptr);
  DCHECK(boot_image_spaces->empty());
  DCHECK(oat_file_end != nullptr);
  DCHECK_NE(image_instruction_set, InstructionSet::kNone);

  if (image_file_name.empty()) {
    return false;
  }

  // For code reuse, handle this like a work queue.
  std::vector<std::string> image_file_names;
  image_file_names.push_back(image_file_name);

  bool error = false;
  uint8_t* oat_file_end_tmp = *oat_file_end;

  for (size_t index = 0; index < image_file_names.size(); ++index) {
    std::string& image_name = image_file_names[index];
    std::string error_msg;
    std::unique_ptr<space::ImageSpace> boot_image_space_uptr = CreateBootImage(
        image_name.c_str(),
        image_instruction_set,
        index > 0,
        &error_msg);
    if (boot_image_space_uptr != nullptr) {
      space::ImageSpace* boot_image_space = boot_image_space_uptr.release();
      boot_image_spaces->push_back(boot_image_space);
      // Oat files referenced by image files immediately follow them in memory, ensure alloc space
      // isn't going to get in the middle
      uint8_t* oat_file_end_addr = boot_image_space->GetImageHeader().GetOatFileEnd();
      CHECK_GT(oat_file_end_addr, boot_image_space->End());
      oat_file_end_tmp = AlignUp(oat_file_end_addr, kPageSize);

      if (index == 0) {
        // If this was the first space, check whether there are more images to load.
        const OatFile* boot_oat_file = boot_image_space->GetOatFile();
        if (boot_oat_file == nullptr) {
          continue;
        }

        const OatHeader& boot_oat_header = boot_oat_file->GetOatHeader();
        const char* boot_classpath =
            boot_oat_header.GetStoreValueByKey(OatHeader::kBootClassPathKey);
        if (boot_classpath == nullptr) {
          continue;
        }

        ExtractMultiImageLocations(image_file_name, boot_classpath, &image_file_names);
      }
    } else {
      error = true;
      LOG(ERROR) << "Could not create image space with image file '" << image_file_name << "'. "
          << "Attempting to fall back to imageless running. Error was: " << error_msg
          << "\nAttempted image: " << image_name;
      break;
    }
  }

  if (error) {
    // Remove already loaded spaces.
    for (space::Space* loaded_space : *boot_image_spaces) {
      delete loaded_space;
    }
    boot_image_spaces->clear();
    return false;
  }

  *oat_file_end = oat_file_end_tmp;
  return true;
}

ImageSpace::~ImageSpace() {
  Runtime* runtime = Runtime::Current();
  if (runtime == nullptr) {
    return;
  }

  if (GetImageHeader().IsAppImage()) {
    // This image space did not modify resolution method then in Init.
    return;
  }

  if (!runtime->HasResolutionMethod()) {
    // Another image space has already unloaded the below methods.
    return;
  }

  runtime->ClearInstructionSet();
  runtime->ClearResolutionMethod();
  runtime->ClearImtConflictMethod();
  runtime->ClearImtUnimplementedMethod();
  runtime->ClearCalleeSaveMethods();
}

std::unique_ptr<ImageSpace> ImageSpace::CreateFromAppImage(const char* image,
                                                           const OatFile* oat_file,
                                                           std::string* error_msg) {
  return ImageSpaceLoader::Init(image,
                                image,
                                /*validate_oat_file*/false,
                                oat_file,
                                /*out*/error_msg);
}

const OatFile* ImageSpace::GetOatFile() const {
  return oat_file_non_owned_;
}

std::unique_ptr<const OatFile> ImageSpace::ReleaseOatFile() {
  CHECK(oat_file_ != nullptr);
  return std::move(oat_file_);
}

void ImageSpace::Dump(std::ostream& os) const {
  os << GetType()
      << " begin=" << reinterpret_cast<void*>(Begin())
      << ",end=" << reinterpret_cast<void*>(End())
      << ",size=" << PrettySize(Size())
      << ",name=\"" << GetName() << "\"]";
}

std::string ImageSpace::GetMultiImageBootClassPath(
    const std::vector<const char*>& dex_locations,
    const std::vector<const char*>& oat_filenames,
    const std::vector<const char*>& image_filenames) {
  DCHECK_GT(oat_filenames.size(), 1u);
  // If the image filename was adapted (e.g., for our tests), we need to change this here,
  // too, but need to strip all path components (they will be re-established when loading).
  std::ostringstream bootcp_oss;
  bool first_bootcp = true;
  for (size_t i = 0; i < dex_locations.size(); ++i) {
    if (!first_bootcp) {
      bootcp_oss << ":";
    }

    std::string dex_loc = dex_locations[i];
    std::string image_filename = image_filenames[i];

    // Use the dex_loc path, but the image_filename name (without path elements).
    size_t dex_last_slash = dex_loc.rfind('/');

    // npos is max(size_t). That makes this a bit ugly.
    size_t image_last_slash = image_filename.rfind('/');
    size_t image_last_at = image_filename.rfind('@');
    size_t image_last_sep = (image_last_slash == std::string::npos)
                                ? image_last_at
                                : (image_last_at == std::string::npos)
                                      ? std::string::npos
                                      : std::max(image_last_slash, image_last_at);
    // Note: whenever image_last_sep == npos, +1 overflow means using the full string.

    if (dex_last_slash == std::string::npos) {
      dex_loc = image_filename.substr(image_last_sep + 1);
    } else {
      dex_loc = dex_loc.substr(0, dex_last_slash + 1) +
          image_filename.substr(image_last_sep + 1);
    }

    // Image filenames already end with .art, no need to replace.

    bootcp_oss << dex_loc;
    first_bootcp = false;
  }
  return bootcp_oss.str();
}

bool ImageSpace::ValidateOatFile(const OatFile& oat_file, std::string* error_msg) {
  const ArtDexFileLoader dex_file_loader;
  for (const OatFile::OatDexFile* oat_dex_file : oat_file.GetOatDexFiles()) {
    const std::string& dex_file_location = oat_dex_file->GetDexFileLocation();

    // Skip multidex locations - These will be checked when we visit their
    // corresponding primary non-multidex location.
    if (DexFileLoader::IsMultiDexLocation(dex_file_location.c_str())) {
      continue;
    }

    std::vector<uint32_t> checksums;
    if (!dex_file_loader.GetMultiDexChecksums(dex_file_location.c_str(), &checksums, error_msg)) {
      *error_msg = StringPrintf("ValidateOatFile failed to get checksums of dex file '%s' "
                                "referenced by oat file %s: %s",
                                dex_file_location.c_str(),
                                oat_file.GetLocation().c_str(),
                                error_msg->c_str());
      return false;
    }
    CHECK(!checksums.empty());
    if (checksums[0] != oat_dex_file->GetDexFileLocationChecksum()) {
      *error_msg = StringPrintf("ValidateOatFile found checksum mismatch between oat file "
                                "'%s' and dex file '%s' (0x%x != 0x%x)",
                                oat_file.GetLocation().c_str(),
                                dex_file_location.c_str(),
                                oat_dex_file->GetDexFileLocationChecksum(),
                                checksums[0]);
      return false;
    }

    // Verify checksums for any related multidex entries.
    for (size_t i = 1; i < checksums.size(); i++) {
      std::string multi_dex_location = DexFileLoader::GetMultiDexLocation(
          i,
          dex_file_location.c_str());
      const OatFile::OatDexFile* multi_dex = oat_file.GetOatDexFile(multi_dex_location.c_str(),
                                                                    nullptr,
                                                                    error_msg);
      if (multi_dex == nullptr) {
        *error_msg = StringPrintf("ValidateOatFile oat file '%s' is missing entry '%s'",
                                  oat_file.GetLocation().c_str(),
                                  multi_dex_location.c_str());
        return false;
      }

      if (checksums[i] != multi_dex->GetDexFileLocationChecksum()) {
        *error_msg = StringPrintf("ValidateOatFile found checksum mismatch between oat file "
                                  "'%s' and dex file '%s' (0x%x != 0x%x)",
                                  oat_file.GetLocation().c_str(),
                                  multi_dex_location.c_str(),
                                  multi_dex->GetDexFileLocationChecksum(),
                                  checksums[i]);
        return false;
      }
    }
  }
  return true;
}

void ImageSpace::ExtractMultiImageLocations(const std::string& input_image_file_name,
                                            const std::string& boot_classpath,
                                            std::vector<std::string>* image_file_names) {
  DCHECK(image_file_names != nullptr);

  std::vector<std::string> images;
  Split(boot_classpath, ':', &images);

  // Add the rest into the list. We have to adjust locations, possibly:
  //
  // For example, image_file_name is /a/b/c/d/e.art
  //              images[0] is          f/c/d/e.art
  // ----------------------------------------------
  //              images[1] is          g/h/i/j.art  -> /a/b/h/i/j.art
  const std::string& first_image = images[0];
  // Length of common suffix.
  size_t common = 0;
  while (common < input_image_file_name.size() &&
         common < first_image.size() &&
         *(input_image_file_name.end() - common - 1) == *(first_image.end() - common - 1)) {
    ++common;
  }
  // We want to replace the prefix of the input image with the prefix of the boot class path.
  // This handles the case where the image file contains @ separators.
  // Example image_file_name is oats/system@framework@boot.art
  // images[0] is .../arm/boot.art
  // means that the image name prefix will be oats/system@framework@
  // so that the other images are openable.
  const size_t old_prefix_length = first_image.size() - common;
  const std::string new_prefix = input_image_file_name.substr(
      0,
      input_image_file_name.size() - common);

  // Apply pattern to images[1] .. images[n].
  for (size_t i = 1; i < images.size(); ++i) {
    const std::string& image = images[i];
    CHECK_GT(image.length(), old_prefix_length);
    std::string suffix = image.substr(old_prefix_length);
    image_file_names->push_back(new_prefix + suffix);
  }
}

void ImageSpace::DumpSections(std::ostream& os) const {
  const uint8_t* base = Begin();
  const ImageHeader& header = GetImageHeader();
  for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
    auto section_type = static_cast<ImageHeader::ImageSections>(i);
    const ImageSection& section = header.GetImageSection(section_type);
    os << section_type << " " << reinterpret_cast<const void*>(base + section.Offset())
       << "-" << reinterpret_cast<const void*>(base + section.End()) << "\n";
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art
