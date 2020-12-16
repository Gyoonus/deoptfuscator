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

package dexfuzz.program;

import dexfuzz.Log;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * A class that represents a CodeItem in a way that is more amenable to mutation.
 */
public class MutatableCode {
  /**
   * To ensure we update the correct CodeItem in the raw DEX file.
   */
  public int codeItemIdx;

  /**
   * This is an index into the Program's list of MutatableCodes.
   */
  public int mutatableCodeIdx;

  /**
   * Number of registers this code uses.
   */
  public short registersSize;

  /**
   * Number of ins this code has.
   */
  public short insSize;

  /**
   * Number of outs this code has.
   */
  public short outsSize;

  /**
   * Number of tries this code has.
   */
  public short triesSize;

  /**
   * CodeTranslator is responsible for creating this, and
   * converting it back to a list of Instructions.
   */
  private List<MInsn> mutatableInsns;

  /**
   * CodeTranslator is responsible for creating this, and
   * converting it back to the correct form for CodeItems.
   */
  public List<MTryBlock> mutatableTries;

  /**
   * The name of the method this code represents.
   */
  public String name;
  public String shorty;
  public boolean isStatic;

  /**
   * The Program that owns this MutatableCode.
   * Currently used to get the size of constant pools for
   * PoolIndexChanger/RandomInstructionGenerator
   */
  public Program program;

  private short originalInVReg;
  private short tempVRegsAllocated;
  private short initialTempVReg;
  private boolean vregsNeedCopying;
  private int numMoveInsnsGenerated;

  public MutatableCode(Program program) {
    this.program = program;
    this.mutatableInsns = new LinkedList<MInsn>();
  }

  /**
   * Call this to update all instructions after the provided mInsn, to have their
   * locations adjusted by the provided offset. It will also mark that they have been updated.
   */
  public void updateInstructionLocationsAfter(MInsn mInsn, int offset) {
    boolean updating = false;
    for (MInsn mInsnChecking : mutatableInsns) {
      if (updating) {
        mInsnChecking.locationUpdated = true;
        mInsnChecking.location += offset;
      } else {
        if (mInsnChecking == mInsn) {
          updating = true;
        }
      }

    }
  }

  private void recalculateLocations() {
    int loc = 0;
    for (MInsn mInsn : mutatableInsns) {
      mInsn.location = loc;
      loc += mInsn.insn.getSize();
    }
  }

  public List<MInsn> getInstructions() {
    return Collections.unmodifiableList(mutatableInsns);
  }

  public int getInstructionCount() {
    return mutatableInsns.size();
  }

  public int getInstructionIndex(MInsn mInsn) {
    return mutatableInsns.indexOf(mInsn);
  }

  public MInsn getInstructionAt(int idx) {
    return mutatableInsns.get(idx);
  }

  public void addInstructionToEnd(MInsn mInsn) {
    mutatableInsns.add(mInsn);
  }

  public void insertInstructionAfter(MInsn toBeInserted, int insertionIdx) {
    if ((insertionIdx + 1) < mutatableInsns.size()) {
      insertInstructionAt(toBeInserted, insertionIdx + 1);
    } else {
      // Appending to end.
      MInsn finalInsn = mutatableInsns.get(mutatableInsns.size() - 1);
      toBeInserted.location = finalInsn.location + finalInsn.insn.getSize();
      mutatableInsns.add(toBeInserted);
    }
  }

  /**
   * Has same semantics as List.add()
   */
  public void insertInstructionAt(MInsn toBeInserted, int insertionIdx) {
    MInsn currentInsn = mutatableInsns.get(insertionIdx);
    toBeInserted.location = currentInsn.location;
    mutatableInsns.add(insertionIdx , toBeInserted);
    updateInstructionLocationsAfter(toBeInserted, toBeInserted.insn.getSize());
  }

