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

class FieldChange {
  // The following is a base64 encoding of the following class.
  // class Transform4 {
  //   private Object greeting;
  //   public Transform4(String hi) { }
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFwoABgAQBwARCAASCgACABMHABQHABUBAAhncmVldGluZwEAEkxqYXZhL2xhbmcv" +
    "T2JqZWN0OwEABjxpbml0PgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEABENvZGUBAA9MaW5lTnVt" +
    "YmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAA9UcmFuc2Zvcm00LmphdmEMAAkAFgEAD2ph" +
    "dmEvbGFuZy9FcnJvcgEAFVNob3VsZCBub3QgYmUgY2FsbGVkIQwACQAKAQAKVHJhbnNmb3JtNAEA" +
    "EGphdmEvbGFuZy9PYmplY3QBAAMoKVYAIAAFAAYAAAABAAIABwAIAAAAAgABAAkACgABAAsAAAAd" +
    "AAEAAgAAAAUqtwABsQAAAAEADAAAAAYAAQAAAAMAAQANAAoAAQALAAAAIgADAAIAAAAKuwACWRID" +
    "twAEvwAAAAEADAAAAAYAAQAAAAUAAQAOAAAAAgAP");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQASXs5yszuhud+/w4q07495k9eO7Yb+l8u4AgAAcAAAAHhWNBIAAAAAAAAAABgCAAAM" +
    "AAAAcAAAAAUAAACgAAAAAgAAALQAAAABAAAAzAAAAAQAAADUAAAAAQAAAPQAAACkAQAAFAEAAFYB" +
    "AABeAQAAbAEAAH8BAACTAQAApwEAAL4BAADPAQAA0gEAANYBAADqAQAA9AEAAAEAAAACAAAAAwAA" +
    "AAQAAAAHAAAABwAAAAQAAAAAAAAACAAAAAQAAABQAQAAAAACAAoAAAAAAAEAAAAAAAAAAQALAAAA" +
    "AQABAAAAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAAAcCAAAAAAAAAgACAAEAAAD7" +
    "AQAABAAAAHAQAwAAAA4ABAACAAIAAAABAgAACQAAACIAAQAbAQUAAABwIAIAEAAnAAAAAQAAAAMA" +
    "Bjxpbml0PgAMTFRyYW5zZm9ybTQ7ABFMamF2YS9sYW5nL0Vycm9yOwASTGphdmEvbGFuZy9PYmpl" +
    "Y3Q7ABJMamF2YS9sYW5nL1N0cmluZzsAFVNob3VsZCBub3QgYmUgY2FsbGVkIQAPVHJhbnNmb3Jt" +
    "NC5qYXZhAAFWAAJWTAASZW1pdHRlcjogamFjay00LjIyAAhncmVldGluZwAFc2F5SGkAAwEABw4A" +
    "BQEABw4AAAEBAQACAIGABJQCAQGsAgANAAAAAAAAAAEAAAAAAAAAAQAAAAwAAABwAAAAAgAAAAUA" +
    "AACgAAAAAwAAAAIAAAC0AAAABAAAAAEAAADMAAAABQAAAAQAAADUAAAABgAAAAEAAAD0AAAAASAA" +
    "AAIAAAAUAQAAARAAAAEAAABQAQAAAiAAAAwAAABWAQAAAyAAAAIAAAD7AQAAACAAAAEAAAAHAgAA" +
    "ABAAAAEAAAAYAgAA");

  public static void doTest(Transform4 t) {
    t.sayHi("FieldChange");
    try {
      Main.doCommonClassRedefinition(Transform4.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("FieldChange");
  }
}
