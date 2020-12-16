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

  // import java.util.function.Consumer;
  // class Transform {
  //   public void sayHi(int recur, Consumer<String> reporter, Runnable r) {
  //     reporter.accept("Hello" + recur + " - transformed");
  //     if (recur == 1) {
  //       r.run();
  //       sayHi(recur - 1, reporter, r);
  //     } else if (recur != 0) {
  //       sayHi(recur - 1, reporter, r);
  //     }
  //     reporter.accept("Goodbye" + recur + " - transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAMwoADgAaBwAbCgACABoIABwKAAIAHQoAAgAeCAAfCgACACALACEAIgsAIwAkCgAN" +
    "ACUIACYHACcHACgBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5" +
    "SGkBADUoSUxqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7TGphdmEvbGFuZy9SdW5uYWJsZTsp" +
    "VgEADVN0YWNrTWFwVGFibGUBAAlTaWduYXR1cmUBAEkoSUxqYXZhL3V0aWwvZnVuY3Rpb24vQ29u" +
    "c3VtZXI8TGphdmEvbGFuZy9TdHJpbmc7PjtMamF2YS9sYW5nL1J1bm5hYmxlOylWAQAKU291cmNl" +
    "RmlsZQEADlRyYW5zZm9ybS5qYXZhDAAPABABABdqYXZhL2xhbmcvU3RyaW5nQnVpbGRlcgEABUhl" +
    "bGxvDAApACoMACkAKwEADiAtIHRyYW5zZm9ybWVkDAAsAC0HAC4MAC8AMAcAMQwAMgAQDAATABQB" +
    "AAdHb29kYnllAQAJVHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAEABmFwcGVuZAEALShMamF2" +
    "YS9sYW5nL1N0cmluZzspTGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwEAHChJKUxqYXZhL2xhbmcv" +
    "U3RyaW5nQnVpbGRlcjsBAAh0b1N0cmluZwEAFCgpTGphdmEvbGFuZy9TdHJpbmc7AQAbamF2YS91" +
    "dGlsL2Z1bmN0aW9uL0NvbnN1bWVyAQAGYWNjZXB0AQAVKExqYXZhL2xhbmcvT2JqZWN0OylWAQAS" +
    "amF2YS9sYW5nL1J1bm5hYmxlAQADcnVuACAADQAOAAAAAAACAAAADwAQAAEAEQAAAB0AAQABAAAA" +
    "BSq3AAGxAAAAAQASAAAABgABAAAAAgABABMAFAACABEAAACfAAQABAAAAGEsuwACWbcAAxIEtgAF" +
    "G7YABhIHtgAFtgAIuQAJAgAbBKAAFS25AAoBACobBGQsLbYAC6cAEBuZAAwqGwRkLC22AAssuwAC" +
    "WbcAAxIMtgAFG7YABhIHtgAFtgAIuQAJAgCxAAAAAgASAAAAIgAIAAAABAAeAAUAIwAGACkABwA1" +
    "AAgAOQAJAEIACwBgAAwAFQAAAAQAAjUMABYAAAACABcAAQAYAAAAAgAZ");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQA7uevryhDgvad3G3EACTdspZGfNKv2i3kkBQAAcAAAAHhWNBIAAAAAAAAAAGwEAAAf" +
    "AAAAcAAAAAkAAADsAAAABgAAABABAAAAAAAAAAAAAAkAAABYAQAAAQAAAKABAABkAwAAwAEAAMoC" +
    "AADaAgAA3gIAAOICAADlAgAA7QIAAPECAAD6AgAAAQMAAAQDAAAHAwAACwMAAA8DAAAcAwAAOwMA" +
    "AE8DAABlAwAAeQMAAJQDAACyAwAA0QMAAOEDAADkAwAA6gMAAO4DAAD2AwAA/gMAABIEAAAXBAAA" +
    "HgQAACgEAAAIAAAADAAAAA0AAAAOAAAADwAAABAAAAARAAAAEwAAABUAAAAJAAAABQAAAAAAAAAK" +
    "AAAABgAAAKgCAAALAAAABgAAALACAAAVAAAACAAAAAAAAAAWAAAACAAAALgCAAAXAAAACAAAAMQC" +
    "AAABAAMABAAAAAEABAAcAAAAAwADAAQAAAAEAAMAGwAAAAYAAwAEAAAABgABABkAAAAGAAIAGQAA" +
    "AAYAAAAdAAAABwAFABgAAAABAAAAAAAAAAMAAAAAAAAAFAAAAJACAABbBAAAAAAAAAEAAABHBAAA" +
    "AQABAAEAAAAvBAAABAAAAHAQAgAAAA4ABgAEAAQAAAA0BAAAUAAAACIABgBwEAQAAAAbAQcAAABu" +
    "IAYAEAAMAG4gBQAwAAwAGwEAAAAAbiAGABAADABuEAcAAAAMAHIgCAAEABIQMwMpAHIQAwAFANgA" +
    "A/9uQAEAAlQiAAYAcBAEAAAAGwEGAAAAbiAGABAADABuIAUAMAAMABsBAAAAAG4gBgAQAAwAbhAH" +
    "AAAADAByIAgABAAOADgD4f/YAAP/bkABAAJUKNoAAAAAAAAAAAEAAAAAAAAAAQAAAMABAAABAAAA" +
    "AAAAAAEAAAAFAAAAAwAAAAAABwAEAAAAAQAAAAMADiAtIHRyYW5zZm9ybWVkAAIoSQACKVYAATwA" +
    "Bjxpbml0PgACPjsAB0dvb2RieWUABUhlbGxvAAFJAAFMAAJMSQACTEwAC0xUcmFuc2Zvcm07AB1M" +
    "ZGFsdmlrL2Fubm90YXRpb24vU2lnbmF0dXJlOwASTGphdmEvbGFuZy9PYmplY3Q7ABRMamF2YS9s" +
    "YW5nL1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABlMamF2YS9sYW5nL1N0cmluZ0J1aWxk" +
    "ZXI7ABxMamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyAB1MamF2YS91dGlsL2Z1bmN0aW9uL0Nv" +
    "bnN1bWVyOwAOVHJhbnNmb3JtLmphdmEAAVYABFZJTEwAAlZMAAZhY2NlcHQABmFwcGVuZAASZW1p" +
    "dHRlcjogamFjay00LjI0AANydW4ABXNheUhpAAh0b1N0cmluZwAFdmFsdWUAAgAHDgAEAwAAAAcO" +
    "AR4PPDxdAR4PGS0AAgIBHhwHFwEXEhcDFxAXBRcPFwIAAAEBAICABMgDAQHgAwAAAA8AAAAAAAAA" +
    "AQAAAAAAAAABAAAAHwAAAHAAAAACAAAACQAAAOwAAAADAAAABgAAABABAAAFAAAACQAAAFgBAAAG" +
    "AAAAAQAAAKABAAADEAAAAQAAAMABAAABIAAAAgAAAMgBAAAGIAAAAQAAAJACAAABEAAABAAAAKgC" +
    "AAACIAAAHwAAAMoCAAADIAAAAgAAAC8EAAAEIAAAAQAAAEcEAAAAIAAAAQAAAFsEAAAAEAAAAQAA" +
    "AGwEAAA=");

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
    doTest(new Transform());
  }

  private static boolean retry = false;

  public static void doTest(Transform t) {
    final TestWatcher reporter = new TestWatcher();
    Method say_hi_method;
    // Figure out if we can even JIT at all.
    final boolean has_jit = hasJit();
    try {
      say_hi_method = Transform.class.getDeclaredMethod(
          "sayHi", int.class, Consumer.class, Runnable.class);
    } catch (Exception e) {
      System.out.println("Unable to find methods!");
      e.printStackTrace(System.out);
      return;
    }
    // Makes sure the stack is the way we want it for the test and does the redefinition. It will
    // set the retry boolean to true if we need to go around again due to jit code being GCd.
    Runnable do_redefinition = () -> {
      if (has_jit && Main.isInterpretedFunction(say_hi_method, true)) {
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
