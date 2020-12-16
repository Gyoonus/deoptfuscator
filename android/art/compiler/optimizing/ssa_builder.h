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

#ifndef ART_COMPILER_OPTIMIZING_SSA_BUILDER_H_
#define ART_COMPILER_OPTIMIZING_SSA_BUILDER_H_

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Transforms a graph into SSA form. The liveness guarantees of
 * this transformation are listed below. A DEX register
 * being killed means its value at a given position in the code
 * will not be available to its environment uses. A merge in the
 * following text is materialized as a `HPhi`.
 *
 * (a) Dex registers that do not require merging (that is, they do not
 *     have different values at a join block) are available to all their
 *     environment uses. Note that it does not imply the instruction will
 *     have a physical location after register allocation. See the
 *     SsaLivenessAnalysis phase.
 *
 * (b) Dex registers that require merging, and the merging gives
 *     incompatible types, will be killed for environment uses of that merge.
 *
 * (c) When the `debuggable` flag is passed to the compiler, Dex registers
 *     that require merging and have a proper type after the merge, are
 *     available to all their environment uses. If the `debuggable` flag
 *     is not set, values of Dex registers only used by environments
 *     are killed.
 */
class SsaBuilder : public ValueObject {
 public:
  SsaBuilder(HGraph* graph,
             Handle<mirror::ClassLoader> class_loader,
             Handle<mirror::DexCache> dex_cache,
             VariableSizedHandleScope* handles,
             ScopedArenaAllocator* local_allocator)
      : graph_(graph),
        class_loader_(class_loader),
        dex_cache_(dex_cache),
        handles_(handles),
        agets_fixed_(false),
        local_allocator_(local_allocator),
        ambiguous_agets_(local_allocator->Adapter(kArenaAllocGraphBuilder)),
        ambiguous_asets_(local_allocator->Adapter(kArenaAllocGraphBuilder)),
        uninitialized_strings_(local_allocator->Adapter(kArenaAllocGraphBuilder)) {
    graph_->InitializeInexactObjectRTI(handles);
  }

  GraphAnalysisResult BuildSsa();

  HInstruction* GetFloatOrDoubleEquivalent(HInstruction* instruction, DataType::Type type);
  HInstruction* GetReferenceTypeEquivalent(HInstruction* instruction);

  void MaybeAddAmbiguousArrayGet(HArrayGet* aget) {
    DataType::Type type = aget->GetType();
    DCHECK(!DataType::IsFloatingPointType(type));
    if (DataType::IsIntOrLongType(type)) {
      ambiguous_agets_.push_back(aget);
    }
  }

  void MaybeAddAmbiguousArraySet(HArraySet* aset) {
    DataType::Type type = aset->GetValue()->GetType();
    if (DataType::IsIntOrLongType(type)) {
      ambiguous_asets_.push_back(aset);
    }
  }

  void AddUninitializedString(HNewInstance* string) {
    // In some rare cases (b/27847265), the same NewInstance may be seen
    // multiple times. We should only consider it once for removal, so we
    // ensure it is not added more than once.
    // Note that we cannot check whether this really is a NewInstance of String
    // before RTP. We DCHECK that in RemoveRedundantUninitializedStrings.
    if (!ContainsElement(uninitialized_strings_, string)) {
      uninitialized_strings_.push_back(string);
    }
  }

 private:
  void SetLoopHeaderPhiInputs();
  void FixEnvironmentPhis();
  void FixNullConstantType();
  void EquivalentPhisCleanup();
  void RunPrimitiveTypePropagation();

  // Attempts to resolve types of aget(-wide) instructions and type values passed
  // to aput(-wide) instructions from reference type information on the array
  // input. Returns false if the type of an array is unknown.
  bool FixAmbiguousArrayOps();

  bool TypeInputsOfPhi(HPhi* phi, ScopedArenaVector<HPhi*>* worklist);
  bool UpdatePrimitiveType(HPhi* phi, ScopedArenaVector<HPhi*>* worklist);
  void ProcessPrimitiveTypePropagationWorklist(ScopedArenaVector<HPhi*>* worklist);

  HFloatConstant* GetFloatEquivalent(HIntConstant* constant);
  HDoubleConstant* GetDoubleEquivalent(HLongConstant* constant);
  HPhi* GetFloatDoubleOrReferenceEquivalentOfPhi(HPhi* phi, DataType::Type type);
  HArrayGet* GetFloatOrDoubleEquivalentOfArrayGet(HArrayGet* aget);

  void RemoveRedundantUninitializedStrings();

  HGraph* const graph_;
  Handle<mirror::ClassLoader> class_loader_;
  Handle<mirror::DexCache> dex_cache_;
  VariableSizedHandleScope* const handles_;

  // True if types of ambiguous ArrayGets have been resolved.
  bool agets_fixed_;

  ScopedArenaAllocator* const local_allocator_;
  ScopedArenaVector<HArrayGet*> ambiguous_agets_;
  ScopedArenaVector<HArraySet*> ambiguous_asets_;
  ScopedArenaVector<HNewInstance*> uninitialized_strings_;

  DISALLOW_COPY_AND_ASSIGN(SsaBuilder);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_BUILDER_H_
