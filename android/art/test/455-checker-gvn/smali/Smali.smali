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

## CHECK-START: int Smali.foo(int, int) GVN (before)
## CHECK: Add
## CHECK: Add
## CHECK: Add

## CHECK-START: int Smali.foo(int, int) GVN (after)
## CHECK: Add
## CHECK: Add
## CHECK-NOT: Add
.method public static foo(II)I

    # int sum1 = x + y;
    # int sum2 = y + x;
    # return sum1 + sum2;

    .registers 5
    .param p0, "x"    # I
    .param p1, "y"    # I

    add-int v0, p0, p1
    add-int v1, p1, p0
    add-int v2, v0, v1

    return v2
.end method

