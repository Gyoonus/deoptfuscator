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

.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: int Smali.testTrueBranch(int, int) dead_code_elimination$after_inlining (before)
##  CHECK-DAG:     <<ArgX:i\d+>>    ParameterValue
##  CHECK-DAG:     <<ArgY:i\d+>>    ParameterValue
##  CHECK-DAG:                      If
##  CHECK-DAG:     <<Add:i\d+>>     Add [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:     <<Sub:i\d+>>     Sub [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:     <<Phi:i\d+>>     Phi [<<Add>>,<<Sub>>]
##  CHECK-DAG:                      Return [<<Phi>>]

##  CHECK-START: int Smali.testTrueBranch(int, int) dead_code_elimination$after_inlining (after)
##  CHECK-DAG:     <<ArgX:i\d+>>    ParameterValue
##  CHECK-DAG:     <<ArgY:i\d+>>    ParameterValue
##  CHECK-DAG:     <<Add:i\d+>>     Add [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:                      Return [<<Add>>]

##  CHECK-START: int Smali.testTrueBranch(int, int) dead_code_elimination$after_inlining (after)
##  CHECK-NOT:                      If
##  CHECK-NOT:                      Sub
##  CHECK-NOT:                      Phi
.method public static testTrueBranch(II)I
    # int z;
    # if (inlineTrue()) {
    #   z = x + y;
    # } else {
    #   z = x - y;
    #   // Prevent HSelect simplification by having a branch with multiple instructions.
    #   System.nanoTime();
    # }
    #return z;

    .registers 4
    .param p0, "x"    # I
    .param p1, "y"    # I

    invoke-static {}, LMain;->inlineTrue()Z

    move-result v1

    if-eqz v1, :cond_9

    add-int v0, p0, p1

    :goto_8
    return v0

    :cond_9
    sub-int v0, p0, p1

    invoke-static {}, Ljava/lang/System;->nanoTime()J

    goto :goto_8
.end method

##  CHECK-START: int Smali.testFalseBranch(int, int) dead_code_elimination$after_inlining (before)
##  CHECK-DAG:     <<ArgX:i\d+>>    ParameterValue
##  CHECK-DAG:     <<ArgY:i\d+>>    ParameterValue
##  CHECK-DAG:                      If
##  CHECK-DAG:     <<Add:i\d+>>     Add [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:     <<Sub:i\d+>>     Sub [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:     <<Phi:i\d+>>     Phi [<<Add>>,<<Sub>>]
##  CHECK-DAG:                      Return [<<Phi>>]

##  CHECK-START: int Smali.testFalseBranch(int, int) dead_code_elimination$after_inlining (after)
##  CHECK-DAG:     <<ArgX:i\d+>>    ParameterValue
##  CHECK-DAG:     <<ArgY:i\d+>>    ParameterValue
##  CHECK-DAG:     <<Sub:i\d+>>     Sub [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:                      Return [<<Sub>>]

##  CHECK-START: int Smali.testFalseBranch(int, int) dead_code_elimination$after_inlining (after)
##  CHECK-NOT:                      If
##  CHECK-NOT:                      Add
##  CHECK-NOT:                      Phi
.method public static testFalseBranch(II)I
    # int z;
    # if (inlineFalse()) {
    #   z = x + y;
    # } else {
    #   z = x - y;
    #   // Prevent HSelect simplification by having a branch with multiple instructions.
    #   System.nanoTime();
    # }
    # return z;

    .registers 4
    .param p0, "x"    # I
    .param p1, "y"    # I

    invoke-static {}, LMain;->inlineFalse()Z

    move-result v1

    if-eqz v1, :cond_9

    add-int v0, p0, p1

    :goto_8
    return v0

    :cond_9
    sub-int v0, p0, p1

    invoke-static {}, Ljava/lang/System;->nanoTime()J

    goto :goto_8
.end method
