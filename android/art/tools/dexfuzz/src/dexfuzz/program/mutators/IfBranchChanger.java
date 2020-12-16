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

/**
 * This class mutates the comparison operator of the if
 * statements by taking in a random instruction, checking whether
 * it is an if statement and, if so, changing the comparison
 * operator. The inheriting classes implement the way comparison
 * operator changes. For example, by choosing the opposite
 * comparison operator or by choosing a random comparison operator.
 */
public abstract class IfBranchChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int ifBranchInsnIdx;

    @Override
    public String getString() {
      return Integer.toString(ifBranchInsnIdx);
    }

    @Override
    public void parseString(String[] elements) {
      ifBranchInsnIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public IfBranchChanger() { }

  public IfBranchChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> ifBranchInsns = null;

  private void generateCachedifBranchInsns(MutatableCode mutatableCode) {
    if (ifBranchInsns != null) {
      return;
    }

    ifBranchInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isIfBranchOperation(mInsn)) {
        ifBranchInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isIfBranchOperation(mInsn)) {
        return true;
      }
    }

    Log.debug("No if branch operation, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedifBranchInsns(mutatableCode);

    int ifBranchInsnIdx = rng.nextInt(ifBranchInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.ifBranchInsnIdx = ifBranchInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedifBranchInsns(mutatableCode);

    MInsn ifBranchInsn = ifBranchInsns.get(mutation.ifBranchInsnIdx);

    String oldInsnString = ifBranchInsn.toString();

    Opcode newOpcode = getModifiedOpcode(ifBranchInsn);

    ifBranchInsn.insn.info = Instruction.getOpcodeInfo(newOpcode);

    Log.info("Changed " + oldInsnString + " to " + ifBranchInsn);

    stats.incrementStat("Changed if branch operator to " + getMutationTag() + " operator");

    // Clear cache.
    ifBranchInsns = null;
  }

  /**
   * Get a different if branch instruction.
   * @return opcode of the new comparison operator.
   */
  protected abstract Opcode getModifiedOpcode(MInsn mInsn);

  /**
   * Get the tag of the mutation that fired.
   * @return string tag of the type of mutation used
   */
  protected abstract String getMutationTag();

  private boolean isIfBranchOperation(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.IF_EQ, Opcode.IF_LEZ)) {
      return true;
    }
    return false;
  }
}