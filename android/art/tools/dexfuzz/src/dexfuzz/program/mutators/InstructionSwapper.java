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

import java.util.List;
import java.util.Random;

public class InstructionSwapper extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int swapInsnIdx;
    public int swapWithInsnIdx;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(swapInsnIdx).append(" ");
      builder.append(swapWithInsnIdx);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      swapInsnIdx = Integer.parseInt(elements[2]);
      swapWithInsnIdx = Integer.parseInt(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public InstructionSwapper() { }

  public InstructionSwapper(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 80;
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    if (mutatableCode.getInstructionCount() == 1) {
      // Cannot swap one instruction.
      Log.debug("Cannot swap insns in a method with only one.");
      return false;
    }
    return true;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    int swapInsnIdx = 0;
    int swapWithInsnIdx = 0;

    boolean foundFirstInsn = false;
    boolean foundSecondInsn = false;

    while (!foundFirstInsn || !foundSecondInsn) {
      // Look for the first insn.
      while (!foundFirstInsn) {
        swapInsnIdx = rng.nextInt(mutatableCode.getInstructionCount());
        MInsn toBeSwapped = mutatableCode.getInstructionAt(swapInsnIdx);
        foundFirstInsn = true;
        if (toBeSwapped.insn.justRaw) {
          foundFirstInsn = false;
        }
      }

      // Look for the second insn.
      int secondInsnAttempts = 0;
      while (!foundSecondInsn) {
        int delta = rng.nextInt(5) - 1;

        if (delta == 0) {
          continue;
        }

        swapWithInsnIdx = swapInsnIdx + delta;
        foundSecondInsn = true;

        // Check insn is in valid range.
        if (swapWithInsnIdx < 0) {
          foundSecondInsn = false;
        } else if (swapWithInsnIdx >= mutatableCode.getInstructionCount()) {
          foundSecondInsn = false;
        }

        // Finally, check if we're swapping with a raw insn.
        if (foundSecondInsn) {
          if (mutatableCode.getInstructionAt(swapWithInsnIdx).insn.justRaw) {
            foundSecondInsn = false;
          }
        }

        // If we've checked 10 times for an insn to swap with,
        // and still found nothing, then try a new first insn.
        if (!foundSecondInsn) {
          secondInsnAttempts++;
          if (secondInsnAttempts == 10) {
            foundFirstInsn = false;
            break;
          }
        }
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.swapInsnIdx = swapInsnIdx;
    mutation.swapWithInsnIdx = swapWithInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MInsn toBeSwapped = mutatableCode.getInstructionAt(mutation.swapInsnIdx);
    MInsn swappedWith = mutatableCode.getInstructionAt(mutation.swapWithInsnIdx);

    Log.info("Swapping " + toBeSwapped + " with " + swappedWith);

    stats.incrementStat("Swapped two instructions");

    mutatableCode.swapInstructionsByIndex(mutation.swapInsnIdx, mutation.swapWithInsnIdx);

    Log.info("Now " + swappedWith + " is swapped with " + toBeSwapped);
  }
}
