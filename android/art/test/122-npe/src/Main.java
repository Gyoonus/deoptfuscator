/*
 * Copyright (C) 2014 The Android Open Source Project
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

/**
 * Test that null pointer exceptions are thrown by the VM.
 */
public class Main {
  private int f;
  public static void main(String[] args) {
    methodOne();
  }

  static void methodOne() {
    methodTwo();
  }

  private int callSpecial() {
    return f;
  }

  final int callFinal() {
    return f;
  }

  static void methodTwo() {
    NullPointerException npe = null;

    int thisLine = 41;

    new Object().getClass(); // Ensure compiled.
    try {
      ((Object) null).getClass();
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 4);

    new Main().callSpecial();  // Ensure compiled.
    try {
      ((Main) null).callSpecial();  // Test invokespecial.
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 8);

    new Main().callFinal();  // Ensure compiled.
    try {
      ((Main) null).callFinal();  // Test invokevirtual on final.
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 8);

    try {
      ((Value) null).objectField.toString();
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).intField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useFloat(((Value) null).floatField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useLong(((Value) null).longField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useDouble(((Value) null).doubleField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).objectField = "Fisk";
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).intField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).floatField = 42.0F;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).longField = 42L;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).doubleField = 42.0d;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).byteField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      if (((Value) null).booleanField) { }
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).charField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).shortField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).byteField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).booleanField = true;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).charField = '\u0042';
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).shortField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileObjectField.toString();
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileObjectField = "Fisk";
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).volatileIntField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileIntField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useFloat(((Value) null).volatileFloatField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileFloatField = 42.0F;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useLong(((Value) null).volatileLongField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileLongField = 42L;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useDouble(((Value) null).volatileDoubleField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileDoubleField = 42.0d;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).volatileByteField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileByteField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      if (((Value) null).volatileBooleanField) { }
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileBooleanField = true;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).volatileCharField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileCharField = '\u0042';
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Value) null).volatileShortField);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Value) null).volatileShortField = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Object[]) null)[0].toString();
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((int[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useFloat(((float[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useLong(((long[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useDouble(((double[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((Object[]) null)[0] = "Fisk";
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((int[]) null)[0] = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((float[]) null)[0] = 42.0F;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((long[]) null)[0] = 42L;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((double[]) null)[0] = 42.0d;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((byte[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      if (((boolean[]) null)[0]) { }
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((char[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((short[]) null)[0]);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((byte[]) null)[0] = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((boolean[]) null)[0] = true;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((char[]) null)[0] = '\u0042';
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      ((short[]) null)[0] = 42;
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((Object[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((int[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((float[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((long[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((double[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((byte[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((boolean[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((char[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      useInt(((short[]) null).length);
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 7);

    try {
      Interface i = null;
      i.methodInterface();  // Test null on invokeinterface.
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 8);

    try {
      Object o = null;
      o.toString();  // Test null on invokevirtual.
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 8);

    npe = null;
    try {
      String s = null;
      try {
        throw new AssertionError();
      } finally {
        // Cause an implicit NPE.
        s.getClass();
      }
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 13);

    npe = null;
    try {
      String s = null;
      try {
        throw new AssertionError();
      } catch (AssertionError ex) {
      }
      s.getClass();
    } catch (NullPointerException e) {
      npe = e;
    }
    check(npe, thisLine += 14);
  }

  static void check(NullPointerException npe, int firstLine) {
    final boolean debug = false;
    if (debug) {
      System.out.print("Got to line ");
      System.out.print(firstLine);
      System.out.println();
    }
    StackTraceElement[] trace = npe.getStackTrace();
    checkElement(trace[0], "Main", "methodTwo", "Main.java", firstLine);
    checkElement(trace[1], "Main", "methodOne", "Main.java", 27);
    checkElement(trace[2], "Main", "main", "Main.java", 23);
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

  interface Interface {
    void methodInterface();
  }

  static void useInt(int i) {
  }

  static void useFloat(float f) {
  }

  static void useDouble(double d) {
  }

  static void useLong(long l) {
  }

  static class Value {
    Object objectField;
    int intField;
    float floatField;
    long longField;
    double doubleField;
    byte byteField;
    boolean booleanField;
    char charField;
    short shortField;

    volatile Object volatileObjectField;
    volatile int volatileIntField;
    volatile float volatileFloatField;
    volatile long volatileLongField;
    volatile double volatileDoubleField;
    volatile byte volatileByteField;
    volatile boolean volatileBooleanField;
    volatile char volatileCharField;
    volatile short volatileShortField;
  }
}
