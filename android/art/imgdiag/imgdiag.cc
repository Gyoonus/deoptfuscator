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

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "android-base/stringprintf.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "image.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "oat.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"

#include "backtrace/BacktraceMap.h"
#include "cmdline.h"

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace art {

using android::base::StringPrintf;

namespace {

constexpr size_t kMaxAddressPrint = 5;

enum class ProcessType {
  kZygote,
  kRemote
};

enum class RemoteProcesses {
  kImageOnly,
  kZygoteOnly,
  kImageAndZygote
};

struct MappingData {
  // The count of pages that are considered dirty by the OS.
  size_t dirty_pages = 0;
  // The count of pages that differ by at least one byte.
  size_t different_pages = 0;
  // The count of differing bytes.
  size_t different_bytes = 0;
  // The count of differing four-byte units.
  size_t different_int32s = 0;
  // The count of pages that have mapping count == 1.
  size_t private_pages = 0;
  // The count of private pages that are also dirty.
  size_t private_dirty_pages = 0;
  // The count of pages that are marked dirty but do not differ.
  size_t false_dirty_pages = 0;
  // Set of the local virtual page indices that are dirty.
  std::set<size_t> dirty_page_set;
};

static std::string GetClassDescriptor(mirror::Class* klass)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  CHECK(klass != nullptr);

  std::string descriptor;
  const char* descriptor_str = klass->GetDescriptor(&descriptor /*out*/);

  return std::string(descriptor_str);
}

static std::string PrettyFieldValue(ArtField* field, mirror::Object* object)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  std::ostringstream oss;
  switch (field->GetTypeAsPrimitiveType()) {
    case Primitive::kPrimNot: {
      oss << object->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(
          field->GetOffset());
      break;
    }
    case Primitive::kPrimBoolean: {
      oss << static_cast<bool>(object->GetFieldBoolean<kVerifyNone>(field->GetOffset()));
      break;
    }
    case Primitive::kPrimByte: {
      oss << static_cast<int32_t>(object->GetFieldByte<kVerifyNone>(field->GetOffset()));
      break;
    }
    case Primitive::kPrimChar: {
      oss << object->GetFieldChar<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimShort: {
      oss << object->GetFieldShort<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimInt: {
      oss << object->GetField32<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimLong: {
      oss << object->GetField64<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimFloat: {
      oss << object->GetField32<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimDouble: {
      oss << object->GetField64<kVerifyNone>(field->GetOffset());
      break;
    }
    case Primitive::kPrimVoid: {
      oss << "void";
      break;
    }
  }
  return oss.str();
}

template <typename K, typename V, typename D>
static std::vector<std::pair<V, K>> SortByValueDesc(
    const std::map<K, D> map,
    std::function<V(const D&)> value_mapper = [](const D& d) { return static_cast<V>(d); }) {
  // Store value->key so that we can use the default sort from pair which
  // sorts by value first and then key
  std::vector<std::pair<V, K>> value_key_vector;

  for (const auto& kv_pair : map) {
    value_key_vector.push_back(std::make_pair(value_mapper(kv_pair.second), kv_pair.first));
  }

  // Sort in reverse (descending order)
  std::sort(value_key_vector.rbegin(), value_key_vector.rend());
  return value_key_vector;
}

// Fixup a remote pointer that we read from a foreign boot.art to point to our own memory.
// Returned pointer will point to inside of remote_contents.
template <typename T>
static T* FixUpRemotePointer(T* remote_ptr,
                             std::vector<uint8_t>& remote_contents,
                             const backtrace_map_t& boot_map) {
  if (remote_ptr == nullptr) {
    return nullptr;
  }

  uintptr_t remote = reinterpret_cast<uintptr_t>(remote_ptr);

  CHECK_LE(boot_map.start, remote);
  CHECK_GT(boot_map.end, remote);

  off_t boot_offset = remote - boot_map.start;

  return reinterpret_cast<T*>(&remote_contents[boot_offset]);
}

template <typename T>
static T* RemoteContentsPointerToLocal(T* remote_ptr,
                                       std::vector<uint8_t>& remote_contents,
                                       const ImageHeader& image_header) {
  if (remote_ptr == nullptr) {
    return nullptr;
  }

  uint8_t* remote = reinterpret_cast<uint8_t*>(remote_ptr);
  ptrdiff_t boot_offset = remote - &remote_contents[0];

  const uint8_t* local_ptr = reinterpret_cast<const uint8_t*>(&image_header) + boot_offset;

  return reinterpret_cast<T*>(const_cast<uint8_t*>(local_ptr));
}

template <typename T> size_t EntrySize(T* entry);
template<> size_t EntrySize(mirror::Object* object) REQUIRES_SHARED(Locks::mutator_lock_) {
  return object->SizeOf();
}
template<> size_t EntrySize(ArtMethod* art_method) REQUIRES_SHARED(Locks::mutator_lock_) {
  return sizeof(*art_method);
}

template <typename T>
static bool EntriesDiffer(T* entry1, T* entry2) REQUIRES_SHARED(Locks::mutator_lock_) {
  return memcmp(entry1, entry2, EntrySize(entry1)) != 0;
}

template <typename T>
struct RegionCommon {
 public:
  RegionCommon(std::ostream* os,
               std::vector<uint8_t>* remote_contents,
               std::vector<uint8_t>* zygote_contents,
               const backtrace_map_t& boot_map,
               const ImageHeader& image_header) :
    os_(*os),
    remote_contents_(remote_contents),
    zygote_contents_(zygote_contents),
    boot_map_(boot_map),
    image_header_(image_header),
    different_entries_(0),
    dirty_entry_bytes_(0),
    false_dirty_entry_bytes_(0) {
    CHECK(remote_contents != nullptr);
    CHECK(zygote_contents != nullptr);
  }

  void DumpSamplesAndOffsetCount() {
    os_ << "      sample object addresses: ";
    for (size_t i = 0; i < dirty_entries_.size() && i < kMaxAddressPrint; ++i) {
      T* entry = dirty_entries_[i];
      os_ << reinterpret_cast<void*>(entry) << ", ";
    }
    os_ << "\n";
    os_ << "      dirty byte +offset:count list = ";
    std::vector<std::pair<size_t, off_t>> field_dirty_count_sorted =
        SortByValueDesc<off_t, size_t, size_t>(field_dirty_count_);
    for (const std::pair<size_t, off_t>& pair : field_dirty_count_sorted) {
      off_t offset = pair.second;
      size_t count = pair.first;
      os_ << "+" << offset << ":" << count << ", ";
    }
    os_ << "\n";
  }

  size_t GetDifferentEntryCount() const { return different_entries_; }
  size_t GetDirtyEntryBytes() const { return dirty_entry_bytes_; }
  size_t GetFalseDirtyEntryCount() const { return false_dirty_entries_.size(); }
  size_t GetFalseDirtyEntryBytes() const { return false_dirty_entry_bytes_; }
  size_t GetZygoteDirtyEntryCount() const { return zygote_dirty_entries_.size(); }

 protected:
  bool IsEntryOnDirtyPage(T* entry, const std::set<size_t>& dirty_pages) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t size = EntrySize(entry);
    size_t page_off = 0;
    size_t current_page_idx;
    uintptr_t entry_address = reinterpret_cast<uintptr_t>(entry);
    // Iterate every page this entry belongs to
    do {
      current_page_idx = entry_address / kPageSize + page_off;
      if (dirty_pages.find(current_page_idx) != dirty_pages.end()) {
        // This entry is on a dirty page
        return true;
      }
      page_off++;
    } while ((current_page_idx * kPageSize) < RoundUp(entry_address + size, kObjectAlignment));
    return false;
  }

  void AddZygoteDirtyEntry(T* entry) REQUIRES_SHARED(Locks::mutator_lock_) {
    zygote_dirty_entries_.insert(entry);
  }

  void AddImageDirtyEntry(T* entry) REQUIRES_SHARED(Locks::mutator_lock_) {
    image_dirty_entries_.insert(entry);
  }

  void AddFalseDirtyEntry(T* entry) REQUIRES_SHARED(Locks::mutator_lock_) {
    false_dirty_entries_.push_back(entry);
    false_dirty_entry_bytes_ += EntrySize(entry);
  }

  // The output stream to write to.
  std::ostream& os_;
  // The byte contents of the remote (image) process' image.
  std::vector<uint8_t>* remote_contents_;
  // The byte contents of the zygote process' image.
  std::vector<uint8_t>* zygote_contents_;
  const backtrace_map_t& boot_map_;
  const ImageHeader& image_header_;

  // Count of entries that are different.
  size_t different_entries_;

  // Local entries that are dirty (differ in at least one byte).
  size_t dirty_entry_bytes_;
  std::vector<T*> dirty_entries_;

  // Local entries that are clean, but located on dirty pages.
  size_t false_dirty_entry_bytes_;
  std::vector<T*> false_dirty_entries_;

  // Image dirty entries
  // If zygote_pid_only_ == true, these are shared dirty entries in the zygote.
  // If zygote_pid_only_ == false, these are private dirty entries in the application.
  std::set<T*> image_dirty_entries_;

  // Zygote dirty entries (probably private dirty).
  // We only add entries here if they differed in both the image and the zygote, so
  // they are probably private dirty.
  std::set<T*> zygote_dirty_entries_;

  std::map<off_t /* field offset */, size_t /* count */> field_dirty_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegionCommon);
};

template <typename T>
class RegionSpecializedBase : public RegionCommon<T> {
};

// Region analysis for mirror::Objects
class ImgObjectVisitor : public ObjectVisitor {
 public:
  using ComputeDirtyFunc = std::function<void(mirror::Object* object,
                                              const uint8_t* begin_image_ptr,
                                              const std::set<size_t>& dirty_pages)>;
  ImgObjectVisitor(ComputeDirtyFunc dirty_func,
                   const uint8_t* begin_image_ptr,
                   const std::set<size_t>& dirty_pages) :
    dirty_func_(dirty_func),
    begin_image_ptr_(begin_image_ptr),
    dirty_pages_(dirty_pages) { }

  virtual ~ImgObjectVisitor() OVERRIDE { }

  virtual void Visit(mirror::Object* object) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    // Sanity check that we are reading a real mirror::Object
    CHECK(object->GetClass() != nullptr) << "Image object at address "
                                         << object
                                         << " has null class";
    if (kUseBakerReadBarrier) {
      object->AssertReadBarrierState();
    }
    dirty_func_(object, begin_image_ptr_, dirty_pages_);
  }

 private:
  ComputeDirtyFunc dirty_func_;
  const uint8_t* begin_image_ptr_;
  const std::set<size_t>& dirty_pages_;
};

template<>
class RegionSpecializedBase<mirror::Object> : public RegionCommon<mirror::Object> {
 public:
  RegionSpecializedBase(std::ostream* os,
                        std::vector<uint8_t>* remote_contents,
                        std::vector<uint8_t>* zygote_contents,
                        const backtrace_map_t& boot_map,
                        const ImageHeader& image_header,
                        bool dump_dirty_objects)
      : RegionCommon<mirror::Object>(os, remote_contents, zygote_contents, boot_map, image_header),
        os_(*os),
        dump_dirty_objects_(dump_dirty_objects) { }

  // Define a common public type name for use by RegionData.
  using VisitorClass = ImgObjectVisitor;

  void VisitEntries(VisitorClass* visitor,
                    uint8_t* base,
                    PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    RegionCommon<mirror::Object>::image_header_.VisitObjects(visitor, base, pointer_size);
  }

  void VisitEntry(mirror::Object* entry)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Unconditionally store the class descriptor in case we need it later
    mirror::Class* klass = entry->GetClass();
    class_data_[klass].descriptor = GetClassDescriptor(klass);
  }

  void AddCleanEntry(mirror::Object* entry)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    class_data_[entry->GetClass()].AddCleanObject();
  }

  void AddFalseDirtyEntry(mirror::Object* entry)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    RegionCommon<mirror::Object>::AddFalseDirtyEntry(entry);
    class_data_[entry->GetClass()].AddFalseDirtyObject(entry);
  }

  void AddDirtyEntry(mirror::Object* entry, mirror::Object* entry_remote)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t entry_size = EntrySize(entry);
    ++different_entries_;
    dirty_entry_bytes_ += entry_size;
    // Log dirty count and objects for class objects only.
    mirror::Class* klass = entry->GetClass();
    if (klass->IsClassClass()) {
      // Increment counts for the fields that are dirty
      const uint8_t* current = reinterpret_cast<const uint8_t*>(entry);
      const uint8_t* current_remote = reinterpret_cast<const uint8_t*>(entry_remote);
      for (size_t i = 0; i < entry_size; ++i) {
        if (current[i] != current_remote[i]) {
          field_dirty_count_[i]++;
        }
      }
      dirty_entries_.push_back(entry);
    }
    class_data_[klass].AddDirtyObject(entry, entry_remote);
  }

  void DiffEntryContents(mirror::Object* entry,
                         uint8_t* remote_bytes,
                         const uint8_t* base_ptr,
                         bool log_dirty_objects)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const char* tabs = "    ";
    // Attempt to find fields for all dirty bytes.
    mirror::Class* klass = entry->GetClass();
    if (entry->IsClass()) {
      os_ << tabs
          << "Class " << mirror::Class::PrettyClass(entry->AsClass()) << " " << entry << "\n";
    } else {
      os_ << tabs
          << "Instance of " << mirror::Class::PrettyClass(klass) << " " << entry << "\n";
    }

    std::unordered_set<ArtField*> dirty_instance_fields;
    std::unordered_set<ArtField*> dirty_static_fields;
    // Examine the bytes comprising the Object, computing which fields are dirty
    // and recording them for later display.  If the Object is an array object,
    // compute the dirty entries.
    mirror::Object* remote_entry = reinterpret_cast<mirror::Object*>(remote_bytes);
    for (size_t i = 0, count = entry->SizeOf(); i < count; ++i) {
      if (base_ptr[i] != remote_bytes[i]) {
        ArtField* field = ArtField::FindInstanceFieldWithOffset</*exact*/false>(klass, i);
        if (field != nullptr) {
          dirty_instance_fields.insert(field);
        } else if (entry->IsClass()) {
          field = ArtField::FindStaticFieldWithOffset</*exact*/false>(entry->AsClass(), i);
          if (field != nullptr) {
            dirty_static_fields.insert(field);
          }
        }
        if (field == nullptr) {
          if (klass->IsArrayClass()) {
            mirror::Class* component_type = klass->GetComponentType();
            Primitive::Type primitive_type = component_type->GetPrimitiveType();
            size_t component_size = Primitive::ComponentSize(primitive_type);
            size_t data_offset = mirror::Array::DataOffset(component_size).Uint32Value();
            if (i >= data_offset) {
              os_ << tabs << "Dirty array element " << (i - data_offset) / component_size << "\n";
              // Skip to next element to prevent spam.
              i += component_size - 1;
              continue;
            }
          }
          os_ << tabs << "No field for byte offset " << i << "\n";
        }
      }
    }
    // Dump different fields.
    if (!dirty_instance_fields.empty()) {
      os_ << tabs << "Dirty instance fields " << dirty_instance_fields.size() << "\n";
      for (ArtField* field : dirty_instance_fields) {
        os_ << tabs << ArtField::PrettyField(field)
            << " original=" << PrettyFieldValue(field, entry)
            << " remote=" << PrettyFieldValue(field, remote_entry) << "\n";
      }
    }
    if (!dirty_static_fields.empty()) {
      if (dump_dirty_objects_ && log_dirty_objects) {
        dirty_objects_.insert(entry);
      }
      os_ << tabs << "Dirty static fields " << dirty_static_fields.size() << "\n";
      for (ArtField* field : dirty_static_fields) {
        os_ << tabs << ArtField::PrettyField(field)
            << " original=" << PrettyFieldValue(field, entry)
            << " remote=" << PrettyFieldValue(field, remote_entry) << "\n";
      }
    }
    os_ << "\n";
  }

  void DumpDirtyObjects() REQUIRES_SHARED(Locks::mutator_lock_) {
    for (mirror::Object* obj : dirty_objects_) {
      if (obj->IsClass()) {
        os_ << "Private dirty object: " << obj->AsClass()->PrettyDescriptor() << "\n";
      }
    }
  }

  void DumpDirtyEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
    // vector of pairs (size_t count, Class*)
    auto dirty_object_class_values =
        SortByValueDesc<mirror::Class*, size_t, ClassData>(
            class_data_,
            [](const ClassData& d) { return d.dirty_object_count; });
    os_ << "\n" << "  Dirty object count by class:\n";
    for (const auto& vk_pair : dirty_object_class_values) {
      size_t dirty_object_count = vk_pair.first;
      mirror::Class* klass = vk_pair.second;
      ClassData& class_data = class_data_[klass];
      size_t object_sizes = class_data.dirty_object_size_in_bytes;
      float avg_dirty_bytes_per_class =
          class_data.dirty_object_byte_count * 1.0f / object_sizes;
      float avg_object_size = object_sizes * 1.0f / dirty_object_count;
      const std::string& descriptor = class_data.descriptor;
      os_ << "    " << mirror::Class::PrettyClass(klass) << " ("
          << "objects: " << dirty_object_count << ", "
          << "avg dirty bytes: " << avg_dirty_bytes_per_class << ", "
          << "avg object size: " << avg_object_size << ", "
          << "class descriptor: '" << descriptor << "'"
          << ")\n";
      if (strcmp(descriptor.c_str(), "Ljava/lang/Class;") == 0) {
        DumpSamplesAndOffsetCount();
        os_ << "      field contents:\n";
        for (mirror::Object* object : class_data.dirty_objects) {
          // remote class object
          auto remote_klass = reinterpret_cast<mirror::Class*>(object);
          // local class object
          auto local_klass =
              RemoteContentsPointerToLocal(remote_klass,
                                           *RegionCommon<mirror::Object>::remote_contents_,
                                           RegionCommon<mirror::Object>::image_header_);
          os_ << "        " << reinterpret_cast<const void*>(object) << " ";
          os_ << "  class_status (remote): " << remote_klass->GetStatus() << ", ";
          os_ << "  class_status (local): " << local_klass->GetStatus();
          os_ << "\n";
        }
      }
    }
  }

  void DumpFalseDirtyEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
    // vector of pairs (size_t count, Class*)
    auto false_dirty_object_class_values =
        SortByValueDesc<mirror::Class*, size_t, ClassData>(
            class_data_,
            [](const ClassData& d) { return d.false_dirty_object_count; });
    os_ << "\n" << "  False-dirty object count by class:\n";
    for (const auto& vk_pair : false_dirty_object_class_values) {
      size_t object_count = vk_pair.first;
      mirror::Class* klass = vk_pair.second;
      ClassData& class_data = class_data_[klass];
      size_t object_sizes = class_data.false_dirty_byte_count;
      float avg_object_size = object_sizes * 1.0f / object_count;
      const std::string& descriptor = class_data.descriptor;
      os_ << "    " << mirror::Class::PrettyClass(klass) << " ("
          << "objects: " << object_count << ", "
          << "avg object size: " << avg_object_size << ", "
          << "total bytes: " << object_sizes << ", "
          << "class descriptor: '" << descriptor << "'"
          << ")\n";
    }
  }

  void DumpCleanEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
    // vector of pairs (size_t count, Class*)
    auto clean_object_class_values =
        SortByValueDesc<mirror::Class*, size_t, ClassData>(
            class_data_,
            [](const ClassData& d) { return d.clean_object_count; });
    os_ << "\n" << "  Clean object count by class:\n";
    for (const auto& vk_pair : clean_object_class_values) {
      os_ << "    " << mirror::Class::PrettyClass(vk_pair.second) << " (" << vk_pair.first << ")\n";
    }
  }

 private:
  // Aggregate and detail class data from an image diff.
  struct ClassData {
    size_t dirty_object_count = 0;
    // Track only the byte-per-byte dirtiness (in bytes)
    size_t dirty_object_byte_count = 0;
    // Track the object-by-object dirtiness (in bytes)
    size_t dirty_object_size_in_bytes = 0;
    size_t clean_object_count = 0;
    std::string descriptor;
    size_t false_dirty_byte_count = 0;
    size_t false_dirty_object_count = 0;
    std::vector<mirror::Object*> false_dirty_objects;
    // Remote pointers to dirty objects
    std::vector<mirror::Object*> dirty_objects;

    void AddCleanObject() REQUIRES_SHARED(Locks::mutator_lock_) {
      ++clean_object_count;
    }

    void AddDirtyObject(mirror::Object* object, mirror::Object* object_remote)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      ++dirty_object_count;
      dirty_object_byte_count += CountDirtyBytes(object, object_remote);
      dirty_object_size_in_bytes += EntrySize(object);
      dirty_objects.push_back(object_remote);
    }

    void AddFalseDirtyObject(mirror::Object* object) REQUIRES_SHARED(Locks::mutator_lock_) {
      ++false_dirty_object_count;
      false_dirty_objects.push_back(object);
      false_dirty_byte_count += EntrySize(object);
    }

   private:
    // Go byte-by-byte and figure out what exactly got dirtied
    static size_t CountDirtyBytes(mirror::Object* object1, mirror::Object* object2)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      const uint8_t* cur1 = reinterpret_cast<const uint8_t*>(object1);
      const uint8_t* cur2 = reinterpret_cast<const uint8_t*>(object2);
      size_t dirty_bytes = 0;
      size_t object_size = EntrySize(object1);
      for (size_t i = 0; i < object_size; ++i) {
        if (cur1[i] != cur2[i]) {
          dirty_bytes++;
        }
      }
      return dirty_bytes;
    }
  };

  std::ostream& os_;
  bool dump_dirty_objects_;
  std::unordered_set<mirror::Object*> dirty_objects_;
  std::map<mirror::Class*, ClassData> class_data_;

  DISALLOW_COPY_AND_ASSIGN(RegionSpecializedBase);
};

