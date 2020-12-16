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

# Inliner used to assign exact type to the artificial multiple-return phi if the
# class type was final which does not hold for arrays.

# The type information is only used by recursive calls to the inliner and is
# overwritten by the next pass of reference type propagation. Since we do not
# inline any methods from array classes, this bug cannot be triggered and we
# verify it using Checker.

## CHECK-START: void TestCase.testInliner() inliner (after)
## CHECK-DAG:             CheckCast [<<Phi:l\d+>>,{{l\d+}}]
## CHECK-DAG:    <<Phi>>  Phi klass:java.lang.Object[] exact:false

.method public static testInliner()V
  .registers 3

  invoke-static {}, Ljava/lang/System;->nanoTime()J
  move-result-wide v0
  long-to-int v0, v0

  invoke-static {v0}, LTestCase;->$inline$getArray(I)[Ljava/lang/Object;
  move-result-object v0

  check-cast v0, [LMain$MyClassA;
  return-void

.end method

.method public static $inline$getArray(I)[Ljava/lang/Object;
  .registers 2
  if-eqz p0, :else

  :then
  const/4 v0, 2
  new-array v0, v0, [LMain$MyClassA;
  return-object v0

  :else
  const/4 v0, 3
  new-array v0, v0, [LMain$MyClassB;
  return-object v0

.end method
