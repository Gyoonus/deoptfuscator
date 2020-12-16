#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTest;

.super Ljava/lang/Object;

## CHECK-START: int Test.testCase(int, int, int) builder (after)
## CHECK:         TryBoundary kind:entry
## CHECK:         TryBoundary kind:entry
## CHECK-NOT:     TryBoundary kind:entry

## CHECK-START: int Test.testCase(int, int, int) builder (after)
## CHECK:         TryBoundary kind:exit
## CHECK:         TryBoundary kind:exit
## CHECK-NOT:     TryBoundary kind:exit

.method public static testCase(III)I
  .registers 4

  :try_start_1
  div-int/2addr p0, p1
  return p0
  :try_end_1
  .catchall {:try_start_1 .. :try_end_1} :catchall

  :catchall
  :try_start_2
  move-exception v0
  # Block would be split here but second part not marked as throwing.
  div-int/2addr p0, p1
  if-eqz p2, :else

  div-int/2addr p0, p1
  :else
  div-int/2addr p0, p2
  :try_end_2
  .catchall {:try_start_2 .. :try_end_2} :catchall2

  :catchall2
  return p0

.end method
