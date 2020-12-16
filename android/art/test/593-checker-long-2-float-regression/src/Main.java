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

public class Main {

  static boolean doThrow = false;
  static long longValue;

  public static void assertEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    assertEquals(1.0F, $noinline$longToFloat());
  }

  /// CHECK-START: float Main.$noinline$longToFloat() register (after)
  /// CHECK-DAG:     <<Const1:j\d+>>   LongConstant 1
  /// CHECK-DAG:     <<Convert:f\d+>>  TypeConversion [<<Const1>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  static float $noinline$longToFloat() {
    if (doThrow) { throw new Error(); }
    longValue = $inline$returnConst();
    return (float) longValue;
  }

  static long $inline$returnConst() {
    return 1L;
  }
}
