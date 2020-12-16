# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LSmali;
.super Ljava/lang/Object;

## CHECK-START: void Smali.stencilSubInt(int[], int[], int) loop_optimization (before)
## CHECK-DAG: <<PAR3:i\d+>>  ParameterValue                       loop:none
## CHECK-DAG: <<CP1:i\d+>>   IntConstant 1                        loop:none
## CHECK-DAG: <<Sub1:i\d+>>  Sub [<<PAR3>>,<<CP1>>]               loop:none
## CHECK-DAG: <<Phi:i\d+>>   Phi                                  loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<Sub2:i\d+>>  Sub [<<Phi>>,<<CP1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get1:i\d+>>  ArrayGet [{{l\d+}},<<Sub2>>]         loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get2:i\d+>>  ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add1:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add2:i\d+>>  Add [<<Phi>>,<<CP1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get3:i\d+>>  ArrayGet [{{l\d+}},<<Add2>>]         loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add3:i\d+>>  Add [<<Add1>>,<<Get3>>]              loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                ArraySet [{{l\d+}},<<Phi>>,<<Add3>>] loop:<<Loop>>      outer_loop:none

## CHECK-START-ARM64: void Smali.stencilSubInt(int[], int[], int) loop_optimization (after)
## CHECK-DAG: <<CP1:i\d+>>   IntConstant 1                         loop:none
## CHECK-DAG: <<CP2:i\d+>>   IntConstant 2                         loop:none
## CHECK-DAG: <<Phi:i\d+>>   Phi                                   loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<Add1:i\d+>>  Add [<<Phi>>,<<CP1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get1:d\d+>>  VecLoad [{{l\d+}},<<Phi>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get2:d\d+>>  VecLoad [{{l\d+}},<<Add1>>]           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Get1>>,<<Get2>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add3:i\d+>>  Add [<<Phi>>,<<CP2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get3:d\d+>>  VecLoad [{{l\d+}},<<Add3>>]           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add4:d\d+>>  VecAdd [<<Add2>>,<<Get3>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                VecStore [{{l\d+}},<<Add1>>,<<Add4>>] loop:<<Loop>>      outer_loop:none
.method public static stencilSubInt([I[II)V
    .registers 7

    const/4 v0, 0x1

    move v1, v0

    :goto_2
    sub-int v2, p2, v0

    if-ge v1, v2, :cond_17

    sub-int v2, v1, v0
    aget v2, p1, v2
    aget v3, p1, v1
    add-int/2addr v2, v3
    add-int v3, v1, v0
    aget v3, p1, v3
    add-int/2addr v2, v3
    aput v2, p0, v1
    add-int/lit8 v1, v1, 0x1

    goto :goto_2

    :cond_17
    return-void
.end method

## CHECK-START: void Smali.stencilAddInt(int[], int[], int) loop_optimization (before)
## CHECK-DAG: <<CP1:i\d+>>   IntConstant 1                        loop:none
## CHECK-DAG: <<CM1:i\d+>>   IntConstant -1                       loop:none
## CHECK-DAG: <<Phi:i\d+>>   Phi                                  loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<Add1:i\d+>>  Add [<<Phi>>,<<CM1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get1:i\d+>>  ArrayGet [{{l\d+}},<<Add1>>]         loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get2:i\d+>>  ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add2:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add3:i\d+>>  Add [<<Phi>>,<<CP1>>]                loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get3:i\d+>>  ArrayGet [{{l\d+}},<<Add3>>]         loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add4:i\d+>>  Add [<<Add2>>,<<Get3>>]              loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                ArraySet [{{l\d+}},<<Phi>>,<<Add4>>] loop:<<Loop>>      outer_loop:none

## CHECK-START-ARM64: void Smali.stencilAddInt(int[], int[], int) loop_optimization (after)
## CHECK-DAG: <<CP1:i\d+>>   IntConstant 1                         loop:none
## CHECK-DAG: <<CP2:i\d+>>   IntConstant 2                         loop:none
## CHECK-DAG: <<Phi:i\d+>>   Phi                                   loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG: <<Add1:i\d+>>  Add [<<Phi>>,<<CP1>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get1:d\d+>>  VecLoad [{{l\d+}},<<Phi>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get2:d\d+>>  VecLoad [{{l\d+}},<<Add1>>]           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Get1>>,<<Get2>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add3:i\d+>>  Add [<<Phi>>,<<CP2>>]                 loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Get3:d\d+>>  VecLoad [{{l\d+}},<<Add3>>]           loop:<<Loop>>      outer_loop:none
## CHECK-DAG: <<Add4:d\d+>>  VecAdd [<<Add2>>,<<Get3>>]            loop:<<Loop>>      outer_loop:none
## CHECK-DAG:                VecStore [{{l\d+}},<<Add1>>,<<Add4>>] loop:<<Loop>>      outer_loop:none
.method public static stencilAddInt([I[II)V
    .registers 6

    const/4 v0, 0x1

    :goto_1
    add-int/lit8 v1, p2, -0x1

    if-ge v0, v1, :cond_16

    add-int/lit8 v1, v0, -0x1
    aget v1, p1, v1
    aget v2, p1, v0
    add-int/2addr v1, v2
    add-int/lit8 v2, v0, 0x1
    aget v2, p1, v2
    add-int/2addr v1, v2
    aput v1, p0, v0
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    :cond_16
    return-void
.end method
