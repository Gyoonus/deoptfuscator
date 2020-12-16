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

  /// CHECK-START: java.lang.Object Main.InlineNullConstant() inliner (before)
  /// CHECK:         NullConstant
  /// CHECK-NOT:     NullConstant

  /// CHECK-START: java.lang.Object Main.InlineNullConstant() inliner (after)
  /// CHECK:         NullConstant
  /// CHECK-NOT:     NullConstant

  public static Object returnNullConstant(Object x) {
    return null;
  }

  public static Object InlineNullConstant() {
    return returnNullConstant(null);
  }

  /// CHECK-START: int Main.InlineIntConstant() inliner (before)
  /// CHECK:         IntConstant 42
  /// CHECK-NOT:     IntConstant 42

  /// CHECK-START: int Main.InlineIntConstant() inliner (after)
  /// CHECK:         IntConstant 42
  /// CHECK-NOT:     IntConstant 42

  public static int returnIntConstant(int x) {
    return 42;
  }

  public static int InlineIntConstant() {
    return returnIntConstant(42);
  }

  /// CHECK-START: long Main.InlineLongConstant() inliner (before)
  /// CHECK:         LongConstant 42
  /// CHECK-NOT:     LongConstant 42

  /// CHECK-START: long Main.InlineLongConstant() inliner (after)
  /// CHECK:         LongConstant 42
  /// CHECK-NOT:     LongConstant 42

  public static long returnLongConstant(long x) {
    return 42L;
  }

  public static long InlineLongConstant() {
    return returnLongConstant(42L);
  }

  public static void main(String[] args) {
    if (InlineNullConstant() != null) {
      throw new Error("Expected null");
    } else if (InlineIntConstant() != 42) {
      throw new Error("Expected int 42");
    } else if (InlineLongConstant() != 42L) {
      throw new Error("Expected long 42");
    }
  }
}
