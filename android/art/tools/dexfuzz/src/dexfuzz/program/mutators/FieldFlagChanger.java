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
import dexfuzz.rawdex.EncodedField;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;
import dexfuzz.rawdex.formats.ContainsPoolIndex;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class FieldFlagChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int fieldInsnIdx;
    public boolean setVolatile;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(fieldInsnIdx).append(" ");
      builder.append(setVolatile);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      fieldInsnIdx = Integer.parseInt(elements[2]);
      setVolatile = Boolean.parseBoolean(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public FieldFlagChanger() { }

  public FieldFlagChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 40;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> fieldInsns = null;

  private void generateCachedFieldInsns(MutatableCode mutatableCode) {
    if (fieldInsns != null) {
      return;
    }

    fieldInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isFileDefinedFieldInstruction(mInsn, mutatableCode)) {
        fieldInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isFileDefinedFieldInstruction(mInsn, mutatableCode)) {
        return true;
      }
    }

    Log.debug("No field instructions in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedFieldInsns(mutatableCode);

    int fieldInsnIdx = rng.nextInt(fieldInsns.size());

    Instruction insn = fieldInsns.get(fieldInsnIdx).insn;
    ContainsPoolIndex containsPoolIndex = (ContainsPoolIndex) insn.info.format;
    int fieldIdx = containsPoolIndex.getPoolIndex(insn);
    EncodedField encodedField = mutatableCode.program.getEncodedField(fieldIdx);

    boolean setVolatile = false;
    if (!encodedField.isVolatile()) {
      setVolatile = true;
    }
    // TODO: Flip more flags?

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.fieldInsnIdx = fieldInsnIdx;
    mutation.setVolatile = setVolatile;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedFieldInsns(mutatableCode);

    Instruction insn = fieldInsns.get(mutation.fieldInsnIdx).insn;
    ContainsPoolIndex containsPoolIndex = (ContainsPoolIndex) insn.info.format;
    int fieldIdx = containsPoolIndex.getPoolIndex(insn);
    EncodedField encodedField = mutatableCode.program.getEncodedField(fieldIdx);

    if (mutation.setVolatile) {
      encodedField.setVolatile(true);
      Log.info("Set field idx " + fieldIdx + " as volatile");
    } else {
      encodedField.setVolatile(false);
      Log.info("Set field idx " + fieldIdx + " as not volatile");
    }

    stats.incrementStat("Changed volatility of field");

    // Clear cache.
    fieldInsns = null;
  }

  private boolean isFileDefinedFieldInstruction(MInsn mInsn, MutatableCode mutatableCode) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.IGET, Opcode.SPUT_SHORT)) {
      Instruction insn = mInsn.insn;
      ContainsPoolIndex containsPoolIndex = (ContainsPoolIndex) insn.info.format;
      int fieldIdx = containsPoolIndex.getPoolIndex(insn);
      if (mutatableCode.program.getEncodedField(fieldIdx) != null) {
        return true;
      }
    }
    return false;
  }
}
