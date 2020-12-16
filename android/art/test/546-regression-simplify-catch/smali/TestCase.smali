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

# Test simplification of an empty, dead catch block. Compiler used to segfault
# because it did expect at least a control-flow instruction (b/25494450).

.method public static testCase_EmptyCatch()I
    .registers 3

    const v0, 0x0
    return v0

    :try_start
    nop
    :try_end
    .catchall {:try_start .. :try_end} :catch

    nop

    :catch
    nop

.end method

# Test simplification of a dead catch block with some code but no control-flow
# instruction.

.method public static testCase_NoConrolFlowCatch()I
    .registers 3

    const v0, 0x0
    return v0

    :try_start
    nop
    :try_end
    .catchall {:try_start .. :try_end} :catch

    nop

    :catch
    const v1, 0x3
    add-int v0, v0, v1

.end method

# Test simplification of a dead catch block with normal-predecessors but
# starting with a move-exception. Verifier does not check trivially dead code
# and this used to trip a DCHECK (b/25492628).

.method public static testCase_InvalidLoadException()I
    .registers 3

    const v0, 0x0
    return v0

    :try_start
    nop
    :try_end
    .catchall {:try_start .. :try_end} :catch

    :catch
    move-exception v0

.end method

# Test simplification of a live catch block with dead normal-predecessors and
# starting with a move-exception. Verifier does not check trivially dead code
# and this used to trip a DCHECK (b/25492628).

.method public static testCase_TriviallyDeadPredecessor(II)I
    .registers 3

    :try_start
    div-int v0, p0, p1
    return v0
    :try_end
    .catchall {:try_start .. :try_end} :catch

    # Trivially dead predecessor block.
    add-int p0, p0, p1

    :catch
    # This verifies because only exceptional predecessors are live.
    move-exception v0
    const v0, 0x0
    return v0

.end method

