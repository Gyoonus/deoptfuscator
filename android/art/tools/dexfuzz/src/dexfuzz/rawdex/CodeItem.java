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

package dexfuzz.rawdex;

import dexfuzz.Log;
import dexfuzz.program.MutatableCode;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

public class CodeItem implements RawDexObject {
  public short registersSize;
  public short insSize;
  public short outsSize;
  public short triesSize;
  public int debugInfoOff; // NB: this is a special case
  public int insnsSize;
  public List<Instruction> insns;
  public TryItem[] tries;
  public EncodedCatchHandlerList handlers;

  private MutatableCode mutatableCode;

  public static class MethodMetaInfo {
    public String methodName;
    public boolean isStatic;
    public String shorty;
  }

  public MethodMetaInfo meta = new MethodMetaInfo();

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().getNewOffsettable(file, this);
    registersSize = file.readUShort();
    insSize = file.readUShort();
    outsSize = file.readUShort();
    triesSize = file.readUShort();
    debugInfoOff = file.readUInt();
    insnsSize = file.readUInt();
    populateInstructionList(file);
    if (triesSize > 0) {
      if ((insnsSize % 2) != 0) {
        // Consume padding.
        file.readUShort();
      }
      tries = new TryItem[triesSize];
      for (int i = 0; i < triesSize; i++) {
        (tries[i] = new TryItem()).read(file);
      }
      (handlers = new EncodedCatchHandlerList()).read(file);
    }
  }

  private void populateInstructionList(DexRandomAccessFile file) throws IOException {
    insns = new LinkedList<Instruction>();
    long insnsOffset = file.getFilePointer();
    if (insnsOffset != 0) {
      long finger = insnsOffset;
      long insnsEnd = insnsOffset + (2 * insnsSize);

      while (finger < insnsEnd) {
        file.seek(finger);
        Instruction newInsn = new Instruction();
        newInsn.read(file);
        insns.add(newInsn);
        finger += (2 * newInsn.getSize());
      }

      file.seek(finger);
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUShort(registersSize);
    file.writeUShort(insSize);
    file.writeUShort(outsSize);
    file.writeUShort(triesSize);
    // We do not support retaining debug info currently.
    file.writeUInt(0 /*debug_info_off*/);
    file.writeUInt(insnsSize);
    for (Instruction insn : insns) {
      insn.write(file);
    }
    if (triesSize > 0) {
      if ((insnsSize % 2) != 0) {
        // produce padding
        file.writeUShort((short) 0);
      }
      for (TryItem tryItem : tries) {
        tryItem.write(file);
      }
      handlers.write(file);
    }
  }

  /**
   * CodeTranslator should call this to notify a CodeItem about its
   * mutatable code, so it can always get the "latest" view of its
   * instructions.
   */
  public void registerMutatableCode(MutatableCode mutatableCode) {
    this.mutatableCode = mutatableCode;
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.TYPE_ID && triesSize > 0) {
      // EncodedCatchHandlerList (well, the EncodedTypeAddrPairs it owns)
      // are only interested in TYPE_IDs.
      handlers.incrementIndex(kind, insertedIdx);
    }

    if (kind == IndexUpdateKind.PROTO_ID) {
      // The only kind we can't encounter in an instruction.
      return;
    }

    List<Instruction> insnsToIncrement = insns;

    // If we have an associated MutatableCode, then it may have created some new insns
    // that we won't know about yet, during the mutation phase.
    //
    // Ask for the latest view of the insns.
    if (mutatableCode != null) {
      insnsToIncrement = mutatableCode.requestLatestInstructions();
    }

    for (Instruction insn : insnsToIncrement) {
      Opcode opcode = insn.info.opcode;
      switch (kind) {
        case STRING_ID:
          if (opcode == Opcode.CONST_STRING || opcode == Opcode.CONST_STRING_JUMBO) {
            // STRING@BBBB
            if (insn.vregB >= insertedIdx) {
              insn.vregB++;
            }
          }
          break;
        case TYPE_ID:
          if (opcode == Opcode.CONST_CLASS
              || opcode == Opcode.CHECK_CAST
              || opcode == Opcode.NEW_INSTANCE
              || opcode == Opcode.FILLED_NEW_ARRAY
              || opcode == Opcode.FILLED_NEW_ARRAY_RANGE) {
            // TYPE@BBBB
            if (insn.vregB >= insertedIdx) {
              insn.vregB++;
            }
          } else if (opcode == Opcode.INSTANCE_OF || opcode == Opcode.NEW_ARRAY) {
            // TYPE@CCCC
            if (insn.vregC >= insertedIdx) {
              insn.vregC++;
            }
          }
          break;
        case FIELD_ID:
          if (Opcode.isBetween(opcode, Opcode.SGET, Opcode.SPUT_SHORT)) {
            // FIELD@BBBB
            if (insn.vregB >= insertedIdx) {
              insn.vregB++;
            }
          } else if (Opcode.isBetween(opcode, Opcode.IGET, Opcode.IPUT_SHORT)) {
            // FIELD@CCCC
            if (insn.vregC >= insertedIdx) {
              insn.vregC++;
            }
          }
          break;
        case METHOD_ID:
          if (Opcode.isBetween(opcode, Opcode.INVOKE_VIRTUAL, Opcode.INVOKE_INTERFACE)
              || Opcode.isBetween(opcode,
                  Opcode.INVOKE_VIRTUAL_RANGE, Opcode.INVOKE_INTERFACE_RANGE)) {
            // METHOD@BBBB
            if (insn.vregB >= insertedIdx) {
              insn.vregB++;
            }
          }
          break;
        default:
          Log.errorAndQuit("Unexpected IndexUpdateKind requested "
              + "in Instruction.incrementIndex()");
      }
    }
  }
}
