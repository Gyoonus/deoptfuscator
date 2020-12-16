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

class MissingField {
  // The following is a base64 encoding of the following class.
  // class Transform4 {
  //   public Transform4(String s) { }
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFQoABgAOBwAPCAAQCgACABEHABIHABMBAAY8aW5pdD4BABUoTGphdmEvbGFuZy9T" +
    "dHJpbmc7KVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5SGkBAApTb3VyY2VGaWxlAQAP" +
    "VHJhbnNmb3JtNC5qYXZhDAAHABQBAA9qYXZhL2xhbmcvRXJyb3IBABVTaG91bGQgbm90IGJlIGNh" +
    "bGxlZCEMAAcACAEAClRyYW5zZm9ybTQBABBqYXZhL2xhbmcvT2JqZWN0AQADKClWACAABQAGAAAA" +
    "AAACAAEABwAIAAEACQAAAB0AAQACAAAABSq3AAGxAAAAAQAKAAAABgABAAAAAgABAAsACAABAAkA" +
    "AAAiAAMAAgAAAAq7AAJZEgO3AAS/AAAAAQAKAAAABgABAAAABAABAAwAAAACAA0=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDBVUVrMUEFx3lYkgJF54evq9vHvOUDZveUAgAAcAAAAHhWNBIAAAAAAAAAAAACAAAL" +
    "AAAAcAAAAAUAAACcAAAAAgAAALAAAAAAAAAAAAAAAAQAAADIAAAAAQAAAOgAAACMAQAACAEAAEoB" +
    "AABSAQAAYAEAAHMBAACHAQAAmwEAALIBAADDAQAAxgEAAMoBAADeAQAAAQAAAAIAAAADAAAABAAA" +
    "AAcAAAAHAAAABAAAAAAAAAAIAAAABAAAAEQBAAAAAAEAAAAAAAAAAQAKAAAAAQABAAAAAAACAAAA" +
    "AAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAAPEBAAAAAAAAAgACAAEAAADlAQAABAAAAHAQAwAA" +
    "AA4ABAACAAIAAADrAQAACQAAACIAAQAbAQUAAABwIAIAEAAnAAAAAQAAAAMABjxpbml0PgAMTFRy" +
    "YW5zZm9ybTQ7ABFMamF2YS9sYW5nL0Vycm9yOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9s" +
    "YW5nL1N0cmluZzsAFVNob3VsZCBub3QgYmUgY2FsbGVkIQAPVHJhbnNmb3JtNC5qYXZhAAFWAAJW" +
    "TAASZW1pdHRlcjogamFjay00LjIyAAVzYXlIaQACAQAHDgAEAQAHDgAAAAEBAIGABIgCAQGgAgAM" +
    "AAAAAAAAAAEAAAAAAAAAAQAAAAsAAABwAAAAAgAAAAUAAACcAAAAAwAAAAIAAACwAAAABQAAAAQA" +
    "AADIAAAABgAAAAEAAADoAAAAASAAAAIAAAAIAQAAARAAAAEAAABEAQAAAiAAAAsAAABKAQAAAyAA" +
    "AAIAAADlAQAAACAAAAEAAADxAQAAABAAAAEAAAAAAgAA");

  public static void doTest(Transform4 t) {
    t.sayHi("MissingField");
    try {
      Main.doCommonClassRedefinition(Transform4.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("MissingField");
  }
}
