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

class Undefault {
  // The following is a base64 encoding of the following class.
  // class Transform5 implements Iface4 {
  //   public void sayHiTwice(String s) {
  //     throw new Error("Should not be called");
  //   }
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAGgoABwASBwATCAAUCgACABUIABYHABcHABgHABkBAAY8aW5pdD4BAAMoKVYBAARD" +
    "b2RlAQAPTGluZU51bWJlclRhYmxlAQAKc2F5SGlUd2ljZQEAFShMamF2YS9sYW5nL1N0cmluZzsp" +
    "VgEABXNheUhpAQAKU291cmNlRmlsZQEAD1RyYW5zZm9ybTUuamF2YQwACQAKAQAPamF2YS9sYW5n" +
    "L0Vycm9yAQAUU2hvdWxkIG5vdCBiZSBjYWxsZWQMAAkADgEAFVNob3VsZCBub3QgYmUgY2FsbGVk" +
    "IQEAClRyYW5zZm9ybTUBABBqYXZhL2xhbmcvT2JqZWN0AQAGSWZhY2U0ACAABgAHAAEACAAAAAMA" +
    "AAAJAAoAAQALAAAAHQABAAEAAAAFKrcAAbEAAAABAAwAAAAGAAEAAAABAAEADQAOAAEACwAAACIA" +
    "AwACAAAACrsAAlkSA7cABL8AAAABAAwAAAAGAAEAAAADAAEADwAOAAEACwAAACIAAwACAAAACrsA" +
    "AlkSBbcABL8AAAABAAwAAAAGAAEAAAAGAAEAEAAAAAIAEQ==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQD5XbJiwMAcY0cucJ5gcVhFu7tMG0dZX8PsAgAAcAAAAHhWNBIAAAAAAAAAAFgCAAAN" +
    "AAAAcAAAAAYAAACkAAAAAgAAALwAAAAAAAAAAAAAAAUAAADUAAAAAQAAAPwAAADQAQAAHAEAAIIB" +
    "AACKAQAAlAEAAKIBAAC1AQAAyQEAAN0BAADzAQAACgIAABsCAAAeAgAAIgIAACkCAAABAAAAAgAA" +
    "AAMAAAAEAAAABQAAAAkAAAAJAAAABQAAAAAAAAAKAAAABQAAAHwBAAABAAAAAAAAAAEAAQALAAAA" +
    "AQABAAwAAAACAAEAAAAAAAMAAAAAAAAAAQAAAAAAAAADAAAAdAEAAAgAAAAAAAAARgIAAAAAAAAB" +
    "AAEAAQAAADUCAAAEAAAAcBAEAAAADgAEAAIAAgAAADoCAAAIAAAAIgACABoBBwBwIAMAEAAnAAQA" +
    "AgACAAAAQAIAAAgAAAAiAAIAGgEGAHAgAwAQACcAAQAAAAAAAAABAAAABAAGPGluaXQ+AAhMSWZh" +
    "Y2U0OwAMTFRyYW5zZm9ybTU7ABFMamF2YS9sYW5nL0Vycm9yOwASTGphdmEvbGFuZy9PYmplY3Q7" +
    "ABJMamF2YS9sYW5nL1N0cmluZzsAFFNob3VsZCBub3QgYmUgY2FsbGVkABVTaG91bGQgbm90IGJl" +
    "IGNhbGxlZCEAD1RyYW5zZm9ybTUuamF2YQABVgACVkwABXNheUhpAApzYXlIaVR3aWNlAAEABw4A" +
    "BgEABw4AAwEABw4AAAABAgCAgAScAgEBtAIBAdQCDAAAAAAAAAABAAAAAAAAAAEAAAANAAAAcAAA" +
    "AAIAAAAGAAAApAAAAAMAAAACAAAAvAAAAAUAAAAFAAAA1AAAAAYAAAABAAAA/AAAAAEgAAADAAAA" +
    "HAEAAAEQAAACAAAAdAEAAAIgAAANAAAAggEAAAMgAAADAAAANQIAAAAgAAABAAAARgIAAAAQAAAB" +
    "AAAAWAIAAA==");

  public static void doTest(Transform5 t) {
    t.sayHi("Undefault");
    try {
      Main.doCommonClassRedefinition(Transform5.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("Undefault");
  }
}
