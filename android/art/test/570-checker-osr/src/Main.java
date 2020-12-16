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

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    new SubMain();
    if ($noinline$returnInt() != 53) {
      throw new Error("Unexpected return value");
    }
    if ($noinline$returnFloat() != 42.2f) {
      throw new Error("Unexpected return value");
    }
    if ($noinline$returnDouble() != Double.longBitsToDouble(0xF000000000001111L)) {
      throw new Error("Unexpected return value ");
    }
    if ($noinline$returnLong() != 0xFFFF000000001111L) {
      throw new Error("Unexpected return value");
    }

    try {
      $noinline$deopt();
    } catch (Exception e) {}
    DeoptimizationController.stopDeoptimization();

    $noinline$inlineCache(new Main(), /* isSecondInvocation */ false);
    if ($noinline$inlineCache(new SubMain(), /* isSecondInvocation */ true) != SubMain.class) {
      throw new Error("Unexpected return value");
    }

    $noinline$inlineCache2(new Main(), /* isSecondInvocation */ false);
    if ($noinline$inlineCache2(new SubMain(), /* isSecondInvocation */ true) != SubMain.class) {
      throw new Error("Unexpected return value");
    }

    // Test polymorphic inline cache to the same target (inlineCache3).
    $noinline$inlineCache3(new Main(), /* isSecondInvocation */ false);
    $noinline$inlineCache3(new SubMain(), /* isSecondInvocation */ false);
    if ($noinline$inlineCache3(new SubMain(), /* isSecondInvocation */ true) != null) {
      throw new Error("Unexpected return value");
    }

    $noinline$stackOverflow(new Main(), /* isSecondInvocation */ false);
    $noinline$stackOverflow(new SubMain(), /* isSecondInvocation */ true);

    $opt$noinline$testOsrInlineLoop(null);
    System.out.println("b28210356 passed.");
  }

  public static int $noinline$returnInt() {
    if (doThrow) throw new Error("");
    int i = 0;
    for (; i < 100000; ++i) {
    }
    while (!isInOsrCode("$noinline$returnInt")) {}
    System.out.println(i);
    return 53;
  }

  public static float $noinline$returnFloat() {
    if (doThrow) throw new Error("");
    int i = 0;
    for (; i < 200000; ++i) {
    }
    while (!isInOsrCode("$noinline$returnFloat")) {}
    System.out.println(i);
    return 42.2f;
  }

  public static double $noinline$returnDouble() {
    if (doThrow) throw new Error("");
    int i = 0;
    for (; i < 300000; ++i) {
    }
    while (!isInOsrCode("$noinline$returnDouble")) {}
    System.out.println(i);
    return Double.longBitsToDouble(0xF000000000001111L);
  }

  public static long $noinline$returnLong() {
    if (doThrow) throw new Error("");
    int i = 0;
    for (; i < 400000; ++i) {
    }
    while (!isInOsrCode("$noinline$returnLong")) {}
    System.out.println(i);
    return 0xFFFF000000001111L;
  }

  public static void $noinline$deopt() {
    if (doThrow) throw new Error("");
    int i = 0;
    for (; i < 100000; ++i) {
    }
    while (!isInOsrCode("$noinline$deopt")) {}
    DeoptimizationController.startDeoptimization();
  }

  public static Class<?> $noinline$inlineCache(Main m, boolean isSecondInvocation) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, just return the expected value.
    if (!isInInterpreter("$noinline$inlineCache")) {
      return SubMain.class;
    }

    ensureHasProfilingInfo("$noinline$inlineCache");

    // Ensure that we have OSR code to jump to.
    if (isSecondInvocation) {
      ensureHasOsrCode("$noinline$inlineCache");
    }

    // This call will be optimized in the OSR compiled code
    // to check and deoptimize if m is not of type 'Main'.
    Main other = m.inlineCache();

    // Jump to OSR compiled code. The second run
    // of this method will have 'm' as a SubMain, and the compiled
    // code we are jumping to will have wrongly optimize other as being a
    // 'Main'.
    if (isSecondInvocation) {
      while (!isInOsrCode("$noinline$inlineCache")) {}
    }

    // We used to wrongly optimize this call and assume 'other' was a 'Main'.
    return other.returnClass();
  }

  public static Class<?> $noinline$inlineCache2(Main m, boolean isSecondInvocation) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, just return the expected value.
    if (!isInInterpreter("$noinline$inlineCache2")) {
      return SubMain.class;
    }

    ensureHasProfilingInfo("$noinline$inlineCache2");

    // Ensure that we have OSR code to jump to.
    if (isSecondInvocation) {
      ensureHasOsrCode("$noinline$inlineCache2");
    }

    // This call will be optimized in the OSR compiled code
    // to check and deoptimize if m is not of type 'Main'.
    Main other = m.inlineCache2();

    // Jump to OSR compiled code. The second run
    // of this method will have 'm' as a SubMain, and the compiled
    // code we are jumping to will have wrongly optimize other as being null.
    if (isSecondInvocation) {
      while (!isInOsrCode("$noinline$inlineCache2")) {}
    }

    // We used to wrongly optimize this code and assume 'other' was always null.
    return (other == null) ? null : other.returnClass();
  }

  public static Class<?> $noinline$inlineCache3(Main m, boolean isSecondInvocation) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, just return the expected value.
    if (!isInInterpreter("$noinline$inlineCache3")) {
      return null;
    }

    ensureHasProfilingInfo("$noinline$inlineCache3");

    // Ensure that we have OSR code to jump to.
    if (isSecondInvocation) {
      ensureHasOsrCode("$noinline$inlineCache3");
    }

    // This call will be optimized in the OSR compiled code
    // to check and deoptimize if m is not of type 'Main'.
    Main other = m.inlineCache3();

    // Jump to OSR compiled code. The second run
    // of this method will have 'm' as a SubMain, and the compiled
    // code we are jumping to will have wrongly optimize other as being null.
    if (isSecondInvocation) {
      while (!isInOsrCode("$noinline$inlineCache3")) {}
    }

    // We used to wrongly optimize this code and assume 'other' was always null.
    return (other == null) ? null : other.returnClass();
  }

  public Main inlineCache() {
    return new Main();
  }

  public Main inlineCache2() {
    return null;
  }

  public Main inlineCache3() {
    return null;
  }

  public Class<?> returnClass() {
    return Main.class;
  }

  public void otherInlineCache() {
    return;
  }

  public static void $noinline$stackOverflow(Main m, boolean isSecondInvocation) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, just return the expected value.
    if (!isInInterpreter("$noinline$stackOverflow")) {
      return;
    }

    // We need a ProfilingInfo object to populate the 'otherInlineCache' call.
    ensureHasProfilingInfo("$noinline$stackOverflow");

    if (isSecondInvocation) {
      // Ensure we have an OSR code and we jump to it.
      while (!isInOsrCode("$noinline$stackOverflow")) {}
    }

    for (int i = 0; i < (isSecondInvocation ? 10000000 : 1); ++i) {
      // The first invocation of $noinline$stackOverflow will populate the inline
      // cache with Main. The second invocation of the method, will see a SubMain
      // and will therefore trigger deoptimization.
      m.otherInlineCache();
    }
  }

  public static void $opt$noinline$testOsrInlineLoop(String[] args) {
    // Regression test for inlining a method with a loop to a method without a loop in OSR mode.
    if (doThrow) throw new Error();
    assertIntEquals(12, $opt$inline$testRemoveSuspendCheck(12, 5));
    // Since we cannot have a loop directly in this method, we need to force the OSR
    // compilation from native code.
    ensureHasProfilingInfo("$opt$noinline$testOsrInlineLoop");
    ensureHasOsrCode("$opt$noinline$testOsrInlineLoop");
  }

  public static int $opt$inline$testRemoveSuspendCheck(int x, int y) {
    // For this test we need an inlined loop and have DCE re-run loop analysis
    // after inlining.
    while (y > 0) {
      while ($opt$inline$inlineFalse() || !$opt$inline$inlineTrue()) {
        x++;
      }
      y--;
    }
    return x;
  }

  public static boolean $opt$inline$inlineTrue() {
    return true;
  }

  public static boolean $opt$inline$inlineFalse() {
    return false;
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static native boolean isInOsrCode(String methodName);
  public static native boolean isInInterpreter(String methodName);
  public static native void ensureHasProfilingInfo(String methodName);
  public static native void ensureHasOsrCode(String methodName);

  public static boolean doThrow = false;
}

class SubMain extends Main {
  public Class<?> returnClass() {
    return SubMain.class;
  }

  public Main inlineCache() {
    return new SubMain();
  }

  public Main inlineCache2() {
    return new SubMain();
  }

  public void otherInlineCache() {
    return;
  }
}
