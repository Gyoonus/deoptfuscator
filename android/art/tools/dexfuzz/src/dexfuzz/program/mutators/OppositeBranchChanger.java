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

import dexfuzz.Log;
import dexfuzz.MutationStats;
import dexfuzz.program.MInsn;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Opcode;
import java.util.List;
import java.util.Random;

public class OppositeBranchChanger extends IfBranchChanger {

  public OppositeBranchChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 40;
  }

  @Override
  protected Opcode getModifiedOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    switch (opcode) {
      case IF_EQ:
        return Opcode.IF_NE;
      case IF_NE:
        return Opcode.IF_EQ;
      case IF_LT:
        return Opcode.IF_GE;
      case IF_GT:
        return Opcode.IF_LE;
      case IF_GE:
        return Opcode.IF_LT;
      case IF_LE:
        return Opcode.IF_GT;
      case IF_EQZ:
        return Opcode.IF_NEZ;
      case IF_NEZ:
        return Opcode.IF_EQZ;
      case IF_LTZ:
        return Opcode.IF_GEZ;
      case IF_GTZ:
        return Opcode.IF_LEZ;
      case IF_GEZ:
        return Opcode.IF_LTZ;
      case IF_LEZ:
        return Opcode.IF_GTZ;
      default:
        Log.errorAndQuit("Could not find if branch.");
        return opcode;
    }
  }

  @Override
  protected String getMutationTag() {
    return "opposite";
  }
}