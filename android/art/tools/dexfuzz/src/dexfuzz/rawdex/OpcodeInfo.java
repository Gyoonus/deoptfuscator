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

import dexfuzz.rawdex.formats.AbstractFormat;

/**
 * Every Instruction points to an OpcodeInfo object that holds useful information
 * about that kind of instruction, including the Format that allows us to read the
 * instructions fields correctly.
 */
public class OpcodeInfo {
  public final Opcode opcode;
  public final String name;
  public final int value;
  public final AbstractFormat format;

  /**
   * Construct an OpcodeInfo. A static list of these is created in Instruction.java.
   */
  public OpcodeInfo(Opcode opcode, String name, int opcodeValue, AbstractFormat fmt) {
    this.opcode = opcode;
    this.name = name;
    this.value = opcodeValue;
    this.format = fmt;
  }
}