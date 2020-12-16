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

import java.lang.ref.Reference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Proxy;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Comparator;

public class Test912 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    testClass("java.lang.Object");
    testClass("java.lang.String");
    testClass("java.lang.Math");
    testClass("java.util.List");

    testClass(getProxyClass());

    testClass(int.class);
    testClass(double[].class);

    testClassType(int.class);
    testClassType(getProxyClass());
    testClassType(Runnable.class);
    testClassType(String.class);
    testClassType(ArrayList.class);

    testClassType(int[].class);
    testClassType(Runnable[].class);
    testClassType(String[].class);

    testClassFields(Integer.class);
    testClassFields(int.class);
    testClassFields(String[].class);

    testClassMethods(Integer.class);
    testClassMethods(int.class);
    testClassMethods(String[].class);

    testClassStatus(int.class);
    testClassStatus(String[].class);
    testClassStatus(Object.class);
    testClassStatus(TestForNonInit.class);
    try {
      System.out.println(TestForInitFail.dummy);
    } catch (ExceptionInInitializerError e) {
    }
    testClassStatus(TestForInitFail.class);

    testInterfaces(int.class);
    testInterfaces(String[].class);
    testInterfaces(Object.class);
    testInterfaces(InfA.class);
    testInterfaces(InfB.class);
    testInterfaces(InfC.class);
    testInterfaces(ClassA.class);
    testInterfaces(ClassB.class);
    testInterfaces(ClassC.class);

    testClassLoader(String.class);
    testClassLoader(String[].class);
    testClassLoader(InfA.class);
    testClassLoader(getProxyClass());

    testClassLoaderClasses();

    System.out.println();

    testClassVersion();

    System.out.println();

    // Use a dedicated thread to have a well-defined current thread.
    Thread classEventsThread = new Thread("ClassEvents") {
      @Override
      public void run() {
        try {
          testClassEvents();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };
    classEventsThread.start();
    classEventsThread.join();
  }

  private static void testClass(String className) throws Exception {
    Class<?> base = Class.forName(className);
    testClass(base);
  }

  private static void testClass(Class<?> base) throws Exception {
    String[] result = getClassSignature(base);
    System.out.println(Arrays.toString(result));
    int mod = getClassModifiers(base);
    if (mod != base.getModifiers()) {
      throw new RuntimeException("Unexpected modifiers: " + base.getModifiers() + " vs " + mod);
    }
    System.out.println(Integer.toHexString(mod));
  }

  private static void testClassType(Class<?> c) throws Exception {
    boolean isInterface = isInterface(c);
    boolean isArray = isArrayClass(c);
    boolean isModifiable = isModifiableClass(c);
    System.out.println(c.getName() + " interface=" + isInterface + " array=" + isArray +
        " modifiable=" + isModifiable);
  }

  private static void testClassFields(Class<?> c) throws Exception {
    System.out.println(Arrays.toString(getClassFields(c)));
  }

  private static void testClassMethods(Class<?> c) throws Exception {
    System.out.println(Arrays.toString(getClassMethods(c)));
  }

  private static void testClassStatus(Class<?> c) {
    System.out.println(c + " " + Integer.toBinaryString(getClassStatus(c)));
  }

  private static void testInterfaces(Class<?> c) {
    System.out.println(c + " " + Arrays.toString(getImplementedInterfaces(c)));
  }

  private static boolean IsBootClassLoader(ClassLoader l) {
    // Hacky check for Android's fake boot classloader.
    return l.getClass().getName().equals("java.lang.BootClassLoader");
  }

  private static void testClassLoader(Class<?> c) {
    Object cl = getClassLoader(c);
    System.out.println(c + " " + (cl != null ? cl.getClass().getName() : "null"));
    if (cl == null) {
      if (c.getClassLoader() != null && !IsBootClassLoader(c.getClassLoader())) {
        throw new RuntimeException("Expected " + c.getClassLoader() + ", but got null.");
      }
    } else {
      if (!(cl instanceof ClassLoader)) {
        throw new RuntimeException("Unexpected \"classloader\": " + cl + " (" + cl.getClass() +
            ")");
      }
      if (cl != c.getClassLoader()) {
        throw new RuntimeException("Unexpected classloader: " + c.getClassLoader() + " vs " + cl);
      }
    }
  }

  private static void testClassLoaderClasses() throws Exception {
    System.out.println();
    System.out.println("boot <- (B) <- (A,C)");
    ClassLoader cl1 = DexData.create2(DexData.create1());
    Class.forName("B", false, cl1);
    Class.forName("A", false, cl1);
    printClassLoaderClasses(cl1);

    System.out.println();
    System.out.println("boot <- (B) <- (A, List)");
    ClassLoader cl2 = DexData.create2(DexData.create1());
    Class.forName("A", false, cl2);
    Class.forName("java.util.List", false, cl2);
    Class.forName("B", false, cl2.getParent());
    printClassLoaderClasses(cl2);

    System.out.println();
    System.out.println("boot <- 1+2 (A,B)");
    ClassLoader cl3 = DexData.create12();
    Class.forName("B", false, cl3);
    Class.forName("A", false, cl3);
    printClassLoaderClasses(cl3);

    // Check that the boot classloader dumps something non-empty.
    ClassLoader boot = ClassLoader.getSystemClassLoader().getParent();
    while (boot.getParent() != null) {
      boot = boot.getParent();
    }

    Class<?>[] bootClasses = getClassLoaderClasses(boot);
    if (bootClasses.length == 0) {
      throw new RuntimeException("No classes initiated by boot classloader.");
    }
    // Check that at least java.util.List is loaded.
    boolean foundList = false;
    for (Class<?> c : bootClasses) {
      if (c == java.util.List.class) {
        foundList = true;
        break;
      }
    }
    if (!foundList) {
      System.out.println(Arrays.toString(bootClasses));
      throw new RuntimeException("Could not find class java.util.List.");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCLXSBQ5FiS3f16krSYZFF8xYZtFVp0GRXMAgAAcAAAAHhWNBIAAAAAAAAAACwCAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAACsAQAAIAEAAGIB" +
    "AABqAQAAcwEAAIABAACXAQAAqwEAAL8BAADTAQAA4wEAAOYBAADqAQAA/gEAAAMCAAAMAgAAAgAA" +
    "AAMAAAAEAAAABQAAAAYAAAAIAAAACAAAAAUAAAAAAAAACQAAAAUAAABcAQAABAABAAsAAAAAAAAA" +
    "AAAAAAAAAAANAAAAAQABAAwAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAHAAAAAAAAAB4CAAAA" +
    "AAAAAQABAAEAAAATAgAABAAAAHAQAwAAAA4AAwABAAIAAAAYAgAACQAAAGIAAAAbAQEAAABuIAIA" +
    "EAAOAAAAAQAAAAMABjxpbml0PgAHR29vZGJ5ZQALTFRyYW5zZm9ybTsAFUxqYXZhL2lvL1ByaW50" +
    "U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xh" +
    "bmcvU3lzdGVtOwAOVHJhbnNmb3JtLmphdmEAAVYAAlZMABJlbWl0dGVyOiBqYWNrLTMuMzYAA291" +
    "dAAHcHJpbnRsbgAFc2F5SGkAEQAHDgATAAcOhQAAAAEBAICABKACAQG4Ag0AAAAAAAAAAQAAAAAA" +
    "AAABAAAADgAAAHAAAAACAAAABgAAAKgAAAADAAAAAgAAAMAAAAAEAAAAAQAAANgAAAAFAAAABAAA" +
    "AOAAAAAGAAAAAQAAAAABAAABIAAAAgAAACABAAABEAAAAQAAAFwBAAACIAAADgAAAGIBAAADIAAA" +
    "AgAAABMCAAAAIAAAAQAAAB4CAAAAEAAAAQAAACwCAAA=");
  private static void testClassVersion() throws Exception {
    Class<?> class_loader_class = Class.forName("dalvik.system.InMemoryDexClassLoader");
    Constructor<?> ctor = class_loader_class.getConstructor(ByteBuffer.class, ClassLoader.class);
    Class target = ((ClassLoader)ctor.newInstance(
        ByteBuffer.wrap(DEX_BYTES), Test912.class.getClassLoader())).loadClass("Transform");
    System.out.println(Arrays.toString(getClassVersion(target)));
  }

  private static void testClassEvents() throws Exception {
    ClassLoader cl = Main.class.getClassLoader();
    while (cl.getParent() != null) {
      cl = cl.getParent();
    }
    final ClassLoader boot = cl;

    // The JIT may deeply inline and load some classes. Preload these for test determinism.
    final String PRELOAD_FOR_JIT[] = {
        "java.nio.charset.CoderMalfunctionError",
        "java.util.NoSuchElementException",
        "java.io.FileNotFoundException",  // b/63581208
        "java.util.zip.ZipException",     // b/63581208
    };
    for (String s : PRELOAD_FOR_JIT) {
      Class.forName(s);
    }

    Runnable r = new Runnable() {
      @Override
      public void run() {
        try {
          ClassLoader cl6 = DexData.create12();
          System.out.println("C, true");
          Class.forName("C", true, cl6);
          printClassLoadMessages();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };

    Thread dummyThread = new Thread();
    dummyThread.start();
    dummyThread.join();

    enableClassLoadPreparePrintEvents(true, Thread.currentThread());

    ClassLoader cl1 = DexData.create12();
    System.out.println("B, false");
    Class.forName("B", false, cl1);
    printClassLoadMessages();

    ClassLoader cl2 = DexData.create12();
    System.out.println("B, true");
    Class.forName("B", true, cl2);
    printClassLoadMessages();

    ClassLoader cl3 = DexData.create12();
    System.out.println("C, false");
    Class.forName("C", false, cl3);
    printClassLoadMessages();
    System.out.println("A, false");
    Class.forName("A", false, cl3);
    printClassLoadMessages();

    ClassLoader cl4 = DexData.create12();
    System.out.println("C, true");
    Class.forName("C", true, cl4);
    printClassLoadMessages();
    System.out.println("A, true");
    Class.forName("A", true, cl4);
    printClassLoadMessages();

    ClassLoader cl5 = DexData.create12();
    System.out.println("A, true");
    Class.forName("A", true, cl5);
    printClassLoadMessages();
    System.out.println("C, true");
    Class.forName("C", true, cl5);
    printClassLoadMessages();

    enableClassLoadPreparePrintEvents(false, null);

    Thread t = new Thread(r, "TestRunner");
    enableClassLoadPreparePrintEvents(true, t);
    t.start();
    t.join();
    enableClassLoadPreparePrintEvents(false, null);

    enableClassLoadPreparePrintEvents(true, Thread.currentThread());

    // Check creation of arrays and proxies.
    Proxy.getProxyClass(Main.class.getClassLoader(), new Class[] { Comparable.class, I0.class });
    Class.forName("[Lart.Test912;");
    printClassLoadMessages();

    enableClassLoadPreparePrintEvents(false, null);

    testClassLoadPrepareEquality();
  }

  private static void testClassLoadPrepareEquality() throws Exception {
    setEqualityEventStorageClass(ClassF.class);

    enableClassLoadPrepareEqualityEvents(true);

    Class.forName("art.Test912$ClassE");

    enableClassLoadPrepareEqualityEvents(false);
  }

  private static void printClassLoaderClasses(ClassLoader cl) {
    for (;;) {
      if (cl == null || !cl.getClass().getName().startsWith("dalvik.system")) {
        break;
      }

      Class<?> classes[] = getClassLoaderClasses(cl);
      Arrays.sort(classes, new ClassNameComparator());
      System.out.println(Arrays.toString(classes));

      cl = cl.getParent();
    }
  }

  private static void printClassLoadMessages() {
    for (String s : getClassLoadMessages()) {
      System.out.println(s);
    }
  }

  private static native boolean isModifiableClass(Class<?> c);
  private static native String[] getClassSignature(Class<?> c);

  private static native boolean isInterface(Class<?> c);
  private static native boolean isArrayClass(Class<?> c);

  private static native int getClassModifiers(Class<?> c);

  private static native Object[] getClassFields(Class<?> c);
  private static native Object[] getClassMethods(Class<?> c);
  private static native Class<?>[] getImplementedInterfaces(Class<?> c);

  private static native int getClassStatus(Class<?> c);

  private static native Object getClassLoader(Class<?> c);

  private static native Class<?>[] getClassLoaderClasses(ClassLoader cl);

  private static native int[] getClassVersion(Class<?> c);

  private static native void enableClassLoadPreparePrintEvents(boolean b, Thread filter);
  private static native String[] getClassLoadMessages();

  private static native void setEqualityEventStorageClass(Class<?> c);
  private static native void enableClassLoadPrepareEqualityEvents(boolean b);

  private static class TestForNonInit {
    public static double dummy = Math.random();  // So it can't be compile-time initialized.
  }

  @SuppressWarnings("RandomCast")
  private static class TestForInitFail {
    public static int dummy = ((int)Math.random())/0;  // So it throws when initializing.
  }

  public static interface InfA {
  }
  public static interface InfB extends InfA {
  }
  public static interface InfC extends InfB {
  }

  public abstract static class ClassA implements InfA {
  }
  public abstract static class ClassB extends ClassA implements InfB {
  }
  public abstract static class ClassC implements InfA, InfC {
  }

  public static class ClassE {
    public void foo() {
    }
    public void bar() {
    }
  }

  public static class ClassF {
    public static Object STATIC = null;
    public static Reference<Object> WEAK = null;
  }

  private static class ClassNameComparator implements Comparator<Class<?>> {
    public int compare(Class<?> c1, Class<?> c2) {
      return c1.getName().compareTo(c2.getName());
    }
  }

  // See run-test 910 for an explanation.

  private static Class<?> proxyClass = null;

  private static Class<?> getProxyClass() throws Exception {
    if (proxyClass != null) {
      return proxyClass;
    }

    for (int i = 1; i <= 21; i++) {
      proxyClass = createProxyClass(i);
      String name = proxyClass.getName();
      if (name.equals("$Proxy20")) {
        return proxyClass;
      }
    }
    return proxyClass;
  }

  private static Class<?> createProxyClass(int i) throws Exception {
    int count = Integer.bitCount(i);
    Class<?>[] input = new Class<?>[count + 1];
    input[0] = Runnable.class;
    int inputIndex = 1;
    int bitIndex = 0;
    while (i != 0) {
        if ((i & 1) != 0) {
            input[inputIndex++] = Class.forName("art.Test912$I" + bitIndex);
        }
        i >>>= 1;
        bitIndex++;
    }
    return Proxy.getProxyClass(Test912.class.getClassLoader(), input);
  }

  // Need this for the proxy naming.
  public static interface I0 {
  }
  public static interface I1 {
  }
  public static interface I2 {
  }
  public static interface I3 {
  }
  public static interface I4 {
  }
}
