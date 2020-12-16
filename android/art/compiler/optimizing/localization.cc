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
#include "localization.h"
#include "art_field.h"
#include <iostream>
#include <algorithm>
#include "dex/modifiers.h"
namespace art {

// This visitor tries to simplify instructions that can be evaluated
// as constants.

class HLocalizationVisitor : public HLocalizationVisitor {
 public:
  explicit HLocalizationVisitor(HGraph* graph)
      : HGraphDelegateVisitor(graph) {}
  
  void AnalysisIfVectors();
  void AnalysisStaticFieldSetVectors();

 private:
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE;
  void VisitInstruction(HInstruction* inst) OVERRIDE;
  
  bool AnalysisVector(std::shared_ptr<HInstructionVector>& vec);
  
  std::shared_ptr<HInstructionVector> instruction_vector;
  std::vector<std::shared_ptr<HInstructionVector>> if_vector;
  std::vector<std::shared_ptr<HInstructionVector>> staticfieldset_vector;
  DISALLOW_COPY_AND_ASSIGN(HOpaqueIdentificationVisitor);
};

void HLocalization::Run() {
  HOpaqueIdentificationVisitor visitor(graph_);
  // Process basic blocks in reverse post-order in the dominator tree,
  // so that an instruction turned into a constant, used as input of
  // another instruction, may possibly be used to turn that second
  // instruction into a constant as well.

  visitor.VisitReversePostOrder();
  visitor.AnalysisIfVectors();
  visitor.AnalysisStaticFieldSetVectors();
  
}

void HOpaqueIdentificationVisitor::AnalysisStaticFieldSetVectors()
{
  for (auto it1 = staticfieldset_vector.begin(); it1 != staticfieldset_vector.end();)
  { 
    if(AnalysisVector(*it1)){
      std::stringstream str1;
      int flag = 0;
        str1<<"\t\t\t\t\t{\"sget\" : [";
      for (auto it2 = (*it1)->begin(); it2 != (*it1)->end(); ++it2)
      {
        if((*it2)->GetKind() == HInstruction::kStaticFieldSet)
        {
          str1 << ((HStaticFieldSet *)(*it2))->GetFieldInfo().GetFieldIndex();
          flag++;
        }
        else if((*it2)->GetKind() == HInstruction::kStaticFieldGet)
        {
          if( flag == 1)
          {
            str1 << "," << ((HStaticFieldGet *)(*it2))->GetFieldInfo().GetFieldIndex() << "]},\n";
            std::cout << str1.str();
            break;
          }
        }

      }
      ++it1;
    }
    else
    { 
      staticfieldset_vector.erase(it1);
    }
  }
  std::cout << "\t\t\t\t\t{}" <<std::endl;
}

void HOpaqueIdentificationVisitor::AnalysisIfVectors()
{
  for (auto it1 = if_vector.begin(); it1 != if_vector.end();)
  { 
    if(AnalysisVector(*it1)){
      for (auto it2 = (*it1)->begin(); it2 != (*it1)->end(); ++it2)
      {
        //std::cout << (*it2)->GetId() << ": " << (*it2)->DebugName()  << " pc: "<< std::hex <<(*it2)->GetDexPc() << std::endl;
        if((*it2)->GetKind() == HInstruction::kStaticFieldGet)
        {
          std::cout << "\t\t\t\t\t{\"if\" : " << ((HStaticFieldGet *)(*it2))->GetFieldInfo().GetFieldIndex() << "},\n";
          //std::cout << "\t\t[I]" << ((HStaticFieldGet *)(*it2))->GetFieldType() << std::endl;
        }
      }
      ++it1;
    }
    else
    { 
      if_vector.erase(it1);
    }
  }
}

bool HOpaqueIdentificationVisitor::AnalysisVector(std::shared_ptr<HInstructionVector>& vec)
{
  if(std::any_of(vec->begin(), vec->end(), [](HInstruction* i)
  {
    if(i->IsInvoke() || i->IsParameterValue())
    {
      return true;
    }
    return false;

  }))
  {
    return false;
  }


  else if(!std::any_of(vec->begin(), vec->end(), [](HInstruction* i) 
        {
        if(i->GetKind() == HInstruction::kStaticFieldGet)
        {
          DataType::Type type_ = reinterpret_cast<HStaticFieldGet*>(i)->GetFieldType();
          //if(type_ != DataType::Type::kReference && type_ != DataType::Type::kBool && type == DataType::Type::kInt32 )
          if(type_ == DataType::Type::kInt32 )
          {
            {
              ScopedObjectAccess soa(Thread::Current());
              if(!(reinterpret_cast<HStaticFieldGet *>(i)->GetFieldInfo().GetField()->GetAccessFlags() == (0x08 + 0x02))) //private + static
                return false;
            }
            auto inputs = i->GetInputRecords();
            for( auto input : inputs)
            {
              HInstruction* ins_ = input.GetInstruction();
              if(ins_->GetKind() == HInstruction::kLoadClass)
              {
                auto inputs_ = ins_->GetInputRecords();
                for (auto input_ : inputs_)
                {
                  if( input_.GetInstruction()->GetKind() == HInstruction::kCurrentMethod)
                    return true;
                }
              }

            }

          }
        
        }
        return false;
        }))
  {
    return false;
  }
  
  else{
    return true;
  }
}

void HOpaqueIdentificationVisitor::VisitBasicBlock(HBasicBlock* block) {
  // Traverse this block's instructions (phis don't need to be
  // processed) in (forward) order and replace the ones that can be
  // statically evaluated by a compile-time counterpart.
  if ( block->EndsWithIf())
  {
    instruction_vector.reset(new HInstructionVector);
    if_vector.push_back(instruction_vector);
    HInstruction* ins_ = reinterpret_cast<HBinaryOperation*>(block->GetLastInstruction()->GetInputRecords()[0].GetInstruction())->GetLeft();
    if(ins_->GetType() == DataType::Type::kInt32)
      block->GetLastInstruction()->Accept(this);
  }

  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (it.Current()->GetKind() == HInstruction::kStaticFieldSet)
    {
      instruction_vector.reset(new HInstructionVector);
      staticfieldset_vector.push_back(instruction_vector);
      it.Current()->Accept(this);
    }
  }
}

/*
void HOpaqueIdentificationVisitor::VisitIf(HIf* inst) {
  
  std::cout << inst->GetId() << ": " << inst->DebugName() << std::endl;
  instruction_list->AddInstruction(inst);
  for (const HUserRecord<HInstruction*>& input_: inst->GetInputRecords()) {
    HInstruction* input_instruction = input_.GetInstruction();
    input_instruction->Accept(this);
  }
  }
*/
void HOpaqueIdentificationVisitor::VisitInstruction(HInstruction* inst) {
  
  if(!std::any_of(instruction_vector->begin(), instruction_vector->end(), [&inst](HInstruction* i) {return inst == i;})){
    //std::cout << inst->GetId() << ": " << inst->DebugName() << std::endl;
    instruction_vector->push_back(inst);
    for (const HUserRecord<HInstruction*>& input_: inst->GetInputRecords()) {
      HInstruction* input_instruction = input_.GetInstruction();
      input_instruction->Accept(this);
    }
  }
}

}
