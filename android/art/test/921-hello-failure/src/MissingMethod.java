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

class MissingMethod {
  // The following is a base64 encoding of the following class.
  // class Transform3 {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFQoABgAPBwAQCAARCgACABIHABMHABQBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP" +
    "TGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApTb3VyY2VG" +
    "aWxlAQAPVHJhbnNmb3JtMy5qYXZhDAAHAAgBAA9qYXZhL2xhbmcvRXJyb3IBABVTaG91bGQgbm90" +
    "IGJlIGNhbGxlZCEMAAcADAEAClRyYW5zZm9ybTMBABBqYXZhL2xhbmcvT2JqZWN0ACAABQAGAAAA" +
    "AAACAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAAAgABAAsADAABAAkA" +
    "AAAiAAMAAgAAAAq7AAJZEgO3AAS/AAAAAQAKAAAABgABAAAABAABAA0AAAACAA4=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQDnVQvyn7XrwDiCC/SE55zBCtEqk4pzA2mUAgAAcAAAAHhWNBIAAAAAAAAAAAACAAAL" +
    "AAAAcAAAAAUAAACcAAAAAgAAALAAAAAAAAAAAAAAAAQAAADIAAAAAQAAAOgAAACMAQAACAEAAEoB" +
    "AABSAQAAYAEAAHMBAACHAQAAmwEAALIBAADDAQAAxgEAAMoBAADeAQAAAQAAAAIAAAADAAAABAAA" +
    "AAcAAAAHAAAABAAAAAAAAAAIAAAABAAAAEQBAAAAAAAAAAAAAAAAAQAKAAAAAQABAAAAAAACAAAA" +
    "AAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAAPABAAAAAAAAAQABAAEAAADlAQAABAAAAHAQAwAA" +
    "AA4ABAACAAIAAADqAQAACQAAACIAAQAbAQUAAABwIAIAEAAnAAAAAQAAAAMABjxpbml0PgAMTFRy" +
    "YW5zZm9ybTM7ABFMamF2YS9sYW5nL0Vycm9yOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9s" +
    "YW5nL1N0cmluZzsAFVNob3VsZCBub3QgYmUgY2FsbGVkIQAPVHJhbnNmb3JtMy5qYXZhAAFWAAJW" +
    "TAASZW1pdHRlcjogamFjay00LjI0AAVzYXlIaQACAAcOAAQBAAcOAAAAAQEAgIAEiAIBAaACAAAM" +
    "AAAAAAAAAAEAAAAAAAAAAQAAAAsAAABwAAAAAgAAAAUAAACcAAAAAwAAAAIAAACwAAAABQAAAAQA" +
    "AADIAAAABgAAAAEAAADoAAAAASAAAAIAAAAIAQAAARAAAAEAAABEAQAAAiAAAAsAAABKAQAAAyAA" +
    "AAIAAADlAQAAACAAAAEAAADwAQAAABAAAAEAAAAAAgAA");

  public static void doTest(Transform3 t) {
    t.sayHi("MissingMethod");
    try {
      Main.doCommonClassRedefinition(Transform3.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("MissingMethod");
  }
}
