/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "method_handles.h"

#include "class_linker-inl.h"
#include "common_runtime_test.h"
#include "handle_scope-inl.h"
#include "jvalue-inl.h"
#include "mirror/method_type.h"
#include "mirror/object_array-inl.h"
#include "reflection.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

namespace {
  bool IsClassCastException(ObjPtr<mirror::Throwable> throwable)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return throwable->GetClass()->DescriptorEquals("Ljava/lang/ClassCastException;");
  }

  bool IsNullPointerException(ObjPtr<mirror::Throwable> throwable)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return throwable->GetClass()->DescriptorEquals("Ljava/lang/NullPointerException;");
  }

  bool IsWrongMethodTypeException(ObjPtr<mirror::Throwable> throwable)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return throwable->GetClass()->DescriptorEquals("Ljava/lang/invoke/WrongMethodTypeException;");
  }

  static mirror::MethodType* CreateVoidMethodType(Thread* self,
                                                  Handle<mirror::Class> parameter_type)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    ClassLinker* cl = Runtime::Current()->GetClassLinker();
    StackHandleScope<2> hs(self);
    ObjPtr<mirror::Class> class_type = mirror::Class::GetJavaLangClass();
    ObjPtr<mirror::Class> class_array_type = cl->FindArrayClass(self, &class_type);
    auto parameter_types = hs.NewHandle(
        mirror::ObjectArray<mirror::Class>::Alloc(self, class_array_type, 1));
    parameter_types->Set(0, parameter_type.Get());
    Handle<mirror::Class> void_class = hs.NewHandle(cl->FindPrimitiveClass('V'));
    return mirror::MethodType::Create(self, void_class, parameter_types);
  }

  static bool TryConversion(Thread* self,
                            Handle<mirror::Class> from,
                            Handle<mirror::Class> to,
                            JValue* value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    StackHandleScope<2> hs(self);
    Handle<mirror::MethodType> from_mt = hs.NewHandle(CreateVoidMethodType(self, from));
    Handle<mirror::MethodType> to_mt = hs.NewHandle(CreateVoidMethodType(self, to));
    return ConvertJValueCommon(from_mt, to_mt, from.Get(), to.Get(), value);
  }
}  // namespace

class MethodHandlesTest : public CommonRuntimeTest {};

//
// Primitive -> Primitive Conversions
//

TEST_F(MethodHandlesTest, SupportedPrimitiveWideningBI) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('B'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('I'));
  JValue value = JValue::FromPrimitive(static_cast<int8_t>(3));
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_EQ(3, value.GetI());
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
}

TEST_F(MethodHandlesTest, SupportedPrimitiveWideningCJ) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('C'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('J'));
  uint16_t raw_value = 0x8000;
  JValue value = JValue::FromPrimitive(raw_value);
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  ASSERT_EQ(static_cast<int64_t>(raw_value), value.GetJ());
}

TEST_F(MethodHandlesTest, SupportedPrimitiveWideningIF) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('F'));
  JValue value = JValue::FromPrimitive(-16);
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  ASSERT_FLOAT_EQ(-16.0f, value.GetF());
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveWideningBC) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('B'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('C'));
  JValue value;
  value.SetB(0);
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveWideningSC) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('S'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('C'));
  JValue value;
  value.SetS(0x1234);
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveWideningDJ) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('D'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('J'));
  JValue value;
  value.SetD(1e72);
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveWideningZI) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('Z'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('I'));
  JValue value;
  value.SetZ(true);
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

//
// Reference -> Reference Conversions
//

