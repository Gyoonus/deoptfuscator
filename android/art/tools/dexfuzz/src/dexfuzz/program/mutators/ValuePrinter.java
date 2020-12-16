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

public class ValuePrinter extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int printedOutputIdx;

    @Override
    public String getString() {
      return Integer.toString(printedOutputIdx);
    }

    @Override
    public void parseString(String[] elements) {
      printedOutputIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public ValuePrinter() { }

  public ValuePrinter(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 40;
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (getInstructionOutputType(mInsn) != OutputType.UNKNOWN) {
        return true;
      }
    }

    Log.debug("No instructions with legible output in method, skipping.");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    // Find an instruction whose output we wish to print.
    int printedOutputIdx = 0;
    boolean foundInsn = false;

    while (!foundInsn) {
      printedOutputIdx = rng.nextInt(mutatableCode.getInstructionCount());
      MInsn insnOutputToPrint =
          mutatableCode.getInstructionAt(printedOutputIdx);
      foundInsn = true;

      // Don't want to insert instructions where there are raw instructions for now.
      if (insnOutputToPrint.insn.justRaw) {
        foundInsn = false;
      }

      if (getInstructionOutputType(insnOutputToPrint) == OutputType.UNKNOWN) {
        foundInsn = false;
      }
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.printedOutputIdx = printedOutputIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    MInsn insnOutputToPrint =
        mutatableCode.getInstructionAt(mutation.printedOutputIdx);

    int outFieldIdx = mutatableCode.program.getNewItemCreator().findOrCreateFieldId(
        "Ljava/lang/System;",
        "Ljava/io/PrintStream;",
        "out");

    OutputType outputType = getInstructionOutputType(insnOutputToPrint);

    if (outputType == OutputType.UNKNOWN) {
      Log.errorAndQuit("Requested to print output of an instruction, whose output"
          + " type is unknown.");
    }
    int printMethodIdx = mutatableCode.program.getNewItemCreator().findOrCreateMethodId(
        "Ljava/io/PrintStream;",
        "print",
        outputType.getSignatureForPrintln());

    boolean isWide = false;
    boolean isRef = false;
    if (outputType == OutputType.LONG || outputType == OutputType.DOUBLE) {
      isWide = true;
    }
    if (outputType == OutputType.STRING) {
      isRef = true;
    }

    // If we're printing a wide value, we need to allocate 3 registers!
    if (isWide) {
      mutatableCode.allocateTemporaryVRegs(3);
    } else {
      mutatableCode.allocateTemporaryVRegs(2);
    }

    int streamRegister = mutatableCode.getTemporaryVReg(0);
    int valueRegister = mutatableCode.getTemporaryVReg(1);

    // Copy the value we want to print to the 2nd temporary register
    // Then load the out stream
    // Then call print(out stream, value)

    MInsn valueCopyInsn = new MInsn();
    valueCopyInsn.insn = new Instruction();
    if (isRef) {
      valueCopyInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_OBJECT_16);
    } else if (isWide) {
      valueCopyInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_WIDE_16);
    } else {
      valueCopyInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_16);
    }
    valueCopyInsn.insn.vregB = insnOutputToPrint.insn.vregA;
    valueCopyInsn.insn.vregA = valueRegister;

    MInsn streamLoadInsn = new MInsn();
    streamLoadInsn.insn = new Instruction();
    streamLoadInsn.insn.info = Instruction.getOpcodeInfo(Opcode.SGET_OBJECT);
    streamLoadInsn.insn.vregB = outFieldIdx;
    streamLoadInsn.insn.vregA = streamRegister;

    MInsn invokeInsn = new MInsn();
    invokeInsn.insn = new Instruction();
    invokeInsn.insn.info = Instruction.getOpcodeInfo(Opcode.INVOKE_VIRTUAL_RANGE);
    if (isWide) {
      invokeInsn.insn.vregA = 3;
    } else {
      invokeInsn.insn.vregA = 2;
    }
    invokeInsn.insn.vregB = printMethodIdx;
    invokeInsn.insn.vregC = streamRegister;

    Log.info(String.format("Printing output value of instruction %s", insnOutputToPrint));

    stats.incrementStat("Printed output value");

    mutatableCode.insertInstructionAfter(invokeInsn, mutation.printedOutputIdx);
    mutatableCode.insertInstructionAfter(streamLoadInsn, mutation.printedOutputIdx);
    mutatableCode.insertInstructionAfter(valueCopyInsn, mutation.printedOutputIdx);

    mutatableCode.finishedUsingTemporaryVRegs();
  }

  private static enum OutputType {
    STRING("(Ljava/lang/String;)V"),
    BOOLEAN("(Z)V"),
    BYTE("(B)V"),
    CHAR("(C)V"),
    SHORT("(S)V"),
    INT("(I)V"),
    LONG("(J)V"),
    FLOAT("(F)V"),
    DOUBLE("(D)V"),
    UNKNOWN("UNKNOWN");

    private String printingSignature;
    private OutputType(String s) {
      printingSignature = s;
    }

    public String getSignatureForPrintln() {
      return printingSignature;
    }
  }

  private OutputType getInstructionOutputType(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (opcode == Opcode.CONST_STRING || opcode == Opcode.CONST_STRING_JUMBO) {
      return OutputType.STRING;
    }
    if (opcode == Opcode.IGET_BOOLEAN || opcode == Opcode.SGET_BOOLEAN) {
      return OutputType.BOOLEAN;
    }
    if (opcode == Opcode.IGET_BYTE || opcode == Opcode.SGET_BYTE
        || opcode == Opcode.INT_TO_BYTE) {
      return OutputType.BYTE;
    }
    if (opcode == Opcode.IGET_CHAR || opcode == Opcode.SGET_CHAR
        || opcode == Opcode.INT_TO_CHAR) {
      return OutputType.CHAR;
    }
    if (opcode == Opcode.IGET_SHORT || opcode == Opcode.SGET_SHORT
        || opcode == Opcode.INT_TO_SHORT) {
      return OutputType.SHORT;
    }
    if (opcode == Opcode.NEG_INT || opcode == Opcode.NOT_INT
        || opcode == Opcode.LONG_TO_INT || opcode == Opcode.FLOAT_TO_INT
        || opcode == Opcode.DOUBLE_TO_INT
        || Opcode.isBetween(opcode, Opcode.ADD_INT, Opcode.USHR_INT)
        || Opcode.isBetween(opcode, Opcode.ADD_INT_2ADDR, Opcode.USHR_INT_2ADDR)
        || Opcode.isBetween(opcode, Opcode.ADD_INT_LIT16, Opcode.USHR_INT_LIT8)) {
      return OutputType.INT;
    }
    if (opcode == Opcode.NEG_LONG || opcode == Opcode.NOT_LONG
        || opcode == Opcode.INT_TO_LONG || opcode == Opcode.FLOAT_TO_LONG
        || opcode == Opcode.DOUBLE_TO_LONG
        || Opcode.isBetween(opcode, Opcode.ADD_LONG, Opcode.USHR_LONG)
        || Opcode.isBetween(opcode, Opcode.ADD_LONG_2ADDR, Opcode.USHR_LONG_2ADDR)) {
      return OutputType.LONG;
    }
    if (opcode == Opcode.NEG_FLOAT
        || opcode == Opcode.INT_TO_FLOAT || opcode == Opcode.LONG_TO_FLOAT
        || opcode == Opcode.DOUBLE_TO_FLOAT
        || Opcode.isBetween(opcode, Opcode.ADD_FLOAT, Opcode.REM_FLOAT)
        || Opcode.isBetween(opcode, Opcode.ADD_FLOAT_2ADDR, Opcode.REM_FLOAT_2ADDR)) {
      return OutputType.FLOAT;
    }
    if (opcode == Opcode.NEG_DOUBLE
        || opcode == Opcode.INT_TO_DOUBLE || opcode == Opcode.LONG_TO_DOUBLE
        || opcode == Opcode.FLOAT_TO_DOUBLE
        || Opcode.isBetween(opcode, Opcode.ADD_DOUBLE, Opcode.REM_DOUBLE)
        || Opcode.isBetween(opcode, Opcode.ADD_DOUBLE_2ADDR, Opcode.REM_DOUBLE_2ADDR)) {
      return OutputType.DOUBLE;
    }
    return OutputType.UNKNOWN;
  }
}
