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
public class ChangeAnnotationValues implements TestCase {
  /**
   * base64 encoded class/dex file for
   * @TestClassAnnotation1("Goodbye")
   * class Transform {
   *   @TestMethodAnnotation1("Bye Bye")
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAJAoABgAXCQAYABkIABYKABoAGwcAHAcAHQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBABJMb2NhbFZhcmlhYmxlVGFibGUBAAR0aGlzAQALTFRyYW5zZm9y" +
    "bTsBAAVzYXlIaQEAGVJ1bnRpbWVWaXNpYmxlQW5ub3RhdGlvbnMBABdMVGVzdE1ldGhvZEFubm90" +
    "YXRpb24xOwEABXZhbHVlAQAHQnllIEJ5ZQEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQEA" +
    "FkxUZXN0Q2xhc3NBbm5vdGF0aW9uMTsBAAdHb29kYnllDAAHAAgHAB4MAB8AIAcAIQwAIgAjAQAJ" +
    "VHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVM" +
    "amF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShM" +
    "amF2YS9sYW5nL1N0cmluZzspVgAgAAUABgAAAAAAAgAAAAcACAABAAkAAAAvAAEAAQAAAAUqtwAB" +
    "sQAAAAIACgAAAAYAAQAAAAIACwAAAAwAAQAAAAUADAANAAAAAQAOAAgAAgAJAAAANwACAAEAAAAJ" +
    "sgACEgO2AASxAAAAAgAKAAAACgACAAAABQAIAAYACwAAAAwAAQAAAAkADAANAAAADwAAAAsAAQAQ" +
    "AAEAEXMAEgACABMAAAACABQADwAAAAsAAQAVAAEAEXMAFg==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQAXfYs9FUE830lxfnB+X66S7iZiP5A7uDSAAwAAcAAAAHhWNBIAAAAAAAAAALwCAAAS" +
    "AAAAcAAAAAgAAAC4AAAAAgAAANgAAAABAAAA8AAAAAQAAAD4AAAAAQAAABgBAABIAgAAOAEAAKIB" +
    "AACqAQAAswEAALwBAADUAQAA7QEAAPoBAAARAgAAJQIAADkCAABNAgAAXQIAAGACAABkAgAAeAIA" +
    "AH0CAACGAgAAjQIAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAALAAAACwAAAAcAAAAAAAAA" +
    "DAAAAAcAAACcAQAABgADAA4AAAACAAAAAAAAAAIAAAAQAAAAAwABAA8AAAAEAAAAAAAAAAIAAAAA" +
    "AAAABAAAAAAAAAAKAAAAhAEAAKsCAAAAAAAAAQAAAJ8CAAABAAAApQIAAAEAAQABAAAAlAIAAAQA" +
    "AABwEAMAAAAOAAMAAQACAAAAmQIAAAkAAABiAAAAGwECAAAAbiACABAADgAAADgBAAAAAAAAAQAA" +
    "AAAAAAABAAAAQAEAAAEAAAAFAAY8aW5pdD4AB0J5ZSBCeWUAB0dvb2RieWUAFkxUZXN0Q2xhc3NB" +
    "bm5vdGF0aW9uMTsAF0xUZXN0TWV0aG9kQW5ub3RhdGlvbjE7AAtMVHJhbnNmb3JtOwAVTGphdmEv" +
    "aW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAS" +
    "TGphdmEvbGFuZy9TeXN0ZW07AA5UcmFuc2Zvcm0uamF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2st" +
    "NC4yNQADb3V0AAdwcmludGxuAAVzYXlIaQAFdmFsdWUAAgAHDgAFAAcOhwABAAERFwIBAQERFwEA" +
    "AAEBAICABMgCAQHgAgAAABAAAAAAAAAAAQAAAAAAAAABAAAAEgAAAHAAAAACAAAACAAAALgAAAAD" +
    "AAAAAgAAANgAAAAEAAAAAQAAAPAAAAAFAAAABAAAAPgAAAAGAAAAAQAAABgBAAADEAAAAgAAADgB" +
    "AAABIAAAAgAAAEgBAAAGIAAAAQAAAIQBAAABEAAAAQAAAJwBAAACIAAAEgAAAKIBAAADIAAAAgAA" +
    "AJQCAAAEIAAAAgAAAJ8CAAAAIAAAAQAAAKsCAAAAEAAAAQAAALwCAAA=");

  public void runTest(Transform t) {
    t.sayHi();
    Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    t.sayHi();
  }
}
