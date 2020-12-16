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

#include "optimization.h"

#ifdef ART_ENABLE_CODEGEN_arm
#include "instruction_simplifier_arm.h"
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
#include "instruction_simplifier_arm64.h"
#endif
#ifdef ART_ENABLE_CODEGEN_mips
#include "instruction_simplifier_mips.h"
#include "pc_relative_fixups_mips.h"
#endif
#ifdef ART_ENABLE_CODEGEN_x86
#include "pc_relative_fixups_x86.h"
#endif
#if defined(ART_ENABLE_CODEGEN_x86) || defined(ART_ENABLE_CODEGEN_x86_64)
#include "x86_memory_gen.h"
#endif

#include "bounds_check_elimination.h"
#include "cha_guard_optimization.h"
#include "code_sinking.h"
#include "constant_folding.h"
#include "constructor_fence_redundancy_elimination.h"
#include "dead_code_elimination.h"
#include "dex/code_item_accessors-inl.h"
#include "driver/dex_compilation_unit.h"
#include "gvn.h"
#include "induction_var_analysis.h"
#include "inliner.h"
#include "instruction_simplifier.h"
#include "intrinsics.h"
#include "licm.h"
#include "load_store_analysis.h"
#include "load_store_elimination.h"
#include "loop_optimization.h"
#include "scheduler.h"
#include "select_generator.h"
#include "sharpening.h"
#include "side_effects_analysis.h"

// Decide between default or alternative pass name.

