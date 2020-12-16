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

#include "profile_compilation_info.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <zlib.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>

#include "android-base/file.h"

#include "base/arena_allocator.h"
#include "base/dumpable.h"
#include "base/file_utils.h"
#include "base/logging.h"  // For VLOG.
#include "base/mutex.h"
#include "base/os.h"
#include "base/safe_map.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "dex/dex_file_loader.h"
#include "jit/profiling_info.h"
#include "zip_archive.h"

namespace art {

const uint8_t ProfileCompilationInfo::kProfileMagic[] = { 'p', 'r', 'o', '\0' };
// Last profile version: merge profiles directly from the file without creating
// profile_compilation_info object. All the profile line headers are now placed together
// before corresponding method_encodings and class_ids.
const uint8_t ProfileCompilationInfo::kProfileVersion[] = { '0', '1', '0', '\0' };

// The name of the profile entry in the dex metadata file.
// DO NOT CHANGE THIS! (it's similar to classes.dex in the apk files).
const char* ProfileCompilationInfo::kDexMetadataProfileEntry = "primary.prof";

static constexpr uint16_t kMaxDexFileKeyLength = PATH_MAX;

// Debug flag to ignore checksums when testing if a method or a class is present in the profile.
// Used to facilitate testing profile guided compilation across a large number of apps
// using the same test profile.
static constexpr bool kDebugIgnoreChecksum = false;

static constexpr uint8_t kIsMissingTypesEncoding = 6;
static constexpr uint8_t kIsMegamorphicEncoding = 7;

static_assert(sizeof(InlineCache::kIndividualCacheSize) == sizeof(uint8_t),
              "InlineCache::kIndividualCacheSize does not have the expect type size");
static_assert(InlineCache::kIndividualCacheSize < kIsMegamorphicEncoding,
              "InlineCache::kIndividualCacheSize is larger than expected");
static_assert(InlineCache::kIndividualCacheSize < kIsMissingTypesEncoding,
              "InlineCache::kIndividualCacheSize is larger than expected");

static bool ChecksumMatch(uint32_t dex_file_checksum, uint32_t checksum) {
  return kDebugIgnoreChecksum || dex_file_checksum == checksum;
}

ProfileCompilationInfo::ProfileCompilationInfo(ArenaPool* custom_arena_pool)
    : default_arena_pool_(),
      allocator_(custom_arena_pool),
      info_(allocator_.Adapter(kArenaAllocProfile)),
      profile_key_map_(std::less<const std::string>(), allocator_.Adapter(kArenaAllocProfile)) {
}

ProfileCompilationInfo::ProfileCompilationInfo()
    : default_arena_pool_(/*use_malloc*/true, /*low_4gb*/false, "ProfileCompilationInfo"),
      allocator_(&default_arena_pool_),
      info_(allocator_.Adapter(kArenaAllocProfile)),
      profile_key_map_(std::less<const std::string>(), allocator_.Adapter(kArenaAllocProfile)) {
}

ProfileCompilationInfo::~ProfileCompilationInfo() {
  VLOG(profiler) << Dumpable<MemStats>(allocator_.GetMemStats());
  ClearData();
}

void ProfileCompilationInfo::DexPcData::AddClass(uint16_t dex_profile_idx,
                                                 const dex::TypeIndex& type_idx) {
  if (is_megamorphic || is_missing_types) {
    return;
  }

  // Perform an explicit lookup for the type instead of directly emplacing the
  // element. We do this because emplace() allocates the node before doing the
  // lookup and if it then finds an identical element, it shall deallocate the
  // node. For Arena allocations, that's essentially a leak.
  ClassReference ref(dex_profile_idx, type_idx);
  auto it = classes.find(ref);
  if (it != classes.end()) {
    // The type index exists.
    return;
  }

  // Check if the adding the type will cause the cache to become megamorphic.
  if (classes.size() + 1 >= InlineCache::kIndividualCacheSize) {
    is_megamorphic = true;
    classes.clear();
    return;
  }

  // The type does not exist and the inline cache will not be megamorphic.
  classes.insert(ref);
}

// Transform the actual dex location into relative paths.
// Note: this is OK because we don't store profiles of different apps into the same file.
// Apps with split apks don't cause trouble because each split has a different name and will not
// collide with other entries.
std::string ProfileCompilationInfo::GetProfileDexFileKey(const std::string& dex_location) {
  DCHECK(!dex_location.empty());
  size_t last_sep_index = dex_location.find_last_of('/');
  if (last_sep_index == std::string::npos) {
    return dex_location;
  } else {
    DCHECK(last_sep_index < dex_location.size());
    return dex_location.substr(last_sep_index + 1);
  }
}

bool ProfileCompilationInfo::AddMethodIndex(MethodHotness::Flag flags, const MethodReference& ref) {
  DexFileData* data = GetOrAddDexFileData(ref.dex_file);
  if (data == nullptr) {
    return false;
  }
  return data->AddMethod(flags, ref.index);
}

bool ProfileCompilationInfo::AddMethodIndex(MethodHotness::Flag flags,
                                            const std::string& dex_location,
                                            uint32_t checksum,
                                            uint16_t method_idx,
                                            uint32_t num_method_ids) {
  DexFileData* data = GetOrAddDexFileData(GetProfileDexFileKey(dex_location),
                                          checksum,
                                          num_method_ids);
  if (data == nullptr) {
    return false;
  }
  return data->AddMethod(flags, method_idx);
}

bool ProfileCompilationInfo::AddMethods(const std::vector<ProfileMethodInfo>& methods,
                                        MethodHotness::Flag flags) {
  for (const ProfileMethodInfo& method : methods) {
    if (!AddMethod(method, flags)) {
      return false;
    }
  }
  return true;
}

bool ProfileCompilationInfo::AddClasses(const std::set<DexCacheResolvedClasses>& resolved_classes) {
  for (const DexCacheResolvedClasses& dex_cache : resolved_classes) {
    if (!AddResolvedClasses(dex_cache)) {
      return false;
    }
  }
  return true;
}

bool ProfileCompilationInfo::MergeWith(const std::string& filename) {
  std::string error;
  int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC;
  ScopedFlock profile_file = LockedFile::Open(filename.c_str(), flags,
      /*block*/false, &error);

  if (profile_file.get() == nullptr) {
    LOG(WARNING) << "Couldn't lock the profile file " << filename << ": " << error;
    return false;
  }

  int fd = profile_file->Fd();

  ProfileLoadStatus status = LoadInternal(fd, &error);
  if (status == kProfileLoadSuccess) {
    return true;
  }

  LOG(WARNING) << "Could not load profile data from file " << filename << ": " << error;
  return false;
}

bool ProfileCompilationInfo::Load(const std::string& filename, bool clear_if_invalid) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  std::string error;

  if (!IsEmpty()) {
    return kProfileLoadWouldOverwiteData;
  }

  int flags = O_RDWR | O_NOFOLLOW | O_CLOEXEC;
  // There's no need to fsync profile data right away. We get many chances
  // to write it again in case something goes wrong. We can rely on a simple
  // close(), no sync, and let to the kernel decide when to write to disk.
  ScopedFlock profile_file = LockedFile::Open(filename.c_str(), flags,
                                              /*block*/false, &error);

  if (profile_file.get() == nullptr) {
    LOG(WARNING) << "Couldn't lock the profile file " << filename << ": " << error;
    return false;
  }

  int fd = profile_file->Fd();

  ProfileLoadStatus status = LoadInternal(fd, &error);
  if (status == kProfileLoadSuccess) {
    return true;
  }

  if (clear_if_invalid &&
      ((status == kProfileLoadVersionMismatch) || (status == kProfileLoadBadData))) {
    LOG(WARNING) << "Clearing bad or obsolete profile data from file "
                 << filename << ": " << error;
    if (profile_file->ClearContent()) {
      return true;
    } else {
      PLOG(WARNING) << "Could not clear profile file: " << filename;
      return false;
    }
  }

  LOG(WARNING) << "Could not load profile data from file " << filename << ": " << error;
  return false;
}

bool ProfileCompilationInfo::Save(const std::string& filename, uint64_t* bytes_written) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  std::string error;
  int flags = O_WRONLY | O_NOFOLLOW | O_CLOEXEC;
  // There's no need to fsync profile data right away. We get many chances
  // to write it again in case something goes wrong. We can rely on a simple
  // close(), no sync, and let to the kernel decide when to write to disk.
  ScopedFlock profile_file = LockedFile::Open(filename.c_str(), flags,
                                              /*block*/false, &error);
  if (profile_file.get() == nullptr) {
    LOG(WARNING) << "Couldn't lock the profile file " << filename << ": " << error;
    return false;
  }

  int fd = profile_file->Fd();

  // We need to clear the data because we don't support appending to the profiles yet.
  if (!profile_file->ClearContent()) {
    PLOG(WARNING) << "Could not clear profile file: " << filename;
    return false;
  }

  // This doesn't need locking because we are trying to lock the file for exclusive
  // access and fail immediately if we can't.
  bool result = Save(fd);
  if (result) {
    int64_t size = OS::GetFileSizeBytes(filename.c_str());
    if (size != -1) {
      VLOG(profiler)
        << "Successfully saved profile info to " << filename << " Size: "
        << size;
      if (bytes_written != nullptr) {
        *bytes_written = static_cast<uint64_t>(size);
      }
    }
  } else {
    VLOG(profiler) << "Failed to save profile info to " << filename;
  }
  return result;
}

// Returns true if all the bytes were successfully written to the file descriptor.
static bool WriteBuffer(int fd, const uint8_t* buffer, size_t byte_count) {
  while (byte_count > 0) {
    int bytes_written = TEMP_FAILURE_RETRY(write(fd, buffer, byte_count));
    if (bytes_written == -1) {
      return false;
    }
    byte_count -= bytes_written;  // Reduce the number of remaining bytes.
    buffer += bytes_written;  // Move the buffer forward.
  }
  return true;
}

// Add the string bytes to the buffer.
static void AddStringToBuffer(std::vector<uint8_t>* buffer, const std::string& value) {
  buffer->insert(buffer->end(), value.begin(), value.end());
}

// Insert each byte, from low to high into the buffer.
template <typename T>
static void AddUintToBuffer(std::vector<uint8_t>* buffer, T value) {
  for (size_t i = 0; i < sizeof(T); i++) {
    buffer->push_back((value >> (i * kBitsPerByte)) & 0xff);
  }
}

