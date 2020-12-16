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

#include "load_store_analysis.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art {

class LoadStoreAnalysisTest : public OptimizingUnitTest {
 public:
  LoadStoreAnalysisTest() : graph_(CreateGraph()) { }

  HGraph* graph_;
};

TEST_F(LoadStoreAnalysisTest, ArrayHeapLocations) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);

  // entry:
  // array         ParameterValue
  // index         ParameterValue
  // c1            IntConstant
  // c2            IntConstant
  // c3            IntConstant
  // array_get1    ArrayGet [array, c1]
  // array_get2    ArrayGet [array, c2]
  // array_set1    ArraySet [array, c1, c3]
  // array_set2    ArraySet [array, index, c3]
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);
  HInstruction* array_get1 = new (GetAllocator()) HArrayGet(array, c1, DataType::Type::kInt32, 0);
  HInstruction* array_get2 = new (GetAllocator()) HArrayGet(array, c2, DataType::Type::kInt32, 0);
  HInstruction* array_set1 =
      new (GetAllocator()) HArraySet(array, c1, c3, DataType::Type::kInt32, 0);
  HInstruction* array_set2 =
      new (GetAllocator()) HArraySet(array, index, c3, DataType::Type::kInt32, 0);
  entry->AddInstruction(array);
  entry->AddInstruction(index);
  entry->AddInstruction(array_get1);
  entry->AddInstruction(array_get2);
  entry->AddInstruction(array_set1);
  entry->AddInstruction(array_set2);

  // Test HeapLocationCollector initialization.
  // Should be no heap locations, no operations on the heap.
  HeapLocationCollector heap_location_collector(graph_);
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 0U);
  ASSERT_FALSE(heap_location_collector.HasHeapStores());

  // Test that after visiting the graph_, it must see following heap locations
  // array[c1], array[c2], array[index]; and it should see heap stores.
  heap_location_collector.VisitBasicBlock(entry);
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 3U);
  ASSERT_TRUE(heap_location_collector.HasHeapStores());

  // Test queries on HeapLocationCollector's ref info and index records.
  ReferenceInfo* ref = heap_location_collector.FindReferenceInfoOf(array);
  size_t field = HeapLocation::kInvalidFieldOffset;
  size_t vec = HeapLocation::kScalar;
  size_t class_def = HeapLocation::kDeclaringClassDefIndexForArrays;
  size_t loc1 = heap_location_collector.FindHeapLocationIndex(ref, field, c1, vec, class_def);
  size_t loc2 = heap_location_collector.FindHeapLocationIndex(ref, field, c2, vec, class_def);
  size_t loc3 = heap_location_collector.FindHeapLocationIndex(ref, field, index, vec, class_def);
  // must find this reference info for array in HeapLocationCollector.
  ASSERT_TRUE(ref != nullptr);
  // must find these heap locations;
  // and array[1], array[2], array[3] should be different heap locations.
  ASSERT_TRUE(loc1 != HeapLocationCollector::kHeapLocationNotFound);
  ASSERT_TRUE(loc2 != HeapLocationCollector::kHeapLocationNotFound);
  ASSERT_TRUE(loc3 != HeapLocationCollector::kHeapLocationNotFound);
  ASSERT_TRUE(loc1 != loc2);
  ASSERT_TRUE(loc2 != loc3);
  ASSERT_TRUE(loc1 != loc3);

  // Test alias relationships after building aliasing matrix.
  // array[1] and array[2] clearly should not alias;
  // array[index] should alias with the others, because index is an unknow value.
  heap_location_collector.BuildAliasingMatrix();
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc3));
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc3));
}

