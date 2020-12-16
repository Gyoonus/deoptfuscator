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

## CHECK-START: char SmaliTests.stringCharAtCatch(java.lang.String, int) instruction_simplifier (before)
## CHECK-DAG:  <<Char:c\d+>>     InvokeVirtual intrinsic:StringCharAt
## CHECK-DAG:                    Return [<<Char>>]

## CHECK-START: char SmaliTests.stringCharAtCatch(java.lang.String, int) instruction_simplifier (after)
## CHECK-DAG:  <<String:l\d+>>   ParameterValue
## CHECK-DAG:  <<Pos:i\d+>>      ParameterValue
## CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
## CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
## CHECK-DAG:  <<Bounds:i\d+>>   BoundsCheck [<<Pos>>,<<Length>>] is_string_char_at:true
## CHECK-DAG:  <<Char:c\d+>>     ArrayGet [<<NullCk>>,<<Bounds>>] is_string_char_at:true
## CHECK-DAG:                    Return [<<Char>>]

## CHECK-START: char SmaliTests.stringCharAtCatch(java.lang.String, int) instruction_simplifier (after)
## CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt
.method public static stringCharAtCatch(Ljava/lang/String;I)C
    .registers 4
    .param p0, "s"    # Ljava/lang/String;
    .param p1, "pos"    # I

    .prologue

    # if (doThrow) { throw new Error(); }
    sget-boolean v1, LMain;->doThrow:Z
    if-eqz v1, :doThrow_false
    new-instance v1, Ljava/lang/Error;
    invoke-direct {v1}, Ljava/lang/Error;-><init>()V
    throw v1

  :doThrow_false
  :try_start
    # tmp = s.charAt(pos)
    invoke-virtual {p0, p1}, Ljava/lang/String;->charAt(I)C
  :try_end
    .catch Ljava/lang/StringIndexOutOfBoundsException; {:try_start .. :try_end} :catch

    # return tmp
    move-result v1
    return v1

  :catch
    # return '\0'
    move-exception v0
    const/4 v1, 0x0
    return v1
.end method

##  CHECK-START: char SmaliTests.stringCharAtCatchPhiReturn(java.lang.String, int) instruction_simplifier (before)
##  CHECK-DAG:  <<Int:i\d+>>      IntConstant 0
##  CHECK-DAG:  <<Char:c\d+>>     InvokeVirtual intrinsic:StringCharAt
##  CHECK-DAG:  <<Phi:i\d+>>      Phi [<<Char>>,<<Int>>]
##  CHECK-DAG:                    Return [<<Phi>>]

##  CHECK-START: char SmaliTests.stringCharAtCatchPhiReturn(java.lang.String, int) instruction_simplifier (after)
##  CHECK-DAG:  <<String:l\d+>>   ParameterValue
##  CHECK-DAG:  <<Pos:i\d+>>      ParameterValue
##  CHECK-DAG:  <<Int:i\d+>>      IntConstant 0
##  CHECK-DAG:  <<NullCk:l\d+>>   NullCheck [<<String>>]
##  CHECK-DAG:  <<Length:i\d+>>   ArrayLength [<<NullCk>>] is_string_length:true
##  CHECK-DAG:  <<Bounds:i\d+>>   BoundsCheck [<<Pos>>,<<Length>>] is_string_char_at:true
##  CHECK-DAG:  <<Char:c\d+>>     ArrayGet [<<NullCk>>,<<Bounds>>] is_string_char_at:true
##  CHECK-DAG:  <<Phi:i\d+>>      Phi [<<Char>>,<<Int>>]
##  CHECK-DAG:                    Return [<<Phi>>]

##  CHECK-START: char SmaliTests.stringCharAtCatchPhiReturn(java.lang.String, int) instruction_simplifier (after)
##  CHECK-NOT:                    InvokeVirtual intrinsic:StringCharAt
.method public static stringCharAtCatchPhiReturn(Ljava/lang/String;I)C
    .registers 4

    sget-boolean v1, LMain;->doThrow:Z

    if-eqz v1, :cond_a
    new-instance v1, Ljava/lang/Error;
    invoke-direct {v1}, Ljava/lang/Error;-><init>()V
    throw v1

    :cond_a
    :try_start_a
    invoke-virtual {p0, p1}, Ljava/lang/String;->charAt(I)C
    :try_end_d
    .catch Ljava/lang/StringIndexOutOfBoundsException; {:try_start_a .. :try_end_d} :catch_f

    move-result v1

    :goto_e
    return v1

    :catch_f
    move-exception v0

    const/4 v1, 0x0
    goto :goto_e
.end method
