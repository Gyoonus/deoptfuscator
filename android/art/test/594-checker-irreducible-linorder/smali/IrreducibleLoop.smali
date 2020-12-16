# Copyright (C) 2016 The Android Open Source Project
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

.class public LIrreducibleLoop;
.super Ljava/lang/Object;

# Test case where liveness analysis produces linear order where loop blocks are
# not adjacent.

## CHECK-START: int IrreducibleLoop.liveness(boolean, boolean, boolean, int) builder (after)
## CHECK-DAG:     Add loop:none
## CHECK-DAG:     Mul loop:<<Loop:B\d+>>
## CHECK-DAG:     Not loop:<<Loop>>

## CHECK-START: int IrreducibleLoop.liveness(boolean, boolean, boolean, int) liveness (after)
## CHECK-DAG:     Add liveness:<<LPreEntry:\d+>>
## CHECK-DAG:     Mul liveness:<<LHeader:\d+>>
## CHECK-DAG:     Not liveness:<<LBackEdge:\d+>>
## CHECK-EVAL:    (<<LHeader>> < <<LPreEntry>>) and (<<LPreEntry>> < <<LBackEdge>>)

.method public static liveness(ZZZI)I
   .registers 10
   const/16 v0, 42

   if-eqz p0, :header

   :pre_entry
   add-int/2addr p3, p3
   invoke-static {v0}, Ljava/lang/System;->exit(I)V
   goto :body1

   # Trivially dead code to ensure linear order verification skips removed blocks (b/28252537).
   :dead_code
   nop
   goto :dead_code

   :header
   mul-int/2addr p3, p3
   if-eqz p1, :body2

   :body1
   goto :body_merge

   :body2
   invoke-static {v0}, Ljava/lang/System;->exit(I)V
   goto :body_merge

   :body_merge
   if-eqz p2, :exit

   :back_edge
   not-int p3, p3
   goto :header

   :exit
   return p3

.end method

## CHECK-START: int IrreducibleLoop.liveness2(boolean, boolean, boolean, int) builder (after)
## CHECK-DAG:     Mul loop:<<Loop:B\d+>>
## CHECK-DAG:     Not loop:<<Loop>>

## CHECK-START: int IrreducibleLoop.liveness2(boolean, boolean, boolean, int) liveness (after)
## CHECK-DAG:     Mul liveness:<<LPreEntry2:\d+>>
## CHECK-DAG:     Not liveness:<<LBackEdge1:\d+>>
## CHECK-EVAL:    <<LBackEdge1>> < <<LPreEntry2>>

.method public liveness2(ZZZI)I
    .registers 10

    const v1, 1

    :header1
    if-eqz p0, :body1

    :exit
    return p3

    :body1
    # The test will generate an incorrect linear order when the following IF swaps
    # its successors. To do that, load a boolean value and compare NotEqual to 1.
    sget-boolean v2, LIrreducibleLoop;->f:Z
    const v3, 1
    if-ne v2, v3, :pre_header2

    :pre_entry2
    # This constant has a use in a phi in :back_edge2 and a back edge use in
    # :back_edge1. Because the linear order is wrong, the back edge use has
    # a lower liveness than the phi use.
    const v0, 42
    mul-int/2addr p3, p3
    goto :back_edge2

    :back_edge2
    add-int/2addr p3, v0
    add-int/2addr v0, v1
    goto :header2

    :header2
    if-eqz p2, :back_edge2

    :back_edge1
    not-int p3, p3
    goto :header1

    :pre_header2
    const v0, 42
    goto :header2
.end method

.field public static f:Z
