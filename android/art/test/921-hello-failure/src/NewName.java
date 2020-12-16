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

class NewName {
  // class NotTransform {
  //   public void sayHi(String name) {
  //     throw new Error("Should not be called!");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAFQoABgAPBwAQCAARCgACABIHABMHABQBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP" +
    "TGluZU51bWJlclRhYmxlAQAFc2F5SGkBABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAApTb3VyY2VG" +
    "aWxlAQARTm90VHJhbnNmb3JtLmphdmEMAAcACAEAD2phdmEvbGFuZy9FcnJvcgEAFVNob3VsZCBu" +
    "b3QgYmUgY2FsbGVkIQwABwAMAQAMTm90VHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAAgAAUA" +
    "BgAAAAAAAgAAAAcACAABAAkAAAAdAAEAAQAAAAUqtwABsQAAAAEACgAAAAYAAQAAAAEAAQALAAwA" +
    "AQAJAAAAIgADAAIAAAAKuwACWRIDtwAEvwAAAAEACgAAAAYAAQAAAAMAAQANAAAAAgAO");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
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
    "AAMgAAACAAAA6QEAAAAgAAABAAAA9AEAAAAQAAABAAAABAIAAA==");

  public static void doTest(Transform t) {
    t.sayHi("NewName");
    try {
      Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("NewName");
  }
}
