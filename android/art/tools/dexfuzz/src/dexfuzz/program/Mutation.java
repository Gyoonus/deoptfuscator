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

import dexfuzz.program.mutators.CodeMutator;

/**
 * Mutation should be subclassed by an AssociatedMutation in each CodeMutator,
 * which will describe the parameters of the mutation, and override the getString()
 * and parseString() methods here to allow serialization of the mutations.
 */
public abstract class Mutation {

  public MutatableCode mutatableCode;

  // The first field of any serialized mutation - the mutator that uses it.
  public Class<? extends CodeMutator> mutatorClass;
  // The second field of any serialized mutation...
  // This is an index into the Program's list of MutatableCodes
  // i.e., it is NOT an index into the DEX file's CodeItems!
  public int mutatableCodeIdx;

  public void setup(Class<? extends CodeMutator> mutatorClass, MutatableCode mutatableCode) {
    this.mutatorClass = mutatorClass;
    this.mutatableCode = mutatableCode;
    this.mutatableCodeIdx = mutatableCode.mutatableCodeIdx;
  }
  /**
   * Serializes the field(s) of the mutation to string format.
   * The fields are separated by a space.
   * @return the serialized string representation of the field(s) of the mutation.
   */
  public abstract String getString();

  /**
   * Deserializes the strings back to the field(s) of the mutation,
   * given a string array as its argument. The string array
   * contains the individual elements which were previously constructed by
   * getstring() method. elements[0] stores the class name and elements[1]
   * stores the mutable code index which are predefined in MutationSerializer.java.
   * Users can deserialize the string representation in elements[2] and so forth.
   * @param elements string array with serialized representations of the field(s) of the mutation.
   */
  public abstract void parseString(String[] elements);
}