TEST_F(LoadStoreAnalysisTest, FieldHeapLocations) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);

  // entry:
  // object              ParameterValue
  // c1                  IntConstant
  // set_field10         InstanceFieldSet [object, c1, 10]
  // get_field10         InstanceFieldGet [object, 10]
  // get_field20         InstanceFieldGet [object, 20]

  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* object = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                              dex::TypeIndex(0),
                                                              0,
                                                              DataType::Type::kReference);
  HInstanceFieldSet* set_field10 = new (GetAllocator()) HInstanceFieldSet(object,
                                                                          c1,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          kUnknownFieldIndex,
                                                                          kUnknownClassDefIndex,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstanceFieldGet* get_field10 = new (GetAllocator()) HInstanceFieldGet(object,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(10),
                                                                          false,
                                                                          kUnknownFieldIndex,
                                                                          kUnknownClassDefIndex,
                                                                          graph_->GetDexFile(),
                                                                          0);
  HInstanceFieldGet* get_field20 = new (GetAllocator()) HInstanceFieldGet(object,
                                                                          nullptr,
                                                                          DataType::Type::kInt32,
                                                                          MemberOffset(20),
                                                                          false,
                                                                          kUnknownFieldIndex,
                                                                          kUnknownClassDefIndex,
                                                                          graph_->GetDexFile(),
                                                                          0);
  entry->AddInstruction(object);
  entry->AddInstruction(set_field10);
  entry->AddInstruction(get_field10);
  entry->AddInstruction(get_field20);

  // Test HeapLocationCollector initialization.
  // Should be no heap locations, no operations on the heap.
  HeapLocationCollector heap_location_collector(graph_);
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 0U);
  ASSERT_FALSE(heap_location_collector.HasHeapStores());

  // Test that after visiting the graph, it must see following heap locations
  // object.field10, object.field20 and it should see heap stores.
  heap_location_collector.VisitBasicBlock(entry);
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 2U);
  ASSERT_TRUE(heap_location_collector.HasHeapStores());

  // Test queries on HeapLocationCollector's ref info and index records.
  ReferenceInfo* ref = heap_location_collector.FindReferenceInfoOf(object);
  size_t loc1 = heap_location_collector.GetFieldHeapLocation(object, &get_field10->GetFieldInfo());
  size_t loc2 = heap_location_collector.GetFieldHeapLocation(object, &get_field20->GetFieldInfo());
  // must find references info for object and in HeapLocationCollector.
  ASSERT_TRUE(ref != nullptr);
  // must find these heap locations.
  ASSERT_TRUE(loc1 != HeapLocationCollector::kHeapLocationNotFound);
  ASSERT_TRUE(loc2 != HeapLocationCollector::kHeapLocationNotFound);
  // different fields of same object.
  ASSERT_TRUE(loc1 != loc2);
  // accesses to different fields of the same object should not alias.
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
}

