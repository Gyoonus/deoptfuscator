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

public class Test1928 {
  public static boolean PRINT_FULL_EXCEPTION = false;
  public static ExceptionHandler HANDLER = null;

  public static interface ExceptionHandler {
    public void exceptionOccurred(
        Executable m, long loc, Throwable exception, Executable catch_m, long catch_l);
  }

  private static void PrintStack() {
    System.out.println("\tCurrent Stack:");
    for (StackTrace.StackFrameData e : StackTrace.GetStackTrace(Thread.currentThread())) {
      if (Objects.equals(e.method.getDeclaringClass().getPackage(), Test1928.class.getPackage())) {
        System.out.println("\t\t" + e.method + " @ line = " +
            Breakpoint.locationToLine(e.method, e.current_location));
      }
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
    if (HANDLER != null) {
      HANDLER.exceptionOccurred(
          throw_method, throw_location, exception, catch_method, catch_location);
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
    public void exceptionOccurred(
        Executable m, long loc, Throwable exception, Executable catch_m, long catch_l) {
      System.out.println("\tDoing nothing!");
      return;
    }
  }

  public static class ThrowCatchBase implements ExceptionHandler {
    public void exceptionOccurred(
        Executable m, long loc, Throwable exception, Executable catch_m, long catch_l) {
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
    // Set up
    Exceptions.setupExceptionTracing(
        Test1928.class,
        TestException.class,
        Test1928.class.getDeclaredMethod(
            "ExceptionEvent",
            Thread.class,
            Executable.class,
            Long.TYPE,
            Throwable.class,
            Executable.class,
            Long.TYPE),
        null);
    Exceptions.enableExceptionEvent(Thread.currentThread());

    ExceptionHandler[] handlers = new ExceptionHandler[] {
      new DoNothingHandler(),
      new ThrowCatchBase(),
    };

    Runnable[] tests = new Runnable[] {
      new DoThrowClass(),
      new DoThrowCatchBaseTestException(),
      new DoThrowCatchTestException(),
      new DoThrowCatchTestExceptionNoRethrow(),
    };

    for (ExceptionHandler handler : handlers) {
      for (Runnable test : tests) {
        try {
          HANDLER = handler;
          System.out.printf("Test \"%s\": Running with handler \"%s\"\n",
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
    Exceptions.disableExceptionEvent(Thread.currentThread());
  }
}