// Region analysis for ArtMethods.
class ImgArtMethodVisitor : public ArtMethodVisitor {
 public:
  using ComputeDirtyFunc = std::function<void(ArtMethod*,
                                              const uint8_t*,
                                              const std::set<size_t>&)>;
  ImgArtMethodVisitor(ComputeDirtyFunc dirty_func,
                      const uint8_t* begin_image_ptr,
                      const std::set<size_t>& dirty_pages) :
    dirty_func_(dirty_func),
    begin_image_ptr_(begin_image_ptr),
    dirty_pages_(dirty_pages) { }
  virtual ~ImgArtMethodVisitor() OVERRIDE { }
  virtual void Visit(ArtMethod* method) OVERRIDE {
    dirty_func_(method, begin_image_ptr_, dirty_pages_);
  }

 private:
  ComputeDirtyFunc dirty_func_;
  const uint8_t* begin_image_ptr_;
  const std::set<size_t>& dirty_pages_;
};

// Struct and functor for computing offsets of members of ArtMethods.
// template <typename RegionType>
struct MemberInfo {
  template <typename T>
  void operator() (const ArtMethod* method, const T* member_address, const std::string& name) {
    // Check that member_address is a pointer inside *method.
    DCHECK(reinterpret_cast<uintptr_t>(method) <= reinterpret_cast<uintptr_t>(member_address));
    DCHECK(reinterpret_cast<uintptr_t>(member_address) + sizeof(T) <=
           reinterpret_cast<uintptr_t>(method) + sizeof(ArtMethod));
    size_t offset =
        reinterpret_cast<uintptr_t>(member_address) - reinterpret_cast<uintptr_t>(method);
    offset_to_name_size_.insert({offset, NameAndSize(sizeof(T), name)});
  }

