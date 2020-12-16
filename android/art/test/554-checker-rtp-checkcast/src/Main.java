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

  public static Object returnIntArray() { return new int[10]; }

  /// CHECK-START: void Main.boundTypeForMergingPhi() builder (after)
  /// CHECK-DAG:              ArraySet [<<NC:l\d+>>,{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:     <<NC>>   NullCheck [<<Phi:l\d+>>]
  /// CHECK-DAG:     <<Phi>>  Phi klass:int[]

  public static void boundTypeForMergingPhi() {
    int[] array = new int[20];
    if (array.hashCode() > 5) {
      array = (int[]) returnIntArray();
    }
    array[0] = 14;
  }

  /// CHECK-START: void Main.boundTypeForLoopPhi() builder (after)
  /// CHECK-DAG:              ArraySet [<<NC:l\d+>>,{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:     <<NC>>   NullCheck [<<Phi:l\d+>>]
  /// CHECK-DAG:     <<Phi>>  Phi klass:int[]

  public static void boundTypeForLoopPhi() {
    int[] array = new int[20];
    int i = 0;
    while (i < 4) {
      ++i;
      array[i] = i;
      if (i > 2) {
        array = (int[]) returnIntArray();
      }
    }
    array[0] = 14;
  }

  /// CHECK-START: void Main.boundTypeForCatchPhi() builder (after)
  /// CHECK-DAG:              ArraySet [<<NC:l\d+>>,{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:     <<NC>>   NullCheck [<<Phi:l\d+>>]
  /// CHECK-DAG:     <<Phi>>  Phi is_catch_phi:true klass:int[]

  public static void boundTypeForCatchPhi() {
    int[] array1 = new int[20];
    int[] array2 = (int[]) returnIntArray();

    int[] catch_phi = array1;
    try {
      System.nanoTime();
      catch_phi = array2;
      System.nanoTime();
    } catch (Throwable ex) {
      catch_phi[0] = 14;
    }
  }

  public static void main(String[] args) {  }
}
