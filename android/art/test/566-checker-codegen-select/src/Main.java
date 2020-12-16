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

  /// CHECK-START: long Main.$noinline$longSelect(long) register (before)
  /// CHECK:         <<Cond:z\d+>> LessThanOrEqual [{{j\d+}},{{j\d+}}]
  /// CHECK-NEXT:                  Select [{{j\d+}},{{j\d+}},<<Cond>>]

  public long $noinline$longSelect(long param) {
    if (doThrow) { throw new Error(); }
    long val_true = longB;
    long val_false = longC;
    return (param > longA) ? val_true : val_false;
  }

  /// CHECK-START: long Main.$noinline$longSelect_Constant(long) register (before)
  /// CHECK:         <<Const:j\d+>> LongConstant
  /// CHECK:         <<Cond:z\d+>>  LessThanOrEqual [{{j\d+}},<<Const>>]
  /// CHECK-NEXT:                   Select [{{j\d+}},{{j\d+}},<<Cond>>]

  // Condition can be non-materialized on X86 because the condition does not
  // request 4 registers any more.
  /// CHECK-START-X86: long Main.$noinline$longSelect_Constant(long) disassembly (after)
  /// CHECK:             LessThanOrEqual
  /// CHECK-NEXT:        Select

  // Check that we generate CMOV for long on x86_64.
  /// CHECK-START-X86_64: long Main.$noinline$longSelect_Constant(long) disassembly (after)
  /// CHECK:             LessThanOrEqual
  /// CHECK-NEXT:        Select
  /// CHECK:             cmpq
  /// CHECK:             cmovle/ngq

  public long $noinline$longSelect_Constant(long param) {
    if (doThrow) { throw new Error(); }
    long val_true = longB;
    long val_false = longC;
    return (param > 3L) ? val_true : val_false;
  }

  // Check that we generate CMOV for int on x86_64.
  /// CHECK-START-X86_64: int Main.$noinline$intSelect_Constant(int) disassembly (after)
  /// CHECK:             LessThan
  /// CHECK-NEXT:        Select
  /// CHECK:             cmp
  /// CHECK:             cmovl/nge

  public int $noinline$intSelect_Constant(int param) {
    if (doThrow) { throw new Error(); }
    int val_true = intB;
    int val_false = intC;
    return (param >= 3) ? val_true : val_false;
  }

  public static void main(String[] args) {
    Main m = new Main();
    assertLongEquals(5L, m.$noinline$longSelect(4L));
    assertLongEquals(7L, m.$noinline$longSelect(2L));
    assertLongEquals(5L, m.$noinline$longSelect_Constant(4L));
    assertLongEquals(7L, m.$noinline$longSelect_Constant(2L));
    assertIntEquals(5, m.$noinline$intSelect_Constant(4));
    assertIntEquals(7, m.$noinline$intSelect_Constant(2));
  }

  public static void assertIntEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error(expected + " != " + actual);
    }
  }

  public static void assertLongEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error(expected + " != " + actual);
    }
  }

  public boolean doThrow = false;

  public long longA = 3L;
  public long longB = 5L;
  public long longC = 7L;
  public int intB = 5;
  public int intC = 7;
}
