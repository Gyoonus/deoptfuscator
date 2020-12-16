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

.class public LSmaliTests;
.super Ljava/lang/Object;

# A very particular set of operations that caused a double removal by the
#  ARM64 simplifier doing "forward" removals (b/27851582).

## CHECK-START-ARM: int SmaliTests.operations() instruction_simplifier_arm (before)
## CHECK-DAG: <<Get:i\d+>> ArrayGet
## CHECK-DAG: <<Not:i\d+>> Not [<<Get>>]
## CHECK-DAG: <<Shl:i\d+>> Shl [<<Get>>,i{{\d+}}]
## CHECK-DAG:              And [<<Not>>,<<Shl>>]

## CHECK-START-ARM: int SmaliTests.operations() instruction_simplifier_arm (after)
## CHECK-DAG: <<Get:i\d+>> ArrayGet
## CHECK-DAG: <<Not:i\d+>> Not [<<Get>>]
## CHECK-DAG:              DataProcWithShifterOp [<<Not>>,<<Get>>] kind:And+LSL shift:2

## CHECK-START-ARM64: int SmaliTests.operations() instruction_simplifier_arm64 (before)
## CHECK-DAG: <<Get:i\d+>> ArrayGet
## CHECK-DAG: <<Not:i\d+>> Not [<<Get>>]
## CHECK-DAG: <<Shl:i\d+>> Shl [<<Get>>,i{{\d+}}]
## CHECK-DAG:              And [<<Not>>,<<Shl>>]

## CHECK-START-ARM64: int SmaliTests.operations() instruction_simplifier_arm64 (after)
## CHECK-DAG: <<Get:i\d+>> ArrayGet
## CHECK-DAG: <<Not:i\d+>> Not [<<Get>>]
## CHECK-DAG:              DataProcWithShifterOp [<<Not>>,<<Get>>] kind:And+LSL shift:2
.method public static operations()I
    .registers 6
    .prologue

    # int r = a[0];
    sget-object v4, LMain;->a:[I
    const/4 v5, 0x0
    aget v2, v4, v5
    # int n = ~r;
    not-int v1, v2
    # int s = r << 2;
    shl-int/lit8 v3, v2, 0x2
    # int a = s & n;
    and-int v0, v3, v1
    # return a
    return v0
.end method