TEST_F(MethodHandlesTest, SupportedReferenceCast) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  static const int32_t kInitialValue = 101;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Object> boxed_value = hs.NewHandle(BoxPrimitive(Primitive::kPrimInt, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(boxed_value->GetClass());
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Number;"));
  value.SetL(boxed_value.Get());
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  JValue unboxed_value;
  ASSERT_TRUE(UnboxPrimitiveForResult(value.GetL(), cl->FindPrimitiveClass('I'), &unboxed_value));
  ASSERT_EQ(kInitialValue, unboxed_value.GetI());
}

TEST_F(MethodHandlesTest, UnsupportedReferenceCast) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  JValue value = JValue::FromPrimitive(3.733e2);
  Handle<mirror::Object> boxed_value =
      hs.NewHandle(BoxPrimitive(Primitive::kPrimDouble, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(boxed_value->GetClass());
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  value.SetL(boxed_value.Get());
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsClassCastException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

//
// Primitive -> Reference Conversions
//

TEST_F(MethodHandlesTest, SupportedPrimitiveConversionPrimitiveToBoxed) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  const int32_t kInitialValue = 1;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  JValue unboxed_to_value;
  ASSERT_TRUE(UnboxPrimitiveForResult(value.GetL(), from.Get(), &unboxed_to_value));
  ASSERT_EQ(kInitialValue, unboxed_to_value.GetI());
}

TEST_F(MethodHandlesTest, SupportedPrimitiveConversionPrimitiveToBoxedSuper) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  const int32_t kInitialValue = 1;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Number;"));
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  JValue unboxed_to_value;
  ASSERT_TRUE(UnboxPrimitiveForResult(value.GetL(), from.Get(), &unboxed_to_value));
  ASSERT_EQ(kInitialValue, unboxed_to_value.GetI());
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveConversionNotBoxable) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  const int32_t kInitialValue = 1;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Runtime;"));
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveConversionPrimitiveToBoxedWider) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  const int32_t kInitialValue = 1;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Long;"));
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedPrimitiveConversionPrimitiveToBoxedNarrower) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  const int32_t kInitialValue = 1;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Class> from = hs.NewHandle(cl->FindPrimitiveClass('I'));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Byte;"));
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

//
// Reference -> Primitive Conversions
//

TEST_F(MethodHandlesTest, SupportedBoxedToPrimitiveConversion) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  const int32_t kInitialValue = 101;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Object> boxed_value = hs.NewHandle(BoxPrimitive(Primitive::kPrimInt, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('I'));
  value.SetL(boxed_value.Get());
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_FALSE(soa.Self()->IsExceptionPending());
  ASSERT_EQ(kInitialValue, value.GetI());
}

TEST_F(MethodHandlesTest, SupportedBoxedToWiderPrimitiveConversion) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  static const int32_t kInitialValue = 101;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Object> boxed_value = hs.NewHandle(BoxPrimitive(Primitive::kPrimInt, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('J'));
  value.SetL(boxed_value.Get());
  ASSERT_TRUE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_EQ(kInitialValue, value.GetJ());
}

TEST_F(MethodHandlesTest, UnsupportedNullBoxedToPrimitiveConversion) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  JValue value = JValue::FromPrimitive(101);
  ScopedNullHandle<mirror::Object> boxed_value;
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('I'));
  value.SetL(boxed_value.Get());
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsNullPointerException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedNotBoxReferenceToPrimitiveConversion) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Class;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('I'));
  // Set value to be converted as some non-primitive type.
  JValue value;
  value.SetL(cl->FindPrimitiveClass('V'));
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedBoxedToNarrowerPrimitiveConversionNoCast) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  static const int32_t kInitialValue = 101;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Object> boxed_value = hs.NewHandle(BoxPrimitive(Primitive::kPrimInt, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Integer;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('S'));
  value.SetL(boxed_value.Get());
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsWrongMethodTypeException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

TEST_F(MethodHandlesTest, UnsupportedBoxedToNarrowerPrimitiveConversionWithCast) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  StackHandleScope<3> hs(soa.Self());
  static const double kInitialValue = 1e77;
  JValue value = JValue::FromPrimitive(kInitialValue);
  Handle<mirror::Object> boxed_value =
      hs.NewHandle(BoxPrimitive(Primitive::kPrimDouble, value).Ptr());
  Handle<mirror::Class> from = hs.NewHandle(cl->FindSystemClass(soa.Self(), "Ljava/lang/Number;"));
  Handle<mirror::Class> to = hs.NewHandle(cl->FindPrimitiveClass('F'));
  value.SetL(boxed_value.Get());
  ASSERT_FALSE(TryConversion(soa.Self(), from, to, &value));
  ASSERT_TRUE(soa.Self()->IsExceptionPending());
  ASSERT_TRUE(IsClassCastException(soa.Self()->GetException()));
  soa.Self()->ClearException();
}

}  // namespace art
