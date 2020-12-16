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

import dexfuzz.Log;
import dexfuzz.program.mutators.CodeMutator;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;

/**
 * Responsible for serializing mutations, allowing replay of mutations, and searching
 * for a minimal set of mutations.
 */
public class MutationSerializer {
  public static String getMutationString(Mutation mutation) {
    StringBuilder builder = new StringBuilder();
    builder.append(mutation.mutatorClass.getCanonicalName()).append(" ");
    builder.append(mutation.mutatableCodeIdx).append(" ");
    builder.append(mutation.getString());
    return builder.toString();
  }

  public static void writeMutation(BufferedWriter writer, Mutation mutation) throws IOException {
    // Write out the common fields.
    writer.write(mutation.mutatorClass.getCanonicalName() + " "
        + mutation.mutatableCodeIdx + " ");

    // Use the mutation's own function to write out the rest of the fields.
    writer.write(mutation.getString() + "\n");
  }

  @SuppressWarnings("unchecked")
  public static Mutation readMutation(BufferedReader reader) throws IOException {
    String line = reader.readLine();
    String[] fields = null;
    if (line != null) {
      fields = line.split(" ");
    } else {
      Log.errorAndQuit("Could not read line during mutation loading.");
    }

    // Read the mutator's class name
    String mutatorClassName = fields[0];

    // Get the class for that mutator
    Class<? extends CodeMutator> mutatorClass = null;
    try {
      mutatorClass = (Class<? extends CodeMutator>) Class.forName(mutatorClassName);
    } catch (ClassNotFoundException e) {
      Log.errorAndQuit("Cannot find a mutator class called: " + mutatorClassName);
    }

    Mutation mutation = null;
    try {
      mutation = mutatorClass.newInstance().getNewMutation();
    } catch (InstantiationException e) {
      Log.errorAndQuit("Unable to instantiate " + mutatorClassName
          + " using default constructor.");
    } catch (IllegalAccessException e) {
      Log.errorAndQuit("Unable to access methods in " + mutatorClassName + ".");
    }

    if (mutation == null) {
      Log.errorAndQuit("Unable to get Mutation for Mutator: " + mutatorClassName);
    }

    // Populate the common fields of the mutation.
    mutation.mutatorClass = mutatorClass;
    // The Program must set this later, using the mutatable_code_idx
    //   into its list of MutatableCodes.
    mutation.mutatableCode = null;
    mutation.mutatableCodeIdx = Integer.parseInt(fields[1]);

    // Use the mutation's own method to read the rest of the fields.
    mutation.parseString(fields);

    return mutation;
  }
}
