#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: int Smali.bufferLen2() instruction_simplifier (before)
##  CHECK-DAG: <<New:l\d+>>     NewInstance
##  CHECK-DAG: <<String1:l\d+>> LoadString
##  CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>]   intrinsic:StringBufferAppend
##  CHECK-DAG: <<String2:l\d+>> LoadString
##  CHECK-DAG: <<Null1:l\d+>>   NullCheck     [<<Append1>>]
##  CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<Null1>>,<<String2>>] intrinsic:StringBufferAppend
##  CHECK-DAG: <<Null2:l\d+>>   NullCheck     [<<Append2>>]
##  CHECK-DAG:                  InvokeVirtual [<<Null2>>]             intrinsic:StringBufferLength

##  CHECK-START: int Smali.bufferLen2() instruction_simplifier (after)
##  CHECK-DAG: <<New:l\d+>>     NewInstance
##  CHECK-DAG: <<String1:l\d+>> LoadString
##  CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>] intrinsic:StringBufferAppend
##  CHECK-DAG: <<String2:l\d+>> LoadString
##  CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<New>>,<<String2>>] intrinsic:StringBufferAppend
##  CHECK-DAG:                  InvokeVirtual [<<New>>]             intrinsic:StringBufferLength
.method public static bufferLen2()I
    .registers 3

    new-instance v0, Ljava/lang/StringBuffer;

    invoke-direct {v0}, Ljava/lang/StringBuffer;-><init>()V

    const-string v1, "x"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuffer;->append(Ljava/lang/String;)Ljava/lang/StringBuffer;
    move-result-object v1

    const-string v2, "x"
    invoke-virtual {v1, v2}, Ljava/lang/StringBuffer;->append(Ljava/lang/String;)Ljava/lang/StringBuffer;
    move-result-object v1

    invoke-virtual {v1}, Ljava/lang/StringBuffer;->length()I
    move-result v1

    return v1
.end method

## CHECK-START: int Smali.builderLen2() instruction_simplifier (before)
## CHECK-DAG: <<New:l\d+>>     NewInstance
## CHECK-DAG: <<String1:l\d+>> LoadString
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>]   intrinsic:StringBuilderAppend
## CHECK-DAG: <<String2:l\d+>> LoadString
## CHECK-DAG: <<Null2:l\d+>>   NullCheck     [<<Append1>>]
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<Null2>>,<<String2>>] intrinsic:StringBuilderAppend
## CHECK-DAG: <<Null3:l\d+>>   NullCheck     [<<Append2>>]
## CHECK-DAG:                  InvokeVirtual [<<Null3>>]             intrinsic:StringBuilderLength

## CHECK-START: int Smali.builderLen2() instruction_simplifier (after)
## CHECK-DAG: <<New:l\d+>>     NewInstance
## CHECK-DAG: <<String1:l\d+>> LoadString
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>] intrinsic:StringBuilderAppend
## CHECK-DAG: <<String2:l\d+>> LoadString
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<New>>,<<String2>>] intrinsic:StringBuilderAppend
## CHECK-DAG:                  InvokeVirtual [<<New>>]             intrinsic:StringBuilderLength
.method public static builderLen2()I
    .registers 3

    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    const-string v1, "x"
    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    move-result-object v1

    const-string v2, "x"
    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    move-result-object v1

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->length()I
    move-result v1

    return v1
.end method

## CHECK-START: int Smali.bufferLoopAppender() instruction_simplifier (before)
## CHECK-DAG: <<New:l\d+>>     NewInstance                                                         loop:none
## CHECK-DAG: <<String1:l\d+>> LoadString                                                          loop:<<Loop:B\d+>>
## CHECK-DAG: <<Null1:l\d+>>   NullCheck     [<<New>>]                                             loop:<<Loop>>
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<Null1>>,<<String1>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG: <<String2:l\d+>> LoadString                                                          loop:<<Loop>>
## CHECK-DAG: <<Null2:l\d+>>   NullCheck     [<<Append1>>]                                         loop:<<Loop>>
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<Null2>>,<<String2>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG: <<String3:l\d+>> LoadString                                                          loop:<<Loop>>
## CHECK-DAG: <<Null3:l\d+>>   NullCheck     [<<Append2>>]                                         loop:<<Loop>>
## CHECK-DAG: <<Append3:l\d+>> InvokeVirtual [<<Null3>>,<<String3>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG: <<Null4:l\d+>>   NullCheck     [<<New>>]                                             loop:none
## CHECK-DAG:                  InvokeVirtual [<<Null4>>]             intrinsic:StringBufferLength  loop:none

