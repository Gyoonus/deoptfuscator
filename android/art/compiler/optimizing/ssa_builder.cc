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

#include "ssa_builder.h"

#include "data_type-inl.h"
#include "dex/bytecode_utils.h"
#include "mirror/class-inl.h"
#include "nodes.h"
#include "reference_type_propagation.h"
#include "scoped_thread_state_change-inl.h"
#include "ssa_phi_elimination.h"

namespace art {

void SsaBuilder::FixNullConstantType() {
  // The order doesn't matter here.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* equality_instr = it.Current();
      if (!equality_instr->IsEqual() && !equality_instr->IsNotEqual()) {
        continue;
      }
      HInstruction* left = equality_instr->InputAt(0);
      HInstruction* right = equality_instr->InputAt(1);
      HInstruction* int_operand = nullptr;

      if ((left->GetType() == DataType::Type::kReference) &&
          (right->GetType() == DataType::Type::kInt32)) {
        int_operand = right;
      } else if ((right->GetType() == DataType::Type::kReference) &&
                 (left->GetType() == DataType::Type::kInt32)) {
        int_operand = left;
      } else {
        continue;
      }

      // If we got here, we are comparing against a reference and the int constant
      // should be replaced with a null constant.
      // Both type propagation and redundant phi elimination ensure `int_operand`
      // can only be the 0 constant.
      DCHECK(int_operand->IsIntConstant()) << int_operand->DebugName();
      DCHECK_EQ(0, int_operand->AsIntConstant()->GetValue());
      equality_instr->ReplaceInput(graph_->GetNullConstant(), int_operand == right ? 1 : 0);
    }
  }
}

void SsaBuilder::EquivalentPhisCleanup() {
  // The order doesn't matter here.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HPhi* phi = it.Current()->AsPhi();
      HPhi* next = phi->GetNextEquivalentPhiWithSameType();
      if (next != nullptr) {
        // Make sure we do not replace a live phi with a dead phi. A live phi
        // has been handled by the type propagation phase, unlike a dead phi.
        if (next->IsLive()) {
          phi->ReplaceWith(next);
          phi->SetDead();
        } else {
          next->ReplaceWith(phi);
        }
        DCHECK(next->GetNextEquivalentPhiWithSameType() == nullptr)
            << "More then one phi equivalent with type " << phi->GetType()
            << " found for phi" << phi->GetId();
      }
    }
  }
}

void SsaBuilder::FixEnvironmentPhis() {
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    for (HInstructionIterator it_phis(block->GetPhis()); !it_phis.Done(); it_phis.Advance()) {
      HPhi* phi = it_phis.Current()->AsPhi();
      // If the phi is not dead, or has no environment uses, there is nothing to do.
      if (!phi->IsDead() || !phi->HasEnvironmentUses()) continue;
      HInstruction* next = phi->GetNext();
      if (!phi->IsVRegEquivalentOf(next)) continue;
      if (next->AsPhi()->IsDead()) {
        // If the phi equivalent is dead, check if there is another one.
        next = next->GetNext();
        if (!phi->IsVRegEquivalentOf(next)) continue;
        // There can be at most two phi equivalents.
        DCHECK(!phi->IsVRegEquivalentOf(next->GetNext()));
        if (next->AsPhi()->IsDead()) continue;
      }
      // We found a live phi equivalent. Update the environment uses of `phi` with it.
      phi->ReplaceWith(next);
    }
  }
}

static void AddDependentInstructionsToWorklist(HInstruction* instruction,
                                               ScopedArenaVector<HPhi*>* worklist) {
  // If `instruction` is a dead phi, type conflict was just identified. All its
  // live phi users, and transitively users of those users, therefore need to be
  // marked dead/conflicting too, so we add them to the worklist. Otherwise we
  // add users whose type does not match and needs to be updated.
  bool add_all_live_phis = instruction->IsPhi() && instruction->AsPhi()->IsDead();
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    HInstruction* user = use.GetUser();
    if (user->IsPhi() && user->AsPhi()->IsLive()) {
      if (add_all_live_phis || user->GetType() != instruction->GetType()) {
        worklist->push_back(user->AsPhi());
      }
    }
  }
}

