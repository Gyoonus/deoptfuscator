/*
 * Copyright (C) 2011 The Android Open Source Project
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
import java.util.Set;
import java.util.HashSet;

public class Test989 {
  static boolean PRINT_STACK_TRACE = false;
  static Set<Method> testMethods = new HashSet<>();

  static MethodTracer currentTracer = new MethodTracer() {
    public void methodEntry(Object o) { return; }
    public void methodExited(Object o, boolean e, Object r) { return; }
  };

  private static boolean DISABLE_TRACING = false;

  static {
    try {
      testMethods.add(Test989.class.getDeclaredMethod("doNothing"));
      testMethods.add(Test989.class.getDeclaredMethod("doNothingNative"));
      testMethods.add(Test989.class.getDeclaredMethod("throwA"));
      testMethods.add(Test989.class.getDeclaredMethod("throwANative"));
      testMethods.add(Test989.class.getDeclaredMethod("returnFloat"));
      testMethods.add(Test989.class.getDeclaredMethod("returnFloatNative"));
      testMethods.add(Test989.class.getDeclaredMethod("returnDouble"));
      testMethods.add(Test989.class.getDeclaredMethod("returnDoubleNative"));
      testMethods.add(Test989.class.getDeclaredMethod("returnValue"));
      testMethods.add(Test989.class.getDeclaredMethod("returnValueNative"));
      testMethods.add(Test989.class.getDeclaredMethod("acceptValue", Object.class));
      testMethods.add(Test989.class.getDeclaredMethod("acceptValueNative", Object.class));
      testMethods.add(Test989.class.getDeclaredMethod("tryCatchExit"));
    } catch (Exception e) {
      throw new Error("Bad static!", e);
    }
  }

  // Disables tracing only on RI. Used to work around an annoying piece of behavior where in the
  // RI throwing an exception in an exit hook causes the exit hook to be re-executed. This leads
  // to an infinite loop on the RI.
  private static void disableTraceForRI() {
    if (!System.getProperty("java.vm.name").equals("Dalvik")) {
      Trace.disableTracing(Thread.currentThread());
    }
  }

  private static String getInfo(Object m, boolean exception, Object result) {
    String out = m.toString() + " returned ";
    if (exception) {
      out += "<exception>";
    } else {
      out += result;
    }
    return out;
  }

  public static interface MethodTracer {
    public void methodEntry(Object m);
    public void methodExited(Object m, boolean exception, Object result);
    public default Class<?> entryException() { return null; }
    public default Class<?> exitException() { return null; }
  }

  public static class NormalTracer implements MethodTracer {
    public void methodEntry(Object m) {
      if (testMethods.contains(m)) {
        System.out.println("Normal: Entering " + m);
      }
    }
    public void methodExited(Object m, boolean exception, Object result) {
      if (testMethods.contains(m)) {
        System.out.println("Normal: Leaving " + getInfo(m, exception, result));
      }
    }
  }

  public static class ThrowEnterTracer implements MethodTracer {
    public void methodEntry(Object m) {
      if (testMethods.contains(m)) {
        System.out.println("ThrowEnter: Entering " + m);
        throw new ErrorB("Throwing error while entering " + m);
      }
    }
    public void methodExited(Object m, boolean exception, Object result) {
      if (testMethods.contains(m)) {
        System.out.println("ThrowEnter: Leaving " + getInfo(m, exception, result));
      }
    }
    public Class<?> entryException() { return ErrorB.class; }
  }

  public static class ThrowExitTracer implements MethodTracer {
    public void methodEntry(Object m) {
      if (testMethods.contains(m)) {
        System.out.println("ThrowExit: Entering " + m);
      }
    }
    public void methodExited(Object m, boolean exception, Object result) {
      if (testMethods.contains(m)) {
        // The RI goes into an infinite loop if we throw exceptions in an ExitHook. See
        // disableTraceForRI for explanation.
        disableTraceForRI();
        System.out.println("ThrowExit: Leaving " + getInfo(m, exception, result));
        throw new ErrorB("Throwing error while exit " + getInfo(m, exception, result));
      }
    }
    public Class<?> exitException() { return ErrorB.class; }
  }

  public static class ThrowBothTracer implements MethodTracer {
    public void methodEntry(Object m) {
      if (testMethods.contains(m)) {
        System.out.println("ThrowBoth: Entering " + m);
        throw new ErrorB("Throwing error while entering " + m);
      }
    }
    public void methodExited(Object m, boolean exception, Object result) {
      if (testMethods.contains(m)) {
        // The RI goes into an infinite loop if we throw exceptions in an ExitHook. See
        // disableTraceForRI for explanation.
        disableTraceForRI();
        System.out.println("ThrowBoth: Leaving " + getInfo(m, exception, result));
        throw new ErrorC("Throwing error while exit " + getInfo(m, exception, result));
      }
    }
    public Class<?> entryException() { return ErrorB.class; }
    public Class<?> exitException() { return ErrorC.class; }
  }

  public static class ForceGCTracer implements MethodTracer {
    public void methodEntry(Object m) {
      if (System.getProperty("java.vm.name").equals("Dalvik")) {
        System.gc();
      }
    }
    public void methodExited(Object m, boolean exception, Object result) {
      if (System.getProperty("java.vm.name").equals("Dalvik")) {
        System.gc();
      }
    }
  }

  private static void maybeDisableTracing() throws Exception {
    if (DISABLE_TRACING) {
      Trace.disableTracing(Thread.currentThread());
    }
  }

  public static void baseNotifyMethodEntry(Object o) {
    currentTracer.methodEntry(o);
  }
  public static void baseNotifyMethodExit(Object o, boolean exception, Object res) {
    currentTracer.methodExited(o, exception, res);
  }

  private static void setupTracing() throws Exception {
    Trace.enableMethodTracing(
        Test989.class,
        Test989.class.getDeclaredMethod("baseNotifyMethodEntry", Object.class),
        Test989.class.getDeclaredMethod(
            "baseNotifyMethodExit", Object.class, Boolean.TYPE, Object.class),
        Thread.currentThread());
  }
  private static void setEntry(MethodTracer type) throws Exception {
    if (DISABLE_TRACING || !System.getProperty("java.vm.name").equals("Dalvik")) {
      Trace.disableTracing(Thread.currentThread());
      setupTracing();
    }
    currentTracer = type;
  }

  private static String testDescription(MethodTracer type, Runnable test) {
    return "test[" + type.getClass() + ", " + test.getClass() + "]";
  }

  private static Class<?> getExpectedError(MethodTracer t, MyRunnable r) {
    if (t.exitException() != null) {
      return t.exitException();
    } else if (t.entryException() != null) {
      return t.entryException();
    } else {
      return r.expectedThrow();
    }
  }

  private static void doTest(MethodTracer type, MyRunnable test) throws Exception {
    Class<?> expected = getExpectedError(type, test);

    setEntry(type);
    try {
      test.run();
      // Disabling method tracing just makes this test somewhat faster.
      maybeDisableTracing();
      if (expected == null) {
        System.out.println(
            "Received no exception as expected for " + testDescription(type, test) + ".");
        return;
      }
    } catch (Error t) {
      // Disabling method tracing just makes this test somewhat faster.
      maybeDisableTracing();
      if (expected == null) {
        throw new Error("Unexpected error occured: " + t + " for " + testDescription(type, test), t);
      } else if (!expected.isInstance(t)) {
        throw new Error("Expected error of type " + expected + " not " + t +
            " for " + testDescription(type, test), t);
      } else {
        System.out.println(
            "Received expected error for " + testDescription(type, test) + " - " + t);
        if (PRINT_STACK_TRACE) {
          t.printStackTrace();
        }
        return;
      }
    }
    System.out.println("Expected an error of type " + expected + " but got no exception for "
        + testDescription(type, test));
    // throw new Error("Expected an error of type " + expected + " but got no exception for "
    //     + testDescription(type, test));
  }

  public static interface MyRunnable extends Runnable {
    public default Class<?> expectedThrow() {
      return null;
    }
  }

  public static void run() throws Exception {
    MyRunnable[] testCases = new MyRunnable[] {
      new doNothingClass(),
      new doNothingNativeClass(),
      new throwAClass(),
      new throwANativeClass(),
      new returnValueClass(),
      new returnValueNativeClass(),
      new acceptValueClass(),
      new acceptValueNativeClass(),
      new tryCatchExitClass(),
      new returnFloatClass(),
      new returnFloatNativeClass(),
      new returnDoubleClass(),
      new returnDoubleNativeClass(),
    };
    MethodTracer[] tracers = new MethodTracer[] {
      new NormalTracer(),
      new ThrowEnterTracer(),
      new ThrowExitTracer(),
      new ThrowBothTracer(),
      new ForceGCTracer(),
    };

    setupTracing();
    for (MethodTracer t : tracers) {
      for (MyRunnable r : testCases) {
        doTest(t, r);
      }
    }

    maybeDisableTracing();
    System.out.println("Finished!");
    Trace.disableTracing(Thread.currentThread());
  }

  private static final class throwAClass implements MyRunnable {
    public void run() {
      throwA();
    }
    @Override
    public Class<?> expectedThrow() {
      return ErrorA.class;
    }
  }

  private static final class throwANativeClass implements MyRunnable {
    public void run() {
      throwANative();
    }
    @Override
    public Class<?> expectedThrow() {
      return ErrorA.class;
    }
  }

  private static final class tryCatchExitClass implements MyRunnable {
    public void run() {
      tryCatchExit();
    }
  }

  private static final class doNothingClass implements MyRunnable {
    public void run() {
      doNothing();
    }
  }

  private static final class doNothingNativeClass implements MyRunnable {
    public void run() {
      doNothingNative();
    }
  }

  private static final class acceptValueClass implements MyRunnable {
    public void run() {
      acceptValue(mkTestObject());
    }
  }

  private static final class acceptValueNativeClass implements MyRunnable {
    public void run() {
      acceptValueNative(mkTestObject());
    }
  }

  private static final class returnValueClass implements MyRunnable {
    public void run() {
      Object o = returnValue();
      System.out.println("returnValue returned: " + o);
    }
  }

  private static final class returnValueNativeClass implements MyRunnable {
    public void run() {
      Object o = returnValueNative();
      System.out.println("returnValueNative returned: " + o);
    }
  }

  private static final class returnFloatClass implements MyRunnable {
    public void run() {
      float d = returnFloat();
      System.out.println("returnFloat returned: " + d);
    }
  }

  private static final class returnFloatNativeClass implements MyRunnable {
    public void run() {
      float d = returnFloatNative();
      System.out.println("returnFloatNative returned: " + d);
    }
  }

  private static final class returnDoubleClass implements MyRunnable {
    public void run() {
      double d = returnDouble();
      System.out.println("returnDouble returned: " + d);
    }
  }

  private static final class returnDoubleNativeClass implements MyRunnable {
    public void run() {
      double d = returnDoubleNative();
      System.out.println("returnDoubleNative returned: " + d);
    }
  }

  private static class ErrorA extends Error {
    private static final long serialVersionUID = 0;
    public ErrorA(String s) { super(s); }
  }

  private static class ErrorB extends Error {
    private static final long serialVersionUID = 1;
    public ErrorB(String s) { super(s); }
  }

  private static class ErrorC extends Error {
    private static final long serialVersionUID = 2;
    public ErrorC(String s) { super(s); }
  }

  // Does nothing.
  public static void doNothing() { }

  public static void tryCatchExit() {
    try {
      Object o = mkTestObject();
      return;
    } catch (ErrorB b) {
      System.out.println("ERROR: Caught " + b);
      b.printStackTrace();
    } catch (ErrorC c) {
      System.out.println("ERROR: Caught " + c);
      c.printStackTrace();
    }
  }

  public static float returnFloat() {
    return doGetFloat();
  }

  public static double returnDouble() {
    return doGetDouble();
  }

  // Throws an ErrorA.
  public static void throwA() {
    doThrowA();
  }

  public static void doThrowA() {
    throw new ErrorA("Throwing Error A");
  }

  static final class TestObject {
    private int idx;
    public TestObject(int v) {
      this.idx = v;
    }
    @Override
    public String toString() {
      return "TestObject(" + idx + ")";
    }
  }

  static int counter = 0;
  public static Object mkTestObject() {
    return new TestObject(counter++);
  }

  public static void printObject(Object o) {
    System.out.println("Recieved " + o);
  }

  // Returns a newly allocated value.
  public static Object returnValue() {
    return mkTestObject();
  }

  public static void acceptValue(Object o) {
    printObject(o);
  }

  public static float doGetFloat() {
    return 1.618f;
  }

  public static double doGetDouble() {
    return 3.14159628;
  }

  // Calls mkTestObject from native code and returns it.
  public static native Object returnValueNative();
  // Calls printObject from native code.
  public static native void acceptValueNative(Object t);
  public static native void doNothingNative();
  public static native void throwANative();
  public static native float returnFloatNative();
  public static native double returnDoubleNative();
}
