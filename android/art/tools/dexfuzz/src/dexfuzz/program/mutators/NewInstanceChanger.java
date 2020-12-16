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
import dexfuzz.rawdex.Opcode;
import dexfuzz.rawdex.formats.ContainsPoolIndex;
import dexfuzz.rawdex.formats.ContainsPoolIndex.PoolIndexKind;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Mutator NewInstanceChanger changes the new instance type in a method to
 * any random type from the pool.
 */
public class NewInstanceChanger extends CodeMutator {

  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int newInstanceToChangeIdx;
    public int newInstanceTypeIdx;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(newInstanceToChangeIdx).append(" ");
      builder.append(newInstanceTypeIdx);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      newInstanceToChangeIdx = Integer.parseInt(elements[2]);
      newInstanceTypeIdx = Integer.parseInt(elements[3]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NewInstanceChanger() {}

  public NewInstanceChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 10;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> newInstanceCachedInsns = null;

  private void generateCachedNewInstanceInsns(MutatableCode mutatableCode) {
    if (newInstanceCachedInsns != null) {
      return;
    }

    newInstanceCachedInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.opcode == Opcode.NEW_INSTANCE) {
        newInstanceCachedInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    // Cannot change the pool index with only one type.
    if (mutatableCode.program.getTotalPoolIndicesByKind(PoolIndexKind.Type) < 2) {
      Log.debug("Cannot mutate, only one type, skipping...");
      return false;
    }

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.opcode == Opcode.NEW_INSTANCE) {
        return true;
      }
    }
    Log.debug("No New Instance in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedNewInstanceInsns(mutatableCode);

    int newInstanceIdxInCache = rng.nextInt(newInstanceCachedInsns.size());
    MInsn newInstanceInsn = newInstanceCachedInsns.get(newInstanceIdxInCache);
    int oldTypeIdx = (int) newInstanceInsn.insn.vregB;
    int newTypeIdx = 0;
    int totalPoolIndices = mutatableCode.program.getTotalPoolIndicesByKind(PoolIndexKind.Type);
    if (totalPoolIndices < 2) {
      Log.errorAndQuit("Less than two types present, quitting...");
    }

    while (newTypeIdx == oldTypeIdx) {
      newTypeIdx = rng.nextInt(totalPoolIndices);
    }

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.newInstanceToChangeIdx = newInstanceIdxInCache;
    mutation.newInstanceTypeIdx = newTypeIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedNewInstanceInsns(mutatableCode);

    MInsn newInstanceInsn = newInstanceCachedInsns.get(mutation.newInstanceToChangeIdx);

    ContainsPoolIndex poolIndex = ((ContainsPoolIndex)newInstanceInsn.insn.info.format);

    poolIndex.setPoolIndex(newInstanceInsn.insn, mutation.newInstanceTypeIdx);

    Log.info("Changed the type of " + newInstanceInsn.toString() +
        " to " + mutation.newInstanceTypeIdx);

    int foundNewInstanceInsnIdx =
        foundInsnIdx(mutatableCode, newInstanceCachedInsns.get(mutation.newInstanceToChangeIdx));

    changeInvokeDirect(foundNewInstanceInsnIdx, mutation);

    stats.incrementStat("Changed new instance.");

    // Clear cache.
    newInstanceCachedInsns = null;
  }

  /**
   * Try to find the invoke-direct/ invoke-direct-range instruction that follows
   * the new instance instruction and change the method ID of the instruction.
   * @param foundInsnIdx
   * @param uncastMutation
   */
  protected void changeInvokeDirect(int foundInsnIdx, Mutation uncastMutation) {
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;
    if (foundInsnIdx == -1 ||
        foundInsnIdx + 1 == mutatableCode.getInstructionCount()) {
      return;
    }

    MInsn insn = mutatableCode.getInstructionAt(foundInsnIdx + 1);
    if (isInvokeInst(insn)) {
      ContainsPoolIndex poolIndex =((ContainsPoolIndex)insn.insn.info.format);
      long oldMethodIdx = poolIndex.getPoolIndex(insn.insn);
      String className = mutatableCode.program.getTypeString(mutation.newInstanceTypeIdx);
      String methodName = mutatableCode.program.getMethodString((int) oldMethodIdx);
      String shorty = mutatableCode.program.getMethodProto((int) oldMethodIdx);

      // Matches the type of the invoke with the randomly changed type of the prior new-instance.
      // This might create a lot of verification failures but still works many times.
      // TODO: Work on generating a program which finds a valid type.
      int methodId = mutatableCode.program.getNewItemCreator().
          findOrCreateMethodId(className, methodName, shorty);

      poolIndex.setPoolIndex(insn.insn, mutation.newInstanceTypeIdx);

      insn.insn.vregB = methodId;

      Log.info("Changed " + oldMethodIdx + " to " + methodId);
    }
  }

  protected boolean isInvokeInst(MInsn mInsn) {
    return (mInsn.insn.info.opcode == Opcode.INVOKE_DIRECT ||
        mInsn.insn.info.opcode == Opcode.INVOKE_DIRECT_RANGE);
  }

  // Check if there is an new instance instruction, and if found, return the index.
  // If not, return -1.
  protected int foundInsnIdx(MutatableCode mutatableCode, MInsn newInstanceInsn) {
    int i = 0;
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn == newInstanceInsn) {
        return i;
      }
      i++;
    }
    return -1;
  }
}
