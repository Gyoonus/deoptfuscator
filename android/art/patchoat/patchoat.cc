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
#include "patchoat.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "android-base/file.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/dumpable.h"
#include "base/file_utils.h"
#include "base/leb128.h"
#include "base/logging.h"  // For InitLogging.
#include "base/mutex.h"
#include "base/memory_tool.h"
#include "base/os.h"
#include "base/scoped_flock.h"
#include "base/stringpiece.h"
#include "base/unix_file/fd_file.h"
#include "base/unix_file/random_access_file_utils.h"
#include "base/utils.h"
#include "elf_file.h"
#include "elf_file_impl.h"
#include "elf_utils.h"
#include "gc/space/image_space.h"
#include "image-inl.h"
#include "intern_table.h"
#include "mirror/dex_cache.h"
#include "mirror/executable.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/reference.h"
#include "noop_compiler_callbacks.h"
#include "offsets.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

using android::base::StringPrintf;

namespace {

static const OatHeader* GetOatHeader(const ElfFile* elf_file) {
  uint64_t off = 0;
  if (!elf_file->GetSectionOffsetAndSize(".rodata", &off, nullptr)) {
    return nullptr;
  }

  OatHeader* oat_header = reinterpret_cast<OatHeader*>(elf_file->Begin() + off);
  return oat_header;
}

static File* CreateOrOpen(const char* name) {
  if (OS::FileExists(name)) {
    return OS::OpenFileReadWrite(name);
  } else {
    std::unique_ptr<File> f(OS::CreateEmptyFile(name));
    if (f.get() != nullptr) {
      if (fchmod(f->Fd(), 0644) != 0) {
        PLOG(ERROR) << "Unable to make " << name << " world readable";
        unlink(name);
        return nullptr;
      }
    }
    return f.release();
  }
}

// Either try to close the file (close=true), or erase it.
static bool FinishFile(File* file, bool close) {
  if (close) {
    if (file->FlushCloseOrErase() != 0) {
      PLOG(ERROR) << "Failed to flush and close file.";
      return false;
    }
    return true;
  } else {
    file->Erase();
    return false;
  }
}

static bool SymlinkFile(const std::string& input_filename, const std::string& output_filename) {
  if (input_filename == output_filename) {
    // Input and output are the same, nothing to do.
    return true;
  }

  // Unlink the original filename, since we are overwriting it.
  unlink(output_filename.c_str());

  // Create a symlink from the source file to the target path.
  if (symlink(input_filename.c_str(), output_filename.c_str()) < 0) {
    PLOG(ERROR) << "Failed to create symlink " << output_filename << " -> " << input_filename;
    return false;
  }

  if (kIsDebugBuild) {
    LOG(INFO) << "Created symlink " << output_filename << " -> " << input_filename;
  }

  return true;
}

// Holder class for runtime options and related objects.
class PatchoatRuntimeOptionsHolder {
 public:
  PatchoatRuntimeOptionsHolder(const std::string& image_location, InstructionSet isa) {
    options_.push_back(std::make_pair("compilercallbacks", &callbacks_));
    img_ = "-Ximage:" + image_location;
    options_.push_back(std::make_pair(img_.c_str(), nullptr));
    isa_name_ = GetInstructionSetString(isa);
    options_.push_back(std::make_pair("imageinstructionset",
                                      reinterpret_cast<const void*>(isa_name_.c_str())));
    options_.push_back(std::make_pair("-Xno-sig-chain", nullptr));
    // We do not want the runtime to attempt to patch the image.
    options_.push_back(std::make_pair("-Xnorelocate", nullptr));
    // Don't try to compile.
    options_.push_back(std::make_pair("-Xnoimage-dex2oat", nullptr));
    // Do not accept broken image.
    options_.push_back(std::make_pair("-Xno-dex-file-fallback", nullptr));
  }

  const RuntimeOptions& GetRuntimeOptions() {
    return options_;
  }

 private:
  RuntimeOptions options_;
  NoopCompilerCallbacks callbacks_;
  std::string isa_name_;
  std::string img_;
};

}  // namespace

bool PatchOat::GeneratePatch(
    const MemMap& original,
    const MemMap& relocated,
    std::vector<uint8_t>* output,
    std::string* error_msg) {
  // FORMAT of the patch (aka image relocation) file:
  // * SHA-256 digest (32 bytes) of original/unrelocated file (e.g., the one from /system)
  // * List of monotonically increasing offsets (max value defined by uint32_t) at which relocations
  //   occur.
  //   Each element is represented as the delta from the previous offset in the list (first element
  //   is a delta from 0). Each delta is encoded using unsigned LEB128: little-endian
  //   variable-length 7 bits per byte encoding, where all bytes have the highest bit (0x80) set
  //   except for the final byte which does not have that bit set. For example, 0x3f is offset 0x3f,
  //   whereas 0xbf 0x05 is offset (0x3f & 0x7f) | (0x5 << 7) which is 0x2bf. Most deltas end up
  //   being encoding using just one byte, achieving ~4x decrease in relocation file size compared
  //   to the encoding where offsets are stored verbatim, as uint32_t.

  size_t original_size = original.Size();
  size_t relocated_size = relocated.Size();
  if (original_size != relocated_size) {
    *error_msg =
        StringPrintf(
            "Original and relocated image sizes differ: %zu vs %zu", original_size, relocated_size);
    return false;
  }
  if ((original_size % 4) != 0) {
    *error_msg = StringPrintf("Image size not multiple of 4: %zu", original_size);
    return false;
  }
  if (original_size > UINT32_MAX) {
    *error_msg = StringPrintf("Image too large: %zu" , original_size);
    return false;
  }

  const ImageHeader& relocated_header =
      *reinterpret_cast<const ImageHeader*>(relocated.Begin());
  // Offsets are supposed to differ between original and relocated by this value
  off_t expected_diff = relocated_header.GetPatchDelta();
  if (expected_diff == 0) {
    // Can't identify offsets which are supposed to differ due to relocation
    *error_msg = "Relocation delta is 0";
    return false;
  }

  // Output the SHA-256 digest of the original
  output->resize(SHA256_DIGEST_LENGTH);
  const uint8_t* original_bytes = original.Begin();
  SHA256(original_bytes, original_size, output->data());

  // Output the list of offsets at which the original and patched images differ
  size_t last_diff_offset = 0;
  size_t diff_offset_count = 0;
  const uint8_t* relocated_bytes = relocated.Begin();
  for (size_t offset = 0; offset < original_size; offset += 4) {
    uint32_t original_value = *reinterpret_cast<const uint32_t*>(original_bytes + offset);
    uint32_t relocated_value = *reinterpret_cast<const uint32_t*>(relocated_bytes + offset);
    off_t diff = relocated_value - original_value;
    if (diff == 0) {
      continue;
    } else if (diff != expected_diff) {
      *error_msg =
          StringPrintf(
              "Unexpected diff at offset %zu. Expected: %jd, but was: %jd",
              offset,
              (intmax_t) expected_diff,
              (intmax_t) diff);
      return false;
    }

    uint32_t offset_diff = offset - last_diff_offset;
    last_diff_offset = offset;
    diff_offset_count++;

    EncodeUnsignedLeb128(output, offset_diff);
  }

  if (diff_offset_count == 0) {
    *error_msg = "Original and patched images are identical";
    return false;
  }

  return true;
}

