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

.field public static sField:I

.method private static $inline$False()Z
    .registers 1
    const/4 v0, 0x0
    return v0
.end method

# Test a case when one entering TryBoundary is dead but the rest of the try
# block remains live.

## CHECK-START: int TestCase.testDeadEntry(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK: Add

## CHECK-START: int TestCase.testDeadEntry(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK:     TryBoundary kind:entry
## CHECK:     TryBoundary kind:entry
## CHECK-NOT: TryBoundary kind:entry

## CHECK-START: int TestCase.testDeadEntry(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK-NOT: Add

## CHECK-START: int TestCase.testDeadEntry(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK:     TryBoundary kind:entry
## CHECK-NOT: TryBoundary kind:entry

.method public static testDeadEntry(IIII)I
    .registers 5

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    if-eqz v0, :else

    add-int/2addr p0, p1

    :try_start
    div-int/2addr p0, p2

    :else
    div-int/2addr p0, p3
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return p0

    :catch_all
    const/4 p0, -0x1
    goto :return

.end method

# Test a case when one exiting TryBoundary is dead but the rest of the try
# block remains live.

## CHECK-START: int TestCase.testDeadExit(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK: Add

## CHECK-START: int TestCase.testDeadExit(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK:     TryBoundary kind:exit
## CHECK:     TryBoundary kind:exit
## CHECK-NOT: TryBoundary kind:exit

## CHECK-START: int TestCase.testDeadExit(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK-NOT: Add

## CHECK-START: int TestCase.testDeadExit(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK:     TryBoundary kind:exit
## CHECK-NOT: TryBoundary kind:exit

.method public static testDeadExit(IIII)I
    .registers 5

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    :try_start
    div-int/2addr p0, p2

    if-nez v0, :else

    div-int/2addr p0, p3
    goto :return
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :else
    add-int/2addr p0, p1

    :return
    return p0

    :catch_all
    const/4 p0, -0x1
    goto :return

.end method

# Test that a catch block remains live and consistent if some of try blocks
# throwing into it are removed.

## CHECK-START: int TestCase.testOneTryBlockDead(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK:     TryBoundary kind:entry
## CHECK:     TryBoundary kind:entry
## CHECK-NOT: TryBoundary kind:entry

## CHECK-START: int TestCase.testOneTryBlockDead(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK:     TryBoundary kind:exit
## CHECK:     TryBoundary kind:exit
## CHECK-NOT: TryBoundary kind:exit

## CHECK-START: int TestCase.testOneTryBlockDead(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK:     TryBoundary kind:entry
## CHECK-NOT: TryBoundary kind:entry

## CHECK-START: int TestCase.testOneTryBlockDead(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK:     TryBoundary kind:exit
## CHECK-NOT: TryBoundary kind:exit

.method public static testOneTryBlockDead(IIII)I
    .registers 5

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    :try_start_1
    div-int/2addr p0, p2
    :try_end_1
    .catchall {:try_start_1 .. :try_end_1} :catch_all

    if-eqz v0, :return

    :try_start_2
    div-int/2addr p0, p3
    :try_end_2
    .catchall {:try_start_2 .. :try_end_2} :catch_all

    :return
    return p0

    :catch_all
    const/4 p0, -0x1
    goto :return

.end method

# Test that try block membership is recomputed. In this test case, the try entry
# stored with the merge block gets deleted and SSAChecker would fail if it was
# not replaced with the try entry from the live branch.

.method public static testRecomputeTryMembership(IIII)I
    .registers 5

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    if-eqz v0, :else

    # Dead branch
    :try_start
    div-int/2addr p0, p1
    goto :merge

    # Live branch
    :else
    div-int/2addr p0, p2

    # Merge block. Make complex so it does not get merged with the live branch.
    :merge
    div-int/2addr p0, p3
    if-eqz p0, :else2
    div-int/2addr p0, p3
    :else2
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return p0

    :catch_all
    const/4 p0, -0x1
    goto :return

.end method

# Test that DCE removes catch phi uses of instructions defined in dead try blocks.

## CHECK-START: int TestCase.testCatchPhiInputs_DefinedInTryBlock(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<Arg0:i\d+>>      ParameterValue
## CHECK-DAG:     <<Arg1:i\d+>>      ParameterValue
## CHECK-DAG:     <<Const0xa:i\d+>>  IntConstant 10
## CHECK-DAG:     <<Const0xb:i\d+>>  IntConstant 11
## CHECK-DAG:     <<Const0xc:i\d+>>  IntConstant 12
## CHECK-DAG:     <<Const0xd:i\d+>>  IntConstant 13
## CHECK-DAG:     <<Const0xe:i\d+>>  IntConstant 14
## CHECK-DAG:     <<Const0xf:i\d+>>  IntConstant 15
## CHECK-DAG:     <<Const0x10:i\d+>> IntConstant 16
## CHECK-DAG:     <<Const0x11:i\d+>> IntConstant 17
## CHECK-DAG:     <<Add:i\d+>>       Add [<<Arg0>>,<<Arg1>>]
## CHECK-DAG:     <<Select:i\d+>>    Select [<<Const0xf>>,<<Add>>,{{z\d+}}]
## CHECK-DAG:                        Phi [<<Const0xa>>,<<Const0xb>>,<<Const0xd>>] reg:1 is_catch_phi:true
## CHECK-DAG:                        Phi [<<Add>>,<<Const0xc>>,<<Const0xe>>] reg:2 is_catch_phi:true
## CHECK-DAG:                        Phi [<<Select>>,<<Const0x10>>,<<Const0x11>>] reg:3 is_catch_phi:true

