/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "reg_type.h"

#include <set>

#include "base/bit_vector.h"
#include "base/casts.h"
#include "base/scoped_arena_allocator.h"
#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "reg_type-inl.h"
#include "reg_type_cache-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {
namespace verifier {

class RegTypeTest : public CommonRuntimeTest {};

TEST_F(RegTypeTest, ConstLoHi) {
  // Tests creating primitive types types.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& ref_type_const_0 = cache.FromCat1Const(10, true);
  const RegType& ref_type_const_1 = cache.FromCat1Const(10, true);
  const RegType& ref_type_const_2 = cache.FromCat1Const(30, true);
  const RegType& ref_type_const_3 = cache.FromCat1Const(30, false);
  EXPECT_TRUE(ref_type_const_0.Equals(ref_type_const_1));
  EXPECT_FALSE(ref_type_const_0.Equals(ref_type_const_2));
  EXPECT_FALSE(ref_type_const_0.Equals(ref_type_const_3));

  const RegType& ref_type_const_wide_0 = cache.FromCat2ConstHi(50, true);
  const RegType& ref_type_const_wide_1 = cache.FromCat2ConstHi(50, true);
  EXPECT_TRUE(ref_type_const_wide_0.Equals(ref_type_const_wide_1));

  const RegType& ref_type_const_wide_2 = cache.FromCat2ConstLo(50, true);
  const RegType& ref_type_const_wide_3 = cache.FromCat2ConstLo(50, true);
  const RegType& ref_type_const_wide_4 = cache.FromCat2ConstLo(55, true);
  EXPECT_TRUE(ref_type_const_wide_2.Equals(ref_type_const_wide_3));
  EXPECT_FALSE(ref_type_const_wide_2.Equals(ref_type_const_wide_4));
}

TEST_F(RegTypeTest, Pairs) {
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  int64_t val = static_cast<int32_t>(1234);
  const RegType& precise_lo = cache.FromCat2ConstLo(static_cast<int32_t>(val), true);
  const RegType& precise_hi = cache.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
  const RegType& precise_const = cache.FromCat1Const(static_cast<int32_t>(val >> 32), true);
  const RegType& long_lo = cache.LongLo();
  const RegType& long_hi = cache.LongHi();
  // Check sanity of types.
  EXPECT_TRUE(precise_lo.IsLowHalf());
  EXPECT_FALSE(precise_hi.IsLowHalf());
  EXPECT_FALSE(precise_lo.IsHighHalf());
  EXPECT_TRUE(precise_hi.IsHighHalf());
  EXPECT_TRUE(long_hi.IsLongHighTypes());
  EXPECT_TRUE(precise_hi.IsLongHighTypes());
  // Check Pairing.
  EXPECT_FALSE(precise_lo.CheckWidePair(precise_const));
  EXPECT_TRUE(precise_lo.CheckWidePair(precise_hi));
  // Test Merging.
  EXPECT_TRUE((long_lo.Merge(precise_lo, &cache, /* verifier */ nullptr)).IsLongTypes());
  EXPECT_TRUE((long_hi.Merge(precise_hi, &cache, /* verifier */ nullptr)).IsLongHighTypes());
}

TEST_F(RegTypeTest, Primitives) {
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);

