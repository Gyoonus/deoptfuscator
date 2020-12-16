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

## CHECK-START: boolean TestCase.testCase() select_generator (after)
## CHECK-DAG:     <<Select:i\d+>>          Select
## CHECK-DAG:                              Return [<<Select>>]

## CHECK-START: boolean TestCase.testCase() load_store_elimination (after)
## CHECK-DAG:     <<Or:i\d+>>              Or
## CHECK-DAG:     <<TypeConversion:b\d+>>  TypeConversion
## CHECK-DAG:                              StaticFieldSet
## CHECK-DAG:                              Return [<<TypeConversion>>]

.method public static testCase()Z
    .registers 6

    sget-boolean v0, LMain;->field0:Z
    sget-boolean v1, LMain;->field1:Z
    or-int v2, v0, v1
    int-to-byte v2, v2
    sput-boolean v2, LMain;->field2:Z

    # LSE will replace this sget with the type conversion above...
    sget-boolean v2, LMain;->field2:Z

    # ... and select generation will replace this part with a select
    # that simplifies into simply returning the stored boolean.
    if-eqz v2, :else
    const v0, 0x1
    return v0

    :else
    const v0, 0x0
    return v0
.end method