  /**
   * Checks if any MTryBlock's instruction refs pointed at the 'before' MInsn,
   * and points them to the 'after' MInsn, if so. 'twoWay' will check if 'after'
   * was pointed to, and point refs to the 'before' insn.
   * (one-way is used when deleting instructions,
   * two-way is used when swapping instructions.)
   */
  private void updateTryBlocksWithReplacementInsn(MInsn before, MInsn after,
      boolean twoWay) {
    if (triesSize > 0) {
      for (MTryBlock mTryBlock : mutatableTries) {
        if (mTryBlock.startInsn == before) {
          Log.debug("Try block's first instruction was updated");
          mTryBlock.startInsn = after;
        } else if (twoWay && mTryBlock.startInsn == after) {
          Log.debug("Try block's first instruction was updated");
          mTryBlock.startInsn = before;
        }
        if (mTryBlock.endInsn == before) {
          Log.debug("Try block's last instruction was updated");
          mTryBlock.endInsn = after;
        } else if (twoWay && mTryBlock.endInsn == after) {
          Log.debug("Try block's last instruction was updated");
          mTryBlock.endInsn = before;
        }
        if (mTryBlock.catchAllHandler == before) {
          Log.debug("Try block's catch-all instruction was updated");
          mTryBlock.catchAllHandler = after;
        } else if (twoWay && mTryBlock.catchAllHandler == after) {
          Log.debug("Try block's catch-all instruction was updated");
          mTryBlock.catchAllHandler = before;
        }

        List<Integer> matchesIndicesToChange = new ArrayList<Integer>();
        List<Integer> replacementIndicesToChange = null;
        if (twoWay) {
          replacementIndicesToChange = new ArrayList<Integer>();
        }

        int idx = 0;
        for (MInsn handler : mTryBlock.handlers) {
          if (handler == before) {
            matchesIndicesToChange.add(idx);
            Log.debug("Try block's handler instruction was updated");
          } else if (twoWay && handler == after) {
            replacementIndicesToChange.add(idx);
            Log.debug("Try block's handler instruction was updated");
          }
          idx++;
        }

        for (int idxToChange : matchesIndicesToChange) {
          mTryBlock.handlers.set(idxToChange, after);
        }

        if (twoWay) {
          for (int idxToChange : replacementIndicesToChange) {
            mTryBlock.handlers.set(idxToChange, before);
          }
        }
      }
    }
  }

  /**
   * The actual implementation of deleteInstruction called by
   * the single-argument deleteInstructions.
   */
  private void deleteInstructionFull(MInsn toBeDeleted, int toBeDeletedIdx) {
    // Make sure we update all locations afterwards first.
    updateInstructionLocationsAfter(toBeDeleted, -(toBeDeleted.insn.getSize()));

    // Remove it.
    mutatableInsns.remove(toBeDeletedIdx);

    // Update any branch instructions that branched to the instruction we just deleted!

    // First, pick the replacement target.
    int replacementTargetIdx = toBeDeletedIdx;
    if (replacementTargetIdx == mutatableInsns.size()) {
      replacementTargetIdx--;
    }
    MInsn replacementTarget = mutatableInsns.get(replacementTargetIdx);

    for (MInsn mInsn : mutatableInsns) {
      if (mInsn instanceof MBranchInsn) {
        // Check if this branch insn points at the insn we just deleted.
        MBranchInsn branchInsn = (MBranchInsn) mInsn;
        MInsn target = branchInsn.target;
        if (target == toBeDeleted) {
          Log.debug(branchInsn + " was pointing at the deleted instruction, updated.");
          branchInsn.target = replacementTarget;
        }
      } else if (mInsn instanceof MSwitchInsn) {
        // Check if any of this switch insn's targets points at the insn we just deleted.
        MSwitchInsn switchInsn = (MSwitchInsn) mInsn;
        List<Integer> indicesToChange = new ArrayList<Integer>();
        int idx = 0;
        for (MInsn target : switchInsn.targets) {
          if (target == toBeDeleted) {
            indicesToChange.add(idx);
            Log.debug(switchInsn + "[" + idx
                + "] was pointing at the deleted instruction, updated.");
          }
          idx++;
        }
        for (int idxToChange : indicesToChange) {
          switchInsn.targets.remove(idxToChange);
          switchInsn.targets.add(idxToChange, replacementTarget);
        }
      }
    }

    // Now update the try blocks.
    updateTryBlocksWithReplacementInsn(toBeDeleted, replacementTarget, false);
  }

  /**
   * Delete the provided MInsn.
   */
  public void deleteInstruction(MInsn toBeDeleted) {
    deleteInstructionFull(toBeDeleted, mutatableInsns.indexOf(toBeDeleted));
  }

