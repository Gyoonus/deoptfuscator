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

#ifndef ART_PATCHOAT_PATCHOAT_H_
#define ART_PATCHOAT_PATCHOAT_H_

#include "arch/instruction_set.h"
#include "base/enums.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/os.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "gc/accounting/space_bitmap.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "runtime.h"

namespace art {

class ArtMethod;
class ImageHeader;
class OatHeader;

namespace mirror {
class Object;
class PointerArray;
class Reference;
class Class;
}  // namespace mirror

class PatchOat {
 public:
  // Relocates the provided image by the specified offset. If output_image_directory is non-empty,
  // outputs the relocated image into that directory. If output_image_relocation_directory is
  // non-empty, outputs image relocation files (see GeneratePatch) into that directory.
  static bool Patch(const std::string& image_location,
                    off_t delta,
                    const std::string& output_image_directory,
                    const std::string& output_image_relocation_directory,
                    InstructionSet isa,
                    TimingLogger* timings);
  static bool Verify(const std::string& image_location,
                     const std::string& output_image_filename,
                     InstructionSet isa,
                     TimingLogger* timings);

  // Generates a patch which can be used to efficiently relocate the original file or to check that
  // a relocated file matches the original. The patch is generated from the difference of the
  // |original| and the already |relocated| image, and written to |output| in the form of unsigned
  // LEB128 for each relocation position.
  static bool GeneratePatch(const MemMap& original,
                            const MemMap& relocated,
                            std::vector<uint8_t>* output,
                            std::string* error_msg);

  ~PatchOat() {}
  PatchOat(PatchOat&&) = default;

 private:
  // All pointers are only borrowed.
  PatchOat(InstructionSet isa, MemMap* image,
           gc::accounting::ContinuousSpaceBitmap* bitmap, MemMap* heap, off_t delta,
           std::map<gc::space::ImageSpace*, std::unique_ptr<MemMap>>* map, TimingLogger* timings)
      : image_(image), bitmap_(bitmap), heap_(heap),
        delta_(delta), isa_(isa), space_map_(map), timings_(timings) {}

  // Was the .art image at image_path made with --compile-pic ?
  static bool IsImagePic(const ImageHeader& image_header, const std::string& image_path);

  enum MaybePic {
      NOT_PIC,            // Code not pic. Patch as usual.
      PIC,                // Code was pic. Create symlink; skip OAT patching.
      ERROR_OAT_FILE,     // Failed to symlink oat file
      ERROR_FIRST = ERROR_OAT_FILE,
  };

  // Was the .oat image at oat_in made with --compile-pic ?
  static MaybePic IsOatPic(const ElfFile* oat_in);

  static bool CreateVdexAndOatSymlinks(const std::string& input_image_filename,
                                       const std::string& output_image_filename);


  void VisitObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FixupMethod(ArtMethod* object, ArtMethod* copy)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool PatchImage(bool primary_image) REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchArtFields(const ImageHeader* image_header) REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchArtMethods(const ImageHeader* image_header) REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchImTables(const ImageHeader* image_header) REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchImtConflictTables(const ImageHeader* image_header)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchInternedStrings(const ImageHeader* image_header)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchClassTable(const ImageHeader* image_header)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void PatchDexFileArrays(mirror::ObjectArray<mirror::Object>* img_roots)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool WriteImage(File* out);

  template <typename T>
  T* RelocatedCopyOf(T* obj) const {
    if (obj == nullptr) {
      return nullptr;
    }
    DCHECK_GT(reinterpret_cast<uintptr_t>(obj), reinterpret_cast<uintptr_t>(heap_->Begin()));
    DCHECK_LT(reinterpret_cast<uintptr_t>(obj), reinterpret_cast<uintptr_t>(heap_->End()));
    uintptr_t heap_off =
        reinterpret_cast<uintptr_t>(obj) - reinterpret_cast<uintptr_t>(heap_->Begin());
    DCHECK_LT(heap_off, image_->Size());
    return reinterpret_cast<T*>(image_->Begin() + heap_off);
  }

  template <typename T>
  T* RelocatedCopyOfFollowImages(T* obj) const {
    if (obj == nullptr) {
      return nullptr;
    }
    // Find ImageSpace this belongs to.
    auto image_spaces = Runtime::Current()->GetHeap()->GetBootImageSpaces();
    for (gc::space::ImageSpace* image_space : image_spaces) {
      if (image_space->Contains(obj)) {
        uintptr_t heap_off = reinterpret_cast<uintptr_t>(obj) -
                             reinterpret_cast<uintptr_t>(image_space->GetMemMap()->Begin());
        return reinterpret_cast<T*>(space_map_->find(image_space)->second->Begin() + heap_off);
      }
    }
    LOG(FATAL) << "Did not find object in boot image space " << obj;
    UNREACHABLE();
  }

  template <typename T>
  T* RelocatedAddressOfPointer(T* obj) const {
    if (obj == nullptr) {
      return obj;
    }
    auto ret = reinterpret_cast<uintptr_t>(obj) + delta_;
    // Trim off high bits in case negative relocation with 64 bit patchoat.
    if (Is32BitISA()) {
      ret = static_cast<uintptr_t>(static_cast<uint32_t>(ret));
    }
    return reinterpret_cast<T*>(ret);
  }

  bool Is32BitISA() const {
    return InstructionSetPointerSize(isa_) == PointerSize::k32;
  }

  // Walks through the old image and patches the mmap'd copy of it to the new offset. It does not
  // change the heap.
  class PatchVisitor {
   public:
    PatchVisitor(PatchOat* patcher, mirror::Object* copy) : patcher_(patcher), copy_(copy) {}
    ~PatchVisitor() {}
    void operator() (ObjPtr<mirror::Object> obj, MemberOffset off, bool b) const
        REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
    // For reference classes.
    void operator() (ObjPtr<mirror::Class> cls, ObjPtr<mirror::Reference>  ref) const
        REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
    // TODO: Consider using these for updating native class roots?
    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
        const {}
    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

   private:
    PatchOat* const patcher_;
    mirror::Object* const copy_;
  };

  // A mmap of the image we are patching. This is modified.
  const MemMap* const image_;
  // The bitmap over the image within the heap we are patching. This is not modified.
  gc::accounting::ContinuousSpaceBitmap* const bitmap_;
  // The heap we are patching. This is not modified.
  const MemMap* const heap_;
  // The amount we are changing the offset by.
  const off_t delta_;
  // Active instruction set, used to know the entrypoint size.
  const InstructionSet isa_;

  const std::map<gc::space::ImageSpace*, std::unique_ptr<MemMap>>* space_map_;

  TimingLogger* timings_;

  class FixupRootVisitor;
  class RelocatedPointerVisitor;
  class PatchOatArtFieldVisitor;
  class PatchOatArtMethodVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PatchOat);
};

}  // namespace art
#endif  // ART_PATCHOAT_PATCHOAT_H_
