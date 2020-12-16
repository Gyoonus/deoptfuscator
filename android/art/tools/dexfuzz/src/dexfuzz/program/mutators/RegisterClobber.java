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

import java.util.List;
import java.util.Random;

public class RegisterClobber extends CodeMutator{

  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation{

    int regClobberIdx;

    @Override
    public String getString() {
      return Integer.toString(regClobberIdx);
    }

    @Override
    public void parseString(String[] elements) {
      Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public RegisterClobber() {}

  public RegisterClobber(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 10;
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    return mutatableCode.registersSize > 0;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    int insertionIdx = rng.nextInt(mutatableCode.getInstructionCount());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.regClobberIdx = insertionIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    int totalRegUsed = mutatableCode.registersSize;
    for (int i = 0; i < totalRegUsed; i++) {
      MInsn newInsn = new MInsn();
      newInsn.insn = new Instruction();
      newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.CONST_16);
      newInsn.insn.vregA = i;
      // Used zero because it may also apply to objects, resulting in fewer verification failures.
      newInsn.insn.vregB = 0;
      mutatableCode.insertInstructionAt(newInsn, mutation.regClobberIdx + i);
    }

    Log.info("Assigned zero to the registers from 0 to " + (totalRegUsed - 1) +
        " at index " + mutation.regClobberIdx);
    stats.incrementStat("Clobbered the registers");
  }
}