// Find a candidate primitive type for `phi` by merging the type of its inputs.
// Return false if conflict is identified.
static bool TypePhiFromInputs(HPhi* phi) {
  DataType::Type common_type = phi->GetType();

  for (HInstruction* input : phi->GetInputs()) {
    if (input->IsPhi() && input->AsPhi()->IsDead()) {
      // Phis are constructed live so if an input is a dead phi, it must have
      // been made dead due to type conflict. Mark this phi conflicting too.
      return false;
    }

    DataType::Type input_type = HPhi::ToPhiType(input->GetType());
    if (common_type == input_type) {
      // No change in type.
    } else if (DataType::Is64BitType(common_type) != DataType::Is64BitType(input_type)) {
      // Types are of different sizes, e.g. int vs. long. Must be a conflict.
      return false;
    } else if (DataType::IsIntegralType(common_type)) {
      // Previous inputs were integral, this one is not but is of the same size.
      // This does not imply conflict since some bytecode instruction types are
      // ambiguous. TypeInputsOfPhi will either type them or detect a conflict.
      DCHECK(DataType::IsFloatingPointType(input_type) ||
             input_type == DataType::Type::kReference);
      common_type = input_type;
    } else if (DataType::IsIntegralType(input_type)) {
      // Input is integral, common type is not. Same as in the previous case, if
      // there is a conflict, it will be detected during TypeInputsOfPhi.
      DCHECK(DataType::IsFloatingPointType(common_type) ||
             common_type == DataType::Type::kReference);
    } else {
      // Combining float and reference types. Clearly a conflict.
      DCHECK(
          (common_type == DataType::Type::kFloat32 && input_type == DataType::Type::kReference) ||
          (common_type == DataType::Type::kReference && input_type == DataType::Type::kFloat32));
      return false;
    }
  }

  // We have found a candidate type for the phi. Set it and return true. We may
  // still discover conflict whilst typing the individual inputs in TypeInputsOfPhi.
  phi->SetType(common_type);
  return true;
}

// Replace inputs of `phi` to match its type. Return false if conflict is identified.
bool SsaBuilder::TypeInputsOfPhi(HPhi* phi, ScopedArenaVector<HPhi*>* worklist) {
  DataType::Type common_type = phi->GetType();
  if (DataType::IsIntegralType(common_type)) {
    // We do not need to retype ambiguous inputs because they are always constructed
    // with the integral type candidate.
    if (kIsDebugBuild) {
      for (HInstruction* input : phi->GetInputs()) {
        DCHECK(HPhi::ToPhiType(input->GetType()) == common_type);
      }
    }
    // Inputs did not need to be replaced, hence no conflict. Report success.
    return true;
  } else {
    DCHECK(common_type == DataType::Type::kReference ||
           DataType::IsFloatingPointType(common_type));
    HInputsRef inputs = phi->GetInputs();
    for (size_t i = 0; i < inputs.size(); ++i) {
      HInstruction* input = inputs[i];
      if (input->GetType() != common_type) {
        // Input type does not match phi's type. Try to retype the input or
        // generate a suitably typed equivalent.
        HInstruction* equivalent = (common_type == DataType::Type::kReference)
            ? GetReferenceTypeEquivalent(input)
            : GetFloatOrDoubleEquivalent(input, common_type);
        if (equivalent == nullptr) {
          // Input could not be typed. Report conflict.
          return false;
        }
        // Make sure the input did not change its type and we do not need to
        // update its users.
        DCHECK_NE(input, equivalent);

        phi->ReplaceInput(equivalent, i);
        if (equivalent->IsPhi()) {
          worklist->push_back(equivalent->AsPhi());
        }
      }
    }
    // All inputs either matched the type of the phi or we successfully replaced
    // them with a suitable equivalent. Report success.
    return true;
  }
}

