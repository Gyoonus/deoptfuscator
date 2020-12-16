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

public class Test997 {
  static final int NO_LAST_LINE_NUMBER = -1;
  static int LAST_LINE_NUMBER = NO_LAST_LINE_NUMBER;
  static Method DO_MULTIPATH_METHOD;

  static {
    try {
      DO_MULTIPATH_METHOD = Test997.class.getDeclaredMethod("doMultiPath", Boolean.TYPE);
    } catch (Exception e) {
      throw new Error("could not find method doMultiPath", e);
    }
  }

  // Function that acts simply to ensure there are multiple lines.
  public static void doNothing() {}

  // Method with multiple paths we can break on.
  public static void doMultiPath(boolean bit) {
    doNothing();
    if (bit) {
      doNothing();
    } else {
      doNothing();
    }
    doNothing();
  }

  public static void notifySingleStep(Thread thr, Executable e, long loc) {
    if (!e.equals(DO_MULTIPATH_METHOD)) {
      // Only report steps in doMultiPath
      return;
    }
    int cur_line = Breakpoint.locationToLine(e, loc);
    // Only report anything when the line number changes. This is so we can run this test against
    // both the RI and ART and also to prevent front-end compiler changes from affecting output.
    if (LAST_LINE_NUMBER == NO_LAST_LINE_NUMBER || LAST_LINE_NUMBER != cur_line) {
      LAST_LINE_NUMBER = cur_line;
      System.out.println("Single step: " + e + " @ line=" + cur_line);
    }
  }

  public static void resetTest() {
    LAST_LINE_NUMBER = NO_LAST_LINE_NUMBER;
  }

  public static void run() throws Exception {
    boolean[] values = new boolean[] { true, false };
    Trace.enableSingleStepTracing(Test997.class,
        Test997.class.getDeclaredMethod(
            "notifySingleStep", Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());
    for (boolean arg : values) {
      System.out.println("Stepping through doMultiPath(" + arg + ")");
      resetTest();
      doMultiPath(arg);
    }

    Trace.disableTracing(Thread.currentThread());
  }
}