  struct NameAndSize {
    size_t size_;
    std::string name_;
    NameAndSize(size_t size, const std::string& name) : size_(size), name_(name) { }
    NameAndSize() : size_(0), name_("INVALID") { }
  };

  std::map<size_t, NameAndSize> offset_to_name_size_;
};

template<>
class RegionSpecializedBase<ArtMethod> : public RegionCommon<ArtMethod> {
 public:
  RegionSpecializedBase(std::ostream* os,
                        std::vector<uint8_t>* remote_contents,
                        std::vector<uint8_t>* zygote_contents,
                        const backtrace_map_t& boot_map,
                        const ImageHeader& image_header,
                        bool dump_dirty_objects ATTRIBUTE_UNUSED)
      : RegionCommon<ArtMethod>(os, remote_contents, zygote_contents, boot_map, image_header),
        os_(*os) {
    // Prepare the table for offset to member lookups.
    ArtMethod* art_method = reinterpret_cast<ArtMethod*>(&(*remote_contents)[0]);
    art_method->VisitMembers(member_info_);
    // Prepare the table for address to symbolic entry point names.
    BuildEntryPointNames();
    class_linker_ = Runtime::Current()->GetClassLinker();
  }

  // Define a common public type name for use by RegionData.
  using VisitorClass = ImgArtMethodVisitor;

  void VisitEntries(VisitorClass* visitor,
                    uint8_t* base,
                    PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    RegionCommon<ArtMethod>::image_header_.VisitPackedArtMethods(visitor, base, pointer_size);
  }

