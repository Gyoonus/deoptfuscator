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

#include "reference_type_propagation.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "handle_scope-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

static inline ObjPtr<mirror::DexCache> FindDexCacheWithHint(
    Thread* self, const DexFile& dex_file, Handle<mirror::DexCache> hint_dex_cache)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (LIKELY(hint_dex_cache->GetDexFile() == &dex_file)) {
    return hint_dex_cache.Get();
  } else {
    return Runtime::Current()->GetClassLinker()->FindDexCache(self, dex_file);
  }
}

static inline ReferenceTypeInfo::TypeHandle GetRootHandle(VariableSizedHandleScope* handles,
                                                          ClassLinker::ClassRoot class_root,
                                                          ReferenceTypeInfo::TypeHandle* cache) {
  if (!ReferenceTypeInfo::IsValidHandle(*cache)) {
    // Mutator lock is required for NewHandle.
    ClassLinker* linker = Runtime::Current()->GetClassLinker();
    ScopedObjectAccess soa(Thread::Current());
    *cache = handles->NewHandle(linker->GetClassRoot(class_root));
  }
  return *cache;
}

ReferenceTypeInfo::TypeHandle ReferenceTypePropagation::HandleCache::GetObjectClassHandle() {
  return GetRootHandle(handles_, ClassLinker::kJavaLangObject, &object_class_handle_);
}

ReferenceTypeInfo::TypeHandle ReferenceTypePropagation::HandleCache::GetClassClassHandle() {
  return GetRootHandle(handles_, ClassLinker::kJavaLangClass, &class_class_handle_);
}

ReferenceTypeInfo::TypeHandle ReferenceTypePropagation::HandleCache::GetStringClassHandle() {
  return GetRootHandle(handles_, ClassLinker::kJavaLangString, &string_class_handle_);
}

ReferenceTypeInfo::TypeHandle ReferenceTypePropagation::HandleCache::GetThrowableClassHandle() {
  return GetRootHandle(handles_, ClassLinker::kJavaLangThrowable, &throwable_class_handle_);
}

class ReferenceTypePropagation::RTPVisitor : public HGraphDelegateVisitor {
 public:
  RTPVisitor(HGraph* graph,
             Handle<mirror::ClassLoader> class_loader,
             Handle<mirror::DexCache> hint_dex_cache,
             HandleCache* handle_cache,
             bool is_first_run)
    : HGraphDelegateVisitor(graph),
      class_loader_(class_loader),
      hint_dex_cache_(hint_dex_cache),
      handle_cache_(handle_cache),
      allocator_(graph->GetArenaStack()),
      worklist_(allocator_.Adapter(kArenaAllocReferenceTypePropagation)),
      is_first_run_(is_first_run) {
    worklist_.reserve(kDefaultWorklistSize);
  }

  void VisitDeoptimize(HDeoptimize* deopt) OVERRIDE;
  void VisitNewInstance(HNewInstance* new_instance) OVERRIDE;
  void VisitLoadClass(HLoadClass* load_class) OVERRIDE;
  void VisitClinitCheck(HClinitCheck* clinit_check) OVERRIDE;
  void VisitLoadString(HLoadString* instr) OVERRIDE;
  void VisitLoadException(HLoadException* instr) OVERRIDE;
  void VisitNewArray(HNewArray* instr) OVERRIDE;
  void VisitParameterValue(HParameterValue* instr) OVERRIDE;
  void VisitInstanceFieldGet(HInstanceFieldGet* instr) OVERRIDE;
  void VisitStaticFieldGet(HStaticFieldGet* instr) OVERRIDE;
  void VisitUnresolvedInstanceFieldGet(HUnresolvedInstanceFieldGet* instr) OVERRIDE;
  void VisitUnresolvedStaticFieldGet(HUnresolvedStaticFieldGet* instr) OVERRIDE;
  void VisitInvoke(HInvoke* instr) OVERRIDE;
  void VisitArrayGet(HArrayGet* instr) OVERRIDE;
  void VisitCheckCast(HCheckCast* instr) OVERRIDE;
  void VisitBoundType(HBoundType* instr) OVERRIDE;
  void VisitNullCheck(HNullCheck* instr) OVERRIDE;
  void VisitPhi(HPhi* phi);

  void VisitBasicBlock(HBasicBlock* block);
  void ProcessWorklist();

 private:
  void UpdateFieldAccessTypeInfo(HInstruction* instr, const FieldInfo& info);
  void SetClassAsTypeInfo(HInstruction* instr, ObjPtr<mirror::Class> klass, bool is_exact)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void BoundTypeForIfNotNull(HBasicBlock* block);
  static void BoundTypeForIfInstanceOf(HBasicBlock* block);
  static bool UpdateNullability(HInstruction* instr);
  static void UpdateBoundType(HBoundType* bound_type) REQUIRES_SHARED(Locks::mutator_lock_);
  void UpdateArrayGet(HArrayGet* instr) REQUIRES_SHARED(Locks::mutator_lock_);
  void UpdatePhi(HPhi* phi) REQUIRES_SHARED(Locks::mutator_lock_);
  bool UpdateReferenceTypeInfo(HInstruction* instr);
  void UpdateReferenceTypeInfo(HInstruction* instr,
                               dex::TypeIndex type_idx,
                               const DexFile& dex_file,
                               bool is_exact);

  void AddToWorklist(HInstruction* instruction);
  void AddDependentInstructionsToWorklist(HInstruction* instruction);

  static constexpr size_t kDefaultWorklistSize = 8;

