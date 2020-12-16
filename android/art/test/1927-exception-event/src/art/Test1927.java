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

import java.util.Arrays;
import java.util.Objects;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;

public class Test1927 {
  private static boolean PRINT_FULL_EXCEPTION = false;
  private static void PrintStack() {
    System.out.println("\tCurrent Stack:");
    for (StackTrace.StackFrameData e : StackTrace.GetStackTrace(Thread.currentThread())) {
      if (Objects.equals(e.method.getDeclaringClass().getPackage(), Test1927.class.getPackage())) {
        System.out.println("\t\t" + e.method + " @ line = " +
            Breakpoint.locationToLine(e.method, e.current_location));
      }
    }
  }

  public static void ExceptionCatchEvent(Thread thr,
                                         Executable catch_method,
                                         long catch_location,
                                         Throwable exception) {
    System.out.println(thr.getName() + ": " + catch_method + " @ line = " +
        Breakpoint.locationToLine(catch_method, catch_location) + " caught " +
        exception.getClass() + ": " + exception.getMessage());
    PrintStack();
    if (PRINT_FULL_EXCEPTION) {
      System.out.print("exception is: ");
      exception.printStackTrace(System.out);
    }
  }

  public static void ExceptionEvent(Thread thr,
                                    Executable throw_method,
                                    long throw_location,
                                    Throwable exception,
                                    Executable catch_method,
                                    long catch_location) {
    System.out.println(thr.getName() + ": " + throw_method + " @ line = " +
        Breakpoint.locationToLine(throw_method, throw_location) + " throws " +
        exception.getClass() + ": " + exception.getMessage());
    String catch_message;
    if (catch_method == null) {
      catch_message = "<UNKNOWN>";
    } else {
      catch_message = catch_method.toString() + " @ line = " +
          Breakpoint.locationToLine(catch_method, catch_location);
    }
    PrintStack();
    System.out.println("\tWill be caught by: " + catch_message);
    if (PRINT_FULL_EXCEPTION) {
      System.out.print("exception is: ");
      exception.printStackTrace(System.out);
    }
  }

  public static class TestException extends Error {
    public TestException(String s) { super(s); }
    public TestException() { super("from java"); }
  }

  // Possibilities
  // ( -> is a JNI/Java call.)
  // Furthest left catches/clears the exception
  // Furthest right throws it.
  // J
  // N
  // J -> J
  // J -> N
  // N -> J
  // N -> N
  // J -> J -> J
  // J -> J -> N
  // J -> N -> J
  // J -> N -> N
  // N -> J -> J
  // N -> J -> N
  // N -> N -> J
  // N -> N -> N
  // extra -> N -> J -> J
  // extra -> N -> J -> N
  // extra -> N -> N -> J
  // extra -> N -> N -> N

  public static void terminal_J() {
    throw new TestException();
  }

  public static native void terminal_N();

  public static void test_J() {
    try {
      throw new TestException();
    } catch (TestException e) {
      printException(e);
    }
  }

  // Do test_J but native
  public static native void test_N();

  public static void test_J_J() {
    try {
      terminal_J();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static void test_J_N() {
    try {
      terminal_N();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static native void test_N_J();
  public static native void test_N_N();

  public static void intermediate_J_J() { terminal_J(); }
  public static void intermediate_J_N() { terminal_N(); }
  public static native void intermediate_N_J();
  public static native void intermediate_N_N();

  public static void test_J_J_J() {
    try {
      intermediate_J_J();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static void test_J_J_N() {
    try {
      intermediate_J_N();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static void test_J_N_J() {
    try {
      intermediate_N_J();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static void test_J_N_N() {
    try {
      intermediate_N_N();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static native void test_N_J_J();
  public static native void test_N_J_N();
  public static native void test_N_N_J();
  public static native void test_N_N_N();

  public static void test_extra_N_J_J() {
    try {
      test_N_J_J();
    } catch (TestException e) {
      printException(e);
    }
  }
  public static void test_extra_N_J_N() {
    try {
      test_N_J_N();
    } catch (TestException e) {
      printException(e);
    }
  }
  public static void test_extra_N_N_J() {
    try {
      test_N_N_J();
    } catch (TestException e) {
      printException(e);
    }
  }
  public static void test_extra_N_N_N() {
    try {
      test_N_N_N();
    } catch (TestException e) {
      printException(e);
    }
  }

  public static void printException(Throwable e) {
    System.out.println("Caught exception: " + e);
    if (PRINT_FULL_EXCEPTION) {
      e.printStackTrace(System.out);
    }
  }

  public static void run() throws Exception {
    // Make sure classes are loaded first.
    System.out.println(TestException.class.toString());
    Exceptions.setupExceptionTracing(
        Test1927.class,
        TestException.class,
        Test1927.class.getDeclaredMethod(
            "ExceptionEvent",
            Thread.class,
            Executable.class,
            Long.TYPE,
            Throwable.class,
            Executable.class,
            Long.TYPE),
        Test1927.class.getDeclaredMethod(
            "ExceptionCatchEvent",
            Thread.class,
            Executable.class,
            Long.TYPE,
            Throwable.class));
    Exceptions.enableExceptionEvent(Thread.currentThread());
    Exceptions.enableExceptionCatchEvent(Thread.currentThread());
    System.out.println("Running test_J");
    test_J();
    System.out.println("Running test_N()");
    test_N();
    System.out.println("Running test_J_J()");
    test_J_J();
    System.out.println("Running test_J_N()");
    test_J_N();
    System.out.println("Running test_N_J()");
    test_N_J();
    System.out.println("Running test_N_N()");
    test_N_N();
    System.out.println("Running test_J_J_J()");
    test_J_J_J();
    System.out.println("Running test_J_J_N()");
    test_J_J_N();
    System.out.println("Running test_J_N_J()");
    test_J_N_J();
    System.out.println("Running test_J_N_N()");
    test_J_N_N();
    System.out.println("Running test_N_J_J()");
    test_N_J_J();
    System.out.println("Running test_N_J_N()");
    test_N_J_N();
    System.out.println("Running test_N_N_J()");
    test_N_N_J();
    System.out.println("Running test_N_N_N()");
    test_N_N_N();
    System.out.println("Running test_extra_N_J_J()");
    test_extra_N_J_J();
    System.out.println("Running test_extra_N_J_N()");
    test_extra_N_J_N();
    System.out.println("Running test_extra_N_N_J()");
    test_extra_N_N_J();
    System.out.println("Running test_extra_N_N_N()");
    test_extra_N_N_N();
    Exceptions.disableExceptionCatchEvent(Thread.currentThread());
    Exceptions.disableExceptionEvent(Thread.currentThread());
  }
}
