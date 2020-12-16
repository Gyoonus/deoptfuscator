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
    int[] array = new int[51];
    testArrayLengthBoundsCheckX86(array, 10);

    System.out.println("passed");
  }

  /// CHECK-START-X86: void Main.testArrayLengthBoundsCheckX86(int[], int) x86_memory_operand_generation (before)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>] is_string_length:false loop:none
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<Value>>]

  /// CHECK-START-X86: void Main.testArrayLengthBoundsCheckX86(int[], int) x86_memory_operand_generation (after)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>] is_string_length:false emitted_at_use:true loop:none
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<Value>>]

  /// CHECK-START-X86: void Main.testArrayLengthBoundsCheckX86(int[], int) disassembly (after)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK:         <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-NEXT:    <<Length:i\d+>>        ArrayLength [<<Array>>] is_string_length:false emitted_at_use:true loop:none
  /// CHECK-NEXT:    <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-NEXT:                           cmp [<<BaseReg:\w+>> + 8], <<IndexReg:\w+>>
  /// CHECK:         <<ArraySet:v\d+>>      ArraySet [<<Array>>,<<Index>>,<<Value>>]
  /// CHECK-NEXT:                           mov [<<BaseReg>> + <<IndexReg>> * 4 + 12], 9

  /// CHECK-START-X86_64: void Main.testArrayLengthBoundsCheckX86(int[], int) x86_memory_operand_generation (before)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>] is_string_length:false loop:none
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<Value>>]

  /// CHECK-START-X86_64: void Main.testArrayLengthBoundsCheckX86(int[], int) x86_memory_operand_generation (after)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK-DAG:     <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-DAG:     <<Length:i\d+>>        ArrayLength [<<CheckedArray>>] is_string_length:false emitted_at_use:true loop:none
  /// CHECK-DAG:     <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-DAG:     <<ArraySet:v\d+>>      ArraySet [<<CheckedArray>>,<<CheckedIndex>>,<<Value>>]

  // Test assumes parameter value is in lower 8 registers (it is passed in edx).
  /// CHECK-START-X86_64: void Main.testArrayLengthBoundsCheckX86(int[], int) disassembly (after)
  /// CHECK-DAG:     <<Array:l\d+>>         ParameterValue
  /// CHECK-DAG:     <<Index:i\d+>>         ParameterValue
  /// CHECK-DAG:     <<Value:i\d+>>         IntConstant 9
  /// CHECK:         <<CheckedArray:l\d+>>  NullCheck [<<Array>>]
  /// CHECK-NEXT:    <<Length:i\d+>>        ArrayLength [<<Array>>] is_string_length:false emitted_at_use:true loop:none
  /// CHECK-NEXT:    <<CheckedIndex:i\d+>>  BoundsCheck [<<Index>>,<<Length>>]
  /// CHECK-NEXT:                           cmp [<<BaseReg:\w+>> + 8], e<<IndexReg:\w+>>
  /// CHECK:         <<ArraySet:v\d+>>      ArraySet [<<Array>>,<<Index>>,<<Value>>]
  /// CHECK-NEXT:                           mov [<<BaseReg>> + r<<IndexReg>> * 4 + 12], 9

  static void testArrayLengthBoundsCheckX86(int[] array, int index) {
    array[index] = 9;
  }
}
