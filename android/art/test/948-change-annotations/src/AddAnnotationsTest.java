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
public class AddAnnotationsTest implements TestCase {
  /**
   * base64 encoded class/dex file for
   * @TestClassAnnotation1("hello")
   * @TestClassAnnotation2("hello2")
   * class Transform {
   *   @TestMethodAnnotation1("hi hi")
   *   @TestMethodAnnotation2("hi hi2")
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAKQoABgAbCQAcAB0IAB4KAB8AIAcAIQcAIgEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBABJMb2NhbFZhcmlhYmxlVGFibGUBAAR0aGlzAQALTFRyYW5zZm9y" +
    "bTsBAAVzYXlIaQEAGVJ1bnRpbWVWaXNpYmxlQW5ub3RhdGlvbnMBABdMVGVzdE1ldGhvZEFubm90" +
    "YXRpb24xOwEABXZhbHVlAQAFaGkgaGkBABdMVGVzdE1ldGhvZEFubm90YXRpb24yOwEABmhpIGhp" +
    "MgEAClNvdXJjZUZpbGUBAA5UcmFuc2Zvcm0uamF2YQEAFkxUZXN0Q2xhc3NBbm5vdGF0aW9uMTsB" +
    "AAVoZWxsbwEAFkxUZXN0Q2xhc3NBbm5vdGF0aW9uMjsBAAZoZWxsbzIMAAcACAcAIwwAJAAlAQAH" +
    "R29vZGJ5ZQcAJgwAJwAoAQAJVHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEvbGFu" +
    "Zy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50U3Ry" +
    "ZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgAgAAUABgAAAAAAAgAAAAcACAAB" +
    "AAkAAAAvAAEAAQAAAAUqtwABsQAAAAIACgAAAAYAAQAAABMACwAAAAwAAQAAAAUADAANAAAAAQAO" +
    "AAgAAgAJAAAANwACAAEAAAAJsgACEgO2AASxAAAAAgAKAAAACgACAAAAFwAIABgACwAAAAwAAQAA" +
    "AAkADAANAAAADwAAABQAAgAQAAEAEXMAEgATAAEAEXMAFAACABUAAAACABYADwAAABQAAgAXAAEA" +
    "EXMAGAAZAAEAEXMAGg==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQA7mPKPjUKe43s+OLHHgFVRVCAPn/rRz9z0AwAAcAAAAHhWNBIAAAAAAAAAADADAAAX" +
    "AAAAcAAAAAoAAADMAAAAAgAAAPQAAAABAAAADAEAAAQAAAAUAQAAAQAAADQBAACgAgAAVAEAAMYB" +
    "AADOAQAA1wEAAO8BAAAHAgAAIAIAADkCAABGAgAAXQIAAHECAACFAgAAmQIAAKkCAACsAgAAsAIA" +
    "AMQCAADLAgAA0wIAANoCAADiAgAA5wIAAPACAAD3AgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAA" +
    "CAAAAAkAAAAKAAAADAAAAAwAAAAJAAAAAAAAAA0AAAAJAAAAwAEAAAgABQATAAAABAAAAAAAAAAE" +
    "AAAAFQAAAAUAAQAUAAAABgAAAAAAAAAEAAAAAAAAAAYAAAAAAAAACwAAAKgBAAAhAwAAAAAAAAIA" +
    "AAAJAwAADwMAAAIAAAAVAwAAGwMAAAEAAQABAAAA/gIAAAQAAABwEAMAAAAOAAMAAQACAAAAAwMA" +
    "AAkAAABiAAAAGwEBAAAAbiACABAADgAAAFQBAAAAAAAAAQAAAAAAAAABAAAAYAEAAAEAAAAHAAY8" +
    "aW5pdD4AB0dvb2RieWUAFkxUZXN0Q2xhc3NBbm5vdGF0aW9uMTsAFkxUZXN0Q2xhc3NBbm5vdGF0" +
    "aW9uMjsAF0xUZXN0TWV0aG9kQW5ub3RhdGlvbjE7ABdMVGVzdE1ldGhvZEFubm90YXRpb24yOwAL" +
    "TFRyYW5zZm9ybTsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABJM" +
    "amF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVtOwAOVHJhbnNmb3JtLmphdmEAAVYA" +
    "AlZMABJlbWl0dGVyOiBqYWNrLTQuMjUABWhlbGxvAAZoZWxsbzIABWhpIGhpAAZoaSBoaTIAA291" +
    "dAAHcHJpbnRsbgAFc2F5SGkABXZhbHVlABMABw4AFwAHDocAAQABFhcPAQEBFhcQAQIBFhcRAQMB" +
    "FhcSAAABAQCAgATsAgEBhAMAEAAAAAAAAAABAAAAAAAAAAEAAAAXAAAAcAAAAAIAAAAKAAAAzAAA" +
    "AAMAAAACAAAA9AAAAAQAAAABAAAADAEAAAUAAAAEAAAAFAEAAAYAAAABAAAANAEAAAMQAAACAAAA" +
    "VAEAAAEgAAACAAAAbAEAAAYgAAABAAAAqAEAAAEQAAABAAAAwAEAAAIgAAAXAAAAxgEAAAMgAAAC" +
    "AAAA/gIAAAQgAAAEAAAACQMAAAAgAAABAAAAIQMAAAAQAAABAAAAMAMAAA==");

  public void runTest(Transform t) {
    t.sayHi();
    Main.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
    t.sayHi();
  }
}
