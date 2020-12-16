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

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if (!hasJit()) {
      // Make the test pass if not using JIT.
      return;
    }
    if (hasImage()) {
      throw new Error("The `run` script should prevent this test from running with an image!");
    }
    if (!isClassMoveable(String.class)) {
      throw new Error("String.class not moveable despite running without image!");
    }

    // Make sure the Main.test() is JIT-compiled and then call it.
    ensureJitCompiled(Main.class, "test");
    test();
  }

  public static void test() {
    int length = 5;

    // Hide the type of these strings in an Object array,
    // so that we treat them as Object for the String.equals() below.
    Object[] array = new Object[length];
    for (int i = 0; i != length; ++i) {
      array[i] = "V" + i;
    }

    // Continually check string equality between a newly allocated String and an
    // already allocated String with the same contents while allocating over 128MiB
    // memory (with heap size limited to 16MiB), ensuring we run GC and stress the
    // instanceof check in the String.equals() implementation.
    for (int count = 0; count != 128 * 1024; ++count) {
      for (int i = 0; i != length; ++i) {
        allocateAtLeast1KiB();
        assertTrue(("V" + i).equals(array[i]));
      }
    }
  }

  public static void allocateAtLeast1KiB() {
    // Give GC more work by allocating Object arrays.
    memory[allocationIndex] = new Object[1024 / 4];
    ++allocationIndex;
    if (allocationIndex == memory.length) {
      allocationIndex = 0;
    }
  }

  public static void assertTrue(boolean value) {
    if (!value) {
      throw new Error("Assertion failed!");
    }
  }

  private native static boolean hasJit();
  private native static boolean hasImage();
  private native static boolean isClassMoveable(Class<?> cls);
  private static native void ensureJitCompiled(Class<?> itf, String method_name);

  // We shall retain some allocated memory and release old allocations
  // so that the GC has something to do.
  public static Object[] memory = new Object[4096];
  public static int allocationIndex = 0;
}
