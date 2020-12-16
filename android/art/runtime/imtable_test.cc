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

#include "imtable-inl.h"

#include <memory>
#include <string>

#include "jni.h"

#include "base/mutex.h"
#include "class_linker.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "mirror/accessible_object.h"
#include "mirror/class.h"
#include "mirror/class_loader.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

class ImTableTest : public CommonRuntimeTest {
 public:
  std::pair<mirror::Class*, mirror::Class*> LoadClasses(const std::string& class_name)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    jobject jclass_loader_a = LoadDex("IMTA");
    CHECK(jclass_loader_a != nullptr);
    jobject jclass_loader_b = LoadDex("IMTB");
    CHECK(jclass_loader_b != nullptr);

    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Thread* self = Thread::Current();

    StackHandleScope<3> hs(self);
    MutableHandle<mirror::ClassLoader> h_class_loader = hs.NewHandle<mirror::ClassLoader>(nullptr);

    // A.
    h_class_loader.Assign(
        ObjPtr<mirror::ClassLoader>::DownCast(self->DecodeJObject(jclass_loader_a)));
    Handle<mirror::Class> h_class_a(
          hs.NewHandle(class_linker->FindClass(self, class_name.c_str(), h_class_loader)));
    if (h_class_a == nullptr) {
      LOG(ERROR) << self->GetException()->Dump();
      CHECK(false) << "h_class_a == nullptr";
    }

    // B.
    h_class_loader.Assign(
        ObjPtr<mirror::ClassLoader>::DownCast(self->DecodeJObject(jclass_loader_b)));
    Handle<mirror::Class> h_class_b(
          hs.NewHandle(class_linker->FindClass(self, class_name.c_str(), h_class_loader)));
    if (h_class_b == nullptr) {
      LOG(ERROR) << self->GetException()->Dump();
      CHECK(false) << "h_class_b == nullptr";
    }

    return std::make_pair(h_class_a.Get(), h_class_b.Get());
  }

  std::pair<ArtMethod*, ArtMethod*> LoadMethods(const std::string& class_name,
                                                const std::string& method_name)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    std::pair<mirror::Class*, mirror::Class*> classes = LoadClasses(class_name);

    const PointerSize pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();

    ArtMethod* method_a =
        classes.first->FindDeclaredVirtualMethodByName(method_name, pointer_size);
    ArtMethod* method_b =
        classes.second->FindDeclaredVirtualMethodByName(method_name, pointer_size);

    return std::make_pair(method_a, method_b);
  }
};

TEST_F(ImTableTest, NewMethodBefore) {
  ScopedObjectAccess soa(Thread::Current());

  std::pair<ArtMethod*, ArtMethod*> methods = LoadMethods("LInterfaces$A;", "foo");
  CHECK_EQ(ImTable::GetImtIndex(methods.first), ImTable::GetImtIndex(methods.second));
}

TEST_F(ImTableTest, NewClassBefore) {
  ScopedObjectAccess soa(Thread::Current());

  std::pair<ArtMethod*, ArtMethod*> methods = LoadMethods("LInterfaces$Z;", "foo");
  CHECK_EQ(ImTable::GetImtIndex(methods.first), ImTable::GetImtIndex(methods.second));
}

}  // namespace art
