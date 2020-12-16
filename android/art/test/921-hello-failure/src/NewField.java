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

class NewField {
  // The following is a base64 encoding of the following class.
  // class Transform {
  //   private Object field;
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFwoABgARBwASCAATCgACABQHABUHABYBAAVmaWVsZAEAEkxqYXZhL2xhbmcvT2Jq" +
    "ZWN0OwEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAFShM" +
    "amF2YS9sYW5nL1N0cmluZzspVgEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwACQAKAQAP" +
    "amF2YS9sYW5nL0Vycm9yAQAVU2hvdWxkIG5vdCBiZSBjYWxsZWQhDAAJAA4BAAlUcmFuc2Zvcm0B" +
    "ABBqYXZhL2xhbmcvT2JqZWN0ACAABQAGAAAAAQACAAcACAAAAAIAAAAJAAoAAQALAAAAHQABAAEA" +
    "AAAFKrcAAbEAAAABAAwAAAAGAAEAAAABAAEADQAOAAEACwAAACIAAwACAAAACrsAAlkSA7cABL8A" +
    "AAABAAwAAAAGAAEAAAAEAAEADwAAAAIAEA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBNWknL2iyjim487p0EIH/8V5OjOeLgw5e0AgAAcAAAAHhWNBIAAAAAAAAAABQCAAAM" +
    "AAAAcAAAAAUAAACgAAAAAgAAALQAAAABAAAAzAAAAAQAAADUAAAAAQAAAPQAAACgAQAAFAEAAFYB" +
    "AABeAQAAawEAAH4BAACSAQAApgEAAL0BAADNAQAA0AEAANQBAADoAQAA7wEAAAEAAAACAAAAAwAA" +
    "AAQAAAAHAAAABwAAAAQAAAAAAAAACAAAAAQAAABQAQAAAAACAAoAAAAAAAAAAAAAAAAAAQALAAAA" +
    "AQABAAAAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAAAECAAAAAAAAAQABAAEAAAD2" +
    "AQAABAAAAHAQAwAAAA4ABAACAAIAAAD7AQAACQAAACIAAQAbAQUAAABwIAIAEAAnAAAAAQAAAAMA" +
    "Bjxpbml0PgALTFRyYW5zZm9ybTsAEUxqYXZhL2xhbmcvRXJyb3I7ABJMamF2YS9sYW5nL09iamVj" +
    "dDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAVU2hvdWxkIG5vdCBiZSBjYWxsZWQhAA5UcmFuc2Zvcm0u" +
    "amF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2stNC4yMgAFZmllbGQABXNheUhpAAEABw4ABAEABw4A" +
    "AAEBAQACAICABJQCAQGsAgAAAA0AAAAAAAAAAQAAAAAAAAABAAAADAAAAHAAAAACAAAABQAAAKAA" +
    "AAADAAAAAgAAALQAAAAEAAAAAQAAAMwAAAAFAAAABAAAANQAAAAGAAAAAQAAAPQAAAABIAAAAgAA" +
    "ABQBAAABEAAAAQAAAFABAAACIAAADAAAAFYBAAADIAAAAgAAAPYBAAAAIAAAAQAAAAECAAAAEAAA" +
    "AQAAABQCAAA=");

  public static void doTest(Transform t) {
    t.sayHi("NewField");
    try {
      Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("NewField");
  }
}