static bool WriteRelFile(
    const MemMap& original,
    const MemMap& relocated,
    const std::string& rel_filename,
    std::string* error_msg) {
  std::vector<uint8_t> output;
  if (!PatchOat::GeneratePatch(original, relocated, &output, error_msg)) {
    return false;
  }

  std::unique_ptr<File> rel_file(OS::CreateEmptyFileWriteOnly(rel_filename.c_str()));
  if (rel_file.get() == nullptr) {
    *error_msg = StringPrintf("Failed to create/open output file %s", rel_filename.c_str());
    return false;
  }
  if (!rel_file->WriteFully(output.data(), output.size())) {
    *error_msg = StringPrintf("Failed to write to %s", rel_filename.c_str());
    return false;
  }
  if (rel_file->FlushCloseOrErase() != 0) {
    *error_msg = StringPrintf("Failed to flush and close %s", rel_filename.c_str());
    return false;
  }

  return true;
}

static bool CheckImageIdenticalToOriginalExceptForRelocation(
    const std::string& relocated_filename,
    const std::string& original_filename,
    std::string* error_msg) {
  *error_msg = "";
  std::string rel_filename = original_filename + ".rel";
  std::unique_ptr<File> rel_file(OS::OpenFileForReading(rel_filename.c_str()));
  if (rel_file.get() == nullptr) {
    *error_msg = StringPrintf("Failed to open image relocation file %s", rel_filename.c_str());
    return false;
  }
  int64_t rel_size = rel_file->GetLength();
  if (rel_size < 0) {
    *error_msg = StringPrintf("Error while getting size of image relocation file %s",
                              rel_filename.c_str());
    return false;
  }
  std::unique_ptr<uint8_t[]> rel(new uint8_t[rel_size]);
  if (!rel_file->ReadFully(rel.get(), rel_size)) {
    *error_msg = StringPrintf("Failed to read image relocation file %s", rel_filename.c_str());
    return false;
  }

  std::unique_ptr<File> image_file(OS::OpenFileForReading(relocated_filename.c_str()));
  if (image_file.get() == nullptr) {
    *error_msg = StringPrintf("Unable to open relocated image file  %s",
                              relocated_filename.c_str());
    return false;
  }

  int64_t image_size = image_file->GetLength();
  if (image_size < 0) {
    *error_msg = StringPrintf("Error while getting size of relocated image file %s",
                              relocated_filename.c_str());
    return false;
  }
  if ((image_size % 4) != 0) {
    *error_msg =
        StringPrintf(
            "Relocated image file %s size not multiple of 4: %" PRId64,
                relocated_filename.c_str(), image_size);
    return false;
  }
  if (image_size > std::numeric_limits<uint32_t>::max()) {
    *error_msg =
        StringPrintf(
            "Relocated image file %s too large: %" PRId64, relocated_filename.c_str(), image_size);
    return false;
  }

  std::unique_ptr<uint8_t[]> image(new uint8_t[image_size]);
  if (!image_file->ReadFully(image.get(), image_size)) {
    *error_msg = StringPrintf("Failed to read relocated image file %s", relocated_filename.c_str());
    return false;
  }

  const uint8_t* original_image_digest = rel.get();
  if (rel_size < SHA256_DIGEST_LENGTH) {
    *error_msg = StringPrintf("Malformed image relocation file %s: too short",
                              rel_filename.c_str());
    return false;
  }

  const ImageHeader& image_header = *reinterpret_cast<const ImageHeader*>(image.get());
  off_t expected_diff = image_header.GetPatchDelta();

  if (expected_diff == 0) {
    *error_msg = StringPrintf("Unsuported patch delta of zero in %s",
                              relocated_filename.c_str());
    return false;
  }

  // Relocated image is expected to differ from the original due to relocation.
  // Unrelocate the image in memory to compensate.
  uint8_t* image_start = image.get();
  const uint8_t* rel_end = &rel[rel_size];
  const uint8_t* rel_ptr = &rel[SHA256_DIGEST_LENGTH];
  // The remaining .rel file consists of offsets at which relocation should've occurred.
  // For each offset, we "unrelocate" the image by subtracting the expected relocation
  // diff value (as specified in the image header).
  //
  // Each offset is encoded as a delta/diff relative to the previous offset. With the
  // very first offset being encoded relative to offset 0.
  // Deltas are encoded using little-endian 7 bits per byte encoding, with all bytes except
  // the last one having the highest bit set.
  uint32_t offset = 0;
  while (rel_ptr != rel_end) {
    uint32_t offset_delta = 0;
    if (DecodeUnsignedLeb128Checked(&rel_ptr, rel_end, &offset_delta)) {
      offset += offset_delta;
      if (static_cast<int64_t>(offset) + static_cast<int64_t>(sizeof(uint32_t)) > image_size) {
        *error_msg = StringPrintf("Relocation out of bounds in %s", relocated_filename.c_str());
        return false;
      }
      uint32_t *image_value = reinterpret_cast<uint32_t*>(image_start + offset);
      *image_value -= expected_diff;
    } else {
      *error_msg =
          StringPrintf(
              "Malformed image relocation file %s: "
              "last byte has it's most significant bit set",
              rel_filename.c_str());
      return false;
    }
  }

  // Image in memory is now supposed to be identical to the original.  We
  // confirm this by comparing the digest of the in-memory image to the expected
  // digest from relocation file.
  uint8_t image_digest[SHA256_DIGEST_LENGTH];
  SHA256(image.get(), image_size, image_digest);
  if (memcmp(image_digest, original_image_digest, SHA256_DIGEST_LENGTH) != 0) {
    *error_msg =
        StringPrintf(
            "Relocated image %s does not match the original %s after unrelocation",
            relocated_filename.c_str(),
            original_filename.c_str());
    return false;
  }

  // Relocated image is identical to the original, once relocations are taken into account
  return true;
}

