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

.method public static topLevel()V
  .registers 2
  const v0, 0x1
  new-array v0, v0, [LVerifyError;
  invoke-static {v0}, LTestCase;->test([LVerifyError;)V
  return-void
.end method

.method public static test([LVerifyError;)V
   .registers 2
   const v0, 0x0
   aget-object v1, v1, v0
   invoke-virtual {v1}, LSuperVerifyError;->bar()V
   return-void
.end method
