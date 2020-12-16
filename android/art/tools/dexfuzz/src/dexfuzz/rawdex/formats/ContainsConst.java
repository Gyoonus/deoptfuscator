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

import dexfuzz.rawdex.Instruction;

/**
 * Every Format that contains a value that is a constant (that includes instructions like
 * const/4, but also add-int/lit8) should implement this interface, to allow the constant
 * part of a provided Instruction to be read and set correctly.
 */
public interface ContainsConst {
  public long getConst(Instruction insn);

  public void setConst(Instruction insn, long constant);

  public long getConstRange();
}
