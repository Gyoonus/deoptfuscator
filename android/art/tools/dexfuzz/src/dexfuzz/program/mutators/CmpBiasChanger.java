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
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class CmpBiasChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int cmpBiasInsnIdx;

    @Override
    public String getString() {
      return Integer.toString(cmpBiasInsnIdx);
    }

    @Override
    public void parseString(String[] elements) {
      cmpBiasInsnIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public CmpBiasChanger() { }

  public CmpBiasChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> cmpBiasInsns = null;

  private void generateCachedCmpBiasInsns(MutatableCode mutatableCode) {
    if (cmpBiasInsns != null) {
      return;
    }

    cmpBiasInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isCmpBiasOperation(mInsn)) {
        cmpBiasInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isCmpBiasOperation(mInsn)) {
        return true;
      }
    }

    Log.debug("No cmp-with-bias operations in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedCmpBiasInsns(mutatableCode);

    int cmpBiasInsnIdx = rng.nextInt(cmpBiasInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.cmpBiasInsnIdx = cmpBiasInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedCmpBiasInsns(mutatableCode);

    MInsn cmpBiasInsn = cmpBiasInsns.get(mutation.cmpBiasInsnIdx);

    String oldInsnString = cmpBiasInsn.toString();

    Opcode newOpcode = getLegalDifferentOpcode(cmpBiasInsn);

    cmpBiasInsn.insn.info = Instruction.getOpcodeInfo(newOpcode);

    Log.info("Changed " + oldInsnString + " to " + cmpBiasInsn);

    stats.incrementStat("Changed comparison bias");

    // Clear cache.
    cmpBiasInsns = null;
  }

  private Opcode getLegalDifferentOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (opcode == Opcode.CMPG_DOUBLE) {
      return Opcode.CMPL_DOUBLE;
    }
    if (opcode == Opcode.CMPL_DOUBLE) {
      return Opcode.CMPG_DOUBLE;
    }
    if (opcode == Opcode.CMPG_FLOAT) {
      return Opcode.CMPL_FLOAT;
    }
    return Opcode.CMPG_FLOAT;
  }

  private boolean isCmpBiasOperation(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.CMPL_FLOAT, Opcode.CMPG_DOUBLE)) {
      return true;
    }
    return false;
  }
}
