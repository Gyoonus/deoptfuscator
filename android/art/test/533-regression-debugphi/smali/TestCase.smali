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

# This is a reduced test case that used to trigger an infinite loop
# in the DeadPhiHandling phase of the optimizing compiler (only used
# with debuggable flag).
.method public static testCase(IILjava/lang/Object;)V
  .registers 5
  const/4 v0, 0x0

  :B4
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  goto :B7

  :B7
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  if-nez p2, :Btmp
  goto :B111

  :Btmp
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  if-nez p2, :B9
  goto :B110

  :B13
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  add-int v0, p0, p1
  goto :B7

  :B110
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  add-int v0, p0, p1
  goto :B111

  :B111
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  goto :B4

  :B9
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  if-nez p2, :B10

  :B11
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  move v1, v0
  goto :B12

  :B10
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  move-object v1, p2
  goto :B12

  :B12
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  goto :B13

  return-void
.end method
