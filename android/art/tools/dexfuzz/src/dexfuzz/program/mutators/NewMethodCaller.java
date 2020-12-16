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
import dexfuzz.rawdex.Instruction.InvokeFormatInfo;
import dexfuzz.rawdex.Opcode;

import java.util.List;
import java.util.Random;

public class NewMethodCaller extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public enum InvokeType {
      VIRTUAL,
      DIRECT,
      SUPER,
      STATIC,
      INTERFACE
    }

    public int insertionIdx;
    public InvokeType invokeType;
    public String className;
    public String methodName;
    public String signature;
    public int numArgs;
    public int[] args;

    @Override
    public String getString() {
      StringBuilder argsString = new StringBuilder();
      for (int i = 0; i < numArgs; i++) {
        argsString.append(args[i]);
        if (i < (numArgs - 1)) {
          argsString.append(" ");
        }
      }
      String result = String.format("%d %d %s %s %s %d %s",
          insertionIdx,
          invokeType.ordinal(),
          className,
          methodName,
          signature,
          numArgs,
          argsString);
      return result;
    }

    @Override
    public void parseString(String[] elements) {
      insertionIdx = Integer.parseInt(elements[2]);
      invokeType = InvokeType.values()[Integer.parseInt(elements[3])];
      className = elements[4];
      methodName = elements[5];
      signature = elements[6];
      numArgs = Integer.parseInt(elements[7]);
      args = new int[numArgs];
      for (int i = 0; i < numArgs; i++) {
        args[i] = Integer.parseInt(elements[8 + i]);
      }
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NewMethodCaller() { }

  public NewMethodCaller(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 10;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Find the insertion point.
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

    // TODO: Right now this mutator can only insert calls to System.gc() Add more!

    mutation.invokeType = AssociatedMutation.InvokeType.STATIC;
    mutation.className = "Ljava/lang/System;";
    mutation.methodName = "gc";
    mutation.signature = "()V";
    mutation.numArgs = 0;

    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MInsn newInsn = new MInsn();
    newInsn.insn = new Instruction();

    switch (mutation.invokeType) {
      case VIRTUAL:
        newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_VIRTUAL);
        break;
      case DIRECT:
        newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_DIRECT);
        break;
      case SUPER:
        newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_SUPER);
        break;
      case STATIC:
        newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_STATIC);
        break;
      case INTERFACE:
        newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_INTERFACE);
        break;
      default:
    }

    // TODO: Handle more than just static invokes.

    int methodIdx = mutatableCode.program.getNewItemCreator()
        .findOrCreateMethodId(mutation.className,
            mutation.methodName, mutation.signature);

    newInsn.insn.vregB = methodIdx;
    newInsn.insn.invokeFormatInfo = new InvokeFormatInfo();

    // TODO: More field population, when we call methods that take arguments.

    MInsn insertionPoint =
        mutatableCode.getInstructionAt(mutation.insertionIdx);

    Log.info(String.format("Called new method %s %s %s, inserting at %s",
        mutation.className, mutation.methodName, mutation.signature, insertionPoint));

    stats.incrementStat("Called new method");

    mutatableCode.insertInstructionAt(newInsn, mutation.insertionIdx);
  }
}