static constexpr size_t kLineHeaderSize =
    2 * sizeof(uint16_t) +  // class_set.size + dex_location.size
    3 * sizeof(uint32_t);   // method_map.size + checksum + num_method_ids

/**
 * Serialization format:
 * [profile_header, zipped[[profile_line_header1, profile_line_header2...],[profile_line_data1,
 *    profile_line_data2...]]]
 * profile_header:
 *   magic,version,number_of_dex_files,uncompressed_size_of_zipped_data,compressed_data_size
 * profile_line_header:
 *   dex_location,number_of_classes,methods_region_size,dex_location_checksum,num_method_ids
 * profile_line_data:
 *   method_encoding_1,method_encoding_2...,class_id1,class_id2...,startup/post startup bitmap
 * The method_encoding is:
 *    method_id,number_of_inline_caches,inline_cache1,inline_cache2...
 * The inline_cache is:
 *    dex_pc,[M|dex_map_size], dex_profile_index,class_id1,class_id2...,dex_profile_index2,...
 *    dex_map_size is the number of dex_indeces that follows.
 *       Classes are grouped per their dex files and the line
 *       `dex_profile_index,class_id1,class_id2...,dex_profile_index2,...` encodes the
 *       mapping from `dex_profile_index` to the set of classes `class_id1,class_id2...`
 *    M stands for megamorphic or missing types and it's encoded as either
 *    the byte kIsMegamorphicEncoding or kIsMissingTypesEncoding.
 *    When present, there will be no class ids following.
 **/
bool ProfileCompilationInfo::Save(int fd) {
  uint64_t start = NanoTime();
  ScopedTrace trace(__PRETTY_FUNCTION__);
  DCHECK_GE(fd, 0);

  // Use a vector wrapper to avoid keeping track of offsets when we add elements.
  std::vector<uint8_t> buffer;
  if (!WriteBuffer(fd, kProfileMagic, sizeof(kProfileMagic))) {
    return false;
  }
  if (!WriteBuffer(fd, kProfileVersion, sizeof(kProfileVersion))) {
    return false;
  }
  DCHECK_LE(info_.size(), std::numeric_limits<uint8_t>::max());
  AddUintToBuffer(&buffer, static_cast<uint8_t>(info_.size()));

  uint32_t required_capacity = 0;
  for (const DexFileData* dex_data_ptr : info_) {
    const DexFileData& dex_data = *dex_data_ptr;
    uint32_t methods_region_size = GetMethodsRegionSize(dex_data);
    required_capacity += kLineHeaderSize +
        dex_data.profile_key.size() +
        sizeof(uint16_t) * dex_data.class_set.size() +
        methods_region_size +
        dex_data.bitmap_storage.size();
  }
  // Allow large profiles for non target builds for the case where we are merging many profiles
  // to generate a boot image profile.
  if (kIsTargetBuild && required_capacity > kProfileSizeErrorThresholdInBytes) {
    LOG(ERROR) << "Profile data size exceeds "
               << std::to_string(kProfileSizeErrorThresholdInBytes)
               << " bytes. Profile will not be written to disk.";
    return false;
  }
  AddUintToBuffer(&buffer, required_capacity);
  if (!WriteBuffer(fd, buffer.data(), buffer.size())) {
    return false;
  }
  // Make sure that the buffer has enough capacity to avoid repeated resizings
  // while we add data.
  buffer.reserve(required_capacity);
  buffer.clear();

  // Dex files must be written in the order of their profile index. This
  // avoids writing the index in the output file and simplifies the parsing logic.
  // Write profile line headers.
  for (const DexFileData* dex_data_ptr : info_) {
    const DexFileData& dex_data = *dex_data_ptr;

    if (dex_data.profile_key.size() >= kMaxDexFileKeyLength) {
      LOG(WARNING) << "DexFileKey exceeds allocated limit";
      return false;
    }

    uint32_t methods_region_size = GetMethodsRegionSize(dex_data);

    DCHECK_LE(dex_data.profile_key.size(), std::numeric_limits<uint16_t>::max());
    DCHECK_LE(dex_data.class_set.size(), std::numeric_limits<uint16_t>::max());
    // Write profile line header.
    AddUintToBuffer(&buffer, static_cast<uint16_t>(dex_data.profile_key.size()));
    AddUintToBuffer(&buffer, static_cast<uint16_t>(dex_data.class_set.size()));
    AddUintToBuffer(&buffer, methods_region_size);  // uint32_t
    AddUintToBuffer(&buffer, dex_data.checksum);  // uint32_t
    AddUintToBuffer(&buffer, dex_data.num_method_ids);  // uint32_t

    AddStringToBuffer(&buffer, dex_data.profile_key);
  }

  for (const DexFileData* dex_data_ptr : info_) {
    const DexFileData& dex_data = *dex_data_ptr;

    // Note that we allow dex files without any methods or classes, so that
    // inline caches can refer valid dex files.

    uint16_t last_method_index = 0;
    for (const auto& method_it : dex_data.method_map) {
      // Store the difference between the method indices. The SafeMap is ordered by
      // method_id, so the difference will always be non negative.
      DCHECK_GE(method_it.first, last_method_index);
      uint16_t diff_with_last_method_index = method_it.first - last_method_index;
      last_method_index = method_it.first;
      AddUintToBuffer(&buffer, diff_with_last_method_index);
      AddInlineCacheToBuffer(&buffer, method_it.second);
    }

    uint16_t last_class_index = 0;
    for (const auto& class_id : dex_data.class_set) {
      // Store the difference between the class indices. The set is ordered by
      // class_id, so the difference will always be non negative.
      DCHECK_GE(class_id.index_, last_class_index);
      uint16_t diff_with_last_class_index = class_id.index_ - last_class_index;
      last_class_index = class_id.index_;
      AddUintToBuffer(&buffer, diff_with_last_class_index);
    }

    buffer.insert(buffer.end(),
                  dex_data.bitmap_storage.begin(),
                  dex_data.bitmap_storage.end());
  }

  uint32_t output_size = 0;
  std::unique_ptr<uint8_t[]> compressed_buffer = DeflateBuffer(buffer.data(),
                                                               required_capacity,
                                                               &output_size);

  if (output_size > kProfileSizeWarningThresholdInBytes) {
    LOG(WARNING) << "Profile data size exceeds "
                 << std::to_string(kProfileSizeWarningThresholdInBytes);
  }

  buffer.clear();
  AddUintToBuffer(&buffer, output_size);

  if (!WriteBuffer(fd, buffer.data(), buffer.size())) {
    return false;
  }
  if (!WriteBuffer(fd, compressed_buffer.get(), output_size)) {
    return false;
  }
  uint64_t total_time = NanoTime() - start;
  VLOG(profiler) << "Compressed from "
                 << std::to_string(required_capacity)
                 << " to "
                 << std::to_string(output_size);
  VLOG(profiler) << "Time to save profile: " << std::to_string(total_time);
  return true;
}

void ProfileCompilationInfo::AddInlineCacheToBuffer(std::vector<uint8_t>* buffer,
                                                    const InlineCacheMap& inline_cache_map) {
  // Add inline cache map size.
  AddUintToBuffer(buffer, static_cast<uint16_t>(inline_cache_map.size()));
  if (inline_cache_map.size() == 0) {
    return;
  }
  for (const auto& inline_cache_it : inline_cache_map) {
    uint16_t dex_pc = inline_cache_it.first;
    const DexPcData dex_pc_data = inline_cache_it.second;
    const ClassSet& classes = dex_pc_data.classes;

    // Add the dex pc.
    AddUintToBuffer(buffer, dex_pc);

    // Add the megamorphic/missing_types encoding if needed and continue.
    // In either cases we don't add any classes to the profiles and so there's
    // no point to continue.
    // TODO(calin): in case we miss types there is still value to add the
    // rest of the classes. They can be added without bumping the profile version.
    if (dex_pc_data.is_missing_types) {
      DCHECK(!dex_pc_data.is_megamorphic);  // at this point the megamorphic flag should not be set.
      DCHECK_EQ(classes.size(), 0u);
      AddUintToBuffer(buffer, kIsMissingTypesEncoding);
      continue;
    } else if (dex_pc_data.is_megamorphic) {
      DCHECK_EQ(classes.size(), 0u);
      AddUintToBuffer(buffer, kIsMegamorphicEncoding);
      continue;
    }

    DCHECK_LT(classes.size(), InlineCache::kIndividualCacheSize);
    DCHECK_NE(classes.size(), 0u) << "InlineCache contains a dex_pc with 0 classes";

    SafeMap<uint8_t, std::vector<dex::TypeIndex>> dex_to_classes_map;
    // Group the classes by dex. We expect that most of the classes will come from
    // the same dex, so this will be more efficient than encoding the dex index
    // for each class reference.
    GroupClassesByDex(classes, &dex_to_classes_map);
    // Add the dex map size.
    AddUintToBuffer(buffer, static_cast<uint8_t>(dex_to_classes_map.size()));
    for (const auto& dex_it : dex_to_classes_map) {
      uint8_t dex_profile_index = dex_it.first;
      const std::vector<dex::TypeIndex>& dex_classes = dex_it.second;
      // Add the dex profile index.
      AddUintToBuffer(buffer, dex_profile_index);
      // Add the the number of classes for each dex profile index.
      AddUintToBuffer(buffer, static_cast<uint8_t>(dex_classes.size()));
      for (size_t i = 0; i < dex_classes.size(); i++) {
        // Add the type index of the classes.
        AddUintToBuffer(buffer, dex_classes[i].index_);
      }
    }
  }
}

uint32_t ProfileCompilationInfo::GetMethodsRegionSize(const DexFileData& dex_data) {
  // ((uint16_t)method index + (uint16_t)inline cache size) * number of methods
  uint32_t size = 2 * sizeof(uint16_t) * dex_data.method_map.size();
  for (const auto& method_it : dex_data.method_map) {
    const InlineCacheMap& inline_cache = method_it.second;
    size += sizeof(uint16_t) * inline_cache.size();  // dex_pc
    for (const auto& inline_cache_it : inline_cache) {
      const ClassSet& classes = inline_cache_it.second.classes;
      SafeMap<uint8_t, std::vector<dex::TypeIndex>> dex_to_classes_map;
      GroupClassesByDex(classes, &dex_to_classes_map);
      size += sizeof(uint8_t);  // dex_to_classes_map size
      for (const auto& dex_it : dex_to_classes_map) {
        size += sizeof(uint8_t);  // dex profile index
        size += sizeof(uint8_t);  // number of classes
        const std::vector<dex::TypeIndex>& dex_classes = dex_it.second;
        size += sizeof(uint16_t) * dex_classes.size();  // the actual classes
      }
    }
  }
  return size;
}

