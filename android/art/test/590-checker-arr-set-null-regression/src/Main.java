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

  public static void main(String args[]) {
    Element[] elements = new Element[51];
    testArraySetCheckCastNull(elements);

    System.out.println("passed");
  }

  /// CHECK-START: void Main.testArraySetCheckCastNull(Main$Element[]) builder (after)
  /// CHECK:         <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         IntConstant 42
  /// CHECK-DAG:     <<Null:l\d+>>          NullConstant
  /// CHECK-DAG:     <<Class:l\d+>>         LoadClass
  /// CHECK-DAG:                            CheckCast [<<Null>>,<<Class>>]
  /// CHECK-DAG:     <<CheckedValue:l\d+>>  BoundType [<<Null>>] klass:Main$Element can_be_null:true
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>]
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<CheckedValue>>] needs_type_check:true

  /// CHECK-START: void Main.testArraySetCheckCastNull(Main$Element[]) instruction_simplifier (after)
  /// CHECK-NOT:                            CheckCast

  /// CHECK-START: void Main.testArraySetCheckCastNull(Main$Element[]) prepare_for_register_allocation (before)
  /// CHECK:         <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         IntConstant 42
  /// CHECK-DAG:     <<Null:l\d+>>          NullConstant
  /// CHECK-DAG:     <<Class:l\d+>>         LoadClass
  /// CHECK-DAG:     <<CheckedValue:l\d+>>  BoundType [<<Null>>]
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>]
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<CheckedValue>>] needs_type_check:true

  /// CHECK-START: void Main.testArraySetCheckCastNull(Main$Element[]) prepare_for_register_allocation (after)
  /// CHECK:         <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         IntConstant 42
  /// CHECK-DAG:     <<Null:l\d+>>          NullConstant
  /// CHECK-DAG:     <<Class:l\d+>>         LoadClass
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<Array>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<Array>>,<<Index>>,<<Null>>] needs_type_check:false

  static void testArraySetCheckCastNull(Element[] elements) {
    Object object = null;
    Element element = (Element) object;
    elements[42] = element;
  }

  class Element {}

}
