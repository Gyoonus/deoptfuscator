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

#include "pc_relative_fixups_x86.h"
#include "code_generator_x86.h"
#include "intrinsics_x86.h"

namespace art {
namespace x86 {

/**
 * Finds instructions that need the constant area base as an input.
 */
class PCRelativeHandlerVisitor : public HGraphVisitor {
 public:
  PCRelativeHandlerVisitor(HGraph* graph, CodeGenerator* codegen)
      : HGraphVisitor(graph),
        codegen_(down_cast<CodeGeneratorX86*>(codegen)),
        base_(nullptr) {}

  void MoveBaseIfNeeded() {
    if (base_ != nullptr) {
      // Bring the base closer to the first use (previously, it was in the
      // entry block) and relieve some pressure on the register allocator
      // while avoiding recalculation of the base in a loop.
      base_->MoveBeforeFirstUserAndOutOfLoops();
    }
  }

 private:
  void VisitAdd(HAdd* add) OVERRIDE {
    BinaryFP(add);
  }

  void VisitSub(HSub* sub) OVERRIDE {
    BinaryFP(sub);
  }

  void VisitMul(HMul* mul) OVERRIDE {
    BinaryFP(mul);
  }

  void VisitDiv(HDiv* div) OVERRIDE {
    BinaryFP(div);
  }

  void VisitCompare(HCompare* compare) OVERRIDE {
    BinaryFP(compare);
  }

