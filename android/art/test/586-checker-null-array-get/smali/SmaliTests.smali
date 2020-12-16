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

.class public LSmaliTests;
.super Ljava/lang/Object;

## CHECK-START: void SmaliTests.bar() load_store_elimination (after)
## CHECK-DAG: <<Null:l\d+>>       NullConstant
## CHECK-DAG: <<BoundType:l\d+>>  BoundType [<<Null>>]
## CHECK-DAG: <<CheckL:l\d+>>     NullCheck [<<BoundType>>]
## CHECK-DAG: <<GetL0:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
## CHECK-DAG: <<GetL1:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
## CHECK-DAG: <<GetL2:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
## CHECK-DAG: <<GetL3:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
## CHECK-DAG: <<CheckJ:l\d+>>     NullCheck [<<Null>>]
## CHECK-DAG: <<GetJ0:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
## CHECK-DAG: <<GetJ1:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
## CHECK-DAG: <<GetJ2:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
## CHECK-DAG: <<GetJ3:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
.method public static bar()V
    .registers 7

    .prologue
    const/4 v6, 0x3
    const/4 v5, 0x2
    const/4 v4, 0x1
    const/4 v3, 0x0

    # We create multiple accesses that will lead the bounds check
    # elimination pass to add a HDeoptimize. Not having the bounds check helped
    # the load store elimination think it could merge two ArrayGet with different
    # types.

    # String[] array = (String[])getNull();
    invoke-static {}, LMain;->getNull()Ljava/lang/Object;
    move-result-object v0
    check-cast v0, [Ljava/lang/String;

    # objectField = array[0];
    aget-object v2, v0, v3
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[1];
    aget-object v2, v0, v4
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[2];
    aget-object v2, v0, v5
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[3];
    aget-object v2, v0, v6
    sput-object v2, LMain;->objectField:Ljava/lang/Object;

    # long[] longArray = getLongArray();
    invoke-static {}, LMain;->getLongArray()[J
    move-result-object v1

    # longField = longArray[0];
    aget-wide v2, v1, v3
    sput-wide v2, LMain;->longField:J
    # longField = longArray[1];
    aget-wide v2, v1, v4
    sput-wide v2, LMain;->longField:J
    # longField = longArray[2];
    aget-wide v2, v1, v5
    sput-wide v2, LMain;->longField:J
    # longField = longArray[3];
    aget-wide v2, v1, v6
    sput-wide v2, LMain;->longField:J

    return-void
.end method

#   This is indentical to bar() except that it has two check-casts
#   that DX tends to generate.

##  CHECK-START: void SmaliTests.bar2() load_store_elimination (after)
##  CHECK-DAG: <<Null:l\d+>>       NullConstant
##  CHECK-DAG: <<BoundFirst:l\d+>> BoundType [<<Null>>]
##  CHECK-DAG: <<BoundType:l\d+>>  BoundType [<<BoundFirst>>]
##  CHECK-DAG: <<CheckL:l\d+>>     NullCheck [<<BoundType>>]
##  CHECK-DAG: <<GetL0:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
##  CHECK-DAG: <<GetL1:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
##  CHECK-DAG: <<GetL2:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
##  CHECK-DAG: <<GetL3:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
##  CHECK-DAG: <<CheckJ:l\d+>>     NullCheck [<<Null>>]
##  CHECK-DAG: <<GetJ0:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
##  CHECK-DAG: <<GetJ1:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
##  CHECK-DAG: <<GetJ2:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
##  CHECK-DAG: <<GetJ3:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
.method public static bar2()V
    .registers 7

    .prologue
    const/4 v6, 0x3
    const/4 v5, 0x2
    const/4 v4, 0x1
    const/4 v3, 0x0

    # We create multiple accesses that will lead the bounds check
    # elimination pass to add a HDeoptimize. Not having the bounds check helped
    # the load store elimination think it could merge two ArrayGet with different
    # types.

    # String[] array = (String[])getNull();
    invoke-static {}, LMain;->getNull()Ljava/lang/Object;
    move-result-object v2
    check-cast v2, [Ljava/lang/String;

    move-object v0, v2
    check-cast v0, [Ljava/lang/String;

    # objectField = array[0];
    aget-object v2, v0, v3
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[1];
    aget-object v2, v0, v4
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[2];
    aget-object v2, v0, v5
    sput-object v2, LMain;->objectField:Ljava/lang/Object;
    # objectField = array[3];
    aget-object v2, v0, v6
    sput-object v2, LMain;->objectField:Ljava/lang/Object;

    # long[] longArray = getLongArray();
    invoke-static {}, LMain;->getLongArray()[J
    move-result-object v1

    # longField = longArray[0];
    aget-wide v2, v1, v3
    sput-wide v2, LMain;->longField:J
    # longField = longArray[1];
    aget-wide v2, v1, v4
    sput-wide v2, LMain;->longField:J
    # longField = longArray[2];
    aget-wide v2, v1, v5
    sput-wide v2, LMain;->longField:J
    # longField = longArray[3];
    aget-wide v2, v1, v6
    sput-wide v2, LMain;->longField:J

    return-void
.end method

# static fields
.field static doThrow:Z # boolean

# direct methods
.method static constructor <clinit>()V
    .registers 1

    .prologue
    # doThrow = false
    const/4 v0, 0x0
    sput-boolean v0, LSmaliTests;->doThrow:Z
    return-void
.end method
