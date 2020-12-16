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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_

#include "code_generator_arm_vixl.h"
#include "scheduler.h"

namespace art {
namespace arm {
// TODO: Replace CodeGeneratorARMType with CodeGeneratorARMVIXL everywhere?
typedef CodeGeneratorARMVIXL CodeGeneratorARMType;

// AArch32 instruction latencies.
// We currently assume that all ARM CPUs share the same instruction latency list.
// The following latencies were tuned based on performance experiments and
// automatic tuning using differential evolution approach on various benchmarks.
static constexpr uint32_t kArmIntegerOpLatency = 2;
static constexpr uint32_t kArmFloatingPointOpLatency = 11;
static constexpr uint32_t kArmDataProcWithShifterOpLatency = 4;
static constexpr uint32_t kArmMulIntegerLatency = 6;
static constexpr uint32_t kArmMulFloatingPointLatency = 11;
static constexpr uint32_t kArmDivIntegerLatency = 10;
static constexpr uint32_t kArmDivFloatLatency = 20;
static constexpr uint32_t kArmDivDoubleLatency = 25;
static constexpr uint32_t kArmTypeConversionFloatingPointIntegerLatency = 11;
static constexpr uint32_t kArmMemoryLoadLatency = 9;
static constexpr uint32_t kArmMemoryStoreLatency = 9;
static constexpr uint32_t kArmMemoryBarrierLatency = 6;
static constexpr uint32_t kArmBranchLatency = 4;
static constexpr uint32_t kArmCallLatency = 5;
static constexpr uint32_t kArmCallInternalLatency = 29;
static constexpr uint32_t kArmLoadStringInternalLatency = 10;
static constexpr uint32_t kArmNopLatency = 2;
static constexpr uint32_t kArmLoadWithBakerReadBarrierLatency = 18;
static constexpr uint32_t kArmRuntimeTypeCheckLatency = 46;

class SchedulingLatencyVisitorARM : public SchedulingLatencyVisitor {
 public:
  explicit SchedulingLatencyVisitorARM(CodeGenerator* codegen)
      : codegen_(down_cast<CodeGeneratorARMType*>(codegen)) {}

  // Default visitor for instructions not handled specifically below.
  void VisitInstruction(HInstruction* ATTRIBUTE_UNUSED) {
    last_visited_latency_ = kArmIntegerOpLatency;
  }

// We add a second unused parameter to be able to use this macro like the others
// defined in `nodes.h`.
#define FOR_EACH_SCHEDULED_ARM_INSTRUCTION(M)    \
  M(ArrayGet         , unused)                   \
  M(ArrayLength      , unused)                   \
  M(ArraySet         , unused)                   \
  M(Add              , unused)                   \
  M(Sub              , unused)                   \
  M(And              , unused)                   \
  M(Or               , unused)                   \
  M(Ror              , unused)                   \
  M(Xor              , unused)                   \
  M(Shl              , unused)                   \
  M(Shr              , unused)                   \
  M(UShr             , unused)                   \
  M(Mul              , unused)                   \
  M(Div              , unused)                   \
  M(Condition        , unused)                   \
  M(Compare          , unused)                   \
  M(BoundsCheck      , unused)                   \
  M(InstanceFieldGet , unused)                   \
  M(InstanceFieldSet , unused)                   \
  M(InstanceOf       , unused)                   \
  M(Invoke           , unused)                   \
  M(LoadString       , unused)                   \
  M(NewArray         , unused)                   \
  M(NewInstance      , unused)                   \
  M(Rem              , unused)                   \
  M(StaticFieldGet   , unused)                   \
  M(StaticFieldSet   , unused)                   \
  M(SuspendCheck     , unused)                   \
  M(TypeConversion   , unused)

#define FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(M) \
  M(BitwiseNegatedRight, unused)                 \
  M(MultiplyAccumulate, unused)                  \
  M(IntermediateAddress, unused)                 \
  M(IntermediateAddressIndex, unused)            \
  M(DataProcWithShifterOp, unused)

#define DECLARE_VISIT_INSTRUCTION(type, unused)  \
  void Visit##type(H##type* instruction) OVERRIDE;

  FOR_EACH_SCHEDULED_ARM_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_ARM(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION

 private:
  bool CanGenerateTest(HCondition* cond);
  void HandleGenerateConditionWithZero(IfCondition cond);
  void HandleGenerateLongTestConstant(HCondition* cond);
  void HandleGenerateLongTest(HCondition* cond);
  void HandleGenerateLongComparesAndJumps();
  void HandleGenerateTest(HCondition* cond);
  void HandleGenerateConditionGeneric(HCondition* cond);
  void HandleGenerateEqualLong(HCondition* cond);
  void HandleGenerateConditionLong(HCondition* cond);
  void HandleGenerateConditionIntegralOrNonPrimitive(HCondition* cond);
  void HandleCondition(HCondition* instr);
  void HandleBinaryOperationLantencies(HBinaryOperation* instr);
  void HandleBitwiseOperationLantencies(HBinaryOperation* instr);
  void HandleShiftLatencies(HBinaryOperation* instr);
  void HandleDivRemConstantIntegralLatencies(int32_t imm);
  void HandleFieldSetLatencies(HInstruction* instruction, const FieldInfo& field_info);
  void HandleFieldGetLatencies(HInstruction* instruction, const FieldInfo& field_info);
  void HandleGenerateDataProcInstruction(bool internal_latency = false);
  void HandleGenerateDataProc(HDataProcWithShifterOp* instruction);
  void HandleGenerateLongDataProc(HDataProcWithShifterOp* instruction);

  // The latency setting for each HInstruction depends on how CodeGenerator may generate code,
  // latency visitors may query CodeGenerator for such information for accurate latency settings.
  CodeGeneratorARMType* codegen_;
};

class HSchedulerARM : public HScheduler {
 public:
  HSchedulerARM(ScopedArenaAllocator* allocator,
                SchedulingNodeSelector* selector,
                SchedulingLatencyVisitorARM* arm_latency_visitor)
      : HScheduler(allocator, arm_latency_visitor, selector) {}
  ~HSchedulerARM() OVERRIDE {}

  bool IsSchedulable(const HInstruction* instruction) const OVERRIDE {
#define CASE_INSTRUCTION_KIND(type, unused) case \
  HInstruction::InstructionKind::k##type:
    switch (instruction->GetKind()) {
      FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(CASE_INSTRUCTION_KIND)
        return true;
      FOR_EACH_CONCRETE_INSTRUCTION_ARM(CASE_INSTRUCTION_KIND)
        return true;
      default:
        return HScheduler::IsSchedulable(instruction);
    }
#undef CASE_INSTRUCTION_KIND
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HSchedulerARM);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_