  const RegType& bool_reg_type = cache.Boolean();
  EXPECT_FALSE(bool_reg_type.IsUndefined());
  EXPECT_FALSE(bool_reg_type.IsConflict());
  EXPECT_FALSE(bool_reg_type.IsZero());
  EXPECT_FALSE(bool_reg_type.IsOne());
  EXPECT_FALSE(bool_reg_type.IsLongConstant());
  EXPECT_TRUE(bool_reg_type.IsBoolean());
  EXPECT_FALSE(bool_reg_type.IsByte());
  EXPECT_FALSE(bool_reg_type.IsChar());
  EXPECT_FALSE(bool_reg_type.IsShort());
  EXPECT_FALSE(bool_reg_type.IsInteger());
  EXPECT_FALSE(bool_reg_type.IsLong());
  EXPECT_FALSE(bool_reg_type.IsFloat());
  EXPECT_FALSE(bool_reg_type.IsDouble());
  EXPECT_FALSE(bool_reg_type.IsReference());
  EXPECT_FALSE(bool_reg_type.IsLowHalf());
  EXPECT_FALSE(bool_reg_type.IsHighHalf());
  EXPECT_FALSE(bool_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(bool_reg_type.IsReferenceTypes());
  EXPECT_TRUE(bool_reg_type.IsCategory1Types());
  EXPECT_FALSE(bool_reg_type.IsCategory2Types());
  EXPECT_TRUE(bool_reg_type.IsBooleanTypes());
  EXPECT_TRUE(bool_reg_type.IsByteTypes());
  EXPECT_TRUE(bool_reg_type.IsShortTypes());
  EXPECT_TRUE(bool_reg_type.IsCharTypes());
  EXPECT_TRUE(bool_reg_type.IsIntegralTypes());
  EXPECT_FALSE(bool_reg_type.IsFloatTypes());
  EXPECT_FALSE(bool_reg_type.IsLongTypes());
  EXPECT_FALSE(bool_reg_type.IsDoubleTypes());
  EXPECT_TRUE(bool_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(bool_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(bool_reg_type.HasClass());

  const RegType& byte_reg_type = cache.Byte();
  EXPECT_FALSE(byte_reg_type.IsUndefined());
  EXPECT_FALSE(byte_reg_type.IsConflict());
  EXPECT_FALSE(byte_reg_type.IsZero());
  EXPECT_FALSE(byte_reg_type.IsOne());
  EXPECT_FALSE(byte_reg_type.IsLongConstant());
  EXPECT_FALSE(byte_reg_type.IsBoolean());
  EXPECT_TRUE(byte_reg_type.IsByte());
  EXPECT_FALSE(byte_reg_type.IsChar());
  EXPECT_FALSE(byte_reg_type.IsShort());
  EXPECT_FALSE(byte_reg_type.IsInteger());
  EXPECT_FALSE(byte_reg_type.IsLong());
  EXPECT_FALSE(byte_reg_type.IsFloat());
  EXPECT_FALSE(byte_reg_type.IsDouble());
  EXPECT_FALSE(byte_reg_type.IsReference());
  EXPECT_FALSE(byte_reg_type.IsLowHalf());
  EXPECT_FALSE(byte_reg_type.IsHighHalf());
  EXPECT_FALSE(byte_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(byte_reg_type.IsReferenceTypes());
  EXPECT_TRUE(byte_reg_type.IsCategory1Types());
  EXPECT_FALSE(byte_reg_type.IsCategory2Types());
  EXPECT_FALSE(byte_reg_type.IsBooleanTypes());
  EXPECT_TRUE(byte_reg_type.IsByteTypes());
  EXPECT_TRUE(byte_reg_type.IsShortTypes());
  EXPECT_FALSE(byte_reg_type.IsCharTypes());
  EXPECT_TRUE(byte_reg_type.IsIntegralTypes());
  EXPECT_FALSE(byte_reg_type.IsFloatTypes());
  EXPECT_FALSE(byte_reg_type.IsLongTypes());
  EXPECT_FALSE(byte_reg_type.IsDoubleTypes());
  EXPECT_TRUE(byte_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(byte_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(byte_reg_type.HasClass());

  const RegType& char_reg_type = cache.Char();
  EXPECT_FALSE(char_reg_type.IsUndefined());
  EXPECT_FALSE(char_reg_type.IsConflict());
  EXPECT_FALSE(char_reg_type.IsZero());
  EXPECT_FALSE(char_reg_type.IsOne());
  EXPECT_FALSE(char_reg_type.IsLongConstant());
  EXPECT_FALSE(char_reg_type.IsBoolean());
  EXPECT_FALSE(char_reg_type.IsByte());
  EXPECT_TRUE(char_reg_type.IsChar());
  EXPECT_FALSE(char_reg_type.IsShort());
  EXPECT_FALSE(char_reg_type.IsInteger());
  EXPECT_FALSE(char_reg_type.IsLong());
  EXPECT_FALSE(char_reg_type.IsFloat());
  EXPECT_FALSE(char_reg_type.IsDouble());
  EXPECT_FALSE(char_reg_type.IsReference());
  EXPECT_FALSE(char_reg_type.IsLowHalf());
  EXPECT_FALSE(char_reg_type.IsHighHalf());
  EXPECT_FALSE(char_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(char_reg_type.IsReferenceTypes());
  EXPECT_TRUE(char_reg_type.IsCategory1Types());
  EXPECT_FALSE(char_reg_type.IsCategory2Types());
  EXPECT_FALSE(char_reg_type.IsBooleanTypes());
  EXPECT_FALSE(char_reg_type.IsByteTypes());
  EXPECT_FALSE(char_reg_type.IsShortTypes());
  EXPECT_TRUE(char_reg_type.IsCharTypes());
  EXPECT_TRUE(char_reg_type.IsIntegralTypes());
  EXPECT_FALSE(char_reg_type.IsFloatTypes());
  EXPECT_FALSE(char_reg_type.IsLongTypes());
  EXPECT_FALSE(char_reg_type.IsDoubleTypes());
  EXPECT_TRUE(char_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(char_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(char_reg_type.HasClass());

  const RegType& short_reg_type = cache.Short();
  EXPECT_FALSE(short_reg_type.IsUndefined());
  EXPECT_FALSE(short_reg_type.IsConflict());
  EXPECT_FALSE(short_reg_type.IsZero());
  EXPECT_FALSE(short_reg_type.IsOne());
  EXPECT_FALSE(short_reg_type.IsLongConstant());
  EXPECT_FALSE(short_reg_type.IsBoolean());
  EXPECT_FALSE(short_reg_type.IsByte());
  EXPECT_FALSE(short_reg_type.IsChar());
  EXPECT_TRUE(short_reg_type.IsShort());
  EXPECT_FALSE(short_reg_type.IsInteger());
  EXPECT_FALSE(short_reg_type.IsLong());
  EXPECT_FALSE(short_reg_type.IsFloat());
  EXPECT_FALSE(short_reg_type.IsDouble());
  EXPECT_FALSE(short_reg_type.IsReference());
  EXPECT_FALSE(short_reg_type.IsLowHalf());
  EXPECT_FALSE(short_reg_type.IsHighHalf());
  EXPECT_FALSE(short_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(short_reg_type.IsReferenceTypes());
  EXPECT_TRUE(short_reg_type.IsCategory1Types());
  EXPECT_FALSE(short_reg_type.IsCategory2Types());
  EXPECT_FALSE(short_reg_type.IsBooleanTypes());
  EXPECT_FALSE(short_reg_type.IsByteTypes());
  EXPECT_TRUE(short_reg_type.IsShortTypes());
  EXPECT_FALSE(short_reg_type.IsCharTypes());
  EXPECT_TRUE(short_reg_type.IsIntegralTypes());
  EXPECT_FALSE(short_reg_type.IsFloatTypes());
  EXPECT_FALSE(short_reg_type.IsLongTypes());
  EXPECT_FALSE(short_reg_type.IsDoubleTypes());
  EXPECT_TRUE(short_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(short_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(short_reg_type.HasClass());

  const RegType& int_reg_type = cache.Integer();
  EXPECT_FALSE(int_reg_type.IsUndefined());
  EXPECT_FALSE(int_reg_type.IsConflict());
  EXPECT_FALSE(int_reg_type.IsZero());
  EXPECT_FALSE(int_reg_type.IsOne());
  EXPECT_FALSE(int_reg_type.IsLongConstant());
  EXPECT_FALSE(int_reg_type.IsBoolean());
  EXPECT_FALSE(int_reg_type.IsByte());
  EXPECT_FALSE(int_reg_type.IsChar());
  EXPECT_FALSE(int_reg_type.IsShort());
  EXPECT_TRUE(int_reg_type.IsInteger());
  EXPECT_FALSE(int_reg_type.IsLong());
  EXPECT_FALSE(int_reg_type.IsFloat());
  EXPECT_FALSE(int_reg_type.IsDouble());
  EXPECT_FALSE(int_reg_type.IsReference());
  EXPECT_FALSE(int_reg_type.IsLowHalf());
  EXPECT_FALSE(int_reg_type.IsHighHalf());
  EXPECT_FALSE(int_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(int_reg_type.IsReferenceTypes());
  EXPECT_TRUE(int_reg_type.IsCategory1Types());
  EXPECT_FALSE(int_reg_type.IsCategory2Types());
  EXPECT_FALSE(int_reg_type.IsBooleanTypes());
  EXPECT_FALSE(int_reg_type.IsByteTypes());
  EXPECT_FALSE(int_reg_type.IsShortTypes());
  EXPECT_FALSE(int_reg_type.IsCharTypes());
  EXPECT_TRUE(int_reg_type.IsIntegralTypes());
  EXPECT_FALSE(int_reg_type.IsFloatTypes());
  EXPECT_FALSE(int_reg_type.IsLongTypes());
  EXPECT_FALSE(int_reg_type.IsDoubleTypes());
  EXPECT_TRUE(int_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(int_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(int_reg_type.HasClass());

  const RegType& long_reg_type = cache.LongLo();
  EXPECT_FALSE(long_reg_type.IsUndefined());
  EXPECT_FALSE(long_reg_type.IsConflict());
  EXPECT_FALSE(long_reg_type.IsZero());
  EXPECT_FALSE(long_reg_type.IsOne());
  EXPECT_FALSE(long_reg_type.IsLongConstant());
  EXPECT_FALSE(long_reg_type.IsBoolean());
  EXPECT_FALSE(long_reg_type.IsByte());
  EXPECT_FALSE(long_reg_type.IsChar());
  EXPECT_FALSE(long_reg_type.IsShort());
  EXPECT_FALSE(long_reg_type.IsInteger());
  EXPECT_TRUE(long_reg_type.IsLong());
  EXPECT_FALSE(long_reg_type.IsFloat());
  EXPECT_FALSE(long_reg_type.IsDouble());
  EXPECT_FALSE(long_reg_type.IsReference());
  EXPECT_TRUE(long_reg_type.IsLowHalf());
  EXPECT_FALSE(long_reg_type.IsHighHalf());
  EXPECT_TRUE(long_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(long_reg_type.IsReferenceTypes());
  EXPECT_FALSE(long_reg_type.IsCategory1Types());
  EXPECT_TRUE(long_reg_type.IsCategory2Types());
  EXPECT_FALSE(long_reg_type.IsBooleanTypes());
  EXPECT_FALSE(long_reg_type.IsByteTypes());
  EXPECT_FALSE(long_reg_type.IsShortTypes());
  EXPECT_FALSE(long_reg_type.IsCharTypes());
  EXPECT_FALSE(long_reg_type.IsIntegralTypes());
  EXPECT_FALSE(long_reg_type.IsFloatTypes());
  EXPECT_TRUE(long_reg_type.IsLongTypes());
  EXPECT_FALSE(long_reg_type.IsDoubleTypes());
  EXPECT_FALSE(long_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(long_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(long_reg_type.HasClass());

  const RegType& float_reg_type = cache.Float();
  EXPECT_FALSE(float_reg_type.IsUndefined());
  EXPECT_FALSE(float_reg_type.IsConflict());
  EXPECT_FALSE(float_reg_type.IsZero());
  EXPECT_FALSE(float_reg_type.IsOne());
  EXPECT_FALSE(float_reg_type.IsLongConstant());
  EXPECT_FALSE(float_reg_type.IsBoolean());
  EXPECT_FALSE(float_reg_type.IsByte());
  EXPECT_FALSE(float_reg_type.IsChar());
  EXPECT_FALSE(float_reg_type.IsShort());
  EXPECT_FALSE(float_reg_type.IsInteger());
  EXPECT_FALSE(float_reg_type.IsLong());
  EXPECT_TRUE(float_reg_type.IsFloat());
  EXPECT_FALSE(float_reg_type.IsDouble());
  EXPECT_FALSE(float_reg_type.IsReference());
  EXPECT_FALSE(float_reg_type.IsLowHalf());
  EXPECT_FALSE(float_reg_type.IsHighHalf());
  EXPECT_FALSE(float_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(float_reg_type.IsReferenceTypes());
  EXPECT_TRUE(float_reg_type.IsCategory1Types());
  EXPECT_FALSE(float_reg_type.IsCategory2Types());
  EXPECT_FALSE(float_reg_type.IsBooleanTypes());
  EXPECT_FALSE(float_reg_type.IsByteTypes());
  EXPECT_FALSE(float_reg_type.IsShortTypes());
  EXPECT_FALSE(float_reg_type.IsCharTypes());
  EXPECT_FALSE(float_reg_type.IsIntegralTypes());
  EXPECT_TRUE(float_reg_type.IsFloatTypes());
  EXPECT_FALSE(float_reg_type.IsLongTypes());
  EXPECT_FALSE(float_reg_type.IsDoubleTypes());
  EXPECT_FALSE(float_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(float_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(float_reg_type.HasClass());

  const RegType& double_reg_type = cache.DoubleLo();
  EXPECT_FALSE(double_reg_type.IsUndefined());
  EXPECT_FALSE(double_reg_type.IsConflict());
  EXPECT_FALSE(double_reg_type.IsZero());
  EXPECT_FALSE(double_reg_type.IsOne());
  EXPECT_FALSE(double_reg_type.IsLongConstant());
  EXPECT_FALSE(double_reg_type.IsBoolean());
  EXPECT_FALSE(double_reg_type.IsByte());
  EXPECT_FALSE(double_reg_type.IsChar());
  EXPECT_FALSE(double_reg_type.IsShort());
  EXPECT_FALSE(double_reg_type.IsInteger());
  EXPECT_FALSE(double_reg_type.IsLong());
  EXPECT_FALSE(double_reg_type.IsFloat());
  EXPECT_TRUE(double_reg_type.IsDouble());
  EXPECT_FALSE(double_reg_type.IsReference());
  EXPECT_TRUE(double_reg_type.IsLowHalf());
  EXPECT_FALSE(double_reg_type.IsHighHalf());
  EXPECT_TRUE(double_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(double_reg_type.IsReferenceTypes());
  EXPECT_FALSE(double_reg_type.IsCategory1Types());
  EXPECT_TRUE(double_reg_type.IsCategory2Types());
  EXPECT_FALSE(double_reg_type.IsBooleanTypes());
  EXPECT_FALSE(double_reg_type.IsByteTypes());
  EXPECT_FALSE(double_reg_type.IsShortTypes());
  EXPECT_FALSE(double_reg_type.IsCharTypes());
  EXPECT_FALSE(double_reg_type.IsIntegralTypes());
  EXPECT_FALSE(double_reg_type.IsFloatTypes());
  EXPECT_FALSE(double_reg_type.IsLongTypes());
  EXPECT_TRUE(double_reg_type.IsDoubleTypes());
  EXPECT_FALSE(double_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(double_reg_type.IsNonZeroReferenceTypes());
  EXPECT_TRUE(double_reg_type.HasClass());
}

class RegTypeReferenceTest : public CommonRuntimeTest {};

TEST_F(RegTypeReferenceTest, JavalangObjectImprecise) {
  // Tests matching precisions. A reference type that was created precise doesn't
  // match the one that is imprecise.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& imprecise_obj = cache.JavaLangObject(false);
  const RegType& precise_obj = cache.JavaLangObject(true);
  const RegType& precise_obj_2 = cache.FromDescriptor(nullptr, "Ljava/lang/Object;", true);

  EXPECT_TRUE(precise_obj.Equals(precise_obj_2));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj_2));
}

TEST_F(RegTypeReferenceTest, UnresolvedType) {
  // Tests creating unresolved types. Miss for the first time asking the cache and
  // a hit second time.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& ref_type_0 = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  EXPECT_TRUE(ref_type_0.IsNonZeroReferenceTypes());

  const RegType& ref_type_1 = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.Equals(ref_type_1));

  const RegType& unresolved_super_class =  cache.FromUnresolvedSuperClass(ref_type_0);
  EXPECT_TRUE(unresolved_super_class.IsUnresolvedSuperClass());
  EXPECT_TRUE(unresolved_super_class.IsNonZeroReferenceTypes());
}

TEST_F(RegTypeReferenceTest, UnresolvedUnintializedType) {
  // Tests creating types uninitialized types from unresolved types.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& ref_type_0 = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  const RegType& ref_type = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.Equals(ref_type));
  // Create an uninitialized type of this unresolved type
  const RegType& unresolved_unintialised = cache.Uninitialized(ref_type, 1101ull);
  EXPECT_TRUE(unresolved_unintialised.IsUnresolvedAndUninitializedReference());
  EXPECT_TRUE(unresolved_unintialised.IsUninitializedTypes());
  EXPECT_TRUE(unresolved_unintialised.IsNonZeroReferenceTypes());
  // Create an uninitialized type of this unresolved type with different  PC
  const RegType& ref_type_unresolved_unintialised_1 =  cache.Uninitialized(ref_type, 1102ull);
  EXPECT_TRUE(unresolved_unintialised.IsUnresolvedAndUninitializedReference());
  EXPECT_FALSE(unresolved_unintialised.Equals(ref_type_unresolved_unintialised_1));
  // Create an uninitialized type of this unresolved type with the same PC
  const RegType& unresolved_unintialised_2 = cache.Uninitialized(ref_type, 1101ull);
  EXPECT_TRUE(unresolved_unintialised.Equals(unresolved_unintialised_2));
}

TEST_F(RegTypeReferenceTest, Dump) {
  // Tests types for proper Dump messages.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& unresolved_ref = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  const RegType& unresolved_ref_another = cache.FromDescriptor(nullptr, "Ljava/lang/DoesNotExistEither;", true);
  const RegType& resolved_ref = cache.JavaLangString();
  const RegType& resolved_unintialiesd = cache.Uninitialized(resolved_ref, 10);
  const RegType& unresolved_unintialized = cache.Uninitialized(unresolved_ref, 12);
  const RegType& unresolved_merged = cache.FromUnresolvedMerge(
      unresolved_ref, unresolved_ref_another, /* verifier */ nullptr);

  std::string expected = "Unresolved Reference: java.lang.DoesNotExist";
  EXPECT_EQ(expected, unresolved_ref.Dump());
  expected = "Precise Reference: java.lang.String";
  EXPECT_EQ(expected, resolved_ref.Dump());
  expected ="Uninitialized Reference: java.lang.String Allocation PC: 10";
  EXPECT_EQ(expected, resolved_unintialiesd.Dump());
  expected = "Unresolved And Uninitialized Reference: java.lang.DoesNotExist Allocation PC: 12";
  EXPECT_EQ(expected, unresolved_unintialized.Dump());
  expected = "UnresolvedMergedReferences(Zero/null | Unresolved Reference: java.lang.DoesNotExist, Unresolved Reference: java.lang.DoesNotExistEither)";
  EXPECT_EQ(expected, unresolved_merged.Dump());
}

TEST_F(RegTypeReferenceTest, JavalangString) {
  // Add a class to the cache then look for the same class and make sure it is  a
  // Hit the second time. Then check for the same effect when using
  // The JavaLangObject method instead of FromDescriptor. String class is final.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& ref_type = cache.JavaLangString();
  const RegType& ref_type_2 = cache.JavaLangString();
  const RegType& ref_type_3 = cache.FromDescriptor(nullptr, "Ljava/lang/String;", true);

  EXPECT_TRUE(ref_type.Equals(ref_type_2));
  EXPECT_TRUE(ref_type_2.Equals(ref_type_3));
  EXPECT_TRUE(ref_type.IsPreciseReference());

  // Create an uninitialized type out of this:
  const RegType& ref_type_unintialized = cache.Uninitialized(ref_type, 0110ull);
  EXPECT_TRUE(ref_type_unintialized.IsUninitializedReference());
  EXPECT_FALSE(ref_type_unintialized.IsUnresolvedAndUninitializedReference());
}

TEST_F(RegTypeReferenceTest, JavalangObject) {
  // Add a class to the cache then look for the same class and make sure it is  a
  // Hit the second time. Then I am checking for the same effect when using
  // The JavaLangObject method instead of FromDescriptor. Object Class in not final.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true, allocator);
  const RegType& ref_type = cache.JavaLangObject(true);
  const RegType& ref_type_2 = cache.JavaLangObject(true);
  const RegType& ref_type_3 = cache.FromDescriptor(nullptr, "Ljava/lang/Object;", true);

  EXPECT_TRUE(ref_type.Equals(ref_type_2));
  EXPECT_TRUE(ref_type_3.Equals(ref_type_2));
  EXPECT_EQ(ref_type.GetId(), ref_type_3.GetId());
}
TEST_F(RegTypeReferenceTest, Merging) {
  // Tests merging logic
  // String and object , LUB is object.
  ScopedObjectAccess soa(Thread::Current());
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  RegTypeCache cache_new(true, allocator);
  const RegType& string = cache_new.JavaLangString();
  const RegType& Object = cache_new.JavaLangObject(true);
  EXPECT_TRUE(string.Merge(Object, &cache_new, /* verifier */ nullptr).IsJavaLangObject());
  // Merge two unresolved types.
  const RegType& ref_type_0 = cache_new.FromDescriptor(nullptr, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  const RegType& ref_type_1 = cache_new.FromDescriptor(nullptr, "Ljava/lang/DoesNotExistToo;", true);
  EXPECT_FALSE(ref_type_0.Equals(ref_type_1));

  const RegType& merged = ref_type_1.Merge(ref_type_0, &cache_new, /* verifier */ nullptr);
  EXPECT_TRUE(merged.IsUnresolvedMergedReference());
  RegType& merged_nonconst = const_cast<RegType&>(merged);

  const BitVector& unresolved_parts =
      down_cast<UnresolvedMergedType*>(&merged_nonconst)->GetUnresolvedTypes();
  EXPECT_TRUE(unresolved_parts.IsBitSet(ref_type_0.GetId()));
  EXPECT_TRUE(unresolved_parts.IsBitSet(ref_type_1.GetId()));
}

TEST_F(RegTypeTest, MergingFloat) {
  // Testing merging logic with float and float constants.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true, allocator);

  constexpr int32_t kTestConstantValue = 10;
  const RegType& float_type = cache_new.Float();
  const RegType& precise_cst = cache_new.FromCat1Const(kTestConstantValue, true);
  const RegType& imprecise_cst = cache_new.FromCat1Const(kTestConstantValue, false);
  {
    // float MERGE precise cst => float.
    const RegType& merged = float_type.Merge(precise_cst, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsFloat());
  }
  {
    // precise cst MERGE float => float.
    const RegType& merged = precise_cst.Merge(float_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsFloat());
  }
  {
    // float MERGE imprecise cst => float.
    const RegType& merged = float_type.Merge(imprecise_cst, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsFloat());
  }
  {
    // imprecise cst MERGE float => float.
    const RegType& merged = imprecise_cst.Merge(float_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsFloat());
  }
}

TEST_F(RegTypeTest, MergingLong) {
  // Testing merging logic with long and long constants.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true, allocator);

  constexpr int32_t kTestConstantValue = 10;
  const RegType& long_lo_type = cache_new.LongLo();
  const RegType& long_hi_type = cache_new.LongHi();
  const RegType& precise_cst_lo = cache_new.FromCat2ConstLo(kTestConstantValue, true);
  const RegType& imprecise_cst_lo = cache_new.FromCat2ConstLo(kTestConstantValue, false);
  const RegType& precise_cst_hi = cache_new.FromCat2ConstHi(kTestConstantValue, true);
  const RegType& imprecise_cst_hi = cache_new.FromCat2ConstHi(kTestConstantValue, false);
  {
    // lo MERGE precise cst lo => lo.
    const RegType& merged = long_lo_type.Merge(precise_cst_lo, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongLo());
  }
  {
    // precise cst lo MERGE lo => lo.
    const RegType& merged = precise_cst_lo.Merge(long_lo_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongLo());
  }
  {
    // lo MERGE imprecise cst lo => lo.
    const RegType& merged = long_lo_type.Merge(
        imprecise_cst_lo, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongLo());
  }
  {
    // imprecise cst lo MERGE lo => lo.
    const RegType& merged = imprecise_cst_lo.Merge(
        long_lo_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongLo());
  }
  {
    // hi MERGE precise cst hi => hi.
    const RegType& merged = long_hi_type.Merge(precise_cst_hi, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongHi());
  }
  {
    // precise cst hi MERGE hi => hi.
    const RegType& merged = precise_cst_hi.Merge(long_hi_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongHi());
  }
  {
    // hi MERGE imprecise cst hi => hi.
    const RegType& merged = long_hi_type.Merge(
        imprecise_cst_hi, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongHi());
  }
  {
    // imprecise cst hi MERGE hi => hi.
    const RegType& merged = imprecise_cst_hi.Merge(
        long_hi_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsLongHi());
  }
}

TEST_F(RegTypeTest, MergingDouble) {
  // Testing merging logic with double and double constants.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true, allocator);

  constexpr int32_t kTestConstantValue = 10;
  const RegType& double_lo_type = cache_new.DoubleLo();
  const RegType& double_hi_type = cache_new.DoubleHi();
  const RegType& precise_cst_lo = cache_new.FromCat2ConstLo(kTestConstantValue, true);
  const RegType& imprecise_cst_lo = cache_new.FromCat2ConstLo(kTestConstantValue, false);
  const RegType& precise_cst_hi = cache_new.FromCat2ConstHi(kTestConstantValue, true);
  const RegType& imprecise_cst_hi = cache_new.FromCat2ConstHi(kTestConstantValue, false);
  {
    // lo MERGE precise cst lo => lo.
    const RegType& merged = double_lo_type.Merge(
        precise_cst_lo, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleLo());
  }
  {
    // precise cst lo MERGE lo => lo.
    const RegType& merged = precise_cst_lo.Merge(
        double_lo_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleLo());
  }
  {
    // lo MERGE imprecise cst lo => lo.
    const RegType& merged = double_lo_type.Merge(
        imprecise_cst_lo, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleLo());
  }
  {
    // imprecise cst lo MERGE lo => lo.
    const RegType& merged = imprecise_cst_lo.Merge(
        double_lo_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleLo());
  }
  {
    // hi MERGE precise cst hi => hi.
    const RegType& merged = double_hi_type.Merge(
        precise_cst_hi, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleHi());
  }
  {
    // precise cst hi MERGE hi => hi.
    const RegType& merged = precise_cst_hi.Merge(
        double_hi_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleHi());
  }
  {
    // hi MERGE imprecise cst hi => hi.
    const RegType& merged = double_hi_type.Merge(
        imprecise_cst_hi, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleHi());
  }
  {
    // imprecise cst hi MERGE hi => hi.
    const RegType& merged = imprecise_cst_hi.Merge(
        double_hi_type, &cache_new, /* verifier */ nullptr);
    EXPECT_TRUE(merged.IsDoubleHi());
  }
}

TEST_F(RegTypeTest, MergeSemiLatticeRef) {
  //  (Incomplete) semilattice:
  //
  //  Excluded for now: * category-2 types
  //                    * interfaces
  //                    * all of category-1 primitive types, including constants.
  //  This is to demonstrate/codify the reference side, mostly.
  //
  //  Note: It is not a real semilattice because int = float makes this wonky. :-(
  //
  //                                       Conflict
  //                                           |
  //      #---------#--------------------------#-----------------------------#
  //      |         |                                                        |
  //      |         |                                                      Object
  //      |         |                                                        |
  //     int   uninit types              #---------------#--------#------------------#---------#
  //      |                              |               |        |                  |         |
  //      |                  unresolved-merge-types      |      Object[]           char[]   byte[]
  //      |                              |    |  |       |        |                  |         |
  //      |                  unresolved-types |  #------Number    #---------#        |         |
  //      |                              |    |          |        |         |        |         |
  //      |                              |    #--------Integer  Number[] Number[][]  |         |
  //      |                              |               |        |         |        |         |
  //      |                              #---------------#--------#---------#--------#---------#
  //      |                                                       |
  //      |                                                     null
  //      |                                                       |
  //      #--------------------------#----------------------------#
  //                                 |
  //                                 0

  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());

  // We cannot allow moving GC. Otherwise we'd have to ensure the reg types are updated (reference
  // reg types store a class pointer in a GCRoot, which is normally updated through active verifiers
  // being registered with their thread), which is unnecessarily complex.
  Runtime::Current()->GetHeap()->IncrementDisableMovingGC(soa.Self());

  RegTypeCache cache(true, allocator);

  const RegType& conflict = cache.Conflict();
  const RegType& zero = cache.Zero();
  const RegType& null = cache.Null();
  const RegType& int_type = cache.Integer();

  const RegType& obj = cache.JavaLangObject(false);
  const RegType& obj_arr = cache.From(nullptr, "[Ljava/lang/Object;", false);
  ASSERT_FALSE(obj_arr.IsUnresolvedReference());

  const RegType& unresolved_a = cache.From(nullptr, "Ldoes/not/resolve/A;", false);
  ASSERT_TRUE(unresolved_a.IsUnresolvedReference());
  const RegType& unresolved_b = cache.From(nullptr, "Ldoes/not/resolve/B;", false);
  ASSERT_TRUE(unresolved_b.IsUnresolvedReference());
  const RegType& unresolved_ab = cache.FromUnresolvedMerge(unresolved_a, unresolved_b, nullptr);
  ASSERT_TRUE(unresolved_ab.IsUnresolvedMergedReference());

  const RegType& uninit_this = cache.UninitializedThisArgument(obj);
  const RegType& uninit_obj_0 = cache.Uninitialized(obj, 0u);
  const RegType& uninit_obj_1 = cache.Uninitialized(obj, 1u);

  const RegType& uninit_unres_this = cache.UninitializedThisArgument(unresolved_a);
  const RegType& uninit_unres_a_0 = cache.Uninitialized(unresolved_a, 0);
  const RegType& uninit_unres_b_0 = cache.Uninitialized(unresolved_b, 0);

  const RegType& number = cache.From(nullptr, "Ljava/lang/Number;", false);
  ASSERT_FALSE(number.IsUnresolvedReference());
  const RegType& integer = cache.From(nullptr, "Ljava/lang/Integer;", false);
  ASSERT_FALSE(integer.IsUnresolvedReference());

  const RegType& uninit_number_0 = cache.Uninitialized(number, 0u);
  const RegType& uninit_integer_0 = cache.Uninitialized(integer, 0u);

  const RegType& number_arr = cache.From(nullptr, "[Ljava/lang/Number;", false);
  ASSERT_FALSE(number_arr.IsUnresolvedReference());
  const RegType& integer_arr = cache.From(nullptr, "[Ljava/lang/Integer;", false);
  ASSERT_FALSE(integer_arr.IsUnresolvedReference());

  const RegType& number_arr_arr = cache.From(nullptr, "[[Ljava/lang/Number;", false);
  ASSERT_FALSE(number_arr_arr.IsUnresolvedReference());

  const RegType& char_arr = cache.From(nullptr, "[C", false);
  ASSERT_FALSE(char_arr.IsUnresolvedReference());
  const RegType& byte_arr = cache.From(nullptr, "[B", false);
  ASSERT_FALSE(byte_arr.IsUnresolvedReference());

  const RegType& unresolved_a_num = cache.FromUnresolvedMerge(unresolved_a, number, nullptr);
  ASSERT_TRUE(unresolved_a_num.IsUnresolvedMergedReference());
  const RegType& unresolved_b_num = cache.FromUnresolvedMerge(unresolved_b, number, nullptr);
  ASSERT_TRUE(unresolved_b_num.IsUnresolvedMergedReference());
  const RegType& unresolved_ab_num = cache.FromUnresolvedMerge(unresolved_ab, number, nullptr);
  ASSERT_TRUE(unresolved_ab_num.IsUnresolvedMergedReference());

  const RegType& unresolved_a_int = cache.FromUnresolvedMerge(unresolved_a, integer, nullptr);
  ASSERT_TRUE(unresolved_a_int.IsUnresolvedMergedReference());
  const RegType& unresolved_b_int = cache.FromUnresolvedMerge(unresolved_b, integer, nullptr);
  ASSERT_TRUE(unresolved_b_int.IsUnresolvedMergedReference());
  const RegType& unresolved_ab_int = cache.FromUnresolvedMerge(unresolved_ab, integer, nullptr);
  ASSERT_TRUE(unresolved_ab_int.IsUnresolvedMergedReference());
  std::vector<const RegType*> uninitialized_types = {
      &uninit_this, &uninit_obj_0, &uninit_obj_1, &uninit_number_0, &uninit_integer_0
  };
  std::vector<const RegType*> unresolved_types = {
      &unresolved_a,
      &unresolved_b,
      &unresolved_ab,
      &unresolved_a_num,
      &unresolved_b_num,
      &unresolved_ab_num,
      &unresolved_a_int,
      &unresolved_b_int,
      &unresolved_ab_int
  };
  std::vector<const RegType*> uninit_unresolved_types = {
      &uninit_unres_this, &uninit_unres_a_0, &uninit_unres_b_0
  };
  std::vector<const RegType*> plain_nonobj_classes = { &number, &integer };
  std::vector<const RegType*> plain_nonobj_arr_classes = {
      &number_arr,
      &number_arr_arr,
      &integer_arr,
      &char_arr,
  };
  // std::vector<const RegType*> others = { &conflict, &zero, &null, &obj, &int_type };

  std::vector<const RegType*> all_minus_uninit_conflict;
  all_minus_uninit_conflict.insert(all_minus_uninit_conflict.end(),
                                   unresolved_types.begin(),
                                   unresolved_types.end());
  all_minus_uninit_conflict.insert(all_minus_uninit_conflict.end(),
                                   plain_nonobj_classes.begin(),
                                   plain_nonobj_classes.end());
  all_minus_uninit_conflict.insert(all_minus_uninit_conflict.end(),
                                   plain_nonobj_arr_classes.begin(),
                                   plain_nonobj_arr_classes.end());
  all_minus_uninit_conflict.push_back(&zero);
  all_minus_uninit_conflict.push_back(&null);
  all_minus_uninit_conflict.push_back(&obj);

  std::vector<const RegType*> all_minus_uninit;
  all_minus_uninit.insert(all_minus_uninit.end(),
                          all_minus_uninit_conflict.begin(),
                          all_minus_uninit_conflict.end());
  all_minus_uninit.push_back(&conflict);


  std::vector<const RegType*> all;
  all.insert(all.end(), uninitialized_types.begin(), uninitialized_types.end());
  all.insert(all.end(), uninit_unresolved_types.begin(), uninit_unresolved_types.end());
  all.insert(all.end(), all_minus_uninit.begin(), all_minus_uninit.end());
  all.push_back(&int_type);

  auto check = [&](const RegType& in1, const RegType& in2, const RegType& expected_out)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const RegType& merge_result = in1.SafeMerge(in2, &cache, nullptr);
    EXPECT_EQ(&expected_out, &merge_result)
        << in1.Dump() << " x " << in2.Dump() << " = " << merge_result.Dump()
        << " != " << expected_out.Dump();
  };

  // Identity.
  {
    for (auto r : all) {
      check(*r, *r, *r);
    }
  }

  // Define a covering relation through a list of Edges. We'll then derive LUBs from this and
  // create checks for every pair of types.

  struct Edge {
    const RegType& from;
    const RegType& to;

    Edge(const RegType& from_, const RegType& to_) : from(from_), to(to_) {}
  };
  std::vector<Edge> edges;
#define ADD_EDGE(from, to) edges.emplace_back((from), (to))

  // To Conflict.
  {
    for (auto r : uninitialized_types) {
      ADD_EDGE(*r, conflict);
    }
    for (auto r : uninit_unresolved_types) {
      ADD_EDGE(*r, conflict);
    }
    ADD_EDGE(obj, conflict);
    ADD_EDGE(int_type, conflict);
  }

  ADD_EDGE(zero, null);

  // Unresolved.
  {
    ADD_EDGE(null, unresolved_a);
    ADD_EDGE(null, unresolved_b);
    ADD_EDGE(unresolved_a, unresolved_ab);
    ADD_EDGE(unresolved_b, unresolved_ab);

    ADD_EDGE(number, unresolved_a_num);
    ADD_EDGE(unresolved_a, unresolved_a_num);
    ADD_EDGE(number, unresolved_b_num);
    ADD_EDGE(unresolved_b, unresolved_b_num);
    ADD_EDGE(number, unresolved_ab_num);
    ADD_EDGE(unresolved_a_num, unresolved_ab_num);
    ADD_EDGE(unresolved_b_num, unresolved_ab_num);
    ADD_EDGE(unresolved_ab, unresolved_ab_num);

    ADD_EDGE(integer, unresolved_a_int);
    ADD_EDGE(unresolved_a, unresolved_a_int);
    ADD_EDGE(integer, unresolved_b_int);
    ADD_EDGE(unresolved_b, unresolved_b_int);
    ADD_EDGE(integer, unresolved_ab_int);
    ADD_EDGE(unresolved_a_int, unresolved_ab_int);
    ADD_EDGE(unresolved_b_int, unresolved_ab_int);
    ADD_EDGE(unresolved_ab, unresolved_ab_int);

    ADD_EDGE(unresolved_a_int, unresolved_a_num);
    ADD_EDGE(unresolved_b_int, unresolved_b_num);
    ADD_EDGE(unresolved_ab_int, unresolved_ab_num);

    ADD_EDGE(unresolved_ab_num, obj);
  }

  // Classes.
  {
    ADD_EDGE(null, integer);
    ADD_EDGE(integer, number);
    ADD_EDGE(number, obj);
  }

  // Arrays.
  {
    ADD_EDGE(integer_arr, number_arr);
    ADD_EDGE(number_arr, obj_arr);
    ADD_EDGE(obj_arr, obj);
    ADD_EDGE(number_arr_arr, obj_arr);

    ADD_EDGE(char_arr, obj);
    ADD_EDGE(byte_arr, obj);

    ADD_EDGE(null, integer_arr);
    ADD_EDGE(null, number_arr_arr);
    ADD_EDGE(null, char_arr);
    ADD_EDGE(null, byte_arr);
  }

  // Primitive.
  {
    ADD_EDGE(zero, int_type);
  }
#undef ADD_EDGE

  // Create merge triples by using the covering relation established by edges to derive the
  // expected merge for any pair of types.

  // Expect merge(in1, in2) == out.
  struct MergeExpectation {
    const RegType& in1;
    const RegType& in2;
    const RegType& out;

    MergeExpectation(const RegType& in1_, const RegType& in2_, const RegType& out_)
        : in1(in1_), in2(in2_), out(out_) {}
  };
  std::vector<MergeExpectation> expectations;

  for (auto r1 : all) {
    for (auto r2 : all) {
      if (r1 == r2) {
        continue;
      }

      // Very simple algorithm here that is usually used with adjacency lists. Our graph is
      // small, it didn't make sense to have lists per node. Thus, the regular guarantees
      // of O(n + |e|) don't apply, but that is acceptable.
      //
      // To compute r1 lub r2 = merge(r1, r2):
      //   1) Generate the reachable set of r1, name it grey.
      //   2) Mark all grey reachable nodes of r2 as black.
      //   3) Find black nodes with no in-edges from other black nodes.
      //   4) If |3)| == 1, that's the lub.

      // Generic BFS of the graph induced by edges, starting at start. new_node will be called
      // with any discovered node, in order.
      auto bfs = [&](auto new_node, const RegType* start) {
        std::unordered_set<const RegType*> seen;
        std::queue<const RegType*> work_list;
        work_list.push(start);
        while (!work_list.empty()) {
          const RegType* cur = work_list.front();
          work_list.pop();
          auto it = seen.find(cur);
          if (it != seen.end()) {
            continue;
          }
          seen.insert(cur);
          new_node(cur);

          for (const Edge& edge : edges) {
            if (&edge.from == cur) {
              work_list.push(&edge.to);
            }
          }
        }
      };

      std::unordered_set<const RegType*> grey;
      auto compute_grey = [&](const RegType* cur) {
        grey.insert(cur);  // Mark discovered node as grey.
      };
      bfs(compute_grey, r1);

      std::set<const RegType*> black;
      auto compute_black = [&](const RegType* cur) {
        // Mark discovered grey node as black.
        if (grey.find(cur) != grey.end()) {
          black.insert(cur);
        }
      };
      bfs(compute_black, r2);

      std::set<const RegType*> no_in_edge(black);  // Copy of black, remove nodes with in-edges.
      for (auto r : black) {
        for (Edge& e : edges) {
          if (&e.from == r) {
            no_in_edge.erase(&e.to);  // It doesn't matter whether "to" is black or not, just
                                      // attempt to remove it.
          }
        }
      }

      // Helper to print sets when something went wrong.
      auto print_set = [](auto& container) REQUIRES_SHARED(Locks::mutator_lock_) {
        std::string result;
        for (auto r : container) {
          result.append(" + ");
          result.append(r->Dump());
        }
        return result;
      };
      ASSERT_EQ(no_in_edge.size(), 1u) << r1->Dump() << " u " << r2->Dump()
                                       << " grey=" << print_set(grey)
                                       << " black=" << print_set(black)
                                       << " no-in-edge=" << print_set(no_in_edge);
      expectations.emplace_back(*r1, *r2, **no_in_edge.begin());
    }
  }

  // Evaluate merge expectations. The merge is expected to be commutative.

  for (auto& triple : expectations) {
    check(triple.in1, triple.in2, triple.out);
    check(triple.in2, triple.in1, triple.out);
  }

  Runtime::Current()->GetHeap()->DecrementDisableMovingGC(soa.Self());
}

TEST_F(RegTypeTest, ConstPrecision) {
  // Tests creating primitive types types.
  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true, allocator);
  const RegType& imprecise_const = cache_new.FromCat1Const(10, false);
  const RegType& precise_const = cache_new.FromCat1Const(10, true);

  EXPECT_TRUE(imprecise_const.IsImpreciseConstant());
  EXPECT_TRUE(precise_const.IsPreciseConstant());
  EXPECT_FALSE(imprecise_const.Equals(precise_const));
}

class RegTypeOOMTest : public RegTypeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions *options) OVERRIDE {
    SetUpRuntimeOptionsForFillHeap(options);

    // We must not appear to be a compiler, or we'll abort on the host.
    callbacks_.reset();
  }
};

TEST_F(RegTypeOOMTest, ClassJoinOOM) {
  // TODO: Figure out why FillHeap isn't good enough under CMS.
  TEST_DISABLED_WITHOUT_BAKER_READ_BARRIERS();

  // Tests that we don't abort with OOMs.

  ArenaStack stack(Runtime::Current()->GetArenaPool());
  ScopedArenaAllocator allocator(&stack);
  ScopedObjectAccess soa(Thread::Current());

  // We cannot allow moving GC. Otherwise we'd have to ensure the reg types are updated (reference
  // reg types store a class pointer in a GCRoot, which is normally updated through active verifiers
  // being registered with their thread), which is unnecessarily complex.
  Runtime::Current()->GetHeap()->IncrementDisableMovingGC(soa.Self());

  // We merge nested array of primitive wrappers. These have a join type of an array of Number of
  // the same depth. We start with depth five, as we want at least two newly created classes to
  // test recursion (it's just more likely that nobody uses such deep arrays in runtime bringup).
  constexpr const char* kIntArrayFive = "[[[[[Ljava/lang/Integer;";
  constexpr const char* kFloatArrayFive = "[[[[[Ljava/lang/Float;";
  constexpr const char* kNumberArrayFour = "[[[[Ljava/lang/Number;";
  constexpr const char* kNumberArrayFive = "[[[[[Ljava/lang/Number;";

  RegTypeCache cache(true, allocator);
  const RegType& int_array_array = cache.From(nullptr, kIntArrayFive, false);
  ASSERT_TRUE(int_array_array.HasClass());
  const RegType& float_array_array = cache.From(nullptr, kFloatArrayFive, false);
  ASSERT_TRUE(float_array_array.HasClass());

  // Check assumptions: the joined classes don't exist, yet.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ASSERT_TRUE(class_linker->LookupClass(soa.Self(), kNumberArrayFour, nullptr) == nullptr);
  ASSERT_TRUE(class_linker->LookupClass(soa.Self(), kNumberArrayFive, nullptr) == nullptr);

  // Fill the heap.
  VariableSizedHandleScope hs(soa.Self());
  FillHeap(soa.Self(), class_linker, &hs);

  const RegType& join_type = int_array_array.Merge(float_array_array, &cache, nullptr);
  ASSERT_TRUE(join_type.IsUnresolvedReference());

  Runtime::Current()->GetHeap()->DecrementDisableMovingGC(soa.Self());
}

}  // namespace verifier
}  // namespace art