static bool VerifySymlink(const std::string& intended_target, const std::string& link_name) {
  std::string actual_target;
  if (!android::base::Readlink(link_name, &actual_target)) {
    PLOG(ERROR) << "Readlink on " << link_name << " failed.";
    return false;
  }
  return actual_target == intended_target;
}

static bool VerifyVdexAndOatSymlinks(const std::string& input_image_filename,
                                     const std::string& output_image_filename) {
  return VerifySymlink(ImageHeader::GetVdexLocationFromImageLocation(input_image_filename),
                       ImageHeader::GetVdexLocationFromImageLocation(output_image_filename))
      && VerifySymlink(ImageHeader::GetOatLocationFromImageLocation(input_image_filename),
                       ImageHeader::GetOatLocationFromImageLocation(output_image_filename));
}

bool PatchOat::CreateVdexAndOatSymlinks(const std::string& input_image_filename,
                                        const std::string& output_image_filename) {
  std::string input_vdex_filename =
      ImageHeader::GetVdexLocationFromImageLocation(input_image_filename);
  std::string input_oat_filename =
      ImageHeader::GetOatLocationFromImageLocation(input_image_filename);

  std::unique_ptr<File> input_oat_file(OS::OpenFileForReading(input_oat_filename.c_str()));
  if (input_oat_file.get() == nullptr) {
    LOG(ERROR) << "Unable to open input oat file at " << input_oat_filename;
    return false;
  }
  std::string error_msg;
  std::unique_ptr<ElfFile> elf(ElfFile::Open(input_oat_file.get(),
                                             PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE,
                                             &error_msg));
  if (elf.get() == nullptr) {
    LOG(ERROR) << "Unable to open oat file " << input_oat_filename << " : " << error_msg;
    return false;
  }

  MaybePic is_oat_pic = IsOatPic(elf.get());
  if (is_oat_pic >= ERROR_FIRST) {
    // Error logged by IsOatPic
    return false;
  } else if (is_oat_pic == NOT_PIC) {
    LOG(ERROR) << "patchoat cannot be used on non-PIC oat file: " << input_oat_filename;
    return false;
  }

  CHECK(is_oat_pic == PIC);

  std::string output_vdex_filename =
      ImageHeader::GetVdexLocationFromImageLocation(output_image_filename);
  std::string output_oat_filename =
      ImageHeader::GetOatLocationFromImageLocation(output_image_filename);

  return SymlinkFile(input_oat_filename, output_oat_filename) &&
         SymlinkFile(input_vdex_filename, output_vdex_filename);
}

