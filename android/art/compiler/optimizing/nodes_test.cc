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

#include "nodes.h"

#include "base/arena_allocator.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art {

class NodeTest : public OptimizingUnitTest {};

/**
 * Test that removing instruction from the graph removes itself from user lists
 * and environment lists.
 */
TEST_F(NodeTest, RemoveInstruction) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  entry->AddInstruction(parameter);
  entry->AddInstruction(new (GetAllocator()) HGoto());

  HBasicBlock* first_block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(first_block);
  entry->AddSuccessor(first_block);
  HInstruction* null_check = new (GetAllocator()) HNullCheck(parameter, 0);
  first_block->AddInstruction(null_check);
  first_block->AddInstruction(new (GetAllocator()) HReturnVoid());

  HBasicBlock* exit_block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(exit_block);
  first_block->AddSuccessor(exit_block);
  exit_block->AddInstruction(new (GetAllocator()) HExit());

  HEnvironment* environment = new (GetAllocator()) HEnvironment(
      GetAllocator(), 1, graph->GetArtMethod(), 0, null_check);
  null_check->SetRawEnvironment(environment);
  environment->SetRawEnvAt(0, parameter);
  parameter->AddEnvUseAt(null_check->GetEnvironment(), 0);

  ASSERT_TRUE(parameter->HasEnvironmentUses());
  ASSERT_TRUE(parameter->HasUses());

  first_block->RemoveInstruction(null_check);

  ASSERT_FALSE(parameter->HasEnvironmentUses());
  ASSERT_FALSE(parameter->HasUses());
}

/**
 * Test that inserting an instruction in the graph updates user lists.
 */
TEST_F(NodeTest, InsertInstruction) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter1 = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* parameter2 = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  entry->AddInstruction(parameter1);
  entry->AddInstruction(parameter2);
  entry->AddInstruction(new (GetAllocator()) HExit());

  ASSERT_FALSE(parameter1->HasUses());

  HInstruction* to_insert = new (GetAllocator()) HNullCheck(parameter1, 0);
  entry->InsertInstructionBefore(to_insert, parameter2);

  ASSERT_TRUE(parameter1->HasUses());
  ASSERT_TRUE(parameter1->GetUses().HasExactlyOneElement());
}

/**
 * Test that adding an instruction in the graph updates user lists.
 */
TEST_F(NodeTest, AddInstruction) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  entry->AddInstruction(parameter);

  ASSERT_FALSE(parameter->HasUses());

  HInstruction* to_add = new (GetAllocator()) HNullCheck(parameter, 0);
  entry->AddInstruction(to_add);

  ASSERT_TRUE(parameter->HasUses());
  ASSERT_TRUE(parameter->GetUses().HasExactlyOneElement());
}

TEST_F(NodeTest, ParentEnvironment) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter1 = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* with_environment = new (GetAllocator()) HNullCheck(parameter1, 0);
  entry->AddInstruction(parameter1);
  entry->AddInstruction(with_environment);
  entry->AddInstruction(new (GetAllocator()) HExit());

  ASSERT_TRUE(parameter1->HasUses());
  ASSERT_TRUE(parameter1->GetUses().HasExactlyOneElement());

  HEnvironment* environment = new (GetAllocator()) HEnvironment(
      GetAllocator(), 1, graph->GetArtMethod(), 0, with_environment);
  HInstruction* const array[] = { parameter1 };

  environment->CopyFrom(ArrayRef<HInstruction* const>(array));
  with_environment->SetRawEnvironment(environment);

  ASSERT_TRUE(parameter1->HasEnvironmentUses());
  ASSERT_TRUE(parameter1->GetEnvUses().HasExactlyOneElement());

  HEnvironment* parent1 = new (GetAllocator()) HEnvironment(
      GetAllocator(), 1, graph->GetArtMethod(), 0, nullptr);
  parent1->CopyFrom(ArrayRef<HInstruction* const>(array));

  ASSERT_EQ(parameter1->GetEnvUses().SizeSlow(), 2u);

  HEnvironment* parent2 = new (GetAllocator()) HEnvironment(
      GetAllocator(), 1, graph->GetArtMethod(), 0, nullptr);
  parent2->CopyFrom(ArrayRef<HInstruction* const>(array));
  parent1->SetAndCopyParentChain(GetAllocator(), parent2);

  // One use for parent2, and one other use for the new parent of parent1.
  ASSERT_EQ(parameter1->GetEnvUses().SizeSlow(), 4u);

  // We have copied the parent chain. So we now have two more uses.
  environment->SetAndCopyParentChain(GetAllocator(), parent1);
  ASSERT_EQ(parameter1->GetEnvUses().SizeSlow(), 6u);
}

}  // namespace art
