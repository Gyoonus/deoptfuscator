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

#include <jni.h>
#include <vector>

#include "art_field-inl.h"
#include "base/enums.h"
#include "common_compiler_test.h"
#include "mirror/field-inl.h"
#include "proxy_test.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace proxy_test {

class ProxyTest : public CommonRuntimeTest {};

// Creates a proxy class and check ClassHelper works correctly.
TEST_F(ProxyTest, ProxyClassHelper) {
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("Interfaces");
  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));

  Handle<mirror::Class> I(hs.NewHandle(
      class_linker_->FindClass(soa.Self(), "LInterfaces$I;", class_loader)));
  Handle<mirror::Class> J(hs.NewHandle(
      class_linker_->FindClass(soa.Self(), "LInterfaces$J;", class_loader)));
  ASSERT_TRUE(I != nullptr);
  ASSERT_TRUE(J != nullptr);

  std::vector<mirror::Class*> interfaces;
  interfaces.push_back(I.Get());
  interfaces.push_back(J.Get());
  Handle<mirror::Class> proxy_class(hs.NewHandle(
      GenerateProxyClass(soa, jclass_loader, class_linker_, "$Proxy1234", interfaces)));
  interfaces.clear();  // Don't least possibly stale objects in the array as good practice.
  ASSERT_TRUE(proxy_class != nullptr);
  ASSERT_TRUE(proxy_class->IsProxyClass());
  ASSERT_TRUE(proxy_class->IsInitialized());

  EXPECT_EQ(2U, proxy_class->NumDirectInterfaces());  // Interfaces$I and Interfaces$J.
  EXPECT_OBJ_PTR_EQ(I.Get(), mirror::Class::GetDirectInterface(soa.Self(), proxy_class.Get(), 0));
  EXPECT_OBJ_PTR_EQ(J.Get(), mirror::Class::GetDirectInterface(soa.Self(), proxy_class.Get(), 1));
  std::string temp;
  const char* proxy_class_descriptor = proxy_class->GetDescriptor(&temp);
  EXPECT_STREQ("L$Proxy1234;", proxy_class_descriptor);
  EXPECT_EQ(nullptr, proxy_class->GetSourceFile());
}

// Creates a proxy class and check FieldHelper works correctly.
TEST_F(ProxyTest, ProxyFieldHelper) {
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("Interfaces");
  StackHandleScope<9> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));

  Handle<mirror::Class> I(hs.NewHandle(
      class_linker_->FindClass(soa.Self(), "LInterfaces$I;", class_loader)));
  Handle<mirror::Class> J(hs.NewHandle(
      class_linker_->FindClass(soa.Self(), "LInterfaces$J;", class_loader)));
  ASSERT_TRUE(I != nullptr);
  ASSERT_TRUE(J != nullptr);

  Handle<mirror::Class> proxyClass;
  {
    std::vector<mirror::Class*> interfaces;
    interfaces.push_back(I.Get());
    interfaces.push_back(J.Get());
    proxyClass = hs.NewHandle(
        GenerateProxyClass(soa, jclass_loader, class_linker_, "$Proxy1234", interfaces));
  }

  ASSERT_TRUE(proxyClass != nullptr);
  ASSERT_TRUE(proxyClass->IsProxyClass());
  ASSERT_TRUE(proxyClass->IsInitialized());

  EXPECT_TRUE(proxyClass->GetIFieldsPtr() == nullptr);

  LengthPrefixedArray<ArtField>* static_fields = proxyClass->GetSFieldsPtr();
  ASSERT_TRUE(static_fields != nullptr);
  ASSERT_EQ(2u, proxyClass->NumStaticFields());

  Handle<mirror::Class> interfacesFieldClass(
      hs.NewHandle(class_linker_->FindSystemClass(soa.Self(), "[Ljava/lang/Class;")));
  ASSERT_TRUE(interfacesFieldClass != nullptr);
  Handle<mirror::Class> throwsFieldClass(
      hs.NewHandle(class_linker_->FindSystemClass(soa.Self(), "[[Ljava/lang/Class;")));
  ASSERT_TRUE(throwsFieldClass != nullptr);

  // Test "Class[] interfaces" field.
  ArtField* field = &static_fields->At(0);
  EXPECT_STREQ("interfaces", field->GetName());
  EXPECT_STREQ("[Ljava/lang/Class;", field->GetTypeDescriptor());
  EXPECT_OBJ_PTR_EQ(interfacesFieldClass.Get(), field->ResolveType());
  std::string temp;
  EXPECT_STREQ("L$Proxy1234;", field->GetDeclaringClass()->GetDescriptor(&temp));
  EXPECT_FALSE(field->IsPrimitiveType());

  // Test "Class[][] throws" field.
  field = &static_fields->At(1);
  EXPECT_STREQ("throws", field->GetName());
  EXPECT_STREQ("[[Ljava/lang/Class;", field->GetTypeDescriptor());
  EXPECT_OBJ_PTR_EQ(throwsFieldClass.Get(), field->ResolveType());
  EXPECT_STREQ("L$Proxy1234;", field->GetDeclaringClass()->GetDescriptor(&temp));
  EXPECT_FALSE(field->IsPrimitiveType());
}