TEST_F(LoadStoreAnalysisTest, ArrayIndexAliasingTest) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  graph_->BuildDominatorTree();

  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c_neg1 = graph_->GetIntConstant(-1);
  HInstruction* add0 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c0);
  HInstruction* add1 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c1);
  HInstruction* sub0 = new (GetAllocator()) HSub(DataType::Type::kInt32, index, c0);
  HInstruction* sub1 = new (GetAllocator()) HSub(DataType::Type::kInt32, index, c1);
  HInstruction* sub_neg1 = new (GetAllocator()) HSub(DataType::Type::kInt32, index, c_neg1);
  HInstruction* rev_sub1 = new (GetAllocator()) HSub(DataType::Type::kInt32, c1, index);
  HInstruction* arr_set1 = new (GetAllocator()) HArraySet(array, c0, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set2 = new (GetAllocator()) HArraySet(array, c1, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set3 =
      new (GetAllocator()) HArraySet(array, add0, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set4 =
      new (GetAllocator()) HArraySet(array, add1, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set5 =
      new (GetAllocator()) HArraySet(array, sub0, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set6 =
      new (GetAllocator()) HArraySet(array, sub1, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set7 =
      new (GetAllocator()) HArraySet(array, rev_sub1, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set8 =
      new (GetAllocator()) HArraySet(array, sub_neg1, c0, DataType::Type::kInt32, 0);

  entry->AddInstruction(array);
  entry->AddInstruction(index);
  entry->AddInstruction(add0);
  entry->AddInstruction(add1);
  entry->AddInstruction(sub0);
  entry->AddInstruction(sub1);
  entry->AddInstruction(sub_neg1);
  entry->AddInstruction(rev_sub1);

  entry->AddInstruction(arr_set1);  // array[0] = c0
  entry->AddInstruction(arr_set2);  // array[1] = c0
  entry->AddInstruction(arr_set3);  // array[i+0] = c0
  entry->AddInstruction(arr_set4);  // array[i+1] = c0
  entry->AddInstruction(arr_set5);  // array[i-0] = c0
  entry->AddInstruction(arr_set6);  // array[i-1] = c0
  entry->AddInstruction(arr_set7);  // array[1-i] = c0
  entry->AddInstruction(arr_set8);  // array[i-(-1)] = c0

  LoadStoreAnalysis lsa(graph_);
  lsa.Run();
  const HeapLocationCollector& heap_location_collector = lsa.GetHeapLocationCollector();

  // LSA/HeapLocationCollector should see those ArrayGet instructions.
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 8U);
  ASSERT_TRUE(heap_location_collector.HasHeapStores());

  // Test queries on HeapLocationCollector's aliasing matrix after load store analysis.
  size_t loc1 = HeapLocationCollector::kHeapLocationNotFound;
  size_t loc2 = HeapLocationCollector::kHeapLocationNotFound;

  // Test alias: array[0] and array[1]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c1);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+0] and array[i-0]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub0);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+1] and array[i-1]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add1);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub1);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+1] and array[1-i]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add1);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, rev_sub1);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+1] and array[i-(-1)]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add1);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_neg1);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));
}

TEST_F(LoadStoreAnalysisTest, ArrayAliasingTest) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  graph_->BuildDominatorTree();

  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c6 = graph_->GetIntConstant(6);
  HInstruction* c8 = graph_->GetIntConstant(8);

  HInstruction* arr_set_0 = new (GetAllocator()) HArraySet(array,
                                                           c0,
                                                           c0,
                                                           DataType::Type::kInt32,
                                                           0);
  HInstruction* arr_set_1 = new (GetAllocator()) HArraySet(array,
                                                           c1,
                                                           c0,
                                                           DataType::Type::kInt32,
                                                           0);
  HInstruction* arr_set_i = new (GetAllocator()) HArraySet(array,
                                                           index,
                                                           c0,
                                                           DataType::Type::kInt32,
                                                           0);

  HVecOperation* v1 = new (GetAllocator()) HVecReplicateScalar(GetAllocator(),
                                                               c1,
                                                               DataType::Type::kInt32,
                                                               4,
                                                               kNoDexPc);
  HVecOperation* v2 = new (GetAllocator()) HVecReplicateScalar(GetAllocator(),
                                                               c1,
                                                               DataType::Type::kInt32,
                                                               2,
                                                               kNoDexPc);
  HInstruction* i_add6 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c6);
  HInstruction* i_add8 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c8);

  HInstruction* vstore_0 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      c0,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_1 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      c1,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_8 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      c8,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_i = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      index,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_i_add6 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      i_add6,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_i_add8 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      i_add8,
      v1,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4,
      kNoDexPc);
  HInstruction* vstore_i_add6_vlen2 = new (GetAllocator()) HVecStore(
      GetAllocator(),
      array,
      i_add6,
      v2,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      2,
      kNoDexPc);

  entry->AddInstruction(array);
  entry->AddInstruction(index);

  entry->AddInstruction(arr_set_0);
  entry->AddInstruction(arr_set_1);
  entry->AddInstruction(arr_set_i);
  entry->AddInstruction(v1);
  entry->AddInstruction(v2);
  entry->AddInstruction(i_add6);
  entry->AddInstruction(i_add8);
  entry->AddInstruction(vstore_0);
  entry->AddInstruction(vstore_1);
  entry->AddInstruction(vstore_8);
  entry->AddInstruction(vstore_i);
  entry->AddInstruction(vstore_i_add6);
  entry->AddInstruction(vstore_i_add8);
  entry->AddInstruction(vstore_i_add6_vlen2);

  LoadStoreAnalysis lsa(graph_);
  lsa.Run();
  const HeapLocationCollector& heap_location_collector = lsa.GetHeapLocationCollector();

  // LSA/HeapLocationCollector should see those instructions.
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 10U);
  ASSERT_TRUE(heap_location_collector.HasHeapStores());

  // Test queries on HeapLocationCollector's aliasing matrix after load store analysis.
  size_t loc1, loc2;

  // Test alias: array[0] and array[0,1,2,3]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c0, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[0] and array[8,9,10,11]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c8, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[1] and array[8,9,10,11]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c1);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c8, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[1] and array[0,1,2,3]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c1);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c0, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[0,1,2,3] and array[8,9,10,11]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c0, 4);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c8, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[0,1,2,3] and array[1,2,3,4]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c1, 4);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c0, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[0] and array[i,i+1,i+2,i+3]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, c0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, index, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i] and array[0,1,2,3]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, index);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, c0, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i] and array[i,i+1,i+2,i+3]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, index);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, index, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i] and array[i+8,i+9,i+10,i+11]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, index);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, i_add8, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+6,i+7,i+8,i+9] and array[i+8,i+9,i+10,i+11]
  // Test partial overlap.
  loc1 = heap_location_collector.GetArrayHeapLocation(array, i_add6, 4);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, i_add8, 4);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+6,i+7] and array[i,i+1,i+2,i+3]
  // Test different vector lengths.
  loc1 = heap_location_collector.GetArrayHeapLocation(array, i_add6, 2);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, index, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+6,i+7] and array[i+8,i+9,i+10,i+11]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, i_add6, 2);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, i_add8, 4);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
}

