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

public class Test945 {

  static class Transform {
    // static block to ensure that there is a <clinit> method. This used to be needed due to a bug.
    // Since it's annoying to recompute the transformed bytes we will just leave this here.
    static { }

    public void sayHi(Runnable r) {
      System.out.println("hello");
      doExecute(r);
      System.out.println("goodbye");
    }

    private static native void doExecute(Runnable r);
  }

  // static class Transform {
  //   static { }
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello - Transformed");
  //     doExecute(r);
  //     System.out.println("Goodbye - Transformed");
  //   }
  //
  //   private static native void doExecute(Runnable r);
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAJwoACAATCQAUABUIABYKABcAGAoABwAZCAAaBwAcBwAfAQAGPGluaXQ+AQADKClW" +
    "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
    "KVYBAAlkb0V4ZWN1dGUBAAg8Y2xpbml0PgEAClNvdXJjZUZpbGUBAAxUZXN0OTQ1LmphdmEMAAkA" +
    "CgcAIAwAIQAiAQATSGVsbG8gLSBUcmFuc2Zvcm1lZAcAIwwAJAAlDAAPAA4BABVHb29kYnllIC0g" +
    "VHJhbnNmb3JtZWQHACYBABVhcnQvVGVzdDk0NSRUcmFuc2Zvcm0BAAlUcmFuc2Zvcm0BAAxJbm5l" +
    "ckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxq" +
    "YXZhL2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExq" +
    "YXZhL2xhbmcvU3RyaW5nOylWAQALYXJ0L1Rlc3Q5NDUAIAAHAAgAAAAAAAQAAAAJAAoAAQALAAAA" +
    "HQABAAEAAAAFKrcAAbEAAAABAAwAAAAGAAEAAAAFAAEADQAOAAEACwAAADkAAgACAAAAFbIAAhID" +
    "tgAEK7gABbIAAhIGtgAEsQAAAAEADAAAABIABAAAAAgACAAJAAwACgAUAAsBCgAPAA4AAAAIABAA" +
    "CgABAAsAAAAZAAAAAAAAAAGxAAAAAQAMAAAABgABAAAABgACABEAAAACABIAHgAAAAoAAQAHABsA" +
    "HQAI");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAFqcJFAAAAAAAAAAAAAAAAAAAAAAAAAAB8BAAAcAAAAHhWNBIAAAAAAAAAALgDAAAY" +
    "AAAAcAAAAAoAAADQAAAAAwAAAPgAAAABAAAAHAEAAAYAAAAkAQAAAQAAAFQBAAAIAwAAdAEAAHQB" +
    "AAB+AQAAhgEAAJ0BAACyAQAAywEAANoBAAD+AQAAHgIAADUCAABJAgAAXwIAAHMCAACHAgAAlQIA" +
    "AKACAACjAgAApwIAALQCAAC/AgAAxQIAAMoCAADTAgAA2gIAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
    "CQAAAAoAAAALAAAADAAAAA8AAAAPAAAACQAAAAAAAAAQAAAACQAAAOQCAAAQAAAACQAAAOwCAAAI" +
    "AAQAFAAAAAAAAAAAAAAAAAAAAAEAAAAAAAEAEgAAAAAAAQAWAAAABAACABUAAAAFAAAAAQAAAAAA" +
    "AAAAAAAABQAAAAAAAAANAAAAqAMAAHQDAAAAAAAACDxjbGluaXQ+AAY8aW5pdD4AFUdvb2RieWUg" +
    "LSBUcmFuc2Zvcm1lZAATSGVsbG8gLSBUcmFuc2Zvcm1lZAAXTGFydC9UZXN0OTQ1JFRyYW5zZm9y" +
    "bTsADUxhcnQvVGVzdDk0NTsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxk" +
    "YWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2" +
    "YS9sYW5nL09iamVjdDsAFExqYXZhL2xhbmcvUnVubmFibGU7ABJMamF2YS9sYW5nL1N0cmluZzsA" +
    "EkxqYXZhL2xhbmcvU3lzdGVtOwAMVGVzdDk0NS5qYXZhAAlUcmFuc2Zvcm0AAVYAAlZMAAthY2Nl" +
    "c3NGbGFncwAJZG9FeGVjdXRlAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAAAB" +
    "AAAABgAAAAEAAAAHAAAABQAHDgAFAAcOAAgBAAcOAQgPAQMPAQgPAAAAAAAAAAAAAAAA9AIAAAEA" +
    "AAAOAAAAAQABAAEAAAD5AgAABAAAAHAQBQAAAA4ABAACAAIAAAD+AgAAFAAAAGIAAAAbAQMAAABu" +
    "IAQAEABxEAIAAwBiAAAAGwECAAAAbiAEABAADgAAAAMBAIiABJAGAYCABKQGAYoCAAMBvAYCAgEX" +
    "GAECAwIRBAgTFw4AAgAAAIwDAACSAwAAnAMAAAAAAAAAAAAAAAAAABAAAAAAAAAAAQAAAAAAAAAB" +
    "AAAAGAAAAHAAAAACAAAACgAAANAAAAADAAAAAwAAAPgAAAAEAAAAAQAAABwBAAAFAAAABgAAACQB" +
    "AAAGAAAAAQAAAFQBAAACIAAAGAAAAHQBAAABEAAAAgAAAOQCAAADIAAAAwAAAPQCAAABIAAAAwAA" +
    "ABADAAAAIAAAAQAAAHQDAAAEIAAAAgAAAIwDAAADEAAAAQAAAJwDAAAGIAAAAQAAAKgDAAAAEAAA" +
    "AQAAALgDAAA=");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
    t.sayHi(() -> {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    });
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
  }
}