// Attempt to set the primitive type of `phi` to match its inputs. Return whether
// it was changed by the algorithm or not.
bool SsaBuilder::UpdatePrimitiveType(HPhi* phi, ScopedArenaVector<HPhi*>* worklist) {
  DCHECK(phi->IsLive());
  DataType::Type original_type = phi->GetType();

  // Try to type the phi in two stages:
  // (1) find a candidate type for the phi by merging types of all its inputs,
  // (2) try to type the phi's inputs to that candidate type.
  // Either of these stages may detect a type conflict and fail, in which case
  // we immediately abort.
  if (!TypePhiFromInputs(phi) || !TypeInputsOfPhi(phi, worklist)) {
    // Conflict detected. Mark the phi dead and return true because it changed.
    phi->SetDead();
    return true;
  }

  // Return true if the type of the phi has changed.
  return phi->GetType() != original_type;
}

void SsaBuilder::RunPrimitiveTypePropagation() {
  ScopedArenaVector<HPhi*> worklist(local_allocator_->Adapter(kArenaAllocGraphBuilder));

  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    if (block->IsLoopHeader()) {
      for (HInstructionIterator phi_it(block->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
        HPhi* phi = phi_it.Current()->AsPhi();
        if (phi->IsLive()) {
          worklist.push_back(phi);
        }
      }
    } else {
      for (HInstructionIterator phi_it(block->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
        // Eagerly compute the type of the phi, for quicker convergence. Note
        // that we don't need to add users to the worklist because we are
        // doing a reverse post-order visit, therefore either the phi users are
        // non-loop phi and will be visited later in the visit, or are loop-phis,
        // and they are already in the work list.
        HPhi* phi = phi_it.Current()->AsPhi();
        if (phi->IsLive()) {
          UpdatePrimitiveType(phi, &worklist);
        }
      }
    }
  }

  ProcessPrimitiveTypePropagationWorklist(&worklist);
  EquivalentPhisCleanup();
}

void SsaBuilder::ProcessPrimitiveTypePropagationWorklist(ScopedArenaVector<HPhi*>* worklist) {
  // Process worklist
  while (!worklist->empty()) {
    HPhi* phi = worklist->back();
    worklist->pop_back();
    // The phi could have been made dead as a result of conflicts while in the
    // worklist. If it is now dead, there is no point in updating its type.
    if (phi->IsLive() && UpdatePrimitiveType(phi, worklist)) {
      AddDependentInstructionsToWorklist(phi, worklist);
    }
  }
}

static HArrayGet* FindFloatOrDoubleEquivalentOfArrayGet(HArrayGet* aget) {
  DataType::Type type = aget->GetType();
  DCHECK(DataType::IsIntOrLongType(type));
  HInstruction* next = aget->GetNext();
  if (next != nullptr && next->IsArrayGet()) {
    HArrayGet* next_aget = next->AsArrayGet();
    if (next_aget->IsEquivalentOf(aget)) {
      return next_aget;
    }
  }
  return nullptr;
}

static HArrayGet* CreateFloatOrDoubleEquivalentOfArrayGet(HArrayGet* aget) {
  DataType::Type type = aget->GetType();
  DCHECK(DataType::IsIntOrLongType(type));
  DCHECK(FindFloatOrDoubleEquivalentOfArrayGet(aget) == nullptr);

  HArrayGet* equivalent = new (aget->GetBlock()->GetGraph()->GetAllocator()) HArrayGet(
      aget->GetArray(),
      aget->GetIndex(),
      type == DataType::Type::kInt32 ? DataType::Type::kFloat32 : DataType::Type::kFloat64,
      aget->GetDexPc());
  aget->GetBlock()->InsertInstructionAfter(equivalent, aget);
  return equivalent;
}

static DataType::Type GetPrimitiveArrayComponentType(HInstruction* array)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ReferenceTypeInfo array_type = array->GetReferenceTypeInfo();
  DCHECK(array_type.IsPrimitiveArrayClass());
  return DataTypeFromPrimitive(
      array_type.GetTypeHandle()->GetComponentType()->GetPrimitiveType());
}

