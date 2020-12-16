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
#include "scoped_thread_state_change-inl.h"
#include "opaque_location.h"
#include "art_field.h"
#include <iostream>
#include <algorithm>
#include "dex/modifiers.h"
#include "art_method.h"
#include "art_method-inl.h"


namespace art {

// This visitor tries to simplify instructions that can be evaluated
// as constants.

class HOpaqueLocationVisitor : public HGraphDelegateVisitor {
 public:
  explicit HOpaqueLocationVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph) {}
  
 private:
  uint32_t ref_1_, ref_2_;
  uint32_t code_off_;
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;
  void SetRef(uint32_t ref_1, uint32_t ref_2, uint32_t code_off);
  DISALLOW_COPY_AND_ASSIGN(HOpaqueLocationVisitor);
  friend class HOpaqueLocation;
};

void HOpaqueLocation::Run(uint32_t ref_1, uint32_t ref_2, uint32_t code_off) {
  ref_1_ = ref_1;
  ref_2_ = ref_2;
  code_off_ = code_off;
  Run();

}


void HOpaqueLocation::Run() {
  HOpaqueLocationVisitor visitor(graph_);
  visitor.SetRef(this->ref_1_, this->ref_2_, this->code_off_);
  // Process basic blocks in reverse post-order in the dominator tree,
  // so that an instruction turned into a constant, used as input of
  // another instruction, may possibly be used to turn that second
  // instruction into a constant as well.
  visitor.VisitReversePostOrder();
}
void HOpaqueLocationVisitor::SetRef(uint32_t ref_1, uint32_t ref_2, uint32_t code_off)
{
    ref_1_ = ref_1;
    ref_2_ = ref_2;
    code_off_ = code_off;
}
void HOpaqueLocationVisitor::VisitBasicBlock(HBasicBlock* block) {
  // Traverse this block's instructions (phis don't need to be
  // processed) in (forward) order and replace the ones that can be
  // statically evaluated by a compile-time counterpart.
  
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (it.Current()->GetKind() == HInstruction::kStaticFieldGet)
    {
        uint32_t ref_field = ((HStaticFieldGet *)it.Current())->GetFieldInfo().GetFieldIndex();
        if(ref_field == this->ref_1_ || ref_field == this->ref_2_)
        {
            uint32_t dex_pc = it.Current()->GetDexPc();
            std::cout << std::hex << code_off_ + dex_pc*2 + 16 << std::endl;
        }

    }
  }
}
}  // namespace art