TEST_F(LoadStoreAnalysisTest, ArrayIndexCalculationOverflowTest) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);
  graph_->BuildDominatorTree();

  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c_0x80000000 = graph_->GetIntConstant(0x80000000);
  HInstruction* c_0x10 = graph_->GetIntConstant(0x10);
  HInstruction* c_0xFFFFFFF0 = graph_->GetIntConstant(0xFFFFFFF0);
  HInstruction* c_0x7FFFFFFF = graph_->GetIntConstant(0x7FFFFFFF);
  HInstruction* c_0x80000001 = graph_->GetIntConstant(0x80000001);

  // `index+0x80000000` and `index-0x80000000` array indices MAY alias.
  HInstruction* add_0x80000000 = new (GetAllocator()) HAdd(
      DataType::Type::kInt32, index, c_0x80000000);
  HInstruction* sub_0x80000000 = new (GetAllocator()) HSub(
      DataType::Type::kInt32, index, c_0x80000000);
  HInstruction* arr_set_1 = new (GetAllocator()) HArraySet(
      array, add_0x80000000, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set_2 = new (GetAllocator()) HArraySet(
      array, sub_0x80000000, c0, DataType::Type::kInt32, 0);

  // `index+0x10` and `index-0xFFFFFFF0` array indices MAY alias.
  HInstruction* add_0x10 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c_0x10);
  HInstruction* sub_0xFFFFFFF0 = new (GetAllocator()) HSub(
      DataType::Type::kInt32, index, c_0xFFFFFFF0);
  HInstruction* arr_set_3 = new (GetAllocator()) HArraySet(
      array, add_0x10, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set_4 = new (GetAllocator()) HArraySet(
      array, sub_0xFFFFFFF0, c0, DataType::Type::kInt32, 0);

  // `index+0x7FFFFFFF` and `index-0x80000001` array indices MAY alias.
  HInstruction* add_0x7FFFFFFF = new (GetAllocator()) HAdd(
      DataType::Type::kInt32, index, c_0x7FFFFFFF);
  HInstruction* sub_0x80000001 = new (GetAllocator()) HSub(
      DataType::Type::kInt32, index, c_0x80000001);
  HInstruction* arr_set_5 = new (GetAllocator()) HArraySet(
      array, add_0x7FFFFFFF, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set_6 = new (GetAllocator()) HArraySet(
      array, sub_0x80000001, c0, DataType::Type::kInt32, 0);

  // `index+0` and `index-0` array indices MAY alias.
  HInstruction* add_0 = new (GetAllocator()) HAdd(DataType::Type::kInt32, index, c0);
  HInstruction* sub_0 = new (GetAllocator()) HSub(DataType::Type::kInt32, index, c0);
  HInstruction* arr_set_7 = new (GetAllocator()) HArraySet(
      array, add_0, c0, DataType::Type::kInt32, 0);
  HInstruction* arr_set_8 = new (GetAllocator()) HArraySet(
      array, sub_0, c0, DataType::Type::kInt32, 0);

  entry->AddInstruction(array);
  entry->AddInstruction(index);
  entry->AddInstruction(add_0x80000000);
  entry->AddInstruction(sub_0x80000000);
  entry->AddInstruction(add_0x10);
  entry->AddInstruction(sub_0xFFFFFFF0);
  entry->AddInstruction(add_0x7FFFFFFF);
  entry->AddInstruction(sub_0x80000001);
  entry->AddInstruction(add_0);
  entry->AddInstruction(sub_0);
  entry->AddInstruction(arr_set_1);
  entry->AddInstruction(arr_set_2);
  entry->AddInstruction(arr_set_3);
  entry->AddInstruction(arr_set_4);
  entry->AddInstruction(arr_set_5);
  entry->AddInstruction(arr_set_6);
  entry->AddInstruction(arr_set_7);
  entry->AddInstruction(arr_set_8);

  LoadStoreAnalysis lsa(graph_);
  lsa.Run();
  const HeapLocationCollector& heap_location_collector = lsa.GetHeapLocationCollector();

  // LSA/HeapLocationCollector should see those ArrayGet instructions.
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 8U);
  ASSERT_TRUE(heap_location_collector.HasHeapStores());

  // Test queries on HeapLocationCollector's aliasing matrix after load store analysis.
  size_t loc1 = HeapLocationCollector::kHeapLocationNotFound;
  size_t loc2 = HeapLocationCollector::kHeapLocationNotFound;

  // Test alias: array[i+0x80000000] and array[i-0x80000000]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add_0x80000000);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0x80000000);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+0x10] and array[i-0xFFFFFFF0]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add_0x10);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0xFFFFFFF0);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+0x7FFFFFFF] and array[i-0x80000001]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add_0x7FFFFFFF);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0x80000001);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Test alias: array[i+0] and array[i-0]
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add_0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0);
  ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));

  // Should not alias:
  loc1 = heap_location_collector.GetArrayHeapLocation(array, sub_0x80000000);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0x80000001);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));

  // Should not alias:
  loc1 = heap_location_collector.GetArrayHeapLocation(array, add_0);
  loc2 = heap_location_collector.GetArrayHeapLocation(array, sub_0x80000000);
  ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
}

