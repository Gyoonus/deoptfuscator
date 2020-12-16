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

# The following test case would crash liveness analysis because the back edge of
# the outer loop would have a smaller liveness position than the two back edges
# of the inner loop. This was caused by a bug which did not split the critical
# edge between TryBoundary and outer loop header (b/25493695).

.method public static testCase(II)I
  .registers 10

  const v0, 0x0                                       # v0 = result
  const v1, 0x1                                       # v1 = const 1

  move v2, p0                                         # v2 = outer loop counter
  :outer_loop
  if-eqz v2, :return
  sub-int/2addr v2, v1

  :try_start

  move v3, p1                                         # v3 = inner loop counter
  :inner_loop
  invoke-static {}, Ljava/lang/System;->nanoTime()J   # throwing instruction
  if-eqz v3, :outer_loop                              # back edge of outer loop
  sub-int/2addr v3, v1

  invoke-static {}, Ljava/lang/System;->nanoTime()J   # throwing instruction
  add-int/2addr v0, v1
  goto :inner_loop                                    # back edge of inner loop

  :try_end
  .catchall {:try_start .. :try_end} :catch

  :catch
  const v4, 0x2
  add-int/2addr v0, v4
  goto :inner_loop                                    # back edge of inner loop

  :return
  return v0

.end method
