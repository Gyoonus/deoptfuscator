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

#include "gvn.h"

#include "base/arena_allocator.h"
#include "builder.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "side_effects_analysis.h"

namespace art {

class GVNTest : public OptimizingUnitTest {};

TEST_F(GVNTest, LocalFieldElimination) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(graph->GetDexFile(),
                                                                 dex::TypeIndex(0),
                                                                 0,
                                                                 DataType::Type::kReference);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kReference,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kReference,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  HInstruction* to_remove = block->GetLastInstruction();
  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kReference,
                                                               MemberOffset(43),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  HInstruction* different_offset = block->GetLastInstruction();
  // Kill the value.
  block->AddInstruction(new (GetAllocator()) HInstanceFieldSet(parameter,
                                                               parameter,
                                                               nullptr,
                                                               DataType::Type::kReference,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kReference,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  HInstruction* use_after_kill = block->GetLastInstruction();
  block->AddInstruction(new (GetAllocator()) HExit());

  ASSERT_EQ(to_remove->GetBlock(), block);
  ASSERT_EQ(different_offset->GetBlock(), block);
  ASSERT_EQ(use_after_kill->GetBlock(), block);

  graph->BuildDominatorTree();
  SideEffectsAnalysis side_effects(graph);
  side_effects.Run();
  GVNOptimization(graph, side_effects).Run();

  ASSERT_TRUE(to_remove->GetBlock() == nullptr);
  ASSERT_EQ(different_offset->GetBlock(), block);
  ASSERT_EQ(use_after_kill->GetBlock(), block);
}

TEST_F(GVNTest, GlobalFieldElimination) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(graph->GetDexFile(),
                                                                 dex::TypeIndex(0),
                                                                 0,
                                                                 DataType::Type::kReference);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kBool,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));

  block->AddInstruction(new (GetAllocator()) HIf(block->GetLastInstruction()));
  HBasicBlock* then = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* else_ = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* join = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(then);
  graph->AddBlock(else_);
  graph->AddBlock(join);

  block->AddSuccessor(then);
  block->AddSuccessor(else_);
  then->AddSuccessor(join);
  else_->AddSuccessor(join);

  then->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                              nullptr,
                                                              DataType::Type::kBool,
                                                              MemberOffset(42),
                                                              false,
                                                              kUnknownFieldIndex,
                                                              kUnknownClassDefIndex,
                                                              graph->GetDexFile(),
                                                              0));
  then->AddInstruction(new (GetAllocator()) HGoto());
  else_->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kBool,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  else_->AddInstruction(new (GetAllocator()) HGoto());
  join->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                              nullptr,
                                                              DataType::Type::kBool,
                                                              MemberOffset(42),
                                                              false,
                                                              kUnknownFieldIndex,
                                                              kUnknownClassDefIndex,
                                                              graph->GetDexFile(),
                                                              0));
  join->AddInstruction(new (GetAllocator()) HExit());

  graph->BuildDominatorTree();
  SideEffectsAnalysis side_effects(graph);
  side_effects.Run();
  GVNOptimization(graph, side_effects).Run();

  // Check that all field get instructions have been GVN'ed.
  ASSERT_TRUE(then->GetFirstInstruction()->IsGoto());
  ASSERT_TRUE(else_->GetFirstInstruction()->IsGoto());
  ASSERT_TRUE(join->GetFirstInstruction()->IsExit());
}

