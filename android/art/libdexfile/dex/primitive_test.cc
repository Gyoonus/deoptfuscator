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

#include "dex/primitive.h"

#include "gtest/gtest.h"

namespace art {

namespace {

void CheckPrimitiveTypeWidensTo(Primitive::Type from,
                                const std::vector<Primitive::Type>& expected_to_types) {
  std::vector<Primitive::Type> actual_to_types;
  int last = static_cast<int>(Primitive::Type::kPrimLast);
  for (int i = 0; i <= last; ++i) {
    Primitive::Type to = static_cast<Primitive::Type>(i);
    if (Primitive::IsWidenable(from, to)) {
      actual_to_types.push_back(to);
    }
  }
  EXPECT_EQ(expected_to_types, actual_to_types);
}

}  // namespace

TEST(PrimitiveTest, NotWidensTo) {
  const std::vector<Primitive::Type> to_types = {};
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimNot, to_types);
}

TEST(PrimitiveTest, BooleanWidensTo) {
  const std::vector<Primitive::Type> to_types = {};
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimBoolean, to_types);
}

TEST(PrimitiveTest, ByteWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimByte,
    Primitive::Type::kPrimShort,
    Primitive::Type::kPrimInt,
    Primitive::Type::kPrimLong,
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimByte, to_types);
}

TEST(PrimitiveTest, CharWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimChar,
    Primitive::Type::kPrimInt,
    Primitive::Type::kPrimLong,
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimChar, to_types);
}

TEST(PrimitiveTest, ShortWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimShort,
    Primitive::Type::kPrimInt,
    Primitive::Type::kPrimLong,
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimShort, to_types);
}

TEST(PrimitiveTest, IntWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimInt,
    Primitive::Type::kPrimLong,
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimInt, to_types);
}

TEST(PrimitiveTest, LongWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimLong,
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimLong, to_types);
}

TEST(PrimitiveTest, FloatWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimFloat,
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimFloat, to_types);
}

TEST(PrimitiveTest, DoubleWidensTo) {
  const std::vector<Primitive::Type> to_types = {
    Primitive::Type::kPrimDouble,
  };
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimDouble, to_types);
}

TEST(PrimitiveTest, VoidWidensTo) {
  const std::vector<Primitive::Type> to_types = {};
  CheckPrimitiveTypeWidensTo(Primitive::Type::kPrimVoid, to_types);
}

}  // namespace art
