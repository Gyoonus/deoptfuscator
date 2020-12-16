/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "var_handle.h"

#include <string>
#include <vector>

#include "art_field-inl.h"
#include "class-inl.h"
#include "class_linker-inl.h"
#include "class_loader.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "jvalue-inl.h"
#include "method_type.h"
#include "object_array-inl.h"
#include "reflection.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace mirror {

// Tests for mirror::VarHandle and it's descendents.
class VarHandleTest : public CommonRuntimeTest {
 public:
  static FieldVarHandle* CreateFieldVarHandle(Thread* const self,
                                              ArtField* art_field,
                                              int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<4> hs(self);
    Handle<FieldVarHandle> fvh = hs.NewHandle(
        ObjPtr<FieldVarHandle>::DownCast(FieldVarHandle::StaticClass()->AllocObject(self)));
    Handle<Class> var_type = hs.NewHandle(art_field->ResolveType());

    if (art_field->IsStatic()) {
      InitializeVarHandle(fvh.Get(), var_type, access_modes_bit_mask);
    } else {
      Handle<Class> declaring_type = hs.NewHandle(art_field->GetDeclaringClass().Ptr());
      InitializeVarHandle(fvh.Get(),
                          var_type,
                          declaring_type,
                          access_modes_bit_mask);
    }
    uintptr_t opaque_field = reinterpret_cast<uintptr_t>(art_field);
    fvh->SetField64<false>(FieldVarHandle::ArtFieldOffset(), opaque_field);
    return fvh.Get();
  }

  static ArrayElementVarHandle* CreateArrayElementVarHandle(Thread* const self,
                                                            Handle<Class> array_class,
                                                            int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<3> hs(self);
    Handle<ArrayElementVarHandle> vh = hs.NewHandle(
        ObjPtr<ArrayElementVarHandle>::DownCast(
            ArrayElementVarHandle::StaticClass()->AllocObject(self)));

    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    InitializeVarHandle(vh.Get(), var_type, array_class, index_type, access_modes_bit_mask);
    return vh.Get();
  }

  static ByteArrayViewVarHandle* CreateByteArrayViewVarHandle(Thread* const self,
                                                              Handle<Class> view_array_class,
                                                              bool native_byte_order,
                                                              int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<4> hs(self);
    Handle<ByteArrayViewVarHandle> bvh = hs.NewHandle(
        ObjPtr<ByteArrayViewVarHandle>::DownCast(
            ByteArrayViewVarHandle::StaticClass()->AllocObject(self)));

    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(view_array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    ObjPtr<mirror::Class> byte_class = class_linker->FindPrimitiveClass('B');
    Handle<Class> byte_array_class(hs.NewHandle(class_linker->FindArrayClass(self, &byte_class)));
    InitializeVarHandle(bvh.Get(), var_type, byte_array_class, index_type, access_modes_bit_mask);
    bvh->SetFieldBoolean<false>(ByteArrayViewVarHandle::NativeByteOrderOffset(), native_byte_order);
    return bvh.Get();
  }

  static ByteBufferViewVarHandle* CreateByteBufferViewVarHandle(Thread* const self,
                                                                Handle<Class> view_array_class,
                                                                bool native_byte_order,
                                                                int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_) {
    StackHandleScope<5> hs(self);
    Handle<ByteBufferViewVarHandle> bvh = hs.NewHandle(
        ObjPtr<ByteBufferViewVarHandle>::DownCast(
            ByteArrayViewVarHandle::StaticClass()->AllocObject(self)));
    // Initialize super class fields
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Handle<Class> var_type = hs.NewHandle(view_array_class->GetComponentType());
    Handle<Class> index_type = hs.NewHandle(class_linker->FindPrimitiveClass('I'));
    Handle<ClassLoader> boot_class_loader;
    Handle<Class> byte_buffer_class = hs.NewHandle(
        class_linker->FindSystemClass(self, "Ljava/nio/ByteBuffer;"));
    InitializeVarHandle(bvh.Get(), var_type, byte_buffer_class, index_type, access_modes_bit_mask);
    bvh->SetFieldBoolean<false>(ByteBufferViewVarHandle::NativeByteOrderOffset(),
                                native_byte_order);
    return bvh.Get();
  }

  static int32_t AccessModesBitMask(VarHandle::AccessMode mode) {
    return 1 << static_cast<int32_t>(mode);
  }

  template<typename... Args>
  static int32_t AccessModesBitMask(VarHandle::AccessMode first, Args... args) {
    return AccessModesBitMask(first) | AccessModesBitMask(args...);
  }

  // Helper to get the VarType of a VarHandle.
  static Class* GetVarType(VarHandle* vh) REQUIRES_SHARED(Locks::mutator_lock_) {
    return vh->GetVarType();
  }

  // Helper to get the CoordinateType0 of a VarHandle.
  static Class* GetCoordinateType0(VarHandle* vh) REQUIRES_SHARED(Locks::mutator_lock_) {
    return vh->GetCoordinateType0();
  }

  // Helper to get the CoordinateType1 of a VarHandle.
  static Class* GetCoordinateType1(VarHandle* vh) REQUIRES_SHARED(Locks::mutator_lock_) {
    return vh->GetCoordinateType1();
  }

  // Helper to get the AccessModesBitMask of a VarHandle.
  static int32_t GetAccessModesBitMask(VarHandle* vh) REQUIRES_SHARED(Locks::mutator_lock_) {
    return vh->GetAccessModesBitMask();
  }

 private:
  static void InitializeVarHandle(VarHandle* vh,
                                  Handle<Class> var_type,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    vh->SetFieldObject<false>(VarHandle::VarTypeOffset(), var_type.Get());
    vh->SetField32<false>(VarHandle::AccessModesBitMaskOffset(), access_modes_bit_mask);
  }

  static void InitializeVarHandle(VarHandle* vh,
                                  Handle<Class> var_type,
                                  Handle<Class> coordinate_type0,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    InitializeVarHandle(vh, var_type, access_modes_bit_mask);
    vh->SetFieldObject<false>(VarHandle::CoordinateType0Offset(), coordinate_type0.Get());
  }

  static void InitializeVarHandle(VarHandle* vh,
                                  Handle<Class> var_type,
                                  Handle<Class> coordinate_type0,
                                  Handle<Class> coordinate_type1,
                                  int32_t access_modes_bit_mask)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    InitializeVarHandle(vh, var_type, access_modes_bit_mask);
    vh->SetFieldObject<false>(VarHandle::CoordinateType0Offset(), coordinate_type0.Get());
    vh->SetFieldObject<false>(VarHandle::CoordinateType1Offset(), coordinate_type1.Get());
  }
};