  /**
   * Delete the MInsn at the provided index.
   */
  public void deleteInstruction(int toBeDeletedIdx) {
    deleteInstructionFull(mutatableInsns.get(toBeDeletedIdx), toBeDeletedIdx);
  }

  public void swapInstructionsByIndex(int aIdx, int bIdx) {
    MInsn aInsn = mutatableInsns.get(aIdx);
    MInsn bInsn = mutatableInsns.get(bIdx);

    mutatableInsns.set(aIdx, bInsn);
    mutatableInsns.set(bIdx, aInsn);

    updateTryBlocksWithReplacementInsn(aInsn, bInsn, true);

    recalculateLocations();
  }

  /**
   * Some mutators may require the use of temporary registers. For instance,
   * to easily add in printing of values without having to look for registers
   * that aren't currently live.
   * The idea is to allocate these registers at the top of the set of registers.
   * Because this will then shift where the arguments to the method are, we then
   * change the start of the method to copy the arguments to the method
   * into the place where the rest of the method's code expects them to be.
   * Call allocateTemporaryVRegs(n), then use getTemporaryVReg(n),
   * and then make sure finishedUsingTemporaryVRegs() is called!
   */
  public void allocateTemporaryVRegs(int count) {
    if (count > tempVRegsAllocated) {
      if (tempVRegsAllocated == 0) {
        Log.info("Allocating temporary vregs for method...");
        initialTempVReg = registersSize;
        originalInVReg = (short) (registersSize - insSize);
      } else {
        Log.info("Extending allocation of temporary vregs for method...");
      }
      registersSize = (short) (initialTempVReg + count);
      if (outsSize < count) {
        outsSize = (short) count;
      }
      vregsNeedCopying = true;
      tempVRegsAllocated = (short) count;
    }
  }

  public int getTemporaryVReg(int number) {
    if (number >= tempVRegsAllocated) {
      Log.errorAndQuit("Not allocated enough temporary vregs!");
    }
    return initialTempVReg + number;
  }

  public void finishedUsingTemporaryVRegs() {
    if (tempVRegsAllocated > 0 && vregsNeedCopying) {
      // Just delete all the move instructions and generate again, if we already have some.
      while (numMoveInsnsGenerated > 0) {
        deleteInstruction(0);
        numMoveInsnsGenerated--;
      }

      Log.info("Moving 'in' vregs to correct locations after allocating temporary vregs");

      int shortyIdx = 0;
      if (isStatic) {
        shortyIdx = 1;
      }

      int insertionCounter = 0;

      // Insert copy insns that move all the in VRs down.
      for (int i = 0; i < insSize; i++) {
        MInsn moveInsn = new MInsn();
        moveInsn.insn = new Instruction();
        moveInsn.insn.vregA = originalInVReg + i;
        moveInsn.insn.vregB = originalInVReg + i + tempVRegsAllocated;

        char type = 'L';
        if (shortyIdx > 0) {
          type = shorty.charAt(shortyIdx);
        }
        shortyIdx++;

        if (type == 'L') {
          moveInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_OBJECT_16);
        } else if (type == 'D' || type == 'J') {
          moveInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_WIDE_16);
          i++;
        } else {
          moveInsn.insn.info = Instruction.getOpcodeInfo(Opcode.MOVE_16);
        }

        insertInstructionAt(moveInsn, insertionCounter);
        insertionCounter++;
        Log.info("Temp vregs creation, Added instruction " + moveInsn);
        numMoveInsnsGenerated++;
      }

      vregsNeedCopying = false;
    }
  }

  /**
   * When we insert new Field/Type/MethodIds into the DEX file, this may shunt some Ids
   * into a new position in the table. If this happens, every reference to the Ids must
   * be updated! Because CodeItems have their Instructions wrapped into a graph of MInsns
   * during mutation, they don't have a view of all their instructions during mutation,
   * and so if they are asked to update their instructions' indices into the tables, they
   * must call this method to get the actual list of instructions they currently own.
   */
  public List<Instruction> requestLatestInstructions() {
    List<Instruction> latestInsns = new ArrayList<Instruction>();
    for (MInsn mInsn : mutatableInsns) {
      latestInsns.add(mInsn.insn);
    }
    return latestInsns;
  }
}
