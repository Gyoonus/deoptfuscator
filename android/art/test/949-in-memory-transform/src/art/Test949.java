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

package art;

import static art.Redefinition.doCommonClassRedefinition;

import java.util.Base64;
import java.lang.reflect.*;
import java.nio.ByteBuffer;

public class Test949 {
  /**
   * base64 encoded class/dex file for
   * public class Transform {
   *   public void sayHi() {
   *    System.out.println("hello");
   *   }
   * }
   */
  private static final byte[] INITIAL_CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAHAoABgAOCQAPABAIABEKABIAEwcAFAcAFQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwA" +
    "BwAIBwAWDAAXABgBAAVoZWxsbwcAGQwAGgAbAQAJVHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVj" +
    "dAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZh" +
    "L2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgAhAAUABgAA" +
    "AAAAAgABAAcACAABAAkAAAAdAAEAAQAAAAUqtwABsQAAAAEACgAAAAYAAQAAABEAAQALAAgAAQAJ" +
    "AAAAJQACAAEAAAAJsgACEgO2AASxAAAAAQAKAAAACgACAAAAGgAIABsAAQAMAAAAAgAN");
  private static final byte[] INITIAL_DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAJX3mZphwHJCT1qdTz/GS+jXOR+O/9e3fMAgAAcAAAAHhWNBIAAAAAAAAAACwCAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAACsAQAAIAEAAGIB" +
    "AABqAQAAdwEAAI4BAACiAQAAtgEAAMoBAADaAQAA3QEAAOEBAAD1AQAA/AEAAAECAAAKAgAAAQAA" +
    "AAIAAAADAAAABAAAAAUAAAAHAAAABwAAAAUAAAAAAAAACAAAAAUAAABcAQAABAABAAsAAAAAAAAA" +
    "AAAAAAAAAAANAAAAAQABAAwAAAACAAAAAAAAAAAAAAABAAAAAgAAAAAAAAAGAAAAAAAAABwCAAAA" +
    "AAAAAQABAAEAAAARAgAABAAAAHAQAwAAAA4AAwABAAIAAAAWAgAACQAAAGIAAAAbAQoAAABuIAIA" +
    "EAAOAAAAAQAAAAMABjxpbml0PgALTFRyYW5zZm9ybTsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwAS" +
    "TGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVt" +
    "OwAOVHJhbnNmb3JtLmphdmEAAVYAAlZMABJlbWl0dGVyOiBqYWNrLTQuMjUABWhlbGxvAANvdXQA" +
    "B3ByaW50bG4ABXNheUhpABEABw4AGgAHDocAAAABAQCBgASgAgEBuAIAAA0AAAAAAAAAAQAAAAAA" +
    "AAABAAAADgAAAHAAAAACAAAABgAAAKgAAAADAAAAAgAAAMAAAAAEAAAAAQAAANgAAAAFAAAABAAA" +
    "AOAAAAAGAAAAAQAAAAABAAABIAAAAgAAACABAAABEAAAAQAAAFwBAAACIAAADgAAAGIBAAADIAAA" +
    "AgAAABECAAAAIAAAAQAAABwCAAAAEAAAAQAAACwCAAA=");


  /**
   * base64 encoded class/dex file for
   * public class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] TRANSFORMED_CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAHAoABgAOCQAPABAIABEKABIAEwcAFAcAFQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwA" +
    "BwAIBwAWDAAXABgBAAdHb29kYnllBwAZDAAaABsBAAlUcmFuc2Zvcm0BABBqYXZhL2xhbmcvT2Jq" +
    "ZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEAE2ph" +
    "dmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExqYXZhL2xhbmcvU3RyaW5nOylWACEABQAG" +
    "AAAAAAACAAEABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAAEQABAAsACAAB" +
    "AAkAAAAlAAIAAQAAAAmyAAISA7YABLEAAAABAAoAAAAKAAIAAAAaAAgAGwABAAwAAAACAA0=");
  private static final byte[] TRANSFORMED_DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAPXh6T3l1FObhHsKf1U2vi+0GmAvElxBLMAgAAcAAAAHhWNBIAAAAAAAAAACwCAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAACsAQAAIAEAAGIB" +
    "AABqAQAAcwEAAIABAACXAQAAqwEAAL8BAADTAQAA4wEAAOYBAADqAQAA/gEAAAMCAAAMAgAAAgAA" +
    "AAMAAAAEAAAABQAAAAYAAAAIAAAACAAAAAUAAAAAAAAACQAAAAUAAABcAQAABAABAAsAAAAAAAAA" +
    "AAAAAAAAAAANAAAAAQABAAwAAAACAAAAAAAAAAAAAAABAAAAAgAAAAAAAAAHAAAAAAAAAB4CAAAA" +
    "AAAAAQABAAEAAAATAgAABAAAAHAQAwAAAA4AAwABAAIAAAAYAgAACQAAAGIAAAAbAQEAAABuIAIA" +
    "EAAOAAAAAQAAAAMABjxpbml0PgAHR29vZGJ5ZQALTFRyYW5zZm9ybTsAFUxqYXZhL2lvL1ByaW50" +
    "U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xh" +
    "bmcvU3lzdGVtOwAOVHJhbnNmb3JtLmphdmEAAVYAAlZMABJlbWl0dGVyOiBqYWNrLTQuMjUAA291" +
    "dAAHcHJpbnRsbgAFc2F5SGkAEQAHDgAaAAcOhwAAAAEBAIGABKACAQG4Ag0AAAAAAAAAAQAAAAAA" +
    "AAABAAAADgAAAHAAAAACAAAABgAAAKgAAAADAAAAAgAAAMAAAAAEAAAAAQAAANgAAAAFAAAABAAA" +
    "AOAAAAAGAAAAAQAAAAABAAABIAAAAgAAACABAAABEAAAAQAAAFwBAAACIAAADgAAAGIBAAADIAAA" +
    "AgAAABMCAAAAIAAAAQAAAB4CAAAAEAAAAQAAACwCAAA=");

  public static void run() throws Exception {
    ClassLoader loader;
    try {
      // Art uses this classloader to do in-memory dex files. There is no support for defineClass
      loader = (ClassLoader)Class.forName("dalvik.system.InMemoryDexClassLoader")
                                 .getConstructor(ByteBuffer.class, ClassLoader.class)
                                 .newInstance(ByteBuffer.wrap(INITIAL_DEX_BYTES),
                                              ClassLoader.getSystemClassLoader());
    } catch (ClassNotFoundException e) {
      // Seem to be on RI. Just make a new ClassLoader that calls defineClass.
      loader = new ClassLoader() {
        public Class<?> findClass(String name) throws ClassNotFoundException {
          if (name.equals("Transform")) {
            return defineClass(name, INITIAL_CLASS_BYTES, 0, INITIAL_CLASS_BYTES.length);
          } else {
            throw new ClassNotFoundException("Couldn't find class: " + name);
          }
        }
      };
    }
    doTest(loader);
  }

  public static void doTest(ClassLoader loader) throws Exception {
    // Get the class
    Class<?> transform_class = loader.loadClass("Transform");
    Method say_hi_method = transform_class.getMethod("sayHi");
    Object t = transform_class.newInstance();

    // Run the actual test.
    say_hi_method.invoke(t);
    doCommonClassRedefinition(transform_class, TRANSFORMED_CLASS_BYTES, TRANSFORMED_DEX_BYTES);
    say_hi_method.invoke(t);
  }
}
