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
import dexfuzz.program.MTryBlock;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;

import java.util.List;
import java.util.Random;

public class TryBlockShifter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int tryIdx;
    public boolean shiftingTryBlock; // false => shifting handler
    public boolean shiftingStart; // false => shifting end (try block only)
    public boolean shiftingHandlerCatchall;
    public int shiftingHandlerIdx;
    public int newShiftedInsnIdx;

    @Override
    public String getString() {
      String result = String.format("%d %s %s %s %d %d",
          tryIdx,
          (shiftingTryBlock ? "T" : "F"),
          (shiftingStart ? "T" : "F"),
          (shiftingHandlerCatchall ? "T" : "F"),
          shiftingHandlerIdx,
          newShiftedInsnIdx
          );
      return result;
    }

    @Override
    public void parseString(String[] elements) {
      tryIdx = Integer.parseInt(elements[2]);
      shiftingTryBlock = elements[3].equals("T");
      shiftingStart = elements[4].equals("T");
      shiftingHandlerCatchall = elements[5].equals("T");
      shiftingHandlerIdx = Integer.parseInt(elements[6]);
      newShiftedInsnIdx = Integer.parseInt(elements[7]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public TryBlockShifter() { }

  public TryBlockShifter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 40;
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    if (mutatableCode.triesSize == 0) {
      Log.debug("Method contains no tries.");
      return false;
    }
    if (mutatableCode.getInstructionCount() <= 1) {
      Log.debug("Not enough instructions to shift try block.");
      return false;
    }
    return true;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Pick a random try.
    int tryIdx = rng.nextInt(mutatableCode.triesSize);
    MTryBlock tryBlock = mutatableCode.mutatableTries.get(tryIdx);

    boolean shiftingTryBlock = rng.nextBoolean();
    boolean shiftingStart = false;
    boolean shiftingHandlerCatchall = false;
    int shiftingHandlerIdx = -1;
    if (shiftingTryBlock) {
      // We're shifting the boundaries of the try block
      // determine if we shift the start or the end.
      shiftingStart = rng.nextBoolean();
    } else {
      // We're shifting the start of a handler of the try block.
      if (tryBlock.handlers.isEmpty()) {
        // No handlers, so we MUST mutate the catchall
        shiftingHandlerCatchall = true;
      } else if (tryBlock.catchAllHandler != null) {
        // There is a catchall handler, so potentially mutate it.
        shiftingHandlerCatchall = rng.nextBoolean();
      }
      // If we're not going to shift the catchall handler, then
      // pick an explicit handler to shift.
      if (!shiftingHandlerCatchall) {
        shiftingHandlerIdx = rng.nextInt(tryBlock.handlers.size());
      }
    }

    // Get the original instruction wherever we're shifting.
    MInsn oldInsn = null;
    if (shiftingTryBlock && shiftingStart) {
      oldInsn = tryBlock.startInsn;
    } else if (shiftingTryBlock && !(shiftingStart)) {
      oldInsn = tryBlock.endInsn;
    } else if (!(shiftingTryBlock) && shiftingHandlerCatchall) {
      oldInsn = tryBlock.catchAllHandler;
    } else if (!(shiftingTryBlock) && !(shiftingHandlerCatchall)
        && (shiftingHandlerIdx != -1)) {
      oldInsn = tryBlock.handlers.get(shiftingHandlerIdx);
    } else {
      Log.errorAndQuit("Faulty logic in TryBlockShifter!");
    }

    // Find the index of this instruction.
    int oldInsnIdx = mutatableCode.getInstructionIndex(oldInsn);

    int newInsnIdx = oldInsnIdx;

    int delta = 0;

    // Keep searching for a new index.
    while (newInsnIdx == oldInsnIdx) {
      // Vary by +/- 2 instructions.
      delta = 0;
      while (delta == 0) {
        delta = (rng.nextInt(5) - 2);
      }

      newInsnIdx = oldInsnIdx + delta;

      // Check the new index is legal.
      if (newInsnIdx < 0) {
        newInsnIdx = 0;
      } else if (newInsnIdx >= mutatableCode.getInstructionCount()) {
        newInsnIdx = mutatableCode.getInstructionCount() - 1;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.tryIdx = tryIdx;
    mutation.shiftingTryBlock = shiftingTryBlock;
    mutation.shiftingStart = shiftingStart;
    mutation.shiftingHandlerCatchall = shiftingHandlerCatchall;
    mutation.shiftingHandlerIdx = shiftingHandlerIdx;
    mutation.newShiftedInsnIdx = newInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MTryBlock tryBlock = mutatableCode.mutatableTries.get(mutation.tryIdx);

    MInsn newInsn =
        mutatableCode.getInstructionAt(mutation.newShiftedInsnIdx);

    // Find the right mutatable instruction in try block, and point it at the new instruction.
    if (mutation.shiftingTryBlock && mutation.shiftingStart) {
      tryBlock.startInsn = newInsn;
      Log.info("Shifted the start of try block #" + mutation.tryIdx
          + " to be at " + newInsn);
    } else if (mutation.shiftingTryBlock && !(mutation.shiftingStart)) {
      tryBlock.endInsn = newInsn;
      Log.info("Shifted the end of try block #" + mutation.tryIdx
          + " to be at " + newInsn);
    } else if (!(mutation.shiftingTryBlock) && mutation.shiftingHandlerCatchall) {
      tryBlock.catchAllHandler = newInsn;
      Log.info("Shifted the catch all handler of try block #" + mutation.tryIdx
          + " to be at " + newInsn);
    } else if (!(mutation.shiftingTryBlock) && !(mutation.shiftingHandlerCatchall)
        && (mutation.shiftingHandlerIdx != -1)) {
      tryBlock.handlers.set(mutation.shiftingHandlerIdx, newInsn);
      Log.info("Shifted handler #" + mutation.shiftingHandlerIdx
          + " of try block #" + mutation.tryIdx + " to be at " + newInsn);
    } else {
      Log.errorAndQuit("faulty logic in TryBlockShifter");
    }

    stats.incrementStat("Shifted boundary in a try block");
  }
}
