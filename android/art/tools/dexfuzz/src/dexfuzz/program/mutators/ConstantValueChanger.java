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
import dexfuzz.rawdex.formats.ContainsConst;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class ConstantValueChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int constInsnIdx;
    public long newConstant;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(constInsnIdx).append(" ");
      builder.append(newConstant);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      constInsnIdx = Integer.parseInt(elements[2]);
      newConstant = Long.parseLong(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public ConstantValueChanger() { }

  public ConstantValueChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 70;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> constInsns = null;

  private void generateCachedConstInsns(MutatableCode mutatableCode) {
    if (constInsns != null) {
      return;
    }

    constInsns = new ArrayList<MInsn>();
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.format instanceof ContainsConst) {
        constInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.format instanceof ContainsConst) {
        return true;
      }
    }

    Log.debug("Method contains no const instructions.");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedConstInsns(mutatableCode);

    // Pick a random const instruction.
    int constInsnIdx = rng.nextInt(constInsns.size());
    MInsn constInsn = constInsns.get(constInsnIdx);

    // Get the constant.
    long oldConstant = ((ContainsConst)constInsn.insn.info.format).getConst(constInsn.insn);

    long newConstant = oldConstant;

    // Make a new constant.
    while (newConstant == oldConstant) {
      newConstant = rng.nextLong()
          % ((ContainsConst)constInsn.insn.info.format).getConstRange();
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.constInsnIdx = constInsnIdx;
    mutation.newConstant = newConstant;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedConstInsns(mutatableCode);

    MInsn constInsn = constInsns.get(mutation.constInsnIdx);

    long oldConstant = ((ContainsConst)constInsn.insn.info.format).getConst(constInsn.insn);

    Log.info("Changed constant value #" + oldConstant + " to #" + mutation.newConstant
        + " in " + constInsn);

    stats.incrementStat("Changed constant value");

    // Set the new constant.
    ((ContainsConst)constInsn.insn.info.format).setConst(constInsn.insn, mutation.newConstant);

    // Clear cache.
    constInsns = null;
  }
}
