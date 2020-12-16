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
import java.util.Collection;
import java.util.Objects;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;

public class Test1929 {
  public static boolean PRINT_FULL_EXCEPTION = false;
  public static ExceptionHandler HANDLER = null;
  public static Collection<Executable> TEST_METHODS;

  public static void doNothing() {};
  static {
    try {
      TEST_METHODS = Arrays.asList(
        Test1929.class.getDeclaredMethod("doThrow"),
        Test1929.class.getDeclaredMethod("throwCatchBaseTestException"),
        Test1929.class.getDeclaredMethod("throwCatchBaseTestExceptionTwice"),
        Test1929.class.getDeclaredMethod("throwCatchTestException"),
        Test1929.class.getDeclaredMethod("throwCatchTestExceptionTwice"),
        Test1929.class.getDeclaredMethod("throwCatchTestExceptionNoRethrow"));
    } catch (Exception e) {
      throw new Error("Unable to list test methods!", e);
    }
  }

  public static interface ExceptionHandler {
    public void exceptionOccurred(
        Executable m, long loc, Throwable exception);
  }

  private static void PrintStack() {
    System.out.println("\tCurrent Stack:");
    for (StackTrace.StackFrameData e : StackTrace.GetStackTrace(Thread.currentThread())) {
      if (Objects.equals(e.method.getDeclaringClass().getPackage(), Test1929.class.getPackage())) {
        System.out.println("\t\t" + e.method + " @ line = " +
            Breakpoint.locationToLine(e.method, e.current_location));
      }
    }
  }

  public static void ExceptionCatchEvent(
      Thread thr, Executable method, long location, Throwable exception) {
    System.out.println(thr.getName() + ": " + method + " @ line = " +
        Breakpoint.locationToLine(method, location) + " caught " +
        exception.getClass() + ": " + exception.getMessage());
    PrintStack();
    if (PRINT_FULL_EXCEPTION) {
      System.out.print("exception is: ");
      exception.printStackTrace(System.out);
    }
    if (HANDLER != null && TEST_METHODS.contains(method)) {
      HANDLER.exceptionOccurred(method, location, exception);
    }
  }

  public static class BaseTestException extends Error {
    public BaseTestException(String e) { super(e); }
    public BaseTestException(String e, Throwable t) { super(e, t); }
  }
  public static class TestException extends BaseTestException {
    public TestException(String e) { super(e); }
    public TestException(String e, Throwable t) { super(e, t); }
  }

  public static class TestExceptionNoRethrow extends TestException {
    public TestExceptionNoRethrow(String e) { super(e); }
    public TestExceptionNoRethrow(String e, Throwable t) { super(e, t); }
  }

  public static class DoNothingHandler implements ExceptionHandler {
    public void exceptionOccurred(Executable m, long loc, Throwable exception) {
      System.out.println("\tDoing nothing!");
      return;
    }
  }

  public static class ThrowCatchBase implements ExceptionHandler {
    public void exceptionOccurred(Executable m, long loc, Throwable exception) {
      System.out.println("\tThrowing BaseTestException and catching it!");
      try {
        throw new BaseTestException("ThrowBaseHandler during throw from " + m + " @ line = " +
            Breakpoint.locationToLine(m, loc), exception);
      } catch (BaseTestException t) {
        System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
        if (PRINT_FULL_EXCEPTION) {
          t.printStackTrace(System.out);
        }
      }
    }
  }
  public static class ThrowBaseTestExceptionHandler implements ExceptionHandler {
    public void exceptionOccurred(Executable m, long loc, Throwable exception) {
      System.out.println("\tThrowing BaseTestException!");
      throw new BaseTestException("ThrowBaseHandler during throw from " + m + " @ line = " +
          Breakpoint.locationToLine(m, loc), exception);
    }
  }

  public static class ThrowTestExceptionNoRethrowHandler implements ExceptionHandler {
    public void exceptionOccurred(Executable m, long loc, Throwable exception) {
      if (exception instanceof TestExceptionNoRethrow) {
        System.out.println("\tInstance of TestExceptionNoRethrow was thrown. Not throwing again.");
      } else {
        System.out.println("\tThrowing TestExceptionNoRethrow!");
        throw new TestExceptionNoRethrow("ThrowTestExceptionNoRethrowHandler during throw from " +
            m + " @ line = " + Breakpoint.locationToLine(m, loc), exception);
      }
    }
  }
  public static void doThrow() {
    throw new TestException("doThrow");
  }

  public static class DoThrowClass implements Runnable {
    public void run() { doThrow(); }
  }

  public static void throwCatchBaseTestException() {
    try {
      throw new TestException("throwCatchBaseTestException");
    } catch (BaseTestException t) {
      System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
      if (PRINT_FULL_EXCEPTION) {
        t.printStackTrace(System.out);
      }
    }
  }

  public static class DoThrowCatchBaseTestException implements Runnable {
    public void run() { throwCatchBaseTestException(); }
  }

