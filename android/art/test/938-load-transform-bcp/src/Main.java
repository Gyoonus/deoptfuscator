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

import static art.Redefinition.*;
import java.lang.reflect.*;
import java.util.Base64;

class Main {
  public static String TEST_NAME = "938-load-transform-bcp";

  /**
   * base64 encoded class/dex file for
   *
   * // Yes this version of OptionalLong is not compatible with the real one but since it isn't used
   * // for anything in the runtime initialization it should be fine.
   *
   * package java.util;
   * public final class OptionalLong {
   *   private long val;
   *
   *   private OptionalLong(long abc) {
   *     this.val = abc;
   *   }
   *
   *   public static OptionalLong of(long abc) {
   *     return new OptionalLong(abc);
   *   }
   *
   *   public String foo() {
   *     return "This is foo for val=" + val;
   *   }
   *
   *   public String toString() {
   *     return "This is toString() for val=" + val;
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAKQoADAAaCQADABsHABwKAAMAHQcAHgoABQAaCAAfCgAFACAKAAUAIQoABQAiCAAj" +
    "BwAkAQADdmFsAQABSgEABjxpbml0PgEABChKKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAC" +
    "b2YBABsoSilMamF2YS91dGlsL09wdGlvbmFsTG9uZzsBAANmb28BABQoKUxqYXZhL2xhbmcvU3Ry" +
    "aW5nOwEACHRvU3RyaW5nAQAKU291cmNlRmlsZQEAEU9wdGlvbmFsTG9uZy5qYXZhDAAPACUMAA0A" +
    "DgEAFmphdmEvdXRpbC9PcHRpb25hbExvbmcMAA8AEAEAF2phdmEvbGFuZy9TdHJpbmdCdWlsZGVy" +
    "AQAUVGhpcyBpcyBmb28gZm9yIHZhbD0MACYAJwwAJgAoDAAXABYBABtUaGlzIGlzIHRvU3RyaW5n" +
    "KCkgZm9yIHZhbD0BABBqYXZhL2xhbmcvT2JqZWN0AQADKClWAQAGYXBwZW5kAQAtKExqYXZhL2xh" +
    "bmcvU3RyaW5nOylMamF2YS9sYW5nL1N0cmluZ0J1aWxkZXI7AQAcKEopTGphdmEvbGFuZy9TdHJp" +
    "bmdCdWlsZGVyOwAxAAMADAAAAAEAAgANAA4AAAAEAAIADwAQAAEAEQAAACoAAwADAAAACiq3AAEq" +
    "H7UAArEAAAABABIAAAAOAAMAAAAFAAQABgAJAAcACQATABQAAQARAAAAIQAEAAIAAAAJuwADWR63" +
    "AASwAAAAAQASAAAABgABAAAACgABABUAFgABABEAAAAvAAMAAQAAABe7AAVZtwAGEge2AAgqtAAC" +
    "tgAJtgAKsAAAAAEAEgAAAAYAAQAAAA4AAQAXABYAAQARAAAALwADAAEAAAAXuwAFWbcABhILtgAI" +
    "KrQAArYACbYACrAAAAABABIAAAAGAAEAAAASAAEAGAAAAAIAGQ==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAOe/TYJCvVthTToFA3tveMDhwTo7uDf0IcBAAAcAAAAHhWNBIAAAAAAAAAAHwDAAAU" +
    "AAAAcAAAAAYAAADAAAAABgAAANgAAAABAAAAIAEAAAkAAAAoAQAAAQAAAHABAACMAgAAkAEAAFYC" +
    "AABeAgAAYQIAAGQCAABoAgAAbAIAAIACAACUAgAArwIAAMkCAADcAgAA8gIAAA8DAAASAwAAFgMA" +
    "AB4DAAAyAwAANwMAADsDAABFAwAAAQAAAAUAAAAGAAAABwAAAAgAAAAMAAAAAgAAAAIAAAAAAAAA" +
    "AwAAAAMAAABIAgAABAAAAAMAAABQAgAAAwAAAAQAAABIAgAADAAAAAUAAAAAAAAADQAAAAUAAABI" +
    "AgAABAAAABMAAAABAAQAAAAAAAMABAAAAAAAAwABAA4AAAADAAIADgAAAAMAAAASAAAABAAFAAAA" +
    "AAAEAAAAEAAAAAQAAwARAAAABAAAABIAAAAEAAAAEQAAAAEAAAAAAAAACQAAAAAAAABiAwAAAAAA" +
    "AAQAAwABAAAASgMAAAYAAABwEAAAAQBaEgAADgAEAAIAAwAAAFIDAAAGAAAAIgAEAHAwBQAgAxEA" +
    "BQABAAMAAABYAwAAFwAAACIAAwBwEAEAAAAbAQoAAABuIAMAEAAMAFNCAABuMAIAIAMMAG4QBAAA" +
    "AAwAEQAAAAUAAQADAAAAXQMAABcAAAAiAAMAcBABAAAAGwELAAAAbiADABAADABTQgAAbjACACAD" +
    "DABuEAQAAAAMABEAAAABAAAAAAAAAAEAAAACAAY8aW5pdD4AAUoAAUwAAkxKAAJMTAASTGphdmEv" +
    "bGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAGUxqYXZhL2xhbmcvU3RyaW5nQnVpbGRl" +
    "cjsAGExqYXZhL3V0aWwvT3B0aW9uYWxMb25nOwART3B0aW9uYWxMb25nLmphdmEAFFRoaXMgaXMg" +
    "Zm9vIGZvciB2YWw9ABtUaGlzIGlzIHRvU3RyaW5nKCkgZm9yIHZhbD0AAVYAAlZKAAZhcHBlbmQA" +
    "EmVtaXR0ZXI6IGphY2stNC4yMgADZm9vAAJvZgAIdG9TdHJpbmcAA3ZhbAAFAQAHDjwtAAoBAAcO" +
    "AA4ABw4AEgAHDgAAAQICAAIFgoAEkAMCCawDBgHIAwIBiAQAAA0AAAAAAAAAAQAAAAAAAAABAAAA" +
    "FAAAAHAAAAACAAAABgAAAMAAAAADAAAABgAAANgAAAAEAAAAAQAAACABAAAFAAAACQAAACgBAAAG" +
    "AAAAAQAAAHABAAABIAAABAAAAJABAAABEAAAAgAAAEgCAAACIAAAFAAAAFYCAAADIAAABAAAAEoD" +
    "AAAAIAAAAQAAAGIDAAAAEAAAAQAAAHwDAAA=");

  public static ClassLoader getClassLoaderFor(String location) throws Exception {
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(String.class, ClassLoader.class);
      return (ClassLoader)ctor.newInstance(location + "/" + TEST_NAME + "-ex.jar",
                                           Main.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI. Use URLClassLoader.
      return new java.net.URLClassLoader(
          new java.net.URL[] { new java.net.URL("file://" + location + "/classes-ex/") });
    }
  }

  public static void main(String[] args) {
    setPopRetransformations(false);
    addCommonTransformationResult("java/util/OptionalLong", CLASS_BYTES, DEX_BYTES);
    enableCommonRetransformation(true);
    try {
      /* this is the "alternate" DEX/Jar file */
      ClassLoader new_loader = getClassLoaderFor(System.getenv("DEX_LOCATION"));
      Class<?> klass = (Class<?>)new_loader.loadClass("TestMain");
      if (klass == null) {
        throw new AssertionError("loadClass failed");
      }
      Method run_test = klass.getMethod("runTest");
      run_test.invoke(null);
    } catch (Exception e) {
      System.out.println(e.toString());
      e.printStackTrace(System.out);
    }
  }
}
