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
import dexfuzz.rawdex.OpcodeInfo;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class ArithOpChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int arithmeticInsnIdx;
    public int newOpcode;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(arithmeticInsnIdx).append(" ");
      builder.append(newOpcode);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      arithmeticInsnIdx = Integer.parseInt(elements[2]);
      newOpcode = Integer.parseInt(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public ArithOpChanger() { }

  public ArithOpChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 75;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> arithmeticInsns = null;

  private void generateCachedArithmeticInsns(MutatableCode mutatableCode) {
    if (arithmeticInsns != null) {
      return;
    }

    arithmeticInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isArithmeticOperation(mInsn)) {
        arithmeticInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isArithmeticOperation(mInsn)) {
        return true;
      }
    }

    Log.debug("No arithmetic operations in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedArithmeticInsns(mutatableCode);

    int arithmeticInsnIdx = rng.nextInt(arithmeticInsns.size());

    MInsn randomInsn = arithmeticInsns.get(arithmeticInsnIdx);

    OpcodeInfo oldOpcodeInfo = randomInsn.insn.info;

    OpcodeInfo newOpcodeInfo = oldOpcodeInfo;

    while (newOpcodeInfo.value == oldOpcodeInfo.value) {
      newOpcodeInfo = Instruction.getOpcodeInfo(getLegalDifferentOpcode(randomInsn));
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.arithmeticInsnIdx = arithmeticInsnIdx;
    mutation.newOpcode = newOpcodeInfo.value;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedArithmeticInsns(mutatableCode);

    MInsn randomInsn = arithmeticInsns.get(mutation.arithmeticInsnIdx);

    String oldInsnString = randomInsn.toString();

    OpcodeInfo newOpcodeInfo = Instruction.getOpcodeInfo(mutation.newOpcode);

    randomInsn.insn.info = newOpcodeInfo;

    Log.info("Changed " + oldInsnString + " to " + randomInsn);

    stats.incrementStat("Changed arithmetic opcode");

    // Clear the cache.
    arithmeticInsns = null;
  }

  private boolean isArithmeticOperation(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.ADD_INT, Opcode.USHR_INT_LIT8)) {
      return true;
    }
    return false;
  }

  private Opcode getLegalDifferentOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;

    for (List<Opcode> opcodeList : opcodeLists) {
      Opcode first = opcodeList.get(0);
      Opcode last = opcodeList.get(opcodeList.size() - 1);
      if (Opcode.isBetween(opcode, first, last)) {
        int newOpcodeIdx = rng.nextInt(opcodeList.size());
        return opcodeList.get(newOpcodeIdx);
      }
    }

    return opcode;
  }

  private static List<Opcode> intOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> int2addrOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> longOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> long2addrOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> floatOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> float2addrOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> doubleOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> double2addrOpcodes = new ArrayList<Opcode>();
  private static List<Opcode> intLit8Opcodes = new ArrayList<Opcode>();
  private static List<Opcode> intLit16Opcodes = new ArrayList<Opcode>();
  private static List<List<Opcode>> opcodeLists = new ArrayList<List<Opcode>>();

  static {
    intOpcodes.add(Opcode.ADD_INT);
    intOpcodes.add(Opcode.SUB_INT);
    intOpcodes.add(Opcode.MUL_INT);
    intOpcodes.add(Opcode.DIV_INT);
    intOpcodes.add(Opcode.REM_INT);
    intOpcodes.add(Opcode.AND_INT);
    intOpcodes.add(Opcode.OR_INT);
    intOpcodes.add(Opcode.XOR_INT);
    intOpcodes.add(Opcode.SHL_INT);
    intOpcodes.add(Opcode.SHR_INT);
    intOpcodes.add(Opcode.USHR_INT);

    int2addrOpcodes.add(Opcode.ADD_INT_2ADDR);
    int2addrOpcodes.add(Opcode.SUB_INT_2ADDR);
    int2addrOpcodes.add(Opcode.MUL_INT_2ADDR);
    int2addrOpcodes.add(Opcode.DIV_INT_2ADDR);
    int2addrOpcodes.add(Opcode.REM_INT_2ADDR);
    int2addrOpcodes.add(Opcode.AND_INT_2ADDR);
    int2addrOpcodes.add(Opcode.OR_INT_2ADDR);
    int2addrOpcodes.add(Opcode.XOR_INT_2ADDR);
    int2addrOpcodes.add(Opcode.SHL_INT_2ADDR);
    int2addrOpcodes.add(Opcode.SHR_INT_2ADDR);
    int2addrOpcodes.add(Opcode.USHR_INT_2ADDR);

    longOpcodes.add(Opcode.ADD_LONG);
    longOpcodes.add(Opcode.SUB_LONG);
    longOpcodes.add(Opcode.MUL_LONG);
    longOpcodes.add(Opcode.DIV_LONG);
    longOpcodes.add(Opcode.REM_LONG);
    longOpcodes.add(Opcode.AND_LONG);
    longOpcodes.add(Opcode.OR_LONG);
    longOpcodes.add(Opcode.XOR_LONG);
    longOpcodes.add(Opcode.SHL_LONG);
    longOpcodes.add(Opcode.SHR_LONG);
    longOpcodes.add(Opcode.USHR_LONG);

    long2addrOpcodes.add(Opcode.ADD_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.SUB_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.MUL_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.DIV_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.REM_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.AND_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.OR_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.XOR_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.SHL_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.SHR_LONG_2ADDR);
    long2addrOpcodes.add(Opcode.USHR_LONG_2ADDR);

    floatOpcodes.add(Opcode.ADD_FLOAT);
    floatOpcodes.add(Opcode.SUB_FLOAT);
    floatOpcodes.add(Opcode.MUL_FLOAT);
    floatOpcodes.add(Opcode.DIV_FLOAT);
    floatOpcodes.add(Opcode.REM_FLOAT);

    float2addrOpcodes.add(Opcode.ADD_FLOAT_2ADDR);
    float2addrOpcodes.add(Opcode.SUB_FLOAT_2ADDR);
    float2addrOpcodes.add(Opcode.MUL_FLOAT_2ADDR);
    float2addrOpcodes.add(Opcode.DIV_FLOAT_2ADDR);
    float2addrOpcodes.add(Opcode.REM_FLOAT_2ADDR);

    doubleOpcodes.add(Opcode.ADD_DOUBLE);
    doubleOpcodes.add(Opcode.SUB_DOUBLE);
    doubleOpcodes.add(Opcode.MUL_DOUBLE);
    doubleOpcodes.add(Opcode.DIV_DOUBLE);
    doubleOpcodes.add(Opcode.REM_DOUBLE);

    double2addrOpcodes.add(Opcode.ADD_DOUBLE_2ADDR);
    double2addrOpcodes.add(Opcode.SUB_DOUBLE_2ADDR);
    double2addrOpcodes.add(Opcode.MUL_DOUBLE_2ADDR);
    double2addrOpcodes.add(Opcode.DIV_DOUBLE_2ADDR);
    double2addrOpcodes.add(Opcode.REM_DOUBLE_2ADDR);

    intLit8Opcodes.add(Opcode.ADD_INT_LIT8);
    intLit8Opcodes.add(Opcode.RSUB_INT_LIT8);
    intLit8Opcodes.add(Opcode.MUL_INT_LIT8);
    intLit8Opcodes.add(Opcode.DIV_INT_LIT8);
    intLit8Opcodes.add(Opcode.REM_INT_LIT8);
    intLit8Opcodes.add(Opcode.AND_INT_LIT8);
    intLit8Opcodes.add(Opcode.OR_INT_LIT8);
    intLit8Opcodes.add(Opcode.XOR_INT_LIT8);
    intLit8Opcodes.add(Opcode.SHL_INT_LIT8);
    intLit8Opcodes.add(Opcode.SHR_INT_LIT8);
    intLit8Opcodes.add(Opcode.USHR_INT_LIT8);

    intLit16Opcodes.add(Opcode.ADD_INT_LIT16);
    intLit16Opcodes.add(Opcode.RSUB_INT);
    intLit16Opcodes.add(Opcode.MUL_INT_LIT16);
    intLit16Opcodes.add(Opcode.DIV_INT_LIT16);
    intLit16Opcodes.add(Opcode.REM_INT_LIT16);
    intLit16Opcodes.add(Opcode.AND_INT_LIT16);
    intLit16Opcodes.add(Opcode.OR_INT_LIT16);
    intLit16Opcodes.add(Opcode.XOR_INT_LIT16);

    opcodeLists.add(intOpcodes);
    opcodeLists.add(longOpcodes);
    opcodeLists.add(floatOpcodes);
    opcodeLists.add(doubleOpcodes);
    opcodeLists.add(int2addrOpcodes);
    opcodeLists.add(long2addrOpcodes);
    opcodeLists.add(float2addrOpcodes);
    opcodeLists.add(double2addrOpcodes);
    opcodeLists.add(intLit8Opcodes);
    opcodeLists.add(intLit16Opcodes);
  }
}
