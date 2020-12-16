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

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class BranchShifter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int branchInsnIdx;
    public int newTargetIdx;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(branchInsnIdx).append(" ");
      builder.append(newTargetIdx);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      branchInsnIdx = Integer.parseInt(elements[2]);
      newTargetIdx = Integer.parseInt(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public BranchShifter() { }

  public BranchShifter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MBranchInsn> branchInsns;

  private void generateCachedBranchInsns(MutatableCode mutatableCode) {
    if (branchInsns != null) {
      return;
    }

    branchInsns = new ArrayList<MBranchInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MBranchInsn) {
        branchInsns.add((MBranchInsn) mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    // Can't shift a branch if there's only one instruction in the method.
    if (mutatableCode.getInstructionCount() == 1) {
      Log.debug("Method contains only one instruction, skipping.");
      return false;
    }
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MBranchInsn) {
        return true;
      }
    }

    Log.debug("Method contains no branch instructions.");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedBranchInsns(mutatableCode);

    // Pick a random branching instruction.
    int branchInsnIdx = rng.nextInt(branchInsns.size());
    MBranchInsn branchInsn = branchInsns.get(branchInsnIdx);

    // Get its original target, find its index.
    MInsn oldTargetInsn = branchInsn.target;
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

      // Check the new index is legal.
      if (newTargetIdx < 0) {
        newTargetIdx = 0;
      } else if (newTargetIdx >= mutatableCode.getInstructionCount()) {
        newTargetIdx = mutatableCode.getInstructionCount() - 1;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.branchInsnIdx = branchInsnIdx;
    mutation.newTargetIdx = newTargetIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedBranchInsns(mutatableCode);

    MBranchInsn branchInsn = branchInsns.get(mutation.branchInsnIdx);

    // Get the new target.
    MInsn newTargetInsn = mutatableCode.getInstructionAt(mutation.newTargetIdx);

    // Set the new target.
    branchInsn.target = newTargetInsn;

    Log.info("Shifted the target of " + branchInsn + " to point to " + newTargetInsn);

    stats.incrementStat("Shifted branch target");

    // Clear cache.
    branchInsns = null;
  }
}
