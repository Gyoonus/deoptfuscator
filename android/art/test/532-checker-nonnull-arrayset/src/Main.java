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

  // Check that we don't put a null check in the card marking code.

  /// CHECK-START: void Main.test() instruction_simplifier (before)
  /// CHECK:          ArraySet value_can_be_null:true

  /// CHECK-START: void Main.test() instruction_simplifier (after)
  /// CHECK:          ArraySet value_can_be_null:false

  /// CHECK-START-X86: void Main.test() disassembly (after)
  /// CHECK:          ArraySet value_can_be_null:false
  /// CHECK-NOT:      test
  /// CHECK:          ReturnVoid
  public static void test() {
    Object[] array = sArray;
    Object nonNull = array[0];
    nonNull.getClass(); // Ensure nonNull has an implicit null check.
    array[1] = nonNull;
  }

  public static void main(String[] args) {}

  static Object[] sArray;
}
