/*
 * Copyright (C) 2014 The Android Open Source Project
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

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.
public class Main {

  public static void assertFloatEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    // Generate, compile and check long-to-float Dex instructions.
    longToFloat();
  }

  private static void longToFloat() {
    // The result for this test case used to be slightly less accurate
    // on ARM (both in Quick and Optimizing).
    assertFloatEquals(Float.intBitsToFloat(-555858671), $opt$LongToFloat(-8008112895877447681L));
  }

  // This method produces a long-to-float Dex instruction.
  static float $opt$LongToFloat(long a) { return (float)a; }
}
