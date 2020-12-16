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

#include "pc_relative_fixups_mips.h"
#include "code_generator_mips.h"
#include "intrinsics_mips.h"

namespace art {
namespace mips {

/**
 * Finds instructions that need the constant area base as an input.
 */
class PCRelativeHandlerVisitor : public HGraphVisitor {
 public:
  PCRelativeHandlerVisitor(HGraph* graph, CodeGenerator* codegen)
      : HGraphVisitor(graph),
        codegen_(down_cast<CodeGeneratorMIPS*>(codegen)),
        base_(nullptr) {}

  void MoveBaseIfNeeded() {
    if (base_ != nullptr) {
      // Bring the base closer to the first use (previously, it was in the
      // entry block) and relieve some pressure on the register allocator
      // while avoiding recalculation of the base in a loop.
      base_->MoveBeforeFirstUserAndOutOfLoops();
      // Computing the base for PC-relative literals will clobber RA with
      // the NAL instruction on R2. Take a note of this before generating
      // the method entry.
      codegen_->ClobberRA();
    }
  }

 private:
  void InitializePCRelativeBasePointer() {
    // Ensure we only initialize the pointer once.
    if (base_ != nullptr) {
      return;
    }
    // Insert the base at the start of the entry block, move it to a better
    // position later in MoveBaseIfNeeded().
    base_ = new (GetGraph()->GetAllocator()) HMipsComputeBaseMethodAddress();
    HBasicBlock* entry_block = GetGraph()->GetEntryBlock();
    entry_block->InsertInstructionBefore(base_, entry_block->GetFirstInstruction());
    DCHECK(base_ != nullptr);
  }

  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) OVERRIDE {
    // If this is an invoke with PC-relative load kind,
    // we need to add the base as the special input.
    if (invoke->HasPcRelativeMethodLoadKind() &&
        !IsCallFreeIntrinsic<IntrinsicLocationsBuilderMIPS>(invoke, codegen_)) {
      InitializePCRelativeBasePointer();
      // Add the special argument base to the method.
      DCHECK(!invoke->HasCurrentMethodInput());
      invoke->AddSpecialInput(base_);
    }
  }

  void VisitLoadClass(HLoadClass* load_class) OVERRIDE {
    HLoadClass::LoadKind load_kind = load_class->GetLoadKind();
    switch (load_kind) {
      case HLoadClass::LoadKind::kBootImageLinkTimePcRelative:
      case HLoadClass::LoadKind::kBootImageAddress:
      case HLoadClass::LoadKind::kBootImageClassTable:
      case HLoadClass::LoadKind::kBssEntry:
        // Add a base register for PC-relative literals on R2.
        InitializePCRelativeBasePointer();
        load_class->AddSpecialInput(base_);
        break;
      default:
        break;
    }
  }

  void VisitLoadString(HLoadString* load_string) OVERRIDE {
    HLoadString::LoadKind load_kind = load_string->GetLoadKind();
    switch (load_kind) {
      case HLoadString::LoadKind::kBootImageLinkTimePcRelative:
      case HLoadString::LoadKind::kBootImageAddress:
      case HLoadString::LoadKind::kBootImageInternTable:
      case HLoadString::LoadKind::kBssEntry:
        // Add a base register for PC-relative literals on R2.
        InitializePCRelativeBasePointer();
        load_string->AddSpecialInput(base_);
        break;
      default:
        break;
    }
  }

  void VisitPackedSwitch(HPackedSwitch* switch_insn) OVERRIDE {
    if (switch_insn->GetNumEntries() <=
        InstructionCodeGeneratorMIPS::kPackedSwitchJumpTableThreshold) {
      return;
    }
    // We need to replace the HPackedSwitch with a HMipsPackedSwitch in order to
    // address the constant area.
    InitializePCRelativeBasePointer();
    HGraph* graph = GetGraph();
    HBasicBlock* block = switch_insn->GetBlock();
    HMipsPackedSwitch* mips_switch = new (graph->GetAllocator()) HMipsPackedSwitch(
        switch_insn->GetStartValue(),
        switch_insn->GetNumEntries(),
        switch_insn->InputAt(0),
        base_,
        switch_insn->GetDexPc());
    block->ReplaceAndRemoveInstructionWith(switch_insn, mips_switch);
  }

  CodeGeneratorMIPS* codegen_;

  // The generated HMipsComputeBaseMethodAddress in the entry block needed as an
  // input to the HMipsLoadFromConstantTable instructions.
  HMipsComputeBaseMethodAddress* base_;
};

void PcRelativeFixups::Run() {
  CodeGeneratorMIPS* mips_codegen = down_cast<CodeGeneratorMIPS*>(codegen_);
  if (mips_codegen->GetInstructionSetFeatures().IsR6()) {
    // Do nothing for R6 because it has PC-relative addressing.
    return;
  }
  if (graph_->HasIrreducibleLoops()) {
    // Do not run this optimization, as irreducible loops do not work with an instruction
    // that can be live-in at the irreducible loop header.
    return;
  }
  PCRelativeHandlerVisitor visitor(graph_, codegen_);
  visitor.VisitInsertionOrder();
  visitor.MoveBaseIfNeeded();
}

}  // namespace mips
}  // namespace art
