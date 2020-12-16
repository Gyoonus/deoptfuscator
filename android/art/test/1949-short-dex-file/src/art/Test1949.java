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

import java.lang.reflect.*;
import java.util.Base64;
import java.nio.ByteBuffer;

public class Test1949 {
  private final static boolean isDalvik = System.getProperty("java.vm.name").equals("Dalvik");

  // This dex file is specifically crafted to have exactly 4 methodIDs in it. They are (in order):
  //   (0) Ljava/lang/Object;-><init>()V
  //   (1) Lxyz/Transform;-><init>()V
  //   (2) Lxyz/Transform;->bar()V
  //   (3) Lxyz/Transform;->foo()V
  //
  // In the transformed version of the dex file there is a new method. The new list of methodIDs is:
  //   (0) Lart/Test1949;->doNothing()V
  //   (1) Ljava/lang/Object;-><init>()V
  //   (2) Lxyz/Transform;-><init>()V
  //   (3) Lxyz/Transform;->bar()V
  //   (4) Lxyz/Transform;->foo()V
  //
  // This test tries to get the JIT to read out-of-bounds on the initial dex file by getting it to
  // read the 5th method id of the new file (Lxyz/Transform;->foo()V) from the old dex file (which
  // only has 4 method ids).
  //
  // To do this we need to make sure that the class being transformed is near the end of the
  // alphabet (package xyz, method foo). If it is further forward than the other method-ids then the
  // JIT will read an incorrect (but valid) method-id from the old-dex file. This is why the error
  // wasn't caught in our other tests (package art is always at the front).
  //
  // The final method that causes the OOB read needs to be a native method because that is the only
  // method-type the jit uses dex-file information to keep track of.

  /**
   * base64 encoded class/dex file for
   * package xyz;
   * public class Transform {
   *   public native void foo();
   *   public void bar() {}
   * }
   */
  private static final byte[] CLASS_BYTES_INIT = Base64.getDecoder().decode(
    "yv66vgAAADUADwoAAwAMBwANBwAOAQAGPGluaXQ+AQADKClWAQAEQ29kZQEAD0xpbmVOdW1iZXJU" +
    "YWJsZQEAA2ZvbwEAA2JhcgEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwABAAFAQANeHl6" +
    "L1RyYW5zZm9ybQEAEGphdmEvbGFuZy9PYmplY3QAIQACAAMAAAAAAAMAAQAEAAUAAQAGAAAAHQAB" +
    "AAEAAAAFKrcAAbEAAAABAAcAAAAGAAEAAAACAQEACAAFAAAAAQAJAAUAAQAGAAAAGQAAAAEAAAAB" +
    "sQAAAAEABwAAAAYAAQAAAAQAAQAKAAAAAgAL");
  private static final byte[] DEX_BYTES_INIT = Base64.getDecoder().decode(
    "ZGV4CjAzNQBDUutFJpeT+okk+aXah8NQ61q2XRtkmChwAgAAcAAAAHhWNBIAAAAAAAAAANwBAAAI" +
    "AAAAcAAAAAMAAACQAAAAAQAAAJwAAAAAAAAAAAAAAAQAAACoAAAAAQAAAMgAAACIAQAA6AAAABwB" +
    "AAAkAQAAOAEAAEkBAABZAQAAXAEAAGEBAABmAQAAAQAAAAIAAAAEAAAABAAAAAIAAAAAAAAAAAAA" +
    "AAAAAAABAAAAAAAAAAEAAAAFAAAAAQAAAAYAAAABAAAAAQAAAAAAAAAAAAAAAwAAAAAAAADDAQAA" +
    "AAAAAAEAAQABAAAAEgEAAAQAAABwEAAAAAAOAAEAAQAAAAAAFgEAAAEAAAAOAAIADgAEAA4AAAAG" +
    "PGluaXQ+ABJMamF2YS9sYW5nL09iamVjdDsAD0x4eXovVHJhbnNmb3JtOwAOVHJhbnNmb3JtLmph" +
    "dmEAAVYAA2JhcgADZm9vAFt+fkQ4eyJtaW4tYXBpIjoxLCJzaGEtMSI6IjkwZWYyMjkwNWMzZmVj" +
    "Y2FiMjMwMzBhNmJkYzU2NTcwYTMzNWVmMDUiLCJ2ZXJzaW9uIjoidjEuMS44LWRldiJ9AAAAAQIB" +
    "gYAE6AECAYACAYECAAAAAAAAAAAMAAAAAAAAAAEAAAAAAAAAAQAAAAgAAABwAAAAAgAAAAMAAACQ" +
    "AAAAAwAAAAEAAACcAAAABQAAAAQAAACoAAAABgAAAAEAAADIAAAAASAAAAIAAADoAAAAAyAAAAIA" +
    "AAASAQAAAiAAAAgAAAAcAQAAACAAAAEAAADDAQAAAxAAAAEAAADYAQAAABAAAAEAAADcAQAA");

