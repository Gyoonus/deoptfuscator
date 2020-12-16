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

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) intrinsics_recognition (after)
##  CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<PhiX:i\d+>>   Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<PhiY:i\d+>>   Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect [<<PhiX>>,<<PhiY>>,<<Method>>] intrinsic:IntegerCompare
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) instruction_simplifier (after)
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<PhiX:i\d+>>   Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<PhiY:i\d+>>   Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<PhiX>>,<<PhiY>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) instruction_simplifier (after)
##  CHECK-NOT:                     InvokeStaticOrDirect

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) select_generator (after)
##  CHECK:         <<ArgX:z\d+>>   ParameterValue
##  CHECK:         <<ArgY:z\d+>>   ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<SelX:i\d+>>   Select [<<Zero>>,<<One>>,<<ArgX>>]
##  CHECK-DAG:     <<SelY:i\d+>>   Select [<<Zero>>,<<One>>,<<ArgY>>]
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<SelX>>,<<SelY>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) select_generator (after)
##  CHECK-NOT:                     Phi

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) instruction_simplifier$after_bce (after)
##  CHECK:         <<ArgX:z\d+>>   ParameterValue
##  CHECK:         <<ArgY:z\d+>>   ParameterValue
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<ArgX>>,<<ArgY>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.compareBooleans(boolean, boolean) instruction_simplifier$after_bce (after)
##  CHECK-NOT:                     Select

#   Note: This test has been written in smali (in addition to the source version) because Dexers
#         such as D8 can perform the same type of intrinsic replacements.
.method public static compareBooleans(ZZ)I
    # return Integer.compare((x ? 1 : 0), (y ? 1 : 0));
    .registers 5
    const/4 v0, 0x1

    const/4 v1, 0x0

    if-eqz p0, :cond_c

    move v2, v0

    :goto_5
    if-eqz p1, :cond_e

    :goto_7
    invoke-static {v2, v0}, Ljava/lang/Integer;->compare(II)I

    move-result v0

    return v0

    :cond_c
    move v2, v1

    goto :goto_5

    :cond_e
    move v0, v1

    goto :goto_7
.end method