  void VisitEntry(ArtMethod* method ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void AddCleanEntry(ArtMethod* method ATTRIBUTE_UNUSED) {
  }

  void AddFalseDirtyEntry(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    RegionCommon<ArtMethod>::AddFalseDirtyEntry(method);
  }

  void AddDirtyEntry(ArtMethod* method, ArtMethod* method_remote)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t entry_size = EntrySize(method);
    ++different_entries_;
    dirty_entry_bytes_ += entry_size;
    // Increment counts for the fields that are dirty
    const uint8_t* current = reinterpret_cast<const uint8_t*>(method);
    const uint8_t* current_remote = reinterpret_cast<const uint8_t*>(method_remote);
    // ArtMethods always log their dirty count and entries.
    for (size_t i = 0; i < entry_size; ++i) {
      if (current[i] != current_remote[i]) {
        field_dirty_count_[i]++;
      }
    }
    dirty_entries_.push_back(method);
  }

  void DiffEntryContents(ArtMethod* method,
                         uint8_t* remote_bytes,
                         const uint8_t* base_ptr,
                         bool log_dirty_objects ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const char* tabs = "    ";
    os_ << tabs << "ArtMethod " << ArtMethod::PrettyMethod(method) << "\n";

    std::unordered_set<size_t> dirty_members;
    // Examine the members comprising the ArtMethod, computing which members are dirty.
    for (const std::pair<size_t, MemberInfo::NameAndSize>& p : member_info_.offset_to_name_size_) {
      const size_t offset = p.first;
      if (memcmp(base_ptr + offset, remote_bytes + offset, p.second.size_) != 0) {
        dirty_members.insert(p.first);
      }
    }
    // Dump different fields.
    if (!dirty_members.empty()) {
      os_ << tabs << "Dirty members " << dirty_members.size() << "\n";
      for (size_t offset : dirty_members) {
        const MemberInfo::NameAndSize& member_info = member_info_.offset_to_name_size_[offset];
        os_ << tabs << member_info.name_
            << " original=" << StringFromBytes(base_ptr + offset, member_info.size_)
            << " remote=" << StringFromBytes(remote_bytes + offset, member_info.size_)
            << "\n";
      }
    }
    os_ << "\n";
  }

  void DumpDirtyObjects() REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void DumpDirtyEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
    DumpSamplesAndOffsetCount();
    os_ << "      offset to field map:\n";
    for (const std::pair<size_t, MemberInfo::NameAndSize>& p : member_info_.offset_to_name_size_) {
      const size_t offset = p.first;
      const size_t size = p.second.size_;
      os_ << StringPrintf("        %zu-%zu: ", offset, offset + size - 1)
          << p.second.name_
          << std::endl;
    }

    os_ << "      field contents:\n";
    for (ArtMethod* method : dirty_entries_) {
      // remote method
      auto art_method = reinterpret_cast<ArtMethod*>(method);
      // remote class
      mirror::Class* remote_declaring_class =
        FixUpRemotePointer(art_method->GetDeclaringClass(),
                           *RegionCommon<ArtMethod>::remote_contents_,
                           RegionCommon<ArtMethod>::boot_map_);
      // local class
      mirror::Class* declaring_class =
        RemoteContentsPointerToLocal(remote_declaring_class,
                                     *RegionCommon<ArtMethod>::remote_contents_,
                                     RegionCommon<ArtMethod>::image_header_);
      DumpOneArtMethod(art_method, declaring_class, remote_declaring_class);
    }
  }

  void DumpFalseDirtyEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
    os_ << "\n" << "  False-dirty ArtMethods\n";
    os_ << "      field contents:\n";
    for (ArtMethod* method : false_dirty_entries_) {
      // local class
      mirror::Class* declaring_class = method->GetDeclaringClass();
      DumpOneArtMethod(method, declaring_class, nullptr);
    }
  }

  void DumpCleanEntries() REQUIRES_SHARED(Locks::mutator_lock_) {
  }

 private:
  std::ostream& os_;
  MemberInfo member_info_;
  std::map<const void*, std::string> entry_point_names_;
  ClassLinker* class_linker_;

  // Compute a map of addresses to names in the boot OAT file(s).
  void BuildEntryPointNames() {
    OatFileManager& oat_file_manager = Runtime::Current()->GetOatFileManager();
    std::vector<const OatFile*> boot_oat_files = oat_file_manager.GetBootOatFiles();
    for (const OatFile* oat_file : boot_oat_files) {
      const OatHeader& oat_header = oat_file->GetOatHeader();
      const void* i2ib = oat_header.GetInterpreterToInterpreterBridge();
      if (i2ib != nullptr) {
        entry_point_names_[i2ib] = "InterpreterToInterpreterBridge (from boot oat file)";
      }
      const void* i2ccb = oat_header.GetInterpreterToCompiledCodeBridge();
      if (i2ccb != nullptr) {
        entry_point_names_[i2ccb] = "InterpreterToCompiledCodeBridge (from boot oat file)";
      }
      const void* jdl = oat_header.GetJniDlsymLookup();
      if (jdl != nullptr) {
        entry_point_names_[jdl] = "JniDlsymLookup (from boot oat file)";
      }
      const void* qgjt = oat_header.GetQuickGenericJniTrampoline();
      if (qgjt != nullptr) {
        entry_point_names_[qgjt] = "QuickGenericJniTrampoline (from boot oat file)";
      }
      const void* qrt = oat_header.GetQuickResolutionTrampoline();
      if (qrt != nullptr) {
        entry_point_names_[qrt] = "QuickResolutionTrampoline (from boot oat file)";
      }
      const void* qict = oat_header.GetQuickImtConflictTrampoline();
      if (qict != nullptr) {
        entry_point_names_[qict] = "QuickImtConflictTrampoline (from boot oat file)";
      }
      const void* q2ib = oat_header.GetQuickToInterpreterBridge();
      if (q2ib != nullptr) {
        entry_point_names_[q2ib] = "QuickToInterpreterBridge (from boot oat file)";
      }
    }
  }

  std::string StringFromBytes(const uint8_t* bytes, size_t size) {
    switch (size) {
      case 1:
        return StringPrintf("%" PRIx8, *bytes);
      case 2:
        return StringPrintf("%" PRIx16, *reinterpret_cast<const uint16_t*>(bytes));
      case 4:
      case 8: {
        // Compute an address if the bytes might contain one.
        uint64_t intval;
        if (size == 4) {
          intval = *reinterpret_cast<const uint32_t*>(bytes);
        } else {
          intval = *reinterpret_cast<const uint64_t*>(bytes);
        }
        const void* addr = reinterpret_cast<const void*>(intval);
        // Match the address against those that have Is* methods in the ClassLinker.
        if (class_linker_->IsQuickToInterpreterBridge(addr)) {
          return "QuickToInterpreterBridge";
        } else if (class_linker_->IsQuickGenericJniStub(addr)) {
          return "QuickGenericJniStub";
        } else if (class_linker_->IsQuickResolutionStub(addr)) {
          return "QuickResolutionStub";
        } else if (class_linker_->IsJniDlsymLookupStub(addr)) {
          return "JniDlsymLookupStub";
        }
        // Match the address against those that we saved from the boot OAT files.
        if (entry_point_names_.find(addr) != entry_point_names_.end()) {
          return entry_point_names_[addr];
        }
        return StringPrintf("%" PRIx64, intval);
      }
      default:
        LOG(WARNING) << "Don't know how to convert " << size << " bytes to integer";
        return "<UNKNOWN>";
    }
  }

  void DumpOneArtMethod(ArtMethod* art_method,
                        mirror::Class* declaring_class,
                        mirror::Class* remote_declaring_class)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PointerSize pointer_size = InstructionSetPointerSize(Runtime::Current()->GetInstructionSet());
    os_ << "        " << reinterpret_cast<const void*>(art_method) << " ";
    os_ << "  entryPointFromJni: "
        << reinterpret_cast<const void*>(art_method->GetDataPtrSize(pointer_size)) << ", ";
    os_ << "  entryPointFromQuickCompiledCode: "
        << reinterpret_cast<const void*>(
               art_method->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size))
        << ", ";
    os_ << "  isNative? " << (art_method->IsNative() ? "yes" : "no") << ", ";
    // Null for runtime metionds.
    if (declaring_class != nullptr) {
      os_ << "  class_status (local): " << declaring_class->GetStatus();
    }
    if (remote_declaring_class != nullptr) {
      os_ << ",  class_status (remote): " << remote_declaring_class->GetStatus();
    }
    os_ << "\n";
  }

  DISALLOW_COPY_AND_ASSIGN(RegionSpecializedBase);
};

