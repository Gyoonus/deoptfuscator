/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "scheduler.h"

#include "base/arena_allocator.h"
#include "builder.h"
#include "codegen_test_utils.h"
#include "common_compiler_test.h"
#include "load_store_analysis.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pc_relative_fixups_x86.h"
#include "register_allocator.h"

#ifdef ART_ENABLE_CODEGEN_arm64
#include "scheduler_arm64.h"
#endif

#ifdef ART_ENABLE_CODEGEN_arm
#include "scheduler_arm.h"
#endif

namespace art {

// Return all combinations of ISA and code generator that are executable on
// hardware, or on simulator, and that we'd like to test.
static ::std::vector<CodegenTargetConfig> GetTargetConfigs() {
  ::std::vector<CodegenTargetConfig> v;
  ::std::vector<CodegenTargetConfig> test_config_candidates = {
#ifdef ART_ENABLE_CODEGEN_arm
    // TODO: Should't this be `kThumb2` instead of `kArm` here?
    CodegenTargetConfig(InstructionSet::kArm, create_codegen_arm_vixl32),
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
    CodegenTargetConfig(InstructionSet::kArm64, create_codegen_arm64),
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    CodegenTargetConfig(InstructionSet::kX86, create_codegen_x86),
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
    CodegenTargetConfig(InstructionSet::kX86_64, create_codegen_x86_64),
#endif
#ifdef ART_ENABLE_CODEGEN_mips
    CodegenTargetConfig(InstructionSet::kMips, create_codegen_mips),
#endif
#ifdef ART_ENABLE_CODEGEN_mips64
    CodegenTargetConfig(InstructionSet::kMips64, create_codegen_mips64)
#endif
  };

  for (const CodegenTargetConfig& test_config : test_config_candidates) {
    if (CanExecute(test_config.GetInstructionSet())) {
      v.push_back(test_config);
    }
  }

  return v;
}

class SchedulerTest : public OptimizingUnitTest {
 public:
  SchedulerTest() : graph_(CreateGraph()) { }

  // Build scheduling graph, and run target specific scheduling on it.
  void TestBuildDependencyGraphAndSchedule(HScheduler* scheduler) {
    HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* block1 = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry);
    graph_->AddBlock(block1);
    graph_->SetEntryBlock(entry);

    // entry:
    // array         ParameterValue
    // c1            IntConstant
    // c2            IntConstant
    // block1:
    // add1          Add [c1, c2]
    // add2          Add [add1, c2]
    // mul           Mul [add1, add2]
    // div_check     DivZeroCheck [add2] (env: add2, mul)
    // div           Div [add1, div_check]
    // array_get1    ArrayGet [array, add1]
    // array_set1    ArraySet [array, add1, add2]
    // array_get2    ArrayGet [array, add1]
    // array_set2    ArraySet [array, add1, add2]

    HInstruction* array = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                           dex::TypeIndex(0),
                                                           0,
                                                           DataType::Type::kReference);
    HInstruction* c1 = graph_->GetIntConstant(1);
    HInstruction* c2 = graph_->GetIntConstant(10);
    HInstruction* add1 = new (GetAllocator()) HAdd(DataType::Type::kInt32, c1, c2);
    HInstruction* add2 = new (GetAllocator()) HAdd(DataType::Type::kInt32, add1, c2);
    HInstruction* mul = new (GetAllocator()) HMul(DataType::Type::kInt32, add1, add2);
    HInstruction* div_check = new (GetAllocator()) HDivZeroCheck(add2, 0);
    HInstruction* div = new (GetAllocator()) HDiv(DataType::Type::kInt32, add1, div_check, 0);
    HInstruction* array_get1 =
        new (GetAllocator()) HArrayGet(array, add1, DataType::Type::kInt32, 0);
    HInstruction* array_set1 =
        new (GetAllocator()) HArraySet(array, add1, add2, DataType::Type::kInt32, 0);
    HInstruction* array_get2 =
        new (GetAllocator()) HArrayGet(array, add1, DataType::Type::kInt32, 0);
    HInstruction* array_set2 =
        new (GetAllocator()) HArraySet(array, add1, add2, DataType::Type::kInt32, 0);

