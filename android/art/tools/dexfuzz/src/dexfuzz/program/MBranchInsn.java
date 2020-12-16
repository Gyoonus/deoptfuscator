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
 * A subclass of the MInsn, that tracks its target instruction.
 */
public class MBranchInsn extends MInsn {
  /**
   * The MInsn this branch instruction branches to.
   */
  public MInsn target;

  /**
   * Clone this MBranchInsn, and clone the wrapped Instruction.
   */
  public MBranchInsn clone() {
    MBranchInsn newInsn = new MBranchInsn();
    newInsn.insn = insn.clone();
    newInsn.target = target;
    return newInsn;
  }
}
