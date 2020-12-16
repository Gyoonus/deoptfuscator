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

import java.util.List;
import java.util.Random;

public class NonsenseStringPrinter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int insertionIdx;
    public String nonsenseString;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(insertionIdx).append(" ");
      builder.append(nonsenseString);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      insertionIdx = Integer.parseInt(elements[2]);
      nonsenseString = elements[3];
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NonsenseStringPrinter() { }

  public NonsenseStringPrinter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 10;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Find the insertion point
    int insertionIdx = 0;
    boolean foundInsn = false;

    while (!foundInsn) {
      insertionIdx = rng.nextInt(mutatableCode.getInstructionCount());
      MInsn insertionPoint =
          mutatableCode.getInstructionAt(insertionIdx);
      foundInsn = true;

      // Don't want to insert instructions where there are raw instructions for now.
      if (insertionPoint.insn.justRaw) {
        foundInsn = false;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.insertionIdx = insertionIdx;
    mutation.nonsenseString = getRandomString();
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    int outFieldIdx = mutatableCode.program.getNewItemCreator().findOrCreateFieldId(
        "Ljava/lang/System;",
        "Ljava/io/PrintStream;",
        "out");
    int printMethodIdx = mutatableCode.program.getNewItemCreator().findOrCreateMethodId(
        "Ljava/io/PrintStream;",
        "print",
        "(Ljava/lang/String;)V");
    int nonsenseStringIdx = mutatableCode.program.getNewItemCreator().findOrCreateString(
        mutation.nonsenseString);

    MInsn insertionPoint = mutatableCode.getInstructionAt(mutation.insertionIdx);

    mutatableCode.allocateTemporaryVRegs(2);

    int streamRegister = mutatableCode.getTemporaryVReg(0);
    int stringRegister = mutatableCode.getTemporaryVReg(1);

    // Load into string and stream into the temporary registers.
    // then call print(stream, string)
    MInsn constStringInsn = new MInsn();
    constStringInsn.insn = new Instruction();
    constStringInsn.insn.info = Instruction.getOpcodeInfo(Opcode.CONST_STRING);
    constStringInsn.insn.vregB = nonsenseStringIdx;
    constStringInsn.insn.vregA = stringRegister;

    MInsn streamLoadInsn = new MInsn();
    streamLoadInsn.insn = new Instruction();
    streamLoadInsn.insn.info = Instruction.getOpcodeInfo(Opcode.SGET_OBJECT);
    streamLoadInsn.insn.vregB = outFieldIdx;
    streamLoadInsn.insn.vregA = streamRegister;

    MInsn invokeInsn = new MInsn();
    invokeInsn.insn = new Instruction();
    invokeInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_VIRTUAL_RANGE);
    invokeInsn.insn.vregA = 2;
    invokeInsn.insn.vregB = printMethodIdx;
    invokeInsn.insn.vregC = streamRegister;

    Log.info(String.format("Printing nonsense string '%s', inserting at %s",
        mutation.nonsenseString, insertionPoint));

    stats.incrementStat("Printed nonsense string");

    mutatableCode.insertInstructionAt(invokeInsn, mutation.insertionIdx);
    mutatableCode.insertInstructionAt(streamLoadInsn, mutation.insertionIdx);
    mutatableCode.insertInstructionAt(constStringInsn, mutation.insertionIdx);

    mutatableCode.finishedUsingTemporaryVRegs();
  }

  private String getRandomString() {
    int size = rng.nextInt(10);
    int start = (int) 'A';
    int end = (int) 'Z';
    StringBuilder builder = new StringBuilder();
    for (int i = 0; i < size; i++) {
      builder.append((char) (rng.nextInt((end + 1) - start) + start));
    }
    return builder.toString();
  }
}