bool SsaBuilder::FixAmbiguousArrayOps() {
  if (ambiguous_agets_.empty() && ambiguous_asets_.empty()) {
    return true;
  }

  // The wrong ArrayGet equivalent may still have Phi uses coming from ArraySet
  // uses (because they are untyped) and environment uses (if --debuggable).
  // After resolving all ambiguous ArrayGets, we will re-run primitive type
  // propagation on the Phis which need to be updated.
  ScopedArenaVector<HPhi*> worklist(local_allocator_->Adapter(kArenaAllocGraphBuilder));

  {
    ScopedObjectAccess soa(Thread::Current());

    for (HArrayGet* aget_int : ambiguous_agets_) {
      HInstruction* array = aget_int->GetArray();
      if (!array->GetReferenceTypeInfo().IsPrimitiveArrayClass()) {
        // RTP did not type the input array. Bail.
        VLOG(compiler) << "Not compiled: Could not infer an array type for array operation at "
                       << aget_int->GetDexPc();
        return false;
      }

      HArrayGet* aget_float = FindFloatOrDoubleEquivalentOfArrayGet(aget_int);
      DataType::Type array_type = GetPrimitiveArrayComponentType(array);
      DCHECK_EQ(DataType::Is64BitType(aget_int->GetType()), DataType::Is64BitType(array_type));

      if (DataType::IsIntOrLongType(array_type)) {
        if (aget_float != nullptr) {
          // There is a float/double equivalent. We must replace it and re-run
          // primitive type propagation on all dependent instructions.
          aget_float->ReplaceWith(aget_int);
          aget_float->GetBlock()->RemoveInstruction(aget_float);
          AddDependentInstructionsToWorklist(aget_int, &worklist);
        }
      } else {
        DCHECK(DataType::IsFloatingPointType(array_type));
        if (aget_float == nullptr) {
          // This is a float/double ArrayGet but there were no typed uses which
          // would create the typed equivalent. Create it now.
          aget_float = CreateFloatOrDoubleEquivalentOfArrayGet(aget_int);
        }
        // Replace the original int/long instruction. Note that it may have phi
        // uses, environment uses, as well as real uses (from untyped ArraySets).
        // We need to re-run primitive type propagation on its dependent instructions.
        aget_int->ReplaceWith(aget_float);
        aget_int->GetBlock()->RemoveInstruction(aget_int);
        AddDependentInstructionsToWorklist(aget_float, &worklist);
      }
    }

    // Set a flag stating that types of ArrayGets have been resolved. Requesting
    // equivalent of the wrong type with GetFloatOrDoubleEquivalentOfArrayGet
    // will fail from now on.
    agets_fixed_ = true;

    for (HArraySet* aset : ambiguous_asets_) {
      HInstruction* array = aset->GetArray();
      if (!array->GetReferenceTypeInfo().IsPrimitiveArrayClass()) {
        // RTP did not type the input array. Bail.
        VLOG(compiler) << "Not compiled: Could not infer an array type for array operation at "
                       << aset->GetDexPc();
        return false;
      }

      HInstruction* value = aset->GetValue();
      DataType::Type value_type = value->GetType();
      DataType::Type array_type = GetPrimitiveArrayComponentType(array);
      DCHECK_EQ(DataType::Is64BitType(value_type), DataType::Is64BitType(array_type));

      if (DataType::IsFloatingPointType(array_type)) {
        if (!DataType::IsFloatingPointType(value_type)) {
          DCHECK(DataType::IsIntegralType(value_type));
          // Array elements are floating-point but the value has not been replaced
          // with its floating-point equivalent. The replacement must always
          // succeed in code validated by the verifier.
          HInstruction* equivalent = GetFloatOrDoubleEquivalent(value, array_type);
          DCHECK(equivalent != nullptr);
          aset->ReplaceInput(equivalent, /* input_index */ 2);
          if (equivalent->IsPhi()) {
            // Returned equivalent is a phi which may not have had its inputs
            // replaced yet. We need to run primitive type propagation on it.
            worklist.push_back(equivalent->AsPhi());
          }
        }
        // Refine the side effects of this floating point aset. Note that we do this even if
        // no replacement occurs, since the right-hand-side may have been corrected already.
        aset->SetSideEffects(HArraySet::ComputeSideEffects(aset->GetComponentType()));
      } else {
        // Array elements are integral and the value assigned to it initially
        // was integral too. Nothing to do.
        DCHECK(DataType::IsIntegralType(array_type));
        DCHECK(DataType::IsIntegralType(value_type));
      }
    }
  }

  if (!worklist.empty()) {
    ProcessPrimitiveTypePropagationWorklist(&worklist);
    EquivalentPhisCleanup();
  }

  return true;
}