bool PatchOat::Patch(const std::string& image_location,
                     off_t delta,
                     const std::string& output_image_directory,
                     const std::string& output_image_relocation_directory,
                     InstructionSet isa,
                     TimingLogger* timings) {
  bool output_image = !output_image_directory.empty();
  bool output_image_relocation = !output_image_relocation_directory.empty();
  if ((!output_image) && (!output_image_relocation)) {
    // Nothing to do
    return true;
  }
  if ((output_image_relocation) && (delta == 0)) {
    LOG(ERROR) << "Cannot output image relocation information when requested relocation delta is 0";
    return false;
  }

  CHECK(Runtime::Current() == nullptr);
  CHECK(!image_location.empty()) << "image file must have a filename.";

  TimingLogger::ScopedTiming t("Runtime Setup", timings);

  CHECK_NE(isa, InstructionSet::kNone);

  // Set up the runtime
  PatchoatRuntimeOptionsHolder options_holder(image_location, isa);
  if (!Runtime::Create(options_holder.GetRuntimeOptions(), false)) {
    LOG(ERROR) << "Unable to initialize runtime";
    return false;
  }
  std::unique_ptr<Runtime> runtime(Runtime::Current());

  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now and then switch to a more manageable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  ScopedObjectAccess soa(Thread::Current());

  std::vector<gc::space::ImageSpace*> spaces = Runtime::Current()->GetHeap()->GetBootImageSpaces();
  std::map<gc::space::ImageSpace*, std::unique_ptr<MemMap>> space_to_memmap_map;

  for (size_t i = 0; i < spaces.size(); ++i) {
    t.NewTiming("Image Patching setup");
    gc::space::ImageSpace* space = spaces[i];
    std::string input_image_filename = space->GetImageFilename();
    std::unique_ptr<File> input_image(OS::OpenFileForReading(input_image_filename.c_str()));
    if (input_image.get() == nullptr) {
      LOG(ERROR) << "Unable to open input image file at " << input_image_filename;
      return false;
    }

    int64_t image_len = input_image->GetLength();
    if (image_len < 0) {
      LOG(ERROR) << "Error while getting image length";
      return false;
    }
    ImageHeader image_header;
    if (sizeof(image_header) != input_image->Read(reinterpret_cast<char*>(&image_header),
                                                  sizeof(image_header), 0)) {
      LOG(ERROR) << "Unable to read image header from image file " << input_image->GetPath();
    }

    /*bool is_image_pic = */IsImagePic(image_header, input_image->GetPath());
    // Nothing special to do right now since the image always needs to get patched.
    // Perhaps in some far-off future we may have images with relative addresses that are true-PIC.

    // Create the map where we will write the image patches to.
    std::string error_msg;
    std::unique_ptr<MemMap> image(MemMap::MapFile(image_len,
                                                  PROT_READ | PROT_WRITE,
                                                  MAP_PRIVATE,
                                                  input_image->Fd(),
                                                  0,
                                                  /*low_4gb*/false,
                                                  input_image->GetPath().c_str(),
                                                  &error_msg));
    if (image.get() == nullptr) {
      LOG(ERROR) << "Unable to map image file " << input_image->GetPath() << " : " << error_msg;
      return false;
    }


    space_to_memmap_map.emplace(space, std::move(image));
    PatchOat p = PatchOat(isa,
                          space_to_memmap_map.at(space).get(),
                          space->GetLiveBitmap(),
                          space->GetMemMap(),
                          delta,
                          &space_to_memmap_map,
                          timings);

    t.NewTiming("Patching image");
    if (!p.PatchImage(i == 0)) {
      LOG(ERROR) << "Failed to patch image file " << input_image_filename;
      return false;
    }

    // Write the patched image spaces.
    if (output_image) {
      std::string output_image_filename;
      if (!GetDalvikCacheFilename(space->GetImageLocation().c_str(),
                                  output_image_directory.c_str(),
                                  &output_image_filename,
                                  &error_msg)) {
        LOG(ERROR) << "Failed to find relocated image file name: " << error_msg;
        return false;
      }

      if (!CreateVdexAndOatSymlinks(input_image_filename, output_image_filename))
        return false;

      t.NewTiming("Writing image");
      std::unique_ptr<File> output_image_file(CreateOrOpen(output_image_filename.c_str()));
      if (output_image_file.get() == nullptr) {
        LOG(ERROR) << "Failed to open output image file at " << output_image_filename;
        return false;
      }

      bool success = p.WriteImage(output_image_file.get());
      success = FinishFile(output_image_file.get(), success);
      if (!success) {
        return false;
      }
    }

    if (output_image_relocation) {
      t.NewTiming("Writing image relocation");
      std::string original_image_filename(space->GetImageLocation() + ".rel");
      std::string image_relocation_filename =
          output_image_relocation_directory
              + (android::base::StartsWith(original_image_filename, "/") ? "" : "/")
              + original_image_filename.substr(original_image_filename.find_last_of("/"));
      int64_t input_image_size = input_image->GetLength();
      if (input_image_size < 0) {
        LOG(ERROR) << "Error while getting input image size";
        return false;
      }
      std::unique_ptr<MemMap> original(MemMap::MapFile(input_image_size,
                                                       PROT_READ,
                                                       MAP_PRIVATE,
                                                       input_image->Fd(),
                                                       0,
                                                       /*low_4gb*/false,
                                                       input_image->GetPath().c_str(),
                                                       &error_msg));
      if (original.get() == nullptr) {
        LOG(ERROR) << "Unable to map image file " << input_image->GetPath() << " : " << error_msg;
        return false;
      }

      const MemMap* relocated = p.image_;

      if (!WriteRelFile(*original, *relocated, image_relocation_filename, &error_msg)) {
        LOG(ERROR) << "Failed to create image relocation file " << image_relocation_filename
            << ": " << error_msg;
        return false;
      }
    }
  }

  if (!kIsDebugBuild && !(RUNNING_ON_MEMORY_TOOL && kMemoryToolDetectsLeaks)) {
    // We want to just exit on non-debug builds, not bringing the runtime down
    // in an orderly fashion. So release the following fields.
    runtime.release();
  }

  return true;
}

bool PatchOat::Verify(const std::string& image_location,
                      const std::string& output_image_directory,
                      InstructionSet isa,
                      TimingLogger* timings) {
  if (image_location.empty()) {
    LOG(ERROR) << "Original image file not provided";
    return false;
  }
  if (output_image_directory.empty()) {
    LOG(ERROR) << "Relocated image directory not provided";
    return false;
  }

  TimingLogger::ScopedTiming t("Runtime Setup", timings);

  CHECK_NE(isa, InstructionSet::kNone);

  // Set up the runtime
  PatchoatRuntimeOptionsHolder options_holder(image_location, isa);
  if (!Runtime::Create(options_holder.GetRuntimeOptions(), false)) {
    LOG(ERROR) << "Unable to initialize runtime";
    return false;
  }
  std::unique_ptr<Runtime> runtime(Runtime::Current());

  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now and then switch to a more manageable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  ScopedObjectAccess soa(Thread::Current());

  t.NewTiming("Image Verification setup");
  std::vector<gc::space::ImageSpace*> spaces = Runtime::Current()->GetHeap()->GetBootImageSpaces();

  // TODO: Check that no other .rel files exist in the original dir

  bool success = true;
  std::string image_location_dir = android::base::Dirname(image_location);
  for (size_t i = 0; i < spaces.size(); ++i) {
    gc::space::ImageSpace* space = spaces[i];

    std::string relocated_image_filename;
    std::string error_msg;
    if (!GetDalvikCacheFilename(space->GetImageLocation().c_str(),
            output_image_directory.c_str(), &relocated_image_filename, &error_msg)) {
      LOG(ERROR) << "Failed to find relocated image file name: " << error_msg;
      success = false;
      break;
    }
    // location:     /system/framework/boot.art
    // isa:          arm64
    // basename:     boot.art
    // original:     /system/framework/arm64/boot.art
    // relocation:   /system/framework/arm64/boot.art.rel
    std::string original_image_filename =
        GetSystemImageFilename(space->GetImageLocation().c_str(), isa);

    if (!CheckImageIdenticalToOriginalExceptForRelocation(
            relocated_image_filename, original_image_filename, &error_msg)) {
      LOG(ERROR) << error_msg;
      success = false;
      break;
    }

    if (!VerifyVdexAndOatSymlinks(original_image_filename, relocated_image_filename)) {
      LOG(ERROR) << "Verification of vdex and oat symlinks for "
                 << space->GetImageLocation() << " failed.";
      success = false;
      break;
    }
  }

  if (!kIsDebugBuild && !(RUNNING_ON_MEMORY_TOOL && kMemoryToolDetectsLeaks)) {
    // We want to just exit on non-debug builds, not bringing the runtime down
    // in an orderly fashion. So release the following fields.
    runtime.release();
  }

  return success;
}

