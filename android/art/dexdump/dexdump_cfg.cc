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
 *
 * Implementation file for control flow graph dumping for the dexdump utility.
 */

#include "dexdump_cfg.h"

#include <inttypes.h>

#include <map>
#include <ostream>
#include <set>
#include <sstream>

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_exception_helpers.h"
#include "dex/dex_instruction-inl.h"

namespace art {

static void dumpMethodCFGImpl(const DexFile* dex_file,
                              uint32_t dex_method_idx,
                              const DexFile::CodeItem* code_item,
                              std::ostream& os) {
  os << "digraph {\n";
  os << "  # /* " << dex_file->PrettyMethod(dex_method_idx, true) << " */\n";

  CodeItemDataAccessor accessor(*dex_file, code_item);

  std::set<uint32_t> dex_pc_is_branch_target;
  {
    // Go and populate.
    for (const DexInstructionPcPair& pair : accessor) {
      const Instruction* inst = &pair.Inst();
      if (inst->IsBranch()) {
        dex_pc_is_branch_target.insert(pair.DexPc() + inst->GetTargetOffset());
      } else if (inst->IsSwitch()) {
        const uint16_t* insns = reinterpret_cast<const uint16_t*>(inst);
        int32_t switch_offset = insns[1] | (static_cast<int32_t>(insns[2]) << 16);
        const uint16_t* switch_insns = insns + switch_offset;
        uint32_t switch_count = switch_insns[1];
        int32_t targets_offset;
        if ((*insns & 0xff) == Instruction::PACKED_SWITCH) {
          /* 0=sig, 1=count, 2/3=firstKey */
          targets_offset = 4;
        } else {
          /* 0=sig, 1=count, 2..count*2 = keys */
          targets_offset = 2 + 2 * switch_count;
        }
        for (uint32_t targ = 0; targ < switch_count; targ++) {
          int32_t offset =
              static_cast<int32_t>(switch_insns[targets_offset + targ * 2]) |
              static_cast<int32_t>(switch_insns[targets_offset + targ * 2 + 1] << 16);
          dex_pc_is_branch_target.insert(pair.DexPc() + offset);
        }
      }
    }
  }

  // Create nodes for "basic blocks."
  std::map<uint32_t, uint32_t> dex_pc_to_node_id;  // This only has entries for block starts.
  std::map<uint32_t, uint32_t> dex_pc_to_incl_id;  // This has entries for all dex pcs.

  {
    bool first_in_block = true;
    bool force_new_block = false;
    for (const DexInstructionPcPair& pair : accessor) {
      const uint32_t dex_pc = pair.DexPc();
      if (dex_pc == 0 ||
          (dex_pc_is_branch_target.find(dex_pc) != dex_pc_is_branch_target.end()) ||
          force_new_block) {
        uint32_t id = dex_pc_to_node_id.size();
        if (id > 0) {
          // End last node.
          os << "}\"];\n";
        }
        // Start next node.
        os << "  node" << id << " [shape=record,label=\"{";
        dex_pc_to_node_id.insert(std::make_pair(dex_pc, id));
        first_in_block = true;
        force_new_block = false;
      }

      // Register instruction.
      dex_pc_to_incl_id.insert(std::make_pair(dex_pc, dex_pc_to_node_id.size() - 1));

      // Print instruction.
      if (!first_in_block) {
        os << " | ";
      } else {
        first_in_block = false;
      }

      // Dump the instruction. Need to escape '"', '<', '>', '{' and '}'.
      os << "<" << "p" << dex_pc << ">";
      os << " 0x" << std::hex << dex_pc << std::dec << ": ";
      std::string inst_str = pair.Inst().DumpString(dex_file);
      size_t cur_start = 0;  // It's OK to start at zero, instruction dumps don't start with chars
                             // we need to escape.
      while (cur_start != std::string::npos) {
        size_t next_escape = inst_str.find_first_of("\"{}<>", cur_start + 1);
        if (next_escape == std::string::npos) {
          os << inst_str.substr(cur_start, inst_str.size() - cur_start);
          break;
        } else {
          os << inst_str.substr(cur_start, next_escape - cur_start);
          // Escape all necessary characters.
          while (next_escape < inst_str.size()) {
            char c = inst_str.at(next_escape);
            if (c == '"' || c == '{' || c == '}' || c == '<' || c == '>') {
              os << '\\' << c;
            } else {
              break;
            }
            next_escape++;
          }
          if (next_escape >= inst_str.size()) {
            next_escape = std::string::npos;
          }
          cur_start = next_escape;
        }
      }

      // Force a new block for some fall-throughs and some instructions that terminate the "local"
      // control flow.
      force_new_block = pair.Inst().IsSwitch() || pair.Inst().IsBasicBlockEnd();
    }
    // Close last node.
    if (dex_pc_to_node_id.size() > 0) {
      os << "}\"];\n";
    }
  }

  // Create edges between them.
  {
    std::ostringstream regular_edges;
    std::ostringstream taken_edges;
    std::ostringstream exception_edges;

    // Common set of exception edges.
    std::set<uint32_t> exception_targets;

    // These blocks (given by the first dex pc) need exception per dex-pc handling in a second
    // pass. In the first pass we try and see whether we can use a common set of edges.
    std::set<uint32_t> blocks_with_detailed_exceptions;

    {
      uint32_t last_node_id = std::numeric_limits<uint32_t>::max();
      uint32_t old_dex_pc = 0;
      uint32_t block_start_dex_pc = std::numeric_limits<uint32_t>::max();
      for (const DexInstructionPcPair& pair : accessor) {
        const Instruction* inst = &pair.Inst();
        const uint32_t dex_pc = pair.DexPc();
        {
          auto it = dex_pc_to_node_id.find(dex_pc);
          if (it != dex_pc_to_node_id.end()) {
            if (!exception_targets.empty()) {
              // It seems the last block had common exception handlers. Add the exception edges now.
              uint32_t node_id = dex_pc_to_node_id.find(block_start_dex_pc)->second;
              for (uint32_t handler_pc : exception_targets) {
                auto node_id_it = dex_pc_to_incl_id.find(handler_pc);
                if (node_id_it != dex_pc_to_incl_id.end()) {
                  exception_edges << "  node" << node_id
                      << " -> node" << node_id_it->second << ":p" << handler_pc
                      << ";\n";
                }
              }
              exception_targets.clear();
            }

            block_start_dex_pc = dex_pc;

            // Seems to be a fall-through, connect to last_node_id. May be spurious edges for things
            // like switch data.
            uint32_t old_last = last_node_id;
            last_node_id = it->second;
            if (old_last != std::numeric_limits<uint32_t>::max()) {
              regular_edges << "  node" << old_last << ":p" << old_dex_pc
                  << " -> node" << last_node_id << ":p" << dex_pc
                  << ";\n";
            }
          }

          // Look at the exceptions of the first entry.
          CatchHandlerIterator catch_it(accessor, dex_pc);
          for (; catch_it.HasNext(); catch_it.Next()) {
            exception_targets.insert(catch_it.GetHandlerAddress());
          }
        }

        // Handle instruction.

        // Branch: something with at most two targets.
        if (inst->IsBranch()) {
          const int32_t offset = inst->GetTargetOffset();
          const bool conditional = !inst->IsUnconditional();

          auto target_it = dex_pc_to_node_id.find(dex_pc + offset);
          if (target_it != dex_pc_to_node_id.end()) {
            taken_edges << "  node" << last_node_id << ":p" << dex_pc
                << " -> node" << target_it->second << ":p" << (dex_pc + offset)
                << ";\n";
          }
          if (!conditional) {
            // No fall-through.
            last_node_id = std::numeric_limits<uint32_t>::max();
          }
        } else if (inst->IsSwitch()) {
          // TODO: Iterate through all switch targets.
          const uint16_t* insns = reinterpret_cast<const uint16_t*>(inst);
          /* make sure the start of the switch is in range */
          int32_t switch_offset = insns[1] | (static_cast<int32_t>(insns[2]) << 16);
          /* offset to switch table is a relative branch-style offset */
          const uint16_t* switch_insns = insns + switch_offset;
          uint32_t switch_count = switch_insns[1];
          int32_t targets_offset;
          if ((*insns & 0xff) == Instruction::PACKED_SWITCH) {
            /* 0=sig, 1=count, 2/3=firstKey */
            targets_offset = 4;
          } else {
            /* 0=sig, 1=count, 2..count*2 = keys */
            targets_offset = 2 + 2 * switch_count;
          }
          /* make sure the end of the switch is in range */
          /* verify each switch target */
          for (uint32_t targ = 0; targ < switch_count; targ++) {
            int32_t offset =
                static_cast<int32_t>(switch_insns[targets_offset + targ * 2]) |
                static_cast<int32_t>(switch_insns[targets_offset + targ * 2 + 1] << 16);
            int32_t abs_offset = dex_pc + offset;
            auto target_it = dex_pc_to_node_id.find(abs_offset);
            if (target_it != dex_pc_to_node_id.end()) {
              // TODO: value label.
              taken_edges << "  node" << last_node_id << ":p" << dex_pc
                  << " -> node" << target_it->second << ":p" << (abs_offset)
                  << ";\n";
            }
          }
        }

        // Exception edges. If this is not the first instruction in the block
        if (block_start_dex_pc != dex_pc) {
          std::set<uint32_t> current_handler_pcs;
          CatchHandlerIterator catch_it(accessor, dex_pc);
          for (; catch_it.HasNext(); catch_it.Next()) {
            current_handler_pcs.insert(catch_it.GetHandlerAddress());
          }
          if (current_handler_pcs != exception_targets) {
            exception_targets.clear();  // Clear so we don't do something at the end.
            blocks_with_detailed_exceptions.insert(block_start_dex_pc);
          }
        }

        if (inst->IsReturn() ||
            (inst->Opcode() == Instruction::THROW) ||
            (inst->IsBranch() && inst->IsUnconditional())) {
          // No fall-through.
          last_node_id = std::numeric_limits<uint32_t>::max();
        }
        old_dex_pc = pair.DexPc();
      }
      // Finish up the last block, if it had common exceptions.
      if (!exception_targets.empty()) {
        // It seems the last block had common exception handlers. Add the exception edges now.
        uint32_t node_id = dex_pc_to_node_id.find(block_start_dex_pc)->second;
        for (uint32_t handler_pc : exception_targets) {
          auto node_id_it = dex_pc_to_incl_id.find(handler_pc);
          if (node_id_it != dex_pc_to_incl_id.end()) {
            exception_edges << "  node" << node_id
                << " -> node" << node_id_it->second << ":p" << handler_pc
                << ";\n";
          }
        }
        exception_targets.clear();
      }
    }

    // Second pass for detailed exception blocks.
    // TODO
    // Exception edges. If this is not the first instruction in the block
    for (uint32_t dex_pc : blocks_with_detailed_exceptions) {
      const Instruction* inst = &accessor.InstructionAt(dex_pc);
      uint32_t this_node_id = dex_pc_to_incl_id.find(dex_pc)->second;
      while (true) {
        CatchHandlerIterator catch_it(accessor, dex_pc);
        if (catch_it.HasNext()) {
          std::set<uint32_t> handled_targets;
          for (; catch_it.HasNext(); catch_it.Next()) {
            uint32_t handler_pc = catch_it.GetHandlerAddress();
            auto it = handled_targets.find(handler_pc);
            if (it == handled_targets.end()) {
              auto node_id_it = dex_pc_to_incl_id.find(handler_pc);
              if (node_id_it != dex_pc_to_incl_id.end()) {
                exception_edges << "  node" << this_node_id << ":p" << dex_pc
                    << " -> node" << node_id_it->second << ":p" << handler_pc
                    << ";\n";
              }

              // Mark as done.
              handled_targets.insert(handler_pc);
            }
          }
        }
        if (inst->IsBasicBlockEnd()) {
          break;
        }

        // Loop update. Have a break-out if the next instruction is a branch target and thus in
        // another block.
        dex_pc += inst->SizeInCodeUnits();
        if (dex_pc >= accessor.InsnsSizeInCodeUnits()) {
          break;
        }
        if (dex_pc_to_node_id.find(dex_pc) != dex_pc_to_node_id.end()) {
          break;
        }
        inst = inst->Next();
      }
    }

    // Write out the sub-graphs to make edges styled.
    os << "\n";
    os << "  subgraph regular_edges {\n";
    os << "    edge [color=\"#000000\",weight=.3,len=3];\n\n";
    os << "    " << regular_edges.str() << "\n";
    os << "  }\n\n";

    os << "  subgraph taken_edges {\n";
    os << "    edge [color=\"#00FF00\",weight=.3,len=3];\n\n";
    os << "    " << taken_edges.str() << "\n";
    os << "  }\n\n";

    os << "  subgraph exception_edges {\n";
    os << "    edge [color=\"#FF0000\",weight=.3,len=3];\n\n";
    os << "    " << exception_edges.str() << "\n";
    os << "  }\n\n";
  }

  os << "}\n";
}

void DumpMethodCFG(const DexFile* dex_file, uint32_t dex_method_idx, std::ostream& os) {
  // This is painful, we need to find the code item. That means finding the class, and then
  // iterating the table.
  if (dex_method_idx >= dex_file->NumMethodIds()) {
    os << "Could not find method-idx.";
    return;
  }
  const DexFile::MethodId& method_id = dex_file->GetMethodId(dex_method_idx);

  const DexFile::ClassDef* class_def = dex_file->FindClassDef(method_id.class_idx_);
  if (class_def == nullptr) {
    os << "Could not find class-def.";
    return;
  }

  const uint8_t* class_data = dex_file->GetClassData(*class_def);
  if (class_data == nullptr) {
    os << "No class data.";
    return;
  }

  ClassDataItemIterator it(*dex_file, class_data);
  it.SkipAllFields();

  // Find method, and dump it.
  while (it.HasNextMethod()) {
    uint32_t method_idx = it.GetMemberIndex();
    if (method_idx == dex_method_idx) {
      dumpMethodCFGImpl(dex_file, dex_method_idx, it.GetMethodCodeItem(), os);
      return;
    }
    it.Next();
  }

  // Otherwise complain.
  os << "Something went wrong, didn't find the method in the class data.";
}

}  // namespace art
