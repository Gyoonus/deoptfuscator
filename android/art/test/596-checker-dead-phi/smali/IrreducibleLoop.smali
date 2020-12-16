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
# not adjacent. This revealed a bug in our SSA builder, where a dead loop phi would
# be replaced by its incoming input during SsaRedundantPhiElimination.

# Check that the outer loop suspend check environment only has the parameter vreg.
## CHECK-START: int IrreducibleLoop.liveness(int) builder (after)
## CHECK-DAG:     <<Phi:i\d+>> Phi reg:4 loop:{{B\d+}} irreducible:false
## CHECK-DAG:     SuspendCheck env:[[_,_,_,_,<<Phi>>]] loop:{{B\d+}} irreducible:false

# Check that the linear order has non-adjacent loop blocks.
## CHECK-START: int IrreducibleLoop.liveness(int) liveness (after)
## CHECK-DAG:     Mul liveness:<<LPreEntry2:\d+>>
## CHECK-DAG:     Add liveness:<<LBackEdge1:\d+>>
## CHECK-EVAL:    <<LBackEdge1>> < <<LPreEntry2>>

.method public static liveness(I)I
    .registers 5

    const-string v1, "MyString"

    :header1
    if-eqz p0, :body1

    :exit
    return p0

    :body1
    # The test will generate an incorrect linear order when the following IF swaps
    # its successors. To do that, load a boolean value and compare NotEqual to 1.
    sget-boolean v2, LIrreducibleLoop;->f:Z
    const v3, 1
    if-ne v2, v3, :pre_header2

    :pre_entry2
    # Add a marker on the irreducible loop entry.
    mul-int/2addr p0, p0
    goto :back_edge2

    :back_edge2
    goto :header2

    :header2
    if-eqz p0, :back_edge2

    :back_edge1
    # Add a marker on the outer loop back edge.
    add-int/2addr p0, p0
    # Set a wide register, to have v1 undefined at the back edge.
    const-wide/16 v0, 0x1
    goto :header1

    :pre_header2
    goto :header2
.end method

.field public static f:Z
