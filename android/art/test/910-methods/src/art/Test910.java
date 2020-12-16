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

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;

public class Test910 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    testMethod("java.lang.Object", "toString");
    testMethod("java.lang.String", "charAt", int.class);
    testMethod("java.lang.Math", "sqrt", double.class);
    testMethod("java.util.List", "add", Object.class);

    testMethod(getProxyClass(), "run");

    // Find a synthetic method in the dummy inner class. Do not print the name. Javac and Jack
    // disagree on the naming of synthetic accessors.
    testMethod(findSyntheticMethod(), NestedSynthetic.class, false);
  }

  private static void testMethod(String className, String methodName, Class<?>... types)
      throws Exception {
    Class<?> base = Class.forName(className);
    testMethod(base, methodName, types);
  }

  private static void testMethod(Class<?> base, String methodName, Class<?>... types)
      throws Exception {
    Method m = base.getDeclaredMethod(methodName, types);
    testMethod(m, base, true);
  }

  private static void testMethod(Method m, Class<?> base, boolean printName) {
    String[] result = getMethodName(m);
    if (!result[0].equals(m.getName())) {
      throw new RuntimeException("Name not equal: " + m.getName() + " vs " + result[0]);
    }
    if (printName) {
      System.out.println(Arrays.toString(result));
    }

    Class<?> declClass = getMethodDeclaringClass(m);
    if (base != declClass) {
      throw new RuntimeException("Declaring class not equal: " + base + " vs " + declClass);
    }
    System.out.println(declClass);

    int modifiers = getMethodModifiers(m);
    if (modifiers != m.getModifiers()) {
      throw new RuntimeException("Modifiers not equal: " + m.getModifiers() + " vs " + modifiers);
    }
    System.out.println(modifiers);

    System.out.print("Max locals: ");
    try {
      System.out.println(getMaxLocals(m));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    System.out.print("Argument size: ");
    try {
      System.out.println(getArgumentsSize(m));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    System.out.print("Location start: ");
    try {
      System.out.println(getMethodLocationStart(m));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    System.out.print("Location end: ");
    try {
      System.out.println(getMethodLocationEnd(m));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    System.out.println("Is native: " + isMethodNative(m));
    System.out.println("Is obsolete: " + isMethodObsolete(m));
    System.out.println("Is synthetic: " + isMethodSynthetic(m));
  }

  private static class NestedSynthetic {
    // Accessing this private field will create a synthetic accessor method;
    private static String dummy;
  }

  private static void dummyAccess() {
    System.out.println(NestedSynthetic.dummy);
  }

  private static Method findSyntheticMethod() throws Exception {
    Method methods[] = NestedSynthetic.class.getDeclaredMethods();
    for (Method m : methods) {
      if (m.isSynthetic()) {
        return m;
      }
    }
    throw new RuntimeException("Could not find synthetic method");
  }

  private static native String[] getMethodName(Method m);
  private static native Class<?> getMethodDeclaringClass(Method m);
  private static native int getMethodModifiers(Method m);
  private static native int getMaxLocals(Method m);
  private static native int getArgumentsSize(Method m);
  private static native long getMethodLocationStart(Method m);
  private static native long getMethodLocationEnd(Method m);
  private static native boolean isMethodNative(Method m);
  private static native boolean isMethodObsolete(Method m);
  private static native boolean isMethodSynthetic(Method m);

  // We need this machinery for a consistent proxy name. Names of proxy classes include a
  // unique number (derived by counting). This means that a simple call to getProxyClass
  // depends on the test environment.
  //
  // To work around this, we assume that at most twenty proxies have been created before
  // the test is run, and canonicalize on "$Proxy20". We add infrastructure to create
  // as many proxy classes but cycling through subsets of the test-provided interfaces
  // I0...I4.
  //
  //
  // (This is made under the judgment that we do not want to have proxy-specific behavior
  //  for testMethod.)

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
            input[inputIndex++] = Class.forName("art.Test910$I" + bitIndex);
        }
        i >>>= 1;
        bitIndex++;
    }
    return Proxy.getProxyClass(Test910.class.getClassLoader(), input);
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