// Convenience method for constructing MethodType instances from
// well-formed method descriptors.
static MethodType* MethodTypeOf(const std::string& method_descriptor) {
  std::vector<std::string> descriptors;

  auto it = method_descriptor.cbegin();
  if (*it++ != '(') {
    LOG(FATAL) << "Bad descriptor: " << method_descriptor;
  }

  bool returnValueSeen = false;
  const char* prefix = "";
  for (; it != method_descriptor.cend() && !returnValueSeen; ++it) {
    switch (*it) {
      case ')':
        descriptors.push_back(std::string(++it, method_descriptor.cend()));
        returnValueSeen = true;
        break;
      case '[':
        prefix = "[";
        break;
      case 'Z':
      case 'B':
      case 'C':
      case 'S':
      case 'I':
      case 'J':
      case 'F':
      case 'D':
        descriptors.push_back(prefix + std::string(it, it + 1));
        prefix = "";
        break;
      case 'L': {
        auto last = it + 1;
        while (*last != ';') {
          ++last;
        }
        descriptors.push_back(prefix + std::string(it, last + 1));
        prefix = "";
        it = last;
        break;
      }
      default:
        LOG(FATAL) << "Bad descriptor: " << method_descriptor;
    }
  }

  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  Thread* const self = Thread::Current();

  ScopedObjectAccess soa(self);
  StackHandleScope<3> hs(self);
  int ptypes_count = static_cast<int>(descriptors.size()) - 1;
  ObjPtr<mirror::Class> class_type = mirror::Class::GetJavaLangClass();
  ObjPtr<mirror::Class> array_of_class = class_linker->FindArrayClass(self, &class_type);
  Handle<ObjectArray<Class>> ptypes = hs.NewHandle(
      ObjectArray<Class>::Alloc(Thread::Current(), array_of_class, ptypes_count));
  Handle<mirror::ClassLoader> boot_class_loader = hs.NewHandle<mirror::ClassLoader>(nullptr);
  for (int i = 0; i < ptypes_count; ++i) {
    ptypes->Set(i, class_linker->FindClass(self, descriptors[i].c_str(), boot_class_loader));
  }
  Handle<Class> rtype =
      hs.NewHandle(class_linker->FindClass(self, descriptors.back().c_str(), boot_class_loader));
  return MethodType::Create(self, rtype, ptypes);
}

