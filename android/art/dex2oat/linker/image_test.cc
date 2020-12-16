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

#include <string.h>
#include <vector>

#include "image_test.h"

#include "image.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace linker {

TEST_F(ImageTest, TestImageLayout) {
  std::vector<size_t> image_sizes;
  std::vector<size_t> image_sizes_extra;
  // Compile multi-image with ImageLayoutA being the last image.
  {
    CompilationHelper helper;
    Compile(ImageHeader::kStorageModeUncompressed, helper, "ImageLayoutA", {"LMyClass;"});
    image_sizes = helper.GetImageObjectSectionSizes();
  }
  TearDown();
  runtime_.reset();
  SetUp();
  // Compile multi-image with ImageLayoutB being the last image.
  {
    CompilationHelper helper;
    Compile(ImageHeader::kStorageModeUncompressed, helper, "ImageLayoutB", {"LMyClass;"});
    image_sizes_extra = helper.GetImageObjectSectionSizes();
  }
  // Make sure that the new stuff in the clinit in ImageLayoutB is in the last image and not in the
  // first two images.
  ASSERT_EQ(image_sizes.size(), image_sizes.size());
  // Sizes of the object sections should be the same for all but the last image.
  for (size_t i = 0; i < image_sizes.size() - 1; ++i) {
    EXPECT_EQ(image_sizes[i], image_sizes_extra[i]);
  }
  // Last image should be larger since it has a hash map and a string.
  EXPECT_LT(image_sizes.back(), image_sizes_extra.back());
}

TEST_F(ImageTest, ImageHeaderIsValid) {
    uint32_t image_begin = ART_BASE_ADDRESS;
    uint32_t image_size_ = 16 * KB;
    uint32_t image_roots = ART_BASE_ADDRESS + (1 * KB);
    uint32_t oat_checksum = 0;
    uint32_t oat_file_begin = ART_BASE_ADDRESS + (4 * KB);  // page aligned
    uint32_t oat_data_begin = ART_BASE_ADDRESS + (8 * KB);  // page aligned
    uint32_t oat_data_end = ART_BASE_ADDRESS + (9 * KB);
    uint32_t oat_file_end = ART_BASE_ADDRESS + (10 * KB);
    ImageSection sections[ImageHeader::kSectionCount];
    ImageHeader image_header(image_begin,
                             image_size_,
                             sections,
                             image_roots,
                             oat_checksum,
                             oat_file_begin,
                             oat_data_begin,
                             oat_data_end,
                             oat_file_end,
                             /*boot_image_begin*/0U,
                             /*boot_image_size*/0U,
                             /*boot_oat_begin*/0U,
                             /*boot_oat_size_*/0U,
                             sizeof(void*),
                             /*compile_pic*/false,
                             /*is_pic*/false,
                             ImageHeader::kDefaultStorageMode,
                             /*data_size*/0u);
    ASSERT_TRUE(image_header.IsValid());
    ASSERT_TRUE(!image_header.IsAppImage());

    char* magic = const_cast<char*>(image_header.GetMagic());
    strcpy(magic, "");  // bad magic
    ASSERT_FALSE(image_header.IsValid());
    strcpy(magic, "art\n000");  // bad version
    ASSERT_FALSE(image_header.IsValid());
}

// Test that pointer to quick code is the same in
// a default method of an interface and in a copied method
// of a class which implements the interface. This should be true
// only if the copied method and the origin method are located in the
// same oat file.
TEST_F(ImageTest, TestDefaultMethods) {
  CompilationHelper helper;
  Compile(ImageHeader::kStorageModeUncompressed,
      helper,
      "DefaultMethods",
      {"LIface;", "LImpl;", "LIterableBase;"});

  PointerSize pointer_size = class_linker_->GetImagePointerSize();
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  // Test the pointer to quick code is the same in origin method
  // and in the copied method form the same oat file.
  mirror::Class* iface_klass = class_linker_->LookupClass(
      self, "LIface;", ObjPtr<mirror::ClassLoader>());
  ASSERT_NE(nullptr, iface_klass);
  ArtMethod* origin = iface_klass->FindInterfaceMethod("defaultMethod", "()V", pointer_size);
  ASSERT_NE(nullptr, origin);
  ASSERT_TRUE(origin->GetDeclaringClass() == iface_klass);
  const void* code = origin->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  // The origin method should have a pointer to quick code
  ASSERT_NE(nullptr, code);
  ASSERT_FALSE(class_linker_->IsQuickToInterpreterBridge(code));
  mirror::Class* impl_klass = class_linker_->LookupClass(
      self, "LImpl;", ObjPtr<mirror::ClassLoader>());
  ASSERT_NE(nullptr, impl_klass);
  ArtMethod* copied = FindCopiedMethod(origin, impl_klass);
  ASSERT_NE(nullptr, copied);
  // the copied method should have pointer to the same quick code as the origin method
  ASSERT_EQ(code, copied->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size));

  // Test the origin method has pointer to quick code
  // but the copied method has pointer to interpreter
  // because these methods are in different oat files.
  mirror::Class* iterable_klass = class_linker_->LookupClass(
      self, "Ljava/lang/Iterable;", ObjPtr<mirror::ClassLoader>());
  ASSERT_NE(nullptr, iterable_klass);
  origin = iterable_klass->FindClassMethod(
      "forEach", "(Ljava/util/function/Consumer;)V", pointer_size);
  ASSERT_NE(nullptr, origin);
  ASSERT_FALSE(origin->IsDirect());
  ASSERT_TRUE(origin->GetDeclaringClass() == iterable_klass);
  code = origin->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  // the origin method should have a pointer to quick code
  ASSERT_NE(nullptr, code);
  ASSERT_FALSE(class_linker_->IsQuickToInterpreterBridge(code));
  mirror::Class* iterablebase_klass = class_linker_->LookupClass(
      self, "LIterableBase;", ObjPtr<mirror::ClassLoader>());
  ASSERT_NE(nullptr, iterablebase_klass);
  copied = FindCopiedMethod(origin, iterablebase_klass);
  ASSERT_NE(nullptr, copied);
  code = copied->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  // the copied method should have a pointer to interpreter
  ASSERT_TRUE(class_linker_->IsQuickToInterpreterBridge(code));
}

}  // namespace linker
}  // namespace art
