/*
 * Copyright (C) 2016 The Android Open Source Project
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

interface Itf {
  public Class<?> sameInvokeInterface();
  public Class<?> sameInvokeInterface2();
  public Class<?> sameInvokeInterface3();
}

public class Main implements Itf {
  public static void assertEquals(Object expected, Object actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected  + ", got " + actual);
    }
  }

  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected  + ", got " + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Main[] mains = new Main[3];
    Itf[] itfs = new Itf[3];
    itfs[0] = mains[0] = new Main();
    itfs[1] = mains[1] = new Subclass();
    itfs[2] = mains[2] = new OtherSubclass();

    // Create the profiling info eagerly to make sure they are filled.
    ensureProfilingInfo566();

    // Make testInvokeVirtual and testInvokeInterface hot to get them jitted.
    // We pass Main and Subclass to get polymorphic inlining based on calling
    // the same method.
    for (int i = 0; i < 10000; ++i) {
      testInvokeVirtual(mains[0]);
      testInvokeVirtual(mains[1]);
      testInvokeInterface(itfs[0]);
      testInvokeInterface(itfs[1]);
      testInvokeInterface2(itfs[0]);
      testInvokeInterface2(itfs[1]);
      $noinline$testInlineToSameTarget(mains[0]);
      $noinline$testInlineToSameTarget(mains[1]);
    }

    ensureJittedAndPolymorphicInline566();

    // At this point, the JIT should have compiled both methods, and inline
    // sameInvokeVirtual and sameInvokeInterface.
    assertEquals(Main.class, testInvokeVirtual(mains[0]));
    assertEquals(Main.class, testInvokeVirtual(mains[1]));

    assertEquals(Itf.class, testInvokeInterface(itfs[0]));
    assertEquals(Itf.class, testInvokeInterface(itfs[1]));

    assertEquals(Itf.class, testInvokeInterface2(itfs[0]));
    assertEquals(Itf.class, testInvokeInterface2(itfs[1]));

    // This will trigger a deoptimization of the compiled code.
    assertEquals(OtherSubclass.class, testInvokeVirtual(mains[2]));
    assertEquals(OtherSubclass.class, testInvokeInterface(itfs[2]));
    assertEquals(null, testInvokeInterface2(itfs[2]));

    // Run this once to make sure we execute the JITted code.
    $noinline$testInlineToSameTarget(mains[0]);
    assertEquals(20001, counter);
  }

  public Class<?> sameInvokeVirtual() {
    field.getClass(); // null check to ensure we get an inlined frame in the CodeInfo.
    return Main.class;
  }

  public Class<?> sameInvokeInterface() {
    field.getClass(); // null check to ensure we get an inlined frame in the CodeInfo.
    return Itf.class;
  }

  public Class<?> sameInvokeInterface2() {
    field.getClass(); // null check to ensure we get an inlined frame in the CodeInfo.
    return Itf.class;
  }

  public Class<?> sameInvokeInterface3() {
    field.getClass(); // null check to ensure we get an inlined frame in the CodeInfo.
    return Itf.class;
  }

  public static Class<?> testInvokeInterface(Itf i) {
    return i.sameInvokeInterface();
  }

  public static Class<?> testInvokeInterface2(Itf i) {
    // Make three interface calls that will do a ClassTableGet to ensure bogus code
    // generation of ClassTableGet will crash.
    i.sameInvokeInterface();
    i.sameInvokeInterface2();
    return i.sameInvokeInterface3();
  }

  public static Class<?> testInvokeVirtual(Main m) {
    return m.sameInvokeVirtual();
  }

  public static void $noinline$testInlineToSameTarget(Main m) {
    if (doThrow) throw new Error("");
    m.increment();
  }

  public Object field = new Object();

  public static native void ensureJittedAndPolymorphicInline566();
  public static native void ensureProfilingInfo566();

  public void increment() {
    field.getClass(); // null check to ensure we get an inlined frame in the CodeInfo
    counter++;
  }
  public static int counter = 0;
  public static boolean doThrow = false;
}

class Subclass extends Main {
}

class OtherSubclass extends Main {
  public Class<?> sameInvokeVirtual() {
    return OtherSubclass.class;
  }

  public Class<?> sameInvokeInterface() {
    return OtherSubclass.class;
  }

  public Class<?> sameInvokeInterface2() {
    return null;
  }
  public Class<?> sameInvokeInterface3() {
    return null;
  }
}
