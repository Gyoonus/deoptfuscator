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

#include <type_traits>

#include "base/enums.h"
#include "class_linker-inl.h"
#include "common_runtime_test.h"
#include "gtest/gtest.h"
#include "handle.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

// Handles are value objects and should be trivially copyable.
static_assert(std::is_trivially_copyable<Handle<mirror::Object>>::value,
              "Handle should be trivially copyable");
static_assert(std::is_trivially_copyable<MutableHandle<mirror::Object>>::value,
              "MutableHandle should be trivially copyable");
static_assert(std::is_trivially_copyable<ScopedNullHandle<mirror::Object>>::value,
              "ScopedNullHandle should be trivially copyable");

class HandleScopeTest : public CommonRuntimeTest {};

// Test the offsets computed for members of HandleScope. Because of cross-compiling
// it is impossible the use OFFSETOF_MEMBER, so we do some reasonable computations ourselves. This
// test checks whether we do the right thing.
TEST_F(HandleScopeTest, Offsets) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  // As the members of HandleScope are private, we cannot use OFFSETOF_MEMBER
  // here. So do the inverse: set some data, and access it through pointers created from the offsets.
  StackHandleScope<0x1> hs0(soa.Self());
  static const size_t kNumReferences = 0x9ABC;
  StackHandleScope<kNumReferences> test_table(soa.Self());
  ObjPtr<mirror::Class> c = class_linker->FindSystemClass(soa.Self(), "Ljava/lang/Object;");
  test_table.SetReference(0, c.Ptr());

  uint8_t* table_base_ptr = reinterpret_cast<uint8_t*>(&test_table);

  {
    BaseHandleScope** link_ptr = reinterpret_cast<BaseHandleScope**>(table_base_ptr +
        HandleScope::LinkOffset(kRuntimePointerSize));
    EXPECT_EQ(*link_ptr, &hs0);
  }

  {
    uint32_t* num_ptr = reinterpret_cast<uint32_t*>(table_base_ptr +
        HandleScope::NumberOfReferencesOffset(kRuntimePointerSize));
    EXPECT_EQ(*num_ptr, static_cast<size_t>(kNumReferences));
  }

  {
    auto* ref_ptr = reinterpret_cast<StackReference<mirror::Object>*>(table_base_ptr +
        HandleScope::ReferencesOffset(kRuntimePointerSize));
    EXPECT_OBJ_PTR_EQ(ref_ptr->AsMirrorPtr(), c);
  }
}

class CollectVisitor {
 public:
  void VisitRootIfNonNull(StackReference<mirror::Object>* ref) {
    if (!ref->IsNull()) {
      visited.insert(ref);
    }
    ++total_visited;
  }

  std::set<StackReference<mirror::Object>*> visited;
  size_t total_visited = 0;  // including null.
};

// Test functionality of variable sized handle scopes.
TEST_F(HandleScopeTest, VariableSized) {
  ScopedObjectAccess soa(Thread::Current());
  VariableSizedHandleScope hs(soa.Self());
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> c =
      hs.NewHandle(class_linker->FindSystemClass(soa.Self(), "Ljava/lang/Object;"));
  // Test nested scopes.
  StackHandleScope<1> inner(soa.Self());
  inner.NewHandle(c->AllocObject(soa.Self()));
  // Add a bunch of handles and make sure callbacks work.
  static const size_t kNumHandles = 100;
  std::vector<Handle<mirror::Object>> handles;
  for (size_t i = 0; i < kNumHandles; ++i) {
    BaseHandleScope* base = &hs;
    ObjPtr<mirror::Object> o = c->AllocObject(soa.Self());
    handles.push_back(hs.NewHandle(o));
    EXPECT_OBJ_PTR_EQ(o, handles.back().Get());
    EXPECT_TRUE(hs.Contains(handles.back().GetReference()));
    EXPECT_TRUE(base->Contains(handles.back().GetReference()));
    EXPECT_EQ(hs.NumberOfReferences(), base->NumberOfReferences());
  }
  CollectVisitor visitor;
  BaseHandleScope* base = &hs;
  base->VisitRoots(visitor);
  EXPECT_LE(visitor.visited.size(), base->NumberOfReferences());
  EXPECT_EQ(visitor.total_visited, base->NumberOfReferences());
  for (StackReference<mirror::Object>* ref : visitor.visited) {
    EXPECT_TRUE(base->Contains(ref));
  }
}

}  // namespace art