  // dx/d8/jack all do an optimization around catch blocks that (while legal) breaks assumptions
  // this test relies on so we have the actual implementation be corrected smali. This does work
  // for RI however.
  public static final class Impl {
    private Impl() {}
    public static void throwCatchBaseTestExceptionTwiceImpl() {
      try {
        try {
          throw new TestException("throwCatchBaseTestExceptionTwice");
        } catch (BaseTestException t) {
          System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
          if (PRINT_FULL_EXCEPTION) {
            t.printStackTrace(System.out);
          }
        }
      } catch (BaseTestException t) {
        System.out.println("2nd Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
        if (PRINT_FULL_EXCEPTION) {
          t.printStackTrace(System.out);
        }
      }
    }

    public static void throwCatchTestExceptionTwiceImpl() {
      try {
        try {
          throw new TestException("throwCatchTestExceptionTwice");
        } catch (TestException t) {
          System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
          if (PRINT_FULL_EXCEPTION) {
            t.printStackTrace(System.out);
          }
        }
      } catch (TestException t) {
        System.out.println("2nd Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
        if (PRINT_FULL_EXCEPTION) {
          t.printStackTrace(System.out);
        }
      }
    }
  }

  public static void throwCatchBaseTestExceptionTwice() {
    // The implementation of this has to change depending upon the runtime slightly due to compiler
    // optimizations present in DX/D8/Jack.
    Impl.throwCatchBaseTestExceptionTwiceImpl();
  }

  public static class DoThrowCatchBaseTestExceptionTwice implements Runnable {
    public void run() { throwCatchBaseTestExceptionTwice(); }
  }

  public static void throwCatchTestException() {
    try {
      throw new TestException("throwCatchTestException");
    } catch (TestException t) {
      System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
      if (PRINT_FULL_EXCEPTION) {
        t.printStackTrace(System.out);
      }
    }
  }

  public static class DoThrowCatchTestException implements Runnable {
    public void run() { throwCatchTestException(); }
  }

  public static void throwCatchTestExceptionTwice() {
    // The implementation of this has to change depending upon the runtime slightly due to compiler
    // optimizations present in DX/D8/Jack.
    Impl.throwCatchTestExceptionTwiceImpl();
  }

  public static class DoThrowCatchTestExceptionTwice implements Runnable {
    public void run() { throwCatchTestExceptionTwice(); }
  }

  public static void throwCatchTestExceptionNoRethrow() {
    try {
      throw new TestException("throwCatchTestExceptionNoRethrow");
    } catch (TestExceptionNoRethrow t) {
      System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
      if (PRINT_FULL_EXCEPTION) {
        t.printStackTrace(System.out);
      }
    }
  }

  public static class DoThrowCatchTestExceptionNoRethrow implements Runnable {
    public void run() { throwCatchTestExceptionNoRethrow(); }
  }

  public static void run() throws Exception {
    // Set up breakpoints
    Exceptions.setupExceptionTracing(
        Test1929.class,
        TestException.class,
        null,
        Test1929.class.getDeclaredMethod(
            "ExceptionCatchEvent",
            Thread.class,
            Executable.class,
            Long.TYPE,
            Throwable.class));
    Exceptions.enableExceptionCatchEvent(Thread.currentThread());

    ExceptionHandler[] handlers = new ExceptionHandler[] {
      new DoNothingHandler(),
      new ThrowCatchBase(),
      new ThrowBaseTestExceptionHandler(),
      new ThrowTestExceptionNoRethrowHandler(),
    };

    Runnable[] tests = new Runnable[] {
      new DoThrowClass(),
      new DoThrowCatchBaseTestException(),
      new DoThrowCatchBaseTestExceptionTwice(),
      new DoThrowCatchTestException(),
      new DoThrowCatchTestExceptionTwice(),
      new DoThrowCatchTestExceptionNoRethrow(),
    };

    for (ExceptionHandler handler : handlers) {
      for (Runnable test : tests) {
        try {
          HANDLER = handler;
          System.out.printf("Test \"%s\": Running breakpoint with handler \"%s\"\n",
              test.getClass().getName(), handler.getClass().getName());
          test.run();
          System.out.printf("Test \"%s\": No error caught with handler \"%s\"\n",
              test.getClass().getName(), handler.getClass().getName());
        } catch (Throwable e) {
          System.out.printf("Test \"%s\": Caught error %s:\"%s\" with handler \"%s\"\n",
              test.getClass().getName(),
              e.getClass().getName(),
              e.getMessage(),
              handler.getClass().getName());
          if (PRINT_FULL_EXCEPTION) {
            e.printStackTrace(System.out);
          }
        }
        System.out.printf("Test \"%s\": Finished running with handler \"%s\"\n",
            test.getClass().getName(), handler.getClass().getName());
        HANDLER = null;
      }
    }
    Exceptions.disableExceptionCatchEvent(Thread.currentThread());
  }
}
