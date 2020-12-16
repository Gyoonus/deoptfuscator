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

import AAA.Derived;

public class Main {
    public static void main(String[] args) {
        try {
            // Allocate memory for the "AAA.Derived" class name before eating memory.
            String aaaDerivedName = "AAA.Derived";
            System.out.println("Eating all memory.");
            // Resolve VMClassLoader before eating all the memory since we can not fail
            // initializtaion of boot classpath classes.
            Class.forName("java.lang.VMClassLoader");
            Object memory = eatAllMemory();

            // This test assumes that Derived is not yet resolved. In some configurations
            // (notably interp-ac), Derived is already resolved by verifying Main at run
            // time. Therefore we cannot assume that we get a certain `value` and need to
            // simply check for consistency, i.e. `value == another_value`.
            int value = 0;
            try {
                // If the ArtMethod* is erroneously left in the DexCache, this
                // shall succeed despite the class Derived being unresolved so
                // far. Otherwise, we shall throw OOME trying to resolve it.
                value = Derived.foo();
            } catch (OutOfMemoryError e) {
                value = -1;
            }
            int another_value = 0;
            try {
                // For comparison, try to resolve the class Derived directly.
                Class.forName(aaaDerivedName, false, Main.class.getClassLoader());
                another_value = 42;
            } catch (OutOfMemoryError e) {
                another_value = -1;
            }
            boolean memoryWasAllocated = (memory != null);
            memory = null;
            System.out.println("memoryWasAllocated = " + memoryWasAllocated);
            System.out.println("match: " + (value == another_value));
            if (value != another_value || (value != -1 && value != 42)) {
                // Mismatch or unexpected value, print additional debugging information.
                System.out.println("value: " + value);
                System.out.println("another_value: " + another_value);
            }
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    public static Object eatAllMemory() {
      Object[] result = null;
      int size = 1000000;
      while (result == null && size != 0) {
          try {
              result = new Object[size];
          } catch (OutOfMemoryError oome) {
              size /= 2;
          }
      }
      if (result != null) {
          int index = 0;
          while (index != result.length && size != 0) {
              try {
                  result[index] = new byte[size];
                  ++index;
              } catch (OutOfMemoryError oome) {
                  size /= 2;
              }
          }
      }
      return result;
  }
}
