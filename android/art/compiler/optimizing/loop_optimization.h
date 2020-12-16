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

#ifndef ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_
#define ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "induction_var_range.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

class CompilerDriver;

/**
 * Loop optimizations. Builds a loop hierarchy and applies optimizations to
 * the detected nested loops, such as removal of dead induction and empty loops
 * and inner loop vectorization.
 */
class HLoopOptimization : public HOptimization {
 public:
  HLoopOptimization(HGraph* graph,
                    CompilerDriver* compiler_driver,
                    HInductionVarAnalysis* induction_analysis,
                    OptimizingCompilerStats* stats,
                    const char* name = kLoopOptimizationPassName);

  void Run() OVERRIDE;

  static constexpr const char* kLoopOptimizationPassName = "loop_optimization";

 private:
  /**
   * A single loop inside the loop hierarchy representation.
   */
  struct LoopNode : public ArenaObject<kArenaAllocLoopOptimization> {
    explicit LoopNode(HLoopInformation* lp_info)
        : loop_info(lp_info),
          outer(nullptr),
          inner(nullptr),
          previous(nullptr),
          next(nullptr) {}
    HLoopInformation* loop_info;
    LoopNode* outer;
    LoopNode* inner;
    LoopNode* previous;
    LoopNode* next;
  };

  /*
   * Vectorization restrictions (bit mask).
   */
  enum VectorRestrictions {
    kNone            = 0,        // no restrictions
    kNoMul           = 1 << 0,   // no multiplication
    kNoDiv           = 1 << 1,   // no division
    kNoShift         = 1 << 2,   // no shift
    kNoShr           = 1 << 3,   // no arithmetic shift right
    kNoHiBits        = 1 << 4,   // "wider" operations cannot bring in higher order bits
    kNoSignedHAdd    = 1 << 5,   // no signed halving add
    kNoUnroundedHAdd = 1 << 6,   // no unrounded halving add
    kNoAbs           = 1 << 7,   // no absolute value
    kNoStringCharAt  = 1 << 8,   // no StringCharAt
    kNoReduction     = 1 << 9,   // no reduction
    kNoSAD           = 1 << 10,  // no sum of absolute differences (SAD)
    kNoWideSAD       = 1 << 11,  // no sum of absolute differences (SAD) with operand widening
  };

  /*
   * Vectorization mode during synthesis
   * (sequential peeling/cleanup loop or vector loop).
   */
  enum VectorMode {
    kSequential,
    kVector
  };

  /*
   * Representation of a unit-stride array reference.
   */
  struct ArrayReference {
    ArrayReference(HInstruction* b, HInstruction* o, DataType::Type t, bool l, bool c = false)
        : base(b), offset(o), type(t), lhs(l), is_string_char_at(c) { }
    bool operator<(const ArrayReference& other) const {
      return
          (base < other.base) ||
          (base == other.base &&
           (offset < other.offset || (offset == other.offset &&
                                      (type < other.type ||
                                       (type == other.type &&
                                        (lhs < other.lhs ||
                                         (lhs == other.lhs &&
                                          is_string_char_at < other.is_string_char_at)))))));
    }
    HInstruction* base;      // base address
    HInstruction* offset;    // offset + i
    DataType::Type type;     // component type
    bool lhs;                // def/use
    bool is_string_char_at;  // compressed string read
  };

  //
  // Loop setup and traversal.
  //

  void LocalRun();
  void AddLoop(HLoopInformation* loop_info);
  void RemoveLoop(LoopNode* node);

  // Traverses all loops inner to outer to perform simplifications and optimizations.
  // Returns true if loops nested inside current loop (node) have changed.
  bool TraverseLoopsInnerToOuter(LoopNode* node);

  //
  // Optimization.
  //

  void SimplifyInduction(LoopNode* node);
  void SimplifyBlocks(LoopNode* node);

  // Performs optimizations specific to inner loop (empty loop removal,
  // unrolling, vectorization). Returns true if anything changed.
  bool OptimizeInnerLoop(LoopNode* node);

  //
  // Vectorization analysis and synthesis.
  //

  bool ShouldVectorize(LoopNode* node, HBasicBlock* block, int64_t trip_count);
  void Vectorize(LoopNode* node, HBasicBlock* block, HBasicBlock* exit, int64_t trip_count);
  void GenerateNewLoop(LoopNode* node,
                       HBasicBlock* block,
                       HBasicBlock* new_preheader,
                       HInstruction* lo,
                       HInstruction* hi,
                       HInstruction* step,
                       uint32_t unroll);
  bool VectorizeDef(LoopNode* node, HInstruction* instruction, bool generate_code);
  bool VectorizeUse(LoopNode* node,
                    HInstruction* instruction,
                    bool generate_code,
                    DataType::Type type,
                    uint64_t restrictions);
  uint32_t GetVectorSizeInBytes();
  bool TrySetVectorType(DataType::Type type, /*out*/ uint64_t* restrictions);
  bool TrySetVectorLength(uint32_t length);
  void GenerateVecInv(HInstruction* org, DataType::Type type);
  void GenerateVecSub(HInstruction* org, HInstruction* offset);
  void GenerateVecMem(HInstruction* org,
                      HInstruction* opa,
                      HInstruction* opb,
                      HInstruction* offset,
                      DataType::Type type);
  void GenerateVecReductionPhi(HPhi* phi);
  void GenerateVecReductionPhiInputs(HPhi* phi, HInstruction* reduction);
  HInstruction* ReduceAndExtractIfNeeded(HInstruction* instruction);
  void GenerateVecOp(HInstruction* org,
                     HInstruction* opa,
                     HInstruction* opb,
                     DataType::Type type);