template <typename T>
class RegionData : public RegionSpecializedBase<T> {
 public:
  RegionData(std::ostream* os,
             std::vector<uint8_t>* remote_contents,
             std::vector<uint8_t>* zygote_contents,
             const backtrace_map_t& boot_map,
             const ImageHeader& image_header,
             bool dump_dirty_objects)
      : RegionSpecializedBase<T>(os,
                                 remote_contents,
                                 zygote_contents,
                                 boot_map,
                                 image_header,
                                 dump_dirty_objects),
        os_(*os) {
    CHECK(remote_contents != nullptr);
    CHECK(zygote_contents != nullptr);
  }

  // Walk over the type T entries in theregion between begin_image_ptr and end_image_ptr,
  // collecting and reporting data regarding dirty, difference, etc.
  void ProcessRegion(const MappingData& mapping_data,
                     RemoteProcesses remotes,
                     const uint8_t* begin_image_ptr)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    typename RegionSpecializedBase<T>::VisitorClass visitor(
        [this](T* entry,
               const uint8_t* begin_image_ptr,
               const std::set<size_t>& dirty_page_set) REQUIRES_SHARED(Locks::mutator_lock_) {
          this->ComputeEntryDirty(entry, begin_image_ptr, dirty_page_set);
        },
        begin_image_ptr,
        mapping_data.dirty_page_set);
    PointerSize pointer_size = InstructionSetPointerSize(Runtime::Current()->GetInstructionSet());
    RegionSpecializedBase<T>::VisitEntries(&visitor,
                                           const_cast<uint8_t*>(begin_image_ptr),
                                           pointer_size);

    // Looking at only dirty pages, figure out how many of those bytes belong to dirty entries.
    // TODO: fix this now that there are multiple regions in a mapping.
    float true_dirtied_percent =
        RegionCommon<T>::GetDirtyEntryBytes() * 1.0f / (mapping_data.dirty_pages * kPageSize);

    // Entry specific statistics.
    os_ << RegionCommon<T>::GetDifferentEntryCount() << " different entries, \n  "
        << RegionCommon<T>::GetDirtyEntryBytes() << " different entry [bytes], \n  "
        << RegionCommon<T>::GetFalseDirtyEntryCount() << " false dirty entries,\n  "
        << RegionCommon<T>::GetFalseDirtyEntryBytes() << " false dirty entry [bytes], \n  "
        << true_dirtied_percent << " different entries-vs-total in a dirty page;\n  "
        << "\n";

    const uint8_t* base_ptr = begin_image_ptr;
    switch (remotes) {
      case RemoteProcesses::kZygoteOnly:
        os_ << "  Zygote shared dirty entries: ";
        break;
      case RemoteProcesses::kImageAndZygote:
        os_ << "  Application dirty entries (private dirty): ";
        // If we are dumping private dirty, diff against the zygote map to make it clearer what
        // fields caused the page to be private dirty.
        base_ptr = &RegionCommon<T>::zygote_contents_->operator[](0);
        break;
      case RemoteProcesses::kImageOnly:
        os_ << "  Application dirty entries (unknown whether private or shared dirty): ";
        break;
    }
    DiffDirtyEntries(ProcessType::kRemote,
                     begin_image_ptr,
                     RegionCommon<T>::remote_contents_,
                     base_ptr,
                     /*log_dirty_objects*/true);
    // Print shared dirty after since it's less important.
    if (RegionCommon<T>::GetZygoteDirtyEntryCount() != 0) {
      // We only reach this point if both pids were specified.  Furthermore,
      // entries are only displayed here if they differed in both the image
      // and the zygote, so they are probably private dirty.
      CHECK(remotes == RemoteProcesses::kImageAndZygote);
      os_ << "\n" << "  Zygote dirty entries (probably shared dirty): ";
      DiffDirtyEntries(ProcessType::kZygote,
                       begin_image_ptr,
                       RegionCommon<T>::zygote_contents_,
                       begin_image_ptr,
                       /*log_dirty_objects*/false);
    }
    RegionSpecializedBase<T>::DumpDirtyObjects();
    RegionSpecializedBase<T>::DumpDirtyEntries();
    RegionSpecializedBase<T>::DumpFalseDirtyEntries();
    RegionSpecializedBase<T>::DumpCleanEntries();
  }

 private:
  std::ostream& os_;

  void DiffDirtyEntries(ProcessType process_type,
                        const uint8_t* begin_image_ptr,
                        std::vector<uint8_t>* contents,
                        const uint8_t* base_ptr,
                        bool log_dirty_objects)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    os_ << RegionCommon<T>::dirty_entries_.size() << "\n";
    const std::set<T*>& entries =
        (process_type == ProcessType::kZygote) ?
            RegionCommon<T>::zygote_dirty_entries_:
            RegionCommon<T>::image_dirty_entries_;
    for (T* entry : entries) {
      uint8_t* entry_bytes = reinterpret_cast<uint8_t*>(entry);
      ptrdiff_t offset = entry_bytes - begin_image_ptr;
      uint8_t* remote_bytes = &(*contents)[offset];
      RegionSpecializedBase<T>::DiffEntryContents(entry,
                                                  remote_bytes,
                                                  &base_ptr[offset],
                                                  log_dirty_objects);
    }
  }

  void ComputeEntryDirty(T* entry,
                         const uint8_t* begin_image_ptr,
                         const std::set<size_t>& dirty_pages)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Set up pointers in the remote and the zygote for comparison.
    uint8_t* current = reinterpret_cast<uint8_t*>(entry);
    ptrdiff_t offset = current - begin_image_ptr;
    T* entry_remote =
        reinterpret_cast<T*>(const_cast<uint8_t*>(&(*RegionCommon<T>::remote_contents_)[offset]));
    const bool have_zygote = !RegionCommon<T>::zygote_contents_->empty();
    const uint8_t* current_zygote =
        have_zygote ? &(*RegionCommon<T>::zygote_contents_)[offset] : nullptr;
    T* entry_zygote = reinterpret_cast<T*>(const_cast<uint8_t*>(current_zygote));
    // Visit and classify entries at the current location.
    RegionSpecializedBase<T>::VisitEntry(entry);

    // Test private dirty first.
    bool is_dirty = false;
    if (have_zygote) {
      bool private_dirty = EntriesDiffer(entry_zygote, entry_remote);
      if (private_dirty) {
        // Private dirty, app vs zygote.
        is_dirty = true;
        RegionCommon<T>::AddImageDirtyEntry(entry);
      }
      if (EntriesDiffer(entry_zygote, entry)) {
        // Shared dirty, zygote vs image.
        is_dirty = true;
        RegionCommon<T>::AddZygoteDirtyEntry(entry);
      }
    } else if (EntriesDiffer(entry_remote, entry)) {
      // Shared or private dirty, app vs image.
      is_dirty = true;
      RegionCommon<T>::AddImageDirtyEntry(entry);
    }
    if (is_dirty) {
      // TODO: Add support dirty entries in zygote and image.
      RegionSpecializedBase<T>::AddDirtyEntry(entry, entry_remote);
    } else {
      RegionSpecializedBase<T>::AddCleanEntry(entry);
      if (RegionCommon<T>::IsEntryOnDirtyPage(entry, dirty_pages)) {
        // This entry was either never mutated or got mutated back to the same value.
        // TODO: Do I want to distinguish a "different" vs a "dirty" page here?
        RegionSpecializedBase<T>::AddFalseDirtyEntry(entry);
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(RegionData);
};

}  // namespace


class ImgDiagDumper {
 public:
  explicit ImgDiagDumper(std::ostream* os,
                         const ImageHeader& image_header,
                         const std::string& image_location,
                         pid_t image_diff_pid,
                         pid_t zygote_diff_pid,
                         bool dump_dirty_objects)
      : os_(os),
        image_header_(image_header),
        image_location_(image_location),
        image_diff_pid_(image_diff_pid),
        zygote_diff_pid_(zygote_diff_pid),
        dump_dirty_objects_(dump_dirty_objects),
        zygote_pid_only_(false) {}