// Creates two proxy classes and check the art/mirror fields of their static fields.
TEST_F(ProxyTest, CheckArtMirrorFieldsOfProxyStaticFields) {
  ScopedObjectAccess soa(Thread::Current());
  jobject jclass_loader = LoadDex("Interfaces");
  StackHandleScope<7> hs(soa.Self());

  Handle<mirror::Class> proxyClass0;
  Handle<mirror::Class> proxyClass1;
  {
    std::vector<mirror::Class*> interfaces;
    proxyClass0 = hs.NewHandle(
        GenerateProxyClass(soa, jclass_loader, class_linker_, "$Proxy0", interfaces));
    proxyClass1 = hs.NewHandle(
        GenerateProxyClass(soa, jclass_loader, class_linker_, "$Proxy1", interfaces));
  }

  ASSERT_TRUE(proxyClass0 != nullptr);
  ASSERT_TRUE(proxyClass0->IsProxyClass());
  ASSERT_TRUE(proxyClass0->IsInitialized());
  ASSERT_TRUE(proxyClass1 != nullptr);
  ASSERT_TRUE(proxyClass1->IsProxyClass());
  ASSERT_TRUE(proxyClass1->IsInitialized());

  LengthPrefixedArray<ArtField>* static_fields0 = proxyClass0->GetSFieldsPtr();
  ASSERT_TRUE(static_fields0 != nullptr);
  ASSERT_EQ(2u, static_fields0->size());
  LengthPrefixedArray<ArtField>* static_fields1 = proxyClass1->GetSFieldsPtr();
  ASSERT_TRUE(static_fields1 != nullptr);
  ASSERT_EQ(2u, static_fields1->size());

  EXPECT_OBJ_PTR_EQ(static_fields0->At(0).GetDeclaringClass(), proxyClass0.Get());
  EXPECT_OBJ_PTR_EQ(static_fields0->At(1).GetDeclaringClass(), proxyClass0.Get());
  EXPECT_OBJ_PTR_EQ(static_fields1->At(0).GetDeclaringClass(), proxyClass1.Get());
  EXPECT_OBJ_PTR_EQ(static_fields1->At(1).GetDeclaringClass(), proxyClass1.Get());

  ASSERT_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), kRuntimePointerSize);
  ASSERT_FALSE(Runtime::Current()->IsActiveTransaction());
  Handle<mirror::Field> field00 =
      hs.NewHandle(mirror::Field::CreateFromArtField<kRuntimePointerSize, false>(
          soa.Self(), &static_fields0->At(0), true));
  Handle<mirror::Field> field01 =
      hs.NewHandle(mirror::Field::CreateFromArtField<kRuntimePointerSize, false>(
          soa.Self(), &static_fields0->At(1), true));
  Handle<mirror::Field> field10 =
      hs.NewHandle(mirror::Field::CreateFromArtField<kRuntimePointerSize, false>(
          soa.Self(), &static_fields1->At(0), true));
  Handle<mirror::Field> field11 =
      hs.NewHandle(mirror::Field::CreateFromArtField<kRuntimePointerSize, false>(
          soa.Self(), &static_fields1->At(1), true));
  EXPECT_EQ(field00->GetArtField(), &static_fields0->At(0));
  EXPECT_EQ(field01->GetArtField(), &static_fields0->At(1));
  EXPECT_EQ(field10->GetArtField(), &static_fields1->At(0));
  EXPECT_EQ(field11->GetArtField(), &static_fields1->At(1));
}

}  // namespace proxy_test
}  // namespace art
