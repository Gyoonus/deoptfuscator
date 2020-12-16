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

## CHECK-START: boolean TestCase.testCase() select_generator (before)
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Value:z\d+>>    StaticFieldGet
## CHECK-DAG:                       If [<<Value>>]
## CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
## CHECK-DAG:                       Return [<<Phi>>]

## CHECK-START: boolean TestCase.testCase() select_generator (after)
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Value:z\d+>>    StaticFieldGet
## CHECK-DAG:     <<Select:i\d+>>   Select [<<Const1>>,<<Const0>>,<<Value>>]
## CHECK-DAG:                       Return [<<Select>>]

.method public static testCase()Z
    .registers 2
    sget-boolean v0, LTestCase;->value:Z
    const/4 v1, 1
    if-eq v0, v1, :label1
    const/4 v1, 1
    goto :label2
    :label1
    const/4 v1, 0
    :label2
    return v1
.end method
