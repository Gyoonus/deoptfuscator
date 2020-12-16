/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "induction_var_analysis.h"
#include "induction_var_range.h"

namespace art {

/**
 * Since graph traversal may enter a SCC at any position, an initial representation may be rotated,
 * along dependences, viz. any of (a, b, c, d), (d, a, b, c)  (c, d, a, b), (b, c, d, a) assuming
 * a chain of dependences (mutual independent items may occur in arbitrary order). For proper
 * classification, the lexicographically first loop-phi is rotated to the front.
 */
static void RotateEntryPhiFirst(HLoopInformation* loop,
                                ArenaVector<HInstruction*>* scc,
                                ArenaVector<HInstruction*>* new_scc) {
  // Find very first loop-phi.
  const HInstructionList& phis = loop->GetHeader()->GetPhis();
  HInstruction* phi = nullptr;
  size_t phi_pos = -1;
  const size_t size = scc->size();
  for (size_t i = 0; i < size; i++) {
    HInstruction* other = (*scc)[i];
    if (other->IsLoopHeaderPhi() && (phi == nullptr || phis.FoundBefore(other, phi))) {
      phi = other;
      phi_pos = i;
    }
  }

  // If found, bring that loop-phi to front.
  if (phi != nullptr) {
    new_scc->clear();
    for (size_t i = 0; i < size; i++) {
      new_scc->push_back((*scc)[phi_pos]);
      if (++phi_pos >= size) phi_pos = 0;
    }
    DCHECK_EQ(size, new_scc->size());
    scc->swap(*new_scc);
  }
}

/**
 * Returns true if the from/to types denote a narrowing, integral conversion (precision loss).
 */
static bool IsNarrowingIntegralConversion(DataType::Type from, DataType::Type to) {
  switch (from) {
    case DataType::Type::kInt64:
      return to == DataType::Type::kUint8 ||
             to == DataType::Type::kInt8 ||
             to == DataType::Type::kUint16 ||
             to == DataType::Type::kInt16 ||
             to == DataType::Type::kInt32;
    case DataType::Type::kInt32:
      return to == DataType::Type::kUint8 ||
             to == DataType::Type::kInt8 ||
             to == DataType::Type::kUint16 ||
             to == DataType::Type::kInt16;
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      return to == DataType::Type::kUint8 || to == DataType::Type::kInt8;
    default:
      return false;
  }
}

/**
 * Returns result of implicit widening type conversion done in HIR.
 */
static DataType::Type ImplicitConversion(DataType::Type type) {
  switch (type) {
    case DataType::Type::kBool:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kUint16:
    case DataType::Type::kInt16:
      return DataType::Type::kInt32;
    default:
      return type;
  }
}

/**
 * Returns true if loop is guarded by "a cmp b" on entry.
 */
static bool IsGuardedBy(HLoopInformation* loop,
                        IfCondition cmp,
                        HInstruction* a,
                        HInstruction* b) {
  // Chase back through straightline code to the first potential
  // block that has a control dependence.
  // guard:   if (x) bypass
  //              |
  // entry: straightline code
  //              |
  //           preheader
  //              |
  //            header
  HBasicBlock* guard = loop->GetPreHeader();
  HBasicBlock* entry = loop->GetHeader();
  while (guard->GetPredecessors().size() == 1 &&
         guard->GetSuccessors().size() == 1) {
    entry = guard;
    guard = guard->GetSinglePredecessor();
  }
  // Find guard.
  HInstruction* control = guard->GetLastInstruction();
  if (!control->IsIf()) {
    return false;
  }
  HIf* ifs = control->AsIf();
  HInstruction* if_expr = ifs->InputAt(0);
  if (if_expr->IsCondition()) {
    IfCondition other_cmp = ifs->IfTrueSuccessor() == entry
        ? if_expr->AsCondition()->GetCondition()
        : if_expr->AsCondition()->GetOppositeCondition();
    if (if_expr->InputAt(0) == a && if_expr->InputAt(1) == b) {
      return cmp == other_cmp;
    } else if (if_expr->InputAt(1) == a && if_expr->InputAt(0) == b) {
      switch (cmp) {
        case kCondLT: return other_cmp == kCondGT;
        case kCondLE: return other_cmp == kCondGE;
        case kCondGT: return other_cmp == kCondLT;
        case kCondGE: return other_cmp == kCondLE;
        default: LOG(FATAL) << "unexpected cmp: " << cmp;
      }
    }
  }
  return false;
}

/* Finds first loop header phi use. */
HInstruction* FindFirstLoopHeaderPhiUse(HLoopInformation* loop, HInstruction* instruction) {
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    if (use.GetUser()->GetBlock() == loop->GetHeader() &&
        use.GetUser()->IsPhi() &&
        use.GetUser()->InputAt(1) == instruction) {
      return use.GetUser();
    }
  }
  return nullptr;
}

/**
 * Relinks the Phi structure after break-loop rewriting.
 */
bool FixOutsideUse(HLoopInformation* loop,
                   HInstruction* instruction,
                   HInstruction* replacement,
                   bool rewrite) {
  // Deal with regular uses.
  const HUseList<HInstruction*>& uses = instruction->GetUses();
  for (auto it = uses.begin(), end = uses.end(); it != end; ) {
    HInstruction* user = it->GetUser();
    size_t index = it->GetIndex();
    ++it;  // increment prior to potential removal
    if (user->GetBlock()->GetLoopInformation() != loop) {
      if (replacement == nullptr) {
        return false;
      } else if (rewrite) {
        user->ReplaceInput(replacement, index);
      }
    }
  }
  // Deal with environment uses.
  const HUseList<HEnvironment*>& env_uses = instruction->GetEnvUses();
  for (auto it = env_uses.begin(), end = env_uses.end(); it != end;) {
    HEnvironment* user = it->GetUser();
    size_t index = it->GetIndex();
    ++it;  // increment prior to potential removal
    if (user->GetHolder()->GetBlock()->GetLoopInformation() != loop) {
      if (replacement == nullptr) {
        return false;
      } else if (rewrite) {
        user->RemoveAsUserOfInput(index);
        user->SetRawEnvAt(index, replacement);
        replacement->AddEnvUseAt(user, index);
      }
    }
  }
  return true;
}

/**
 * Test and rewrite the loop body of a break-loop. Returns true on success.
 */
bool RewriteBreakLoopBody(HLoopInformation* loop,
                          HBasicBlock* body,
                          HInstruction* cond,
                          HInstruction* index,
                          HInstruction* upper,
                          bool rewrite) {
  // Deal with Phis. Outside use prohibited, except for index (which gets exit value).
  for (HInstructionIterator it(loop->GetHeader()->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* exit_value = it.Current() == index ? upper : nullptr;
    if (!FixOutsideUse(loop, it.Current(), exit_value, rewrite)) {
      return false;
    }
  }
  // Deal with other statements in header.
  for (HInstruction* m = cond->GetPrevious(), *p = nullptr; m && !m->IsSuspendCheck(); m = p) {
    p = m->GetPrevious();
    if (rewrite) {
      m->MoveBefore(body->GetFirstInstruction(), false);
    }
    if (!FixOutsideUse(loop, m, FindFirstLoopHeaderPhiUse(loop, m), rewrite)) {
      return false;
    }
  }
  return true;
}

//
// Class methods.
//

HInductionVarAnalysis::HInductionVarAnalysis(HGraph* graph, const char* name)
    : HOptimization(graph, name),
      global_depth_(0),
      stack_(graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)),
      map_(std::less<HInstruction*>(),
           graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)),
      scc_(graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)),
      cycle_(std::less<HInstruction*>(),
             graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)),
      type_(DataType::Type::kVoid),
      induction_(std::less<HLoopInformation*>(),
                 graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)),
      cycles_(std::less<HPhi*>(),
              graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)) {
}

