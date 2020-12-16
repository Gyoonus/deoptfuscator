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

import java.lang.reflect.*;
import java.util.Base64;

import art.Redefinition;

class Main {
  public static String TEST_NAME = "935-non-retransformable";

  /**
   * base64 encoded class/dex file for
   * class Transform {
   *   public void sayHi() {
   *     System.out.println("Hello");
   *   }
   *   public void sayGoodbye() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAHwoABwAQCQARABIIABMKABQAFQgAFgcAFwcAGAEABjxpbml0PgEAAygpVgEABENv" +
    "ZGUBAA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEACnNheUdvb2RieWUBAApTb3VyY2VGaWxlAQAO" +
    "VHJhbnNmb3JtLmphdmEMAAgACQcAGQwAGgAbAQAFSGVsbG8HABwMAB0AHgEAB0dvb2RieWUBAAlU" +
    "cmFuc2Zvcm0BABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxq" +
    "YXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExq" +
    "YXZhL2xhbmcvU3RyaW5nOylWACAABgAHAAAAAAADAAAACAAJAAEACgAAAB0AAQABAAAABSq3AAGx" +
    "AAAAAQALAAAABgABAAAAAQABAAwACQABAAoAAAAlAAIAAQAAAAmyAAISA7YABLEAAAABAAsAAAAK" +
    "AAIAAAADAAgABAABAA0ACQABAAoAAAAlAAIAAQAAAAmyAAISBbYABLEAAAABAAsAAAAKAAIAAAAG" +
    "AAgABwABAA4AAAACAA8=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDpaN+7jX/ZLl9Jr0HAEV7nqL1YDuakKakgAwAAcAAAAHhWNBIAAAAAAAAAAIACAAAQ" +
    "AAAAcAAAAAYAAACwAAAAAgAAAMgAAAABAAAA4AAAAAUAAADoAAAAAQAAABABAADwAQAAMAEAAJYB" +
    "AACeAQAApwEAAK4BAAC7AQAA0gEAAOYBAAD6AQAADgIAAB4CAAAhAgAAJQIAADkCAAA+AgAARwIA" +
    "AFMCAAADAAAABAAAAAUAAAAGAAAABwAAAAkAAAAJAAAABQAAAAAAAAAKAAAABQAAAJABAAAEAAEA" +
    "DAAAAAAAAAAAAAAAAAAAAA4AAAAAAAAADwAAAAEAAQANAAAAAgAAAAAAAAAAAAAAAAAAAAIAAAAA" +
    "AAAACAAAAAAAAABrAgAAAAAAAAEAAQABAAAAWgIAAAQAAABwEAQAAAAOAAMAAQACAAAAXwIAAAkA" +
    "AABiAAAAGwEBAAAAbiADABAADgAAAAMAAQACAAAAZQIAAAkAAABiAAAAGwECAAAAbiADABAADgAA" +
    "AAEAAAADAAY8aW5pdD4AB0dvb2RieWUABUhlbGxvAAtMVHJhbnNmb3JtOwAVTGphdmEvaW8vUHJp" +
    "bnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEv" +
    "bGFuZy9TeXN0ZW07AA5UcmFuc2Zvcm0uamF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2stNC4yMgAD" +
    "b3V0AAdwcmludGxuAApzYXlHb29kYnllAAVzYXlIaQABAAcOAAYABw6HAAMABw6HAAAAAQIAgIAE" +
    "sAIBAcgCAQHsAgAAAA0AAAAAAAAAAQAAAAAAAAABAAAAEAAAAHAAAAACAAAABgAAALAAAAADAAAA" +
    "AgAAAMgAAAAEAAAAAQAAAOAAAAAFAAAABQAAAOgAAAAGAAAAAQAAABABAAABIAAAAwAAADABAAAB" +
    "EAAAAQAAAJABAAACIAAAEAAAAJYBAAADIAAAAwAAAFoCAAAAIAAAAQAAAGsCAAAAEAAAAQAAAIAC" +
    "AAA=");


  public static ClassLoader getClassLoaderFor(String location) throws Exception {
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(String.class, ClassLoader.class);
      /* on Dalvik, this is a DexFile; otherwise, it's null */
      return (ClassLoader)ctor.newInstance(location + "/" + TEST_NAME + "-ex.jar",
                                           Main.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI. Use URLClassLoader.
      return new java.net.URLClassLoader(
          new java.net.URL[] { new java.net.URL("file://" + location + "/classes-ex/") });
    }
  }

  public static void main(String[] args) {
    Redefinition.setPopRetransformations(false);
    Redefinition.addCommonTransformationResult("Transform", CLASS_BYTES, DEX_BYTES);
    Redefinition.enableCommonRetransformation(true);
    try {
      /* this is the "alternate" DEX/Jar file */
      ClassLoader new_loader = getClassLoaderFor(System.getenv("DEX_LOCATION"));
      Class<?> klass = (Class<?>)new_loader.loadClass("TestMain");
      if (klass == null) {
        throw new AssertionError("loadClass failed");
      }
      Method run_test = klass.getMethod("runTest");
      run_test.invoke(null);

      // Remove the original transformation. It has been used by now.
      Redefinition.popTransformationFor("Transform");
      // Make sure we don't get called for transformation again.
      Redefinition.addCommonTransformationResult("Transform", new byte[0], new byte[0]);
      Redefinition.doCommonClassRetransformation(new_loader.loadClass("Transform"));
      run_test.invoke(null);
    } catch (Exception e) {
      System.out.println(e.toString());
      e.printStackTrace(System.out);
    }
  }
}