  Handle<mirror::ClassLoader> class_loader_;
  Handle<mirror::DexCache> hint_dex_cache_;
  HandleCache* const handle_cache_;

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator_;
  ScopedArenaVector<HInstruction*> worklist_;
  const bool is_first_run_;
};

ReferenceTypePropagation::ReferenceTypePropagation(HGraph* graph,
                                                   Handle<mirror::ClassLoader> class_loader,
                                                   Handle<mirror::DexCache> hint_dex_cache,
                                                   VariableSizedHandleScope* handles,
                                                   bool is_first_run,
                                                   const char* name)
    : HOptimization(graph, name),
      class_loader_(class_loader),
      hint_dex_cache_(hint_dex_cache),
      handle_cache_(handles),
      is_first_run_(is_first_run) {
}

void ReferenceTypePropagation::ValidateTypes() {
  // TODO: move this to the graph checker.
  if (kIsDebugBuild) {
    ScopedObjectAccess soa(Thread::Current());
    for (HBasicBlock* block : graph_->GetReversePostOrder()) {
      for (HInstructionIterator iti(block->GetInstructions()); !iti.Done(); iti.Advance()) {
        HInstruction* instr = iti.Current();
        if (instr->GetType() == DataType::Type::kReference) {
          DCHECK(instr->GetReferenceTypeInfo().IsValid())
              << "Invalid RTI for instruction: " << instr->DebugName();
          if (instr->IsBoundType()) {
            DCHECK(instr->AsBoundType()->GetUpperBound().IsValid());
          } else if (instr->IsLoadClass()) {
            HLoadClass* cls = instr->AsLoadClass();
            DCHECK(cls->GetReferenceTypeInfo().IsExact());
            DCHECK(!cls->GetLoadedClassRTI().IsValid() || cls->GetLoadedClassRTI().IsExact());
          } else if (instr->IsNullCheck()) {
            DCHECK(instr->GetReferenceTypeInfo().IsEqual(instr->InputAt(0)->GetReferenceTypeInfo()))
                << "NullCheck " << instr->GetReferenceTypeInfo()
                << "Input(0) " << instr->InputAt(0)->GetReferenceTypeInfo();
          }
        }
      }
    }
  }
}

void ReferenceTypePropagation::Visit(HInstruction* instruction) {
  RTPVisitor visitor(graph_,
                     class_loader_,
                     hint_dex_cache_,
                     &handle_cache_,
                     is_first_run_);
  instruction->Accept(&visitor);
}

// Check if we should create a bound type for the given object at the specified
// position. Because of inlining and the fact we run RTP more than once and we
// might have a HBoundType already. If we do, we should not create a new one.
// In this case we also assert that there are no other uses of the object (except
// the bound type) dominated by the specified dominator_instr or dominator_block.
static bool ShouldCreateBoundType(HInstruction* position,
                                  HInstruction* obj,
                                  ReferenceTypeInfo upper_bound,
                                  HInstruction* dominator_instr,
                                  HBasicBlock* dominator_block)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // If the position where we should insert the bound type is not already a
  // a bound type then we need to create one.
  if (position == nullptr || !position->IsBoundType()) {
    return true;
  }

  HBoundType* existing_bound_type = position->AsBoundType();
  if (existing_bound_type->GetUpperBound().IsSupertypeOf(upper_bound)) {
    if (kIsDebugBuild) {
      // Check that the existing HBoundType dominates all the uses.
      for (const HUseListNode<HInstruction*>& use : obj->GetUses()) {
        HInstruction* user = use.GetUser();
        if (dominator_instr != nullptr) {
          DCHECK(!dominator_instr->StrictlyDominates(user)
              || user == existing_bound_type
              || existing_bound_type->StrictlyDominates(user));
        } else if (dominator_block != nullptr) {
          DCHECK(!dominator_block->Dominates(user->GetBlock())
              || user == existing_bound_type
              || existing_bound_type->StrictlyDominates(user));
        }
      }
    }
  } else {
    // TODO: if the current bound type is a refinement we could update the
    // existing_bound_type with the a new upper limit. However, we also need to
    // update its users and have access to the work list.
  }
  return false;
}

// Helper method to bound the type of `receiver` for all instructions dominated
// by `start_block`, or `start_instruction` if `start_block` is null. The new
// bound type will have its upper bound be `class_rti`.
static void BoundTypeIn(HInstruction* receiver,
                        HBasicBlock* start_block,
                        HInstruction* start_instruction,
                        const ReferenceTypeInfo& class_rti) {
  // We only need to bound the type if we have uses in the relevant block.
  // So start with null and create the HBoundType lazily, only if it's needed.
  HBoundType* bound_type = nullptr;
  DCHECK(!receiver->IsLoadClass()) << "We should not replace HLoadClass instructions";
  const HUseList<HInstruction*>& uses = receiver->GetUses();
  for (auto it = uses.begin(), end = uses.end(); it != end; /* ++it below */) {
    HInstruction* user = it->GetUser();
    size_t index = it->GetIndex();
    // Increment `it` now because `*it` may disappear thanks to user->ReplaceInput().
    ++it;
    bool dominates = (start_instruction != nullptr)
        ? start_instruction->StrictlyDominates(user)
        : start_block->Dominates(user->GetBlock());
    if (!dominates) {
      continue;
    }
    if (bound_type == nullptr) {
      ScopedObjectAccess soa(Thread::Current());
      HInstruction* insert_point = (start_instruction != nullptr)
          ? start_instruction->GetNext()
          : start_block->GetFirstInstruction();
      if (ShouldCreateBoundType(
            insert_point, receiver, class_rti, start_instruction, start_block)) {
        bound_type = new (receiver->GetBlock()->GetGraph()->GetAllocator()) HBoundType(receiver);
        bound_type->SetUpperBound(class_rti, /* bound_can_be_null */ false);
        start_block->InsertInstructionBefore(bound_type, insert_point);
        // To comply with the RTP algorithm, don't type the bound type just yet, it will
        // be handled in RTPVisitor::VisitBoundType.
      } else {
        // We already have a bound type on the position we would need to insert
        // the new one. The existing bound type should dominate all the users
        // (dchecked) so there's no need to continue.
        break;
      }
    }
    user->ReplaceInput(bound_type, index);
  }
  // If the receiver is a null check, also bound the type of the actual
  // receiver.
  if (receiver->IsNullCheck()) {
    BoundTypeIn(receiver->InputAt(0), start_block, start_instruction, class_rti);
  }
}