  bool Init() {
    std::ostream& os = *os_;

    if (image_diff_pid_ < 0 && zygote_diff_pid_ < 0) {
      os << "Either --image-diff-pid or --zygote-diff-pid (or both) must be specified.\n";
      return false;
    }

    // To avoid the combinations of command-line argument use cases:
    // If the user invoked with only --zygote-diff-pid, shuffle that to
    // image_diff_pid_, invalidate zygote_diff_pid_, and remember that
    // image_diff_pid_ is now special.
    if (image_diff_pid_ < 0) {
      image_diff_pid_ = zygote_diff_pid_;
      zygote_diff_pid_ = -1;
      zygote_pid_only_ = true;
    }

    {
      struct stat sts;
      std::string proc_pid_str =
          StringPrintf("/proc/%ld", static_cast<long>(image_diff_pid_));  // NOLINT [runtime/int]
      if (stat(proc_pid_str.c_str(), &sts) == -1) {
        os << "Process does not exist";
        return false;
      }
    }

    // Open /proc/$pid/maps to view memory maps
    auto tmp_proc_maps = std::unique_ptr<BacktraceMap>(BacktraceMap::Create(image_diff_pid_));
    if (tmp_proc_maps == nullptr) {
      os << "Could not read backtrace maps";
      return false;
    }

    bool found_boot_map = false;
    // Find the memory map only for boot.art
    for (const backtrace_map_t* map : *tmp_proc_maps) {
      if (EndsWith(map->name, GetImageLocationBaseName())) {
        if ((map->flags & PROT_WRITE) != 0) {
          boot_map_ = *map;
          found_boot_map = true;
          break;
        }
        // In actuality there's more than 1 map, but the second one is read-only.
        // The one we care about is the write-able map.
        // The readonly maps are guaranteed to be identical, so its not interesting to compare
        // them.
      }
    }

    if (!found_boot_map) {
      os << "Could not find map for " << GetImageLocationBaseName();
      return false;
    }
    // Sanity check boot_map_.
    CHECK(boot_map_.end >= boot_map_.start);
    boot_map_size_ = boot_map_.end - boot_map_.start;

    // Open /proc/<image_diff_pid_>/mem and read as remote_contents_.
    std::string image_file_name =
        StringPrintf("/proc/%ld/mem", static_cast<long>(image_diff_pid_));  // NOLINT [runtime/int]
    auto image_map_file = std::unique_ptr<File>(OS::OpenFileForReading(image_file_name.c_str()));
    if (image_map_file == nullptr) {
      os << "Failed to open " << image_file_name << " for reading";
      return false;
    }
    std::vector<uint8_t> tmp_remote_contents(boot_map_size_);
    if (!image_map_file->PreadFully(&tmp_remote_contents[0], boot_map_size_, boot_map_.start)) {
      os << "Could not fully read file " << image_file_name;
      return false;
    }

    // If zygote_diff_pid_ != -1, open /proc/<zygote_diff_pid_>/mem and read as zygote_contents_.
    std::vector<uint8_t> tmp_zygote_contents;
    if (zygote_diff_pid_ != -1) {
      std::string zygote_file_name =
          StringPrintf("/proc/%ld/mem", static_cast<long>(zygote_diff_pid_));  // NOLINT [runtime/int]
      std::unique_ptr<File> zygote_map_file(OS::OpenFileForReading(zygote_file_name.c_str()));
      if (zygote_map_file == nullptr) {
        os << "Failed to open " << zygote_file_name << " for reading";
        return false;
      }
      // The boot map should be at the same address.
      tmp_zygote_contents.resize(boot_map_size_);
      if (!zygote_map_file->PreadFully(&tmp_zygote_contents[0], boot_map_size_, boot_map_.start)) {
        LOG(WARNING) << "Could not fully read zygote file " << zygote_file_name;
        return false;
      }
    }

    // Open /proc/<image_diff_pid_>/pagemap.
    std::string pagemap_file_name = StringPrintf(
        "/proc/%ld/pagemap", static_cast<long>(image_diff_pid_));  // NOLINT [runtime/int]
    auto tmp_pagemap_file =
        std::unique_ptr<File>(OS::OpenFileForReading(pagemap_file_name.c_str()));
    if (tmp_pagemap_file == nullptr) {
      os << "Failed to open " << pagemap_file_name << " for reading: " << strerror(errno);
      return false;
    }

    // Not truly clean, mmap-ing boot.art again would be more pristine, but close enough
    const char* clean_pagemap_file_name = "/proc/self/pagemap";
    auto tmp_clean_pagemap_file = std::unique_ptr<File>(
        OS::OpenFileForReading(clean_pagemap_file_name));
    if (tmp_clean_pagemap_file == nullptr) {
      os << "Failed to open " << clean_pagemap_file_name << " for reading: " << strerror(errno);
      return false;
    }

    auto tmp_kpageflags_file = std::unique_ptr<File>(OS::OpenFileForReading("/proc/kpageflags"));
    if (tmp_kpageflags_file == nullptr) {
      os << "Failed to open /proc/kpageflags for reading: " << strerror(errno);
      return false;
    }

    auto tmp_kpagecount_file = std::unique_ptr<File>(OS::OpenFileForReading("/proc/kpagecount"));
    if (tmp_kpagecount_file == nullptr) {
      os << "Failed to open /proc/kpagecount for reading:" << strerror(errno);
      return false;
    }

    // Commit the mappings, etc.
    proc_maps_ = std::move(tmp_proc_maps);
    remote_contents_ = std::move(tmp_remote_contents);
    zygote_contents_ = std::move(tmp_zygote_contents);
    pagemap_file_ = std::move(*tmp_pagemap_file.release());
    clean_pagemap_file_ = std::move(*tmp_clean_pagemap_file.release());
    kpageflags_file_ = std::move(*tmp_kpageflags_file.release());
    kpagecount_file_ = std::move(*tmp_kpagecount_file.release());

    return true;
  }

  bool Dump() REQUIRES_SHARED(Locks::mutator_lock_) {
    std::ostream& os = *os_;
    os << "IMAGE LOCATION: " << image_location_ << "\n\n";

    os << "MAGIC: " << image_header_.GetMagic() << "\n\n";

    os << "IMAGE BEGIN: " << reinterpret_cast<void*>(image_header_.GetImageBegin()) << "\n\n";

    PrintPidLine("IMAGE", image_diff_pid_);
    os << "\n\n";
    PrintPidLine("ZYGOTE", zygote_diff_pid_);
    bool ret = true;
    if (image_diff_pid_ >= 0 || zygote_diff_pid_ >= 0) {
      ret = DumpImageDiff();
      os << "\n\n";
    }

    os << std::flush;

    return ret;
  }

