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

class NewMethod {
  // The following is a base64 encoding of the following class.
  // class Transform {
  //   public void extraMethod() {}
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFgoABgAQBwARCAASCgACABMHABQHABUBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP" +
    "TGluZU51bWJlclRhYmxlAQALZXh0cmFNZXRob2QBAAVzYXlIaQEAFShMamF2YS9sYW5nL1N0cmlu" +
    "ZzspVgEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwABwAIAQAPamF2YS9sYW5nL0Vycm9y" +
    "AQAVU2hvdWxkIG5vdCBiZSBjYWxsZWQhDAAHAA0BAAlUcmFuc2Zvcm0BABBqYXZhL2xhbmcvT2Jq" +
    "ZWN0ACAABQAGAAAAAAADAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAA" +
    "AQABAAsACAABAAkAAAAZAAAAAQAAAAGxAAAAAQAKAAAABgABAAAAAgABAAwADQABAAkAAAAiAAMA" +
    "AgAAAAq7AAJZEgO3AAS/AAAAAQAKAAAABgABAAAABAABAA4AAAACAA8=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBeV7dLAwN1GBTa/yRlkuiIQatNHghVdrnIAgAAcAAAAHhWNBIAAAAAAAAAADQCAAAM" +
    "AAAAcAAAAAUAAACgAAAAAgAAALQAAAAAAAAAAAAAAAUAAADMAAAAAQAAAPQAAAC0AQAAFAEAAGoB" +
    "AAByAQAAfwEAAJIBAACmAQAAugEAANEBAADhAQAA5AEAAOgBAAD8AQAACQIAAAEAAAACAAAAAwAA" +
    "AAQAAAAHAAAABwAAAAQAAAAAAAAACAAAAAQAAABkAQAAAAAAAAAAAAAAAAAACgAAAAAAAQALAAAA" +
    "AQABAAAAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAACACAAAAAAAAAQABAAEAAAAQ" +
    "AgAABAAAAHAQBAAAAA4AAQABAAAAAAAVAgAAAQAAAA4AAAAEAAIAAgAAABoCAAAJAAAAIgABABsB" +
    "BQAAAHAgAwAQACcAAAABAAAAAwAGPGluaXQ+AAtMVHJhbnNmb3JtOwARTGphdmEvbGFuZy9FcnJv" +
    "cjsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABVTaG91bGQgbm90IGJl" +
    "IGNhbGxlZCEADlRyYW5zZm9ybS5qYXZhAAFWAAJWTAASZW1pdHRlcjogamFjay00LjIyAAtleHRy" +
    "YU1ldGhvZAAFc2F5SGkAAQAHDgACAAcOAAQBAAcOAAAAAQIAgIAElAIBAawCAQHAAgAADAAAAAAA" +
    "AAABAAAAAAAAAAEAAAAMAAAAcAAAAAIAAAAFAAAAoAAAAAMAAAACAAAAtAAAAAUAAAAFAAAAzAAA" +
    "AAYAAAABAAAA9AAAAAEgAAADAAAAFAEAAAEQAAABAAAAZAEAAAIgAAAMAAAAagEAAAMgAAADAAAA" +
    "EAIAAAAgAAABAAAAIAIAAAAQAAABAAAANAIAAA==");

  public static void doTest(Transform t) {
    t.sayHi("NewMethod");
    try {
      Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("NewMethod");
  }
}
