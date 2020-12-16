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

class Verification {
  // Jasmin program:
  //
  // .source                  Transform.java
  // .class                   Transform
  // .super                   java/lang/Object
  // .method                  <init>()V
  //    .limit stack          1
  //    .limit locals         1
  //    aload_0
  //    invokespecial         java/lang/Object/<init>()V
  //    return
  // .end method
  // .method                  sayHi(Ljava/lang/String;)V
  //    .limit stack          1
  //    .limit locals         2
  //    aload_1
  //    areturn
  // .end method
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgADAC0ADgoADQAHBwAIAQAQamF2YS9sYW5nL09iamVjdAEAClNvdXJjZUZpbGUBAAY8aW5p" +
    "dD4BAAVzYXlIaQwABQAKAQAJVHJhbnNmb3JtAQAEQ29kZQEAAygpVgEADlRyYW5zZm9ybS5qYXZh" +
    "AQAVKExqYXZhL2xhbmcvU3RyaW5nOylWBwADACAAAgANAAAAAAACAAAABQAKAAEACQAAABEAAQAB" +
    "AAAABSq3AAGxAAAAAAABAAYADAABAAkAAAAOAAEAAgAAAAIrsAAAAAAAAQAEAAAAAgAL");

  // Smali program:
  //
  // .class LTransform;
  // .super Ljava/lang/Object;
  // .source "Transform.java"
  // # direct methods
  // .method constructor <init>()V
  //     .registers 1
  //     invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  //     return-void
  // .end method
  // # virtual methods
  // .method public sayHi(Ljava/lang/String;)V
  //     .registers 2
  //     return-object p1
  // .end method
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQClOAc4ZDMXaHMezhYcqZxcjUeVCWRYUkooAgAAcAAAAHhWNBIAAAAAAAAAAJQBAAAI" +
    "AAAAcAAAAAQAAACQAAAAAgAAAKAAAAAAAAAAAAAAAAMAAAC4AAAAAQAAANAAAAA4AQAA8AAAAPAA" +
    "AAD4AAAABQEAABkBAAAtAQAAPQEAAEABAABEAQAAAQAAAAIAAAADAAAABQAAAAUAAAADAAAAAAAA" +
    "AAYAAAADAAAATAEAAAAAAAAAAAAAAAABAAcAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAEAAAA" +
    "AAAAAIYBAAAAAAAABjxpbml0PgALTFRyYW5zZm9ybTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGph" +
    "dmEvbGFuZy9TdHJpbmc7AA5UcmFuc2Zvcm0uamF2YQABVgACVkwABXNheUhpAAABAAAAAgAAAAAA" +
    "AAAAAAAAAQABAAEAAAAAAAAABAAAAHAQAgAAAA4AAgACAAAAAAAAAAAAAQAAABEBAAABAQCAgATc" +
    "AgEB9AIMAAAAAAAAAAEAAAAAAAAAAQAAAAgAAABwAAAAAgAAAAQAAACQAAAAAwAAAAIAAACgAAAA" +
    "BQAAAAMAAAC4AAAABgAAAAEAAADQAAAAAiAAAAgAAADwAAAAARAAAAEAAABMAQAAAxAAAAIAAABU" +
    "AQAAASAAAAIAAABcAQAAACAAAAEAAACGAQAAABAAAAEAAACUAQAA");

  public static void doTest(Transform t) {
    t.sayHi("Verification");
    try {
      Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    } catch (Exception e) {
      System.out.println(
          "Transformation error : " + e.getClass().getName() + "(" + e.getMessage() + ")");
    }
    t.sayHi("Verification");
  }
}
