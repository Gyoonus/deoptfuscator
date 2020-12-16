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

public class Test942 {

  static class Transform {
    private void privateSayHi(int recur, Runnable r) {
      System.out.println("hello" + recur);
      if (recur == 1) {
        r.run();
        privateSayHi(recur - 1, r);
      } else if (recur != 0) {
        privateSayHi(recur - 1, r);
      }
      System.out.println("goodbye" + recur);
    }

    public void sayHi(int recur, Runnable r) {
      privateSayHi(recur, r);
    }
  }


  // static class Transform {
  //   public void sayHi(int recur, Runnable r) {
  //     privateSayHi(recur, r);
  //   }
  //   private void privateSayHi(int recur, Runnable r) {
  //     System.out.println("Hello" + recur + " - transformed");
  //     if (recur == 1) {
  //       r.run();
  //       privateSayHi(recur - 1, r);
  //     } else if (recur != 0) {
  //       privateSayHi(recur - 1, r);
  //     }
  //     System.out.println("Goodbye" + recur + " - transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAPAoADwAaCgAOABsJABwAHQcAHgoABAAaCAAfCgAEACAKAAQAIQgAIgoABAAjCgAk" +
    "ACULACYAJwgAKAcAKgcALQEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUB" +
    "AAVzYXlIaQEAGChJTGphdmEvbGFuZy9SdW5uYWJsZTspVgEADHByaXZhdGVTYXlIaQEADVN0YWNr" +
    "TWFwVGFibGUBAApTb3VyY2VGaWxlAQAMVGVzdDk0Mi5qYXZhDAAQABEMABYAFQcALgwALwAwAQAX" +
    "amF2YS9sYW5nL1N0cmluZ0J1aWxkZXIBAAVIZWxsbwwAMQAyDAAxADMBAA4gLSB0cmFuc2Zvcm1l" +
    "ZAwANAA1BwA2DAA3ADgHADkMADoAEQEAB0dvb2RieWUHADsBABVhcnQvVGVzdDk0MiRUcmFuc2Zv" +
    "cm0BAAlUcmFuc2Zvcm0BAAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9s" +
    "YW5nL1N5c3RlbQEAA291dAEAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwEABmFwcGVuZAEALShMamF2" +
    "YS9sYW5nL1N0cmluZzspTGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwEAHChJKUxqYXZhL2xhbmcv" +
    "U3RyaW5nQnVpbGRlcjsBAAh0b1N0cmluZwEAFCgpTGphdmEvbGFuZy9TdHJpbmc7AQATamF2YS9p" +
    "by9QcmludFN0cmVhbQEAB3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBABJqYXZhL2xh" +
    "bmcvUnVubmFibGUBAANydW4BAAthcnQvVGVzdDk0MgAgAA4ADwAAAAAAAwAAABAAEQABABIAAAAd" +
    "AAEAAQAAAAUqtwABsQAAAAEAEwAAAAYAAQAAAAUAAQAUABUAAQASAAAAIwADAAMAAAAHKhsstwAC" +
    "sQAAAAEAEwAAAAoAAgAAAAcABgAIAAIAFgAVAAEAEgAAAJ0AAwADAAAAX7IAA7sABFm3AAUSBrYA" +
    "Bxu2AAgSCbYAB7YACrYACxsEoAAULLkADAEAKhsEZCy3AAKnAA8bmQALKhsEZCy3AAKyAAO7AARZ" +
    "twAFEg22AAcbtgAIEgm2AAe2AAq2AAuxAAAAAgATAAAAIgAIAAAACgAeAAsAIwAMACkADQA0AA4A" +
    "OAAPAEAAEQBeABIAFwAAAAQAAjQLAAIAGAAAAAIAGQAsAAAACgABAA4AKQArAAg=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDiy6hGAAAAAAAAAAAAAAAAAAAAAAAAAAC4BQAAcAAAAHhWNBIAAAAAAAAAAPQEAAAh" +
    "AAAAcAAAAAwAAAD0AAAABgAAACQBAAABAAAAbAEAAAoAAAB0AQAAAQAAAMQBAADUAwAA5AEAAOQB" +
    "AAD0AQAA/AEAAAUCAAAMAgAADwIAABICAAAWAgAAGgIAADMCAABCAgAAZgIAAIYCAACdAgAAsQIA" +
    "AMcCAADbAgAA9gIAAAoDAAAYAwAAIwMAACYDAAArAwAALwMAADwDAABEAwAASgMAAE8DAABYAwAA" +
    "ZgMAAGsDAAByAwAAfAMAAAQAAAAIAAAACQAAAAoAAAALAAAADAAAAA0AAAAOAAAADwAAABAAAAAR" +
    "AAAAFAAAAAUAAAAIAAAAAAAAAAYAAAAJAAAAlAMAAAcAAAAJAAAAjAMAABQAAAALAAAAAAAAABUA" +
    "AAALAAAAhAMAABYAAAALAAAAjAMAAAoABQAaAAAAAQADAAEAAAABAAQAHAAAAAEABAAeAAAABQAF" +
    "ABsAAAAGAAMAAQAAAAcAAwAdAAAACQADAAEAAAAJAAEAGAAAAAkAAgAYAAAACQAAAB8AAAABAAAA" +
    "AAAAAAYAAAAAAAAAEgAAAOQEAAC0BAAAAAAAAA4gLSB0cmFuc2Zvcm1lZAAGPGluaXQ+AAdHb29k" +
    "YnllAAVIZWxsbwABSQABTAACTEkAAkxMABdMYXJ0L1Rlc3Q5NDIkVHJhbnNmb3JtOwANTGFydC9U" +
    "ZXN0OTQyOwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5v" +
    "dGF0aW9uL0lubmVyQ2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2Jq" +
    "ZWN0OwAUTGphdmEvbGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwAZTGphdmEvbGFu" +
    "Zy9TdHJpbmdCdWlsZGVyOwASTGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0OTQyLmphdmEACVRyYW5z" +
    "Zm9ybQABVgADVklMAAJWTAALYWNjZXNzRmxhZ3MABmFwcGVuZAAEbmFtZQADb3V0AAdwcmludGxu" +
    "AAxwcml2YXRlU2F5SGkAA3J1bgAFc2F5SGkACHRvU3RyaW5nAAV2YWx1ZQAAAgAAAAAABwABAAAA" +
    "CAAAAAEAAAAAAAAABQAHDgAKAgAABw4BIA8BAw8BAw8BBRIBIA8BAQoBAg8ABwIAAAcOAQMPAAAB" +
    "AAEAAQAAAJwDAAAEAAAAcBAEAAAADgAGAAMAAwAAAKEDAABVAAAAYgAAACIBCQBwEAYAAQAbAgMA" +
    "AABuIAgAIQAMAW4gBwBBAAwBGwIAAAAAbiAIACEADAFuEAkAAQAMAW4gAwAQABIQMwQrAHIQBQAF" +
    "ANgABP9wMAEAAwViAAAAIgEJAHAQBgABABsCAgAAAG4gCAAhAAwBbiAHAEEADAEbAgAAAABuIAgA" +
    "IQAMAW4QCQABAAwBbiADABAADgA4BN//2AAE/3AwAQADBSkA2P8AAAMAAwADAAAAvQMAAAQAAABw" +
    "MAEAEAIOAAAAAgEAgIAEyAcBAuAHAgGcCQAAAgMBIBgCAgQCFwQIGRcTAAIAAADIBAAAzgQAANgE" +
    "AAAAAAAAAAAAAAAAAAAQAAAAAAAAAAEAAAAAAAAAAQAAACEAAABwAAAAAgAAAAwAAAD0AAAAAwAA" +
    "AAYAAAAkAQAABAAAAAEAAABsAQAABQAAAAoAAAB0AQAABgAAAAEAAADEAQAAAiAAACEAAADkAQAA" +
    "ARAAAAMAAACEAwAAAyAAAAMAAACcAwAAASAAAAMAAADIAwAAACAAAAEAAAC0BAAABCAAAAIAAADI" +
    "BAAAAxAAAAEAAADYBAAABiAAAAEAAADkBAAAABAAAAEAAAD0BAAA");

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
