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

.class public LSsaBuilder;

.super Ljava/lang/Object;

# Tests that catch blocks with both normal and exceptional predecessors are
# split in two.

## CHECK-START: int SsaBuilder.testSimplifyCatchBlock(int, int, int) builder (after)

## CHECK:      name             "B1"
## CHECK-NEXT: from_bci
## CHECK-NEXT: to_bci
## CHECK-NEXT: predecessors
## CHECK-NEXT: successors       "<<BAdd:B\d+>>"

## CHECK:      name             "<<BAdd>>"
## CHECK-NEXT: from_bci
## CHECK-NEXT: to_bci
## CHECK-NEXT: predecessors     "B1" "<<BCatch:B\d+>>"
## CHECK-NEXT: successors
## CHECK-NEXT: xhandlers
## CHECK-NOT:  end_block
## CHECK:      Add

## CHECK:      name             "<<BCatch>>"
## CHECK-NEXT: from_bci
## CHECK-NEXT: to_bci
## CHECK-NEXT: predecessors
## CHECK-NEXT: successors       "<<BAdd>>"
## CHECK-NEXT: xhandlers
## CHECK-NEXT: flags            "catch_block"

.method public static testSimplifyCatchBlock(III)I
    .registers 4
    # Avoid entry block be a pre header, which leads to
    # the cfg simplifier to add a synthesized block.
    goto :catch_all

    :catch_all
    add-int/2addr p0, p1

    :try_start
    div-int/2addr p0, p2
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    return p0
.end method

# Should be rejected because :catch_all is a loop header.

## CHECK-START: int SsaBuilder.testCatchLoopHeader(int, int, int) builder (after, bad_state)

.method public static testCatchLoopHeader(III)I
    .registers 4

    :try_start_1
    div-int/2addr p0, p1
    return p0
    :try_end_1
    .catchall {:try_start_1 .. :try_end_1} :catch_all

    :catch_all
    :try_start_2
    div-int/2addr p0, p2
    return p0
    :try_end_2
    .catchall {:try_start_2 .. :try_end_2} :catch_all

.end method

# Tests creation of catch Phis.

## CHECK-START: int SsaBuilder.testPhiCreation(int, int, int) builder (after)
## CHECK-DAG:     <<P0:i\d+>>   ParameterValue
## CHECK-DAG:     <<P1:i\d+>>   ParameterValue
## CHECK-DAG:     <<P2:i\d+>>   ParameterValue

## CHECK-DAG:     <<DZC1:i\d+>> DivZeroCheck [<<P1>>]
## CHECK-DAG:     <<Div1:i\d+>> Div [<<P0>>,<<DZC1>>]
## CHECK-DAG:     <<DZC2:i\d+>> DivZeroCheck [<<P1>>]
## CHECK-DAG:     <<Div2:i\d+>> Div [<<Div1>>,<<DZC2>>]
## CHECK-DAG:     <<DZC3:i\d+>> DivZeroCheck [<<P1>>]
## CHECK-DAG:     <<Div3:i\d+>> Div [<<Div2>>,<<DZC3>>]

## CHECK-DAG:     <<Phi1:i\d+>> Phi [<<P0>>,<<P1>>,<<P2>>] reg:0 is_catch_phi:true
## CHECK-DAG:     <<Phi2:i\d+>> Phi [<<Div3>>,<<Phi1>>]    reg:0 is_catch_phi:false
## CHECK-DAG:                   Return [<<Phi2>>]

.method public static testPhiCreation(III)I
    .registers 4

    :try_start
    move v0, p0
    div-int/2addr p0, p1

    move v0, p1
    div-int/2addr p0, p1

    move v0, p2
    div-int/2addr p0, p1

    move v0, p0
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return v0

    :catch_all
    goto :return
.end method

# Tests that phi elimination does not remove catch phis where the value does
# not dominate the phi.

## CHECK-START: int SsaBuilder.testPhiElimination_Domination(int, int) builder (after)
## CHECK-DAG:     <<P0:i\d+>>   ParameterValue
## CHECK-DAG:     <<P1:i\d+>>   ParameterValue
## CHECK-DAG:     <<Cst5:i\d+>> IntConstant 5
## CHECK-DAG:     <<Cst7:i\d+>> IntConstant 7

## CHECK-DAG:     <<Add1:i\d+>> Add [<<Cst7>>,<<Cst7>>]
## CHECK-DAG:     <<DZC:i\d+>>  DivZeroCheck [<<P1>>]
## CHECK-DAG:     <<Div:i\d+>>  Div [<<P0>>,<<DZC>>]

## CHECK-DAG:     <<Phi1:i\d+>> Phi [<<Add1>>] reg:1 is_catch_phi:true
## CHECK-DAG:     <<Add2:i\d+>> Add [<<Cst5>>,<<Phi1>>]

## CHECK-DAG:     <<Phi2:i\d+>> Phi [<<Cst5>>,<<Add2>>] reg:0 is_catch_phi:false
## CHECK-DAG:                   Return [<<Phi2>>]

.method public static testPhiElimination_Domination(II)I
    .registers 4

    :try_start
    # The constant in entry block will dominate the vreg 0 catch phi.
    const v0, 5

    # Insert addition so that the value of vreg 1 does not dominate the phi.
    const v1, 7
    add-int/2addr v1, v1

    div-int/2addr p0, p1
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return v0

    :catch_all
    add-int/2addr v0, v1
    goto :return
.end method

# Tests that phi elimination loops until no more phis can be removed.

## CHECK-START: int SsaBuilder.testPhiElimination_Dependencies(int, int, int) builder (after)
## CHECK-NOT:     Phi

.method public static testPhiElimination_Dependencies(III)I
    .registers 4

    # This constant reaches Return via the normal control-flow path and both
    # exceptional paths. Since v0 is never changed, there should be no phis.
    const v0, 5

    :try_start
    div-int/2addr p0, p1
    div-int/2addr p0, p2
    :try_end
    .catch Ljava/lang/ArithmeticException; {:try_start .. :try_end} :catch_arith
    .catchall {:try_start .. :try_end} :catch_all

    :return
    # Phi [v0, CatchPhi1, CatchPhi2]
    return v0

    :catch_arith
    # CatchPhi1 [v0, v0]
    goto :return

    :catch_all
    # CatchPhi2 [v0, v0]
    goto :return
.end method

# Tests that dead catch blocks are removed.

## CHECK-START: int SsaBuilder.testDeadCatchBlock(int, int, int) builder (after)
## CHECK-DAG:     <<P0:i\d+>>   ParameterValue
## CHECK-DAG:     <<P1:i\d+>>   ParameterValue
## CHECK-DAG:     <<P2:i\d+>>   ParameterValue
## CHECK-DAG:     <<Add1:i\d+>> Add [<<P0>>,<<P1>>]
## CHECK-DAG:     <<Add2:i\d+>> Add [<<Add1>>,<<P2>>]
## CHECK-DAG:                   Return [<<Add2>>]

## CHECK-START: int SsaBuilder.testDeadCatchBlock(int, int, int) builder (after)
## CHECK-NOT:                   flags "catch_block"
## CHECK-NOT:                   Mul

.method public static testDeadCatchBlock(III)I
    .registers 4

    :try_start
    add-int/2addr p0, p1
    add-int/2addr p0, p2
    move v0, p0
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return v0

    :catch_all
    mul-int/2addr v1, v1
    goto :return
.end method