TEST_F(VarHandleTest, InstanceFieldVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);

  ObjPtr<Object> i = BoxPrimitive(Primitive::kPrimInt, JValue::FromPrimitive<int32_t>(37));
  ArtField* value = mirror::Class::FindField(self, i->GetClass(), "value", "I");
  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndBitwiseXor);
  StackHandleScope<1> hs(self);
  Handle<mirror::FieldVarHandle> fvh(hs.NewHandle(CreateFieldVarHandle(self, value, mask)));
  EXPECT_FALSE(fvh.IsNull());
  EXPECT_EQ(value, fvh->GetField());

  // Check access modes
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)Z")));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;I)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;II)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode,
                                             MethodTypeOf("(Ljava/lang/Integer;II)I")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;II)I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;II)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;I)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(IIII)V")));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;I)I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;I)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/lang/Integer;I)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)S")));
  }

  // Check synthesized method types match expected forms.
  {
    MethodType* get = MethodTypeOf("(Ljava/lang/Integer;)I");
    MethodType* set = MethodTypeOf("(Ljava/lang/Integer;I)V");
    MethodType* compareAndSet = MethodTypeOf("(Ljava/lang/Integer;II)Z");
    MethodType* compareAndExchange = MethodTypeOf("(Ljava/lang/Integer;II)I");
    MethodType* getAndUpdate = MethodTypeOf("(Ljava/lang/Integer;I)I");
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGet)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSet)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetVolatile)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetVolatile)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAcquire)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetRelease)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetOpaque)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetOpaque)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchange)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeAcquire)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeRelease)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetPlain)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetAcquire)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetRelease)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSet)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAdd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOr)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAnd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXor)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorAcquire)->IsExactMatch(getAndUpdate));
  }
}

TEST_F(VarHandleTest, StaticFieldVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);

  ObjPtr<Object> i = BoxPrimitive(Primitive::kPrimInt, JValue::FromPrimitive<int32_t>(37));
  ArtField* value = mirror::Class::FindField(self, i->GetClass(), "MIN_VALUE", "I");
  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kSet,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease);
  StackHandleScope<1> hs(self);
  Handle<mirror::FieldVarHandle> fvh(hs.NewHandle(CreateFieldVarHandle(self, value, mask)));
  EXPECT_FALSE(fvh.IsNull());
  EXPECT_EQ(value, fvh->GetField());

  // Check access modes
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_FALSE(fvh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)Z")));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(I)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(F)V")));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode,
                                             MethodTypeOf("(II)Ljava/lang/String;")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("()Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(ID)I")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)S")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(IIJ)V")));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(I)I")));
    EXPECT_TRUE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(I)V")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(I)Z")));
    EXPECT_FALSE(fvh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)V")));
  }

  // Check synthesized method types match expected forms.
  {
    MethodType* get = MethodTypeOf("()I");
    MethodType* set = MethodTypeOf("(I)V");
    MethodType* compareAndSet = MethodTypeOf("(II)Z");
    MethodType* compareAndExchange = MethodTypeOf("(II)I");
    MethodType* getAndUpdate = MethodTypeOf("(I)I");
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGet)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSet)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetVolatile)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetVolatile)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAcquire)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetRelease)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetOpaque)->IsExactMatch(get));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetOpaque)->IsExactMatch(set));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchange)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeAcquire)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeRelease)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetPlain)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetAcquire)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetRelease)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSet)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAdd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOr)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAnd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXor)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(fvh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorAcquire)->IsExactMatch(getAndUpdate));
  }
}