void ProfileCompilationInfo::GroupClassesByDex(
    const ClassSet& classes,
    /*out*/SafeMap<uint8_t, std::vector<dex::TypeIndex>>* dex_to_classes_map) {
  for (const auto& classes_it : classes) {
    auto dex_it = dex_to_classes_map->FindOrAdd(classes_it.dex_profile_index);
    dex_it->second.push_back(classes_it.type_index);
  }
}

ProfileCompilationInfo::DexFileData* ProfileCompilationInfo::GetOrAddDexFileData(
    const std::string& profile_key,
    uint32_t checksum,
    uint32_t num_method_ids) {
  const auto profile_index_it = profile_key_map_.FindOrAdd(profile_key, profile_key_map_.size());
  if (profile_key_map_.size() > std::numeric_limits<uint8_t>::max()) {
    // Allow only 255 dex files to be profiled. This allows us to save bytes
    // when encoding. The number is well above what we expect for normal applications.
    if (kIsDebugBuild) {
      LOG(ERROR) << "Exceeded the maximum number of dex files (255). Something went wrong";
    }
    profile_key_map_.erase(profile_key);
    return nullptr;
  }

  uint8_t profile_index = profile_index_it->second;
  if (info_.size() <= profile_index) {
    // This is a new addition. Add it to the info_ array.
    DexFileData* dex_file_data = new (&allocator_) DexFileData(
        &allocator_,
        profile_key,
        checksum,
        profile_index,
        num_method_ids);
    info_.push_back(dex_file_data);
  }
  DexFileData* result = info_[profile_index];

  // Check that the checksum matches.
  // This may different if for example the dex file was updated and we had a record of the old one.
  if (result->checksum != checksum) {
    LOG(WARNING) << "Checksum mismatch for dex " << profile_key;
    return nullptr;
  }

  // DCHECK that profile info map key is consistent with the one stored in the dex file data.
  // This should always be the case since since the cache map is managed by ProfileCompilationInfo.
  DCHECK_EQ(profile_key, result->profile_key);
  DCHECK_EQ(profile_index, result->profile_index);

  if (num_method_ids != result->num_method_ids) {
    // This should not happen... added to help investigating b/65812889.
    LOG(ERROR) << "num_method_ids mismatch for dex " << profile_key
        << ", expected=" << num_method_ids
        << ", actual=" << result->num_method_ids;
    return nullptr;
  }

  return result;
}

const ProfileCompilationInfo::DexFileData* ProfileCompilationInfo::FindDexData(
      const std::string& profile_key,
      uint32_t checksum,
      bool verify_checksum) const {
  const auto profile_index_it = profile_key_map_.find(profile_key);
  if (profile_index_it == profile_key_map_.end()) {
    return nullptr;
  }

  uint8_t profile_index = profile_index_it->second;
  const DexFileData* result = info_[profile_index];
  if (verify_checksum && !ChecksumMatch(result->checksum, checksum)) {
    return nullptr;
  }
  DCHECK_EQ(profile_key, result->profile_key);
  DCHECK_EQ(profile_index, result->profile_index);
  return result;
}

bool ProfileCompilationInfo::AddResolvedClasses(const DexCacheResolvedClasses& classes) {
  const std::string dex_location = GetProfileDexFileKey(classes.GetDexLocation());
  const uint32_t checksum = classes.GetLocationChecksum();
  DexFileData* const data = GetOrAddDexFileData(dex_location, checksum, classes.NumMethodIds());
  if (data == nullptr) {
    return false;
  }
  data->class_set.insert(classes.GetClasses().begin(), classes.GetClasses().end());
  return true;
}

bool ProfileCompilationInfo::AddMethod(const std::string& dex_location,
                                       uint32_t dex_checksum,
                                       uint16_t method_index,
                                       uint32_t num_method_ids,
                                       const OfflineProfileMethodInfo& pmi,
                                       MethodHotness::Flag flags) {
  DexFileData* const data = GetOrAddDexFileData(GetProfileDexFileKey(dex_location),
                                                dex_checksum,
                                                num_method_ids);
  if (data == nullptr) {
    // The data is null if there is a mismatch in the checksum or number of method ids.
    return false;
  }

  // Add the method.
  InlineCacheMap* inline_cache = data->FindOrAddMethod(method_index);
  if (inline_cache == nullptr) {
    // Happens if the method index is outside the range (i.e. is greater then the number
    // of methods in the dex file). This should not happen during normal execution,
    // But tools (e.g. boot image aggregation tools) and tests stress this behaviour.
    return false;
  }

  data->SetMethodHotness(method_index, flags);

  if (pmi.inline_caches == nullptr) {
    // If we don't have inline caches return success right away.
    return true;
  }
  for (const auto& pmi_inline_cache_it : *pmi.inline_caches) {
    uint16_t pmi_ic_dex_pc = pmi_inline_cache_it.first;
    const DexPcData& pmi_ic_dex_pc_data = pmi_inline_cache_it.second;
    DexPcData* dex_pc_data = FindOrAddDexPc(inline_cache, pmi_ic_dex_pc);
    if (dex_pc_data->is_missing_types || dex_pc_data->is_megamorphic) {
      // We are already megamorphic or we are missing types; no point in going forward.
      continue;
    }

    if (pmi_ic_dex_pc_data.is_missing_types) {
      dex_pc_data->SetIsMissingTypes();
      continue;
    }
    if (pmi_ic_dex_pc_data.is_megamorphic) {
      dex_pc_data->SetIsMegamorphic();
      continue;
    }

    for (const ClassReference& class_ref : pmi_ic_dex_pc_data.classes) {
      const DexReference& dex_ref = pmi.dex_references[class_ref.dex_profile_index];
      DexFileData* class_dex_data = GetOrAddDexFileData(
          GetProfileDexFileKey(dex_ref.dex_location),
          dex_ref.dex_checksum,
          dex_ref.num_method_ids);
      if (class_dex_data == nullptr) {  // checksum mismatch
        return false;
      }
      dex_pc_data->AddClass(class_dex_data->profile_index, class_ref.type_index);
    }
  }
  return true;
}

bool ProfileCompilationInfo::AddMethod(const ProfileMethodInfo& pmi, MethodHotness::Flag flags) {
  DexFileData* const data = GetOrAddDexFileData(pmi.ref.dex_file);
  if (data == nullptr) {  // checksum mismatch
    return false;
  }
  InlineCacheMap* inline_cache = data->FindOrAddMethod(pmi.ref.index);
  if (inline_cache == nullptr) {
    return false;
  }
  data->SetMethodHotness(pmi.ref.index, flags);

  for (const ProfileMethodInfo::ProfileInlineCache& cache : pmi.inline_caches) {
    if (cache.is_missing_types) {
      FindOrAddDexPc(inline_cache, cache.dex_pc)->SetIsMissingTypes();
      continue;
    }
    for (const TypeReference& class_ref : cache.classes) {
      DexFileData* class_dex_data = GetOrAddDexFileData(class_ref.dex_file);
      if (class_dex_data == nullptr) {  // checksum mismatch
        return false;
      }
      DexPcData* dex_pc_data = FindOrAddDexPc(inline_cache, cache.dex_pc);
      if (dex_pc_data->is_missing_types) {
        // Don't bother adding classes if we are missing types.
        break;
      }
      dex_pc_data->AddClass(class_dex_data->profile_index, class_ref.TypeIndex());
    }
  }
  return true;
}

bool ProfileCompilationInfo::AddClassIndex(const std::string& dex_location,
                                           uint32_t checksum,
                                           dex::TypeIndex type_idx,
                                           uint32_t num_method_ids) {
  DexFileData* const data = GetOrAddDexFileData(dex_location, checksum, num_method_ids);
  if (data == nullptr) {
    return false;
  }
  data->class_set.insert(type_idx);
  return true;
}

#define READ_UINT(type, buffer, dest, error)            \
  do {                                                  \
    if (!(buffer).ReadUintAndAdvance<type>(&(dest))) {  \
      *(error) = "Could not read "#dest;                \
      return false;                                     \
    }                                                   \
  }                                                     \
  while (false)

bool ProfileCompilationInfo::ReadInlineCache(
    SafeBuffer& buffer,
    uint8_t number_of_dex_files,
    const SafeMap<uint8_t, uint8_t>& dex_profile_index_remap,
    /*out*/ InlineCacheMap* inline_cache,
    /*out*/ std::string* error) {
  uint16_t inline_cache_size;
  READ_UINT(uint16_t, buffer, inline_cache_size, error);
  for (; inline_cache_size > 0; inline_cache_size--) {
    uint16_t dex_pc;
    uint8_t dex_to_classes_map_size;
    READ_UINT(uint16_t, buffer, dex_pc, error);
    READ_UINT(uint8_t, buffer, dex_to_classes_map_size, error);
    DexPcData* dex_pc_data = FindOrAddDexPc(inline_cache, dex_pc);
    if (dex_to_classes_map_size == kIsMissingTypesEncoding) {
      dex_pc_data->SetIsMissingTypes();
      continue;
    }
    if (dex_to_classes_map_size == kIsMegamorphicEncoding) {
      dex_pc_data->SetIsMegamorphic();
      continue;
    }
    for (; dex_to_classes_map_size > 0; dex_to_classes_map_size--) {
      uint8_t dex_profile_index;
      uint8_t dex_classes_size;
      READ_UINT(uint8_t, buffer, dex_profile_index, error);
      READ_UINT(uint8_t, buffer, dex_classes_size, error);
      if (dex_profile_index >= number_of_dex_files) {
        *error = "dex_profile_index out of bounds ";
        *error += std::to_string(dex_profile_index) + " " + std::to_string(number_of_dex_files);
        return false;
      }
      for (; dex_classes_size > 0; dex_classes_size--) {
        uint16_t type_index;
        READ_UINT(uint16_t, buffer, type_index, error);
        auto it = dex_profile_index_remap.find(dex_profile_index);
        if (it == dex_profile_index_remap.end()) {
          // If we don't have an index that's because the dex file was filtered out when loading.
          // Set missing types on the dex pc data.
          dex_pc_data->SetIsMissingTypes();
        } else {
          dex_pc_data->AddClass(it->second, dex::TypeIndex(type_index));
        }
      }
    }
  }
  return true;
}

