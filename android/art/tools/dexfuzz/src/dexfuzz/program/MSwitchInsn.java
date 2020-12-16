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

import java.util.LinkedList;
import java.util.List;

/**
 * A subclass of the MInsnWithData, that also has multiple jump targets.
 */
public class MSwitchInsn extends MInsnWithData {
  /**
   * The MInsns this switch instruction branches to.
   */
  public List<MInsn> targets = new LinkedList<MInsn>();

  public boolean packed;

  public int[] keys;

  /**
   * Clone this MSwitchInsn, and clone the wrapped Instruction.
   */
  public MSwitchInsn clone() {
    MSwitchInsn newInsn = new MSwitchInsn();
    newInsn.insn = insn.clone();
    newInsn.dataTarget = dataTarget;
    newInsn.packed = packed;
    for (MInsn target : targets) {
      newInsn.targets.add(target);
    }
    newInsn.keys = new int[keys.length];
    System.arraycopy(keys, 0, newInsn.keys, 0, keys.length);
    return newInsn;
  }
}