 private:
  bool DumpImageDiff()
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return DumpImageDiffMap();
  }

  bool ComputeDirtyBytes(const uint8_t* image_begin, MappingData* mapping_data /*out*/) {
    std::ostream& os = *os_;

    size_t virtual_page_idx = 0;   // Virtual page number (for an absolute memory address)
    size_t page_idx = 0;           // Page index relative to 0
    size_t previous_page_idx = 0;  // Previous page index relative to 0


    // Iterate through one page at a time. Boot map begin/end already implicitly aligned.
    for (uintptr_t begin = boot_map_.start; begin != boot_map_.end; begin += kPageSize) {
      ptrdiff_t offset = begin - boot_map_.start;

      // We treat the image header as part of the memory map for now
      // If we wanted to change this, we could pass base=start+sizeof(ImageHeader)
      // But it might still be interesting to see if any of the ImageHeader data mutated
      const uint8_t* local_ptr = reinterpret_cast<const uint8_t*>(&image_header_) + offset;
      uint8_t* remote_ptr = &remote_contents_[offset];

      if (memcmp(local_ptr, remote_ptr, kPageSize) != 0) {
        mapping_data->different_pages++;

        // Count the number of 32-bit integers that are different.
        for (size_t i = 0; i < kPageSize / sizeof(uint32_t); ++i) {
          uint32_t* remote_ptr_int32 = reinterpret_cast<uint32_t*>(remote_ptr);
          const uint32_t* local_ptr_int32 = reinterpret_cast<const uint32_t*>(local_ptr);

          if (remote_ptr_int32[i] != local_ptr_int32[i]) {
            mapping_data->different_int32s++;
          }
        }
      }
    }

    std::vector<size_t> private_dirty_pages_for_section(ImageHeader::kSectionCount, 0u);

    // Iterate through one byte at a time.
    ptrdiff_t page_off_begin = image_header_.GetImageBegin() - image_begin;
    for (uintptr_t begin = boot_map_.start; begin != boot_map_.end; ++begin) {
      previous_page_idx = page_idx;
      ptrdiff_t offset = begin - boot_map_.start;

      // We treat the image header as part of the memory map for now
      // If we wanted to change this, we could pass base=start+sizeof(ImageHeader)
      // But it might still be interesting to see if any of the ImageHeader data mutated
      const uint8_t* local_ptr = reinterpret_cast<const uint8_t*>(&image_header_) + offset;
      uint8_t* remote_ptr = &remote_contents_[offset];

      virtual_page_idx = reinterpret_cast<uintptr_t>(local_ptr) / kPageSize;

      // Calculate the page index, relative to the 0th page where the image begins
      page_idx = (offset + page_off_begin) / kPageSize;
      if (*local_ptr != *remote_ptr) {
        // Track number of bytes that are different
        mapping_data->different_bytes++;
      }

      // Independently count the # of dirty pages on the remote side
      size_t remote_virtual_page_idx = begin / kPageSize;
      if (previous_page_idx != page_idx) {
        uint64_t page_count = 0xC0FFEE;
        // TODO: virtual_page_idx needs to be from the same process
        std::string error_msg;
        int dirtiness = (IsPageDirty(&pagemap_file_,           // Image-diff-pid procmap
                                     &clean_pagemap_file_,     // Self procmap
                                     &kpageflags_file_,
                                     &kpagecount_file_,
                                     remote_virtual_page_idx,  // potentially "dirty" page
                                     virtual_page_idx,         // true "clean" page
                                     &page_count,
                                     &error_msg));
        if (dirtiness < 0) {
          os << error_msg;
          return false;
        } else if (dirtiness > 0) {
          mapping_data->dirty_pages++;
          mapping_data->dirty_page_set.insert(mapping_data->dirty_page_set.end(), virtual_page_idx);
        }

        bool is_dirty = dirtiness > 0;
        bool is_private = page_count == 1;

        if (page_count == 1) {
          mapping_data->private_pages++;
        }

        if (is_dirty && is_private) {
          mapping_data->private_dirty_pages++;
          for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
            const ImageHeader::ImageSections section = static_cast<ImageHeader::ImageSections>(i);
            if (image_header_.GetImageSection(section).Contains(offset)) {
              ++private_dirty_pages_for_section[i];
            }
          }
        }
      }
    }
    mapping_data->false_dirty_pages = mapping_data->dirty_pages - mapping_data->different_pages;
    // Print low-level (bytes, int32s, pages) statistics.
    os << mapping_data->different_bytes << " differing bytes,\n  "
       << mapping_data->different_int32s << " differing int32s,\n  "
       << mapping_data->different_pages << " differing pages,\n  "
       << mapping_data->dirty_pages << " pages are dirty;\n  "
       << mapping_data->false_dirty_pages << " pages are false dirty;\n  "
       << mapping_data->private_pages << " pages are private;\n  "
       << mapping_data->private_dirty_pages << " pages are Private_Dirty\n  "
       << "\n";

    size_t total_private_dirty_pages = std::accumulate(private_dirty_pages_for_section.begin(),
                                                       private_dirty_pages_for_section.end(),
                                                       0u);
    os << "Image sections (total private dirty pages " << total_private_dirty_pages << ")\n";
    for (size_t i = 0; i < ImageHeader::kSectionCount; ++i) {
      const ImageHeader::ImageSections section = static_cast<ImageHeader::ImageSections>(i);
      os << section << " " << image_header_.GetImageSection(section)
         << " private dirty pages=" << private_dirty_pages_for_section[i] << "\n";
    }
    os << "\n";

    return true;
  }

  // Look at /proc/$pid/mem and only diff the things from there
  bool DumpImageDiffMap()
      REQUIRES_SHARED(Locks::mutator_lock_) {
    std::ostream& os = *os_;
    std::string error_msg;

    // Walk the bytes and diff against our boot image
    os << "\nObserving boot image header at address "
       << reinterpret_cast<const void*>(&image_header_)
       << "\n\n";

    const uint8_t* image_begin_unaligned = image_header_.GetImageBegin();
    const uint8_t* image_end_unaligned = image_begin_unaligned + image_header_.GetImageSize();

    // Adjust range to nearest page
    const uint8_t* image_begin = AlignDown(image_begin_unaligned, kPageSize);
    const uint8_t* image_end = AlignUp(image_end_unaligned, kPageSize);

    if (reinterpret_cast<uintptr_t>(image_begin) > boot_map_.start ||
        reinterpret_cast<uintptr_t>(image_end) < boot_map_.end) {
      // Sanity check that we aren't trying to read a completely different boot image
      os << "Remote boot map is out of range of local boot map: " <<
        "local begin " << reinterpret_cast<const void*>(image_begin) <<
        ", local end " << reinterpret_cast<const void*>(image_end) <<
        ", remote begin " << reinterpret_cast<const void*>(boot_map_.start) <<
        ", remote end " << reinterpret_cast<const void*>(boot_map_.end);
      return false;
      // If we wanted even more validation we could map the ImageHeader from the file
    }

    MappingData mapping_data;

    os << "Mapping at [" << reinterpret_cast<void*>(boot_map_.start) << ", "
       << reinterpret_cast<void*>(boot_map_.end) << ") had:\n  ";
    if (!ComputeDirtyBytes(image_begin, &mapping_data)) {
      return false;
    }
    RemoteProcesses remotes;
    if (zygote_pid_only_) {
      remotes = RemoteProcesses::kZygoteOnly;
    } else if (zygote_diff_pid_ > 0) {
      remotes = RemoteProcesses::kImageAndZygote;
    } else {
      remotes = RemoteProcesses::kImageOnly;
    }

    // Check all the mirror::Object entries in the image.
    RegionData<mirror::Object> object_region_data(os_,
                                                  &remote_contents_,
                                                  &zygote_contents_,
                                                  boot_map_,
                                                  image_header_,
                                                  dump_dirty_objects_);
    object_region_data.ProcessRegion(mapping_data,
                                     remotes,
                                     image_begin_unaligned);

    // Check all the ArtMethod entries in the image.
    RegionData<ArtMethod> artmethod_region_data(os_,
                                                &remote_contents_,
                                                &zygote_contents_,
                                                boot_map_,
                                                image_header_,
                                                dump_dirty_objects_);
    artmethod_region_data.ProcessRegion(mapping_data,
                                        remotes,
                                        image_begin_unaligned);
    return true;
  }

  static bool GetPageFrameNumber(File* page_map_file,
                                size_t virtual_page_index,
                                uint64_t* page_frame_number,
                                std::string* error_msg) {
    CHECK(page_map_file != nullptr);
    CHECK(page_frame_number != nullptr);
    CHECK(error_msg != nullptr);

    constexpr size_t kPageMapEntrySize = sizeof(uint64_t);
    constexpr uint64_t kPageFrameNumberMask = (1ULL << 55) - 1;  // bits 0-54 [in /proc/$pid/pagemap]
    constexpr uint64_t kPageSoftDirtyMask = (1ULL << 55);  // bit 55 [in /proc/$pid/pagemap]

    uint64_t page_map_entry = 0;

    // Read 64-bit entry from /proc/$pid/pagemap to get the physical page frame number
    if (!page_map_file->PreadFully(&page_map_entry, kPageMapEntrySize,
                                  virtual_page_index * kPageMapEntrySize)) {
      *error_msg = StringPrintf("Failed to read the virtual page index entry from %s",
                                page_map_file->GetPath().c_str());
      return false;
    }

    // TODO: seems useless, remove this.
    bool soft_dirty = (page_map_entry & kPageSoftDirtyMask) != 0;
    if ((false)) {
      LOG(VERBOSE) << soft_dirty;  // Suppress unused warning
      UNREACHABLE();
    }

    *page_frame_number = page_map_entry & kPageFrameNumberMask;

    return true;
  }

  static int IsPageDirty(File* page_map_file,
                         File* clean_pagemap_file,
                         File* kpageflags_file,
                         File* kpagecount_file,
                         size_t virtual_page_idx,
                         size_t clean_virtual_page_idx,
                         // Out parameters:
                         uint64_t* page_count, std::string* error_msg) {
    CHECK(page_map_file != nullptr);
    CHECK(clean_pagemap_file != nullptr);
    CHECK_NE(page_map_file, clean_pagemap_file);
    CHECK(kpageflags_file != nullptr);
    CHECK(kpagecount_file != nullptr);
    CHECK(page_count != nullptr);
    CHECK(error_msg != nullptr);

    // Constants are from https://www.kernel.org/doc/Documentation/vm/pagemap.txt

    constexpr size_t kPageFlagsEntrySize = sizeof(uint64_t);
    constexpr size_t kPageCountEntrySize = sizeof(uint64_t);
    constexpr uint64_t kPageFlagsDirtyMask = (1ULL << 4);  // in /proc/kpageflags
    constexpr uint64_t kPageFlagsNoPageMask = (1ULL << 20);  // in /proc/kpageflags
    constexpr uint64_t kPageFlagsMmapMask = (1ULL << 11);  // in /proc/kpageflags

    uint64_t page_frame_number = 0;
    if (!GetPageFrameNumber(page_map_file, virtual_page_idx, &page_frame_number, error_msg)) {
      return -1;
    }

    uint64_t page_frame_number_clean = 0;
    if (!GetPageFrameNumber(clean_pagemap_file, clean_virtual_page_idx, &page_frame_number_clean,
                            error_msg)) {
      return -1;
    }

    // Read 64-bit entry from /proc/kpageflags to get the dirty bit for a page
    uint64_t kpage_flags_entry = 0;
    if (!kpageflags_file->PreadFully(&kpage_flags_entry,
                                     kPageFlagsEntrySize,
                                     page_frame_number * kPageFlagsEntrySize)) {
      *error_msg = StringPrintf("Failed to read the page flags from %s",
                                kpageflags_file->GetPath().c_str());
      return -1;
    }

    // Read 64-bit entyry from /proc/kpagecount to get mapping counts for a page
    if (!kpagecount_file->PreadFully(page_count /*out*/,
                                     kPageCountEntrySize,
                                     page_frame_number * kPageCountEntrySize)) {
      *error_msg = StringPrintf("Failed to read the page count from %s",
                                kpagecount_file->GetPath().c_str());
      return -1;
    }

    // There must be a page frame at the requested address.
    CHECK_EQ(kpage_flags_entry & kPageFlagsNoPageMask, 0u);
    // The page frame must be memory mapped
    CHECK_NE(kpage_flags_entry & kPageFlagsMmapMask, 0u);

    // Page is dirty, i.e. has diverged from file, if the 4th bit is set to 1
    bool flags_dirty = (kpage_flags_entry & kPageFlagsDirtyMask) != 0;

    // page_frame_number_clean must come from the *same* process
    // but a *different* mmap than page_frame_number
    if (flags_dirty) {
      CHECK_NE(page_frame_number, page_frame_number_clean);
    }

    return page_frame_number != page_frame_number_clean;
  }

  void PrintPidLine(const std::string& kind, pid_t pid) {
    if (pid < 0) {
      *os_ << kind << " DIFF PID: disabled\n\n";
    } else {
      *os_ << kind << " DIFF PID (" << pid << "): ";
    }
  }

  static bool EndsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  // Return suffix of the file path after the last /. (e.g. /foo/bar -> bar, bar -> bar)
  static std::string BaseName(const std::string& str) {
    size_t idx = str.rfind('/');
    if (idx == std::string::npos) {
      return str;
    }

    return str.substr(idx + 1);
  }

  // Return the image location, stripped of any directories, e.g. "boot.art" or "core.art"
  std::string GetImageLocationBaseName() const {
    return BaseName(std::string(image_location_));
  }

  std::ostream* os_;
  const ImageHeader& image_header_;
  const std::string image_location_;
  pid_t image_diff_pid_;  // Dump image diff against boot.art if pid is non-negative
  pid_t zygote_diff_pid_;  // Dump image diff against zygote boot.art if pid is non-negative
  bool dump_dirty_objects_;  // Adds dumping of objects that are dirty.
  bool zygote_pid_only_;  // The user only specified a pid for the zygote.

  // BacktraceMap used for finding the memory mapping of the image file.
  std::unique_ptr<BacktraceMap> proc_maps_;
  // Boot image mapping.
  backtrace_map_t boot_map_{};
  // The size of the boot image mapping.
  size_t boot_map_size_;
  // The contents of /proc/<image_diff_pid_>/maps.
  std::vector<uint8_t> remote_contents_;
  // The contents of /proc/<zygote_diff_pid_>/maps.
  std::vector<uint8_t> zygote_contents_;
  // A File for reading /proc/<zygote_diff_pid_>/maps.
  File pagemap_file_;
  // A File for reading /proc/self/pagemap.
  File clean_pagemap_file_;
  // A File for reading /proc/kpageflags.
  File kpageflags_file_;
  // A File for reading /proc/kpagecount.
  File kpagecount_file_;

  DISALLOW_COPY_AND_ASSIGN(ImgDiagDumper);
};

