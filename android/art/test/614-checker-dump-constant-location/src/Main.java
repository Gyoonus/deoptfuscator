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

  public static int array_int[] = { 0 };
  public static long array_long[] = { 0 };
  public static float array_float[] = { 0.0f };
  public static double array_double[] = { 0.0 };

  // The code used to print constant locations in parallel moves is architecture
  // independent. We only test for ARM and ARM64 as it is easy: 'store'
  // instructions only take registers as a source.

  /// CHECK-START-ARM: void Main.store_to_arrays() register (after)
  /// CHECK:    ParallelMove {{.*#1->.*#2->.*#3\.3->.*#4\.4->.*}}

  /// CHECK-START-ARM64: void Main.store_to_arrays() register (after)
  /// CHECK:    ParallelMove {{.*#1->.*#2->.*#3\.3->.*#4\.4->.*}}

  public void store_to_arrays() {
    array_int[0] = 1;
    array_long[0] = 2;
    array_float[0] = 3.3f;
    array_double[0] = 4.4;
  }

  public static void main(String args[]) {}
}
