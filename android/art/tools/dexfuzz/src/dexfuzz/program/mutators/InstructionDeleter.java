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
import dexfuzz.program.MInsnWithData;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;

import java.util.List;
import java.util.Random;

public class InstructionDeleter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int insnToDeleteIdx;

    @Override
    public String getString() {
      return Integer.toString(insnToDeleteIdx);
    }

    @Override
    public void parseString(String[] elements) {
      insnToDeleteIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public InstructionDeleter() { }

  public InstructionDeleter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 40;
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    if (mutatableCode.getInstructionCount() < 4) {
      // TODO: Make this more sophisticated - right now this is to avoid problems with
      // a method that has 3 instructions: fill-array-data; return-void; <data for fill-array-data>
      Log.debug("Cannot delete insns in a method with only a few.");
      return false;
    }
    return true;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Pick an instruction at random...
    int insnIdx = rng.nextInt(mutatableCode.getInstructionCount());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.insnToDeleteIdx = insnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MInsn toBeDeleted =
        mutatableCode.getInstructionAt(mutation.insnToDeleteIdx);

    Log.info("Deleting " + toBeDeleted);

    stats.incrementStat("Deleted instruction");

    // Delete the instruction.
    mutatableCode.deleteInstruction(mutation.insnToDeleteIdx);

    // If we delete a with-data insn, we should delete the associated data insn as well.
    if (toBeDeleted instanceof MInsnWithData) {
      Log.info(toBeDeleted + " had associated data, so the data insn was deleted.");
      // Get the data instruction.
      MInsn dataInsn =
          ((MInsnWithData)toBeDeleted).dataTarget;
      mutatableCode.deleteInstruction(dataInsn);
      stats.incrementStat("Deleted a with-data insn's data");
    }
    // If we delete a data insn, we should delete the associated with-data insn as well.
    if (toBeDeleted.insn.justRaw) {
      // .justRaw implies that this is a data insn.
      Log.info(toBeDeleted
          + " was data, so the associated with-data insn was deleted.");

      // First, find the with-data insn that is pointing to this insn.
      MInsn withDataInsn = null;
      for (MInsn mInsn : mutatableCode.getInstructions()) {
        if (mInsn instanceof MInsnWithData) {
          if (((MInsnWithData)mInsn).dataTarget == toBeDeleted) {
            withDataInsn = mInsn;
            break;
          }
        }
      }

      // Now delete the with-data insn.
      if (withDataInsn != null) {
        mutatableCode.deleteInstruction(withDataInsn);
        stats.incrementStat("Deleted a data insn's with-data insn");
      } else {
        Log.errorAndQuit("Tried to delete a data insn, "
            + "but it didn't have any with-data insn pointing at it?");
      }
    }
  }
}
