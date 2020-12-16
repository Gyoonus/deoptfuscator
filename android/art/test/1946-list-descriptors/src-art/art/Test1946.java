/*
 * Copyright (C) 2016 The Android Open Source Project
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

package art;

import java.util.*;
import java.lang.reflect.*;
import java.nio.ByteBuffer;
import dalvik.system.InMemoryDexClassLoader;

public class Test1946 {
  // Base64 encoded dex file containing the following classes. Note the class E cannot be loaded.
  // public class A {}
  // public class B {}
  // public class C {}
  // public class D {}
  // public class E extends ClassNotThere {}
  private static final byte[] TEST_CLASSES = Base64.getDecoder().decode(
      "ZGV4CjAzNQDzTO8rVDlKlz80vQF4NLYV5MjMMjHlOtRoAwAAcAAAAHhWNBIAAAAAAAAAAOACAAAO" +
      "AAAAcAAAAAgAAACoAAAAAQAAAMgAAAAAAAAAAAAAAAcAAADUAAAABQAAAAwBAAC8AQAArAEAACQC" +
      "AAAsAgAANAIAADwCAABEAgAATAIAAFQCAABZAgAAXgIAAGMCAAB0AgAAeQIAAH4CAACSAgAABgAA" +
      "AAcAAAAIAAAACQAAAAoAAAALAAAADAAAAA0AAAANAAAABwAAAAAAAAAAAAAAAAAAAAEAAAAAAAAA" +
      "AgAAAAAAAAADAAAAAAAAAAQAAAAAAAAABQAAAAAAAAAGAAAAAAAAAAAAAAAAAAAABgAAAAAAAAAB" +
      "AAAAAAAAAK4CAAAAAAAAAQAAAAAAAAAGAAAAAAAAAAIAAAAAAAAAuAIAAAAAAAACAAAAAAAAAAYA" +
      "AAAAAAAAAwAAAAAAAADCAgAAAAAAAAQAAAAAAAAABgAAAAAAAAAEAAAAAAAAAMwCAAAAAAAABQAA" +
      "AAAAAAADAAAAAAAAAAUAAAAAAAAA1gIAAAAAAAABAAEAAQAAAJUCAAAEAAAAcBAGAAAADgABAAEA" +
      "AQAAAJoCAAAEAAAAcBAGAAAADgABAAEAAQAAAJ8CAAAEAAAAcBAGAAAADgABAAEAAQAAAKQCAAAE" +
      "AAAAcBAGAAAADgABAAEAAQAAAKkCAAAEAAAAcBADAAAADgAGPGluaXQ+AAZBLmphdmEABkIuamF2" +
      "YQAGQy5qYXZhAAZELmphdmEABkUuamF2YQADTEE7AANMQjsAA0xDOwAPTENsYXNzTm90VGhlcmU7" +
      "AANMRDsAA0xFOwASTGphdmEvbGFuZy9PYmplY3Q7AAFWAAEABw4AAQAHDgABAAcOAAEABw4AAQAH" +
      "DgAAAAEAAICABKwDAAABAAGAgATEAwAAAQACgIAE3AMAAAEABICABPQDAAABAAWAgASMBAsAAAAA" +
      "AAAAAQAAAAAAAAABAAAADgAAAHAAAAACAAAACAAAAKgAAAADAAAAAQAAAMgAAAAFAAAABwAAANQA" +
      "AAAGAAAABQAAAAwBAAABIAAABQAAAKwBAAACIAAADgAAACQCAAADIAAABQAAAJUCAAAAIAAABQAA" +
      "AK4CAAAAEAAAAQAAAOACAAA=");
  public class TMP1 {}
  public class TMP2 {}
  public class TMP3 extends ArrayList {}

  private static void check(boolean b, String msg) {
    if (!b) {
      throw new Error("Test failed! " + msg);
    }
  }

  private static <T> void checkEq(T[] full, T[] sub, String msg) {
    List<T> f = Arrays.asList(full);
    check(full.length == sub.length, "not equal length");
    msg = Arrays.toString(full) + " is not same as " + Arrays.toString(sub) + ": " + msg;
    check(Arrays.asList(full).containsAll(Arrays.asList(sub)), msg);
  }

  private static <T> void checkSubset(T[] full, T[] sub, String msg) {
    msg = Arrays.toString(full) + " does not contain all of " + Arrays.toString(sub) + ": " + msg;
    check(Arrays.asList(full).containsAll(Arrays.asList(sub)), msg);
  }

  public static void run() throws Exception {
    initializeTest();
    // Check a few random classes in BCP.
    checkSubset(getClassloaderDescriptors(null),
        new String[] { "Ljava/lang/String;", "Ljava/util/TreeSet;" },
        "Missing entries for null classloader.");
    // Make sure that null is the same as BootClassLoader
    checkEq(getClassloaderDescriptors(null),
        getClassloaderDescriptors(Object.class.getClassLoader()), "Object not in bcp!");
    // Check the current class loader gets expected classes.
    checkSubset(getClassloaderDescriptors(Test1946.class.getClassLoader()),
        new String[] {
          "Lart/Test1946;",
          "Lart/Test1946$TMP1;",
          "Lart/Test1946$TMP2;",
          "Lart/Test1946$TMP3;"
        },
        "Missing entries for current class classloader.");
    // Check that the result is exactly what we expect and includes classes that fail verification.
    checkEq(getClassloaderDescriptors(makeClassLoaderFrom(TEST_CLASSES,
            ClassLoader.getSystemClassLoader())),
        new String[] { "LA;", "LB;", "LC;", "LD;", "LE;" },
        "Unexpected classes in custom classloader");
    checkEq(getClassloaderDescriptors(makeClassLoaderFrom(TEST_CLASSES,
            Object.class.getClassLoader())),
        new String[] { "LA;", "LB;", "LC;", "LD;", "LE;" },
        "Unexpected classes in custom classloader");
    checkEq(getClassloaderDescriptors(makeClassLoaderFrom(TEST_CLASSES,
            Test1946.class.getClassLoader())),
        new String[] { "LA;", "LB;", "LC;", "LD;", "LE;" },
        "Unexpected classes in custom classloader");
    // Check we only get 1 copy of each descriptor.
    checkEq(getClassloaderDescriptors(makeClassLoaderFrom(Arrays.asList(TEST_CLASSES, TEST_CLASSES),
            Test1946.class.getClassLoader())),
        new String[] { "LA;", "LB;", "LC;", "LD;", "LE;" },
        "Unexpected classes in custom classloader");
    System.out.println("Passed!");
  }

  private static ClassLoader makeClassLoaderFrom(byte[] data, ClassLoader parent) throws Exception {
    return new InMemoryDexClassLoader(ByteBuffer.wrap(data), parent);
  }

  private static ClassLoader makeClassLoaderFrom(List<byte[]> data, ClassLoader parent)
      throws Exception {
    ArrayList<ByteBuffer> bufs = new ArrayList<>();
    for (byte[] d : data) {
      bufs.add(ByteBuffer.wrap(d));
    }
    return new InMemoryDexClassLoader(bufs.toArray(new ByteBuffer[0]), parent);
  }

  private static native void initializeTest();
  private static native String[] getClassloaderDescriptors(ClassLoader loader);
}
