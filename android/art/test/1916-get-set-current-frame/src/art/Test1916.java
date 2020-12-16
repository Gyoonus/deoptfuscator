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

public class Test1916 {
  public static final int SET_VALUE = 1337;
  public static final String TARGET_VAR = "TARGET";

  public static void reportValue(Object val) {
    System.out.println("\tValue is '" + val + "'");
  }

  public static class IntRunner implements Runnable {
    private volatile boolean continueBusyLoop;
    private volatile boolean inBusyLoop;
    public IntRunner() {
      this.continueBusyLoop = true;
      this.inBusyLoop = false;
    }
    public void run() {
      int TARGET = 42;
      // We will suspend the thread during this loop.
      while (continueBusyLoop) {
        inBusyLoop = true;
      }
      reportValue(TARGET);
    }
    public void waitForBusyLoopStart() { while (!inBusyLoop) {} }
    public void finish() { continueBusyLoop = false; }
  }

  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    runGet();
    runSet();
  }

  public static void runGet() throws Exception {
    Method target = IntRunner.class.getDeclaredMethod("run");
    // Get Int
    IntRunner int_runner = new IntRunner();
    Thread target_get = new Thread(int_runner, "GetLocalInt - Target");
    target_get.start();
    int_runner.waitForBusyLoopStart();
    try {
      Suspension.suspend(target_get);
    } catch (Exception e) {
      System.out.println("FAIL: got " + e);
      e.printStackTrace();
      int_runner.finish();
      target_get.join();
      return;
    }
    try {
      StackTrace.StackFrameData frame = FindStackFrame(target_get, target);
      int depth = frame.depth;
      if (depth != 0) { throw new Error("Expected depth 0 but got " + depth); }
      int slot = FindSlot(frame);
      int value = Locals.GetLocalVariableInt(target_get, depth, slot);
      System.out.println("From GetLocalInt(), value is " + value);
    } finally {
      Suspension.resume(target_get);
      int_runner.finish();
      target_get.join();
    }
  }

  public static void runSet() throws Exception {
    Method target = IntRunner.class.getDeclaredMethod("run");
    // Set Int
    IntRunner int_runner = new IntRunner();
    Thread target_set = new Thread(int_runner, "SetLocalInt - Target");
    target_set.start();
    int_runner.waitForBusyLoopStart();
    try {
      Suspension.suspend(target_set);
    } catch (Exception e) {
      System.out.println("FAIL: got " + e);
      e.printStackTrace();
      int_runner.finish();
      target_set.join();
      return;
    }
    try {
      StackTrace.StackFrameData frame = FindStackFrame(target_set, target);
      int depth = frame.depth;
      if (depth != 0) { throw new Error("Expected depth 0 but got " + depth); }
      int slot = FindSlot(frame);
      System.out.println("Setting TARGET to " + SET_VALUE);
      Locals.SetLocalVariableInt(target_set, depth, slot, SET_VALUE);
    } finally {
      Suspension.resume(target_set);
      int_runner.finish();
      target_set.join();
    }
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

  private static StackTrace.StackFrameData FindStackFrame(Thread thr, Method target) {
    for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(thr)) {
      if (frame.method.equals(target)) {
        return frame;
      }
    }
    throw new Error("Unable to find stack frame in method " + target + " on thread " + thr);
  }
}

