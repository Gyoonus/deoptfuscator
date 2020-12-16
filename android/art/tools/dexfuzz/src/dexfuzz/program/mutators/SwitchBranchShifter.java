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
import dexfuzz.program.MSwitchInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class SwitchBranchShifter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int switchInsnIdx;
    public int switchTargetIdx;
    public int newTargetIdx;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(switchInsnIdx).append(" ");
      builder.append(switchTargetIdx).append(" ");
      builder.append(newTargetIdx);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      switchInsnIdx = Integer.parseInt(elements[2]);
      switchTargetIdx = Integer.parseInt(elements[3]);
      newTargetIdx = Integer.parseInt(elements[4]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public SwitchBranchShifter() { }

  public SwitchBranchShifter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MSwitchInsn> switchInsns;

  private void generateCachedSwitchInsns(MutatableCode mutatableCode) {
    if (switchInsns != null) {
      return;
    }

    switchInsns = new ArrayList<MSwitchInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MSwitchInsn) {
        switchInsns.add((MSwitchInsn) mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MSwitchInsn) {
        return true;
      }
    }

    Log.debug("Method contains no switch instructions.");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedSwitchInsns(mutatableCode);

    // Pick a random switch instruction.
    int switchInsnIdx = rng.nextInt(switchInsns.size());
    MSwitchInsn switchInsn = switchInsns.get(switchInsnIdx);

    // Pick a random one of its targets.
    int switchTargetIdx = rng.nextInt(switchInsn.targets.size());

    // Get the original target, find its index.
    MInsn oldTargetInsn = switchInsn.targets.get(switchTargetIdx);
    int oldTargetInsnIdx = mutatableCode.getInstructionIndex(oldTargetInsn);

    int newTargetIdx = oldTargetInsnIdx;

    int delta = 0;

    // Keep searching for a new index.
    while (newTargetIdx == oldTargetInsnIdx) {
      // Vary by +/- 2 instructions.
      delta = 0;
      while (delta == 0) {
        delta = (rng.nextInt(5) - 2);
      }

      newTargetIdx = oldTargetInsnIdx + delta;

      // Check the new index is legal
      if (newTargetIdx < 0) {
        newTargetIdx = 0;
      } else if (newTargetIdx >= mutatableCode.getInstructionCount()) {
        newTargetIdx = mutatableCode.getInstructionCount() - 1;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.switchInsnIdx = switchInsnIdx;
    mutation.switchTargetIdx = switchTargetIdx;
    mutation.newTargetIdx = newTargetIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedSwitchInsns(mutatableCode);

    MSwitchInsn switchInsn = switchInsns.get(mutation.switchInsnIdx);

    // Get the new target.
    MInsn newTargetInsn =
        mutatableCode.getInstructionAt(mutation.newTargetIdx);

    // Set the new target.
    switchInsn.targets.remove(mutation.switchTargetIdx);
    switchInsn.targets.add(mutation.switchTargetIdx, newTargetInsn);

    Log.info("Shifted target #" + mutation.switchTargetIdx + " of " + switchInsn
        + " to point to " + newTargetInsn);

    stats.incrementStat("Shifted switch target");

    // Clear cache.
    switchInsns = null;
  }
}