namespace art {

const char* OptimizationPassName(OptimizationPass pass) {
  switch (pass) {
    case OptimizationPass::kSideEffectsAnalysis:
      return SideEffectsAnalysis::kSideEffectsAnalysisPassName;
    case OptimizationPass::kInductionVarAnalysis:
      return HInductionVarAnalysis::kInductionPassName;
    case OptimizationPass::kLoadStoreAnalysis:
      return LoadStoreAnalysis::kLoadStoreAnalysisPassName;
    case OptimizationPass::kGlobalValueNumbering:
      return GVNOptimization::kGlobalValueNumberingPassName;
    case OptimizationPass::kInvariantCodeMotion:
      return LICM::kLoopInvariantCodeMotionPassName;
    case OptimizationPass::kLoopOptimization:
      return HLoopOptimization::kLoopOptimizationPassName;
    case OptimizationPass::kBoundsCheckElimination:
      return BoundsCheckElimination::kBoundsCheckEliminationPassName;
    case OptimizationPass::kLoadStoreElimination:
      return LoadStoreElimination::kLoadStoreEliminationPassName;
    case OptimizationPass::kConstantFolding:
      return HConstantFolding::kConstantFoldingPassName;
    case OptimizationPass::kDeadCodeElimination:
      return HDeadCodeElimination::kDeadCodeEliminationPassName;
    case OptimizationPass::kInliner:
      return HInliner::kInlinerPassName;
    case OptimizationPass::kSharpening:
      return HSharpening::kSharpeningPassName;
    case OptimizationPass::kSelectGenerator:
      return HSelectGenerator::kSelectGeneratorPassName;
    case OptimizationPass::kInstructionSimplifier:
      return InstructionSimplifier::kInstructionSimplifierPassName;
    case OptimizationPass::kIntrinsicsRecognizer:
      return IntrinsicsRecognizer::kIntrinsicsRecognizerPassName;
    case OptimizationPass::kCHAGuardOptimization:
      return CHAGuardOptimization::kCHAGuardOptimizationPassName;
    case OptimizationPass::kCodeSinking:
      return CodeSinking::kCodeSinkingPassName;
    case OptimizationPass::kConstructorFenceRedundancyElimination:
      return ConstructorFenceRedundancyElimination::kCFREPassName;
    case OptimizationPass::kScheduling:
      return HInstructionScheduling::kInstructionSchedulingPassName;
#ifdef ART_ENABLE_CODEGEN_arm
    case OptimizationPass::kInstructionSimplifierArm:
      return arm::InstructionSimplifierArm::kInstructionSimplifierArmPassName;
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
    case OptimizationPass::kInstructionSimplifierArm64:
      return arm64::InstructionSimplifierArm64::kInstructionSimplifierArm64PassName;
#endif
#ifdef ART_ENABLE_CODEGEN_mips
    case OptimizationPass::kPcRelativeFixupsMips:
      return mips::PcRelativeFixups::kPcRelativeFixupsMipsPassName;
    case OptimizationPass::kInstructionSimplifierMips:
      return mips::InstructionSimplifierMips::kInstructionSimplifierMipsPassName;
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case OptimizationPass::kPcRelativeFixupsX86:
      return x86::PcRelativeFixups::kPcRelativeFixupsX86PassName;
#endif
#if defined(ART_ENABLE_CODEGEN_x86) || defined(ART_ENABLE_CODEGEN_x86_64)
    case OptimizationPass::kX86MemoryOperandGeneration:
      return x86::X86MemoryOperandGeneration::kX86MemoryOperandGenerationPassName;
#endif
  }
}

#define X(x) if (name == OptimizationPassName((x))) return (x)

OptimizationPass OptimizationPassByName(const std::string& name) {
  X(OptimizationPass::kBoundsCheckElimination);
  X(OptimizationPass::kCHAGuardOptimization);
  X(OptimizationPass::kCodeSinking);
  X(OptimizationPass::kConstantFolding);
  X(OptimizationPass::kConstructorFenceRedundancyElimination);
  X(OptimizationPass::kDeadCodeElimination);
  X(OptimizationPass::kGlobalValueNumbering);
  X(OptimizationPass::kInductionVarAnalysis);
  X(OptimizationPass::kInliner);
  X(OptimizationPass::kInstructionSimplifier);
  X(OptimizationPass::kIntrinsicsRecognizer);
  X(OptimizationPass::kInvariantCodeMotion);
  X(OptimizationPass::kLoadStoreAnalysis);
  X(OptimizationPass::kLoadStoreElimination);
  X(OptimizationPass::kLoopOptimization);
  X(OptimizationPass::kScheduling);
  X(OptimizationPass::kSelectGenerator);
  X(OptimizationPass::kSharpening);
  X(OptimizationPass::kSideEffectsAnalysis);
#ifdef ART_ENABLE_CODEGEN_arm
  X(OptimizationPass::kInstructionSimplifierArm);
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
  X(OptimizationPass::kInstructionSimplifierArm64);
#endif
#ifdef ART_ENABLE_CODEGEN_mips
  X(OptimizationPass::kPcRelativeFixupsMips);
  X(OptimizationPass::kInstructionSimplifierMips);
#endif
#ifdef ART_ENABLE_CODEGEN_x86
  X(OptimizationPass::kPcRelativeFixupsX86);
  X(OptimizationPass::kX86MemoryOperandGeneration);
#endif
  LOG(FATAL) << "Cannot find optimization " << name;
  UNREACHABLE();
}

#undef X

ArenaVector<HOptimization*> ConstructOptimizations(
    const OptimizationDef definitions[],
    size_t length,
    ArenaAllocator* allocator,
    HGraph* graph,
    OptimizingCompilerStats* stats,
    CodeGenerator* codegen,
    CompilerDriver* driver,
    const DexCompilationUnit& dex_compilation_unit,
    VariableSizedHandleScope* handles) {
  ArenaVector<HOptimization*> optimizations(allocator->Adapter());

  // Some optimizations require SideEffectsAnalysis or HInductionVarAnalysis
  // instances. This method uses the nearest instance preceeding it in the pass
  // name list or fails fatally if no such analysis can be found.
  SideEffectsAnalysis* most_recent_side_effects = nullptr;
  HInductionVarAnalysis* most_recent_induction = nullptr;
  LoadStoreAnalysis* most_recent_lsa = nullptr;

  // Loop over the requested optimizations.
  for (size_t i = 0; i < length; i++) {
    OptimizationPass pass = definitions[i].first;
    const char* alt_name = definitions[i].second;
    const char* name = alt_name != nullptr
        ? alt_name
        : OptimizationPassName(pass);
    HOptimization* opt = nullptr;

    switch (pass) {
      //
      // Analysis passes (kept in most recent for subsequent passes).
      //
      case OptimizationPass::kSideEffectsAnalysis:
        opt = most_recent_side_effects = new (allocator) SideEffectsAnalysis(graph, name);
        break;
      case OptimizationPass::kInductionVarAnalysis:
        opt = most_recent_induction = new (allocator) HInductionVarAnalysis(graph, name);
        break;
      case OptimizationPass::kLoadStoreAnalysis:
        opt = most_recent_lsa = new (allocator) LoadStoreAnalysis(graph, name);
        break;
      //
      // Passes that need prior analysis.
      //
      case OptimizationPass::kGlobalValueNumbering:
        CHECK(most_recent_side_effects != nullptr);
        opt = new (allocator) GVNOptimization(graph, *most_recent_side_effects, name);
        break;
      case OptimizationPass::kInvariantCodeMotion:
        CHECK(most_recent_side_effects != nullptr);
        opt = new (allocator) LICM(graph, *most_recent_side_effects, stats, name);
        break;
      case OptimizationPass::kLoopOptimization:
        CHECK(most_recent_induction != nullptr);
        opt = new (allocator) HLoopOptimization(graph, driver, most_recent_induction, stats, name);
        break;
      case OptimizationPass::kBoundsCheckElimination:
        CHECK(most_recent_side_effects != nullptr && most_recent_induction != nullptr);
        opt = new (allocator) BoundsCheckElimination(
            graph, *most_recent_side_effects, most_recent_induction, name);
        break;
      case OptimizationPass::kLoadStoreElimination:
        CHECK(most_recent_side_effects != nullptr && most_recent_induction != nullptr);
        opt = new (allocator) LoadStoreElimination(
            graph, *most_recent_side_effects, *most_recent_lsa, stats, name);
        break;
      //
      // Regular passes.
      //
      case OptimizationPass::kConstantFolding:
        opt = new (allocator) HConstantFolding(graph, name);
        break;
      case OptimizationPass::kDeadCodeElimination:
        opt = new (allocator) HDeadCodeElimination(graph, stats, name);
        break;
      case OptimizationPass::kInliner: {
        CodeItemDataAccessor accessor(*dex_compilation_unit.GetDexFile(),
                                      dex_compilation_unit.GetCodeItem());
        opt = new (allocator) HInliner(graph,                   // outer_graph
                                       graph,                   // outermost_graph
                                       codegen,
                                       dex_compilation_unit,    // outer_compilation_unit
                                       dex_compilation_unit,    // outermost_compilation_unit
                                       driver,
                                       handles,
                                       stats,
                                       accessor.RegistersSize(),
                                       /* total_number_of_instructions */ 0,
                                       /* parent */ nullptr,
                                       /* depth */ 0,
                                       name);
        break;
      }
      case OptimizationPass::kSharpening:
        opt = new (allocator) HSharpening(graph, codegen, driver, name);
        break;
      case OptimizationPass::kSelectGenerator:
        opt = new (allocator) HSelectGenerator(graph, handles, stats, name);
        break;
      case OptimizationPass::kInstructionSimplifier:
        opt = new (allocator) InstructionSimplifier(graph, codegen, driver, stats, name);
        break;
      case OptimizationPass::kIntrinsicsRecognizer:
        opt = new (allocator) IntrinsicsRecognizer(graph, stats, name);
        break;
      case OptimizationPass::kCHAGuardOptimization:
        opt = new (allocator) CHAGuardOptimization(graph, name);
        break;
      case OptimizationPass::kCodeSinking:
        opt = new (allocator) CodeSinking(graph, stats, name);
        break;
      case OptimizationPass::kConstructorFenceRedundancyElimination:
        opt = new (allocator) ConstructorFenceRedundancyElimination(graph, stats, name);
        break;
      case OptimizationPass::kScheduling:
        opt = new (allocator) HInstructionScheduling(
            graph, driver->GetInstructionSet(), codegen, name);
        break;
      //
      // Arch-specific passes.
      //
#ifdef ART_ENABLE_CODEGEN_arm
      case OptimizationPass::kInstructionSimplifierArm:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) arm::InstructionSimplifierArm(graph, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
      case OptimizationPass::kInstructionSimplifierArm64:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) arm64::InstructionSimplifierArm64(graph, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_mips
      case OptimizationPass::kPcRelativeFixupsMips:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) mips::PcRelativeFixups(graph, codegen, stats);
        break;
      case OptimizationPass::kInstructionSimplifierMips:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) mips::InstructionSimplifierMips(graph, codegen, stats);
        break;
#endif
#ifdef ART_ENABLE_CODEGEN_x86
      case OptimizationPass::kPcRelativeFixupsX86:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) x86::PcRelativeFixups(graph, codegen, stats);
        break;
      case OptimizationPass::kX86MemoryOperandGeneration:
        DCHECK(alt_name == nullptr) << "arch-specific pass does not support alternative name";
        opt = new (allocator) x86::X86MemoryOperandGeneration(graph, codegen, stats);
        break;
#endif
    }  // switch

    // Add each next optimization to result vector.
    CHECK(opt != nullptr);
    DCHECK_STREQ(name, opt->GetPassName());  // sanity
    optimizations.push_back(opt);
  }

  return optimizations;
}

}  // namespace art