TEST_F(GVNTest, LoopFieldElimination) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);

  HInstruction* parameter = new (GetAllocator()) HParameterValue(graph->GetDexFile(),
                                                                 dex::TypeIndex(0),
                                                                 0,
                                                                 DataType::Type::kReference);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                               nullptr,
                                                               DataType::Type::kBool,
                                                               MemberOffset(42),
                                                               false,
                                                               kUnknownFieldIndex,
                                                               kUnknownClassDefIndex,
                                                               graph->GetDexFile(),
                                                               0));
  block->AddInstruction(new (GetAllocator()) HGoto());

  HBasicBlock* loop_header = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* loop_body = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* exit = new (GetAllocator()) HBasicBlock(graph);

  graph->AddBlock(loop_header);
  graph->AddBlock(loop_body);
  graph->AddBlock(exit);
  block->AddSuccessor(loop_header);
  loop_header->AddSuccessor(loop_body);
  loop_header->AddSuccessor(exit);
  loop_body->AddSuccessor(loop_header);

  loop_header->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                                     nullptr,
                                                                     DataType::Type::kBool,
                                                                     MemberOffset(42),
                                                                     false,
                                                                     kUnknownFieldIndex,
                                                                     kUnknownClassDefIndex,
                                                                     graph->GetDexFile(),
                                                                     0));
  HInstruction* field_get_in_loop_header = loop_header->GetLastInstruction();
  loop_header->AddInstruction(new (GetAllocator()) HIf(block->GetLastInstruction()));

  // Kill inside the loop body to prevent field gets inside the loop header
  // and the body to be GVN'ed.
  loop_body->AddInstruction(new (GetAllocator()) HInstanceFieldSet(parameter,
                                                                   parameter,
                                                                   nullptr,
                                                                   DataType::Type::kBool,
                                                                   MemberOffset(42),
                                                                   false,
                                                                   kUnknownFieldIndex,
                                                                   kUnknownClassDefIndex,
                                                                   graph->GetDexFile(),
                                                                   0));
  HInstruction* field_set = loop_body->GetLastInstruction();
  loop_body->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                                   nullptr,
                                                                   DataType::Type::kBool,
                                                                   MemberOffset(42),
                                                                   false,
                                                                   kUnknownFieldIndex,
                                                                   kUnknownClassDefIndex,
                                                                   graph->GetDexFile(),
                                                                   0));
  HInstruction* field_get_in_loop_body = loop_body->GetLastInstruction();
  loop_body->AddInstruction(new (GetAllocator()) HGoto());

  exit->AddInstruction(new (GetAllocator()) HInstanceFieldGet(parameter,
                                                              nullptr,
                                                              DataType::Type::kBool,
                                                              MemberOffset(42),
                                                              false,
                                                              kUnknownFieldIndex,
                                                              kUnknownClassDefIndex,
                                                              graph->GetDexFile(),
                                                              0));
  HInstruction* field_get_in_exit = exit->GetLastInstruction();
  exit->AddInstruction(new (GetAllocator()) HExit());

  ASSERT_EQ(field_get_in_loop_header->GetBlock(), loop_header);
  ASSERT_EQ(field_get_in_loop_body->GetBlock(), loop_body);
  ASSERT_EQ(field_get_in_exit->GetBlock(), exit);

  graph->BuildDominatorTree();
  {
    SideEffectsAnalysis side_effects(graph);
    side_effects.Run();
    GVNOptimization(graph, side_effects).Run();
  }

  // Check that all field get instructions are still there.
  ASSERT_EQ(field_get_in_loop_header->GetBlock(), loop_header);
  ASSERT_EQ(field_get_in_loop_body->GetBlock(), loop_body);
  // The exit block is dominated by the loop header, whose field get
  // does not get killed by the loop flags.
  ASSERT_TRUE(field_get_in_exit->GetBlock() == nullptr);

  // Now remove the field set, and check that all field get instructions have been GVN'ed.
  loop_body->RemoveInstruction(field_set);
  {
    SideEffectsAnalysis side_effects(graph);
    side_effects.Run();
    GVNOptimization(graph, side_effects).Run();
  }

  ASSERT_TRUE(field_get_in_loop_header->GetBlock() == nullptr);
  ASSERT_TRUE(field_get_in_loop_body->GetBlock() == nullptr);
  ASSERT_TRUE(field_get_in_exit->GetBlock() == nullptr);
}

