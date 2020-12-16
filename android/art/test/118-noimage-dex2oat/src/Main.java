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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    boolean hasImage = hasImage();
    String instructionSet = VMRuntime.getCurrentInstructionSet();
    boolean isBootClassPathOnDisk = VMRuntime.isBootClassPathOnDisk(instructionSet);
    System.out.println(
        "Has image is " + hasImage + ", is image dex2oat enabled is "
        + isImageDex2OatEnabled() + ", is BOOTCLASSPATH on disk is "
        + isBootClassPathOnDisk + ".");

    if (hasImage && !isImageDex2OatEnabled()) {
      throw new Error("Image with dex2oat disabled runs with an oat file");
    } else if (!hasImage && isImageDex2OatEnabled()) {
      throw new Error("Image with dex2oat enabled runs without an oat file");
    }
    if (hasImage && !isBootClassPathOnDisk) {
      throw new Error("Image with dex2oat disabled runs with an image file");
    } else if (!hasImage && isBootClassPathOnDisk) {
      throw new Error("Image with dex2oat enabled runs without an image file");
    }

    testB18485243();
  }

  private native static boolean hasImage();

  private native static boolean isImageDex2OatEnabled();

  private static class VMRuntime {
    private static final Method getCurrentInstructionSetMethod;
    private static final Method isBootClassPathOnDiskMethod;
    static {
        try {
            Class<?> c = Class.forName("dalvik.system.VMRuntime");
            getCurrentInstructionSetMethod = c.getDeclaredMethod("getCurrentInstructionSet");
            isBootClassPathOnDiskMethod = c.getDeclaredMethod("isBootClassPathOnDisk",
                                                              String.class);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static String getCurrentInstructionSet() throws Exception {
      return (String) getCurrentInstructionSetMethod.invoke(null);
    }
    public static boolean isBootClassPathOnDisk(String instructionSet) throws Exception {
      return (boolean) isBootClassPathOnDiskMethod.invoke(null, instructionSet);
    }
  }

  private static void testB18485243() throws Exception {
    Class<?> k = Class.forName("B18485243");
    Object o = k.newInstance();
    Method m = k.getDeclaredMethod("run");
    try {
      m.invoke(o);
    } catch (InvocationTargetException e) {
      Throwable actual = e.getTargetException();
      if (!(actual instanceof IncompatibleClassChangeError)) {
        throw new AssertionError("Expected IncompatibleClassChangeError", actual);
      }
    }
    System.out.println("testB18485243 PASS");
  }
}
