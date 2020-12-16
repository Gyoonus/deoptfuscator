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

class MultiRedef {

  // class NotTransform {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static CommonClassDefinition INVALID_DEFINITION_T1 = new CommonClassDefinition(
      Transform.class,
      Base64.getDecoder().decode(
          "yv66vgAAADQAFQoABgAPBwAQCAARCgACABIHABMHABQBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP" +
          "TGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApTb3VyY2VG" +
          "aWxlAQARTm90VHJhbnNmb3JtLmphdmEMAAcACAEAD2phdmEvbGFuZy9FcnJvcgEAFVNob3VsZCBu" +
          "b3QgYmUgY2FsbGVkIQwABwAMAQAMTm90VHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAAgAAUA" +
          "BgAAAAAAAgAAAAcACAABAAkAAAAdAAEAAQAAAAUqtwABsQAAAAEACgAAAAYAAQAAAAEAAQALAAwA" +
          "AQAJAAAAIgADAAIAAAAKuwACWRIDtwAEvwAAAAEACgAAAAYAAQAAAAMAAQANAAAAAgAO"),
      Base64.getDecoder().decode(
          "ZGV4CjAzNQDLV95i5xnv6iUi6uIeDoY5jP5Xe9NP1AiYAgAAcAAAAHhWNBIAAAAAAAAAAAQCAAAL" +
          "AAAAcAAAAAUAAACcAAAAAgAAALAAAAAAAAAAAAAAAAQAAADIAAAAAQAAAOgAAACQAQAACAEAAEoB" +
          "AABSAQAAYgEAAHUBAACJAQAAnQEAALABAADHAQAAygEAAM4BAADiAQAAAQAAAAIAAAADAAAABAAA" +
          "AAcAAAAHAAAABAAAAAAAAAAIAAAABAAAAEQBAAAAAAAAAAAAAAAAAQAKAAAAAQABAAAAAAACAAAA" +
          "AAAAAAAAAAAAAAAAAgAAAAAAAAAFAAAAAAAAAPQBAAAAAAAAAQABAAEAAADpAQAABAAAAHAQAwAA" +
          "AA4ABAACAAIAAADuAQAACQAAACIAAQAbAQYAAABwIAIAEAAnAAAAAQAAAAMABjxpbml0PgAOTE5v" +
          "dFRyYW5zZm9ybTsAEUxqYXZhL2xhbmcvRXJyb3I7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZh" +
          "L2xhbmcvU3RyaW5nOwARTm90VHJhbnNmb3JtLmphdmEAFVNob3VsZCBub3QgYmUgY2FsbGVkIQAB" +
          "VgACVkwAEmVtaXR0ZXI6IGphY2stNC4yMAAFc2F5SGkAAQAHDgADAQAHDgAAAAEBAICABIgCAQGg" +
          "AgAADAAAAAAAAAABAAAAAAAAAAEAAAALAAAAcAAAAAIAAAAFAAAAnAAAAAMAAAACAAAAsAAAAAUA" +
          "AAAEAAAAyAAAAAYAAAABAAAA6AAAAAEgAAACAAAACAEAAAEQAAABAAAARAEAAAIgAAALAAAASgEA" +
          "AAMgAAACAAAA6QEAAAAgAAABAAAA9AEAAAAQAAABAAAABAIAAA=="));

  // Valid redefinition of Transform2
  // class Transform2 implements Iface1, Iface2 {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static CommonClassDefinition VALID_DEFINITION_T2 = new CommonClassDefinition(
      Transform2.class,
      Base64.getDecoder().decode(
          "yv66vgAAADQAGQoABgARBwASCAATCgACABQHABUHABYHABcHABgBAAY8aW5pdD4BAAMoKVYBAARD" +
          "b2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApT" +
          "b3VyY2VGaWxlAQAPVHJhbnNmb3JtMi5qYXZhDAAJAAoBAA9qYXZhL2xhbmcvRXJyb3IBABVTaG91" +
          "bGQgbm90IGJlIGNhbGxlZCEMAAkADgEAClRyYW5zZm9ybTIBABBqYXZhL2xhbmcvT2JqZWN0AQAG" +
          "SWZhY2UxAQAGSWZhY2UyACAABQAGAAIABwAIAAAAAgAAAAkACgABAAsAAAAdAAEAAQAAAAUqtwAB" +
          "sQAAAAEADAAAAAYAAQAAAAEAAQANAA4AAQALAAAAIgADAAIAAAAKuwACWRIDtwAEvwAAAAEADAAA" +
          "AAYAAQAAAAMAAQAPAAAAAgAQ"),
      Base64.getDecoder().decode(
          "ZGV4CjAzNQDSWls05CPkX+gbTGMVRvx9dc9vozzVbu7AAgAAcAAAAHhWNBIAAAAAAAAAACwCAAAN" +
          "AAAAcAAAAAcAAACkAAAAAgAAAMAAAAAAAAAAAAAAAAQAAADYAAAAAQAAAPgAAACoAQAAGAEAAGIB" +
          "AABqAQAAdAEAAH4BAACMAQAAnwEAALMBAADHAQAA3gEAAO8BAADyAQAA9gEAAAoCAAABAAAAAgAA" +
          "AAMAAAAEAAAABQAAAAYAAAAJAAAACQAAAAYAAAAAAAAACgAAAAYAAABcAQAAAgAAAAAAAAACAAEA" +
          "DAAAAAMAAQAAAAAABAAAAAAAAAACAAAAAAAAAAQAAABUAQAACAAAAAAAAAAcAgAAAAAAAAEAAQAB" +
          "AAAAEQIAAAQAAABwEAMAAAAOAAQAAgACAAAAFgIAAAkAAAAiAAMAGwEHAAAAcCACABAAJwAAAAIA" +
          "AAAAAAEAAQAAAAUABjxpbml0PgAITElmYWNlMTsACExJZmFjZTI7AAxMVHJhbnNmb3JtMjsAEUxq" +
          "YXZhL2xhbmcvRXJyb3I7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAV" +
          "U2hvdWxkIG5vdCBiZSBjYWxsZWQhAA9UcmFuc2Zvcm0yLmphdmEAAVYAAlZMABJlbWl0dGVyOiBq" +
          "YWNrLTQuMjAABXNheUhpAAEABw4AAwEABw4AAAABAQCAgASYAgEBsAIAAAwAAAAAAAAAAQAAAAAA" +
          "AAABAAAADQAAAHAAAAACAAAABwAAAKQAAAADAAAAAgAAAMAAAAAFAAAABAAAANgAAAAGAAAAAQAA" +
          "APgAAAABIAAAAgAAABgBAAABEAAAAgAAAFQBAAACIAAADQAAAGIBAAADIAAAAgAAABECAAAAIAAA" +
          "AQAAABwCAAAAEAAAAQAAACwCAAA="));

  public static void doTest(Transform t1, Transform2 t2) {
    t1.sayHi("MultiRedef");
    t2.sayHi("MultiRedef");
    try {
      Main.doMultiClassRedefinition(VALID_DEFINITION_T2, INVALID_DEFINITION_T1);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t1.sayHi("MultiRedef");
    t2.sayHi("MultiRedef");
    try {
      Main.doMultiClassRedefinition(INVALID_DEFINITION_T1, VALID_DEFINITION_T2);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t1.sayHi("MultiRedef");
    t2.sayHi("MultiRedef");
  }
}
