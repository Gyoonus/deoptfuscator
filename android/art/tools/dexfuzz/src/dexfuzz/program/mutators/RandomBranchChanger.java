/*
 * Copyright (C) 2017 The Android Open Source Project
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

package dexfuzz.program.mutators;

import dexfuzz.MutationStats;
import dexfuzz.program.MInsn;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Opcode;
import java.util.List;
import java.util.Random;

public class RandomBranchChanger extends IfBranchChanger {

  private static final Opcode[] EQUALITY_CMP_OP_LIST = {
    Opcode.IF_EQ,
    Opcode.IF_NE,
    Opcode.IF_LT,
    Opcode.IF_GE,
    Opcode.IF_GT,
    Opcode.IF_LE
  };

  private static final Opcode[] ZERO_CMP_OP_LIST = {
    Opcode.IF_EQZ,
    Opcode.IF_NEZ,
    Opcode.IF_LTZ,
    Opcode.IF_GEZ,
    Opcode.IF_GTZ,
    Opcode.IF_LEZ
  };

  public RandomBranchChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  @Override
  protected Opcode getModifiedOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.IF_EQ, Opcode.IF_LE)) {
      int index = opcode.ordinal() - Opcode.IF_EQ.ordinal();
      int length = EQUALITY_CMP_OP_LIST.length;
      return EQUALITY_CMP_OP_LIST[(index + 1 + rng.nextInt(length - 1)) % length];
    } else if (Opcode.isBetween(opcode, Opcode.IF_EQZ, Opcode.IF_LEZ)) {
      int index = opcode.ordinal() - Opcode.IF_EQZ.ordinal();
      int length = ZERO_CMP_OP_LIST.length;
      return ZERO_CMP_OP_LIST[(index + 1 + rng.nextInt(length - 1)) % length];
    }
    return opcode;
  }

  @Override
  protected String getMutationTag() {
    return "random";
  }
}