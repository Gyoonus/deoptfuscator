/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "class_table-inl.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "class_linker-inl.h"
#include "common_runtime_test.h"
#include "dex/dex_file.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "obj_ptr.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace mirror {

class CollectRootVisitor {
 public:
  CollectRootVisitor() {}

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(GcRoot<MirrorType>& root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root.IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<MirrorType>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  void VisitRoot(GcRoot<MirrorType>& root) const REQUIRES_SHARED(Locks::mutator_lock_) {
    VisitRoot(root.AddressWithoutBarrier());
  }

  template <class MirrorType>
  void VisitRoot(mirror::CompressedReference<MirrorType>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    roots_.insert(root->AsMirrorPtr());
  }

  mutable std::set<mirror::Object*> roots_;
};


class ClassTableTest : public CommonRuntimeTest {};

TEST_F(ClassTableTest, ClassTable) {
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("XandY");
  VariableSizedHandleScope hs(soa.Self());
  Handle<ClassLoader> class_loader(hs.NewHandle(soa.Decode<ClassLoader>(jclass_loader)));
  const char* descriptor_x = "LX;";
  const char* descriptor_y = "LY;";
  Handle<mirror::Class> h_X(
      hs.NewHandle(class_linker_->FindClass(soa.Self(), descriptor_x, class_loader)));
  Handle<mirror::Class> h_Y(
      hs.NewHandle(class_linker_->FindClass(soa.Self(), descriptor_y, class_loader)));
  Handle<mirror::Object> obj_X = hs.NewHandle(h_X->AllocObject(soa.Self()));
  ASSERT_TRUE(obj_X != nullptr);
  ClassTable table;
  EXPECT_EQ(table.NumZygoteClasses(class_loader.Get()), 0u);
  EXPECT_EQ(table.NumNonZygoteClasses(class_loader.Get()), 0u);

  // Add h_X to the class table.
  table.Insert(h_X.Get());
  EXPECT_EQ(table.LookupByDescriptor(h_X.Get()), h_X.Get());
  EXPECT_EQ(table.Lookup(descriptor_x, ComputeModifiedUtf8Hash(descriptor_x)), h_X.Get());
  EXPECT_EQ(table.Lookup("NOT_THERE", ComputeModifiedUtf8Hash("NOT_THERE")), nullptr);
  EXPECT_EQ(table.NumZygoteClasses(class_loader.Get()), 0u);
  EXPECT_EQ(table.NumNonZygoteClasses(class_loader.Get()), 1u);

  // Create the zygote snapshot and ensure the accounting is correct.
  table.FreezeSnapshot();
  EXPECT_EQ(table.NumZygoteClasses(class_loader.Get()), 1u);
  EXPECT_EQ(table.NumNonZygoteClasses(class_loader.Get()), 0u);

  // Test inserting and related lookup functions.
  EXPECT_EQ(table.LookupByDescriptor(h_Y.Get()), nullptr);
  EXPECT_FALSE(table.Contains(h_Y.Get()));
  table.Insert(h_Y.Get());
  EXPECT_EQ(table.LookupByDescriptor(h_X.Get()), h_X.Get());
  EXPECT_EQ(table.LookupByDescriptor(h_Y.Get()), h_Y.Get());
  EXPECT_TRUE(table.Contains(h_X.Get()));
  EXPECT_TRUE(table.Contains(h_Y.Get()));

  EXPECT_EQ(table.NumZygoteClasses(class_loader.Get()), 1u);
  EXPECT_EQ(table.NumNonZygoteClasses(class_loader.Get()), 1u);

  // Test adding / clearing strong roots.
  EXPECT_TRUE(table.InsertStrongRoot(obj_X.Get()));
  EXPECT_FALSE(table.InsertStrongRoot(obj_X.Get()));
  table.ClearStrongRoots();
  EXPECT_TRUE(table.InsertStrongRoot(obj_X.Get()));

  // Collect all the roots and make sure there is nothing missing.
  CollectRootVisitor roots;
  table.VisitRoots(roots);
  EXPECT_TRUE(roots.roots_.find(h_X.Get()) != roots.roots_.end());
  EXPECT_TRUE(roots.roots_.find(h_Y.Get()) != roots.roots_.end());
  EXPECT_TRUE(roots.roots_.find(obj_X.Get()) != roots.roots_.end());

  // Checks that vising only classes works.
  std::set<mirror::Class*> classes;
  table.Visit([&classes](ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    classes.insert(klass.Ptr());
    return true;
  });
  EXPECT_TRUE(classes.find(h_X.Get()) != classes.end());
  EXPECT_TRUE(classes.find(h_Y.Get()) != classes.end());
  EXPECT_EQ(classes.size(), 2u);
  classes.clear();
  table.Visit([&classes](ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    classes.insert(klass.Ptr());
    // Return false to exit the Visit early.
    return false;
  });
  EXPECT_EQ(classes.size(), 1u);

  // Test remove.
  table.Remove(descriptor_x);
  EXPECT_FALSE(table.Contains(h_X.Get()));

  // Test that WriteToMemory and ReadFromMemory work.
  table.Insert(h_X.Get());
  const size_t count = table.WriteToMemory(nullptr);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[count]());
  ASSERT_EQ(table.WriteToMemory(&buffer[0]), count);
  ClassTable table2;
  size_t count2 = table2.ReadFromMemory(&buffer[0]);
  EXPECT_EQ(count, count2);
  // Strong roots are not serialized, only classes.
  EXPECT_TRUE(table2.Contains(h_X.Get()));
  EXPECT_TRUE(table2.Contains(h_Y.Get()));

  // TODO: Add tests for UpdateClass, InsertOatFile.
}

}  // namespace mirror
}  // namespace art
