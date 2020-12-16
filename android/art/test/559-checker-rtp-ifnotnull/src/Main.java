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

  /// CHECK-START: void Main.boundTypeForIfNotNull() builder (after)
  /// CHECK-DAG:     <<Null:l\d+>>        NullConstant
  /// CHECK-DAG:     <<Cst5:i\d+>>        IntConstant 5
  /// CHECK-DAG:     <<Cst10:i\d+>>       IntConstant 10

  /// CHECK-DAG:                          InvokeVirtual [<<NullCheck:l\d+>>]
  /// CHECK-DAG:     <<NullCheck>>        NullCheck [<<LoopPhi:l\d+>>] klass:int[]
  /// CHECK-DAG:     <<LoopPhi>>          Phi [<<Null>>,<<MergePhi:l\d+>>] klass:int[]

  /// CHECK-DAG:     <<BoundType:l\d+>>   BoundType [<<LoopPhi>>] klass:int[] can_be_null:false
  /// CHECK-DAG:     <<LoadClass1:l\d+>>  LoadClass
  /// CHECK-DAG:     <<LoadClass2:l\d+>>  LoadClass
  /// CHECK-DAG:     <<NewArray10:l\d+>>  NewArray [<<LoadClass2>>,<<Cst10>>] klass:int[]
  /// CHECK-DAG:     <<NotNullPhi:l\d+>>  Phi [<<BoundType>>,<<NewArray10>>] klass:int[]

  /// CHECK-DAG:     <<NewArray5:l\d+>>   NewArray [<<LoadClass1>>,<<Cst5>>] klass:int[]
  /// CHECK-DAG:     <<MergePhi>>         Phi [<<NewArray5>>,<<NotNullPhi>>] klass:int[]

  public static void boundTypeForIfNotNull() {
    int[] array = null;
    for (int i = -1; i < 10; ++i) {
      if (array == null) {
        array = new int[5];
      } else {
        if (i == 5) {
          array = new int[10];
        }
        array[i] = i;
      }
    }
    array.hashCode();
  }

  public static void main(String[] args) {  }
}