  // Vectorization idioms.
  bool VectorizeHalvingAddIdiom(LoopNode* node,
                                HInstruction* instruction,
                                bool generate_code,
                                DataType::Type type,
                                uint64_t restrictions);
  bool VectorizeSADIdiom(LoopNode* node,
                         HInstruction* instruction,
                         bool generate_code,
                         DataType::Type type,
                         uint64_t restrictions);

  // Vectorization heuristics.
  Alignment ComputeAlignment(HInstruction* offset,
                             DataType::Type type,
                             bool is_string_char_at,
                             uint32_t peeling = 0);
  void SetAlignmentStrategy(uint32_t peeling_votes[],
                            const ArrayReference* peeling_candidate);
  uint32_t MaxNumberPeeled();
  bool IsVectorizationProfitable(int64_t trip_count);
  uint32_t GetUnrollingFactor(HBasicBlock* block, int64_t trip_count);

  //
  // Helpers.
  //

  bool TrySetPhiInduction(HPhi* phi, bool restrict_uses);
  bool TrySetPhiReduction(HPhi* phi);

  // Detects loop header with a single induction (returned in main_phi), possibly
  // other phis for reductions, but no other side effects. Returns true on success.
  bool TrySetSimpleLoopHeader(HBasicBlock* block, /*out*/ HPhi** main_phi);

  bool IsEmptyBody(HBasicBlock* block);
  bool IsOnlyUsedAfterLoop(HLoopInformation* loop_info,
                           HInstruction* instruction,
                           bool collect_loop_uses,
                           /*out*/ uint32_t* use_count);
  bool IsUsedOutsideLoop(HLoopInformation* loop_info,
                         HInstruction* instruction);
  bool TryReplaceWithLastValue(HLoopInformation* loop_info,
                               HInstruction* instruction,
                               HBasicBlock* block);
  bool TryAssignLastValue(HLoopInformation* loop_info,
                          HInstruction* instruction,
                          HBasicBlock* block,
                          bool collect_loop_uses);
  void RemoveDeadInstructions(const HInstructionList& list);
  bool CanRemoveCycle();  // Whether the current 'iset_' is removable.

  // Compiler driver (to query ISA features).
  const CompilerDriver* compiler_driver_;

  // Range information based on prior induction variable analysis.
  InductionVarRange induction_range_;

  // Phase-local heap memory allocator for the loop optimizer. Storage obtained
  // through this allocator is immediately released when the loop optimizer is done.
  ScopedArenaAllocator* loop_allocator_;

  // Global heap memory allocator. Used to build HIR.
  ArenaAllocator* global_allocator_;

  // Entries into the loop hierarchy representation. The hierarchy resides
  // in phase-local heap memory.
  LoopNode* top_loop_;
  LoopNode* last_loop_;

  // Temporary bookkeeping of a set of instructions.
  // Contents reside in phase-local heap memory.
  ScopedArenaSet<HInstruction*>* iset_;

  // Temporary bookkeeping of reduction instructions. Mapping is two-fold:
  // (1) reductions in the loop-body are mapped back to their phi definition,
  // (2) phi definitions are mapped to their initial value (updated during
  //     code generation to feed the proper values into the new chain).
  // Contents reside in phase-local heap memory.
  ScopedArenaSafeMap<HInstruction*, HInstruction*>* reductions_;

  // Flag that tracks if any simplifications have occurred.
  bool simplified_;

  // Number of "lanes" for selected packed type.
  uint32_t vector_length_;

  // Set of array references in the vector loop.
  // Contents reside in phase-local heap memory.
  ScopedArenaSet<ArrayReference>* vector_refs_;

  // Static or dynamic loop peeling for alignment.
  uint32_t vector_static_peeling_factor_;
  const ArrayReference* vector_dynamic_peeling_candidate_;

  // Dynamic data dependence test of the form a != b.
  HInstruction* vector_runtime_test_a_;
  HInstruction* vector_runtime_test_b_;

  // Mapping used during vectorization synthesis for both the scalar peeling/cleanup
  // loop (mode is kSequential) and the actual vector loop (mode is kVector). The data
  // structure maps original instructions into the new instructions.
  // Contents reside in phase-local heap memory.
  ScopedArenaSafeMap<HInstruction*, HInstruction*>* vector_map_;

  // Permanent mapping used during vectorization synthesis.
  // Contents reside in phase-local heap memory.
  ScopedArenaSafeMap<HInstruction*, HInstruction*>* vector_permanent_map_;

  // Temporary vectorization bookkeeping.
  VectorMode vector_mode_;  // synthesis mode
  HBasicBlock* vector_preheader_;  // preheader of the new loop
  HBasicBlock* vector_header_;  // header of the new loop
  HBasicBlock* vector_body_;  // body of the new loop
  HInstruction* vector_index_;  // normalized index of the new loop

  friend class LoopOptimizationTest;

  DISALLOW_COPY_AND_ASSIGN(HLoopOptimization);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_
