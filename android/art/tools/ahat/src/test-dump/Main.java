/*
 * Copyright (C) 2015 The Android Open Source Project
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

import dalvik.system.VMDebug;
import java.io.IOException;
import org.apache.harmony.dalvik.ddmc.DdmVmInternal;

/**
 * Program used to create a heap dump for test purposes.
 */
public class Main {
  // Keep a reference to the DumpedStuff instance so that it is not garbage
  // collected before we take the heap dump.
  public static DumpedStuff stuff;

  public static void main(String[] args) throws IOException {
    if (args.length < 1) {
      System.err.println("no output file specified");
      return;
    }
    String file = args[0];

    // If a --base argument is provided, it means we should generate a
    // baseline hprof file suitable for using in testing diff.
    boolean baseline = args.length > 1 && args[1].equals("--base");

    // Enable allocation tracking so we get stack traces in the heap dump.
    DdmVmInternal.enableRecentAllocations(true);

    // Allocate the instance of DumpedStuff.
    stuff = new DumpedStuff(baseline);

    // Create a bunch of unreachable objects pointing to basicString for the
    // reverseReferencesAreNotUnreachable test
    for (int i = 0; i < 100; i++) {
      stuff.basicStringRef = new Object[]{stuff.basicString};
    }

    // Take a heap dump that will include that instance of DumpedStuff.
    System.err.println("Dumping hprof data to " + file);
    VMDebug.dumpHprofData(file);
  }
}
