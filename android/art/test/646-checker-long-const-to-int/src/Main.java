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

public class Main {

  public static void main(String[] args) {
    System.out.println(test());
  }

  public static long testField = 0;
  public static long longField0 = 0;
  public static long longField1 = 0;
  public static long longField2 = 0;
  public static long longField3 = 0;
  public static long longField4 = 0;
  public static long longField5 = 0;
  public static long longField6 = 0;
  public static long longField7 = 0;

  /// CHECK-START-ARM: int Main.test() register (after)
  /// CHECK: TypeConversion locations:[#-8690466096623102344]->{{.*}}
  public static int test() {
    // To avoid constant folding TypeConversion(const), hide the constant in a field.
    // We do not run constant folding after load-store-elimination.
    testField = 0x8765432112345678L;
    long value = testField;
    // Now, the `value` is in a register because of the store but we need
    // a constant location to trigger the bug, so load a bunch of other fields.
    long l0 = longField0;
    long l1 = longField1;
    long l2 = longField2;
    long l3 = longField3;
    long l4 = longField4;
    long l5 = longField5;
    long l6 = longField6;
    long l7 = longField7;
    if (l0 != 0 || l1 != 0 || l2 != 0 || l3 != 0 || l4 != 0 || l5 != 0 || l6 != 0 || l7 != 0) {
      throw new Error();
    }
    // Do the conversion from constant location.
    return (int)value;
  }
}