    DCHECK(div_check->CanThrow());

    entry->AddInstruction(array);

    HInstruction* block_instructions[] = {add1,
                                          add2,
                                          mul,
                                          div_check,
                                          div,
                                          array_get1,
                                          array_set1,
                                          array_get2,
                                          array_set2};
    for (HInstruction* instr : block_instructions) {
      block1->AddInstruction(instr);
    }

    HEnvironment* environment = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                  2,
                                                                  graph_->GetArtMethod(),
                                                                  0,
                                                                  div_check);
    div_check->SetRawEnvironment(environment);
    environment->SetRawEnvAt(0, add2);
    add2->AddEnvUseAt(div_check->GetEnvironment(), 0);
    environment->SetRawEnvAt(1, mul);
    mul->AddEnvUseAt(div_check->GetEnvironment(), 1);

    SchedulingGraph scheduling_graph(scheduler, GetScopedAllocator());
    // Instructions must be inserted in reverse order into the scheduling graph.
    for (HInstruction* instr : ReverseRange(block_instructions)) {
      scheduling_graph.AddNode(instr);
    }

    // Should not have dependencies cross basic blocks.
    ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, c1));
    ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add2, c2));

    // Define-use dependency.
    ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(add2, add1));
    ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(add1, add2));
    ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div_check, add2));
    ASSERT_FALSE(scheduling_graph.HasImmediateDataDependency(div_check, add1));
    ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(div, div_check));
    ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set1, add1));
    ASSERT_TRUE(scheduling_graph.HasImmediateDataDependency(array_set1, add2));

    // Read and write dependencies
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set1, array_get1));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set2, array_get2));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_get2, array_set1));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set2, array_set1));

    // Env dependency.
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(div_check, mul));
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(mul, div_check));

    // CanThrow.
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(array_set1, div_check));

    // Exercise the code path of target specific scheduler and SchedulingLatencyVisitor.
    scheduler->Schedule(graph_);
  }

  void CompileWithRandomSchedulerAndRun(const std::vector<uint16_t>& data,
                                        bool has_result,
                                        int expected) {
    for (CodegenTargetConfig target_config : GetTargetConfigs()) {
      HGraph* graph = CreateCFG(data);

      // Schedule the graph randomly.
      HInstructionScheduling scheduling(graph, target_config.GetInstructionSet());
      scheduling.Run(/*only_optimize_loop_blocks*/ false, /*schedule_randomly*/ true);

      RunCode(target_config,
              graph,
              [](HGraph* graph_arg) { RemoveSuspendChecks(graph_arg); },
              has_result, expected);
    }
  }

  void TestDependencyGraphOnAliasingArrayAccesses(HScheduler* scheduler) {
    HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry);
    graph_->SetEntryBlock(entry);
    graph_->BuildDominatorTree();

    HInstruction* arr = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                             dex::TypeIndex(0),
                                                             0,
                                                             DataType::Type::kReference);
    HInstruction* i = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                           dex::TypeIndex(1),
                                                           1,
                                                           DataType::Type::kInt32);
    HInstruction* j = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                           dex::TypeIndex(1),
                                                           1,
                                                           DataType::Type::kInt32);
    HInstruction* object = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                                dex::TypeIndex(0),
                                                                0,
                                                                DataType::Type::kReference);
    HInstruction* c0 = graph_->GetIntConstant(0);
    HInstruction* c1 = graph_->GetIntConstant(1);
    HInstruction* add0 = new (GetAllocator()) HAdd(DataType::Type::kInt32, i, c0);
    HInstruction* add1 = new (GetAllocator()) HAdd(DataType::Type::kInt32, i, c1);
    HInstruction* sub0 = new (GetAllocator()) HSub(DataType::Type::kInt32, i, c0);
    HInstruction* sub1 = new (GetAllocator()) HSub(DataType::Type::kInt32, i, c1);
    HInstruction* arr_set_0 =
        new (GetAllocator()) HArraySet(arr, c0, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_1 =
        new (GetAllocator()) HArraySet(arr, c1, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_i = new (GetAllocator()) HArraySet(arr, i, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_add0 =
        new (GetAllocator()) HArraySet(arr, add0, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_add1 =
        new (GetAllocator()) HArraySet(arr, add1, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_sub0 =
        new (GetAllocator()) HArraySet(arr, sub0, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_sub1 =
        new (GetAllocator()) HArraySet(arr, sub1, c0, DataType::Type::kInt32, 0);
    HInstruction* arr_set_j = new (GetAllocator()) HArraySet(arr, j, c0, DataType::Type::kInt32, 0);
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

    HInstruction* block_instructions[] = {arr,
                                          i,
                                          j,
                                          object,
                                          add0,
                                          add1,
                                          sub0,
                                          sub1,
                                          arr_set_0,
                                          arr_set_1,
                                          arr_set_i,
                                          arr_set_add0,
                                          arr_set_add1,
                                          arr_set_sub0,
                                          arr_set_sub1,
                                          arr_set_j,
                                          set_field10};

    for (HInstruction* instr : block_instructions) {
      entry->AddInstruction(instr);
    }

    SchedulingGraph scheduling_graph(scheduler, GetScopedAllocator());
    HeapLocationCollector heap_location_collector(graph_);
    heap_location_collector.VisitBasicBlock(entry);
    heap_location_collector.BuildAliasingMatrix();
    scheduling_graph.SetHeapLocationCollector(heap_location_collector);

    for (HInstruction* instr : ReverseRange(block_instructions)) {
      // Build scheduling graph with memory access aliasing information
      // from LSA/heap_location_collector.
      scheduling_graph.AddNode(instr);
    }

    // LSA/HeapLocationCollector should see those ArraySet instructions.
    ASSERT_EQ(heap_location_collector.GetNumberOfHeapLocations(), 9U);
    ASSERT_TRUE(heap_location_collector.HasHeapStores());

    // Test queries on HeapLocationCollector's aliasing matrix after load store analysis.
    // HeapLocationCollector and SchedulingGraph should report consistent relationships.
    size_t loc1 = HeapLocationCollector::kHeapLocationNotFound;
    size_t loc2 = HeapLocationCollector::kHeapLocationNotFound;

    // Test side effect dependency: array[0] and array[1]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, c0);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, c1);
    ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(arr_set_1, arr_set_0));

    // Test side effect dependency based on LSA analysis: array[i] and array[j]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, i);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, j);
    ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_i));

    // Test side effect dependency based on LSA analysis: array[i] and array[i+0]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, i);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, add0);
    ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_add0, arr_set_i));

    // Test side effect dependency based on LSA analysis: array[i] and array[i-0]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, i);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, sub0);
    ASSERT_TRUE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_sub0, arr_set_i));

    // Test side effect dependency based on LSA analysis: array[i] and array[i+1]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, i);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, add1);
    ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(arr_set_add1, arr_set_i));

    // Test side effect dependency based on LSA analysis: array[i+1] and array[i-1]
    loc1 = heap_location_collector.GetArrayHeapLocation(arr, add1);
    loc2 = heap_location_collector.GetArrayHeapLocation(arr, sub1);
    ASSERT_FALSE(heap_location_collector.MayAlias(loc1, loc2));
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(arr_set_sub1, arr_set_add1));

    // Test side effect dependency based on LSA analysis: array[j] and all others array accesses
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_i));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_add0));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_sub0));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_add1));
    ASSERT_TRUE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, arr_set_sub1));

    // Test that ArraySet and FieldSet should not have side effect dependency
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(arr_set_i, set_field10));
    ASSERT_FALSE(scheduling_graph.HasImmediateOtherDependency(arr_set_j, set_field10));

    // Exercise target specific scheduler and SchedulingLatencyVisitor.
    scheduler->Schedule(graph_);
  }

  HGraph* graph_;
};

