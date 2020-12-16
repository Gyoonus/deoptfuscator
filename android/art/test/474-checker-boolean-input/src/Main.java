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

  public static void assertBoolEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /*
   * Test that integer Phis are accepted as Boolean inputs until
   * we implement a suitable type analysis.
   */

  /// CHECK-START: boolean Main.TestPhiAsBoolean(int) select_generator (after)
  /// CHECK-DAG:     <<Phi:i\d+>>     Phi
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<Phi>>]

  public static boolean f1;
  public static boolean f2;

  public static boolean InlinePhi(int x) {
    return (x == 42) ? f1 : f2;
  }

  public static boolean TestPhiAsBoolean(int x) {
    return InlinePhi(x) != true ? true : false;
  }

  /*
   * Test that integer And is accepted as a Boolean input until
   * we implement a suitable type analysis.
   */

  /// CHECK-START: boolean Main.TestAndAsBoolean(boolean, boolean) select_generator (after)
  /// CHECK-DAG:     <<And:i\d+>>     And
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<And>>]

  public static boolean InlineAnd(boolean x, boolean y) {
    return x & y;
  }

  public static boolean TestAndAsBoolean(boolean x, boolean y) {
    return InlineAnd(x, y) != true ? true : false;
  }

  /*
   * Test that integer Or is accepted as a Boolean input until
   * we implement a suitable type analysis.
   */

  /// CHECK-START: boolean Main.TestOrAsBoolean(boolean, boolean) select_generator (after)
  /// CHECK-DAG:     <<Or:i\d+>>      Or
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<Or>>]

  public static boolean InlineOr(boolean x, boolean y) {
    return x | y;
  }

  public static boolean TestOrAsBoolean(boolean x, boolean y) {
    return InlineOr(x, y) != true ? true : false;
  }

  /*
   * Test that integer Xor is accepted as a Boolean input until
   * we implement a suitable type analysis.
   */

  /// CHECK-START: boolean Main.TestXorAsBoolean(boolean, boolean) select_generator (after)
  /// CHECK-DAG:     <<Xor:i\d+>>     Xor
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<Xor>>]

  public static boolean InlineXor(boolean x, boolean y) {
    return x ^ y;
  }

  public static boolean TestXorAsBoolean(boolean x, boolean y) {
    return InlineXor(x, y) != true ? true : false;
  }

  public static void main(String[] args) {
    f1 = true;
    f2 = false;
    assertBoolEquals(true, TestPhiAsBoolean(0));
    assertBoolEquals(false, TestPhiAsBoolean(42));
    assertBoolEquals(true, TestAndAsBoolean(true, false));
    assertBoolEquals(false, TestAndAsBoolean(true, true));
    assertBoolEquals(true, TestOrAsBoolean(false, false));
    assertBoolEquals(false, TestOrAsBoolean(true, true));
    assertBoolEquals(true, TestXorAsBoolean(true, true));
    assertBoolEquals(false, TestXorAsBoolean(true, false));
  }
}
