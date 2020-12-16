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

# The phi in this method has no actual uses but one environment use. It will
# be eliminated in normal mode but kept live in debuggable mode. Test that
# Checker runs the correct test for each compilation mode.

## CHECK-START: int TestCase.deadPhi(int, int, int) builder (after)
## CHECK-NOT:         Phi

## CHECK-START-DEBUGGABLE: int TestCase.deadPhi(int, int, int) builder (after)
## CHECK:             Phi

.method public static deadPhi(III)I
  .registers 8

  move v0, p1
  if-eqz p0, :after
  move v0, p2
  :after
  # v0 = Phi [p1, p2] with no uses

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use

  :return
  return p2
.end method
