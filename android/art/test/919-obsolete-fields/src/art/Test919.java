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

package art;

import java.util.function.Consumer;
import java.util.Base64;

public class Test919 {

  static class Transform {
    private Consumer<String> reporter;
    public Transform(Consumer<String> reporter) {
      this.reporter = reporter;
    }

    private void Start() {
      reporter.accept("hello - private");
    }

    private void Finish() {
      reporter.accept("goodbye - private");
    }

    public void sayHi(Runnable r) {
      reporter.accept("Pre Start private method call");
      Start();
      reporter.accept("Post Start private method call");
      r.run();
      reporter.accept("Pre Finish private method call");
      Finish();
      reporter.accept("Post Finish private method call");
    }
  }


  // What follows is the base64 encoded representation of the following class:
  //
  // import java.util.function.Consumer;
  //
  // static class Transform {
  //   private Consumer<String> reporter;
  //   public Transform(Consumer<String> reporter) {
  //     this.reporter = reporter;
  //   }
  //
  //   private void Start() {
  //     reporter.accept("Hello - private - Transformed");
  //   }
  //
  //   private void Finish() {
  //     reporter.accept("Goodbye - private - Transformed");
  //   }
  //
  //   public void sayHi(Runnable r) {
  //     reporter.accept("pre Start private method call - Transformed");
  //     Start();
  //     reporter.accept("post Start private method call - Transformed");
  //     r.run();
  //     reporter.accept("pre Finish private method call - Transformed");
  //     Finish();
  //     reporter.accept("post Finish private method call - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAOAoADgAfCQANACAIACELACIAIwgAJAgAJQoADQAmCAAnCwAoACkIACoKAA0AKwgA" +
    "LAcALgcAMQEACHJlcG9ydGVyAQAdTGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjsBAAlTaWdu" +
    "YXR1cmUBADFMamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyPExqYXZhL2xhbmcvU3RyaW5nOz47" +
    "AQAGPGluaXQ+AQAgKExqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7KVYBAARDb2RlAQAPTGlu" +
    "ZU51bWJlclRhYmxlAQA0KExqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI8TGphdmEvbGFuZy9T" +
    "dHJpbmc7PjspVgEABVN0YXJ0AQADKClWAQAGRmluaXNoAQAFc2F5SGkBABcoTGphdmEvbGFuZy9S" +
    "dW5uYWJsZTspVgEAClNvdXJjZUZpbGUBAAxUZXN0OTE5LmphdmEMABMAGQwADwAQAQAdSGVsbG8g" +
    "LSBwcml2YXRlIC0gVHJhbnNmb3JtZWQHADIMADMANAEAH0dvb2RieWUgLSBwcml2YXRlIC0gVHJh" +
    "bnNmb3JtZWQBACtwcmUgU3RhcnQgcHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkDAAY" +
    "ABkBACxwb3N0IFN0YXJ0IHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAcANQwANgAZ" +
    "AQAscHJlIEZpbmlzaCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQMABoAGQEALXBv" +
    "c3QgRmluaXNoIHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAcANwEAFWFydC9UZXN0" +
    "OTE5JFRyYW5zZm9ybQEACVRyYW5zZm9ybQEADElubmVyQ2xhc3NlcwEAEGphdmEvbGFuZy9PYmpl" +
    "Y3QBABtqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXIBAAZhY2NlcHQBABUoTGphdmEvbGFuZy9P" +
    "YmplY3Q7KVYBABJqYXZhL2xhbmcvUnVubmFibGUBAANydW4BAAthcnQvVGVzdDkxOQAgAA0ADgAA" +
    "AAEAAgAPABAAAQARAAAAAgASAAQAAQATABQAAgAVAAAAKgACAAIAAAAKKrcAASortQACsQAAAAEA" +
    "FgAAAA4AAwAAAAgABAAJAAkACgARAAAAAgAXAAIAGAAZAAEAFQAAACgAAgABAAAADCq0AAISA7kA" +
    "BAIAsQAAAAEAFgAAAAoAAgAAAA0ACwAOAAIAGgAZAAEAFQAAACgAAgABAAAADCq0AAISBbkABAIA" +
    "sQAAAAEAFgAAAAoAAgAAABEACwASAAEAGwAcAAEAFQAAAG8AAgACAAAAOyq0AAISBrkABAIAKrcA" +
    "Byq0AAISCLkABAIAK7kACQEAKrQAAhIKuQAEAgAqtwALKrQAAhIMuQAEAgCxAAAAAQAWAAAAIgAI" +
    "AAAAFQALABYADwAXABoAGAAgABkAKwAaAC8AGwA6ABwAAgAdAAAAAgAeADAAAAAKAAEADQAtAC8A" +
    "CA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBeEZYBAAAAAAAAAAAAAAAAAAAAAAAAAACMBgAAcAAAAHhWNBIAAAAAAAAAAMgFAAAi" +
    "AAAAcAAAAAkAAAD4AAAABAAAABwBAAABAAAATAEAAAcAAABUAQAAAQAAAIwBAADgBAAArAEAAKwB" +
    "AACvAQAAsgEAALoBAAC+AQAAxAEAAMwBAADtAQAADAIAACUCAAA0AgAAWAIAAHgCAACXAgAAqwIA" +
    "AMECAADVAgAA8wIAABIDAAAZAwAAJwMAADIDAAA1AwAAOQMAAEEDAABOAwAAVAMAAIMDAACxAwAA" +
    "3wMAAAwEAAAWBAAAGwQAACIEAAAIAAAACQAAAAoAAAALAAAADAAAAA0AAAAOAAAAEQAAABUAAAAV" +
    "AAAACAAAAAAAAAAWAAAACAAAADQEAAAWAAAACAAAADwEAAAWAAAACAAAACwEAAAAAAcAHgAAAAAA" +
    "AwACAAAAAAAAAAUAAAAAAAAAEgAAAAAAAgAgAAAABQAAAAIAAAAGAAAAHwAAAAcAAQAXAAAAAAAA" +
    "AAAAAAAFAAAAAAAAABMAAACoBQAARAUAAAAAAAABKAABPAAGPGluaXQ+AAI+OwAEPjspVgAGRmlu" +
    "aXNoAB9Hb29kYnllIC0gcHJpdmF0ZSAtIFRyYW5zZm9ybWVkAB1IZWxsbyAtIHByaXZhdGUgLSBU" +
    "cmFuc2Zvcm1lZAAXTGFydC9UZXN0OTE5JFRyYW5zZm9ybTsADUxhcnQvVGVzdDkxOTsAIkxkYWx2" +
    "aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNs" +
    "YXNzOwAdTGRhbHZpay9hbm5vdGF0aW9uL1NpZ25hdHVyZTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAU" +
    "TGphdmEvbGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwAcTGphdmEvdXRpbC9mdW5j" +
    "dGlvbi9Db25zdW1lcgAdTGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjsABVN0YXJ0AAxUZXN0" +
    "OTE5LmphdmEACVRyYW5zZm9ybQABVgACVkwABmFjY2VwdAALYWNjZXNzRmxhZ3MABG5hbWUALXBv" +
    "c3QgRmluaXNoIHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAAscG9zdCBTdGFydCBw" +
    "cml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQALHByZSBGaW5pc2ggcHJpdmF0ZSBtZXRo" +
    "b2QgY2FsbCAtIFRyYW5zZm9ybWVkACtwcmUgU3RhcnQgcHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRy" +
    "YW5zZm9ybWVkAAhyZXBvcnRlcgADcnVuAAVzYXlIaQAFdmFsdWUAAAAAAQAAAAcAAAABAAAABQAA" +
    "AAEAAAAGAAAACAEABw4BAw8BAg8AEQAHDgEIDwANAAcOAQgPABUBAAcOAQgPAQMPAQgPAQMPAQgP" +
    "AQMPAQgPAAACAAIAAQAAAEQEAAAGAAAAcBAEAAAAWwEAAA4AAwABAAIAAABQBAAACQAAAFQgAAAb" +
    "AQYAAAByIAYAEAAOAAAAAwABAAIAAABYBAAACQAAAFQgAAAbAQcAAAByIAYAEAAOAAAABAACAAIA" +
    "AABgBAAAKgAAAFQgAAAbAR0AAAByIAYAEABwEAIAAgBUIAAAGwEbAAAAciAGABAAchAFAAMAVCAA" +
    "ABsBHAAAAHIgBgAQAHAQAQACAFQgAAAbARoAAAByIAYAEAAOAAABAwEAAgCBgAT8CAECmAkBArwJ" +
    "AwHgCQICASEYAQIDAhgECBkXFAIEASEcBBcQFwEXDxcDAgQBIRwFFwAXEBcBFw8XBAAAAAIAAABc" +
    "BQAAYgUAAAEAAABrBQAAAQAAAHkFAACMBQAAAQAAAAEAAAAAAAAAAAAAAJgFAAAAAAAAoAUAABAA" +
    "AAAAAAAAAQAAAAAAAAABAAAAIgAAAHAAAAACAAAACQAAAPgAAAADAAAABAAAABwBAAAEAAAAAQAA" +
    "AEwBAAAFAAAABwAAAFQBAAAGAAAAAQAAAIwBAAACIAAAIgAAAKwBAAABEAAAAwAAACwEAAADIAAA" +
    "BAAAAEQEAAABIAAABAAAAHwEAAAAIAAAAQAAAEQFAAAEIAAABAAAAFwFAAADEAAAAwAAAIwFAAAG" +
    "IAAAAQAAAKgFAAAAEAAAAQAAAMgFAAA=");

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
  }

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    TestWatcher w = new TestWatcher();
    doTest(new Transform(w), w);
  }

  public static void doTest(Transform t, TestWatcher w) {
    Runnable do_redefinition = () -> {
      w.accept("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    };
    // This just prints something out to show we are running the Runnable.
    Runnable say_nothing = () -> { w.accept("Not doing anything here"); };

    // Try and redefine.
    t.sayHi(say_nothing);
    t.sayHi(do_redefinition);
    t.sayHi(say_nothing);

    // Print output of last run.
    System.out.print(w.getOutput());
  }
}