## CHECK-START: int Smali.bufferLoopAppender() instruction_simplifier (after)
## CHECK-DAG: <<New:l\d+>>     NewInstance                                                       loop:none
## CHECK-DAG: <<String1:l\d+>> LoadString                                                        loop:<<Loop:B\d+>>
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG: <<String2:l\d+>> LoadString                                                        loop:<<Loop>>
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<New>>,<<String2>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG: <<String3:l\d+>> LoadString                                                        loop:<<Loop>>
## CHECK-DAG: <<Append3:l\d+>> InvokeVirtual [<<New>>,<<String3>>] intrinsic:StringBufferAppend  loop:<<Loop>>
## CHECK-DAG:                  InvokeVirtual [<<New>>]             intrinsic:StringBufferLength  loop:none
.method public static bufferLoopAppender()I
    .registers 4

    new-instance v0, Ljava/lang/StringBuffer;

    invoke-direct {v0}, Ljava/lang/StringBuffer;-><init>()V

    const/4 v1, 0x0

    :goto_6
    const/16 v2, 0xa

    if-ge v1, v2, :cond_1e

    const-string v2, "x"
    invoke-virtual {v0, v2}, Ljava/lang/StringBuffer;->append(Ljava/lang/String;)Ljava/lang/StringBuffer;
    move-result-object v2

    const-string v3, "y"
    invoke-virtual {v2, v3}, Ljava/lang/StringBuffer;->append(Ljava/lang/String;)Ljava/lang/StringBuffer;
    move-result-object v2

    const-string v3, "z"
    invoke-virtual {v2, v3}, Ljava/lang/StringBuffer;->append(Ljava/lang/String;)Ljava/lang/StringBuffer;

    add-int/lit8 v1, v1, 0x1
    goto :goto_6

    :cond_1e
    invoke-virtual {v0}, Ljava/lang/StringBuffer;->length()I

    move-result v2

    return v2
.end method

## CHECK-START: int Smali.builderLoopAppender() instruction_simplifier (before)
## CHECK-DAG: <<New:l\d+>>     NewInstance                                                         loop:none
## CHECK-DAG: <<String1:l\d+>> LoadString                                                          loop:<<Loop:B\d+>>
## CHECK-DAG: <<Null1:l\d+>>   NullCheck     [<<New>>]                                             loop:<<Loop>>
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<Null1>>,<<String1>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG: <<String2:l\d+>> LoadString                                                          loop:<<Loop>>
## CHECK-DAG: <<Null2:l\d+>>   NullCheck     [<<Append1>>]                                         loop:<<Loop>>
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<Null2>>,<<String2>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG: <<String3:l\d+>> LoadString                                                          loop:<<Loop>>
## CHECK-DAG: <<Null3:l\d+>>   NullCheck     [<<Append2>>]                                         loop:<<Loop>>
## CHECK-DAG: <<Append3:l\d+>> InvokeVirtual [<<Null3>>,<<String3>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG: <<Null4:l\d+>>   NullCheck     [<<New>>]                                             loop:none
## CHECK-DAG:                  InvokeVirtual [<<Null4>>]             intrinsic:StringBuilderLength loop:none

## CHECK-START: int Smali.builderLoopAppender() instruction_simplifier (after)
## CHECK-DAG: <<New:l\d+>>     NewInstance                                                       loop:none
## CHECK-DAG: <<String1:l\d+>> LoadString                                                        loop:<<Loop:B\d+>>
## CHECK-DAG: <<Append1:l\d+>> InvokeVirtual [<<New>>,<<String1>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG: <<String2:l\d+>> LoadString                                                        loop:<<Loop>>
## CHECK-DAG: <<Append2:l\d+>> InvokeVirtual [<<New>>,<<String2>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG: <<String3:l\d+>> LoadString                                                        loop:<<Loop>>
## CHECK-DAG: <<Append3:l\d+>> InvokeVirtual [<<New>>,<<String3>>] intrinsic:StringBuilderAppend loop:<<Loop>>
## CHECK-DAG:                  InvokeVirtual [<<New>>]             intrinsic:StringBuilderLength loop:none
.method public static builderLoopAppender()I
    .registers 4

    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    const/4 v1, 0x0

    :goto_6
    const/16 v2, 0xa

    if-ge v1, v2, :cond_1e

    const-string v2, "x"

    invoke-virtual {v0, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    move-result-object v2
    const-string v3, "y"

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    move-result-object v2
    const-string v3, "z"

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    add-int/lit8 v1, v1, 0x1

    goto :goto_6

    :cond_1e
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->length()I

    move-result v2

    return v2
.end method
