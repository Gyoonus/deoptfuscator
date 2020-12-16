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
import dexfuzz.rawdex.Opcode;

import java.util.List;
import java.util.Random;

public class InstructionDuplicator extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int insnToDuplicateIdx;

    @Override
    public String getString() {
      return Integer.toString(insnToDuplicateIdx);
    }

    @Override
    public void parseString(String[] elements) {
      insnToDuplicateIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public InstructionDuplicator() { }

  public InstructionDuplicator(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 80;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    boolean foundInsn = false;
    int insnIdx = 0;

    while (!foundInsn) {
      // Pick an instruction at random...
      insnIdx = rng.nextInt(mutatableCode.getInstructionCount());
      MInsn oldInsn = mutatableCode.getInstructionAt(insnIdx);
      foundInsn = true;
      Opcode opcode = oldInsn.insn.info.opcode;
      // ...check it's a legal instruction to duplicate.
      if (opcode == Opcode.SPARSE_SWITCH || opcode == Opcode.PACKED_SWITCH
          || opcode == Opcode.FILL_ARRAY_DATA || oldInsn.insn.justRaw) {
        foundInsn = false;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.insnToDuplicateIdx = insnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MInsn oldInsn = mutatableCode.getInstructionAt(mutation.insnToDuplicateIdx);

    MInsn newInsn = oldInsn.clone();

    Log.info("Duplicating " + oldInsn);

    stats.incrementStat("Duplicated instruction");

    mutatableCode.insertInstructionAt(newInsn, mutation.insnToDuplicateIdx);
  }
}
