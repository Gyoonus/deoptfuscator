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

import java.util.Arrays;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.Base64;
import art.Breakpoint;
import art.Redefinition;

public class Main {
  static class Transform {
    public void sayHi() {
      System.out.println("Hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQA7jFommHUzfbuvjq/I2cDcwdjqQk6KPfqYAwAAcAAAAHhWNBIAAAAAAAAAANQCAAAU" +
    "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAABUAgAARAEAAJ4B" +
    "AACmAQAArwEAAMEBAADJAQAA7QEAAA0CAAAkAgAAOAIAAEwCAABgAgAAawIAAHYCAAB5AgAAfQIA" +
    "AIoCAACQAgAAlQIAAJ4CAAClAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
    "DAAAAAgAAAAAAAAADQAAAAgAAACYAQAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
    "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAAiAEAAMYCAAAAAAAAAgAAALcCAAC9AgAAAQABAAEA" +
    "AACsAgAABAAAAHAQAwAAAA4AAwABAAIAAACxAgAACAAAAGIAAAAaAQEAbiACABAADgBEAQAAAAAA" +
    "AAAAAAAAAAAAAQAAAAYABjxpbml0PgAHR29vZGJ5ZQAQTE1haW4kVHJhbnNmb3JtOwAGTE1haW47" +
    "ACJMZGFsdmlrL2Fubm90YXRpb24vRW5jbG9zaW5nQ2xhc3M7AB5MZGFsdmlrL2Fubm90YXRpb24v" +
    "SW5uZXJDbGFzczsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABJM" +
    "amF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVtOwAJTWFpbi5qYXZhAAlUcmFuc2Zv" +
    "cm0AAVYAAlZMAAthY2Nlc3NGbGFncwAEbmFtZQADb3V0AAdwcmludGxuAAVzYXlIaQAFdmFsdWUA" +
    "EgAHDgAUAAcOeAACAgETGAECAwIOBAgPFwsAAAEBAICABNACAQHoAhAAAAAAAAAAAQAAAAAAAAAB" +
    "AAAAFAAAAHAAAAACAAAACQAAAMAAAAADAAAAAgAAAOQAAAAEAAAAAQAAAPwAAAAFAAAABAAAAAQB" +
    "AAAGAAAAAQAAACQBAAADEAAAAQAAAEQBAAABIAAAAgAAAFABAAAGIAAAAQAAAIgBAAABEAAAAQAA" +
    "AJgBAAACIAAAFAAAAJ4BAAADIAAAAgAAAKwCAAAEIAAAAgAAALcCAAAAIAAAAQAAAMYCAAAAEAAA" +
    "AQAAANQCAAA=");

  public static void notifyBreakpointReached(Thread thr, Executable e, long loc) {
    System.out.println(
        "\tBreakpoint reached: " + e + " @ line=" + Breakpoint.locationToLine(e, loc));
  }

  public static void check(boolean b, String msg) {
    if (!b) {
      throw new Error("FAILED: " + msg);
    }
  }
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    // Set up breakpoints
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
    Breakpoint.startBreakpointWatch(
        Main.class,
        Main.class.getDeclaredMethod(
            "notifyBreakpointReached", Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());

    Method targetMethod = Transform.class.getDeclaredMethod("sayHi");
    Transform t = new Transform();
    check(isInterpretOnly() || !isMethodDeoptimized(targetMethod),
        "method should not be deoptimized");
    t.sayHi();

    // Set a breakpoint at the start of the function.
    Breakpoint.setBreakpoint(targetMethod, 0);
    check(isInterpretOnly() || isMethodDeoptimized(targetMethod),
        "method should be deoptimized");
    t.sayHi();

    System.out.println("Redefining transform!");
    Redefinition.doCommonClassRedefinition(Transform.class, new byte[0], DEX_BYTES);
    check(isInterpretOnly() || !isMethodDeoptimized(targetMethod),
        "method should not be deoptimized");
    t.sayHi();

    Breakpoint.setBreakpoint(targetMethod, 0);
    check(isInterpretOnly() || isMethodDeoptimized(targetMethod),
        "method should be deoptimized");
    t.sayHi();
  }

  static native boolean isMethodDeoptimized(Method m);
  static native boolean isInterpretOnly();
}