bool PatchOat::WriteImage(File* out) {
  TimingLogger::ScopedTiming t("Writing image File", timings_);
  std::string error_msg;

  // No error checking here, this is best effort. The locking may or may not
  // succeed and we don't really care either way.
  ScopedFlock img_flock = LockedFile::DupOf(out->Fd(), out->GetPath(),
                                            true /* read_only_mode */, &error_msg);

  CHECK(image_ != nullptr);
  CHECK(out != nullptr);
  size_t expect = image_->Size();
  if (out->WriteFully(reinterpret_cast<char*>(image_->Begin()), expect) &&
      out->SetLength(expect) == 0) {
    return true;
  } else {
    LOG(ERROR) << "Writing to image file " << out->GetPath() << " failed.";
    return false;
  }
}

bool PatchOat::IsImagePic(const ImageHeader& image_header, const std::string& image_path) {
  if (!image_header.CompilePic()) {
    if (kIsDebugBuild) {
      LOG(INFO) << "image at location " << image_path << " was *not* compiled pic";
    }
    return false;
  }

  if (kIsDebugBuild) {
    LOG(INFO) << "image at location " << image_path << " was compiled PIC";
  }

  return true;
}

PatchOat::MaybePic PatchOat::IsOatPic(const ElfFile* oat_in) {
  if (oat_in == nullptr) {
    LOG(ERROR) << "No ELF input oat fie available";
    return ERROR_OAT_FILE;
  }

  const std::string& file_path = oat_in->GetFilePath();

  const OatHeader* oat_header = GetOatHeader(oat_in);
  if (oat_header == nullptr) {
    LOG(ERROR) << "Failed to find oat header in oat file " << file_path;
    return ERROR_OAT_FILE;
  }

  if (!oat_header->IsValid()) {
    LOG(ERROR) << "Elf file " << file_path << " has an invalid oat header";
    return ERROR_OAT_FILE;
  }

  bool is_pic = oat_header->IsPic();
  if (kIsDebugBuild) {
    LOG(INFO) << "Oat file at " << file_path << " is " << (is_pic ? "PIC" : "not pic");
  }

  return is_pic ? PIC : NOT_PIC;
}

class PatchOat::PatchOatArtFieldVisitor : public ArtFieldVisitor {
 public:
  explicit PatchOatArtFieldVisitor(PatchOat* patch_oat) : patch_oat_(patch_oat) {}

  void Visit(ArtField* field) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtField* const dest = patch_oat_->RelocatedCopyOf(field);
    dest->SetDeclaringClass(
        patch_oat_->RelocatedAddressOfPointer(field->GetDeclaringClass().Ptr()));
  }

 private:
  PatchOat* const patch_oat_;
};

void PatchOat::PatchArtFields(const ImageHeader* image_header) {
  PatchOatArtFieldVisitor visitor(this);
  image_header->VisitPackedArtFields(&visitor, heap_->Begin());
}

class PatchOat::PatchOatArtMethodVisitor : public ArtMethodVisitor {
 public:
  explicit PatchOatArtMethodVisitor(PatchOat* patch_oat) : patch_oat_(patch_oat) {}

  void Visit(ArtMethod* method) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* const dest = patch_oat_->RelocatedCopyOf(method);
    patch_oat_->FixupMethod(method, dest);
  }

 private:
  PatchOat* const patch_oat_;
};

void PatchOat::PatchArtMethods(const ImageHeader* image_header) {
  const PointerSize pointer_size = InstructionSetPointerSize(isa_);
  PatchOatArtMethodVisitor visitor(this);
  image_header->VisitPackedArtMethods(&visitor, heap_->Begin(), pointer_size);
}

void PatchOat::PatchImTables(const ImageHeader* image_header) {
  const PointerSize pointer_size = InstructionSetPointerSize(isa_);
  // We can safely walk target image since the conflict tables are independent.
  image_header->VisitPackedImTables(
      [this](ArtMethod* method) {
        return RelocatedAddressOfPointer(method);
      },
      image_->Begin(),
      pointer_size);
}

void PatchOat::PatchImtConflictTables(const ImageHeader* image_header) {
  const PointerSize pointer_size = InstructionSetPointerSize(isa_);
  // We can safely walk target image since the conflict tables are independent.
  image_header->VisitPackedImtConflictTables(
      [this](ArtMethod* method) {
        return RelocatedAddressOfPointer(method);
      },
      image_->Begin(),
      pointer_size);
}

class PatchOat::FixupRootVisitor : public RootVisitor {
 public:
  explicit FixupRootVisitor(const PatchOat* patch_oat) : patch_oat_(patch_oat) {
  }

