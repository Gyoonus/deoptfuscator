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

#include <gtest/gtest.h>

#include "data_type-inl.h"

#include "base/array_ref.h"
#include "base/macros.h"
#include "dex/primitive.h"

namespace art {

template <DataType::Type data_type, Primitive::Type primitive_type>
static void CheckConversion() {
  static_assert(data_type == DataTypeFromPrimitive(primitive_type), "Conversion check.");
  static_assert(DataType::Size(data_type) == Primitive::ComponentSize(primitive_type),
                "Size check.");
}

TEST(DataType, SizeAgainstPrimitive) {
  CheckConversion<DataType::Type::kVoid, Primitive::kPrimVoid>();
  CheckConversion<DataType::Type::kBool, Primitive::kPrimBoolean>();
  CheckConversion<DataType::Type::kInt8, Primitive::kPrimByte>();
  CheckConversion<DataType::Type::kUint16, Primitive::kPrimChar>();
  CheckConversion<DataType::Type::kInt16, Primitive::kPrimShort>();
  CheckConversion<DataType::Type::kInt32, Primitive::kPrimInt>();
  CheckConversion<DataType::Type::kInt64, Primitive::kPrimLong>();
  CheckConversion<DataType::Type::kFloat32, Primitive::kPrimFloat>();
  CheckConversion<DataType::Type::kFloat64, Primitive::kPrimDouble>();
  CheckConversion<DataType::Type::kReference, Primitive::kPrimNot>();
}

TEST(DataType, Names) {
#define CHECK_NAME(type) EXPECT_STREQ(#type, DataType::PrettyDescriptor(DataType::Type::k##type))
  CHECK_NAME(Void);
  CHECK_NAME(Bool);
  CHECK_NAME(Int8);
  CHECK_NAME(Uint16);
  CHECK_NAME(Int16);
  CHECK_NAME(Int32);
  CHECK_NAME(Int64);
  CHECK_NAME(Float32);
  CHECK_NAME(Float64);
  CHECK_NAME(Reference);
#undef CHECK_NAME
}

TEST(DataType, IsTypeConversionImplicit) {
  static const DataType::Type kIntegralTypes[] = {
      DataType::Type::kBool,
      DataType::Type::kUint8,
      DataType::Type::kInt8,
      DataType::Type::kUint16,
      DataType::Type::kInt16,
      DataType::Type::kInt32,
      DataType::Type::kInt64,
  };
  const ArrayRef<const DataType::Type> kIntegralInputTypes(kIntegralTypes);
  // Note: kBool cannot be used as a result type.
  DCHECK_EQ(kIntegralTypes[0], DataType::Type::kBool);
  const ArrayRef<const DataType::Type> kIntegralResultTypes = kIntegralInputTypes.SubArray(1u);

  static const bool kImplicitIntegralConversions[][arraysize(kIntegralTypes)] = {
      //             Bool   Uint8   Int8 Uint16  Int16  Int32  Int64
      { /*   Bool    N/A */  true,  true,  true,  true,  true, false },
      { /*  Uint8    N/A */  true, false,  true,  true,  true, false },
      { /*   Int8    N/A */ false,  true, false,  true,  true, false },
      { /* Uint16    N/A */ false, false,  true, false,  true, false },
      { /*  Int16    N/A */ false, false, false,  true,  true, false },
      { /*  Int32    N/A */ false, false, false, false,  true, false },
      { /*  Int64    N/A */ false, false, false, false, false,  true },
  };
  static_assert(arraysize(kIntegralTypes) == arraysize(kImplicitIntegralConversions), "size check");

  for (size_t input_index = 0; input_index != kIntegralInputTypes.size(); ++input_index) {
    DataType::Type input_type = kIntegralInputTypes[input_index];
    for (size_t result_index = 1u; result_index != kIntegralResultTypes.size(); ++result_index) {
      DataType::Type result_type = kIntegralResultTypes[result_index];
      EXPECT_EQ(kImplicitIntegralConversions[input_index][result_index],
                DataType::IsTypeConversionImplicit(input_type, result_type))
          << input_type << " " << result_type;
    }
  }
  for (DataType::Type input_type : kIntegralInputTypes) {
    EXPECT_FALSE(DataType::IsTypeConversionImplicit(input_type, DataType::Type::kFloat32));
    EXPECT_FALSE(DataType::IsTypeConversionImplicit(input_type, DataType::Type::kFloat64));
  }
  for (DataType::Type result_type : kIntegralResultTypes) {
    EXPECT_FALSE(DataType::IsTypeConversionImplicit(DataType::Type::kFloat32, result_type));
    EXPECT_FALSE(DataType::IsTypeConversionImplicit(DataType::Type::kFloat64, result_type));
  }
  EXPECT_TRUE(
      DataType::IsTypeConversionImplicit(DataType::Type::kFloat32, DataType::Type::kFloat32));
  EXPECT_FALSE(
      DataType::IsTypeConversionImplicit(DataType::Type::kFloat32, DataType::Type::kFloat64));
  EXPECT_FALSE(
      DataType::IsTypeConversionImplicit(DataType::Type::kFloat64, DataType::Type::kFloat32));
  EXPECT_TRUE(
      DataType::IsTypeConversionImplicit(DataType::Type::kFloat64, DataType::Type::kFloat64));
}

}  // namespace art