TEST_F(VarHandleTest, ArrayElementVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<2> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kSet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kSetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kSetRelease,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kSetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchange,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kCompareAndExchangeRelease,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSet,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetRelease,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAdd,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndAddRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAnd,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseAndAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  ObjPtr<mirror::Class> string_class = mirror::String::GetJavaLangString();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<Class> string_array_class(hs.NewHandle(class_linker->FindArrayClass(self, &string_class)));
  Handle<mirror::ArrayElementVarHandle> vh(hs.NewHandle(CreateArrayElementVarHandle(self, string_array_class, mask)));
  EXPECT_FALSE(vh.IsNull());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;I)Ljava/lang/String;")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;I)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;Ljava/lang/String;)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)Z")));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;I)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;I)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(
        vh->IsMethodTypeCompatible(
            access_mode,
            MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode,
                                             MethodTypeOf("([Ljava/lang/String;III)I")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;I)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;II)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(III)V")));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)V")));
  }

  // Check synthesized method types match expected forms.
  {
    MethodType* get = MethodTypeOf("([Ljava/lang/String;I)Ljava/lang/String;");
    MethodType* set = MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)V");
    MethodType* compareAndSet = MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Z");
    MethodType* compareAndExchange = MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    MethodType* getAndUpdate = MethodTypeOf("([Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;");
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGet)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSet)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetVolatile)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetVolatile)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAcquire)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetRelease)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetOpaque)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetOpaque)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchange)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeAcquire)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeRelease)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetPlain)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetAcquire)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetRelease)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSet)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAdd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOr)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAnd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXor)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorAcquire)->IsExactMatch(getAndUpdate));
  }
}

TEST_F(VarHandleTest, ByteArrayViewVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<2> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::Class> char_class = class_linker->FindPrimitiveClass('C');
  Handle<Class> char_array_class(hs.NewHandle(class_linker->FindArrayClass(self, &char_class)));
  const bool native_byte_order = true;
  Handle<mirror::ByteArrayViewVarHandle> vh(hs.NewHandle(CreateByteArrayViewVarHandle(self, char_array_class, native_byte_order, mask)));
  EXPECT_FALSE(vh.IsNull());
  EXPECT_EQ(native_byte_order, vh->GetNativeByteOrder());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BI)C")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BI)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BC)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)Z")));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BIC)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BI)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BI)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(
        vh->IsMethodTypeCompatible(
            access_mode,
            MethodTypeOf("([BICC)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode,
                                             MethodTypeOf("([BIII)I")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BI)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BICC)C")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BICC)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BII)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(III)V")));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BIC)C")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BIC)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("([BIC)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)V")));
  }

  // Check synthesized method types match expected forms.
  {
    MethodType* get = MethodTypeOf("([BI)C");
    MethodType* set = MethodTypeOf("([BIC)V");
    MethodType* compareAndSet = MethodTypeOf("([BICC)Z");
    MethodType* compareAndExchange = MethodTypeOf("([BICC)C");
    MethodType* getAndUpdate = MethodTypeOf("([BIC)C");
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGet)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSet)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetVolatile)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetVolatile)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAcquire)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetRelease)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetOpaque)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetOpaque)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchange)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeAcquire)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeRelease)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetPlain)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetAcquire)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetRelease)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSet)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAdd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOr)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAnd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXor)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorAcquire)->IsExactMatch(getAndUpdate));
  }
}

