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

import java.util.Base64;
public class Test1910 {
  static interface TestInterface {
    public void sayHi();
    public default void sayHiTwice() {
      sayHi();
      sayHi();
    }
  }

  static class Transform implements TestInterface {
    public void sayHi() {
      System.out.println("hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform implements TestInterface {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAIwoABgAPCQAQABEIABIKABMAFAcAFgcAGQcAGgEABjxpbml0PgEAAygpVgEABENv" +
    "ZGUBAA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAA1UZXN0MTkxMC5qYXZh" +
    "DAAIAAkHABwMAB0AHgEAB0dvb2RieWUHAB8MACAAIQcAIgEAFmFydC9UZXN0MTkxMCRUcmFuc2Zv" +
    "cm0BAAlUcmFuc2Zvcm0BAAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAaYXJ0L1Rl" +
    "c3QxOTEwJFRlc3RJbnRlcmZhY2UBAA1UZXN0SW50ZXJmYWNlAQAQamF2YS9sYW5nL1N5c3RlbQEA" +
    "A291dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmlu" +
    "dGxuAQAVKExqYXZhL2xhbmcvU3RyaW5nOylWAQAMYXJ0L1Rlc3QxOTEwACAABQAGAAEABwAAAAIA" +
    "AAAIAAkAAQAKAAAAHQABAAEAAAAFKrcAAbEAAAABAAsAAAAGAAEAAAAdAAEADAAJAAEACgAAACUA" +
    "AgABAAAACbIAAhIDtgAEsQAAAAEACwAAAAoAAgAAAB8ACAAgAAIADQAAAAIADgAYAAAAEgACAAUA" +
    "FQAXAAgABwAVABsGCA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCimuj5gqsyBEhWaMcfKWwG9eiBycoK3JfcAwAAcAAAAHhWNBIAAAAAAAAAABgDAAAV" +
    "AAAAcAAAAAoAAADEAAAAAgAAAOwAAAABAAAABAEAAAQAAAAMAQAAAQAAACwBAACQAgAATAEAAK4B" +
    "AAC2AQAAvwEAAN0BAAD3AQAABwIAACsCAABLAgAAYgIAAHYCAACKAgAAngIAAK0CAAC4AgAAuwIA" +
    "AL8CAADMAgAA0gIAANcCAADgAgAA5wIAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAAJAAAA" +
    "CgAAAA0AAAANAAAACQAAAAAAAAAOAAAACQAAAKgBAAAIAAUAEQAAAAEAAAAAAAAAAQAAABMAAAAF" +
    "AAEAEgAAAAYAAAAAAAAAAQAAAAAAAAAGAAAAoAEAAAsAAACQAQAACAMAAAAAAAACAAAA+QIAAP8C" +
    "AAABAAEAAQAAAO4CAAAEAAAAcBADAAAADgADAAEAAgAAAPMCAAAIAAAAYgAAABoBAQBuIAIAEAAO" +
    "AEwBAAAAAAAAAAAAAAAAAAABAAAAAAAAAAEAAAAHAAY8aW5pdD4AB0dvb2RieWUAHExhcnQvVGVz" +
    "dDE5MTAkVGVzdEludGVyZmFjZTsAGExhcnQvVGVzdDE5MTAkVHJhbnNmb3JtOwAOTGFydC9UZXN0" +
    "MTkxMDsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3Rh" +
    "dGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVj" +
    "dDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AA1UZXN0MTkxMC5qYXZh" +
    "AAlUcmFuc2Zvcm0AAVYAAlZMAAthY2Nlc3NGbGFncwAEbmFtZQADb3V0AAdwcmludGxuAAVzYXlI" +
    "aQAFdmFsdWUAHQAHDgAfAAcOeAACAwEUGAICBAIPBAgQFwwAAAEBAICABNgCAQHwAgAAEAAAAAAA" +
    "AAABAAAAAAAAAAEAAAAVAAAAcAAAAAIAAAAKAAAAxAAAAAMAAAACAAAA7AAAAAQAAAABAAAABAEA" +
    "AAUAAAAEAAAADAEAAAYAAAABAAAALAEAAAMQAAABAAAATAEAAAEgAAACAAAAWAEAAAYgAAABAAAA" +
    "kAEAAAEQAAACAAAAoAEAAAIgAAAVAAAArgEAAAMgAAACAAAA7gIAAAQgAAACAAAA+QIAAAAgAAAB" +
    "AAAACAMAAAAQAAABAAAAGAMAAA==");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(TestInterface t) {
    t.sayHiTwice();
    Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    t.sayHiTwice();
  }
}