TEST_F(LoadStoreAnalysisTest, TestHuntOriginalRef) {
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry);
  graph_->SetEntryBlock(entry);

  // Different ways where orignal array reference are transformed & passed to ArrayGet.
  // ParameterValue --> ArrayGet
  // ParameterValue --> BoundType --> ArrayGet
  // ParameterValue --> BoundType --> NullCheck --> ArrayGet
  // ParameterValue --> BoundType --> NullCheck --> IntermediateAddress --> ArrayGet
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* array = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                             dex::TypeIndex(0),
                                                             0,
                                                             DataType::Type::kReference);
  HInstruction* array_get1 = new (GetAllocator()) HArrayGet(array,
                                                            c1,
                                                            DataType::Type::kInt32,
                                                            0);

  HInstruction* bound_type = new (GetAllocator()) HBoundType(array);
  HInstruction* array_get2 = new (GetAllocator()) HArrayGet(bound_type,
                                                            c1,
                                                            DataType::Type::kInt32,
                                                            0);

  HInstruction* null_check = new (GetAllocator()) HNullCheck(bound_type, 0);
  HInstruction* array_get3 = new (GetAllocator()) HArrayGet(null_check,
                                                            c1,
                                                            DataType::Type::kInt32,
                                                            0);

  HInstruction* inter_addr = new (GetAllocator()) HIntermediateAddress(null_check, c1, 0);
  HInstruction* array_get4 = new (GetAllocator()) HArrayGet(inter_addr,
                                                            c1,
                                                            DataType::Type::kInt32,
                                                            0);
  entry->AddInstruction(array);
  entry->AddInstruction(array_get1);
  entry->AddInstruction(bound_type);
  entry->AddInstruction(array_get2);
  entry->AddInstruction(null_check);
  entry->AddInstruction(array_get3);
  entry->AddInstruction(inter_addr);
  entry->AddInstruction(array_get4);

  HeapLocationCollector heap_location_collector(graph_);
  heap_location_collector.VisitBasicBlock(entry);

  // Test that the HeapLocationCollector should be able to tell
  // that there is only ONE array location, no matter how many
  // times the original reference has been transformed by BoundType,
  // NullCheck, IntermediateAddress, etc.
  ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 1U);
  size_t loc1 = heap_location_collector.GetArrayHeapLocation(array, c1);
  size_t loc2 = heap_location_collector.GetArrayHeapLocation(bound_type, c1);
  size_t loc3 = heap_location_collector.GetArrayHeapLocation(null_check, c1);
  size_t loc4 = heap_location_collector.GetArrayHeapLocation(inter_addr, c1);
  ASSERT_TRUE(loc1 != HeapLocationCollector::kHeapLocationNotFound);
  ASSERT_EQ(loc1, loc2);
  ASSERT_EQ(loc1, loc3);
  ASSERT_EQ(loc1, loc4);
}

}  // namespace art
