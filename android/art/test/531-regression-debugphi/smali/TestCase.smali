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

# Situation:
#  - PhiA: PrimVoid + PrimNot equivalents
#  - PhiB: PrimVoid (PrimVoid PhiA as input)
# DeadPhiHandling:
#  - iterate over blocks in reverse post order
#    - update PrimVoid PhiA to PrimNot
#    - update inputs of PrimNot PhiA
#    - set type of PhiB
#  - left with two PrimNot equivalents of PhiA

.method public static testCase_ReversePostOrder(IILjava/lang/Object;)V
  .registers 5

  # v0 - Phi A
  # v1 - Phi B
  # p0 - int arg1
  # p1 - int arg2
  # p2 - ref arg3

  if-nez p0, :else1
  :then1
    if-nez p1, :else2
    :then2
      const/4 v1, 0x0
      goto :merge2

    :else2
      move-object v1, p2
      goto :merge2

    :merge2
      # PhiA [null, arg3]
      move-object v0, v1                                 # create PrimNot PhiA equivalent
      invoke-static {}, Ljava/lang/System;->nanoTime()J  # env use of both PhiA equivalents
      goto :merge1

  :else1
    move-object v0, p2
    goto :merge1

  :merge1
    # PhiB [PhiA, arg3]
    invoke-static {}, Ljava/lang/System;->nanoTime()J    # env use of PhiB

  return-void
.end method

# Situation:
#  - PhiA: PrimVoid + PrimNot (PrimInt inputs)
#  - PhiB: PrimVoid + PrimNot (PrimInt inputs)
#  - PhiC: PrimVoid only
# DeadPhiHandling:
#  - iterate over blocks in reverse post order
#    - add both PhiAs to worklist, set PrimVoid PhiA to PrimInt
#    - update inputs of PrimNot PhiB ; add PrimNot PhiA to worklist
#    - update PhiC to PrimNot
#  - start processing worklist
#    - PrimNot PhiA: update inputs, no equivalent created
#    - PrimInt PhiA: update inputs, set to PrimNot, use instead of PrimNot PhiA
#    - add PhiBs to worklist as users of PhiA
#    - PrimInt PhiB: set type to PrimNot, equivalent live and in worklist

.method public static testCase_FixPointIteration(IILjava/lang/Object;Ljava/lang/Object;)V
  .registers 6

  # v0 - Phi A, C
  # v1 - Phi B
  # p0 - int arg1
  # p1 - int arg2
  # p2 - ref arg3
  # p3 - ref arg4

  const/4 v0, 0x0

  :loop_header
  # PhiA [null, PhiC] for v0

  if-eqz p0, :else1
  :then1
    const/4 v1, 0x0
    goto :merge1
  :else1
    move-object v1, v0                                   # create PrimNot equivalent of PhiA
    invoke-static {}, Ljava/lang/System;->nanoTime()J    # env use of both PhiA equivalents
    goto :merge1
  :merge1
  # PhiB [null, PhiA] for v1

  move-object v0, v1                                     # creates PrimNot equivalent of PhiB
  invoke-static {}, Ljava/lang/System;->nanoTime()J      # env use of both PhiB equivalents

  if-eqz p1, :else2
  :then2
    move-object v0, p2
    goto :merge2
  :else2
    move-object v0, p3
    goto :merge2
  :merge2
  # PhiC [arg3, arg4] for v0, second input of PhiA

  if-eqz p1, :loop_header
  return-void
.end method
