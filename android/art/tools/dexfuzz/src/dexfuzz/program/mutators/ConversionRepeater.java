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

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class ConversionRepeater extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int conversionInsnIdx;

    @Override
    public String getString() {
      return Integer.toString(conversionInsnIdx);
    }

    @Override
    public void parseString(String[] elements) {
      conversionInsnIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public ConversionRepeater() { }

  public ConversionRepeater(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 50;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> conversionInsns = null;

  private void generateCachedConversionInsns(MutatableCode mutatableCode) {
    if (conversionInsns != null) {
      return;
    }

    conversionInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isConversionInstruction(mInsn)) {
        conversionInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isConversionInstruction(mInsn)) {
        return true;
      }
    }

    Log.debug("No conversion operations in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedConversionInsns(mutatableCode);
    int conversionInsnIdx = rng.nextInt(conversionInsns.size());
    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.conversionInsnIdx = conversionInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedConversionInsns(mutatableCode);

    MInsn originalInsn = conversionInsns.get(mutation.conversionInsnIdx);

    // We want to create two new instructions:
    // [original conversion] eg float-to-int
    // NEW: [there] eg int-to-float (with vregs of first inst swapped)
    // NEW: [back] eg float-to-int

    // Create the "there" instruction.
    MInsn newInsnThere = originalInsn.clone();

    // Flip the opcode.
    Opcode oppositeOpcode = null;
    switch (newInsnThere.insn.info.opcode) {
      case INT_TO_LONG:
        oppositeOpcode = Opcode.LONG_TO_INT;
        break;
      case INT_TO_FLOAT:
        oppositeOpcode = Opcode.FLOAT_TO_INT;
        break;
      case INT_TO_DOUBLE:
        oppositeOpcode = Opcode.DOUBLE_TO_INT;
        break;
      case LONG_TO_INT:
        oppositeOpcode = Opcode.INT_TO_LONG;
        break;
      case LONG_TO_FLOAT:
        oppositeOpcode = Opcode.FLOAT_TO_LONG;
        break;
      case LONG_TO_DOUBLE:
        oppositeOpcode = Opcode.DOUBLE_TO_LONG;
        break;
      case FLOAT_TO_INT:
        oppositeOpcode = Opcode.INT_TO_FLOAT;
        break;
      case FLOAT_TO_LONG:
        oppositeOpcode = Opcode.LONG_TO_FLOAT;
        break;
      case FLOAT_TO_DOUBLE:
        oppositeOpcode = Opcode.DOUBLE_TO_FLOAT;
        break;
      case DOUBLE_TO_INT:
        oppositeOpcode = Opcode.INT_TO_DOUBLE;
        break;
      case DOUBLE_TO_LONG:
        oppositeOpcode = Opcode.LONG_TO_DOUBLE;
        break;
      case DOUBLE_TO_FLOAT:
        oppositeOpcode = Opcode.FLOAT_TO_DOUBLE;
        break;
      default:
        Log.errorAndQuit(
            "Trying to repeat the conversion in an insn that is not a conversion insn.");
    }
    newInsnThere.insn.info = Instruction.getOpcodeInfo(oppositeOpcode);

    // Swap the vregs.
    long tempReg = newInsnThere.insn.vregA;
    newInsnThere.insn.vregA = newInsnThere.insn.vregB;
    newInsnThere.insn.vregB = tempReg;

    // Create the "back" instruction.
    MInsn newInsnBack = originalInsn.clone();

    // Get the index into the MutatableCode's mInsns list for this insn.
    int originalInsnIdx = mutatableCode.getInstructionIndex(originalInsn);

    // Insert the new instructions.
    mutatableCode.insertInstructionAfter(newInsnThere, originalInsnIdx);
    mutatableCode.insertInstructionAfter(newInsnBack, originalInsnIdx + 1);

    Log.info("Performing conversion repetition for " + originalInsn);

    stats.incrementStat("Repeating conversion");

    // Clear the cache.
    conversionInsns = null;
  }

  private boolean isConversionInstruction(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.INT_TO_LONG, Opcode.DOUBLE_TO_FLOAT)) {
      return true;
    }
    return false;
  }
}
