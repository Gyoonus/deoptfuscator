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

#ifndef ART_DEX2OAT_LINKER_IMAGE_WRITER_H_
#define ART_DEX2OAT_LINKER_IMAGE_WRITER_H_

#include <stdint.h>
#include "base/memory_tool.h"

#include <cstddef>
#include <memory>
#include <ostream>
#include <set>
#include <stack>
#include <string>

#include "art_method.h"
#include "base/bit_utils.h"
#include "base/dchecked_vector.h"
#include "base/enums.h"
#include "base/length_prefixed_array.h"
#include "base/macros.h"
#include "base/os.h"
#include "base/safe_map.h"
#include "base/utils.h"
#include "class_table.h"
#include "driver/compiler_driver.h"
#include "image.h"
#include "intern_table.h"
#include "lock_word.h"
#include "mem_map.h"
#include "mirror/dex_cache.h"
#include "oat_file.h"
#include "obj_ptr.h"

namespace art {
namespace gc {
namespace accounting {
template <size_t kAlignment> class SpaceBitmap;
typedef SpaceBitmap<kObjectAlignment> ContinuousSpaceBitmap;
}  // namespace accounting
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

namespace mirror {
class ClassLoader;
}  // namespace mirror

class ClassLoaderVisitor;
class ImTable;
class ImtConflictTable;

static constexpr int kInvalidFd = -1;

namespace linker {

// Write a Space built during compilation for use during execution.
class ImageWriter FINAL {
 public:
  ImageWriter(const CompilerDriver& compiler_driver,
              uintptr_t image_begin,
              bool compile_pic,
              bool compile_app_image,
              ImageHeader::StorageMode image_storage_mode,
              const std::vector<const char*>& oat_filenames,
              const std::unordered_map<const DexFile*, size_t>& dex_file_oat_index_map,
              const std::unordered_set<std::string>* dirty_image_objects);

  bool PrepareImageAddressSpace();

  bool IsImageAddressSpaceReady() const {
    DCHECK(!image_infos_.empty());
    for (const ImageInfo& image_info : image_infos_) {
      if (image_info.image_roots_address_ == 0u) {
        return false;
      }
    }
    return true;
  }

  ObjPtr<mirror::ClassLoader> GetClassLoader() {
    CHECK_EQ(class_loaders_.size(), compile_app_image_ ? 1u : 0u);
    return compile_app_image_ ? *class_loaders_.begin() : nullptr;
  }

  template <typename T>
  T* GetImageAddress(T* object) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (object == nullptr || IsInBootImage(object)) {
      return object;
    } else {
      size_t oat_index = GetOatIndex(object);
      const ImageInfo& image_info = GetImageInfo(oat_index);
      return reinterpret_cast<T*>(image_info.image_begin_ + GetImageOffset(object));
    }
  }