// Recognize the patterns:
// if (obj.shadow$_klass_ == Foo.class) ...
// deoptimize if (obj.shadow$_klass_ == Foo.class)
static void BoundTypeForClassCheck(HInstruction* check) {
  if (!check->IsIf() && !check->IsDeoptimize()) {
    return;
  }
  HInstruction* compare = check->InputAt(0);
  if (!compare->IsEqual() && !compare->IsNotEqual()) {
    return;
  }
  HInstruction* input_one = compare->InputAt(0);
  HInstruction* input_two = compare->InputAt(1);
  HLoadClass* load_class = input_one->IsLoadClass()
      ? input_one->AsLoadClass()
      : input_two->AsLoadClass();
  if (load_class == nullptr) {
    return;
  }

  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  if (!class_rti.IsValid()) {
    // We have loaded an unresolved class. Don't bother bounding the type.
    return;
  }

  HInstanceFieldGet* field_get = (load_class == input_one)
      ? input_two->AsInstanceFieldGet()
      : input_one->AsInstanceFieldGet();
  if (field_get == nullptr) {
    return;
  }
  HInstruction* receiver = field_get->InputAt(0);
  ReferenceTypeInfo receiver_type = receiver->GetReferenceTypeInfo();
  if (receiver_type.IsExact()) {
    // If we already know the receiver type, don't bother updating its users.
    return;
  }

  {
    ScopedObjectAccess soa(Thread::Current());
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    ArtField* field = class_linker->GetClassRoot(ClassLinker::kJavaLangObject)->GetInstanceField(0);
    DCHECK_EQ(std::string(field->GetName()), "shadow$_klass_");
    if (field_get->GetFieldInfo().GetField() != field) {
      return;
    }
  }

  if (check->IsIf()) {
    HBasicBlock* trueBlock = compare->IsEqual()
        ? check->AsIf()->IfTrueSuccessor()
        : check->AsIf()->IfFalseSuccessor();
    BoundTypeIn(receiver, trueBlock, /* start_instruction */ nullptr, class_rti);
  } else {
    DCHECK(check->IsDeoptimize());
    if (compare->IsEqual() && check->AsDeoptimize()->GuardsAnInput()) {
      check->SetReferenceTypeInfo(class_rti);
    }
  }
}

void ReferenceTypePropagation::Run() {
  RTPVisitor visitor(graph_, class_loader_, hint_dex_cache_, &handle_cache_, is_first_run_);

  // To properly propagate type info we need to visit in the dominator-based order.
  // Reverse post order guarantees a node's dominators are visited first.
  // We take advantage of this order in `VisitBasicBlock`.
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    visitor.VisitBasicBlock(block);
  }

  visitor.ProcessWorklist();
  ValidateTypes();
}

void ReferenceTypePropagation::RTPVisitor::VisitBasicBlock(HBasicBlock* block) {
  // Handle Phis first as there might be instructions in the same block who depend on them.
  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    VisitPhi(it.Current()->AsPhi());
  }

  // Handle instructions. Since RTP may add HBoundType instructions just after the
  // last visited instruction, use `HInstructionIteratorHandleChanges` iterator.
  for (HInstructionIteratorHandleChanges it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instr = it.Current();
    instr->Accept(this);
  }

  // Add extra nodes to bound types.
  BoundTypeForIfNotNull(block);
  BoundTypeForIfInstanceOf(block);
  BoundTypeForClassCheck(block->GetLastInstruction());
}

void ReferenceTypePropagation::RTPVisitor::BoundTypeForIfNotNull(HBasicBlock* block) {
  HIf* ifInstruction = block->GetLastInstruction()->AsIf();
  if (ifInstruction == nullptr) {
    return;
  }
  HInstruction* ifInput = ifInstruction->InputAt(0);
  if (!ifInput->IsNotEqual() && !ifInput->IsEqual()) {
    return;
  }
  HInstruction* input0 = ifInput->InputAt(0);
  HInstruction* input1 = ifInput->InputAt(1);
  HInstruction* obj = nullptr;

  if (input1->IsNullConstant()) {
    obj = input0;
  } else if (input0->IsNullConstant()) {
    obj = input1;
  } else {
    return;
  }

  if (!obj->CanBeNull() || obj->IsNullConstant()) {
    // Null check is dead code and will be removed by DCE.
    return;
  }
  DCHECK(!obj->IsLoadClass()) << "We should not replace HLoadClass instructions";

  // We only need to bound the type if we have uses in the relevant block.
  // So start with null and create the HBoundType lazily, only if it's needed.
  HBasicBlock* notNullBlock = ifInput->IsNotEqual()
      ? ifInstruction->IfTrueSuccessor()
      : ifInstruction->IfFalseSuccessor();

  ReferenceTypeInfo object_rti = ReferenceTypeInfo::Create(
      handle_cache_->GetObjectClassHandle(), /* is_exact */ false);

  BoundTypeIn(obj, notNullBlock, /* start_instruction */ nullptr, object_rti);
}