// Test that inner loops affect the side effects of the outer loop.
TEST_F(GVNTest, LoopSideEffects) {
  static const SideEffects kCanTriggerGC = SideEffects::CanTriggerGC();

  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);

  HBasicBlock* outer_loop_header = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* outer_loop_body = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* outer_loop_exit = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* inner_loop_header = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* inner_loop_body = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* inner_loop_exit = new (GetAllocator()) HBasicBlock(graph);

  graph->AddBlock(outer_loop_header);
  graph->AddBlock(outer_loop_body);
  graph->AddBlock(outer_loop_exit);
  graph->AddBlock(inner_loop_header);
  graph->AddBlock(inner_loop_body);
  graph->AddBlock(inner_loop_exit);

  entry->AddSuccessor(outer_loop_header);
  outer_loop_header->AddSuccessor(outer_loop_body);
  outer_loop_header->AddSuccessor(outer_loop_exit);
  outer_loop_body->AddSuccessor(inner_loop_header);
  inner_loop_header->AddSuccessor(inner_loop_body);
  inner_loop_header->AddSuccessor(inner_loop_exit);
  inner_loop_body->AddSuccessor(inner_loop_header);
  inner_loop_exit->AddSuccessor(outer_loop_header);

  HInstruction* parameter = new (GetAllocator()) HParameterValue(graph->GetDexFile(),
                                                                 dex::TypeIndex(0),
                                                                 0,
                                                                 DataType::Type::kBool);
  entry->AddInstruction(parameter);
  entry->AddInstruction(new (GetAllocator()) HGoto());
  outer_loop_header->AddInstruction(new (GetAllocator()) HSuspendCheck());
  outer_loop_header->AddInstruction(new (GetAllocator()) HIf(parameter));
  outer_loop_body->AddInstruction(new (GetAllocator()) HGoto());
  inner_loop_header->AddInstruction(new (GetAllocator()) HSuspendCheck());
  inner_loop_header->AddInstruction(new (GetAllocator()) HIf(parameter));
  inner_loop_body->AddInstruction(new (GetAllocator()) HGoto());
  inner_loop_exit->AddInstruction(new (GetAllocator()) HGoto());
  outer_loop_exit->AddInstruction(new (GetAllocator()) HExit());

  graph->BuildDominatorTree();

  ASSERT_TRUE(inner_loop_header->GetLoopInformation()->IsIn(
      *outer_loop_header->GetLoopInformation()));

  // Check that the only side effect of loops is to potentially trigger GC.
  {
    // Make one block with a side effect.
    entry->AddInstruction(new (GetAllocator()) HInstanceFieldSet(parameter,
                                                                 parameter,
                                                                 nullptr,
                                                                 DataType::Type::kReference,
                                                                 MemberOffset(42),
                                                                 false,
                                                                 kUnknownFieldIndex,
                                                                 kUnknownClassDefIndex,
                                                                 graph->GetDexFile(),
                                                                 0));

    SideEffectsAnalysis side_effects(graph);
    side_effects.Run();

    ASSERT_TRUE(side_effects.GetBlockEffects(entry).DoesAnyWrite());
    ASSERT_FALSE(side_effects.GetBlockEffects(outer_loop_body).DoesAnyWrite());
    ASSERT_FALSE(side_effects.GetLoopEffects(outer_loop_header).DoesAnyWrite());
    ASSERT_FALSE(side_effects.GetLoopEffects(inner_loop_header).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetLoopEffects(outer_loop_header).Equals(kCanTriggerGC));
    ASSERT_TRUE(side_effects.GetLoopEffects(inner_loop_header).Equals(kCanTriggerGC));
  }

  // Check that the side effects of the outer loop does not affect the inner loop.
  {
    outer_loop_body->InsertInstructionBefore(
        new (GetAllocator()) HInstanceFieldSet(parameter,
                                               parameter,
                                               nullptr,
                                               DataType::Type::kReference,
                                               MemberOffset(42),
                                               false,
                                               kUnknownFieldIndex,
                                               kUnknownClassDefIndex,
                                               graph->GetDexFile(),
                                               0),
        outer_loop_body->GetLastInstruction());

    SideEffectsAnalysis side_effects(graph);
    side_effects.Run();

    ASSERT_TRUE(side_effects.GetBlockEffects(entry).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetBlockEffects(outer_loop_body).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetLoopEffects(outer_loop_header).DoesAnyWrite());
    ASSERT_FALSE(side_effects.GetLoopEffects(inner_loop_header).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetLoopEffects(inner_loop_header).Equals(kCanTriggerGC));
  }

  // Check that the side effects of the inner loop affects the outer loop.
  {
    outer_loop_body->RemoveInstruction(outer_loop_body->GetFirstInstruction());
    inner_loop_body->InsertInstructionBefore(
        new (GetAllocator()) HInstanceFieldSet(parameter,
                                               parameter,
                                               nullptr,
                                               DataType::Type::kReference,
                                               MemberOffset(42),
                                               false,
                                               kUnknownFieldIndex,
                                               kUnknownClassDefIndex,
                                               graph->GetDexFile(),
                                               0),
        inner_loop_body->GetLastInstruction());

    SideEffectsAnalysis side_effects(graph);
    side_effects.Run();

    ASSERT_TRUE(side_effects.GetBlockEffects(entry).DoesAnyWrite());
    ASSERT_FALSE(side_effects.GetBlockEffects(outer_loop_body).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetLoopEffects(outer_loop_header).DoesAnyWrite());
    ASSERT_TRUE(side_effects.GetLoopEffects(inner_loop_header).DoesAnyWrite());
  }
}
}  // namespace art
