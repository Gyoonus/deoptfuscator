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

import dexfuzz.rawdex.Instruction;

/**
 * Base class that is a thin wrapper for Instructions currently, also tracking location
 * as the instruction is moved around.
 */
public class MInsn {
  /**
   * The raw DEX instruction that this instruction represents.
   */
  public Instruction insn;


  /**
   * The location of this instruction, as an offset in code words from the beginning.
   * May become invalid if instructions around it are mutated.
   */
  public int location;

  /**
   * Denotes if the currently associated location can be trusted.
   */
  public boolean locationUpdated;

  /**
   * Clone this MInsn, and clone the wrapped Instruction.
   */
  public MInsn clone() {
    MInsn newInsn = new MInsn();
    newInsn.insn = insn.clone();
    // It is the responsibility of the cloner to update these values.
    newInsn.location = location;
    newInsn.locationUpdated = locationUpdated;
    return newInsn;
  }

  /**
   * Get the String representation of an instruction.
   */
  public String toString() {
    return String.format("{0x%04x%s: %s}",
        location,
        (locationUpdated) ? "!" : "",
            insn.toString());
  }
}
