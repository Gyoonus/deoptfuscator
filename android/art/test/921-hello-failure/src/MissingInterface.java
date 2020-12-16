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

import java.util.Base64;

class MissingInterface {
  // The following is a base64 encoding of the following class.
  // class Transform2 implements Iface1 {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADQAFwoABgAQBwARCAASCgACABMHABQHABUHABYBAAY8aW5pdD4BAAMoKVYBAARDb2Rl" +
      "AQAPTGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApTb3Vy" +
      "Y2VGaWxlAQAPVHJhbnNmb3JtMi5qYXZhDAAIAAkBAA9qYXZhL2xhbmcvRXJyb3IBABVTaG91bGQg" +
      "bm90IGJlIGNhbGxlZCEMAAgADQEAClRyYW5zZm9ybTIBABBqYXZhL2xhbmcvT2JqZWN0AQAGSWZh" +
      "Y2UxACAABQAGAAEABwAAAAIAAAAIAAkAAQAKAAAAHQABAAEAAAAFKrcAAbEAAAABAAsAAAAGAAEA" +
      "AAABAAEADAANAAEACgAAACIAAwACAAAACrsAAlkSA7cABL8AAAABAAsAAAAGAAEAAAADAAEADgAA" +
      "AAIADw==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQDiWVay8/Z0/tXQaTTI+QtwTM65gRJVMOusAgAAcAAAAHhWNBIAAAAAAAAAABgCAAAM" +
      "AAAAcAAAAAYAAACgAAAAAgAAALgAAAAAAAAAAAAAAAQAAADQAAAAAQAAAPAAAACcAQAAEAEAAFoB" +
      "AABiAQAAbAEAAHoBAACNAQAAoQEAALUBAADMAQAA3QEAAOABAADkAQAA+AEAAAEAAAACAAAAAwAA" +
      "AAQAAAAFAAAACAAAAAgAAAAFAAAAAAAAAAkAAAAFAAAAVAEAAAEAAAAAAAAAAQABAAsAAAACAAEA" +
      "AAAAAAMAAAAAAAAAAQAAAAAAAAADAAAATAEAAAcAAAAAAAAACgIAAAAAAAABAAEAAQAAAP8BAAAE" +
      "AAAAcBADAAAADgAEAAIAAgAAAAQCAAAJAAAAIgACABsBBgAAAHAgAgAQACcAAAABAAAAAAAAAAEA" +
      "AAAEAAY8aW5pdD4ACExJZmFjZTE7AAxMVHJhbnNmb3JtMjsAEUxqYXZhL2xhbmcvRXJyb3I7ABJM" +
      "amF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAVU2hvdWxkIG5vdCBiZSBjYWxs" +
      "ZWQhAA9UcmFuc2Zvcm0yLmphdmEAAVYAAlZMABJlbWl0dGVyOiBqYWNrLTQuMjAABXNheUhpAAEA" +
      "Bw4AAwEABw4AAAABAQCAgASQAgEBqAIMAAAAAAAAAAEAAAAAAAAAAQAAAAwAAABwAAAAAgAAAAYA" +
      "AACgAAAAAwAAAAIAAAC4AAAABQAAAAQAAADQAAAABgAAAAEAAADwAAAAASAAAAIAAAAQAQAAARAA" +
      "AAIAAABMAQAAAiAAAAwAAABaAQAAAyAAAAIAAAD/AQAAACAAAAEAAAAKAgAAABAAAAEAAAAYAgAA");

  public static void doTest(Transform2 t) {
    t.sayHi("MissingInterface");
    try {
      Main.doCommonClassRedefinition(Transform2.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("MissingInterface");
  }
}
