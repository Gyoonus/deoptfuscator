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
    assertEquals(4.2f, returnFloat());
    float[] a = new float[2];
    a[0] = 42.2f;
    a[1] = 3.2f;
    assertEquals(45.4f, returnFloat(a));

    assertEquals(4.4, returnDouble());
    double[] b = new double[1];
    b[0] = 42.4;
    assertEquals(42.4, returnDouble(b));

    assertEquals(4.2f, invokeReturnFloat());
    assertEquals(4.4, invokeReturnDouble());
    assertEquals(4.2f, takeAFloat(4.2f));
    assertEquals(3.1, takeADouble(3.1));
    assertEquals(12.7, takeThreeDouble(3.1, 4.4, 5.2));
    assertEquals(12.7f, takeThreeFloat(3.1f, 4.4f, 5.2f));
    assertEquals(4.2f, invokeTakeAFloat(4.2f));
    assertEquals(3.1, invokeTakeADouble(3.1));
    assertEquals(12.7, invokeTakeThreeDouble(3.1, 4.4, 5.2));
    assertEquals(12.7f, invokeTakeThreeFloat(3.1f, 4.4f, 5.2f));

    testArrayOperations(new float[2], 0, 1.2f, 3.4f);
    testArrayOperations(new double[2], 0, 4.1, 7.6);
  }

  public static float invokeReturnFloat() {
    return returnFloat();
  }

  public static double invokeReturnDouble() {
    return returnDouble();
  }

  public static float returnFloat() {
    return 4.2f;
  }

  public static float returnFloat(float[] a) {
    return a[0] + a[1];
  }

  public static double returnDouble() {
    return 4.4;
  }

  public static double returnDouble(double[] a) {
    return a[0];
  }

  public static float takeAFloat(float a) {
    return a;
  }

  public static double takeADouble(double a) {
    return a;
  }

  public static double takeThreeDouble(double a, double b, double c) {
    return a + b + c;
  }

  public static float takeThreeFloat(float a, float b, float c) {
    return a + b + c;
  }

  public static float invokeTakeAFloat(float a) {
    return takeAFloat(a);
  }

  public static double invokeTakeADouble(double a) {
    return takeADouble(a);
  }

  public static double invokeTakeThreeDouble(double a, double b, double c) {
    return takeThreeDouble(a, b, c);
  }

  public static float invokeTakeThreeFloat(float a, float b, float c) {
    return takeThreeFloat(a, b, c);
  }

  // Test simple operations on a float array to ensure the register allocator works
  // properly.
  public static void testArrayOperations(float[] a, int index, float value1, float value2) {
    a[0] = value1;
    a[1] = value2;
    assertEquals(value1 + value2, a[0] + a[1]);
    a[0] = 0.0f;
    a[1] = 0.0f;
    assertEquals(0.0f, a[0] + a[1]);
    a[index] = value1;
    a[index + 1] = value2;
    assertEquals(value1 + value2, a[0] + a[1]);
  }

  // Test simple operations on a double array to ensure the register allocator works
  // properly.
  public static void testArrayOperations(double[] a, int index, double value1, double value2) {
    a[0] = value1;
    a[1] = value2;
    assertEquals(value1 + value2, a[0] + a[1]);
    a[0] = 0.0;
    a[1] = 0.0;
    assertEquals(0.0, a[0] + a[1]);
    a[index] = value1;
    a[index + 1] = value2;
    assertEquals(value1 + value2, a[0] + a[1]);
  }

  public static void assertEquals(float expected, float actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertEquals(double expected, double actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }
}
