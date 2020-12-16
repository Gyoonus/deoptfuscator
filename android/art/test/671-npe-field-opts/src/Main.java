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

public class Main {
  static Main obj;
  // Make 'doCheck' volatile to prevent optimizations
  // in $noinline$bar like LICM that could hoist the null check
  // out of the loop.
  static volatile boolean doCheck = true;

  float floatField;
  int intField;

  public static void main(String[] args) {
    try {
      $noinline$bar();
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
      check(e, 29, 52, "$noinline$bar");
    }

    try {
      $noinline$foo();
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
      check(e, 36, 44, "$noinline$foo");
    }
  }

  public static float $noinline$foo() {
    int v1 = obj.intField;
    float v2 = obj.floatField;
    return v2;
  }

  public static float $noinline$bar() {
    float a = 0;
    while (doCheck) {
      float f = obj.floatField;
      int i = obj.intField;
      a = (float)i + f;
    }
    return a;
  }

  static void check(NullPointerException npe, int mainLine, int methodLine, String methodName) {
    StackTraceElement[] trace = npe.getStackTrace();
    checkElement(trace[0], "Main", methodName, "Main.java", methodLine);
    checkElement(trace[1], "Main", "main", "Main.java", mainLine);
  }

  static void checkElement(StackTraceElement element,
                           String declaringClass, String methodName,
                           String fileName, int lineNumber) {
    assertEquals(declaringClass, element.getClassName());
    assertEquals(methodName, element.getMethodName());
    assertEquals(fileName, element.getFileName());
    assertEquals(lineNumber, element.getLineNumber());
  }

  static void assertEquals(Object expected, Object actual) {
    if (!expected.equals(actual)) {
      String msg = "Expected \"" + expected + "\" but got \"" + actual + "\"";
      throw new AssertionError(msg);
    }
  }

  static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }
}