  void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      *roots[i] = patch_oat_->RelocatedAddressOfPointer(*roots[i]);
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      roots[i]->Assign(patch_oat_->RelocatedAddressOfPointer(roots[i]->AsMirrorPtr()));
    }
  }

 private:
  const PatchOat* const patch_oat_;
};

void PatchOat::PatchInternedStrings(const ImageHeader* image_header) {
  const auto& section = image_header->GetInternedStringsSection();
  if (section.Size() == 0) {
    return;
  }
  InternTable temp_table;
  // Note that we require that ReadFromMemory does not make an internal copy of the elements.
  // This also relies on visit roots not doing any verification which could fail after we update
  // the roots to be the image addresses.
  temp_table.AddTableFromMemory(image_->Begin() + section.Offset());
  FixupRootVisitor visitor(this);
  temp_table.VisitRoots(&visitor, kVisitRootFlagAllRoots);
}

void PatchOat::PatchClassTable(const ImageHeader* image_header) {
  const auto& section = image_header->GetClassTableSection();
  if (section.Size() == 0) {
    return;
  }
  // Note that we require that ReadFromMemory does not make an internal copy of the elements.
  // This also relies on visit roots not doing any verification which could fail after we update
  // the roots to be the image addresses.
  WriterMutexLock mu(Thread::Current(), *Locks::classlinker_classes_lock_);
  ClassTable temp_table;
  temp_table.ReadFromMemory(image_->Begin() + section.Offset());
  FixupRootVisitor visitor(this);
  temp_table.VisitRoots(UnbufferedRootVisitor(&visitor, RootInfo(kRootUnknown)));
}


class PatchOat::RelocatedPointerVisitor {
 public:
  explicit RelocatedPointerVisitor(PatchOat* patch_oat) : patch_oat_(patch_oat) {}

  template <typename T>
  T* operator()(T* ptr, void** dest_addr ATTRIBUTE_UNUSED = 0) const {
    return patch_oat_->RelocatedAddressOfPointer(ptr);
  }

 private:
  PatchOat* const patch_oat_;
};

void PatchOat::PatchDexFileArrays(mirror::ObjectArray<mirror::Object>* img_roots) {
  auto* dex_caches = down_cast<mirror::ObjectArray<mirror::DexCache>*>(
      img_roots->Get(ImageHeader::kDexCaches));
  const PointerSize pointer_size = InstructionSetPointerSize(isa_);
  for (size_t i = 0, count = dex_caches->GetLength(); i < count; ++i) {
    auto* orig_dex_cache = dex_caches->GetWithoutChecks(i);
    auto* copy_dex_cache = RelocatedCopyOf(orig_dex_cache);
    // Though the DexCache array fields are usually treated as native pointers, we set the full
    // 64-bit values here, clearing the top 32 bits for 32-bit targets. The zero-extension is
    // done by casting to the unsigned type uintptr_t before casting to int64_t, i.e.
    //     static_cast<int64_t>(reinterpret_cast<uintptr_t>(image_begin_ + offset))).
    mirror::StringDexCacheType* orig_strings = orig_dex_cache->GetStrings();
    mirror::StringDexCacheType* relocated_strings = RelocatedAddressOfPointer(orig_strings);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::StringsOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_strings)));
    if (orig_strings != nullptr) {
      orig_dex_cache->FixupStrings(RelocatedCopyOf(orig_strings), RelocatedPointerVisitor(this));
    }
    mirror::TypeDexCacheType* orig_types = orig_dex_cache->GetResolvedTypes();
    mirror::TypeDexCacheType* relocated_types = RelocatedAddressOfPointer(orig_types);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::ResolvedTypesOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_types)));
    if (orig_types != nullptr) {
      orig_dex_cache->FixupResolvedTypes(RelocatedCopyOf(orig_types),
                                         RelocatedPointerVisitor(this));
    }
    mirror::MethodDexCacheType* orig_methods = orig_dex_cache->GetResolvedMethods();
    mirror::MethodDexCacheType* relocated_methods = RelocatedAddressOfPointer(orig_methods);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::ResolvedMethodsOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_methods)));
    if (orig_methods != nullptr) {
      mirror::MethodDexCacheType* copy_methods = RelocatedCopyOf(orig_methods);
      for (size_t j = 0, num = orig_dex_cache->NumResolvedMethods(); j != num; ++j) {
        mirror::MethodDexCachePair orig =
            mirror::DexCache::GetNativePairPtrSize(orig_methods, j, pointer_size);
        mirror::MethodDexCachePair copy(RelocatedAddressOfPointer(orig.object), orig.index);
        mirror::DexCache::SetNativePairPtrSize(copy_methods, j, copy, pointer_size);
      }
    }
    mirror::FieldDexCacheType* orig_fields = orig_dex_cache->GetResolvedFields();
    mirror::FieldDexCacheType* relocated_fields = RelocatedAddressOfPointer(orig_fields);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::ResolvedFieldsOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_fields)));
    if (orig_fields != nullptr) {
      mirror::FieldDexCacheType* copy_fields = RelocatedCopyOf(orig_fields);
      for (size_t j = 0, num = orig_dex_cache->NumResolvedFields(); j != num; ++j) {
        mirror::FieldDexCachePair orig =
            mirror::DexCache::GetNativePairPtrSize(orig_fields, j, pointer_size);
        mirror::FieldDexCachePair copy(RelocatedAddressOfPointer(orig.object), orig.index);
        mirror::DexCache::SetNativePairPtrSize(copy_fields, j, copy, pointer_size);
      }
    }
    mirror::MethodTypeDexCacheType* orig_method_types = orig_dex_cache->GetResolvedMethodTypes();
    mirror::MethodTypeDexCacheType* relocated_method_types =
        RelocatedAddressOfPointer(orig_method_types);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::ResolvedMethodTypesOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_method_types)));
    if (orig_method_types != nullptr) {
      orig_dex_cache->FixupResolvedMethodTypes(RelocatedCopyOf(orig_method_types),
                                               RelocatedPointerVisitor(this));
    }

    GcRoot<mirror::CallSite>* orig_call_sites = orig_dex_cache->GetResolvedCallSites();
    GcRoot<mirror::CallSite>* relocated_call_sites = RelocatedAddressOfPointer(orig_call_sites);
    copy_dex_cache->SetField64<false>(
        mirror::DexCache::ResolvedCallSitesOffset(),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(relocated_call_sites)));
    if (orig_call_sites != nullptr) {
      orig_dex_cache->FixupResolvedCallSites(RelocatedCopyOf(orig_call_sites),
                                             RelocatedPointerVisitor(this));
    }
  }
}

