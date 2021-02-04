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
#include "opaque_clinit.h"
#include "art_field.h"
#include <iostream>
#include <algorithm>
#include "dex/modifiers.h"
#include "art_method.h"
#include "art_method-inl.h"


namespace art {

// This visitor tries to simplify instructions that can be evaluated
// as constants.

class HOpaqueClinitVisitor : public HGraphDelegateVisitor {
 public:
  explicit HOpaqueClinitVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph) {}
  
 private:
  uint32_t ref_1_, ref_2_;
  uint32_t code_off_;
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;
  void SetRef(uint32_t ref_1, uint32_t ref_2, uint32_t code_off);
  DISALLOW_COPY_AND_ASSIGN(HOpaqueClinitVisitor);
  friend class HOpaqueClinit;
};

void HOpaqueClinit::Run(uint32_t ref_1, uint32_t ref_2, uint32_t code_off) {
  ref_1_ = ref_1;
  ref_2_ = ref_2;
  code_off_ = code_off;
  Run();

}


void HOpaqueClinit::Run() {
  HOpaqueClinitVisitor visitor(graph_);
  visitor.SetRef(this->ref_1_, this->ref_2_, this->code_off_);
  // Process basic blocks in reverse post-order in the dominator tree,
  // so that an instruction turned into a constant, used as input of
  // another instruction, may possibly be used to turn that second
  // instruction into a constant as well.
  visitor.VisitReversePostOrder();
}
void HOpaqueClinitVisitor::SetRef(uint32_t ref_1, uint32_t ref_2, uint32_t code_off)
{
    ref_1_ = ref_1;
    ref_2_ = ref_2;
    code_off_ = code_off;
}
void HOpaqueClinitVisitor::VisitBasicBlock(HBasicBlock* block) {
  // Traverse this block's instructions (phis don't need to be
  // processed) in (forward) order and replace the ones that can be
  // statically evaluated by a compile-time counterpart.
  
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (it.Current()->GetKind() == HInstruction::kStaticFieldSet)
    {
        uint32_t ref_field = ((HStaticFieldSet *)it.Current())->GetFieldInfo().GetFieldIndex();
        if(ref_field == this->ref_1_ || ref_field == this->ref_2_)
        {
           for (const HUserRecord<HInstruction*>& input_: it.Current()->GetInputRecords()) {
              HInstruction* input_instruction = input_.GetInstruction();
              if (input_instruction -> GetKind() == HInstruction::kIntConstant)
                std::cout << ref_field << " : "<< ((HIntConstant *)input_instruction)->GetValue() << std::endl; 
              else
               std::cout << "No_Integer" << std::endl;
           }
        }

    }

  }
}
}  // namespace art
