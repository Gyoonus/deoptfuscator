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

public class Main {
  public static void main(String[] args) {
    if (new Main().$opt$TestFloatPhi() != 33.0f) {
      throw new Error("Unexpected result");
    }
  }

  public float $opt$TestFloatPhi() {
    float a = floatField;
    float b = 42.0f;
    if (test1) {
      // The phi for `a` will be found to be of type float.
      a = otherFloatField;
      // The phi for `b` will be found to be of type int (constants in DEX).
      b = 33.0f;
    }
    // Use a different condition to avoid having dx being too clever.
    if (test2) {
      // Type propagation now realizes that `b` must be of type float. So
      // it requests a float equivalent for `b`. Because the phi for `a` is
      // next to the phi for `b` in the phi list, the compiler used to crash,
      // assuming that a float phi following a phi *must* be for the same DEX
      // register.
      a = b;
    }
    return a;
  }

  float floatField = 4.2f;
  float otherFloatField = 42.2f;
  boolean test1 = true;
  boolean test2 = true;
}