bool PatchOat::PatchImage(bool primary_image) {
  ImageHeader* image_header = reinterpret_cast<ImageHeader*>(image_->Begin());
  CHECK_GT(image_->Size(), sizeof(ImageHeader));
  // These are the roots from the original file.
  auto* img_roots = image_header->GetImageRoots();
  image_header->RelocateImage(delta_);

  PatchArtFields(image_header);
  PatchArtMethods(image_header);
  PatchImTables(image_header);
  PatchImtConflictTables(image_header);
  PatchInternedStrings(image_header);
  PatchClassTable(image_header);
  // Patch dex file int/long arrays which point to ArtFields.
  PatchDexFileArrays(img_roots);

  if (primary_image) {
    VisitObject(img_roots);
  }

  if (!image_header->IsValid()) {
    LOG(ERROR) << "relocation renders image header invalid";
    return false;
  }

  {
    TimingLogger::ScopedTiming t("Walk Bitmap", timings_);
    // Walk the bitmap.
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    auto visitor = [&](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
      VisitObject(obj);
    };
    bitmap_->Walk(visitor);
  }
  return true;
}


void PatchOat::PatchVisitor::operator() (ObjPtr<mirror::Object> obj,
                                         MemberOffset off,
                                         bool is_static_unused ATTRIBUTE_UNUSED) const {
  mirror::Object* referent = obj->GetFieldObject<mirror::Object, kVerifyNone>(off);
  mirror::Object* moved_object = patcher_->RelocatedAddressOfPointer(referent);
  copy_->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(off, moved_object);
}

void PatchOat::PatchVisitor::operator() (ObjPtr<mirror::Class> cls ATTRIBUTE_UNUSED,
                                         ObjPtr<mirror::Reference> ref) const {
  MemberOffset off = mirror::Reference::ReferentOffset();
  mirror::Object* referent = ref->GetReferent();
  DCHECK(referent == nullptr ||
         Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(referent)) << referent;
  mirror::Object* moved_object = patcher_->RelocatedAddressOfPointer(referent);
  copy_->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(off, moved_object);
}

// Called by PatchImage.
void PatchOat::VisitObject(mirror::Object* object) {
  mirror::Object* copy = RelocatedCopyOf(object);
  CHECK(copy != nullptr);
  if (kUseBakerReadBarrier) {
    object->AssertReadBarrierState();
  }
  PatchOat::PatchVisitor visitor(this, copy);
  object->VisitReferences<kVerifyNone>(visitor, visitor);
  if (object->IsClass<kVerifyNone>()) {
    const PointerSize pointer_size = InstructionSetPointerSize(isa_);
    mirror::Class* klass = object->AsClass();
    mirror::Class* copy_klass = down_cast<mirror::Class*>(copy);
    RelocatedPointerVisitor native_visitor(this);
    klass->FixupNativePointers(copy_klass, pointer_size, native_visitor);
    auto* vtable = klass->GetVTable();
    if (vtable != nullptr) {
      vtable->Fixup(RelocatedCopyOfFollowImages(vtable), pointer_size, native_visitor);
    }
    mirror::IfTable* iftable = klass->GetIfTable();
    for (int32_t i = 0; i < klass->GetIfTableCount(); ++i) {
      if (iftable->GetMethodArrayCount(i) > 0) {
        auto* method_array = iftable->GetMethodArray(i);
        CHECK(method_array != nullptr);
        method_array->Fixup(RelocatedCopyOfFollowImages(method_array),
                            pointer_size,
                            native_visitor);
      }
    }
  } else if (object->GetClass() == mirror::Method::StaticClass() ||
             object->GetClass() == mirror::Constructor::StaticClass()) {
    // Need to go update the ArtMethod.
    auto* dest = down_cast<mirror::Executable*>(copy);
    auto* src = down_cast<mirror::Executable*>(object);
    dest->SetArtMethod(RelocatedAddressOfPointer(src->GetArtMethod()));
  }
}

void PatchOat::FixupMethod(ArtMethod* object, ArtMethod* copy) {
  const PointerSize pointer_size = InstructionSetPointerSize(isa_);
  copy->CopyFrom(object, pointer_size);
  // Just update the entry points if it looks like we should.
  // TODO: sanity check all the pointers' values
  copy->SetDeclaringClass(RelocatedAddressOfPointer(object->GetDeclaringClass()));
  copy->SetEntryPointFromQuickCompiledCodePtrSize(RelocatedAddressOfPointer(
      object->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size)), pointer_size);
  // No special handling for IMT conflict table since all pointers are moved by the same offset.
  copy->SetDataPtrSize(RelocatedAddressOfPointer(
      object->GetDataPtrSize(pointer_size)), pointer_size);
}