bool ProfileCompilationInfo::ReadMethods(SafeBuffer& buffer,
                                         uint8_t number_of_dex_files,
                                         const ProfileLineHeader& line_header,
                                         const SafeMap<uint8_t, uint8_t>& dex_profile_index_remap,
                                         /*out*/std::string* error) {
  uint32_t unread_bytes_before_operation = buffer.CountUnreadBytes();
  if (unread_bytes_before_operation < line_header.method_region_size_bytes) {
    *error += "Profile EOF reached prematurely for ReadMethod";
    return kProfileLoadBadData;
  }
  size_t expected_unread_bytes_after_operation = buffer.CountUnreadBytes()
      - line_header.method_region_size_bytes;
  uint16_t last_method_index = 0;
  while (buffer.CountUnreadBytes() > expected_unread_bytes_after_operation) {
    DexFileData* const data = GetOrAddDexFileData(line_header.dex_location,
                                                  line_header.checksum,
                                                  line_header.num_method_ids);
    uint16_t diff_with_last_method_index;
    READ_UINT(uint16_t, buffer, diff_with_last_method_index, error);
    uint16_t method_index = last_method_index + diff_with_last_method_index;
    last_method_index = method_index;
    InlineCacheMap* inline_cache = data->FindOrAddMethod(method_index);
    if (inline_cache == nullptr) {
      return false;
    }
    if (!ReadInlineCache(buffer,
                         number_of_dex_files,
                         dex_profile_index_remap,
                         inline_cache,
                         error)) {
      return false;
    }
  }
  uint32_t total_bytes_read = unread_bytes_before_operation - buffer.CountUnreadBytes();
  if (total_bytes_read != line_header.method_region_size_bytes) {
    *error += "Profile data inconsistent for ReadMethods";
    return false;
  }
  return true;
}

bool ProfileCompilationInfo::ReadClasses(SafeBuffer& buffer,
                                         const ProfileLineHeader& line_header,
                                         /*out*/std::string* error) {
  size_t unread_bytes_before_op = buffer.CountUnreadBytes();
  if (unread_bytes_before_op < line_header.class_set_size) {
    *error += "Profile EOF reached prematurely for ReadClasses";
    return kProfileLoadBadData;
  }

  uint16_t last_class_index = 0;
  for (uint16_t i = 0; i < line_header.class_set_size; i++) {
    uint16_t diff_with_last_class_index;
    READ_UINT(uint16_t, buffer, diff_with_last_class_index, error);
    uint16_t type_index = last_class_index + diff_with_last_class_index;
    last_class_index = type_index;
    if (!AddClassIndex(line_header.dex_location,
                       line_header.checksum,
                       dex::TypeIndex(type_index),
                       line_header.num_method_ids)) {
      return false;
    }
  }
  size_t total_bytes_read = unread_bytes_before_op - buffer.CountUnreadBytes();
  uint32_t expected_bytes_read = line_header.class_set_size * sizeof(uint16_t);
  if (total_bytes_read != expected_bytes_read) {
    *error += "Profile data inconsistent for ReadClasses";
    return false;
  }
  return true;
}

// Tests for EOF by trying to read 1 byte from the descriptor.
// Returns:
//   0 if the descriptor is at the EOF,
//  -1 if there was an IO error
//   1 if the descriptor has more content to read
static int testEOF(int fd) {
  uint8_t buffer[1];
  return TEMP_FAILURE_RETRY(read(fd, buffer, 1));
}

// Reads an uint value previously written with AddUintToBuffer.
template <typename T>
bool ProfileCompilationInfo::SafeBuffer::ReadUintAndAdvance(/*out*/T* value) {
  static_assert(std::is_unsigned<T>::value, "Type is not unsigned");
  if (ptr_current_ + sizeof(T) > ptr_end_) {
    return false;
  }
  *value = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    *value += ptr_current_[i] << (i * kBitsPerByte);
  }
  ptr_current_ += sizeof(T);
  return true;
}

