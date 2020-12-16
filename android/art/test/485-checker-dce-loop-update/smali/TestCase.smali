# Copyright (C) 2015 The Android Open Source Project
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

.method public static $inline$True()Z
  .registers 1
  const/4 v0, 1
  return v0
.end method


## CHECK-START: int TestCase.testSingleExit(int, boolean) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst1:i\d+>>  IntConstant 1
## CHECK-DAG:     <<Cst5:i\d+>>  IntConstant 5
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add5:i\d+>>,<<Add7:i\d+>>] loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:                    If [<<Cst1>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add5>>       Add [<<PhiX>>,<<Cst5>>]                    loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

## CHECK-START: int TestCase.testSingleExit(int, boolean) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<AddX:i\d+>>]               loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<AddX>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

.method public static testSingleExit(IZ)I
  .registers 3

  # p0 = int X
  # p1 = boolean Y
  # v0 = true

  invoke-static {}, LTestCase;->$inline$True()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically
  if-nez v0, :loop_end    # will always exit

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method


## CHECK-START: int TestCase.testMultipleExits(int, boolean, boolean) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst1:i\d+>>  IntConstant 1
## CHECK-DAG:     <<Cst5:i\d+>>  IntConstant 5
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add5:i\d+>>,<<Add7:i\d+>>] loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:                    If [<<ArgZ>>]                              loop:<<HeaderY>>
## CHECK-DAG:                    If [<<Cst1>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add5>>       Add [<<PhiX>>,<<Cst5>>]                    loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

## CHECK-START: int TestCase.testMultipleExits(int, boolean, boolean) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add7:i\d+>>]               loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    If [<<ArgZ>>]                              loop:none
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

.method public static testMultipleExits(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->$inline$True()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically
  if-nez p2, :loop_end    # may exit
  if-nez v0, :loop_end    # will always exit

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method


## CHECK-START: int TestCase.testExitPredecessors(int, boolean, boolean) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst1:i\d+>>  IntConstant 1
## CHECK-DAG:     <<Cst5:i\d+>>  IntConstant 5
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<Cst11:i\d+>> IntConstant 11
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add5:i\d+>>,<<Add7:i\d+>>] loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Mul9:i\d+>>  Mul [<<PhiX>>,<<Cst11>>]                   loop:<<HeaderY>>
## CHECK-DAG:     <<SelX:i\d+>>  Select [<<PhiX>>,<<Mul9>>,<<ArgZ>>]        loop:<<HeaderY>>
## CHECK-DAG:                    If [<<Cst1>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add5>>       Add [<<SelX>>,<<Cst5>>]                    loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    Return [<<SelX>>]                          loop:none

## CHECK-START: int TestCase.testExitPredecessors(int, boolean, boolean) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
## CHECK-DAG:     <<Cst11:i\d+>> IntConstant 11
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add7:i\d+>>]               loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:     <<Mul9:i\d+>>  Mul [<<PhiX>>,<<Cst11>>]                   loop:none
## CHECK-DAG:     <<SelX:i\d+>>  Select [<<PhiX>>,<<Mul9>>,<<ArgZ>>]        loop:none
## CHECK-DAG:                    Return [<<SelX>>]                          loop:none

.method public static testExitPredecessors(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->$inline$True()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically

  # Additional logic which will end up outside the loop
  if-eqz p2, :skip_if
  mul-int/lit8 p0, p0, 11
  :skip_if

  if-nez v0, :loop_end    # will always take the branch

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method


## CHECK-START: int TestCase.testInnerLoop(int, boolean, boolean) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst0:i\d+>>  IntConstant 0
## CHECK-DAG:     <<Cst1:i\d+>>  IntConstant 1
## CHECK-DAG:     <<Cst5:i\d+>>  IntConstant 5
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
#
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add5:i\d+>>,<<Add7:i\d+>>] loop:<<HeaderY:B\d+>>
## CHECK-DAG:     <<PhiZ1:i\d+>> Phi [<<ArgZ>>,<<XorZ:i\d+>>,<<PhiZ1>>]     loop:<<HeaderY>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
#
#                                ### Inner loop ###
## CHECK-DAG:     <<PhiZ2:i\d+>> Phi [<<PhiZ1>>,<<XorZ>>]                   loop:<<HeaderZ:B\d+>>
## CHECK-DAG:     <<XorZ>>       Xor [<<PhiZ2>>,<<Cst1>>]                   loop:<<HeaderZ>>
## CHECK-DAG:     <<CondZ:z\d+>> Equal [<<XorZ>>,<<Cst0>>]                  loop:<<HeaderZ>>
## CHECK-DAG:                    If [<<CondZ>>]                             loop:<<HeaderZ>>
#
## CHECK-DAG:     <<Add5>>       Add [<<PhiX>>,<<Cst5>>]                    loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

## CHECK-START: int TestCase.testInnerLoop(int, boolean, boolean) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<ArgX:i\d+>>  ParameterValue
## CHECK-DAG:     <<ArgY:z\d+>>  ParameterValue
## CHECK-DAG:     <<ArgZ:z\d+>>  ParameterValue
## CHECK-DAG:     <<Cst0:i\d+>>  IntConstant 0
## CHECK-DAG:     <<Cst1:i\d+>>  IntConstant 1
## CHECK-DAG:     <<Cst7:i\d+>>  IntConstant 7
#
## CHECK-DAG:     <<PhiX:i\d+>>  Phi [<<ArgX>>,<<Add7:i\d+>>]               loop:<<HeaderY:B\d+>>
## CHECK-DAG:                    If [<<ArgY>>]                              loop:<<HeaderY>>
## CHECK-DAG:     <<Add7>>       Add [<<PhiX>>,<<Cst7>>]                    loop:<<HeaderY>>
#
#                                ### Inner loop ###
## CHECK-DAG:     <<PhiZ:i\d+>>  Phi [<<ArgZ>>,<<XorZ:i\d+>>]               loop:<<HeaderZ:B\d+>>
## CHECK-DAG:     <<XorZ>>       Xor [<<PhiZ>>,<<Cst1>>]                    loop:<<HeaderZ>>
## CHECK-DAG:     <<CondZ:z\d+>> Equal [<<XorZ>>,<<Cst0>>]                  loop:<<HeaderZ>>
## CHECK-DAG:                    If [<<CondZ>>]                             loop:<<HeaderZ>>
#
## CHECK-DAG:                    Return [<<PhiX>>]                          loop:none

.method public static testInnerLoop(IZZ)I
  .registers 4

  # p0 = int X
  # p1 = boolean Y
  # p2 = boolean Z
  # v0 = true

  invoke-static {}, LTestCase;->$inline$True()Z
  move-result v0

  :loop_start
  if-eqz p1, :loop_body   # cannot be determined statically

  # Inner loop which will end up outside its parent
  :inner_loop_start
  xor-int/lit8 p2, p2, 1
  if-eqz p2, :inner_loop_start

  if-nez v0, :loop_end    # will always take the branch

  # Dead block
  add-int/lit8 p0, p0, 5
  goto :loop_start

  # Live block
  :loop_body
  add-int/lit8 p0, p0, 7
  goto :loop_start

  :loop_end
  return p0
.end method
