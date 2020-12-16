/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not read this file except in compliance with the License.
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

#include "data_type.h"
#include "nodes.h"

namespace art {

// Only runtime types other than void are allowed.
static const DataType::Type kTestTypes[] = {
    DataType::Type::kReference,
    DataType::Type::kBool,
    DataType::Type::kInt8,
    DataType::Type::kUint16,
    DataType::Type::kInt16,
    DataType::Type::kInt32,
    DataType::Type::kInt64,
    DataType::Type::kFloat32,
    DataType::Type::kFloat64,
};

/**
 * Tests for the SideEffects class.
 */

//
// Helper methods.
//

void testWriteAndReadSanity(SideEffects write, SideEffects read) {
  EXPECT_FALSE(write.DoesNothing());
  EXPECT_FALSE(read.DoesNothing());

  EXPECT_TRUE(write.DoesAnyWrite());
  EXPECT_FALSE(write.DoesAnyRead());
  EXPECT_FALSE(read.DoesAnyWrite());
  EXPECT_TRUE(read.DoesAnyRead());

  // All-dependences.
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.MayDependOn(write));
  EXPECT_FALSE(write.MayDependOn(all));
  EXPECT_FALSE(all.MayDependOn(read));
  EXPECT_TRUE(read.MayDependOn(all));

  // None-dependences.
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.MayDependOn(write));
  EXPECT_FALSE(write.MayDependOn(none));
  EXPECT_FALSE(none.MayDependOn(read));
  EXPECT_FALSE(read.MayDependOn(none));
}

void testWriteAndReadDependence(SideEffects write, SideEffects read) {
  testWriteAndReadSanity(write, read);

  // Dependence only in one direction.
  EXPECT_FALSE(write.MayDependOn(read));
  EXPECT_TRUE(read.MayDependOn(write));
}

void testNoWriteAndReadDependence(SideEffects write, SideEffects read) {
  testWriteAndReadSanity(write, read);

  // No dependence in any direction.
  EXPECT_FALSE(write.MayDependOn(read));
  EXPECT_FALSE(read.MayDependOn(write));
}

//
// Actual tests.
//

TEST(SideEffectsTest, All) {
  SideEffects all = SideEffects::All();
  EXPECT_TRUE(all.DoesAnyWrite());
  EXPECT_TRUE(all.DoesAnyRead());
  EXPECT_FALSE(all.DoesNothing());
  EXPECT_TRUE(all.DoesAllReadWrite());
}

TEST(SideEffectsTest, None) {
  SideEffects none = SideEffects::None();
  EXPECT_FALSE(none.DoesAnyWrite());
  EXPECT_FALSE(none.DoesAnyRead());
  EXPECT_TRUE(none.DoesNothing());
  EXPECT_FALSE(none.DoesAllReadWrite());
}

TEST(SideEffectsTest, DependencesAndNoDependences) {
  // Apply test to each individual data type.
  for (DataType::Type type : kTestTypes) {
    // Same data type and access type: proper write/read dep.
    testWriteAndReadDependence(
        SideEffects::FieldWriteOfType(type, false),
        SideEffects::FieldReadOfType(type, false));
    testWriteAndReadDependence(
        SideEffects::ArrayWriteOfType(type),
        SideEffects::ArrayReadOfType(type));
    // Same data type but different access type: no write/read dep.
    testNoWriteAndReadDependence(
        SideEffects::FieldWriteOfType(type, false),
        SideEffects::ArrayReadOfType(type));
    testNoWriteAndReadDependence(
        SideEffects::ArrayWriteOfType(type),
        SideEffects::FieldReadOfType(type, false));
  }
}

TEST(SideEffectsTest, NoDependences) {
  // Different data type, same access type: no write/read dep.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfType(DataType::Type::kInt32, false),
      SideEffects::FieldReadOfType(DataType::Type::kFloat64, false));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      SideEffects::ArrayReadOfType(DataType::Type::kFloat64));
  // Everything different: no write/read dep.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfType(DataType::Type::kInt32, false),
      SideEffects::ArrayReadOfType(DataType::Type::kFloat64));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      SideEffects::FieldReadOfType(DataType::Type::kFloat64, false));
}

TEST(SideEffectsTest, VolatileDependences) {
  SideEffects volatile_write =
      SideEffects::FieldWriteOfType(DataType::Type::kInt32, /* is_volatile */ true);
  SideEffects any_write =
      SideEffects::FieldWriteOfType(DataType::Type::kInt32, /* is_volatile */ false);
  SideEffects volatile_read =
      SideEffects::FieldReadOfType(DataType::Type::kInt8, /* is_volatile */ true);
  SideEffects any_read =
      SideEffects::FieldReadOfType(DataType::Type::kInt8, /* is_volatile */ false);

  EXPECT_FALSE(volatile_write.MayDependOn(any_read));
  EXPECT_TRUE(any_read.MayDependOn(volatile_write));
  EXPECT_TRUE(volatile_write.MayDependOn(any_write));
  EXPECT_FALSE(any_write.MayDependOn(volatile_write));

  EXPECT_FALSE(volatile_read.MayDependOn(any_read));
  EXPECT_TRUE(any_read.MayDependOn(volatile_read));
  EXPECT_TRUE(volatile_read.MayDependOn(any_write));
  EXPECT_FALSE(any_write.MayDependOn(volatile_read));
}