static bool HasAliasInEnvironments(HInstruction* instruction) {
  HEnvironment* last_user = nullptr;
  for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
    DCHECK(use.GetUser() != nullptr);
    // Note: The first comparison (== null) always fails.
    if (use.GetUser() == last_user) {
      return true;
    }
    last_user = use.GetUser();
  }

  if (kIsDebugBuild) {
    // Do a quadratic search to ensure same environment uses are next
    // to each other.
    const HUseList<HEnvironment*>& env_uses = instruction->GetEnvUses();
    for (auto current = env_uses.begin(), end = env_uses.end(); current != end; ++current) {
      auto next = current;
      for (++next; next != end; ++next) {
        DCHECK(next->GetUser() != current->GetUser());
      }
    }
  }
  return false;
}

void SsaBuilder::RemoveRedundantUninitializedStrings() {
  if (graph_->IsDebuggable()) {
    // Do not perform the optimization for consistency with the interpreter
    // which always allocates an object for new-instance of String.
    return;
  }

  for (HNewInstance* new_instance : uninitialized_strings_) {
    DCHECK(new_instance->IsInBlock());
    DCHECK(new_instance->IsStringAlloc());

    // Replace NewInstance of String with NullConstant if not used prior to
    // calling StringFactory. In case of deoptimization, the interpreter is
    // expected to skip null check on the `this` argument of the StringFactory call.
    if (!new_instance->HasNonEnvironmentUses() && !HasAliasInEnvironments(new_instance)) {
      new_instance->ReplaceWith(graph_->GetNullConstant());
      new_instance->GetBlock()->RemoveInstruction(new_instance);

      // Remove LoadClass if not needed any more.
      HInstruction* input = new_instance->InputAt(0);
      HLoadClass* load_class = nullptr;

      // If the class was not present in the dex cache at the point of building
      // the graph, the builder inserted a HClinitCheck in between. Since the String
      // class is always initialized at the point of running Java code, we can remove
      // that check.
      if (input->IsClinitCheck()) {
        load_class = input->InputAt(0)->AsLoadClass();
        input->ReplaceWith(load_class);
        input->GetBlock()->RemoveInstruction(input);
      } else {
        load_class = input->AsLoadClass();
        DCHECK(new_instance->IsStringAlloc());
        DCHECK(!load_class->NeedsAccessCheck()) << "String class is always accessible";
      }
      DCHECK(load_class != nullptr);
      if (!load_class->HasUses()) {
        // Even if the HLoadClass needs access check, we can remove it, as we know the
        // String class does not need it.
        load_class->GetBlock()->RemoveInstruction(load_class);
      }
    }
  }
}

