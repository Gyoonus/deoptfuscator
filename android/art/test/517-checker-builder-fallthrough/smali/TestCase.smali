# Copyright (C) 2015 The Android Open Source Project
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

.class public LTestCase;

.super Ljava/lang/Object;

.field public static value:Z

## CHECK-START: int TestCase.testCase(int) builder (after)

## CHECK:  name            "B0"
## CHECK:  <<Const0:i\d+>> IntConstant 0

## CHECK:  name            "B1"
## CHECK:  successors      "B5" "B2"
## CHECK:  <<Cond:z\d+>>   Equal [<<Const0>>,<<Const0>>]
## CHECK:  If [<<Cond>>]

## CHECK:  name            "B2"
## CHECK:  successors      "B4"
## CHECK:  Goto

## CHECK:  name            "B3"
## CHECK:  Return

## CHECK:  name            "B4"
## CHECK:  successors      "B3"
## CHECK:  Goto

## CHECK:  name            "B5"
## CHECK:  successors      "B3"
## CHECK:  Goto

.method public static testCase(I)I
    .registers 2

    const/4 v0, 0
    packed-switch v0, :switch_data
    goto :default

    :switch_data
    .packed-switch 0x0
        :case
    .end packed-switch

    :return
    return v1

    :default
    goto :return

    :case
    goto :return

.end method