static int DumpImage(Runtime* runtime,
                     std::ostream* os,
                     pid_t image_diff_pid,
                     pid_t zygote_diff_pid,
                     bool dump_dirty_objects) {
  ScopedObjectAccess soa(Thread::Current());
  gc::Heap* heap = runtime->GetHeap();
  std::vector<gc::space::ImageSpace*> image_spaces = heap->GetBootImageSpaces();
  CHECK(!image_spaces.empty());
  for (gc::space::ImageSpace* image_space : image_spaces) {
    const ImageHeader& image_header = image_space->GetImageHeader();
    if (!image_header.IsValid()) {
      fprintf(stderr, "Invalid image header %s\n", image_space->GetImageLocation().c_str());
      return EXIT_FAILURE;
    }

    ImgDiagDumper img_diag_dumper(os,
                                  image_header,
                                  image_space->GetImageLocation(),
                                  image_diff_pid,
                                  zygote_diff_pid,
                                  dump_dirty_objects);
    if (!img_diag_dumper.Init()) {
      return EXIT_FAILURE;
    }
    if (!img_diag_dumper.Dump()) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

struct ImgDiagArgs : public CmdlineArgs {
 protected:
  using Base = CmdlineArgs;

  virtual ParseStatus ParseCustom(const StringPiece& option,
                                  std::string* error_msg) OVERRIDE {
    {
      ParseStatus base_parse = Base::ParseCustom(option, error_msg);
      if (base_parse != kParseUnknownArgument) {
        return base_parse;
      }
    }

    if (option.starts_with("--image-diff-pid=")) {
      const char* image_diff_pid = option.substr(strlen("--image-diff-pid=")).data();

      if (!ParseInt(image_diff_pid, &image_diff_pid_)) {
        *error_msg = "Image diff pid out of range";
        return kParseError;
      }
    } else if (option.starts_with("--zygote-diff-pid=")) {
      const char* zygote_diff_pid = option.substr(strlen("--zygote-diff-pid=")).data();

      if (!ParseInt(zygote_diff_pid, &zygote_diff_pid_)) {
        *error_msg = "Zygote diff pid out of range";
        return kParseError;
      }
    } else if (option == "--dump-dirty-objects") {
      dump_dirty_objects_ = true;
    } else {
      return kParseUnknownArgument;
    }

    return kParseOk;
  }

  virtual ParseStatus ParseChecks(std::string* error_msg) OVERRIDE {
    // Perform the parent checks.
    ParseStatus parent_checks = Base::ParseChecks(error_msg);
    if (parent_checks != kParseOk) {
      return parent_checks;
    }

    // Perform our own checks.

    if (kill(image_diff_pid_,
             /*sig*/0) != 0) {  // No signal is sent, perform error-checking only.
      // Check if the pid exists before proceeding.
      if (errno == ESRCH) {
        *error_msg = "Process specified does not exist";
      } else {
        *error_msg = StringPrintf("Failed to check process status: %s", strerror(errno));
      }
      return kParseError;
    } else if (instruction_set_ != InstructionSet::kNone && instruction_set_ != kRuntimeISA) {
      // Don't allow different ISAs since the images are ISA-specific.
      // Right now the code assumes both the runtime ISA and the remote ISA are identical.
      *error_msg = "Must use the default runtime ISA; changing ISA is not supported.";
      return kParseError;
    }

    return kParseOk;
  }

  virtual std::string GetUsage() const {
    std::string usage;

    usage +=
        "Usage: imgdiag [options] ...\n"
        "    Example: imgdiag --image-diff-pid=$(pidof dex2oat)\n"
        "    Example: adb shell imgdiag --image-diff-pid=$(pid zygote)\n"
        "\n";

    usage += Base::GetUsage();

    usage +=  // Optional.
        "  --image-diff-pid=<pid>: provide the PID of a process whose boot.art you want to diff.\n"
        "      Example: --image-diff-pid=$(pid zygote)\n"
        "  --zygote-diff-pid=<pid>: provide the PID of the zygote whose boot.art you want to diff "
        "against.\n"
        "      Example: --zygote-diff-pid=$(pid zygote)\n"
        "  --dump-dirty-objects: additionally output dirty objects of interest.\n"
        "\n";

    return usage;
  }

 public:
  pid_t image_diff_pid_ = -1;
  pid_t zygote_diff_pid_ = -1;
  bool dump_dirty_objects_ = false;
};

struct ImgDiagMain : public CmdlineMain<ImgDiagArgs> {
  virtual bool ExecuteWithRuntime(Runtime* runtime) {
    CHECK(args_ != nullptr);

    return DumpImage(runtime,
                     args_->os_,
                     args_->image_diff_pid_,
                     args_->zygote_diff_pid_,
                     args_->dump_dirty_objects_) == EXIT_SUCCESS;
  }
};

}  // namespace art

int main(int argc, char** argv) {
  art::ImgDiagMain main;
  return main.Main(argc, argv);
}
