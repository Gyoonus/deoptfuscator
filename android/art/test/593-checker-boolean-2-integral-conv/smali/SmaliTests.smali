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

# static fields
.field public static booleanField:Z

.method static constructor <clinit>()V
    .registers 1

    .prologue
    const/4 v0, 0x1

    # booleanField = true
    sput-boolean v0, LSmaliTests;->booleanField:Z

    return-void
.end method

##  CHECK-START: byte SmaliTests.booleanToByte(boolean) builder (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<Zero>>]
##  CHECK-DAG:                            If [<<Cond>>]
##  CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<IToS:b\d+>>          TypeConversion [<<Phi>>]
##  CHECK-DAG:                            Return [<<IToS>>]

##  CHECK-START: byte SmaliTests.booleanToByte(boolean) select_generator (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
##  CHECK-DAG:     <<IToS:b\d+>>          TypeConversion [<<Sel>>]
##  CHECK-DAG:                            Return [<<IToS>>]

##  CHECK-START: byte SmaliTests.booleanToByte(boolean) instruction_simplifier$after_bce (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:                            Return [<<Arg>>]
.method static booleanToByte(Z)B
    .registers 2
    if-eqz p0, :cond_5
    const/4 v0, 0x1

    :goto_3
    int-to-byte v0, v0
    return v0

    :cond_5
    const/4 v0, 0x0
    goto :goto_3
.end method

##  CHECK-START: short SmaliTests.booleanToShort(boolean) builder (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<Zero>>]
##  CHECK-DAG:                            If [<<Cond>>]
##  CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<IToS:s\d+>>          TypeConversion [<<Phi>>]
##  CHECK-DAG:                            Return [<<IToS>>]

##  CHECK-START: short SmaliTests.booleanToShort(boolean) select_generator (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
##  CHECK-DAG:     <<IToS:s\d+>>          TypeConversion [<<Sel>>]
##  CHECK-DAG:                            Return [<<IToS>>]

##  CHECK-START: short SmaliTests.booleanToShort(boolean) instruction_simplifier$after_bce (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:                            Return [<<Arg>>]
.method static booleanToShort(Z)S
    .registers 2
    if-eqz p0, :cond_5
    const/4 v0, 0x1

    :goto_3
    int-to-short v0, v0
    return v0

    :cond_5
    const/4 v0, 0x0
    goto :goto_3
.end method

##  CHECK-START: char SmaliTests.booleanToChar(boolean) builder (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<Zero>>]
##  CHECK-DAG:                            If [<<Cond>>]
##  CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<IToC:c\d+>>          TypeConversion [<<Phi>>]
##  CHECK-DAG:                            Return [<<IToC>>]

##  CHECK-START: char SmaliTests.booleanToChar(boolean) select_generator (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
##  CHECK-DAG:     <<IToC:c\d+>>          TypeConversion [<<Sel>>]
##  CHECK-DAG:                            Return [<<IToC>>]

##  CHECK-START: char SmaliTests.booleanToChar(boolean) instruction_simplifier$after_bce (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:                            Return [<<Arg>>]
.method static booleanToChar(Z)C
    .registers 2
    if-eqz p0, :cond_5
    const/4 v0, 0x1

    :goto_3
    int-to-char v0, v0
    return v0

    :cond_5
    const/4 v0, 0x0
    goto :goto_3
.end method

##  CHECK-START: int SmaliTests.booleanToInt(boolean) builder (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<Zero>>]
##  CHECK-DAG:                            If [<<Cond>>]
##  CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:                            Return [<<Phi>>]

##  CHECK-START: int SmaliTests.booleanToInt(boolean) select_generator (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>           IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
##  CHECK-DAG:                            Return [<<Sel>>]