GraphAnalysisResult SsaBuilder::BuildSsa() {
  DCHECK(!graph_->IsInSsaForm());

  // 1) Propagate types of phis. At this point, phis are typed void in the general
  // case, or float/double/reference if we created an equivalent phi. So we need
  // to propagate the types across phis to give them a correct type. If a type
  // conflict is detected in this stage, the phi is marked dead.
  RunPrimitiveTypePropagation();

  // 2) Now that the correct primitive types have been assigned, we can get rid
  // of redundant phis. Note that we cannot do this phase before type propagation,
  // otherwise we could get rid of phi equivalents, whose presence is a requirement
  // for the type propagation phase. Note that this is to satisfy statement (a)
  // of the SsaBuilder (see ssa_builder.h).
  SsaRedundantPhiElimination(graph_).Run();

  // 3) Fix the type for null constants which are part of an equality comparison.
  // We need to do this after redundant phi elimination, to ensure the only cases
  // that we can see are reference comparison against 0. The redundant phi
  // elimination ensures we do not see a phi taking two 0 constants in a HEqual
  // or HNotEqual.
  FixNullConstantType();

  // 4) Compute type of reference type instructions. The pass assumes that
  // NullConstant has been fixed up.
  ReferenceTypePropagation(graph_,
                           class_loader_,
                           dex_cache_,
                           handles_,
                           /* is_first_run */ true).Run();

  // 5) HInstructionBuilder duplicated ArrayGet instructions with ambiguous type
  // (int/float or long/double) and marked ArraySets with ambiguous input type.
  // Now that RTP computed the type of the array input, the ambiguity can be
  // resolved and the correct equivalents kept.
  if (!FixAmbiguousArrayOps()) {
    return kAnalysisFailAmbiguousArrayOp;
  }

  // 6) Mark dead phis. This will mark phis which are not used by instructions
  // or other live phis. If compiling as debuggable code, phis will also be kept
  // live if they have an environment use.
  SsaDeadPhiElimination dead_phi_elimimation(graph_);
  dead_phi_elimimation.MarkDeadPhis();

  // 7) Make sure environments use the right phi equivalent: a phi marked dead
  // can have a phi equivalent that is not dead. In that case we have to replace
  // it with the live equivalent because deoptimization and try/catch rely on
  // environments containing values of all live vregs at that point. Note that
  // there can be multiple phis for the same Dex register that are live
  // (for example when merging constants), in which case it is okay for the
  // environments to just reference one.
  FixEnvironmentPhis();

  // 8) Now that the right phis are used for the environments, we can eliminate
  // phis we do not need. Regardless of the debuggable status, this phase is
  /// necessary for statement (b) of the SsaBuilder (see ssa_builder.h), as well
  // as for the code generation, which does not deal with phis of conflicting
  // input types.
  dead_phi_elimimation.EliminateDeadPhis();

  // 9) HInstructionBuidler replaced uses of NewInstances of String with the
  // results of their corresponding StringFactory calls. Unless the String
  // objects are used before they are initialized, they can be replaced with
  // NullConstant. Note that this optimization is valid only if unsimplified
  // code does not use the uninitialized value because we assume execution can
  // be deoptimized at any safepoint. We must therefore perform it before any
  // other optimizations.
  RemoveRedundantUninitializedStrings();

  graph_->SetInSsaForm();
  return kAnalysisSuccess;
}

/**
 * Constants in the Dex format are not typed. So the builder types them as
 * integers, but when doing the SSA form, we might realize the constant
 * is used for floating point operations. We create a floating-point equivalent
 * constant to make the operations correctly typed.
 */
HFloatConstant* SsaBuilder::GetFloatEquivalent(HIntConstant* constant) {
  // We place the floating point constant next to this constant.
  HFloatConstant* result = constant->GetNext()->AsFloatConstant();
  if (result == nullptr) {
    float value = bit_cast<float, int32_t>(constant->GetValue());
    result = new (graph_->GetAllocator()) HFloatConstant(value);
    constant->GetBlock()->InsertInstructionBefore(result, constant->GetNext());
    graph_->CacheFloatConstant(result);
  } else {
    // If there is already a constant with the expected type, we know it is
    // the floating point equivalent of this constant.
    DCHECK_EQ((bit_cast<int32_t, float>(result->GetValue())), constant->GetValue());
  }
  return result;
}

/**
 * Wide constants in the Dex format are not typed. So the builder types them as
 * longs, but when doing the SSA form, we might realize the constant
 * is used for floating point operations. We create a floating-point equivalent
 * constant to make the operations correctly typed.
 */
HDoubleConstant* SsaBuilder::GetDoubleEquivalent(HLongConstant* constant) {
  // We place the floating point constant next to this constant.
  HDoubleConstant* result = constant->GetNext()->AsDoubleConstant();
  if (result == nullptr) {
    double value = bit_cast<double, int64_t>(constant->GetValue());
    result = new (graph_->GetAllocator()) HDoubleConstant(value);
    constant->GetBlock()->InsertInstructionBefore(result, constant->GetNext());
    graph_->CacheDoubleConstant(result);
  } else {
    // If there is already a constant with the expected type, we know it is
    // the floating point equivalent of this constant.
    DCHECK_EQ((bit_cast<int64_t, double>(result->GetValue())), constant->GetValue());
  }
  return result;
}

/**
 * Because of Dex format, we might end up having the same phi being
 * used for non floating point operations and floating point / reference operations.
 * Because we want the graph to be correctly typed (and thereafter avoid moves between
 * floating point registers and core registers), we need to create a copy of the
 * phi with a floating point / reference type.
 */
