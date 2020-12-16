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

package dexfuzz.rawdex.formats;

import dexfuzz.rawdex.DexRandomAccessFile;
import dexfuzz.rawdex.Instruction;

import java.io.IOException;

/**
 * Every Format subclasses this AbstractFormat. The subclasses then implement these
 * methods to write out a provided Instruction according to this format, and also methods
 * to read the vregs from an Instruction's raw bytes.
 * Hierarchy is as follows:
 * AbstractFormat
 *   |____________Format1
 *   |              |_____Format10t
 *   |              |_____Format10x
 *   |              |_____Format11n
 *   |              |_____Format11x
 *   |              |_____Format12x
 *   |____________Format2
 *   |              |_____Format20bc
 *   |              |_____Format20t
 *     etc...
 */
public abstract class AbstractFormat {
  /**
   * Get the size of an Instruction that has this format.
   */
  public abstract int getSize();

  /**
   * Given a file handle and an instruction, write that Instruction out to the file
   * correctly, considering the current format.
   */
  public abstract void writeToFile(DexRandomAccessFile file, Instruction insn) throws IOException;

  /**
   * Read the value of vA, considering this format.
   */
  public abstract long getA(byte[] raw) throws IOException;

  /**
   * Read the value of vB, considering this format.
   */
  public abstract long getB(byte[] raw) throws IOException;

  /**
   * Read the value of vC, considering this format.
   */
  public abstract long getC(byte[] raw) throws IOException;

  /**
   * Only Format35c should return true for this.
   */
  public abstract boolean needsInvokeFormatInfo();
}
