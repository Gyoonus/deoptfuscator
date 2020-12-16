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

class NewInterface {
  // The following is a base64 encoding of the following class.
  // class Transform2 implements Iface1, Iface2, Iface3 {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAGwoABgASBwATCAAUCgACABUHABYHABcHABgHABkHABoBAAY8aW5pdD4BAAMoKVYB" +
    "AARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYB" +
    "AApTb3VyY2VGaWxlAQAPVHJhbnNmb3JtMi5qYXZhDAAKAAsBAA9qYXZhL2xhbmcvRXJyb3IBABVT" +
    "aG91bGQgbm90IGJlIGNhbGxlZCEMAAoADwEAClRyYW5zZm9ybTIBABBqYXZhL2xhbmcvT2JqZWN0" +
    "AQAGSWZhY2UxAQAGSWZhY2UyAQAGSWZhY2UzACAABQAGAAMABwAIAAkAAAACAAAACgALAAEADAAA" +
    "AB0AAQABAAAABSq3AAGxAAAAAQANAAAABgABAAAAAQABAA4ADwABAAwAAAAiAAMAAgAAAAq7AAJZ" +
    "EgO3AAS/AAAAAQANAAAABgABAAAAAwABABAAAAACABE=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCBWnko4SMXeuXSO3fGJBp0WSlc0HLRr63UAgAAcAAAAHhWNBIAAAAAAAAAAEACAAAO" +
    "AAAAcAAAAAgAAACoAAAAAgAAAMgAAAAAAAAAAAAAAAQAAADgAAAAAQAAAAABAAC0AQAAIAEAAG4B" +
    "AAB2AQAAgAEAAIoBAACUAQAAogEAALUBAADJAQAA3QEAAPQBAAAFAgAACAIAAAwCAAAgAgAAAQAA" +
    "AAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAoAAAAKAAAABwAAAAAAAAALAAAABwAAAGgBAAADAAAA" +
    "AAAAAAMAAQANAAAABAABAAAAAAAFAAAAAAAAAAMAAAAAAAAABQAAAFwBAAAJAAAAAAAAADICAAAA" +
    "AAAAAQABAAEAAAAnAgAABAAAAHAQAwAAAA4ABAACAAIAAAAsAgAACQAAACIABAAbAQgAAABwIAIA" +
    "EAAnAAAAAwAAAAAAAQACAAAAAQAAAAYABjxpbml0PgAITElmYWNlMTsACExJZmFjZTI7AAhMSWZh" +
    "Y2UzOwAMTFRyYW5zZm9ybTI7ABFMamF2YS9sYW5nL0Vycm9yOwASTGphdmEvbGFuZy9PYmplY3Q7" +
    "ABJMamF2YS9sYW5nL1N0cmluZzsAFVNob3VsZCBub3QgYmUgY2FsbGVkIQAPVHJhbnNmb3JtMi5q" +
    "YXZhAAFWAAJWTAASZW1pdHRlcjogamFjay00LjIwAAVzYXlIaQABAAcOAAMBAAcOAAAAAQEAgIAE" +
    "oAIBAbgCDAAAAAAAAAABAAAAAAAAAAEAAAAOAAAAcAAAAAIAAAAIAAAAqAAAAAMAAAACAAAAyAAA" +
    "AAUAAAAEAAAA4AAAAAYAAAABAAAAAAEAAAEgAAACAAAAIAEAAAEQAAACAAAAXAEAAAIgAAAOAAAA" +
    "bgEAAAMgAAACAAAAJwIAAAAgAAABAAAAMgIAAAAQAAABAAAAQAIAAA==");

  public static void doTest(Transform2 t) {
    t.sayHi("NewInterface");
    try {
      Main.doCommonClassRedefinition(Transform2.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("NewInterface");
  }
}
