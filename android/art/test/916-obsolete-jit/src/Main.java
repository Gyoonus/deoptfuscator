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


import art.Redefinition;

import java.util.function.Consumer;
import java.lang.reflect.Method;
import java.util.Base64;

public class Main {

  // import java.util.function.Consumer;
  //
  // class Transform {
  //   private void Start(Consumer<String> reporter) {
  //     reporter.accept("Hello - private - Transformed");
  //   }
  //
  //   private void Finish(Consumer<String> reporter) {
  //     reporter.accept("Goodbye - private - Transformed");
  //   }
  //
  //   public void sayHi(Runnable r, Consumer<String> reporter) {
  //     reporter.accept("pre Start private method call - Transformed");
  //     Start(reporter);
  //     reporter.accept("post Start private method call - Transformed");
  //     r.run();
  //     reporter.accept("pre Finish private method call - Transformed");
  //     Finish(reporter);
  //     reporter.accept("post Finish private method call - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAMAoADQAcCAAdCwAeAB8IACAIACEKAAwAIggAIwsAJAAlCAAmCgAMACcIACgHACkH" +
    "ACoBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAFU3RhcnQBACAoTGph" +
    "dmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjspVgEACVNpZ25hdHVyZQEANChMamF2YS91dGlsL2Z1" +
    "bmN0aW9uL0NvbnN1bWVyPExqYXZhL2xhbmcvU3RyaW5nOz47KVYBAAZGaW5pc2gBAAVzYXlIaQEA" +
    "NChMamF2YS9sYW5nL1J1bm5hYmxlO0xqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7KVYBAEgo" +
    "TGphdmEvbGFuZy9SdW5uYWJsZTtMamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyPExqYXZhL2xh" +
    "bmcvU3RyaW5nOz47KVYBAApTb3VyY2VGaWxlAQAOVHJhbnNmb3JtLmphdmEMAA4ADwEAHUhlbGxv" +
    "IC0gcHJpdmF0ZSAtIFRyYW5zZm9ybWVkBwArDAAsAC0BAB9Hb29kYnllIC0gcHJpdmF0ZSAtIFRy" +
    "YW5zZm9ybWVkAQArcHJlIFN0YXJ0IHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAwA" +
    "EgATAQAscG9zdCBTdGFydCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQHAC4MAC8A" +
    "DwEALHByZSBGaW5pc2ggcHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkDAAWABMBAC1w" +
    "b3N0IEZpbmlzaCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQBAAlUcmFuc2Zvcm0B" +
    "ABBqYXZhL2xhbmcvT2JqZWN0AQAbamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyAQAGYWNjZXB0" +
    "AQAVKExqYXZhL2xhbmcvT2JqZWN0OylWAQASamF2YS9sYW5nL1J1bm5hYmxlAQADcnVuACAADAAN" +
    "AAAAAAAEAAAADgAPAAEAEAAAAB0AAQABAAAABSq3AAGxAAAAAQARAAAABgABAAAAEwACABIAEwAC" +
    "ABAAAAAlAAIAAgAAAAkrEgK5AAMCALEAAAABABEAAAAKAAIAAAAVAAgAFgAUAAAAAgAVAAIAFgAT" +
    "AAIAEAAAACUAAgACAAAACSsSBLkAAwIAsQAAAAEAEQAAAAoAAgAAABkACAAaABQAAAACABUAAQAX" +
    "ABgAAgAQAAAAZQACAAMAAAAxLBIFuQADAgAqLLcABiwSB7kAAwIAK7kACAEALBIJuQADAgAqLLcA" +
    "CiwSC7kAAwIAsQAAAAEAEQAAACIACAAAAB0ACAAeAA0AHwAVACAAGwAhACMAIgAoACMAMAAkABQA" +
    "AAACABkAAQAaAAAAAgAb");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBc8wr9PcHqnOR61m+0kimXTSddVMToJPuYBQAAcAAAAHhWNBIAAAAAAAAAAOAEAAAc" +
    "AAAAcAAAAAYAAADgAAAABAAAAPgAAAAAAAAAAAAAAAcAAAAoAQAAAQAAAGABAAAYBAAAgAEAAHoC" +
    "AAB9AgAAgAIAAIgCAACOAgAAlgIAALcCAADWAgAA4wIAAAIDAAAWAwAALAMAAEADAABeAwAAfQMA" +
    "AIQDAACUAwAAlwMAAJsDAACgAwAAqAMAALwDAADrAwAAGQQAAEcEAAB0BAAAeQQAAIAEAAAHAAAA" +
    "CAAAAAkAAAAKAAAADQAAABAAAAAQAAAABQAAAAAAAAARAAAABQAAAGQCAAASAAAABQAAAGwCAAAR" +
    "AAAABQAAAHQCAAAAAAAAAgAAAAAAAwAEAAAAAAADAA4AAAAAAAIAGgAAAAIAAAACAAAAAwAAABkA" +
    "AAAEAAEAEwAAAAAAAAAAAAAAAgAAAAAAAAAPAAAAPAIAAMoEAAAAAAAAAQAAAKgEAAABAAAAuAQA" +
    "AAEAAQABAAAAhwQAAAQAAABwEAQAAAAOAAMAAgACAAAAjAQAAAcAAAAbAAUAAAByIAYAAgAOAAAA" +
    "AwACAAIAAACTBAAABwAAABsABgAAAHIgBgACAA4AAAAEAAMAAgAAAJoEAAAiAAAAGwAYAAAAciAG" +
    "AAMAcCACADEAGwAWAAAAciAGAAMAchAFAAIAGwAXAAAAciAGAAMAcCABADEAGwAVAAAAciAGAAMA" +
    "DgAAAAAAAAAAAAMAAAAAAAAAAQAAAIABAAACAAAAgAEAAAMAAACIAQAAAQAAAAIAAAACAAAAAwAE" +
    "AAEAAAAEAAEoAAE8AAY8aW5pdD4ABD47KVYABkZpbmlzaAAfR29vZGJ5ZSAtIHByaXZhdGUgLSBU" +
    "cmFuc2Zvcm1lZAAdSGVsbG8gLSBwcml2YXRlIC0gVHJhbnNmb3JtZWQAC0xUcmFuc2Zvcm07AB1M" +
    "ZGFsdmlrL2Fubm90YXRpb24vU2lnbmF0dXJlOwASTGphdmEvbGFuZy9PYmplY3Q7ABRMamF2YS9s" +
    "YW5nL1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABxMamF2YS91dGlsL2Z1bmN0aW9uL0Nv" +
    "bnN1bWVyAB1MamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyOwAFU3RhcnQADlRyYW5zZm9ybS5q" +
    "YXZhAAFWAAJWTAADVkxMAAZhY2NlcHQAEmVtaXR0ZXI6IGphY2stNC4xOQAtcG9zdCBGaW5pc2gg" +
    "cHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkACxwb3N0IFN0YXJ0IHByaXZhdGUgbWV0" +
    "aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAAscHJlIEZpbmlzaCBwcml2YXRlIG1ldGhvZCBjYWxsIC0g" +
    "VHJhbnNmb3JtZWQAK3ByZSBTdGFydCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQA" +
    "A3J1bgAFc2F5SGkABXZhbHVlABMABw4AGQEABw5pABUBAAcOaQAdAgAABw5pPGk8aTxpAAIBARsc" +
    "BRcAFwwXARcLFwMCAQEbHAYXABcKFwwXARcLFwMAAAMBAICABJADAQKoAwECyAMDAegDDwAAAAAA" +
    "AAABAAAAAAAAAAEAAAAcAAAAcAAAAAIAAAAGAAAA4AAAAAMAAAAEAAAA+AAAAAUAAAAHAAAAKAEA" +
    "AAYAAAABAAAAYAEAAAMQAAACAAAAgAEAAAEgAAAEAAAAkAEAAAYgAAABAAAAPAIAAAEQAAADAAAA" +
    "ZAIAAAIgAAAcAAAAegIAAAMgAAAEAAAAhwQAAAQgAAACAAAAqAQAAAAgAAABAAAAygQAAAAQAAAB" +
    "AAAA4AQAAA==");

  // A class that we can use to keep track of the output of this test.
  private static class TestWatcher implements Consumer<String> {
    private StringBuilder sb;
    public TestWatcher() {
      sb = new StringBuilder();
    }

    @Override
    public void accept(String s) {
      sb.append(s);
      sb.append('\n');
    }

    public String getOutput() {
      return sb.toString();
    }

    public void clear() {
      sb = new StringBuilder();
    }
  }

  public static void main(String[] args) {
    doTest(new Transform(), new TestWatcher());
  }

  private static boolean interpreting = true;
  private static boolean retry = false;

  public static void doTest(Transform t, TestWatcher w) {
    // Get the methods that need to be optimized.
    Method say_hi_method;
    // Figure out if we can even JIT at all.
    final boolean has_jit = hasJit();
    try {
      say_hi_method = Transform.class.getDeclaredMethod(
          "sayHi", Runnable.class, Consumer.class);
    } catch (Exception e) {
      System.out.println("Unable to find methods!");
      e.printStackTrace(System.out);
      return;
    }
    // Makes sure the stack is the way we want it for the test and does the redefinition. It will
    // set the retry boolean to true if the stack does not have a JIT-compiled sayHi entry. This can
    // only happen if the method gets GC'd.
    Runnable do_redefinition = () -> {
      if (has_jit && Main.isInterpretedFunction(say_hi_method, true)) {
        // Try again. We are not running the right jitted methods/cannot redefine them now.
        retry = true;
      } else {
        // Actually do the redefinition. The stack looks good.
        retry = false;
        w.accept("transforming calling function");
        Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      }
    };
    // This just prints something out to show we are running the Runnable.
    Runnable say_nothing = () -> { w.accept("Not doing anything here"); };
    do {
      // Run ensureJitCompiled here since it might get GCd
      ensureJitCompiled(Transform.class, "sayHi");
      // Clear output.
      w.clear();
      // Try and redefine.
      t.sayHi(say_nothing, w);
      t.sayHi(do_redefinition, w);
      t.sayHi(say_nothing, w);
    } while (retry);
    // Print output of last run.
    System.out.print(w.getOutput());
  }

  private static native boolean hasJit();

  private static native boolean isInterpretedFunction(Method m, boolean require_deoptimizable);

  private static native void ensureJitCompiled(Class c, String name);
}
