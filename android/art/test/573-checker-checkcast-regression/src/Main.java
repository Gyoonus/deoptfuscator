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
    Object[] array = { new Integer(1), new Integer(2), new Integer(3) };
    int result = test(array, 0, 2);
    System.out.println(result);
  }

  // This test method uses two integers (`index1` and `index2`) to
  // force the register allocator to use some high registers (R8-R15)
  // on x86-64 in the code generated for the first CheckCast (which
  // converts `new_array` to an `Object[]`), so as to produce code
  // containing a conditional jump whose offset does not fit in a
  // NearLabel when using Baker's read barrier fast path (because
  // x86-64 instructions using these high registers have a larger
  // encoding).
  //
  // The intent of this artifical constraint is to ensure the initial
  // failure is properly tested by this regression test.

  /// CHECK-START: int Main.test(java.lang.Object, int, int) register (after)
  /// CHECK-DAG:     CheckCast check_kind:array_object_check
  /// CHECK-DAG:     CheckCast check_kind:exact_check
  /// CHECK-DAG:     CheckCast check_kind:exact_check

  static public int test(Object new_array, int index1, int index2) {
    Object[] objectArray = (Object[]) new_array;
    Integer integer1 = (Integer) objectArray[index1];
    Integer integer2 = (Integer) objectArray[index2];
    return integer1.intValue() + integer2.intValue();
  }

}
