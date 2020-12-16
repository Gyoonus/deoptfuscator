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
import java.lang.reflect.Executable;
import java.lang.reflect.Method;

public class Test995 {
  public static final Breakpoint.Manager MANAGER = new Breakpoint.Manager();
  public static BreakpointHandler HANDLER = null;

  public static void doNothing() { }

  public static interface BreakpointHandler {
    public void breakpointReached(Executable e, long loc);
  }

  public static void breakpoint() {
    return;
  }

  public static void breakpointCatchLate() {
    doNothing();
    try {
      doNothing();
    } catch (Throwable t) {
      System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
    }
  }

  public static void breakpointCatch() {
    try {
      doNothing();
    } catch (Throwable t) {
      System.out.println("Caught " + t.getClass().getName() + ": \"" + t.getMessage() + "\"");
    }
  }

  public static void notifyBreakpointReached(Thread thr, Executable e, long loc) {
    System.out.println("\tBreakpoint: " + e + " @ line=" + Breakpoint.locationToLine(e, loc));
    HANDLER.breakpointReached(e, loc);
  }


  public static BreakpointHandler makeHandler(String name, BreakpointHandler h) {
    return new BreakpointHandler() {
      public String toString() {
        return name;
      }
      public void breakpointReached(Executable e, long loc) {
        h.breakpointReached(e, loc);
      }
    };
  }

  public static Runnable makeTest(String name, Runnable test) {
    return new Runnable() {
      public String toString() { return name; }
      public void run() { test.run(); }
    };
  }

  public static void run() throws Exception {
    // Set up breakpoints
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
    Breakpoint.startBreakpointWatch(
        Test995.class,
        Test995.class.getDeclaredMethod(
            "notifyBreakpointReached", Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());

    Method breakpoint_method = Test995.class.getDeclaredMethod("breakpoint");
    Method breakpoint_catch_method = Test995.class.getDeclaredMethod("breakpointCatch");
    Method breakpoint_catch_late_method = Test995.class.getDeclaredMethod("breakpointCatchLate");
    MANAGER.setBreakpoint(breakpoint_method, Breakpoint.getStartLocation(breakpoint_method));
    MANAGER.setBreakpoint(
        breakpoint_catch_method, Breakpoint.getStartLocation(breakpoint_catch_method));
    MANAGER.setBreakpoint(
        breakpoint_catch_late_method, Breakpoint.getStartLocation(breakpoint_catch_late_method));

    BreakpointHandler[] handlers = new BreakpointHandler[] {
      makeHandler("do nothing", (e, l) -> {}),
      makeHandler("throw", (e, l) -> { throw new Error("throwing error!"); }),
    };

    Runnable[] tests = new Runnable[] {
      makeTest("call Test995::breakpoint", Test995::breakpoint),
      makeTest("call Test995::breakpointCatch", Test995::breakpointCatch),
      makeTest("call Test995::breakpointCatchLate", Test995::breakpointCatchLate),
      makeTest("catch subroutine Test995::breakpoint",
          () -> {
            try {
              breakpoint();
            } catch (Throwable t) {
              System.out.printf("Caught %s:\"%s\"\n", t.getClass().getName(), t.getMessage());
            }
          }),
    };

    for (BreakpointHandler handler : handlers) {
      for (Runnable test : tests) {
        try {
          HANDLER = handler;
          System.out.printf("Test \"%s\": Running breakpoint with handler \"%s\"\n",
              test, handler);
          test.run();
          System.out.printf("Test \"%s\": No error caught with handler \"%s\"\n",
              test, handler);
        } catch (Throwable e) {
          System.out.printf("Test \"%s\": Caught error %s:\"%s\" with handler \"%s\"\n",
              test, e.getClass().getName(), e.getMessage(), handler);
        }
        System.out.printf("Test \"%s\": Finished running with handler \"%s\"\n", test, handler);
        HANDLER = null;
      }
    }

    MANAGER.clearAllBreakpoints();
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
  }
}