##  CHECK-START: int SmaliTests.booleanToInt(boolean) instruction_simplifier$after_bce (after)
##  CHECK:         <<Arg:z\d+>>           ParameterValue
##  CHECK-DAG:                            Return [<<Arg>>]
.method static booleanToInt(Z)I
    .registers 2
    if-eqz p0, :cond_4
    const/4 v0, 0x1

    :goto_3
    return v0

    :cond_4
    const/4 v0, 0x0
    goto :goto_3
.end method

## CHECK-START: long SmaliTests.booleanToLong(boolean) builder (after)
## CHECK-DAG:     <<Arg:z\d+>>           ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
## CHECK-DAG:     <<One:i\d+>>           IntConstant 1
## CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<Zero>>]
## CHECK-DAG:                            If [<<Cond>>]
## CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<IToJ:j\d+>>          TypeConversion [<<Phi>>]
## CHECK-DAG:                            Return [<<IToJ>>]

## CHECK-START: long SmaliTests.booleanToLong(boolean) select_generator (after)
## CHECK-DAG:     <<Arg:z\d+>>           ParameterValue
## CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
## CHECK-DAG:     <<One:i\d+>>           IntConstant 1
## CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
## CHECK-DAG:     <<IToJ:j\d+>>          TypeConversion [<<Sel>>]
## CHECK-DAG:                            Return [<<IToJ>>]

## CHECK-START: long SmaliTests.booleanToLong(boolean) instruction_simplifier$after_bce (after)
## CHECK-DAG:     <<Arg:z\d+>>           ParameterValue
## CHECK-DAG:     <<ZToJ:j\d+>>          TypeConversion [<<Arg>>]
## CHECK-DAG:                            Return [<<ZToJ>>]
.method public static booleanToLong(Z)J
    .registers 3
    .param p0, "b"    # Z
    .prologue

    # return b ? 1 : 0;
    if-eqz p0, :b_is_zero

# :b_is_one
    const/4 v0, 0x1

  :l_return
    int-to-long v0, v0
    return-wide v0

  :b_is_zero
    const/4 v0, 0x0
    goto :l_return
.end method

## CHECK-START: int SmaliTests.longToIntOfBoolean() builder (after)
## CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
## CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
## CHECK-DAG:     <<ZToJ:j\d+>>          InvokeStaticOrDirect [<<Sget>>,<<Method>>]
## CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<ZToJ>>]
## CHECK-DAG:                            Return [<<JToI>>]

## CHECK-START: int SmaliTests.longToIntOfBoolean() inliner (after)
## CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
## CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
## CHECK-DAG:     <<One:i\d+>>           IntConstant 1
## CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
## CHECK-DAG:                            If [<<Sget>>]
## CHECK-DAG:     <<Phi:i\d+>>           Phi [<<One>>,<<Zero>>]
## CHECK-DAG:     <<IToJ:j\d+>>          TypeConversion [<<Phi>>]
## CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<IToJ>>]
## CHECK-DAG:                            Return [<<JToI>>]

## CHECK-START: int SmaliTests.longToIntOfBoolean() select_generator (after)
## CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
## CHECK-DAG:     <<Zero:i\d+>>          IntConstant 0
## CHECK-DAG:     <<One:i\d+>>           IntConstant 1
## CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
## CHECK-DAG:     <<Sel:i\d+>>           Select [<<Zero>>,<<One>>,<<Sget>>]
## CHECK-DAG:     <<IToJ:j\d+>>          TypeConversion [<<Sel>>]
## CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<IToJ>>]
## CHECK-DAG:                            Return [<<JToI>>]

## CHECK-START: int SmaliTests.longToIntOfBoolean() instruction_simplifier$after_bce (after)
## CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
## CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
## CHECK-DAG:                            Return [<<Sget>>]
.method public static longToIntOfBoolean()I
    .registers 3
    .prologue

    # long l = booleanToLong(booleanField);
    sget-boolean v2, LSmaliTests;->booleanField:Z
    invoke-static {v2}, LSmaliTests;->booleanToLong(Z)J
    move-result-wide v0

    # return (int) l;
    long-to-int v2, v0
    return v2
.end method
