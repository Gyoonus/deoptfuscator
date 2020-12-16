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

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Base64;

public class Test1901 {
  // Class & Dex file containing the following class.
  // Using this representation to prevent any changes to the compiler or the file formats from
  // changing the output of this test.
  //
  // package art;
  // public class Target {
  //   public void doNothing() {
  //     return;
  //   }
  //
  //   public static void staticNothing() {
  //     return;
  //   }
  //
  //   public void sayHi() {
  //     System.out.println("hello");
  //   }
  // }
  public static byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAHgoABgAQCQARABIIABMKABQAFQcAFgcAFwEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAlkb05vdGhpbmcBAA1zdGF0aWNOb3RoaW5nAQAFc2F5SGkBAApT" +
    "b3VyY2VGaWxlAQALVGFyZ2V0LmphdmEMAAcACAcAGAwAGQAaAQAFaGVsbG8HABsMABwAHQEACmFy" +
    "dC9UYXJnZXQBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxq" +
    "YXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExq" +
    "YXZhL2xhbmcvU3RyaW5nOylWACEABQAGAAAAAAAEAAEABwAIAAEACQAAAB0AAQABAAAABSq3AAGx" +
    "AAAAAQAKAAAABgABAAAAAgABAAsACAABAAkAAAAZAAAAAQAAAAGxAAAAAQAKAAAABgABAAAABAAJ" +
    "AAwACAABAAkAAAAZAAAAAAAAAAGxAAAAAQAKAAAABgABAAAACAABAA0ACAABAAkAAAAlAAIAAQAA" +
    "AAmyAAISA7YABLEAAAABAAoAAAAKAAIAAAAMAAgADQABAA4AAAACAA8=");
  public static byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAbYkxNjiZ8a+fNWF4smR2+uXbrq88/FNoYAwAAcAAAAHhWNBIAAAAAAAAAAHgCAAAP" +
    "AAAAcAAAAAYAAACsAAAAAgAAAMQAAAABAAAA3AAAAAYAAADkAAAAAQAAABQBAADkAQAANAEAAJoB" +
    "AACiAQAAsAEAAMcBAADbAQAA7wEAAAMCAAAQAgAAEwIAABcCAAAiAgAAKQIAAC4CAAA3AgAAPgIA" +
    "AAEAAAACAAAAAwAAAAQAAAAFAAAABwAAAAcAAAAFAAAAAAAAAAgAAAAFAAAAlAEAAAQAAQALAAAA" +
    "AAAAAAAAAAAAAAAACQAAAAAAAAANAAAAAAAAAA4AAAABAAEADAAAAAIAAAAAAAAAAAAAAAEAAAAC" +
    "AAAAAAAAAAYAAAAAAAAAYgIAAAAAAAABAAEAAQAAAE0CAAAEAAAAcBAFAAAADgAAAAAAAAAAAFIC" +
    "AAABAAAADgAAAAEAAQAAAAAAVwIAAAEAAAAOAAAAAwABAAIAAABcAgAACAAAAGIAAAAaAQoAbiAE" +
    "ABAADgABAAAAAwAGPGluaXQ+AAxMYXJ0L1RhcmdldDsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwAS" +
    "TGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVt" +
    "OwALVGFyZ2V0LmphdmEAAVYAAlZMAAlkb05vdGhpbmcABWhlbGxvAANvdXQAB3ByaW50bG4ABXNh" +
    "eUhpAA1zdGF0aWNOb3RoaW5nAAIABw4ACAAHDgAEAAcOAAwABw54AAAAAgIAgYAEtAIDCcwCAQHg" +
    "AgEB9AINAAAAAAAAAAEAAAAAAAAAAQAAAA8AAABwAAAAAgAAAAYAAACsAAAAAwAAAAIAAADEAAAA" +
    "BAAAAAEAAADcAAAABQAAAAYAAADkAAAABgAAAAEAAAAUAQAAASAAAAQAAAA0AQAAARAAAAEAAACU" +
    "AQAAAiAAAA8AAACaAQAAAyAAAAQAAABNAgAAACAAAAEAAABiAgAAABAAAAEAAAB4AgAA");

  public static byte[][] DO_NOTHING_BYTECODES = new byte[][] {
    // Dex Bytecodes for doNothing
    // 0e00           |0000: return-void
    new byte[] { 14, 0 },
    // Java bytecodes
    // 0: return
    new byte[] { -79 },
  };

  public static byte[][] STATIC_NOTHING_BYTECODES = new byte[][] {
    // Dex Bytecodes for staticNothing
    // 0e00           |0000: return-void
    new byte[] { 14, 0 },
    // Java bytecodes
    // 0: return
    new byte[] { -79 },
  };

  public static byte[][] SAY_HI_NOTHING_BYTECODES = new byte[][] {
    // Dex Bytecodes for sayHi
    // 6200 0000      |0000: sget-object v0, Ljava/lang/System;.out:Ljava/io/PrintStream; // field@0000
    // 1a01 0a00      |0002: const-string v1, "hello" // string@000a
    // 6e20 0400 1000 |0004: invoke-virtual {v0, v1}, Ljava/io/PrintStream;.println:(Ljava/lang/String;)V // method@0004
    // 0e00           |0007: return-void
    new byte[] { 98, 0, 0, 0, 26, 1, 10, 0, 110, 32, 4, 0, 16, 0, 14, 0 },
    // Java bytecodes
    // 0: getstatic     #2  // Field java/lang/System.out:Ljava/io/PrintStream;
    // 3: ldc           #3  // String hello
    // 5: invokevirtual #4  // Method java/io/PrintStream.println:(Ljava/lang/String;)V
    // 8: return
    new byte[] { -78, 0, 2, 18, 3, -74, 0, 4, -79 },
  };

  public static ClassLoader getClassLoader() throws Exception {
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.InMemoryDexClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(ByteBuffer.class, ClassLoader.class);
      // We are on art since we got the InMemoryDexClassLoader.
      return (ClassLoader)ctor.newInstance(
          ByteBuffer.wrap(DEX_BYTES), Test1901.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI.
      return new ClassLoader(Test1901.class.getClassLoader()) {
        protected Class<?> findClass(String name) throws ClassNotFoundException {
          if (name.equals("art.Target")) {
            return defineClass(name, CLASS_BYTES, 0, CLASS_BYTES.length);
          } else {
            return super.findClass(name);
          }
        }
      };
    }
  }

  public static void CheckMethodBytes(Method m, byte[][] possible_bytecodes) {
    byte[] real_codes = getBytecodes(m);
    for (byte[] pos : possible_bytecodes) {
      if (Arrays.equals(pos, real_codes)) {
        return;
      }
    }
    System.out.println("Unexpected bytecodes for " + m);
    System.out.println("Received: " + Arrays.toString(real_codes));
    System.out.println("Expected one of:");
    for (byte[] pos : possible_bytecodes) {
      System.out.println("\t" + Arrays.toString(pos));
    }
  }

  public static void run() throws Exception {
    Class<?> target = getClassLoader().loadClass("art.Target");
    CheckMethodBytes(target.getDeclaredMethod("doNothing"), DO_NOTHING_BYTECODES);
    CheckMethodBytes(target.getDeclaredMethod("staticNothing"), STATIC_NOTHING_BYTECODES);
    CheckMethodBytes(target.getDeclaredMethod("sayHi"), SAY_HI_NOTHING_BYTECODES);
  }

  public static native byte[] getBytecodes(Method m);
}
