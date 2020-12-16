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

package dexfuzz.program.mutators;

import dexfuzz.Log;
import dexfuzz.MutationStats;
import dexfuzz.program.MBranchInsn;
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;
import dexfuzz.rawdex.OpcodeInfo;
import dexfuzz.rawdex.formats.AbstractFormat;
import dexfuzz.rawdex.formats.ContainsConst;
import dexfuzz.rawdex.formats.ContainsPoolIndex;
import dexfuzz.rawdex.formats.ContainsPoolIndex.PoolIndexKind;
import dexfuzz.rawdex.formats.ContainsVRegs;

import java.util.List;
import java.util.Random;

public class RandomInstructionGenerator extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int insertionIdx;
    public int newOpcode;
    public boolean hasConst;
    public long constValue;
    public boolean hasPoolIndex;
    public int poolIndexValue;
    public boolean hasVregs;
    public int vregCount;
    public int vregA;
    public int vregB;
    public int vregC;
    public int branchTargetIdx;

    @Override
    public String getString() {
      String result = String.format("%d %d %s %d %s %d %s %d %d %d %d %d",
          insertionIdx,
          newOpcode,
          (hasConst ? "T" : "F"),
          constValue,
          (hasPoolIndex ? "T" : "F"),
          poolIndexValue,
          (hasVregs ? "T" : "F"),
          vregCount,
          vregA,
          vregB,
          vregC,
          branchTargetIdx
          );
      return result;
    }

    @Override
    public void parseString(String[] elements) {
      insertionIdx = Integer.parseInt(elements[2]);
      newOpcode = Integer.parseInt(elements[3]);
      hasConst = (elements[4].equals("T"));
      constValue = Long.parseLong(elements[5]);
      hasPoolIndex = (elements[6].equals("T"));
      poolIndexValue = Integer.parseInt(elements[7]);
      hasVregs = (elements[8].equals("T"));
      vregCount = Integer.parseInt(elements[9]);
      vregA = Integer.parseInt(elements[10]);
      vregB = Integer.parseInt(elements[11]);
      vregC = Integer.parseInt(elements[12]);
      branchTargetIdx = Integer.parseInt(elements[13]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public RandomInstructionGenerator() { }

  public RandomInstructionGenerator(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Find the insertion point.
    int insertionIdx = 0;
    boolean foundInsn = false;

    while (!foundInsn) {
      insertionIdx = rng.nextInt(mutatableCode.getInstructionCount());
      MInsn insertionPoint =
          mutatableCode.getInstructionAt(insertionIdx);
      foundInsn = true;

      // Don't want to insert instructions where there are raw instructions for now.
      if (insertionPoint.insn.justRaw) {
        foundInsn = false;
      }
    }

    Opcode newOpcode = null;
    int opcodeCount = Opcode.values().length;
    boolean foundOpcode = false;

    while (!foundOpcode) {
      newOpcode = Opcode.values()[rng.nextInt(opcodeCount)];
      foundOpcode = true;
      if (Opcode.isBetween(newOpcode, Opcode.FILLED_NEW_ARRAY, Opcode.FILL_ARRAY_DATA)
          || Opcode.isBetween(newOpcode, Opcode.PACKED_SWITCH, Opcode.SPARSE_SWITCH)
          || Opcode.isBetween(newOpcode, Opcode.INVOKE_VIRTUAL, Opcode.INVOKE_INTERFACE)
          || Opcode.isBetween(newOpcode,
              Opcode.INVOKE_VIRTUAL_RANGE, Opcode.INVOKE_INTERFACE_RANGE)
              // Can never accept these instructions at compile time.
              || Opcode.isBetween(newOpcode, Opcode.IGET_QUICK, Opcode.IPUT_SHORT_QUICK)
              // Unused opcodes...
              || Opcode.isBetween(newOpcode, Opcode.UNUSED_3E, Opcode.UNUSED_43)
              || Opcode.isBetween(newOpcode, Opcode.UNUSED_79, Opcode.UNUSED_7A)
              || Opcode.isBetween(newOpcode, Opcode.UNUSED_EF, Opcode.UNUSED_FF)) {
        foundOpcode = false;
      }
    }

    OpcodeInfo newOpcodeInfo = Instruction.getOpcodeInfo(newOpcode);

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.insertionIdx = insertionIdx;
    mutation.newOpcode = newOpcodeInfo.value;

    AbstractFormat fmt = newOpcodeInfo.format;

    if (fmt instanceof ContainsConst) {
      mutation.hasConst = true;
      mutation.constValue = rng.nextLong() % ((ContainsConst)fmt).getConstRange();
    }
    if (fmt instanceof ContainsPoolIndex) {
      mutation.hasPoolIndex = true;
      ContainsPoolIndex containsPoolIndex = (ContainsPoolIndex) fmt;
      PoolIndexKind poolIndexKind = containsPoolIndex.getPoolIndexKind(newOpcodeInfo);
      int maxPoolIndex = mutatableCode.program.getTotalPoolIndicesByKind(poolIndexKind);
      if (maxPoolIndex > 0) {
        mutation.poolIndexValue = rng.nextInt(maxPoolIndex);
      } else {
        mutation.poolIndexValue = 0;
      }
    }
    if (mutatableCode.registersSize == 0) {
      mutatableCode.registersSize = 1;
    }
    if (fmt instanceof ContainsVRegs) {
      mutation.hasVregs = true;
      ContainsVRegs containsVregs = (ContainsVRegs) fmt;
      mutation.vregCount = containsVregs.getVRegCount();
      switch (mutation.vregCount) {
        case 3:
          mutation.vregC = rng.nextInt(mutatableCode.registersSize);
          // fallthrough
        case 2:
          mutation.vregB = rng.nextInt(mutatableCode.registersSize);
          // fallthrough
        case 1:
          mutation.vregA = rng.nextInt(mutatableCode.registersSize);
          break;
        default:
          Log.errorAndQuit("Invalid number of vregs specified.");
      }
    }
    // If we have some kind of branch, pick a random target.
    if (Opcode.isBetween(newOpcode, Opcode.IF_EQ, Opcode.IF_LEZ)
        || Opcode.isBetween(newOpcode, Opcode.GOTO, Opcode.GOTO_32)) {
      mutation.branchTargetIdx = rng.nextInt(mutatableCode.getInstructionCount());
    }

    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    Opcode newOpcode = Instruction.getOpcodeInfo(mutation.newOpcode).opcode;

    boolean isBranch = false;
    if (Opcode.isBetween(newOpcode, Opcode.IF_EQ, Opcode.IF_LEZ)
        || Opcode.isBetween(newOpcode, Opcode.GOTO, Opcode.GOTO_32)) {
      isBranch = true;
    }

    MInsn newInsn = null;
    if (!isBranch) {
      newInsn = new MInsn();
    } else {
      newInsn = new MBranchInsn();
    }
    newInsn.insn = new Instruction();
    newInsn.insn.info = Instruction.getOpcodeInfo(mutation.newOpcode);
    AbstractFormat fmt = newInsn.insn.info.format;

    if (mutation.hasConst) {
      ContainsConst containsConst = (ContainsConst) fmt;
      containsConst.setConst(newInsn.insn, mutation.constValue);
    }
    if (mutation.hasPoolIndex) {
      ContainsPoolIndex containsPoolIndex = (ContainsPoolIndex) fmt;
      containsPoolIndex.setPoolIndex(newInsn.insn, mutation.poolIndexValue);
    }
    if (mutation.hasVregs) {
      switch (mutation.vregCount) {
        case 3:
          newInsn.insn.vregC = mutation.vregC;
          // fallthrough
        case 2:
          newInsn.insn.vregB = mutation.vregB;
          // fallthrough
        case 1:
          newInsn.insn.vregA = mutation.vregA;
          break;
        default:
          Log.errorAndQuit("Invalid number of vregs specified.");
      }
    }

    if (isBranch) {
      // We have a branch instruction, point it at its target.
      MBranchInsn newBranchInsn = (MBranchInsn) newInsn;
      newBranchInsn.target = mutatableCode.getInstructionAt(mutation.branchTargetIdx);
    }

    MInsn insertionPoint =
        mutatableCode.getInstructionAt(mutation.insertionIdx);

    Log.info("Generated random instruction: " + newInsn
        + ", inserting at " + insertionPoint);

    stats.incrementStat("Generated random instruction");

    mutatableCode.insertInstructionAt(newInsn, mutation.insertionIdx);

    // If we've generated a monitor insn, generate the matching opposing insn.
    if (newInsn.insn.info.opcode == Opcode.MONITOR_ENTER) {
      MInsn exitInsn = newInsn.clone();
      exitInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MONITOR_EXIT);
      mutatableCode.insertInstructionAfter(exitInsn, mutation.insertionIdx);
      Log.info("Generated matching monitor-exit: " + exitInsn);
    } else if (newInsn.insn.info.opcode == Opcode.MONITOR_EXIT) {
      MInsn enterInsn = newInsn.clone();
      enterInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MONITOR_ENTER);
      mutatableCode.insertInstructionAt(enterInsn, mutation.insertionIdx);
      Log.info("Generated matching monitor-enter: " + enterInsn);
    }
  }
}
