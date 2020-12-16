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

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.function.Consumer;

public class Test1915 {
  public static final int SET_VALUE = 1337;
  public static final String TARGET_VAR = "TARGET";

  public static void reportValue(Object val) {
    System.out.println("\tValue is '" + val + "'");
  }
  public static interface ThrowRunnable {
    public void run() throws Exception;
  }

  public static void IntMethod(ThrowRunnable safepoint) throws Exception {
    int TARGET = 42;
    safepoint.run();
    reportValue(TARGET);
  }

  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    final Method target = Test1915.class.getDeclaredMethod("IntMethod", ThrowRunnable.class);
    // Get Variable.
    System.out.println("GetLocalInt on current thread!");
    IntMethod(() -> {
      StackTrace.StackFrameData frame = FindStackFrame(target);
      int depth = FindExpectedFrameDepth(frame);
      int slot = FindSlot(frame);
      int value = Locals.GetLocalVariableInt(Thread.currentThread(), depth, slot);
      System.out.println("From GetLocalInt(), value is " + value);
    });
    // Set Variable.
    System.out.println("SetLocalInt on current thread!");
    IntMethod(() -> {
      StackTrace.StackFrameData frame = FindStackFrame(target);
      int depth = FindExpectedFrameDepth(frame);
      int slot = FindSlot(frame);
      Locals.SetLocalVariableInt(Thread.currentThread(), depth, slot, SET_VALUE);
    });
  }

  public static int FindSlot(StackTrace.StackFrameData frame) throws Exception {
    long loc = frame.current_location;
    for (Locals.VariableDescription var : Locals.GetLocalVariableTable(frame.method)) {
      if (var.start_location <= loc &&
          var.length + var.start_location > loc &&
          var.name.equals(TARGET_VAR)) {
        return var.slot;
      }
    }
    throw new Error(
        "Unable to find variable " + TARGET_VAR + " in " + frame.method + " at loc " + loc);
  }

  public static int FindExpectedFrameDepth(StackTrace.StackFrameData frame) throws Exception {
    // Adjust the 'frame' depth since it is modified by:
    // +1 for Get/SetLocalVariableInt in future.
    // -1 for FindStackFrame
    // -1 for GetStackTrace
    // -1 for GetStackTraceNative
    // ------------------------------
    // -2
    return frame.depth - 2;
  }

  private static StackTrace.StackFrameData FindStackFrame(Method target) {
    for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(Thread.currentThread())) {
      if (frame.method.equals(target)) {
        return frame;
      }
    }
    throw new Error("Unable to find stack frame in method " + target);
  }
}

