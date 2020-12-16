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
import dexfuzz.rawdex.formats.ContainsVRegs;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class VRegChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int vregInsnIdx;
    public int mutatingVreg;
    public int newVregValue;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(vregInsnIdx).append(" ");
      builder.append(mutatingVreg).append(" ");
      builder.append(newVregValue);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      vregInsnIdx = Integer.parseInt(elements[2]);
      mutatingVreg = Integer.parseInt(elements[3]);
      newVregValue = Integer.parseInt(elements[4]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public VRegChanger() { }

  public VRegChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 60;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> vregInsns = null;

  private void generateCachedVRegInsns(MutatableCode mutatableCode) {
    if (vregInsns != null) {
      return;
    }

    vregInsns = new ArrayList<MInsn>();
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.format instanceof ContainsVRegs) {
        vregInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    if (mutatableCode.registersSize < 2) {
      Log.debug("Impossible to change vregs in a method with fewer than 2 registers.");
      return false;
    }

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.format instanceof ContainsVRegs) {
        return true;
      }
    }

    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedVRegInsns(mutatableCode);

    // Pick a random vreg instruction.
    int vregInsnIdx = rng.nextInt(vregInsns.size());
    MInsn vregInsn = vregInsns.get(vregInsnIdx);

    // Get the number of VRegs this instruction uses.
    int numVregs = ((ContainsVRegs)vregInsn.insn.info.format).getVRegCount();

    // Pick which vreg to mutate.
    int mutatingVreg = rng.nextInt(numVregs);

    // Find the old index.
    int oldVregValue = 0;

    switch (mutatingVreg) {
      case 0:
        oldVregValue = (int) vregInsn.insn.vregA;
        break;
      case 1:
        oldVregValue = (int) vregInsn.insn.vregB;
        break;
      case 2:
        oldVregValue = (int) vregInsn.insn.vregC;
        break;
      default:
        Log.errorAndQuit("Invalid number of vregs reported by a Format.");
    }

    // Search for a new vreg value.
    int newVregValue = oldVregValue;
    while (newVregValue == oldVregValue) {
      newVregValue = rng.nextInt(mutatableCode.registersSize);
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.vregInsnIdx = vregInsnIdx;
    mutation.mutatingVreg = mutatingVreg;
    mutation.newVregValue = newVregValue;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedVRegInsns(mutatableCode);

    MInsn vregInsn = vregInsns.get(mutation.vregInsnIdx);

    // Remember what the instruction used to look like.
    String oldInsnString = vregInsn.toString();

    int oldVregValue = 0;

    String vregId = "A";
    switch (mutation.mutatingVreg) {
      case 0:
        oldVregValue = (int) vregInsn.insn.vregA;
        vregInsn.insn.vregA = (long) mutation.newVregValue;
        break;
      case 1:
        oldVregValue = (int) vregInsn.insn.vregB;
        vregInsn.insn.vregB = (long) mutation.newVregValue;
        vregId = "B";
        break;
      case 2:
        oldVregValue = (int) vregInsn.insn.vregC;
        vregInsn.insn.vregC = (long) mutation.newVregValue;
        vregId = "C";
        break;
      default:
        Log.errorAndQuit("Invalid number of vregs specified in a VRegChanger mutation.");
    }

    Log.info("In " + oldInsnString + " changed v" + vregId + ": v" + oldVregValue
        + " to v" + mutation.newVregValue);

    stats.incrementStat("Changed a virtual register");

    // Clear cache.
    vregInsns = null;
  }
}