HPhi* SsaBuilder::GetFloatDoubleOrReferenceEquivalentOfPhi(HPhi* phi, DataType::Type type) {
  DCHECK(phi->IsLive()) << "Cannot get equivalent of a dead phi since it would create a live one.";

  // We place the floating point /reference phi next to this phi.
  HInstruction* next = phi->GetNext();
  if (next != nullptr
      && next->AsPhi()->GetRegNumber() == phi->GetRegNumber()
      && next->GetType() != type) {
    // Move to the next phi to see if it is the one we are looking for.
    next = next->GetNext();
  }

  if (next == nullptr
      || (next->AsPhi()->GetRegNumber() != phi->GetRegNumber())
      || (next->GetType() != type)) {
    ArenaAllocator* allocator = graph_->GetAllocator();
    HInputsRef inputs = phi->GetInputs();
    HPhi* new_phi = new (allocator) HPhi(allocator, phi->GetRegNumber(), inputs.size(), type);
    // Copy the inputs. Note that the graph may not be correctly typed
    // by doing this copy, but the type propagation phase will fix it.
    ArrayRef<HUserRecord<HInstruction*>> new_input_records = new_phi->GetInputRecords();
    for (size_t i = 0; i < inputs.size(); ++i) {
      new_input_records[i] = HUserRecord<HInstruction*>(inputs[i]);
    }
    phi->GetBlock()->InsertPhiAfter(new_phi, phi);
    DCHECK(new_phi->IsLive());
    return new_phi;
  } else {
    // An existing equivalent was found. If it is dead, conflict was previously
    // identified and we return nullptr instead.
    HPhi* next_phi = next->AsPhi();
    DCHECK_EQ(next_phi->GetType(), type);
    return next_phi->IsLive() ? next_phi : nullptr;
  }
}

HArrayGet* SsaBuilder::GetFloatOrDoubleEquivalentOfArrayGet(HArrayGet* aget) {
  DCHECK(DataType::IsIntegralType(aget->GetType()));

  if (!DataType::IsIntOrLongType(aget->GetType())) {
    // Cannot type boolean, char, byte, short to float/double.
    return nullptr;
  }

  DCHECK(ContainsElement(ambiguous_agets_, aget));
  if (agets_fixed_) {
    // This used to be an ambiguous ArrayGet but its type has been resolved to
    // int/long. Requesting a float/double equivalent should lead to a conflict.
    if (kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      DCHECK(DataType::IsIntOrLongType(GetPrimitiveArrayComponentType(aget->GetArray())));
    }
    return nullptr;
  } else {
    // This is an ambiguous ArrayGet which has not been resolved yet. Return an
    // equivalent float/double instruction to use until it is resolved.
    HArrayGet* equivalent = FindFloatOrDoubleEquivalentOfArrayGet(aget);
    return (equivalent == nullptr) ? CreateFloatOrDoubleEquivalentOfArrayGet(aget) : equivalent;
  }
}

HInstruction* SsaBuilder::GetFloatOrDoubleEquivalent(HInstruction* value, DataType::Type type) {
  if (value->IsArrayGet()) {
    return GetFloatOrDoubleEquivalentOfArrayGet(value->AsArrayGet());
  } else if (value->IsLongConstant()) {
    return GetDoubleEquivalent(value->AsLongConstant());
  } else if (value->IsIntConstant()) {
    return GetFloatEquivalent(value->AsIntConstant());
  } else if (value->IsPhi()) {
    return GetFloatDoubleOrReferenceEquivalentOfPhi(value->AsPhi(), type);
  } else {
    return nullptr;
  }
}

HInstruction* SsaBuilder::GetReferenceTypeEquivalent(HInstruction* value) {
  if (value->IsIntConstant() && value->AsIntConstant()->GetValue() == 0) {
    return graph_->GetNullConstant();
  } else if (value->IsPhi()) {
    return GetFloatDoubleOrReferenceEquivalentOfPhi(value->AsPhi(), DataType::Type::kReference);
  } else {
    return nullptr;
  }
}

}  // namespace art
