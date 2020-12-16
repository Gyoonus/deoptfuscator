/*
 * Copyright (C) 2016 The Android Open Source Project
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

package art;

import java.util.Base64;
public class Test932 {

  // This class is never used so just have it print out a bogus value so we can detect if something
  // goes very wrong.
  static class Transform {
    public void sayHi() {
      System.out.println("foobar");
    }
  }

  /**
   * base64 encoded class/dex file for
   * static class Transform {
   *   public void sayHi() {
   *    System.out.println("hello");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES_A = Base64.getDecoder().decode(
    "yv66vgAAADQAIAoABgAOCQAPABAIABEKABIAEwcAFQcAGAEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAAxUZXN0OTMyLmphdmEMAAcA" +
    "CAcAGQwAGgAbAQAFaGVsbG8HABwMAB0AHgcAHwEAFWFydC9UZXN0OTMyJFRyYW5zZm9ybQEACVRy" +
    "YW5zZm9ybQEADElubmVyQ2xhc3NlcwEAEGphdmEvbGFuZy9PYmplY3QBABBqYXZhL2xhbmcvU3lz" +
    "dGVtAQADb3V0AQAVTGphdmEvaW8vUHJpbnRTdHJlYW07AQATamF2YS9pby9QcmludFN0cmVhbQEA" +
    "B3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAAthcnQvVGVzdDkzMgAgAAUABgAAAAAA" +
    "AgAAAAcACAABAAkAAAAdAAEAAQAAAAUqtwABsQAAAAEACgAAAAYAAQAAAAUAAQALAAgAAQAJAAAA" +
    "JQACAAEAAAAJsgACEgO2AASxAAAAAQAKAAAACgACAAAABwAIAAgAAgAMAAAAAgANABcAAAAKAAEA" +
    "BQAUABYACA==");
  private static final byte[] DEX_BYTES_A = Base64.getDecoder().decode(
    "ZGV4CjAzNQAngjnzAAAAAAAAAAAAAAAAAAAAAAAAAAC4AwAAcAAAAHhWNBIAAAAAAAAAAPQCAAAU" +
    "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB0AgAARAEAAEQB" +
    "AABMAQAAZQEAAHQBAACYAQAAuAEAAM8BAADjAQAA9wEAAAsCAAAZAgAAJAIAACcCAAArAgAAOAIA" +
    "AD8CAABFAgAASgIAAFMCAABaAgAAAQAAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAALAAAA" +
    "CwAAAAgAAAAAAAAADAAAAAgAAABkAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
    "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAJAAAA5AIAALgCAAAAAAAABjxpbml0PgAXTGFydC9UZXN0" +
    "OTMyJFRyYW5zZm9ybTsADUxhcnQvVGVzdDkzMjsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3Np" +
    "bmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRT" +
    "dHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFu" +
    "Zy9TeXN0ZW07AAxUZXN0OTMyLmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAAVo" +
    "ZWxsbwAEbmFtZQADb3V0AAdwcmludGxuAAVzYXlIaQAFdmFsdWUAAAAAAQAAAAYAAAAFAAcOAAcA" +
    "Bw4BCA8AAAAAAQABAAEAAABsAgAABAAAAHAQAwAAAA4AAwABAAIAAABxAgAACQAAAGIAAAAbAQ4A" +
    "AABuIAIAEAAOAAAAAAABAQCAgAT8BAEBlAUAAAICARMYAQIDAg0ECA8XCgACAAAAyAIAAM4CAADY" +
    "AgAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAUAAAAcAAAAAIAAAAJAAAAwAAAAAMA" +
    "AAACAAAA5AAAAAQAAAABAAAA/AAAAAUAAAAEAAAABAEAAAYAAAABAAAAJAEAAAIgAAAUAAAARAEA" +
    "AAEQAAABAAAAZAIAAAMgAAACAAAAbAIAAAEgAAACAAAAfAIAAAAgAAABAAAAuAIAAAQgAAACAAAA" +
    "yAIAAAMQAAABAAAA2AIAAAYgAAABAAAA5AIAAAAQAAABAAAA9AIAAA==");

  /**
   * base64 encoded class/dex file for
   * static class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES_B = Base64.getDecoder().decode(
    "yv66vgAAADQAIAoABgAOCQAPABAIABEKABIAEwcAFQcAGAEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAAxUZXN0OTMyLmphdmEMAAcA" +
    "CAcAGQwAGgAbAQAHR29vZGJ5ZQcAHAwAHQAeBwAfAQAVYXJ0L1Rlc3Q5MzIkVHJhbnNmb3JtAQAJ" +
    "VHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEvbGFuZy9T" +
    "eXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50U3RyZWFt" +
    "AQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEAC2FydC9UZXN0OTMyACAABQAGAAAA" +
    "AAACAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAABQABAAsACAABAAkA" +
    "AAAlAAIAAQAAAAmyAAISA7YABLEAAAABAAoAAAAKAAIAAAAHAAgACAACAAwAAAACAA0AFwAAAAoA" +
    "AQAFABQAFgAI");
  private static final byte[] DEX_BYTES_B = Base64.getDecoder().decode(
    "ZGV4CjAzNQByglN3AAAAAAAAAAAAAAAAAAAAAAAAAAC4AwAAcAAAAHhWNBIAAAAAAAAAAPQCAAAU" +
    "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB0AgAARAEAAEQB" +
    "AABMAQAAVQEAAG4BAAB9AQAAoQEAAMEBAADYAQAA7AEAAAACAAAUAgAAIgIAAC0CAAAwAgAANAIA" +
    "AEECAABHAgAATAIAAFUCAABcAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
    "DAAAAAgAAAAAAAAADQAAAAgAAABkAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
    "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAA5AIAALgCAAAAAAAABjxpbml0PgAHR29vZGJ5ZQAX" +
    "TGFydC9UZXN0OTMyJFRyYW5zZm9ybTsADUxhcnQvVGVzdDkzMjsAIkxkYWx2aWsvYW5ub3RhdGlv" +
    "bi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEv" +
    "aW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAS" +
    "TGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0OTMyLmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vz" +
    "c0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAQAAAAYAAAAFAAcOAAcA" +
    "Bw4BCA8AAAAAAQABAAEAAABsAgAABAAAAHAQAwAAAA4AAwABAAIAAABxAgAACQAAAGIAAAAbAQEA" +
    "AABuIAIAEAAOAAAAAAABAQCAgAT8BAEBlAUAAAICARMYAQIDAg4ECA8XCwACAAAAyAIAAM4CAADY" +
    "AgAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAUAAAAcAAAAAIAAAAJAAAAwAAAAAMA" +
    "AAACAAAA5AAAAAQAAAABAAAA/AAAAAUAAAAEAAAABAEAAAYAAAABAAAAJAEAAAIgAAAUAAAARAEA" +
    "AAEQAAABAAAAZAIAAAMgAAACAAAAbAIAAAEgAAACAAAAfAIAAAAgAAABAAAAuAIAAAQgAAACAAAA" +
    "yAIAAAMQAAABAAAA2AIAAAYgAAABAAAA5AIAAAAQAAABAAAA9AIAAA==");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_RETRANSFORM);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    // TODO We currently need to do this transform call since we don't have any way to make the
    // original-dex-file a single-class dex-file letting us restore it easily. We should use the
    // manipulation library that is being made when we store the original dex file.
    // TODO REMOVE this theoretically does nothing but it ensures the original-dex-file we have set
    // is one we can return to unaltered.
    Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES_A, DEX_BYTES_A);
    t.sayHi();

    // Now turn it into DEX_BYTES_B so it says 'Goodbye'
    Redefinition.addCommonTransformationResult("art/Test932$Transform", CLASS_BYTES_B, DEX_BYTES_B);
    Redefinition.enableCommonRetransformation(true);
    Redefinition.doCommonClassRetransformation(Transform.class);
    t.sayHi();

    // Now turn it back to normal by removing the load-hook and transforming again.
    Redefinition.enableCommonRetransformation(false);
    Redefinition.doCommonClassRetransformation(Transform.class);
    t.sayHi();
  }
}
