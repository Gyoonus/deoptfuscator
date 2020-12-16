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

.class public LTestCase;

.super Ljava/lang/Object;

.field public static staticField:Ljava/lang/String;

## CHECK-START: void TestCase.testNoAlias(int[], java.lang.String) register (after)
## CHECK:         <<Null:l\d+>>   NullConstant
## CHECK:                         Deoptimize env:[[<<Null>>,{{.*]]}}
## CHECK:                         InvokeStaticOrDirect method_name:java.lang.String.<init>
.method public static testNoAlias([ILjava/lang/String;)V
    .registers 6
    const v1, 0
    const v2, 1
    new-instance v0, Ljava/lang/String;

    # Will deoptimize.
    aget v3, p0, v1

    # Check that we're being executed by the interpreter.
    invoke-static {}, LMain;->assertIsInterpreted()V

    invoke-direct {v0, p1}, Ljava/lang/String;-><init>(Ljava/lang/String;)V

    sput-object v0, LTestCase;->staticField:Ljava/lang/String;

    # Will throw AIOOBE.
    aget v3, p0, v2

    return-void
.end method

## CHECK-START: void TestCase.testAlias(int[], java.lang.String) register (after)
## CHECK:         <<New:l\d+>>    NewInstance
## CHECK:                         Deoptimize env:[[<<New>>,<<New>>,{{.*]]}}
## CHECK:                         InvokeStaticOrDirect method_name:java.lang.String.<init>
.method public static testAlias([ILjava/lang/String;)V
    .registers 7
    const v2, 0
    const v3, 1
    new-instance v0, Ljava/lang/String;
    move-object v1, v0

    # Will deoptimize.
    aget v4, p0, v2

    # Check that we're being executed by the interpreter.
    invoke-static {}, LMain;->assertIsInterpreted()V

    invoke-direct {v1, p1}, Ljava/lang/String;-><init>(Ljava/lang/String;)V

    sput-object v1, LTestCase;->staticField:Ljava/lang/String;

    # Will throw AIOOBE.
    aget v4, p0, v3

    return-void
.end method
