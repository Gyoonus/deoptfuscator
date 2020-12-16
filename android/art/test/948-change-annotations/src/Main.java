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

import art.Redefinition;
import java.util.Arrays;
import java.util.Base64;
import java.util.Comparator;
import java.lang.reflect.*;
import java.lang.annotation.*;
public class Main {

  /**
   * base64 encoded class/dex file for for initial Transform.java
   */
  private static final byte[] INITIAL_CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAJAoABgAXCQAYABkIABYKABoAGwcAHAcAHQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBABJMb2NhbFZhcmlhYmxlVGFibGUBAAR0aGlzAQALTFRyYW5zZm9y" +
    "bTsBAAVzYXlIaQEAGVJ1bnRpbWVWaXNpYmxlQW5ub3RhdGlvbnMBABdMVGVzdE1ldGhvZEFubm90" +
    "YXRpb24xOwEABXZhbHVlAQAFaGkgaGkBAApTb3VyY2VGaWxlAQAOVHJhbnNmb3JtLmphdmEBABZM" +
    "VGVzdENsYXNzQW5ub3RhdGlvbjE7AQAFaGVsbG8MAAcACAcAHgwAHwAgBwAhDAAiACMBAAlUcmFu" +
    "c2Zvcm0BABBqYXZhL2xhbmcvT2JqZWN0AQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxqYXZh" +
    "L2lvL1ByaW50U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExqYXZh" +
    "L2xhbmcvU3RyaW5nOylWACAABQAGAAAAAAACAAAABwAIAAEACQAAAC8AAQABAAAABSq3AAGxAAAA" +
    "AgAKAAAABgABAAAAEgALAAAADAABAAAABQAMAA0AAAABAA4ACAACAAkAAAA3AAIAAQAAAAmyAAIS" +
    "A7YABLEAAAACAAoAAAAKAAIAAAAVAAgAFgALAAAADAABAAAACQAMAA0AAAAPAAAACwABABAAAQAR" +
    "cwASAAIAEwAAAAIAFAAPAAAACwABABUAAQARcwAW");
  private static final byte[] INITIAL_DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCufKz9atC18kWgSsEfRq699UEcX4cHonN8AwAAcAAAAHhWNBIAAAAAAAAAALgCAAAS" +
    "AAAAcAAAAAgAAAC4AAAAAgAAANgAAAABAAAA8AAAAAQAAAD4AAAAAQAAABgBAABEAgAAOAEAAKIB" +
    "AACqAQAAwgEAANsBAADoAQAA/wEAABMCAAAnAgAAOwIAAEsCAABOAgAAUgIAAGYCAABtAgAAdAIA" +
    "AHkCAACCAgAAiQIAAAEAAAACAAAAAwAAAAQAAAAFAAAABgAAAAcAAAAJAAAACQAAAAcAAAAAAAAA" +
    "CgAAAAcAAACcAQAABgADAA4AAAACAAAAAAAAAAIAAAAQAAAAAwABAA8AAAAEAAAAAAAAAAIAAAAA" +
    "AAAABAAAAAAAAAAIAAAAhAEAAKcCAAAAAAAAAQAAAJsCAAABAAAAoQIAAAEAAQABAAAAkAIAAAQA" +
    "AABwEAMAAAAOAAMAAQACAAAAlQIAAAkAAABiAAAAGwEMAAAAbiACABAADgAAADgBAAAAAAAAAQAA" +
    "AAAAAAABAAAAQAEAAAEAAAAFAAY8aW5pdD4AFkxUZXN0Q2xhc3NBbm5vdGF0aW9uMTsAF0xUZXN0" +
    "TWV0aG9kQW5ub3RhdGlvbjE7AAtMVHJhbnNmb3JtOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJM" +
    "amF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07" +
    "AA5UcmFuc2Zvcm0uamF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2stNC4yNQAFaGVsbG8ABWhpIGhp" +
    "AANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQASAAcOABUABw6HAAEAAREXDAEBAREXDQAAAQEA" +
    "gIAEyAIBAeACAAAAEAAAAAAAAAABAAAAAAAAAAEAAAASAAAAcAAAAAIAAAAIAAAAuAAAAAMAAAAC" +
    "AAAA2AAAAAQAAAABAAAA8AAAAAUAAAAEAAAA+AAAAAYAAAABAAAAGAEAAAMQAAACAAAAOAEAAAEg" +
    "AAACAAAASAEAAAYgAAABAAAAhAEAAAEQAAABAAAAnAEAAAIgAAASAAAAogEAAAMgAAACAAAAkAIA" +
    "AAQgAAACAAAAmwIAAAAgAAABAAAApwIAAAAQAAABAAAAuAIAAA==");

  public static void main(String[] args) {
    doTest(new RemoveAnnotationsTest());
    doTest(new AddAnnotationsTest());
    doTest(new ChangeAnnotationValues());
  }

  private static Annotation[] sortedAnno(Annotation[] annos) {
    Arrays.sort(annos, Comparator.comparing((v) -> v.toString()));
    return annos;
  }

  public static void doTest(TestCase t) {
    // Get back to normal first.
    doCommonClassRedefinition(Transform.class, INITIAL_CLASS_BYTES, INITIAL_DEX_BYTES);
    System.out.println("Running test " + t.getClass());
    printAnnotations(Transform.class);
    t.runTest(new Transform());
    printAnnotations(Transform.class);
  }

  private static void printAnnotations(Class<?> transform) {
    System.out.println("Type annotations: "
        + Arrays.toString(sortedAnno(transform.getAnnotations())));
    for (Method m : transform.getDeclaredMethods()) {
      System.out.println("method " + m + " -> "
          + Arrays.toString(sortedAnno(m.getDeclaredAnnotations())));
    }
  }

  // Transforms the class
  public static void doCommonClassRedefinition(Class<?> target,
                                               byte[] class_file,
                                               byte[] dex_file) {
    Redefinition.doCommonClassRedefinition(target, class_file, dex_file);
  }
}
