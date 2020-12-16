/*
 * Copyright (C) 2017 The Android Open Source Project
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
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class NewArrayLengthChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int newArrayToChangeIdx;

    @Override
    public String getString() {
      return Integer.toString(newArrayToChangeIdx);
    }

    @Override
    public void parseString(String[] elements) {
      newArrayToChangeIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NewArrayLengthChanger() { }

  public NewArrayLengthChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 50;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> newArrayLengthInsns = null;

  private void generateCachedArrayLengthInsns(MutatableCode mutatableCode) {
    if (newArrayLengthInsns != null) {
      return;
    }

    newArrayLengthInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isNewArray(mInsn)) {
        newArrayLengthInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      // TODO: Add filled-new-array and filled-new-array/range with their respective
      // positions of registers and also proper encoding.
      if (isNewArray(mInsn)) {
        return true;
      }
    }
    Log.debug("No New Array instruction in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedArrayLengthInsns(mutatableCode);

    int newArrayIdx = rng.nextInt(newArrayLengthInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.newArrayToChangeIdx = newArrayIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;
    MInsn newArrayInsn = newArrayLengthInsns.get(mutation.newArrayToChangeIdx);
    int newArrayInsnIdx = mutatableCode.getInstructionIndex(newArrayInsn);
    // If the original new-array instruction is no longer present
    // in the code (as indicated by a negative index), we make a
    // best effort to find any other new-array instruction to
    // apply the mutation to. If that effort fails, we simply
    // bail by doing nothing.
    if (newArrayInsnIdx < 0) {
      newArrayInsnIdx = scanNewArray(mutatableCode);
      if (newArrayInsnIdx == -1) {
        return;
      }
    }

    MInsn newInsn = new MInsn();
    newInsn.insn = new Instruction();
    newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.CONST_16);
    mutatableCode.allocateTemporaryVRegs(1);
    newArrayInsn.insn.vregB = mutatableCode.getTemporaryVReg(0);
    newInsn.insn.vregA = (int) newArrayInsn.insn.vregB;
    // New length chosen randomly between 1 to 100.
    newInsn.insn.vregB = rng.nextInt(100);
    mutatableCode.insertInstructionAt(newInsn, newArrayInsnIdx);
    Log.info("Changed the length of the array to " + newInsn.insn.vregB);
    stats.incrementStat("Changed length of new array");
    mutatableCode.finishedUsingTemporaryVRegs();
  }

  private boolean isNewArray(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    return opcode == Opcode.NEW_ARRAY;
  }

  // Return the index of first new-array in the method, -1 otherwise.
  private int scanNewArray(MutatableCode mutatableCode) {
    int idx = 0;
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isNewArray(mInsn)) {
        return idx;
      }
      idx++;
    }
    return -1;
  }
}