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

## CHECK-START: int Test.synchronizedHashCode(java.lang.Object) dead_code_elimination$initial (before)
## CHECK:         MonitorOperation [<<Param:l\d+>>] kind:enter
## CHECK:         MonitorOperation [<<Param>>]      kind:exit

## CHECK-START: int Test.synchronizedHashCode(java.lang.Object) dead_code_elimination$initial (after)
## CHECK:         MonitorOperation [<<Param:l\d+>>] kind:enter
## CHECK:         MonitorOperation [<<Param>>]      kind:exit

.method public static synchronizedHashCode(Ljava/lang/Object;)I
  .registers 2

  monitor-enter p0
  invoke-virtual {p0}, Ljava/lang/Object;->hashCode()I
  move-result v0

  # Must not get removed by DCE.
  monitor-exit p0

  return v0

.end method
