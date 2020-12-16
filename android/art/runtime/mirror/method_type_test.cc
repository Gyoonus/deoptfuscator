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

#include "method_type.h"

#include <string>
#include <vector>

#include "class-inl.h"
#include "class_linker-inl.h"
#include "class_loader.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "object_array-inl.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace mirror {

class MethodTypeTest : public CommonRuntimeTest {};

static std::string FullyQualifiedType(const std::string& shorthand) {
  return "Ljava/lang/" + shorthand + ";";
}

static mirror::MethodType* CreateMethodType(const std::string& return_type,
                                            const std::vector<std::string>& param_types) {
  CHECK_LT(param_types.size(), 3u);

  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  Thread* const self = Thread::Current();

  ScopedObjectAccess soa(self);
  StackHandleScope<5> hs(soa.Self());

  Handle<mirror::ClassLoader> boot_class_loader = hs.NewHandle<mirror::ClassLoader>(nullptr);

  Handle<mirror::Class> return_clazz = hs.NewHandle(class_linker->FindClass(
          soa.Self(), FullyQualifiedType(return_type).c_str(), boot_class_loader));
  CHECK(return_clazz != nullptr);

  ObjPtr<mirror::Class> class_type = mirror::Class::GetJavaLangClass();
  mirror::Class* class_array_type = class_linker->FindArrayClass(self, &class_type);
  Handle<mirror::ObjectArray<mirror::Class>> param_classes = hs.NewHandle(
      mirror::ObjectArray<mirror::Class>::Alloc(self, class_array_type, param_types.size()));

  for (uint32_t i = 0; i < param_types.size(); ++i) {
    Handle<mirror::Class> param = hs.NewHandle(class_linker->FindClass(
        soa.Self(), FullyQualifiedType(param_types[i]).c_str(), boot_class_loader));
    param_classes->Set(i, param.Get());
  }

  return mirror::MethodType::Create(self, return_clazz, param_classes);
}


TEST_F(MethodTypeTest, IsExactMatch) {
  ScopedObjectAccess soa(Thread::Current());
  {
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::MethodType> mt1 = hs.NewHandle(CreateMethodType("String", { "Integer" }));
    Handle<mirror::MethodType> mt2 = hs.NewHandle(CreateMethodType("String", { "Integer" }));
    ASSERT_TRUE(mt1->IsExactMatch(mt2.Get()));
  }

  // Mismatched return type.
  {
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::MethodType> mt1 = hs.NewHandle(CreateMethodType("String", { "Integer" }));
    Handle<mirror::MethodType> mt2 = hs.NewHandle(CreateMethodType("Integer", { "Integer" }));
    ASSERT_FALSE(mt1->IsExactMatch(mt2.Get()));
  }

  // Mismatched param types.
  {
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::MethodType> mt1 = hs.NewHandle(CreateMethodType("String", { "Integer" }));
    Handle<mirror::MethodType> mt2 = hs.NewHandle(CreateMethodType("String", { "String" }));
    ASSERT_FALSE(mt1->IsExactMatch(mt2.Get()));
  }

  // Wrong number of param types.
  {
    StackHandleScope<2> hs(soa.Self());
    Handle<mirror::MethodType> mt1 = hs.NewHandle(
        CreateMethodType("String", { "String", "String" }));
    Handle<mirror::MethodType> mt2 = hs.NewHandle(CreateMethodType("String", { "String" }));
    ASSERT_FALSE(mt1->IsExactMatch(mt2.Get()));
  }
}

}  // namespace mirror
}  // namespace art
