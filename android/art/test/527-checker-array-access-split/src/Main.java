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

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /**
   * Test that HArrayGet with a constant index is not split.
   */

  /// CHECK-START-ARM64: int Main.constantIndexGet(int[]) instruction_simplifier_arm64 (before)
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-START-ARM64: int Main.constantIndexGet(int[]) instruction_simplifier_arm64 (after)
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArrayGet [<<Array>>,<<Index>>]


  /// CHECK-START-ARM: int Main.constantIndexGet(int[]) instruction_simplifier_arm (before)
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-START-ARM: int Main.constantIndexGet(int[]) instruction_simplifier_arm (after)
  /// CHECK:           <<Array:l\d+>>         NullCheck
  /// CHECK:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-NOT:                              IntermediateAddress
  /// CHECK:                                  ArrayGet [<<Array>>,<<Index>>]

  public static int constantIndexGet(int array[]) {
    return array[1];
  }

  /**
   * Test that HArraySet with a constant index is not split.
   */

  /// CHECK-START-ARM64: void Main.constantIndexSet(int[]) instruction_simplifier_arm64 (before)
  /// CHECK:             <<Const2:i\d+>>        IntConstant 2
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Const2>>]

  /// CHECK-START-ARM64: void Main.constantIndexSet(int[]) instruction_simplifier_arm64 (after)
  /// CHECK:             <<Const2:i\d+>>        IntConstant 2
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Const2>>]


  /// CHECK-START-ARM:   void Main.constantIndexSet(int[]) instruction_simplifier_arm (before)
  /// CHECK:             <<Const2:i\d+>>        IntConstant 2
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Const2>>]

  /// CHECK-START-ARM:   void Main.constantIndexSet(int[]) instruction_simplifier_arm (after)
  /// CHECK:             <<Const2:i\d+>>        IntConstant 2
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Const2>>]

  public static void constantIndexSet(int array[]) {
    array[1] = 2;
  }

  /**
   * Test basic splitting of HArrayGet.
   */

  /// CHECK-START-ARM64: int Main.get(int[], int) instruction_simplifier_arm64 (before)
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-START-ARM64: int Main.get(int[], int) instruction_simplifier_arm64 (after)
  /// CHECK:             <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArrayGet [<<Address>>,<<Index>>]


  /// CHECK-START-ARM:   int Main.get(int[], int) instruction_simplifier_arm (before)
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArrayGet [<<Array>>,<<Index>>]

  /// CHECK-START-ARM:   int Main.get(int[], int) instruction_simplifier_arm (after)
  /// CHECK:             <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArrayGet [<<Address>>,<<Index>>]

  public static int get(int array[], int index) {
    return array[index];
  }

  /**
   * Test basic splitting of HArraySet.
   */

  /// CHECK-START-ARM64: void Main.set(int[], int, int) instruction_simplifier_arm64 (before)
  /// CHECK:                                    ParameterValue
  /// CHECK:                                    ParameterValue
  /// CHECK:             <<Arg:i\d+>>           ParameterValue
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Arg>>]

  /// CHECK-START-ARM64: void Main.set(int[], int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                                    ParameterValue
  /// CHECK:                                    ParameterValue
  /// CHECK:             <<Arg:i\d+>>           ParameterValue
  /// CHECK:             <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address>>,<<Index>>,<<Arg>>]


  /// CHECK-START-ARM:   void Main.set(int[], int, int) instruction_simplifier_arm (before)
  /// CHECK:                                    ParameterValue
  /// CHECK:                                    ParameterValue
  /// CHECK:             <<Arg:i\d+>>           ParameterValue
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Arg>>]

  /// CHECK-START-ARM:   void Main.set(int[], int, int) instruction_simplifier_arm (after)
  /// CHECK:                                    ParameterValue
  /// CHECK:                                    ParameterValue
  /// CHECK:             <<Arg:i\d+>>           ParameterValue
  /// CHECK:             <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address>>,<<Index>>,<<Arg>>]

  public static void set(int array[], int index, int value) {
    array[index] = value;
  }

  /**
   * Check that the intermediate address can be shared after GVN.
   */

  /// CHECK-START-ARM64: void Main.getSet(int[], int) instruction_simplifier_arm64 (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: void Main.getSet(int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: void Main.getSet(int[], int) GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Address>>,<<Index>>,<<Add>>]


  /// CHECK-START-ARM:   void Main.getSet(int[], int) instruction_simplifier_arm (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM:   void Main.getSet(int[], int) instruction_simplifier_arm (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM:   void Main.getSet(int[], int) GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Address>>,<<Index>>,<<Add>>]
  public static void getSet(int array[], int index) {
    array[index] = array[index] + 1;
  }

  /**
   * Check that the intermediate address computation is not reordered or merged
   * across IRs that can trigger GC.
   */

  /// CHECK-START-ARM64: int[] Main.accrossGC(int[], int) instruction_simplifier_arm64 (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: int[] Main.accrossGC(int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: int[] Main.accrossGC(int[], int) GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:                                    ArraySet [<<Address2>>,<<Index>>,<<Add>>]


  /// CHECK-START-ARM:   int[] Main.accrossGC(int[], int) instruction_simplifier_arm (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM:   int[] Main.accrossGC(int[], int) instruction_simplifier_arm (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM:   int[] Main.accrossGC(int[], int) GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant
  /// CHECK:             <<Array:l\d+>>         NullCheck
  /// CHECK:             <<Index:i\d+>>         BoundsCheck
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Add:i\d+>>           Add [<<ArrayGet>>,<<Const1>>]
  /// CHECK:                                    NewArray
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:                                    ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  public static int[] accrossGC(int array[], int index) {
    int tmp = array[index] + 1;
    int[] new_array = new int[1];
    array[index] = tmp;
    return new_array;
  }

  /**
   * Test that the intermediate address is shared between array accesses after
   * the bounds check have been removed by BCE.
   */
  // For checker tests `instruction_simplifier_<arch> (after)` below, by the time we reach
  // the architecture-specific instruction simplifier, BCE has removed the bounds checks in
  // the loop.

  // Note that we do not care that the `DataOffset` is `12`. But if we do not
  // specify it and any other `IntConstant` appears before that instruction,
  // checker will match the previous `IntConstant`, and we will thus fail the
  // check.

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE1() instruction_simplifier_arm64 (before)
  /// CHECK:             <<Const7:i\d+>>        IntConstant 7
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Div>>]

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE1() instruction_simplifier_arm64 (after)
  /// CHECK-DAG:         <<Const7:i\d+>>        IntConstant 7
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Div>>]

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE1() GVN$after_arch (after)
  /// CHECK-DAG:         <<Const7:i\d+>>        IntConstant 7
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Address>>,<<Index>>,<<Div>>]


  /// CHECK-START-ARM:   int Main.canMergeAfterBCE1() instruction_simplifier_arm (before)
  /// CHECK:             <<Const7:i\d+>>        IntConstant 7
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index>>,<<Div>>]

  /// CHECK-START-ARM:   int Main.canMergeAfterBCE1() instruction_simplifier_arm (after)
  /// CHECK-DAG:         <<Const7:i\d+>>        IntConstant 7
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:        <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK:             <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-NEXT:                               ArraySet [<<Address2>>,<<Index>>,<<Div>>]

  /// CHECK-START-ARM:   int Main.canMergeAfterBCE1() GVN$after_arch (after)
  /// CHECK-DAG:         <<Const7:i\d+>>        IntConstant 7
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK:             <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:             <<ArrayGet:i\d+>>      ArrayGet [<<Address>>,<<Index>>]
  /// CHECK:             <<Div:i\d+>>           Div [<<ArrayGet>>,<<Const7>>]
  /// CHECK-NOT:                                IntermediateAddress
  /// CHECK:                                    ArraySet [<<Address>>,<<Index>>,<<Div>>]

  public static int canMergeAfterBCE1() {
    int[] array = {0, 7, 14, 21};
    for (int i = 0; i < array.length; i++) {
      array[i] = array[i] / 7;
    }
    return array[array.length - 1];
  }

  /**
   * This test case is similar to `canMergeAfterBCE1`, but with different
   * indexes for the accesses.
   */

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE2() instruction_simplifier_arm64 (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Array>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index1>>,<<Shl>>]

  // Note that we do not care that the `DataOffset` is `12`. But if we do not
  // specify it and any other `IntConstant` appears before that instruction,
  // checker will match the previous `IntConstant`, and we will thus fail the
  // check.

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE2() instruction_simplifier_arm64 (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:         <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Address2>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:             <<Address3:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:                                    ArraySet [<<Address3>>,<<Index1>>,<<Shl>>]

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE2() GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Address>>,<<Index>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Address>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:                                    ArraySet [<<Address>>,<<Index1>>,<<Shl>>]

  // There should be only one intermediate address computation in the loop.

  /// CHECK-START-ARM64: int Main.canMergeAfterBCE2() GVN$after_arch (after)
  /// CHECK:                                    IntermediateAddress
  /// CHECK-NOT:                                IntermediateAddress


  /// CHECK-START-ARM:   int Main.canMergeAfterBCE2() instruction_simplifier_arm (before)
  /// CHECK:             <<Const1:i\d+>>        IntConstant 1
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Array>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:                                    ArraySet [<<Array>>,<<Index1>>,<<Shl>>]

  /// CHECK-START-ARM:   int Main.canMergeAfterBCE2() instruction_simplifier_arm (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:         <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Address2>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:             <<Address3:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK:                                    ArraySet [<<Address3>>,<<Index1>>,<<Shl>>]

  /// CHECK-START-ARM:   int Main.canMergeAfterBCE2() GVN$after_arch (after)
  /// CHECK-DAG:         <<Const1:i\d+>>        IntConstant 1
  /// CHECK-DAG:         <<DataOffset:i\d+>>    IntConstant 12
  /// CHECK:             <<Array:l\d+>>         NewArray
  /// CHECK:             <<Index:i\d+>>         Phi
  /// CHECK:                                    If
  //  -------------- Loop
  /// CHECK-DAG:         <<Index1:i\d+>>        Add [<<Index>>,<<Const1>>]
  /// CHECK-DAG:         <<Address:i\d+>>       IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:         <<ArrayGetI:i\d+>>     ArrayGet [<<Address>>,<<Index>>]
  /// CHECK-DAG:         <<ArrayGetI1:i\d+>>    ArrayGet [<<Address>>,<<Index1>>]
  /// CHECK:             <<Shl:i\d+>>           Shl [<<ArrayGetI>>,<<ArrayGetI1>>]
  /// CHECK:                                    ArraySet [<<Address>>,<<Index1>>,<<Shl>>]

  /// CHECK-START-ARM:   int Main.canMergeAfterBCE2() GVN$after_arch (after)
  /// CHECK:                                    IntermediateAddress
  /// CHECK-NOT:                                IntermediateAddress

  public static int canMergeAfterBCE2() {
    int[] array = {64, 8, 4, 2 };
    for (int i = 0; i < array.length - 1; i++) {
      array[i + 1] = array[i] << array[i + 1];
    }
    return array[array.length - 1];
  }

  /// CHECK-START-ARM: int Main.checkLongFloatDouble() instruction_simplifier_arm (before)
  /// CHECK-DAG:         <<Array1:l\d+>>        NewArray
  /// CHECK-DAG:         <<Array2:l\d+>>        NewArray
  /// CHECK-DAG:         <<Array3:l\d+>>        NewArray
  /// CHECK-DAG:         <<Index:i\d+>>         Phi
  /// CHECK-DAG:                                ArrayGet [<<Array1>>,<<Index>>]
  /// CHECK-DAG:                                ArrayGet [<<Array2>>,<<Index>>]
  /// CHECK-DAG:                                ArrayGet [<<Array3>>,<<Index>>]

  /// CHECK-START-ARM: int Main.checkLongFloatDouble() instruction_simplifier_arm (after)
  /// CHECK-DAG:         <<Array1:l\d+>>        NewArray
  /// CHECK-DAG:         <<Array2:l\d+>>        NewArray
  /// CHECK-DAG:         <<Array3:l\d+>>        NewArray
  /// CHECK-DAG:         <<Index:i\d+>>         Phi
  /// CHECK-DAG:                                ArrayGet [<<Array1>>,<<Index>>]
  /// CHECK-DAG:                                ArrayGet [<<Array2>>,<<Index>>]
  /// CHECK-DAG:                                ArrayGet [<<Array3>>,<<Index>>]

  /// CHECK-START-ARM: int Main.checkLongFloatDouble() instruction_simplifier_arm (after)
  /// CHECK-NOT:                                IntermediateAddress
  public static int checkLongFloatDouble() {
    long[] array_long = {0, 1, 2, 3};
    float[] array_float = {(float)0.0, (float)1.0, (float)2.0, (float)3.0};
    double[] array_double = {0.0, 1.0, 2.0, 3.0};
    double s = 0.0;

    for (int i = 0; i < 4; i++) {
      s += (double)array_long[i] + (double)array_float[i] + array_double[i];
    }
    return (int)s;
  }

  public static void main(String[] args) {
    int[] array = {123, 456, 789};

    assertIntEquals(456, constantIndexGet(array));

    constantIndexSet(array);
    assertIntEquals(2, array[1]);

    assertIntEquals(789, get(array, 2));

    set(array, 1, 456);
    assertIntEquals(456, array[1]);

    getSet(array, 0);
    assertIntEquals(124, array[0]);

    accrossGC(array, 0);
    assertIntEquals(125, array[0]);

    assertIntEquals(3, canMergeAfterBCE1());
    assertIntEquals(1048576, canMergeAfterBCE2());

    assertIntEquals(18, checkLongFloatDouble());
  }
}
