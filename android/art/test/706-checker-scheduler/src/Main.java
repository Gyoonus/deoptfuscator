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

  public class ExampleObj {
    int n1;
    int n2;

    public ExampleObj(int n1, int n2) {
      this.n1 = n1;
      this.n2 = n2;
    }
  }

  static int static_variable = 0;

  public ExampleObj my_obj;
  public static int number1;
  public static int number2;
  public static volatile int number3;

  /// CHECK-START-ARM64: int Main.arrayAccess() scheduler (before)
  /// CHECK:    <<Const1:i\d+>>       IntConstant 1
  /// CHECK:    <<i0:i\d+>>           Phi
  /// CHECK:    <<res0:i\d+>>         Phi
  /// CHECK:    <<Array:i\d+>>        IntermediateAddress
  /// CHECK:    <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<i0>>]
  /// CHECK:    <<res1:i\d+>>         Add [<<res0>>,<<ArrayGet1>>]
  /// CHECK:    <<i1:i\d+>>           Add [<<i0>>,<<Const1>>]
  /// CHECK:    <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<i1>>]
  /// CHECK:                          Add [<<res1>>,<<ArrayGet2>>]

  /// CHECK-START-ARM64: int Main.arrayAccess() scheduler (after)
  /// CHECK:    <<Const1:i\d+>>       IntConstant 1
  /// CHECK:    <<i0:i\d+>>           Phi
  /// CHECK:    <<res0:i\d+>>         Phi
  /// CHECK:    <<Array:i\d+>>        IntermediateAddress
  /// CHECK:    <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<i0>>]
  /// CHECK:    <<i1:i\d+>>           Add [<<i0>>,<<Const1>>]
  /// CHECK:    <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<i1>>]
  /// CHECK:    <<res1:i\d+>>         Add [<<res0>>,<<ArrayGet1>>]
  /// CHECK:                          Add [<<res1>>,<<ArrayGet2>>]

  public static int arrayAccess() {
    int res = 0;
    int [] array = new int[10];
    for (int i = 0; i < 9; i++) {
      res += array[i];
      res += array[i + 1];
    }
    return res;
  }

  /// CHECK-START-ARM: void Main.arrayAccessVariable(int) scheduler (before)
  /// CHECK:     <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Const3:i\d+>>       IntConstant -1
  /// CHECK:     <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:     <<Add2:i\d+>>         Add [<<Param>>,<<Const2>>]
  /// CHECK:     <<Add3:i\d+>>         Add [<<Param>>,<<Const3>>]
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:     <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const1>>]
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Add2>>]
  /// CHECK:     <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const1>>]
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Add2>>,<<AddArray2>>]
  /// CHECK:     <<ArrayGet3:i\d+>>    ArrayGet [<<Array>>,<<Add3>>]
  /// CHECK:     <<AddArray3:i\d+>>    Add [<<ArrayGet3>>,<<Const1>>]
  /// CHECK:     <<ArraySet3:v\d+>>    ArraySet [<<Array>>,<<Add3>>,<<AddArray3>>]

  /// CHECK-START-ARM: void Main.arrayAccessVariable(int) scheduler (after)
  /// CHECK:     <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Const3:i\d+>>       IntConstant -1
  /// CHECK:     <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:     <<Add2:i\d+>>         Add [<<Param>>,<<Const2>>]
  /// CHECK:     <<Add3:i\d+>>         Add [<<Param>>,<<Const3>>]
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           Add
  /// CHECK:                           Add
  /// CHECK:                           Add
  /// CHECK:                           ArraySet
  /// CHECK:                           ArraySet
  /// CHECK:                           ArraySet

  /// CHECK-START-ARM64: void Main.arrayAccessVariable(int) scheduler (before)
  /// CHECK:     <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Const3:i\d+>>       IntConstant -1
  /// CHECK:     <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:     <<Add2:i\d+>>         Add [<<Param>>,<<Const2>>]
  /// CHECK:     <<Add3:i\d+>>         Add [<<Param>>,<<Const3>>]
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:     <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const1>>]
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Add2>>]
  /// CHECK:     <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const1>>]
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Add2>>,<<AddArray2>>]
  /// CHECK:     <<ArrayGet3:i\d+>>    ArrayGet [<<Array>>,<<Add3>>]
  /// CHECK:     <<AddArray3:i\d+>>    Add [<<ArrayGet3>>,<<Const1>>]
  /// CHECK:     <<ArraySet3:v\d+>>    ArraySet [<<Array>>,<<Add3>>,<<AddArray3>>]

  /// CHECK-START-ARM64: void Main.arrayAccessVariable(int) scheduler (after)
  /// CHECK:     <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Const3:i\d+>>       IntConstant -1
  /// CHECK:     <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:     <<Add2:i\d+>>         Add [<<Param>>,<<Const2>>]
  /// CHECK:     <<Add3:i\d+>>         Add [<<Param>>,<<Const3>>]
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           ArrayGet [<<Array>>,{{i\d+}}]
  /// CHECK:                           Add
  /// CHECK:                           Add
  /// CHECK:                           Add
  /// CHECK:                           ArraySet
  /// CHECK:                           ArraySet
  /// CHECK:                           ArraySet
  public static void arrayAccessVariable(int i) {
    int [] array = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int j = 0; j < 100; j++) {
      array[i + 1]++;
      array[i + 2]++;
      array[i - 1]++;
    }
  }

  /// CHECK-START-ARM: void Main.arrayAccessSub(int) scheduler (before)
  /// CHECK:      <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG:  <<Const1:i\d+>>       IntConstant -1
  /// CHECK-DAG:  <<Const2:i\d+>>       IntConstant 9
  /// CHECK-DAG:  <<Const3:i\d+>>       IntConstant 1
  /// CHECK:      <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:      <<Sub2:i\d+>>         Sub [<<Const2>>,<<Param>>]
  /// CHECK:      <<Array:i\d+>>        IntermediateAddress
  /// CHECK:      <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:      <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const3>>]
  /// CHECK:      <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:      <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Sub2>>]
  /// CHECK:      <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const3>>]
  /// CHECK:      <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Sub2>>,<<AddArray2>>]

  /// CHECK-START-ARM: void Main.arrayAccessSub(int) scheduler (after)
  /// CHECK:      <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG:  <<Const1:i\d+>>       IntConstant -1
  /// CHECK-DAG:  <<Const2:i\d+>>       IntConstant 9
  /// CHECK-DAG:  <<Const3:i\d+>>       IntConstant 1
  /// CHECK:      <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:      <<Sub2:i\d+>>         Sub [<<Const2>>,<<Param>>]
  /// CHECK:      <<Array:i\d+>>        IntermediateAddress
  /// CHECK:      <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:      <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const3>>]
  /// CHECK:      <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:      <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Sub2>>]
  /// CHECK:      <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const3>>]
  /// CHECK:      <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Sub2>>,<<AddArray2>>]

  /// CHECK-START-ARM64: void Main.arrayAccessSub(int) scheduler (before)
  /// CHECK:      <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG:  <<Const1:i\d+>>       IntConstant -1
  /// CHECK-DAG:  <<Const2:i\d+>>       IntConstant 9
  /// CHECK-DAG:  <<Const3:i\d+>>       IntConstant 1
  /// CHECK:      <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:      <<Sub2:i\d+>>         Sub [<<Const2>>,<<Param>>]
  /// CHECK:      <<Array:i\d+>>        IntermediateAddress
  /// CHECK:      <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:      <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const3>>]
  /// CHECK:      <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:      <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Sub2>>]
  /// CHECK:      <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const3>>]
  /// CHECK:      <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Sub2>>,<<AddArray2>>]

  /// CHECK-START-ARM64: void Main.arrayAccessSub(int) scheduler (after)
  /// CHECK:      <<Param:i\d+>>        ParameterValue
  /// CHECK-DAG:  <<Const1:i\d+>>       IntConstant -1
  /// CHECK-DAG:  <<Const2:i\d+>>       IntConstant 9
  /// CHECK-DAG:  <<Const3:i\d+>>       IntConstant 1
  /// CHECK:      <<Add1:i\d+>>         Add [<<Param>>,<<Const1>>]
  /// CHECK:      <<Sub2:i\d+>>         Sub [<<Const2>>,<<Param>>]
  /// CHECK:      <<Array:i\d+>>        IntermediateAddress
  /// CHECK:      <<ArrayGet1:i\d+>>    ArrayGet [<<Array>>,<<Add1>>]
  /// CHECK:      <<AddArray1:i\d+>>    Add [<<ArrayGet1>>,<<Const3>>]
  /// CHECK:      <<ArraySet1:v\d+>>    ArraySet [<<Array>>,<<Add1>>,<<AddArray1>>]
  /// CHECK:      <<ArrayGet2:i\d+>>    ArrayGet [<<Array>>,<<Sub2>>]
  /// CHECK:      <<AddArray2:i\d+>>    Add [<<ArrayGet2>>,<<Const3>>]
  /// CHECK:      <<ArraySet2:v\d+>>    ArraySet [<<Array>>,<<Sub2>>,<<AddArray2>>]
  public static void arrayAccessSub(int i) {
    int [] array = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int j = 0; j < 100; j++) {
      // These two accesses MAY ALIAS
      array[i - 1]++;
      array[9 - i]++;
    }
  }

  /// CHECK-START-ARM: void Main.arrayAccessLoopVariable() scheduler (before)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK:     <<Phi:i\d+>>          Phi
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray1:i\d+>>    Add
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet
  /// CHECK:     <<AddVar:i\d+>>       Add
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray2:i\d+>>    Add
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet

  /// CHECK-START-ARM: void Main.arrayAccessLoopVariable() scheduler (after)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK:     <<Phi:i\d+>>          Phi
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<AddVar:i\d+>>       Add
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray1:i\d+>>    Add
  /// CHECK:     <<AddArray2:i\d+>>    Add
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet

  /// CHECK-START-ARM64: void Main.arrayAccessLoopVariable() scheduler (before)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK:     <<Phi:i\d+>>          Phi
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray1:i\d+>>    Add
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet
  /// CHECK:     <<AddVar:i\d+>>       Add
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray2:i\d+>>    Add
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet

  /// CHECK-START-ARM64: void Main.arrayAccessLoopVariable() scheduler (after)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK:     <<Phi:i\d+>>          Phi
  /// CHECK:     <<Array:i\d+>>        IntermediateAddress
  /// CHECK:     <<AddVar:i\d+>>       Add
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet
  /// CHECK:     <<AddArray1:i\d+>>    Add
  /// CHECK:     <<AddArray2:i\d+>>    Add
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet
  public static void arrayAccessLoopVariable() {
    int [] array = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    for (int j = 0; j < 9; j++) {
      array[j]++;
      array[j + 1]++;
    }
  }

  // This case tests a bug found in LSA where LSA doesn't understand IntermediateAddress,
  // and incorrectly reported no alias between ArraySet1 and ArrayGet2,
  // thus ArrayGet2 is scheduled above ArraySet1 incorrectly.

  /// CHECK-START-ARM64: void Main.CrossOverLoop(int[], int[]) scheduler (before)
  /// CHECK:     <<ParamA:l\d+>>       ParameterValue                           loop:none
  /// CHECK:     <<ParamB:l\d+>>       ParameterValue                           loop:none
  /// CHECK:     <<NullB:l\d+>>        NullCheck [<<ParamB>>]                   loop:none
  /// CHECK:     <<NullA:l\d+>>        NullCheck [<<ParamA>>]                   loop:none
  /// CHECK:                           Phi                                      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet [<<NullB>>,{{i\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<Addr1:i\d+>>        IntermediateAddress [<<NullA>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet [<<Addr1>>,{{i\d+}},{{i\d+}}]   loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet [<<NullB>>,{{i\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<Addr2:i\d+>>        IntermediateAddress [<<NullA>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet [<<Addr2>>,{{i\d+}},{{i\d+}}]   loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.CrossOverLoop(int[], int[]) scheduler (after)
  /// CHECK:     <<ParamA:l\d+>>       ParameterValue                           loop:none
  /// CHECK:     <<ParamB:l\d+>>       ParameterValue                           loop:none
  /// CHECK:     <<NullB:l\d+>>        NullCheck [<<ParamB>>]                   loop:none
  /// CHECK:     <<NullA:l\d+>>        NullCheck [<<ParamA>>]                   loop:none
  /// CHECK:                           Phi                                      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     <<ArrayGet1:i\d+>>    ArrayGet [<<NullB>>,{{i\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<Addr1:i\d+>>        IntermediateAddress [<<NullA>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArraySet1:v\d+>>    ArraySet [<<Addr1>>,{{i\d+}},{{i\d+}}]   loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArrayGet2:i\d+>>    ArrayGet [<<NullB>>,{{i\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<Addr2:i\d+>>        IntermediateAddress [<<NullA>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK:     <<ArraySet2:v\d+>>    ArraySet [<<Addr2>>,{{i\d+}},{{i\d+}}]   loop:<<Loop>>      outer_loop:none
  /// CHECK:                           Add                                      loop:<<Loop>>      outer_loop:none
  private static void CrossOverLoop(int a[], int b[]) {
    b[20] = 99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
      i++;
      a[i] = b[20] - 7;
    }
  }

  // This test case is similar to above cross over loop,
  // but has more complex chains of transforming the original references:
  // ParameterValue --> BoundType --> NullCheck --> ArrayGet.
  // ParameterValue --> BoundType --> NullCheck --> IntermediateAddress --> ArraySet.
  // After using LSA to analyze the orginal references, the scheduler should be able
  // to find out that 'a' and 'b' may alias, hence unable to schedule these ArraGet/Set.

  /// CHECK-START-ARM64: void Main.CrossOverLoop2(java.lang.Object, java.lang.Object) scheduler (before)
  /// CHECK:  Phi        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:  ArrayGet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  Add        loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArraySet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArrayGet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  Add        loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArraySet   loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.CrossOverLoop2(java.lang.Object, java.lang.Object) scheduler (after)
  /// CHECK:  Phi        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:  ArrayGet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  Add        loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArraySet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArrayGet   loop:<<Loop>>      outer_loop:none
  /// CHECK:  Add        loop:<<Loop>>      outer_loop:none
  /// CHECK:  ArraySet   loop:<<Loop>>      outer_loop:none
  private static void CrossOverLoop2(Object a, Object b) {
    ((int[])b)[20] = 99;
    for (int i = 0; i < ((int[])a).length; i++) {
      ((int[])a)[i] = ((int[])b)[20] - 7;
      i++;
      ((int[])a)[i] = ((int[])b)[20] - 7;
    }
  }

  /// CHECK-START-ARM: void Main.accessFields() scheduler (before)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet

  /// CHECK-START-ARM: void Main.accessFields() scheduler (after)
  /// CHECK-DAG:        InstanceFieldGet
  /// CHECK-DAG:        InstanceFieldGet
  /// CHECK-DAG:        StaticFieldGet
  /// CHECK-DAG:        StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK-DAG:        InstanceFieldSet
  /// CHECK-DAG:        InstanceFieldSet
  /// CHECK-DAG:        StaticFieldSet
  /// CHECK-DAG:        StaticFieldSet

  /// CHECK-START-ARM64: void Main.accessFields() scheduler (before)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet

  /// CHECK-START-ARM64: void Main.accessFields() scheduler (after)
  /// CHECK-DAG:        InstanceFieldGet
  /// CHECK-DAG:        InstanceFieldGet
  /// CHECK-DAG:        StaticFieldGet
  /// CHECK-DAG:        StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK:            Add
  /// CHECK-DAG:        InstanceFieldSet
  /// CHECK-DAG:        InstanceFieldSet
  /// CHECK-DAG:        StaticFieldSet
  /// CHECK-DAG:        StaticFieldSet
  public void accessFields() {
    my_obj = new ExampleObj(1, 2);
    for (int i = 0; i < 10; i++) {
      my_obj.n1++;
      my_obj.n2++;
      number1++;
      number2++;
    }
  }

  /// CHECK-START-ARM: void Main.accessFieldsVolatile() scheduler (before)
  /// CHECK-START-ARM64: void Main.accessFieldsVolatile() scheduler (before)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet

  /// CHECK-START-ARM: void Main.accessFieldsVolatile() scheduler (after)
  /// CHECK-START-ARM64: void Main.accessFieldsVolatile() scheduler (after)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet
  /// CHECK:            StaticFieldGet
  /// CHECK:            Add
  /// CHECK:            StaticFieldSet

  public void accessFieldsVolatile() {
    my_obj = new ExampleObj(1, 2);
    for (int i = 0; i < 10; i++) {
      my_obj.n1++;
      my_obj.n2++;
      number1++;
      number3++;
    }
  }

  /// CHECK-START-ARM: void Main.accessFieldsUnresolved() scheduler (before)
  /// CHECK-START-ARM64: void Main.accessFieldsUnresolved() scheduler (before)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            UnresolvedInstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            UnresolvedInstanceFieldSet
  /// CHECK:            UnresolvedStaticFieldGet
  /// CHECK:            Add
  /// CHECK:            UnresolvedStaticFieldSet

  /// CHECK-START-ARM: void Main.accessFieldsUnresolved() scheduler (after)
  /// CHECK-START-ARM64: void Main.accessFieldsUnresolved() scheduler (after)
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            InstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            InstanceFieldSet
  /// CHECK:            UnresolvedInstanceFieldGet
  /// CHECK:            Add
  /// CHECK:            UnresolvedInstanceFieldSet
  /// CHECK:            UnresolvedStaticFieldGet
  /// CHECK:            Add
  /// CHECK:            UnresolvedStaticFieldSet

  public void accessFieldsUnresolved() {
    my_obj = new ExampleObj(1, 2);
    UnresolvedClass unresolved_obj = new UnresolvedClass();
    for (int i = 0; i < 10; i++) {
      my_obj.n1++;
      my_obj.n2++;
      unresolved_obj.instanceInt++;
      UnresolvedClass.staticInt++;
    }
  }

  /// CHECK-START-ARM64: int Main.intDiv(int) scheduler (before)
  /// CHECK:               Sub
  /// CHECK:               DivZeroCheck
  /// CHECK:               Div
  /// CHECK:               StaticFieldSet

  /// CHECK-START-ARM64: int Main.intDiv(int) scheduler (after)
  /// CHECK:               Sub
  /// CHECK-NOT:           StaticFieldSet
  /// CHECK:               DivZeroCheck
  /// CHECK-NOT:           Sub
  /// CHECK:               Div
  public static int intDiv(int arg) {
    int res = 0;
    int tmp = arg;
    for (int i = 1; i < arg; i++) {
      tmp -= i;
      res = res / i;  // div-zero check barrier.
      static_variable++;
    }
    res += tmp;
    return res;
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static final int ARRAY_SIZE = 32;

  // Check that VecReplicateScalar is not reordered.
  /// CHECK-START-ARM64: void Main.testVecReplicateScalar() scheduler (before)
  /// CHECK:     Phi                loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     NewArray           loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecReplicateScalar loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.testVecReplicateScalar() scheduler (after)
  /// CHECK:     Phi                loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     NewArray           loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecReplicateScalar loop:<<Loop>>      outer_loop:none
  private static void testVecReplicateScalar() {
    for (int j = 0; j <= 8; j++) {
      int[] a = new int[ARRAY_SIZE];
      for (int i = 0; i < a.length; i++) {
        a[i] += 1;
      }
      for (int i = 0; i < a.length; i++) {
        expectEquals(1, a[i]);
      }
    }
  }

  // Check that VecSetScalars, VecReduce, VecExtractScalar are not reordered.
  /// CHECK-START-ARM64: void Main.testVecSetScalars() scheduler (before)
  /// CHECK:     Phi                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     NewArray             loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecSetScalars        loop:<<Loop>>      outer_loop:none
  //
  /// CHECK:     VecReduce            loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecExtractScalar     loop:<<Loop>>      outer_loop:none
  /// CHECK:     InvokeStaticOrDirect loop:<<Loop>>      outer_loop:none
  /// CHECK:     InvokeStaticOrDirect loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.testVecSetScalars() scheduler (after)
  /// CHECK:     Phi                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK:     NewArray             loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecSetScalars        loop:<<Loop>>      outer_loop:none
  //
  /// CHECK:     VecReduce            loop:<<Loop>>      outer_loop:none
  /// CHECK:     VecExtractScalar     loop:<<Loop>>      outer_loop:none
  /// CHECK:     InvokeStaticOrDirect loop:<<Loop>>      outer_loop:none
  /// CHECK:     InvokeStaticOrDirect loop:<<Loop>>      outer_loop:none
  private static void testVecSetScalars() {
    for (int j = 0; j <= 8; j++) {
      int[] a = new int[ARRAY_SIZE];
      int s = 5;
      for (int i = 0; i < ARRAY_SIZE; i++) {
        s+=a[i];
      }
      expectEquals(a[0], 0);
      expectEquals(s, 5);
    }
  }

  public static void main(String[] args) {
    testVecSetScalars();
    testVecReplicateScalar();
    if ((arrayAccess() + intDiv(10)) != -35) {
      System.out.println("FAIL");
    }
  }
}