void HInductionVarAnalysis::Run() {
  // Detects sequence variables (generalized induction variables) during an outer to inner
  // traversal of all loops using Gerlek's algorithm. The order is important to enable
  // range analysis on outer loop while visiting inner loops.
  for (HBasicBlock* graph_block : graph_->GetReversePostOrder()) {
    // Don't analyze irreducible loops.
    if (graph_block->IsLoopHeader() && !graph_block->GetLoopInformation()->IsIrreducible()) {
      VisitLoop(graph_block->GetLoopInformation());
    }
  }
}

void HInductionVarAnalysis::VisitLoop(HLoopInformation* loop) {
  // Find strongly connected components (SSCs) in the SSA graph of this loop using Tarjan's
  // algorithm. Due to the descendant-first nature, classification happens "on-demand".
  global_depth_ = 0;
  DCHECK(stack_.empty());
  map_.clear();

  for (HBlocksInLoopIterator it_loop(*loop); !it_loop.Done(); it_loop.Advance()) {
    HBasicBlock* loop_block = it_loop.Current();
    DCHECK(loop_block->IsInLoop());
    if (loop_block->GetLoopInformation() != loop) {
      continue;  // Inner loops visited later.
    }
    // Visit phi-operations and instructions.
    for (HInstructionIterator it(loop_block->GetPhis()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (!IsVisitedNode(instruction)) {
        VisitNode(loop, instruction);
      }
    }
    for (HInstructionIterator it(loop_block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (!IsVisitedNode(instruction)) {
        VisitNode(loop, instruction);
      }
    }
  }

  DCHECK(stack_.empty());
  map_.clear();

  // Determine the loop's trip-count.
  VisitControl(loop);
}

void HInductionVarAnalysis::VisitNode(HLoopInformation* loop, HInstruction* instruction) {
  const uint32_t d1 = ++global_depth_;
  map_.Put(instruction, NodeInfo(d1));
  stack_.push_back(instruction);

  // Visit all descendants.
  uint32_t low = d1;
  for (HInstruction* input : instruction->GetInputs()) {
    low = std::min(low, VisitDescendant(loop, input));
  }

  // Lower or found SCC?
  if (low < d1) {
    map_.find(instruction)->second.depth = low;
  } else {
    scc_.clear();
    cycle_.clear();

    // Pop the stack to build the SCC for classification.
    while (!stack_.empty()) {
      HInstruction* x = stack_.back();
      scc_.push_back(x);
      stack_.pop_back();
      map_.find(x)->second.done = true;
      if (x == instruction) {
        break;
      }
    }

    // Type of induction.
    type_ = scc_[0]->GetType();

    // Classify the SCC.
    if (scc_.size() == 1 && !scc_[0]->IsLoopHeaderPhi()) {
      ClassifyTrivial(loop, scc_[0]);
    } else {
      ClassifyNonTrivial(loop);
    }

    scc_.clear();
    cycle_.clear();
  }
}

uint32_t HInductionVarAnalysis::VisitDescendant(HLoopInformation* loop, HInstruction* instruction) {
  // If the definition is either outside the loop (loop invariant entry value)
  // or assigned in inner loop (inner exit value), the traversal stops.
  HLoopInformation* otherLoop = instruction->GetBlock()->GetLoopInformation();
  if (otherLoop != loop) {
    return global_depth_;
  }

  // Inspect descendant node.
  if (!IsVisitedNode(instruction)) {
    VisitNode(loop, instruction);
    return map_.find(instruction)->second.depth;
  } else {
    auto it = map_.find(instruction);
    return it->second.done ? global_depth_ : it->second.depth;
  }
}

void HInductionVarAnalysis::ClassifyTrivial(HLoopInformation* loop, HInstruction* instruction) {
  InductionInfo* info = nullptr;
  if (instruction->IsPhi()) {
    info = TransferPhi(loop, instruction, /*input_index*/ 0, /*adjust_input_size*/ 0);
  } else if (instruction->IsAdd()) {
    info = TransferAddSub(LookupInfo(loop, instruction->InputAt(0)),
                          LookupInfo(loop, instruction->InputAt(1)), kAdd);
  } else if (instruction->IsSub()) {
    info = TransferAddSub(LookupInfo(loop, instruction->InputAt(0)),
                          LookupInfo(loop, instruction->InputAt(1)), kSub);
  } else if (instruction->IsNeg()) {
    info = TransferNeg(LookupInfo(loop, instruction->InputAt(0)));
  } else if (instruction->IsMul()) {
    info = TransferMul(LookupInfo(loop, instruction->InputAt(0)),
                       LookupInfo(loop, instruction->InputAt(1)));
  } else if (instruction->IsShl()) {
    HInstruction* mulc = GetShiftConstant(loop, instruction, /*initial*/ nullptr);
    if (mulc != nullptr) {
      info = TransferMul(LookupInfo(loop, instruction->InputAt(0)),
                         LookupInfo(loop, mulc));
    }
  } else if (instruction->IsSelect()) {
    info = TransferPhi(loop, instruction, /*input_index*/ 0, /*adjust_input_size*/ 1);
  } else if (instruction->IsTypeConversion()) {
    info = TransferConversion(LookupInfo(loop, instruction->InputAt(0)),
                              instruction->AsTypeConversion()->GetInputType(),
                              instruction->AsTypeConversion()->GetResultType());
  } else if (instruction->IsBoundsCheck()) {
    info = LookupInfo(loop, instruction->InputAt(0));  // Pass-through.
  }

  // Successfully classified?
  if (info != nullptr) {
    AssignInfo(loop, instruction, info);
  }
}

void HInductionVarAnalysis::ClassifyNonTrivial(HLoopInformation* loop) {
  const size_t size = scc_.size();
  DCHECK_GE(size, 1u);

  // Rotate proper loop-phi to front.
  if (size > 1) {
    ArenaVector<HInstruction*> other(
        graph_->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis));
    RotateEntryPhiFirst(loop, &scc_, &other);
  }

  // Analyze from loop-phi onwards.
  HInstruction* phi = scc_[0];
  if (!phi->IsLoopHeaderPhi()) {
    return;
  }

  // External link should be loop invariant.
  InductionInfo* initial = LookupInfo(loop, phi->InputAt(0));
  if (initial == nullptr || initial->induction_class != kInvariant) {
    return;
  }

  // Store interesting cycle in each loop phi.
  for (size_t i = 0; i < size; i++) {
    if (scc_[i]->IsLoopHeaderPhi()) {
      AssignCycle(scc_[i]->AsPhi());
    }
  }

  // Singleton is wrap-around induction if all internal links have the same meaning.
  if (size == 1) {
    InductionInfo* update = TransferPhi(loop, phi, /*input_index*/ 1, /*adjust_input_size*/ 0);
    if (update != nullptr) {
      AssignInfo(loop, phi, CreateInduction(kWrapAround,
                                            kNop,
                                            initial,
                                            update,
                                            /*fetch*/ nullptr,
                                            type_));
    }
    return;
  }

  // Inspect remainder of the cycle that resides in scc_. The cycle_ mapping assigns
  // temporary meaning to its nodes, seeded from the phi instruction and back.
  for (size_t i = 1; i < size; i++) {
    HInstruction* instruction = scc_[i];
    InductionInfo* update = nullptr;
    if (instruction->IsPhi()) {
      update = SolvePhiAllInputs(loop, phi, instruction);
    } else if (instruction->IsAdd()) {
      update = SolveAddSub(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kAdd, true);
    } else if (instruction->IsSub()) {
      update = SolveAddSub(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kSub, true);
    } else if (instruction->IsMul()) {
      update = SolveOp(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kMul);
    } else if (instruction->IsDiv()) {
      update = SolveOp(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kDiv);
    } else if (instruction->IsRem()) {
      update = SolveOp(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kRem);
    } else if (instruction->IsShl()) {
      HInstruction* mulc = GetShiftConstant(loop, instruction, /*initial*/ nullptr);
      if (mulc != nullptr) {
        update = SolveOp(loop, phi, instruction, instruction->InputAt(0), mulc, kMul);
      }
    } else if (instruction->IsShr() || instruction->IsUShr()) {
      HInstruction* divc = GetShiftConstant(loop, instruction, initial);
      if (divc != nullptr) {
        update = SolveOp(loop, phi, instruction, instruction->InputAt(0), divc, kDiv);
      }
    } else if (instruction->IsXor()) {
      update = SolveOp(
          loop, phi, instruction, instruction->InputAt(0), instruction->InputAt(1), kXor);
    } else if (instruction->IsEqual()) {
      update = SolveTest(loop, phi, instruction, 0);
    } else if (instruction->IsNotEqual()) {
      update = SolveTest(loop, phi, instruction, 1);
    } else if (instruction->IsSelect()) {
      update = SolvePhi(instruction, /*input_index*/ 0, /*adjust_input_size*/ 1);  // acts like Phi
    } else if (instruction->IsTypeConversion()) {
      update = SolveConversion(loop, phi, instruction->AsTypeConversion());
    }
    if (update == nullptr) {
      return;
    }
    cycle_.Put(instruction, update);
  }

  // Success if all internal links received the same temporary meaning.
  InductionInfo* induction = SolvePhi(phi, /*input_index*/ 1, /*adjust_input_size*/ 0);
  if (induction != nullptr) {
    switch (induction->induction_class) {
      case kInvariant:
        // Construct combined stride of the linear induction.
        induction = CreateInduction(kLinear, kNop, induction, initial, /*fetch*/ nullptr, type_);
        FALLTHROUGH_INTENDED;
      case kPolynomial:
      case kGeometric:
      case kWrapAround:
        // Classify first phi and then the rest of the cycle "on-demand".
        // Statements are scanned in order.
        AssignInfo(loop, phi, induction);
        for (size_t i = 1; i < size; i++) {
          ClassifyTrivial(loop, scc_[i]);
        }
        break;
      case kPeriodic:
        // Classify all elements in the cycle with the found periodic induction while
        // rotating each first element to the end. Lastly, phi is classified.
        // Statements are scanned in reverse order.
        for (size_t i = size - 1; i >= 1; i--) {
          AssignInfo(loop, scc_[i], induction);
          induction = RotatePeriodicInduction(induction->op_b, induction->op_a);
        }
        AssignInfo(loop, phi, induction);
        break;
      default:
        break;
    }
  }
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::RotatePeriodicInduction(
    InductionInfo* induction,
    InductionInfo* last) {
  // Rotates a periodic induction of the form
  //   (a, b, c, d, e)
  // into
  //   (b, c, d, e, a)
  // in preparation of assigning this to the previous variable in the sequence.
  if (induction->induction_class == kInvariant) {
    return CreateInduction(kPeriodic,
                           kNop,
                           induction,
                           last,
                           /*fetch*/ nullptr,
                           type_);
  }
  return CreateInduction(kPeriodic,
                         kNop,
                         induction->op_a,
                         RotatePeriodicInduction(induction->op_b, last),
                         /*fetch*/ nullptr,
                         type_);
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::TransferPhi(HLoopInformation* loop,
                                                                         HInstruction* phi,
                                                                         size_t input_index,
                                                                         size_t adjust_input_size) {
  // Match all phi inputs from input_index onwards exactly.
  HInputsRef inputs = phi->GetInputs();
  DCHECK_LT(input_index, inputs.size());
  InductionInfo* a = LookupInfo(loop, inputs[input_index]);
  for (size_t i = input_index + 1, n = inputs.size() - adjust_input_size; i < n; i++) {
    InductionInfo* b = LookupInfo(loop, inputs[i]);
    if (!InductionEqual(a, b)) {
      return nullptr;
    }
  }
  return a;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::TransferAddSub(InductionInfo* a,
                                                                            InductionInfo* b,
                                                                            InductionOp op) {
  // Transfer over an addition or subtraction: any invariant, linear, polynomial, geometric,
  // wrap-around, or periodic can be combined with an invariant to yield a similar result.
  // Two linear or two polynomial inputs can be combined too. Other combinations fail.
  if (a != nullptr && b != nullptr) {
    if (IsNarrowingLinear(a) || IsNarrowingLinear(b)) {
      return nullptr;  // no transfer
    } else if (a->induction_class == kInvariant && b->induction_class == kInvariant) {
      return CreateInvariantOp(op, a, b);  // direct invariant
    } else if ((a->induction_class == kLinear && b->induction_class == kLinear) ||
               (a->induction_class == kPolynomial && b->induction_class == kPolynomial)) {
      // Rule induc(a, b) + induc(a', b') -> induc(a + a', b + b').
      InductionInfo* new_a = TransferAddSub(a->op_a, b->op_a, op);
      InductionInfo* new_b = TransferAddSub(a->op_b, b->op_b, op);
      if (new_a != nullptr && new_b != nullptr)  {
        return CreateInduction(a->induction_class, a->operation, new_a, new_b, a->fetch, type_);
      }
    } else if (a->induction_class == kInvariant) {
      // Rule a + induc(a', b') -> induc(a', a + b') or induc(a + a', a + b').
      InductionInfo* new_a = b->op_a;
      InductionInfo* new_b = TransferAddSub(a, b->op_b, op);
      if (b->induction_class == kWrapAround || b->induction_class == kPeriodic) {
        new_a = TransferAddSub(a, new_a, op);
      } else if (op == kSub) {  // Negation required.
        new_a = TransferNeg(new_a);
      }
      if (new_a != nullptr && new_b != nullptr)  {
        return CreateInduction(b->induction_class, b->operation, new_a, new_b, b->fetch, type_);
      }
    } else if (b->induction_class == kInvariant) {
      // Rule induc(a, b) + b' -> induc(a, b + b') or induc(a + b', b + b').
      InductionInfo* new_a = a->op_a;
      InductionInfo* new_b = TransferAddSub(a->op_b, b, op);
      if (a->induction_class == kWrapAround || a->induction_class == kPeriodic) {
        new_a = TransferAddSub(new_a, b, op);
      }
      if (new_a != nullptr && new_b != nullptr)  {
        return CreateInduction(a->induction_class, a->operation, new_a, new_b, a->fetch, type_);
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::TransferNeg(InductionInfo* a) {
  // Transfer over a unary negation: an invariant, linear, polynomial, geometric (mul),
  // wrap-around, or periodic input yields a similar but negated induction as result.
  if (a != nullptr) {
    if (IsNarrowingLinear(a)) {
      return nullptr;  // no transfer
    } else if (a->induction_class == kInvariant) {
      return CreateInvariantOp(kNeg, nullptr, a);  // direct invariant
    } else if (a->induction_class != kGeometric || a->operation == kMul) {
      // Rule - induc(a, b) -> induc(-a, -b).
      InductionInfo* new_a = TransferNeg(a->op_a);
      InductionInfo* new_b = TransferNeg(a->op_b);
      if (new_a != nullptr && new_b != nullptr) {
        return CreateInduction(a->induction_class, a->operation, new_a, new_b, a->fetch, type_);
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::TransferMul(InductionInfo* a,
                                                                         InductionInfo* b) {
  // Transfer over a multiplication: any invariant, linear, polynomial, geometric (mul),
  // wrap-around, or periodic can be multiplied with an invariant to yield a similar
  // but multiplied result. Two non-invariant inputs cannot be multiplied, however.
  if (a != nullptr && b != nullptr) {
    if (IsNarrowingLinear(a) || IsNarrowingLinear(b)) {
      return nullptr;  // no transfer
    } else if (a->induction_class == kInvariant && b->induction_class == kInvariant) {
      return CreateInvariantOp(kMul, a, b);  // direct invariant
    } else if (a->induction_class == kInvariant && (b->induction_class != kGeometric ||
                                                    b->operation == kMul)) {
      // Rule a * induc(a', b') -> induc(a * a', b * b').
      InductionInfo* new_a = TransferMul(a, b->op_a);
      InductionInfo* new_b = TransferMul(a, b->op_b);
      if (new_a != nullptr && new_b != nullptr) {
        return CreateInduction(b->induction_class, b->operation, new_a, new_b, b->fetch, type_);
      }
    } else if (b->induction_class == kInvariant && (a->induction_class != kGeometric ||
                                                    a->operation == kMul)) {
      // Rule induc(a, b) * b' -> induc(a * b', b * b').
      InductionInfo* new_a = TransferMul(a->op_a, b);
      InductionInfo* new_b = TransferMul(a->op_b, b);
      if (new_a != nullptr && new_b != nullptr) {
        return CreateInduction(a->induction_class, a->operation, new_a, new_b, a->fetch, type_);
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::TransferConversion(
    InductionInfo* a,
    DataType::Type from,
    DataType::Type to) {
  if (a != nullptr) {
    // Allow narrowing conversion on linear induction in certain cases:
    // induction is already at narrow type, or can be made narrower.
    if (IsNarrowingIntegralConversion(from, to) &&
        a->induction_class == kLinear &&
        (a->type == to || IsNarrowingIntegralConversion(a->type, to))) {
      return CreateInduction(kLinear, kNop, a->op_a, a->op_b, a->fetch, to);
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolvePhi(HInstruction* phi,
                                                                      size_t input_index,
                                                                      size_t adjust_input_size) {
  // Match all phi inputs from input_index onwards exactly.
  HInputsRef inputs = phi->GetInputs();
  DCHECK_LT(input_index, inputs.size());
  auto ita = cycle_.find(inputs[input_index]);
  if (ita != cycle_.end()) {
    for (size_t i = input_index + 1, n = inputs.size() - adjust_input_size; i < n; i++) {
      auto itb = cycle_.find(inputs[i]);
      if (itb == cycle_.end() ||
          !HInductionVarAnalysis::InductionEqual(ita->second, itb->second)) {
        return nullptr;
      }
    }
    return ita->second;
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolvePhiAllInputs(
    HLoopInformation* loop,
    HInstruction* entry_phi,
    HInstruction* phi) {
  // Match all phi inputs.
  InductionInfo* match = SolvePhi(phi, /*input_index*/ 0, /*adjust_input_size*/ 0);
  if (match != nullptr) {
    return match;
  }

  // Otherwise, try to solve for a periodic seeded from phi onward.
  // Only tight multi-statement cycles are considered in order to
  // simplify rotating the periodic during the final classification.
  if (phi->IsLoopHeaderPhi() && phi->InputCount() == 2) {
    InductionInfo* a = LookupInfo(loop, phi->InputAt(0));
    if (a != nullptr && a->induction_class == kInvariant) {
      if (phi->InputAt(1) == entry_phi) {
        InductionInfo* initial = LookupInfo(loop, entry_phi->InputAt(0));
        return CreateInduction(kPeriodic, kNop, a, initial, /*fetch*/ nullptr, type_);
      }
      InductionInfo* b = SolvePhi(phi, /*input_index*/ 1, /*adjust_input_size*/ 0);
      if (b != nullptr && b->induction_class == kPeriodic) {
        return CreateInduction(kPeriodic, kNop, a, b, /*fetch*/ nullptr, type_);
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolveAddSub(HLoopInformation* loop,
                                                                         HInstruction* entry_phi,
                                                                         HInstruction* instruction,
                                                                         HInstruction* x,
                                                                         HInstruction* y,
                                                                         InductionOp op,
                                                                         bool is_first_call) {
  // Solve within a cycle over an addition or subtraction.
  InductionInfo* b = LookupInfo(loop, y);
  if (b != nullptr) {
    if (b->induction_class == kInvariant) {
      // Adding or subtracting an invariant value, seeded from phi,
      // keeps adding to the stride of the linear induction.
      if (x == entry_phi) {
        return (op == kAdd) ? b : CreateInvariantOp(kNeg, nullptr, b);
      }
      auto it = cycle_.find(x);
      if (it != cycle_.end()) {
        InductionInfo* a = it->second;
        if (a->induction_class == kInvariant) {
          return CreateInvariantOp(op, a, b);
        }
      }
    } else if (b->induction_class == kLinear && b->type == type_) {
      // Solve within a tight cycle that adds a term that is already classified as a linear
      // induction for a polynomial induction k = k + i (represented as sum over linear terms).
      if (x == entry_phi && entry_phi->InputCount() == 2 && instruction == entry_phi->InputAt(1)) {
        InductionInfo* initial = LookupInfo(loop, entry_phi->InputAt(0));
        InductionInfo* new_a = op == kAdd ? b : TransferNeg(b);
        if (new_a != nullptr) {
          return CreateInduction(kPolynomial, kNop, new_a, initial, /*fetch*/ nullptr, type_);
        }
      }
    }
  }

  // Try some alternatives before failing.
  if (op == kAdd) {
    // Try the other way around for an addition if considered for first time.
    if (is_first_call) {
      return SolveAddSub(loop, entry_phi, instruction, y, x, op, false);
    }
  } else if (op == kSub) {
    // Solve within a tight cycle that is formed by exactly two instructions,
    // one phi and one update, for a periodic idiom of the form k = c - k.
    if (y == entry_phi && entry_phi->InputCount() == 2 && instruction == entry_phi->InputAt(1)) {
      InductionInfo* a = LookupInfo(loop, x);
      if (a != nullptr && a->induction_class == kInvariant) {
        InductionInfo* initial = LookupInfo(loop, entry_phi->InputAt(0));
        return CreateInduction(kPeriodic,
                               kNop,
                               CreateInvariantOp(kSub, a, initial),
                               initial,
                               /*fetch*/ nullptr,
                               type_);
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolveOp(HLoopInformation* loop,
                                                                      HInstruction* entry_phi,
                                                                      HInstruction* instruction,
                                                                      HInstruction* x,
                                                                      HInstruction* y,
                                                                      InductionOp op) {
  // Solve within a tight cycle for a binary operation k = k op c or, for some op, k = c op k.
  if (entry_phi->InputCount() == 2 && instruction == entry_phi->InputAt(1)) {
    InductionInfo* c = nullptr;
    InductionInfo* b = LookupInfo(loop, y);
    if (b != nullptr && b->induction_class == kInvariant && entry_phi == x) {
      c = b;
    } else if (op != kDiv && op != kRem) {
      InductionInfo* a = LookupInfo(loop, x);
      if (a != nullptr && a->induction_class == kInvariant && entry_phi == y) {
        c = a;
      }
    }
    // Found suitable operand left or right?
    if (c != nullptr) {
      InductionInfo* initial = LookupInfo(loop, entry_phi->InputAt(0));
      switch (op) {
        case kMul:
        case kDiv:
          // Restrict base of geometric induction to direct fetch.
          if (c->operation == kFetch) {
            return CreateInduction(kGeometric,
                                   op,
                                   initial,
                                   CreateConstant(0, type_),
                                   c->fetch,
                                   type_);
          }
          break;
        case kRem:
          // Idiomatic MOD wrap-around induction.
          return CreateInduction(kWrapAround,
                                 kNop,
                                 initial,
                                 CreateInvariantOp(kRem, initial, c),
                                 /*fetch*/ nullptr,
                                 type_);
        case kXor:
          // Idiomatic XOR periodic induction.
          return CreateInduction(kPeriodic,
                                 kNop,
                                 CreateInvariantOp(kXor, initial, c),
                                 initial,
                                 /*fetch*/ nullptr,
                                 type_);
        default:
          LOG(FATAL) << op;
          UNREACHABLE();
      }
    }
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolveTest(HLoopInformation* loop,
                                                                       HInstruction* entry_phi,
                                                                       HInstruction* instruction,
                                                                       int64_t opposite_value) {
  // Detect hidden XOR construction in x = (x == false) or x = (x != true).
  int64_t value = -1;
  HInstruction* x = instruction->InputAt(0);
  HInstruction* y = instruction->InputAt(1);
  if (IsExact(LookupInfo(loop, x), &value) && value == opposite_value) {
    return SolveOp(loop, entry_phi, instruction, graph_->GetIntConstant(1), y, kXor);
  } else if (IsExact(LookupInfo(loop, y), &value) && value == opposite_value) {
    return SolveOp(loop, entry_phi, instruction, x, graph_->GetIntConstant(1), kXor);
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::SolveConversion(
    HLoopInformation* loop,
    HInstruction* entry_phi,
    HTypeConversion* conversion) {
  DataType::Type from = conversion->GetInputType();
  DataType::Type to = conversion->GetResultType();
  // A narrowing conversion is allowed as *last* operation of the cycle of a linear induction
  // with an initial value that fits the type, provided that the narrowest encountered type is
  // recorded with the induction to account for the precision loss. The narrower induction does
  // *not* transfer to any wider operations, however, since these may yield out-of-type values
  if (entry_phi->InputCount() == 2 && conversion == entry_phi->InputAt(1)) {
    int64_t min = DataType::MinValueOfIntegralType(to);
    int64_t max = DataType::MaxValueOfIntegralType(to);
    int64_t value = 0;
    InductionInfo* initial = LookupInfo(loop, entry_phi->InputAt(0));
    if (IsNarrowingIntegralConversion(from, to) &&
        IsAtLeast(initial, &value) && value >= min &&
        IsAtMost(initial, &value)  && value <= max) {
      auto it = cycle_.find(conversion->GetInput());
      if (it != cycle_.end() && it->second->induction_class == kInvariant) {
        type_ = to;
        return it->second;
      }
    }
  }
  return nullptr;
}

//
// Loop trip count analysis methods.
//

void HInductionVarAnalysis::VisitControl(HLoopInformation* loop) {
  HInstruction* control = loop->GetHeader()->GetLastInstruction();
  if (control->IsIf()) {
    HIf* ifs = control->AsIf();
    HBasicBlock* if_true = ifs->IfTrueSuccessor();
    HBasicBlock* if_false = ifs->IfFalseSuccessor();
    HInstruction* if_expr = ifs->InputAt(0);
    // Determine if loop has following structure in header.
    // loop-header: ....
    //              if (condition) goto X
    if (if_expr->IsCondition()) {
      HCondition* condition = if_expr->AsCondition();
      InductionInfo* a = LookupInfo(loop, condition->InputAt(0));
      InductionInfo* b = LookupInfo(loop, condition->InputAt(1));
      DataType::Type type = ImplicitConversion(condition->InputAt(0)->GetType());
      // Determine if the loop control uses a known sequence on an if-exit (X outside) or on
      // an if-iterate (X inside), expressed as if-iterate when passed into VisitCondition().
      if (a == nullptr || b == nullptr) {
        return;  // Loop control is not a sequence.
      } else if (if_true->GetLoopInformation() != loop && if_false->GetLoopInformation() == loop) {
        VisitCondition(loop, if_false, a, b, type, condition->GetOppositeCondition());
      } else if (if_true->GetLoopInformation() == loop && if_false->GetLoopInformation() != loop) {
        VisitCondition(loop, if_true, a, b, type, condition->GetCondition());
      }
    }
  }
}

void HInductionVarAnalysis::VisitCondition(HLoopInformation* loop,
                                           HBasicBlock* body,
                                           InductionInfo* a,
                                           InductionInfo* b,
                                           DataType::Type type,
                                           IfCondition cmp) {
  if (a->induction_class == kInvariant && b->induction_class == kLinear) {
    // Swap condition if induction is at right-hand-side (e.g. U > i is same as i < U).
    switch (cmp) {
      case kCondLT: VisitCondition(loop, body, b, a, type, kCondGT); break;
      case kCondLE: VisitCondition(loop, body, b, a, type, kCondGE); break;
      case kCondGT: VisitCondition(loop, body, b, a, type, kCondLT); break;
      case kCondGE: VisitCondition(loop, body, b, a, type, kCondLE); break;
      case kCondNE: VisitCondition(loop, body, b, a, type, kCondNE); break;
      default: break;
    }
  } else if (a->induction_class == kLinear && b->induction_class == kInvariant) {
    // Analyze condition with induction at left-hand-side (e.g. i < U).
    InductionInfo* lower_expr = a->op_b;
    InductionInfo* upper_expr = b;
    InductionInfo* stride_expr = a->op_a;
    // Test for constant stride and integral condition.
    int64_t stride_value = 0;
    if (!IsExact(stride_expr, &stride_value)) {
      return;  // unknown stride
    } else if (type != DataType::Type::kInt32 && type != DataType::Type::kInt64) {
      return;  // not integral
    }
    // Since loops with a i != U condition will not be normalized by the method below, first
    // try to rewrite a break-loop with terminating condition i != U into an equivalent loop
    // with non-strict end condition i <= U or i >= U if such a rewriting is possible and safe.
    if (cmp == kCondNE && RewriteBreakLoop(loop, body, stride_value, type)) {
      cmp = stride_value > 0 ? kCondLE : kCondGE;
    }
    // If this rewriting failed, try to rewrite condition i != U into strict end condition i < U
    // or i > U if this end condition is reached exactly (tested by verifying if the loop has a
    // unit stride and the non-strict condition would be always taken).
    if (cmp == kCondNE && ((stride_value == +1 && IsTaken(lower_expr, upper_expr, kCondLE)) ||
                           (stride_value == -1 && IsTaken(lower_expr, upper_expr, kCondGE)))) {
      cmp = stride_value > 0 ? kCondLT : kCondGT;
    }
    // A mismatch between the type of condition and the induction is only allowed if the,
    // necessarily narrower, induction range fits the narrower control.
    if (type != a->type &&
        !FitsNarrowerControl(lower_expr, upper_expr, stride_value, a->type, cmp)) {
      return;  // mismatched type
    }
    // Normalize a linear loop control with a nonzero stride:
    //   stride > 0, either i < U or i <= U
    //   stride < 0, either i > U or i >= U
    if ((stride_value > 0 && (cmp == kCondLT || cmp == kCondLE)) ||
        (stride_value < 0 && (cmp == kCondGT || cmp == kCondGE))) {
      VisitTripCount(loop, lower_expr, upper_expr, stride_expr, stride_value, type, cmp);
    }
  }
}

void HInductionVarAnalysis::VisitTripCount(HLoopInformation* loop,
                                           InductionInfo* lower_expr,
                                           InductionInfo* upper_expr,
                                           InductionInfo* stride_expr,
                                           int64_t stride_value,
                                           DataType::Type type,
                                           IfCondition cmp) {
  // Any loop of the general form:
  //
  //    for (i = L; i <= U; i += S) // S > 0
  // or for (i = L; i >= U; i += S) // S < 0
  //      .. i ..
  //
  // can be normalized into:
  //
  //    for (n = 0; n < TC; n++) // where TC = (U + S - L) / S
  //      .. L + S * n ..
  //
  // taking the following into consideration:
  //
  // (1) Using the same precision, the TC (trip-count) expression should be interpreted as
  //     an unsigned entity, for example, as in the following loop that uses the full range:
  //     for (int i = INT_MIN; i < INT_MAX; i++) // TC = UINT_MAX
  // (2) The TC is only valid if the loop is taken, otherwise TC = 0, as in:
  //     for (int i = 12; i < U; i++) // TC = 0 when U <= 12
  //     If this cannot be determined at compile-time, the TC is only valid within the
  //     loop-body proper, not the loop-header unless enforced with an explicit taken-test.
  // (3) The TC is only valid if the loop is finite, otherwise TC has no value, as in:
  //     for (int i = 0; i <= U; i++) // TC = Inf when U = INT_MAX
  //     If this cannot be determined at compile-time, the TC is only valid when enforced
  //     with an explicit finite-test.
  // (4) For loops which early-exits, the TC forms an upper bound, as in:
  //     for (int i = 0; i < 10 && ....; i++) // TC <= 10
  InductionInfo* trip_count = upper_expr;
  const bool is_taken = IsTaken(lower_expr, upper_expr, cmp);
  const bool is_finite = IsFinite(upper_expr, stride_value, type, cmp);
  const bool cancels = (cmp == kCondLT || cmp == kCondGT) && std::abs(stride_value) == 1;
  if (!cancels) {
    // Convert exclusive integral inequality into inclusive integral inequality,
    // viz. condition i < U is i <= U - 1 and condition i > U is i >= U + 1.
    if (cmp == kCondLT) {
      trip_count = CreateInvariantOp(kSub, trip_count, CreateConstant(1, type));
    } else if (cmp == kCondGT) {
      trip_count = CreateInvariantOp(kAdd, trip_count, CreateConstant(1, type));
    }
    // Compensate for stride.
    trip_count = CreateInvariantOp(kAdd, trip_count, stride_expr);
  }
  trip_count = CreateInvariantOp(
      kDiv, CreateInvariantOp(kSub, trip_count, lower_expr), stride_expr);
  // Assign the trip-count expression to the loop control. Clients that use the information
  // should be aware that the expression is only valid under the conditions listed above.
  InductionOp tcKind = kTripCountInBodyUnsafe;  // needs both tests
  if (is_taken && is_finite) {
    tcKind = kTripCountInLoop;  // needs neither test
  } else if (is_finite) {
    tcKind = kTripCountInBody;  // needs taken-test
  } else if (is_taken) {
    tcKind = kTripCountInLoopUnsafe;  // needs finite-test
  }
  InductionOp op = kNop;
  switch (cmp) {
    case kCondLT: op = kLT; break;
    case kCondLE: op = kLE; break;
    case kCondGT: op = kGT; break;
    case kCondGE: op = kGE; break;
    default:      LOG(FATAL) << "CONDITION UNREACHABLE";
  }
  // Associate trip count with control instruction, rather than the condition (even
  // though it's its use) since former provides a convenient use-free placeholder.
  HInstruction* control = loop->GetHeader()->GetLastInstruction();
  InductionInfo* taken_test = CreateInvariantOp(op, lower_expr, upper_expr);
  DCHECK(control->IsIf());
  AssignInfo(loop, control, CreateTripCount(tcKind, trip_count, taken_test, type));
}

bool HInductionVarAnalysis::IsTaken(InductionInfo* lower_expr,
                                    InductionInfo* upper_expr,
                                    IfCondition cmp) {
  int64_t lower_value;
  int64_t upper_value;
  switch (cmp) {
    case kCondLT:
      return IsAtMost(lower_expr, &lower_value)
          && IsAtLeast(upper_expr, &upper_value)
          && lower_value < upper_value;
    case kCondLE:
      return IsAtMost(lower_expr, &lower_value)
          && IsAtLeast(upper_expr, &upper_value)
          && lower_value <= upper_value;
    case kCondGT:
      return IsAtLeast(lower_expr, &lower_value)
          && IsAtMost(upper_expr, &upper_value)
          && lower_value > upper_value;
    case kCondGE:
      return IsAtLeast(lower_expr, &lower_value)
          && IsAtMost(upper_expr, &upper_value)
          && lower_value >= upper_value;
    default:
      LOG(FATAL) << "CONDITION UNREACHABLE";
  }
  return false;  // not certain, may be untaken
}

bool HInductionVarAnalysis::IsFinite(InductionInfo* upper_expr,
                                     int64_t stride_value,
                                     DataType::Type type,
                                     IfCondition cmp) {
  int64_t min = DataType::MinValueOfIntegralType(type);
  int64_t max = DataType::MaxValueOfIntegralType(type);
  // Some rules under which it is certain at compile-time that the loop is finite.
  int64_t value;
  switch (cmp) {
    case kCondLT:
      return stride_value == 1 ||
          (IsAtMost(upper_expr, &value) && value <= (max - stride_value + 1));
    case kCondLE:
      return (IsAtMost(upper_expr, &value) && value <= (max - stride_value));
    case kCondGT:
      return stride_value == -1 ||
          (IsAtLeast(upper_expr, &value) && value >= (min - stride_value - 1));
    case kCondGE:
      return (IsAtLeast(upper_expr, &value) && value >= (min - stride_value));
    default:
      LOG(FATAL) << "CONDITION UNREACHABLE";
  }
  return false;  // not certain, may be infinite
}

bool HInductionVarAnalysis::FitsNarrowerControl(InductionInfo* lower_expr,
                                                InductionInfo* upper_expr,
                                                int64_t stride_value,
                                                DataType::Type type,
                                                IfCondition cmp) {
  int64_t min = DataType::MinValueOfIntegralType(type);
  int64_t max = DataType::MaxValueOfIntegralType(type);
  // Inclusive test need one extra.
  if (stride_value != 1 && stride_value != -1) {
    return false;  // non-unit stride
  } else if (cmp == kCondLE) {
    max--;
  } else if (cmp == kCondGE) {
    min++;
  }
  // Do both bounds fit the range?
  int64_t value = 0;
  return IsAtLeast(lower_expr, &value) && value >= min &&
         IsAtMost(lower_expr, &value)  && value <= max &&
         IsAtLeast(upper_expr, &value) && value >= min &&
         IsAtMost(upper_expr, &value)  && value <= max;
}

bool HInductionVarAnalysis::RewriteBreakLoop(HLoopInformation* loop,
                                             HBasicBlock* body,
                                             int64_t stride_value,
                                             DataType::Type type) {
  // Only accept unit stride.
  if (std::abs(stride_value) != 1) {
    return false;
  }
  // Simple terminating i != U condition, used nowhere else.
  HIf* ifs = loop->GetHeader()->GetLastInstruction()->AsIf();
  HInstruction* cond = ifs->InputAt(0);
  if (ifs->GetPrevious() != cond || !cond->HasOnlyOneNonEnvironmentUse()) {
    return false;
  }
  int c = LookupInfo(loop, cond->InputAt(0))->induction_class == kLinear ? 0 : 1;
  HInstruction* index = cond->InputAt(c);
  HInstruction* upper = cond->InputAt(1 - c);
  // Safe to rewrite into i <= U?
  IfCondition cmp = stride_value > 0 ? kCondLE : kCondGE;
  if (!index->IsPhi() || !IsFinite(LookupInfo(loop, upper), stride_value, type, cmp)) {
    return false;
  }
  // Body consists of update to index i only, used nowhere else.
  if (body->GetSuccessors().size() != 1 ||
      body->GetSingleSuccessor() != loop->GetHeader() ||
      !body->GetPhis().IsEmpty() ||
      body->GetInstructions().IsEmpty() ||
      body->GetFirstInstruction() != index->InputAt(1) ||
      !body->GetFirstInstruction()->HasOnlyOneNonEnvironmentUse() ||
      !body->GetFirstInstruction()->GetNext()->IsGoto()) {
    return false;
  }
  // Always taken or guarded by enclosing condition.
  if (!IsTaken(LookupInfo(loop, index)->op_b, LookupInfo(loop, upper), cmp) &&
      !IsGuardedBy(loop, cmp, index->InputAt(0), upper)) {
    return false;
  }
  // Test if break-loop body can be written, and do so on success.
  if (RewriteBreakLoopBody(loop, body, cond, index, upper, /*rewrite*/ false)) {
    RewriteBreakLoopBody(loop, body, cond, index, upper, /*rewrite*/ true);
  } else {
    return false;
  }
  // Rewrite condition in HIR.
  if (ifs->IfTrueSuccessor() != body) {
    cmp = (cmp == kCondLE) ? kCondGT : kCondLT;
  }
  HInstruction* rep = nullptr;
  switch (cmp) {
    case kCondLT: rep = new (graph_->GetAllocator()) HLessThan(index, upper); break;
    case kCondGT: rep = new (graph_->GetAllocator()) HGreaterThan(index, upper); break;
    case kCondLE: rep = new (graph_->GetAllocator()) HLessThanOrEqual(index, upper); break;
    case kCondGE: rep = new (graph_->GetAllocator()) HGreaterThanOrEqual(index, upper); break;
    default: LOG(FATAL) << cmp; UNREACHABLE();
  }
  loop->GetHeader()->ReplaceAndRemoveInstructionWith(cond, rep);
  return true;
}

//
// Helper methods.
//

void HInductionVarAnalysis::AssignInfo(HLoopInformation* loop,
                                       HInstruction* instruction,
                                       InductionInfo* info) {
  auto it = induction_.find(loop);
  if (it == induction_.end()) {
    it = induction_.Put(loop,
                        ArenaSafeMap<HInstruction*, InductionInfo*>(
                            std::less<HInstruction*>(),
                            graph_->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)));
  }
  it->second.Put(instruction, info);
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::LookupInfo(HLoopInformation* loop,
                                                                        HInstruction* instruction) {
  auto it = induction_.find(loop);
  if (it != induction_.end()) {
    auto loop_it = it->second.find(instruction);
    if (loop_it != it->second.end()) {
      return loop_it->second;
    }
  }
  if (loop->IsDefinedOutOfTheLoop(instruction)) {
    InductionInfo* info = CreateInvariantFetch(instruction);
    AssignInfo(loop, instruction, info);
    return info;
  }
  return nullptr;
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::CreateConstant(int64_t value,
                                                                            DataType::Type type) {
  HInstruction* constant;
  switch (type) {
    case DataType::Type::kFloat64: constant = graph_->GetDoubleConstant(value); break;
    case DataType::Type::kFloat32: constant = graph_->GetFloatConstant(value);  break;
    case DataType::Type::kInt64:   constant = graph_->GetLongConstant(value);   break;
    default:                       constant = graph_->GetIntConstant(value);    break;
  }
  return CreateInvariantFetch(constant);
}

HInductionVarAnalysis::InductionInfo* HInductionVarAnalysis::CreateSimplifiedInvariant(
    InductionOp op,
    InductionInfo* a,
    InductionInfo* b) {
  // Perform some light-weight simplifications during construction of a new invariant.
  // This often safes memory and yields a more concise representation of the induction.
  // More exhaustive simplifications are done by later phases once induction nodes are
  // translated back into HIR code (e.g. by loop optimizations or BCE).
  int64_t value = -1;
  if (IsExact(a, &value)) {
    if (value == 0) {
      // Simplify 0 + b = b, 0 ^ b = b, 0 * b = 0.
      if (op == kAdd || op == kXor) {
        return b;
      } else if (op == kMul) {
        return a;
      }
    } else if (op == kMul) {
      // Simplify 1 * b = b, -1 * b = -b
      if (value == 1) {
        return b;
      } else if (value == -1) {
        return CreateSimplifiedInvariant(kNeg, nullptr, b);
      }
    }
  }
  if (IsExact(b, &value)) {
    if (value == 0) {
      // Simplify a + 0 = a, a - 0 = a, a ^ 0 = a, a * 0 = 0, -0 = 0.
      if (op == kAdd || op == kSub || op == kXor) {
        return a;
      } else if (op == kMul || op == kNeg) {
        return b;
      }
    } else if (op == kMul || op == kDiv) {
      // Simplify a * 1 = a, a / 1 = a, a * -1 = -a, a / -1 = -a
      if (value == 1) {
        return a;
      } else if (value == -1) {
        return CreateSimplifiedInvariant(kNeg, nullptr, a);
      }
    }
  } else if (b->operation == kNeg) {
    // Simplify a + (-b) = a - b, a - (-b) = a + b, -(-b) = b.
    if (op == kAdd) {
      return CreateSimplifiedInvariant(kSub, a, b->op_b);
    } else if (op == kSub) {
      return CreateSimplifiedInvariant(kAdd, a, b->op_b);
    } else if (op == kNeg) {
      return b->op_b;
    }
  } else if (b->operation == kSub) {
    // Simplify - (a - b) = b - a.
    if (op == kNeg) {
      return CreateSimplifiedInvariant(kSub, b->op_b, b->op_a);
    }
  }
  return new (graph_->GetAllocator()) InductionInfo(
      kInvariant, op, a, b, nullptr, ImplicitConversion(b->type));
}

HInstruction* HInductionVarAnalysis::GetShiftConstant(HLoopInformation* loop,
                                                      HInstruction* instruction,
                                                      InductionInfo* initial) {
  DCHECK(instruction->IsShl() || instruction->IsShr() || instruction->IsUShr());
  // Shift-rights are only the same as division for non-negative initial inputs.
  // Otherwise we would round incorrectly.
  if (initial != nullptr) {
    int64_t value = -1;
    if (!IsAtLeast(initial, &value) || value < 0) {
      return nullptr;
    }
  }
  // Obtain the constant needed to treat shift as equivalent multiplication or division.
  // This yields an existing instruction if the constant is already there. Otherwise, this
  // has a side effect on the HIR. The restriction on the shift factor avoids generating a
  // negative constant (viz. 1 << 31 and 1L << 63 set the sign bit). The code assumes that
  // generalization for shift factors outside [0,32) and [0,64) ranges is done earlier.
  InductionInfo* b = LookupInfo(loop, instruction->InputAt(1));
  int64_t value = -1;
  if (IsExact(b, &value)) {
    DataType::Type type = instruction->InputAt(0)->GetType();
    if (type == DataType::Type::kInt32 && 0 <= value && value < 31) {
      return graph_->GetIntConstant(1 << value);
    }
    if (type == DataType::Type::kInt64 && 0 <= value && value < 63) {
      return graph_->GetLongConstant(1L << value);
    }
  }
  return nullptr;
}

void HInductionVarAnalysis::AssignCycle(HPhi* phi) {
  ArenaSet<HInstruction*>* set = &cycles_.Put(phi, ArenaSet<HInstruction*>(
      graph_->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis)))->second;
  for (HInstruction* i : scc_) {
    set->insert(i);
  }
}

ArenaSet<HInstruction*>* HInductionVarAnalysis::LookupCycle(HPhi* phi) {
  auto it = cycles_.find(phi);
  if (it != cycles_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool HInductionVarAnalysis::IsExact(InductionInfo* info, int64_t* value) {
  return InductionVarRange(this).IsConstant(info, InductionVarRange::kExact, value);
}

bool HInductionVarAnalysis::IsAtMost(InductionInfo* info, int64_t* value) {
  return InductionVarRange(this).IsConstant(info, InductionVarRange::kAtMost, value);
}

bool HInductionVarAnalysis::IsAtLeast(InductionInfo* info, int64_t* value) {
  return InductionVarRange(this).IsConstant(info, InductionVarRange::kAtLeast, value);
}

bool HInductionVarAnalysis::IsNarrowingLinear(InductionInfo* info) {
  return info != nullptr &&
      info->induction_class == kLinear &&
      (info->type == DataType::Type::kUint8 ||
       info->type == DataType::Type::kInt8 ||
       info->type == DataType::Type::kUint16 ||
       info->type == DataType::Type::kInt16 ||
       (info->type == DataType::Type::kInt32 && (info->op_a->type == DataType::Type::kInt64 ||
                                                 info->op_b->type == DataType::Type::kInt64)));
}

bool HInductionVarAnalysis::InductionEqual(InductionInfo* info1,
                                           InductionInfo* info2) {
  // Test structural equality only, without accounting for simplifications.
  if (info1 != nullptr && info2 != nullptr) {
    return
        info1->induction_class == info2->induction_class &&
        info1->operation       == info2->operation       &&
        info1->fetch           == info2->fetch           &&
        info1->type            == info2->type            &&
        InductionEqual(info1->op_a, info2->op_a)         &&
        InductionEqual(info1->op_b, info2->op_b);
  }
  // Otherwise only two nullptrs are considered equal.
  return info1 == info2;
}

std::string HInductionVarAnalysis::FetchToString(HInstruction* fetch) {
  DCHECK(fetch != nullptr);
  if (fetch->IsIntConstant()) {
    return std::to_string(fetch->AsIntConstant()->GetValue());
  } else if (fetch->IsLongConstant()) {
    return std::to_string(fetch->AsLongConstant()->GetValue());
  }
  return std::to_string(fetch->GetId()) + ":" + fetch->DebugName();
}

std::string HInductionVarAnalysis::InductionToString(InductionInfo* info) {
  if (info != nullptr) {
    if (info->induction_class == kInvariant) {
      std::string inv = "(";
      inv += InductionToString(info->op_a);
      switch (info->operation) {
        case kNop:   inv += " @ ";  break;
        case kAdd:   inv += " + ";  break;
        case kSub:
        case kNeg:   inv += " - ";  break;
        case kMul:   inv += " * ";  break;
        case kDiv:   inv += " / ";  break;
        case kRem:   inv += " % ";  break;
        case kXor:   inv += " ^ ";  break;
        case kLT:    inv += " < ";  break;
        case kLE:    inv += " <= "; break;
        case kGT:    inv += " > ";  break;
        case kGE:    inv += " >= "; break;
        case kFetch: inv += FetchToString(info->fetch); break;
        case kTripCountInLoop:       inv += " (TC-loop) ";        break;
        case kTripCountInBody:       inv += " (TC-body) ";        break;
        case kTripCountInLoopUnsafe: inv += " (TC-loop-unsafe) "; break;
        case kTripCountInBodyUnsafe: inv += " (TC-body-unsafe) "; break;
      }
      inv += InductionToString(info->op_b);
      inv += ")";
      return inv;
    } else {
      if (info->induction_class == kLinear) {
        DCHECK(info->operation == kNop);
        return "(" + InductionToString(info->op_a) + " * i + " +
                     InductionToString(info->op_b) + "):" +
                     DataType::PrettyDescriptor(info->type);
      } else if (info->induction_class == kPolynomial) {
        DCHECK(info->operation == kNop);
        return "poly(sum_lt(" + InductionToString(info->op_a) + ") + " +
                                InductionToString(info->op_b) + "):" +
                                DataType::PrettyDescriptor(info->type);
      } else if (info->induction_class == kGeometric) {
        DCHECK(info->operation == kMul || info->operation == kDiv);
        DCHECK(info->fetch != nullptr);
        return "geo(" + InductionToString(info->op_a) + " * " +
                        FetchToString(info->fetch) +
                        (info->operation == kMul ? " ^ i + " : " ^ -i + ") +
                        InductionToString(info->op_b) + "):" +
                        DataType::PrettyDescriptor(info->type);
      } else if (info->induction_class == kWrapAround) {
        DCHECK(info->operation == kNop);
        return "wrap(" + InductionToString(info->op_a) + ", " +
                         InductionToString(info->op_b) + "):" +
                         DataType::PrettyDescriptor(info->type);
      } else if (info->induction_class == kPeriodic) {
        DCHECK(info->operation == kNop);
        return "periodic(" + InductionToString(info->op_a) + ", " +
                             InductionToString(info->op_b) + "):" +
                             DataType::PrettyDescriptor(info->type);
      }
    }
  }
  return "";
}

}  // namespace art
