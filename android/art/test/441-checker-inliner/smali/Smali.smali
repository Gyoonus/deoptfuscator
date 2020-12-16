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
.source "Smali.java"

##  CHECK-START: int Smali.InlineWithControlFlow(boolean) inliner (before)
##  CHECK-DAG:     <<Const1:i\d+>> IntConstant 1
##  CHECK-DAG:     <<Const3:i\d+>> IntConstant 3
##  CHECK-DAG:     <<Const5:i\d+>> IntConstant 5
##  CHECK-DAG:     <<Add:i\d+>>    InvokeStaticOrDirect [<<Const1>>,<<Const3>>{{(,[ij]\d+)?}}]
##  CHECK-DAG:     <<Sub:i\d+>>    InvokeStaticOrDirect [<<Const5>>,<<Const3>>{{(,[ij]\d+)?}}]
##  CHECK-DAG:     <<Phi:i\d+>>    Phi [<<Add>>,<<Sub>>]
##  CHECK-DAG:                     Return [<<Phi>>]

##  CHECK-START: int Smali.InlineWithControlFlow(boolean) inliner (after)
##  CHECK-DAG:     <<Const4:i\d+>> IntConstant 4
##  CHECK-DAG:     <<Const2:i\d+>> IntConstant 2
##  CHECK-DAG:     <<Phi:i\d+>>    Phi [<<Const4>>,<<Const2>>]
##  CHECK-DAG:                     Return [<<Phi>>]
.method public static InlineWithControlFlow(Z)I

    # int x, const1, const3, const5;
    # const1 = 1;
    # const3 = 3;
    # const5 = 5;
    # if (cond) {
    #   x = returnAdd(const1, const3);
    # } else {
    #   x = returnSub(const5, const3);
    # }
    # return x;

    .registers 5
    .param p0, "cond"    # Z

    .prologue
    const/4 v0, 0x1

    .local v0, "const1":I
    const/4 v1, 0x3

    .local v1, "const3":I
    const/4 v2, 0x5

    .local v2, "const5":I
    if-eqz p0, :cond_a

    invoke-static {v0, v1}, LSmali;->returnAdd(II)I

    move-result v3

    .local v3, "x":I
    :goto_9
    return v3

    .end local v3    # "x":I
    :cond_a
    invoke-static {v2, v1}, LSmali;->returnSub(II)I

    move-result v3

    .restart local v3    # "x":I
    goto :goto_9
.end method

.method private static returnAdd(II)I
    .registers 3
    .param p0, "a"    # I
    .param p1, "b"    # I

    add-int v0, p0, p1

    return v0
.end method

.method private static returnSub(II)I
    .registers 3
    .param p0, "a"    # I
    .param p1, "b"    # I

    sub-int v0, p0, p1

    return v0
.end method