#if defined(ART_ENABLE_CODEGEN_arm64)
TEST_F(SchedulerTest, DependencyGraphAndSchedulerARM64) {
  CriticalPathSchedulingNodeSelector critical_path_selector;
  arm64::HSchedulerARM64 scheduler(GetScopedAllocator(), &critical_path_selector);
  TestBuildDependencyGraphAndSchedule(&scheduler);
}

TEST_F(SchedulerTest, ArrayAccessAliasingARM64) {
  CriticalPathSchedulingNodeSelector critical_path_selector;
  arm64::HSchedulerARM64 scheduler(GetScopedAllocator(), &critical_path_selector);
  TestDependencyGraphOnAliasingArrayAccesses(&scheduler);
}
#endif

#if defined(ART_ENABLE_CODEGEN_arm)
TEST_F(SchedulerTest, DependencyGraphAndSchedulerARM) {
  CriticalPathSchedulingNodeSelector critical_path_selector;
  arm::SchedulingLatencyVisitorARM arm_latency_visitor(/*CodeGenerator*/ nullptr);
  arm::HSchedulerARM scheduler(GetScopedAllocator(), &critical_path_selector, &arm_latency_visitor);
  TestBuildDependencyGraphAndSchedule(&scheduler);
}

TEST_F(SchedulerTest, ArrayAccessAliasingARM) {
  CriticalPathSchedulingNodeSelector critical_path_selector;
  arm::SchedulingLatencyVisitorARM arm_latency_visitor(/*CodeGenerator*/ nullptr);
  arm::HSchedulerARM scheduler(GetScopedAllocator(), &critical_path_selector, &arm_latency_visitor);
  TestDependencyGraphOnAliasingArrayAccesses(&scheduler);
}
#endif

