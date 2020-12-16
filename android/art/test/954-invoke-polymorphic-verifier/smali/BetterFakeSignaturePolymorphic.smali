#
# Copyright (C) 2016 The Android Open Source Project
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

.source "BetterFakeSignaturePolymorphic.smali"

.class public LBetterFakeSignaturePolymorphic;
.super Ljava/lang/Object;

.method public constructor <init>()V
.registers 4
  invoke-direct {p0}, Ljava/lang/Object;-><init>()V
  invoke-static {}, LBetterFakeSignaturePolymorphic;->getMain()LMain;
  move-result-object v0
  const/4 v1, 0
  move-object v1, v1
  # Fail here because Main;->invokeExact is on wrong class.
  invoke-polymorphic {v0, v1}, LMain;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, ([Ljava/lang/Object;)Ljava/lang/Object;
  return-void
.end method

.method public static getMethodHandle()Ljava/lang/invoke/MethodHandle;
.registers 1
  const/4 v0, 0
  return-object v0
.end method

.method public static getMain()LMain;
.registers 1
  const/4 v0, 0
  return-object v0
.end method