# Copyright (C) 2016 The Android Open Source Project
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

.field static private test1:Z

.method public static topLevel()V
   .registers 1
   invoke-static {}, LTestCase;->$inline$foo()LUnresolved;
   return-void
.end method

# We need multiple returns to trigger the crash.
.method public static $inline$foo()LUnresolved;
  .registers 2
  const v1, 0x0
  sget-boolean v0, LTestCase;->test1:Z
  if-eqz v0, :other_return
  return-object v1
  :other_return
  invoke-static {}, LTestCase;->$noinline$bar()LUnresolved;
  move-result-object v0
  return-object v0
.end method

.method public static $noinline$bar()LUnresolved;
  .registers 2
  const v1, 0x0
  sget-boolean v0, LTestCase;->test1:Z
  if-eqz v0, :return
  throw v1
  :return
  return-object v1
.end method
