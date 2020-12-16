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

.class public LTypePropagation;
.super Ljava/lang/Object;

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeDeadPhi(boolean, boolean, int, float, float) builder (after)
## CHECK-NOT: Phi
.method public static mergeDeadPhi(ZZIFF)V
  .registers 8

  if-eqz p0, :after1
  move p2, p3
  :after1
  # p2 = merge(int,float) = conflict

  if-eqz p1, :after2
  move p2, p4
  :after2
  # p2 = merge(conflict,float) = conflict

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeSameType(boolean, int, int) builder (after)
## CHECK:     {{i\d+}} Phi
## CHECK-NOT:          Phi
.method public static mergeSameType(ZII)V
  .registers 8
  if-eqz p0, :after
  move p1, p2
  :after
  # p1 = merge(int,int) = int
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeVoidInput(boolean, boolean, int, int) builder (after)
## CHECK:     {{i\d+}} Phi
## CHECK:     {{i\d+}} Phi
## CHECK-NOT:          Phi
.method public static mergeVoidInput(ZZII)V
  .registers 8
  :loop
  # p2 = void (loop phi) => p2 = merge(int,int) = int
  if-eqz p0, :after
  move p2, p3
  :after
  # p2 = merge(void,int) = int
  if-eqz p1, :loop
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeDifferentSize(boolean, int, long) builder (after)
## CHECK-NOT: Phi
.method public static mergeDifferentSize(ZIJ)V
  .registers 8
  if-eqz p0, :after
  move-wide p1, p2
  :after
  # p1 = merge(int,long) = conflict
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeRefFloat(boolean, float, java.lang.Object) builder (after)
## CHECK-NOT: Phi
.method public static mergeRefFloat(ZFLjava/lang/Object;)V
  .registers 8
  if-eqz p0, :after
  move-object p1, p2
  :after
  # p1 = merge(float,reference) = conflict
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeIntFloat_Success(boolean, float) builder (after)
## CHECK:     {{f\d+}} Phi
## CHECK-NOT:          Phi
.method public static mergeIntFloat_Success(ZF)V
  .registers 8
  if-eqz p0, :after
  const/4 p1, 0x0
  :after
  # p1 = merge(float,0x0) = float
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.mergeIntFloat_Fail(boolean, int, float) builder (after)
## CHECK-NOT: Phi
.method public static mergeIntFloat_Fail(ZIF)V
  .registers 8
  if-eqz p0, :after
  move p1, p2
  :after
  # p1 = merge(int,float) = conflict
  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method

## CHECK-START-DEBUGGABLE: void TypePropagation.updateAllUsersOnConflict(boolean, boolean, int, float, int) builder (after)
## CHECK-NOT: Phi
.method public static updateAllUsersOnConflict(ZZIFI)V
  .registers 8

  :loop1
  # loop phis for all args
  # p2 = merge(int,float) = float? => conflict
  move p2, p3
  if-eqz p0, :loop1

  :loop2
  # loop phis for all args
  # requests float equivalent of p4 phi in loop1 => conflict
  # propagates conflict to loop2's phis
  move p2, p4
  if-eqz p1, :loop2

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method
