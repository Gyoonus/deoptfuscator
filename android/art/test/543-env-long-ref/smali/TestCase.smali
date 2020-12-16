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

.method public static testCase()I
  .registers 5
  const-wide/16 v0, 0x1
  invoke-static {v0, v1}, LMain;->$noinline$allocate(J)LMain;
  move-result-object v1
  invoke-static {v1}, LMain;->lookForMyRegisters(LMain;)V
  iget v2, v1, LMain;->field:I
  return v2
.end method
