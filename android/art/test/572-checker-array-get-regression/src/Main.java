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

  public static void main(String[] args) {
    System.out.println(test().intValue());
  }

  /// CHECK-START: java.lang.Integer Main.test() builder (after)
  /// CHECK-DAG:     <<Const2P19:i\d+>>    IntConstant 524288
  /// CHECK-DAG:     <<ConstM1:i\d+>>      IntConstant -1
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass
  /// CHECK-DAG:     <<Array:l\d+>>        NewArray [<<LoadClass>>,<<Const2P19>>]
  /// CHECK-DAG:     <<Length1:i\d+>>      ArrayLength [<<Array>>]
  /// CHECK-DAG:     <<Index:i\d+>>        Add [<<Length1>>,<<ConstM1>>]
  /// CHECK-DAG:     <<Length2:i\d+>>      ArrayLength [<<Array>>]
  /// CHECK-DAG:     <<BoundsCheck:i\d+>>  BoundsCheck [<<Index>>,<<Length2>>]
  /// CHECK-DAG:     <<LastElement:l\d+>>  ArrayGet [<<Array>>,<<BoundsCheck>>]
  /// CHECK-DAG:                           Return [<<LastElement>>]


  /// CHECK-START: java.lang.Integer Main.test() register (before)
  /// CHECK-DAG:     <<Const2P19:i\d+>>    IntConstant 524288
  /// CHECK-DAG:     <<Const2P19M1:i\d+>>  IntConstant 524287
  /// CHECK-DAG:     <<LoadClass:l\d+>>    LoadClass
  /// CHECK-DAG:     <<Array:l\d+>>        NewArray [<<LoadClass>>,<<Const2P19>>]
  /// CHECK-DAG:     <<LastElement:l\d+>>  ArrayGet [<<Array>>,<<Const2P19M1>>]
  /// CHECK-DAG:                           Return [<<LastElement>>]

  public static Integer test() {
    Integer[] integers = new Integer[1 << 19];
    initIntegerArray(integers);
    // Array load with a large constant index (after constant folding
    // and bounds check elimination).
    Integer last_integer = integers[integers.length - 1];
    return last_integer;
  }

  public static void initIntegerArray(Integer[] integers) {
    for (int i = 0; i < integers.length; ++i) {
      integers[i] = new Integer(i);
    }
  }

}
