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

import java.lang.reflect.Method;
import java.util.Enumeration;

import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * DexFile tests (Dalvik-specific).
 */
public class Main {
    private static final String CLASS_PATH =
        System.getenv("DEX_LOCATION") + "/071-dexfile-map-clean-ex.jar";

    /**
     * Prep the environment then run the test.
     */
    public static void main(String[] args) throws Exception {
        // Load the dex file, this is a pre-requisite to mmap-ing it in.
        Class<?> AnotherClass = testDexFile();
        // Check that the memory maps are clean.
        testDexMemoryMaps();

        // Prevent garbage collector from collecting our DexFile
        // (and unmapping too early) by using it after we finish
        // our verification.
        AnotherClass.newInstance();
    }

    private static boolean checkSmapsEntry(String[] smapsLines, int offset) {
      String nameDescription = smapsLines[offset];
      String[] split = nameDescription.split(" ");

      String permissions = split[1];
      // Mapped as read-only + anonymous.
      if (!permissions.startsWith("r--p")) {
        return false;
      }

      boolean validated = false;

      // We have the right entry, now make sure it's valid.
      for (int i = offset; i < smapsLines.length; ++i) {
        String line = smapsLines[i];

        if (line.startsWith("Shared_Dirty") || line.startsWith("Private_Dirty")) {
          String lineTrimmed = line.trim();
          String[] lineSplit = lineTrimmed.split(" +");

          String sizeUsuallyInKb = lineSplit[lineSplit.length - 2];

          sizeUsuallyInKb = sizeUsuallyInKb.trim();

          if (!sizeUsuallyInKb.equals("0")) {
            System.out.println(
                "ERROR: Memory mapping for " + CLASS_PATH + " is unexpectedly dirty");
            System.out.println(line);
          } else {
            validated = true;
          }
        }

        // VmFlags marks the "end" of an smaps entry.
        if (line.startsWith("VmFlags")) {
          break;
        }
      }

      if (validated) {
        System.out.println("Secondary dexfile mmap is clean");
      } else {
        System.out.println("ERROR: Memory mapping is missing Shared_Dirty/Private_Dirty entries");
      }

      return true;
    }

    // This test takes relies on dex2oat being skipped.
    // (enforced in 'run' file by using '--no-dex2oat'
    //
    // This could happen in a non-test situation
    // if a secondary dex file is loaded (but not yet maintenance-mode compiled)
    // with JIT.
    //
    // Or it could also happen if a secondary dex file is loaded and forced
    // into running into the interpreter (e.g. duplicate classes).
    //
    // Rather than relying on those weird fallbacks,
    // we force the runtime not to dex2oat the dex file to ensure
    // this test is repeatable and less brittle.
    private static void testDexMemoryMaps() throws Exception {
        // Ensure that the secondary dex file is mapped clean (directly from JAR file).
        String smaps = new String(Files.readAllBytes(Paths.get("/proc/self/smaps")));

        String[] smapsLines = smaps.split("\n");
        boolean found = true;
        for (int i = 0; i < smapsLines.length; ++i) {
          if (smapsLines[i].contains(CLASS_PATH)) {
            if (checkSmapsEntry(smapsLines, i)) {
              return;
            } // else we found the wrong one, keep going.
          }
        }

        // Error case:
        System.out.println("Could not find " + CLASS_PATH + " RO-anonymous smaps entry");
        System.out.println(smaps);
    }

    private static Class<?> testDexFile() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> DexFile = classLoader.loadClass("dalvik.system.DexFile");
        Method DexFile_loadDex = DexFile.getMethod("loadDex",
                                                   String.class,
                                                   String.class,
                                                   Integer.TYPE);
        Method DexFile_entries = DexFile.getMethod("entries");
        Object dexFile = DexFile_loadDex.invoke(null, CLASS_PATH, null, 0);
        Enumeration<String> e = (Enumeration<String>) DexFile_entries.invoke(dexFile);
        while (e.hasMoreElements()) {
            String className = e.nextElement();
            System.out.println(className);
        }

        Method DexFile_loadClass = DexFile.getMethod("loadClass",
                                                     String.class,
                                                     ClassLoader.class);
        Class<?> AnotherClass = (Class<?>)DexFile_loadClass.invoke(dexFile,
            "Another", Main.class.getClassLoader());
        return AnotherClass;
    }
}