  ArtMethod* GetImageMethodAddress(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetOatFileOffset(size_t oat_index) const {
    return GetImageInfo(oat_index).oat_offset_;
  }

  const uint8_t* GetOatFileBegin(size_t oat_index) const {
    return GetImageInfo(oat_index).oat_file_begin_;
  }

  // If image_fd is not kInvalidFd, then we use that for the image file. Otherwise we open
  // the names in image_filenames.
  // If oat_fd is not kInvalidFd, then we use that for the oat file. Otherwise we open
  // the names in oat_filenames.
  bool Write(int image_fd,
             const std::vector<const char*>& image_filenames,
             const std::vector<const char*>& oat_filenames)
      REQUIRES(!Locks::mutator_lock_);

  uintptr_t GetOatDataBegin(size_t oat_index) {
    return reinterpret_cast<uintptr_t>(GetImageInfo(oat_index).oat_data_begin_);
  }

  // Get the index of the oat file containing the dex file.
  //
  // This "oat_index" is used to retrieve information about the the memory layout
  // of the oat file and its associated image file, needed for link-time patching
  // of references to the image or across oat files.
  size_t GetOatIndexForDexFile(const DexFile* dex_file) const;

  // Get the index of the oat file containing the dex file served by the dex cache.
  size_t GetOatIndexForDexCache(ObjPtr<mirror::DexCache> dex_cache) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Update the oat layout for the given oat file.
  // This will make the oat_offset for the next oat file valid.
  void UpdateOatFileLayout(size_t oat_index,
                           size_t oat_loaded_size,
                           size_t oat_data_offset,
                           size_t oat_data_size);
  // Update information about the oat header, i.e. checksum and trampoline offsets.
  void UpdateOatFileHeader(size_t oat_index, const OatHeader& oat_header);

 private:
  using WorkStack = std::stack<std::pair<mirror::Object*, size_t>>;

  bool AllocMemory();

  // Mark the objects defined in this space in the given live bitmap.
  void RecordImageAllocations() REQUIRES_SHARED(Locks::mutator_lock_);

  // Classify different kinds of bins that objects end up getting packed into during image writing.
  // Ordered from dirtiest to cleanest (until ArtMethods).
  enum class Bin {
    kKnownDirty,                  // Known dirty objects from --dirty-image-objects list
    kMiscDirty,                   // Dex caches, object locks, etc...
    kClassVerified,               // Class verified, but initializers haven't been run
    // Unknown mix of clean/dirty:
    kRegular,
    kClassInitialized,            // Class initializers have been run
    // All classes get their own bins since their fields often dirty
    kClassInitializedFinalStatics,  // Class initializers have been run, no non-final statics
    // Likely-clean:
    kString,                      // [String] Almost always immutable (except for obj header).
    // Add more bins here if we add more segregation code.
    // Non mirror fields must be below.
    // ArtFields should be always clean.
    kArtField,
    // If the class is initialized, then the ArtMethods are probably clean.
    kArtMethodClean,
    // ArtMethods may be dirty if the class has native methods or a declaring class that isn't
    // initialized.
    kArtMethodDirty,
    // IMT (clean)
    kImTable,
    // Conflict tables (clean).
    kIMTConflictTable,
    // Runtime methods (always clean, do not have a length prefix array).
    kRuntimeMethod,
    // Dex cache arrays have a special slot for PC-relative addressing. Since they are
    // huge, and as such their dirtiness is not important for the clean/dirty separation,
    // we arbitrarily keep them at the end of the native data.
    kDexCacheArray,               // Arrays belonging to dex cache.
    kLast = kDexCacheArray,
    // Number of bins which are for mirror objects.
    kMirrorCount = kArtField,
  };
  friend std::ostream& operator<<(std::ostream& stream, const Bin& bin);

  enum class NativeObjectRelocationType {
    kArtField,
    kArtFieldArray,
    kArtMethodClean,
    kArtMethodArrayClean,
    kArtMethodDirty,
    kArtMethodArrayDirty,
    kRuntimeMethod,
    kIMTable,
    kIMTConflictTable,
    kDexCacheArray,
  };
  friend std::ostream& operator<<(std::ostream& stream, const NativeObjectRelocationType& type);

  enum class StubType {
    kInterpreterToInterpreterBridge,
    kInterpreterToCompiledCodeBridge,
    kJNIDlsymLookup,
    kQuickGenericJNITrampoline,
    kQuickIMTConflictTrampoline,
    kQuickResolutionTrampoline,
    kQuickToInterpreterBridge,
    kLast = kQuickToInterpreterBridge,
  };
  friend std::ostream& operator<<(std::ostream& stream, const StubType& stub_type);

  static constexpr size_t kBinBits =
      MinimumBitsToStore<uint32_t>(static_cast<size_t>(Bin::kMirrorCount) - 1);
  // uint32 = typeof(lockword_)
  // Subtract read barrier bits since we want these to remain 0, or else it may result in DCHECK
  // failures due to invalid read barrier bits during object field reads.
  static const size_t kBinShift = BitSizeOf<uint32_t>() - kBinBits - LockWord::kGCStateSize;
  // 111000.....0
  static const size_t kBinMask = ((static_cast<size_t>(1) << kBinBits) - 1) << kBinShift;

  // Number of bins, including non-mirror bins.
  static constexpr size_t kNumberOfBins = static_cast<size_t>(Bin::kLast) + 1u;

  // Number of stub types.
  static constexpr size_t kNumberOfStubTypes = static_cast<size_t>(StubType::kLast) + 1u;

  // We use the lock word to store the bin # and bin index of the object in the image.
  //
  // The struct size must be exactly sizeof(LockWord), currently 32-bits, since this will end up
  // stored in the lock word bit-for-bit when object forwarding addresses are being calculated.
  struct BinSlot {
    explicit BinSlot(uint32_t lockword);
    BinSlot(Bin bin, uint32_t index);

    // The bin an object belongs to, i.e. regular, class/verified, class/initialized, etc.
    Bin GetBin() const;
    // The offset in bytes from the beginning of the bin. Aligned to object size.
    uint32_t GetIndex() const;
    // Pack into a single uint32_t, for storing into a lock word.
    uint32_t Uint32Value() const { return lockword_; }
    // Comparison operator for map support
    bool operator<(const BinSlot& other) const  { return lockword_ < other.lockword_; }

   private:
    // Must be the same size as LockWord, any larger and we would truncate the data.
    const uint32_t lockword_;
  };

  struct ImageInfo {
    ImageInfo();
    ImageInfo(ImageInfo&&) = default;

    // Create the image sections into the out sections variable, returns the size of the image
    // excluding the bitmap.
    size_t CreateImageSections(ImageSection* out_sections, bool app_image) const;

    size_t GetStubOffset(StubType stub_type) const {
      DCHECK_LT(static_cast<size_t>(stub_type), kNumberOfStubTypes);
      return stub_offsets_[static_cast<size_t>(stub_type)];
    }

    void SetStubOffset(StubType stub_type, size_t offset) {
      DCHECK_LT(static_cast<size_t>(stub_type), kNumberOfStubTypes);
      stub_offsets_[static_cast<size_t>(stub_type)] = offset;
    }

    size_t GetBinSlotOffset(Bin bin) const {
      DCHECK_LT(static_cast<size_t>(bin), kNumberOfBins);
      return bin_slot_offsets_[static_cast<size_t>(bin)];
    }

    void IncrementBinSlotSize(Bin bin, size_t size_to_add) {
      DCHECK_LT(static_cast<size_t>(bin), kNumberOfBins);
      bin_slot_sizes_[static_cast<size_t>(bin)] += size_to_add;
    }

    size_t GetBinSlotSize(Bin bin) const {
      DCHECK_LT(static_cast<size_t>(bin), kNumberOfBins);
      return bin_slot_sizes_[static_cast<size_t>(bin)];
    }

    void IncrementBinSlotCount(Bin bin, size_t count_to_add) {
      DCHECK_LT(static_cast<size_t>(bin), kNumberOfBins);
      bin_slot_count_[static_cast<size_t>(bin)] += count_to_add;
    }

    // Calculate the sum total of the bin slot sizes in [0, up_to). Defaults to all bins.
    size_t GetBinSizeSum(Bin up_to) const;

    std::unique_ptr<MemMap> image_;  // Memory mapped for generating the image.

    // Target begin of this image. Notes: It is not valid to write here, this is the address
    // of the target image, not necessarily where image_ is mapped. The address is only valid
    // after layouting (otherwise null).
    uint8_t* image_begin_ = nullptr;

    // Offset to the free space in image_, initially size of image header.
    size_t image_end_ = RoundUp(sizeof(ImageHeader), kObjectAlignment);
    uint32_t image_roots_address_ = 0;  // The image roots address in the image.
    size_t image_offset_ = 0;  // Offset of this image from the start of the first image.

    // Image size is the *address space* covered by this image. As the live bitmap is aligned
    // to the page size, the live bitmap will cover more address space than necessary. But live
    // bitmaps may not overlap, so an image has a "shadow," which is accounted for in the size.
    // The next image may only start at image_begin_ + image_size_ (which is guaranteed to be
    // page-aligned).
    size_t image_size_ = 0;

    // Oat data.
    // Offset of the oat file for this image from start of oat files. This is
    // valid when the previous oat file has been written.
    size_t oat_offset_ = 0;
    // Layout of the loaded ELF file containing the oat file, valid after UpdateOatFileLayout().
    const uint8_t* oat_file_begin_ = nullptr;
    size_t oat_loaded_size_ = 0;
    const uint8_t* oat_data_begin_ = nullptr;
    size_t oat_size_ = 0;  // Size of the corresponding oat data.
    // The oat header checksum, valid after UpdateOatFileHeader().
    uint32_t oat_checksum_ = 0u;

    // Image bitmap which lets us know where the objects inside of the image reside.
    std::unique_ptr<gc::accounting::ContinuousSpaceBitmap> image_bitmap_;

    // The start offsets of the dex cache arrays.
    SafeMap<const DexFile*, size_t> dex_cache_array_starts_;

    // Offset from oat_data_begin_ to the stubs.
    uint32_t stub_offsets_[kNumberOfStubTypes] = {};

    // Bin slot tracking for dirty object packing.
    size_t bin_slot_sizes_[kNumberOfBins] = {};  // Number of bytes in a bin.
    size_t bin_slot_offsets_[kNumberOfBins] = {};  // Number of bytes in previous bins.
    size_t bin_slot_count_[kNumberOfBins] = {};  // Number of objects in a bin.

    // Cached size of the intern table for when we allocate memory.
    size_t intern_table_bytes_ = 0;

    // Number of image class table bytes.
    size_t class_table_bytes_ = 0;

    // Number of object fixup bytes.
    size_t object_fixup_bytes_ = 0;

    // Number of pointer fixup bytes.
    size_t pointer_fixup_bytes_ = 0;

    // Intern table associated with this image for serialization.
    std::unique_ptr<InternTable> intern_table_;

    // Class table associated with this image for serialization.
    std::unique_ptr<ClassTable> class_table_;
  };

  // We use the lock word to store the offset of the object in the image.
  void AssignImageOffset(mirror::Object* object, BinSlot bin_slot)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SetImageOffset(mirror::Object* object, size_t offset)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsImageOffsetAssigned(mirror::Object* object) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  size_t GetImageOffset(mirror::Object* object) const REQUIRES_SHARED(Locks::mutator_lock_);
  void UpdateImageOffset(mirror::Object* obj, uintptr_t offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void PrepareDexCacheArraySlots() REQUIRES_SHARED(Locks::mutator_lock_);
  void AssignImageBinSlot(mirror::Object* object, size_t oat_index)
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* TryAssignBinSlot(WorkStack& work_stack, mirror::Object* obj, size_t oat_index)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SetImageBinSlot(mirror::Object* object, BinSlot bin_slot)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsImageBinSlotAssigned(mirror::Object* object) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  BinSlot GetImageBinSlot(mirror::Object* object) const REQUIRES_SHARED(Locks::mutator_lock_);

  void AddDexCacheArrayRelocation(void* array, size_t offset, ObjPtr<mirror::DexCache> dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void AddMethodPointerArray(mirror::PointerArray* arr) REQUIRES_SHARED(Locks::mutator_lock_);

  static void* GetImageAddressCallback(void* writer, mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<ImageWriter*>(writer)->GetImageAddress(obj);
  }

  mirror::Object* GetLocalAddress(mirror::Object* object) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t offset = GetImageOffset(object);
    size_t oat_index = GetOatIndex(object);
    const ImageInfo& image_info = GetImageInfo(oat_index);
    uint8_t* dst = image_info.image_->Begin() + offset;
    return reinterpret_cast<mirror::Object*>(dst);
  }

  // Returns the address in the boot image if we are compiling the app image.
  const uint8_t* GetOatAddress(StubType type) const;

  const uint8_t* GetOatAddressForOffset(uint32_t offset, const ImageInfo& image_info) const {
    // With Quick, code is within the OatFile, as there are all in one
    // .o ELF object. But interpret it as signed.
    DCHECK_LE(static_cast<int32_t>(offset), static_cast<int32_t>(image_info.oat_size_));
    DCHECK(image_info.oat_data_begin_ != nullptr);
    return offset == 0u ? nullptr : image_info.oat_data_begin_ + static_cast<int32_t>(offset);
  }

  // Returns true if the class was in the original requested image classes list.
  bool KeepClass(ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Debug aid that list of requested image classes.
  void DumpImageClasses();

  // Preinitializes some otherwise lazy fields (such as Class name) to avoid runtime image dirtying.
  void ComputeLazyFieldsForImageClasses()
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit all class loaders.
  void VisitClassLoaders(ClassLoaderVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Remove unwanted classes from various roots.
  void PruneNonImageClasses() REQUIRES_SHARED(Locks::mutator_lock_);

  // Remove unwanted classes from the DexCache roots and preload deterministic DexCache contents.
  void PruneAndPreloadDexCache(ObjPtr<mirror::DexCache> dex_cache,
                               ObjPtr<mirror::ClassLoader> class_loader)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::classlinker_classes_lock_);

  // Verify unwanted classes removed.
  void CheckNonImageClassesRemoved() REQUIRES_SHARED(Locks::mutator_lock_);

  // Lays out where the image objects will be at runtime.
  void CalculateNewObjectOffsets()
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ProcessWorkStack(WorkStack* work_stack)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CreateHeader(size_t oat_index)
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::ObjectArray<mirror::Object>* CreateImageRoots(size_t oat_index) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CalculateObjectBinSlots(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void UnbinObjectsIntoOffset(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Creates the contiguous image in memory and adjusts pointers.
  void CopyAndFixupNativeData(size_t oat_index) REQUIRES_SHARED(Locks::mutator_lock_);
  void CopyAndFixupObjects() REQUIRES_SHARED(Locks::mutator_lock_);
  void CopyAndFixupObject(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);
  void CopyAndFixupMethod(ArtMethod* orig, ArtMethod* copy, const ImageInfo& image_info)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CopyAndFixupImTable(ImTable* orig, ImTable* copy) REQUIRES_SHARED(Locks::mutator_lock_);
  void CopyAndFixupImtConflictTable(ImtConflictTable* orig, ImtConflictTable* copy)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FixupClass(mirror::Class* orig, mirror::Class* copy)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FixupObject(mirror::Object* orig, mirror::Object* copy)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FixupDexCache(mirror::DexCache* orig_dex_cache, mirror::DexCache* copy_dex_cache)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FixupPointerArray(mirror::Object* dst,
                         mirror::PointerArray* arr,
                         mirror::Class* klass,
                         Bin array_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get quick code for non-resolution/imt_conflict/abstract method.
  const uint8_t* GetQuickCode(ArtMethod* method,
                              const ImageInfo& image_info,
                              bool* quick_is_interpreted)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if a method is likely to be dirtied at runtime.
  bool WillMethodBeDirty(ArtMethod* m) const REQUIRES_SHARED(Locks::mutator_lock_);

  // Assign the offset for an ArtMethod.
  void AssignMethodOffset(ArtMethod* method,
                          NativeObjectRelocationType type,
                          size_t oat_index)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if imt was newly inserted.
  bool TryAssignImTableOffset(ImTable* imt, size_t oat_index) REQUIRES_SHARED(Locks::mutator_lock_);

  // Assign the offset for an IMT conflict table. Does nothing if the table already has a native
  // relocation.
  void TryAssignConflictTableOffset(ImtConflictTable* table, size_t oat_index)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if klass is loaded by the boot class loader but not in the boot image.
  bool IsBootClassLoaderNonImageClass(mirror::Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if klass depends on a boot class loader non image class. We want to prune these
  // classes since we do not want any boot class loader classes in the image. This means that
  // we also cannot have any classes which refer to these boot class loader non image classes.
  // PruneAppImageClass also prunes if klass depends on a non-image class according to the compiler
  // driver.
  bool PruneAppImageClass(ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // early_exit is true if we had a cyclic dependency anywhere down the chain.
  bool PruneAppImageClassInternal(ObjPtr<mirror::Class> klass,
                                  bool* early_exit,
                                  std::unordered_set<mirror::Object*>* visited)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsMultiImage() const {
    return image_infos_.size() > 1;
  }

  static Bin BinTypeForNativeRelocationType(NativeObjectRelocationType type);

  uintptr_t NativeOffsetInImage(void* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Location of where the object will be when the image is loaded at runtime.
  template <typename T>
  T* NativeLocationInImage(T* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Location of where the temporary copy of the object currently is.
  template <typename T>
  T* NativeCopyLocation(T* obj, mirror::DexCache* dex_cache) REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true of obj is inside of the boot image space. This may only return true if we are
  // compiling an app image.
  bool IsInBootImage(const void* obj) const;

  // Return true if ptr is within the boot oat file.
  bool IsInBootOatFile(const void* ptr) const;

  // Get the index of the oat file associated with the object.
  size_t GetOatIndex(mirror::Object* object) const REQUIRES_SHARED(Locks::mutator_lock_);

  // The oat index for shared data in multi-image and all data in single-image compilation.
  size_t GetDefaultOatIndex() const {
    return 0u;
  }

  ImageInfo& GetImageInfo(size_t oat_index) {
    return image_infos_[oat_index];
  }

  const ImageInfo& GetImageInfo(size_t oat_index) const {
    return image_infos_[oat_index];
  }

  // Find an already strong interned string in the other images or in the boot image. Used to
  // remove duplicates in the multi image and app image case.
  mirror::String* FindInternedString(mirror::String* string) REQUIRES_SHARED(Locks::mutator_lock_);

  // Return true if there already exists a native allocation for an object.
  bool NativeRelocationAssigned(void* ptr) const;

  void CopyReference(mirror::HeapReference<mirror::Object>* dest, ObjPtr<mirror::Object> src)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CopyReference(mirror::CompressedReference<mirror::Object>* dest, ObjPtr<mirror::Object> src)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CopyAndFixupPointer(void** target, void* value);

  const CompilerDriver& compiler_driver_;

  // Beginning target image address for the first image.
  uint8_t* global_image_begin_;

  // Offset from image_begin_ to where the first object is in image_.
  size_t image_objects_offset_begin_;

  // Pointer arrays that need to be updated. Since these are only some int and long arrays, we need
  // to keep track. These include vtable arrays, iftable arrays, and dex caches.
  std::unordered_map<mirror::PointerArray*, Bin> pointer_arrays_;

  // Saved hash codes. We use these to restore lockwords which were temporarily used to have
  // forwarding addresses as well as copying over hash codes.
  std::unordered_map<mirror::Object*, uint32_t> saved_hashcode_map_;

  // Oat index map for objects.
  std::unordered_map<mirror::Object*, uint32_t> oat_index_map_;

  // Boolean flags.
  const bool compile_pic_;
  const bool compile_app_image_;

  // Size of pointers on the target architecture.
  PointerSize target_ptr_size_;

  // Image data indexed by the oat file index.
  dchecked_vector<ImageInfo> image_infos_;

  // ArtField, ArtMethod relocating map. These are allocated as array of structs but we want to
  // have one entry per art field for convenience. ArtFields are placed right after the end of the
  // image objects (aka sum of bin_slot_sizes_). ArtMethods are placed right after the ArtFields.
  struct NativeObjectRelocation {
    size_t oat_index;
    uintptr_t offset;
    NativeObjectRelocationType type;

    bool IsArtMethodRelocation() const {
      return type == NativeObjectRelocationType::kArtMethodClean ||
          type == NativeObjectRelocationType::kArtMethodDirty ||
          type == NativeObjectRelocationType::kRuntimeMethod;
    }
  };
  std::unordered_map<void*, NativeObjectRelocation> native_object_relocations_;

  // Runtime ArtMethods which aren't reachable from any Class but need to be copied into the image.
  ArtMethod* image_methods_[ImageHeader::kImageMethodsCount];

  // Counters for measurements, used for logging only.
  uint64_t dirty_methods_;
  uint64_t clean_methods_;

  // Prune class memoization table to speed up ContainsBootClassLoaderNonImageClass.
  std::unordered_map<mirror::Class*, bool> prune_class_memo_;

  // Class loaders with a class table to write out. There should only be one class loader because
  // dex2oat loads the dex files to be compiled into a single class loader. For the boot image,
  // null is a valid entry.
  std::unordered_set<mirror::ClassLoader*> class_loaders_;

  // Which mode the image is stored as, see image.h
  const ImageHeader::StorageMode image_storage_mode_;

  // The file names of oat files.
  const std::vector<const char*>& oat_filenames_;

  // Map of dex files to the indexes of oat files that they were compiled into.
  const std::unordered_map<const DexFile*, size_t>& dex_file_oat_index_map_;

  // Set of objects known to be dirty in the image. Can be nullptr if there are none.
  const std::unordered_set<std::string>* dirty_image_objects_;

  class ComputeLazyFieldsForClassesVisitor;
  class FixupClassVisitor;
  class FixupRootVisitor;
  class FixupVisitor;
  class GetRootsVisitor;
  class ImageAddressVisitorForDexCacheArray;
  class NativeLocationVisitor;
  class PruneClassesVisitor;
  class PruneClassLoaderClassesVisitor;
  class RegisterBootClassPathClassesVisitor;
  class VisitReferencesVisitor;
  class PruneObjectReferenceVisitor;

  DISALLOW_COPY_AND_ASSIGN(ImageWriter);
};

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_IMAGE_WRITER_H_