// Returns true if one of the patterns below has been recognized. If so, the
// InstanceOf instruction together with the true branch of `ifInstruction` will
// be returned using the out parameters.
// Recognized patterns:
//   (1) patterns equivalent to `if (obj instanceof X)`
//     (a) InstanceOf -> Equal to 1 -> If
//     (b) InstanceOf -> NotEqual to 0 -> If
//     (c) InstanceOf -> If
//   (2) patterns equivalent to `if (!(obj instanceof X))`
//     (a) InstanceOf -> Equal to 0 -> If
//     (b) InstanceOf -> NotEqual to 1 -> If
//     (c) InstanceOf -> BooleanNot -> If
static bool MatchIfInstanceOf(HIf* ifInstruction,
                              /* out */ HInstanceOf** instanceOf,
                              /* out */ HBasicBlock** trueBranch) {
  HInstruction* input = ifInstruction->InputAt(0);

  if (input->IsEqual()) {
    HInstruction* rhs = input->AsEqual()->GetConstantRight();
    if (rhs != nullptr) {
      HInstruction* lhs = input->AsEqual()->GetLeastConstantLeft();
      if (lhs->IsInstanceOf() && rhs->IsIntConstant()) {
        if (rhs->AsIntConstant()->IsTrue()) {
          // Case (1a)
          *trueBranch = ifInstruction->IfTrueSuccessor();
        } else {
          // Case (2a)
          DCHECK(rhs->AsIntConstant()->IsFalse()) << rhs->AsIntConstant()->GetValue();
          *trueBranch = ifInstruction->IfFalseSuccessor();
        }
        *instanceOf = lhs->AsInstanceOf();
        return true;
      }
    }
  } else if (input->IsNotEqual()) {
    HInstruction* rhs = input->AsNotEqual()->GetConstantRight();
    if (rhs != nullptr) {
      HInstruction* lhs = input->AsNotEqual()->GetLeastConstantLeft();
      if (lhs->IsInstanceOf() && rhs->IsIntConstant()) {
        if (rhs->AsIntConstant()->IsFalse()) {
          // Case (1b)
          *trueBranch = ifInstruction->IfTrueSuccessor();
        } else {
          // Case (2b)
          DCHECK(rhs->AsIntConstant()->IsTrue()) << rhs->AsIntConstant()->GetValue();
          *trueBranch = ifInstruction->IfFalseSuccessor();
        }
        *instanceOf = lhs->AsInstanceOf();
        return true;
      }
    }
  } else if (input->IsInstanceOf()) {
    // Case (1c)
    *instanceOf = input->AsInstanceOf();
    *trueBranch = ifInstruction->IfTrueSuccessor();
    return true;
  } else if (input->IsBooleanNot()) {
    HInstruction* not_input = input->InputAt(0);
    if (not_input->IsInstanceOf()) {
      // Case (2c)
      *instanceOf = not_input->AsInstanceOf();
      *trueBranch = ifInstruction->IfFalseSuccessor();
      return true;
    }
  }

  return false;
}

// Detects if `block` is the True block for the pattern
// `if (x instanceof ClassX) { }`
// If that's the case insert an HBoundType instruction to bound the type of `x`
// to `ClassX` in the scope of the dominated blocks.
void ReferenceTypePropagation::RTPVisitor::BoundTypeForIfInstanceOf(HBasicBlock* block) {
  HIf* ifInstruction = block->GetLastInstruction()->AsIf();
  if (ifInstruction == nullptr) {
    return;
  }

  // Try to recognize common `if (instanceof)` and `if (!instanceof)` patterns.
  HInstanceOf* instanceOf = nullptr;
  HBasicBlock* instanceOfTrueBlock = nullptr;
  if (!MatchIfInstanceOf(ifInstruction, &instanceOf, &instanceOfTrueBlock)) {
    return;
  }

  HLoadClass* load_class = instanceOf->InputAt(1)->AsLoadClass();
  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  if (!class_rti.IsValid()) {
    // He have loaded an unresolved class. Don't bother bounding the type.
    return;
  }

  HInstruction* obj = instanceOf->InputAt(0);
  if (obj->GetReferenceTypeInfo().IsExact() && !obj->IsPhi()) {
    // This method is being called while doing a fixed-point calculation
    // over phis. Non-phis instruction whose type is already known do
    // not need to be bound to another type.
    // Not that this also prevents replacing `HLoadClass` with a `HBoundType`.
    // `HCheckCast` and `HInstanceOf` expect a `HLoadClass` as a second
    // input.
    return;
  }

  {
    ScopedObjectAccess soa(Thread::Current());
    if (!class_rti.GetTypeHandle()->CannotBeAssignedFromOtherTypes()) {
      class_rti = ReferenceTypeInfo::Create(class_rti.GetTypeHandle(), /* is_exact */ false);
    }
  }
  BoundTypeIn(obj, instanceOfTrueBlock, /* start_instruction */ nullptr, class_rti);
}

