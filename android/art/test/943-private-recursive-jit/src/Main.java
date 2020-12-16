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

import static art.Redefinition.doCommonClassRedefinition;

import java.util.Base64;
import java.util.function.Consumer;
import java.lang.reflect.Method;

public class Main {
  static final boolean ALWAYS_PRINT = false;

  // import java.util.function.Consumer;
  // class Transform {
  //   public void sayHi(int recur, Consumer<String> reporter, Runnable r) {
  //     privateSayHi(recur, reporter, r);
  //   }
  //   private void privateSayHi(int recur, Consumer<String> reporter, Runnable r) {
  //     reporter.accpet("hello" + recur + " - transformed");
  //     if (recur == 1) {
  //       r.run();
  //       privateSayHi(recur - 1, reporter, r);
  //     } else if (recur != 0) {
  //       privateSayHi(recur - 1, reporter, r);
  //     }
  //     reporter.accept("goodbye" + recur + " - transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQANAoADgAbCgANABwHAB0KAAMAGwgAHgoAAwAfCgADACAIACEKAAMAIgsAIwAkCwAl" +
    "ACYIACcHACgHACkBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5" +
    "SGkBADUoSUxqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7TGphdmEvbGFuZy9SdW5uYWJsZTsp" +
    "VgEACVNpZ25hdHVyZQEASShJTGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjxMamF2YS9sYW5n" +
    "L1N0cmluZzs+O0xqYXZhL2xhbmcvUnVubmFibGU7KVYBAAxwcml2YXRlU2F5SGkBAA1TdGFja01h" +
    "cFRhYmxlAQAKU291cmNlRmlsZQEADlRyYW5zZm9ybS5qYXZhDAAPABAMABcAFAEAF2phdmEvbGFu" +
    "Zy9TdHJpbmdCdWlsZGVyAQAFaGVsbG8MACoAKwwAKgAsAQAOIC0gdHJhbnNmb3JtZWQMAC0ALgcA" +
    "LwwAMAAxBwAyDAAzABABAAdnb29kYnllAQAJVHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAEA" +
    "BmFwcGVuZAEALShMamF2YS9sYW5nL1N0cmluZzspTGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwEA" +
    "HChJKUxqYXZhL2xhbmcvU3RyaW5nQnVpbGRlcjsBAAh0b1N0cmluZwEAFCgpTGphdmEvbGFuZy9T" +
    "dHJpbmc7AQAbamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyAQAGYWNjZXB0AQAVKExqYXZhL2xh" +
    "bmcvT2JqZWN0OylWAQASamF2YS9sYW5nL1J1bm5hYmxlAQADcnVuACAADQAOAAAAAAADAAAADwAQ" +
    "AAEAEQAAAB0AAQABAAAABSq3AAGxAAAAAQASAAAABgABAAAAAgABABMAFAACABEAAAAkAAQABAAA" +
    "AAgqGywttwACsQAAAAEAEgAAAAoAAgAAAAQABwAFABUAAAACABYAAgAXABQAAgARAAAAnwAEAAQA" +
    "AABhLLsAA1m3AAQSBbYABhu2AAcSCLYABrYACbkACgIAGwSgABUtuQALAQAqGwRkLC23AAKnABAb" +
    "mQAMKhsEZCwttwACLLsAA1m3AAQSDLYABhu2AAcSCLYABrYACbkACgIAsQAAAAIAEgAAACIACAAA" +
    "AAcAHgAIACMACQApAAoANQALADkADABCAA4AYAAPABgAAAAEAAI1DAAVAAAAAgAWAAEAGQAAAAIA" +
    "Gg==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCevtlr8B0kh/duuDYqXkGz/w9lMmtCCuRoBQAAcAAAAHhWNBIAAAAAAAAAALAEAAAg" +
    "AAAAcAAAAAkAAADwAAAABgAAABQBAAAAAAAAAAAAAAoAAABcAQAAAQAAAKwBAACcAwAAzAEAAPYC" +
    "AAAGAwAACgMAAA4DAAARAwAAGQMAAB0DAAAgAwAAIwMAACcDAAArAwAAOAMAAFcDAABrAwAAgQMA" +
    "AJUDAACwAwAAzgMAAO0DAAD9AwAAAAQAAAYEAAAKBAAAEgQAABoEAAAuBAAANwQAAD4EAABMBAAA" +
    "UQQAAFgEAABiBAAABgAAAAoAAAALAAAADAAAAA0AAAAOAAAADwAAABEAAAATAAAABwAAAAUAAAAA" +
    "AAAACAAAAAYAAADUAgAACQAAAAYAAADcAgAAEwAAAAgAAAAAAAAAFAAAAAgAAADkAgAAFQAAAAgA" +
    "AADwAgAAAQADAAQAAAABAAQAGwAAAAEABAAdAAAAAwADAAQAAAAEAAMAHAAAAAYAAwAEAAAABgAB" +
    "ABcAAAAGAAIAFwAAAAYAAAAeAAAABwAFABYAAAABAAAAAAAAAAMAAAAAAAAAEgAAALQCAACeBAAA" +
    "AAAAAAEAAACKBAAAAQABAAEAAABpBAAABAAAAHAQAwAAAA4ABgAEAAQAAABuBAAAUAAAACIABgBw" +
    "EAUAAAAbARoAAABuIAcAEAAMAG4gBgAwAAwAGwEAAAAAbiAHABAADABuEAgAAAAMAHIgCQAEABIQ" +
    "MwMpAHIQBAAFANgAA/9wQAEAAlQiAAYAcBAFAAAAGwEZAAAAbiAHABAADABuIAYAMAAMABsBAAAA" +
    "AG4gBwAQAAwAbhAIAAAADAByIAkABAAOADgD4f/YAAP/cEABAAJUKNoEAAQABAAAAIEEAAAEAAAA" +
    "cEABABAyDgAAAAAAAAAAAAIAAAAAAAAAAQAAAMwBAAACAAAAzAEAAAEAAAAAAAAAAQAAAAUAAAAD" +
    "AAAAAAAHAAQAAAABAAAAAwAOIC0gdHJhbnNmb3JtZWQAAihJAAIpVgABPAAGPGluaXQ+AAI+OwAB" +
    "SQABTAACTEkAAkxMAAtMVHJhbnNmb3JtOwAdTGRhbHZpay9hbm5vdGF0aW9uL1NpZ25hdHVyZTsA" +
    "EkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEvbGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3Ry" +
    "aW5nOwAZTGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwAcTGphdmEvdXRpbC9mdW5jdGlvbi9Db25z" +
    "dW1lcgAdTGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjsADlRyYW5zZm9ybS5qYXZhAAFWAARW" +
    "SUxMAAJWTAAGYWNjZXB0AAZhcHBlbmQAEmVtaXR0ZXI6IGphY2stNC4yNAAHZ29vZGJ5ZQAFaGVs" +
    "bG8ADHByaXZhdGVTYXlIaQADcnVuAAVzYXlIaQAIdG9TdHJpbmcABXZhbHVlAAIABw4ABwMAAAAH" +
    "DgEeDzw8XQEeDxktAAQDAAAABw48AAICAR8cBxcBFxAXAxcOFwUXDRcCAAACAQCAgATUAwEC7AMC" +
    "AZwFDwAAAAAAAAABAAAAAAAAAAEAAAAgAAAAcAAAAAIAAAAJAAAA8AAAAAMAAAAGAAAAFAEAAAUA" +
    "AAAKAAAAXAEAAAYAAAABAAAArAEAAAMQAAABAAAAzAEAAAEgAAADAAAA1AEAAAYgAAABAAAAtAIA" +
    "AAEQAAAEAAAA1AIAAAIgAAAgAAAA9gIAAAMgAAADAAAAaQQAAAQgAAABAAAAigQAAAAgAAABAAAA" +
    "ngQAAAAQAAABAAAAsAQAAA==");

  // A class that we can use to keep track of the output of this test.
  private static class TestWatcher implements Consumer<String> {
    private StringBuilder sb;
    public TestWatcher() {
      sb = new StringBuilder();
    }

    @Override
    public void accept(String s) {
      if (Main.ALWAYS_PRINT) {
        System.out.println(s);
      }
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
    doTest(new Transform());
  }

  private static boolean retry = false;

  public static void doTest(Transform t) {
    final TestWatcher reporter = new TestWatcher();
    Method say_hi_method;
    Method private_say_hi_method;
    // Figure out if we can even JIT at all.
    final boolean has_jit = hasJit();
    try {
      say_hi_method = Transform.class.getDeclaredMethod(
          "sayHi", int.class, Consumer.class, Runnable.class);
      private_say_hi_method = Transform.class.getDeclaredMethod(
          "privateSayHi", int.class, Consumer.class, Runnable.class);
    } catch (Exception e) {
      System.out.println("Unable to find methods!");
      e.printStackTrace(System.out);
      return;
    }
    // Makes sure the stack is the way we want it for the test and does the redefinition. It will
    // set the retry boolean to true if we need to go around again due to jit code being GCd.
    Runnable do_redefinition = () -> {
      if (has_jit &&
          (Main.isInterpretedFunction(say_hi_method, true) ||
           Main.isInterpretedFunction(private_say_hi_method, true))) {
        // Try again. We are not running the right jitted methods/cannot redefine them now.
        retry = true;
      } else {
        // Actually do the redefinition. The stack looks good.
        retry = false;
        reporter.accept("transforming calling function");
        doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      }
    };
    do {
      // Run ensureJitCompiled here since it might get GCd
      ensureJitCompiled(Transform.class, "sayHi");
      ensureJitCompiled(Transform.class, "privateSayHi");
      // Clear output.
      reporter.clear();
      t.sayHi(2, reporter, () -> { reporter.accept("Not doing anything here"); });
      t.sayHi(2, reporter, do_redefinition);
      t.sayHi(2, reporter, () -> { reporter.accept("Not doing anything here"); });
    } while(retry);
    System.out.println(reporter.getOutput());
  }

  private static native boolean hasJit();

  private static native boolean isInterpretedFunction(Method m, boolean require_deoptimizable);

  private static native void ensureJitCompiled(Class c, String name);
}
