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
import dexfuzz.rawdex.formats.AbstractFormat;
import dexfuzz.rawdex.formats.ContainsConst;
import dexfuzz.rawdex.formats.ContainsPoolIndex;
import dexfuzz.rawdex.formats.ContainsTarget;
import dexfuzz.rawdex.formats.ContainsVRegs;
import dexfuzz.rawdex.formats.Format10t;
import dexfuzz.rawdex.formats.Format10x;
import dexfuzz.rawdex.formats.Format11n;
import dexfuzz.rawdex.formats.Format11x;
import dexfuzz.rawdex.formats.Format12x;
import dexfuzz.rawdex.formats.Format20t;
import dexfuzz.rawdex.formats.Format21c;
import dexfuzz.rawdex.formats.Format21h;
import dexfuzz.rawdex.formats.Format21s;
import dexfuzz.rawdex.formats.Format21t;
import dexfuzz.rawdex.formats.Format22b;
import dexfuzz.rawdex.formats.Format22c;
import dexfuzz.rawdex.formats.Format22s;
import dexfuzz.rawdex.formats.Format22t;
import dexfuzz.rawdex.formats.Format22x;
import dexfuzz.rawdex.formats.Format23x;
import dexfuzz.rawdex.formats.Format30t;
import dexfuzz.rawdex.formats.Format31c;
import dexfuzz.rawdex.formats.Format31i;
import dexfuzz.rawdex.formats.Format31t;
import dexfuzz.rawdex.formats.Format32x;
import dexfuzz.rawdex.formats.Format35c;
import dexfuzz.rawdex.formats.Format3rc;
import dexfuzz.rawdex.formats.Format51l;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class Instruction implements RawDexObject {
  // Only used by Format35* instructions
  public static class InvokeFormatInfo {
    public byte vregD;
    public byte vregE;
    public byte vregF;
    public byte vregG;
  }

  // Immutable information about this class of instruction.
  public OpcodeInfo info;

  // The raw bytes of the instruction.
  // Only used during reading, and writing out is done from the decoded instruction data.
  //  Except in the case of the 3 "data" instructions.
  public byte[] rawBytes;

  public static final int RAW_TYPE_PACKED_SWITCH_DATA = 1;
  public static final int RAW_TYPE_SPARSE_SWITCH_DATA = 2;
  public static final int RAW_TYPE_FILL_ARRAY_DATA_DATA = 3;

  public int rawType;
  public boolean justRaw;
  public int rawSize;

  public long vregA = 0;
  public long vregB = 0;
  public long vregC = 0;

  public InvokeFormatInfo invokeFormatInfo;

  /**
   * Clone an instruction.
   */
  public Instruction clone() {
    Instruction newInsn = new Instruction();
    // If we've generated a new instruction, we won't have calculated its raw array.
    if (newInsn.rawBytes != null) {
      newInsn.rawBytes = new byte[rawBytes.length];
      for (int i = 0; i < rawBytes.length; i++) {
        newInsn.rawBytes[i] = rawBytes[i];
      }
    }
    newInsn.justRaw = justRaw;
    newInsn.rawType = rawType;
    newInsn.rawSize = rawSize;

    newInsn.vregA = vregA;
    newInsn.vregB = vregB;
    newInsn.vregC = vregC;
    newInsn.info = info;
    if (invokeFormatInfo != null) {
      newInsn.invokeFormatInfo = new InvokeFormatInfo();
      newInsn.invokeFormatInfo.vregD = invokeFormatInfo.vregD;
      newInsn.invokeFormatInfo.vregE = invokeFormatInfo.vregE;
      newInsn.invokeFormatInfo.vregF = invokeFormatInfo.vregF;
      newInsn.invokeFormatInfo.vregG = invokeFormatInfo.vregG;
    }
    return newInsn;
  }

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    // Remember the offset, so after reading the opcode, we can read the whole
    // insn into raw_bytes.
    long offset = file.getFilePointer();
    int opcodeValue = readOpcode(file);
    info = getOpcodeInfo(opcodeValue);
    if (info == null) {
      Log.errorAndQuit("Couldn't find OpcodeInfo for opcode with value: "
          + opcodeValue);
    }

    rawBytes = new byte[2 * getSize()];
    file.seek(offset);
    file.read(rawBytes);

    vregA = info.format.getA(rawBytes);
    vregB = info.format.getB(rawBytes);
    vregC = info.format.getC(rawBytes);

    // Special case for 35* formats.
    if (info.format.needsInvokeFormatInfo()) {
      invokeFormatInfo = new InvokeFormatInfo();
      invokeFormatInfo.vregD = (byte) (rawBytes[4] >> 4);
      invokeFormatInfo.vregE = (byte) (rawBytes[5] & 0xf);
      invokeFormatInfo.vregF = (byte) (rawBytes[5] >> 4);
      invokeFormatInfo.vregG = (byte) (rawBytes[1] & 0xf);
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    if (justRaw) {
      // It is the responsibility of the CodeTranslator to make
      // sure the raw bytes have been updated.
      file.write(rawBytes);
    } else {
      info.format.writeToFile(file, this);
    }
  }

  /**
   * Get the size of an instruction, in code-words. (Code-words are 16-bits.)
   */
  public int getSize() {
    if (justRaw) {
      // It is the responsibility of the CodeTranslator to make sure
      // the raw size has been updated.
      return rawSize;
    }
    return info.format.getSize();
  }

  private int readOpcode(DexRandomAccessFile file) throws IOException {
    short firstCodeWord = file.readUShort();
    int opcode = (firstCodeWord & 0xff);
    int upperBits = (firstCodeWord & 0xff00) >> 8;
    if (opcode == 0x0 && upperBits != 0x0) {
      justRaw = true;
      rawType = upperBits;
      // Need to calculate special sizes.
      switch (rawType) {
        case RAW_TYPE_PACKED_SWITCH_DATA:
          rawSize = (file.readUShort() * 2) + 4;
          break;
        case RAW_TYPE_SPARSE_SWITCH_DATA:
          rawSize = (file.readUShort() * 4) + 2;
          break;
        case RAW_TYPE_FILL_ARRAY_DATA_DATA:
        {
          int elementWidth = file.readUShort();
          rawSize = ((file.readUInt() * elementWidth + 1) / 2) + 4;
          break;
        }
        default:
          Log.errorAndQuit("Unrecognised ident in data-payload instruction: " + rawType);
      }
    }
    return opcode;
  }

  @Override
  public String toString() {
    if (justRaw) {
      switch (rawType) {
        case RAW_TYPE_PACKED_SWITCH_DATA:
          return "PACKED SWITCH DATA";
        case RAW_TYPE_SPARSE_SWITCH_DATA:
          return "SPARSE SWITCH DATA";
        case RAW_TYPE_FILL_ARRAY_DATA_DATA:
          return "FILL ARRAY DATA DATA";
        default:
      }

    }

    String repr = info.name;

    AbstractFormat format = info.format;

    if (invokeFormatInfo != null) {
      String vregs = "";

      int numVregs = (int) vregA;

      if (numVregs > 5) {
        Log.debug("vA in an 35c invoke was greater than 5? Assuming 5.");
        numVregs = 5;
      } else if (numVregs < 0) {
        Log.debug("vA in an 35c invoke was less than 0? Assuming 0.");
        numVregs = 0;
      }

      switch (numVregs) {
        case 5:
          vregs = ", v" + invokeFormatInfo.vregG;
          // fallthrough
        case 4:
          vregs = ", v" + invokeFormatInfo.vregF + vregs;
          // fallthrough
        case 3:
          vregs = ", v" + invokeFormatInfo.vregE + vregs;
          // fallthrough
        case 2:
          vregs = ", v" + invokeFormatInfo.vregD + vregs;
          // fallthrough
        case 1:
          vregs = "v" + vregC + vregs;
          break;
        default:
      }

      repr += "(" + vregs + ")";

      long poolIndex = ((ContainsPoolIndex)format).getPoolIndex(this);
      repr += " meth@" + poolIndex;

      return repr;
    }



    if (format instanceof ContainsVRegs) {
      String vregs = "";
      switch (((ContainsVRegs)format).getVRegCount()) {
        case 3:
          vregs = ", v" + vregC;
          // fallthrough
        case 2:
          vregs = ", v" + vregB + vregs;
          // fallthrough
        case 1:
          vregs = "v" + vregA + vregs;
          break;
        default:
          Log.errorAndQuit("Invalid number of vregs reported by a Format.");
      }

      repr += " " + vregs;
    }
    if (format instanceof ContainsConst) {
      long constant = ((ContainsConst)format).getConst(this);
      repr += " #" + constant;
    }
    if (format instanceof ContainsPoolIndex) {
      long poolIndex = ((ContainsPoolIndex)format).getPoolIndex(this);
      repr += " pool@" + poolIndex;
    }
    if (format instanceof ContainsTarget) {
      long target = ((ContainsTarget)format).getTarget(this);
      repr += " +" + target;
    }

    return repr;
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }

  // STATIC INSTRUCTION CODE
  private static Map<Integer,OpcodeInfo> opcode_map_by_int = new HashMap<Integer,OpcodeInfo>();
  private static Map<Opcode,OpcodeInfo> opcode_map_by_enum = new HashMap<Opcode,OpcodeInfo>();

  public static OpcodeInfo getOpcodeInfo(Opcode opcode) {
    return opcode_map_by_enum.get(opcode);
  }

  public static OpcodeInfo getOpcodeInfo(int opcodeValue) {
    return opcode_map_by_int.get(opcodeValue);
  }

  private static void addOpcodeInfo(Opcode opcode, String name,
      int opcodeValue, AbstractFormat fmt) {
    OpcodeInfo info = new OpcodeInfo(opcode, name, opcodeValue, fmt);
    if (opcode.ordinal() != opcodeValue) {
      Log.errorAndQuit(String.format("Opcode: %s (enum ordinal 0x%x) != (value 0x%x)",
          opcode.toString(), opcode.ordinal(), opcodeValue));
    }
    opcode_map_by_int.put(opcodeValue, info);
    opcode_map_by_enum.put(opcode, info);
  }

  static {
    addOpcodeInfo(Opcode.NOP, "nop", 0x00, new Format10x());
    addOpcodeInfo(Opcode.MOVE, "move", 0x01, new Format12x());
    addOpcodeInfo(Opcode.MOVE_FROM16, "move/from16", 0x02, new Format22x());
    addOpcodeInfo(Opcode.MOVE_16, "move/16", 0x03, new Format32x());
    addOpcodeInfo(Opcode.MOVE_WIDE, "move-wide", 0x04, new Format12x());
    addOpcodeInfo(Opcode.MOVE_WIDE_FROM16, "move-wide/from16", 0x05, new Format22x());
    addOpcodeInfo(Opcode.MOVE_WIDE_16, "move-wide/16", 0x06, new Format32x());
    addOpcodeInfo(Opcode.MOVE_OBJECT, "move-object", 0x07, new Format12x());
    addOpcodeInfo(Opcode.MOVE_OBJECT_FROM16, "move-object/from16", 0x08, new Format22x());
    addOpcodeInfo(Opcode.MOVE_OBJECT_16, "move-object/16", 0x09, new Format32x());
    addOpcodeInfo(Opcode.MOVE_RESULT, "move-result", 0x0a, new Format11x());
    addOpcodeInfo(Opcode.MOVE_RESULT_WIDE, "move-result-wide", 0x0b, new Format11x());
    addOpcodeInfo(Opcode.MOVE_RESULT_OBJECT, "move-result-object", 0x0c, new Format11x());
    addOpcodeInfo(Opcode.MOVE_EXCEPTION, "move-exception", 0x0d, new Format11x());
    addOpcodeInfo(Opcode.RETURN_VOID, "return-void", 0x0e, new Format10x());
    addOpcodeInfo(Opcode.RETURN, "return", 0x0f, new Format11x());
    addOpcodeInfo(Opcode.RETURN_WIDE, "return-wide", 0x10, new Format11x());
    addOpcodeInfo(Opcode.RETURN_OBJECT, "return-object", 0x11, new Format11x());
    addOpcodeInfo(Opcode.CONST_4, "const/4", 0x12, new Format11n());
    addOpcodeInfo(Opcode.CONST_16, "const/16", 0x13, new Format21s());
    addOpcodeInfo(Opcode.CONST, "const", 0x14, new Format31i());
    addOpcodeInfo(Opcode.CONST_HIGH16, "const/high16", 0x15, new Format21h());
    addOpcodeInfo(Opcode.CONST_WIDE_16, "const-wide/16", 0x16, new Format21s());
    addOpcodeInfo(Opcode.CONST_WIDE_32, "const-wide/32", 0x17, new Format31i());
    addOpcodeInfo(Opcode.CONST_WIDE, "const-wide", 0x18, new Format51l());
    addOpcodeInfo(Opcode.CONST_WIDE_HIGH16, "const-wide/high16", 0x19, new Format21h());
    addOpcodeInfo(Opcode.CONST_STRING, "const-string", 0x1a, new Format21c());
    addOpcodeInfo(Opcode.CONST_STRING_JUMBO, "const-string/jumbo", 0x1b, new Format31c());
    addOpcodeInfo(Opcode.CONST_CLASS, "const-class", 0x1c, new Format21c());
    addOpcodeInfo(Opcode.MONITOR_ENTER, "monitor-enter", 0x1d, new Format11x());
    addOpcodeInfo(Opcode.MONITOR_EXIT, "monitor-exit", 0x1e, new Format11x());
    addOpcodeInfo(Opcode.CHECK_CAST, "check-cast", 0x1f, new Format21c());
    addOpcodeInfo(Opcode.INSTANCE_OF, "instance-of", 0x20, new Format22c());
    addOpcodeInfo(Opcode.ARRAY_LENGTH, "array-length", 0x21, new Format12x());
    addOpcodeInfo(Opcode.NEW_INSTANCE, "new-instance", 0x22, new Format21c());
    addOpcodeInfo(Opcode.NEW_ARRAY, "new-array", 0x23, new Format22c());
    addOpcodeInfo(Opcode.FILLED_NEW_ARRAY, "filled-new-array", 0x24, new Format35c());
    addOpcodeInfo(Opcode.FILLED_NEW_ARRAY_RANGE, "filled-new-array/range",
        0x25, new Format3rc());
    addOpcodeInfo(Opcode.FILL_ARRAY_DATA, "fill-array-data", 0x26, new Format31t());
    addOpcodeInfo(Opcode.THROW, "throw", 0x27, new Format11x());
    addOpcodeInfo(Opcode.GOTO, "goto", 0x28, new Format10t());
    addOpcodeInfo(Opcode.GOTO_16, "goto/16", 0x29, new Format20t());
    addOpcodeInfo(Opcode.GOTO_32, "goto/32", 0x2a, new Format30t());
    addOpcodeInfo(Opcode.PACKED_SWITCH, "packed-switch", 0x2b, new Format31t());
    addOpcodeInfo(Opcode.SPARSE_SWITCH, "sparse-switch", 0x2c, new Format31t());
    addOpcodeInfo(Opcode.CMPL_FLOAT, "cmpl-float", 0x2d, new Format23x());
    addOpcodeInfo(Opcode.CMPG_FLOAT, "cmpg-float", 0x2e, new Format23x());
    addOpcodeInfo(Opcode.CMPL_DOUBLE, "cmpl-double", 0x2f, new Format23x());
    addOpcodeInfo(Opcode.CMPG_DOUBLE, "cmpg-double", 0x30, new Format23x());
    addOpcodeInfo(Opcode.CMP_LONG, "cmp-long", 0x31, new Format23x());
    addOpcodeInfo(Opcode.IF_EQ, "if-eq", 0x32, new Format22t());
    addOpcodeInfo(Opcode.IF_NE, "if-ne", 0x33, new Format22t());
    addOpcodeInfo(Opcode.IF_LT, "if-lt", 0x34, new Format22t());
    addOpcodeInfo(Opcode.IF_GE, "if-ge", 0x35, new Format22t());
    addOpcodeInfo(Opcode.IF_GT, "if-gt", 0x36, new Format22t());
    addOpcodeInfo(Opcode.IF_LE, "if-le", 0x37, new Format22t());
    addOpcodeInfo(Opcode.IF_EQZ, "if-eqz", 0x38, new Format21t());
    addOpcodeInfo(Opcode.IF_NEZ, "if-nez", 0x39, new Format21t());
    addOpcodeInfo(Opcode.IF_LTZ, "if-ltz", 0x3a, new Format21t());
    addOpcodeInfo(Opcode.IF_GEZ, "if-gez", 0x3b, new Format21t());
    addOpcodeInfo(Opcode.IF_GTZ, "if-gtz", 0x3c, new Format21t());
    addOpcodeInfo(Opcode.IF_LEZ, "if-lez", 0x3d, new Format21t());
    addOpcodeInfo(Opcode.UNUSED_3E, "unused-3e", 0x3e, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_3F, "unused-3f", 0x3f, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_40, "unused-40", 0x40, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_41, "unused-41", 0x41, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_42, "unused-42", 0x42, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_43, "unused-43", 0x43, new Format10x());
    addOpcodeInfo(Opcode.AGET, "aget", 0x44, new Format23x());
    addOpcodeInfo(Opcode.AGET_WIDE, "aget-wide", 0x45, new Format23x());
    addOpcodeInfo(Opcode.AGET_WIDE, "aget-wide", 0x45, new Format23x());
    addOpcodeInfo(Opcode.AGET_OBJECT, "aget-object", 0x46, new Format23x());
    addOpcodeInfo(Opcode.AGET_BOOLEAN, "aget-boolean", 0x47, new Format23x());
    addOpcodeInfo(Opcode.AGET_BYTE, "aget-byte", 0x48, new Format23x());
    addOpcodeInfo(Opcode.AGET_CHAR, "aget-char", 0x49, new Format23x());
    addOpcodeInfo(Opcode.AGET_SHORT, "aget-short", 0x4a, new Format23x());
    addOpcodeInfo(Opcode.APUT, "aput", 0x4b, new Format23x());
    addOpcodeInfo(Opcode.APUT_WIDE, "aput-wide", 0x4c, new Format23x());
    addOpcodeInfo(Opcode.APUT_OBJECT, "aput-object", 0x4d, new Format23x());
    addOpcodeInfo(Opcode.APUT_BOOLEAN, "aput-boolean", 0x4e, new Format23x());
    addOpcodeInfo(Opcode.APUT_BYTE, "aput-byte", 0x4f, new Format23x());
    addOpcodeInfo(Opcode.APUT_CHAR, "aput-char", 0x50, new Format23x());
    addOpcodeInfo(Opcode.APUT_SHORT, "aput-short", 0x51, new Format23x());
    addOpcodeInfo(Opcode.IGET, "iget", 0x52, new Format22c());
    addOpcodeInfo(Opcode.IGET_WIDE, "iget-wide", 0x53, new Format22c());
    addOpcodeInfo(Opcode.IGET_OBJECT, "iget-object", 0x54, new Format22c());
    addOpcodeInfo(Opcode.IGET_BOOLEAN, "iget-boolean", 0x55, new Format22c());
    addOpcodeInfo(Opcode.IGET_BYTE, "iget-byte", 0x56, new Format22c());
    addOpcodeInfo(Opcode.IGET_CHAR, "iget-char", 0x57, new Format22c());
    addOpcodeInfo(Opcode.IGET_SHORT, "iget-short", 0x58, new Format22c());
    addOpcodeInfo(Opcode.IPUT, "iput", 0x59, new Format22c());
    addOpcodeInfo(Opcode.IPUT_WIDE, "iput-wide", 0x5a, new Format22c());
    addOpcodeInfo(Opcode.IPUT_OBJECT, "iput-object", 0x5b, new Format22c());
    addOpcodeInfo(Opcode.IPUT_BOOLEAN, "iput-boolean", 0x5c, new Format22c());
    addOpcodeInfo(Opcode.IPUT_BYTE, "iput-byte", 0x5d, new Format22c());
    addOpcodeInfo(Opcode.IPUT_CHAR, "iput-char", 0x5e, new Format22c());
    addOpcodeInfo(Opcode.IPUT_SHORT, "iput-short", 0x5f, new Format22c());
    addOpcodeInfo(Opcode.SGET, "sget", 0x60, new Format21c());
    addOpcodeInfo(Opcode.SGET_WIDE, "sget-wide", 0x61, new Format21c());
    addOpcodeInfo(Opcode.SGET_OBJECT, "sget-object", 0x62, new Format21c());
    addOpcodeInfo(Opcode.SGET_BOOLEAN, "sget-boolean", 0x63, new Format21c());
    addOpcodeInfo(Opcode.SGET_BYTE, "sget-byte", 0x64, new Format21c());
    addOpcodeInfo(Opcode.SGET_CHAR, "sget-char", 0x65, new Format21c());
    addOpcodeInfo(Opcode.SGET_SHORT, "sget-short", 0x66, new Format21c());
    addOpcodeInfo(Opcode.SPUT, "sput", 0x67, new Format21c());
    addOpcodeInfo(Opcode.SPUT_WIDE, "sput-wide", 0x68, new Format21c());
    addOpcodeInfo(Opcode.SPUT_OBJECT, "sput-object", 0x69, new Format21c());
    addOpcodeInfo(Opcode.SPUT_BOOLEAN, "sput-boolean", 0x6a, new Format21c());
    addOpcodeInfo(Opcode.SPUT_BYTE, "sput-byte", 0x6b, new Format21c());
    addOpcodeInfo(Opcode.SPUT_CHAR, "sput-char", 0x6c, new Format21c());
    addOpcodeInfo(Opcode.SPUT_SHORT, "sput-short", 0x6d, new Format21c());
    addOpcodeInfo(Opcode.INVOKE_VIRTUAL, "invoke-virtual", 0x6e, new Format35c());
    addOpcodeInfo(Opcode.INVOKE_SUPER, "invoke-super", 0x6f, new Format35c());
    addOpcodeInfo(Opcode.INVOKE_DIRECT, "invoke-direct", 0x70, new Format35c());
    addOpcodeInfo(Opcode.INVOKE_STATIC, "invoke-static", 0x71, new Format35c());
    addOpcodeInfo(Opcode.INVOKE_INTERFACE, "invoke-interface", 0x72, new Format35c());
    addOpcodeInfo(Opcode.RETURN_VOID_NO_BARRIER, "return-void-no-barrier", 0x73, new Format10x());
    addOpcodeInfo(Opcode.INVOKE_VIRTUAL_RANGE, "invoke-virtual/range", 0x74, new Format3rc());
    addOpcodeInfo(Opcode.INVOKE_SUPER_RANGE, "invoke-super/range", 0x75, new Format3rc());
    addOpcodeInfo(Opcode.INVOKE_DIRECT_RANGE, "invoke-direct/range", 0x76, new Format3rc());
    addOpcodeInfo(Opcode.INVOKE_STATIC_RANGE, "invoke-static/range", 0x77, new Format3rc());
    addOpcodeInfo(Opcode.INVOKE_INTERFACE_RANGE, "invoke-interface/range",
        0x78, new Format3rc());
    addOpcodeInfo(Opcode.UNUSED_79, "unused-79", 0x79, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_7A, "unused-7a", 0x7a, new Format10x());
    addOpcodeInfo(Opcode.NEG_INT, "neg-int", 0x7b, new Format12x());
    addOpcodeInfo(Opcode.NOT_INT, "not-int", 0x7c, new Format12x());
    addOpcodeInfo(Opcode.NEG_LONG, "neg-long", 0x7d, new Format12x());
    addOpcodeInfo(Opcode.NOT_LONG, "not-long", 0x7e, new Format12x());
    addOpcodeInfo(Opcode.NEG_FLOAT, "neg-float", 0x7f, new Format12x());
    addOpcodeInfo(Opcode.NEG_DOUBLE, "neg-double", 0x80, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_LONG, "int-to-long", 0x81, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_FLOAT, "int-to-float", 0x82, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_DOUBLE, "int-to-double", 0x83, new Format12x());
    addOpcodeInfo(Opcode.LONG_TO_INT, "long-to-int", 0x84, new Format12x());
    addOpcodeInfo(Opcode.LONG_TO_FLOAT, "long-to-float", 0x85, new Format12x());
    addOpcodeInfo(Opcode.LONG_TO_DOUBLE, "long-to-double", 0x86, new Format12x());
    addOpcodeInfo(Opcode.FLOAT_TO_INT, "float-to-int", 0x87, new Format12x());
    addOpcodeInfo(Opcode.FLOAT_TO_LONG, "float-to-long", 0x88, new Format12x());
    addOpcodeInfo(Opcode.FLOAT_TO_DOUBLE, "float-to-double", 0x89, new Format12x());
    addOpcodeInfo(Opcode.DOUBLE_TO_INT, "double-to-int", 0x8a, new Format12x());
    addOpcodeInfo(Opcode.DOUBLE_TO_LONG, "double-to-long", 0x8b, new Format12x());
    addOpcodeInfo(Opcode.DOUBLE_TO_FLOAT, "double-to-float", 0x8c, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_BYTE, "int-to-byte", 0x8d, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_CHAR, "int-to-char", 0x8e, new Format12x());
    addOpcodeInfo(Opcode.INT_TO_SHORT, "int-to-short", 0x8f, new Format12x());
    addOpcodeInfo(Opcode.ADD_INT, "add-int", 0x90, new Format23x());
    addOpcodeInfo(Opcode.SUB_INT, "sub-int", 0x91, new Format23x());
    addOpcodeInfo(Opcode.MUL_INT, "mul-int", 0x92, new Format23x());
    addOpcodeInfo(Opcode.DIV_INT, "div-int", 0x93, new Format23x());
    addOpcodeInfo(Opcode.REM_INT, "rem-int", 0x94, new Format23x());
    addOpcodeInfo(Opcode.AND_INT, "and-int", 0x95, new Format23x());
    addOpcodeInfo(Opcode.OR_INT, "or-int", 0x96, new Format23x());
    addOpcodeInfo(Opcode.XOR_INT, "xor-int", 0x97, new Format23x());
    addOpcodeInfo(Opcode.SHL_INT, "shl-int", 0x98, new Format23x());
    addOpcodeInfo(Opcode.SHR_INT, "shr-int", 0x99, new Format23x());
    addOpcodeInfo(Opcode.USHR_INT, "ushr-int", 0x9a, new Format23x());
    addOpcodeInfo(Opcode.ADD_LONG, "add-long", 0x9b, new Format23x());
    addOpcodeInfo(Opcode.SUB_LONG, "sub-long", 0x9c, new Format23x());
    addOpcodeInfo(Opcode.MUL_LONG, "mul-long", 0x9d, new Format23x());
    addOpcodeInfo(Opcode.DIV_LONG, "div-long", 0x9e, new Format23x());
    addOpcodeInfo(Opcode.REM_LONG, "rem-long", 0x9f, new Format23x());
    addOpcodeInfo(Opcode.AND_LONG, "and-long", 0xa0, new Format23x());
    addOpcodeInfo(Opcode.OR_LONG, "or-long", 0xa1, new Format23x());
    addOpcodeInfo(Opcode.XOR_LONG, "xor-long", 0xa2, new Format23x());
    addOpcodeInfo(Opcode.SHL_LONG, "shl-long", 0xa3, new Format23x());
    addOpcodeInfo(Opcode.SHR_LONG, "shr-long", 0xa4, new Format23x());
    addOpcodeInfo(Opcode.USHR_LONG, "ushr-long", 0xa5, new Format23x());
    addOpcodeInfo(Opcode.ADD_FLOAT, "add-float", 0xa6, new Format23x());
    addOpcodeInfo(Opcode.SUB_FLOAT, "sub-float", 0xa7, new Format23x());
    addOpcodeInfo(Opcode.MUL_FLOAT, "mul-float", 0xa8, new Format23x());
    addOpcodeInfo(Opcode.DIV_FLOAT, "div-float", 0xa9, new Format23x());
    addOpcodeInfo(Opcode.REM_FLOAT, "rem-float", 0xaa, new Format23x());
    addOpcodeInfo(Opcode.ADD_DOUBLE, "add-double", 0xab, new Format23x());
    addOpcodeInfo(Opcode.SUB_DOUBLE, "sub-double", 0xac, new Format23x());
    addOpcodeInfo(Opcode.MUL_DOUBLE, "mul-double", 0xad, new Format23x());
    addOpcodeInfo(Opcode.DIV_DOUBLE, "div-double", 0xae, new Format23x());
    addOpcodeInfo(Opcode.REM_DOUBLE, "rem-double", 0xaf, new Format23x());
    addOpcodeInfo(Opcode.ADD_INT_2ADDR, "add-int/2addr", 0xb0, new Format12x());
    addOpcodeInfo(Opcode.SUB_INT_2ADDR, "sub-int/2addr", 0xb1, new Format12x());
    addOpcodeInfo(Opcode.MUL_INT_2ADDR, "mul-int/2addr", 0xb2, new Format12x());
    addOpcodeInfo(Opcode.DIV_INT_2ADDR, "div-int/2addr", 0xb3, new Format12x());
    addOpcodeInfo(Opcode.REM_INT_2ADDR, "rem-int/2addr", 0xb4, new Format12x());
    addOpcodeInfo(Opcode.AND_INT_2ADDR, "and-int/2addr", 0xb5, new Format12x());
    addOpcodeInfo(Opcode.OR_INT_2ADDR, "or-int/2addr", 0xb6, new Format12x());
    addOpcodeInfo(Opcode.XOR_INT_2ADDR, "xor-int/2addr", 0xb7, new Format12x());
    addOpcodeInfo(Opcode.SHL_INT_2ADDR, "shl-int/2addr", 0xb8, new Format12x());
    addOpcodeInfo(Opcode.SHR_INT_2ADDR, "shr-int/2addr", 0xb9, new Format12x());
    addOpcodeInfo(Opcode.USHR_INT_2ADDR, "ushr-int/2addr", 0xba, new Format12x());
    addOpcodeInfo(Opcode.ADD_LONG_2ADDR, "add-long/2addr", 0xbb, new Format12x());
    addOpcodeInfo(Opcode.SUB_LONG_2ADDR, "sub-long/2addr", 0xbc, new Format12x());
    addOpcodeInfo(Opcode.MUL_LONG_2ADDR, "mul-long/2addr", 0xbd, new Format12x());
    addOpcodeInfo(Opcode.DIV_LONG_2ADDR, "div-long/2addr", 0xbe, new Format12x());
    addOpcodeInfo(Opcode.REM_LONG_2ADDR, "rem-long/2addr", 0xbf, new Format12x());
    addOpcodeInfo(Opcode.AND_LONG_2ADDR, "and-long/2addr", 0xc0, new Format12x());
    addOpcodeInfo(Opcode.OR_LONG_2ADDR, "or-long/2addr", 0xc1, new Format12x());
    addOpcodeInfo(Opcode.XOR_LONG_2ADDR, "xor-long/2addr", 0xc2, new Format12x());
    addOpcodeInfo(Opcode.SHL_LONG_2ADDR, "shl-long/2addr", 0xc3, new Format12x());
    addOpcodeInfo(Opcode.SHR_LONG_2ADDR, "shr-long/2addr", 0xc4, new Format12x());
    addOpcodeInfo(Opcode.USHR_LONG_2ADDR, "ushr-long/2addr", 0xc5, new Format12x());
    addOpcodeInfo(Opcode.ADD_FLOAT_2ADDR, "add-float/2addr", 0xc6, new Format12x());
    addOpcodeInfo(Opcode.SUB_FLOAT_2ADDR, "sub-float/2addr", 0xc7, new Format12x());
    addOpcodeInfo(Opcode.MUL_FLOAT_2ADDR, "mul-float/2addr", 0xc8, new Format12x());
    addOpcodeInfo(Opcode.DIV_FLOAT_2ADDR, "div-float/2addr", 0xc9, new Format12x());
    addOpcodeInfo(Opcode.REM_FLOAT_2ADDR, "rem-float/2addr", 0xca, new Format12x());
    addOpcodeInfo(Opcode.ADD_DOUBLE_2ADDR, "add-double/2addr", 0xcb, new Format12x());
    addOpcodeInfo(Opcode.SUB_DOUBLE_2ADDR, "sub-double/2addr", 0xcc, new Format12x());
    addOpcodeInfo(Opcode.MUL_DOUBLE_2ADDR, "mul-double/2addr", 0xcd, new Format12x());
    addOpcodeInfo(Opcode.DIV_DOUBLE_2ADDR, "div-double/2addr", 0xce, new Format12x());
    addOpcodeInfo(Opcode.REM_DOUBLE_2ADDR, "rem-double/2addr", 0xcf, new Format12x());
    addOpcodeInfo(Opcode.ADD_INT_LIT16, "add-int/lit16", 0xd0, new Format22s());
    addOpcodeInfo(Opcode.RSUB_INT, "rsub-int", 0xd1, new Format22s());
    addOpcodeInfo(Opcode.MUL_INT_LIT16, "mul-int/lit16", 0xd2, new Format22s());
    addOpcodeInfo(Opcode.DIV_INT_LIT16, "div-int/lit16", 0xd3, new Format22s());
    addOpcodeInfo(Opcode.REM_INT_LIT16, "rem-int/lit16", 0xd4, new Format22s());
    addOpcodeInfo(Opcode.AND_INT_LIT16, "and-int/lit16", 0xd5, new Format22s());
    addOpcodeInfo(Opcode.OR_INT_LIT16, "or-int/lit16", 0xd6, new Format22s());
    addOpcodeInfo(Opcode.XOR_INT_LIT16, "xor-int/lit16", 0xd7, new Format22s());
    addOpcodeInfo(Opcode.ADD_INT_LIT8, "add-int/lit8", 0xd8, new Format22b());
    addOpcodeInfo(Opcode.RSUB_INT_LIT8, "rsub-int/lit8", 0xd9, new Format22b());
    addOpcodeInfo(Opcode.MUL_INT_LIT8, "mul-int/lit8", 0xda, new Format22b());
    addOpcodeInfo(Opcode.DIV_INT_LIT8, "div-int/lit8", 0xdb, new Format22b());
    addOpcodeInfo(Opcode.REM_INT_LIT8, "rem-int/lit8", 0xdc, new Format22b());
    addOpcodeInfo(Opcode.AND_INT_LIT8, "and-int/lit8", 0xdd, new Format22b());
    addOpcodeInfo(Opcode.OR_INT_LIT8, "or-int/lit8", 0xde, new Format22b());
    addOpcodeInfo(Opcode.XOR_INT_LIT8, "xor-int/lit8", 0xdf, new Format22b());
    addOpcodeInfo(Opcode.SHL_INT_LIT8, "shl-int/lit8", 0xe0, new Format22b());
    addOpcodeInfo(Opcode.SHR_INT_LIT8, "shr-int/lit8", 0xe1, new Format22b());
    addOpcodeInfo(Opcode.USHR_INT_LIT8, "ushr-int/lit8", 0xe2, new Format22b());
    addOpcodeInfo(Opcode.IGET_QUICK, "+iget-quick", 0xe3, new Format22c());
    addOpcodeInfo(Opcode.IGET_WIDE_QUICK, "+iget-wide-quick", 0xe4, new Format22c());
    addOpcodeInfo(Opcode.IGET_OBJECT_QUICK, "+iget-object-quick", 0xe5, new Format22c());
    addOpcodeInfo(Opcode.IPUT_QUICK, "+iput-quick", 0xe6, new Format22c());
    addOpcodeInfo(Opcode.IPUT_WIDE_QUICK, "+iput-wide-quick", 0xe7, new Format22c());
    addOpcodeInfo(Opcode.IPUT_OBJECT_QUICK, "+iput-object-quick", 0xe8, new Format22c());
    addOpcodeInfo(Opcode.INVOKE_VIRTUAL_QUICK, "+invoke-virtual-quick", 0xe9, new Format35c());
    addOpcodeInfo(Opcode.INVOKE_VIRTUAL_QUICK_RANGE, "+invoke-virtual-quick/range",
        0xea, new Format3rc());
    addOpcodeInfo(Opcode.IPUT_BOOLEAN_QUICK, "+iput-boolean-quick", 0xeb, new Format22c());
    addOpcodeInfo(Opcode.IPUT_BYTE_QUICK, "+iput-byte-quick", 0xec, new Format22c());
    addOpcodeInfo(Opcode.IPUT_CHAR_QUICK, "+iput-char-quick", 0xed, new Format22c());
    addOpcodeInfo(Opcode.IPUT_SHORT_QUICK, "+iput-short-quick", 0xee, new Format22c());
    addOpcodeInfo(Opcode.UNUSED_EF, "unused-ef", 0xef, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F0, "unused-f0", 0xf0, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F1, "unused-f1", 0xf1, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F2, "unused-f2", 0xf2, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F3, "unused-f3", 0xf3, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F4, "unused-f4", 0xf4, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F5, "unused-f5", 0xf5, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F6, "unused-f6", 0xf6, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F7, "unused-f7", 0xf7, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F8, "unused-f8", 0xf8, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_F9, "unused-f9", 0xf9, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FA, "unused-fa", 0xfa, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FB, "unused-fb", 0xfb, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FC, "unused-fc", 0xfc, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FD, "unused-fd", 0xfd, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FE, "unused-fe", 0xfe, new Format10x());
    addOpcodeInfo(Opcode.UNUSED_FF, "unused-ff", 0xff, new Format10x());
    if (opcode_map_by_int.size() != 256) {
      Log.errorAndQuit("Incorrect number of bytecodes defined.");
    }
  }
}