TEST_F(VarHandleTest, ByteBufferViewVarHandle) {
  Thread * const self = Thread::Current();
  ScopedObjectAccess soa(self);
  StackHandleScope<2> hs(self);

  int32_t mask = AccessModesBitMask(VarHandle::AccessMode::kGet,
                                    VarHandle::AccessMode::kGetVolatile,
                                    VarHandle::AccessMode::kGetAcquire,
                                    VarHandle::AccessMode::kGetOpaque,
                                    VarHandle::AccessMode::kCompareAndSet,
                                    VarHandle::AccessMode::kCompareAndExchangeAcquire,
                                    VarHandle::AccessMode::kWeakCompareAndSetPlain,
                                    VarHandle::AccessMode::kWeakCompareAndSetAcquire,
                                    VarHandle::AccessMode::kGetAndSet,
                                    VarHandle::AccessMode::kGetAndSetRelease,
                                    VarHandle::AccessMode::kGetAndAddAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseOr,
                                    VarHandle::AccessMode::kGetAndBitwiseOrAcquire,
                                    VarHandle::AccessMode::kGetAndBitwiseAndRelease,
                                    VarHandle::AccessMode::kGetAndBitwiseXor,
                                    VarHandle::AccessMode::kGetAndBitwiseXorAcquire);

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::Class> double_class = class_linker->FindPrimitiveClass('D');
  Handle<Class> double_array_class(hs.NewHandle(class_linker->FindArrayClass(self, &double_class)));
  const bool native_byte_order = false;
  Handle<mirror::ByteBufferViewVarHandle> vh(hs.NewHandle(CreateByteBufferViewVarHandle(self, double_array_class, native_byte_order, mask)));
  EXPECT_FALSE(vh.IsNull());
  EXPECT_EQ(native_byte_order, vh->GetNativeByteOrder());

  // Check access modes
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetVolatile));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetVolatile));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetOpaque));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kSetOpaque));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchange));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kCompareAndExchangeRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetPlain));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSet));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kWeakCompareAndSetRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSet));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndSetRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAdd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndAddRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOr));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseOrAcquire));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAnd));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndRelease));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseAndAcquire));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXor));
  EXPECT_FALSE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorRelease));
  EXPECT_TRUE(vh->IsAccessModeSupported(VarHandle::AccessMode::kGetAndBitwiseXorAcquire));

  // Check compatibility - "Get" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;I)D")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;I)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;D)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)Z")));
  }

  // Check compatibility - "Set" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kSet;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;ID)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;I)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;I)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndSet" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndSet;
    EXPECT_TRUE(
        vh->IsMethodTypeCompatible(
            access_mode,
            MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode,
                                             MethodTypeOf("(Ljava/nio/ByteBuffer;IDI)D")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;I)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Z)V")));
  }

  // Check compatibility - "CompareAndExchange" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kCompareAndExchange;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)D")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;II)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(III)V")));
  }

  // Check compatibility - "GetAndUpdate" pattern
  {
    const VarHandle::AccessMode access_mode = VarHandle::AccessMode::kGetAndAdd;
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;ID)D")));
    EXPECT_TRUE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;ID)V")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(Ljava/nio/ByteBuffer;ID)Z")));
    EXPECT_FALSE(vh->IsMethodTypeCompatible(access_mode, MethodTypeOf("(II)V")));
  }

  // Check synthesized method types match expected forms.
  {
    MethodType* get = MethodTypeOf("(Ljava/nio/ByteBuffer;I)D");
    MethodType* set = MethodTypeOf("(Ljava/nio/ByteBuffer;ID)V");
    MethodType* compareAndSet = MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)Z");
    MethodType* compareAndExchange = MethodTypeOf("(Ljava/nio/ByteBuffer;IDD)D");
    MethodType* getAndUpdate = MethodTypeOf("(Ljava/nio/ByteBuffer;ID)D");
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGet)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSet)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetVolatile)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetVolatile)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAcquire)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetRelease)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetOpaque)->IsExactMatch(get));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kSetOpaque)->IsExactMatch(set));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchange)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeAcquire)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kCompareAndExchangeRelease)->IsExactMatch(compareAndExchange));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetPlain)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSet)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetAcquire)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kWeakCompareAndSetRelease)->IsExactMatch(compareAndSet));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSet)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndSetRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAdd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndAddRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOr)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseOrAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAnd)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseAndAcquire)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXor)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorRelease)->IsExactMatch(getAndUpdate));
    EXPECT_TRUE(vh->GetMethodTypeForAccessMode(self, VarHandle::AccessMode::kGetAndBitwiseXorAcquire)->IsExactMatch(getAndUpdate));
  }
}

TEST_F(VarHandleTest, GetMethodTypeForAccessMode) {
  VarHandle::AccessMode access_mode;

  // Invalid access mode names
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName(nullptr, &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("", &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("CompareAndExchange", &access_mode));
  EXPECT_FALSE(VarHandle::GetAccessModeByMethodName("compareAndExchangX", &access_mode));

  // Valid access mode names
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchange", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchange, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchangeAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchangeAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndExchangeRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndExchangeRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("compareAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kCompareAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("get", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAdd", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAdd, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAddAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAddAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndAddRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndAddRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAnd", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAnd, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAndAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAndAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseAndRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseAndRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOr", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOr, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOrAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOrAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseOrRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseOrRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXor", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXor, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXorAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXorAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndBitwiseXorRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndBitwiseXorRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSetAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getAndSetRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetAndSetRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getOpaque", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetOpaque, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("getVolatile", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kGetVolatile, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("set", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setOpaque", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetOpaque, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetRelease, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("setVolatile", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kSetVolatile, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSet", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSet, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetAcquire", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetAcquire, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetPlain", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetPlain, access_mode);
  EXPECT_TRUE(VarHandle::GetAccessModeByMethodName("weakCompareAndSetRelease", &access_mode));
  EXPECT_EQ(VarHandle::AccessMode::kWeakCompareAndSetRelease, access_mode);
}

}  // namespace mirror
}  // namespace art