  void VisitReturn(HReturn* ret) OVERRIDE {
    HConstant* value = ret->InputAt(0)->AsConstant();
    if ((value != nullptr && DataType::IsFloatingPointType(value->GetType()))) {
      ReplaceInput(ret, value, 0, true);
    }
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeVirtual(HInvokeVirtual* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitInvokeInterface(HInvokeInterface* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitLoadClass(HLoadClass* load_class) OVERRIDE {
    HLoadClass::LoadKind load_kind = load_class->GetLoadKind();
    if (load_kind == HLoadClass::LoadKind::kBootImageLinkTimePcRelative ||
        load_kind == HLoadClass::LoadKind::kBootImageClassTable ||
        load_kind == HLoadClass::LoadKind::kBssEntry) {
      HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(load_class);
      load_class->AddSpecialInput(method_address);
    }
  }

  void VisitLoadString(HLoadString* load_string) OVERRIDE {
    HLoadString::LoadKind load_kind = load_string->GetLoadKind();
    if (load_kind == HLoadString::LoadKind::kBootImageLinkTimePcRelative ||
        load_kind == HLoadString::LoadKind::kBootImageInternTable ||
        load_kind == HLoadString::LoadKind::kBssEntry) {
      HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(load_string);
      load_string->AddSpecialInput(method_address);
    }
  }

  void BinaryFP(HBinaryOperation* bin) {
    HConstant* rhs = bin->InputAt(1)->AsConstant();
    if (rhs != nullptr && DataType::IsFloatingPointType(rhs->GetType())) {
      ReplaceInput(bin, rhs, 1, false);
    }
  }

  void VisitEqual(HEqual* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitNotEqual(HNotEqual* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitLessThan(HLessThan* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitLessThanOrEqual(HLessThanOrEqual* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitGreaterThan(HGreaterThan* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitGreaterThanOrEqual(HGreaterThanOrEqual* cond) OVERRIDE {
    BinaryFP(cond);
  }

  void VisitNeg(HNeg* neg) OVERRIDE {
    if (DataType::IsFloatingPointType(neg->GetType())) {
      // We need to replace the HNeg with a HX86FPNeg in order to address the constant area.
      HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(neg);
      HGraph* graph = GetGraph();
      HBasicBlock* block = neg->GetBlock();
      HX86FPNeg* x86_fp_neg = new (graph->GetAllocator()) HX86FPNeg(
          neg->GetType(),
          neg->InputAt(0),
          method_address,
          neg->GetDexPc());
      block->ReplaceAndRemoveInstructionWith(neg, x86_fp_neg);
    }
  }

  void VisitPackedSwitch(HPackedSwitch* switch_insn) OVERRIDE {
    if (switch_insn->GetNumEntries() <=
        InstructionCodeGeneratorX86::kPackedSwitchJumpTableThreshold) {
      return;
    }
    // We need to replace the HPackedSwitch with a HX86PackedSwitch in order to
    // address the constant area.
    HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(switch_insn);
    HGraph* graph = GetGraph();
    HBasicBlock* block = switch_insn->GetBlock();
    HX86PackedSwitch* x86_switch = new (graph->GetAllocator()) HX86PackedSwitch(
        switch_insn->GetStartValue(),
        switch_insn->GetNumEntries(),
        switch_insn->InputAt(0),
        method_address,
        switch_insn->GetDexPc());
    block->ReplaceAndRemoveInstructionWith(switch_insn, x86_switch);
  }

  HX86ComputeBaseMethodAddress* GetPCRelativeBasePointer(HInstruction* cursor) {
    bool has_irreducible_loops = GetGraph()->HasIrreducibleLoops();
    if (!has_irreducible_loops) {
      // Ensure we only initialize the pointer once.
      if (base_ != nullptr) {
        return base_;
      }
    }
    // Insert the base at the start of the entry block, move it to a better
    // position later in MoveBaseIfNeeded().
    HX86ComputeBaseMethodAddress* method_address =
        new (GetGraph()->GetAllocator()) HX86ComputeBaseMethodAddress();
    if (has_irreducible_loops) {
      cursor->GetBlock()->InsertInstructionBefore(method_address, cursor);
    } else {
      HBasicBlock* entry_block = GetGraph()->GetEntryBlock();
      entry_block->InsertInstructionBefore(method_address, entry_block->GetFirstInstruction());
      base_ = method_address;
    }
    return method_address;
  }

  void ReplaceInput(HInstruction* insn, HConstant* value, int input_index, bool materialize) {
    HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(insn);
    HX86LoadFromConstantTable* load_constant =
        new (GetGraph()->GetAllocator()) HX86LoadFromConstantTable(method_address, value);
    if (!materialize) {
      load_constant->MarkEmittedAtUseSite();
    }
    insn->GetBlock()->InsertInstructionBefore(load_constant, insn);
    insn->ReplaceInput(load_constant, input_index);
  }

  void HandleInvoke(HInvoke* invoke) {
    // If this is an invoke-static/-direct with PC-relative dex cache array
    // addressing, we need the PC-relative address base.
    HInvokeStaticOrDirect* invoke_static_or_direct = invoke->AsInvokeStaticOrDirect();
    // We can't add a pointer to the constant area if we already have a current
    // method pointer. This may arise when sharpening doesn't remove the current
    // method pointer from the invoke.
    if (invoke_static_or_direct != nullptr &&
        invoke_static_or_direct->HasCurrentMethodInput()) {
      DCHECK(!invoke_static_or_direct->HasPcRelativeMethodLoadKind());
      return;
    }

    bool base_added = false;
    if (invoke_static_or_direct != nullptr &&
        invoke_static_or_direct->HasPcRelativeMethodLoadKind() &&
        !IsCallFreeIntrinsic<IntrinsicLocationsBuilderX86>(invoke, codegen_)) {
      HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(invoke);
      // Add the extra parameter.
      invoke_static_or_direct->AddSpecialInput(method_address);
      base_added = true;
    }

    // Ensure that we can load FP arguments from the constant area.
    HInputsRef inputs = invoke->GetInputs();
    for (size_t i = 0; i < inputs.size(); i++) {
      HConstant* input = inputs[i]->AsConstant();
      if (input != nullptr && DataType::IsFloatingPointType(input->GetType())) {
        ReplaceInput(invoke, input, i, true);
      }
    }

    // These intrinsics need the constant area.
    switch (invoke->GetIntrinsic()) {
      case Intrinsics::kMathAbsDouble:
      case Intrinsics::kMathAbsFloat:
      case Intrinsics::kMathMaxDoubleDouble:
      case Intrinsics::kMathMaxFloatFloat:
      case Intrinsics::kMathMinDoubleDouble:
      case Intrinsics::kMathMinFloatFloat:
      case Intrinsics::kMathRoundFloat:
        if (!base_added) {
          DCHECK(invoke_static_or_direct != nullptr);
          DCHECK(!invoke_static_or_direct->HasCurrentMethodInput());
          HX86ComputeBaseMethodAddress* method_address = GetPCRelativeBasePointer(invoke);
          invoke_static_or_direct->AddSpecialInput(method_address);
        }
        break;
      default:
        break;
    }
  }

  CodeGeneratorX86* codegen_;

  // The generated HX86ComputeBaseMethodAddress in the entry block needed as an
  // input to the HX86LoadFromConstantTable instructions. Only set for
  // graphs with reducible loops.
  HX86ComputeBaseMethodAddress* base_;
};

void PcRelativeFixups::Run() {
  PCRelativeHandlerVisitor visitor(graph_, codegen_);
  visitor.VisitInsertionOrder();
  visitor.MoveBaseIfNeeded();
}

}  // namespace x86
}  // namespace art
