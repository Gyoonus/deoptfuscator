/*
 * Copyright (C) 2015 The Android Open Source Project
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
    assertEqual(4, add3(1));
    assertEqual(4l, add3(1l));
    assertEqual(4f, add3(1f));
    assertEqual(4d, add3(1d));
  }

  public static int add3(int a) {
    return 1 + a + 2;
  }

  public static long add3(long a) {
    return 1l + a + 2l;
  }

  public static float add3(float a) {
    return 1f + a + 2f;
  }

  public static double add3(double a) {
    return 1d + a + 2d;
  }

  public static void assertEqual(int a, int b) {
    if (a != b) {
      throw new RuntimeException("Expected: " + a + " Found: " + b);
    }
  }

  public static void assertEqual(long a, long b) {
    if (a != b) {
      throw new RuntimeException("Expected: " + a + " Found: " + b);
    }
  }

  public static void assertEqual(float a, float b) {
    boolean aproxEquals = (a > b)
      ? ((a - b) < 0.0001f)
      : ((b - a) < 0.0001f);
    if (!aproxEquals) {
      throw new RuntimeException("Expected: " + a + " Found: " + b);
    }
  }

  public static void assertEqual(double a, double b) {
    boolean aproxEquals = (a > b)
      ? ((a - b) < 0.0001d)
      : ((b - a) < 0.0001d);
    if (!aproxEquals) {
      throw new RuntimeException("Expected: " + a + " Found: " + b);
    }
  }
}