static int orig_argc;
static char** orig_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < orig_argc; ++i) {
    command.push_back(orig_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: patchoat [options]...");
  UsageError("");
  UsageError("  --instruction-set=<isa>: Specifies the instruction set the patched code is");
  UsageError("      compiled for (required).");
  UsageError("");
  UsageError("  --input-image-location=<file.art>: Specifies the 'location' of the image file to");
  UsageError("      be patched.");
  UsageError("");
  UsageError("  --output-image-directory=<dir>: Specifies the directory to write the patched");
  UsageError("      image file(s) to.");
  UsageError("");
  UsageError("  --output-image-relocation-directory=<dir>: Specifies the directory to write");
  UsageError("      the image relocation information to.");
  UsageError("");
  UsageError("  --base-offset-delta=<delta>: Specify the amount to change the old base-offset by.");
  UsageError("      This value may be negative.");
  UsageError("");
  UsageError("  --verify: Verify an existing patched file instead of creating one.");
  UsageError("");
  UsageError("  --dump-timings: dump out patch timing information");
  UsageError("");
  UsageError("  --no-dump-timings: do not dump out patch timing information");
  UsageError("");

  exit(EXIT_FAILURE);
}

static int patchoat_patch_image(TimingLogger& timings,
                                InstructionSet isa,
                                const std::string& input_image_location,
                                const std::string& output_image_directory,
                                const std::string& output_image_relocation_directory,
                                off_t base_delta,
                                bool base_delta_set,
                                bool debug) {
  CHECK(!input_image_location.empty());
  if ((output_image_directory.empty()) && (output_image_relocation_directory.empty())) {
    Usage("Image patching requires --output-image-directory or --output-image-relocation-directory");
  }

  if (!base_delta_set) {
    Usage("Must supply a desired new offset or delta.");
  }

  if (!IsAligned<kPageSize>(base_delta)) {
    Usage("Base offset/delta must be aligned to a pagesize (0x%08x) boundary.", kPageSize);
  }

  if (debug) {
    LOG(INFO) << "moving offset by " << base_delta
        << " (0x" << std::hex << base_delta << ") bytes or "
        << std::dec << (base_delta/kPageSize) << " pages.";
  }

  TimingLogger::ScopedTiming pt("patch image and oat", &timings);

  bool ret =
      PatchOat::Patch(
          input_image_location,
          base_delta,
          output_image_directory,
          output_image_relocation_directory,
          isa,
          &timings);

  if (kIsDebugBuild) {
    LOG(INFO) << "Exiting with return ... " << ret;
  }
  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int patchoat_verify_image(TimingLogger& timings,
                                 InstructionSet isa,
                                 const std::string& input_image_location,
                                 const std::string& output_image_directory) {
  CHECK(!input_image_location.empty());
  TimingLogger::ScopedTiming pt("verify image and oat", &timings);

  bool ret =
      PatchOat::Verify(
          input_image_location,
          output_image_directory,
          isa,
          &timings);

  if (kIsDebugBuild) {
    LOG(INFO) << "Exiting with return ... " << ret;
  }
  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int patchoat(int argc, char **argv) {
  Locks::Init();
  InitLogging(argv, Runtime::Abort);
  MemMap::Init();
  const bool debug = kIsDebugBuild;
  orig_argc = argc;
  orig_argv = argv;
  TimingLogger timings("patcher", false, false);

  // Skip over the command name.
  argv++;
  argc--;

  if (argc == 0) {
    Usage("No arguments specified");
  }

  timings.StartTiming("Patchoat");

  // cmd line args
  bool isa_set = false;
  InstructionSet isa = InstructionSet::kNone;
  std::string input_image_location;
  std::string output_image_directory;
  std::string output_image_relocation_directory;
  off_t base_delta = 0;
  bool base_delta_set = false;
  bool dump_timings = kIsDebugBuild;
  bool verify = false;

  for (int i = 0; i < argc; ++i) {
    const StringPiece option(argv[i]);
    const bool log_options = false;
    if (log_options) {
      LOG(INFO) << "patchoat: option[" << i << "]=" << argv[i];
    }
    if (option.starts_with("--instruction-set=")) {
      isa_set = true;
      const char* isa_str = option.substr(strlen("--instruction-set=")).data();
      isa = GetInstructionSetFromString(isa_str);
      if (isa == InstructionSet::kNone) {
        Usage("Unknown or invalid instruction set %s", isa_str);
      }
    } else if (option.starts_with("--input-image-location=")) {
      input_image_location = option.substr(strlen("--input-image-location=")).data();
    } else if (option.starts_with("--output-image-directory=")) {
      output_image_directory = option.substr(strlen("--output-image-directory=")).data();
    } else if (option.starts_with("--output-image-relocation-directory=")) {
      output_image_relocation_directory =
          option.substr(strlen("--output-image-relocation-directory=")).data();
    } else if (option.starts_with("--base-offset-delta=")) {
      const char* base_delta_str = option.substr(strlen("--base-offset-delta=")).data();
      base_delta_set = true;
      if (!ParseInt(base_delta_str, &base_delta)) {
        Usage("Failed to parse --base-offset-delta argument '%s' as an off_t", base_delta_str);
      }
    } else if (option == "--dump-timings") {
      dump_timings = true;
    } else if (option == "--no-dump-timings") {
      dump_timings = false;
    } else if (option == "--verify") {
      verify = true;
    } else {
      Usage("Unknown argument %s", option.data());
    }
  }

  // The instruction set is mandatory. This simplifies things...
  if (!isa_set) {
    Usage("Instruction set must be set.");
  }

  int ret;
  if (verify) {
    ret = patchoat_verify_image(timings,
                                isa,
                                input_image_location,
                                output_image_directory);
  } else {
    ret = patchoat_patch_image(timings,
                               isa,
                               input_image_location,
                               output_image_directory,
                               output_image_relocation_directory,
                               base_delta,
                               base_delta_set,
                               debug);
  }

  timings.EndTiming();
  if (dump_timings) {
    LOG(INFO) << Dumpable<TimingLogger>(timings);
  }

  return ret;
}

}  // namespace art

int main(int argc, char **argv) {
  return art::patchoat(argc, argv);
}
