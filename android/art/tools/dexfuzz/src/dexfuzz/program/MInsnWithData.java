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

/**
 * A subclass of the MInsn, that tracks the data instruction.
 */
public class MInsnWithData extends MInsn {
  /**
   * The MInsn that represents the data this instruction uses.
   */
  public MInsn dataTarget;

  /**
   * Clone this MInsnWithData, and clone the wrapped Instruction.
   */
  public MInsnWithData clone() {
    MInsnWithData newInsn = new MInsnWithData();
    newInsn.insn = insn.clone();
    newInsn.dataTarget = dataTarget;
    return newInsn;
  }
}