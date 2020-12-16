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

public class Test994 {
  public static final Breakpoint.Manager MANAGER = new Breakpoint.Manager();
  public static void doNothing() {}

  // Method with multiple paths we can break on.
  public static void doMultiPath(boolean bit) {
    doNothing();
    if (bit) {
      System.out.println("\targument was true");
    } else {
      System.out.println("\targument was false");
    }
    doNothing();
  }

  public static void notifyBreakpointReached(Thread thr, Executable e, long loc) {
    System.out.println(
        "\tBreakpoint reached: " + e + " @ line=" + Breakpoint.locationToLine(e, loc));
  }

  public static void run() throws Exception {
    // Set up breakpoints
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
    Breakpoint.startBreakpointWatch(
        Test994.class,
        Test994.class.getDeclaredMethod(
            "notifyBreakpointReached", Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());

    Method multipath_method = Test994.class.getDeclaredMethod("doMultiPath", Boolean.TYPE);

    Breakpoint.LineNumber[] lines = Breakpoint.getLineNumberTable(multipath_method);

    // Make sure everything is in the same order.
    Arrays.sort(lines);

    boolean[] values = new boolean[] { true, false };

    for (Breakpoint.LineNumber line : lines) {
      MANAGER.clearAllBreakpoints();
      MANAGER.setBreakpoint(multipath_method, line.location);
      for (boolean arg : values) {
        System.out.println("Breaking on line: " + line.line + " calling with arg: " + arg);
        doMultiPath(arg);
      }
    }

    Breakpoint.stopBreakpointWatch(Thread.currentThread());
  }
}
