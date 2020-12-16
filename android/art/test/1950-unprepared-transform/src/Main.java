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

import art.Redefinition;

import java.lang.reflect.*;
import java.util.Base64;
import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;

class Main {
  public static String TEST_NAME = "1950-unprepared-transform";

  // Base 64 encoding of the following class:
  //
  // public class Transform {
  //   public String toString() {
  //     return "Transformed object!";
  //   }
  // }
  public static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAEQoABAANCAAOBwAPBwAQAQAGPGluaXQ+AQADKClWAQAEQ29kZQEAD0xpbmVOdW1i" +
    "ZXJUYWJsZQEACHRvU3RyaW5nAQAUKClMamF2YS9sYW5nL1N0cmluZzsBAApTb3VyY2VGaWxlAQAO" +
    "VHJhbnNmb3JtLmphdmEMAAUABgEAE1RyYW5zZm9ybWVkIG9iamVjdCEBAAlUcmFuc2Zvcm0BABBq" +
    "YXZhL2xhbmcvT2JqZWN0ACEAAwAEAAAAAAACAAEABQAGAAEABwAAAB0AAQABAAAABSq3AAGxAAAA" +
    "AQAIAAAABgABAAAAEgABAAkACgABAAcAAAAbAAEAAQAAAAMSArAAAAABAAgAAAAGAAEAAAAUAAEA" +
    "CwAAAAIADA==");

  public static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzOACaXU/P8oJOECPrdN1Cu9/ob2cUb2vOKxqYAgAAcAAAAHhWNBIAAAAAAAAAABACAAAK" +
    "AAAAcAAAAAQAAACYAAAAAgAAAKgAAAAAAAAAAAAAAAMAAADAAAAAAQAAANgAAACgAQAA+AAAADAB" +
    "AAA4AQAAOwEAAEgBAABcAQAAcAEAAIABAACVAQAAmAEAAKIBAAACAAAAAwAAAAQAAAAHAAAAAQAA" +
    "AAIAAAAAAAAABwAAAAMAAAAAAAAAAAABAAAAAAAAAAAACAAAAAEAAQAAAAAAAAAAAAEAAAABAAAA" +
    "AAAAAAUAAAAAAAAAAAIAAAAAAAACAAEAAAAAACwBAAADAAAAGgAGABEAAAABAAEAAQAAACgBAAAE" +
    "AAAAcBACAAAADgASAA4AFAAOAAY8aW5pdD4AAUwAC0xUcmFuc2Zvcm07ABJMamF2YS9sYW5nL09i" +
    "amVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAOVHJhbnNmb3JtLmphdmEAE1RyYW5zZm9ybWVkIG9i" +
    "amVjdCEAAVYACHRvU3RyaW5nAFx+fkQ4eyJtaW4tYXBpIjoyNywic2hhLTEiOiI3YTdjNDlhY2Nj" +
    "NTkzNTIyNzY4MTY3MThhNGM3YWU1MmY5NjgzZjk5IiwidmVyc2lvbiI6InYxLjIuNC1kZXYifQAA" +
    "AAEBAIGABJACAQH4AQAACwAAAAAAAAABAAAAAAAAAAEAAAAKAAAAcAAAAAIAAAAEAAAAmAAAAAMA" +
    "AAACAAAAqAAAAAUAAAADAAAAwAAAAAYAAAABAAAA2AAAAAEgAAACAAAA+AAAAAMgAAACAAAAKAEA" +
    "AAIgAAAKAAAAMAEAAAAgAAABAAAAAAIAAAAQAAABAAAAEAIAAA==");

  public static native void setupClassLoadHook(Thread target);
  public static native void clearClassLoadHook(Thread target);
  private static Consumer<Class<?>> doRedefine = null;

  public static void doClassLoad(Class<?> c) {
    try {
      if (c.getName().equals("Transform")) {
        Redefinition.addCommonTransformationResult("Transform", CLASS_BYTES, DEX_BYTES);
        doRedefine.accept(c);
        System.out.println("retransformClasses on an unprepared class succeeded");
      }
    } catch (Throwable e) {
      System.out.println("Trying to redefine: " + c + ". " +
          "Caught error " + e.getClass() + ": " + e.getMessage());
    }
  }

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

  public static void testCurrentThread() throws Throwable {
    System.out.println("Redefine in ClassLoad on current thread.");
    doRedefine = (c) -> { Redefinition.doCommonClassRetransformation(c); };
    ClassLoader new_loader = getClassLoaderFor(System.getenv("DEX_LOCATION"));
    Class<?> klass = (Class<?>)new_loader.loadClass("Transform");
    if (klass == null) {
      throw new AssertionError("loadClass failed");
    }
    Object o = klass.newInstance();
    System.out.println("Object out is: " + o);
  }

  public static void testRemoteThread() throws Throwable {
    System.out.println("Redefine during ClassLoad on another thread.");
    final Class[] loaded = new Class[] { null, };
    final CountDownLatch gotClass = new CountDownLatch(1);
    final CountDownLatch wokeUp = new CountDownLatch(1);
    Thread redef_thread = new Thread(() -> {
      try {
        gotClass.await();
        wokeUp.countDown();
        // This will wait until the otehr thread returns so we need to wake up the other thread
        // first.
        Redefinition.doCommonClassRetransformation(loaded[0]);
      } catch (Exception e) {
        throw new Error("Failed to do redef!", e);
      }
    });
    redef_thread.start();
    doRedefine = (c) -> {
      try {
        loaded[0] = c;
        gotClass.countDown();
        wokeUp.await();
        // Let the other thread do some stuff.
        Thread.sleep(5000);
      } catch (Exception e) {
        throw new Error("Failed to do redef!", e);
      }
    };
    ClassLoader new_loader = getClassLoaderFor(System.getenv("DEX_LOCATION"));
    Class<?> klass = (Class<?>)new_loader.loadClass("Transform");
    if (klass == null) {
      throw new AssertionError("loadClass failed");
    }
    Object o = klass.newInstance();
    System.out.println("Object out is: " + o);
    redef_thread.join();
    System.out.println("Redefinition thread finished.");
  }

  public static void main(String[] args) {
    // make sure we can do the transform.
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_RETRANSFORM);
    Redefinition.setPopRetransformations(false);
    Redefinition.enableCommonRetransformation(true);
    setupClassLoadHook(Thread.currentThread());
    try {
      testCurrentThread();
      testRemoteThread();
    } catch (Throwable e) {
      System.out.println(e.toString());
      e.printStackTrace(System.out);
    }
    clearClassLoadHook(Thread.currentThread());
  }
}