bool ProfileCompilationInfo::SafeBuffer::CompareAndAdvance(const uint8_t* data, size_t data_size) {
  if (ptr_current_ + data_size > ptr_end_) {
    return false;
  }
  if (memcmp(ptr_current_, data, data_size) == 0) {
    ptr_current_ += data_size;
    return true;
  }
  return false;
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::SafeBuffer::Fill(
      ProfileSource& source,
      const std::string& debug_stage,
      /*out*/ std::string* error) {
  size_t byte_count = (ptr_end_ - ptr_current_) * sizeof(*ptr_current_);
  uint8_t* buffer = ptr_current_;
  return source.Read(buffer, byte_count, debug_stage, error);
}

size_t ProfileCompilationInfo::SafeBuffer::CountUnreadBytes() {
  return (ptr_end_ - ptr_current_) * sizeof(*ptr_current_);
}

const uint8_t* ProfileCompilationInfo::SafeBuffer::GetCurrentPtr() {
  return ptr_current_;
}

void ProfileCompilationInfo::SafeBuffer::Advance(size_t data_size) {
  ptr_current_ += data_size;
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::ReadProfileHeader(
      ProfileSource& source,
      /*out*/uint8_t* number_of_dex_files,
      /*out*/uint32_t* uncompressed_data_size,
      /*out*/uint32_t* compressed_data_size,
      /*out*/std::string* error) {
  // Read magic and version
  const size_t kMagicVersionSize =
    sizeof(kProfileMagic) +
    sizeof(kProfileVersion) +
    sizeof(uint8_t) +  // number of dex files
    sizeof(uint32_t) +  // size of uncompressed profile data
    sizeof(uint32_t);  // size of compressed profile data

  SafeBuffer safe_buffer(kMagicVersionSize);

  ProfileLoadStatus status = safe_buffer.Fill(source, "ReadProfileHeader", error);
  if (status != kProfileLoadSuccess) {
    return status;
  }

  if (!safe_buffer.CompareAndAdvance(kProfileMagic, sizeof(kProfileMagic))) {
    *error = "Profile missing magic";
    return kProfileLoadVersionMismatch;
  }
  if (!safe_buffer.CompareAndAdvance(kProfileVersion, sizeof(kProfileVersion))) {
    *error = "Profile version mismatch";
    return kProfileLoadVersionMismatch;
  }
  if (!safe_buffer.ReadUintAndAdvance<uint8_t>(number_of_dex_files)) {
    *error = "Cannot read the number of dex files";
    return kProfileLoadBadData;
  }
  if (!safe_buffer.ReadUintAndAdvance<uint32_t>(uncompressed_data_size)) {
    *error = "Cannot read the size of uncompressed data";
    return kProfileLoadBadData;
  }
  if (!safe_buffer.ReadUintAndAdvance<uint32_t>(compressed_data_size)) {
    *error = "Cannot read the size of compressed data";
    return kProfileLoadBadData;
  }
  return kProfileLoadSuccess;
}

bool ProfileCompilationInfo::ReadProfileLineHeaderElements(SafeBuffer& buffer,
                                                           /*out*/uint16_t* dex_location_size,
                                                           /*out*/ProfileLineHeader* line_header,
                                                           /*out*/std::string* error) {
  READ_UINT(uint16_t, buffer, *dex_location_size, error);
  READ_UINT(uint16_t, buffer, line_header->class_set_size, error);
  READ_UINT(uint32_t, buffer, line_header->method_region_size_bytes, error);
  READ_UINT(uint32_t, buffer, line_header->checksum, error);
  READ_UINT(uint32_t, buffer, line_header->num_method_ids, error);
  return true;
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::ReadProfileLineHeader(
    SafeBuffer& buffer,
    /*out*/ProfileLineHeader* line_header,
    /*out*/std::string* error) {
  if (buffer.CountUnreadBytes() < kLineHeaderSize) {
    *error += "Profile EOF reached prematurely for ReadProfileLineHeader";
    return kProfileLoadBadData;
  }

  uint16_t dex_location_size;
  if (!ReadProfileLineHeaderElements(buffer, &dex_location_size, line_header, error)) {
    return kProfileLoadBadData;
  }

  if (dex_location_size == 0 || dex_location_size > kMaxDexFileKeyLength) {
    *error = "DexFileKey has an invalid size: " +
        std::to_string(static_cast<uint32_t>(dex_location_size));
    return kProfileLoadBadData;
  }

  if (buffer.CountUnreadBytes() < dex_location_size) {
    *error += "Profile EOF reached prematurely for ReadProfileHeaderDexLocation";
    return kProfileLoadBadData;
  }
  const uint8_t* base_ptr = buffer.GetCurrentPtr();
  line_header->dex_location.assign(
      reinterpret_cast<const char*>(base_ptr), dex_location_size);
  buffer.Advance(dex_location_size);
  return kProfileLoadSuccess;
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::ReadProfileLine(
      SafeBuffer& buffer,
      uint8_t number_of_dex_files,
      const ProfileLineHeader& line_header,
      const SafeMap<uint8_t, uint8_t>& dex_profile_index_remap,
      bool merge_classes,
      /*out*/std::string* error) {
  DexFileData* data = GetOrAddDexFileData(line_header.dex_location,
                                          line_header.checksum,
                                          line_header.num_method_ids);
  if (data == nullptr) {
    *error = "Error when reading profile file line header: checksum mismatch for "
        + line_header.dex_location;
    return kProfileLoadBadData;
  }

  if (!ReadMethods(buffer, number_of_dex_files, line_header, dex_profile_index_remap, error)) {
    return kProfileLoadBadData;
  }

  if (merge_classes) {
    if (!ReadClasses(buffer, line_header, error)) {
      return kProfileLoadBadData;
    }
  }

  const size_t bytes = data->bitmap_storage.size();
  if (buffer.CountUnreadBytes() < bytes) {
    *error += "Profile EOF reached prematurely for ReadProfileHeaderDexLocation";
    return kProfileLoadBadData;
  }
  const uint8_t* base_ptr = buffer.GetCurrentPtr();
  std::copy_n(base_ptr, bytes, data->bitmap_storage.data());
  buffer.Advance(bytes);
  // Read method bitmap.
  return kProfileLoadSuccess;
}

// TODO(calin): Fix this API. ProfileCompilationInfo::Load should be static and
// return a unique pointer to a ProfileCompilationInfo upon success.
bool ProfileCompilationInfo::Load(
    int fd, bool merge_classes, const ProfileLoadFilterFn& filter_fn) {
  std::string error;

  ProfileLoadStatus status = LoadInternal(fd, &error, merge_classes, filter_fn);

  if (status == kProfileLoadSuccess) {
    return true;
  } else {
    LOG(WARNING) << "Error when reading profile: " << error;
    return false;
  }
}

bool ProfileCompilationInfo::VerifyProfileData(const std::vector<const DexFile*>& dex_files) {
  std::unordered_map<std::string, const DexFile*> key_to_dex_file;
  for (const DexFile* dex_file : dex_files) {
    key_to_dex_file.emplace(GetProfileDexFileKey(dex_file->GetLocation()), dex_file);
  }
  for (const DexFileData* dex_data : info_) {
    const auto it = key_to_dex_file.find(dex_data->profile_key);
    if (it == key_to_dex_file.end()) {
      // It is okay if profile contains data for additional dex files.
      continue;
    }
    const DexFile* dex_file = it->second;
    const std::string& dex_location = dex_file->GetLocation();
    if (!ChecksumMatch(dex_data->checksum, dex_file->GetLocationChecksum())) {
      LOG(ERROR) << "Dex checksum mismatch while verifying profile "
                 << "dex location " << dex_location << " (checksum="
                 << dex_file->GetLocationChecksum() << ", profile checksum="
                 << dex_data->checksum;
      return false;
    }

    if (dex_data->num_method_ids != dex_file->NumMethodIds()) {
      LOG(ERROR) << "Number of method ids in dex file and profile don't match."
                 << "dex location " << dex_location << " NumMethodId in DexFile"
                 << dex_file->NumMethodIds() << ", NumMethodId in profile"
                 << dex_data->num_method_ids;
      return false;
    }

    // Verify method_encoding.
    for (const auto& method_it : dex_data->method_map) {
      size_t method_id = (size_t)(method_it.first);
      if (method_id >= dex_file->NumMethodIds()) {
        LOG(ERROR) << "Invalid method id in profile file. dex location="
                   << dex_location << " method_id=" << method_id << " NumMethodIds="
                   << dex_file->NumMethodIds();
        return false;
      }

      // Verify class indices of inline caches.
      const InlineCacheMap &inline_cache_map = method_it.second;
      for (const auto& inline_cache_it : inline_cache_map) {
        const DexPcData dex_pc_data = inline_cache_it.second;
        if (dex_pc_data.is_missing_types || dex_pc_data.is_megamorphic) {
          // No class indices to verify.
          continue;
        }

        const ClassSet &classes = dex_pc_data.classes;
        SafeMap<uint8_t, std::vector<dex::TypeIndex>> dex_to_classes_map;
        // Group the classes by dex. We expect that most of the classes will come from
        // the same dex, so this will be more efficient than encoding the dex index
        // for each class reference.
        GroupClassesByDex(classes, &dex_to_classes_map);
        for (const auto &dex_it : dex_to_classes_map) {
          uint8_t dex_profile_index = dex_it.first;
          const auto dex_file_inline_cache_it = key_to_dex_file.find(
              info_[dex_profile_index]->profile_key);
          if (dex_file_inline_cache_it == key_to_dex_file.end()) {
            // It is okay if profile contains data for additional dex files.
            continue;
          }
          const DexFile *dex_file_for_inline_cache_check = dex_file_inline_cache_it->second;
          const std::vector<dex::TypeIndex> &dex_classes = dex_it.second;
          for (size_t i = 0; i < dex_classes.size(); i++) {
            if (dex_classes[i].index_ >= dex_file_for_inline_cache_check->NumTypeIds()) {
              LOG(ERROR) << "Invalid inline cache in profile file. dex location="
                  << dex_location << " method_id=" << method_id
                  << " dex_profile_index="
                  << static_cast<uint16_t >(dex_profile_index) << " type_index="
                  << dex_classes[i].index_
                  << " NumTypeIds="
                  << dex_file_for_inline_cache_check->NumTypeIds();
              return false;
            }
          }
        }
      }
    }
    // Verify class_ids.
    for (const auto& class_id : dex_data->class_set) {
      if (class_id.index_ >= dex_file->NumTypeIds()) {
        LOG(ERROR) << "Invalid class id in profile file. dex_file location "
                   << dex_location << " class_id=" << class_id.index_ << " NumClassIds="
                   << dex_file->NumClassDefs();
        return false;
      }
    }
  }
  return true;
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::OpenSource(
    int32_t fd,
    /*out*/ std::unique_ptr<ProfileSource>* source,
    /*out*/ std::string* error) {
  if (IsProfileFile(fd)) {
    source->reset(ProfileSource::Create(fd));
    return kProfileLoadSuccess;
  } else {
    std::unique_ptr<ZipArchive> zip_archive(ZipArchive::OpenFromFd(fd, "profile", error));
    if (zip_archive.get() == nullptr) {
      *error = "Could not open the profile zip archive";
      return kProfileLoadBadData;
    }
    std::unique_ptr<ZipEntry> zip_entry(zip_archive->Find(kDexMetadataProfileEntry, error));
    if (zip_entry == nullptr) {
      // Allow archives without the profile entry. In this case, create an empty profile.
      // This gives more flexible when ure-using archives that may miss the entry.
      // (e.g. dex metadata files)
      LOG(WARNING) << std::string("Could not find entry ") + kDexMetadataProfileEntry +
            " in the zip archive. Creating an empty profile.";
      source->reset(ProfileSource::Create(nullptr));
      return kProfileLoadSuccess;
    }
    if (zip_entry->GetUncompressedLength() == 0) {
      *error = "Empty profile entry in the zip archive.";
      return kProfileLoadBadData;
    }

    // TODO(calin) pass along file names to assist with debugging.
    std::unique_ptr<MemMap> map(zip_entry->MapDirectlyOrExtract(kDexMetadataProfileEntry,
                                                                "profile file",
                                                                error));

    if (map != nullptr) {
      source->reset(ProfileSource::Create(std::move(map)));
      return kProfileLoadSuccess;
    } else {
      return kProfileLoadBadData;
    }
  }
}

ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::ProfileSource::Read(
    uint8_t* buffer,
    size_t byte_count,
    const std::string& debug_stage,
    std::string* error) {
  if (IsMemMap()) {
    if (mem_map_cur_ + byte_count > mem_map_->Size()) {
      return kProfileLoadBadData;
    }
    for (size_t i = 0; i < byte_count; i++) {
      buffer[i] = *(mem_map_->Begin() + mem_map_cur_);
      mem_map_cur_++;
    }
  } else {
    while (byte_count > 0) {
      int bytes_read = TEMP_FAILURE_RETRY(read(fd_, buffer, byte_count));;
      if (bytes_read == 0) {
        *error += "Profile EOF reached prematurely for " + debug_stage;
        return kProfileLoadBadData;
      } else if (bytes_read < 0) {
        *error += "Profile IO error for " + debug_stage + strerror(errno);
        return kProfileLoadIOError;
      }
      byte_count -= bytes_read;
      buffer += bytes_read;
    }
  }
  return kProfileLoadSuccess;
}

bool ProfileCompilationInfo::ProfileSource::HasConsumedAllData() const {
  return IsMemMap()
      ? (mem_map_ == nullptr || mem_map_cur_ == mem_map_->Size())
      : (testEOF(fd_) == 0);
}

bool ProfileCompilationInfo::ProfileSource::HasEmptyContent() const {
  if (IsMemMap()) {
    return mem_map_ == nullptr || mem_map_->Size() == 0;
  } else {
    struct stat stat_buffer;
    if (fstat(fd_, &stat_buffer) != 0) {
      return false;
    }
    return stat_buffer.st_size == 0;
  }
}

// TODO(calin): fail fast if the dex checksums don't match.
ProfileCompilationInfo::ProfileLoadStatus ProfileCompilationInfo::LoadInternal(
      int32_t fd,
      std::string* error,
      bool merge_classes,
      const ProfileLoadFilterFn& filter_fn) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  DCHECK_GE(fd, 0);

  std::unique_ptr<ProfileSource> source;
  ProfileLoadStatus status = OpenSource(fd, &source, error);
  if (status != kProfileLoadSuccess) {
    return status;
  }

  // We allow empty profile files.
  // Profiles may be created by ActivityManager or installd before we manage to
  // process them in the runtime or profman.
  if (source->HasEmptyContent()) {
    return kProfileLoadSuccess;
  }

  // Read profile header: magic + version + number_of_dex_files.
  uint8_t number_of_dex_files;
  uint32_t uncompressed_data_size;
  uint32_t compressed_data_size;
  status = ReadProfileHeader(*source,
                             &number_of_dex_files,
                             &uncompressed_data_size,
                             &compressed_data_size,
                             error);

  if (status != kProfileLoadSuccess) {
    return status;
  }
  // Allow large profiles for non target builds for the case where we are merging many profiles
  // to generate a boot image profile.
  if (kIsTargetBuild && uncompressed_data_size > kProfileSizeErrorThresholdInBytes) {
    LOG(ERROR) << "Profile data size exceeds "
               << std::to_string(kProfileSizeErrorThresholdInBytes)
               << " bytes";
    return kProfileLoadBadData;
  }
  if (uncompressed_data_size > kProfileSizeWarningThresholdInBytes) {
    LOG(WARNING) << "Profile data size exceeds "
                 << std::to_string(kProfileSizeWarningThresholdInBytes)
                 << " bytes";
  }

  std::unique_ptr<uint8_t[]> compressed_data(new uint8_t[compressed_data_size]);
  status = source->Read(compressed_data.get(), compressed_data_size, "ReadContent", error);
  if (status != kProfileLoadSuccess) {
    *error += "Unable to read compressed profile data";
    return status;
  }

  if (!source->HasConsumedAllData()) {
    *error += "Unexpected data in the profile file.";
    return kProfileLoadBadData;
  }

  SafeBuffer uncompressed_data(uncompressed_data_size);

  int ret = InflateBuffer(compressed_data.get(),
                          compressed_data_size,
                          uncompressed_data_size,
                          uncompressed_data.Get());

  if (ret != Z_STREAM_END) {
    *error += "Error reading uncompressed profile data";
    return kProfileLoadBadData;
  }

  std::vector<ProfileLineHeader> profile_line_headers;
  // Read profile line headers.
  for (uint8_t k = 0; k < number_of_dex_files; k++) {
    ProfileLineHeader line_header;

    // First, read the line header to get the amount of data we need to read.
    status = ReadProfileLineHeader(uncompressed_data, &line_header, error);
    if (status != kProfileLoadSuccess) {
      return status;
    }
    profile_line_headers.push_back(line_header);
  }

  SafeMap<uint8_t, uint8_t> dex_profile_index_remap;
  if (!RemapProfileIndex(profile_line_headers, filter_fn, &dex_profile_index_remap)) {
    return kProfileLoadBadData;
  }

  for (uint8_t k = 0; k < number_of_dex_files; k++) {
    if (!filter_fn(profile_line_headers[k].dex_location, profile_line_headers[k].checksum)) {
      // We have to skip the line. Advanced the current pointer of the buffer.
      size_t profile_line_size =
           profile_line_headers[k].class_set_size * sizeof(uint16_t) +
           profile_line_headers[k].method_region_size_bytes +
           DexFileData::ComputeBitmapStorage(profile_line_headers[k].num_method_ids);
      uncompressed_data.Advance(profile_line_size);
    } else {
      // Now read the actual profile line.
      status = ReadProfileLine(uncompressed_data,
                               number_of_dex_files,
                               profile_line_headers[k],
                               dex_profile_index_remap,
                               merge_classes,
                               error);
      if (status != kProfileLoadSuccess) {
        return status;
      }
    }
  }

  // Check that we read everything and that profiles don't contain junk data.
  if (uncompressed_data.CountUnreadBytes() > 0) {
    *error = "Unexpected content in the profile file";
    return kProfileLoadBadData;
  } else {
    return kProfileLoadSuccess;
  }
}

bool ProfileCompilationInfo::RemapProfileIndex(
    const std::vector<ProfileLineHeader>& profile_line_headers,
    const ProfileLoadFilterFn& filter_fn,
    /*out*/SafeMap<uint8_t, uint8_t>* dex_profile_index_remap) {
  // First verify that all checksums match. This will avoid adding garbage to
  // the current profile info.
  // Note that the number of elements should be very small, so this should not
  // be a performance issue.
  for (const ProfileLineHeader other_profile_line_header : profile_line_headers) {
    if (!filter_fn(other_profile_line_header.dex_location, other_profile_line_header.checksum)) {
      continue;
    }
    // verify_checksum is false because we want to differentiate between a missing dex data and
    // a mismatched checksum.
    const DexFileData* dex_data = FindDexData(other_profile_line_header.dex_location,
                                              0u,
                                              false /* verify_checksum */);
    if ((dex_data != nullptr) && (dex_data->checksum != other_profile_line_header.checksum)) {
      LOG(WARNING) << "Checksum mismatch for dex " << other_profile_line_header.dex_location;
      return false;
    }
  }
  // All checksums match. Import the data.
  uint32_t num_dex_files = static_cast<uint32_t>(profile_line_headers.size());
  for (uint32_t i = 0; i < num_dex_files; i++) {
    if (!filter_fn(profile_line_headers[i].dex_location, profile_line_headers[i].checksum)) {
      continue;
    }
    const DexFileData* dex_data = GetOrAddDexFileData(profile_line_headers[i].dex_location,
                                                      profile_line_headers[i].checksum,
                                                      profile_line_headers[i].num_method_ids);
    if (dex_data == nullptr) {
      return false;  // Could happen if we exceed the number of allowed dex files.
    }
    dex_profile_index_remap->Put(i, dex_data->profile_index);
  }
  return true;
}

std::unique_ptr<uint8_t[]> ProfileCompilationInfo::DeflateBuffer(const uint8_t* in_buffer,
                                                                 uint32_t in_size,
                                                                 uint32_t* compressed_data_size) {
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  int ret = deflateInit(&strm, 1);
  if (ret != Z_OK) {
    return nullptr;
  }

  uint32_t out_size = deflateBound(&strm, in_size);

  std::unique_ptr<uint8_t[]> compressed_buffer(new uint8_t[out_size]);
  strm.avail_in = in_size;
  strm.next_in = const_cast<uint8_t*>(in_buffer);
  strm.avail_out = out_size;
  strm.next_out = &compressed_buffer[0];
  ret = deflate(&strm, Z_FINISH);
  if (ret == Z_STREAM_ERROR) {
    return nullptr;
  }
  *compressed_data_size = out_size - strm.avail_out;
  deflateEnd(&strm);
  return compressed_buffer;
}

int ProfileCompilationInfo::InflateBuffer(const uint8_t* in_buffer,
                                          uint32_t in_size,
                                          uint32_t expected_uncompressed_data_size,
                                          uint8_t* out_buffer) {
  z_stream strm;

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = in_size;
  strm.next_in = const_cast<uint8_t*>(in_buffer);
  strm.avail_out = expected_uncompressed_data_size;
  strm.next_out = out_buffer;

  int ret;
  inflateInit(&strm);
  ret = inflate(&strm, Z_NO_FLUSH);

  if (strm.avail_in != 0 || strm.avail_out != 0) {
    return Z_DATA_ERROR;
  }
  inflateEnd(&strm);
  return ret;
}

bool ProfileCompilationInfo::MergeWith(const ProfileCompilationInfo& other,
                                       bool merge_classes) {
  // First verify that all checksums match. This will avoid adding garbage to
  // the current profile info.
  // Note that the number of elements should be very small, so this should not
  // be a performance issue.
  for (const DexFileData* other_dex_data : other.info_) {
    // verify_checksum is false because we want to differentiate between a missing dex data and
    // a mismatched checksum.
    const DexFileData* dex_data = FindDexData(other_dex_data->profile_key,
                                              0u,
                                              /* verify_checksum */ false);
    if ((dex_data != nullptr) && (dex_data->checksum != other_dex_data->checksum)) {
      LOG(WARNING) << "Checksum mismatch for dex " << other_dex_data->profile_key;
      return false;
    }
  }
  // All checksums match. Import the data.

  // The other profile might have a different indexing of dex files.
  // That is because each dex files gets a 'dex_profile_index' on a first come first served basis.
  // That means that the order in with the methods are added to the profile matters for the
  // actual indices.
  // The reason we cannot rely on the actual multidex index is that a single profile may store
  // data from multiple splits. This means that a profile may contain a classes2.dex from split-A
  // and one from split-B.

  // First, build a mapping from other_dex_profile_index to this_dex_profile_index.
  // This will make sure that the ClassReferences  will point to the correct dex file.
  SafeMap<uint8_t, uint8_t> dex_profile_index_remap;
  for (const DexFileData* other_dex_data : other.info_) {
    const DexFileData* dex_data = GetOrAddDexFileData(other_dex_data->profile_key,
                                                      other_dex_data->checksum,
                                                      other_dex_data->num_method_ids);
    if (dex_data == nullptr) {
      return false;  // Could happen if we exceed the number of allowed dex files.
    }
    dex_profile_index_remap.Put(other_dex_data->profile_index, dex_data->profile_index);
  }

  // Merge the actual profile data.
  for (const DexFileData* other_dex_data : other.info_) {
    DexFileData* dex_data = const_cast<DexFileData*>(FindDexData(other_dex_data->profile_key,
                                                                 other_dex_data->checksum));
    DCHECK(dex_data != nullptr);

    // Merge the classes.
    if (merge_classes) {
      dex_data->class_set.insert(other_dex_data->class_set.begin(),
                                 other_dex_data->class_set.end());
    }

    // Merge the methods and the inline caches.
    for (const auto& other_method_it : other_dex_data->method_map) {
      uint16_t other_method_index = other_method_it.first;
      InlineCacheMap* inline_cache = dex_data->FindOrAddMethod(other_method_index);
      if (inline_cache == nullptr) {
        return false;
      }
      const auto& other_inline_cache = other_method_it.second;
      for (const auto& other_ic_it : other_inline_cache) {
        uint16_t other_dex_pc = other_ic_it.first;
        const ClassSet& other_class_set = other_ic_it.second.classes;
        DexPcData* dex_pc_data = FindOrAddDexPc(inline_cache, other_dex_pc);
        if (other_ic_it.second.is_missing_types) {
          dex_pc_data->SetIsMissingTypes();
        } else if (other_ic_it.second.is_megamorphic) {
          dex_pc_data->SetIsMegamorphic();
        } else {
          for (const auto& class_it : other_class_set) {
            dex_pc_data->AddClass(dex_profile_index_remap.Get(
                class_it.dex_profile_index), class_it.type_index);
          }
        }
      }
    }

    // Merge the method bitmaps.
    dex_data->MergeBitmap(*other_dex_data);
  }
  return true;
}

const ProfileCompilationInfo::DexFileData* ProfileCompilationInfo::FindDexData(
    const DexFile* dex_file) const {
  return FindDexData(GetProfileDexFileKey(dex_file->GetLocation()),
                     dex_file->GetLocationChecksum());
}

ProfileCompilationInfo::MethodHotness ProfileCompilationInfo::GetMethodHotness(
    const MethodReference& method_ref) const {
  const DexFileData* dex_data = FindDexData(method_ref.dex_file);
  return dex_data != nullptr
      ? dex_data->GetHotnessInfo(method_ref.index)
      : MethodHotness();
}

bool ProfileCompilationInfo::AddMethodHotness(const MethodReference& method_ref,
                                              const MethodHotness& hotness) {
  DexFileData* dex_data = GetOrAddDexFileData(method_ref.dex_file);
  if (dex_data != nullptr) {
    // TODO: Add inline caches.
    return dex_data->AddMethod(
        static_cast<MethodHotness::Flag>(hotness.GetFlags()), method_ref.index);
  }
  return false;
}

ProfileCompilationInfo::MethodHotness ProfileCompilationInfo::GetMethodHotness(
    const std::string& dex_location,
    uint32_t dex_checksum,
    uint16_t dex_method_index) const {
  const DexFileData* dex_data = FindDexData(GetProfileDexFileKey(dex_location), dex_checksum);
  return dex_data != nullptr ? dex_data->GetHotnessInfo(dex_method_index) : MethodHotness();
}


std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> ProfileCompilationInfo::GetMethod(
    const std::string& dex_location,
    uint32_t dex_checksum,
    uint16_t dex_method_index) const {
  MethodHotness hotness(GetMethodHotness(dex_location, dex_checksum, dex_method_index));
  if (!hotness.IsHot()) {
    return nullptr;
  }
  const InlineCacheMap* inline_caches = hotness.GetInlineCacheMap();
  DCHECK(inline_caches != nullptr);
  std::unique_ptr<OfflineProfileMethodInfo> pmi(new OfflineProfileMethodInfo(inline_caches));

  pmi->dex_references.resize(info_.size());
  for (const DexFileData* dex_data : info_) {
    pmi->dex_references[dex_data->profile_index].dex_location = dex_data->profile_key;
    pmi->dex_references[dex_data->profile_index].dex_checksum = dex_data->checksum;
    pmi->dex_references[dex_data->profile_index].num_method_ids = dex_data->num_method_ids;
  }

  return pmi;
}


bool ProfileCompilationInfo::ContainsClass(const DexFile& dex_file, dex::TypeIndex type_idx) const {
  const DexFileData* dex_data = FindDexData(&dex_file);
  if (dex_data != nullptr) {
    const ArenaSet<dex::TypeIndex>& classes = dex_data->class_set;
    return classes.find(type_idx) != classes.end();
  }
  return false;
}

uint32_t ProfileCompilationInfo::GetNumberOfMethods() const {
  uint32_t total = 0;
  for (const DexFileData* dex_data : info_) {
    total += dex_data->method_map.size();
  }
  return total;
}

uint32_t ProfileCompilationInfo::GetNumberOfResolvedClasses() const {
  uint32_t total = 0;
  for (const DexFileData* dex_data : info_) {
    total += dex_data->class_set.size();
  }
  return total;
}

// Produce a non-owning vector from a vector.
template<typename T>
const std::vector<T*>* MakeNonOwningVector(const std::vector<std::unique_ptr<T>>* owning_vector) {
  auto non_owning_vector = new std::vector<T*>();
  for (auto& element : *owning_vector) {
    non_owning_vector->push_back(element.get());
  }
  return non_owning_vector;
}

std::string ProfileCompilationInfo::DumpInfo(
    const std::vector<std::unique_ptr<const DexFile>>* dex_files,
    bool print_full_dex_location) const {
  std::unique_ptr<const std::vector<const DexFile*>> non_owning_dex_files(
      MakeNonOwningVector(dex_files));
  return DumpInfo(non_owning_dex_files.get(), print_full_dex_location);
}

std::string ProfileCompilationInfo::DumpInfo(const std::vector<const DexFile*>* dex_files,
                                             bool print_full_dex_location) const {
  std::ostringstream os;
  if (info_.empty()) {
    return "ProfileInfo: empty";
  }

  os << "ProfileInfo:";

  const std::string kFirstDexFileKeySubstitute = "!classes.dex";

  for (const DexFileData* dex_data : info_) {
    os << "\n";
    if (print_full_dex_location) {
      os << dex_data->profile_key;
    } else {
      // Replace the (empty) multidex suffix of the first key with a substitute for easier reading.
      std::string multidex_suffix = DexFileLoader::GetMultiDexSuffix(dex_data->profile_key);
      os << (multidex_suffix.empty() ? kFirstDexFileKeySubstitute : multidex_suffix);
    }
    os << " [index=" << static_cast<uint32_t>(dex_data->profile_index) << "]";
    const DexFile* dex_file = nullptr;
    if (dex_files != nullptr) {
      for (size_t i = 0; i < dex_files->size(); i++) {
        if (dex_data->profile_key == (*dex_files)[i]->GetLocation()) {
          dex_file = (*dex_files)[i];
        }
      }
    }
    os << "\n\thot methods: ";
    for (const auto& method_it : dex_data->method_map) {
      if (dex_file != nullptr) {
        os << "\n\t\t" << dex_file->PrettyMethod(method_it.first, true);
      } else {
        os << method_it.first;
      }

      os << "[";
      for (const auto& inline_cache_it : method_it.second) {
        os << "{" << std::hex << inline_cache_it.first << std::dec << ":";
        if (inline_cache_it.second.is_missing_types) {
          os << "MT";
        } else if (inline_cache_it.second.is_megamorphic) {
          os << "MM";
        } else {
          for (const ClassReference& class_ref : inline_cache_it.second.classes) {
            os << "(" << static_cast<uint32_t>(class_ref.dex_profile_index)
               << "," << class_ref.type_index.index_ << ")";
          }
        }
        os << "}";
      }
      os << "], ";
    }
    bool startup = true;
    while (true) {
      os << "\n\t" << (startup ? "startup methods: " : "post startup methods: ");
      for (uint32_t method_idx = 0; method_idx < dex_data->num_method_ids; ++method_idx) {
        MethodHotness hotness_info(dex_data->GetHotnessInfo(method_idx));
        if (startup ? hotness_info.IsStartup() : hotness_info.IsPostStartup()) {
          if (dex_file != nullptr) {
            os << "\n\t\t" << dex_file->PrettyMethod(method_idx, true);
          } else {
            os << method_idx << ", ";
          }
        }
      }
      if (startup == false) {
        break;
      }
      startup = false;
    }
    os << "\n\tclasses: ";
    for (const auto class_it : dex_data->class_set) {
      if (dex_file != nullptr) {
        os << "\n\t\t" << dex_file->PrettyType(class_it);
      } else {
        os << class_it.index_ << ",";
      }
    }
  }
  return os.str();
}

bool ProfileCompilationInfo::GetClassesAndMethods(
    const DexFile& dex_file,
    /*out*/std::set<dex::TypeIndex>* class_set,
    /*out*/std::set<uint16_t>* hot_method_set,
    /*out*/std::set<uint16_t>* startup_method_set,
    /*out*/std::set<uint16_t>* post_startup_method_method_set) const {
  std::set<std::string> ret;
  const DexFileData* dex_data = FindDexData(&dex_file);
  if (dex_data == nullptr) {
    return false;
  }
  for (const auto& it : dex_data->method_map) {
    hot_method_set->insert(it.first);
  }
  for (uint32_t method_idx = 0; method_idx < dex_data->num_method_ids; ++method_idx) {
    MethodHotness hotness = dex_data->GetHotnessInfo(method_idx);
    if (hotness.IsStartup()) {
      startup_method_set->insert(method_idx);
    }
    if (hotness.IsPostStartup()) {
      post_startup_method_method_set->insert(method_idx);
    }
  }
  for (const dex::TypeIndex& type_index : dex_data->class_set) {
    class_set->insert(type_index);
  }
  return true;
}

bool ProfileCompilationInfo::Equals(const ProfileCompilationInfo& other) {
  // No need to compare profile_key_map_. That's only a cache for fast search.
  // All the information is already in the info_ vector.
  if (info_.size() != other.info_.size()) {
    return false;
  }
  for (size_t i = 0; i < info_.size(); i++) {
    const DexFileData& dex_data = *info_[i];
    const DexFileData& other_dex_data = *other.info_[i];
    if (!(dex_data == other_dex_data)) {
      return false;
    }
  }
  return true;
}

std::set<DexCacheResolvedClasses> ProfileCompilationInfo::GetResolvedClasses(
    const std::vector<const DexFile*>& dex_files) const {
  std::unordered_map<std::string, const DexFile* > key_to_dex_file;
  for (const DexFile* dex_file : dex_files) {
    key_to_dex_file.emplace(GetProfileDexFileKey(dex_file->GetLocation()), dex_file);
  }
  std::set<DexCacheResolvedClasses> ret;
  for (const DexFileData* dex_data : info_) {
    const auto it = key_to_dex_file.find(dex_data->profile_key);
    if (it != key_to_dex_file.end()) {
      const DexFile* dex_file = it->second;
      const std::string& dex_location = dex_file->GetLocation();
      if (dex_data->checksum != it->second->GetLocationChecksum()) {
        LOG(ERROR) << "Dex checksum mismatch when getting resolved classes from profile for "
            << "location " << dex_location << " (checksum=" << dex_file->GetLocationChecksum()
            << ", profile checksum=" << dex_data->checksum;
        return std::set<DexCacheResolvedClasses>();
      }
      DexCacheResolvedClasses classes(dex_location,
                                      dex_location,
                                      dex_data->checksum,
                                      dex_data->num_method_ids);
      classes.AddClasses(dex_data->class_set.begin(), dex_data->class_set.end());
      ret.insert(classes);
    }
  }
  return ret;
}

// Naive implementation to generate a random profile file suitable for testing.
bool ProfileCompilationInfo::GenerateTestProfile(int fd,
                                                 uint16_t number_of_dex_files,
                                                 uint16_t method_percentage,
                                                 uint16_t class_percentage,
                                                 uint32_t random_seed) {
  const std::string base_dex_location = "base.apk";
  ProfileCompilationInfo info;
  // The limits are defined by the dex specification.
  const uint16_t max_method = std::numeric_limits<uint16_t>::max();
  const uint16_t max_classes = std::numeric_limits<uint16_t>::max();
  uint16_t number_of_methods = max_method * method_percentage / 100;
  uint16_t number_of_classes = max_classes * class_percentage / 100;

  std::srand(random_seed);

  // Make sure we generate more samples with a low index value.
  // This makes it more likely to hit valid method/class indices in small apps.
  const uint16_t kFavorFirstN = 10000;
  const uint16_t kFavorSplit = 2;

  for (uint16_t i = 0; i < number_of_dex_files; i++) {
    std::string dex_location = DexFileLoader::GetMultiDexLocation(i, base_dex_location.c_str());
    std::string profile_key = GetProfileDexFileKey(dex_location);

    for (uint16_t m = 0; m < number_of_methods; m++) {
      uint16_t method_idx = rand() % max_method;
      if (m < (number_of_methods / kFavorSplit)) {
        method_idx %= kFavorFirstN;
      }
      // Alternate between startup and post startup.
      uint32_t flags = MethodHotness::kFlagHot;
      flags |= ((m & 1) != 0) ? MethodHotness::kFlagPostStartup : MethodHotness::kFlagStartup;
      info.AddMethodIndex(static_cast<MethodHotness::Flag>(flags),
                          profile_key,
                          /*method_idx*/ 0,
                          method_idx,
                          max_method);
    }

    for (uint16_t c = 0; c < number_of_classes; c++) {
      uint16_t type_idx = rand() % max_classes;
      if (c < (number_of_classes / kFavorSplit)) {
        type_idx %= kFavorFirstN;
      }
      info.AddClassIndex(profile_key, 0, dex::TypeIndex(type_idx), max_method);
    }
  }
  return info.Save(fd);
}

// Naive implementation to generate a random profile file suitable for testing.
// Description of random selection:
// * Select a random starting point S.
// * For every index i, add (S+i) % (N - total number of methods/classes) to profile with the
//   probably of 1/(N - i - number of methods/classes needed to add in profile).
bool ProfileCompilationInfo::GenerateTestProfile(
    int fd,
    std::vector<std::unique_ptr<const DexFile>>& dex_files,
    uint16_t method_percentage,
    uint16_t class_percentage,
    uint32_t random_seed) {
  std::srand(random_seed);
  ProfileCompilationInfo info;
  for (std::unique_ptr<const DexFile>& dex_file : dex_files) {
    const std::string& location = dex_file->GetLocation();
    uint32_t checksum = dex_file->GetLocationChecksum();

    uint32_t number_of_classes = dex_file->NumClassDefs();
    uint32_t classes_required_in_profile = (number_of_classes * class_percentage) / 100;
    uint32_t class_start_index = rand() % number_of_classes;
    for (uint32_t i = 0; i < number_of_classes && classes_required_in_profile; ++i) {
      if (number_of_classes - i == classes_required_in_profile ||
          std::rand() % (number_of_classes - i - classes_required_in_profile) == 0) {
        uint32_t class_index = (i + class_start_index) % number_of_classes;
        info.AddClassIndex(location,
                           checksum,
                           dex_file->GetClassDef(class_index).class_idx_,
                           dex_file->NumMethodIds());
        classes_required_in_profile--;
      }
    }

    uint32_t number_of_methods = dex_file->NumMethodIds();
    uint32_t methods_required_in_profile = (number_of_methods * method_percentage) / 100;
    uint32_t method_start_index = rand() % number_of_methods;
    for (uint32_t i = 0; i < number_of_methods && methods_required_in_profile; ++i) {
      if (number_of_methods - i == methods_required_in_profile ||
          std::rand() % (number_of_methods - i - methods_required_in_profile) == 0) {
        uint32_t method_index = (method_start_index + i) % number_of_methods;
        // Alternate between startup and post startup.
        uint32_t flags = MethodHotness::kFlagHot;
        flags |= ((method_index & 1) != 0)
            ? MethodHotness::kFlagPostStartup
            : MethodHotness::kFlagStartup;
        info.AddMethodIndex(static_cast<MethodHotness::Flag>(flags),
                            MethodReference(dex_file.get(), method_index));
        methods_required_in_profile--;
      }
    }
  }
  return info.Save(fd);
}

bool ProfileCompilationInfo::OfflineProfileMethodInfo::operator==(
      const OfflineProfileMethodInfo& other) const {
  if (inline_caches->size() != other.inline_caches->size()) {
    return false;
  }

  // We can't use a simple equality test because we need to match the dex files
  // of the inline caches which might have different profile indexes.
  for (const auto& inline_cache_it : *inline_caches) {
    uint16_t dex_pc = inline_cache_it.first;
    const DexPcData dex_pc_data = inline_cache_it.second;
    const auto& other_it = other.inline_caches->find(dex_pc);
    if (other_it == other.inline_caches->end()) {
      return false;
    }
    const DexPcData& other_dex_pc_data = other_it->second;
    if (dex_pc_data.is_megamorphic != other_dex_pc_data.is_megamorphic ||
        dex_pc_data.is_missing_types != other_dex_pc_data.is_missing_types) {
      return false;
    }
    for (const ClassReference& class_ref : dex_pc_data.classes) {
      bool found = false;
      for (const ClassReference& other_class_ref : other_dex_pc_data.classes) {
        CHECK_LE(class_ref.dex_profile_index, dex_references.size());
        CHECK_LE(other_class_ref.dex_profile_index, other.dex_references.size());
        const DexReference& dex_ref = dex_references[class_ref.dex_profile_index];
        const DexReference& other_dex_ref = other.dex_references[other_class_ref.dex_profile_index];
        if (class_ref.type_index == other_class_ref.type_index &&
            dex_ref == other_dex_ref) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
  }
  return true;
}

bool ProfileCompilationInfo::IsEmpty() const {
  DCHECK_EQ(info_.empty(), profile_key_map_.empty());
  return info_.empty();
}

ProfileCompilationInfo::InlineCacheMap*
ProfileCompilationInfo::DexFileData::FindOrAddMethod(uint16_t method_index) {
  if (method_index >= num_method_ids) {
    LOG(ERROR) << "Invalid method index " << method_index << ". num_method_ids=" << num_method_ids;
    return nullptr;
  }
  return &(method_map.FindOrAdd(
      method_index,
      InlineCacheMap(std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)))->second);
}

// Mark a method as executed at least once.
bool ProfileCompilationInfo::DexFileData::AddMethod(MethodHotness::Flag flags, size_t index) {
  if (index >= num_method_ids) {
    LOG(ERROR) << "Invalid method index " << index << ". num_method_ids=" << num_method_ids;
    return false;
  }

  SetMethodHotness(index, flags);

  if ((flags & MethodHotness::kFlagHot) != 0) {
    method_map.FindOrAdd(
        index,
        InlineCacheMap(std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)));
  }
  return true;
}

void ProfileCompilationInfo::DexFileData::SetMethodHotness(size_t index,
                                                           MethodHotness::Flag flags) {
  DCHECK_LT(index, num_method_ids);
  if ((flags & MethodHotness::kFlagStartup) != 0) {
    method_bitmap.StoreBit(MethodBitIndex(/*startup*/ true, index), /*value*/ true);
  }
  if ((flags & MethodHotness::kFlagPostStartup) != 0) {
    method_bitmap.StoreBit(MethodBitIndex(/*startup*/ false, index), /*value*/ true);
  }
}

ProfileCompilationInfo::MethodHotness ProfileCompilationInfo::DexFileData::GetHotnessInfo(
    uint32_t dex_method_index) const {
  MethodHotness ret;
  if (method_bitmap.LoadBit(MethodBitIndex(/*startup*/ true, dex_method_index))) {
    ret.AddFlag(MethodHotness::kFlagStartup);
  }
  if (method_bitmap.LoadBit(MethodBitIndex(/*startup*/ false, dex_method_index))) {
    ret.AddFlag(MethodHotness::kFlagPostStartup);
  }
  auto it = method_map.find(dex_method_index);
  if (it != method_map.end()) {
    ret.SetInlineCacheMap(&it->second);
    ret.AddFlag(MethodHotness::kFlagHot);
  }
  return ret;
}

ProfileCompilationInfo::DexPcData*
ProfileCompilationInfo::FindOrAddDexPc(InlineCacheMap* inline_cache, uint32_t dex_pc) {
  return &(inline_cache->FindOrAdd(dex_pc, DexPcData(&allocator_))->second);
}

std::unordered_set<std::string> ProfileCompilationInfo::GetClassDescriptors(
    const std::vector<const DexFile*>& dex_files) {
  std::unordered_set<std::string> ret;
  for (const DexFile* dex_file : dex_files) {
    const DexFileData* data = FindDexData(dex_file);
    if (data != nullptr) {
      for (dex::TypeIndex type_idx : data->class_set) {
        if (!dex_file->IsTypeIndexValid(type_idx)) {
          // Something went bad. The profile is probably corrupted. Abort and return an emtpy set.
          LOG(WARNING) << "Corrupted profile: invalid type index "
              << type_idx.index_ << " in dex " << dex_file->GetLocation();
          return std::unordered_set<std::string>();
        }
        const DexFile::TypeId& type_id = dex_file->GetTypeId(type_idx);
        ret.insert(dex_file->GetTypeDescriptor(type_id));
      }
    } else {
      VLOG(compiler) << "Failed to find profile data for " << dex_file->GetLocation();
    }
  }
  return ret;
}

bool ProfileCompilationInfo::IsProfileFile(int fd) {
  // First check if it's an empty file as we allow empty profile files.
  // Profiles may be created by ActivityManager or installd before we manage to
  // process them in the runtime or profman.
  struct stat stat_buffer;
  if (fstat(fd, &stat_buffer) != 0) {
    return false;
  }

  if (stat_buffer.st_size == 0) {
    return true;
  }

  // The files is not empty. Check if it contains the profile magic.
  size_t byte_count = sizeof(kProfileMagic);
  uint8_t buffer[sizeof(kProfileMagic)];
  if (!android::base::ReadFully(fd, buffer, byte_count)) {
    return false;
  }

  // Reset the offset to prepare the file for reading.
  off_t rc =  TEMP_FAILURE_RETRY(lseek(fd, 0, SEEK_SET));
  if (rc == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to reset the offset";
    return false;
  }

  return memcmp(buffer, kProfileMagic, byte_count) == 0;
}

bool ProfileCompilationInfo::UpdateProfileKeys(
      const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    for (DexFileData* dex_data : info_) {
      if (dex_data->checksum == dex_file->GetLocationChecksum()
          && dex_data->num_method_ids == dex_file->NumMethodIds()) {
        std::string new_profile_key = GetProfileDexFileKey(dex_file->GetLocation());
        if (dex_data->profile_key != new_profile_key) {
          if (profile_key_map_.find(new_profile_key) != profile_key_map_.end()) {
            // We can't update the key if the new key belongs to a different dex file.
            LOG(ERROR) << "Cannot update profile key to " << new_profile_key
                << " because the new key belongs to another dex file.";
            return false;
          }
          profile_key_map_.erase(dex_data->profile_key);
          profile_key_map_.Put(new_profile_key, dex_data->profile_index);
          dex_data->profile_key = new_profile_key;
        }
      }
    }
  }
  return true;
}

bool ProfileCompilationInfo::ProfileFilterFnAcceptAll(
    const std::string& dex_location ATTRIBUTE_UNUSED,
    uint32_t checksum ATTRIBUTE_UNUSED) {
  return true;
}

void ProfileCompilationInfo::ClearData() {
  for (DexFileData* data : info_) {
    delete data;
  }
  info_.clear();
  profile_key_map_.clear();
}

}  // namespace art
