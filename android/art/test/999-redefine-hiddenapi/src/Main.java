/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.io.File;
import java.lang.reflect.Method;
import java.util.Base64;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    // Run the initialization routine. This will enable hidden API checks in
    // the runtime, in case they are not enabled by default.
    init();

    // Load the '-ex' APK and attach it to the boot class path.
    appendToBootClassLoader(DEX_EXTRA);

    // Find the test class in boot class loader and verify that its members are hidden.
    Class<?> klass = Class.forName("art.Test999", true, BOOT_CLASS_LOADER);
    assertMethodIsHidden(klass, "before redefinition");
    assertFieldIsHidden(klass, "before redefinition");

    // Redefine the class using JVMTI.
    art.Redefinition.setTestConfiguration(art.Redefinition.Config.COMMON_REDEFINE);
    art.Redefinition.doCommonClassRedefinition(klass, CLASS_BYTES, DEX_BYTES);

    // Verify that the class members are still hidden.
    assertMethodIsHidden(klass, "after redefinition");
    assertFieldIsHidden(klass, "after redefinition");
  }

  private static void assertMethodIsHidden(Class<?> klass, String msg) throws Exception {
    try {
      klass.getDeclaredMethod("foo");
      // Unexpected. Should have thrown NoSuchMethodException.
      throw new Exception("Method should not be accessible " + msg);
    } catch (NoSuchMethodException ex) {
      // Expected.
    }
  }

  private static void assertFieldIsHidden(Class<?> klass, String msg) throws Exception {
    try {
      klass.getDeclaredField("bar");
      // Unexpected. Should have thrown NoSuchFieldException.
      throw new Exception("Field should not be accessible " + msg);
    } catch (NoSuchFieldException ex) {
      // Expected.
    }
  }

  private static final String DEX_EXTRA =
      new File(System.getenv("DEX_LOCATION"), "999-redefine-hiddenapi-ex.jar").getAbsolutePath();

  private static ClassLoader BOOT_CLASS_LOADER = Object.class.getClassLoader();

  // Native functions. Note that these are implemented in 674-hiddenapi/hiddenapi.cc.
  private static native void appendToBootClassLoader(String dexPath);
  private static native void init();

  /**
   * base64 encoded class/dex file for
   *
   * public class Test999 {
   *   public void foo() {
   *     System.out.println("Goodbye");
   *   }
   *
   *   public int bar = 64;
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADUAIAoABwARCQAGABIJABMAFAgAFQoAFgAXBwAYBwAZAQADYmFyAQABSQEABjxpbml0" +
    "PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAANmb28BAApTb3VyY2VGaWxlAQAMVGVz" +
    "dDk5OS5qYXZhDAAKAAsMAAgACQcAGgwAGwAcAQAHR29vZGJ5ZQcAHQwAHgAfAQALYXJ0L1Rlc3Q5" +
    "OTkBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxqYXZhL2lv" +
    "L1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExqYXZhL2xh" +
    "bmcvU3RyaW5nOylWACEABgAHAAAAAQABAAgACQAAAAIAAQAKAAsAAQAMAAAAJwACAAEAAAALKrcA" +
    "ASoQQLUAArEAAAABAA0AAAAKAAIAAAATAAQAGAABAA4ACwABAAwAAAAlAAIAAQAAAAmyAAMSBLYA" +
    "BbEAAAABAA0AAAAKAAIAAAAVAAgAFgABAA8AAAACABA=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQD0dZ+IWxOi+cJDSWjfTnUerlZj1Lll3ONIAwAAcAAAAHhWNBIAAAAAAAAAAJwCAAAQ" +
    "AAAAcAAAAAcAAACwAAAAAgAAAMwAAAACAAAA5AAAAAQAAAD0AAAAAQAAABQBAAAUAgAANAEAAIYB" +
    "AACOAQAAlwEAAJoBAACpAQAAwAEAANQBAADoAQAA/AEAAAoCAAANAgAAEQIAABYCAAAbAgAAIAIA" +
    "ACkCAAACAAAAAwAAAAQAAAAFAAAABgAAAAcAAAAJAAAACQAAAAYAAAAAAAAACgAAAAYAAACAAQAA" +
    "AQAAAAsAAAAFAAIADQAAAAEAAAAAAAAAAQAAAAwAAAACAAEADgAAAAMAAAAAAAAAAQAAAAEAAAAD" +
    "AAAAAAAAAAgAAAAAAAAAhwIAAAAAAAACAAEAAQAAAHQBAAAIAAAAcBADAAEAEwBAAFkQAAAOAAMA" +
    "AQACAAAAeQEAAAgAAABiAAEAGgEBAG4gAgAQAA4AEwAOQAAVAA54AAAAAQAAAAQABjxpbml0PgAH" +
    "R29vZGJ5ZQABSQANTGFydC9UZXN0OTk5OwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9s" +
    "YW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0" +
    "OTk5LmphdmEAAVYAAlZMAANiYXIAA2ZvbwADb3V0AAdwcmludGxuAFx+fkQ4eyJtaW4tYXBpIjox" +
    "LCJzaGEtMSI6IjU2YzJlMzBmNTIzM2I4NDRmZjZkZGQ4N2ZiNTNkMzRmYjE3MjM3ZGYiLCJ2ZXJz" +
    "aW9uIjoidjEuMi4xNS1kZXYifQAAAQEBAAEAgYAEtAIBAdQCAAAAAAAOAAAAAAAAAAEAAAAAAAAA" +
    "AQAAABAAAABwAAAAAgAAAAcAAACwAAAAAwAAAAIAAADMAAAABAAAAAIAAADkAAAABQAAAAQAAAD0" +
    "AAAABgAAAAEAAAAUAQAAASAAAAIAAAA0AQAAAyAAAAIAAAB0AQAAARAAAAEAAACAAQAAAiAAABAA" +
    "AACGAQAAACAAAAEAAACHAgAAAxAAAAEAAACYAgAAABAAAAEAAACcAgAA");
}
