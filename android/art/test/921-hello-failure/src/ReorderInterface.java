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

class ReorderInterface {
  // The following is a base64 encoding of the following class.
  // class Transform2 implements Iface2, Iface1 {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAGQoABgARBwASCAATCgACABQHABUHABYHABcHABgBAAY8aW5pdD4BAAMoKVYBAARD" +
    "b2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApT" +
    "b3VyY2VGaWxlAQAPVHJhbnNmb3JtMi5qYXZhDAAJAAoBAA9qYXZhL2xhbmcvRXJyb3IBABVTaG91" +
    "bGQgbm90IGJlIGNhbGxlZCEMAAkADgEAClRyYW5zZm9ybTIBABBqYXZhL2xhbmcvT2JqZWN0AQAG" +
    "SWZhY2UyAQAGSWZhY2UxACAABQAGAAIABwAIAAAAAgAAAAkACgABAAsAAAAdAAEAAQAAAAUqtwAB" +
    "sQAAAAEADAAAAAYAAQAAAAEAAQANAA4AAQALAAAAIgADAAIAAAAKuwACWRIDtwAEvwAAAAEADAAA" +
    "AAYAAQAAAAMAAQAPAAAAAgAQ");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQChWfUC02YEHJZLC4V4pHrGMdqwD8NnzXvAAgAAcAAAAHhWNBIAAAAAAAAAACwCAAAN" +
    "AAAAcAAAAAcAAACkAAAAAgAAAMAAAAAAAAAAAAAAAAQAAADYAAAAAQAAAPgAAACoAQAAGAEAAGIB" +
    "AABqAQAAdAEAAH4BAACMAQAAnwEAALMBAADHAQAA3gEAAO8BAADyAQAA9gEAAAoCAAABAAAAAgAA" +
    "AAMAAAAEAAAABQAAAAYAAAAJAAAACQAAAAYAAAAAAAAACgAAAAYAAABcAQAAAgAAAAAAAAACAAEA" +
    "DAAAAAMAAQAAAAAABAAAAAAAAAACAAAAAAAAAAQAAABUAQAACAAAAAAAAAAcAgAAAAAAAAEAAQAB" +
    "AAAAEQIAAAQAAABwEAMAAAAOAAQAAgACAAAAFgIAAAkAAAAiAAMAGwEHAAAAcCACABAAJwAAAAIA" +
    "AAABAAAAAQAAAAUABjxpbml0PgAITElmYWNlMTsACExJZmFjZTI7AAxMVHJhbnNmb3JtMjsAEUxq" +
    "YXZhL2xhbmcvRXJyb3I7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAV" +
    "U2hvdWxkIG5vdCBiZSBjYWxsZWQhAA9UcmFuc2Zvcm0yLmphdmEAAVYAAlZMABJlbWl0dGVyOiBq" +
    "YWNrLTQuMjAABXNheUhpAAEABw4AAwEABw4AAAABAQCAgASYAgEBsAIAAAwAAAAAAAAAAQAAAAAA" +
    "AAABAAAADQAAAHAAAAACAAAABwAAAKQAAAADAAAAAgAAAMAAAAAFAAAABAAAANgAAAAGAAAAAQAA" +
    "APgAAAABIAAAAgAAABgBAAABEAAAAgAAAFQBAAACIAAADQAAAGIBAAADIAAAAgAAABECAAAAIAAA" +
    "AQAAABwCAAAAEAAAAQAAACwCAAA=");

  public static void doTest(Transform2 t) {
    t.sayHi("ReorderInterface");
    try {
      Main.doCommonClassRedefinition(Transform2.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("ReorderInterface");
  }
}