TEST(SideEffectsTest, SameWidthTypesNoAlias) {
  // Type I/F.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfType(DataType::Type::kInt32, /* is_volatile */ false),
      SideEffects::FieldReadOfType(DataType::Type::kFloat32, /* is_volatile */ false));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      SideEffects::ArrayReadOfType(DataType::Type::kFloat32));
  // Type L/D.
  testNoWriteAndReadDependence(
      SideEffects::FieldWriteOfType(DataType::Type::kInt64, /* is_volatile */ false),
      SideEffects::FieldReadOfType(DataType::Type::kFloat64, /* is_volatile */ false));
  testNoWriteAndReadDependence(
      SideEffects::ArrayWriteOfType(DataType::Type::kInt64),
      SideEffects::ArrayReadOfType(DataType::Type::kFloat64));
}

TEST(SideEffectsTest, AllWritesAndReads) {
  SideEffects s = SideEffects::None();
  // Keep taking the union of different writes and reads.
  for (DataType::Type type : kTestTypes) {
    s = s.Union(SideEffects::FieldWriteOfType(type, /* is_volatile */ false));
    s = s.Union(SideEffects::ArrayWriteOfType(type));
    s = s.Union(SideEffects::FieldReadOfType(type, /* is_volatile */ false));
    s = s.Union(SideEffects::ArrayReadOfType(type));
  }
  EXPECT_TRUE(s.DoesAllReadWrite());
}

TEST(SideEffectsTest, GC) {
  SideEffects can_trigger_gc = SideEffects::CanTriggerGC();
  SideEffects depends_on_gc = SideEffects::DependsOnGC();
  SideEffects all_changes = SideEffects::AllChanges();
  SideEffects all_dependencies = SideEffects::AllDependencies();

  EXPECT_TRUE(depends_on_gc.MayDependOn(can_trigger_gc));
  EXPECT_TRUE(depends_on_gc.Union(can_trigger_gc).MayDependOn(can_trigger_gc));
  EXPECT_FALSE(can_trigger_gc.MayDependOn(depends_on_gc));

  EXPECT_TRUE(depends_on_gc.MayDependOn(all_changes));
  EXPECT_TRUE(depends_on_gc.Union(can_trigger_gc).MayDependOn(all_changes));
  EXPECT_FALSE(can_trigger_gc.MayDependOn(all_changes));

  EXPECT_TRUE(all_changes.Includes(can_trigger_gc));
  EXPECT_FALSE(all_changes.Includes(depends_on_gc));
  EXPECT_TRUE(all_dependencies.Includes(depends_on_gc));
  EXPECT_FALSE(all_dependencies.Includes(can_trigger_gc));
}

TEST(SideEffectsTest, BitStrings) {
  EXPECT_STREQ(
      "|||||||",
      SideEffects::None().ToString().c_str());
  EXPECT_STREQ(
      "|GC|DFJISCBZL|DFJISCBZL|GC|DFJISCBZL|DFJISCBZL|",
      SideEffects::All().ToString().c_str());
  EXPECT_STREQ(
      "|||||DFJISCBZL|DFJISCBZL|",
      SideEffects::AllWrites().ToString().c_str());
  EXPECT_STREQ(
      "||DFJISCBZL|DFJISCBZL||||",
      SideEffects::AllReads().ToString().c_str());
  EXPECT_STREQ(
      "||||||L|",
      SideEffects::FieldWriteOfType(DataType::Type::kReference, false).ToString().c_str());
  EXPECT_STREQ(
      "||DFJISCBZL|DFJISCBZL||DFJISCBZL|DFJISCBZL|",
      SideEffects::FieldWriteOfType(DataType::Type::kReference, true).ToString().c_str());
  EXPECT_STREQ(
      "|||||Z||",
      SideEffects::ArrayWriteOfType(DataType::Type::kBool).ToString().c_str());
  EXPECT_STREQ(
      "|||||C||",
      SideEffects::ArrayWriteOfType(DataType::Type::kUint16).ToString().c_str());
  EXPECT_STREQ(
      "|||||S||",
      SideEffects::ArrayWriteOfType(DataType::Type::kInt16).ToString().c_str());
  EXPECT_STREQ(
      "|||B||||",
      SideEffects::FieldReadOfType(DataType::Type::kInt8, false).ToString().c_str());
  EXPECT_STREQ(
      "||D|||||",
      SideEffects::ArrayReadOfType(DataType::Type::kFloat64).ToString().c_str());
  EXPECT_STREQ(
      "||J|||||",
      SideEffects::ArrayReadOfType(DataType::Type::kInt64).ToString().c_str());
  EXPECT_STREQ(
      "||F|||||",
      SideEffects::ArrayReadOfType(DataType::Type::kFloat32).ToString().c_str());
  EXPECT_STREQ(
      "||I|||||",
      SideEffects::ArrayReadOfType(DataType::Type::kInt32).ToString().c_str());
  SideEffects s = SideEffects::None();
  s = s.Union(SideEffects::FieldWriteOfType(DataType::Type::kUint16, /* is_volatile */ false));
  s = s.Union(SideEffects::FieldWriteOfType(DataType::Type::kInt64, /* is_volatile */ false));
  s = s.Union(SideEffects::ArrayWriteOfType(DataType::Type::kInt16));
  s = s.Union(SideEffects::FieldReadOfType(DataType::Type::kInt32, /* is_volatile */ false));
  s = s.Union(SideEffects::ArrayReadOfType(DataType::Type::kFloat32));
  s = s.Union(SideEffects::ArrayReadOfType(DataType::Type::kFloat64));
  EXPECT_STREQ("||DF|I||S|JC|", s.ToString().c_str());
}

}  // namespace art