TEST_F(SchedulerTest, RandomScheduling) {
  //
  // Java source: crafted code to make sure (random) scheduling should get correct result.
  //
  //  int result = 0;
  //  float fr = 10.0f;
  //  for (int i = 1; i < 10; i++) {
  //    fr ++;
  //    int t1 = result >> i;
  //    int t2 = result * i;
  //    result = result + t1 - t2;
  //    fr = fr / i;
  //    result += (int)fr;
  //  }
  //  return result;
  //
  const std::vector<uint16_t> data = SIX_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 12 | 2 << 8,          // const/4 v2, #int 0
    Instruction::CONST_HIGH16 | 0 << 8, 0x4120,       // const/high16 v0, #float 10.0 // #41200000
    Instruction::CONST_4 | 1 << 12 | 1 << 8,          // const/4 v1, #int 1
    Instruction::CONST_16 | 5 << 8, 0x000a,           // const/16 v5, #int 10
    Instruction::IF_GE | 5 << 12 | 1 << 8, 0x0014,    // if-ge v1, v5, 001a // +0014
    Instruction::CONST_HIGH16 | 5 << 8, 0x3f80,       // const/high16 v5, #float 1.0 // #3f800000
    Instruction::ADD_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // add-float/2addr v0, v5
    Instruction::SHR_INT | 3 << 8, 1 << 8 | 2 ,       // shr-int v3, v2, v1
    Instruction::MUL_INT | 4 << 8, 1 << 8 | 2,        // mul-int v4, v2, v1
    Instruction::ADD_INT | 5 << 8, 3 << 8 | 2,        // add-int v5, v2, v3
    Instruction::SUB_INT | 2 << 8, 4 << 8 | 5,        // sub-int v2, v5, v4
    Instruction::INT_TO_FLOAT | 1 << 12 | 5 << 8,     // int-to-float v5, v1
    Instruction::DIV_FLOAT_2ADDR | 5 << 12 | 0 << 8,  // div-float/2addr v0, v5
    Instruction::FLOAT_TO_INT | 0 << 12 | 5 << 8,     // float-to-int v5, v0
    Instruction::ADD_INT_2ADDR | 5 << 12 | 2 << 8,    // add-int/2addr v2, v5
    Instruction::ADD_INT_LIT8 | 1 << 8, 1 << 8 | 1,   // add-int/lit8 v1, v1, #int 1 // #01
    Instruction::GOTO | 0xeb << 8,                    // goto 0004 // -0015
    Instruction::RETURN | 2 << 8);                    // return v2

  constexpr int kNumberOfRuns = 10;
  for (int i = 0; i < kNumberOfRuns; ++i) {
    CompileWithRandomSchedulerAndRun(data, true, 138774);
  }
}

}  // namespace art
