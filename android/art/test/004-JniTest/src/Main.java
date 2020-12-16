/*
 * Copyright (C) 2013 The Android Open Source Project
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

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.util.regex.Pattern;

import dalvik.annotation.optimization.CriticalNative;
import dalvik.annotation.optimization.FastNative;

public class Main {
    public static void main(String[] args) {
        System.loadLibrary(args[0]);

        if (!isSlowDebug()) {
          throw new RuntimeException("Slow-debug flags unexpectedly off.");
        }

        testFieldSubclass();
        testFindClassOnAttachedNativeThread();
        testFindFieldOnAttachedNativeThread();
        testReflectFieldGetFromAttachedNativeThreadNative();
        testCallStaticVoidMethodOnSubClass();
        testGetMirandaMethod();
        testZeroLengthByteBuffers();
        testByteMethod();
        testShortMethod();
        testBooleanMethod();
        testCharMethod();
        testIsAssignableFromOnPrimitiveTypes();
        testShallowGetCallingClassLoader();
        testShallowGetStackClass2();
        testCallNonvirtual();
        testNewStringObject();
        testRemoveLocalObject();
        testProxyGetMethodID();
        testJniCriticalSectionAndGc();
        testCallDefaultMethods();
        String lambda = "λ";
        testInvokeLambdaMethod(() -> { System.out.println("hi-lambda: " + lambda); });
        String def = "δ";
        testInvokeLambdaDefaultMethod(() -> { System.out.println("hi-default " + def + lambda); });

        registerNativesJniTest();
        testFastNativeMethods();
        testCriticalNativeMethods();

        testClinitMethodLookup();

        testDoubleLoad(args[0]);
    }

    static class ABC { public static int XYZ = 12; }
    static class DEF extends ABC {}
    public static void testFieldSubclass() {
      try {
        System.out.println("ABC.XYZ = " + ABC.XYZ + ", GetStaticIntField(DEF.class, 'XYZ') = " +
            getFieldSubclass(ABC.class.getDeclaredField("XYZ"), DEF.class));
      } catch (Exception e) {
        throw new RuntimeException("Failed to test get static field on a subclass", e);
      }
    }

    public static native int getFieldSubclass(Field f, Class sub);

    private static native boolean registerNativesJniTest();

    private static native void testCallDefaultMethods();

    private static native void testFindClassOnAttachedNativeThread();

    private static boolean testFindFieldOnAttachedNativeThreadField;

    private static native void testReflectFieldGetFromAttachedNativeThreadNative();

    public static boolean testReflectFieldGetFromAttachedNativeThreadField;

    private static void testFindFieldOnAttachedNativeThread() {
      testFindFieldOnAttachedNativeThreadNative();
      if (!testFindFieldOnAttachedNativeThreadField) {
            throw new AssertionError();
        }
    }

    private static native void testFindFieldOnAttachedNativeThreadNative();

    private static void testCallStaticVoidMethodOnSubClass() {
        testCallStaticVoidMethodOnSubClassNative();
        if (!testCallStaticVoidMethodOnSubClass_SuperClass.executed) {
            throw new AssertionError();
        }
    }

    private static native void testCallStaticVoidMethodOnSubClassNative();

    private static class testCallStaticVoidMethodOnSubClass_SuperClass {
        private static boolean executed = false;
        private static void execute() {
            executed = true;
        }
    }

    private static class testCallStaticVoidMethodOnSubClass_SubClass
        extends testCallStaticVoidMethodOnSubClass_SuperClass {
    }

    private static native Method testGetMirandaMethodNative();

    private static void testGetMirandaMethod() {
        Method m = testGetMirandaMethodNative();
        if (m.getDeclaringClass() != testGetMirandaMethod_MirandaInterface.class) {
            throw new AssertionError();
        }
    }

    private static native void testZeroLengthByteBuffers();

    private static abstract class testGetMirandaMethod_MirandaAbstract implements testGetMirandaMethod_MirandaInterface {
        public boolean inAbstract() {
            return true;
        }
    }

    private static interface testGetMirandaMethod_MirandaInterface {
        public boolean inInterface();
    }

    // Test sign-extension for values < 32b

    static native byte byteMethod(byte b1, byte b2, byte b3, byte b4, byte b5, byte b6, byte b7,
        byte b8, byte b9, byte b10);

    private static void testByteMethod() {
      byte returns[] = { 0, 1, 2, 127, -1, -2, -128 };
      for (int i = 0; i < returns.length; i++) {
        byte result = byteMethod((byte)i, (byte)2, (byte)(-3), (byte)4, (byte)(-5), (byte)6,
            (byte)(-7), (byte)8, (byte)(-9), (byte)10);
        if (returns[i] != result) {
          System.out.println("Run " + i + " with " + returns[i] + " vs " + result);
          throw new AssertionError();
        }
      }
    }

    private static native void removeLocalObject(Object o);

    private static void testRemoveLocalObject() {
        removeLocalObject(new Object());
    }

    private static native short shortMethod(short s1, short s2, short s3, short s4, short s5, short s6, short s7,
        short s8, short s9, short s10);

    private static void testShortMethod() {
      short returns[] = { 0, 1, 2, 127, 32767, -1, -2, -128, -32768 };
      for (int i = 0; i < returns.length; i++) {
        short result = shortMethod((short)i, (short)2, (short)(-3), (short)4, (short)(-5), (short)6,
            (short)(-7), (short)8, (short)(-9), (short)10);
        if (returns[i] != result) {
          System.out.println("Run " + i + " with " + returns[i] + " vs " + result);
          throw new AssertionError();
        }
      }
    }

    // Test zero-extension for values < 32b

    private static native boolean booleanMethod(boolean b1, boolean b2, boolean b3, boolean b4, boolean b5, boolean b6, boolean b7,
        boolean b8, boolean b9, boolean b10);

    private static void testBooleanMethod() {
      if (booleanMethod(false, true, false, true, false, true, false, true, false, true)) {
        throw new AssertionError();
      }

      if (!booleanMethod(true, true, false, true, false, true, false, true, false, true)) {
        throw new AssertionError();
      }
    }

    private static native char charMethod(char c1, char c2, char c3, char c4, char c5, char c6, char c7,
        char c8, char c9, char c10);

    private static void testCharMethod() {
      char returns[] = { (char)0, (char)1, (char)2, (char)127, (char)255, (char)256, (char)15000,
          (char)34000 };
      for (int i = 0; i < returns.length; i++) {
        char result = charMethod((char)i, 'a', 'b', 'c', '0', '1', '2', (char)1234, (char)2345,
            (char)3456);
        if (returns[i] != result) {
          System.out.println("Run " + i + " with " + (int)returns[i] + " vs " + (int)result);
          throw new AssertionError();
        }
      }
    }

    // http://b/16531674
    private static void testIsAssignableFromOnPrimitiveTypes() {
      if (!nativeIsAssignableFrom(int.class, Integer.TYPE)) {
        System.out.println("IsAssignableFrom(int.class, Integer.TYPE) returned false, expected true");
        throw new AssertionError();
      }

      if (!nativeIsAssignableFrom(Integer.TYPE, int.class)) {
        System.out.println("IsAssignableFrom(Integer.TYPE, int.class) returned false, expected true");
        throw new AssertionError();
      }
    }

    private static native boolean nativeIsAssignableFrom(Class<?> from, Class<?> to);

    private static void testShallowGetCallingClassLoader() {
        nativeTestShallowGetCallingClassLoader();
    }

    private native static void nativeTestShallowGetCallingClassLoader();

    private static void testShallowGetStackClass2() {
        nativeTestShallowGetStackClass2();
    }

    private static native void nativeTestShallowGetStackClass2();

    private static native void testCallNonvirtual();

    private static native void testNewStringObject();

    private interface SimpleInterface {
        void a();
    }

    private static class DummyInvocationHandler implements InvocationHandler {
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            return null;
        }
    }

    private static void testProxyGetMethodID() {
        InvocationHandler handler = new DummyInvocationHandler();
        SimpleInterface proxy =
                (SimpleInterface) Proxy.newProxyInstance(SimpleInterface.class.getClassLoader(),
                        new Class<?>[] {SimpleInterface.class}, handler);
        if (testGetMethodID(SimpleInterface.class) == 0) {
            throw new AssertionError();
        }
        if (testGetMethodID(proxy.getClass()) == 0) {
            throw new AssertionError();
        }
    }

    private static native long testGetMethodID(Class<?> c);

    // Exercise GC and JNI critical sections in parallel.
    private static void testJniCriticalSectionAndGc() {
        Thread runGcThread = new Thread(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < 10; ++i) {
                    Runtime.getRuntime().gc();
                }
            }
        });
        Thread jniCriticalThread = new Thread(new Runnable() {
            @Override
            public void run() {
                final int arraySize = 32;
                byte[] array0 = new byte[arraySize];
                byte[] array1 = new byte[arraySize];
                enterJniCriticalSection(arraySize, array0, array1);
            }
        });
        jniCriticalThread.start();
        runGcThread.start();
        try {
            jniCriticalThread.join();
            runGcThread.join();
        } catch (InterruptedException ignored) {}
    }

    private static native void enterJniCriticalSection(int arraySize, byte[] array0, byte[] array);

    private static native void testInvokeLambdaMethod(LambdaInterface iface);

    private static native void testInvokeLambdaDefaultMethod(LambdaInterface iface);

    // Test invoking @FastNative methods works correctly.

    // Return sum of a+b+c.
    @FastNative
    static native int intFastNativeMethod(int a, int b, int c);

    private static void testFastNativeMethods() {
      int returns[] = { 0, 3, 6, 9, 12 };
      for (int i = 0; i < returns.length; i++) {
        int result = intFastNativeMethod(i, i, i);
        if (returns[i] != result) {
          System.out.println("FastNative Int Run " + i + " with " + returns[i] + " vs " + result);
          throw new AssertionError();
        }
      }
    }

    // Smoke test for @CriticalNative
    // TODO: Way more thorough tests since it involved quite a bit of changes.

    // Return sum of a+b+c.
    @CriticalNative
    static native int intCriticalNativeMethod(int a, int b, int c);

    private static void testCriticalNativeMethods() {
      int returns[] = { 3, 6, 9, 12, 15 };
      for (int i = 0; i < returns.length; i++) {
        int result = intCriticalNativeMethod(i, i+1, i+2);
        if (returns[i] != result) {
          System.out.println("CriticalNative Int Run " + i + " with " + returns[i] + " vs " + result);
          throw new AssertionError();
        }
      }
    }

    private static native boolean isSlowDebug();

    private static void testClinitMethodLookup() {
      // Expect this to print <NSME Exception>
      try {
        System.out.println("Clinit Lookup: ClassWithoutClinit: " + methodString(lookupClinit(ClassWithoutClinit.class)));
      } catch (NoSuchMethodError e) {
        System.out.println("Clinit Lookup: ClassWithoutClinit: <NSME Exception>");
      }
      // Expect this to print <clinit>
      try {
        System.out.println("Clinit Lookup: ClassWithClinit: " + methodString(lookupClinit(ClassWithClinit.class)));
      } catch (NoSuchMethodError e) {
        System.out.println("Clinit Lookup: ClassWithClinit: <NSME Exception>");
      }
   }

    private static String methodString(java.lang.reflect.Executable method) {
      if (method == null) {
        return "<<null>>";
      } else {
        return method.toString() + "(Class: " + method.getClass().toString() + ")";
      }
    }
    private static native java.lang.reflect.Executable lookupClinit(Class kls);

    private static class ClassWithoutClinit {
    }
    private static class ClassWithClinit {
      static {}
    }

  private static void testDoubleLoad(String library) {
    // Test that nothing observably happens on loading "library" again.
    System.loadLibrary(library);

    // Now load code in a separate classloader and try to let it load.
    ClassLoader loader = createClassLoader();
    try {
      Class<?> aClass = loader.loadClass("A");
      Method runMethod = aClass.getDeclaredMethod("run", String.class);
      runMethod.invoke(null, library);
    } catch (InvocationTargetException ite) {
      if (ite.getCause() instanceof UnsatisfiedLinkError) {
        if (!(loader instanceof java.net.URLClassLoader)) {
          String msg = ite.getCause().getMessage();
          String pattern = "^Shared library .*libarttest.* already opened by ClassLoader.*" +
                           "004-JniTest.jar.*; can't open in ClassLoader.*004-JniTest-ex.jar.*";
          if (!Pattern.matches(pattern, msg)) {
            throw new RuntimeException("Could not find pattern in message", ite.getCause());
          }
        }
        System.out.println("Got UnsatisfiedLinkError for duplicate loadLibrary");
      } else {
        throw new RuntimeException(ite);
      }
    } catch (Throwable t) {
      // Anything else just let die.
      throw new RuntimeException(t);
    }
  }

  private static ClassLoader createClassLoader() {
    String location = System.getenv("DEX_LOCATION");
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(String.class, ClassLoader.class);

      return (ClassLoader)ctor.newInstance(location + "/004-JniTest-ex.jar",
                                           Main.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI. Use URLClassLoader.
      try {
        return new java.net.URLClassLoader(
            new java.net.URL[] { new java.net.URL("file://" + location + "/classes-ex/") });
      } catch (Throwable t) {
        throw new RuntimeException(t);
      }
    } catch (Throwable t) {
      throw new RuntimeException(t);
    }
  }
}

@FunctionalInterface
interface LambdaInterface {
  public void sayHi();
  public default void sayHiTwice() {
    sayHi();
    sayHi();
  }
}

class JniCallNonvirtualTest {
    public boolean nonstaticMethodSuperCalled = false;
    public boolean nonstaticMethodSubCalled = false;

    private static native void testCallNonvirtual();

    public JniCallNonvirtualTest() {
        System.out.println("Super.<init>");
    }

    public static void staticMethod() {
        System.out.println("Super.staticMethod");
    }

    public void nonstaticMethod() {
        System.out.println("Super.nonstaticMethod");
        nonstaticMethodSuperCalled = true;
    }
}

class JniCallNonvirtualTestSubclass extends JniCallNonvirtualTest {

    public JniCallNonvirtualTestSubclass() {
        System.out.println("Subclass.<init>");
    }

    public static void staticMethod() {
        System.out.println("Subclass.staticMethod");
    }

    public void nonstaticMethod() {
        System.out.println("Subclass.nonstaticMethod");
        nonstaticMethodSubCalled = true;
    }
}
