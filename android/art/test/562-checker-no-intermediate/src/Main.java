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

  /**
   * Check that the intermediate address computation is not reordered or merged
   * across the call to Math.abs().
   */

  /// CHECK-START-ARM: void Main.main(java.lang.String[]) instruction_simplifier_arm (before)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:                                  ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM: void Main.main(java.lang.String[]) instruction_simplifier_arm (after)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<DataOffset:i\d+>>    IntConstant
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:           <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:                                  ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM: void Main.main(java.lang.String[]) GVN$after_arch (after)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<DataOffset:i\d+>>    IntConstant
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:           <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:                                  ArraySet [<<Address2>>,<<Index>>,<<Add>>]


  /// CHECK-START-ARM64: void Main.main(java.lang.String[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Array>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:                                  ArraySet [<<Array>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: void Main.main(java.lang.String[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<DataOffset:i\d+>>    IntConstant
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:           <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:                                  ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  /// CHECK-START-ARM64: void Main.main(java.lang.String[]) GVN$after_arch (after)
  /// CHECK-DAG:           <<ConstM42:i\d+>>      IntConstant -42
  /// CHECK-DAG:           <<DataOffset:i\d+>>    IntConstant
  /// CHECK-DAG:           <<Array:l\d+>>         NullCheck
  /// CHECK-DAG:           <<Index:i\d+>>         BoundsCheck
  /// CHECK-DAG:           <<Address1:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:           <<ArrayGet:i\d+>>      ArrayGet [<<Address1>>,<<Index>>]
  /// CHECK-DAG:           <<AbsM42:i\d+>>        InvokeStaticOrDirect [<<ConstM42>>] intrinsic:MathAbsInt
  /// CHECK-DAG:           <<Add:i\d+>>           Add [<<ArrayGet>>,<<AbsM42>>]
  /// CHECK-DAG:           <<Address2:i\d+>>      IntermediateAddress [<<Array>>,<<DataOffset>>]
  /// CHECK-DAG:                                  ArraySet [<<Address2>>,<<Index>>,<<Add>>]

  public static void main(String[] args) {
    array[index] += Math.abs(-42);
  }

  static int index = 0;
  static int[] array = new int[2];
}
