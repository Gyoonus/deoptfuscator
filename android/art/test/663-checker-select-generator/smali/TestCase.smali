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

.class public LTestCase;

.super Ljava/lang/Object;

## CHECK-START: boolean TestCase.testCase(boolean) select_generator (before)
## CHECK-DAG:     <<Param:z\d+>>           ParameterValue
## CHECK-DAG:     <<Int0:i\d+>>            IntConstant 0
## CHECK-DAG:     <<Int1:i\d+>>            IntConstant 1
## CHECK-DAG:                              If [<<Param>>]
## CHECK-DAG:                              Return [<<Int0>>]
## CHECK-DAG:                              Return [<<Int1>>]

## CHECK-START: boolean TestCase.testCase(boolean) select_generator (after)
## CHECK-DAG:     <<Param:z\d+>>           ParameterValue
## CHECK-DAG:     <<Int0:i\d+>>            IntConstant 0
## CHECK-DAG:     <<Int1:i\d+>>            IntConstant 1
## CHECK-DAG:     <<Select:i\d+>>          Select [<<Int0>>,<<Int1>>,<<Param>>]
## CHECK-DAG:                              Return [<<Select>>]

.method public static testCase(Z)Z
    .registers 1

    # The select generation will replace this with a select
    # instruction and a return.
    if-eqz v0, :else
    const v0, 0x1
    return v0

    :else
    const v0, 0x0
    return v0
.end method


## CHECK-START: java.lang.Object TestCase.referenceTypeTestCase(Main$Sub1, Main$Sub2, boolean) select_generator (before)
## CHECK-DAG:     <<Param0:l\d+>>          ParameterValue
## CHECK-DAG:     <<Param1:l\d+>>          ParameterValue
## CHECK-DAG:     <<Param2:z\d+>>          ParameterValue
## CHECK-DAG:                              If [<<Param2>>]
## CHECK-DAG:                              Return [<<Param1>>]
## CHECK-DAG:                              Return [<<Param0>>]

## CHECK-START: java.lang.Object TestCase.referenceTypeTestCase(Main$Sub1, Main$Sub2, boolean) select_generator (after)
## CHECK-DAG:     <<Param0:l\d+>>          ParameterValue
## CHECK-DAG:     <<Param1:l\d+>>          ParameterValue
## CHECK-DAG:     <<Param2:z\d+>>          ParameterValue
## CHECK-DAG:     <<Select:l\d+>>          Select [<<Param1>>,<<Param0>>,<<Param2>>]
## CHECK-DAG:                              Return [<<Select>>]

.method public static referenceTypeTestCase(LMain$Sub1;LMain$Sub2;Z)Ljava/lang/Object;
    .registers 3

    if-eqz v2, :else
    return-object v0

    :else
    return-object v1
.end method