void ReferenceTypePropagation::RTPVisitor::SetClassAsTypeInfo(HInstruction* instr,
                                                              ObjPtr<mirror::Class> klass,
                                                              bool is_exact) {
  if (instr->IsInvokeStaticOrDirect() && instr->AsInvokeStaticOrDirect()->IsStringInit()) {
    // Calls to String.<init> are replaced with a StringFactory.
    if (kIsDebugBuild) {
      HInvokeStaticOrDirect* invoke = instr->AsInvokeStaticOrDirect();
      ClassLinker* cl = Runtime::Current()->GetClassLinker();
      Thread* self = Thread::Current();
      StackHandleScope<2> hs(self);
      const DexFile& dex_file = *invoke->GetTargetMethod().dex_file;
      uint32_t dex_method_index = invoke->GetTargetMethod().index;
      Handle<mirror::DexCache> dex_cache(
          hs.NewHandle(FindDexCacheWithHint(self, dex_file, hint_dex_cache_)));
      // Use a null loader, the target method is in a boot classpath dex file.
      Handle<mirror::ClassLoader> loader(hs.NewHandle<mirror::ClassLoader>(nullptr));
      ArtMethod* method = cl->ResolveMethod<ClassLinker::ResolveMode::kNoChecks>(
          dex_method_index, dex_cache, loader, /* referrer */ nullptr, kDirect);
      DCHECK(method != nullptr);
      mirror::Class* declaring_class = method->GetDeclaringClass();
      DCHECK(declaring_class != nullptr);
      DCHECK(declaring_class->IsStringClass())
          << "Expected String class: " << declaring_class->PrettyDescriptor();
      DCHECK(method->IsConstructor())
          << "Expected String.<init>: " << method->PrettyMethod();
    }
    instr->SetReferenceTypeInfo(
        ReferenceTypeInfo::Create(handle_cache_->GetStringClassHandle(), /* is_exact */ true));
  } else if (IsAdmissible(klass.Ptr())) {
    ReferenceTypeInfo::TypeHandle handle = handle_cache_->NewHandle(klass);
    is_exact = is_exact || handle->CannotBeAssignedFromOtherTypes();
    instr->SetReferenceTypeInfo(ReferenceTypeInfo::Create(handle, is_exact));
  } else {
    instr->SetReferenceTypeInfo(instr->GetBlock()->GetGraph()->GetInexactObjectRti());
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitDeoptimize(HDeoptimize* instr) {
  BoundTypeForClassCheck(instr);
}

void ReferenceTypePropagation::RTPVisitor::UpdateReferenceTypeInfo(HInstruction* instr,
                                                                   dex::TypeIndex type_idx,
                                                                   const DexFile& dex_file,
                                                                   bool is_exact) {
  DCHECK_EQ(instr->GetType(), DataType::Type::kReference);

  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::DexCache> dex_cache = FindDexCacheWithHint(soa.Self(), dex_file, hint_dex_cache_);
  ObjPtr<mirror::Class> klass = Runtime::Current()->GetClassLinker()->LookupResolvedType(
      type_idx, dex_cache, class_loader_.Get());
  SetClassAsTypeInfo(instr, klass, is_exact);
}

void ReferenceTypePropagation::RTPVisitor::VisitNewInstance(HNewInstance* instr) {
  ScopedObjectAccess soa(Thread::Current());
  SetClassAsTypeInfo(instr, instr->GetLoadClass()->GetClass().Get(), /* is_exact */ true);
}

void ReferenceTypePropagation::RTPVisitor::VisitNewArray(HNewArray* instr) {
  ScopedObjectAccess soa(Thread::Current());
  SetClassAsTypeInfo(instr, instr->GetLoadClass()->GetClass().Get(), /* is_exact */ true);
}

void ReferenceTypePropagation::RTPVisitor::VisitParameterValue(HParameterValue* instr) {
  // We check if the existing type is valid: the inliner may have set it.
  if (instr->GetType() == DataType::Type::kReference && !instr->GetReferenceTypeInfo().IsValid()) {
    UpdateReferenceTypeInfo(instr,
                            instr->GetTypeIndex(),
                            instr->GetDexFile(),
                            /* is_exact */ false);
  }
}

void ReferenceTypePropagation::RTPVisitor::UpdateFieldAccessTypeInfo(HInstruction* instr,
                                                                     const FieldInfo& info) {
  if (instr->GetType() != DataType::Type::kReference) {
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  ObjPtr<mirror::Class> klass;

  // The field is unknown only during tests.
  if (info.GetField() != nullptr) {
    klass = info.GetField()->LookupResolvedType();
  }

  SetClassAsTypeInfo(instr, klass, /* is_exact */ false);
}

void ReferenceTypePropagation::RTPVisitor::VisitInstanceFieldGet(HInstanceFieldGet* instr) {
  UpdateFieldAccessTypeInfo(instr, instr->GetFieldInfo());
}

void ReferenceTypePropagation::RTPVisitor::VisitStaticFieldGet(HStaticFieldGet* instr) {
  UpdateFieldAccessTypeInfo(instr, instr->GetFieldInfo());
}

void ReferenceTypePropagation::RTPVisitor::VisitUnresolvedInstanceFieldGet(
    HUnresolvedInstanceFieldGet* instr) {
  // TODO: Use descriptor to get the actual type.
  if (instr->GetFieldType() == DataType::Type::kReference) {
    instr->SetReferenceTypeInfo(instr->GetBlock()->GetGraph()->GetInexactObjectRti());
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitUnresolvedStaticFieldGet(
    HUnresolvedStaticFieldGet* instr) {
  // TODO: Use descriptor to get the actual type.
  if (instr->GetFieldType() == DataType::Type::kReference) {
    instr->SetReferenceTypeInfo(instr->GetBlock()->GetGraph()->GetInexactObjectRti());
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitLoadClass(HLoadClass* instr) {
  ScopedObjectAccess soa(Thread::Current());
  Handle<mirror::Class> resolved_class = instr->GetClass();
  if (IsAdmissible(resolved_class.Get())) {
    instr->SetLoadedClassRTI(ReferenceTypeInfo::Create(
        resolved_class, /* is_exact */ true));
  }
  instr->SetReferenceTypeInfo(
      ReferenceTypeInfo::Create(handle_cache_->GetClassClassHandle(), /* is_exact */ true));
}

void ReferenceTypePropagation::RTPVisitor::VisitClinitCheck(HClinitCheck* instr) {
  instr->SetReferenceTypeInfo(instr->InputAt(0)->GetReferenceTypeInfo());
}

void ReferenceTypePropagation::RTPVisitor::VisitLoadString(HLoadString* instr) {
  instr->SetReferenceTypeInfo(
      ReferenceTypeInfo::Create(handle_cache_->GetStringClassHandle(), /* is_exact */ true));
}

void ReferenceTypePropagation::RTPVisitor::VisitLoadException(HLoadException* instr) {
  DCHECK(instr->GetBlock()->IsCatchBlock());
  TryCatchInformation* catch_info = instr->GetBlock()->GetTryCatchInformation();

  if (catch_info->IsCatchAllTypeIndex()) {
    instr->SetReferenceTypeInfo(
        ReferenceTypeInfo::Create(handle_cache_->GetThrowableClassHandle(), /* is_exact */ false));
  } else {
    UpdateReferenceTypeInfo(instr,
                            catch_info->GetCatchTypeIndex(),
                            catch_info->GetCatchDexFile(),
                            /* is_exact */ false);
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitNullCheck(HNullCheck* instr) {
  ReferenceTypeInfo parent_rti = instr->InputAt(0)->GetReferenceTypeInfo();
  if (parent_rti.IsValid()) {
    instr->SetReferenceTypeInfo(parent_rti);
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitBoundType(HBoundType* instr) {
  ReferenceTypeInfo class_rti = instr->GetUpperBound();
  if (class_rti.IsValid()) {
    ScopedObjectAccess soa(Thread::Current());
    // Narrow the type as much as possible.
    HInstruction* obj = instr->InputAt(0);
    ReferenceTypeInfo obj_rti = obj->GetReferenceTypeInfo();
    if (class_rti.IsExact()) {
      instr->SetReferenceTypeInfo(class_rti);
    } else if (obj_rti.IsValid()) {
      if (class_rti.IsSupertypeOf(obj_rti)) {
        // Object type is more specific.
        instr->SetReferenceTypeInfo(obj_rti);
      } else {
        // Upper bound is more specific, or unrelated to the object's type.
        // Note that the object might then be exact, and we know the code dominated by this
        // bound type is dead. To not confuse potential other optimizations, we mark
        // the bound as non-exact.
        instr->SetReferenceTypeInfo(
            ReferenceTypeInfo::Create(class_rti.GetTypeHandle(), /* is_exact */ false));
      }
    } else {
      // Object not typed yet. Leave BoundType untyped for now rather than
      // assign the type conservatively.
    }
    instr->SetCanBeNull(obj->CanBeNull() && instr->GetUpperCanBeNull());
  } else {
    // The owner of the BoundType was already visited. If the class is unresolved,
    // the BoundType should have been removed from the data flow and this method
    // should remove it from the graph.
    DCHECK(!instr->HasUses());
    instr->GetBlock()->RemoveInstruction(instr);
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitCheckCast(HCheckCast* check_cast) {
  HLoadClass* load_class = check_cast->InputAt(1)->AsLoadClass();
  ReferenceTypeInfo class_rti = load_class->GetLoadedClassRTI();
  HBoundType* bound_type = check_cast->GetNext()->AsBoundType();
  if (bound_type == nullptr || bound_type->GetUpperBound().IsValid()) {
    // The next instruction is not an uninitialized BoundType. This must be
    // an RTP pass after SsaBuilder and we do not need to do anything.
    return;
  }
  DCHECK_EQ(bound_type->InputAt(0), check_cast->InputAt(0));

  if (class_rti.IsValid()) {
    DCHECK(is_first_run_);
    ScopedObjectAccess soa(Thread::Current());
    // This is the first run of RTP and class is resolved.
    bool is_exact = class_rti.GetTypeHandle()->CannotBeAssignedFromOtherTypes();
    bound_type->SetUpperBound(ReferenceTypeInfo::Create(class_rti.GetTypeHandle(), is_exact),
                              /* CheckCast succeeds for nulls. */ true);
  } else {
    // This is the first run of RTP and class is unresolved. Remove the binding.
    // The instruction itself is removed in VisitBoundType so as to not
    // invalidate HInstructionIterator.
    bound_type->ReplaceWith(bound_type->InputAt(0));
  }
}

void ReferenceTypePropagation::RTPVisitor::VisitPhi(HPhi* phi) {
  if (phi->IsDead() || phi->GetType() != DataType::Type::kReference) {
    return;
  }

  if (phi->GetBlock()->IsLoopHeader()) {
    // Set the initial type for the phi. Use the non back edge input for reaching
    // a fixed point faster.
    HInstruction* first_input = phi->InputAt(0);
    ReferenceTypeInfo first_input_rti = first_input->GetReferenceTypeInfo();
    if (first_input_rti.IsValid() && !first_input->IsNullConstant()) {
      phi->SetCanBeNull(first_input->CanBeNull());
      phi->SetReferenceTypeInfo(first_input_rti);
    }
    AddToWorklist(phi);
  } else {
    // Eagerly compute the type of the phi, for quicker convergence. Note
    // that we don't need to add users to the worklist because we are
    // doing a reverse post-order visit, therefore either the phi users are
    // non-loop phi and will be visited later in the visit, or are loop-phis,
    // and they are already in the work list.
    UpdateNullability(phi);
    UpdateReferenceTypeInfo(phi);
  }
}

void ReferenceTypePropagation::FixUpInstructionType(HInstruction* instruction,
                                                    VariableSizedHandleScope* handle_scope) {
  if (instruction->IsSelect()) {
    ScopedObjectAccess soa(Thread::Current());
    HandleCache handle_cache(handle_scope);
    HSelect* select = instruction->AsSelect();
    ReferenceTypeInfo false_rti = select->GetFalseValue()->GetReferenceTypeInfo();
    ReferenceTypeInfo true_rti = select->GetTrueValue()->GetReferenceTypeInfo();
    select->SetReferenceTypeInfo(MergeTypes(false_rti, true_rti, &handle_cache));
  } else {
    LOG(FATAL) << "Invalid instruction in FixUpInstructionType";
  }
}

ReferenceTypeInfo ReferenceTypePropagation::MergeTypes(const ReferenceTypeInfo& a,
                                                       const ReferenceTypeInfo& b,
                                                       HandleCache* handle_cache) {
  if (!b.IsValid()) {
    return a;
  }
  if (!a.IsValid()) {
    return b;
  }

  bool is_exact = a.IsExact() && b.IsExact();
  ReferenceTypeInfo::TypeHandle result_type_handle;
  ReferenceTypeInfo::TypeHandle a_type_handle = a.GetTypeHandle();
  ReferenceTypeInfo::TypeHandle b_type_handle = b.GetTypeHandle();
  bool a_is_interface = a_type_handle->IsInterface();
  bool b_is_interface = b_type_handle->IsInterface();

  if (a.GetTypeHandle().Get() == b.GetTypeHandle().Get()) {
    result_type_handle = a_type_handle;
  } else if (a.IsSupertypeOf(b)) {
    result_type_handle = a_type_handle;
    is_exact = false;
  } else if (b.IsSupertypeOf(a)) {
    result_type_handle = b_type_handle;
    is_exact = false;
  } else if (!a_is_interface && !b_is_interface) {
    result_type_handle =
        handle_cache->NewHandle(a_type_handle->GetCommonSuperClass(b_type_handle));
    is_exact = false;
  } else {
    // This can happen if:
    //    - both types are interfaces. TODO(calin): implement
    //    - one is an interface, the other a class, and the type does not implement the interface
    //      e.g:
    //        void foo(Interface i, boolean cond) {
    //          Object o = cond ? i : new Object();
    //        }
    result_type_handle = handle_cache->GetObjectClassHandle();
    is_exact = false;
  }

  return ReferenceTypeInfo::Create(result_type_handle, is_exact);
}

void ReferenceTypePropagation::RTPVisitor::UpdateArrayGet(HArrayGet* instr) {
  DCHECK_EQ(DataType::Type::kReference, instr->GetType());

  ReferenceTypeInfo parent_rti = instr->InputAt(0)->GetReferenceTypeInfo();
  if (!parent_rti.IsValid()) {
    return;
  }

  Handle<mirror::Class> handle = parent_rti.GetTypeHandle();
  if (handle->IsObjectArrayClass() && IsAdmissible(handle->GetComponentType())) {
    ReferenceTypeInfo::TypeHandle component_handle =
        handle_cache_->NewHandle(handle->GetComponentType());
    bool is_exact = component_handle->CannotBeAssignedFromOtherTypes();
    instr->SetReferenceTypeInfo(ReferenceTypeInfo::Create(component_handle, is_exact));
  } else {
    // We don't know what the parent actually is, so we fallback to object.
    instr->SetReferenceTypeInfo(instr->GetBlock()->GetGraph()->GetInexactObjectRti());
  }
}

bool ReferenceTypePropagation::RTPVisitor::UpdateReferenceTypeInfo(HInstruction* instr) {
  ScopedObjectAccess soa(Thread::Current());

  ReferenceTypeInfo previous_rti = instr->GetReferenceTypeInfo();
  if (instr->IsBoundType()) {
    UpdateBoundType(instr->AsBoundType());
  } else if (instr->IsPhi()) {
    UpdatePhi(instr->AsPhi());
  } else if (instr->IsNullCheck()) {
    ReferenceTypeInfo parent_rti = instr->InputAt(0)->GetReferenceTypeInfo();
    if (parent_rti.IsValid()) {
      instr->SetReferenceTypeInfo(parent_rti);
    }
  } else if (instr->IsArrayGet()) {
    // TODO: consider if it's worth "looking back" and binding the input object
    // to an array type.
    UpdateArrayGet(instr->AsArrayGet());
  } else {
    LOG(FATAL) << "Invalid instruction (should not get here)";
  }

  return !previous_rti.IsEqual(instr->GetReferenceTypeInfo());
}

void ReferenceTypePropagation::RTPVisitor::VisitInvoke(HInvoke* instr) {
  if (instr->GetType() != DataType::Type::kReference) {
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  ArtMethod* method = instr->GetResolvedMethod();
  ObjPtr<mirror::Class> klass = (method == nullptr) ? nullptr : method->LookupResolvedReturnType();
  SetClassAsTypeInfo(instr, klass, /* is_exact */ false);
}

void ReferenceTypePropagation::RTPVisitor::VisitArrayGet(HArrayGet* instr) {
  if (instr->GetType() != DataType::Type::kReference) {
    return;
  }

  ScopedObjectAccess soa(Thread::Current());
  UpdateArrayGet(instr);
  if (!instr->GetReferenceTypeInfo().IsValid()) {
    worklist_.push_back(instr);
  }
}

void ReferenceTypePropagation::RTPVisitor::UpdateBoundType(HBoundType* instr) {
  ReferenceTypeInfo input_rti = instr->InputAt(0)->GetReferenceTypeInfo();
  if (!input_rti.IsValid()) {
    return;  // No new info yet.
  }

  ReferenceTypeInfo upper_bound_rti = instr->GetUpperBound();
  if (upper_bound_rti.IsExact()) {
    instr->SetReferenceTypeInfo(upper_bound_rti);
  } else if (upper_bound_rti.IsSupertypeOf(input_rti)) {
    // input is more specific.
    instr->SetReferenceTypeInfo(input_rti);
  } else {
    // upper_bound is more specific or unrelated.
    // Note that the object might then be exact, and we know the code dominated by this
    // bound type is dead. To not confuse potential other optimizations, we mark
    // the bound as non-exact.
    instr->SetReferenceTypeInfo(
        ReferenceTypeInfo::Create(upper_bound_rti.GetTypeHandle(), /* is_exact */ false));
  }
}

// NullConstant inputs are ignored during merging as they do not provide any useful information.
// If all the inputs are NullConstants then the type of the phi will be set to Object.
void ReferenceTypePropagation::RTPVisitor::UpdatePhi(HPhi* instr) {
  DCHECK(instr->IsLive());

  HInputsRef inputs = instr->GetInputs();
  size_t first_input_index_not_null = 0;
  while (first_input_index_not_null < inputs.size() &&
         inputs[first_input_index_not_null]->IsNullConstant()) {
    first_input_index_not_null++;
  }
  if (first_input_index_not_null == inputs.size()) {
    // All inputs are NullConstants, set the type to object.
    // This may happen in the presence of inlining.
    instr->SetReferenceTypeInfo(instr->GetBlock()->GetGraph()->GetInexactObjectRti());
    return;
  }

  ReferenceTypeInfo new_rti = instr->InputAt(first_input_index_not_null)->GetReferenceTypeInfo();

  if (new_rti.IsValid() && new_rti.IsObjectClass() && !new_rti.IsExact()) {
    // Early return if we are Object and inexact.
    instr->SetReferenceTypeInfo(new_rti);
    return;
  }

  for (size_t i = first_input_index_not_null + 1; i < inputs.size(); i++) {
    if (inputs[i]->IsNullConstant()) {
      continue;
    }
    new_rti = MergeTypes(new_rti, inputs[i]->GetReferenceTypeInfo(), handle_cache_);
    if (new_rti.IsValid() && new_rti.IsObjectClass()) {
      if (!new_rti.IsExact()) {
        break;
      } else {
        continue;
      }
    }
  }

  if (new_rti.IsValid()) {
    instr->SetReferenceTypeInfo(new_rti);
  }
}

// Re-computes and updates the nullability of the instruction. Returns whether or
// not the nullability was changed.
bool ReferenceTypePropagation::RTPVisitor::UpdateNullability(HInstruction* instr) {
  DCHECK((instr->IsPhi() && instr->AsPhi()->IsLive())
      || instr->IsBoundType()
      || instr->IsNullCheck()
      || instr->IsArrayGet());

  if (!instr->IsPhi() && !instr->IsBoundType()) {
    return false;
  }

  bool existing_can_be_null = instr->CanBeNull();
  if (instr->IsPhi()) {
    HPhi* phi = instr->AsPhi();
    bool new_can_be_null = false;
    for (HInstruction* input : phi->GetInputs()) {
      if (input->CanBeNull()) {
        new_can_be_null = true;
        break;
      }
    }
    phi->SetCanBeNull(new_can_be_null);
  } else if (instr->IsBoundType()) {
    HBoundType* bound_type = instr->AsBoundType();
    bound_type->SetCanBeNull(instr->InputAt(0)->CanBeNull() && bound_type->GetUpperCanBeNull());
  }
  return existing_can_be_null != instr->CanBeNull();
}

void ReferenceTypePropagation::RTPVisitor::ProcessWorklist() {
  while (!worklist_.empty()) {
    HInstruction* instruction = worklist_.back();
    worklist_.pop_back();
    bool updated_nullability = UpdateNullability(instruction);
    bool updated_reference_type = UpdateReferenceTypeInfo(instruction);
    if (updated_nullability || updated_reference_type) {
      AddDependentInstructionsToWorklist(instruction);
    }
  }
}

void ReferenceTypePropagation::RTPVisitor::AddToWorklist(HInstruction* instruction) {
  DCHECK_EQ(instruction->GetType(), DataType::Type::kReference)
      << instruction->DebugName() << ":" << instruction->GetType();
  worklist_.push_back(instruction);
}

void ReferenceTypePropagation::RTPVisitor::AddDependentInstructionsToWorklist(
    HInstruction* instruction) {
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    HInstruction* user = use.GetUser();
    if ((user->IsPhi() && user->AsPhi()->IsLive())
       || user->IsBoundType()
       || user->IsNullCheck()
       || (user->IsArrayGet() && (user->GetType() == DataType::Type::kReference))) {
      AddToWorklist(user);
    }
  }
}

}  // namespace art
