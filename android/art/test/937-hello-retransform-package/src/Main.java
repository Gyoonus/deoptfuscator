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

import java.util.Base64;

import testing.*;
import art.Redefinition;

public class Main {

  /**
   * base64 encoded class/dex file for
   * package testing;
   * class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAHAoABgAOCQAPABAIABEKABIAEwcAFAcAFQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQwA" +
    "BwAIBwAWDAAXABgBAAdHb29kYnllBwAZDAAaABsBABF0ZXN0aW5nL1RyYW5zZm9ybQEAEGphdmEv" +
    "bGFuZy9PYmplY3QBABBqYXZhL2xhbmcvU3lzdGVtAQADb3V0AQAVTGphdmEvaW8vUHJpbnRTdHJl" +
    "YW07AQATamF2YS9pby9QcmludFN0cmVhbQEAB3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7" +
    "KVYAIQAFAAYAAAAAAAIAAQAHAAgAAQAJAAAAHQABAAEAAAAFKrcAAbEAAAABAAoAAAAGAAEAAAAC" +
    "AAEACwAIAAEACQAAACUAAgABAAAACbIAAhIDtgAEsQAAAAEACgAAAAoAAgAAAAQACAAFAAEADAAA" +
    "AAIADQ==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBhYIi3Gs9Nn/GN1fCzF+aFQ0AbhA1h1WHUAgAAcAAAAHhWNBIAAAAAAAAAADQCAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAAC0AQAAIAEAAGIB" +
    "AABqAQAAcwEAAIoBAACeAQAAsgEAAMYBAADbAQAA6wEAAO4BAADyAQAABgIAAAsCAAAUAgAAAgAA" +
    "AAMAAAAEAAAABQAAAAYAAAAIAAAACAAAAAUAAAAAAAAACQAAAAUAAABcAQAAAwAAAAsAAAAAAAEA" +
    "DAAAAAEAAAAAAAAABAAAAAAAAAAEAAAADQAAAAQAAAABAAAAAQAAAAAAAAAHAAAAAAAAACYCAAAA" +
    "AAAAAQABAAEAAAAbAgAABAAAAHAQAQAAAA4AAwABAAIAAAAgAgAACQAAAGIAAAAbAQEAAABuIAAA" +
    "EAAOAAAAAQAAAAIABjxpbml0PgAHR29vZGJ5ZQAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2" +
    "YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07ABNM" +
    "dGVzdGluZy9UcmFuc2Zvcm07AA5UcmFuc2Zvcm0uamF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2st" +
    "NC4yMgADb3V0AAdwcmludGxuAAVzYXlIaQACAAcOAAQABw6HAAAAAQECgYAEoAIDAbgCDQAAAAAA" +
    "AAABAAAAAAAAAAEAAAAOAAAAcAAAAAIAAAAGAAAAqAAAAAMAAAACAAAAwAAAAAQAAAABAAAA2AAA" +
    "AAUAAAAEAAAA4AAAAAYAAAABAAAAAAEAAAEgAAACAAAAIAEAAAEQAAABAAAAXAEAAAIgAAAOAAAA" +
    "YgEAAAMgAAACAAAAGwIAAAAgAAABAAAAJgIAAAAQAAABAAAANAIAAA==");

  public static void main(String[] args) {
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    t.sayHi();
    Redefinition.addCommonTransformationResult("testing/Transform", CLASS_BYTES, DEX_BYTES);
    Redefinition.enableCommonRetransformation(true);
    Redefinition.doCommonClassRetransformation(Transform.class);
    t.sayHi();
  }
}
