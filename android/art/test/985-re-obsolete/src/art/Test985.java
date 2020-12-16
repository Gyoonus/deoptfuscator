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

public class Test985 {

  static class Transform {
    private void Start() {
      System.out.println("hello - private");
    }

    private void Finish() {
      System.out.println("goodbye - private");
    }

    public void sayHi(Runnable r) {
      System.out.println("Pre Start private method call");
      Start();
      System.out.println("Post Start private method call");
      r.run();
      System.out.println("Pre Finish private method call");
      Finish();
      System.out.println("Post Finish private method call");
    }
  }

  // static class Transform {
  //   private void Start() {
  //     System.out.println("Hello - private - Transformed");
  //   }
  //
  //   private void Finish() {
  //     System.out.println("Goodbye - private - Transformed");
  //   }
  //
  //   public void sayHi(Runnable r) {
  //     System.out.println("Pre Start private method call - Transformed");
  //     Start();
  //     System.out.println("Post Start private method call - Transformed");
  //     r.run();
  //     System.out.println("Pre Finish private method call - Transformed");
  //     Finish();
  //     System.out.println("Post Finish private method call - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES_1 = Base64.getDecoder().decode(
    "yv66vgAAADQANgoADgAZCQAaABsIABwKAB0AHggAHwgAIAoADQAhCAAiCwAjACQIACUKAA0AJggA" +
    "JwcAKQcALAEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAAVTdGFydAEA" +
    "BkZpbmlzaAEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7KVYBAApTb3VyY2VGaWxlAQAM" +
    "VGVzdDk4NS5qYXZhDAAPABAHAC0MAC4ALwEAHUhlbGxvIC0gcHJpdmF0ZSAtIFRyYW5zZm9ybWVk" +
    "BwAwDAAxADIBAB9Hb29kYnllIC0gcHJpdmF0ZSAtIFRyYW5zZm9ybWVkAQArUHJlIFN0YXJ0IHBy" +
    "aXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAwAEwAQAQAsUG9zdCBTdGFydCBwcml2YXRl" +
    "IG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQHADMMADQAEAEALFByZSBGaW5pc2ggcHJpdmF0ZSBt" +
    "ZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkDAAUABABAC1Qb3N0IEZpbmlzaCBwcml2YXRlIG1ldGhv" +
    "ZCBjYWxsIC0gVHJhbnNmb3JtZWQHADUBABVhcnQvVGVzdDk4NSRUcmFuc2Zvcm0BAAlUcmFuc2Zv" +
    "cm0BAAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEA" +
    "A291dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmlu" +
    "dGxuAQAVKExqYXZhL2xhbmcvU3RyaW5nOylWAQASamF2YS9sYW5nL1J1bm5hYmxlAQADcnVuAQAL" +
    "YXJ0L1Rlc3Q5ODUAIAANAA4AAAAAAAQAAAAPABAAAQARAAAAHQABAAEAAAAFKrcAAbEAAAABABIA" +
    "AAAGAAEAAAAEAAIAEwAQAAEAEQAAACUAAgABAAAACbIAAhIDtgAEsQAAAAEAEgAAAAoAAgAAAAYA" +
    "CAAHAAIAFAAQAAEAEQAAACUAAgABAAAACbIAAhIFtgAEsQAAAAEAEgAAAAoAAgAAAAkACAAKAAEA" +
    "FQAWAAEAEQAAAGMAAgACAAAAL7IAAhIGtgAEKrcAB7IAAhIItgAEK7kACQEAsgACEgq2AAQqtwAL" +
    "sgACEgy2AASxAAAAAQASAAAAIgAIAAAADAAIAA0ADAAOABQADwAaABAAIgARACYAEgAuABMAAgAX" +
    "AAAAAgAYACsAAAAKAAEADQAoACoACA==");
  private static final byte[] DEX_BYTES_1 = Base64.getDecoder().decode(
    "ZGV4CjAzNQAh+CJbAAAAAAAAAAAAAAAAAAAAAAAAAADUBQAAcAAAAHhWNBIAAAAAAAAAABAFAAAd" +
    "AAAAcAAAAAoAAADkAAAAAwAAAAwBAAABAAAAMAEAAAcAAAA4AQAAAQAAAHABAABEBAAAkAEAAJAB" +
    "AACYAQAAoAEAAMEBAADgAQAA+QEAAAgCAAAsAgAATAIAAGMCAAB3AgAAjQIAAKECAAC1AgAA5AIA" +
    "ABIDAABAAwAAbQMAAHQDAACCAwAAjQMAAJADAACUAwAAoQMAAKcDAACsAwAAtQMAALoDAADBAwAA" +
    "BAAAAAUAAAAGAAAABwAAAAgAAAAJAAAACgAAAAsAAAAMAAAAFAAAABQAAAAJAAAAAAAAABUAAAAJ" +
    "AAAA0AMAABUAAAAJAAAAyAMAAAgABAAYAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAARAAAAAAABABsA" +
    "AAAEAAIAGQAAAAUAAAAAAAAABgAAABoAAAAAAAAAAAAAAAUAAAAAAAAAEgAAAAAFAADMBAAAAAAA" +
    "AAY8aW5pdD4ABkZpbmlzaAAfR29vZGJ5ZSAtIHByaXZhdGUgLSBUcmFuc2Zvcm1lZAAdSGVsbG8g" +
    "LSBwcml2YXRlIC0gVHJhbnNmb3JtZWQAF0xhcnQvVGVzdDk4NSRUcmFuc2Zvcm07AA1MYXJ0L1Rl" +
    "c3Q5ODU7ACJMZGFsdmlrL2Fubm90YXRpb24vRW5jbG9zaW5nQ2xhc3M7AB5MZGFsdmlrL2Fubm90" +
    "YXRpb24vSW5uZXJDbGFzczsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwASTGphdmEvbGFuZy9PYmpl" +
    "Y3Q7ABRMamF2YS9sYW5nL1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABJMamF2YS9sYW5n" +
    "L1N5c3RlbTsALVBvc3QgRmluaXNoIHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAAs" +
    "UG9zdCBTdGFydCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQALFByZSBGaW5pc2gg" +
    "cHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkACtQcmUgU3RhcnQgcHJpdmF0ZSBtZXRo" +
    "b2QgY2FsbCAtIFRyYW5zZm9ybWVkAAVTdGFydAAMVGVzdDk4NS5qYXZhAAlUcmFuc2Zvcm0AAVYA" +
    "AlZMAAthY2Nlc3NGbGFncwAEbmFtZQADb3V0AAdwcmludGxuAANydW4ABXNheUhpAAV2YWx1ZQAB" +
    "AAAABwAAAAEAAAAGAAAABAAHDgAJAAcOAQgPAAYABw4BCA8ADAEABw4BCA8BAw8BCA8BAw8BCA8B" +
    "Aw8BCA8AAQABAAEAAADYAwAABAAAAHAQBQAAAA4AAwABAAIAAADdAwAACQAAAGIAAAAbAQIAAABu" +
    "IAQAEAAOAAAAAwABAAIAAADlAwAACQAAAGIAAAAbAQMAAABuIAQAEAAOAAAABAACAAIAAADtAwAA" +
    "KgAAAGIAAAAbARAAAABuIAQAEABwEAIAAgBiAAAAGwEOAAAAbiAEABAAchAGAAMAYgAAABsBDwAA" +
    "AG4gBAAQAHAQAQACAGIAAAAbAQ0AAABuIAQAEAAOAAAAAwEAgIAEiAgBAqAIAQLECAMB6AgAAAIC" +
    "ARwYAQIDAhYECBcXEwACAAAA5AQAAOoEAAD0BAAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAA" +
    "AAEAAAAdAAAAcAAAAAIAAAAKAAAA5AAAAAMAAAADAAAADAEAAAQAAAABAAAAMAEAAAUAAAAHAAAA" +
    "OAEAAAYAAAABAAAAcAEAAAIgAAAdAAAAkAEAAAEQAAACAAAAyAMAAAMgAAAEAAAA2AMAAAEgAAAE" +
    "AAAACAQAAAAgAAABAAAAzAQAAAQgAAACAAAA5AQAAAMQAAABAAAA9AQAAAYgAAABAAAAAAUAAAAQ" +
    "AAABAAAAEAUAAA==");

  // static class Transform {
  //   private void Start() {
  //     System.out.println("second - Hello - private - Transformed");
  //   }
  //
  //   private void Finish() {
  //     System.out.println("second - Goodbye - private - Transformed");
  //   }
  //
  //   public void sayHi(Runnable r) {
  //     System.out.println("second - Pre Start private method call - Transformed");
  //     Start();
  //     System.out.println("second - Post Start private method call - Transformed");
  //     r.run();
  //     System.out.println("second - Pre Finish private method call - Transformed");
  //     Finish();
  //     System.out.println("second - Post Finish private method call - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES_2 = Base64.getDecoder().decode(
    "yv66vgAAADQANgoADgAZCQAaABsIABwKAB0AHggAHwgAIAoADQAhCAAiCwAjACQIACUKAA0AJggA" +
    "JwcAKQcALAEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAAVTdGFydAEA" +
    "BkZpbmlzaAEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7KVYBAApTb3VyY2VGaWxlAQAM" +
    "VGVzdDk4NS5qYXZhDAAPABAHAC0MAC4ALwEAJnNlY29uZCAtIEhlbGxvIC0gcHJpdmF0ZSAtIFRy" +
    "YW5zZm9ybWVkBwAwDAAxADIBAChzZWNvbmQgLSBHb29kYnllIC0gcHJpdmF0ZSAtIFRyYW5zZm9y" +
    "bWVkAQA0c2Vjb25kIC0gUHJlIFN0YXJ0IHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1l" +
    "ZAwAEwAQAQA1c2Vjb25kIC0gUG9zdCBTdGFydCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNm" +
    "b3JtZWQHADMMADQAEAEANXNlY29uZCAtIFByZSBGaW5pc2ggcHJpdmF0ZSBtZXRob2QgY2FsbCAt" +
    "IFRyYW5zZm9ybWVkDAAUABABADZzZWNvbmQgLSBQb3N0IEZpbmlzaCBwcml2YXRlIG1ldGhvZCBj" +
    "YWxsIC0gVHJhbnNmb3JtZWQHADUBABVhcnQvVGVzdDk4NSRUcmFuc2Zvcm0BAAlUcmFuc2Zvcm0B" +
    "AAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291" +
    "dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxu" +
    "AQAVKExqYXZhL2xhbmcvU3RyaW5nOylWAQASamF2YS9sYW5nL1J1bm5hYmxlAQADcnVuAQALYXJ0" +
    "L1Rlc3Q5ODUAIAANAA4AAAAAAAQAAAAPABAAAQARAAAAHQABAAEAAAAFKrcAAbEAAAABABIAAAAG" +
    "AAEAAAAEAAIAEwAQAAEAEQAAACUAAgABAAAACbIAAhIDtgAEsQAAAAEAEgAAAAoAAgAAAAYACAAH" +
    "AAIAFAAQAAEAEQAAACUAAgABAAAACbIAAhIFtgAEsQAAAAEAEgAAAAoAAgAAAAkACAAKAAEAFQAW" +
    "AAEAEQAAAGMAAgACAAAAL7IAAhIGtgAEKrcAB7IAAhIItgAEK7kACQEAsgACEgq2AAQqtwALsgAC" +
    "Egy2AASxAAAAAQASAAAAIgAIAAAADAAIAA0ADAAOABQADwAaABAAIgARACYAEgAuABMAAgAXAAAA" +
    "AgAYACsAAAAKAAEADQAoACoACA==");
  private static final byte[] DEX_BYTES_2 = Base64.getDecoder().decode(
    "ZGV4CjAzNQBw/x+UAAAAAAAAAAAAAAAAAAAAAAAAAAAMBgAAcAAAAHhWNBIAAAAAAAAAAEgFAAAd" +
    "AAAAcAAAAAoAAADkAAAAAwAAAAwBAAABAAAAMAEAAAcAAAA4AQAAAQAAAHABAAB8BAAAkAEAAJAB" +
    "AACYAQAAoAEAALkBAADIAQAA7AEAAAwCAAAjAgAANwIAAE0CAABhAgAAdQIAAHwCAACKAgAAlQIA" +
    "AJgCAACcAgAAqQIAAK8CAAC0AgAAvQIAAMICAADJAgAA8wIAABsDAABTAwAAigMAAMEDAAD3AwAA" +
    "AgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAKAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJ" +
    "AAAACAQAAA8AAAAJAAAAAAQAAAgABAASAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAALAAAAAAABABUA" +
    "AAAEAAIAEwAAAAUAAAAAAAAABgAAABQAAAAAAAAAAAAAAAUAAAAAAAAADAAAADgFAAAEBQAAAAAA" +
    "AAY8aW5pdD4ABkZpbmlzaAAXTGFydC9UZXN0OTg1JFRyYW5zZm9ybTsADUxhcnQvVGVzdDk4NTsA" +
    "IkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9J" +
    "bm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAFExq" +
    "YXZhL2xhbmcvUnVubmFibGU7ABJMamF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVt" +
    "OwAFU3RhcnQADFRlc3Q5ODUuamF2YQAJVHJhbnNmb3JtAAFWAAJWTAALYWNjZXNzRmxhZ3MABG5h" +
    "bWUAA291dAAHcHJpbnRsbgADcnVuAAVzYXlIaQAoc2Vjb25kIC0gR29vZGJ5ZSAtIHByaXZhdGUg" +
    "LSBUcmFuc2Zvcm1lZAAmc2Vjb25kIC0gSGVsbG8gLSBwcml2YXRlIC0gVHJhbnNmb3JtZWQANnNl" +
    "Y29uZCAtIFBvc3QgRmluaXNoIHByaXZhdGUgbWV0aG9kIGNhbGwgLSBUcmFuc2Zvcm1lZAA1c2Vj" +
    "b25kIC0gUG9zdCBTdGFydCBwcml2YXRlIG1ldGhvZCBjYWxsIC0gVHJhbnNmb3JtZWQANXNlY29u" +
    "ZCAtIFByZSBGaW5pc2ggcHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkADRzZWNvbmQg" +
    "LSBQcmUgU3RhcnQgcHJpdmF0ZSBtZXRob2QgY2FsbCAtIFRyYW5zZm9ybWVkAAV2YWx1ZQAAAAEA" +
    "AAAHAAAAAQAAAAYAAAAEAAcOAAkABw4BCA8ABgAHDgEIDwAMAQAHDgEIDwEDDwEIDwEDDwEIDwED" +
    "DwEIDwABAAEAAQAAABAEAAAEAAAAcBAFAAAADgADAAEAAgAAABUEAAAJAAAAYgAAABsBFgAAAG4g" +
    "BAAQAA4AAAADAAEAAgAAAB0EAAAJAAAAYgAAABsBFwAAAG4gBAAQAA4AAAAEAAIAAgAAACUEAAAq" +
    "AAAAYgAAABsBGwAAAG4gBAAQAHAQAgACAGIAAAAbARkAAABuIAQAEAByEAYAAwBiAAAAGwEaAAAA" +
    "biAEABAAcBABAAIAYgAAABsBGAAAAG4gBAAQAA4AAAADAQCAgATACAEC2AgBAvwIAwGgCQAAAgIB" +
    "HBgBAgMCEAQIERcNAAIAAAAcBQAAIgUAACwFAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAEAAAAAAAAA" +
    "AQAAAB0AAABwAAAAAgAAAAoAAADkAAAAAwAAAAMAAAAMAQAABAAAAAEAAAAwAQAABQAAAAcAAAA4" +
    "AQAABgAAAAEAAABwAQAAAiAAAB0AAACQAQAAARAAAAIAAAAABAAAAyAAAAQAAAAQBAAAASAAAAQA" +
    "AABABAAAACAAAAEAAAAEBQAABCAAAAIAAAAcBQAAAxAAAAEAAAAsBQAABiAAAAEAAAA4BQAAABAA" +
    "AAEAAABIBQAA");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
    t.sayHi(() -> {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES_1, DEX_BYTES_1);
    });
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
    t.sayHi(() -> {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES_2, DEX_BYTES_2);
    });
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
  }
}
