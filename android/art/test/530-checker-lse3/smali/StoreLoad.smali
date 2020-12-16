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

.class public LStoreLoad;

.super Ljava/lang/Object;

## CHECK-START: int StoreLoad.test(int) load_store_elimination (before)
## CHECK-DAG:     <<Arg:i\d+>>    ParameterValue
## CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<Arg>>] field_name:StoreLoad.byteField
## CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<Arg>>] field_name:StoreLoad.byteField2
## CHECK-DAG:     <<Val:b\d+>>    StaticFieldGet [{{l\d+}}] field_name:StoreLoad.byteField
## CHECK-DAG:     <<Val2:b\d+>>   StaticFieldGet [{{l\d+}}] field_name:StoreLoad.byteField2
## CHECK-DAG:     <<Val3:i\d+>>   Add [<<Val>>,<<Val2>>]
## CHECK-DAG:                     Return [<<Val3>>]

## CHECK-START: int StoreLoad.test(int) load_store_elimination (after)
## CHECK-NOT:                     StaticFieldGet

## CHECK-START: int StoreLoad.test(int) load_store_elimination (after)
## CHECK-DAG:     <<Arg:i\d+>>    ParameterValue
## CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<Arg>>] field_name:StoreLoad.byteField
## CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<Arg>>] field_name:StoreLoad.byteField2
## CHECK-DAG:     <<Conv:b\d+>>   TypeConversion [<<Arg>>]
## CHECK-DAG:     <<Val3:i\d+>>   Add [<<Conv>>,<<Conv>>]
## CHECK-DAG:                     Return [<<Val3>>]
.method public static test(I)I
    .registers 2
    sput-byte v1, LStoreLoad;->byteField:B
    sput-byte v1, LStoreLoad;->byteField2:B
    sget-byte v0, LStoreLoad;->byteField:B
    sget-byte v1, LStoreLoad;->byteField2:B
    add-int/2addr v0, v1
    return v0
.end method

## CHECK-START: int StoreLoad.test2(int) load_store_elimination (before)
## CHECK-DAG:     <<Arg:i\d+>>    ParameterValue
## CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<Arg>>] field_name:StoreLoad.byteField
## CHECK-DAG:                     Return [<<Arg>>]

## CHECK-START: int StoreLoad.test2(int) load_store_elimination (after)
## CHECK-NOT:                     TypeConversion
.method public static test2(I)I
    .registers 1
    sput-byte v0, LStoreLoad;->byteField:B
    return v0
.end method

.field public static byteField:B
.field public static byteField2:B
