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

import java.util.Base64;

public class Test940 {

  static class Transform {
    public void sayHi(int recur, Runnable r) {
      System.out.println("hello" + recur);
      if (recur == 1) {
        r.run();
        sayHi(recur - 1, r);
      } else if (recur != 0) {
        sayHi(recur - 1, r);
      }
      System.out.println("goodbye" + recur);
    }
  }


  // static class Transform {
  //   public void sayHi(int recur, Runnable r) {
  //     System.out.println("Hello" + recur + " - transformed");
  //     if (recur == 1) {
  //       r.run();
  //       sayHi(recur - 1, r);
  //     } else if (recur != 0) {
  //       sayHi(recur - 1, r);
  //     }
  //     System.out.println("Goodbye" + recur + " - transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAOwoADwAZCQAaABsHABwKAAMAGQgAHQoAAwAeCgADAB8IACAKAAMAIQoAIgAjCwAk" +
    "ACUKAA4AJggAJwcAKQcALAEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUB" +
    "AAVzYXlIaQEAGChJTGphdmEvbGFuZy9SdW5uYWJsZTspVgEADVN0YWNrTWFwVGFibGUBAApTb3Vy" +
    "Y2VGaWxlAQAMVGVzdDk0MC5qYXZhDAAQABEHAC0MAC4ALwEAF2phdmEvbGFuZy9TdHJpbmdCdWls" +
    "ZGVyAQAFSGVsbG8MADAAMQwAMAAyAQAOIC0gdHJhbnNmb3JtZWQMADMANAcANQwANgA3BwA4DAA5" +
    "ABEMABQAFQEAB0dvb2RieWUHADoBABVhcnQvVGVzdDk0MCRUcmFuc2Zvcm0BAAlUcmFuc2Zvcm0B" +
    "AAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291" +
    "dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEABmFwcGVuZAEALShMamF2YS9sYW5nL1N0cmluZzsp" +
    "TGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwEAHChJKUxqYXZhL2xhbmcvU3RyaW5nQnVpbGRlcjsB" +
    "AAh0b1N0cmluZwEAFCgpTGphdmEvbGFuZy9TdHJpbmc7AQATamF2YS9pby9QcmludFN0cmVhbQEA" +
    "B3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBABJqYXZhL2xhbmcvUnVubmFibGUBAANy" +
    "dW4BAAthcnQvVGVzdDk0MAAgAA4ADwAAAAAAAgAAABAAEQABABIAAAAdAAEAAQAAAAUqtwABsQAA" +
    "AAEAEwAAAAYAAQAAAAUAAQAUABUAAQASAAAAnQADAAMAAABfsgACuwADWbcABBIFtgAGG7YABxII" +
    "tgAGtgAJtgAKGwSgABQsuQALAQAqGwRkLLYADKcADxuZAAsqGwRkLLYADLIAArsAA1m3AAQSDbYA" +
    "Bhu2AAcSCLYABrYACbYACrEAAAACABMAAAAiAAgAAAAHAB4ACAAjAAkAKQAKADQACwA4AAwAQAAO" +
    "AF4ADwAWAAAABAACNAsAAgAXAAAAAgAYACsAAAAKAAEADgAoACoACA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDQv3jgAAAAAAAAAAAAAAAAAAAAAAAAAAB8BQAAcAAAAHhWNBIAAAAAAAAAALgEAAAg" +
    "AAAAcAAAAAwAAADwAAAABgAAACABAAABAAAAaAEAAAkAAABwAQAAAQAAALgBAACkAwAA2AEAANgB" +
    "AADoAQAA8AEAAPkBAAAAAgAAAwIAAAYCAAAKAgAADgIAACcCAAA2AgAAWgIAAHoCAACRAgAApQIA" +
    "ALsCAADPAgAA6gIAAP4CAAAMAwAAFwMAABoDAAAfAwAAIwMAADADAAA4AwAAPgMAAEMDAABMAwAA" +
    "UQMAAFgDAABiAwAABAAAAAgAAAAJAAAACgAAAAsAAAAMAAAADQAAAA4AAAAPAAAAEAAAABEAAAAU" +
    "AAAABQAAAAgAAAAAAAAABgAAAAkAAAB8AwAABwAAAAkAAAB0AwAAFAAAAAsAAAAAAAAAFQAAAAsA" +
    "AABsAwAAFgAAAAsAAAB0AwAACgAFABoAAAABAAMAAQAAAAEABAAdAAAABQAFABsAAAAGAAMAAQAA" +
    "AAcAAwAcAAAACQADAAEAAAAJAAEAGAAAAAkAAgAYAAAACQAAAB4AAAABAAAAAAAAAAYAAAAAAAAA" +
    "EgAAAKgEAAB8BAAAAAAAAA4gLSB0cmFuc2Zvcm1lZAAGPGluaXQ+AAdHb29kYnllAAVIZWxsbwAB" +
    "SQABTAACTEkAAkxMABdMYXJ0L1Rlc3Q5NDAkVHJhbnNmb3JtOwANTGFydC9UZXN0OTQwOwAiTGRh" +
    "bHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVy" +
    "Q2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEv" +
    "bGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwAZTGphdmEvbGFuZy9TdHJpbmdCdWls" +
    "ZGVyOwASTGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0OTQwLmphdmEACVRyYW5zZm9ybQABVgADVklM" +
    "AAJWTAALYWNjZXNzRmxhZ3MABmFwcGVuZAAEbmFtZQADb3V0AAdwcmludGxuAANydW4ABXNheUhp" +
    "AAh0b1N0cmluZwAFdmFsdWUAAAAAAgAAAAAABwABAAAACAAAAAEAAAAAAAAABQAHDgAHAgAABw4B" +
    "IA8BAw8BAw8BBRIBIA8BAQoBAg8AAAAAAQABAAEAAACEAwAABAAAAHAQAwAAAA4ABgADAAMAAACJ" +
    "AwAAVQAAAGIAAAAiAQkAcBAFAAEAGwIDAAAAbiAHACEADAFuIAYAQQAMARsCAAAAAG4gBwAhAAwB" +
    "bhAIAAEADAFuIAIAEAASEDMEKwByEAQABQDYAAT/bjABAAMFYgAAACIBCQBwEAUAAQAbAgIAAABu" +
    "IAcAIQAMAW4gBgBBAAwBGwIAAAAAbiAHACEADAFuEAgAAQAMAW4gAgAQAA4AOATf/9gABP9uMAEA" +
    "AwUpANj/AAAAAAEBAICABKgHAQHABwAAAgMBHxgCAgQCFwQIGRcTAAIAAACMBAAAkgQAAJwEAAAA" +
    "AAAAAAAAAAAAAAAQAAAAAAAAAAEAAAAAAAAAAQAAACAAAABwAAAAAgAAAAwAAADwAAAAAwAAAAYA" +
    "AAAgAQAABAAAAAEAAABoAQAABQAAAAkAAABwAQAABgAAAAEAAAC4AQAAAiAAACAAAADYAQAAARAA" +
    "AAMAAABsAwAAAyAAAAIAAACEAwAAASAAAAIAAACoAwAAACAAAAEAAAB8BAAABCAAAAIAAACMBAAA" +
    "AxAAAAEAAACcBAAABiAAAAEAAACoBAAAABAAAAEAAAC4BAAA");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    t.sayHi(2, () -> { System.out.println("Not doing anything here"); });
    t.sayHi(2, () -> {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    });
    t.sayHi(2, () -> { System.out.println("Not doing anything here"); });
  }
}