## CHECK-START: int TestCase.testCatchPhiInputs_DefinedInTryBlock(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<Const0xb:i\d+>>  IntConstant 11
## CHECK-DAG:     <<Const0xc:i\d+>>  IntConstant 12
## CHECK-DAG:     <<Const0xd:i\d+>>  IntConstant 13
## CHECK-DAG:     <<Const0xe:i\d+>>  IntConstant 14
## CHECK-DAG:     <<Const0x10:i\d+>> IntConstant 16
## CHECK-DAG:     <<Const0x11:i\d+>> IntConstant 17
## CHECK-DAG:                        Phi [<<Const0xb>>,<<Const0xd>>] reg:1 is_catch_phi:true
## CHECK-DAG:                        Phi [<<Const0xc>>,<<Const0xe>>] reg:2 is_catch_phi:true
## CHECK-DAG:                        Phi [<<Const0x10>>,<<Const0x11>>] reg:3 is_catch_phi:true

.method public static testCatchPhiInputs_DefinedInTryBlock(IIII)I
    .registers 8

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    if-eqz v0, :else

    shr-int/2addr p2, p3

    :try_start
    const v1, 0xa                  # dead catch phi input, defined in entry block (HInstruction)
    add-int v2, p0, p1             # dead catch phi input, defined in the dead block (HInstruction)
    move v3, v2
    if-eqz v3, :define_phi
    const v3, 0xf
    :define_phi
    # v3 = Phi [Add, 0xf]          # dead catch phi input, defined in the dead block (HPhi)
    div-int/2addr p0, v2

    :else
    const v1, 0xb                  # live catch phi input
    const v2, 0xc                  # live catch phi input
    const v3, 0x10                 # live catch phi input
    div-int/2addr p0, p3

    const v1, 0xd                  # live catch phi input
    const v2, 0xe                  # live catch phi input
    const v3, 0x11                 # live catch phi input
    div-int/2addr p0, p1
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return p0

    :catch_all
    sub-int p0, v1, v2      # use catch phi values
    sub-int p0, p0, v3      # use catch phi values
    goto :return

.end method

# Test that DCE does not remove catch phi uses of instructions defined outside
# dead try blocks.

## CHECK-START: int TestCase.testCatchPhiInputs_DefinedOutsideTryBlock(int, int, int, int) dead_code_elimination$after_inlining (before)
## CHECK-DAG:     <<Const0xa:i\d+>> IntConstant 10
## CHECK-DAG:     <<Const0xb:i\d+>> IntConstant 11
## CHECK-DAG:     <<Const0xc:i\d+>> IntConstant 12
## CHECK-DAG:     <<Const0xd:i\d+>> IntConstant 13
## CHECK-DAG:     <<Const0xe:i\d+>> IntConstant 14
## CHECK-DAG:     <<Const0xf:i\d+>> IntConstant 15
## CHECK-DAG:                       Phi [<<Const0xa>>,<<Const0xb>>,<<Const0xd>>] reg:1 is_catch_phi:true
## CHECK-DAG:                       Phi [<<Const0xf>>,<<Const0xc>>,<<Const0xe>>] reg:2 is_catch_phi:true

## CHECK-START: int TestCase.testCatchPhiInputs_DefinedOutsideTryBlock(int, int, int, int) dead_code_elimination$after_inlining (after)
## CHECK-DAG:     <<Const0xa:i\d+>> IntConstant 10
## CHECK-DAG:     <<Const0xb:i\d+>> IntConstant 11
## CHECK-DAG:     <<Const0xc:i\d+>> IntConstant 12
## CHECK-DAG:     <<Const0xd:i\d+>> IntConstant 13
## CHECK-DAG:     <<Const0xe:i\d+>> IntConstant 14
## CHECK-DAG:     <<Const0xf:i\d+>> IntConstant 15
## CHECK-DAG:                       Phi [<<Const0xa>>,<<Const0xb>>,<<Const0xd>>] reg:1 is_catch_phi:true
## CHECK-DAG:                       Phi [<<Const0xf>>,<<Const0xc>>,<<Const0xe>>] reg:2 is_catch_phi:true

.method public static testCatchPhiInputs_DefinedOutsideTryBlock(IIII)I
    .registers 7

    invoke-static {}, LTestCase;->$inline$False()Z
    move-result v0

    if-eqz v0, :else

    shr-int/2addr p2, p3

    :try_start
    const v1, 0xa           # dead catch phi input, defined in entry block
    const v2, 0xf           # dead catch phi input, defined in entry block
    div-int/2addr p0, v2

    :else
    const v1, 0xb           # live catch phi input
    const v2, 0xc           # live catch phi input
    div-int/2addr p0, p3

    const v1, 0xd           # live catch phi input
    const v2, 0xe           # live catch phi input
    div-int/2addr p0, p1
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :return
    return p0

    :catch_all
    sub-int p0, v1, v2      # use catch phi values
    goto :return

.end method