  /**
   * base64 encoded class/dex file for
   * package xyz;
   * public class Transform {
   *   public native void foo();
   *   public void bar() {
   *     // Make sure the methodID is before any of the ones in Transform
   *     art.Test1949.doNothing();
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES_FINAL = Base64.getDecoder().decode(
    "yv66vgAAADUAFAoABAANCgAOAA8HABAHABEBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAPTGluZU51" +
    "bWJlclRhYmxlAQADZm9vAQADYmFyAQAKU291cmNlRmlsZQEADlRyYW5zZm9ybS5qYXZhDAAFAAYH" +
    "ABIMABMABgEADXh5ei9UcmFuc2Zvcm0BABBqYXZhL2xhbmcvT2JqZWN0AQAMYXJ0L1Rlc3QxOTQ5" +
    "AQAJZG9Ob3RoaW5nACEAAwAEAAAAAAADAAEABQAGAAEABwAAAB0AAQABAAAABSq3AAGxAAAAAQAI" +
    "AAAABgABAAAAAgEBAAkABgAAAAEACgAGAAEABwAAABwAAAABAAAABLgAArEAAAABAAgAAAAGAAEA" +
    "AAAEAAEACwAAAAIADA==");
  private static final byte[] DEX_BYTES_FINAL = Base64.getDecoder().decode(
    "ZGV4CjAzNQBHXBiw7Hso1vnmaXE1VCV41f4+0aECixOgAgAAcAAAAHhWNBIAAAAAAAAAAAwCAAAK" +
    "AAAAcAAAAAQAAACYAAAAAQAAAKgAAAAAAAAAAAAAAAUAAAC0AAAAAQAAANwAAACkAQAA/AAAADQB" +
    "AAA8AQAATAEAAGABAABxAQAAgQEAAIQBAACJAQAAlAEAAJkBAAABAAAAAgAAAAMAAAAFAAAABQAA" +
    "AAMAAAAAAAAAAAAAAAcAAAABAAAAAAAAAAIAAAAAAAAAAgAAAAYAAAACAAAACAAAAAIAAAABAAAA" +
    "AQAAAAAAAAAEAAAAAAAAAPYBAAAAAAAAAQABAAEAAAAsAQAABAAAAHAQAQAAAA4AAQABAAAAAAAw" +
    "AQAABAAAAHEAAAAAAA4AAgAOAAQADgAGPGluaXQ+AA5MYXJ0L1Rlc3QxOTQ5OwASTGphdmEvbGFu" +
    "Zy9PYmplY3Q7AA9MeHl6L1RyYW5zZm9ybTsADlRyYW5zZm9ybS5qYXZhAAFWAANiYXIACWRvTm90" +
    "aGluZwADZm9vAFt+fkQ4eyJtaW4tYXBpIjoxLCJzaGEtMSI6IjkwZWYyMjkwNWMzZmVjY2FiMjMw" +
    "MzBhNmJkYzU2NTcwYTMzNWVmMDUiLCJ2ZXJzaW9uIjoidjEuMS44LWRldiJ9AAAAAQICgYAE/AED" +
    "AZQCAYECAAAAAAAMAAAAAAAAAAEAAAAAAAAAAQAAAAoAAABwAAAAAgAAAAQAAACYAAAAAwAAAAEA" +
    "AACoAAAABQAAAAUAAAC0AAAABgAAAAEAAADcAAAAASAAAAIAAAD8AAAAAyAAAAIAAAAsAQAAAiAA" +
    "AAoAAAA0AQAAACAAAAEAAAD2AQAAAxAAAAEAAAAIAgAAABAAAAEAAAAMAgAA");

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

  // A method with a methodID before anything in Transform.
  public static void doNothing() {}

  private static ClassLoader CreateClassLoader(byte[] clz, byte[] dex) throws Exception {
    if (isDalvik) {
      Class<?> class_loader_class = Class.forName("dalvik.system.InMemoryDexClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(ByteBuffer.class, ClassLoader.class);
      /* on Dalvik, this is a DexFile; otherwise, it's null */
      return (ClassLoader)ctor.newInstance(ByteBuffer.wrap(dex), Test1949.class.getClassLoader());
    } else {
      return new ClassLoader() {
        public Class<?> findClass(String name) throws ClassNotFoundException {
          if (name.equals("xyz.Transform")) {
            return defineClass(name, clz, 0, clz.length);
          } else {
            throw new ClassNotFoundException("Couldn't find class: " + name);
          }
        }
      };
    }
  }

  public static void doTest() throws Exception {
    Class c = CreateClassLoader(CLASS_BYTES_INIT, DEX_BYTES_INIT).loadClass("xyz.Transform");
    Redefinition.doCommonClassRedefinition(c, CLASS_BYTES_FINAL, DEX_BYTES_FINAL);
    System.out.println("Passed");
  }
}
