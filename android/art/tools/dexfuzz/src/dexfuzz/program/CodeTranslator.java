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
import dexfuzz.rawdex.CodeItem;
import dexfuzz.rawdex.EncodedCatchHandler;
import dexfuzz.rawdex.EncodedTypeAddrPair;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;
import dexfuzz.rawdex.TryItem;
import dexfuzz.rawdex.formats.ContainsTarget;
import dexfuzz.rawdex.formats.RawInsnHelper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * Translates from a CodeItem (the raw list of Instructions) to MutatableCode
 * (graph of Instructions, using MInsns and subclasses) and vice-versa.
 */
public class CodeTranslator {

  /**
   * Given a raw DEX file's CodeItem, produce a MutatableCode object, that CodeMutators
   * are designed to operate on.
   * @param codeItemIdx Used to make sure the correct CodeItem is updated later after mutation.
   * @return A new MutatableCode object, which contains all relevant information
   *         obtained from the CodeItem.
   */
  public MutatableCode codeItemToMutatableCode(Program program, CodeItem codeItem,
      int codeItemIdx, int mutatableCodeIdx) {
    Log.debug("Translating CodeItem " + codeItemIdx
        + " (" + codeItem.meta.methodName + ") to MutatableCode");

    MutatableCode mutatableCode = new MutatableCode(program);

    codeItem.registerMutatableCode(mutatableCode);

    mutatableCode.name = codeItem.meta.methodName;
    mutatableCode.shorty = codeItem.meta.shorty;
    mutatableCode.isStatic = codeItem.meta.isStatic;

    mutatableCode.codeItemIdx = codeItemIdx;

    mutatableCode.mutatableCodeIdx = mutatableCodeIdx;

    mutatableCode.registersSize = codeItem.registersSize;
    mutatableCode.insSize = codeItem.insSize;
    mutatableCode.outsSize = codeItem.outsSize;
    mutatableCode.triesSize = codeItem.triesSize;

    // Temporary map from bytecode offset -> instruction.
    Map<Integer,MInsn> insnLocationMap = new HashMap<Integer,MInsn>();

    List<Instruction> inputInsns = codeItem.insns;

    // Create the MInsns.
    int loc = 0;
    for (Instruction insn : inputInsns) {
      MInsn mInsn = null;

      if (isInstructionSwitch(insn)) {
        mInsn = new MSwitchInsn();
      } else if (isInstructionBranch(insn)) {
        mInsn = new MBranchInsn();
      } else if (isInstructionFillArrayData(insn)) {
        mInsn = new MInsnWithData();
      } else {
        mInsn = new MInsn();
      }

      mInsn.insn = insn;

      // Populate the temporary map.
      insnLocationMap.put(loc, mInsn);

      // Populate the proper list of mutatable instructions.
      mutatableCode.addInstructionToEnd(mInsn);

      // Calculate the offsets for each instruction.
      mInsn.location = loc;
      mInsn.locationUpdated = false;

      loc += mInsn.insn.getSize();
    }

    // Now make branch/switch instructions point at the right target instructions.
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MSwitchInsn) {
        readSwitchInstruction((MSwitchInsn) mInsn, insnLocationMap);
      } else if (mInsn instanceof MInsnWithData) {
        ContainsTarget containsTarget = (ContainsTarget) mInsn.insn.info.format;
        int targetLoc = mInsn.location + (int) containsTarget.getTarget(mInsn.insn);
        ((MInsnWithData)mInsn).dataTarget = insnLocationMap.get(targetLoc);
        if (((MInsnWithData)mInsn).dataTarget == null) {
          Log.errorAndQuit("Bad offset calculation in data-target insn");
        }
      } else if (mInsn instanceof MBranchInsn) {
        ContainsTarget containsTarget = (ContainsTarget) mInsn.insn.info.format;
        int targetLoc = mInsn.location + (int) containsTarget.getTarget(mInsn.insn);
        ((MBranchInsn)mInsn).target = insnLocationMap.get(targetLoc);
        if (((MBranchInsn)mInsn).target == null) {
          Log.errorAndQuit("Bad offset calculation in branch insn");
        }
      }
    }

    // Now create try blocks.
    if (mutatableCode.triesSize > 0) {
      readTryBlocks(codeItem, mutatableCode, insnLocationMap);
    }

    return mutatableCode;
  }

  /**
   * Given a MutatableCode item that may have been mutated, update the original CodeItem
   * correctly, to allow valid DEX to be written back to the output file.
   */
  public void mutatableCodeToCodeItem(CodeItem codeItem, MutatableCode mutatableCode) {
    Log.debug("Translating MutatableCode " + mutatableCode.name
        + " to CodeItem " + mutatableCode.codeItemIdx);

    // We must first align any data instructions at the end of the code
    // before we recalculate any offsets.
    // This also updates their sizes...
    alignDataInstructions(mutatableCode);

    // Validate that the tracked locations for instructions are valid.
    // Also mark locations as no longer being updated.
    int loc = 0;
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.justRaw) {
        // All just_raw instructions need alignment!
        if ((loc % 2) != 0) {
          loc++;
        }
      }
      if (mInsn.location != loc) {
        Log.errorAndQuit(String.format("%s does not have expected location 0x%x",
            mInsn, loc));
      }
      mInsn.locationUpdated = false;
      loc += mInsn.insn.getSize();
    }

    // This new list will be attached to the CodeItem at the end...
    List<Instruction> outputInsns = new LinkedList<Instruction>();

    // Go through our new list of MInsns, adding them to the new
    // list of instructions that will be attached to the CodeItem.
    // Also recalculate offsets for branches.
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MSwitchInsn) {
        updateSwitchInstruction((MSwitchInsn)mInsn, mutatableCode);
      } else if (mInsn instanceof MInsnWithData) {
        MInsn target = ((MInsnWithData) mInsn).dataTarget;
        int dataOffset = target.location - mInsn.location;
        ContainsTarget containsTarget = (ContainsTarget) mInsn.insn.info.format;
        containsTarget.setTarget(mInsn.insn, dataOffset);
      } else if (mInsn instanceof MBranchInsn) {
        MInsn target = ((MBranchInsn) mInsn).target;
        int branchOffset = target.location - mInsn.location;
        ContainsTarget containsTarget = (ContainsTarget) mInsn.insn.info.format;
        containsTarget.setTarget(mInsn.insn, branchOffset);
      }
      outputInsns.add(mInsn.insn);
    }

    // Calculate the new insns_size.
    int newInsnsSize = 0;
    for (Instruction insn : outputInsns) {
      newInsnsSize += insn.getSize();
    }

    if (mutatableCode.triesSize > 0) {
      updateTryBlocks(codeItem, mutatableCode);
    }

    codeItem.insnsSize = newInsnsSize;
    codeItem.insns = outputInsns;
    codeItem.registersSize = mutatableCode.registersSize;
    codeItem.insSize = mutatableCode.insSize;
    codeItem.outsSize = mutatableCode.outsSize;
    codeItem.triesSize = mutatableCode.triesSize;
  }

  /**
   * The TryItem specifies an offset into the EncodedCatchHandlerList for a given CodeItem,
   * but we only have an array of the EncodedCatchHandlers that the List contains.
   * This function produces a map that offers a way to find out the index into our array,
   * from the try handler's offset.
   */
  private Map<Short,Integer> createTryHandlerOffsetToIndexMap(CodeItem codeItem) {
    // Create a sorted set of offsets.
    List<Short> uniqueOffsets = new ArrayList<Short>();
    for (TryItem tryItem : codeItem.tries) {
      int index = 0;
      while (true) {
        if ((index == uniqueOffsets.size())
            || (uniqueOffsets.get(index) > tryItem.handlerOff)) {
          // First condition means we're at the end of the set (or we're inserting
          //   into an empty set)
          // Second condition means that the offset belongs here
          // ...so insert it here, pushing the element currently in this position to the
          //    right, if it exists
          uniqueOffsets.add(index, tryItem.handlerOff);
          break;
        } else if (uniqueOffsets.get(index) == tryItem.handlerOff) {
          // We've already seen it, and we're making a set, not a list.
          break;
        } else {
          // Keep searching.
          index++;
        }
      }
    }
    // Now we have an (implicit) index -> offset mapping!

    // Now create the reverse mapping.
    Map<Short,Integer> offsetIndexMap = new HashMap<Short,Integer>();
    for (int i = 0; i < uniqueOffsets.size(); i++) {
      offsetIndexMap.put(uniqueOffsets.get(i), i);
    }

    return offsetIndexMap;
  }

  private void readTryBlocks(CodeItem codeItem, MutatableCode mutatableCode,
      Map<Integer,MInsn> insnLocationMap) {
    mutatableCode.mutatableTries = new LinkedList<MTryBlock>();

    Map<Short,Integer> offsetIndexMap = createTryHandlerOffsetToIndexMap(codeItem);

    // Read each TryItem into a MutatableTryBlock.
    for (TryItem tryItem : codeItem.tries) {
      MTryBlock mTryBlock = new MTryBlock();

      // Get the MInsns that form the start and end of the try block.
      int startLocation = tryItem.startAddr;
      mTryBlock.startInsn = insnLocationMap.get(startLocation);

      // The instructions vary in size, so we have to find the last instruction in the block in a
      // few tries.
      int endLocation = tryItem.startAddr + tryItem.insnCount - 1;
      mTryBlock.endInsn = insnLocationMap.get(endLocation);
      while ((mTryBlock.endInsn == null) && (endLocation >= startLocation)) {
        endLocation--;
        mTryBlock.endInsn = insnLocationMap.get(endLocation);
      }

      // Sanity checks.
      if (mTryBlock.startInsn == null) {
        Log.errorAndQuit(String.format(
            "Couldn't find a mutatable insn at start offset 0x%x",
            startLocation));
      }
      if (mTryBlock.endInsn == null) {
        Log.errorAndQuit(String.format(
            "Couldn't find a mutatable insn at end offset 0x%x",
            endLocation));
      }

      // Get the EncodedCatchHandler.
      int handlerIdx = offsetIndexMap.get(tryItem.handlerOff);
      EncodedCatchHandler encodedCatchHandler = codeItem.handlers.list[handlerIdx];

      // Do we have a catch all? If so, associate the MInsn that's there.
      if (encodedCatchHandler.size <= 0) {
        mTryBlock.catchAllHandler =
            insnLocationMap.get(encodedCatchHandler.catchAllAddr);
        // Sanity check.
        if (mTryBlock.catchAllHandler == null) {
          Log.errorAndQuit(
              String.format("Couldn't find a mutatable insn at catch-all offset 0x%x",
                  encodedCatchHandler.catchAllAddr));
        }
      }
      // Do we have explicitly-typed handlers? This will remain empty if not.
      mTryBlock.handlers = new LinkedList<MInsn>();

      // Associate all the explicitly-typed handlers.
      for (int i = 0; i < Math.abs(encodedCatchHandler.size); i++) {
        EncodedTypeAddrPair handler = encodedCatchHandler.handlers[i];
        MInsn handlerInsn = insnLocationMap.get(handler.addr);
        // Sanity check.
        if (handlerInsn == null) {
          Log.errorAndQuit(String.format(
              "Couldn't find a mutatable instruction at handler offset 0x%x",
              handler.addr));
        }
        mTryBlock.handlers.add(handlerInsn);
      }

      // Now finally add the new MutatableTryBlock into this MutatableCode's list!
      mutatableCode.mutatableTries.add(mTryBlock);
    }
  }

  private void updateTryBlocks(CodeItem codeItem, MutatableCode mutatableCode) {

    // TODO: Support ability to add extra try blocks/handlers?

    for (MTryBlock mTryBlock : mutatableCode.mutatableTries) {
      if (mTryBlock.startInsn.location > mTryBlock.endInsn.location) {
        // Mutation has put this try block's end insn before its start insn. Fix this.
        MInsn tempInsn = mTryBlock.startInsn;
        mTryBlock.startInsn = mTryBlock.endInsn;
        mTryBlock.endInsn = tempInsn;
      }
    }

    // First, manipulate the try blocks if they overlap.
    for (int i = 0; i < mutatableCode.mutatableTries.size() - 1; i++) {
      MTryBlock first = mutatableCode.mutatableTries.get(i);
      MTryBlock second = mutatableCode.mutatableTries.get(i + 1);

      // Do they overlap?
      if (first.endInsn.location > second.startInsn.location) {

        Log.debug("Found overlap in TryBlocks, moving 2nd TryBlock...");
        Log.debug("1st TryBlock goes from " + first.startInsn + " to "  + first.endInsn);
        Log.debug("2nd TryBlock goes from " + second.startInsn + " to "  + second.endInsn);

        // Find the first instruction that comes after that does not overlap
        // with the first try block.
        MInsn newInsn = second.startInsn;
        int ptr = mutatableCode.getInstructionIndex(newInsn);
        while (first.endInsn.location > newInsn.location) {
          ptr++;
          newInsn = mutatableCode.getInstructionAt(ptr);
        }
        second.startInsn = newInsn;

        Log.debug("Now 2nd TryBlock goes from " + second.startInsn + " to "  + second.endInsn);
      }
    }

    Map<Short,Integer> offsetIndexMap = createTryHandlerOffsetToIndexMap(codeItem);

    int tryItemIdx = 0;
    for (MTryBlock mTryBlock : mutatableCode.mutatableTries) {
      TryItem tryItem = codeItem.tries[tryItemIdx];

      tryItem.startAddr = mTryBlock.startInsn.location;
      int insnCount = mTryBlock.endInsn.location - mTryBlock.startInsn.location +
          mTryBlock.endInsn.insn.getSize();
      tryItem.insnCount = (short) insnCount;

      // Get the EncodedCatchHandler.
      EncodedCatchHandler encodedCatchHandler =
          codeItem.handlers.list[offsetIndexMap.get(tryItem.handlerOff)];

      if (encodedCatchHandler.size <= 0) {
        encodedCatchHandler.catchAllAddr = mTryBlock.catchAllHandler.location;
      }
      for (int i = 0; i < Math.abs(encodedCatchHandler.size); i++) {
        MInsn handlerInsn = mTryBlock.handlers.get(i);
        EncodedTypeAddrPair handler = encodedCatchHandler.handlers[i];
        handler.addr = handlerInsn.location;
      }
      tryItemIdx++;
    }
  }

  /**
   * Given a switch instruction, find the associated data's raw[] form, and update
   * the targets of the switch instruction to point to the correct instructions.
   */
  private void readSwitchInstruction(MSwitchInsn switchInsn,
      Map<Integer,MInsn> insnLocationMap) {
    // Find the data.
    ContainsTarget containsTarget = (ContainsTarget) switchInsn.insn.info.format;
    int dataLocation = switchInsn.location + (int) containsTarget.getTarget(switchInsn.insn);
    switchInsn.dataTarget = insnLocationMap.get(dataLocation);
    if (switchInsn.dataTarget == null) {
      Log.errorAndQuit("Bad offset calculation for data target in switch insn");
    }

    // Now read the data.
    Instruction dataInsn = switchInsn.dataTarget.insn;

    int rawPtr = 2;

    int targetsSize = (int) RawInsnHelper.getUnsignedShortFromTwoBytes(dataInsn.rawBytes, rawPtr);
    rawPtr += 2;

    int[] keys = new int[targetsSize];
    int[] targets = new int[targetsSize];

    if (dataInsn.rawType == 1) {
      switchInsn.packed = true;
      // Dealing with a packed-switch.
      // Read the first key.
      keys[0] = (int) RawInsnHelper.getUnsignedIntFromFourBytes(dataInsn.rawBytes, rawPtr);
      rawPtr += 4;
      // Calculate the rest of the keys.
      for (int i = 1; i < targetsSize; i++) {
        keys[i] = keys[i - 1] + 1;
      }
    } else if (dataInsn.rawType == 2) {
      // Dealing with a sparse-switch.
      // Read all of the keys.
      for (int i = 0; i < targetsSize; i++) {
        keys[i] = (int) RawInsnHelper.getUnsignedIntFromFourBytes(dataInsn.rawBytes,
            rawPtr);
        rawPtr += 4;
      }
    }

    // Now read the targets.
    for (int i = 0; i < targetsSize; i++) {
      targets[i] = (int) RawInsnHelper.getUnsignedIntFromFourBytes(dataInsn.rawBytes,
          rawPtr);
      rawPtr += 4;
    }

    // Store the keys.
    switchInsn.keys = keys;

    // Convert our targets[] offsets into pointers to MInsns.
    for (int target : targets) {
      int targetLocation = switchInsn.location + target;
      MInsn targetInsn = insnLocationMap.get(targetLocation);
      switchInsn.targets.add(targetInsn);
      if (targetInsn == null) {
        Log.errorAndQuit("Bad offset calculation for target in switch insn");
      }
    }
  }

  /**
   * Given a mutatable switch instruction, which may have had some of its branch
   * targets moved, update all the target offsets in the raw[] form of the instruction.
   */
  private void updateSwitchInstruction(MSwitchInsn switchInsn, MutatableCode mutatableCode) {
    // Update the offset to the data instruction
    MInsn dataTarget = switchInsn.dataTarget;
    int dataOffset = dataTarget.location - switchInsn.location;
    ContainsTarget containsTarget = (ContainsTarget) switchInsn.insn.info.format;
    containsTarget.setTarget(switchInsn.insn, dataOffset);

    int targetsSize = switchInsn.targets.size();

    int[] keys = switchInsn.keys;
    int[] targets = new int[targetsSize];

    // Calculate the new offsets.
    int targetIdx = 0;
    for (MInsn target : switchInsn.targets) {
      targets[targetIdx] = target.location - switchInsn.location;
      targetIdx++;
    }

    // Now write the data back to the raw bytes.
    Instruction dataInsn = switchInsn.dataTarget.insn;

    int rawPtr = 2;

    // Write out the size.
    RawInsnHelper.writeUnsignedShortToTwoBytes(dataInsn.rawBytes, rawPtr, targetsSize);
    rawPtr += 2;

    // Write out the keys.
    if (switchInsn.packed) {
      // Only write out one key - the first.
      RawInsnHelper.writeUnsignedIntToFourBytes(dataInsn.rawBytes, rawPtr, keys[0]);
      rawPtr += 4;
    } else {
      // Write out all the keys.
      for (int i = 0; i < targetsSize; i++) {
        RawInsnHelper.writeUnsignedIntToFourBytes(dataInsn.rawBytes, rawPtr, keys[i]);
        rawPtr += 4;
      }
    }

    // Write out all the targets.
    for (int i = 0; i < targetsSize; i++) {
      RawInsnHelper.writeUnsignedIntToFourBytes(dataInsn.rawBytes, rawPtr, targets[i]);
      rawPtr += 4;
    }
  }

  /**
   * After mutation, data instructions may no longer be 4-byte aligned.
   * If this is the case, insert nops to align them all.
   * This makes a number of assumptions about data currently:
   * - data is always at the end of method insns
   * - all data instructions are stored contiguously
   */
  private void alignDataInstructions(MutatableCode mutatableCode) {
    // Find all the switch data instructions.
    List<MInsn> dataInsns = new ArrayList<MInsn>();

    // Update raw sizes of the data instructions as well.
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn instanceof MSwitchInsn) {
        // Update the raw size of the instruction.
        MSwitchInsn switchInsn = (MSwitchInsn) mInsn;
        int targetsSize = switchInsn.targets.size();
        Instruction dataInsn = switchInsn.dataTarget.insn;
        if (switchInsn.packed) {
          dataInsn.rawSize = (targetsSize * 2) + 4;
        } else {
          dataInsn.rawSize = (targetsSize * 4) + 2;
        }
        dataInsns.add(switchInsn.dataTarget);
      } else if (mInsn instanceof MInsnWithData) {
        MInsnWithData insnWithData =
            (MInsnWithData) mInsn;
        dataInsns.add(insnWithData.dataTarget);
      }
    }

    // Only need to align switch data instructions if there are any!
    if (!dataInsns.isEmpty()) {

      Log.debug("Found data instructions, checking alignment...");

      // Sort data_insns by location.
      Collections.sort(dataInsns, new Comparator<MInsn>() {
        @Override
        public int compare(MInsn first, MInsn second) {
          if (first.location < second.location) {
            return -1;
          } else if (first.location > second.location) {
            return 1;
          }
          return 0;
        }
      });

      boolean performedAlignment = false;

      // Go through all the data insns, and insert an alignment nop if they're unaligned.
      for (MInsn dataInsn : dataInsns) {
        if (dataInsn.location % 2 != 0) {
          Log.debug("Aligning data instruction with a nop.");
          int alignmentNopIdx = mutatableCode.getInstructionIndex(dataInsn);
          MInsn nop = new MInsn();
          nop.insn = new Instruction();
          nop.insn.info = Instruction.getOpcodeInfo(Opcode.NOP);
          mutatableCode.insertInstructionAt(nop, alignmentNopIdx);
          performedAlignment = true;
        }
      }

      if (!performedAlignment) {
        Log.debug("Alignment okay.");
      }
    }
  }

  /**
   * Determine if a particular instruction is a branch instruction, based on opcode.
   */
  private boolean isInstructionBranch(Instruction insn) {
    Opcode opcode = insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.IF_EQ, Opcode.IF_LEZ)
        || Opcode.isBetween(opcode, Opcode.GOTO, Opcode.GOTO_32)) {
      return true;
    }
    return false;
  }

  /**
   * Determine if a particular instruction is a switch instruction, based on opcode.
   */
  private boolean isInstructionSwitch(Instruction insn) {
    Opcode opcode = insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.PACKED_SWITCH, Opcode.SPARSE_SWITCH)) {
      return true;
    }
    return false;
  }

  private boolean isInstructionFillArrayData(Instruction insn) {
    return (insn.info.opcode == Opcode.FILL_ARRAY_DATA);
  }
}
